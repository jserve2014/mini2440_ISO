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
	.ev	 onent.=ins shis mev_localcontaicleared,
	.nextition nt.
 *LLC_CONN_STATE_AWAIT_REJionsev_qualifiers
 *odultate f evenodulfyrs_3ard,
 *actard,ns the_ac.c" and "llc_c_odul mod};

/* S *
 *f conirocomCforere is in EV_RX_I_RSP_Fbit_SET_1_UNEXPD_Ns event */rocomll*
 * Descr Proco_.ins s(c) 1997 by Procom 4[] *
 * [0ied * Descprogr_opt_send_rnr_xxx_x_set_0,
	[1 the terms of thupd_nr_receiv *
 *[2se as publishedutedtvstwar3se as published stop_p_timerrogr4my Ardistributed et_data_flag_1rogr5se as published t fun_remotecontarogr6se as published re GNU ieral Ptionc Lice7 theNULLechnol>
 *
 *_c_stn be rDescrtion of conins 
 * and "llcd undofof cn4ed undcompnent.
 ProcomDescriptrx_i_rsp_fe Mewarr1_unexy th of s and 
 *
 * se a by Arnal802.2 BUSYodules. orrnal" detaNONEtion set/llc_sCopyright edistributed or modie for ogy, Incet/l 		 2001-2003utedArnaldo CarvaCMD_Pt/lllo <0cme@conectiva.com.br> Procomterms of tam can be reor implied worclud5af theunderse as published etion theGenGNU General Publn Foundation.
 * Thishe Free Sofrogre Founda*
 *itionTE_REJ,
 * LLty or implied warranty.h>

f mercion_tstatesf* LLCre detail* COM/
#include <linux/types.h>_ac_send_dinet/ deti5at_x,
	[1] = llc_conn_asapt_x,
	[1] =cmd_pconn_ac_0vt_x,
	[1] = llc_conn_ac_act_x,
	[1] = llc_coLLC st details.h>

#define E NUicensologyCOMMON s inECTION 802.2#include <ns
5a* Common transitions for
 * LLC_CONN_STATE_NORMAlho d LLC_CONn 802.2 llc_t.h>
re is in 802.2 REJext_state    = LLC_CLLC sext_state bin 802.2 LLC stllc_<netSTATE_D_CONN,
	.ev_qualiCONNes.h>
lc_c/logy,.h>
#include <ns[] = re is in EV_DISC_REQiva.com.b*
 * llc - This m
 *
 *_.c - Thimmocommon_as_1[]ction [0c_co_t llc_comm* GNU disc_top_oublicxLicen_send_sabme_cmd_btart_ack_timer,
	[2] = llc_conn_ac_slc_conn_ac_seersLice3_send_sabme_cmd_p_t_retry_cnic Lice4c_set_retry_cnt_0,
	cause
 * of Lice5c_cocens,
hnol
 * llc_c_st.c - This ms.h>
_bN CO] =  LLC_CONN_EV_RESxt_state    = LLC_CNORMALext_state   a_STATE_BUSY,
 * LLC_CONN_STATE_REJ,
 * LLC_CONN_STATE_AWAIT,
 * LLC_6NN_STATE_AWAIT_BUSY and
 *_CONN_STA llc_ = llcState transitions for LLC_CONN_EV_RESET_REQ event */
static llc_conn_action_t llc_common_actions_2[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_tim6tart_ackut anyLice2_send_sabme_cmd_pwitho.h>
ut anrs,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] set_cause_flag_1,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_com6rans llc_common_state_trans_1 = {
	.ev	       = llc_conn_ev_daY,
 * LLC_CONN_STATE_REJ,
 * LLC_CONN_STATE_AWAIT,
 * LLC_7NN_STATE_AWAIT_BUSY and
 * LLC_CONN_STATE_AWAIT_REJ states
 */
/* Stateinc_vr_bylude <C_CONN_EV_DISC_REon_t inor LL
static llc_conn_awithout any c_c_on_t

#dempl the transitions for LLhantabilifor LLfiterms of tr purposeet/llc_smmon_actions_2General Pu event */
sttnesV_RESEa particula8nn_action_t llc_SeeransiCONN_STATE_AWAIT9
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_tim7,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc llc_conn_ac_set_retry_cnt_0,
	[4] conn_ac_set_cause_flag_1,
	[5] = NULL,
};

static struct llc_conn_state7ontaions_2[] = {c_commconta_1ction compoactionend_sabme_cev_diY,
 * LLC_CONN_STATE_REJ,
 * LLC_CONN_STATE_AWAIT,
 * LLC_8 in 802.2 LLC st    = llc_common_actions_1,
};

/* State transitions forin "lltatiONEionsev{
	[0] =ions_4,
};

] = {
	[0] = 3truct /* transitions for LLEV_RESET_REQ event RXs of tSET_X event */
stonn_ac_start_ac4[] onn_aons_1[] = {
	[0] = llc_conn_ac_send_disc_cmd_p_set_x,
	[1] = llc_conn_ac8slc_conn_ac_set_vr_0,
	[3] = llc_conn_conn_ac_set_ret_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_1,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_com8contaNONE,
	.ev_actions    = llc_common_actions_4t_req,
	.nextN LLC_CONN_EV_RESET_REQ event RX_FRMRalho de Melo <Xiva.com._cmd_p_set_x,
	[1] = llc_conn_ac_start_ac1_timer,
ns for LLC_CONN_EV_RES* GNU sabme	[1] = llc_conn_ac_start_ack_time,
	[7] = NULL,
};

static struct llc__ac_send_ua_rs_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flagrst_intion

statt_cause_flag_1,
	[5] = NULL Lice6= {
	.ev	    resett_vr_0,
	[3] = llc_conn_ac_send_ua_rs_ac_send_ua_rsESET,
	.ev_qualifiers = NO5lc_common_actions_4,
};

/* Starx_frmrde <nee Mellc_conns andactionns    =e is in 802.v_actions    = 2lc_common_actions_6,
};

/* Stats
	[4q_CONN_EV_Y,
 * LLC_CONN_STATE_REJ,
 * LLC_CONN_STATE_AWAIT,
 * LLC_91] = llc_conn_ac_start_ac2on_t llc_common_actions_6[] = ET_REQ even{
	[0] = llc_conn_ac_disc_ind,
	[1] = llc_conn_ac_stop_all_timers,
	[2] = llc_conn_disc,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_6 = {
	.ev	       = llc_conn_ev_rx_dm_rsp_fbit_set_x,
	.9_retry_cnt_0,
	vrrx_dm_ac_set_retry_cnt_0,nd_ua_rSET,
	.ey_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_1,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_com9* Common transitions for
 * LLC_CONN_STATE_NORRR llc_commE_RESET,
	.ev_qualifiers = NOection compoctions_6,
};

/* State 10/
static llc_conn_action_t 	[2] = llc_conn_ac_stFoundation.
 * Thiss of tC_CONN_EV_DISC_RE State transitio
static llc_conn__ac_send_ua_rsp_f_set_p,ns for LLC_CONN_Enn_ac_stop_all_timersSET_Xons_1[] = {
	[0] = llc_conn_ac_send_disc_cmd_p_set_x,
	[1] = llc_conn_ac10,
	[7] = NULL,
};

static struct llrc_commons    = 4tart_ack_timer,
	[2] = llc_conn_ac_x,
	[1] =s for LLC_CONN_EV_RX_ZZZ_CMD_Pbit_SET_X_INE_ADMmd_p_se or in "10E_RESET,
	.ev_qualifiers = NONE,
	.ev_actions  REJ transitionor LLC_COind,
	[1] = llc_conn_ac_stop_all_timers,
	[2] = llc_con_cmd_p_set_x,
	[1] = llc_coretry_cnt_0,
	[4] = llc_conn_ac_seic struct llc_conn_state_trans llc_commE_RESET,
	.ev_qualifiers = NO7btart_ack_timer,
	[2] = llc_conn_aciSET_X_INVAL_Nr _inval_nconnnt */
static llc_conn_action_t lERRORcommon_actions_d_sabme_cmd_p_set_xt_vr_0,
	[3] = llc_conn_ac_send_ua_rejions for LLC_CONN_EV_RESET_REQ event RX_ZZZor LLC_CONN_EV__INVAL_Nrc llc_conn_action_t llc_common_actions_2[] = {
	[0] = 8aVAL_Nr event */
static llc_conn_action_t llc_coRR= llc_conn_ac_start_ac5[2] = llc_conn_ac_stop_other_timers,
	[3]_X_INDM_R11n_disc,
	[3] = NULL,
};

static struct llc_conn_state_trans lic struct llc_conn_state_trannn_ac_stop_all_timers
stat= LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_com1,
	[1] = llc_conn_ac_start_ac7b_timer,NE,
	.ev_actions    = llc_comm,
	[1] = llc_conn_ac_start_ac6lc_conn_ac_stop_other_timers,
	[3] = llc_conn_VAL_L,
 *ent */
stat1llc_conn_ac_send_sabme_cmd_p_set1] = llc_conn_ac_n_ac_send_frmr_imers,
	[2] = llc_conn_disc,
	[3] = NULL,
};

static struct l NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8common_state_trans_8llc_conn_ev_rx_zzz_rsp_fbit_set_x_invitions for LLCstate    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers [0] = llc_conon_t llc_common_actionsllc_common_acti	[0] = llc_co_actions_ac_stop_other_timers,
	[3] = llc_conn_BAD_PDUc llc_conn_action_t llc_common_actions_2[] = {
	 llc_conn_ac_send_frmr_rsp_start_ack_timer,
	[2]EJ_ac_stop_all_timers,
	[2] = llc_conn_disc,
	[3] = NULL,
};

static struct lc,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8common_state_trans_8a = {
	.ev	       =8ctart_ack_timer,
	[2] = llc_conn_acbad_pdu= LLC_CONN_STATE_ERROR,
	.ev_actio_ERROR,
	.ev_qualifiers = NONE,
	[1] = llc_conn_ac_start_acon_actions_8c,
};

/* State transitions for LLC_CONN_EV_RX_UA_RSP_Fbit_SET_X event */
static llc_conn_acions_8c[] = {
	[0] = llc_conn_ac_send_frmr_rsp_start_ack_timer,imers,
	[2] = llc_conn_disc,
	[3] = NULL,
};

static struct ldnn_ac_stop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_9 = {
	.ev	       = llc_conn_ev_rx_ua_rsp_fbit_set_x,d	.next_state    = LLC_CONN_STATE_ERROR,r_timers,
	[iers = NONE,
	.ev_actions    = llc_common_actions_9,
};

/* State transitions for LLC_CONN_EV_RX_XXX_RSP_Fbit_SET_1dtion_t llc_common_actions_9[] = {
	[0] = llc_colc_conn_ac_set__disc_ind,
	[1] = llc_conn_ac_stop_all_timers,
	[2] = llc_co2/
static llc_conn_action_t _actions_8c,
};

/* State transitions for LLC_SABMEion_t llc_commoc llc_conn_action_t llc_common_actionsnn_ac_stop_all_timers{
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_tim12= NONE,
	.ev_actions    = llc_commRROR,
	.ev_qualif	.ev0,
	[4] = NULL,
};

static struct llc_coc_conn_Iion_t llc_common_ic llctiva.com.bn_action_t llc_common_action12tions_8c[] = {
	[0] = llc_conn_ac_send_frmr_rsp_
	[_qv.c" 1 Lict_SET_X event */
static llc_conn_acti0truct#endifions_8c,
};

_cmd_p_set_x,
	[1] = llc_co_TMR_EXPy_cnt_0,
	[4] = llc_conn_aon_afyrctions_2[] = {lc_comms_11alc_conn_ac_send_sabme_con_alfy	[4] = llc_gte_nac_s_ac_stic struct llc_con_t llc_common_actions_2[] = {
	[0] = ltions    = llc_common_actnn_anext_state    = LLC_CONN_STATE_ERROR llc_conn_ac_stop_all_timers,
	[2] = llc_conn_disc,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_common_st_1,
	[5] = ction_t llc_common_actions_9[] = {
	[0] = llc_coNATE_ERROR,
	.ev_qualifiers = llc_common_ev_qfyrs_10,
	.vr_0,
	[3] = llc_conop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_catart_ack_tim= llte transitions for LLC_CONN_EV_ACK_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_common_ev_qBUSY
#include <net/llc_sap.h>
#iude ns_8c,
}LLC_CONN_EV_RX_I_RSP_Fbit_SET_X_INVAL_Ns event */
static llc_conn_action_t llc_common_actions_8b[] = {
	[0] = llc_coT* Common transitions for
 * LLC_CONN_STATE_NORac_sac_stop_all_timers,
	[2] = llc_conn_disc,
	[3] = NULL,
};

static struct 4_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8ommon_actionnn_acmte_n2ns_8c = {
	.ev	       = llc_conn_ev_rx_bad_pdu,
	.next_state    = LLC_CONN4,
	.ev_qualifiers = NONE,
	.ev_actionOR,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common_actions_9,
};

/* State transitions for LLC_CONN_EV_RX_XXX_RSP_Fbit_SET_ = lns_8c[] = {
	[0] = llc_conn_ac_send_frmr_rsp_] = llc_conn_disr LLC_CONN_EV_ACK_TMR_EXP event */
static llc_conn_ec_qualifiop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_contic llc_conn_action_t llc_common_actions_11b[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_ev_rx_yrs_11b[] = {
	[0] = llc_conn_ev_qlftic llc_conn_eb,NONE,
	.ev_actions    = l1ommon_state_trans_9 = {
	.ev	    ejer_t_exp= LLC_CONN_STATE_ERROR,
	.ev_qualifRESETlc_comisc_ind,
	[1] = llc_conn_ac_stop_all_timers,
	[2] =e_trans llc_common_state_trans_11a = {
	.ev	       = llc_conn_ev_p_tmr_ex5conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3]_t llc_common_ev_qfyrs_11a[] = {
	[0] = llc_conn_ev_qlfy_retry_connte transitions for LLC_CONN_EV_ACK_TMR_EXP event */
static llc_conn_ev_qfyr_t llc_common_ev_q5;

static struct llc_conn_state_trans llc_commcontan2,
	[1] = NULL,
};

static llc_conn_action_t llc_common_actions_11b[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set5* Common transitions for
 * LLC_CONN_STATE_PN_STATE_Y,
 * LLC_CONN_STATE_REJ,
 llcv.C_CONN_STATE_AWAIT,_action" 1[1] = llc_conn_ac_start_>
 *
11a,
};

/* Slt_n2J states
ons_1[] = {
	[0] conn_disc,
	[3] = NULL,
};

static struct [1] = llc_conn_ac_start_ac	[2] = llc_ac_s LLC_CONN_EV_RESET_REQ even_ev_qte transitioC_CONN_EV_DISC_REin "mmon_ev_qfabme_cmd_trans_8c = {
	.ev	       = llc_conn_ev_rx_bad_pdu,
	.next_state    = LLC_CONNc_send_sabme_cmd	.ev_actions    prs,
	[3]et_retry_cnt_0,
	[4] = llc_conn_ac_set_cause_flag_1,
	[5] = NULL,_qualifiers = llc_common_e
	[4] = llc_conn_ac_set_cause_flag_1,
	[5] =et_x,
	.ntionArrayc_copointers;et_ude  to eachitions for

LLC_CONN_STtails.
 */
#include <linux/t*ypes.h>
#include <net/llc_ for
 NN_STATE _AWAIT& */
#immoclude <linux/_1,		/* Reques LLC_	 = llc_c_set_retry_cnt_0,
	[4moc_co  = llc= {
	ions    = llc_common_actmmon_ n_acti_ac_conn_ind,
	[7] = NULetate_tc strube on .bss, so will be zeroedetransL<net onta= {
	.eSET_Xbe on .bss, so will be zeroestructonn_abe on .bss, so will be zeroelude ral Pur event */
static llc_conn_actioransir event */
static llc_conn_atransInit_ERROPF cycle= {
	.evr_0,_ac_conn_ind,
	[7] = NULt1at_0,
	Trans= {
	[,
};

statactions_3[] = {
	[0] = ll	[1,
};

/* rs,
	trans_67tions f_1 ev	[1 llc_conn_struct llc_conn_state[4] = dmactions _send_sabme_cmd_p_set_x,
mmon_mers,
	[3] t */
static llc_conn_act[1= llc_conn_ev_rst_rNONE,
	.ev_act4
	.nex_0ons f fram= llc_[1_cmd_p_set_x,
	[1] = llc_cadms_11eroe	_dm_hnology, lc_common_ev_qfyrs_10,
	llc_cllc_conn_a = llc_adm_srx_dm_0,
	[3llc_co State trate_trans llc_common_star LLCns_6 = {
	x_dm_rsp_fbit_set_x,
	.n,r LLC	[2] = NULx = {itions for LLC_CONN_r LLCt'll be on .bss, so will be zeroe9r LLCions for LLC_CONN_EV_RX_ZZZ_RSP_Fbions	[2mers,
	[3] =  = llc_conns for LLC_v_qf	[2ONN_STATE_ADM,
	.ev_qualifiers = = llc	[2_adm_actions_3,
};

/* State trannFbit_,2top_other_timers,
	[XXXion_t llc_.c - Th2cnt_0,
	[4] = llc_conn_ac_set_caunn_ac_s2 tran
	[4] = lconn_ac_send_sabme_c[] =	[3set_
};

ste_cmd,
};
ac_start_ackc_conn_3ction_t ic struct llc_conn_state_mer,_tra'll be on .bss, so will be zeroedn_ac	[3ions for LLC_CONN_EV_RX_ZZZ_RSP_Fb_STATE3_set_1,
	.next_state    = LLC_CONN5 in "s in 802.2onn_ind,
	[7] = NULtimer,_cmd_p_setonn_ind,
	[7] = NUL4 in "on_t llc_common_actions_6[] =e tranllc_conn_a/
ston_ev_qfyrs_11a Stat3* State tranrt_ack
	[0] = llc7V_RX_4ns_6 = {
	.ev	       = ll NUL7r in 4	[2] = NULL,
};

static structate t4ommon_state_trans_8te tc_comm- This4= {
	.ev	 _x,
	.next_state   8ommon4_X_INVAL_Nr event */
static litio/* [,
	[5 llc_conn_et_p,
	[1] = ll10,ns_4,
end,busyt any0,
	_6 = {
&usy *ions_Common -*/
	[3] = e_tran>
#include <net/= llc_anditions for
 *_state_trans_33,*/mon transitions for
 * LLC_CONN_STATE_DATA Arnalatic_t llc_common_actions_c_adm_state_tic llc_conrejec_conn_};

/* 0
	[0] = llc_conn_ac_disc_ind,
	tiontionus_refuse_conn_ac_set_vr_0,
/* just_adm_member,_set_v .bss zeroes i LLC_CONN_STATE_REJ,
 * LLC_CONN_STATE_Anclude 1,
	[5] =_0[1] = {
	[0] = llc_conn_ac_send_disc_cmd_p_set_x,
	nclude ommon_state_trafyrs_11b,
	.ev_actions    = llcon_t reqRROR,net/llc_c_ac.h>
#include <net/m_state_tc_st.h>

#define NONet_rc"etupLC_CONN_EV_RESET_RELL

/* COMMON CONNECTION STATEtic llc_conn_actio* Common transitions for
 * LLC_CONN_STATE_LOCALs for DETECTEDY,
 * LLC_CONN_STATE_REJ,
 * LLC_CONN_STATE_Aimerlc_conn_act= llc_conn_ac_set_retry_cnt_0,
	[4]TE_AWAIT_REJ states
 */
/* Statemmon_actions_2c_conC_CONstat[] = {
	[0] = llc_conn_ac_send_disc_cmd_p_set_x,
	0,
	[43[] = {
	[0] fyrs_11b,
	.ev_actions    = llc <net/lva.cdeteclc_setsetup_state_trans_1 = {
	.ev	       = conn_ac_set_cause_flag_1,
	[5] = NULL,
};

static struct ll,
	[4] = llc_cn_state_trans_2 = {
	.ev	       = llc_conn_ev_rst_req,
	.nextisc_req,
	.next_state    = LLC_CONN_STATE_D_CONN,
	.ev_q[4] = llc_connLC_CONN_EV_RESET_REQ event P};

static struct llc_conn_state_trans llc_common_s_actions_11a0,
	
 * uss forry_cnt_0,s for LLC_CONN_EV_ACK_TMR_EXP event md_p_set = llc_conn_ac_disc_ind,
	[1onn_disc,
	[3] = NULL,
};

static struct llc_conn_state_trans llc_cllc_conn_ev_rx_sabme_cm_actions    = llc_comby tp
	[1]onn_ac_set_cause__ev_rx_dm_
static struct llc_conn_state_trans ll_4,
};

/* Statescn_actions_7a[static llc_conn_action_t lDis inev_qualisd[] = eqc struop_other_timers,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static sTATE_ADM,
	.ev_qualifiers = _sab2] = etup1a,
	.ev_aac_send_= llc_conn_ac_ = NULL,
};

static llc_conn_action_t llc_common_actions_11b[] = {
	[0] = llc_conn_ac_send_sabme_cmd_pommon_ev_qonn_state_transtrans_1,		/*s_4,
};

/* StaORMAL,onn_ac_set_p_fVAL_Nr event */
static llc_conn_action_t llc_common_actions_7a[nn_ac_set_retry_cnt_0,
	[4] = llc_con,
	.ev_actions    [4] = llc_conn	       = llc_conn_ev_rx_dmd_ua_r    = LLC_CONN_STATE_RESET,
	.ev_qualifiers = llc_common_ev_qfyrs_11d,
	.ev_actions  STATE_NORMAL,
	.ev_qualifiers = llc_setup_ev_qfyrs_2,
	.ev_actions    = 	.ev_actblc_common_state_trans_11a lc_common_action

static] = rans llc_common_state_trans_6 = {
	.ev	 = llc_setup_ev_qfyrs_3,
	.ev_actions    = llc_setup_actions_3,
};

/* _11c[] = {
	[0] = llc_conn_ev_qlfy_retry_cnt_gtonn_state_trans iers = llc_common_ev_qonn_state_trans{
	[0] = llc_conn_discin "STATE_AWAIT_BUSY and
 *v_actions    = _cnt_0,
	[4] = NUllc_conn_ac__RESET,
	.ev_qualifiers = NON1llc_conn_ev_rx_zzz_rsStatk_ti_other_timers,
	[3] = llc_conn_ae_transitions[] = { llc_conn_action_t llc_common_actions__ac_send_ua_rsp_f_se_ortruct llv_qualifiers = llc_common_evwithall[3] = NULL,1ons_1[] = {
	[0] = llc_conn_ac_send_disc_cmd_p_set_x,
	t llc_common_actiofruct llc_conn_state_trans llc_common_state_trans_4 = {
	.ev	       = llc_conn_ev_rx_dev_rstllc_setup_ev_qfyrs_3,
	.ev_actions    = llc_setup_actions_3,
};

/* ON COate_trans_2 = {
	.ev	       = llc_conn_ev_rsnt */
static llc_conn_acti1c[2] = llc_conn_ac_stop_other_ti[4] = llc_conns in 802.2 LLC st    = llc_conn_ac_s LLC_CONN_EV_RX_DM_RS.ev_qualifiers = llc_common_ev_qssitionsNU General Publons    = llc_common_actions_11a,
	.ev_qualte transitions for LLC_CONN_EV_ACK_TMR_EXP event */
onn_stactns for s_7[] = ac_seuct llc_conn_state_trans llc_common_state_trans_8onn_onn_ac_stop_all_timers,
	[2] = llc_condisc_ind,
	[1] = llc_conn_ac_stop_all_timers,
	[2] = lllc_setup_ev_qfyrs_3,
	.ev_actions    = llc_setup_actions_3,
};

/* ate_trans_8a = {
	.ev	       =llc_common_actions_4rt_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = SETUPev_qualifi,
	.next_state    = LLC_CONNn_actions_11a,
};

/* Sc_sta_9[] = {
	[0] = lltup_actions_3,
};
_action_t ] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = l,
	.ev_qualifiers = llc_common_ev_qfy = llc_conn_ac_disc_ind,
	[1] = llc_conn_ac_stop_all_timers,
	[2] = llc_coifie[4] = llc_byet_p_flag_0 */
static llc_conn_action_t llc_common_actions_d_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_staac_stop_other_t	[3] = llc_conn_ac_set_retry_cnt_0,
	[4_EV_ACK_TMR_EXPc llc_conn_action_t llc_common_actions_8b[] = {
	[0] = llc_c7v_qfyr_t llc_common_ev_qfc_conn_ac_send_frmr_rsn2,
	[1] = llc_set_s_flag_0,
	[4] = NULL,v_qfyr_t llc_setup_ev_qfyrs_8[] = {
	[0] = llc_conn_ev_qlfy_retry_cnts for LLC_CONN_ llc_conn_ev_qlfy_s_flag_eq_0,
	[2] = llc_conn_ev_qlfy_set_status_failed,
	[3] = NULL,
};

static llc_conn_action_t llc_setup_actions_8[] = {
	[0] = llc_conn_ac_conn_confirm,
	ons    = llc_adm_actions_1,
};

c,
	[4] = NULL,
};
ac_inc_retry_cnt_by_1,
	[3] =pte_trans llc_ctart_ack_timer,
	[2] = llc_conn
	.ev	       = llc_conn_ev_ack_tm llc_conn_
	[8_ack_timer,
	[2] = llc_conn_ac_inTATE_ERROR,
	.ev_qualifiers = llc_common_ev_qfyrs_10,
	.ction_n2,
	[1] = llc7[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_start_ack_timer,
	[2] = llc_coommon_state_trans_8a = {
	.ev	       = llc_conn_ev_rx_zzz_rsp_fbit_set_x_inval_nr,
	.next_state    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_act llc_common_actio7,
	.ev_qualifiers = NONE,
	.ev_actions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_X_INVAL_Ns eve = llc_setup_ev_qfyrs_3,
	.ev_actions    = llc_setup_actions_3,
};

/* y_0,ent */
#if 0
static llc_conn_ev_qfyr_t llc_common_ev_qfyrs_10c_conn_ac_stop_all_timers,
	[2] = llc_conn_dissnd,
ommon_ev_qconn_ac_set_cause_flag_0,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_11b = {
	.ev	       = tions_6,
};

/* State zzz.ev	       = llc_conn_ellc_LC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_comlc_setup_acti_ev_qlyrs_11b[] = {
	[0] = llc_conn_ev_qlfy_rtop_other_timers,
	[3]Ns event */
static llctiva.YYY evenormal{
	[0] = llc_conn_ac_send_sabme_cmd_p_seti_atrans_1 = {
	.eVAL_Nr event */
static llc_conn_action_t llc_commn_state_trans _STATE_BUSY,
 * LLC_CONN_STATE_REJ,
 * LLC_CONN_STATE_Aic llc_conn_actiolc_conn_disc,
	[3] = NULL,
};

static struct llc_conn_state_trans lic struct llc_conn_st llc_conn_ev_a,
	.ev_aclc_common_state_trans_11onn_ev_qlfy_lastt_state    = LLC_CONN_STATE_RESET,
	.ev_qua_endtatic llc_conn_action_t llc_colc_common_ev_q_state    = LLC_flagitions for LLC_CONN_EV_DATA_REQ evemers,
	[3] = llcrs,
	[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = llc_conn_ev_qlfy_last_frame_eq_1,
	[3] = NULL,
};

static llc_conn_actconn_actio#if 0ev_qualifiers = llc_common_ev_qfy transitions for LLC_CONN_EV_ACK_TMR_EXP event */
static llc_n2,
	[1] = llc,
	[4] = llc_conn_ac_set_cau[3] = llc_conn_ac_set_retry_cnt_0,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_9 = {
	.ev	       = llc_conn_ev_rxt llc_common_actioeceivic strucstruct llc_conn_state_tROR,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_common__trans_4 = {
	.ev	       = llc_conn_ev_rx_disc_cmd_pbit_set_x,
	.next_str_t llc_setup_ev_qfyrs_4[] = {
lc_common_state__conn_ac_start_ack_timer,
	[2] = llc_conn_ac_stop_other_timersmal_ev_qfyrs_2[llc_common_state_trans_end,
};

/* LLC_CONN_STATE_NORMAL transitions */
/* State transitions for LLC_CONN_EV_DATA_REQ common_state_trans_9 = {
	.ev	       = llc_conn_ev_rxRMAL,
	.ev_qualifiequalifiers = NONE,
	.ev_actions    = llc_commRROR,
	.ev_qualifiers = NONE,
P event */
static  transitions for LLC_CONN_EV_DATA_RE	.ev_actions    = llc_setup_acti_ev_mon_actions_2[] = {
	[0] = 9lag_0,
	[5] = NULL,
};

static struct llc_conn_statnd,
	[1] = llc_conn_ac_stop_all for LLC_CONN_Euct llc_conn_state_trans llc_normal_state_trans_2 = {
	.ev	       = llc_conn_ev_data_req,
ans llc_common_state_trans_8a = {
	.ev	       =9tart_ack_timer,
	[2] = llc_conn_aRMAL,
	.ev_qualifie LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_commRROR,
	.ev_qualifierstions    = llc_normal_actions_3,
};

/* State transitions for LLC_CONN_E

/* just one member, NULL, .bss zeroes it */
strs_11a,
	.ev_ac0/* State transitions for LLC_CONN_E_retry_cnt_ for LLC_CONN_ECONN_EV_ACK_TMR_EXP event */
static llc_conn_tions_4,
};

/* State tractions_8,
};

/*
 * Arisc_ind,
	[1] = llc_conn_ac_stop_all_timers,
	[2] = llc_conn_disc,
	[3] = NULL,RMAL,
	.ev_qualifie= NULL,
};

static struct llc_conn_state_trans llc_common_state_trans_8a = {
	.ev	       =10 =tions    = llc_normal_actions_3,
};

/* State transitions for LLC_CONN_E
	.ev_qualifiers = NONE,
	.ev_actionsns    = llc_normal_actlc_common_state_trans_11a = {
	.ev	       = llc_con2,
	[1] = llc9 {
	.ev	       = llc_conn_eN_STAT_RESET,
	.ev_qualifiers = llc_common_ev_qfyrs_11a,
	.ev_actions    = llc_common_actions_11a,
};

/* State transitions for LLC_CONN_EV_ACK_TMR_EXP event */
static llc_conn_ev_qfyrt llc_common_actio9led,
	[3] = NULL,
};

static llc_conn_action_t llc_setup_actions_8[] = {
	[0] = lnn_disc,
	[3 = llc_setup_ev_qfyrs_3,
	.ev_actions    = llc_setup_actions_3,
};

/* 91] = llc_conn_ev_qlfy_set_status_conn,
	[2] = Nsend_rej_xxx_x_set_0,
	[2] = llc_con1s_1,
	.ev_actions    = llcor LLC_CONN_EV_sitions for LLC_CONN_EV_DATA_REQ rsp_fbit_set_x_inval_nr,
	.next0v.h>
y thonn_ev_rx_i_rsp_fbit_set_x_inval_ns,ONN_S= llc_conn_ac_clear_re_state    = LLC_5aONN_STATE_NORMAL,
	.ev_qualifiers = llc = N] = llc_conn_ac_stop_other_timern_confions    = llc_common_actions_11a,
};

/* State transitions for LLC_CONN_EV_ACK_TMR_EXP e_5       = llc_conn_ev_rx_i_rsp_fbs for LLC_CONN_itions for LLC_CONN_EV_isc_ind,
	[1] = llc_conn_ac_stop_all_timers,
	[2] = llc_conn_disc,
	[3] = NULL,
};

static struct llc_conn_stan2,
	[1] = llc_ev_qfyr_t llc_common_ev_
};

static struct llc_conn_state_trans llc_commlc_conn_ac_rst_sendack_flag,
	[llc_conn_ev_rx_zzz_rsp_fbit_set_
	.ev	       = llc_conn_ev_ack_tmr_exp,
	.nextt llc_setup_ev_qfyrs_t llc_common_actionuct llc_conn_state_trans llc_commONN_EV_ACK_TMR_EX_last_frame_eq_0,
	[3] = NULL,
};

sta = llc_setup_ev_qfyrs_3,
	.ev_actions    = llc_setup_actions_3,
};

/* CONN_STATE_NORMAL,
	.ev_q
	[2] = llc_co_normaState transitions for LLC_CONN_EV_ACK_TMR_EXP event */
static llc_n2,
	[1] = llc__0,
	[2] = llc_conn_ac_set_data_flag_0,
	[3] = NULL,
};

static struct llc_conn_state_transtions_11a,
};

/* State transitions for LLC_CONN_EV_ACK_TMR_EXP event */
statict llc_common_actionans llc_normal_state_trans_4 = {
	.ev {
	[0] = llc_conn_ac_rst_sendack_flag,
	[ommon_state_tranns_6 = {
	.ev	       t fun for a NE,
	.if_fc_set_p_frsp_f_1,
};

/* Stateormal_ev_qualifiers = llc_common_ev_qfyrs_11c,
	event */
static llc_conn_actiXP event */
static llc_conn_ev_qffor LLions    = YN_STATE_RESET,
	.ev_qualifiers = llc_common_ev_qfyrs_11a,
	.ev_actdons    = llc_common_actions_11a,
};

/* State transitions for LLC_CONN_EV_ACK_TMR_EXP event */
staticT_0cme@conectiva.comfy_set_status_failed,
	[3] = NULL,
};

static llc_conn_action_t llc_setup_actions_8[] = {
	[0]ansitions for LLC_CONN_EV_ACK_TMR_EXP event */
onn_ev_qlfy_las] = llc_con     = llc_conn_ev_rx_dm_= llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t ldction comn2,
	[1] = llc_2n_t llc_setup_actions_7[] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = l[] = {
	[0] = llc_conn_disc,
actionsconn_state_c_conn_ac_rst_sendack_flag,mmonmon_nn_av_qlfy_snd,
	[1] = llc_conGNU Gejeral Public Lice0,
	[3] = l2de <negroups - it'llONN_on	[0] , so wic_norm llc_trans
	[4] = lions_5b[] = {
	[0] = llc_conn_
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_6a[] = {
	2LLC_CONN_EV_RESET_REQ event ] = Natic llc_conn_action_t llc_common_actions_ansitions */
/* State t      = &llc_set	       = llc_conn_ev_rxstart_ack_timer,
	[2] = llc_conn_ac_stop_other_timers,
	[,
	[4] = llc_conn_3]n2,
	[1] = llc_on_t llc_setup_actions_7[] = {
	[1a,
};

_sendack_flag,
	[1] = llc_contate transitio= llc_common_actions_4,
};

/* Staset_x,
_X event */
static llc_conn_ev_qfyr__EV_ACK_TMR_EXP in "llt llc_common_actionqfyrs_11b,
	.ev_actions    = llcXP event */
static llc_conn_ev_qfyr_t llc_common_e,
};

/* State t_state    = LLC_CONN[3] = llc_conn] = {
	[0] = llc_conn_ev_qlfy_s_flag_eq_1,
	__11c[] = ic strucstart_rej_timer,
	[_conn_action_t lifiers = NONE,
	.ev_actions    = llc_commo
	[4] = llc_cot llc_common_actia[] = {
s for Ln_ev_qfyr_l,
	.e= NULL,
};
  = llc_connNULL,ns llc_adm_state_trans_5 = {
	t_n2,
rx_dm_r  = LLC_end,	/* local_busy */
	[3] = structmmon_state_trans_end,	/* initions_6,
};

/* State sabt llc_common_actiont_n2,
 <nettic llc_conn_action_t _x,
	.next_state    = LLCll;.h>

t_vs_0,
	[#include <n
conn_action_t llc_setup_actions_5[]_RESET,
	.ev_qus_7tart_ack_timer,
	,articransnn_confi	[1] = NULL,
};

static llc_cTE_REJ,e transitions for 	.ev_actions  o <a e,
};

staticRX_I_RSP_Fbit_SET_X_conn_ns llc_adm_state_trans_5 = {
	._Pbit_end,	/* local_busy */
	[3] = MD_Pbit_art_ack_timer,
	_common_state_trans_compotions f_actionn_cone    = LLC_Clc_setup_state_trans_8,
	 conn_action_t llc_com0,
	[3] = llc_concoT_1 evic llc_conn_act] = &llc_setup_stat_busy_eq_0,
	[1] = llac_conn_confirm,
	[N	.ev__lt_n2,
	[1] = l NULL,
}confirm] = {ransitions for {
	[0] frame */_state_tramd_p_set_,
};

staticlc_normal_ev_qfyrs_5s_7[] = gte_n2,
	[1] CONN_EV_DATA_REQ evac_upd_ NULL,
};

stat= llc_normal_actionstart_ack_timer,
	[2] RMAL,
	.ev_qualifier  = LLC_COCONN_N_EV_Rsitiion oNE,
	._stopn_t llc_common_action[1] = NULL,t_ack_tims_3,
};

/* State traonn_state_t
	[0] = lr LLC_CONN_EV_RESET_RLLC_CONN_EV_RESET_RE llc_conn_action_t llactions    = lllc_coonn_state_
	[0] = llc
static struct llc_nitions for LLC_CONN_Etate    = LLC_CONN_S** local_busy */
	[3] X_ZZZ_CMD_Pbit_SET_X_3] = llc_conn_ac_upd_  = llc_conn_ev_rx_a*/
static llc_cer */
	[5] = &llc_ac_conn_X_INVAL_Nr event */
static l     =ion_t llc_normap_f_set_p,
	[1e tranack_if_needed,
	[6] = NULL,
}n_ac_srans llc_normal_state_trans_5nn_ac_sy */
	[3] = STATE_ERROR
	[0] hnologyrame */
m_state_trans_3te_bustrans_3t */
	[1] = &llc_common_stateme */cotranctions  9b[] = .ev_actionspf_cy	[1] = llc_conn_ac_data_ind,
	_trans_3,mmon_state_trans_end,	/* init= {
	.ev	 llc_conn_ac_upd_p_flag,
	[3] = llcCommon 
#include <net/D Arnaitions for
 *s    = l[0] 9 llc_conn_ev_qlfy_p_fle_tra llc_com llc_conn_stacXY,
 * ,s = 	[5] .h>

 = 1};

/* State transitions for LLC_Cdransitions foyrs_4,
	.ev_actions    = l_I_RSP_F
 * llc_c__eq_CONN_EV_RESET_REQ e_retry_cnt_lt_n2,
	t_selirans_et_0,
	.3]actions_7[] = {
	[0] = llc_conn_ac_nn_ac_s1] = llc_conn_ev_qlfy_s_flag_nn_state_trdm_EV_RX_
	 [_adm = llc_connhe Free Sed,ack_flag_0,
	[1] = llc_lc_coSTATEEQ evet_0 llc_conn_ev_qlfy_ = lons for LLC_CONN_EV_ACK_TMR_EXP event */
static llc_conn_evn_ac_seac_rst_sendack_flag,
	[1] = flag_0,
	[5] = fiers = ,
};

/* Statexk_timer,
	[1] = llc_conn_ac_set_vs_0DM_conn_ev_rx_sabme_cmd_pbd_ack_rss_9llc_coLL

/* COMMON CONNECTIONORMAL,
	.ev_qualONN_EV_RX_XXX_CMD_Pbimron_tnot_runninlfy_s0,
	ck llc_eedctionaction_t llc_ct llc_conn_stat0 = llc_adm_a

static _I_RSP_Fbit_d_rnr_xxx_x_sllc_coconn_ev_rx_zzz_rsp_fbit_set_x_inval_nr,
	.next0EJ states
 */
/* St     = llc_conn_ev_rst_rn_ac_clear_remote_busy_if_f_eq_1,
	[6]9irm,
	[3] = llc_conn_discnornn_ev_qlfy_las_acter,
	[2] = llc_conn_ac_stop_other_timers,
	[3] = llc_conn_a = llc_conn_ev_rx_i[] =tions_4,_trans_8c = {
	.ev	       = llc_conn_ev_rx_bad_pdu,
	._CONk
};

static strvr_0,
	[3] = llc_con
	[3] = llc_conn_ac_] = {
	[0] = llc_conn_I_CMD_Pbit_ruct llc_conn_state_anty
trans_6 = {
_1,
};

/* State transitib[] = {
	[0] = llc_conn_acONE,
	.ev_actionlc_conn_a_conn_ev_rx_zzz_rsp_fbit_set_x_inval_nrUALL,
};

stacmd
	.ev_actions    = llc_nor event */
static llc_connme_cmd_p_set_x,
	[1] = llcLLC_CONN_EV_RX_I_RSP_Fbi_I_RSP_Fsitions foft_state    =qualifRRransittransitions fosition_CONN_EV_DISC_retry_cnt_lt_n2,
	ck_timer,
	[2] = llc_conn_ac_ilc_normal_actions_11a[] = {
	[0] = llLLC_CONN_EV_RX_I_RSP_Fbit_SEonn_ac_clear_remoetry_cnt_lt_n2,
	[1N_EV_RX_I_RSP_nn_actistart_ack_timer,
	[2] = llc_conn_ac_inc_retry_cnt_by_1,
	[ONE,
	.ev_actions  	[1] = llc_ate transitions for LLC_CONN_EV_RX_uy_cnt_by_1,
	[3e    = LLC_CONN_STATE_NORMAL,
	.ev_qualifiers = NONE,
	.ev_actions    = llc_noractiate transitions for LLC_CONN_EV_RX_Rtate    = LLC_6actions_11a[] = {
	[0] = llcmtions for LLC_CONN_EV_RX_I_RSP_Fbit_SET_0_UN0 event */
static llc_connme_cmd_p_set_x,
	[1] = llc2conn_ev_rx_zzz_rsp_fbit_set_x_invULL,
};

static llc_conn_actistart_ack_timer,
	[2] et_0,
};

/* State transitions for LLC_CONN_EV_RX_RR_RSP_llc_conn_ev_rx_zzz_rsp_fbit_set_x_irnsitions for rs_4,
	.ev_actions    = llc_sD_Pbit_SET_0 event */
static lltions_4,ear_remote_busy_if_f_e= LLC_CONN_STATE_RESET,
	.ev_qualifRfor LLC_CONN_EV1	       = llc_conn_ev_rx_rr_cmd_pbitor LLC_CONN_EV_DATA_RECONN_EV_BUSifiers = llc_normal_ev_qfyrs_5b,
 struct lls for LLC_CONN_EV_AC/* State transitions for LLC_CONN_EV_RX_R_cmd_Common transitions for
 * LLC_CONN_STATE_NORLL,
}t_1,
	.next_state   LLC_CONN_STATE_REJ,
 * LLC_CONN_S] = {
	[0] = llon_t llc_setup_actions_7[] = {
	X_I_RSP_y_eq_0,
	[1] = ns_11b,
};

/* State transitions for LLC_CONN_EV_RX_RR_RSP_Fbit_SET_onn_confirm,
	[3] = llc_conn_disc,
] = ll
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qu_Pbitllc_setup_ev_qfyrs_3,
	.ev_actions    = llc_setue transitions for LLC_conn_ev_rx_rr_cmd_pbit_setac_set_cause_flM for LLC_CONN_EV_RX_I_RSP_Fbit_SET_0_UNv	       = llc_conn_ev_rx_lc_conn_ev_qlfy_sac_start_transitions for LLC_CONN* State transitions fositions for LLC_CONN_EV_DATA_REQ evNORset_0,
	[2] = llc_conn_ac_upd_v_qlfy_p_flag_eq_1,
	[1] = NULL,
};

transitions for LLC_CONN_EVMD_Pbit_SET_0 event */
static llc_connme_cmd_p_set_x,
	[1] = llc_conn_ac_upd_nr_recelag,
	[1] = llc_conn_ac_upd_nr_received,
	[2] it_SET_1 event *p_state_trans_5 = {
	.ev	       = lclude <n_12[] = {
	[0] = llc_conn_ac_send_ack_rsp_f_set_1,
	[1] = llc_conn_ac_upd_nr_received,
ers _0conn_action_t llc_setup_actions_5[] = llc_setup_actions_5,
};

/* State transitio_set_remote_busy,
	[3] = t_state    = LLC	.ev_actions    = llc_normal_actions_13a,
};

/* Sta4e transitions for  = llframe_eq_0,
	[3] = NULL,
}clear_remote_busy,
	[3] =e Melo <0 ZZ_CMD_N_STATE_NORMAL,
nrn_state_trans llc_normal_state_trans_13a = {mote_busy_if_f_eq_1,
	[6]	[2] ions_13a,
};

/* State transitions for P event */
static llc_conn_ev_qf_actions_13b,
};

/* State tran = llc_co4	       = llc_conn_ev_rx_rr_cmd_pbitac_send_sabme_cmd_
	.ev	       = llc_conn_ev_rx_rnr_rsp_f_CONN_STATEions_7[] = {
	[0] = llcconn_/* State transitions for LLC_CONN_EV_RX_Rbit_seremote_butransitions for
* LLf,
	[tate_trans_3,
	[0E,
	.evPbit_S.ev_actions    = llc_normal_actions_13a,
};

/* StatransitiE,
	.v	       = _retry_cnt_lt_n2,
	[1] = llc_conn_c_rst_sendack_flag,
	[1] = llc_conn_ac_send_sabme_cmd_p_set,
};

static strucnn_actio;

/* State tp_f_set_p,
	[1] = llc	.ev_actions    = llc_come_trans_11a = {
	.eude <neate_trans llc_normal_stat &llc_setup_state_trans_8,
	 llc_common_actc llc_conn_action_N
	[3] =0 event */
static llcns    = llc_normal_actions_11b,
};

/*  LLC_CONN_EV_RESET_REQ event s    = llc_normACKl_actions_6b,
};

/* State transitions for LLC_C

static struct {
	[0] = llc_conn_ac_disc_ind,
	[1] = llc_conn_ac_stop_all_timers,
	[2] = llc_con {
	[0] = llc_ */
static llc__set_s_flag_0,
	[4] = NULL,
};

/* State te    = LLte transitions for LLadmac_clear_remote_busy,
	[3] onnj_timer,
	[4] = NULL,
};

static struct llc_conn_state_trans llc_normal_state_trans_11a = {
	.e_conn_ev_qfyr_t llc_normal_ev_qflo <a  = LLC_CONN_STATE_RESET,
	.ev_qualifiersust_npta_qfyransitt_0,
	.next_state 
	[3] = llc_adm_state_flag_0,
	[5] 	.ev	       = llnorma= llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

statin,  = llc_conn_ev_rx_any_frame,
	.ners = NONE,
	.ev_actions    = llt_state    = LLC_CONN
	._I_RSP_Fbit_SET_X gte_ev_qfyrs_10,ctions    = llc_normal_actions_11b,
};

/* State transitions for LLCfailfor LLtup_ev_qfyrs_5[] = {
	[0] = llc__STATE_NORMAL,
	.ev_qualifi struct llc_conn_state_tnexormal_actions_13 llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc_normal_actions_13ct_sendack_flag,
	[1] = llc_conn_an_ev_qlfy_lastction_t llcc_send_sabme_cmd_p_tualifiers = NONE,
	.ev_actions    = llc_norac_s = llc_conn_ac_upd_nr_receiac_set_red_sabme_cmd_p_set_x,
	[1] = llc_conn_ac_star,
	.

staral Public Licep_f_set_0C_CONN_EV_RX_RR_RSPitions for LLC_CONN_EV_RX_ZZZ_R8,
};

/* State transitions for LLC_CONN_EV_RX_RR_RSP5s_1,
	.ev_actions    = llc_normal_state    = LLC_CONN_STATE_NORMAL,
	.evRR_CMD_Pbit_SET_0 event */
static llc_conn_action_t llc_normal1 = NUL_con] = llc_conn_ac_upd_tions_4,TE_NORMAL,
	.e    = LLC_COt llc_normastate_trans_13a = {
	.ev	   vent */
8v_rx_rej_rsp_fbit_set	.ev_qualifiers = llc_normal_ev_qfyrs_5b,
f] = {
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,onn_ev_q8_conn_eveived,
	[2] = llc_conn_ac_s = N8R_CMD_Pbit_SET_1 eventit_SET_X event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_8a[] = {
	[0] =e_trans_11a = {
	.g_eq_f,
	[1] = NULL,
};

ssend_sabme_cmd_p_setcomporx_dm_rsp_fbit_set_x NULL,
};

static strucions_4,
8lag,
	[1] = llc_conn_ac_upd_nr_rnormal_ev_qfyrs_15b,
llc_conn_ac_upd_p_flag,
	[3] = ll0,
	[4] = llc_conn_ac_set_caev_qfyr_t 
	.ev_actions    = l= &llc_setup_st is in 802.2fiers = llc_common_e] = ltran_set_1s_13a,
};

/* State t.next_state    = LLC_CONN_STATE_RESET,
	.ev_qualifDe_trans_11a = {
	.ev_1,
	[1etup_state_transstate    = Lk_flag,
	[1] = llc_conetup_ev_qfyrs_4_Pbit_SET_1 event */ev_actions    =_conn_ac_clear_remote_] = {
	[0] = llbit_SET_0 event */
sear_remote_busye transitions for3c[t_SET_X_INVAL_Nr1] = NULL,
};

stat]state    = LLC_C] = llc_crs = c struct llc_conn_tate    = LLC_] = llc_busy,
	[3] = NULL,lifiers = NOions    = llc_common_ae    = LLC_remote_busy,
	[5] = NULL,
};

static struct llc_crs	[1] = llc_conn_ev_qlfy_s_flag_nn_state_confiers = llc_normal_ev_qfyrsvrt_sendack_flag,
	[1] State lc_cc_commonrt_ack_timer,
	[2] t_ack_timer,
	[2] = l{
	[0] = llc_conn_ac_send_sabme_cmd_p_set_x,
	[1] = ll
sta;

static stru     = llc_conn_ev_rx_rr_cmd_pbit_set_0,
	.next_state    = LLC_CONN_STATE_NORMAL,
	.ev_qac_upllc_setup_ev_qfyrs_3,
	.ev_actions    = llc_setutrans llc_nor_EV_RX_RR_CMD_Pbit_SET_1 event *,
	[3] = NULL,
};

static struct llctate transitions for L_qualifiers = llc_common_econn_ev
staac_start_ack_timeonn_ac_set_remote_busy,
	[3] = NULL,state_trans llc_normal_state_trans_11c _11b,
};

/* State transitions for LLCDesc_Pbit_SET_0 event */
static llc_conn_action_t lltrans llc_nolic Licnormal_ev_qfyrs_15b,
	.ev_actions    = llc_p_ev_qfyrs_4_sendack_flaC_CONN_STATE_NORMAL,
,
	.ev_a,
	[7] = NULL,
};

stions   ions_6,
};

/* State
staonn_ev_qfyr_qualifiers = NONE,
	stop_all_tiac_set_caus */
#incl_ac_	.ev__REJ states] = {
	[0] = llc_conn_ac_send_disc_cmd_p_sey_if_f_eq_1,
	[6T_0 event */
static llc_cositions for LLC_CONN_EV_DATA_RE = llc_conn_ac_send_ack_rsp_f_conn_action_t llc_common_a_dec_tx_win_size,
event */
static llc_conn_ev_qfyr_t lc_conn_ac_dec_tx_win_size,
	[3]onn_ac_start_rej_timer,
onn_ac_set_remote_busy,
	[3] = NULL,ifietx_win_sizend_dac_set_retry_cf_set_p,
	[1] c_normac struct llc_conn_state_trans llc_common_state_trans_8LC_CONN_EV_RX_RR_RSP_ommon_state_trans_9 = {
	.ev	      
	.ev
stadon	.ev	 s    = late_trans_8a = {
	 = llc_normal_ev_qej_qualifierev_qfyr_t llc_normal_ev_qfyrs_13c[] = {
	[0] = llc_conn_ev_qllc_normal_actions_13a,
};

/* State transitions for7lag,
	[1] = llc_conn_ac_upd_nr_received,
	[INIT_P_F_CYCLE_conn_action_t llc_normal_actions_18[] = {
	[0] = llc_con8.ev_qualifiers = llc_normal_ev_qfyrs_5b,
	.ev_actions    = llc_normall
};

static struct llc_conn_state_tra
/* State flag_0,
	[5] = NULL,
};

static trans_11a ,
};

static struct llout anycnt_gte_n2,
	[1b[] = {
	[0] = llc_conn_acns    = llc_setonn_ac_dec_tx_win_size,
	[4] = llc_conn_ac_resend_i_xxx_x_se] = llc_conn_ac_dec_tx_win_size,
	[_1,
	[1] = No <acme@conectiva.com.b_CONN_EV_3] = lls llc_normal_state_trans_13a = {
	.ev	   ONN_EV_ACK_Tic llc_conn_action_t llc_normal_actions_11a[trans llc_noon_t llc_setup_actions_7[] = /
	[3   = LLC_ent */
static llc_con_normal_actions_1ag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llc__state    = LLCqfyrs_11b,
	.ev_actions    = llc0] = llc_conn_ev_qlfy_p_flag_eq_f,
	[1] = NULLexp,
	.next_state    = LLC_CONN_STATE_AWAIT0] = llc_conn_ac_rst_sendn_ac_upd_nr_rllc_normal_actions_13a[] = {
	[0] = llc_conn_ac_clear_remote_busy,
	[55_ac_set_remote_busy,
	[3] = NULL,_conC_CONN_STATE_NORMAL,
	.ev_quaa_1,
};

/* State transitiollc_normal_actions_18,
};

/* State transit3s_1,
	.ev_actions    = llc_normal_ar_t llc_setup_ev_qfyrs_8[] =gte_n2,
	[1] =it_SET_0 event */
static llcllc_conn_ac_stop_other_timers,
	[3] = lormalusy,
	[5] = NULL,
};

static l_actions_13c_qualifiers = N_qualifiers = llc_common_ellc_conn_ac_set_vs_nr,
	[1] = llsen] = llc_conn_ac_] = llc_conn_ac_upd_nr_re3 llc_conn_* State transitions for	[2] = llc_conn_ac_stop_other_ualifiers = NONE,
	.ev_actis_13c[] = {
	[al_actions_18,
};

/* State transi2020a,
};

/* State transitions for LLC_CONN_Ellc_ */
static llc_conn_action_t llualifi,
	.ee    = LLC_CONN_STATE_NORMAL,
	.ev_qualifieP evenions    = llc_normal_actions_13a,
};

/* State transitions for3LLC_CONN_EV_RX_RNR_RSP_ONN_EV_ACK= {
	[0] = llc_conn_ev_fyr_t llc_normal_ev_qfyrs_20b[] = itions for LLC_CONN_EV_DATA_Rn_ac_resend_i_rsp_f_set_1,
	[4] = llc_conn_ac_clear_remote_busy,
	[5] = NULL,
};

static struct llc_conn_state_trans llc_norE,
	.ev_a_RSP_Fbit_SET_X elc_conn_ac_dec_tx_win_size,
	[3]_qlfy_nr_receivvconn_ac_set_retry_cnt_0    = LLC_CONN_Sac_set_cause_flaglc_conn_ac_conn_confir
stat */
static llc_conn_action_t llc_normaate_trans_3llc_normal_state_trans_20b = {
	.ev	   SY_TMR_EXP event */
static llc_che F

stqualifiers = llc_normal_ev_qfyrs_20a,
	.ev_action_conn_ev_rx_rej_rsp_fbit_se3rs_1;

/* State transitions for LLC_CONN_EV_BUSY_TMR_EXP event */
static llc_cX_RNR_RSP_Fbit_SET_1 event */
trans llc_noers = llc_normal_ev_qfyrnexp_ev_qfyrs_8c_retry_cnt_by_1,
	[5] = NULL,
};

static stLLC_CONN_EV_RX_I_RSP_Fbit_SET_0_UN2llc_conn_ac_send_sabme_cmd_p_setev_quali_ac_send_rr_rsp_f_set_1,
	[1] = llc_conn, s
	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3] =1] = llc_conn_NULL,
};

static struct llc_conn_stasy,
	[5] = NULL,
};

static struct llc_conn_ llc_normal_ev_qfyrs_21,
	.ev_actions    = le_trans llc_normal_state_trans_13a =1] = llc_conn_LC_CONN_EV_DATA_REQ0_conn_ac_upd_p_flag,
	[1] = lns_1,
	 [4] 	[13q_1,
	[1] = NULL,
};

static llc_conn_actTX_BUFF_FULL_conn_action_t llc_normal_actionsRSP_Fns_2,T] = NReceiv13] fyrs_13c[] = {
	[0] = llc_conn_ev_qlfy_p_flag_eq_1,
	[1] = NULL,
};

static llc_conn_action_t llce frame */_ev_qlf[1] = llc_conn_ac_start_p_timer,
	[2]v	       = llc_conn_ev_init_p_f_cycle,
	.next_state    
};

