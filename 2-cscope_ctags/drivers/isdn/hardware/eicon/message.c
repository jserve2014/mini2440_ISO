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
 c) Ei}() Eiccchannel_xmit_xon (plci);
n Netwoif(appl->DataFlags[n] &4) {n Netwo  nl_req_yrighis s,N_DATA_ACK,(byte)yrigt th
  Eicoreturn 1s.
 *
  on Netn : n : on :Filfalse;
}

static er A reset_b3Netw(dword Id, distrNumber, DIVA_CAPI_ADAPTER *a,
			 PLCI *ange  APPL * supU GeI_PARSE *parms)
ith
e it infos.
 e it yrigurce dbug(1,dprintf("re; you can ")ore Fo  th = _WRONG_IDENTIFIERs.
 **
  = (e it)(Id>>16rs.
 ifrange &&sion.)
 wishe  our option)
  STATEs.
 *
switchTrange->B3_prot thehe hope case B3_ISO8208: ANY KIND WHAX25_DCE INCLUle ision.on.
 freepyrigh==CONNECTEDANTY O OF ANY sion :   works re GNUofRESETrvoftwdapters.of MEREisend 2, rangeour Licensethat itGOOD Revision :   wobreakilic LDING ANYTRANSPARENTied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULARstart_internal_command (ibute GNU  version oundatiaof theon.
You should have received a copalongthe 2.1, MA/*75 Mass A mustcludult in aclude "dive,n & "pc.h"
#iInd */
se foe detup,dge, MA 0c., 67_B3_R|CONFIRMpi.h"
#incIdnclude "diand/or dge, MA 0"w",our ridge  ITHOUprogramantyHANT softwa version 2,sred by
  Inc.it 
#definmodify
 ----underlatfoterms "platfo, 67General Public License as p supshed by
------ Fe, Cambon; either vered i 2,sor (at yd in, MA 0ine dprintf




----------dclude hope  but WITHOUTINCLUWARRS FOR FINCLUDING ANYTSOEVERied waDINGINCL----mpl License
  along with this progrINCThis_PENery t, write to the Fralong with this pro =ram; if noidge, MA 0 PURPOSE.l beee/* This iA SeGeneral Public License  This true Revision :   lude "platformlude efine dprintf









/*----connecsion t90_a-define DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR  0x00000002
#d*/
/*ecessaist it wiivacaporveryfor all al PubliGE.Ct are upported for ncpine D*------qre server by    */
/*    0x0can re1
#defi Allo----is not necessary to ask----from every0x000000*/crose t toned here
 *
 *onlyACTcal meanie hope Ei CAP #inc/*define DIVA_CAPI_XDI_PROVIn :   elseOVIDES_N04
#iibut xdiOVIDES_NCONhis
#incst toVIDEdify
  itSUPPORTS_NO_CANCEL(__a__)dge, MAreq = NCTUR if ----nufactureh"



all [0]==0CANCE---- originis
 *ssagorts thi by ! "diluense
  _OK_FC_L.iGE.Cdefine DIconcp not&_XDI_P1]idge, MA 0if(
SUPP>length>2   ((__a__))define DIVAIDES[1] &1)er_feaR_FEs & MANUFA | N_D_BIT----------- add_d See t necessdefine DIVA_C3),&000002
#def4hrs.
 *
  eceived a eived a  PURPOSE.
  See tDMA eneral Public Licen  This e Revisorm.h"
#necessary to ask 






/*----selI can more-------------------------------------------------*/
/* This is options supported formsgadapters thour =0e F



SoOV----_RX_telof thepported fobp__XDI_P7]CTUREif(!-----|| !ci, w OF ANY 
#defit will bany lat/* XDI ----_)->F_FLOW_COrver by    */
/*;s free svoi[%d],long=0x%x,Tel  *, CNL_MSG  vaca_MSG  sth th_MSG "ne FILE_ "rd, er A *,msgdefine D,it is Id));
wortel void aNL.d api_d avacapit is SuppSh thAllord CapiRegister(distPlcirmat,sever A byte *d ap _CONTAVfor(i=0;i<7;i++)*is s, wori].define = 0CTURER_/* check if noSE.
, 20CAPI_Xen,i_reB3A_CAP0x0edpport  &FF_FLif( it is  *in, == IDLE)matiCICAPI void aic OUTG_DIohis
 */
#diva_get_extended_adportal functio(DFF_FLOVA_CAPI_ADA*#defSAVE!I_PARSEIVA_it is move_cos  it under :   move_idessary to saPI_ADAPTER   *);e useful,
 ------c voPARSE_ADAPTERdram_b r byfillt);

word pot are _removewordmanu  iticense  Incapi

wose(&LCI  
#defin,PI_ADAPLCI   *, by) "wwwsss",t);

word)  callback(y laTYet_extes frMESSAGE_FORMAVA_CAP ()->manu->  *);
static ( of tt_extenderototres ER_FEAPL  VA_CAPI_ADAPTERnfo(APPL  ALERT))_rc(CANCEL ert tone inbr bytolatfonRPOSE.,c void daC{dist byte *,  * * as pubdInfo(rd api_d VSbut WReqIndvoid Send);

w/* e.g. Qsigram;RBSte  Cornet-Nparmxess PRI plci, dwo api_Id & EXTres TROLLERt, writete to the Frres = IVA_cap _SEL proB_REQsglude "d-------------PLCI 0x2002); dwowrong byttrollic void Vdgroup_ it mizbyte ve received a  t under byte o(APPL  am; if noer Ad;idge, MA 0icense  *i = vacaidge, MA 0clear_c_ind_mask_bit_CAPI_id Send,ROVIDElId-1E   *in,_SAVEumpder t_MSG PITHOUTourdisconneE   *PAR i<max_d Id,  *out/*it f CAPI c_MSG o/
/*,dists Id, d Id, Dstributyte *d, D parms);

/* its quasi astatic vdisconne
static bytIVif(testdisconnect_PPL   *, Geni SendSe, APPL  fy
  i&word,cancti[i], s frtion proILonnec0);
stat_OTHER_s op
stat);
wodefine DIVA_CAPI_XDI_XONOFF_Fpi_save_msg
sta, " sig&it is y
  dit u;
static te Id,PARSE pi_
static ;

static vyte *    0xtNetw(dNO_CANCEL))c bytlid Seexare
 turnVA_Csrediinic v byrer_f-*/
/* es(dword, wdefine DIVA_CAa->AdvSignalAedise);
  it under t  *!=vacastenA_CAPI_;
static bytIAVE   *out);
statiExt_Ctrl*);
stat1 Alloes(dword, woITY   *);

static void coARSE *);
static by
static ata_rtic byte   *, ADIVA_CAPSNOT*);
staticdword, woal? SenE *);
static byte faify
  it under toft_a_resextended_adyte *;
volity API_PARSE *);
static byte c2, APPL   *);
static byte , DIVA_CAPI_ADAword, word, DIVA_CAPI_PAR/* activ_barE *)codecstatic byte cord, DIVA_CAPI_ADAPTER   *, PLCI   *, AP testAPI_PARSE *);
staif(AdvCSE *_sdrort(aInc., 67ons su0) PARSE *);
statrd, DIVA_CAPI_I_ADAPTER   *, PLCIrroes(dwonnectprocedu selAPI_ * parms);

sttic byte connect_b3_res(dword,  *);
static by datatatiLCI   *ploofyte co==SPOOFINGof tUIREDid Sewa-----tiSE *)*, isAPPL  estatic byte coPLCI   *, APdisconSE *);
static byt = AWAITer tconnect_E *);
static bytit is -----
  server by= BLOCK_b3_r DIVAlockIVA_CAPe, Camb*, PLCI   *, ct_b3_res(dwreq(dwoata*, APPL   *CI   *, can req8----
  CAtinu_rc(A_CAPI_loadedRSE *);
static bytenecessary to a word, DIVA_Cord, w *, Ard, DIVA_CAPI_ADAP--------  it und byte connect_b3_resd Seis OFF );

wo
staAPI_PARSE *);
static byte connect_b3_res(dword, neete con/* andoff  *, API_PARSE *);
static byte connect_b  *=ct_b3_res(dword, word, DIVA;
voI_PARIdCoid atic bytSE *);
static b);
staticbyte);
stat);
word_a_res(dv_nbyte connect_b3_resADAPTER   *, PLCI   *, APdisableRSE *);
static );
static byte ce connect_b3_res(dword, word, DIVAtatic byb3_aAPI_nos.h"quAPI_ *, API_PARSE, Pt---*; you API_PARS
staticif (!ADES_PPL   *, API_PARSE,_CAPI_AD *);_dir & CALL_DIR_OUTPARSE *);
stify
  it under =t_extended_ad |t_extended_RIGINAPPL   *, APIata_rc(, APPL   *, API_PARSEAPI_PARINPARSE dd_pword, worconnecconnect_re);
INic void VaddANSW* XDI atfote test------atic byte dan,   *, APP);
word ae, Cambridge, MA   *, Ayte connect_b3_n : bytrm.h"
#atic word get_plci();
static byte connect_b3_res(dwo;

st"URE_XONOFn; eith
*, API_PAUPPORTrc(PURER_FEr void set_group_ind_mask (PLCI   *plci);
static void cistr*/
/* This is options supported for all adapters the, Cambyte * API_d, D004URE_XONOFF_FLOW_CONXDIm PLCI  word, worm

word 5defin);

stadecgroup_indDwordoup_indchd  for_d serPLCIrd, byt send_hi[2defi{0x01,I ca}atic void VCI   lltavoid SendSetupId_addistris s_remer Ad_cae_check(PLCI   *1;
static void lisnullit unde{);
static voidported fote *ADIVA_)= {yte &yte  ddIn;
sta "plad, Dv_angelci, byteLCI   *, server by    */
/*static bytte, byconnectct_b3_res(5*, PLCciword, atic e

voiee So(vif(GET_DWORD
stat_PR_group_!=_DI_prot_ID   ((__aoid VSendInfoCI   * plci*, API_b----   *, API_, by
static fin1_group_lci_mdefine DIV(PLCI-s(dwords free code ord,an( p_lengty to save ity
  ASSIGNI_PAR License
  atatic void _resstatPL * plcite);
stat"wbbs",ie_comp)------------byte   *, byte   *);
static wordyt functio= 0;

#LAR PURPOSE, APIokver AARSEie_compad_cip(DIVA_CAPI_c  *,ie_compad, word[0define DIbyte cie_compa2turehI   *);
stvte   i=getA_CAP(a);

woactureh,-----= &a->ange[idefine DIVA_Crd, wo;
stati);
static byteid add_d(PLCI   *it u(APPL   * add_ss(Pieit is mARSE *);at  * c void Vc vo  *,
#inn#defi fun#defi, DIVA_C, byte  );
word LOCA, API_PARSdge, MA 02   *( (I_ADAP);
word <<8)|word, worPubl_ADA|0x8  *, A
statiic vo   *, APPL ManCMDI_AD(i_load_IdAllo ittic byte (ch==1ARSEunde2)CI  (dir<=2hannel_can_xo_CAPIhelp= XDI Ad( plc|chSE *);
static
vois, 20eq(dword, wor   * plci, byte code, API_tatic void Vss(Psword, worPL   *,bhelpers
  *deceq(dword, word, DIVA_CAPDING 0 Licensecan_xoDIVA_CA senb1IVA_CA&ie_compa3],0,dd_ai(PLCI  _  Thic void adv_writeFoun1anc void a * p senpIVA_CACAI,ARSE
void  add_ai(PLCI  static API_b1_fac/*tatical 'swi *, n'e connecn (PLCRSE PLCdapteout snder lingstatic byte co/* first 'as(PLCI   *'I_ADAd, DIVfuncnctib1_factatic void add_ai(DING 2s (PLCI   * plstatic void add_ai(PLCI   *, API_PARSEPI_PARSE *);
statthat it RESOURCE_ERROtic byte SeSE *);
static byAPPL   nnel_canRcf(PLCI   *plcaticce_I_ADAPc void , b,0,B1_FAC prog_sk, bSE *);
static byt  it underx10 API_PAR);
stlVA_CAPI_streamc void add_ai(PLC, PLCI   *plcb1_facilities (PLstatic byatic byte conne *, Pvoid adsetupPars PLCs, byte  i API_PARd); =_xdi_ADAPTER ADAPTER  
  Thic void adv_vfo_res(dword, woSeword Id, IE_connect_ack_ctatic wributconnect_res(dword,connect_reid add_d(PL;

static vwoi);
sAPPL );
statiannel_canb1LLI,llPLCI   *, APPdd_x_connect_i_HI,chndationedistributLCI   *plcUID,"\x06\x43\x61\x70\x69\x32\x30"tatic byte add_ bytmorortst,flow_c,DSI);
w (at yfile *, DIVA_CAefsx_connect_plcAPI_PARSE *);
stattatic *plcfa23ilitiesconfigx_coDAPTER   *, PLCI   *plci, SAVE   *bp_AVE   *bp_msg, word define DIVA_CAPI_XD fax_d *, is sourPLCI  d forANCEatic voiridge, MA 0static void fax_adjcset_b3_command   *bp_msg, word diconI_PARSE *);
static bytatic void fi*, wo ap byti_load_dirc voiAPIVA_CAPI_ADASE *);
static bytodeA_CAPI_   *, APPL   *, API_PARSEvoid fadd_ai(PLCI   *eveAPI_toriliticode, API_PARSE  rks, 20res(d_con);
static byte connect_b3_R   *, PLCI   *, APPL   *, API_PARSE, P adv_APPL   *,  *bp_msg, wores(dword, word, DIVAc);
staticvoid cha rej_PARVA_CAPcal  */
whilmsg, word length)atic byte connect_b3_res(dword, word,  *plcieatuies dword Id, *plci, tatic byte connect_b3_res(dwPLCI  I_ADAPbytete bcribu=1;
static void rstaig (PLCI   *plciAPI_REQchi);
static voidtiidI   *plci, byiata_rc(P!dir)hDAPTER   *,*plcimixermmand (dwLISTE ThiCI   *plci,   *plci, byte othersnoti   *plci, byte bchannel_id);d, PLCI   *plci, Aer_command (dPLCI   *plci)dtmA_CAPI_ADAPclude "di;
stayte cont_ack_command (  *, APPLcIVA_CAPI(PLCI   *plciasynclude

s_set);
statURE_XONOF *, word (dword Id,"dww",y
  it unde,e, CambI   word a 200i(PLCI   * ploi2nect_b3_res(dword, word, DIVA   *, APPL   *, API_PARte faplci, dwoOUT_OFI_PARSE

#include "pl_id (Pks, 200IDI_CTRL License
   Auto*);
static API_plcthat it will b
word CapiRele_xtatic word get_b1static vx_connect_ack_commtatic void Vrks, 2002_ofcommand (dword Id, PLc void advlage Rc);
static byte ec_requc_command (dword Id, PLcal fuyte   *mreVA_CA Rc);
stati void fax_connect_ack_command (dword IdCI   *plci, byte Rc);
static voidic void ck_command (dwoie_typif(req==ord Id,  void mixer_removeit is b_

voicom=r_reCove_co(te bchann1i);
static vothersersiomove_co0id_escrd Id, PPLCI   *plci)t word el_id)   *plci);
static void dtmf_indicatioLCItic void fa *, APPL  ;
static byte connecord Id, P|ER  icr by );
static void fax_aic void msgd hold_------e Rc);
static bdtmf_parameter__facioid mixer_notiber, byteyte connect_b3_res(dword, wb1_facilities (c void mixer_indicatiedistribLAWI   *plci, Id, PLCI   *plci,cr_enquiry---------------, PLCI      ss, byteFTY (dword Id);
static v Rc);
static ck(PLoid fax_avoid init_b1_config (PedistribHANGUP*);
statiplciecer_removei(tic byte;
stI MaptaticEQ SendIn*plci   *);
, PLCI   *plci, Id, word I_ADAPIDeq(dword, word, DIVA_CAPI_) &&( is not1;s all < MAX_NCCI+, ...);++function protot--------------- Id, along wi voidd, PLCI tatic vIdId(Id) A_CAPI_SUPPORTS_NO_CANCEL(__a__ut);SE *);
staticyte conn supA_CAPI_A_CAPI_SUPPORTS_NO_CAI_ADAstatic void byte   *md Id, PLit unupSE.
 _static Id, P Public LicenseCANCEL))
PURPOSE.
  See the ISCGeneral Public Licenser_command (dword Id, PLCPLCI   *);
static byl_id);f00L) atic void diva_frdefine DIVA_CAPI_XDREMOVE---------------void init_b1_config (PLC})))URE_XO



#include "pltic byte ec_rtati----------  * * appl, dw*, API_CI   loo disconne

/*B-oid rtp_ *, API_P  *plci, byte othersyte   al function prototypeplci, dworelci,te Rc);
static void fate ec_reqCI   *plci, void fax_connect_ack_command (dword Id, byte  onnect_b3_req_command (e,r ver Ad;
ADAPrnmupported for mixer_indical Publ;-------------void init_b1_config (Ptatic byte fac, DIVA_CAPI_ADAPTER   *a, PLCI   *pdefin_CAPI_sg(API_Sx_R DIVrd);_b3_req}PI_ADAPTER l|RESPONSEDI/





r/transmi);
stat*0x000000|RESPONSEyte conn  supnr);

/toty"ss",word length);
e   *, byted = programove_stael_iddummyfine  remove_stastruct _ftADAP {PLCI   *alse;
s;
, APPL ) (((matR,       (* w_b1t wilI   *plci);
static void dtmf_indicatiostatic byteDSPtime (cPPL   *, API} info_r[] =es},
{f DIVAdm_m, bytCTIVE_I|RESPONSE,     id mESSAG       c);
static },es},
  ADV_CODECfor every ay
           Licensee DIELWHAT|y
  it undci, bes (PLnoni, bndI_SAadjustments:FO_R,     /* R   *on/off, Ht unet micro volumexer_indicatstatic by.E,           h      +  * appl,speaker*, API_},        +     c. gain                  




necehook----
  i)    ord,xify
  it u00002
#deficommand   *,d length=en_res},
 ISCOic void dtmf_indicati!d SendI_PARAPPL  * appl, voidtic byte connect_b3_res(dword,          I_SUPPORTS_NO_    "wssld_sommand);I|RESPhave received a void fdistrxer_remove UnMapIdm);
static
  {te ec_req >= 3void fax_ad       
#define *
 woI   *plci, bUnMatmf_comm2]    1ufferFree(Aller (ver Ad(Id)CI   *plci, A}        _OLD_;

/MIXER_COEFFICIENT Pubdword, word, DIVA_CAPI_fc(PLstatic byt!= s},
VOIC_CAPI_CAPI_ws",cturer_res(dwortic ------------------------tern bhold_    "wsssss",  PLCI   *, ARc);
static void fation;VA_CA);
staa      1_faciloef_byte   *,TIVE     "s-zation(Dier (;
statinnect_a_res} GNU CONN    >PONS       -"ws",         *, APPL  reset_b3_res},
  {_CSPnel_id)FITNCONNT90_AC);
static byte c          *, APPL  ws",   CON        N {_DISBUFFER_SIZEci);
static vi, ,   "ws",           connect      dtmf_comman001
#tatinect_b3_res(dwplci, byt0_ACT<LCI                    byte RPLs",   SELa_res                "dwbuffer[)   (((f_comm4 + mand (dword      con))0L) Bs (PL_ADAition   Foundati);  conN manufacturer_re reset_b3_facb3_resPPL   *, "s",      WRITE_UPDATEmand (dword Id,es (PLres},
  (PL);
static byte corer_res(dword, wows",      ;

/DTMFrtedAMEI_PARS      CTIVE_I|RESxer_request!\x03, word *, bytfea *pls},
_ack_command FEA  * 0 {_C  { b23_3\x83",     "\x03\d, PLCI   *plci, Aws",  UFACTURERNOTNTROL)&word Id, PLCI RESPONSE,       a3",     "\xnect_b3_res(dword----lci(D/
/* e  connectONS                   ",        0"  90\x91\xa2"x90\x},_fac4 r FITNes},
  {_CONSE,      
  {03\x91\x90\xa5"     }, /* 5 */   select_b_r"",       
  { "\x02\x98\x03\x91\x90\xa5"     }, /* 5 */\x02\x88\x90" */
  { "\x04\x88\xcacturer_r"\x02\x98\x90"         }, /* 68\x904\x88\x91\x21\x8f", " "ws",   prototPPL   *ECT_ACTI03\x91\x90\xa5"     }, /* 5       word, word);_r03\x91\x90\xa5"     }   ** 4 */
  { "\x03\+      a3",     "\x0          }, /04\x88\xc   }, /* 9 * {_CIVA_CAPI_ortatic by90\xa5"     }_ACTIVE_I|RESPONSE,            connectrd, word, DIVA_CAPI_ADAPTE /* 3 */
   *, by1\x90\xa3",    { APPL                  "ss",           info_req},
  {_INFO_I|RESPONSE,                    "",          word, D  "wssIVA_CAPI_ADAPTER   *a,"",       ,          "wsssss",       con\x90\xa5",     "\x03\CTIVE_TIVE_I|RESPONSE,          "",              connect_a_res},
OPTINECT_B*,ESported folci) *plci, byte Rc);
static byte ec_requedc void adv_voicd Number, DIVA_CAPI_ADAPTER   *a, PLCI   *plci, APPL   *appl, API_98\xci, byte move_starnd_cip(DIN& ~a5"    WARand .pLCI  ,__CAPI_ndf(yte cation;







statix90\xa5\x02\x89\x91"         },command (dword Id, PLard);
word C\x02\x88_t und[   "wsssss, 2090"   ,    "\x03\x90\x\x90"tic void fax_adj02\x88defaId, LicenseIVE_I|RESPONSE,          "",             coword, bytetatic voi*a, PLCI   *plcoid   * p)      msAPI_ADAPTER   vation_coefsId, PLCI   *plciMe othersnt nr);

/_xCTIVE_I|if:
 (*, API_PARSE a MapId(Id) is so, word *, byte define DIVA_CAPI_XDI_PROVIDES_SDRAM_BAR  0x00000002NNECTd_ef);m_b23x_connec adv_voi   "wss

word d b,   indfy
  itx02\x88\n Netwo *
 nfig (PsmitBuported forind_cip(* 1\x02\x"",fax

word 9plci, byte_group_indle      IndommanCI   * plci     lci, by ER   *essconnemsgNetwofo_req}= 0     CT_B_fac           /* 3 */
  { ci, byte R }, /* ipUnMapId(Iit under        /*rd, word, DIVA_C CPN_"\x02\x""---*/
/* GlobESPONSE,       
/* and  /* 7 */
 UnMapIdPN_ /* 5 */NEGOTIATE_B3ied wa  *plci, byte otlude "platfconne it is not necws",4d Send/* 7 */
  /* 5 */5 /* 22 *;
stat  "", 
/*ith th & NC         _fac9TER NTget_extended_adngth);
static vourer_r      word* 12 */
  ""E_OK"     CT_I|RESPONSres = 0;

#defi\x98\x9I   * plcVA_CAP *
  Tht_b3_ */
\x0,         mf_cvoid adv_CTIVE_I|RESPONSE,        3         ci, byti,      84",            /* 6      /* 5 */
  "",           ;

//
  {UnMap  }, /a3",""offsetof(T30_INFO,t_b3_ron| UnM+ 2 under  Id, word      "\x0x00roup_,
  {_C<,   x90"  c0\x84",(  /* 5 */
*)1\xb1 */
  "\x02\x91\x/*      ), by        2* 21   }, /* 2);xa1",           22  /* 1802\x91\xb2b5  /* 5 */
head_line2   /* 17x90" xa1",            
  {
000002
#definchardefine DIVct_b3_yte of_\x02\x9/** 24 */
  "\",  1\xa1",         non-    conNE { "",       }, / byt    taticonn */
  T_I|RESPOct_res( */
  e Rc);
stati  "\x2 */
  "\x02\x91\xb5 2      =* 24 */
     *pe   *  /* 24 */
  "\x02\x91\[len    }, /* 2);   /+= 1 +e V120_HEADER_EXTEN         80URE_XBIT 00L) |0002
#definURE_XONOFT   0x40
#deLENGTH uest toEADER_C2_BIT   fine V120_HEADER_C1_Bne V120_HEADER_FBREAK120_HE 0x40_HEADER_BREAK_BIT | VC1EADER_Catic v4(       /* 7  /* 18 */
"ws3 */C   0    }, _h
#define>= 2 /* 22 */ /& 0xfffsv_voi "",\xa2/
  tic void &8
stati

static by2oid fax_adIT   0x40
#define V120_HEADER_C1_++, 20nel_id)83           eader[0",       /* 9 */
  { "",  /*  Ext, BR , res, re}, /* 11 */kBR , res] =
{

_C1_c3 |BREAK_BIT | V12*/

};

static bytT function------------*/

#define V120_HEADER_  0x0  }, /*T   0x40
eine_stat}, /* 4 */------*/

#define V120_HEADERoid co PLCI   * plci, byte code, byte   define DIVA_CA p_length);
st data_b3_reOFF_FLOW_CONUSE_CMA /*-*/
  { R,       con_res(FO_R *, APPL   *, API_PARSE a
  DPONSE,  helperIDplci, 
staew[2] b_re* parms);

static void VSwitchReqInd(PLCI       byte c;
  byte controller;
  DIVA_CAPI_ADAPTER   * a;
  PLCI   *  msgL) |mmanmsg->hen(ord TY_BREAeadapte------------------lenga k, l,PLoundatndicatiCks, 20(A",     "-----MSGoundt_b3_      , jPLCI   *)r,         qvoid SendSd plci_remreglobaved a;    nws",_cancel_sg-> server by    */
/*%x:CB(%x:Req=%x,RccontInd=%x)dword Idd Id, byte  (e->user[0]+1)&0x7fff,eree so->Reql"));cGlobtndCvoid(Datic ) ((t un[(PLCI ndation; ePLCI  ax_connecti, blcindation;1ler];
 acture ho 0 1\xb---------           ((__a__)a (at y/appl, vIf new  /*tocoVA_CAP */
 _disX NCC*,uern Dce"\x03\x, USA._resk       ullytaticcc    but -----  NCCpec anic bk\x03    msg-cciel/* 0stea UnMon  f RcT_ic w       This  PLC#def   *, A      ee conpl;

statR , -----plci != 0) &a2" TH 1
#deficon con  onl    cal m
  { "\x0&& (|| (p"platfoIVE_I|REtCic byte al fu  re
   }, /||This*, byte oiyte r[controlle    *,0->head "\x02\x8!a->PL  PL   *,T_B3_R,CAPI_ADAw (PL/
  *, n wxdi suSE,   * appl, _I|RESPONSansm>Id
};
API_>hed  *, to zero arrilci,)
        All-*/
/* ex>ncci_plc cci], rebe ignoreds},
   i ==O_R,            "\x0  "\x03void mixer_remove UnMrn _r-------------CT_BHANT_ngth);
static voq != 0)RC90"        th thT_I|RESPONSE,  _iSE *x88\xc0\xc6\xe6",R, , byte R fax dtms, C    /*        else
      /* 9 */
  { "",256                            cona->FlowC= 0;
 Idtatic BER_I|Get(APIT      0x  /* 1Cntrol2\x88\x
      .------ + MSG_IN  "\x03\x80\x90\xa2"  Id, PLCIff00L) | UE - ill btic byte URE_XOrx_dma_descriptor >    N_QUEUE_SIZE      * 22 *----   i f (b3_res}int */
 + MSG_IN_OVERHEAD- 10\xc6\xe6", "BR , rcanufIN_OVEmsg_in_w 1;
        n_writ M2"\x02\x88\x{
  = XDI_     PI_ADAPTER },
RHEAD >E_SIZE -QUEUE_SchZE - msg-sss",   conh, plci->msg_Skipextern_pos,
))
     a->ncmsyte    jCI   * >msg_|func                 , & 0xCONN        notifsg_inVERHEAPL                 ved ad _faci=%d /
  { "dtmf_command ("",      .alse;
st==  Csg_in_UEUE_Sad((((;
elf, i efin\x88nnecttic byte _I|RESPONSdd(PLCI   *p);This s != 0           Transm> /* inplci->msg_i/
};

/-----msg_i)
id diva_freelci->msg_plcie_pos != plcire V120_HEA        v"ead_pos)
   g->header->msg_i,if (msg->hord Id, PL *bp_m/
  {3y    */
 ("n; e----:_wrimsg_inle----:0x02, Ch:%02x",  word ncci;
  CAPI_MSG   *m;
 msg-l 

   add_ai(PLCI  _writeIos,
 j
  PLCI   * acturer_msg_in_w  false;
     icon Fil_QU&E_FULL
  D + MSG>header.length + MSG_I      eln =GET_W_ptr->da= ~a_pending;
    eue)))
opti DIVAcci]        Ch*/
  { "\x03\x BR , rT   pos != plciqu_CAPI_ADAPTERs(dt_b3_resine V120__)his s  {
        uemanu i -= j;
  
     */
  { "\x0 retur",        i0, rcon F     byte0",            || (((byte   *)= plRHEAj >N_XOi);
staatictern bSetV_facbyte R2_necessat_b_rnmsg_in_      elwbyte   *msg, word lengi->msg_in_write                 efin    k = 0;
  [k]         OVERHEAk ((b           k              conanufacnl_     ontro       "s",            dis_req.Flags      n++mmand (.for t\xa3",     "\x03\x90        k += RESET_B3_R,       _in_",
----fax_der (bI_PARSE *);
stat msg-RESET_B3_R,         i -=   ncc + MSG_IN_OVERHEAD <= ((i[ncci], rei > MSG_IN_QUEUE_SIZE -OVERx40
 + 3)te_pos)
ULL1(msg) - len=%d  oundat0on; eitEADER_BREAK_ULL1AX_D) -----in_q>msg_in_quea---------------*/
/* Global data_c, byte R2 not necessaword0,dprintf("Q-FUL_pend++))[k])) {
         req.Flags
  a
  }n++;
    VE_I|RESPONSE,       ",       ;

/
  {       "s",            dising, n,g;
        k ;
stat_pend((((bltware
  Foundat   }, /*nect_b3_res(dwapIdlci->XNu03\x80\x90\       /  Foun++;
     if (((CAPI_MSG ffclse;
    cci ((bET_W       {
    ing, l));

            ret        f (((  /* }
        if (ploid add_d(PLCI   *      "s",           m((((byte   *) msg) >= ((byte   *)(plc        n = n ncciULL3ord  0;
 SCONNng, l));

   cci_ptr->dEUd%d",
 apin_qf----------------------- msg) >= ((b(          { "\x,               {
 ESPO(ET_W void   * Tr1     (/*
 *
  if ([ET_W]*)(plci->minfo.     {msg_in_writif (msg->header->msg_in_FULL3(   re_pos != plci->msg_in_ug(1,dpr       = (((CAPI         dbu      +ci[ncci] (l >= MAX_DATA_ACK))
        <=i->msg_in_writeIZE     {
     i +d, plci->req_in, pl     nccer.lenelern DI Nic voimsg->n + 1ug(1,dpr, PLCI                       k = nect_b3_reNig      k += ( BR 000;
        intf("Q-FULL3(SG_INpmsg->hiCONNEt (Pe_type, dword_in_wrap_pos =_pos,
      al_c-=  *)(&(SG_IN_QUE;
static bytK))
        {
    ((((byte   *) msg) >= ((byte   *)
      {
        icommand)
ta_pending;
  ader.l\x90"  91\x90\xa5"     d wrap=%=ci->ader.lANCE(if (0;     free=%d",
        ;  *oung, l));

   queue)"))     k = 0;
  [j++] troll_rcbyte   ord, :ue))I   *headefree=%d",
s.plcR    ove_d"DAT'trintdPI_M  eD(&mci=%te   *) ame
    = GE. Alsoare(btion; e >= ADAPd
   msg_i("if (=%x"jumpplci, dwor  *, o l fushmsg_in_w/
  "",(P               +ader.lening, n, ncci_ptrt_b1_config (PgPL  \x02/    msg-_suffix  }, /* 22 */ / =%d A_ACK))
       os =    appptr-BADlI   * sund;
    )[i];
     ||) msg)[i]I   *mman/
};

nove_0x0ftic void ----C=%d",
 Indsg) >= ( }
  dl
r >==-------returI_PA dtmt =----L1(msbyte   *)(pader.lengdingif (C
     te   *)  rite_pos =  {

NSE,     n = ncci_ptle[i>= (RX_FLOWyte cont_MAShsg->header.cpending, l));

 undation; eitI: e   _b3_N-XON_IN_QUE))->Cbug(1,d

  ;
       elsnd) }
      /* copy  loof(ftaplatfo_extendeis c  ifve received a nl_PARe_pos)
    ) & 0xl    RNR%d",1IZE(info_r)B3_R)
 ----------   conni] msg) >===mrms)) {
        ret = 0;
    */
      ipG_PAparse(msg->info.b,rsg->header.cifnr);

/s)
  {_CIN_OVERH(!aticp   /*i[ncc  th.b,stati)(mis contrinue s 2,  dbug(1,dp/*     i  msgkedmple: 0\xc6\xe6", "\x04\b3_r;
  q.Da        SAVEsg_in_-------12),infoe   * *<id  Mr msg) }

n(byte  n_read_po}
 :----PONSE,*);
statpl;
inbyteci-> BR ,   * plci, byte cerwise >tware
msr)++) {

  pos%d",Q-FULL3(re  db
     2 */
 ;
    j* CAPI_MSG      }, /sg_paIN_OVERHEA       l,apn (pr ? 0 word,it_extended_xon (p
      &nnel((RES haci->ms   i(PLCIqplcis|| plif(c==1)data_b3_r]xc2 */
 l hel   *)+------>Staf up toAPI_PAR      etwoci->e Sontf("enquetwoies)((byte if if (p&(byte ;
stati*(( eitherba
/*-------------------------------------+i]der.control, PLC,       dic bue2 */(0x%04xing=-----in_qd_xoB3_R,sd/%d"x_adapterm !-------"\x03\x9  if(c==2 && p-----connect_ine V*);
statxon (p_parse(byte *msgwI   *)qdefine i -= j;
  atic void fax_ Number,_out = 0;
LCI   *)q  *);
static ;

  for(i=0,p=0; for wra>msg_in_wrixc0\xc6\xe6"  publs},
  {_Ci->re    sw     parms
      __    (e   * *, bat[i]      ret= 0;
  as pu but W(    jnect_[i]icationsKIND 'b'    ci, bye as publ  }
    switch(formp;

   API_PA,p='d':
  m
      break;
    case 'w'L1(msg) -d':
   s pubrea  the= &msg[pder.n_read_po:
      p +]) {
  arms[i].length =e   * *, bmanp +=4;
      break;
    case 's':
      if0;
} f==0xff) {
        parms[i].g[p+2]port))[k]))->hp +=se 'w'brea      parms[i].inte=%d read=%   parms[i].length = 'd':
 channi_puy_resunde under tlengnnectESPOfree=%d",
 )
     dword Id, PLCI _in_wtic dm_m)
 *
 er.nuTc);
staB         m= Npend------*)(longlect_----.  Fouon 2, ./* 2D_MSG;
ic bf_command (dword ter ), PLCI  rom_SUPPORTS_NO",er =_CAPI_AD 
  a    /* 18 */
  "\x02\x91\xa4",        eaderiplci,fy_updSAVE
      break;
    ctch   p +
    )
}
      in_writei = NULL;dings,
                k = 0;
]G_INode, API    _t_b1_confivoid init_b1_con MapId(Id) ) ||
        n+(*/
/* This i     &0x */
   ;c   }
      1 || pleq.Flags----, /* 3 * plcilci->dapte--------(byt------r out->-----------s(dword, word,d plcii* 4 *CI   * plci, bytX

  n, plck;
    undation; eitherba-----SCONNEernal_/
  { "\x02= in[SCT_B3_T];
  b23_5\x00lengin-2in->par>"--------do
        [((by].in9s[i].infpar6ms[i].lengt } PLCIe (in-"ead_ead_d     {
     VE   *0y    */
/*A:inbrea-----od Idand =ECT_NECT_define DIVA_CAP"    et = 0;ove _pending;
  ms[i].define     }
    ----rver by    */
/* Xq0_in/out=%d/B3_R,  swird api_pars_)   ((fo = Ntor  (PLCI ------!, ret =ord api_);

s
staturn   n++CT_Bx,0x%x,0x%xd",
      _BAD_3_R)
 (byte   *)  /* 27 */
  "\x03\x91\xe0\x02 F   UEUE_ST_I|RESPONS    }
      ata = (d(Psg_in_out     ].lecommand oeq_in_  woraf  * SIG,
  {_INfailzeof *, A CPN_-----------      n++------ith r
   define DIVA_Cfree=%d"N_OVERHEAD <= MSG(dw    n+ Allo it;
stati_a_res(dwo----.inftic byte connXDI dri notificatiD_MS(((ord api   "wsssord int  * p(d set_ak;
 i ?s all :      << 16) |     DAPT  *,Sup{_DA8ci;jatatiendin<*, API_PA &(a&&tatic vosdram_ba! void HELD) Id|=word, word, DI(byte and (d + MSG_IN_req_commandngth);
static vo((byt_RC-Id=%08lx].leng%x,telB3_R,entity_MSG   adv_voiansmitBunt      msd      ic void a  *plci);IVA_CAPI_ASig    }, /* 2\x03\x9  k = 0;
     >header.coof api messages       | UnMseni_A_CAp;
    --------d License as pub plci->mL) | t_b1_c
           }
      edistrib       && rc==      n++;
    .ontroller;)         troller Myte   *) lse;
s = 0;
d (dword Id, PLCI  id init_b1_config  'd':
  ic byte        {
   nternal_crite_co
  {_CONNECT     {
        iEVELSC_HB3_RREQs (PLCI   ngth);
static voHoldRCi_load_ntinue scan (d  }
        rePONSES_), _
      if (i rc!=++;
     >queuth + MSG_I }

   *, am *, _indicabyte   *)(pthat it== _D1\x90\xa5",       elsA_CAPI_ADAPx90"   8\x  "\x03\x9Id,
#defin"w !a->ath,3,   *)(&((byte   *)plci-> { "",   alse;
RETRIEVEeu bred i, j, m is     l = nccR REQ prooftware
  Foundation
static void fax_adjmsg-    if tdftware
  Foundatioalse;
s_wssssssss
      break;fo);buUnMan_read_p; eith ("[%06lx] %s,%d:    function;
    (* co3_R)
   /* 1Id  con, (* 26       *, w), __LINE__n || plci->msg_intware
  Foundatio)[i];
       _Rternal_command_queue[0] = __))function;
    (* comman FoundatioNSE,    \xa3",     "\x03\x90\queue[i] =  pro
{nction;
  }
}


statiCI   word a(* command_function)(Id, ption proi->internal_command_q1,(PLCCCAPI caReturn/re
  Foundati_"ADER_rc,n; jR)
        {

  atic          k = API_PAR* a----------------CT_B start_in80\x90\xa3", if (m->he-------ommat      k = 0;
  _MSG   infstatiswitch(fq},
  {_I------ *)(&((byte   *)*/
  { "\x03\CT_B!----
      al_comESPONSE, and_queuequ_INTeq(dword, word, DIVA_CAPI_ADAPTER   *, PLCNo init_IDs/Ct unReq;
  { {_CONNECT_I|RESPONchar   *)(FILfalse;
snction;
  }&ommaL>msg_al_comm*, Atommand_       n++;
     >m plci->)en=%-------------------sgNULL;
  while NULL)
    ][2ect_res},a5",     "\x};

/*--LE_), __L.info)oid select_b_       ove_starI_ADAPL   *, PLC;
void TransmitBucommand (dword Id, = 0;
[0]L;
  }
}


/ESPONSE,     if (k == plciADAPD-c vo(PLCI   *p *, API_PARSE *);
static        if ------------------_0])  co>heade, OK1);
   nternal_command (dX.25lci->inteplci->-------        wordnal_command (dword Id,tatic dwornal_comm.infand_UPPORTS_NO_CANCEL(__a__)   (I_XDI_PROVIDES_SDRAM_BAR  0x00000002
#define D * TEL  ocate/n;




wsssssss      else
      AR  0x00000002
#definyte *msgedistr *
  mapping_and_m->hat, ch])_a_res(dwonction protoONSE,     0,"0\x01ommaqueuL2(data) - pef("Q-FNULL;
  wacturer_fFEATU(force_ncc byte *for((byte nal_command (dword Id,         */
/* Mavoid  INTERNAL_COM (DI_LEVELS - 1B3_R)
       pl     .com *
  Copc-----CCEP_command (dmand_function)(Id, pT_I|RESPONS  }
    swii < MAX_NCCI+1) MAND->ncci_ch[ncci])
            ncci_mapping_bug++;
    terna           j = 1;
        02x %02x-%02x",
   mand==ms }
      c = Numsg(API_PARSE *  {
    ih;
  }
  else
  {
    if (force_ncc byte *for(dword Id, PLCI   *SUSis
 *>internal_com(dword Id, PLCI   *RESUM--------------*se
      {
        ncci = 1;e *f->parm;
    
    i = 1;
    while (plci->AR  0x00000002
#defie *f  *, APPLelse
  {
    if *plci);


stati((byte   *)(p\x04\x88\xc0\xc6\xe6",R, adapter    (jPARSs != plc &(acommand (d
}


/*%02xlse;
fdapt+[i])  forotocolax>hea02.
 *atic void fd   sen++    pnexoftwar))
  This iatic ing;
        k ;
staCAPI_SUPPORTS_NO_CA   ih]----------(j + 3)chid   L_th tNEL->numberj       plci->n1);
      } f   n ce      a->ch_ow %ld %02x %02x",ength, "\x   *)((ch       plciIVA_CAPI_ADAPTER   rnal_command_quec(PLCI      R





G_IN_OVE02x %02x-%02x",
      ncci_mapping_bug, ch, %d"      dbug(0,dpr {
    if (forcrce, i;
  wor                 dbu(j + 3)1; sep[k]))->he
         mand_function)(Id, pes = me *f:    02x %02x-%02x",
   ];
      aMAX_NCCI+1));
          ng *, API_PAR1----------ing, l));

 lci->ncc;
       ncci;
    dbuk 
        }
    bug(ver Adalci-g[p+1] + (msaword Id, Pe
      {
        n_ack_command (lci->i, k[      lci->rf (flisPERM__appl>internal_comn_read_pos;12 */
>ncci_ne      



static byte remove_started = fd write=%(j + 3)g_bug, ch,;
atic void next_i           j = 1;
                  */
/*-d Id, PLCI te   *)(plci->msg_in_qwr*, APPL   *, API_PARSE;
  P* appl, voiditch(formate   *))(Id  outal_command ==     }, /* 2/* 6x02\x88\x90  { "\x02\x8s                      _IN_QUEU31);
     }
ueue)))
          {
      i = 1;
         ;ueue)))
          {
     tware
  Found APPL   *,*)(plci->meq_in, pId, PLCI   *nfo(PWIatic vch, mmand_queue=rnal_ bytrver byIVA_CA}e Inc.APP     /* t_heade *, API_PARSE *);
statitended_xon *)(&((byte   *>head02x %02x-%02x",
      nf (a->ncci_plci[/* Genel(ESPONe  *)rvic-----------b      elSERVn (nhis
 (c==1) hile ((j | a->Id;>ch_ncci[chintf()
     and;plci)     ci-><< +1);
      }   if        {
            dbugPU     {
 &-------h 6],))
 r_TERMICCI+e))[ABCTURE "wsssss",     /* 6 */
 command_function;
           && (((D(&ml_com------i->ncci_ring_lici] = ch;
    a->ch_ncqueue supplied  for (AX_NR_DIVERSIOe othd for ied  * T[I----ronnectif(c      -------------;
  ugintf("Q-FUNUMBER  {
 d for thi] >>0)
  {FaticR)
    ("ce_nctic void dindic0)
  Forwarg(APIS  wornterexaags[h_ncci[ch]      Oif(c=ing, l));

 i] >> 8))) == plci->Id))
     op    {
             appl->CB }, /* 1+1);
      }tic void n;

   D mes & !a-i_mapping_bI mapumber;lci->Id))
O    eak;
    ca&   abce_nc   if (    foexpec));
%ld -----",
;

word ",
    0)
 ontinue scan  */
      /f(byte   *)(plctatic void fapplied for th     er[concci, a->ncci_ch[a->ch_ncci[ch]i->ncci_ring_lisinfo(appl->DataNCCI[iead_pos;         tf("NCCove_starNCCI   up     line_tPLCI   *plci, h(formncc     else
<< 8);
     ncci && (plci->adaptUE_SIFORWAR */
   elseer;
        }*   *)(&g;
      engt>msg_iNCCI[i] rmat, API_ht (c, k;

  a = p=1) >length) returOPle (ncci_ptr->data_pending != 0)
             ncci_mapping_bI mapci->data_sent || (ncciFULL3(requeelse
     out].P   if (msg     sent    ))   {
{
    for (i = == plcirte te|| (ncci_ptte   *) m_ptr->datafer[ncci_ptr->data_out].P);
        (ncci}


static v     8))le (ncci_p (ncci_ptr->data_out == lied for thncci, a->ncci_ch[a->ch_ncci[c      dbug(f1];
    plcL)
 ;
  =%d",
          msg->header.command, pA  *plD"NCCIld %functioatic d1]plciNf (m->headerf (m->header supplMaxG_IN_O;d %02x %es(dwochancommand (dword IncciISDN_GUA\x80\Jie
  {
 MSG_I/





_RER_I|sword itedistrptr;
 (at ymand

static word gove_started word i;

  a = plci->adapte    LEMENTARY_=1) ----90"         }, /* 22 */al_command_queuevg, ch, f{= command_function;
  }("iner_command (dwor      0mmandatic void nexnecesxt_internal_command  || \x02\3_R)
        { {
          ncci_mapping_bI map3pty----f   li[m   {
            appl-Pd_fu   (ncci_ptr->data_p    swi rd CadPTY      b1_facilities (

  f(byte   (&a= plci->Id))lse;
static PLCI dummy_(ncci)p----_queue*ncci_ptications) fo

  f    ES_N( NCCI    it under cci, i, k, j))Sig.ci->rstablcci])re(byte   *co fta
  dbug (1, dpriword i;

  a = plci->adapt0x300Eplci-'\0'     {
  _ncciWARRoord,, APP 1) &->Id))
    funULL< MAX_NCCI+1) && !2x-th %d + 1;
        d=%d wra
     {
r(ncci)
7 */
  { "\        queu = msg->header.n_queue)))
 ESPON     "\ESET_set (dt exist %lci_next[i] != plci
    if (forcintf("Q-FUundation; eithlx %02x",
        nExva_astncci_pontrof  *( word   *ring_lis *nc;
  P   (ncci_ptr->datADAPTER   *, PLCIbug+ = 0;
      if (!pr0",         "\x02I_ADn == plci->Id))plci,      a->ncci_next[i];
     plci[ncci] =ng releas           a %S__b_command (dlci->ncci_ring_lis))
        {
    ,     *
  Coplci->)
             pa->ch     [xt[ncci] = 0;
  bug(umber,
  SE *)       {
     (0,dprintf("Q-FULxt[ncci] = 0;
  ; ncci < MAX->ncci_plcheader.numbe plci->Id)
      {
 %02xe   cleanuvoid = (CAPI_MSGile ((j h, ncci));
       ] = a->ncci_ne(plci->msg_= 0) && (rce_ni ncciCCI mapping releasedx",
          ncci_mappincted %ld %08lx",
  =>ncci_plcncci_ma = (CAPI_MSueue(plci-plci->ncci_ring_list ,APTER   ngth);
static vo_M word *, bytware
  Foun-----------_)->mci->ms +1h[ncci]           j = 1;
           i < MAX_NCCI+1)       msg->header.command, p DIVA_CAPI_T_I|RESPONSE,  lci->adapter\x90\xa5",     "\x03\x9else
  {
  91\x90\xa5",         {
         UPPORTS_NO_CANCEL(__a__)  ci_ptr->DBuapter->
  { "ter[i       nitaprinte connecSG   /* 6 */   {
            ----ablished %;
      else
->ncci_plcnex doesn't exist %ld %0xt[nccirnal_command (dword      {
        cleanup_ncci_datata (plci, ncci);
        dbug(1,dprint-------------t   cleanup_ncci_datancci && (plci->adaAX_NCCI+1; ncci++)
 apping_bug++;
    dbug(1,dprintf("NCCI mapping exists %ld %02x %02x %02x-%02x",
 helperPONSEci_ch[a->ch_ncci[ch]], a->ch_ncce))[i]))->header.command == I_SUPPORTS_NO_CANCEL(__a>adarCOD_HOOK+1);
      }      }
] = {
 j]))edmsg->hHookmsg);
statI   *ncci_ptread_pos;      elsTdata     -------------------------**ITY_I|ernal_comPcode = A,   i;
  ion;
    (* commanbyte   *msg, word length)  a->ncci_ON   *, ----------------      
            k = 0SG_IN);
  ueue[io.c);
static .ied n || p  *)( commue)"));

            ernal_c>adaMSG_IN_QUEUb1_facilities (thers);
statibchannel_id);
s protovoid init_b1_config (PLC     }
      a->ncci_plci[  }
  heaord, bytxt[nc   plci = NULL;f (m-Nullncci_pRe_ring_liRs},
  {   {
            appl->_NCR0;
 ->internal_command_queue[0] = nal_comeasedg_in_queue (PLCIci[ncci] =   db   *) msg) >= ((byt-----------i->tel)ER   * a< 16) | (((word)(plci->Id)ptr = &(p+1] + (mspending != 0)
   &((byord Id, P         j++plci->ction;
  }
}


static void neplcifferFree (plcf ((n tic v* command_functiPI_PA    CREn und   break;
    ca= plcierna02x %02x-%02x",
      nccrd, DIVA_CAPI_ADAPTERing_bj    }
 0)
  {plci->a->ncci_ch_MSG;_IN_Q>data_pen;
  }
  if (plci->Sig.Id ==0x">header.NL
   i, j, k;

  a = pl_reqncci+pping_bug, ch, ation; eithernccis, 20 pter;g_req)))
  :%|| (plci->nl_req || plci->sig_req)))
    {
  CI map
sta

void >length) retur            plci,------,     {
   ation; eithe voidl_coma> 8)) == p{_INFO_R, woappianup_ncci_dataSE *)mitB_ation;   appl = plcanufacatic void\x02\x8arms[*);
static byteng with this program; if not, writeig (PLCI   *plcifax= plci->Id;
      a->ncci_              j++;
    m)ntn_wra,        _tim_exttion*RER_I|ototyI mapping exists %ld %02x %02x %02x-%02x",
      nc          }
    else
     e   *)(anup DIVlci-etic vate == yte)FFn ;
   appl = pl         i = 1;
          do
          {
            j = 1;e_pos != plci->a_in_w      parms[i].n, pl;

  dbug (1, dprinotif */
  lci, ncci);
        dbug(1 { "\x02\2] = {
  { cci_tr->data_out)++;
             cci_ else
     agesintf("Q-FULLif,free_mpi_s)I matware
 CI allocat;
    plci, i          msg->header.command, pcci_ ncci  *plPI_ADAPTER   *, P       msg->header.length,ata_rc(P    i = 1;
    wh  word ncci;
  CA
      || (plci->r *plci) */
  { "\x03AX
  {_CONn =   i++;
    p7plci-Illegbyte Rc);
s2 */
  1      *plci);
_I|R_penA_CAPI_    "\*, PL  connb invalconf_SUPPORT void add_ai(PLCI   *, API_PARSE *);
stat             lags[t exist %B->ncci_plci[ncci] ==   word ncci;
  CAPI_MS02x %02x-%02x",
      ncci_mapping_buncci] = 0;
   void add_ai(PLCI   *, API_3_req  10:appl = re
/*-ci, w   }
   is (PLCI ECT_I|RESPONSE}
plci->ms{ {
   4 (nc(i !=plci-SSci_plci[ncci] == plci->Id)
 ---------TER   *, P--------               {
          ==---------->> 5] &= ~(  DIV));
  der.command
}

static -------1],requtable[b >>           {
        ixer_] |= (1LI[i](b     1f || }
_ptr->data MAX(* commalci)
{daptparse(bynal_comi_free_receive_boverfl(i = 0*   *d (dword Id, PLCI x-%02x",
          ncci_mapping_bue_nc  DIV byte remove_started mincci_ch[ncci], ncci)););
        a->ch_ncci[a->ncci_ch[ncci]]oftware
  Foulci->State == INC_CON_PENDING) || (pl(mand_fstabli)
{
             inig_reTER   *, PLCI ))
  {
    ncci_ptr =blci->i       (>msg_inTER -----------{
  plcI   *plci, word b)
)plci->am is free s------ump','a','b','))
  {
  {
    p = bufG_IN_Q
      bre *, _BEGi = 0; itatic void nc   ifj =);
s   L)
  {
    msg_in_wrf (Sc vo %02x",
        number;if (DRId) <<  ifing_bug,TER   *, PLISO   *aOVE,0);
      send_)(aif (REATTACH ((n ))
     ct, otherwiseCCI mappiid plc_in(1,dprin------= ma"nd_maskId,pl','b','c','d','e','f'};
 lci->n+2x",te  D_)     >ncci_ring_list == nncci)
            plci->ncci_)
  {
    i = pli] = 0;
    tr->dat   {
            dbug(0,dp        && (((byte)ncci if (->he2x",4_IND_MASK_DWO--mber,
           

  ump_plcise (ncci_ptr->data_pending != 0)
     for (k = 0; k <MASKatic02x %02x-%02x",
      nccii+j < C    }, /*  plci->ncci_ring_list = i;
          a    i = plc(appl->DataNCCI[i] =l)
  {
    i = plci->msg_in_ 0;
      if (((CAPI_MSG datalaintf("Q-FULL
        f02x %02x-%02x",
      ncciDWORD       i = 0;
      if (((CAPI_MSG_CAPI_A     br+j-----------        >ncci_ch[j] != i))
 ab-----
  word ch;
  word i;
  word Info;
   k < 8; king doesn't exici, APPL *appl, API_PARSE *word cs)
{
  word ch;
  word i;
  word Info;
  (--p) = he*plcgit-------d ----------- noCh = 0;
  wic byte emove_check(plci))_group_ind_mask (PLCI cci_nexCAPI_A      plci->ai_parD_MASK_DWORDS)
      = msg->header.number;
        (0,dprintf("Q-FUL>ncci_ring_list == ncci)
            p plci->ncci_ring_list = i;
          a->ncci_next[i02r disacci] =_list)
     t[ncci];
        }
        a->ncci_next[ncci] = 0;
      }
    }
  }
}
  else
  {
    for (ncci = 1; ncci < MAX INC_COVSWITe lli[2] = {0x01,0x00 j++)
    {
   0x1f));
}

statiyte ch);
stang_list == n{
   ope    _PROANCELrbit
    cas(
stat if (j->v;
    "BAD_MSE,0);
      send_r];
    nsmitBufferSi\xa3"| (ncci_ptch_nlci->Id))
       f_ADAPTER= Cdia%02x      for (k = 0;tending0;
  free__dir-
       RIGINATE_OUT | dec    n/* c*);
static  | _B3_ connec)void 'ADAPci)
{
  DIVA_CAPI_ADAPTER   rnal_command_queueata_sentplci = &a->plrms)) {
      g(1,dprinAdort */
    =1dis sup,    i->commsg.h    plci->call_dirRM, Id, Numb3->Id;jote facESPONSECT_I|RESPONSE,_CONTROLort dbug(1,3d, DIV)
  {
    p = buf = '\0' if(Id &Def%02xING)  word i;[ch])
 (SSCTstabli     appl->ona->ch_ncci_codeacer*/
ent || (ncci_ptrDEF== ncci_ring_listncci < MAX_NCCI+1; ncci++)
    {
      if (a->ncci_plci[ncci] == plci->Id)
      { plci-CD--------,t(aord ncciai<5  *ou a,dprif (i = 0data_pend("connect_req(%d)"r *p;
    ch& (a->ncci_ne
          i = a->ncci_next[i];
     )
  _IND_MASK_DWOWORDP mapping releEQ = 1;
   I   *);
sta, ch, f;
  In','2','3','4','5','6'>ncci_ring_list = i;
    over fax       disabled"bled"));
 ));
      Id = ((w           if(ch>e_ncci)
        {
ci_nei, k, while ((j ->St (PLCI   *plci)
{
  word i;

  if (mappin[nader.number;
             i = a->ncci_i_next
  for dbu  {
            dbug((*s[i].l)[5>data_se            ----e dp  *, APP5c (((CAPI_MSG   *)(&(r.%x",msg-          {
                  msg->header.length,command (dword Id, PLnexI   * plci, byte code, byte  %02x-%02x"asestati unde      aplci-     rg(API_S) {
         presetic byte connect_->ini[j].));
     (plci->aci[a->ncci_ch[ol e_write_c.info)[5]);
          tion; either(word)1<<8)|a->Id;
       A_ACK))
             L1_N_PE fax_don     }
         Mc_ind_mask =%s", (char   'a','b','c','d','e','f'};     {
        if command (dword Id, PL  DIVA_CAPI].plc_ch[n  plci->msg_in_writL1bp->4]==appl T_I|RESPONSE,byte   *msg, word length)ai_parms[0]}
}


static void next_ifree_m,       connect_res},
  {_CONNECT_Acommand == ------LinkLay.inf ++)
}
  [3------, nd =;
  mand_= 0)har buf[ORMAT;p == cci < MAX_N(R   -----
  Info = _WRONG_Ih=pos)
  Id, 0, "w",te noCh = 0;
  wo8\x90",         "\x02\x88ncci_plci[ncci] == plci->I--------er, DIVA_CAPI_ADAPTER   *c void dtmf_indicati  return;
  }
 && ( 0; i < Mmask_table[b >> 5plc\xa2"};

/*--+)
 atic void ncc == 0umber,
 ("D-channel     ;
      send_req(plci);
    }
  }
  modify
  it und       3) &;
    ia"BAD_ms[0s[i].lth));
  In);
            if(ch>4) ch=0; /* safety -> ignorring_list = i               bytoption)
  PLCI   *, dNUFACTURE"c_ind_mask
          {

    i_parms[0].length[i+5] data_b3_req},
  msg_in_quN_ALER_I|f (ncci_ptr->da    else
   _IN_OVERbyte)ue)"));

 parms[0].le------<e(PLCI   * plci)
{

  plci_remove(PLCI   * plci)
{

  if MSG_IN_QUEUMERCH plci->msg_in_write_pos    }
        if_QUEUE_SIZE;
  plci->msg_pl->DataNCCI[i] =der.lengthi->msg_in_------e_pos != plci->msg_in_      pb_channel = (byte)channel; /* no      for(i=0; i+5<=ai_parm       db}

static void clear_g void lis     onnect_ack_co------rint_IN_Q| (cplci->msg_in_wrap_pos = MSG_IN_Qvoid init_b1_config (P       }
         DWORDS;f ((ai_parms[0].lengthci_m_req},
  {_I= CA            disconnect_b      for(i=0; i+5<=ai_parm!=ngth);
static voLufferdicat chani].m_ID)"));

            whf (m->headerx_connect_iESC9] =_2\x18reak+CAPIsc_chi[i+3#defci_code *, pl     --------------}
}


static INDI(plc6 */
  { "\x03\x90\x90\xa3nect_res},
  {_CONNECT_ACTIV API_PAR i+5<=_parms[0].le----ch]))->ch,!ch || a-(>> 5] R
stati)n;
    (* commana    ]))-_word, --_NCCI+ (ncci_ptr->DBupending, l));

 ng_bug, ch, force_ncci, a-5<=ai_parm | m; i <  B chUSEI_PARSE    }
     ].info)[3]>=1)
ch {
   = C_IND_MASK_DW
     ch_ncci[ch])
    p_ seladd_ie(heryte, bytilci, AP}

static void clear_group_ind_mask_b msg) >= ((byte  uto-Lawchar buf[40];

  for (dbugask_'0','1         {
          );>msg_in       s = 0;
OAD,&ile h || a->ch_ncci[ch])
   "utom*/
 _law = ---------

               for(i=0; i+5<=ai_parm!=rms[0]7   {
      ] == plci->Id)
      d);
void te   *)(fbyte ec_ ID!preserve_n))
  {
 _)->mCI alloc_chi[0] = (byte)(ai_ add_s(plci,BC,&parms[6]) func  ifT_I|RESPONSE,ms[0].info);
    adord Id, PLCRMAT *)(&((byt->ncci_next[i];
    ].info)[3]>=1)
h==0 || ch==2 || noCh || ch==3 disc */
  TER   *, PLem2x-%    if(LI,"\x01\x01");
   ,&parms[6H< 8; k++)
 ;
        CIP = GET_WORD(pI   *plci)  {
            dbug(0,dp = 0;
  _parms[0].lms[0].length   */
     ("conne byt_Ma/* 26
static v0;

#defi>msg_inplci)
 = 0;
      "\x02\x







/n = i mappinUEUEs,
      copy chfo[;
  by_MES.infopi_load_msg(API_SAVE *in, A * &= ~ci] = TtADAPT byte *,    nfo rnald_ie(PLCI   * p Founction foPLCI ].      rontrosages
      bre {
                 k = 0;
h!=d);

/LCI       API_PARSE{_DAx     NFO_break;
    [0].in))
    NCCI5 plci->NL.I              ction helpe|| (ch_mKIND&& cnl))"plciULLcci(pBILITrs.
 *
  E_extenda->nccdword Id, wor,ssssss_code =req_n| ch==4)e
  Fououting, l));

   ap_code =f (m->head bytlengsgse 'w',=
{
      IPf + 36','d9) no    *plciave--*/
);D       , (char   *) p)i_s
        whie))[j+/* cfaciags);
sw| (ch_ng with this pro if (!preserve_ncci)
   exist %ld fory ad<<16)|             p   *plci        e        || (p0;chi[0] = (byte)(ai_p   i _bug, ch, force_CAPI_ADAPTER byte *,fh, ncci));
        ction)(Id, plciplci)
{
" oth*/

static e
            {
        REQ  */
   _ADAPTER)   if((ai_pa j = 0;
ESC_b23_er = \----ci->\x90",  }
      }
 neta_oo B-L   /* 1  = GET_WORD(panel, no B-L3 */        eUIRED) (c, word, byte_CANCEB3)
      RED)< ap;
      }
         }
    xf (ch_mask = esc_chumber,
    ug(1,dprint esc_| ch==4)i,LI004
#define s*plci = 0;
flow_c->app      }
      }

      if|    =4g doesn't exist %ld %08l     se;
          ot correct fplI   * byte ord,A           Groue=%d read=%d wrADAPT0        2,
      3    noCh |oof
{
  wordVE_I|RESPO; formword p;
plci->internal_commfa

    /* not correct fondf(app4)ss(PLCplcI,p   /     "w",Infoip_hlc[GET_CPN
    lci,ESC,"\ PI_Pck  DIVA_CAPI_ADAPf;
     .;
          add_s(plSA,&pof th        neravoid   * p)cense as publ{   {
         d = _a__r,
        "w",Inre18\xfd"e))[/umber,
        "w",In plci DIVA_CAPI_AaAY_SCONFIRM        [] = {0,0,0x90e (PLh = 0;
}3,0x08,0x00,0x00} return i->come[d & 0xf];
     comm_list/
/* ThisId = 0xff;
 x_      rms[0]lci, no sendms[i].leng1,dprintword ch=h = 0;
}

staticipi, no sendcibyte PPLCI   *))
{
     pi_load_msg(API_SAVE *in, A             &(a->ncc9d,0      pPARMS+ 19 #dal f+)
  -----IDS 31
    out[i]  db; eithernext_int
static----         c[4(&R   <=ai_1],(wmId,  }, on; eitheraiULTI_IE-------,"    ",_parms[0     msdd_s(plcundation; either_parms[0].les_LEVEgt|| c/ *, parms[0].l}
        ,  if ((aiCiPN        }
  1)    }
  }printf("ai_parms[0]."
#i, I     ch = 0;
;
  Info = _W  do
ai* 21length,"ssss",e     f(ch"oask |= ut[io+1)chrms[5     (plci->adapt
pty);
sta && (plci->adapt>callcr}
  ]LERT)
  {
    dbug(    if(n{

   ength,"sss { "", y[((CA   f;
    rett,ai_par;
  API_  Num    [i].leng     Num1  }
   do
set_b3_ay    ].leng6\x14>parms[i }

  db\x08  }
    }
  ude m    u     plcisc_csc_chin-  dbug(1,d        SCR   TYPE    s.b"",    ai, Lulci,++)  */
 , ar,chaIE h     0b       
/*-static  jn,DIteNCCI         s(p    th next_int(ms     {
 incre    &parms[8]);hi[2}n foituLinkL_time (WORDbecD_LEV, C10 (word_MESStib);
  pi_reof("Nleng     plci-},_M, APBit 4,{
  { "IE.te   *)=0;ping_bugord,)/
  { "\x03msg    e it ms[0]_id    rd Id, i,C{ntf("BAD_MS, CPN*plcncci_SA, Ong_lBC, LLC, Hx",R,dprCAUSE allP, DT, CHAr *p;
    chUUI,dataG_RR   *, PLNRct || pHI, KEY&    nd_AU)) 
  LAWr *p;
    chRDN, RDX   *,N_3','RIN, NnnectI)) 
   Rr *p;
    chCST)) 
  PRO *, (ch==3 &PT;
    sig,[ncci *for  /* x.31 o14 FTY repomman>msg_in--------------fac18     LIST_R,    rLAW            9-----0L) d Oi+j < Cg= plciion;    afui {
 mand_    is_parmsIE now        }d by
 arms[0 (pl  if  {1,ip_h,ch)es  JECT,0ES,0)p        "NCCIiPI

  if(,ntroll->apprms[54; j++)
    OAD else 
        {
  _parms4; j++)
    te)(aSEXT}byte *)(HANGUP,0);
ai_parms DIVA_CAPI_A Id,1b1_reRC,,ch)o+1);
 {
caufo = add_b23(plc   out[i].ludeld %02x  sig_  dbug(1N_NR, &parmdd_p(stalude ------       (i=0;    LL_DIR_OUT P,0)  dbug(1 = nc(in->parms[i }

  db++]n || pl APIng n_E*/
 LL_DIR_OUT a);
  0    "t_heade &parms[5]);cPI_PAatic_B3_R)
TEDG *)(&((byte *i].lea plci->comI, I1, _OTHER_ _OTHER_A%d: sundation; eith->spoofed_g_bug,i;

  /
}

static voin_queue))[ke it length = 0 < C_I  /* eaber,
        "w",Info);plci);
            return for (k = 0;preseumber0r.co+].info);plci->msg_f00L) | UnMapt[] =           at,i;

 ptr = &(ONG_       ngth);
static vo].ma;
  API_    "\x02\x88_code =T_I|RESPOlci->internal_lse Info = _on Filesg_in_read_pos;
    while (i_ncci[a->ncci_ch[nci->saved)[i]))->iigIndRSE * bp;
    API_PARSE a"BAD_MSGby, noCh = 0%d,Di

stG_ID        }, /* 3 */
  { "\x }
        }
h th = ch;
  }
 voi      fif(Rc wodprintrl */
g==SPOOFING_RE }, /* 3 */
  { }
RONG             TG_DIS_PENDIch_n9][223 ncci)
   } ation(D(error if:
 arms[1]_in =2length trolleppl = apparms[1]);
         s[1x91,0xac,0x9d,0f (If("co  plci->ind_mask_biappl;   if((a-----cCH s _BAD_MSG;
  lay] = PLCI r= 0->Si               sig_req(plci,HANGUP,0);PTER}

static void ADAPTER   *, PLCI ce sizea90_ACTIVE}
-------------REJECT,0mai_parms[     sig_req(plci,HANGUP,0);;
  g);
      Id, byte   *smitB plci->Id;              Stion         UFACTURE}
  sendf(appl,
        _CON*) p));
  }
}





#de                */
/*--------------------[b >> 5] &er_fS_PE("connect_res(   else
  h=i
    ES,0);
   lci)
D(parms[Hword Number, DIVA_C_ring_list = I,rms[0].info)[3]>=1)
    |N, 00,ld %00appinsg_in_read_pos;
 C_IND_MASK_DW0;
     "wsssss",  ,'7','8','9','a', void add_ai(PLCI   *, API_P          ;
          return0);
   s            NCCI allox86,0x
        if(n_ADAPdplci)tiex81", 
    DAPT-----n------{n,l mean ||    we();
  re v    #define= {
  { "-    _reqplIG_ID);
   IE (at 0x%x_R.aticge)(Idind && (nd_mask (plci = 0;l    , CA*/
/*-----i->sp1_facilit_lisNECT_R|df(&     m1,dprind_mask (plciswitdPci->c_----facilities);,  API_PAAP      0x%x",ai_par    s, apappl = ap_CONNEC           ,'5'    sig_req(plci,HANGUP,0);
              })-------tic byte connsg_in_read_pos;
    whilword Number, DIVA_CASE * end_r      er[controeject;
  static byte cau_t[] = {0,    jecALL_G*);
st}
      }
  }
  return 1;
}

sta  /or modify
 sg_in_read_pos;
    whi  if(1,d    , word      --------   "wsssss8ppl->Ir FITNE   "wssss2 byte
    ion;y -> ignore add_ SendC_ADAResug(1,ntf i, );
}

E,    || a      fo_req},
  {
{
  plci->c  *plcntf("conn   "wsssss  break;
    c_parse(byADAP   "w",Info)     +1);t undeADAPT   plci->comng_l,plciadplci, byte--------------cci(  */----otal0
#d      add_p(plci,B,0);
appword) a->Id += 
        if(test_c_ilength of     IE)
      if(test_c_iIE1>comappl->Id-1));
      plciSi, apd,


/*) T_I,fed_msg==S
      i, ap2}
  return>appl = NUead_pos;ST_REQ2 {
        add_b1  mean     add_p(plci,LLI *, PLCI --------------Ic0\xc6\xe6" }, /* 7 */0\x88\x90"  91bbbbSbSx90\xa5",     "I   *plci, byt----------2+1+1)
     plci->internlaw[0]      fask_tni, byt  }
     =0;

  if(      es"))
  {

       ci,H        esc_chi bytre   *   iaddunctiaic bf[i].     {
 1,dp"));

      ation[i], _DISCONNECT_R;
 it unde[] = {05]add_bKEY     er.length,_ind_sg_in_queue    i[] = {01      iUi = 0;ication;





_IN_QUEIER;
     de_nc(

voiLER){
        if(c byte ca brD);
   word !a_Aci =T;
(1,dp"\x02\
};
f us[6])NCELc  /*      uapl)
 plci      (jeq},onnecttesword) a->IdUEUE_Ss>> 5 wordd_b1if         k   retur_chi =i->Sig.Id ==iNDING;
 parm1,dprLaw(_PLCI;
 table app"wss b1_faclci, 0, false);
  plci_free_msg_in98\x90",].info);
   <4

     == worOUTG_DIS_P     if(batic void   ifn_wrapuTER   *,worOFING_REQ"MESS   cle  if(Id & EXi,nfo_u-------        ", word         api messe noCh = 0;
  wo1,dpriturn false;
}

static bytetf("co_req(<29ications)LI,"\x01\x01");
   4ber, DIV2byte c==   sig_req(plci,ASSIe max_aand fax_c     sig_req(   {
       -------*/

 API_PARSEust_b23_command (dword Id, PLCI        "w",Intate = OUTG_D(plci);DSI  ang.Id && plc 'd':
     _WRONG_IDEf)
  {
  >msg_insd, D  whinue scan      x] Card,      : %lparms||IST_REQ;lci)
dapter ) /* 1((byte },
  1]);
),->Id)
    {
&>length) ret6])----------;
     || (%d read=%d wr10]w]);
   ENDING)plci->State =4->State==SU!=SUS;
     )->State==SUS=8 dbug(1,dprintundation; eith46]) for (i = 0}


staar_cG; j++)O sizeoLL_RPI_ACo00ft Call_  *      "w",IB1_PPL   *  word Number3{
      ncPLCI   *, APl2fferFree(APPL nect_b1fd Number, DIVA_CAPI_ADA3TER *a,
		       PLCo0b7L      
        "w",Inncci && (plci->ad;
        dbug(1,dprintf = 0Glength - GL_Blow %ld_OPER,
			         }, /* 22 IVA_CAPI_ADAufferFree(APPL ->channels));
        if( IDnd/or modify
  it under the ter < C_a->adapter_disabled)
    chi, APPL  byte cau_t[] = {0,0,0x90,0 elss, 20s *ncci_ptr;
ll_dirappl = appl      */
/*       88\x9"     tic w   brea1s[i].lPARS (i != plIG_ID      }
 i] == appl->Id-xffffffffL;
,ommand)>Info_Mask[appl->Id-1] {
      if (!->internaECHa3);
x_p  return;
  }
  if (psl;
      ifG_ID);
  sk[ supplI|= 1ncci(PRlengthvoid api_CAPI_ree_receive_b

  dbug(1,dprintf("lis|E_FOWONTROLLER && G         }, /* 22     =_C1_1ueue[eq.FL  0printesdprin}
  , APPdbug(1,      seq.FRT         ffer      but W->Info_= 1Lor offdbug(1,_t[2d&EXT_CRTt[] = {0,en and switch lrriterimary_pay *, /
/*);
sta){listearlase acon5sig_req(plci,A 0;
         (i=MOVE,0);
 [3        >=isendi<22
      }

           CG)plci->Stat          .(parms[0].bug(1,dprint  el  *,easedeT38r off*IRM,engt                   for(TelOAD dbug((b & 0xpaT38->TelOSA[i] = parms[4].faci4].info+1) = parms[3].info1;   a->TFAX_SUB_SEP_PW)][a->u_law]= &dummy_plci;
        a->TelOAD[0] = (byte)(pa = appl
stat; /o[i];
        }
        a->TelOSA[i] = 0;
      }
      else IV1
        a->TelOAD[i] = 0;
        a->TelOSA  while  a->TelV1ai_p------------------*/

stISTEN_R|CONFumber,
        arly B302\x88TON */
  { "\xntroller, codec not supported */
    }
    else{(D_LEmbero[i];
        }
        a->TelOSA[i] = 0;
      }
      else IPIA    m "\x03\ntroller, codec not supported */
    }
    else{->Sig Id,
        Number,
        "w",Info);

  if (a) listen_checkesc_Ptend-------];
    plset_bchannela
       if(!dirc     printf("con_parmVA_C API_PARSE5;ieq.F_ADAi;
     dbug(1,dpor(i=1;parms[4n or off*=i && i    if(!OWlci_remove_      _PEND=1) s         _c voiN_msg.h"
#io wrongOWNE ai_parms[5];
  word Info = 0;

  dbug(1,dprintf("info_req"));
  NONSTANDARCI   connontroller;CI   *,l retVIDES_NO_C    a->costen_che_tablr) plcsk_taata_sen   }
         plci->Si; i;

  dbug(1,dprintf("listen Number,7 Number, DIVA_CAPI_ADAufferFree(APPL nect_b3_t;
  static byte cau_t[]k[appl->Id-1]      dbat;
  static byte cau_t[] = {0,0,0x90,0PLCI   *p   elseN,DSIG_ID);
  on or off*IRM,&, stat"\x02\x88\x90",  HARDnfo[1],(woppl->DataNCCI[my_plci.State = IDLE;
 (Q->appl = NU  Number,
       l_comi_pa && ai_parms[2].lengtSOFTEY */SEND |* User_);
   it winfo[1],(wodbuRECEIVENNECT_R|CONFIRMearly B3 con)) }

      if->profls) {
       }, /* 2x86,0xai_parms[ate = OUTG_D    _RE~ && ai_parms[2].lengtOOB_)
  anynect_res")---------isconnect_res;
      if(a-ci->State==SU=->channelsDWORDS)
   ()][a->u_law])    sig_req(plci,USER_DAT<< (b &    {
      /* overlap se_buffers (pling_lOvlSnd" && ai_par  rc_plci ] = {0,0,0xe_buf>Info_Mask[appl->Id-1   "w",Info)}
ic bytmandif((a_mask_2 */
ller =%02x", D-ch fis= (bytanintf("conn||wareturn lci, !lci,          "sO_save_msg)
        {
    ormat; }
  sendf(apLLER){
        if(_bitcility_res},_bitAC"));
    if(|=i = &a->plplci,&parms[isten_)
       PICAPI_Aturn 2;
    = &SSExtnd(PNNECT_RES     sig_r_parms[0].lNCR_comLCI      f(Id & E (plci,) p));
 i_reCRt_commanVS/* anReql->Id(Id & EXTCALLai_parms[0].loid            DIVA_CA     fwor,
    Ub,
  {_INFblished   if (plci->NL.Id && !plci->nl_nd/or modceC
   --------cSupport)"))dram_barconnect_res(va_get_dm_co *appl,[_to _   i;]&0x1gth=1;j       while ntroller (, DIVA_CA& 0xfffT_B3_I| }

     E   *out);
stati)*appcommand-------*/

stCILITY,0);mber, DIV        "q==      conntf("FAC"));
  .number,
*)(&           "", s) {
       er,etur C_Iparms[5       */
>Id = con\0';
 te(IVA_CAPr[i].m (plci)defi   i       /*             bfed_msg==Sespoid &p Id, _ng;
   co_->data_oucommand sc_chD(parms[1]._)->mINC_CO1;
          }
  0;

  codech==3 || ; ncci++)
   rc_ptic byte c
   ONG_IDC_LABord, 
     D_ALERT)
emove l->Id-16],ci, byte R&        1]) 0] = (byte)==4)ad plci->internaleturi->sa0xff;
  r_plc++) ai_parms[i].lengthCI alSKg++;
   lci;
 _commD);
->si for (k  A_CAPI_ADAINC_     ifif(ch==3 && ai_inf4{
  
{
  wo       {
            ch = GET_WORD(ai_p     I   *plci, wo));
  Info l->Id-0] = (bplci, CONN_NR, &parms[2ci->internal_coword {
    if(aplciI_PA Filearyr->data_-------_plci,ASSI     ifword iREJeatui_paCo)
{
  wordtf("R------------*   /* 11at;
  a        }
   ++;
          msg/* and i ;

  T_IGssary to save itncciEXEC   }
 c_in     HRE/* call  }, /* 2 */
ct_res == _plcis---------------ter      "w",Infic void clear_g&& i<
  cci_ring_list = i;
                PI_ADAP&& i<
        ->appl _next[ncci] = 0;
          }
  }
  else
  {
    for (ncci = 1Ido wrong")5]==se
        pl)
    }
  ;
    lGE_F  {_ISCOE &&d b)
  PL[i] != plci      bp = &parms[5] = (bytatici->appl)
           return 2;
    plc    3 (ch_mask =    add_p(plci,BC,ciumber, DIVrtater, DIrc_plci-In+LER){
  _Btic void farc_plci-2]ile ((j < MAX_lci,KEY,&ai_pao);

  i1];
  mber;600_req(plcSreqrd IlONFIR= xdi_se
        f(appl,
        
  return ret;
}
ord)A 0 (pl  fo- 2     "w",Info)
        cleanup_ncci_data (pi)
{
  word i;

  if (plci->ap{
< AR            ch = GI,mapp    plc

  lci, \x88\x90"   ,    l=%x)",plci      if (ch_o a CALL   Ser;
  bytconnre_ncci_LCIvalms  = IVA_CAPI_ADAPTER   * a(1,dpriN_PENDING)        SSIRM,
    ""rd Id,ntrollCIRM,
 out[ifos fo =  dbug(12 dbug(1,s = &msg[1];SSsDIVA_CAPI_ADAPTER *)(&((bytci,  = 0;
   */
  "\x02\x91\xa4"t correct for ETSIt correct for ETSI   Info = _W;
}

static byte conn_mappinnoCh) Inf  else
 N_PENDING) {
 \x06\x43\x61\x70\      nc1
        if(!dr      \x06\x43\x61\x70\x esc_chi[0] REMOVE,0);
    ofed_msg       a data_b3_req},
  _ptr->data->ncci_next[for every a  esc_chi[0] = (byte)(ai_p   if(GETer[con}


static vCF   }
 f)
  {
   dummy     if ((ai&(mBg[1].info[1]));
        PUT_WORD(&RN
      nfo[1]));
        PUT_WORD(NUMfor every a_chi = "DBufupport(a, plccas_ptr->data_SUPPORTED_SERVIlied for th PERM_LIST_REQ;    FIRM,
          ADSE(byte)l_commandtion; eithersg->header.l
{
  wo  rc_plci        <9;i++) ss_parms[i].length = 0;

  parms = &msg[1];

  if(!a)
  {
    ds", (char   *) pms[5]("wrong Ctrl"));
         G)plci->St esc_chir(i=0; i+5<=         esc_chi[0] = (byte)(ai_p      "w",Info);PI_ADADeact_DivT_I|RESPONSE,  
              0]=0x9    {
   PU200){ /*&SSlengt3[6],6 API_PARSECHommand (dword Id, PLCI   d b)
 case S_GET_I|R     ow %ld %0    {
            d   if(GET_W& plci->St

  dbug(1sg[1].length)
 afCapi Inf       dbug(0,dpINAL_PORTABILITY);t     n for 1; ncci L)&&is prog   */
        if(= 0;

  = parms *)   rplcird Id, word Numbnd = GETSARN_ACCEPT;  case SELECTO    bLCI   ]));
        PUT_WORD(&Rsgy B3  B3 conadd_ = 0;
     INALPORTAB

  if(!om add,dummy = 0;
     ORTED,0);


  if(req(0] = NULL;
  while in sendnumber = Number;
            rplci->app     }pl;
            sig_req(rp7ci,S_SUPPORTED,0);
            send_req(rp
  dbug (1, dpri>ncci_eypad gram(rplci);
          UPPORTED_SERVIe S;
     word,rc_pl(plci) IRM,
--------,"wbd",ssNum5]);
  _p(plci,UID,"\x06\x43\x61\x70\x
              rp byte,            Id,
           esc_chi[0] = (byte)(ai_p   if(GET_Wi[ncci] break;
              }
                    ifTROL)rn fals0xf   send_req  CBS     Infber = Number;
            rplci->apDIVA_CAPI_ord          sig_req(rplcier[controlED,0);
            send_req(rplci);
   rt Call_RSER_DATA,0N(for,     "\ig_req(plCES 'd':
  i_parsK_MWI)o_reMWIPTER  e?);
      retu    r = Number;
            rplci->appl = appl;
            sig_req(rplci,S_SUPPORTED,0);
            send_req(rpli,_resfos  */
&parms[8]);
            add_p(rplci,CA = true;
      lci)
            add_p(rplci,
    nfc;
ptr))
   mber;
            rplci->apbCodecSupport)")ci->number = Numb        b          return 2;
               brealied for th          if(a->Notificati
{
  wordInfO_I|RES
              6    {
  send_rS_>data_seI   *r 0=DT   {
            ch = G_ind     OOKNTROL)&
              ad 0;
      RSE * bp;
    API_PARlude "platfo      esc_chi[0MWIplci[nc->number = Nu esc_chi[0i->St_WRONG_ME1]);
 ci->appl)
    *for: start_intr, DIVA_CAPIo = 0;
  word i      ND_MASK_DWORDS;parsI+1) &;
statiLCI rd Id, word Numbpl = a/* not correct for    "wsssssplci[i-1];
              rplci->appl = appl;
 API_PARSE9
     ssarms[0]-------- SSpR;
  }TED,0);
th = 
   >call_dplci-API_SAVE *in, API_PARS      , DI")y_req"));
 ON_PENDING || pli, wordo resource e(   }  gth,"ssss",a{
           fmitBNTROL)&& = 0;
     copy -------&i->State==SUS     {
          add_p(plci,BC,ci       if(Id & EXT_C        plcom adSUPPORT);
        bre



#include "platfo, 67o;
  wio:2ht (exter,tel=%x)",plc----------ppl,
       ai_parm         ncci)bug(1rd c     */
        ic byte rms[0HOL
        0]=pl = appl
        3]=6HOLD= 0;
}    b,A);
    if(plci_removex02\x91\x84",           wro          sig_re
        return ump_plcis(plci,UID,"\x06\x4;
        dbug(1,dprinci,KEY,&ai_par
/
         UID,"\x06  case SELECTOR_}
5break;
          _)->mplord Id, PLCj].Sig.IUP,0);
         plciTRI API      sig_req(plcidprintf("spc_ind_mask_table[i] ADAPTERLCI , PLC{
    for (i = 0;   {
State = RETRIEV       "w",Info);LER)
       plcitification_Mask[ap           j = 1;
        ta_b3_re  add
              else
   %02x %02x-%02x"       break;
             {
      case SELECTOR_        return false;
              }
              else
              {
ic byte          sig_req(plci,CALL_RETRIEVE,0);
                send_req(plci);
    In   }
              else
              {
ADplci->msg)
{
      {
        cleanup_ncci_data (plci,_PEND = chlci-, APPL *appl, API_PARSE *ntf("Ad)      plci,CALL_RETRIEVE,0);
           req(plci,CALL_RETRIEVE,0);
                send_req                send_req(plci);
              return false;
      ->command = <9;i++) ss_parms[i].length = 0;

  parms = &msg[1];

  if(!a)
  {
    d            if(303
    iTime----\x01\x80")didndicatioss k = 0;
     >channe->appl = NU            {
  _ADA->codetures
    iLCI oCh) In/
  word Number, DIVA_C        6CAPI_ESSAGEAD_MSG plci->State && plci->SuppSt  Infos _WR4; j++)
    fos    */
  TER  l functio(a->ai_parms[0].lengt  *, API") plci, apdsuccUTG_c_chi[0] = any late  ret = false;
  if(plcSAGE_Fp = &ci->numb  dbug(1,dgram is free s*, APPLlci,ASSIGN,DSIG_ID);
  on)
  e use     Info annels))/* wrong stDIVA_CAPI_AD_pars usese
        vCodec      {
    e    Info yte);plci,HLC,cip_h1,dpri        0   {
       _parms[3].len)
   yte);0);
          < AR      sig_h || ai_parms[tf("AddInfo wrong"));     IInfo = _WRONG_MEEQUEST;
      PTER/or         sd = C_HOLD_REQ;
              ad, DIVA_CAPI_ADAMAT;
  }
D(ai_pa(plcu!= = 0;
   }        send_req(            ,&parms[6      if else Info = 0x3010;              
                ofe(1,dprint        p

    if(CCI mapping  OK     {
                  tion[i], _DISCON  else In plci, appl, 0))[i];
     if(AdvCodAGE_FORMAT;
        /* and it is i_parms[nfo = add_b1(per[con       3 appl;
          sig_req(plci,CALL_RETRIEVE,0);
           ngth);
static vo connON            PUT_ (a->ncci_plci[         a->ct_r  *);
stat))[i] break;          sig_req(plci,    {
        cleanup_ncci_data (plci, ncci);
        dbug(1,dprin->Id-1] = GET_DWORD(ss_pFFord Id, word Numbpoof")q(plci,HANGUP,0);
     = _WRONG_IDENTIFIER;
  }k_bitsedpri_bchannel           if(api_parse(&parms->info[1],(word)      info+1);
                ("cok_bit[4]==CHIVA_CAPI esc_chi[0] = any later ->req_in(CIP mao      if ((ai->callg_req(pl>call_di->savedrc_plc:
     \x32\x30>length) retur      ctureOR    DSET\x01\x80")DIVA_CA      }
        _parms[0].w", _=CALL_HELD)
            {
  msg_in_que           adSU_appl\x01\x80")rint       i,LLI,"\x01\x01");
      [appl->Id-1] = GET_DWORD(ss_parms[2].in] & 0x2= GETSERV_REQ+1; ncci++)
          rplci = &s[fo = axBuf->internly B3 conne2         mber, DIVA_CAPI_ADAPT    HOLD\x01\x80");
    else Info = 0x3010;                       Infowrong;
  static by---------            if(AdvCodecSu[1].info[1]));
        PUT_WORD(&R       if(parms->length==7)
            {
          if(api_parse(&parms->info[1]ir = CALL_
            doesn't exist %ld %0ppl->Id-1] & SMARTED     II,"\x01\x80");
 0;
          sen    break;
             dbug(0,dp         plci, 0);
    if(plci_remove,
        Numb  }
            else
State==CALL_HELD            I==7",ss_parms))
        dbug(0,dp     Info = 0x300A;
                br         Info = _WRms[5]);
  
              plc_s(plci,CA  break;

                case S_CONFrms[i].l3]=dprintf("F-
stati+  case S_RESUME:
   -m->i    _DATA_B3_R)ddInECT_I|RESPONSE,  /* not correct for   case SELECTO-------------           dbug(0,dppl->Id-1] = GET_DWORD(ss_parlse;
       pty(parms[0(b & 0dummy;eq(plci,ASSIGLL_HELD);
                send_recai dbug(2ength = 0;
      if(plci && plci->Statai[1] = CONF_BEGIN;
         CONssss",ai_parm& Sn for            add_p(rplci,UID,"\x0cSupport(a, plci, apmand = CONF_BEGIN break;_OUT | CALL_DIR_OR= GETSERV_RE  plci->internalF_tabl            dbse S_CONF_ && i         _req(plci,CALL_RETelse
              {
     }
   ree_receive_bx32\x30");
   ;

   ust_b23_command (dword Id;
              k <ATE             case SELECTOR_el = CONF_ISOLATE;
                  plci->internal_command = CONF_ISOLATE_REQ_PEND;
                  break;
   plci);
            )
    {
      case SELECTOR_ break;

  e
  FoundatioTACH;
                  plci->inNGUP,0);
     _p(rpak;
              case S_CONF_DROP:
                  cai[1] = CONF_DROP;
                  plci->i  }
        c_chi[rms[i].l   * (i !=i[0] = 2;
           ),ASSIGN,DSIG      "w",Info)rms[i].l         (--p) =             dbug(= _WRONG_MESSAGE_FORMAT;
             S   seNDING)             }
                  dbug(0,dper[control          plci  }
   PERM_LIST_REQ;o[1],(word)pCONN    ESord Id, word Numb    plci->internals))
        sig_req(plci,CALL_ | CALLI,&se(&parms-   {
             _DIR_OUT | CALL_DIs))
0);
                       plci            dbug(1)
            {
              [1],(wo==    br     wrong"));
                Info 
static    if(AdvCodAGE_FORMAT;
              break;
I_ADAPT
            rplci->command0\x00          dbug(       dbug(0,d{
PTER           send_req(plci)0]>=0x1     }
  d = C_HOLD_REQ;
    02x %02x-%i,CALL_wrong")L     if(ch>4er;
  bytength = 0;
 WRONG_ddIn         ;
             EST;
              1 */ONF_DROP:
                  cai[1] = CONF_DROP;
             if(G  dbug, API_PAN  p fy
  itPLCI [_CONF_REATTAC == &Sug(1,(no p        }
               (i != 0)
      {
       eak;
            ude "dion MADAPTERWPIcludL || ch                            parms->length==8) /* workaround for the g(1,dp         ength = 0;
02x %02x-%02x",
      ncci_mapping_bu     [1],(word)undation; eitherD-\x02\x91\x84",   bdb",ss_parms))
      rintf("faci");
   0){ /*                              {
     plci->State && ((plci-;
  }30");
            sig_req(rplciRSE *);
static byatic voidd (PLCI   *pl, byeak;
              ciLCI   *, dwoPI_ADAPTER/or modify
  it under t
stas_parms))
              {
        dbug(1,dpx",relatedPLCIvalue));
 m_LEVELSn false;
  ntrolle *msg, word length);
static       nccCTOR_Hrintf("FAd (dword Id,    knowledg         }    ]     sig_r]NECT_ACTIVThismsg[1]er.command == _DATA_es(dwo2ndRT);
=ntf(xfc_plci,++--------------mand;      for(i=-------- *, API_PAR        case S_CON     ncci_map(plci,CAI,c    << i;    sig_
          case SELECT1]=byte,d %08lx ONG_M    
  if *, API_P PLCI   *plc_DIS_PENDING;sNG_IDEN */
      if(I| CALL_DIR_ORIadd{
  UEST;
 me * pa if (((CAPI ->bit (PTA_B3ossi = &->length==7_ptr = &(a->any late (char   *)(((lci->gth-12),ftables                        
      return fals\x06\x43\x61\x70\x69UID,"\x06\x43\x61\x70\x69tate  OK       = RETRIEVE_REQUEST;
  D)
              {
 
      return false PartyId */
          internal_command to Yi,INinternal_command =ternal_command = CL_RET_WOR10;        {
              wbd",ss_parm    plci              {
                dbug(1,dprlue)
       NC_CON_ALERTf(Id & EXT_  N6e;
      rc_plci-l = appl;  d] = (byte supplrelease after     "w",Info);ead_posd
     Party = 0;
ak;
      ci,HANGUP,0);
      send_req(plci);
    }        "w",Info);EGIN:
            ) + s     l = ncci; yo;
      0000FFFF;
       S           Id,
              cai[1] pter->max_plci;i++)
_par        plci->Seturn false;
              }
           0);
          return 1;
         {
   req"));
  PPL *0relatedadapter->plci[i].Id == (byt   break;
            }
  _p(rp:o+1)_p(rp /* wrong state  if(ch==3 && ai_paUID,"\x06\x43\x61\x70\x6nal_command (dword Id, P        sig_->ptyState));
            dbug(1,dprintf("plci->ptyState:%x",plci->ptySta           dbug(1,dprintf("fow-S */
            {
               }
     >Notification_Mask[        ) {
                   case S_CONF_B       Info wrong        }
                        break;
            }
            a->latedPLCIvalue)
        == (_DIall_dir]    dbuger[controotification_Mask[a       4        /*"));
                          er;
1,dprSci->appl  (ncci_ptrplci->ptyState:%x",plci->ptySta      dum1,dprin        );
*/
            /* se= 36) || (ch_mastions (ch==ci,CALL_RETRIEVE,0);
           dbug(1,dprintf("rplci->ptyState:%x",rplcie)relatedPLCIvalue)
     ning_nce Sizst *ETAi].Id == (byte* 14 *ASDIVAdLINKAGE j < 4; j+  "\ for {
   ----AT;
 ci[i];
           ug(1,dprie;
              }
  | plci->internci || !relatedPLCIvalue)
            break;vsturn 1;
       plci->internal_WI_POLL,0);
             if((ai_"DIVA_CAPI_ADAPTEtf("plci:%x",plci))   *r        WARRdi,dpriCCEPAD[i] = parms[3plci[nplci,KEY,&ai_par

        CCEPT;
 MWI_POLL,0);
    es because of US stufAI,cai);
   length = 0;
      Keypad 
            dbug(0,dpvCode02x %02x-%02x""SSreq:%x",SSreq))6n; j++)
  dprin10;    7      cai[1]     if(!dir) pate         iw           dbug(1,de S_CONF_DR         pl      plci-10;            }
      "rplci->appl:%x",rplci->appl)); esc_chi[0] = (byte)(ai    word Id, word Nd)parms->length,"wbdb",ss_parms))
                  cai[1] = ECONTROLLER)
              {
   - 1)value)
         queue)(1,dprint>manufac;

                 (1,dpri send_int Nu;


s    if(!diESPOfc;

    dprintf("implicit inntf("faing_lex    ss anv a->ialse;
  if()",plci->ADAPTElude "pla  rplci->vs = 0;
I;
    ca dbugplci) rplci     ifu wrong state      {L3i_parms|    info_r/  send_req(=PL=0)     <9;i+   send------------------------              if(api_  return false;
 eari, t_s  sig_req(plci,\x00\x00\x02"format S_ECT:
   l_command_=;
      [OUT |
      break;
    crd)parms->length,"wbLL)
rms->length,"wbdb",ss_parms))
              {
                 sig_req(ci->Su->appl = ACK /* 18 */alue)
            case S_CONF_BE  rplci = &related    sig_req(r      whcci_ULL)ATA_B3_R,     ify
  it under t   ncci = ch, PLCI .info          
    y
  i p_length);
st&& plci->SuppState   Info                       send_req(plug(1,dprintf("rplc             ase S_CONF_BEGIN:
                  cai[1] = CONF_BEGIN;
   < MAX_NCCI+1) &W== (b    >Notification_Mask[    dbug(1,dprint
}

static bytelci[i].Id == (byte)relatedPLCIvalue)
           b  msg_reak;

            }
            a->Notification_Mask[pl->Id-1] = GET_DWORD(ss_parms[2].in*);
sEFtureIO             dbug(= _WRONG_MESSAGE_FORMAT;
                break;
 lci->vspr cannot cller u2       *a;
        /* reuodec sutf("SSreq:%x",SSreq))C
                 /* reu supplCDEn\xa3"Sparms         /* rify
  it under | code, API_FO----    }N6 dbug(1,dpr0\x&msg[1 *a,
	->call_dir = CA        &msg[1,* DISCONNMAT;
    ONF_ove fREQ_P &msg[1]);
      sig_req(rc_plci,NCR_FACILITY,0);
      if bug++;
    dbug(ce Size r      }
           faciNCEL     Y */
    rcd Alert (! &msg[1[1],(word) esc_chi[0] = (bbdvoid si canB2t nec==B2options suppontf("connnot nec==s options supp=%d",
          msg->header.command, p     j].Sige (i != plci->LCI   * plci, byte code, byte   5             a-d p_---------------0] =
  {'0','1','2','3',max_plci;   appl->(,ss_parms))
              {
                dbu           k H:
                  cai[1] = CONF_REA ncci_free_receive_b              {
                    -----       cai[1] = CONF| chWORDSATURE_XONOFF_ng_bug++;
        Idnction;
  }nternal_command = 0d i;

     sig_req(rip    
   cip(aect_req    ] = {nal_prov1];
].infss_pL<<ntf("FA   els*ncci_ptr;

      LLER].info);
  cipg_req(plcak;
    d dtmf_indicatioP);
   printf   *, lci->mrms->efind Pontro a =   * pl     t[i]; i++) {
 yoPARSE *);
static byte ld->Id-1TE;
      , DI     PI_A    AGE_FORMAT;
      - 1) */
            if ( if(!Info)
  {
    switch(selIk = 0;
   _in_wriCIPtf("impli    |g_req(plci,ASSIGMAT;
   D, cannot [5    fiedPT okord find_,aatedPLCIvao);
      sig_re     "s",           l     I_PARSE *);
   send_re if((i=);
 o[0]));
    x06\;
  i,add_p(rplci,CPN,se (i != plci->;
          }
          p(rprelatedPTYPLCIvalue & 0x7f));
static byte chi);
 e test_group_i (e)))
 :
          ca   * plci, byte c= 0)
             ~(nnelOff(PLCI   *plci);
static voin || pltate  
    Id)))

voi {
    f(PLCI   *plci);
stie= tic       addrect, otherwiseM    if(api_parse(&parms->info[1],(word)parms-> {
               if(api_parse(&pa  }
            a->Notification_Mask = '\0';
    forif a       ].in-1))ext        && ((s don0;
         2 */
  NCRI_ADAPTER   *a,prinrR_F {
  {plci)>ncci    ncci(bod_b1s   }
     LLCt_b23_co      = MSG_IN_QUEU *,CK_Purce)_CANCE       }I,ca,           "", }, /* 2 */
  {
}


sta if (ncci && (plci->    +j < t correct for ETSIrp*plci,P      con(byte   *)(plcata_rc(Pfac"wbd"q, cannot check all ON_BOrd adLECTOR_H {
             ss_parms                       

static byte connect_res(dword Id,  SSptf("Pplci, &paadd_p(p cai[1] = CONF           ""   >internaONF_DROP:     sig {
           (i = 0; i < C_IND_MSL2(datber[co
  byt", ms     P       nccend_req(rplcpl->Id-1] = GET_DWORD(ss_paend_req(p if (m->heae   {tyfo);onnS_DEomman      plpter )
  {
    _CON   for(i=0; i+rplci->vsprotdiaonnectrintf("format wrreq(rc_plciv_DWORD(ss_parms[2].     }
     f)
  {
  Suba     s->length,"wbd",eq, cannot check all s3Info = 0x3010;       2(data) intf("format wrong"));
              Irplc     aBefo =Cap;

 ec s_req(rplci);
            )SSreq;
        1 */
  "  Lows(error from adsend_req(rplci);
            break;

   r (n     aHigh            {
  intf("format wrong"));
         ("AddImat wg_CAPn            lci->a1] = CONF_DROP;
                  
voidfo) break;Bncci_ch[ncInfoi[1] = CONF_DROP;
                  queue (eturn 2;key  In == 8j].Sig0|(b & 0 PUT_WORD(&  else Inarse(&_req     }
   tionSION_RLER)
add_p(plci,CAI,cai);
              s retTesc_chi[0]    OGAlue)
               AI,cai);
              sig     add_p(plc1] x.31 ;
  SG_Ielse(SCR   cintf("format wrong"));
         ER"\x01\x80")
           r modify
  it und    cai[0] = 1;
         S:
      cai[0] = 1;
         >ncci_ring_list) &   }
 S:*/
 );
sword)parms->p     UM;}
   un_in_p                 case S_INTERROGATE_NUMBERS: /*         ug, Id           */
 




tolci->Id    ssighis g(1,dpriInfo = _WRONG_MESSAGE_FOa3",Sparms  = "";
                send_req(plciand_qPARSE * bp;
    API_PARSE a   "\x03\x80\x90\xa2"  &(a->->internal_com4].ci->intern*);

    {ery _STO        (ECT_I|RESPOem             NG_STOP:
                CFplciRT          return fad = CCBng_bug, ch, force_ncci, ],(word)parmsD;
 /* 2CT_B3invocatiu a->        plci->comm        l, API[0] = (b add_p(plci,CPN,ss_pa    NOTIFid SING
}
              plccation(DIALL_FORWARDING_STOP:
                CCBed scf"));
      CI   *, d----*/

static led {_DA+j <    {
  /* 6 */
   rplcOG  rplci->nu    OS>vsprolalue & 0x7f),'b','c','d','e','f'};
                 es */
              add_p       case S_CCBS_INTERROGATS MAX_NR      (plci,HLC,cip_hlc[GET_BC,&rite_pos f("rplci->appl:anufacturer_res(dworORWARDING_STOP:
 SAGE_FORMAai(plci,nfo = 0x3010;   NG_MESSAGE_FORMATf("AddI, word lengterSet(APPL   * appl, dword reffalse;
static P***Ser;
   M      NF_REATTACH:
          SE *);
static bytf("AddInd_mask (PLCI   *plci)
{
  f("AddI[8
           CIP");
        par   db "\x02\x91\x8GINATE;
            /* check 'ext_req(plCI   *plci, byte R  sendATTACH:
                  cai[1  * appl, void   *rms->i cannoS            da
              break;>ncci_nex%ld %08lx Q_PEND;
fo = _WROtms[i *, x03\x9        a  * appl,undation; eieappl, int Num);

ib1_faci_PEND,&            1,    ("AddI/
  { "",    );
                ET_D      /* check 'e*, PLCI [) no releasInfo_Ma3disa  _CO
   ci &&            j++plci->Id;p->spoo) {
  * appl, d (->Nod = C{
  9rms[5],leING plci->appternal_command = CON    dmsg->heaTE;

 o>numb1plci-);
                 void mixer_noti0"   0\_      = plci->Sig.Id;
       st_c     }          rplci-7)
     o+1)1] = CC       0"         }, /* 3 *lci,Rai7)
     dd_p(rplci,CA70\x69\x32\IN:
              case S_CCBS_REQeue)))
id   * Trdword) nc       )
       && plc         disconnect_b3_reseatuollsion ofmand = INT)
  f (a->ncci_plci[nc0])); /* FT_I|RESPONSE,    plci,UID,"\x06\x4rue;
            cs_parBter_ferd add_add_ai(PLCI   *rue;
            cai[0]
   plcalse;lPPL        cas           sig_r)nd = Basicde)
                {
  :
       /*-------ate  oincl    pL_CHi[ncci] == pleqedTOP_chi[0] = (bytic byte remove_starparms[3]nd =     sig esc_chi[0] = (>internal_command = lci->msg_in_read_pos;
 D!sk[appl->Id-1] & 0x2            dbug(0,dpc0\xc6\xe6"->lengtENDING)   {
  "AddIvlSnd"));
 f("NCCI mapping exists %ld %02x %02x %02x-%02x",
      n  else In   sig_  br  a->Telsig_req(pli,S_SE_ADD_REQ_PEND;
       cai&& i3_REQ_PEND;
                  rp0|(byalse;
static PLCI dumbdws",ss_parms))
   NF_REATTACH:
              ca  return 1;
        byte crplci->internaword ncci)
{
nda_get_eword Id, PLC * Numbe >> 5] & (1L MAT;
           wrap_pos = MSG_IN_QUE   a,
     tedPLCIvalue(i=rmat[i])




static byt         ONF_DROP:
     , byte others)lse;
static PLC\x61\x70\x69\x32\x30                c      {            break;
            }
            a->Notification_Mask[eq(rplci);
            break;

                 disconnect_bRVICE,0);
            send_SPUT_W                 breakONG_MESSAGEter->max_plci;i++)
     :
                  cai[1]   }
              else
          RVICE,0);
            send_f("NCCI nfo[1],(word)parms->length,"wbdb= _WRONG_MESSAGE_FORMAT;
                break;
 w   ""ai_pa==7)
            {
              if(api_parse(&i->msg_>length) ret      /* check 'ex  Inf-----------------------------\x70\x69\x32\dir |="c_ind_mask =%eue))[i]))->header.command == _             Info )))
            {
   o[0]))); /* Basic S"wbdws",ss_parms))
   -1];
                rplci->appl = appl;
        {
   7)
    mand_ plci->internCl_command = INT)
      {
CCBS_INtf_CALL_D       ;
        while nel; /* :
        UT_WORD(&cai[90]      if(anternal_         /* reuse     break;
        0);
                send_d = CCBS_REQUEST_REQ_PEND;MWIx98\x9 /* al_commandEND;
            rplci->appl =i(a)))
         REQ_PENDarms[6].inms[6o[0])); /* Basi;
 ,CAI,cad_p(rp                rplci->internal_com     rplci->internal_commaIFIER;
              break;
        }

                 {
          d->int          cai[1] = CONFd_s(plci,:
          c           dbug(1,dprintf("forma+
         ication;    reset            pendi<;i++) ss_parms if ext    
  dbug(1ci->tel));
  if(pl_mask (PLCI        add_p(plci,CAI,cafo[i IpporPARSEadd_a, appl, 0) )
        {
         )
{

  iN_AL_WRONG_IDENCI *plci, A,0)ternal:
9DING) 2 */
  VICE b   *[0] >inter        rplci,UID,"sig_rsk[a6\x02\x88\x90"  91\x90\xa
stati if(!plci)
   length +3);
 u          breaning indicator     )
  olrnalyl    rmaORMATe */
                    sig_reug(1,dprintf("NC /* explicit               dbug(1,dprintf("format wrong"));
          OLATE;
     _PEND;    nleESPOdes tic void sendbig_req    -e S_CCBS_I           SAGE_FOn imminvoca 4;
                  send_req    e Info = 0x3lse;
static   }

            if((i=get_plci/
            Pate = Or       API_ADAPTER   *a;
  }
          ncS  }
arms[6]1]ueue= ch;
      ncci++;
        i,dprintf("NCCI ma       {
            "\x03\x91\xe0\       l;
exter iENTIrms[5]);
  lci,U0;
    cn  swig_rC              / /* 7 */
 "\x06\x43 CONF         bernal_command =     if(!word    i = 1;
     (ie[1    9rplci,CAI,"\xpl, APx80plciD[i] = paPI_ADAPTER   *s_parms["",      PUT_           return fa       case S_CON_ALeq(dword, word, DIVA_CAPI_ADAPTER   *, PLCIDENword,_offdd_p(rplci,CAI,cai);
{
 I;   for(&(splci && plci->Sxplicit invocationi,CPbreak;
        tedPLCIvaluemove function                         plci-> doussss*/

e=ak;
              }\xa3",     "\x03\x80\x, PLCI h==7)
       ;
}
f0)=x7f)RMAT;
       r add_p(rplcid dtmf_command (dwordal_commINAL_ADAPTER   *, PLCI   x80\x90\xa3",     "\x03\   *, Ai,CAI,cai);
          ,G     
                  dbug(0,del = 0;  appcom   ncc          )
                   Pnder t'+' && *,lci, de              rd_p(rple S_CCBS   i88\xlci-  selci, rnal_command = print    ].info)d,rnal_command =           {
    - 1) *i = 0;
 = plc          | bre    {
              l;
            ai[1] fo[1Us    ;
   ;
            dS     byteyte connecPLCrequest (Id, r |Id)); rplci->command CONgth,"RONG_ME------->appl = appl;
               a, plci, appl,NF_BEGIN:
             ;
stati modify
  it unci, byte   *msg, word length);
stati))
          {           doutgCTIVATE:se
        "w",Info);
  return      A_CArted upB  break;
         _commanmf_commandq(rpl
     F_REATTACH:
     ) & 0xfffcntbyte max_a     n_wrap_po false;
         }, /* 19 */
  { ""\x06;
  API_PARSE *      rplci-         case S_Cr            MAX_NCCI+1) & if( !rplci->internal_c ll rlength)5]
    *, API_PARSEcci, tatic

  sel= 0;
      /* 5 */
  "",ONF_DROPf
            sendength==7)
    ions for arbit (l (ss_parms[5].info[0
            addo[0]))); /* Message Status */
       dbug(1,d(&cai[10         rplciPEND;
            cac void mixer_notiyte facility_t       } /* e    else
        e terms of thelse isiTelOS               bit, just i      ng_l    rplSAlear ind mask bit, just i    SHIFT|6LLER)dtmf_command (dword nect_b3_reIN>n0x7f) ord Id,       add_pRc) CONF_ADD word-----commlse;
static PLCI dummy_pl, API_PARSE *painfo); /* Contro   *plcstatic bycwordec      retu)
    x7f)) a,_ind_mask_bit        al_com    }
 bits0;

x     _ET_Dg        "wsss Info = 0x3010;                  CANCELLEw
            se            send_req(pl)d &((by;
    rOfest (dword byte Rc);
static badvb1_faci(ss_parms[6].inOSA,ss_parms[9].in)parms->lengt)parms->length,"wbdwERT;
        add_ {
   E_NUMBERS:GINATE;
      E_FORMAT;
       else
  {
    for (ncci = 1; n      }
           'b','c','d','e','f'};
  worg(1,dprintfommand = CF_STOP_PEND;
     ter->max_plci;i++)
  x01",     tic tic    sendf(&a             plci->ctic byt9           ret      dbugONF_ommand ( < appl->MaxBucai);
   ci, APP(CIP bFO_Rer Number */
  F_DROP:5        /* reu
         reak;
  EQ_PMWUT_W*test_c     }tate        R_LINE_INT     |            {
  _par         sig_req(plci,S_SERVICE,0);
            send_ling User Numb4RD(&(ss_parms[6].in.info[0])); /* Basntrolling U
   ntf(          send_req(rplci,selector,SSparms);
  retadd90  rplci = &a->plci[ ] = 1;
 DCE))
        && ((plci->channels ! = 0;

   S_res(d         rv.  * appith CE_FORMAT;
  -((dword) "rplc; /* Basic Service plci->State && plci->SuppStatERS:
     
byte [9];
  word i;quiry=false/* , a-TG_Dci);sss",ss_parmsd, word, DIVA_C+1)
        

  dbug(1,dpr            seand = CONADER          }er    plci->comInfo) brea = CF_STOP_PEND;
       plci->Sig.Id;
         2;
    col or nany, /* 3 */
  { "\orks,[9];
  word i;
   _PRO = &msg[1];pvc[2]e oth, DIVA_CAPIappl, msg));

          nect_b3_res(dword, wor             if(!rplci |and = CF_START_PEN  *plci)
{
  wo     if(!rplci ||    ncci = ch;
  }
 ate           */
 WI_POLL,0);
              THER_Afor eve- n)
          i = MSG_IN_QUEUNNEC        if((i=geNL.Id,plc:
      &01ee_recei}4],SA.
         *, API_PARSEmsg_in_wrapummand = RESUM,
     370\xMplci,S_NTERROGATE_NUMBERS: /* use{
 cpngth,"ss2]_R|CO     ->data_acommani] = com   /* find PLCI PTR*/
     0xffif (!plci-1\x80");e))[i]))->header.command == _DATA_B3_R)
      {

 PEND;
  pplcilci->internaxternas                   cai[1] = CONF_BEGIN;
  0)
                {
       x86,0x       break;
      RONG_IDENTatic void
      retu      else
IR_ANSWER) && !(p_d      2,pv*bp_lci,BC,&par
     g, wdbug(1,dp   else Info = 0x3010;       defa3];
      ear_c_ind_mask_bitk];

 PVC        && ((plc             PVC */
        ai[1] rms[7].info
     plci);Sig.Id && pl       */--------e == OUTG_DI             s     I_SUPPORTS_ms->info[1],(wo;
              break;
        }
            /* reuse unused screening indicator *          ngth lse if(pl           ((dword) ncciata_rc(P        9PARSE       _req(plc_L     }
         {
    = 0x301>=5fer)->f10] =
  {'0','1','2','length +3);      }
           0;
  wPLCI         }
     ));



      case SECAI,cai);
                retuITROLLER)
    x30");
            sig_req(rplci,ASSIGN,DSIG_sk (PLCI   *plci)
{
  word /* overlap sen*---------------     case S_CCBS_REQUEST:
         printf("lo;
   arms  = "";
      }
                        7                * Request */RSE * bp;
    API_PARSE\x06\x43\x61\x70\x6, API_PARSE *pa\x06\x43\x61\x70\x69\    cai[1]  CCBS_DEACTIVATE;
      (nnect_ini;
               c     }
          c CAI,cai);
            add_p        fax_info_change dummy.length = 0;
    f(ncpi->info[1] &1) req = N_CONNECT | i[0] = 1;
                 add_d(plci,(word)(ncpi->lemmand == _DATA_B3_R)
      {

    -void set_c_iD(&((T30_INFO   ead_pos 1;
    while (plci->intern            {
              Info   add_p(plci,UID,"\x06\x43\x61\xf("NCCI mappi           PUT_DWORD(&cai[4],GET_DTROLLER)
    != 0)
 larms}
 nl_nd = CONF0         }
  ES[0].infion; ec    w = 0x3010;          {res =eruct*  foif ! Don_wrg optorthe VA_CAPIx%x)",appl->Idt 6)
 TS ncpi->in T30_CONw; i <(",       d plci_removRSE xa2"   lci, dwo0_ACTIS_RE(plc!CALL_H             sig_reqcci_ch[ncci])
     ulEND_REQ;
  70_OR_200) |
THER_;
              else
           els = _WRO;
      e cau_t[] = {_WORD(ET_WORD(&((T30_INFO   *)plci->fax_connect_innn = 0;

 featuarmsadd_ai(PLCI   *, API_PROL            _POLLate = );
      sig_req(plci,INFOPLCI mand = RE>internalfax);
void ->fax_connect_info_el, n a->    e         sig_nfo[0]));_OUT | CALL_DIR_ORI      breaPVC */
        
      plci,S_SERd)(appl->Id-1));
    ncci_free_receive_parms(1,dprintf("B3i->Sn |= (1L enabl dummy.length = 0;
 tf("AddInport */
    )
    mb3 nl__parmFAXe=INC_CON_ALER     /* DISal_comma     ,dprintf("BAD_MS-1]  rc_p   nx_connect_E,0);
       if ,nccitate        Priv     T30_Cbg_bug++;
   s e      alue)
 n |= (1L << PRIVA       return         rplci->tel      if1; i        = 0x2002; lci->SuppState = RETt correct for ETSIinfo);on        plci-)(plci->fax_coPI_PARSE *out *plT_WORD(&ncp,0xff;
   plcis(a ret;
    cplcis(a0) |
Staticreturn false;
     if(ah)
 l;
    fo_changen)
 
       con<< PR0) |
if(Ref(_XDI_PRconne |c','dffffffff
                b        Nulo it - 1) */
  ].info);
   lci->appl = appl;ch[ncatic (plci-n ci->Sta0) |
           |    r             PUT_DWO_ring_list = i;
---------] = 0;
CTURERPL *ap /* MWI active? PN      {
      )
  { D
}

statx00);
       ci);rot != :
      return fals            if-----------------D);
            re8:t      spl
     D(&ncpi->in 6\x30");
         Ited_o(%d->B3bridge, MA 02139    break;
   2_WRONG_MEBuffuestednfo_rx0f (!(fax_co}
              plcer->max_plci;i++)
    for code16ate  command]));d_optiourn 0;
}

A_CAs | pt    nquiry=false       eq(plci,INF if(S-----       k;
    ca1e cai[1     }
             nquiry=false;
   &msg[1]);
      sig_d))
          {
          }
            ifci && =19ate  R-------ADAPInfo =|= T30_CONTROL_BIT_ACCEPT_SRDword Id, word Numb    break;
    G || pl         k;
    ca     S_REax_connta = API_ & ,     if(Ad<< PR          if ((plc20Tappl, _88\xAX_SUB_S;]);
  ding(rplci    return false;
      XARSE    a->TelOSA[i]        /* rd_s(plci,HLdata_> 2   add_p(plci,UID,"e) w;
  = 2se
              {if (((b30_INFO   *)(p2lci->fax_cone = sted_options_conh,           el w;
 IFO_I              for (i = 0; i <       rplc                    ((T30_INFO   *)(plci->fax_connect_info_buffer))->stder.number;
     CONTROL_BIT_ACCLCI   *plci, APPL   *appl, APi->fax_coni== {
         2)3 conSERVIlci, aind_m      Si += rd i;
  >s_paron_iCONTROL_BIT_ACCE  }

|ug(1,dprid,
              ci->ada
#de+D))
      ing_lConi->fax_con      if(!rplci     lci->fax_alue & 0x7f)Id, word Number, DIV dbug(x43\x61\x70\xRMAT;
   & plci->Suppction;
  }
}


st    ,rel    CONTROL_BIT_ 0x301o[5])        w 
              SS |                 caSuppState==I              *r all plcis(aieBLOCK_PLCIfor codec ee Sof      by
 lci->requestCEPT_Sested_optioCONTROL_BIT_ACCEPT_S     {
                    ONTROL_B word plci_remdaargeOSA,ble[ilen;D(&(CAI,"\x01\x80"/* o_SEP_PT_WOR= (b   {   *);
staPI_PARSE*out)
(plci,INFO/
         ca  }
 move fus_parSSic w;
 * Request *lci->Su {
   ASSIGN,h th                     ss_p!itable[b    pable[aping_lisR_Flci->msg_in_read_pIT_ACCEPT_S     {
                 
                if (w       fax_contr             if (w ic blci)
  { Info = _Wp(plci,UID,"\x06\x43\x61\x70\x69\x          pl(&o_change = 1     ORD(&ncpi-lci, void si30_INFO   on-standard facilitnd_req(rplci);
            break;

         
                     if ((plc7       TE_FA          d    apping estw;ci,R;

     i>faxx_connect_info_buffer(plci,INF            ((T30_INFO   & plci ATE_FAXci->fax_co    )Aable[appl1\x01");
         plci) {
    db                                    < w; i++)
                      plci->fax_connect_info_bufferquested_options | a->_WORD(&fax_parm                     if ((plc9ci->fai,OAD_ACCEPT_ < w; i++)
                 ate     plci->fax_connect_info_buffer[lenpl = appl;
_WORD(&fax_parmord Id, PLCtssend             mand = arms[8 dbug(1,dprin    Infj<4;ji->aparms[8]1+jf("D-channel 1,dprin plci  }
 eq(rpl
  Fo+jalue  *,         umber;
   k=1,02x",
  ation_idk<    j++,ci));
  SE faklci->fa      SWORD;
      OL_BIT_ACCE4ted_o                        connectoftwTERR
         6);
         w = 20;f(T30_INFO, univ1ate  EQ_PEl_com        se */
              add_uui30_INFO   *)(plci->fax_connect_info_bufferparms[7].info[1+_flole[applnfo_buffer)->controidto aconofed_mf)
  {
   f(ch!=on_id_len = (byte) wead_pos;
lRDY30_INFO   *)(plci->fax_connect_CparmsD))
      {
       Pw = 20;);
                add_pd = ct_info_buffer)->contro
sta    rplci->internal_c>dicati_connect&((T30_= 0x3010;leh])
  {case S_CALL        fax_parms[9/
          rot d)parms->length,"wbd      l funct = 20;
     , PLCI << PRMORE_DOCUncciS)
5ate  K
             if1+        sig_remand (Id, Kength<=36nnect_info_command);
                  dbug(1,dp ;
}
fos  D))
      {
                     w = 20;f(T30_INFO, univIlci->fax_cUBADDRcase          w =          el    if (Inf      {
               nnec    }
  _SUB_S }
              plci      }
            }
          }
  
    pre(byt1tr6S_CONF_DL   *,_in_&& incp +3); *appte = art_internal_command (Id, pq97         | pl2on-standard faid fax->fax_ = plc->fax<adjustoue;
_s(plc grtedi_pa1 rplcId && plci->appl)
   ns\x30    w = 20;   PUT_WORD(0_INFO      if(d>=0D(&ncp->fax_!i->fax_i = 1;
FO   *)(d, w                }
          }
          elsonnect_info_bstation_id_len = (byte) w;
  T30_INFO   *)(plci->fax_connect_info_buffer>data_               ((T30_I_INFO  }
  length >=   whil0_INFO   *)(p      t_internal_command (Id, p(T30_INFO   *)(plci->fax_connect&((T30_INFO  G   *)(&      ) && (length = 0;ffer))->station_P;
               ;
  buffRESSNFIRM,
        Id,
        N Info = _WRONG_STATE;
      and = C       case S      }
     parms[ak;
 mand_functilue & 0x7      i  se0;
 (1 }
          }
          else_FAX     add_s(plci,KEY,&aablishedACbreak;
           lci->intern, appl,fo && pe =  .e.->appl         rprms[VATE_F   "wsssss        else Ici->faEscapr[len++] =Trd,   rplc%x",Roller uppl->DataNCCI[i] = 0;
ADAPTER   *, PLCISC/MTncci,]P_REC3on-standard facr[2+w] = ncpi-eturn );
ste
  Foundatio_INFO  >16 = 1;
       %02, /* 19 */
  {       +mapping est  word          }
                else
          th >= ) */
se
              {word ch=0;

  _parms[2].inplci, ncci);
        }
                else
          ;
              }
          worD))
      {
         {
    el = 0ECT_I|RESPONroadcIT_MO   return 1;
        }
 NULL;R   * aNTERRontroller;      _a__LERPEND;
    apr)        w < wB3_R)A,ss_pL << PRIVATE_FAX_NON            dbug(1(T30_INFO   *)plci, n0_INFO   *5_R|CONFIRM->appl)
  {
    i = plci->msg_in_read_CRMBERS:
             INFO ("ch=%info_change = trues",ss_parms)j]i,req,0);
 &     ms[8].return 1;
      }
    }
  }
  elselength - 2);
   arms  = "";    }
    ?T30_I adjL     verlci);
lci,    Y WARRA== 7
                     ==    API_);||reak;2+w&& i==KDIVA_CAP *plcternal_com< NIw-S */
            {
  DSPw-S */
            {
    }
  for (i = 0; i < LL3(requeueth);
          for (i}----------------sk (PLCI   *plci)
{
  word jg_bug++;
    db;
  word ncci, i, j, k;

  aOv
  re5
      ) || (lci,w] = ncpi->info[1+w& TT30_INSF             ENABLEms)) = p     ((gth, "wwwwssss", faxarms))
        MANUFACTURE_BIT_NEGOTIATE_R          for ( Numbti out->,
  {_DA->internf)
  {
  ata_rc(P->control_bits_l].info[1+i];
          dbug(        if((i=get_p
     rqueue)pl            }lse
        ((b            {
  (1,dprintf("non-s1+w];
        s((----0_INFO   *ci->nsf_control_bits & T30_NSF_CONT
      if ((plci->B3o
            e  Info = _WRONG_ST"w",Info);
  return fals_20;
 ----    si     ase plcis(a       }
 ta (plci, ncci);
                  }
(m->headersted_oe if (plci->B3_prot == Blci->fax_TANDARD)))
>reo);
  return false;
}
[l((T30_INFO , APPL ect_info_bfer[len++] = fax_parms[7].inf1+i];
        dbug(1,dprintf("no(0,dprintf("Q-FUL    turn fals    add_d(plci,(word)(wrong format"M-IE));
                  appl->Ms           -------arms[0].->nsf_nect_info_buffer[len      }ead_pos;                dbug(1,dprintf("non-standard facilities info missing or wrong k;
              }lse
       [h"
#ci[i:IE-------*/

st(rplci);
                _b3_res"));

  ncci  (byte) w;
         ci, nccord nccinfo_buff   "w",Info);
  returni ANY WARRA== 4   Id,th);
 

        n5_req_ncci(plci,req,(byt0;
     CAPI remove f         ((T30_INFO *)(plc = 0; i < fax_parms[7].length; i++)
     plci->fax_connect_info (ch==0), fer[len++] = fax_pact_info_length <= lenbuffer[len++] =i->fax_conneci(plci,req,(byt4+= 1 +------------------------------------ltrnal_cpi->info[1], ncpiarms))
            {
            [1], ncpiug(1,dprintf("non-standard facilities            /* nect+] = (   sig_req(plciax_parms[7].info&& (offatic void VSwitchReqInd(PLCI   *plci,     a8",            a,  return 1;
      }
     _VALID_CONNECT_B3_ACT)
         && !rn false;
}

static byte connect_b3_res(dwId,0,03\x     ("connect_b3_replci->channelf("AddInfo_B3_R));
                  
            {
              if(api_parse(&paI+1) &"   s, API_SA= {
  { "",  Nu == (b             Id,
    value & 0x7f) == 0)
rol_bitEGIN:
                  cai[1] = CONF_BEGIN;
  ci->msg_in      itart_internal_com<mand             dbug(1NECT_ACK | N_D_BIT;
   b         plci->internal_ruffer[2+w] = ncpiervice */
    enested&((T30_INFO   *)pinternal_command _CONNECT_ACK | N_D_BIT;
          connect_r+= 1 +f









/                  if(!Info)
   |se  Info = _WRONG_S         }
          elta (plci, ncci);  rc_p, bytolOAD[0]  }
      }
[i]!a)
= 0xFORMAT;
  lert_b   {t[i].EXTIEnfo =           ncpi/UB/PWD en}
  3;
    }
i_parms[       RS:
      {
    6..rted */sci->ncpi_s word s},
  {_             se cau_t[] = {0ffer[len++] = (byte)(fax_)
-----------        EQUESi->internECT_ACK | N_it 9) no ]<6)5ig_req(rd = I);
stat_Pi]nterncturer_reext_internal
     n   "wsssssonnectx  }
             ->faiplci->ncp1; ncci++)
    {
   D)))
  ci);
;
       else i"     _conn0\xa5",     ET_D;
   te con     T_ACK annels))e MapId(Ix86,0            pl    ! return 1;
ONNECTED plci->ncpi_s],(wor!      add_) &&  (plctiNG->data_pend else
lci, ncci);
    ct_res    a->nc  rplci->app>ncci_ch[ncci]);
  CONF_I{
    plci->number fo);its);
         (byte               0;    fax_    IGINATE;
        plci->appl = appl;
            sig_------CCst_b2n, API_PARSE t_b3   IE && ai_coui->fax_connN Avetions_con = fax_parms[5      if(no----ax_feature
     sg 0x3010;        = fax_pa (plci+ sizeoctorC*, API_Perax>len",ss_parms)IVATE_FAX_NONSTAv   0        {
    2OUT_OF_PLC




/ sen prog      		   ncci);       esc_chi[i+3]fo wrer))SUCCESS       "w",Info);
  return fals:%x",rppoofed*/IONeturn 2;     (for1;
  },c_in&ai_mappi_ind_m       tf("rpROTOCOLb3_reqi);
cci_ch[ncciTIME----q,(byte)ncw_xmit_x>length) retur;
    }
  }
  return f    adId, ong"_s(plci,HLsg);
st_request_xon (plci, a->ncci_ch[ncci]);
    IOOg_re worPEAT Id,>ncci_chequest_xon (plci, a->ncci_ch[ncci]);
    UNEXP, "www              ns},
ral Publi--*/

_ABORTi);
   plci,req,(byts},
30)
DCNncci);
      
  word i;PEoid se_nd iFLUSH_NREQ_S
}

stDTC_Uq,(by     nnel_xmit_x>length) retuTRA     >ncci_ch[ncci]);
         ATES_FAIL    }
           yte *)(plci->msg_fo = _WRONG_IDENTIFI8)) == pllse iB3_prot == B_LEVEL        a5",     ncci);
  i->data_se   INC__CORRUP             IONS)))
      {
          plci>command =  dbug(1, dprioptions fax_per = Nub & 0   */
/*----          dbug(1,dt_b3a->NotiT_WORD(a->nc4 }
  NS)))
      {
   REJEC   plccci_ch[ncciINCOMPATIB_MSG NS)))
      {
 ral Publi
        }
        nl_req_nccddInfo wrong")C_DISCONNECT_B3_R|CONFIRMTng or wroneq_ncci(plci,req,(= 0;
      *  {
li;
  return false;
}

static byte disconnect_b3_res
static vlue & 0x7f)) ==           ir, DIVA_CAPI_ADnfo); /* Receiviw;


    word Nu* BasLONnvocation  if (a->ncci[ncci].data_pending
    D_ALERTad_pos;
           ]);
      ecERT) plci-te dis>ncci_ch[ncci])Tnect------          if(1]);
      cci = (word   d  *, NG)
  ))
  SUP;

 SORYw;


   3]>=1)
  pontroller u0D(&((T30_INFOifT_WORD(&ncpi        _SCANic vte disconnect_b3_res==r the terms;
      act;
  static byte te RcAFlci)M
}

s
          IVE_I,I2 WARRA!= B2_LAPDx",
  )))
    {
      plci-C!a->nEE_S--------->length) returnINATE;
      /|ER)
    DCS  plci-FT},
  q,(by  rc_pi], _DISCONtic low %ldD_ALR  sig          inc));
_EOM modify
  it under the terms != B2_LAPD) && (plci->B2_p_PLCI) {
 APD_FRE   bre! ("%s, }
   3_R)
Id = 0;
 i++);
    if(i<MAX_CN  plci->C    ad-ncci])
S_PER_PLCI) {
           brehile ((j < _dis_ncci_tabR     i-plci->channels)plci->channels--;
      for(; i<MAX,G_NL;
te)ncci);
        plci-ls)plci->channels--;
      for(; i<MAXMCFCIrplci->=IDLE LEci,CPN,&msg[0]);
      addG sendfHANNELST_DWORD(d read=%d wra rplci->Id ci->appl,FACILITY_I,
  ----------].in; i++);S_ECT:
   MAX_CHAN;
         1di->send_disc    a (i != plEL))      n = 0;

      req =word Number    plci->3,  {     \xa5bug(1,3_a_n = 0;

      req = N_CON[i]!=(b"wsfECT_I, Id & 0xffffL, 0, "w",ee_receivVATE_FAX_NON]!=(byt
    plci->ncpi_state = 0x00;
    if (((INVALIREQU;
  dbF    90NL) && (plci->B3_prot != B3_ISO8208) && (plcin = 0;

   
     COolu Use && (plci->B3_prot != B3_ISO8208) && (plci  }, /* lci,req,(byte))     breakead_pos;            sirnal controller' bit for c     }
NFIt_b3_reax_featur
        }
        nl_req_ncc if(plciFROMsend_reqN_(i = S_ECT)
      0;
          }, l function pV34ACCEPT_Rptr->ON_ACCMARlicit ina
  if(plci &&
  Foundation word ncci  *pllse if(pl    pl},
  intf("NCCI urn 1;
      }
    }
  }
  return fal    {
     V2appl = ata_b3_req(dword Id, word Number, DIVA_CAPI_ADAPPRIMplciCTS_th);
] = E DIVrver Adi = 0;      }
        ->B3_p        URNAROUND_ 1;
  th);
          
        }
  0xffffL, 0, "w",urn falV8LLER)
     plci, OK)D[i] = parides *_remot_b2ORD(&(ss_paend_reqUUV_DIGIT  select_b_ry LA19      ER){
     
info[n = 0;
    plcSA.
, (word)3, "\x03\COppl,Tfax_connn = 0;
    plci->fa     (1,dprinpl600    }
  }
  return ONF_I\x00"SRC_OR_PAYLOAD     TED)
 latedPTYPLCplci(a)    siu_ADAP (ch_mask   }, /* 22 ry=fst>9)  (plci-
     ci, ncc "",  0se;
 Number,0turn| pltic s for ar    {
  *)(     
        i -= MAX_I_MSG          i -= MAXer[contro
  if       /* G_IN_QUE    dbug(1,dprintf("plci:%x",p      }
        rplci->appl      Infj<->B3_prnfo TIVATE:
 WORD(&((T GETSERV_}    
       &&  "\x02\x91\D(&cai[10lity_res(dword Id, "));

 ATE;
     "w",Inf   dbug(1,dprintford I-Id(NL:
        I *plci,tati or no B chE;
                 
   | plmchannel = p;
    ou & T30_CONTRll another document * c*plci)
{;
      else
    || ",   0f & ((1Llci->Staurn te   *_in_= 0x300word, lude = msg->heain_qs      wof(pkLayer d_p(rpL3(requeue))"));
        }
  arms[1]);NLor from add_b23 2)"));
       
          }
  *);lci->relatedPTYPLCI = plcue & 0x7f) =o)
          {
         /*       Inf2 */
 -------connect_res == IN(T30_INFO  static void = 20;     break;

  ferGet(APPL   * ath, plc3)
        A byte 
      break;
    c(&parms->inte tenternal_rd);
vh);
          for (i   if(         plci->B3_pr
      }

      iffo[1nt nr);_WOR<ci->ncci_ring_li set_grouied Acknfo      &&     lci);
  }MESSAGE3]>=1)
       n++;
 alue      B-CH_bit (plcand = C   */
           }

      D(ss_par!"));

 TIVE_I,Id,0,"));
               *, dO   *)(p  app else    CA=4)add
        plci->internal_req_buffer[0] =    databuff    hanneS(parms[1].infocheck for deliverynfo Haci(pli->reqplci);
      reda {
        ;
         sk[a"\x02\xarmsing != 0)ngth>3g->heACTIVEe;
   "\x02\xif (!pi, rB2_SDLC0])); word, worddword)      17*);
static wXT_CONTROLLER)
 8ng_bug++;
   < nc& 0x20g.h"
#i(plci DIVA_CAPI_ADAPTrplc      /rd nc)

void    add_s(rc,dpri      0and = Ba  "\x03\ User Num= _WRONG_IDENTIFIntf("connect_b3NL   }eq_ncci(p1    send_data(      *e;
      plci);
         ->3_T30)       breafct_b3_rec);
static----------0] = (byte-----_lengIrce_ws", N_PEND   a->Noti_DCD_      sffer)           Info   sig_req(plci,Adbu  DIVA--------CT_ACK | N_D_remove f    ad)(Id>>opI_PARrintf--------HexDump ("MDMddInfo w:"el   senprogr(plci);
           dits);
      ;
}
;
",n));
    NCCIcode = ncci | (            rpi &&e  Info = _WRONG_id c)));MDMRONG_IDEisc = (plci->Fax-pRCE_",.lengd = CCCI  {
            appl->aNCCI /*
 *plci->ncpi_state |= NCPI_MDM_CTS_ON_RECEIVED;

*
 *

  data++;rce /* indication code */s sou supfile += 2;pplitimestampwith
  Eiconif ((* Netw== DSP_CONNECTED_NORM_V18) || .ource  Adap FNetwRevisthe :  OWN))h
  Eicon  right (c) E&= ~(n *
  orkDCD2002*
  ThiT |o/or modifs,  it under th);h
  Eicon Netwis surangconnected norm Server Adaped b_opt = GET_WORD( NetPublic License of th rangFouny
  thop thes Serh
  EiconPUither  (&(  Copyistribuffer[1]), (word)( eitDograversio & 0x0000FFFF))hs pu EicontersFoundr th&program is freeOPTION_MASK_V42 you can r{mplied waedistrir th|=our o/or mECMY
  n 2, or (a} ANY WARRelseRANTY OF ANY KIND WHATSOEVER INCLUDING GNUMNPimplied warranty of MERCHANTABILITY or FITNESS MNP A PARTICULAR PURPOSE.
 etails.
 *
  You should have received a coTRANSPARENThe GNU General PubliSee the GNU General Public License for moCOMPRESSIONdetails.
 *
  You should have received a coA 02139, Thi GNU General Publis programas pERCHANbu  thin th3 hopld have on 2, or (aapi20.h"
#include 0] = 4UT ANY WARR distributebute i progNetVALIDram is f_B3_INhRRANTms 
#define dprintf
ACT
-----/*------DISC----


s.h"
#ineral Peral PANTY distrB3_protMESS7implie*
  You ANTY.(atribcILE_ "M[ANTY]MESSINC----_PENe fo2.1
 *dap-*/
/that are serverOUTGram */
/* XDIimplied w&&ask iThii20.h"
#i it------------sary to save irom every ad!capi20.h"
#ind itas pnot sary to save i_SENTf separat*
  You sher. Allo it is not ner by     ask it --------clsendfapter*/appl,Macrose ------dIVE_I,Id,0,"S",api20.h"
#include-----ude "mdm_msg-----E_ "MESSAGE.C"-------------- hereessary to save it     */
!(apter*/requestedave e us_Foun |--------------DIVA_CAPI_ROVIDES_Saer. Al---------R  0x00000Xtable[sary to sav>Id-1]implied wa& ((1L << PRIVATE */
2.1 ES_NDI_P002
#defare; implied|| 000000aer    */
/* Modifny lit under th 0000ty of Mfine  to#inccess all rhe GNU General P) */
  *
  You  distrNL.RNR  opt-------returnPI_SUPeral_a__)i        NL.completeMESS detail*
  Youppora((__a__)-Ind &0x0f"MESSN_UDATAimplied for u Net_forwardingNO_CANCEL  0xRData[0].Pe "d>> 5]0x00    0xTURER_F& MANUFACLABELl,
 1f)
 f only*
  You sionchLABEL) &&_a__)(diva_have onlyPI_SUPPOcase DTMFFileTR----ICAnse fFAX_CALLING_TONE:are
   OF ANY distrdtmf_rec_activ  */     LISTEN        FLAGimplied waedessary to save it _FACrecei_I, Idxdi_effffL, 0,"ws", SELECTOR_    , "\x01X"on 2, or (abreak_CANCEPOE_CMA                 ion(DANSWERoid group_optimization(DI ask ilocal func the s opotypes_a__) sk (PLCI   *plci);
static void cle ask  void group_optimization(DIlci, word b);
static byY  * a, PLCI   * pl
 (c)ic void groupdatiimizr the(DIDIGITS under th, PLCI   * plcied  ((_the (Id,/----CapiRe_XDI_PROVIDEleasee that;
 tha Lengthtest_  *plcind_mask_bit (PLCIlear  Co,our d b);
PLCI AutomI_USR  0x00000ADAPTconfirm*er(word)lease(test_group_ind_mas)not necessary to save it*formMIXrd, bP_----lci, word capi plci);
vis ode s_blocke "capi20.it (PLCI bute )CapiRegister(word) + 1opeo it iase(gisteregister(word)- 1HOUT i =bit (PLCI E__a_PARSE *ES_R*in, 0000PARSmove(outbyte *,deincludeove_e(votersi != 0)
aturesid DES_Nget_exteURERiv *);
static word api_parse(bybit Law(----------, byte void plci_re);
statileA_CAPI_ADAPTER  start(voi_SDRdd_adaude "pc.h"
avetend(   *);
staticPLCIid divformatCI   COEFid di *tic wo, mixl beLCI   Co__coefs_set plcik_bit (PLCI aght  contr_rc(PLCI   *, byte, byte, byX--------FROMstatic void dafile_rcMSG   x
  any _ havI_SYNC_REQ  piRegister(word)lease(void api_remove_comta_aput(APPLove(,f onl_MSG  static void dasigPPL MSG  TO
static void daSendInfoMSG   *);, dwto byte   * *, byte);
static void SendSetupInfo(APPL   *, PLCI   *, dworg(API_PARSE  LEC
static word api_parSABLE_DETEC);
static vecPTER  *, IDI_SYNC_REQ   byte);
static void SendSetupInfo(APPL   *, PLCI   *, dword Idyte, defaultest_group_ind_mask_bit lci, word b) or (atr  th ((_pporter(id data_aremndedcomes (DIVrate foaapter*/s2loadp theB2_V120_ASYNCimplied drivte ed by
 _a_res(datic atic vD FORBIS*, word, byte void p,  * plci, d  *, BITwriteocoltheY   *fSE *(DIV   &lci, word b);
static b----       (R  0x0000  "dwww"
static void dpiRegister(w1c vonect_res(dwor tic byteXDI_Pum < 2) ? 0 :ic vL   *,   0x00r(word  *, PI   *, APPE *);Numsten_req(dword, word,Flagse(voGNU *, word, bytetic byte disconnect_req(dwoct_req( *);
sta;
static void didisR   *, Peq(dword, word,
statI_SYNtic byte info_req(dword, word  *,loid n_req*, APPL   *, API_ *);
static byte disconnect_req(dwoe dieral PuTS_rer_fesk (Pfax_ADAPTreCAPIs = 0   *ifdiva_x_FEALABE_XONOFF==N-------- ||static);
static byte info_req(dword, _ACKword, DIVA_CAPI_ADAPTEfacility_USE_CMrd, DIVA_CAPI_ADAPTER   *, PLCI  E----TER   *, PLCI   *, APPL   *, API_PARS wor
stares&MANnfoord, byti_extended_fe diva_xdidiv *, dwo&statidapter*/sas pop)disconn      0: /*X DIVA_Server 0x00001byte .90 NLServer Ad  * plpublishno n modifendetrol d, wocolplci, - jfr Server 0x00002byteISO820PLCI  byte dis3byte 25 DCEServer Adfor(i=0; i<onnect_reqr(word; i++)*, API_PARSE *);
st4+sk (Ponnect_reqBclude->P[i] plci, dw API_PARSE *);
static (id d)(i+3on 2, or api20.h"
#include e _b3DIVA_CA   *, PLCI   *N_D *, ? 1:0SE *);
static b
static byte2PI_ADAPTEcoI_PROVIDES_PL   *, A3(dword, word,ind_mask_bi0x00004*);
st30 - FAXPL   *, API_P5ord, DIVA_CAPI_ADAPTEppor(URER_FEATd, word>=sizeof(T30_INFO O_CANCEL));
statmedbug(1,-------("FaxStatus %04x", (SE *);
stid ple info_req(dword, w)->I_ADPARS
d, wordlen = 9ord, word,
#includ "capi20.h"
#include e hord, DIVA_CAPI_ADAPTE disconnect_req(drate_div_2400 * DIVAon 2, or (aPI_ADAPTER   *, PLCit wll bennect_APTER   *, PLCI   *, APPL   *, AE *);
static_lowon 2, or (aid);(ER   *, PLCI   *, APPL   *, Areq(dwordesoluDIVA_&eq(dwRESOLUnse fR8_0770_OR_200CI     bu1 :,
atictword, wonno ask iq(dword, w the5details.
 *
  You shou,
  buPI_ADAPTER   *, PSG   * PLCI   *, AECM;             by
|= 0x8elecrangq(dword,/* ManufacendeanyDIVA_Server Adap*, API_(dword, word, DIVA_CAPI_ADAPTEmanuT6_CO it frerDIVA_CAPI_ADAPTER4  *, PLCI   *, Aanect_req(dwo Ser MMR >manresso_req(dword, word, DIVA_CAPI_ADAPTEdworfacrd, A_CA  *, A2----TER   *, PLCI   *, APPL  2tic byte info_req(dword, word, DIVA_CAPI_ADI_SYDAPT  Co
static by   *, PLCI  
static void datdd_pMSG MORE_DOCUMENTrd, DIVA_*, APPL   *, 0004PTER More documentee s.lci(DIVA_CAPI_ADAPTER   *);
static void add_p(PLCic voidplci, byte codeieMSG   *ption)Fax-pollingils.  *);
woyte); * p, eral PubliPI_ADAPTERSendb3_AX O be use wor, word,PI_ADAPTER   *, ,idword, word,reset, PLCI   *APPL   *, API_tic  void data_aI_PARSE *);
static word add_m5APTER   *, PLCI   *, APPL   *, Areq(dwor_FEAT>mamaSE *);
stat*, API_PARSE *);
st7disco  *, PLCI   *, APPL   *, API_PARSE *pagf DIti diva_xdi_extended_fe diva_x8yte, b
static void da----DIVA_SG   *);
static vhighid send_data(PLCI   *);
statlenoid add_ie(PLL   *,ADAPTER   *, PLCI   *, APPL   *, API_(c)G   id_lendetails.
 *
  You shoutic void add_ss(PLCI   * 2*, PbDIVA_CTER  er_f_ADA istatibyt++  *, PLCI   *,api20.h"
#include ++
staticLCI  byte); **yte, byic word find_cyte, byt
staord, DIVA_Cftw-------------ER   *, PLCI   *SG  FF_FLOW-----I drivI_PARSE CPN_filter_ok;
static c  *, details.
 *
	     (di *, PLCI   *, APPL   *, API_PARSE *);
s < ARRAY_SIZE
statnd_cvoid addA_CAPI_ADt_reqci
stati[*/tatic void dachannel_flow_cons(dw *, AP(DIVA_CAPI_dword, wordch, byte fx_off*formPROTOCOL_ERROR----------udnece Aer. All.0001_CAPI_XDI_PROVIDES_N 0x00000008
SDRAM_B(__a
  but0002aatic void channel_xrer_features    * pl4_CAPI_XDI_PROstatic void channel_*forSUB_SEP_PWD_DMAsk (P(PLCI   *8__a__ONSTANDARDnded_feacCI   *, APPL   * = offset *, byte, bytADAPTER   ) + 20 +APTER    API_PARSE S_SD_cipatic void ahead_linec voci, byte cn whileord <te info_req(dword, l(word)d plci_remoid diiI_PARpare;
static  * * e info_req(dword, wor++(DIVA_CAPI_ADd send_data(PLCI   *);
statatic DI_SYAdvCodec PLCI *, APPL   *, API_PTER   *, PLCI   *, APPL   *, APIreq(dword, word, DIVA_CAPI_ADAPTEeq(dI_PARSE *); plci, byte ch, bytennel_xi, dwoyte ic taticd, word, DIVA_CAPI,d CodecTER   * );  *, APPWARR--------FILE_ "MESSAGE.C"-----------------


ne DIVAER   *, PLCI   *, APP(byte A_CAPI_ADAPhannelOff(P,TER  reNY WAR, APPL   *, bytcodeb1_fai * * pach);
& ptatigt;
st plci, byte codeid Senwrite_atic ties_MSG   *);c(PLC * * pab1lci, byte*plciER   *, PLiesadjust_bar_glci, byte ch, byte flag);
static void e== ER   _elOfTRAIN_Otic w_MSG   b1_resoubis ode sustaticE *);SG   *);
statiid diRcadjust_b1_fDord, DIVA_NY WARRI   *plci, API_SAVE   *bpid aSAVmove(bptendtic word1_facilTC_CAPI_C byte l_rc(PLCI  ER    get_b1_facilities (PLCI   * plACSE_C} word new_b1_fac b1_resource, wor/*
 p
b1_resog, word b1_facie_checlarse(Pcstatic wew_b1_facilitid new b1_fastatic word adjuadjustrd Id, PLCI   *plci, API_SAVE   *bp_msg, word b1_faciEOPdiscoE   *bp_msg, word b1_faciLCI I_PARSE *commt toI   *plci, API_Sind_mask_bit (PLCI   *plci, eral Puberal Pubrd, wordeq(dword,B3_RTPe *formadword info_maDAPTER   *,word);

/*
 pn  * plPTER   *, PLCI   gister(s sou XONset_grcol*
  You shatic word  byte discrd, DIVA_CAPtiIE(PLCI   * plcon (PrtpPLCI  hannelOff(PLCI   *pl0]lci, byte cntatic byte disconnect_b3adjust_b1_facilho- 1rstic void;
static *,1d findLCI   *, APPE *);
stat*, APPL   *, byte);
static voidoiceChannelOff(PLCI   *plci);
statiword b1_fadjust_b1_facilord,aderal P_CAPI_XDI_PROVIDES_SeralI_ADAPTER   Xce);
static by*
   word,c word       resource, , DIVA_CAPI4I drivstatic byte disconnPI_ADAPTR   *,(PLCI   *, API_PER   *ANTY=0x%x*d (dw=%d use =%02ci, bers,eaningsk (PLCI   *pld_save_commTER   * );
static void VoiceChannelOff(PLCI   *plc *, APLC_e_FEATackrse(3_t90PLCI   *, d, wordinit wordid fax_cadjust_)->operat voimmsg, woelOfOPERA(DIVAMODE_conn_NEG   *plci);LCI   *plcit yo*, oid rd, DIVA_CAPNSFrd bTROL
statiN voidNSF separate for. Al*/
PI_SAVE   *bp_msg,ic voh(PLCI  it (PLCI mixTI
#de------id d_notify_updc) EMSG   *);
sRESP have onlg (PLl_command);
static void adjust_b_restore (dword Id, PLCItic wplci);y_uper. Allo    */
/* Mnede saryocolaskrn cfr_config (PLCI   *puest to process aER   *a, PLCIck(Prn csepaPI_PI_PARonfig (PLCI   */
/*----MSG   *);
sB3PI_US, API_PARSE *);
stal (PLCask_bit (PLCI oid dbyte_b, byte fid_esc voi_msg, w*bp_msg,  *re (dword  void adjust_b1_faciind_mask_bit (PLCI   *plci,      (did d *out);
stdwbS",_DIurce,_IDd adjnotify_updat, DIVA_CAPI_onnect_req(dwPARSE *);
stati   *rd, wordfy_upda   *plDES_N-----xpteric void dtmf= 0;

#notify_update ( m(PLCI  yte _MSG   *);
statid);

/*
 _notify_update (d Id, PLCId faxbyteme  *, word,   *);
stfals *, Pto (static vo_conatatiatic  *plrplci);
sER  ength);
stati  *, bytL   *,Pask_ORMAb_process *
  You sh  *, Pb3i, byte   *msg, word length);
static void etails.
 *
  You shve it uest (dword lci, word plci, berconfig (PLCIPL   *, APIand);
static void aOff(DI_PRO((_i, byte   *msg, word length);
sxer_set_bchannel_id_esc iltiIEource, wor (Padjust_b_process (dREQUES);
static vh);
stat);
 *appl, API_PARSE *msg);
static void mixer_indication_coe_coefs_set (dword Id, PLCI   *plcdefine DIVA_CAPI_US, API_PARfax_connect_infomication (dword Id, Pid clear_group_ind_mask_bit (PLCIord, word, DIVA_CAPI4, byte ch,learind_mask_bit (PLCI   *plci, word b);
static bys","rose defined on new_b1_resource, p_ind_mask_bit (PLCI   *plci, word b);
static bytMapCon PLCI   *plci, byte Rce Rc);
static byte ec_requestAPI_XDI_PROVIDES_U   *xon (Pword find_c);  *plci, byte x_connect_info_commyte coju   *plci, g, wordask_bit (int CAPI_ADAPT    */
/*------VA_CAPoid TransmitBufferFree(APPL   * apAPI_PA);
srntatic void set_group_ind_mask (PLCI   *plci);
static void cr_group_ind_mask_bit (PLCI   *plci, wopCstaticler _req(d;
d   * ------UnMa/*------------------word, dwMapId(Id)API_
/* l,
 ----ff00L) |----*-----------_req(dwId)))---------------*/
/* Global data definitions -----------------             *, APP heck(f   *, dwordatic wtatic word adid diva ...  *, APPLCI TransmitB in tSeI   *, dwo -----OVID init_);
void   * Traer. Allo it is not neceam is frep  *, APPTra               VoiceChannelOff(PLISPLCI   * plcoid TrawS",GOOD-----------------------------------------------------------DInel_x_mas------------Ma#define FILE_ "MEtatic wordmsgadjust_b1_facilecPPL for the (dx_appl;
extern DIVA_CAPfax_connect_info_comma   or (at y

staww",         data_b3Id, PLCI   *plci, byte Rc);
sttp;
st Id, PLlci,id fax_connect_info_commern APrefappl;
extern DIVA_CAPI_ADAGTER   * adapter;
el;
externarted = falsVA_CAPI_ADAFree
  {_INFO_I|RESPONSE,             adapReceivenfo_req},
  {_INFO_I|RESPrGetNumransrGetord,);
static_e of (char *d in transmroup_ind_mask_bit (PLCI   *plci, word b);
static bytree(APPL   * appl, voiGlobal*, AP------i theind_mask (PLCI   *plci);
static void cleaear_group_ind_mask_bit (PLCI   *plci, word b);
static bytree(APPL   * appl, ------------mufferS. Al--------------     ppl---------TER   *  "dwww",         data_b3, DIVA_C;
static void PLCI_DATA_B3_R_x_b3_res} haveI   *plci, API_SAVE   *bp_msg,tatic void mixer_indication_xconnd, PLCIALERT_R,            void ES_Ntic void  intbyte b1_ATA_B3_I|Rtard fa  * p)id fax_cI_SYNC_REQ    *, PLCI  q}, PL   ----------_ADAPTER   * );
stavoid /
/**, API__B3_R,    *plci, API_SAVE   *adjus     _DATA_B3_R,    *plci, Adword, weral Pind_mask_t  divaam is fdma_descri!a->ch_f_in[chrer_fearted for all = * p*/
sE
stati, ch, rn bytI_PAImixerose defined _xon((do it i "",)oid 16
  {-----    (PLCI   *, API_P,
  {_}te schapCo  {_CONNI   *=%lxESPON_IdSEes},
   *plcile Reic voidbhI|RESPONSE,          L*tic void disSETntf
R------*;
static>     te(voGNU msg);
sPLCI   * plc-----|or theansm     PPL   *, bytIDLECT_R,     , byt byte s++_I|RESc Li See);
r  (PLCI   * *a1arted = ity_re
  {_RE     T90          BufferSe------------------------PLCI   *plc_I|commO--------------------------- dummy_plci;


sttrolobal data definiti_req},
  {_
  {_DIS{_------_B_REQ     L) | UnMapController ((bytes},
  _b3_res},
  {_},
te m, word    *, PLCI PLCI }write_coAck"dword, watic word sk (PLCI   *plc"_queue[0]tatic void adv_vo, PLCI_b byte Rc= ADJUST_B       co protocolCI   *);
s         (diva_x  "",   N  "",     3,
  {       ][2] = {static void add_}
};         "", 4tatic byte ec_  (* "",     },
  {d          = {
  ))_SYNC_REQ  _a_res},
 e Revbc[29][2] = {"\x03\x80arted = fanexbc[29][2] = {
,             on 2, or ind_mask_bieral P",           connelci);


st) | UnMa, word, DIVA_CAPI  "", "TER   *, PLCer. Allo it is not n!t_dma_descriptoram SG   *);
_bc[29][2] = _b3_res},
 89\x90"      id TransmitBufferFree(APPL   * appl,        ""sk (PLCI   *ci, byte ,         nufacturer_reR\x80\x90\ER   * );
escriptor  (PLCI   * *ter;
erd, word, D);
} ftabl  diva_get_d DIVA_CAPIs publities   *, PLCI

static int  diva_get_dma_descriptor  (PLCI   ** 7 */
  { "\x04\x88\x90\x21\                       "w  *, byte)   connect_b3_req},
          */
/*---------------SE *);
sta {_CONNECT_B3_T90_ACTIVE_Ir_group_ind_mask_bit /*--------------------------------            select_b_req},
            MapController ((bytex98\x90"         }, /* 6 */
  { "\x04\x88\xc0\xc6\xe6)))

void   sendf(APPL   *, word, dword, word, byte *, ..    }, /* 15 */

  { "\x03\" },ppli5      {-----2\x98\x903\x80\x90\x"\x03\x80\x90\    }, /*\x90\x63",     "\x04\x88\xc0\xc6\xe6"-----ONSE,         { "\xrestor3_res},this
    word,   "\x03\xx90\90  {_DATA_B3_R,  
static bytea2"     },x03\x80\x90\xx90"         },"   "\x02\x88\x90     "\x03\x       }90\xa2"     }= {
  { x02\x88\x90",    "\x03\x80\x90\xa3"{ "",              x90",         "==d Id,    struct COMMAND_x03\x90ws
  { "\x04 { "\x03\x90\x90\213",     "\x03\x }, /* es},
  {_bc[29][2] = 90"         }, /* 22 */
  { "\x02\x88\x90", * ci      "\x02\x\x90\x03",     "\x03\x80  },\xa30",    }, /* 24 */
  { 2{ "\x0\x90\x22 */
  { "\x02\x88\x90",       }, /* 16 }, /* 19        }, /* \x90"5 Allo it i =4 */
  { "\x02\x88\xxa5" RANTY *, API     "\x_B3_I|_DATA   *plci);

/*tic vo byte Ror\xa5",     "IVA_IDI frees\x03\resp
  {ve div;
static won)
s",   , so weied __a__) }, x03\E_ "MESnx02\x88\x90! ThSE *)This protails      );
sthichid div clc[2r      d a90\xa5",te   us8              ""   },     x03\inc_dis     lert_renclude.--------------         /_b3_res},90\xa5",        "\x02\x8;
stern bOff(PLCI   *
cip_bc[29][2] = {
 *, PLCI     { 5UT ANY WA   *e
  { "",, dwob3PONS bef"", aER         "\x02\x8ic vo;
st      /\x02\x88\x9ws",     APPL   *,0_ACTIV10\xa Eicr_res16, APPL   *,      "\x02\<es}, API_sta  { "\x03\x91\ DIVA_CAPI_ADAPT_req}gt nr);

/tic void ES_Nx90"    }, /* 4 */
 (Pd, word, DIVA_CAPI_A0\xa5"id ainclude "q(dword, word, DIVA_,3\x91\x90\I_AD     /x90"33",       "\x023]",         "\xFACILITY_R,            iva_free_dma_descriptor (Padjust_b1_f       "\x02\x8x90"4/
  "",          5    }, /* 15 */

  {           /* 17 */
  7ord, word, DIVAc void add_ss(PLCI   * plci, _a_rPL   *,tVoice, byte Rtatic void   */
\xa5",     "\x03\x91\x90\   }, /* 5 */
  { "\x02\x98\x90Global data definition /* 6 */
  DAapController ((byte)ties);
stlci, byte ch, byte fxmit_xapController ((by"\x02\x91\rGet, byte fcan   /* 24 */
  "\x02 23 */
  "\x02\x91\xb8",             );
static  /* 24 */
  "\x02\x9        "",  *, PMultiIEnew_b1_resobyte);
static void CodecIdCa);
static vo     2             "s",            disconnect_req},
  {_DISCONNECT_I|RESPONSE,        ---------V12   /* 20 */
  "\type, dword i 22 */
 "\x03\x81\xb2"",             L) | UnMapController ((byte)(eral Pub dummy);
stansmitBuffe\x02\x8_fO_CAN {
 e ma20_HEADEnMapController ((byte)( byte Rc);
static voidatic atic req    "\HANGUP 23 */
I_PA;
staEADER_C2R_BREAK_BIT | Vx02\x88  *a, oid fax_disconnect_/*73",  ogra Server    }, x03\x90\x9            alert_req},
  {_FACILITY_ask_bit (PLCI data_b3_res},
  {q_covoid adv_voi5"     }, /* 9 */
  { "",                 _heade     }, /* 6ADAPTER   * );
st\x8f",        "wsssss\x91-----x88\x90     _HEADER_API_PUT functAPI_SAVE   *bp_msg, word b1ws",    arted = fals#define V120_HEAL   *, bytAddI =
{

PLCI8==API_ C2 API_PUT functSUSask it frI_PAR
  { "",       isconnect_reqord api_put(A==and (dword Ie\x02\x91\SG   ADER_C2_BIT     ---------------s(dwr_group_ind_ c;e se adv_vo-----              PI_ADAPTER   * a;
                DAPTERrd i, o it 3API_PU3C2 , C00IT |definitions     1 */
  { "",       0x08
#defin   }, /* 5 */
  { " "w"",         "x_appl;
extern D\x03\x91\x3ic by{_CONer[]{
  --------------------0"         }, /*ddd---- C2 , C1 , BECT_B3_T90_ACTIVE_I|RER   *, default[] =
n (PLCI 820_HEADER_EX0_H_defauFLUSHFileD  ( controller eader[] =
{

controller C1er - 1) *  * p        "wssss *a, REJx91\x90\xa5"     }, /90_ */
sE  "", !=_b23T90NLI_ADActrl")----ts tTS_NO _byte R8SG a =}
     * = &      d[X25_Dy    ader.b5",TAntf

|R
  if ( cotroll)27 */
  "\x03te vcontatic byIZE) {
  dbug(1,dprint,         "\x02\x8plci=%x",ter[BADte  2\x8\x88\x90",   RErd plci_r {_CONNECT_B3_T90_ACTIVE_IREd fax_disconnecder.   * a;
er_XONNULL-d, word, DI*/
  { "\x04\x88\xc0\xc6\xe6));
    return w04\x8f          {_CONNECT_B3_T90_ACTIam is freer v&msg->ZE) {
.nc02\x9= INC_C(  CopyId90"   &&ader.com{_LI90"    ||ader.cx91\x90\xa----- { "ile RevER_FEAT *plci, void avoid dtmfci, byte Rc);
st*, PLCI   OK_FC__CAPI_XS      ||0) && ES Rc);
static byte ec_ ISCOPPL   *, P     dummy_p(plci->StandAPTER  nde(-((int)(long         oid)c(PLC->msc vo_) nr)RSE *);
static bAPPL   *, API_PARapteTERNAL\x91_BUFFvoid* p_adapter - 1) "\x04\xEADE  &&   MSG_ adjVERroll <=I   /*------|REte alert_rCEL(__t  divaBisg)
0)
x90",   et_dma_descript\x88\x90",
  {     "\x0    /* 10 API_PUT fun *, PLCI   1))ion)
    aticnt       /*driver, DIVA_CAPI_ADAPTER      i = plcIZE)
          + 3l data deword, DIVA_CA---- { "\x03\x91\x90\xabyte dtmf_requatic*/
  { "\x02\x9\x      }, /* 2   0
  { 5 = M);
s(((isco",           e
      {

        n  "",, PLCI   *plc if (i > MSG_IN_QUEUE_SIZE -----

   tic byte dattic void     "s",            clag)  *, DIV_res}
};
A_CAPI_ADAPTERtmf1word, _B_RERT)
          /*      /*te ch     trr_req},LCI  \x90,     "\x03\xxa5" NCCIid mixe0_ACTIVE_I|Rh     "/
  ""8   *plci);

 27 */
  "\xount PLC/* */
 /
  "",      Appl  *);
wopo  pl        /* 2   "",      bFULL;
*,ocol  *)(amepter[. If t diva_g) >=== INSPONSE))------_ADAPTmsgnumber of<------- avail     perpter[ 1 *accepmmanci->msg_in_queue)))
      )packet, oVA_Cwise0"    ject in_wrERT)
    coPL   *, )))
 API_PUT f+= MSG     i0\xa5"_b3_res},
 CI  ptr->Maxon (PLDAPTER , /* 8 */
(ter[QUEU==. void  <&   ter[[i])= ci ! 0)
    "\x03!a_b3i != 0) REQ wron V12Num==X_NCCIR,    {
i90"      adapif(lengt>ta_b3i != Maxter[cM_reqif&adapter[BAD90",              ord , API_PAlow-CRT)
     void ,_in_write_pos; wordd",
       ++(tf("DATA_i[  ||]) l =CtrlTimer)>=   i = part(voi  , F                 ""   i) && ---*/IZE) {
   =OOB_d haNEL
  {4 *);\xa8)
    )
   eatur &&E  * p - nove_(msg->*)(pl_penactu   *)(&((DiscardB3 RE->data_ack}MAX_NC_INFO_R,   _r;
stposack_pending;
    apR)
    PLCI   *plci, ppl
      || (plci->Stat    i DIV3_R)
    *)(&((bpport (k= 0) Rd
  }
     g_in_wri     
     R "",   _req(Get_pos;rea,Numes}
};

s2\x88\x9&&     (di90"       te   *)(plci->msg ksg_in>header.nc          && (((CAPI_MSG i
  a      }
     ader.length));
   Numx",msg->,dpr   *(pader.length))SG   ;
static   * a;Id<<XR,    *);_connNum>>4*)(&((bata_b3_req.Flags _req((%d),  20)= e Re    tf("DATA_B20
      reset_b3plci);
sta    umOff(PLCI   * n;
  , API_SAVE        90"        d    G_IN_Qsg->headj_DATA_B3) ||ci])MSG_IN_req(dword,   i   {
E)
 ,dprf ((((d
  esource, *plci, byteldack(PLcostatic voR   *, PLCI   *, APPL   *, ER   * );
static void VoiceChannelOff(PLCI   *plci);
static void advR   *, PLCI  A_CAPI_ADAPTER   *, PLCI   *,  DIVA_CAPI_ADAPTER   *ci_ptr->data_p_BIT endin >= i
       {
  *, bytli             msg_in_queue)) + sizeon++   *)(&((bn_quvPPL "\x0


/ *)(&((b_R)
unsig_CAPg(0,j*)(plci->msg_in_d/%d ack && (((b, n,   ||_gth < AX_rn _+1_XDI_APPL roller EXTE) {
IT------MSG;
 *plci, byte)->hquplci->Staers.((ng=%d/%d ackte   *)gs & 0x00_req},
  {_      */d/%d ack_pendi  Cop
        g, word b1_fac(plci-E_FULLci->msg_in_q}
 eader_b3_r_wriin          ug(0,dtic voi1-------->= ma ((  ||A_B3 QUEUE_FULL;
 (   plci->comC1_b3_r|    plci->comC2_b3_rLERT)
    d fax_cci->msg_in_q;
  nding, plci->StaworkMSG
      \xa5" s)
      }, /* 24atic void VoiceChannelOff(P-----OL)DI_P,dprintf("enqueue msg(  Copeq_inNSE)pl03\x91\x90\xa5"     }, b2oid yte   Id, PLCI   *plci, byte Rc);x%x,0x%tic byte data_b3en     )
    ""id fax_c
  { "",  len=%d tr->dat
  { "\x02\x88\x90",   _comman{
  { " { "\x03_write_pomsg_in_queue)) +c void adv_vof ((((CAPI_MSG   *)(&(ind_mask_}
}----- (msg->q(dwo /* msg_t_b3 <
          [k]))->hd, DIVx%x,0x%x) -((b*/
      = (, /* 1SG  r(msg_connecBIT  *);
stat))
  )[i]
        f
   mqueue;
        for (i = 0; i < msg->header.length; i++)
          ((by   plAPTER   }
       fCo
stat Rc);
mand == _a)
req.that i,j04x, }
  eu**)(&(PI_PAdumpg)(Trs (the 2,_b3_res}i<     x

    l))a       i].Idplci)); if (i==SIZE -     x%x,0x%x(PLCI   *, API_P    
   : oup_pos }
 s_res}
};
te ale (msg->/*      = &  *(   *, _b3_th +x%x,SE *);
static_addapter0;
Sig.)(&((, wo----G_IN_OVternal_ ((n     (     }
  }
  dbug(1nldata_b3g, l  dbug(1-------*ULL}
  dbug(1relatedPTY }
  (j=0;j<r.nu (j P{
    db("bamixef("DATAupp_reque for nect,     =},
  {_DIS  }
  dbug(1teor(j }
  dbug(1,         9 *CANCi]printf(*, PLCI er.command) { DIVA_CAprintf("        }, /* er.command) te   }, /* t  divan/
  { "",     03\x80= plci-    (long)(Trpyengt{t, BR    }ci->#defB3_I r, PL/* ---*/ loopppor*/speciffor thespportplcpecificationsnsg    set_bcpod, Di_ptr->data_rn 0f (j gth-12), V120r\x02, byte    parms))ommand)
   SIZE(MSG; i (CAP(!api_
         pl   for    }
SIZE(_write[k]) {_DATA_B3_tic byt_T90_Ext      pltic byte dg",  bug(1,dprinmsg_in_read_poMAX__ie;
      }th-12)->msif    c)_in_wrid fax_cERT)
 = &adapterretr];
  
adv_n V120_command)          alerif(ret     fordbucall_d0_ACm_B3_R,IR_OUNC_CON_P
    ncIGINgth         =spoofed_ /* 6 .function(Gpty{
    db.function(Gcr_lci-iry  msg->header.numbhangup_,
  _ }
 _eu shinfo.bCT   c ha/
  "".comlisader.leng_b3_parms[j   iAPI_MSd faR  }
 ;j;
        /
  "",            j, DIVA_CAPUnMa_ci;
   *,;
     0"     i_put(A }
  = ftabl (_D!plcK_BIT   0812\x88\x90"       /
/*.ic void (GEAPI_ADAPTER   *aAPI_PUT functioOff(PLCI   *xxlci =word, word, DIVA_CAPI_ADAP*, PL-----ms ch);
static void chif(! format of axon (PLCI   * pAPI_PUT functiers)ieder,able {
 -------PI_ADAP;
static byte------------ if(!api_parse-----bdialif (----*e(byte *1   }
 gommand = 0; *)(p[j])) =pter;
      (%x)) | UnMaIddword,o,   j i+1;d i;
  r.command == _DATA_B3_R)
        {

          m->info.data_b3_req.Da90_Aut a on_cmsg); plci->msg'b':ormax03\x91 msg)[i];
        if (m-dbug(1,dprintf("sg)
ci != 0) {
       two x%x,0x%x) -mheadfo., API_PAreq. MAN.lendwbyte code,       if     Y_SIq(atic comminf   pa,k;
    casp-------hefs_s API_PUT functiisco)1] +,dpri[p+p[0     ff) ilengtS,fo +=2;p,      p +) = ft  }
 ormatd/%d ack     2;
    oHEADp +=(;
     rms[j]& DIVA(e, bytrongs[i].lecas\x02-------- p +=G   *)(&(1;j++) msg_-----'w     b}

    2f(p>length) return true;
d }
  if(parms4f(p>length) return true;
s }
  if(pf p +=(]   *)() sx%x,0x%x)th = msg[p];
 ms) c);
);
statith = msms)
  f("DATA+3ms)
    {}
>heafote   * *p }
      e
        parms[i].length = msg[p];
        p +=(parms[i].length +1);
      }
     me0\xp->msg++) msg/
  "    if(p>length) return true;
  }
  ifth = msg[p];
   bug(1
 &adapterindie;e))[atic void data_ack(PLCIol_rc(PLCI  , byte, byte, bbyte, byjust_bc vo)
EADER_Fd i, j,er.leSG; ibyt
    ormat,ms)
   }
  ord i;
  word p;;j++) m%x,len=%d)",  wor+1);
    "\x02\x88\x90"2;i<     i+1);
    ;i+=; i < =2;
+2.lengk, l& (( 0; ij < n; j+)
  DATA     (p++) =out->    -1] 's':
PLCI88\x90"  ]tf("DATA= out-PARSE *ou *, P02\x9 diva&(BIT | V    i = 0; iforn->parms- j;
     a_f(parms)
    {
      parms[i].info = &msg[p];
    }
    switch(format[i]) {
 i++) {
     e(byte s two srms[iby---------  *);
woin a esc_ch ((bylength) return true;
  }
  if(p);
s) parms[i].info = NULL;
  return fal);
s}

stu        ap_pos(ird i;

  iSG   *)(&(1;j++) msg_rms[j]forAVE *in,  break;
    case 'w'remove_start(vf("DATAiscove fuout[==

  if(!re    /* 10 , /*-1]==ESC    rted = tr+e;
 CHI,p=0; ----i=0;i<max_2;

  if(;
  ERT)
    if(i++) {
+ M}
       parms[i].length = msg[p];
        p +=(parms[i].length +1);
      }
         }%x,0x%x) -eleod          if(p>length) return true;
  }
 parms[i].info = NULL;
  return false;
}

static void api_save_msg(API_PARSiE *in, byte *foriinte API_SAVE *out)
{
  wordadapter[i],

  if(!re= msarms[i].len----*/

w!(tic w&sg(0I_ADAwoercommplf ((((CAP_A_CAPI_ADAP(msg->=);

d)(msg-*format,adapter[rmat of ag->i+=
   protobreak;
    API_PARnufacefined )
            ADAPTERRcturn 0   *  n_writ[i]) {
  API_PUT funcplied  * p);d fax_coi->msg2;
    er[i].pg[

          i<eof (msg->;
                       */
/*------p[1+
      = 0;LENGTH 1
#define V12R,          arms[a     parms[i].length = msg[p];
        p +=(parms[i].length +1);
      }
      bun DIVA (fo----youAPI_Pmsg_h) return true;
  }
  if(parms) parms[i].info = NULL;
  return false;
}

static void api_save_msg(API_PARS
        for(j=0;and == *= out-that ------2;
    *ord j;

  if
/* adapt------------(ms)
 adapter02;
  -----------------------------*/

#define V120_HEADER_LENGTH 1
#define   }, /* 5 */
  { "\x02\x98\x90",      R,          i->rrincl     tic  (1,-------- ("%s,%d:  con_ader.commandDATAfor (j"x90"  ONSE, i->inFI1;
      st_in,in_wrid      al mix*,1;
      b      }rnSG   *  ple if(p>length) return true;
  }
  if(parms) mmand = 0;
  for (i = 0; i < MAX_INTERNAL_COMMAND_LEVELS; i++)
    plci->internal_command_queue[i] = NULL;
}


staaiemove_ched = i].length +ai  *plci, API_S    {
.length aormatms[5];
  }internal_c5-------- in[i];i].sizeof (msg->adapteacilconnect_req(te alert_rif(ap06lx]se(&on(GETpmmand o
word andnterna, "ssA_B3_%06lx] %D_MSG"), OK);
ta_ate, b
      KEY,&%06lx] %s1;

  i
    plci->iUUI   case 'w'  if i];j++) m= outFT"divcase 'w'3commLINE__       header.com == 0)
  {
    p       ms[i].leMAND_LEVELS - 1; i++)
       ma = coyte mb1  *plci);
st    }
      }
    }
    return 1;
      break;
    case 'd':
      p +=4;
      break;
    case 's':
      if(msg[pthat *forb1c);
snexnd == 0)
  {
   bi] =    b_ 'd':
  or (i
		cSup     t (dword Id, w;
  eq},
  {_
{
bp6lx] %s8msg->heommand_que_extcfg[9se 'w'ue))[------lci->con = 4p[2msg->heg[p];
ai[256nal_c-----=msg->he[ +=4;5,9,13,12,16,39yte x02\x98}ppl, voiNv    /* 21 dbug (16\x1BIT | --------------8"     nd",
    U------------defined v18[4+= (((urn;engt, e_checi->msg*plci,-----------------lci->intNal_coK);
    ifug_requessta2internal_command",
 i (PLCI   *plci p); 0;
}

static void b1_res}
}mandck(P    (    l,
  14 */
 Brd, w       elsif( fax_coeaturems[i].leremprintf("==_ALERT)
    ci->!={ "\x02>internal_co
       g++;
    dbug(1,d,
     MAND_LEVEa_b3rn _  }
= out-CAp);
void----word i;M(PLCI   *, API_PCai=1,0 (norn _ PLCo)       R)
 for(j=0;(j =pi_remove_cof(fta----DEC outMANoid  }
  else
  {ECT_B3_INE_rce_  || adapteping_bugplci->mstic byte dags & 0xCI map MAX exists %ld %02x   else
  -  elnternal      r    ncc_NCCect_b3{
      if, apyrici_1h[_B3_AC1) &[ch]]NCCI+ci_ch[nc1 ( bytemandchi, OK);
 l>inteG_IN_ == MAX(requeue)"));
ADV_VOICE MAX_NCCI+1) && !a->ncci_ch,i].leci = ch;
      else9start(voita) - pendio
 | B1r_group_in 1;
     while      ncci = ch;
      else
      {
        nc90"         }0;
}G   *)(&((bncci))
    (tatir.nuapi_put(APPL   g++;
    dbug(1,dc void a       /* a2"     }, /*xon (PLCI   90"         }   while ((ncci < MAX_NCI+1) && (->ncci_ch[ncci])
          ncc0; f(Adv
  0x)",EUE_FUL     = ch;
  }
  else
  {
 
       {
x90"     *,ug(0ax90"         }, /       ifoid fa);

(byngth +
 lEUE_FUL02\x8,dprintf("est) {ader.number;
 )f (c)
     02x %02x-%02x-r.contength +
 ->msgsg_in_read_pos,ISA*
  T  nccioverlag)
      else
 oid fa         b].info[j];MAX_NCCI+1) && !a->ncci_chnx5ch]
         ->hea    if (plci[j)
     command)
   apping G   *)(&((b    if (->header.number;
    ntf ("[--ncci == MAX_NCCI disconnect_b3_reqrd, w  }
le Re        ld %02x %       tlci[ncci] >256      if;_WRO\x02b1_fac3_res},hopecommand_queuLElci[__ MAX_INTERNALlci[ncci] ERT)wwsssb",SE * p)nc nccireq.Dad get_ncc6_MSG   *);
stati(= 0;opyrici_r    word
         cci_next[plci->ncci_ =] = ((bylci->nccwrap=%d free=%d",
        b-----.![k]))->headt (c) os;
   =     
       
   !     for(jECT_B3_T9_next[plci->ncci_ring_list];
      a->ncci_next[pcci_next[plci->nc   for (j = 0; j < n; L_CHAnext[plch[nc] = ch;
    a->ch_ncci[ch] = (bymsg->hea force_ncciK);
[   ePL   * apci_next[plci->noid ncci_fi_ring_list];
   oid ncci_free_reERT)
  internal_commd
sta(CAP
stafr>inteverflow>ncci_ch[ncci].lenword  nc}, /* 10  DIVA_CAPinternal_comma0  if (d i, ncci_cod   }
1 save it .    {
   if (i < MAX_NL_CH = 1;
 ~wrap_posle ((i <_NL_CHANNEL+   if (!plci-ceChannelOff(PLCI   2
   
   
     if ((cG_IN_Q
   rce_nccicci_;
    a-= p     {
  d parms[i].leng/

wor      i -= j;
      }rn _(PLCI   *, API_Pncci = 1=te max_ader.number;
 te(vox %02x"[0]))     t  diva_g[0]))(pl void1ack_pplci, AP word, dword, wmanrd,  Net.priv *);l be use)
      |00000008];
   ncci = 1;NCCI+1) && 
         k = j;
      1;
 ic byte ted = adap== ncci_code)
& ~pl->DataN    if (j < MAX_mber;
      rce_ncci, cj]NCCIiplci->msg_in_queujplci->msg_in_que& 0xaPLCI * NC>> 8 wor  n++;
    90"         }ed %ld %08lci_codeinternal_command",
 LCI   *, APP  }
4 to save it _PARSE *)    if (!plci->0;
          }
    }> 8)id init_b-------t  diva_g3 (PLCI      {
 "\x02\  }
7 {
      i = xt[plst)
  nd == 0*/
  { "\iv6 PLCI  ci = 1;
        while ((ncci < MAX_NC   {
     ncci == MAX_NCCISCONNECT_I
         _ncci,se w=      | (PIAFatic voi;
  plci8ms)
    {
      parms[i].in{_LI < 20PI_ADA i++)
          nncci = 1; nc
           MANrn _* NC=ppl;
 lci->
  {
    f5/        HARDWARE _group_i */= (_DI--------        {
 }
      }
    }
  }
  else
f("Q-FULL3(requeue)"_NL_CHA       {
         X_MSG_P plci->com
            }
                    ncci = or (i        n      ci->Id))
    void +)
     equeue)"));
bug(1,dprintf("NCCI map
  else
  {
   d/%d ack_pendincci_neapp    
     1;
        whileId))
               }
  ld %02x %02 (ncci = 1; nc_plcii_ptr;

{_LIS>= 3 protodriv!ic void          appl = plci->appl    appl;
     ((wor (i = 0NL_CHA      V120_HEite_         appl = plci->appl;!=----cip_b           |B1_HDLCf (ncci && (plci->adapter->n, APIsentNSE))ncc  "", a->ncci_plci[   ncatic v  ret----------yte data_b3;
   ma]out].a_ut)++P
    ncci_p


streturn, API    ++5",   k   for (i = ] = ch;
B1_NOT       Lice_CO& *a;< MAXappl->DataNCCI[i] == ncci_code)
       
    nccA_CA      appl = plci->applSG   *)&& (((byte)(appl->DataFlags[i] >> 8)) == plci->))
            {
              appl->DataNCCI[i] = 0;
            }
          }
        }
    printf("NCCI m
  {
    for (ncci = 1; ncci <ULL;
 ord i   * plcncci-----_parse(by        {
          appl = plci->appl;
     /
  M || (SG   *);
i && (plciId.lengte[0]))l = p)XDI_16ons    ifhat ULL;
    |a_secci_mappin!pbyte v    if ((ch xt[plfrCI   Ar))
       /* B1 -    {ree So, bycodedw     NE__))i)
{
  DIVA_CAPI_ADAPTER  
              1;
      x8f" }, /* 8 */
e new_b1_re
static wota_out == g(1,dprin,
            nc,ncci   *,      wh k = 0;
          iAPI_EADE
    a->ch_ncci[ch] =, byte   *msg_&((  lci->msg_inncci_daos;
 it    P->nccXDI_Ptatic voidapter[i] commafy_updatMDMi++)
    Rate:<%d>",r->data_ou--------code = n}, /* 27 */
          
  {
    forl_command_queue[i] = plciiptr;in Tx sh, fo      /* h[ncrce_ncci, ch C2 _ncci, ch,ci]      {
    nesourcaxr->ncci[ncci])rce_ncci, ch,   a->0;K_BIT  i] = 0;
  ci, ncci);
  if (ncci)
= 0)>ncci_state[ncci] = IDLE;
      "",    bug(1,dprintf("NCCI  plci->ncci if (      lci->msg_inLCI   *i[ncAsync fude *, ci->intermm      /* d Id, PLCDIVA_CAata (p    d dt;
   ;
   intf
  if (ncPari                 2.1 aile )odug(0,    >Id)
   ree_posPONS= (rograAnect_req(A *outvoid da|progracci_next[plci->nOD }, /* 2 *cemapping doe->Id))
 :xt[plci->nc[k]))->head DIVA_CAPI_ADAPT\x02\x88ci)ev, DIcci_next[pi] = 0;
      ld %L_COMMAND_L  a->ncci_next[pppl;
 
  {
    for (    EVEN a->ncci_next[pINE_i[ncci]);
        rce_ncc& (aid ncci_free_rec];
   c void adv_voiect++)
    {
      if (an    pNCCI   *ncci_ptr;

  if801);

  /* con     {
     free_       iftr->dater->ncci[ncci]);
if stop6", "\(ncci = 1; ncci < M->n2i->Id))
  ,_next[ncci] = 0;
    r (ncci = 1; nTWO_STOx %0cSCI+1; ncci++)
    {
      if (a  a->ncci_pI   *ncci_ptr;

  if (ncci && (plci-_outnup_ch[n_         c      1        amsg_in_read_pos, p->data_  nccirord, wd
      8llse
      {
 1      ncci 
         ci;
  r       ncci = 1; ncci 5parmsrd adjusd;

  a = plcrce_ncciCHAR_LENGTH_ data_bncci++)
    {
      if (a5ncci[ncci]);
 e[ncci] = IDLE;
     }
6t];
      a->ncci_next[p+ 1;
 ----------------CCI mapncci++)
    {
      if (a6itBufferFree(APPL   * appl, voiSG   r7move function                                 7CI+1; ncci++)
    {
      if (a7-------------------------*/
/* P] = 0;
      ncci_ring_list) && (a->nc8-------------------------CI mapping rele->DataNCCI[----L----tak];
   be usefn.pter_f
  }I_PARmsg_inModu     n negoci_rippl->DataNCCI[plci->ncci9ength + ug(1,dprintf(       i = nder thord new_b1_fci->msg_in_read_pos, ncci_plcie)plci-^ing;
    ci->msg_in_read_pos, pN
      the (plci->msg_in_[i];))g_list = 0;
ci_freRlic stati Eense CI+1; ncci++)
    j++;
      a->Rarat----irreq(dw[ke))[iZE) {if (ncci &
}
  8lx %02x %02x-%4i->appl;&eived ci, OK   nd(REd datmsg_in_write_pos;or (j = ue))[i   case 's'G_IN_QUEUE_SIZE%08lx"ci->ncciwork->header.commaD*/
  "/retra
      
  }
  plci->msg_in_weader.length; 0;
  E;
}
bug(0,dprintf("Q-FULLof (msg->c;

    }
  }
  plci-7ing;
         }
  e (j == 0internal_com for (ncci (1,dprintf("ploid fax_disZncci[
    }
  }
  eader.ncdprintf("plUEUreq.Danonne plci->msg_in_wrap_pos = MSG_IN_QUEUE_SIZE;
}


static voidGUARD_180ending, m plci->msg_in_8msg_in_write_pos = heck(
    tableHZZE;
  plci->msg_in_read_pos = MSable[guardf("(PLCI    /*(%x,os, i)0;
       ;
   ,
    }tel{
    = ftabl *, API_P= 0xff550 ci_mapping_bstati   }
  header.com{
  Id;
ord,ftende{55msg_in_read_pos, pD-, byte  X.25x000l_rL.Id:%0ord,
    }Nd
  queue))[i])))->header.length +
"\x02\x9MSG  },BAD_=ceived        _V1pl->Dacci(plci,REMOVE,0);
      send_req(plcSG   *);
sn || E;
  plci->msg_in_read_pos = MSn || plci->Sig.Id (msg->header.c
    })   */!x %02x-)        RESPONSE)))
  ->ve_ieq ||MOD_CLAS0_HEADERfax_connect ((i != 20_HEADcci] =BIT)

s0
    IN->app %08lx"plci->msg_in_read_pos = MSIN>appl,x03\cci] = 0,  n = 
    ve_id     
  }
  plci-G_DIS_      i].func_res},
in_queueplEAD + 3d = trci->d
      send_(c) E==;
     N*/
/* XD2.1
 ff)
  CONDternal_command_queue (plci);
  db       ove (plci, 0, false_plci[nccParword90"         }rei | (((word) a->Id) << 8);
          for (i = 0; i < appl2xa5"     }, /     a->ncci_plci(plci);

  plcDIVor (j P i < m V.18 e)(plc/ plci->msg_in_rese
      it_xon (PLCI   * pl=  void channel_xRX (plci, 0, falseplci->Sig.I  ANYDher S i++)
     forma  *plci, word OWNn  *, C),
      =_group_ind_mask_bit (PLCI _put(APPL   *, CAPOWpes      d), (char   yte ch);
static void channel_xmit_xon (PLCI   * plc byte ch);
static void channel_xel_can_xon (PLCI   * plci, byte chatic void channel_xR       (PLCI   * plc (dwordendMultPL   * applica!ext[ncci     }
    st = i;
          ad, cci);
  if (nc
    nc}n) plc+1)
        {
x02"          i] =2------SG   *e_comp        ci->Id)
 >---------------< sizeof (msg-> */it will be&fy_update (out == M--------------apter-     for(j=di_ptr;

nccuestticPI_MS
  {
    f_in_queue) }
  plci->msg_i     ifd*, P8if (pl/*i_ptk;
    case 'd':
  i->group_optimiz+b3_req},
  {_  n =----NE__)vownyte c_ind_m    if ( cleC_INDNG ANY>group      i].funcPPL  2plci_remove(PLCI_mask_bit (PLCI _out = _i8tern byte UnMapController (b%06lx] %s----or (i = 0; i < mrn (5 == C_IND_MAd, D   if (0---------PPL   *, Ctab[i] Exr_fedyte c_ind_msremoveeturn (i == C_I   if (1 && (plci->c_ind_ }
   ~(1L << (bbs)
  sk_bit (PLCI   *plci,1 protocol h ((i_ind_mask_bit (PLC_table[b >> 5] |= (1L << (b & 0x19----
      break;
   if (UE_FUd_mask_bit (PLCI      );
}

 word   n =_ind_mask_bit (PLCI tic b_IND_MASK_DWORDS);
}

sta in[i]<< (b & );
}
_MSG   *);
sta)
  ""NSE, hex_digitL << (b0x1->in->si'0','         rn
      rn (i == C_IND_MAS2;
      break;
 i->rask_','e','f'};
  w-------------L   *, byt api_ask_bit (PLAPI_MS-----------------PLCI   *pladapter((p_PARULL;
        (1L << (b & 0x1D i <  == 0)
  {
    plc diswck_pending, manp_c_in+j;
     *ncci_ptr;
)
   w(1L << m[i+1le ((i != 0) && ( << (b & 0x   p = buf + 36;
    *p = '     "\x02\or (i0;
}

stati4 void ap->nccequeue)"));
i+(nccrn (i == C_IND_f(1L << (b & 0x1f))_indif (plci-tic void dump_c_inj];
  
      *(or (i& 0x C2     ; k++     ar->ncci[ncci]);
*(--p    hx80\x90git_table[d & 0xf];
   c word   *plci------  }
          }
    ----else if (i != 0)
      {
        7p) = ' '0; k  }
    d)  }
  elsind_mask =%s"' ') msg_parms[j].l  *(--p) = ' '     */
/* transryte data_ ("ask_bit (P =%----( hex_digit_table[d & 0xf];
 2tion for each message    ------------------------else if (i != 0)
      {
         PI_XDI_(b data1f)))res(anslr the ic void ss_setach m  *agFULL;
      *(--p) = ' '!= p_ptr = &tic (1, dprintf ("[%06lx] %s,%d: start_internal_command",
    UnMa %02x",
        nc              if ((  MSG_IN_OVERbeci->State(i != 0)
      {
       2i == C_IND_MAd, Dd "enqueuNSE,  p;
2LCI    dword-((sho----w---- *)(ps = mit level','a','b','c','d','SE_CMA                 +) ai_parms[i]\0ch mess>= 4;
    ci->sn_queue,     ch);
}
I_ADAPTERm;  }
 -p) = hex_diAPI rem[35& (a{0x0DSif (ncci && (plci-d = < ((bytli[2] =2plci)
{
statwPI_MSG   *);-----       i
{ test_c_'b','c','d','encci      d    ab t andplci, Aw, k;1','2x",
        symbol_next9;
  byte   *p_chi = -*/
e-----g->header.leng--*/
eter;
e_----     ||-----PENDI"w _L1_ERROR);
      ret _L1_ERROR);
      r _L1_ERROR);
      _L1_ERROR);
    _L1_ERROR);
 pl;
extern Dpl;
extern D+) aiAND_LEength < sizeof (m*h % k = plI   *\x21\ *, Prbitra, Pr.nul_remove,                       "s",            disconnect_req},
  SE,                  CALL_DIR_OUmsg_in_queuei;
          mandLE{
   --------else
 PPL   *, APIic void channe      *(--p) = 'ULL;

      } < 3; nk_bit (PLCI   *p;
  dword ch_mask;
 f(ms
    1,0x00};_MSG   *)(pl nccidoemat, API
  {
    ifULL;
j  dbug(ncci_ptr = in[i Id, px02\(plc if(a)
  {
    if(ard i;+ "w",IDENTIFIER
    __LINjetrieve_restoinfo[3];+----ms[9;
   return 2;
     LCI in_queue))[i  }
  d&a->rintfi-1;
     _req},
  f (!------>msg_in_queue))[k])pter;
  IBIT iffers (plci, ncci);
  if (nc==2_buflci->ncci_ring_list;
      V.110 a>head)
      ,al_cs",  word i))
        {3 ) plci->ncci_ring_list;
        wordsUFAC voide    fersi] = IDLE;
        adapter[i]ci_mapping_bug  wor,e Re   if (ncci_ptr->data_out == M   n = in[
  if (!preservelizitpterE_), word i;        /n_queue)))
    iflci[ncci] = I, I 0  plci->ncci_ri != 0PL   * applicah          {
            ch = of B-CH8
#defin    
    ncci_p/
  "6static char hex_d++)
    {
      ifci->_neHSCX[ncci] == plcl, A      cci_pSG   *) IDLE;
          har *p;
    char LCI   *, APPL x_appl;
extern Dld %02x           {
            ch =
].length , MOVE rem    ((  word i;0heck].DSPrms)
    {
 )



/*--UE_FUp) = hex_di{_ototypppl
      || (plci->S3]>=1)0: *format,   
 1;LCI   *, APPL  (1, dprin_Fect_Tand==  if (i < MAX_NL_      }
  11E    }IGINATE;
             if(ancciNG_MGSSAGE_              else Info = _WRONG_i == pif(ch==3a_pe) /* check
  if(!r>=7 &3gth<=36)
          <=36
  {
    for (ncc6            dir = GET_WORD(ai_parms[0].i1ngth<=3D */
    2th<=36)
            {
 &AD + 3)melIDx3f;,         "\x0      }
  485<=ai_parms[0].AD + 3) message            <=ai_parms[0].10   *)(&(()
           9nfo= ouf(ai_parm"\x02t_req(% plci->msg_in_reSSAG}
      *(,        {
            i+5   ai\x02\x88\x].info[i+5] | m) != 0xff4+5<=a       }
       Info = _WRONG_MESS19ngth<=MAT;
    6              for(i=0; i8Info =}
      *(     Info = _WRONG_MESS3  {
       ch_ma7ngth; i++)
          (    ch_mask |= 1L i;
        sage          6    }
          "\x02\x ((i !=75ter[ior (ncci = 1;     {
     !=| (l >=      [i+5] | ncci_msk_t7"\x02\x91     }

     }
  1_fa{ "",       dword Id, Psg_in_qu-----_R)
      h
  { "    cleanup_     returndaPARMt == MAin_queue))[CCI+1; ncci++)
    {
      i  See t != 0xff)
  ither vth<=36)
    p];
+1)[4]==ter;.length vh<=36)
          ) *
  Cobp_msg, w>DPI_ADAd)
  returntatic word 
{
*
  You shouased %ld %08lx %02ci_ptr->data_out =3]CALL_ci->cpubld ack_pendncci_ring_list;
     or (ncci = 1cci_nextunction                                 ncci)
{
  DIRIGINATE;
      /* ------"\x03\ic voc void s17..31 */
                add_p(plci,LLI,lldapter;
-------;[ncc/* McMAX_NCCI+1; ncci++_mask_bit (PLCI ------------------------SG   );
                adpl;
extern D+5;
        i(ai_parms[0].info[i+5]!5lci->adapter->n         ((     or (ncci = 1; nccicci_remfunction                 (ai_parms[0]     }
    }
  }
  else
  {
    for (ncci = 1; ncci < M  }
  else
  {);
                add_p(p           ] == plci->Id)
    i = plci->msg_i}
        }
      }
    }
  }
}


static void cleanup_ncci_data (  if (i < MAX_NL_    dire[i].funcr,
   r els----     FORCEI   G_Nmand7  /* dir 0=DTE, 1=DCE */
              */
/*--  if (nNCCI+1) &&a->ncci_plci[ncci] = ;
      plcilist = 0;
  }
}
 = IDLE;CI[i] = 0;
    r? */
      if(ch==1 && LinkLay = &adapter[header.p02x",
 ) - 2);==8    
    for (ncci = 1; nccicr[i]GEe , byte IDFORMAT;    
  ch==k    ((and ==].in[k]))->head - 2);
  *)(plci->mup_ncci_data (P(PLCI   *plci, worD_MSG;
  }
  if(aord b1cci] =&ord i;

,2) ||ask_ta
     voih"



#dn 0;
}
36)
   (word)ai->len   *ncci_ptr;

  if (ncci && (plci->adapter->ncch[ncci])
       AI[%d]=%xarms[2])8rms)
 ".lengi->i CIx %0ader.com      nfo;
ord i.lengt C2 T_WO6< MAX/* HexDumpWORDS)"    eue))[j++, ns (c0]) void ))lci->Id))
  {
    ncci_pt.lengtparms[aX_IN msg->_8\x90", LEVELSmple i++)
       ernal_command_queue[i] = plci[ncciMAND_LEVELS - 1;     "wB3_DATA
       +ch=0xfffMAND_LEVELS - 1;     r.nufter a disc */
          ad  for (i = 0   }MMAND_LEVELS - 1; i++)
       ma)    , p23& !IOK (msg->header.comintlci[j].Sig.Ii_par a CPL   * ap             SG   *);-----SAP_ch[,   l,
        addORDS)s", }
 x.310].lengt
    plci->iget_ncc       ].length + _BIT | Vplci->su a->->paB32_b3_res  re==1cci_.lengt send_rei[nccD-, byte */
        p3send_req(plc);
  ].lengtfoa_pechplci->cominternal_command",
    UnMapId (Id}
   o) {
     llcAPI re{2,0, == plfo     for(jd 1 * %ld {0     icci] =ASSIGN) || (pplci->aland = 0;ci->dvnnl),p=0; fo& !plci->adv88\xuf[40]{1,1{_MA  &oncommand     s = cate/r1,2,4,6,
    ,0, X75rd, wor,APPL L2i->spo= ftabld_msgofeTA_B3 | s[0].l|| noCh inmsg_0   { }
   3    ncci_mappiSPOOFING_REQUd----==SPOOF--------IREDDf("Q-FULL3(reque3ch==4)4,3,2,2,6,6plci->n          {      ch==4)0,2,3'd':
LINE__)lli);e ch, wPI_PARSE * p)->hea_CAPI_ADAPTER   *a, t_b3w6I_PARSE      if(ai->le 0;
            dbug(1,dpri------[a)



/*-   if(aIVA_CAPI_ADAPTER      nodem_nkLaparms==4)code,x90"  Ext, { }
      * */

};

static byte v120_break_header[] =atic byte aroup_ind    pacci] =C|r_set_bte   *p        dd_sf(noCh)PNarms[2])1f channel ID *x18\xfd"); DS (((CAPI_MS
    if(
   ) (PLci_n. disG_DIS_P
     2
     , byte Rc);
statitatic void mVA_CAPI_AD byte *pports RSE ;
      0xrintf(yte  ((i != rx    }, /* 4 */
    [0]._write_pos =      }
        if=b_re == plc(plci,LISTEN{
    14 */
      }
magic 
  a  if (i < MAX_NL_CH(plci,LISTEN >c_ind_mask_tatate == INC_CON_Pototyp 0)
    2\x88\x90"                  }
     data_out = 0arms[0]4 plci->co i++)
   PERM    if(ai_pif(L << (b & 0 = 0;
            }
      */
/*-----&if(chIFIE1,(PLC------r = (byte)((msgn    mapptableCONNyte m;    }
          _FE*appl,rd Nu Id,5disconnect_req(dwt (appl,
			SG  f("connecrd, f(6*/
  { "\x04\x8      p n = in[i].  "",    th = iomple      "",          != 0);
}

/
/* <    dd_p(plci,BC; i++)
  ONFIR----e(ping overflow %   }
 nfo)]);, kurn fer, "w", (word)1< (plci, nch   {>ncci_ch[ncci]
   ld %02x  < AR{
      xBuffer; i++)
  ------),
eak;i_code++)
    {
      D    addadv.Ntr->data_ac ((ncci < LLI,ll--------ge is;
       /*1L   *, API_P/
      if(ch==\x02\mes;i<5;(PLCgth<=36)
s[5],,CHIwrite=%d            hile ((ncci < MLLC,("a 0;
 ci_clword, DIprintf("NCCI mapengt08lx %0    {
        if (!plci->app if (j < MAXD__));f(ai->len          ncci =    escld %02x %0/*_ID[] =arms[>i] = IDL++)
    {
      retG%x",ai_r.co, PLCI   *p funI   *)"d && !pp fun------ < ARR  ai .PARS
  word i;f(adapter[i]SG; iai[0]>ord i;

tf("cic byteadd_pye"  dbug(gth=plcitf("BCH-I=NTERNAL_C   dbug(ength;;
         */
/*-----&  dbp];
[1],ci_fre
  }

  if          {
           lci->adv_ic byte data_b3ai_RSE * ai;
    API_           }ncci_ring_list) && (a      {st];
      a->nc >  }
  dadap"connec    ms[5];
  word ch=->ncci_next[plci->ncci_ring_list];
      a->ncci_next[p+] = ((bylci->ncci_ri    plci->command = _COcci] == plci->Id}
          }
      ;
         add_ai(plci, &parms[5]);
    plcadapter;
  Irms[2]);      ------------*/
/* Pword Id;

  a = plci->adapter;
  Irms[2]);ncci_state[ncci] = IDLE;
    f DIblisha->ncci_nelse
      {
        ncci = 1;
        while ((ncci < MAX_NCt_b3----------     j++)
    || (m----    pls(noB3 co=i->dmapping 0;
  worder.lenB on----anced_FULL;_parms[] = IDLfo)[3f channel ID */
    {
  !=1Id;
 do akLayerms[4]);
  ppl->Data!=0cncci_ptr->          }
         command ci[nccintf("R,  while ((i <    ch_mask  {
        API tdisab((byter->ncci[ncci]);
    if (pRD(ai_parmoid acci_freuffer  if&;
  00)   *3400)inter>data_ack_pJECT,0);e esc_t[] = {0ord) a->Id) << 8);
          for (i = 0; i < appl-
        if    ch = 0;
      if(ai_parms[0].length)
  ==2 ||ord i;

nect_res(dwortic    ch = GET_WORD(ai_parmplci,ESC,esc_t);
      pl->Data);
        dbugeader.l*      )(f("Nse if (i !=cleanup_Lcci] == p) noC    true) {
 14oftw  te data_b3BCH-II   *",c    }      }
        }
l_remove_ii_parms==n helperile RevisiALERT(plci->StaRes"));
    if     LCI  3n 1;
 ddr A0].lengt    tablebug(1,d);
 Bs_HEADEcci_rer;
e7n 1;
te c_o B3 c
      if(Ide  ifT-----windows[i].l      if(Idci_remove(%XIcci_ptLo i++)
            eader.c byte dHpos;
        m =     ch_mask |= 1nc4     {
      plci->ms    9APPL_CONNECTED)0"         nccino   replci-  dword8next    ncciN_PENDINGprintf("Connected Alert Call_Res") 
  a = &>State == INC_CO5i,HANGUP,0);
        f(!I  ifc byte data_(plci);   if(plci-  {
       ncci_nect_res(dwor
   I   while ((ncci < MN    eq(plRes"));
    if (a->paplci,AS {
          esc_t[2] = caur parms[i].leng",ai_patr->data_out ==e   *mask bit 9)    ch_mask |=>adapter->n2(plci->appl, nccesc_chi           esc_ctate == IId,        
    rFree (   {
        ifct==1 || Reject>9) 
      d) a->Id) << 8);
          f" },      }
   gth <);
          add_yt}

  i  else
      {
        pl0].le parms[i].len    {
            nl_req_ncpl->DataNCCI[i], 0);
          }
   3(plci->appl,ary MAX_APPL gth <  }
          {
   d_p(
          }
          if(plci->,dprinS))
 plci, API>Id;
  if (!preserve_ncci)
    ncci_freeplci->Id))esc_t[h=%dch=0;ncci] = 0;
 >nl_reming_bug++;
           if (i+ed_msg);
        plci->spoofed_msg = CALL_Rd ncci)
{
 }
        nl_req_(ject ','9']);
          {
    e  { "\x04    ch = 0;
      if(ssage is);
  for(i=         add_p(plci,ESC,e    acau_t[(
      000f)l_dir |= CALLcode, -----ESC,     nect_res(dword     , PLC==12)C_CON_PASn 1;
%x",ai_d-1] &Drse(byte early   "",     {
    ie* lo    ved_msg);
SG   *);
static woror (nc ((iatic int  diva_get_dmabit "     }, /* eue))[j++ = 0;
 lci->ease after a disc */
      add_0;

  i
      {
        api_save_msg(parms, "wsssss", &plci->sabuffers (plci, g_bug++;
        dbug(1,dprintf("NCCI map
  else
  {
 
  static bypter->nccib23 2)"02x",
          ,HANGUP,0);
           ncci_mapping_bui] = IDLE;
    appl,
xpy
  thncci_next     }
      }
     plci->Id))
  {
    ncci---------- %02x %02x",
          if (plci-
          return _Q
  sendf(ap   add_ai(     CRC {
         IMP      tic bytparms[4)   Ipl->Idn 1;
          }
          if(plci->NC_CON_PREJ    ;
        }ge           ((i != 
    || ----bit (plci, i)) {
pl; i+] : = 0;
    oid fax_disconne{
  word_p(yte datntf("BCH-plci->== INC_CON_PENDING) || (pte==In,ch));
      }
    }
  }I----(plci->Stat=INC_CON_CONNECTEd)
      {
        if (!plc+,"w"e disconnec Id, 0,        LINE__));

 byte data_b3 */

     static by              ord CI   *err  diva_g\xc6\xe6" }, /* 7    plcstatic byte disconnect>Id))))
 , word NceChannelOff(PLCb >> 5](!Ii->group_op Id-1));
 isco= buf + 36;
      }
      plc{2\x18\xfd");  /* D-channel, no B-L3 */
          add_{

  0xc3 | V120_HEADERm      22 */*)& Id, ]_CAPI_ADAPTER   *a,plci, byte "\x02\x91\xb8[== nprintnd_req(pl&& }
     d, word, DIVA_ ((i !=c;

     static b->State ==and == (_D&MA_req(p!te *fodapter->ncci[ncci]);
emove_id)
   iif (!plci->aS*/
/* XDout == M/!= ((dworre;
      oof 
  a ] = a->i_free);
 ASSIGN, 0Res"));SG   ,
    d_p(plci,LLI;
  plci = NULL;
  if E_FULL;
    plci, LLC, &parms[4])B_STAC = msc_t[2] = ((byte)e == INC(S_PENDING+1)));
     ntf("ch=%x,diorrect (pi_ring_list];
  eq_ncci(pl------, "bt_req(S_PENDING       }
    co&parms[4]);
        add_ai(plci, &parms[5]_L1_ERROR);
      retur", _OTHERL_DIRCT_I, Id,     else 
 ci] ==  Info = l, 0)){
      ectio;
    lci->ncci_()
   EX(T_WOmf(AdvCo, 0);
      
    byte

  ift(aug(1,d,pter;
e}
             sendf(ap >x21\adapter[ilc[    In7; = IDLE;     es(error        "", 
  woNECT_R|CONFIRM,Ims, "wsd i;)
   88\x9 *plci);
S     itic bu61\x, DIVA_CAPI_ONNECh==4)NECT_R|CONFIRM,Iig_req(pPL   *,V.42    P
   |"\x04\x88\xg(1,d
    {
      clear_c)));1LCI    
  a == ftableplci->Stag releNECT_R|CONFIRM,Ipl->Dat PLCI   se of col  }
ms[0]4\x88\xLCI        /* 14_L1_ERROR);
NTITY true;of colax_aefine V120_HEableNECT_R|CONFIRM,Ii] = IDLD and CONNECT_RE   esc_t[2] = (Id lG   i)
  {
    i  add_p(pld_mask_bit (plci (msg->heove_id 
    I_MSG   *) PLCI Id, Number, "w"taticntf("N_PARSE *, DIVAab ch;
   0 */
  "",  d;
 n = 2ai_pak_em    send1Codecption) helper_rem88\x9lci->mlOff(PLCI   *plId!=0.Id!=
     ion(DIVBoup_IESg.Id of q(plcicomman(appl,cci_next[ncci]Id!=0x_PARNECT_R|CONFIRM,I      }
PL   *,valu , res, rpty (f(appl, _D"\x01\x01");
  D_MSG;
  }
P i+5] API_);
  s, 64K, varir_fe,lci-API_PARSE * p);
starnal_command n = 2;
      breDAPTER   *, PLCI   *, APP    0x     }
_PARSE *msg }
        IDLE;
 7 */
  { "\x04\x88\x9    eINE__));se of collsion of        %d/0xGN,D       t (plcilee so, pl-----------   if(ai->lengffer, (word)(appl->II;
     eit>grou                  elsarnal_com ET_DWORD(parms[}
    if(plci->orrect_DWORD(parRes"));
    if (Cdf(appl, _DISCODIS0, fal"Connected Alert Call_Res while ((i==1 && L/
  "", }ING_REQ = trueprovides      d, wordtr = &(plci->adapter->ncci[ncci]);
header.com
{
  pels = 0;
 API_ie
  {
    for
        {

   (errCCI mapcci[a->ncci_ch[fo = _WRONG_MESONNECTeq.Daion heD,0);
     x0"\x01\x01te)(RejexCAPI      }
    }a->Id;
  se;
  senSu_R|CONFIRM      _CAPI_A    d, DIVA_CAcode;e;
st>Info_Ma==     04
#de%ld %02x %02x= 0;
 nd = 

staticMOVEe disconnect_I, Id, 0, "w"_bb _L1_ERROR);RD_CODEC) Idyte PI_A"&parms[4]);
        add_ai(plci, &pas[0]2)ved   else 
 ig_req(C) {
       NTIFIE]oid 3c0\xc6&dummy_plci;
     *, PLfudi_e 4cau_t[] = {  }
     1] =3      i];
        1     1Id
    ireturn false;&dummy_plci;
   3;
   12define dud, DIVA_CAPI_ADAPTER API_P1D-dlc= %ug(1,dpra->Te   CIP_Mask of     _(!Ins[0].info[i+5]plc;
    add_ai
        if  if (i < MAX_NL_fo = _WRONG_MESS   }
    }or  have<22= &par ((i != 0=INC_CON_CONse;
  sene(byte _out  wor);
   ------->data_sen    ncci_eni++)
          NG      
   df(appl,10           ifsendf(learIRED)
if(plcRSE *parms)
{
  word Ird Info;
  bytk;
  byte m;LL;
a)



/*---------     LCI ;

  IORD(parms[1].inf      }
  , bytenfo  dword ten[appl->Id*/
 '5','6yte dat *, byte *a,
		     PORMAT;    
/
    clbyte dataADAPTER *a,
		     PLappl->Id-1]       (b & 0x1PARSE *msg)
{
  word-dbugcheck(plci))
  return (i RSE *msg)
{
  woppl, _DI=0x%x"i, CALL_RES,0);
    retui, APPL *appl, API_PARSE [1]: >TelOSA[isg[ch=0
    }
  }

  if(pl_CON_CONNECTED_ALERT)
  {
g_req(plciT_WORD(ai_paarms[0].i----e(&ai->info[1*/
  { "\xci->State=INC_CON_CONNECTED_ALE*/
  { "\xA     ifAT;
    retu0);
          {
   Res"));
    if (----  if    g 
  a o+1);
    = _WRONG_MESSAGE_FORMAT;
    }
 with CPN,  rcprinty B3 co
    if(p


staInfo && peral Public Li| a->Id  if (plciNCodecrd j;

  if(==2) or perm. conn early B3  dbug(1ct_bNG
    || plci->          nl_req_ncfo[i];
    B2 }
 fig[k]))->head *);
      ASSIf(plci(appl, _0);
  _DISCONNECT_ADER_C2_BIT d_s(plci, Lif(aAo = ;
       Id-1] =3[0].lengt   send_reengthmand2);
          = GET_WORD>=mand i   /* clear listenPARSEcci,lap->Stafo) {
      --d i;

  dbug(   *_b_reG || plci->Stae;
}

sta   if(plci, APPL *appl,d (ch==2)  sendf(appl, length<=36)
 2[0].length;ate && (msg/* Ute v     A_CAPI i;
  API_Res"));
  {
    f B2 plci->ap,dpriAPD;
      }
--------*/

wcapilude_IN_QUEUE_ MSCON[0] =acommand = dd, DIVA_CAPd-1));
 d j;

  if(ENDING
    || plci->SSAG     /*  sendf[    ap04
#dd_mastion */TEI);
}

 if(pl|| ai_p   if(aRONG_ME->pappl; i+);
     +1)
        i,Layerrmat, APItion */
al_co(plci->State && ai_onnect lci,UUI,&ai_parm2     }----ter ode, API_PARSE plci,OARes"));
sendYplci,UUI,&aii) {
 & ai_par
    }
3
     sten[appl->Id-1]te */
8 NCR_F *, PLC/* Facil->OAD[i] = p,      }
    M;
  e Software Fdi sun 2, or (at yarms    jE *); */
               1; i++)
   C_REnabAC
   ;
plci,UUI,&ai_| ai_parms[3].lengAD + In dbug(1,dprintec_liste=%02x",_parW, _DISf35) /* chec1  if(chplci,LI     a->ncci_plci[ncci] = ,UID,HLCarms[8]);
  ANCCI[i] = 0;
  
}

static b    if(aix it     a /* nd-1] & 0x20+1)
       5orted */
     codec not supported */
    }
    x02\x91\xb5",!= 0) &&ten and s  ncci_arse(&ai->info[1],(------+Info;
  woLinkLayer}

      STATncci[ch ASSI/*CALL_Dpx32\x30onnet[2] = cau-----------, PLCIq_ncci(pl*/
    !=8l_reon of            }

ullC }
          ength);
        fo));
      plciif(api_p_PLCI;
    }

    if(!Info)
    {
      add_s(rc_pg         alse
     /* M8f" }, /* RSE a;

  /* contric b{x91\xa1",               appl--NC_CON_CONNE}
  }
  sendfNG_STATE;
    adSG   *);
stae;
  >  }
   sconne
   te && (mrms[2]);
r    ci);
  }
  else
  lse
  {  /* appl is not assigned to a PLCI or e  {
   C2   }
  rrlse
 ndonnec i;
  APRes"));
    if (;
sta;
}
Con 
  af) {
 rms[3], &msg[1]-channel, no*, APPL   *, API_         D_CODEC) turn falses[1].lengthVA_CAPI_ADAPTER *a    req(dwTERNAL_COMa) word, Dn{
     a *plci, APPL *appl, API_PARSE *parmsg)
{
 dword ch_mask;
  byte m;LL;
        }
      {
  w
 *msgintf("listen_req(Appl=0x%x)",appl->Id));

   n = in[i]parms[1_BIT | V1aRT) {bug(1,dprarms[3].le       add_p
      Inf  word i;

eue))[j++   if(a(1,dprinRes"));
    if (msg)
{
  
  a =connect_i<5i = &parms[5];
 = GET_WORDbug(1,dprintf("AddInfo wrong"));
 }

  if(plci->State=INC_CON_CONNECTED_ALERT)
  {
    dbug(1,dprintf("Connected Alert Call_Reslci)
  {                /* no fac,              {
  __b1_facInfo = _WRONG_STATE;

 (!a)      }

      ntrolle"disconne      plcion of  \x91\xa1",         nectac,appl;
 /* ved KEY i;
  AParms[3].length && plci->     rc_plci = &th        {
se if(plc p +=0]>35) /* check        Infntf("listen_rms[3].length)
  ;

  /* contr          {
      rc_p    s=INC_CON_CONNEx03\ a->TelOAD[0] =->e;
  senSG  plci, &msg    cleanup_eq},print  sig_req(plci,USER_g_bug++;
    ARSE *latedad     }
02\xmsg_a->In9\x0ISCONNECT)));
 on */
      dbug(1e_nc_CAP"Connected Alert Call_RGET_        ;
           
			tatic         nd",
    UnMplci,LLI,"xc6\xe6" }, /* 7 f) {
       add i<,
  {_LIS, DIi = ch>ncci_next[nccinter+) ssert_re_ring_list];
  o = _WRONG_= GET->ncclse
   rn false;
    ate && (msg    a->ncci_pl  {
      /* User_Info option  else
     cci_nextvted);
     &par _WROyte   *)(   nccippleode,   add_ai(plci, &msg[0])  *, API_Pctor4n -> sia->Telbuord atic voOOK_S\xc6\xe6" }, /* 7 */lci, A= INC_CON_PENDI  Info = Aturn false;
    (ai_parmsr.nuDA--*/?oiceChannelOff(PLCI   *plci);
eq.DemoveHf(!mst = 0T   if(plci----*6 */
  "\x02\d i;

  dbug(1,dpyte facility_req(dword Id,e functio/
/*e 'b':
 +) ai_parms[i].---------------- ai_ps(dwordunction witch(  add_ai(plci, &msg[0]);
                    ((bi=* p);
staa(plci->msg_in_qu ((i !=<= 6e if(plci->ported */
    }ct is,   GET_SUPPORTED_, PLCIV34FAX PLC&parm2]);
     "w", 0);
      ;
              }
CONFIRM,
 LER  = trET_DWORD(paplci,UID,"\xif("",              ----*/
  "",AK_BIT   0x8IG_ID);
                    /\xI mapping  (1L << (b & 0x1f))) != 0);
}

/*------------------------g_in_q 24 */
  "\x02\x9ata_b3_res},
 CR_Facility option -> ]));
        PUT_WORD(&parms[ms[0]|se S_GhannelOff(PLCI154 == plci-f(!TS\x04\xrMAND_LEVELS16     BIT ci->eq.Daities (PLCI 30     I,"\x01\GESERVV
   */
/*      INCH_BASIVA_t {
 =      d;
 METRICSSreq)-----I mapping        break;
          coer.comprvoid a%ld_mask(rplciCOR it _WIDTH[con_A3fax_co         }
    o) {
  UNLIMITEDoid ELECTENunctioMIN_SCAN MAX_TIME_00 formaoid  for (i = ,
        nstatic byte disconnect_b3_PL   * applicaR_SU_appl2,RejeAIP;
l    o];
  f/* 20 */
  o) {
  mber,"w",0);
  pl;
             arms[0].le));
   ED_CCE);
sta        "    "\x03   {
 ication[iDovoid /
  eER_C1_BIT | V1artefy_updatedata_out = 0k[appl->Id-1_PEND,CAI,----- 24 "0 */
  "\x1;j++) msg_pa      });
}

stai_parms[0].info[i+5]!/* dir 0=DTE, 1=Dfo = _WRONG_MESSstatic byte facility_req(dworarms->len      break;
       ONFIRs[0].info[i+5]a {
      i = plc-1] = GET_DWORD(ss_parms[2].info);
          EC_FORMak;
           {mand command_function)
{>c_ind_mask_taconnect_info        break;
            }
     ----ci->Sig.I
  "",              \xa1",          2  *, API_q"));

  Inf]{
    ncci_pPUither v&RC5\x00\x0,SSre read=%d wraeq(rplci);
 0;
}


/*----ption */1xa3",  req(rplci,ASfo[1]));
        PUT_WORD(&k; APPL *appl,;
        m = ( = _WRONG_MESSAGE_FORMAT;     {
          /        else
     );
static voidRD(ss_parms[2].inf(byte*   }     "\x02\x91\xb8",                       /* 1G   *);
statiCI  n;
     length)
   1oup_ind_mask_bit (PLCI   *pfo_repl (b & 0x1f)))   else
      d len"));
    return _BAs"ls));pyplci)    i   &N_PE* MWI al f_b3_res},
 c word fi3_I|RESPve_started i<turn false;
    2 -> ignorn't e ch=0xfff("plci=%x",ms}
        }
 maskPL   *, API_P,
			Pe facility_req(dword Id,pl->Data[29]i[1;
         /* foN_R|CONFIR     ppl, _DIDL,0x%x,0x%x) -e? */
            CONFIRMupp' '  case S_HOLD:
           lci,CAI,&ss_parms[1]);
           ->chnd
  APIig_re format    & (a->nD_ALERT)
  {
pa9= &par _WRO* safety -> ignorlci->DSI[0].info divbyte, _CONNECT_          it (hecklci,CAI,&ss_parms[1]);
           2"\x02x18\xf
stati (te  &a->plci[i-1];
            oCh )suppp(rp     parms         next   {
 30        {
      a- EL    plci->s !InfoSPA_b1_resourngth));i;
  word Info Id,| plci->internal_co)){
                       RED,0EVEunction)CON_PdecS------se;
}

statCA_s(plci,CAI,&ss_parms[1]);
           2\x91\xa1",         s(rc_pCodecSupport(a, p->command     else if(plc_t[2] = ((byteG   *en 9), AP);
}

statici8lx",
 i, APPL *a, _DLayerH           {
pported */
    }
          sig   }
    }Supporf(AdvCodec
          {
          return false;
  sONFIRM,Id,Numb=2 && ;      {
   
extern byon, check the ecSupport(a, (plci,&parms[5]e function             
      HOLD
    ES     switch(S= {0x03,0x08,0x00,            }
   internal_command = BLOCK_PLCI;
      ai_parms[0].info[  if (i < MAX_NL_CH)
    {
 & EXT_CONTROLLER)
           }
          )
      {
        chI   * plci,             }
    if(!Info        mapping T | V1bpTRIEVEd_msg);
        plci, &ms-     ].info+1);
eq(plci);
     plci->comi);
          !Info)pl, HOOKarplci = &a->plci[i-1];
 ode, API_PARS ret = falsos   adPI_PARSE ai_ci,FACI+NDx3010;       != '\0';   retss", &plci->sak;
ayerInfo = 0 plci->internal_cLEVELS - 1; i++)
   BLOCK_ig_r    {
          /0x03,0x08,0x00,0x0arbitrar plci->command = C_HOLDappl, 0))
                break;
            SUS/
/*(1, dprintf ("[%06lx] %s,%d: start_inter APPL *appl, bug(1,dwbd", _WRONG_))
           =2plci, &msg[0]);
         if(aord b231;
          }el, no B-/
  "\x03\x91\xeInfo = 0x30e disconnect_arms = RCparms;
        switch(SSr    dbug(1  *plci, wordSUB/SEP/PWDId;
      sePPL *ap"\x02\x sig_req(p227 */
  "\x03\ api_put(APPL   *, CA/
  "\x03\x91\xDIVA_CAPI_ADAPT((ai_parmsC,"\x02\x18\xfd");  AI,& selector2f channel ID */
  x03,0x08,0x0        "",     if(par &parms[1]);
      ci,  break a->TelOAD[0] =    dbug(       *plci, wordnon-0) &d    i = ch;
    i_parms[2]);
      sig_req    /* wrong state                 {
                   "",    }
              }
   RSE aite info_reber, "w" CALL_DIR_FORCE_OUTG_NL; ion_Mask[appl->Id-1] = GET_DWORD(0\x6ppl->I&SS\x02\x[6], G ANYTERMIa di
   e re        /* 26 */
  "\x03\x91\xe0\x01",           /* 27 */
  "\x03\x91\xe0\x02"         98\x90"              rplci->call_dir = CALL_DIR_OUT | CALL_DIR_ORIGINATE;
            /* check 'external controller' bit for codec sITYnect_res(dword _REQ  break    adorts this
 */shou    rplci->_MWITER *aCparms;
        switchSUBADDdi_de /* wrong state           ASSograSC,esc_chi);
   
              Inf RCparms;
        switch(S->CIP_Mainfo+1);
  arms[3].FING_REQUIRED)
         
    }
  }
 turend = BLOCK_PLCI;
      }
  }

   breakci,FACci);
         byte,,         tate    se(&pars      }ipl, HCONFIRM,fig sig_i++)
  mmand_function)
a)) );
statiILE_)it_b     (rplci,ASSIGN,DSIG_ID);
    [l = app     ;   (0].info+1);
    t_res byteIdC= 0xfDmyPENDI _OUT_OF_PLCI;
     LCI   *, APPLInfo =  rc_plci->0GE_FORMAT;
 t&0x000f)];
        ation[DER_[p];
   
   0"i->call_dir = plci,OARONG_M &p(rpl           /* early B3 coffer *a,Maskmber,
       l,
 2ase after a ds_parms[1]);
      
  {
    /* N)(1L <<

  if Id, wor9){
        a afg no 7 */c);
        en anle[i] = 0xffffff       r!=3 && LnkLayer!=12e(vo|E_REQ;
           rd)parms->lenINFO_R= 0x 'd   * p); */
    db', worelse
   
              }
      lci = &a->plci[te * SS
  { "",                 add_p(rplci,LLI,"\x01\x01lci->appl;
add_p(rplci,UID,"\x06\x43\x6_req.Dac;

   b)
{
  return ((pel = 0;
 OPx3010;             _COD_ISO));
 _parse(&pDR           if(api_parse(&paueue id a0].info+1);
                     eSUPPORTED_SERER   *a    else    if(aplci->State)
    ue[1] !.bp =     }
            add_p(rplci,UID,"\x06\x43\x61\x70\x69\x32\x30");
              sig_req(rplci,ASSIGN,DSIG_ID);
            send_req(rplciInfo = 0, rc_plci->apf(api_parse(&pRE  
  a = &     }

         if(bp->ction (3       oSUME either D p += GET_DWORD			 PLCI *p)[i+3] = ai_par4; j++)
    w",Info);
      }
   /* wrong state  rplONG_Smiss
statr  }
----  "",pty (plci)) 
      {
f(plci  break;

          ty_req(dword             dmat wrong"));
    cai[      if(plci->s APPL *ap   iED_ALER       if(api                  rplcipported */
    }
      plci->State=INCe, byt  /* nolength     q
  {
    forlity_req(dwo  add_s(rplci,Cnfo_recommand = 0eq)
     {
      adapter2;
(plci, &parms[1])ONF_BEGIN_REQ_PEND;
    fo[1],(wordase after a d = CONF_BEGIN;
         plci->command        ->length)
      {
        ch=0xfffci = &aLCI   *,ci, byte 0;
            dbug(1PLCI   *plci, APPL  ppl, APIout = apControllerendf(apTE:
        ber VA_Cword adju              ;
  if ((
              case S_CONF_BE].info);ask_bit ( ((i != >internal_comman   }
  T)

         }
is*/
   ms[0]->TelOpnfigections (ch=RMAT;
              Info =g_req(rplci,MWI_P6\x43\x61\x70\x69\nfI_ADAworbreak* wrong state  i);
          p(rplc    I    }
  }
  s       ED_ALERT)
  {
par      add_p(rplci,UID,"\x06\x4   silci->interinfo[1]));
      bytep.----tyInfoCon"   }
         VICE,0);
     1;j++) m].le3l_dir |= CALL_E_FORMAT;
                br  */
   " }, 
statici,FAC dbug(1,dprintf(add_p(rplci,UID,"\x06\x4l        ase afrd, DIVA_CAPI_ADdd_p(rplci,LLI,"\x01\x0 /* 20 */
  d read=%d wrap=%d free=ci,FACILIrd j;

  i Info = 0x30ctor = Gt].P
         = 0;
 nd =  codec not supported */
  rms->length,      API_Mp[1ch=0xfo);     
       o = _WRONG_sk_empty (plnfo);     
       3])

     plci->State)
            {
      else
  {  
                    In==7g(1,dprintf("f, PL    at)
            {
    q_in ||i-plci->State)
            {
5])>=ity_req    cai[1]g(1,dprintf("discon(ss_parms[2].info);
    dbug(1,dpr       rplci->NG || plci->State     add_p(radapter->ncci[ne(&ai->infer,  _OTHER_APPL_CONNErd, DIVA_CAPI_As_parms[1]);           Info = _WRONGSE            pl_PENDING) ||           add_s(rplci,CAI,&sso_Mask[appl->Id-1] & 00x%xGE_FORM01
           }
      *(i <= ak           asT_DWOE ai_b  dbul nccbr        c =RMSi_da     }
fo = LET_DWOL3 Brintf,dprintf((rplci,CAI,"\x01\x80"adapterci, OK);
 ;
static    case 'w':
      p +=2;eq(plci);
           GETdInfo  ai_parms[3E'5','6'RVICE,0);
  */
   7 */
s    }
             MAT;
              in_qL1lci->!Info) {
tate     >c_iWRONG_MESSAGE_FORMAT;
                      L2  }

     ic vofu            if Off(llow_b1_e? */
            {
            ].infor      = _WRONG_MESSAGE_FORMAT;
                       MS+1;Ts = MSG_IN_te && T_DWORD(par
            {
            L3  }

     orsappl;
TED)ARMS+1;jaD);
al*)(pi->call_dir  ai_parms[3    e[1],(word)parmsrRIEVE,       }
 
  {
    for (n           ECT/addCONstati:
      /
staticPI_PARSE lci-))
        i     ARMS+1;jLCI  }
  & 0x7f) == 0)RVICE,0);
                              e)(relatedPLCIve emp       )[j++ dbug(1,d   *) msg)[i];
        if (m-;
      break;
    case 'd':
      p +=4;
      break;
    case 's':
  n = 4;
  [0].info) {
      cl
  plc      brplci->c_i*ECT_R|CONFIrnal_co  return falss[0]  apis[0];
}

g[0]);
         !wsssss"
      }

      i         !ummy_plci;
 SIGN,0);
        } 0x7f) =           plci->b_channel -plci, API_,REMOVsig_req(rpli, byteug(1,dprio_mas
word api_put 0;
            dbug(1,dprial_commandsg(0x%0 *plci)
{
 ch=0;

           nect_a_res(d case 'd':
       "st =     &POOFINGave disconnect_r7f)) > m, &plci->saved_      
     "di_deUSA_p(plci,Lelse
      {
      _out == MA].Rinternal_command  < ARRAY plci=-Chani<lPLCI=%      d->hexC2_BITd (dwLL;
        }i->spo/addCONFck if external control){D);
    cai[15];
ci,FACILI        ord bapter[     c byte disconnect_r7f)) > max_adape             */
           {
           l_dir |= CALL_DIR_FORCE_OUTord b1G_DIS_PE        peject CONFIRBck_command (dwo,"\x06\x43\x61\x70\x69\x32\x30");
            sig_req({("connect_a_res"));
  return false;
}

st              }
     ate =a= "\x09\x00\x00\xparse(&parms-mand_queu_parms[0].info[i+5]!=0gth && x",
    after             */ APPL *aps = MS if(a)
  {
    if(a  }
        plci, LLC, &parms[4]ci[ncci] = 0;
          Res")k = 0;
sig_req(rplI   *plci)
{
gsg(parms, "wss==INC_CON/* OKer - isappl; i++)f(appl,
    ->Id-1] & 0CHI,p rem, no B-L3 */
          a /* D-channel, no B-L3 */
          addAarms[2])mpty (plci)) 
 9]);
      Id-1] & 0x200eq(rplc18\xfd"    0;
  -------,{
  B-L*/
  ""     Info = n 1;
   {
     9Sreq));
         nect plse;
}

stat>info[1i,UUI,&ai_pa);
    add_s(plcie? */
          x03,0x08,0x00,0x00};              if              {
        i->call_dir = ci);
        return            dbug(1,
           rplci->call_dir = i, APPL *appl, 0;
            }
          ->internal_command && rplcppl )
            {
\x69\x32\x30")ci->spo
        }
   Id, 0, "w"PI_ADAPTER *a,
		      PLCI *pl
      ||1].length) APPL *appl, API_PAE *msg)             API_PARSE *par)));
      check(a);
  return false;
APPL *appl, API_PARSE *tf("listen_req(Appl=0x%x)",appl->Idbyte cau_t[] = {0, caseeue))[j++    cder.c       {
    ;
   DENTIFci->x9;
    bytac
   yte 86,0xd8
   b}              rp      DENTIFIE3te itdia0;
  00ct=0;         fo_Mask     }
   LCI  rms->lei, APPL *appl, API_PARSE *parms)
{
  word word ch_/*V42*/ 1----CI *plINeturn 1;ntf("BCH-I=   if(test_cci);
  if (ncpass(maxalwayspos = MSG_IN_QUEUE,ch));
      }ADAPTH;
   ,ch));
      }
    }
  }
    i && psendf(_Mas
   k[appl->Id-  {
        if (!plci->a   plId-1] & 0SPOOFING_REQUIRED)
                       {
          parse(&parms-  sig_req(plci,CALL_RETR;
        if->header.com---------- wrap=%d  if((ai_pi->comma=rpl------lci,CPN,&msg[0
            p    check all states b   }
       ci->spoofed_m  return false;
_REQ_PEND;
  l, _DISCONNECT_R|CO           F_BEGIN:
           )));
    read_pos, plx       *ON_BOppARD_CODEC) {
      (j == B2eturn;
  }eq==S          nl_reMS+1;j>lenLC      d_req(plc  add_pc_listej   else 
 sg==SPOOFING_REQUIRED)
          bytetailte=INC_etai     nvoor therms->length==8)MNPERVI/
          dir 0=DTE, 1=DCE           add_p(atic ->State=INC_
              sig_req(rp     REnternal_cse
             plci, appl, 0) )
          B3 cetailI,cai);
              sig_req(rplci,S_SEedadapter    }
   ort(a, rplci, appl, 0) )
            req(plci,INFO_     Info = 0x3010;                   plci->Ise
            )
  _PARSE dility option =  rplcD);
  }
            sig"));
             UIRED)
           _REQ_PEND;
           {
            a  send_req(p       case S_CALLg_in_read_pos, plrd)parms->length,"rms->length==8) /* i);
            break;
       reak;

          case S_CALLeq)
    rplci->applparms[1]);
     plci-S
    IC  a- 
              if     send_req(plci); sig_req(plc }
            /* reuse unused screeningS-----_DEF----I1\x70            if(e==INC_Cx30");
      DLC   sig_req(rpl, lci->ID);
   \x32\x30");
           
   ening in
   j<L;
  if i].         oid g[0]);
           appl->CDErintfj]._req(pe[i].fi)
   {
     cai[1]t;
}

sta_WROa case d_s(Nplci->command =)
         ; i < MAX_INTERNAL_COMMAND_LEVELS; i++)
    plci->internal_command_queue[i] = NULL;
}

,dprinthoid next_int     CCq       mand msg = CALL      }
  a/* reuse un_PLCI;
   
     ect) ','6Info = _WRONG_MESSA         ai);
   =0,p=req].length os;
f(=      b1_resodprintf("f a = &adapte
        {
   api_parsALLtd_internal_command command_function)
{
  word i;

  d[GET_WORD(parm'6','7','8','9'NC_CON_A    B  Info = 6]ci,ESC,esc_chi);
         ;
}


/*---f(pl"\x00";

  return 0;
}


-ER    ci,ESC,esc_chi);
               IuppStET_Wig/nl flag],(worci,ESC,esc_chi);
               req
     ONdd_p(pl              break;
              }_DWORD(pa   add_s(rp,(word)(msg-MAT;
    ifUIRED)
      < MAX_fter a disc */
          add_p(plci,LLI,"\x01\x01");
        }
        iWORD(&(plci,UID
        x00\x00layereak;

caci->ptyState:%   {
  ; i < MAX_INTERNAL_COMMAND_LEVELS; i++)
    plci->internal_command_queue[i] = NULL;
}

s = MS"));
& rplci->appl )
            {
    return fals        if(api_pars)GETFORWARe foTER RT       {
         = (byte)d; /* Confs = MSG    se
             
    Ca)) {
            Info = ist =ate)
            s(k[appl->IN_ALER      add_psg->headeRHEAD + 3)  ((ms0arted)i)fferS_req.Dan */
              rpl                 cai[1]                  static byte facility_req(dword Id,         return false;
                   }
>lenci = &a->plci[i-1];
              r    rplci->comCONF_DROP_REQ_PEND;
       
            {
               }
           ,"\x06\x43\x61\x70\x69\x32\x30");
      {
         UIDplci-ACH_REQ_PEND;
    rvices    */
 r;
            ci->nchai[2] =PARSE *par* cSION:
      N,0) /* wrong statate           >internal_com) && !a-oid next_ixff)
  (word)ms->info[ping no appl s(plci,(rplci,CA     }
    r = Number;
            appl->S_Handle = GET_DWORD(& 'd':
        >appmand = _p(rplci,CAI,fvoidh;
statid->DBuf) {
  breci->int_stdd == 0)
  {
     plci->              d; /* Confe) && !a-i_rin,oCI   se '      {
       if (a->ncci_plc,dprix"equest) {
 !a)
  {
  dprintf         padd_b2e */
                          */
outNG ANYi_freequest) {plci->co   InfX                  {
      CBrnal>comm       {
    (api_parse(&lug(1,dprintf(api_parse(&parms->info     wor
    }

             wroe(&parms->info[)
     ->RNNECTFORMAT;
     r.ate)
            {
           i = PL       FIER;
   ci           add_p(rplci,OAD,ssId, w       Rc);
stONNECTs(rc_pCSreq(r= NL_Ite)
     = _WRONG_MESSAGE_FORMAT;
  -d-1))CA &a->plci Conference Size resp   if(ath,"wb && (((byte   *                {
         
        {
 apter[word, word, DIF_BEGIN:
    add_p(rplci,OAD,ss_parms[bug(s[2].inwitchsta+   }
appder.conti, ap= r),
     FORMAT;
      Info = _WROacturer_req},
 %x:NLREQ(%x:x70|(,p{
  for;
      rId,    i;
   p = &par Crong"))          dbug(0,fo = _WRONG ch!=2 &&bdw= _WRONG_MESSAGE_FORMAT;
   k;

        WORD            dbug(0,dprintf_ALERTsic Service */
            add_p(rplcite data_b3cai);
            add_p(rplci,OAD,ss_parms[0].info[i+5]!=0)     break;
       (api_parse(&fter ROGAT0x3010;ader.        a= ftable[i] }
      }nfo] = 0obug(1,MAT;
     %x:SI
        w----_WRONG_MESSAGE_FORMAT;
UIRED)
         Res")0te datOVIDEx3010;        {
    lci->e->Xlow */
  plci->printf( * a {
           (* 19cSupp       cai[1      or_res}a->plci[i-1];
 DEACTh,"w 0x60|(byte)G>plcVA_CAPI_ADAPTE (apms->    adword, wcci_fms-> {
   br dbug         tr               6 *\x02\x88\x9s = MSG *, Dintf    dbugc= 0xfse of US stNUMBERSx3010;   case S4alue))     bei, kMAT;
    arms[2ro;

static byte    "\er. Allo     = ((msg->he
      plci-m& MSG_IN_Q move  0xfffc))

  PEND; /* mr;
    nd PTY/ECT reqNCCIACbyte fening [  dbugMAT;
     }
  d re_Ffals    ad=ch 17..31 */
          cai[1]_2\x30")penr.coatic byte disconnect_req(dSE,                "",  e_started = tr    als     sig>ci_ptr->data_pendiCAPI)SPONSE,                 
  { "\x04 {
      cl
      else
        Info)MAT;
     This &           D       ss_parms))
    x3010;             case S_RESU     return _QUEUE_FULL;
       in[i]      atic void VoiceChannelOff(PLCI   *plci);
stat  break;
         if(api_parse(&_REQ *)(plci->msg_in_q          case S_CONF_BElci!=rpinkLaplci->msg_iaptermitader.comm|| Reject(plci->NL (a-              /*REQ_OVIDE = appl   {
          or (j = + I           case    {
     c Rc);*(--p) = ' ';
     NTERROGATE:
}
  plci->m,p=0*/plci, &              /*       dbning is_parms))
    1];
            %x",ai_pl;
      c[29][2] = {
   default:
     if(SSreq=         {
          breL.X*(p++_set_b=%  send_req(rplci);
 ength==8) /* workaros[4].lenber = Numbe*, A7)<<4 | (((CA  if(api_parse(ROP = appl;
   OCK_PLCI;
           default:
 _REQ_ROGATE:

            case S_CALL_D     break;
       &a->plci[i-1];
              swit(1,dprintf("format     1];
                   case S_CALL_D_PEND;
      - 1; i++)
     {
 ci,S_SUPPOR   sig_req(rpprintf       {

      d ap*------msg->header.length;ack_pendin }
            e;
      /* dir 0=DTE, 1=Drms[5].info);
            add_p(rp }

 >S_H    LER sk[appl->     {
              rms[5].info);
            add_p(re;
            break;

          case S_INTERROGATE_DIVERSannot checbug(/* Funta_pending, OUT_OF_PLCI;    {
msg->hcase S_CALL_FOR("listen_req(Appl=0x%x)",appl-%x:DSION: /* ) = MSGcontrollcommand));
    R    {
  low */
           _STATE;

 ) | 0x80n->DBuffe_p(rplci,CAI,cai_com);
 "       der.cpter[(R,  
                l      ;
        }
        ielPLCI=%      dilci,n        );

 mand = INTE(&cai[2],GET_WORD(&(ss_0].length || ai3_T90 if(api_parse(&parms->ig"));
               (rpl_req},
  {tern"th,"wbs"ci,ASSIGN,DSIG_ID);
             ngth < sizeof (msg->te)channel; /* n\x69\x32\rmat w, eitheinfo[( _WRON  add_p(rplci,CAI,ult:
ADAPTER *a,
	            /* c); /* Basic Serviis cormsg_rue;
  0x08
#define Vte f-------(1,dprintf("info_res"headease after          db!Info)
t;
 aiappl;

    OAD,ss_p(TE_NUM; /* Function  
    =0;
  &,dprin*/
      s_mask/* r (j = E *in, byte     e byte rIVA_CAPI_ADAPTE*/
 ai);;
    case 's':
      ifeturn f",rplcitatic= (byte)GET_WORD(&(ss_pF_PLCI;
                   :%d,API_Sa->       tatic g(1,(plc      else
     !].info[ug(1,dd}
   a  appl->S_Handle = (PLCI   *  {_QUEUE_SIZE -ci->numberr(i=0; i<max_appl= appl;
       
          ainternal_command",)lci->Swdisc      ss    else
             below */
              RT:
    if((i=get_plci_ring_li                    ca {
    s[0].in          add+RVICE,0);
            seUIRED)
     db(j=l;
     aa               },           cai->Id));
all          }

   j-ublis*/
    T_WORD(ai_parm_group->Not           ((ncci < OAD,H;
    fncci]        }
   /* wron    f (a-CH_REQ41INTERROch[a- send_req(rplci);
  && !H;
    cch[a->ch_].le(rplci);
  U
      ",  }
 _PEND;
   L_DIR_OUT |     }

OUT_OF     {
               if(ai_parconnect_b3+5        DDER_ Cif (move(wor+ SCCBSpes     }ONNECT_I, Id,          }
  SHIFT|6,arms
            {
   _plcS     if2--------   Info = _O90\xa5",     "\x03\x91\x9 = cau_/* wSIGN,CI   *        mao API_PARe      f OK        /* 2 nfo[1],( {
        ix61\xI
      rc_pl         >adapter_disa   API_PARSEf(parms)
    {
      parms[i].info = &msg[p];
    }
    switch(format[i]) {
 )u /*  Erd, 2x       ndle = GATE;

n IND----------         add_p(rp/s[3]tic un/
   screE:
 gied for or"));
           F_PLCI;
              bre CFpl->S_   rci_rioid next_intd == *in[i]_id;
  if (TE:
 M       t if IE    rd j;

  if(locontroller = yte)roiei_pto c Transmlf (!IVA_tate!=I:
   tState)    {d ncci_mappcause of US I_ADset,            Ims->inf    switch(SSrerelci_removstatimIE_SDRA ((3,0x08,o       
    send
            LCIvalue /
/*yte facili  {
         dbug(1,dp (by */
     _plcd = b >> 5info[0,S_SERess;
   ains{ "\xci,CA1ERSION:
T_DWO     for(i=0; ber umpf + 36;arms)
    {;
  s  *
 y->ptlar(plci->cp(plst_c
              {
",rpffffffinfo[0])))rn & 0xffo[0]));fo[1],(wo(&cai[1 {
  Po_Mask byte m;  cai[1]AT;
lse Ipl))PLCI   lue));
(ss_Rrn fals-1;
  _p(rplci,CAI,"\x] = 1;
               dT_DWO------- Basi4 Servi].info[(s);
     in[pl))sconneND;
 wx32\x30;
  edPLCIw &ject&0xppl->I->intern{
detrollcongespi_pa    0uppStedPLCIupB3_T4pControdi   add_p Eic     now     ;
        unction)* Bas]);
 ch       send_req18\xfd"ate = H,
		     PWORD+1]ig_req    add_p(rplci,OA   r----------validh]))t exceer.co ORD(ofnc plci.length);
      _WROoc+ Bas)    x02\UIRED)
        I      Confere->CIP_M0x7reak;=I|| ai_par&cai[12 caseci,CPN,ssw&   {)  cai[1;
              }
            k = plci-e  ncci_mad_p(r8)i,S_SERID);
            ad7&parms[4]);
      =              |>codeSAGE_FORMAT;
            d; /th = 0;
* Bas  {
 QUEUE_FULL;
  }led)     )   */N_R|CO        e_check(P1;
        g(1,dug(1,dprintf("rplci1              s+1
            i]!=  {
                add_       rplci-;
          BEGI(rplci,OAD,sntf("N"\x02\x8fo[0]}
        fi      d       "\x02\     }

 eturnS_INTERROGbreak;
 retic voe Fr           s->info[addlik  if(GET_WORx80");
     LEVELS - 1ALERT  rplci->ovil = 0;

}
 rms[3]_plci,UID,"\x06\x43\mIE          ,us */
 cai[0] =     char  if(plci && plpos;
       ==Oncci      }
foppl-    {
 =     _NRSUPPO rplci = &a->pl)
    ADch_mask;
  byte        rp2 switch(},              eq(plcif;
        ifeq(plci);
        ; /* Basic Servid Id, word Nh,"wbwss",ss_p7k[app(api_parse(h,"wbwss",s  rplcc byte disconnect  send_req(rplc        end_req(rplci);
  ity_req(dword ISSAGE_FORM publf + 36;ffte!=S plci->msgeue    ;
  if(a) {
  _enquiak;
+=sig_ree
            {
(ncci_preq(rplci);
            }
            else
            {
              Intryfo[0      a!= p_WORDfp(rplci,CBCT_DWOHL    e 'w':
      p +=2;
      break;
    case 'd':
      p +=4;
      break;
    case 's':
      if(msg[p(rplcie_posInfo
     *ie1;
  if (ie2, /* 10 */
  { "BEGIie1Free  add_  Info =_DATA_B3_V:    c;
         --------Dword)parm  ck( if(word)p   plci-A voie    Inie2(rc_preturn (dtmf_reqUPPORTED_ rplci->numbcid dn;
   byte:
            if(api_;
  if (b= CONF_IShplci  connect_b3_rPARSE sLL;
  if M9
     !      rplci-c,= plc     senu_law]);x32\ELECTOR_Ej=16;j<29 &LLe disconnectendR     }
       apapj flaags elsug(1,dc     }
 msg)d, Numh dbugINTERR;
       j==29return (d   {
plci->S      && !plci->advAdd70\x         *parms  Info = _WRONG_MESS    [2].le*ftyj++)
  esg));

    
        duma_featem, appl, msg));


      case*i->Id))y--------------length -eq},    {       FF;
   ms->lengtE__))s[0].  *)(p     ------n   {
1;j++) msg_unctio    TYbwws blci  = MSa    cfo)]rplci->	edPLLECTOR_ULL;
llerPI_ADAPTPI_ADAPTER;
}

s7  Infose;
}

stafnoCh ) {
     {
 90",            *,ms->in    seblci- -----       bre1];
       dbug(0,    ,x80)
   ,SS      MAT;
         rintf0]& EXT_C;
  ret = fawrongase S            sESSAGE_arstyStass0)
   ) *g(1,dpfoun      _PLCI;
    ,j=1;dprincMULTI_IE-----    ca            /(MapController ((bND;
   V_ECFrong")ncci;ER;
  reci->comman  }

=TIFIER;
  ret              
        
{
  word>internal_cobyt
  retur    word)ig.Idpi_paai[1]IE             {k=0;k<d i,    d{
   wrong")*
  You sh
static bi]Contro    k switchIVATE:
 selectorlength_wrap_pos
    0x83  4)adgfo); /eq;
[2(&ai->("con   /* 20 */
  l->Id));

 nvocatiolci->numbe(PLCI   *, API_PArcArrnfo =aCONN*(_req},
  e faciDIVA_CAPi, ADIVA_C1         lc2->SuppStatewith     
      rnal_command = PTY_REQ_PEND;
    (    )nd ofNG)   _Servnternplci->e)(rel_parms[5].info); /* Controlling User Number */
            sig_req(rplci,S_SER ((i != ign ad    re (dwo        {
                dbug(1, dprintf("wrong Controller use 2nd PLCI=PLCI"));
                rplci = plc     if(msg[pbyte   if          db rplci->appl )
  *OR_V,dpri as   dbugMAT;i_parse();
  return     rpl   }>intelengt[1c Servicap_pos      _Lncci]=   *,lci, AP&info->as",            coxtDevON(C    rp    _X25_DCE))
r!= tf

SO8      = B2_SDL ? }

            (rplci);
  RD(parms[ret    7_Mask[appl->Id-1  *)0On,plci);
  onInfo;   (rplci);
  ESCss",ai_phrms->length==7 ((i != 0)    if(!I    r = Number;

TEL_CTRLsend_req(plc      else
   i;
= "\xS;
   
  pse of US stuv !Info)     f    ca    applore S_NL.DCE))mappin_WRITE    }
000000ptyS         ],GET_WO
    }

H:
  ff; /* move to rplc     if (!->msg_in_rplcFFi_parms[ot    B2_;
  _FREE_S
    E8))
          && ((Info,s    
  if(a)
  {
    if                3 al     {
  nec
            {
             IdI   *----rapplss(plci, AS
             rplci->interCAPI_ADAPTlci->numbd_queue[0]))(Advlci->m_BAor
  dw));


      case SEi, OK);
    if(App 3;
  | (Mrlci_reinook false;
 ;
    caseirection */
----------n false;
   hardwAT;
      rs3_ree(&paic vos opt          v.     ))))))
d-1)s);
 0);
 a & Cbo          */
  "", {                FO_Ro    ca>lenug(1,dect     e       _enquiE,0);
 _DIR_FORCE_O     i[4],GET_DWORD(&(sonfere    LCI  pvc[0] -pECTED_AL3 swit
            {
              &paN_RESEy
  tr (i = 0Gci->c_i       & HANDSE   }
          "plci,-------    rllerPVCAT;
      e fo
   ,'6'plci,CPN,s         }

         "wrong Ctrl"*plci, APPLrplct].Pfor|nd =    Number,
       word, DIVA_CAPI_ADAPTER API_P      P

stuct[] = s is op==5n fal add_s(rc_p   * a;
    bug(1,T;
  )
n     by , PLCe] + (msg[p+2*/3_req"));
  if(ND;
     F_ADD:
                  },              case S"wbs",ss_parms))("R /* mappinConference Size }
  else
RD(ai_parms[0].info+1);
  dv
      sti->msg h-3),&nc DIVA_oid G_MESSAGE_FORMATc void dtmf_c&
          case S_INTERROGAT      i_ncci>appl_fapping fo APPL *appl      2    (!Info)
       NSE)))
       }     );
       add*/
       h,"wbdels !(((rela Obit 9) no     2] ||rd i, jecn     ("Sende02\x9ngs},
d = C_NCR_FA
            wctiobe  API_     3];    rONG_S(& T3  }
*/
are */(cpi->iord,msg)
cha
   ber 4n false;
  _bits);
       Info = i=geti, S
              Info = _    d{
        p0, PLCI {
    dbADVVICECCBS_DchS
      _plci,NCR_FACILITY,0);
 __a__( APPL   *,tate!=INC_>faxNOa_b3_res}}
  }
     mand e0\x03             \x90\CCBS_DR *a,
	ed_msug(1,dprint   }

               ss_parms[4].inf((wul,
  bu1, APAPPL *, tails.pl;
      
          /* eamsg)
b          PUT  ((w & 0x0001) ? T30_RESOLLUTIWORD(&(_MES any lat0) | false;
      }   API_ts l =add_b23(plci, rd, word, DIVA_CAP either v&                 (byte)((T30_INFO       k[appl-----------    ppassign LLING | f channel ID */
api_pars_bits &= ~(TTROL_B ((i !=
           on =      cai[1]         }
-----
              {
  eplci->intern /* Request t!(p (_DISCONNEBFACTn    pyte code,=%d wra* reuse unused screening indieq(plci);
        ai[8]            _R  *)0; ai_parms[i].lei->l[ncci])
            A[4].d   add_s(plccase S_17..31 */
        eq)
     13         NTS))
           /  || (plci->State
    API_NTROL_BITp(rplcbute L   *-*/
eequestes &T30_RCO;
          add_     {
          s&1   *ON_BO0xff(cTA_B dbug(1,dpri User Number 
    API_300Bord, DIVA_OL_BIT      returns",s_CALL_DEFLhex_RONG_IDE a->Id;
  i     1<<e = true;
               S_CCBS_ic voSCO_FORMAT;    
  ~ APPL->spoof      RE_BIT_MORE_    case S_INTERROGATES/rd)((     _res}
};
i->int;
  ---*wonferx61\x70\x     her v_in_wr shut d[j];
  }
 )(plci-      case S_CALL_DEi->fax_conneo plcdword .t  div     isdon        aT_MO__a__) N_RESET;
5_INFO);
     SE     (Id, et.BI      {
     }
  wis '    {
     ->he';

 LER tr    it_e plcs>info[1]B3 connheck           break;
                 LLING | T30_CONTR    i4) /* Request to send)
            }

       and == (_DI&&     (diva_x, PLCI   ----_PARSE * p)TSk;
            }
    0xC].len90"  B  , o); /* RdID] = 3;
      ((w        >= WORD(aId
       |s[0].info[i+5]!=0)trol_bits 0, false);
 =1;false;
    }

ROL_BIT_MORE_INFO   *)(p     esc_t[2] =st (Id,    API_PARSE d"wbw=SPOOFING_REQUIRED)
           =
                  (byte)( !Info no releaseand == rollinpoof_IdC = 0x60|(byte)G          _DOCp[0].infolut    esc_t[2]        ature_bits__res}ed by
  teature_bits = REQCONTROL_B
  plci->ms
  word p;
  plowntende))_res}
};
    siparms[0].l|=   if ((     tate!=INC_AX_NONSTANDARD)1,dprmsg)[cci_free_rreq(dw(            addx18\xfd");  /* D if(}, /* tempSUB_SE

  pr->data_ack_pen /* stanInfostatic byt 
      ci, aue;
    
        adETRIEGAX_NONSTANDARD)Blci->intj].o = _WRONG(PLCI   *,p(rpmat)
     x69\x3NG | T30_CONTROL_Barms[j].MWI
    ms)
    {
      parms[i].info = &msg[p];
    }
    switch(format[i       sk  *)(physistatdo_PARSivatP/SUoptiPCI            ine DIci, wonsL << (bG_STATE;
  PTER *a,
			   PLC      { *plci      {
             ask->he || (sdram_baPLCI->vsprotdase S_CAL* a, appl, msg));


     CEPT_PO-----     if IDI    pl)
  s->info[1lse
I_PR 0x7f)) > max_adpl
            dbug(1,dprintf("rplci->Id:%xXDInternnel_xmit_xon (PLo[0]))i[i-1];
    SSAGi[i-1];
  )        }
 e->us     ND     cg, word b1    
           f (((rela_DIR_OUT | CALL_IT_ACCEPT_SUBADDRESS |  er
       T30_CONTROL_B if (w & 0x0002  }

         ci = &a-info_bufferit   if(a3].ix case    }
 (*if(a            Inl_bit0] =AT;
       IT_ACCEPT_SUBADDRESS        N_QUEUE_SIZE -       cAi,S_S      BAprin%08 DEAfax_conw = GE;
    
       s_req(p   fax_control_bits |= T30_CONTROL_BIT_ACCEPT_SUBADDRESS | T30_CONI_PROOVIDEt frab    send             if(plci->= (byt    w = GE               {IT_A    E_FO&par139, |ength;
                 PASSW  rplci->interppl->S_nnected
 :
            if(ap = Gntrol_bits            f
     i->commifnclude eq(plci);
      ic voidIT_ACCEPT_SUBADDRESrms->inf)+4_BIT].length_pars                   i;

.  break;
                  dbug(1     if :w & 0x0002)    format on functre (dw     "w= (b _L1_adapter;
      )der.co    plci->fax_PASSW    if (((relrms->infe SELECTth; IT_ACCEPT_SUBADDRESS )
  w >IDLE;
          dbug(1,dprintf("   rplci ci, by|| (l >=  plci->fax_connecwEND;       {
          _connect_icai[0] on_id_len = (byte] = fax_parms[5reakTIVATor (i = 0; i < mw           D     ] = DIVERS+] = (byte) w;
       wsssss[i] = *, APId
       |    {nect_info_30_INFO  ir = Cinfo_buffer))->head_line_len = 0;nnected
 EAD;
*/
     G |
                  T30_CONTROL,"\x06\x43\x6  plci->byte)(eue (plcfax_controup_in\x70\x69\xrintf(" DEAC                ss_pinternal_c    {'0req(plg(1,dpLL;
      }
    ((i=get_pfax_parms[      }
             {
          equested_options_conn | p)
       User Number \x01\x01");
                 case S_CALL_DE        w = fax_p---- (w & 0x0002)    len+] = fCONF_REATTATE_FAX_NONSTANDARD);connect_inons_command = MWI_DEACTIVATE_REQ   rfo_Mask[aRx_xonve (plci, 0, falsemat          case S_CALL_DE        w = fax_pfax_parms[5SELECTax_parms[57mber, a1]_con2ESSAGE_FORMAT;
}
     s  "s"en++] = (byte) if (w & 0x0002)_parms[7].infpi_pars 0; i < fax_p  }

    iPARSE ss__RCQUEUE&1L << i < fax_parms[7].length; i++)
     plci->fax_connect_PARSE ss_mmand = MWI_DEA)(plci->msg    iflci, wo+] = (byte) w;
     ci_remove(%x,tel=%91,0xac,0 w = OR1] = 0;
             a,th);
  _parms[5].info); /* Controlling User Number */
            sig_req(rplci,S_SERaue   *)c l&parmplci,LLI,lli);
      or B2 protocol not any LAPD
               and connect_b3_req contradicts originate/answer direction *
     ,'6'  rplOS Receific      aSAGE_FOi(T30_INtMAX_NAC")); = fax_par      APPL   -w = (Euro)
           us,japan)0002)    esc_t[BC);
     
Setupk lengtal_cobyte A);
     Law     if (w & 0x0002)pi_parse(&parm    3
              /* x_infoS);
     5].l        f (ple  req 
   ->heais o(i = 0; iI_PAopt_info_buffer[len++] = fax_     w =   j = */
#& (1L << PRIVATE_FAX_NONST30_INng_queue))        parms->_POtart_internal_cos (crintf("Narms | a->requested_o     either.command == (_DI0x4000)) /* Private non-standard facNTOFING
          {
      USELA 20;
       fax_coe sed w S_CALL_FORtic byt------nfo.b,(ities enable */
       arms[i].length control_bitIT  /|=             }
 ax_parms[5 l =      cdworn8000)1],(wP     _srms-leas20EQ  ESSA*/
retur   else 
  turn false;
  SION:
  j,e  Ins_     irection */
OAD,ss_pR   ; sig /* explw = fax_par         mber = Number;
  DDRESS | T30*)(pl+     :SSreq     }

c byte c_  if (ncci ==ci[ncci]R          i1];
 r (j 3\x6->fax_conn[* plc->fax_connect_info_buffequested_w [5].ine  diveq = N_CONDCE))     }
 i->lenax      i;


  d1  * rET_WORD     pl=   },.length L    plci->interWRha CCBe> mamappi);
  return fa, /* 24 2+w] = ncpi           ss_parms[2]i->len[1+wDIreq.connect_engtfax_connect     repmy_plcis.| ai""
{
  dbug(1se(&pmy_plcitatic voi = MSG_             }, /* 24 */70\xx69\x-----gn unrms->info[1CIP3API_ADAPTER *a,
		      Pelse
Info(PLCAPI_ADAPTER *a,
		      P_parms]))); /DAPTER *a,
		  ->length, (b & 0x1f))) != 0);
}
DAPTER *a,
		    _command turne           bp _BIT_REQUEST_PO----llrite_pos;
    fax        T_CONTROLLER)
 hange = true;
              eue (pl    ->fax_conne        /* 2 
          case Sinfo_req(dw);
_parms       if(noCh)           adbytef----------Sno         if (w & 0x000q     RSE fax_  {
    ireak;

ax_parms[ && !dpry((w &0\x6e     cpRT) {GET_WORq; = ECT_REQ_PErintf("f
    re

static byt DEACT++) ai_parms[i].= trREJ0, "wcommand (dwor,               (&app        f(pppl,;
      art(voi* pl3\x91\xe0\x0);
        fo{
     lci, ASSIGN, (pl1];
             a->ncci_for (k = 0; k <byt(y_plci;
 N_TY_  br     , "w", ength);
        fo{
       r = Number;

0;
   <msg_i                     T_B3word)1<<8)|a-th,"w)
     3len+    dbug(1,dprintf("plci=%x",m        case S_CONF_REATlci,S_SERVICE,0);
                 /*  rplci->call_di    a->ncci_plci[ncci] =mber, a[0].       mmy_plci;
 N _L1_,lse
    || (msg->      if(!msg[1]] & 0x200)
    {
  = ch;
    a->cNC    _      DEACTIVATEss'1','2, DIbyci] = INC_ACT_PENDING;

      req ions_c  Info+3);ndinOTHER_APPF_BEGIN:
        ; i < ARRAYs is opt== 7
                 rplci wsssss"      {
  D_ALERT)
 ==LERTnfo_buffer))->head_line_leis corre no releasMAX T30_NS4      
            EN_ind(NSFt_b3_annels =x_connect__FAX_NONSTA cai[1] = CONF_REATTrm.               Ich 17..31 */return ->reyna{
   1rnal_ch 17..31 */
             *rp    plci          rplclaif (->fax_connedo
    tern    PI_PARSEic byte disconnect_reqnfo_le_PARSE * p);
S)
PPL   *,
     onf exyte        case S_INTERROGATE_          olutio byte {_MANsend_req(rplci);
               }
   ] = 3;
     );
            sig_ren false;
 ORE_           }
            elACTURER_FEATURE_FAX_MORE_DOCUME    ].info);
            add_p(rp->commang(1,dprintf("f         rplci->comma         SEP_PWa3",            {
    REM_L11] = 0;
       rI_ADAj].lengthA                cai[1] = DEACTIV 13    len++] = (byt                else
ber;      &)
      {
          plci->inI_ADA  {
              pS_Handle = GE      & is optPL   *,on_id_len = (byte) -------------------i, dword*, APPy local mea i;->N

  R(!api_pACILITY,0);c0\xc6 = MSG__WRONG_MESSAGfax_connect_info_buffer))-lci,CAI,cai);
            add_ps[5].length;
          }

  (b & ST_POLLING | +i];
       0;  /* n conne          }

     TIATE_RESP))
   {
      offsetof(T30len];
))
            internal_command =  w = fons_tab /*lse
  ax_parms[_mask;
  byte m   API_PARSEIGURE;LECTION:
     plci->St coC>info[1],(wci,REarms[7].len      I              send_req(rplc    {
 ARD);
   & T30_NS{
     }&&        
 {
   if (((plci->Bug(1,dprin{
  dfax_conni->State = CONF_BEGIl, 0)){
     = = CONF_ISIdfax_cocpi->info[1], ncpi->length, "w if (w & 0x00       RRAYss_parms= NULL;req(Appl=0x%x)",appl- *)plci ,Sigeck if printf("RAX_NCCI       ppl )
            {     & (length; i++)info_buffer)     PLCI *plc     break;
    clci, ATE;

                     rplci->("format wrond_ai(p\x90se;
         x90\x_b3_resMAX_NCCITctrl")_l,
  buplci->fax_coa2"     }, /*i <= ((_NCCIl"));
  ,Irong"     if(ncp(_DISCONNE  {
              ((IT_ENrms[7].)d, DIV     }91\x90\xa5e;
            ;
    [0] = ncpi->length + 1;
      plci->internalx_parm       plci->v        te aleARD);
     fax_connecOAD,s_DATA_B    appl->CDEnable = true;
            cai[0] = 1;
            cai[1] = CALi->St    _PR      nl_bus ncci)elsi->appr[len]       fo =*);
  el         [len++]xed,dprintf(;       e;
      CE,0);
   arms[7].l )
            {
   E;
      i[1] = DIVERSD);
       )
            {
         "\x02\          plci codec not supported */
    }30_INFO   *)(plci->fax_q    _CO     appl-           if( (Id, NumbCDEable[     _PLCI;
          <= len)
         sendf(appl      )GET_       || (((pplr (j = 0         {
                dbug(1, dprintf("wrong Controller use 2nd PLCI=PLCI"));
                rplci = plc     i        , __LI              } (pl }D);
 nd _info_sig_req(plci,UN_COc(plcON_Service    }(ai_p
d','e'm, DIVA_;
  {GIN;
,
  return      
    Aut   }
 t_infHASHMARK      ;
  return false;aAPPL *appl, API_PARSE *l->S"listen_req(Appl=0x%x)30APPL *appl, API_PARSE *0b >> 5apping        Id>1APPL *appl, API_PARSE *1onnect_b3_a_res(ncci=0x      }appl, API_PARSE *2onnect_b3_a_res(ncci=0x = ECT_REQ_PEND;
       3onnect_b3_a_res(ncci=0x4APPL *appl, API_PARSE *4onnect_b3_a_res(ncci=0x5APPL *appl, API_PARSE *5onnect_b3_a_res(ncci=0x6APPL *appl, API_PARSE *6onnect_b3_a_res(ncci=0x7APPL *appl, API_PARSE *cci_map_b3_a_res(ncci=0x8APPL *appl, API_PARSE *8quest_xon (plci, a->ncc9APPL *appl, API_PARSE *9"listen_req(Appl=0x%x)4     --------);
      seAI_PARSE *parms)
{
  worl_bid_s(p  }

          BI_PARSE *parms)
{
  wor = ECT_REQ_PEND;
       CI_PARSE *parms)
{
  worarms[
   if (((plci->add_"listen_req(Appl=0pl, 6E *);
staticG_IDENTIFIER;
  ret = false;
 ERT;
 PARSE *msgInfo = _ALERT_IGNORED;
    if(plERT;
 byte cau_t[] = {>vswitchstatelci->Stao = _WRONG_nect_ERT)
    {
      clear_csten_req4pl, AGINC_8>  "",   plciLrms-eICE  parms = &ERT;
*/
/* Xb3_req(d    a->UNIDbuffFIEi(plci ch;
    a-PENDI*/
/* X*a,
			     || DIALch;
    a-3_prot == s        = ECT_R    || PABX_ +
      pi;

  dbug(1,dpri           sig ncpi;

    || SPECi;

xon (plci, a->ncci_ch[ncci]);
   %ld %08lx      ECOpes  
   if (((plci->B3_        sigbreak;=      chyte i->NL.no te[ncci] = OUTG_DIS_PEe Revis;_options_conn |
             =(plciT3        plc||ns (ch==    || BUSYci_state[ncci] = OUTG_DIS_PE_STATE;
    || CIvalSnse fi_state[ncci] = OUTG_DIS_PENreak;

_options_conn |CONF_t_blci->relatedPTYPLCI = plci;rms-bnfo_buffer[len] MFORparms)NSIONS)))
      {
     TIVATE:fax_connHOL ch;
    a-cci] = OUTG_DIS_PEe
  {
    f     d Id, (plci, ncci);

        if(p(plci->mand (Id,AAdvC_WAI
    d)
                   ncpi fINFO         -QUEU {
      &_INFO   *)(p406\x43\x61\x9)
           chanfo[1+i];
parms[2]);
  plci->9      ;
      }POSp);
void TransmitATE_FAX_NONSTANDAelatedPTYPms[3].length)
aNEGA< MAX_NCCIPI_ADAPTER *a,
		      PLCI *plNu_req(rplci,se SWAR adjfalse;
}

static byte discon byte l        INTRU  {
false;
}

static byte disconnl_req_ncng|= (1L << P-C0xffappl>faxIRM,
        Id,
        Nu", _OTE;
      PAYPHCT_I, {
GNIlci[ncci] = 0;
         dbugif(9        & is opCPE !Infoarms    ||eq(dword lci->command urn POOFINGlci,dwordfo[1 _WRONG_IDENTIFIER;
  ncci = (worAP_DISCONeq(plci);
requove_id)
        o[4]));
       c,
        _ || ))->da plci->NL.          PLCI *p        appl,
        _[0] = ncpi->leng (plci((msg-    sig_  &mber,
        "oid fax_dis& (plci->B2_prot != B2_nect_b3_res(dwo, adjus,   L 
      l, 0)){
      r!=3 && LER *a,
			     ANSAM_buffe= B3_X25_DCE))
      &arms)
{
  word oCh = true;
          ncci_ps[7].len!=lse
(1,dprintf("disBELL103NL;
    }
    for(i=0; i<MAX_CHANNEL;
            MAT;
   S& (plci->B2_prot != B2_x_connect_info_G2}
   GRO     ps[0];
          T_PEN  add_b1mand (IdHUMAN    ECH& (plci->B2_prot != B2_
          }
  1)
    E;
 ACHities9Sparms = (by2pl, Af];
 fb3_req(dq(dword Id, word Nui->fa *)0&& i->Stdf(appl*a,
			 		      PLCI *plci,e*a,
	& GET_ !plci->chan = ECT_R(plci->State ==        sig_req( !plci->chan ncpi;

;

  dbug(1,dprintf] = OUTG_DIS !plci->chanH;
       if (((plci->B3_ile Revis    | !plci->chanT)
     !DISCONNECT_I, Id, 0, "w",conne !plci->chan3, "\x03x03\x04R *a,
			        {0\x01" !plci->channs (ch==s;
   b3_res(dwo             /* !plci->chanId,
         plci->vsprotdialect=0;

  DING){
      reak;

    plci->State=INCSparms = (by !plci->chanDCE))
  DLE) && (plci->StatKbreak        }

    e=ID        plci->fax_co_DCE))
 unctio  breakNG)
fax_con        state[ncci] == INC_DPo = ;
  if(a)          _INFO  l
                rpl_prot == 5))
       && (a-      
                rplnIDLE;
   wordand d fax_discoMAP_);
sIESnew_b1_resouparms = (by_re)c byinfo missin   },);
}

i);
N_D_r== 0)
  {
           f(!msg[ (pl_DCE))
    mins = (byd       , API_gap



            plci    rplci-  re%06lx] rnal_coRIVATE_FAX_NONSTi->adwrap=%     api_parselectorId3].info-------
      gth >| N_D    no B channel x",
      = Adord g),ernal_r _,n = 2;
              Id = 2;
      CR_Facip       's':
   



     vs{
   0;
 plci);
spuls-----    if(ncpi->   {
      cleaDINGI   ->adapNTIFIER;
  ret =    plci->State=INCnau (plcx     _      } && !   Numect_inf ((by);
      a3",   *plci,D;
  *, A* &RCparms[1],onnect_to (01\x80")
     _parms[       /* 14 */
  if (((plci->B3== SUfax_ = (word)(Id>>16);LITY,Rd, wo    nl_req_[0]>3cpi;

 os;
       || NTIFIER;
  ret =    return fa        rplci->appl5>staatrms->lentr->data_pending;
      if (i ;
   request_ MSG_IN_QUEUE_te)(rel-=  InfLITY,0)rintf("plaqI_ADAPTERc void data_al);
}

si->NL.  *);
static void vo = (word)(Id>>16);internal_command =->stap(plc      dbug(0,data_pending;
      if (i = OUTG_DIS_PEms[3].lengtG_IN_QUEUE          */parms[0].info)) >= ((bytaAPI_MSG ci->msg_in_queu_WRONG_      && (((byte   *)(pa
  plci->mparms[0].info)) >= 
  staticrong")i[ncci]);
,0x08,0x0001)))
              ; /* Bad i;


  d4]ect_info_bus[5].info);
            add_p(


stat=E:
    void s(plci,&parms[5 14 */
  /* Bax_control_ << PRIVAT"));
   ----eak;
      i, APPL *msg-= (byf(apllb    ppi  if ([0]._DCE))
    w,PLCI   REQ_PEND;
                cai[1] = ECT_ add_d(del;
sto[1],(w     
      eai[2]ITY,0);D   f    at&parmif ((a->ncci_state[ncci] =able[b >> 5    {
      clearat         plci->ndbug(1,  &(a->ncci[ncci]);
 

        data->P = (bytelse
arse(b     ck[i].Number = dlci- if (plca->ncci_state[ncci] == CO        mix       rplci->appdata_pending;
      if (i >= Mck the if (plci-g         r i++)
   xon (plci, a->ncci_cord);
  -1] = GETWRONG_STATE;

 f(api_PLCI;
         fax_i-----*/R, &parm      {
            pl        start_i  ifi->adapelse In(w <          ad- 1; i++)
 ax_parmsinfo[1&& ((i -=-/* T!=Sparms = (bylci,w].PPL *appl->lengtnsf_contro           {
     plci->State *)(>
          PL +=4;
      bre       "",  



     if (!prfax_p-1] = G    :E;

                rplr, DIVI,Id,0,"s0x[0].info)));

       + }
  plci->m*)(p                      _DATA_B;
wordROL_BIT_MORE5\x00\x00ue));
       DATA_B    cai[0] = lci, APPL *value));
       DATA_BPLCI PLCI *plci, APPL *length)_CAPI_ADA            nl_req_ncbyte   ning indicatcheE *)          rplc   TransmitBuf(I   if (adapter->p  if    +< channel))))t    );
sI *plci, Afax_       } >= MAX_DATA_ACK)
          i -= MAX_DATA_ACK;
        ncci_ptr->DataAck[i].Number = data-ci_ptr->data* plci);
static vword ncci;
  lci));

  if (plcx,ert Call_Res"));
    ims[0].infpl,_CONN;
        _CAPmove(PLCI   *);
static vo= plci;
      (lci, ->stif(plci && ncci) {
  send_clear_config (PLCI   *plci)
{

  dbug (1, dprintf ("[%06lx] %s,%d: dtmf_/*
 *
  Copyright",
    (dword)(( Net->Id << 8) | UnMapController ks rangadapternge ))  Eiconcharn : )(FILE_), __LINE__));ks, *
  Eiplied for requests = 0;ee software; you capulse_mtribute it and/or modify
 aut under the}


static voidpre; yprepare_switchons raw Id, c)ion : oftwaorree 2002.or theThis source file at yulished by
 blic Free
sion :erverId (Id),:Eico2.1 2, or (at program)
  free whileor theEior modify
n redistr!= 0ndatiore; ye witrmation)
  ,it an)ic License asand/ore; yoavese with ( and/or FoundSOEVE; ei, byte Rcther version 2, or (at your option)
  any laterof MERCHANT%02x %d the hope that it Y WARr thed insion.hope that i, RcCLUDINon :just_b_ impet will return (GOODG ANY
  imre; yowarrantyrestor for moreABILITY or FITNESS FOR A PARTICULAR  t, wriInfowill rsion 2, or (at your option)
  any laterion.
 *
 S and detailsn 2, orYou should havng wceivty o copynse ion.GNU General Publ ANY
  imp
  alongidge =)
  phe tifeful,
  B1_facilities & B1_FACe
  F_DTMFRY OFve,   .h"
#incvacapi.h"
#include "m  Thi------case ADJUST_B_RESTOREC"
#d_1:  Thi it and/inFileal_command =ivacapi.h"
#inclis optihe trs tivasync._nl_busy
    *)-/* XDI driver. Allo i*/.h"
#include "m=/* XDI driver. Allo it-- Fils th  breakpter*/
/}ter*/
/re; yenable_ncludefs.h"
#idivacapiplatforc_active)pter*/
/o teris not necessary to ask terfrom every ad2pter*/
/ptiofined hto ask it from every ada---2/* XDI dat a(RcRRANOK) && /* XDI dr_FC
/* XDI driver. Allrsion 2, or (at your option)
  anyRe s"
#i ---- t sy
  atfailede "di  This 00001
include "pc.h"
#include "capi20.h"
#include "t wi000101
#.h"
#in_WRONG_STATE Fil*/stat   */
/*---ere have o   */
/*---e haCAPI_with th.h"
rogram; if not p.h"
shNY Wl adaoar "mdr FITNESS , Inc., 675 Mass A dprCambrstat (at   CAPI ,r.h"
prin75 Mamasfined h75 Maresult[4], MA 02139, USAn 2, o*/    */#inclu    dapte  CAPI c "di_d04 (((__a_d>manufactfs.h"
#include "pc.h"
#include "capi20.h"
#include "divacapi only if:
 ope to002
#       n FicmNeneralres & MrEico*/
/undFACTURER_FEA) &&_Odivacapurer_featur FILEM GNU GK_FC_LABEL) &&     (lude "divacapmdm_msgne DIVAORTS_NOd any or0] =RE_OK PUT_WORD (&-------1],PI_US_SUCCESS2
#d2 only if:
  proton  any o only if:
  prothe terms o function prototypethe trts-----x0n Filtf    */----/*es   ANUf----d
/* XDI driveLISTEN_TONEXDI_RT/* XDI driv<<= n Filr. Allo it iicens-----d group_ adamizSOEVE((DIVA_CAPI_ADAPTER  a, c) Eicon.h"
#inc only if:
  prot/* XDI drivedefault/* XDI d.h"
#inc1formour oR IUFACTUR, NULL, ( eithere sh"PI_SUde-----FI|rer_fbit _ "MESSAGE.-----ef/* XDI COMMAND_1local #/* XDI driwic void /* XDI drive.h"
#inclproave b);cense asbRc)---- hopese as Netw diva_xdi_extendedd_featuresibutsk_bit (PLoad-----E------ 0x02
#d#defibit (PLc void setvoidPROVIDES_SDRAM_BAve,API_P*plci, ;
staticd b);
voiNOT_SUPPORTE     ES_NO_CANCEL  0x00000004
#dat are sh"iter(word); pub 
  Cop api_PROVIet_group_in *);
  *, Cap/* XDI driverre sprogr bybyteES_NO_ic dword diva_xdefine function prototype);
ut);
api_re*in, byte remove_startere have oEiconPARSE   *out);
CAPI_get   *);
sta3*in, bytnly loVA_CAi_CMA     eou cae(75 MI   ind__FEATURE_OES_NO_|* plc  *in, byttures (DIVA_( pubget_extended_3---------------------------_CAPI_ADAPTense as   *, CAPI_MSG   *);
static word api_parse(bytEvoid setUSE_CMAd, byteic void si *, API_PARSE *);
static void api_save_msg(API_PARSE  *, API_P, byte *format,  setSAVrd, *outPARSE   *out);
ndedload----(APIet_group_in* tone_lastQ  *icSOEVE_codnly  byte)IGNAL_NOse asse as Allo it i;
t_exte  callbac=o----t(at &SE *);
static voiSE *);
striver. Allo it iicense as puOPCI   * plci);
statc void set_group_in * a,**parm_getcense as pu*out);
set_LCI     *)atic bytEicon NetwARSE   *out);

word d, DIVA_CAmask_bit (PEQ  * a)Id, byte   * *&= ~ byte connec  *in, API;

void   callbaPPL   *, PLCI   /*start(void);
void api_riRelease(Netw   *);
_mask (PLC, APPLI_ADAPord diva_xat TY ooid);

static void plci_A_CAPI_ADAPTE   * *,a a);
static void div
stafor_MSGn FilES_NO_featuresoid nl_iere have oPARTIconnect_resn Netw, nect_rc void setct_req(dword, word, DIVA_CAPI_ADAPTER   *on Fild_fer (DIVA_CAPI_ADAPTER  *, IDIfals* Macrose*, APPL   *, API_set_grouR* plcic void control_rc(PLCI  ures (complesk_bit (Pdriver. Allo it itic void data_rc(PLCI   *, byte);
static void data_ackDisLCI   *, byte);
static void sig_ind(PLCI   *);
static void Sendfarnfo(c) Eicon,d dataA PARTI);

*A PARTLCI   *, APPL  SendSetupidge(TER   *, P  *, API_PARSE */atic by) Eicon Net_res(d ster(word);
ARTItesord, DIVA_CA;
static byLCI&PPL   *, ~(word b);
void AuX |);
static ut);
AutooTSOEcLaword Cap3nd(PLCI   *);

void api_r byte alA PARte connect_a_rgisterord,w
void apndedputPI_ADAPTER TER  MSGid);
voense asARSE *);
sarse(bytUnl API_PRSE *);
staDAPTEI_PARSE * a);
static voindedVA_C*, dword*, API_  *in   *, APci, atPL   *id SendSetupInfo(APPL   *, PLCI   *, dwordPTER   *,, wo_b3_res(dwordetupInPARSE *);
tures (DIVA_  *out);
VS FreeReqInd*, API_PARSE *)dwoSENDse asparms);

static void nl_ind(PLI_PARMF facilitc byte disqonnect_res(dDIGITSSE *)k (c) Eicon NetwCI   *, APPL   *, API_PARSE *);
static by API_PARSE *);
static byte facility_req(dword, word, DIVA_CAPI_I_PARSE *)r *
  Ei lateraramePI_Plength----TY ?, API_PARSE *);
s_ADAPTE *, API_PARSE  :CI   *);
R   *, PLvisEVER:isco byte facilitind(PLCI   *);
te connect_a_res(dword,w*, PLCI   *, APPL   *, API_PARSE *);
static byte connect_b3_req(dword, word, DIVA_CAPI_ADAPtatic, PLCI   *, APPL   *, API_PARSE *);
static byte connect_b3_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_b3_a_res(dword, word, DIVA_CAPI_A   *, API_PARSE *);
static bytee controlRSE   *out);
 Net, DIVA_word, DIV a);
static void diva_ask_for_xdi*, API_PARSVA_CAPI_ADAPTER   *, P  * *,re; ymsg_number_queue[es(dword, woITHOUT ANY WA)++icLaw) && ect_re, word, bit (Pword, word, DIVA_CAPI_ADAPTER   _sdram_bar  *, for digits  *, IDI&PPL   impldTER .ic by[3].infoeq(dwI_ADAPTER   *, PPPL   *, word, TYid);
voRSE   *out);

Adapte_rc   *, API_PDAPTER   yte   *);
static vo a);
static voidataPLCI   *, byte, by_s(PLCI   * plci, byackSen *, APICI   *);ic void sig_ind(PLCI   *);
static void SendInfo(PLCI   *, dword, byte   * *, bytPLCI   *);
R   ITHOUTogra WA_CAPI_AOF Aacilitd, DIVA_CAPI_ADAPTER   *, --d, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
stati DIVA_CAPI_ADCI   *, APPLt_res( ford,es(dworappl,eq(dword, wR | CONFIRM, Id & 0xffffLrd, wordect_reended_"wws"ocol c, SELECTOR-----,eq(dworrogram; if not75 Madapter ANY WOUT ANredis and/oNIctureord, word, _group_n : andation, Inc., 6TER RSE * *);
yte,, API_Pmsg codes selfol codes self , jode-------ms);

staARSE   *ouPTER   *ms[5];

sta& xdid plci_r0s);

sIVA_CSE *);
static voidout);
sS_NO_CANCPARSE *s.
 *
  You should have received a copy of the Gt will----------*);
static void IndParse(PLCI   *, word *, byte   *ES_NO_local f (!(a->protion.Global_Opoid   ThGL*);
stetupInfo(statid, DIVAsion 2, or (at your option)
  anyFSE *);yx0000 plci_r*, APPL   ***, byte   *, byte *);
static byte getChannelte alert_req(dword, word, DIVA_CAPI_ADA havlse  *);
pi Netse (&msg[1PI_PARSwordl, DIVAword, , "w",_CAPI_ADAPT f  *)cipoid nl_ind(PLCI   *);
*);
staticte, Wrong mve og *, ATSOPN_filPI_Pok APPL   *cpn,I   * plci, byte ch, bL   *, 
 for XON pr void d b);
v_FORMAT, wordut);

hannel(GE*);
statCAPI_ADAPTE0PI_PAR) =Ext*, Al_xmetupInfo(_DETECT_CODESR   *,|| e chait_xon word, DIV(dwor);
static intid chann_can_xI_PARoid ch_off (PLCIord);
   *PARSE *ed_olag);
_t"
#i[tatie Re-1] get_plci(& (1Lof DPRADAP*);
stae as)static ord divaA_CAPI_ADAPTER   *, PPPL   *, API_P*, bunknown_PARSE * __)-g_ind(PLCI include "pc.h"
#include "capi20.h"
#includee ch);
static void channel_xme   * *, b*);
static void IndParse(PLUNKNOWN_REQUESTquest_xo haveoid skA PARTIsetuu ca( clear i < 32; i++ord, word, Dplci_r +  cleathe tacilitSte ch);
static void channel_xmit_extended_xon (PLCI  tatic void channeVA_CAPI_ADAPTPTER   *oiceChan  *, API_P_MAP_ENTRIESlOffword, DIVA_ed hac byte c byre; yanufa_map[i].lPL  n_);
static dd_ APPL   *);
static by1discour o);
static   2acATUR>> 3)]* pa(1e iePPL byte b1_res*);
stat


#deilit7e   * *, byte have oe have oI   *);

 Copyright (c) Eicon Netw*);
static;
staa_asies (PLfine FIid adv_voice_cleAPPL  adjust_b1_);
staticPI_ADAdss (dword Id, PLCI   *plci, byte Rc);
static vofacilities (PLfine FARSE *);
static dlic Li (dword Id, PLCI   *pli, byte Rc)newmand   *bp_ms*);
static v3 +hanndR   *, PLCplc3ER   *, ense void c void channs rate bPPL _offPLCI   *plci, byte Rc)chyte Rc)flag);
static bunda void adv_voice_cleelect_b);
static byte d_xon (Pic voidic void adv_voicIDENTIFIERax_connect_ayte facilitSenPPL   Sde "dv_voi|| oid addNL.Id Net23PLCI  * plcie_i  *, API_PARSstatic void select_b_command (dword Id, PLude "m_CAPI_ADAPTER   *, PPARSE * p);
static CodecIdChecke   * *, b;
static void api_save_msnd(PLCI   *);

);
statiPPL   ES_NO*);
staticded_features   ANU---- ch);
static void channel_xmdword, w from every adapThis ------------*);
static v(dwordev it s*plci, dword Id, byte    PLCI   * *, API_PARSrdre FoAPPL   *tic bytrms);

static voidRTICU);
static byte i;

styte Rc);
id nlVA_C, DIVA_CAPI_ byte facil API_PARSE *);
static fax_edadMPPL   iIEid adv_voice_clc by Rc);
staatic byte dtmf_reon Netw Iion;Fion)
tic byte dtmf_reqatic void init_b1_c chac byms init_bie_type, APPL   *);ord, , bPLCI   plci, bytpPAPI_(dword, word, DAdvand (Splci_rCI   * plci, byte ch, by  *, API_P;
static void fax_adjust_b23_command (dword I (dword Id, PLCI   *plci, byte  );
staticeq(dword, wot*plci);

anne   *, byte, bylag);
c void ES_NO_CANCEL  0x000RSE *);
staword, word, DIVA_CA;
stPLCI onnect_res(dword, word, _grouc);
statica-nufactureended_fennel  Thc);
BEL) &&     noti_HARDon (d*, API_Pms&& ities);
static wor PLCI   *plcmixer_datefy_updR  *SOF, dworRECEIVESOEVERVA_CAPI_
static void r*);
static byte pliedatic a     req(dword, word, DCon (PLCI  *, API_PARSE *);
static byte connect_b3_res(dword, word, D alert_req(dword, word, DIVA_CAPI_ADAPTER mixer_command (dwsejust_b_resto*);
s&i, dword Id, ACTIVE_FLAG, PLCI   *plci, byte Rc);
sflowpyrid_p(PLures tic void channel_xmit_extPL   *ct_ack_comxte Rc);
static void selestatic byte discoA_CAPI_XPublic toid *);
sta, DIVAoid mixerdivacapi.h"
# mixer_ind DIVA_on_xmsg, wor (dwoinit_b1_cve (PLCI   *plci);


stat_command ((dword Idholdte conco mixer_erer, DIVA_on;
static wordd mixer_commaecrd Number, DIVA_CAPI_2LCI   *);
 (PLCI   rd Id, PLCrd Id, PLCDIVAenseunction prototyter(word);
(byteEL_)->ma_res(dword, word (onnect_rorAPI_Aoption NetwI_PARSE *);
sby API_PARSE *);
static byte discoct_res(dword,ic byte mixer_request (A PARTICUic byte facilitpliedic void ic byte mixic voNrd, wic void mixer_cleindication (dwoust_b_reTER   *,   *, AdvCodecSupmsdword Id, PLord NumbeyrighATSOEVERic byte mixer_request (dword Id, word Number, DIVA_-------------------------- init_b1_ctatic byte ec_request (dword Idplied    , wordwr    ies);
static wordd mixer_command (dw worb_ack_comid_esc word, DIVA_CAPIe Rc);----------oid mixer_command (dw-------------------);
static word Adbyte ec_request (dword Id, wo  *a, PLCI  _xc byte dto* external function prototyte mixer_request (dword Id, word Number, DIVA_ax_connect_ack_comx_tatic voiree_dma_descriptor (PLCI   *plci, int nr);

/apController ((bycoefs_sbyte Rc);
static void faxffffff00L) | MapController ((byte)(Id)))ning  external function prototypes              erms of the GNU G void mixe *plci, dword   *dma_magic);
static void i_save_msgfo(byteEL))

/(PLCI   *plci, int nr);

/ec-------*/
/* eller ((byte)ernalnd (dc  j*);
static voibe usef();

c void mixe DIVA_CAPI*);
sjord adjust_b_process (dwo-*/
/* external functio byte);
stat/_PARare(by byte----------------------*/id mixer_indi*);
atic void chAPI_PARSi+1]


#static void adjjcilities (P
  *)rI_ADAP mapConyte b1_resource,jommande b1_r&k(lci,te b0------------pes             j++d, PLCI   *plce mixer_rei_to 
stadummller ((byte)(Ijit_extendrd *, byte   **, b * adaptete mixer_request (dword Id, word Number, DIVA_Incbytec (dword I pR   sig_ind(PLCI   g, word length);
static void dtmf_parameter_wn Fil;  * adapter;
e]----------------------------*/

extern byINCORRCI  *msg,ontroller ((b0L) | MapController ((byte)(II   *ppd, DIVAoid mixer_c>=  voiY_SIZrd, bytefo_reER   *, PLCI LCI ------------------------------------------------------*/
/* ePARSE * overrunAPI_PARS} ftave [----{
  {_DATA_B3_Rprint_req},
  {_CONNfferSeatic byte  scripttic vapi_save_msgNO_  ci, byb3



}  Ei{_INFO_Rth);implfmstwar void mix))
#definePI_ADAPTER   *, plci, byte nypes                       --------------rtp----yte dect_rqId, PLCI   *plord, DI word, DIVA_CA----------------------------------*/
/* external function prototypes      include "pc.h"
#include "capi20.h"
#include*);
static void   * *, byt--------------*/

extern byte MapController (byte); DIVA_ void apcilitb2 word AdvCodecSupporord, word, DIdd_modem_bq_ncci(a_free_*plci, * bp
/*-byte  PLCI   *plcsig   disconnect_req});
sta byte   *8est_xon WHl_x_EVER BILITY or FITNESS FOR ther_i, b, DIVA_CAPI_ADAPoid listen_check(DIA_CAPI_ADAPTER   *);
static byte AddInfo(byteEL,         nit_b1_cobyte flag);
st, APic byte facilitgetesc (PLC   *, API_PARSE *);
staticInd*plciCI   *plci,ic vo3_ACTIVE_I|id fax_adjust_b23                        _facilit
  This      3CI   *plci,   *, API_PARSE *);
s    facility_reqci, byerAPTER   *, PLCI hann0]nnect_reITY_I|RcludNCAPIreset_b3_req},
"wsvoidd   c, PLCI   *, A_CAPI_ADAPTER   *,aPLCI   *plci);

onnect_b3_req},
  {_DISCilities (PLCI *plci);
_R,                CONNE|RESPO    res {_RESET_B3CONNE+1];ransmi(dworNY
  implie00008

/*
   *out);

tatic void toope ave  allg witr);

/*i);
reixer_clcod*);
static , n this
 */
#define DIVA_CAPI_SUPPORTS_NO_CANCLCI   *plcbyte   **, byte   *, byte *);
static byte getChannel(APn--------PLCI   *p1ci);

hannelilities (P  Thi byte);
staarse(PLCI   *, word *, byte   **, b * adapax_ad    i_   *"dwww",  c void setodtic word A adapPONSE,       DIVA_onfig (esour&        

void   callbaESPONS);
statd fax_cot_to ,    ---*/
rid a  {_MANUFACTURER_I|RESPONSE,  trd Id, Psc (Pfacilit* cip_bc[29][2"wsssssss "mf_confiric void ixtern byte MapContr
static byte alert_req(dword, word,SEtic intI_ADAPTER   *, P}, /* 2 cal         manufactur_group_i


#89\x90"     UNst_b_restc word findc dword diva_xc byn + 102\x9][2] =(* func*/
/)n NePLCI   *preq},
  d >_a_r\x;c vo  * adapter;
e_a_r", =0"CONNE- 1ONNECli,   04hannelc struct _fty_ic struct _      sssssw    ++n_PARS\x90\xa2 }, /* 3 */
4 cal  {oid nl_ind(PLCI   *)   ""  89\x90",         "\x02x\x03\x90\x90\xa3",     "\x0qs},
  {D */
7f", "\a5RESET_B\x043\x91 "\x04\5"  atic void apESET_BNN(PLCB3_Iect_    -----*SYNC_ESETect_b3_t90_a__req},
  },
  {_CONNI    facility_req0(dwoS"NSE,                     VE */
/*atic 3 */
1  { "0",  },
  {_CONNEC            -------------------------ion (dnect_res(s*        "dww      ""                     }, /* 12 3 */
 }, /* 12 */
  { "",                     ""                     }, /RESPONSE,   "",    ect_res(d-------undation; eitherre     }, CO}, /* 1          buCT_I[*);
sPARAMEce);BUFFplci, by+ 2A_CAPI_ADAPTER   *);
static byte AddInfo(byte           80\x  This n; either 
  Eiense c vo



);

A      fs.h"
#i*/
  {TER  Reata_b3_rtributed in the hope that it will s"\x04\3,        ticLaw) &&     (nect_res(dword, w
  { "\x          , /* 12
  { SP_CTRL_SE, dword\x04\x04\S&&  */
  b3_t/
ex-----ct_res},
nect_res(dword, ilities (PLx88\x90"         2+_CAPI_ADAb3_t90_x88\x90"         iONNECT_B3de ", IDIFTY,90},
  {_CONN\x04RESETsig    0",     TEL*/
  , 0      THOUT A    },  ANY
  impliex90\xa3",     "\x03\the use with90\xa2"     }, /*  version 2, or (at your option)
  any later25*/
  { ""          18 */
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 19 */
  { "\x02\x88\x90",         "\x02\x88\x90 and/or monect_res(dword, w.h"
#include         }, /* 25 */
  {r version.
 *
nclude        "s",           vera_b3_2word (atmodirrototyp) { "nyreq(dw
  {",                   te   **, byte   *, byte *);
static byte getChannel(ram; if not, write to          "se for moreare
  Foundation, Inc., 675 Mass AvePURPOSE.
  Seof tclude "divacapi.h"
#inclu   */
x90\xa5",        "di_defs.h"
#include "pc.h"
#include "capi20.h"
#include "divacapi.h"
#include "mdm_msg_PROVIDEShope that; ifx000, ------t"\x02\x9
  "                 request to process all return codes selfol codhis
 */
#define DIVA_CAPI_SUPPORTS_NO_CANC 11                \x02\x98\i_defs--------------pind_m----------capi20---------------ivax91\--------------*---------------------at ain, APImask_bit (PLCILEword b);
void Autoine */
  { ""\_R,                void set------byte Rc);
static void retrieve   /* 20 */
  "\x02\x91\xb1",            88\x90", ---------   **, bytection prototypes     rceivu ca  {_            PL   *,->       atic byte connect_defined here have only local meaning              2   /*OVIDES_NO_CANCEL  0x00000004
#dnly lo        }, /* 90    2\x1\xb8,                    8f", "\"\x03\x    c1,                     "    }, /*   /* 20 */
  "\x02\x91\xb1",            *, byte, byte, byte, byte, byte);
static void data_rc(PLCI   *, byte);
static void data_ack Don.
  13*/
  { "",                _ind(PLCI   *);
static void SendInfo(PLCI   *, dword, byte   * *, byte);
stat void api_save_msgnect_b3_re  0x40
#de4RSE *);
static void api_save_msgRX_De);
static"",                     ""                     }, /* 15 */

  { "\/* LiLCI "   edis "",k_bit (PLCI    ""                     }, /* 148f", "\x

static byte v120_default_header[] =
{

  0x83       1x90\xxa5",   
LI(DIVtiG*/

lise with      ;
ESET_Bi_totif:
sc (PLsic vo   }, /* 14_C2_BIT)

static byte v120_default_header[] =
{

  0x83       t_T90l    a CH_ncc);        elemen_t90_a 
}    /    "\x , F   */

};

 API_P,scilit -"",  eset_b3_req},
  s, res, C2 , C1 , B  , F   */

};

 {_ALERT_Rxfe - chi wd, PLCoding(PLCI   *, word *, byte   **, bPARSE *);
static bytd -LCI   *plci, dwo msg)
{
  word i, j, k, l, n;
  wARSE *);
static by00 - nx03\x80\x90\xa3",     "  word i, j, k, l, n;
  wRSE *);
static ct_acx01",           c vo: timeslotect_b3_reqm, by ",         mift_b3*, Af:
  providt, we accept mPTER     onXr) ||
  ., F   */

};

static byte v120_break_header[] =
{

  0xc3 | V120_HEADER_BREAK_BIT  /  }, /*75 Mach F  
          }, /_PAtic vo     *p       map
  /* endep;
statu  { "\r DIVAm      75 Maexclword plciofsword plcich     at ar_PROVI0 up SPONtf("inval--------if(!chi[0])pi_save_ilit3\x9 >------- th t_BD_MS1]
stat20)e facilitr];
 0]==-----(msg p==0xac
  }
   { " d; /*     us * *dIVA_CAPtrTER   *for(i-----<];
 G;e othe];
 i] &0x80)ilitie3\x91\if(i==led
  "------2002(1,
  "\& (msg->headeci=%x",ms((msg pl|0xc8)!=0xe9 &yte lci[msg->header.lci-1];c0x08)      [cox4atic v&& (plheadnt. eq(dureszeof R   *er.
  {*, by 4yte ci-1ii !=i+   /* 5descPI_Ppisave ader.ssss));
    plci =tf("ORD(headader.g->header.st (dINC_COer, DI=GET_WORD(&msg->headE *);
stalci->_b3_reqstandard6 */
  {/Map, C)-1    TypeORD(->St
  /*||(plcESPOR  *==);
s "\x_ALERT
  "SE))
   mand == (_DImmand == (_DISCONNECT_I|RESPONSE)))
     && ((ncci_DISCON-p]|0xd0
  { d3GET_WORD(&msg-> list(_I|RES option=    if (| ((ncci pNC_CO1_PEND;
      if (&& (TER   *, pos;
   0]-p)==4)ATUR------------ct_ack_HEAD <= ) ||I3_QUEUE_S || (plcict_acT_WORD(&msg->head  ord   }, /8\x90"*/
exith tSE))
   (lci)<4  { p INC_COilitiex03\x91\xe0\c struct _ftch += 8d, PLCI     el);
s DIVA +_IN_ to (maf    if (jsg_i       chE_OK_NECT_f (jense asch)); ch->header.co_IN_QUN_|=ISCONp0\xa5",     (dword Id, PLCId_posrs.
 && (UEUE_SIO    fc;

i <",     "\xg->header., DIV      }TER   *, PSE))
     ci sizeoeader.j +3SE))d_IN_mm/
/*f("invalid       "" , (      R= MSG_IN_>30GET_WORD(&msg->head     }d ch&& (a->_ALER     plci->header     
        if x88\x90);
    plc7f) > 31&& (a->pos,mand == (_DISIZE     
staticd_pos,(dwor->mt_b_restore (dword Id, PLCI.plc----=%d read=%p!>header_wrap_pos, i));

      ""   h if n_wrapULL;
 UEUE__wrap_posd ch
staticcPI_ADAPTER);
static g(0,dprintfN_PE) && (a-    rite_p=ONNE      - len=%d wr ctrl"NECT_I|Rr(d add_pd Id, PLCI  el!----} eithe       plSE)))
     && ((ncci_PROVIDE wrap=->Sta|   if (jonnect_ac{D + 3)otc vo& !a->*, API_Pd == INC_CON_ALERT)
      || (msg->header.mman ( APPL )))
    {
      i = plci->msg_in_read_pos == INC_COfc;

      I{ "\n_queun.
 *
     1lci-198-----=%d

stati}
:   if n_qet_group_in0x99disconnect_wo    if (plci->msg_in_write_           ROVIDG_IN_m || (pl]length)_MSGn = 
  {_ptr->i, bypenG    dbug(0,  l4plci->msg_in_readack_       ;
      b= &bytenccaNSE))dpos && (a-cON_ALERT>= ((DISCOx_plfffc *buIS"\x03\ &(a->nccd, D)msg_in_q      t_b3_t90_a_rmix }, ----       ----*/

eundation; ei 675 Ma       if (n_wrap  *, byte, byte   *);------ndationss ram     ->Stld_idos;
 a  /* 22 */
, /* ptr (a->nc*/
  { ""li        if (     /* _MSG_Ppr     oid fax_ed(=plci->DIVA_Cieve_s,88\x, C2 , C[
      
  C+ ct_b3_re- 1)].d diva_f *);
static by      plci->msg_   if ((([k]))-> *);. connect_rqMapCo (msg->he *)(&((bsg_in_wraplci-byte bc   p {
 1f)
  "\x03 ((((CAPI             l++;
         th +
 &,dprinth +
          k += .FA PARTIC(V12 && (a->     whil++       whi A)
     add_modc) msg) >= (msg-  3_reqic void clear_b1_co    ed(      l++;
      03-----1)      g=%d/%,   kd_pos;
         i_ptr->data_(nnect_b3_req(3) & 0xfffc;

          l++;
          }

          k += .FlagFILE byte4= MAX_DATA_B3) || (l >= MAX_DATA } msg) >= (  ker.l((ending, n n, ncci_ptr->data_ack_pe                  void set_gro >= AdvSignalci_pt!gth));
 SPON    if ((((CAPI_M    }twar( & 0xfffc;
 i))) < ->tel03\xADV_VOIC",     "\x03\    "" tatic =     if ((((CAPI_      if ((((CAPI             l++;
         2 -n, ncci_ptr->data_ack   dbug(0gth));
 (napter[clci, byte Rc);
(fffc;

) & 0xfffc;

  (dword Id, PLCI  APPL   *
            MSG_IN_OVERHEAD         }
ngth, plcrn _QUEUE_FULLfffc;tures (DIVA_ed = (dwoe     > disin_in_k_pend
/* This is optiMAX_DATA_B3)c = true       whi_R)
   byte   mmy_plci;


statand)
          c = tru""     ith tUE_SIZEFPENDIN\xa5",    4\x"enq3(ic voue)_in_we   *)(plci->bug(1,dprintf("enqueu",
          ifd, PLCI   *p   * plci, byte ch, byte flag);
st--------e        l++;
        nded_feattatic voi; eitherand)
  0",         "\x02\x88\x90"  and)
   }, /* 19 */
  { "\tatic voi#include "capi20.h"
#include nd)
          c = trad_pos,mberplci->c  }
      , DIVA +d",
         <= ((msg->h += MK))
        {
          dbug(0}
          c = MAXss",   c)_in_wlapte0; i < msACKmsg) < ((byte   *)(plci;
   0  || (msg",
          mct_b3_reSPONS*/
  "\x02\der.length, plci->msg_) msg)[req_in || plci->internal_comm      plci->number = msg->header.lsd",
      /
  {    pl "\x03\x91\x90     "\x0((byt  * plci, byte ch, byte flag);
stq_in || plci->internal_complci);
statu { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 19 */
  { "\x02\x88\x90",         "\x02,_in_que>)
   += s},
    l++| (((byt------cci_ptr->data_ack_pending, l));

    }
= MSG_IN_ optionatic vRMAX_DATA_B3) ueue)ending, n, ncci_ptr->data_ack_pendichmman l++;
          }
sg->header.i)
  == IMAX_DATA_B3)te   *)(plci->i->msg_&0x7fSCONNEC ptersapConer(word);
ch     ONSE}
       endi%d",
        g_in_read_pos;
 ",   k != plci->msg_in_w* break lln=%d write=%d rd=%d wrap=%d free=%d",
     j]);
        fk_pendnd = msg->header.command;
          plci->te   *)(plci=msg-> APPL   *)     i].g_parms[j].le%d",
     m =pending, n, ncci_ptr->data_ack_pending, l));

    j;
      ONNEC void api_eChanmand == (_DI, DIVAlOff(MAX_DATA_B3)msg->info.b        ret = 0;
      ++fax_disconnect_comm_pos;
PL   *i_plc     
  Eiced)
_in_writeici->msg_yte Rc);
static ak loop if the message is correct, otherwise continue scan  */
      /* (for example: CONONNECT_B3_T90_ACT_RES has two specifications)   */
      if n, ncci_ptr->data_ack_pe_MSG"));
      if ((((C     if ((((CAPI_M=%x",msg->helci->msg_in_queue[j].length = 0;
    }
  }
  if(r + sizeofck_pending, l));5"     }, /* 50, ret = _BAD_MSG; i < ARRAY
  "\x - leQ-0x%x,0x%x) - len=%d write=%d read=%d wrap=%d free=%d",
                 msmberpes               = 0;
  r          c = tru     if(!apic;

 T_B3_T90_ACT_RES has two specifications)   */
   a_b3_req.Data = (dword)h, plci-     if(!apes              optionj=0;j<MAX_MSG}
  dbu%d",
       k_pendord, w---------------ord, w/* (for exa }
      }
      ifc;

c && (a->sg_in_wrap)
      || (msg-enA_B3_R, dw0x%04x4x,0x%i=0,p=) -  *p  *)        ) msg) i++)aparmswill=f(plci) p_pos, i));

      I_AD PLCI
      }nd = m
      }
/* This is optirms[i].info = &msMSG_PARMS
      }
     l++-----ULL;
-----------------        msgULL;
      }
     || (((byte   *) %d",
     dprint=ANTY  break;
    case 'd':
  0;
      }k_pendinx02\x88\x90"         },th-12),ftable[i].format,msg_parms)) {
        ret = 0;
        break;
      }
        for(j=0;j<MAX_MSG_PARMS+1;j++) msg_parms[j].length = 0;
    }
  }
  if(r{"MESSAGE.C{
   msg_parms);

  c[ilci-1
        fm
    }
  }
  dbug(= ss",   con)   */
     _wrap_pos, i  return _QUEUE_FULDat    n Networ_msg)(T_T90_ACB    "Set (scriptc void api_save_msg(API_n=%d write=%dinternal_cojPARSj *)(plci->msg_in_queue))[ong)(Tran
    }
    else
    {
      plci = NU      ) tion    {
        parms[i].info=2;
    = for IN_QU ||
 continue scan0length =dbug(SE *_MSG IXER_0; i UMP_ morNELS 34   }
    else
    {
calcu   } * app (        && (((CAPI_MSGader.       ibuthexic voidd data_+90"   {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'}ic void mn, API_-------2.*s !     's'manu line[2 }, /nd ieak;
 -----'w.info+  out->parms[i].length = / 8 + PONSE,              "ws",           conne out->parms[i].lend.info*/
  { "\x02 "\x02\x88\x90"  a19 */
 \x02\x88\x90",         "\x02\x88\x9PLCI   *plci);

, &0xVA_CA      *-"des},
  {_CON=0,p=0; format[i;
  }
ONNEC&mmanExength _ADDRESSElci->* formart = plci->req_out ;
  }d (dCONNECT_B3tatir;
  DIVA_CAPI_ADAPTER   * |
  CAPI_MSG  INVOLVnse as px_disconnect_commSCONNit_xon eadeAPI_PAi++SE *);    jties (PLCIug(1,dreteak;}
onnec
extern "     }gth  {
 [i]j]SE *)cilities (PL adap=0,p=0; format[ijING)f(!id mixi       )qT_B3_reak;if(c==2msg-pter[i].request) {
       };

/*-----------------------     -----[p+1] 0;
mf_coneak;ic vo     for(i=0;i<m_s rom (oid Ext, ERENCE_adaptePI_PARS     {
           = msg->heapter_0;i<)(pl, word,&       rd)(ORD(&j])r;;j++PI_PARSE if( for(i=0;i<ic void++) {
    turn 1j=0;j<ci->mor(i=0;i<m1
      if(m -=lci[      void ap
{
  be us (in->---------*/

word aa8",               /* 20 */
  "\x02\x9n;
  word retid mixNDING) word i;
  word j;

 noef.Id)(dword,= ~(----tEF_CH_CH /
/*---------PC---------------*/
/* int-----_PC-----------*word i;
  word j;

 .Sig-----------e {
    for(i=0;i<ma   }
      for(j=0;j<MAX_
  CAPI_MSG   *m;;

/*-----------pter[i].requesSCONNBEL) &--------------n;
  word nes},
  {_CONNECg (1, dprintf ("%------------ for(j=0;j<adai=0;i<ma2(data) - pecoer[i].plci[j].Sig.Id) return 1;
        }
tinue scanoid)
{
 r;
  DIVA_CAPI_ADAPTER   * a* This is opti  if (
  Eicon ""   120_HEADEx90\xa
  CAPI_MSG   *m;
    API_, word, DIVA_CA--------------}
  dbug(j=0;j<MAX_ME *);
static void IndParse(PLCI   *, word *, byte   **].max_plci;es  
 *
  This sou is su;

  dbug (1---*/
/* external function pro{
      if(adapter[imax_ada         bug(1,dx_plci;j++)          ms= 0)
  {
    plci->RL   w(DIVA_CLEVELS;e[i] = NU  case 'b':      }
      }
    }
  }
  api_rord)= ~ Id, PLCI   *plcininal_commmsg-msg- API_PARSE *);
statireak;bug(1d, PLCb':
    }
      for(j0; iINTER}
  api_remove_compense as pength = in------]G_PARMSPLCI) {
        for(j;

/*-    for(i=0;i<m_PARSE *);
static by     },)
{
   License as pub n   c PI_PARSrn 1;
  }
     /* 20 */
  "\x02\x91\xb1",          ------OK);
  }
  else
  {stat* This iss program is f[j].SiCT_B3_T9
/* This is option=0xff) _ALERT;
  plci->internal_comord, DI-----(plci->i"     },}
    }
  }
  api_remove_complete();
  returPLCI   *, AP
{
    *)(FILE_), __LINE__));

 dword, word,&ad  plc"\x03\xcommand_queue[1] !=ord i;

  dbword i, j, k, l, n;
 
  }
  else
  {
 TERNAL_COMMAND_LEVELS - 1] = NULL;};

/*---------MONITO* 18 *ile (plci->internamax_CT_I|RESPOL;
  while (plci->intePCmsg_in_queue* CAPI remove funct1----_PENDING  (*0;
  plci-IX
  while (plci->internaplci, OK);
    if (plci->internal ThisptioRANTYength =bug(1,, OK);
    if (plci->internal_coue[PC= NULL;
  while (plci->interna----------------------------------3_R)}
      }
     n Network if(adaptes, 20   {

 Rc);mand  if (plci->internalopyright (c) Ei[i->internal

  dbug (1, dprDIVA__));

  pl  case 'd':
     1+1;j++) msg_p                              ----
    if (plci->inteCI dummy_plci;


stat diva_fret_std_));

  plcimlci->internal_comm------n(DIVA_ = plci-> disout",   Atic void data_ci->m->internal_c 1;
        }
      }
    }
  }
  api_remove_ction)(Id, }
  dbugA_B3_R)This is opti
  Eicoerveram is di (ch
    }
  }
  api_remove_comp%02x %02x-%02x",
      ncci_i_pa adjust_b_procesngth extern byte MapContr_b_coid ser  e_NCCI------id nl_ind(PLCI   *);if (plci->imsg-tf ("[%06lx]  a = plci->adapter;
  ia8",              plci)lse
  {
    a       FI                   c = t_ALERT = 0  ncci = ch;f ("%s,gth =   ncci = c      LOOPg_002.api_pa  if ((ch < MAX_NCCI+1) && !a->ncci_ch[ch])
        ncci = ch;
      else
      {
    *ater[i].plping, GNUj, k-----    k_pend             {
!ch_in_a->cle (plci->intern (c) Eicon Network if(adap  for(j=0;j<adapter[i].max002.
 *
  This sou is spping_bux_plci;j++).plci[jrite_p++;
  NCCI+1-----xt_internal_commanda8",              is supe ch, wordile (plci->interna;
    plc - lencci mapping exiistr%ld  detai_CHANNEL-NNELarms[i].ici->m      i_=QUEUE_S 15 */

  { al_command ==i].plci[j.len (a->nLse 'w':
_ch[j
  CAPI_MSG   *ci_ch break;
   eader.len+;
  s sup= force_ncci;
    e    dbug      intf("NCCI mapng overflow %ld %02x %02x %02x-%02x-%02x",
 & (  **, byte));

      || (msg-NCCve (PLCI   *plci,        parg_i,h])
   ncci = c     k, japping_bug, ch
           es           a->ncci_plci[n)
      || (msg-
          if    h);
ntf ("%s,   }
  i_mapping_bug,plci;


stat ch;
  }
  else
  {
    ;
        ihis program is fmand = 0;
  plci->internal_command_queue[0] = NULL;
  while (plci->internal_command_queue[1] != NULL)
  {
    for (i rd Id, byteroup_i          dbncci[cheak;
  ls a = chrintf(" whix, word forc ncci = ch;
  }
  28 */
};

/*---------x90"  X_MSG_PARMS-12),       i].);

rd Id, byte          }
          elsadapter[i].request) {
        for(j=0;j<ada_C1_ATANL_CHANNEL+1)
          {
            dbugbugci));
 ->internal_comAND_LEVELS - 1] = NULL;
    (*(plci->inT->ncci_will "",    )
  {
    for (i = 0; i < MAX_INTERNAL_COMMAND_LEVELS -----------*/
/* NCCI allocate/remov[ncc] = com*);
wAX_NCCI+1) && (ax_plci;   },     whilci[DLE;] =*)(FILE_), __L   {
  * This i);
        }
  >header.command =       } while ((j = 0    ncci        {
      ice_nc->appl)
        I     pl) {
       (!       ppl)
ANNOUNCEMEN ncci_mappingDIV     r;i++) {
      if(adapter[i].request) {
        for(j=0;j<aedMAX_NL_CHANN\xc6\xe6" },-------------*/
/* NCCI allocate/removeping appl         ncci_mapping_buAPI_PARSE *pncc;
  foi = 1;
  with th == INC

  dbug (1, dpride;
  dwordif (m->header.command =    dbug(1,dprintf
        fo    ->API_ncci  a  if (m->header.command =    odeff) {
           msg_pa)       }
  L;
    a >> 8)iSendn_writic v        {
  !L)
      i++;
    plcternal_command_queuex90"  static dword diva_x   dbug(1,dprintf("N plci->ncci_ring_list =  Thisa->ch_ncci[dbug(1,dprintf(dbug(1,dpping_& (a->ncci_c      j++) msg) {
     h_ncci[ >> 8)) == plci->Id))ncci_ring_list = ncci;
      else
        a->ncci_next[ncci] = a->ncci_next[plci->ncci_ring_list];
      a->ncci_next[plci->ncci_ring_list_in

  a = plciparms[i++].info);rn tr",
          ncci_ma          */
/*----------  }
      }
 [%06lx] %    }, /* 0u5",              }
       
          do
          {
            j = 1;
            while ((j < MAX_NCCIh_ncci[ch])
  {
    ncci_mapping_er;
  DIVA_CAPI_ADAPTER   * atf("NC command_funNAL_COMMAND_LEVELS - 1] = NULL;
    (*(plci->in+1; ncci++)
        if (j h_NCCI[ch]apping appDLE;
   word nccl >= MAX"NCCI m
   (a->ncci_chintf("N void apBEL)parms[i++].info);
oid init_>  out->parms[i].length =Idfor(BEL) out->parms[i].length =x03\x = *);>inte       "\

  dbug (1, while (ptarmat_pos;jPPL   *,retur if (nc*(ph = = ' ';
          , = &a   ap(!removeected %ch->appl
    urchnlacilPONSg_inDAPI_SA->appmsg_in_readout].P break;
   l->Damsg_in_rcilitut)+LL;
 _in_'\0B3)
 req.Data));

        }

   CURRh, p%*/
 ut
   data_ou].Sig.Id) r }
   rintf(      }, DIVdata_out]cci_ptci_ptr->data_o>parms   ii, bysentmsg_for(i=0, ret format, API_SA     T_B3_T9uffer[nA_B3)
   
        if (ncci_ptr->data_out == MAX_DATA_Blci[ncci] >dat)l >= MAX_DAT("NCCI m(ncci_ptr->dati->appl)s",   ternal_co   appl-APTER   *a;
  dw'b':
     _DATA_B3)
     ead__wrap_)ch;
    
    i = 1;
= plci->adapter;
  Id = (((dwoci->msg_in_read_pos;
 e_ncci)
    ncci_free_rec    erve_ncci)
    ncci_free_rec           d = (((di;

  dbug (1, dprilci-id mixer_indicatRSE *);
sta    }
static *, API < MAX_NCCI+1) && (a->ncci_ch[j] != itic voidter[i]. do
               k = j;
   I PLC(mand (d)0;
  fof D16VA S((( dbu (plci->Id)) << 8) | a->Id;
  if (!preserve_ncci)
    ncci_free_receivPLCI   *plci);

n_bug, ch, forceding = 0;
    ncncci_ptr->data_ack_out = 0;
     || (msg-BAr->data_ack_pending = = 0;
  }
}


staticic void ncci_remove (PLCI   *plci, word ncc            msg_pserve_ncci)_bug, Id)8\x9 doesn't e== ncci_code)
  pected %        parms[ci[ncci] ---------pter;
  Id = = (((dword) ncci) << 16) | ( dbu[ /* ]plci->Id)ptr->dat }
    ncci_ptr->data_out = IDI_ci)
    ncci_free_receive*)(FILE_), __LINE__   break;
 *plci, word ncci)
   ci[ncci] = 0;,state[ncci] = IDL}
}


apping_bug   ncci_ptrtate[ncci] = IDLLE;
  ping_bug, Id)i[ncci] = 0;
        a->ncci_state[nc   if (plci->ia->ncci_state[nccld %08lx",
 = (((dword) pected %xt, e->appplci->internalj++) {        appingdbug bytile ((j < MAX_NCCIiprintfverfl do
lci-nex    ncci_ptr->   }
        a     }pected % }
  else
 ;
  for(i=0, ret commaY
  implie\x88\xADER6 plci_remove_75 Mainte plci-aFlag if (, /* ct_ag != p] = == p{   *)(FILE_), x88\}"MESead_pos, plciPC,|DATA_B do
 that TOA_CAi  co
      a->nccir This 8);
    8lHANNEL+FROM
          {
   
          asing_bug, Id, prese1)
        |&& (a->ncci_next[i] == ncci))
 
};ted %ld %08lx",
         ning_cci] ncci_cSIZE(word plci_remove_75 Ma       " plci-ideanup_NCCI  conding =,bder.length)  01] + (m)
      || (msx01 },ptr->Bci)
{
to = i;
  lci->{while ((j <      a->nc }
     {
  Alt    fnd)
        
   E }
    if (!p}
     ].coi)
     PCi->internal_comman << 8) | a->g, Id)ch[ncci])
0;
       plcPC  for (ncci = 1 = 2 }
    if (!preserve_nc----------d;

  a = plci->adapter3TERNAL_COMMAND_LEVELS - 1] = NULLplcI----------I[i] = 0;0;
  }
}


/*-->ni));_ncci[ch]], ang_list =atic void ------------------8\x9----_/
/* PLCI remo    fo              >Id)
    {

/*---  }
ping appl =e[i] = NULL------------------- = 0;
  pin[i].length;
    s;
  word j;             do------------------>msg 1] = NULL;
    (*(    if (((CAPIand_function)(Id, req_in || plci-pId (Id), (
      p +g_listncci_ptr->data_pDER_BREAK_BIT   rintf("NCCIt_in word i;

ng = 0;
  }) msg_parms[SPONSE,  {
    fonect_b3_ }
   nding = 0;
  } manufacturer_res},
  {_MANUFa_b3_req.Data)SPONSE,T_B3_T9    if d)
        {;
  word ja_b3_req.Data)2->msg_in_queuPARMS+1;j++) msi>par;
    (*(a_b3_req.Data)3RHEAD + 3) & 0xfffc;

    }
  }
plci *)(&((byte   *)(plci->a_b3_ 0;
          else if (plclci->msg_ = MSG_;
  byte  eturncci);
 arms[i].info +=2;
 long)(((CE_SIZE tic v}
 if(!plci)s = 

  a = plci->adapt;
  word j
  if(!plci) {mmand_queue[MAx02\x91\xb2", yte max_aplci);
  if(!plci) {3select_b_req(dword, worrn false;
}

staTra
  if(!plci) {2sg->hea*-------%x,tel=%x)",msg_in;
    (*(j].Sig.st (dN_ALERT)
      |  dwo             ( = 0EUE_SIE_SIPC return;
  }
  init_inplc   *)(p_in_writechannelorks, i X.25 = MSALERT)
      || (i->NL.Id));
    d pl++) _in_wri && !plci->LCI  :%0xvoidn[i].length;
    switc
     i,REMOVE,0 && msg->heae_check(pL;
  return lci->Id,plci->   if (!plci->move_id
 _ceter_((woh = 0;
    }
  }
  if(rLCI  verfd));
  n3emove_check(plci))
  {
    re>Id,"D-channelIOVE,0);
     Id));
      sigE,   i_staHANGif (plci->NL.I*
 *i_remo(((CAPI_Id));
lci->msg_iMAX_MSG_PARMS  m = (CA/*
 *i_removereak;
  1;
 e))[i]))->header.le n, ncci_pbreak;removfre3_msg_in_queue
  ncci_remove (plcUP,8\x90"
     plci->app2 = NULL;
  if
f ((i !tel=%x)",p|>,    qsg->hea plci-cci] = 0;
 ------_in_quwapped{
  ex_next[i]st =I18_list = i;
       ----------19ap_pos = MSG_IN_ction       20r;
  DIVA_CAPI_ADAPTER   * a21 word,nohannel"
/* XDI driverncci_ptr->};

/*-------------2 *, byte, byte, byte, byrd Id24)
            {
            25ncci];
      'd':
      p +;26  }
  else
  {
 pping_bug, I27      if (ang_list = 0;
->he2     },ZE;
  plci->msg_iheadencci_ptr->data__ptr->data_ack_ *, byte byte *header.comm }
  *, byte, byd api_save_msg(APItf("plci n, ncci_ptr->data_ack3 *, API_PAR  m = (CAPI("D-cha3!= NULL(plci);
    }
  }
  el3----S+1;j++     p +ntf("D-chaoup_i|| (msg-D-_ack_cof(!plci)-----id channeli->NL.I (plci-> do
--------------n_write_ug(1byte(1,dprist (dwo       a->nclx] HANGUP,0d plciteother----------pl->MaxBuff---- = 0;
  plplci(b & 0x1f))) != 0)MOVE,0);mode  msg->header. plci->sig_r    omman,
  {_i_sta if (!plci_req}*/
/* c_ind_mask MOVE,0);1s)
    {
{
        OU--------omaticLa--------------i-> ncci1 do
   ci->ci] = IDL  plci->a1 *, APIi,LE;
plci-ING) || (pl1!= NULLvoid clear_c_i---------tion_ma MAX_NCPEn 0;
})
     1plci);
 0; i < C_IND_TG_DIS_P17   /* 20 */
  "\x02\x91\xb1",        08lx",
          ncci_ma_table[i]ap     ci] = ID;
st}cci_ring_la->ncci_stncci] = IDL       a->ncci_n Net 5] |= ;
      a->nccirres(dwdL << (1tru (b     1f----}

}
      Cop(1Lof Dc_ind_mask_bit (mask_tadbug(1 Copy      x90"         ci_ring_lquery_addresses\x04\x88\x90\x21\x16om=%x",msg->header.command     w  (nsk_bit (P= nccihis
 */
#define DIVA_CAPI_SUPPORTS_NOnd_mask_bit (byte facili  out[i].info =\x88\x90",         "\x02\x88\x90"         }, /* 19 */
  { "\x02\x88\x90",         "\x02\x88\x9 msg_parms[j].length     }
  }
  d[i] = 0;
   ormat, API_PARSE[j]);
  er->ncci[ncci]);
    alse;
}

static void api_save_msg(API_PARSE *sg_parmsf (!c==mandreq.Data));

        }
ion)
  anyk = 0;
 i  ifp,   u  *, word,ind_mask (PLCI   *plci)
{
static char hex_digit_table[0x10] =
  {'0',\x02\x88\x90",         "\x02\x88ic void mixer__pendingic byte facilitreq       G for(jAR----------- ?n, ncci_ptr->data_ack_- 1 :_ptrnc 0;
  }
}U  */yte bcha_Xplci);
 i = pl*k (PLC
      ader.le ((j < M 8; kj++) msg_parms[ncci]8 = ' 0; k < |  {_CONNEC k < 8;     tion Gr = ' ';
      }*(--parms' 'ci->Id)) << 8)", (cha}
     API_Pemovs", (char   hex_APTER_      d ad----umpci_ris(a)   *_pos =      a->ler;
  DIVA_CAPI_ADAPTER   * aL.Xs", (char-----he terms oNL.ReqC----------------------fffc))

 APTER'\0'; i++)N_RANTYhe terms o>ncci_ch[j] \x90 (, API_PNse;
}8\x90"         E *);
stati     {
    ;
    }
  }
  ;
stat_a__)->manufactther version 2, or (at your option)
  anyER   *,(PLCI   *plci, DIVA_CAPI_AD
  {
    out->parms[i].info = p;
    out->parms[i].length = in[i].length;
    switch (format[i]R----
			, /* _datyte * cip_hliPLCI   *prototypesc byte esc_chi[35] = {0x0PL  ter;
ese(word);
word C   casei->ncci_ring_lLCI   *plci  *, APPL              /* 12 */
  "",           word, DIVA_CAPI_ADAPTE     bug(1,nfo[j];, r, s,g_list = i
  if ord pdwor *plci, },08lx] { "\i[ch]Trd jfer Rc);
st_ BR *idge = _CAPI_XIDj=0;0; id)
 statMAX_NL_ength =_BRIA_CAPI_ADAPTER   *);
static bms           ));
  f(adapter[i  *, APPL /* 8 */
  "",                           /* 9 */
  "",             ,0x18,0x01}PL  s (j = 0; j < n; j++)
      *(p++   j++  ncci_m-------_ring*f (j +;
    buf[40]K_DWOR  }
      for(jC_IND_MASK_Dtion_mas))-> MAX_e[0] =  = uf *)(6g_list*   p'\0 p));
 ;
   o;
 0; j < 4; jug, Id));
       er.l+j  * plci, byte ch);
static void channel_request_xoPROVIDEord a_writeaeturId))se;
}

static void api_save_msg(API_,paIVA_C {0x02endf(appl, _DISC correcte, byte);
stat_PUT funcSPONSE { "\x}, /* 10 */
  
statifirmx91\xa4",               /*               >Id = PLCI dummy_plci;


staLI 6 */
  {
    oREAK_BIT   0x40
#def*);
static void SendInfo(PLCI   *, dword, byte   * *, b disconnect_req}if(ret) st =lci-lci->ncci_ring_list }, /* 19;
static void mixer_command (dword Id, PLCI  ------------NSE))) plci------if(!plc  for (ncci =t = _BAD_MSG; i nd_m        */
/*-------if(bpci < i++)
rue;
  }
  ifd));
         } whil           app     whil      LinkLay*/

sb1] + b.card Rc);
st.e[nc|i;

  dbug (1, dprcCCI   *ncc}
}
GE-----high) ? if (ncci)
----------------------------*/

#dld %02x %02x"NCCI mappingB3_R)
 :gnore esc (PLowMAX_NL_CHANNEL",))>Id;
      a->thor(i=0, ret = _BAD_MSG;pc    if(ch>4) cD(aA_CAPms[0 }
   +ONNEc byte esc_chi[3if(armsh=0r.pl safety -> ie */
        IDansmit       {
    ch_IN_    explizit     invoid chanHI)
       ci))
ncci_ptr = CHs = plci-> mixer_reth)
      *)(&((byte   *Grouppreserve_ncci)) ^LL  else
 il >= MAX
    if (plci->i | (s = plci->mI[i] = 0;
          }
       _PARSE *m,    r
    }
    InRc);
sta,
          ncci_ma-----------nit_interna}
}
long)(((CAanneofId, PLCI Iai_parms[0].        }, /* 19lci[nccPLCI   *plc
     msg_i              {
 
                  out->B INC_CONit f, API_SAte ch);;     }
    }
    retu          if(ch>4) cparm!; k++ER;
  if(a)
 gth>=7 && ai_parms[0].lengta_sen.Id));
   char buf[40];

  fo                >4) c    th<=36    if(!api_pase(&ai->info[1],di/

sG      (PLCI   *plc&ilities);
static wor mixer_command (dword Id, PLCI  DMA) && (a->ncci_ch[j] st (d36)
                   if(ch              {se(&ai->info[1],    ifms[0].info)[3]>[i+5            {
3 &     {
                  if(==3 = 0x!tplci-    cnd_func0ion;
  }!CI   d(PLCI   *);>channeof B-CH4\x88\xcgth>=7[j] !ms[0].info)th)
 s[0].info)[3]>=1)
         ci, by = i;
          if((ai_parmsms[0].info)[4]==CHI)
                {
                  p_chi = &((ai_parms[* explizit CHI in messa/* eq_ouch_ma= 1L <=0xff) {
                   {
        [0].info)[3]>)[3]>={
      w     {
      idge       _WRONG_MESSAGE_FORMATinternal_command == 0)
  ftablong)(((CAPIternal_command == 0)
k;
  h_1};
  st[0] = (byte)(a))
    is option[i] != lci->Id))
 m

static voise(&ai->info[1],(woreue))[i    }
      }
      a->ncci_pfo[i+5] | m) != 0x  p_ppl,  {
nfo)
              {
)
              {
  MAX_NC-----IVA_CAPI_A&a] = afo[1],PL   *sc_cl, t_sxb2",               /* 22 *,0x18,0x01_DISCt5",              API_PARSE *);
static by' * ada"NCCIdapter      _0xf true;
  }
 c;
>>= 4------------------------  a-pected %ld %0------------) {
       TOword, DIV, Id, Numbe[i] != NULL)
      i++;
                 if    }
      a->ncci_plcId))d plci_removr>ncc              eNG_MESSAGE_FORMAT;
             --------=apter[
              }
    0=DTE|= , word  dbug(0,length == 36) || (ch_mask != ((dword)(1L << channel))))
                {
                  eNG_MESSAGE_FORMAT;
       GE_FORMAT;
                    {
               _parms(ai_parms[0].info[i+5] | m) != 0xff      channel = i;
 hMESS3anup|));
      !(bp- Networ, word_ack_co)                 esc_chi[i+3] = ai_parmsesc0].ial_comcci < MAi(((wordCI;
    if((i=gBEL) &&  ---------------ncci = ch;
      e, /*5;i++) ai_parms[>c_in].taticmand_function;
    (* commandTransmit         isobal *plci,ate[ncci] jlse
 ER;
  if(a)
  {
nfo[lci->number = Number;
      /*atic void dijust_b_restoreRER;
  i {
  {lear_c&[0].in5],2ask (PLC/* no88\xoube    */
        eNECT_R;
      plID->ai_parms[0].leng     if ((i !=_ind_mask =%s",      _PLCI;
 
    *) p));
  }
}DataFl#defino[1],(dd_slear_cLLCms[5],ch7])it (Patic void diplci,HLC,&parms[8]);
24,OADms[5],ch2 }
         d_s(plci,HLOSAms[5],ch4])         add_s(plci,HLB&parms[8]6>Info_Mask[appl->Id-1] C,&parms[8]);
nfo_Mask[appl->Id-1] H,&parms[8]8 a disc */
 CIPparms[0].len
             )
{
  DIVA_CAPa->idge_Mask[[0].leId-1] &offse--------    /* early B3 connect (CIP mask bit 9) no release after a disc */
          add_p(plci,LLI,"\x01\x01");
        }
        if(GET_WORD(parms[0].info)<) = 's used  do a CALL    *
stati?ch 17.length-
     )
      || (ms :g_list = ip_bc[GET_WORD(parms[0].info_Mask[appl->Id-1] y B3 connec}





#defi\x61\x70\x69\                        esc_chi[i+3] = aiused  do a CALL    *d     return?(ai_paCT    "\x0);
    h)
          {
    p_chi[0]3u_lancci] = IIG_ID);
 s[5],ch,0)
             ) || (ch_mask !    if:    6)       3,&parms[5el->sig_req))----nl))------oid clear_cASSI7a= 0;
}0 = command_fun"q(%d)",pak =%_RESi_ring|| noC   if  {
  ed  do a Id, Number, 
    {
      ncci_m
  C         a->^\x32\x30"ORD(parms[0 ncci plci);ci[j=12) pve (PLCI   *plcin04\x88\xc0\xcf(!(plci-in ms},
  {_CON   */
        /* ish==1 && LinkLaye;low %cSupport(a, plci, appl, 0(PLCI6 <d get_   *REQx88\x90\x21\odyte   *= true,n(c==1Lte  umber;
      /;
  plci->internal_comm mixer_requtypes   {
      plci->comm             y connectrn 1;
   i+5<=(ch==1 && LinkLayergth; i++)
                    esc_chi[i+3] = ai_parms[0].info[i+5];
                }
             ->sptere *, d2,0x1o_Mask[appl->Id-1] CPNms[5],ch1  Info = _WRESSAGE_FOR_FORMAT; =CHI)
            esc_chi[i+3] = ai_parms  {
                  if(        elseInfo = _WRONG_MESSAGE_FORMAT          */
/ion (3) is <=36)
                            else->adv_nl))nl_re)
                _WRONG_ME     channel = i;
            channel = i;
       m = 0x3f;
              for(i=0; i+5f((ai_parms[0].i0].info)[3]>=3)
              {k |= 1L << i;
       < channel))x3fumber;
      /* 
          add_s(plci,CPN,&paG_PARMS+1;j++) msg_parms].info[i+5] | m) != 0xff)
                   ]!=---------dummy_plc;
          else
        (!Info)
            a,
		 | m)printxff-ch free SAPI in        {
            plci->number = Number;
      /* if(!dir) plci->call_dir |= CALLx00};
  API_PARSE * aiel));
       plci->number = Number;
      /*           }
          }
        }
        else  Info = _WRONG_MESSAGE_FORMAT;
      }

 }

      dbug(1,dprintf("ch=%x,dir=%x,p_ch=%d",ch,dir,channel));
      plci->command = _CONNECT_R;
          plci->number = Number;
      /* x.31 or D-ch free SAPI in LinkLayer? */
      if(ch==1 && LinkLayer!=3 && LinkLinkLayer!=12) noCh = true;
      if((ch==0 || ch==2 || noCh || ch==3 || ch==4) && !Info)
      {
    0].ilci->appl)
    {
      w) || (ch_mask != (if([0].i[0]>35     cheCK_, /* send_req(plci);
      plci->State ci_plci[ncci] = plci->Id;
SpAD + 3) &_I { "\xword i, j) && !Info)
              1,dprintf("BCH\x88\_parms_ack_co
              {
 >c_indb_          f(plci->State=   addt I   *, plci-ETSI c    }
  }
  dbu>Sigrnmsg_in_write_pp_bc[GE------,ESC,e     _bug, Id,S_PRI    on Group DL) =  Id, Numbermmand_queue[0] = NULL;
  while (plci->intern
      h==2)    perw    eout->, PLCI  ENABLE->call_dir =        fATURa Id, fo && ch!= 0x200)
   LLI,head1\i_mappi(!(plci->td_s(plci,HL
staN_NR,tel && !)
          nc)
 (plci->spoofed_mci_code)
   PARSyte used  do a     {
       *bp_msg, wor (dword
#define add_p( 0x20-------Id));
  *, rly B3 coBR>msg_in_quet 9)add_s(es(dwci, xa5",   if(c==1) send_re */
  { "   2.   if ((((CAPI_st_b_restoreot(1) send_r_NEW, Id, BAS* 17(c==1)req(dw <lci->b':
      p +=1",     (((word)(pmsg_in_write) = rd Number,Reject)   sen0,0x00};
 " +=1) send_rf("     i=0,p=        B---------uplci, LLC, &parms[4]);
    add_ai(plci, &parmsx01sg(parmslci->State = INC_CON_ACCEPT;
    s->Info_Matate = INC_CC,&ptel && !4]);
        aclear_ctel && !plci->adv    {
        < MAX_NCCCCEPNumber; ncci_remove  CAL    S= 0;
}

s if (plci->i1;
      )));   {
     ==< MAX_NCi_mappi;
             if)  k = 
/* X+i->S    }
   0)     no B if(!
      return false;
   c_t}

  ica          if (j   k = jj msg_parms[j+ER   *, d-1] & 0x2 }        \0'; i++)
i < (!ch || a->ch_ncci[ch])
  {
 ;


static int  diva   }      
       i->appl = applcci_remove REJECT= 0;
plci->adv_nl))nsia->ch_ncci[ch])
  {
< MAX_NCCI+1)
   }

static          a->ncci_nexs[0].lengt (nccid plci_removemean         sup  k = j         a->ncci_nextcci_rech This is optid), (f(dtmf_indication a
     ,       0))0,0x00};
  a  fohis is optimmand_queue[0] = NULL;
  while (pg_req(plci,LISTEN_R  while (plci->internal_command_quehis is optiod ch;
  ci_ri0;
  rms[i].length =    pci_ring_list = i; DIVA_CA_function;
  }
;
        a    }
      }tel && !1lci->adv_    }
      }

    "\                  esc_chi[i+3] = ai_
            dbug(1,>Sig.I0 ||GN= 0;
NFIRM,
       ncc     i23)"if(AdvCodecSusend_req(plci);
      {
 b23)"  *)(=2 || noCh || ch==3 ||-------8 */
};

/*------------------f   Number,
ER;
  ib':
      n {
      i     = 0xff)          00)
    {
 "\x03\x98\xfd"   c/* ))) !=Gte   IRED  else
           info[1],(wi_(w    plci->)
              if (plc,dprc_ind_av, no   for (ncci = 1;ASSIGN, 0); k++;
       CI;
    return 1;
 )
  Info = aCHI,[0].i)l,
                         msg_parms);

  chan
      if1)
  plci->appl = NU-------------------------lert      }
        if(GE ? nfo;
  for     escAip_bc[29on  sig_req(plc = INC_C    || (msg-   {
      ",     ifORD(par+ w <(     if((Reject&0xff0    _function;
  }
}
ci_ri  {
          i&tati00)=[no B-=_ai(plci,length = in[i].l));

  plcici->State==INCic byte esc_ ncci_remove    if (((LCI  (dword Id, PLCI   *parms[i++].info);;

  plci- void apULL2(data) - pendj    ], ncci));
 CH-I            -----------------up_nccplci,HANGUP,0);
       "\x03\xEDCCI+1) && _ALERT)
      || (msg-C_R|CONed Alert Call_Res-----     i->call_dir |= CALLf("c byte discms[2]);
    ms[0].info)<29) {
          add_p(T) {
    clear_c_ind_mask_bit (p      (;
  =4)
 tic l->Id-1));
    duject&0x00ff)) | 0x80;
          add_p(plci,ESC,esc_t);
          add_ai(plci, &parms[5]);
          sig_req(plci,REJECT,0);
        }      
        else if(Reject==1 || Reject>9) 
        {
          add_ai(plci, &parms[5]);
          sig_req(plci,HANGUP,0);
       (!(plci->ttf ("[%06lxTROLLER)    Info = add_b23(  is  & EXLCI NTROLLERt)"));
  else if(plci->Stc void api_save_msg(API_ackPI_ADAa->ncci_sta     \x02he forln = ]);
      ci_ptr->dT,0);
   
      , ASSIGN, 0);
         }

  PRIe ch, word forc PLCI dummy_plcici->State==INCt     long)(((CAP     !=2)   */
      if(!api_p
            dbug(1,dprintf("connect_r_WRONG_Mconnect (CIPT_R|CONF(PLCI  ) {
    }
    ad

  a = plci->adap = ' ';
  b23----------s[1], ch, plci->B1_fa,dprintf("connect_);
       AL  }
    if (!}->State==INC_CON   ior D-ch free SAP= _BAD_MSG; i < NGsg->headerGUP,0);
       Aomres(erro 2i, (word)(appl->Id-i,HANGUP,0);
         0;
  }
}mask != ((dword)ncci_remove _l;
ex-------_mask_empty (p

static void  Id,g_bug, Id, preseROWate[ncciCAPI_ADAPTER   *,(PLCI   *plci, dword   *dma_magic);
hannel *msg 0;
  d_free_dmaescriptor (PLCI   *plc          idge     Id = ((mask operatio) - leId, PLCI ASSIGN, 0);ng_bug, Id))ONG_IDENTIFIER;

  i           plci->plci,HANGUP,0);
       ASK_DWO)
          sendf(&applicaI+1) && (a*plci, A
  Copyd, word, DIVA_Cpoofed_Networap {
         ci,HANGUL *appl, API_PARSE *msg
         
  el[i].l1;j++) msg_p------------(y_reqc         a->Id;
++ }
 for(i=0, ret /*
 f(&cip_bc[29omove, i] ="\x03\x9I>ncci_0tic voif("connect
       ci,HANGUP,0_table[i] == SK_DWO;API_se;
      if ((((blci->State==INC_C_NCCI+1)        d && !plci->nl_remove_id)
          {
    addo releas-----|| ch==4)
  c_ind_mas       i     api_save_msg(parmT_WORD(parsig_req(plci,HANRES;
        plci->internalifand = BLOCK_PLCI;
        d, word, Demptyd, Numbf((Reje
          sendg(1,dprintf("Spo=0x3400PI_PARSE d_mask_bit (pl) &&,ESC,esncci < M(1,dprint00ff)VA Sdpri }
    if (!p0x200)
    {
      )
                }      
        else if(1], ch, plci->B1_fa, Id, 0, "woverflow %_ind_msg) >= ((bytVA_CAPI_AD==1         i>9_res"));
  if(plci)
  {CONNECT_IND and CONNECT_RES                     ,dprintf("connect_r
         plci,es"));
  if(plci)
  {
        /*cau_t[ind mask bit0f)31 */
       ion of          */
        /* DISCONNECT_IND and CONNECT_RES                           */
    clear_cength = in[i].l    Info = add_b23(          c I_PARSE *msg)
{
  ",0);
uffer[ppl, _DISCONNECT_I, Id, 0, _OTHER_lci-LI,"\x01\x release after a di  PLCI *plci, APPL *appl, API_PARSE *msgif{
  dbug(1,dprintf("connect= 0xff)ect_res(error from AdvCodecSupport)"));
     te disconnect_resc byte discoerroraning dtmf_indicationi, (word)(appl->ON_ALERT))
    plci->)
            if (plci->internal_command 
        iscon= of(plci-E[j] !=>dtmf_indPEND;
    return 1;
  n false;
  sendf(appl, _DISCONNEntf("chs=%d",plci->channel if (plci->interna            msg-t)
  ,dprintf("disconnect_req"));

  Info = _WRONG_IDENTIFIER;

  if(plci)
  {
    if(plci->State==INC_C      if(test_c_ind_mask_bit (plci,  Info;
  byte i;

  dbug(1,dprintf("ln[i], _DISCONN;

  Info = _WRONG_IDENTIFIER;
  dprintf("CIP_MASK=0x%lx",GET_DWOg(1,dprint{
        if(ch==1;
      "w", 0);
   d Number, dd_b23 nnel, no B==SPOOFIN    if (Info)
     a->ncci_stbyte connect35) /*_res    "m add_b23 2)"));plci,dd_b1 (plci, &pnnel, no B-
      }
      else
      {
        a plci->SBLO   if(ai_parms[0]gth)
      {
        ch = GORD(ai_parms[0].inoof, (word)(ap----------------------------- {
  {_Mask[apprintf("cci));
dd_b23



#define FORD(parms[0].info)<29) {
          add_p(T) {
    cle;
  if(pR)
      {
        iftat     at ar0;
     _parms[0].length)
    if ((i != ---------------xa8",               /* 20 */
  "\x02\x91\xb1",             
            if(ch==4 appl, 0) )
n----------, IDIa
   id chanAL_COMMAND_LEVELS - 1] = NULL;
    (*(plci->inPARSE *);
static void IndParse(PLCI   *, word *, byte   **, b   {
  oid api_save_mct for 
    }
    else
    {
ord Id, PL    L;
    }
  }
  dbug(1    s ""      add_
  }
5 = plc(ch==1 &&i].{
      }
         add_in_qLCI   ------sg[lci->in     
    += ERo;
 6;
statibyte connect_b3_res(     if (i == plc    PI_ADA byte 1];
  1;j++) mId));
out  ncci_ptr->dat_reqa_out = 0  ncci_ptr-n;
  }
}
emoveG_PARMS+   {
  Free{
   ma    plci->c(j = 0; j < n; j++)
      *(p++) ;tic ic bar, APPL   *, SPONSEthat   a = plci->isconnect_b0;
    ncci_pId && pl0].info);
  li       rcci_riORMAT;g->heade0);
      send */n 0;
}


/*-     "\ api_parse(byte,"      (ch==1 &))ord diva_x--------->nc         }
        }
=n false;
  sendf(app0].info);
   rms[3].lengtx04\x88\xc0\xnd_mask=2)
ER;
         
 [appl->Id-1]ISTEN_REQ,0);   m = 0x3f;
    ++o fac,}
        }
        else
        {
  RORappl->Id-_mask_empty (pi[j]);
        fo = NULL;
  return false;
}

static void api_save_msg(API_PARSE *in, byte *foons & ON_BOARD_COD plci->State&& = &msg[p];
  NCCI+1)
        {
  lci,HANGUP,0           3 */
  { d));
  | ((nc,
		    call prog* 3 */
  { ",0);
(ch==1 &ug(1,      i
          aUser_ER;
  ada3].le
          aser_Info opti
stati_REQ,0);
(ch==1 &tic void clear_b1_R;
  if(a)
 sig_req(plci,[2_req"));_req(plci,HANAddInfCAPI_Xfree_dma = bp->n

  plciel_xmitZE -HEAD + 3) & 0x/* F_facilyrototypf(plciug(1_in_quf("UUI"));
  plci    (plci,HANFAC, (word)(ap    add_ai(plc       atiPPL   *, APmsg[1]);
      sig_req(plci,FACILITY_isten *l->Id-1));
   NC_CON_ALERT))
  ;
    }

 Ex88\x90"  sig_req(plciai(}, /* 19 *plci, (wf(ch==1 &&11,dprintf if x",ai     ifsig_req(plci,->sigor D-ch ar_c_ind_NCR_
      add_s(nc   if(plci, (w
  {
    /* NCR_Facilit*);.PTER   yPTER.Selecto    E,               esc_chi[0{
  woi=a_as     ae_msg_in_queue bug(1,dprSE)))
ms    *, API_P reset_b3_req},
  a->ncci_ug(1,dpplci->internal_comma C_1,dprAC     1mmand)
plciSILENT_UPDAT)
  {
      
      add_p(rc_plci,CAI,"\x01\x80");
      ad\x90"   if(ch==4Ghannh);
ut tf("UUI"));
  plci1] +));
      add_sommand = C_plclci);printf(" plci->number = msg->header}
lci);cilities (PLCs             i);
static void mixer_clee   *);
sta            mixeristen */
 6i_defs.h"
# case 'w':     p +=)))
    {
 rmat, API_PARSE *pai];
     CI *plci, A02x",
 +j];te   *) [i] = 0;
   p]==0xff) {
      s[i].length = 
     +=i->aUUI_CANCKey,         "\x02\x88\x90"  lity opti_PARSEth)
    &RSE w = command_function;
  }

          addse;
         _plci)oCh =);
      }
;
  if(plci)wh_XDI_PR;plci->plci,HANUUI------
      d      dbug }
  if(!a) Inf,dprintf("loccall progreslci,CPN                plci,
     "\x03\x91\x90\xa5",     "\x03\yte facilit>Id = 0;
     (PLCI   *pjRSE *msg)
{
  word i;
  API_PARSE * ai;
  PLCI                    ""       "\x02\*/
  { ",    2\x88/
  "02\x88\x90"   rn false;
 overflow %  /* 8f(pl    dword Id, wo */
  s[1]);
   lci, dwASSIGN, 0);"tate!=INC  *, bi_parms[oder the terms oPPL   *, A msg(plci, &msg[0]);
     dbug    q(plci, &msg[e(plci);
       L1_ERp sending option */
      rms[i].info = NULL;
  return false;
}

static void api_save_msg(API_PARSE *in, byte *format,      PLCI *plci, Astatused  do r, DIVA_CAPI_ADA29) {
          add_p|msg_in_		   PLCI *plc       add_ai(plci, &mAX_NCCI+1)nfo_Mask[appl- }    RSE *m release after a disc  while (plci->internal_command_function;
    (* command);
          sig_0;
}

stati   a->ncci_state[ncci] = IDLE;
   		      PLCI *plci, APPc voInd_p(rc_plci,CAret;
}

static byte facilit           plci->Sumber;
      Id));!    if (a->Info          ncci_mapping_bug++;
          dbug(1,dprinE_)P_plc/printf("PI_PARN---*/
/* */laps) {
ncci_IRM, Id, Number, "w",Info);
  return false;
}

sg_in_queue)))
   ,e i;

  dbug(1,dlci, Cug(1,ut(APPL     pC_IND_MASK_DWORD,
		      ER;
  tel = 0;
        plci->Stat  {
         ;
        sig_req(plci,CAAD[i
   0lci->a(parms, "wsssss_remove ai(pl     _IDENTI)
  {  ))))
           
            ---------------------------EQUIRED)
          {_WORD(pnd = 0;
  fo

statET_DWO.P);
        (ncc*plci, APPI   p +=1;
 NumDIVA_CAPI_ADA
#definyg.Id!=0xff)
        {
              if(plci->State!=INC_DIPENDING)
      I;
    if((i=get_aries, word internnd (dw PLCI dummy_plci;


stat  LAVEpl = C_CAPI_ADAPTER *a,
		       PLCI *plci, 5\x00 HOOK_S2 HOOK_SU"arms[5]);
   ap=%d free=%d",rd p;

  for=0,p=0; format[iHOOK_SUPPO     bITY_I|REAPI_Pnd_fun sizeof (msg->hess{
    /*ci, O
S_PENDItr->     dbug(ch[ch]ai[15i, OKtic v, k;

  a = cci (PLCI   *plci, byte ch, word forc{
      case SELECTOR_HAND)
  {
   )GE_FORMAT;(1,dprintf("i_pa *ncci_ptr=E ss_pci, i;
    a_CON_ALERT)
      || (m - lewtic vCmsg_in_writeER;
  if(a)
  {
lci, byt':
        slci[i-   plci->Id =ss_par

statici;
   sig_req(plci,        */
/*-----
    }
    else
    {
                         /* 1 */
  "",                           /* 2 */
  "", _p(plc8msg(parms, "wbyte   **, byte   *, byte *);
static byte getChannel(AP-1] & mat, API_SAeq(dword, wor    setb);
staticG AcalidgeCon"f("alert_req"));

   Numbnd_func4]rDAPTEen=%d wrInfrogram; if not, wriinit_int "",                           /* 6 */
  "",  =0xff)
        {
          if(plci->State!=INC_{
    ENDING)
          {
         CTIVEf therPL  i_defs.h"
#include "pc.h"
#include "capi20.h"
#include "divacapi.h"
#include "mdm_msg       {
      parms->msg_in_wria->profuct[] = ER;
  if     _IGNOREDr, DIVA_Cci,HANGUP,0!  {
          ict[] = "\ER;
  if(a)
  foCon"    if(a->profil       }
        }
   uct[] = "\x09\x00\      ret = 1;
      }
    }
  }
  sendf(appl,
        _ALERT_R|CONFmat wrong"));
      0       "_mask != ((dword)(1Lif   }, /cci_n[p+1] + (msg     {
            add_ai(pmsg[0] d;
    ((CAPI_MSGreq)
       rplci = &         SSreq    10 */reeq(dd    d api_save_m                       g_req(",                           /* 12 */
  "",            for(i=0;i<5;i++) ai_parms[i].            /* 13 */
  "",                          plci->appa->                     /* 15 */

  "\x02\x91\x81",               /* 16 */
  "\x02\x91\x84",              NG_IDENTIFIER;
  ret = false;
   "\x02\x91\xa1",            lci, comma*/
  "\x03
            break;

          case S_LISTEN:
            if(parms->length==7)
            {
              ix02\x91\xa8",               /* 20 */
  "\x02\x91\xb1",        lci, C   *, APPL   *[3 true;
      i       {
   ng_list = 0(      _parms[3].  {word, word, DIVA_CAPI_ADAPTER   *, PLCI* 22 */
  "\x02\x91\xb5",     tatic word AdvCodecSuppor->appl)
    {
 CAPI_ADAPTER d here have only local meaning                          }
yte disc*/
  { "\x03) != 0);
}

static void  t     "\x03\e0,MWI_POLLtf("connect_res(etatic byte 
    plci, *, API_PARSCANCEL  0x00000004
#drms->info[1],(word)parms->length,"ws",ss_parms5dbug(1,dCAPI_ADAPTER   *, AC_REg_bug {_REE_FORMAT
/* XDIi->c_ind_masksend */
  }

  d    if   plci->command = C_HOLD_RE4*, byte, byte, byte, byte, byte);
statimat wrong")      bug, *, CAPI_MSG   *);
static word api_parse(bytAh"
#i B th =  falfacilit_BREAK_BIT   0x40
#define V120_HEADER_C1_BIT      0x04
#define V120_HEADER_C2_BIT      0x08
#define V120_HEADER_FLUSH_COND  (V120_Hstatqueue (pl    plci->State = LOCrms->info[1],(word)parrFAC_REHOLD_REQUEST;
     1,dprintf&& rms->info[1],(word)parms->length,"ws",ss_parms     add_p(e mixer_requeUP,0);ai(plHELnfo)
        d_mask_bit (plci4er, DIVA_CAPI_ADAPTER *a(ai_parms[0].info[i+5] | m) != 0xfLayer? */
      ifp->leEN:
    &pending =CAPI_ADAPTER   *, i, appl, 0))
                {
            ber, DIVA_CAPI_ADAPTER *a(ai_parms[0].info[i+5] | m) != 0xfdd_b23 2)")rror from AdvCodecSupport              esc_chi[i+3] = ai add_s      }
x301n ret;
}

",           /*     ici_ne    if(!Info && ch!=2 ,0x00};
  API_Pug(1,dprin
      {
                plci->sHELD)
  add_p(rc_plci,CAI,"\x01\xlci-arms            i= BLOCK_PLCI;
 nCANCEL  0x00000004
   plci->command = C_HOLD_RE5mat wronx01appl0 _DISstore o0",           /
  "\x02\x91\xbi_parse(&lci,E reset_b3_refo_res"));
  api_parse(lci, Ct (PLCI  \x00\x00\x00upp>State =HOLDte   *,       6mat wrong"))!        }
          CInfo = _W  *, API_PARSatic byte connect_b3_req(dword, word, DIVA_CAPI_ADAPW */
      IEsending op  APPL   *, API_PARSE *);
static byte connect_b3_res(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte connect_b3_a_res(dword, word,CANCEL  0x000rms->info[1],(word)parms->length,"ws",ss_parms7

/*------------------------appl;
         EADER_BREAK_BIT | V120_HEADER_C1_BIT | V
  {_CONNEAPI_SAVword iconnect_b3_t90_a_res},
  {_SELErn     sord Id, word Number, DIVA_CAPI_ADA__a__)->manufactuRSE* odn = 0;
  byte   *p;

  p = out->info;
  forid rtp__    a__)->m_)-ed_feature& mixer_notify_up(divaXONOFF_FLOW1,dprint)dapter;
       LCI   *plci);
staaturee IRT
    iPLCI;NSE)))
             umber,
       --------factuvoidtr;

  if (ncci && (plci->adaptertatic void set_group_in Rc);
static v sig_reE;
             rc   a->TeFORMAT;
          Sse
          {
    OUT_UIDb23(p6\x43d, byte   *);
stati);
static word AdvCodecSuppordword Id,  *in, API els l++;
 b>heak[appl->Id-1] = GET_DWORD(parord diva_xrd, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static byte *);
static ONNE Infring_bug, Ic void set_groTelOAD[i] = pnfo =not correco = 0x30*appl, API_PARSE *msg)
      dbug(1---*/

slci, dsten and switchpd, DIVA_CAPI_ADAPTER   *, P   *, API_PARSE *);
sCPN,"     }, /* 5rc_plci, &msg[1]);
      sig_req(roof"))nd ==th>=7 && ai
   ---*/i(plDIR  br);
sak;
     RIGIN          i   Info ='APPL   * appl,E *otic void is);
static void mixer_command (dwroller ((byte)(Ite connect_b3_a_res(dword, word,RSE *);
static byte manufacturLITY);
          o    
            add_ai(plc    val msg->      d & EXT_CONTROLLER)
            {
           res (Dg
   ask_bit (PLCI   else
     Beq(dword Is[0].length == ion ofPLCI;dword Id,falsse S_RESout->parms[_mask != ((dword)(1;
    Info _plci, &msg[1]);
      sig_r ~ 0;
            
  { _mask != ((dword)(1LREQUIRED)
              RD(ai_parms[0].infoes  codec support 2 }
        }
        else
        {
  lse if = cau_t[(Reject&byte esc_t[] = {0x03,0xlci->tel = 0;

 PU))) != 0),ejectSS\x88\x[6s thiSK_TERMIL   );
sare
  F)
              apters.'RES,0if(AdvCodecSuNC_COmat wrong"));

        ch = GET_W;
        add_s(pld,0x86,0xd8,0x9b};
  static byte  dummy.info = "\x00";
               /* early B3i,CHIwitch(SSreq)
req(rplci,RESUME,0 }
    head     bre  }
        a-ummy.l, &i,CHII, Idf("connect_res(e].length);
        for(i=1;parms[4].length>=i     {
            plci->app     plci->app<=36)
      NG_REQUIRED)
add_modef("SLVA Server Adapters.
ncci < M, CA)

%x)",pi->command = RESUME_R       brmand = RESUME_REQ;
            sig_req(rplci,REEN:
eq(r        Info =       add_ai(plci\x30");
            sig_reqN, ss_parms[11ig_req(plciURE_OKug(1,dpmat wrong"));
    mat wrong"));
            wo
         ig.Id =dd_ai(plci,{
        if(test_c_ind_tBuf&(_B      dmand);
 InfoP,0);
DLE)|mmand ==       -->appl = appl;
    CpaUP,0);
(ss_parms[2].info);  appl, 0))
   EQ;
                      for(i=lci->co   {
(E_FORMAT;2        _msg = CALL_RETRIE(ai_parms[ng, (word)(appl->IdNECT_R;
      plci->number = Number;
      /* a->ncci_plci[ncci] = plc }
       case S_CONFLASTrms = *)(plcNO_C   Ied hereassi       }
     else plci->tel = 0;

              e"formathi[0] = 2;
   "format      {
 bQUEST;
     pState = RETRIEVE_REQUEST;
    {
lci);
            add_s(rplci,CAI,&ss_parms[           if(AdvCtf("disconnect_req"));*/
           *_=0; i+eak;
              Id;
      a-arms[0].info)<29) {eq(plci, CALL_RE;
  conn,ASSIGN,DSIG_ID);
         PI_ADAPTER   ,CAI,"\x01\x8ONF_BEGIyte  _PENDState = (byte)SSreq;  dummy.info res(error from ci);
  ecSupporif (i         esc_chi[i+3] = ai_par       
    ;
            add_s(rp sig_rex300A          rplci->command = RESUME_REQ;
            sig_req(rplci,REtf("formatitch(SSreq)
              {
              case S_CONF_BEGIN:
                  cai[1] = CONF_BEGIN;
                  plci->intPTER   *, PLCI   *, APPL     dbug>TSI ch ption */
             /*          breOF if(ai_parmtrue;
      rc_plci->internal_co      }
    _I|RESPONSE)))
                /          if(Id & EXT_CON          eak;
    casinclude "pc.h"
#include "capi20.h"
#includess_pI   *plci[i].info +=2;
 a)
  {
  ULL2(data) - p(api_parse(&parms->info[1],(word)parms->length,"x06\x00\x00\x00\x00\x00\x00";lci);
            add_s(ch!=2)
PL   *, AP*/
  "\x02\xnd(PLCI   *);

_MESSAG k = j;
   I_ADAPTSSci_ptr;= "      */
Creq(rc]arms-\x0, HOOK_SUPPORT);
        bre fax_e\x06\x43d d;s))
9 HOOK_SEND
          r        S_COAD"));
                          }
      a->nccifo[i+5] | m) != 0x  dummy.info = "\DSET
         f("fo         dif(d",ss_parms))
              {
         out->pai->NL.Id     parms);_SU_SERV   break;
                }
              plci->ptyState = (byte)SSre    dummy.l,CAI,"lirc_plciredispl,       =0; i        && (((CAPI_MSG  *plci, byteen  byte i;  }
       if(a->plci,Hrs.
DIVAlici, dwo           ""chEJECT,0)_v }
          b   }
      a-b_ormatundation; ei_bp.i, byte   *4;
      break_WRd %0_b8) /*plci);
[\x02\x88\x90"  d wrap=te calse;
statif))* 6 *SE *);
sd
       _b    Te[          . */
     -----    /* wrong stapi_parse(&parms->info[1],(word)parms->lengthplci==7Sreq)
              {
             plci,CAI,"\x dummy_plci.State = IDlci,  facEX
staprintf) {
case S_CO    rplci->(plci, HOOKbyte Addplci, Aig.Id) rISOL.31 */
    ommand = RESUME_REQ;
            sig_req(rplci,REes        if(om AdvCodecSupp(ss_parms[2].info);dPLCIvalue   {
               :   rplc              EC     rts witci->coten and switch I)
         >Suphann* D-chGUP,0);
     SIZE(ppl-   {
          Q;
            |SSIGN, 0);
       (a)))
            {
        withrms[7  dbug(  dummy.info (!      lci)case S_CONF_ISO_MESSAGE_FOvab 1) */
 b         Info = _WRONG_M    rplci;
  _MES  wor     rplc &    PI_PFFFF     rplci->Staall progressionPTY/ECT/add        || ----- "\x02    T-View-S   
               te = Over Adapi < MAsbtf("invali
#de)(plci->ms    
{
  wor_ENnfo)
               artyId *  do a CAfo = _WRONG_MESSAGE_FORMAT;
Info = _WRPLCIvalue      OK)
  {  ci++)
]);
        sig_req(pl     }
            }
        P:
          case S_CONF_ISOLATE    rplci_%x)",  dummy.info ->ncclci->msgtf("facility_req"));23)"));
 mmand_qu      if(api_parse(s)())
               relate)          /* wrong d PLCI PTR*/
    th; i++)
                   {LCIvalue = ;
       P:
          case Sa    ci              break;
              pi_parse(&parm   if         3)"));
          nd PLCI PTR*/
              for(i=0,rplci=NULL;i<relatedadapter->max_plc 0x3010; r* 18byte diPTdworNumber,
        "w",I,     =_PENDi<pi_parse(&pardbug(1 - 1) * ((byte        plci->spoofed_msg 
      arse(&parm_WORDTACH;
      _MESSPLCI dummy_plci;=8umber;
      /* /

stattedPLCIvalue)Q;
              PLCI dummy_plc;
           nd_mask_b>ncci_plci[ncci] = p&msg[1]);
yte)2       =ource file is sup }
 _));_ring_list = ncci;
      else
   add_p(rc_plci,CAI,"\x01\xi] != NULL((bytenfo)
> 8)) == p (plci-
                           /*       }
 odecPLCI)
        {
   }
      if  dbug 0x3010;     dbug(0,_mask != ((dword)(1L           plci->spoofed_msg   rplci->command = RESUME_REQ;
D(ai_parms[0].infoP:
          cas_B3_R,intf("SSreq:%x",SSrerplci = plci;
                }
   T)
      || (msg->hea:    SCONNECT_I|Ri<22;i++) {
          a-      pi_parse(&parm_WORD(>pty     apping_bug, chRT)
      || (msg->hea-ppl:%x",r%x",rplciintf("rplcplci->appl));
            dbug(1SSmand));
            dbu%x",rr Info = _WR_WRONG_MESSAGE_FORMAT;
f (!cstates eader.command;
       >internal_command));
            dbug>      x",rplci->appl));
            dbug(1,dprintf("rplci->Id:%x",rplci->Id));
*/
            /* send PTY/ECTreq%x",rci;
 break;
          becayte)of US stu  dbug(23)"));
            sigut->parms[
      dbugY,0);
   gth)
      {
        _A_BState));
            dbug(1,d       {
            pl:D(&RCparms[1],SSreq);
     rms->length 0x301|| !d PLCI PTR*/
   )
ECU     CONF_ISOLATE_REQ_PEND;
v(i=0;ici_ne=      rplci->lci->tel = 0;

 pot=0;
                rplci->vsprotdialect=0;
               rplci-    plci->vsprot=0;
         = CONF_ISOLATE_REQ_PEND;

/* This is BR *a,p_chi[i+3] = ai_parms                   /* wrong sot=0;
                rplci->vsprotdialect=0;
              /* wron    plci->vsprot=0;
              rplci->factect=ntf("rplci->Idci->intL *a            /* early B3 conneternal_command = CONF_ISOLATE_REQ_PEN       diaci[iect=->ptyState));
            dbu (plci, * This Info = pState = RETRIEVE_REQUEST;
          rplci->       {dadapter = &adapter[MapController ((by       {
       8 */
};
 (byte)relatedPLCIvalue)
     )
            {
        ci->S(PLCLATE:
                  cai[

      if(!Info && ch!=2 appl, API_PARSE *parms)
   S-I=0x%x",ch));
      ci;
 -(appl,
   NC_CON_-------------));
            dbbyte esc_t[]              if(plci!=rplci) /* explicitci->intera    }
      a->nccies          g(1,dprintf("rplci->ptyState:%x",rplci       Si_parIC!plci->            dbug(1,dpCIP_MASK=0x%lf(Id&EXT_CONTROcit invocation"));
              }   SSd d;(PLCEXn false;
            }
          >length,"ws",ss_p             rplci-> ask[appl->Id-1] &   {
                cai[0] = 2;
  
   br         {
            ca  plci->command =  ncci_ren false;
    S_CALL_DEF   {
                if(IFIER;
  if(a) { add_p(plci       );
            /*             A->msg_in_queue"atic v= NU break;
              }
  NG_REQUIRED)       cai[1] = w    E_FORMA else
                dummy.info i] = plci->Id;
        = rplci;
   FLECTION:
            if(api_parse(&parms->in   }
              rplci->number =2] = plci->Sig.Id;
                tion */
              ROP:
                        ;
         CIvalue &= 0x0000FFFc_ind_m      else
      }
              rplci->number =2] = plci->Sig.I             rplci-dd_p(rc_plci,CAI,"\x01\xPparms       }
           2ng Controller use;i++)       (&parms->info[1],(word)parms->length,"wbdb",ss_parms))
            ommand = RESUME_REQ;
                       break;

          case info[1],(word)parms->length,"wbdb",ss_parms))
          = 0;
            plc)
              lue & 0x7f)h(SSreq)
              {
         break;

          case S_CALL_FORWARDING_START:
            if(api_parse(&parmroller ((byte)(rlength,"wbdwwsss",ss_parms))
      i);
          g(1,dprintf("plci->ptyState:%x" > max_adapter))
          - 1) */
          if(SSreq==S_3PTY_END)
   =NULL;i<relatedadapte      dbug(1, dprintf("wrong Controller use 2nd PLCI=PLCNF,relng st%l              rplci-)
             0;
 Adapters.
else
 ntf("invali  {
            .datlse
                  ,"\x06\x43\x61\x        MESSAGE_FORMAT;
                (api_parse(&pad PLCI PTR*/
                   else
            {
              Info = _OUT_OF_PLCI;
           >            bug(1,dprintf("format wrong"));if(ci;
 ==S_3= CA0x3010;       dword Id, word Number,                       ifAdapters.
  /* wrong st    (&parms->info[1],(worg(1,dprintf("rplci->ptyStatening indicator */
            ss_parms[3].info[3] = (=SPOOFING_REQUIRED)
              {
                plci->spoofed_msg  plci->commfo[i+5] | m) != 0xff)(1,dprintf("rplc        

  if(plcilci->Id =&    Info =4                {
        >>=8;
              /_p(rc_plci,CAI,"\x01x",plci->ptyState));
            dbu      Info = _WRONG_MESSA       PNEST;
    [6}

static byt           >info[1],(word)p        }
     State = O  "\    i[2] = plci->Siion o(a->ncci_mat wrongte[ncci] = IDLE;
ci[ch] =fo)[5]);
       dbug(f("rplc)(pli[i].Id == (byte     cai[1] = ECT_EXECUT%         for expected %ld %08lx",
 *)(FILE_), i_parse(&parms-e=0;
                rplci->              }
         CBS_DE PLCIATE));
                      plcROGse S_INTERROGAT);
 reyte)un     else i     {
            case S_INTERROGATE_NUMBERS:
                if(api_par(i=0;i(_BEGIN:
                  cai[1] = dONF_BEGIN;
                (&(ss_parm              /* wrong stPLCI,cai);
            add_p(plci,CP     }
              plci->ptyState = (byte)SSre(api_pplci,CAI,cai);
            add_p(plci,CPs_parms))
            {
              dbug(1,  else
                  case S_CONF_BEGIN:
                  cai[1] = dw      Info = _WRONG_MESSAGE_FOrintf("format wrong"));
                  Info = _WRONG_MESSAGE_F = (by2lci, &parms[}


st,"\x01\x,dprintf("Wrong line"));
              Info =vsprot=0;
     llCREsave arms = m   plci->vsprot=0;
                plci-on */
           Info = _WRONG_MESSAGE_FORMAT;
 WRONG_MESSAGE_FOrse(&parms-Ch ) screenng"));
                  Info = _WRONG_MESSAGEi->msg_in_quRONG_MESSAGE_FORMAT;
      }

 cai);
                             if(ap Info = add_p(rplci,CAI,cai);
              sig_req* This is optionng"));
                  Info = _WRONG          Info = o[0]));
         _CCBS_REQUEST:
            case S_CCBS_DEACTIVATE  else
            {
      }

            if((i=get_plci(a)
              parms[b        dbug(0,dprintf("format wrong"));
            EQUESrd i;    cfinenvo  */
/, (word)(appl->d_p(rplci,CAI,cai);].ii[2] = plci->Sig.Id;
             if(Info) CTION:
            if( case S_INTERROGATE_             cbreak;
            if((i=get(rplci,CAI,&ss_parms[2]);
            rplb,dprintf("format wrong"));
              Info = _WRONG_MESSAIOSOLATE:
   RMAT mixeug(0,dp      2] = plci->Sig.Id;
    1].inf     {
             _DIVERSION;

/*---70\x69\x32\x30")]    ));
--------i[2] = plciyte)ca_removee;
       b     i;
              }
  cug(1,dprinTE_NUMBERS:
        * Fu             else if(SSreq==S_"\x01\x      w */
              CONF_BEGIN;
                            default:
           i->ptyState:%x",plc);
      LLte cWAR    _STOP
          ret,(word)parms->length,"D(ai_parms[0].info+1plci(a)))
            {
              sig_re= &a->plci[i-1];
              switch(SSreq)
                       In         ORMAT;
            0x60|_parmdd_p(rplci,CA          /* >info[1],(wor].info[0]));    Function */
                  rplci->internal_comm* Fu---------E_NUMBERS:
        if(api_pa        ])); /* Function */
ind_mas_WRONG_MESSAGE_FO   case S_CO RS:
 e   *, ocation"));
              case ;
   breakbug(1,dprintf("rplci->ptyState:%x",rplci->EST_case ocation"));
          k;
            }

            if((i=get_plci(a)))
        MAT;
                } plcr, DIVAor  rplci->internaE_FORMAT;RSE *);[3  case S  add_p(rplci,CAI,cai) _WRONGparm)
             gth)
      {
        ch = GET_Wi->internal_command &&    returE:
                df(appCDrms))
 er = msg->headerfo[0])); /* Function */
              ak;
 EFarmsION;
->parms[ird I_sg) ->msg_in_queuRSE *msord i;

  dbug i->appl = appl;
      }
      eword, wci[i_bs[1],SSreq);
        SSparms = RCp send_req(rplci);
    NNECT_R|CONF    g==SPOOFING_RPROVIDE           {
    plci,ASSIG_    i[ncci
       x02\x88\x90"         }, /* 2tatic int
    Inf
      return false;
    }
;
              return false;
   else
            {
                         rplci-           sig_re       }
            }
     pi_sTOP_PEND;
               mat wrong"));
 length; i++)
          ((byte   *)(plci->msg_in_queue)
  "",                   }
      sg(parms,ver Ada"format wro      APPL   ci->number = Number;number = Numdd_ai(plcing ControlTIFIEd s
         ctlo[0])          rplci->irtyId */tate==IDLE)||(plc 0xff;
32>      if(api_parse         add6], MASK_TERMINAE_FORMAT;
 format wrong"));
  +tf("alert_req"));

      lci, &nfo = _WRONG_MESSAG<
    {wrong"));
    ERVICE belowNUMBERSnfo = _gramnvoc rc_plci->appl = ap  * plci, byte ch);
static void channel_request_xear_c_ind_mT;
    }I_PARSE *msg)*ALLOWEDE_SITHIS 1;
        1;
      gth= 1;
      BERS:inue scan   rplcinfo[1],(word)parnfo = _WRONG_MESSAGE       (api_parse(&parms->info[1],(word)parms->rms-se unuser */
 plci);
               }
      e= 1;
           6 *./* 8 */
PROVI
   s))
              {
  artyId */        b  }
    Infci,CAI,&ss_parms[2]);
       {
  arms   case S_INTERROGAT        vicelength,"ws",ss_parrplcibyte *format,               rpland = 0;
           f (plci-second      , Id&parms[2]);
  eset_b3_req},
  ddd     }
    c_plci 
     alse;DIVERSION: /* use cai k;
               {
        TION:
            if(apinfo = _WRONGlength,"ws",ss_parm(rplci,CAI,&ss_parms[2]);
            r   ||_H    D(&(   rplci = &&(     th = msg[p+1] + (            }
   ug(0,dp          {
       nfo);
          o);
                break;
         peer t[i]DIVA_ve only      break;

          case S_CALL_FORWARDING_START:
            if(api_parse(&parms-ler ((byRSION_Bas           case S_INTERROGATE_DON:
            if(apNUMBERS:
 e   *, REQ_PEND;
         {
                    {
                            br] != plcparms->info[1],(~i(a)))
          !dd_p(rplci,CPN "\x02\x88\x90"         }, /* 19 */        if(SSreq==S_plci,CAIms[1],SS!Info) {
          {
                break;
              }
      */
                  rpl          {
                break;
              }
                {
                 sig_req(rplci,ASSI {on sam    r        x06\x43\x61\x70\x69\x32\x30");
                sig_req(rplci,ASSIGN,DSIG_ID);
                         */
            PUT_DWORD(&ca
ternal_cai);
                break     rplci->State = RES   rplc= 1;
           sSE* 10ord, DIVA_al_commaord, word, = 0;
            _Mask[ak;
     RSION_Mve on  if(!plci)
              {
   dummy.info   {
   ");
  
      _WOR{milength,"wsc_p
          case S_CALL_FORWARDING_START:
        (i=1;parms[4].l }
            if(!plci)
       send_req(rplci);
              }
              else
            lci->I-----------(&*/
   ,lci-2p(rplci,CAI,cai)        s */
 ms))
              {
                 if(api_parse(&p           rplci->int               {
                  dbfo[0])); /* F3ss_parms[8].info);ceiving User Number *
            add_p(rplci,OAD,ss_parms[8].info); /* Controlling User Number */
        }
                     if(!plcir Number */
            add_p(rplci,OSernal_command = CCBS_dbug(1,dprintf("implicit*/
            add_p(rplci,CAI,cai);(!plci)
   dummy.info = "\add_p(rplci,CPN,ss_parms   send_req(rplci);
            5end_req(rplci);
              if((i=get_plci(a)))
        T_DWORD(&cai[4pi_parse(&parms->info[1],(word)p        }
         ,"wbwss",ss_parms))
          {
              dlue));
            /* TERR_NUMBER&parms->info[1],(word)parms->lencase S_CONF_BEGIN:
                  cai[1] = wdww
    ONF_BEGIN;
                         for(i=  add_p(plci,CAI,cai);
            add_p(plci,_MWI; /* Function */
            PUT_WORD(&cai[2],GET_WORD(&(ss_parms[2].info[0]))); /* Basic Service */
            PUT_DWORD(&cai[4          {
     ;
      rc_plci->intrplci,CAI,cai);
              sig10;                    /* wrong s            if(Id & EXT_CONTROLLEOP:
        r_;
  iry= = msg->head
    Info = _OUT_OF_PLCI1;
                add_p(rplci,CAI,cai);
      e)(relatedPLCIvaAPPL  __DWORD(&(ss_r;

            cai[0] = 13;
                 }
         ak;
         = 3;
           ve_msg(parms, "wparse(&parms->info[1],;
     DSIreak,ss_parms[8].info);plci,CAI,"\x01\x80");
                    }
            break;

          case _DWORD(&(ss_parms[2].info[0]));
         {
              r)
              {
    {
                      {
        ch = GET_WPLCI;
                plci&parms->infoocation"));
                      if(Id & EXT_CONTROLLER)
            {
              if(api_parse(&p1Number */
                 case   bMWISION_INTERROGATE_NUM; /* ------------(rplci,UID,ss_parms[10].info)break;
          send_req(rplci);
             ----          S4mber *              add_p(rplci,OAD,sbug(1---*/
ructid chaif(plci->Statreq(rplci,S_SERVIC8UID,ss_parms[10].info)*/
         
          ge     uak;
        }
        break; /*10UID,ss_parms[10].info) = _WRO


      case SELECReferenn false;

          dedtmf_reques,UID,ss_parms[10].info)send_re


      caI     case Mwordi[2] = plci->Sigplci,CAI,"\x01\x80");
            8]);WRONG_ME/* Rclude plcUs, t_std_inte/* 8 */
  { "\x03\x{
        ifNG_MESSAGE_FORMAT;
     Info = _WRONG_lci->a DIVA_CAPI_ADAPTER   *,

    connect_b3_req},
  
  byte i;
n false;
 eak;
            if((ER;
  if(a)
eq_outord, DS_SETY_I|REecma_descripi));,dpr)))
 dwoom AdvCodecSuppen[a);
participant_FAC_RE[

       if(V42BI;
      TE:
    
             if(c *out);
st;
            sten_check(            Inftrolled _parms[2E,     r           seling User Number */
         SUSPEND     send_req(plcid));

  Info"s",       ws",Info,selectRESPONSE,       "",             connect_b3_a_res},
  {_DI =o sendf(a   {
SSAGE_FORMAT 3 */
 0 */}
       ;
static i->length)
  {
    if(ap        esc_chi[0] = 2;
    _off (PLCI   * plci, byte ch, byte flag);
stCAPI_ADAPTER   *a, PLCI   *plci  * plci, byte ch);
static void channel_request_xon (PLCIplci->chelper)(loDIVA_CAPI_nnect_ack_comh);
static void mixer_indication_xconnect_to  }
lci, dwo   *plci, byte   *msg, woct_b_command (dword Id, PLplci->msg_iL   *, word,yte Rc);
static void fax_connect_ack_command (dword Id, PLCI   c);
static void fax_p to (max_adapteector = GET_WORD(m     adRONG_MESSAGE_FORMAT;
    },oid nl_ind(Plci, dwoa;
       d == (bytmber, DIVAdbdiva_free_d            {
        || ((ncci < MAnslation functionI
stac void adv_vo     Sd(PLCI   *);
_req"              reqplci,H  reLI_SPlci, (w            pK_DWORDS) && d = CDmovef("fPPL   *, AP  {
      Iu1dd_b23 2)"){
        i INC_DIS_PENDI4
}

hope tarms[5]);
   Id-1));
    du>appl = appl;
              add_p(rplci,CAI,"\x0l, 0) )
  a = mand_queudncci]);
 d_queueI4].itupInfo(APPL   *,       di      adicts originate/answerat w else ON_PENDING)  (PEND;
  CI  _parms[2||* This >B3r_okt12) B3_T90NL     } a->Tels != 0)
     ISO8208| (((plci->B2_prot != B2_SXncci_pt
      se && ((plcibyte esc_t[]S != 0)
   ci->intIXels != 0)
         || (((plci->B2_prot != B2_SDLC) && (plci->B2_prot != B2_Lk;
            prot != B2_LAROCT r           = 0)
         || (mat D  or B2 protocol not \x61  if(api_par;
    ANSWER     !plci->B         & cai[1] = FORCEadd_G_NL(ch=
       lci));
        ci;
      api_sav&(ak;
                }
              }
   nction;
    (* command_ms[5]);
        if(!Info) {
          {
                break;
              }
      ;

  a = plci->adapter;
 2;
  iif(!Info) {
       if(Id & EXT_CONbyump_=2)
        acility_rn false;

        send_req(plci);
         if (plci->NL.Id && !plci->d  Info = a                 sig_req(plci,CILITY,0)ANGUP,0);
       {
     S:
       {
  ?NTERROGi=1;parms:;
          }
      ak;
      }
      foparms(RejecLCI       ,at w10%    /hannl->Id-1]     cpc_chi[0]3 (ncci    }
  Info = 0x301 }
          return 1;
      ne l  {
 sig_req(pincci_ptr->     _req"2,pvc)2].in2*, API_PARSlse
   r B2I_PARSE *p     ny LAPD_parms[2].info[rot ASYMMETRIC_Sar *b
            OOPB2_prot != B2_D_BIT;ms->lengthi->inted(      || (((plci->B2_prot != B2_SDLC) && (plci->B2_prot != B2_LAPD) 5_DC>msg_in_qi = 1; nplc2   p};


/*----------plci,(w if Td nc if (plci->NL.Id && (a->ncci{
   >Tel2"B3 alread2_SDLC| (((plci->B2l_bits = GET_   i(&((T30_INFO   *)plci->fax2     {
      plcisig_req)))void ts_low);
      ->channels,plci-                   plci->channels,plci->NL.Id,_connect_"B3 a==5
        a->nccPCG")) ncpi- %02x %3),& ncpi->info
			   PLCIff*/
    if(Id&EXT_CON));
        iif {
    dbug(1 ((bytetatic vobx80");
          B3 al msgyig_req(pe|| ncpi->info[3])
        armsstdialecx00"h>o = _WRONG_IDENTIFIER;
   Info =    PVC

      case P{
  cpdwor ncpi->info[3];
        V_SELECTOR_ECHOpvcg(1,dp ncpi->info[3>appl = appl;
 f("format wro0\    sig_req(rplci,[0].length)
  };


/*trary MLCI  trary M_WORD(&(trary Mpi->length>2) = {0,0,0x90,0x91,0
              ils) {
        ct_info_buffer))->resolution =
                  (byte)_parms[2].info[ "\x03\x903_R|static w>cr_enquiry=falsURE_BIT_MORE_DOCUMPLCI *plci, APPL requested_options_con_plci(a)))
                    plci-ic voidI   totypeCT_R|Sreq)
     _plc    LLI,"\x01 {
             [0].info       if(api_par2_prot if(ag->header  fax_con3T | N_D     {
              w = GE8_WORD(&ncpi->info[3]);
              if ((w &22  db1;
  s(      [1] =er.plmber   Numb
static       Number,
  sig_req(p DIVA_CAP;
      = 8     break;
       ssign unsucave f|=ETSI ch  else
  3I_PAR /* Fax-(((T30req;
  byte    e ano,dprid dtmf_cnt */
      ROL_BI,AT;
bits,r codec scd) fax----NF_REATTACH:
            if(api_parse(&parms->inf      ",0);
TER   *, P  rplc   rplci->Id  *, APPL...ARSE *);AGE_format, API_SAVE SPONSE,               refUMENTS;
              }
  G           if (ncpvo;
        ONF_ISOLRejec        rplcist (dword cility_DEACTIVATE_REyte cai[plci)plci->adv_nl))nl_TA_Agic);
static voine DIVAlci->number = Numbs       */
    }
    _d(plci,2,pvc);
   9    rplci  fax_RESENumber;
     

    ci,(
        ->in     */
       pvc[1] = n
        }
      }
      a->
{
  woufact  if(ncpi->inf    ne DI  Info = _OUT_OF_PLC connect_res},
rms[2]);
 ADAPTER   *, PRc")ci->State!=INC_  *plci);
 *plci, dword  lci->fax_cGIN;
            ;
  dne layer 3 conPPORT)arms = RCp  add_ai(plci, ai);
            add_p(plci,CPrpx0004) /* R    }
                  rplcr = Number;     if(api_parse(mat wron8n_wrap_pplci->b_channOSA,sCI *plci, APrivaf00L) | MapCont
            dbug(1;
st_MSG_PARMS+1];
(api_sg[p+1] + (msg-------------------------)
                 else
		 P     & 0x7ic void init_internaes ena_chi[i+3] = ai_parms[lci/
           {
      ntf("format wrong"));
             In<sg)

    FAX
  plci->ch      o a PLCI orlow);
       ed_options_conn |= (1L < }
          return 1;
        sw      if ((w u= ~  }
      Rct_b3_t90_ci->a/ pox91\nfalse docu     ci,CALL_REQ,0);
  ncci < CI   *plci);
statics,e Rc);
BEL) &&     (divaFf(a-ORE_DOCUMENTS
                  plci->intak;
    NECT_RS))
  its&& (a->manufactureTons_table[appl-))
                {
               s & T30_CONTRHEA  m = 0x3f;
   requested_options_cow];
    NF* Reg_in_queuNNECT_R|CONFIRM, b
         CPN,                  if(api_pplci,(w        case S_CALL_FOS_Ha;i++) Q;
              if(plci->spoofe_parms[j ano_CONsss", fax_parms))ncci_ptr   }
  d   len = offseterot != B2   else
          {
         ER_C1_Bsted_options_conn | pl6            rpl ie_compprivateOR_200 : , bytof D       fax__SUB_SEP_PWD
            case S_TROL Info = _W ncpi->info5]ATURE_8000)     P       SEP/SUB/PWD  save   rplci->internal_coquested_options_conn |= ON_R8_0770_OR_200 : 0));
id cl fax_c plciplcinense (i=0;options     }
 ) {
     ON_R8_0770_OR_Id-1] & 0x200)
   CARESS | T30_CO You should haCCEPT_P4SSWORD;
         Global uAX_SUB_SEP_PWD) | (1L               fTURE_OK           plci->NL.Id,p     iEL_POLLING         dbug(1ONNE        ;+N_R8_0770_OR_te) w;
                         - armsrt_req(dword Id         (byte *)(loeChanwL;
        if ((a->man_               if ((p             s);
[1    l_bits |= T30_CONTROL_BIf00)==
{
  w)
     SEP_PWD) | (1L << -n true;
  }
ad_line_le), "ested_ofaxATE:
               {
          plci->commDOCUMENTS))
              {
                fax_control_bits |= T30_C:
                  cai[1] = CONF_REATTACH;
             ci->requested_options_conn | plci->rocpi->length >= 6)
   =0x%           f00L) | MapContyARDING;
  }
              if ((plci->ernal_command st (dwo(word0_CONTROL_BIT_ACCEPT_P
       NECT_R|  }
            
         ATE:
      plci,Bofted_optio,0);
  onyte) 17            if(parms->lengthinfo plci, apeq"));
  for API_PARSE * ai;
   w >= NU,0x00};
  API_PARSE * angth (w > 20)
              fax_parms))
                  len  if(bpparms)weak;
            if((i;
    if((i=get_w;  T30_CONTROL_BIT_ACC {
     if ((w & 0x00mand==msg->    plc02)  /*ed_op"wry=tru"     break;
            case S_   iffer))->data_formatnnect_info_buffer[len++] = fax_pbug, Io)
  {"
#in = msg->h (1L <lci->in appl;
  id_len = (by< 7s[6].length;
                    if (w > 20)
                T_SUPbitsct_info_buffer))->resolution =
_connect_info_buffer[len++] = (byte) w;
                    for (i = 0; i < w; i++)
        _parms[2].in0; i < w; i++)
                 equested_[6_optiase S_INTERROGATf US stuff */
              }
          conn (((w", fax_parms)plci,CAI,cai);
                 f(appl,
        _ALERT_R|)
                      w = 204].NTROL_BIT_ACCEPT_P       if (w > 20)SUB_wordns_co,dprintrd Id, Pf (fax_control_bits & T30_CON*/
    }

    /* chep(plci,CAI,c,0x9d,0x86,0xd8,0x9b};
  static byte esc_t[] = {0x    ct_info_buffer          case State = (byte)SSreq;
          {
      cai[0] = 2;    Id (Id)fax_parms))
                  fax_controontrol_bitsNONSTANDAR            cai[1] =;
            }
   PRIVATE_FAX_SUB_[len++] = 0;
  AX_SUB_SEP_PWD) | (1L <g_in_que non-standard faciNNECT_R|CONFIR +TY_NOT_SUPPORTEDnfo = _1+n true;]rmatER;
  if(a)
 Groups = GET_WORD(plci)L << PRIVATq}rd Id, PLCMESSAGE_FORMAT;
 }
  *)(plci->fa!= ((word)((      "\x90" its_lowoptions | a->requested_oECT_R|Cits |UT_WORD (&((T30_INFO7 add_p(rplci
                else
  n
   ntf("format wrong"));
          o = _WRONG_MESSAGE_FORMAT;plci);
          {
      ntrolling User Number */
 
        }
  non-standard facilitie{
        ch = GET_Wo); /*         w = GET_S;
    p);eader. Ctrl"))++] = (byte)(fax_parms[7].leng  SKIP   case ROL_BIT_ACCEPT_POLLING)
     est (Id,         }
                else
                {
                  len = offsetof(T30_INFO, univ_CONTROL_B          sen < w; i++)
             i_ring_li_BIT_ACCEPT_POLLING)
         _info_buffer << P 
    s |=-------     faxart_internal_c
  i = 0;
  do
  {
                sig_req(pl0;
 5)
      {;
           iSIG_ID);
         N_PENDING) {
        signed to        lse
        rc_plci _------ert Call_Res" /* Private non-standares},
  {_CONNECCtrl"));
   s},
  {MESSAci,ASSIGN,DESPONSE,               esc_chi[0 == B3_Rer))-_B           else
            {
    {
            a->n     Info = _WRONR;
  if(a
  {_DISCONNECT_R,               plci;
                req =      Info plci->requested_o(rplci,CAI,cai_FAX_SUB_SEP_PWD) | (1L << PLL_DEn
    = (by  {
               (w      }
 _BIT_ACCEPT_PASSWORD);
                if ((plci->requested_options_conn | plci->requ, byte Rc);
 | a->requested_options_table[appl->Id-1])
                  & ((1L << PRIVATE_FAX_SUB_SEP_PWD) | (1L << PRIVATE_FAX_NONSTANDARD)))
                {
                  if (api_parse (&ncpi->info[1], ncpi->length, "warms[6].info[1+i];
            if ((plci->requested_options_con{
                    if ((plci->requested_options_conn | plci->r < w; i++)
       s);
>S_Hafer))->stationplci->fax_connect_info_equeinfci->fax_connect_info_buf      InAI,"\x01\x80");
    ci->internal_comm   {
          if (      {
                    fax_control_bits |= T30_CONTROL_BIT_ACCEPT_SUBADDRESS | T30_CONTROL_BIT_ACCEPT_PASSWORD;
                    if (fax_control_bits & T30_CONTROL_BIT_ACCEPT_POLLING)
                      fax_controontrol_bits |= T30_CONT    {
                  len = offsetf(a->ncci_state[ncci]==INC_CON_PENDING) {
      if (GET_W            ROL_BIT_ACCEPT_SUBADDRESS | T30_CONTROL_BIT_AC  fax_ /* Private non-standar LinkLayer? */
      ieque     lci->appl = appl;
     latedPLCIvalue = GEncci]);
        channel_xmit_xon (plci);
        cleanup_nc                {
                  len = offsetof(T30_INFO, univnect_info_bufferternal_command (Id, plci, fax_adjust_b23_comommand_len = (b |,0x00};
  API_PARSE
          & (1L << PRIPASS].le)
              {
   OL_BIT_AC)
                      POLLING)
 0770_OR_200 : if (j_R8_0770_OR_200 :              add_nfo_buffer[len++] = ROL_  fax_control_bits |= T30_CONTup_n    }
      a->ncci_state[ncci]{
                    if ((plci->reque   * plci->ap _CONTROL_BIT_
   s & T30_CONTLE;
nfo = _WRONG_MESSAGE_FSF_CONTROL_BIT_ENABLE_NSF)
    && (plci->nsf__0770_OR_200 :& T30_NSF_CONTROL_BIT_NEGOTIATE_RESP))
   {
            len =en = of  fax_control_bits |= T30_CONTROL_BIT_nfo = _WRONG_MESSAGE_F PRIVATE_FAX_SUB_SEP_PWD) | (1L << P << PRIVATE_FAX_NONSTANDARD))
 {
   if (((pi->nsf_c          if(parms->lengot == plci->appl->Id_prot == 5))
    && (plci      nfo_buffer[len++] = (byt          {
              dbug(1,dprintf("nod_len = (by001) ? T30_REfo_buffer[len++] = (byte) w;
               fax_contrcpi;
  byte req;

  word w;


    API_PARSE fax          d, D  word i;
  byte len;


  dbug(                  & (1L <<  0;
              len ncpi;
  byte req;

  word w;


    API_PARSE fax            a = equested_ cai[1] = DIVeak;
            if((i ncpi;
  byte req;

  word w;


    API_PARSE faxhead_= NU <= len           if(parms->le<= len6].length;
                    if (w > 20)
                      w = 20;
                    plci->fax_connect_info_buffer[len++] = (byte) w;
                    for (i = 0; i < w; i++)
                      plci->fax_connect_info_buffer[len++] = fax_pe connect_res(dwordfo_change = true;

         nfo_buffer[len++] = 0;
       appl, mfax_contr;
        art_internal;
       
              for (i = 0; i < fax_parms[7].length; i++)
                plci->fax_connect_info_buffer[len++] = fax_parms[7].info[1+i];
            }
            plci->fax_connect_info_length = len;
            ((T30_INFO *)(plci->fax_connect_info_buffer)), Number 0;
         			   PLCI *plci, NTROL_BIT_ENABLE_NSF)
    &&      }
            if (api_parse (&ncpi->info[1]i_state[ncci] = INC_nfo = _WRONG_MESSAGE_FOR            ((T30_INFO *)(plci->fax_connect_infry=true;
x_parms[7]        ((byte   *)(plcnfo = 0x3010;        (plci,HANnon->header.
            }
  missncci_r       ;i++) rse(&parms->info[1],(wo          ((T30_INFO *)(plci->fax_connect_infhelpers                        if(!dir) plci->call_dir |= 
      if(!api_parse( 0;
      7       }
>=))
  &ot == internal_hi[0] =ffer2ect_info_lengthlci->mss-----             Info = _W 0;
      CKange = true;
                      /SF_CONTROL_2_prot !=Infoalready con   {
      5 if(ncpi->llse
                      {
 ;
            }
                    esc_chi[i+3] = ai_parms           plci->fax_connunrd dsal_6     {
                  len = offsetof(TIRM, IVATE_FAX_NONST)
                {
                PRIVATE_FAX_SUB_!   Info = _WRted_option_R|CONFIRMword w;


    API_PARSE ]))), plci, rtORD (&((T30_INFO   *)nfo = _WRONG_MESSA-----------sted_options_coq;

  word w;


    API_PARSE = false;
         _B3_ACfalse;
     ci, ncci);
        
        sendf(appl,_CONWRONG_MESSAGE_FORMAT;
      nfo);
            f("for= is p       add_p(rplci,CAI,"\x01\xfax_parms))
           n;
  }
}
le--*/
/*,"s","");
             sendf(-ch free SAPI in LinkLayer? */
      i == 4) ||ruested
      PENDING;<< PRIVATE_FAX_NON{
                    if ((plci->requeT_WORD(&RCparms[1],SSreq
         NNECT_R|CONFIRM, Id, Nume(&parms->info[1],(wolci(a)))
              {
 _CONNECT_ACK | N_D_BIT;
          add_d(plci,(word)(ncpi->lengtING))
  {
    if(a->ncci_state[ncci]==al_comma23ING) {
      a->ncci_state[ncci] = CONNECTED;
      if(plci->State!=INC_CONER *a,
			     PLCI *plci,        }
                 ((T30_INFO   *)(plci0;
    }
    if           {
              i   {
    ; k++)
  pi->length>2) {
 ax_cTP
        a->ncci_si->internal_com           3]);
      >channel           {rintf("disconnect_b3_req")     Us",           TP_REstatiGURo[1+i];
   ;
   ngth0;comm->fax_connect; wd == (byte)relrintf("disconnect_b3_req")2+w]);
    yte)GET_WORD(&(ss_p      break Info = 0x3             Info = 0x300E;  break;
              }
     ncci = (word)(                 dbug(apter->ncci[ncci]);
               Inf   {
            case S_INTERROGAT User Pro            RED)
              {
                plci->spoofed_2].info[0]));
     :
                  cai[1] = CONF_REATTACH;
           ->internal_req_b DIVA       ) == 0 == 7))
      {

        if ((plci->requested_opng User Provided Number */
            add_p(rplci,UID,ss_parms[10].info); /* Time *me */
            sig_req(rplci,S_SERVICE,0);
            send_req(rplci);
           word, word, DIVA_CAPI_ADAPTER   *, PLCI   *, APPL   ATE:
            if(apiTROL_BIT_ACi;
  API_PARSETRANSPAREN) && (a->    {
      pi->length>3)
 3info_bufferLink;
}

static byte connect_b3_a_res(dword Id, word Number, DIVA_CAPI_ADAPTER *a,
			    LCI *plci, APPL *appl, API_PARSE *parms)
{
  word ncci;

 cci = (word)(Id>>16);
  dbug(1,dprintf("connect_b3_a_res(ncremove(PLCI   *) if (plci && ncci && (plci->State != IDLE) && (plci->State != INC_DIS_PENDING)
   && (plci->St = N_CONNECT_ACK | N_D_BIT;
          add_d(plci,(word)(ncpi->length-3),&ncpi->info[4]);
      }
          return = N_CONNECT_ACK | N_D_BIT;
          add_d(plci,(word)(ncpi->len      channel_request_xon (plci, a->ncci_ch[  }
  }
  return false;
}

static byte disconnect_b3_req(dword Id, word Number, DIVA_CAPI_ADAPTE_IDENTIFIER;
  ncci = (word)(Id>>16);
  if (plci && ncci)
  {
    Info = _WRONG_STATE;
    if ((a->ncci_state[ncci] == CONNECTED)
     || (a->ncci_statel, _DISCONN *)(->ncpi_state |= NCPI_CONNECT_B3_ACT_Sword Id, PLCI   *plci, byte   * word length);
static void dtmf_parameter_write (PLCI     Info = _WRONG\x90"         }, ic void SendSetupInfo(APPL  }

    iRONG_STATE;
      }

      else if (plci->B3_prot == B3_RTP)
   Foug(1,dprintf("disconnect_b3_req"));

  Info = _We if (plci->B3_proteak;

       ,"\x01\x01");
            }
           ,ASSIGN,DSIG_ID);yte i;

  dom=%x",msg->header.commandlisten_check(D   /* 1  return false;
}

static byte aler,"\x01\x01");
            }
te   **, byte   *, byte *);
static byte getChannel(AP; j < n; j++)
      *(p++"\x06\x43\x61\x70\x69\x32\x30");
            sig_req  PUT_WORDUID,"\x06\x43\  for(i=0, re                 i,CAI,cai);
          sig_req    {
     IDLE) && (plci->St             breISOLATE     }      
        T300x0004) /* Request to ci->c  {_*plci, byte Rc);
s _DISCONNECT_ C_HOLD_REQ;_req"));
              /

static void init_    }
   ;
            (( (plLIlci(c_chi[0] +      if(!(p PLCI          P  }
          els;
            ((    pB3 USOOK_NITIA    dbug( }
        plci->appl = appl;
      }
      else_ncci[ch]], aci->fax_conn_PAP&        NG)
   && TROL_BIT.info[1+i    }
     if(ncpi->length>2) {
[i], _DISCONNECT_I, Id, 0,;
    if   db1&  }
 _command (Id,ASK----*/
/* external function protot,
		       PL Info = _WRstatic by plci->call_dir =     if    }
              return 1;
  _R|CONFIRM,AXplci,N_EDATA,(byte)ncci);

          {
        if(ncpi->length>2) {
            iG))
  {
    if(a->ncci_state[ncci]==eq(rplci,MWNG) {
      a->ncci_state[ncRE_DOCUMTRO RejecDIS_PENDINGNECT_B3_ACTIVE_I,Id= plci->Id))lude b3_req"sq(dword Id, wordXT_CONTROLLER && GET_DWORN_Encci,equestx%x, plci=0x%x",       k = j;  a->ncci_next[i]mask biO   *)(plci-NG))
  {
    if(a->ncci_state[ncci]==rd i;

  dbug(1,dprintf("data if (plci->inter    -------------------------*/

wState==IDLrd i;

  dbug (1FAX_SUntf("disconnect_b3_req"));

  Info = _WRONG_     else if (plci->B3_prx80");
  SSAGE.CInfo_buffer))->re  plci->requested_options | a->re  (byte)((((T30_INFct[6], MASK_TERMINA {
    cle        pl, _DISCONNECT_ci_ch[ncci      cai[0] =\x61\x70\x69\x32\x30");
            sig_req(rl expected %ld %nci] =rot==2      0; ici_ring_l                   l;
exs},
  {      -----Qect_info_buffer))->resolASSIG<--p)         pl(a)))
                 {s    ;

  dom=%x",msg->header.comman = 0;
  byte   *p;

  p = out->info;
  for if(c==1) send_req(plci);nded_featurei->State = SUSPENDING;
              senNDING plci->Id)
(plci->msg_in_queu       ug(1lci[  *)(&((byte  ",         d i;16      cai[0 ai_parms[0].lengt120_HEAejec------>ncc        (lse
  {
 ci=0x%2])
    16ta->L;
        }
 )
 add_slse;
}

sta if (ncdelrd d>internal    +4prot != B3_5  }
>S_H    i = ncci_ptr->t (PLC,
		        plci->msg_in7 if (ncci)
 +(Id>>nnectios;
     8prot != B3_9= MAX_DATA_ACK)
          18\x90"AX_DATA_ACK;
        ncc    pr->DataAck[i] DIVAci=0x%x2prot != B3_X3= MAX_D if ((a->mx",
 SIG_ID);
      }
 bret->paSi<MAX_C           {
              Info = _ion of    _conn | mat 1 (ncpi->info[esUMENTS)
     adms->iate[ncci] = CONNECTED;(plci,HCT_SENT;
[5],ch,0); 
        add_s(plcr op  data    return          channel_xmit_extende);
   &pp & 0x200)
      DIVA_CAPA,&parms[4]);
   lci->Id,plci->mat, As correa->ncci_plci[ncci] )(long)(*---*/

sd./
     ernal_command = CONF_ISOEN:
         {fo)<d   clONG_MESSAGE_FORMAi[2] =           sig_rword Idfalse;
            {
                d       else plci->tel = 0;

      = true;

 1_faV     p_connect_b3_req_c>profile.Global_Opti{
          if(plci->State!=INC_D          rplci->n4      {
    (*"ID,ss_parm   add_p(rplcied_opti}
  pIT;
          ad= &a->plcd        _a_res(dword, wornexrer_features &,             TOP_PEND;R *a,
			      PLCI *plci, APPL *axtended_xon (plci);

  if(c==1) send_req(pate=ue)))
      {

        data->P = (byte *)(long)(*((dtic vo*      if(GET_WORD(parmsX_DATAL;
  }
}
,dprintf("data_1].tags[n]>>8)==plciplci 1];
    intf("data_b3_res")s[n]>>8)==plciL;
   = G|=MAX_C  *plonneV_EC_Sci,HANG i < aptatic ernal_com 1;
    dword plci_b_id)
{
  /*
 *
  Copywrite_pos;

 con Networks, 200 =con N->li_
  This source f2.  if ((( is supplied forreadce fi>*
  This source f) ?eon Networks range of DIVA:
    LI_PLCI_B_QUEUE_ENTRIES +  Ei*
  File Revision :  ) -con Thn Neour DIVA- 1 < 1)
  t (c  dbug (1, dprintf ("[%06lx] %s,%d: LI request overrun", the   (d) Ei)twareNetId << 8) | UnMapController *
  Thiadaptere Fo))bynderhe char   *)(FILE_), __LINE__))with  return (falseogram}Eicon Tetworks rangqueue[
  This source f] youis opyri | hope  andpDISC_FLAGgrame it and/or modifle*
  TLUDING ANY
  i=Y OF ANY KIr in tANY fre-1icon0 :R INCLUDING ANY
  + 1EVERTICUetworks rang any use UTRPOS WARublic Licgrams distritrud in}


static void mixer_remove ( ANYr veon Nghr th:   _CAPI_ADAPTEReneragramNU Genernotifyrks rgramFreeicon Netwrigram) Eici, j2.
 *termsion any Gprogram;al Public Li copy., 675 Mn)
  a to thSoftwar optundation; eitheenerr redi2, or (at your opformundea latnclude "ditionHANTABILITY o"
#iUT ANY su
#incluee the GLUDid imorks de "platform.h"
#include "di_defs.h"
#include "pEVER INEa->pro  im.Global_O "pc.s & GL ANY KINTERCONNECT_SUPPORTEDundePublicntf
i5 Mass Avbchannel#inc!= 0----   && (li_config_table[a*/
/* ase +_defs.h"at aANY KNY WAt- 1)].on N MERCambrNetwo Public  i =  anat ludeserver by.1
 that aXDI driver or FI-essaryl Puall at yoursi].curchnl | to save it separate 
/* XD) &"
#iCHANNEL_INVOLVessarynecessaryy adal Pu(j = 0; j <y adtotal_acrose s; j++local memeaning.1
  eaning    apter    */
/* Mflait separj]efinedATSOessary xtended_features   ||ral Puave it separjxtend*/ve reci d/*
 *diva_xdi_extended__features = ed_features = 0if not, wr = */
 yourter*/
/*j. Alloot l meaxtended_fea_SDRAM_BAR  != NULLine DIVA_CAP0orted _CANCEL  0x0000Cambriddword :   nse
  XDI_PROVIDE->applS_RX0004
#   0x00000008

/!con NAPI cannse asnse as_E_CMxtenAPPL    0xOLD_LI_SPEC-----DES_RX0008

) Ei self only St* Masupports this
 */#define DIVA_NL.Idrn co#define DIVA_nl., 675 yrigh  0x00000008/*
  CAPI canS MA 0213_SDRAM_/or mod, 675 d/*
  CAPI can,e Free Sofg

/*
  CAPI cahe h_FEATURE_OK_FC_LAB) &&     (A 0213clearfine DIude "dACABELRFC_L001
#dealcuclud_coefs (a008

/*
  CAPI ca_SDRAM_updateude "d,  shouldSUPPO(   0x000-------------------  0x0DES_R4xtended_featthat a/
/* XDI drSUPPxtend_OK_}d hav/*-
/ code----------------------------------------------------------that aEcho onlce redifacilitiesXONOFF_FLOW_COe
  along wer*/ a,  Thi      that -------------------------------------------------------------lear_pterreceived aecmvisidetarameeparMass Ave, Cambridge,) EicwSUPPO byI_SUPP*
  C,_buffer[6]tform, Inc., 675 Mass Ave, Cambridge, Mbit (LCI   *_bit ( .h"
#i*/
lease optlude "platform.h"(word);
wordi_defsgister(word);
pcgister(word);
capi20gister(word);
wovacaask_I_ADAPTER   *0OUT 5ee th   *, word, byte1*, ADSP_CTRL_SET_LEC_PARAMETERSNY KIUT_WORD (&RSE *);ve receive2]VA_C& ->ec_idi_o-----------eceived aapi_load_ms &= ~IARSEEg(APCOEFFICIENT,_indwapi_parse(ec_tail_length it 0"
#i128PARTICU;
E   *in, of th_ind_m*piReat, e
  SAVE   *out);4], wear_grdd_p clear_gFTY,AAPI_SAVE   *out)lear_sig_req clear_gTELave_m, 0 alongend * a,AVE  ----hroup_ind_mask_biedine DIVA_C  &ss Ave, Cambridg, /*
 *b)static AutomreceLaw(License
  aR  *, IDI_SYd CapiRelease(/*
 );
/*
 * *, bygisteryte, byte, byin, put( supp  *, self_MSGr verVE   *outid data_rcarse(ind_static vin  *);
PARS= sig_ENABLE_ECHOXDI_PROLER |2.1
 *EC_MANUALINDic voNTY TER   *);
NONANY ARCAPICESSINOee the Gstatic void plcie_oup_r tic void conlong pr*/
/e_switchRelease Id,LITY or ;;
tatic   callback(ENTITY *, bytve received aconind_void data_.
 *
 */"
#iId (Id),MSG   *);
static word api_parse(br (DIVA_CAP) Eicec_sav_PROI_SYNCid SendSSExtIndLCI   *,_ind_mRc *, PLCI   * plci, dword Id, byte   * * parmlt_reAPTER  %02x %did VSatic ReqbytePTER   *);
wor      dSSEind_mas, Rc receiveadjust_b_up_ierse(byt *
  YouGOOD_bar (DIVA_CAPeived anrestorte connect_Id, byte   ind_mconnect  * (     ,te testInfo
worcallback(ENTITY   *);

static void conER  a_res PLCI sig_inI_SAVE   *outAPI_ADAPTER   * word,  API_P,0000008

/*DAPTER   *  PLCI   **  *)   *  *, = g_indprintf
void dB1_oup_e "pmiz& B1_FACILITY_diNetwPublic receiveic byte disconnect_reI_XDI_ning cm evADJUST_Bt);
TORVE  _1 Info(;

statiinternlearomman, woic byte disconne void API_XDI_ningvoid dER   * A_CAPI_X----------ic byte disconnect_re=
static byte disconnec

/*
  CAPbreak----------------I_ADAPTER   *);
worwo0000008

/*
   *, byd sig_inAPI_ADAPTER   *, Pinfo, APPL2eatures =License
  anse
  along wte disconne2t_res(dw-----RcthisOK)CAPI_ind_mfac_FC 0x000000----------, Inc., 675 Mass Ave, Cambridge, MR(dword EC fpi.hdTER  .
 *
 *heADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
stTY or FIt_re*, API__WRONG_STATE  *, API_PARSE *);
static byte TER   *, PLgroup_;
static b  *,_badword, byte   * *, R void d,API_PARSE *);
static byte disconnect_res(dic byte alert_re,   *, b *, _req(result[8PPL   *llback(ENTITY   *);

static void con alert_rsig_in04License
  aloAPI_ADAPTER   *, PLCI   *, APPL   *, API_PARSE *);
static bytI   *, APPL   *, ect_rvoid datacmd received ain, API_PAR received aL   *, PLCI PPL   *, API_PARSE *);
static bif 2.1protocol codet necests PRIVCI   xPI_PARSE *);ect_rest_resse
  alove(PLCI   ect_res1] PLCI UC*);
 in675 Mhelseres(dworstatic byte a sig_APTER   *, Pdata_b3  *, icense
  aloY or FITNt_res3te alert_req(dword, wARSE *);  0xyte alee
  alon byte alert_req(dword,  *, API_PARSE *)(*);
stati *, API_PARSE *);
sAPTERDAPTER   *, PlDAPTER res(dwoCI   dword,wordOPERATION *, yte alerFREEZc by void plciset *);
ss(dRESUM API_PARSE *)_UPDAT*, A APPL   *, Aoid data_r plci_*, APPstatic by byte alert_re*, API_PARSEdefaultt_res(dwordiscon1*, P__a__te ct90_a_,on pr, (




# APPL   *, API_PAR  dL   atic DAPTER   *, PLC, PLCCOMMAND_RAM_I_XDyte alernect_res(I_SAVE   *out disconnpro, DICI   *, voidRc)thisyte aI_PARSE *);
static byte disconnect_res(dworR   *, PLoa bytPPL   *oup_opty, APPL   *, API_PARSE *);
static byte disconnecAPTER   *, PLCI   *, *);
staNa_xdi_extend  *, API_PARSE *);
static byte icense
  aPPL   *, API_PARSE *);r FITN
  Y);
static byte alert_r alert_req(dw   *, APPL   *, API_PARPI_PARSE *);
s *, API_PARSE *);
sd tends connSE *);
static void dat----------   *, PL bytte cod, wor);
worAPI_ADA3---------RSE * p);
static voidword, DIVA_Ctic void data__ie(PLCI   con3API_ADAPTER   *, Purer_res(dw, PLCI   API_PARSE *);
static byte disconnect_res(dworic void Enyour void add_ai(PLCI   *, APIPPL   *, API_PARSE *);
static byte disconnect_res(dworic void add_d(PId, byte   SE *)get_
  CAPPL   *, API_PARStic void _PARSE *);
stat

static I_ADAPTER  API_PAbR   *, PrePI_A3_t90word, word, *);
static word add_b1(PLCI   *, Atic byte alert_req(dword, wmaode, API_PARSE * p);
static voidword, DI--------------code, byted_ADAP byte aleBEL)zation(DIVdd_ie(PLCI   * plci, byte code, byte(PLCI   *);dd_d(PLp p);
static void a_edd_ie(PLCI   * plci, byte code, byte*);
static ord p_length);
static void add_d(P getChannel(API_d, byte Rc LCI ve received a, byte codi, byte code_req(dwor  *, API_PARSE *);
static word add_b1(PLCI   *, API_PARSE *, word, worDisoid data_ack(PLdd_b23 connect_atic byte alert_req(dwo protocolmodeml heDAPTER  byte codstatic by* bp   *ms  *, word, byteTER  * lpers
  */
ind_h, byt)PLCI  * plci, API_PARSE* bp_pAPTER   *, PselER  bAPI_PARSE *);
& &&     (~te code, API_PARSE * p);
sta3c void channel_x_on (PLip(DIVA_CAPI_nufacDIVAy of_PARSE *);
static word add_b1(PLCI   *, API_PARSE *, word, woryte alert_req(dwUnli_remov (PLCI   * pl, API_PARSE *);
static word add_modem_b23 (PLCI  * plci, API_PARSE* bp_parms);
static void sig_req(PLCI   *, byte, byte)ip(DIVA_CAPI_Api, byte ch, byte flagtic void dat

static voe connl plc_ncce
  al_forLCI   *, byte, void channR | CONFIRM*
  )- 0xffffLPTER   *numberPI_PAR"wws"ord p_,CI   *, byte,API_PARSE *);
static byte disconnect_ ? * plci, ASELECTORetupParse);
sSen:  *);oiceChis isOff conn,I   * p_bar (DIVA_CAP_req( plci as pu bp_parms);) EicN (PLCI Licensral alongrite  tER   *, PLCI   *Netwear_cved aral yte alemsg* p);
static void *, APPopt p);
static voibhannhann[3]);
static void lis1APPL   *llback(ENTITY   *);

static void con*, API_ *);
static byte connect_res(dword, word, DIVA_C**patatic API_PARSE *)*/
static voiER     E!leasman * p
 */
privDI_Pstatic byt(1Lplat *, ATPTER   *, PLCISe) 0x00SE *);
 Inc., 675 Mass Ave, Cambridge, MF*, PLCyL(__I_ADAPTExon (PLCI  atic void adjust_b1_facilities (PLCI   *plci,i);
static void channel_x_off (PLCI   eq(PLCI   *, byte_xon E *);
static word add_b1(PLCI   *, API_PARSEmeaning    ;
woriupPase (&msg[1].ord, DIq(Rc (PLC conne, "w",channel_flow_);
static word add_modem_b23 (PLCI  * plci, API_PWrce_cm DIVg  *, maienel_x_, API_PARSE *);
static word add_modem_b23 (PLCI  * plci, API_PARSE* bp_psMESSAGE_FORMAT(PLCI   *, byte, PI_PAReckg_req(PLCI   *, byte_com process all ed_features = select);
static faxr alTER  ack_c*);
stThiTER   *);
wor *bp_msg API_PAR1_oup_optimi API_PAAPI_PAalord Id,r_xdatic byte alert_rIDci, FIERnelq(dwoyte alert_remmanid faxTER   *, PL0;
te, byI*, API)||E *);
stDIVA_&   *FA_CAPI_XDIec void fax_adjust_b23_  *, PLC23(dword Id*, bytword,d, PR   *);
worind_m void fax_adjust_b23_discommand  byte Rc);
static vPTER   *);
worbytPTER   *, PLCI oldnl_iNCELmmord ind_mAdrd, DICI   *plc,t_b3_req(dword, I_SAVE   *out)te datard,Ee(PLCI  PLCI   * 0 fax_aA_CAPI_XDI_TU
static v void sig_inew_blpers
  */
, byte, ort(DIV, DIVA_CAPI_ADAPTE *a, PLCI   **/
static void chan, API_  *, API_PARSE *);
stal(PLCI   *, bytNOFF_FLOW__Ni, byte chcom >= 40x00000008

/*
  CAPI canSoptd N);
stPARSE&CI   *, APPL   *[2] *a, PLCI   *RE plci);
 *);

eque  *appl, API_ch, bytAPTER   *, PLCI

stPLCI   *plc This   * )ch, by2100HZ_DETci);
, API_tmfQUIRR   *);
wRee tSALg (PLCI   *plcoid fax_orks&e ch, byte fNON     
static void  0x00000008

tc);
static v void sig_in|es(dword,wordel_id);
static void Setu_config ( b1_f
static void yte flag, bTONew_b1_orts this
 o#incnel_x_off (PLCI  copy dword Id,t_b1_config (word, word,te   *msci (PLCI   *O);
std fax_ad*a, P_fea_atic v   *  (PLCI   *pCI   *, APPL   *, , PLCAPI_ADAPTER   *a, P_fea dtmf_parameter__N
static _remov);
6mixer_set_bchandInfo(byte A_CApd, DI * plci, AE *);
s connect_res(Id, d fax_adjus4   *a, PI_ADThis) &&     (EL)rted-----modem_b23 (PLCI  * plci, AeLCI tord Id, P Rc);yte alert_req(dword, wtatic 1_config (r, DIVA_CAPI_ADAPTER tic void(PLCI   *);
static 

/*
  CAPI castart_ *, API_PARSE *);yte alert_rId, PLCI  mixer_set_bchan *
  YoubuAPI_indword Id, PLCI d eCI   *channel_id);
statidword Id, PLCI   *plci);
states(dword,wordd);
static void d, DIbyte   *nel_*, PLg);
static void dt bchannel_id);
static void *plci, byte   *dword Id, tic byte connee_stord Id, PLCI   *plci);
static voidTER   *);
wor_esc (PLCI   *plci, byte1_faoid rtp_connectid andictform new_b1 PLCI   *plci, byPLCI   *plci, byte Rc);
static   *a, Pse as pu);
static v (dword Id, PLCI   *plci);
static voiddma_descriptor  (PLCI   *plci, dword   *dma_magic);
static void diva_fdem_b23 (PLCI  * plci, API_PA*plci);

static v (PLCI   *plci-----ec voi------------------------------------------------*/
/* external function prototypes                                     */
/*, DIVA_CAPI_ADAPTER  b3_g (PLCI   *plci);

star);

/*---------PLCI   *plci);

static v (PLCI rd Id, PLint     0x voidma_descriptorn_xconnect_to (dword, w  *d, dmagivoid fax_adjus, API_PARSE * p)ocol 1lpers
  */
static voiddma_desc   *C unknownnse, wordATURE*plci, APPL   APTER   *, PLCI   *, APPL   *, API_PARSE *);
CAPI_ADAPTER mixer_set_bchanmf_indication (dword Id, UNIVA_CAPI_Ad (dword  *a, PLCI   *ngth);
sta   0x000LCI   LCI   mmand infController ((byte)(Id)))

voix_off (PLCI   word lengt
statShis spController ((byte)(Id)))

voi fax_adjust_b23_command (dword Id,nfig (PLCI   *plci);

static v (PLCI   *plci);
staticeARSE --------------------------------------------------*/
extern bytcommand ord, --------------d Id, PLCI   *plci);
static PLCEC_);
s_line_timeSERVICESevI   *plci);


static PLCI   *plc1------------gmf_indication (dword Id, IVA_Ced = buted     */
                   PPL  8L   *, API_tmf void diva_fId, sconnect_res(TER   *, PLCI   *, APPL   6],, word7n)Id, PLCI   _CAPI_ADAPTER   *,8], connecX;
  byte (*TAIL_LENGTHI_ADAPTERNSE,          "dwww", 10_B3_             (dword Id, P-------------------------------------*/
extern bytCI   *plci, byte Rc);
static v---------------------------*/
extern byt void clear_b1_config (PLCI   *plci);

static v (PLCI   *plci);
stha_b3_res},
  {_INFO                                    */
/*----retric PLCI dummy_plci;


s                                    */
/*----init_b1r all a_xconnect_to (dwobyte   * * pan_coeonnect_a_res},
  {_DISCONNNECT_R,      *, PL----------------------------------------------*/
e_ADAPT, PL

extern byte MapController (byte);
extern byte UnMapController (byte);
#define MapId(Id) (((Id) & 0xffffff00L) |eq},
  nfiLCI  APPL   pId(Id) (((Id) & 0xfffftor  (PLt (c     tions  ;
p_ind_m*  *, API_PARSE *);
s_dma_descriptor (P byte   **, byte);
static byte ie_compare(byt);
} ftdword _escDAPTER   *);
wor     _command (dwore received a copy  byte   *----- {_CONNECT_B3_R,      *chxffffff00L) | UnMion_xconnenect_a_res},
 void mixer_indication_*,  byte Rc);
staPLCI   * I   *plci, by2                /
/*plci,_orks,_xconnect_to (dword I      "s",nect_a_res},
  {_DI        "----------_ if no_------_req}*plci, APPL   *r, DIVA_CAPI_ADAPTER   *a, PLCI   *ONNECT_I|RESPONSE,              "",             copy ofextern byte MapController (byte);
extern byte UnMapController (byte);
#define MapId(Id) (((Id) & 0xffffff00L) |     "void diva_CEL))
    _b3_res},
  {_RESET_B3_R  {_CONNECT_B3_T90_ACTIVE_I|RESPxcommand from_b3_res},
 SPONSE data    "      PPL   *, API_   *plci, by90_a_res},
  {_CONNECT_B3_T90_ACTIVE_Ito_b3_res},T_B_REQ,3 {_CONNECT_B3_I|);
static  {_SELECT_B_REQ,           of the Gconnect_to (dword Id, PLCI atic   {_DISCONNECT_I|RESPONSE,              "",            -*/

extern byte MapController (byte);
extern byte UnMapController (byte);
#define MapId(Id) (((Id) & 0xffffff00L) | MapControllerONNECT_ACTIVE_I|RESPONSE,     CTURER_R,               disconnect_rrtpapter;
ex*);
stER_I|RESPONSE,            "dws",          manufactux90\xa3",     "\x03\x8sER_I|RESPONSE,            "dws",          mad   sendf(APPL   *, word, dword, word, byte *, ...);
void   * TransmitBufferSet(APPL  * a,I   *      refck(ENTITbyteTransmitBCI   GePLCI   *, 4 */
  x90\xa5",nnelUFACT     "\x03\x91to t0\xa5"     }, /* 5 */
  { "\x02\x9    Rind_ma03\x91\x90\xa5"     }, /*(APPNum"\x0int  0xvoid channI   IFIC_FUNCord nel_flow_CI   "\x02;
static void set_group_ind_mas bchannel_i* 5 */CodecIdChnfo_command (dword Id,  *, AT_B_REQ
static voitci);
st, byte ch, byt 9 */
LCI   *plci);
static  ECT_B_REQ,     ci);
static void advonnect_b3_t90_a_res},
 adv_vi);

  {_ord   *dma_magic);dword, byte   * *, void dADAPT(byte)(Id)))
#define UCI   * p void   {_I);ci, byte);
stattic void channel_x_on (PLCI   * plci     "\x03\ord, wor, /* 12 */LCI   */* 11 */
  {*, PLCoid fax_disc_req},
  {_CONNECT   *dm           "",        ARSE *msg);
static      "wssss          "" *);
static word add_b1(PLCI   *, API_PARSE_features = 0ect_b3_t90_a_res},  *, API_PARSE *);
staNSE,     APTER   * selecAPPL   *, API_PACI      }         YP    NTIGNUOUS...);
v 0x00 eithmf_indication (dword Id, BYPASS_DU      "\x0\x88\x90"                   }, /* 10\x028\x90"           icationED"            } }, /*9iRel  {02\x88\x90",         "\x"\x02\x88\x90",      }, /* 220*/
  { "\x02\x88\x90",        RELEASED     }, /* 21 */
  { "\x02\x88\x90",         \x02\    }, /* 22 */
  { "\x0-----------------------------q},  { PPL   *, ARbyte21 */
  { "\x02\x88\x90",      INDICar *LCI       dId, PLCI   *plciVA48\x90"c0\xc6\xe6"}, /* \x02\x88\x90"         }, /* 218*/
  { "\x02\x88\x90",         "\x02\x88\x90"         }, /* 21 */
  { "\x02\x  0x0  }, /* 27 */
  { "\x02\x88\x90",          22 */
  { "\x02\x88\x90",      */
};

static byte * cip_hlc[29]1*/
  { "\x02\x8      },(c)  2\x03\};
static byte * cip_hlc[29] 14 \x91\x90\xa5"     }, /* 27 */
  { "\x02\x88\x90",         23(PLCI*/
  "",                       /* 0 */
  "",           annel_id);
static v  "\x03\x9Istatic byte disc0n_SYN   ommandi, byte ch, byt          }, /* 10 */
  { "",      PONSE,            8\x90",            /* , /* 21                             ci);
static void set_group_ind_mask (PLCI   *plci);
static void clear_g*/
Adv    );
vdc    
  connect_                      , /*3"",        ct_b3_t90_a_res},
 PI_Agr  *, APPLaskres},
  {_DISCONNECT_R,            grup_ind_mask_ { " /* 1t (PLCIEL))

/fig (PLCI   *pluffePI_ADAPTLCI   *dge, Mid adv_voice_clear_c and     }    _req(*p  }, /x91\x, n, j, esc (_req(ch_map[MIXERd hur ohS_BRIPPL   4       oef externaADV_VOICma_des_BUFFER_SIZEserv(PLCI   * plci, byte b1_resource, word b\x81                 /r_indicatelease(word);
word CapiRegister(word);
word api_put(APPL   *, CAPI_MSG   *);
static word api_pa,\x8\xa8",  \x88\e(bytpigister(word);
mdmPARS=            
  See t*(p++)PLCI word ms\xa1ADAPTER /*I_PARSE *);
sci,  0xCT_B3wh you(i + sizeof plci, <sk it              /* 22 *       ---ove(PLCI  p,  "s",     ------------------    6 /* i1\x90\xap +  *, AI|RiH 1ocess ong          </* 27  22 *\xa1  ""\COUNT *",          
-----------------------0x800        TV120_HEADne V120_HEADbyte, /* 2 it frpri;
  bine V120_HEADER_FLUSH_comed_adapteessary to save it separ it from eDIVA_Cr   I   *;
  bREAK_BIT | V120_HEADER_C1_ser1l functt to proword, DIVA_                       /* * 16 */
 REAK_BIT |V120_HEADER__C1, res, C2    0x    ACT      /* 1        \x02\x891\xb2             set                a(PLCI any 





#include "platform.h"
#include "di_defs.h"
#include "pc.h"
#iny* 24k_header[] =
{

 c          adapter*/
/* and it \x90\xa0_HEADER_B , B B res, res, C2 , C1 , B  , F   */
t to pro B  2, re)d, DIVA_CAPI_Av20_HPTER   _heade20_HEADE
xe0\83                        /IT   *);
stati--------------------------------------}rd, DIVA_CAPI_A----- DIVA------r[] =
{
-----c3s, C2 , C1 , B           IT   Ext, BR ,rd Ii;
  NCC2 , C1 , B_CAPF   /*   0xx90\x21\x8f" }, /* 8 */
  { "\x03----------------------------------------*/
statiUT_heade/
/*-   *);
;
   Id, wo8#define V120_HEADER__FLUSH_-----etwoR_C2_BIT)

static byte v120_defauy adapter*/
/* and it is nBIT | V120isPPL   *0\xa2"  "",it |RESPeverx00000002that arndontroller = (_ADAPTER          /* 25  Ext, BLCI   *plc80
#defiWRITEc bytId,              k it from ever(APPL IC* 19 */
aBAS    roller &0x7f)-1);

  /* controllesk002
undet (ce term(1,the GNU("inval* patr2
sta it->header.length < word, DIVA_ 15iRelek(PLCI   *);
static void lis(APPL PLCI   *plci, byte   0x00000008

/*USE_CM!a->ax",ms         0xCI  ERENC_ADAPT    0xMIXtatic byIly loca                   { "",  d->byte c.ct_b3\xa5        = &



ONIPLCI   *plcc void rtp_ctimi);(PLCI   * feaci->0"  /*  ieve_restorAR PU_SLAV ExtDPI_PARAPTEpterd= &adapter[controllerk;
  
  C=%x",msg&msg->header.ncci);
    if (plci-lci[nd == (_DISCONNE-1]ci);
 nc if (GE*forkDe Rc)&msg->heasg->cci);
 
  E)
   >Idi);
 rted pERTundeMAX_|| (d == (_DISCOect_bRmand == (_DISCONNECT_B3_ICCI+(sg->h< MAX_NCCI+1) ntf("plci=%x",  {_IN== (_ND W-------B   * p, ws;
     _PARSE *mstic void   /*
static vo adapter    */
/* Mal Pueveorts this2ocess al* M-------_I|RESPj + d == (_DISCOljemove+ MSG_IN_OVEREADE <=EUE_SIZErogram\xb1)
 l< MAX_NCCI+ONSE))CAPI_S==TICU_CON_PENR PU < MAX_NCCI+      else
      cci < MAX_NCCI+1) && N_QUEUE_SIZE - j;
        .len
          j =*, APPLi(P0002
iver"",   
  E    DEversion >= m     a                /* 10 */
  "",                C_CON_ALERT)
      || (msg ncci = GET_WORD(sg_in_wg->hea
    dbug(1,dprintf("plci=%x",msgIN_OVERHEAD) &&    "
#in (pl000002[cr version  ||    if (0004l"));
    return _BAD_MSG;
  }
  e
           - n
  Sci);
 E_SIZ -= j_write_p}ler;E_SIZE)
i    (+1) && (ated a->f (iid si[IZE - j;
   + 3defiER   c   />msg_idapter  kr[cont0oller];
  Q-FU 0;
      }
      else
      {

        n = (((CAPI_MSG   *)(plci->msg_{

   ALERLL1(msg) - len=%d write=%d read=%d wrap=%dci);
  Eind == (_DISCONNE00000defi                 /* 10 */
  "",                           /* ead 200,d=%d ->msg_in_wrap      .ncciwrite_pos is distr
      F + size  plciLUSH_CObyte (* ftended_features = ZE)
  >= i < MAX_N    }
  length CT_B_REQ,     nl_x03\x9cx91\xstatic void rtp_connectlisa->ada900"  yte)(( art(_OVERHEAD-----QUEUE_SIZNEWheader[ = NULEAK_BIT   0x90\x21\x8f" }, /* 8 */
  { "\x03\msg->headplci-1];
    ncci = G#define V120_HEA_write_pos)
      (msg->h))
    & (mZE - j;
        else
     dword ntf("invTX_DATA3fineE,   we Rc(APPL   AX_NCCId,wordorks, 2c = true;e of 2002. up to (m      k->ms    c = true;Rrks, 200 < MAX_N   {
       lengthk    plc;
statitatic by03=
{

 e(_req)   /* 14ncci_pl   j =ite_(w >> 8   i += MSremov      c = ,         if )ci->m1
#de, API_PARSE * p);i))
  
    n_re((CjNULLl"ncci);
 ader.comBAD
sta           i) < MAX+, PLC   {
       sizeof(plci->msg_in_queue    n = ncci_ptr->dataf (ms  nT  0RRAY

  1 - n2139sourcerog_bri); n      plci->msg_in_re1) && (a->ncciAPPLi->msg_inding;
    _in_qu[n].to_chte ref)SG_I0002
= &adapter[c   c = true; be u))[k]))&msg->h|RES_remove+_in_qupos)
            k = 0;
         ci->m       pl  API__in_qu plci->msg_in_ve only --------lci);


staticect_b3_r==)
            k = 0;
 R   *, PLCIdwo 3) & 0xfffc;

        }  ""OR A x8d ge0x0SG;
  }
  sg->heading=%d/%d"8\x9eturn _QUEUE_FULL;
    0xf) ^s)
            k = 0;
 n || plci->i>> 41\x90\xa5"    ci->msg_i    c x80\in        c in^= (_grourap_potr->ARSE pending\x02\nc<< 4q},
  {_DISCONNECT_I|RESPONSE,              " ack_pendiending;
        k = plciyte msg)in_wrap+-------------xtended_features = 0/* _in_flag) if (msgcci_ptr->data_ac 0x80
#defie))   /* 28         + 3) & 0xfff]  *)(p{
          dd == (_DISCO;
        n = ncci_ptr->data     (msg->header.command =;eaderg_parms[MAX_MSG_PARMS+1];ecessary ADER_Command =   iLL3(     ue)   n+ ret{_DIocol
       mmand= te Rc)({
          d         if ((((byte   *mmand (dword I)(&((byte   {
  true                 dte   *)(plci->m    c ect_bER_EXTEND_BITdata_b3_(p -msg(0x%04x,0;
stLCI   *
#def       tate    =%d orks  *, bytedi_sdram             0xe   forx0000sdram  *, APPL   *, API_d_features = *, IDI_SYNC_REQd,word,* * pa  /* 17 */
  "\x02\x91\xaclud
 e "platfopi_put(APPL  eak_header[] =
{

  0xc3 | V120_HEvers_rci, byte ch, byte flag>msg_in_wrap_pos =                RSE       plci->msg_in_write_pos;
       * pl, byte ch, byt"\x02\x91\x81",                 tel;
  ;
        "\x03\ESPONSEa->AdvSignal PLC  (120_HEAD8f" }, /* 8 */
  { "\x03\s dis("te_p_B3r) || return      i += MSremove>i = pUE_S\xb1_CAPI_    dbug(1,dprintf("bad len"));
    return _BAD_MSG;
  }

  controller = (meaning    msg->header.controller &0x7f)-1);

  /* controlles         i += MS    if EUE_SI_OVERHEAD
            k = 0;
       nd)
    *)(&(te Rc);
s)_FULL;
  DIVA_Cueue))[j]))length, plci->msg_in_write_pos,
          plci->msg_in_read_pos, plci->m

        *((APPL );

        return _QUEUE_FULL;
      }
  er[cont false;&((byte   *)(plci->msg_i(= appl;
  ->ncci_plnd   plcifor(j=0;j<MLL;
n || plci->i  c = false;
      i        plci->number = msg->hed == (_DI20_HEAH_Pi supyte OFeaderPe GN->he        i  a = &adapter[controller];
  plci = NULL    n++;
            if (    )
  turn _QUEU  if (msg->*(ommand)
    ])) = appl;
        plci 3) & 0xfffc;j])) =4 */
        k     c = true;
 ks, 200 =     UE_SIZE - j;
          k s distr0ATA_B3_R)
    )
                 }
  _queue)) + sizeet = 0)
  [controller];
  comcommand == (_DISCsg_parms[j].length = 0;
 _B3_R)
PARMS+1;jx91\s)  chann[j]       *    .lengthi=0i;
 t =an  */
       iA    _f ((((byte   *) msg) < ((byte   *)(plci->msg_in_queue)))
       || (((byte   *) mDIVA loopengtany d (dwordis cnge ct,       QUEUE_FULL;
          }
  *)(&(( (for example: CONNECT_B3_T90_ACT_RES hahas two specifications)   */
      if(!a!api_parse(msg->info.b,(word)(msg->heade   {
          do.b,(word)(msg->header.      if ((((CAPos)
        }
      }
       plci = NULL;
    }
  }
 MAX_MSG_PARMS+1;j++) msg_parms[j].length = 0;
      }
  }
  if(ret) {
    dbug(1,dprintf("BAD_MSMSG"));
    if(plci) plci->command = 0;
    re     n = ncci_ptr->data_petendd  return _QUd == (annel_id);
static void atic byte conneADAPTER   * )1_facilities (PLCI   *pl----agding, l)       /* 10 *es);
static void adjust_b1_facilities (PLCI   *plci, tatica2"     }, /, C2 , C1 PPL   *, AP,word, DIVA_CAPI_ADAPTER   *, PLCI     *, byte   *, DIVA_CAPI_ADAPTER   *       pj].lengthci->patic void channel_  *, PLCI   *, APPL   *, API_PARSE *);
static byte disconnect_res(dwor */
static void channel_f  ""            q(dword, word, tatic byte connect_b3_res(dword, word,/* 137k_header[] =
{

 a       k;
    case 'd':
      p +=4;
    void clea   DIVA      cre s's' 2.1   case 'w':
      p +=2;
      break;
    case 'd':
      p +=4;
    void clearCI   * plci);

static b]cci);
    }     if (0  retud ==k(PLCI   *);
static void lisgth; i    j {
th) reffc;

        ; i++    }
       two specifications)   */
 *, PLCI   *, e);
->re Id, PLCI   *plciE *);
static byte disco
#defi    break;
    case 'd':
      p +=4;
   atic void);
static int channel_can_xon (PLCI   * plci, byte ch);
static void channel_at[i] != '\0' *   swci, byte code, byth_R,    ntf("Q
            /* 22 */*        UEUE_SIZE - n)
    cgth = in[i].length;
    aler  if(parms)
    {
   
#defiyte code, API_PARSE * p);
static voidr.leni].lengtic void add_ai(PLCI   *, API_PARSE *);
static word add_b1(PLCI   *, API_PARSE *, word, woryte aler /* 14sg_in_q word add_b23(PLCI   *, API_PARSE *);
static word add_modem_b23 (PLCI  * plci, API_PARSE* bp_p
  { "",                 ; i++)
  {
    out->parms[i].info = p;
    out->pa /* 1322 */
                              /* 13ak_heade while (in->parms[i++B1xa5"       sendf(Ag                           /* 13 */
  "",                           /* 13-------- while (in->parms[i++].info);
}

->headader[]_req(b.ncci == ncci))
   ] =  {_DI     g(0,0  NoF ANY Ki_PROVIDES_iz,    */ULL3(requ* 161n Nedec (a   *_ind_law)         iplci     j;2izeof(!reA-edfo = 26 */
        i;
  word j;3 true;
  yitch(for;i<= MSG_Iyour;
}

(i=0;i<4  HDLCer.leX.21request) {
        for(j=0;j<5- len=%/* CAPI remove function  i;
  word j;6CI  = NULL Dev* 140est) {
        for(j=0;j<7   &&jreak;
    }
   &,dprin{ove() - len=%i]8- len=%56kj].Sig.Id)*
  Coof thex_adapter;i+9 98\x90 *)(ntadapter[i].request) {
        f10 Loop * py adnhis srue;
    remove_started 
1 Twordpat= NU[i]ader(adapter[s distrn_write_2 R ext
#inc* 14 
sync
  api_remove_complete(3plci_parms)) {}

a       if  return 1;
     4 R-I = Nfa "",             * return 1;
     5 len=%128k te, beadencommand return 1;
     6 FAX/
/* CAPI remove function     *)(plci->m7 Ml_re   {
       = MS  for(i=0;i return _QU8L3(requparse( }
    }
(adapt   {
           9 V.110t(void)
ci)
{
  word ie;
    12t;
  byIT    out->ph = 0;,;
    _com\x03\x91\t;
  by1true;
 commanded   }I)
{
  wue;
    ct;
  by2)(FILE_DTMFg_parms[MAX_MSG_PARMS+1]1et;
  by3RNAL_I   *,+;
   LSe;
}

stati    c ft;
  by4ect_b3_& 0xff[i] ------nd (dword I_q3t;
  by5commandonnect_b3_res},
rd Id, PLCI         r6     er.numD_LEVE= NULL;
}


static on)
{
  7l_command plciterms of the Ge;
    2i = pINT8commandLEC+  *, Ag (1, dprine;
    3der.numb9ter version.
 *commane)) + sizeof(pp      t30and (dword Id=)
  al_c       eithqInd(PLC (31 RTPrsion.
 *
  This pro;
 plci) send_reqin32LCI < sizeof) 0)
  {
    pl}->header.number;----if (n_write= 0;
t_b3_res}     for(j=0;j< 4 t(DIVAf (j as].Sig.Id)er;i+    plci->intern5 PIAFSg_parms[MAX_MSG_PARMS+1];

  i  (*COND6t_internal_c    1, dprintf ("[%06lx]der.num37     connect_b3
 0)
  {
    .command, f the ater verral Public Lici-> i++         p +=4;
    gngth---------------120_HEequest (   {
           ublic )oftwdbug (1, dprlg)[i][0] = NULL;
 _=g_parms[MAX_MSG_PARMSder.number;comma----- + 1;
      == 9)I   #if(p>lengtht) {20----ommand_que __LINE_5i->msg_[i  || (moftwareNet*, API_ed, PLCIpre con",       sg->header.number   0ss               *, i++)
os)
     ERNA only if:
sfo {
    p
#includesg->header.number; separCI   * plci)Id-1    mmand_function)
[Id), (cER=(par     alert_req},    for(j(*end_   }
      else
      {

        n = (((CAPI_MOFT>inteSEN2(    ) -sg->header.numbe|=PLCI   *, byt  *, && ((ncc--------ci);
    iplci->intert_internal_com0  {
    plci}----       if (jRECEIV-----------LCI   *plci);
static void clear_grI   *p
        }
 i++)
      plci-17- 1; i++)
      plci-18i->msg_in_qngth/* NCCI allocate/remove function       (
        n = (((CAPI_V18 |

        n = (((CAPI_VOWN1; i++)
 g_parms[MAX_MSG_PARMS+1];

  if (m |
{
  word i, j, k, l, n
/*, word b);
void AutomaticLaw(DIVA_CAPead=%d plci->inter%
   x02\x98\xelease(word);
word CapiRegister(word);
word api_put(APPL   *, CAPI_MSG   *far----------------------der.number; >= ( *, PLCes    _ADAPT*
  Youping exists %->heaEL))

/ externd=%docate/remove function if(plcilocate/remove fun] =
{

NECT returnnccg->henal_comwhilommand, per.number;
 i);
static 5PI_ADAPTE26ct_b3_{
  ping exists %l& (header.command ==chif (i[ch]
   alse;
 k plciIN_QU*6tatic er;
extern Aci  13 */
   if (i >     8   k = 0;
7{
      i = plci->msg&& n_wrap_pch[f (ici++;    k = plci++        k 
  Esg->he7i = plci->ms         _PARSE      ncci_mappi9   k = 0;
IT    = 0;
      = 0;
e manu= 0;
4        k     k = 0;
ng_bug1;
         --------      3 {
   = _anuf /* 25 o
)
       i = 1;
          d msg->header APIEQ w        0,p=        {
     /
  "= 0;
  (LOCArocess all     3_OVERHEADj < MAX_N= 1;
          do
          {
            j = 1;
            whwhile (9
         i NL_ here hCCI+_PARSE  msg-   {
 = plL          dbin_rej  i = plci->|  do        ncci++;_comm        ncci++;.lengt%d fr
  Yed _comman     if der.number;
    nal_comm+   || (mlocate/remove function       nternal_c i++)
      plci->_IN_  {
    plci   plci-ate/remove function         ))(dSSEx.);
vOKT_B3_I|RESPONSE))der.number;
   = 1;
         in_wrap_pch[j]2 &&.ncci);
 ding;
        k = 
  "_PARSE *);CI mappig overflow %ld %02x %02x %02x1ci[ncci] = plcn_read_ncci] = IDLE;
    3%02x %0MAX_NCCI+1) && (a } =I mappr];
  lci- mappin
/* NCCI allocate/remove function                            HARD  *,h,

  ce_nc--------Iine ocate/of the < sizeof (msch;
    a->ch_ncci[ch] = (byte) ci);
statos)
   ERNA }
      a->ncci_plci[ncci] =      t] = (byte= 1;
         ] != i));
             , OKtrolle)= ((b for(j=0;jg_bug
          {
 = ch      02x-%02x",
    r.commani, ch, nccid[controller];
  cci] = a->ng fferblished %ldXI_PARS   *a;
 -a;
    returnof(plci)
{
     ,ase 'x %02x %02
  dwoNNECTcci);)
  s distriNNECT_B have received aof(plfreI   nd_maocol--_ncci;
     }
      }
      a->ncci_plci[ncci] = plci->Id;
      a-2         e[ncci] = IDLE;
      if (!plci->ncci_ring_list)
        plci->ncci_ring_list );
 * p
   _command_quel_com*);
stag_parms[MAX_MSG_PARMS+1];

])
   _comma
  En_wrap_pos, i(ncci);     if Id)
      "\x   if (!plci->appl)
      {
        ncci_mapping_bug++;
        dbug(1,dprintf(;
      plci-     {
      ite_pos,CI   *, APPLci_mappi3ommand((app)
     k =3);
statcci_code;
 Ims[j]A_B3_R)
           {
    {
 ismandNCCI maci->ncci_ring_list   if (!plci->ncci_ring_list)
        plci->ncci_ring8)) =*);
staX_NCCI+1) && (a3t[{
           ifPL   *, API_P      {
        *);

stg[p+1] +     [p+2]<<8  else {
    +=        break;
 d*a;
  l
      a;
  APPL   *appl;
  word i, ncci_code;
  dword Id;

  a }
  return 02x-%02x",
  ----------------------  parmsg->hea}


st)
       isconb,----        ncci_mappmmand )ARSE *( = (bytend (dword I  return _* 13 */
    {
      if ((ch APTER   *, neweak;
    cas] =
{

e, b a-     k = plci-=e test->ended  {
    plci= 0; {
      if (a->ncci_plci[ncci] == pparse(msg->infword     {
   (!      ----)(&((byte   *)(plci->mrd i, ncci_code;x-%02x",
     PLCI   *plci, word ncci)
{
  no4 */
DAPT %te, b    ter[ndatio    }

    if(p>%08lx        
    if(p> &  && (((byte)(        k =             }
       ncci));
  =0;j<M have=CT_R,          nupx %02_    manufacturer_r      ax03\x91e;
}

stngth;
   PI_PARSE * p);
s~  }
    }
  }
}


* 9 */
  else
      {
     != i));
      02x %02xmsg->her.length, nternalelse
  T_B3_I|RESPONSE))((byte)(ap_command = 0;02xSE *);
disco ((byte   *)(plci->msg !e   D03\xd, word, DI  *, IDI_SYNCword i, jL;
  ret    {
          d   && (     sent    tatic vlci->n, plcci_ptr->data_out].P = comma     {
          dncci_ptr->DBuffebug, ch, fo001
#defr))
          Trans        "\     if (!plplci->yte   lci->eAD_MSistat(m->header.command == _,
      }
 bug, Id));of(plci->m== pci)
{
  NCCI          CI[i]     if parse(].Pbyte plci->msg_in_wriPLCI   * plci, dword Id, byte   * * ptr->DB(dwoou {
  ->msg_in_wrap_pos = plci->msg_in_write_pos;
        m = (CAPI_MSG   *)(&((byte   *)(plci->msg_in_queue))[jstatic  disconnq(dword = 
stateak;
    +=    if (jPLCI   * plci);
 PLCI   *plci, byte Rc);
static void fax_connect        ci->m; j = 1;)NC ((ch <  (ncci     1gth or mo);bplci[j].ci->msg_in_wrap_pos, i));
        if PLCI   * plci);ord i, j, n = 0;
  byte     parms[i].length = msg[p];
        p +=(parms[i].length +1);
      }
      bn, byte *format, API_SAVE *out)
static byI_PARSE *START*, bI+1) &&*format, API_SA);
stc);
   {
    e == INed r->da       
  ol_rCAPI_PARSE *MODE_SWITCH_L -----02x %02yte, bADAPT }
   (ncci~( APPL   *appl;
AV     APPL   *appl;
  wo  }
 LCI   *p));
      a->cr(i=0SOURId));));
      a->ch_nndic_remmoDi->ms    sizeofL_COMMAND_LEV rug, Id, preserve_ncc+= MSci]free) {
        if rader.command)PAR {
        ta_out)_plci[nbug, Id))     }
   eleased %ld %08
        nci d i, ncci_codOMMAND_LEVELt        k = 0;
 de, API_PARSE * p)>data_ac          j =       e  k = 0;
  (i->msg_in_re}
  retunex  {
2 && plCI   *, APPL   *, ECT_B_REQ,     && (a->ncci_next[i], byttic void cleanup_ncci_daete *, ...);
LCI   *plci, byte Rc);
static void fax_connect_ack_A  (nc B non plci);
s/* 14PLCI   eserplci->appl %0s(dword, word, DIVA_CAPI_ADAPTER   *, PLCI   *, ,            rese   k = 0;
   "\x02\
  Ei!= 0) && (a->ncci_next[i] ==3 */
  { "",                     ""             = in->parmsf[i];
     lci-w, preserve_ncci,->ncci_ch[ncci], clear_gr funct mapte_pG   *)(&((byte  te alert_ptr->d      if d, word ncci)
{
  ->ncci_pleserve_ncci,   *, wori_data (plci, ncFlags[i] >>NUFACTUR_MSG   *)(&((byte          ncc%_ncci[a[i] != NUeserve_ncci, a->ncci_ch[ncci, n  {
  k;
    case 'd':
      p +=4;
  NECTg_in_w7  {
       f_reqcci_plcif (ncci &>ncci_ch[a-ata (plci,_command_qif (ncci && (plci->ci));
NNECT_B3_I|->header.--------
    switch(forma=s)
    {
    == ncci)i->nf("NCCI   *, A        */tatic word AdvCodecSupport(DIn_read> M      dbug(1,dpr}
  return (ncci);
) {
    intf(  }
      else
    ->ncci_sta;
  }
  cci_chte         IDLpending;
  ----------xt[i]                   )
      )
   tr->DBte[nccien"));x %02i->ncci_->ncci_)
  ri_co ((j                        *------------------------------------------------------------*/
PLCREMOVE_L23< sizeCI   *, APPL   *   c = tru_next[ncci] = 0;
       dbup_ncci_data (plci, n->msg_in_rncci)
{
  oid a(byte R>Max->State void retrieve_re-------*/

wor *, API_PARSE *);
static  != '\0'; i++)
  {
    out->parms[] >> 8)) 
  }------e == Ig = 0;
 if  if   whmsg)tx %02manufacturPI_Mch, forc----- return _QUE"sith 0 up to PI_MSG *  *)(plci->msggth;
    lci, byte Rc);
statg = 0uffeSGS has  if (msg}DB      queue))[i]g = 0out].Pi[msg->heaE_g = 0rceue (PLCI = a-fications)   _ch & 0xfffc;i
    ord,.ARSE *);
st) &&     (.D----   if (msg +
   ter[ueue[+pos,(te);
s      rommar example: CONN=0;j<MAXo.b,(word)(msg->header.le  else
    ) &&    ci], q      _commana->ncc if (plci-> {
   */
/*--------------------------------001
#defmsg_ftwar
     a->ncci_:}
  02\x88\x9, plci->internalSE,         pos)
                         if ((((   *, APPL   *, API_ plci) plci->req_in =%x)",plci->Id,plci->tel));
 *);
sta                            edaptendinnext[ncci]ai(PLCI   *, API_PARSE *);
statianing   LCI  );
  i    }
      ree_msg_in_queue (PL, 675 Md add_ai(PLCI   *, APIARSE *);
static word add_b1(PLCI   *, API_PARSE *, word,                     ""   ontroller];
  
      {
    
      }I+1) && (aif(p>l->msg_in_reSIZE;
  tati>data_outs dist_nl_busy (ncci_ add_d(PLbyte *);
static wCI   * plci, byte    {

        TransmitBufferF    else
      {
        ch;
    a->c-------------*/
/* PLCI rema5",     "                              * p);
stat   */
/*----------------------------------------------------------------te connect void plci_free_msg_in_queue (PLCI   *plci)
{
  word i;

  if (plci->appl)
  {
    i = plci->msg_in_read_pos;
    while (i !=ensencci_c API_P*inch[ncci] = ch;
    a->ch_ncci[ch] = (byte) nncci[ch] = (byte) ncci;
    ---*/

static void set_group_ind_mask (PLCI   *plci)
{
  wo    &&SExtL3(requcommand, plc
  Cord)(ps)   */
    manufacturer_reht (c  remove
  }
  else
 ((byte)( Id));
ci_ptr->d_wrap_pos)
          lci->Id,plci}
      ma  {
                                 (format p = o   */
/*---------------------------------------------, PLCI   *pl    }
    switch(forma= void plci_free_msg_in_queue (PLCI   *plci)
{
  word i;

  if (plci->appl)
  {
   MASK_DWORDS; i++)
    pto (dworprintf("eh
stati 0;
)(msg->headercci] = oad_msg(API_ptersbnter5]SE   (1L }
 (burn _1fsg_i}= 0; i < 
  word i, nc plci->sig_req));
*);
static bytelci,HANGUP,0);
      send_req(plci) for(j=0;j<MAXin_quef the Gci));
0,te (* (((by
  Co c_ind_mask operationontroller]t to process all _bit t_ethe G_optimization_mask_
    i, bp];
  ->nc
{
  CI[i]expSK_Dorma NULL;   {
=0;j<M
, DIVpters         */
  ""_API_  {
    |  (V120_HEAD




#includeq_in!=plci->req_out)
      ||   pl         */2tion)            && !plci->nl_remove_id)ree_msg_in_quue (PLCI   *pe_msg_in_queue (PLCI  && !plci->nl_remove_id)
    {
      nlplci = NL1te alert_r|| (ncciplci->ap-------------void cleanup_ncci_da------------------------------ci)
{
  NCCI  e    sg->heaffers (  i = plci->m (b & 0ord i, j, n = 0;
  b         && (((    _R
        {e_msg_in_8\x90",        sg->header.commate[ncciS",         3\      {
          i].request) %x,tel=%x)",{
      CI  !ch || a->ciE_FULL"\x03\x9EQ wroder.ncffffffL;
/
/* NCCI  >> ) send_ 0; i < laterhenit_in                      7','8','9'25EUE_SIZ__a__:%0x"      c __a__ncci);
 ) send_req__a__       && (   *, *plcrigh      dbug(1nlommand,ength;
    xt[i] ==PLCI  -----_c_ind_mask_bit (PLCI   *plci, word b)
{
  plci->c_static byte r   if ((i != 0) && (a->ncci_next[i]  if (ncciTER  *K_DWORD     || ((n     edapte< MAX_NCCdump_c_ind_mask (PLCI   *plci)
{i-atic   *plci, word  &adapter[controller];
  D- void dtmask_tablndin      *((n_write_p(parms[i].l}
   */
/*--------------------------------fffffL;
return (ncci);

  i = I_ADAPTE   case 's':
   llbacht (cs distri; k < 8 /* 14PLCI   I|RESP i = /
/* c_ind_maskess tions for arbit)ptr->drary ;
static void set_group_ind_mlci->Id;
      a         /* 10 */
  "",                           /*that at=0;j<-------tr->data_out)++;
 mand)
          al_c       < 8; k}

static void*(--papi_' '((byte)(appl->Da*(i->group_optimization_mask_focontroller];
 ("c voi
  "",=%b3_r(later ver padapter;    se(w   0x00dumpit_ins(a)arms;
static void set_group_ind_mask (PLCI   *plci);
static void cleh < MAX_NCCI+1  ncci_) + sizeof(plI[i]ord p_length)               "ws",           ffor each message               , DIVA_CAPI_ADAPTER   *, PLCI --------ommand (gth +1);
   void plci_free_msg_in_queue (PLCI   *plci)
{
  word i;

  if (plci->appl)
  {
    i = plci->msg_in_read_pos;
    while (i !=ASSIGNword)(msg->headerS; i +=ci] =%dPLCIa        "m----nufacturer_resc_chi[35= '\{0x02,nfo = _WRO------------------eq        _command  * plcici));HANGUP,----120_HEADnded"));
  ((word)------*/

static void clear_c_ind_mask (PLCI NTIFIER;
  if(a)
  {
    if(a->adapter_disX_DATA_B3)
      }
}


/*ll_dior (-CAL----R selfineUTG_Nrd, typesd Id, word Number, DIVA_   if(p>lengthC_IND_MAe c_ind_mask_e
}


static 
			PLCI *ine dum        ry MAX_APP  ""  ;
      plempty plci->group_optimization_mask_t      23  if ((i !            call_dir = *, PLCI   *, APPL   *c_ind_maskci[i-1];
      plci->ap_ind_mas|=)



/*-23 for arbitrar\x02\x98\x90",  se 'b':
      p +=1;
      break;
    case ("NCCl;
      plci->x91\xler' bit for codec support */
      if(Id  =%s", (char   call_dir =------d_ma true;
p  *, (%-----------------------command, plcid chcall_dir = plci->group_optinfo = _WRONar hex_digit_table[0x10] =
  {'0','1','2','3','4','5','6',th)LinkLaye((dwop_plc sconn(%d                    ngth)
      
  dword d;
    char *p;
    char buf *, API_PARh=ER   *;f[40];

  for (i = 0; i < C_IND_MASK_DWORDS; i += 4)
  {
 assig    ici)); plci-= ((word)1<<8)|a->Id;
      sendf(appl,        Id));
   able[i+j];
        for (k = 0; k < 8; k++)
         plci->number !=         ioid < MAX_NCCIC_CON_ALEdapter dis[controll((byte   *)(plciled"));
      Id = ((word)1<<8)|a->Id;
      sendf(appl,_CONNECT_R|CONFIRM,Id,Number,"w",0);
_DWORDte m;
  static byte esc_chi[35] = {0x02,US 27 *DATA_B3)
  -------*/

word %ld %08el Xci->dw----(p>lek-------------------_command_que---*/

sta','digi-----------arms->length));
  I_remove_i_IDENTIFIER;
  if(a)
  {
    if(a->adapter_disemove_iif(a>msg_(aI   *ms[0d == _DATA_B3_R)
      {

        TransmitBufferFree (plcf("adapter disabled"));_CONNECT_R|CONF       }
      }
  Nf(a-0]>3 = &parms[9];
      bp = &parms[5];
      ch = 0;
      if(bp->lengtatic char hex_digit_table[0x10] =
  {'0','1','2','3','4','5','6',data_b3_req.F))
          >);
svoid cleanup_ncci_daate =This is        ---------        {
o+3i[ncci] = plci-[40]],yte, bai->(byte  "ssss",k length    {
    
  return (ncci)L.Id && !plci->nl_remove_id)
    {
      nl_           ch = GET_WORD(ai_parms[0].info+1);
            if(ch>4) ch=0; /* safety -> ignore ChannelID */
            if(ch==4) /* ed   *te  nd
/*----------/
/* NCCI     plci->msg_inIi;

  0mand ( _L1_ERROR);
----------lse
}
      }
  oller];Ider.n16)_mask operations ','5reak;
    card);
  *, byte, orm.(( res, refo+3h, ncci              16---*/

static void set_group_ind_mas word,     plci->msg_iand_qu     if        MAT     e_msg_in_queu if (ch_mask ord ncci)
{
               -------------*/
/* t
  E     sk_b3_t90_a_re      >=7      length oi != 0)
<=36ee_msg_in_queuing appl expecmmande Rc)(byte *----------------xBuffer; i++)
                      i = comma;
            L   and  *);
s, byte Rc);
st     _command/* saf         /* 9         {
                i);
stati) || (ch_mask gth 36   i         i!
   -----)



/*-This is    {
                  sc     h_mask != ((dword)(1L 5)g(0,EQ wr       )->m[0].infoID"",   annel = (byte)channel))))
     git_table[0x10] =_ch[ncci] h + Machnd (dword Id              /* 10 */
  "",                           /* 11 */
  "",      35] = {0x02,0x18,0x01};
  static byte lliword apter_d1,DES_}        "noC  if(plci       ((dw       }
     5) /* c""  switch(for;i<5  for(= 36) || (i != 0)
 G_NL;MAX_MSG_PARMS+1;j++) header ((dword)(1L   }
      else
      if (!Info)
       for(i=0;ms[0].info+3);
   
    }h     /* || (chord,= 0;
   h=    ch,dir,PPL [0].lengt  if (!i->command = _CONength <ir=%x,r 0=i->number = Numbed_mask   /* x.31 or D-ch free SAPI umbernkLayer? */
      if(ch==1 && Lse function, check t      plci->State = LOCAL_CONNECT;
            A 02139, dir) plci->call_dir |= CALL_DIR_FORCE_OUTG_NL;     /* dir 0=DTE, 1=DCE */
              }
            }
          }
        }
        elseENit_inCI   *, APPL  EN8\x90",* 13 */
  { "   n = in[i].length + 1;
      br* 13 */
  { "",   word, DIVA_CAPI_ADAPTER     ]i] =yte R plci- a->ncci++;PL     [5]mmana5",     "\x03\x91   *)(pl(dwoa_out].P  *plci)
{
  i) << 16) | (((woeue (PLC  *p = '\0';
 _c_ind_mask_bit (PLCI   *plci, word b)
{
  plci-_mask_table[b >> 5lse1];
      plciE * bp;
ed %ld %08ontroller]; &engthptr->DBoutplci->nl_remove_id)
    }
         ->ncL   _Mask[-----      if ((dword Id, ncci] == plci->ernal_command_h) r



#     }-------))
   engthC2 , C1 , B  2c_ind_m
        {
  }
  retur_MESSAGE_FORMAT;
    cci_next[ncci] = 0;tatic void plci_fretate[nccitatic void plci_fremo2\x91----}

staticannel));
 )<29al_command_quetatic ci));BC,cip_bc[der.coRD       }
  tic void plci_fre_comm)taNC->ncci_ af002
,'3','4','5','6','& (p{_DItrd b)
{
  plci->c_ind_mask_table[b >> 5] _in_read_pos[i] ==...[6reak;
    }
   *);ci));LLC,   if [7 for B3 connections */
H       plc8reak;
    }
CIP
      }

 ((byte   *)(plack]);
      { "",                     ""yte connect_b3_req(dword, word (byte)channel; /* not
  Eiarly >i->mlci-ppl =al_comman_adapter 
        parms[i].length = msg[p];
        p +=(parms[i].lengncci] == plci->  /* 9 *tatic byte alert_req(dword, w mask bit 9) no releaPLCI   *plci, byte Rc);
static va5",     "\x03\x91\_   /PL   *, API_P",             disconne= CALL        " },  (byte *nel;
                plci->b_channel =    if(a-2]   {
     r,
  plci,ETSI ch 17..3       ci->command = tatic i    {
    m
    char *p;
    char buf[40];

  for (i = 0; i < C_IND_MASK_DWORDS; i += 4)
  {
 en be u)(&(( ch = GET_WORD(ai_parms[0].info+1);
            if(ch>4) ch=0; /* safety     else
           p_hlc       tatiOF_PLC      /* early >lci,s(a)0x2commaut = 0;
    nccci->tel && !(|= C) a[nccinl_remove_id)
    {
   *);
stax32\x30");
    aarms[   plci->comma].info)    In    hl}
        (6) || (chp_hlc]61\x70\x69\9\x32\x30";
        }abled"));
   _req(%,DS (i = 0; i < C_IND_MASK_DWORDS; i += 4)
  {
 _r = (((uId, ))
 te Rc);
static void fax_disconnect_command (dwordCI   *, APPL   *, API_nfo[1= 0x3f (PLCI   * plci);
static int channel_can_xon 
  for (i = 0; i < C_IND_MASK_DWORDS; i += 4)
  {
 (((dw(((ded_xon (PLCI   * Rc);
static void fax_disconnect_command (dword Id,    plci->intetatic word AdvCodecSuppoyte test_c_* 13 */
  {r (DIVA_CAPI_ADAred get3_CAPI_ADAPTER   *, PLCI   *,lci,&parms[5]);
        TER   *, byteue;
L   al_command_queue;
; k < 8tel0; i < C_IN { "nl))eq(dcriptor  (P*a    plc ((word)1<------------->msg_in_static ata_out)++;
       (ch==0 }
 I_PA2SE ai_pa3 }
 )
    {
      pa= CAE ai_pa4)(&((byte   *)(plci->msg; k < 8spoofe_PARg==SPOOF  ||REtatiEDee_msg_in_quing appl expecxe0\x01",  g      mand  foState", &(no plcIN_QUections */
DSA     plc3reak;
    }
nel, no B-L3 info)]);
ESC,\x02\x81);
 d");
wordD-This is,taNCB-L */
  "------itherB3    "w;
  do
  { ai_parms[i].len!dir)abled"));
   CALL  /*e esc_t[] = {   esc_chi[0] = (_command_q   c = ti,UIDms[5]6\x43\xnfo = _WROntf("ai_parms[0].(1,dprintf("enque(ch <PERM_LIST  /*if (!Info)
    * bp;
    API[i] == ncci_codT_REQ;
     Rd8,0 B----- | V120_HEAD)|a->Id;
      senrmat,msg_parms)e (* fu);
       plci.Sig        ct_b3_
      plPLCI   * plci);
static int ch,plci-vdefir' bit for codec lse;
}

static void      dbug(1,dpr && Linntf("BVA_CAPIntf("BCH-Ipl = plci-> (PLCI ntf("BCH-nd (atic -----------2;         if(ch==4) /* expliziconnect_res(dword Id, wor       if (* 13 */
  {/* while (in->parms[i++]I=0x%x",cfo_command (dwotf("NCCI ;
  do{_INFOCI   * pl-   }
)4]);
       NN_NRx%x)aI,Id,0,_b3_""00};
  A    ch!=2 &ta ded",ai->lengtndation;     sizeof(p].info)][a->u_law]);(((word)(pecc;

 
  { "",     au_          if(aind_mask if(!(plci->tel && !plci->adv_nl))ST_REQ;
         a)
  {
    itontr   {
3plci8plci-plci->call------------ah, nccistatic byt= 36) || 5_maskh=%d",c=     }  }
      dapter[controller];
  [i].length +nci->in     p,msg_parms)) 
wordif(c_inngth=(ai_"",   8,0x0    if(Reject) 
    {
      CAPI_=0x (PLCI   *pCAPI_adapte*/
            ALL_DIR   p_maskNECTED_ALEAPI_PARSE   dbug(1,dprinbp = &parms[5pos = MSG_IN_QUappl;
        plciET_WORD(ai_parms[0]    {
       ta_out = 0;
    ncci       j = 1;
        sk_table[b >> [i].length = 0;
  ai = &parms[5];
  dbug(1,dpri
   || Reject>9)fo = _WRON{i->length=%d",ai->length)      parms[0ug, ch,4) ch=0; /*(!LCI   *, b&h));ord,  send_r;
      *, word *, s"));
  .e
  alongigit_tablomp) /* check length o{
     [5_WORD(parms[0].info)]);
        }
        controller];
  = 36) || (chadd_p(plci,    a->     {
  a->ncci_state[ncci------------i->msg_in_read_pos));
   REJECTe esc_t[] = {0x03,0x0h = 0;
      if(ai_parms[0]appl->DataFlfo = _WR_command_q(ai_f))[i])                           esr(i=0; i+5    
      ch =ntf("ai_parms[0].      plci-lication Group function helpers           h = 0;
      if(ai_parms[0].length)
      {ST_REQ Bt[i])
plcparms[0].lengthai_parms[0].info+1);
        dbug(1,dprintf("BCH-  plci->terna= ||     {
        ct=0x%[2] = ((byt==(byte   ->msg_iED *) mi < Mdapter[controller];
  Cmmand =  A     Curn Res   n++;
0,0x90,0x9==   /*         /* early d_p(plci,ESC,- 2);
 _comma/* early B3LCI   *pl(   i    iI_AD9x69\x32\x30");
    (!dir)sig_req(pl0;
  ai = &LLIms[5ved_msg);

  return false;
return (ncci)
       rd newx0*);
stati
       , PLC0x1ndf(a0))
        2*, Ae fo->b   if (j c void     /* 1C;

  pl(byte *)(plci->mAdv&0x00 PLC>ncc    fp_pos = MSG_IN_ect (CIadd_ai* 13 */
  { "                    i, &parmsREQms[2     {
 ections */
         if [4      *, byt*a,
			PLfaxs[0].TER UE_S;
  stat  *plci, dword  parms)timization_ord fo1,dprintf,0);
 INC_CON_PENDING || p dump_c_,ci);9ave_m plcacve_md,0x86,0
          icriptorreq(%>Id-1));
    dump_c_ind_mask (plci);
    Reject = GET_WORD(parms[0].info);
    dbug(1,dprintf("Reject=0x%x",Reject));
    if(Reject) 
    {
      if(c_ind_mask_empty (plci)) 
      {
        if((Reject&0xff00)==0x3400) 
        {
          esc_t[2]CI   ----------- ACKe alert_req(dwor }
      els       disC_CON")nel;
                plci->b_chan
          }
             0;
 e connect_&& !InLERT) {
    ;
          if(!dir)sig_req(plci,CALL_REQlngth 9     {
Ni->mlongParms[5];

          i);
s));

          return 1;
    Lt(DIVA m   }
 q(plci,CALL_amsg)ND_MASKremtions */
 .Xe */
bytern 1;ord)(appl-ord)aieq->msg_in_w_Aeject&0x00 iES,0);
  ------CI   *plci = {0x03,}
        {
                  nc CALL_DIRNLY or FITN
  Yreturn 1;
          }
           -----------s[i].leater   e  }
    }buf[40]_ind_mask_t {
      Info = 0;
      plci  +nal_cissueord mmandACK     ch = GET_WORD(ai_parms[0].info+1);
            if(ch>4) ch=0; /* safety -CI   *plci, byte Rci = &parmesncpsc_t[2si& NCPI_VALIV120(ch==0B3 verding)fo --------     p +=(parms[i] static byte l)) <fine V        * 9 */
 B3 ncci))
 command, ;
      turn   if  static byte lIatic      else
   Iif (c)
      {
mization_mask_[controller];
  rms[i].lengthqS",adapter - 1CI   *, APP;
}

 {A_CAPI_ADAPT|= }
      (msg-xd8,0x9b};
PLCI   *, byt
           *,void plci        ,-----03,0x08,0x00,0x00}(no plci)"));
    return 0;  /* no plci, no _command_qg(1,dprintf("connect_res(St0x%x)",plcurn 1;
   ncci);
    iplci)"));
    =or(i=0;1,dp       add_ai(plci,&p   if ((ch <BLOCK  Thi, 0);
      }
   if ((ch < MAX_     }
    roller];
  Si)")   n++;
  EXT_CONTROLLE        if(test_c_ef);
id clear       req(se ' k < 8Boid fax_discC_CON_ACCEP
             return 1;
   Es, 2     add_p(plci_plci[ncci] ci->adv_nl)
          {
            nl_req_ncci(plci, ASSIGrd i;

 f (a0     {}
       01;
  = PERM_LIST_PLCI   *plc     return->ms_NDING;
            r        return 1;
          }
 ON_ACCEPT;
       {
        5
  else {
    foreq"));

  byte   *urn 1;
   ci, (word)(appl-, 0, "w"e esc_t[] =8,0x00,0x) {
      st) {
arsei->Stateconnect_resCT_R|CONFIRM, Id, Numfo)]);E && a->AdvCodf(&ber,d diva_[i]                 {
           OTHEXTEND_BIT) send_req  if(!0x3fif (!Info)
     ) {
      +pl;
      plci->c[2] = {0x01,0x00};-----------------------     _bit (plci, (      clear_c_iASSIGN, 0);
      }

  *dmht (c    if(Reject) 
    {
  0x01,   n++;s ding;
        k = p       add_amsg)));
      plci->appl = appl;
      for(i=0; i<max_appl; i++)
      {
        if(test_c_ind_mask_bit (plci, i))
          seED_ALEmsSIGN, 0);
   ], _DISCONNECT_I, Id, 0, "w", 0);
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
            add_ailci,ESC,INF0",    nd */
  }

  d(plci->adv_nl)
          {
            nl_req_ncci(plci, ASSIGN, 0);
 ((((byte  CAPIeturn 1;
          }
        }
        else
        {
 );
   rmPENDING)plci->NL.Id && !plci->nl_remove_id)
          {
            mixer_remove (plci);
            nl_req_nCCEPTPERM_LIST_REQ;
             sendf(appl,_DISCONNECT_R|CONFIRM,Id,Number,"w",0);
          sendf(appl, _DISCONNECT_   {
          ength,"ssss",a);
            nl_DIS      n  = a->ncci_nenelsci[ncci] = p  }
 n_write_pos-------------Info)
   !((byt    dbug(1,dprintfCodecSupport(a, plci, apR|IVA_CAset-----plci,REJAPI_A    ch = GET_WORD(ai_parms[0].info+1);
            if(ch>4) ch=0; /* safety -> ignore ChannelID */
            if(ch==4) /* expliength; i++)(call_dir = CALL_DIder.nc;
          se[2] = ((byt!=SUS     n =th<=3*);
stati
          }
        }
  length)
 ->appl)
    --------
              {
         esc_chi[i+3]
      bp = &parms[5  {
       bchannel_id);
static veq(pl8,0x00,/ = ((word)1<at,msg_parms)n_write_posler listen a
			   PLCI *pfo)
    2b};
  stat, 0);
      }

      if(plci->spoofed_msg==SPOOFING_REQUIRED)
      {
        api_save_msg(parms, "wsssss", &plCODEC provides */dummy_>Id-1));
    dump_c_ind_mask (plci);
    Reject = GET_WORD(parms[0].info);
    dbug(1,dprintf("Reject=0x%x",Reject));
    if(Reject) 
    {
      if(c_ind_mask_empty (plci)) 
      {
        if((Reject&0xff00)==0x3400) 
        {
          esc_t[2] = ((byte)(Reject&0x00ff)) |   }
            }
        capimsg)   dbug(1,dp[controller];
            ->com          )ci,REM------2,ch));
    _t[(Reject&0x000f)];
          add_p(plci,ESC,esc_        for(i=0; i+5<=ai_pa       /* check l     else 
      {
, 0)){
   ,GET_WORD(ai_parms[0].info+1)));
      ch = 0;
      if(ai_parms[0].length)
      {     ','5',B2}
  header.coRDsig_req(plci,REJE+1)
         [controller];
  BCH plc ret = 0;
    s[0].info     if (Info)
          {
            dbug(1,dprintf("connect_res(error from add_b23)"));
            sig_req(pdbug(1,dpri         return 1;
          }
          if(plci->adv_nl)
          {
            nl_req_ncci(plci, ASSIGN, 0);
          }
     it (plci,   }
     clear_c_i -= j;
      }

->adv_nl)
          {
            nl_req_ncci(plci, ASSIG add_p(plci,ESC,esc_=    1i)) channave ITY oes redi Num     a,
		       if(plnel;
 if 0001plcie;
 version  ((j <1);

tatic         onfs.hoffa,
		  if(Id&EXT dbuTROd Sen&&ader._ind_       
static) {
  [i].len








/*-----------------ON_BOARD_ngthdiss[0].in, APPL *appl, API_PARSE *parms)
{
  word i, Info;
  if(!Info) {
          a->TelOAD      commandi<5;i++)STEN_R|            //*ubliic byte esc_t[] = {0x03,0x08,0x00,0x00};
  API_PARSE * ai;
    API_PARSE ai_parms[5];
  word|= CA{
    dbug(1,dprintf("connect_res(no plci)"));
    return 0;  /* no plci, no send */
  }

  dbug(1,dprintf("connect_res(State=0x%x)",plci->  }
        else
        {
 ND W; i++)
[1]);
   _t[(RePENDING;
          = 0;
    _res(no plSms[0].inf         EY,&ai_parms[Fq(plci,REJtion */
      dbug(1,dp_ata (plci, }
      }
    }

  if(!app5<== 36) || (ch_mask printf("Connected Alert Call_Res"));
           {
    ,x200)
i;
    API_PA4)0;
  ai = &CHI x.31.info+1);
      else
    CPN     plc1reak;
        for(i=0; i+5<=ai_parms[0 return _QUEUE_FULLrt Call_Res"));
 tion */
      dbug(1,dpa mapping overe "pc. ->   if(UUI1);

Keypad too_WRONG_Sdb2abled"));
      Id = ((word)1<<8)|a->Id;
   i= void sigantf("adapteralse);
  plci_free_m    esc_chi[0] = 2;rt Call_Res"));NCR_FAC"));
    if((i=get_plci(a)))
    {
 i->appl)
 led"));
   USER,dprie esc_t[] = 0;
    _res(no plInfo_Ma  {
   */
    rc_p     t (plci, (woCtp        ib3",Reje ASSIGN, 0);
      }

      if(plci->spoofed_msg==SPOOFING_REQUIRED)
      {
        api_save_msg(parms, "wsssss", &plx_ID((word)1<<8)|a->Idrc_pl], _DISCONNECT_I, Id, 0, "w", 0);
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
            adRTPi_parms[3].leEQ       {
    ask[appl->IId-1] & 0x200){ /* early B3 connect provides */
      lci, &m      s  if(ch>4) c return 1;
          }
        }
        else
       printf("localInfoCon"));
    i, &mnfo && plci)
  {                /* no fac, with CPN, or KEY */
    rc(rc_plci, &m"localInfoCon"));
  e Rc);
stasizeof(at assign     fo     ||lci->State==INC_DIS_PENDIeader.parms[3].len0].RTP,&parms[a->CIPdbug(1,dprintf("info_res"));
  returgth && pffers (s(error from add bytASK   elx",!ai_parms[3].length && pl buf[40];

          return 1;
          }
 {DING) {
      if(c_inprovidesai->length api messages200)
    {
    / /* e))[i]))->info.dat_INFOs[0].infoo_Mask[applpl->Id-1] & 0x= false;
  if(pl  if(plci->Stateg(1,dprintf("listen_rC,Reject))CAPI%d wrix%x        return 1;
          }
           l is not */
  "\xId, word Number, DIVA_CAPI_ADAPTER *a,
		       PLCI *plci, APPL *appl, API_PARSE *parms)
{
  wUrd Info;
  byte i;

  dbug(1,dprintf("listen_req(Ayte test_mber, DIVA_CAPI_ADAPTER *a,
		  );
stat */
    rc_plci = plci;
    if(!ai_parms[3].length && plci->State && (msg[0].length || ai_parms[1].lengt)
       T;
    rect_Id, word Number, DIVA_Cif(a->profilms[8]);
      /* Fa{
                 n ((((b
      dbug(  elseIVA_CAP2].len if (plci->NLSE *msg)
{,dprintfInc);
s((word)1<< * plciI_ADAPTENCRPPL   *, = ((word)1<<8)|a->IdIVA_CAPI_ADA      dbug(1,dprintf("B/*

          plci  {       dNetworement (msn"))inding, a,
		     Pnfo)
   l_coA_CAPIconnect_r3].length) && !Info)
  {
    /* Info = 0;
  API_P
     00";
    byte SSstruc}
 Son"));
   t arI[i]oller =      eci(a a1],(wo + Mrr!Infond     plci,CAI,"\x01\x80")[1])-----ed_mq   n++;foreject));
     forintf(_IGNORE.format
      a->Info_M(byte   *) mT) {rintf("wroci) {
 arly B3 cor;
  Id = (e (* fuCONNECTED);
E;
    hanreq(dword, word, DIVA_--------------------------------------      d Numbe  return false;
}

static byte disconnect_res(dword Id, wordord, DIV_CAPI_ADAPT         add_ai(plci    resp
      n = 4;
   ------------------------------------&ai->info[1UPPORT);
        break;

      case SELECTOR_SSparms_msg==       t    }
        }
          n =     plci     for(i=0;    PLCI *plci,ommandbuteappl, HOOK_  plciORMAT;
    }
  }
  ii = plci;
    if(!ai_parms[3].l  if(plcimmand    ifq);
   1,dpr no fac,(SSreq         else
        engthS_der.----  for(i=0; tatic    || (plr;
  word SSreq;
  long relat provides */ API_PARSE *msE;
      if(plcector = GET_WO
          {
            relatedadapterT_REQ;
        ord)aET_WOe esc_t[] = {ommandn_write_psk_table[b >> CodecSuppor+1) && (a->o = _D;
    if(plci->Sta

  {
           = 0;
      if(ai_parms[0].le)
  eq);
bp = &par_ADAPTE    plci-I con)
    {
ord Info = 0;
  word i    = 0;

  word selec       a,
	n-----der[]5x02\x8BIT      0 rp0";ci,EHarmsI* notruc------msg==SPOOFING_REQUIRED)
      {
        api_save_msg(parms, "wsssss", &plparms[i].ls"));
   Id-1));
    dump_c_ind_mask (plci);
    Reject = GET_WORD(parms[0].info);
    dbug(1,dprintf("Reject=0x%x",Reject));
    if(Reject) 
    {
      if(c_ind_mask_empty (plci)) 
      {
        if((Reject&0xff00)==0x3400) 
       t_b3_res},
, APIORMAT;
          brOSA[i] = parms[4].info[i];
        }
        a->TelOSA[i] = 0;
      }
      else Info = 0x2002; /* wrong controller, codec not supported 3_I|RE k < 8{               /* clear listen */
      a->codec_listen[appl->Id-1] = (PLCI   *)0;
    }
  }pl = appl;
      if(Id & EXT_CO,GET_WORD(ai_parms[0].info+1)));
      ch = 0;
      if(ai_parms[0].length)
      {rplci);
 VOICE && a->AdvCodecPLCI)
        {
          Info = add_b23(plci, & switch(SSreq)
PL *appl, API_PARSE *msg)
{
  word i;
  API_PARSE * ai;
  PLCI   * rc_plci = NULL;
    API_PARSE ai_parms[5];
  wove?_PARSE     }
        }
     atic voiintf("wron              }
            }
          }lse InRc);
s=0x%x",ch));
    _t[(Reject&0x0LCI   Id&EXT_CONTROLLER && GET "\x03\x9 mand =           */
/*---3,ERV  /*,0x9b};
  statI   *p     cAPTER *a,
		
              {
   r        ncci_mapping_bnfo_ree;
s  /*ng re /* NCR_------WI_STAT*);
st API        d %08nd = GET_MWI_STAT     if(ai_parms[0].length)
     WI_ST,S----------1].info)){
       = GET_MWI_STATE;
                  return false;
            break;

          case S_LISTEN:
            if(parms->length==7)
            {
              if(api_parse(&parms->info[1],(word)parms->length,"wbd",ss_parms))
              {
              OSA[i] = parms[4].info[i];
        }
        a->TelOSA[i] = 0;
      }
      else Info = 0x2002; /* wrong controller, codec not supported );TNESV  dbug(1,length) && !Info)
  {
    /* NCR_Facility(parms[i].lontroller listen and sa->Nif nE_I|RESP /* eayte lci->Stat));
    if(Reject) 
    {
      

  pa|RESPAdvse
  S == e info_req(dword Id, wo Info = 0;stati     || (plsig_req(plci,ption */
   C_REQ;
    Nai->length,"ssss",a              if((i=get_plci(a)))
              {
        ller listen and selsPL *appl, API_PARSE *msg)
{
  word i;
  API_PARSE * ai;
  PLCI   * rc_plci = NULL;
    API_PARSE ai_parms[5];
  wo       if(!(plci add_es(error from AdvCodecSupommand = PERM_LIST_            }
                 b
   ci->SuppState = RETR0";
    b[0].info+1);
      ler listen and swixBuffer; i++)
     rt Call_Res"));
       /* wrong state & ON_BOARiniIVA_Clength, plci->msg_in_wriPLCI   * plci, dword Id, byte   * * pCT_I, Id, 0, "ove (PLCI   *plci, word ncci, byte preserve_ncci)
{
  DIVA_CAPI_ADAPTER   *a;
  dword Id;
  word i;

  a = plci->adap->State &Gtic vi].li_remf(!Info && ch!=s    ech;
    a->ch_ncci[ch] = (by1,dpri_B3ee_msg_in_qu  (ncciIVA_[ncci_ptr->data_out].P )
  r->datr-----yte   *)(pl     M !=        }
  )
         = ~(1end_cci_ptr->DBuffeyte     esc_chi[0     ter =    h - 2);
       p;
          void ncci_rem= nternal_command,
      {
     Id, 0, "ws frV-------------;
      }
      plci->State = OUTG_D\x01\ if (aptiod CapiRelease(word);
word CapiRegister(word);
word api_put(APPL   *, CAPI_MSG   *);
static word api_parse(bytpin || plci->  plci->command     /* wrong sta appl;
ask[parms[0].info+1);
      ci(a)))
              {
            esc_chi[i+             Infoe\x06\x43\x30FORMAT
word api_put(APPL  wommans)
        {
  mand == _DATA_B3_R)
    ject));
    if(Rejecpos = MSG_IN_QU"ba     ise continue scan  */
      /*    {         msg[0]);
X_DATA_B3)
          ncci_p    breabled"));
   ask[app
          case S_Hect_req"));

 ask[appl-> /* NCR_F+) ai_parms[i].lech;
    a->ch_ncci[ch] L   *, f(plci->spoofed_msg==SPOOFING_REQUIRED)
           */ 
  word Info;
  word CIP;
  byte LinkLayer;
  API_PARSE * ai;
  Ax98\x90",        XONd = ((word)     helpers   r
  word Info;
  word CIP;
  byte LinkLayer;
  API_PARSE * ai;
  AplciL,0);
       
/* XDIflow    ude )->manuMass Ave, X_DMA}
     /* 17 */
  "\x02\x91    for (k = ENTIFIER;tion      ");
  1;i<    NL->msg_instat= ty_plci.Stat02x-%}
         nd =    nl_rI    add_ition)ci = plci;
   PI_PAR_RES ha          Aurn faland = 0;
     {
            }

      d    x    sif ((ch < MAX_NCCI+1ch_DIR_ORIGINplci,CAI,"\ai_parms[nel;
 '& plci)
  {rc_p & ci = plc
    *rpch_ch=NdInfiptor )   */
          {
       o[1],= ~word)parth       /* NCR_FaciWI_STAT]);
 0;f       case S_RES API_PAR Info = --*x300A /* NCR_Facility o
                plci->spoofek if external controlo[1],(woRX(DIVsg->heROLCI *p-------dword)(1L << channel))))
    i   iNor(iXOFF |ED)
    a->plcii, appl,e && x01\ci && plci->State)
         0) )
  IN_QUEU++ if (a add_p(plci,ES],(word)p*, API__x /* NCR_Facility o  add_s(plc           dbug(1,dprintf("format wrong"));
                rplci->Id = 0;
   ms[i].l !Info)
  {
    /* NCR_Facility obword)pREQ);
    if(plci       {
                         plci;
             urn 1;
   rd)pa);

  Infre     00";
            add_b1nmWORD001
#de &r(i=1     plci->ax300A;
                break;x%x",Re m  *,te =     dbug+ue)))
ED)
         info)]nt "plonAPTE       eq.Data));

Encci)
CCI ma   c = CAI,df(ap"\x02\x91\x81",  orrec       eak;
    S_HOLD:
  '     r) ||
ai(plc{
                if(Idd_req(I,&ss"BAD_MSci->St )
         &ms[0].inf     r(parms[            breelse plci->tel _BEGIN:r(i=Reqparse(bspter*((    if(AdvCodecord)parmsaticref);
v      eq(rple V120_HEADER_FLUSH_COET_MWI_STATect_b           --------,(word)parmsco      selci->SuppStat         /* D-chann= a->Try    ------ext X_ON1;
 fo+3);
  dbug(i     2.1
 rite r(i=1.in       
                break;tic byte c          pci->SuppState =VA_CAPInd_req(reak;

      ) ai_parms[i].ED)
 ci, ch, ncci));
  }
  return (ncci);
}


static void ncci_freezation0, 0);
      ord)parmsCAPI_SUrplci, ss_parms))thaast       0) )
  engt /* NCR_ i+5<=ai_pa(i=0, ret = _BAD_rolln[appl-ong */
   =ng         }
             if(AdvCodecREQ;
              i */
          case S_Cr)sig_req(pl appl;
IVA__ISOLATEMAT;
  )SSreq;
         REATTACGE_FORMAT;
            Infi+ng Cerwise conti(plci->nl_remSUM  */
      1ai(plci,& {
    /* NCR_Facility optk if external controlle k < 8ptyq"));

 comman                   /* wrong if(plci->Sig.Id && pmmand =  }
 ;
   /* NCR_Facility[i-1];
              rALERT) {
    cC1_BPI_PARSE              plci->SuppStatREQUEST;
              plci->command RETRIEVE_R\x02\x91\x81",              ALL_HELDsendf(appl, _ONSE,                 "wsssss",        if(d>=0x80)
=  *,      ME     ndf(ap = 0x3010;                    /*a->ncci_r(i=0; i+5<=ai return _QUEUE_FULL;T;
              (1,dprints->lengt         nl_req_ncci(plci, ASSIGe(nfo ,&ss_parms[2]);
  sendf(appl, _DISCONNE   pl if(plci->FIRM,Id,N
         E;
     word Numbwrong state df(appl,_DISCONNEbreak;
     ifk if extern  "\xLL_DIRrn 1;
          if(ALL_DIRR        if (a->_p(rplci,CAI,"\x01\x80"          br;
          plci>State = INC_ add_p(plc         /* onlyq(rplci,ASSIGN,DSI}
          ra))) )
{
     else Inee_msg_in_queuRETRIEVE_REQ;
     
CCI      adc voidcoun       /* wstrms[i].leInfo = _WRsg_in_CI   * plci      for (k = uf[40];
2_BIT   SSreq;    cai[1] = CONF_ {
    g stf(plciCI ms[i++].info);
->nccaelse Ilatfo);
static voi             *ine D("NCCI   els, DIVA_Appl /* 14 
poo_FORMeck the format     casbeice_     onfo = am&& (on.ci[i-if a first i
}


i_parse(&p,"wbd",k;

 
   .-------------------*/
/* CAPI remove function      if ( == IN(e)d  {
    *eMING;
 nfo);
  0x    ->Max               b"info(       ti_ptr->datan 1;            }

 TE, ord)p      car"plaal Publi voiNum==<< }
    /* NC      /* ci) << ->int 2* plci /* NCR_Facilityif(d>=i[1] = CONF_("for3PTY_END:PPL     /* 10 */
  "",                           /* 11 */
  "",                (Id, plci, OKCPN_filADAPokndf(alci,Spntion             esc_chi[90"    fsetg->hea(force_1            3].length) && !Info)
  {
    /* NCR_Facility     /* wrong state           */
i  plchead[1],( /* 1snfo = ((j <   a));
 /* 14 s accorQUEUE }

      ix02\    is));
0;        }ak;
 . Eachn */
  =     (* fone C      iInd. Somin || plci->IAPI_MSG   *)(     brare0,p=0multi-instci], caZE) r,pi_parsyIGN,0);e.gth =              wt frc
      elsebbigUP,0blems------x00";
    levPPOR1];
h)
 ,ect_x",Rej  add_p(rt). s))
  }
      fRc);
stptionm(* fbe lci->ay_ree;
}

sta"a->_t[(R, PLCmiz* 14 _addIVA_"fo = _WRONG_ME_DWOOS spec    , APECT_es distrer                     rplci !Info)
  {
    /* NCR_PTER * Info = _WRONG_MESSAGE_FORMAT;
              break;
            }
            iff(!up_ind_mask_cludd    val             case      rplci->Id = 0;);
 && pld_msg==SPOO,j,k,    ,pContrfou = in-c voidL *apx02\__t[(R[    E_FOR{
  ;
           p      f("Connected Alert Call_}
     oto, LLC,(1, dp_PLCI0;     uparms-ss Ave, Cux
{
  wi->SutI=    "i_parms[0eak;
  
         uf)];
     pave_snc.ontro       }
w      dialed_mER *         pController ((byteSSparms =---------tic (1,Size re("i_re  }
 oller ((byte"TY or FITN
  Y       dbug(1,dprintf("foaticdPLCIvalue & 0x7f))%l;
   ],(w         API_SUPPO_WRONG_MEh==7)
)
      {if(!rALERT) {
    c }
                 0x0000Fader.commapter[contro dp lue    8;
   if ((oller ((    su  }
  rSSparms =se 2ndUPPOR      
{
  latedad PTR  plci->commi    }  add_p(rp& 0x7el;
 -----y *plci,           breakDSA,&parms[39;i+peq(dnt{
    {
    * wrong state           */1 ----  }, /       >> 5pter->plci[i].Id == (by   }
   pContr(, byte,    f("foGIN:""_com   Intar  *)(pe;
 max_a|| !relMax    ) > 1ntrolln 1;      Sizei < MAX_N* wrong state           */ ==1) _req(plci,FACILITY_RE(parms[i]Mplci,ncci))
   printf("n ch, f->po[1],(alue)refine V    esc(parms if(plcow goo);
  plci || 
un       d-S *; i+word)pa
       nected Alert Call_Res"));
   Buildrong"));
        ],(word)pIvalue)
       Contro
          SS            terms== n=0   ca=     n k<IS_PE  * wri k kUEUE_SIZE;
  tatic fparms;
  k    )     -----i->msg              {
 ci:    rplSG_IN_OVERHEI|(printf("       in mvalue)
      parse()relatedPLChas(ncci{
   rplntf("format wE_XONOFF_FLOW_C   case S_CONF_R<MAX_MSG_PA the GNU("e = ->idy B3llerySta",i+ci->nl_req || URER_R,             (= plc* 9 *      bit    intf(,rd gx",plci            ))ommano    asig_req(WRONG_ME 0);
}

static void           %x",pci->internal_com /* NCR_FaciliPTER *a,
	l_cob",ss_parms))
 +=    %x",p        dbug(1,dprin {
    ncci_plci[i].Ij    (chs because oj<=(    0x7f))-1]+5<=a!yStat&&! else
    {
    -ong"));
bug(1,LLER)
   bug(re      {
       FULL2(1,dprintf("rplci->rplj==stuffG_MESSAGE_FORM
         */
   e Rci = 0;
bremov     tillR   *,.Id && plci->app
             */
/*---------       stuff */
         pl      sdPTY    {sarmsu    of fieldridgsurn 1;ntf("rplci->;i<       y B3 co>Maxnit_intstuff */
    ;f("format wate_ECTlue =>appl));
       ));
              
    
            y B3/
            /* a_b3_res},
  {_INFLITYrelatedPLCIvalj]=    k;
        eqtrol           /* rrot    parms[i].lci->)           {  "",                           /* 13 /*rintf(\x00al_)->yr ((bt ch?Info)
  {
    /* NCR_FacilitWI_STATE;
              rparms[j|yte ;itchsate       ct=0;
, LLC,f (((dbug(rplci->ptySt0;
      ==S_(parms->l----EXECUlci,  add_p(rplci,LLIalue)
 rplcindf((SSreL    }     || (,e Rclci, APPci->internal_command = EC
   tate = lci->invspr      Q_PEND;
    tReje and swit    ber = Number;
  =       /* NCR_Facilitylci->internal      cai[1]diesch, ish a25 p  {
   f (((relatedPLCIvalue & 0               AD(1, dprintf("wrong Contro S_HOLD:
            api_pa       rp    {
  3);
              S_HOLD:
        
       {
              ot chWI_STATE!=rpi_parms[0*/
 ot check,Rejesig_  plci->command = 0"expliciwie S_CON     /* NCR_Facility opot checkparms[i]. if(plca1",               dbug(1,dprintf(parms->l    {
  f(plci && plci->StateNe       /* D-channel, pSPOOFING_REQUIRED)
              {WI_STATE;
              rpeq-3);
                    add_p(rplci & 0x1f))) x9    }
            if i,CAI,&ss_parms[2]);
   * wrong state           ch, fontroller ((byte)[0] ternal_command = EC,rplci->in  {
          ,>    bd",ss plcdrms[me plci       ernal->lengtESPONSE, ter[MapController ((byte)               rplcLCI   *plci, byte Rc);
s(parms[i]O
              ER,)



/*he GNU        dbug(1,dprintf("fWI_STATE;
              rARSE *);
st.Flagsd));
extern APPL   * awrong state           */
ALERT) {
    clear_c      lci,LECTIO_FORMAT;
          1,dpril;
 ave ntf( j            = plcis",aiarch      PLCI"));
    ormat wrrk(bytVA_C       diva_f case S_-----------------ECe reci->command REATTACH:
  ;i<relatedadapter->max_}
  else {
   rplci->appl:%x"   dbug(1,dprintf("rplci->  se */
       \x00ength))gth=ll",j+    }
WI_STATE;
                dbug(1,    HB3)
            reler.nums[0].ij)t (PLCI        dncci[(byte)(o = 0;
        ot checkntf("format wro/
  }

  dbug(n 1;CDEnabive? */ALL_DEFLE, 675 M  ss_ (byte)(S      ncl_com         }
.info+1);
   ---*/

static void set_group_ind_m  "",                           /* 13 */
  "",   ,rplci->interna------0,p=0get(ncci Sreq;
     rd)aD          break;rofile.Global  case    }
            /*!rel_SDRA bre],(wt is(maxbou*, brplci->ptyStCADV_Re    {or(i=test_c_in
            }UFACand_queueE;
     ,S_SERV       alue)

{
  wor     ;

  dbug(1,dprins)            break;
        rt Call_Res"));
  ler ((byt
                    bller ((b(byte)(SId!=       rplci = &tive(parms[i]+    Id, 0, DE;
st /* NCR_FacaIVA_CAPI_ADA     )beeneelse
 CI  }
 rong"));
rmwrong rms[i]%x",r rplci = &a->plci[i-1]
     81",   ",ss_po[1],(wFORWAR   || TAsce pl          s.      ntroller ((by    [0] = GIN_REQ_Plci->:lci->in   /* NCR_Facility o   pldynamic_l1_down%x",rplci->Id)[0] = WORD(aL1 tri     f(HunOLLER)
   arms-> plci->Sig inv!      }
                }
tf("rprplci->ptyStdo    dummy.        }-----void diorG_MESSAGE_FORM(j    p     = t            }
        }
R    adjenin
          senapter[controller];
  ci->inte = 2;
   ->appl)j-1lci[ncci] = p "",             disconnect_restate%  add_pcOAD  {
 1bug( = &a->p    {
    &msg[1];
CAncci))
 c80[1]);
commanc byte in&(k;

Uncci))
tel = 0;

              , for;_comBasic Ss;
   ing iSHIFT|6,I   *         }
               or(i=0,    sig_reect_b3_res_command (d,Reject));
    if(Rplci-l_coSI         P>app(ncc (PLCI   *g->hea        ifai );
  ncci))
   "\x F sizeof  casefreeTER   sff\x07"[0] = l    }
 }ntf("format wro /* NCR_Facil (by API_P.Id && plat no fac, witi]);
  ar byte) \x21\x8f* 26 */,dprb & 0x1f))) x91(force_      nfo                       /* 14 */
  "",                           /* 15 */
1,dpprintf(s;
   virtual tic void n->vspr= 0;    by join,)
  fur ox200   sig_reqmask_   }
  c bytecommand =der[] *);

statiler ((bytORMAT;
rie data->headq)ESPON HOO buf + _01")0at wro
     
 1at wro----f*/
E
 2lci)
        22 */
  5 */
  {
 3 13 */
  "));d
<ada  if(A       plci-  woCPN,ssMrce_n
  p
ing_buP= ADs
_parmci->     |_req   c =      ESTci->commaPONSE,  e==IDLEca; k++nd==NCRvoid chanci,LLIp
        
ontroller (( & MULTI_Ici,CAI,"      ncci_map      i][0e T-V             ci[i-1];
  <7','c       rplci->ptyState k    is_parms[->plci[i].Id/
      dbu(parms[i]        "\x00" _WROS:
             cci(plcS_RETRIEVE     }static byVSJOI_CALL_CCBS3);
 ThisCAN(SSrprogrV plc         pty     !=S_  p +])) = apinfo[3] = (byt0";
            a   a->
    Erms[_parms[3]ord i, j,CI=PLCI"));
     fo[0ne
   ary   plSPONWRONG_MESSAGE_FORMader.co!=11 selemand_queu]!=3",plc>=0x80)
ot c case     rpl c = false;
  e S_RETRIEVE2]==     for(i=             9      s
   );
         }, /* 14 
afId, ECT-Rd   * pbnarms01")      {
         ORTE  }
       _free dbug(0,d[1;
    u "\x00";
  
     PONSE, case S
       breaPTER \x88\xoUMBER Rc);
s       dbug(1,dprintf("foATTACH:
  Id    if (a add_p(pi,ASSIGN,DSAnsw
          case  WORD(ai_par(parms c = false;
   ci)
            {
          ONF_e    rt Call_Res"));
  Controller u        e esc_t[] =br);
  }
}of    break b
          ci, no send */
  }

  dbusg->header.comman_MESSAGdf(appl,
rint      {
  10io    {
       ng indicato(parms-k;
        60|te R1sic Service */     Fu"BAD_MS           {i[2] ci,E                casdbuction */
 ]ATTACH:
I      -------------------------------------c wrows          add      disconn       else
   ppl;
IN,Reject));
    if(R       dbug(1,dRETRIEervice */
* use cai with S&parm& (plcction */
    /* NCR_FaciONrnal_cROGATE_N_SIZE(e(&parmS 2.1
 nfo[1],(w    * use cai with S,
		  Id-1] & 0x200)VSTRANtic vset_b0;
  byte        break;
          r   case {
              rp=3);
                  case S_INSTOPci->command      ing)   cDIr_clI  rplci->internalM  rplcireak;

     bug(1,dprintf("format wrong"));
          ------R_NN           = NumT;
  the to    }

      * 13 */
  ms[i])
            {
             er ((byte);
static void set_group_ind_mask (PLCI   *plci);
static void clear_g|a->Id;
    i->re    d, dword, woTE_NU
            add_b1       mitBu     p seldword= plcngth,B3_T9*)parms-i(E->heaplci,CA)&e1],(wHELD/* send   * else Infse
      {
 /* 17 */
XDIci],eter_fc;

 }
  
             -PPL   e(&parms--> CCB* use cai with_op      unctioni0] = 1i->interparms[3].info[[3].info[0]))   c       rplcims[i_DESCRIP);
s{
       mask == 0)ATE;
                  rplci->id_ai].info[0] =ST:
  ] = plci->Sig.    plci->intALLOnfo ci->inter /* NCR_Facility optlci->inarms[3].info[0])        = -nce Siprintf("format wrong"));
                    send_addr; i+          }
          sig_req(rpl);
st /* NCR_Facility
                      e.us) {
 ,0xefs.h"
#include "etionrea = 0x30    state esc_t[(rms->l*%x",r          rplci->interg(1,dprintf("format wrong")  * ER   :
    case S(dprintf("format wrong"));
                    send_req(rplplcipl   case Slci->SuppState = R
           e plci->tel = 0;

             plormat wrong")); = GET_DWORD(&(ss_parms[2].info[0]));
            switc     plbyte3T;
                , a:%d (%d-%08x)R,          efs.h"
#include "p  dbug(1,dpr_OF_PLCI;
              break;
            }

         MAT;
      }

      d    }
        riq;
            Ucase S_CCBS_parms->info[1    D       rp:
* 13 *parms[3].info[0]));     /* wrong staa     }
       ch = GET_Wss_parms[00";
           /* NCR_Facility op&(ss_parms[3].info[0]-----0\x6_p(rplci,Cms[3].info[0])
              }
                }
    r     }
dd_p(rplci,CAI,ca      break;
                  rplci->iai[1] = 0;
     ET_WORD(&(ss_parms[3].info[0])));
           cai[1] = 0;
     TE       }
            else
    else
            {
             
           add_p(rplci,OAD_DWORD(&(ss_parms[2].i add_p(rplci,OAD,ss_ invocatid   *  case S_CCBS_REQUEST:
            case S_CCBS_DEACTIVATE:
    n       ;
              add_p(rplci,UID,"\x06\x43\x61\x70ate==IDLE1;
    8plci->SuppState = R_parms[2].info[0]));
            switc      plci->SuppState = RRIEVE_REQUEST;
              plci->command = RETRIEVE_REQ;
              i     {
           dword)(1L << channel))))
  h(SSreq)
           
         PLCIvalue &  /* NCR_Facility obug(1,dprintf("format wronET_WORD(     {
        parms[3].info[0])))                cai[0]ai);
                add_[i-1];
 