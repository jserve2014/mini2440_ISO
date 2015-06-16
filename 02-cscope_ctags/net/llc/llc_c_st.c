/*
 * llc_c_st.c - This module contains state transition of connection component.
 *
 * Description of event functions and actions there is in 802.2 LLC standard,
 * or in "llc_c_ac.c" and "llc_c_ev.c" modules.
 *
 * Copyright (c) 1997 by Procom Technology, Inc.
 * 		 2001-2003 by Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *
 * This program can be redistributed or modified under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * This program is distributed without any warranty or implied warranty
 * of merchantability or fitness for a particular purpose.
 *
 * See the GNU General Public License for more details.
 */
#include <linux/types.h>
#include <net/llc_if.h>
#include <net/llc_sap.h>
#include <net/llc_c_ev.h>
#include <net/llc_c_ac.h>
#include <net/llc_c_st.h>

#define NONE NULL

/* COMMON CONNECTION STATE transitions
 * Common transitions for
 * LLC_CONN_STATE_NORMAL,
 * LLC_CONN_STATE_BUSY,
 * LLC_CONN_STATE_REJ,
 * LLC_CONN_STATE_AWAIT,
 * LLC_CONN_STATE_AWAIT_BUSY and
 * LLC_CONN_STATE_AWAIT_REJ states
 */