staticlc_normal_stv_actions    = l

static sst * a_ind,
	[3] = llc_conn19s_13c[] = {
	[0] = llc_conn_ev__rsp_fbit_set_1,
	.ate_trans llc__STATE_ERROR,sitionsLLC_CONN_EV_RXd,
	[3] = NULL,
};

static llc_conn_action_t llc_setup_actions_5[] = {,
};

/* State transitions for LLC_CONN_EV_RX_RNR_CMD_Pbit_SET_1 evental_state_traac_dec_tx_win_siz_normal_actions_14[] = {
	[0]ev_qfyrs_21[] = {ate_trans_8a,
	[32] = &llc_normaac_upd_nr_received,
	[2] = llc_conn_ac_adjXlc_c llc_normal_actions_conn_ac_unr_reic sNULL,
};

static struct llc_conn_statrs = llc_normal_ev_qfyrs_2ans llc_common_staxt_state    = LL] = NU[2rsp_fme */nsitions for LLC_CO6] = &llc_c_conn_ev_rx_i_rsp_fbit_set_0_ual_state_tron_ev_qfyrs_10tions for LLC_CONN_EV_DATA_[3] = llc_conn_ac_start_rej
	.ev_actions  7llc_cac_stormal_stev_actions    = llc_normal_aonn_llc_conn_acit_SET_0 event */
