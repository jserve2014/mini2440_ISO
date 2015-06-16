/*
 *
  Copyright (c) Eicon Networks, 2002.
 *
  This source file is supplied for the use with
  Eicon Networks range of DIVA Server Adapters.
 *
  Eicon File Revision :    2.1
 *
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.
 *
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.
 *
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */





#include "platform.h"
#include "di_defs.h"
#include "pc.h"
#include "capi20.h"
#include "divacapi.h"
#include "mdm_msg.h"
#include "divasync.h"



#define FILE_ "MESSAGE.C"
#define dprintf









/*------------------------------------------------------------------*/
/* This is options supported for all adapters that are server by    */
/* XDI driver. Allo it is not necessary to ask it from every adapter*/
/* and it is not necessary to save it separate for every adapter    */
/* Macrose defined here have only local meaning                     */
/*------------------------------------------------------------------*/
static dword diva_xdi_extended_features = 0;

#define DIVA_CAPI_USE_CMA                 0x00000001
#define DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR  0x00000002
#define DIVA_CAPI_XDI_PROVIDES_NO_CANCEL  0x00000004
#define DIVA_CAPI_XDI_PROVIDES_RX_DMA     0x00000008

/*
  CAPI can request to process all return codes self only if:
  protocol code supports this && xdi supports this
 */
#define DIVA_CAPI_SUPPORTS_NO_CANCEL(__a__)   (((__a__)->manufacturer_features&MANUFACTURER_FEATURE_XONOFF_FLOW_CONTROL)&&    ((__a__)->manufacturer_features & MANUFACTURER_FEATURE_OK_FC_LABEL) &&     (diva_xdi_extended_features   & DIVA_CAPI_XDI_PROVIDES_NO_CANCEL))

/*------------------------------------------------------------------*/
/* local function prototypes                                        */
/*------------------------------------------------------------------*/

static void group_optimization(DIVA_CAPI_ADAPTER   * a, PLCI   * plci);
static void set_group_ind_mask (PLCI   *plci);
static void clear_group_ind_mask_bit (PLCI   *plci, word b);
static byte test_group_ind_mask_bit (PLCI   *plci, word b);
void AutomaticLaw(DIVA_CAPI_ADAPTER   *);
word CapiRelease(word);
word CapiRegister(word);
word api_put(APPL   *, CAPI_MSG   *);
static word api_parse(byte   *, word, byte *, API_PARSE *);
static void api_save_msg(API_PARSE   *in, byte *format, API_SAVE   *out);
static void api_load_msg(API_SAVE   *in, API_PARSE   *out);

word api_remove_start(void);
void api_remove_complete(void);

static void plci_remove(PLCI   *);
static void diva_get_extended_adapter_features (DIVA_CAPI_ADAPTER  * a);
static void diva_ask_for_xdi_sdram_bar (DIVA_CAPI_ADAPTER  *, IDI_SYNC_REQ  *);

void   callback(ENTITY   *);

static void control_rc(PLCI   *, byte, byte, byte, byte, byte);
static void data_rc(PLCI   *, byte);
static void data_ack(PLCI   *, byte);
static void sig_ind(PLCI   *);
static void SendInfo(PLCI   *, dword, byte   * *, byte);
static void SendSetupInfo(APPL   *, PLCI   *, dword, byte   * *, byte);
static void SendSSExtInd(APPL   *, PLCI   * plci, dword Id, byte   * * parms);

static void VSwitchReqInd(PLCI   *plci, dword Id, byte   **parms);

static void nl_ind(PLCI   *);

static byte connect_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_a_res(dword,word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte listen_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte info_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte info_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte alert_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte facility_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte facility_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_b3_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_b3_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_b3_a_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_b3_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_b3_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte data_b3_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte data_b3_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte reset_b3_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte reset_b3_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_b3_t90_a_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte select_b_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte manufacturer_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte manufacturer_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);

static word get_plci(DIVA_CAPI_ADAPTER   *);
static void add_p(PLCI   *, byte, byte   *);
static void add_s(PLCI   * plci, byte code, API_PARSE * p);
static void add_ss(PLCI   * plci, byte code, API_PARSE * p);
static void add_ie(PLCI   * plci, byte code, byte   * p, word p_length);
static void add_d(PLCI   *, word, byte   *);
static void add_ai(PLCI   *, API_PARSE *);
static word add_b1(PLCI   *, API_PARSE *, word, word);
static word add_b23(PLCI   *, API_PARSE *);
static word add_modem_b23 (PLCI  * plci, API_PARSE* bp_parms);
static void sig_req(PLCI   *, byte, byte);
static void nl_req_ncci(PLCI   *, byte, byte);
static void send_req(PLCI   *);
static void send_data(PLCI   *);
static word plci_remove_check(PLCI   *);
static void listen_check(DIVA_CAPI_ADAPTER   *);
static byte AddInfo(byte   **, byte   **, byte   *, byte *);
static byte getChannel(API_PARSE *);
static void IndParse(PLCI   *, word *, byte   **, byte);
static byte ie_compare(byte   *, byte *);
static word find_cip(DIVA_CAPI_ADAPTER   *, byte   *, byte   *);
static word CPN_filter_ok(byte   *cpn,DIVA_CAPI_ADAPTER   *,word);

/*
  XON protocol helpers
  */
static void channel_flow_control_remove (PLCI   * plci);
static void channel_x_off (PLCI   * plci, byte ch, byte flag);
static void channel_x_on (PLCI   * plci, byte ch);
static void channel_request_xon (PLCI   * plci, byte ch);
static void channel_xmit_xon (PLCI   * plci);
static int channel_can_xon (PLCI   * plci, byte ch);
static void channel_xmit_extended_xon (PLCI   * plci);

static byte SendMultiIE(PLCI   * plci, dword Id, byte   * * parms, byte ie_type, dword info_mask, byte setupParse);
static word AdvCodecSupport(DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, byte);
static void CodecIdCheck(DIVA_CAPI_ADAPTER   *, PLCI   *);
static void SetVoiceChannel(PLCI   *, byte   *, DIVA_CAPI_ADAPTER   * );
static void VoiceChannelOff(PLCI   *plci);
static void adv_voice_write_coefs (PLCI   *plci, word write_command);
static void adv_voice_clear_config (PLCI   *plci);

static word get_b1_facilities (PLCI   * plci, byte b1_resource);
static byte add_b1_facilities (PLCI   * plci, byte b1_resource, word b1_facilities);
static void adjust_b1_facilities (PLCI   *plci, byte new_b1_resource, word new_b1_facilities);
static word adjust_b_process (dword Id, PLCI   *plci, byte Rc);
static void adjust_b1_resource (dword Id, PLCI   *plci, API_SAVE   *bp_msg, word b1_facilities, word internal_command);
static void adjust_b_restore (dword Id, PLCI   *plci, byte Rc);
static void reset_b3_command (dword Id, PLCI   *plci, byte Rc);
static void select_b_command (dword Id, PLCI   *plci, byte Rc);
static void fax_connect_ack_command (dword Id, PLCI   *plci, byte Rc);
static void fax_edata_ack_command (dword Id, PLCI   *plci, byte Rc);
static void fax_connect_info_command (dword Id, PLCI   *plci, byte Rc);
static void fax_adjust_b23_command (dword Id, PLCI   *plci, byte Rc);
static void fax_disconnect_command (dword Id, PLCI   *plci, byte Rc);
static void hold_save_command (dword Id, PLCI   *plci, byte Rc);
static void retrieve_restore_command (dword Id, PLCI   *plci, byte Rc);
static void init_b1_config (PLCI   *plci);
static void clear_b1_config (PLCI   *plci);

static void dtmf_command (dword Id, PLCI   *plci, byte Rc);
static byte dtmf_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void dtmf_confirmation (dword Id, PLCI   *plci);
static void dtmf_indication (dword Id, PLCI   *plci, byte   *msg, word length);
static void dtmf_parameter_write (PLCI   *plci);


static void mixer_set_bchannel_id_esc (PLCI   *plci, byte bchannel_id);
static void mixer_set_bchannel_id (PLCI   *plci, byte   *chi);
static void mixer_clear_config (PLCI   *plci);
static void mixer_notify_update (PLCI   *plci, byte others);
static void mixer_command (dword Id, PLCI   *plci, byte Rc);
static byte mixer_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void mixer_indication_coefs_set (dword Id, PLCI   *plci);
static void mixer_indication_xconnect_from (dword Id, PLCI   *plci, byte   *msg, word length);
static void mixer_indication_xconnect_to (dword Id, PLCI   *plci, byte   *msg, word length);
static void mixer_remove (PLCI   *plci);


static void ec_command (dword Id, PLCI   *plci, byte Rc);
static byte ec_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void ec_indication (dword Id, PLCI   *plci, byte   *msg, word length);


static void rtp_connect_b3_req_command (dword Id, PLCI   *plci, byte Rc);
static void rtp_connect_b3_res_command (dword Id, PLCI   *plci, byte Rc);


static int  diva_get_dma_descriptor  (PLCI   *plci, dword   *dma_magic);
static void diva_free_dma_descriptor (PLCI   *plci, int nr);

/*------------------------------------------------------------------*/
/* external function prototypes                                     */
/*------------------------------------------------------------------*/

extern byte MapController (byte);
extern byte UnMapController (byte);
#define MapId(Id) (((Id) & 0xffffff00L) | MapController ((byte)(Id)))
#define UnMapId(Id) (((Id) & 0xffffff00L) | UnMapController ((byte)(Id)))

void   sendf(APPL   *, word, dword, word, byte *, ...);
void   * TransmitBufferSet(APPL   * appl, dword ref);
void   * TransmitBufferGet(APPL   * appl, void   * p);
void TransmitBufferFree(APPL   * appl, void   * p);
void   * ReceiveBufferGet(APPL   * appl, int Num);

int fax_head_line_time (char *buffer);


/*------------------------------------------------------------------*/
/* Global data definitions                                          */
/*------------------------------------------------------------------*/
extern byte max_adapter;
extern byte max_appl;
extern DIVA_CAPI_ADAPTER   * adapter;
extern APPL   * application;







static byte remove_started = false;
static PLCI dummy_plci;


static struct _ftable {
  word command;
  byte * format;
  byte (* function)(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
} ftable[] = {
  {_DATA_B3_R,                          "dwww",         data_b3_req},
  {_DATA_B3_I|RESPONSE,                 "w",            data_b3_res},
  {_INFO_R,                             "ss",           info_req},
  {_INFO_I|RESPONSE,                    "",             info_res},
  {_CONNECT_R,                          "wsssssssss",   connect_req},
  {_CONNECT_I|RESPONSE,                 "wsssss",       connect_res},
  {_CONNECT_ACTIVE_I|RESPONSE,          "",             connect_a_res},
  {_DISCONNECT_R,                       "s",            disconnect_req},
  {_DISCONNECT_I|RESPONSE,              "",             disconnect_res},
  {_LISTEN_R,                           "dddss",        listen_req},
  {_ALERT_R,                            "s",            alert_req},
  {_FACILITY_R,                         "ws",           facility_req},
  {_FACILITY_I|RESPONSE,                "ws",           facility_res},
  {_CONNECT_B3_R,                       "s",            connect_b3_req},
  {_CONNECT_B3_I|RESPONSE,              "ws",           connect_b3_res},
  {_CONNECT_B3_ACTIVE_I|RESPONSE,       "",             connect_b3_a_res},
  {_DISCONNECT_B3_R,                    "s",            disconnect_b3_req},
  {_DISCONNECT_B3_I|RESPONSE,           "",             disconnect_b3_res},
  {_RESET_B3_R,                         "s",            reset_b3_req},
  {_RESET_B3_I|RESPONSE,                "",             reset_b3_res},
  {_CONNECT_B3_T90_ACTIVE_I|RESPONSE,   "ws",           connect_b3_t90_a_res},
  {_CONNECT_B3_T90_ACTIVE_I|RESPONSE,   "",             connect_b3_t90_a_res},
  {_SELECT_B_REQ,                       "s",            select_b_req},
  {_MANUFACTURER_R,                     "dws",          manufacturer_req},
  {_MANUFACTURER_I|RESPONSE,            "dws",          manufacturer_res},
  {_MANUFACTURER_I|RESPONSE,            "",             manufacturer_res}
};

static byte * cip_bc[29][2] = {
  { "",                     ""                     }, /* 0 */
  { "\x03\x80\x90\xa3",     "\x03\x80\x90\xa2"     }, /* 1 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 2 */
  { "\x02\x89\x90",         "\x02\x89\x90"         }, /* 3 */
  { "\x03\x90\x90\xa3",     "\x03\x90\x90\xa2"     }, /* 4 */
  { "\x03\x91\x90\xa5",     "\x03\x91\x90\xa5"     }, /* 5 */
  { "\x02\x98\x90",         "\x02\x98\x90"         }, /* 6 */
  { "\x04\x88\xc0\xc6\xe6", "\x04\x88\xc0\xc6\xe6" }, /* 7 */
  { "\x04\x88\x90\x21\x8f", "\x04\x88\x90\x21\x8f" }, /* 8 */
  { "\x03\x91\x90\xa5",     "\x03\x91\x90\xa5"     }, /* 9 */
  { "",                     ""                     }, /* 10 */
  { "",                     ""                     }, /* 11 */
  { "",                     ""                     }, /* 12 */
  { "",                     ""                     }, /* 13 */
  { "",                     ""                     }, /* 14 */
  { "",                     ""                     }, /* 15 */

  { "\x03\x80\x90\xa3",     "\x03\x80\x90\xa2"     }, /* 16 */
  { "\x03\x90\x90\xa3",     "\x03\x90\x90\xa2"     }, /* 17 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 18 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 19 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 20 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 21 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 22 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 23 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 24 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 25 */
  { "\x03\x91\x90\xa5",     "\x03\x91\x90\xa5"     }, /* 26 */
  { "\x03\x91\x90\xa5",     "\x03\x91\x90\xa5"     }, /* 27 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }  /* 28 */
};

static byte * cip_hlc[29] = {
  "",                           /* 0 */
  "",                           /* 1 */
  "",                           /* 2 */
  "",                           /* 3 */
  "",                           /* 4 */
  "",                           /* 5 */
  "",                           /* 6 */
  "",                           /* 7 */
  "",                           /* 8 */
  "",                           /* 9 */
  "",                           /* 10 */
  "",                           /* 11 */
  "",                           /* 12 */
  "",                           /* 13 */
  "",                           /* 14 */
  "",                           /* 15 */

  "\x02\x91\x81",               /* 16 */
  "\x02\x91\x84",               /* 17 */
  "\x02\x91\xa1",               /* 18 */
  "\x02\x91\xa4",               /* 19 */
  "\x02\x91\xa8",               /* 20 */
  "\x02\x91\xb1",               /* 21 */
  "\x02\x91\xb2",               /* 22 */
  "\x02\x91\xb5",               /* 23 */
  "\x02\x91\xb8",               /* 24 */
  "\x02\x91\xc1",               /* 25 */
  "\x02\x91\x81",               /* 26 */
  "\x03\x91\xe0\x01",           /* 27 */
  "\x03\x91\xe0\x02"            /* 28 */
};

/*------------------------------------------------------------------*/

#define V120_HEADER_LENGTH 1
#define V120_HEADER_EXTEND_BIT  0x80
#define V120_HEADER_BREAK_BIT   0x40
#define V120_HEADER_C1_BIT      0x04
#define V120_HEADER_C2_BIT      0x08
#define V120_HEADER_FLUSH_COND  (V120_HEADER_BREAK_BIT | V120_HEADER_C1_BIT | V120_HEADER_C2_BIT)

static byte v120_default_header[] =
{

  0x83                          /*  Ext, BR , res, res, C2 , C1 , B  , F   */

};

static byte v120_break_header[] =
{

  0xc3 | V120_HEADER_BREAK_BIT  /*  Ext, BR , res, res, C2 , C1 , B  , F   */

};


/*------------------------------------------------------------------*/
/* API_PUT function                                                 */
/*------------------------------------------------------------------*/

word api_put(APPL   * appl, CAPI_MSG   * msg)
{
  word i, j, k, l, n;
  word ret;
  byte c;
  byte controller;
  DIVA_CAPI_ADAPTER   * a;
  PLCI   * plci;
  NCCI   * ncci_ptr;
  word ncci;
  CAPI_MSG   *m;
    API_PARSE msg_parms[MAX_MSG_PARMS+1];

  if (msg->header.length < sizeof (msg->header) ||
      msg->header.length > MAX_MSG_SIZE) {
    dbug(1,dprintf("bad len"));
    return _BAD_MSG;
  }

  controller = (byte)((msg->header.controller &0x7f)-1);

  /* controller starts with 0 up to (max_adapter - 1) */
  if ( controller >= max_adapter )
  {
    dbug(1,dprintf("invalid ctrl"));
    return _BAD_MSG;
  }
  
  a = &adapter[controller];
  plci = NULL;
  if ((msg->header.plci != 0) && (msg->header.plci <= a->max_plci) && !a->adapter_disabled)
  {
    dbug(1,dprintf("plci=%x",msg->header.plci));
    plci = &a->plci[msg->header.plci-1];
    ncci = GET_WORD(&msg->header.ncci);
    if (plci->Id
     && (plci->appl
      || (plci->State == INC_CON_PENDING)
      || (plci->State == INC_CON_ALERT)
      || (msg->header.command == (_DISCONNECT_I|RESPONSE)))
     && ((ncci == 0)
      || (msg->header.command == (_DISCONNECT_B3_I|RESPONSE))
      || ((ncci < MAX_NCCI+1) && (a->ncci_plci[ncci] == plci->Id))))
    {
      i = plci->msg_in_read_pos;
      j = plci->msg_in_write_pos;
      if (j >= i)
      {
        if (j + msg->header.length + MSG_IN_OVERHEAD <= MSG_IN_QUEUE_SIZE)
          i += MSG_IN_QUEUE_SIZE - j;
        else
          j = 0;
      }
      else
      {

        n = (((CAPI_MSG   *)(plci->msg_in_queue))->header.length + MSG_IN_OVERHEAD + 3) & 0xfffc;

        if (i > MSG_IN_QUEUE_SIZE - n)
          i = MSG_IN_QUEUE_SIZE - n + 1;
        i -= j;
      }

      if (i <= ((msg->header.length + MSG_IN_OVERHEAD + 3) & 0xfffc))

      {
        dbug(0,dprintf("Q-FULL1(msg) - len=%d write=%d read=%d wrap=%d free=%d",
          msg->header.length, plci->msg_in_write_pos,
          plci->msg_in_read_pos, plci->msg_in_wrap_pos, i));

        return _QUEUE_FULL;
      }
      c = false;
      if ((((byte   *) msg) < ((byte   *)(plci->msg_in_queue)))
       || (((byte   *) msg) >= ((byte   *)(plci->msg_in_queue)) + sizeof(plci->msg_in_queue)))
      {
        if (plci->msg_in_write_pos != plci->msg_in_read_pos)
          c = true;
      }
      if (msg->header.command == _DATA_B3_R)
      {
        if (msg->header.length < 20)
        {
          dbug(1,dprintf("DATA_B3 REQ wrong length %d", msg->header.length));
          return _BAD_MSG;
        }
        ncci_ptr = &(a->ncci[ncci]);
        n = ncci_ptr->data_pending;
        l = ncci_ptr->data_ack_pending;
        k = plci->msg_in_read_pos;
        while (k != plci->msg_in_write_pos)
        {
          if (k == plci->msg_in_wrap_pos)
            k = 0;
          if ((((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[k]))->header.command == _DATA_B3_R)
           && (((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[k]))->header.ncci == ncci))
          {
            n++;
            if (((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[k]))->info.data_b3_req.Flags & 0x0004)
              l++;
          }

          k += (((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[k]))->header.length +
            MSG_IN_OVERHEAD + 3) & 0xfffc;

        }
        if ((n >= MAX_DATA_B3) || (l >= MAX_DATA_ACK))
        {
          dbug(0,dprintf("Q-FULL2(data) - pending=%d/%d ack_pending=%d/%d",
                          ncci_ptr->data_pending, n, ncci_ptr->data_ack_pending, l));

          return _QUEUE_FULL;
        }
        if (plci->req_in || plci->internal_command)
        {
          if ((((byte   *) msg) >= ((byte   *)(plci->msg_in_queue)))
           && (((byte   *) msg) < ((byte   *)(plci->msg_in_queue)) + sizeof(plci->msg_in_queue)))
          {
            dbug(0,dprintf("Q-FULL3(requeue)"));

            return _QUEUE_FULL;
          }
          c = true;
        }
      }
      else
      {
        if (plci->req_in || plci->internal_command)
          c = true;
        else
        {
          plci->command = msg->header.command;
          plci->number = msg->header.number;
        }
      }
      if (c)
      {
        dbug(1,dprintf("enqueue msg(0x%04x,0x%x,0x%x) - len=%d write=%d read=%d wrap=%d free=%d",
          msg->header.command, plci->req_in, plci->internal_command,
          msg->header.length, plci->msg_in_write_pos,
          plci->msg_in_read_pos, plci->msg_in_wrap_pos, i));
        if (j == 0)
          plci->msg_in_wrap_pos = plci->msg_in_write_pos;
        m = (CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[j]);
        for (i = 0; i < msg->header.length; i++)
          ((byte   *)(plci->msg_in_queue))[j++] = ((byte   *) msg)[i];
        if (m->header.command == _DATA_B3_R)
        {

          m->info.data_b3_req.Data = (dword)(long)(TransmitBufferSet (appl, m->info.data_b3_req.Data));

        }

        j = (j + 3) & 0xfffc;

        *((APPL   *   *)(&((byte   *)(plci->msg_in_queue))[j])) = appl;
        plci->msg_in_write_pos = j + MSG_IN_OVERHEAD;
        return 0;
      }
    }
    else
    {
      plci = NULL;
    }
  }
  dbug(1,dprintf("com=%x",msg->header.command));

  for(j=0;j<MAX_MSG_PARMS+1;j++) msg_parms[j].length = 0;
  for(i=0, ret = _BAD_MSG; i < ARRAY_SIZE(ftable); i++) {

    if(ftable[i].command==msg->header.command) {
      /* break loop if the message is correct, otherwise continue scan  */
      /* (for example: CONNECT_B3_T90_ACT_RES has two specifications)   */
      if(!api_parse(msg->info.b,(word)(msg->header.length-12),ftable[i].format,msg_parms)) {
        ret = 0;
        break;
      }
      for(j=0;j<MAX_MSG_PARMS+1;j++) msg_parms[j].length = 0;
    }
  }
  if(ret) {
    dbug(1,dprintf("BAD_MSG"));
    if(plci) plci->command = 0;
    return ret;
  }


  c = ftable[i].function(GET_DWORD(&msg->header.controller),
                         msg->header.number,
                         a,
                         plci,
                         appl,
                         msg_parms);

  channel_xmit_extended_xon (plci);

  if(c==1) send_req(plci);
  if(c==2 && plci) plci->req_in = plci->req_in_start = plci->req_out = 0;
  if(plci && !plci->req_in) plci->command = 0;
  return 0;
}


/*------------------------------------------------------------------*/
/* api_parse function, check the format of api messages             */
/*------------------------------------------------------------------*/

static word api_parse(byte *msg, word length, byte *format, API_PARSE *parms)
{
  word i;
  word p;

  for(i=0,p=0; format[i]; i++) {
    if(parms)
    {
      parms[i].info = &msg[p];
    }
    switch(format[i]) {
    case 'b':
      p +=1;
      break;
    case 'w':
      p +=2;
      break;
    case 'd':
      p +=4;
      break;
    case 's':
      if(msg[p]==0xff) {
        parms[i].info +=2;
        parms[i].length = msg[p+1] + (msg[p+2]<<8);
        p +=(parms[i].length +3);
      }
      else {
        parms[i].length = msg[p];
        p +=(parms[i].length +1);
      }
      break;
    }

    if(p>length) return true;
  }
  if(parms) parms[i].info = NULL;
  return false;
}

static void api_save_msg(API_PARSE *in, byte *format, API_SAVE *out)
{
  word i, j, n = 0;
  byte   *p;

  p = out->info;
  for (i = 0; format[i] != '\0'; i++)
  {
    out->parms[i].info = p;
    out->parms[i].length = in[i].length;
    switch (format[i])
    {
    case 'b':
      n = 1;
      break;
    case 'w':
      n = 2;
      break;
    case 'd':
      n = 4;
      break;
    case 's':
      n = in[i].length + 1;
      break;
    }
    for (j = 0; j < n; j++)
      *(p++) = in[i].info[j];
  }
  out->parms[i].info = NULL;
  out->parms[i].length = 0;
}

static void api_load_msg(API_SAVE *in, API_PARSE *out)
{
  word i;

  i = 0;
  do
  {
    out[i].info = in->parms[i].info;
    out[i].length = in->parms[i].length;
  } while (in->parms[i++].info);
}


/*------------------------------------------------------------------*/
/* CAPI remove function                                             */
/*------------------------------------------------------------------*/

word api_remove_start(void)
{
  word i;
  word j;

  if(!remove_started) {
    remove_started = true;
    for(i=0;i<max_adapter;i++) {
      if(adapter[i].request) {
        for(j=0;j<adapter[i].max_plci;j++) {
          if(adapter[i].plci[j].Sig.Id) plci_remove(&adapter[i].plci[j]);
        }
      }
    }
    return 1;
  }
  else {
    for(i=0;i<max_adapter;i++) {
      if(adapter[i].request) {
        for(j=0;j<adapter[i].max_plci;j++) {
          if(adapter[i].plci[j].Sig.Id) return 1;
        }
      }
    }
  }
  api_remove_complete();
  return 0;
}


/*------------------------------------------------------------------*/
/* internal command queue                                           */
/*------------------------------------------------------------------*/

static void init_internal_command_queue (PLCI   *plci)
{
  word i;

  dbug (1, dprintf ("%s,%d: init_internal_command_queue",
    (char   *)(FILE_), __LINE__));

  plci->internal_command = 0;
  for (i = 0; i < MAX_INTERNAL_COMMAND_LEVELS; i++)
    plci->internal_command_queue[i] = NULL;
}


static void start_internal_command (dword Id, PLCI   *plci, t_std_internal_command command_function)
{
  word i;

  dbug (1, dprintf ("[%06lx] %s,%d: start_internal_command",
    UnMapId (Id), (char   *)(FILE_), __LINE__));

  if (plci->internal_command == 0)
  {
    plci->internal_command_queue[0] = command_function;
    (* command_function)(Id, plci, OK);
  }
  else
  {
    i = 1;
    while (plci->internal_command_queue[i] != NULL)
      i++;
    plci->internal_command_queue[i] = command_function;
  }
}


static void next_internal_command (dword Id, PLCI   *plci)
{
  word i;

  dbug (1, dprintf ("[%06lx] %s,%d: next_internal_command",
    UnMapId (Id), (char   *)(FILE_), __LINE__));

  plci->internal_command = 0;
  plci->internal_command_queue[0] = NULL;
  while (plci->internal_command_queue[1] != NULL)
  {
    for (i = 0; i < MAX_INTERNAL_COMMAND_LEVELS - 1; i++)
      plci->internal_command_queue[i] = plci->internal_command_queue[i+1];
    plci->internal_command_queue[MAX_INTERNAL_COMMAND_LEVELS - 1] = NULL;
    (*(plci->internal_command_queue[0]))(Id, plci, OK);
    if (plci->internal_command != 0)
      return;
    plci->internal_command_queue[0] = NULL;
  }
}


/*------------------------------------------------------------------*/
/* NCCI allocate/remove function                                    */
/*------------------------------------------------------------------*/

static dword ncci_mapping_bug = 0;

static word get_ncci (PLCI   *plci, byte ch, word force_ncci)
{
  DIVA_CAPI_ADAPTER   *a;
  word ncci, i, j, k;

  a = plci->adapter;
  if (!ch || a->ch_ncci[ch])
  {
    ncci_mapping_bug++;
    dbug(1,dprintf("NCCI mapping exists %ld %02x %02x %02x-%02x",
      ncci_mapping_bug, ch, force_ncci, a->ncci_ch[a->ch_ncci[ch]], a->ch_ncci[ch]));
    ncci = ch;
  }
  else
  {
    if (force_ncci)
      ncci = force_ncci;
    else
    {
      if ((ch < MAX_NCCI+1) && !a->ncci_ch[ch])
        ncci = ch;
      else
      {
        ncci = 1;
        while ((ncci < MAX_NCCI+1) && a->ncci_ch[ncci])
          ncci++;
        if (ncci == MAX_NCCI+1)
        {
          ncci_mapping_bug++;
          i = 1;
          do
          {
            j = 1;
            while ((j < MAX_NCCI+1) && (a->ncci_ch[j] != i))
              j++;
            k = j;
            if (j < MAX_NCCI+1)
            {
              do
              {
                j++;
              } while ((j < MAX_NCCI+1) && (a->ncci_ch[j] != i));
            }
          } while ((i < MAX_NL_CHANNEL+1) && (j < MAX_NCCI+1));
          if (i < MAX_NL_CHANNEL+1)
          {
            dbug(1,dprintf("NCCI mapping overflow %ld %02x %02x %02x-%02x-%02x",
              ncci_mapping_bug, ch, force_ncci, i, k, j));
          }
          else
          {
            dbug(1,dprintf("NCCI mapping overflow %ld %02x %02x",
              ncci_mapping_bug, ch, force_ncci));
          }
          ncci = ch;
        }
      }
      a->ncci_plci[ncci] = plci->Id;
      a->ncci_state[ncci] = IDLE;
      if (!plci->ncci_ring_list)
        plci->ncci_ring_list = ncci;
      else
        a->ncci_next[ncci] = a->ncci_next[plci->ncci_ring_list];
      a->ncci_next[plci->ncci_ring_list] = (byte) ncci;
    }
    a->ncci_ch[ncci] = ch;
    a->ch_ncci[ch] = (byte) ncci;
    dbug(1,dprintf("NCCI mapping established %ld %02x %02x %02x-%02x",
      ncci_mapping_bug, ch, force_ncci, ch, ncci));
  }
  return (ncci);
}


static void ncci_free_receive_buffers (PLCI   *plci, word ncci)
{
  DIVA_CAPI_ADAPTER   *a;
  APPL   *appl;
  word i, ncci_code;
  dword Id;

  a = plci->adapter;
  Id = (((dword) ncci) << 16) | (((word)(plci->Id)) << 8) | a->Id;
  if (ncci)
  {
    if (a->ncci_plci[ncci] == plci->Id)
    {
      if (!plci->appl)
      {
        ncci_mapping_bug++;
        dbug(1,dprintf("NCCI mapping appl expected %ld %08lx",
          ncci_mapping_bug, Id));
      }
      else
      {
        appl = plci->appl;
        ncci_code = ncci | (((word) a->Id) << 8);
        for (i = 0; i < appl->MaxBuffer; i++)
        {
          if ((appl->DataNCCI[i] == ncci_code)
           && (((byte)(appl->DataFlags[i] >> 8)) == plci->Id))
          {
            appl->DataNCCI[i] = 0;
          }
        }
      }
    }
  }
  else
  {
    for (ncci = 1; ncci < MAX_NCCI+1; ncci++)
    {
      if (a->ncci_plci[ncci] == plci->Id)
      {
        if (!plci->appl)
        {
          ncci_mapping_bug++;
          dbug(1,dprintf("NCCI mapping no appl %ld %08lx",
            ncci_mapping_bug, Id));
        }
        else
        {
          appl = plci->appl;
          ncci_code = ncci | (((word) a->Id) << 8);
          for (i = 0; i < appl->MaxBuffer; i++)
          {
            if ((appl->DataNCCI[i] == ncci_code)
             && (((byte)(appl->DataFlags[i] >> 8)) == plci->Id))
            {
              appl->DataNCCI[i] = 0;
            }
          }
        }
      }
    }
  }
}


static void cleanup_ncci_data (PLCI   *plci, word ncci)
{
  NCCI   *ncci_ptr;

  if (ncci && (plci->adapter->ncci_plci[ncci] == plci->Id))
  {
    ncci_ptr = &(plci->adapter->ncci[ncci]);
    if (plci->appl)
    {
      while (ncci_ptr->data_pending != 0)
      {
        if (!plci->data_sent || (ncci_ptr->DBuffer[ncci_ptr->data_out].P != plci->data_sent_ptr))
          TransmitBufferFree (plci->appl, ncci_ptr->DBuffer[ncci_ptr->data_out].P);
        (ncci_ptr->data_out)++;
        if (ncci_ptr->data_out == MAX_DATA_B3)
          ncci_ptr->data_out = 0;
        (ncci_ptr->data_pending)--;
      }
    }
    ncci_ptr->data_out = 0;
    ncci_ptr->data_pending = 0;
    ncci_ptr->data_ack_out = 0;
    ncci_ptr->data_ack_pending = 0;
  }
}


static void ncci_remove (PLCI   *plci, word ncci, byte preserve_ncci)
{
  DIVA_CAPI_ADAPTER   *a;
  dword Id;
  word i;

  a = plci->adapter;
  Id = (((dword) ncci) << 16) | (((word)(plci->Id)) << 8) | a->Id;
  if (!preserve_ncci)
    ncci_free_receive_buffers (plci, ncci);
  if (ncci)
  {
    if (a->ncci_plci[ncci] != plci->Id)
    {
      ncci_mapping_bug++;
      dbug(1,dprintf("NCCI mapping doesn't exist %ld %08lx %02x",
        ncci_mapping_bug, Id, preserve_ncci));
    }
    else
    {
      cleanup_ncci_data (plci, ncci);
      dbug(1,dprintf("NCCI mapping released %ld %08lx %02x %02x-%02x",
        ncci_mapping_bug, Id, preserve_ncci, a->ncci_ch[ncci], ncci));
      a->ch_ncci[a->ncci_ch[ncci]] = 0;
      if (!preserve_ncci)
      {
        a->ncci_ch[ncci] = 0;
        a->ncci_plci[ncci] = 0;
        a->ncci_state[ncci] = IDLE;
        i = plci->ncci_ring_list;
        while ((i != 0) && (a->ncci_next[i] != plci->ncci_ring_list) && (a->ncci_next[i] != ncci))
          i = a->ncci_next[i];
        if ((i != 0) && (a->ncci_next[i] == ncci))
        {
          if (i == ncci)
            plci->ncci_ring_list = 0;
          else if (plci->ncci_ring_list == ncci)
            plci->ncci_ring_list = i;
          a->ncci_next[i] = a->ncci_next[ncci];
        }
        a->ncci_next[ncci] = 0;
      }
    }
  }
  else
  {
    for (ncci = 1; ncci < MAX_NCCI+1; ncci++)
    {
      if (a->ncci_plci[ncci] == plci->Id)
      {
        cleanup_ncci_data (plci, ncci);
        dbug(1,dprintf("NCCI mapping released %ld %08lx %02x %02x-%02x",
          ncci_mapping_bug, Id, preserve_ncci, a->ncci_ch[ncci], ncci));
        a->ch_ncci[a->ncci_ch[ncci]] = 0;
        if (!preserve_ncci)
        {
          a->ncci_ch[ncci] = 0;
          a->ncci_plci[ncci] = 0;
          a->ncci_state[ncci] = IDLE;
          a->ncci_next[ncci] = 0;
        }
      }
    }
    if (!preserve_ncci)
      plci->ncci_ring_list = 0;
  }
}


/*------------------------------------------------------------------*/
/* PLCI remove function                                             */
/*------------------------------------------------------------------*/

static void plci_free_msg_in_queue (PLCI   *plci)
{
  word i;

  if (plci->appl)
  {
    i = plci->msg_in_read_pos;
    while (i != plci->msg_in_write_pos)
    {
      if (i == plci->msg_in_wrap_pos)
        i = 0;
      if (((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[i]))->header.command == _DATA_B3_R)
      {

        TransmitBufferFree (plci->appl,
          (byte *)(long)(((CAPI_MSG *)(&((byte *)(plci->msg_in_queue))[i]))->info.data_b3_req.Data));

      }

      i += (((CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[i]))->header.length +
        MSG_IN_OVERHEAD + 3) & 0xfffc;

    }
  }
  plci->msg_in_write_pos = MSG_IN_QUEUE_SIZE;
  plci->msg_in_read_pos = MSG_IN_QUEUE_SIZE;
  plci->msg_in_wrap_pos = MSG_IN_QUEUE_SIZE;
}


static void plci_remove(PLCI   * plci)
{

  if(!plci) {
    dbug(1,dprintf("plci_remove(no plci)"));
    return;
  }
  init_internal_command_queue (plci);
  dbug(1,dprintf("plci_remove(%x,tel=%x)",plci->Id,plci->tel));
  if(plci_remove_check(plci))
  {
    return;
  }
  if (plci->Sig.Id == 0xff)
  {
    dbug(1,dprintf("D-channel X.25 plci->NL.Id:%0x", plci->NL.Id));
    if (plci->NL.Id && !plci->nl_remove_id)
    {
      nl_req_ncci(plci,REMOVE,0);
      send_req(plci);
    }
  }
  else
  {
    if (!plci->sig_remove_id
     && (plci->Sig.Id
      || (plci->req_in!=plci->req_out)
      || (plci->nl_req || plci->sig_req)))
    {
      sig_req(plci,HANGUP,0);
      send_req(plci);
    }
  }
  ncci_remove (plci, 0, false);
  plci_free_msg_in_queue (plci);

  plci->channels = 0;
  plci->appl = NULL;
  if ((plci->State == INC_CON_PENDING) || (plci->State == INC_CON_ALERT))
    plci->State = OUTG_DIS_PENDING;
}

/*------------------------------------------------------------------*/
/* Application Group function helpers                               */
/*------------------------------------------------------------------*/

static void set_group_ind_mask (PLCI   *plci)
{
  word i;

  for (i = 0; i < C_IND_MASK_DWORDS; i++)
    plci->group_optimization_mask_table[i] = 0xffffffffL;
}

static void clear_group_ind_mask_bit (PLCI   *plci, word b)
{
  plci->group_optimization_mask_table[b >> 5] &= ~(1L << (b & 0x1f));
}

static byte test_group_ind_mask_bit (PLCI   *plci, word b)
{
  return ((plci->group_optimization_mask_table[b >> 5] & (1L << (b & 0x1f))) != 0);
}

/*------------------------------------------------------------------*/
/* c_ind_mask operations for arbitrary MAX_APPL                     */
/*------------------------------------------------------------------*/

static void clear_c_ind_mask (PLCI   *plci)
{
  word i;

  for (i = 0; i < C_IND_MASK_DWORDS; i++)
    plci->c_ind_mask_table[i] = 0;
}

static byte c_ind_mask_empty (PLCI   *plci)
{
  word i;

  i = 0;
  while ((i < C_IND_MASK_DWORDS) && (plci->c_ind_mask_table[i] == 0))
    i++;
  return (i == C_IND_MASK_DWORDS);
}

static void set_c_ind_mask_bit (PLCI   *plci, word b)
{
  plci->c_ind_mask_table[b >> 5] |= (1L << (b & 0x1f));
}

static void clear_c_ind_mask_bit (PLCI   *plci, word b)
{
  plci->c_ind_mask_table[b >> 5] &= ~(1L << (b & 0x1f));
}

static byte test_c_ind_mask_bit (PLCI   *plci, word b)
{
  return ((plci->c_ind_mask_table[b >> 5] & (1L << (b & 0x1f))) != 0);
}

static void dump_c_ind_mask (PLCI   *plci)
{
static char hex_digit_table[0x10] =
  {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
  word i, j, k;
  dword d;
    char *p;
    char buf[40];

  for (i = 0; i < C_IND_MASK_DWORDS; i += 4)
  {
    p = buf + 36;
    *p = '\0';
    for (j = 0; j < 4; j++)
    {
      if (i+j < C_IND_MASK_DWORDS)
      {
        d = plci->c_ind_mask_table[i+j];
        for (k = 0; k < 8; k++)
        {
          *(--p) = hex_digit_table[d & 0xf];
          d >>= 4;
        }
      }
      else if (i != 0)
      {
        for (k = 0; k < 8; k++)
          *(--p) = ' ';
      }
      *(--p) = ' ';
    }
    dbug(1,dprintf ("c_ind_mask =%s", (char   *) p));
  }
}





#define dump_plcis(a)



/*------------------------------------------------------------------*/
/* translation function for each message                            */
/*------------------------------------------------------------------*/

static byte connect_req(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			PLCI *plci, APPL *appl, API_PARSE *parms)
{
  word ch;
  word i;
  word Info;
  word CIP;
  byte LinkLayer;
  API_PARSE * ai;
  API_PARSE * bp;
    API_PARSE ai_parms[5];
  word channel = 0;
  dword ch_mask;
  byte m;
  static byte esc_chi[35] = {0x02,0x18,0x01};
  static byte lli[2] = {0x01,0x00};
  byte noCh = 0;
  word dir = 0;
  byte   *p_chi = "";

  for(i=0;i<5;i++) ai_parms[i].length = 0;

  dbug(1,dprintf("connect_req(%d)",parms->length));
  Info = _WRONG_IDENTIFIER;
  if(a)
  {
    if(a->adapter_disabled)
    {
      dbug(1,dprintf("adapter disabled"));
      Id = ((word)1<<8)|a->Id;
      sendf(appl,_CONNECT_R|CONFIRM,Id,Number,"w",0);
      sendf(appl, _DISCONNECT_I, Id, 0, "w", _L1_ERROR);
      return false;
    }
    Info = _OUT_OF_PLCI;
    if((i=get_plci(a)))
    {
      Info = 0;
      plci = &a->plci[i-1];
      plci->appl = appl;
      plci->call_dir = CALL_DIR_OUT | CALL_DIR_ORIGINATE;
      /* check 'external controller' bit for codec support */
      if(Id & EXT_CONTROLLER)
      {
        if(AdvCodecSupport(a, plci, appl, 0) )
        {
          plci->Id = 0;
          sendf(appl, _CONNECT_R|CONFIRM, Id, Number, "w", _WRONG_IDENTIFIER);
          return 2;
        }
      }
      ai = &parms[9];
      bp = &parms[5];
      ch = 0;
      if(bp->length)LinkLayer = bp->info[3];
      else LinkLayer = 0;
      if(ai->length)
      {
        ch=0xffff;
        if(!api_parse(&ai->info[1],(word)ai->length,"ssss",ai_parms))
        {
          ch = 0;
          if(ai_parms[0].length)
          {
            ch = GET_WORD(ai_parms[0].info+1);
            if(ch>4) ch=0; /* safety -> ignore ChannelID */
            if(ch==4) /* explizit CHI in message */
            {
              /* check length of B-CH struct */
              if((ai_parms[0].info)[3]>=1)
              {
                if((ai_parms[0].info)[4]==CHI)
                {
                  p_chi = &((ai_parms[0].info)[5]);
                }
                else
                {
                  p_chi = &((ai_parms[0].info)[3]);
                }
                if(p_chi[0]>35) /* check length of channel ID */
                {
                  Info = _WRONG_MESSAGE_FORMAT;    
                }
              }
              else Info = _WRONG_MESSAGE_FORMAT;    
            }

            if(ch==3 && ai_parms[0].length>=7 && ai_parms[0].length<=36)
            {
              dir = GET_WORD(ai_parms[0].info+3);
              ch_mask = 0;
              m = 0x3f;
              for(i=0; i+5<=ai_parms[0].length; i++)
              {
                if(ai_parms[0].info[i+5]!=0)
                {
                  if((ai_parms[0].info[i+5] | m) != 0xff)
                    Info = _WRONG_MESSAGE_FORMAT;
                  else
                  {
                    if (ch_mask == 0)
                      channel = i;
                    ch_mask |= 1L << i;
                  }
                }
                m = 0;
              }
              if (ch_mask == 0)
                Info = _WRONG_MESSAGE_FORMAT;
              if (!Info)
              {
                if ((ai_parms[0].length == 36) || (ch_mask != ((dword)(1L << channel))))
                {
                  esc_chi[0] = (byte)(ai_parms[0].length - 2);
                  for(i=0; i+5<=ai_parms[0].length; i++)
                    esc_chi[i+3] = ai_parms[0].info[i+5];
                }
                else
                  esc_chi[0] = 2;
                esc_chi[2] = (byte)channel;
                plci->b_channel = (byte)channel; /* not correct for ETSI ch 17..31 */
                add_p(plci,LLI,lli);
                add_p(plci,ESC,esc_chi);
                plci->State = LOCAL_CONNECT;
                if(!dir) plci->call_dir |= CALL_DIR_FORCE_OUTG_NL;     /* dir 0=DTE, 1=DCE */
              }
            }
          }
        }
        else  Info = _WRONG_MESSAGE_FORMAT;
      }

      dbug(1,dprintf("ch=%x,dir=%x,p_ch=%d",ch,dir,channel));
      plci->command = _CONNECT_R;
      plci->number = Number;
      /* x.31 or D-ch free SAPI in LinkLayer? */
      if(ch==1 && LinkLayer!=3 && LinkLayer!=12) noCh = true;
      if((ch==0 || ch==2 || noCh || ch==3 || ch==4) && !Info)
      {
        /* B-channel used for B3 connections (ch==0), or no B channel    */
        /* is used (ch==2) or perm. connection (3) is used  do a CALL    */
        if(noCh) Info = add_b1(plci,&parms[5],2,0);    /* no resource    */
        else     Info = add_b1(plci,&parms[5],ch,0); 
        add_s(plci,OAD,&parms[2]);
        add_s(plci,OSA,&parms[4]);
        add_s(plci,BC,&parms[6]);
        add_s(plci,LLC,&parms[7]);
        add_s(plci,HLC,&parms[8]);
        CIP = GET_WORD(parms[0].info);
        if (a->Info_Mask[appl->Id-1] & 0x200)
        {
          /* early B3 connect (CIP mask bit 9) no release after a disc */
          add_p(plci,LLI,"\x01\x01");
        }
        if(GET_WORD(parms[0].info)<29) {
          add_p(plci,BC,cip_bc[GET_WORD(parms[0].info)][a->u_law]);
          add_p(plci,HLC,cip_hlc[GET_WORD(parms[0].info)]);
        }
        add_p(plci,UID,"\x06\x43\x61\x70\x69\x32\x30");
        sig_req(plci,ASSIGN,DSIG_ID);
      }
      else if(ch==1) {

        /* D-Channel used for B3 connections */
        plci->Sig.Id = 0xff;
        Info = 0;
      }

      if(!Info && ch!=2 && !noCh ) {
        Info = add_b23(plci,&parms[5]);
        if(!Info) {
          if(!(plci->tel && !plci->adv_nl))nl_req_ncci(plci,ASSIGN,0);
        }
      }

      if(!Info)
      {
        if(ch==0 || ch==2 || ch==3 || noCh || ch==4)
        {
          if(plci->spoofed_msg==SPOOFING_REQUIRED)
          {
            api_save_msg(parms, "wsssssssss", &plci->saved_msg);
            plci->spoofed_msg = CALL_REQ;
            plci->internal_command = BLOCK_PLCI;
            plci->command = 0;
            dbug(1,dprintf("Spoof"));
            send_req(plci);
            return false;
          }
          if(ch==4)add_p(plci,CHI,p_chi);
          add_s(plci,CPN,&parms[1]);
          add_s(plci,DSA,&parms[3]);
          if(noCh) add_p(plci,ESC,"\x02\x18\xfd");  /* D-channel, no B-L3 */
          add_ai(plci,&parms[9]);
          if(!dir)sig_req(plci,CALL_REQ,0);
          else
          {
            plci->command = PERM_LIST_REQ;
            plci->appl = appl;
            sig_req(plci,LISTEN_REQ,0);
            send_req(plci);
            return false;
          }
        }
        send_req(plci);
        return false;
      }
      plci->Id = 0;
    }
  }
  sendf(appl,
        _CONNECT_R|CONFIRM,
        Id,
        Number,
        "w",Info);
  return 2;
}

static byte connect_res(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			PLCI *plci, APPL *appl, API_PARSE *parms)
{
  word i, Info;
  word Reject;
  static byte cau_t[] = {0,0,0x90,0x91,0xac,0x9d,0x86,0xd8,0x9b};
  static byte esc_t[] = {0x03,0x08,0x00,0x00};
  API_PARSE * ai;
    API_PARSE ai_parms[5];
  word ch=0;

  if(!plci) {
    dbug(1,dprintf("connect_res(no plci)"));
    return 0;  /* no plci, no send */
  }

  dbug(1,dprintf("connect_res(State=0x%x)",plci->State));
  for(i=0;i<5;i++) ai_parms[i].length = 0;
  ai = &parms[5];
  dbug(1,dprintf("ai->length=%d",ai->length));

  if(ai->length)
  {
    if(!api_parse(&ai->info[1],(word)ai->length,"ssss",ai_parms))
    {
      dbug(1,dprintf("ai_parms[0].length=%d/0x%x",ai_parms[0].length,GET_WORD(ai_parms[0].info+1)));
      ch = 0;
      if(ai_parms[0].length)
      {
        ch = GET_WORD(ai_parms[0].info+1);
        dbug(1,dprintf("BCH-I=0x%x",ch));
      }
    }
  }

  if(plci->State==INC_CON_CONNECTED_ALERT)
  {
    dbug(1,dprintf("Connected Alert Call_Res"));
    if (a->Info_Mask[appl->Id-1] & 0x200)
    {
    /* early B3 connect (CIP mask bit 9) no release after a disc */
      add_p(plci,LLI,"\x01\x01");
    }
    add_s(plci, CONN_NR, &parms[2]);
    add_s(plci, LLC, &parms[4]);
    add_ai(plci, &parms[5]);
    plci->State = INC_CON_ACCEPT;
    sig_req(plci, CALL_RES,0);
    return 1;
  }
  else if(plci->State==INC_CON_PENDING || plci->State==INC_CON_ALERT) {
    clear_c_ind_mask_bit (plci, (word)(appl->Id-1));
    dump_c_ind_mask (plci);
    Reject = GET_WORD(parms[0].info);
    dbug(1,dprintf("Reject=0x%x",Reject));
    if(Reject) 
    {
      if(c_ind_mask_empty (plci)) 
      {
        if((Reject&0xff00)==0x3400) 
        {
          esc_t[2] = ((byte)(Reject&0x00ff)) | 0x80;
          add_p(plci,ESC,esc_t);
          add_ai(plci, &parms[5]);
          sig_req(plci,REJECT,0);
        }      
        else if(Reject==1 || Reject>9) 
        {
          add_ai(plci, &parms[5]);
          sig_req(plci,HANGUP,0);
        }
        else 
        {
          esc_t[2] = cau_t[(Reject&0x000f)];
          add_p(plci,ESC,esc_t);
          add_ai(plci, &parms[5]);
          sig_req(plci,REJECT,0);
        }
        plci->appl = appl;
      }
      else 
      {
        sendf(appl, _DISCONNECT_I, Id, 0, "w", _OTHER_APPL_CONNECTED);
      }
    }
    else {
      plci->appl = appl;
      if(Id & EXT_CONTROLLER){
        if(AdvCodecSupport(a, plci, appl, 0)){
          dbug(1,dprintf("connect_res(error from AdvCodecSupport)"));
          sig_req(plci,HANGUP,0);
          return 1;
        }
        if(plci->tel == ADV_VOICE && a->AdvCodecPLCI)
        {
          Info = add_b23(plci, &parms[1]);
          if (Info)
          {
            dbug(1,dprintf("connect_res(error from add_b23)"));
            sig_req(plci,HANGUP,0);
            return 1;
          }
          if(plci->adv_nl)
          {
            nl_req_ncci(plci, ASSIGN, 0);
          }
        }
      }
      else
      {
        plci->tel = 0;
        if(ch!=2)
        {
          Info = add_b23(plci, &parms[1]);
          if (Info)
          {
            dbug(1,dprintf("connect_res(error from add_b23 2)"));
            sig_req(plci,HANGUP,0);
            return 1;
          }
        }
        nl_req_ncci(plci, ASSIGN, 0);
      }

      if(plci->spoofed_msg==SPOOFING_REQUIRED)
      {
        api_save_msg(parms, "wsssss", &plci->saved_msg);
        plci->spoofed_msg = CALL_RES;
        plci->internal_command = BLOCK_PLCI;
        plci->command = 0;
        dbug(1,dprintf("Spoof"));
      }
      else
      {
        add_b1 (plci, &parms[1], ch, plci->B1_facilities);
        if (a->Info_Mask[appl->Id-1] & 0x200)
        {
          /* early B3 connect (CIP mask bit 9) no release after a disc */
          add_p(plci,LLI,"\x01\x01");
        }
        add_s(plci, CONN_NR, &parms[2]);
        add_s(plci, LLC, &parms[4]);
        add_ai(plci, &parms[5]);
        plci->State = INC_CON_ACCEPT;
        sig_req(plci, CALL_RES,0);
      }

      for(i=0; i<max_appl; i++) {
        if(test_c_ind_mask_bit (plci, i)) {
          sendf(&application[i], _DISCONNECT_I, Id, 0, "w", _OTHER_APPL_CONNECTED);
        }
      }
    }
  }
  return 1;
}

static byte connect_a_res(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			  PLCI *plci, APPL *appl, API_PARSE *msg)
{
  dbug(1,dprintf("connect_a_res"));
  return false;
}

static byte disconnect_req(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			   PLCI *plci, APPL *appl, API_PARSE *msg)
{
  word Info;
  word i;

  dbug(1,dprintf("disconnect_req"));

  Info = _WRONG_IDENTIFIER;

  if(plci)
  {
    if(plci->State==INC_CON_PENDING || plci->State==INC_CON_ALERT)
    {
      clear_c_ind_mask_bit (plci, (word)(appl->Id-1));
      plci->appl = appl;
      for(i=0; i<max_appl; i++)
      {
        if(test_c_ind_mask_bit (plci, i))
          sendf(&application[i], _DISCONNECT_I, Id, 0, "w", 0);
      }
      plci->State = OUTG_DIS_PENDING;
    }
    if(plci->Sig.Id && plci->appl)
    {
      Info = 0;
        if(plci->Sig.Id!=0xff)
        {
          if(plci->State!=INC_DIS_PENDING)
          {
            add_ai(plci, &msg[0]);
            sig_req(plci,HANGUP,0);
            plci->State = OUTG_DIS_PENDING;
            return 1;
          }
        }
        else
        {
          if (plci->NL.Id && !plci->nl_remove_id)
          {
            mixer_remove (plci);
            nl_req_ncci(plci,REMOVE,0);
          sendf(appl,_DISCONNECT_R|CONFIRM,Id,Number,"w",0);
          sendf(appl, _DISCONNECT_I, Id, 0, "w", 0);
          plci->State = INC_DIS_PENDING;
          }
          return 1;
        }
      }
    }

  if(!appl)  return false;
  sendf(appl, _DISCONNECT_R|CONFIRM, Id, Number, "w",Info);
  return false;
}

static byte disconnect_res(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			   PLCI *plci, APPL *appl, API_PARSE *msg)
{
  dbug(1,dprintf("disconnect_res"));
  if(plci)
  {
        /* clear ind mask bit, just in case of collsion of          */
        /* DISCONNECT_IND and CONNECT_RES                           */
    clear_c_ind_mask_bit (plci, (word)(appl->Id-1));
    ncci_free_receive_buffers (plci, 0);
    if(plci_remove_check(plci))
    {
      return 0;
    }
    if(plci->State==INC_DIS_PENDING
    || plci->State==SUSPENDING) {
      if(c_ind_mask_empty (plci)) {
        if(plci->State!=SUSPENDING)plci->State = IDLE;
        dbug(1,dprintf("chs=%d",plci->channels));
        if(!plci->channels) {
          plci_remove(plci);
        }
      }
    }
  }
  return 0;
}

static byte listen_req(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
		       PLCI *plci, APPL *appl, API_PARSE *parms)
{
  word Info;
  byte i;

  dbug(1,dprintf("listen_req(Appl=0x%x)",appl->Id));

  Info = _WRONG_IDENTIFIER;
  if(a) {
    Info = 0;
    a->Info_Mask[appl->Id-1] = GET_DWORD(parms[0].info);
    a->CIP_Mask[appl->Id-1] = GET_DWORD(parms[1].info);
    dbug(1,dprintf("CIP_MASK=0x%lx",GET_DWORD(parms[1].info)));
    if (a->Info_Mask[appl->Id-1] & 0x200){ /* early B3 connect provides */
      a->Info_Mask[appl->Id-1] |=  0x10;   /* call progression infos    */
    }

    /* check if external controller listen and switch listen on or off*/
    if(Id&EXT_CONTROLLER && GET_DWORD(parms[1].info)){
      if(a->profile.Global_Options & ON_BOARD_CODEC) {
        dummy_plci.State = IDLE;
        a->codec_listen[appl->Id-1] = &dummy_plci;
        a->TelOAD[0] = (byte)(parms[3].length);
        for(i=1;parms[3].length>=i && i<22;i++) {
          a->TelOAD[i] = parms[3].info[i];
        }
        a->TelOAD[i] = 0;
        a->TelOSA[0] = (byte)(parms[4].length);
        for(i=1;parms[4].length>=i && i<22;i++) {
          a->TelOSA[i] = parms[4].info[i];
        }
        a->TelOSA[i] = 0;
      }
      else Info = 0x2002; /* wrong controller, codec not supported */
    }
    else{               /* clear listen */
      a->codec_listen[appl->Id-1] = (PLCI   *)0;
    }
  }
  sendf(appl,
        _LISTEN_R|CONFIRM,
        Id,
        Number,
        "w",Info);

  if (a) listen_check(a);
  return false;
}

static byte info_req(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
		     PLCI *plci, APPL *appl, API_PARSE *msg)
{
  word i;
  API_PARSE * ai;
  PLCI   * rc_plci = NULL;
    API_PARSE ai_parms[5];
  word Info = 0;

  dbug(1,dprintf("info_req"));
  for(i=0;i<5;i++) ai_parms[i].length = 0;

  ai = &msg[1];

  if(ai->length)
  {
    if(api_parse(&ai->info[1],(word)ai->length,"ssss",ai_parms))
    {
      dbug(1,dprintf("AddInfo wrong"));
      Info = _WRONG_MESSAGE_FORMAT;
    }
  }
  if(!a) Info = _WRONG_STATE;

  if(!Info && plci)
  {                /* no fac, with CPN, or KEY */
    rc_plci = plci;
    if(!ai_parms[3].length && plci->State && (msg[0].length || ai_parms[1].length) )
    {
      /* overlap sending option */
      dbug(1,dprintf("OvlSnd"));
      add_s(plci,CPN,&msg[0]);
      add_s(plci,KEY,&ai_parms[1]);
      sig_req(plci,INFO_REQ,0);
      send_req(plci);
      return false;
    }

    if(plci->State && ai_parms[2].length)
    {
      /* User_Info option */
      dbug(1,dprintf("UUI"));
      add_s(plci,UUI,&ai_parms[2]);
      sig_req(plci,USER_DATA,0);
    }
    else if(plci->State && ai_parms[3].length)
    {
      /* Facility option */
      dbug(1,dprintf("FAC"));
      add_s(plci,CPN,&msg[0]);
      add_ai(plci, &msg[1]);
      sig_req(plci,FACILITY_REQ,0);
    }
    else
    {
      Info = _WRONG_STATE;
    }
  }
  else if((ai_parms[1].length || ai_parms[2].length || ai_parms[3].length) && !Info)
  {
    /* NCR_Facility option -> send UUI and Keypad too */
    dbug(1,dprintf("NCR_FAC"));
    if((i=get_plci(a)))
    {
      rc_plci = &a->plci[i-1];
      appl->NullCREnable  = true;
      rc_plci->internal_command = C_NCR_FAC_REQ;
      rc_plci->appl = appl;
      add_p(rc_plci,CAI,"\x01\x80");
      add_p(rc_plci,UID,"\x06\x43\x61\x70\x69\x32\x30");
      sig_req(rc_plci,ASSIGN,DSIG_ID);
      send_req(rc_plci);
    }
    else
    {
      Info = _OUT_OF_PLCI;
    }

    if(!Info)
    {
      add_s(rc_plci,CPN,&msg[0]);
      add_ai(rc_plci, &msg[1]);
      sig_req(rc_plci,NCR_FACILITY,0);
      send_req(rc_plci);
      return false;
     /* for application controlled supplementary services    */
    }
  }

  if (!rc_plci)
  {
    Info = _WRONG_MESSAGE_FORMAT;
  }

  if(!Info)
  {
    send_req(rc_plci);
  }
  else
  {  /* appl is not assigned to a PLCI or error condition */
    dbug(1,dprintf("localInfoCon"));
    sendf(appl,
          _INFO_R|CONFIRM,
          Id,
          Number,
          "w",Info);
  }
  return false;
}

static byte info_res(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
		     PLCI *plci, APPL *appl, API_PARSE *msg)
{
  dbug(1,dprintf("info_res"));
  return false;
}

static byte alert_req(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
		      PLCI *plci, APPL *appl, API_PARSE *msg)
{
  word Info;
  byte ret;

  dbug(1,dprintf("alert_req"));

  Info = _WRONG_IDENTIFIER;
  ret = false;
  if(plci) {
    Info = _ALERT_IGNORED;
    if(plci->State!=INC_CON_ALERT) {
      Info = _WRONG_STATE;
      if(plci->State==INC_CON_PENDING) {
        Info = 0;
        plci->State=INC_CON_ALERT;
        add_ai(plci, &msg[0]);
        sig_req(plci,CALL_ALERT,0);
        ret = 1;
      }
    }
  }
  sendf(appl,
        _ALERT_R|CONFIRM,
        Id,
        Number,
        "w",Info);
  return ret;
}

static byte facility_req(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			 PLCI *plci, APPL *appl, API_PARSE *msg)
{
  word Info = 0;
  word i    = 0;

  word selector;
  word SSreq;
  long relatedPLCIvalue;
  DIVA_CAPI_ADAPTER   * relatedadapter;
  byte * SSparms  = "";
    byte RCparms[]  = "\x05\x00\x00\x02\x00\x00";
    byte SSstruct[] = "\x09\x00\x00\x06\x00\x00\x00\x00\x00\x00";
  API_PARSE * parms;
    API_PARSE ss_parms[11];
  PLCI   *rplci;
    byte cai[15];
  dword d;
    API_PARSE dummy;

  dbug(1,dprintf("facility_req"));
  for(i=0;i<9;i++) ss_parms[i].length = 0;

  parms = &msg[1];

  if(!a)
  {
    dbug(1,dprintf("wrong Ctrl"));
    Info = _WRONG_IDENTIFIER;
  }

  selector = GET_WORD(msg[0].info);

  if(!Info)
  {
    switch(selector)
    {
      case SELECTOR_HANDSET:
        Info = AdvCodecSupport(a, plci, appl, HOOK_SUPPORT);
        break;

      case SELECTOR_SU_SERV:
        if(!msg[1].length)
        {
          Info = _WRONG_MESSAGE_FORMAT;
          break;
        }
        SSreq = GET_WORD(&(msg[1].info[1]));
        PUT_WORD(&RCparms[1],SSreq);
        SSparms = RCparms;
        switch(SSreq)
        {
          case S_GET_SUPPORTED_SERVICES:
            if((i=get_plci(a)))
            {
              rplci = &a->plci[i-1];
              rplci->appl = appl;
              add_p(rplci,CAI,"\x01\x80");
              add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
              sig_req(rplci,ASSIGN,DSIG_ID);
              send_req(rplci);
            }
            else
            {
              PUT_DWORD(&SSstruct[6], MASK_TERMINAL_PORTABILITY);
              SSparms = (byte *)SSstruct;
              break;
            }
            rplci->internal_command = GETSERV_REQ_PEND;
            rplci->number = Number;
            rplci->appl = appl;
            sig_req(rplci,S_SUPPORTED,0);
            send_req(rplci);
            return false;
            break;

          case S_LISTEN:
            if(parms->length==7)
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbd",ss_parms))
              {
                dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
            }
            else
            {
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            a->Notification_Mask[appl->Id-1] = GET_DWORD(ss_parms[2].info);
            if(a->Notification_Mask[appl->Id-1] & SMASK_MWI) /* MWI active? */
            {
              if((i=get_plci(a)))
              {
                rplci = &a->plci[i-1];
                rplci->appl = appl;
                add_p(rplci,CAI,"\x01\x80");
                add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
                sig_req(rplci,ASSIGN,DSIG_ID);
                send_req(rplci);
              }
              else
              {
                break;
              }
              rplci->internal_command = GET_MWI_STATE;
              rplci->number = Number;
              sig_req(rplci,MWI_POLL,0);
              send_req(rplci);
            }
            break;

          case S_HOLD:
            api_parse(&parms->info[1],(word)parms->length,"ws",ss_parms);
            if(plci && plci->State && plci->SuppState==IDLE)
            {
              plci->SuppState = HOLD_REQUEST;
              plci->command = C_HOLD_REQ;
              add_s(plci,CAI,&ss_parms[1]);
              sig_req(plci,CALL_HOLD,0);
              send_req(plci);
              return false;
            }
            else Info = 0x3010;                    /* wrong state           */
            break;
          case S_RETRIEVE:
            if(plci && plci->State && plci->SuppState==CALL_HELD)
            {
              if(Id & EXT_CONTROLLER)
              {
                if(AdvCodecSupport(a, plci, appl, 0))
                {
                  Info = 0x3010;                    /* wrong state           */
                  break;
                }
              }
              else plci->tel = 0;

              plci->SuppState = RETRIEVE_REQUEST;
              plci->command = C_RETRIEVE_REQ;
              if(plci->spoofed_msg==SPOOFING_REQUIRED)
              {
                plci->spoofed_msg = CALL_RETRIEVE;
                plci->internal_command = BLOCK_PLCI;
                plci->command = 0;
                dbug(1,dprintf("Spoof"));
                return false;
              }
              else
              {
                sig_req(plci,CALL_RETRIEVE,0);
                send_req(plci);
                return false;
              }
            }
            else Info = 0x3010;                    /* wrong state           */
            break;
          case S_SUSPEND:
            if(parms->length)
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbs",ss_parms))
              {
                dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
            }
            if(plci && plci->State)
            {
              add_s(plci,CAI,&ss_parms[2]);
              plci->command = SUSPEND_REQ;
              sig_req(plci,SUSPEND,0);
              plci->State = SUSPENDING;
              send_req(plci);
            }
            else Info = 0x3010;                    /* wrong state           */
            break;

          case S_RESUME:
            if(!(i=get_plci(a)) )
            {
              Info = _OUT_OF_PLCI;
              break;
            }
            rplci = &a->plci[i-1];
            rplci->appl = appl;
            rplci->number = Number;
            rplci->tel = 0;
            rplci->call_dir = CALL_DIR_OUT | CALL_DIR_ORIGINATE;
            /* check 'external controller' bit for codec support */
            if(Id & EXT_CONTROLLER)
            {
              if(AdvCodecSupport(a, rplci, appl, 0) )
              {
                rplci->Id = 0;
                Info = 0x300A;
                break;
              }
            }
            if(parms->length)
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbs",ss_parms))
              {
                dbug(1,dprintf("format wrong"));
                rplci->Id = 0;
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
            }
            dummy.length = 0;
            dummy.info = "\x00";
            add_b1(rplci, &dummy, 0, 0);
            if (a->Info_Mask[appl->Id-1] & 0x200)
            {
              /* early B3 connect (CIP mask bit 9) no release after a disc */
              add_p(rplci,LLI,"\x01\x01");
            }
            add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
            sig_req(rplci,ASSIGN,DSIG_ID);
            send_req(rplci);
            add_s(rplci,CAI,&ss_parms[2]);
            rplci->command = RESUME_REQ;
            sig_req(rplci,RESUME,0);
            rplci->State = RESUMING;
            send_req(rplci);
            break;

          case S_CONF_BEGIN: /* Request */
          case S_CONF_DROP:
          case S_CONF_ISOLATE:
          case S_CONF_REATTACH:
            if(api_parse(&parms->info[1],(word)parms->length,"wbd",ss_parms))
            {
              dbug(1,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if(plci && plci->State && ((plci->SuppState==IDLE)||(plci->SuppState==CALL_HELD)))
            {
              d = GET_DWORD(ss_parms[2].info);     
              if(d>=0x80)
              {
                dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
              plci->ptyState = (byte)SSreq;
              plci->command = 0;
              cai[0] = 2;
              switch(SSreq)
              {
              case S_CONF_BEGIN:
                  cai[1] = CONF_BEGIN;
                  plci->internal_command = CONF_BEGIN_REQ_PEND;
                  break;
              case S_CONF_DROP:
                  cai[1] = CONF_DROP;
                  plci->internal_command = CONF_DROP_REQ_PEND;
                  break;
              case S_CONF_ISOLATE:
                  cai[1] = CONF_ISOLATE;
                  plci->internal_command = CONF_ISOLATE_REQ_PEND;
                  break;
              case S_CONF_REATTACH:
                  cai[1] = CONF_REATTACH;
                  plci->internal_command = CONF_REATTACH_REQ_PEND;
                  break;
              }
              cai[2] = (byte)d; /* Conference Size resp. PartyId */
              add_p(plci,CAI,cai);
              sig_req(plci,S_SERVICE,0);
              send_req(plci);
              return false;
            }
            else Info = 0x3010;                    /* wrong state           */
            break;

          case S_ECT:
          case S_3PTY_BEGIN:
          case S_3PTY_END:
          case S_CONF_ADD:
            if(parms->length==7)
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbd",ss_parms))
              {
                dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
            }
            else if(parms->length==8) /* workaround for the T-View-S */
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbdb",ss_parms))
              {
                dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
            }
            else
            {
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if(!msg[1].length)
            {
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if (!plci)
            {
              Info = _WRONG_IDENTIFIER;
              break;
            }
            relatedPLCIvalue = GET_DWORD(ss_parms[2].info);
            relatedPLCIvalue &= 0x0000FFFF;
            dbug(1,dprintf("PTY/ECT/addCONF,relPLCI=%lx",relatedPLCIvalue));
            /* controller starts with 0 up to (max_adapter - 1) */
            if (((relatedPLCIvalue & 0x7f) == 0)
             || (MapController ((byte)(relatedPLCIvalue & 0x7f)) == 0)
             || (MapController ((byte)(relatedPLCIvalue & 0x7f)) > max_adapter))
            {
              if(SSreq==S_3PTY_END)
              {
                dbug(1, dprintf("wrong Controller use 2nd PLCI=PLCI"));
                rplci = plci;
              }
              else
              {
                Info = 0x3010;                    /* wrong state           */
                break;
              }
            }
            else
            {  
              relatedadapter = &adapter[MapController ((byte)(relatedPLCIvalue & 0x7f))-1];
              relatedPLCIvalue >>=8;
              /* find PLCI PTR*/
              for(i=0,rplci=NULL;i<relatedadapter->max_plci;i++)
              {
                if(relatedadapter->plci[i].Id == (byte)relatedPLCIvalue)
                {
                  rplci = &relatedadapter->plci[i];
                }
              }
              if(!rplci || !relatedPLCIvalue)
              {
                if(SSreq==S_3PTY_END)
                {
                  dbug(1, dprintf("use 2nd PLCI=PLCI"));
                  rplci = plci;
                }
                else
                {
                  Info = 0x3010;                    /* wrong state           */
                  break;
                }
              }
            }
/*
            dbug(1,dprintf("rplci:%x",rplci));
            dbug(1,dprintf("plci:%x",plci));
            dbug(1,dprintf("rplci->ptyState:%x",rplci->ptyState));
            dbug(1,dprintf("plci->ptyState:%x",plci->ptyState));
            dbug(1,dprintf("SSreq:%x",SSreq));
            dbug(1,dprintf("rplci->internal_command:%x",rplci->internal_command));
            dbug(1,dprintf("rplci->appl:%x",rplci->appl));
            dbug(1,dprintf("rplci->Id:%x",rplci->Id));
*/
            /* send PTY/ECT req, cannot check all states because of US stuff */
            if( !rplci->internal_command && rplci->appl )
            {
              plci->command = 0;
              rplci->relatedPTYPLCI = plci;
              plci->relatedPTYPLCI = rplci;
              rplci->ptyState = (byte)SSreq;
              if(SSreq==S_ECT)
              {
                rplci->internal_command = ECT_REQ_PEND;
                cai[1] = ECT_EXECUTE;

                rplci->vswitchstate=0;
                rplci->vsprot=0;
                rplci->vsprotdialect=0;
                plci->vswitchstate=0;
                plci->vsprot=0;
                plci->vsprotdialect=0;

              }
              else if(SSreq==S_CONF_ADD)
              {
                rplci->internal_command = CONF_ADD_REQ_PEND;
                cai[1] = CONF_ADD;
              }
              else
              {
                rplci->internal_command = PTY_REQ_PEND;
                cai[1] = (byte)(SSreq-3);
              }
              rplci->number = Number;
              if(plci!=rplci) /* explicit invocation */
              {
                cai[0] = 2;
                cai[2] = plci->Sig.Id;
                dbug(1,dprintf("explicit invocation"));
              }
              else
              {
                dbug(1,dprintf("implicit invocation"));
                cai[0] = 1;
              }
              add_p(rplci,CAI,cai);
              sig_req(rplci,S_SERVICE,0);
              send_req(rplci);
              return false;
            }
            else
            {
              dbug(0,dprintf("Wrong line"));
              Info = 0x3010;                    /* wrong state           */
              break;
            }
            break;

          case S_CALL_DEFLECTION:
            if(api_parse(&parms->info[1],(word)parms->length,"wbwss",ss_parms))
            {
              dbug(1,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if (!plci)
            {
              Info = _WRONG_IDENTIFIER;
              break;
            }
            /* reuse unused screening indicator */
            ss_parms[3].info[3] = (byte)GET_WORD(&(ss_parms[2].info[0]));
            plci->command = 0;
            plci->internal_command = CD_REQ_PEND;
            appl->CDEnable = true;
            cai[0] = 1;
            cai[1] = CALL_DEFLECTION;
            add_p(plci,CAI,cai);
            add_p(plci,CPN,ss_parms[3].info);
            sig_req(plci,S_SERVICE,0);
            send_req(plci);
            return false;
            break;

          case S_CALL_FORWARDING_START:
            if(api_parse(&parms->info[1],(word)parms->length,"wbdwwsss",ss_parms))
            {
              dbug(1,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }

            if((i=get_plci(a)))
            {
              rplci = &a->plci[i-1];
              rplci->appl = appl;
              add_p(rplci,CAI,"\x01\x80");
              add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
              sig_req(rplci,ASSIGN,DSIG_ID);
              send_req(rplci);
            }
            else
            {
              Info = _OUT_OF_PLCI;
              break;
            }

            /* reuse unused screening indicator */
            rplci->internal_command = CF_START_PEND;
            rplci->appl = appl;
            rplci->number = Number;
            appl->S_Handle = GET_DWORD(&(ss_parms[2].info[0]));
            cai[0] = 2;
            cai[1] = 0x70|(byte)GET_WORD(&(ss_parms[3].info[0])); /* Function */
            cai[2] = (byte)GET_WORD(&(ss_parms[4].info[0])); /* Basic Service */
            add_p(rplci,CAI,cai);
            add_p(rplci,OAD,ss_parms[5].info);
            add_p(rplci,CPN,ss_parms[6].info);
            sig_req(rplci,S_SERVICE,0);
            send_req(rplci);
            return false;
            break;

          case S_INTERROGATE_DIVERSION:
          case S_INTERROGATE_NUMBERS:
          case S_CALL_FORWARDING_STOP:
          case S_CCBS_REQUEST:
          case S_CCBS_DEACTIVATE:
          case S_CCBS_INTERROGATE:
            switch(SSreq)
            {
            case S_INTERROGATE_NUMBERS:
                if(api_parse(&parms->info[1],(word)parms->length,"wbd",ss_parms))
                {
                  dbug(0,dprintf("format wrong"));
                  Info = _WRONG_MESSAGE_FORMAT;
                }
                break;
            case S_CCBS_REQUEST:
            case S_CCBS_DEACTIVATE:
                if(api_parse(&parms->info[1],(word)parms->length,"wbdw",ss_parms))
                {
                  dbug(0,dprintf("format wrong"));
                  Info = _WRONG_MESSAGE_FORMAT;
                }
                break;
            case S_CCBS_INTERROGATE:
                if(api_parse(&parms->info[1],(word)parms->length,"wbdws",ss_parms))
                {
                  dbug(0,dprintf("format wrong"));
                  Info = _WRONG_MESSAGE_FORMAT;
                }
                break;
            default:
            if(api_parse(&parms->info[1],(word)parms->length,"wbdwws",ss_parms))
            {
              dbug(0,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
                break;
            }

            if(Info) break;
            if((i=get_plci(a)))
            {
              rplci = &a->plci[i-1];
              switch(SSreq)
              {
                case S_INTERROGATE_DIVERSION: /* use cai with S_SERVICE below */
                  cai[1] = 0x60|(byte)GET_WORD(&(ss_parms[3].info[0])); /* Function */
                  rplci->internal_command = INTERR_DIVERSION_REQ_PEND; /* move to rplci if assigned */
                  break;
                case S_INTERROGATE_NUMBERS: /* use cai with S_SERVICE below */
                  cai[1] = DIVERSION_INTERROGATE_NUM; /* Function */
                  rplci->internal_command = INTERR_NUMBERS_REQ_PEND; /* move to rplci if assigned */
                  break;
                case S_CALL_FORWARDING_STOP:
                  rplci->internal_command = CF_STOP_PEND;
                  cai[1] = 0x80|(byte)GET_WORD(&(ss_parms[3].info[0])); /* Function */
                  break;
                case S_CCBS_REQUEST:
                  cai[1] = CCBS_REQUEST;
                  rplci->internal_command = CCBS_REQUEST_REQ_PEND;
                  break;
                case S_CCBS_DEACTIVATE:
                  cai[1] = CCBS_DEACTIVATE;
                  rplci->internal_command = CCBS_DEACTIVATE_REQ_PEND;
                  break;
                case S_CCBS_INTERROGATE:
                  cai[1] = CCBS_INTERROGATE;
                  rplci->internal_command = CCBS_INTERROGATE_REQ_PEND;
                  break;
                default:
                  cai[1] = 0;
                break;
              }
              rplci->appl = appl;
              rplci->number = Number;
              add_p(rplci,CAI,"\x01\x80");
              add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
              sig_req(rplci,ASSIGN,DSIG_ID);
              send_req(rplci);
            }
            else
            {
              Info = _OUT_OF_PLCI;
              break;
            }

            appl->S_Handle = GET_DWORD(&(ss_parms[2].info[0]));
            switch(SSreq)
            {
            case S_INTERROGATE_NUMBERS:
                cai[0] = 1;
                add_p(rplci,CAI,cai);
                break;
            case S_CCBS_REQUEST:
            case S_CCBS_DEACTIVATE:
                cai[0] = 3;
                PUT_WORD(&cai[2],GET_WORD(&(ss_parms[3].info[0])));
                add_p(rplci,CAI,cai);
                break;
            case S_CCBS_INTERROGATE:
                cai[0] = 3;
                PUT_WORD(&cai[2],GET_WORD(&(ss_parms[3].info[0])));
                add_p(rplci,CAI,cai);
                add_p(rplci,OAD,ss_parms[4].info);
                break;
            default:
            cai[0] = 2;
            cai[2] = (byte)GET_WORD(&(ss_parms[4].info[0])); /* Basic Service */
            add_p(rplci,CAI,cai);
            add_p(rplci,OAD,ss_parms[5].info);
                break;
            }
                        
            sig_req(rplci,S_SERVICE,0);
            send_req(rplci);
            return false;
            break;

          case S_MWI_ACTIVATE:
            if(api_parse(&parms->info[1],(word)parms->length,"wbwdwwwssss",ss_parms))
            {
              dbug(1,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if(!plci)
            {                               
              if((i=get_plci(a)))
              {
                rplci = &a->plci[i-1];
                rplci->appl = appl;
                rplci->cr_enquiry=true;
                add_p(rplci,CAI,"\x01\x80");
                add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
                sig_req(rplci,ASSIGN,DSIG_ID);
                send_req(rplci);
              }
              else
              {
                Info = _OUT_OF_PLCI;
                break;
              }
            }
            else
            {
              rplci = plci;
              rplci->cr_enquiry=false;
            }

            rplci->command = 0;
            rplci->internal_command = MWI_ACTIVATE_REQ_PEND;
            rplci->appl = appl;
            rplci->number = Number;

            cai[0] = 13;
            cai[1] = ACTIVATION_MWI; /* Function */
            PUT_WORD(&cai[2],GET_WORD(&(ss_parms[2].info[0]))); /* Basic Service */
            PUT_DWORD(&cai[4],GET_DWORD(&(ss_parms[3].info[0]))); /* Number of Messages */
            PUT_WORD(&cai[8],GET_WORD(&(ss_parms[4].info[0]))); /* Message Status */
            PUT_WORD(&cai[10],GET_WORD(&(ss_parms[5].info[0]))); /* Message Reference */
            PUT_WORD(&cai[12],GET_WORD(&(ss_parms[6].info[0]))); /* Invocation Mode */
            add_p(rplci,CAI,cai);
            add_p(rplci,CPN,ss_parms[7].info); /* Receiving User Number */
            add_p(rplci,OAD,ss_parms[8].info); /* Controlling User Number */
            add_p(rplci,OSA,ss_parms[9].info); /* Controlling User Provided Number */
            add_p(rplci,UID,ss_parms[10].info); /* Time */
            sig_req(rplci,S_SERVICE,0);
            send_req(rplci);
            return false;

          case S_MWI_DEACTIVATE:
            if(api_parse(&parms->info[1],(word)parms->length,"wbwwss",ss_parms))
            {
              dbug(1,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if(!plci)
            {                               
              if((i=get_plci(a)))
              {
                rplci = &a->plci[i-1];
                rplci->appl = appl;
                rplci->cr_enquiry=true;
                add_p(rplci,CAI,"\x01\x80");
                add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
                sig_req(rplci,ASSIGN,DSIG_ID);
                send_req(rplci);
              }
              else
              {
                Info = _OUT_OF_PLCI;
                break;
              }
            }
            else
            {
              rplci = plci;
              rplci->cr_enquiry=false;
            }

            rplci->command = 0;
            rplci->internal_command = MWI_DEACTIVATE_REQ_PEND;
            rplci->appl = appl;
            rplci->number = Number;

            cai[0] = 5;
            cai[1] = DEACTIVATION_MWI; /* Function */
            PUT_WORD(&cai[2],GET_WORD(&(ss_parms[2].info[0]))); /* Basic Service */
            PUT_WORD(&cai[4],GET_WORD(&(ss_parms[3].info[0]))); /* Invocation Mode */
            add_p(rplci,CAI,cai);
            add_p(rplci,CPN,ss_parms[4].info); /* Receiving User Number */
            add_p(rplci,OAD,ss_parms[5].info); /* Controlling User Number */
            sig_req(rplci,S_SERVICE,0);
            send_req(rplci);
            return false;

          default:
            Info = 0x300E;  /* not supported */
            break;
        }
        break; /* case SELECTOR_SU_SERV: end */


      case SELECTOR_DTMF:
        return (dtmf_request (Id, Number, a, plci, appl, msg));



      case SELECTOR_LINE_INTERCONNECT:
        return (mixer_request (Id, Number, a, plci, appl, msg));



      case PRIV_SELECTOR_ECHO_CANCELLER:
        appl->appl_flags |= APPL_FLAG_PRIV_EC_SPEC;
        return (ec_request (Id, Number, a, plci, appl, msg));

      case SELECTOR_ECHO_CANCELLER:
        appl->appl_flags &= ~APPL_FLAG_PRIV_EC_SPEC;
        return (ec_request (Id, Number, a, plci, appl, msg));


      case SELECTOR_V42BIS:
      default:
        Info = _FACILITY_NOT_SUPPORTED;
        break;
    } /* end of switch(selector) */
  }

  dbug(1,dprintf("SendFacRc"));
  sendf(appl,
        _FACILITY_R|CONFIRM,
        Id,
        Number,
        "wws",Info,selector,SSparms);
  return false;
}

static byte facility_res(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			 PLCI *plci, APPL *appl, API_PARSE *msg)
{
  dbug(1,dprintf("facility_res"));
  return false;
}

static byte connect_b3_req(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			   PLCI *plci, APPL *appl, API_PARSE *parms)
{
  word Info = 0;
  byte req;
  byte len;
  word w;
  word fax_control_bits, fax_feature_bits, fax_info_change;
  API_PARSE * ncpi;
    byte pvc[2];

    API_PARSE fax_parms[9];
  word i;


  dbug(1,dprintf("connect_b3_req"));
  if(plci)
  {
    if ((plci->State == IDLE) || (plci->State == OUTG_DIS_PENDING)
     || (plci->State == INC_DIS_PENDING) || (plci->SuppState != IDLE))
    {
      Info = _WRONG_STATE;
    }
    else
    {
      /* local reply if assign unsuccessfull
         or B3 protocol allows only one layer 3 connection
           and already connected
             or B2 protocol not any LAPD
               and connect_b3_req contradicts originate/answer direction */
      if (!plci->NL.Id
       || (((plci->B3_prot != B3_T90NL) && (plci->B3_prot != B3_ISO8208) && (plci->B3_prot != B3_X25_DCE))
        && ((plci->channels != 0)
         || (((plci->B2_prot != B2_SDLC) && (plci->B2_prot != B2_LAPD) && (plci->B2_prot != B2_LAPD_FREE_SAPI_SEL))
          && ((plci->call_dir & CALL_DIR_ANSWER) && !(plci->call_dir & CALL_DIR_FORCE_OUTG_NL))))))
      {
        dbug(1,dprintf("B3 already connected=%d or no NL.Id=0x%x, dir=%d sstate=0x%x",
                       plci->channels,plci->NL.Id,plci->call_dir,plci->SuppState));
        Info = _WRONG_STATE;
        sendf(appl,                                                        
              _CONNECT_B3_R|CONFIRM,
              Id,
              Number,
              "w",Info);
        return false;
      }
      plci->requested_options_conn = 0;

      req = N_CONNECT;
      ncpi = &parms[0];
      if(plci->B3_prot==2 || plci->B3_prot==3)
      {
        if(ncpi->length>2)
        {
          /* check for PVC */
          if(ncpi->info[2] || ncpi->info[3])
          {
            pvc[0] = ncpi->info[3];
            pvc[1] = ncpi->info[2];
            add_d(plci,2,pvc);
            req = N_RESET;
          }
          else
          {
            if(ncpi->info[1] &1) req = N_CONNECT | N_D_BIT;
            add_d(plci,(word)(ncpi->length-3),&ncpi->info[4]);
          }
        }
      }
      else if(plci->B3_prot==5)
      {
        if (plci->NL.Id && !plci->nl_remove_id)
        {
          fax_control_bits = GET_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low);
          fax_feature_bits = GET_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->feature_bits_low);
          if (!(fax_control_bits & T30_CONTROL_BIT_MORE_DOCUMENTS)
           || (fax_feature_bits & T30_FEATURE_BIT_MORE_DOCUMENTS))
          {
            len = offsetof(T30_INFO, universal_6);
            fax_info_change = false;
            if (ncpi->length >= 4)
            {
              w = GET_WORD(&ncpi->info[3]);
              if ((w & 0x0001) != ((word)(((T30_INFO   *)(plci->fax_connect_info_buffer))->resolution & 0x0001)))
              {
                ((T30_INFO   *)(plci->fax_connect_info_buffer))->resolution =
                  (byte)((((T30_INFO   *)(plci->fax_connect_info_buffer))->resolution & ~T30_RESOLUTION_R8_0770_OR_200) |
                  ((w & 0x0001) ? T30_RESOLUTION_R8_0770_OR_200 : 0));
                fax_info_change = true;
              }
              fax_control_bits &= ~(T30_CONTROL_BIT_REQUEST_POLLING | T30_CONTROL_BIT_MORE_DOCUMENTS);
              if (w & 0x0002)  /* Fax-polling request */
                fax_control_bits |= T30_CONTROL_BIT_REQUEST_POLLING;
              if ((w & 0x0004) /* Request to send / poll another document */
               && (a->manufacturer_features & MANUFACTURER_FEATURE_FAX_MORE_DOCUMENTS))
              {
                fax_control_bits |= T30_CONTROL_BIT_MORE_DOCUMENTS;
              }
              if (ncpi->length >= 6)
              {
                w = GET_WORD(&ncpi->info[5]);
                if (((byte) w) != ((T30_INFO   *)(plci->fax_connect_info_buffer))->data_format)
                {
                  ((T30_INFO   *)(plci->fax_connect_info_buffer))->data_format = (byte) w;
                  fax_info_change = true;
                }

                if ((a->man_profile.private_options & (1L << PRIVATE_FAX_SUB_SEP_PWD))
                 && (GET_WORD(&ncpi->info[5]) & 0x8000)) /* Private SEP/SUB/PWD enable */
                {
                  plci->requested_options_conn |= (1L << PRIVATE_FAX_SUB_SEP_PWD);
                }
                if ((a->man_profile.private_options & (1L << PRIVATE_FAX_NONSTANDARD))
                 && (GET_WORD(&ncpi->info[5]) & 0x4000)) /* Private non-standard facilities enable */
                {
                  plci->requested_options_conn |= (1L << PRIVATE_FAX_NONSTANDARD);
                }
                fax_control_bits &= ~(T30_CONTROL_BIT_ACCEPT_SUBADDRESS | T30_CONTROL_BIT_ACCEPT_SEL_POLLING |
                  T30_CONTROL_BIT_ACCEPT_PASSWORD);
                if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[appl->Id-1])
                  & ((1L << PRIVATE_FAX_SUB_SEP_PWD) | (1L << PRIVATE_FAX_NONSTANDARD)))
                {
                  if (api_parse (&ncpi->info[1], ncpi->length, "wwwwsss", fax_parms))
                    Info = _WRONG_MESSAGE_FORMAT;
                  else
                  {
                    if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[appl->Id-1])
                      & (1L << PRIVATE_FAX_SUB_SEP_PWD))
      {
                    fax_control_bits |= T30_CONTROL_BIT_ACCEPT_SUBADDRESS | T30_CONTROL_BIT_ACCEPT_PASSWORD;
                    if (fax_control_bits & T30_CONTROL_BIT_ACCEPT_POLLING)
                      fax_control_bits |= T30_CONTROL_BIT_ACCEPT_SEL_POLLING;
      }
                    w = fax_parms[4].length;
                    if (w > 20)
                      w = 20;
                    ((T30_INFO   *)(plci->fax_connect_info_buffer))->station_id_len = (byte) w;
                    for (i = 0; i < w; i++)
                      ((T30_INFO   *)(plci->fax_connect_info_buffer))->station_id[i] = fax_parms[4].info[1+i];
                    ((T30_INFO   *)(plci->fax_connect_info_buffer))->head_line_len = 0;
                    len = offsetof(T30_INFO, station_id) + 20;
                    w = fax_parms[5].length;
                    if (w > 20)
                      w = 20;
                    plci->fax_connect_info_buffer[len++] = (byte) w;
                    for (i = 0; i < w; i++)
                      plci->fax_connect_info_buffer[len++] = fax_parms[5].info[1+i];
                    w = fax_parms[6].length;
                    if (w > 20)
                      w = 20;
                    plci->fax_connect_info_buffer[len++] = (byte) w;
                    for (i = 0; i < w; i++)
                      plci->fax_connect_info_buffer[len++] = fax_parms[6].info[1+i];
                    if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[appl->Id-1])
                      & (1L << PRIVATE_FAX_NONSTANDARD))
      {
                      if (api_parse (&ncpi->info[1], ncpi->length, "wwwwssss", fax_parms))
        {
                        dbug(1,dprintf("non-standard facilities info missing or wrong format"));
                        plci->fax_connect_info_buffer[len++] = 0;
        }
                      else
                      {
          if ((fax_parms[7].length >= 3) && (fax_parms[7].info[1] >= 2))
            plci->nsf_control_bits = GET_WORD(&fax_parms[7].info[2]);
   plci->fax_connect_info_buffer[len++] = (byte)(fax_parms[7].length);
          for (i = 0; i < fax_parms[7].length; i++)
     plci->fax_connect_info_buffer[len++] = fax_parms[7].info[1+i];
                      }
                    }
                  }
                }
                else
                {
                  len = offsetof(T30_INFO, universal_6);
                }
                fax_info_change = true;

              }
              if (fax_control_bits != GET_WORD(&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low))
              {
                PUT_WORD (&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low, fax_control_bits);
                fax_info_change = true;
              }
            }
            if (Info == GOOD)
            {
              plci->fax_connect_info_length = len;
              if (fax_info_change)
              {
                if (fax_feature_bits & T30_FEATURE_BIT_MORE_DOCUMENTS)
                {
                  start_internal_command (Id, plci, fax_connect_info_command);
                  return false;
                }
                else
                {
                  start_internal_command (Id, plci, fax_adjust_b23_command);
                  return false;
                }
              }
            }
          }
          else  Info = _WRONG_STATE;
        }
        else  Info = _WRONG_STATE;
      }

      else if (plci->B3_prot == B3_RTP)
      {
        plci->internal_req_buffer[0] = ncpi->length + 1;
        plci->internal_req_buffer[1] = UDATA_REQUEST_RTP_RECONFIGURE;
        for (w = 0; w < ncpi->length; w++)
          plci->internal_req_buffer[2+w] = ncpi->info[1+w];
        start_internal_command (Id, plci, rtp_connect_b3_req_command);
        return false;
      }

      if(!Info)
      {
        nl_req_ncci(plci,req,0);
        return 1;
      }
    }
  }
  else Info = _WRONG_IDENTIFIER;

  sendf(appl,
        _CONNECT_B3_R|CONFIRM,
        Id,
        Number,
        "w",Info);
  return false;
}

static byte connect_b3_res(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			   PLCI *plci, APPL *appl, API_PARSE *parms)
{
  word ncci;
  API_PARSE * ncpi;
  byte req;

  word w;


    API_PARSE fax_parms[9];
  word i;
  byte len;


  dbug(1,dprintf("connect_b3_res"));

  ncci = (word)(Id>>16);
  if(plci && ncci) {
    if(a->ncci_state[ncci]==INC_CON_PENDING) {
      if (GET_WORD (&parms[0].info[0]) != 0)
      {
        a->ncci_state[ncci] = OUTG_REJ_PENDING;
        channel_request_xon (plci, a->ncci_ch[ncci]);
        channel_xmit_xon (plci);
        cleanup_ncci_data (plci, ncci);
        nl_req_ncci(plci,N_DISC,(byte)ncci);
        return 1;
      }
      a->ncci_state[ncci] = INC_ACT_PENDING;

      req = N_CONNECT_ACK;
      ncpi = &parms[1];
      if ((plci->B3_prot == 4) || (plci->B3_prot == 5) || (plci->B3_prot == 7))
      {

        if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[plci->appl->Id-1])
          & (1L << PRIVATE_FAX_NONSTANDARD))
 {
   if (((plci->B3_prot == 4) || (plci->B3_prot == 5))
    && (plci->nsf_control_bits & T30_NSF_CONTROL_BIT_ENABLE_NSF)
    && (plci->nsf_control_bits & T30_NSF_CONTROL_BIT_NEGOTIATE_RESP))
   {
            len = offsetof(T30_INFO, station_id) + 20;
            if (plci->fax_connect_info_length < len)
            {
              ((T30_INFO *)(plci->fax_connect_info_buffer))->station_id_len = 0;
              ((T30_INFO *)(plci->fax_connect_info_buffer))->head_line_len = 0;
            }
            if (api_parse (&ncpi->info[1], ncpi->length, "wwwwssss", fax_parms))
            {
              dbug(1,dprintf("non-standard facilities info missing or wrong format"));
            }
            else
            {
              if (plci->fax_connect_info_length <= len)
                plci->fax_connect_info_buffer[len] = 0;
              len += 1 + plci->fax_connect_info_buffer[len];
              if (plci->fax_connect_info_length <= len)
                plci->fax_connect_info_buffer[len] = 0;
              len += 1 + plci->fax_connect_info_buffer[len];
              if ((fax_parms[7].length >= 3) && (fax_parms[7].info[1] >= 2))
                plci->nsf_control_bits = GET_WORD(&fax_parms[7].info[2]);
              plci->fax_connect_info_buffer[len++] = (byte)(fax_parms[7].length);
              for (i = 0; i < fax_parms[7].length; i++)
                plci->fax_connect_info_buffer[len++] = fax_parms[7].info[1+i];
            }
            plci->fax_connect_info_length = len;
            ((T30_INFO *)(plci->fax_connect_info_buffer))->code = 0;
            start_internal_command (Id, plci, fax_connect_ack_command);
     return false;
          }
        }

        nl_req_ncci(plci,req,(byte)ncci);
        if ((plci->ncpi_state & NCPI_VALID_CONNECT_B3_ACT)
         && !(plci->ncpi_state & NCPI_CONNECT_B3_ACT_SENT))
        {
          if (plci->B3_prot == 4)
            sendf(appl,_CONNECT_B3_ACTIVE_I,Id,0,"s","");
          else
            sendf(appl,_CONNECT_B3_ACTIVE_I,Id,0,"S",plci->ncpi_buffer);
          plci->ncpi_state |= NCPI_CONNECT_B3_ACT_SENT;
        }
      }

      else if (plci->B3_prot == B3_RTP)
      {
        plci->internal_req_buffer[0] = ncpi->length + 1;
        plci->internal_req_buffer[1] = UDATA_REQUEST_RTP_RECONFIGURE;
        for (w = 0; w < ncpi->length; w++)
          plci->internal_req_buffer[2+w] = ncpi->info[1+w];
        start_internal_command (Id, plci, rtp_connect_b3_res_command);
        return false;
      }

      else
      {
        if(ncpi->length>2) {
          if(ncpi->info[1] &1) req = N_CONNECT_ACK | N_D_BIT;
          add_d(plci,(word)(ncpi->length-3),&ncpi->info[4]);
        }
        nl_req_ncci(plci,req,(byte)ncci);
        sendf(appl,_CONNECT_B3_ACTIVE_I,Id,0,"s","");
        if (plci->adjust_b_restore)
        {
          plci->adjust_b_restore = false;
          start_internal_command (Id, plci, adjust_b_restore);
        }
      }
      return 1;
    }
  }
  return false;
}

static byte connect_b3_a_res(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			     PLCI *plci, APPL *appl, API_PARSE *parms)
{
  word ncci;

  ncci = (word)(Id>>16);
  dbug(1,dprintf("connect_b3_a_res(ncci=0x%x)",ncci));

  if (plci && ncci && (plci->State != IDLE) && (plci->State != INC_DIS_PENDING)
   && (plci->State != OUTG_DIS_PENDING))
  {
    if(a->ncci_state[ncci]==INC_ACT_PENDING) {
      a->ncci_state[ncci] = CONNECTED;
      if(plci->State!=INC_CON_CONNECTED_ALERT) plci->State = CONNECTED;
      channel_request_xon (plci, a->ncci_ch[ncci]);
      channel_xmit_xon (plci);
    }
  }
  return false;
}

static byte disconnect_b3_req(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			      PLCI *plci, APPL *appl, API_PARSE *parms)
{
  word Info;
  word ncci;
  API_PARSE * ncpi;

  dbug(1,dprintf("disconnect_b3_req"));

  Info = _WRONG_IDENTIFIER;
  ncci = (word)(Id>>16);
  if (plci && ncci)
  {
    Info = _WRONG_STATE;
    if ((a->ncci_state[ncci] == CONNECTED)
     || (a->ncci_state[ncci] == OUTG_CON_PENDING)
     || (a->ncci_state[ncci] == INC_CON_PENDING)
     || (a->ncci_state[ncci] == INC_ACT_PENDING))
    {
      a->ncci_state[ncci] = OUTG_DIS_PENDING;
      channel_request_xon (plci, a->ncci_ch[ncci]);
      channel_xmit_xon (plci);

      if (a->ncci[ncci].data_pending
       && ((plci->B3_prot == B3_TRANSPARENT)
        || (plci->B3_prot == B3_T30)
        || (plci->B3_prot == B3_T30_WITH_EXTENSIONS)))
      {
        plci->send_disc = (byte)ncci;
        plci->command = 0;
        return false;
      }
      else
      {
        cleanup_ncci_data (plci, ncci);

        if(plci->B3_prot==2 || plci->B3_prot==3)
        {
          ncpi = &parms[0];
          if(ncpi->length>3)
          {
            add_d(plci, (word)(ncpi->length - 3) ,(byte   *)&(ncpi->info[4]));
          }
        }
        nl_req_ncci(plci,N_DISC,(byte)ncci);
      }
      return 1;
    }
  }
  sendf(appl,
        _DISCONNECT_B3_R|CONFIRM,
        Id,
        Number,
        "w",Info);
  return false;
}

static byte disconnect_b3_res(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			      PLCI *plci, APPL *appl, API_PARSE *parms)
{
  word ncci;
  word i;

  ncci = (word)(Id>>16);
  dbug(1,dprintf("disconnect_b3_res(ncci=0x%x",ncci));
  if(plci && ncci) {
    plci->requested_options_conn = 0;
    plci->fax_connect_info_length = 0;
    plci->ncpi_state = 0x00;
    if (((plci->B3_prot != B3_T90NL) && (plci->B3_prot != B3_ISO8208) && (plci->B3_prot != B3_X25_DCE))
      && ((plci->B2_prot != B2_LAPD) && (plci->B2_prot != B2_LAPD_FREE_SAPI_SEL)))
    {
      plci->call_dir |= CALL_DIR_FORCE_OUTG_NL;
    }
    for(i=0; i<MAX_CHANNELS_PER_PLCI && plci->inc_dis_ncci_table[i]!=(byte)ncci; i++);
    if(i<MAX_CHANNELS_PER_PLCI) {
      if(plci->channels)plci->channels--;
      for(; i<MAX_CHANNELS_PER_PLCI-1; i++) plci->inc_dis_ncci_table[i] = plci->inc_dis_ncci_table[i+1];
      plci->inc_dis_ncci_table[MAX_CHANNELS_PER_PLCI-1] = 0;

      ncci_free_receive_buffers (plci, ncci);

      if((plci->State==IDLE || plci->State==SUSPENDING) && !plci->channels){
        if(plci->State == SUSPENDING){
          sendf(plci->appl,
                _FACILITY_I,
                Id & 0xffffL,
                0,
                "ws", (word)3, "\x03\x04\x00\x00");
          sendf(plci->appl, _DISCONNECT_I, Id & 0xffffL, 0, "w", 0);
        }
        plci_remove(plci);
        plci->State=IDLE;
      }
    }
    else
    {
      if ((a->manufacturer_features & MANUFACTURER_FEATURE_FAX_PAPER_FORMATS)
       && ((plci->B3_prot == 4) || (plci->B3_prot == 5))
       && (a->ncci_state[ncci] == INC_DIS_PENDING))
      {
        ncci_free_receive_buffers (plci, ncci);

        nl_req_ncci(plci,N_EDATA,(byte)ncci);

        plci->adapter->ncci_state[ncci] = IDLE;
        start_internal_command (Id, plci, fax_disconnect_command);
        return 1;
      }
    }
  }
  return false;
}

static byte data_b3_req(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			PLCI *plci, APPL *appl, API_PARSE *parms)
{
  NCCI   *ncci_ptr;
  DATA_B3_DESC   *data;
  word Info;
  word ncci;
  word i;

  dbug(1,dprintf("data_b3_req"));

  Info = _WRONG_IDENTIFIER;
  ncci = (word)(Id>>16);
  dbug(1,dprintf("ncci=0x%x, plci=0x%x",ncci,plci));

  if (plci && ncci)
  {
    Info = _WRONG_STATE;
    if ((a->ncci_state[ncci] == CONNECTED)
     || (a->ncci_state[ncci] == INC_ACT_PENDING))
    {
        /* queue data */
      ncci_ptr = &(a->ncci[ncci]);
      i = ncci_ptr->data_out + ncci_ptr->data_pending;
      if (i >= MAX_DATA_B3)
        i -= MAX_DATA_B3;
      data = &(ncci_ptr->DBuffer[i]);
      data->Number = Number;
      if ((((byte   *)(parms[0].info)) >= ((byte   *)(plci->msg_in_queue)))
       && (((byte   *)(parms[0].info)) < ((byte   *)(plci->msg_in_queue)) + sizeof(plci->msg_in_queue)))
      {

        data->P = (byte *)(long)(*((dword *)(parms[0].info)));

      }
      else
        data->P = TransmitBufferSet(appl,*(dword *)parms[0].info);
      data->Length = GET_WORD(parms[1].info);
      data->Handle = GET_WORD(parms[2].info);
      data->Flags = GET_WORD(parms[3].info);
      (ncci_ptr->data_pending)++;

        /* check for delivery confirmation */
      if (data->Flags & 0x0004)
      {
        i = ncci_ptr->data_ack_out + ncci_ptr->data_ack_pending;
        if (i >= MAX_DATA_ACK)
          i -= MAX_DATA_ACK;
        ncci_ptr->DataAck[i].Number = data->Number;
        ncci_ptr->DataAck[i].Handle = data->Handle;
        (ncci_ptr->data_ack_pending)++;
      }

      send_data(plci);
      return false;
    }
  }
  if (appl)
  {
    if (plci)
    {
      if ((((byte   *)(parms[0].info)) >= ((byte   *)(plci->msg_in_queue)))
       && (((byte   *)(parms[0].info)) < ((byte   *)(plci->msg_in_queue)) + sizeof(plci->msg_in_queue)))
      {

        TransmitBufferFree (appl, (byte *)(long)(*((dword *)(parms[0].info))));

      }
    }
    sendf(appl,
          _DATA_B3_R|CONFIRM,
          Id,
          Number,
          "ww",GET_WORD(parms[2].info),Info);
  }
  return false;
}

static byte data_b3_res(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			PLCI *plci, APPL *appl, API_PARSE *parms)
{
  word n;
  word ncci;
  word NCCIcode;

  dbug(1,dprintf("data_b3_res"));

  ncci = (word)(Id>>16);
  if(plci && ncci) {
    n = GET_WORD(parms[0].info);
    dbug(1,dprintf("free(%d)",n));
    NCCIcode = ncci | (((word) a->Id) << 8);
    if(n<appl->MaxBuffer &&
       appl->DataNCCI[n]==NCCIcode &&
       (byte)(appl->DataFlags[n]>>8)==plci->Id) {
      dbug(1,dprintf("found"));
      appl->DataNCCI[n] = 0;

      if (channel_can_xon (plci, a->ncci_ch[ncci])) {
        channel_request_xon (plci, a->ncci_ch[ *
 ]);
 c) Ei}(c) Eicchannel_xmit_xon (plci);
(c) Eicif(appl->DataFlags[n] &4) {(c) Eic  nl_req_ *
 his s,N_DATA_ACK,(byte) *
 t (c) Eic  return 1 (c) Eicon Neton Non Ncon Filfalse;
}

static er A reset_b3Netw(dword Id, distrNumber, DIVA_CAPI_ADAPTER *a,
			 PLCI *ange  APPL * supU GeI_PARSE *parms)
ith
distrinfo (c)distr *
 urce dbug(1,dprintf("re; you can ")ource   th = _WRONG_IDENTIFIER (c) *
  = (dist)(Id>>16t (c)ifhis s &&sion.)
 with
  our option)
  STATE (c) EswitchThis s->B3_prot thehe hope case B3_ISO8208: ANY KIND WHAX25_DCE INCLUle is *
 *
   freepyrigh==CONNECTEDANTY Ohe hope Eicon Networks range ofRESETrver Adapters.
 *
  Eisendcan ris sour
 *
  Eiour optGOOD (c) Eicon Netwobreakils.
 KIND WHATRANSPARENTied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULARstart_internal_command (ibute GNU re; you coundatiails.
 *
  You should have received a copy of the 2.1
 *
/*75 Mass A must75 Mult in a75 Mass Ave,n &75 Mass AvInd */
se foris sup,ls.
 *
  e GNU _B3_R|CONFIRMpi.h"
#incIdpi.h"
#incand/or ls.
 *
  "w",  thails.  This program is free software; you cansredistribute it and/or modify
  it under the terms of the GNU General Public License as published by
ftware Foundation; either version 2,sor (at yion.
 *
  This program is distributed in with
   but WITHOUT ANY WARRANTY OF ANY KIND WHATSOEVER INCLUDING ANY
  implied warranty of MERCHANTABILITY INCe GN_PENDINGS FOR A PARTICULARy of MERCHANTABILIT = or FITNESils.
 *
   PURPOSE.
  See the GNU A Server Adapters.
 *
  Eicon Filtrue (c) Eicon Netlude "platform.h"
#  This program is free softwaconnecyou ct90_a-------------------------------------------------------*/
/* This is options supported for all adapters that are lic License  ncpine Doftwareqre Foundation; either    0x00000001
#defi Allo it is not necessary to ask it from every adapter*/crose defined here have onlyACTcal meaniwith
  Ei      */
/*--------------------------on Netelse supports this && xdi supportCONhis
 */
#define DIVA_CAPI_SUPPORTS_NO_CANCEL(__a__)ls.
 *
req = NCTURFITNA Senufacture
#incas pu[0]==0 for 
  i original message definition!#incluwarrant_OK_FC_L.iGE.Cwith
  Eiconcp.
 *&_OK_FC1]ils.
 *
  if(

/*->length>2_NO_CANCEL))-----------IDES[1] &1)er_features & MANUFA | N_D_BITils.
 *
    add_drange 
  This-------------3),&----------*4ht (c) Eicicon Netwoon Netwon Networks range reqrver Adapters.
 *
  con File Revis 2.1
 *
  This program iss free softwasel0x000can redistribute it and/or modify
  it under the terms of the GNU General Public License msgblished by
  th=0e Free SoOVIDES_RX_telils.
 ic Licensebp__OK_FC7]nufacif(!strib|| !ci, whe hope that it will bany later verson N_)->DIVA_CAPIundation; either;
static voi[%d], of =0x%x,Tel  *, CNL  *, C sup  *, CsCHANT  *, "ne FILE_ "rd, byte *,msg--------,HOUT AId);
stattel);
statNL.ic void avacapHOUT ASuppSHANTr (ard CapiRegister(wordPlcirmat,se(byte, byte mat, API_SAVfor(i=0;i<7;i++)*plci, wori].------ = 0nufactu/* check if noorks, 20 is open,i_reB3     0x0ed only  & DIVAif(THOUT AI_SAV == IDLE)matiCI   *);
static OUTG_DIocal meanidiva_get_extended_adonly_features (D DIVA_iva_get_ext*format, !c void divaHOUT Arks, 20sAPI_ADAPTEn Netmove_idANTY OF ANY e that it will be useful,
 on Netd);
void _extendeformat atiofill*plci, wo potware  & DIVA_)->manuAPI_PARSE *buteapici, se(&API_P----*/
,*
  ThiAPI_PARSE *) "wwwsss",*plci, wo)  callback(ENTITY   *);

staMESSAGE_FORMA       ((__a__)-> callback(ENT(PLCI   *);
staANUFACTURER_FEATUREiva_get_extendeANUFACTUREALERT))_rc( for alert tone inbatioto the network,  & DIVA_C{word, byte *, * * parms);

static void VSwitchReqInd(PLCI   *plci,/* e.g. Qsig or RBSte  Cornet-Nte  xess PRI  & DIVA_Cc voiId & EXTCTURTROLLERS FOR A A PARTICULARlude "divacap _SEL MANB_REQsg.h"
#inributand/or mMESS 0x2002); dwowrong    trolltic void dgroup_optimiz bytec) Eicon Netwo _ADAPTE, byteUFACTUREor FITNESbyte);ils.
 *
  PARSE   *i =  supils.
 *
  clear_c_ind_mask_bita_get_LCI   *,s supplId-1 API_SAVrd CaumpPTER   *, PThis sourPTER   * API_PAR i<max_word,  *out/* dis    0x0  *, other,wordsord, word, Drd Id, byte   * * parms);
/* its quasi a     0x0PTER   *rd, word, DIVif(testPTER   *, PLCI   *, APPiI   *);ord, DIVA_CAPI& supicaures[i], 
staes & MANIL   *,0ARSE *)_OTHER_GeneSE *);
sta---------------------fine DIVpi_save_msgPLCI, " sig&HOUT A_CAPdI_ADord, wordteord,void api_rd, word);

static byte connect_req(dwith
  Eico *, AlLCI  exare
  nect_res(dwoin use by thiss of thstatic byte--------------a->AdvSignalAdwore);
API_ADAPTER   *!= supsten_req(dword, word, DI CapiRegister(wordExt_CtrlSE *);
s1or (a* * parms);
that it will be useful,
 ER   *, PLCI   *, PLCI   *_)->m   *, APPL   *, API_PARSNOTSE *);
static byte al?lert_req(dword, word, DIVA_CAPI_ADAPTER of PARSE *);
static byte facility_req(dword, word, DIVA_CAPI_2DAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte _res(dw/* activ_bar *, codecCI   *, APPL  atic byte facility_req(dword, word, DIVee SoDAPTER   *, PLCI if(AdvC  *,_sdrort(aInc., 67l Publ0) sten_req(dwordatic byte facicility_req(dword, wrroRSE *   *, procedu to proc* * parms);

s  *, APPL   *, API_PARSE *);
st, word, DIVA_C data_rc(PLCI   *poofAPPL  ==SPOOFINGLCI UIREDLCI  wait unti  *, *, isPTER  eCI   *, APPL  static byte disconrd, word, DIVA_CA = AWAITTER R   *, P *, PLCI   *, APHOUT Atware
  Foundatio= BLOCK_ of c bytlockAPI_PARoundati);
static byt API_PARSE * byte dataADAPTER   *atic byx00000008

/*
  CAtinu>man, API_PloadedR   *, PLCI   *, AP  This program
static byte disconnect_atic byte facilityon NetwoAPI_ADAP, APPL   *, API_PARSCI  is OFF plci, dworDAPTER   *, PLCI   *, APPL   *, API_PARSE *);
, neePPL   but WIofflert_req(dword, word, DIVA_CAPI_ADAPTER   *= API_PARSE *);
static byte faciTER  IdCvoidI   *, A  *, PLCI   *, void api_PI_PARSE *);
statiPARSE  dv_n APPL   *, API_PARSlity_req(dword, word, DIVdisableR   *, PLCI   *word, DIVA_CAPI_PL   *, API_PARSE *);
static byte connect_b3_a_resnos.h"questPI_ADAPTER   *, Pte reset_b3_res(dword, worif (!AGE.CA_CAPI_ADAPTER   *,a_get_excall_dir & CALL_DIR_OUTsten_req(dwoVA_CAPI_ADAPTER=  *);
static  |  *);
statiRIGINPI_PARSE *);
_)->manDIVA_CAPI_ADAPTER   *);
statINvoid add_p(PLCI   *, byte, byte   *);
INtic void addANSWr versthe Free Software
  Foundation, Inc., 67;
static e, Cambridge, MA 0  *, APPL   *, API_Pn :    2.1
 *
A_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *)AGE.C"
#define dprintf









/*----manufacturercan redistribute it and/or modify
  it under the termsord  of the GNU General Public License as published by
oundatibyte test_grou004
#define DIVA_CAPI_XDImsk_bit (PLCI   *mci, wor5-----s);
stadecVIDES_RX_DMA  DES_RX_chd send_redird se             hi[2----{0x01,0x00}tatic void send_llta(PLCI   *);
static word plci_rembyte)_cata(PLCI   *);
st1tic word plci_remnullI_ADAPT{atic word plcic Licenseyte A byte)= {APPL&yte AddIntic w of t  * v_is s*, byte, tatic by Foundation; eitherword, word);
staR   *,  API_PARSE5  *outci(PLCI api_remove_start(vif(GET_DWORDI_XDI_PROVIDES_!=_DI_MANU_ID_NO_CANCvoid SendInfo(PLCI   *, dword, bon N byte data
staic word fin1OVIDES_d sem----------2-----SE *);

statipter*/
/* an(e, CambrOF ANY KIND _CAPASSIGNes(dwied warrantystatic voidata_ack(PLI   *, _PARSE *)"wbbs",ci(PLCI)_NO_CANCEL))void SendInfo(PLCI   *, dword, byteatures = 0;

#icon NetworAPI_Pok(byte   *ci(PLCI ROVIDES_*, API_Pcve_sci(PLCI VA_CAPI[0---------te, byci(PLCI 2te ch);
static vo(PLCi=getI_PAR(a)lci, byte ch,strib= &a->is s[i-------------_a_res(dword,word, DIVA_CAP, APPL   *, API_PI_ADFACTURER_void add_ieHOUT Amc byte datatatic void yte SendMultnnd/oreatund/oric byte SendMult;
stati LOCA *);
statils.
 *
  Ydata( (
  Thi;
static<<8)|E *);
stapterPARS|0x80  *, PLCI  byte reset_b3_reManCMD_msg(se(byteIdr (at yDIVA_CAPI(ch==1   *DAPT2), PL(dir<=2lci, byte ch,LCI i*/
/= ver Ad(  *,|ch  *, PLCI   *ove_annelPARSE *);
staLCI   *, byte, byte   *);
static void add_s(PLCI   * plci, bhelpers
  *decPARSE *);
static byte faKIND 0ied warrte ch, byte f    b1 *, AP&ci(PLCI 3],0,, PLCI   *, _on (PLCI   * plcwrite_com1and);
static v    p *, APCAI,ten_check  *, PLCI   *, c word get_b1_fac/* wordal 'swiA_CAn'PL   *, ch);
ss*, PLC without sAPTERlingCI   *, APPL  /* first 'as;
static 'cilittic byfuncures, PLC *);
sTER   *, PLCIKIND 2and);
static vI_ADAPTER   *, PLCI   *, APPL   *, APItatic byte disconour optiRESOURCE_ERROvoid add_iebyte disconnect_b3_res(i, byte Rc);
static voi_voice_clear_yte   *, b,0,B1_FACILITY_sk, b  *, PLCI   *, APAPI_ADAPTEx10ord, wor   *al*, API_PstreamPTER   *, PLCI   disconnect_b3c word get_b1_facd, word,    *, APPL   *,_mask, byte setupParse);
sSendMultiord, word); =_xdi_extendedextended_xon (PLCI   * plci);

static byte SeSendMultiIE(PLCI   * plci, dword Id, , byte   * * parms, byte   *, APPL   *,);

static wodata_b3_req(dword,ci, byte b1LLI,llstatic byte add_ci, byte b1_HI,chmmand (dword Id, PLCI   *plUID,"\x06\x43\x61\x70\x69\x32\x30"  *, PLCI   *, sig more det,flow_c,DSI);
wource file, PLCI   *oefs (PLCI   *plcstatic byte discon, word b1_fa23_clear_config (PL  *, PLCI   *, AP   *plci, byte Rc);
byte Rc);
static voi------------------- fax_di  *plci);

statiense for more details.
 *
  word Id, PLCI   *plc disconnect_b3e Rc);
static void retatic byte disconnect_b3_req(dwordirword apd, Dse(bytedir_msg(APrd, DIVA_CA   *, PLCI   *, APode, API_Prd, DIVA_CAPI_ADAPTER   *, PLCI*, PLCI   *, APeve_restore_commanDIVA_CAPI_AD  channelARSE (PLC PLCI   *, APPL   *, API_Pq(dword, word, DIVA_CAPI_ADAPTER   *, P* plcSE * byte Rc);
static ARSE *);
static byte data_b3_res(dword, rejER  PI_PARr_femeanwhil
static byte data   *, APPL   *, API_PARSE *);
static c void init_b1_config (PLCI   *pI   *, APPL   *, API_PARSE * void clear_b1_c_conf Id,=1lci, byte Rc);
stac);
static void *);
REQci, byte Rc);
statiid (PLCI   *plci_)->manu!dir)hi);
static void mixer_clear_coLISTEhe GLCI   *plci);
static void mixer_notivoid init_b1_config (PLCI    disconnect_b3_resCI   *plci);

static void dtmde "divacapi.h"
#inclci, APPL    * plci);

statsg.h"
#include "di;
static voidasync.h"


s_set (dword
#define FILE_ "i);
static v"dww",_CAPI_ADAPT,oundatiSAGE.C"
#dnel_id);
static voi2*, API_PARSE *);
static byte reset_b3_res(dword, word, DIatic voidOUT_OFes(dwoived a copy of void channel_IDI_CTRLied warrant Autotatic word get_plcour option)
  any later vers_x_on (PLCI   * plci, byte  (PLCI   * plci);
static void channel_x_of(PLCI   * plci, byte ch, byte flag);
static void channel_x_on (PLCI   * plci, byte r_featchannel_request;
static vonded_xon (PLCI   * plci);

static byte ndMultiIE(PLCI   * plci, dword Idbyte   * * parms, byte ie_typif(req==nfig (PLtatic word get_plcHOUT Ab_move_com= getCks, 20(_config (1 PLCI   *plcmixer_; yourks, 200id_esc  *, APPstatic void rtp  *, PLCI  dword, word, DIVA_CAPI_ADAPTER   *, PLCI_req(dword, word, DIVord, DIVA_CAPI_ADAPTnfig (PL |_indication (dword Id, PLCI   *byte   *msg, word length);
static void dtmf_parameter_write (PLCI   *plci);


st, APPL   *, API_PARSE *);
stac word get_b1_f3_res(dword, word, DIdword IdLAWCI   *plci, byte Rc);
staticcr_enquiryi, byte Rc);
ston Netwo    ssc voidFTYr_config ( (dword Id,;
static void-*/
, PLCI   * for more details.
 *
dword IdHANGUPtatic woroid ecd get_plci(DIVA_CAPg(API, PL AutoEQ  *);

void   callbyte Rc);
staticode, API_PA  *, IDPARSE *);
static byte faci) &&(ion.
 *1;t is n< MAX_NCCI+, ...);++LCI   *plci);

static void dtmode,ty of MER plci------- *);
staIdCI   *      */
/*---------------------isten_req(dword, APPL   *appl, API_P     */
/*-----------apter_features ( bchannel_id);
staI_ADnuporks _data  *, APPapters.
 *
  Eih
  Eicon Networks range of ISCrver Adapters.
 *
  EiI   *plci);
static void clear_b1word, DIVA_CLCI   );

voreq(dword, word, -------------------REMOVEci, byte Rc);
s for more details.
 *
  })))
#defieived a copy of void channel_connword lengtatic
static voect_res wordloopPTER   *ion B-move_comert_req(d;
static void mixer_remove (PLCI   *plci);


static void ec_command (dword Id, PLCI hannel_x_LCI   *plcinded_xon (PLCI   * plci);

static byte SendMult * * parms, byte ie_type,r (byte);
externmlic License 
static voidadapter;ci, byte Rc); for more details.
 *
ord, word, DI byte flag);
static void channel_x_o--*/
extern byte max_Rc byer;
extern bPI_ADAPTER l;
extern DIreceiver/transmitatic  * adapter;
extern APPL   * application;







static byte remove_started = false;
static PLCI dummy_plci;


static struct _ftable {
  word command;
  byte * format;
  byte (* function)(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APDSP  *, API_PARSE *);
} ftable[] = {
  {_DATA_B3_R,                          "dwww",         data_b3_req},
  {_DAADV_CODEC INCLUDING _CAP        ied warr/* TELB3_I|_CAPI_ADAPrd,  b1_facnonic vndard adjustments:er;
extern/* Rc voon/off, H_ADAet micro volume,_CAPI_ADAPsconnect_.E,           h     d+, APPL   speakerect_res},        + data_. gain,                 free   ThhookIDESCAPI)  * discxVA_CAPI_AD---------*/rce file is byte dat=es},
  {_DISCOVA_CAPI_ADAPTER   *, !CI   *TER  b3_re APPL   *appl  *, APPL   *, API_PARSE *);
st         */
/*--------- API_PARrd bACILITY_I|RESP (c) Eicon Netwod Id, word rd get_plci(DIVA_Cm (dword I   *hannel_x_ >= 3, PLCI   *pE,     ----*/
/= (dwo (PLCI   *pl   connect_b2]   "1isten_req(dller ((byte)(Id)onnect_b3_res}        _OLD_----MIXER_COEFFICIENTSPARSE *);
static byte facif);
vLCI   *, A!=   {_VOICd diva_get_},
 yte connect_b3_re   *plci, byte Rc);
static void hold_ API_PARSE *);
static bytend (dword Id, PLCI er_request (dwora;
statvoice_coef__remove_sTIVE_I|RESP-le Revisio   disconn3_req},
  {_RESET_B3_I|RE>PONS       -"ws",        set_b3_req},
  {_RESET_B3_I|RESP(PLCI  NECT_B3_T90_ACword, DIVA_CAPI_       reset_b3_res},
  {_CON {_DISCON     _BUFFER_SIZE, PLCI   *plci, 3_req},
  {_RESET_B3_I|RESP      connect_b3_t90_a_res*, API_PARSE *word,    API_ <_REQ,                     *, PL,
  {_SELECT_B_REQ,              buffer[-----onnect_b4 + i-----------yte)(Id)))

voB1_facilities & al_command);ISCONN,
  {_SELECT_B_Rq},
  {_Rwrit_RESETs  *, APP      connWRITE_UPDATEtatic byte add_b1_facilities (PLword, DIVA_CAPI_A connect_b3_a_res},
  {_DIS----DTMFLiceAMETER        "s",            disconn!    word, word);
feaord)  {_* plci);

staFEA;

s0 */
  { "\x03\x8",             disconnect_b3_res},
  {command);NOT_SUPPOR-------------                "s",         *, API_PARSE *);
dtmf  *,ameter_B3_I|RESPONSE,                "",        03\x90\x90\xa2"     }, /* 4 ONNECT_B3_T90_ACTIVE_I|RESPONSE03\x90\x90\xa2"     }, /* 4 */_b3_t90_a_res},
  {_CONNECT_B3_T90_ACTI03\x91\x90\xa5"     }, /* 5 */ */
  { "\x03\b3_t90_a_res},
  {_SELECT_B_03\x90\x90\xa2"     }, /* 4 */ "\x04\x88\x90\x21\x8f", "eq},
  {_MANUFACTURER_R,      03\x90\x90\xa2"     }, /* 4       manufacturer_r03\x90\x90\xa2"     }RER_I|RESPONSE,      +   "dws",          manufacturer_res},
  {_MANUFACTURER */
ct_req(dworatic byx90\xa2"     }                           data_b3_req*);
static byte facility_r         _PARSE *0\x90\xa3",    {_PI_PAR* application;







static byte remove_started = false;
static PLCI dummy_plci;


static strucr (bytePI_PARCAPI_ADAPTER   *, PLCI   0\xa3",,       API_PARSE *);
} ftable       ",            connec                      "dwww",          data_b3_req},
  {_DAOPTIONS   *,ESc License
  aCI   * plci);
static void channel_x_ofdLCI   * plci, byte ch, byte flag);
static void channel_x_on (PLCI   * plci, byte ACTI
static w
static vPROVIDES_N& ~a2"    WARfile.pr   *,_opPTERndf(APPL xer_remove (PLCI         "\x02\x89\x90"         },(PLCI   * plci, byte a->DIVA_CAPI */
  { _tADAP[API_PARSE nnelx02\x88\x90",         "\x02d Id, PLCI   *pl90",  defaultied warr                     "dwww",         data_b:    2.1
 clude "divacapi.h"
#inclppl, API_PARSE *msg);
static voivasync.h"



#define FILE_ "Md mixer_indication_xconnect_from (_ADAPTER   * a, PLCI   * plci)word, word);
st---------------------------------------------------rd add_modem_b23 (PLCI  * plci, API_PARci, word b);
sindA_CAPI_x90",   nl_req_ncci(PLCI 3-----ic License _PROVIDE* 1 */
  "",faxci, wor9 *, byte, OVIDES_RX_le",  d IndParse(PLCI   *, word *, byte o processf);
vmsg_req_remove_= 0SPONSiva_ /* 1 */
  "",             
static wo /* 4 *ip(DIVA_CAPI_ADAPTE(DIVA_CAPI  *, APPL   *, Aon N 0 */
  ""static void c              " but WIT 0 */
  ""(DIVA_CPN_,       NEGOTIATE_B3 INCLU;
static void micopy of thef);
vTHOUT ANY WARR},
 4CI   *0 */
  "",       5       }atic       

/*RCHANT & NCP         /* 9 _SENTI   *);
static byte reset_b3_ree conn      ) &&       /* 9 E_OK"     ER   *, PLCIlude "platform.0_ACTIVstatic v oid dit_xon (P(PLC
  "\x0);
static mf_cI   * plc                     /* 13 */
  "",, byte,in                  /* 14 */
  "",                    

/*----byte UnMap0"    le  ""offsetof(T30_INFO,ic voionid   + 2ADAPTERode, API_P        0x00IDES_B3_I|RE<  "\\x02\x91\x84",(",        *)1\xb1",               /*RER_I|))->      /* 2_B3_APTER   * );             /* 22 */
  "\x02\x91\xb5",        head_line23 */
  "\x02\             /* 15 */
----------*/
char----------tic vooid mf_c      /* "\x02\x91\x84",               /non-_DISCONNEer_res},
       miss  "",rte conn, byteR   *, PLe   * *, byte);
static vo91\xb1",               /* 21 */
 = "\x02\x91 fax_edata/
  "\x02\x91\xb5",    [lenDAPTER   * );3 */+= 1 +e V120_HEADER_EXTEND_BIT  0x80
#defffff00L) |--------*/

#define V120_HEADER_LENGTH 1
#define V120_HEADER_EXTEND_BIT  0x80
#define V120_HEADER_BREAK_BIT   0x40
#define V120_HEADER_C1_BIT      0x04(      /* 27 */
  "",  "wsB3_AC120_default_h----*/
/>= 2       }, /& 0xfffsf    _resLCI byte(byte   *&83                2 PLCI   *p V120_HEADER_EXTEND_BIT  0x80
#de++nnel(PLCI  120_default_header[    "\x02ACTURER_R,      120_default_header[       manufactk_header[] =
{

  0xc3 | V120_HEADER_BR83                          on Net\xb1",               /* 21 */
ENGTH0"     V120_HEADeine__ack_B3_I|RESP\xb1",               /* 21 */ful,
  e Software
  Foundation, Inc., 67--------------e, Cambridge, --*/
externe DIVA_CAPI_USE_CMA /*- byte c;
  byte controller;
  DIVA_CAPI_ADAPTER   * a;
  PLCI   **/
/* IDI   *pbackew_b1_res* * parms);

static void VSwitchReqInd(PLCI plci; byte c;
  byte controller;
  DIVA_CAPI_ADAPTER   * a;
  PLCI   * plc
voiCILICI   * n(y laTYnel(Aeblisheodify
  it under tel(Aane DIVPL dbug(ord, DIChannel(A_PARSE *
  itMSG dbuic vo"",   , jd send_rer);
static q(PLCI   *)void send_regloba Netw;

/*n},
 _cancel_sg-> Foundation; either%x:CB(%x:Req=%x,Rc conInd=%x)te   *, word, byte *(e->user[0]+1)&0x7fff,eatic e->Reql"));cid ctndCheck(DaLCI (API_ADA[ver Adbug(1,dpri PLCI n (PLCI      plcibug(1,dp1ler];
 arts with 0 x91\dify
  itx89\x90S_NO_CANCEL(aource /*      If newCAPItoco*, API, byt_disXDI  *,us worce,      shouldtrolk      fullycludeccord witci, by  NCCpec anIVA_k  "sCI   * ncciel/* 0stead      of RcT_WORD(",    n Filoefss.    ert_r  /* 1ePL   pletatic 0xff

  0arts with 0 I   \x02\x91  ret(Id)  INC_CON_PENDONSE,    && (  retof the       retC-------r_feat"));
 0"    || (pl_start(voi0x04bug(1,dpri &   *,0      88\x90", !a->adapadapter------ DIVA_CA was;
}   *, n we haveonne, APPL   *m (dword Ilci->Id
   msg->hedlert_to zero arriv   && (    i Allrameter_wi->Id
    ->headerbe ignored{
      i =r;
extern A ord ,   ------tatic word get_plci(DId ctrstatic void diva_free_byte reset_b3_req with RCx02\ISCONNECHANTR   *, PLCI   *_in_re,
  {_CONNECT_B3_R, rks, 200flow, res, C2                       ACTURER_R,      256       manufactller ((byte)(Id)a->FlowCres, CIdT     BufferGet(APffff00L) | UnMapContro0",    >header.length + MSG_IN_ADAPTER   *, PLCI   *, APPL  *);

void E - n)
       0x04
#defirx_dma_descriptor >, APngth + MSG_INiva_      }

      if (itatic int  di    }

      if (i- 1CONNECT_B3_T9 0xfffc))

      {
      - n)
          i = M20 */
  { "\ && = OK_F    facility_req},
if (i > MSG_IN_QUEUE_SchZE -d ctrn)(dword, wf (i > MSG_INSkip_write_pos,
tart(voilci->msch     j = 0;
 e_pos|atur     PPL   * appl, int ));

       plci_pos,
Get(APPLyte * format;
  b Netwd write=%d r      connect_b3_req},
  {_CO.command ==  C_IN_QU_in_read_pos;
elf, i -*/
  { |RESPDIVA_CAPIm (dword Idnd(PLCI   *); (plci != 0) && * appl plci->msg_in_write_pos,,
     

  0d_pos)
rd, word, DIwrite_pos != plci->msg_in_rTER   * );
stayte v",
     

  0>msg_in_read_pos, plci->msglci, byte Rc);
sbyte 3on; eith ("dpri
  i:UEUEG_IN_Qletrib:0x02, Ch:%02x",* * parms);

static void VSwitd ctrl ch   *, PLCI   *, UEUE_SIZE - j;
  PLCI   *, APP   {
        ));

        return _QU&E_FULL;
      }
static void diva_free_        n = ncci_ptr->da= ~_FULL;
      }
      c = _DATA_->heDISCONNEChSPONSE,       & 0xffffff0ci->msg_in_que facility_res(dic void fax_edata_)(plci->msg_in_queue)))
 0x04
#defi)
    SPONSE,    = 0;
   
static i0, rceturi->msxdi_    "\x02nnect_b3_req},
  {_CONNECif (j >N_XOd add_ss(PL void SetVoices, 2002_  This seturnd_pos;
        wARSE *);
static byte dSPONSE,       &((byte   *)(plci--*/
sg_in_queue))[k]))->heade   if (k == plci->msg_in_ller ((byte)(Id)))

vonl_  /* contrPARSE *);
static byte faci  /* contr(byte   *_b3_req.Flags   *, API_PARSE *);
_b3_req.FlagsPI_PARSE *);
statie=%d",
,
  low_co    atic byte discond ctrPI_PARSE *);
static i -= j;
      }

      if (i <= ((msg->headerheader.length + MSG_IN_OVERHEAD + 3) & 0xfffc))

      {
        dbug(0,dprintdefine V120_ULL1(msg) - len=%d write=%d reaI   *plci);
static void clear_b1_crks, 2002.
 *
  This sour      {
            n++;
     >msg_in_que  /* contrAPI_MSG   *)(&((b                    "" 03\x90\----ONSEPARSE *);
static byte faciing, n, ncci_ptr->data_ack_pending, linternal_comman-----   *, API_PARSE *apId(Id) XNuTER         "",    al_co  *)(plci-in_queue))[k]))ffc;

      cci == ncci))
          {
              n++;
            if (((CAPI_MSG   *)(&((b   *, APPL   *, API_PARSE *);
static byte ming, n, ncci_ptr->data_ack_pending, l));

          retULL3(requeue)"));

            return _QUEUd=%d wrap=%d fe   * *, byte);
static .command == (_DISCONNECT_B3_I|RESPONSE))
      || ((ncci < MAX_NCCI+1) && (a->ncci_plci[ncci] == plci->Id))))
    {
      i = plci->msg_in_read_pos;
      j = plci->msg_in_write_pos;
      if (j >= i)
      {
        if (j + msg->header.length + MSG_IN_OVERHEAD <= MSG_IN_QUEUE_SIZE)
          i += MSG_IN_QUEUE_SIZE - j;
        els word N
statIZE - n + 1;
      on Netwo plci->msg_ici->msg_in_qu Id, word Nig3_req.Flags & 0x000I_PARSE *)++;
          }

  p_pos, i));
  ic byte SendMultp_pos, i));
   + 1;
        i -=->header.length ord, DIVA_CAIN_OVERHEAD + 3) &ing, n, ncci_ptr->data_ack_pendin));

          return _QUEUE_FULL;
      
     "\x03\x90\x90\xa2"       if (j == 0)
     for (i = 0; i < msg->header.length; i++)
            if (((CAPmsg_in_queue))[j++] ntrol_rcck_pendA    :ci = &a->plci[msg->heades.plcRlci-statd"DAT't j =del    e/* 0ci=%pId(Id) amencci = GE. Also api_(1,dpridataabled)
  {
   ("plci=%x"jumpatic void L   *o featsh{
      _remove(P  if (plci->ms +
       rks, 2002.
 *
  e details.
 *
goto c   /CI   * n_suffix0"         }, / j + MSG_IN_OVERHEAD;
  ci->apprn _BADl code su ((ncci == 0)
     ||f (j == 0oftwa_b3_,
    nstat0x0fd Id, PLCftwaC>headerIndommand =i->appl
r >==------I   *,(i=0, ret =A Se)

  ck_pending,
        n = plciCin_reaGet(APPL  +
         ACTIVE_

        returnle[iata_RX_FLOWbyte con_MASh +
             {
          dbug(1,dprintI: pendc voN-XONlength %d",Crn _BAD_MSG;
ECT_B3_R, nd) {
      /* break loo= plcf the message is cor (c) Eicon NetwonlER  & 0xffffff00L) | l
   RNRhead1IZE(ftable); i++) {

    if(ftable[i].command==mZE(ftable); i++) {

    if /* break loop if the message is corr +
         ifications)   */
      if(!api_parse(msg->info.b,(word)(mise continue scan  */
      /* (       fakedmple: CONNECT_B3_T90_ACT_RES _req.Daci, API_SAVE  p_polength-12),ftab((__a__<MAX_Mr.comma}

n 0;
      }
    }
 :
  }PLCI  static     _inck_pId) & 0xfftware
  Foundati       >internmsr),
       poshead          appr    po    /*I_PARSj* 6                msg_pafferGet(AP    appl,apg_par ? 0 :                msg_pa)
     &* 6 ((_MSG;
  }

 )(&(ver Aq_in_s;

  if(c==1)queue))[j]xc1",   er*/
  "",+
{

 ->Staf up toAPTER  plci->req_in_start = plci->req_out = 0;
  if(plci &ack_pe(dword,*((rintf("ba_in_start = plci->req_out = 0;
  if(plci+i] API_SAVE   *out);
statidIVA_ue1", (0x%04x) -      =%d   ms---- sen=%dte   *, wm !plci->rn_xconne              l,
      -----*/

static  msg_pa-----*/

static wend_req-------0x04
#defiextended_xon (plci);

  if(c==1) send_req callback(ENT;

  if(c==1) send_re = MSG_IN_QUEUEx91\x90\xa5" rms)
{
  word i;
  word i +    parmOVERHEA__)   (((__a__)->at[i]; i++) {
    if(parmsswitch(formj    t[i]) {
    case 'b':
      p  *parms)
{
  word i;
  word p;

  for(i=0,p=
      mat[i]; i++) {
    if(parms)

            parms[i].info = &msg[p];
    }
    switch(form   parms[i].info = &msg((__a__)->manparms)
{
  word i;
  word p;

  for(i=0,p=0; format[i]; i++) {
    if(parmsg[p+2]<<8);
        p +=(parms[i]. {
    if(parms)
    {
      parms[i].info = &msg:
     rd,woi_put    DAPTADAPTER 
   |RESP|| (msg->header connec--------------    "wDIVAdm_m)>ncci[ncciTdata_b3BER_I|F    = NULL;rt = p*)(long90_a_IDES.al_cou can .ied Check(DIVA_nect_b3_req(dword, wor------ from/
/*--------",er =save_msg(API_PARS/
  "",                          a,
  li_notify_updat, at[i]; i++) {
    itch (format[i])
)))
           &LCI      n = 1;
    i->msg_msg_in_queue].lente   *);line_e details. for more detail, PLCI   * ) ||
&((byte   ( of the GNU ller &0x  break;c  break_que1);

  /* contr ter, /* 3 *ic byin_wrlisheedistriback_edistrr out->e it and/orSE *);
static void sig_req(PLCI   *, byte,X_MSG_SIZE) {
    dbug(1,dprintf("bad len"));
    retr          = in[SS      ]  = b23_5\x00o = in-2o = in->" = 0;
  do
  struct[der.b23_9o = in->par6o = in->par } while (in-"}
  }
  dtatic 
word CapiRe0on; eitherA:in[i].lengtoid);3_I|Rth %gth %---------------"    {

        FULL;
      i].inf-------UEUE_SIZE on Nundation; either vq0_in/out=%d/---- a,
        -----------out      dword, wo      !=--------------s);

  ch  plce   *iva_pos;
      eader.lenI   *,",
   er.length \x02\x91\x84",               /    1_in_reR   *, PLCIUEUE_SIZE - ntrol_rc(P_IN_QUout_DISCc voCT_B3_I|o }

 );
   afaticSIGstarted failzeofert_ron N a,
        (byte   *      RCHAre)))-------------msg->heabyte reset_b3_req(dwyte   or (at y(dword,PARSE   *iengt  }
DIVA_CAPI_ADAversion.
 *nd) {
 *
  (((-------API_PARS   *, batic (redist    ci ?t is n:turn  << 16) |word AdvCodecSupmax_8ci;ja *) msg) <dword, wo*, A&&, dword *format,!d, PLCHELD) Id|=ic byte connecack_pes, byt    }

   , byte ie_tbyte reset_b3_reder.l_RC-Id=%08lxc void%x,tel---- entity  *, C* plci, -------intG   * msd CodecId);
static void api_save_msSigAPI_PARSE n_xconne_in_queue)))
          API_SAVE   *out);
statiid   seni_load_msg(APid   sendI_PARSE *parms)g_in_wri
voide deta)(plci->UEUE_SIZE - dword IdISCONNE&& rc==(byte   *)(plci.controller)atic voidntrolle MapId(Id) ommand_queue----------------- for more details.
 :
      IVA_CAPIi))
        .controllpers
     */
/*----tatic word get_p     C_HCONNREQand);
statbyte reset_b3_reHoldRCse(byte          d (d  {
    annel(PLCI Snd_qu_descriptor  rc!= *)(plci->msd diva_free_dma_de_sdram_barc voidack_pending,our opt;
sta        "",ECT_B3_R, de "divacap\x02\x88\xE *msg);
sId,and/or "w /*   th,3,  {
   _ack_pendin   ""         _commanRETRIEVEeue[i] = NULL;
}


static voidRetrievert_internal_command (dword Id, PLCI   *plci,    plcitd_internal_command command_function)
{
  word i;

  dbug (    }
   printf ("[%06lx] %s,%d: start_internal_command",
    UnMapId (Id), (char   *)(FILE_), __LINE__));

  if (plci->internal_command == 0)
       _Ri] = NULL;
}


static void  thrt_internal_command (dwl_command tatic PL   *, API_PARSE *);
d",
    UnMalci)
{(char   *)(FILE_), __SSAGE.C"
#dmmand (dword Id, PLCI   *pes & MAN{
  word i;

  dbug (1, dprC   0x00R  *, /ernal_command_"efinerc,n; j++)
                lci->msg_in_quAPTER  * a);
static void diva_a               "s", r (i = 0;      !=>Stat>msg_in_queue))[k]))->inf(word i;
  woe_started

  0x->header.length SPONSE,      iva_!oid)
{commalci->i, PLCI   _command_qu_INTPARSE *);
static byte facility_req(dword, No more IDs/C_ADAReq;j++)PI_ADAPTER   *, PLCd",
    UnMal_command(char   *)(&>StaL

  plci->inect_to (dwor((byte   *)(plci->mg_in_wr) >= ((byte   *)(plci->msg  dbug (1, dprintf ("[%0][2] = {
  { "",               mmand_queu  }
  sk, byte setuvCodecSstatic vapterTURER_FEATU----------------i->internal_command_queue[0]

  plci->in, PLCI   *, facility_res(dw/* D-ch,           ert_req(dword, word, DIVA   *)(&((byte   *)(plci->msg_0]))(Id, plci, OK);
    if (plci->internalX.25 _command != 0)
      return;
    plci->internal_command_queue[0] = NULL;
  }
}


/*------------------------------------------------------------------*/
/* NCCI allocate/remove function                 ------------------*/

static dword ncci_mapping_bug = 0;cci[ch])PARSE   *inres & MANUFTIVE_Idwor0,"0\x01"   if (dbug(0,dprintf("Q-F  dbug (1orts this
 */
g_bug = 0;

static w= 0;
  plci->internal_commanI|RESPONSlied warr< MAX_INTERNAL_COM (DI_LEVELS - 1; i++)
      pl) && !a->ncci_ch[c*, byCCEPupParse);
s(dword Id, PLCI   *pR   *, PLCI{
  word i;MAX_INTERNAL_COMMAND_LEVELS - 1; i++)
      plci->internal_command_queue[i] = plci->internal_command_que-------------------t(APPL   * appl, int Nu------------   ncci_mapp
static dword ncci_mapping_bug = 0;

static wal_command == 0)
  SUSal meue[i] = NULL;al_command == 0)
  RESUMi->internal_com= 0;
  plci->internal_commandm_mhile ((ncci command command_function)
{
 ------------------*/dm_msg.h"
#inword ncci_mappion)
  any later _ack_pending,_res},
  {_CONNECT_B3_R, ion.
 *tati(j=0;i->msg_in*, ApParse);
static 
statommanfci;j++     foery adaax_plc_xmit_extended_xo  *, ID++,%d: next_inted Idhe GNU     l = ncci_ptr->data_ac   */
/*-----------ch[ch])
        ncci = chMAX_NL_CHANNEL+1) && (j < MAX_NCCI+1));
         f (force_ncci)
     HANNEL+1) && (j <    else
    {
 ((ch < MAX_NCCIyte   *)(plci->msg_in_queue))[k]))-);
void   * ReceiveBufferGe-----------------------------------------*/2x",
              ncci_mapping_bug, ch;
      else
      {
        ncci = 1;WHAT        while ((ncc(dword Id, PLCI   *pude "mdm_m:ci;
-------------------ude "mdm_msg.h"
#inword ncci_mappingert_req(dwo1)
        {
          ncci_mapi));
                 k = j;
        st] = (byte) ncci;
    }
    a_command = 0;
  plci->interna* plci);

statncci_next[plci->ncci_ring_lisPERM_c voeue[i] = NULL;   }
          } while ((i < MA (PLCI   *plci);


static void ec_i)
      ncci = force_ncci;
 __LINE__));

  plci->internal_command ----------------------------- if (k == plci->msg_in_wr DIVA_CAPI_ADAPTER   *a;
  APPL   *appl;
  word i, ncci_code;
  d   ""             }, /* 24 */*/
  { "\x02\x88\x90", , APPL   *, API_PARSE *length +3);
      }
 cci == ncci))
        RNAL_COMMAND_LEVELS; cci == ncci))
          internal_comm_b3_res(dw == plci->UEUE_SIZand == 0)
  
staMWIbe usecci);
}


stati=al_cod);
undatioct_req}ebute APP) && /* 7 */
  ert_req(dword, word, DI       msg->header.length, plc-----------------------   ""           /* Get  *, PLCed Service);
static belse
   SERVeue[cal m   appl = plci->appl;
        ncci_code = ncci | (((word) a->Id) << 8);
        for (i_res},
  {_CONNECT_B3_R, PUtatic wo&].length 6],  cor_TERMINAL_\x90ABmand)I_PARSE *);
} fPI_ADAPTEpId (Id), (char   *   *, API_PARSE /* 0;

  lengthci_mapping_bug,)
        {
          if ((appl->Data     INTERR_DIVERSIOd mixtaFlagDataNCCI[Iwarerog    P   /* 14 eue)))
      {
  ug++;
      NUMBER }, /taFlags[i] >>_commaFbe uRthis
 ("NCCI es(dword, word,_com Forwar) {
 S plcifor exam       ncci_malse
 OP    {
          appl = pplci->appl;
          opi_code =  ncci | (((word)CB }, /* 18);
        }
        el    DE    e /* (appl->DataNCCI[i] == ncci
     OG             && (((bNCCI mapping appl expected %ld %08lx",
lci, word write_com
          dbug(1,dprintf(ck_pending, lord Id, PLCI l->DataFlags[i    dbug(I allocate/remove function     ci_mapping_bug, Id));
        }
     }
      }
    }
  }
}


static vug, Id)up_ncci_data (PLCI   *plci, word ncclse
        {
        }
      }
    }
  }
 *);
FORWARmeane
     == plci->Id))
  {
    ncci_ptr = &(plci-Id) << 8>ncci[ncci]);
    if (plci->appl)
    {
      OP == plci->Id))
  {
    ncci_ptr = &(p        if ((appl->DataNCCI[cci]);
    if (plci->a        if ptr->data_out].P != plci->data_sent_ptr))_code)
             && (((brFree (plci->appl, ncci_pt_code)
   ptr->data_out].P != plci->data_sent_ptr))>DataFlags[i] >> 8)) == plci->ree (plci->appl, ncci_pt>DataFlags[I allocate/remove function   ECT_B3_R, ifword i;
  wf ("%s,%>header.length + MSG_IN_OVERHEAD <= MSA(PLCID    s  /*rt_intequeue[1] != N(i = 0; i < (i = 0; i < appl->MaxBuffer;;
staticE   *i)lci->internal_commai->aISDN_GUAR    Jid ncci_free_receive_buffers (
  itdword 1_resource (dwofacility_res(dwstatic void ncci_free_receive_buffers (x89\LEMENTARY_applconnx02\x89\x90"         },nal_command",
  ve_ncci)
{MapId (Id), (char   *)(("inCI   *plci);
staeturn 0;
}

 __LINE__));
  Thi

  if (plci->intern);

DES_N i++)
        {
          if ((appl->DataNCCI[3ptyin[ifereci[m_code = ncci | (((wordPd), DataFlags[i] >> 8))   a,
    latedPTYRESPONc word get_b1_f

  ii_remove(&a_ncci_data (ommand (dword Id, PLCI ve_nccpty     cci);
     ) {
      fo

  icSupport(bug, IdAPI_ADAPTER,%d: next_inteSig.Id) r) ncc }
  api_remove_comple}


static void ncci_free_receive_buffers 0x300Ec bytrd, ci_code =tion prototypes     tf("NCCI mappieatuULLbug(0,dprintf("Q-F2x-%02x"E - n)
          i = MSG    {
rve_nccia, PLCI   *plci, APif (a->ncci_plci[nccm (dword Id, PLCcation_coefs_set (  {
      nccm (dword Id, PLi_mapping_bug++;
      dbug(1,dprintff ((appl->DataNCCI[ExIVA_iti->appSAVE fer, Id, preserve_ncci));
a;
  DataFlags[i] >> 8lity_req(dword, w;
  

static void ncci_remove (PLCI     cleanup_ncci_data (plci, ncci);
      dbug(1,dprintf("NCCI mapping released %ld %08lx %S_etupParse);
sncci_mapping_bug, Id, preserve_ncci, a->ncci_ch[ncci], ncci));
      a->ch_ncci[a->ncci_ch[ncci]] = 0;
      if (!preserve_ncci)
      {
        a->ncci_ch[ncci] = 0;
        a->ncci_plci[ncci] = 0;
        a->ncci_state[ncci] = IDLE;
        i = plci->ncci_ring_list;
        while ((i != 0) && (a->ncci_next[i] != plci->ncci_ring_list) && (a->ncci_next[i] != ncci))
          i = a->ncci_next[i];
        if ((i != 0 ncci_mapping_bug, ch, force_nbyte reset_b3_re_Mord, word);
nternal_com
          else if (pl +1];
    plci->internal_command_queue[MAX_INTERNAL_COMngth + MSG_IN_OVERHEAD <= MSci->internaR   *, PLCI   *------------ppl, API_PARSE *msg);
sword ncci_md mixer_indicatioi)
      plci->/*------------------------>appl)
        {
 ONSE, ter[i API_PAnital coPPL   *, *, CAPI_ADAP_code = ncci | ((t = ncci;
      else
        a->ncci_nex
        {
          a->ncci(plci->internal_com a->ncci_plci[ncci] = 0;
          a->ncci_state[ncci] = IDLE;
          a->ncci_next[ncci] = 0;
        }
      }
    }
    if (!preserve_nccinal_command_queue[0] = NULL;
  }
}


/*------------------------------------------*/
/* PLCI remove function                                             */
/*-------------------}
  rCOD_HOOK8);
          for (i1_facilj]))edci, byHook.h"
#inclu Id));
      }
      else
    Trans *);        {
          a->ncc***TER   (plci->iPor examA,nfo.{
  ternal_command (dwARSE *);
static byte datamsg_in_queONRER_FEh);
static void byte *)(plci->msg_in_quer.len)[i]))->info.data_b3_req.Data));

 nding  += (((CAPI_MSG   *)(&((byte    }
  er.length +c word get_b1_fixer_clear_config (PLCI   *plci);
 for more details.
 *
    *)(plci->msg_in_queue))[i]))->heaA       a->nc   MSG_IN_OVERH(i = Nulli->appReing_bug,RT_B3_I|_code = ncci | (((word)_NCRif (eue[i] = NULL;
}


static voidl_comma_list = 0;
          else if (plc   ncci_ptr->data_ack_out = 0;
    ncci_TER   * DIVA_CAPI_ADAPTER   *a;
  word ncc    }
        ncci_ptr = &(ader.command =L   * appl, void  char   *)(FILE_), __LINE__)); = N);
          if (i e usemand (dword Id, suppli_reCREnADAP)))
           && (((byte -------------------------atic byte facility_res(dw j));
   _commaNCCI+1i_remove_check(plci))
  {
  API_ADAPTER   *a;
  word ncc0x", plci->NL.Id));
    if (plci->NL.Id && !pf (force_ncci)g(1,dprintf("D-channel X.25 plci->NL.Id:%0x", plci->NL.Id));
    if (plci->NL.Id && !plci->nl_remove_id)
    {
      nl_req_ncci(plci,REMOVE,0);
      g(1,dprintf(rd Id;

  a = plci->adapter;
  Id = ( = 0;
        if (!mitB_g(1,dpcci);
}


sta))

void   sendf(APPL   rd, word, DIVA_CAPf MERCHANTABILITY or FITNESS FOR A c);
static void fax;
void   * ReceiveBufferGet(APPL   * appl, int Num)nt fax_head_line_time (char *buffer);


/*-------------------------------------------------g(1,dprintf(cci_ptr->data_pending = 0ci->State == INC_CON_ALERFFn (ncci);
}


stX_INTERNAL_COMMAND_LEVELS - 1; i++)
      plci->internal_coplci->msg_in_wrap_pos = MSG_IN_QUEUE_SIZE;
}


static void plci_, bytencci_state[ncci] = IDLE;
 \x88\x90"b1_faciliti  {
ode)
             && (((byte)(  {
_ptr->data_out)++;
        if,plci->tel));i->internadprintf ("%s,%d: init_ir.length + MSG_IN_OVERHEAD <= MS  {
lci, (PLCIPI_ADAPTER   *, PUEUE_SIZE - j;
        els_)->manuommand command_fu* * parms);

statheck(plci))
  {
  on)
  aESPONSE,     AX_DATA_B3) ||6lx] %s,%d: 7c bytIllegxdi_extende    /* 14    c void adjust_b1_optimization_mask_table[b invalid */
/*----TER   *, PLCI   *, APPL   *, API_PARSE *);
static byte m     {
     B  a->ncci_ch[ncci] = * * parms);

static vo-------------------------------------ncci_ch[ncci]TER   *, PLCI   *, APPL   */*
    10:);
    rerd, allow     }

is,      TER   *, PLCI }
 else
  {      4],      ic bytSS>ncci_ch[ncci] = 0;
           plci->c_ind_maski, byte preseing appl expected %ld ==k_table[b >> 5] &= ~(lci->Sig.Id
      || (>c_ind_mask_table[1],S   {
ode)
   ng_bug = 0;

static word g] |= (1L << (b & 0x1f));
}
_code)
   t_command (dtate = OUT----*/

e      } while ((i < MAX_NL_C(ncci)
  {
i);
static void mi != 0) && (a->ncci_next[i] != plcist) lci->plci);


static void mincci_next[i] != ncci))))
          i = a->ncci_next[i];
     t_internal_cord Id;

  a = plci->adapter;
  Id = (((dword) ncci));
}

static byte test_c_ind_mask_bit (PLCI   *plci, word b)
{
  return ((plci->c_insk_table[b >> 5] & (1L << (b & 0x1f))) != 0);
}

static void dump_c_ind_mask (PLCI   mask (PLCI   *plci)
{
  word g.h"_BEGI         }
         for (j =AD     ; j++)
    {
      if (SPLI ((appl->DataNCCI[i] == (j =DROP      d = plci->c_ind_mask_ISOL             && (((byte)(a(j =REATTACHf (i == ncci)
            plci->ncc    *(lci_remove(%x,tel=%x)",plci->Id,plnd_mask_bit (PLCI   *plci if (i+j < C_IND_)&&   cleanup_ncci_data (pplci, ncci);
      dbug(1,dpr->ncci_next[nccii_ch[ncci], Id))
            {
              appl->DataNCCI[i] = 0;
 (j = 0; j < 4; j++)
    {
--;
      }
    }
    (j = 0; j == plci->Id))
  {
    ncci_ptr = &(pif (i+j < C_IND_MASK_DWO--------------------------AD         }, /*   dbug(1,dprintf("NCCI mapping release_next[ncci];
        }
        a->ncci_next[ncci] = 0;
    ------------------*/
/* transla {
        d = plci->c-------------------------- {
  ----------------------------*/
/* translatable[i+j];
        f--------------------------tabl----------------------------*/
/* transla k < 8; k++)
        {
 -------------------------- k < 8;----------------------------*/
/* transla(--p) = hex_digit_table[d &------------------------(--p) = out = 0;
    ncci_ptr->data_pending = 0cci = 1; ncci < MAX_NCCI+1; ncci++)
    {
      if (a->ncci_plci[ncci] == plci->Id)
      {
        cleanup_ncci_data (plci, ncci);
        dbug(1,dprintf("NCCI mapping released %ld %08lx %02x %02x-%02x",
          ncci_mapping_bug, Id, preserve_ncci, a->ncci_ch[ncci], ncci));
        a->ch_ncci[a->ncci_ch[ncci]] = 0;
        if (!VSWIT= hex_digit_table[d &   }
          } while ((i < MA-----------_ncci_data (pmask operations for arbit
    if((i=get_plci(->vMAND_Li_parse       && (((byte    plci = &a->plci[iable plci->appl = appl;
      plci->call_dir = CdiastatCAPI->c_ind_mask_tLL;
  if ((plci-ci[i-1];
      plci->appl = dec supp = CALL_DIR_OUT | _CONTROLLER)heck 'exte  *)(&((byte   *)(plci->msg_in_queue))[k]))->i;
    if((i=get_plci(ZE(ftable); i+      if(Ad-1];
      =1df(appl, _CONNECT_R|CON plci = &a->plci[i-1];
      =3l;
  jod, DI || (plTER   *, PLCI  dec support */
    3group_ind_mask (PLCI   *plci)
lci->appDefstatr;
 ;
    return;
  (SSCT) ncci | (((word)on function for each
    if (plci->appl)DEF  *,eanup_ncci_da 0;
      if (!preserve_ncci)
      {
        a->ncci_ch[ncci] = 0;
        a->ncc    nlCDcci(plci,t(a, plci, ai<5;i++) ai
  if (ncci)
  {
    if (a->ncci_plci[nsk_table[b >  {
      ncci_mapping_bug++;
      dbug(1,dprintf j++;
              TPi->ncci_ring_EQ_COMMAND_);
static w_ncci)
)
    ','2','3','4','5','6'(1,dprintf("NCCI mapping overflow %ld % %02x %02x %02x-%02x-%02x",
              ncci_mapping_g_bug, ch, force_ncci, i, k,i] = plci->Id;
      a->ncci_state[ncci] = IDLE;
 ->ncci[nci[ncci] == plci->apping_bug++;
        dbug
  if([0]          {
          (*].info)[5]);
                }
  is pInc., 675c_queue))[k]))->header.ncci == ncci))
          {
    UEUE_SIZE - j;
        els(PLCI   * plci, byte nexoftware
  Foundation, Inc., and = 0;
  ase(wordADAPTRSE *0ai = &equest) {
   d AdvCodecSupport(DIVA_CAPI_ADAPTER.plci[j].Sig.Id) r }
  }
  api_remove_cool helpers
 ng_bug++;
        dbug(1,dprintf("  ncci_mapping_bug, Id)   MSG_IN_OVE           L1_adapflow_conFlags[i]       }M            {
           _ind_mask_bit (PLCI   *pltatic word get_plc(PLCI   * plci, byte ((byte   *) msg)[i];
fo.data_b3_req.DataL1bp->;
  );
  R   *, PLCI  ARSE *);
static byte datat(a, plci, (FILE_), __LINE__));

  plci->);
} ftable[] = {
  {_DATA_B3_R,   ""         ength)LinkLayer = bp->info[3];
   , _I|Ri_re(dworptr >> 5] & ai = &pa = 0;
      if(ai->lengt
      {
        ch=0xffff;
        if-----------------xer_remove (PLCI         a->ncci_ch[ncci] = 0;
    on NetwoDIVA_CAPI_ADAPTER   *, PLA_CAPI_ADAPTER   *,  DIVA_CAPI_ADA.        lci->Sig.Id
      || (plc!           m =   }
           m = 0;
     er.command == _D"D-channel X.25 plci->NL.Id:%0x", p, DIVA_CAPI_ADAP    ch = GET_WORD(ai_parms[0].info->Id)
    {
      ncci_mapping_bug++;
      dbug(1,dprintfintf("NCCI ma              Info = _WRONG_MESSAGE_FOR_command)
          c =             if(ai_parms[0].info[i+5]--*/
extern byte     TransmitBufferFree (plci->app
          (byte *)(long)(((CAPI_MSi_parms[0].length<msg_in_queue))[i]))->  *)(plci->msg_in_queue))[i]))->header.length +
   fo.data_b3_req.Data));

      }

      i= (((CAPI_MSG   *)(&((byt       }
                elffc;

    }
    plci->msg_in_write_pos = MSG_= (((CAPI_MSG   *)(&((byt          if(ai_parms[0].info[i+5]_read_pos = MSG_IN_QUEUE_SIZE;
 plci_remove(PLCI   * plci)
{

  if(!plci) {
 ixer_clear_config (PLCI   *plci) for more details.
 *
i_parms[0].length<c void _WORD(ai_parms[0].info+3);move_started) {
*plci, byte Rc);
static    if(ai_parms[0].info[i+5]!=byte reset_b3_reListenrd, Dord,wi].m_ID(CAPI_MSGnd_queue[i] (i = 0; i < ci, byte b1ESC_b23_2\x18ms[i+--*/sc_chi[i+3] = ct_req},  *plcci c void adjust_b(FILE_), __LIINDIC       API_PARSE *);
} ftable[] = {
  {_DATA_B3_R,       for(i=0; i+5<=ai_parms[0].lengch=%d",ch,d != 0)
 ( (PLCIRc  *, )rnal_command (dwa:
  =%d"_E *);
--if (!plci->appl)
        {
          ---------*/
/* NCCI alloca.info[i+5] | m) != 0     USE-------0].info+3);
              ch_mask = 0;
          _ncci)    return;
  }
  i to the Frehers);
static------ = MSG_IN_QUEUE_SIZE;
}


static voidptr->data_ack_penuto-Law >> 5] & (1L << (b & 00] =
  {'0','1','2','3','4','5','6'); 
        add_s(plci,OAD,&par != 0)
      return;
 3_reutomree _law = ];
      ch            if(ai_parms[0].info[i+5]!=parms[7]);
       i] = 0;
        a->nc/* check length of channel ID *--------- (PLCI  else dprintf  = _WRONG_MESSAGE_FO); 
        add_s(plci,OAeaturncciR   *, PLCI  rms[7]);
        adlci, byte  RMAT;
      }

      dbug(1,dprint
              for(i=0; i+5<=ai_parms[0].lengs(plci,OAe c_ind_mask_empty (PLCI  rms[7]);
        add_s(plci,HL          if(ai_parms[0].info[i+5]!=0)
                {
                  if((ai_parms[0]arms[0].info);
        if (a->Info_Ma



#include "platform.(plci->State _queue (PLCI       is free s) ||
i->msg_h + 1;
      breakchfo[j];
  }
  out->X_MSG_SIZE) {
    dbug(1,dp * T---*   * Tt<max_, byte *DESC---*al_ce Free Software FoC_IND_MASKter[i].requestSAVE *out)
{
  word (ncci)
  {
ci->msg_in_queueh!=2--------{
    for(i=0;i<max_x_adapter;i++) {
      if(atart(voj=0; * appl, voiTransmitBufferGet(APPL     {
    case&& ch!=2i = NULL{
   yright (c) Eic messages        ----------,ctionsfor exa-----&& ch!=2rnal_coout{
            apfor exa(i = 0; i Info)
  sg(parms, "wsssssssIP mask bit 9) no ne_t = Naved_msg);Dt)
{
               api_s PLCI   *plc\x90\x = Criteagsuse w {
   f MERCHANTABILITid ncci_free_receive_bu {
        fod in <<16)|          }
        er[i].plci[  plci->Id = 0;= _WRONG_MESSAGE_FORMAT; orce_ncci)
      ncci = forc, byte *fncci_ring_li      Id, PLCI   *plci);
stati" mix          leping_bug = 0;

static wREQ;
      SPOOFING)_ncci, i, k, j(plci,ESC,"\x02\x18\ */
id  , byte a)))
    {
 nel, no B-L3 */
    .info[i+5]!=plci,ESC,"\x02\xwsssssssections (cn :    2.1
 d for B3 connectionssg->
        plci->Sig.Id = 0xff;
        Info = 0;
      }

      if(!Info && ch!=2 && rs that are sencci(plci,ASSIGN,0);
adapter;i++) {
      if(a| ch==4)
        {
          ifchannel, no B-L3 */ }
         plcve_start(v
/* Application Grou   {
        if(ch==0 || ch==2 || ch==3 || noCh |oof"));
            send_req(plci);
            return fal          }
          if(ch==4)add_p(plcI,p_chi);
          add_s(plci,CPN,&parEQ;
       ataAcklci->internal_co }
 mman.Id, PLCI   *plci);
sSA,&pLCI *plci, APPL *appl, API_PARSE *parms)
{]);
          ion Neteq(plci);
        re18\xfd");  /d_req(plci);
        re
          add_aAY_S noCh || ch==4)_PARSE *parms)!dir)sig_req(plci);
        return falection            plci-ler),
   of the GNfo[j];
  }
 x_ out->parms[ out->parms[i].info = NULL;
  !dir)sigsig_req(PLCI   ciput->parms[cip *, Pd send_re  *i",
  X_MSG_SIZE) {
    dbug(1,dp* 1 */
  "",*, APP      id     pPARMS+ 19 #d_feae    engthIDS 31= 0;
  do
 retuprintf(");

  ifBIT    gth)
  {
    i[4(&ai->info[1],(wmultirer_,dprintf("aiULTI_IE>length,"ssss",ai_parmstaticms 
      dbug(1,dprintf("ai_parms[0].ss
   gth=%d/0x%x",ai_parms[0].length,GET_WORD(CiPNrms[0].info+1)));
     ength,"ssss",ai_parmFIRM, Irms[0].length)
      {
   = in[ai_B3_&ai->info[1],(esc_chug(1"o;
    out[i%x",ch)    ad   }
    }
  }

ptyheck      }
    }
  }

  ifcrut[i]  }
    }
  }

  if       ci->Staai->info[1er_res}y[256 if(aChannel(At dbug(1
       send_data= in->paer = Num1o;
  = in[
  {_REay B3 in->pa6\x14ile (in->parms[i\x08o;
    out[ih"
#mreleu B3 connefo =fo = in-ms[i].info;
  && (SCai->TYPE_defs.b, PLCIlai, Lu fore  *, byt, ar,chaIE haconneb ai = &rd, nclude  j fintej=0;
    add_s(pARSEth));

  if(ms[5]);
  increESPO          i+
  }MASKituncci a  *, A voibeca);
s, C10 () &&}
  atibfo_Ma----son);
stat/* (se    },_Mct_rBit 4,cilitieIE.ci=%x"j=0;_extendetype) API_PARSE msg&pardistrgth=%_id B3 d_p(plci,C{(!api_parse, CPN*);
    DSA, Otf("BC, LLC, Hx",R
   CAUSErintP, DT, CHAsk_table[b >UUI,ransG_RRnd_mask_NRct));
 HI, KEY 
  ind_AUct));
LAWsk_table[b >RDN, RDXnd_maN_ci))RIN, N  if(Ict));
 Rsk_table[b >CSTct));
PROFILEg(1,dpri
    add_s(,se iftic wchi[i+3] = 14 FTY replstat 
     oid adjust_b1_fac18 PI    plci, &parLAWai = &parms[9d dtm

vod OAD     g word,1,dp| (((fuizeof(dworelseisai_parIE nowai = &pardistri_parms))
 GET_W {1,add_esc_t);
, &parms[5]pi           siPIeq(plci,HANGUP,0);
i_par     }
     OADeq(plci,HANGUP,0);
ai_par     }
     
   SSEXT} }
     , &parms[5]D(ai_par        add_p(pl1_ERRORC,esc_gth)
  {
caue Free Software 0;
  do
  .h"
}
    add_s(pms[i].inms[i].info;   
 sta.h"
#lengthlci);
 ) msg_F  plci->appl rms[ms[i].inh;
  } while (in->parms[i++]));

  for(ng n_Err  plci->appl a Id, 0, "w"7 */
      add_p(plcTHER_APPL_CONNECTED);
      }
   
  staISCONNECT_I, I1h;
  } wh;
  } whi
sta dbug(1,dprintfask bit 9)force_m     /)))
           *);
statiedistrvoid sig_rw       send_req(plci);
                  }
        er[i].plc>c_ind_mask_port(i] ==0  ||}
  }
  d_in_write_);

void   caI_PARSnd_queue[iat,m    CI   *, ONNElci);
 byte reset_b3_re].ma
       PLCI         for exaR   *, PL              j].Sig.Id) return 1;
        }
      }
    }
  }
  api_remove_compInfo)
          {igInd-------------------------i_parse(by,---------%d,Disc    cl----------                             HANT-*/

static voiplci->cif(Rup     j trl_tim    -------------             }
CONNtotyturn 1;
 id   sendf(A = add_b23(plci, &parme Revis(error from add_b23)"))=2)
     ntroll  Info = add_b23(plci, &parms[1]);
          if (In    {
        plci->tel = 0;_ncci, id dtmcover  *, PLCI    layPARSter[i = 0
  w_remove(PL    {
        plci->tel = 0;==1 || (msg->headerlity_req(dword, woce/
  {aCAPI_    }
, byte Rc);
sci, &parmt(a, plci    {
        plci->tel = 0;msg(parms, "word, byte *, ...);
void   * TransmitBufferSet(A->header.command)      if(ch==0 || ch==2 || c   appl->DataNCCI[i] =t fax_head_line_time (char *buffer);


/*tate = OUTG_DIS_PEfo = add_b23(pections (ch=i,&parms[5]);
      _p(plci,CHI,p_chi);
         rintf("NCCI mI,02x",
              ncc |N, 00,  /* 0 = ch;
        }
     0;
          if ((API_PARSE *);
c byte test_c_indTER   *, PLCI   *, APPL   *,02x %02x %nd_req(plci);
    }
  }
 , APPL   *, A, dprintfon Net            clear_de))[itie* 15 *j=0; AdvCi, byno     {n,ENDING ||elsew, byt     vecci] = 0;
facilitie-1));_parpla->Info_Ma IEource, LL_R.APPLgroid ind.    API_PARSE msgig_rel->Id-1))    /* 14 i->spparms[5]),
  +          G   *m;
    API_PARSE msg i;
dPic vo->msgrms[5]);
   , _OTHER_APtic vo      dbug(1   {
I, Id, 0, "w", _OTHE     rn 1;
}

s;
        }
      }
    }
  }
  return 1;
})];
    DIVA_CAPI_ADA;
        }
      }
     }
  }
  return 1;
}add_ai(plc)
{
  dbug(1,dpLCI *plci, APPL *appl, API_PARSE *msg)ject = Ge disc     siI, Id, 0, "w", _OTHE       ber, DIVA_CA;
        }
      }
   ,ch));
 x %0b & 0x->leng  if(plcAPI_PARSE 8 if(aCONNECTEAPI_PARSE2*msg)
{
  1,dp(1,dprintf  *, blert Call_Res"dprintfd b)   {
onnect= 0)
urn 1;remove_start >> 5] &= ~(1L <<  Info = aAPI_PARSE ; i++) {
    i-----*/

exte
           &    8);
_ADAPT[i], _DISCONNECT_tf("plc  adiIE(PLCI  ueue)))
      {
  /* bi, botalADER        for(i=0; i<max_app /* 7 */
  mp_c  for(i=0; i<max_app)
     ofe (( IEsfor(i=0; i<max_appIE1tion[i], _DISCONNECT_tf("plcS_I, Id,eq_in) pl res              T_I, I2, 0, "w", 0);
      }
      plci->2tate = OUTG_DIS_PENDING;
c byte test_c_ind_mask_biti)
      plci-I91\x90\xa5"     }, /* 0  { "\x03\x91bbbbSbSixer_indicationdMultiIE(PLCI sk_table[b2+1+1+)
  {
            law[0]plci->commannd,           sig_req(plci,HANGUci->onnectENDING;
 law       Info = _d);
re *, PLCIaddturesaIVA_fo
      zeof;
  CAPI_MSG   *m;
    API_PARSE msgT_R;
 I_ADAPT_PARSE 5][1]);KEYpplic       else
   nl_remove_i(PLCI_PARSE 1{
    UUI     mixer_remove (plci);
print   "ddCCI (move__CONNECTED);
    L *appl,  brnfo_Mai) && !a_ACCEPT;
);
  0 */
 
};
f uci,OAor ac */
 RSE *ua plc("plctatic (j rd    if(tes /* 7 */
  _in_rese = INC_DIS_Pifci->msg_ine = INC_ci_ptr;
  word ncci  if(tess(pl;
   Law()       d
   ((woi   *, PLC    if (plci->NL.Id && !plci->nl_reACTIVE_I7]);
       <4->State==INC_(plci,HANGLCI   *plres(dword I2, word Num------------------, "w",;
stat   add_p(plci,LLI,uf(plci-e Revision :   lci, API_SAVE   *----------------;
    PPL *appl, API_PARSE *msg)
{
  dinfo)<29) {
     rms[7]);
        ad4 return 2;
}

s==ms[0].info);
       annel_can_xon (Parms[0].info)]);
        }
        add_p(plci,UID,"\x06\x43\x61\x70\x69\x32\x30");
        sig_req(plci,ASSIGN,DSIG_ID);
      }
:
      p lert Call_R}
       (plci->spoof (1,        dbu[%06x] CardP      : %lx    || plci->State   *, worUnMapder.len(dwo    Id),*/
  { "\x02&)
    {
    6])sk_table[bpty (plci)) {
        if10]_empty (plci)) {
        if14plci->State!=SUSPENDING)plci->State =8IDLE;
        dbug(1,dprintf46])Check(DIVA_ULL;      }G /* coO/
  {  if(>AdvCo00ff      a->i);
       B1_Pbled)
   }
  }
  re3urn 0;
}

static byte l2sten_req(dword Id, w1fdrn 0;
}

static byte l3sten_req(dword Id, wo0b7Lemove(plci);
        }
      }
    }
 pty (plci)) {
        if(pl G)
       GL_BCHANNEL_OPERA;
  \x89\x90"         tatic byte listen_req(dword=SUSPENDING)plci->State = IDumber, DIVA_CAPI_ADAPTER *a,
		    ;
        dbug(1,dprintf("chci, APPL *appl, API_PARSE *parms)
{->channels));
        if(!pl   Info = 0     }, /* 1 */
  { "\x",GET_DWORD(parms[1].infoplci
    if (a->In        }, /* 21 */
  { _in_wrap_pos,ACTIVE_     }, /* 1 */
  { "\x02\x88\x90",         "ECHa->max_p DIVA_CAPI_ADAPTER   s */
      a->Info_Mask[appl->I|= 1L    PR)
    heck if extern0");
        
        }
      }
    |== _Wheck if extern\x89\x90"         , PL=  0x10;   /* call progression infos    */
    }

    /* cRTler ((bytsten and switch listen on or off*/
    if(Id&EXT_CRTI_PARSE *s */
      a->Irtp_primary_payA_CA1] & 0x200){ /* early B3 con50].info);
    ;
        for(i= mixer_rem[3].length>=i && i<22;i++) {
        );
  C) {
        dummy_plci.State = IDLE;
        a->codec_listeT38ppl->Id-1] = &dummy_plci;
        a->TelOAD[0] = (byte)(paT38->TelOSA[0] = (byte)(parms[4].length);
        for(i=1;parms[4FAX_SUB_SEP_PWc_ind_mask_en and switch listen on or off*/
    if(Id&EXT_Cnfo = 0x2002; /->TelOSA[0] = (byte)(parms[4].length);
        for(i=1;parms[4V1length>=i && i<22;i++) {
          a->TelOSA[i] = parms[4].V1fo[i];
        }
        a->TelOSA[i] = 0;
      }
      else I */
 TONESPONSE,   en and switch listen on or off*/
    if(Id&EXT_C(a);
  re->TelOSA[0] = (byte)(parms[4].length);
        for(i=1;parms[4PIAF        "s"en and switch listen on or off*/
    if(Id&EXT_C
  woo[i];
        }
        a->TelOSA[i] = 0;
      }
      else Info PAP,    dworord i;
  API_PARSE * ai;
  PLCI   * rc_plci = NULL;
    API;
  for(i=0;i<5;i/* clear listen */
      a->codec_listen[appl->Id-1] = (PLCI   OW_ack_out =  }
  sendf(appl,
        _LISTEN_R|CONFIRM,
     OWNo[i];
        }
        a->TelOSA[i] = 0;
      }
      else Info NONSTANDAR/* wrong controller, codec not supported */
    }
    else{    plci)
  {  ;
    i    p +=1;
      break;i);
        }
      }
    }
  }
  ret7rn 0;
}

static byte listen_req(dword Id, wor*plci, APPL *appl, API_PTER *a,
		       PLCIa*plci, APPL *appl, API_PARSE *parms)
{
  word Info;   if (a->Info_Mask[appl->Id-1]&, (wor8\x90",         "HARD */
                 /* call progression infos (Q,0);
      send_req(plci);
 ;

  InfoQ,0);
      send_req(SOFT */
 SEND |* User_Info option */
      dbuRECEIVEch==3 || noCh |arms[1].info)){
      if(a->prof  dbug89\x90"       on Net;
      sig_req(plci,INFO_RE~Q,0);
      send_req(OOB_ONG_IDEPI_SAVE     if(plci->State==INC_PENDING
    || plci->State==SUSPENDIN {
      if(c_ind_mask_emarms[1].info)){
      if(sk_tabletatic byte listen_req(d   sig_req(plntf("OvlSnd"Q,0);
    }
    elsePARSE *parm   si     }, /* 1 */
  { "
           }
IVA_CAdbugci, i))        /,   ,      ct_req},isG_MESSantion      ||
 urn fal, "w"!CT_IILITY_I|RESPOci, &parmrror from AdvCod SendM       if(ch=L_CONNECTED);
    add_*);
st      add_rror from AdvC|==get_plci(a)))
    {
      e connect PI*);
s1i-1];
     get_SSExtInd(
       a  {
      ai_parms[0]NCR_FAC_REQSCONNi->appl    nl_,  appl->NullCRource fiVSbut WReq   ri->appl = applD(ai_parms[0]ppl, A
        but WIource, worL   *, b * adaptecci;
  CAPI_MSG   *m;
    API_PARSE msgumber, DIceCh   if(plci            *format,  = add_b23(p void rtp_co,ch));
[T_OF_PLCI;]&0x1RMS+1;jLCI   *plci, byte Rc);


static int  diva_get_dma_descrapiRegister(word)torect_b3_ri_load_msg(APva_get_dma  return ect_b3_req== {_DISCONlci->State==INCn 0;
    SetV  {_ct_b3_re    dbug(1,dprier,"w",0);
ci)          }, /ation coni)
{
 te();
  return     nl_rd/or_ncc      /*             dbes        espon, &po[i] _= ncci_co_e)
      CT_B3_I|fo =       
    else  if (!Info)
          { PLCI or e     {
  eserve_ncci)
{
  DIVA_CAPI_atioONNECTC_LABdiscorn 0;
     }
  else
 , _DISC6],
static wo&
       1]) RONG_MESSAGORMAT;
              "w",Info);
  }
  recci = 1; ncci < MAX_NCCI+dprinSKnd_queue  plciyte info_res(>c_ind_m o);
  }
  /* 02x %02xbug(1,dprintf("inf4_res"));
  
  if (ncci)
  {
    if (a->ncci_plci[ncci]  (1L << (b & Id)
      {, _DISRONG_MEnd_req(plci);
    }
  }          "s",     r(i=0;i<5;i   pupplurn 1aryde)
    dtmf_pa        bug(1,dp ncci_fREJocalInfoCon"));
    sendRmf_pai_load_m       5at of api              IN_QUEUE_SIZE but WITH _ALERT_IGNTY OF ANY KIND i_riEXECU     app     THREE_                di==INC_CON_ 0; j length);
staticFIER);
         IN_QUEUE_SIZE;
x200)
  ,dprintf("NCCI mapping releancci_ma*
  Thix200)cSupport(,0);
  i, a->ncci_ch[ncci] *, ));
      a->ch_ncci[a->ncci_ch[ncci]]Id,
       5]==;
      if( plci->spoofed_msgl == ADV_VOICE &&x1f))----word Id, PLdec support */
      if(Id & E
      plci = &a->plci[i-1];
      plc= 0x3f;
              for(i=0; i+5<=
  return ret;
}

st{
      In+_CONNECT_Bd Id, PLCI {
      2]= plci->intern
{
  word Info = 0;
  word i] == 600|       Sreq;
  lnoCh = true;
      if((ch==0 || ch==2 l == ADV_VOICE && a->A 0))
 gth - 2);
           i_plci[ncci] = 0;
        a->_state[ncci] = IDLE;
         {
et = 1;
  {
    if (a-I,->ncB3 conn3,     pl    "\x02\x88\x90      else ch=0xffff;
  0].info+3);SSreq;
  long relatedPLCIvalue;
  DIVA_CAPI_ADAPTER   * relatedadapter;
  byte * SSparms  = "";
    byte RCparms[]  = "\x05\x00\x00\x02\x00\x00";
    byte SSsInfo = 0;
  word i    = 0;

  , res, C2 ,                  }
                }
                m = 0;
              }
              f (ch_mask == 0)
  
              {
               e
    {
11];
  PLCI   *rplci;
{
                Info = _WRON
           && it 9) no release--*/
extern byte _code)
  ;
      dbug( INCLUDING  Info = _WRONG_MESSAGE_FORMAT;
    dbug(1>DataFlags[iCFU     }
        SSreq = GET_WORD(&(mB     }
        SSreq = GET_WORD(&(mNch, for
        SSreq = GET_WORD(&NUM INCLUDING ci_ptr->DBuf {
          cas_code)
    {
          cas>DataFlags[        plci->Stati)
{
  DIVA_CAPI_ADSE dummy;

  dbug(1,dprintf("facility_req"));
  }
    else {
    * relatedadapter;
  byte * SSparms  = "";
    byte RCparms[]  = "\x05\x
              add_p(0\x00";
    byte SSs_ALERT) {
      Info = _*) msg)[i];
        Info = _WRONG_MESSAGE_FORlci->spoofed_msg==SPOODeact_DivR   *, PLCI   * }
    else {
 0]=0x9         PUT_DWORD(&SSstruc3[6],6dd_p(plci,CH\x43\x61\x70\x69\x32\x30"x1f))i_ptr->DBuffer[ncci_HANNEL+1)
          {
      MAT;
          break;
        } 9) no release after a   {
              PUT_DWORD(&SSstruct[6], MASK_TERMINAL_PORTABILITY);
              SSparms = (byte *)SSstruct;
              break;
  AR byt     }
            rplci->inte    SSreq = GET_WORD(&(msg[1].i1].info[1]));
        PUT_WORD(&RCparms[rms[1],SSreq);
        SSparms = RCparms;ord i;

  dbug (1, dprin
    
              PUT_DWORD(&SSstruct[6], = &parERMINAL_PORTABILITY);
    7         SSparms = (byte *)SSstruct;
     }


static void cleanurn false;
            break;

          case S_LISTEN:    {
   ,(word)parms->length,"wbd",ssNumrms))
              {
                dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
      ug, Id)urn false;
            break;

    case S_GET_SUPPOle[d & 0xf];
          CBSbp->info            PUT_DWORD(&SSstruct[6],ect_req(dwordL_PORTABILITY);
       dbug(1,dprarms = (byte *)SSstruct;
              if           if(a->Notification_Mask[applCES:
           SK_MWI) /* MWI active? */
         *,            PUT_DWORD(&SSstruct[6], MASK_TERMINAL_PORTABILITY);
              SSparms = (byte *)SSstruct;
      i,CAI,"\x01\x8          if(a->Notification_Mask[appl_plci(a)))
        _MWI) /* MWI active? */
 ping no app            PUT_DWORD(&SSstruct[6],b)))
              {
            8   rplci = &a->plci[i-1];
                rplci>DataFlags[urn false;
            bre"));
    Inf false; }
    else {
 6GUP,0);
    nlS_]);
    /* dir 0=DTf (ncci)
  {
    if (a-e
  cci] OOK_SUPPOR }
    else {
   {
        ---------------------copy of the GNU  Info = _WRMWI("NCCI  {
          Info = _WR    IRM,
        Id,
      plci->SuppSta        "w",Info);
  return ret;
}

stat;
}

static void cleaintf("disconn_REQ;
              add_s(     }
            API_PARSE dummy;

  dbug(1,dprintf("facility_req"));
  for(i=0;i<9;i++) ss_parms[i].length = 0;

  parms = &msg[1];

  if(!a)
  {
    dbug(1,dprintf("wrong Ctrl") DIVA_CAPI_> 5] &= ~(1L << (b & 0x to the Free(&parms->info[1],(word)parms->lefHOOK_SUPPORT);
        break(plci && plci->State && p
              for(i=0; i+5<= && plci->State && plci->SuppStrms[11];
  PLCI   *rplci;
eived a copy of the GNU anslatio:2]);
_res          else(j = 0; j {
         i;
  API/* wrong state   k < 8;/* wrong state  (--p) = te = HOLcSupport(0]= MASK_TERcSupport(3]=6;
  g_req(rplci,ASIGN,DSIG_ID);
                           /* wroci = &a->plci[i-cSupport(      (j = 0; j            {
     [ncci] = IDLE;
       {
  word Info;
ng state           *}
              }
5rn false;
       else pllci, byte   *      plci->command = C_RETRItablEQ;
              if(plci->spo-------------------POOFING_REQUIRED)
                 bre               plci->spoofed_msg = CALL_RETRIEVE;
                plci->internal_comm k < 8;= BLOCK_PLCI;
                plci->command = 0;
                dbug(1,dp          }
                 plci->spoofed_msg = CALL_RETRIEVE;
                plci->internal_comm(--p) = = BLOCK_PLCI;
                plci->command = 0;
                dbug(1,dp  InEVE;
                plci->internal_commADc byte info_  a->ncci_plci[ncci] = 0;
        a->ncciendf(-*/

word------------------------  sendf()s->len             plci->command = 0;
  
                plci->command = 0;
               g(1,dprintf(API_PARSE dummy;

  dbug(1,dprintf("facility_req"));
 cSupport(   * relatedadapter;
  byte * SSparms  = "";
    byte RCparms[]  = "\x05\x(plci && plci->303ic bytTime-out:
        didord, DIassin_queue)))
 SUSPEND,0);
              plci->Stilit  }

 itioni     = 0ch_mask |= 1         "w",Info);Support(6a->Ad  api_parse(&parms->info[1],(word)parms-gth,"ws",ss     }
     "\x02\x88\x9nded_features = 0 dbug(1,dprintf("alert_req")NECT_I, Idsuccoid o = _WRONG_IDENTIFIocalInfoCon"));
    sen)
    port     {
  return false;
}

static byte alALERT) {
      Info = _WRONG_STATE==INC_CON_PENDING) {
        Info = 0;
      STATE;
      if(plci->  plci->State=INC_CON_ALERT;
        add_ai(plci, &msg[0]);
        sig_req(plci,CALL_ALERT,0);
        ret = 1;
      }
    }
  }
  sendf(appl,
        _ALERT_R|CONFIRM,
        Id,
        Number,
        "w",Info);
  return ret;
}

static byte facilitation conFIRM, Id, Nu!=(plci,&parm                if(plci->spod_s(plci,    if(api_parse(&parms->info[1],(word)par     if(plci->spoofeptr = &(a->ncci[ncci       plci->ncci_r OKR   *, PLCI  ARSE * parms;
    API_PARSE ss_parms[11];
  PLCI   *r>TelOSA[0ONTROLLER)
              {
   but WITHOUT A2x-%02x" = 0;
        dbug(1bug(1,d3nfo = 0;
        LCI;
                plci->command = 0;
  byte reset_b3_rerong ONR   *, PLCI   *   ""           ormat wrong" = {0x01,0x00};    rplci->Id = 0;
              a->ncci_plci[ncci] = 0;
        a->ncci_state[ncci] = IDLE;
       fo = _WRONG_MESSAGE_FORMFFT;
                bre }
      }
    }
  }
  = 0;
  word i    = 0;

  word select_PARSE * parms;
    API_PARSE ss_parms[11];
  PLCI   *rplci;
length)
        {
       
  dword d;
    API_PAR Info = _WRONG_IDENTIFIER;
  }

  selector = GET_WORD(msg[0].info);

  if(!Info)
  {
    switch(selector)
    {
      case SELECTOR_HANDSET:
        Info = AdvCodecSupport(a, plci, appl, HOOK_SUPPORT);
        break;

      case SELECTOR_SU_SERV:
        if(!msg[1].length)
        {
          Info = _WRONG_MESSAGE_FORMAT;
          break;
        }preserve_ncci)
{
  DIVA_CAPI_ADs[2]);
    ommand =   = 0;

  w2urn false  "w",Info);
  }
  ree S_HOLD:
            api_parse(&parms->info[1],(word)parms->length,"ws",ssplci, APPL *aif(plci && plci->State && plci->Su    }
        SSreq = GET_WORD(&(msg[1].info[1]));
        PUT_WORD(&RCparms[1],SSreq);
        SSparms = RCparms;
        switch(SSreq)
        {
          case S_GET_SUPPORTED_SERVICES:
            if((i=get_plci(a)))
            {
              rplci = &plci,ASSIGN,DSIG_ID);
              send_r       case S_LISTEN:
            if(parms->length==7)
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbd",ss_parms))
                   
}

static_FORMAT;
                break;
     ter;
  b3]=if(plci->S-3    i+,dprintf("alert_req"-m->iific */
/*-----(appTER   *, PLCI         }
            }
            else
            {
              Info = _WRONG_MESSAGE_FORMAT      plci->ptyState = (byte)SSreq;fo);
            if(ommand = 0;
              cai[0] = 2;
              switch(SSreq)
              {
              case S_CON[appl->Id-1] & SMASK_MWI) /* MWI active? */
            {
               plci->ptyState = rplci->appl = appl;
     ak;
              case S_CONF_DROP:
                  cai[1] = CONF_DROP;
                  plci->internal_commI,"\x01\x80");
                add_p(rplci,UID,"\x06\x43\x61\x70\x69   case S_CONF_ISOLATE:
         }
              elak;
              case S_CONF_DROP:
                  cai[1] = CONF_DROP;
                  plci->internal_comm          break;
              }
              rplci->internal_command   case S_CONF_ISOLATE:
              send_req(rplciommand = 0;
              cai[0] = 2;
              switch(SSreq)
              {
              cas"));
    Info = _Wter;
  bi] =
    i+,dprintf("alert_req")RT) {
        {
 }
  else
 ter;
  be S_CONF_REATTACH:
            if(api_parse(&parms->info[1],(word)parms-S];
  ter;
  ",ss_parms))
            {
              dbug(1,dpr plci->SuppState = H        plci->SuppState = HOLD_REQUEST;
              plci->command = C_HOLD_REQ;
              add_s(plci,CAI,&ss_parms[1]);
              sig_req(plci,CALL_HOLD,0);
           case S_RETRIEVE:
            if(plci && plci->State && plci->SuppState==CALL_HELD)
            {
              if(Id & EXT_CONTROLLER)
              {
                if(AdvCodecSupport(a, plci, appl, 0))
                {
             {
==1 &&     API_PARSE dummy;

0]>=0x12        "w",Info);
  return ommand = 0 add_s(==1 && Lci_mapping_bSreq;
  l;
          s[1]);(app %ld %08l        {
     Id,
    
          5   cai[0] = 2;
              switch(SSreq)
              {
  MAT;
  }

  i    }
  N(forA_CAPI_k_bit[e S_HOLD:
   /* 2&SPL *alci-lci->Sig.Id
      || (plcd_mask_bit (PLCI   *plci      {
     l;
  h"
#incon MextendeWPI in Lch=%d" APPL   *, API_PARSE *);
sta && plci->State && plci->SuppState==CALLf(parm        byte * SSpar------------------------------------- 0xff)
  {
    dbug(1,dprintf("D-c                if(AdvCodecSupport(a, edPLCIvalue = GET_DWORD(ss_par
          t(a, plci, appl, 0) )
        {
          plcitor = GET_WORD(msg[0].info);

   byte disconnect_res(dwordid clear_b1_conf      {
             iESSAGE_FORMA, word Number, DIVA_CAPI_ADAPTER   dvCodecSupport(a, plci, appl,         if(parmvalue = GET_DWORD(ss_parms
            /* controllSE *);
static byte reset_b3_e
    {
      f(plci->Sttatic byte SE,  knowledg ai = &parms[9]>Info_Mask]_R,       n Fi_plci                */
/*_)->m 2nd PLCI=PLCIxfRMS+1;j++eue)))
       || (((byte   *) mdtmf_parert_req(dwo       }
         ));
                rplc 1L << i;Info_Mas0]       }
           1]=rmat        w  /* Fi_ptr;
 ert_req(dci, byte b1_res          sNNECT_R;
      plci->= appl;
      add->St  /* wrmed/ore---------*/ ->d plci
   possi _WR;
        PLCI   *, APPIDENTIFIe MapId(Id) (((Id) & 0xffffff00L), APPL   *, API_PARSE *);/
                {
                       {
                  Info =OK10;                    /* wrong state           */
                  break;
                }
            PARTY>Id-  }
              }
              else plciel = 0;

              plci->SuppState = RETRIEVE_REQUEST;
              plci->command = C_RETRIEVE_REQ;
         Id,
          N6a)))
    {
      Info = 0;  dONG_MESSAappl->;
          i);
            }
     d((bytePartyID        {
    g(1,dprintf("D-channel X.25 plci->NL.Id:%lci);
            }
            else -*/

static void set_group_in dbug(1,dprintf("Spoof"));
          else
              {
                sig_         plci->spoofed_msg = CALL_RETRIEVE;
           send_req(plci);
                returnelse Info = 0x3010;                    /* wrong st          dbug(1,dprintf("rplci:%x",rplci));
            dbug(1,dprintf("       {
                 plci->internal_command = BLOCK_PLCI;          dbug(1,dprintf("rplci:%x",rplci));
            dbug(1,dprintf("
            if(parms->length)
            {
              ind PLCI=PLCI"));
              rplci = plci;
                }
            ->length,"wbs",ss_parms))
              {
                dbug(1,dprintf("format wrong"          */
          adapter->plci[i];       dbug(1,dp);
               else pl4;
      /* 
            dbug(1,dprintf("SSreq:%x",Sadapter->lci->appl));
            dbug(1,dprintf("rplci->Id:%x",rp4                }
            Id)
    {
      if (!plci->           plci->command = 0Info = 0x3010;                    /* wrong state           */
        nference Sizi)
{ETA  /* wrong sta    ERASE
  dLINKAGEI          ditate={
   yte);ING  }
              }
i->spoofeg = CALL_RETRIEVE;
  

              plci->SuppState = RETRI           rplci->vsw          plci->command = C_R           rplc2x %02x %02x-%02x"i->internal_comm;
                rpl   plci->vsprotdialect=0;

    ;
           msg)
{
  word Info;
itchstate=0;
                rplci   {
                rplci->inter0;
               urn false;
  {
              plci-ommand = 0;
  nternal_command = 6  /* controll     }
  7  /* controll(PLCI   * plci        rplciwd PLCI=PLCI"));
   s[2]);
              pw             }
          case S_Rh)
            {
              Info = _WRONG_MESSAGE_F
   AT;
           {
                if(AdvCodecSupport(a, plci,  state           
              for(i=0; i+5<= - 1) */
            if (((relatedPLe supporpController ((byte)(related     nnel_id_esc (PLCI   * || (MapController ((byte)(relatedPLCIvalntf("explicit invocation"));
      else if(ch==1copy of t          d_queue
  ret cau     nl_r  if(p      au

static void a
  {L3
    i |  serd);

/;
        I=PL=0)false* rel];
    e   * *, byte);
static v    {
              d              /* eari, t_s_PLCI;
        _VOICE && a-gth==7)
{
        ;

  dbug = d_queue[ppl =at[i]; i++) {
    ii;

  dbug (1, dprintf (              if(AdvCodecSupport(a, plci, appl, 0))
       break;

    ERVICE,0);
    ACK*/
  "", */
              break;
            }
            break;

     ueue[i] != NULL)
ord, word, DIVA_CAPI_ADAPTER --------*/

word api_put(APPL   * h    _CAPIe, Cambridge, 1],(word)parms->length,"wb    plci->i        send_req(rplci);
              return false;
            }
            else
            {
              dbug(0,dprintf("Wrong line"));
              Info = 0x3010;   API_PARSE *msg)      /* wrong state           */
              br    plci->indbug(1,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
      ALL_DEFLECTION:
            if(api_parse(&parms->info[1],(word)parms->length,"wb    plci-ss_parms)parms[2].info[0]));
            plci-ci->internal_command = CD_REQ_PEND;
            appl->CDEnable = true;;
          VA_CAPI_ADAPTER|byte   *);
FOst_bapterN6\x00\x00\x00\xc_plci,CPN,&msg[0]);
      add_ai(rc_plci,n 2;
}

static    cai[2] = plci-LCI   *plci, byte Rc);


static int  diva_get_dma_descriptomand_queue[0] = comm     return false;
     /* for applic*/
    }
  }

  if (!rc_plci)
  {
    Info = _WRONG_MEbdwwsss",ss_pB2 WARR==B2neral Public  Info = aNY WARR==General Public>header.length + MSG_IN_OVERHEAD <= MSi] =  *    }
    if (!pre Software
  Foundation, Inc., 675 mmand_    );
 d p_length);
static;
  Id = (((dword) ncci) << 16) | (((word)(dvCodecSupport(a, plci, appl, 0))
             plci->msg_in_ add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
              s1],(word)parms->lengt==1 && LiI   if (!               && c void*/
#define DIcommand_queue",
    (char   *)(for more details.
 *
    else if(ch==1cip_SERR   cip(ae disc = (_PARSEect provi
  for(i  dbL<<lci->Stlci[j]);
        }
i----,
  for(i=e==SUcipfo[0]));
r applicI_ADAPTER   *, P->data_ack_peSE *)_in_wri add_e((Id) 3_reqI_ADA_PI_ADAP   callback(ENT; yoTER   *, PLCI   *, APPid  rint
        rroup */
 miz    /IVA_CAPI_ADAPTER tor = GET_WORD(msg[0].inf       }
                m = In_queue))) * applCIP((byte)(rER   .info);
         cai[0] D,ss_parms[5CPN_fil)); okI_XDI_PRO,a,         rms[5 API_info[0RSE *);
static byte liplci, byte Rc); send_req(rc_plci);
 d
  for(iput(  *,==SUi,.info);
         }
    if (!preI_PARSE *);
static byte lici->appl));
 te disconnect_req(dword, word,    ncci++;
        if (n-------------------LCI   *, byte, byIVA_CAPI_ADAPTER  ~(te   *);
static void add_s(PLCI  ));

  Info =((bytde, API_PARSE * p);
static void add_ie= _OUT_OF_PLCI; +
            Mi_parse(&parms->info[1],(word)parms->length,"wbdww    {
              dbug(1,dprintf("format wrong"));
             *plci)
{
  word if a d for for(j=0;ext *, API_PARSEs doneue))oid api    /* NCRAPTER   *, PLCISE, rR_Facilitoid r    tion -{
  boIS_Ps[3].llci, LLCD,"\x06\l;
   I   *, APPL   *,nd) en_chd for _DIV[3].inf connect_b3_re            discoNULL;        }
      }
    } HAND    }
                rpl*, APP {_DISCONack_pending, l_)->manufac"wbdw",ss_parms))
       ON_BOord         facil              dbug(0DISCOword, DIVA_CAPI_A       }
          if(ch==4)add_p(plci,CH  if(r        if(api_              0\x90\xa3",     "       cai[0] = 1;
     *, PLCI       0x1f))) != 0);
}

sSdbug(0bdbug(",    ", mslci-IPse
    {
      Info = Info = _WRONG_MESSAGE_FORMA_req(rplcr (i = 0; ie Id,tyf("connS_DEACTIVATE:
   *, word, byte *, A if(ai_parms[0]             }
ing              *plci);


static vESSAGE_FORMAT;
    2           }
       Subal;
  s->length,"wbdwws",ss_parms))
        3se(&parms->info[1],(wbug(0,dps->length,"wbdwws",ss_parms))
        tatit wronBearerCapaCON_ALE;
              break;
            }
    5         LowL                    Info = _WRONG_MESSAGE_FORMAT;
    r (nt wronHighet_plci(a)))
   s->length,"wbdwws",ss_parms))
  ndf(ap", msg-a->n }
   lengthword)a(SSreq)
              {
          ove_i          Bemove_comp
   ch(SSreq)
              {
          ove_id)[i-1];
 keypa  /* 28 *    0|(byte)GET_WORD(&(ss_parms[3].info[0]    {
    (1,dSION_R = CA            rplci->internal_command = INTnfo = _WROTERROGA/
                  rplci->internal_command        if(api_pa1] x.31 orecond i_pa(SCR_inds->length,"wbdwws",ss_parms))
  ERS:
        _REQ;
      r, DIVA_CAPI_ADAP  Info = _WRONG_MESSAGE_Fi->ap  Info = _WRONG_MESSAGE_Fcation_coefs_set (dword S: /* use ;
      add_p(rc_pUM; /* FuncetupSCONN                  rplci->internal_command = INTERR_NUMBERS_REQ_PEND; /* move to rplci if assigned *);
     Info = _WRONG_MESSAGE_Fle  = true;
      rc_plci->internal_command = C)[k])---------------------------I_ADAPTER   *, PLCI   *, APPRD(&(ss_parms[4].  case S_CALL_FORWARDING_STO;
  for (TER   *, PLemci_m (PLCI  rplci->internal_command = CF_START_PEND;
            rplci->---------*/
/* NCCI allo             (foried  *pl in case used for B3 connections d for Bqueue WRONG_MErd)parms->length,"wb {
  NOTIFY      :
                  ce Revis            rplci->internal_command = CCBed scre          CSAGE_FOR              lled max_D   conn].maPI_ADAPTEINTERROGATE;
         NOSI     l word Numberd_mask_bit (PLCI   *plci[i]))->header.leng              }
       rplci->internal_command = CCBS_INTERRrms[4]);
        add_s(plci,BC,&ata));

 gth)
          tic byte connect_b3_req(dword, word, DIVA_CAPI_x200)
  e(&parms->info[1word, DIVA_CAPI_Aendf(apatic byte dtCI   *plci);

static void dtmf_command (dword***Sd, DIV M/*----           add_p(rplci,  *, PLCI   *, APendf(ap   *, API_PARSE *);
static endf(ap[8]);
        CIP = GET_WORD(paro =  plc          ,0);
        ret = 1;
      }
   |      *, PLCI   *, AP       {       add_p(rplci,UID,"\x06\x43, APPL   *appl, APi,CPN,ss_parS if (!plci->dad_req(rplci);
       ncci = 1;
        wd Info;
  byte reti].iA_CAsg);
s&       a, APPL  ,dbug(1,dprine bchannel_id);
sta_voice_endf(,&bug(1,dprint1LCI endf(apturer_res},
 e bchannel_id);
stachanet = 1;
      }
 mask_bit[ parms;
   }, /* 23%02x2 || (msgFree(APPL   * appl, void   * pword, arly;

static v (g"))i[1] CI  9)    releIND       a             cai[1] = DIVERci, by      nfo_b23_1IP mae bchannel_id);
staid (PLCI   *plci03\x80\_CCBS_I   {
           case S_ax_adapter))
             PUT_WORD%x",R      case S_-------------       case aiPUT_WORD      case S_ }

  selec            else
+;
        if (ncci == MAX_NCCI+resource    */
 UT_WORD(Sreq)
 ci, byte Rc);
static void init
     rms[3].info[0])));   ""             lci->interR   *, PLCI   *,           {
     
              InfstatiBTG_DIord,R   *, PLCI   *, AP
              Info = _OUT plc,   els  * p, word p_length);
static); /* Basic Service */
            add_p(rpln (dword nfo =o a c vo per _ch[ncci] = 0;eqed to = _WRONG_MESS *plci);


static v   i      brmmand   Info = _WRONG_MD(&(ss_parms[3].infoi] = 0;
        }
     D!
                brea    {
              91\x90\xa5",     "\x03\x9     endf(ap*a,
		     }
}


/*------------------------------------------------ss_parms[2].info);
 arms[4].info[0])); /* Ba{
  word Info;
  byte ret] = 3;
                  case S_INTE_ack_command (dword Id, PL   cai[0] = 1;
                add_p(rplci,CAI,cai);
                te, byET_WORD(&(ss_ptic void ec_indiCI   *plci, byte   *chi);
static void m cai[0] = 2;
   nfig (PLCI   *plci);
static              if((i=fy_update (PLCI   *plci, byte o cai[0] = 2;
   c void mixer_command (dword I              if((i=RD(&(ss_parms[4].info[0])); /* Ba          dbug(1,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
          i, byte Rc);
static sic Service */
            S_CCBSCI  req(rplci);
      )           {
                     add_p(rplci,UID,"\x06\x43\xRD(&(ss_parms[4].info[0])); /* Basic Service */
            i = plci           {
                if(api_parse(&parms->info[1],(word)parms->length,"wbwdwwwssss",ss_parms))
            {
              dbug(1,dite_pos)
    {
    et = 1;
      }
  gth,"
/*--------------------------          if(!plci)
            {                               
              if((i=get_plci(a)))
    ---------               cai[0] = 1;
                add_p(rplci,CAI,cai);
                brea PUT_WORdbug      case S_CCarms[3].info[0])));
           ntf(      case S_, PLCI   *plci, byt     add_p(rplc      case S_90],GET_WORD(&(ss_pare;
            }

            rplci->command = 0;
            rplci->internal_command = MWI_ACTIVATE_REQ_PEND;  plci           {
            case S_INT{
    plci->inD(&(ss_parms[6].info[0]))); /* Invocati  plcicai[2] = (byte)GET_WORD(&(ss_parms[4].info[0])); /* Basic Ser   send_req(rplci);
                 add_p(rplciVERHEAD + 3) & 0xfffc;

  else
            {
    ;
}

stat-------------
                {
              atic byte mixer_request (dwg(1,dprintf(ULL;i<relatedadapter->max_plci;i++)
       ncci_ptr->data_pending = 0    }
              rplci*)(l IT_R|R   **, by(byte   *)(plci->msg_in_queue))[i]))->hemitB _CONNECT_R;
      pla->pROGATE:
9er;
      /* x.31 oi] =wwss"     inkLayer? */
      if(ch/* 16 */
  { "\x03\x90\x90\xa3     "\x03\x90\x90\((__a__)->manu(rplci);
     !             {
   S_INolGATEylrintrmaneue)        lci);
 turn 1;
        ch[ch])
            }
       LCI   *plci, byte Rc);


static int  diva_get_dma_descrip        casei->ind supplementary se           brvoid dlci-, by       {
   , bytDIVA_CAn immed    lci_remove*) msg)[i];
          _parse(&parms-ommand (dwo  }
  }

  if (!rc_plci)
  {
   )
            {ig_req(rp      ncci = force_ncci;
    else
    {
S\x01D(&(ss_1]f ((ch < MAX_NC) && !a->ncci_ch[ch])
        ncci 1],(word)parms->lengt,             f("connect_res  iRes"i_parms))
    {
0{
    inm->ici, CA/
           / 0 */
  "");
static] = CCBS_INTERROGATE;
             && i    rRNAL_COMMAND_L (ie[1_ai(9 *chi);
statie_commx80("NC    ;
   PI_ADAPTER   *IVATE_REes (PLCI   * Q_PEND;
                  break;
   mitBPARSE *);
static byte facility_req(dword, CT_R:    _offR   *, PLCI   *,     {
 I; /      =    ------------
          case S_MWI_DEACTIVATE:
            if][2] = {
  { "",                        plci- douunctquiry=false;
             "s",            disco wor      PUT_WORD(&f("if0)=mber     sig_req(rpatic byte disconnect_b3_req(dworparms[3PUT_R   *, PLCI   *, AP          "s",            reset_b      PUT_WORD(&cai[2],Gmmandprintf("format wrong"));
  T;
    (((wocom_applcati                        b {
 APTER '+'.   *,defs.deci        rplci  plci-, by    0x00{ "\wordcatidefs.         cai[1]SE,  NG_R      rd, ;
                  Info = _Wtor = G       =
    _ADAPTER   Buff(word)parms->length,if(api_parse(&pntrolling Use * applif(api_parse(&pdSetupInfo(APPL   *, PLCntrolling User |f(pl a, plci, appl, msg));



      yte);
 LCI   *plci, byte     add_p(if(api_parse(&p    }
            else
(dword,, DIVA_CAPI_ADA*, API_PARSE *);
static byte reset_b S_INTo
            s,  adoutgo       seplci);
            return fals /* uest.    upBS_DEACTIVATE:
    _FACILInnect_b3_r_parsd_req(          add_p(r;
static int channel_can_xopos, i));   appl->appl_CI   * plci, byte ch);
st
          add_p(rplci,CAI,"\x01\x80");
    r */
        g(0,dprintf("format wrong"           s no releas5]_INTI_ADAPTER   *->Max APPL, res, res, C2 "",             cai[0] =f switch(selector)       PUT_WORAX_DATA_B3) || (l [0])));
                add_p(rplci,CAI,cai);
                break;
           ase S_CCBS_INTERROGATE:
                caiid (PLCI   *plciase S_CCBS_I_t[2]  no relea4].info[0])); /* a,
			 PLCI *OAD   siTelOA             i *msg)
{
  dbug(1,tf("ntf("faSApl, API_PARSE *msg)
{
  dbug(SHIFT|6,
   connect_b3_req(dword Id, word IN>number ATE:
  *plci, byte Rc)msg)
{
  dbug(ust_b23_command (dword Id, PLCI   *plci, byte Rc)info[0]))); /* I fax_disconnect_crn (ec_request (Id, Number, a,e
            {
     i;

  for (i_bits, fax_info_change;
  API_PARS);
            }
            else
(dword,w switch(selecto         send_req(rplci)d VoiceChannelOff(PLCI   *plci);
static void adv_voice_T_WORD(&(ss_parms[4].info[0])); /0\xa3",     "0\xa3",     "         IN_QUEUE_SIZE;
  urn 1;
       ,0);
        r   return false;
>ch_ncci[a->ncci_ch[ncci]] =      {
            _mask_bit (PLCI   *plci, wo    if(plciPEND; /* move to rplci if as {
                  mf_c      PPL PPL +                  break;
          0x9Q_PEND;
      n         cai[t_b3_reqpl;
            rplci->number = Number;

            cai[0] = 5;
            cai[1] = DEACTIVATION_MWI; /*<max_adapter         rplling User Number */
            sig_parms[2].info[0]))); /* Basic Service */
            PUT_WORD(&cai[4],GET_WORD(&(ss_parms[3].info[0]))); /* Invocation Mode */
            add_p(rplci,CAI,cai);
            add90 Info = _WRONG_STAT Controlling User Number */
            sig_req(rplci,S_SE         }
  v., APPL not s   return fa-1_resourch)
  DEACTIVATE:
       parms->info[1],(word)parms->lT_OF_PLCI;
 change;
  API_PARS   {
      /* local repln 2;
}

static byte connect_res(dword Id, ci++)
    {
  switch(selectocai[1] = 0x80|(byte) layer 3 connection
         /* move to rplci if assi {
                or B2 protocol not any-               cchange;
  API_PARSE * ncpi;
    byte pvc[2]d mix",Info);
        return false;
    *, API_PARSE *);
statiedPLCIva              ple",
    (char   *)i->State == INC              plc---------*/

static void set_group_in            rd)parms->lengt } whi INCLUD_ADAPTER   *, PLCI   *, APPL   *, 3;
             F_PLCI;
        (&01");
    }4],uld  byte alert_req(dword Id, word Numpl, HOOK_SUProtocol3675 M);
    lci->internal_command     {
 cpi->info[2] || ncpi->This source fi UnMapId(Id) (((Id) & 0xffffff00L) | Un   rplci->appl                                 */
/*----------------:
      p i->Stat>msg_in_write N_D_BIT;
 else
            {
              (FILE_), __LINE__));

  plcion NetD;
            rplci     cleanid   send/
         pi->info[2];
            add_d(plci,2,pvc);
    add_s(pc */
   ms[i].info  api_parse(&parms->info[1,"\x01\x01");
        }
        add_k for PVC */
            {
    
  ret rd)parms->lengtntrollE_REQ_PEND;
ci, ASSIGN, 0);
      }  "\x02\x>req_outPLCI   *plcirplci = &a->pl=    */
/*------plci->SuppStatend_req(rplci);
          return false;
            }
            else
            )->header.co   else
    8          1_resource (d_)->manulse
    9R   *se
    1       _L2           if (ncpi->length >=5 {
    r;
  Id = (((dword) nc((__a__)->mreturn false;
     if ((w & 0arms))
        tupInfo(APPL   *, PLC      rplci->intexer_request (Iber,
        tor = GET_WORD(msg[0].info);

  if(!Info)
  { API_PARSE *);
static byte listen_req(dwo, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *,     else   c = true;
        else
              cai[1] = 0x70|(byte;
  for (rve_ncci)
{
-----------------------;
static           *plci, byte Rc);
static             /* controllused for B3 connections (c);
           a->ncci_ch[ct(APPL   * appl, in      rplci->internal_comma*plci, byte Rc);
static->Id = 0;
             UnMapId(Id) (((Id) & 0xffffff00L) | UnMapController ((byte)                                          */
/*--------------------i, byte pres      }
        }
     command_function)
{
  word [8]);
        CIP = GET_WORD(par0)
                {
            i = plci->nccmand_queu  {                      ber,
        _parms[lli  }
ppli>Id-1] & 0parms--------ES(byte *1,dprica
  wert_req(dwo         {lude elow */   if ! Doq.Dageneror *anPI_PAR  {
        if(t 6)
 TS;
       ((byte) w) != ((T30_INFO   *)(plci->fax_connec & DIVA_CAPI_  if (w & !dd_s( rplci = &a->plci[i-1VELS - 1; i++)
    ult:
                  cai } wh       l = ncci_ptr->datacpi->info[3];
          ppl, API_PARSEncpi->info[2];
            add_d(plci,2,pvc);
            req = N_   *, PLCI   *, APPL   *,ROL_BIT_REQUEST_POLLING |  (a->Info_Mask[appl->Id-1] & 0xpl, HOOK_"        fax* check for PVC */
        ci,ESsed screen_REQ;
      rc_plci->appl = appl;
      add_p(rc_prd)parms->lengt1_ERROR);
       \x06\x43\x61\x70\x69\x32\x30");
      sig_rereq(rplci,S_SERVIC1_ERROR)ed scr->Id = 0;
          sendf(app_R|CONFIRM, Id, Numb3 PRIVATE_FAXER);
          return 2;
       PRIVAT   if(!api_parse-1]}
    I_PAci, byte b       && (GET_W,e ifInfo = ) /* Private n     bommand_queues enable */
    1_ERROR);
        D;
          ate=INC_CON_ALERT;
& 0x0001) !=TE_FAX_SUB_SEP_PWD);
                }
                ntf("confor B3 connec             crintf("bad le,);
    return ,;
  }
  o0; j < el(A{
    i0; j <   caiSdv_n             /* =0;i<5;i++)  ai_par);
staticRONG
  out-TROL_BIT_  caiErintf(options_conn |_bit in_wrap_
              Inf       or (at ytor = GET_W(!api_parse,CAI,cai);
       _compare      onn | plci-  cai[1] =tions | a->rcai[2        {      rintf("NCCI mapp
/*------ite_command);
stat 0xf];
          PN R   *, PLCI  TANDARD)))
    x007         ap  cai[0] _req"            apD);
          i, byte Rc);
statDD_REQ_PEND;
     8:t (plcispla     ->length >= 6f_command (dword Ited_o(%d)i->iails.
 *
  You   Info = _WRON2r;
        options_tablex0_REQ_PEND;

                  {
                    if ((plc16nfo =ct_b3_rci->_conn | plci->requested_optCH      {
      s_table[appl->Id-1   Id_options_tab        &10       else
                  {
          LCI   *plci, byte Rc)I mappintions | a->r
              else if(SSreq=19nfo =Re_in_qu = fD)))
 _conn | plci->requested_optRDAT;
                Info = _WRONG(1L << PRIVATE_F        &4    if (fax_control_bits & T30_CONTROL_BIT_DD_REQ_PEND;
     20T_ACCEPT_SEL_POLLING;
 , APn = I     }
                    w X fax_parms[4].length;
          ];
      chf (w > 20)
                      w = 20;
                    ((T30_INFO   *)(2T_ACCEPT_SELexamprd)parms->length,"                wI= fax_parms[4].length;
                    Sf (w > 20)
                      w = 20;
                    ((T30_INFOi[ncci] == plci->s_table[appl->Innel_x_on (PLCI   * plci, bytci->fax_coi==) /* Private2) dprit    CT_I, d-1));
    dump_c       ">station_is_table[appl->Id   | |i->spoofe                o_buffer[len+ {
       ntf("Conci->fax_co
              &&   plci->fax word Number
                cai[0] =             P  cai[0] ],(word)parmchar   *)(FILE_),CONF,relPLCIs_table[applength    fax_control_        {
     SS | T30_CONTROL_SCONN of the GNU    T30_CONTROL_* as pu0; j < iemmand)    if ((plci-distr (bytdistri=0;i<5;i++) ed_options_conn | plci->requested_options | a->requested_options_table[atic void send_daargems[4    4CI  ];
 ;
static void listNG |      rong nnec;
static wrintf("bd len"ppl->Id-1])
         caId, 0      FORMATSSWORD;
 rve_ncci)
{SERVICE,0);
if(!InfoHANTSERVICE,0);
           db!id
     && (pSSWORD;ntf("NCR_Fi] = 0;
        }
quested_opt                  if ((p(PLCIfer[l << PRIVATE_FAX_SUB_SEP_PWD) | (1L << PRIVATE_FAX_NONSTANDARD)))
                {
                  if (api_parse (&ncpi->info[1], ncpi->length, "wwwwsss", fax_parms))
                    Info = _WRONG_MESSAGE_FORMAT;
                  elseDD_REQ_PEND;
     7nfo = );
   >station_id_len = (byte) w;
caurplci,ASiwwwwested_options_table[appl->Id-10)
                      & ))[j]);
    , "wwwwsss>= 2)ASSWORD;
       {
       
        a->nc          if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[appl->Id-1])
                      & (1L << PRIVDD_REQ_PEND;
     9->requD    ested_op_conn | plci->requested_optiatea->requested_options_table[appl->Id-1] MASK_TERMI             & lci, byte  ts |= T30_CONTROL_Bplci->parms[6o = 0x3010;  or(j=0;j<4;jI,caparms[6]1+jder.command ==  }
      j<     _parsnal_c+j*/
 *, P     rd, word, DIVAk=1,j++        }
   k<
   j++,k      fax_inkplci->f    ted_options_table[appl->Id40    if (fax_contr> 20)
     _connect_info_buffer[len+parms[6control_bits |= T30_CONTROL_B1nfo =ION_REQ_PEve (plci);              }
      uui fax_parms[4].length;
                                  & PASSWORD;
 ts |= T30_CONTROL_Bid[i] conges   }
        ----- }
                  }
      clRDY fax_parms[4].length;
         B      {
                Pl_bits = GET_WORD           for (i l_bits |= T30_CONTROL_B3   {
              plci->rd, DIx_connect_info_length = len;
    N          if (fax_info_change)
          r   {
                if (fax_feature_bits & T30_FEATURE_BIT_MORE_DOCUMENTS)
5nfo =Ktion *F
       [1+i];
                    KE        if (fax_info_change)
   2    break;
             &
  "\x02 {
                    fax_control_bits |= T30_CONTROL_BIT_ACCEPT_SUBADDRESS | T30_CONTROL_BIT_ACCEPT_PASSWORD;
                    if (fax_ACCEPT_POLLING)
                      fax_control_bits |= T30_CONTROL_B(fax_papi_re1tr62]);
   * byte_rem] = ncp_)->m,ch))NG ||[1+i];
                    q97].info[1] >= 2))
           Id, , "wwwws)
    "wwww< if (fo == G
     g.   }
  1 g no
      }
      plci->nsf_control_bits = GET_WORD(&fax_parm          s->leng"wwwws!ci->faxcommanINFO   *)plcId, control_bits |= T30_CONTROL_BIT_ACCEPT_SEL_POLLING;
      }
                    w = fax_parms[4].length;
                    if (w > 20)
                _buffer))->station_id[i] = fax_parms[4].info[1+i];
                    ((T30_INFO   *)(plci->fax_connect_info_buffer))->head_line_len = 0;
        ((T30_INFO   *)(         {
       d IdA_CAonfo[1+i];
                    NADDRESS | T30_CONTROL_BIT_AC      Nx01\x80");
  return false;e) w;
    s(dword Id, word Numbfax_concatis & (1 |= T30_CONTROL_BIT_ACCEPT_SUST_PARSE *parms)
{
  word ncci;
  ACuffer[ncci_p      start_inter     ad Info NG || .e.,0);
 add_ai(plci w;


    API_PARSE f     if (w > ->requEscape         Type,plci, LLC, &parms[word) a->Id) << 8);
  lity_req(dword, wSC/MT[plci]] >= 3))
            plci->nsf_contect_innel_rrnal_command _buffer>16);
  if(plci-%02 * plci, byte cr[len++] = (byte) w;
    _feature_bits & T30_FEATURE_BIT_MORE_DOCUMion_id) + 20;
               !dir)sig_req(pRMAT;
      >ncci_state[ncci] =e_bits & T30_FEATURE_BIT_MORE_DOCUMth;
                       dbugd
     && (pl      Id,
    T;
   TER   *, PLCroadcrms[lci);
                    DAPTER   *    l controller listeNCELLER:
        apr)->control_< w; i++[4].inSERVICE,0);
        L << PRIVATE_FAX_Ninfo_buffer[len++] = fax_parms[5.info[1+i]     a->ncci_next[ncci] = 0;
        }CROUT_OF_PLCI;
   {
  buffe=byte Rc);
static       cai[0] = 1;
j] w = fax_pa&_PARrms[6].length;
                    if (w )
          c = true;
     & 0x0001) ? T30_RESOLi(a))verlap    plcc vo3_prot == 7))
   equested_options==CPN_bits);||ffer[2+w] = ==KE
       t_info_length < NI)
            {
       DSP)
            {
       plcied_options_conn |       if ((plci->requested_optio}

/*------------ API_PARSE *);
static byte jommand_queue[0]))(Id, plci, OK);
    if (plOvl == 5))
    && (plclci->nsf_control_bits & T T30_NSF_CONTROL_BIT_ENABLE_NSF)
  (plci->nsf_control_bits & T30_NSF_CONTROL_al_command)
          c = t       appl = plcicatiPI_PARyte max_app     }
       _)->manuted_options_tabl       & (1L << PRIVATE_FAX_3;
                
 {
   if (((pl(plci->spoofed_msg==SPOOSt    {     {
    i->nsf_control_bitsts = GET_WORD(&((T = fax_parms[6].length;
                    if (                   do
 et_plci(a))                   plci->fax_connect_info_b_mp_c termedistr{
   *, P0; j < LECTOId, 0r[len++] = (byte) w;
          for (i = 0; i < w; i++)
                      plci->faxonn | plci->reax_connect_info_buffer[l_info_buffei, APPLs_table[ap->Id-1])
                    & (1L << PRATE_FAX_NONSTANDARD))
      {
           ect_info_b                      if (api_parseM-IE (&ncpi->info[1], ncpg->heade, APPL   *, byte);
parms[7].lengt, fax_parms))
      dbug(1 }
      | (1L << PRIVATE_FAX_NONSTANDARD)))
                {
                  if (ap   if(plci->spoofed_msg==SPOO[Ind   ch:IEi_load_msg(API              ested_opti)
{
  word ncci;
  A       o_buffer[len++] = (b, plci, plci->f *)(plci->fax_connect_ii->B3_prot == 4) || (plci->B3_prot == 5) || (plci->B3_prot == 7))
      {

        if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[plci->appl->Id-1])
          & (1L << PRIVATE_FAX_NONSTANDARD))
 {
   if (((plci->B3_prot == 4i->nsfi, byte Rc);
statictimization_mask_tltal_co== 5))
    && (plc30_NSF_CONTROL_BIT_ENABLE_NSF)
    && (plci->nsf_control_bits & T30_NSF_CONTROL_BIT_NEGOTIATE_RESP))
   {
            l               pn = offtatic void VSwitchReqInd(PLCI   *plci, dwoof(T30_INFO, station_id) + 20;
            if ((plci->requested_options_conn | _info_buffer))->head_line_len = 0;
       Id,0,"s","");
          else
            sendf(appl,_CONN (&ncpi->info[1], ncpiarms))
            {
              dbug(1,dprintf("non-standard facilities  or wrong format"));
          disconnect_res(dword, word,}
            else
            {
              if (plci->fax_connect_info_length <= len)
                plci->fax_connect_info_b;
          else
        sendf(appl,_CONN              len nnect_info_buffer[len];
              if (plci->fax_connect_info_length  (a->nccii->nsfam is free s_CONTROL_B
      r_SEL_POLLING |
                  T30_CONTROL_BIT_ACCEPT_r[len++] = (byte}
    byte,of*/
               [i][]     0         gthst_b1
  do
  EXTIEst_b2eturn falseplci/UB/PWD enst_b3 1;
    }
  }
  4      ci->aC          6...      s&& (plci->atableT_B3_I|Rrplci = &a->plpl, API_PARSE NSTANDARD))
      {
     )
yte);
statix_parms[7].inBufferSetlci->fax_con }
     0]<6)5].info);info[API_XDI_Pi]
    yte conne;

  if (plcci && nAPI_PARSE = add_x%x)",ncci    Func, byi= 7))
   reserve_ncci)
      plci-    if(_parms[0 {
    "dPLCI_bitser_indicatioACT_PENDIyte co6 */
i->faxPENDING)
, PLCI  on Ne             
ate != INC_DIS_PENDING)
   && (plci->State != OUTG_DISI; /* FunctiNG))
  {
    if(a->ncci_state[ncci]==INC_ACT_PENDING) {
      a->ncci_state[ncci] = CONNECTED;
      if(plf("c;              er.leng;
    return 0; ader.controT,0);
       xff;
        Info = 0;
      }

      ie it aCCID,"\1,dprintf("barintEN_REQ,0);
 cou>requested_Nu controller =                      char *disconnect, &parsgength = 0;nnel =        pl    */
  { 2 , Cect_res(eraxommad----------er[len++] = fax_v120_plci->        2 plc      free srd)(Id>>1ic voinfo[_state[fferFree (plci->appl,
  ,   SUCCESSO   *)(plci->fax_connect_info_b= plci;sages */ION[i-1];
 ,   ng_buO     ,UUI,&a(appl->Id-1));
     ngth)
ROTOCOLINFO  te[n>ncci_stateTIMEct_tN         wENDING))
    {
      a->ncci_state[ncci] = OUTG_D Use"\x0;
      channel_r))
    {
      a->ncci_state[ncci] = OUTG_DIOOest_  }
PEAT|| (a->ncci_
    {
      a->ncci_state[ncci] = OUTG_DUNEXP;
statPLCI   prot == B3_TRANSPAREN----TE_ABORTte[ncci->B3_prot == B3_T30)
DCNstate[ncci] == INC_CON_PEsk, by_WITH_EXTENNSIONS)))
  DTC_UN    else CT_PENDING))
    {
     TRAI    _state[ncci] = OUTG_D = plATES_FAILturn false;
     }
      else
      {
        cleanup_    && ((else
|| (a->ncci_s
    {
      \x88\x90\state[nccci]);
      UI,&a_CORRUPeturn false;
->B3_prot == B3_T30_WITH_EXTENSIONS)))
      {
      I     ->send_disc = (byte)ncci;
        plci->command = rintng"));
(ncpi->info[4]))->B3_prot == B3_T30REJECH_EXTE>ncci_stateINCOMPATIBL voiB3_prot == B3_TRANSPARENcci;
        plci->command = (appl,
       CB3_prot == B3_TRANSPARENT)
        || (plci->B3_prot IS_PENDING;* expliprot == B3_TRANSPARENT)
        || (plci->B3_prot   channelword Number, DIVurn false;
}

static byte disconnect_b3_res(dword I* expliz    LON }

     {
      a->ncci_state[ncci] = OUTG_DIS_PENDI
      cbug(1,dprinf("disconnecNDING)
     || (a->ncci_state[nT;
word Capurn false;
  tf("disconnect_b3_res(ncci=0x%x",ncci));
 SUP nccSORYs(dword        ncpi = &parms[0];
          if(ncpi->lengtug(1,dpr_SCAN_LIN  || (plci->B3_prot ==ER *a,
			      PLCI *plci, APPL *applP   *AFx90\MP|| (a     && ((plci->B2_prot != B2_LAPD) && (plci->B2_prot != B2_LCF  dbEE_SAPI_SEL)))
    {
      plci->call_dir |= CALL_DDCS!= B2_LFT3_T90N    }
    for(i=0; i<MAX_CHANNELS_PER_PLCI && plci->inc_dis_EOM, DIVA_CAPI_ADAPTER *a,
			      PLCI *plci, APPL *appl->inc_dis_APD_FREble[i]!=(byte)ncci; i++);
    if(i<MAX_CHANNELS_PER_PN!= B2_LACF_PLCI-1; i++) plci->inc_dis_ncci_table[i] = plci->inc_dis_ncci_taRT plci-A_CAPI_ADAPTER *a,
			      PLCI *plci, APPL *appl,G_NL;
>info[4]));
          }ER *a,
			      PLCI *plci, APPL *applMCFCI) {
   ));
  LE || plci->State==SUSPENDING) && !plci->channels){
        if   if(plci->channels)plci->channels--;
      for(; i<MAX{
        PER_PLCI-1; i++lci-1dte[ncci] == OUTG_
    if (((pl0x2002       return false         "wse, (word)3, "\x03\x04\x00\x00PWD        return false;
      }
   "wsf, (word)3, "\x03\x04\x00\x00");
     0);
        }
    f("disconnect_b3_res(ncci=0x%x",ncci));
 INVALI_FORexplizF "\x     ncpi = &parms[0];
          if(ncpi->lengt       retuprot !COoluti  ncpi = &parms[0];
          if(ncpi->lengtANUFACTU>B3_prot == 5))i);
      }
      return 1;
    }
  }
  sendf(appl,
      >B3_proNFIrintf("disconneccci;
        plci->command = IS_PENDIFROMci(plci,N_DI   {
      if ((a->manufacturer_features & MV34sted_opRcode)ONesteMAR(relateda  start_internal_command (Id, plci, fax_delse
    t != B3_T90
        start_internal_command (Id, plci, fax_d== B3_T30)
V2d, 0, "
        start_internal_command (Id, plci, fax_dPRIM)
  CTS_IDLE;i,N_EDATA,(byte)ncci);

        plci->adapteurn falsURNAROUND_PO    plci->requestedcci;
       "\x03\x04\x00\x00, fax_dV8,
        "plci,        ;
    if (a-    }D,"\        
  i(plci,UUV_DIGITb3_t90_a_res +  19 i);
 CONNECTED)
r(i= || (a->ncci_stuld te[ncci] == OUTG_COACCETP)
     || (a->ncci_state[ncci] _remove(pl600>ncci_state[ncci] == CON (((pSRC_OR_PAYLOADion *   ||   dbug(1,dp
    InedistruSPOOFf;
       0"           / sizeof     foT_WORDcci_sta    030NNEC }
  ret0i-1] >= MAX_DATA_B3)
     nding;
   MAX_DATA_B3)
        i -= MAX_TA_B3)
    dbug(1,dp->heaapId(Id) (.length   send_req(plci);
            return false;
  ) {
        for(j=0;j<adapter[i].max_plci;j++      k;
      }
if(adapter[i].plc        case S_CCBS_INTERROGATE:
   ord nccurn 1;
  }
  elsei, APPL *appl, APATE:
-Id(NL:plci,  *,(plci->adv_nl)
          {
      {
       nl_rr >= mC_ACT_Pave_msg(API_, 0);
          }
        }
      c*);
stat  else
      {
ber;
 head0fr (at y      INC_DI    ""_rem   0x00E *);
.h"
#RESPONSE))=%d sstate=wof(pi | (((xplici     if ((((CAP       Info = add_b23(pNL, &parms[1]);
          if (Info)
          NL           dbug(1,dprintf("connect_res(error from add_bMapId(Id) (T_WORD(pa    /*------>State==INC_CON_PEinfo_buffer_features (_bits     rplci->inteapter_features ( if (i >= MAX_DATA_ACvoid at[i]; i++) {
    iata_pending)++;

        /* chlci->requested_optionDIS_Pn        max_adapter;i++) {
      if(aing indicator *<bug(1,dprintf("Ncci_ptr->DataAck[i].Number = && ncc           "w       nccyte   *)(pf)
     ing ov    &          N{
        ;i++) {
      ifE_FORMAT!ord ncc(plci->B3_pr      appl = plciAGE_FORPI_ADAPT(((wo------ = CARMAT; ic void VSwitchReqInd(PLCI   *plci, dwo  dbug(1A_CAc voSPENDS));
        if (Info)
          [i].HanADAPTER   ck[i].Number = daLC, &parms[4]);
      /* 10 */
  i = ncci_ptr      U, by (((plc     10 */
     rpli, rB2_SDLClci->imanufacturresourc   br17I   *, dword   Number,
     8command_queu  "ww",GET_CONFIRM== 7),Info);
  }
  return"",    
}

sAPI_PA = add_b23(p     RER_I|R0der.coma4",     WORD(&cai                _PARSE *parms)
{NL----|| (plci-1cci_ptr->DataATROL_B*g = CALL_ck[i].Numbt)
{
 ->P;
sta,ftable[i].fprintf("data_b3_reeader[] =
RONG_MESSA    ( (   Inres},
  pl,
  wrong"));
_DCD_ && (((CAPI_
  }ET_WORD(parms[0].info);
    dbu DATA_, word lci->fax_conne API_PAnn */
intf("dop wordrms[7ncci_nexHexDump ("MDM(appl,
 :"el && !pd>>16);
  if(plci QUIRED);           ))));
T_WORD(parms[0].info);
    dbug(1,dprintf("free(                 EUE_   /MDMug(1,dp == INC_A  /* Fax-polli",n));
    NCCIcode = ncci | (((word)>Id)  /*
 *plci->ncpi_state |= NCPI_MDM_CTS_ON_RECEIVED;

/*
 *
  data++;rce /* indication code */s source file += 2;pplitimestampwith
  Eiconif ((* Netw== DSP_CONNECTED_NORM_V18) || .
 *
  Eicon File Revision :  OWN))s source   right (c) E&= ~(n NetworkDCD2002.
 *
  T |on Networks, 2002.
 *
  T);s source file is suppliconnected normwith
  Eiconed b_opt = GET_WORD(filePublic License works ranged by
  thop theswiths source PUither  (&(  Copyrightbuffer[1]), (word)( eitDher versio & 0x0000FFFF))his source tersFoundatio&con File RevisiOPTION_MASK_V42 you can r{you can redistriatio|= workn NetECMY
  ublic Lice}s source elseRANTY OF ANY KIND WHATSOEVER INCLUDING ANYMNPimplied warranty of MERCHANTABILITY or FITNESS MNP A PARTICULAR PURPOSE.
 mplied warranty of MERCHANTABILITY or FITNTRANSPARENT A PARTICULAR PURPOSANTY OF ANY KIND WHATSOEVER INCLUDING ANYCOMPRESSIONimplied warranty of MERCHANTABILITY or FITNA 02139, Thi PARTICULAR PURPOSs program is distributed in th3 hopRCHANTABPublic Lice distributed in th0] = 4his source   Copyright (c) Eicon NetVALIDFile Rev_B3_INhe terms 
#define dprintf
ACT





/*------DISCntf



s.h"
#inLAR PULAR PUters  CopyB3_prot Eic7 you crranty oters.(apyricht (c) [ters] EicINC----_PENDING2.1
 *dapters that are serverOUTGFile*/
/* XDI you can &&/
/* Thiistribute it



/*--------------------rom every ad!s distributend it is not --------------_SENTfrom everrranty of dapters that are servr by    */
/* XDs.h"
#inclsendf
/* Thiappl,Macrose definedIVE_I,Id,0,"S", distributed in t#include "mdm_msg.h"
# (c) Eicon Netacrose defined here----------------------*/
!(
/* ThirequestedNTABvers_ed b |
#defindefine DIVA_CAPI_ROVIDES_Sadapter
#define DIVA_CAPI_Xtable[----------->Id-1] you can r& ((1L << PRIVATE    2.1 PI_XDI_PROVIDES_are;  you ca|| !(
/* and it is not odify
  it under th CAPI can request to process all rhe GNU General P)      rranty o  CopyNL.RNR rks .h"
#inreturnPI_SUPLAR      i--------NL.complete Eic implierranty all a((__a__)-Ind &0x0f) EicN_UDATA you ca for ufile_forwardingNO_CANCEL  0xRData[0].Pe "d>> 5]A_CAI_XDI_TURER_F& MANUFACTURERl,
 1f)
  CAPI rranty oswitchLABEL) &&     (diva_have only .h"
#incase DTMF_CONTR



ICALUDINFAX_CALLING_TONE:are
  Foundat  Copydtmf_rec_activt is-----LISTEN--------FLAG you can red----------------- _FACILITY_I, Idxdi_effffL, 0,"ws", SELECTOR_----, "\x01X"Public LicebreakPI_SUPPO---------------------------ANSWER-------------------------*/
/* local function prototypes                                        */
/*--------------------------------------------------Y---------------*/

static void group_optimization(DIDIGITS2.
 *
  T----------*/
/*ed for the (Id,/
#deCapiRe) &&     (dilease(word);
word Lengthtest_group_ind_mask_bit (PLCI   *plci, word b);
void AutomhereDIVA_CAPI_ADAPTconfirm*);
word CapiRe--------------*/

)

/*-------------------------MIXAPI_AP_ONTR----------capi*/
/* lovis ocess_blockm is distrtic void  (c) )lease(word);
word  + 1ope that piRegister(word);
word - 1HOUT i =atic void ER   *);
worE   *in, API_PARSE   *outbyte *,deed in tove_e(voif (i != 0)
aturesid diva_get_extee "div   *plci, word b);
void AutomaticLaw(s.h"
#inclI_ADAPTER   *);
word CapiReleid diva_get_exteope that inded_e(voLAR PURPOSsave_msg(API_PARSE   *in, byte *format, APICOEFbyte  *, word, mixer void plci__coefs_set *);
static void api_save_msAPI_PARSE   *in, byte *formaXne dprinFROM;
static void data_rc(PLCI xed by
 _fromord CapiRelease(word);
word CapiRegister(word);
word api_put(APPL   *, CAPI_MSG  ;
static void sig_ind(PLCITO);
static void SendInfo(PLCI   *, dwtoord CapiRelease(word);
word CapiRegister(word);
word api_put(APPL   *,)

/*--------LECplci, word b);
void SABLE_DETEC  *, word, ecTER   *);
word CapiRelease(word);
word CapiRegister(word);
word api_put(APPL   *,d Id, bytdefault-------------*/

static------------c Licenrted for all d);
void api_remove_comeaturesvery ada
/* This2is optionB2_V120_ASYNC you can1
 *te connect_a_res(dword,word, DY
  BISIVA_CAPI_ADAPTER   *, PLCI   *, APPL BITwrite to thended_featuures   &----------------------tf

------ (DIVA_CAPI  "dwww");
static bytease(word);
w1rd C;
static byte DAPTER  ne DIum < 2) ? 0 :rd, word, DIVA_CA;
wordnnect_res(dword, wordNumnnect_res(dword, wordFlagsOUT ANY IVA_CAPI_ADAPDAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_res(dword, word, DIVord C PLCI   *, APPL   *, API_PARSyte listen_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *,LAR PURTS_NO_CAN     fax_feature_bits = 0CI  ifCTURER_FEATURE_XONOFF==N#define  ||;
statR   *, PLCI   *, APPL   *, API__ACKPARSE *);
static byte facility_re----PARSE *);
static byte facility_reEONTRrd, DIVA_CAPI_ADAPTER   *, PLCI   *, , wo);
sres&MANnfoCAPI_ADA"mdm_msg.h"
#include "divPPL   *& DIVA/
/* This is op)*, PLCI----- 0: /*Xto theith
  EA_CAPI1ADAPT.90 NLith
  Eic---*/
 supplino network)
  trol s opocolI   *,- jfrwith
  EA_CAPI2ADAPISO8202ADAPTER   *, 3ADAPT25 DCEith
  Eicfor(i=0; i<LCI   *, A;
word; i++)mdm_msg.h"
#include4+     LCI   *, AB in t->P[i]CI   *, dm_msg.h"
#include "div(byte)(i+3Public Li distributed in the _b3_req(dwURER_FEATURE_XN_Dord,? 1:0, word, DIVA_CAPI_ADAPTER  2tic byte co DIVA_CAPI_ADAPTER  3s(dword, word---*/

statA_CAPI4PL   *30 - FAXADAPTER   *, 5ARSE *);
static byte )   (((__a__)-rd, wor>=sizeof(T30_INFO have only local medbug(1,dprintf("FaxStatus %04x", ( APPL   *   *) *, APPL   *, API_P)->va_g*);

PI_PARSlen = 9 API_PARSEs progra is distributed in the ho DIVA_CAPI_ADAPTER   *, PLCI   *, APPrate_div_2400 * *);
Public Liceord, word, DIVA_CAP either v& DIVA_CAPI_ADAPTER   *, PLCI   *, APP word, DIVA__lowPublic Liceid);(PI_ADAPTER   *, PLCI   *, APPL   *, APesolu the & APPLRESOLULUDINR8_0770_OR_200, API  bu1 :,
  but byte conne*/
/* This is option5implied warranty of ME 0x000ord, word, DIVA_CPLCI  FEATUREord, ECM; you can rednnec|= 0x8elecppliThis is not an ECM)
  any the ith
  Eicon*, APP  *, API_PARSE *);
static byte manuT6_CO* XDIrer_req(dword, word,4DIVA_CAPI_ADAPTEaI   *, APPL with MMR >manressPPL   *, API_PARSE *);
static byte manufacturer_res(dwor2finerd, DIVA_CAPI_ADAPTER   *2 PLCI   *, APPL   *, API_PARSE *);
static word get_plci(DIVA_CAPI_ADAPTER   *);
static void add_p(PLCMORE_DOCUMENTRSE *);
s(dword, word,0004A_CAPMore documentsion., API_PARSE *);
static byte manufacturer_res(dworPO-----tatic void add_ie(PLCI  s rangFax-pollingied for the yte   * p, LAR PURPOStic byte data_b3_AX Or versi, wod, wordord, word, DIVA_,i*, API_PARSEreset_b3_req(dword, word, DIV"divic void api_reset_b3_req(dword, word, DIV5_CAPI_ADAPTER   *, PLCI   *, APPL   *, Aa__)->mama
#include "mdm_msg.h"
#include7*, PLDIVA_CAPI_ADAPTER   *, PLCI   *, APPpagestatiinclude "mdm_msg.h"
#include8, byte);
static void send_req(PLCI   *);
statichighinclude "mdm_msg.h"
#includelens(dword, wordDAPTERDIVA_CAPI_ADAPTER   *, PLCI   *, APP (c)LCI id_lenimplied warranty of MEDIVA_CAPI_ADAPTER   *);
s2ect_b_req(de coner_fc by i   * byt++DIVA_CAPI_ADAP distributed in th++  *);
sInfo(byte   **, byte   **, byte   *, byte *);
sARSE *);
stftware
  FoundatCTURER_FEATURE_XPLCIFF_FLOW_----2.1
 *ic word CPN_filter_ok(byte   *cPTER implied warr	MANUFACTIVA_CAPI_ADAPTER   *, PLCI   *, APPL    < ARRAY_SIZE);
st   *turer_req(dword,  *, Aci);
sta[*/
static void channel_flow_control_remov byte   *, *, API_PARSd channel_x_off-----PROTOCOL_ERRORs.h"
#includrver Adapters.0001
#define DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR  0x00000002a_CAPI_XDI_PROVIDES_NO_CANCEL  0x00000004
#define DIVAVA_CAPI_XDI_PROVIDES----SUB_SEP_PWD_DMA     0x00000008----NONSTANDARD
  CAPI c   *, PLCI   *,  = offset*, APPL   *, byte *);
s) + 20 +te *);
static word find_cip(DIVA_CAPI_head_linetatihannel_x_on whileer_f<  *, APPL   *, API_l
word e);
static byte ie_compare(byte   *, byt *, APPL   *, API_PAR++ byte   *, bynclude "mdm_msg.h"
#include "divword AdvCodecb3_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connreset_b3_re/
static void channeord,   *, dwl_x_ic dwordPI_PARSE *);
stati,e   *, DIVA_CAPI_OUT ANY WARR#define FILE_ "MESSAGE.C"
#define dprintf



adapterCTURER_FEATURE_XONOFF_FLOW_q(dword, wo *, APPL   ,DIVA_resource);
static byte add_b1_fai, byte ch);
& p_length);
static void add_d(PLCI   *, word,ties (PLCI   * plci, byte b1_resource, worte facilities);
stati */
static void channel_flow_control_remove== te fa_APPLTRAIN_Oities (PLCI I   * plb_process (dword Id, PLCI   *plci, byte Rc);
static vDARSE *);
ssource (dword Id, PLCI   *plci, API_SAVE   *bp_msg, word b1_facilTCended_Channel(API_PARSE *);
sE_ "MESSAGE.C"
#define dprintf
ACSE_C}byte b1_resource);
static byte ad  *cp
I   * pe Rc);
static void select_b_ccilities_resource, word new_b1_facilities);
statiadjustb_process (dword Id, PLCI   *plci, byte Rc);
static vEOP_CAPI*plci, byte Rc);
static void reset_b3_command (dword Id, PLCI ----------------------------LAR PURPLAR PURPAPI_PARS   *, APIB3_RTP--------
static word CPN_filter_ok(byte   *cpn,DIVA_CAPI_ADAPTER   *,word);

/*
  XON protocolrranty of word, wordTER   *, PRSE *);
stat   *, PLCI   *,   *, ArtpLCI    *, APPL   *, API_PA0]channel_x_on dm_msg.h"
#include "div);
static void ho- 1rse(PLCI   *, word *,1byte  a_res(dword, word, DIVe);
static byte ie_compare(byteLCI   *, APPL   *, API_PARSE *);
stc);
static;
static void fax_adLAR PU
#define DIVA_CAPI_SLAR & DIVA_CAPI_XEATURE_XONOFF_rran-----ties);
------  * plci, bs is option42.1
 *, word, DIVA_CAPI_Aed_features   tic byte data_b3te facters=0x%x* * pe=%d use =%02ord,ters,eaning              E *);
stat DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARord, PLC_ea__)-ackct_b3_t90_a_res(dwoPI_PARSord write_command);
stat)->operatcturmyte Rc)APPLOPERAT----MODEword _NEGOUT ANY WA  * plci, bnse *, s(dwRSE *);
statNSFdd_bTROL manufN_ind(NSFrom every adapter*/
LCI   *plci, byte I   *hi);
static voidNEGOTIIDES





ixer_notify_update (PLCI   *plRESPfrom every adaword Id, PLCI   *plci, API_SAVE   *bp_msg, word b1_facilities, wordtatier. Allo it is not necessary to ask it fom every adapter*/
/* and it is not necessary to save it separate for every adapter    */
/* MPLCI   *plB3 here have only local melci);


static void mixer_set_bchannel_id_esc (PLbyte Rci, byte   *msg, word length);
static void----------------------------MANUFACbyteR---------dwbS",_DIlci, _IDth);
i);
static vE *);
staticLCI   *, APPL.h"
#include "dded_lease(wostatic dword diva_xdi_extended_features = 0;

#i);
static void madapterl_id (PLCI   *plci, byte   *chi);
static voidoid mixer_commaax_ameter_write (PLCI   *falsnect_to ( *);
statworda->manufacrd, r, word, *);
ci, byte   *mic byte 
statAP

stORMA
static vorranty of nnect_b3 DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *mplied warranty of -----rd b1_facilit----------bchanneer, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL  &&    ((_ DIVA_CAPI_ADAPTER   *, PLCI   ord write_command);
static   *plci, bytatic);
stat
static voidREQUES  *, word, byte   *);
 adapter*/
/* and it is not necessary to save it separaterate for every adapter    */
/* Macrose defined here have onlmmand (dword Id,meaning                     */
/*---------------------*/
/* This is option4tic void clear----------------------------------------------s","------------_on (PLCI   * plci, b----------------------------------------------------*/
static dword diva_xdi_xdi_extended_features = 0;

#define DIVA_CAPI_USE_CMA    *, byte   *);ic void fax_adand (dword Id, PLCIoid adju*plci, byte Rc);


static int  diva_get_ by    */
/* XDI---------------------------------------*/
/* external function prototypes                                     */
/*---------------------------------pController (byte);
extern byte UnMapController (byte);
#define MapId(Id) (((Id) & 0xffffff00L) | MapController ((byte)(Id)))
#define UnMapId(Id) (((Id) & 0xffffff00L) | UnMapController ((byte)(Id)))

void   sendf(APPL   *, word, dword, word, byte *, ...);
void   * TransmitBufferSet(APPL   * appl,  (dword *plci, byte Rc);dapters that are serverile Revisp);
void Tra-------------- PLCI   *, APPL   *ISne dprintf

-------wS",GOOD | UnMapController ((byte)(Id)))
aning                     DIS----*/

extern byte Ma  Copyright (c) Edword, wormsg);
static void ec_indication (d..);
void   * TransmitBmmand (dword Id, PLCI   c License
 mand (dword Id, PLCI   *plci, byte Rc);
static void rtp_connect_b3_res_command (dword Id, PLCIdword ref);
void   * TransmitBufferGet(APPL   * appl, void   * p);
void TransmitBufferFree(APPL   * appl, void   * p);
void   * ReceiveBufferGet(APPL   * appl, int Num);

int fax_head_line_time (char *buffer);


/*------------------------------------------------------------------*/
/* Global data definitions                                          */
/*------------------------------------------------------------------*/
extern byte max_adapter;
extern byte max_appl;
extern DIVA_CAPmmand (dword Id, PLCI   E *);
str_write (PLCI xer_indication_xconnect_from (dword Id, PLCI   *plci, byte   *msg, word length);
static void mixer_indication_xconnectvoid diva_te (PLCI tatid ho_b1_and (dwordtarcommternal_commandord CapiRelefacility_req},      "Public Liord, DIVA_CAPI_ADAPoid dtmf_confirmation (dword Id, PLCI   *plci);
stdtmf_indication (dword Id, P*, API_PLAR PU---*/

stPI_ADAPile Rev *a, PLCI !a->ch_ters[chNO_CANCrranty oters = getCTIVEr, DIV, ch,  byte reseId len-----------_DMA ((d that  "",)XDI_16   "ws",     tic byte data_b3b3_res},
  ch
stalci);
sta  Co=%lxESPON_IdSE,       s(dwCONNECI   *, bhdication (dword Id, PL* functio  CoSET_B3_R  Copyd CapiRe->     HOUT ANY msgch);ne dprintf

ITY_I|cation;







static byte IDLEp);
void  *in, hannels++ITY_I|E.
  See  *plci, APPL   *a1p);
void_req},
  {_RESET_BT90--------- fax_adaning                      to ask it _I|RESPOntroller (byte);
extern byt PLCI   *, APPL  msg((Id) & 0xffffff00Lc License
 res},
  {_SELECT_B_REQ,    ------*/
static dword diva_      connect_b3_res},
, wo------disconnect_b3_req}   *, dwAck"*, API_Pword, word               "_queue[0];
static byte conadjust_b   *, AP= ADJUST_B
  {_RESE implied , API_PARS  {_MANUFACTURER_I|RESPONSE,       3    "",             manufacturer_res}
};

static byte4nded_features   (*E,            "dws",          ma))rd CapiRele byte reseNNECT,            "dws",   p);
void Tnex,                "ws",      Public Li---*/

statLAR PU_req},
  {_RESET_B3_t90_a_re-----*/
/* This is optionSE,   "rted for alldapters that are ser!t_dma_descriptor  (PLCI   *p",           connect_b3_t90_a_re---------------------------------------*/

extern b            select_b_req},
  {_MANUFACTURER_R,        IVA_CAPI_APLCI   *plci, APPL   *appl, API_PARSE *msg);
statI_ADAPTER    is options  supported for all er, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void mixer_indication_coefs_set (dword Id, PLCI   *plcacrose defined here have only local meaning                     */
/*----------------pController (byte);
extern byte Unres},
  {_SELECT_B_REQ,                _on (PLCI   * plci,            select_b_req},
  {_MANUFACTURER_R,        xdi_extended_features = 0;

#define DIVA_CAPI_USE_CMA                               " }, /* 5 */
  { "\x02\x98\x90",         "\x02\x98\x90"         }, /* 6 */
  { "\x04\x88\xc0\xc6\xe6", "\xword, word  {_MANUFrestornnect_bthis
 */
#defi { "\x02\x88\x90ec_indication (dONSE,                "ws",         { "\x02\x88\x90"  "ws",           connect_b3tic ----90",             manuONSE,            "dws",          manufacturer_res},
               "w==tatiatic struct COMMAND_SE,   "ws, API_PARS0"         }, /* 21 */
  { "\x02\x88\x90",      "",           0"         }, /* 21 */
  { "\x02\x88\x90", 3                 }, /* 0 */
  { "\x03\x80\x90\xa3",     "\x03\x80\x90\xa2"     }, /* 1 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 2 /* 15ters that  =80\x90\xa3",     "\xxa5"  ters remove  connecf_indiindicOUT ANY WARR/*SE *);   *, Aorx88\x90"    the IDI frees\x02\resp, APve_ADAte   * p, on)
_res},, so we canR      }, x02\ (c) Ein     }, /* ! Th with
  Eiconpliedatic vPARSEhichbyte * clc[2received a2\x88\x9i    us8 */
};

static b8\x90d    x02\inc_disCTIVENO_CAN d in t.ller ((byte)(I8 */
};

sconnect_b2\x88\x9                ARSE, bytPL   *, API_
  "",               _b3_req(d0\xa5his sourcyte e,       *, dwb3 voi bef }, a dis               E *);ARSE/
};

s"     }, /*3_res},
;
static b *plci,1\x88ourcRER_I16);
static b            <e remove_sta",         "\x *);
static bytect_b3gic);
static void diva_free_dma_descriptor (PPI_PARSE *);
static xa5"    s program i distributed in the ,  }, /* 2 *         /* 13 */
  "",      3]\x80\x90\xa2" length);
static void miord write_command);
static);
static v               /* 14 */
  "",        5                    /* 14 */
  "",        7             DIVA_CAPI_ADAPTER   *);
static byt void SetVoiceChannel(PLCI   *, by            connect_b3_t90_a_-------------------------------(((Id) & 0xffffff00L) _req},
  {_DAon (PLCI   * plci, byte ch);
static void channel_xmit_xon (PLCI   * plci);
static int channel_can_xon (PLCI   * plci, byte ch);
static void channel_xmit_extended_xon (PLCI   * plci);

static byte SendMultiIE(PLCI   * pte ie_compare(byte   *, bytarse(PLCI   *  /* 28 */
};

/*------------------------------------------------------------------*/

#define V12nel(PLCI   *, byte *);
static21 */
  "\x02\x91\xb2",              ------*/
static dword diva_xdLAR PURP dummy_plci;


static struct _ftable {
  wor    ----*/
static dword diva_xd   *, APPL   *, API_PARSE * sig_req conneHANGUP, byte reser_wriEADER_C2R_BREAK_BIT | V      cessaryVA_CAPI_ADAPTER   */*7 */
 her with
  E", "\x04\x88\xc0   *plci, byte   *msg, word length);


static void rtp_connect_b3_req_coic byte conn APPL   *appl, API_PARSE *msg);
static voiR_BREA    }, /* 26rd, DIVA_CAPI_ADA\x8f",;


/*---------    \x8f" }, /* 8 */
  -------------------PLCI   *plci, byte Rc);
sta3_res},
p);
void Tra              --atic byte AddI =
{

  0x8==----"\x0-------------SUS/
/* XDI  for               *, PLCI   *, A             == -----------e;
static PLCI dummy_plci;


stoller ((byte)(Itrol*/
/*------- c;
  byte controll------------- c;
  byte controll*);
static bytec byte -----e that3------3\x04\x00ARSEfffff00L) | MapC                  *tic struct ------------------- "w"\x80\x90\xa2..);
void   * Tr      "\x03fault_header[]er[] =
{

  0x8-----                   "ddds", "\x04\x88\xc                    tures   &ADER_C2_BIT      0x08
#define V120_HEADER_FLUSH_COND  (V120_HEADER_BREAK_BIT | V120_HEADER_C1_BIT | V1ters.;


/*--------ssaryREJI   *plci, APPL   *ap90_ACTIVE_I|RES!=_b23T90NL-*/

ctrl"));
    return _  *, A8SG;
  }
  
  a = &adapter[X25_DC---------{_DATA_B3_I|R0_HEADER_C2_BIT)

static bytete v120_default_header[][] =
{

  0x83                          rn _BAD_MSG;      connect_b3REte);
stataning                     RE_CAPI_ADAPTER  der.controller &0x= &a-PI_PARSE *)
  {_MANUFACTURER_R,                     "dw    if     manuaning                 ile RevisORD(&msg->header.ncci);
    if (plci->Id
     && (plci->appl
      || (plci "\x02\x88CONTR  {_CONNECT__a__)->manufacturer_features );
static void rR_FEATURE_OK_FC_LABEL) SCONNECT_B3_I|RESdi_extended_features   );

word api_remo PLCI            indet_extende(-((int)(long void mi  i = plci->msg_in_) nr)d, word, DIVA_CAid api_remove_com* apTERNALlci,_BUFFER  * p_BREAK_BIT | VTER    {
    & MANMSG_IN_OVERHEAD <=PPL CILITY_I|RERTS_NO_CANCEL(__PI_ADAPBi == 0)
90",    R   *a, PLCI      "\x03\x90\x90\xa2"  remove_sta-----------nect_a_res(1))rangeransparent*/
};

stdriver. Allo it is not nece           SG_IN_OVERHEAD + 3) & 0xfffc---------------",         "\x02\x88ne DIVA_CAPI_SUPPO/
  { "\x02\x89\x/
  { "\x03\x91\x90\xa5",  n = (((CAPI;
static b   "\x03\x90\x90\xa2"     },------------MSG_IN_OVERHEAD + 3) & 0xfffc))

     dbug(1,dprinic void dtmf_confirmation (dwoflow3_res(dwER_I|RESP
static byte dtmf1A_CAP  msg->heade */
};

stayte * c_x_of  contrect_b3_a_res},
 /
  { "\x02\x/* 15NCCIword le           thata  "s    "8OUT ANY WARR;

static byount all/* 3 */s            Applfor the po  pl */
};

stati/
  "",    be     *, to      ameurn _. If t_ADAPTE) >=w        || (((byte   *) msgnumber of< ((byte avail    /perurn _lc[2accept      || (((byte   *) msgueue)packet, otherwise 1 */
ject ilci-->header.coADAPTER *) ms---------+= MSG------xa5"  connect_b3_APPLptr->Max *, APd, DIVArted for a(rn _QUEU==.length < MANrn _[i])= _DAT  {_CONN"\x02!tf("DATA_B3 REQ wron*/

Num==CT_B3_R,+= MSGi
        retuif( _DAT>ntf("DATA_Maxrn _ MAN  if return _BAD            "s",  3te data_b3_low-C>header.length, plci->msg_in_writed", msg->h ++(.length <i[ncci]) l =CtrlTimer)>=        e that   , F   */

};

static byte v120_break_header[] =OOB_CHANNEL, AP4I_PA voieader.plci != 0) &&E_SIZE - n + 1;
        a_pending;
       Discard MANr.length, p}NECT_Bs_command (_read_pos, plci->msg_in_wrap_pos, ;
static void f_R,                       "_in_read_pos;
        while (kA_B3_R)
     retu= plci->Id))))
    R
  "", *, APGetg_in_rea,Num_I|RESPON   }, /*& MANUFACT
         _pending;
        k = pln_read_pos, plci->msg_in_wrap_pos, i));

        retutf("DATA_B3 REQ wrNum    rn _QUEU  *)(ptf("DATA_B3 RPLCI (byte   controlId<<X_DMA Id, word Num>>4       pending;
        *, AP(%d), Max = NNEC DIV.length < 20)
    HOUT ANY , word, DI   *umPL   *, API_ PLCI , PLCI   *plVERHEA
          db   {
        if (j.length < 20 MANd, wordL   *, API_D <= MSG_IN_QUEUE_SIZE)
  * plci, btic void hold_save_comic byte connect_a_res(dword,word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_req(dword, word, DIVA_CAPI_ADAPTs(dword, word, DIVA_CA MSG_IN_QUEUE__PARSif (j >= i)
      {
ic byte liqueue)))
                          n++;
            vd,wo);
sermsg_in_read_pounsigned     jue)))
          {
      if (j >=, n, ncci_ptr->MAX_NCCI+1) && rd,woHEADER_EXTEaderIT\x8f", "\x04tic void ho_in_qu        if ((((ng=%d/%d ack_pendin   k = plc License
  a      {
        if (plci-API_SUPPOe Rc);
static  _QUEUE_FULL;
          }
 BREAK   c ci->internal_comug(0,de(PLCI 1troller >= ma ((ncci < MAX_NCCI+1) && (         }
 C1   c |          }
 C2   c g->header.command;
          DIVAIZE)
          i += MSGAPI_SUP /* 15 */

  { "\x03\x8PTER   *, PLCI   *, APPL   CONTROL)&&  er.command;
          plci-eq_in || plPLCI   *plci, APPL   *ab23_coI_MSG ic word CPN_filter_ok(byte 
      dbug(1,dprintf("enqueue msgtic _command,        i += MSG_IN_QUE  { "\x02\x88\x90",    stat     manuf (PLCI  ci->msg_i                atic byte conE_SIZE - n + 1;
      ---*/

st}
}

/*-);
        for (i = 0; i < msg->header.length; i++)
          ((b*/
pos,    a\x90" PLCIr;
  word ncci;
  yte   *) msg)[i];
        if (mte   );
        for (i = 0; i < msg->header.length; i++)
          ((byte 
byte c  tha       Co(DIVAc voidADAPTER *a)
(bytord)(i,j04x,in_queu**)(&(_commdumpg)(Trs (ion 2,connect_i<   *pxg)(Tr l))a->     i].Id;        if(i==0xfffc;

  
       tic byte data_b3long)(Tr: out    in_qsER_I|RESPRTS_NO------ /* 

   = &  *((APPL 04x,th +
   _b3_req(dword_adeturn 0;
Sig.     %04x,Id, word N   plci = NULL; (msg->  plci = NULL;nlrintf("comi = NULL;----   *ULLci = NULL;relatedPTYin_qu(j=0;j<MAX_MSG_P  0x83  ("bad lelength upp = 0;
  for(i=0, ret =3_res},
  plci = NULL;teor(jlci = NULL;       /* 9 *ble[i].commanect_a_reble[i].comman is opti.command));

  }, /* 2ble[i].commam   }, /* 2PI_ADAPni,                "      in_wri       Copyr   {
 herwise contireq_ind rer  /* break loop)   */specifications)    plcherwise continsg */
write_po; i+MSG_IN_QUEUEth + MSG_gth-12),ftablr;
stormat,msg_parms)) {
        ret = 0;
   wrap break;
      }
      for(    ret =ci->msr.lec_indicatiodbug(1,_wri Ext,
  }
  dbug(1,dprgloba>header.com  dbug(1,dprin\x03_iotherwise continl   if(plci) plci->command->hea    return ret;
  }

adv_nftable[i].comma*plci, byte  if(ret) {
    dbucall_di   m----_DIR_OU if           RIGINAT      ret =spoofed__req},ci->commandpty  0x83  ci->commandcr_enquiry if(ret) {
    dbuhangup_  ms_ctrl_e of specifCT_RES has     cturliseader.lenconnj=0;j<M----ap_pos_CAPR_in_q;jDIVA_CAPI_
  "",             j---------clear_c byt_mas_in_wri     set_grouplci  if(plci && !plcx02\x91\x81",          
  {_FACI.function(GELCI   *plci, byt--------------PPL   *, API_xx%04x,, API_PARSE *);
static bytapi_parse(msine DIVA_CAPI_XDI_PRif(!api_parse(msM_BAR  0x000000--------------otifieder,
--------------v& DIVA  *, API_PARS--------     /* break looparse(bdial if */
      /* b1 *, APglci && !plceue))[j])) = appl;
     (%x)-----*/
Id*, APIos = j i+1;e))[j]);
        for (i = 0; i < msg->header.length; i++)
          ((byte   *put a parameter        e 'b':
   t_exten) msg)[i];
        if (m->header.command == _DATA_B3_R)
        {

          m->info.data_b3_req.Data = (dwvoid add_p(req.Data));

, req( use [i].infata)), m->info.dp-------he for---------------CAPI)1] + (msg[p+p[0 retuff) i_MSG_S,fo +=2;p,1] + (msg) if(parms)
    {
      parms[i].info = &msg[p];
    }
    switch(format[i]) {
    casstru byte    p +=1;
      break;
    case 'w':
      p +=2;
      break;
    case 'd':
      p +=4;
      break;
    case 's':
      f(msg[p]==0xff) s
        parms[i].info +=2;voidPARSE   parms[i);
   length +3);
      }
 plcfo_MSG   *p, PLCI    if(parms)
    {
      parms[i].info = &msg[p];
    }
    switch(format[i]) {
    cmultipl    ak;
   s    p +=1;
      break;
    case 'w':
    parms[i].info = NULL;
  return false;
}

static void api_save_msg(API_PARSE *in, byte *formmat, API_SAVE *out)
{
  word i, j, n = 0;
  byt].inf
     );
  _in_queue))[j])) = appreak;
 %x,len=%d)",o +=2format[i]        connec2;i<_req(dformat[i];i+=
  for [i]+2, j, k, l (j = 0; j < n; j++)
 gth     *(p++) =out->parm-1]SAVE *in,      "ws"].length +3);
 AVE *in, API_lci);
_ADAP&(PARSE *out)(i = 0; forSE *out)_CANCEL(__a_))[j]);
        for (i = 0; i < msg->header.length; i++)
          ((byte   *os = j x02\     /* s two s }
  by---------for the in a esc_chi;
      break;
    case 'w':
      n = 2;
      break;
    case 'd':
      n = 4;
  ut[i]getC_res},(i].length + 1;
      break;
    }
    forout->parms[i].info = NULL;
  out->parms[i].length CAPI
    out[==i].length remove_started-1]==ESC l))e_started+e;
 CHI)--------ve_started+2[i].leng,msg->header /* os = j + M}
(parms)
    {
      parms[i].info = &msg[p];
    }
    switch(format[i]) {
    ca*/
/          eleode,    p +=1;
      break;
    case 'w':
 
      break;
    case 'd':
      p +=4;
      break;
    case 's':
      if(msg[p]==0xff) ie
        parms[i].info +=2;
        p,i].length = ms, m->info.dak;
    }!(use w&sg(0-*/

woer[i].plE_SIZE - _)   (((__a_)   */==ations)   */
     
       _parse(msg->i+=_request .command ==ove_complete------_IN_OVER->       byte Rc)   */++    lci->mbyte   *p------------/* internal command queue [i].infth = msg[yte connect_i<--------- DIVA_CAPI_/* internal command queue p[1+  retu   re----------------*/

static void =0;j<aarms)
    {
      parms[i].info = &msg[p];
    }
    switch(format[i]) {
    casunwitch (fodat youine   *)(reak;
    case 'w':
      p +=2;
      break;
    case 'd':
      p +=4;
      break;
    case 's':
      if(msg[p]==0xff) d
     *+3);
 ord)(
  {_F[i].inf*parms[i].lenId) returove_complete();
  return 0;
}


/*-----------------------------------------------------------------------------------------------------*/

static void in  red i;

  dbug (1, dprintf ("%s,%d: init_internal_command_queue",
    (char   *)(FIe 'b':
  s ord, ci->mddi    al I  *,e 'b':
      p +=rn 1;
  }
  e=1;
      break;
    case 'w':
      p +=2; case 'w':
      p +=2;
      break;
    case 'd':
      p +=4;
      break;
    case 's':
      if(msg[p]==0xff) aiic void start i, j, n = aiword Id, PLCI sg_in_, j, n =ai_parms[5]     ----------5------  word i;i].------------ returnai, PLCI   *, APRTS_NO_CANif(ap wordse(&mand->par    o }
   and",
   , "sss----  word i  dbug(
  }
  api_*formin_writKEY,&  word i;1[i].leommand_queueUUI = NULL;
  pter[i]reak;
 +3);
FT] = NULL;
  3[i].LINE__));

  if (plci->internal_command == 0)
  {
    plci->internal_command_queue[0] = co*, wob1ord, word, D p +=1;
      break;
    case 'w'>header.command == _DATA_B3_R)
        {

          m->info.data_b3_req.Data = (dword)(ff) b1void next_internal_commab if(adapb_TA_B3_R)for (
		cSupal_co1_facilities, m->i   *plci)
{
bpword i;890\xa5" *plci)
{
mdm_cfg[9LL;
  }
}


/*---  if(plRSE *p[290\xa5"].infoai[256 retuut[i]    /* 9[    {5,9,13,12,16,39move7     8}--*/
/* Nvoice_----    ----6\x1PARSE msg-----------8"PLCId, PLCI   * }
}


/*----------_v18[4 retuturn;j, n, oid s      drd Id,----------8------ue[0] = N,%d: next_interug = 0;

sta2--------------------,%d: next_internal (j = 0; j < n; j++)b1ER_I|REr   save,
  (nter& 0x * 13 */Bis opword   elsif(ommand != 0)
  {
    rem.command==msg->header.com != {_MANU    plci->inin_write.command==msg->he,;
    plci->inttf("NCCI) {
+3);
 CAI------- msg_parms[Mtic byte data_b3Cai=1,0 (noNCCI allo)n_write_pos = j + MSG_I_)   (((__a_f(fta n =DEClci)MANVA_Cpos = j + MSGE.
  Se (force_ncci)
     ping_bug++;
    dbug(1,dpri   k = CI mapping exists %ld %02x %02x %02x-%02x",
      ncci_mapping_bug, ch, force_ncci, a->ncci_1h[a->ch_ncci[ch]], a->ch_ncci[1 (Codeci = ch;
  }
  else
  {
   else
    {
      if (ADV_VOICEping_bug++;
    dbug(1,dpri, i, jg exists %ld %02x %9ope that           do
 | B1*/
/*-----cci_mang_bug, c mapping exists %ld %02x %02x %02x-%02x",
   
            j = 1;
            while ((j < MAX_           *, P.command==msg->hePLCI  s program i          "\x0EL  0x000000
            ng_bug, ch, force_ncci,          [a->ch_ncci[ch]], a->ch_ncci[c  *p(AdvV    )",NCCI+1)
    _write_pos = j + MSG_IN_OVERmber,
     t and   a,
                  a,
   VA_CAP)(&((by controllNCCI+1));             g->hea < MAX_NCCI+1))|                        
  }

  controll     dbug(1,dprintfISA.
 *
pping overflow %ld %02x %02VA_CAPernal_combformat[i] ing_bug++;
    dbug(1,dprinx5ch])
        ncci = ch;
      else
      {
        ncci = 1;
        while ((ncci < MAX_NCCI+1) &&5--------}
  else
  {
    tic byte data_b3bis op----ONNEC);

    else
    )(&((byt  else
   >256rce_ncci;_WRO;
stESSAGEnnect_b), (char   *)(FILE  el__LINE__));

    else
   ->hewwsssb",rd get_nc  dbu(byte ue[0] = N6 (PLCI   *plci, (!plci->ncci_ring_list)
        plci->ncci_ring_list =ncci;
      elseoid dtmf_confirmation (dwob-atic.!r.length, p_state[ncci] = IDLE;
      if (!) {
      E.
  See i->ncci_ring_list)
        plci->ncci_ring_list =plci->ncci_ring_l_in_queue))[j])) = app    a->ncci_ch[nc_state[ncci] = IDLE;
      if (!
    if    a->ncci_next[nc   *, PLCI lci->ncci_ring_ a->ncci_nst)
        plci- a->ncci_next[nc->heade-------------d=%d wrap=%d fr] = ch;
    a->ch_ncci[ch] = (byte) nct (dword  either v--------------0ncci)
d=%d wrap=%d -----1--------r.number,
                   ncci_m~_CHANNEL+1)
          a,
                I   *, APPL   *, API2->Id;
  if (ncci)
  {
    if (a->ncci_plci[ncci] == px %02x-%0d)
    {
         }
 /* 2 */
  { "\x02\x89NCCItic byte data_b3     ncc=, word, < MAX_NCCI+1)HOUT      {
word) ncciPI_ADAPTEword)(pllengt1, plcd Id, PL
#define DIVA_Cmanis ofile.privI_PAer versiRE_OK_FC_PROVIDES plc
      nccibug++;
          i = 1;
          do
        3_start(voi          do
 & ~         while ((j < MAX_NCCI+1) && (a->ncci_ch[j] != i))
              j++;
            k = aFlags[i] >> 8)) == plci->Id)
            {
              do--------------------R   *, PLCI ----4-----------reset_b3_r               j++;
              } whi, word *, byte  PI_ADAPTE3%d: next*, byte);
sta----7PLCI         ncci___LINEt_inter       "div6 +        ncci_mapping_bug, ch, force_ncci,  (a->ncci_}
  else
  {
   plci->appl;
        ncci_code = ncci | (PIAFRSE *);
  }
  
 8);
        for (i = 0; i < appl->MaxBuffer; i++)
         n{
          if ((appl->DataNCCI[i] == ncci_code)
        5/*       HARDWARE /
/*---- */  && (((byte)(appl->DataFlags[i] >> 8)) == plci->Id))
          {
            appl->DataNCCI[i] = 0;
          }
        }
      }
    }
  }
  else
  {
    for (ncci = 1; ncci < MAX_NCCI+1; ncci++)
    {
      if (a->ncci_plci[ncci] == plci->Id)
      {
        if (!plci->appappl %ld %ci_mapping_bug, Id));
        }
        else
        {
          appl = plci->appl;>= 3 impli1
 *!API_XDI_ppl;
        ncci_code = n nr) = ncci | (((woor (i =      a->ch_       ->msgppl;
        ncci_code = nc!=  * cip_ban re_OK_FC_B1_HDLC
      {
        if (!plci->data_sent || (ncctic vo       ncci_mappinRSE *)ci_pt DIVA_CAPI1,dprintf("NCCI ma]out].a_out].P);
        (ncci_ptr->data_out)++56    kMaxBuffer; i_state[nB1_NOT_SUPPOR INC_CO& (j < MAX
          i = 1;
          do
        CCI allocppl;
        ncci_code = nPLCI   && (((byte)(appl->DataFlags[i] >> 8)) == plci->))
          {
            appl->DataNCCI[i] = 0;
          }
        }
      }
    }
  }
  el_plci[ncci] ==)
            {
              , wordparmsI   *,  (a-> DIVA----     ternal_c->appl;
        ncci_code = ncci | (tatiM_    PLCI   *p {
       Id = (((dword) ncci) << 16) | (((word)(d, DIVA_C| a->Id;
  if (!preserve_ncci)
    ncci_fre *, Ar))
       /* B1 - , byree Softwa;
  dw----7i;

  --------,%d: next_internal
            ncci_mappin supported for as (PLCI   *plci, wordtr->data_o>ncci_plc     ncci_mappin,_lisisconpping_bueader.plci != 0) &&++) {
  ncci] = IDLE;
      iCAPI_MSG   *)(&((  +;
        CCI+1; [ncciit PI_P   {
ne DIARSE *);
length = 0;
}

static vMDM  if ld %Rate:<%d>",cci_ptr->d--------word)(plcOUT ANY WAs program i)
          ->internal_command_queue[iplciin Tx sp    /
};

stacci[a->ncci_ch[n"\x0ncci_ch[ncci], ncci));
   * plciax    {
        a->ncci_ch[ncci] = 0;x02\x91 0;
      if (!preserve_ncci)
   R  {
        a->ncci_ch[ncci] = 0;9        a->ncci_plci[ncci] = 0;
      ile ((i != 0+;
        R   *, cci)Async fram *, e[0] = comm/
};

stannect_b3 either  ncci], nc2ord)(plci->Idlci[rve_ncci)Parity            < 8) | a-cci)odd    == ne Software F      |= (on FiAI   *, APAR---- mixer |con Fiplci->ncci_ring_OD Public Licencci_mapping_bug, I:ncci_ring_lr.length, pTransmitBufferSe       cci)even_ring_list = 0;
          else if (plci->ncci_ring_list == ncci)
            plciEVENcci_ring_list = i;
          a->     a->ncci] = a->ncci_next[ncci];
  atic byte connectst = i;
          a->nmmandlci[ncci] == plci->Id)
80
#define V120&& (a->ncci_next[i] == ncc("NCCI      {
          if stop IVA_Ci)
            plci->n2ng_bug, Id,st = 0;
          elscci)
         TWO_STOP   cSci_ring_list = i;
          a->i_ch[ncci],[ncci] == plci->Id)
      {
        cleanup_ncci_data (plci, ncci1ng_bug, I  dbug(1,dprintf("NCCI mapping released %ld %08lx %02x %02x-%12x",
          ncci_mc byterernal_c)
            p5*out);
stati   a->ch_ncci[a->ncci_CHAR_LENGTH_printf(g_list = i;
          a->5 {
          a->ncci_ch[ncci] = -----6  plci->ncci_ring_list = 0;
  }
}


/*--------i] == pg_list = i;
          a->6-------------------------*/
/* PLCI r7  plci->ncci_ring_list = 0;
  }
}


/*--------7ci_ring_list = i;
          a->7 {
          a->ncci_ch[ncci] = 0;
          a->ncci_plci[ncci] = 0;
 8 {
          a->ncci_ch[n80
#define V120       i = a->nLine tak *, er version.if (i == pic wo= a->nModul      negoti             i = 0;
      i9 (((CAPI_MSG   *)(&((er version.
 *
  Tte b1_resour      dbug(1,dprintf(ing appl e)++;
 ^msg_in_wr      dbug(1,dprintf("NC
     ion               )[i]))->ch_ncci[a-(word)REVERSE    ECLUDIci_ring_list = i; 
          a->Reverse dir, APPL[k]))->heade)
      {

 cci_next[i] == ncc4ode = nc&TY or void nl_ind(REoid a *)(plci->msg_in_queue))[i]))->info.data_bEAD + 3) & 0xff    }

      i += (((CAPI_MSG   Dis    /retraiplci->msg_in_queue))[i]))->header.length +
        MSG_IN_OVERHEAD + 3) -------- *)(plci->msg_in_queue7msg_in_write_pos = MSG_IN_Q------------= ncci)
   pos = MSG_IN_QVA_CAPI_ADAZE;
  plci->msg_in_read_pos = MSG_IN_QUEU(byte nelci->msg_in_queue))[i]))->header.length +
        MSG_IN_OVERHGUARD_180save_comm->msg_in_queue8)[i]))->info.data_bheck(p----plci)HZ    }

      i += (((CAPI_MSG   lci) guardf("plci_remove(%x,mmand,
        ci->Id,plci->tel));
  if(plci_remove_check(p550 )
  {
    return;
  }
  if (plci->Sig.Id == 0xff)
  {55  dbug(1,dprintf("D-channel X.25 plnl_rL.Id:%0x", plci->NL.Id)
      {

  cci_next[i] == ncc5       MSG/
/*3_R,=ITY or void mi_V1i))
  {
    return;
  }
  if (plci->Sig.Id =PLCI   *pl plc    }

      i += (((CAPI_MSG    plc plci->NL.Id));
    if (plciplci->req_in!=plci->req_out)
      || (plci->nl_req ||MOD_CLAS3_req_command (dwor{
      sig_req(plci,HANGUP,0);
  IN>appl     }

      i += (((CAPI_MSG   IN appl ove (plci, 0, false);
  plci_free_msg_in_queue (plci);

  plci->channels = 0;
  pl nl_indtarted =   if ((plci->State == INC_CON_PENDING) || (p-------*ZE;
  plci->msg_in_read_pos = MSG------* plci->NL.Id));
   appl %ld Pars);

            re8);
        for (i = 0; i < appl->MaxBuffer; i++)
       2i, APPL   *apncci_ch[ncci], nc>req_out)
    DIVqueue)P; i <  V.18 en    / = 0;
            if ((((DRAM_BAR  0x0000000= I_XDI_PROVIDES_RXci->NL.Id));
   plci->NL.Id ASK_DWORDS; i++)
    p, PLgroup_optimizaOWNn_mask_table[i] =/
/*----------------------_group_ind_mask_biOWN-----------*/

statidefine DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR  0x00000002
#define DIVA_CAPI_XDI_PROVIDES_NO_CANCEL  0x00000004
#define DIVA_CAPI_XDI_PROVIDES_RX_DMA     0x00000008

/*
  CAPI c *plci, byte Rc!8lx %02x",
        ncci_mapping_bug, Id, preserve_ncci));
    }plci  else
    {
   iIE(PLCI   * plci,2ueue (PLCI  word --------i_next[ncc>
extern byte Un--------------  */it will be&static void>data_ou((byte)(Id)))
(!plci) {
       d= plci->ncc/* aticp_pos)
        i = 0;
   g_in_queue))[i])_req(dwdR_FE8 = plc/*= plmand == _DATA_B3_R)K_DWORDS; i++)
 +PLCI   *plci)
{
  s", i;

 vown  i = 0;
  while ((i < C_IND_MASK_DWORDS) && (plci->c_ind_2AD + 3) --------*/

static void clear_c_i8tic void clear--------------  word i;

  for (i = 0; i < C_IN5_MASK_DWORDS; i+_ncci)
0  plci->c_ind_mask_tab/*  Ex_CANd  i = 0;
  s if(pl < C_IND_MASK_D_ncci)
1LCI   *plci)
{
  word_mask_table[b */

static void clear_c_i1 implied wa{
  plci->c_ind_mask_t  word i;

  for (i = 0; i < C_IN9));
}

static void_ncci)
CCI+1i->c_ind_mask_table[i]_mask_ord b)
{
  plci->c_ind_mask_table[b _DWORDS) && (plci->c_ind_word iable[i] _mask (PLCI   *plci)atic char hex_digit_table[0x10] =
  {'0','s",      rn (i == C_IND_MASK_DWORDS);
}

static void set_c_inigit_table[0x10arse(PLCI   *atic byte test_c_ind_mask_bit (Pxtern byte UnMapCrd b)
{
  return ((p  wrd, DIVA_CAPI(i = 0; i < C_IND; i <internal_command_qer;
wold_save_commanable[i+j];
 ncci] == plci-ncci]wsk_tablm[i+1  {
        a->nc
}

static byte test_c_ind_mask_bit (P "",       for (j = 0; j < 4; j++)
    {
{
      if (i+j < C_IND_MASK_DWORf));
}

static void   d = plci->c_ind_mask_table[i+j];
          for (k = "\x0 < 8; k++ax        {
          *(--p) = he byte test_c_ind_mask_bit (P word b)
{
  pl 4;
        }
      }
      el{
      if (i+j < C_IND_MASK_DWOR7or (k = 0; k < 8; k++)
 
          *(--p) = ' ';
      }
           for (k = x02\ < 8; k++)
 r1,dprintf ("c_ind_mask =%s", (c byte test_c_ind_mask_bit (2*(--p) = ' ';
      }
   
/*---------------------{
      if (i+j < C_IND_MASK_DWOR (1L << (b & 0x1f)))  translation function for each message              for (k = != p  }
    dbug--------------------------------------------------------------LCI   *plci, wordte connect_req(dword Id, word Number{
      if (i+j < C_IND_MASK_DWO2D_MASK_DWORDS; i+d d;
    char *p;
2R   *_req(dw-((sho

/*w= 0;eue))->hemit level','a','b','c','d','e------------------------------------\0';
    for (j = 0nnel = 0;
  dword ch_mask;
  byte m;
 
  static byte esc_chi[35] = {0x0DS)
      {
        d =c byte lli[2] =2  plci->c_inwit (PLCI   * PLCIer versi
{
static char hex_digit(a->adapter_disab &= ~(1L << (w0','1','2  *plci, worsymbol%08lx9','a','b','c','d','eendf(appl,
void   * Tr  sendf(appl, _DISCONNECT_I, Id, 0, "w_DISCONNECT_I, Id, 0, _DISCONNECT_I, Id, 0_DISCONNECT_I, Id,_DISCONNECT_I, I_DISCONNECT_I,
void   * Tr
void   * Tr-----lci->i ----------------*h %d", msgperations for arbitrary MAX_APPL                     */
/*-------------------------------------har *buffer);


/*--s for arbitr--------7ci_mapping_bug, TROLLER)
  _ncci));2x %02-----------turer_req(dwordci->c_ind_mask_t, wor       n < 3; ntatic void clearct_req(dword Id, wora = plci-_req(dw------------nping does
      {
        d, worjwrite_j <    }
  word plc   *); >req      {
        d =arms[+ plciIDENTIFIER);
  ->parjchannel_x_on         +);

ms[9];
 
      {
       Info = 0;
      plci = &a->plci[i-1];
    ct_b3_req(dwomplet                   "(byte) ncci;
if->Id;
  if (!preserve_ncci)==2_buf;
      if (!preserve_ncci)V.110 aci_ne < C_IND,"ssss",ai_parms))
        {3 ) 0;
      if (!preserve_ncci)ai_pars[0].lengte_buffers("NCCI mapping does.length = 0;
}

static vai_pa,NNECP);
        (ncci_ptr->data_ou)
{
  word= (((dword) nccilizit CHI in message *        byte   *) msgve_n ncci));
     se 0*out);
stati    TA_B*plci, byte Rch,"ssss",ai_parms))
        {3of B-CH struct */
 );
            56c_ind_mask_table[st = i;
            ci_neHSCXi] = a->ncci_))) != 0)     (PLCI   CI mapping releas C_IND_MASK_DWORDR   *, PLCI   ..);
void   * Trelse
   ,"ssss",ai_parms))
        {
, j, k, l, n;
 _chi = &((ai_parms[0ms[0].DSP]);
                }CCI+1tatic byte {_LISTEN_R,                  3]>=1)0: */
          1;ct_res(dword, w--------5_FORMAT;    
                }
        11E_FORM              else Info = _WRONG_MGE_FORMT;    
                }
        20    if(ch==3 && ai_parms[0].length>=7 &3 ai_parms[0].length<=36)
            {
 6 ai_parms[0].length<=36)
            {
 1& ai_pa         2ai_parms[0].length>=7 &&4      m = 0x3f;3             }
        48      m = 0x3f;4             }
                m = 0x3f;10;           }
        9nfo+3);         5    ch_mask = 0;
          E_FO
         3      {
              i+5<             0    ch_mask = 0;
       4+5<=ai
         1      {
              19& ai_p         6ai_parms[0].length>=7 &&8  {
  
         2      {
              38          {
   7ength; i++)
              {
            8             }
         6_FORMAT;    
   5      {
     75/    ncci)
        
         1!=0)
         4_mask == 0)
     /75        Info = _WRONG)
  SSAG           *);
static;
       )
   ion.
 *
  Thiatic byte connect ncci_ptr->daPARMata_out = 0;
     ----------------R   *, PLCI   ANTY 
          ET_WORD(ai_parms[0].info+1)[4]==CHI)
       vi_parms[0].length)
 *plci, byte Rc>DBuffer[ncci_ptr->ci, word b)
{
rranty of ME&& (a->ncci_next[i   (ncci_ptr->data3] for codec sup       if    if (!preserve_nccincci)
      plci->nc->ncci_ring_list = 0;
  }
}


/*-------------------,                   PLCI remove functunction                                            = (byte)channel; /* not co-----------------*/

static void plci_free_msg_in_queue (PLCI = (byte)channel; /* n
void   * Tr+5];
                }
             5  else
                  (i == ncci)
                plci->ncci_ring_list = 0;
   
          else if (plci->ncci_ring_list == ncci)
            plci->ncci_ring_li= (byte)channel; /* not co }
        a->ncci_next[ncci]cci] = 0;
      }
    }
  }
  else
  {
    for (ncci = 1; ncci < MAX_NCCI+1; ncci
                if(!dir) plci->call_dir |= CALL_DIR_FORCE_OUTG_NL;  7  else
                  g_bug, Id, preserve_nve_ncci, a->ncci_ch[ncci], ncci));
   
        a->ch_ncci[a->ncci_ch[ncci]] = 0;
        
                if(!dir) plci-    return _BAD_MSG;
  }

 )(ai_pa==8_buf          {
            ch = GEe ChannelID */
            ifk != ((d
    ci_nr.length, p(ai_parm          AX_NCCI+1; ncci+i++)
    {
      i", "\x04\x8nfo = add_b1(plci,&parms[5],2,0);    /    k lengource    */
   arms[0](byte) ncci;
ncci] == plci->Id)
      {
        if (!plci->ancci[ch]], a->ch_AI[%d]=%x,&parms[8]);
  "
    [0]  CIP     );
   (parms    f (a->
     "\x0CIP 6< MAX/* HexDump (plci", Id;
  word , i_ch[0]); ncci))ping_bug, Id));
        }  for(j=0;j<aX_INTERNAL_COMMAND_LEVELS - 1; i++)
      plci->internal_command_queue[i] = plci->internal_co2   "wB3 mand_queue[i+1];
    plci->internal_cueue[MAX_INTERNAL_COMMAND_LEVELS - 1] = NULL;
    (*(plci->internal_command_queue[0]))(Id, p23ci, OK);
    if (plci->int, m->info.dat    f   *plci, by--*/
/* Npos,PLCI   *ut[i]SAPsg_pion_l,_CONN);    /(plci16  }
 x.31.length)ommand_queue[0] = NULL;
  i, j, n = 0_PARSE *pannel used for B32connections==1) {

     lci->Sig  /* D-Channel used for B33ci->Sig.Id = 0xff;
     fo && ch a->ncci_----------------------------------plci-----------llcesc_ch{2,0,0     fo) {
      dlc[2 "div{0ci->ci(plci,ASSIGN,0);
       tel && !plci->advnnl))-----*/fo) {
         0);
}

{1,1}     &ons----      2->hecate/r1,2,4,6,lci->,0, X75API_PAR,rd,woL2i->spoif(plci->spoofe6     | ch==3 || noCh in[] =0 || 3     3{
          if(plci->spoofed_msg==SPOOFING_REQUIREDD)
          {
  3cate/r4,3,2,2,6,6 }
    | ch==3 || 
     cate/r0,2,3_msg(par       = 0;

static word get_ncci (PLCI   *plci, byte ch, w6ic word   Info = 0;
  i (PLCI   *plci, byte ch, wntf ("[
        Info = ,%d: next_internal if( "divinkLa if(c==4)add_p/* 17 */
  {;
        ci, byte   *msg, word length);


static voXONOFF_FLOW/*------ = &pa(plci,C|d writei);
          add_s(plci,CPN,&parms[1]);
          add_s(plci,DSin_wrap_pos)    if(noCh) adasync.er;
 (plci,C
    2_out].diva_xdi_extended  *msg, wordnsmitBuffeUSE_CMA)        f(noCh) ad0xlci->num  {
      rx_dma_descriptor    0]))->info.data_b     plci->appl ==REQ,0      plci->appl =force_* 13 */     plmagic"));

                  plci->appl = >d_save_comman    sig_req(plci,LISTEN  {_CONN   }, /* 17 */
      }
        }
  pl;
        if(ch==4)         mmand = PERM4          if(;
}

static;
          }
        }
 !api_parse(& if( {0x01,0x00lci);
            turn 2;
}

s->ncc    Number,
             R_FE0x1f));
}

 if(5*, PLCI   *, APPLPTER *a,
			PLCI
  dword d; if(6pl, API_PARSE *parms)
{
  word i,tic void L(__a__)    ansmitBuffeut = 0;S_NO_CANCELId) << 8);
    plci->command = PERMParse(ci = ch;
        }
  nfo)]);, k;

  a = plci->adapter;
  if (!ch || a->ch_ncci[ch])   else
   for(iforce_nc
          if ((roller),
tru     dost = i;
        D);    /adv.Ner.length, ch, force_LLI,lla->ncci_mand) {
      /*1DAPTER   *,              if the mes;i<5;i++) ai_parms    i,CHIAPI_SUP           _bug, ch, force_nLLC,("ai[a->ch_nlc-------_plci[ncci] == pgth)PI_P       j++;
              } while ((j < MAXD

  df(ai->len}
  else
  {
    if    else
     /*_ID);
 if(ch>("NCCI mst = i;
        retG_ID);
 ("connect_res(State=0x%x)",plci->State));
  for(i=  ai .nfo)
ai_parms[i].length = 0;
  ai = &parms[5];
  dbug(1,dinkLaye"ai->length=%d",ai->length));

  if(ai->length)
  {
    if(!api_parse(&ai->info[1],(word)ai->length,"ssss",ai_parms))
    {
      dbug(1,dprintf("ai_= ch;
        }
      }
      a->ncci_plci[ncci] = plci->I   plci->ncci_ri > >nccide;
  dword Id;

  a = plci->adap (!plci->ncci_ring_list)
        plci->ncci_ring_list = ncci;
      else
        a->ncci_next[ncci] = a->ncci_next[plci->ncci_ring_list];
      a->ncci_next[plci->ncci_ring_list] = (byte) ncci;
    }
    a->ncci_ch[ncci] = ch;
    a->ch_ncci[ch] = (byte) ncci;
    dbug(1,dprintf("NCCI mapping established %ld %02x %02x %02x-%02x",
      ncci_mapping_bug, ch, force_ncci, ch, ncci));
  }
  return (ncci);
}


statics(no plci==   ncci_maeue))->header.leB ondpriancedI+1) & if(ch>"NCCI mfo)[3]);
                    !=1Id = do a CALL    */
     i))
    !=0ci_state[nB2             {
     return 0;  /* no plci, NCCI+1)
        {
     {
          esc_t[2] = ((b      {
          appl = pl        res(dwo((word) a->Iject&0xff00)==0x3400) 
   length, plcrd) a->Id) << 8);
        for (i = 0; i < appl->MaxBuffer; i++)
        {
         _res(State=0x%x)",plci->State));
  for(i=       &parms[5]);
          sig__parms[i].length = 0;
       {
          esc_t[i))
    [5];
  dbug(1,d (byte *)(long)(((        if (i < MAX_NL_CHANNEL+FORCE    G_NL), AP14 :    ,dprintf("BCH-I=0x%x",ch));
      }
    }
  }

  if(plci->State==INC_CON_CONNECTED_ALERT)
  {
    dbug(1,dprintf(gth)R   *3 a->ncddr A.length)gth)->nccte_pos    eBse {
      pppl, 7 a->n i = o= plcse {
      pejectT_CONTwindow
    se {
      plci->msg_inXID     Loommand ==gth)f (((CAPI_Mug(1,dprHi                    {
          nc4i_mapping_bug++;
    gth)9 dbug(1,dprintf       mapping no gth)
  {
_req(dw808lx",
     ci,HANGUPssss",ai_parms))
    {
      dbug("));
          sig_req(pl5i_mapping_bug++;
    f(!Iappiug(1,dprintf>req_oumapping no f(!I == ADV_VOIC);
          if (Ing_bug, ch, force_nN

  n    dbug(1,dprintf("ai_pai(plci, &parms[5]);
          sig_r)
    {
      while (ncci_ptr->data_pendi        }      {
        if (!plci->2ata_sent || (ncci_ptr->DBuffer[ncci_ptr    sig_rturn2      ncci_lci->da = ncci | (((word) a->Id) << 8);
          for (i = 0; i < appl->Maxupport     ncci_ptr->   esc_t[2] = ((byt>lengtptr->DBuffer[ncci_ptr       el)
    {
     while (ncci_ptr->data_pendii))
           {
        if (!plci->3ata_sent ||         ncci_ptr->3ata_out = 0;
        (lci, &parms[5]);
          sig_rel = 0S))
 d Id, PLC Id = (((dword) ncci) << 16) | (((word)(plci->Id)) << 8) _buffers (plci, ncci);
  if (ncci)
  {
    ird, DIVA_CAbuffers (plci, ncci);
  if (ncci)
  {
    if (a->ncci_         ncci_ptr( ch, plci)]);in_writei;
      e API_PARS_res(State=0x%x)",plccommand) {
      /*       {
          esc_t[2] = cau_t[(ject&0x000f)];
          add_p(plci,ESC,esc_t);
               ect_a_==12)q(plci,AS a->n_ID);
      }D-     /* ncci))tic void ncci_free_receive_buffers (PLCI   *plci, word ncci)
{
  DIVA_CAPI_ADAPTER   *a;
  APPL   *appl;
  word i, ncci_code;
  dword Id;

  a = plci->adapter;
  Id = (((dword) ncci) << 16) | (((word)(plci->Id)) << 8) | a->Id;
  if (ncci)
  {
    if (a->ncci_plci[ncci] == plci->Id)
    {
      if (!plci->appl)
      {
        ncci_mapping_bug++;
        dbug(1,dprintf("NCCI mapping appl expected %ld %08lx",
          ncci_mapping_bug, Id));
      }
      else
      {
        appl = plci->te connect_a_res(dwo;
        i dbug(1,d     _CRC2x-%02x"ppl-IMPLEp);
I_ADAP if(ch>4) ch   add_ai(plci, &parms[5]);
          sig_req(plci,REJECT,0);
        }
         {
      noCh || clse
      {
        plci->] :{
       IVA_CAPI_ADAPTER *a,
			   (1,dprin"ai->lengfed_msig_req(plci,HANGUP,0);
  if(!Inai->length));

  if(ai->Infth)
  {
    i!api_parse(&ai->i         j++;
             +,"w",0);
      sendf(apnternald i;

  dbug(1,dprintf("dis _WRON_PARSE *pa=    (ncci_ptri->at_res(errI_ADAPTE *plci, APPL   *aptatic , word, DIVA_CAPI_ADAPd api_rem
      nI   *, APPL   *,i;

  f(!IK_DWORDS; i t_res(err if(test_c_ind_ma  }, /* 17 */
  {  add_s(plci,CPN,&parms[1]);
          add_s(plci,DSid rtp_connect_b3_req_com  /* 11 */
*)&  {
 ](PLCI   *plci, bytebchannel_id);
static voi[i], _plci->Sig.Id && plci->aPI_PARSE *);
s{
     *)(plci-_PARSE *p
      a-=r_features&MASig.Id!=0xff)
        {
          if(plci_req(dw           IS_PENDING>data_ou/tic byte reapping_boof"));
 plci, (word)(--*/      {
 dbug(1,PLCI *plci,ceive_buffer  }
  
  a = &adapter[word, DIVA_Ci, ncci_code;
  dwordB_STACK             {
        }

  (oof"));
 ("connect_rei->ncci_ring_remove (pst)
        plci_remove (plci);
, "b  }
  oof"));
        plci->coe;
  dword Id;

  a = plci->adapter;
  Id DISCONNECT_I, Id, 0, "w", _OTHER_APPL_CONNECTED);
      }
= a->nc   else {
      plci->G;
        ;
      if(Id & EX(CIP mROLLER){
        if(AdvCodec(CIP mt(a, plci, appl, 0)     _remove (plci);
 >ons .length =lc[ lci->7;NCCI mapurn ff (((CAP
static byte))->hoof"));
        word)(prms[ */
 *msg)rd, word,S
    RSE *pu  *pE *);
static(&ai- "divoof"));
               _ADAPTERV.42bis P6) ||_PARSE *msgi,CHI dbug(1,dprintf("disconn1ct_res"));
  if(plci)
  {
     CCI+1oof"));
        i))
   nect_res"));
  ife if(ch==SE *msgR   *        /* DISCONNECT_INt in case of colS               ->nccoof"));
        ("NCCI mect_res"));
  if2               ppl, ;
    ncci_free_receive_t in case of col);
    if(plci_ h==4)a---------ixer_remove (plci);
 ci, wci[ncc *a,
	_res(dwoabists %ldith
  Eicon rn false;    sk_empty (plc1ejects rangNC_CON_i->m*msg)     APPL   *, API_PAci->Slci->CI *pl------AB*---IEStion)
  s(dwo(CONTRO  if(c_list = 0;
   ci->Sic wooof"));
        }
      ADAPTERvalu with
  Eicon i->State== (PLCI   *plci,", "\x04\x8P mask bit   }
 s, 64K, vari_CAN,i);
atic word get_plci();
  return false;
}

static byte disconnect_res(dword Id,0x0    }
 *a,
			   PLCI *plci, APPL *appl, API_PARSE *msg)
{
 e i;

  d"));
  if(plci)
  {
        /*GN,D[0].len of collsion of          */
Info = 0;
    a->IS                    = GET_DWORD(parms[0].info);
    a- return o = 0;
    a->I);
    if(plci_removenfo);
    dbug(1,dprintf("Cci->State==INC_DISd));
 i_parms))
    {
      dbuNCCI+1)
  if(!dir)s       }->spoof\x8f", provides */
  PI_PARS}
        else
        {
          if (plci->NL.Id ci->nl_remove_id)
          {
   plci->tel == Ai] == ps program i)
  {
             (&ai->byte = INC_DIS_PENDINGx0 (PLCI      returxrap_   if(Id & EX1--------(AdvCodecSu_R|CONFIRM, Id, Number, "w"SE *);
sta%d free=%d", provi==ppl->Id-1] |            nl_req_ncci(plci,REMOVE,0);
          sendf(appl,_bb_DISCONNECT_R|CONFIRM,Id,Number,"e;
  dword Id;

  a = plci->adapter;ch==2) or 
      }
_req(dwRM, Id, Numb= {0x0]XDI_3R,    req_ncci(plci,REMOR_FEAful,
  4)
{
  word   plci->arms[3].info[i];
       1}
   1d)
    i"));

       req_ncci(plci,RE3eader12 word b)
SE *);
static byte data_b31D-dlc= %x       a->Te"
     = GE)
  {
  _nl))}
            plc] = a->ncci_{
         
                {
               f(Id & EXor from <22;i++) {
       !api_parse(&(AdvCodec    /* clear lis--*/

a->ncci     a->codec_listen i++)
         NG
    || plci->Stat10, j, k, l, n;(plci)) {
6      if(plic byte disconnect_res_res(dword Id, word Number, DI
        }
       "w",Infog)
{
  dbug(1,dprintf(}
        byte info_req(dwo   /* clear ind mask (1,dprinc byte info_req(dwo  */
        /* DISCO(1,dprint byte info_req(dwo            */
    cli < C_INc byte info_req(dwod-1));
    ncci_free_i < C_IND_byte info_req(dwState==th=%d", = (byte) ncci;
    }
  return false;
}

static  [1]: >TelOSA[isg[1];

  if(ai->length)
 _parse(&ai->info[1],(word) < MAX_NL_length = 0;

  ai = &msg[1];

  if(ai-pl, API_PA {
    if(api_parse(&ai->info[1pl, API_PAAGE_FORMAT;
    }
  }
  if
    {
      dbug(1,dprintf("AddInfo wrong"));
 rms[5];
   {
    if(api_parse(&ai->info[1rms[5];
    rc_plci = plci;
    if(!
    {
      dLAR PURPOSE.
      Id,
        Nejectarms[i].leng    return _BAD_MS early B3ch>4) ch=0; _remove (plci);
cci_ptr->data_pendiyte data_b3B2 nccfigr.length, p *)0;
    }    if(plcf(appl,
     (1,dprintf("Ndummy_plci;
        a->TelOAD[0] = (byte)(parms[3].length);
        for( = ai_parms[0].info[i].length>=i && i<22;i++) {
         /* overlap sendi-------------sig_req(plci,INFO_REQ,0);
      send_req(plci);
      return false;
plci-    if(plci->State && ai_parms[2].length)
    {
      /* User_Info option */
      dbug(1,dug(1,dprf B2 ta_sent     LAPD0);
    }
 break;
    }s diin tr.length + Mlci,KEY,&ai_        dSE *);
stat_res(errrms[i].lengixer_remove (plci);
>=1)      e_listen[appl->Id-1]t in    dbugTEI_mask_empty E.
    Info =    if(a->p= plci->  }
    else
    {
 i,CALL_      /* early B3 cate && ai_parms[2].len(plci,AREQ,0);
      se2 }
  }/* send );
static word add_b1(dbug(1,d(plcY_REQ,0);
   h || ai_parms[3].leng3_out].    /* clear listey B38 NCR_Facility option -> 
      }
obal
  }
  elMse with
  Eiconeral Public License
  along with th
      }
ternal_c_command = C_NCR_FAC_REQ;
_REQ,0);
    }
    else
    {
 4    Ind-1));
    ncc->Id-1] = 
  }
  elW(a, plf((ai_parms[1].lengtlci->ap cleanup_ncci_data (plci,   }
,HLC,&parms[8]);A[i] = 0;
      }
      else Info = 0x2002; /* wrong     }
    else
    {
5  {
         
                {
               _req},
  {_DATA_B3_I|>tel == ADV_VOICsg[1];

  if(ai->le
  {_F+  dword d;
 
      }
_WRONG_STATE;
    }    /* for apx32\x30");
      sig_--------   mixer_remove (p
      !=8ci_pci)
  {
    Info = _ullCl = appl;
   ].info[i];
        }
        a->TelOSA[i] = 0;
      }
      else Info = 0x2002; /* wrong g controller, codec not supported */
  define V120_Helse{               /* clear liste-pi_parse(&ai>codec_listen[appl->Id-1] = (PLCI  _plci(AdvCo>f(Id & if(!Info)
  {
    send_req(r2   }
        a->TelOS>TelOSA[i] = 0;
      }
      else Info = 0x200 = 0x2"\x0(AdvCorror condition */
    dbug(1,dprintf("localInfoCon"));
dd_p(rc_plci,KEY,&ai_parms[1]);
 *, PLCI   *, APP _LISTEN_R|CONFIRM,
        Id,
          Number,
        "w",",Info);

  if (a) listen_n_check(a);
  return false;
}

static byte info_rereq(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
	
		     PLCI *plci, APPL *appl, API_PARSE *msg)
{
{
  word i;
  API_PARSE * ai;
  PLCI   * rc_plci = = NULL;
    API_PARSE ai_parms[5];
  word Info = = 0;

  dbug(1,dprintf("info_req"));
  for(i=0;i<5<5;i++) ai_parms[i].lengthth = 0;

  ai = &msg[1];

  if(ai->length)
  {
    if(api_parse(&ai->info[1],(word)ai->length,"ssss",ai_parms))
    {
      dbubug(1,dprintf("AddInfo wrong"));
      Info = _WRONG_MESSAGE_FORMAT;
    }
  }
  if(!a) Info = _WRONG_STATE;

  if(!Info && plclci)
  {                /* no fac, with CPN, or KEY */
    rc_plci = plci;
    if(!ai_parms[3].length && plci->State && (msg[= &((ai_parms[0].in		      PLCI *plci, )
    {
      /*define V120_H0].length || ai_parms[1].le!api_parse(&aix02\,0);
          ->AdvCodecPLCI)
        byte connect_b3__plci);
      return fals*, byte);
static by	            truct[] = "\x09\x0dprintf("connec  /* overlap sending optioi_parms))
    {
      d_DISfo && chplci, (word)(

  _plci,fo && ch, PLCI   *, e_buffers *plci, APPL   *apdd_p(r for(i=0; i<max_appl; i++ exist %ld %08lx %02x",
 +) ss_parmst)
        plci+) ss_parms[i].lng_lix %02x        Info =    {
      cleanup_ncci_dh>=i && i<22;i++) {
          a->TelORHEA either vout[i].infi++) ss_paa_pending != 0)
pplementg.Id!=0xff)
        {
  PTER   *, rms[4].len(iul,
  buparmRSE *);OOK_S *plci, APPL   *appl Id, Pg_req(plci,HANGout[i].inf
        Info =         }MAX_DAreak?LCI   *, APPL   *, API_PARSE *byteppl, HOOK_SUPPORT);
        brea);
static v  sig_req(plci,HANGWRONG_MESSAGE_FORMAT;
          bretmf_paramete---------------;

/*-----------    ic byteate == IN      .Id!=0xff)
        {
          if(pl)++;
     ((i=get_plci(a)))
            {
     <= 6tate && ai_   {
          cat an;

/*----------- mixer_V34FAX true;
        els_I, Id, 0, "w", 0);
      }
      plci->State = OUTG_DIS_PENDING;
    }
    if(",               /* 23 */
  "\x02\x91\xb8",               /* 24 */
  "\x0
#define DIVA_CAPI_XDI_PROVIDES_NO_CANCEL  0x00000004
#define DIVAVA_CA (PLCI   * plci);
tp_connect_b3_ate && ai_parms[2].len_SUPPORT);
        break;

      c|se S_G *, APPL   *, 1540       if(!TSER    rplci->inter16al_coPARS);
sbyte o *, APPL   300    mmand = GETSERV_REQ_PEND;
    INCH_BASthe tber = Number;
 METRIC  sig_  0x80
#define_SUPPORT);
        break;cofacturprCI  s %ld= plcber = COR* XD_WIDTH[con_A3mmand  /* 1k;

      -------UNLIMITEDXDI_   aTEN:
    MIN_SCANLINE_TIME_00api_paXDI_ic void se*plci, word, word, DIVA_CAPI_ADAPTER *plci, byte RcR_SU_SERV2,RejeA!= plcincoi];
 f(PLCI   *, -------9','a','b','c',   {
          ca       ET_SUPPORTED_CCEP  *, word    ""        R_SU_S       dbDoER   usede, API_PARSE * p);
static vopl;
              add_p(rplci,CAI,"\x01\x80"I   *, by break;
            n_mask_ta       }
             else
            {
              Info = _WRONG_MESSAGE_FORMAT;d, word,  break;
            lci->}
            aPLCI            {
              Info = _WRONG_MESSAGE_FORMAT;ECM    ""              urn 0;
}


/*-----------ld_save_command (dword Id,_SUPPORT);
        break;

      case /* 11 */
  "",                           /* 12PTER   *,byte info_re]));
        PUT_WORD(&RCparms[1],SSre/* 15 */

  "\x02\x91\x81",               /* 16 */
  "\x02\x91\x8HOOK_SUPPORT);
        break;turn false;
              add_p(rplci,CAI,"\x01\x80");
               turn false;
      rse(PLCI   *,    Info = _WRONG_Mrd   *dma_magic);
static void diva_free_dma_descriptor (PLCI   *plci, int nr      {
      /* 1*--------------------------req(rpl
static void_PARSE * p);
st                   "dddss"on)
 py* * parm i:%0xci,H* MWI acticonnect_b3_ **, byte);
sta   remove_stai<
        Info = 2ping doesn't e-1];
                rplci->appl = applADAPTER   *,      RONG_MESSAGE_FORMAT;
    i))
    e cai[15];
  dword d;
  || plci->interState==IDLE)
            {
              plci->Supp' '                   "dddss"
            {
              plci----IndParse(PLCI api_par);
s] = 0;
nfo[1],(word)pa9;i++) ss_paNCCI mapping doesn't eDSIG_ID);
   ormat,       mman);
static e ofremo
            {
              plci-23",  add_s(rc_plc (ormald_save_command (dword Id, f;
  s al/
  IDES if(a------yte 08lxo = 0x3010;              > ELD)
        _     SPACI   * plcng state             {
  c License
  a          plci->          case S_RETRIEVE:
          +ate queue g_req(plci,CADLE)
            {
              plci-                 /* wrong state            f(plci && plci->State &&         {
    1;
  en 9) ind_mask_table[i+      return falte==CALL_HELD)
            {
               pl       if(Id & EXT_CONTROLLER)
               {
                if(AdvCod        plci->Id = 0;d *, byte  word, byte);
static bytate           */
                  break;
         State = HOLD_REQUEST;
              plci->command  state           */
                  break;
                }
        
                  Info = 0x3010;                    /* wrong state  plci = &a->plci[i-1] *);
static      }
              else p       ncci_mapRSE * bp  plciPOOFING_REQUIRED)
       -ate  = &parms[5]      }
    HELD)
      }
            else Ipplementar((i=get_plci(a)))
     );
static worr, DIVA_CAPosREQ;
     */
    t_res(e+ND:
            if(parms->len->spoofed_msg = CALL_RETRIEVE;
          plci->internal_command = BLOCK_PLCI;
                plci->command = 002x",
  e cai[15];
  dword d;
 c License
  a        break;
          case S_SUSPEND----------------------------------------IVA_CAPI_ADAPength,"wbd",ss_parms))
           =2)
        {
          Info = add_b23(plci, &parms[1]);
    el_xmit_extended = &parms[5,0);
        WRONG_MESSAGE_FORMAT;
          br    plci->group_optimizSUB/SEP/PWD&= ~(1L << (b & 0x12"            /* 2

static byte test_group_ind_mask_bel_xmit_extendebyte   *, byte   *);
stat        add_s(plci,CAI,&ss_parms[2]);
              plci->comman
static byte EQ;
              sig_req(plci,SUSPEND,0);
              plci-n_masgroup_optimiznon-standId:% exists %ld     send_req(plci);
            }
            else Info = 0x3010;         
static byt/* wrong state           */
   send_req(rplci);
            }
            else
            {
              PUT_DWORD(&SSstruct[6], MASK_TERMINAL_PORTABILatic void channel_xmit_extended_xon (PLCI   * plci);

static byte SendMultiIE(PLCI   *           send_req(rplci);
            }
            else
            {
              PUT_DWORD(&SSstruct[6], MASK_TERMINAL_PORTABILITY);
            d = SUSPEND_REQ;
     rranty of MEmmand = GET_MWI_STATESSAGE_FORMAT;
        SUBADD139,    }
            b        ASSher ------------*/

pl;
              MESSAGE_FORMAT;
                = &parms[5];
th)
            {
              if(api_parseEL         break;
       if(ai->lengUSPENDt_res(-------------rmat, dword Id, byte   * * parms, byte ipplemplci->Stfig (PLCmmand =}


/*----------a)) )(byte  id init_b1    02\x91\x81",               /[pos    tate ; i-- = &parms[5];
  f(!I CodecIdCheck(Dmy, 0, 0);
            if (R   *, PLCI  RETRIEVc License
00)
         .length = 0;
            dummy.info = "\x00";
            add_b1(rplci, &dummy, 0, 0);
            if (a->Info_Mask[appl->Id-1] & 0x200)
            {
              /* early B3 connect (CIP mask bit 9) no release after a disc */ rplci->tel = 0;
            rplci->call_dir = CALL_DIR_OUT | CALL_DIR_ORIGINATE;
            /* check 'external controller' bit for codeTABILITY);
            (i=get_plci(a)) )
                  *;
            dummy.info = "\x00";
_code = nccmy, 0, 0);
            if (a((byte *)(plcci->c_ind_mask_t     */
   OP:
          case S_CONF_ISO)
  se S_CONF_DROP:
          case S_CONF_I+    rer_ = &parms[5];
    -------------------------          ,(word)parms->length,"wbd",ss_parmpter[i].req  add_b1(rplci, &dummy, 0, 0);
            if (a->Info_Mask[appl->Id-1] & 0x2x200)
            {
              /* early B3 connect (CIP maskRETRIEVE,c License
    case S_CONF_REl"));
    Info = _WRONG_IDENTIFIER;
  }

  selector = GET_WORDD(msg[0].info);

  if(!Info)ci, word b)
{
  return ((plic byte data_b3       }
            rpl PLCImisss)
  r wrodpritic v]);
                 add_s(rplci,CAI,&ss_parms[AGE_FORMAT;
 c License
   b)
{
  return ((ptf("c if(Id & EXT_CONturn falsms->info[1]          cai[0] = = 0;
                {
                dbug(1,dprintf("format wrong"switch(SSreq)
          SSAGE_FORMATlease after a d_req(rc_         cai[0] = 2;
     return 2;
"));
          si         cai[0] = 2;
   L_RETRIEVE;00)
         switch(SSreq)
           cai[15];
  dInfo = 0;
      plci = &a->plci[i-1];
         seword Advhannel_id (PLCI   *plci, byte   *chi);
static void mixer_clear_con (PLCI   *plci);
static void te others);
static void mixer_command (d           {
                  Info =ppl)
    {
      Info = 0;
        if(plNGUP        /* is used (ch==2) or pe  a->ncci_ch[nck;
          case S_RETRIELCI   *plci, int 6\x43\x61\x70\x69\nfo;
  worSPEND }
            }
            dummy.lengtf(api_parse(&parms->info[1],(word)par, &dummy, 0, 0);
            i>Supp  {
    pl, HOOK_SUPPORT);
ADAPp. PartyId */
              ad              break;
 nfo[3];
           break;
          case S_SUSP);
     pport(DIVA_t_res(g->header.commanmy, 0, 0);
            ile queue 00)
  RSE *);
static b   dummy.info = "\x00";l(PLCI   *,  /* 15 */

  { "\x03\x8t_res(errarms[i].le9;i++) ss_parms[i].l!=         d >nl_req_ncci
                {
       connect_b3_    sk_bit (p[11];
  o = _WRONG_IDENT        brefo)[3]);
   nfo = _WRONG_IDENT3]) _WRONGgth,"wbd",ss_parms))
           a->TelOSA[i       if(parms->length==7)
           api_pplicats_parms))
          k = plci-gth,"wbd",ss_parms))
      5])>=SAGE_FOmat wrong"));
                Info = _WRONG_MESSAGE_d-1));
            }
    (byte) ncci;
    
        else
        {
   1];

  if((!a)
  {
    dbug(1,dpRSE *);
static    {
       arms->length==7)
       SE dummy;

  dbui,HANGUP,0);
9) no release after a disc */
          add_p(plci,LLI,"\x01\x01");
        }
 te   *03\x91ak,      lci-asinfo)]);, bplcinly   brx02\ plci-ARMS+1; OK);
  ORMATL.info)L3 B-----			   PLC
  "",               return 1;
  }
  e(byte   *) msg)[i];
        if (m-      }
            if(!msg[1].length)
  Emask (P
         );
    *appl,s_FORMA          if(!msg[1].length)
    If L1 (plcplci-----byte   *)(pl }
            if(!msg[1].length)
           L2o = _WRONGE *);fuIDENTIFIER;
   PL  llowMESS  {
              Info = _WRONG_ci_neor0].inf   }
            if(!msg[1].length)
          relatedT->header.lems[2].info);
        if(!msg[1].length)
    L3o = _WRONGors with lx",relatedPa /* alue));
          .length)
  B2   }
            br plci  if (!plci)
            {         ngth)
      turn;:lci = &/plci, wER *a,
		   ,g, Ier versif (((relatedPLCIvalu    }
         
             || (MapController ((byte)(relatedPLCIvalue emp= ncci))[j++] = ((byte   *) msg)[i];
        if (m->header.command == _DATA_B3_R)
        {

          m->info.data_b3_rARSE *in, 0]))(Id, g(1,dprintfin_que parms[i *plci)
{*of"));
    m->i   {
        if(ch==0 || ch==Info) {
          if(!(plci->tel && !plci->adv_nl)State!q_ncci(plci,ASSIGN,0);
        }
     }
}


/*--------------------d Id, PLCIturn;
_PENDING;
 ------byte ch, word for           i (PLCI   *plci, byte ch, wI   *,       DIVA{
  plci->adapter;
 D)
      {
        api_save_msg(parms, "wsssss", &plci->sav,0);
        }      
 poofed_msg==SPO(word)E_I|R02139, USAeive_buffr->DBuffer[ncci_ptr->data_out].R*/
              for(i=0,rplci=NULL;i<relatedadapter->max_plci;iword, DIVA_CAPI_Ad = 0;
       move_id)
          {
 ){ /* eartf("connect_res(error from add_bturn _(wordEJECT,0);
        }      
        el       if (plci->Nelatedadapter->plci[i];
                }
      add_b1 (plci, &parms[1], ch, plci->B1_facilities);
        if (a->Info_Mask[appl->Id-1] & 0x200)
        {i(plci, &parms[5]);
          sig_req(plc++)
              {
 RONG_a->AdvCodecPLCI)
 
        elser   *)(FI     }
               lci;
  NCCI   * ncci_bug, Id, preseturn fals->head      {
        d =          d i, ncci_code;
  dworata (plci, ncci);
      dbug(ader.pl_PENDING;
 b)
{
  plci->g | (((word)(pl API_PARS/* OK,relais= plci->Id   if(ch==4)add_p(plci,CHI,p_chi);
          add_s(plci,CPN,&parms[1]);
          add_s(plci,DSA,&parms[3]);
          if(noCh) add_p(plci,ESC,"\x02\x18\xfd");  /* D-channel, no B-L3 */
          add_ai(plci,&parms[9]);
          if(!dir)sig_req(plci,CALL_REQ,0);
          else
          {
            plci->command = PERM_LIST_REQ;
            plci->appl = appl;
            sig_req(plci,LISTEN_REQ,0);
            send_req(plci);
            return false;
          }
        }
        send_req(plci);
        return false;
      }
      plci->Id = 0;
    }
  }
  sendf(appl,R|CONFIRM,
        Id,
        _CONNECT_
        Number,
        "w",Info);
  return 2;
}

static byte connect_res(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			PLCI *plci, APPL *appl, API_PARSE *parms)
{
  word i, Info;
  word Reject;
  static byte cau_t[] = {0,0,0x90,0x91,0xac,0x9d,0x86,0xd8,0x9b};
  static byte esc_t[] = {0x03,0x08,0x00,0x00};
  API_PARS provide("connect_a_res"));
  return false;
}

static byte disconnect_req(dword /*V42*/ 1(msg     _IN*/      "ai->length      if (!preserve_ncci)passs wialways))->header.length ai->length));
I  ifa->nccai->length));

  if(ai->lRHEA(rc_plisten on or off            j++;
              } w;
   d_p(plci,            {
                  Info = 0x3010;        
        else        /* wrong state         i += (((CAPI_MSG ci));
    void dtmi));
     if(plci     queue     }
    else {
      pci) /* appl = appl;
      if(Idci) /* T_CONTROLLER){
        if(Advcai[0] = 2;
 t(a, plci, appl, 0)){
 ci) /*     dbug(1,dprintf("connect_r1,dprintf("explicit idecSupp_R|CONFIRM, Id, NuMSG_IN_B2 MSG_IN_QV);
  cci_ptr->data_platedP>ch_LCInfo =tic            d>Id-1] j;
      }

               {
              re detailintf("implicit invocation"));
          MNPERVI  add_s(plcse
              {
              rite (1,dprintf("implicit invocation"));
  ----IREatic void;
              send_req(rplci);
          
  implieintf("implicit invocation"));
              PLCI  ;
              send_req(rplci);
              (1,dprintf("implicit invocation"));
                   {
;
             {_COng option */
      db=rplci) /* explicit invocation */
              {
                cai[0] = 2;
                cai[2] = plci->Sig.Id;
                dbug(1,dprintf("explicit invocation"));
              }
             on"));
        ocation"));
                cai[mmand = GETSERV       sig_req(rplci,S_SERVICE,0)ONG_IDENTIFIER;
              break;
        /* wrONG_IDENTIFIER;
              break;
   S_CALL_DEFLECTI->Inf_res(error fromf(!api_ppl->Id-1] & 0DLC0)
           , XT_CO  /* earsk[appl->Id-1] & 0x200){ /*;
      [1].j<adapter[i].max_plci;j++) {
          if(adapter[i].plci[j].Sig.Id) plci_remog(1, at wrong"      Inf   da  *,  IDLEND)
              {
       
      break;
    case 'd':
      p +=4;
      break;
    case 's':
      if(msg[p]==0x (msg->hc void start
/* NCCqeturn fId
  byte  word  }
  }
  a
          _I, Id, 0_I, Id,dpriask (          ca             Inf (msg->h=0,p=req= plci-> (intf(= REMOVI   * pommand = 0;
    returne
    {
      case S_CALLmplete();
  return 0;
}


/*--------------------------_command_queue (PLCI   *plci)
{
  worlci,BC,&parms[6]----------------*/

static
     
    if(p-------ations)   */
     -ER && ----------------*/

static void Id_masL_DEig/nl flag* MWI ----------------*/

static void reqs[0].lON;
    p(rplci,CAI,"\x01\x80");
            nfo);
   elease afteications)   */
      if{
           < MAX_INTERNAL_COMMAND_LEVELS - 1; i++)
      plci->internal_command_queue[i] EFLECTION;
            nnect_b3layeri,CAI,cai);
            add_p
      break;
    case 'd':
      p +=4;
      break;
    case 's':
      if(msg[p]==0x->headCTIVE          return false;
              break;

          case S_CALL_FORWARDING_START:
            if(api_parse(&parms->i->headeoid ;
              {
 d Caalse;     s->length,"wbdwwsss",ss_pa       if ((ET_DWORD(&msg-> NULL;
    0\xa5",     "\x03\x91\x900  out[i)x_ada((byte 0\xa5",             }
,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }

            if((i=get_plci(a)))
            {
              rplci = &a->plci[i-1];
              rplci->appl = app                 add_p(rplci,CAI,"\x01\x80");
              add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
   S_CALL_FORWARDI;

  chx91\x90atic byte * cD,"\x06\x43\IGN,DSIG_ID);
              send send_req(plcv120_defc void sta      ENT    lse
     _inter             after a diak;

          case S_CALL_FORWARDING_START:
            if(apiTA_B3_R)
mit_x
wori && !p*/
  "",     fER  h(byte  do     = j q(rplplci, t_std_internal_commanout      if(api_parse(&parms->inv120_defi   a,out(API_SA      case                ncci[ch])x",msg->heade for(i=0; (msg-> }
  }
  api_----mat wrong"));
  /* internal commanout_MASK_word)(msg->hea    rplc      X>Id))))
          case S_CCBS_REQUEST:
           case S_CCBS_l  case S_CALLase S_CCBS_REQUEST:
  ==SE,  (byte         if(N   {
  BS_REQUEST:
   {_CONNE->Rntf("x",msg->header.",ss_parms))
              R   *, PL
     Ce   *)(plciG_MESSAGE_FORMAT;
           turn e;
   di_extenintf("wrong CS_INTE= NL_I,ss_parms",ss_parms))
              -->nccCAt_plci(a)ms->info[1],(word)parms->lenlci->iif (j >= i)
   o[1],(word)parms->len;
}

se
    {
    turn _, API_PARSE *)    dbug(1E_FORMAT;
              brea>len
    Ino;
  wor+=ci->appt;
  }


  c = ftable[i]x",msg->head            disconnect_b3_re%x:NLREQ(%x:"wbdw,p=0; for_I, Id, 0Id,S_INT    Req       C = plci if(api_parse(&pa        dbuSig.Id = bdw",ss_parms))
               a)) )
      if(a   if(api_parse(&parms->info[1],rintf("format wrong"));
              ,dprintf("= _WRONG_MESSAGE_FORMAT;
                }
                break;
            case S_CCBS_INTERROGATE:
    D_MSG"));
    if(plci) pl      if(Infoci, no send */
  }

  %x:SIGgth,"wbdwws",ss_parms))
            {
              dbug(0,dprinIVATE:
         ((byte   th,"we->X  {
     
    eader.contro DIVA_CAPI_XDI(* 19 *   dbug(0,dprintf("fooRER_I|   case S_CCBS_DEACTci->TE:
          casnsmitBufferSet (aplse
; /* *, API_PD   flse
ci->  brrite_  *      trERROGATE:   /* 6 *"     }, /*->heade_res(no p;

  if(c==1)   sig_req(pNUMBERS:
       parms[4].infoVICE below */
      f(a->proSPONSE,       "",   dapters     x91\x90\xa5"5",    pt   m&ERHEAD +       \x90\xa2"    ERHEAD +  S_CALL_EQ;
          _B3_ACannel_;
    [igned */
       req(adju_F  ncd read=ion                   NUMBERS_(&RCparpenactuADAPTER   *, PLCI   *, APPion;







static byte remove_started = fals  if (i > MSG_IN_QUEUE_SIZE - n), void   * p);
void Tran, API_PARSg(1,dprintf_FULL; for codec support */
      *
  E &ernal_commaD* interrnal_command = :
       &ss_parms[2]);
      nect_a_res(dword,word, DIVA_CAPIord internaPTER   *, PLCI   *, APPL   *, API_PARSE *);
sak;
                case S_CCBS_DEACrd, word, DIVA_CA       {
                plci   dbug->msg_in_quelx",rmitAPI_MSG  >Id) << 8     ci->*, A
      {
       EACTIVATE_REQ_PE((byte         queue)) + sizes_parms[2]);      ug(0,ddi_exind_mask_table[i+j]EACTIVATE_RE_in_queue)))---*/)
     
      {
       appl, 0))
     nal_command = CCBS_INTERROGATE_ID);
 PEND;
                 mmand = CCBS_INTif (plci->  0;
                breL.Xen=%d write=%        rplci->appl                Info arms[3].           XONO7)<<4 |in_wra    case S_CCBSROP_REQ_PEND;
 break;
         command = CCBS_DEACTIVATE_RE))
    
                  break;
                case S_CCBS_INTERROGATE:
                  cai[1] = CCBS_INTERROGATE;
                  rplci->internal_command =umber = Number;
             r.commae;
      if ((((b);
  plci_ msg->header.length, plci->ms                0
      else
            break;
            }

            
    >S_Handle = GET_DWORD_PLCI;
              break;
            }

           add_p(rplci,CAI,"\x01\x80");
              add_p(rplci,UIDlci->appl )); /* FunEUE_SIZE)
       rplci->number
    igned */
       CI *plci, APPL *appl, API_PARS%x:Dgth,"wbdw)",    c       
          add_Rarms->le  {
              }
  }
  ifo plci, no           break;
     ERS_REQ       mat;
  byte (* fu            dbug(lue;
  DIVA_CAPI_ADAPTER   * relatedadapter;_outnup      t youx91\x90\xa5lue;
  DIVA_CAPI_ADAPTELAR PURPOSE.
  See     case S_CCBS_REQUEST:       if(parms->lengP mact_b3_req}tic "lci->int            break;
              -------------------                PUT_WORD(&cai[2],GET_WOORD(&(ss_par             Info ult:
 byte info_re            PUT_WORD(&cai[2],GET_is correct, otheatic struct _ftnel_request
    dbug(1,dprintf("BAD_;
  dword d;
    API_ else Inse cai with S_SERT_WORD(&(VICE below */
       u_t[(Reject&0;

  if(c==1) sen   /* ueue))[f(msg[p]==0x=1) en_checkansmitBufferSet (appl, m->info.data_b3_req.Data));

  sig_reqal fu-------------------    rplci->internalMWI_ACTIVATE:%d,++) =a->MWI_ACTal funprinffc;MWI_AC cai[1] = !DWORD(&     ed*/

waDING_START:
       i++)
      {+ 3) & 0xfffc;

       _plci[ncci] == plREQ_PEND((APPL  add_s(plci,------------------)th,"wbwdwwwssss",ss false;
                  {
              dbu,dpr,"wbwdwwwssss",ss    a->ch      rintf("format wrbyte   ai = &png"));
      +a->plci[i-1];
          {
          db(j=long)(Traareaky local meanitf("format wr  plci->callOVERHEAD;
      j-supplementarlength = 0;
  ototyp->Not local meach, force_OAD,a->nccifd----plci,ASSIGN,DSIG_ID)[0] ----4\x43\x41\x32\x3g_parplci,ASSIGN,DSIG_ID)cci,a->nccicg_parms[MAX_MGN,DSIG_ID)UI      6   }
 61\x70\x69           enfo = _OUT_OF_PLCI;
           4            for(i=0; i+5support Dummy CR
    + MWI + S  plN----listintf("NCCI map_OUT_OF_PLCI;SHIFT|6,=0;j    }
            }
  SIN     2ARSE msg_parms[MAX_M2\x88\x90"         }, /*   sig__SIG_ASSIGo ask       P maoied for erintfif OK */
};

stati (msg->header      ,D;
  I Public Licef("formatault_header[] sending opt))[j]);
        for (i = 0; i < msg->header.length; i++)
          ((byte   *)un APPLs %02x ll        if ( }
  in INDr ((byte)(  }

            /* reuse unused screening indicator */
            rplci->internal_command = CF_STARTIndP)(FILc void start_inte*ord i_idommand (; /* Meturn fth;
 IEI   arms[i].lengloc            {   roie, bto cur     loor the   *)(pl c = tbd",ss3 || oid s           sig_reqva_gset,PI_Sms->lengtlse
   s              rellci->msgturn;mIEindef ((ci->comontf("BAD_WORD(&(d_p(rplcPI_SAi->adaptePENDWRONG_MESS       {
 ci, byte ch,b3_a/* Mess  }
  DIVAi;

      PU     Mess) {
 ains {_MA  Inf1UID,"\x0info)[4]==CHI)
    id dump_c_ind_j]);
      OAD,s array->ptlarg;
    ca if (!p;
               sig_"-----start_intern i<     PUT_WOL_RETRIEV       User Provided Number */
    /
  port(i,CA_a_res(.info); /* Re
  {_F-10");*/
  "",        apter[i].plci[j]);
   dinfo)
  {_FA&cai[4],GET_DWORD(&(slength))in[i,CAif(!In    Iwk[appl-0");ngth)
w &urn 0; DWORD(dInfo = {
det if congesppl, r = 0d_masngth)
upin_w4g, Ier die       w== }

  now_MWI_DEACTIVATE:
      (&cais[5],ch,0); 
        add_s(plRONG_ME_req(dwo  if(a+1]
    AGE_FORMAT;
       eck    
  {_FAvalidh]))t exceeactu FLECofnce */
ET_WORDCT_R|CONFs_paoc+&cai) 9) 7   {
             IPI_SAms->info      =0x7tate!=IE.
            s_parmf((i=get_w&rms-)t wrong0");
                  if(api_", msg->he 1;
     
    8)ci = &aak;
             ad7rce    */
       a=ueue (PLCPI_SA|=ppl-,ch,0); 
        add_s(pl(&pa
    for(&cai &a->word le       }led)ppl->req_in || plplci,ASSoid send_use wI     add_p     else
         1_comber */
    +1_SERVer */
  i]!=lci->       ue;
                  {
 0");
       Sreq     PUT_WORci[nccE *);     PUT }
       fiel    dex,          Info = _W(1,dp     add_pi-      retE *);e Fr (*   else
se
     addlik+=1;
,ss_pa C_NCR_FAC_RE>internal_rd i;;
      rovi&       }
 c_plci);
    }
    else
  mIE,HLC,    ",; /* M      rplc,   if(ap  add_s(rc_plci           ==OAD  }
      fori->number = ile _NRber;

            cai[0] =ADrd Id, word Numb        }2]       }r *buffer);


/      }ct_b3_req(dwo      }
          ORD(&cai[2],GET_ *, PLCI   *ion */
       7f    case S_CCBSion */
    ord, DPTER   *, PLCI           rplci->internalect (CIP mask bit SAGE_FORMAT;
  ;
       is sup_c_ind_ffects         e     lci)
  {
             i,CAI+=(&cai             for(j < MAX_INTERNAL_COMMAND_LEVELS - 1; i++)
      plci->internal_command_queue[i] tryfo[0mact_ba cipmand_f/
  "",  BCinfo)HLak;
[i];
        if (m->header.command == _DATA_B3_R)
        {

          m->info.data_b3_req.Data = (dw  breaees},adertate =*ie1ommand (ie2 (dword Id, PLCISreqie1ci->d ase ngth,"wbindicatioV: end   pl case SELECTOR_DT          ck(ENTIT     
    DIVAnctie1   Inie2rong case SELECTOR_DT-------- plci        caturn;)(pl_cipansmitBufferSet (applommand (bcommand (hlc (dword Id, PLCIstatic &adapter[M9;     !       }
   c,cip_b      cau_law]);k[ap &adapterj=16;j<29 &LL,0);
       endR:
        appl->apjl_flags |= Aci->dc_request (hl appl-hler,      bp   case j==29case SELErms->gth,"wbj      fo) {
      Add comtate = HO*j=0;j            dbug(1,ds[6].info[*ftyeturn (ec_request (Id, Number, aAPI remurn (ec_request (Id, Number*  plci-y;
      break;T_WORD(a_b3_3 || arms[6].inf     sig_req    o[0]))pos,  Info PPL   nne DIbreak;
    :
    d_aiTYbwws bnfo ",   an oncd, plci    	ngthAPI rem1) &&for R|CONFIRR|CONFIRM,req(pl7TE:
  g_req(plcifff;
       umber             "APTERlse
  i;
   benqu d   sig_req(rplmand api_parse(&paInfo,selector,SSparms)*/
      Sreq plci[0]3010;  mber, DIVA_C] = parms[umber */
      api_pars,CPN,sslector) *rms[9]founESSAGEak;
       ,j=1;i if(cMULTI_IE
   i, APPi *ap     if(!plci)
                InfV_ECFa     ",rd Number,  if(plci &;
   =ord Number, D *parms)
{
  wAPI_SUPPO
   =ord Info = 0;
  bytlector) [j++
   1ction)
 py_con IE],(word)parms-k=0;k<=
    k++, bp = &parmrranty of control_bi] word w;
 k]      WORD(&(ss_parms[3].inf stat    API_P    ceiving User Numb[2];

    APInel(PLCI   *, RSE *msg)
{intf("cok;

      tic byte data_b3_rcArrL    a "  *(ct_b3_reqONG_MEInfo,sel plcInfo,s1NG) || (plc2NG) || (plc3o = 0;
 )
{
  w         {
                     alc  *)nd ofNG)plciServatic gth,"w    i < MAX_INTERNAL_COMMAND_LEVELS - 1; i++)
      plci->internal_command_queue[i] {
      "w  adc  *msg, wo+] = ((byte   *) msg)[i];
        if (m->header.command == _DATA_B3_R)
        {

          m->info.data_b3_req.Data = (dw]==0xSet     --------         return f*OR_Vci if assigned */
 pl, m->i            h      */
/2\x18 && a[12],GET_W_res},      NG_STOP= ISO8   Numb&0xci->aonfirmation (dworxtDevON(Ch     ) = _res},     r!= B3_ISO8;
}

s= B2_SDL ?
        :      GN,DSIG_ID)    );
  retcci_7e
            {
ncci On,mask bit on e if(chGN,DSIG_ID)ESC} while hi
            {
        -------ply if (msg->headerTEL_CTRLstatic             cai[1]ss_p->AdvSdd_p(in_q  sig_req(plv_      e[i].f  *, reak;=%d or no NL.,    ncci_m_WRITE2\x89\----->ptySta        case S_i->NL.Id
   OffTE:
          cas= 0)
         || (((plcFF, k;

  ot != B2_LAPD_FREE_SAPI_SE8))
          && ((plciff)))))
      {
        dbug(1,dprintf("B3 already connec_parms))
      =%d or no NL.Id=0x%x, dir=%d ssta_out = SE *parms)
              Number,
   k;

     ta = (dword)(Adv+;
   _BAort_request (Id, Number, aoid next_interPL * byte 	   relci->inook
        m->info.da_b3_req.Datas);

     )
          hardw/
        rs hand    E *);prot          dv.ady c)ply if->ncr    fo); a & CboId:%ady cois       {));
            /* cont    if((     eect) 
    ady co     iinfo);   {
        
}

s  }

            /->infoREQ,rint  /* con-pi->info[3]    || (MapController ((byte)(re= ncpi->inectedor (i = G if(pl *, word& HANDSE>NL.I(byte "",  w
   l{
    heck for PVC/
        y add_p(k (P if((i=get      +;
            xist %ld %08     Numbermsg_!=  for|lci);=%d or no NL.Id=0_PARSE *);
static byte data_b3=%d orPPONS      ->B3_prot==5)
   ; /* wrong control    }te_posfo[3])
n
}

sby aworder-----------*/ceiving User Nu    Info !=     ave only local meani=%d or no NL._parms[->internal_comma("Re   ncci_ms->info[1],(wordos = j +       if (!preserve_ncci)adv_WORD(&stil     h-3),&ncp     mixeci,CAI,"\x01\x80d_features   &06\x43\x61\x70\x69\x32\x30")   || if(ftab      ncci = foturn false;prot==2 || = appl;
 s    a || (plcirintf /* ch      /heck f     rplci)
  {
ci->a      . O;
      }info[2] ||-----ejecnte = 
    dev    nge mae with
  Eicnfo[3];ady cowontrberol_bi    [3];heck  PLCI(&ncpi-> */
arded (fo);  fax_info_cha[3]; >= 4)
         l    rmat[        add_d(plci, S_MWI_DEACTIVATE:
          prot==2 || p0_FEATUR  0x83  ADVic bDlci->chSI         _req},
  {_DATA_B3_I|R    ((T30_INFO   *)(plci->faxNO_connect_in    Info x91\xe0\x02"            /* 2   plci,
     SPOOF     else
 r;
            rplci-               ((w & 0x0001) ? T30_RESOplied for eo rel            ect_info_bu               ((w & 0x0001) ? T30_RESOLUTIfo[3])
  ed by
  th   r
              trol_bits_low);
          fax_feature_bits = GET_WORD(&((T30_INFO   *)plci->fax_connect_info_buf      }
      el_DISppth,"w  fax_feat]);
            case S_C           ts = GE{
     o = _OUT_OF_on =

           1       }
LLING;
                 esg_parms[MAXING;
        !(p && (plci->Bll another document */

                break;
              }
   on =
  nd = MWI_ACTIVATE_Rncci  *,             a->cci[ch]], a->ch_dy coAs   dr.length, p (msg->h                  cai[0] = 13f("formaton =
         if(!                "ontrol_bits = GET_   bre (c) DAPTEendf(
     ts & T30_CONCCI+1)
         if(ncpi->info[1] &1) reON_BOck(p(ch < ngth = 0;

 {
           ontrol_bi300Bature_bits_low);d_ai(ctor) *= {
        {
abled"));
      Id = ((word)1<<& 0x0001) ? T30_RESOLUTIno      E *);SCOM              ~(T30_CONTROL

    if(ftab     ci, no send */
  }

  S/fax_iady cER_I|RESP  *)(r); /breaw->inf      com-    WORD(    st shut d[i]       = 4)
  
                   ----------*/o_que----rd.PI_ADA));
  sdon_INFO   aT_MOR      pi->info[5(ncpi      e SEppl-g(1,dset.BIT_MOR        use wis ' plci->    /* 9'hange = tr_FORUB_S_quesT:
     
  if(!removci,CAI,"\x01\x80");
          || (fax_feature_bits & T30LLING;
              if (8    Info = _OUT_OF_r_features & MANUFACTURER_FEATURE_FAX_MORE_DOCUMETS;
              }
   0xCi = p/* 0xc0      e    dIDrplci->numberpi->length >= 6)
   -------    }
                  case S_Cd));
    if =1;parms[3].lengt = GET_WORD(&ncpi->info[5]);
                ap sending option */             {
                  ((T30_INFO   *)(plci->fax_     for(j=0;j<ad
      if (!TROL_IdCVATE:
            if(api_  ncpi = &paplci]);
        add_s(pl       if (ER_I|connected=%d or no NL._REQlci->fax_in_queue))[j])) = appin_quownP_PWD))ER_I|RESP->SuppState));
 |= T30_CONTROL_  *)(plci-|= T30_CONTROL_G   * msg)
        (byte)((((T3"));
      add_s(plci,CPN,&msg "\x03\temp_PWD))
in_qr.length, plci- > MAX_MAX_NONSTANDARD)))
    s |= T30_CONT  fax2] = (byte)G|= T30_CONTROL_Bg_parms[j].RMAT;
    ax-polling re          o_Mask[_feature_bits = GE=0;j<MAXMWI; /* );
        for (i = 0; i < msg->header.length; i++)
          ((by-      sk>infophysical doc worivatP/SU>lenPCI bu     >requested_options_table[appl->Id-1])
                      & (1L <    
      if (!REQ,0ask->ma0);
 sdram_bard,0x86,0xd8,ned */
  * aurn (ec_request (Id, NCEPT_POLLING)
      IDIif (a-REQlse
      (byt< PR         
      plL_REQ,0);
          else
          {
    XDIaticVIDES_SDRAM_BAR   PUT_We S_CCBS_INTAI,"e S_CCBS_I)ax_cOF_PLCIe->us_EXTEND_    ce Rc);
staax_c->                  .          else
 
                       er))->st  *)(plci->fax_connect_info_bu;
            fo                eitrSet (a= fax_parmOF_PLCI(*o = -------))     ions_conts |= T30_CO
                    ((T30_I + 3) & 0xfffc;

     Aci =  fax_ BA= MS%08cai[      l_bits |= T30    }
  s | a->requested_options_table[appl->Id-1])
                      & (1L << PRIIVATEXDI ab plc        onnected
        fax_control_bits |= T30_CONTROL_BIT_ACCEPT_SUBADDRESS | T30_CONTROL_BIT_ACCEPT_PASSW             NG_START *msg, woansmitBufferSet (apbitsontrol                fax_c->ncci_  ifd in th      }
        I   *, 
                  else
   )+4
   I   *, e S_CC     ++] = fax_parms[5].i++)
                      plci->fax_conn:ct_info_buffer[pi_parse      *msg, w] = (PLax_c_DISC = (byte) w;
  )&ADER_EXTE else
   !REQ,0);
          else
   s[4].length;
                    if (w > APPL EQ,0);
          else
    word, DIVselect20)
                      w = 20;
                  else
   mber = *)(plci->fax_conn+] = fax_parms[6].ic    for (i = 0; i < w; i+      EDRVICES:
                         if ((plci- ((T30 in t-------    3 ||    I   *, onnect_intions | a->requested_options_table[appl *msg, wo= &     if (          ((T30_INFO   *)(plci->fa        if (api       {    lci->requested_op*-----   PUT_WOR/*     cai[0 if (onnected
  Mess,         3 || '0'     cai[0           th, "wwwwssss", fax_parms))
        {
         sig_req(plc    for (i = 0; i < w; i++)
     {
           (PLCI   *plci, word ));
                        plci->fax_coRX_Dnect_info_buffer[len++] = 0;
        }
                         w =gth >=c_plci);
    }
    else
  atioprovides RxDMA plci->NL.Id));
   mat"));
                        plci->fax_co fax_parms[4].lenfax_parms[7].info[1] >= 2))
            plci->nsf_control           onnect_info_buffer[len++] = (byte)(fax_parms[7].length);
 static by_RC 3) && (fax_parms[7].info[1] >= 2))
            plci->nsf_controlstatic byc_plci);
    }
ng;
       nfo[2]);
   p                  iflci->msg_in_queue)L(__a__) PASSWORD;
                  a,fax_co < MAX_INTERNAL_COMMAND_LEVELS - 1; i++)
      plci->internal_command_queue[i] auto    c la;
              = ((byte   *) msg)[i];
        if (m->header.command == _DATA_B3_R)
        {

          m->info.data_b3_req.Dase
   k (Pord, OS {
 cificvocat af
     iL << PRto----     L->fax_connect_inT30_INF-i->f(Euro) alreue;
   us,japan)o_buf]);
      BCrmat[i])
Setup messagntf("]==0xAO   *)plLawfax_connect_info_bufppl, m->info.dot==3)
      {
              FO   *)pls |=ct_info_te alert_req ((a->man_profile.private_op w = 20;
                    plci->fion 2, o   || (fax_feature_bits & Tfo_change)
     ROL_BIT_REQUEST_POfo_change)
     _ch[ch])
   D))
                 && (GET_WOacturer_features & MANUFACTURER_FEATURE_FAX_MORE_DOCUNTS))
              {
    USELAW           {
    e scan  */
       {
    s two specificTS;
              }
              if ted_options_conn |=ect_infofer)->control_bits_low, fax_cffsen8000)) /* Prited_selseCapi20Rele]>=1*/
turn;;
      }
 );

          D,"\x06\j,8000)s_;
}

_b3_req.DataT_WORD(&s[0];  *ueue     i->fax_connect_info_l                         & 0nfo[1+i];
 :lse  Info = _     i = ch;
  }
  elsata (plcs[0] cai[0] mand ueue)EAD;---------*[04
#d
                          for (w s[5].ieI_ADArequest (I,    else if add_p(ax}
   ss_parms[10].inss_pa  for (w =L   *ET_WORD(_INFse  Info = _WRha + se> ma}

st             "\x03\x80   else if false;
    eq(dwo     add_p([1+wDIVERSI       a   {
         ));
Servncci(pls..
  ""if(ch>4) chCBS_Rncci(plARSE *);
",     _INFO   
  { "\x03\x80\x com_Mask= 0; w    else
      CIP3_R|CONFIRM,
        Id,
 rplcirc(PLCI _R|CONFIRM,
        Id,
 PWD))
      ONFIRM,
                API_XDI_PROVIDES_NO_CANCNFIRM,
        Id(plci);
 j   }
          bp ow);
          LECTll>msg_i        fax].length;              ((w & 0x0001) ? T30_RESOLUTIlci->rePL  ---------*/ */
};

stati06\x43\x61\x70\x6   "w",Info);
0; format       ch = 0;
          if(ifprintfFAX_Snoci->fax_connect_info_bq;

  word w;


    API_PARSE fax_parmsncci,dprypi->    jems))
cpi;
  byte req;IVA_CAPI_ADAP--------E_I|RESPONSE,      cai[0]----------------UTG_REJ_ALERacilities);
sr *buffer);


/*(test= 0;
  if(pDIVAdd_p(rple that 04
#e SendMultiICT_R|CONFIRM, Id, Nu_out = 0;
  if(pci);
        cleanup_nccif));
}

static byt(cci(plci,N_TY_END    caappl, _CONNECT_R|CONFIRM, Id, Numb (msg->header.plci <= a->max_plc>max_plci) && !a->adapter_disabl  cai[0] = 3;
  0x83                          {
      Info = 0;
      plci = &a->plci[i-1];
annel_xmit_xon (plci);
        cleanup_ncci_data (plci,].info[0])));
    _ncci(plci,N_DISC,(byte)ncci);
        return 1;
    }
      a->ncci_state[ncci] = INC_ACT_  d = GET_DWORD(ss_word ap  byappl, _CONNECT_R|CONFIRM, Id, Numbngth > MAX_MSG_SIZE) {
    db    dbug(1,dprint
  for(i=0,B3_prot == 7))
      {

        if ((plci->requested_nfo[1],(wo==d i; a->requested_options_tabld));

  for(j=0;j<MAXrot == 4) || (_CONTROL_BIT_ENABLE_NSF)
   (plci->nsf_control_bits & T30s used (ch==2) or perm.  _MWI_ACTIVATE:ion         }
    add_dynam
   1_    ion                 ernal] = ncpi    );
        retla2] |---------*/doplciatic WRON }
     APTER   *, PLCI   *, AL_BIT_MORE_DOCUMENTS)
30_INFO, station_id) + 243\x61\x70\x69\x32\x30");

         plci, fax_adjust_lci,ASSIGN,DSIG_ID);
               t;
  byte (*         {
           && (GET_WORD(&= _OUT_OF_PLCI;
                break;
              }
     se;
            }

            rplci->ci->command = 0;
            rplci->internal    /* 5 */
 = MWI_ACTIVATE_RREM_L1D;
            rprms[MAX_MSG_PA = Number;

            cai[0] = 13;
se;
           APD_FREE_SAff        head1     & < C_IND_MASK_D->fax_connect;
  bug(1,dprin&parms[1];
      if ((plci->B3_prot 0_INFO *)(plci->fax_connec }
      }
      el= offsetof(T30rranty of d i;->NullCR break  {_DATA_B3_R,    ",     NONSTANDARD))91\xe0\x02"            /*    Info = _WRONG_MESSAGE_FORMATits |= T30_CONTROL_ if(ftable[i]      fax_featbyte)(fax_proller),
     ,cai);
               if ((plci->requested_op          if ((p) msg_parms[j].*/
                fax_co[0]))); /*WORD(&fax_parms Id, word Numbe sending optIGURE;
;
           gth,"wbd coCT:
        retur      plci-TIVATE:ci->B3_prot         rplci->number      caseprot == 5)
    }&& 
      a->ncci_state[ncci] h = 0;

  parms{
      {
      switch(SSreq{
      plci =_command (Id{
    "));
      add_s(plci,CPN,&msgconnect_info_manufaci=0,p=0; format[i]; APPL *appl, API_PARS("Re    ,Sigemove_is(no plciECT_B3_(byte)nreturn false;
     e)ncci);>= 2))
     | a->request Id,
         
static void api__out =_PARSE *parms)
{
  w       "",       cai[2] = (b    }, /             x90",
      NECT_B3_T90_ACT_& 0x0004)
                  "\x03\x91\xT_B3_ACTIVE_I,Ilen;
     )
    && (plci->nsf_control_bits & T_SIZE(ftable); i++) {
   "\x02\x88\            if (plci--------------------------->fax_connect_info_buffere esc_t[] = {0PI_SUPPORTS_NO     case S{
        for(jindicatj<adapter[i].max_plci;j++) {
          if(adapter[i].plci[j].Sig.Id) plci_rAPPL_FLAG_PRI     nl_busy     elsode = 0;
   ->nc    e_len =->ns  {
    th;
    xed			   PLC       ++) {
    cai with S      plcurn false;
         ;
static BERS:
          case S_urn false;
         anufacturer_parms))
      
                {
          if(ncpi->info[1] &1) req = N_COWARDING_STOP:
              appl->CDEnable = true;
            cai[0] = 1;
            cai[1] = CALL_D----         rplueue))[j++] = ((byte   *) msg)[i];
        if (m->header.command == _DATA_B3_R)
        {

          m->info.data_b3_req.DatL_BIT_ACwitch ->B3_prot ted_oif(p } /* end WI_ACT
      return    acERSION_,GET_WORD;
} *);

digit_map     m->i{eq)
 ,, word Nu  {
-----d Autf)
  {    _HASHMARK }    d, word Number, DaVA_CAPI_ADAPTER *a,
			STARI *plci, APPL *appl, A30VA_CAPI_ADAPTER *a,
			0i;

  ncci = (word)(Id>1VA_CAPI_ADAPTER *a,
			1i;

  ncci = (word)(Id>2VA_CAPI_ADAPTER *a,
			2i;

  ncci = (word)(Id>IVA_CAPI_ADAPTER *a,
			3i;

  ncci = (word)(Id>4VA_CAPI_ADAPTER *a,
			4i;

  ncci = (word)(Id>5VA_CAPI_ADAPTER *a,
			5i;

  ncci = (word)(Id>6VA_CAPI_ADAPTER *a,
			6i;

  ncci = (word)(Id>7VA_CAPI_ADAPTER *a,
			7i;

  ncci = (word)(Id>8VA_CAPI_ADAPTER *a,
			8i;

  ncci = (word)(Id>9VA_CAPI_ADAPTER *a,
			9I *plci, APPL *appl, A4%x)",ncci));

  if (plciAstatic byte disconnect_ != IDLE) && (plci->StatBstatic byte disconnect_IVA_CAPI_ADAPTER *a,
			Cstatic byte disconnect_f(a->ncci_state[ncci]==IDI *plci, APPL *ap0d Nu6b3_req(dword Id, word Number, DIVA_CAPI_ADq"));
*a,
			      PLCI *plci, APPL *appl, API_Pq"));
parms)
{
  word Info;
  word ncci;
  API_Pq"));
 ncpi;

  dbug(1,dprintf("displci, AP4d NumG_CON8>16);
      ALelsee(no  *plci, APq"));_PENDIN%x)",ncc|| (a->UNIDe S_FIEff)
  ate[ncci] =G_CON_PENDIN != IDLE|| (a->DIALte[ncci] == INC_ACT_PENDING)IVA_CAPI|| (a->PABX_ + msg->h->ncci_state[ncci] = OUTG_DIS_PEf(a->ncc|| (a->SPEC>ncc->ncci_state[ncci] = OUTG_DIS_PEa->ncci_nel_xmitECON----ncci_state[ncci] = OUTG_DIS_PEState!=I|| (a->moveemove(no i] == INC_ACT_PENDING)NNECTED;nel_xmit_xon (pplci->B3_prot == B3_T30)
        ||i_ch[ncc|| (a->BUSYte[ncci] == INC_ACT_PENDING)}
  }
  || (a->CONGESLUDINe[ncci] == INC_ACT_PENDING)I_PARSE nel_xmit_xon (p   *ct_beturn false;
      }
      elsebommand = 0;
    MFORPTER *t == B3_T30)
        ||c      {
      HOLate[ncci] == INC_ACT_PENDING)d)
        || (p;

  rn false;
      }
      elseeommand = 0;
   ALLER_WAId);
s->B3_prot==3)
        {
   fncpi->length - 3) byte   *)&(ncpi->info[4]));
       9G)
     || (a->PA = (byte)ncci;
        plci-9NDING)
     || POSI----------------eturn 1;
    }
  }
  sendf()
    {
      aNEGAONNECT_B3_R|CONFIRM,
        Id,
        NuNDING;
      chWAR_reqFIRM,
        Id,
        Nu    channel_xmiINTRU, USFIRM,
        Id,
        Nuata_pending
   --------Cck(pSERV->cheturn 1;
    }
  }
  sendf(T)
        || (PAYPH
  {  {
GNIdata (plci, ncci);

        if(9 (plci->B3_protCPEi, a->d, w|| (a-,ncci));
  if(plci && n   plci->send_dparmHOOKord Id, word Number, DIVA_CAPI_ADAPb  }
        }
  + ms     ->B3_prot==3)
        {
  cG)
     || (a->(word)ci_remove(no NL) && (plci->B3_prot NDING)
     || ----------------!= B3_X25_DCE))
      &)
    {
      aVA_CAPI_ADA!= B3_X25_DCE))
      &NDING;
      ch3_req.DD_SEL)))
    {
      plci->call_dir     channel_xmiANSAMB3_T90NL) && (plci->B3_prot ata_pending
   CE_OUTG_NL;
i->inc_dis_ncci_table[i]!=(bytT)
        || (BELL103_SEL)))
    {
      plci->call_dir  (plci->B3_prot*/
     S!= B3_X25_DCE))
      &   plci->send_dG2 i++)GROUP   pte[ncci] == INC_CON_Pccommand = 0;
  HUMANit_xECH!= B3_X25_DCE))
      &
      {
      VA_CAP);
stACH    39connect_b3_a2d Numf((plf%x)",nccncci));

  if (plci && ncci && f((plci->Sta != IDLEDLE) && (plci->State != INC_DISf((plci->StaIVA_CAPIplci->State != OUTG_DIS_PENDINGf((plci->Staf(a->nccncci_state[ncci]==INC_ACT_PENDIf((plci->Staa->ncci_ci_state[ncci] = CONNECTED;
   f((plci->StaState!=I!=INC_CON_CONNECTED_ALERT) plcif((plci->StaNNECTED;ED;
      channel_request_xon (f((plci->Stai_ch[nccncci]);
      channel_xmit_xon f((plci->Sta}
  }
  
  return false;
}

static bytef((plci->StaI_PARSE 
  dbug(1,dprintf("connect_b3_af((plci->Stalci->B3_API_ADAPTER *a,
			KSPENDING) && !plci->chan       nt == 4) || (plci->B3te == SUSPENDING){
     gth>3)
 t == 4) || (plci->B3PRMATS)
       && ((plci(ncpi->lSE *parms)
{
  word SPENDING) && !plci->chan  }
    SE *parms)
{
  word nncci]    #def= 0;_CAPI_ADAPTMAP_ENTRIES(PLCI   * plonnect_b3_a_re)WORD);
        */
/*_mask_* lo "",rnternal_commaneturn furn 1;
if(pplci->B3_prminct_b3_ad *appl,,ata_bgap(dword Idms))
      += (((CAPI ("[%06lx] %s,%d:   return 1;
      }
 oid dtarse (      s_parms[Id= fals | UnMapncci_ptnnec | N_D_BIT;
        
  NCCI      = AdFILE_), _      _,alse;
}

stat  else
   lse;
}

statate && p(byte ta_b3_req(dword Idvsprot=0;
*/
/* locpuls"S",     )
       bug(1,dprintf("ncci=0x% }
   word Number, DIV
  dbug(1,dprintf("naui=0x%x, plci=0x%x",ncci,plci));

    if ((      /* 5 */
       ND;
PTER  * a);
static void-------- mixer_.
 *
  fer[len         /* 13 */i_state[ncci] == INC{
  ta_b3_req(dword IdDATA_RE
      ncci_ptr = &(a->ncci[ncci]);
     word Number, DIVu_t[(Reject&0         }
        5->stati
      ncci_ptr = &(a->ncci[ncci]);
 "\x0 + msg->header.length +     i -= MAX_DATA_B3;
      daq;
  byte tic void api_l_mask_temove(PLCI   *);
static vota_b3_req(dword Id, word Number, DIV)->sta if(api_parse(&pa_ptr = &(a->ncci[ncci]);
 _ACT_PENDING))
    {
   EAD + 3) &ueue data */= MAX_DATA_B3;
      dataap_pos, tic void api_l_NONSTANemove(PLCI   *);
static in_queue))= MAX_DATA_B3;
    {
      i = plccci] == INi->commandACTIVATE:
            ORD(&(ss_parms[4]ci->commandeak;
            }

          (ncci == /* Function */
           /* 13 */RD(&cested_optio       retg(1,dprgVA_C   }
  }
  return fa  *t_b3_a  callba_mappi_b3_a _DATplci->B3_prw,LCI   *I_ADAPTER *a,
			PLCI *plci, APPL *applck for deliv  disconnI   *ncci_ptr;
  DATA_B3_DESC   *data;
  word Info;
  word ncci;
  word i;

  dbug(1,dprintf("dat& 0x0004)
      add_b1 (i_state[ncci] == INC_ACT_PENDING))
    {
   S    d Auto)); /
  dbug(1,dprinted_oncci=0x%x, plci=0x%x",ncci,plci))      }

          }
       _ptr = &(a->ncci[ncci]);
     ic bytack_pending)++;
        if ((a->ncci_state[ncci] == CO*)(parms[0].i
    }
  }
  if (appl)
  {
    if (plci)
0; k <   *a;
  d *, byte  & 0x0004)
 ss_parms[10].inack_  }
   pport(D(w <start_internal_command [7].infoALL_RE if (data- Pro!=connect_b3_a_resw].rn false;MENTS)
        w false;
      fer[i]);
      data->Number) && (p    {

        TransmitBuff(dword Id((dword *)(parms[0]
   :PARSE *parms)
{
  word ncc   cai[1] = 0xDATA_B3;
      data  +g_in_queue)) d *)parms[0].info);
      data->Length = GET_WORD(parms[1].info);
      data->Handle = GET_WORD(parms[2].info);
      data->Flags = GET_WORD(parms[3].info);
      (ncci_ptr->data_pending)++;

        /* che loc);
        reternal_command (I= ncci_ptr->data_ack_out + ncci_ptr->data_  n = GET_WORD(p{
  NCCI   *ncci_ptr;
  DATA_B3_DESC   *data;
  word Info;
  word ncci;
  word i;

  dbug(1,dprintf("d    ncci_ptr-*/
/* local funcD(parms[2].indprintf("ncci=0x%x, {
      dbug(1,dprint  if ((a-       

static vo);
 E   *in, API_PARSE   *out);

woon */
   gs |=)->sg)++;

        /* chesend_clear_config (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_/*
 *
  Copyright",
    (dword)(( Net->Id << 8) | UnMapController ks rangadapternge ))  Eiconcharicon)(FILE_), __LINE__));ks, s rangplied for requests = 0;ee software; you capulse_mtribute it and/or modify
 aut under the}


static voidppliedprepare_switchon Netw Id, c) Eicon Networks, 2002.
 *
  This source file is supplied by
  the Free
  EicoerverId (Id),:    2.1
 *
  This program is free while
 *
  Eire; you can redistr!= 0) Eicopliedyrighrmation is , soft)ic License asftwarplied avepyright (oftware Foundation; ei, byte Rcworks, 2002.
 *
  This source file is supplied of MERCHANT%02x %d  This program is distributed in the hope that i, RcCLUDINEicojust_b_ensees free return (GOODG ANY
  implied warrantyrestor MERCHANTABILITY or FITNESS FOR A PARTICULAR  ed warInfofree 2002.
 *
  This source file is supplied the Free Softw details.
 *
  You should have received a copy of the GNU General Public License
  alongidge = is pte iif
 *
  EiB1_facilities & B1_FACILITY_DTMFRY OFve,   e Free Seral Public License
 dprin dprincase ADJUST_B_RESTOREC"
#d_1:dprine softwainternal_command =neral Public Licis optite irs tivasync._nl_busy





)----------------------*/ublic License
 =------------------------pters th  breakpters th}ters thpliedenable_receivs.
 *
  eneral Pplatforc_active)pters tho it is not necessary to ask it from every ad2pters thand it is n---------------------------2--------ivas(RcRRANOK) && ---------_FC
/* XDI driver. All2002.
 *
  This source file is supRe save  "
#d t separatfailede "di
  Eico00001
 You should have received a copy of the GNU Gs fr000001
#.h"
#in_WRONG_STATEpter*/
/* and it is not necessaand it is n nec nec with thidgeG ANY
  implie publishedis optioare
  Foundation, Inc., 675 Mass Ave, Cambr
/* This is opti,ridge,   PARTImasit is nPARTIresult[4], MA 02139, USA.
 *
 */





#include "platfis optio detai04 (((__a_d>manufacts.
 *
  You should have received a copy of the GNU General P only if:
  proto002
#very adaptecmNCLUDIN adapter     it undFACTURER_FEATURE_Oeneral urer_features & Mdify
  it undFACTURER_FEAT GNU General mdm_msg.h"
#include "d suppor0] =      PUT_WORD (& suppor1],PI_US_SUCCESS00002
/* This is options suppo
/* This is optite it and/
/* This is optionsute irts ----x0aptertf









/*s & MANUfine d
------------LISTEN_TONEXDI_RT-----------<<= apter----------*/

sta----id group_optimization((DIVA_CAPI_ADAPTER  a, PLCI   *e Free S
/* This is opti------------default--------ublic Li1formourceR INCLUDIN, NULL, (Networync.h"



#define FI|002
#defi_ "MESSAGE.C"
#def-------COMMAND_100002
#----------w(DIVA_CA------------ublic Licprocessb);
static bRc)RRANis pratic dword diva_xdi_extended_features = 0;

#defineLoadPI_USE       0x00000001
#define DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR  000002
#define DIVA_MESSAGE.CNOT_SUPPORTEde "d*/
/* and it is not necessaivasync.h"i);
static void clear_ api with I_ADAPTER   *);
word Cap-------------re server by    */
/* XDI driver. Allo it i
/* This is options);
void api_re0002
#defiremove_startot necessaI   *);
static void diva_get_extended_30002
#dery to save it separate for e(PARToup_ind_dapter    */
/* |optim000002
#deremove_start(void);
void api_r3-----------------------------------*/
static dword diva_xdi_extended_features = 0;

#defineEVA_CAPI_USE_CMA                 0x00000001
#define DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR  0x00000002
#define DIVA_ API_SAVE   *out);
static void api_load_msg(APII_ADAPTER  * tone_last_indicSOEVE_codry t------IGNAL_NOatic atic --------*/;

void   callbac=orts this &#define DIVA_CAPI#define D--------------*/

static voiOProup_optimization(DIVA_CAPI_ADAPTER   * a,**parms);

static voic void set_group_ind_**parms)I   *plci);
static void clear_group_ind_mask_bit (PEQ  *);

void   callba&= ~rms);

statiivasync.h"dapter    */
/* ic void api_load/*_ADAPTER   *);
word CapiRelease(wordTER   *, PLCI   *, APPL   *, river. Allat are server by    */
/* XDI d PLCI   *, APTER  * a);
static void diva_ask_for_xdiapter*/
/* eatures (DIVA_CAot necessabyte connect_res(dword, word, DIVA_CAPI_PTER  * a);
static void diva_ask_for_xdiadapter_feary to save it separate for efals* Macrose eatures (DIVA_CAPI_ADAPTRc ----_start(void);
void api_remove_complet-----------------------*/
static dword diva_xdi_extended_features = 0;

#defineDisA_CAPI_USE_CMA                 0x00000001
#define DIVA_CAPI_XDIfarnfo(PLCI   *, dword, byte   * *, byte);
static void SendSetupInfo(APPL   *, PLCI   *, dword*/_bit (PLCI   *plci, word b);
static byte test_group_ind_mask_bit (PLCI&ic void a~(_ "MESSAGE.C"
#dX | word b);
void AutoomaticLaw(DIVA_C3PI_ADAPTER   *);
word Cap *, byte, byt);
word CapiRegister(word);
word api_put(APPL   *, CAPI_MSG   *);
static word api_parse(bytUnl   *, word, byte *, API_PARSE *);
static void api_save_msg(API_PARSE   *in, byte *format, API_SAVE   *out);
static void api_load_msg(API_SAVE   *in, API_PARSE   *out);

word api_remove_starttic void VSwitchReqInd(PLCI   *plci, dwoSENDatic roup_optimization(DIVA_CAPI_ADE *);MFic byte connect_req(dword, wordDIGITSord, k (PLCI   *plci);
static void clear_group_ind_mask_bit (PLCI   *plci, word b);
static byte test_group_ind_mask_bit (PLCI   *plci, rks rang laterarameter_lengthRRANTY ? word b);
void Au  *, API_PARSE *);
st :APTER   *, PLCI   vision :res(ticLaw(DIVA_CAPI_ADAPTER   *);
word CapiRelease(word);
word CapiRegister(word);
word api_put(APPL   *, CAPI_MSG   *);
static word api_parse(byte   *, word, byte *, API_PARSE *);
static void api_save_msg(API_PARSE   *in, byte *format, API_SAVE   *out);
static void api_load_msg(API_SAVE   *in, API_PARSE   *out);

word api_remove_start(void);
void api_remove_complete(void);

static void plci_remove(PLCI   *);
static void diva_get_extended_adapter_features (DIVA_CAPI_ADAPTER  * pliedmsg_number_queue[word, DIVA_Cou can redist)++----CTURERord, wMacrose definea);
static void diva_ask_for_xdi_sdram_bar (DI/*
 *digitste for e&CTURERnse deq(d.parms[3].info-----   *, APPL   *, API_PARSE, DIVATY   *);

static void control_rc(PLCI   *, byte, byte, byte, byte, byte);
static void data_rc(PLCI   *, byte);
static void data_ackSen *, worAPTER              0x00000001
#define DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR  0x00000002
#defiADAPTER   *, PLITHOUT ANY WARRANTY OF A byte dA_CAPI_ADAPTER   *, PLCI --*in, byte *format, API_SAVE   *out);
static void api_load_msg(API_SAVE remove_starttchReqInd(PLCd, wor/*
 d,word, Dappl,atic void SR | CONFIRM, Id & 0xffffLtatic woord, wurer_f"wws"ocol c, SELECTORC"
#d,-------G ANY
  impliePARTIplatforrediscan request ftwarNI  * p DIVA_CAPI_ADAPTERiconar FITNESS FOR A APPLte);
  *, *, bPARSE *msg Ave, Cambridge, es self , jode supports this &;
static vreq(dworms[5]this && xdi support0s this
 */
#define DIVA_CAPI_SUPPORTS_NO_CANCbyte);
  This program is distributed in the hope that it will ------------------------------------------------------------*/
/* local f (!(a->profile.Global_OpOEVEFILEGL------out);
sta
/* Xgroup_i002.
 *
  This source file is supF#definy not supportyte *, API_program is distributed in the hope that it wi byte);
static void SendSetupInfo(APPL necelseER   *pidworse (&msg[1SE *);

stal_remov, DIVA, "w",  *);
stati find_cip(DIVA_CAPI_ADAPTER   *, byte   *, byWrong message foATSOPN_filter_ok(byte   *cpn,DIVA_CAPI_ADAPTER   *,word);

/*
  XON prCAPI_XMESSAGE_FORMATtic wovoid channel(GE-------- *);
static0SE *);) =ExtInd(l_xmout);
sta_DETECT_CODESPL   *|| el_xmit_xon (PLCI   * plci);
static int channel_can_xE *);I   * find_cip(Datic bbytebyte);
ed_o   *, _tave [I   nge -1]PL   *, AP& (1Lof DPRIVAT-------tic )
/* XDIriver. A PLCI   *, APPL   *, API_PARSE *);
_USEunknown byte);
s((__ 0x00000001 You should have received a copy of the GNUl_xmit_xon (PLCI   * plci);
s000002
#de---------------------------UNKNOWN_REQUEST);

/*
 necesd chsk, byte setufor (i----- i < 32; i++
word api_reupport + i----ute i byte Sel_xmit_xon (PLCI   * plci);
static int channel_can_xon (PLCI   * plci, PLCI   *, Ac void VoiceChanPLCI   *, _MAP_ENTRIESlOff(PLCI   *ple disconnectaticpliedAPTER_map[i].listen_-----oid add_d(PLCI  ---------atic1_resource);
static   2acter >> 3)]* pa(1of Dyte b1_resource, word b1_faciadd_7000002
#defi necessa necessaPTER   * ear_config (PLCI   *plci);

static word get_b1_facilities (PLCI   * plci, byte b1_resource);
static/*
 *dd_b1_facilities (PLCI   * plci, byte b1_resource, word b1_facilities);
static void adjust_b1_facilities (PLCI   *plci, byte new_b1_resource,------------3 +nneld, PLCI   *plc3, byt);
statCAPI_Xvoid channel Nettatiyte _off (PLCI   * plci, byte ch, byte flag);
static vc) Eon (PLCI   * plci, byte ch);
static void channel_request_xon (PLCI   * pIDENTIFIERatic void chtatic byte SenCTURERSnse
dv_voi|| ata_ackNL.Iddwor23 (PLCl_remove_i clear_group_CI   * plci, byte ch, byte flag);
static vense
 PLCI   *, APPL   *, byte);
static void CodecIdCheck000002
#dene DIVA_CAPI_XDI_PROVIDESPI_ADAPTER   * );
statiCTURER*/
/*----------er_features & MANU#inc_xmit_xon (PLCI   * plci);
s;
static---------------printf









/*------------ew_b1_eve_res----------*/

static void group_o  *plci, dword Id, byte   **parms)up_optimization(DIte Rc);
static void i * a, PLCI   *id nl_ind(PLCI   *);

static bytPLCI   *plci);
static yte SendMCTURERiIE(PLCI   * plci,conn plci, byiIE(PLCI   * plci (dword Icon File iIE(PLCI   * plci, dword Id, byte   * * parmsms, byte ie_type, dword info_mask, b byte disconnectpParse);
static word AdvCodecSupport(DIVA_CAPI_ADAPTER   *, PLCI   *,  *, APPL   *, byte);
static void CodecIdCheck(DIVA_CAPI_ADAPTER   *, PLCI   *);
staticatic void SetVoiceChannel(PLCI   *, byte   *, DIVA_CA*/
/* and it is notword, byte void set_group_ind_mask (PLC(dword, word, DIVA_CAPI_ADAPTlci, byte a->manufacturer_fealci)FILEMANUFACTURER_FEAnoti_HARDrt(DI_PARSE *ms&& g (PLCI   *plci);
static void mixer_notify_update SOFT-----RECEIVEation (dword Id, PLCI   *plci);
static void dtmf_indicatite   *);
static word CPN_filterI_PARSE *);
static void api_save_msg(API_PARSE   *in, byte yte);
static void SendSetupInfo(APPL   *, tatic void mixer_se  *plci, byt-----&------*/

staACTIVE_FLAGes (PLCI   * plci, byte b1_flow_control_remove (PLCI   * plci);
static PI_PARd channel_x_off (PLCI   * plci, bytbyte connect_res(  it under the t*msg, word length);
staticeneral Publicmove (PLCIdication_xe, word new_b byte   *msg, word length);
static void mixetic void hold_save_comove (Per_indicationI   *plci);


static void ectic void hold_save_co2DAPTER   *a, PLCI  dication_xdication_xstarstat* This is optio);
static b_CANCEL(__a_d, word, DIVA_CAP ((dword, ore_command (dwordE *);
static byPLCI   *plci);
static void clearrd, word, DIV(dword Id, PLCI   *plci, byte Rc);
static byte dtmf_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void dtmf_confirmation (dword Id, PLCI   *plci);
static void dtmf_indication (dword Id, PLCI   *plci, byte   *msg, word length);
static void dtmf_parameter_write (PLCI   *plci);


static void mixer_set_bchannel_id_esc (PLCI   *plci, byte bchannel_id);
static void mixer_set_bchannel_id (PL  *, APPL   *, APord length);
static void mixer_indication_xconnect_to (dword Id, PLCI   *plci, bd Id, PLCI   *plci);
static void dtmf_indicatiatic void channel_x_on (PLCI , APPL   *appl, API_PARSE *msg);
static void mixer_indication_coefs_s   * plci, byte ch);
sta);
static void mixer_indication_xconnect_from (dword Id, PLCI   *plci, byte   *msg, word t and/or modify
  it under rd Number, DIVA_CAPI_ADAPTER   *a, PLCI  _PROVIDES_NO_CANCEL))

/_PARSE *msg);
static void ec_indication (ddication_xcoid adv_voic  j--------------be usef(Chan *);
static_CAPI_ADAP-----j;

static word get_b1_facation (dword Id, PLCI  ---------*/
/* * Global d                           remove (PLCI ----n (PLCI   * RSE *);
i+1]1_fa_resource);
stajword b1_fac
extern byte m, byt1_resource);
stajst_b1_resour&k(ENTItati0mation (dworyte   *msg, wordj++command (dword Id, PLCI iic PLCI dummdication_xconnjtatic int---------------*/
extern byd Id, PLCI   *plci);
static void dtmf_indicatiIncorrectPLCI   * pl      0x00000001
#, APPL   *, byte);
static void CodecIdCheck(Dapter;
extern byte m] void mixer_set_bchannel_id_esc (PLCI   *INCORR(PLC  *, er_indicationc void mixer_indication_xconn word p_length);
static v>= ARRAY_SIZE            q(dword, word, Dation (dword Id, PLCI   *plci);
static void dtmf_indication (dbyte);
soverrunRSE *);
} ftable[] = {
  {_DATA_B3_R,                   fferSet(APPL   * appl, dwordDI_PROVIDES_NO_   data_b3_res},
  {_INFO_R_flonse fmsNTAB*);
statit_to (dworI   *, APPL   *,00002
#defi byte   *msg, word length);


static void rtp_connect_b3_req_command (dworPLCI   nd_mask_bit (P   *plci);
static void dtmf_indication (dword Id, PLCI   *plci, byte   *ms You should have received a copy of the GNU---------------00002
#defit_bchannel_id_esc (PLCI   *plci, byte bchannel_id);dicatiCAPI_XDI add_b2   *, API_PARSE *);
static word add_modem_bq_ncci(lci, API_PARSE* bp_parms);
static void sig3_req_command (dwoA     0x00000008

/*
   WHATSOEVER oftware Foundation; either_data(PLCI   *);
statthis && xdi supports this
 */
#define DIVA_CAPI_SUPPORTS_NO_CANCEL WHATSOEVEbyte   **, byte   *, byte *);
static byte getChannel(API_PARSE *);
static void IndParse(PLCI   *, word *, byte   **, byte);
static word p_length);
static void add_ dprintadd_b23(PLCI   *, API_PARSE *);
static word add_modem_b23 (PLer_req(dword, word, DI0]a_b3_resAPI_PARis pNSE,                "ws",      *, word, byte   *);
static void ac void VoiceChan word p_length);
static lOff(PLCI   *facturer_req(dword, word, DI void|RESPONSE,   "ws",           +1];ransmitic v License as publishedatic void can request to process all retu void d nl_rePI_ADAPcodes self , j, n, MA 02139, USA.
 *
 */





#include "platfatic void   This program is distributed in the hope that it will noid adv_c void Vo1ceChan, DIVAlOff(PLCI dprin---------*/--------------------------------*/
extern ax_adl_rei_appl;
extern DIVA_CAPI_odPPL   *, Atern APPL   * applicati byte add_b1&every adapter    */
/*  byte remove_d fax_cotic PLCI dicatiord co----------------------*/
extertore_commhannec byte * cip_bc[29][2] = {
  { "word infoword Id, PLCI   *plci, byte ----  * *, byte);
static void SendSSEExtInd(APPL   *, PLCI  }, /* 2 */
;
extern DIVA_CAPI_ADAPTER 1_fatInd(APPL   UN*plci, by);
sta
/* XDI driver. Allaticn + 102\xi byte (* function)(dwoc void Vo       "d >   "\x; i--
extern byte m   "", =0"     - 1void li "\x04, DIVAic PLCI dummy_ic struct _ftable {
  wl_re++n    0\x90\xa2"     }, /* 4 */
  {(DIVA_CAPI_ADAPTER  *  byte);
static void SendSSEx;
extern DIVA_CAPI_ADAPTER q},
  {_D /* 7 */
  a5",     "\x03\x91\x90\xa5"  IVA_CAPI_XDI,     NNECT_B3_I|RESl_re-----_SYNC_,   |RESPONSE,           "",         Iword add_modem_b0tic S"bp_parms);
staticconnect_VE_I|R/*-   }, /* 12 */
  { "",                     ""                    */
/*ort(DIword, wors* adapter;
ex       ""                     }, /* 1}, /*   }, /* 12 */
  { "",                     ""                     }, cense as published ord, wordwrite (c) Eicon Networkreq},
  {_CONNECT_B    "\x03\buffer[-----PARAMETER_BUFFER      + 2s this
 */
#define DIVA_CAPI_SUPPORTS_NO_CANC    "\x03\x80\x
  Eicon Networks range of DIVA Server Adapters.
 *
  Eicon File Revision :    2.1
 *
  This program is free sx90\xa3",     "\-----CTURER_FEATword, word, DIVA_    \x90"         }, /* 1/
  {SP_CTRL_SET------90\x90\xaSRER_I|RESPONS  reset_b3_res},
 word, word, DIVAlOff(PLCI  0"         }, /* 2+ connect_b3_t90_0"         }, /* ivoid add_pte for eFTY,90",         "\x0",   sige   te for eTEL */
 , 0      u can r    */
ic License as published ord, word
  Copyright (c) Eicon Networks, 2002.
 *
  This source file is supplied 25 */
  { "\x03\x91\x
  Eicon Networks range of DIVA Server Adapters.
 *
  Eicon File Revision :    2.1
 *
  This program is free software; yword, word, DIVA_ublic License as published ord, word by
  the Free Software Foundation; either version 2, or (at your option)
  any later/
  "",                  This program is distributed in the hope that it wilNY
  implied warranty           of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public Lice   /* 5 */
  "",       details.
 *
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to           the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */





#include "platf 11 */
  "",              "di_defs.h"
#include "pc.h"
#include "capi20.h"
#include "divacapi.h"
#include "mdm_msg.h"
#include "divassync.h"



#define FILE_ "MESSAGE.C"
#define2 */
  { "\req(dword, word, DIVA_CAPI_fine dprintf









/*-----------------------------------------------------90\x90\xa2-------------*/
/* This is options supported for all adapters that are s-> }, /* t(APPL   *, CAPI_Mo it is not necessary to ask it from every ad  /* 21 */
pter*/
/* and it is not necessary to    "\x03\x80\x90"\x02\x1\xb8",               /* 24 */
  "\x02\x91\xc1",                         */
/*------------------------------"          -----------------------------------*/
static dword diva_xdi_extended_features = 0;

#define Dhe Fr 13 */
  { "",               0x00000001
#define DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR  0x00000002
#define DIVA_CAPI_XDI_PROVIDES_NO_CANCEL  0x00000004
#define DIVA_CAPI_XDI_PROVIDES_RX_DMA        }, /* 12 */
  { "",                     ""                     }, /* Linel funcuest*, A
#define FI     ""                     }, /* 14 */
  { "",                     ""                     }, /* 15 */

  { "\x
LI_statiG 14 lipyright, dwor;
,     i_totis ihannelsword     }, /* 12 */
  { "",                     ""                     }, /* translsarya CHI  *);ATSOEVERelement to a 
};


/ ord, w  }, /* 14 */
 ARSE *,sdd_mo - any                ""                     }, /* 14 */
 ---------0xfe - chi wtic vcoding----------------------------*/

word api_put(APPL  d - D-----------------------------------------*/

woword api_put(APPL 00 - no-----------------------------------------*/

woord api_put(APPd cha               /e_ty: timeslotPI_MSG   *m;
    API_PARSE mif_MSG_PARs is provided we accept m
#dethan onX_MSG_PAR. }, /* 14 */
  { "",                     ""                     }, /* 15 */

  { "\x03\x80\PARTIch F  

};


/      I_PAchtatiftwar*p
};


/map      int pocal fu  {_COr starmadaptePARTIexclode suppoofsode suppochq},
 ivasy with 0 up rts with 0 upoid adv_if(!chi[0])I_PROVIDd_mo\x91 >=       urn _BD_MS1]LCI  20)tic byte r];
 0]==12 */];
  p==0xac
  }
  
  a d; /*dapteusllbadyte contrAPPL   for(i=    <D_MSG;e otheD_MSi] &0x80)lOff(P\x91\xif(i==led)
  word  dbug(1,dprin
  }
  
  a eci=%x",msr];
  pl|0xc8)!=0xe9 &a->plci[msg->header.];
  plc0x08)dapter[cox4ontrol&& (plheadnt. id pres    APPL   er.ncci);
   4NULL;
  i  p=i+  /* 25 *apter_pisabled)
  {
    dbug(1,dprintf("plci=%x"x",msg->header.plci));
    plci = &a->plci[msg->headrd, byte lci->MSG   *standardeq},
  {/Map, C)-1);

Typeplci->St     || (pl->State == INC_CON_ALERT)
      || (msg->header.,msg->header.plci));
    plci = &a->plci[msg->header.plci-p]|0xd0ncci d3 &a->plci[msg->  && (plci->mmand == (plci->State == p;
   1NULL;  && (plci->
   APPL   *,er.plci-0]-p)==4)ter oid adv_voicd channHEAD <= MSG_I3_QUEUE_S  /* 25 *d chaa->plci[msg->head  c        Id, PLC   return     || ((iceC<42 */pbled)
 lOff(P02\x91\xb8",ic PLCI dummch += 8command (d  elzatiength + MSG      if (plci->msg_i 5 */
 ch    i));
 f (jstatic ch)); ch->header.co MSG_IN_|=.plcip*/
  { "\x0new_b1_resource,ader.ler )
   MSG_IN_OVER  if (i <PI_ADAPTERg->header.lengt       APPL   *,     || (plci;
      if (j +3= &adr.commaintf("invalid1\x90\xa5", (_SYNC_RAD <= MSG>30 &a->plci[msg->head      else
      {
   || ((ncci >header   *)(plci->msg_i2"      dbug(1,dp7f) > 31
          msg->header.EUE_SIZE byte ied_pos, plci->mplci, byte new_b1_resource, worrite=%d read=%p!>header
          msg->header.aticchsg_in_wrap_pos, i));

        elsebyte iecAPTER   *)PI_SAVE       if (j +N_PE))
     && ((ncci =FO_R,    ntf("invalid ctrl"));
    r( controid channel  el!=   }Networ   *)(plc= &a->plci[msg->head with thSYNC_Rapter|(plci->m void cha{D + 3)ote_ty& !a->adapter_disabled)
  {
    dbug(1,dprintf("plci=%x",msg ((byte .plci));
    plci = &a->plci[msg->header.ncci);
    if (plci->Id
     && e Freeplci-1];
  98write=%d-----  }
:>msg_in_qI_ADAPTER  0x99_res(dword,wontf("invalid ctrl"));
    r);
static vith ter.com  /* 25]);
    a   n = ncci_ptr->data_pending;
        l4= ncci_ptr->data_ack_);
stat]);
    b= &(a->ncca = &adpos)
     c  {
        er.plcx_plfffc))

ISCONNEC>msg_in_queue)))
     VE_I|RESPONSE,   "mix */
et_b
};


/_id_esc (c) Eicon NetA PARTIsg_in_queue      LCI   *, byte, byte);
-----) Eicons Netmax_adapteld_idos;
 aons supportn Fil  }
sg_in_ */
  { "limsg_in_queuee "divasa     pr91\x9ic byte Se(== ncciCAPI_A----es, res, C2 , C[;
    b----+ I_MSG   - 1)].   *plci  */
/* XDI d   *)(plci->msg_in_queue))[k]))->info.data_b3_reqi, by  }
  
          {
            yte   *)(pladd_1f)  "\x02\ue)))
   *)(plci->msg_in_queue))[k])   *)(&((byte   *)(pl.data_b3_req.F, byte R0004)
              l++;
         AD + 3) & 0xfffc;

        }
    G   *e Rc);
static void fax_ed(->msg_in_queue))[03  "" 1)
statig=%d/%d ack_pending=%d/2               (CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[k]))->info.data_b3_req.Flags & 0x000404)
              l++;
          }

          k += (((CAPI_MSGSG   *)(&((byte   *)(plcig=%d/%d ack_pendinVA_CAPI_ADAP+;
 AdvSignal)(&((!    if (bytein_queue)))
         Netw (((byte   *) msg) < ->tel02\xADV_VOIC{ "\x03\x91\x90\xa5"e   * = in_queue)))
    g_in_queue)))
   *)(plci->msg_in_queue))[k])2 -G   *)(&((byte   *)(p   }
        if ((n >=    * plci, byte b1(e   *)(&((byte   *)(pl_facilities (PLCIbyte   *)(plci->msg_in_queue))[k])     }
      else
    .data_b3_req.Fe   *remove_started = false;
sta>req_in || plci->internal_command)
          c = true;
        e        {
   (dword Id, PLCI      }
      else
    byte return _QUEUE_FULL;
 /
  { "\x04\xFULL3(requeue)"));

            return _QUEUE_FULL;
              ifcommand (dwoIVA_CAPI_ADAPTER   *, byte   *, byadv_voiceci->msg_in_queue))[k]rer_featud(PLCI    Network     }
e of DIVA Server Adapters.
      }
con File Revision :d(PLCI   e received a copy of the GNU     }
      else
   
      c = false;
      if (length +
            MSG_IN_OVERHEAD + 3) & 0xfffc;

        }
        if ((n >= = MAX_DATA_B3) || (l >= MAX_DATA_ACK))
        {
          dbug(0,dprintf(              I_MSG   byte 2 */
  { "\      else
      {
     byte   *)(plci->msg_in_queue))[k])nd)
          c = true;
        els
         */
  {)(plc
  Copyright ("\x03\x91    IVA_CAPI_ADAPTER   *, byte   *, by*)(plci->msg_in_queue))[k]cturer_featun Networks range of DIVA Server Adapters.
 *
  Eicon File Revision :    2.1
 *
  This program i,) msg) >= ((_req},
 msg_in_wrap_pos =    *)(&((byte   *)(plci->msg_in_queue]))->header.command =ontrolR)
           && (((CAPI_MSG   *)(&((byte   *)(plci->mch,msg_in_queue))[k]))->header.ncci == ncci))
          {
            dbug(0,&0x7f)-1);

 rolle, byt;
static bchue))[i =     if (((CAP;
           tr->data_pending, n, ncci_ptr->data_ack_pending, l));

          return _QUEUE_FULL;
        }
        if (plci->req_in || plci->internal_command)
        {
          if ((((byte   *)able[i].k]))->header.;
        m = (CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[j]);
        for (i = 0; i < msg->header.length; i++)
          ((byte   *)(plci->msg_in_queue))[j++DAPTER   * );
statipendin(word)(msg-%d/%d",
   _MSG"));
    incci_ptr PLCI   *, APPL ng, n, ncci_ptr->data_ack_pending, l));

          return _QUEUE_FULL;
        }
          if (plci->req_in || plci->internal_command)
        {
   SG   *)(&((byte   *)(plci(word)(msg_in_queue)))
_in_queue)))
           && (((byte   *) msg) < ((byte   *)(plci->msg_in_queue)) + sizeof(plci->msg_in_qu function)(dwo       {
            dbug(0,dprintntf("Q-FULL3(requeue)"));

            return _QUEUE_FULL;
          }
          c = yte   *msg, word  }
      }
      else
      {
        if (p(plci->req_in || plci->internal_command)
          c = true;
        else
        {
      yte   *msg, wordmmand = msg->header.command;
          plci->number = msg->header.number;
        }word Id, PLCI      if (c)
      {
        dbug(1,dprintf("enqueue msg(0x%04x4x,0x%x,0x%x) - len=%d write=%d read=%d wrap=%d free=%d",
          msg->header.comommand, plci->req_in, plci->internal_command,
          msg->er.length, plci->m>msg_in_write_pos,
          plci->msg_in_read_pos, plci->msg_in_wrap_pos, i));
;
        if (j == 0)
          plci->msg_in_wrap_pos = plci->msword Id, PLCI   *plci,        m = (CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[j]);
        for (i = = 0; i < msg->header.length; i++)
          ((byte   *)(plci->msg_in_queue)){_FACILITY_R,   ((byte   *) msg)[i];
        if (m->header.command == _DATA_B3_R)
        {

          m->info.data_b3_req.Data = (dword)(long)(TransmitBufferSet (appl, m->info.data_b3_req.Data));

        }

        j = (j + 3) & 0xfffc;

                *   *)(&((byte   *)(plci->msg_in_queue))[j])) = appl;
        plci->msg_in_write_pos = j + MSG_ISG_P       return 0;
      }
   #def    MIXER_MAX_DUMP_CHANNELS 34 *)(&((byte   *)(plccalcuctio_coefs (LCI   *, byte, byte);
    ense as   2.hexource); dword + m    {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'}  *);
stan,atic word   2.*s != pl 's':
   line[2 *  break;
    case 'w':
  +  break;
    case 'w':
  / 8 + ts this
 */
#define DIVA_CAPI_SUPPORTS_NO break;
    case 'd':
  Eicon Networerver Adapters.
 ae Revis:    2.1
 *
  This program is free c void VoiceChan, F   */

};


/*-"dws",        ,0x%x,0x%x) - le word 1);

&=   Ex 'w':
_ADDRESSEAPPL * Tranf("Q-FULL3(requeue)" wordflag void add_d(PL---------------------------|-------------INVOLVtatic voPTER   * );
static voi------ \x03parms[i++].info);
}jf(PLCI   *eturn ret;
  }
 CAPI remove functio    = in[i]j]rd ap add_d(PLCI tern ,0x%x,0x%x) - lej
  if(!removeistarted)q(plci);
  if(c==2 &&                           */
/*------------------------ngth = msg[p+1] +  word i;
  word j;

  if(!remove_s &---- *plExt, ERENCEtarted) {
    remove_started = true;
    for(i=0;i<max_remove(&adapter[i].plci[j])r;i++) {
      if(adapter[i].request) {
        for(j=0;j<adaptapter[i].p1;
        i -= j;
     CAPI_XDI
  } while (in->parms[i++].info);
}


/*---------------------------------*/

word api_removeNDING)
 CAPI remove functionoef.Id) plci_r= ~(  ExtEF_CH_CH |------------PC-----------PC--------------PC_PCheader.comm* CAPI remove functio.Sig.Id) plci_remove(&adapter[i].plfor (i = 0; i < msg->head-----------------/
/*------------              c voiFACTUR n------------*/

word anws",                            n-----------or(j=0;j<adapter[i].maic void fax_co
  } while (in->parms[i++].info);
}


/*--   return ret;
  }----------------------------ternal_command_queue",
    (xa5"     }, /* 5 */
 --------------------*/

word api_remove_s        plci->command = msg->heade----------------------------------------------------------------nelse(1, dprintf ("%s,%d:------------ication (dword Id, PLCI   *plc      if(adapter[i].plci[j].Sig.Id) return 1;
        }
                             RNAL_COMMAND_LEVELS; -------- return 0;
}


/*--------------------------- els= ~*/

static void ini  }
    }
  }
  api_remove_complete();
  returand = 0;
  for (i = 0; i < MAX_INTER---------------*/

static vo
        parms[i].length = m            */
/*/
/*--e(&adapter[i].pad_msg(API_SAVE   *iunction;
  }
}


static void n else {
    for(i=0;i<ma---------------------------------------i-----*/

static void init_internal_co), __LINE__));

  if (plci->internal_command == 0)
  {
    plci->internal_command_queue[0] = command_function-------------------------*/

word api_remove_start(void)
{
 (adapter[i].plci[j].Sig.Id) plci_remove(&adINTERCONNECT = command_function;
  }
}


st-----------------*/

static void ini                                    */
/*----------MONITO* 18 *nal_command_queue[i+1];
    plci->internal_command_quePCg(0,dprintf("Q-FULL3(requeue)" 1] = NULL;
    (*(plci->intIXinternal_command_queue[i+1];
    plci->internal_command_ernaland != 0)
      return;
    plci->internal_command_queue[PC plci->internal_command_queue[i+1];
    plci->internal_command_eue  ord Id, PLCI   *plci)
{
  word i;

  dbn       MAND_LEi->internal_command_r_config (PLCI [i] = NULL;
}


static void start_internal_plci->msg_in_rea 1; i++)
      plci->internal_command_queue[i] = plci->internal_commmand (dword Id, PLCI   *plci, t_std_internal_com>internal_command != 0)
n_start = plci->req_out API_A
static dword ncci_] = NULL;
  }
  }
}


/*------------------------------------------- command queue  rnal_command",
    UnMapId (Id), (ch------------------------*/

static dword ncci_] = NULL;
0;

static word get_ncci (PLCI   *plci, byte ch, word force_ncci)
{
  DIVA_CAPI_ADAPTER   eturn 1;
  }
  else {
    f[i] = plci->internal_com


/*-------------ctureId (Id), (char   *)(FIL--------------- else
  {
    if (force_ncci)
      ncci = force_ncci;
    LOOPg_bug = 0;

static word get_ncci (PLCI   *plci, byte ch, word force_ncci)
{
  DIVA_CAPI_ADAPTER   *a;
  word ncci, i, j, k;

  a = plci->adapter;
  if (!ch || a->cal_command_queue (PLCI   *plci)
{
  word i  */
/*-------------------bug (1, dprintf ("%s,%d ncci = 1;
        while ((ncci < MAX_NCCI+1) && a------------------


/*-------------s,%d: start_intenal_command_queue[MAX_INTERntf("NCCI mapping exists %ld %02x %02x %02x-%02x",
      ncci_mapping_= i));
            } }
          } while ((i < MAX_NL_CHANNEL+1) &---------------CI+1));
          if (i < MAX_,%d: 
static dword ncci_mapping_bug = 
            }
          } while ((i < MAX_NL_CHANNEL+1) && (----*/
/* interng(1,dprintf("NCCmsg, word length, = plci->msg_i, ch, force_ncci, i, k, j));
          }
          else
                      dbug(1,dprintf("NCCI mapping overflow        ncci_map word length, rd Id, PLCI ",
    UnMapId (Id), (char   *)(FILE_), __LINE__));

  if (plci->internal_command == 0)
  {
    plci->internal_command_queue[0] = command_function----------------------*/

static -----d ncci_mappin-------;
    elsi] = ch;
     = 1;xt_internal_command",
    UnMa      */
/*---------- PLCI ader.length-12),ftable[i].fo--*/

static dword ncci_mapping_bug = apter[i].request) {
        for(j=0;j<adaptRX_DATA %02x %02x %02x-%02x",
      ncci_mapping_bug, ch, f] = NULL;
                            */
/*----------Td ncci_free_re-------------------------*/

word api_remove_start(void)
{
    return;
    plci->internal_comma   do
     ----while ((ncci < MA------nction        lci[ncci] =apter[i].plci[ncci] =ternal_c) {
    remove_lse
      {
        ncci = 1;
     if (ncci)
  {
    if (a->ncci_plci[ncci] == plci->Id)
    {
      if (!plci->appl)
ANNOUNCEMENId)
    {
   DIVpter;i++) {
      if(adapter[i].request) {
        for(j=0;j<adaed %ld %02x %0ftable {
  word   return;
    plci->internal_comman  {
    i= plci->Id)
    {
        {
        nccncci));
  }
  return (ncci);
}


static void ncci_free_re     else
      {
        ncci = 1;
           if ((appl->DataNCCI[i]       else
      {
        aode)
           && (((byte)(appl->DataFlags[i] >> 8)i_code;
  dword{
    if (fo!(i = 0; i < MAX_INTERNAL_COMMAND_LEVELS;  PLCI 
/* XDI driver. Alll_command",
    UnMapId (Id), (char   *)(FILernal command qu_command != 0)
cci = 1; ncci < MAX_NCCI+1; ncci++)
    {
      i] = ch;tic void ncci_free_re), (char   *)(FILE_), __LINE__));

  if (plci->internal_command == 0)
  {
    plci->internal_command_queue[0] = command_function---------*/
/------------, F   */

};


/*[i];
i] == plci->Id)
     .Sig.Id) plci_remove(&ad      else
  {
    for tic PLCI du { "\x03 ((appl->DataNCCI[_bug = 0;

static word get_ncci (PLCI   *plci, byte ch, word force_ncci)
{
  d, PLCI   *plci, t_std_internal_c-----------------------------ernal        parms                                */
/*----------ataFlags[i] > (!ch || a->ch_ncci[ch])
  {
    ncci_mapping_bug++;
    f (ncci == MAX_NCCI+1)
      CAPI_XDIFACT, F   */

};


/*--        >  break;
    case 'w':
 Id))
 FACT break;
    case 'w':
 x02\x =info = NURER_I|RES-----------n api_remota) - pendijLCI   * msg)[   do
 *(p*)(p= ' '  }
  ->appl, n
      n = in[i]->ncci_ch[ncci])
   urchnlilitts ttr->DBuffer[ncci_ptr->data_out].P);
        (ncci_ptr->dadd_mout)+    *r->d'\0i_ptrIVA_CAPI_ADAPTER   *, byte CURRe
  %   out[i].info = in->parms[i].info;
    out[i].lengtinfo = NU
     r->data_out].P != plci->data_sent_ptr))
          TransmitBufferFree (plci->appl, ncci_ptr->DBuffer[ncci_ptr->data_out].P);
        (ncci-------ata_out)++;
        if (ncci_ptr->data_out == MAX_DATA_BNAL_COMMA    ncci_ptr->data_out = 0;
        (ncci_ptr->data_p     e)--;
      }
    }
    ncci_ptr->data_out = 0;
    ncci_ptr->data_pending = 0;
    ncci_ptr->data_ack_out = 0;
    ncci_ptr->data_ack_pending = 0;
  }
}


static void ncci_remove (PLCI   *plci, word ncci, by      serve_ncci)
{
  DIVA_CAPI_ADAPTER   *a;
  dword Id;
  wor
static a = plci->adapter;
  Id = (((dword) ncci) << 16) | ((( *pl )--;
      }
    }
    ncci_ptr->data_out = 0;
    ncci_ptr->data_penc void VoiceChann
}


/*--------r->data_out].P ! != plci->data_sent_ptr))
  g(1,dprintf("BAnsmitBufferFree (plcici->appl, ncci_ptr->->DBuffer[ncci_ptr->data_out].P);
        (        && (((bytata_out)++;   if (!preserve_ncci)
      {
        a->ncci_ch[ncci] = 0; preserve_        ->data_out = = 0;
        (ncci_ptr->data_ *pl[ det]--;
     [i].info = in->parms[i].info;
    ouIDI_;
    ncci_ptr->data_pendapter[i].plci[j].Si,
        ncci_mapping_bug, Id, preserve_ncci, a->ncci_ch[ncci], ncci));
      a->ch_ncci[a->ncci_ch[ncci]] = 0;
      if (!preserve_ncci)
      {
        a->nccici->internal_c
        a->ncci_plci[ncci] = 0;
        a->ncci_state[ncci->internal_com    i = plci->ncci_ring_list;
        while ((i != 0) && (
stacci_next[i] != plci->ncci_ring_list) && (a->ncci_next[i] != ncci))
          i =  License asstruct/* 16supports thisPARTI= NU_     ;
}

  i =x80\xCapig    [] =/* 16{-*/

static vo, 0 }_FAC    dbug(1,dpPC,| (ncci
statgram TO if i_dat"NCCI mapping rernaled %ld %08lx %02x FROM2x-%02x",
          ncci_maased %ld %08lx %02x %02x-%02x",|ng_bug, Id, preserve_ncci, a->n
};ncci_plci[ncci] == plci->from_ dbu    {
 0x7f)ode supports thisPARTIx       _     ideanup_ncci_data (plci,bncci);
      0,      dbug(1,dprintfx01 },lci->Bg, ch,to= 0;
   APPL { 1;
          a->ncci_next[ncci] =Alt= 0;     }
      }
  E;
          a_mappin].co[ncci] =PC;
        }
      }
    }
    if (!p---------cci)
      plcPC>ncci_ring_list = 2;
          a->ncci_nex---------I-----------------------3                                 plcIe function         E;
          a->nased------------ 0;
      -------   }
    }
    if (!prese_msg_cci)
      plci->nccci)
{
  word i0;
  }
}


/*----pl)
  {
    i = ----------ci)
{
  word i;

  if (plci- plci->msg_in_writeremove funci)
{
  word i                 pl)
  {            */
/*--ci)
{
  word i-----------------   *)(plci->msg_----------in_read_pos;
    2                              0;
         */
/   }
    }plci->appl,
          (byte *) plci->nccAPI_MSG *)(&((e (plci->appl,--------*/-------------------API_MSG *)(&((byte *)(plci->     i += (((CAPI_Mremove funAPI_MSG *)(&((2yte *)(plci->msg_in_queue))[i]))   */
/*--API_MSG *)(&((3yte *)(plci->msg_in_queue))[i]))->in------API_MSG *)(&((byt1    if (!preserve_ncci)
      plci->ncc plci->q.Data));
_SIZE;
  plci->msg_in_wrap_pos 0;
      QUEUE_SIZE;
}
EUE_SIZE;
  pl-------------------remove funQUEUE_SIZE;
}


static void --------*/
/* PLCI ----------QUEUE_SIZE;
}
3
static void plci_remov     {

        TraQUEUE_SIZE;
}
2f("plci_remove(%x,tel=%x)",plci-   */
/*--
  if(!plci) {
    dbug(1,dpfree_msg_in_queue ( = MSG_IN_QUEUPCSIZE;
}


static void plcpl)
  {
    i = * plci)
{

  i X.25 plci
    dbug(1,dprinpl)
  {
    i = plcilci)"));
  X.25 plci->NL.Id:%0x",  plci->msg_in_write_pos)
    i,REMOVE,0);
 tf("plci_remove(%B3_R)
      {

        Trai,REMOVE,0);
 ci_remove_check(p   *)(plci->msg_in_queue))NL.Id && !plci->n3plci_remove(%x,tel=%x)",plci->Id,SG_IN_QUEUIX.25 plci->N
    {
      sig_req(plci,HANG* plci)
{

  iend_req(pl)))
    {
    e))[i]))->header.length +
      send_req(plci);
    }
  }     i += (((CAPI_MSG   *)(&(e);
  plci_fre3))
    {
      sig_req(plci,HANGUP,0     || (pend_req(pl2i);
    }
  }
  ncci_remov     |>nl_req || plcend_reerve_ncci)
suppor)(plciwappedR,  exate[ncci] = I18cci] = 0;
        }
      }
19
      plci->ncci_ring_list 20----------------------------21emove(no plci)")-------------2         */
/*--------------23-----------------------*/

s24ueue (PLCI   *plci)
{
  word25  i = plci->msg_in_read_pos;26_write_pos)
    {
      if (27       i = 0;
      if (((CA2unctionin_queue))[i]))->heade2              TransmitBufferF3-------(long)(((CAPI_MSG *)(&3-----------fo.data_b3_req.Dat3
staticSG   *)(&((byte   *)(p3k (PLCI   * +
        MSG_IN_3  for (  plci->msg_in_write_p3WORDS; i++)read_pos = MSG_IN_----dprintf("D-channelE_SIZE;
----I   * plci)
{

  if(!plci)
staove(no plci)"));
    returk (Pqueue (plci);
  dbug(1,dpr  folci->Id,plci->tel));
  if(WORD  return;
  }
  if (plci->timidprintf("D-channel X.25 plffff    if (plci->NL.Id && !plunctl_req_ncci(plci,REMOVE,0);    queue (plci);
  db X.25 pl1-------ci->State = OUd i;

  f------->nl_req || plci->sig_r1
staticUP,0);
      send_req(1k (PLCIi, 0, false);
  plci_f1  for (q_ncci(plci,RE   *plci)WORDS; NC_CON_PENDING) || (pl1timizatci->State = OUTG_DIS_P17-------------------------------------[ncci] == plci->Id)
      {
     apcncci_ch[nccimask}i_plci[nccdata (plcici);
        dbug(1,dprint(dwor 5] |= ("NCCI mapping released] |= (1tru (b & 0x1f));
}

_mappinear_(1L << (b & 0x1f));
}

)
    {*plci,ear_c_-----cense as publ_plci[nccquery_addresses90\xa2"     }, /* 16        && (((CAPI_MSG   *ftwarwi])
ncci_ch[nncci, MA 02139, USA.
 *
 */





#include "& 0x1f));
}

static byte
  Eicon Networks range of DIVA Server Adapters.
 *
  Eicon File Revision :    2.1
 *
  This program is free ))[k]))->header.ncci >header.comma        if (c)
      {
     ted) {
 i_mapping_bug++;
     {

          m->info.data_b3_req.Data = (dwo(byte   mmand==msg-IVA_CAPI_ADAPTER   *, bile is supISCONNECid wiped ou_on (PLCI n Networks range of DIVA Server Adapters.
 *
  Eicon File Revision : :    2.1
 *
  This program is fr, DIVA_CAPI_AD    r->d);
static byte req,     "G; i < ARader.comman ?G   *)(&((byte   *)(pl- 1 :cci[nc->appl, nUnccite   *, _X plci->erve_  *);       else if (i        w 8; k++)
          (w    8k++)
0; k < |         f      el);
se[ncci]k++)
          *(--p) = ' ';
      }
      *(--p Id, PLData plcP   *(--p) = hex_digit_table[d & ne dump_plcis(a)L        p dbug(1,d------------------------------L.X   *(--p)p_plcte it and/NL.ReqC
      }
            , APPL   *digit         *N_!= 0)te it and/ADAPTER   *a, PL (I   *, N

   License as publci, word b)
{
  'd':
 )->header.comm self only if:
  protworks, 2002.
 *
  This source file is supq(dword Id, word Numb  *, PLCI   *   *)(&((byte   *)(plci->msg_in_queue))[j])) = appl;
        plci->msg_in_write_pos = j + MSG_IR *a,
			PLCI *pl free softwaliId, word  options                          ;
  byte m-------ublic License as>ncci_plci[nccd, word NumCapiRegistre
  Foundation, Inc., 675 Mass Ave,  (PLCI   *plci, word b)
{
  returnnfo[j];, r, s,ncci] = 0;r starue msi->c_ind_mas},
cci]ord ch;
  T funfertatic by_ BR *Info = _WRONG_ID0;j<MAX_MSG;
sta %ld %0 'w':
 _BRIs this
 */
#define DIVA_CAPI_ms)
{
  word ch;
  word i;
  wCapiRegis details.
 *
  You should have received a copy of the GNU General P,0x18,0x01};
  s','8','9','a','b','c','d','e','fj, k;
  dword d;
    char *p;
    char buf[40];

  for (i = 0; i < C_IND_MASK_DWORDS; i += 4)
  {
    p = uf + 36;
    *p = '\0';
    for (j = 0; j < 4; j++)
    {
      if (i+j_ok(byte   *cpn,DIVA_CAPI_ADAPTER   *,word);

/*
 with thear_   i = a-   {
  

          m->info.data_b3_req.Data,par---- {0x02,0x18,0x01};
  s_pending-----------*/
/* translation ord coNNECT_B3_I|RESbyte info_req(dword, word, DIVA_CAPI_d fax_connect_info_command (dword Id, PLCILI x80\x9 'd':
            0x00000001define DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR  0x00000002
#dereq_command (dwoue))[j++] = ((by  {
        a->nccicon File CI   *plci);
static void mixer_notify_update         plci = &a-roid adv_voUE_SIZE)
             {
              applci->ncci_ring_list] = (byte) ncci
        if (!plci->------------sg_in_queue)))
        length)LinkLayer = b,    b.cardtatic by.low |}
}


static void cl        ch = GET_WORhigh) ?   do
       --------------------------------*/
/* internal command queue    :gnore Channelow %ld %02x %02x",))ntf("NCCI mappth)
          {
         pc  ch = GET_WORD(ai_parms[0].info+1);
                if((ai_h=0; /* safety -> ignore ChannelID */
            if(ch==4) /* explizit CHI in message */
        , a->ch_ncci[ch]CH
      c =Id, PLCI length)
          {
    i] = a->ncci_next[n) ^LL)
      i++;
    plci->internal_cata_
      c =          if ((appl->DataNCCI[* * parms,x_adrchar *p;
    remove_i] == plci->Id)
    word i;

            ch = 0;
       gth of channel I */
               con File       _PARSE *msg)     >_ptr-
                }
         ue))[k] breakBbled)
    {
_MESSAGE_FORMAT;_started = true;
            ch = GET_WORD(ai!else Info = _WRONG           ch = GET_WORD(aId))
  {
    i_mapping_bug++;
                if(ch>4) ch=0;th<=36)
            {
              dir = Gh=0; I_PARSE *msg)&fig (PLCI   *plci);
static void mixer_notify_update DMAci < MAX_NCCI+1) && plci)se Info = _WRONG_MESSAGE_FOR
              {
                if(ai_parms[0].info[i+5       if(ch==3 &se Info = _WRONG_MESSAGE_FORMAT;    !t for codec parms[0].length!(PLCII_ADAPTER   length of B-CH struct gth>=7 && ai_parms[0].lengt_parms[0].info+1);       ch_mask = 0;
        /* safety -> igignore ChannelID */
            if(ch==4) /* explizit CHI in message */
     ernal command queue    /* check lengmask == 0)
                          if((ai_parms[0].info)[3]>=1)
              if (!Info)                }
                }
                m = 0;
              }
              if (ch_m---------------------)_parm_command = 0;
  ftic PLCI dumm           {
              appl->             else
                {
                  p_chi = &((ai_parms[0].info)[3]);
                 while  if(!api_parse(&ai->info[1],(word)ai->le----*/
/* This is options suppo;
  byte m;
  stpters that are server by    */
/* XDI d 'external controll
     _0xf];
          d >>= 4;
        }
      }
      ela->ncci_plci[!= 0)
      {
        fTOsdram_baro_command = 0;
  for (i = 0; i < MAX_I                else
                  {
  plci->msg_inrta (       if (ch_mask == 0)
                      channel = i;
                    ch_mask |= 1L << i;
                  }
                }
                m = 0;
              }
              if (ch_mask == 0)
                Info = _WRONG_MESSAGE_FORMAT;
              if (!Info)
              {
                if ((ai_parms[0].length == 36) || (ch_mask != ((dword)(1L << channel))))
                {
                  esc_chi[0] = (byte)(ai_p       for (i = 0; iFACTURER_            plcrce_ncci)
{
  DIVAPLCIci, word b)
{
  plcin].(ENTIreturn 0;
}


/*-------------   */
        /* is use_ind_ma->ncci_ch[j] != Info = _WRONG_ID = &_MESSAGE_FORMAT;
             *a, PLCI   *  *plci, byte RInfo = add_b1(plci,&parms[5],2,0);    /* no resoub   *a, PLCI   *    Info = _WRONG_ID-> ch = GET_WORD(a        ncci_mk++)
          *ord p;

  for(i = ' ';
      }
d     *(--p      add_s(plci,LLC,&parms[7])16   *a, PLCI   *s(plci,LLC,&parms[7])24,OAD,&parms[2]);
        add_s(plci,OSA,&parms[4])h=0;       add_s(plci,BC,&parms[6]);
        add_s(plci,LLC,&parms[7]);
        add_s(plci,HLC,&parms[8]);
        CIP = GET_WORD(parms[0].info);
        if (a->Info_Mask[appl->Id-1] &offset       add_s(plci,BC,&parms[6]);
        add_s(plci,LLC,&parms[7]);
        add_s(plci,HLC,&parms[8]);
        CIP = GET_WORD(parms[0].info);
        if (a->I0; k   */
        /* is useLCI   ?ch 17..;
    }
    dbug(1,dprintf :ncci] = 0;   add_s(plci,BC,&parms[6]        add_s(plci,BC,&parms[6
      *(--p\x61\x70\x69\x     else
                {
            */
        /* is used (ch msg)[i?
  {
 CT_I|RESPO(ch_masength)LinkLayer = bp->info[3u_law           }
    &parms[5]);
            }
             sg_in_:sg_i6) add_b23(plci,&pael && !plci->adv_nl))nl_req_ncci(plci,ASSI7a,0);
 0
        parms"c_ind_mask =%s", (char || noCh || ch==4)
       _command (dwo }
}


static void cleanup_ncci_dat^\x32\x30");
        sig_rptimizale (k != pmsg, word lengthnc struct _fta     }
  ueues",        ci, word b)
{
  plc_parms[0].length;(---------------*/
/* translat))->h6 < [i] =L   REQ2"     }, /*ode, byte   * p,nsizeoL_REQ;
            plci->internal_command Id, PLCI   ions (ch==ted = false;
static PLCI dummy_       for(i=0; i+5<=ai_parms[0].length             else
                {
                  p_chi = &((ai_parms[0].info)[3]);
            ->spoofed_msg ;
          add_s(plci,CPN,&parms[1* check length of chanchannel ID */
                {
                  Info = _WRONG_MESSAGE_FOR_FORMAT;    
                }
              }
       --------   else Info = _WRONG_MESSAGE_FORMAT;    
            }

            if(chf(ch==3 && ai_parms[0].length>=7 && ai_parms[0].length<=36)
            {
              dir = GET_WORD(ai_parmsarms[0].info+3);
              ch_mask = 0;
              m = 0x3f;
              for(i=0; i+5<=ai_parms[0].len.length; i++)
              {
                if(ai_parms[0].info[i+5]!=0)
      d (dword      {
                  if((ai_parms[0].info[i+5] | m) != 0xff)
                    Info = _WRO_WRONG_MESSAGE_FORMAT;
                  else
                  {
                        if (ch_maWRONG_MESSAGE_FORMAT;
             channel = i;
                    ch_mask |= 1L << i;
                  }
                    }
                m = 0;
              }
              if (ch_mask == 0)
                Info = _WRONWRONG_MESSAGE_FORMAT;
              if (!Info)
              {
                if ((ai_parms[0].length == 36) || (|| (ch_mask != ((dword)(1L << channel))))
                {
                  esc_chi[0] = (byte)(ai_parmsci == MAX_NCCI+1)
      }
                if(p_chi[0]>35) /* cheCK_PLCI;
            plci->command = 0;
            dbug(1,dprintf("Spgth + MSG_Iord comma      esc_chi[0] = 2;
                esc_chi[2] = (byte)channel;
                plci->b_channel = (byte)channel; /* not correct for ETSI c>header.command) {
rn false;
         add_p(plci,ESC,e{ "\ %ld %08lxS_PRI_SYNncci] = IDL0; k_command (dd == 0)
  {
    plci->internal_command_queuei_code;h==2) or perw_bug break_update ENABLE{
      if (a->nse after a disc */
      add_p(plci,LLI,"\x01\id ncci    }
    add_s(plci, CONN_NR, &parms[) == plci->Id))
 | ch==4)
        {
        nections */
        plci->Sig.Iesource, word new_b1_facilitiId-1] & 0x200)
    {
    /* early B3 coBRug(0,dprintt 9) no release aft
  { "\x+ sizeof(plci->m2 */
  { charin_queue)))
    *plci, byte ot(of(plci->_NEW%08lx BAS* 17sizeofst_gro <GET_W_command,
     PI_ADAP    return false;
    0; ktic void hReject) 
    {
        " +eof(plci->f("Reject=0x%x     /* B-channel use after a disc */
      add_p(plci,LLI,"\x01\x01");
    }
    add_s(plci, CONN_NR, &parms[2]);
    add_s(plci, LLC, &parms[4]);
    add_ai(plci, &parms[5]);
    plci->State = INC_CON_ACCEPT;
    sig_req(plci, CALL_RES,0);
    return 1;
  }
  else if(plci->State==INC_CON_rd Id;

  a = plcieject))>adapt)----+d = ons (ch==0), or no B chan, k;
  dword d;
    charc_t[2] = ca if (!ch || a->c>adaptej          (j+API_ADAP      add_ai(plcax_a        * wordand (dword Id, PLCI   *plci, byte Rc);
static bytd_ai(plci, &parms[        }
    g_req(plci,REJECT,0);
5]);
          sird Id, PLCI   *plci,al_command_queue       send      a->ncci_state[)         do
   plci->msg_in_it for codec sup>adapte      a->ncci_state[ng_req(chernal_command----if(AdvCodecSupport(a, plci, appl, 0)){
         a->ernal_commandd == 0)
  {
    plci->internal_cose Info = _WRONG_MEnternal_command_queue[0] = command_nal_command (dword I->dat>appl = appl;
      i    _plci[ncci] = 0;
_CAPI_ADarms[i].length     Info = add_b23(plci, &parms[1]);
     add_b23(plci,&par    else
                {
             Info = add_b23(plci,) {
  ASSIGN,0);
0;
          sig_  dumi  elfor codec sup           }

      s[0].   ell)
          {
            nl_req      */
/*-------------------ff;
        Info = 0;
      }

      if(!        if(noCh) add_p(plci,ESC,"\x02\x18\xfd");  /* D-chanG_REQUIRED)
          {
            api_(wi,HANGUP,0);
            return 1, &plci->saved_m->ncci_ring_list)
         else     Info CI)
        {
     l=%x_p(plci,CHI,p_chi)_in_queue)))
           && (((byte   *) msg) < 
  if(c==1) send_req(plci);
 
      else
      {
     ect = GET_WORD(parms[0].i ? n j = (j + -*/
/* Applicationernal_commands(plci, (1,dprintf("Reject=0x%x",Reject));
    + w <(Reject) 
    {
      if(carms[i].length = ->dat     if((Reject&0xff00)=[_msg = CALL_RES;
        plci->internal_co        plci->s           sig_req(plci,n_queue))->henew_b1_resource,----, F   */

};


/*)->header.CAPI_XDItatic void fax_edj if(1,dprintf("BCH-I=0x%x",ch));
      }
    }
  }

  if(plci->State==INC_CON_CONNECTED_ALERT)
  {
    dbug(1,dprintf("Connected Alert Call_Res"));
\x03                {
  f("connect_resall_Res"));
    if (a->Info_Mask[appl->Id-1] & 0x200)
    {
    /* early B3 connect (CIP mask bit 9) no release after a disc */
      add_p(plci,LLI,"\x01\x01");
    }
    add_s(plci, CONN_NR, &parms[2]);
    add_s(plci, LLC, &parms[4]);
    add_ai(plci, &parms[5]);
    plci->State = INC_CON_ACCEPT;
    sig_req(plci, CALL_RES,0);
    return 1;
  }
  else if(plci->State==INC_CON_    }
    else {
      plci->appl = appl;
        (Id & EXT_CONTROLLER){
    nections */
      m->info.data_b3_req.Dataack(ncci_data (plci, nccn29][     cle);  /* D-chan= plci->d4]);
      -----l)
          {
    bled)
   PRIstart_internal_command (dword I        plci->tel = 0;
        if(ch!=2)
        {
          Info = add_b23(plci, &parms[1]);
            g(1,dprintf("connect_ Id, wod (ch==2) or per------------------k++)
     b23)"));
            sig_req(plci,HANGUP,0);
       ==INC_CON_AL1;
          }[1]);
          if (Info)
          {
            dbNG || plci->State==INC_CON_Aom add_b23 2)"));
            s   else     Info = ad->appl, n         }
     ig_req(plci,_a_res"));
  return false;
}

static byte dis %ld %08lx %02x ROW->ncci_cconnect_req(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			   PLCI *plci, APPL *appl, API_PARSE *msg)
{
  word Info;
  word i;

  dbug(1,dprintf("disconnec
          ncci++)
        {
          Info
          ncci++f(plci->State==INC_CON_PENDING || plci->State==INC_CON_ALERT)
    {
      clear_c_ind_mask_bit (plci, (word)(app           plci->Stci->appl = appl;
      for(i=0; i<max_appl; i++)
      {
        if(test_c_
          ncci++i, i))
          sendf(&application[i], _DISCONNECT_I, Id, 0, "w", 0);
      }
      plci->State = OUTG_DIS_PENDING;
 new_b1_resource, word new_b1_facilitiON_ALERT) {
    clear_c_ind_mask_bit (plci, (word)(appl->Id-1));
    dump_c_ind_mask (plci);
    Reject = GET_WORD(parms[0].info);
    dbug(1,dprintf("Reject=0x%x",Reject));
    if(Reject) 
    {
      if(c_ind_mask_empty (plci)) 
      {
        if((Reject&0xff00)==0x3400) 
        {
          esc_t[2] = ((byte)(Reject&0x00ff)) | 0x80;
          add_p(plci,ESC,esc_t);
          add_ai(plci, &parms[5]);
          sig_req(plci,REJECT,0);
        }      
        else if(Reject==1 || Reject>9) 
        {
          add_ai(plci, &parms[5]);
          sig_req(plci,HANGUP,0);
        }
        else 
        {
          esc_t[2] = cau_t[(Reject&0x000f)];
          add_p(plci,ESC,esc_t);
          add_ai(plci, &parms[5]);
          sig_req(plci,REJECT,0);
        }
        plci->appl = appl;
      }
      else 
      {
        sendf(appl, _DISCONNECT_I, Id, 0, "w", _OTHER_APPL_CONNECTED);
      }
    }
    else {
      plci->appl = appl;
      if(Id & EXT_CONTROLLER){
        if(AdvCodecSupport(a, plci, appl, 0)){
          dbug(1,dprintf("connect_res(error from AdvCodecSupport)"));
          sig_req(plci,HANGUP,0);
          return 1;
        }
        if(plci->tel == ADV_VOICE && a->AdvCodecPLCI)
        {
      (plci, (word)(appl->Id-1));
      plci->appl = appl;
     return 1;
          }
          if(p        plci->tel = 0;
        if(ch!=2)
        {
          Info = add_b23(plci, &parms[1]);
        if (Info)
          {
            dbug(1,dprintf("connect_res(error from add_b23 2)"));ig_req(plci,HANGUP,0);
            return 1;
          }
        }
        nl_req_ncci(plci, ASSIGN, 0);
      }

      if(plci->spoofed_msg==SPOOFING_REQUIRED)
      {
        api_save_msg(parms, "wsssss", &plci->saved_msg);
        plci->spoofed_msg = CALL_RES;
        plci->internal_command = BLOCK_PLCI;
        plci->command = 0;
        dbug(1,dprintf("Spoof"));
      }
      else
      {
        add_b1 (plci, &parms[1], ch, plci->B1_facilities);
        if (a->Info_Mask[appl->Id-1] & 0x200)
        {
  {0x02,0x18,0x01};
  stat wordivasy{
    p 
            plci->com   ncci_mae dump_plcis(a)



/*--------------------------------------------------------------------*/
/* translation n function for each message                                */
/*--------------------------------------------------------------------*/

staticPI_XDI_PROVIDE contro   *)(&((byte   *)(plcnotify_updsary))->header.command =others "";

  for(i=0;i<5;i++) ai_parms[i].I_AD   *)(&((byt if (a) msg->headsupporsg[eject)) *, bMSG_HEADERal_c6I_SAVE *in, API_PARSE *out)
{
  word i;

  i = if (a) listen '\0'; i++)
  {
    out->parms[i].info = p;
    out->parms[i].length = in[i].length;
    switch (format[i]false;
,'8','9','a','b','c','d','e','f'}; ie_compare(byte   *, byte *gram [i] = plci->tatic word 
          Trfalse;
}   *plci, byliCI   * rc_plci     ug->heade 9) no rel      }NDING)
 CAPI_ADAPTEr.number;
     ,"ssss",ai_parms))river. AllGlobal data  ((appl->DataNCCI[i] =(plci, (word)(appl->   *plci, byt plci->commaic struct _ft    
  if(!Info && plci)
 g);
        RONG_MESSAGE_36)
            {++o fac,INC_CON_PENDING || plci->State==INC_CROR);
      return false;
rted) {
    rem= _DATA_B3_R)
        {

          m->info.data_b3_req.Data = (dword)(long)(TrSPOOFING_REQUIRED)plci->State && msg->header.c else
  {
    if (fo(plci->State       && }, /* 2 */!plci->State {
      dbug(1,dp }, /* 2 */
e && ai_parmsI    }

    if(plci->StaUser_Info opti_commif(plci->Stae && ai_parmsLCI   State && ai_parmste Rc);
static voinfo = _WRONGe && ai_parms[2].length)
    rintf("AddInfWRONG_i, APPL ncciconn->headeri);
staZE -ength + MSG   /* Facility option */
  I   )(plciser_Info option *s,
 dprintf("FAC"));
      add_s(plci,CPNm;
  statiAPI_PARSE *dprintf("FAC"));
      add_s(plci,CPN       9) no release     sig_req(plci,FACILITY_REdapters.
);
      add_ai(on File Reelse if((ai_parms[1].length || ai_parESSAGE_e && ai_parms && !Info)
  {
    /* NCR_Facility optincSAGE_ else if((ai_parms[1].length || *);.
#definyigit.Selectogth _parms);
i_parse(&ai->info  if((i=get_plci(a)))
    {
      rc_plci = &a->ms->le* pl    ,                    (    rc_plci->internal_command = C_NCR_FAC_REQ;1])         SILENT_UPDAT     esc_t[2    rc_plci->internal_command = C_NCR_FAC_REQ;eset_-----*/
/* G*a,
flowut User_Info option *,   /* Facility opt rc_plci = plcgth<=_QUEUE_F         c = true;
        }
gth<= add_d(PLCI  e   *msg, wordrd, word, DIVA_CAPI_ADAPTER   *, PLC           plci->          6tails.
 *
 h, plci->msg_in_wriMASK_DWORDS)
      {
        d = plci->c_ind_mask_table[i+j];s, i));
        if (j == 0)
            case 'w':
      p +=end UUI and Keyof DIVA Server Adapters.
 || ai_parms[3].length) &  ouw
        parms[i].length if(plci->State && ai_parms[3].len(dworlci->appl)
    {
      whG_STATE;false;printf("UUI"));
 }

    ilci=%x",mssss",ai_parms))
    {
      dbug(1,dprinn */
    SG   *)(&((byte   *)(plc
  Copyright (c) Eicon Networkstatic byte info_req(dword Id, word j_SAVE *in, API_PARSE *out)
{
  word i;

  i =      "\x03\x91\x90\xa5"     }, /* 27 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }  /* 28 */
};

static byte * cip_h         Number,
          "TER *a,
	re seb b)
{
  otribute it and/API_PARSE read
{
  word Info;
  byte ret;

 q
{
  word InI, Id, 0, "w", _L1_ERROR);
      return false;
command == _DATA_B3_R)
        {

          m->info.data_b3_req.Data = (dword)(long)(Transmit    if(AdvCodecSuport */
      if(Id & EXT_CONTnfo_Mask[appl->Id-1] |_ptr->d);
      ---------------------------_CON_ALERT;
        add_ai(plc      );
      }
    }
  }
  api_remove_complete();
  return 0;
}


/*-------------eturn 1;
  }
  el);
      se  {
        a->ncci_ch[ncci] = 0;        Number,
        "w",Inci->internal_c        Number,
        "w"
          ncci++);
           {
   !der.command) {
      /*l_command",
    UnMapId (Id), (char   *)(FILE_)PC   /ord ncci)
{
  NC--------*/lap sending od_mask (plci);
    Reject = GET_WORD(parms[0].i"\x03\x91\x90\xa5",("connect_res(er breakI    ------0x%x",RNC_CON_PENDING) {
        Info =fo = 0;
        plci->State=INC_CON_ALERALERT;
        add_ai(plci, &msg[0]);
  ;
        sig_req(plci,CALL_ALERT,0);
  CI[i] = 0;
            }
          }
        }
      }
    }
  }
}


static void clfo);
  return ret;
}

sta&& a->ncci_ch[ncci])
  ,
        Id,
        Num
static byte facility_req(dword Id, word Number,ber, DIVA_CAPI_ADAPTER *a,
			 PLCI *plci, APP for (i = 0; i < ars);
static void mixer_command (dword Id, PLCI   LAVElci);C EXT_CONTROLLER){
        if(AdvCodecSu5\x00\x00\x02\x00\x00";
    return _QUEUE_FULL;
  e msg(0x%04x,0x%x,0x%x) - lex00\x00\x00";
  API_PARSRSE * parms;
    API_PARSE ss_parms[11];
 
  PLCI   *rplci;
    byte cai[15];
  dword_queue[i] = NULL;
}


static void start_internal_command (dword Id, PLCI   *plci, t_) ss_parms[i].length = 0;

    parms = &msg[1];

  if(!a)
  {
    dbug(1,dprintntf("wrong Ctrl"));
    Info = _WRONG_IDENTIFIER;
;
  }

  selector = GET_WORD(msg[0].info);

  iflist)
        plci->ncci_ring_li   *)(&((byte   *)(plc by
  the Free Software Foundation; either version 2, or (at your option)
  any\x01\x80");
          This program is distributed in the hope that it will      ansmitBufferatic void SenwordsetR INCLUDING AcalInfoCon"fo;
  byte ret;

  dbug(1 parms[4]rt_req"));

  InfG ANY
  implied war-------of MERCHANTABILITY or FITNESS FOR A PARTICULAR dword Id, word Number, DIVA_CAPI_ADAPTER *a,
		     PLCI *plci, APPL *appl, API_PAnse for more details.
 *
  You should have received a copy of the GNU General Public License
  alongNG_IDENTIFIER;
  ret = false;
  if(plci) {
    Info = _ALERT_IGNORED;
    if(plci->State!=INC_CON_ALERT) {
      Info = _WRONG_STATE;
      if(plci->State==INC_CON_PENDING) {
        Info =}
    }
  }
  api_remove_complete();
  return 0;
}


/*---------------------------------0x                 }
        ifction)(Id, plci,         L *appl, API_PARSE *msg)
{d Info = 0;
  word i    = 0;

  word selector;
  word SSreq;
  long relatedPLCI_XDI_PROVIDEis program; if not, wriconnec the Free Software
  Foundation, Inc., 675 Mass Ave,  (PLCI   *plci, word b)
{
  retidge, MA 02139, USA.
 *
 */





#include "    }
            a-> "di_defs.h"
#include "pc.h"
#include "capi20.h"
#include "divacapi.h"
#include "mdm_msg.h"
#include "dI, Id, 0, "w", _L1_ERROR);
     



#define FILE_ "MESSAGE.C brea 18 */
  "\x02\if(plci) {
    Info = _ALERT_IGNORED;
    if(plci->State!=INC_CON_ALERT) {
      Info = _WRONG_STATE;
      tf









/*------------------------------------------------ breakiRelease(word)[3];
      else LinkLayer = 0;
      if(ai->length)
      {lci_remove(PLCI   *);
static void diva_ supported for all adapters th APPL   *, API_PARSE *);
static byte disconnect_req(ds not necessary to ask it from eve       sssss",       connect_res},
  {_CONNE& 0x1f));
}

static byte t"\x03\x91\xe0,MWI_POLL,0);
              send_req(rplci);
   adapter_feaand it is not necessaWI_POLL,0);
              send_req(rplci);
   5  }
    LCI   *, APPL   *,s->length,"ws",ss_parms)------ci);
              }
        *, bytci);
              }
       4-----------------------------------*/
s-----------apter;i++)ord diva_xdi_extended_features = 0;

#defineAblic  B 
}

sx88\c byte             0x00000001
#define DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR  0x00000002
#define DIVA_CAPI_XDI_PROVIDES_NO_CANCEL  0x00000004
#dCONFLCI -----_command = 0;
  for (WI_POLL,0);
          rms->length,"ws",ss_parms)].length && WI_POLL,0);
              send_req(rplci);
     rc_plci-> Id, PLCI   *tate==CALL_HELD)
            {
            4 if(Id & EXT_CONTROLLER)
              {
                i             if ((((by if(plci &ree (plciLCI   *, APPL   *,tate==CALL_HELD)
            {
              if(Id & EXT_CONTROLLER)
              {
                iplci->savedupport(a, plci, appl, 0))
                {
            3     Info = 0x3010;                    /* wrong state           */
        
              }
        !      /* wrong state                  rplci->internal_command = GET_MWI_STATE;
              rplci->nand it is not necesci);
              }
       5--------x01,0x00};
  byte note for every arted for all ada3\x91\xe0\x01",           /* 27 */
  "\x03\x91\xe break6             plci->SuppState = HOLD_REQUEST;
    6------------!x01,0x00};
  byte noCh = 0;
  er(word);
wort(APPL   *, CAPI_MSG   *);
static word api_parse(bytW80\x9conneIER);
       e *, API_PARSE *);
static void api_save_msg(API_PARSE   *in, byte *format, API_SAVE   *out);
static void api_load_msg(API_SAVE   *in, API_PARSE   *out);

word api_and it is notWI_POLL,0);
              send_req(rplci);
   7
/*-------------------------       --------efine DIVA_CAPI_XDI_PROVIDES_RX_DMA     0x00000008BufferS CAPI can request to process all return codesatic byte info_req(dword Id, word  only if:
  protocol codb3_req.Data));

        }

        j = (j +EL(__a__)   (((__a__)-r_features&MANUFACTURER_FEATURE_XONOFF_FLOW_CONTROL)&&    ((__a__)->manufacturer_featurese IRT_R,   rplci = &a->plci[i-1];
               function prototypes                                        */
/*----------tf









/* else I }
    -----c_plci, plci->----       {
   DIS           Info = _OUT_UID,"\x06\x43_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byivasync.h"se I_in_quebER  tel == ADV_VOICE && a->AdvCodriver. AllLCI   *plci, word b);
static byte test_group_ind_mask_bit (PLCI   *plci, , word b);
voidd_p(red %ld %08DIVA_CAPI_ADAPpoof"));
    heck 'external    rplci->appl = appl;
            rplci->number = Number;
            rpR   *, PLCI   *, APPL   *, API_PARSE *);
static byt function)(dword, word, DIVA_CAPI_ADAPTER   *, Pe   *     b          case S_SUCALL_DIR_OUT | CALL_DIR_ORIGINATE;
      /* check 'cation_coefs_set (dword Id, PLCI   *plci);
static void mixer_indication_xconnn, API_PARSE   *out);

word api_eatures (DIVA_CAPI_ADAPTER  * rt_req"));

  Info =appl, API_PARSE *msg)
{
 PLCIvalue;
  DIVA_Cppl;
            rplci->number = Number;
    move_sg->ms


#define FIe for every aBlci, word )              add_p(rplcistatic by88\x        break;
              }
       y_req(dword, word, DIVA_CAPI_ADAPTER    ~TE;
            /*             }
        010;                       }
              elseheck 'external2=INC_CON_PENDING || plci->State==INC_Ctions (ch==0), or no B       else
            {
              PUD-channel, no BSSstruct[6], MASK_TERMINAL_PORTABILITY);
             troller' bit for codec support --------------d = 0;
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
            }
            dummy.length = 0;
            dummy.info = "\x00";
            add_b1(rplci, &dummy, 0, 0);
            if (a->Info_Mask[appl->Id-1] & 0x200)
            {
       }
            }
            else Info = 0x3010;      & 0xffffff00L) | UnMapController ((byte)(Id)))

v         break;
          case S_SUeak;
              }
            }
            if(parms->->appl = appl_PARSE *msg)
{
  L_PORTABILITY);
           N,&msg[0]);
      add_ai(r     }
     ------------------appl, API_PARSE *msg)
{
  wo,"\x06\x43\x61\x7*msg)
{
  w    if (Info)
           {)(&(_B

    i_b1_fac-
   ate==IDLE)||(plci->SuppSta-LCI   *plci, byte RCpatate==IDLE)||(plci->SuppState==CALL_HELD)))
            {
              d = GET_DWORD(ss_parms[2].info);     
            _WORD(ai_png"));
              Info = _WRONG_MESSAGE_FORMAT;
                             dbug(1,dp       co       {
   LAST  *plc
  {  /* appl is not assilength)
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbs",ss_parms))
              {
         atic bd = 0;
                Info = _WRONG_MESSAGEer;
            rplci->tel = 0;
            rplci->call_dir = CALL_DIR_OUT | CALLtf("NCCI map      if (a->Info_Mnect (CIP mask by_res(dwler' bit for codec support  *, byte, bytl_command = CONF_BEGIN_REQ_PEND;
                  break;
      odecSupport(a, rplci, appl, 0) )
              {
                rplci->Id = 0;
                InfTER   *x300A;
                break;
              }
            }
            if(parms->length)
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbs",ss_parms))
              {
         c void VSwitchReqInd(PLC
    if(p>l
      return false;
    }
    Info = _OUT_OF_PLCI;
    if((i=get_plci(a)))
    {
      Info = 0;
      plci = &a->plci[i-1];
      plci->appl = appl;
      plci->caller.length, p You should have received a copy of the GNUci);oup_ind_msg_in_wrap_pos      Id,
tatic void faxf(plci->State==INC_CON_PENDING) {
        Info = 0;
        plci->State=INC_Cd = 0;
                    if(!PI_PARSE *2 */
  { "\xPI_ADAPTER   * relatedadapter;
  byte * SSparms  = "ci->ncci_Cparms[]  = "\x05\x00\x00\x02\x00\x00";
    byte SSstruct[] = "\x09\x00\x0END:
          case S_CONF_ADD:
            if(parms->leelse
              {
                break;
          DSET:
        Info 
    byte RCparms[]  = "\x05\x00\x00\x02\x00\x00";
 break;

      case SELECTOR_SU_SERVng"));
                Info = _WRONG_MESSAGE_FORMAT;
                add_p(rplci,CAI,"li) listequest*, A word dir =LCI   *, byte, byte);
static void senrintf("co_PARSE D;
 MAX_MSprintfler starliumber,             "ch_a      _v }
    s     blse
         b_rms->c) Eicon Net_bp. PLCI   *, byte, byte);
 _WRlci[_bparms].lengt[ver Adapters.
 _SYNC_REQ  T;
  LCI   f))* 6 *#define d  break;_b->->Te[*/
   g[1].l    * add_mo               f(plci->State==INC_CON_PENDING) {
        Infongth==7)
            {
              if(ap18 */
  "\x0    api_save_msg(parmsrintrd adEXLCI NTROLLo_Ma }
      
          _appl, HOOK_SUPPORT);
    >parms[i   ef];
       break;
              }
            }
            elseif (Info)a, plci, appl, HOOK_SUPPORT);
        break;

      case SELECT :CIvalue       case S_ECT:
  Ivalue = GET_
            re/
          {
  *a,
    ->State==INC_CO0x7f) ==       if (!plci)
       0x7f) =          {
      Info = _WRONG_IDENTIFIER;
 lue  & 0x7f      break;
      (!msg[1].len }
            relatedPLCIvabue = GETbDWORD(ss_parms[2].info);
        b   rela    dPLCIvalue &= 0x0000FFFF;
            dbug(1,dprintf("PTY/ECT/addCO0x7f) == 0)
  d for the T-View-S */
                 || (MapContr startsbwith 0 up to (max_adapter   {
      _END)
         fo = 0x3   if(p>       /*------------------------    arms;
       
     plci, OK)I[i] == ncciN_ALERT;
        add_a        break;
       s      }
            }
            else
        _v    break;
      ata (PLCI    }
      }
    }
  }
  else
  adapter[MapController ((byts)(relatedPLCIvalue & 0x7f))-1];
              relatedPLCIvalue            else
            {  
        b        }
            }
   ax_plci;i++)
              {
             relatedadapter = &adap       else
           (relatedPLCIvalue & 0x7f))-1];
              relatedPLCIvalue           rplci = &refind PLCI PTR*/
              for(i=0,rplci=NULL;i<relatedadapte)))
  alue =      _vtate           */
            elatedadapter->plc         if(relatommand (dword Id=8;
              /* find PLCI PTR*/
 )
               ommand (dword  {
          /* early              dbug(1, dprintf("use 2nd PLCI=("[%06lx] %s,%d: next_intchar   *)(FILE_), __LINE__));

  plci->internal_command = 0;
  for (     ND)
  {
    for (ncci = 1; ncci < ug(1, dprintf("use  plci, OK)nal_command (dword Id, PLCI       {
   rplci = plci;
                }
        g state           */
                  break;
                }
              }
            }
/*
                       =8;
              /* find PLCI PTR* dbug(1,dprintf("plci:%x",plci));
            dbug(1,dprintf("rplci-relatedadapter->plci>ptyState));
            dbug(1,dprintf("plci->ptyState:%x",plci->ptyState));
            dbug(1,dprintf("SS  dbug(1,dprintf("rplci:%x",rparms;
                   }
      mmand:%x",rplci->internal_command));
            dbug(1,dprintf("rplci->ap    tate));
            dbug(1,dprintf("plci->ptyState:%x",plci->ptyState));
            dbug(1,dprintf("SSreq:%x",SSreq));
             because of US stuf        else
                reak;
    }

    if(p>        plci->command = 0;
  _A_Btate           */
           (1,dprintf("rplci->appl:_internal_command (dword Id       if(!rplci || !relatedPLCIvalue)
ECUTE;

                rplci->vswitchstate=0;
          {
              pE;

                rplci->vswitchstate=0;
                rplci->v>vswitchstate=0;
            {
                rplci->internal_comB_    p{
                  dbug(1, dprintf("use 2nd PLCIE;

                rplci->vswitchstate=0;
   1, dprintf("use 2nd P>vswitchstate=0;
                plci->vsprot=0;
>ptyState:%x",rplci->pty          }
              else
              {
                rplc>vsprotdialect=0;

              }
              else iternal_ONF_ADD)
              {
                 rplci->vsprot=0;
           }
            else
            {  
             */
              {
             rplci->number = Number;
 nd = ECT_REQ_PEND;
                   {
            */
              {
                cai[0] = 2;
             SSreq-3);
      ->Sig.Id;
                dbug(1,dprin      else
 ONF_ADD)
              {
                rplci->inaelse
              else
       rplci = plci;
                }
      (rplci,S_SERVICE,0);
             cai[0] = 1;
              }
        nd = ECT_REQ_PEND;
                cai[1] = ECT_EXrplci,S_SERVICE,0);
              send_req(rplci); !relatedPLCIvalue)
  false;
            }
            else
            {
  MIX       add_p(rplci,CAI,cai);
              sig_req(rplci,S_SERVIMIX         send_req(rplci);
              return false;
    arms->l    break;

          case S_CAbug(0,dprintf("Wrong line"));
              Info = 0x3010;     arms->length,"wbwss",ss_parm   */
              break;
      bug(1,dprintf("ND)
   intf("SSreq:% add_p(rplci,CAI,cai);
              sig_req(              }
              else
              {
                rp>vsprot=0;
                rpl_WRONG_IDEN  {
      b           break;
            }                plc
              }
              else
              { !relatedPLCIvalue)lci->internal_command = PTY_RE   {
               2 dbug(1,dprintf("format wrong"));
                Info = _WRONG_MESSAGE_FORMAT;
                break;
              }
         }
            else
            {
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if(!msg[1].length)
            {
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if (!plci)
            {
              Info = _WRONG_IDENTIFIER;
              break;
            }
            relatedPLCIvalue = GET_DWORD(ss_parms[2].info);
            relatedPLCIvalue &= 0x0000FFFF;
            dbug(1,dprintf("PTY/ECT/addCONF,relPLCI=%lx",relatedPLCIvalue));
            /* controller starts with 0 up to (max_adapter - 1) */
            if (((relatedPLCIvalue & 0x7f) == 0)
             || (MapController ((byte)(relatedPLCIvalue & 0x7f)) == 0)
             || (MapController ((byte)(relatedPLCIvalue & 0x7f)) > max_adapter))
            {
              if(SSreq==S_3PTY_END)
              {
                dbug(1, dprintf("wrong Controller use 2nd PLCI=PLCI"));
                rplci = plci;
              }
              else
              {
                Info = 0x3010;                    /* wrong state           */
           b       {
                if(relatedadapter->    cai[2] = (byte)GET_WORD(&(ss_parms[4    relatedadapter = &adapter[MapController ((bi->internal_command       }
              }
            if (!plci)
            {
rplci,CPN,ss_parms[6].info);
            sigeq(rplci,S_SERVICE,0);
        n        Service */
            add_pbug, Id, --------->ncci_ch[ncci] = ch;
    a->ch_ncci[ch] = (bytapter->max_plci;i++)
      (1,dprintf("rplci->appl:% 0x7f))-1];
f (a->ncci_plci[ncci] =apter[i].pl           if(!rplci || !relatedPLCIvalue)
                 case S_CCBS_DEACTIVATE:
          case S_CCBS_INTERROGATE:
           /* reuse unT:
          case S_CCBS_DEACTIVATE:
          case S_CCBS_INTERROGATE:
            switch(ms->info[1],(word)parms->length,"wbd",ss_parms))
                {
      1, dprintf("use 2nd PLCI=PLCformat wrong"));
                  Info = _WRONG_MESSAGE_FORMAT;
                }
    rintf("format wrong"));
                  Info = _WRONG_MESSAGE_FORMAT;
             cai[0] = 1;
       if(api_parse(&parms->info[1],(word)parms->length,"wbdw",ss_parms))
                {ms->info[1],(word)parms->length,"wbd",ss_parms))
                    rp2
    if (a->ncci_pmmand = ECT_REQ_PEND;
                cai[1] = ECT_EXECUTE;

       llCREnable  = truevswitchstate=0;
                rplci->vsprot=0;
     s_parms))
                {
                      /* reuse unused screens_parms))
                {
                  dbug(0,dprin             }
                {
                rpINTERROGATE:
       CONF_ADD)
              {
                rplci->internal_command =s_parms))
                {
          cai[1] = CONF_ADD;
              }     Info = _WRONG_MESSAGE_FORMAT;
              cai[0] = 1;
                         break;
            }

            if(Info) bf(api_parse(&parms->info[1],(word)parms->length,"wbdwws",stf("explicit invocation"));
          T_WORD(&(ss_parms[4].i*/
              {
                cai[0] =d_p(rplci,CAI,cai);
  ation"));
                cai[0] = 1;
                        Info = _WRONG_MESSAGE_FORMAT;
           b  break;
            }
            if (!plci)
            {
ION_REQ_PEND; /* move switch(SSreq)
              {
      = NULL  case S_INTERROGATE_DIVERSION/
/*----));
            ])); /* Function */
        use cai with S_SERVICE below */
                  crms->l     case S_CCBS_INTERRO = 1           rplci->internal_command = INTERR_DIVERSIength,"wbdws",ss_parms))
                {
                  dbug(0,dprk;
                case S_CALL_FORWARDING_STOP:
                 break;
            }
                break;
            }

              rplci->if(api_parse(&parms->info[1],(word)parms->length,"wbdwws",s/* moP    Inf                  cai[1] = 0x60|(byteET_WORD(&(ss_reak;

      T:
                  cai[1] =use cai with S_SERVICE below */
                   = 1!= 0)
   case S_CCBS_INTERROn        mmand =  break;
            default:
            if(api_parse(&p CCBS_REQUEST_REQ_PEND;
             CTIVATE;
 CONF_AD  rplci = plci;
                }
           TIVATE_REQ_PEND;
           DENTIFIER;
              break;
            }
            /* reuse unused screening indicator */
            ss_parms[3].info[3] = (byte)GET_WORD(&(ss_parms[2].info[0]));
            plci->command = 0;
            plci->internal_command = CD_REQ_PEND;
            appl->CDEnable = true;
            cai[0] = 1;
            cai[1] = CALL_DEFLECTION;
ak;
    check_mainl,
                "s",            PLCI   *plci, byte Rc);
static void select_b_command (dword Id, PLCI   *plci, byte Rc);
static void fax_connect_ack_command (dwo with thPLCI   *plci, byttroller' b_edata_ack_command word Id, PLCI   *plci, byte Rc);
statp;
    c, k;
  dword d;
    char *plci = &a->plci[i-1];
      plci-rd Id, PLCI   *plci, byte Rc);
            Info = _OUT_OF_PLCI;
              break;
          DI_PRS_CALL_FORWARDING_STOP:
 --------------->= MAX_DATA_ACK))
        {
          dbug(0,dprintf( with this program; if not)(&((byt");
     MapCont word dir = 0;
  byte   MESSAGE_FORMAT;
    AGE_FORMAT;
*msg)
{
   dbug(1,dp
   id s == plci->ctlr = 0x3      Info = _WR  if(p>l,"\x06\x43\x61\x70\x69\x32>,cai);
           d >>= 4;
   e ret;

  dbug(1------     {
              d  +nfo;
  byte ret;

  dbug(    _parms[2].info);     <lci,ES       case S_INTERROGATE_NUMBERS:
     LINECT_R,                  _ok(byte   *cpn,DIVA_CAPI_ADAPTER   *,word);

/*
----------k;
     , 
      {
   *ALLOWED_IN_THIS
          
         ) {

    PLCIvS_CCB  return _ */
   g[1].length)
   {
                  a      }
            if(!msg[1].length)
       if(!(       > max_].lengt(plci) p      *)(&((byte;
      CE,0);
 6 *.byte);
swith CPN,                add_p(   if(p>l        ar *p;
    c = _WRONG_MESSAGE_FORMAT;
case S_MWI_ACTIVATE:
            if(api>   send_req(rplci);
 _req(long)(Transmitfo);
                break;
         invalid secondNTERRO%08l, PLCI   *                   "dddss",        listen_req},T;
  T_WORD(&(ss_parms[4].info[0])        }

          _p(rplci,CAI,cai);
     {
          send_req(rplci);
  Info = _WRONG_MESSAGE_FORMAT;
         pl->S_Han    e = GET_DWORD(&(s    LCI   *plci,     ]));
            switch(      || (MapContro  {
            case S_INTERROGATE_NUMBERS:
     LI peer in, CAPI_essary      Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if(!plci)
  ); /* Basic Service */
            add_p(rplci,CAI,cai);
    se S_CCBS_REQUEST:
              case S_CCBS_DEA      || (MapController  dbug(0,dpri = 0x3pending   if(!msg[1].len~ }
            r !mmand       }
erver Adapters.
 *
  Eicon File Rev   else
            18 */
  l_commanp->info[3];
      else LinkLayer = 0;
      if(ai->length)
      {                   Info ];
      else LinkLayer = 0;
      if(ai->length)
               add_p(rplci,CAI,"\x01\x80");
           {on samse Srl       Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if(!plci)
            {                               
 atic b            break;
        ;
            dummy.inf    LCI;
   0";
        SET_B3t_group_in       k_bit (PLCI ATE;
            /*    info[0]))); /* Messarms[4].info);
                break;
      c_plci,NCR_FA     can   {misend_req(rc_pWRONG_MESSAGE_FORMAT;
              break;
      >Id-1] & 0x200)WORD(&(ss_parms[4].info[0])); /* Basic Service */
            add_p(rplci,CAI,cai);
     with thGET_WO   PUT_WORD(&cai[2],GET_2WORD(&(ss_parms[3].info[0])));
                add_p(rplci,CAI,cai);
                break;
            case S_CCBS_INTERROGATE:
                cai[0] = 3;
                PUT_WORD(&cai[2],GET_WORD(&(ss_parms[3].info[0])));
                add_p(rplci,CAI,cai);
                add_p(rplci,OAD,ss_parms[4].info);
                break;
            default:
            cai[0] = 2;
            cai[2] = (byte)GET_WORD(&(ss_parms[4].info[0])reak;
          _p(rplci,CAI,cai);
            add_p(rplci,OAD,ss_parms[5].info);
                break;
            }
                        
            sig_req(rplci,S_SERVICE,0);
            send_req(rplci);
            return false;
            break;

          case S_MWI_ACTIVATE:
            if(api_parse(&parms->info[1],(word)parms->length,"wbwdwwwssss",ss_parms))
            {
              dbug(1,dprintf("format wrong"));
              Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if(!plci)
            {                               
              if((i=get_plci(a)))
              {
                rplci = &a->plci[i-1];
                rplci->appl = appl;
                rplci->cr_enquiry=true;
      p;
    char buf[40];

              Info = _OUT_OF_PLCI;
                break;
      (byte  _b           add_p(rplci,CAI,"\x01\x80");
                add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
                sig_req(rplci,ASSIGN,DSIG_ID);
                {
              dbug(1,dprintf("fo          }
            else
            {
              rplci = plci;
              rplci->cr_enquiry=false;
            }

            rplci->command = 0;
            rplci->internal_command = MWI_ACTIVATE_REQ_PEND;
            rplci->appl = appl;
            rplci->number = Number;

            cai[0] = 13;
            cai[1] = ACTIVATION_MWI; /* Function */
            PUT_WORD(&cai[2],GET_WORD(&(ss_parms[2].info[0]))); /* Basic Service */
            PUT_DWORD(&cai[4],GET_DWORD(&(ss_parms[3].info[0]))); /* Number of Messages */
            PUT_WORD(&cai[8],GET_WORD(&(ss_parms[4].info[0]))); /* Message Status */
            PUT_WORD(&cai[10],GET_WORD(&(ss_parms[5].info[0]))); /* Message Reference */
            PUT_WORD(&cai[12],GET_WORD(&(ss_parms[6].info[0]))); /* Invocation Mode */
             {
              dbug(1,dprintf("fos[7].info); /* Receiving Use-----------byte);
static void nl_req_ncci(PLCI   *, byte, byte);
static void send_req(PLCI   *);
static void send_data(PLCI   *);
statprintf("con,         ;
                   Info = _WRONcheck(PLCI  RD(&PI_PARSec_request (Id, Num

  Ier, a, plci, appl, msg));
participant_ms->le[ */
ELECTOR_V42BIS:
      default    case SELAGE_FOT_SUPPORTED;
           & xdi suppo,     "\3   Infi->c_ind
       *);
star) */
        ,CAI,cai);
          mmand = SUSPEND_REQ;
              sig_req(pl   **, byte   **, byte   *, byte *);
static byte getChannel(API_PARSE *);
static vo =or) */
  }

  ----------  }, /* 20 */dd_p(rplc byte ie_compare(byte   *, byte *i_parse(&ai->info[1],(word)afind_cip(DIVA_CAPI_ADAPTER   *, byte   *, byte   *);
static word CPN_filter_ok(byte   *cpn,DIVA_CAPI_ADAPTER   *,word);

/*
  XON protocol helpers
  */
static void channel_flow_control_remove (PLCI   * plci);
static _PARNumber, x_off (PLCI   * plci, byte ch, byte flag);
static void channel_x_on (PLCI   * plci, byte ch);
static void channel_request_xon (PLCI   * plci, byte ch);
stati case S_ECT:
  I_ADAPTER *a,
			 P  rc_pl-----------------        },(DIVA_CAPI_ANumber, a PLCI   *++)
     */
  }

  db *plci, APPe Free S     || (plci->State == INC_----------------LIded_xon (PLCI   *RVICESI_ADAPTER   *plci,,&msg        _reqprintfOLD_LI_SPlse if(parms->lengthNDING) || (plc
    1   InfoLCI   * pl || (plci->Su1plci->saved->State == OUTG_DIS_PENDI4G)
 s progarms[2]);
   no release aftatedPLCIvalue &= 0x0000FFFF;
            dbug(1,, API_PA[i] = command_fd_bug++;
 ter[i].Ik;
 ut);
static void at_b3_req contradicts originate/answer direction */
      if ( CCBS_REL.Id
       ||ternal_>B3_prot != B3_T90NL) && (plci->B3_prot != B3_ISO8208) && (plci->B3_prot != B3_X2      ci->NL.Id
       ||      else
 S_prot != B------MIX>B3_prot != B3_T90NL) && (plci->B3_prot != B3_ISO8208) && (plci->B3_prot !=    }
          Id
       || ROSS           _prot != B3_T90NL) &----Date == OUTG_DIS_PENDI6], CTOR_SU_SERVL_DIR_ANSWER) && !(plci->call_dir & CALL_DIR_FORCE_OUTG_NL))))))
     d Id, PLCI     SSreq = GET_WORD(&(E_), __LINE__));

  plci->internal_comman0;
}


/*--------------ength)LinkLayer = bp->info[3];
      else LinkLayer = 0;
      if(ai->length)
      {----------------------*/
/* api= bp->info[3i->appl = appl;
      byt
  if(!           "w",Info);
      ;    
            }

       LERT)
    {
      clear_c_ind_ddd_p(plci,CHI,p_ncci_ring_list)
        plci->   else     Infi->SuppState));
=7)
      ?)
     Id-1] & 0: {
            nl_re       for (i = 0; id=%d or no NL.Id=0x%x, dir10%d s / it 9) no r] || ncpi->info[3])
         any           esource, word new_b1_facilitine layer 3 connecti2         dd_d(plci,2,pvc)i->Su2adapter_fea      or B2 protocol not any LAPD
              n */ASYMMETRIC_SEL))
        2    OOP>B3_prot != B_D_BIT;X            add_d(3_T90NL) && (plci->B3_prot != B3_ISO8208) && (plci->B3_prot != B3_X25_DCE))
        && ((plc2i->channels != 0)
   _D_BIT;REMOTEi->channels != 0)
         || (((plci->B2_prot != B2_SDLC) && (plci->B2_prot != B2_LAPD) && (plci->B2_prot != B2_2  && ((plci->call& !plci->nl_rem&& ((plci->call_dir & CALL_DIR_ANSWER) && !(plci->call_dir & CALL_DIR_FORCE_OLAPD) && _prot==5)
      {
     PCrd)(ncpi->length-3),&ncpi->info[4]);
          }
        }
      }
      else if Id,
              Ncontrol_b dbug(1,dprintf("B3 already connected=%d or no NL.Id=0x%x, dir=%d sstate=0x%x"h>2)
        {
          /* check for PVC */
          if(ncpi|| ncpi->info[3])
          {
            pvc[0] = ncpi->info[3];
            p "\x09\x00\x00\,
                       plci->channels,plci->NL.Id,plci->call_dir,plci->SuppState));
        Info = _WRONG_STATE;
        sendf(appl,                                                        
              _CONNECT_B3_R|CONFIRM,
              Id,
              Number,
              "w",Info);
        return false;
      }
      plci->requested_options_conn = 0;

      req = N_CONNECT;
      ncpi = &parms[0];
      if(plci->B3_prot==2 || plci->B3_prot==3)
     || ncpi->info[3])
         8{
            pvc[0] = ncpi->info[3];
       22x0001) != ((word)byte)d; /* Conf;
    }
    e
              s(plci, LLC_CAPI_ADA    dbug= 8er;
            rplssign unsuccessf|=ll
         or B3 prot ((word)(((T30_flow_contro |= T30_CONT *);

sta |= T30_CONTROL_BI, "ddbits, 

      cd) (((Id) & 0xffffff00L) | UnMapController ((byte)(Id)))

void   sendf(APPL   *, word, dword, word, byte *, ...);
void   * TransmitBufferSet(APPL   * appl, dword ref);
void   * TransmitBufferGet(APPL   * appl, vo,OAD,ss_pa
      or no );


      caplci);
stadd_modeif(api_parse(LL_ALERTinfo[5]);
              ADAPTER   *a, PLCI.h"
#in");
              si            add_p(rne layer 3 connecti9       req = N_RESET;
          send_r_BIT_REQUEST_POLLING | T30_Cany            }
          else
          {
     NTROLAPD
          rplc.h"
#           {
       data_b3_res},
 Id, word   *, APPL   *,  *);_ADAPTER *a,
		d VoiceChard Number, DIVA         parms))
     upporI *plNDING) || (plc\x02\x   *plci, SE *msg)
{
  womat wrong"));
                rpl
          T_WORD(&(ss_parms;
static bORMAT;
    cai);
            --------8      ers that are se  casse {
        partic void mixer_   dbug(1,dprintf("INCLa              }
   *plci,                              plci->comma               p0 */
  {g[1].l-----              ca     {
                  plcivalue & i->SuppState=     {
              d = =0; iGET_DWO< PRIVATE_FAX>header.len& plci->State && ((plci->SuppS"));
                rplesource, word new_b1_facilitiassign unsuccessfu= ~x0004) /* Request to send / poll another document */
               && (a->manufacturer_fe_bits, & MANUFACTURER_FEATURE_FAX_MORE_DOCUMENTS))
              {
                fax_control_bits |= T30_CONTROL_BIT_MORE_DOCUMENTS;
              }
              if (ncpi->length >= 6)
              {
                w = GET_NFO   *)(plci->fax_connect_info_buf  if (((byt    INTERROGATE:
          _D_BIT;ws",ss_parms))
       ata_format)
                {
                  ((T30_INFO   *)(plci->fax_cn
           and already conneted
             or B2 protocol not any S_RX_DM((T30_INFO   *)(plci->60].info);

  ifprofile.private_options & (1L << PRIVATE_FAX_SUB_SEP_PWD))
                 && (GET_WORD(&ncpi->info[5]) & 0x8000)) /* Private SEP/SUB/PWD enable */
                {
                  plci->requested_options_conn |= (1L << P   } /* end of switc    and connec sendf(app->requested_op       add_p(plci,CA (GET_WORD(&nprogram is dis]) & 0x4000)) /* Private be useful_bits |= T30_CONTROL if(!t_info_buffer       plci->com_DIR_FORCE_OUB/PWD  sendf(app_format = (byte_SEL_POLLING;+>requested_op_BIT_REQUEST_POLLINGect_info_bu - =%d 8\x90"                           0; i < w; i+& 0xLAPD
          oll another document t_info_buffer))->[1_p(r   } /* end of switc        " {
    Networ |= T30_CONTROL_BIT-i];
                  ), "_bits, fax  default:
    remove_started = false;
sta) | UnMapController ((byte)(Id)))

void   sendf(APPL   *, word, dwordeak;
              }
            }
            if(parms->         ((T30_INFO   *)(plci->fax_co, dword ref);
void    = 20;
        tic void mixer_y_plci;


stati another document */
  default:
     plci);
ING)   {
                   plci->fax_connacturer_features ];
               = offsetof(T30_INFO, station_id) + 20;
                    w = fax_parms[5].length;
                    if (w > 20)
                      w = 20;
                    plci->fax_connect_info_buffer[len++] = (byte) w;
                    for (i = 0; i < w; WORD(&ncpi->info[5]);
    ];
                 if (((byte) w) != ((T30_I"wwwwsss", fax_parms))
                 if ((ADAPTER   *a, PLCI             for (i = 0; i < w; i++)
 o_change = true;
   ; i++)eject))
       -SEL_POLLING;< 7= offsetof(T30_INFO, station_id) + 20;
                    w     d, wo                               if (w > 20)
                      w = 20;
                    plci->fax_connect_info_buffer[
           ax_connect_info_buffer[len++] = fax_parms[6E_FAX/
            ad                }
                if ((a->man_p  *)(plci->fax_rintf("format wrong"));
   0;
  return 0;
}


/*---------                w = fax_parms[4].cpi->info[5]) & 0x4000)) /* Private its &= ~(T30_CONTROLilities enable */
                {
 s(error from add_b23;
    }
   RONG_MESSAGE_FORMAT;
                  else
       h);
          for CCBS_DEACTIVATE;
                          case   }
             OF_P  *plc plci->fax_connectptions_conn |= (1L << PRIVATE_FAX_NONSTANDARD);
                }
                fax_control_bits len++] = fax_pal_bits |= T30_CONTROL_B*)(plci             plci->fax_connect_in +S:
      default:
     1+i];
   ]) - Info = _WRONGi] = fax_parms[4].info[ data_b3_req}dication_x------------     }
 0; i < w; i+x09\x00\x00\ void reset_bits_low))
                 if (x_conneAX_SUbits_low))
         7 _OUT_OF_PLC|= (1L << PRIVATE_FAX_NO   In     {
              d = GET_DWOms[2].info);     
        rplci,OAD,ssi->SuppState=rplci,CAI,cai);
          {
    remove_           plci->command = 0;
              cai[APPL   * appl, void   * p);andard facilities enable */
                  SKIP->ncci_c                plci->request_parms[6]ons_conn |= (1L << PRIVATE_FAX_NONSTANDARD);
                }
                fax_control_bits ==3)
      {
        ifonnect_info_buffer[len++(char   *             plci->requested_o(T30_CONTROL_BIT_ACCEPT_SUBADDRESS | T30_CONTROL_BIT_ACC break;
    case 'd':
 a3\x91\xe0\x01",      /* c      && (msg[0].length for codec support */
      if(Id & EXT_ if(plci-  }
     );

  if (a) listen_r.commct for ETSI c                      "ws",           facility_req},
  {_FACIL   "s",    SE* bp_parms);
i_parse(&ai->infoq},
  {_RESET_Bmmand (dword Id, PLCI   *plci, by else  
      {
   lci->State == INCnfo = _WRbyte   *msg, word length);


statreq(plci,SUSP= &parms[5];
      ch = 0 */
             OF_PLCI;
     rol_bits |= T30_CONTROL_BIT_RE      InfLING;
              if ((w & 0x0004) /* Request to send / poll another document */
               && (a->manufacturer_fetures & MANUFACTURER_FEATURE_FAX_MORE_DOCUMENTS))
              {
                fax_control_bits |= T30_CONTROL_BIT_MORE_DOCUMENTS;
              }
              if (ncpi->length >= 6)
              {
                w = GET_WORD(&ncpi->info[5]);
                if (((byte) w) != ((T30_I)
                {
                  ((T30_INFO   *)(plci->fax_connect_info_buffer))->data_format = (byte) w;
                  fax_infinfo[5]);
                if (((= true;
                }

                if ((a->man_profile.private_options & (1L << PRIVATE_FAX_SUB_SEP_PWD))
                 && (GET_WORD(&ncpi->info[5]) & 0x8000)) /* Private SEP/SUB/PWD enable */
                {
                  plci->requested_options_conn |= (1L << PRIVATE_FAX_SUB_SEP_PWD);
                }
                if ((a->man_profile.private_options & (1L << PRIVATE_FAX_NONSTANDARD))
                 && (GET_WORD(&ncpi->info[5] |= (1                       {
                if (fax_featu        {
             }
      
                  plci->requested_options_conn |= (1L << PRIVATE_FAX_NONSTANDARD);
                }
                fax_control_bits &= ~(T30_CONTROL_BIT_ACCEPT_SUBADDRESS | T30_CONTROL_BIT_ACCEPT_SEL_POLLING |
                  T30_CONTROL_BIT_ACCEPT_PASSWORD);
                if ((plci->requested_options_conn | plci->reqested_options | a->requested_options_table[appl->Id-1])
                  & ((1L << PRIVATE_FAX_SUB_SEP_PWD) | (1L << PRIVATE_FAX_NONSTANDARD)))
                {
                  if (api_parse (&ncpi->info[1], ncpi->length, "w{
                    if ((plci->requested_options_conn | plci->requested_options | a->requested_options_table[appl->Id-1])
                      & (1L << PRIVATE_FAX_SUB_SEP_PWD))
      {
                    fax_control_bits |= T30_CONTROL_BIT_ACCEPT_SUBADDRESS | T30_CONTROL_BIT_ACCEPT_PASSWORD;
                    if (fax_control_bits & T30_CONTROL_BIT_ACCEPT_POLLING)
                      fax_control_bits |= T30_CONTROL_BIT_ACCEPT_SEL_POLLING;
      }
    
                      w = 20;
             1+i];
   T30_INFO   *)(plci->fax_connect_info_buffer))->station_id_len = (byte) w;
                    for (i = 0; i < w; i++)
                      ((T30_INFO   *)(plci->fax_connect_info_buffer))->station_id[i] = fax_parms            ];
                    ((T30_INFO   *)(plci->fax_connect_info_buffer))->head_line_len = 0;
                    len = offsetof(T30_INFO, station_id) + 20;
                    w = fax_parms[5].length;
                    if (w > 20)
                      w = 20;
                    plci->fax_connect_info_buffer[len++] = (byte) w;
                    for (i = 0; i < w; i++)
                      plci->fax_connect_info_buffer[len++] = fax_parms[5].info[1+i];
       
                 x_parms[6].length;
                    if (w > 20)
                      w = 20;
                    plci->fax_connect_info_buffer[len++] = (byte) w;
                    for (i = 0; i < w; i++)
                      plci->fax_connect_info_buffer[len++] = fax_parms[6].info[1+i];
                    if ((plci->requested_options_conn -1])
                      & (1L << PRIVATE_FAX_NONSTANDARD))
      {
                      if (api_parse (&ncpi->info[1], ncpi->length, "wwwwssss", fax_parms))
        {
                        dbug(1,dprintf("non-standard facilities info missing or wrong format"));
                        plci->fax_connect_info_buffer[len++] = 0;
        }
                      else
                      {
          if ((fax_parms[7].length >= 3) && (fax_parms[7].info[1] >= 2))
            plci->nsf_control_bits = GET_WORD(&fax_parms[CK;
      ncpi = &p= &parms[1];
      if ((plci->B3_prot == 4) || (plci->B3_prot == 5) || (plci-           }
                }
                else
                {
                  len = offsetof(T30_INFO, universal_6);
                }
                fax_info_change = true;

              }
              if (fax_control_bits != GET_WORD(&((T30_INFO onnect_inffax_connect_info_buffer)->control_bits_low))
              {
                PUT_WORD (&((T30_INFO   *)plci->fax_connect_info_buffer)->control_bits_low, fax_control_bits);
                fax_info_change = true;
              }
            }
            if (Info == GOOD)
            {
              plci->fax_connect_info_length = len;
              if (fax_info_change)
              {
                if (fax_feature_bits & T30_FEATURE_BIT_MORE_DOCUMENTS)
                {
                  start_internal_command (Id, plci, fax_connect_info_command);
                  return false;
                }
                else
                {
                  start_internal_command (Id, plci, fax_adjust_b23_command);
                  return false;
                }
              }
            }
          }
          else  Info = _WRONG_STATE;
        }
        else  Info = _WRONG_STATE;
      }

      else if (plci->B3_prot == B3_RTP)
      {
        plci->internal_req_buffer[0] = ncpi->length + 1;
        plci->internal_req_buffer[1] = UDATA_REQUEST_RTP_RECONFIGURE;
        for (w = 0; w < ncpi->length; w++)
          plci->internal_req_buffer[2+w] = ncpiak;
            }
 i = &a->plcDWORD(&(ss__command (d_DWORD(&(ss_parms[2].info[0]));
             plci->fax_conne    rplci = plci;
    ncci_mapping_bug++;
   T:
            case S_CCBS_DEACTIVATE:
                cai;
      plci                   /* wrong state           */
    *plci, byte Rc);
steak;
              }
            }
            if(parms5];
      ch = 0;
   && plci->Stat           plci->requested_options_conn |= (1L <        cai[0] = 3;
                PUT_WORD(&cai[2],GET_WORD(&(ss_parms[3].info[0])[0])));
                add_p(rplci,CAI,cai);
                add_p(rplci,OAD,ss_parmsPL   *, CAPI_MSG   *);
static word api_parse(byte     default:
            && ((plci->B3_prot == B3_TRANSPARENT)
        || (plci->B3_prot == B3_T30)
        || (ax_control_bits);
                fax_info_change = true;
              }
                        if (Info == GOOD)
            {
              ci->fax_connect_info_length = len;
              if (fax_inriver. Allo it i     {
                if (fax_feature_bits & T30_FEATURE_BIT_MORE_DOCUMENTS)
                        }
                else
                {
                  len = offsetof(T30_INFO, unesource, word new_b        }
                else
                {
                start_internal_command (Id, plci, fax_adjustalse;
                }
              }
            }
          }
          else  Info = _WRONG_ 1;
        plci->internal_req_buffer[1] = UDATA_REQUEST_RTP_RECONFIGURE;
        for (w = 0; w < ncpi->length; w++)
          plci->internal_req_bufferISCONNECT_I|RES1], ncpi->length, "wwwwssss", fax_parVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, byte);
static void CodecIdCheck(DIVA_CAPI_Alci->State == INC_, PLCI   *plci, b API_SAVE   *out);
static voTY_R,                         "ws",           facility_req},
  {_FACILITY        plci->internal_req_buffer[0] = ncpi->leng        facility_res},
  {_CONNE      else
            {
               "s",            tf("connect        && (((CAPI_MSG   * && xdi suppor1 */
  { "\x02\x88\x90",         "\x02\x88      else
            {
   This program is distributed in the hope that it will 9','a','b','c','d','e','fSSstruct[6], MASK_TERMINAL_PORTABILITY);
           rms[4].inf      Info = _ci))
        _buffer[len++] =   byte ret;

  dbug(ernal_comatic b.lenre_bits & T30_FEATU  cai[1] = CONF_ISOLATEadd_ai(plci, unsuccessfull
         or B3 protocol all   * plci, byte b1_CONNECT_I, Id }
        *plci, byte   *msg, word------------        req                plci->NG)
LI_INDi->info[1+w            }
  void reset_adapter_fea                plci->any  B3 US00\xNITIAord command (dword Id, PLCI   *plci, byte Rc);
static byt------------ w;
         _PAPER_FORMATS)
       && ((plcIVATE;
  appl)
== 4) || (plci->B3_prot ==dd_b23 2)"));
            for (i =0x0001&, OK)  {
         ASKdication (dword Id, PLCI   *plci, {
        if(ncpi->lengt
    else
    {
      if ((a->manufacturer_features & MANUFACTUREonnect_infoAX_PAPER_FORMATS)
       && ((plci->B3_prot == 4) || (plci->B3_prot ==))->station_iart_internal_command (Id, plci, fax_disconnect_command);
                         NTRO= INC_DIS_PENDING))
      {
        ncci_free_receive_buffers (plci, ncci);

        nl_req_ncci(plci,N_EDATA,(byte)ncci);

        plci->adapter->ncci_state[ncci] = IDLE;
        start_internal_command (Id, plci, fax_disconnect_command);
        return 1;
      }   ""                     }, /* 11 */
  { "",              ol_bitci->internal_req_buffer[0] = ncpi->length +               dbug(1,dpri dbug(1,dCILITY_I,
                _NONSTANDARD);
                         
         byte ret;

  dbug(1x200)
    0;
     DISCONNECT_I, Id       caii,CAI,"\x01\x86], MASK_TERMINAL_PORTABILITY);
             if (a->ncci_plcinc_dis_ncci_table[MAX__plci[ncc    connect_b3_t90_a_res},
  {_SELECT_B_REQ,                       "s", < 8; ms->length));
  Info = _WRONG_IDENTsPLCIt->par        && (((CAPI_MSG  b3_req.Data));

        }

        j = (j + + sizeof(plci->msg_in_qurer_features&MANUFACTURER_FEATURE_XONOFF_FLOW_CONTRO   */        lci->appl,
                _ZE - j;
I|RESPONSE,            "dyte)16plci,ASSIGN  ch = GET_WORD(ai   }, /] |     ""ax_af DIVA S(pos)
    
     2])rom a16ta->Flags & 0x0004)
3     {

        ck for delivery        }, /+4on */
     5if (data->Flags & 0x0004)
6     {
        i = ncci_ptr->7ata_ack_out + ncciplci,Bnding;
  8on */
     9if (data->Flags & 0x0004)
10     {
        i = ncci_ptr->1   p_ack_out + nc;
   
      2on */
      3if (dat      ----ble[i    }
    dbug(1,NUMBeak;
S*, APPLngth==7)
            {
            add_p(plcia->manuf----1 -           es4]);
        add)))
      return false;
  dprintf_parms))
parms[5],2,0);    /* no resource    */ word new_bs[0].info)) < ((byte   *)(plcii,OAD,&pp,OSA,&parms[4]);
       ch = GET_WORD(a  {

        TransmitBk_pendici_ptr->data_ack_pe  {

    umber = d.Number               {
        if(plci->ord) a->Id) << ---------------- */
    );
static byte manufac         18 */
  SuppState==CALL_HELD)
            {
              if(Idci->fax_coTRIEVE_REQ;
              if(plci->spoofed_msg==Number, DIVA_CAPI_ADAPTER *a,
			PLCI *plci, APPL *4mber = Numbe(*",GET_WORD(parms[2].info),Info);))     else
         pl->S_Handl_PARSE   *out);

word apnexe   *msg, word length);


stS_CALL_FOSTATE;
        }
        else  Inf)(plci->msg_in_queue)) + sizeof(plci->msg_toconnect_b3_t90_a_res},
  {_SELECT_B_REQ,              dword *)parms[0].info);
      data->Length = GET_WORD(parms[1].to);
      data->Handle = GET_WORD(parms[2].info);
      data->Flags = G|= APPL_FLAG_PRIV_EC_Splci->S word  Rc);
sULL;
    }
  }
  dword plci_b_id)
{
  /*
 *
  Copywrite_pos;

 con Networks, 200 =*
  C->li_on Networks, 2002.  if ((( is supplied forreadce fi>con Networks, 200) ?e is supplied fore of DIVA:
    LI_PLCI_B_QUEUE_ENTRIES +  Eicon File Revision :  ) -*
  This source fi- 1 < 1)
  t (c  dbug (1, dprintf ("[%06lx] %s,%d: LI request overrun", the   (d/*
 )icon NetId << 8) | UnMapController con Netadaptere Fo))by
  the char   *)(FILE_), __LINE__))with  return (falseogram}
 *
  Tsupplied forqueue[on Networks, 200]ile is opyri | *
  This pDISC_FLAGwith
  This source filecon Nhis source fil= *
  This program is fre-1 *
 0 :R INCLUDING ANY
  + 1EVER INCsupplied for the use UT ANY WAR the use withs distritrud in}


static void mixer_remove ( Thir ve
  Cght (cDIVA_CAPI_ADAPTERr veawithNU Genernotifylied withFree *
  Copyriwith/*
 *i, j2.
 *terms of the GNU General Public Li copy of the by
  t Free Software Foundation; either version 2, or (at your option)
  a later version.
 *
  This progr
  aile is suat yourEVER INCLUDid impliede Foundation; either version 2, or (at your optiwith
  Ea->profile.Global_Options & GL This INTERCONNECT_SUPPORTEDunder the 
  Eihe GNU Gebchannel#inc!= 0unde   && (li_config_table[a*/
/* ase + 2, or (
/* This is opt- 1)].
  C MERal Pu suppor the   i =  that are server by    */
/* XDI drivergram i-------for all adaptersi].curchnl | to save it separate his is) & *
 CHANNEL_INVOLV------necessary to  for (j = 0; j <y adtotal_acrose s; j++local memeaning     essary to save it separateflaadaptersj]efinedATSO-----------------------   ||d for all adaptersj-----*/
statii dword diva_xdi_extended__features = ------------   if not, wr =y adapter    */
/j. Alloot neces-----------_SDRAM_BAR  != NULL_features = 0orted _CANCEL  0x0000al Pubdefine DIVA_CAPI_XDI_PROVIDE->appl00000004
#define DIVA_CAPI!*
  CAPI can reque reque_---*----APPL diva_OLD_LI_SPEC     0x00000008

/*
  CAPI can rState     0x00000008

/
  CAPI can rNL.Idrn co
  CAPI can rnl of theyrighefine DIVA_CI_XDI_PROVIDES MA 0213 if notsource of thedI_XDI_PROVIDE,*
  CopyrigAPI_XDI_PROVIDhe h_FEATURE_OK_FC_LAB_OK_FC_LAB copy clearr all ampliedACTURER_FEAxtendedalculate_coefs (aA_CAPI_XDI_PROVID if notupdatemplied,  should     (diva_xdito save it separateBAR  0x0004------------*/
/* This is opt    -----he h}d hav/*-
/*--------------------------------------------------------------*/
/* Echo cancesion facilitiesXONOFF_FLOW_COAPI_ADAPTER   * a, PLCI   ---*/
/*------------------------------------------------------------------ave received aecmore detarameters GNU General Public /*
 *w      byte    *plci,_buffer[6]ation, Inc., 675 Mass Ave, Cambridge, Mbit (PLCI   *plci, .
 *
 */





#include "platform.h"
#include "di_defs.h"
#include "pc.h"
#include "capi20.h"
#include "divacaask_bit (PLCI   *0OUT 5EVER sk_bit (PLCI   *1OUT DSP_CTRL_SET_LEC_PARAMETERShis pUT_WORD (&RSE *);
static vo2]res & ->ec_idi_o-----------tic void api_load_ms &= ~I_PAREg(APCOEFFICIENT, bytwclude "divec_tail_length MER0 *
 128PARTICU;
void api_remov byte *format, API_SAVE   *out);4], w-----add_p--------FTY,ARSE *);
static v-----sig_req--------TELave_m, 0_ADAPTend  * a);
stuld have received aeced_features   &NU General Publi, word b);
void AutomaticLaw(DIVA_CAPI_Ad_features  d CapiRelease(word);
word CapiRegister(word);
word api_put(APPL   *, CAPI_MSG   *);
static word api_parse(byteSAVE   *in, API_PARS= I_PAENABLE_ECHO_CANCELLER |2.1
 *EC_MANUALIND  *);NTY LCI   *);
NONThisAR_PROCESSINOEVER INC;
void api_remove_    r (DIVA_CAPI_ADAPTEprepare_switch*/




 Id, program;;

void   callback(ENTITY   *);

static void conbyte);
static  by
  t eithId (Id),include "capi20.h"
#include "diva have recei/*
 *ec_savNCELes   &id SendSSExtInd(APPL  ,_ind_mRcoid   callback(ENTITY   *);

static void conl_ind(PLCI  %02x %did VSwitchReqInd(PLCI   *plci, dword Id, byte   , Rctatic voadjust_b_e ree"divacas distriGOODuld have receic void nrestornd(PLCI   *);

static byte connect_req(dword,te testInfoci, word b);
void AutomaticLaw(DIVA_CAPI_Aect_a_res(dwordI_PARSE *);
static byte connect_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL  L    = PARSwith
  E);
staB1_oup_optimiz& B1_FACILITY_di supr the tatic voAPTER   *, PLCI   *,  necessary cre sADJUST_Bt);
TOR;
st_1 2.1
 te);
staintern----omman    APTER   *, PLCI *);
stot necessary);
staTER  * ACTURER_aning     APTER   *, PLCI   *, =PI_ADAPTER   *, PLCI  API_XDI_PRbreak----------------bit (PLCI   *plci, wo DIVA_CAPI_XDL   *, API_PARSE *);
static byte info_res(d2-------- DIVA_CAPI_A_CAPI_ADAPTER   *, PLCI 2 *, APPL
  EiRc0000OK)rted byte fac_FCfine DIVAaning     terms of the GNU General Public LiRct_a_r EC failed DIVA by
  the;
static byte connect_res(dword, word, DIVA_CAPI_rogram i *, d, word_WRONG_STATEord, word, DIVA_CAPI_ADAPTER    DIVA_CAPI_         *, API_L   _bar (DIVA_CAPI_ADAPTER*);
sta,word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPAPI_PARSE *);
st,PL   *,oup_ind_mresult[8lci, word b);
void AutomaticLaw(DIVA_CAPI_ASE *);
sI_PARS04DIVA_CAPI_ADAE *);
static byte connect_res(dword, word, DIVA_CAPI_ADAPTER _CAPI_ADAPTER   *   *,);
staticcmdtatic void api_load_mstatic void d api_removes(dword, word, DIVA_CAPI_ADAPTEif:
  protocol code supports PRIVLCI  xdi supr the    *, A *, ACAPI_ADe *format,    *, A1], PLCIUCid S in the helse, APPL   *, API_PARSEPI_PAstatic byte data_b3_req(IVA_CAPI_ADAogram is  *, A3ARSE *);
static byte data_b3_iva_PARSE *API_ADAPI_PARSE *);
static byteAPI_PARSE *);
st(API_SAVE API_PARSE *);
stati     
static byte lte data, APPL A_CAPLCI   *);
OPERATION 2.1PARSE *)FREEZEord api_removset_b3_res(dRESUM word, DIVA_C_UPDATEPI_ADAPTER   *,
word api_remov 2.1
 tatic voAPI_PARSE *);
std, word, DIVdefault *, APPL  *, PLC1eq(d__a__nd(Ptatic ,on pr, (ree SoADAPTER   *, PLCI   dInfo(IVA_ APPL   *, API_q(dwoCOMMAND_ not necPARSE *)I   *, APE *);
static  *, PLCIprocess_PARSE *);
sRc)0000PARSErd, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_Load);
static facility_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI  APPL   *, API_PARSEPL   *, ANO-----------ord, word, DIVA_CAPI_ADAPTER   IVA_CAPI_As(dword, word, DIVA_CAam is distL   *, API_PARSE *);
sSE *);
staticCAPI_ADAPTER   *, PLCI   *, APPL   *,API_PARSE *);
statid add_s(PLCIPPL   *, A   *);
stati (diva_xdic void add_ss(PLCI   * plci, byte co3--------*, PLCI   *, APPL   *, API_PARSE *   *);
static void add_s(PLC3E *);
static byte facility_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_Enpter);
static byte facility_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);

static word get_plci(DIVA_CAPI_ADAPTER   *);
std, DIVA_CAPI_ADe);
statiet_b3_res(dword, bic byte reset_3_t90_a_res(dword, word, DIVA_CAPI_ADAPTER   *, PLC API_PARSE *);
static byte maADAPTER   *, PLCI   *, APPL   *, API_PAR--------------i, byte code, API_PARSE *URE_XONOFF_FLOc void add_ss(PLCI   * plci, byte coword, word,ARSE * p);
stati (diva_xdi_ec void add_ss(PLCI   * plci, byte code, API_PAR*, PLCI   *, APPL   *, API_PARSE *ARSE * p);
static void aRc  fac
static void add_s(PLCI   * plci, b byte facility_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_Dis;
static word add_b23(PLCI   *, API_PARSE *);
static word add_modem_b23 (PLCI  * plci, API_PARSE* bp_parms);
static void sig_req(PLCI   *, byte, byte)  *, APPL   *, API_PARSE *);
static byte select_b_req(dword, wo&OK_FC_LAB~API_ADAPTER   *, PLCI   *, A3PL   *, API_PARSE *);
satic void addnufacturer_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
staticUnlbyte manufacturer_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);

static word get_plci(DIVA_CAPI_ADAPTER   *);
static void add_p(PLCI   *, byte, byte   *);
statie);
static void nl_req_nccAPI_AD_forb3_res(dword,,E* bp_parmsR | CONFIRM, __)- 0xffffLtatic bynumberord, D"wws"*, PLC,3_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   * ?APPL   *, SELECTOR
static void Sen: id VoiceChannelOff(PLCI,es(dworuld have receiind_m_req( as pu *);

stati/*
 *N);
sta License
  along with ttic byte connect supith tvoid e
  PARSE *msgCI   *, APPL   *,te testoptI   *,word get_barmsarms[3]I   *, APPL   *, A1plci, word b);
void AutomaticLaw(DIVA_CAPI_Aci, worid VSwitchReqInd(PLCI   *plci, dword Id, byte   **parord, word, DIVA_C*, API_PARSE     
  E!



maner_r


/*privCANCAPI_PARSE (1Lundaic vAT;
static void Se)fine r the terms of the GNU General Public LiFup_optyL(__ supportcturer_res(tchReqInd(PLCI   *plci, dword Id, byte   **pa, API_PARSE* bp_parms);
static void siVA_CAPI_ADAPTER  _xon rd, word, DIVA_CAPI_ADAPTER   *, PLCI   *, Acessary to lci, itatise (&msg[1].info_req(Rc);
st_remov, "w",;
static word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_Wrong message    maies);
sts(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
sMESSAGE_FORMAT_CAPI_ADAPTER   *PI_ADAeck(DIVA_CAPI_ADAPTER  MER0004
#define D------------ Rc);
static void fax_connect_ack_comman ThiLCI   *plci,  *bp_msg, word b1_facilities, word internal_command  *, API_PARSE *);
sIDENTIFIERnel(API_PARSE *);
stnnec_facil DIVA_CAPI_0;

word Id__a__)||PLCI   *tures&MANUFACTURER_FEe Rc);
static void fax_adjust_b23_command (dword Id,   *,I   *plci, byte Rc);
static void fax_disconnect_command (dword Id, PLCI   *plci, byttatic byte connold_save_commaic byte AddInfo(byte   **, _CAPI_ADAPTER  E *);
static vPI_ADAord,E*format,
static b0staticACTURER_FEATU_SAVE   *in, API_PARSE   (PLCI   *, dword, byte   *
static byte conneACTURER_FEATU*, API_PARSE *);
stvoid datic byte data_b3_req(dword, word, DII_PROVIDES_N select_b_com >= 4ine DIVA_CAPI_XDI_PROVIDESoptd Number, DIV&A_CAPI_ADAPTER  [2]ACTURER_FEATUREplci, APPL   *appl, API_PARSE  *, byte);
static void Sendord, DIVA_Cbchann   * *, byt2100HZ_DETVoicevoid dtmfQUIRI   *plciREVERSALdword Id, PLCI1_faciliwrit&  *, byte, bNON Thistic void Sendfine DIVA_CAPt (dword Id, in, API_PARS|d(PLCI   *);
e);
static void SendSetuci, byte   *ms);
static  byte, byte, bTONE     0x00000008 others);
static void mixer_command   *plci, byte  API_XDI_PROVIDES_Nci);
static Oatic 
static xer_clear_d Id, word Number, DIVA_CAPI_ADAPTER   *a, P
static void mixer_clearAPI_XDI_PROVIDES_Ng, word length);
6ACTURER_FEATURE_XONOFF_FLOW_COpInfo(APPL   *, PLCI   (PLCI   *plci);


static voi4 mixer_set_bchan_OK_FC_LABEL) &&     (   *, PLCI   *, APPL   *, e_restore_command (dPARSE *);
static byte reset_lci, byte others);
static void E   *outword, word, DIVA_CAAPI_XDI_PROVIDstart_API_PARSE *);
staPARSE *);
s(dword, woACTURER_FEATUREs distributed in);


static void ec byte, byte);
static vord Number, DIVA_CAPI_ADAPTER d(PLCI   *);
static void SendInfo(et_bchannel_id   *, dword, byte   * *, byte);
static void Sendmixer_set_bchannel_id);

word api_remove_st Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void ec_indication word, word, DIVA_CAPI_Alci, byte others);
static void mixer_crequest (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void ec_indication  *, PLCI   *, APPL   *, API_PPLCI   *plci, byte Rc);
static byte ec_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void ec_indication 
static byte connect_b3_dword Id, PLCI   *plci);
static void mrd Id, PLCI   *plci, byte Rc);


static int  diva_get_dma_descriptor  (PLCI   *plci, dword   *dma_magic);
static voiPTER   *, PLCI  add_b1(PLCI   *, API_PARSE *, word, wordC unknownnse as puVA_CI   *plci, bytestatic byte connect_res(dword, word, DIVA_CAPtic byte dataACTURER_FEATUREatic byte data_b3_req(dwoUN---------ic byte reACTURER_FEATU) &&     (diva_xdiid nl_id nl_nnect_inf Id, PLCI   *plci, byte Rc);
static void select_b_command Setword Id, PLCI   *plci, byte Rc);
static void fax_connect_ack_command (dword Id, PLCI   *plci, byte Rc);
static void fax_edata_ack_command (dword Id, PLCI   *plci, byte Rc);
static void fax_connect_info_command (dwordumber, DIVA_CAPI_ADAPTER    MEREC_umbe_line_timeSERVICESeve_restore_command (d*, API_PARSE1static byte gatic byte data_b3_req(dwostarted = false;
staticord Id, PLCI   *plc, wor8atic void dtmf_indication (dwo PLCI   *, AP void dtmf_indication (dwo6], 0x0007n)(dword, woratic byte data_b3_8],(PLCI  Xrted = falsTAIL_LENGTH                       "dwww", 10_B3_n)(dword, wold_save_command (d Id, PLCI   *plci, byte Rc);
static void fax_adjust_b23_command (dword Id, PLCI   *plci, byte Rc);
static void fax_disconnect_command (dword Id, PLCI   *plci, byte Rc);
static void hold_save_command (dword Id, PLCI   *plci, byte Rc);
static void retrieve_restore_command (dword Id, PLCI   *plci, byte Rc);
static void init_b1_config (PLCI   *plci);
static void clear_b1_config (PLCI   *plci);

static void dtmf_command (dword Id, PLCI   *plci, byte Rc);
static byte dtmf_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void dtmf_confirmation (dword Id, PLCI   *plci);
s, APPL  {
  word command;
  byte * tic byte data_b3_red, word, DIVA_CAPIde, API_PAR*, PLCI   *, APPL   *, API_PARSE *);
} ftnel_id_esc (PLCI   *plci, byte bchannel_id);
static void mixer_set_bchanl_id (PLCI   *plci, byte   *chi);
static void mixer_clear_config (PLCIpInfo(APPL   *, PLCI   *, ci, byte   *ms
static bord length);
2tatic void dtmf_parameter_write (PLCI   *plci);


st
static vo_config (PLCI   *pl);
staticommand (dw_notify_update (PLC   *plci, byte others);
static void mixer_command (dword Id, PLCI   *plci, byte Rc);
static byte mixer_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void mixer_indication_coefs_set (dword Id, PLCI   *plci);
static void mixer_indication_xconnect_from (dword Id,SPONSE,       "",     static void drd length);
static void mixer_indication_xconnect_to (dword I
static 3 *plci, byte   *msg, word length);
static void mixer_remove (PLCI   *plci);


static void ec_command (dword Id, PLCI   *plci, byte Rc);
static byte ec_request (dword Id, word Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_PARSE *msg);
static void ec_indication (dword Id, PLCI   *plci, byte   *msg, word length);


static void rtp_connect_b3_req_command (dword Id, PLCI   *plci, byte Rc);
static void rtp_connect_b3_res_command (dword Id, PLCI   *plci, byte Rc);


static int  diva_get_dma_descriptor  (PLCI   *plci, dword   *dma_magic);
static voi  * appl, dword ref);
void   * TransmitBufferGet(APPL   * appl, void   * p);
void TransmitBufferFree(APPL   * appl, void   * p);
void   * ReceiveBufferGet(APPL   * appl, int Num);

intiva_* bp_parms xdiIFIC_FUNCe reatic worduffer);


/*----------------------------- *, byte);
 void CodecIdCheck(DIVA_CAPI_ADAPTER   *, P
statictic void SetVoiceChPLCI   *, byte   *, DIVA_CAPI_ADAPTER   * );
static void VoiceChannelOff(PLCI   *plci);
static void adv_voice_writRSE *msg);
static r (DIVA_CAPI_ADAPTEindica----static byte connect_b3_res(dwor b1_fammand);E *);
sI   **, APPL   *, API_PARSE *);
static byte connect_b3_a_res(dw, /* 12 */es);
static void adjust_b1_facilities (PLCI   *plci, by *msg)ci, byte Rc);
static b(PLCI   *, dword, bte Rc);
sta_b3_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, A----------   *plci);
static voiatic byte data_b3_req(         dtatic voRc);
sDAPTER   *, PLCIA_CAP   *equest (dYP worNTIGNUOUS*plci, fine UnMapatic byte data_b3_req(dwoBYPASS_DU (dw  "\x0\x88\x90"   n)(dword, w DIVA_CAPI_AD\x02\x88\x90",        xer_clEDx90"         }, /* 19 */
  { "\x02\x88\x90",         "\x02\x88\x90"       }, /* 20 */
  { "\x02\x88\x90",       RELEASED      }, /* 19 */
  { "\x02\x88\x90",        "\x02   }, /* 20 */
  { "\x02-----------------------------q},
  {_FACILITY_R,  * 19 */
  { "\x02\x88\x90",     INDICar *buffer);

s(dword, word, DIVA4\x88\xc0\xc6\xe6" }, /*"\x02\x88\x90"         }, /* 18 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 19 */
  { "\x02\iva_x90",         "\x02\x88\x90"         }, /* 20 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 21 */
  { "\x02\x       }  /* 28 */
};2\x88\x90"         }, /* 22 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 23 *       }  /* 28\x90",         "\x02\x88\x90"         }, /*byte);
static void CodecIdCheII_ADAPTER   *, P0ns      hannel(PLCI   *, byte   *, DIVA_CAPI_ADAPTER   * );
stat                 ""                     }, /* 11 */
  { "",                 */
/*------------------------------------------------------------------*/
Advc vodivedc""  
  "",                           /* 13 */
  "",  plci);
static void set_group_ind_mask (PLCI   *plci);
static void clear_gre received aadv_* 12 more deEL))

/c byte connect test_       rd, DIVlic License
  along with this  DIVA_    ind_m*pc voi test_, n, j, PPL  ind_mch_map[MIXERd here hS_BRIlci, w4",     oefPLCI   *ADV_VOIC word,_BUFFER_SIZE + 2lci, word b);
void AutomaticLaw(DIVA_CAP\x81",               /   *, PLC/





#include "platform.h"
#include "di_defs.h"
#include "pc.h"
#include "capi20.h"
#include ,\x84",         divacapi.h"
#include "mdm_ms =            
  See t*(p++) api_save_msthistati    /* d api_remove_st  0xCT_B3while (i + sizeofbyte s <sk it\x81",                 --------e *format,p,_xconnect_t------------------/* 26 */
irGet(APPp +ILITY_I|RiH 1
#defiAPTE         <   /* 20 */this
  "\COUNT * /* 28 */
};
-----------------------0x800        TH 1
#define V120_HEADER  }, /*  that prirted                       comp---------------for all adapters that are . Allo it 0004
rted for all adapters that are ser1l funct0000004
t necessary to                       word, worREAK_BIT | V120_HEADER_C1_BIT | V1     T_B3_ACT       /* 21 */
  "\x02\x91\xb2",           set* This is opt   *, PLC the Free Software Foundation; either version 2, or (at your option)
  any* 24 */
  "\x02\x91\xc1",      er by    */
/* XDI drGet(APP----------ADER_BREAK_BIT | V120_HEADER_C1_BIT | V0000004
ER_C2_BIT)

static byte v120_default_heade120_HEAD
  0x83                          /*  de, API_P--------------------------------------};

static byte v120_break_header[] =
{

  0xc3 | V120_HEADER_BREAK_BIT  /*  Ext, BR , res, res, C2 , C1 , B  , F   */

};


/*------------------------------------------------------------------*/
/* API_PUT function    PL   IT      0x08
#define V120_HEADER_FLUSH_ions suppted for all adapters that are server by    */
/* XDI driver. Allo it is not ne0\xa2" ask it from every adapter*/
/* and it is not nectatic vox84",           word, DIVA_CAPI_/* 20 */WRITE_ACT Id,rd Id, PLCI     that are ser    /*ICd here haBAS1", y adapter*/
/* and it is not neceskter )
  {
    dbug(1,dprintf("invalid ctr2bute it--*/
/* API_PUT fot necessary 15 */

R   *, PLCI   *, APPL   *, A    /word, DIVA_CAPI_ADAdefine DIVA_CAPI_USE_CMA         mixed diva_IVA_ERENCbyte d diva_MIXT_B3_ACTIV----------------------*/
static d->header.plci));
    plci = &a->pONI *appl, API  *);
staticties);ufacturer_feaci->\x90   *FACTURER_FEADING_SLAV worDdi supportabled)
  {
    dbug(1,dpriktf("plci=%x",msg->header.plci));
    plci = &a->plci[msg->header.plci-1];
    ncci = GET_WOkD(&msg->header.ncci);
    if (plci->Id
     && (pERT)
      || (msg->header.commaRD(&msg->header.ncci);
   || ((ncci < MAX_NCCI+1) _USE_CMA      mmand == (_DISCONNECT_B3----------------PROVIDES_NO_CANCEL))

/*---------to save it separate for eve0x00000002
#define* Macrose    if (j + msg->header.ljngth + MSG_IN_OVERHEAD <= MSG_IN_QUEUE_SIZE)
 l
      || (plci->State == INC_CON_PENDING)
      || (plci->State == INERT)
      || (msg->hth + MSG_IN_OVERHEAD <= MS.len_QUEUE_SIZE)
 req_ncci(Ppter - 1) */
  if ( conDEtroller >= max_ada                */
/*-------------------------abled)
  {
    dbug(1,dpri------*/
static dNECT_B3_ACTIVdefine DIVA_CAPI_USE_CMA         NECT_B3_ACT_OK_FC_L
  a = &adapter[controller];
  plci = NULL adapter*/
/* and it is not necesN_QUEUE_SIZE - n + 1;
        i -= j;
      }

      if (i <= ((msg->hea&& (a->ncci_plci[IN_OVERHEAD + 3) & 0xfffc))

      {
      k dbug(0,dprintf("Q-FUl
      || (plci->State == INC_CON_PENDING)
      || (plci->State == INC_CON_ALER
  a = &adapter[controller];
  plci = NULL;
  if ((msg->header.plci != 0) &                  */
/*---------------------------------------ead_pos, plci->msg_in_wrap_pos, i));

          return _QUEUE_FULL;
      }
      c = false;----------------  if (j >= i)
      {
        if (j;
static void nl_emove_check(PLCI   *);
static void lisa->ada90\x90\xa2"  art(CT_B3_ACTeade1) */
  ifNEW/
  "\xlid ct/* 28 */
};

/*-----------------------------        re------------------*/

#define V120_HE;
        }
        ncci != 0) && (mN_OVERHEAD <= MSG_IN_QUEUEefined here haTX_DATA3_I|RESPONw(&ms    /*      ||   *);
write_p->msg_in_read_pos;
        while (k != plci->msg_in_Rrite_pos)
        {
          if (k == plcI_MSG  T_B3_ACT03\x91\xe(ind_)t_group_r.command == _DAT(w >> 8->header.length, plci->ms/* 28 *      )     1
#DAPTER   *, PLCI         , i))&& (((Cj ctrl"));
    return _BAD_MSGn)(dword, wi))
    +id ap  {
        ;
  if ((msg->header.plci-----------------     n      nT  0RRAY1\xb1( copy orks, 2rog_bri); n;
      }

      if (msg->header.cont       ;
          }

     [n].to_chte add__adapter )
  {
    dblci->msg_in_queue))[k]))->headefromlength +
     _read_pos;
        while (k != plZE - j;
        else
     efined here have only local meore_command (dcommand ==ad_pos;
        while    
static dwog_in_queue))[k]))->heademaskOR A x8 PAR0x0 not neces      reing=%d/%d",
                         0xf) ^ead_pos;
        while             >> 4rGet(APPL   *      if (plci->req_in || plci->in^= (w    ncci_ptr->data_pending, n, nc<< 4tmf_command (dword Id, PLCI   *plci, byte Rc)command ==;
        }
        ncci_ptr = &(a->ncc+     -------------------------   /* 8 * flag);-------*/

#define V120   /* 20 */e)) + sizeof(plci->msg_in_queue)]->data
        if (msg->header.-----------------------     (msg         return _QUEUE_; IT  ------------------------; i------------03\x91\xeQ-FULL3(requeue)"));

   ite add_ (msg->hnnect= (byte)(
        if (plci->req_in || plci->internal_command)
          c = true;
      else
        {
          plci->commaAPTE             *, A(p -msg(0x%04x,0ibutword,tended_adapter_fea len=%d writADAPTER  * a);
static void diva_ask_for_xdi_sdram_bar (DIVA_CAPI_ADA-----------  *, IDI_SYNC_REQ  *);

void c License
  along with thiware
  Foundati          /* 21 */
  "\x02\x91\xb2",           trol_rc(PLCI   *, byte, byte, byte, byte, byte);
static void data_rc(PLCI   *, byte);
static void data_ack(PLCI   *, bytepi.h"
#include "mdm_m---------*/tel----1) */
  i------ Id, PLCa->AdvSignal Thi  (V120_HEA------------------------ retur("DATA_B3r) ||
      msg->header.length > MAX_MSG_SIZEorted for all adapters that are server by    */
/* XDI driver. Allo it is not necessary to ask it from every adapter*/
/* and it is not neces + msg->header.length + MSG_ICT_B3_ACTd_pos;
        while (k != L   *   *)(&((byte   *)(plci->mscol coL   *   *)(N_QUEUE_SIZE - n + 1;
        i -= j;
      }

      if (i <= ((msg->header.length + MSG_IN_OVERHEAD + 3) & 0xfffc))

      {
        dbug(0,dprin        {
          if ((((byte   *der.command));

  for(j=0;j<MG_IN            dbug(0,dprintf("Q-FU     if (plci->req_in || plci-msg->head120_HEH_PC    NTY OF
  "\Print    urn _BAD_ter )
  {
    dbug(1,dprintf("invalid ctrl"));
    return _BAD_MSG;
  }
  ) & 0xfffc;

        *((APPL   *   *)(&((byte   *)(plci->msg_in_queue))[j])) = appl;
        plci->msg_in_write_pos = j + MSG_IN_OVERHEAD;
        return 0;
      }
    }
    else
    {
      plci = NULL;
    }
  }
  dbug(1,dprintf("com=%x",msg->header.command));

  for(j=0;j<MAX_MSG_PARMS+1;j++) msg_parms[j].length = 0;
  for(i=0, ret = _BAD_MSG; i < ARRAY_l
      || (plci->State == INC_CON_PENDING)
      || (plci->State == INC_CON_ALERreak loop if the message is correct, othersizeof(plci->msg_in_queue)))
      ) & 0xfffc;

        *((APPL   *   *)()(&((byte   *)(plci->msg_in_queue))[j])))) = appl;
        plci->msg_in_write_po  {
        if (plci->msg_in_write_pos != plci->msg_in_read_pos)
          c =der.length + MSG_IN_OVERHEAD +
  dbug(1,dprintf("com=%x",msg->header.command))));

  for(j=0;j<MAX_MSG_PARMS+1;j++) msg_parms[s[j].length = 0;
  for(i=0, ret = _BAD_MSG; i ------------------         mand,
          msg->hbyte);
static void SendSSExtInd(APPL   *, PLCI   * plci, dword Id, byte   * * pages             */
/*--id VSwitchReqInd(PLCI   *plci, dword Id, byte   **parms);

static voi | V120_HEA_ind(PLCI   *);

static byte connect_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI     word p;

  for(i=0,pI_PARSE *);
static byte connect_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte c\x81",    ect_a_res(dword,word, DIVA_CAPI_ADAPTER   *, PLCI   * /* 17 */
  "\x02\x91\xa1",    L   *, API_PARSE *);
static byte disconnect  break;
    case 's':
  DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_res(dword, word, DIVA_C]);
        for (i = 0; i < msg-R   *, PLCI   *, APPL   *, Agth; iSIZE) {
< msg->header.length; i++)
          ((byte   *)(plci->msg_in_q
static byte listen_req(dword, word, DIVA_CAPI_ADAPTER   *, PL 20 */  *, APPL   *, API_PARSE *);
static byte info_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte info_re *p;

 I   * plci, byte ch, byte flag);
81",               /*I   *pl) */
  if ( con  *, AI_PARSE *);
static byte alert_req(dword, word, DI 20 */CAPI_ADAPTER   *, PLCI   *, APPL   *, API_    casE *);
static byte facility_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *)* 12 *(PLCI  tatic byte facility_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_b3_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, AP  /* 10 */
  "",                           /* 11 */
  "",                   B1xa5", API_tatic ing,                           /* 13 */
  "",                           /* 14 */
  "",                           /* 15 */

  "\x0ind_mb   *, PLCI         ] =write    ,  /* 0  No  This is---------ization*/-----*/

word1  Codec (automreceilaw){
  word i;
  word j;2
  if(!reA-ed) {
    
  "",     ----*/

word3
  if(!rey for(i=0;i<max_adapter;i++) {
    4  HDLCos = X.21i=0;i<max_adapter;i++) {
    5dapter[      /* 13 */
  "",     ----*/

word6  ExI_PARS Dev12 *0i<max_adapter;i++) {
    7plci[j]);
        plci;j++) {ove(&adapter[i]8dapter[56kj].Sig.Id) plci_remove(&adapter[i]9  Transe);
ntSig.Id) plci_remove(&adapter[i]10 Loopback to networ if(adaptrd i;
  word j;
1 Ts pupatI_PA[i].plc.Sig.Id) return 1;
     2 RPLCIat yo12 */
syncSig.Id) return 1;
     3;
  return 0;
}

a
/*-------rd i;
  word j;
4 R-IPI_Pfa2 */
  "",        rd i;
  word j;
5apter[128k leased lin */
  "rd i;
  word j;
6 FAX        /* 13 */
  "",      {
          7 Modem------------max_plci;j++) {
          8---*/


/*--i].plci[j].Sig.I----------------9 V.110-------i].plci[j].Sig.-------12arted =0     out->p(j=0;j, copy {
  remove_started =1  if(!rconnected[i].Iplci[j]--------carted =2(j=0;j,DTMF------------------------1earted =3RNAL_COMMAN+ copyLS; i++)
    plci->farted =4command_queue[i] +localnal_command_q3arted =5RNAL_COommand (dword Idnal_command_queue",
 6 initnternaD_LEVELS; i++)
    plci->ueue",
 7 {
      ;

  dbug (1, dprin-------2 MAX_INT8RNAL_COLEC+MMAND_LEVELS; i++-------3internal9ar   *)(FILE_),ue[i] = NULL;
  if (pvoid st30rnal_command == 0)
  d (dworUnMapId (Id), (31 RTP)(FILE_), __LINE__));
;

  if (plci->in32and_function)ue[i] = NULL;
}lci->internal_co3   i = 1;
    whiled (dword ter;i++) {
     4 e   *)----tas if(adapter[i]ter;i++) {
     5 PIAFS------------------------------  (* com6command_queudworELS; i++)
    plci->interna37, PLCI   *plci)
ue[i] = NULL
static v , dprihar   *)06lx] %s,%d: ned (dwo*/
}s, ptatic byte cgDER_   *, PLCI   * 16 */
 E *);
s----------------ght (c) Eiclci->internalg)[i]lci->internal_=---------------------internal_cote ad
  EiPL   *, API_== 9)0;

#i = 0; i < MAX_20NTERNAL_COMMAND_LEVELS5  (V120[i];
     Eicon Netci, woredust_b_prd(PLn );
stati plci->internal_cdefiss (dword Id, PMMAN(dword3_I|RESPO;

# can requesfo = NULL;at your o plci->internal_coaptersres(dword, wId-1 dwoal_command_queue[MAX_INTERyte dword Id, PLCI   ULL;
    (*(plc || (plci->State == INC_CON_PENDING)
      || OFTe[MAXSEN2(data) - plci->internal_|=CAPI_ADAPTER MMANci[msg->h  return;
    plci->internal_command_queue[0] = NULL;
  }
}


/*---------RECEIVrd Id, word---------------------------------appl, A (msg->headeAL_COMMAND_LEVEL17NTERNAL_COMMAND_LEVEL18  (V120_HEADER_turn;
    plci->internal_command_queue[(CON_PENDING)
      ||V18 |_CON_PENDING)
      ||VOWNERNAL_COM---------------------------------- |----------------------}
/*ion, Inc., 675 Mass Ave, Cambridge, M
  plci->internal_%ord,
void Tra/





#include "platform.h"
#include "di_defs.h"
#include "pc.h"
#includefar "\x02\x91\xc1",      internal_co   ncoup_optesdata-----s distrilci->internalrite_coefs (PLCI   tendlci->internal_command = 0;
  plci->internal_co02\x91\cci)
      nccwrite------g)[i]tatic vointernal_comm, API_PARSE5set_b3_re26 2.1
 eadelci->internal_& ( msg->header.lengch_ncci[ch])
  gth; i k;

  a    *6T_B3_Annect_info_cci , APPL req_ncci(PLCI  8     while7((ncci < MAX_NCCI+1) && a->ncci_ch[ncci])
          ncci++;
        if (ncci =7 MAX_NCCI+1)
        _ADAPTEreq_ncci(PLCI  9     while0     while
     whileatic  while4;
              whileng_bug++;
           3 j++;
    3 ((nc   do
        do
ng_buci < MAX_NCCI+1) && a-tic void listen_chec Id, PLCI   ++;
              } while ((LOCA4
#define Dcci 3CT_B3_ACT         AX_NCCI+1) && a->ncci_ch[ncci])
          ncci++;
        if (n(ncci =9hile ((i < MNL_CHANNEL+1)
_ADAPTE the (i < MAX_NLL_CHANNEL+1) && (j < MAX_NCCI+| a->ch_ncci[ch])
  {
   ch_ncci[ch])
  a->adapter_disted eue[i] = plci->internal_command_queue[i+1];
    plci->internal_command_queue[MAX_INTERNAL_COMMAND_LEVELS - 1] = NULL;
    (*(plci->internal_command_queue[0]))(Id, plci, OK);
    if (plci->internal_commaMAX_NCCI+1) && (a->ncci_ch[j] != i));
            }
          } _ADAPTER  < MAX_NL_CHANNEL+1) && (j < MAX_NCCI+1));
          if (i < MAX_NL_CHANNEL+1)3(j < MA_NCCI+1)
         } = MAX_Nintf("NCCI mappineturn;
    plci->internal_command_queue[0] = NULL;
  }
}


/*HARDMMANh, force_nc 15 */

I allocate/remove function                                    */
/*----3_I|RES;

#(a->ncci_ch[j] != i));
      -----h, force_ncci++;
              } while ((.length));
(1,dpr) ncci;
    }
    a->ncci_ch[ncci] = ch;
    a->ch_ncci[ch] = (byte) ncci;
    ddbug(1,dprintf("NCCI mapping established %ldX%02x %02x %02x-%02x",
      ncci_mapping_bug, ch, force_ncci, ch, ncci));
  }
  return (ncci);
}


static void ncci_free_receive_----- (PLCI  NCCI+1) && (a->ncci_ch[j] != i));
            }
          }2))
       < MAX_NL_CHANNEL+1) && (j < MAX_NCCI+1));
          if (i < MAX_NL_CHANNEL+1)
   * p, wo{
            dbude, API---------------------------ci)
  {
    if (a->ncci_plci[ncci] == plci->Id)
LITY_R,   < MAX_NL_CHANNEL+1) && (j < MAX_NCCI+1));
          if (i < MAX_NL_CHANNEL+1)
while ((i < Melse
               id nl_req_ncci(PLCI  3tatic ((app       k =3atic vopping_bug, Id));
      }
      else
      {
 ist =   * p, < MAX_NL_CHANNEL+1) && (j < MAX_NCCI+1));
          if (i < MAX_NL_CHANist =de, APICCI+1)
        3t[i])
 req_ncci(PPTER   *, PLCcci internal_coPL   *, g[p+1] + (msg[p+2]<<8);
        p +=else
    {
     d %02x ld %02x %02x %02x-%02x",
      ncci_mapping_bug, ch, force_ncci, a->ncci_ch[a->ch_ncci[ch  "\x02\x91\xc1",     x91\xancci = ch;
  }
  else
ies, b,

  plci->internal_connect_)data_b(force_ncnal_command,
        *, APPL ->internal_command =onnect_req(newPPL   *, API02\x91\ord) a-
        ncci = /*
 *->manuf] = NULL;
  whilg[p+1] + (msg[p+2]<<8);
        p +=appl;
          ncc
        (!plci->appl)
        {
          ncci_mapping_bug++;
          dbug(1,dprintf("NCCI mapping no appl %ld %word) a->Id) << 8)       for (i = 0%08lx"       for (i = 0 &, Id));
        }
        ord) a->Id) << lse
 
    }
    }
  }
}


s=tatic void cleanup_ncci_data (PLCI   *plci      axBuffer; i++)
 atic byteR   *, PLCI   *,~       for (i = 0    }, /*ci[ncci] == plci->I    } while ((j < MAX_PTER  *, IDI_SYNCdram_bai[ncci]);
    if (plci->appl)
    {
      %ld %02xr the ttmf_rncci_ptr->data_pending !tr->DBuffRSE *);
std_features   & DIVA_CAPg->heade   {
        if (!plci->data_sent || ER   *aDBuff_for_i_ptr->data_pending != 0)
      {
        if (!plci->data_sent.length));
xtended_features   & DIVA_CBufferFree (plci->appl, ncci_ptr->DBuffearms[i].in     msg->header.length,  DIVA_CAP)
  {
    ncci_ptr ==   }
    }
  }
}

       appl = plci->appl;
].P);
NC_REQ  *);

void   callback(ENTITY   *);

static void >data_ack_out    *, byte, byte, byte, byte, byte);
static void data_rc(PLCI   *, byte);
static void data_ack(PLCI   *, byte);
sta  *, PLCIect_a_r = buted
      p +=4;
      facturer_req(dwoword, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *,= ch;
  CCI+1; ncci++)NCmand = ncci_ptmdm_m1_resource);bp[              /* 21 */
  "\x02\x91\xb2",   facturer_req(dwDIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_res(dword, word, DIVA_C
static byte listen_req(dword, wAPI_PARSEI_ADAPTERSTARTcode)
     byte listen_reqatic _msg---------supported ci->adapter;
  modeode _ADAPTERMODE_SWITCH_L underce_nccieleased %ld %08lx %02x~( %02x-%02x",
  AVbyte %02x-%02x",
        ncord, DIVA %02x-%02x",
 N    SOUR  plc %02x-%02x",
    *, P) remoD  (V1->Id;
  ifi = 0; i < MA released %ld %08lx %0der.lci]] = 0;
      if (!prdbug(0,dprin PAR plci->Id)
      {       )
  {
   >Id) << 8)ci->adapter;
  )
      ncci cci_mapping_b 0; i < MAX_t;
        while DAPTER   *, PLCI  ->appl;
          nccionnect_    while ((i != 0) && (a->ncci_next[i] != plcA_CAPI_ADAPTER   *);
static void ) && (a->ncci_next[ci,  Id));
        }
        eLCI   *plci,rd, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_Ancci_ B nonplci, APIroup_optimizd %0 %02x %02x %0y_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI  _request (dword I     while ((      if ((i != 0) && (a->ncci_next[i] APPL   *, API_PARSE *);
static byte connect_b3_req(dword, wf("DATA_B3 REQ wd %ld %08lx %02x %02x-%02x",
  A--------al_commMAX_DATAbyte);
static voARSE *);g != 0)!= plci->dtf("NCCI mapping released d %08lx %02xsk_bit (Px %02x-%02x",
          ncc void ec*, byte);
static vog released %      n = 1;
   x %02x-%02x",
          ncci_maid nl_L   *, API_PARSE *);
static bytecci)  /* 27t[i])
 ord find_ci     cleanup_nc         a-code)
    {
        cleanup_ncci_data (plci, ncci);
        dbug(d, word copy 
  for(i=0,p=rd, word, DIV] != plci->nPLCI A_CAPI_ADRc);
statiadd_p(PLCI   *, byte, byte   if (i > M    {
          a->ncci_ch[ncci] = 0;
     e[MAX->ncci_plci[ncci] = 0;
          a->nove fute[ncci] = IDLE;
          a->ncci_next[ncci] = 0;
        }
      }
   >data_if (!preserve_ncci)
      plci->ncci_ring_list = 0;
  }
}


/*------------------------------------------------------------------*/
/* PLCREMOVE_L23functiA_CAPI_ADAPTER  lci->msg_iata (plci, ncci);
      dblx %02x %02x-%02x",
 
      if I mapping    (((__a__)->marer_features&MANUFACTURE3             API_PARSE *);
static byte info_req(dword, word, DIVA_CAPI_A ncci_map
  itions suppor

      if 
  if (n = &t_ncci (PLCI   *
  iue[0]))(appl,
           "s",          
  if (n->data_pendingtic byte AddInfo(byte   **, 

   sentSG *)(&;

      }DBCI   *;

      }

   out].PT_B3_ACTIVE_

   rcring_list;
   )(plci->msg_i_chn_queue))[i]))->info.data_b3_req_OK_FC_LAB.Data));

      }

   ack    i += (((CAPI_MHEAD + ter[& 0xfffc;

    }
  }
  plci->msg_in_write_pos = MSG_IN_QUEU_OK_FC_Lc voiq      [i];
  
     -----------& (((C= IDLE;
          a->ncci_next[ncci] xtended_*
  Eicon>appl,
       :a->nx02\x88\xfor_xdi_sdram_ba byte others_read_pos;
    while (i != plci->msg_CAPI_ADAPTER   *, PLplci->msg_in_read_pos;
    while (i != plci->msg_de, APIplci[ncci] = 0;
         i->Sig.Id ==ata (plci, byte facility_req(dword, word, Dssary toist == ncci)
            plci->ncci_ring_liof the tatic byte facility_redword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *,PI_PARSE *);
static byte coug(1,dprintf("DATA_B3 REQ wwrap_pos)
        i = 0;
      if (((CAPI_MSG  0)
      return_nl_busycci_ptr_PARSE * p);
static void add_ss(PLCI   * plc byte info_req(dword, word, D_plci[ncci] == plci->Id)
            a->ncci_ch[ncci] = 0;
     s(dword, wplci[ncci] = 0;
          a->nCI   *, APi] = IDLE;
          a->ncci_next[ncci] = 0;
        }
      }
   d nl_ind(PLCI   e_ncci)
      plci->ncci_ring_list = 0;
  }
}


/*------------------------------------------------------------------*/
/* PLCI remove fRSE   *infunction                                                       */
/*---------------------------------------------------------------ug, Id, pr---*/

static void plci_free_msg_in_queue (PLCI   *plci)
{
  word i;

  if (plci->appl)
  {
    i = plci->msg_in_read_pos;
    while (i != a->n(format[i])
 plci[ncci] = 0;
          a->n *p;

  p = oi] = IDLE;
          a->ncci_next[ncci] = 0;
         *, API_PARS  word p;

  for(i=0,p=e_ncci)
      plci->ncci_ring_list = 0;
  }
}


/*--------------------------------lci_free_msg_in_queue (plci);

  plci->chan      nc_in_write_pos)
    {-----------table[b >> 5] &= ~(1L << (b & 0x1f));
}

static      ncci_map 0)
      return;
de, API_PARSE * p);
static void add_ss(PLCI   * pl;
    }
  }
  ncci_remove (plci, 0, false);
  plci_e[b >> 5] &= ~(1L << ug(1,dprin0000004
#define D*plcit_edprinci)
{
  word i;

  for (i, bpntf("plciping appl expSK_DWORDS; i++&    }
  }

tocolable[i] = 0;
ind_mask_elset[i];
   p-----------ree Software wrap_pos)
        i = 0;
     _state[ncci] = 2_queucci] = 0;
 == ncci)
            plc  plci->ncci_ing_list = 0;plci->ncci_ring_list == ncci)
            plci->ncci_ring_liinvalid L1ARSE *);
s%ld %02x %02x %0ci_next[ncci];
        }
        a->ncci_next[ncci] = 0;
      }
    }
  }
  eor (ncci = 1; ncci < MAX_NCCI+1; ncci DIVA_CAPI_ADAPTER   command == _DATA_B3_R)
      {

        TransmitBufferFTER  * a);
stat if (!prS*/
  { "\x03\  dbug(1,dprintf("plci_remove(%x,tel=%x)",plci->Id,plc---------- if(plci_remove_check(plci))
  {
    return;
  }
  if (plc
static char hex_plci[ncci] = 0;
          static cha25 plci->NL.Id:%0x", plci->NL.Id));
    if (plci->NL.Id && !plci->nl_remove_id)
    {
      nltatic vtatic byte next[i] = a->ncci_nt[ncci];
        }
        a->ncci_next[ncci] = 0;
I_ADAPTER      while ((i != 0) && (a->ncci_next[i (!plci->sig_remove_id
     && (plci->Sig.Id
      ||_remove(%x,tel=%x)",plci->Id,plci-witch (format[i])
 
  {
    dbug(1,dprintf("D-byte   *p;

  p =[i].length + 1;
      break;
    }
i] = IDLE;
          a->ncci_next[ncci]   {
    cci_ch[ncci] ind_mask_bit (PLCect_a_res(dword,rd b)
{
  return ((plci->group_optimization_mask_table[b >> 5] & (1L << (b & 0x1f))) != 0);
}

/*---------------------------    }
                   */
/*--------------------------------------*/
/* transe if (i != 0)
      {
    ---------------;
  dword d < 8; k++)
          *(--p) = ' ';
      }
      *(CI   *plci)
{
  word i;

  fobug(1,dprintf ("c_ind_mask =%s", (char   *) p));
  }
}





#define dump_plcis(a)



/*-------------------------------------------------------------- = 0;
  plci->appl = NULL;
  if ((p   *, PLCI   *, CI   *, APPL   *, API_PARSE *);
----------------------------*/

static byte connect_req(dword Id, woronnect_a_res(dword,e_ncci)
      plci->ncci_ring_list = 0;
  }
}


/*------------------------------------------------------------------*/
/* PLCASSIGN>msg_in_write_pos)
    {_req(%d)",pa;
  byte m;
  static byte esc_chi[35] = {0x02,_req(%d)",;
        }
      eq)))
    {
      sig_req(plci,HANGUP,0);
      send_req(plci);
    }
  }
  ncci_remove (plci, 0, false);
  plci_byte m;
  static byte esc_chi[35] = {0x02,xtended_features ;
statiall_dir----CAL dwoRRc);efinUTG_Nrototypes*plci)
{
  word i;

  for (i = 0; i < C_IND_MASK_DWORDS; i++)
    plci->c_ind_mask_table[i] = 0;
}

static byte c_ind_mask_empty (PLCI   *plci)
{
  word i;

  i = 0;
23 while ((iic void set_c_ind_mask_bit (PLCI   *plci, word b)
{
  plci->c_ind_mask_table[b >> 5] |= (1L << 23b & 0x1f));
};
void TransmitBatic byte connect_res(dword, word, DIVA_CAPLCI  }

static byte test_c_ind_mask_bit (PLCI   *plci, word b)
{
  return ((plci->c_ind_mask* plci)
{

  if(!pl_req(%(b & 0x1f))) != 0);
}

static void dump_c_ind_mask (PLCI   *plci)
{_req(%d)",p if(plci_remove_check(plci))
  {
    return;
  }
  if (plcth)LinkLayer = bp-ord f_req(%dncci] = 0;
         th)LinkLayer25 plci->NL.Id:%0x", plci->NL.Id));
 lity_req(dwh=0xffff;    if (plci->NL.Id && !plci->nl_remove_id)
    {
      nlassigncci(plci,REMOVE,0);
      send_req(plci);
    }
  }
  else
  {
    if (!plci->sig_remove_id
     && (plci->Sig.Id
      || (plci->req_in!=plci->req_out)
      || abled)
    {
      dbug(1,dp       {
       g_req(plci,HANGUP,0);
      send_req(plci);
    }
  }
  ncci_remove (plci, 0, false);
  plci_free_m----------------------------*/

static bUS 27 *ended_featu3             apter;
  Id = (((dw sho= 0; k < 8; k++)
        {
          *(--p) = hex_digit_t--------in_write_pos)
    {        p;
  byte m;
  static byte esc_chi[35] = {0x02,      p_chi = &((ai_parms[0API_PARSE *);
static byte info_req(dword, word, DIVA_CAPI)
    {
      sig_req(p byte test_c_in* plci)
{

  if(!plNchi[0]>3(b & 0x1f))) != 0);
}

static void dump_c_ind_mask (PLCI   *plci)
{-------- if(plci_remove_check(plci))
  {
    return;
  }
  if (plci------------
          d >>= 4;
        }
           d channel = 0;
  d--------j;
       o+3);
               i],(word)ai->length,"ssss",ai_parms))
       ->ncci_ch[ncci] ist == ncci)
            plci->ncci_ring_liommand cci(plci,REMOVE,0);
      send_req(plci);
    }
  }
  else
  {
    if (!plci->sig_remove_id
     && (plci->Sig.Id
      || (plord  facind != 0)
      return;
  pl, _DISCONNECT_I, Id, 0, "w", _L1_ERROR);
aning     
  p
{

  if(!pl,dprintIdnter16) 5] &= ~(1L << (b
  iAPPL   *, APncludADAPTER   *ion;(( Free Sol = i;
             ueue)16------------------------------------E *);
s      }

           ERROR);
      reMAT;    
            }

          "NCCI mapping 0;
              }
              if (ch_mask static voidlength>=7 && ai_parms[0].length<=36)
            {
                AGE_FOs suppo                else
                    if (ch_mask == 0)
                Info = _WRONG_MESSAGE_FORMAT;    {
      if (!p             }
              if (ch_mask PI_MSG  arms[0].length == 36) || (ch_mask != ((dword)(1L << channel))
                    esc_    .length<=36)
         5) /* check length of channel ID */
                {
               remove_check(plci function for each message                  */
/*--------------------------------------------------------*/

static byte connect_req(dword Id, word Num = {0x01,0x00};
  byte noCh = 0;
  word dir = 0;
  byte   *p_chi = "";

  for(i=0;i<5;i++) ai_parms[i].length = 0;

  dbug(1,dprintf("connect=36)
        a->ncci_plci[ncci] = 0;
           Info = _WRONGd channel = 0;
  dword ch_    /*rms[0].info+3);
   h=%d",ch,dir,c   ch_mask = 0;
   h=%d",ch,dir,c    if (j ir=%x,p_ch=%d",ch,dir,c      whilir=%x,p_ch=%d",ch,dir,c          ir=%x,p_ch=%d",ch,dir,c         i ------------------------*/

static byte connect_req(dword Id, wor copy of {0x01,0x00};
  byte noCh = 0;
  word dir = 0;
  byte   *p_chi = "";

  for(i=0;i<5;i++) ai_parms[i].length = 0;

  dbug(1,dprintf("connectEN_plciA_CAPI_ADAPTEREN2\x88\x*, APPL   *, R   *, PLCI   *, APPL   *, API_  *, APPL   *, API_P*);

static byte connecte
  ], nc  *bpsk_tab_ch[ch])
        n[5],ch,s(dword, word, DIVr->data_ack_pending = 0;
  }
}


static void ncci_ring_lisext[i] = a->nct[ncci];
        }
        a->ncci_next[ncci] =    }
    }
  }
  else>c_ind_mask_ta plci->adapter;
  ug(1,dprint &parms->data_outci)
            plcinal_command_que(a->Info_Mask[appl- *);
statibyte reset_b3_req(dword, w>appl,
        msgee So     chan    for (iparmsV120_HEADER_C2_DWORDS)
      {
   a->ncci_c>ncci_plci[ncci] = 0;i_data (plci, ncci)eserve_ncci)
        if (!preserve_ncci)
    rd i, j, f)))ping applms[0].info)<29) {
          add_p(plci,BC,cip_bc[GET_WORDh_ncci[a->nerve_ncci)
      {
   ) no release after ,'3','4','5','6','ci_dite tst == ncci)
            plci->ncci_ring_l-----------ext[i] ...[6]);
        add_s(plci,LLC,&parms[7]);
        add_s(plci,HLC,&parms[8]);
        CIP = 0;
    ncci_ptr->data_ackId = (((tatic byte connect_b3_res(dword, word, DIVA_CAPI_ADAPTER            {
            if ((appl->DataNCCI[i] =) {
     d, DIVA_C *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_b3_req(dword, w      },*, API_PARSE *);
static byte reset_b3_req(dword, word, DIVA_CAPI_ADAPTER   *, PLCIs(dword, word, DIVA_t_grPTER   *, PLC;
static byte dtmf_requoCh = truelci)intions suppocheck length of channel ID */
         esc_chi[2] t[i])
   rrect for ETSI ch 17..31 */
                add_p(i == plci->m.Id:%0x", plci->NL.Id));
    if (plci->NL.Id && !plci->nl_remove_id)
    {
      nlen be u    (plci,REMOVE,0);
      send_req(plci);
    }
  }
  else
  {
    if (!plci      {
          *(-info);
     n prototypInfo_Mask[appl->Id-1] & 0x20)
  {
    ncci_ptr           if(noCh) ad;
            plci->internade, API release after a disc */
          add_p(pi,HLC,cip_hlc[GET_WORD(parms[0].info)]61\x70\x69\9\x32\x30");
        sig_req(plci,ASSIGN,DS>NL.Id && !plci->nl_remove_id)
    {
      nl_rt_a_r used for  *bp_msg, word b1_facilities, word internal_commaA_CAPI_ADAPTER   *, PLnfo[1],(wornufacturer_req(dword, word, DIVA_CAPI_ADAPTER(plci->NL.Id && !plci->nl_remove_id)
    {
      nl_r = (((nufacturer_res(dwbp_msg, word b1_facilities, word internal_command        esc_chi[add_p(PLCI   *, byte, by DIVA_CAPI_*, APPL   * have received areADER_3RSE *);
static byte connect_b3_res(dword, word, DIVALCI   * plci,if(!Info) {
          if(!(plci->tel && !plci->adv_nl))CI *plci, APPL *aASSIGN,0);
        }
      }

      if(!Info)
      {
        if(ch==0 || ch==2 || ch==3 || d, word, DIVA_CAoCh || ch==4)
        {
          if(plci->spoofed_msg==SPOOFING_REQUIRED)
          {
            api_save_msg(parms, "wsssssssss", &plci->sa    add_s(plci,DSA,&parms[3]);
          if(noCh) add_p(plci,ESC,"\x02\x18\xfd");  /* D-channel, no B-L3 */
       nMapCoB3byte *);
static );
          if(!dir)sig_req(plci,CALL_REQ,0);
          else
          {
        lci->msgi,UID,"\x06\x43\x_req(%d)",i,UID,"\x06\x43\x      plci->command = PERM_LIST_REQ;
            plci->appl = appl;
            sig_req(plRI *p B3;
            send_req(plci);
            return false;
        );

  if(ai->lengt 2.1
 ind_mask_facturer_req(dword, word, DIVci->saved__ind_mask_bit (PLCth; i++)
              {
                i
     ONFIRM,
        Id,
        Number,
        "w",Info);
  return 2;ci->Sig.Id
      || (plci->readd_p(PLCI   *, byte, by   *);
stati*, APPL   */*",                    );

  if(ck(DIVA_CAPI_AD PLCI   *);
staand (dLCI   -----byte)
static voidNN_NR, &paI,Id,0,"s",""if(!Info && ch!=2 &selec-L3 */
     << 8) | a->Id;
  if (!preserve_ncci)
    ncci_free_ect;
  static byte cau_= ch;
 esc_chibyte        {
            if ((appl->DataN   sig_req(plci, ic byte esc_t[] = {0x03,0x08,0x00,0x00};
  API_PARSE * ai;
    API_PARSE ai_parms[5];
  word ch=0;

  if(!plci) {
    dbug(1,dprintf("connect_res(no plci)"));
    return 0;  /* no plci, no send */
  }

  dbug(1,dprintf("connect_res(State=0x%x)",plci->State));
  for(i=0;i<5;i++mpty (P---*DS; ici->saved_= NULL;
  return false;
}

static voic;

    }
  }
 byte   *)(plci->msi->command = PERM_L == plci->Id))
  {
    ncci_ptr =       ncci++;
       }
    }
  }
      if(noCh) add_p(plci,ESC,"\x02\x18\xfd");  /ci, || Reject>9) 
        {annel, no B-L3 */
       id Voi     ->length)
  {
    if(!api_parse(&ai->info[1],(word)ai->lei, byte cod        .API_ADAPT_remove_compchi = &((ai_parms[0].info)[5 {
          add_p(plci,BC,cip_bc[GET_WORDbug(1,dprintf("ai_parms[0].h_ncci[a->ncci_ch[h[ncci]] = 0;
      if (!prf))) != 0);
-----------------eq(plci,REJECT,0);
        }
        plci->appl = appl;
      }
      else 
      {
        sendf(appl, _DISCONNECT_I,length=%d/0x%x",ai_parms[0]           pi,UID,"\x06\x43\x61\x70\x69\-------------------------------*/
/* PLCI   plci->appl = appl;
            sig_req(plS  sig B I   *plc;
            send_req(plci);
            return false;
            esc_t[2] = cau  }
    }
  }

  if(plci->State==INC_CON_CONNECTED_ALERT)
  {
    dbug(1,dprintf("Connected Alert Call_Res"));
   plci->tel == ADV_a->Info_Mask[appl->Id-1] & 0x200)
    {
    /* early B3 connect (CIP mask bit 9) no release after a disc */
      add_p(plci,LLI,"\xoCh = true>header.length; icci_ch[ncci] ate==INCrd newx0de, API_Pate==INCid ap0x1ncci_statete==INC2OUT ANY ->b--------tf("plciSetV 12 Cove_stat_ncci (PLCI   *Adv if(! Thi,  Info =ffc;

    }
  }turn 2;
}

st*, APPL   *, byte);
static void C    esc_tREQms[2]);
    add_s(plci, LLC, &parms[4]);
ADAPTER *a,
			PLfaxommansig_MSG_ APPL *appl, API_PARSE *parms)
{
  word i, Info;
  word Reject;
  static byte cau_t[] = {0,0,0x90,0x91,0xac,0x9d,0x86,0nl_req_ncci(plci, ASSIGNic byte esc_t[] = {0x03,0x08,0x00,0x00};
  API_PARSE * ai;
    API_PARSE ai_parms[5];
  word ch=0;

  if(!plci) {
    dbug(1,dprintf("connect_res(no plci)"));
    return 0;  /* no plci, no send */
  }

  dbug(1,dprintf("connect_res(State=0x%x)",plcA_CAPFAX         ACKRSE *);
static b1,dprintf("adapter disabled")check length of channel ID */
   Id-1] & 0x200)
        {
 add_d(PLCI   *, wor      {
     release after a disc */
          add_p(plarms[9]);
   NDataDAPTPSC,"\x02\nl_req_ncci(aticne V120  add_s(plci, LLC, &paLe   *) m[4]);
        add_ai(pld plci_remd_s(plci,L.Xe = INC_C, LLCreq(plci, CALL_ReqCONNECT_B3_A  for(i=0; ie = INC_C* plcid == _DATA    }
   00)

        else  (*(plci->inte_empty (PNLogram is distsk[appl->Id-1] & 0x200)
        {;
  dword d;
    char *p;
    char buf[40];

  for (i = 0; i < C_IND_MASK_DWORDS; i +=----issueDIVAnnectACKcci(plci,REMOVE,0);
      send_req(plci);
    }
  }
  else
  {
    if (!plci->d, DIVA_CAPI_ADAPTEplci,ESC,esncpi     si& NCPI_VALI V12------B3ntroi].info  15 */

static byte disconeq(dword Id, w)) <efine 0\xa2"     }, /*B3CI       static vodd_ai(plci, &parmseq(dword Id, wIVE  plci->State = I;
      else
  
  word i;

  dbug(1,dprintf("disconnect_reqS",, DIVA_CAPIA_CAPI_ADAP i++) {
static byte|=R *a,
			   PLCI *plci, ACAPI_ADAPTER *a,
			PLnl_reos = MSG_, ASSIGN, 0);
      }

      if(plci->spoofed_msg==SPOOFING_REQUIRED)
      {
        api_save_msg(parms, "wsssss", &plci->appl->Id-1));
    plci->spoofed_msg = CALL_RES;
        plci->internal_command = BLOCK_PLCI;
        plci->command = 0;
        dbug(1,dprintf("Spoof"));
      }
      else
      {
        add_b1 (plci, &parms[1], ch, plci->B1_facilities);
        if (a->Info_Mask[appl->Id-1Ete_p00)
        {
          /* early B3 connect (CIP mask bit 9) no release after a disc *(plci, &msg[0]);
  i,LLI,"\x01\x01");
        }
        add_s(plci, CONN_N(plci, &msg[0]);
    add_s(plci, LLC, &parms[4]);
        add_ai(plci, &parms[5]);
        plci->State = INC_CON_Aappl->Id-1  sig_req(plci, CALL_RES,0);
      }

      for(i=0; i<max_appl; i++) {
        if(test_c_ind_mask_bit (p(plci          sendf(&application[i], _DISCONNECT_I, Id, 0, "w", _OTHE         if (plci->  m = 0x3f;
              for(i=0; i+
}

static byte connect_a_res(dword Id, word Number, DIVA_(plciAPTER *a,
			  PLCI *plci, APPL *appl, API_PARSE *msg)
{
  dbug(1,dprintf("connect_a_res"));
  retu      }
        nl_req_ncci(ai(pl, ASSIGN, 0);
      }

      if(plci->spoofed_msg==SPOOFING_REQUIRED)
      {
        api_save_msg(parms, "wsssss", &plci->saved_msL *appl, API_plci->spoofed_msg = CALL_RES;
        plci->internal_command = BLOCK_PLCI;
        plci->command = 0;
        dbug(1,dprintf("Spoof"));
      }
      else
      {
        add_b1 (plci, &parms[1], ch, plci->B1_facilities);
        if (a->Info_Mask[appl->Id-1] & 0x20INF\x02     {
          /* early B3 connect (CIP mask bit 9) no release after a disc */
       || plci->Stati,LLI,"\x01\x01");
        }
        add_s(plci, CONN_NR, &parm || plci->Sta   add_s(plci, LLC, &parms[4]);
        add_ai(plci, &parms[5]);
        plci->State = INC_CON_ACCEPT;
        sig_req(plci, CALL_RES,0);
      }

      for(i=0; i<max_appl; i++) {
        if(test_c_ind_mask_bit (pI, Id, 0, "w", 0);
          plci->State = INC_DIS_PENDING;
          nnels));
        if(!p1;
        }
      }
    }

  if(!appl)  return false;
  sendf(appl, _DISCONNECT_R|CONFIRsett----s[0].infaticcci(plci,REMOVE,0);
      send_req(plci);
    }
  }
  else
  {
    if (!plci->sig_remove_id
     && (plci->Sig.Id
      || (plci-> {
      if(c_ind_mask_empty (plci)) {
        if(plci->State!=SUSPENDING)plci-de, API_Px01\x01");
        }
    *);
statig(1,dprintf(appl, A       }
              }
              != 0);
}

static voi, Id, 0, " *, byte);
static void C    }

    /P,0);
            return 1;
          }
        }
        nl_r

  if(p2i, APPL *appl, API_PARSE *parms)
{
  word i, Info;
  word Reject;
  static byte cau_t[] = {0,0,0x90,0x91,0xac,0x9d,0x86,0CODEC) {
        dummy_ic byte esc_t[] = {0x03,0x08,0x00,0x00};
  API_PARSE * ai;
    API_PARSE ai_parms[5];
  word ch=0;

  if(!plci) {
    dbug(1,dprintf("connect_res(no plci)"));
    return 0;  /* no plci, no send */
  }

  dbug(1,dprintf("connect_res(State=0x%x)",plci->State));
  for(i=0;i<5;i++) ai_parms[i].length = 0;
  ai = &parms[5];
  dbug(1,dprintf("ai->length=%d",ai->length)Id-1      es2f(ai->length)
  {
    if(!api_parse(&ai->info[1],(word)ai->length,"ssss",ai_parms))
    {
      dbug(1,dprintf("ai_parms[0].length=%d/lci->command = PERM_LIST_REQ;
            plci->appl = appl;
            sig_req(pl----

  if B2  ch = GET_WORD(ai_parms[0].info+1);
        dbug(1,dprintf("BCH-*/
    }
    else{        }
    }
  }

  if(plci->State==INC_CON_CONNECTED_ALERT)
  {
    dbug(1,dprintf("Connected Alert Call_Res"));
   turn false;->Info_Mask[appl->Id-1] & 0x200)
    {
    /* early B3 connect (CIP mask bit 9) no release after a disc */
      add_p(plci,LLI,"TER *a,
		     PLCI *plci, A---------------arly B3 connect (CIP mask bit 9) no release after a disc *info[1],(word)ai->le=  0x10;   /* call progression infos    */
    }

    /* check if external controller listen and switch listen on or off*/
    if(Id&EXT_CONTROLLER && GET_DWORD(parms[1].info)){
      if(a->profile.Global_Options & ON_BOARD_CODEdisommand RSE *);
static byte connect_b3_res(dword, word, DIVA_CAPI_ADAPTER         a->TelOAD[0] = (byte)(parms[3].length
    {
      /* oveASSIGN,0);
        }
      }

      if(!Info)
      {
        if(ch==0 || ch==2 || ch==3 || noCh || ch==4)
        {
          if(plci->spoofed_msg==SPOOFING_REQUIRED)
          {
            api_save_msg(parms, "wsssssssss", &plci->sa }
        add_s(plci, CONN_NND Wq(dword[2] = cau_t[(Re, Id, 0, "w", _OTHE
    else if(plci->Sd channelength)
    {
      /* Frms[0].infength)
    {
      /* F_code)
    ;
              for(i=0; i+5<=ai_parms[0].length; i++)
              {
                i----
    {
   , Numbe      if(ch==4)add_p(plci,CHI,p_chi);
          add_s(plci,CPN,&parms[1]);
     Info = _WRONG_MESSAGE_FORMAT;
                  {
               ength)
    {
      /* FaRNAL_COMMAND_option -> send UUI and Keypad too */
    db2sig_req(plci,HANGUP,0);
      send_req(plci)i=get_plci(a)))
    {
  cci] == plci->Id)
  ))
                {
             option -> send UUI and Keypad too */
    dbug(1,dpring_req(plci,USER_DATA,0);
    }
    else if(plci->FIRM,
 id nl_*/
    if(Idrms[iER *a,
			PLCtpreq_ncci(b3plci) APPL *appl, API_PARSE *parms)
{
  word i, Info;
  word Reject;
  static byte cau_t[] = {0,0,0x90,0x91,0xac,0x9d,0x86,0x_ID);
      send_req(rc_plplci->spoofed_msg = CALL_RES;
        plci->internal_command = BLOCK_PLCI;
        plci->command = 0;
        dbug(1,dprintf("Spoof"));
      }
      else
      {
        add_b1 (plci, &parms[1], ch, plci->B1_facilities);
        if (a->Info_Mask[appl->RTPT_DWORD(parmsEQplci->State==SUSPENDING) {
      if(c_ind_mask_empty (plci)) {
        if(plciq(rc_plci);
  }
  else
  {  i,LLI,"\x01\x01");
        }
        add_s(plci, COq(rc_plci);
  }
  else
  {   rc_plk if external controller listen and switch listen on or off*/
    if(nd_req(rc_plci);
  }
  else
  { IDENTIFIER;
  if(a) {
    Info = 0;
    a->Info_Mask[appl->Id-1] = GET_DWORD(parms[0].RTPo);
    a->CIP_Mask[appl->Id-1] = GET_DWORD(parms[1].info);
    dbug(1,dprintf("CIP_MASK=0x%lx",GET_DWORD(parms[1].info)));
    if (a->Info_Mask[appl->Id-1] & 0x200){ /* early B3 connect provides */
      d,
          Number,
          "w"(appl,
          _INFO_R|CONFIRM,
          Id,
          Number,
      FIRM,
             plci->State = INC_Clci) {
  lci)%04x,0x%x  add_s(plci, LLC, &parms[4]);
==INC_CON_PENDING) 
  See tplci, CALL_RES,0);
      }

      for(i=0; i<max_appl; i++) {
        if(test_c_ind_mask_bit (pU, Id, 0, "w", 0);
          plci->State = INC_DIS_ DIVA_CAPnd_req(rc_plci);
  }
  else
  { atic vo*/
    if(Id&EXT_CONTROLLER && GET_DWORD(parms[1].info)){
      if(a->profile.Global_Options & ON_BOARD_);
      add_ai(rcommplci, CALL_RES,0);
    return 1;
  }
  else if(plci->State==INC_CON_PENDING || p    {
      add_s(rc_plci,CPN,&msg[0]);
      add_ai(r
  word Insg[1]);
      sig_req(rc_plci,NCR_FACILITY,0);
      send_req(rc_plci);
      return false;
     /* for application controlled supplementary services    */
    }
  }

  if (!rc_plci)
  {
    Info = _WRONG_MESSAGE_FORMAT;
  }

  if(!Info)
  {
    send_req(rc_plci);
  }
 Selse
  {  /* appl is not assigned to a PLCI or error condition */
    dbug(1,dprintf("localInfoq"));
  fori) {
    Info = _ALERT_IGNORED;
    if(plci->State!=INC_CON_ALERT) {q"));
  fo    "w",Info);
  }
  return false;
00)
              chanstatic byte info_res(dword Id, word Number, DIVA_CAPI_ADAPTEmsg[0].info  PLCI *plci, APPL *appl, API_PARSE *msg)
{
  dbug(1,dprintf("info_res"));
  return false;
}

static byterespe alert_req(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
		      PLCI *plci, APPL *appl, API_PARSE *msg)
{
  word Info;
  byte ret;

  dbug(1,dprintf("alert_req"));

  Info = _WRONG_IDENTIFIER;
  ret = falsmsg[0].info);

  0;   /* call progresXT_CONTROLLER && GET_DWORD(parm"disconne add_     Set;

  Info   switch(SSreq)
        {
          case S_GET_SUPPo = _WRONG_STATE;
      if(plci->State==INC_CON_PENDING) {
        Info = 0;
        plci->State=INC_CON_ALERT;
        add_ai(plci, &msg[0]);
        sig_req(plci,CALL_ALERT,0);
        ret = 1;
      }
    }
  }
  sendf(appl,
        _ALERT_R|CONFIRM,
        Id, Id, 0, "w", _plci->appl = appl;
         urn ret;
}

staticc_plci,ASSIGN,DSIholdor (i = plci, CALL_RES,0);
    return 1;
  }
  else = ch;
 */
 nd---- "\x05\x02\x00         rp0";wordHeak;Ind struct-----o;
  word Reject;
  static byte cau_t[] = {0,0,0x90,0x91,0xac,0x9d,0x86,0reak;
            ic byte esc_t[] = {0x03,0x08,0x00,0x00};
  API_PARSE * ai;
    API_PARSE ai_parms[5];
  word ch=0;

  if(!plci) {
    dbug(1,dprintf("connect_res(no plci)"));
    return 0;  /* no plci, no send */
  }

  dbug(1,dprintf("connectd (dword Id__a__d Id, word Number, _res(State=0x%x)",plci->State));
  for(i=0;i<5;i++) ai_parms[i].length = 0;
  ai = &parms[5];
  dbug(1,dprintf("ai->length=%d",ai->length)H     plci->->length)
  {
    if(!api_parse(&ai->info[1],(word)ai->length,"ssss",ai_parms))
    {
      d plci->appl = appl;
      }
   lci->command = PERM_LIST_REQ;
            plci->appl = appl;
            sig_req(pl         ;
            send_req(plci);
            return false;
              Info = _WRONG  }
    }
  }

  if(plci->State==INC_CON_CONNECTED_ALERT)
  {
    dbug(1,dprintf("Connected Alert Call_Res"));
   ve? */
  0;

  dbug(1,dprintf("info_req"));
  for(i=0;i<5;i++) ai_parms[i].length = 0;

  ai = &msg[1];

  if(ai->length)
  {
    if(api_pa *, byte);
static void CodecIdChe 6 */
  "",               3,ERV_REQ*plci, APPL *appl, Aetriev dbug(1,dpri      }
            rplci->internal_command = GETSERV_REQ_PEND;
      3     rplci->number = NR       r;
            rplci->appl = appl;
            sig_req(rplci,S_SUPPORTED,0);
                    rplci->internal_cic byte esc_t[] = {0x03,0x08,0x00,0x00};
  API_PARSE * ai;
    API_PARSE ai_parms[5];
  word ch=0;

  if(!plci) {
    dbug(1,dprintf("connect_res(no plci)"));
    return 0;  /* no plci, no send */
  }

  dbug(1,dprintf("connect_res(State=0x%x)",plci->State));
  for(i=0;i<5;i++) ai_parms[i].length = 0;
  ai = &parms[5];
  dbug(1,dprintf("ai->length=%d",ai->length));s frV        po = _WRONG_MESSAGE_FORMAT;
              break;
            }
            a->Notification_Mask[a, 0)){
          dbug(1,dprintf("connect_res(error from AdvCodecSuppoORD(ai_parms[0].info+1)));
      ch = 0;
      if(ai_parms[0].length)
      {
       N_REQ,0);
            send_req(plci);
            return false;
                 }
            els  }
    }
  }

  if(plci->State==INC_CON_CONNECTED_ALERT)
  {
    dbug(1,dprintf("Connected Alert Call_Res"));
             {
    lci,UID,"\x06\x43\x61\x70\x69\x32\x30");
                sig_req(rplci,ASSIGN,DSIG_ID);
                send_req(rplci);
              }
              else
              {
                break;
              }
      ini add_IDI_SYNC_REQ  *);

void   callback(ENTITY   *);

static void d_msg = CALL_R   *, byte, byte, byte, byte, byte);
static void data_rc(PLCI   *, byte);
static void data_ack(PLCI   *, byte);
sta        if(GE   *  ta_out = 0;
    ncci_psg)[i]                            X_DATA_B3)
          ncci_pt addcci_ptr->data_pending != 0)DBuffer[ncci_ptr->data_out].P !=>data_out].P);
        (ncci_pt plci->data_sent_ptr))
          Transread=(j == 0)
          pcci_ptr->da>data_ack_out = dram_bar (DIVA_CAPI_ADAeader. = CALL_RETRIEVE;
                plci->internal_command = BLOCK if(parms->leng.
 *
 */





#include "platform.h"
#include "di_defs.h"
#include "pc.h"
#include "capi20.h"
#include "divacape           */
            break;
          case S_SUSP_req(plci);
                return false;
              }
            }
            else Info = 0x3010;                    /* wrong surn false;
}

er.length > MAX_MSG_SIZE) {
    dbug(1,dprinc;

    }
  }
 "bad len"));
    return _BAD_MSG;
  }

  controller = (byte)(xtended_features   & DIVA_CAPI_   sig_req(plci,SUSPEND,0);
              plci->State = SUSPENDING;
       3]);
          if                       the      }
              else
              {
            */ 
/*--------------------------------------------------------------d TransmitBuffe  XONUP,0);
    (dworhelpersnd =
/*--------------------------------------------------------------tern
            his is flow CALvers of the GNU GenerX_DMA *applLicense
  along with break;
    }

    if(p,         for(i=1;i<dataNLd here h+1; = t*appl, API_a->chlci->c);
s = _B = INC_CId       i_queuEXT_CONTROLLER)
    *   *)(       if(Aall_dir = _BAD_MSG;          = 0;
            rplx_*/
 ommand = 0;
  plci->ch_DIR_ORIGINATE;
            /* check 'external cof(Id & EXT_CONT 0) )
  ch dwoN_XONci, APsg_in_queu  }
            if(pa= ~ms->lengthstruct;
          rplci->Id = 0;ff                Info = 0 plci->---*x300A;
                break;
              }
             }
            if(parmsRX_FLO    NTROL_MASK    a->
            {
              i|= (Nd) {XOFF | {
     Info       if(AdvCodif(p            }
            }all_dir    i +=++arms->info[1],(word)parms->leci, wor_x;
                Info = 0x300A;
                break;
              }
             }
            if(parmsk;
    _MESSAGE_FORMAT;
                bms->leREQlength = 0;
                if(api_k;
                 add_p(rplci,LLI,"\x01\->lennels));
 re (dwoinfo[1],(word)parms->lenmit_extended &dummy, 0, 0);
   _DIR_ORIGINATE;
            /f(!plci ml_re*) m        l++      {
          d_p(plntFounon dbulci->in         
  Eilci->NTERNAlci->ms   {dprinpi.h"
#include "m remo------APPL   *,          r' bit      0i->int       c = t*appl, API_lci,CAI,&ss_parms[2      onnect (CIP&d channel_     break;

          caseci,UID,"\x06\x4_BEGIN: /* Reqe "divasy    ((EXT_CONTROLLER)  rplci->G   add_b1(rplci, &dumm                     c    rplci->comma << channelR_C2_BIT     rplci->coci);
   ");
         f(api_par          mappiTry[i].     next X_ON"\x0l = 0;
      fiut].:
     withdummy.in= 0;
  RIGINATE;
            /SExtInd(APN,DSIG_ID););
            add_s(rplci,CAI,&ss_parms[2]);
           {
  ) ncci;
    }
    a->ncci_ch[ncci] = ch;
    a->ch_ncci[ch] = XONOFF           In  rplci->State  (      R_C2_BIT  thaastlci->call_dir cth);
      _MESSAGE_FO            dbug(1,dplength,APTEG;
   =ng"));
                     send_req(rplci);
            break;

          casesc */
      case S_CONF_ISOLATE:
          case S_CONF_REATTACng"));
                Infi+ng Ctrl"));
    (ci)
        SUMING;
      1i->internFORMAT;
                         }
              plci->ptyState = (byte)SSreq;
              plci->command = 0;
              cai[0] = 2;
              switch(SSreq)
              {
      0x80)
      x70\x69\x32\x30");
         q(rplci,ASSIGN,DSIG_ID);
            send_req(ri.h"
#include "mdm_mnfo = 0     {
  f(test_c_ind_   *plci, byte Rc);
static void retrierplci->State = RESU RESUME  Inf,dprinGE_FORMAT;
              break;
arelease_WRONG_MESSAGE
                   {
              if(api_pars/
     bit 9) no release after a disc *e(&par                 if(test_c_ind_mask_bite = (else
     i=0; i<ma        =     plci, CALL_RE           }S,0);
      }

  ci, CALL_RNum        }
  See the GNU, LLC, &par    }
  mpty (PR)->head
     ONG_STATE;
      if(plci-           sndf(&applicationi], _DISCONNEinfo[1],(w    RMAT;
  can &dummy, 0, 0);
            if (a-lci);

s      (ncci)
            send_req(rplciand);
CCIcod     id Sencounyte band);
stxa1",       
      G *)(&res(dword,   break;
    }

    if  }, /*      ca  {
  0x80)
      plci, ng stse
   ch               /
};

a  sig_ndatic void ec_ind     {/*     * all LCI   s     in the Appl* 12 */
pooord I-----            {
   belongnd_quo&parmsame wron.ci[i-if a first iizatims->length,"wbd",ss_paused.,                           /* 13 */
  "",      msg    upport(e)d;   {   *er' bit fo    <     ca->Max)->hea      cai[1] = (wrong st==   }
     LLCwronS_CO      fo = "s->le      caround for thee_ncNum==<< i;
  ;
    stati         p     > 2g_req(;
              if(d>=0x80)
          _3PTY_END: not    */
/*------------------------------------------------------------------ __LINE__));
CPN_filt (Pok,dpriprincpn,            }
          plci, ffsetcci = s distr1parms[i/*   Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            i--*/
func2 */
groups&parmlisten----, by* 12 */s accori +=        CIP  n,  d UUparm       s[4])_Mask. Eachth)
   == s alse;one Cq_ncci(Ind. Some           I  || (plci->S      bareI   *multi-instc vo cappter, s     yDIVA_C e.g.comm          Infwhat causes      bbigl ==blems ----->info[1],(levnfo)if (turn,comm!plci)
        t). T     command_fmsg[1].lengtmlse;be e);
std byo);
    a"a->h)
  ust_bmiz12 */_addCONF" 
               bOS specificARSEt (pereturn er)            Info = _WRONG_MESSAGE_FORMAT;
      dbug(1  Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            if(!e received alatedPLCIvalue));

            }
            if(plci && plword, DIVA_,j,k,  {
,latedPfoureq(dwid Senai(pl n, _h)
  [dataCIP     S            ipTY_END)
              {
      
     roto*);
stD)
   _type      sup     NU Generaux;

sta  addtI=PLCI"E_FORMAT;e S_SUS
        supi_parse(&pais inc.tedPL_IDENallow= 0;
 dial inintf          latedPLCIvalue));
        under the term(1,the GNU("No   }
  LCIvalue));
"rogram is dist                break;
  G        }
            %l usedparm* wrong state           */
h==7)
}
      codec       {
      L *appl, A_3PTY_END)
      dbug(0,dpr    dbug(1, dp lue >>=8;
 WRONG_MLCIvalue &  sup;
            use 2nd PLCI=PLCI"));
 nd PLCI PTR*/
         i<    
         c    heck     nyIER;
              breainfo);
      is pCI *ntintf("{{
   latedPLCIvalue));
        1 meansI   *to    }
  eIER;
              brea      relatedP(PTER   )intf("foif  "" * 12 */star__)->mf(!rplci || !relMaxwron) > 1edPLCI->    
   the T        latedPLCIvalue));
         ==1) ].length; i++)
      break;
  MR;
  I           break;ndapter->p12 */
rplcired     }
     break;else
  ow goor use * 12 */
unbreak;edfo =q(dwms->len (msg->h          {
                iBuildGE_FO            parms->le(!rplci || !relatedPL        if(SSr           dbug    k=0 {
  =ord) n k<ties)xt, wri k k += (((CAPI_MSG   *f




OLLEkelat) data) - pending=%d/%d= plci;    ci:%x",rplT_B3_ACTIVE_I|(= plci; reques=plci!rplci || !re    info);
      has a   {
 NU Gems->length,"wI_XDI_PROVIDES  {
             }
  dbug(1, dprintf("uses->idadapCIva  {
",i+ (((CAPI_MSG  *msg, word le      i(tci,    },TY_ENDbitremoplci;,EADE%x",rplci->ptyState))n    om   a-       i +=    dbug(1,dprintf("plci->ptyState:%x",plci->ptyState));
            dbug(1,dprate)             i +=Sreq:%x",SSreq));
           ----------}
         j=0                j<=(e & 0x7f))-1]ms))
!  {
 &&!           i-----d",ss_par    /h)
         fre }
            only    dbug(1,dprintf("rplj==stuff */
          {
       h)
    IDENin
    but   }
  stillI   *         dbug(1,d{
  "",                     /*     e & 0x7f))-1]   }
  addCONsdPTYP    s belue = of fieldublisci->    dbug(1,dpr;i<relatedadapter->max_plci;e & 0x7f))-1];->length,"wates because%x",plci->ptySta(1, dprintf("useF      {
      equesadapSreq:%x",SSreq));old_save_command (_plc3PTY_END)
    j]=AGE_     if(SSreqdPLC    dbug(1, dprrotdialreak;
   ate:%) ;
         
  "",                           /* 13/*CIva  /* fale ofyalue)
   ?ESSAGE_FORMAT;
             rplci->internal_command = ECT_Rj|_ptr;rotdi      parm  /* f*);
stNG_MEe    nfo);
       if(SSreq==S_cai[1] = ECT_EXECUTE;

                rplci      1,dprdPTYPL)
   0;
      ,GE_F=0x%lx",;i<relatedadapter->max_plreq:,      rplci->vspro     rplci->vsprot=0;
        !PTY_REQ_PEND;
      = rplci;
              rplci->ptyStaplci->vsprotdiesapteish aata internaNG_MESSAGE_FORMAT;
        Sreq==S_CONF_ADD)
              {
                rplci->internal_command = CONF_ADD_REQ_PEND;
                rplci->vsprose
        if(SSr  }
   rplci->i!=rpE_FORMAT;G;
 }
      lci) /* e*/
                plci->vswitse
   te=0;
                 }
      reak;
   else
  is           {D;
                cai[1] = CONF_ADD;
              }
   Newrong s           if(pe
              {
                rplci->internal_command = PTY_REQ_PEND;
                cai[1] =
  { "\x03\x91         Info = 0x3010;                    /latedPLCIvalue));
 Y_ENDapterate           */
  elatedadapter->max_plx",rplci->internais     ,>Id:%    {
commdak;
mecommao      /* f*/
     Id, PLCI wrong state           */
 tf("stuff */
     rd, DIVA_CAPI_ADAPTER   break;
  O{
     plci,S_SER, (1L <<printf                         rplci->internal_command =data_b3_req.Flags nnect_info_command (d           }
                  {
            tf("format wrong"));
              Info =eck all swit j{
         l_comma    earch other        break      mark
   

sttyStatcation */
        --------------ECTION:
            if(api_paruse 2nd PLCI=PLCI"));
 j]);
        printf("plci->pt));
            dbug(1,dprint break;

   /* f      naticll",j+1     rplci->internal_com              TTACH_feat      }
      nternaommandj);plci->vsprotdid;

/*
      on
            }
      ms->length,"wbd            appl->CDEnabl      >vsprotdiof the CTION;
       ci->inte  /* lci,S_    ai);
         ----------------------------------
  "",                           /* 13 */
  "",  x",rplci->ptyStshouldI   *get a       case S_CALL_DE      cai[0] = 1;
            ca         {
         /*star if nlci,parmdriv(maxbourse(nfo);
      C*plcReg   { _WRO      api           if right (c) Eic    plcis      if     rplci ;

stati_pos,
          plci->msg_i          break;
       {
                        *mat wrong"))!relatedPLCIvalue)
       Id!=F_IS  {
        pl  break;
  ++1] = CALL_DEmber;
         a.info);
      ate)beenedPTYPLCI = rpbd",ss_parm*/
  eak;
  )    {
                    {
    ude "md     cS_CALL_FORWARDING_STAscUID,    df(&aps..     te             &   else->commandrplci:rplci->AT;
                ---*/dynamic_l1_down)               elseE,0);
L1 triE *);
(Hunth)
           >command = 0;
 !  sig_req(rplORWARDING_STA dbug(nfo);
      doif(S    i r        ning indicator */
          (j=
  p%x",(a))(rplci);
           _STARctd adjenin

        else
    dbug(1,dprintf("plci->ptntf("plci:%x",plj-1i));
        c);
static byte dtmf_request (dead=%d      cOAD,;
  1\xfd

  Info))
       ion */
  CAI       c80[2] = (byte)GET_WORD(&(ss_pUI      6\x43\x61\x70\x69\x32\x3[0])); /* Basic Service */
 SHIFT|6,0004
          add_p(rplci,OAD,IN     lci->numbemixer_set_bchannel_idlci) {
    dbug(1,d0);
 ----SIG        P  do a  Number, DIER  *      c    ai D;
  I         "\x Function */
  r_fei,CPN,sff\x07"  elsel1      }ms->length,"wbd;
           ;
  oid d         datd switch lisie_compare(byte \x21\x8f" }, /* 8 */
  { "\x03\x91s distrord) ncci)ci);
static void set_group_ind_mask (PLCI   *plci);
static void clear_gr/* F1].lengselse
virtual S---------
    j=0;jERT;by join,lue fere    AT;
       ved aV      ReqInd* 16 */
  "\x02id SendSSE        *d Id, writ,        req)Id, P].invtatic _ *, 0h,"wbd_remov
 1h,"wbdVrd i, IE
 2"format wrong    /        IND
 3, APPL   erved
 4      se(&parternal_
 6 appl->Mdisterror
 8    P  *ps
N:
  al_cntf("|dInflci->msequesEST:
       , PLCI  ci,CAI,caSig.Ind==NCR* bp_parm
 add_pState = R
tedPLCIvalue & MULTI_IE;
     ci->internal_catic bi][0e T-Vntin      if(           <7lci[i-1];               /* k->reiON:
    
           
    {
    break;
  se(&parms->inf%d)",         4       tatic      dbug(4    }API_PARSEVSJOI Id, PCCBS_REQPLCI_CANdPTYNU GeVATE:_CONF_ISOptyCAPI_!=S_nect *)(&((by     if(api_panfo[1],(word)parmpl)  r{
   Eak;
N:
      DIVA_CAPI           break;fo[0ne{
  arydprind, P     */
          dbug(0,!=11Rc);
        8]!=3x",rp->State }
  */
   and = Gbug(0,dprintf(       dbug(2]==Info = _WROs->le        9]      {
  D;
        , /* 12 */
afsed ECT-Re as pubnlue  *, ng"));
 arms->info);
sta          *,=rms->info[1lci->Sums->info[1]2;
   , PLCI*/
    /* nte=0[0] dbug(    dtoUMBERE_FORMA                         if(api_paNGUP&parms->info[1],  {
       Answak;

            E,0);
     break;bug(0,dprintf("format wrong"));
           te =eSreq{
               
      
            P,0);
      br_optimizofdbug(     b&parms->inf
          {
                  dbug(0,dpri   dbugdprintf("I   ng"));
   10ion"))P,0);
   */
        cai[1]     ec = 0x60|(byt1)GET_WORD(&(ss; /* Fu_parms[   brnd      ci, wordto      if         dbu 0x60|(byt]if(api_pI", _OT                                     c"wbdws",ss_parmsic byte dtmf_req             case S_INlci) {
    dbug(1,d               send_rWORD(&(ss_         case S_,ESC, API              ;
          ON_INTERROGATE_NU           RS:
    e S_CALL_FORW         case S_/
    ,
        NumbVSTRANS---- 2.1PTER   *, PL              if(api_parEGIN: /printf("format wron=3LL_FORWARDING_"wbdws",ss_parmsSTOP:
         word)ping)--; DIVERSION_INTERROGATE_NUM; /* Function */
                   rplci->internal_command = INTERR_NNUMBERS_REQ_PEND; /* move to rplci i     *, APPL   k;
  rmat wrong"));
                     */
/*------------------------------------------------------------------d_req(plci);diva_
  pdma_descript;
  o[1],(word)parms->le  *    magicISOLATRc);TY      IDI_SYNid mi* p     i(EACTIVATE;
  )&eCALL_HELD));
   xdi    sig_reate == INC_License
  XDIc voVIDES))[k]MA   if(d>=0x80)
  - not n      Req-> CCB         case _oper    I      i else
ERROGATE:
                  cai[1] rd fEACTIVATE;
 k;
 _DESCRIPiceCc byte re      ERROGATE:
                  cai[1]    .    cai[1 = *, APal_command = CCBS_INTERROGAALLOC_INTERROGATE;
                  rplci->                l_comma = -See th break;
              }
              rplci->appl addrq(dw      brea      rplci->number = Number;
              add_p(r      }
  sg)[i]e.us,0x%x,0x, or (at your opte=%d rea(plci,S_SERVICE,0);
   ([1] = *)    i_ptr->dat!                 break;
                default:
 EGIN: /* (  break;
              }
              rplci->appl = appl;>complEGIN: /* ");
              add_p(rplci,UID,"\x06\x43\x61\x70\x69\ISOLATE;              );
              add_p(rplci,UID,"\x06\x43\x61\x70\x69\P != plSSAG3 break;
          c, a:%d (%d-%08x)ic void clea, or (at your opti);
         break;
              }
              rplci->appl = applcci] = 0;
            ogram is districase S_CCBS_REQUEST:
            case S_CCBS_DEACTIVATE:
*, APP                   break;
          a0;

  db] = 3;
         se S_CCBSinfo[1],(word)p;
                 case S_CCBS_DEACTIVAT    nr   cai[1] = CCBS_DEACTIVATE;
                  rplci->internal_conr <           break;
    _INTERROGATE:
                  cai[1] = CCBS_INTERROGATE;
                  rplci->internal_command = CCBS_INTERROGATE_REQ_PEND;
                  break;
                default:
       /* Basic Service */
            add_p(rplci           cai[1] = 0;
     word      break;
              }
              rplci->appl = appl;
  nmdm_ms     rplci->number = Number;
              add_p(rplci,CAI,"\x01\x80");
              add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
              sig_req(rplci,ASSIGN,DSIG_ID);
              send_req(rplci);
            }
            else
            {
            ISOLATE;add_p(rplci,CAI,cai)      }
      ;
                add_p(rplci,CAI,cai)           iong"));
         S_CCBS_DEACTIVATE:
          case S_CCBS_INTERROGATE:
            switch(S