/* State transitions for LLC_CONN_EV_DISC_REQ event */
static llc_conn_action_t llc_common_actions_1[] = {
	[0] = llc_conn_ac_send_disc_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_1,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_1 = {
	.ev	       = llc_conn_ev_disc_req,
	.next_state    = LLC_CONN_STATE_D_CONN,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_1,
};

/* State transitions for LLC_CONN_EV_RESET_REQ event */
static llc_conn_action_t llc_common_actions_2[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_1,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_2 = {
	.ev	       = llc_conn_ev_rst_req,
	.next_state    = LLC_CONN_STATE_RESET,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_2,
};

/* State transitions for LLC_CONN_EV_RX_SABME_CMD_Pbit_SET_X event */
static llc_conn_action_t llc_common_actions_3[] = {
	[0] = llc_conn_ac_stop_all_timers,
	[1] = llc_conn_ac_set_vs_0,
	[2] = llc_conn_ac_set_vr_0,
	[3] = llc_conn_ac_send_ua_rsp_f_set_p,
	[4] = llc_conn_ac_rst_ind,
	[5] = llc_conn_ac_set_p_flag_0,
	[6] = llc_conn_ac_set_remote_busy_0,
	[7] = llc_conn_reset,
	[8] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_3 = {
	.ev	       = llc_conn_ev_rx_sabme_cmd_pbit_set_x,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_3,
};

/* State transitions for LLC_CONN_EV_RX_DISC_CMD_Pbit_SET_X event */
static llc_conn_action_t llc_common_actions_4[] = {
	[0] = llc_conn_ac_stop_all_timers,
	[1] = llc_conn_ac_send_ua_rsp_f_set_p,
	[2] = llc_conn_ac_disc_ind,
	[3] = llc_conn_disc,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_4 = {
	.ev	       = llc_conn_ev_rx_disc_cmd_pbit_set_x,
	.next_state    = LLC_CONN_STATE_ADM,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_4,
};

/* State transitions for LLC_CONN_EV_RX_FRMR_RSP_Fbit_SET_X event */
static llc_conn_action_t llc_common_actions_5[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_rst_ind,
	[5] = llc_conn_ac_set_cause_flag_0,
	[6] = llc_conn_reset,
	[7] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_5 = {
	.ev	       = llc_conn_ev_rx_frmr_rsp_fbit_set_x,
	.next_state    = LLC_CONN_STATE_RESET,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_5,
};

/* State transitions for LLC_CONN_EV_RX_DM_RSP_Fbit_SET_X event */
static llc_conn_action_t llc_common_actions_6[] = {
	[0] = llc_conn_ac_disc_ind,
	[1] = llc_conn_ac_stop_all_timers,
	[2] = llc_conn_disc,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_6 = {
	.ev	       = llc_conn_ev_rx_dm_rsp_fbit_set_x,
	.next_state    = LLC_CONN_STATE_ADM,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_6,
};

/* State transitions for LLC_CONN_EV_RX_ZZZ_CMD_Pbit_SET_X_INVAL_Nr event */
static llc_conn_action_t llc_common_actions_7a[] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_7a = {
	.ev	       = llc_conn_ev_rx_zzz_cmd_pbit_set_x_inval_nr,
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_7a,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_X_INVAL_Ns event */
static llc_conn_action_t llc_common_actions_7b[] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_7b = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_x_inval_ns,
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_7b,
};

/* State transitions for LLC_CONN_EV_RX_ZZZ_RSP_Fbit_SET_X_INVAL_Nr event */
static llc_conn_action_t llc_common_actions_8a[] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8a = {
	.ev	       = llc_conn_ev_rx_zzz_rsp_fbit_set_x_inval_nr,
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_8a,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_X_INVAL_Ns event */
static llc_conn_action_t llc_common_actions_8b[] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8b = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_x_inval_ns,
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_8b,
};

/* State transitions for LLC_CONN_EV_RX_BAD_PDU event */
static llc_conn_action_t llc_common_actions_8c[] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8c = {
	.ev	       = llc_conn_ev_rx_bad_pdu,
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_8c,
};

/* State transitions for LLC_CONN_EV_RX_UA_RSP_Fbit_SET_X event */
static llc_conn_action_t llc_common_actions_9[] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_9 = {
	.ev	       = llc_conn_ev_rx_ua_rsp_fbit_set_x,
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_9,
};

/* State transitions for LLC_CONN_EV_RX_XXX_RSP_Fbit_SET_1 event */
#if 0
static llc_conn_ev_qfyr_t llc_common_ev_qfyrs_10[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_common_actions_10[] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_10 = {
	.ev	       = llc_conn_ev_rx_xxx_rsp_fbit_set_1,
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers = llc_common_ev_qfyrs_10,
	.ev_actions    = llc_common_actions_10,
};
#endif

/* State transitions for LLC_CONN_EV_P_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_common_ev_qfyrs_11a[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_gte_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_common_actions_11a[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_0,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_11a = {
	.ev	       = llc_conn_ev_p_tmr_exp,
	.next_state    = LLC_CONN_STATE_RESET,
	.ev_qualifiers = llc_common_ev_qfyrs_11a,
	.ev_actions    = llc_common_actions_11a,
};

/* State transitions for LLC_CONN_EV_ACK_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_common_ev_qfyrs_11b[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_gte_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_common_actions_11b[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_0,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_11b = {
	.ev	       = llc_conn_ev_ack_tmr_exp,
	.next_state    = LLC_CONN_STATE_RESET,
	.ev_qualifiers = llc_common_ev_qfyrs_11b,
	.ev_actions    = llc_common_actions_11b,
};

/* State transitions for LLC_CONN_EV_REJ_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_common_ev_qfyrs_11c[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_gte_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_common_actions_11c[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_0,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_11c = {
	.ev	       = llc_conn_ev_rej_tmr_exp,
	.next_state    = LLC_CONN_STATE_RESET,
	.ev_qualifiers = llc_common_ev_qfyrs_11c,
	.ev_actions    = llc_common_actions_11c,
};

/* State transitions for LLC_CONN_EV_BUSY_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_common_ev_qfyrs_11d[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_gte_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_common_actions_11d[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_0,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_11d = {
	.ev	       = llc_conn_ev_busy_tmr_exp,
	.next_state    = LLC_CONN_STATE_RESET,
	.ev_qualifiers = llc_common_ev_qfyrs_11d,
	.ev_actions    = llc_common_actions_11d,
};

/*
 * Common dummy state transition; must be last entry for all state
 * transition groups - it'll be on .bss, so will be zeroed.
 */
static struct llc_conn_state_trans llc_common_state_trans_end;

/* LLC_CONN_STATE_ADM transitions */
/* State transitions for LLC_CONN_EV_CONN_REQ event */
static llc_conn_action_t llc_adm_actions_1[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_set_retry_cnt_0,
	[3] = llc_conn_ac_set_s_flag_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_adm_state_trans_1 = {
	.ev	       = llc_conn_ev_conn_req,
	.next_state    = LLC_CONN_STATE_SETUP,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_adm_actions_1,
};

/* State transitions for LLC_CONN_EV_RX_SABME_CMD_Pbit_SET_X event */
static llc_conn_action_t llc_adm_actions_2[] = {
	[0] = llc_conn_ac_send_ua_rsp_f_set_p,
	[1] = llc_conn_ac_set_vs_0,
	[2] = llc_conn_ac_set_vr_0,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_p_flag_0,
	[5] = llc_conn_ac_set_remote_busy_0,
	[6] = llc_conn_ac_conn_ind,
	[7] = NULL,
};

static struct llc_conn_state_trans llc_adm_state_trans_2 = {
	.ev	       = llc_conn_ev_rx_sabme_cmd_pbit_set_x,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_adm_actions_2,
};

/* State transitions for LLC_CONN_EV_RX_DISC_CMD_Pbit_SET_X event */
static llc_conn_action_t llc_adm_actions_3[] = {
	[0] = llc_conn_ac_send_dm_rsp_f_set_p,
	[1] = llc_conn_disc,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_adm_state_trans_3 = {
	.ev	       = llc_conn_ev_rx_disc_cmd_pbit_set_x,
	.next_state    = LLC_CONN_STATE_ADM,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_adm_actions_3,
};

/* State transitions for LLC_CONN_EV_RX_XXX_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_adm_actions_4[] = {
	[0] = llc_conn_ac_send_dm_rsp_f_set_1,
	[1] = llc_conn_disc,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_adm_state_trans_4 = {
	.ev	       = llc_conn_ev_rx_xxx_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_ADM,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_adm_actions_4,
};

/* State transitions for LLC_CONN_EV_RX_XXX_YYY event */
static llc_conn_action_t llc_adm_actions_5[] = {
	[0] = llc_conn_disc,
	[1] = NULL,
};

static struct llc_conn_state_trans llc_adm_state_trans_5 = {
	.ev	       = llc_conn_ev_rx_any_frame,
	.next_state    = LLC_CONN_OUT_OF_SVC,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_adm_actions_5,
};

/*
 * Array of pointers;
 * one to each transition
 */
static struct llc_conn_state_trans *llc_adm_state_transitions[] = {
	[0] = &llc_adm_state_trans_1,		/* Request */
	[1] = &llc_common_state_trans_end,
	[2] = &llc_common_state_trans_end,	/* local_busy */
	[3] = &llc_common_state_trans_end,	/* init_pf_cycle */
	[4] = &llc_common_state_trans_end,	/* timer */
	[5] = &llc_adm_state_trans_2,		/* Receive frame */
	[6] = &llc_adm_state_trans_3,
	[7] = &llc_adm_state_trans_4,
	[8] = &llc_adm_state_trans_5,
	[9] = &llc_common_state_trans_end,
};

/* LLC_CONN_STATE_SETUP transitions */
/* State transitions for LLC_CONN_EV_RX_SABME_CMD_Pbit_SET_X event */
static llc_conn_action_t llc_setup_actions_1[] = {
	[0] = llc_conn_ac_send_ua_rsp_f_set_p,
	[1] = llc_conn_ac_set_vs_0,
	[2] = llc_conn_ac_set_vr_0,
	[3] = llc_conn_ac_set_s_flag_1,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_setup_state_trans_1 = {
	.ev	       = llc_conn_ev_rx_sabme_cmd_pbit_set_x,
	.next_state    = LLC_CONN_STATE_SETUP,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_setup_actions_1,
};

/* State transitions for LLC_CONN_EV_RX_UA_RSP_Fbit_SET_X event */
static llc_conn_ev_qfyr_t llc_setup_ev_qfyrs_2[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_f,
	[1] = llc_conn_ev_qlfy_set_status_conn,
	[2] = NULL,
};

static llc_conn_action_t llc_setup_actions_2[] = {
	[0] = llc_conn_ac_stop_ack_timer,
	[1] = llc_conn_ac_set_vs_0,
	[2] = llc_conn_ac_set_vr_0,
	[3] = llc_conn_ac_upd_p_flag,
	[4] = llc_conn_ac_set_remote_busy_0,
	[5] = llc_conn_ac_conn_confirm,
	[6] = NULL,
};

static struct llc_conn_state_trans llc_setup_state_trans_2 = {
	.ev	       = llc_conn_ev_rx_ua_rsp_fbit_set_x,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_setup_ev_qfyrs_2,
	.ev_actions    = llc_setup_actions_2,
};

/* State transitions for LLC_CONN_EV_ACK_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_setup_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_s_flag_eq_1,
	[1] = llc_conn_ev_qlfy_set_status_conn,
	[2] = NULL,
};

static llc_conn_action_t llc_setup_actions_3[] = {
	[0] = llc_conn_ac_set_p_flag_0,
	[1] = llc_conn_ac_set_remote_busy_0,
	[2] = llc_conn_ac_conn_confirm,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_setup_state_trans_3 = {
	.ev	       = llc_conn_ev_ack_tmr_exp,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_setup_ev_qfyrs_3,
	.ev_actions    = llc_setup_actions_3,
};

/* State transitions for LLC_CONN_EV_RX_DISC_CMD_Pbit_SET_X event */
static llc_conn_ev_qfyr_t llc_setup_ev_qfyrs_4[] = {
	[0] = llc_conn_ev_qlfy_set_status_disc,
	[1] = NULL,
};

static llc_conn_action_t llc_setup_actions_4[] = {
	[0] = llc_conn_ac_send_dm_rsp_f_set_p,
	[1] = llc_conn_ac_stop_ack_timer,
	[2] = llc_conn_ac_conn_confirm,
	[3] = llc_conn_disc,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_setup_state_trans_4 = {
	.ev	       = llc_conn_ev_rx_disc_cmd_pbit_set_x,
	.next_state    = LLC_CONN_STATE_ADM,
	.ev_qualifiers = llc_setup_ev_qfyrs_4,
	.ev_actions    = llc_setup_actions_4,
};

/* State transitions for LLC_CONN_EV_RX_DM_RSP_Fbit_SET_X event */
static llc_conn_ev_qfyr_t llc_setup_ev_qfyrs_5[] = {
	[0] = llc_conn_ev_qlfy_set_status_disc,
	[1] = NULL,
};

static llc_conn_action_t llc_setup_actions_5[] = {
	[0] = llc_conn_ac_stop_ack_timer,
	[1] = llc_conn_ac_conn_confirm,
	[2] = llc_conn_disc,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_setup_state_trans_5 = {
	.ev	       = llc_conn_ev_rx_dm_rsp_fbit_set_x,
	.next_state    = LLC_CONN_STATE_ADM,
	.ev_qualifiers = llc_setup_ev_qfyrs_5,
	.ev_actions    = llc_setup_actions_5,
};

/* State transitions for LLC_CONN_EV_ACK_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_setup_ev_qfyrs_7[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[1] = llc_conn_ev_qlfy_s_flag_eq_0,
	[2] = NULL,
};

static llc_conn_action_t llc_setup_actions_7[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_inc_retry_cnt_by_1,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_setup_state_trans_7 = {
	.ev	       = llc_conn_ev_ack_tmr_exp,
	.next_state    = LLC_CONN_STATE_SETUP,
	.ev_qualifiers = llc_setup_ev_qfyrs_7,
	.ev_actions    = llc_setup_actions_7,
};

/* State transitions for LLC_CONN_EV_ACK_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_setup_ev_qfyrs_8[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_gte_n2,
	[1] = llc_conn_ev_qlfy_s_flag_eq_0,
	[2] = llc_conn_ev_qlfy_set_status_failed,
	[3] = NULL,
};

static llc_conn_action_t llc_setup_actions_8[] = {
	[0] = llc_conn_ac_conn_confirm,
	[1] = llc_conn_disc,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_setup_state_trans_8 = {
	.ev	       = llc_conn_ev_ack_tmr_exp,
	.next_state    = LLC_CONN_STATE_ADM,
	.ev_qualifiers = llc_setup_ev_qfyrs_8,
	.ev_actions    = llc_setup_actions_8,
};

/*
 * Array of pointers;
 * one to each transition
 */
static struct llc_conn_state_trans *llc_setup_state_transitions[] = {
	 [0] = &llc_common_state_trans_end,	/* Request */
	 [1] = &llc_common_state_trans_end,	/* local busy */
	 [2] = &llc_common_state_trans_end,	/* init_pf_cycle */
	 [3] = &llc_setup_state_trans_3,	/* Timer */
	 [4] = &llc_setup_state_trans_7,
	 [5] = &llc_setup_state_trans_8,
	 [6] = &llc_common_state_trans_end,
	 [7] = &llc_setup_state_trans_1,	/* Receive frame */
	 [8] = &llc_setup_state_trans_2,
	 [9] = &llc_setup_state_trans_4,
	[10] = &llc_setup_state_trans_5,
	[11] = &llc_common_state_trans_end,
};

/* LLC_CONN_STATE_NORMAL transitions */
/* State transitions for LLC_CONN_EV_DATA_REQ event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_1[] = {
	[0] = llc_conn_ev_qlfy_remote_busy_eq_0,
	[1] = llc_conn_ev_qlfy_p_flag_eq_0,
	[2] = llc_conn_ev_qlfy_last_frame_eq_0,
	[3] = NULL,
};

static llc_conn_action_t llc_normal_actions_1[] = {
	[0] = llc_conn_ac_send_i_as_ack,
	[1] = llc_conn_ac_start_ack_tmr_if_not_running,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_1 = {
	.ev	       = llc_conn_ev_data_req,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_1,
	.ev_actions    = llc_normal_actions_1,
};

/* State transitions for LLC_CONN_EV_DATA_REQ event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_2[] = {
	[0] = llc_conn_ev_qlfy_remote_busy_eq_0,
	[1] = llc_conn_ev_qlfy_p_flag_eq_0,
	[2] = llc_conn_ev_qlfy_last_frame_eq_1,
	[3] = NULL,
};

static llc_conn_action_t llc_normal_actions_2[] = {
	[0] = llc_conn_ac_send_i_cmd_p_set_1,
	[1] = llc_conn_ac_start_p_timer,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_2 = {
	.ev	       = llc_conn_ev_data_req,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_2,
	.ev_actions    = llc_normal_actions_2,
};

/* State transitions for LLC_CONN_EV_DATA_REQ event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_2_1[] = {
	[0] = llc_conn_ev_qlfy_remote_busy_eq_1,
	[1] = llc_conn_ev_qlfy_set_status_remote_busy,
	[2] = NULL,
};

/* just one member, NULL, .bss zeroes it */
static llc_conn_action_t llc_normal_actions_2_1[1];

static struct llc_conn_state_trans llc_normal_state_trans_2_1 = {
	.ev	       = llc_conn_ev_data_req,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_2_1,
	.ev_actions    = llc_normal_actions_2_1,
};

/* State transitions for LLC_CONN_EV_LOCAL_BUSY_DETECTED event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_3[] = {
	[0] = llc_conn_ac_rst_sendack_flag,
	[1] = llc_conn_ac_send_rnr_xxx_x_set_0,
	[2] = llc_conn_ac_set_data_flag_0,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_3 = {
	.ev	       = llc_conn_ev_local_busy_detected,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_normal_ev_qfyrs_3,
	.ev_actions    = llc_normal_actions_3,
};

/* State transitions for LLC_CONN_EV_LOCAL_BUSY_DETECTED event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_4[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_4[] = {
	[0] = llc_conn_ac_rst_sendack_flag,
	[1] = llc_conn_ac_send_rnr_xxx_x_set_0,
	[2] = llc_conn_ac_set_data_flag_0,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_4 = {
	.ev	       = llc_conn_ev_local_busy_detected,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_normal_ev_qfyrs_4,
	.ev_actions    = llc_normal_actions_4,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_0_UNEXPD_Ns event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_5a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_5a[] = {
	[0] = llc_conn_ac_rst_sendack_flag,
	[1] = llc_conn_ac_send_rej_xxx_x_set_0,
	[2] = llc_conn_ac_upd_nr_received,
	[3] = llc_conn_ac_upd_p_flag,
	[4] = llc_conn_ac_start_rej_timer,
	[5] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[6] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_5a = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0_unexpd_ns,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_normal_ev_qfyrs_5a,
	.ev_actions    = llc_normal_actions_5a,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_0_UNEXPD_Ns event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_5b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_5b[] = {
	[0] = llc_conn_ac_rst_sendack_flag,
	[1] = llc_conn_ac_send_rej_xxx_x_set_0,
	[2] = llc_conn_ac_upd_nr_received,
	[3] = llc_conn_ac_upd_p_flag,
	[4] = llc_conn_ac_start_rej_timer,
	[5] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[6] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_5b = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_0_unexpd_ns,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_normal_ev_qfyrs_5b,
	.ev_actions    = llc_normal_actions_5b,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_1_UNEXPD_Ns event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_5c[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_5c[] = {
	[0] = llc_conn_ac_rst_sendack_flag,
	[1] = llc_conn_ac_send_rej_xxx_x_set_0,
	[2] = llc_conn_ac_upd_nr_received,
	[3] = llc_conn_ac_upd_p_flag,
	[4] = llc_conn_ac_start_rej_timer,
	[5] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[6] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_5c = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_1_unexpd_ns,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_normal_ev_qfyrs_5c,
	.ev_actions    = llc_normal_actions_5c,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_0_UNEXPD_Ns event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_6a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_6a[] = {
	[0] = llc_conn_ac_rst_sendack_flag,
	[1] = llc_conn_ac_send_rej_xxx_x_set_0,
	[2] = llc_conn_ac_upd_nr_received,
	[3] = llc_conn_ac_start_rej_timer,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_6a = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0_unexpd_ns,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_normal_ev_qfyrs_6a,
	.ev_actions    = llc_normal_actions_6a,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_0_UNEXPD_Ns event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_6b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_6b[] = {
	[0] = llc_conn_ac_rst_sendack_flag,
	[1] = llc_conn_ac_send_rej_xxx_x_set_0,
	[2] = llc_conn_ac_upd_nr_received,
	[3] = llc_conn_ac_start_rej_timer,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_6b = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_0_unexpd_ns,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_normal_ev_qfyrs_6b,
	.ev_actions    = llc_normal_actions_6b,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_1_UNEXPD_Ns event */
static llc_conn_action_t llc_normal_actions_7[] = {
	[0] = llc_conn_ac_rst_sendack_flag,
	[1] = llc_conn_ac_send_rej_rsp_f_set_1,
	[2] = llc_conn_ac_upd_nr_received,
	[3] = llc_conn_ac_start_rej_timer,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_7 = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_1_unexpd_ns,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_normal_actions_7,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_X event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_8a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_f,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_8[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_upd_p_flag,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[5] = llc_conn_ac_send_ack_if_needed,
	[6] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_8a = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_x,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_8a,
	.ev_actions    = llc_normal_actions_8,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_8b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_8b = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_8b,
	.ev_actions    = llc_normal_actions_8,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_9a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_9a[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_data_ind,
	[3] = llc_conn_ac_send_ack_if_needed,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_9a = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_9a,
	.ev_actions    = llc_normal_actions_9a,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_9b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_9b[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_data_ind,
	[3] = llc_conn_ac_send_ack_if_needed,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_9b = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_9b,
	.ev_actions    = llc_normal_actions_9b,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_normal_actions_10[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_send_ack_rsp_f_set_1,
	[2] = llc_conn_ac_rst_sendack_flag,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_ac_data_ind,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_10 = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_normal_actions_10,
};

/* State transitions for * LLC_CONN_EV_RX_RR_CMD_Pbit_SET_0 event */
static llc_conn_action_t llc_normal_actions_11a[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_11a = {
	.ev	       = llc_conn_ev_rx_rr_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_normal_actions_11a,
};

/* State transitions for LLC_CONN_EV_RX_RR_RSP_Fbit_SET_0 event */
static llc_conn_action_t llc_normal_actions_11b[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_11b = {
	.ev	       = llc_conn_ev_rx_rr_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_normal_actions_11b,
};

/* State transitions for LLC_CONN_EV_RX_RR_RSP_Fbit_SET_1 event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_11c[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_11c[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_inc_tx_win_size,
	[3] = llc_conn_ac_clear_remote_busy,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_11c = {
	.ev	       = llc_conn_ev_rx_rr_rsp_fbit_set_1,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_11c,
	.ev_actions    = llc_normal_actions_11c,
};

/* State transitions for LLC_CONN_EV_RX_RR_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_normal_actions_12[] = {
	[0] = llc_conn_ac_send_ack_rsp_f_set_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_adjust_npta_by_rr,
	[3] = llc_conn_ac_rst_sendack_flag,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_12 = {
	.ev	       = llc_conn_ev_rx_rr_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_normal_actions_12,
};

/* State transitions for LLC_CONN_EV_RX_RNR_CMD_Pbit_SET_0 event */
static llc_conn_action_t llc_normal_actions_13a[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_13a = {
	.ev	       = llc_conn_ev_rx_rnr_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_normal_actions_13a,
};

/* State transitions for LLC_CONN_EV_RX_RNR_RSP_Fbit_SET_0 event */
static llc_conn_action_t llc_normal_actions_13b[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_13b = {
	.ev	       = llc_conn_ev_rx_rnr_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_normal_actions_13b,
};

/* State transitions for LLC_CONN_EV_RX_RNR_RSP_Fbit_SET_1 event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_13c[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_13c[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_13c = {
	.ev	       = llc_conn_ev_rx_rnr_rsp_fbit_set_1,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_13c,
	.ev_actions    = llc_normal_actions_13c,
};

/* State transitions for LLC_CONN_EV_RX_RNR_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_normal_actions_14[] = {
	[0] = llc_conn_ac_send_rr_rsp_f_set_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_adjust_npta_by_rnr,
	[3] = llc_conn_ac_rst_sendack_flag,
	[4] = llc_conn_ac_set_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_14 = {
	.ev	       = llc_conn_ev_rx_rnr_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_normal_actions_14,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_15a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_15a[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_upd_p_flag,
	[3] = llc_conn_ac_dec_tx_win_size,
	[4] = llc_conn_ac_resend_i_xxx_x_set_0,
	[5] = llc_conn_ac_clear_remote_busy,
	[6] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_15a = {
	.ev	       = llc_conn_ev_rx_rej_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_15a,
	.ev_actions    = llc_normal_actions_15a,
};

/* State transitions for LLC_CONN_EV_RX_REJ_RSP_Fbit_SET_X event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_15b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_f,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_15b[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_upd_p_flag,
	[3] = llc_conn_ac_dec_tx_win_size,
	[4] = llc_conn_ac_resend_i_xxx_x_set_0,
	[5] = llc_conn_ac_clear_remote_busy,
	[6] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_15b = {
	.ev	       = llc_conn_ev_rx_rej_rsp_fbit_set_x,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_15b,
	.ev_actions    = llc_normal_actions_15b,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_16a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_16a[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_dec_tx_win_size,
	[3] = llc_conn_ac_resend_i_xxx_x_set_0,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_16a = {
	.ev	       = llc_conn_ev_rx_rej_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_16a,
	.ev_actions    = llc_normal_actions_16a,
};

/* State transitions for LLC_CONN_EV_RX_REJ_RSP_Fbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_16b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_16b[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_dec_tx_win_size,
	[3] = llc_conn_ac_resend_i_xxx_x_set_0,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_16b = {
	.ev	       = llc_conn_ev_rx_rej_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_16b,
	.ev_actions    = llc_normal_actions_16b,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_normal_actions_17[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_dec_tx_win_size,
	[3] = llc_conn_ac_resend_i_rsp_f_set_1,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_17 = {
	.ev	       = llc_conn_ev_rx_rej_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_normal_actions_17,
};

/* State transitions for LLC_CONN_EV_INIT_P_F_CYCLE event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_18[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_18[] = {
	[0] = llc_conn_ac_send_rr_cmd_p_set_1,
	[1] = llc_conn_ac_start_p_timer,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_18 = {
	.ev	       = llc_conn_ev_init_p_f_cycle,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_18,
	.ev_actions    = llc_normal_actions_18,
};

/* State transitions for LLC_CONN_EV_P_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_19[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_19[] = {
	[0] = llc_conn_ac_rst_sendack_flag,
	[1] = llc_conn_ac_send_rr_cmd_p_set_1,
	[2] = llc_conn_ac_rst_vs,
	[3] = llc_conn_ac_start_p_timer,
	[4] = llc_conn_ac_inc_retry_cnt_by_1,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_19 = {
	.ev	       = llc_conn_ev_p_tmr_exp,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = llc_normal_ev_qfyrs_19,
	.ev_actions    = llc_normal_actions_19,
};

/* State transitions for LLC_CONN_EV_ACK_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_20a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[2] = NULL,
};

static llc_conn_action_t llc_normal_actions_20a[] = {
	[0] = llc_conn_ac_rst_sendack_flag,
	[1] = llc_conn_ac_send_rr_cmd_p_set_1,
	[2] = llc_conn_ac_rst_vs,
	[3] = llc_conn_ac_start_p_timer,
	[4] = llc_conn_ac_inc_retry_cnt_by_1,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_20a = {
	.ev	       = llc_conn_ev_ack_tmr_exp,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = llc_normal_ev_qfyrs_20a,
	.ev_actions    = llc_normal_actions_20a,
};

/* State transitions for LLC_CONN_EV_BUSY_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_20b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[2] = NULL,
};

static llc_conn_action_t llc_normal_actions_20b[] = {
	[0] = llc_conn_ac_rst_sendack_flag,
	[1] = llc_conn_ac_send_rr_cmd_p_set_1,
	[2] = llc_conn_ac_rst_vs,
	[3] = llc_conn_ac_start_p_timer,
	[4] = llc_conn_ac_inc_retry_cnt_by_1,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_20b = {
	.ev	       = llc_conn_ev_busy_tmr_exp,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = llc_normal_ev_qfyrs_20b,
	.ev_actions    = llc_normal_actions_20b,
};

/* State transitions for LLC_CONN_EV_TX_BUFF_FULL event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_21[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_21[] = {
	[0] = llc_conn_ac_send_rr_cmd_p_set_1,
	[1] = llc_conn_ac_start_p_timer,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_21 = {
	.ev	       = llc_conn_ev_tx_buffer_full,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_21,
	.ev_actions    = llc_normal_actions_21,
};

/*
 * Array of pointers;
 * one to each transition
 */
static struct llc_conn_state_trans *llc_normal_state_transitions[] = {
	 [0] = &llc_normal_state_trans_1,	/* Requests */
	 [1] = &llc_normal_state_trans_2,
	 [2] = &llc_normal_state_trans_2_1,
	 [3] = &llc_common_state_trans_1,
	 [4] = &llc_common_state_trans_2,
	 [5] = &llc_common_state_trans_end,
	 [6] = &llc_normal_state_trans_21,
	 [7] = &llc_normal_state_trans_3,	/* Local busy */
	 [8] = &llc_normal_state_trans_4,
	 [9] = &llc_common_state_trans_end,
	[10] = &llc_normal_state_trans_18,	/* Init pf cycle */
	[11] = &llc_common_state_trans_end,
	[12] = &llc_common_state_trans_11a,	/* Timers */
	[13] = &llc_common_state_trans_11b,
	[14] = &llc_common_state_trans_11c,
	[15] = &llc_common_state_trans_11d,
	[16] = &llc_normal_state_trans_19,
	[17] = &llc_normal_state_trans_20a,
	[18] = &llc_normal_state_trans_20b,
	[19] = &llc_common_state_trans_end,
	[20] = &llc_normal_state_trans_8b,	/* Receive frames */
	[21] = &llc_normal_state_trans_9b,
	[22] = &llc_normal_state_trans_10,
	[23] = &llc_normal_state_trans_11b,
	[24] = &llc_normal_state_trans_11c,
	[25] = &llc_normal_state_trans_5a,
	[26] = &llc_normal_state_trans_5b,
	[27] = &llc_normal_state_trans_5c,
	[28] = &llc_normal_state_trans_6a,
	[29] = &llc_normal_state_trans_6b,
	[30] = &llc_normal_state_trans_7,
	[31] = &llc_normal_state_trans_8a,
	[32] = &llc_normal_state_trans_9a,
	[33] = &llc_normal_state_trans_11a,
	[34] = &llc_normal_state_trans_12,
	[35] = &llc_normal_state_trans_13a,
	[36] = &llc_normal_state_trans_13b,
	[37] = &llc_normal_state_trans_13c,
	[38] = &llc_normal_state_trans_14,
	[39] = &llc_normal_state_trans_15a,
	[40] = &llc_normal_state_trans_15b,
	[41] = &llc_normal_state_trans_16a,
	[42] = &llc_normal_state_trans_16b,
	[43] = &llc_normal_state_trans_17,
	[44] = &llc_common_state_trans_3,
	[45] = &llc_common_state_trans_4,
	[46] = &llc_common_state_trans_5,
	[47] = &llc_common_state_trans_6,
	[48] = &llc_common_state_trans_7a,
	[49] = &llc_common_state_trans_7b,
	[50] = &llc_common_state_trans_8a,
	[51] = &llc_common_state_trans_8b,
	[52] = &llc_common_state_trans_8c,
	[53] = &llc_common_state_trans_9,
	/* [54] = &llc_common_state_trans_10, */
	[54] = &llc_common_state_trans_end,
};

/* LLC_CONN_STATE_BUSY transitions */
/* State transitions for LLC_CONN_EV_DATA_REQ event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_1[] = {
	[0] = llc_conn_ev_qlfy_remote_busy_eq_0,
	[1] = llc_conn_ev_qlfy_p_flag_eq_0,
	[2] = NULL,
};

static llc_conn_action_t llc_busy_actions_1[] = {
	[0] = llc_conn_ac_send_i_xxx_x_set_0,
	[1] = llc_conn_ac_start_ack_tmr_if_not_running,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_1 = {
	.ev	       = llc_conn_ev_data_req,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_1,
	.ev_actions    = llc_busy_actions_1,
};

/* State transitions for LLC_CONN_EV_DATA_REQ event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_2[] = {
	[0] = llc_conn_ev_qlfy_remote_busy_eq_0,
	[1] = llc_conn_ev_qlfy_p_flag_eq_1,
	[2] = NULL,
};

static llc_conn_action_t llc_busy_actions_2[] = {
	[0] = llc_conn_ac_send_i_xxx_x_set_0,
	[1] = llc_conn_ac_start_ack_tmr_if_not_running,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_2 = {
	.ev	       = llc_conn_ev_data_req,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_2,
	.ev_actions    = llc_busy_actions_2,
};

/* State transitions for LLC_CONN_EV_DATA_REQ event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_2_1[] = {
	[0] = llc_conn_ev_qlfy_remote_busy_eq_1,
	[1] = llc_conn_ev_qlfy_set_status_remote_busy,
	[2] = NULL,
};

/* just one member, NULL, .bss zeroes it */
static llc_conn_action_t llc_busy_actions_2_1[1];

static struct llc_conn_state_trans llc_busy_state_trans_2_1 = {
	.ev	       = llc_conn_ev_data_req,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_2_1,
	.ev_actions    = llc_busy_actions_2_1,
};

/* State transitions for LLC_CONN_EV_LOCAL_BUSY_CLEARED event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_data_flag_eq_1,
	[1] = llc_conn_ev_qlfy_p_flag_eq_0,
	[2] = NULL,
};

static llc_conn_action_t llc_busy_actions_3[] = {
	[0] = llc_conn_ac_send_rej_xxx_x_set_0,
	[1] = llc_conn_ac_start_rej_timer,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_3 = {
	.ev	       = llc_conn_ev_local_busy_cleared,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_busy_ev_qfyrs_3,
	.ev_actions    = llc_busy_actions_3,
};

/* State transitions for LLC_CONN_EV_LOCAL_BUSY_CLEARED event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_4[] = {
	[0] = llc_conn_ev_qlfy_data_flag_eq_1,
	[1] = llc_conn_ev_qlfy_p_flag_eq_1,
	[2] = NULL,
};

static llc_conn_action_t llc_busy_actions_4[] = {
	[0] = llc_conn_ac_send_rej_xxx_x_set_0,
	[1] = llc_conn_ac_start_rej_timer,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_4 = {
	.ev	       = llc_conn_ev_local_busy_cleared,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_busy_ev_qfyrs_4,
	.ev_actions    = llc_busy_actions_4,
};

/* State transitions for LLC_CONN_EV_LOCAL_BUSY_CLEARED event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_5[] = {
	[0] = llc_conn_ev_qlfy_data_flag_eq_0,
	[1] = llc_conn_ev_qlfy_p_flag_eq_0,
	[2] = NULL,
};

static llc_conn_action_t llc_busy_actions_5[] = {
	[0] = llc_conn_ac_send_rr_xxx_x_set_0,
	[1] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_5 = {
	.ev	       = llc_conn_ev_local_busy_cleared,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_busy_ev_qfyrs_5,
	.ev_actions    = llc_busy_actions_5,
};

/* State transitions for LLC_CONN_EV_LOCAL_BUSY_CLEARED event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_6[] = {
	[0] = llc_conn_ev_qlfy_data_flag_eq_0,
	[1] = llc_conn_ev_qlfy_p_flag_eq_1,
	[2] = NULL,
};

static llc_conn_action_t llc_busy_actions_6[] = {
	[0] = llc_conn_ac_send_rr_xxx_x_set_0,
	[1] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_6 = {
	.ev	       = llc_conn_ev_local_busy_cleared,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_busy_ev_qfyrs_6,
	.ev_actions    = llc_busy_actions_6,
};

/* State transitions for LLC_CONN_EV_LOCAL_BUSY_CLEARED event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_7[] = {
	[0] = llc_conn_ev_qlfy_data_flag_eq_2,
	[1] = llc_conn_ev_qlfy_p_flag_eq_0,
	[2] = NULL,
};

static llc_conn_action_t llc_busy_actions_7[] = {
	[0] = llc_conn_ac_send_rr_xxx_x_set_0,
	[1] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_7 = {
	.ev	       = llc_conn_ev_local_busy_cleared,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_busy_ev_qfyrs_7,
	.ev_actions    = llc_busy_actions_7,
};

/* State transitions for LLC_CONN_EV_LOCAL_BUSY_CLEARED event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_8[] = {
	[0] = llc_conn_ev_qlfy_data_flag_eq_2,
	[1] = llc_conn_ev_qlfy_p_flag_eq_1,
	[2] = NULL,
};

static llc_conn_action_t llc_busy_actions_8[] = {
	[0] = llc_conn_ac_send_rr_xxx_x_set_0,
	[1] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_8 = {
	.ev	       = llc_conn_ev_local_busy_cleared,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_busy_ev_qfyrs_8,
	.ev_actions    = llc_busy_actions_8,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_X_UNEXPD_Ns event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_9a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_f,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_9a[] = {
	[0] = llc_conn_ac_opt_send_rnr_xxx_x_set_0,
	[1] = llc_conn_ac_upd_p_flag,
	[2] = llc_conn_ac_upd_nr_received,
	[3] = llc_conn_ac_set_data_flag_1_if_data_flag_eq_0,
	[4] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_9a = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_x_unexpd_ns,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_9a,
	.ev_actions    = llc_busy_actions_9a,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_0_UNEXPD_Ns event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_9b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_9b[] = {
	[0] = llc_conn_ac_opt_send_rnr_xxx_x_set_0,
	[1] = llc_conn_ac_upd_p_flag,
	[2] = llc_conn_ac_upd_nr_received,
	[3] = llc_conn_ac_set_data_flag_1_if_data_flag_eq_0,
	[4] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_9b = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0_unexpd_ns,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_9b,
	.ev_actions    = llc_busy_actions_9b,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_0_UNEXPD_Ns event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_10a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_10a[] = {
	[0] = llc_conn_ac_opt_send_rnr_xxx_x_set_0,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_data_flag_1_if_data_flag_eq_0,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_10a = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_0_unexpd_ns,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_10a,
	.ev_actions    = llc_busy_actions_10a,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_0_UNEXPD_Ns event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_10b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_10b[] = {
	[0] = llc_conn_ac_opt_send_rnr_xxx_x_set_0,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_data_flag_1_if_data_flag_eq_0,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_10b = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0_unexpd_ns,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_10b,
	.ev_actions    = llc_busy_actions_10b,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_1_UNEXPD_Ns event */
static llc_conn_action_t llc_busy_actions_11[] = {
	[0] = llc_conn_ac_send_rnr_rsp_f_set_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_data_flag_1_if_data_flag_eq_0,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_11 = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_1_unexpd_ns,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_busy_actions_11,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_busy_actions_12[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_send_rnr_rsp_f_set_1,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_ac_stop_rej_tmr_if_data_flag_eq_2,
	[5] = llc_conn_ac_set_data_flag_0,
	[6] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_12 = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_busy_actions_12,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_X event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_13a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_f,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_13a[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_upd_p_flag,
	[3] = llc_conn_ac_opt_send_rnr_xxx_x_set_0,
	[4] = llc_conn_ac_upd_nr_received,
	[5] = llc_conn_ac_stop_rej_tmr_if_data_flag_eq_2,
	[6] = llc_conn_ac_set_data_flag_0,
	[7] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[8] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_13a = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_x,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_13a,
	.ev_actions    = llc_busy_actions_13a,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_13b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_13b[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_upd_p_flag,
	[3] = llc_conn_ac_opt_send_rnr_xxx_x_set_0,
	[4] = llc_conn_ac_upd_nr_received,
	[5] = llc_conn_ac_stop_rej_tmr_if_data_flag_eq_2,
	[6] = llc_conn_ac_set_data_flag_0,
	[7] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[8] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_13b = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_13b,
	.ev_actions    = llc_busy_actions_13b,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_14a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_14a[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_opt_send_rnr_xxx_x_set_0,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_ac_stop_rej_tmr_if_data_flag_eq_2,
	[5] = llc_conn_ac_set_data_flag_0,
	[6] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_14a = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_14a,
	.ev_actions    = llc_busy_actions_14a,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_14b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_14b[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_opt_send_rnr_xxx_x_set_0,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_ac_stop_rej_tmr_if_data_flag_eq_2,
	[5] = llc_conn_ac_set_data_flag_0,
	[6] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_14b = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_14b,
	.ev_actions    = llc_busy_actions_14b,
};

/* State transitions for LLC_CONN_EV_RX_RR_CMD_Pbit_SET_0 event */
static llc_conn_action_t llc_busy_actions_15a[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_15a = {
	.ev	       = llc_conn_ev_rx_rr_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_busy_actions_15a,
};

/* State transitions for LLC_CONN_EV_RX_RR_RSP_Fbit_SET_0 event */
static llc_conn_action_t llc_busy_actions_15b[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_15b = {
	.ev	       = llc_conn_ev_rx_rr_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_busy_actions_15b,
};

/* State transitions for LLC_CONN_EV_RX_RR_RSP_Fbit_SET_1 event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_15c[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_15c[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_15c = {
	.ev	       = llc_conn_ev_rx_rr_rsp_fbit_set_1,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_15c,
	.ev_actions    = llc_busy_actions_15c,
};

/* State transitions for LLC_CONN_EV_RX_RR_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_busy_actions_16[] = {
	[0] = llc_conn_ac_send_rnr_rsp_f_set_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_16 = {
	.ev	       = llc_conn_ev_rx_rr_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_busy_actions_16,
};

/* State transitions for LLC_CONN_EV_RX_RNR_CMD_Pbit_SET_0 event */
static llc_conn_action_t llc_busy_actions_17a[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_17a = {
	.ev	       = llc_conn_ev_rx_rnr_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_busy_actions_17a,
};

/* State transitions for LLC_CONN_EV_RX_RNR_RSP_Fbit_SET_0 event */
static llc_conn_action_t llc_busy_actions_17b[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_17b = {
	.ev	       = llc_conn_ev_rx_rnr_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_busy_actions_17b,
};

/* State transitions for LLC_CONN_EV_RX_RNR_RSP_Fbit_SET_1 event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_17c[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_17c[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_17c = {
	.ev	       = llc_conn_ev_rx_rnr_rsp_fbit_set_1,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_17c,
	.ev_actions    = llc_busy_actions_17c,
};

/* State transitions for LLC_CONN_EV_RX_RNR_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_busy_actions_18[] = {
	[0] = llc_conn_ac_send_rnr_rsp_f_set_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_18 = {
	.ev	       = llc_conn_ev_rx_rnr_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_busy_actions_18,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_19a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_19a[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_upd_p_flag,
	[3] = llc_conn_ac_resend_i_xxx_x_set_0,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_19a = {
	.ev	       = llc_conn_ev_rx_rej_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_19a,
	.ev_actions    = llc_busy_actions_19a,
};

/* State transitions for LLC_CONN_EV_RX_REJ_RSP_Fbit_SET_X event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_19b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_f,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_19b[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_upd_p_flag,
	[3] = llc_conn_ac_resend_i_xxx_x_set_0,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_19b = {
	.ev	       = llc_conn_ev_rx_rej_rsp_fbit_set_x,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_19b,
	.ev_actions    = llc_busy_actions_19b,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_20a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_20a[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_resend_i_xxx_x_set_0,
	[3] = llc_conn_ac_clear_remote_busy,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_20a = {
	.ev	       = llc_conn_ev_rx_rej_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_20a,
	.ev_actions    = llc_busy_actions_20a,
};

/* State transitions for LLC_CONN_EV_RX_REJ_RSP_Fbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_20b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_20b[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_resend_i_xxx_x_set_0,
	[3] = llc_conn_ac_clear_remote_busy,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_20b = {
	.ev	       = llc_conn_ev_rx_rej_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_20b,
	.ev_actions    = llc_busy_actions_20b,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_busy_actions_21[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_send_rnr_rsp_f_set_1,
	[3] = llc_conn_ac_resend_i_xxx_x_set_0,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_21 = {
	.ev	       = llc_conn_ev_rx_rej_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_busy_actions_21,
};

/* State transitions for LLC_CONN_EV_INIT_P_F_CYCLE event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_22[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_22[] = {
	[0] = llc_conn_ac_send_rnr_cmd_p_set_1,
	[1] = llc_conn_ac_start_p_timer,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_22 = {
	.ev	       = llc_conn_ev_init_p_f_cycle,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_22,
	.ev_actions    = llc_busy_actions_22,
};

/* State transitions for LLC_CONN_EV_P_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_23[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_busy_actions_23[] = {
	[0] = llc_conn_ac_send_rnr_cmd_p_set_1,
	[1] = llc_conn_ac_rst_vs,
	[2] = llc_conn_ac_start_p_timer,
	[3] = llc_conn_ac_inc_retry_cnt_by_1,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_23 = {
	.ev	       = llc_conn_ev_p_tmr_exp,
	.next_state    = LLC_CONN_STATE_AWAIT_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_23,
	.ev_actions    = llc_busy_actions_23,
};

/* State transitions for LLC_CONN_EV_ACK_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_24a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[2] = NULL,
};

static llc_conn_action_t llc_busy_actions_24a[] = {
	[0] = llc_conn_ac_send_rnr_cmd_p_set_1,
	[1] = llc_conn_ac_start_p_timer,
	[2] = llc_conn_ac_inc_retry_cnt_by_1,
	[3] = llc_conn_ac_rst_vs,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_24a = {
	.ev	       = llc_conn_ev_ack_tmr_exp,
	.next_state    = LLC_CONN_STATE_AWAIT_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_24a,
	.ev_actions    = llc_busy_actions_24a,
};

/* State transitions for LLC_CONN_EV_BUSY_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_24b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[2] = NULL,
};

static llc_conn_action_t llc_busy_actions_24b[] = {
	[0] = llc_conn_ac_send_rnr_cmd_p_set_1,
	[1] = llc_conn_ac_start_p_timer,
	[2] = llc_conn_ac_inc_retry_cnt_by_1,
	[3] = llc_conn_ac_rst_vs,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_24b = {
	.ev	       = llc_conn_ev_busy_tmr_exp,
	.next_state    = LLC_CONN_STATE_AWAIT_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_24b,
	.ev_actions    = llc_busy_actions_24b,
};

/* State transitions for LLC_CONN_EV_REJ_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_25[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[2] = NULL,
};

static llc_conn_action_t llc_busy_actions_25[] = {
	[0] = llc_conn_ac_send_rnr_cmd_p_set_1,
	[1] = llc_conn_ac_start_p_timer,
	[2] = llc_conn_ac_inc_retry_cnt_by_1,
	[3] = llc_conn_ac_rst_vs,
	[4] = llc_conn_ac_set_data_flag_1,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_25 = {
	.ev	       = llc_conn_ev_rej_tmr_exp,
	.next_state    = LLC_CONN_STATE_AWAIT_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_25,
	.ev_actions    = llc_busy_actions_25,
};

/* State transitions for LLC_CONN_EV_REJ_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_26[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[2] = NULL,
};

static llc_conn_action_t llc_busy_actions_26[] = {
	[0] = llc_conn_ac_set_data_flag_1,
	[1] = NULL,
};

static struct llc_conn_state_trans llc_busy_state_trans_26 = {
	.ev	       = llc_conn_ev_rej_tmr_exp,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_busy_ev_qfyrs_26,
	.ev_actions    = llc_busy_actions_26,
};

/*
 * Array of pointers;
 * one to each transition
 */
static struct llc_conn_state_trans *llc_busy_state_transitions[] = {
	 [0] = &llc_common_state_trans_1,	/* Request */
	 [1] = &llc_common_state_trans_2,
	 [2] = &llc_busy_state_trans_1,
	 [3] = &llc_busy_state_trans_2,
	 [4] = &llc_busy_state_trans_2_1,
	 [5] = &llc_common_state_trans_end,
	 [6] = &llc_busy_state_trans_3,		/* Local busy */
	 [7] = &llc_busy_state_trans_4,
	 [8] = &llc_busy_state_trans_5,
	 [9] = &llc_busy_state_trans_6,
	[10] = &llc_busy_state_trans_7,
	[11] = &llc_busy_state_trans_8,
	[12] = &llc_common_state_trans_end,
	[13] = &llc_busy_state_trans_22,	/* Initiate PF cycle */
	[14] = &llc_common_state_trans_end,
	[15] = &llc_common_state_trans_11a,	/* Timer */
	[16] = &llc_common_state_trans_11b,
	[17] = &llc_common_state_trans_11c,
	[18] = &llc_common_state_trans_11d,
	[19] = &llc_busy_state_trans_23,
	[20] = &llc_busy_state_trans_24a,
	[21] = &llc_busy_state_trans_24b,
	[22] = &llc_busy_state_trans_25,
	[23] = &llc_busy_state_trans_26,
	[24] = &llc_common_state_trans_end,
	[25] = &llc_busy_state_trans_9a,	/* Receive frame */
	[26] = &llc_busy_state_trans_9b,
	[27] = &llc_busy_state_trans_10a,
	[28] = &llc_busy_state_trans_10b,
	[29] = &llc_busy_state_trans_11,
	[30] = &llc_busy_state_trans_12,
	[31] = &llc_busy_state_trans_13a,
	[32] = &llc_busy_state_trans_13b,
	[33] = &llc_busy_state_trans_14a,
	[34] = &llc_busy_state_trans_14b,
	[35] = &llc_busy_state_trans_15a,
	[36] = &llc_busy_state_trans_15b,
	[37] = &llc_busy_state_trans_15c,
	[38] = &llc_busy_state_trans_16,
	[39] = &llc_busy_state_trans_17a,
	[40] = &llc_busy_state_trans_17b,
	[41] = &llc_busy_state_trans_17c,
	[42] = &llc_busy_state_trans_18,
	[43] = &llc_busy_state_trans_19a,
	[44] = &llc_busy_state_trans_19b,
	[45] = &llc_busy_state_trans_20a,
	[46] = &llc_busy_state_trans_20b,
	[47] = &llc_busy_state_trans_21,
	[48] = &llc_common_state_trans_3,
	[49] = &llc_common_state_trans_4,
	[50] = &llc_common_state_trans_5,
	[51] = &llc_common_state_trans_6,
	[52] = &llc_common_state_trans_7a,
	[53] = &llc_common_state_trans_7b,
	[54] = &llc_common_state_trans_8a,
	[55] = &llc_common_state_trans_8b,
	[56] = &llc_common_state_trans_8c,
	[57] = &llc_common_state_trans_9,
	/* [58] = &llc_common_state_trans_10, */
	[58] = &llc_common_state_trans_end,
};

/* LLC_CONN_STATE_REJ transitions */
/* State transitions for LLC_CONN_EV_DATA_REQ event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_1[] = {
	[0] = llc_conn_ev_qlfy_remote_busy_eq_0,
	[1] = llc_conn_ev_qlfy_p_flag_eq_0,
	[2] = NULL,
};

static llc_conn_action_t llc_reject_actions_1[] = {
	[0] = llc_conn_ac_send_i_xxx_x_set_0,
	[1] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_1 = {
	.ev	       = llc_conn_ev_data_req,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_1,
	.ev_actions    = llc_reject_actions_1,
};

/* State transitions for LLC_CONN_EV_DATA_REQ event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_2[] = {
	[0] = llc_conn_ev_qlfy_remote_busy_eq_0,
	[1] = llc_conn_ev_qlfy_p_flag_eq_1,
	[2] = NULL,
};

static llc_conn_action_t llc_reject_actions_2[] = {
	[0] = llc_conn_ac_send_i_xxx_x_set_0,
	[1] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_2 = {
	.ev	       = llc_conn_ev_data_req,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_2,
	.ev_actions    = llc_reject_actions_2,
};

/* State transitions for LLC_CONN_EV_DATA_REQ event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_2_1[] = {
	[0] = llc_conn_ev_qlfy_remote_busy_eq_1,
	[1] = llc_conn_ev_qlfy_set_status_remote_busy,
	[2] = NULL,
};

/* just one member, NULL, .bss zeroes it */
static llc_conn_action_t llc_reject_actions_2_1[1];

static struct llc_conn_state_trans llc_reject_state_trans_2_1 = {
	.ev	       = llc_conn_ev_data_req,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_2_1,
	.ev_actions    = llc_reject_actions_2_1,
};


/* State transitions for LLC_CONN_EV_LOCAL_BUSY_DETECTED event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_3[] = {
	[0] = llc_conn_ac_send_rnr_xxx_x_set_0,
	[1] = llc_conn_ac_set_data_flag_2,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_3 = {
	.ev	       = llc_conn_ev_local_busy_detected,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_reject_ev_qfyrs_3,
	.ev_actions    = llc_reject_actions_3,
};

/* State transitions for LLC_CONN_EV_LOCAL_BUSY_DETECTED event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_4[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_4[] = {
	[0] = llc_conn_ac_send_rnr_xxx_x_set_0,
	[1] = llc_conn_ac_set_data_flag_2,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_4 = {
	.ev	       = llc_conn_ev_local_busy_detected,
	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = llc_reject_ev_qfyrs_4,
	.ev_actions    = llc_reject_actions_4,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_0_UNEXPD_Ns event */
static llc_conn_action_t llc_reject_actions_5a[] = {
	[0] = llc_conn_ac_upd_nr_received,
	[1] = llc_conn_ac_upd_p_flag,
	[2] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_5a = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0_unexpd_ns,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_reject_actions_5a,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_0_UNEXPD_Ns event */
static llc_conn_action_t llc_reject_actions_5b[] = {
	[0] = llc_conn_ac_upd_nr_received,
	[1] = llc_conn_ac_upd_p_flag,
	[2] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_5b = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_0_unexpd_ns,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_reject_actions_5b,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_1_UNEXPD_Ns event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_5c[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_5c[] = {
	[0] = llc_conn_ac_upd_nr_received,
	[1] = llc_conn_ac_upd_p_flag,
	[2] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_5c = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_1_unexpd_ns,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_5c,
	.ev_actions    = llc_reject_actions_5c,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_1_UNEXPD_Ns event */
static llc_conn_action_t llc_reject_actions_6[] = {
	[0] = llc_conn_ac_send_rr_rsp_f_set_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_6 = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_1_unexpd_ns,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_reject_actions_6,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_X event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_7a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_f,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_7a[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_upd_p_flag,
	[3] = llc_conn_ac_send_ack_xxx_x_set_0,
	[4] = llc_conn_ac_upd_nr_received,
	[5] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[6] = llc_conn_ac_stop_rej_timer,
	[7] = NULL,

};

static struct llc_conn_state_trans llc_reject_state_trans_7a = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_x,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_reject_ev_qfyrs_7a,
	.ev_actions    = llc_reject_actions_7a,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_7b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_7b[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_upd_p_flag,
	[3] = llc_conn_ac_send_ack_xxx_x_set_0,
	[4] = llc_conn_ac_upd_nr_received,
	[5] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[6] = llc_conn_ac_stop_rej_timer,
	[7] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_7b = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_reject_ev_qfyrs_7b,
	.ev_actions    = llc_reject_actions_7b,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_8a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_8a[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_send_ack_xxx_x_set_0,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_ac_stop_rej_timer,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_8a = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_reject_ev_qfyrs_8a,
	.ev_actions    = llc_reject_actions_8a,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_8b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_8b[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_send_ack_xxx_x_set_0,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_ac_stop_rej_timer,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_8b = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_reject_ev_qfyrs_8b,
	.ev_actions    = llc_reject_actions_8b,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_reject_actions_9[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_send_ack_rsp_f_set_1,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_ac_stop_rej_timer,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_9 = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_reject_actions_9,
};

/* State transitions for LLC_CONN_EV_RX_RR_CMD_Pbit_SET_0 event */
static llc_conn_action_t llc_reject_actions_10a[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_10a = {
	.ev	       = llc_conn_ev_rx_rr_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_reject_actions_10a,
};

/* State transitions for LLC_CONN_EV_RX_RR_RSP_Fbit_SET_0 event */
static llc_conn_action_t llc_reject_actions_10b[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_10b = {
	.ev	       = llc_conn_ev_rx_rr_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_reject_actions_10b,
};

/* State transitions for LLC_CONN_EV_RX_RR_RSP_Fbit_SET_1 event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_10c[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_10c[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_10c = {
	.ev	       = llc_conn_ev_rx_rr_rsp_fbit_set_1,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_10c,
	.ev_actions    = llc_reject_actions_10c,
};

/* State transitions for LLC_CONN_EV_RX_RR_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_reject_actions_11[] = {
	[0] = llc_conn_ac_send_ack_rsp_f_set_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_11 = {
	.ev	       = llc_conn_ev_rx_rr_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_reject_actions_11,
};

/* State transitions for LLC_CONN_EV_RX_RNR_CMD_Pbit_SET_0 event */
static llc_conn_action_t llc_reject_actions_12a[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_12a = {
	.ev	       = llc_conn_ev_rx_rnr_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_reject_actions_12a,
};

/* State transitions for LLC_CONN_EV_RX_RNR_RSP_Fbit_SET_0 event */
static llc_conn_action_t llc_reject_actions_12b[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_12b = {
	.ev	       = llc_conn_ev_rx_rnr_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_reject_actions_12b,
};

/* State transitions for LLC_CONN_EV_RX_RNR_RSP_Fbit_SET_1 event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_12c[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_12c[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_12c = {
	.ev	       = llc_conn_ev_rx_rnr_rsp_fbit_set_1,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_12c,
	.ev_actions    = llc_reject_actions_12c,
};

/* State transitions for LLC_CONN_EV_RX_RNR_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_reject_actions_13[] = {
	[0] = llc_conn_ac_send_rr_rsp_f_set_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_13 = {
	.ev	       = llc_conn_ev_rx_rnr_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_reject_actions_13,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_14a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_14a[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_upd_p_flag,
	[3] = llc_conn_ac_resend_i_xxx_x_set_0,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_14a = {
	.ev	       = llc_conn_ev_rx_rej_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_14a,
	.ev_actions    = llc_reject_actions_14a,
};

/* State transitions for LLC_CONN_EV_RX_REJ_RSP_Fbit_SET_X event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_14b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_f,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_14b[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_upd_p_flag,
	[3] = llc_conn_ac_resend_i_xxx_x_set_0,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_14b = {
	.ev	       = llc_conn_ev_rx_rej_rsp_fbit_set_x,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_14b,
	.ev_actions    = llc_reject_actions_14b,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_15a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_15a[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_resend_i_xxx_x_set_0,
	[3] = llc_conn_ac_clear_remote_busy,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_15a = {
	.ev	       = llc_conn_ev_rx_rej_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_15a,
	.ev_actions    = llc_reject_actions_15a,
};

/* State transitions for LLC_CONN_EV_RX_REJ_RSP_Fbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_15b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_15b[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_resend_i_xxx_x_set_0,
	[3] = llc_conn_ac_clear_remote_busy,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_15b = {
	.ev	       = llc_conn_ev_rx_rej_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_15b,
	.ev_actions    = llc_reject_actions_15b,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_reject_actions_16[] = {
	[0] = llc_conn_ac_set_vs_nr,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_resend_i_rsp_f_set_1,
	[3] = llc_conn_ac_clear_remote_busy,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_16 = {
	.ev	       = llc_conn_ev_rx_rej_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_reject_actions_16,
};

/* State transitions for LLC_CONN_EV_INIT_P_F_CYCLE event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_17[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_17[] = {
	[0] = llc_conn_ac_send_rr_cmd_p_set_1,
	[1] = llc_conn_ac_start_p_timer,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_17 = {
	.ev	       = llc_conn_ev_init_p_f_cycle,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_17,
	.ev_actions    = llc_reject_actions_17,
};

/* State transitions for LLC_CONN_EV_REJ_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_18[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[2] = NULL,
};

static llc_conn_action_t llc_reject_actions_18[] = {
	[0] = llc_conn_ac_send_rej_cmd_p_set_1,
	[1] = llc_conn_ac_start_p_timer,
	[2] = llc_conn_ac_start_rej_timer,
	[3] = llc_conn_ac_inc_retry_cnt_by_1,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_18 = {
	.ev	       = llc_conn_ev_rej_tmr_exp,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_18,
	.ev_actions    = llc_reject_actions_18,
};

/* State transitions for LLC_CONN_EV_P_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_19[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_reject_actions_19[] = {
	[0] = llc_conn_ac_send_rr_cmd_p_set_1,
	[1] = llc_conn_ac_start_p_timer,
	[2] = llc_conn_ac_start_rej_timer,
	[3] = llc_conn_ac_inc_retry_cnt_by_1,
	[4] = llc_conn_ac_rst_vs,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_19 = {
	.ev	       = llc_conn_ev_p_tmr_exp,
	.next_state    = LLC_CONN_STATE_AWAIT_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_19,
	.ev_actions    = llc_reject_actions_19,
};

/* State transitions for LLC_CONN_EV_ACK_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_20a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[2] = NULL,
};

static llc_conn_action_t llc_reject_actions_20a[] = {
	[0] = llc_conn_ac_send_rr_cmd_p_set_1,
	[1] = llc_conn_ac_start_p_timer,
	[2] = llc_conn_ac_start_rej_timer,
	[3] = llc_conn_ac_inc_retry_cnt_by_1,
	[4] = llc_conn_ac_rst_vs,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_20a = {
	.ev	       = llc_conn_ev_ack_tmr_exp,
	.next_state    = LLC_CONN_STATE_AWAIT_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_20a,
	.ev_actions    = llc_reject_actions_20a,
};

/* State transitions for LLC_CONN_EV_BUSY_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_reject_ev_qfyrs_20b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[2] = NULL,
};

static llc_conn_action_t llc_reject_actions_20b[] = {
	[0] = llc_conn_ac_send_rr_cmd_p_set_1,
	[1] = llc_conn_ac_start_p_timer,
	[2] = llc_conn_ac_start_rej_timer,
	[3] = llc_conn_ac_inc_retry_cnt_by_1,
	[4] = llc_conn_ac_rst_vs,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_reject_state_trans_20b = {
	.ev	       = llc_conn_ev_busy_tmr_exp,
	.next_state    = LLC_CONN_STATE_AWAIT_REJ,
	.ev_qualifiers = llc_reject_ev_qfyrs_20b,
	.ev_actions    = llc_reject_actions_20b,
};

/*
 * Array of pointers;
 * one to each transition
 */
static struct llc_conn_state_trans *llc_reject_state_transitions[] = {
	 [0] = &llc_common_state_trans_1,	/* Request */
	 [1] = &llc_common_state_trans_2,
	 [2] = &llc_common_state_trans_end,
	 [3] = &llc_reject_state_trans_1,
	 [4] = &llc_reject_state_trans_2,
	 [5] = &llc_reject_state_trans_2_1,
	 [6] = &llc_reject_state_trans_3,	/* Local busy */
	 [7] = &llc_reject_state_trans_4,
	 [8] = &llc_common_state_trans_end,
	 [9] = &llc_reject_state_trans_17,	/* Initiate PF cycle */
	[10] = &llc_common_state_trans_end,
	[11] = &llc_common_state_trans_11a,	/* Timer */
	[12] = &llc_common_state_trans_11b,
	[13] = &llc_common_state_trans_11c,
	[14] = &llc_common_state_trans_11d,
	[15] = &llc_reject_state_trans_18,
	[16] = &llc_reject_state_trans_19,
	[17] = &llc_reject_state_trans_20a,
	[18] = &llc_reject_state_trans_20b,
	[19] = &llc_common_state_trans_end,
	[20] = &llc_common_state_trans_3,	/* Receive frame */
	[21] = &llc_common_state_trans_4,
	[22] = &llc_common_state_trans_5,
	[23] = &llc_common_state_trans_6,
	[24] = &llc_common_state_trans_7a,
	[25] = &llc_common_state_trans_7b,
	[26] = &llc_common_state_trans_8a,
	[27] = &llc_common_state_trans_8b,
	[28] = &llc_common_state_trans_8c,
	[29] = &llc_common_state_trans_9,
	/* [30] = &llc_common_state_trans_10, */
	[30] = &llc_reject_state_trans_5a,
	[31] = &llc_reject_state_trans_5b,
	[32] = &llc_reject_state_trans_5c,
	[33] = &llc_reject_state_trans_6,
	[34] = &llc_reject_state_trans_7a,
	[35] = &llc_reject_state_trans_7b,
	[36] = &llc_reject_state_trans_8a,
	[37] = &llc_reject_state_trans_8b,
	[38] = &llc_reject_state_trans_9,
	[39] = &llc_reject_state_trans_10a,
	[40] = &llc_reject_state_trans_10b,
	[41] = &llc_reject_state_trans_10c,
	[42] = &llc_reject_state_trans_11,
	[43] = &llc_reject_state_trans_12a,
	[44] = &llc_reject_state_trans_12b,
	[45] = &llc_reject_state_trans_12c,
	[46] = &llc_reject_state_trans_13,
	[47] = &llc_reject_state_trans_14a,
	[48] = &llc_reject_state_trans_14b,
	[49] = &llc_reject_state_trans_15a,
	[50] = &llc_reject_state_trans_15b,
	[51] = &llc_reject_state_trans_16,
	[52] = &llc_common_state_trans_end,
};

/* LLC_CONN_STATE_AWAIT transitions */
/* State transitions for LLC_CONN_EV_DATA_REQ event */
static llc_conn_ev_qfyr_t llc_await_ev_qfyrs_1_0[] = {
	[0] = llc_conn_ev_qlfy_set_status_refuse,
	[1] = NULL,
};

/* just one member, NULL, .bss zeroes it */
static llc_conn_action_t llc_await_actions_1_0[1];

static struct llc_conn_state_trans llc_await_state_trans_1_0 = {
	.ev	       = llc_conn_ev_data_req,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = llc_await_ev_qfyrs_1_0,
	.ev_actions    = llc_await_actions_1_0,
};

/* State transitions for LLC_CONN_EV_LOCAL_BUSY_DETECTED event */
static llc_conn_action_t llc_await_actions_1[] = {
	[0] = llc_conn_ac_send_rnr_xxx_x_set_0,
	[1] = llc_conn_ac_set_data_flag_0,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_1 = {
	.ev	       = llc_conn_ev_local_busy_detected,
	.next_state    = LLC_CONN_STATE_AWAIT_BUSY,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_1,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_1_UNEXPD_Ns event */
static llc_conn_action_t llc_await_actions_2[] = {
	[0] = llc_conn_ac_send_rej_xxx_x_set_0,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_upd_vs,
	[3] = llc_conn_ac_stop_p_timer,
	[4] = llc_conn_ac_resend_i_xxx_x_set_0,
	[5] = llc_conn_ac_start_rej_timer,
	[6] = llc_conn_ac_clear_remote_busy,
	[7] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_2 = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_1_unexpd_ns,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_2,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_0_UNEXPD_Ns event */
static llc_conn_action_t llc_await_actions_3a[] = {
	[0] = llc_conn_ac_send_rej_xxx_x_set_0,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_upd_vs,
	[3] = llc_conn_ac_start_rej_timer,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_3a = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0_unexpd_ns,
	.next_state    = LLC_CONN_STATE_AWAIT_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_3a,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_0_UNEXPD_Ns event */
static llc_conn_action_t llc_await_actions_3b[] = {
	[0] = llc_conn_ac_send_rej_xxx_x_set_0,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_upd_vs,
	[3] = llc_conn_ac_start_rej_timer,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_3b = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_0_unexpd_ns,
	.next_state    = LLC_CONN_STATE_AWAIT_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_3b,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_1_UNEXPD_Ns event */
static llc_conn_action_t llc_await_actions_4[] = {
	[0] = llc_conn_ac_send_rej_rsp_f_set_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_upd_vs,
	[3] = llc_conn_ac_start_rej_timer,
	[4] = llc_conn_ac_start_p_timer,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_4 = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_1_unexpd_ns,
	.next_state    = LLC_CONN_STATE_AWAIT_REJ,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_4,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_1 event */
static llc_conn_action_t llc_await_actions_5[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_stop_p_timer,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_ac_upd_vs,
	[5] = llc_conn_ac_resend_i_xxx_x_set_0_or_send_rr,
	[6] = llc_conn_ac_clear_remote_busy,
	[7] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_5 = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_1,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_5,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_0 event */
static llc_conn_action_t llc_await_actions_6a[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_send_rr_xxx_x_set_0,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_ac_upd_vs,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_6a = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_6a,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_0 event */
static llc_conn_action_t llc_await_actions_6b[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_send_rr_xxx_x_set_0,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_ac_upd_vs,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_6b = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_6b,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_await_actions_7[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_data_ind,
	[2] = llc_conn_ac_send_rr_rsp_f_set_1,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_ac_upd_vs,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_7 = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_7,
};

/* State transitions for LLC_CONN_EV_RX_RR_RSP_Fbit_SET_1 event */
static llc_conn_action_t llc_await_actions_8a[] = {
	[0] = llc_conn_ac_upd_nr_received,
	[1] = llc_conn_ac_upd_vs,
	[2] = llc_conn_ac_stop_p_timer,
	[3] = llc_conn_ac_resend_i_xxx_x_set_0,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_8a = {
	.ev	       = llc_conn_ev_rx_rr_rsp_fbit_set_1,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_8a,
};

/* State transitions for LLC_CONN_EV_RX_REJ_RSP_Fbit_SET_1 event */
static llc_conn_action_t llc_await_actions_8b[] = {
	[0] = llc_conn_ac_upd_nr_received,
	[1] = llc_conn_ac_upd_vs,
	[2] = llc_conn_ac_stop_p_timer,
	[3] = llc_conn_ac_resend_i_xxx_x_set_0,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_8b = {
	.ev	       = llc_conn_ev_rx_rej_rsp_fbit_set_1,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_8b,
};

/* State transitions for LLC_CONN_EV_RX_RR_CMD_Pbit_SET_0 event */
static llc_conn_action_t llc_await_actions_9a[] = {
	[0] = llc_conn_ac_upd_nr_received,
	[1] = llc_conn_ac_upd_vs,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_9a = {
	.ev	       = llc_conn_ev_rx_rr_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_9a,
};

/* State transitions for LLC_CONN_EV_RX_RR_RSP_Fbit_SET_0 event */
static llc_conn_action_t llc_await_actions_9b[] = {
	[0] = llc_conn_ac_upd_nr_received,
	[1] = llc_conn_ac_upd_vs,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_9b = {
	.ev	       = llc_conn_ev_rx_rr_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_9b,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_0 event */
static llc_conn_action_t llc_await_actions_9c[] = {
	[0] = llc_conn_ac_upd_nr_received,
	[1] = llc_conn_ac_upd_vs,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_9c = {
	.ev	       = llc_conn_ev_rx_rej_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_9c,
};

/* State transitions for LLC_CONN_EV_RX_REJ_RSP_Fbit_SET_0 event */
static llc_conn_action_t llc_await_actions_9d[] = {
	[0] = llc_conn_ac_upd_nr_received,
	[1] = llc_conn_ac_upd_vs,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_9d = {
	.ev	       = llc_conn_ev_rx_rej_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_9d,
};

/* State transitions for LLC_CONN_EV_RX_RR_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_await_actions_10a[] = {
	[0] = llc_conn_ac_send_rr_rsp_f_set_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_upd_vs,
	[3] = llc_conn_ac_clear_remote_busy,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_10a = {
	.ev	       = llc_conn_ev_rx_rr_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_10a,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_await_actions_10b[] = {
	[0] = llc_conn_ac_send_rr_rsp_f_set_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_upd_vs,
	[3] = llc_conn_ac_clear_remote_busy,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_10b = {
	.ev	       = llc_conn_ev_rx_rej_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_10b,
};

/* State transitions for LLC_CONN_EV_RX_RNR_RSP_Fbit_SET_1 event */
static llc_conn_action_t llc_await_actions_11[] = {
	[0] = llc_conn_ac_upd_nr_received,
	[1] = llc_conn_ac_upd_vs,
	[2] = llc_conn_ac_stop_p_timer,
	[3] = llc_conn_ac_set_remote_busy,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_11 = {
	.ev	       = llc_conn_ev_rx_rnr_rsp_fbit_set_1,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_11,
};

/* State transitions for LLC_CONN_EV_RX_RNR_CMD_Pbit_SET_0 event */
static llc_conn_action_t llc_await_actions_12a[] = {
	[0] = llc_conn_ac_upd_nr_received,
	[1] = llc_conn_ac_upd_vs,
	[2] = llc_conn_ac_set_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_12a = {
	.ev	       = llc_conn_ev_rx_rnr_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_12a,
};

/* State transitions for LLC_CONN_EV_RX_RNR_RSP_Fbit_SET_0 event */
static llc_conn_action_t llc_await_actions_12b[] = {
	[0] = llc_conn_ac_upd_nr_received,
	[1] = llc_conn_ac_upd_vs,
	[2] = llc_conn_ac_set_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_12b = {
	.ev	       = llc_conn_ev_rx_rnr_rsp_fbit_set_0,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_12b,
};

/* State transitions for LLC_CONN_EV_RX_RNR_CMD_Pbit_SET_1 event */
static llc_conn_action_t llc_await_actions_13[] = {
	[0] = llc_conn_ac_send_rr_rsp_f_set_1,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_upd_vs,
	[3] = llc_conn_ac_set_remote_busy,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_13 = {
	.ev	       = llc_conn_ev_rx_rnr_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_await_actions_13,
};

/* State transitions for LLC_CONN_EV_P_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_await_ev_qfyrs_14[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_await_actions_14[] = {
	[0] = llc_conn_ac_send_rr_cmd_p_set_1,
	[1] = llc_conn_ac_start_p_timer,
	[2] = llc_conn_ac_inc_retry_cnt_by_1,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_await_state_trans_14 = {
	.ev	       = llc_conn_ev_p_tmr_exp,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = llc_await_ev_qfyrs_14,
	.ev_actions    = llc_await_actions_14,
};

/*
 * Array of pointers;
 * one to each transition
 */
static struct llc_conn_state_trans *llc_await_state_transitions[] = {
	 [0] = &llc_common_state_trans_1,	/* Request */
	 [1] = &llc_common_state_trans_2,
	 [2] = &llc_await_state_trans_1_0,
	 [3] = &llc_common_state_trans_end,
	 [4] = &llc_await_state_trans_1,	/* Local busy */
	 [5] = &llc_common_state_trans_end,
	 [6] = &llc_common_state_trans_end,	/* Initiate PF Cycle */
	 [7] = &llc_common_state_trans_11a,	/* Timer */
	 [8] = &llc_common_state_trans_11b,
	 [9] = &llc_common_state_trans_11c,
	[10] = &llc_common_state_trans_11d,
	[11] = &llc_await_state_trans_14,
	[12] = &llc_common_state_trans_end,
	[13] = &llc_common_state_trans_3,	/* Receive frame */
	[14] = &llc_common_state_trans_4,
	[15] = &llc_common_state_trans_5,
	[16] = &llc_common_state_trans_6,
	[17] = &llc_common_state_trans_7a,
	[18] = &llc_common_state_trans_7b,
	[19] = &llc_common_state_trans_8a,
	[20] = &llc_common_state_trans_8b,
	[21] = &llc_common_state_trans_8c,
	[22] = &llc_common_state_trans_9,
	/* [23] = &llc_common_state_trans_10, */
	[23] = &llc_await_state_trans_2,
	[24] = &llc_await_state_trans_3a,
	[25] = &llc_await_state_trans_3b,
	[26] = &llc_await_state_trans_4,
	[27] = &llc_await_state_trans_5,
	[28] = &llc_await_state_trans_6a,
	[29] = &llc_await_state_trans_6b,
	[30] = &llc_await_state_trans_7,
	[31] = &llc_await_state_trans_8a,
	[32] = &llc_await_state_trans_8b,
	[33] = &llc_await_state_trans_9a,
	[34] = &llc_await_state_trans_9b,
	[35] = &llc_await_state_trans_9c,
	[36] = &llc_await_state_trans_9d,
	[37] = &llc_await_state_trans_10a,
	[38] = &llc_await_state_trans_10b,
	[39] = &llc_await_state_trans_11,
	[40] = &llc_await_state_trans_12a,
	[41] = &llc_await_state_trans_12b,
	[42] = &llc_await_state_trans_13,
	[43] = &llc_common_state_trans_end,
};

/* LLC_CONN_STATE_AWAIT_BUSY transitions */
/* State transitions for LLC_CONN_EV_DATA_CONN_REQ event */
static llc_conn_ev_qfyr_t llc_await_busy_ev_qfyrs_1_0[] = {
	[0] = llc_conn_ev_qlfy_set_status_refuse,
	[1] = NULL,
};

/* just one member, NULL, .bss zeroes it */
static llc_conn_action_t llc_await_busy_actions_1_0[1];

static struct llc_conn_state_trans llc_await_busy_state_trans_1_0 = {
	.ev	       = llc_conn_ev_data_req,
	.next_state    = LLC_CONN_STATE_AWAIT_BUSY,
	.ev_qualifiers = llc_await_busy_ev_qfyrs_1_0,
	.ev_actions    = llc_await_busy_actions_1_0,
};

/* State transitions for LLC_CONN_EV_LOCAL_BUSY_CLEARED event */
static llc_conn_ev_qfyr_t llc_await_busy_ev_qfyrs_1[] = {
	[0] = llc_conn_ev_qlfy_data_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_await_busy_actions_1[] = {
	[0] = llc_conn_ac_send_rej_xxx_x_set_0,
	[1] = llc_conn_ac_start_rej_timer,
	[2] = NULL,
};

static struct llc_conn_state_trans llc_await_busy_state_trans_1 = {
	.ev	       = llc_conn_ev_local_busy_cleared,
	.next_state    = LLC_CONN_STATE_AWAIT_REJ,
	.ev_qualifiers = llc_await_busy_ev_qfyrs_1,
	.ev_actions    = llc_await_busy_actions_1,
};

/* State transitions for LLC_CONN_EV_LOCAL_BUSY_CLEARED event */
static llc_conn_ev_qfyr_t llc_await_busy_ev_qfyrs_2[] = {
	[0] = llc_conn_ev_qlfy_data_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_await_busy_actions_2[] = {
	[0] = llc_conn_ac_send_rr_xxx_x_set_0,
	[1] = NULL,
};

static struct llc_conn_state_trans llc_await_busy_state_trans_2 = {
	.ev	       = llc_conn_ev_local_busy_cleared,
	.next_state    = LLC_CONN_STATE_AWAIT,
	.ev_qualifiers = llc_await_busy_ev_qfyrs_2,
	.ev_actions    = llc_await_busy_actions_2,
};

/* State transitions for LLC_CONN_EV_LOCAL_BUSY_CLEARED event */
static llc_conn_ev_qfyr_t llc_await_busy_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_data_flag_eq_2,
	[1] = NULL,
};

static llc_conn_action_t llc_await_busy_actions_3[] = {
	[0] = llc_conn_ac_send_rr_xxx_x_set_0,
	[1] = NULL,
};

static struct llc_conn_
 * e_transc - Tawait_busymodule conta_3 = {
	.ev	 onent.=c - This mev_localtransicleared,
	.nextmodulent.
 *LLC_CONN_STATE_AWAIT_REJionsev_qualifiers
 *
 * tate transi
 * fyrs_3ard,
 *actionsnt.
 *
 * tate transi
 *
 *  mod};

/* Sctioncontai*
 * Cforere is in EV_RX_I_RSP_Fbit_SET_1_UNEXPD_Ns event */*
 * llc - This m
 *
 *_.c - T(c) 1997 by Procom 4[]ction [0ied  This progr_opt_send_rnr_xxx_x_set_0,
	[1 the terms of thupd_nr_receivction[2se as published by tvstwar3 the terms of thstop_p_timertwar4m is distributed et_data_flag_1twar5 the terms of tht fun_remotetranstwar6 the terms of thre GNU ieral Public Lice7ied NULLechnol
 * llc_c_st.c - This module contains state transition of conn4ction component.
 *
 * Descriptrx_i_rsp_fe Mewarr1_unexy thprogs and actions there is in 802.2 BUSYard,
 * or in "llc_cNONEodules.
 *
 * Copyright (c) 1997 by Procom 4echnology, Inc.
 * 		 2001-2003 by Arnaldo CarvaCMD_Pe Melo <0cme@conectiva.com.br>
 *
 * This program can be redistributed or mod5afied under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * This program is distributed arranty
 * of mercrantycense for more details.
 */
#include <linux/types.h>
#include <net/llc_i5a.h>
#include <net/llc_sap.h>
#includcmd_pt/llc_c_0v.h>
#include <net/llc_c_ac.h>
#include <net/LLC stllc_c_st.h>

#define NONE NULL

/* COMMON CONNECTION STATE transitions
5aechnology, Inc.
 * 		 2001-2003 by Arnaldo Carvalho de Melo <N_STATE_BUSY,
 * LLC_CONN_STATE_REJ,
 * LLC_CONN_STATE_AWAIT,
 * LLC_CbNN_STATE_AWAIT_BUSY and
 * LLC_CONN_STATE_AWAIT_REJ states
 */
/* State transitions for LLC_CONN_EV_DISC_REQ event */
static llc_conn_action_t llc_common_actions_1[] = {
	[0] = llc_conn_ac_send_disc_cmd_p_set_x,
	[1] = llc_conn_ac_b.h>
#include <net/llc_sap.h>
#include <net/llc_c_ers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_1,
	[5] = NULL,
};

static struct llc_conn_state_b* Common transitions for
 * LLC_CONN_STATE_NORMAL,
 * LLC_CONacme@conectiva.com.br>
 *
 * This program can be redistributed or mod6fied under the terms of th GNU Genee <nelc_c_eJ states
 */
/* State transitions for LLC_CONN_EV_DISC_REQ event */
static llc_conn_action_t llc_common_actions_1[] = {
	[0] = llc_conn_ac_send_disc_cmd_p_set_x,
	[1] = llc_conn_ac6tart_ack_timer,
	[2] = llc_conn_ac_stop_other_timev.h>
#include <net/llc_c_ac.h>
#include <net/= llc_conn_ac_set_cause_flag_1,
	[5] = NULL,
};

static struct llc_conn_state6echnology, Inc.
 * 		 2001-2003 by Arnaldo Carvalho de Melo <aiva.com.br>
 *
 * This program can be redistributed or mod7fied under the terms of the
 * GNU General Public License as published inc_vr_bynsitioe Foundation.
 * anty
inftwaram is distributed without any warranty or implied by the Free Softwarhantability or fiThis progrr purpose.
 *
 * action_t llc_c Public Libility or fitness for a particula8 purpose.
 *
 * See the GNU General Publ9actions_1[] = {
	[0] = llc_conn_ac_send_disc_cmd_p_set_x,
	[1] = llc_conn_ac7.h>
#include <net/llc_sap.h>
#include <net/llc_c_eude <net/llc_c_ac.h>
#include <net/llc_c_st.h>

#define NONE NULL

/* COMMON CONNECTION STATE transitions
7rans llc_common_state_trans_1 = {
	.ev	       = llc_conn_ev_diiva.com.br>
 *
 * This program can be redistributed or mod8ONN_STATE_AWAIT_BUSY and
 * LLC_CONN_STATE_AWAIT_REJ states
 */
/* Stateifiers = NONE,
	.ev_actions    = llc_common_actions_3,
};

/*by the Free Softwarns for LLC_CONN_EV_RX progrhantability or ficommon_actions_4[] r purcense for more details.
 */
#include <linux/types.h>
#include <net/llc_i8start_ack_timer,
	[2] = llc_conn_ac_srs,
	[3] = llc llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_1,
	[5] = NULL,
};

static struct llc_conn_state8trans llc_common_state_trans_1 = {
	.ev	       = L,
 * LLC_CONN transitions for LLC_CONN_EV_RX_FRMR_RSP_Fbit_SET_X event *NE,
	.ev_actions    = llc_common_actions_1,
};

/* State transitions for_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_rst_ind,
	[5] = llc_conn_ac_set_cause_flag_0,
	[6] = llc_conn_resetr,
	[2] = llc_conn_ac_stop_other_timetop_other_timens llc_common_state_trans_5 = {
	.ev	       = llc_conn_ev_rx_frmr_rsp_fbit_set_x,
	.next_state    = LLC_CONN_STATon_state_trans_2 = {
	.ev	       = llc_conn_ev_rst_req,
	.next_iva.com.br>
 *
 * This program can be redistributed or mod9    = llc_common_actions_2,
};

/* State transitions for LLC_CONN_E_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_rst_ind,
	[5] = llc_conn_ac_set_cause_flag_0,
	[6] = llc_conn_rese9lc_conn_ac_set_vr_0,
	[3] = llc_conn_ac_send_ua_rs llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_1,
	[5] = NULL,
};

static struct llc_conn_state9echnology, Inc.
 * 		 2001-2003 by Arnaldo CarRRnn_state_trans llc_common_state_trans_3 = {
	.ev	       = llc_conn_ev_rx_10ONN_STATE_AWAIT_BUSY and
 *by the Free Softwarnse as published by t progre Foundation.
 *  without any waram is distributedSee the GNU General Publranty or implied tness for a particulahantacense for more details.
 */
#include <linux/types.h>
#include <net/llc_i10start_ack_timer,
	[2] = llc_conn_acr* Statee_trans_4 = {
	.ev	       = llc_conn_ev_rx_disc_cmd_pbit_set_x,
	.next_state    = LLC_CONN_STATE_ADM,
	.ev_qualifie10trans llc_common_state_trans_1 = {
	.ev	       REJ_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_sNE,
	.ev_actions    = llc_cc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_7b = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_x_inval_ns,
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_actr,
	[2] = llc_conn_ac_stop_other_tirej/* State transitions for LLC_CONN_EV_RX_ZZZ_RSP_Fbit_SET_X_INVAL_Nr event */
static llc_conn_action_t llc_common_actions_8aon_state_trans_2 = {
	.ev	       = llc_conn_ev_RR = llc_common_actions_5,
};

/* State transitions for LLC_CONN_EV_RX_DM_R11top_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_ctness for a particulaam is,
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_ac1ions    = llc_common_actions_7b,
};

/ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_6,
};

/* State transitions for LLC_CONN_EV_RX_ZZZ_CMD_Pbit_SET_X_I11[] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,d_frmr_rsp_f_se
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_b = {
	.ev	       = llc_conn_ev_rx_i_rsp_fbit_set_x_inval_ns,
	.next_state    = LLC_CONN_STATE_ERRORon_actions_8a,
};

/* State transiti
/* State transmon_actions_8b,
};

/* State transitions for LLC_CONN_EV_RX_BAD_PDU event */
static llc_conn_action_t llc_common_acn_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_sEJart_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_setcmers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8c = {
	.ev	       = llc_conn_ev_rx_bad_pdu,
	.next_state    = LLC_CONN_cn_actions_8a,
};

/* State transitions    = llc_common_actions_8b,
};

/* State transitions for LLC_CONN_EV_RX_BAD_PDU event */
static llc_conn_action_t llc_common_acc] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_setdmers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8c = {
	.ev	       = llc_conn_ev_rx_bad_pdu,
	.next_state    = LLC_CONN_dn_actions_8a,
};

/* State transitions for LLC_CONNmon_actions_8b,
};

/* State transitions for LLC_CONN_EV_RX_BAD_PDU event */
static llc_conn_action_t llc_common_acd_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timer,t_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_2ONN_STATE_AWAIT_BUSY and
 *,
};

/* State transitions for LLC_CONN_EV_RX_SABME_CMD_Pbit_SET_X event */
static llc_conn_action_t llctness for a particula_actions_1[] = {
	[0] = llc_conn_ac_send_disc_cmd_p_set_x,
	[1] = llc_conn_ac12,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_comns_7a,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_X_INVAL_Ns event */
static llc_conn_action_t ll12[] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,
	[_qfyrs_10,
	.ev_actions    = llc_common_actions_10,
};
#endif

/* State trNE,
	.ev_actions    = llc_c_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_common_ev_qfyrs_11a[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_gte_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_common_actions_11a[] = {
	[0] = llc_conn_aon_actions_8a,
};

/* State transitionsonn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_fn_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_sNd_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8a = {
	.ev	  actie_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_common_actions_11a[] = {
	[0] = llc_conn_ection component.
 *
 * Descriptsiti/* Statete transitions for LLC_CONN_EV_RX_ZZZ_RSP_Fbit_SET_X_INVAL_Nr event */
static llc_conn_action_t llc_common_actions_8Technology, Inc.
 * 		 2001-2003 by Arnaldo Carlc_crt_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_se4top_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_colc_conn_ev_ack_tmrlc_conn_ev_rx_i_rsp_fbit_set_x_inval_ns,
	.next_state    = LLC_CONN_STATE_ERRO4ions    = llc_common_actions_7b,
};
nns    = llc_common_actions_8b,
};

/* State transitions for LLC_CONN_EV_RX_BAD_PDU event */
static llc_conn_action_t llc_common_a4[] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,c_conn_ac_stop_o,
};

static llc_conn_action_t llc_common_actions_11c[] = {
	3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_cnn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_0,on_actions_8a,
};

/* State transitimon_actions_11b, llc_common_state_trans_11c = {
	.ev	       = llc_conn_ev_rej_tmr_exp,
	.next_state    = LLC_CONN_STATE_RESET,
	.evx,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_c_qfyrs_10,
	.ev_actions    = llc_common_actions_10,
};
#endif

/* State t5    = llc_common_actions_2,
};

/* State transitions for LLC_CONN_EV_RX_SABME_CMD_Pbit_SET_X event */
static llc_conn_action_t llc_come_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_common_actions_11a[] = {
	[0] = llc_conn_5	[5] = NULL,
};

static struct llc_conn_state_transack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_5echnology, Inc.
 * 		 2001-2003 by Arnaldo P_TMR_EXPiva.com.br>
 *
 * This pro_c_ev.can be redistribute_c_ev.c" 1s    = llc_common_action
 * lfy_retry_cnt_lt_n2License acense for more dec_stop_other_timers,
	[3] = llc_conn_ac_ses    = llc_common_actions_2,
};

/* top_o transitions for LLC_CONN_Estarthout any ware Foundation.
 * ifiellc_conn_a= NONE,
lc_conn_ev_rx_i_rsp_fbit_set_x_inval_ns,
	.next_state    = LLC_CONN_STATE_ERROllc_conn_ac_set_vr_0,
	[3] = llcp_tmr_exp llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_1[] = {
	[0] = llc_conn_ac_t_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_onn_reset
 * Array of pointers;et_pone to each.
 * 		 200
m.br>
 *
 *_c_st.c - This module contai*ns state transition of con	 2001fied und er the& - Thimmo module conta_1,		/* Requesom.br	 y_cnt_] = llc_conn_ac_set_remoart_ ns_1 =] = la[] = {
	[0] = llc_conn_lc_co _req,
] = llc_conn_ac_set_remoeommon ULL,
}ic struct llc_conn_state_trane_busyLon o rans] = llchantaic struct llc_conn_state_traL,
};
r puric struct llc_conn_state_tramodu lic Liate_trans_2 = {
	.ev	       = ll,
	[1ate_trans_2 = {
	.ev	       _busyInititionPF cycle] = llc
	[2]] = llc_conn_ac_set_remot1ant */
T any] = l[1,
	[5] = llc_conn_ac_set_remotacti	[1_conn_ac_conn_ind,
	[7] = NUL_1 ev	[1
static stL,
};

static struct retry_dm_state_tifiers = NONE,
	.ev_actiolc_coc_conn_ev_rans_2 = {
	.ev	       = [1CONN_STATE_NORMAL,
	.ev_qualifier4e_busy_0ee So framaction[1NE,
	.ev_actions    = llc_adm_act_tra	[1,
};

/* Sc_conn_ac_start_ack_timertionsNN_EV_RX_Dnn_ac_set_vs_0,
	[2] = ltions llc_adm_a4] = NULL,
};

static sttware,
	[5] = l0,
	[6] = llc_conn_reset,tware_conn_ac_cx_dm_rsp_fbit_set_x,
	.netware
static struct llc_conn_state_tra9tware= {
	.ev	       = llc_conn_ev_rx_da[] 	[2c_conn_ev_rx_sabme_cmd_pbit_set_x,nn_a	[2CONN_STATE_NORMAL,
	.ev_qualifieractio	[2NE,
	.ev_actions    = llc_adm_actn_disc,2 for LLC_CONN_EV_RX_XXX_CMD_Pbit_t llc_c2vent */
static llc_conn_action_t trans_32m_actions_4[] = {
	[0] = llc_conn_flag	[3nd_dm_rsp_f_set_1,
	[1] = llc_cont_x,
	[3
	[2] = NULL,
};

static struct ls_11	[3
static struct llc_conn_state_tranv_qu	[3= {
	.ev	       = llc_conn_ev_rx_dnn_a	[3c_conn_ev_rx_sabme_cmd_pbit_set_x,5lifieCONN_STATElc_conn_ac_set_remo	     NE,
	.ev_alc_conn_ac_set_remo4lifie,
};

/* State transitions fodm_actNN_EV_RX_DISC_CMD_Pbit_SET_X llc_a3 llc_adm_actions_3[] = {
	[0]7t_sta4,
	[5] = llc_conn_ac_set_remo7alifi4_conn_ac_conn_ind,
	[7] = NULn_disc4nn_state_trans llc_adm_state_lc_con4dm_state_trans_2 = {
	.ev	   8llc_c4d_pbit_set_x,
	.next_state   4 = /* [4ions_5,
};

/*
 * Array of po10,  = llend,	/* timer */
	[5] = &llc_a    =hnology-adm_state_trans_ere is in 802.2 LLC stand.
 * 		 2001-m_state_trans_3,*/logy, Inc.
 * 		 2001-2003 by Arnaldo DATAis in REQ llc_conn_action_t llc_adm_actions_1[] = {
	[0rejectllc_conn_ac_0send_sabme_cmd_p_set_x,
	[1] = _ev_
 * us_refusert_ack_timer,
	[2] /* justset_vmember,imer,
 .bss zeroes iom.br>
 *
 * This program can be redistrransitiet_cause__0[1]or more details.
 */
#include <linux/types.h>
#iransitinn_state_trans ction component.
 *
 * Descriptanty
reqions and actions there is in 802.2 LLC standard,
 * or in "llc_c_ac.c" andransitions for LLC_odules.
 *
 * Copyright (c) 19n_ac_set_vs_0,
	[2echnology, Inc.
 * 		 2001-2003 by Arnaldo LOCAL_conn_DETECTEDiva.com.br>
 *
 * This program can be redistrrej_set_vs_0,
	
	.next_state    = LLC_CONN_STATE_Rral Public License as published action_t llc_cart_ae Foucens for more details.
 */
#include <linux/types.h>
#i */
st_ac_set_remotction component.
 *
 * Description of evendetectctions and actions there is in 802.2 LLC stllc_c_st.h>

#define NONE NULL

/* COMMON CONNECTION STATE */
static llc_* Common transitions for
 * LLC_CONN_STATE_NORMAL,
 * LLC_CONN_STATE_BUSY,
 * LLC_CONN_STATE_REJ,
 * LLC_CONN_STATE_A */
static llcransitions for LLC_CONN_EV_Pc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static onn_ev_qlfy_set_status_conn,
	[2] = NULL,
};

static llc_conn_action_t lac_send_sabme_cmd_p_set_x,
	[1] = ll_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_andard,
 * or in "llc_c,
	[3] = llc_conn_ac_upd_p_flag,
	[4] = llc_connflag_0,
	[5] = NULL,
};

static struct llc_conn_= llc_conn_ev_disc_req,
	.next_state    = LLC_CONN_STATE_D_CONN,
	.ev_qs_flag_eq_1,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_statSTATE_NORMAL,
	.ev_qualifiers = llc_setup_ev_qfyrs_2,
	.ev_actions    = lr,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cafyr_t llc_setup_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_s_flag_eq_1,
	[on_state_trans_2 = {
	.ev	       = llc_conn_ev_rst_req,
	.next_state    = LLC_CONN_STATE_RESET,
	.ev_qualifiers = NONE */
static llcconn_ac_set_cause_flag_0,
	 GNU G* State transitions for LLC_CONN_EV_RX_SABME_CMD_Pbit_SET_X event */
static llc_conn_onn_ev_qlfy_set_status_conn,
	[2] = NULL,
};

static llc_conn_action_t lqfyrs_11b,
	.ev_actions    = llc_conn_ac_send_ua_rsp_f_set_p,
	[4] = llc_conn_ac_rst_ind,
	[5] = llc_cfyr_t llc_setup_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_s_flag_eq_1,
	Technology, Inc.
 * 		 2001-2003 by Arnaldo Carvalho de Melo <alc_conn_ev_qfyr_t llc_setup_ev_qfyrs_4[] = {
	[0] = llc_conified under the terms of thifiers = NONE,
nse as published  llc_common_rans llc_common_state_trans_11b = {
	.ev	       = lwithionstransitions for LLC_CONN_EV_RX_DISC_CMD_Pbit_SET_X event */
static llc_conn_action_t llc_See the GNU General _or
	[1] = 4[] = {
	[0] = llc_conn_ac_stop_all_timers,
	[1cense for more details.
 */
#include <linux/types.h>
#ilc_conn_action_t lf.h>
#include <net/llc_sap.h>
#include <net/llc_c_eude <net/llc_c_ac.h>
#include <net/NORMAL_t llc_setup_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_s_flag_eq_1,
	 * Common transitions for
 * LLC_CONN_STATE_NORMA    = llc_common_actions_11c,
};

/* State transitions for  */
static llcCONN_STATE_AWAIT_BUSY and
 *_RX_DM_RSP_Fbit_SET_X event */
static llc_conn_ev_qfyr_t llc_s[1] = Nral Public Lice[] = {
	[0] = llc_conn_ev_qlfy_set_status_disc,
	[1] = NULL,
};

static llc_conn_action_t llc_setup_actc_conn_disc,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_setu_start_ack_timer,
	[2] = llc_conn_ac_s_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_con_t llc_setup_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_s_flag_eq_1,
	_trans llc_common_state_trans_1 = {
	.ev	       = = llc_common_actions_5,
};

/* State transitions for LLC_COSETUP,
	.ev_quaNE,
	.ev_actions    = llc_c_conn_ev_qlfy_retry_cnt_lt_n2,
	[1] = llc_conn_ev_qlfy_s_flag_eq_0,
	[2] = NULL,
};

static llc_conn_action_t llc_setup_actions_7[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_inc_retry_cnt_by_1,
	[3] = ext_state    = LLC_CONN_STATE_ADM,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actconn_ev_ack_tmr_exp,
	.next_state    = LLC_CONN_STATE_SETUP,
	.ev_quaVAL_Nr event */
static llc_conn_action_t llc_common_actions_7a[] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_ */
static llcs    = llc_common_actions_2_conn_ev_qlfy_retry_cnt_lt_n2,
	[1] = llc_conn_ev_qlfy_s_flag_eq_0,
	State transitio
static llc_conn_action_t llc_setup_actions_7[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_inc_retry_cnt_by_1,
	[3] =llc_conn_ac_set_vr_0,
	[3] = llc_conn_ac_send_ua_rsllc_conn_state_trans llc_setup_state_trans_8 = {
	.ev	       = llc_conn_ev_ack_tmr_exp,
	.next_state    = LLonn_reset,
	[8] = NULL,
};

static struct llc_cnd_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2]  */
static llc7top_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_7b = {
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_x_inval_ns,
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qualifielc_conn_action_t l7ions    = llc_common_actions_7b,
};

/* State transitions for LLC_CONN_EV_RX_ZZZ_RSP_Fbfyr_t llc_setup_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_s_flag_eq_1,
	 Req= {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_os_ack,
	[1] = l3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8a = {
	.ev	       = llc_conn_ev_rx_zzz_rsp_fbit_set_x_inval_nr,
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_acconn_ev_qlfy_p_flagon_actions_8a,
};

/* State transitions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_X_INVAL_Ns eve_t llc_normal_actions_1[] = {
	[0] = llc_conn_ac_send_i_as_ack,
	[1] = lon_state_trans_2 = {
	.ev	       = llc_conn_ev_rslho de Melo <acme@conectiva.com.br>
 *
 * This program can be redistrs_ack,
	[1] = lonn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static stru= llc_normal_ev_qfyrs_1,
	.ev_actions    = llc_normal_actions_1,
};

/* State transitions for LLC_CONN_EV_DATA_REQ event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_2[] 
	.next_state    = LLC_CONN_STATE_Eude <net/llc_c_ev.h>
#include <net/llc_c_ac.h>
#include <net/_t llc_normal_actions_1[] = {
	[0] = llc_conn_ac_send_i_as_ack,
	[1] = l event */
#if 0
static llc_conn_ev_qfyr_t llc_con2,
	[1] = NULL,
};

static llc_conn_action_t llc_common_acti */
static llc*/
static llc_conn_action_t et_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8c = {
	.ev	       = llc_conn_ev_rx_bad_pdu,
	.next_slc_conn_action_t lt,
	[7] = NULL,
};

static struct llons    = llc_common_actions_8b,
};

/* State transitions ffyr_t llc_setup_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_s_flag_eq_1,
	TE_RESET,
	.ev_qualifiers = NONE,
	.ev_actions  _f_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_nn_ev_qlfy_p_flimers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8c = {
	.ev	       = llc_conn_ev_rx_bad_pdu,
	.next_sns    = llc_normal_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_8c,
};

/* State tr_conn_ev_qfyr_t llc_normal_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_p_flction_t llc_common_actions_9[] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timenn_ev_qlfy_p_flonn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_9 = {
	.ev	       = llc_conn_ev_rxns    = llc_normal_
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common__conn_ev_qfyr_t llc_normal_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_p_fl event */
#if 0
static llc_conn_ev_qfyr_t llc_common_ev_qfyrs_10[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[nn_ev_qlfy_p_flstatic llc_conn_action_t llc_common_actions_10[] = {
	[0] = llc_conn_ac_send_frmr_rsp_f_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[ns    = llc_normal_t_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_10 =_conn_ev_qfyr_t llc_normal_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_p_fl_CONN_STATE_ERROR,
	.ev_qualifiers = llc_common_ev_qfyrs_10,
	.ev_actions    = llc_common_actions_10,
};
#end */
static llc9ansitions for LLC_CONN_EV_P_TMR_EP event */
static llc_conn_ev_qfyr_t llc_common_ev_qfyrs_11a[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_gte_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_common_actions_11a[] = lc_conn_action_t l9c_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_tfyr_t llc_setup_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_s_flag_eq_1,
	9lag_0,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_11a = {
	.ev	       = llc_coL,
};

static l,
	.next_state    = LLC_CONN_STAT llc_conn_ev_rx_i_cmd_pbit_set_0_unexpd_ns,
	.next_state    = LLC_CONN_STATE_REJ,
	.ev_qualifiers = llc_normal_ev_qfyrs_5a,
	.ev_actions    = llc_normal_actions_5a,
};

/* State transitions for LLCyrs_11b[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_gte_n2,
	[1] = NULL,
};

static llc_conn_actio_5b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static lx,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_ */
static llc__CONN_EV_RX_SABME_CMD_Pb,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_11b = {
	.ev	       = llc_conn_ev_ack_tmr_exp,
	.next_state    = LLC_CONN_STATE_RESET,
	.ev_qualifierlc_conn_action_t ll};

static struct llc_conn_state_common_actions_11b,
};

/* State transitions for LLC_COfyr_t llc_setup_ev_qfyrs_3[] = {
	[0] = llc_conn_ev_qlfy_s_flag_eq_1,
	1,
	.ev_actions    = llc_setup_actions_1,
};
_gte_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_common_acti */
static llc_t_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8_ev_qlfy_retry_cnt_gte_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_commonlc_conn_action_t llR,
	.ev_qualifiers = NONE,
	.ev_actioconn_state_trans llc_common_state_trans_11c = {
	.ev	     ,
	[5] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[6] = NULL,
};

statictions_8c[] = {
	[0] = llc_conn_ac_send_frmr_rsp_ons    = llc_common_actions_11c,
};

/* State transitions for ions_6a[] = {
	[Y_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_common_ev_qfyrs_11d[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_gte_n2,
	[1] = NULL,
};

static llc_conn_action_t llc_commonT_0_UNEXPD_Ns event 	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_6a[] = {
	[n_ac_set_cause_flag_0,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_11d = {
	.ev */
static llc_2_ev_qlfy_set_status_disc,
	[1] = NULL,
};

static llc_conn_action_t llc_setup_actions_4[] = {
	[0] = llc_conn_ac_sendl_actions_5c[] = {
	[0] = llc_conn_ac_rst_sendack_flag,
	[1] = llc_conn_ac_send_rej_xxx_x_set_0,
	[2] = llc_co2ition groups - it'll be on .bss, so will be zeroed.
 */
static struct llc_conn_state_trans ll,
	[5] = llc_conn_ac_clear_remote_busy_if_f_eq_1,
	[6] = NULL,
};

stati2transitions for LLC_CONN_EV_CONN_REQ event */
static llc_conn_action_t llc_adm_actions_1[] = {
	[0 */
stlc_conn_acconn_ac_set_cause_flag_0
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_set_retry_cnt_0,
	[3] */
static llc_n_ev_qlfy_set_status_disc,
	[1] =y_retry_ruct llc_conn_state_trans llcithout any wars_1 = {
	.ev	       = llc_conn_ev_conn_req,
	.next_state    = LLC_CONN_STATE_SETUP,
	.ev_qualifierslc_conn_action_t llection component.
 *
 * Descript/* State transitions for LLC_CONN_EV_RX_SABME_CMD_llc_conn_ev_rx_sabme_cmd_pbit_set_x,_I_CMD_Pbit_SE3] = llc_conn_ac_upd_p_flag,
	[4] = llc_conn_Technologet_p,
	[1] = llc_conn_ac_set_vs_0,
	[2] = llc_conn_ac_set_vr_0,
	[3] = llc_conn_ac_set_retry_cnt_0,lc_conn_action_t _ac_set_p_flag_0,
	[5] = lset_s_flag_1,
	[4] = NULL,
}ons_2	[2] = NULL,
};

static structnt */
r0,
	[6s] = llcnn_state_trans llc_adm_state_L,
};
dm_state_trans_2 = {
	.ev	       = llc_conn_ev_rx_sablc_conn_action_t llnt */
ion ote    = LLC_CONN_STATErans_2 = {
	.ev	       = ll;
 * one to each transition
 event */
static llc_conn_action_t for LLC_CONN_EVs_7 = {
	.ev	       ,ranst any_by_1,
	NN_EV_RX_DISC_CMD_Pbit_SET_X _actio_t llc_adm_actions_3[] = {
	[0] =ET_1 em_rsp_f_set_p,
	[1] = llc_conn_llc_co	[2] = NULL,
};

static struct  LLC_Cnn_state_trans llc_adm_state_ = LLC_C= {
	.ev	       	.ev_actions    = ll.ev	 Free Soions  _by_1,cmd_pbit_setans llc_setup_state_trans_LLC_CONN_STATE_ADM,
	2] = llc_conn_ac_cotionsNE,
	.ev_actionsate_trans llc_setuptions for LLC_CONN_EVy_cnt_by_1,
	[3] = NT_1 event */
static lac_conn_confirm,
	[1lc_adm_actions_4[] = ] = &llc_setup_statac_send_dm_rsp_f_set_conn_ev_qlfy_p_flag_disc,
	[2] = NULL,
};ormal_ev_qfyrs_2[] =_conn_state_trans llcCONN_EV_DATA_REQ eve = {
	.ev	       = llns    = llc_normal_a_pbit_set_1,
	.next_s_ev_local_busy_detecTATE_ADM,
	.ev_qualif_flag_0,
	[3] = NULLtions    = llc_adm_ac_actions_5a[] = {
	[e transitions for LLCtransitions for LLC_event */
static llc_c_state_trans_5b = {
_actions_5[] = {
	[0]et_0,
	[2] = llc_con1] = NULL,
};

staticT_0_UNEXPD_Ns event *te_trans llc_adm_stattate    = LLC_CONN_ST       = llc_conn_ev_
static struct llc_c_state    = LLC

/*
 * Array of pointersd_pbit_set_x,
	.next_state   */
stay_if_f_eq_1,
	[5] = llc_conn_dm_act;
 * one to each transition
 
	[0] ,
};

/* State transitions fo
	.ev_lc_adm_state_transitions[] = {};

/* = &llc_adm_state_trans_1,		/*_trans_,
	[5] = llc_conn_ac_set_remo&llc_coend,
	[2] = &llc_common_state_pf_cycnn_state_trans llc_adm_state_e_trans_edm_state_trans_2 = {
	.ev	   dm_state_tdm_state_trans_2 = {
	.ev	       =hnologyre is in 802.2 Dis in.
 * 		 2001-_trans_5,
	[9] = &llc_common_state_transRX_SABME= llc_setup_acXiva.co,et_pcause
 * o = 1onn_action_t llc_adm_actions_1[] =dlc_adm_actionlc_conn_ev_qfyr_t llc_setu
	[1] = static str_eqnsitions for LLC_CONit_SET_X event */
sconflictconfirm,
	[3]
	[2] = llc_conn_ac_set_retry_cnt_0ate_traatic llc_conn_ev_qfyr_t llc_setup_ev_qfydms[] = {
	 [ traonn_ac_upd_nr_received,ack	[3] = llc_conn_ac_start_redis Thisfirm] = &llc_common_st_act= NULL,
};

static llc_conn_action_t llc_common_actions_11ate_tran_action_t llc_setup_actions_2[] = {
	[0] = rx_sabmeretry_cnt_gte_xions and actions there is in 802.2 LDMard,
 * or in "llc_c_ac.ate_trans_9b = {
odules.
 *
 * Copyright ons    = llc_norc_conn_ac_start_ack_tmr_if_not_running,
	[2] =ck_if_needed,
	[4] = NULL,
};

static struct0nn_ac_set_vrate_trans llc_normal_state_trans_9b = {
{
	.ev	       = llc_conn_ev_rx_i_cmd_pbit_set_0 License as publish LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_9b,
	.ev_actions    = llc_norormal_actions_9b,
};

/* State transitions for LLC_CONN_EV_RX_I_CMD_Pbit_SET_1 event */
static llc_c0[] = {
lc_conn_ev_rx_i_rsp_fbit_set_x_inval_ns,
	.next_state d_ack_rsp_f_set_1,
	
	[2] = llc_conn_ac_rst_sendack_flag,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_ac_data_ind,
	[5] = NULL,
};

static struct l llc_conn_state_trans llc_normal_state_trarans_10 = {
	.ev	       = llc_conn_ev_rx_i_cmd_pUA_ac_send_i_cmd= NULL,
};

static struct ORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = ll_conn_action_t llc_norma
	[1] = pbit_set_0f * LLC_CONN_EV_RX_RR_CMD_Pcmd_pbit_set_0,
	.ne Foundation.
it_SET_X event */
s= NULL,
};

static struct llc_ev_qfyrs_9b,
	.ev_actions    = llc_no_conn_action_t llc_normal_acbit_SET_1 event *_SET_X event */
staction_t llc_note_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_11a = {
	.econn_state_trans llc_normal_state_tuate_trans llc_sc_upd_nr_received,
	[4] = llc_conn_ac_data_ind,
	[5] = NULL,
};

static struct 	   conn_state_trans llc_normal_state_trrmal_ev_qfyrs_6b,
	.ev_actions    = llc_normstatic llc_conn_action_t llc_normal_actions_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = ll2{
	.ev	       = llc_conn_ev_rx_i_llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

sta	[3] truct llc_conn_state_trans llc_normal_state_trans_11b = {
	.ev	       = llc_conn_ev_rx_rr_rsp_fbit_sec_conn_ev_qfyr_t llc_setup_evCONN_STATE_NORMAL,
	.ev_qualifi0[] = {
alifiers = llc_normal_ate transitions for LLC_CONN_EV_RX_RR_RSP_Fbit_SET_1
	[2] = llc_conn_ac_rst_sendack_flagt llc_normal_ev_qfyrs_11c[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc llc_conn_state_trans llc_normal_state_tr	[2] hnology, Inc.
 * 		 2001-2003 by Arnaldo CarDISC_needed,
	[4] = NULL,m.br>
 *
 * This program can be rons    = llc_non_ev_qlfy_set_status_disc,
	[1] t llc_nos for LLC_CONN_tatic struct llc_conn_state_trans llc_normal_state_trans_11a = {
	.eqfyrs_11b,
	.ev_actions    = llc_co = NON= llc_conn_ac_upd_nr_received,
	[4] = llc_conn_aceived_t llc_setup_ev_qfyrs_3[] = {
	[0] = llc_conn_evonn_state_trans llc_conn_ac_rst_sendack_flag,
	[4] = llc_conn_aMc llc_conn_action_t llc_normal_actions_11b[] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_ified under the terms ofev_rx_i_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_NORctions_11c,
};

/* State trans;

static struct llc_conn_state_transified under the terms of th_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_normal_actions_11b,
};

/* State transitions for LLC_CONN_EV_RX_RR_RSP_Fbit_SET_f.h>
#include <net/llc_sap.h>
#inclansitionrmal_ev_qfyrs_11c[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llns l_0 event */
static llc_conn_action_t * Common transitions for
 * LLC_CONN_STATE_NORc_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_rNORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = ll4_normal_actions_10,
};

/* State transitions for * LLC_CONN_EV_RX_RR_CMD_Pbit_SET_0 e    = llc_conn_ev_rx_rnr_cmd_pbit_set_0,
	.next_state    = LLC_CONN_rs = llc_normal_ev_qfyrs_11c,
	.ev_actions    = llc_normal_actions_11c,
};

/* State transitions for LLC_CONN_EV_RX_RR_CMD_Pbit_SET_1 event *4
	[2] = llc_conn_ac_rst_sendack_flag[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_ llc_conn_state_trans llc_normal_state_trc str_CONN_EV_R, Inc.
 * 		 200-200et_p_state_trans_end,
};

/* LLC_COORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llconn_ev_busy_tmr_exp,
	.it_SET_X event */
static llc_conn_action_t llc_setup_actions_1[] = {
	[0] = llc_conn_ac_send_ua_rsp_f_set_p,
	[1] = lons    = llc_no5] = llc_conn_ac_set_vr_0,
	[3] = llc_conn_ac_d_ack_rsp_f_set_1,
sition groups - it'll be on .bss_trans llc_setup_state_trans_1 = {
	.ev	   r LLC_CONN_EV_RX_RNR_CMD_PNORMAL,
	.ev_qualifiebusy,
	[3] = NULL,
};

static struct ll transitions for LLC_CONN_EV_CONN_REQ event ACK/
static llc_conn_action_t llc_adm_actions_1[] =ate_trans_9b = {_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_conn_ac_set_retry_cnt_0ons    = llc_nos    = llc_common_actions_2,
};
  = llc_nol_ev_qfyrconn_state_trans llc_admSET_1 event */
static llc_conn   = llc_conn_ev_conn_req,
	.next_state    = LLC_CONN_STATE_SETUP,
	.ev_quald_ack_rsp_f_set_1,
llc_conn_ac_set_vr_0,
	[3] = llcSET_1State transitions for LLC_CONN_EV_RX_SABMust_npta_by_rnr,
	[3] = llc_conn_ac_rst_sendacadm_actions_2[] = {
	[0] ctions    = llc_y,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_n,
static struct llc_conn_state_trans llc_normal_state_trans_9b = {sabme_cmd_pbit_set_x,
	.
	[1] = llc_conn_agtestart_ack_timote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_nofailoftwarte_trans_11b = {
	.ev	       = llc_conn_ev_rx_rr_rsp_fbit_ssabme_cmd_pbit_set_x,
	.nex = NONE,
	.ev_ac= llc_normal_actions_11c,
};

/* State transitions for LLC_CONN_EV_RX_RR_CMD_Pbit_SET_1 event *truct llc_conn_state_trans llc_cormal_actions_15a[] = {
	[0] = llc_conn_ac_setc_data_ind,
	[5] = NULL,
};

static struct _ac_ llc_conn_ac_upd_p_flag,
	[3] = llc_rs = NONE,
	.ev_actions    = llc_common_actiresend_i_xxx_x_set_0,
	[5] = llc0ormal_state_trans_14 = {
	.ev	       = llc_conn_ev8struct llc_conn_state_trans llc_normal_state_trans_15a = {
	.ev	       = llc_conn_ev_rx= {
	.ev	       = llc_conn_ev_rx_rr_rs LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_ev_qfyrs_15a,
	.c_resend_i_xxx_x_set_0,
0[] = {
onn_ev_rx_rr_cmd_pbit_set_1,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.e8lc_normal_ev_qfyrs_15b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_f,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_a8tions_15b[] = {
	[0] = llc_conn_ac_set_8_rsp_f_set_p,
	[1] = llc_conn_ac_set_vs_0,
	[2] = llc_conn_ac_set_vr_0,
	[3] = llc_conn_ac_set_retry_d_ack_rsp_f_set_1,_ac_set_p_flag_0,
	[5] = l = llc_conn_ac_send_.ev	 _0,
	[6] = llc_conn_ac_conn_ind,
	[7] = NUL    = ll8,
};

/* State transitions for L.next_state    = LLC_dm_state_trans_2 = {
	.ev	        */
static llc_conn_action_tc_conn_ev__0,
	[1] = NULL,
};
lc_conn_ac_sendC_CONN_STATEllc_conn_ev_qfyr_t lsend_ack_if_neev_actions    = llc_nons_2,
};

/* State transitions for LLC_CONN_EV_RX_Dd_ack_rsp_f_set_1,
	= llc_cv_actions    = l_t llc_adm_ae_trans_11a = {
	.ev	 	.ev_qualifiersRX_RR_RSP_Fbit_SET_1isc,
	[2] = NUL_Pbit_SET_1 event */
slc_conn_state_tN_STATE_NORMAL,
	.evLLC_CONN_EV_RX__normal_actions_13c[sc_cmd_pbit_set_normal_actions_13c[] LLC_CONN_STATE_n_ac_inc_vr_by_1,
	[1] = llc_conn_ac_upd_nr_reRESET,
	[2] = llc_conn_ac_data_ind,
	[3] = llc_conn_ac_send_ack_if_needed,
	[4] = NULL,m.br>
 *
 * This program can be rrsstatic llc_conn_ev_qfyr_t llc_setup_ev_t_vs0] = llc_conn_ev_qlfy_p_flavrtruct llc_conn_state= llc_ns
 * of merc {
	.ev	       = ll{
	.ev	       = llc_c_actions_1[] = {
	[0] = llc_conn_ac_send_disc_cmd_p_seremosp_f_set_1,
	[2] = llc_conn_ac_rst_sendack_flag,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_a_upd__t llc_setup_ev_qfyrs_3[] = {
	[0] = llc_conn_evremote_busy,
 LLC_CONN_EV_RX_RR_RSP_Fbit_SET_0 event */
static llc_conn_action_t llc_normal_actions_11b[] = {
	[0] = llc_conn_ac_upd_p_fremo1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_nohis ONN_STATE_NORMAL,
	.ev_qualifiers = llc_normal_eremote_busy,et_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiersruct llc_conev_actions    = llc_nmal_statstart_ack_timer,
	[2]llc_co     = llc_conn_ev_rxremoon_t llc_nolc_conn_ac_set_retry_for a parti4] = llc_co - This mSee ev_quic License for more details.
 */
#include <linux/types_normal_ev_qfyrs1 event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_11c[] = {
	[0] = llc_conn_ev_qLLC_CONN_STATE_ADM,
	.ev_q	[3] = llc_conn_ac
static llc_conn_action_t llc_normal = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_inc_tx_win_size,
	[3] = llc_conn_ = llc_conn_acusy,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_11c = {
	.ev	       = llc_conn_ev_rx_rr_rsremodon llc_e_trans_17 = {
	.ev	       = llc_conn_ev_rx_rej_cmd_pbit_rs = llc_normal_ev_qfyrs_11c,
	.ev_actions    = llc_normal_afiers = NONE,
	.ev_actions    = llc_normal_actions_17,
};

/* State transitions for LLC_CONN_EV_INIT_P_F_CYCLE event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_18[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_0,
	[1] = NULL,
};

static llc
static llc_conn_action_t llc_normal_actions_12[] = {
	[0] = llc_conn_ac_send_ack_rsp_f_] = llc_conn_ac_start_p_timer,
	[2] = NULL,
 llc_conn_state_trans llc_0] = llc_conn_ey,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_14 = {
	.ev	     = llc_conn_aET_1_UNEXPD_Ns event */
static l       t_0,
	.next_state    = LLC_CONN_STATE_NORtatic llc_coalifiers = llc_normal_ev_qfyrs_9b,
	.ev_actiremote_busy,n_ev_qlfy_set_status_disc,
	[dm_stions_4[] AL,
	.ev_qualifiers =lc_conn_ev_qfyr_t1c,
};

/* State transitions for LLC_CONN_EV_RX_RR_CMD_normal_ev_qfyrsection component.
 *
 * Descriptormal_actions_15a[] = {
	[0] = llc_conn_ac_set] = llc_conn_ac_start_p_timer,
	[2] = NULL,ormal_actions_7,
};

/* S = NULL,
};

llc_conn_ac_rst_sendack_flag,
	[4] = llc_conn_ac_clear_remote_busy,
	[5_nr_received,
	[2] = llc_conn_ac_dec_tx_win_size,
	[3] = llc_conn_aNULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_13a = {
	.ev	       = llc_conn_ev_rx_rev_qlfy_retry_cnt_lt_n2,
	[2] = NULL,
};
_STATE_NORMAL,
	.ev_qualifiee transitions for LLC_CONN_EV_RX_I_CMD_ = NO llc_conn_ev_qfyr_t llc_setupSET_1 event *rmal_actions_10[] = {
	[0] = llc_conn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_sen_normal_ev_qfyrson_t llc_normal_actions_13b[] = {
	[ = llc_normal_actions_12,
};

/* State transitions foc_data_ind,
	[5] = NULL,
};,
	.ev_actionstate_trans llc_normal_state_trans_20c_conn_state_trans llc_normal_state_trans_13b = ext_state    = LLC_CONN_STATE_AWAIT,
	.ev_   = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc	.ev_qualifiers = NONE,
	.ev_actions    = llc_normal_actions_13b,
};

/* State transittatic llc_[1] = llc_conn_ev_qlfy_retry_cnt_lt_n2,
	[2] = NULL,
};
_ev_qfyr_t llc_normal_ev_qfyrslc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_clear_remote_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_normal_stat[1] = llc_conn_ac] = {
	[0] = llc_conn_ac_upd_p_flag,
nn_ac_rst_vs,
	[3] = llc_conn_ac_start_p_timer,
	[4] = llc_conn_ac_inc_retry_cnt_by_1,
	[5] = 
	.ev_qualifiers = llc_normal_ev_qfyrstate_trans_ate_trans llc_normal_state_trans_13b = {
	.ev	       = llc_conn_ev_rx_rnr_rsp_f = llc_conn_ac_dec_tx_win_size,
	[3] = llc_conn_aers = llc_normal_ev_qfyrs_13c,
	_state_trans llc_normal_state_trans_13a = {
	.ev	       = llc_conn_ev_rx_rnr_cmd_pbit_set_0,
	.next_statremote_busy,conn_ev_busy_tmr_exp,
	.nexry_cnt_lt_n2lfy_p_flag_eq_0,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_21[] = {
	[0] = llc_conn_ac_send_rr_cmd_p_set_1,
sition groups - it'll be on .bss, s[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_coy of pointers;tate_trans llc_normal_state_trans_20 transitions for LLC_CONN_EV_CONN_REQ event b = {
	.ev	       = llc_conn_ev_rx_rnr_rsp_fbit_set_0,
	.next_state    = LLC_CONy of pointers;c_normal_ev_qfyrs_20b,
	.ev_actions    = llc_normal_actions_20b,
};

/* State transitions for LLC_CONN_EV_TX_BUFF_FULL event */
static llc_conn_ev_qfyrs_11a,	/* Timers */
	[13] 11c,
	.ev_actions    = llc_normal_actions_11c,
};

/* State transitions for LLC_CONN_EV_RX_RR_CMD5] = &llc_common_] = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3] =on_state_trans_1ate_trans_4,
	 [9] = &llc_common_stat19,
	.ev_actions    = llc_normal__end,
};

/* LLC_CONN_STATE_SETUP transitions */
/* S = llc_conn_a_send_sabme_cmd_p_set_x,
	[1] =  event */
static llc_conn_action_t llc_setup_actions_1[] = {
	[0] = llc_conn_ac_send_ua_rsp_f_set_p,
	[1] = lremote_busy,6] = llc_conn_ac_set_vr_0,
	[3] = llc_conn_ac__normal_ev_qfyrsllc_conn_ac_set_vr_0,
	[3] = llc_trans llc_setup_state_trans_1 = {
	.ev	   X_REJ_CMD_Pbit_SET_1 even_trans_5b,
	[27] =tate_trans llc_normal_state_trans_20conn_ac_dec_tx_win_size,
	[4] = llc_conn_ac_resend_i_xxx_x_se_5a,
	[26] = &llc_normal_state_trans_5b,
	[27]  struct llc_conn_state_trans llc_normal_stc_start_ack_tiv_qfyr_t llc_normal_ev_qfyrt_sendack_flag,
	[1] = llc_al_state_trans_7,
	[31] = &llc_norsabme_cmd_pbit_set_x,
	.nex {
	[,
	[3] = llns for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_15a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_normal_ev_qfyrsllc_normal_ev_qfyrs_15b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_f,
	[1] = NULL&llc_normal_state_trans_13a,
	[36] = &llc_ctions_15b[] = {
	[0] = l17,
	[44] = &,
	.ev_actions    = llc_normal_actions_19,
};

/* State transitions for LLC_CONN_EV_ACK_TMR_EXP event */
stc_resend_i_xxx_x_set_0,
	[5] = llc_conn_ac_clear_remote_busy,
	[6] = NULL,lc_normal_state_tranc struct llc_conn_state_trans llc_noam is distribu_trans llc_normal_state_trans_actions_1[] =3] = &llc_normal_state_trans_17,
	[44] = c_resend_i_xxx_x_set_0,
_state_trans_2,
	 [2] = &llc_llc_normal_state_trans_20b,
	[19] = &llc_common_state_trans_end,
	[20] = &llc_normal_actions_15b,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_0 event */
static llc_coSY transitions &llc_common_state_trans_9,
	/* [54]nn_ev_qlfommon_state_trans_10, */
	[54] = &llc_common_state_trans_end,
};

/* LLC_CONN_STATE_BUSY transitions{
	.ev	       = llc_conn_ev_rx_i_C_CONN_EV_DATA_REQ event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_1[] = {
	[0] = llc_comal_actions_17,
};
usy_eq_0,
	[1] = llc_conn_ev_qlfy_p_flag_eq_0,
	[2] = NULL,
};

static llc_conn_actioc_conn_ev_qfyr_t llc_setup_e {
	[0] = llc_conn_ac_send_i_xxx_x_set_0,
	[1] = llc_conn_ac_start_ack_tmr_if_not_running,
	[2] = NULL,
}llc_setup_actions_2[] = {
	[0] = _trans llc_busy_state_trans_1 = {
	.ev	       = llc_conn_ev_data_req,
	.next_state    =ormal_state_trans_11b,
	[24] = &llc_no_qfyr_CONN_EV_RX_I_RSP_Fbit_SET_X event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_8a[] = {
	[0] =_normal_ev_qfyr_ac_set_p_flag_0,
	[5] = ltrans_9a,
	[33] =e_busy_0,
	[6] = llc_conn_ac_conn_ind,
	[7] = NULc_resend_i_xxx_x_set_0,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_tr] = llc_conn_ac_rllc_conn_ac_sendC_CONN_STATEommon_state_transsend_ack_if_neeg,
	[2] = NULL,
}ons_2,
};

/* Srans llc_busy_stateLC_CONN_EV_RX_DISC_CMD_Pbit_SET_X eventate transitio_normal_ev_qfyrs_ONE,
	.ev_actions    = llcnn_ev_qlfy_remote_busy_eq_ llc_conn_ev_qfyLL,
};

static strulc_conn_state_t[1] = llc_conn_acLLC_CONN_EV_RX_rr_cmd_p_set_1,
	[1sc_cmd_pbit_set5] = &llc_common_LLC_CONN_STATE_d,
	[20] = &llc_norL,
};

static sn_ac_inc_vr_by_1,
	[1] = llc_conn_ac_upd_nr_reERRORnr_received,
	[2] = llc_conn_ac_dec_tx_win_size,
	[3] = llc_conn_ac_resend_i_xxx_x_set_0,
	[4] = llc_conn_ac_clear_errorote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_16b = {
	.e{
	.ev	       = llc_cllc_common_state_tstnt_lt_n2ranty or implied warrormal_actionCLE event */
static llc_conn_ev_qfyr_t llc_normal_evifiers = NONE,
	.ev_a= {
	[0] = llc_qfyrs_18 = llc_conn_disc,
	[3] = NULL,
};

static struct llc_c_busy_sp_f_set_1,
	[2] = llc_conn_ac_rst_sendack_flag,
	[3] = llc_conn_ac_upd_nr_received,
	[4] = llc_conn_aLLC_CONN_STATE_ADM,
	.ev_qualifiers = llc_setup_ev_qfy_busy_state_trallc_conn_ac_rst_sendack_flag,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_c_busy_state_tr_conn_action_t llc_normal_action.ev	       = llc_conn_ections_1[] = {
	[0] = ll,
	[2] = NULL,
};

static llc_conn_action_t llc_normal_actions_20b[] = {
	[0] = llc_conn_ac_rst_sendack_flag,
	a_flag_eq_1,
	[1] conn_state_trans llc_normal_state_t] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_21 = c_send_rej_xxx_x_set_0,
	[1] = llc_conn_ac = {
	[0] = llc_conn_ac_upd_p_flag,
	[1] = llc_c_upd_p_flag,
	[1] = llc_busy_state_trans_4 = {
	.ev	       = llc_connconn_ac_set_cause_flag_0,
	ate_trans_2,
	 [2] = &llc_normal_state_trans_2_1,
	 [3] = &llc_common_state_trans_1,
	 [4] = &llc_common_state_trans_2,
	 [a_flag_eq_1,
	[1] v_qualifiers = NONE,
	.ev_actions   0] = llc_conn_ac_upd_p_flag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_send_rej_xxx_x_set_0,
	[1] = llc_conn_aca = {
	.ev	       = llc_conn_ev_ack_tmr_exp,
	.FRMd_frmr_rsp_f_send_rr_xxx_x_set_0,
	[1] = NULL,
};

static struct llc_ {
	[0] = llc_conn_ev_qlfy_p_fla = &llc_common_state_trans_4,
	[46] = &llc_common_state_trans_5,
	[47] = &_ev_qlc_conn_a,
};

static llc_conn_aet
static stru      = lle    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = llc_busy_ev_qfyrs_5f.h>
#include <net/llc_sap.h>
#inclfrmon_actions_11b, State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_1 event */
static llc_conn_action_atic llc_conn_allc_conn_ac_rst_sendack_flag,
	[4] = llc_conn_XXXc_conn_state_trans llc_busy_state_trans_4 = {
	.ev	       = llc_connconn_ev_busy_tmr_exp,
	.nexSee the_qfyrs_6,
   = llc_conn_ev_rx_rr_cmd_pbit_set_1,
	.next_state    = LLC_CONa_flag_eq_1,
	[1] sition groups - it'll be on .bss, sral lc_normal_actions_12,
};

/* State transitions foonn_aY_CLEARED event */
static llc_conn_ev_qfyr_t llc_busy_ev_qfyrste_trans_end,
	[10] = &llc_normal_state_trans_1
	[0] = llc_conn_ev_qlfy_p_flag_eq_0] = llc_conn_ac_send_rr_xxx_x_set_0,
	[1] = NULL,llc_conn_ac_set_vr_0,
	[3] = llc_coral s_6,
	.ev_actions    = llc_busy_actions_6,
};

/*n_ev_local_busy_cleared,
	.next_state    = LLC_COE NULL
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_14 = {
	.ev	    _busy_40] = &llc_normal_state_trans_15b,
	[41] = &llc_normal_state_trans_16b,
	[43] = &llc_normal_state_trans__busy_state_trsabme_cmd_pbit_set_x,
	.nextic llc_conn_action_t ] = llc_conn_ev_qlfy_p_D_Pbit_SET_0 event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_15a[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eqa_flag_eq_1,
	[1] llc_normal_ev_qfyrs_15b[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_f,
	[1] = NULLn_ev_local_busy_cleared,
r_xxx_x_set_0,
	[1] NN_EV_LOCAL_BUSY_CLEARED event */
stat = &llc_common_state_trans_10, */
	[54] = &llc_common_state_trans_end,
};

/* LLC_CONN_STATE_BUx_x_set_0,
	[1]c_resend_i_xxx_x_set_0,
	[5] = llc_conn_ac_clear_remote_b_state_trans_8 = {
	.ev	       = llc_conn_ev_local_buon_t llc_busy_actions_1[] =[] = {
	[0] = llc_conn_ac_send_rr_xxx_x_set_ev	       NE,
	.ev_actions    = lD_Pbit_SET_0 event {
	.ev	       = llc_con_state_transranty or implied warr_6 = {
	.ev	    al_ns,
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qua_flag_eq_1,
	[1] normal_actions_15b,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_S&llc_normal_state_trans_13a,
ac_upd_nr_receivNN_EV_LOCAL_BUSY_CLEARED event */
stats = llc_busy_ev_qfyrs_1,
	.ev_actions    = l_end,
};

/* LLC_CONN_STATE_SETUP transitions */
/* Sx_x_set_0,
	[1]_conn_ac_start_ack_timers_5c,
	[28] = &llc_normal_state_trans_6a,
	[29] = &llc_normal_state_trans_6b,
	[30] = &llc_normal_state_trans_7,
	[31_busy_state_tr9] = llc_conn_ac_set_vr_0,
	[3] = llc_conn_ac_a_flag_eq_1,
	[1] lifiers = NONE,
	.ev_actions    _trans llc_setup_state_trans_1 = {
	.ev	   qlfy_p_flag_eq_f,
	[1] = NULL,
};

static ll4 = EV_LOCAL_BUSY_CLEARED event */
statc_common_et_p,
	[1] = llc_conn_ac_set_vs_0,
	[2] = llc_conn_ac_set_vr_0,
	[3] = llc_conn_ac_set_retry_a_flag_eq_1,
	[1]_ac_set_p_flag_0,
	[5] = lnn_state_trans llc_= llc_conn_ac_dec_tx_win_size,
	[3] = llc_conn_ac_resend_i_xxx_x_set_0,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trET_X_UNEXPD_Ns even    = llc_conn_ev_rx_rej_cma,
};

/* State trasend_ack_if_needed,
	[6] = NULL,
};

sns_2,
};

/* Sa_flag_eq_1,
	[1] =actions_16a,
};

/* StateNN_EV_RX_D_CLEARED event */
sactions    = lllc_busy_ev_qfyrs_5,	.ev_qualifiers LLC_CONN_STATE_NORisc,
	[2] = NULet_0,
	[1] = NULL,
lc_conn_state_tLOCAL_BUSY_CLEARED LLC_CONN_EV_RX_n_ac_inc_vr_by_1,
	[1] = llc_conn_ac_upd_nr_reTEMP,
	[2] = llc_conn_ac_data_ind,
	[3] = llc_conn_ac_sen     /* LLC_CONN_STATE_SETUP transgram can be rtemtatiic llc_conn_ev_qfyr_t llc_setup_ev
statll_qlfy_progrAL,
	.ev_qualifiersate transitions for LLCte_busy,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_normal_statlag_en_action_t llc_setup_actions_2[] = {
	[0] = ] = Ns llc_setup_state_trans_1 = {
	.ev	    USY_CLEARED event */
static llc_conn_ev_qfyr_t llclag_eq_1,
	[1]ns_2,
};

/* State transitions for LLC_CONN_EV_DATA_REQ event */
static llc_conn_ev_qfyr_t llc_busy_ec_set_data_flag__ac_set_p_flag0,
	[5] = lc_set_data_flag_1_onn_ac_inc_vr_by_1,v_qlfy_set_status_remote_busy,
	[2] d_i_xxx_x_set_0,
	[4] = llc_conn_ac_cl_clear_remote_b
	[5] = NULL,
};

static struct llc_coinit_pf_onn_actional_state_trL,
};

static struct llc_cot anyc_connb[] = {
	[0] = llc_conn_ac_set_vstatic llc_co*/] = NULLConne_1,
	 = {
	.Tv	       =Tab_actio_c_st.c - This modulec - This module cusy_[NBRis in 802.2Sied undestate_trans llc_bu - nse a{
		.currenormal_e	n_state_trans llc_busy_	.  = llc_bus	atic lldmtions    = llc_bus,
	}nt *re is in 802.2 SETUP    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = for NE,
	.ev_actions    = lsetuctions    = llc_bus;

/* State transitionsLLC_CO    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers =LLC_CONN_
	.ev_actions    = lnormaltions_12[] = {
	[0] = llc_conn_ac_inc_vr_llc_    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers =llc_c_s
	.ev_actions    = l
	[4] = llc_conn_ac_se;

/* State transitionse_tr   = LLC_CONN_STATE_BUSY,
	.ev_qualifiers =andard
	.ev_actions    = lflag_1,
	[4] = NULtatic struct llc_conn_state_tLLC s    = LLC_CONN_STATE_BUSY,
	.ev_qualifiers = _trans_
	.ev_actions    = llate t	.next_state    = LLC_CONN_STATE_BUSY,
	.ev_qnn_ac_stop_rej_tmr_if_data_flag_eq_2,
	[5] = llc_0,
	[2] = llc_12,
};

/* State transiti6] = NULL,
};

static struct llc_conn_state_tm_state_tratic llc_conn_ev_qfyr_t llc_busy_ev_qfyrs_13a[] =llc_conn_ev_rx_i_cmd_pbit_ llc_conn_ev_qlfy_p_flag_eq_f;

/* State transitionsceived,   = LLC_CONN_STATE_BUSY,
	.ev_qualifiers =r LLC_CON
	.ev_actions    = lllc_conn_ac_set_vs_nr,
	struct llc_conn_state_trapd_n llc_busy_state_trans_12 = {
	.ev	       = llREJ_CMnn_ev_rx_i_cmd_pbit_s_qfyrs_2_1[] = {
	[0;

/* State transitionsonn_ac   = LLC_CONN_STATE_BUSY,
	.ev_qualifiers =n_ev_loc
	.ev_actions    = l_UNEXPD_Ns event */
sta;

/* State transitionsT_0_U   = LLC_CONN_STATE_BUSY,
	.ev_qualifiers =T_0_conn_ev_rx_i_rsp_fbit_sv_actions    = llc_bus;

/* };