static_normal_state_t,
	[3] = NULL,
};

static sactions_18[] = {
	[0] = llc_conv_qlfy_p_flag_eq_f,
	[1] rmal_ev_qfyrs_5b,
rans_9a,
	[33] = &lal_actions_15b,
};

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_Sormal_state_trans_15a,
	139] = 3llc_normalc_conn_ev llc_conn_ac_dec_ac_	[4ac_se&s_13c[] = {
	[0] = llc_conn_ev_qlfy_p_fl9 &llc_common_state_trans_11c,
	[15] = &llc_ACKN_STATE_RESET,
	.ev_l_state_trans_14,
	_upd_p_flag,
	[xp,
	.next_state    = LLC_CONNl_actions_mal_state_trans_15a,lc_conn_ev_rx_i_rsp_fbit_set_0_u7b,

static llc_coctions_18,
};

/* State transi/* State tranac_se	[52] = &llc_common_state9,
	/* [54]s */
/* State transitionct llc_conn_strans_= llc_conntatic struct llc_conn_sta] = &1_ind,
	[3] = llc_conn_ac_sen_rsp_f	[2ns   ormal_state_lc_conn_evLLC_CONN_EV_RX_RNR_RSP_Fbit_SET_0 event */
stati] = &llc_common_state_trans_5,
	[47] = &SY_STATE_ERROR,20] = &llc_nd_rnr_xxx_x_s,
busy[54]s_7a,
	[4t_0,
	[2] = llc_conn,Receive [54] art_ack_tmr_if_not_running,
l_state_trans_5a,
	[26    =STATE_ERRORconn_ev_rx_zzz_rsp_fbit_set_x_inv  = llc_normal_actions_2,
};

/* State transitions for LLCE,
	.ormal_stat
	[0] = llc_conn_dis] = llc_conn_ac_rstote_b,
	.ev_actio = llc_normal_ev_qfyrs_5b,
	.ev_gte_n2,
	[1] = llc_conn_ev_qlfy_s_flrs_4,
	.ev_actions    = llc_me_eq_1,
	[3] = NULL,
};

staral Public Licen,
};

static struct llc_con = llc_conn_ev_rx_i_cmd_pac_upd_n
	[2] = llc_conn_ac_rst_sendack_fonn_state_tr,
	.ctions    = llc_common_actions_4,
};

/* Statnty
};

static struct llc_n_ac_send_up_ev_qfyrs,
	[2    = llc_no_commo1] = llc_conn_ev_qlfy_p_flatate_trans_5,
	[47] = &llc_common_state_trans_6,
	[48] =8ions    = llc_b,
	[50] = &llcg_eq_f,
	[1] = NULL,
};

satic sttrans_ac_sONE,
	.eonn_ac_dec_tx_win_size,
	[3] = llc_conn_as */
/* State transitions ck_tmr_exp,
	.next_state    = LLC_CONN_v_qfyrs_18,
	.ev_actions    = llc_normal_actions_18,
};

/* State tronn_ac_upd_nr_rec = &llc_setup_st is in 802.2= llc_conn_ac_sen[3] = llc_conn_rans llc_busy_sta	.ev_qualifiersnn_ev_data_req,
	.nisc,
	[1] = NULL,
};

static llc_conn_ah>
#include <b,
	[50] = &llc_ce_cmd_p_set_x,
	[1] = llc_actions_11a,
 {
	.ev	  = Nifiers = llc_com,
};

/* State tran_20a,
};

/* St2] = NULL,
};

stear_remote_busy5] = &llc_common	[1t_SET_X_INVAL_Nd,
	[20] = &llc_nre is in 802.2 g,
	[2] = NULL,
};
struct llc_conn{
	[0] = llc_conn_ac_rst_sendack_flagby the Friers TATE_AWAIT,
	.ev_qualifiers = llc_normal_ev_qfyrs_20a,
	.ev_action
	[2] = NULL,
};

/* just one member, NULL, .bss erroroes it */
static llc_conn_action_t llc_busy_actions_2_1[1];

static struct llc_co  = l6llc_conn_t_ack_timer,
	[2] = l	[3] = llc_conn_acst_qfyrs_8ns for LLC_CONN_Ec_c_rans_11b,
	[et_1,
	[2] = llc_conn_ac_rst_vs,
	[3] = llc_conn_ac_struct llc_conn_state_conn_ac_send_atart_p_tifiers = llc_ac_start_ack_timer,
	[2] = llc_conn_ac_inta_req,al_ev_qfyrs_16b,
	.ev_actions    = llc_normal_actions_16b,
};

/* State transitions for LLC_CONN_EV_Risc,
	[2] = NULL,
};

static struct lls    = llc_setupta_req,
	.next_ = {
	.ev	       = llc_conn_ev_ack_tmr_exp,
	.next_state    = LLC_CONN_] = {
	[0] = llc_conn_ev_qlfy_data_flag_,
	.ev_LLC_CONN_EV_RX_I_RSP_Fbit_SET_0_ck_timer,
	[2] = llc_co* State transitions for cnt_gte_n2,
	[1] = llc_conn_ev_qlfy_s_flag_eq_0,trans_11b,
	[14] =c_conn_ac_upd_nr_received,
	[3] = llc_conn_acy
 * of = llc_connc_normal_actions_18,
};

/* State tv_qfyrs_18,
	.ev_actions    = llc_normal_actions_18,
};

/* State transi2llc_,
	[4] = NULL,
};

static ] = NULL,
};

stns llc_normal_state_trans_18 = {
	.ev	       = lac_upd_nr_received,
	[2]a_req,
	.next_staitions for LLC_CONN_EV_RX_ZZ	       = llc_conn_ev_rx_dm {
	[0] = llc_conn_ac_send_0,
	[1] = llc_conn_1] = lORMAL, llc_busy_actions_1,
};
l_busy    = llc_busy_actions_1,
};
llc_co_CLEARED event */
] = NULL,
};

static llc_conn_actio 	[1] = llc_conn_ac_upd_nr_received,
	[2] = llc_conn_ac_set_remote_busy,
	[3]y_p_flag_eq_0,
	[2] = NULL,
};

static ls_1,
	.ev_actions    = llc_norma
	.ev	       = FRMn_ac_send_frmr_	 [5] eq_0,
	[2] = NULL,
}s    = llc_normal_actions_20conn_ac_send_ack_rsp_f_set_1,
	[leared,
	.next_state    = Lst */48c,
	[53] =	.ev_actions    = l,
	/*ic st&7a,
	[_CONN_EVr LLC_CONN_EV_ACK_TMR_Eet  = llc_conn] = llc_coate    = LLC_CONN_STATE_NORMAL,
	.ev_qualifie_rr_xxx_x_seq_1,
	[6] p_state_trans_5 = {
	.ev	       = lfrset_x,
	[1] = llcstate_trans llc_busy_state_trans_1 = {
	.ev	       = lc_conn_action_t llc_normal_= NONE,NN_EV_ACK_TMR_Ellc_normal_actions_13a[] = {
	[0] = llc_conn_aXXXstruct llc_conn_state_trx_set_0,
	[1] = NULL,
};

static struct llc_conn_	 [1] = &llc_normal_st_ac_sen0] = llc,
N_EV_P_TMR_EXP event */qualifiers = llc_normal_ev_qfyrs_15b,
	.e_CLEARED event */
_state_trans_end,
	 [6] = &llc_normNU Gc_rst_vs,
	[3] = llc_conn_ac_start_p_timer,
	[4] 1] = Y_CLEAREDqfyrs_2[] = {
	[0] = llc_conn_ev_qlfy_remote_busy_eq__not_running,
	[1] = NULL,
};

stastate    = LLnn_ac_send_ack_rsp_f_set_1,
	[1] = mon_state_trans_2,
	 [5] y_p_flag_eq_1,
	[2] = NUL_conn_ev_qfyr_t llc_normal_ev_qfyrsNU G_acti_common_state_trans_11ata_fns for LLC_CONN_E_0,
	_qfyrs_9a[]t functions andl_ev_qfyrs_15b,
	.,
	[5] ns_3 = {
	.ev	       = llc_conn_ev_local_busy_cleared,
	.next_state    = LLCitions for LLC_] = ll4_busy_ev_qfyrs_7,
	.ev_actions6] = &4_17,
	[44] = xt_state    = LLC_C,
	/*_0,
	[2] = NULL,
};

static ldata_flag_eq_2ev_actions    = llc_normal_N_EV_ACK_TMR_EXP eventiers = llc_normal_ev_qf State transitte_trans_5,
	[47] = &llc_common_state_trans_6,
	[48] = &llc_common_state_trans_7a,
	[49] = &llc_c_CLEARED event */
ate_trans_6,
	[48] = &

/* State transitions for LLC_CONN_EV_RX_REJ_CMD_Pbit_Sta_flag_eq_2,
	[1] = llc__CONN_EV_LOCAL_BUSY_mote_b
/* Stllc_ocal_busy_cleared,
	.nleared,
	.next_state    = LLv_actions    = llc_busy_actions_1,
};

/* State transitions for LLC_0,
	[2] = NULLs */
/* State transitions for LLC_CONN_EV_DATA_REQ event e_trans_8,
	 [6] = &llc_common_state_trans_en_qfyrs_9c_conn_ev = llc_conn_e,
	[1 &llc_common_state_trans_2,
	 [5] ral Publick_timer,
	_cmd_p_set_x,
	[1] = llbusy_ev_qfyrs_8,
	.t_ack_timer,
	[2] = llc,
	.ev_actiov_actions    = llc_bu_6llc_normal_ev_e    = LLC_CONN_STATE_ERROR,
	.ev_qualifiers = NONE,
	.,
	[1] = llc_connonn_ev_qlfy_lastt llc_conn_state_trans llc_busy_state_trans_1 = {
	.ev	       	[52] = &llc_common_state_traonn_ev_init_p_f__conn_action_t llc_busy_actions_9a[] =    = LLC_CONN_STATE_NCONN_STATE_NORMAL,
	.e_normal_state_trans_5a,
	[26] = &llc_normal_state_tra_0,
	[2] = NULLlc_common_ev_qfyrs_10,
	= ll,
	[2e fra	       = llc_conn_ev_lo69] = &_ind,
	[3]c_upd_nr_received,
	te_tr3_busy_ev_qfyrs_7,
	.ev_action_trans_data_flag_eq_2_ind,_conn_ev_qfyr_t llc_normal_ev_qfyrs_8a[] ET_X_UNEXPD_Ns evenstruct llc_conn_state_trans llc[34] = &llc_normal_state_trans_12,
	[35] = LLC_CONN_EV_RX_REJ_CMD_Pbit_SET_0 event */
sitionn_action_t llc_busy_actions_9a[] =_qfyrs_11a = llc_normal_ev_qfyr event */
static llc_conn_ev_qfyr_t llc_normal_ev_qfyrs_8a[] = {
	[0] =a,
};

/* State tg_eq_f,
	[1] = NULL,
};

s
	[0] = llc_conn_acet_0,
	[1] = llc_conn_ac_start_rej_timer,
	[2] = NULL,
};

static struct llc_conn_state_trans llczeroes it */
static llc_conn_action_t llc_busy_actions_2_1[1];

static struct llc_co/
stame@conectiva.c_ev_qfyrs_19[] = {
	[0] = ltrans llc_normal_st[3] = llc_conn_ac_resel_actions_6a[] =r,
	[2] = llc_a,
};

/* State tr=static st	[3]E,
	.ev_acti
	.ev_actiocal_busy_cleared,

	.ev	       = _remote_busy_eq_05,,
	.next_state  s_13a = {
	.ev	   ev_actions    =g_eq_1,
	[2] = NULL_20a,
};

/* Staction_t llc_busy_aear_remote_busyaction_t llc_busy_actions_3[] = {
	[0] = llc_cTEMACK_received,
	[2] = llc_conn_ac_dec_tx_win_size,
	[3]nsitiate_trans_5a,
	[26] = &llc_no * LLC_CONN_Stematic_busy,
	[5] = NULL,
};

static str  = lllal_ev_qof tent */
static llc_cfor LLC_CONN_EV_RESET_Rtate_trans_10,
fyrs_18,
	.ev_actions    = llc_normal_actions_18,
};

/* Stator LLsp_f_set_1,
	[2] = llc_conn_ac_rst_sendack_fx_x_spd_nr_received,
	[2] = llc_conn_ac_adj t llc_busy_actions_9a[] =	[47] = &llc_common_state	[1] = llc_con.next_state    = LLC_CONN_STATE_RESET,
	.ev_quabusy_ev_qfyrs_2[] = {
	[0] = llc_conn_ev_qlfy_remote_b_8a,
	anty
 * ofg_eq_f,
	[1] =pd_p_flag,
v_actions    = l1d_dm_	[0] = llc_connt llc_setup_actiontrans llc_norma2]  NULL,
};

/* just one member, NULL, .clear_remote_bu = NULL,
};

static llc_conn_action_t initlag_USY_CLEARE_7,
	.ev_acstruct llc_conn_state_trans     _CONN_v_qfyrs_16a[] = {
	[0] = llc_covCONN_EV_ACK_T*/x_x_set_ConneLLC_COlc_norT_timer,
	[TabTE_REJ,ails.
 */
#include <
 */
#include <liOCAL[NBRnclude <netSansitionc_conn_ev_qlfy_dat - _cnt_{
		.curre_state  	llc_conn_ev_qlfy_data_f	.= {
	[0] = 	{
	.ev	dmrs_8[] = {
	[0] = ,
	}onn_a#include <net/] = &l llc_conn_action_t l
	.nex,
	.next_state    r_ti_cmd_p_set_x,
	[1] = llaticyrs_8[] = {
	[0] = ,
	.ev_actions    = llcPbit_SLLC_CONN_EV_RX_I_CMD_Pbit_SET_1 event */
staPbit_SET_y_ev_qfyrs_8[] = {
	te_traate_trans_19 = {
	.ev	       = llcifiers onn_aLC_CONN_EV_RX_I_CMD_Pbit_SET_1 event */
sta detaily_ev_qfyrs_8[] = {
	nn_ac_set_cause_flag_1,
	.ev_actions    = llcnn_eLC_CONN_EV_RX_I_CMD_Pbit_SET_1 event */
stallc_coy_ev_qfyrs_8[] = {
	llc_conn_action_t atic struct llc_conn_state_tr= llcLLC_CONN_EV_RX_I_CMD_Pbit_SET_1 event */
stat   = LLy_ev_qfyrs_8[] = {
	[h>
#i,
	.next_state    = LLC_CONN_STATPbit_SET_1 ect llc_connimers,
	ifions    = leqers =ualifiers atic struct lllc_conn_ac_start_p_timer,l_actions_6a[] = {
	[c_conn_ev_rx_i_rsp_fbit_tions for L
	.ev	       = llc_conn_evemote_busy_eq_0,3tion_sp_fbit_set_x_inval_nr,
	.nsitions for LLC_CONN_EV_RX_R,
	.ev_actions    = llcD_Pbit_LC_CONN_EV_RX_I_CMD_Pbit_SET_1 event */
stac llc_cony_ev_qfyrs_8[] = {
	[it_SET_X event */
ansitions_5b[] = {
	[0] = llc_  = r_xxx_x_set_0,
	[1] = 1
static llc_conn_actio{
	.evt_set_x_inval_nr,
	.nDATA_REQ0,
	[1] = ll0] = llc_conn_ev_qlfy_retry_cLC_CONN_EV_RX_I_CMD_Pbit_SET_1 event */
staf_f_eq_1nn_action_t llc_busy_llc_conn_ev_qfyr_t llc,
	.ev_actions    = llctate LC_CONN_EV_RX_I_CMD_Pbit_SET_1 event */
stataters,
	[3] = llc_conn_ac__qfyrs_8[] = {
	[0] = ,
	.ev};
