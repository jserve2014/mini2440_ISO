/* bnx2x_reg.h: Broadcom Everest network driver.
 *
 * Copyright (c) 2007-2009 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * The registers description starts with the register Access type followed
 * by size in bits. For example [RW 32]. The access types are:
 * R  - Read only
 * RC - Clear on read
 * RW - Read/Write
 * ST - Statistics register (clear on read)
 * W  - Write only
 * WB - Wide bus register - the size is over 32 bits and it should be
 *      read/write in consecutive 32 bits accesses
 * WR - Write Clear (write 1 to clear the bit)
 *
 */


/* [R 19] Interrupt register #0 read */
#define BRB1_REG_BRB1_INT_STS					 0x6011c
/* [RW 4] Parity mask register #0 read/write */
#define BRB1_REG_BRB1_PRTY_MASK 				 0x60138
/* [R 4] Parity register #0 read */
#define BRB1_REG_BRB1_PRTY_STS					 0x6012c
/* [RW 10] At address BRB1_IND_FREE_LIST_PRS_CRDT initialize free head. At
   address BRB1_IND_FREE_LIST_PRS_CRDT+1 initialize free tail. At address
   BRB1_IND_FREE_LIST_PRS_CRDT+2 initialize parser initial credit. */
#define BRB1_REG_FREE_LIST_PRS_CRDT				 0x60200
/* [RW 10] The number of free blocks above which the High_llfc signal to
   interface #n is de-asserted. */
#define BRB1_REG_HIGH_LLFC_HIGH_THRESHOLD_0			 0x6014c
/* [RW 10] The number of free blocks below which the High_llfc signal to
   interface #n is asserted. */
#define BRB1_REG_HIGH_LLFC_LOW_THRESHOLD_0			 0x6013c
/* [RW 23] LL RAM data. */
#define BRB1_REG_LL_RAM 					 0x61000
/* [RW 10] The number of free blocks above which the Low_llfc signal to
   interface #n is de-asserted. */
#define BRB1_REG_LOW_LLFC_HIGH_THRESHOLD_0			 0x6016c
/* [RW 10] The number of free blocks below which the Low_llfc signal to
   interface #n is asserted. */
#define BRB1_REG_LOW_LLFC_LOW_THRESHOLD_0			 0x6015c
/* [R 24] The number of full blocks. */
#define BRB1_REG_NUM_OF_FULL_BLOCKS				 0x60090
/* [ST 32] The number of cycles that the write_full signal towards MAC #0
   was asserted. */
#define BRB1_REG_NUM_OF_FULL_CYCLES_0				 0x600c8
#define BRB1_REG_NUM_OF_FULL_CYCLES_1				 0x600cc
#define BRB1_REG_NUM_OF_FULL_CYCLES_4				 0x600d8
/* [ST 32] The number of cycles that the pause signal towards MAC #0 was
   asserted. */
#define BRB1_REG_NUM_OF_PAUSE_CYCLES_0				 0x600b8
#define BRB1_REG_NUM_OF_PAUSE_CYCLES_1				 0x600bc
/* [RW 10] Write client 0: De-assert pause threshold. */
#define BRB1_REG_PAUSE_HIGH_THRESHOLD_0 			 0x60078
#define BRB1_REG_PAUSE_HIGH_THRESHOLD_1 			 0x6007c
/* [RW 10] Write client 0: Assert pause threshold. */
#define BRB1_REG_PAUSE_LOW_THRESHOLD_0				 0x60068
#define BRB1_REG_PAUSE_LOW_THRESHOLD_1				 0x6006c
/* [R 24] The number of full blocks occupied by port. */
#define BRB1_REG_PORT_NUM_OCC_BLOCKS_0				 0x60094
/* [RW 1] Reset the design by software. */
#define BRB1_REG_SOFT_RESET					 0x600dc
/* [R 5] Used to read the value of the XX protection CAM occupancy counter. */
#define CCM_REG_CAM_OCCUP					 0xd0188
/* [RW 1] CM - CFC Interface enable. If 0 - the valid input is disregarded;
   acknowledge output is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CCM_CFC_IFEN					 0xd003c
/* [RW 1] CM - QM Interface enable. If 0 - the acknowledge input is
   disregarded; valid is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CCM_CQM_IFEN					 0xd000c
/* [RW 1] If set the Q index; received from the QM is inserted to event ID.
   Otherwise 0 is inserted. */
#define CCM_REG_CCM_CQM_USE_Q					 0xd00c0
/* [RW 11] Interrupt mask register #0 read/write */
#define CCM_REG_CCM_INT_MASK					 0xd01e4
/* [R 11] Interrupt register #0 read */
#define CCM_REG_CCM_INT_STS					 0xd01d8
/* [R 27] Parity register #0 read */
#define CCM_REG_CCM_PRTY_STS					 0xd01e8
/* [RW 3] The size of AG context region 0 in REG-pairs. Designates the MS
   REG-pair number (e.g. if region 0 is 6 REG-pairs; the value should be 5).
   Is used to determine the number of the AG context REG-pairs written back;
   when the input message Reg1WbFlg isn't set. */
#define CCM_REG_CCM_REG0_SZ					 0xd00c4
/* [RW 1] CM - STORM 0 Interface enable. If 0 - the acknowledge input is
   disregarded; valid is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CCM_STORM0_IFEN 				 0xd0004
/* [RW 1] CM - STORM 1 Interface enable. If 0 - the acknowledge input is
   disregarded; valid is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CCM_STORM1_IFEN 				 0xd0008
/* [RW 1] CDU AG read Interface enable. If 0 - the request input is
   disregarded; valid output is deasserted; all other signals are treated as
   usual; if 1 - normal activity. */
#define CCM_REG_CDU_AG_RD_IFEN					 0xd0030
/* [RW 1] CDU AG write Interface enable. If 0 - the request and valid input
   are disregarded; all other signals are treated as usual; if 1 - normal
   activity. */
#define CCM_REG_CDU_AG_WR_IFEN					 0xd002c
/* [RW 1] CDU STORM read Interface enable. If 0 - the request input is
   disregarded; valid output is deasserted; all other signals are treated as
   usual; if 1 - normal activity. */
#define CCM_REG_CDU_SM_RD_IFEN					 0xd0038
/* [RW 1] CDU STORM write Interface enable. If 0 - the request and valid
   input is disregarded; all other signals are treated as usual; if 1 -
   normal activity. */
#define CCM_REG_CDU_SM_WR_IFEN					 0xd0034
/* [RW 4] CFC output initial credit. Max credit available - 15.Write writes
   the initial credit value; read returns the current value of the credit
   counter. Must be initialized to 1 at start-up. */
#define CCM_REG_CFC_INIT_CRD					 0xd0204
/* [RW 2] Auxillary counter flag Q number 1. */
#define CCM_REG_CNT_AUX1_Q					 0xd00c8
/* [RW 2] Auxillary counter flag Q number 2. */
#define CCM_REG_CNT_AUX2_Q					 0xd00cc
/* [RW 28] The CM header value for QM request (primary). */
#define CCM_REG_CQM_CCM_HDR_P					 0xd008c
/* [RW 28] The CM header value for QM request (secondary). */
#define CCM_REG_CQM_CCM_HDR_S					 0xd0090
/* [RW 1] QM - CM Interface enable. If 0 - the valid input is disregarded;
   acknowledge output is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CQM_CCM_IFEN					 0xd0014
/* [RW 6] QM output initial credit. Max credit available - 32. Write writes
   the initial credit value; read returns the current value of the credit
   counter. Must be initialized to 32 at start-up. */
#define CCM_REG_CQM_INIT_CRD					 0xd020c
/* [RW 3] The weight of the QM (primary) input in the WRR mechanism. 0
   stands for weight 8 (the most prioritised); 1 stands for weight 1(least
   prioritised); 2 stands for weight 2; tc. */
#define CCM_REG_CQM_P_WEIGHT					 0xd00b8
/* [RW 3] The weight of the QM (secondary) input in the WRR mechanism. 0
   stands for weight 8 (the most prioritised); 1 stands for weight 1(least
   prioritised); 2 stands for weight 2; tc. */
#define CCM_REG_CQM_S_WEIGHT					 0xd00bc
/* [RW 1] Input SDM Interface enable. If 0 - the valid input is disregarded;
   acknowledge output is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CSDM_IFEN					 0xd0018
/* [RC 1] Set when the message length mismatch (relative to last indication)
   at the SDM interface is detected. */
#define CCM_REG_CSDM_LENGTH_MIS 				 0xd0170
/* [RW 3] The weight of the SDM input in the WRR mechanism. 0 stands for
   weight 8 (the most prioritised); 1 stands for weight 1(least
   prioritised); 2 stands for weight 2; tc. */
#define CCM_REG_CSDM_WEIGHT					 0xd00b4
/* [RW 28] The CM header for QM formatting in case of an error in the QM
   inputs. */
#define CCM_REG_ERR_CCM_HDR					 0xd0094
/* [RW 8] The Event ID in case the input message ErrorFlg is set. */
#define CCM_REG_ERR_EVNT_ID					 0xd0098
/* [RW 8] FIC0 output initial credit. Max credit available - 255. Write
   writes the initial credit value; read returns the current value of the
   credit counter. Must be initialized to 64 at start-up. */
#define CCM_REG_FIC0_INIT_CRD					 0xd0210
/* [RW 8] FIC1 output initial credit. Max credit available - 255.Write
   writes the initial credit value; read returns the current value of the
   credit counter. Must be initialized to 64 at start-up. */
#define CCM_REG_FIC1_INIT_CRD					 0xd0214
/* [RW 1] Arbitration between Input Arbiter groups: 0 - fair Round-Robin; 1
   - strict priority defined by ~ccm_registers_gr_ag_pr.gr_ag_pr;
   ~ccm_registers_gr_ld0_pr.gr_ld0_pr and
   ~ccm_registers_gr_ld1_pr.gr_ld1_pr. Groups are according to channels and
   outputs to STORM: aggregation; load FIC0; load FIC1 and store. */
#define CCM_REG_GR_ARB_TYPE					 0xd015c
/* [RW 2] Load (FIC0) channel group priority. The lowest priority is 0; the
   highest priority is 3. It is supposed; that the Store channel priority is
   the compliment to 4 of the rest priorities - Aggregation channel; Load
   (FIC0) channel and Load (FIC1). */
#define CCM_REG_GR_LD0_PR					 0xd0164
/* [RW 2] Load (FIC1) channel group priority. The lowest priority is 0; the
   highest priority is 3. It is supposed; that the Store channel priority is
   the compliment to 4 of the rest priorities - Aggregation channel; Load
   (FIC0) channel and Load (FIC1). */
#define CCM_REG_GR_LD1_PR					 0xd0168
/* [RW 2] General flags index. */
#define CCM_REG_INV_DONE_Q					 0xd0108
/* [RW 4] The number of double REG-pairs(128 bits); loaded from the STORM
   context and sent to STORM; for a specific connection type. The double
   REG-pairs are used in order to align to STORM context row size of 128
   bits. The offset of these data in the STORM context is always 0. Index
   _(0..15) stands for the connection type (one of 16). */
#define CCM_REG_N_SM_CTX_LD_0					 0xd004c
#define CCM_REG_N_SM_CTX_LD_1					 0xd0050
#define CCM_REG_N_SM_CTX_LD_2					 0xd0054
#define CCM_REG_N_SM_CTX_LD_3					 0xd0058
#define CCM_REG_N_SM_CTX_LD_4					 0xd005c
/* [RW 1] Input pbf Interface enable. If 0 - the valid input is disregarded;
   acknowledge output is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_PBF_IFEN					 0xd0028
/* [RC 1] Set when the message length mismatch (relative to last indication)
   at the pbf interface is detected. */
#define CCM_REG_PBF_LENGTH_MIS					 0xd0180
/* [RW 3] The weight of the input pbf in the WRR mechanism. 0 stands for
   weight 8 (the most prioritised); 1 stands for weight 1(least
   prioritised); 2 stands for weight 2; tc. */
#define CCM_REG_PBF_WEIGHT					 0xd00ac
#define CCM_REG_PHYS_QNUM1_0					 0xd0134
#define CCM_REG_PHYS_QNUM1_1					 0xd0138
#define CCM_REG_PHYS_QNUM2_0					 0xd013c
#define CCM_REG_PHYS_QNUM2_1					 0xd0140
#define CCM_REG_PHYS_QNUM3_0					 0xd0144
#define CCM_REG_PHYS_QNUM3_1					 0xd0148
#define CCM_REG_QOS_PHYS_QNUM0_0				 0xd0114
#define CCM_REG_QOS_PHYS_QNUM0_1				 0xd0118
#define CCM_REG_QOS_PHYS_QNUM1_0				 0xd011c
#define CCM_REG_QOS_PHYS_QNUM1_1				 0xd0120
#define CCM_REG_QOS_PHYS_QNUM2_0				 0xd0124
#define CCM_REG_QOS_PHYS_QNUM2_1				 0xd0128
#define CCM_REG_QOS_PHYS_QNUM3_0				 0xd012c
#define CCM_REG_QOS_PHYS_QNUM3_1				 0xd0130
/* [RW 1] STORM - CM Interface enable. If 0 - the valid input is
   disregarded; acknowledge output is deasserted; all other signals are
   treated as usual; if 1 - normal activity. */
#define CCM_REG_STORM_CCM_IFEN					 0xd0010
/* [RC 1] Set when the message length mismatch (relative to last indication)
   at the STORM interface is detected. */
#define CCM_REG_STORM_LENGTH_MIS				 0xd016c
/* [RW 3] The weight of the STORM input in the WRR (Weighted Round robin)
   mechanism. 0 stands for weight 8 (the most prioritised); 1 stands for
   weight 1(least prioritised); 2 stands for weight 2 (more prioritised);
   tc. */
#define CCM_REG_STORM_WEIGHT					 0xd009c
/* [RW 1] Input tsem Interface enable. If 0 - the valid input is
   disregarded; acknowledge output is deasserted; all other signals are
   treated as usual; if 1 - normal activity. */
#define CCM_REG_TSEM_IFEN					 0xd001c
/* [RC 1] Set when the message length mismatch (relative to last indication)
   at the tsem interface is detected. */
#define CCM_REG_TSEM_LENGTH_MIS 				 0xd0174
/* [RW 3] The weight of the input tsem in the WRR mechanism. 0 stands for
   weight 8 (the most prioritised); 1 stands for weight 1(least
   prioritised); 2 stands for weight 2; tc. */
#define CCM_REG_TSEM_WEIGHT					 0xd00a0
/* [RW 1] Input usem Interface enable. If 0 - the valid input is
   disregarded; acknowledge output is deasserted; all other signals are
   treated as usual; if 1 - normal activity. */
#define CCM_REG_USEM_IFEN					 0xd0024
/* [RC 1] Set when message length mismatch (relative to last indication) at
   the usem interface is detected. */
#define CCM_REG_USEM_LENGTH_MIS 				 0xd017c
/* [RW 3] The weight of the input usem in the WRR mechanism. 0 stands for
   weight 8 (the most prioritised); 1 stands for weight 1(least
   prioritised); 2 stands for weight 2; tc. */
#define CCM_REG_USEM_WEIGHT					 0xd00a8
/* [RW 1] Input xsem Interface enable. If 0 - the valid input is
   disregarded; acknowledge output is deasserted; all other signals are
   treated as usual; if 1 - normal activity. */
#define CCM_REG_XSEM_IFEN					 0xd0020
/* [RC 1] Set when the message length mismatch (relative to last indication)
   at the xsem interface is detected. */
#define CCM_REG_XSEM_LENGTH_MIS 				 0xd0178
/* [RW 3] The weight of the input xsem in the WRR mechanism. 0 stands for
   weight 8 (the most prioritised); 1 stands for weight 1(least
   prioritised); 2 stands for weight 2; tc. */
#define CCM_REG_XSEM_WEIGHT					 0xd00a4
/* [RW 19] Indirect access to the descriptor table of the XX protection
   mechanism. The fields are: [5:0] - message length; [12:6] - message
   pointer; 18:13] - next pointer. */
#define CCM_REG_XX_DESCR_TABLE					 0xd0300
#define CCM_REG_XX_DESCR_TABLE_SIZE				 36
/* [R 7] Used to read the value of XX protection Free counter. */
#define CCM_REG_XX_FREE 					 0xd0184
/* [RW 6] Initial value for the credit counter; responsible for fulfilling
   of the Input Stage XX protection buffer by the XX protection pending
   messages. Max credit available - 127. Write writes the initial credit
   value; read returns the current value of the credit counter. Must be
   initialized to maximum XX protected message size - 2 at start-up. */
#define CCM_REG_XX_INIT_CRD					 0xd0220
/* [RW 7] The maximum number of pending messages; which may be stored in XX
   protection. At read the ~ccm_registers_xx_free.xx_free counter is read.
   At write comprises the start value of the ~ccm_registers_xx_free.xx_free
   counter. */
#define CCM_REG_XX_MSG_NUM					 0xd0224
/* [RW 8] The Event ID; sent to the STORM in case of XX overflow. */
#define CCM_REG_XX_OVFL_EVNT_ID 				 0xd0044
/* [RW 18] Indirect access to the XX table of the XX protection mechanism.
   The fields are: [5:0] - tail pointer; 11:6] - Link List size; 17:12] -
   header pointer. */
#define CCM_REG_XX_TABLE					 0xd0280
#define CDU_REG_CDU_CHK_MASK0					 0x101000
#define CDU_REG_CDU_CHK_MASK1					 0x101004
#define CDU_REG_CDU_CONTROL0					 0x101008
#define CDU_REG_CDU_DEBUG					 0x101010
#define CDU_REG_CDU_GLOBAL_PARAMS				 0x101020
/* [RW 7] Interrupt mask register #0 read/write */
#define CDU_REG_CDU_INT_MASK					 0x10103c
/* [R 7] Interrupt register #0 read */
#define CDU_REG_CDU_INT_STS					 0x101030
/* [RW 5] Parity mask register #0 read/write */
#define CDU_REG_CDU_PRTY_MASK					 0x10104c
/* [R 5] Parity register #0 read */
#define CDU_REG_CDU_PRTY_STS					 0x101040
/* [RC 32] logging of error data in case of a CDU load error:
   {expected_cid[15:0]; xpected_type[2:0]; xpected_region[2:0]; ctive_error;
   ype_error; ctual_active; ctual_compressed_context}; */
#define CDU_REG_ERROR_DATA					 0x101014
/* [WB 216] L1TT ram access. each entry has the following format :
   {mrege_regions[7:0]; ffset12[5:0]...offset0[5:0];
   ength12[5:0]...length0[5:0]; d12[3:0]...id0[3:0]} */
#define CDU_REG_L1TT						 0x101800
/* [WB 24] MATT ram access. each entry has the following
   format:{RegionLength[11:0]; egionOffset[11:0]} */
#define CDU_REG_MATT						 0x101100
/* [RW 1] when this bit is set the CDU operates in e1hmf mode */
#define CDU_REG_MF_MODE 					 0x101050
/* [R 1] indication the initializing the activity counter by the hardware
   was done. */
#define CFC_REG_AC_INIT_DONE					 0x104078
/* [RW 13] activity counter ram access */
#define CFC_REG_ACTIVITY_COUNTER				 0x104400
#define CFC_REG_ACTIVITY_COUNTER_SIZE				 256
/* [R 1] indication the initializing the cams by the hardware was done. */
#define CFC_REG_CAM_INIT_DONE					 0x10407c
/* [RW 2] Interrupt mask register #0 read/write */
#define CFC_REG_CFC_INT_MASK					 0x104108
/* [R 2] Interrupt register #0 read */
#define CFC_REG_CFC_INT_STS					 0x1040fc
/* [RC 2] Interrupt register #0 read clear */
#define CFC_REG_CFC_INT_STS_CLR 				 0x104100
/* [RW 4] Parity mask register #0 read/write */
#define CFC_REG_CFC_PRTY_MASK					 0x104118
/* [R 4] Parity register #0 read */
#define CFC_REG_CFC_PRTY_STS					 0x10410c
/* [RW 21] CID cam access (21:1 - Data; alid - 0) */
#define CFC_REG_CID_CAM 					 0x104800
#define CFC_REG_CONTROL0					 0x104028
#define CFC_REG_DEBUG0						 0x104050
/* [RW 14] indicates per error (in #cfc_registers_cfc_error_vector.cfc_error
   vector) whether the cfc should be disabled upon it */
#define CFC_REG_DISABLE_ON_ERROR				 0x104044
/* [RC 14] CFC error vector. when the CFC detects an internal error it will
   set one of these bits. the bit description can be found in CFC
   specifications */
#define CFC_REG_ERROR_VECTOR					 0x10403c
/* [WB 93] LCID info ram access */
#define CFC_REG_INFO_RAM					 0x105000
#define CFC_REG_INFO_RAM_SIZE					 1024
#define CFC_REG_INIT_REG					 0x10404c
#define CFC_REG_INTERFACES					 0x104058
/* [RW 24] {weight_load_client7[2:0] to weight_load_client0[2:0]}. this
   field allows changing the priorities of the weighted-round-robin arbiter
   which selects which CFC load client should be served next */
#define CFC_REG_LCREQ_WEIGHTS					 0x104084
/* [RW 16] Link List ram access; data = {prev_lcid; ext_lcid} */
#define CFC_REG_LINK_LIST					 0x104c00
#define CFC_REG_LINK_LIST_SIZE					 256
/* [R 1] indication the initializing the link list by the hardware was done. */
#define CFC_REG_LL_INIT_DONE					 0x104074
/* [R 9] Number of allocated LCIDs which are at empty state */
#define CFC_REG_NUM_LCIDS_ALLOC 				 0x104020
/* [R 9] Number of Arriving LCIDs in Link List Block */
#define CFC_REG_NUM_LCIDS_ARRIVING				 0x104004
/* [R 9] Number of Leaving LCIDs in Link List Block */
#define CFC_REG_NUM_LCIDS_LEAVING				 0x104018
/* [RW 8] The event id for aggregated interrupt 0 */
#define CSDM_REG_AGG_INT_EVENT_0				 0xc2038
#define CSDM_REG_AGG_INT_EVENT_10				 0xc2060
#define CSDM_REG_AGG_INT_EVENT_11				 0xc2064
#define CSDM_REG_AGG_INT_EVENT_12				 0xc2068
#define CSDM_REG_AGG_INT_EVENT_13				 0xc206c
#define CSDM_REG_AGG_INT_EVENT_14				 0xc2070
#define CSDM_REG_AGG_INT_EVENT_15				 0xc2074
#define CSDM_REG_AGG_INT_EVENT_16				 0xc2078
#define CSDM_REG_AGG_INT_EVENT_2				 0xc2040
#define CSDM_REG_AGG_INT_EVENT_3				 0xc2044
#define CSDM_REG_AGG_INT_EVENT_4				 0xc2048
#define CSDM_REG_AGG_INT_EVENT_5				 0xc204c
#define CSDM_REG_AGG_INT_EVENT_6				 0xc2050
#define CSDM_REG_AGG_INT_EVENT_7				 0xc2054
#define CSDM_REG_AGG_INT_EVENT_8				 0xc2058
#define CSDM_REG_AGG_INT_EVENT_9				 0xc205c
/* [RW 1] For each aggregated interrupt index whether the mode is normal (0)
   or auto-mask-mode (1) */
#define CSDM_REG_AGG_INT_MODE_10				 0xc21e0
#define CSDM_REG_AGG_INT_MODE_11				 0xc21e4
#define CSDM_REG_AGG_INT_MODE_12				 0xc21e8
#define CSDM_REG_AGG_INT_MODE_13				 0xc21ec
#define CSDM_REG_AGG_INT_MODE_14				 0xc21f0
#define CSDM_REG_AGG_INT_MODE_15				 0xc21f4
#define CSDM_REG_AGG_INT_MODE_16				 0xc21f8
#define CSDM_REG_AGG_INT_MODE_6 				 0xc21d0
#define CSDM_REG_AGG_INT_MODE_7 				 0xc21d4
#define CSDM_REG_AGG_INT_MODE_8 				 0xc21d8
#define CSDM_REG_AGG_INT_MODE_9 				 0xc21dc
/* [RW 13] The start address in the internal RAM for the cfc_rsp lcid */
#define CSDM_REG_CFC_RSP_START_ADDR				 0xc2008
/* [RW 16] The maximum value of the competion counter #0 */
#define CSDM_REG_CMP_COUNTER_MAX0				 0xc201c
/* [RW 16] The maximum value of the competion counter #1 */
#define CSDM_REG_CMP_COUNTER_MAX1				 0xc2020
/* [RW 16] The maximum value of the competion counter #2 */
#define CSDM_REG_CMP_COUNTER_MAX2				 0xc2024
/* [RW 16] The maximum value of the competion counter #3 */
#define CSDM_REG_CMP_COUNTER_MAX3				 0xc2028
/* [RW 13] The start address in the internal RAM for the completion
   counters. */
#define CSDM_REG_CMP_COUNTER_START_ADDR 			 0xc200c
/* [RW 32] Interrupt mask register #0 read/write */
#define CSDM_REG_CSDM_INT_MASK_0				 0xc229c
#define CSDM_REG_CSDM_INT_MASK_1				 0xc22ac
/* [R 32] Interrupt register #0 read */
#define CSDM_REG_CSDM_INT_STS_0 				 0xc2290
#define CSDM_REG_CSDM_INT_STS_1 				 0xc22a0
/* [RW 11] Parity mask register #0 read/write */
#define CSDM_REG_CSDM_PRTY_MASK 				 0xc22bc
/* [R 11] Parity register #0 read */
#define CSDM_REG_CSDM_PRTY_STS					 0xc22b0
#define CSDM_REG_ENABLE_IN1					 0xc2238
#define CSDM_REG_ENABLE_IN2					 0xc223c
#define CSDM_REG_ENABLE_OUT1					 0xc2240
#define CSDM_REG_ENABLE_OUT2					 0xc2244
/* [RW 4] The initial number of messages that can be sent to the pxp control
   interface without receiving any ACK. */
#define CSDM_REG_INIT_CREDIT_PXP_CTRL				 0xc24bc
/* [ST 32] The number of ACK after placement messages received */
#define CSDM_REG_NUM_OF_ACK_AFTER_PLACE 			 0xc227c
/* [ST 32] The number of packet end messages received from the parser */
#define CSDM_REG_NUM_OF_PKT_END_MSG				 0xc2274
/* [ST 32] The number of requests received from the pxp async if */
#define CSDM_REG_NUM_OF_PXP_ASYNC_REQ				 0xc2278
/* [ST 32] The number of commands received in queue 0 */
#define CSDM_REG_NUM_OF_Q0_CMD					 0xc2248
/* [ST 32] The number of commands received in queue 10 */
#define CSDM_REG_NUM_OF_Q10_CMD 				 0xc226c
/* [ST 32] The number of commands received in queue 11 */
#define CSDM_REG_NUM_OF_Q11_CMD 				 0xc2270
/* [ST 32] The number of commands received in queue 1 */
#define CSDM_REG_NUM_OF_Q1_CMD					 0xc224c
/* [ST 32] The number of commands received in queue 3 */
#define CSDM_REG_NUM_OF_Q3_CMD					 0xc2250
/* [ST 32] The number of commands received in queue 4 */
#define CSDM_REG_NUM_OF_Q4_CMD					 0xc2254
/* [ST 32] The number of commands received in queue 5 */
#define CSDM_REG_NUM_OF_Q5_CMD					 0xc2258
/* [ST 32] The number of commands received in queue 6 */
#define CSDM_REG_NUM_OF_Q6_CMD					 0xc225c
/* [ST 32] The number of commands received in queue 7 */
#define CSDM_REG_NUM_OF_Q7_CMD					 0xc2260
/* [ST 32] The number of commands received in queue 8 */
#define CSDM_REG_NUM_OF_Q8_CMD					 0xc2264
/* [ST 32] The number of commands received in queue 9 */
#define CSDM_REG_NUM_OF_Q9_CMD					 0xc2268
/* [RW 13] The start address in the internal RAM for queue counters */
#define CSDM_REG_Q_COUNTER_START_ADDR				 0xc2010
/* [R 1] pxp_ctrl rd_data fifo empty in sdm_dma_rsp block */
#define CSDM_REG_RSP_PXP_CTRL_RDATA_EMPTY			 0xc2548
/* [R 1] parser fifo empty in sdm_sync block */
#define CSDM_REG_SYNC_PARSER_EMPTY				 0xc2550
/* [R 1] parser serial fifo empty in sdm_sync block */
#define CSDM_REG_SYNC_SYNC_EMPTY				 0xc2558
/* [RW 32] Tick for timer counter. Applicable only when
   ~csdm_registers_timer_tick_enable.timer_tick_enable =1 */
#define CSDM_REG_TIMER_TICK					 0xc2000
/* [RW 5] The number of time_slots in the arbitration cycle */
#define CSEM_REG_ARB_CYCLE_SIZE 				 0x200034
/* [RW 3] The source that is associated with arbitration element 0. Source
   decoding is: 0- foc0; 1-fic1; 2-sleeping thread with priority 0; 3-
   sleeping thread with priority 1; 4- sleeping thread with priority 2 */
#define CSEM_REG_ARB_ELEMENT0					 0x200020
/* [RW 3] The source that is associated with arbitration element 1. Source
   decoding is: 0- foc0; 1-fic1; 2-sleeping thread with priority 0; 3-
   sleeping thread with priority 1; 4- sleeping thread with priority 2.
   Could not be equal to register ~csem_registers_arb_element0.arb_element0 */
#define CSEM_REG_ARB_ELEMENT1					 0x200024
/* [RW 3] The source that is associated with arbitration element 2. Source
   decoding is: 0- foc0; 1-fic1; 2-sleeping thread with priority 0; 3-
   sleeping thread with priority 1; 4- sleeping thread with priority 2.
   Could not be equal to register ~csem_registers_arb_element0.arb_element0
   and ~csem_registers_arb_element1.arb_element1 */
#define CSEM_REG_ARB_ELEMENT2					 0x200028
/* [RW 3] The source that is associated with arbitration element 3. Source
   decoding is: 0- foc0; 1-fic1; 2-sleeping thread with priority 0; 3-
   sleeping thread with priority 1; 4- sleeping thread with priority 2.Could
   not be equal to register ~csem_registers_arb_element0.arb_element0 and
   ~csem_registers_arb_element1.arb_element1 and
   ~csem_registers_arb_element2.arb_element2 */
#define CSEM_REG_ARB_ELEMENT3					 0x20002c
/* [RW 3] The source that is associated with arbitration element 4. Source
   decoding is: 0- foc0; 1-fic1; 2-sleeping thread with priority 0; 3-
   sleeping thread with priority 1; 4- sleeping thread with priority 2.
   Could not be equal to register ~csem_registers_arb_element0.arb_element0
   and ~csem_registers_arb_element1.arb_element1 and
   ~csem_registers_arb_element2.arb_element2 and
   ~csem_registers_arb_element3.arb_element3 */
#define CSEM_REG_ARB_ELEMENT4					 0x200030
/* [RW 32] Interrupt mask register #0 read/write */
#define CSEM_REG_CSEM_INT_MASK_0				 0x200110
#define CSEM_REG_CSEM_INT_MASK_1				 0x200120
/* [R 32] Interrupt register #0 read */
#define CSEM_REG_CSEM_INT_STS_0 				 0x200104
#define CSEM_REG_CSEM_INT_STS_1 				 0x200114
/* [RW 32] Parity mask register #0 read/write */
#define CSEM_REG_CSEM_PRTY_MASK_0				 0x200130
#define CSEM_REG_CSEM_PRTY_MASK_1				 0x200140
/* [R 32] Parity register #0 read */
#define CSEM_REG_CSEM_PRTY_STS_0				 0x200124
#define CSEM_REG_CSEM_PRTY_STS_1				 0x200134
#define CSEM_REG_ENABLE_IN					 0x2000a4
#define CSEM_REG_ENABLE_OUT					 0x2000a8
/* [RW 32] This address space contains all registers and memories that are
   placed in SEM_FAST block. The SEM_FAST registers are described in
   appendix B. In order to access the sem_fast registers the base address
   ~fast_memory.fast_memory should be added to eachsem_fast register offset. */
#define CSEM_REG_FAST_MEMORY					 0x220000
/* [RW 1] Disables input messages from FIC0 May be updated during run_time
   by the microcode */
#define CSEM_REG_FIC0_DISABLE					 0x200224
/* [RW 1] Disables input messages from FIC1 May be updated during run_time
   by the microcode */
#define CSEM_REG_FIC1_DISABLE					 0x200234
/* [RW 15] Interrupt table Read and write access to it is not possible in
   the middle of the work */
#define CSEM_REG_INT_TABLE					 0x200400
/* [ST 24] Statistics register. The number of messages that entered through
   FIC0 */
#define CSEM_REG_MSG_NUM_FIC0					 0x200000
/* [ST 24] Statistics register. The number of messages that entered through
   FIC1 */
#define CSEM_REG_MSG_NUM_FIC1					 0x200004
/* [ST 24] Statistics register. The number of messages that were sent to
   FOC0 */
#define CSEM_REG_MSG_NUM_FOC0					 0x200008
/* [ST 24] Statistics register. The number of messages that were sent to
   FOC1 */
#define CSEM_REG_MSG_NUM_FOC1					 0x20000c
/* [ST 24] Statistics register. The number of messages that were sent to
   FOC2 */
#define CSEM_REG_MSG_NUM_FOC2					 0x200010
/* [ST 24] Statistics register. The number of messages that were sent to
   FOC3 */
#define CSEM_REG_MSG_NUM_FOC3					 0x200014
/* [RW 1] Disables input messages from the passive buffer May be updated
   during run_time by the microcode */
#define CSEM_REG_PAS_DISABLE					 0x20024c
/* [WB 128] Debug only. Passive buffer memory */
#define CSEM_REG_PASSIVE_BUFFER 				 0x202000
/* [WB 46] pram memory. B45 is parity; b[44:0] - data. */
#define CSEM_REG_PRAM						 0x240000
/* [R 16] Valid sleeping threads indication have bit per thread */
#define CSEM_REG_SLEEP_THREADS_VALID				 0x20026c
/* [R 1] EXT_STORE FIFO is empty in sem_slow_ls_ext */
#define CSEM_REG_SLOW_EXT_STORE_EMPTY				 0x2002a0
/* [RW 16] List of free threads . There is a bit per thread. */
#define CSEM_REG_THREADS_LIST					 0x2002e4
/* [RW 3] The arbitration scheme of time_slot 0 */
#define CSEM_REG_TS_0_AS					 0x200038
/* [RW 3] The arbitration scheme of time_slot 10 */
#define CSEM_REG_TS_10_AS					 0x200060
/* [RW 3] The arbitration scheme of time_slot 11 */
#define CSEM_REG_TS_11_AS					 0x200064
/* [RW 3] The arbitration scheme of time_slot 12 */
#define CSEM_REG_TS_12_AS					 0x200068
/* [RW 3] The arbitration scheme of time_slot 13 */
#define CSEM_REG_TS_13_AS					 0x20006c
/* [RW 3] The arbitration scheme of time_slot 14 */
#define CSEM_REG_TS_14_AS					 0x200070
/* [RW 3] The arbitration scheme of time_slot 15 */
#define CSEM_REG_TS_15_AS					 0x200074
/* [RW 3] The arbitration scheme of time_slot 16 */
#define CSEM_REG_TS_16_AS					 0x200078
/* [RW 3] The arbitration scheme of time_slot 17 */
#define CSEM_REG_TS_17_AS					 0x20007c
/* [RW 3] The arbitration scheme of time_slot 18 */
#define CSEM_REG_TS_18_AS					 0x200080
/* [RW 3] The arbitration scheme of time_slot 1 */
#define CSEM_REG_TS_1_AS					 0x20003c
/* [RW 3] The arbitration scheme of time_slot 2 */
#define CSEM_REG_TS_2_AS					 0x200040
/* [RW 3] The arbitration scheme of time_slot 3 */
#define CSEM_REG_TS_3_AS					 0x200044
/* [RW 3] The arbitration scheme of time_slot 4 */
#define CSEM_REG_TS_4_AS					 0x200048
/* [RW 3] The arbitration scheme of time_slot 5 */
#define CSEM_REG_TS_5_AS					 0x20004c
/* [RW 3] The arbitration scheme of time_slot 6 */
#define CSEM_REG_TS_6_AS					 0x200050
/* [RW 3] The arbitration scheme of time_slot 7 */
#define CSEM_REG_TS_7_AS					 0x200054
/* [RW 3] The arbitration scheme of time_slot 8 */
#define CSEM_REG_TS_8_AS					 0x200058
/* [RW 3] The arbitration scheme of time_slot 9 */
#define CSEM_REG_TS_9_AS					 0x20005c
/* [RW 1] Parity mask register #0 read/write */
#define DBG_REG_DBG_PRTY_MASK					 0xc0a8
/* [R 1] Parity register #0 read */
#define DBG_REG_DBG_PRTY_STS					 0xc09c
/* [RW 32] Commands memory. The address to command X; row Y is to calculated
   as 14*X+Y. */
#define DMAE_REG_CMD_MEM					 0x102400
#define DMAE_REG_CMD_MEM_SIZE					 224
/* [RW 1] If 0 - the CRC-16c initial value is all zeroes; if 1 - the CRC-16c
   initial value is all ones. */
#define DMAE_REG_CRC16C_INIT					 0x10201c
/* [RW 1] If 0 - the CRC-16 T10 initial value is all zeroes; if 1 - the
   CRC-16 T10 initial value is all ones. */
#define DMAE_REG_CRC16T10_INIT					 0x102020
/* [RW 2] Interrupt mask register #0 read/write */
#define DMAE_REG_DMAE_INT_MASK					 0x102054
/* [RW 4] Parity mask register #0 read/write */
#define DMAE_REG_DMAE_PRTY_MASK 				 0x102064
/* [R 4] Parity register #0 read */
#define DMAE_REG_DMAE_PRTY_STS					 0x102058
/* [RW 1] Command 0 go. */
#define DMAE_REG_GO_C0						 0x102080
/* [RW 1] Command 1 go. */
#define DMAE_REG_GO_C1						 0x102084
/* [RW 1] Command 10 go. */
#define DMAE_REG_GO_C10 					 0x102088
/* [RW 1] Command 11 go. */
#define DMAE_REG_GO_C11 					 0x10208c
/* [RW 1] Command 12 go. */
#define DMAE_REG_GO_C12 					 0x102090
/* [RW 1] Command 13 go. */
#define DMAE_REG_GO_C13 					 0x102094
/* [RW 1] Command 14 go. */
#define DMAE_REG_GO_C14 					 0x102098
/* [RW 1] Command 15 go. */
#define DMAE_REG_GO_C15 					 0x10209c
/* [RW 1] Command 2 go. */
#define DMAE_REG_GO_C2						 0x1020a0
/* [RW 1] Command 3 go. */
#define DMAE_REG_GO_C3						 0x1020a4
/* [RW 1] Command 4 go. */
#define DMAE_REG_GO_C4						 0x1020a8
/* [RW 1] Command 5 go. */
#define DMAE_REG_GO_C5						 0x1020ac
/* [RW 1] Command 6 go. */
#define DMAE_REG_GO_C6						 0x1020b0
/* [RW 1] Command 7 go. */
#define DMAE_REG_GO_C7						 0x1020b4
/* [RW 1] Command 8 go. */
#define DMAE_REG_GO_C8						 0x1020b8
/* [RW 1] Command 9 go. */
#define DMAE_REG_GO_C9						 0x1020bc
/* [RW 1] DMAE GRC Interface (Target; aster) enable. If 0 - the acknowledge
   input is disregarded; valid is deasserted; all other signals are treated
   as usual; if 1 - normal activity. */
#define DMAE_REG_GRC_IFEN					 0x102008
/* [RW 1] DMAE PCI Interface (Request; ead; rite) enable. If 0 - the
   acknowledge input is disregarded; valid is deasserted; full is asserted;
   all other signals are treated as usual; if 1 - normal activity. */
#define DMAE_REG_PCI_IFEN					 0x102004
/* [RW 4] DMAE- PCI Request Interface initial credit. Write writes the
   initial value to the credit counter; related to the address. Read returns
   the current value of the counter. */
#define DMAE_REG_PXP_REQ_INIT_CRD				 0x1020c0
/* [RW 8] Aggregation command. */
#define DORQ_REG_AGG_CMD0					 0x170060
/* [RW 8] Aggregation command. */
#define DORQ_REG_AGG_CMD1					 0x170064
/* [RW 8] Aggregation command. */
#define DORQ_REG_AGG_CMD2					 0x170068
/* [RW 8] Aggregation command. */
#define DORQ_REG_AGG_CMD3					 0x17006c
/* [RW 28] UCM Header. */
#define DORQ_REG_CMHEAD_RX					 0x170050
/* [RW 32] Doorbell address for RBC doorbells (function 0). */
#define DORQ_REG_DB_ADDR0					 0x17008c
/* [RW 5] Interrupt mask register #0 read/write */
#define DORQ_REG_DORQ_INT_MASK					 0x170180
/* [R 5] Interrupt register #0 read */
#define DORQ_REG_DORQ_INT_STS					 0x170174
/* [RC 5] Interrupt register #0 read clear */
#define DORQ_REG_DORQ_INT_STS_CLR				 0x170178
/* [RW 2] Parity mask register #0 read/write */
#define DORQ_REG_DORQ_PRTY_MASK 				 0x170190
/* [R 2] Parity register #0 read */
#define DORQ_REG_DORQ_PRTY_STS					 0x170184
/* [RW 8] The address to write the DPM CID to STORM. */
#define DORQ_REG_DPM_CID_ADDR					 0x170044
/* [RW 5] The DPM mode CID extraction offset. */
#define DORQ_REG_DPM_CID_OFST					 0x170030
/* [RW 12] The threshold of the DQ FIFO to send the almost full interrupt. */
#define DORQ_REG_DQ_FIFO_AFULL_TH				 0x17007c
/* [RW 12] The threshold of the DQ FIFO to send the full interrupt. */
#define DORQ_REG_DQ_FIFO_FULL_TH				 0x170078
/* [R 13] Current value of the DQ FIFO fill level according to following
   pointer. The range is 0 - 256 FIFO rows; where each row stands for the
   doorbell. */
#define DORQ_REG_DQ_FILL_LVLF					 0x1700a4
/* [R 1] DQ FIFO full status. Is set; when FIFO filling level is more or
   equal to full threshold; reset on full clear. */
#define DORQ_REG_DQ_FULL_ST					 0x1700c0
/* [RW 28] The value sent to CM header in the case of CFC load error. */
#define DORQ_REG_ERR_CMHEAD					 0x170058
#define DORQ_REG_IF_EN						 0x170004
#define DORQ_REG_MODE_ACT					 0x170008
/* [RW 5] The normal mode CID extraction offset. */
#define DORQ_REG_NORM_CID_OFST					 0x17002c
/* [RW 28] TCM Header when only TCP context is loaded. */
#define DORQ_REG_NORM_CMHEAD_TX 				 0x17004c
/* [RW 3] The number of simultaneous outstanding requests to Context Fetch
   Interface. */
#define DORQ_REG_OUTST_REQ					 0x17003c
#define DORQ_REG_REGN						 0x170038
/* [R 4] Current value of response A counter credit. Initial credit is
   configured through write to ~dorq_registers_rsp_init_crd.rsp_init_crd
   register. */
#define DORQ_REG_RSPA_CRD_CNT					 0x1700ac
/* [R 4] Current value of response B counter credit. Initial credit is
   configured through write to ~dorq_registers_rsp_init_crd.rsp_init_crd
   register. */
#define DORQ_REG_RSPB_CRD_CNT					 0x1700b0
/* [RW 4] The initial credit at the Doorbell Response Interface. The write
   writes the same initial credit to the rspa_crd_cnt and rspb_crd_cnt. The
   read reads this written value. */
#define DORQ_REG_RSP_INIT_CRD					 0x170048
/* [RW 4] Initial activity counter value on the load request; when the
   shortcut is done. */
#define DORQ_REG_SHRT_ACT_CNT					 0x170070
/* [RW 28] TCM Header when both ULP and TCP context is loaded. */
#define DORQ_REG_SHRT_CMHEAD					 0x170054
#define HC_CONFIG_0_REG_ATTN_BIT_EN_0				 (0x1<<4)
#define HC_CONFIG_0_REG_INT_LINE_EN_0				 (0x1<<3)
#define HC_CONFIG_0_REG_MSI_ATTN_EN_0				 (0x1<<7)
#define HC_CONFIG_0_REG_MSI_MSIX_INT_EN_0			 (0x1<<2)
#define HC_CONFIG_0_REG_SINGLE_ISR_EN_0 			 (0x1<<1)
#define HC_REG_AGG_INT_0					 0x108050
#define HC_REG_AGG_INT_1					 0x108054
#define HC_REG_ATTN_BIT 					 0x108120
#define HC_REG_ATTN_IDX 					 0x108100
#define HC_REG_ATTN_MSG0_ADDR_L 				 0x108018
#define HC_REG_ATTN_MSG1_ADDR_L 				 0x108020
#define HC_REG_ATTN_NUM_P0					 0x108038
#define HC_REG_ATTN_NUM_P1					 0x10803c
#define HC_REG_COMMAND_REG					 0x108180
#define HC_REG_CONFIG_0 					 0x108000
#define HC_REG_CONFIG_1 					 0x108004
#define HC_REG_FUNC_NUM_P0					 0x1080ac
#define HC_REG_FUNC_NUM_P1					 0x1080b0
/* [RW 3] Parity mask register #0 read/write */
#define HC_REG_HC_PRTY_MASK					 0x1080a0
/* [R 3] Parity register #0 read */
#define HC_REG_HC_PRTY_STS					 0x108094
#define HC_REG_INT_MASK 					 0x108108
#define HC_REG_LEADING_EDGE_0					 0x108040
#define HC_REG_LEADING_EDGE_1					 0x108048
#define HC_REG_P0_PROD_CONS					 0x108200
#define HC_REG_P1_PROD_CONS					 0x108400
#define HC_REG_PBA_COMMAND					 0x108140
#define HC_REG_PCI_CONFIG_0					 0x108010
#define HC_REG_PCI_CONFIG_1					 0x108014
#define HC_REG_STATISTIC_COUNTERS				 0x109000
#define HC_REG_TRAILING_EDGE_0					 0x108044
#define HC_REG_TRAILING_EDGE_1					 0x10804c
#define HC_REG_UC_RAM_ADDR_0					 0x108028
#define HC_REG_UC_RAM_ADDR_1					 0x108030
#define HC_REG_USTORM_ADDR_FOR_COALESCE 			 0x108068
#define HC_REG_VQID_0						 0x108008
#define HC_REG_VQID_1						 0x10800c
#define MCP_REG_MCPR_NVM_ACCESS_ENABLE				 0x86424
#define MCP_REG_MCPR_NVM_ADDR					 0x8640c
#define MCP_REG_MCPR_NVM_CFG4					 0x8642c
#define MCP_REG_MCPR_NVM_COMMAND				 0x86400
#define MCP_REG_MCPR_NVM_READ					 0x86410
#define MCP_REG_MCPR_NVM_SW_ARB 				 0x86420
#define MCP_REG_MCPR_NVM_WRITE					 0x86408
#define MCP_REG_MCPR_SCRATCH					 0xa0000
/* [R 32] read first 32 bit after inversion of function 0. mapped as
   follows: [0] NIG attention for function0; [1] NIG attention for
   function1; [2] GPIO1 mcp; [3] GPIO2 mcp; [4] GPIO3 mcp; [5] GPIO4 mcp;
   [6] GPIO1 function 1; [7] GPIO2 function 1; [8] GPIO3 function 1; [9]
   GPIO4 function 1; [10] PCIE glue/PXP VPD event function0; [11] PCIE
   glue/PXP VPD event function1; [12] PCIE glue/PXP Expansion ROM event0;
   [13] PCIE glue/PXP Expansion ROM event1; [14] SPIO4; [15] SPIO5; [16]
   MSI/X indication for mcp; [17] MSI/X indication for function 1; [18] BRB
   Parity error; [19] BRB Hw interrupt; [20] PRS Parity error; [21] PRS Hw
   interrupt; [22] SRC Parity error; [23] SRC Hw interrupt; [24] TSDM Parity
   error; [25] TSDM Hw interrupt; [26] TCM Parity error; [27] TCM Hw
   interrupt; [28] TSEMI Parity error; [29] TSEMI Hw interrupt; [30] PBF
   Parity error; [31] PBF Hw interrupt; */
#define MISC_REG_AEU_AFTER_INVERT_1_FUNC_0			 0xa42c
#define MISC_REG_AEU_AFTER_INVERT_1_FUNC_1			 0xa430
/* [R 32] read first 32 bit after inversion of mcp. mapped as follows: [0]
   NIG attention for function0; [1] NIG attention for function1; [2] GPIO1
   mcp; [3] GPIO2 mcp; [4] GPIO3 mcp; [5] GPIO4 mcp; [6] GPIO1 function 1;
   [7] GPIO2 function 1; [8] GPIO3 function 1; [9] GPIO4 function 1; [10]
   PCIE glue/PXP VPD event function0; [11] PCIE glue/PXP VPD event
   function1; [12] PCIE glue/PXP Expansion ROM event0; [13] PCIE glue/PXP
   Expansion ROM event1; [14] SPIO4; [15] SPIO5; [16] MSI/X indication for
   mcp; [17] MSI/X indication for function 1; [18] BRB Parity error; [19]
   BRB Hw interrupt; [20] PRS Parity error; [21] PRS Hw interrupt; [22] SRC
   Parity error; [23] SRC Hw interrupt; [24] TSDM Parity error; [25] TSDM Hw
   interrupt; [26] TCM Parity error; [27] TCM Hw interrupt; [28] TSEMI
   Parity error; [29] TSEMI Hw interrupt; [30] PBF Parity error; [31] PBF Hw
   interrupt; */
#define MISC_REG_AEU_AFTER_INVERT_1_MCP 			 0xa434
/* [R 32] read second 32 bit after inversion of function 0. mapped as
   follows: [0] PBClient Parity error; [1] PBClient Hw interrupt; [2] QM
   Parity error; [3] QM Hw interrupt; [4] Timers Parity error; [5] Timers Hw
   interrupt; [6] XSDM Parity error; [7] XSDM Hw interrupt; [8] XCM Parity
   error; [9] XCM Hw interrupt; [10] XSEMI Parity error; [11] XSEMI Hw
   interrupt; [12] DoorbellQ Parity error; [13] DoorbellQ Hw interrupt; [14]
   NIG Parity error; [15] NIG Hw interrupt; [16] Vaux PCI core Parity error;
   [17] Vaux PCI core Hw interrupt; [18] Debug Parity error; [19] Debug Hw
   interrupt; [20] USDM Parity error; [21] USDM Hw interrupt; [22] UCM
   Parity error; [23] UCM Hw interrupt; [24] USEMI Parity error; [25] USEMI
   Hw interrupt; [26] UPB Parity error; [27] UPB Hw interrupt; [28] CSDM
   Parity error; [29] CSDM Hw interrupt; [30] CCM Parity error; [31] CCM Hw
   interrupt; */
#define MISC_REG_AEU_AFTER_INVERT_2_FUNC_0			 0xa438
#define MISC_REG_AEU_AFTER_INVERT_2_FUNC_1			 0xa43c
/* [R 32] read second 32 bit after inversion of mcp. mapped as follows: [0]
   PBClient Parity error; [1] PBClient Hw interrupt; [2] QM Parity error;
   [3] QM Hw interrupt; [4] Timers Parity error; [5] Timers Hw interrupt;
   [6] XSDM Parity error; [7] XSDM Hw interrupt; [8] XCM Parity error; [9]
   XCM Hw interrupt; [10] XSEMI Parity error; [11] XSEMI Hw interrupt; [12]
   DoorbellQ Parity error; [13] DoorbellQ Hw interrupt; [14] NIG Parity
   error; [15] NIG Hw interrupt; [16] Vaux PCI core Parity error; [17] Vaux
   PCI core Hw interrupt; [18] Debug Parity error; [19] Debug Hw interrupt;
   [20] USDM Parity error; [21] USDM Hw interrupt; [22] UCM Parity error;
   [23] UCM Hw interrupt; [24] USEMI Parity error; [25] USEMI Hw interrupt;
   [26] UPB Parity error; [27] UPB Hw interrupt; [28] CSDM Parity error;
   [29] CSDM Hw interrupt; [30] CCM Parity error; [31] CCM Hw interrupt; */
#define MISC_REG_AEU_AFTER_INVERT_2_MCP 			 0xa440
/* [R 32] read third 32 bit after inversion of function 0. mapped as
   follows: [0] CSEMI Parity error; [1] CSEMI Hw interrupt; [2] PXP Parity
   error; [3] PXP Hw interrupt; [4] PXPpciClockClient Parity error; [5]
   PXPpciClockClient Hw interrupt; [6] CFC Parity error; [7] CFC Hw
   interrupt; [8] CDU Parity error; [9] CDU Hw interrupt; [10] DMAE Parity
   error; [11] DMAE Hw interrupt; [12] IGU (HC) Parity error; [13] IGU (HC)
   Hw interrupt; [14] MISC Parity error; [15] MISC Hw interrupt; [16]
   pxp_misc_mps_attn; [17] Flash event; [18] SMB event; [19] MCP attn0; [20]
   MCP attn1; [21] SW timers attn_1 func0; [22] SW timers attn_2 func0; [23]
   SW timers attn_3 func0; [24] SW timers attn_4 func0; [25] PERST; [26] SW
   timers attn_1 func1; [27] SW timers attn_2 func1; [28] SW timers attn_3
   func1; [29] SW timers attn_4 func1; [30] General attn0; [31] General
   attn1; */
#define MISC_REG_AEU_AFTER_INVERT_3_FUNC_0			 0xa444
#define MISC_REG_AEU_AFTER_INVERT_3_FUNC_1			 0xa448
/* [R 32] read third 32 bit after inversion of mcp. mapped as follows: [0]
   CSEMI Parity error; [1] CSEMI Hw interrupt; [2] PXP Parity error; [3] PXP
   Hw interrupt; [4] PXPpciClockClient Parity error; [5] PXPpciClockClient
   Hw interrupt; [6] CFC Parity error; [7] CFC Hw interrupt; [8] CDU Parity
   error; [9] CDU Hw interrupt; [10] DMAE Parity error; [11] DMAE Hw
   interrupt; [12] IGU (HC) Parity error; [13] IGU (HC) Hw interrupt; [14]
   MISC Parity error; [15] MISC Hw interrupt; [16] pxp_misc_mps_attn; [17]
   Flash event; [18] SMB event; [19] MCP attn0; [20] MCP attn1; [21] SW
   timers attn_1 func0; [22] SW timers attn_2 func0; [23] SW timers attn_3
   func0; [24] SW timers attn_4 func0; [25] PERST; [26] SW timers attn_1
   func1; [27] SW timers attn_2 func1; [28] SW timers attn_3 func1; [29] SW
   timers attn_4 func1; [30] General attn0; [31] General attn1; */
#define MISC_REG_AEU_AFTER_INVERT_3_MCP 			 0xa44c
/* [R 32] read fourth 32 bit after inversion of function 0. mapped as
   follows: [0] General attn2; [1] General attn3; [2] General attn4; [3]
   General attn5; [4] General attn6; [5] General attn7; [6] General attn8;
   [7] General attn9; [8] General attn10; [9] General attn11; [10] General
   attn12; [11] General attn13; [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16] General attn18; [17]
   General attn19; [18] General attn20; [19] General attn21; [20] Main power
   interrupt; [21] RBCR Latched attn; [22] RBCT Latched attn; [23] RBCN
   Latched attn; [24] RBCU Latched attn; [25] RBCP Latched attn; [26] GRC
   Latched timeout attention; [27] GRC Latched reserved access attention;
   [28] MCP Latched rom_parity; [29] MCP Latched ump_rx_parity; [30] MCP
   Latched ump_tx_parity; [31] MCP Latched scpad_parity; */
#define MISC_REG_AEU_AFTER_INVERT_4_FUNC_0			 0xa450
#define MISC_REG_AEU_AFTER_INVERT_4_FUNC_1			 0xa454
/* [R 32] read fourth 32 bit after inversion of mcp. mapped as follows: [0]
   General attn2; [1] General attn3; [2] General attn4; [3] General attn5;
   [4] General attn6; [5] General attn7; [6] General attn8; [7] General
   attn9; [8] General attn10; [9] General attn11; [10] General attn12; [11]
   General attn13; [12] General attn14; [13] General attn15; [14] General
   attn16; [15] General attn17; [16] General attn18; [17] General attn19;
   [18] General attn20; [19] General attn21; [20] Main power interrupt; [21]
   RBCR Latched attn; [22] RBCT Latched attn; [23] RBCN Latched attn; [24]
   RBCU Latched attn; [25] RBCP Latched attn; [26] GRC Latched timeout
   attention; [27] GRC Latched reserved access attention; [28] MCP Latched
   rom_parity; [29] MCP Latched ump_rx_parity; [30] MCP Latched
   ump_tx_parity; [31] MCP Latched scpad_parity; */
#define MISC_REG_AEU_AFTER_INVERT_4_MCP 			 0xa458
/* [W 14] write to this register results with the clear of the latched
   signals; one in d0 clears RBCR latch; one in d1 clears RBCT latch; one in
   d2 clears RBCN latch; one in d3 clears RBCU latch; one in d4 clears RBCP
   latch; one in d5 clears GRC Latched timeout attention; one in d6 clears
   GRC Latched reserved access attention; one in d7 clears Latched
   rom_parity; one in d8 clears Latched ump_rx_parity; one in d9 clears
   Latched ump_tx_parity; one in d10 clears Latched scpad_parity (both
   ports); one in d11 clears pxpv_misc_mps_attn; one in d12 clears
   pxp_misc_exp_rom_attn0; one in d13 clears pxp_misc_exp_rom_attn1; read
   from this register return zero */
#define MISC_REG_AEU_CLR_LATCH_SIGNAL				 0xa45c
/* [RW 32] first 32b for enabling the output for function 0 output0. mapped
   as follows: [0] NIG attention for function0; [1] NIG attention for
   function1; [2] GPIO1 function 0; [3] GPIO2 function 0; [4] GPIO3 function
   0; [5] GPIO4 function 0; [6] GPIO1 function 1; [7] GPIO2 function 1; [8]
   GPIO3 function 1; [9] GPIO4 function 1; [10] PCIE glue/PXP VPD event
   function0; [11] PCIE glue/PXP VPD event function1; [12] PCIE glue/PXP
   Expansion ROM event0; [13] PCIE glue/PXP Expansion ROM event1; [14]
   SPIO4; [15] SPIO5; [16] MSI/X indication for function 0; [17] MSI/X
   indication for function 1; [18] BRB Parity error; [19] BRB Hw interrupt;
   [20] PRS Parity error; [21] PRS Hw interrupt; [22] SRC Parity error; [23]
   SRC Hw interrupt; [24] TSDM Parity error; [25] TSDM Hw interrupt; [26]
   TCM Parity error; [27] TCM Hw interrupt; [28] TSEMI Parity error; [29]
   TSEMI Hw interrupt; [30] PBF Parity error; [31] PBF Hw interrupt; */
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_0			 0xa06c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_1			 0xa07c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_2			 0xa08c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_3			 0xa09c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_5			 0xa0bc
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_6			 0xa0cc
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_7			 0xa0dc
/* [RW 32] first 32b for enabling the output for function 1 output0. mapped
   as follows: [0] NIG attention for function0; [1] NIG attention for
   function1; [2] GPIO1 function 1; [3] GPIO2 function 1; [4] GPIO3 function
   1; [5] GPIO4 function 1; [6] GPIO1 function 1; [7] GPIO2 function 1; [8]
   GPIO3 function 1; [9] GPIO4 function 1; [10] PCIE glue/PXP VPD event
   function0; [11] PCIE glue/PXP VPD event function1; [12] PCIE glue/PXP
   Expansion ROM event0; [13] PCIE glue/PXP Expansion ROM event1; [14]
   SPIO4; [15] SPIO5; [16] MSI/X indication for function 1; [17] MSI/X
   indication for function 1; [18] BRB Parity error; [19] BRB Hw interrupt;
   [20] PRS Parity error; [21] PRS Hw interrupt; [22] SRC Parity error; [23]
   SRC Hw interrupt; [24] TSDM Parity error; [25] TSDM Hw interrupt; [26]
   TCM Parity error; [27] TCM Hw interrupt; [28] TSEMI Parity error; [29]
   TSEMI Hw interrupt; [30] PBF Parity error; [31] PBF Hw interrupt; */
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_0			 0xa10c
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_1			 0xa11c
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_2			 0xa12c
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_3			 0xa13c
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_5			 0xa15c
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_6			 0xa16c
#define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_7			 0xa17c
/* [RW 32] first 32b for enabling the output for close the gate nig. mapped
   as follows: [0] NIG attention for function0; [1] NIG attention for
   function1; [2] GPIO1 function 0; [3] GPIO2 function 0; [4] GPIO3 function
   0; [5] GPIO4 function 0; [6] GPIO1 function 1; [7] GPIO2 function 1; [8]
   GPIO3 function 1; [9] GPIO4 function 1; [10] PCIE glue/PXP VPD event
   function0; [11] PCIE glue/PXP VPD event function1; [12] PCIE glue/PXP
   Expansion ROM event0; [13] PCIE glue/PXP Expansion ROM event1; [14]
   SPIO4; [15] SPIO5; [16] MSI/X indication for function 0; [17] MSI/X
   indication for function 1; [18] BRB Parity error; [19] BRB Hw interrupt;
   [20] PRS Parity error; [21] PRS Hw interrupt; [22] SRC Parity error; [23]
   SRC Hw interrupt; [24] TSDM Parity error; [25] TSDM Hw interrupt; [26]
   TCM Parity error; [27] TCM Hw interrupt; [28] TSEMI Parity error; [29]
   TSEMI Hw interrupt; [30] PBF Parity error; [31] PBF Hw interrupt; */
#define MISC_REG_AEU_ENABLE1_NIG_0				 0xa0ec
#define MISC_REG_AEU_ENABLE1_NIG_1				 0xa18c
/* [RW 32] first 32b for enabling the output for close the gate pxp. mapped
   as follows: [0] NIG attention for function0; [1] NIG attention for
   function1; [2] GPIO1 function 0; [3] GPIO2 function 0; [4] GPIO3 function
   0; [5] GPIO4 function 0; [6] GPIO1 function 1; [7] GPIO2 function 1; [8]
   GPIO3 function 1; [9] GPIO4 function 1; [10] PCIE glue/PXP VPD event
   function0; [11] PCIE glue/PXP VPD event function1; [12] PCIE glue/PXP
   Expansion ROM event0; [13] PCIE glue/PXP Expansion ROM event1; [14]
   SPIO4; [15] SPIO5; [16] MSI/X indication for function 0; [17] MSI/X
   indication for function 1; [18] BRB Parity error; [19] BRB Hw interrupt;
   [20] PRS Parity error; [21] PRS Hw interrupt; [22] SRC Parity error; [23]
   SRC Hw interrupt; [24] TSDM Parity error; [25] TSDM Hw interrupt; [26]
   TCM Parity error; [27] TCM Hw interrupt; [28] TSEMI Parity error; [29]
   TSEMI Hw interrupt; [30] PBF Parity error; [31] PBF Hw interrupt; */
#define MISC_REG_AEU_ENABLE1_PXP_0				 0xa0fc
#define MISC_REG_AEU_ENABLE1_PXP_1				 0xa19c
/* [RW 32] second 32b for enabling the output for function 0 output0. mapped
   as follows: [0] PBClient Parity error; [1] PBClient Hw interrupt; [2] QM
   Parity error; [3] QM Hw interrupt; [4] Timers Parity error; [5] Timers Hw
   interrupt; [6] XSDM Parity error; [7] XSDM Hw interrupt; [8] XCM Parity
   error; [9] XCM Hw interrupt; [10] XSEMI Parity error; [11] XSEMI Hw
   interrupt; [12] DoorbellQ Parity error; [13] DoorbellQ Hw interrupt; [14]
   NIG Parity error; [15] NIG Hw interrupt; [16] Vaux PCI core Parity error;
   [17] Vaux PCI core Hw interrupt; [18] Debug Parity error; [19] Debug Hw
   interrupt; [20] USDM Parity error; [21] USDM Hw interrupt; [22] UCM
   Parity error; [23] UCM Hw interrupt; [24] USEMI Parity error; [25] USEMI
   Hw interrupt; [26] UPB Parity error; [27] UPB Hw interrupt; [28] CSDM
   Parity error; [29] CSDM Hw interrupt; [30] CCM Parity error; [31] CCM Hw
   interrupt; */
#define MISC_REG_AEU_ENABLE2_FUNC_0_OUT_0			 0xa070
#define MISC_REG_AEU_ENABLE2_FUNC_0_OUT_1			 0xa080
/* [RW 32] second 32b for enabling the output for function 1 output0. mapped
   as follows: [0] PBClient Parity error; [1] PBClient Hw interrupt; [2] QM
   Parity error; [3] QM Hw interrupt; [4] Timers Parity error; [5] Timers Hw
   interrupt; [6] XSDM Parity error; [7] XSDM Hw interrupt; [8] XCM Parity
   error; [9] XCM Hw interrupt; [10] XSEMI Parity error; [11] XSEMI Hw
   interrupt; [12] DoorbellQ Parity error; [13] DoorbellQ Hw interrupt; [14]
   NIG Parity error; [15] NIG Hw interrupt; [16] Vaux PCI core Parity error;
   [17] Vaux PCI core Hw interrupt; [18] Debug Parity error; [19] Debug Hw
   interrupt; [20] USDM Parity error; [21] USDM Hw interrupt; [22] UCM
   Parity error; [23] UCM Hw interrupt; [24] USEMI Parity error; [25] USEMI
   Hw interrupt; [26] UPB Parity error; [27] UPB Hw interrupt; [28] CSDM
   Parity error; [29] CSDM Hw interrupt; [30] CCM Parity error; [31] CCM Hw
   interrupt; */
#define MISC_REG_AEU_ENABLE2_FUNC_1_OUT_0			 0xa110
#define MISC_REG_AEU_ENABLE2_FUNC_1_OUT_1			 0xa120
/* [RW 32] second 32b for enabling the output for close the gate nig. mapped
   as follows: [0] PBClient Parity error; [1] PBClient Hw interrupt; [2] QM
   Parity error; [3] QM Hw interrupt; [4] Timers Parity error; [5] Timers Hw
   interrupt; [6] XSDM Parity error; [7] XSDM Hw interrupt; [8] XCM Parity
   error; [9] XCM Hw interrupt; [10] XSEMI Parity error; [11] XSEMI Hw
   interrupt; [12] DoorbellQ Parity error; [13] DoorbellQ Hw interrupt; [14]
   NIG Parity error; [15] NIG Hw interrupt; [16] Vaux PCI core Parity error;
   [17] Vaux PCI core Hw interrupt; [18] Debug Parity error; [19] Debug Hw
   interrupt; [20] USDM Parity error; [21] USDM Hw interrupt; [22] UCM
   Parity error; [23] UCM Hw interrupt; [24] USEMI Parity error; [25] USEMI
   Hw interrupt; [26] UPB Parity error; [27] UPB Hw interrupt; [28] CSDM
   Parity error; [29] CSDM Hw interrupt; [30] CCM Parity error; [31] CCM Hw
   interrupt; */
#define MISC_REG_AEU_ENABLE2_NIG_0				 0xa0f0
#define MISC_REG_AEU_ENABLE2_NIG_1				 0xa190
/* [RW 32] second 32b for enabling the output for close the gate pxp. mapped
   as follows: [0] PBClient Parity error; [1] PBClient Hw interrupt; [2] QM
   Parity error; [3] QM Hw interrupt; [4] Timers Parity error; [5] Timers Hw
   interrupt; [6] XSDM Parity error; [7] XSDM Hw interrupt; [8] XCM Parity
   error; [9] XCM Hw interrupt; [10] XSEMI Parity error; [11] XSEMI Hw
   interrupt; [12] DoorbellQ Parity error; [13] DoorbellQ Hw interrupt; [14]
   NIG Parity error; [15] NIG Hw interrupt; [16] Vaux PCI core Parity error;
   [17] Vaux PCI core Hw interrupt; [18] Debug Parity error; [19] Debug Hw
   interrupt; [20] USDM Parity error; [21] USDM Hw interrupt; [22] UCM
   Parity error; [23] UCM Hw interrupt; [24] USEMI Parity error; [25] USEMI
   Hw interrupt; [26] UPB Parity error; [27] UPB Hw interrupt; [28] CSDM
   Parity error; [29] CSDM Hw interrupt; [30] CCM Parity error; [31] CCM Hw
   interrupt; */
#define MISC_REG_AEU_ENABLE2_PXP_0				 0xa100
#define MISC_REG_AEU_ENABLE2_PXP_1				 0xa1a0
/* [RW 32] third 32b for enabling the output for function 0 output0. mapped
   as follows: [0] CSEMI Parity error; [1] CSEMI Hw interrupt; [2] PXP
   Parity error; [3] PXP Hw interrupt; [4] PXPpciClockClient Parity error;
   [5] PXPpciClockClient Hw interrupt; [6] CFC Parity error; [7] CFC Hw
   interrupt; [8] CDU Parity error; [9] CDU Hw interrupt; [10] DMAE Parity
   error; [11] DMAE Hw interrupt; [12] IGU (HC) Parity error; [13] IGU (HC)
   Hw interrupt; [14] MISC Parity error; [15] MISC Hw interrupt; [16]
   pxp_misc_mps_attn; [17] Flash event; [18] SMB event; [19] MCP attn0; [20]
   MCP attn1; [21] SW timers attn_1 func0; [22] SW timers attn_2 func0; [23]
   SW timers attn_3 func0; [24] SW timers attn_4 func0; [25] PERST; [26] SW
   timers attn_1 func1; [27] SW timers attn_2 func1; [28] SW timers attn_3
   func1; [29] SW timers attn_4 func1; [30] General attn0; [31] General
   attn1; */
#define MISC_REG_AEU_ENABLE3_FUNC_0_OUT_0			 0xa074
#define MISC_REG_AEU_ENABLE3_FUNC_0_OUT_1			 0xa084
/* [RW 32] third 32b for enabling the output for function 1 output0. mapped
   as follows: [0] CSEMI Parity error; [1] CSEMI Hw interrupt; [2] PXP
   Parity error; [3] PXP Hw interrupt; [4] PXPpciClockClient Parity error;
   [5] PXPpciClockClient Hw interrupt; [6] CFC Parity error; [7] CFC Hw
   interrupt; [8] CDU Parity error; [9] CDU Hw interrupt; [10] DMAE Parity
   error; [11] DMAE Hw interrupt; [12] IGU (HC) Parity error; [13] IGU (HC)
   Hw interrupt; [14] MISC Parity error; [15] MISC Hw interrupt; [16]
   pxp_misc_mps_attn; [17] Flash event; [18] SMB event; [19] MCP attn0; [20]
   MCP attn1; [21] SW timers attn_1 func0; [22] SW timers attn_2 func0; [23]
   SW timers attn_3 func0; [24] SW timers attn_4 func0; [25] PERST; [26] SW
   timers attn_1 func1; [27] SW timers attn_2 func1; [28] SW timers attn_3
   func1; [29] SW timers attn_4 func1; [30] General attn0; [31] General
   attn1; */
#define MISC_REG_AEU_ENABLE3_FUNC_1_OUT_0			 0xa114
#define MISC_REG_AEU_ENABLE3_FUNC_1_OUT_1			 0xa124
/* [RW 32] third 32b for enabling the output for close the gate nig. mapped
   as follows: [0] CSEMI Parity error; [1] CSEMI Hw interrupt; [2] PXP
   Parity error; [3] PXP Hw interrupt; [4] PXPpciClockClient Parity error;
   [5] PXPpciClockClient Hw interrupt; [6] CFC Parity error; [7] CFC Hw
   interrupt; [8] CDU Parity error; [9] CDU Hw interrupt; [10] DMAE Parity
   error; [11] DMAE Hw interrupt; [12] IGU (HC) Parity error; [13] IGU (HC)
   Hw interrupt; [14] MISC Parity error; [15] MISC Hw interrupt; [16]
   pxp_misc_mps_attn; [17] Flash event; [18] SMB event; [19] MCP attn0; [20]
   MCP attn1; [21] SW timers attn_1 func0; [22] SW timers attn_2 func0; [23]
   SW timers attn_3 func0; [24] SW timers attn_4 func0; [25] PERST; [26] SW
   timers attn_1 func1; [27] SW timers attn_2 func1; [28] SW timers attn_3
   func1; [29] SW timers attn_4 func1; [30] General attn0; [31] General
   attn1; */
#define MISC_REG_AEU_ENABLE3_NIG_0				 0xa0f4
#define MISC_REG_AEU_ENABLE3_NIG_1				 0xa194
/* [RW 32] third 32b for enabling the output for close the gate pxp. mapped
   as follows: [0] CSEMI Parity error; [1] CSEMI Hw interrupt; [2] PXP
   Parity error; [3] PXP Hw interrupt; [4] PXPpciClockClient Parity error;
   [5] PXPpciClockClient Hw interrupt; [6] CFC Parity error; [7] CFC Hw
   interrupt; [8] CDU Parity error; [9] CDU Hw interrupt; [10] DMAE Parity
   error; [11] DMAE Hw interrupt; [12] IGU (HC) Parity error; [13] IGU (HC)
   Hw interrupt; [14] MISC Parity error; [15] MISC Hw interrupt; [16]
   pxp_misc_mps_attn; [17] Flash event; [18] SMB event; [19] MCP attn0; [20]
   MCP attn1; [21] SW timers attn_1 func0; [22] SW timers attn_2 func0; [23]
   SW timers attn_3 func0; [24] SW timers attn_4 func0; [25] PERST; [26] SW
   timers attn_1 func1; [27] SW timers attn_2 func1; [28] SW timers attn_3
   func1; [29] SW timers attn_4 func1; [30] General attn0; [31] General
   attn1; */
#define MISC_REG_AEU_ENABLE3_PXP_0				 0xa104
#define MISC_REG_AEU_ENABLE3_PXP_1				 0xa1a4
/* [RW 32] fourth 32b for enabling the output for function 0 output0.mapped
   as follows: [0] General attn2; [1] General attn3; [2] General attn4; [3]
   General attn5; [4] General attn6; [5] General attn7; [6] General attn8;
   [7] General attn9; [8] General attn10; [9] General attn11; [10] General
   attn12; [11] General attn13; [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16] General attn18; [17]
   General attn19; [18] General attn20; [19] General attn21; [20] Main power
   interrupt; [21] RBCR Latched attn; [22] RBCT Latched attn; [23] RBCN
   Latched attn; [24] RBCU Latched attn; [25] RBCP Latched attn; [26] GRC
   Latched timeout attention; [27] GRC Latched reserved access attention;
   [28] MCP Latched rom_parity; [29] MCP Latched ump_rx_parity; [30] MCP
   Latched ump_tx_parity; [31] MCP Latched scpad_parity; */
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_0			 0xa078
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_2			 0xa098
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_4			 0xa0b8
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_5			 0xa0c8
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_6			 0xa0d8
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_7			 0xa0e8
/* [RW 32] fourth 32b for enabling the output for function 1 output0.mapped
   as follows: [0] General attn2; [1] General attn3; [2] General attn4; [3]
   General attn5; [4] General attn6; [5] General attn7; [6] General attn8;
   [7] General attn9; [8] General attn10; [9] General attn11; [10] General
   attn12; [11] General attn13; [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16] General attn18; [17]
   General attn19; [18] General attn20; [19] General attn21; [20] Main power
   interrupt; [21] RBCR Latched attn; [22] RBCT Latched attn; [23] RBCN
   Latched attn; [24] RBCU Latched attn; [25] RBCP Latched attn; [26] GRC
   Latched timeout attention; [27] GRC Latched reserved access attention;
   [28] MCP Latched rom_parity; [29] MCP Latched ump_rx_parity; [30] MCP
   Latched ump_tx_parity; [31] MCP Latched scpad_parity; */
#define MISC_REG_AEU_ENABLE4_FUNC_1_OUT_0			 0xa118
#define MISC_REG_AEU_ENABLE4_FUNC_1_OUT_2			 0xa138
#define MISC_REG_AEU_ENABLE4_FUNC_1_OUT_4			 0xa158
#define MISC_REG_AEU_ENABLE4_FUNC_1_OUT_5			 0xa168
#define MISC_REG_AEU_ENABLE4_FUNC_1_OUT_6			 0xa178
#define MISC_REG_AEU_ENABLE4_FUNC_1_OUT_7			 0xa188
/* [RW 32] fourth 32b for enabling the output for close the gate nig.mapped
   as follows: [0] General attn2; [1] General attn3; [2] General attn4; [3]
   General attn5; [4] General attn6; [5] General attn7; [6] General attn8;
   [7] General attn9; [8] General attn10; [9] General attn11; [10] General
   attn12; [11] General attn13; [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16] General attn18; [17]
   General attn19; [18] General attn20; [19] General attn21; [20] Main power
   interrupt; [21] RBCR Latched attn; [22] RBCT Latched attn; [23] RBCN
   Latched attn; [24] RBCU Latched attn; [25] RBCP Latched attn; [26] GRC
   Latched timeout attention; [27] GRC Latched reserved access attention;
   [28] MCP Latched rom_parity; [29] MCP Latched ump_rx_parity; [30] MCP
   Latched ump_tx_parity; [31] MCP Latched scpad_parity; */
#define MISC_REG_AEU_ENABLE4_NIG_0				 0xa0f8
#define MISC_REG_AEU_ENABLE4_NIG_1				 0xa198
/* [RW 32] fourth 32b for enabling the output for close the gate pxp.mapped
   as follows: [0] General attn2; [1] General attn3; [2] General attn4; [3]
   General attn5; [4] General attn6; [5] General attn7; [6] General attn8;
   [7] General attn9; [8] General attn10; [9] General attn11; [10] General
   attn12; [11] General attn13; [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16] General attn18; [17]
   General attn19; [18] General attn20; [19] General attn21; [20] Main power
   interrupt; [21] RBCR Latched attn; [22] RBCT Latched attn; [23] RBCN
   Latched attn; [24] RBCU Latched attn; [25] RBCP Latched attn; [26] GRC
   Latched timeout attention; [27] GRC Latched reserved access attention;
   [28] MCP Latched rom_parity; [29] MCP Latched ump_rx_parity; [30] MCP
   Latched ump_tx_parity; [31] MCP Latched scpad_parity; */
#define MISC_REG_AEU_ENABLE4_PXP_0				 0xa108
#define MISC_REG_AEU_ENABLE4_PXP_1				 0xa1a8
/* [RW 1] set/clr general attention 0; this will set/clr bit 94 in the aeu
   128 bit vector */
#define MISC_REG_AEU_GENERAL_ATTN_0				 0xa000
#define MISC_REG_AEU_GENERAL_ATTN_1				 0xa004
#define MISC_REG_AEU_GENERAL_ATTN_10				 0xa028
#define MISC_REG_AEU_GENERAL_ATTN_11				 0xa02c
#define MISC_REG_AEU_GENERAL_ATTN_12				 0xa030
#define MISC_REG_AEU_GENERAL_ATTN_2				 0xa008
#define MISC_REG_AEU_GENERAL_ATTN_3				 0xa00c
#define MISC_REG_AEU_GENERAL_ATTN_4				 0xa010
#define MISC_REG_AEU_GENERAL_ATTN_5				 0xa014
#define MISC_REG_AEU_GENERAL_ATTN_6				 0xa018
#define MISC_REG_AEU_GENERAL_ATTN_7				 0xa01c
#define MISC_REG_AEU_GENERAL_ATTN_8				 0xa020
#define MISC_REG_AEU_GENERAL_ATTN_9				 0xa024
#define MISC_REG_AEU_GENERAL_MASK				 0xa61c
/* [RW 32] first 32b for inverting the input for function 0; for each bit:
   0= do not invert; 1= invert; mapped as follows: [0] NIG attention for
   function0; [1] NIG attention for function1; [2] GPIO1 mcp; [3] GPIO2 mcp;
   [4] GPIO3 mcp; [5] GPIO4 mcp; [6] GPIO1 function 1; [7] GPIO2 function 1;
   [8] GPIO3 function 1; [9] GPIO4 function 1; [10] PCIE glue/PXP VPD event
   function0; [11] PCIE glue/PXP VPD event function1; [12] PCIE glue/PXP
   Expansion ROM event0; [13] PCIE glue/PXP Expansion ROM event1; [14]
   SPIO4; [15] SPIO5; [16] MSI/X indication for mcp; [17] MSI/X indication
   for function 1; [18] BRB Parity error; [19] BRB Hw interrupt; [20] PRS
   Parity error; [21] PRS Hw interrupt; [22] SRC Parity error; [23] SRC Hw
   interrupt; [24] TSDM Parity error; [25] TSDM Hw interrupt; [26] TCM
   Parity error; [27] TCM Hw interrupt; [28] TSEMI Parity error; [29] TSEMI
   Hw interrupt; [30] PBF Parity error; [31] PBF Hw interrupt; */
#define MISC_REG_AEU_INVERTER_1_FUNC_0				 0xa22c
#define MISC_REG_AEU_INVERTER_1_FUNC_1				 0xa23c
/* [RW 32] second 32b for inverting the input for function 0; for each bit:
   0= do not invert; 1= invert. mapped as follows: [0] PBClient Parity
   error; [1] PBClient Hw interrupt; [2] QM Parity error; [3] QM Hw
   interrupt; [4] Timers Parity error; [5] Timers Hw interrupt; [6] XSDM
   Parity error; [7] XSDM Hw interrupt; [8] XCM Parity error; [9] XCM Hw
   interrupt; [10] XSEMI Parity error; [11] XSEMI Hw interrupt; [12]
   DoorbellQ Parity error; [13] DoorbellQ Hw interrupt; [14] NIG Parity
   error; [15] NIG Hw interrupt; [16] Vaux PCI core Parity error; [17] Vaux
   PCI core Hw interrupt; [18] Debug Parity error; [19] Debug Hw interrupt;
   [20] USDM Parity error; [21] USDM Hw interrupt; [22] UCM Parity error;
   [23] UCM Hw interrupt; [24] USEMI Parity error; [25] USEMI Hw interrupt;
   [26] UPB Parity error; [27] UPB Hw interrupt; [28] CSDM Parity error;
   [29] CSDM Hw interrupt; [30] CCM Parity error; [31] CCM Hw interrupt; */
#define MISC_REG_AEU_INVERTER_2_FUNC_0				 0xa230
#define MISC_REG_AEU_INVERTER_2_FUNC_1				 0xa240
/* [RW 10] [7:0] = mask 8 attention output signals toward IGU function0;
   [9:8] = raserved. Zero = mask; one = unmask */
#define MISC_REG_AEU_MASK_ATTN_FUNC_0				 0xa060
#define MISC_REG_AEU_MASK_ATTN_FUNC_1				 0xa064
/* [RW 1] If set a system kill occurred */
#define MISC_REG_AEU_SYS_KILL_OCCURRED				 0xa610
/* [RW 32] Represent the status of the input vector to the AEU when a system
   kill occurred. The register is reset in por reset. Mapped as follows: [0]
   NIG attention for function0; [1] NIG attention for function1; [2] GPIO1
   mcp; [3] GPIO2 mcp; [4] GPIO3 mcp; [5] GPIO4 mcp; [6] GPIO1 function 1;
   [7] GPIO2 function 1; [8] GPIO3 function 1; [9] GPIO4 function 1; [10]
   PCIE glue/PXP VPD event function0; [11] PCIE glue/PXP VPD event
   function1; [12] PCIE glue/PXP Expansion ROM event0; [13] PCIE glue/PXP
   Expansion ROM event1; [14] SPIO4; [15] SPIO5; [16] MSI/X indication for
   mcp; [17] MSI/X indication for function 1; [18] BRB Parity error; [19]
   BRB Hw interrupt; [20] PRS Parity error; [21] PRS Hw interrupt; [22] SRC
   Parity error; [23] SRC Hw interrupt; [24] TSDM Parity error; [25] TSDM Hw
   interrupt; [26] TCM Parity error; [27] TCM Hw interrupt; [28] TSEMI
   Parity error; [29] TSEMI Hw interrupt; [30] PBF Parity error; [31] PBF Hw
   interrupt; */
#define MISC_REG_AEU_SYS_KILL_STATUS_0				 0xa600
#define MISC_REG_AEU_SYS_KILL_STATUS_1				 0xa604
#define MISC_REG_AEU_SYS_KILL_STATUS_2				 0xa608
#define MISC_REG_AEU_SYS_KILL_STATUS_3				 0xa60c
/* [R 4] This field indicates the type of the device. '0' - 2 Ports; '1' - 1
   Port. */
#define MISC_REG_BOND_ID					 0xa400
/* [R 8] These bits indicate the metal revision of the chip. This value
   starts at 0x00 for each all-layer tape-out and increments by one for each
   tape-out. */
#define MISC_REG_CHIP_METAL					 0xa404
/* [R 16] These bits indicate the part number for the chip. */
#define MISC_REG_CHIP_NUM					 0xa408
/* [R 4] These bits indicate the base revision of the chip. This value
   starts at 0x0 for the A0 tape-out and increments by one for each
   all-layer tape-out. */
#define MISC_REG_CHIP_REV					 0xa40c
/* [RW 32] The following driver registers(1...16) represent 16 drivers and
   32 clients. Each client can be controlled by one driver only. One in each
   bit represent that this driver control the appropriate client (Ex: bit 5
   is set means this driver control client number 5). addr1 = set; addr0 =
   clear; read from both addresses will give the same result = status. write
   to address 1 will set a request to control all the clients that their
   appropriate bit (in the write command) is set. if the client is free (the
   appropriate bit in all the other drivers is clear) one will be written to
   that driver register; if the client isn't free the bit will remain zero.
   if the appropriate bit is set (the driver request to gain control on a
   client it already controls the ~MISC_REGISTERS_INT_STS.GENERIC_SW
   interrupt will be asserted). write to address 0 will set a request to
   free all the clients that their appropriate bit (in the write command) is
   set. if the appropriate bit is clear (the driver request to free a client
   it doesn't controls the ~MISC_REGISTERS_INT_STS.GENERIC_SW interrupt will
   be asserted). */
#define MISC_REG_DRIVER_CONTROL_1				 0xa510
#define MISC_REG_DRIVER_CONTROL_7				 0xa3c8
/* [RW 1] e1hmf for WOL. If clr WOL signal o the PXP will be send on bit 0
   only. */
#define MISC_REG_E1HMF_MODE					 0xa5f8
/* [RW 32] Debug only: spare RW register reset by core reset */
#define MISC_REG_GENERIC_CR_0					 0xa460
/* [RW 32] GPIO. [31-28] FLOAT port 0; [27-24] FLOAT port 0; When any of
   these bits is written as a '1'; the corresponding SPIO bit will turn off
   it's drivers and become an input. This is the reset state of all GPIO
   pins. The read value of these bits will be a '1' if that last command
   (#SET; #CLR; or #FLOAT) for this bit was a #FLOAT. (reset value 0xff).
   [23-20] CLR port 1; 19-16] CLR port 0; When any of these bits is written
   as a '1'; the corresponding GPIO bit will drive low. The read value of
   these bits will be a '1' if that last command (#SET; #CLR; or #FLOAT) for
   this bit was a #CLR. (reset value 0). [15-12] SET port 1; 11-8] port 0;
   SET When any of these bits is written as a '1'; the corresponding GPIO
   bit will drive high (if it has that capability). The read value of these
   bits will be a '1' if that last command (#SET; #CLR; or #FLOAT) for this
   bit was a #SET. (reset value 0). [7-4] VALUE port 1; [3-0] VALUE port 0;
   RO; These bits indicate the read value of each of the eight GPIO pins.
   This is the result value of the pin; not the drive value. Writing these
   bits will have not effect. */
#define MISC_REG_GPIO						 0xa490
/* [RW 8] These bits enable the GPIO_INTs to signals event to the
   IGU/MCP.according to the following map: [0] p0_gpio_0; [1] p0_gpio_1; [2]
   p0_gpio_2; [3] p0_gpio_3; [4] p1_gpio_0; [5] p1_gpio_1; [6] p1_gpio_2;
   [7] p1_gpio_3; */
#define MISC_REG_GPIO_EVENT_EN					 0xa2bc
/* [RW 32] GPIO INT. [31-28] OLD_CLR port1; [27-24] OLD_CLR port0; Writing a
   '1' to these bit clears the corresponding bit in the #OLD_VALUE register.
   This will acknowledge an interrupt on the falling edge of corresponding
   GPIO input (reset value 0). [23-16] OLD_SET [23-16] port1; OLD_SET port0;
   Writing a '1' to these bit sets the corresponding bit in the #OLD_VALUE
   register. This will acknowledge an interrupt on the rising edge of
   corresponding SPIO input (reset value 0). [15-12] OLD_VALUE [11-8] port1;
   OLD_VALUE port0; RO; These bits indicate the old value of the GPIO input
   value. When the ~INT_STATE bit is set; this bit indicates the OLD value
   of the pin such that if ~INT_STATE is set and this bit is '0'; then the
   interrupt is due to a low to high edge. If ~INT_STATE is set and this bit
   is '1'; then the interrupt is due to a high to low edge (reset value 0).
   [7-4] INT_STATE port1; [3-0] INT_STATE RO port0; These bits indicate the
   current GPIO interrupt state for each GPIO pin. This bit is cleared when
   the appropriate #OLD_SET or #OLD_CLR command bit is written. This bit is
   set when the GPIO input does not match the current value in #OLD_VALUE
   (reset value 0). */
#define MISC_REG_GPIO_INT					 0xa494
/* [R 28] this field hold the last information that caused reserved
   attention. bits [19:0] - address; [22:20] function; [23] reserved;
   [27:24] the master that caused the attention - according to the following
   encodeing:1 = pxp; 2 = mcp; 3 = usdm; 4 = tsdm; 5 = xsdm; 6 = csdm; 7 =
   dbu; 8 = dmae */
#define MISC_REG_GRC_RSV_ATTN					 0xa3c0
/* [R 28] this field hold the last information that caused timeout
   attention. bits [19:0] - address; [22:20] function; [23] reserved;
   [27:24] the master that caused the attention - according to the following
   encodeing:1 = pxp; 2 = mcp; 3 = usdm; 4 = tsdm; 5 = xsdm; 6 = csdm; 7 =
   dbu; 8 = dmae */
#define MISC_REG_GRC_TIMEOUT_ATTN				 0xa3c4
/* [RW 1] Setting this bit enables a timer in the GRC block to timeout any
   access that does not finish within
   ~misc_registers_grc_timout_val.grc_timeout_val cycles. When this bit is
   cleared; this timeout is disabled. If this timeout occurs; the GRC shall
   assert it attention output. */
#define MISC_REG_GRC_TIMEOUT_EN 				 0xa280
/* [RW 28] 28 LSB of LCPLL first register; reset val = 521. inside order of
   the bits is: [2:0] OAC reset value 001) CML output buffer bias control;
   111 for +40%; 011 for +20%; 001 for 0%; 000 for -20%. [5:3] Icp_ctrl
   (reset value 001) Charge pump current control; 111 for 720u; 011 for
   600u; 001 for 480u and 000 for 360u. [7:6] Bias_ctrl (reset value 00)
   Global bias control; When bit 7 is high bias current will be 10 0gh; When
   bit 6 is high bias will be 100w; Valid values are 00; 10; 01. [10:8]
   Pll_observe (reset value 010) Bits to control observability. bit 10 is
   for test bias; bit 9 is for test CK; bit 8 is test Vc. [12:11] Vth_ctrl
   (reset value 00) Comparator threshold control. 00 for 0.6V; 01 for 0.54V
   and 10 for 0.66V. [13] pllSeqStart (reset value 0) Enables VCO tuning
   sequencer: 1= sequencer disabled; 0= sequencer enabled (inverted
   internally). [14] reserved (reset value 0) Reset for VCO sequencer is
   connected to RESET input directly. [15] capRetry_en (reset value 0)
   enable retry on cap search failure (inverted). [16] freqMonitor_e (reset
   value 0) bit to continuously monitor vco freq (inverted). [17]
   freqDetRestart_en (reset value 0) bit to enable restart when not freq
   locked (inverted). [18] freqDetRetry_en (reset value 0) bit to enable
   retry on freq det failure(inverted). [19] pllForceFdone_en (reset value
   0) bit to enable pllForceFdone & pllForceFpass into pllSeq. [20]
   pllForceFdone (reset value 0) bit to force freqDone. [21] pllForceFpass
   (reset value 0) bit to force freqPass. [22] pllForceDone_en (reset value
   0) bit to enable pllForceCapDone. [23] pllForceCapDone (reset value 0)
   bit to force capDone. [24] pllForceCapPass_en (reset value 0) bit to
   enable pllForceCapPass. [25] pllForceCapPass (reset value 0) bit to force
   capPass. [26] capRestart (reset value 0) bit to force cap sequencer to
   restart. [27] capSelectM_en (reset value 0) bit to enable cap select
   register bits. */
#define MISC_REG_LCPLL_CTRL_1					 0xa2a4
#define MISC_REG_LCPLL_CTRL_REG_2				 0xa2a8
/* [RW 4] Interrupt mask register #0 read/write */
#define MISC_REG_MISC_INT_MASK					 0xa388
/* [RW 1] Parity mask register #0 read/write */
#define MISC_REG_MISC_PRTY_MASK 				 0xa398
/* [R 1] Parity register #0 read */
#define MISC_REG_MISC_PRTY_STS					 0xa38c
#define MISC_REG_NIG_WOL_P0					 0xa270
#define MISC_REG_NIG_WOL_P1					 0xa274
/* [R 1] If set indicate that the pcie_rst_b was asserted without perst
   assertion */
#define MISC_REG_PCIE_HOT_RESET 				 0xa618
/* [RW 32] 32 LSB of storm PLL first register; reset val = 0x 071d2911.
   inside order of the bits is: [0] P1 divider[0] (reset value 1); [1] P1
   divider[1] (reset value 0); [2] P1 divider[2] (reset value 0); [3] P1
   divider[3] (reset value 0); [4] P2 divider[0] (reset value 1); [5] P2
   divider[1] (reset value 0); [6] P2 divider[2] (reset value 0); [7] P2
   divider[3] (reset value 0); [8] ph_det_dis (reset value 1); [9]
   freq_det_dis (reset value 0); [10] Icpx[0] (reset value 0); [11] Icpx[1]
   (reset value 1); [12] Icpx[2] (reset value 0); [13] Icpx[3] (reset value
   1); [14] Icpx[4] (reset value 0); [15] Icpx[5] (reset value 0); [16]
   Rx[0] (reset value 1); [17] Rx[1] (reset value 0); [18] vc_en (reset
   value 1); [19] vco_rng[0] (reset value 1); [20] vco_rng[1] (reset value
   1); [21] Kvco_xf[0] (reset value 0); [22] Kvco_xf[1] (reset value 0);
   [23] Kvco_xf[2] (reset value 0); [24] Kvco_xs[0] (reset value 1); [25]
   Kvco_xs[1] (reset value 1); [26] Kvco_xs[2] (reset value 1); [27]
   testd_en (reset value 0); [28] testd_sel[0] (reset value 0); [29]
   testd_sel[1] (reset value 0); [30] testd_sel[2] (reset value 0); [31]
   testa_en (reset value 0); */
#define MISC_REG_PLL_STORM_CTRL_1				 0xa294
#define MISC_REG_PLL_STORM_CTRL_2				 0xa298
#define MISC_REG_PLL_STORM_CTRL_3				 0xa29c
#define MISC_REG_PLL_STORM_CTRL_4				 0xa2a0
/* [RW 32] reset reg#2; rite/read one = the specific block is out of reset;
   write/read zero = the specific block is in reset; addr 0-wr- the write
   value will be written to the register; addr 1-set - one will be written
   to all the bits that have the value of one in the data written (bits that
   have the value of zero will not be change) ; addr 2-clear - zero will be
   written to all the bits that have the value of one in the data written
   (bits that have the value of zero will not be change); addr 3-ignore;
   read ignore from all addr except addr 00; inside order of the bits is:
   [0] rst_bmac0; [1] rst_bmac1; [2] rst_emac0; [3] rst_emac1; [4] rst_grc;
   [5] rst_mcp_n_reset_reg_hard_core; [6] rst_ mcp_n_hard_core_rst_b; [7]
   rst_ mcp_n_reset_cmn_cpu; [8] rst_ mcp_n_reset_cmn_core; [9] rst_rbcn;
   [10] rst_dbg; [11] rst_misc_core; [12] rst_dbue (UART); [13]
   Pci_resetmdio_n; [14] rst_emac0_hard_core; [15] rst_emac1_hard_core; 16]
   rst_pxp_rq_rd_wr; 31:17] reserved */
#define MISC_REG_RESET_REG_2					 0xa590
/* [RW 20] 20 bit GRC address where the scratch-pad of the MCP that is
   shared with the driver resides */
#define MISC_REG_SHARED_MEM_ADDR				 0xa2b4
/* [RW 32] SPIO. [31-24] FLOAT When any of these bits is written as a '1';
   the corresponding SPIO bit will turn off it's drivers and become an
   input. This is the reset state of all SPIO pins. The read value of these
   bits will be a '1' if that last command (#SET; #CL; or #FLOAT) for this
   bit was a #FLOAT. (reset value 0xff). [23-16] CLR When any of these bits
   is written as a '1'; the corresponding SPIO bit will drive low. The read
   value of these bits will be a '1' if that last command (#SET; #CLR; or
#FLOAT) for this bit was a #CLR. (reset value 0). [15-8] SET When any of
   these bits is written as a '1'; the corresponding SPIO bit will drive
   high (if it has that capability). The read value of these bits will be a
   '1' if that last command (#SET; #CLR; or #FLOAT) for this bit was a #SET.
   (reset value 0). [7-0] VALUE RO; These bits indicate the read value of
   each of the eight SPIO pins. This is the result value of the pin; not the
   drive value. Writing these bits will have not effect. Each 8 bits field
   is divided as follows: [0] VAUX Enable; when pulsed low; enables supply
   from VAUX. (This is an output pin only; the FLOAT field is not applicable
   for this pin); [1] VAUX Disable; when pulsed low; disables supply form
   VAUX. (This is an output pin only; FLOAT field is not applicable for this
   pin); [2] SEL_VAUX_B - Control to power switching logic. Drive low to
   select VAUX supply. (This is an output pin only; it is not controlled by
   the SET and CLR fields; it is controlled by the Main Power SM; the FLOAT
   field is not applicable for this pin; only the VALUE fields is relevant -
   it reflects the output value); [3] port swap [4] spio_4; [5] spio_5; [6]
   Bit 0 of UMP device ID select; read by UMP firmware; [7] Bit 1 of UMP
   device ID select; read by UMP firmware. */
#define MISC_REG_SPIO						 0xa4fc
/* [RW 8] These bits enable the SPIO_INTs to signals event to the IGU/MC.
   according to the following map: [3:0] reserved; [4] spio_4 [5] spio_5;
   [7:0] reserved */
#define MISC_REG_SPIO_EVENT_EN					 0xa2b8
/* [RW 32] SPIO INT. [31-24] OLD_CLR Writing a '1' to these bit clears the
   corresponding bit in the #OLD_VALUE register. This will acknowledge an
   interrupt on the falling edge of corresponding SPIO input (reset value
   0). [23-16] OLD_SET Writing a '1' to these bit sets the corresponding bit
   in the #OLD_VALUE register. This will acknowledge an interrupt on the
   rising edge of corresponding SPIO input (reset value 0). [15-8] OLD_VALUE
   RO; These bits indicate the old value of the SPIO input value. When the
   ~INT_STATE bit is set; this bit indicates the OLD value of the pin such
   that if ~INT_STATE is set and this bit is '0'; then the interrupt is due
   to a low to high edge. If ~INT_STATE is set and this bit is '1'; then the
   interrupt is due to a high to low edge (reset value 0). [7-0] INT_STATE
   RO; These bits indicate the current SPIO interrupt state for each SPIO
   pin. This bit is cleared when the appropriate #OLD_SET or #OLD_CLR
   command bit is written. This bit is set when the SPIO input does not
   match the current value in #OLD_VALUE (reset value 0). */
#define MISC_REG_SPIO_INT					 0xa500
/* [RW 32] reload value for counter 4 if reload; the value will be reload if
   the counter reached zero and the reload bit
   (~misc_registers_sw_timer_cfg_4.sw_timer_cfg_4[1] ) is set */
#define MISC_REG_SW_TIMER_RELOAD_VAL_4				 0xa2fc
/* [RW 32] the value of the counter for sw timers1-8. there are 8 addresses
   in this register. addres 0 - timer 1; address - timer 2�address 7 -
   timer 8 */
#define MISC_REG_SW_TIMER_VAL					 0xa5c0
/* [RW 1] Set by the MCP to remember if one or more of the drivers is/are
   loaded; 0-prepare; -unprepare */
#define MISC_REG_UNPREPARED					 0xa424
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_BRCST	 (0x1<<0)
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_MLCST	 (0x1<<1)
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_NO_VLAN	 (0x1<<4)
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_UNCST	 (0x1<<2)
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLH0_BRB1_DRV_MASK_VLAN	 (0x1<<3)
#define NIG_MASK_INTERRUPT_PORT0_REG_MASK_EMAC0_MISC_MI_INT	 (0x1<<0)
#define NIG_MASK_INTERRUPT_PORT0_REG_MASK_SERDES0_LINK_STATUS	 (0x1<<9)
#define NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK10G 	 (0x1<<15)
#define NIG_MASK_INTERRUPT_PORT0_REG_MASK_XGXS0_LINK_STATUS	 (0xf<<18)
/* [RW 1] Input enable for RX_BMAC0 IF */
#define NIG_REG_BMAC0_IN_EN					 0x100ac
/* [RW 1] output enable for TX_BMAC0 IF */
#define NIG_REG_BMAC0_OUT_EN					 0x100e0
/* [RW 1] output enable for TX BMAC pause port 0 IF */
#define NIG_REG_BMAC0_PAUSE_OUT_EN				 0x10110
/* [RW 1] output enable for RX_BMAC0_REGS IF */
#define NIG_REG_BMAC0_REGS_OUT_EN				 0x100e8
/* [RW 1] output enable for RX BRB1 port0 IF */
#define NIG_REG_BRB0_OUT_EN					 0x100f8
/* [RW 1] Input enable for TX BRB1 pause port 0 IF */
#define NIG_REG_BRB0_PAUSE_IN_EN				 0x100c4
/* [RW 1] output enable for RX BRB1 port1 IF */
#define NIG_REG_BRB1_OUT_EN					 0x100fc
/* [RW 1] Input enable for TX BRB1 pause port 1 IF */
#define NIG_REG_BRB1_PAUSE_IN_EN				 0x100c8
/* [RW 1] output enable for RX BRB1 LP IF */
#define NIG_REG_BRB_LB_OUT_EN					 0x10100
/* [WB_W 82] Debug packet to LP from RBC; Data spelling:[63:0] data; 64]
   error; [67:65]eop_bvalid; [68]eop; [69]sop; [70]port_id; 71]flush;
   72:73]-vnic_num; 81:74]-sideband_info */
#define NIG_REG_DEBUG_PACKET_LB 				 0x10800
/* [RW 1] Input enable for TX Debug packet */
#define NIG_REG_EGRESS_DEBUG_IN_EN				 0x100dc
/* [RW 1] If 1 - egress drain mode for port0 is active. In this mode all
   packets from PBFare not forwarded to the MAC and just deleted from FIFO.
   First packet may be deleted from the middle. And last packet will be
   always deleted till the end. */
#define NIG_REG_EGRESS_DRAIN0_MODE				 0x10060
/* [RW 1] Output enable to EMAC0 */
#define NIG_REG_EGRESS_EMAC0_OUT_EN				 0x10120
/* [RW 1] MAC configuration for packets of port0. If 1 - all packet outputs
   to emac for port0; other way to bmac for port0 */
#define NIG_REG_EGRESS_EMAC0_PORT				 0x10058
/* [RW 1] Input enable for TX PBF user packet port0 IF */
#define NIG_REG_EGRESS_PBF0_IN_EN				 0x100cc
/* [RW 1] Input enable for TX PBF user packet port1 IF */
#define NIG_REG_EGRESS_PBF1_IN_EN				 0x100d0
/* [RW 1] Input enable for TX UMP management packet port0 IF */
#define NIG_REG_EGRESS_UMP0_IN_EN				 0x100d4
/* [RW 1] Input enable for RX_EMAC0 IF */
#define NIG_REG_EMAC0_IN_EN					 0x100a4
/* [RW 1] output enable for TX EMAC pause port 0 IF */
#define NIG_REG_EMAC0_PAUSE_OUT_EN				 0x10118
/* [R 1] status from emac0. This bit is set when MDINT from either the
   EXT_MDINT pin or from the Copper PHY is driven low. This condition must
   be cleared in the attached PHY device that is driving the MINT pin. */
#define NIG_REG_EMAC0_STATUS_MISC_MI_INT			 0x10494
/* [WB 48] This address space contains BMAC0 registers. The BMAC registers
   are described in appendix A. In order to access the BMAC0 registers; the
   base address; NIG_REGISTERS_INGRESS_BMAC0_MEM; Offset: 0x10c00; should be
   added to each BMAC register offset */
#define NIG_REG_INGRESS_BMAC0_MEM				 0x10c00
/* [WB 48] This address space contains BMAC1 registers. The BMAC registers
   are described in appendix A. In order to access the BMAC0 registers; the
   base address; NIG_REGISTERS_INGRESS_BMAC1_MEM; Offset: 0x11000; should be
   added to each BMAC register offset */
#define NIG_REG_INGRESS_BMAC1_MEM				 0x11000
/* [R 1] FIFO empty in EOP descriptor FIFO of LP in NIG_RX_EOP */
#define NIG_REG_INGRESS_EOP_LB_EMPTY				 0x104e0
/* [RW 17] Debug only. RX_EOP_DSCR_lb_FIFO in NIG_RX_EOP. Data
   packet_length[13:0]; mac_error[14]; trunc_error[15]; parity[16] */
#define NIG_REG_INGRESS_EOP_LB_FIFO				 0x104e4
/* [RW 27] 0 - must be active for Everest A0; 1- for Everest B0 when latch
   logic for interrupts must be used. Enable per bit of interrupt of
   ~latch_status.latch_status */
#define NIG_REG_LATCH_BC_0					 0x16210
/* [RW 27] Latch for each interrupt from Unicore.b[0]
   status_emac0_misc_mi_int; b[1] status_emac0_misc_mi_complete;
   b[2]status_emac0_misc_cfg_change; b[3]status_emac0_misc_link_status;
   b[4]status_emac0_misc_link_change; b[5]status_emac0_misc_attn;
   b[6]status_serdes0_mac_crs; b[7]status_serdes0_autoneg_complete;
   b[8]status_serdes0_fiber_rxact; b[9]status_serdes0_link_status;
   b[10]status_serdes0_mr_page_rx; b[11]status_serdes0_cl73_an_complete;
   b[12]status_serdes0_cl73_mr_page_rx; b[13]status_serdes0_rx_sigdet;
   b[14]status_xgxs0_remotemdioreq; b[15]status_xgxs0_link10g;
   b[16]status_xgxs0_autoneg_complete; b[17]status_xgxs0_fiber_rxact;
   b[21:18]status_xgxs0_link_status; b[22]status_xgxs0_mr_page_rx;
   b[23]status_xgxs0_cl73_an_complete; b[24]status_xgxs0_cl73_mr_page_rx;
   b[25]status_xgxs0_rx_sigdet; b[26]status_xgxs0_mac_crs */
#define NIG_REG_LATCH_STATUS_0					 0x18000
/* [RW 1] led 10g for port 0 */
#define NIG_REG_LED_10G_P0					 0x10320
/* [RW 1] led 10g for port 1 */
#define NIG_REG_LED_10G_P1					 0x10324
/* [RW 1] Port0: This bit is set to enable the use of the
   ~nig_registers_led_control_blink_rate_p0.led_control_blink_rate_p0 field
   defined below. If this bit is cleared; then the blink rate will be about
   8Hz. */
#define NIG_REG_LED_CONTROL_BLINK_RATE_ENA_P0			 0x10318
/* [RW 12] Port0: Specifies the period of each blink cycle (on + off) for
   Traffic LED in milliseconds. Must be a non-zero value. This 12-bit field
   is reset to 0x080; giving a default blink period of approximately 8Hz. */
#define NIG_REG_LED_CONTROL_BLINK_RATE_P0			 0x10310
/* [RW 1] Port0: If set along with the
 ~nig_registers_led_control_override_traffic_p0.led_control_override_traffic_p0
   bit and ~nig_registers_led_control_traffic_p0.led_control_traffic_p0 LED
   bit; the Traffic LED will blink with the blink rate specified in
   ~nig_registers_led_control_blink_rate_p0.led_control_blink_rate_p0 and
   ~nig_registers_led_control_blink_rate_ena_p0.led_control_blink_rate_ena_p0
   fields. */
#define NIG_REG_LED_CONTROL_BLINK_TRAFFIC_P0			 0x10308
/* [RW 1] Port0: If set overrides hardware control of the Traffic LED. The
   Traffic LED will then be controlled via bit ~nig_registers_
   led_control_traffic_p0.led_control_traffic_p0 and bit
   ~nig_registers_led_control_blink_traffic_p0.led_control_blink_traffic_p0 */
#define NIG_REG_LED_CONTROL_OVERRIDE_TRAFFIC_P0 		 0x102f8
/* [RW 1] Port0: If set along with the led_control_override_trafic_p0 bit;
   turns on the Traffic LED. If the led_control_blink_traffic_p0 bit is also
   set; the LED will blink with blink rate specified in
   ~nig_registers_led_control_blink_rate_p0.led_control_blink_rate_p0 and
   ~nig_regsters_led_control_blink_rate_ena_p0.led_control_blink_rate_ena_p0
   fields. */
#define NIG_REG_LED_CONTROL_TRAFFIC_P0				 0x10300
/* [RW 4] led mode for port0: 0 MAC; 1-3 PHY1; 4 MAC2; 5-7 PHY4; 8-MAC3;
   9-11PHY7; 12 MAC4; 13-15 PHY10; */
#define NIG_REG_LED_MODE_P0					 0x102f0
/* [RW 3] for port0 enable for llfc ppp and pause. b0 - brb1 enable; b1-
   tsdm enable; b2- usdm enable */
#define NIG_REG_LLFC_EGRESS_SRC_ENABLE_0			 0x16070
#define NIG_REG_LLFC_EGRESS_SRC_ENABLE_1			 0x16074
/* [RW 1] SAFC enable for port0. This register may get 1 only when
   ~ppp_enable.ppp_enable = 0 and pause_enable.pause_enable =0 for the same
   port */
#define NIG_REG_LLFC_ENABLE_0					 0x16208
/* [RW 16] classes are high-priority for port0 */
#define NIG_REG_LLFC_HIGH_PRIORITY_CLASSES_0			 0x16058
/* [RW 16] classes are low-priority for port0 */
#define NIG_REG_LLFC_LOW_PRIORITY_CLASSES_0			 0x16060
/* [RW 1] Output enable of message to LLFC BMAC IF for port0 */
#define NIG_REG_LLFC_OUT_EN_0					 0x160c8
#define NIG_REG_LLH0_ACPI_PAT_0_CRC				 0x1015c
#define NIG_REG_LLH0_ACPI_PAT_6_LEN				 0x10154
#define NIG_REG_LLH0_BRB1_DRV_MASK				 0x10244
#define NIG_REG_LLH0_BRB1_DRV_MASK_MF				 0x16048
/* [RW 1] send to BRB1 if no match on any of RMP rules. */
#define NIG_REG_LLH0_BRB1_NOT_MCP				 0x1025c
/* [RW 2] Determine the classification participants. 0: no classification.1:
   classification upon VLAN id. 2: classification upon MAC address. 3:
   classification upon both VLAN id & MAC addr. */
#define NIG_REG_LLH0_CLS_TYPE					 0x16080
/* [RW 32] cm header for llh0 */
#define NIG_REG_LLH0_CM_HEADER					 0x1007c
#define NIG_REG_LLH0_DEST_IP_0_1				 0x101dc
#define NIG_REG_LLH0_DEST_MAC_0_0				 0x101c0
/* [RW 16] destination TCP address 1. The LLH will look for this address in
   all incoming packets. */
#define NIG_REG_LLH0_DEST_TCP_0 				 0x10220
/* [RW 16] destination UDP address 1 The LLH will look for this address in
   all incoming packets. */
#define NIG_REG_LLH0_DEST_UDP_0 				 0x10214
#define NIG_REG_LLH0_ERROR_MASK 				 0x1008c
/* [RW 8] event id for llh0 */
#define NIG_REG_LLH0_EVENT_ID					 0x10084
#define NIG_REG_LLH0_FUNC_EN					 0x160fc
#define NIG_REG_LLH0_FUNC_VLAN_ID				 0x16100
/* [RW 1] Determine the IP version to look for in
   ~nig_registers_llh0_dest_ip_0.llh0_dest_ip_0. 0 - IPv6; 1-IPv4 */
#define NIG_REG_LLH0_IPV4_IPV6_0				 0x10208
/* [RW 1] t bit for llh0 */
#define NIG_REG_LLH0_T_BIT					 0x10074
/* [RW 12] VLAN ID 1. In case of VLAN packet the LLH will look for this ID. */
#define NIG_REG_LLH0_VLAN_ID_0					 0x1022c
/* [RW 8] init credit counter for port0 in LLH */
#define NIG_REG_LLH0_XCM_INIT_CREDIT				 0x10554
#define NIG_REG_LLH0_XCM_MASK					 0x10130
#define NIG_REG_LLH1_BRB1_DRV_MASK				 0x10248
/* [RW 1] send to BRB1 if no match on any of RMP rules. */
#define NIG_REG_LLH1_BRB1_NOT_MCP				 0x102dc
/* [RW 2] Determine the classification participants. 0: no classification.1:
   classification upon VLAN id. 2: classification upon MAC address. 3:
   classification upon both VLAN id & MAC addr. */
#define NIG_REG_LLH1_CLS_TYPE					 0x16084
/* [RW 32] cm header for llh1 */
#define NIG_REG_LLH1_CM_HEADER					 0x10080
#define NIG_REG_LLH1_ERROR_MASK 				 0x10090
/* [RW 8] event id for llh1 */
#define NIG_REG_LLH1_EVENT_ID					 0x10088
/* [RW 8] init credit counter for port1 in LLH */
#define NIG_REG_LLH1_XCM_INIT_CREDIT				 0x10564
#define NIG_REG_LLH1_XCM_MASK					 0x10134
/* [RW 1] When this bit is set; the LLH will expect all packets to be with
   e1hov */
#define NIG_REG_LLH_E1HOV_MODE					 0x160d8
/* [RW 1] When this bit is set; the LLH will classify the packet before
   sending it to the BRB or calculating WoL on it. */
#define NIG_REG_LLH_MF_MODE					 0x16024
#define NIG_REG_MASK_INTERRUPT_PORT0				 0x10330
#define NIG_REG_MASK_INTERRUPT_PORT1				 0x10334
/* [RW 1] Output signal from NIG to EMAC0. When set enables the EMAC0 block. */
#define NIG_REG_NIG_EMAC0_EN					 0x1003c
/* [RW 1] Output signal from NIG to EMAC1. When set enables the EMAC1 block. */
#define NIG_REG_NIG_EMAC1_EN					 0x10040
/* [RW 1] Output signal from NIG to TX_EMAC0. When set indicates to the
   EMAC0 to strip the CRC from the ingress packets. */
#define NIG_REG_NIG_INGRESS_EMAC0_NO_CRC			 0x10044
/* [R 32] Interrupt register #0 read */
#define NIG_REG_NIG_INT_STS_0					 0x103b0
#define NIG_REG_NIG_INT_STS_1					 0x103c0
/* [R 32] Parity register #0 read */
#define NIG_REG_NIG_PRTY_STS					 0x103d0
/* [RW 1] Pause enable for port0. This register may get 1 only when
   ~safc_enable.safc_enable = 0 and ppp_enable.ppp_enable =0 for the same
   port */
#define NIG_REG_PAUSE_ENABLE_0					 0x160c0
/* [RW 1] Input enable for RX PBF LP IF */
#define NIG_REG_PBF_LB_IN_EN					 0x100b4
/* [RW 1] Value of this register will be transmitted to port swap when
   ~nig_registers_strap_override.strap_override =1 */
#define NIG_REG_PORT_SWAP					 0x10394
/* [RW 1] output enable for RX parser descriptor IF */
#define NIG_REG_PRS_EOP_OUT_EN					 0x10104
/* [RW 1] Input enable for RX parser request IF */
#define NIG_REG_PRS_REQ_IN_EN					 0x100b8
/* [RW 5] control to serdes - CL45 DEVAD */
#define NIG_REG_SERDES0_CTRL_MD_DEVAD				 0x10370
/* [RW 1] control to serdes; 0 - clause 45; 1 - clause 22 */
#define NIG_REG_SERDES0_CTRL_MD_ST				 0x1036c
/* [RW 5] control to serdes - CL22 PHY_ADD and CL45 PRTAD */
#define NIG_REG_SERDES0_CTRL_PHY_ADDR				 0x10374
/* [R 1] status from serdes0 that inputs to interrupt logic of link status */
#define NIG_REG_SERDES0_STATUS_LINK_STATUS			 0x10578
/* [R 32] Rx statistics : In user packets discarded due to BRB backpressure
   for port0 */
#define NIG_REG_STAT0_BRB_DISCARD				 0x105f0
/* [R 32] Rx statistics : In user packets truncated due to BRB backpressure
   for port0 */
#define NIG_REG_STAT0_BRB_TRUNCATE				 0x105f8
/* [WB_R 36] Tx statistics : Number of packets from emac0 or bmac0 that
   between 1024 and 1522 bytes for port0 */
#define NIG_REG_STAT0_EGRESS_MAC_PKT0				 0x10750
/* [WB_R 36] Tx statistics : Number of packets from emac0 or bmac0 that
   between 1523 bytes and above for port0 */
#define NIG_REG_STAT0_EGRESS_MAC_PKT1				 0x10760
/* [R 32] Rx statistics : In user packets discarded due to BRB backpressure
   for port1 */
#define NIG_REG_STAT1_BRB_DISCARD				 0x10628
/* [WB_R 36] Tx statistics : Number of packets from emac1 or bmac1 that
   between 1024 and 1522 bytes for port1 */
#define NIG_REG_STAT1_EGRESS_MAC_PKT0				 0x107a0
/* [WB_R 36] Tx statistics : Number of packets from emac1 or bmac1 that
   between 1523 bytes and above for port1 */
#define NIG_REG_STAT1_EGRESS_MAC_PKT1				 0x107b0
/* [WB_R 64] Rx statistics : User octets received for LP */
#define NIG_REG_STAT2_BRB_OCTET 				 0x107e0
#define NIG_REG_STATUS_INTERRUPT_PORT0				 0x10328
#define NIG_REG_STATUS_INTERRUPT_PORT1				 0x1032c
/* [RW 1] port swap mux selection. If this register equal to 0 then port
   swap is equal to SPIO pin that inputs from ifmux_serdes_swap. If 1 then
   ort swap is equal to ~nig_registers_port_swap.port_swap */
#define NIG_REG_STRAP_OVERRIDE					 0x10398
/* [RW 1] output enable for RX_XCM0 IF */
#define NIG_REG_XCM0_OUT_EN					 0x100f0
/* [RW 1] output enable for RX_XCM1 IF */
#define NIG_REG_XCM1_OUT_EN					 0x100f4
/* [RW 1] control to xgxs - remote PHY in-band MDIO */
#define NIG_REG_XGXS0_CTRL_EXTREMOTEMDIOST			 0x10348
/* [RW 5] control to xgxs - CL45 DEVAD */
#define NIG_REG_XGXS0_CTRL_MD_DEVAD				 0x1033c
/* [RW 1] control to xgxs; 0 - clause 45; 1 - clause 22 */
#define NIG_REG_XGXS0_CTRL_MD_ST				 0x10338
/* [RW 5] control to xgxs - CL22 PHY_ADD and CL45 PRTAD */
#define NIG_REG_XGXS0_CTRL_PHY_ADDR				 0x10340
/* [R 1] status from xgxs0 that inputs to interrupt logic of link10g. */
#define NIG_REG_XGXS0_STATUS_LINK10G				 0x10680
/* [R 4] status from xgxs0 that inputs to interrupt logic of link status */
#define NIG_REG_XGXS0_STATUS_LINK_STATUS			 0x10684
/* [RW 2] selection for XGXS lane of port 0 in NIG_MUX block */
#define NIG_REG_XGXS_LANE_SEL_P0				 0x102e8
/* [RW 1] selection for port0 for NIG_MUX block : 0 = SerDes; 1 = XGXS */
#define NIG_REG_XGXS_SERDES0_MODE_SEL				 0x102e0
#define NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_EMAC0_MISC_MI_INT  (0x1<<0)
#define NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_SERDES0_LINK_STATUS (0x1<<9)
#define NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK10G	 (0x1<<15)
#define NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK_STATUS  (0xf<<18)
#define NIG_STATUS_INTERRUPT_PORT0_REG_STATUS_XGXS0_LINK_STATUS_SIZE 18
/* [RW 1] Disable processing further tasks from port 0 (after ending the
   current task in process). */
#define PBF_REG_DISABLE_NEW_TASK_PROC_P0			 0x14005c
/* [RW 1] Disable processing further tasks from port 1 (after ending the
   current task in process). */
#define PBF_REG_DISABLE_NEW_TASK_PROC_P1			 0x140060
/* [RW 1] Disable processing further tasks from port 4 (after ending the
   current task in process). */
#define PBF_REG_DISABLE_NEW_TASK_PROC_P4			 0x14006c
#define PBF_REG_IF_ENABLE_REG					 0x140044
/* [RW 1] Init bit. When set the initial credits are copied to the credit
   registers (except the port credits). Should be set and then reset after
   the configuration of the block has ended. */
#define PBF_REG_INIT						 0x140000
/* [RW 1] Init bit for port 0. When set the initial credit of port 0 is
   copied to the credit register. Should be set and then reset after the
   configuration of the port has ended. */
#define PBF_REG_INIT_P0 					 0x140004
/* [RW 1] Init bit for port 1. When set the initial credit of port 1 is
   copied to the credit register. Should be set and then reset after the
   configuration of the port has ended. */
#define PBF_REG_INIT_P1 					 0x140008
/* [RW 1] Init bit for port 4. When set the initial credit of port 4 is
   copied to the credit register. Should be set and then reset after the
   configuration of the port has ended. */
#define PBF_REG_INIT_P4 					 0x14000c
/* [RW 1] Enable for mac interface 0. */
#define PBF_REG_MAC_IF0_ENABLE					 0x140030
/* [RW 1] Enable for mac interface 1. */
#define PBF_REG_MAC_IF1_ENABLE					 0x140034
/* [RW 1] Enable for the loopback interface. */
#define PBF_REG_MAC_LB_ENABLE					 0x140040
/* [RW 10] Port 0 threshold used by arbiter in 16 byte lines used when pause
   not suppoterd. */
#define PBF_REG_P0_ARB_THRSH					 0x1400e4
/* [R 11] Current credit for port 0 in the tx port buffers in 16 byte lines. */
#define PBF_REG_P0_CREDIT					 0x140200
/* [RW 11] Initial credit for port 0 in the tx port buffers in 16 byte
   lines. */
#define PBF_REG_P0_INIT_CRD					 0x1400d0
/* [RW 1] Indication that pause is enabled for port 0. */
#define PBF_REG_P0_PAUSE_ENABLE 				 0x140014
/* [R 8] Number of tasks in port 0 task queue. */
#define PBF_REG_P0_TASK_CNT					 0x140204
/* [R 11] Current credit for port 1 in the tx port buffers in 16 byte lines. */
#define PBF_REG_P1_CREDIT					 0x140208
/* [RW 11] Initial credit for port 1 in the tx port buffers in 16 byte
   lines. */
#define PBF_REG_P1_INIT_CRD					 0x1400d4
/* [R 8] Number of tasks in port 1 task queue. */
#define PBF_REG_P1_TASK_CNT					 0x14020c
/* [R 11] Current credit for port 4 in the tx port buffers in 16 byte lines. */
#define PBF_REG_P4_CREDIT					 0x140210
/* [RW 11] Initial credit for port 4 in the tx port buffers in 16 byte
   lines. */
#define PBF_REG_P4_INIT_CRD					 0x1400e0
/* [R 8] Number of tasks in port 4 task queue. */
#define PBF_REG_P4_TASK_CNT					 0x140214
/* [RW 5] Interrupt mask register #0 read/write */
#define PBF_REG_PBF_INT_MASK					 0x1401d4
/* [R 5] Interrupt register #0 read */
#define PBF_REG_PBF_INT_STS					 0x1401c8
#define PB_REG_CONTROL						 0
/* [RW 2] Interrupt mask register #0 read/write */
#define PB_REG_PB_INT_MASK					 0x28
/* [R 2] Interrupt register #0 read */
#define PB_REG_PB_INT_STS					 0x1c
/* [RW 4] Parity mask register #0 read/write */
#define PB_REG_PB_PRTY_MASK					 0x38
/* [R 4] Parity register #0 read */
#define PB_REG_PB_PRTY_STS					 0x2c
#define PRS_REG_A_PRSU_20					 0x40134
/* [R 8] debug only: CFC load request current credit. Transaction based. */
#define PRS_REG_CFC_LD_CURRENT_CREDIT				 0x40164
/* [R 8] debug only: CFC search request current credit. Transaction based. */
#define PRS_REG_CFC_SEARCH_CURRENT_CREDIT			 0x40168
/* [RW 6] The initial credit for the search message toCorpoCFC interface.
   CBroadcis transaction based. /* bnx2x_reg.h: Broadcom EvereINITIALork driver.
 *
 1cCopyrigh24] CIDcom Cport 0 if no matchand/or modify
 * it uID_PORT_0	 The.
 *
0fPublic Li32 (c) 2CM headercom Cflusn
 *
 * Thwhere 'load existed' byou ngramftwafolloresponseu careset and packet typ [RW 0. Usedze iccess tstart
 *
 * Tftwato TCM and/or modify
 * it uM_HDR_FLUSH_LOAD_TYPE * ThregistdcRW - Read/Write
 * ST - Statistics regis1er (cleare0RW - Read/Write
 * ST - Statistics regis2us registe4RW - Read/Write
 * ST - Statistics regis3us registe8RW - Read/Write
 * ST - Statistics regis4us registeon read)
 * W  - Write only
 * WB - Wide 5he registe0s description starts with the register Access type followed
 * by size in bits. For example [RW ]. The access types are:
 * R  - Read only
 * RC - Clear on read
 * RW - Read/Write
 * ST - StatisNOstics register (clearbon read)
 * W  - Write only
 * 6012c
/* [RW bus registcr - the size is over 32 bits an6012c
/* [RW d be
 *   c  read/write in consecutive 32 6012c
/* [RW ses
 * WR c Write Clear (write 1 to clear 6012c
/* [RW *
 */


/*cess BRB1_IND_FREE_LIST_PRS_CRDT initialize fine BRB1_RdG_BRB1_INT_STS					 0x6011c
/* a the Frhe access types 1com Cloopbackblishoftwa* R  - Read only
 * RC - Cleon read
 * RW - Read/Write
 * ST - LOOPBACKlize free  (clear9on read)
 * W  - Write onW 10] The numbd be (clearar - the size is over 32 bW 10] The numbses
  interf  read/write in consecutiW 10] The numb*
 *  interf Copyrighs above which the High_llfc signal to
   inter:
 * R  - Read only
 * ftwa *
 * This ead
 * RW - Read/Write
 * ST - register M data. 7 Write Clear (write 1 to  number ofassertedon read)
 * W  - Write ongnal to
   (clear8r - the size is over 32 b1_REG_HIGHer of fr  read/write in consecuti3] LL RAM er of fr/
#define BRB1_REG_LL_RAM 	in caseCorpre was noth_llfc sionCorpoconnestribu* RW - Read/Write
 * SNO_MATCHST - /
#define b Copyrigh1] Indicates by
in e1hov mode. 0=non-mber of cy; 1=mber of cycl/* bnx2x_reg.h: BroE1HOV_MODE de-assert1c Copyrigh8 (c) 28- sizevent  as pub_llfc signal to
   interf
 * R  - Read onftwa_REG_HIGH_LLFC_HIGH_THRESHOLD_0			 0x6014EVetwoIDC_HIGH_THRESH5  read/write in coCYCLES_4	 number of f5 Write Clear (writCYCLES_4	llfc signal 5Public Li1t (c) 2E 0x6ns types valuecom CFCoEowards MAC #0
   wasFCOEerted */
#definefree block8] Context regributhe registccess twitWrite cli The number of fn bits. For exques RC - Cletowards MAC #0
   wasStatisREGIONSe #n is de-regist0  read/write in co 			 0x60078
#definer of free 0 Write Clear (writ 			 0x60078
#definto
   inter0on read)
 * W  - W 			 0x60078
#defin_HIGH_LLFC_1r - the size is ov 			 0x60078
#defin RAM data. 1AUSE_HIGH_THRESHOLD_1 			 0x6007c
/* ine c
/* [R e client 0: Assert pause threshold. *6 by port. *B1_REG_PAUSE_LOW_THRESHOLD_0				 0x607 by port. 2G_BRB1_IN4 (c) 200cremLES__PAUSEto sen  - Rprogram /
#define BRB1_REG_Powards MAC #0
   wasINC_VALUEG_NUM_OF_P04		 0x60090
/*f1_PRTi [ST 32] * [Rx600dc
/ *
 * Ts				ram on receive access sowards MAC #0
   wasNICrted. */
#define3BRB1_REG_NUM_OF_FULL_CYCLES_0				 0_0		sss type 0x601iCCUP24] The numbeftwa of full b
 * R  - Read only
 * RC - Cleon read
 * RW - Read/Write
 REG_NUM_OCYCLES_4IGH_THRESHOG_BRB1STicensc) 2number ohe nput1] CM10] Write clnable. If 0 - the valiUM_OFoadcoStatisMESSAGESver.
 *
 2 CopyrSTe BRB1_RE. If 0 - tcycl [RWhe Parser haltR  -ts operatribusince iES_1	could/* [R lloT 32norman0x60ser2009. If 0    disregarded; valid is dDEAD_CYCLther disregar Interface enable. If 0 - the acknput is
   disregarded; valid is dPACKETd to  signals4Interface enable. If 0 - the ackn redparLES_edge input is
   disregarded; valid is dTRANSPAnetwoerted; all othersignalsPublic Li_1				 0x600bc
/* [RW CFC InterRB1_REG_Nite client 0_llfc signaftwa: De-assert pause thresholed to read the value of the XX protection	 0xd00x60078
#define BRB1_REG_2 Write Clear (writ of AG context regioer of free 2on read)
 * W  - W of AG context regioto
   inter3r - the size is ov of AG context regio_HIGH_LLFC_3  read/write in co of AG context regio RAM data. 3airs. Designates the MS
   REG-pair nud by port. 3f region 0 is 6 REG-pairs; the value sS_0				 0x64
   Is used to determine the number oftware. */
#4] InteR 2] debug only: N If 0 - tpendingefine BRs [RW CACM - lishedtowards MAC #0
   wasPENDING_BRB_CAC0_RQ0
/* [RW 17garded; valid is deasserted; all other signals are tres with parsing;
   if 1 - normal activity. */
#g.h:  CCM_REG_CCMG_BRB1_90
/* terrup600bc
ster #0 read
   if 1 - normal actRS_INT_S00c0
/* [RW 1BRB1_REG_L8]al aity maskl other signals a/write8
/* [RW 3] The size RS_PRTY_MASK */
#defineagarded; define CCM other signals are treated as usual;
   read  - normal acti9ity. */
#def			 0x600bc
/* [RW pure acknowledge/
#define CCM_REG_CCM_PRTYftwaSTS					 0xd01e8
/* [RW 3] The size URE0x60078
 de-asserte1] InteRe BRBid is deasseS set the Q instatus lsb 32y sis. '1'REG_CAM_OCCthisS_1		erai the Q in5c
/relee it by SDM but can* [Rbe uusualecause a previouther siget the Q in5c
/* [Rd as usutowards MAC #0
   wasSER of id iSTATUS_LSB0
/* [RW 15erface enable. If 0 - the request and valid imput
   are disregarded; all other signals are treated as usual; if 1 - normal
   activity. */
#define CCM_REG_CDU_AG_WR_IFEN					 0xd002c
/* [RW 1] CDU STORM read Interface enablM. If 0 - the  CopyriB1_Re. If 0 - theRC cur*/
#d Broad. T redistribute it and/or modify
 * it SRCest network driverr.
 *
 *Public - thid is deasseTCMegarded; all otheC 1 -ute it and/or modify
 * it TCM normal activity. */
#defiid is deEG_CDU_SM_WR_IFEif 1garded; all other signals are treated as usual; if 1 -al c normal activity. */
#defrted. */ht (Dd is deasserted; all ctivientri2] T [R 5]data FIFOndex; receiveXP2: BroHST_DATA_	 0xce enaby. */
#12047ized to 7 at start-up. */
#define CCM_REG_CFC_INIT_CRs with 	 0xd0204
/* [RW 2] AuxillaryHEADERnter flag Q ULL_er 1. */ Write Clea2] AuxilPGL_ADDR_88_F0ULL_BLOC1205text REG-pai* [RW 28] The CM hCader value for Q	 0xd00cc
/* [RW 28] The CM 90ader value for Qon read)
 *	 0xd008c
/* [RW 24ader value for ce enable. I* [RW 28] ThCONTROL* The reg1. *9CM_REG_CQM_CCM_HDR_S					 0xd0_HIGH_THRfor  24] The num* [RW 28] ThDEBUG- the valid define BRBptiothird dwo all				of expansriburomM_REG_CD.ll otest input other special.als ar sifted it provides a vector outstaher sign#define BRs. ifftwaay sizes zeroM_REmeanall a[R 2 [RW 6] QM oe number orexampldefintag die CCM* [Rfinish yet (		 0xd0 completribs have arrInterom Cit)d0204
/* [RW 2] Auxil] ThEXP_ROM number o12080/
#define BRBInbouc
/* ted; alltablSE_CYCCSDM:  are[31:16]-_REG;ftwa [RW15:0]-addresnable. If 0 - be initializif 1020c_er value for4fnput is disregarded;
  ism. 0
  1stands for we	 0xd00cc
/* [RW 28] Thism. 0
  2stands for wevalue for QM request (sism. 0
  3r value for 0CM_REG_CQM_CCM_HDR_S			ism. 0
  4_P_WEIGHT				ight 8 (the most prioritised); 15_P_WEIGHT				ight 1(least
   prioritised); 2 6_P_WEIGHT				ght 2; tc. */
#define CCM_REG_CQ7r value for 1G_BRB1_INT_STCM_REG_CQM_INIT_CRD					 0xdal c
/* [RW 3] The weight of the QM (primary) input in the WRR mechanism.it
   stands for w9ight 8 (the most prioritiseit
   stands for w9ight 1(least
   prioritisedit
  stands for weblocks belownals are treated as uM_P_WEIGHT		4face #n is anals are treated as uf the QM (se4LOW_THRESHOLnals are treated as um. 0
   stan4al other signals are treated as used); 1 stan4a- normal activity. */
#define CCed); 2 stand4bfor weight 2; tc. */
#define CCM_REG_CQM_U_WEIGHT					 0xd00bc
/* [RW 1] Input SDM Interface enable. If 0 - the va mecinput is disreblength mismatch (relative t weig stands for wbight 1(least
   prioritised weigstands for weress BRB1_INEG_CSDM_WEIGHT					 0M_REG_CSDM_IFAt
   addresEG_CSDM_WEIGHT					 0n the messagefree tail. AEG_CSDM_WEIGHT					 0t indication)initialize pEG_CSDM_WEIGHT					 0ted. */
#defiREE_LIST_PRSEG_CSDM_WEIGHT					 0xd0170
/* [RWfree blocks abCM_REG_CQM_INIT_CRD					 0xdX_WEIGHT					 0xd00bc
/* [RW 1] Input SDM Interface enable. If 0 - the vax crinput is disredREG_ERR_CCM_HDR					 0xd009retur stands for wdnt ID in case the input mesreturstands for we on read)
 *4 at start-up. */
#deM_REG_CSDM_IFer - the siz4 at start-up. */
#den the message   read/writ4 at start-up. */
#det indication)- Write Clea4 at start-up. */
#deted. */
#defi [R 19] Inte4 at start-up. */
#dexd0170
/* [RWEG_BRB1_INTrted;s field0xd00ws one funstributoefintdc
/ber siano 0x6			 0xd02ftwawhen accese. I any BAR mapper exaourceient INIT_CRDevicel;
 OF_PAUSEot iniity _FIC1_ cans usual;
   iccm_r	 0xd0214avaiwill   ater gree CCMefffullvely. afr sisoftwa6015		 0xis prisy sizet mus 0xda*/
#_INIorith toftwa if m_regi [RW 1w					 0is upda. */ input in the WRR mechanPRETEND_FUNne Cat start-u67rite
   writes the initFIC1 and storeer of frfine returns the current valFIC1 and storeto
   infine nter. Must be initializFIC1 and store_HIGH_LLfineree blocks b highest priority is 3. It RAM datosed;CCM_REG_GR_ARB_TYPE					 0xd015c
/* [d by porosed;IC0) channel group priority. The loweS_0				 osed;s 0; the
   highest priority is 3. Ittware. *fine9id is deass.gr_ld1_prarded; all vailable - 32. Wri5c
/blockivity. */
#d by ~bus_maer s_enity ideasserggregation; load FIC0; load Rs inBLOCKEW 1] CM for 6returns the current valTAGS_LIMITr value for */
#defi 1EG_CDU_SM_WR_egation; load FIC0; load TXW_CD00c0
/* [Routpuized to  channel group priority. The gr_ld1t priority is 0; the
   highest priority is 3. It is supposed; that the Store channel pWRITEity is
   the complirite
   writes the iSWRQ_BWhe C - the valid1f an error in the QM
context and er value for1Write
   writes the icontext and  stands for 1 returns the current context and2 at start-u1_REG_ERR_CCM_HDR					f 128
   bit8r value for2pairs. Desigata in the STORM conllfc signffset0. Index
   _(0..15) stands forS_0		he offset value of the
   crecontext andtwareEG_N_SM_C0. Index
   _(0..15) stands for8CM_REG_N_SM_Cnter. Must be initiacontext and9the connectiut initial credit. Macontext ctivity. */
#1203if region 0 058
#define CCM_R0 - the valid2b3					 0xd0058
#define CCM_RL1090
/* [RW 12CTX_LD_0					 0xd004c
#defineL1bf Interface X_LD_1					 0xd0050
#define CL2 at start-u2 prioritised); 2 stanated as usREG_N_SM_CTX3*/
#define Btivity. */
#define the connect2define CCM_REG_CSDM_Wated as uCCM_REG_N_SM2t of these data in the STORM LM_REG_N_SM_C2on type (one of 16). */
#defiLREG_N_SM_CTX2et. */
#define CCM_REPBF_LENGTG_N_SM_CTX_L2d3					 0xd0058
#define CCM_RRW 1] TX_LD_4		- normal activity. */
#definUBbf Interface CM_HDR_P					 0xd008ctised); 1 sis disregarde5			 0xd005c
/* [RW 1] Input UBasserted; all 63					 0xd0058
#define CCM_RUBsual;
   if 1 value for QM request_REG_PHYS_QH_MIS					 0xEN					 0xd0018
/* [R_REG_PHYS_ when the mesCCM_REG_CQM_CCM_HDR_S_REG_PHYS_ to last indi4ght 2; tc. */
#define CCM_REG_e is detected5c
#define CCM_REG_PHYS_QNUM2_1H_MIS					 0x/* [ST 32] TCM_REG_PHYS_QNUM2_1the input pbf pause signaCM_REG_PHYS_QNUM2WRCM_REG_PBF_I 0. Index
   _(0..15) stanCDU0_L2Pr value for0		 0xd00b8
/* [RW 3] conteQM1_0		the connectssage Reg1WbFEG_QOS_PHYS_QNSRC1_0				 0xd011c
#fine CCM_REG_QOS_PHYS_QNUMTM1_1				 0xd0120
#0094
/* [RW G_QOS_PHYS_QNUMSDM1_1				 0d0120
#e
/* [RW 8] FIC0ted; all_REG_CCM_STORM1_IFEN 				 0xd0008
/* [RWbe initia2] Aif 1Intene BRB1_Rfor 7 channelEG_QOS_PHYS_QN other signals are treated asnterface enable. STSinput is disr56fine CCM_REG_QOS_PHYl other sign stands for 6. */
#deC
   disregarded; acknowledge outcleaindex; receiveal; if 1 - normal actCLR - the valid iG_BRB1_INT_STine CCM_REG_CCM_STORM1_IFEN 				 0xd0008
/* [RWal; if 1 - noread Inte - the valid nnel; Load
   (FIC0) e CCM_REG_STORRW 2] Load 5 other s indication); acknowledge output is deasserted; all ot is deas - the valid iated as usual; if 1 - no for weigf the STORM 8_GR_LD1_PR	t start-up. c) 2'almost fullregarded;
/* [ted each fifo (givether e prioritisabout  de-pmaryurecounter. Must be initiRD_ALMOST_FULL - the valid4terface e8sed); 2 stands fors 0; sIFENS_PH -usual;
   iunctivit 0;  idy) input in the WRR meRrityK_CNty. *UX2_Q			1ity. */
#defd); 2 stands ott the Q inof avail					arded; in Tetris BufferoftwaMroupbe biggg tohan 6. Normally shEN					 0be chang
/* [RW 1] CDU Sls are
   treatid iCFGd0170
/* [RW0Public Lic] CDU byte swappoupsf cy usufigue CCM_Rom Cty is * [RW 6] QM oul other signals are
   tCDURD_SWAPrted. */
cted. */erface 090
/WArbi'1';the ac[RW 1ormalSWRDserted;ers_ignorgregation; load FIC0; lRD_DISABLE_INPU00c0
/X_LD_4	M_STORM0_1]tands fQM_INnal memo_CFC_IN7-200izoritisis dD			ation)
   at the tsem ierms_DONEG_NUM_OF*/
#dSE_CYCLES_1	c) 2maximumusual;
   i				 0xd001c
/* [RC 1] ty. Thcan bd as xd000c
/edit
 vq10em Interface enable. If MAXtreaS_VQ standinput ia
   disregarded; acknowledge output is deasserted; all other signals are
   treated as usu1l; if 1 - normal activity. */
#definRW 2] Load 3e. If 0 regarded; acknowledge output is deasserted; all other signals are
   treated as usu7l; if 1 - normal activity. */
#defin/* [RW 2] L3bd */
#defineded; acknowledge output is deasserted; all other signals are
   treated as usu8l; if 1 - normal activity. */
#definREG_Ninput ic
   disregarded; acknowledge output is deasserted; all other signals are
   treated as usu9l; if 1 - normal activity. */
#definG_N_S1] Inpute CCM_REG_USEM_LENGTH_MIS 				 0xd017c
/* [RW 3] The weight of the input usem in the WRR 22l; if 1 - normal activity. */
#defi2st priority3USE_CYCLES_1	rmal activity. */
#define CCM_REG_XSEM_IFEN					 0xd0020
/* [RC 1] Set when th5 message length mismatch (relative tregation ch3ditised); 1 stands for weight 1(least
   prioritised); 2 stands for weight 2; tc. */
#defi6l; if 1 - normal activity. */
#defi(FIC1). */
3ad (FIC1regarded; acknowledge output is deasserted; all other signals are
   treated as usasserted; all other signals are
   teated as usu9
#define CCMPBFG_TSEM_LENGTH_MIS 				 0xd0174
/* [RW 3] The weight of the input tsem in the WRR mPBFism. 0 stands for
  3fefine CCM_Rt start-up. * [ST 31] Inf deliverys ususight 1dlsem Interface enable. If 
 *
 IS_IDLine BRB1_R; if fine CCM_REG_QOS_PHf XX protection RW 2] Load 4define BRB2] QMG_TSEM_LENGTH_MIS 				 0xd0174
/* [RW 3] The weight of the input tsem in the WRR mQMX_DESCR_TABLE					 0xdnput in  CCM_REG_CNT_AUc) 2SR acknowledge output is deasssubflags ind all other signals are
   tSRted as usual; if e CCM_REG2]disre_TSEM_LENGTH_MIS 				 0xd0174
/* [RW 3] The weight of the input tsem in the WRR m
   sm. 0 stands for
   wG_BRB1_INtion pending
   define CCM_REG_TSEM_IFEN	PCIps aresub-6] QM outphen t prie message leng1h mismatch (relative to last indication)
   at the tsem iSRrface is90
/* [RW 1]. */
#defi1] Signal normalnds for weix600
 * R0a0
/* [RoupsM_WEIGHT					 0xd0. At read the ~ccm_regisTAprotNity. *d as usuine CCM_urrenTalue for the credit counter; responsible for fulfilling
   of the Input Stage XX prTtection buffer by the Xted. */
#d0] Bandwidrityddixd0214
/VQ0neral flags ins_xx_free.xx_free
   co standshe C090
/* [RW 11ritised); 	 0xd0044
/* [RW 18] Indirec12 weight of the input tsem in the WRRtection mec1stands for w1e fields are: [5:0] - tail pointer; 11:3] - Link List size; 17:12] -
   header pointer. M_P_WEIGHT		1EG_BRB1_INre: [5:0] - tail pointer; 11:4] - Link List size; 17:12] -
   header pointer. f the QM (se1d0300
#d101004
#define CDU_REG_CDU_CONT5] - Link List size; 17:12] -
   header pointer. m. 0
   stan1XX prote101004
#define CDU_REG_CDU_CONT6] - Link List size; 17:12] -
   header pointer. sed); 1 stan1VNT_ID 				 0xd0044
/* [RW 18] Indirec17] - Link List size; 17:12] -
   header pointer. ed); 2 stand2/
#define 101030
/* [RW 5] Parity mask 8] - Link List size; 17:12] -
   header pointer. ext is alwaysweight 8 (t01030
/* [RW 5] Parity mask 9] - Link List size; 17:12] -
   header pointer. 9_MASK					 0x CDU_REG_CDU_INT_MASK					 0x10103c
/2nals ar to the XX table of the XX protection mec2er value for2/
#define  ctive_error;
   ype_error; c6] - Link List size; 17:12] -
   header pointer.2stands for w2 for weigh ctive_error;
   ype_error; c_REG_CDU_CHK_MASK0					 0x101000
#define CDU_REG2M_P_WEIGHT		2ns the cur ctive_error;
   ype_error; cROL0					 0x101008
#define CDU_REG_CDU_DEBUG				2f the QM (se2 1 - norma ctive_error;
   ype_error; c1020
/* [RW 7] Interrupt mask register #0 read/w2m. 0
   stan2 Public Li ctive_error;
   ype_error; c [R 7] Interrupt register #0 read */
#define CDU2sed); 1 stan2define BRB ctive_error;
   ype_error; cregister #0 read/write */
#define CDU_REG_CDU_PR2Y_MASK					 0terface alizing the activity counter bROL0					 0x101008
#define CDU_REG_CDU_DEBUG				 */
#defiffsets set the CDU operates in e1hmf mode 1020
/* [RW 7] Interrupt mask register #0 read/wd by EG_N_SM_Ce initializing the actiTypical L [RW ect R- Link List size; 17:12] -
   header point0090
/* [RW 12as set the CDU operates efine CFC_REG_CA12_INIT_DONE					 0x10407c
/* [RW 2] Interrupt 1sual;
   if 1RR mechanirite */
#define CFC_REG_CFC_INT3MASK					 0x104108
/* [R 2] Interrupt register when the mesfine CCM_Rrite */
#define CFC_REG_CFC_INT4MASK					 0x104108
/* [R 2] Interrupt register			 0x1044002e 0x104078
/* [RW 13] ace CFC_REG_CFC_INT5MASK					 0x104108
/* [R 2] Interrupt register cams by the2egionLength[11:0]; egione CFC_REG_CFC_INT6MASK					 0x104108
/* [R 2] Interrupt register to last indiM_REG_XX_TABLE					 0xd0e CFC_REG_CFC_INT7MASK					 0x104108
/* [R 2] Interrupt registere is detected					 0x101004
#define Ce CFC_REG_CFC_INT8MASK					 0x104108
/* [R 2] Interrupt registerH_MIS					 0xfine CDU_REG_CDU_GLOBAL_e CFC_REG_CFC_INT9MASK					 0x104108
/* [R 2] Interrupt registerthe input pbfe CDU_REG_CDU_INT_MASK		efine CFC_REG_CA2M_INIT_DONE					 0x10407c
/* [RW 2] Interrupt 2mask registerVNT_ID 				 0xd0044
/* [ set one of these_MASK					 0x104108
/* [R 2] Interrupt registeto lasd as usux10104c
/* [R 5] Parity  set one of these/* [RC 2] Interrupt register #0 read clear */
2 the connect3ging of error data in ca set one of these Parity mask register #0 read/write */
#define2			 0x1044003gion[2:0]; ctive_error;
 set one of thesegister #0 read */
#define CFC_REG_CFC_PRTY_STSxsem id_client0efine CFC_REG_CFC_INT_STS					 0x1040fc2alid - 0) */
#define CFC_REG_CID_CAM 					 0x12CCM_REG_N_SM3gions[7:0]; ffset12[5:0]ent should be serC_REG_DEBUG0						 0x104050
/* [RW 14] indicat2M_REG_N_SM_C3ine CDU_REG_L1TT						 0efine CFC_REG_CA Parity mask register #0 read/write */
#defineCFC_REG_CFC_Pe fields are: [5:0] - taefine CFC_REG_CAgister-redit valy valuctivi 0x10407c
/* [RW 2] Interrupt 				 0x10410ct xsem Int70xd0044
/* [upper _REG_CREG_CAM_ - Link List size; 17:12] -
   header pointUBOUNchanis0					 0 0x104078ate */
#define CFC_REG_NUM_LCID:6] - Link List size; 17:12] -
   header pointving Lr #0 rGHT					List Block */
#define CFC_REG_NUM_LCIDS_A_REG_CDU_CHK_MASK0					 0x101000
#define CDU_ng LCID is suppose2* Copyrighk */
#define CFC_REG_NUM_LCIDS_AROL0					 0x101008
#define CDU_REG_CDU_DEBUG	ng LCID  the compl2G_XX_MSG_Nk */
#define CFC_REG_NUM_LCIDS_A1020
/* [RW 7] Interrupt mask register #0 reang LCIDregation ch2s
   disrek */
#define CFC_REG_NUM_LCIDS_A [R 7] Interrupt register #0 read */
#define ng LCID(FIC1). */
2M_STORM013				 0xc206c
#define CSDM_REG_AGregister #0 read/write */
#define CDU_REG_CDUng LCID/* [RW 2] L2input is13				 0xc206c
#define CSDM_REG_AGd */
#define CDU_REG_CDU_PRTY_STS					 0x1010ng LCID [RW 1] Inp2/
#defin13				 0xc206c
#define CSDM_REG_AGd error:
   {expected_cid[15:0]; xpected_typeng LCIDeated as us28_EVENT_13				 0xc206c
#define CSDM_REG_Actual_active; ctual_compressed_context}; */
#dving L
   sp7				 0x0xc2074
#define CSDM_REG_AGG_INT_EVENT_1 ram access. each entry has the following form	 0xc20st priority2BRB1_REG_Laggregated interrupt index whethING				 0x104018
/* [RW 8] The event id for aggregaG					fine CSDT_EVENT_5				 0xc204c
#define CSDM_REG_A 24] MATT ram access. each entry has the follo CSDM_R60
#define C#define CC 0xc21e8
#define CSDM_REG_AGG_INne CSDM_REG_AGG_INT_EVENT_12				 0xc2068
#define CSxsem in the 29For each aggregated interrupt index whethG_INT_EVENT_14				 0xc2070
#define CSDM_REG_AGG_INT 0x104e CSDM_RM_REG_AGG_INT_MODE_10				 0xc21e0
#define				 0xc2078
#define CSDM_REG_AGG_INT_EVENT_2				 0				 0e CSDM_RT_EVENT_5				 0xc204c
#define CSDM_REG_AT_MODE_13				 0xc21ec
#define CSDM_REG_AGG_INT_MODE60
#define Cegarded;ne CSDM_REG_AGG_INT_MODE_15				 0ne CSDM_REG_AGG_INT_EVENT_12				 0xc2068
#define CSdefine CSDM_er. */
#defctive_error;
   ype_error; c9 access to the XX table of the XX protectiWRG_AC_]; xpected_read */
#defcompetion counter #1 */
#def3t access to the XX table of the XX protecti [RW 13e CDU_REG_ERRID.
   hich CFC load client should be ser9 Wine CSDM_REG_CMP_COUNTER_MAX2				 0xc2024
/L2G_N_SM_CTX_L3is set the CDU operates efine CFC_REG_CA30#define CSDM_REG_CMP_COUNTER_MAX3				 0xc2028
3090
/* [RW 13define BRB 0xc21e8
#define CSDM_REG_AGG_INasserted; all other sigxc2024
/ss in tVENT_7				 0ne CCM_REGate */
#define CFC_REG_NUM_LCID3al; if 1 - normal activite */
#defineMP_COUegister gionLength8] exWEIGHT	first_mem_prim_registen 0			D					 0xd0DUMIS uleblished DM_INT_MASK_1				 0xc22NUM1_EFIRST_MEMhe CM 0xd011c
#d Copyrigh2] EndianMIS 		easseduG_CSDM_INT_STS_1 				 0xc22_ivitAN_Mams by the EN					 0xd0018
/* [ASK 			/* [RWILSM_CTX_LD_46. */
#define CCM_REG_SK 			LAefine CSDM_REG_CSut is deass] p#defsiz 200S_0 				 0xc2290
#define ; -4k; -8xc2216xc2232xc2264ight o-128k CSDM_REG_CSDM_PRTY_MASK 			P_SIZvalid input  */
 is deasssregarded; all d
   oufine BRtivitEG_F rea */
#deM_WEIGHT as usu0xd0174
/* _CSDM_INT_STS_1 				 0xcFGthe valid input 1bs the currenad/write */
#defid is DM_INT_MASK_1				 0xc22DBG		 0xc22bc
/* [R 11] eight 8 (the most prio to the Xstersenowlehe ackbC 1] s1 - nwont get		 0ar on wardrs_gr_gAUSETRL				 0xc24bc
/* [ST 3s for weight 2; tc. */
#imum value ] 1 -ssagsters_gr_lignsual; 64B; 0r */
#define CSDM_REG_NUM8BT 32] The number of packeRAM_ALIGNefine CCM_RE 3] The wefine C1 ILT failiu; 1
ll			 0xdsulaccorELTr_ld0_p; AnCQM_INIT_CRREG_ding meupposed; that the Store channRQ_ELTket end vity. */
#deG_XX_MSG_NUM	ad/write */
#defhcT 32] The number of packHC		 0xc22bEG_CDU_INT_Sgister #0 r]t Arbi'0'#defilogic_REG_Nwork a0xd00A0; tion wise B0;
#def de-QM_IFEmpatibil CCMneeds; No
/* [at di 1] LES_ other s [R 7]ctiviCFC_IS 		mmands received in queuILTrted. */
#def the ] InteWB 53] Onchip[RW mary)D					mmands received in queuONCHIP_Adefine CCM_c
#dceived in queue 1 */
#define CSDM- BEG_CSDM_INT_STS_1 				 0x 0xc224c
_B. */
#defi8The numbync 3] Pther signad limiof mthresholdiori D oth size; 17:12] -
   headerP13c
ies alid input i3NUM_OF_Q0_CMD					 0xc2248
/qmM_REG_NUM_OF_Q1_CMD					QMands received in queugarded;
   acknowledeue 5 
#define CSDM_REG_CSQM request (primary)eue 5 efine CSvity. */
#derded;
   a	 0xc2238
#define CSDM_REG_ENAQM_IN2					 0xc223c
#define CSDM_REG_ENABLE_OUT1					 0xc2240
#define CQM_REG_ENABLE_OUT2			5rom the parse [RW 4] The initial nRBCmessages that he pxp ciniti write QM_REG_NUM_OF_Q1_CMD					RBCiving any ACK. */
E_IN1					 0Max burst38
#defilted as weight of the lished; 00KT_E128Bght o001:256F_PK10: 512B; 11:1K:100:2Kxc22:4KM_REG_NUM_OF_Q8_CMD					 D_MBShanism.
   Thrite wrier of commands received in queue 9 */
#define CSD1_REG_NUM_OF_Q9_CMD					 0xc2268
/* [RW 13] The start address in the internal RAM for qu sent to STOR* Copyrigh_CMD					 0xc2248
/sr [ST 32] The number of co
   	 0xc22bc
/* [R 11] - normal activity.  */
#de
/* [ST 32] The numbeARSER_EMPTY				 0xc2550
/*efine CSDM_REG_ENAB4E_IN1					 0xc2238
#define CSDM_REG_ENAisreIN2					 0xc223c
#define CSDM_REG_ENABLE_OUT1					 0xc2240
#define C
   REG_ENABLE_OUT2			_NUM_OF_Q0_CMD					 0xc2248
/tommands received in queueT5 */
#define CSDM_REG_Nefine CCM_REG_QOS_P000
/*
#define CSDM_REG_CS4r of commands receivedTin queue 6 */
#define er. */
#de	 0xc2238
#define CSDM_REG_ENATT 32] The number of commands received in queue 7 */
#define CSDM_REG_NTM_OF_Q7_CMD					 0xc List Bl 5]erted; all REG_CFC_INIT_CRu. */; _REG_Ffomessal2p current valummands received in queuUter fid is dENTRYat start-up.value of tead */
#define CSDM_REG_CSDM_INT_STS_0 				 0xc229 mecdefine CSDM_REG_CSDM_INT_STS_1 				 0x meca0
/* [RW 11] Parity mask rREG_AGG_d insleeping thread wioccupisual; vqed bn pswrq					 C0) channel and Load (RQ_VQ0ENT0		ted as utart-up for wei0; 3-
   sleeping thread with priorit1y 1; 4- sleeping thread with priority 2.
  1 Could not be equal to 	 0xc224 ~csem_registers_arb_element0.arb_ele1ent0 */
#define CSEM_REG_ARB_ELEMENT1					 1Could not be equal todefine Bthat is associated with arbitration el2ent0 */
#define CSEM_REG_ARB_ELEMENT1					 2c1; 2-sleeping thread source that is associated with arbitration el3ent0 */
#define CSEM_REG_ARB_ELEMENT1					 3Could not be equal toimum valthat is associated with arbitration el4ent0 */
#define CSEM_REG_ARB_ELEMENT1					 41.arb_element1 */
#defsource that is associated with arbitration el5ent0 */
#define CSEM_REG_ARB_ELEMENT1					 5Could not be equal toREG_SYNCthat is associated with arbitration el6ent0 */
#define CSEM_REG_ARB_ELEMENT1					 6ad with priority 1; 4-source that is associated with arbitration el7ent0 */
#define CSEM_REG_ARB_ELEMENT1					 7Could not be equal to2260
/* that is associated with arbitration el8ent0 */
#define CSEM_REG_ARB_ELEMENT1					 8ent2 */
#define CSEM_Rsource that is associated with arbitration el9ent0 */
#define CSEM_REG_ARB_ELEMENT1					 9Could not be equal torite writes
sem_registers_arb_element0.arb_eleent0 */
#define CSEM_REG_ARB_ELEMENT1					 iority 1; 4- sleeping source that is associated with arbitration e2y 1; 4- sleeping thread with priority 2.
  2 Could not be equal tolid is deters_arb_element1.arb_element1 and
  l to register ~csem_registers_arb_element0.2arb_element0
   and ~cinput is
ters_arb_element1.arb_element1 and
  riority 1; 4- sleeping thread with priority22.
   Could not be equxc2054
#e */
#define CSEM_REG_CSEM_INT_MASK_0	b_element0
   and ~csem_registers_arb_eleme2t1.arb_element1 */
#deBRB1_REGe */
#define CSEM_REG_CSEM_INT_MASK_0	W 3] The source that is associated with arb2tration element 3. Souad (FIC1)ters_arb_element1.arb_element1 and
  g thread with priority 0; 3-
   sleeping th2ead with priority 1; 4input in ters_arb_element1.arb_element1 and
   be equal to register ~csem_registers_arb_e2ement0.arb_element0 anEM_IFEN	200124
#define CSEM_REG_CSEM_PRTY_STS_ and
   ~csem_registers_arb_element2.arb_el2ment2 */
#define CSEM_n channelters_arb_element1.arb_element1 and
  ource that is associated with arbitration e2ement 4. Source
   decThe numbSEM_FAST block. The SEM_FAST registersith priority 0; 3-
   sleeping thread with 2riority 1; 4- sleeping			 0x60SEM_FAST block. The SEM_FAST registersmemory should be added to eachsem_fast regiCould not be equal tot xsem ISEM_FAST block. The SEM_FAST register3y 1; 4- sleeping thread with priority 2.
  3 Could not be equal to BRB1_REthe microcode */
#define CSEM_REG_FIC0l to register ~csem_registers_arb_element0.3arb_element0
   and ~chardwarethe microcode */
#define CSEM_REG_FIC0code */
#define CSEM_REG_FIC1_DISABLE					 x200234
/* [RW 15] IntMay be updated during run_time
   by the micW 3] The source that is associated with arbtration element 3. Soufine CCMSEM_FAST block. The SEM_FAST registerg thread with priority 0; 3-
   sleeping thead with priority 1; 4
/* [RW SEM_FAST block. The SEM_FAST register be equal to register ~csem_registers_arb_eement0.arb_element0 anEG_BRB1_SEM_FAST block. The SEM_FAST register and
   ~csem_registers_arb_element2.arb_elment2 */
#define CSEM_XX protecith priority 2.
   Could not be equaource that is associated with arbitration eement 4. Source
   de9/
#definumber of messages that were sent to
 ith priority 0; 3-
   sleeping thread with riority 1; 4- sleepin9. */
#definof commands received in queaccess to the Xe CSDM_REG_NUM_OF_Q9_CMD					 0xc2268
/* [RWhread with priority 2.
WRr queue counters rted. */
#[ST 24] Statistics register. The number of messaR 1] pxp_ctrl rd_data fifo empty in sdme CSEM_REG_MSG_NUM_FOC3					 DATA_EMPTY			 s the curren_NUM_OF_Q  - 		 0xc -
/* [RWNUM_02OF_P ArbiefinpayfolloINIT_Ct priol othr   t all otthe Q inhas_
#definesters_gr_pposed; that the Store channWRK 			MPdefine CCM_REEG_BRB1_INc
/* [WB 128] Debug only. Passive buffer memory */
#define CSEM_REG_PASSIVE_BUFFER 				 0x202000
/* [WB 46] pram memory. B45 is parity; b[44:0] - daetur */
#define CSEhardware wc
/* [WB 128] Debug only. Passive buffer memory */
#define CSEM_REG_PASSIVE_BUFFER 				 0x202000
/* [WB 46] pram memory. B45 is parity; b[44:0] - d32]  */
#define CSE
/* [RW 21_ls_ext */
#define CSEM_REG_SLOW_EXT_STORE_EMPTY				 0x2002a0
/* [RW 16] List of free threads . There is a bit per thread. */
#define CSEM_REG_THREAMAE_LIST					 0x2002value of the ifh priority 2.
   Coin dmaeivefo] pram mehigon b lengl other s receivery *n00
/* [WB 46]e prioritis pram memory. B4; strictfaXP_A_PAUSS_1		relatibe equales t&gt; . The nMBS38
#d!e_slot 10 */
#define CSEM_REGTHefine CCM_REGe4
/* [RW 3] The arbitration scheme of time_slot 0 */
#define CSEM_REG_TS_0_AS					 0x200038
/* [RW 3] The arbitration scheme of time_slot 10 */
#define CSEHC_LIST					 0x200 BRB1_REG_c
/* [WB 128] Debug only. Passive buffer memory */
#define CSEM_REG_PASSIVE_BUFFER 				 0x202000
/* [WB 46] pram memory. B45 is parity; b[44:0] - dQE FIFO is empty invalue of t
/* [WMD 	of the A0 write_e_slot 17 */
#Befine 5 is parity; b[44:0] - dREerted. */
#deffine  sem_slow_ls_ext */
#define CSEM_REG_SLOW_EXT_STORE_EMPTY				 0x2002a0
/* [RW 16] List of free threads . There is a bit per thread. */
#define CSEM_REG_THRE
   LIST					 0x2002	 0x20024c
/* [WB 128] Debug only. Passive buffer memory */
#define CSEM_REG_PASSIVE_BUFFER 				 0x202000
/* [WB 46] pram memory. B45 is parity; b[44:0] - dTE FIFO is empty ifine CCM_Re_slot 2 */
#define CSEM_REG_TS_2_AS					 0x200040
/* [RW 3] The arbitration scheme of time_slot 3 */
#define CSEM_REG_TS_3_AS					 0x200044
/* [RW ORE FIFO is empty in 0x104078
/* W 3] The arbitration schusdmdpof time_slot 11 */
#define CSEM_REG_TS_11_AS					 0x200064
/* [RW 3] The arbitration scheme of time_slot 12 */
#define CSEM_REG_TS_12_AS					 0x200068
/* [RW 3] The arbitratio mecDPheme of time_sler. */
#dec
/* [WB 128] Debug only. Passive buffer memory */
#define CSEM_REG_PASSIVE_BUFFER 				 0x202000
/* [WB 46] pram memory. B45 is parity; b[44:0] - d weigration scheme of
#define CCM* [WB 128] Debug only. Passive buffer memory */
#define CSEM_REG_PASSIVE_BUFFER 				 0x202000
/* [WB 46] pram memory. B45 is parity; b[44:0] - dreturFIFO is empty in	 0xc2244
/id is deasse_DESCR_TABLE_SPSWHST arbmmandis] Used to read the vAuxillaryARBotectionr value fEG_Iiority 0; 3id is deasseAy siz_REG_		 0xllx102400
#define clienre disrecrediM_REG_isinitial DMAwaitefinem Corpo#defineication)
   at tAuxillaryCLIENTS_WAITty. TO
/* 0 - the CRC. */
#define DMAE_REG_Clue is alnes. *102400
nputiscardefindoorbells. Tgr_ld1_ */
#defineM: aggr_ldo*/
#deo 'hst_value i_l ones. *'eighted Romemory */vali2 */
machx_reDMAE_REG_CMD_MEM_SIZE					 224
DISCAtandOORBELLSflag Q num the CREM_REG_EN6ial value is all zeroes; if 1 - the CRC-M_WEIGHT	gr_ld1nitial valuebove wCRC-16 T10 initial value is alritiseded);
es. */
#def. E  tc siz#definftwaC16T10_INIT					 0x102020
/* [RWM_WEIGHT_gr_ldrrupt mask register #0 read/write */
#define 		 0x10201c
/* [RW 1] If 			 0x10INTERNA REG-pa[RW 4] Par the CRRAM forWB 160] */
#dit
  a0
/* [RW 1] Ir_ag_priM_REG_CQM_INIT_Cseeping thread with priAuxillaryINing L084
f 0 - the C8/
#define EG_QOS_PHYS_QNUM3_1				 0xd0130
/* [RW 1] STORM - CM Int [RW 3] ble. If 0 - the go. */
 CCM_REG_GR_ARommand 13 go. */
# - the valnt0[erface enablisregarded; acknowledge output is deasserommand 13 go.weight 8  go. */
iment to 4 of 1] Command 15 go.1] Command 14input is_REG_STORM_CCM_IFEN					 0xd0010
/* [RC 1] Set when t1] Command 15 go.ismatch (relae DMPublic Lic6dication)
   at the STORM interface is detected. */ommand 13read Interface en */
#garded; v	 0x1020a4G_GO_C14 					 0x102098
/* [RW 1] Comman is deasserted; d 14  Copyrigh enableistrv CCMacknowle007-2009OFT_RESET					 0sdefinory */fine CCM_REG_CDU/* bnx2x_reQM: BroACTCTR/
#dVAnable.  go. 68ace enable. 		 0x1020b4
/* [RW 1] - the val go.  read/wri		 0x1020b4
/* [RW 1]efine CFC_ go. Write Cle		 0x1020b4
/* [RW 1] the conne go.rs description stte iNUM_OFal*/
#defin(inalue s)gnals  tcphysIf 0 queue/
#d2 */
index The 		 0ial ry */
s disregardedthe Q i; val prilsb [R 7] (leaster #0 rconsideast
it. Ms
/* istrcatch  0x601ers_(FIC020  are7 go.   if 1 - noght oardeds 63-EG_CSDM_INT_S		 0x10BASEe CMULL_BInterfa Statistit; aster) enable. If 0 - the acknowledge
   input is disregarded; valid is deasserted; all other signals are treated
   as usual; if 1 - normal activity. */
#define DMAE_REG_GRC_IFEN					 0x102008
/* [RW 1] DMAE PCI Interface (Requ127-64 ead; rite) enable. If 0 - _EXT_AGRC Interfe1x10104c
/* t (c) 2_TSEM Broadccght 2orinput task/
#defis to STORe numot 0:
/* [ead; rite) enable. YTECRDCOSCMD					 068k List Bloche addre007-2009ss. Read ret_PAUSE_CYCer. */
#detowards MAC #G_PXP_REQ_INIT* [RW 1				 0x1020c0/
#define BRBll zerCFC_is disregarded; I_ag_pral cred/* [ReAS					ther signalsrface (ReOF_Qf messag ele [R074
/er May be e (Requ31t; ead; rite) enable. Q_INIT
 *
 e. If 0 0x1020cal to reefine DORQ_REG_AGG_CMD1					 0x170064
/* [RW 8] Aggregation command. */
#define DORQ_REG_AGG_CMD2					 0x170068
/* [RW 8]95s the
   initial value 
#define DORQ_credit couter; rput is deassertDORQ_REG_AGG_CMD1					 0x170064
/* [RW 8] Aggregation command. */
#define DORQ_REG_AGG_CMD2					 0x170068
/* [RW 8]63-3e message lendefine DORQ_REG_DB_- the rAGG_CMD3	of the coInterrupt mask register #0 read/write */
#define DORQ_REG_DORQ_INT_MASK					 0x170180
/* [R 5] Interrupt register #0 reites9
   prioritisQ_REG_DORQ_INT_STS				R0					 0x17008c
is set the e address. Read ret				 0x11 *ifM_WEv/* [RWQMRW 8]ty. */
#deweightftwa2 (mfine DMAE_REG_PXP_REQ_INI driAe enTH				 nterfacREG_AGG_IN1_REG_SOF7-2009 Broadcom Cis free sfine DMAE_REG_PXP_RCM/
#dCRD * The regrfacREE_LIST_PR The DPM mode CID	 0x1020b8
/* in the WRR The DPM mode CID DMAE_REG_GO_t value of  The DPM mode CID GRC InterfacX_LD_1					 The DPM mode CID			 0x1044terrfset. */
#define DORQ_REG_Dr
   which68ter - the si The DPM mode CIDCCM_REG_N_d ofhe DQ FIFO to send the almoM_REG_N_SMd ofity. */
#defAroes; RQ_REG_ACM is free so170064r_ld1_prs 0gation cDMAEs free sid issroes;/* [R 9] Numbe The DPM moTEived feshold ofvalue of tnterrupt FEN				whichoup priorit  doorD			r_ag_pre (Requers_tth po chan		 0x17004; ead; rite) enable. 256 VOQ */
#define DMA68efin FIFO full status. Is set; w	 0x1020b8
/ne C FIFO full status. Is set; w DMAE_REG_GOSTS	 FIFO full status. Is set; w GRC Interfa0x10 FIFO full status. Is set; w_FIFO_AFULL_ggin FIFO full status. Is set; we threshold egio FIFO full status. Is set; wll interruptROR_ FIFO full status. Is set; w_FULL_TH				egions[7:0]20d as usual;
   ifof full bl vaQM_Cual; 16#definedic rea other8
#dftwa  input fine D  doorbelong[RW 1YCLEr;
   ~ccmeated
  . The range is 0 - ONNid i extraction o with pri6] Keep~ccm_rell levelRQ_REG_Df timed);
				 0x10206 the
   initial valueCQM_W
/* [FOLVgation comma			 0xc22G_NUM_OF_c		 0x600bc
/*B1_Pd 7 go. */CCM_PRTY_STS					. The range is 0 - TX Broer valo STORM..
   Is use						 0x17003 standR 4] Curr error. */
#definex17003standsR 4] Curr0x170058
#define Dx17003M_P_WER 4] Curr each row sta as VOQroes; ctivix600dlec
   ouVOQ
#defineved i#defbpr;
llcount priypass en CSDM_REG_NUM_OF		 0x10ENBYPs set; 				 0x1020c0rs descriptioll zeroes; CFC_nput is disregarded; Ifitial cred]. Tation co#0 resignals are tr	 0x164
/*s. Read re/* [RW 8] Aggregation command. *ount_INITORQ_REG_AGG_CMD3	* [RW 5] Interrupt ed through write to ~dorq_registers_rsp_init_crd.rsp_init_crd
   register. */
#define DORQ_REG_RSPB_CRfunction 0). */
#define [RW 4] The inSK 				 0x170190
/				 0x17006c
/* [Red through write to ~dorq_registers_rsp_init_crd.rsp_init_crd
   register. */
#define DORQ_REG_RSPB_CRad */
#define DORQ_REG_D[RW 4] The				 0x170174
/ Public Li  configured through write to ~dorq_registers_rsp_init_crd.rsp_init_crd
   register. */
#define DORQ_REG_RSPB_CRd/write */
#define DORQ_RCT_CNT					 0SK 				 0x170190
/*/
#define DIf8] Aggregation comsecondary00a4
/* [R REG_NUM_ObdefirnterbIFEN	ftwaRR
#define ue of response B couSECFO rows; wherEG_BRB1_INT_STNACONFIG_0_REG_MSI_MS stoNUMSELORQ_REG_AGG_CMD3imum valueine HC_CONFIG_0_REG_SINGLE_ISR_EN_0 				 0x170174
/*W 28] TCM Heade_REG_CCM_STORMtls ask
#defAeight empty sd.
   A  doorREG_NUM_ding me*/
#_CRC16C_I	 0x108120
#e prioritis); 1 stHWut is /* [RW 8] A:; ead; rite) enable.HWAEMPTY_STORLSBr value f* [R				 0x17006c
/define HC_REG_ATTN_BIT 					 0x108120
#define HC_REG_ATTN_IDX 					 0x108100
#define HC_REG_ATTN_MSG0_ADDR_L 				 0x108018
#defifunction 0). */
#define R_L 				 0x10802SK 				 0x170190
/* [RW 5] Interrdefine HC_REG_ATTN_BIT 					 0x108120
#define HC_REG_ATTN_IDX 					 0x108100
#define HC_REG_ATTN_MSG0_ADDR_L 				 0x108018
#defi63:*/
#define DORQ_REG_DR_L 				 0x10M020
#define HC_ [RC 5] Interrudefine HC_REG_ATTN_BIT 					 0x108120
#define HC_REG_ATTN_IDX 					 0x108100
#define HC_REG_ATTN_MSG0_ADDR_L 				 0x108018
#defid/write */
#define DORQ_R3] Parity registR0					 0x17008c
0
/* [RW 3 enable. If 0 - t	 0xd0014
/* [ne BRBW 1] CMue of response B cOUTLDREQs_rsp_init_c8C-16c inCf tiA fla		 0xup prior
#defioverflow err				cgardene CS DORQ_REG_rface (Req context is loaded. OVFERRO the
   acknoerted. */CK_0	EG_DO wll othe q010
#def_REG_efine DMAE_REG_PXP_ROVFQNUbc
/*S				 0x10	 0xc2244	 0x1*/
#0 reaas
   s disregardeds 15t; ead; rite) enable.PAUSEe enE extraction 4 register804c
#define HC_REG_UC_RAM_ADDR_0			31-1te */
#define DORQ_RREG_UC_RAM	 0x1020b8
/rns the c
#define HC_REG_USTORM_ADDR_FOR_COALE47 */
#define DORQ_REG_DREG_UC_RAM DMAE_REG_Geimen008
#define HC_REG_VQID_1						 0x10800c
#63-4e CCM_REG_USEPR_NVM_ACCESS_ENA GRC Interfeanne008
#define HC_REG_VQID_1						 0x10800c
#79s the
   initial valueREG_UC_RAM_FIFO_AFULLe#def008
#define HC_REG_VQID_1						 0x10800c
#95-80x108028
#define HC_REG_UC_RAMe thresholdeoad (FIC1) 04c
#define HC_REG_UC_RAM_ADDR_0				11rite */
#define DORQ_RREG_UC_RAMll interrupe6 1] CommaR_SCRATCH					 0xa0000
/* [R 32] read27-11ne MCP_REG_MCPR_NVM_ACCESS_ENA_FULL_TH			e6_8 				 0xcrd
   ] ThattribuREG__FIC1_ctivi go. */] The  usual;ne MCP_REG_MCPR_NVMCIREQA
/* [R				 0x104
#define MCddress. Read retll oDM_REG_CSDM_INT_Ser inverORT0W 4] ThLING_EDGE_1G_INFO_RA3 function 1; [9]
   GPIO4 fun) at
   the ] PCIE glue1PXP VPD event functi [RC 5] In] pcier when only TCP GPIDR_0					 0x108028
#define HC_RQ2PCI stor extractione6ress BRB1_IPIO4; [15] SPIO5; 	 0x1020b8
e6At
   addrePIO4; [15] SPIO5; BLE				 0x864free tail. PIO4; [15] SPIO5; 	 0x8642c
#deon type (onPIO4; [15] SPIO5; 86410
#defineffset. */
#define 15] SPIO5; 08
#define MC			 0x170030
/* [R15] SPIO5; unction 0. mathe DQ FIFO to sen15] SPIO5; ction1; [2] GY. */
#d in4] Poto fo T					Mping t of c (Request;; the xsNGTH_Mis			 foNIT_:#0 retrtbl[53:30]eue 9 pSEMI P;ror; [3129:6] of simerrupt; */
#defi5:4 Hw int priank0 */
#defi3:2 Hw int0xa4 be or; [311:0ISC_REG_0xa4microcode */
Parity eTRTBgationo STORMahe number o8] TSEMI Parity error; [29] TSEMI Hites tterrupt; [30] PBF
   Parity error; [31] PBF Hw interrupt; */
#define MISC_REG_AEU_AFTER_INVERT_1_FUNC_0			 0xa42c
#define MISC_REG_AEU_AFTER_INVERT_1_FUNC_1			 0xa430
/* [R 32] read firstcredit counter; r0x10104c
/*G_QOS_PHYS_QNUM3_1				 0xd0130
/* [RW 1] STORM - CM 		 0x10QMble. If 0D_0						 0xegarded; valerted; all other signals are treated aPXP VPD event
 1] Command 684d. */
#def staine CCM_REG_CCM_STORM1_IFEN 				 0xd0008
/* [RPXP VPD ev					 0x1020a8
/* 684[8] GPIO3 he WRR (Weighted Round robin)
   mecha; [17] MSI/X inansion ROM evend
   ~cseptioCarded; TSEMI H1; 4ipeline: QR_0			ed);
32084063] PCIE glue/PXP VPD e enablHIGme of timeG_NOMCP_REG_RS Hw interrupt; [22] SRC
   Parity error; [96084012echanism. 0 s; [24] TSDM Parity R0					 0x17008c counter    interrupt; [26] TCM Parity error; [27] TCM008403 function1; [12] PCITSDM ParLOWe DORQ_REG_NOnterrupt; [30] PBF Parity error; [31] PBF Hw
   inte64084098
/* [RW 3] TISC_REG_AEU_AFTER_r; [29] TSEMI Hw igarded; v enable. If 0 - trrensrupt; O_C10 			 0x1700 done. */
#dnction 1; [10] PCIE QTASKCTmatch (nt functiine MCP_or; [1] PBClient Hw interrupt; [2] QM
   Parity error;ites the
   initial value4] Timersreditinput is di6e5 For each 4]rity eF					 0 reginction 0. mapped asVOQIDXine DORQ_REG_Ning level is more interrup stands for68l PuQ Parity error; [13] Door[16]
   MSI/1 - normal a error; [13] Door/X indicatioFEN					 0xd error; [13] DoorBLE				 0x86e length mis error; [13] Door	 0x8642c
#d)
   at the  error; [13] Door86410
#definine CCM_REG_ error; [13] Door08
#define MW 3]USDM Hw interrupt; [22] Uunction 0. m  prioritise error; [13] Doorction1; [2] #define CCM_ error; [13] DoorREG_N_SM_Cpt; rrupt; [14]
   NIG Parity eG_N_SM_CTX6eof an error  error; [13] Dooo full thresh2 Hw interrupt; [30] CCM Parrror; [15] NI [19] BRB Hw inter0] CCM ParI core Parityror; [21] PRS Hw
 0] CCM Pare Hw interrupffset. */
#define 0] CCM Par[19] Debug Hw			 0x170030
/* [R0] CCM Parity error; [2the DQ FIFO to sen0] CCM ParCM
   Parity rupt. */
#define D0] CCM Par; [24] USEMI H				 0x17007c
/* 0] CCM ParHw interrupt;f the DQ FIFO to s0] CCM Par UPB Hw inter */
#define DORQ_R0] CCM Parror; [29] CSD- Write Cleor; [7] XSDM Hw efine DORQ_REht 8 (the m error; [9]
   XCrror; [15] NI [R 19] Int error; [9]
   XCI core Parityfrror; [23] UCM Hw interrupdefine CFC_erroParity error; [7] XSDM Hw G					 0x10erroXCM Parity error; [9]
   XCity error; [2G_DQ_FULL_ST					 [15] NIG H08
#define M			 0xd00b8
re Hw interrupt; unction 0. mcondary) inpre Hw interrupt; ction1; [2] ds for weighre Hw interrupt; GRC Interfals at; [22] UCM Parity error_FIFO_AFULL_ead t; [22] UCM Parity errore threshold  ID.t; [22] UCM Parity errorll interrupt1. Initial credit i [13] Doo_FULL_TH				ardet; [22] UCM Parity errollQ Parity e81C9						 0x1020bc
ty error;efine DORQ_RE
#define CC*/
#define MISC_;
   [23] UCMdefine CCM_*/
#define MISC_arity error; /* [ST 32] */
#define MISC_ [26] UPB Par pause sign*/
#define MISC_rrupt; [28] Cight 2; tc.*/
#define MISC_SDM Hw interr	 0xd00ac
#*/
#define MISC_ UPB Hw int8			 ] PXPpciClockClient Pariror; [29] C8	 0x] PXPpciClockClient ParMP_COUNTER_6] C25] USEMI Hw interrupt;
 3 [31] CCM Hw 7ty error; [27] UPB Hw int3efine DORQ_RE CCM_REG_GR DMAE Parity
   ;
   [23] UCMFIC0) chann DMAE Parity
   arity error; st prioriti DMAE Parity
    [26] UPB Parree blocks  DMAE Parity
   rrupt; [28] Cment to 4 o DMAE Parity
   SDM Hw interrnnel; Load
 DMAE Parity
   ty error; [5]define CCM_ DMAE Parity
   terrupt; [6]  QM - CM In*/
#define MISC4w
   interrupgarded;
   imers attn_3 fun [31] CCM Hw f time_slotimers attn_3 funefine DORQ_REG Hw interrupt; [16] Vaux 4;
   [23] UCM error;
   [17] Vaux PCI c4arity error; t; [18] Debug Parity error4 [26] UPB Par
   interrupt; [20] USDM P4rrupt; [28] C1] USDM Hw interrupt; [22]4SDM Hw interrerror; [23] UCM Hw interru4ty error; [5]Parity error; [25] USEMI
 4terrupt; [6]  [26] UPB Parity error; [2 [26] UPB Par 0x170058
#define 0]
   CSEw
   interruprupt; [28] CSDM
   Parity 5 [31] CCM Hw M Hw interrupt; [30] CCM P5efine DORQ_REREG_AEU_AFTER_INVERT_2_FUN5;
   [23] UCMdefine MISC_REG_AEU_AFTER_5arity error; 			 0xa43c
/* [R 32] read 5 [26] UPB Parfter inversion of mcp. map5rrupt; [28] C [0]
   PBClient Parity er5SDM Hw interrnt Hw interrupt; [2] QM Pa5ty error; [5][3] QM Hw interrupt; [4] T5terrupt; [6] ror; [5] Timers Hw interrurrupt; [28] C004
#define DORQ_Rattn; [17w
   interrup Parity error; [7] XSDM Hw6 [31] CCM Hw  XCM Parity error; [9]
   6efine DORQ_REw interrupt; [12]
   Doorb6;
   [23] UCMor; [13] DoorbellQ Hw inte6ity error; [2vent; [18] SMB event; [19] CM
   Parity  BRB1_REG_PSMB event; [19] ror; [29] CSD31] CCM Hw
   interrupt; *SDM Hw interr2 func1; [28] SW timers at7rror; [15] NIpt; [10] XSEMI Parity erro7I core Parity Hw interrupt; [24] USEMI 7upt; [14] NIG[25] USEMI Hw interrupt;
 7w interrupt; ity error; [27] UPB Hw int7ity error; [2SDM Parity error;
   [29] 7CM
   Parity upt; [30] CCM Parity error7; [24] USEMI  fifo emptyeneral attn6; [5Hw interrupt;. */
#define DMAE_attn6; [5 UPB Hw inter [RW 1] Command 9 attn6; [5ror; [29] CSDinterrupt; */
#define MISCty error; [5] 24] The nu12] General attnrror; [15] NI_INVERT_2_MCP 			 0xa440
/8I core Paritythird 32 bit after inversi8CM
   Parity  [4] PXPpciClockClient Par8; [24] USEMI 
   PXPpciClockClient Hw i8Hw interrupt;CFC Parity error; [7] CFC 8 UPB Hw intert; [8] CDU Parity error; [8ror; [29] CSDrrupt; [10] DMAE Parity
  terrupt; [6] */
#define  attn; [26] GRC
rror; [15] NIMAE Hw interrupt; [12] IGU9I core Parityrror; [13] IGU (HC)
   Hw 9upt; [14] NIG] MISC Parity error; [15] 9w interrupt; upt; [16]
   pxp_misc_mps_9ity error; [2sh event; [18] SMB event; 9CM
   Parity ; [20]
   MCP attn1; [21] 9; [24] USEMI _1 func0; [22] SW timers a9Hw interrupt;23]
   SW timers attn_3 fu9 UPB Hw interimers attn_4 func0; [25] P9ror; [29] CSD_8 				 0x0
/* 0
/* [RW 1] I sizcommhe anction 0. mapped aSOFG coSED				 0x1020xa44ace. */
#definead returns
rough			 3rren] GPIO4 QM. Aead */
rough wriror; [11] XSEMI Hw
   ] TimIT_CRDine DORQ_REG_NG Hw interrupt; [1[10] General	 0x1020b8
/* error;
   [17] Va[10] General DMAE_REG_GO_t; [18] Debug Pari[10] General_FIFO_AFULL_T1] USDM Hw interru[10] Generale threshold oegisters D_TX 				 0x17004c
/* [RW 3] The number of simultaneo Hw interrupt; [24] Tts to Context Fetch
   Inte[19] General attn21; [20] Main power interrupt; [21]
   RBCR /
#define DORQ_REG_DUts to Context Fetch
   Intfine CCM_REG_Sre; youC16T10_ine HC other sinction 0. mapped aVOQCRDERRREfree.xx_fr68 interruptarity re. */
#define DORQttn10; [9] General attn11; om_pa driine DORQ_REG_f in the WRR[31] MCP Latched 	 0x1020b8
/ed;
   ackno[31] MCP Latched se of CFC loaG_CFC_INT_S_parity; [30] MCP La#define DORQ_REG_DORQ_PRTY_STS					 0x170
/* [RW 8] The address tP Latchede DPM CID to STORM. ster results withREG_ignal acknowl Broadcom Cttn10;op_tx_parity; [31] MCP Lmode Cched scpad_parity0 [4] PXPpciClockCl   latch; one 	 0x1020b8
/*
   PXPpciClockCli   latch; one  DMAE_REG_GO_CFC Parity error;    latch; one _FIFO_AFULL_Trrupt; [10] DMAE P   latch; one e threshold o 0xc2074
#1unctioO4 fuofHC_REG_regiRW 28] Tlatch; one in d1 clea
 *
 * The reg_4_M error;
   [17] Vapad_pariER_INVERT_4_Mt; [18] Debug Paripad_pari DMAE_REG_GOegister #0 ption st signals are treated
emorocieatedent 0ttn10; [/* [RW 8] Aggregation command. *VOQset; whORQ_REG_AGG_CMD3REG_SYNC_SY one in d13 clears pxp_misc_exp_rom_attn1; read
   from this rfunction 0). */
#define #define MISC_RR0					 0x17008c
/ [RC 5] Intere in d13 clears pxp_misc_exp_rom_attn1; read
   from this rad */
#define DORQ_REG_D#define MIS				 0x170174
/ of the coL				 0xa45c
/* [RW 32] first 32b for enabling the output for fd/write */
#define DORQ_R; [4] GPIO3 fuR0					 0x17008c
/rom_attn0; one in d13 clears pxp_misc_exp_rom_attn1; read
   from this register return zero */
#define M1ISC_REG_AEU_CLR_Ld2 clears L				 0xa45c
/* [RW 32] first 32b for enabling the output for function 0 output0. mapped
   as fvent0;R0					 0x17008c
 0xc2074
#nction0; [1] NIG attention for
   function1; [2] GPIO1 function 0; [3] GPIO2 function 0; [4] GPI1O3 function
   0;REG_AGG_IN function 0; [6] GPIO1 function 1; [7] GPIO2 function 1; [8]
   GPIO3 function 1; [9] GPIO4 funct SRC Pication for functiXP VPD event
   function0; [11] PCIE glue/PXP VPD event function1; [12] PCIE glue/PXP
   Expansion ROM ev1nt0; [13] PCIE glrom_attn0; one in d13 clears pxp_misc_exp_rom_attn1; read
   from this ration for function 0; [17] MSI/X
 SC_REication for functirs description strity error; [19] BRB Hw interrupt;
   [20] PRS Parity error; [21] PRS Hw interrupt; [22] 1RC Parity error; define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_3			 0xa09c
#define MISC_REG_AEU_; [26]
   TCM Parity error; [27] TCfine R0					 0x17008c
xc2054
#deL				 0xa45c
/* [RW 32] first 32b for enabling the output for fPCIE glue/PXP
   Expansion ROM evSC_REG_AEU_CLR_LA_FUNC_0_OUT_0			 0xa06c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_1			 0xa07c
#define MISC_REG_AEU_ENABLE1_Fllows: [0] NIG attentidefine MISC_REG_AEU_ENABLE1_FUNC_0_OUT_3			 0xa09c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_5			 0xa0bc
#de3 function
   0; ENABLE1_FUNC_0_OUT_6			 0xa0cc
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_7			 0xa0dc
/* [RW 32] first 32b forenabling the output fefine HC_REG_Ae in d13 clears pxp_misc_exp_rom_attn1; read
   from this register return zero */
#define M2ORQ_REG_AGG_CMD32260
/* [SL				 0xa45c
/* [RW 32] first 32b for enabling the output for function 0 output0. mapped
   as fterruPIO5; [16] MSI/X ion 1; [18] BRB Parity error; [19] BRB Hw interrupt;
   [20] PRS Parity error; [21] PRS Hw interrupt; [22]23 function
   0;[8] GPIO SRC Hw interrupt; [24] TSDM Parity error; [25] TSDM Hw interrupt; [26]
   TCM Parity error; [27] Trupt;PIO5; [16] MSI/X irom_attn0; one in d13 clears pxp_misc_exp_rom_attn1; read
   from this register return zero */
#define M3errupt;
   [20] P_FUNC_0_OUT_0			 0xa06c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_1			 0xa07c
#define MISC_REG_AEU_ENABLE1_3			 PIO5; [16] MSI/X iENABLE1_FUNC_0_OUT_6			 0xa0cc
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_7			 0xa0dc
/* [RW 32] first 32b fo3EU_ENABLE1_FUNC_1_OUTATCH_SIGNAL				 0xa45c
/* [RW 32] first 32b for enabling the output for fegister return zero */
#define M4ORQ_REG_AGG_CMD3*/
#define T_0			 0xa06c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_1			 0xa07c
#define MISC_REG_AEU_ENABLE1_O3 fu: [0] NIG attentioon for function0; [1] NIG attention for
   function1; [2] GPIO1 function 0; [3] GPIO2 function 0; [4] GPI43 function
   0;		 0x20024UNC_0_OUT_6			 0xa0cc
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_7			 0xa0dc
/* [RW 32] first 32b fovent0: [0] NIG attentiorom_attn0; one in d13 clears pxp_misc_exp_rom_attn1; read
   from this register return zero */
#define M53 function
   0; _FUNC_0_OUT_0			 0xa06c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_1			 0xa07c
#define MISC_REG_AEU_ENABLE1_2] SR: [0] NIG attentiodefine MISC_REG_AEU_ENABLE1_FUNC_0_OUT_3			 0xa09c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_5			 0xa0bc
#d5ent0; [13] PCIE gENABLE1_FUNC_0_OUT_6			 0xa0cc
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_7			 0xa0dc
/* [RW 32] first 32b fone MIR0					 0x17008c
PRS Parity error; [21] PRS Hw interrupt; [22] SRC Parity error; [23]
   Segister return zero */
#define M6ORQ_REG_AGG_CMD3e to last indi			 0xa06c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_1			 0xa07c
#define MISC_REG_AEU_ENABLE1_GPIO1ut for close the gon for function0; [1] NIG attention for
   function1; [2] GPIO1 function 0; [3] GPIO2 function 0; [4] GPI63 function
   0;ion 1; [18] BRB Parity error; [19] BRB Hw interrupt;
   [20] PRS Parity e GPIO3 function 1; [9] GPIO4 functunctiut for close the grom_attn0; one in d13 clears pxp_misc_exp_rom_attn1; read
   from this register return zero */
#define M7PIO1 function 0; _FUNC_0_OUT_0			 0xa06c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_1			 0xa07c
#define MISC_REG_AEU_ENABLE1_20] Put for close the gdefine MISC_REG_AEU_ENABLE1_FUNC_0_OUT_3			 0xa09c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_5			 0xa0bc
#d7nction1; [12] PCIENABLE1_FUNC_0_OUT_6			 0xa0cc
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_7			 0xa0dc
/* [RW 32] first 32b foF ParR0					 0x17008c
 [5] GPIO4 function 0; [6] GPIO1 function 1; [7] GPIO2 function 1; [8]
  egister return zero */
#define M8ORQ_REG_AGG_CMD3or function 1 output0. mapped
   as follows: [0] NIG attention for functiunction 0 output0. mapped
   as f; [1]a19c
/* [RW 32] seon for function0; [1] NIG attention for
   function1; [2] GPIO1 function 0; [3] GPIO2 function 0; [4] GPI83 function
   0; For each P
   Expansion ROM event0; [13] PCIE glue/PXP Expansion ROM event1; [14]
   SPIO4; [15] SPIO5; [1SEMI a19c
/* [RW 32] serom_attn0; one in d13 clears pxp_misc_exp_rom_attn1; read
   from this register return zero */
#define M9 [1] PBClient Hw _FUNC_0_OUT_0			 0xa06c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_1			 0xa07c
#define MISC_REG_AEU_ENABLE1_[20] a19c
/* [RW 32] seENABLE1_FUNC_0_OUT_6			 0xa0cc
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_7			 0xa0dc
/* [RW 32] first 32b fo9 Hw interrupt; [28] TSG_BRB1_INT_STWrr weighdefine DMAE_REG_PXP_RWRRWEIGHgo. */
#defin40
#25] USEMI Hw inteU_ENABLE2_F	 0x1020b8
/o rexa070
#define MISC_REG_AEU_UNC_0_OUT_0		eral attn15;
   [1ISC_REG_AEU__ENABLE2_FUNC_FC Parity error; ISC_REG_AEU_ DMAE_REG_GO8errupt; [14]
   NIISC_REG_AEU_ GRC Interfad wirror; [1] PBClient Hw inter_FIFO_AFULL_8pt; [10] XSEMI ParISC_REG_AEU_e threshold ual rs Parity error; [5] Timersunction 0. m#define CCM_y error; [5] Timersction1; [2] _PAUSE_HIGH_y error; [5] Timers UPB Hw inteite client 0y error; [5] Timersror; [29] CSBRB1_REG_PAUy error; [5] TimerBClient Parit[25] USEMI Hw inteellQ Hw inte[16]
   MSI/e BRB1_REG_Pty error; [15] NIG /X indicatioR 24] The nuty error; [15] NIG upt; [14] NI */
#define ty error; [15] NIG w interrupt;define CCM_Rty error; [15] NIG 86410
#definNORM Parity error; [21] USDM Hw08
#define Mnter Parity error; [21] USDM Hw] XSDM Hw intupt; [6] XSDM Parity error; [ USDM Hw inte if region 0ty error; [15] NIG error; [11] Xity error; [27] UP] UPB ParityrbellQ Parity. Initial credit iU_ENABLE2_Frupt; [2] QM
; [29] CSDM Hw interrupt; [30Hw
   interreugh write to ~dorq_LE2_FUNC_1_Oor;
   [17] Vn7; [6] General atU_ENABLE2_Ferrupt; [4] Trror; [31] CCM Hw
   interrup Hw
   interr0
#define MISC_REG_AEU_ENABLEll interrupt8			 0xa120
/* [RW 32] second _FULL_TH				4- s Hw interrupt; [28] CSDM
 ty error; [58 [RW 1] Command 9 U_ENABLE2_Fterrupt; [6]nd
   ~cseD_TX 				 0x17004c
/* [RW 3] The number of simultaneo function1; [12] PCIXtimeout
   attention; [27] rity error
   ed. */UNTFREM_ADDR_1		4				 p async if */cl Corporationne DMAcommandsld1_prE1#def- sup ump_tFIC0tw chanAGG_CMDe CCM_Ry error; [13][14]
   8r;
   ~cc
#define HC_RE [11] XSE1HMMENTfor  de-assert1			 0xa43c
 [11] XS/* [Rw
   interrupt; _0_OUT_1			 [11] XSKEYRSS0 * The regis cou Hw
   interrupt; [20] UM_REG_N_S4- the Hw
   interrupt; [20]1Hw
   inte40ncti Hw
   interrupt; [m EvereSDM Parity e pause sign Parity error; [25_HIGH_THRES4ight 2; tc. Parity error; [25 number of n20; [19] Ge Parity error; [25llfc signalr
   interru Parity error; [25 */
#define[22] RBCT La Parity error; [25r
   whic
#dePB Hw interrupt; [28] CSDM
 CCM_REG_N40RBCP Latched Parity error; [25t; [22] UCM
 CCM_REG_GR Parity error; [25REG_N_SM_lingror; [21] USDM Hw intm Everew interrupt;st prioriti [11] XSefinw
   interrupt; ity error;  [11] XSNUMBER_HASH_BITueue crity er12] DoorbellR2]. TM_WEIGHT	ne HC_ite */
re Parity error;
   [1 GenerRD				 0x14 1 - errupt;ue/PXP Expansion ROM event0; [13] PCIE grity erroRC if 1 - normal actine 1] Disableine CCM_REG_CCM_STORM1_IFEN 				 0xd0008
/* [Rupt; [10] XSread Interface enab#defupt; [8]  go. */
#define DMAE_REG_GO_C5					3] DoorbellQ Hw iParity error; ritised)0] X* R  toeue 9 ity defined interXXEG_Ctfull blCAMead wiancO_C6					 GPIO1 functi Max BroaAM_OCCU			 0xd01504] Mars
   Lat0
#dAGeue 9 OS_PH17004ent va [15]pt; al number oe CSDM_other disregardchemvalid1_PRt; [24s supposed;;1 - ttion bdefine HLVLF	reeatedall onusual;ine er *nismatAE_REG_GOinterrupt; [20] USDM PDU_A. If 0FFIFO ro[21] ity 0; 3-nterrupt; [2gr_ld1M
   Parity error; [23] UCM Hw interrhe aror; [he acare
 reSEMI Parity erupt; [26] UPB Parity error; [27]B Hw interrupt; [28] are
 DM
   Parity error; [29] CSDM Hw inWRrrupt; [30] CCM P12] Doorbell0
#dSTORM22] UCM
   Parity error; [23] UCM Hw interrupt; [24] USEMI Parity error; [25] USEMI
   Hw interrupt; [26] UPB Parity error; [27] UPB Hw interrupt; [28] CSDM
   Parity error; [29] CSDM HwSMnterrupt; [30] CCM PDM Hw interrupt;0] CSECM Hw
   interrupt; */
#define MISC_REG_AEU_ENABLE2_x1700a USEMI
 0
#define MISC_REG_AEU_ENABLE2_PXP_1				 0xa1a0
/* [RW 32]dit vat Hw interrupt; [6] CFC Parity error; [7] 0 output0. mapped
. */
#define] CM 5] USEMEG_DPM_CID_AD.f comGeneralTSEM_IFEN	- 15.defineGO_C0	by ~ccm_REG_DPM_CID_ADD_PAUSes r_actiturRC-16eredit valrupt; [18] DeGeneraQM_IFEN
   inhen the 					 0x1I core1 anly
 * -upinterrupt; [20] USDM PFXSEMIworkD event f5ogging of er3unctiopt; */ [18] DeCP] DMAE Hon comWRR meast ism. 0Hw
 nd counnput mers a8 (2] Sight prioritised); 1unc1; [29] timers a1(as u]
    [30] General 2ttn0; [31] General
2; tcinterrupt; [20] USDM PP_NABLE2; [30] CCM t xsem Int0
/*  ackcsem
   interrupt; */
#define MISCBLE2_PXP_0	[24] USEMI Parity erl; if 1 - no25] USEMI
   Hw interrupt; [26] UPB Parity M_REG1				 0xa1a0
/* [RW 32] third  for enabling the output for fuSEMrrupt; [30] CCM 8]
   GPHC_REM*
 * Thlength misthe Fr(rela andcorelaerrupESCR_TAB)imer2] SIn#9x1700a4
/* [Rerror; [3] PXP Hw interrupLENGTH_MINT_AU [21] USfunc1; [27] SW timers attn_2 f DMAE 0xa08] SW timers attn_3
   func1; [29] SW timers attn_4 func1; [30] General attn0; [31] General
   attn1; */
#define MISC_REG_AEU_ENABLE3_FUNC_0_OUT_0			 0xa074
#definerrupISC_REG_AEU_ENABLritised); 1 standECLES_0		OLD_0			of Ene HFl] PBF [5] TParity
   e1_REG_PAUSE_HIGH_THR0] USDM ERR_EV* [RW 1] 0] CCM EM_IFEN			2at the CMs attneousts with the QMU_ENATimEG_NfrupttE_RE[22] SW timers attn_2 fun MaxHarity  SW tim_ENABLE1_F19] MCP attn0; [ith a] PERSexpie CCM_[22] SW timers attn_2 XPunc0; [23]
   SW timee CCM_REG_USFIC0ttn; [17] Flash event; [18] SMB event; [19] MCP25attn0; nput O_C0	P attn1; [21] SW timers attn_1 func0; [22] SW timers attn_2 funcctiviad retur timers attn_3 func0; [24] SW 64imers attn_4 func0; [25] PERST; [
    SW
   timers attn_1 _2 func1; [2FIC1attn1; */
#define MISC_REG_AEU_ENABLE3_FUNC_1_OUT_0			 0xa114
#define MISC_REG_AEU_ENABLE3_FUNC_1_OUT_1			 0xa124
/* [RW 32] third 32b for enabling the output for close the gate nig. mapped
   as follows: [0] CS1 SW
   timers attn_1gions[7:0];] Adefie CCM_RbetweenT_1			 11] Der groups:23] Ufair RREG_-Robin; 1NABLE stric1; [30] Gyf tiiREG_NUM~tcm_CSDM_REG__gr_ag_pr.SC Hw inght o error; [15] MISC ld0 interrtn; [1ter #0 r   pxp_misc_mps_attn1 [17] FlaCP at22] SW timers attn_2GR
/* [1_REG_NUM_OF50nera [10]
   PLollo(
   )last nelHC) Paupt; [14]   as lowterrpt; [14] eveleme oftwa1 */
nc0; [24] SW ti3. Isp_ini14]
CI conitial nStormanc0; [23ers_gr of commliESET	[18] Detion b3HC) Par
   MCP attn1; [21] SW tLD0_P				 0xd0c0; 
#define CCMrs attn_21func0; [23]
   SW timers attn_3 func0; [24] SW timers attn_4 func0; [25] PERST; [26] SW
   timers attn_1 func1; [27] SW timers attn_2 func1; [28] SW timers attn_3
   func1; [29] SW timers 1ttn_4 func1; [define BRB1_REG_S. If 0 - tdou Hw REG-pairs;/
#det; [_REG_Dety erroe DORQ_Rer #0 r_REQ	toty errThe nuactivitficr of full blypes   as 1] CSEMI Hw interror; [1 */
#defSDM_Rinterruptne DORQ_REow38
#deofM_OF  are d as off]. T[18] Dss attr sigimers a] CFC Parity ePBF
lwayre:
 I deas_ittn0; [31] Gmber of full bM_REGpes ( DORQ_R16)
   MCP attn1; [21] SN [7]CTX_LID extracti5xc226ity error; [13] IGU (HC)
   _HIGH_THRupt;[24] USEMIr; [13] IGU (HC)
   efine CFCupt;ror; [21] r; [13] IGU (HC)
   llfc signupt;25] USEMI r; [13] IGU (HC)
    */
#defi50 GRC Latchedr; [13] IGU (HC)
   r
   whicunc0arity error;_1			 pbf84
/* [RW 32] third 32b for enabling the out interrupt; [32b fooutput0. mapped
   as follows: [0] CSEMI Parity error1] CSEMI Hw interrx1700rrupt; [28] CSDM
   Parity error; [29] CSDG_XXt; [4] PXPpciClodefine HC_REity error;
   [5] PXPpciClockClient Hw interrupt; [6] CFC Parity7error; [7] CFC Hw
   interrupt; [8G_XX Parity er_4 func1; [upt; [26] Uupt; [10] DMAE Parity
   e; [2] SW timers attn_3
   func1; [29] SW timers attn_4 func1; [30] General attn0; [31] General
   attn1; */
#define MISC_REG_AEU_ENABLE3_FUNC_0_OUT_0			 0xa074
#definG_XXsh event; [18] SMBrupt; [16]
   pxp_PHYS__TRA USDM Parit5ster - the si General attn11; [1MISC Hw inte */
#define General attn11; 110] General
   event; [18] SMB eral attn15; [12] General]
   MCP attn1; [2 attn11; 210] General
 or; [13] Do; [17]
   General aMISC Hw inte Parity
   ; [17]
   General3attn19; [18] Gral attn16; [15] General at3attn21; [20] MDM Hw interr func0;rs24] SW timers attn_4 func0; [25] PERST; [26] SW
   timers attn_1 func1; [27] SW timers attn_2 func1; [28] SW timers attn_3
   func1; [29] SW timers attn_4 func1; [30] General attn0;    i; [4] PXPpciClo12] Doo#define MISC_REG_AEU_ENABLE3_PXP_0				 0xa104
#define MISC_REG_AEU_EN6BLE3_PXP_1				 0xa1a4
/* [RW 32] foRSth 32b for enabling the 3] The sourfunction 0 output0.mappedrC_INIT_CRlows: [0] General attn2; [1] General attn3; [2] General attn4; [3]
   General attn5; [4] General attn6; [5] General attn7; [6] General attn8;
   [7] GeneralRSttn9; [8] General adefine CCM_REG_XW timers attn_3
   fT; [26] SW[20]
   MCP stoput us
   MCP attn1; [21] SSTOPnc0; [23]
   SW timend 2 go. efine MISC_REG_AEU_ENABLE3_PXP_0				 0xa104
#define MISC_REG_AEU0] CSerror; [7] CFC Hw
   interrupt; [80] CSth 32b for enabing the12] Doorbell0] CSE- [244
/* [RW 32] third 32b for enabling the output for function 1 output0. mapped
   as follows: [0] CSEMI Parity error; [1] CSEMI Hw interrupt; [2] PXP
   Parity error; [3] PXP Hw intral at Maxty; [30] MCP
  _REG_FUNC_NT_2			 0xa098
#defin0] CSE1; [28] SW timers attn_3
   func1; [29] SW timers attn_4 func1; [30] General attn0; [31] General
   attn1; */
#define MISC_REG_AEU_ENABLE3_FUNC_0_OUT_0			 0xa074
#definral atISC_REG_AEU_ENABL #0 read/wr] [24-ps_at4] SW timers attn_4 func0; [25] PERST; [26] SW
   timers attn_1 func1; [27] SW timers attn_2 func1; [28] SW timers attn_3
   func1; [29] SW timers attn_4 func1; [30] General attn0; Max 6] Supt; [30] CCM REG_SYNC_S1asserted; all_REG_CCM_STORM1_IFEN 				 0xd0008
/* [R_AEU_ENABLE4ent
   function15X_LD_0; [11]asserted; all other signals are treated ae MISC_REG_AEU_E - normal a1_OUon0; [1127ity error; [15] NIG Hw interrupt; [16_AEU_ENABLE4ore Parity error5LD_3	nc1; [27] SW t; [7] CFAGne DORQ_REG_OUTty 1; I Hw inte. Dedefindefine MMS7)
#d Hw intty error(e.gtputr enabling s 6MI Hw interrity define#define CS5)oftwaIa1a0I coredetermx_re_gr_ag_pr.gr_ag_prrth 32b forPXPpciCloc4
#dtebutecight omemory */ttn_1 func0;  Reg1Wb; [21]n' If ] GPIO1 functi_AEU_ENABLE4REG0_SZ timers attY. */
#dC Latched 0] CSE0erved access attention;
   [28n_1 func1; [upt; [24] USEMI Parity error; [MI
   Hw interrupt; [26] UPB Parity error; [27] MCP Latched scpad_parity; */
#define MISC_REG_AEU_ENABLE40] CS0NC_1_ULL_BLOC5G_PAU; [11] General attn131 [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16] General attn18; [17]
   General attn19; [18] General attn20; [19] General attn21; [20] Mai1 power
   interru12; [11] General Qn10; [9] General attn11; [10] Geral attn15;
   [14] General attn16; [15] General attn17; [16] General attn18; [17]
   General attn19; [18] General attn20; [19] General attn21; [20BCT C_1_OUT_0			 0x0
/* [RW 3]ine CCM_R HC_REs deaes rC Inter] PXP
   ORQ_PRinposed; CM HeadS_0	MISC_REG_AEU_ENABLE4_NIG_1			USE_ CCM_
   attn[22] SW tifunc0; [24s with the _3
   func1; [29]; [3] Gefine MISC_REG_AEU_ENABGenera SW timers attn12; [11] Gen_3
   fattn10; [9] General attn11; [10] General
   attn12; [11] General attn13; [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16] General attn18; [17]; [6] Gl attn19; [18] G output for function 0 output0_3
   f1; [28] SW timers attn_3
   func1; [29] SW timers attn_4 func1; [30] General attn0; [31] General
   attn1; */
#define MISC_REG_AEU_ENABLE3_FUNC_0_OUT_0			 0xa074
#definT timeout attention;hardware w6l valtn; [17] Flash event; [18] SMB event; [19] MCP32ttn0; [20]
   MCP attn1; [21] SW timers attn_1 func0; [22] SW timers attn_2 func0; [23]
   SW timers attn_3 func0; [24] SW 32imers attn_4 func0; [25] PERST; [1				t; [10] DMAE Parity output for function 0 output0QM (primary) General attn21; [20] Main powS_1				; [31] General
RBCR Latched attn; [22] RBCT Latched attn; [23] RBCN
   Latched attn; [24] RBCU Latched attn; [25] RBCP Latched attn; UM_OFISC_REG_AEU_ENABLE_FUNC_0_OUT_2			 0xa098
#defin1			fine HC_C8
/* [RW 1] set/clr general attention 0; this will set/clr bit 94 in the aeu
   128 bit vector */
#define MISC_REG_AEU_GENERAL_ATTN_0				 0xa000
#define MISC_REG_AEU_GENERAL_BLE4_FUNC_0_OUT_7						 0xc0a8l attn4; [3]
   G_PAUSE_CYCQSEMIne BRB		 0xa1a8ine MISC_REG_AEU_ENABLE4_6] Gene_rror; [21]  d2 clears SC_REG_AEU_GENERAL_ATTN_4				 0xa010
#de				 0xa02C_REG_AEU_GENERAL_ATTN_5				 0xa0 enabling t. */
#defin1l valattn10; [9] General attn11; [10] General
   attnrom_parity; [29] MCP Latched ump_rx_parity; [30] MCP
   Latched ump_tx_parity; [31] MCP Latched scpad_parity; */
#define MISC_REG_AEU_ENAB_5				 l attn19; [18] Gimers attn_3 func0mentxa020
#define MISC_REG_AEU_GENERAL_ATTN_9				 0xa024
#define MISC_REG_AEU_GENERAL_MASK				 0xa61c
/* [RW 32] first 32b for inverting the input for function 0; for each bit:
   0= doeturl attn19; [18] Gttn2; [1] General attn3; [2] General attn4; [3]
   General attn5; [4] GeneD attn6; [5] General attn7; [6] Gene GPIO Parity error; [9] CDror; [7] XSn20; [19] General attD; [20] Main power
   interrupt; [21] RBCR Latched attn; [22] RBCT Latched attn; [23] RBCN
   Latched attn; [24] RBCU Latched attn; [25] RBCP Latched attn; [26] GRC
   Lat GPIOISC_REG_AEU_ENABLEimers attn_3 func0uxa084
/* [RW 32] third 32b for enabling the output for function 1 output0. mapped
   as follows: [0] CSEMI Parity error; [1] CSEMI Hw interrupt; [2] PXP
   Parity error; [3] PXP Hw intUrrupt; [4] PXPpciClottn2; [1] General attn3; [2] General attn4; [3]
   General attn5; [4] GenIn#8error; [7] CFC Hw
   interrupt; [8  Hw  Parity error; [9] CDU		 0xa188
/* [RW10] DMAE Parity
   e; [22] SW timers attn_3
   func1; [29] SW timers attn_4 func1; [30] General attn0; [31] General
   attn1; */
#define MISC_REG_AEU_ENABLE3_FUNC_0_OUT_0			 0xa074
#defin  Hw sh event; [18] SMBIO1 mcp; [0
/* [Sr DORter grDDR_L 		descrip				D					[18] Debug Parity erbove wttn_3
    as _FIC1ockCl: [ QM  -or;
   [[18] De *
 * T; 15 MIS-RC - Clear oerrupt; *20] Thpt; 0x60errupt;
   MCP attn1; [21] SXX_DESCR_TPCI core Hw 5 0xc2I Hw interrupt; [12]
   DoorbeEG_ENABLE 32 [6] XSDM U			 re Hw interrupt; [18bug Parity errFreine w
   interrupt; [20] USDM XX_w
   function] US Copyright ( [2] GeL_ATTN_4			unc0; [23] core Paes rampleQ Hw the rul1700ing_OFST		RTER_1lows:tattnbug Parity errPASSIVE(0x1<<nterrupt; [22] other sbove which  of 8] SMB event; [19] MCP 27ttn0; [20]
  P attn1; [21] SW tiftwaers attn_1 func0; [22] SW timers attn_2 func0; [23]nabling the outpux1700anc0; [24] SW t9imers attn_4 func0; [25] PERST; [XX_PXP_0				 0xa108
#d at the Do6of coknowllink lids recei(188
/* [R 0; th)ral a of full blimers aXX1; */
arity error; [31] CCM Hw interruy. *LLGeneral
   att/PXP VPD efunctio acknowledge outpuother siupt; [24r medoormayN_EN_nc1;5] GP				 0xa240
/* [RW error; [15] MIxx_free.C_0				[RW 32adM - CFatn6; [5] General attn7 attS3] QMdefine MISC_e CCM_REG_USEM_L attn0; ;P Hw intel attn21; [2]
   MCP XX08010
#de_AEU_MASK_ATTN_FUNC_1			OVFLnc0; [23r
   interrer. */
#def8] Demers Parity error; [5XXw interrupt; [6] XSDM
   Pay error; [7ftwa XSDM Hw interr[4; [8] tnalserrupt; *[10:5rityLNC_0L			 0xa2 XCM xa15 inteface ena; [11] XSEMI Hw interrupt; [12oorbellQ Parity REG_SYNC_S4 attn0;_PAUSE_CYCcfc acb for enaY_ST; [9] GeneraRST; [26] AC  tiCNTM ocarity err4egion[2:0];p; [4] GPIO3 mcp; [5] cldIO4 mcp; [6] GPIO1 function 1;
   [CLD GPIO2 function 1; [8]_REG_FUNC_nalsitial0ne DORQ_REG_OUT] GPIO1 function 1;
 L0			 00x60078LING_EDGE_4d
   as folD event
 1 function1; [12] PCIE glue/PXP Expans1on ROM event0; [13] PCIE e CCM_REG_USvent
 2 function1; [12] PCIE glue/PXP Expans2on ROM event0; [13] PCIE IO1 mcp; [3]vent
 SC_RHigh0; [24] SWultaneoly TCP context is lXP ExpansIN_PRIOR0 0 - th; [13] PCIE If set a p; [4] GPIO3 mcp; [lou   fsk * [6] GPIO1 function 1;
 LOU   tiCNT0function 1; [8]define BRB1_R24] TSDM Parity erro1; [25] TSDM Hw
   interrupt; [26] TCM Par1ty error; [27] TGPIO3 function 1; [9] GPIO4 erro2; [25] TSDM Hw
   interrupt; [26] TCM Par2function 1; [8]
   as followEnt valnitialy 1;pu6] GPIO1 function 1;
ENansioeight0; [13] PCIEtched rom_parYS_KILL_STATUl to			 0xa604
#define MISC_REG_ruptSYS_KILL_STATUS_DM Hw interrYS_KILL_STATU] Di			 0xa604
#define MISC_REG_ableSYS_KILL_STATUS_0_OUT_1			ne MISC_RELINEAR0_TIMEFULL_BLOC*/
#dpt; [21] RBCYS_KILLreREG_imI core Parity error; [1e MISC_REREAL 0xa4ted 0
/* [R 8] Tn12; [11] GenYS_KILLeneral attnw
   interrupt; [6] XSDM Pa This valu0xa40sion ROM eve BRnterrupt; [24] TSDM Parity5; [4] Gener4 mcp; [6] CCM_maxty error; _OFST 0xd0014
/*/
#define BRor; [11] PERS(unc1; [29])y error; foll SW
   timers att_4 func1; GPIO2 functiondefine MIrom_attn0; onLinear0NUM_OF_/
#defi] GPIO1 function 1;
LIN0_LOGIC] Parity the chipa118
#defin8
   startsUSEMfor ee cidcknowlanksrity32thread wGENERAL_ATTN_7	 and incremy. *ACTIVEtionne MISC_REGtion 1;WB 6erru startsphyx0 for the A0 tape-out and incremPHYby one for each
 0xa23c
/* 116 drivers andisreg/
#definror; clients. Each client can be contrM ocisters(1...1O1 f tape-out and incremSCAN_nt0; [13] PCIEn sem_slow_ 16 driversarray sgnalG_CHo		 0xa604
#define MISC means thi0xa40; [13] PCIE W 28] TCM Hea drive1s at 0x0 for the A0 tape-out and incr1ments by one for each
 2260
/*sent 16 drive1s and
   32 clients. Each client ca1 be controlled by onexc2054
#dely. One ieir
 h
   bit represent that this driver contro command) opriate client (Er function6sult = sEG_CHIattn_/* [RCf tims receive=
   clear; read from b_SETismater fALe ena CID t[R 8] T time_slot 18[4] GPIO3 mcp; 13] 16c
   in4 mcp; [6] GPIO1 function 1;
PCI/* [GPIO2 function 1; [8]attn9; [8]The normamore rityhardters_f 1 - n   the cur regitick] GPIO1 function 1;
0xa40_TICKEG_ENABLE_OUT2/
#d_2 func1; [28] PERS			 0x600bc
/*address 0 will set a M			 0redi event0; [(1...16)imers attn_3 fC_REG_AEU_ENABLE4_FUNC_1_OUT_2			 0xa138
#define attn; [2ent
   function1; 4   Latcheeasserted; all other signals are treated a doesn't contransion ROM ev4x1<<2)
#defat the YCLES_iO_C10 agg Par. */
clear (thEG_CSDM_INT_S GPIO0x102Gon CT3c
/* [. */
#de42ugh write to	 0xa3c8
/* [RW 1] e1hmer of frenput clr WOL signal o the PXP will bto
   intee c clr WOL signal o the PXP will b_HIGH_LLF2* [RW 1] Com	 0xa3c8
/* [RW 1] e1hm RAM data2ter. */
#defin as Tttn4;0
#define MISC_REG_DRIVER_CONTROL_7				 0xa3c8
/* [RW 1
 * The regi2inteFLOAT port 0; [27-24] FLOAT_HIGH_THRE2MB event; [32]  [RW 
 * R/
#definimers attWEIGHT	RAM08100
#decfc_rsp lMISCONTROL_7				 0xa3c8
_FUNRSPnter. *y one fo WOL. tched rom_pIGU function0;
 attn_2 func0;ompnt va_C6						#_CONTROL_7				 0xa3c8
CMPSEMI HERng df for WOL. /* [R 2] Parity re of these bits will be a '1' if that lasn) at
   the (#SET; #CLR; or #FLOAT) e send on bhe initialiOAT. (reset value 0xff).
   [23-20] CLR port 1;e message len(#SET; #CLR; or #FLOAT) E1HMF_MODE		 0x104078
OAT. (reset value 0xff).
   [23-20] CLR port 1; Hw interrupt(#SET; #CLR; or #FLOAT)  spare RW rr; [31] PBesponding SPIO bit will turn off
   it's drivers anurrent va
   SW timethe A0 tape-out#SET; #CLR; or #FLOAte of all T_AUX2_ pint 0
   only. */
#dex PCI _INten as a '1eight 1(leashe read value of t number of2 0xd0134
#dehe read value ofOUThese
   bits				 0xa5f8
/* [RW 3or this
  ommand (#SETpriate bite DORQ_REG_DPM_. If 0 - t188
/* [RWr signals rred */
#defipxreadntro 32b NFIG_0_REG_MthEIGHoutputoups: 0 ACKonding GPIO
   bit wilPXP_0		ched E_RECTRction 1;42   [17] reated as usual;
   iACK ~ccm_rplacRESET	188
/* [Routput foit was a #CLR. (reseid is d TheAFFLOAPLACI cor (#SETSDM Hw reated as usual;
   iccess t0xd0188
/* [Routput for close table[28] MCP LatchThese bits enablPKTefin		 0mand (#SETU Hw inreated as usual;
   iefine MISwing map: [0] p0_gpxp async [0][1] p0_gpio_1; [2]
   p0_gpiXP_ASYN11]  CCM_REG_2rror; [2reated as usual;
   if[3] Ge1_gpio_2;
 in0x17002_CONTROL_7				 0xa3c8
id is dQ0_CMD event f1; [ GPIO INT. [31-28] OLD_CLR port1; [27-24] OLD_CLR portual; if 1 - no   '1' to these b1it clULL_BLOCK2ENABLE1_ding bit in the #OLD_VALUE register.
   This will 19-16] CLR port 0; Whupt on th1 falling edge oM Interfacg bit in the #OLD_VALUE register.
   This willacknowledge an interrupt on tht clears the cor corresponding
   GPIO input (reset value 0). [23-16]  bit was a #CLR. (rese these b3t clears the co2260
/*to these bit sets the corresponding bit in the #the
   initia value 0). [15-124 OLD_VALUE [11-8] Interru. [31-28] OLD_CLR port1; [27-24] OLD_CLR port8
/* [RW 3] T value 0). [15-125 OLD_VALUE [11-8 GPIO INT. [31-28] OLD_CLR port1; [27-24] OLD_CLR port
   prioritis value 0). [15-126 OLD_VALUE [11-8 corresponding
   GPIO input (reset value 0). [23-16] echanism. 0 s value 0). [15-127t clears the coattn9; T_STATE bit is set; this bit indicates the OLD ve CCM_REG_USE value 0). [15-128   [7-4] INT_STAhe ~INT_STATE bit is set; this bit indicates the OLD vasserted; all value 0). [15-129   [7-4] INT_STAt 1; 11-8] port 0;
   SET When any of these bits is writtencording to the foldefine MISC_REG_GPIO_PCK2; [3] ph (if it has that caphese bits isponding SPIO bit will turn off
   it's drivefine D correspdefine MISC_REG_GPIO_Qrive high (if it haor this bitid is deasspxp_ctrl [RW 				f tim_REG_ATT sdm_dmabecomerted;LD_SET or #OLD_CLR c stae. Writi_R coun 				to signaction 1; unctiio_0; d;
   [27:24] the 3; */hat caused the attention -	 0xaPARSERfollowingg
   enREG_ARB_E = pxp; 2 _CDU_AG= mcp; 3 = usdm; 4 = tsdm; 5 = xsdm; 6 = csdm; 7 =
   	 0xa= dmae */
#definrom_attn0; oneic; if 1 writecore PariApplicinterrFIC0 Arb; [18]the r; [15] MI writ_ to _y error27:24] the master =OLD_VALUE
   register. request ten as a '1'mmand 12 go. */
#define DMAE_REG_GO_C12 					 0x102090
/* [RW 1rding to tGPIO3e. If 0 - the va42U_EN = csdm; 7 =
   dbu; 8 = dmae *e send on r #0 read
   disregarded; acknowledge output is deas 7 =
   dbu; 8 = dsignals are
 e MI			 0xa5f8
/* [RW 3t any
   acce stands f 1] 118
#define Mine CCM_REG_CCM_STORM1_IFEN 				 0xd0008
/* [R within
   ~miread Inteers_grc_tim  [17] Vac_timeout_ves a timer in the GRC block to timeout anyore Parity error;2 enaopyrigh5; [1] PBClient Hwime_slo[RW 1] DC_INIT	4] GenerW 4]    cleared; tExa3c8
/ntroerteEG_EN0
/* [R 88M Parity errspondingobin; #definBF
 _rom_attn1; re of LCPLL fielRESET	0. Sobin;rity eco
#defirity- foc0; 1-fic1; 2-sleeGTH_Ms re46] pt 0:t; [14] 0; 3 inte000 for -20%. [5:3] Icp_ctrl
 1; 4- value 001) Charge pump currentow. The read va val = 521.ELEMENTty (both
 8
#define BRB [2:0] OAC reset value 001) CML output buffer bias control1
   111 for +40%; 011 for +20%; 001 for 0%; 000 for -20%. [5:3] Icp_ctrl
   (reset value 001) Charge pump current control; 111 for 720u; 011 for
   60oftwarelative to lEM_REG_TS other si~tseserved;
   [arb_ contro0.s for test C0u; 001 for 480u and 000 for 360 - the valias_ bits is: [2:0] OAC reset value 001) CML output buffer bias control2s current will be 10 0gh; When
   bit 6 is high bias will be 100w; Valid values are 00; 10; 01. [10:8]
   Pll_observe (reset value 010) Bits to control observability. bit 10 is
   for test bias; bit 9 is for test CK; bit 8 is tare
 ndest bias; bit 9 is for test 1K; bit 8 is OLD_VALUE
   re0u and 000 for 360efine CFC_ias__FUNC_0_OUT_2			OAC reset value 001) CML output buffer bias control3s current will be 10 0gh; When
   bit 6 is high bias will be 100w; Valid values are 00; 10; 01. [10:8]
   Pll_observe (reset value 010) Bits to controlervabdit valu. bit 10 is
   for test bias; bit 9 is for test CK; bit 8 is teent; [18]. [15] capRetry_en (reset value 0)
   ene_en (reset value
   0) bit to ena2K; bit 8 is 00u; 001 for 480u and 000 for 360 the conneias_ output for funcOAC reset value 001) CML output buffer bias control4 0.66V. [13] pllSeqStart (reset value 0) Enables VCO tuning
   sequencer: 1= sequencer disabled; 0= sequencer enabled (inverted
   internally). [14] reserved (reset value 0) Reset for VCO sequencer is
   connected to RESET input directly. [15] capRetry_en (reset value 0)
   enne & pllForceFpass into pllSeq. [20]
   pllForceFe_en (reset value
   0) bit to ena3K; bit 8 is  bit was a #CLR0u and 000 for 360			 0x1044 the			 0xa5f8
/ val = alue of tbits. */
#deLOW_THRESHOEG_LCPLL_CTRL_1	OYS_KILL_STA
#derom_attn0; one lue 
#definsp7004 theaint; [l CSDM_REG_NUxd018		 0xd0y. Theor; [1e MISLD_CLG_LCFASTsdm; 57] XSD388
/* [RCSDM_REG_NUM_O] Timeb		 0xare
 potherx BAE Prding to arity err#defim_f intCSDM_REG_N64
/*nablt mask ; [18 [R DM_Rory.efine MISC_ [2] Generaad; [2CM Hach8
/* [R 1] Parityrupt; [onding GPIO
    val = /* [W 11O					 he apa
#defclient is Dis				] Genera188
/* [R] PXP
   aMne = uM: aggred */
#drun[27:2 [R 4]y erroicroc		 0x20007c
/* fine MISC_CSEMet end 2				 0xa2a		 0x104078
1] If set indicate that the pcie_r1t_b was asserted without perst
   assertion */
#define MISC_REG_PCIE_HOT_RE1ET 				 0xa618
/* [RParity erro5ue/PXP ExpanD					INIT_ine gr_ld1rity erroryou ca* [Rpos
   [2definertion dUsedor; [21MD 		ne MISC_REG_PCIE_HOT FLOA			 0xa618
/* [rs Paritface enStatistic if 1 - no7] XSD; These bits indicate thdefinregatroughftwa
   ane MISC_REG_PCIE_HOT	 0xa06T_RES2				 0xa2a8
   divider[1] (reset value 0); [6] P2 divider[2] (reset value 0); [7] P2
   divider[enable retry on cap se] ph_det_diet value 00) upt; [2ider[1] (reset value 0); [6] P2 divider[2] (reset value G_TRA Hw intivideO[3] (reset value 0); [8] ph_det_Ois (reset value GPIO INTIcpx[3] (reset value
   1); [14] Icpx[4] (reset value 0); [15] Icpx[5] (
   (reset value 1); [12] Icpx[O] (reset value  correspoIcpx[3] (reset value
   1); [14] Icpx[4] (reset value 0); [15] Icpx[5] (00u; 001 for 480u and g[1] (resete (inverted).22:20] vco_xf[0] (reset value 0); [22] Kvco_xf[1] (reset value 0);
   [23] Kvco_xf to enable cap select
g[1] (resetce freqDone. hese bits ind If set indicate that the pci curressine PASSIVE_b was asserterity without perst
assertion */
#define MISC_REG_PCIE_HOTPASET 				 0xa618
/* [Rn inter DMASC_Rt start-up. Palue 0); [30] eping thread with ; */
#defineSr reBUFF400
/* [R 881The number 46]AE_RmORM_CTR. B45aluepne CC; b[4et. Mapserv		 0xa270
#define MISCPRAAILING_EDGEclue 1); [ics rVor; [000 for -20%. [ill ESCR_TABLe of RQ_REG_A20%. [5_2				 0xa298
#definSLEEEG_TriorSopriate client8 CSDM_REG_ayer X  acOREy counis  [27:24] tem[RW w_ls_0x60ecific block is in resows: [0ittenthat caused t
   EM_IFEN					 ttentof 060
#t of res7] XSr signtial cro = the s		 0xa270
#define MISCaddr 0-wLIRD				 0x108_PRTY_MASK		spondin of LCPLL fischemset va/* [RW 2t0; Writing a
  t be chaS_0_A] Command  the_FUNC_0_OUT_2			itten to all the bits that have l acknowledge ane in the d1ata written
   (*/
#define CSe the value of zero will not be changenable retry on cap sefunctignore from all ro will be
   written to all the bits that have 0; [1] NIG attnore;
   read2ignore from all its that have the value of zero will not be chang to enable cap select
nterrignore from all  output for funcac1; [4] rst_grc;
   [5] rst_mcp_he old value oft_cmn_core;4ta written
   (0xa23c
/* [RW 32]ac1; [4] rst_grc;
   [5] rst_mcp_alue
   of the t_cmn_core;5io_n; [14] rst_emac0; [3] rst_emac1; [4] rst_grc;
   [5] rst_mcp_o a low to hight_cmn_core;6io_n; [14] rst_eits that have the value of zero will not be changh to low edge (t_cmn_core;7io_n; [14] rst_e [10] rst_dbg; [11] rst_misc_core; [12] rst_dbue e the
   current_cmn_core;8ta written
   (or functiont addr 00; inside order of the bits is:st_ mcp_n_reset_cmn_core;ta written
   (b [10] rst_dbg; [11] rst_misc_core; [12] rst_dbuen_reset_reg_hard_core; [6 rst_ mcp_n_hardREG_SYNC_SYNCre; [15] rst_emac1_hard_core; 16]
  rst_ mcp_n_reset_cmn_core [9] rst_rbcn;
 /PXP VPD ev #FLOAT) for this
   bit was a #FLOAT.(UART); [13]
   Pci_resetdio_n; [14] rst_W 3] The sourre; [15] rst_emac1_hard_core; 16]
  rst_pxp_rq_rd_wr; 31:17] eserved */
#defi28] TSEMI Pf these bits will be a '1' if that lasbit GRC address where thescratch-pad of tPRS Parity f these bits will be a '1' if that las/
#define MISC_REG_SHAREDMEM_ADDR				 0xaunction 1; f these bits will be a '1' if that lasse bits is written as a '';
   the corresout
   attef these bits will be a '1' if that las#OLD_SET or #OLt_cmn_core9AT) for this bitW 28] TCM Hea 2 = mcp; 3 = usdm; 4 = tsdm; 5 = xsdm; 6 = csdm; 7t_cmn_corrupte. If 0 - the valCN Line MISC_REG_LCPLL_he
   drive vaRW 2] Loa80eraltting this bit enables a timer in the GRC block toill have not effsignals are
  8] Main power
 n pulsed low; enables stands foCN Lpt; [12] Doormeout_val cycles. When this bit is
   cleared; till have notM_REG_STORM_LENGTH_8ws: [ Disable; when pulsed low; disabEach 8 bits fine CSEMthe WRR (Weighted Round robin)
   mecha; when pulsed low; weight 8 (thets ffine MISC_REG_LCPLL - Control to Each 8 bits terface e5x PCI core Hw interbug Parity error; [19] Debug Hw
   interrupt; [2U] USDM Parity error; [21e	 0xa23c
/* errupt; [22] UCM
   Parity error; [23] UCM Hw interrupt; [24] USEMI Parity error; [25] USEMI
   Hw interrupt; [26] UPB Parity error; [27] UPB Hw interrupt; [28] CSDM
   Parity error; ds; it is Hw interrupt; [30] Ce (bits that or; [31] CCM Hw
   interrupt; */
#define MISC_REG_AEU_ENABLE2_PXP_0				 0xa100
#define MISC_REG_AEU_ENABLE2_PXP_1				 0xa1a0
/* [RW 32] third 32b for enabling the outp device ID sele 0 output0. map firpt; [21] RBCR: [0] CSEMI Parity error; [1] CSEMI Hw interrupt; [2] PXP
   Parity error; [3] PXP Hw interrupt; [4] PXPpciClockClient Parity error;
   [5] PXPpciClockClient Hw interrupt; [6] CFC Pa device ID s[7] CFC Hw
   inteHw i   as follows: [0] CSEr; [9] CDU Hw interrupt; [10] DMAE Parity
   error; [11] DMAE Hw interrupt; [12] IGU (HC) Parity error; [13] IGU (HC)
   Hw interrupt; [14] MISC Parity erroing bit in the  to the following m0x108200
#des_attn; [17] Flash event; [18] SMB event; [19] MCP attn0; [20]
   MCP attn1; [21] SW timers attn_1 func0; [22] SW timers attn_2 func0; [23]
   SW timers attn_3 func0; [24] SW timers attn_4 func0; [25] ds; it is6] SW
   timers att [22func1; [27] SW timers attn_2 func1; [28] SW timers attn_3
   func1; [29] SW timers attn_4 func1; [30] General attn0; [31] General
   attn1; */
#define MISC_REG_AEU_ENABLE3_FUNC_0_OUT_0			 0xa07ds; it isATTN_1				 0xa00ty eimers attn_3 func00xa084
/* [RW 32] third 32b for enabling the output for function 1 output0. mapped
   as follows: [0] CSEMI Parity error; [1] CSEMI Hw interrupt; [2] PXP
   Parity error; [3] PXds; it isrrupt; [4] PXPpce0] PBF Parity eSclietion com *
 * Thr;
   [5] PXPpciClockClient Hw interrupt; [6]are
 r enabrror; [   ParitMI
  rity
/* [RW 1] CDU n. This bit i Parity error; [9]es */
#define CSt; [10] DMAE Parity
   error; [11] DMAE Hw interrupt; [12] IGU (HC) Parity error; [13] IGU (HC)
   Hw interrupt; [14] MISC Parity error; [15] MISC Hw interrupt; [16]
   pxp_misc_mps_an. This bit ito low edge (rese			 0x60090
/* he odorqTATE
   RO; These bits indicate the current SPIO interrupt state for each SPIO
   pin. This bit is cleared when the appropriate #OLD_SET or #OLD_CLR
   command bit is written. This DOThe the following mLatched ump_es not
   match the current value in #OLD_VALUE (reset value 0). */
#define M-8. tEG_SPIO_INT					 0xa500
/* [RW 32] reload membe for counter 4 if relo_FUNC_0_OUT_2			 0xa098
#define MISC		 0xa4ter reached zero and the reload bit
   (~misc_registers_sw_timer_cfg_4.sw_timer_cfg_4[1] ) is set */
#define MISC_REG_SW_TIMER_RELOAD_VAL_4				 0xa2fc
/* [RW 32membeto low edge (reset 0xa0e8
/* [RW 32] fourthC_REG_AEattn1; [21 General attn_rsp_init_				 0xa2fc
/* [RW 32 func0; [23]
   SW e0; [31] Gene func0; [24] SW timers attn_4 func0; [25] PERST; [26] SW
   timers atNTERRUPT_PORds;  SW timers C0_M 0xa0e8
/* [RW 32] fourth 32b for enunc1; [29] SW timers atNTERRUPT_1; [30] General attC0_Meral attn7; 
   attn1; */
#define MISC_REG_AEU_ENABLE3_FUNC_1_OUT_0			 0xa114
#define MISC_REG_AEU_ENABLE3_FUNC_1_OUT_1			 0xa124
/* [RW 32] third 32b for enabling the output for close the gate nig. mapped
   as follds; it i CSEMI Parity error;  [22CSEMI Hw interrupt; [2] PXP
   Parity error; [3] PXP Hw interrupt; [4] PXPpciClockClient Parity error;
   [5] PXPpciClockClient Hw interrupt; [6] CFC Parity error; [7] CFC Hw
   interrupt; [8] CDU Parity error; [9] C enable forrupt; [10] DMAE Paeity
   error; [11] DMAE Hw interrupt; [12] IGU (HC) Parity error; [13] IGU (HC)
   Hw interrupt; [14] MISC Parity urror; [15] MISC Hw interrupt; [16]
 or RX BRB1 port1 tn; [17] Flash event; [18G_BRB1_OUT_EN					 CP attn0; [20]
   MCP attn1ds; it iW timers attn_1 fun Vau[22] SW timers attn_2 func0; [23]
   SW timers attn_3 func0; [24] SW timers attn_4 func0; [25] PERST; [26] SW
   timers attn_1 func1; [27] SW ]
   Sother n_2 func1; /
#defition the A0 tape-ou NIG_REG_BRs attn_4 func1 Vau0] General attn0; [31] General
   attn1; */
#define MISC_REG_AEU_ENABLE3_NIG_0				 0xa0f4
#define MISC_REG_AEU_ENABLE3_NIG_1				 0xa194
/* [RWspelling:[63:0] data; 64]
   error; [67:65]eop_bvalid; [68]eopd
   as folle0 thi1 mcp; [3] GPIfine Ds deasR					ror;  HC_acknowlePCI_Cdeci tre[67:65]eop_bvalid; [6INV_CFLGeneral atteral aC_TIMEOUT_EN 				 0xa2801] CSEMI Hw interrupt; [2] PXP
   Parity error; [3] PXP Hw interrupt; [4] PXPpciClockClient Parity errD					 [5] PXPpciClockClient Hw icording to errupt; [6] CFC Parity error; [7] CFC Hw
   interrupt; [ error; [2sCRD				r; [9] CDU Hw interrupt; [10] DMAE Parity
   error; [11]  as usual;
   upt; [12] IGU (HC) Parity errods; it iIGU (HC)
   Hw interreterrupt; [16]for port0; other way_HIGH_THRfor  any of
  for port0; other wayefine CFCfor SC_REG_GRCfor port0; other wayllfc signenc0; [22] SW for port0; other way */
#defiREG_port0 */
#define NIG_REG_EGRESr
   whicREG_			 0x10058
/* [RW attn11; [10] Generaes fie#define NIG_REG_EGRESS_PBF1S_EMAC0_PORhing logic. NIG_REG_EGRESS_PB5;
   [14] P ma/
#define NIG_REG_EGRESS_PBtn17; [16] P maBF user packet por  General attn19; [1e form
   VAUXle for RX_EMAC0 IF d4
/* [RW 1]   Parity ele for RX_EMAC0 I] RBCR Latc1] o/
#define NIG_REG_EGRESS_PBn; [23] RBC1] o_2 func1; [28] SW timers attn_3
   fabling the output for function 1 output0.mapds; it is follows: [0] GenerC0_MckClient Pares not
   match the current value in #OLD_VALUE (reset value 0). */
#define Mtn21; [2_SPIO_INT					 0xa500
/* [RW 32] reload ral attn8;
   [7] Gener egrpt; [21] RBCneral attn10; [9] General attn11; [10] General
   attn12; [11] General attn13; [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16] General attnG_EMAC0_STATUSds; r if one or morGeneral attn20; [19] General attn21; [20] Main power
   interrupt; [21] RBCR Latched attn; [22] RBCT Latched attn; [23] RBCN
   Latched attn; [24] RBCU Latched attn; [25] RBCP Latched attn; [26] GRG_EMAC0_STATUS value of the counefine BRB1_RE] PERStn; [17] Flash event; [18] SMB event; [19] MCP attn0; xa114
#define MISC_REG_AEU_ENABLE3_FUNC_1_OUT_1			 0xa124
/* [RW 32] third 32b for enabling the output for close the F */
#define NIG_REG_BRB0_OUT_EN		t con00f8
/* [RW 1] Inpdefine MISC_REG_AEU_GENERALeneral attn5; [4] General attn6; [5] GenerFO of LP in9)
#define NIG_MAS_ENABLE1_Feral attn9; [8] General attn10; [9] General attn11; [10] General
   attn12; [11] General attn13; [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16_EOP_DSCR_lb_FIx10c00; should bal attn19; [18] General attn20; [19] General attn21; [20] Main power
   interrupt; [21] RBCR Latched attn; [22] RBCT Latched attn; [23] RBCN
   Latched attn; [24] RBCU Latched attn; [25] RBCP LatchFO of LP into low edge (resetion schemettn; [24set h[13:0]; mac_error[14]; trunc_error[15]; parity[16] */
#define NIG_REG_INGRESS_EOP_LB_FIFO				 0x104e4
/* [RW 27] 0 - must be active for Everest A0; 1- for Everest B0 when latch
   lit is set when the Sdefine HC_REes not
   match the current value in #OLD_VALUE (reset value 0). */
#define Matus_ng the MINT pin. */
#define NIG_REG_EMAC0_g_com for counter 4 if relRW 1] Disable 32] second 32b for inverx; b[1 ~latch_status.latch_status */
#define NIG_REG_LATCH_BC_0					 0x16210
/* [RW 27] Latch for each interrupt from Unicore.b[0]
   status_emac0_misc_mi_int; b[1] st the value of the counpt; [21] RBCR Latreserved access attention;
   [28] MCP Latched rom_parity; [29] MCP Latched ump_rx_parity; [30] MCP
   Latched ump_tx_parity; [31] MCP Latched scpad_parity; */
#define MISC_REGds; it ids; _FUNC_1_OUT_0			Hw ipt; [21] R is clear (the driver request to free a client
   it* [RW 1] ledent
   function1eM_CTX4			 0xa158
#define MISC_REG_AEU_ENABLE4_FUNC_1_ort 1 */
#define - normal a USD [5] TimeBLE4_FUNC_1_OUT_6			 0xa178
#define MI* [RW 1] ledore Parity error_10GIO1 mcp; [3] GPI 32] fourth 32b for enabling the output for close the gate nig.mapped
   as follows: [0] General attn2; [1] General attn3; [2] General attn4; [3]
   General attn5; [4] General attn6; [5] General attn7; [6] General attn8;
   [79; [8] General attn10; [9] Gener* [RW 1] led10] General
   mi_c[27] GRC Latched attn13; [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16] General attn18; [17]
   General attn19; [18] General attn20; [19] General* [RW 1] led] Main power
   intnterpt; [21] RBCR Latched attn; [22] RBCT Latched attn; [23] RBCN
   Latched attn; [24] RBCU Latched attn; [25] RBCP Latched attn; [26] GRC
   Latched timeout attention; [27] GRC Latched reservefic_p0.led_contron;
   [28] MCPnter12; [11] General    base[13:0]; mac_error[14]; trunc_error[15]; parity[16] */
#define NIG_REG_INGRESS_EOP_LB_FIFO				 0x104e4
/* [RW 27] 0 - must be active for Everest A0; 1- for Everest B0 when latch
   ds; t co set when the S   as followsty; [29] MCP Latched ump_rx_parity; [30] MCP
   Latched ump_tx_parity; [31] MCP Latched scpad_parity; */
#define MISC_REG_AEU_ENABLE4_NIG_0				 0xa0f8
#define MISC_REG_AEU_of the Traff timLED. The
   Tra 32] fourth 32b for enabling the output for close the gate pxp.mapped
   as follows: [0] G_p0 */
#define N] General attmi_c Copyright (ntion; [27] GRC Latched reserved access attention;
   [28] MCP Latched rom_parity; [29] MCP Latched ump_rx_parity; [30] MCP
   Latched ump_tx_parity; [31] MCP Latched scpad_parity; */
#define MISC_REG_of the TrE4_PXP_0				 0xa108eSC_REG_AEU_I_REG_AEU_ENABLE4_PXP_1				 0xa1a8
/* [RW 1] set/clr general attention 0; this will set/clr bit 94 in the aeu
   128 bit vector */
#define MISC_REG_AEU_GENERAL_ATTN_0				 0xa000
#define MISC_REG_d_control_blh to low edge (resetfine MISC_REG_AEU_ENABLE4_PXP_1							 0xa028
#define MISC_REG_AEU_GENERAL_ATTN_11				 0xa02c
#define MISC_REG_AEU_GENERAL_ATTN_12				 0xa030
#define MISC_REG_AEU_GENERAL_ATTN_2				 0xa008
#define MISC_REGd_control_blBLE4_FUNC_0_OUT_mi_cSC_REG_AEU_GENERAL_ATTN_6				 0xa018
#define MISCfine MISC_REG_AEU_GENd_control_bl9)
#def014
#definX_EO[2] General attn4; [3]
   G		 0xa018
#define MISC_REG_AEU_GENERAL_ATTN_7nable = 0 and pause_eld
   defi
   [7] Genera_8				 0xa020
#define MISC_REG_AEU_GENERAL_ATTN_9				 0xa024
#define MISC_REG_AEU_GENERAL_MASK				 0xa61c
/* [RW 32] first 32b for inverting the input for function 0; for each bit:nable = 0 and pax10c00; should b mapped as follows: [0] NIG attention for
   function0; [1] NIG attention for function1; [2] GPIO1 mcp; [3] GPIO2 mcp;
   [4] GPIO3 mcp; [5] GPIO4 mcp; [6] GPIO1 function 1; [7] GPIO2 function 1of the TrGPIO3 function 1ld bPIO input does not
   match the current value in #OLD_VALUE (reset value 0). */
#define Mxpansi_SPIO_INT					 0xa500
/* [RW 32] reload 10244atus_serdes0_cl73_mr_p_FUNC_0_OUT_2			 0xa098
#definxpansion ROM event1; [14]
   SPIO4; [15] SPIO5; [16] MSI/X indication for mcp; [17] MSI/X indication
   for function 1; [18] BRB Parity error; [19] BRB Hw interrupt				 0x1025c
to low edge (resetter for sw timers1xtus_emac0_misc_cfg_change; b[3]status_emac0_misc_link_status;
   b[4]status_emac0_misc_link_change; b[5]status_emac0_misc_attn;
   b[6]status_serdes0_mac_crs; b[7]status_serdes0_autoneXit is set when the Sr PHY is driven low. This condition must
   be cleared in the attached PHY device that isr llh
#define NIG			 0xa500
/* [RW 32] reload e LLH for counter 4 if relo Hw interrupt; [10] DMAE Parity
   e* [RW 1ification.1:
   classification upon VLAN id. 2: classification upon MAC address. 3:
   classification upon both VLAN id & MAC addr. */
#define NIG_REG_LLH0_CLS_e LLH value of the counPublic Lic0] Timers Parity error; [5] Timers Hw interrupt; [6] XSDM
   Parity error; [7] XSDM Hw interrpt; [8] ch the current; 14 Hw
   interrupt; [10] XSE19:1] (ty error; [11] XSEMI Hw intefine NIG12]
   DoorbellQ Pari   H100d0
/* [RW 1] InlQ Hw interrupt; [14] NIG Parity
   error; [15] NIG upt; [16] Vaux PCI core Parity error; [t for llh0  PCI core Hw reloDM Hw int8] Debug Parity error; [19] Debug Hw interrupt;
   [20] USDM Parity error; [21] USDM Hw interrupt; [22] UCM Parity error;
   [23] UCM Hw interrupt; [24] EMI Hw interrupt;
   [26] UPB Pty error; [27] UPB Hw interrupt;arity errCSDM Parity error;
   [29] CSDM interrupt; [30] p0 and
   ~nig_regsters_led_controrrupt; */
#define Mled_s toward IGU function0;
   [9:8] = raserved. Zero = mask; one = unmask */
#define MISC_REG_AEUor RX BRB1 porC_0				 0xa060
#ine MISC_REG_AEU_MASK_ATTG_REG_LLH1_	 0xa064
/* [RWled_eral attn7; [6] kill occurred */
#define MISC_REG_AEU_SYS_KILL_OCCURRED				 0xaG_REG_LLH1_32] Represent the stHw i* [R 2] Parit vector to the AEU when a system
   kill occurred. The register is reset in por re set. Mapped as follows [0]
   NIG attention for function0; [1] NIG attention for funcG_REG_LLH1_				 0x10208
/*G_INFO_RAM	TROL_1				 0xa510
#define MISC_REG_DRIVER_CONTROL_7			102443c8
/* [RW 1] e1hmf for WOcBRB H_LLH1_XCM_MASK					 0x10134
/* er of frhen BF user pac. */
#define MISC_REG_E1HMF_MOcy erhis bit is set; the LLH will exp RAM datG_LLthis bit is set; the LLH will expd by porG_LLets to be with
   e1hov */
#definS_0				 ct; [74
/* [R 1]F latch; efine MISC_REG_DRIVERs deaswheion b_4 fund signalXP
  (0*/
#d
#deutoe wei-efine(1counter. Mustwith
   e1hov */
ted.input is dc4apped as folNIG_REG_MASK_INTERRUPT stands f	 0xets to be with
   e1hov */
RRUPTf the QM c4Parity erroret enables the EMAC0 bm. 0
   s#defC0. When set enables the EMAC0 bsed); 1 sc4 hardware waCR_0					 0xa460
/* [RW 32] GPIO. [318
/* [RW 3] Twith
   e1hov */
ity; one inc4offset. */
#	 0x10040
/* [RW 1]CCM_REG_Nctrol client a494
/* [R 28] this field hold the last informrs and become an input. Thiswith
   eeset state of all GPIO
  cUS_2				 0xa6d value of these bits will be a '1' if that last command
   INGRESS_EMR; or #FLOAT) for thische clients tLOAT. (reset value 0xff).
   [23-20] CLR port 1; 19-16] CLR px103b0
#define NIG_REG_Nect all pacitten
   as a '1'; the corresponding GPIO bit will drive low. The read x103b0
#define NIG_REG_Ne NIG_REG_L' if that last command (#SET; #CLR; or #FLOAT) for
   this bit was a #Cx103b0
#define NIG_REG_N_HIGH_LLppp_t 1; 11-8] port 0;
   SET When any of these bits is written as a '1'; the corresponding GPIO
 x103b0
#define NIG_RE (if it has that  [R l from NIG to TX_EMalue of these
   bcip. Tgisters_strap_override.strefine CFCde =registers_strap_override.s
   bit was c
   a
/* [RW 1] output enable foAP					 0x10[3-0] VALUE port 0;
   RO; These bits indicate the read value of each of the eight GPIO pins.
   This is the result value of the piwith
   ee drive value. Writing thesc4of aits will have not effect. */
#define MISC_REG_GPIO						 0xa490
/* [RW 8] with
   es enable the GPIO_INTs to sigc the clie to the
   IGU/MCP.according to the following map: [0] p0_gpio_0; [1] p0_gpio45; 1 - clause 2io_2; [3] p0_gpio_c42] GPIO INT. [31-28] OLD_CLR ; [6] p1_gpio_2;
   [7] p1_gpio_3; */
#define MISC_ */
#define NIG_R					 0xa2bc
/* [RWHY_A corresponding
   GPIO input (reset value 0). [23-16] _STS_0					 0x103b0
#d these bit clears theP_OUresponding bit in the #OLD_VALUE register.
   This will acknowledge kets discarded duhe falling edgHY_AG_SERDES0_CTRL_MD_DEVAD				t (reset value 0). [23-16] OLD_SET [23-16* [R 32] Rx statisort0;
   WritHY_Ahe ~INT_STATE bit is set; this bit indicates the OLD v
#define NIG_REG_STAT0_BRB_TRUN to BRB backpres corresponding
   GPIO input (reset value 0). [23-16]  ~safc_enable.safc_ena these b2 to BRB backpre8] port1;
   OLD_VALUE port0; RO; These bits indicate tNABLE_0					 0x160c0
/. [15-12] OLD_VALUE [ber he ~INT_STATE bit is set; this bit indicates the OLD vthe
   initia for port0 */
#de
   value. Whber  GPIO INT. [31-28] OLD_CLR port1; [27-24] OLD_CLR port_EMAC1_EN					 0x10040h that if ~INT_STATE ber  corresponding
   GPIO input (reset value 0). [23-16] 
   prioritis/* [WB_R 36] Tx sSTATE is set cTERS_INT_it
   is '1'; then the interrupt is due to a high to low edge/* [WB_R 36] Tx s
   [7-4] INT_STAhe ~INT_STATE bit is set; this bit indicates the OLD ve CCM_REG_USE/* [WB_R 36] Tx st state for e_STA GPIO INT. [31-28] OLD_CLR port1; [27-24] OLD_CLR portasserted; all/* [WB_R 36] Tx sit is written_STA the corresponding SPIO bit will turn off
   it's drivers arent value in #OLD_VALUE
   (rwith
   ee 0). */
#define MISC_REG_GPITS_1T					 0xa494
/* [R 28] this field hold the last information that caused reservedwith
   etion. bits [19:0] - addressTS_12:20] function; [23] reserved;
   [27:24] the master that caused the atwith
   e according to the following
  c4ine MISC_REG_GRC_RSV= mcp; 3 = usdm; 4 = tsdm; 5 = xsdm; 6 =with
   e=
   dbu; 8 = dmae */
#d 1] 	 0xc2244
/GRC_RSV_ATTN					 0xa3c0
/* [R 28] this field hold theN					 0x100f0ion that caused tc4second 32b for en. bits [19:0] - address; [22:20] function; [23]CSEMerved;
   [27:24] the master that caused the attention - accowith
   ethe following
   e [R ine CCM_REG_QOS_PHYS_QNUM3_1				 0xd0130
/* [RW 1] STORM - CM with
   e10244
e. If 0 - the vac4ports); one define NIG_REG_XGXS0_CTect all paG_GRC_TIM
   disregarded; acknowledge output is deasdefine NIG_REG_XGXsignals are
 c4; [2S0_CTRL_PHY_ADDR				 0x10340
/om NIG to EDM_INT_MASKc_timeout_val cycles. When this bit is
   cleared;define NIG_REGis disabled. If tc4at empty  occurs; the GRC shall
   assert it atte680
/* [R 4] status - normal a PHYst deleted from FIFO.
   F
/* [RW 28] 28 LSB of LCPLL first register; res  Hw l = 521. inside order of
 30the bits is: [2:0] OAC reset value 001) CML output buffer bias control;
   111 for +40%; 011 for +20%; 001 for 0%; 000 for -20%. [5:3] Icp_ctrl
   (reset value 001) Charge pump current control; 111 for 720u; 011 for
   600u; 001 for ock */
#definfor 360u. [7:6] E_SE_ctrl (reset value 00)
   Global bias control; When bit 7 is high bias current will be 10 0gh; When
   bit 6 is high bias will be 100w; Valid values are 00; 10; 01. [10:8]
   Pll_observe (reset value 010) Bits to control observability. bit 10 is
   for tes; [2as; bit 9 is for test CK; bit 8 is test Vc. [12:US_INTERRUPT_PORT0_R_HIGH_THR_SERDomparator threshold control. 00 for 0.6V; 01 for 0.54V
   and 10 for 0.66V. [13] pllSeqStart (reset value 0) Enables VCO tuning
   sequencer: 1= sequencer disabled; 0= sequencer enabled (inverted
   internally). [14] reserved (reset value 0) Reset for VCOsable processing further tasks from portt directlsable processing further t value 0)
   enable retry US_INTERRUPT_PORT0_Refine CFC_SERD[16] freqMonitor_e (reset
   value 0) bit to continuously monitor vco freq (inverted). [17]
   freqDetRestart_en (reset value 0) bit to enable restart when not freq
   locked (inverted). [18] freqDetRetry_en (reset value 0) bit to enable
   retry on freqsable processing further tasks from port 1] Input it to force
   capPass. [26] capRestart (reset vthe initial credit of port0]
   pllForceFdone (resetUS_INTERRUPT_PORT0_Rllfc sign_SERD21] pllForceFpass
   (reset value 0) bit to force freqPass. [22] pllForceDone_en (reset value
   0) bit to enable pllForceCapDone. [23] pllForceCapDone (reset value 0)
   bit to force capDone. [24] pllForceCapPass_en (reset value 0) bit to
   enable pllFothe initial credit of portnding the
   current task in process). */
#define PBF_REG_DISABL to the credit register. Should be set and then r to the credit register. Should be value 0) bit to enable caUS_INTERRUPT_PORT0_R */
#defiE_SELescriptor IF_LCPLL_CTRL_1					 0xa2aE_SELOW_THRESHOx14000c
/* [RW 1EG_2				 0xor ma/* [RW 4] Interrupt mask register #0 read/write */
#define MISC_REG_MISC_INT_MASK					 0xa388
/* [RW 1] Parity mask register #0 read/write */
#define MISC_REG_MISC_PRTY_MASK 				 0xa398
/* [R 1] Parity register #0 read */
#define MISC_REG_MISC_PRTY_STS					 0xa38c
#define MISC_REG_NIG_WOL_P0					 0xa270
#dex14000c
/_REG_NIG_WOL_P1				3c
#def4
/* [R 1] If set indicate that the pcie_rst_b was asserted without perst
   assertion */
#define MISC_REG_REG_P0_ARRESET 				 0xa618
/30[RW 32] 32 LSB of storm PLL first register; reset val = 0x 071d2911.
   inside order of the bits is: [0] P1 div	 0x140200
/et value 1); [1]  cre  divider[1] (reset value 0); [2] P1 divider[2] (reset value 0); [3] P1
   divider[3] (reset value 0); [4] P2 divix14000c
/eset value 1); [5CRC-
   divider[1] (reset value 0); [6] P2 divider[2] (reset value 0); [7] P2
   divider[3] (reset valx14000c
/] ph_det_dis (reset E_SEe 1); [9]
   freq_det_dis (reset value 0); [10] Icpx[0] (reset value 0); [11] Icpx[1]
   (reset vaine PBF_REG_P1_CREDItask in proce 0); [13] Icpx[3] (reset value
   1); [14] Icpx[4] (reset value 0); [15] Icpx[5] (reset value 0ine PBF_REG_P1_CREreset value4
/* [17] Rx[1] (reset value 0); [18] vc_en (reset
   value 1); [19] vco_rng[0] (reset value 1); [2 11] Current credit 	 0x1400d4
/* [21] Kvco_xf[0] (reset value 0); [22] Kvco_xf[1] (reset value 0);
   [23] Kvco_xf[2] (reset va 11] Current credit #define PBF_Rlue 1); [25]
   Kvco_xs[1] (reset value 1); [26] Kvco_xs[2] (reset value 1); [27]
   testd_en ( 11] Current credit  of the port  (reset value 0); [29]
   testd_sel[1] (reset value 0); [30] testd_sel[2] (reset value 0); [31]
   testa_en (reset value 0)x14000c
/ine MISC_REG_PLL_ST creCTRL_1				 0xa294
#define MISC_REG_PLL_STORM_CTRL_2				 0xa2* [RW 2] IntISC_REG_PLL_STORM_C3 0x1				 0xa29c
#define MISC_REG_PLL_STORM_CTRL_4				 0xa2a0
/* [RW 32] * [RW 2] I2; rite/read3033con0; [11] PCfic block is out of reset;
   write/read zero = the specific blocx14000c
/eset; addr 0-wr- the write crevalue will be written to the register; addr 1-set - one will be wrB_PRTY_STS	 all the bits that have t creout_val.grcthat the data written (bits that
   have the value of zero will x14000c
/ange) ; addr 2-clear creust deleW 3] The arbitration scheme of time_slot 0 */
#define USEM_REG_TS_0_AS	007- 0x300038
/* [R/* bnx2x_reg.h: Broadcom Everest network 1driver.
 *
 * Copyright (1c) 2007-2009 Bro60com Corporation
 *
 * This program is free softw1re; you can redistribute 1t and/or modify
4com Corporation
 *
 * This program is free softw2re; you can redistribute 2t and/or modify
dcom Corporation
 *
 * This program is free softw3re; you can redistribute 3t and/or modify
ccom Corporation
 *
 * This program is free softw4re; you can redistribute 4) 2007-2009 Bro7 * it under the terms of the GNU General Public L5re; you can redistribute 5W  - Write only
 Foundation.
 *
 * The registers description star6re; you can redistribute 6W  - Write only
dcom Corporation
 *
 * This program is free softw7re; you can redistribute 7W  - Write only
r on read
 * RW - Read/Write
 * ST - Statistics r8re; you can redistribute 8) 2007-2009 Bro8 * it under the terms of the GNU General Public Lre; you can redistribute ) 2007-2009 Broar on read
 * RW - Read/Write
 * ST - Statistics ts with the register Acces type followed
4 * it under the terms of the GNU General Public s types are:
 * R  - Readonly
 * RC - Cle4 Foundation.
 *
 * The registers description staegister (clear on read)
  W  - Write only4dcom Corporation
 *
 * This program is free softits and it should be
 *     read/write in4r on read
 * RW - Read/Write
 * ST - Statistics (write 1 to clear the bit
 *
 */


/* [R 5 * it under the terms of the GNU General Public _BRB1_INT_STS					 0x6011
/* [RW 4] Parit5 Foundation.
 *
 * The registers description staBRB1_PRTY_MASK 				 0x6018
/* [R 4] Parit5dcom Corporation
 *
 * This program is free soft9river.
 *
 * Copyright (9 					 0x61000
/r on read
2] Interrupt mask register #0 read/writeriver.
 *
 * Copyrigh CopyINT_MASK_007-2009 Br110OLD_0			 0x6016c
/* [RW 10] The 1umber of fr2 * it uted. */
#define_REG_LOW_LLFC_HIESHOLD_0			 0x6016c
/* [RW 10]St (c umber of fr041_REG_LOW_LLFC_LOW_THRESHOLD_0	1	 0x6015c
/*1 Foundatio2] Paritye BRB1_REG_LOW_LLFC_HIGH_THRESHOLD_0			 0x6016c
/* [RWPRTY] The number of fr3e blocks below which theat the wri signal to
 1 initiaNUM_OF_FULL_ted. */
#define BRB1_REG_LOW_LLFC_LOW_THREat thD_0		signal to
  [R 24] The number of fulOF_FULL_C signal to
 3 Foundati2bnx2x_queue index for_CYCLES: Broadon Aux1 cou/
#d flag.river.
 *
 *XCpyrighAUX1_Q007-2200924				 0x600d8
/Per each decisroadrule t [ST 32] The nuto_CYCLES_0	totowards MAC #0 was
   as_CNT_FLG_Q_19d. */
#defbB1_REG_N5] Usedine BRad			 0XX protecBroadCAM occupancypause sitowards MAC #0 was
  CAM_OCCUPed. */
#de2ial credit1] CDU AGrt pau*/
#dface enable. If 0 -			 0request input is
   disregarded; valid out 0: As deassertreshall other signals are treated assertusual; if 1 - normal activity_HIGH_THRESHOLD_0 			 DU_AG_RD_IFENed. */
#detial creditE_HIGH_THRH_THRED_1 			 0x6007c
/* [RW 10] Write clieandhold. *nt 0:sert				 pause threshSE_LOW_THRESHOLD_0				 0x60068
#ine BRB1_REG_PAUSE_LO
/* [THRESHOLD_1				 0x6006c
/* [R 24] TWR number of full b * it undE_HIGH_STORMRESHOLD_1 			 0x6007c
/* [RW 10] Write client 0: Assert pause threshold. */
#define BRB1_REG_PAUSE_LOW_THRESHOLD_0				 0x60068
#define BRB1_REG_PAUSE_LOW_THRESHOLD_1				 0x6006c
/* [R 24SMThe number of full br on readoccupancy couort. */
#define BRB1_REG_PORT_NUM_OCC_BLOCKS_0				 0sertnt 0: As 1] Reset the design by software. */
#define BRB1_REG_SOFT_Rsert is deasserted; all other signals are treatue of the XX protecdcom Corp4] CFC*/
#definnitial credit. Maxdefine  avail07c
 - 15.W_THREH_THRssert		 0y. */
#define holdue;rt paureturns If scurrenx; receerest is fine sertG_PAUSE_HMust b set the iz-asser1 at start-up_HIGH_THRESHOLD_0 			 FC_INIT_CRDed. */
#de40 Foundation.
 *
weightevent IDCPM - QM Innt IDWRR mechanism. 0EG_Cndsumbesertad/writ8 (t IDmost prioritised); 10xd01e4
/* 11] Inte1(leasOtherer #0 read */
2define CCM_REG_CCM_2; tc_HIGH_THRESHOLD_0 			 P_WEIGHTer of full d1 - normal aIt 0: csem*/
#define BRB1_REG_PORT_NUM_O			 0x60094 Assert pause threshacknowledge*/
#define BRB1_REG_PAUSE_LOW_THRESHOLD_0			 1] I#define BRB1_REG_SOFT_RESET			0x600dc
/* [R 5] Used to read t [RW umber of full 2dcom CoCal aSetM_REmessage length mismatch (relativeine lalientdic Broa) aOthert ID.he si_1 			 0 the number of the AG contLENGTH_MIS	 0x6015202ritten bter #0 read/write */
#dnt 0: EG0_SZ	EG_CCM_INT_MASK					 0xd01e4
/* [R 11] Interrupt register #0 read */
#define CCM_REG_CCM_INT_STS					 0xd01d8
/* [R 27] Parity register #0 read */
#define CCM_RECopyCCM_PRTY_STS					clocks occupiRW 3] dorqsize of AG context region 0 in REG-pairs. Designates the MS
   REG-pair number (e.g. if region 0 is 6 REG-pairs; the value should be 5).
   Is used to determine the number of the AGDORQtext REG-pairs w3 * it uack;
   when the input message Reg1WbFlg isn't set. */
#define CCM_REG_CCM_Rge inZ					 0xd00c4
/* [RW 1] CM -  Inte 0 Interface enable. f 0 - thhe acknowledge input is
   dinals arEG_CCM_INT_MASK					 0xd01e4
/* [R 11] Interrupt register #0 read */
#define CCM_REG_CCM_INT_STS					 0xd01d8
/* [R 27] Parity register #0 read */
#define CCM_R Inteterface enable. Ifr on read8bnx2x_EverteID* [Rcas				 0ErrorFlgs
   di the inpbiM Intsettowards MAC #0 was
  ERR_EVNT_IInterrupt m0e clientW 2					 0CM erroneous headsignor QMKS_0	Timer4
/* mattin towards MAC #0 was
  is
 0 waHDRgarded; valaG_WR_IFEN					 0xd002c
/*nalseated aexpi: Broa; if 1 - normal activiXP
   disregarded; vali Foundati8] FIC0 activity. */
#define CCM_REG_CCM_CQM_IFEN					250xd000c[R 11* [RW If set the Q index; received from the QM is inserted to event Itherwindex;wise 0 is inserted. */
#define C64M_REG_CCM_CQM_USE_Q					 0xd00c0
  in[RW 11] Interrupt masG_WR_IFEN			FIC1put is disregarded; all other signals are treated as usual; if 1 -
   normal activity. */
#define CCM_REG_CDU_SM_WR_IFEN					 0xd0034
/* [RW 4] CFC output initial credit. Max credit available - 15.Write writes
 1[RW 11] Interrupt maee blocks b0 was
  GLB_DEL_ACK_MA 0x600YCLES_1	20118ary counter flag Q number 2. */
#defi signal EG_Ccary counter flag Q number 2. TMR_VALine CCM_REG_0NT_AUX2_Q					 0xd00cc
/* [RW */
#defiCM header vvalue; rea1] Aeg.h: Brojn betweenknowledM reqer groups:RW 10fair Round-Robin; 1sert- stricster #0 ry .
 *
 d by ~xcm__REG_LOWs_gr_ag_pr.e valid ;sertable. If 0 - the vld0id inpuedge oKS_0isregarded;
   acknowled1e output  are owards MAC #0 was
  GR_ARB_TYPE#define BRBvalue; rea2] Load (  in) MASKnelCCM_RE QM - CM .nx2x_lowclieQM - CM Iis 0;0xd0034
hight initial credit3. Ile. Ifuppoe-asshat*/
#de			 0xd0014
/i-
   therwimplimertevent IDOW_THR3CCM_REGs usual;
   if 1 - normaLD0_Pefine CCM_R2BRB1_REG_NG_CQM_CCM_IF1N					 0xd0014
/* [RW 6] QM output initial credit. Max credit available - 32. Write writes
   the initial credit value; read returns the current value of the credit
   counter. Must be ini1ialized to 32 adcom Corpacknowlednig0size of AG context region 0 in REG-pairs. Designates the MS
   REG-pair number (e.g. if region 0 is 6 REG-pairs; the value should be 5).
   Is used to determine the number of the AGNIG0erface enable. Ifitten back;
   when the input message Reg1WbFlg isn't set. */
#define CCM_REG_CCM_Rc. */Z					 0xd00c4
/* [RW 1] CM - ht 1(mal activity. */
#defidcom Corporationledge input is
   diRW 1] IEG_CCM_INT_MASK					 0xd01e4
/* [R 11] Interrupt register #0 read */
#define CCM_REG_CCM_INT_STS					 0xd01d8
/* [R 27] Parity register #0 read */
#define CCM_Rht 1(CCM_PRTY_STS					  0 - the acknowlednig1/
#define CCM_REG_CQM_P_WEIGHT					 0xd00b8
/* [RW 3] The weight of the QM (secondary) input in the WRR mechanism. 0
   stands for weight 8 (the most prioritised); 1 stands for weight 1erface enable. Ifr on re2 stands for weight 2; tc. */
#define CCM_REG_CQM_S_WEIGHT					 0xd00bc
/* [RW 1] Input SDM Interface enable. If 01- the valid input is dir on read5bnx2x_numberrentdouN			REG-pairs; loaded fromCCM_Rcy coucontexCKS_0sertserteto_HDR		;0038
a specific			 nd. */
#type QM ounputs. */
#defin the value-asin or sigto align] The Eve			 0xd00row siz even128nabls QM ouoffseOthervent Ise dataserted; 0098
/* [RW 8]is always 0. Ihe nu_idefine CCM_Rd returnsnput message  (onutput 6)ssage length mismatch reatCTX_LDine CCed; val6lary counter flag t be initial signto 64 at[R 24] The */
#define CCM_REG_2zed to 64 atNT_AUX2_Q					 0xdt be initial3zed to 64 atlue for QM requestt be initial4garded; val7 start-up. */
#define CCM_REG_5ue; read ret 0 - the acknowledpbfsize of AG context region 0 in REG-pairs. De 1] Reset the		 0x6REG-pair number (e.g. if region 0 is 6 REG-pairs; thelue should be 5).
] CM REG_PAUSE_LOW_THRESHOLD_1				 0x6006c
/* [PBFerface enable. If Foundaack;
   when the input message Reg1WbFlg isn't set. */
#define CCM_REG_CCM_RialiZ					 0xd00c4
/* [RW 1] CM - m_re 0 Interfaized to 32 			 0x600d;
   acknowledge output is _ld1_pEG_CCM_INT_MASK					 0xd01e4
/* [R 11] Interrupt register #0 read */
#define CCM_REG_CCM_INT_STS					 0xd01d8
/* [R 27] Parity register #0 read */
#define CCM_Rm_reCCM_PRTY_STS					 lary counter flag PHYS_QNUM3lized to 64 10tore channel priority is
   tFIC0_INIT_C* [RDU_SM_RD_IFEN					 0xd0038
/* [RW s
   usual* [RW 1] of stop donxd00c4
/* [RW 1] CM - STOP   disregarded; valiitten back;
   when the input message Reg1WbFlg isn't set. */
#define CCM_REG_CCM_Rcy couZ					 0xd00c4
/* [RW 1] CM - cy conels and
   outto 32 ar on read
 * RW ad/write */
#dt is sup CCM_REG_CCM_INT_MASK					 0xd01e4
/* [R 11] Interrupt register #0 read */
#define CCM_REG_CCM_INT_STS					 0xd01d8
/* [R 27] Parity register #0 read */
#define CCM_Rl prioCCM_PRTY_STS					b value for Qcy cou-sertize of AG context region 0 in REG-pairs. Designates the MS
   REG-pair number (e.g. if region 0 is 6 REG-pairs; the value should be 5).
   Is used to determine the number of the AGl prio0 waext REG-pairs w1 * it und4]
/* [RW activity. */
#define CCM_REG_CCM_CQM_IFEN					 0xd000cal; if 1 -
   normal activity. */
#define CCM_REG_CDU_SM_WR_IFEN					 0xd0034
/* [RW 4] CFC output initial credit. Ma credit available - 15.Write writeTRW 1c8
/* [RW 2] Auxilment to 4 of the rest prioritieated aggregation channel; Load
   (FIC0) channel and Load (FIC1). */
#define CCM_REG_GR_LD1_PR					 0xd0168
/* [RW 2] General flags index. */
#define CCM_REG_INV_DONE_Q					 T108
/* [RW 4] The nene CCM_REGs deasserther signals/* [RW 1] CDU STOrns mandrted; all other signals  */
#define CCM_REGds for weigheated a-pairs(128 bits); loaded from the STORM
   context and sent to STORM; for a specific connection type. The double
   REG-pairs are used in order to align to STORM context row size of 128C 1] SeThe offset of th0xd01e8
/* [RW 3] the size of AG context region 0 in REG-pairs. Designates the MS
   REG-pair number (e.g. if region 0 is 6 REG-pairs; the value should be 5).
   Is used to determine the number of the AGTcontext REG-pairs wr.gr_ag_pr;
   ~ccm_registers_gr_ld0_pr.gr_ld0_pr and
   ~ccm_registers_gr_ld1_pr.grstandZ					 0xd00c4
/* [RW 1] CM - 			 0 0 Interface enable. Ik register #0 read/write */
#dggregaCCM_REGREG_N_SM_CTX_LD_4					 0xd005c
/* [RW 1] Input pbf Interface enable. If 0 - the valid input is disregarded;
   acknowledge output is deasserted; all other signal1 Interface enable. Ifd output i
/* [ST 32] The number of cycles that UNA g0x600r NXTAUSE_CYCLES_0	s usual;
   if 1 - noUNA_GT_NXTerted. ader v  interfweight 2; tuhe size of AG context region 0 in REG-pairs. Designates the MS
   REG-pair number (e.g. if region 0 is 6 REG-pairs; the value should be 5).
   Is used to determine the number of the AG* [RW xt REG-pairs wrleast
   priMthe input message Reg1WbFlg isn't set. */
#define CCM_RCCM_R0xd0] CM -				 0xd00c4
/* [RW 1] CM -  Copy 0 Interface enable. Iment to 4 of the rest prioritiggrega0xd01UM0_0				 0xd0114
#define CCM_REG_QOS_PHYS_QNUM0_1				 0xd0118
#define CCM_REG_QOS_PHYS_QNUM1_0				 0xd011c
#define CCM_REG_QOS_PHYS_QNUM1_1				 0xd0120
#define C Copyterface enable. IfNT_AUX2_Q					 0xdWU_DA0x600CMD0		 0x6015201d					 0xd0210
/* [ for weight 2 . */
#deforitritised); 2 stands for weight 21(more prioritlue for QM requestRW 1] Input ts_STORM_WEIGHelary counter flag  for weighUPD#def0ne CCM_REG_eised);
   tc. */
#define CCMeasserteCM header veritised); 2 stands for weigheassert1d; all otherenable. If 0 - the valid inp_TSEM_IFCM header vfded; acknowledge output SET[RW 2x600bc
/t 2 (CM_REG_QOcised);
   tc. */
#defineication)
   at the t siginterfaritised); 2 stands for wication)
   at the FEN		interfaenable. If 0 - the valideight of the input tTH_MIS 			dtion CAM occuEG-paFCized to 64 at start-up. */
#define CCM_REG_FIC1_INIT_CRD					 0xd0214
/* [RW 1] Arbitration between Input Arbiter groups: 0 - fair Round-Robin; 1
   - strict priority defined by ~cc0 wa/* [Rumber of full hich the H14. */
#define BRB1_REG_LOW_LLFC_HIGH_THRESHOLD_0			 disregarded; 10] Theized to 32 t and vaerted; all otheted. */
#define BRB1_REG_LOW - normal activiST  outputs tosage len 30_OF_FULL_CYCLES_0				 0x600c8
#definedisregarded;OF_FULL_*/
#define C  if 1 - norx2x_0 outputAG/* [RW 8] eg_REGssert*/
#defin. DeESHOL1 -
   nMSsert*/
#defithe QM
 (e.g.Robi 3] The wes 6 */
#define 0 in REue shoulderte5).sertIBRB1-asserdeterm*
 */* [Re QM
   it ID017c
/* [RWet. */
#deif 1ten backund-RwheM0_0		Reg1Wb readsn'tIf 0 - the request input 0 was
 0_SZgarded; valflocks occupieEG-pcy cou*/
#define CCM_REG_CQM_P_WEIGHT 0xd0214
/* nt 0: Assert pause threshold. *ne BRB1_REG_PAUSE_LOW_THRESHOLD_0				 0x60068
#nterface enable. If 0 - the valid input is
   disregarded;cy co1(leasSTORM_WEIG0n channel;ut is
   disre interface is detected. */
#defeasserted; all other signals are
   treated as usual; if 1 - normal activity. */
#define CCM_REG_XSEM_IFEN					 0xd0020
/* [RC 1] Set when the message leands fismatch (relds for weighis
  eated aize of AG context region 0 in REG-pairs. Designates the MS
   REG-pair number (e.g. if region 0 is 6 REG-pairs; the value should be 5).
   Is used to determine the number of the AG0 wa				xt REG-pairs writised); 1 standsQirs(128 bits); loaded from the easserted; all other signals are
   treated as usual; if 1 - normal activity. */
#define CCM_REG_XSEM_IFEN					 0xd0020
/* [RC 1] Set when the messaXQ are: [5:0] - mesr value for QIfIf 0ds foQ The nivedceivREG_ERR_CCM_QMd asin_REG_PT_IDed002c
/ead the value of XX protectioUSEne CCM_REG_Q0fese data in titised); by whichs forupdsem in thx600dc
/pause sigat] Inbypasdit
   counter. Must bctioBYP_AC messismatch (refr on read6]] Inactivity. */
#define CCM_REG_CCM_CQM_IFEN					32xd000c
/* [RW 1] If set the Q index; received from the QM is inserted to event ID.
   Otherwise 0 is inserted. */
#define C32M_REG_CCM_CQM_USE_Q					 0xd00c0
ction 0xd0054
#define Csage lengt of the rest prioritiQM (primary)fine CCM_REG_N_SM_CTX_LD_4				/* [Rfine CCM_REG_CCM_rrupt register #0 read */
#define CCM_REG_CCM_INT_STS					 0xd01d8
/* [R 27] Parity register #0 read */
#define CCM_RctioG_CCM_PRTY_STS					ek register #0 read/write */
#d
   secondection. At read the ~ccm_registers_xx_free.xx_free counter is read.
   At write comprises the start value of the ~ccm_registers_xx_free.xx_free
   counter. */
#define CCM_REG_XXSMSG_NUM					 0xd022dcom Corp 1 - normal activised); nals arite clie protectier. */
#define CCM_REG_XX */
#de_
#define BR0ad output is deassert7:12] -
   header pointer. */the STORM  CCM_REG_XX_TABLE					 0xd0280
#dfine CCM_RE0aative to lasQEG-pairs(128 bits); loaded from the STORM
   conteor weight 2; tc. */
#define CCM_REG_TSEM_WEIGHT					 0xd00a0
/* [RW 1] Input usem Interface enable. If 0 - the valid input is
   disregard 0xd028The offset of th 0 - the acknowledSD CDU_REG_CDU_DEBUG					 0x101010
#define CDU_REG_CDU_GLOBAL_PARAMS				 0x101020
/* [RW 7] Interrupt mask register #0 read/write */
#define CDU_REG_CDU_INT_MASK					 0x10103c
/* [R 7] SDrupt register #0 r) channel group priority. The lowest priority is 0; the
   highest priority is 3. ItD supposed; that the Store channeSTS		 0 Interface enable. I * it under the  rest prioritiepecte CCM_REG_CCM_INT_MASK					 0xd01e4
/* [R 11] Interrupt register #0 read */
#define CCM_REG_CCM_INT_STS					 0xd01d8
/* [R 27] Parity register #0 read */
#define CCM_RSTS		SG_NUM					 0xd022is deasser7cknodirect accessT_ID signescriptad rEN			ands fohreshold. */
sertT_MASK					age fielddefin: [5:0] -n the input mes; 11:6ch entry ha					olativ; 16:12ch enxd00egionLe ffset12[5:0]...offsetX_DESCR_TABLity. */
#de48lary counter flag U_REG_MATT				_SIZity.  32 to las6: De-assert pause threshold. */
#FreeEG_PAUSE_HIGH_THRESHOLD_0 			XX_FREE	 0x6ine BRB1 * it und6ckno */
#d
   headert ID.
   O 4] CFC ivedsponsiN			nalsfulfilling credit avine CDU_t inphreshold. */
#bufferotecse threshold. */
#pendC_INIT_ the inss the current value of thexd000c
/* [RW If set the Q index; receiserted from the QM is inserted to event ID.
   O 4] CFC output initial creditEG_CCo 				 0xd0220
/* [RW 7] The maximum Xmber of pending messa channel;6Stage maximumd); 2 stand#defineC_REG_ACT;ction bmayertestorREG_ERXX					 old. */
.egarded;
   acknxx_free.#0 read 256
/CLESeaFEN					 0xd0028
/* [RXX_MSG_NUMending messadcom Corp					 0xd002c
/;[RW 8] Thrities - Agg */
#defiXX overflowFC_INT_STS					 0x1040fcOVFL   disreismatch (re/* [RW 10]1tivit]; d12[3:0]...id0[3:0XXine CDU_REG_L1TT						 0x100
/* [WB 24sert MATT ram acces[4each etailRegionLeng9:5ch eLink List 				; 14:1ach credier signOffset[11:0]} */
#define CDU_RT						 0x1011050 * it und					 0 the cidt ID igguse lue lativefinedriver.
 *
 *STS		s
   GGctiviEVEefine CCM_R166itis#define CFC_REG_DEBUG0						 0x signal * [RWlue for QM tes per error (in #cfc_104050
/* [Rt start-up. tes per error (in #cfc__registers_cD					 0xd02tes per error (in #cfc_utputine CFC_ 14] indicates per error (in #cfc_- 255ine CFC_c_error_vector.cfc_error
   vectorlue; sters_cturns the cuFC_REG_DEBUG0						 0x/* [RC 14] C4it description can be found in CFts an intern4REG_DISABLE_ON_ERROR				 0x104044e bits. the 4 14] indicates per error (in #cfc   cr */
#defc_error_vector.cfc_error
   vecto6registers_c5it description can be found in CF7ne CFC_REG_IREG_DISABLE_ON_ERROR				 0x1040448ne CFC_REG_I 14] indicates per error (in #cfcRW 10] WREG_I value for QFoM_OF_PAEG_CONTROL0					 0x10The nuwheW_THRpt regdeDU_RUSE_LOW(0) credr auto- BRB-obin (1)028
#define CFC_REG_DEBUG0			MODE			 0x60151661b 14] indicates per error (it */
. */
#defFC_RE value for bnx2x_G_CCM addr]...UM0_0		lativnal RAMy the hardfc_rsp lciem interface iFC_REG_D/* [RSP_START_ADdefinethis
  stands for k register #0 rd to event ID.ompeM_REG_Pse sig#4028
#define CFC_REG_DCMP_COUNTER */
104050
/* [Rst prioriti[R 1] indication the initializing the link listSTS					 0x60are was done. */
#define_registers_csage length[R 1] indication the initializing the link listts with the rare was done. */
#define/* [RC 14] Cerrupt masC 				 0x104020
/* [R 9] Number of Arriving LCIDs types are:
are was done. */
#definets an internpt registeaccess; data = {prev_lcid; ext_lcid} */
#define CFCs thg thetherwise 0 dit
   counter.re was done. */
#def_REG_LINK_	 0x_SIZE			c_error_vector.cfc_ENet thINFIC0_INIT166disr_EVENT_10				 0xc2060
#defutput iniREG_ANT_EVENT_10				 0xc2060
#dOUTine CSDM_REG_cations */
#define C				 0xc20#define CSDM_ial creditTH_MIS y. */
#d); 2 stand_REG_ACTe initcaseco*/
#define CFpxp			 0ro			 0lative to withouRW 3xd01defiany ACKM_REG_AGG_INT_EVENT_0	ber of EDIT_PXP_CTRLefine CSDM4 List rSTNUM_Oin the QM
   iACK af8
#dplace curr070
#defi 0xd0184
ink List Block */
#dNUM_OFr 2. AF#defPLACthe ie CSDM_y mask efine CSDM_REG_AGG_INpacket end44
#define CSDM_REG_ERR_CCM_parserG_AGG_INT_EVENT_4				 0xc204PKT_ENDc
/*efine CSDM_unter. efine CSDM_REG_AGG_INite cliEVENT_6				 0xc2050
#xp asyncRobi CSDM_REG_AGG_INT_EVENT_7			XP_ASYNC_RErted.ine CSDMdcom Cefine CSDM_REG_AGG_IN_PBF_IFne CSDM_REGinST 32] 4028
#define CFC_REG_D 0xc204Q0the 206c
#define ndex whether the mode is normal (0)
   or auto-mask-moware; you can fine CSDM_REG_AGG1_INT_[RW 16] Lin2ar on r
#define CSDM_REG_AGG_INT_MODE_11				 0xc21e4
#de which are at empty stMODE_12		1	 0xc21e8
#defin
 * WB 
#define CSDM_REG_AGG_INT_MODE_11				 0xc21e4
#deine CSDM_REG_AGG_INT_MODE_12		INT_MODE_10				 0 CSDM_REG_AGG_INT_MODE_13				 0xc21ec
#define CSDM_REG Link List Block */
#d_REG_AGG3INT_MODE_10				 hich thhether the mode is normal (0)
   or auto-mask-moegister (cleaDM_REG_AGG_INT_MO4E_8 				 0xc21d8
_REG_AGG_INT_EVENT_8				 0xrmal (0)
   or auto-mask-moits and it shDM_REG_AGG_INT_MO5E_8 				 0xc21d8
ndex whether the mode is normal (0)
   or auto-mask-mo(write 1 to cDM_REG_AGG_INT_MO6E_8 				 0xc21d8
 CSDM_REG_AGG_INT_MODE_13				 0xc21ec
#define CSDM_REG_BRB1_INT_STSDM_REG_AGG_INT_MO7INT_MODE_10				 
 * it p lcid */
#define CSDM_REG_CFC_RSP_START_ADDR			BRB1_PRTY_MASDM_REG_AGG_INT_MO8e maximum value ocfc_rsp lcid */
#define CSDM_REG_CFC_RSP_START_ADDR			the Low_llfc DM_REG_AGG_INT_MO9e maximum value oING				 0x104018
/* [RW 8] The event id for aggregated T 32] fine CSD			 0xc205c
/* [RW 1]Q 0xc2038
#define CSCFC_REG_LL_IB1_REG_N1]_INT_ctrl rd_able fifo empcredn sdm_dmaEG_LIblock			 0xc205c
/* [RW 1] CFCINT_EVEN_RDATA_EMPTYinterrupt5   if 1 rruptdefine0 read/write */
#dVENT_DM_REG_CSDM_INT_MASK_0				gregaPARSERSDM_REG_CCSDM_INThich the			 0xc22acser/
#d
/* [R 32] Interrupt register #0 read */
#define CSDM_gregaM_INT_STS_0 				 dcom Corpoe CSicky the ated 4] CFC ouApplice CDU_nlectie/
#de~x/
#d If 0 - th_PRTY_tick_6007c
/ad */
#define CSD =16				 0xc21f8
#defineTIMER_TIC */
#defiZE			 * it unded. */
#define BRB1_REG_LOW_LLFC_HIGH_THRESHOLD_0			 _EVENT_0	STS			10] The number oefin9c_error_vector.cfc_LE_OUT1					 0_registers_2G_CDU_SMace #n is asserted. */
#define BRB1_REG_LOW_ENABLE_OUT2					 D_0			 0x6015definit description can control
   in [RW 16] Lin2G_CDU_CHK_11_OF_FULL_BLOCKS				 0x60090
/* [ST 32] The numbeg any ACK. */
at the wrerface witho List raXP_CTRL				 messages that can be sent to the pxp contr */
#define CCM_ */
# * it undor in the QM
   it network_lcid; exreg.h: Broadcyclber of ACK afteopyrighl acCYC the CDSTORM_WEI8_pr.gr_ag_ich may bsourcitisates thssociTROL0CSDMdefine CSDM_Rel0xc2040. Sceivesert eco#defiiG_CQ- foc0; 1-fic1; 2-sleepdefitht pau if *QM - CM I0; 3am ace number of commands received 1; 4- 0 */
#define CSDM_REG_NUM_OF_Qs in Link List SG				 0xc2ELEMENTized to 648essage lengtuests received from the pxp async if */
#define CSDM_REG_N1M_OF_PXP_ASYNC_REQ				 0xc2278
/* [ST 32] The number of commands received in queue 0 */
#define CSDM_REG_NUM_OF_Q0_CMD					 0xc2248
/* [ST 32] The numr #0 Cstandnoserteequaline BRB1_REG_~xsegister #0 rearb_DM_REG_0.mmands receiber of commands received in queuFIC0_INIT_efiner of requests received from the pxp async if */
#define CSDM_REG_N2ands received in queue 11 */
#define CSDM_REG_NUM_OF_Q11_CMD 				 0xc2270
/* [ST 32] The number of commands received in queue 1 */
#define CSDM_REG_NUM_OF_Q1_CMD					 0xc224c
/* [ST 32] The number of commands received in queue 
/* [nd The number of commands rece1ved in queue16				 0xc21f8
s received in queuutput initefindcom Corporationeceived from the pxp async if */
#define CSDM_REG_N3ands received in queue 11 */
#define CSDM_REG_NUM_OF_Q11_CMD 				 0xc2270
/* [ST 32] The number of commands received in queue 1 */
#define CSDM_REG_NUMQ1_CMed; va			 0xc224c
/* [ST 32] The number of commands received in queue 3asserted;
/* [ST 32] The number of commands receine CSDM_REG_Q_COUNTER_START_ADDR		2ved in queuember of commands received in queu- 255.Writefinr on read
 * RW eceived from the pxp async if */
#define CSDM_REG_N4CSDM_REG_NUM_OF_Q4_CMD					 0xc2254
/* [ST 32] The number of commands received in queue 5 */
#define CSDM_REG_NUM_OF_Q5_CMD					 0xc2258
/* [ST 32] The number of commands received in queue 6 */
#define CSDM_REG_NUM_OF_Q6_CMD					 0xc225c
/* [ST 32] The number of commands receiR 1] pxp_ctrl rd_data fifo empty in sdm_dma_rsp bne CSDM_REG_Q_COUNTER_START_ADDR		3ved in queue Link List Blocs received in queulue; read numbit descriptiopyrigh2060
#def34
/* [RW 3]aREG_DISABLE_hat is associaOURTY_STS			bitr
#define CSDM_hs th{prev_lspine 		 0ain thelM_OF_ACK_s0xc22memoriefine CSthe val			 0uto-mnt 0FASTegiste4] MAT- sleepiny 0; 3-
   RW 1} */
by 1; 
/* [p#defix Bal cRR_EVNT_ID	:0]...i48
/em_f. */y 0; 3-
   Theb 1] leepingarityrce _epingy.ion element 1 stands fad_REGfor ach source that is ait. MaxM_REG_AGG_INT_Eopyrigheepi_MEMORT_STS in ta0		 0xc2238
1] Dis07c
efine CC070
#defi_ERR_  inpMT_MASKr by td durdefirunead *sertounter microcbin with priority 0; 3-
 
   DIS						 0x101183_1					 0xd1; 4- sleeping thread with priority12.
   Could not be equal to register ~csem_registers_arb_element0.arb_eleme1t0 */
#define CSEM_R			 0x600d15. */
#definene CDUR pauxc22H_THRE3:0]...id0ble. I] Thpos#defin_ARB_Ecsem_rddCDU_REG_L1worEG_CSDM_INT_MASopyrighT1		T						 0x1011804	 0xc22ST 24]1040tisticne CEG_LOW4] MAT4				 0xc2070
#define CSeativthe iroughsert  inp2.
   Could not be e
/* [RClemencoding is: 0m_registers_arb_element0.arb_element0
   and ~csem_registers_arb_element1.arb_elementeived in queue 7 */
#dARB_ELEMENTCMD					 0xc2n channters_arb_element0.arb_element0
   and ~csem_registers_arwerG_AGG_INTelemeOt1 */
#define CSEM_REG_ARB_ELEMEOT2					 0x20002ndex whety 0; 3-
   sleeping thread with priority 1; 4- sleeping thread with prioding is: 0- foc0; 1-fic1; 2-sleOping thread wit CSDM_REGy 0; 3-
   sleeping thread with priority 1; 4- sleeping thread with priomber of commands receiters_arb_elREG_NUM_OF_Q732] Int*/
#define CSEM_REG_ARB_ELEMENT3					 0x20002c
/* [RW 3] The source that isSEM_REG_ARB_CYCLE_SIZEters_arb_elP_CTRL_RDATA_read */
#defi4- sleeping thread with prior050
#dsssn'tivity c.
   Could notsert e equal to reger ~csem_registers_arb_element0.arb_elPASt0 */
#define CSEM_R				 0xWBt in] Debug* [R . Per ~csem_regisurce
  rs_arb_element1.arb_eleSIVE_BUFFESDM_Rter ~cer. isters_a46] pram ~csem_. B45DU_Rp_FULL; b[4CFC_REGabled with priority 0; 3-
PRAC 2] I in tcth priorit				 Vld. *e number of com_lcidefine C hacsemit proundt can be sent to topyrighSLEEP_THREADS#defregard CSEM_Rar on reEMENEX
   ORE FIFODU_Rd/write */emtworw_ls_xd00SK_1				 0x200120
/* [OW_ine CSEM_r #0 read/wriEM_R_CREDIT_PXP6]0c
/* of #defi	 0x20014] MArn arba_REG_CSEM_INT_Md with priority 0; 3-
] InterrLISecoding is: 224
/* [RW 8] The reg.h: Broadcom Everest network driver.
 *
 *0				 0x20 (c) 2007-2009W 3] dcom Corporation
 *
 * This program is free software; you can  */
#define  it and/or mos: 0
 * it under the terms of the GNU General Public License as pub0134
#define 0] At addresBLE_I Foundation.
 *
 * The registers description starts with the r0134
#define s type folloBLE_I* by size in bits. For example [RW 32]. The access types are:
0134
#define only
 * RC -BLE_Ir on read
 * RW - Read/Write
 * ST - Statistics register (clea0134
#define  W  - Write s: 0
 * WB - Wide bus register - the size is over 32 bits and it sh0134
#define    read/writ CSEMconsecutive 32 bits accesses
 * WR - Write Clear (write 1 to c0134
#define 
 *
 */


/* CSEM9] Interrupt register #0 read */
#define BRB1_REG_BRB1_INT_STS0134
#define 
/* [RW 4] P CSEM mask register #0 read/write */
#define BRB1_REG_BRB1_PRTY_MAS0134
#define 8
/* [R 4] Ps: 0y register #0 read */
#define BRB1_REG_BRB1_PRTY_STS					 0x600134
#define SEM_REG_CSEM_PRT1_IND_FREE_LIST_PRS_CRDT initialize free head. At
   address  */
#define EM_FAST register1 initialize free tail. At address
   BRB1_IND_FREE_LIST_PRS_C */
#define the base addressial credit. */
#define BRB1_REG_FREE_LIST_PRS_CRDT				 0x60200 */
#define . */
#define CSEee blocks above which the High_llfc signal to
   interface #n  */
#define May be updated dRB1_REG_HIGH_LLFC_HIGH_THRESHOLD_0			 0x6014c
/* [RW 10] The n */
#define 00224
/* [RW 1] hich the High_llfc signal to
   interface #n is asserted. */
# */
#define icrocode */
#defW_THRESHOLD_0			 0x6013c
/* [RW 23] LL RAM data. */
#define BR */
#define write access to /* [RW 10] The number of free blocks above which the Low_llfc  */
#define  interface #UM_FOde-asserted. */
#define BRB1_REG_LOW_LLFC_HIGH_THRESHOLD_0			 200120
/*20012T1					 0xc2240
#28free blocks b1] Disables input messaCM header8
   interface #n is asserted. */
#define BRB1_REG_LOW1] Disables input D_0			 0x6015un_t [R 24] TheE					 0x20024c
/* [WB_STORM_WEIthe RB1_REG_NUM_OF_FULL_BLOCKS				 0x60090
/* [ST 32] The numbe1] Disables inat the write_full _REG The source that is b[44:0] - data.during run_tRB1_REG_NUM_OF_FULL_CYCLES_0				 0x600c8
#defineAM						 0x240000
/LL_CYCLES_1	un_tily. Passive buffer memorNUM_OF_FULL_CYCLEine C[R 24] TheMCPR_NVM_ACCESS associamber o (1L<<0) in sem_slow_ls_ext */
#define CWRCSEM_REG_SLOW1EXT_STORE_EMPTY				 DD There is aVALUDU ope(0xfefineLOW_EXT_STORE_EMPTY				CFG4_FLASHhe CDU ope(0x7CSEM_REG_THREADS_LIST			OMMAND_DOIRTY_STS_SLOW4rbitration scheme of time_sloNity. */_SLOW3rbitration scheme of time_sFIRine CSEM_SLOW7rbitration scheme of time_sLAslot 10 */
#d8rbitration scheme of time_sWefine C_SLOW5EXT_STORE_EMPTY				SW 0xc2TS_11_A signa_SLOW91 */
#define CSEM_REG_TS_11_ASREQ_CLR		 0x20006411 */
#define CSEM_REG_TS_11_ASme oSE_CMD		st of free threadBIGMAgateGIS#defBratiCONTRONT_2		(0x00] The arbitrabitration scheme of tXGXStime_slot 1 */
#1efine CSEM_REG_TS_13_AS					 x600*/
#4
/* [RW 3] 05efine CSEM_REG_TS_13_AS					 RXtime_slot 13 */
2bitration scheme of time_slot RX_LLFCy 1; FLDfine */
46 scheme of time_slot 15 */
#defi/
#define CSEM_R23 scheme of time_slot 15 */
#defiSTAT_GR6e bits of 				 0x200074
/* [RW 3] The arbitTS_16_AIPJ				 0x242efine CSEM_REG_TS_13_AS					 T 3] The arbitrati07 */
#define CSEM_REG_TS_17_AS			/
#define CSEM_RE9 */
#define CSEM_REG_TS_17_AS			PAfulf] InSHOL_MOD */
#A */
#define CSEM_REG_TS_17_AS			SOURCELINK_LIST_/* [8ration scheme of time_slot 1 */
#S_16_TBYRTY_S 0x20define CSEM_REG_TS_13_AS					 he arbitraPKon scheme0C] The arbitraEratiLED_1000MB_OVERRID* [RW 3 of free thread The arbitraion scheme 	 0x60me_sl2t 3 */
#define CSEM_Rion scheme of t[RW 3] The arbitra The arbi25ation scheme of time_slo4
/* [RW 3] The arbion scheme of time_s_EXT_STORE_ The arbiTRAFFICe of time_s6me_slot 5 */
#dMDIOof tiof time_s is ES  the (00044					 0x20004c
/* [RW 3] The arbitInte_40
#def(3of time_slot 6 */
#define CSEM_REG_TS_WRITE_45REG_A200044					 0x20004c
/* [RW 3] Tne Ce of ti#definf time_slot 5 */
#d* [RW 3] T_REG_LBUST_STS_200044
/* [RW 3] ration scht */
AUTO_POLot 13 *efine CSEM_REG_G_TS_8_AS					 CL080
/				 0x2 3] Tot 3 */
#define scheme of tiOCK 14 200054
/3RW 31					 0x20004c
/* [RW			 0x20005c
/_BITSHIF/* [ 16e arbitration s */
25G				  time_slot 11 */
#defiASK					 0HALF_DUPLEXe of time_sne CSEM_REG_TS_9_ */
PORT_GMIIe of ti20044
/* [RW 3] The c09c
/* [R 32] Comma00044
/* [RW 3] The ess to commanitrae of ti00050y register #0 read *RESEThe init of time_slot 5 */
#d is aThe arb007-2009lue for QM_MEM_SIZE					6c iniTCHerface wie blocks b the CRC-16c in [RW 3] 	 0x6015a 1] If 0 - the CRC-16c in0 read/w	 0x6015b[R 24] The the CRC-16c in
/* [R 1]0xl zeroes; if 1 - the CRCbitrRW 1] If 0 	 0xd0174
/16 T10 initial valTUdefine CSE0xne CSDM_REG16 T10 initial vaarbitA_REG_f pa
/* [RW 1] nes. */
#define DMAE_REG_2ht_load_cfC_INIT					 0x10201c
/* RW 2] Inter */
# of t23INIT					 0x102020
/* [f tiRW 1] If 0 be is all ones. */
#definhe arbitEG_CRC16T20_INIT					 0x102020
/* [rite */
#dMAE_REG_DMAE2INIT					 0x102* [RW _FINT_Sed withow Y is to calculated0 read *KR 32VLAN_TA
#defime_sloMAE_REG_CMD_MEM_S read *PROMISCUOU scheme* [RW 3] The arSTS					 0x10x10240
#define DMAE_REG_CMD_MEM_S6 T10 init_JUMBO ass 9 */
#define CSEM_REG_TS_9/* [RW _STS_0080
/SEM_REG_SLOWlot 4 */
#defineAE_REG_G/
#define DMAE_REGThe arbitration AE_REG_G */
#define DMAE_REG_GO_C1	_C0	ion schemS_GPIOlized to 12 go. */
#define DMAE_REG_GOFIC0_IN112 go. */
#define DMAE_REG_GOutput irity regis
#define DMAE_REG_GO- 255.W_INT_MASK	
#define DMAE_REG_GOCLR_PO  the _DBG_PRTY_M
#define DMAE_REG_GOFLOA/* [RW 1][RW 32e CSEM_REG_T15 go. */
#define DMAE_R98
/* [RWID				 0x20/* [RW 1] Command 2 HIGH3 go. */
#define DMAE_REG_GO_C13 		INPUT_HI_. If 102094
/* [RW 1] Command 14 go.T1		02098
/REG_A DMAE_REG_GO_C2						 0x1020a0
T1		OUTEG_GCL200c				 0x102090
/* [RW 1] Command* [RW 1] Co/
#def] Command 3 go. */
#define DMAE_R1020a
#define  1] Command 15 go. */
#define DLOW	 0x601MAE_REG_GO_C2						 0x1020a0
W 1] Co/* [_GO_C6*/
#define DMAE_REG_GO_C7						 0x10LOW7 go. */
#define DMAE_REG_GO_C7				/* [Re DBG_RE	 DMAE_REG_GO_C2						 0x1020a0
MAE_REG*/
#dGG_INT_EVE/* [RW 1] Commax1024_SIZE1_CLEAand 5 x58 1] DMAE GRC Interface (Target; asterRSTh (r scheme1#define CSEM_RC Interface (Target; aster/
#defi. If DMAE_REG_GO_C2						 0x10arget; ast2r) enable. If9 1] DMAE GRC Interface (Target; ast2egard of sem iid is d 12 go. */
#define DMAE_R[RW 1] DMAE PCI 				0_HARD_COREace (Requ1	 0x10209c
/* [RW 1] Comma[RW 1] DMAE ted
   as u9ual; if 1 - normal activity. */
#de3r) enable. Ifa 1] DMAE GRC Interface (Target; ast3erfaCh (r_MUX_SERDES0_IDDQ disregareasserted; all other signals are trIFEN					 0x102004
/* [PWRDWNce (RequAE_REG_CMD_ Interface initial credit. Write writes the
   initial_SDe (RequThe arbitratInterface initial credit. Write writes the
   iRSTB_HW e (Request; ead; rite) enable. If 0 - the
 IFEN					 0x10206c
 [RW 4] DMAE- 11 */
#defingation command. */
#define DORQ_REG_AGG_CMD0	nitial value t					 0x200 Aggregation command. */
#define DORQ_REG_AGG_CMD1		 curlid is deasserted; all other signals are trne DORQ_REG_AGG_CMD0			 0x10ce (Requded; valid is deasserted; full is asne DORQ_REG_AGG_CMD0	TXtimeFOAGG_Ce (RequW 3] The arbW 28] UCM Header. */
#defited
   as uration elemrbell address fSG_GOlue; reon 0). */
#define DORQ_REG_DB_   cred5n 0). */
#define DORQ_REG_DB__REG_	 7n 0). */
#define DORQ_REG_DB_02098
/* [RW 1] Command 15 go. */
#defG_DB_MAE_REG_GO_C15 					 0x10209c
/* [RW 1] Commaad */
#def/
#define DMAE_REG_GO_C2						 0x10G_DB__REG_GO_C3						 0x1020a4
/* [RW 1] Comm
#definT_OLD1020bc
/* [5] Interrupt register #0 read */		 0x1020b4
/* [RW 1] Command 8 go. */
#d
#define DOREG_GO_C8						 0x1020b8
/* [RW 1]G_DB_020bc
/* [RW 1] DMAE GHW_20005*/
#RE#definethread. */3RW 1] Comm 0x17018 [RW 8] TEG_G13 go. */
#definhe DPM CID to STOine C12 					 0x1020he DPM CID to STO/* [0_AT0] TheREG_A_INT_MASK	he DPM CID to STOG_DB		 0x102094
/* [he DPM CID to STOUND2] Comm register PRS0x20GitratETH_IPVlue; reRW 1] CommAEUfine DSxtraNefine_BRB_REGITYvityOR		 TH			(egarW 3] The arupt. */
#define DORQ_C*/
#W* [RERRUPTL_TH				 0x1ine CSEM_REG 12] The threshold of DUe DQ FIFO to send the ful
/* [RW 3]  */
#define DORQ_REG_DQ_FDQ_FIFO_AFULL_TH				 0x1007c
/* [RW 12] The threshold of FCe DQ FIFO to send the fulefine CSEM_ng
   pointer. The range DQ_FIFO_AFULL_TH				 0x1					 0x200 12] The threshold of TS		 DQ FIFO to send the fulfine CSEM_RELF					 0x1700a4
/* [R 1] DQ_FIFO_AFULL_TH				 0x12007c
/* [RW 12] The threshold of SEMI DQ FIFO full _TH				 0x17ld; reset on full clear. */
#define DDQ_FIFO_AFULULL_ST					 _EXT_STORE_upt. */
#define DORQ_DEBUb_el header in the case of7007c
/* [RW 12] The threshold ofDMAE DQ FIFO full status. Is 10x1700c0
/* [RW 28] The value seDOORBELLQ DQ FIFO full TH				 0x17The arbitra008
/* [RW 5] The normal mode CDQ_FIFO_AFULLTH				 0x17AE_REG_CMD_upt. */
#define DORQ_EG_G3_FUNCTIONine TH				 0x111 */
#defi TCP context is loaded. */
#define D1			 0x170078
/* [R 13] Current value of the IG FIFO fill level accordinger when only TCP context is loaderbell#define DORQ_REG_MODE_ACTCMHEAD_TX 				 0x17004c
/* [RW 3rbellDQ_FIFO_AFULL_TH				 0x17e CSEM_REG_upt. */
#define DORQ_REG_CSD02c
/* [RW 28] TCM Head2 CFC load error. */
#define DORQPBCLI	 0x02c
/* [RW 28] TCM Headers_rsp_init_crd.rsp_init_crd
   rFe DQ FIFO to send the full interrupt. */
#define DORQ_REG_ch a DQ FIFO to send the full counter credit. Initial credit is
r
   equal to full thresh counter credit. Initial credit isPCIx2000egisterORQ_REG_DQ_FU_NORM_CMHEAD_TX 				 0x17004c
/* [RW 3NT					 0x1700b0
/RR_CMHEAD						 0x1. Initial credit is
   configureQhe DQ FIFO to send the fullame initial credit to the rspa_crdrsp_init_crd
   register. */
#define DORQ_REG_RSPB_CRD_CSEARCHrough write to ~dorq_registP_INIT_CRD					 0x170048
/* [RW 4PIO   cG_REGN						 0x170038
/* [R 4] Current valuTthe DQ FIFO to send the ful2ere each row stands for the
   dSDM_RSCID extraction offset. * */
#define DORQ_REG_SHRT_ACT_CNT 1] DQ FIFO full status. Is sORQ_REG_SHRT_CMHEAD					 0x170054
#der
   equal to full thresh. Initial credit is
   configureCM_R DORQ_REG_DQ_FULL_ST					 set; when FIFO filling level is mREG_MSRR_CMHEAD					 0x170058
hold; reset on full clear. */
#deU					 0x170070
/* [RW 28] TChe
   read reads this written vaUP0x10 0x170070
/* [RW 28] TCM Header when both ULP and TCP c_REGr
   equal to full thresh_DQ_FILL_LVLF					 0x1700a4
/* [U 1] DQ FIFO full status. Is s0x1700c0
/* [RW 28] The value seATTN_r
   equal to full thresh CFC load error. */
#define DORQ CopMSI_ATTN_EN_0				 (0x1<<7)
CMHEAD_TX 				 0x17004c
/* [RW 3e HC_RN_0			 (0x1<<2)
#define H. Initial credit is
   configureV		 0PCIput iwrite
   writes 				 0x17_DQ_FILL_LVLF					 0x1700a4
/* [ */
#FO_FULL_TH				 0x170078
/* [R 13] Current value of the _EVENis 0 - 256 FIFO rows; where each row stands for the
   dT 32] ll. */
#define DORQ_REG_DQ_FILL_LVLF					 0x1700a4
/* [2001 DORQ_REG_DQ_FULL_ST					 00x1700c0
/* [RW 28] The value seC_PRTYRR_CMHEAD					 0x170058
#_EXT_STORE_x102RVED_GENERALxtraENine DBIfine0
* [RW 1] CsendSTne Hefine IN fulfty. */1] Pfrded; ackno48
#defiLl vaEDHC_REG_P0_PROD_CONC15 e0028				 0x108040
#define HC_REG_LEADING_EDGE_6	DBG_PRTY_M40
#define HC_REG_LEADING_EDGE_7	RQ_REG_DOR40
#define HC_REG_LEADING_EDGE_8	GG_INT_EVE40
#define HC_REG_LEADING_EDGE_9	9 HC_REG_STATISTIC_COUNTERS				 0x109000
10	ll zeroes; EG_TRAILING_EDGE_0					 0x1080441	1RW 1] CommEG_TRAILING_EDGE_0					 0x1080442	1rity regisEG_TRAILING_EDGE_0					 0x1080443	1_INT_MASK	EG_TRAILING_EDGE_0					 0x1080444	1[R 24] TheEG_TRAILING_EDGE_0					 0x1080445	1 register EG_TRAILING_EDGE_0					 0x1080446	_DBG_PRTY_MEG_TRAILING_EDGE_0					 0x1080447	1ne HC_REG_PCI_CONFIG_1					 0x108014
#def18	CNT_AUX2_Q	PR_NVM_ADDR					 0x8640c
#define9	1efine HC_REG_TRAILING_EDGE_0					 0x1080420	2efine HC_REG_TRAILING_EDGE_1					 0x1080421	21
chan				mhe perts ane C] actiite CSDM_REG_CMTl prioFATREG_SSEG_LILEADING_EDGE#defi_PCI_CONFIG_0					 0x108010
#defer.
 *
 * CCPR_NVM_WRITE					 0x86408
#define MCP_REG_MCPR_SCRATCH					 0xa000GG_INT_EVEC read first 32 bit after inversion of function 0. mapped as
   folloefine HC_RX read first 32 bit after inversion of function 0. mapped as
   follo10_MCPRmcpted; ing
 0x86420
#d#define CSEMMCPNVM_WRITE					 0x86408
#defcut is EG_TRAILING_EDGE_1					 0x10804c
_MCPE1H NIG dattus VENT_n 1; [8] GmLEME0- fo0014
/4-7iver.
 *
 *LINKgister_LEADING_EDGE_#def_0cut iEG_UC_RAM_ADDR_0					 0x108028
#dPXP Expansion ROM event0;
   [13] PCIE1glue/PXP Expansion ROM event1; [14] S_INT_MASK	sion ROM event0;
   [13] PCIE2glue/PXP Expansion ROM event1; [14] S[R 24] Thesion ROM event0;
   [13] PCIE3glue/PXP Expansion ROM event1; [14] S register sion ROM event0;
   [13] PCIE4glue/PXP Expansion ROM event1; [14] SDBG_PRTY_Msion ROM event0;
   [13] PCIE5glue/PXP Expansion ROM event1; [14] S0
/* [R 32sion ROM event0;
   [13] PCIE6glue/PXP Expansion ROM event1; [14] SGG_INT_EVEsion ROM event0;
   [13] PCIE7glue/PXP Expansion ROM event1; [14] S9
			 0x1080C_REG_P1_PRODRBC200cE_INT_MASK	 read first 32 b#defID				 0x20 read first 32 bed w2t; [24] TSDread first 32 bUion [27] TCM Hwread first 32 b
#de230] PBF
   _REG_P1_PRODSDM_OU16_AC		2_INVERT_1_Fread first 32SVDcp; [5	2efine HC_R; [6] GPIO1 fuO080a0
/* M1
   The source; [6] GPIO1 fUM	 0x GPIO3 functiRW 1] CommGPIO4 function 1x2000PIO3 functiPIO4; [15] _REG_P1_PRODSCPAD [11] PCIE glu3			 0x1080e HC_REG_LEAD_WORD(n 1; _name)cut is  ((94 +event CIE gl / 3. */
#definnsion ROM evenOFFSET[13] PCIE gl\
	(1UL <<   Expansion ROM eve%t1; )
/*
 *1; 2-sfiline  *
 s GRCated with arbne CFeveryng threfor function 1itialclu_REGby chipsim, asmm_registersxc22cppm_registerinterruvailsed);#definM_REG_ERnput u.xmlFC_REGgBed wittribu the* De-sociated w if * Thegener not t. Maxsdefincase they 0; 3-
 inte/e/PXP ExpanRCBASEse ICSNS		00
#d */
#definMI
   ParCICONFIGrror;e CSE TSEMI Hw interruptREGerror;2em_r TSEMI Hw interrowlederror; witt; */
#define MISC_RE1_AEU_AFrupt; */
#define MISDBU_AEU_AF8ER_INVERT_1_MCP 			rbelerror;ATER_INVERT_1_MCP 			DB   inteCTER_INVERT_1_MCP 			ed; v arb [29] TSEMI Hw interrXCMent Her. ity error; [31] PBFR error4rror; [3] QM Hw interSRCHt; [4] rupt; */
#define MIS4
#dt; [4]Parity error; [31] PBTParity 5rror; [3] QM Hw interB				 0x0			 nversion of functio1
  TS_1] XCM Hw interrupt; [UPB XSEMCw int; */
#define MISR 1]I Hw
 Parity error; [31] PBATTNI Hw
 ] Titerrupt; [12] DoorParity D Parity error; [11] XSParity E] XCM Hw interrupt; [CDit af1Hw ins
   follows: [0] MAEerror;Parity error; [31] PBFX] XSE10 Broaux PCI core ParitF0. ma14] Tit; */
#define MISHrupt; [FTER_INVERT_1_MCP 			PXP2upt; error; [3] QM Hw interrBFupt; ] Timers Parity error;XEMI Hw16
   [17] Vaux PCI corontexterror;20] USDM Parity error_EVEerror;			 0 TSEMI Hw interrQUPB Hw FTER_INVERT_1_MCP 			DQerror7] XCM Hw interrupt; [CM_RerrorI Parity error; [11] Xfine XSEerrorB Parity error; [27]/
#defiI Parity error; [11] XSNVERT_2 Broinversion of function 0_AEU	f function 0
_MCPRt. Max CSDM_nfigu: BroadchreadUM0_0		pci corfineEG_LOW_iver.
 *
 *upt;FG; [16] 007-2efine ows: [0]
   PBClVENDOR_ID; [16] 	 0xror;ows: [0]
   PBClDEVICErrupt; [2] QM ParPIO4; [15]   PBClf time_sient Parit [4]mers Parity error; [5] TRQ_RPDM_REGEG_RSPA_CRD_CNTty error; [5] TMEMrror; 7] XSD0x1700c0
/*ty error; [5] TBUSOD_CTER7] XSD. */
#definty error; [5] TSPECIAL2274
/S7] XSDhe
   read upt; [8] XCM ParWI]
   Doorbelle CSEM_REG_ty error; [5] TVGA_SNOOP7] XSD11 */
#defity error; [5] TPis
  NA[7] XSD					 0x200SEMI Hw interrupTEPPING[7] XSDefine CSEM_SEMI Hw interrupre Parity erroW 3] The arty error; [5] T   slB2B[7] XSD
/* [RW 3] [6] XSDM Parity NTt0 */
#defGE_0					 0x1080ty error; [5] T40
#defi[7] sk rfine HC_REG_IN   PBClarbiUSTimers Hw inteDBG_PRTY_M   PBClREVESne Drupt; [2]w inteGG_INT_EVEty errorACHE_LINthe CDU op0xINT_EVENT_1   PBClLATENCYterrupM Hw inderrupt; [30] CCMBAR_1arity re all zeroes; errupt; */
#d/* [RW 1nterREG_AEU_AFTER_INVERT2define MISC] CSDM Parity errad thi_MCP 			 0xterrupt; [30] CCMSUBSYSTEMHw interrupt; [2]ty erped as
   follows: [0] CSEM interrupt; [22eREG_AEU_AFTER_INPari [29	 0x60xREG_AGG_INTHw interrupPted wit0x3CCM Hw interrupt;PM_CAPABILINT_STS0xfine CFC_REockClient Hw interru_VERPB Hrity 3 HC_REG_FUNC_Ny error; [7] CFC Hw
 x2000r;
   [DM Hw interrupt; [; [7] CFC Hw
 40
#defiity err23] UCM Hw interru; [7] CFC Hw
 DSIity err; [25] USEMI Hw in; [7] CFC Hw
 		 0xURRENTity 7lue on the loaHC) Parity error; [13]1_SUP/* [ ty errinterrupt; [16] Vaity error; [13]2s_attn; [17] Fl CDU Parity error; [9] CDU Hw inPM#def_D0ity err8] Debug Parity erSW timers attn_1 func01ity errrrupt;
   [20] USDSW timers attn_1 func02ity err] DMAE Parity
   error; [11] DMA_1 func03_HOTrbellQ [12] IGU (HC) Parity error; [13[27] SW tiC3] T full interrupt. SW timers SRTimers Hw intC_REG_INFO_ func1; [30] Garbi* [Rpt; [8 [12] IGU (HC) Parity SR
   f2060
#w interrupt;
   [20] USDVERT_3_FUNCterrupr;
   [interrupt; [16] VaMSI Hw  Parity
   erront7[2:0] to
/* [R 32] rme_sloC_0			 0xasregar CDU Parity error; mcp. mappedMCAParity 7<<18] Debug Parity ererror; [1] CSarity e] MISC23] UCM Hw interru mcp. mapped64EDGE_ is aCAPlows: [GG_INT_0				P Parity error; [3] SI_PV The Hw inLE5] PXPpct; [14] NIG ParityGRCtration sche0x7fter inversion of[8] ne C4] PXPp
/* [RW 1]  PXPpciCloX read third 32 bit aity error;
   [3]r; [11 mappedset the CDU  Hw frror]
   CSEMI Parity errParity errE Hw interr] MISCupt; [2] PXP Parity eParity err PCIEty. */rent vaerrupt; [4] PXPpciCloParity err even2060
# ]
   Fla1)			 0x1080
   [3] QM Hw ime_slot 13C16C_INIT				
   [3] QM Hw NVERT_3_c0; [ Parity error; [2n_2 func0; [2put Rvity.DErs att23] UCM Hw interruunc0; [24] SW NONNVM_WRIs attn_4 funmers attn_4 func1;unc0; [24] SW 1
   func1; [27] S Hw interrupt; [16unc0; [24] SW UNSUP				 tn_4 funiClockClient
   Hwunc0; [24] SW EG_COWattn_4 funt; [14] NIG Parity6] SW timers att_PENterruptinterrupt; [16] Vasion ttn_1 func0; [c0
/* [R 32] */
USTRprioINTM/
#d; [6]EG_AEU_AFTER_I */
C
   follows: [0] Genw interrupt; [ */
X
   follows: [0] Generror; [3] QM  */
T
   follows: [0] GenREG_A_MCPR ID i:0]..ber ofe IGU1). */
#define/PXP DM_REGT_EViver.
 *
 * */
 Fetlows: [0] tn0;] Timeral attn11; [mal modeTimers Hw intI Par [11] General aMc
/* schem4] PXPp4upt; [chand 32 b_2 read seiver.
 *
 *[8] ; [30]sser4
/*w
   ] Genefter inversioeneral attBAR1define C_C15f time_slot 5 9; [18] General attn20t0 */
#dD[20]General attn21; [20] Main power
   in64upt; General attn21; [20] Main power
   in12817]
nds eral attn21; [20] Main power
   in25617]
0005eral attn21; [20] Main power
   in51217]
4 [23] RBCN
   Latched attn; [24] RBCU M		(5 attn; [25] RBCP Latched attn; [26] GRtche6tched timeout attention; [27] GRC Lat4tcheThe arbitration; [20] Main power
   in8tche8 [23] RBCN
   Latched attn; [24] RBCU 6tche9tched timeout attention; [27] GRC Lat3parit11] RBCR Latched attn; [22] RBCT Latched  0xa4; [23] RBCN
   Latched attn; [24] RBCU La 0xa4d attn; [25] RBCP Latched attn; [26] GRC
 0xa4atched timeout attention; [27] GRC Latche 0xa4served access attention;
   [28] MCP LaG0xa4d rom_parity; [29] MCP Latched um64arity er_REG_GO_C11 		9; [18] GenerEXP; [8]RETng tot 12 */
#define9; [18] Genererror74
/*General attn CDU Parity ereneral attme_sl; [130038
/*/
#define CSEM_eral attn12; [11]
   3] IGU (H[RW 3rrupt;
   [20]ttn18; [17] General atnterrupt;[21] R18] General attn20; [19] General attned re* [RW 3] The ar attn20; [19] General attn attnnds 2] RBCT Latched attn; [23] RBCN Latchatche00052] RBCT Latched attn; [23] RBCN Latch1
   Lserv2] RBCT Latched attn; [23] RBCN Latch3ed red ro2] RBCT Latched attn; [23] RBCN Latchd attny; [ttn; [26] GRC Latched timeout
   attenLatcheThe power interrupt; [21]
   RBCR Latched C
   LEG_A2] RBCT Latched attn; [23] RBCN Latchched rene Mttn; [26] GRC Latched timeout
   atten 0xa454
/power interrupt; [21]
   RBCR Latched version o2] RBCT Latched attn; [23] RBCN Latcheral attn2[24]
   RBCU Latched attn; [25] RBCP L4; [3] Gettn; [26] GRC Latched timeout
   attent] General] GRC Latched reserved access attention 0xa48] MCP Latched
   rom_parity */
PREFET5] Tme_slo[14] General
   attn16; 40
#defi2			 (HC) Parit7)attn16; [15]3General attn17; [16] General a3tn18; [17]
   Gen [31] Generalin d9 clearG_ENY_BY#def19;
   [Latched scpad_parity; 3_FOode CM0; [1000441; [10] General attn123TER_INVERT_al attn1lash event; [18_attn; one in ttn0; [2 clears[14] General
   attn16ne i */
#define M   p[16] General attn18; [3_REG_CO */
#de
#defi_parity (both
   ports);_romPOWor; [
#define rrupt; [28] ed a2eneral [14] Genrded; ackno9; [18] General ttn18;; [19] General attn21; [20] Main powettn18; terrupt; [21] RBCR Latched attn; [22] RBCTttn18; d attn; [23] RBCN
   Latched attn; [2ttn18;  Latched attn; [25] RBCP Latched attn;ttn18; RC
   Latched timeout attention; [27] ttn18; ched reserved access attention;
   [28 functiotched rom_parity; [29] MCP Latched O1 functarity; [30] MCP
   Latched ump_tx_pttn18; [31] MCP Latched scpad_parity; */
#dttn18; ISC_REG_AEU_AFTER_INVERT_4_FUNC_0			 functio#define MISC_REG_AEU_AFTER_INVERT_4_ttn18; 		 0xa454
/* [R 32] read fourth 32 bit functionversion of mcp. mapped as follows: [ function
ral attn2; [1] General attn3; [2] GeO1 functio4; [3] General attn5;
   [4] GeneralPIO3 funct] General attn7; [6] General attn8;  functioral
   attn9; [8] General attn10; [92 General attn11; ction 0 outpuP				 0_0x2000enerat; [28] TSEMI Parity Brror; [29a440
/* [R 32 ParM_IFrror; [2ty in sem_srror; [31]utput w in80
/* [R 32]ine DrighBAinveL73_IEEEB2			0 - the CRC--16c
 0xa06c
#deNABLE1ANversion ofe MISC_REG_AEU_ENABLE1_FUNC_0_OUT_1			 0xa07d
  REG_LINity errfine MISC_REG_AEU_ENABLE1_FUNC_0_OUT_2			 AN		 0xt; [MISC_REG_AEU_ENABLE1_FUNC_0_OUT_3			 0xa09c
MAI funTl attn1_ENABLE1_FUNC_0_OUT_0			 0xa06c
#d PBF all zeroes; EU_ENABLE1_FUNC1
#deADVutput arbMISC_REG_AEU_ENABLE1_FUNC_0_OUT_7OUT_Ritratierror; [dc
/* [RW 32] first 32b for enabling the out_KX  inter function 1 output0. mapped
   as follows:G_KX4M Parications */
function0; [1] NIG attention for
  R_AEU_AFT_ENABLE1_FUNC_0_OUT_0			Re CFC_ attb function 1 outRX0/
#defin[23] SW e MISC_REG_AEU_Eunction 1; [7_SIGtn_4al attn1function 1; [8]
   GPIO3 fune DMGene038
/* MISC_REG_AEU_ENABLEunctionEQ_BOOine CSmapped as
   ; [11] PCIE glue/PXP_EQUALIZot 1#def			 0x1RQ_REG_DORQn1; [12] PCIE glue/P [16] _EVENT_IO2 f3 function
   1; [5] GPIO PBF  attcPIO4 function 1; 1CIE glue/PXP VPD event function1; [1unction 1; [1PXP
   Expansion ROM event0; [13] PCIE gor function 1;ion ROM event1; [14]
   SPIO4; [15] SPIO5; 7			 0x8 Store chann PCIE g2CIE glue/PXP VPD event function1; [1
   SRC Hw inPXP
   Expansion ROM event0; [13] PCIE gy error; [25] ion ROM event1; [14]
   SPIO4; [15] SPIO5; - 255SRC rded; ackno PCIE g3CIE glue/PXP VPD event function1; [10] PBF ParityPXP
   Expansion ROM event0; [13] PCIE grrupt; */
#defion ROM event1; [14]
   SPIO4; [15] SPIO5; _A8
/* [SRC tch (relati PCIE g11c
CIE glue/PXP QM Pavent function1; [1E1_FUNC_1_OUT_2	PXP
   Expansion ROM event0; [13] PCIE gEG_AEU_ENABLE1_FUion ROM even1; [14]
   SPIO4; [15] SPIO5TO4 functiot start-up. PCIETXD_RX_DRIVor; [31][30] PBF
  0xa16c
#define MIS MISEMPHASISEMI ONS		f GPIO4 function 1_1_OUT_7			 0xa17c
/* [RW e DBG_RSPIO4; [15]bling the output foI7			 0xtion of0x0f32b for enabling the output foIG attene DBG_R 1] DMAE GR NIG attention for
PREG attentionM PariMISC_REG_AEU_ENAion 0; [3] GPIO2 functione DBG_Ron 0). */
#
   0; [5] GPIO4 fuFULLSPDOD_CONS		000; [3] PXP n 1; [7] GPIO2 function 1; [gate nigfunction 1; [9] GPIO4 functioCBUF1 1; 1
#define MISC_REG_AEU_ENAB[16] MSI/turns the cn 1; [71define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_7			 0xa17c
/* [RW 32] first 32b for enabling the output for close the gate nig. mapped
   as follows: [0] NIG attention for function0; [1] NIG attention for
   function1; [2] GPIO1 function 0; [3] GPIO2 function 0; [4] GPIO3 function
   0; [5] GPIO4 function 0; [6] GPIO1 function 1; [7] GPIO2 function 1; [8]
   GPIO3 function 1; [9] GPIO4 function 1; [10] PCIE glue/PXP VPD event
   function0; [11] PCIE glue/PXP VPD event func[22] SRC 
/* [RW 1] n 1; [72define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_7			 0xa17c
/* [RW 32] first 32b for enabling the output for close the gate nig. mapped
   as follows: [0] NIG attention for function0; [1] NIG attention for
   function1; [2] GPIO1 function 0; [3] GPIO2 function 0; [4] GPIO3 function
   0; [5] GPIO4 function 0; [6] GPIO1 function 1; [7] GPIO2 function 1; [8]
   GPIO3 function 1; [9] GPIO4 function 1; [10] PCIE glue/PXP VPD event
   function0; [11] PCIE glue/PXP VPD event func]
   TSEMut receivinn 1; [73define MISC_REG_AEU_ENABLE1_FUNC_1_OUT_7			 0xa17c
/* [RW 32] first 32b for enabling the output for close the gate nig. mapped
   as follows: [0] NIG attention for function0; [1] NIG attention for
   function1; [2] GPIO1 function 0; [3] GPIO2 function 0; [4] GPIO3 function
   0; [5] GPIO4 function 0; [6] GPIO1 function 1; [7] GPIO2 function 1; [8]
   GPIO3 function 1; [9] GPIO4 function 1; [10] PCIE glue/PXP VPD event
   function0; [11] PCIE glue/PXP VPD event fu06c
/B2000efine ] GPIO4 function 1 TSEMI006c
/* [RW 3] T1; [14]
   SPIO4; [15] SPIO5 [28] TSEM#defing onrror; [29]
   TSEMI1_LANEansio4 functpt; [24] TSSC_REG_AEU_ENABLE1_PX[16] MS 1] Command C_REG_AEU_ENABLE1_PX7			 0xAEU_ENABLE1_FUNCG_AEU_ENABLEPRB7] GPIO29BF Parity error; [31] PBF Hw interutpu0x8nt to 4 of t19c
/ws: [0] PBCdefinN_SWA
#deIO2 function 1; [8 PBClient Hw interruptC_0			 0xrity error; [29]
   ror; [3] QM Hw interrup one i_0			 0Generaity error; [1] PBClient Hwnitierrupt; 0xdefine HC_R[7] XSDM Hw interrupt; [8] pt; [4] Timers Parity error; [5] Timers HUNIG_1 	84
/* 0] PBBF Parity e; [11] XSEMI Hw
   interrupt; [12_CX4 [5]  GPIO3glue/PXP VPD everror; [13] DoorbellQ Hw inteHIGIGpt; [14]
  QM
   Parity error; [3] QM Tdefic09c
NABL 			 0xENABLE1_FUNC_0_OUT_0			GFC_RE clearent Pention for functror; [19]_64
/ANnc0; [2BLE1_PXPB   interrupt; [20] USDM Parity error; 0_OUT_1UTONs doOMPLETDM P
   NIG Parity err; [22] UCM
   Parity error;3723] UCM Hw interrupt; [. mapped
   as ; [22] UCM
   Parity errosion R1; [7] pt; [orbellQ Parity ; [22] UCM
   Parity errofine DParity error; [2] GPIO1 funct; [22] UCM
   Parity error; [2MR_LP_Nrity  XSDM Pore Parity error;
; [22] UCM
   Parity error; [2C_0_OUBAM0			 0xa07w
   interrupt; [20] USDM Parity error; 10 				RSOLUine DTXSme oction1; [2] GPIO1 funabling the output for function 1 output0R mapped
  y error; [31] PBnabling the output for fuACTUALupt;E [8]
 nter3unction0; [1] NIGerror; [3] QM Hw interrupt; [4] Timers */
#t for function 1 outHw
   interrupt; [6] XSDM Parity error; 0Mnterent to 4 of tupt; [8] XCM Parity
   error; [9] XCM Hw intient H MISC_REG_AEU_ENAerror; [3] QM Hw interrupt; [4] Timers2_5Gupt; 3ellQ Parity error; [13] DoorbellQ Hw interrupt; [14]
 5ient Hrupt; */
#de error; [13] DoorbellQ Hw interrupt; [14]
 6ient HAM 	; [5] Timers Hw
   interrupt; [6] XSDM Parity error; [16] r; [9]  [19] Debug Hw
   interrupt; [20] USDM Parity error; [21CX4pt; 7w interrupt; [22] UCM
   Parity error; [23] UCM Hw inte221] USDM  inversion oty error; [25] USEMI
   Hw interrupt; [26] UParit
#dew interrupt; [22] UCM
   Parity error; [23] UCM Hw inte3ient HAw interrupt; [22] UCM
   Parity error; [23] UCM Hw interity erBw interrupt; [22] UCM
   Parity error; [23] UCM Hw inteity errC10] XSEMI Parity error; [11] XSEMI Hw
   interrupt; [12]] NIy erro [19] Debug Hw
   interrupt; [20] USDM Parity error; [21  fu; [16]U_ENABLE1_FUNC_0_OUT_0			 [21PARALLELneraEped nt P The source19c
/QM Hw interrupt; [4Hw ineraterrup 0xa07c
#dParity
   error; [5] Timers Hw
   interrupt; [6] XSDM PHw iDET [21Eefin error; [7] XSDM Hw interrupt; [8] XCM Parity
 sionXCM Pfine DMAE_R] XSEMI Parity error; [11] XSEMI Hw
   i5c
/* (0xb7] SW tENABLE1_FUNC_0_OUT_0			004
/*_DIGITA; [30]8ity error; [15] N5] NIG Hw inte_AitratParity er#define MISC_REG_AEU_Ere Parity error;
   [17] Vaux P_FIBtentAE_REGpt; [24] USEMI Parit [18] Debug Parity error; [19] DTBI_IFQM Pariy error; [27] UP [18] Debug Parity error; [19] DSIGNA Hw
   infine Mr; [29] CSDM Hw in [18] Debug Parity error; [19] DINV				EMI Parity erM Hw
   interrupt; */ [18] Debug Parity error; [19] D3] Uon 1; [9ore Parity error;
 [18] Debug Parity error; [19] DMST Hw
  QM Pariention for functre Parity error;
   [17] Vaux ClientParity
   error;0f0
#define MISC_REG_AEU_ENABLE_PRL_Drror; nterrupt; [20] USDM Parity error; [21] USDM Hw inte2ity FSation interrn1; [2] GPIO1 fure Parity error;
   [17error; [21 DoorbellQ Parity  Hw interrupt; [2] QM
   Paritefine DBG_[25] USEMI
   Hw interrupt; [26] UPB Parityity erroTimers ParM Parity CSDM
   Parity error; [29] CSDM Hw in; [7] XSDM Hw e DBG 		upt; [12] Doorb  error; [9] XCM Hw interrupt; [10] X [29]rupt; [8] XCM Parity
   error; [9] XCM Hw interrupt; [10] X2] DParity error; [31] CCM Hw
   interrupt; */
#   NIG Parity eoutpu; [28] CSDM
   Parity error; [29] CSDM Hw in Parity error;
  [17] Vaur; [1] PBClient Hw interrupt; rbelBLE1_PXP CSDM
   Parity error; [29] CSy err_REFCL[15]on ROM 17] 16]  USDM Hw interrupt; [22] UCM
   Parity error; 25 Debug Hw
   interrupt; [20] USDM Parity errrity error; 
   [17] rror; [1] PBC UPB Parity error; [27] UPB Hw interru   Hw inarity error; [7] XParity error; [27] UPB Hw interru56
   Hw ininterrupt; [21] CCM Hw
   interrupt; */
#define MIS87_  Hw in] GPIO4 function 1rupt; [22] UCM
   Par one i[10] XSE; [30]ity error; [31] CCM Hw
   interru enabling the outpinterrupt; 0fa1a0
/* [RW 32] third 32b for enabling the outpQ Parity er0xa1a0
/* [RW 32] third 32b for enabling the outp [3] PXP H1				 0xa190
/* [RW 32] second enabling the outpity ] UCM
   Parity error; [23] UCM Hw i enabling the outp [21] US [7] CFity error; [11] XSEMI Hw
   inity error; [9] CDU Hw pt; Timers Hw
   interrupt; [6] XSDM Pariity error; [9] CDU 2terrupt; 0xa0fc
#define  [13] IGU (HC)
   Hw interrupt; [14;
   [5] PX_1				 0xa19c
/ [13] IGU (HC)
   Hw interrupt; [1nterimers EU_ENABLE1_FUNC [13] IGU (HC)
   Hw interrupt; [1 [3] PXP H] USDM Hw interrupt; [22] UCM
   Parror; [9] CDU or; [7] CFtput0. mapped
   as follo sch errorupt; ention for funct[26] SW Hw CTL_3_44] PXPpoorbellQ Parity  [27] SW timers at_Mad thty. */
#x108401; [28] SW timers attn_3
   func1; [29] go. */
#0xa0fc
#define  [27] SWUP PBF Hw _REG_MCPR_Ndefine MISC_REG_or; [3] ut for close the gate p0			 0xa074
efine MISC_RE. mapped
   as 0			 0xa074
or; [ IGU (HC) Parity error;0			 0xa074
[12] the outp
#define MISC_RE1 output0. mapp[RW 117] Vaux PCI core Hw i1 output0. ma4] MIU_ENABLE2_NIG_0				 0xa] PXP
   Parit			 0xa084
/*n1; [2] GPIO1 fu1 output0. ma0; [2 CSEMI y error; [31] PB1 output0. maefine MISC_[10] XSEMI Parity1 output0. maing the out MISC_REG_AEU_ENAe MISC_REGMISC_REG1ACDU Hw interrupt; [10] DMIO2 function 0; [4[20]
   MCP attn1; [21] DMAE Hw intupt; [12] IGU (r funAGG_INT_EVEr; [13] IGU (HC)
a17c
/* [RW 32] fi; [143Ctn_4 func1; [30] GeneraUP- 255. USDM Hw interruptn; [17] Fla16] Va2he iniSC_REG_AEU_ENABLE3_FUNC_0_OULPREG_AEU_ENABC attn1; [21] SW timers attnMAE ParitD[22] SW timers attn_2 func0;_FUNADVnt; [19] tion off
   f; [1] CSEMI Hw _3 func0; [24] C Hw interrupt; [16]
  7interrupt; [6] CFC Paritrs attn_1 func1; [2 go. */
#arity error; [13] IGU ( attn- 255.ventE14]
   SPIO4; [15] SPIO5;EMOTErity   timers Parity error;n1; */
#deue of ion 1; [7] GPIO2 function 1; [8]3_FUNC_1_OUT_0			 0xa114w inteFeateECEIefin schtn_4S] PBF ne MISC_REG_AEU_ENABLE3_FUNC_1_OUT_1			 0xa124
/* [RW 32] thBRCM_OUIb for enHw iENABLE1_FUNC_0_OUT_0			0
/*NO_C10 GEU_ENA83INIT_REG			19c
/*rrupt; [2] P_MP5P Hw interrEVENT_2IO2 function 1; [8 PXP Hw interrupt; [4] PXPpciClor;
  [RW 4] pt; [24] USEMI Parit;
   [5] PXPpciClockClient Hw intTETON#def [7] CFC ENABLE1_FUNC_0_OUT_0			 0xa0USERdefi   P2] PCIE glue/PXPMAE Parity
w interEVENT_2	IO2 function 1; [8 interrupt; [12] IGU (as
 AT10x10ut for fun [RW 32] third 3 interrupt; [12] It; [1neral a0dc
/* [RW 32] firstinterrupt; [16]
  M
   Parity _CHEENABLCFC Hw
   interrupt;7] Flash event; [18] SMBAN_GOOD] MCP [7] 37ty error;
   [17] Vaux7] Flash event; [ PXP_ENAB		 0xa1error; [15] MISC Hw interrupt; [n_3 func0RST; fine D			 0xa1a0
/* [RW 324 func0; [25] PERST; [26] SW
  t; [ne DMNGW 16] arity error; [31] CC4 func0; [25] PERST; [26] SW
  _OUTfine B; [1[28] CSDM
   Parity er4 func0; [25] PERST; [26]3		 0xa1orbellQ Parity fine MISC_REG_AEU_ENABLE3_ fulfABLE1HCD_arity err0CIE glue/PXP VPD event fuA [312000 inteFFDtn_4 func1; [30the outpuxp. m[17]
  al attn0; [31] General
   attCORW 16c
#
   eFFE function 1 outp] CSEMI Hw*X+Y.ttn_1 func0; rity error; [13] ] CSEMI Oor; [3] PXP _Mty eW 32_SP Parity er masrupt; [4] PXPpciClockClient Parity error;
   [5] Pr) wht for function 1 outpt; [6] CFC Parity error; [7] CFC Hw
    intl
   attn1; */
#defity error; [9] CDU Hw interrupt; [10]    interlient Hw interrupt; [6] CFC Parity erroion  error;nc0; [2 [20]
   MCP attn1iClockClient Parity er0xa08c
#defror; [9] CDU Hw interruterrupt; [16]
   pxp_mi#definene MISC_REG_AEU_ENABLE1iClockClient Parity erLOOPBAENABLE  func1; [29] SW timeerrupt; [16]
   pxp_misc_eneral attn1ash event; [18] SMB everror; [ 1; [7] GPIO20dc
/* [RW 32] ffunc0; [25] PERST; [2M
   P3.arthe output for function  func1; [27] SW timers 3] UCM Hw interru_ENABLE2_NIG_0				 0xa func1; [27]0x200CM HADVn_2 func1; [28] SW tim[31] General
   attn1; *] MISC Parit1; [30] General attn0; [31] General
   attn1; **/
#define DBG_ error; [13] IGU (HC)
   Hw i1a4
/* [RW 32] f0080
/interrupt;10_INIT					tput for function 0 output0.mapped
 NPD eventP Hw interrupt; [4for function 0 output0.mapped
 SYMMETRM_REG Hw interrupt; [6] Cfor function 0 output0.mapped
 AGeneral attn7; ; [15] MISC Hw interrupt; ion 0 output0.mapped
 BOT[RW llows: [0] General attn2; [1] General attn3; pt; [2] P Debug  SW timers attn_4 func0; [25]
   attn1attn_2 RTNER_interru1nterrupattn16; [15] General attn17; [16] General attn18; [17]
upt; [2] PXeneral attn16; [15] General attn17; [16] General attn18; [17]
r 2.timers attn_2 func0; [23]
   SWatched attn; [22] RBCT Latched attpped
   as llows: [0] General attn2; [1] General attd attn; [25] RBCP Latched atneralttn4; [3]
   General attn5; [4] General ad attn; [25] RBCP Latched atneral26] GRC
   Latched timeout attention; [27] GRC Latched reserved/
#definr; [5enabling the output for function 0 output [22] RBCT Latched att MISC Paine MISC_errorWhenthelinkpartnerisin
   [obin(bit0=1),t] Pabit15=98
#,_OUT2=duplex0xa0s11:10=speed0xa0b4= 0xd0214
/*.
TheOW_TH MISarereserved (0) stanbezero function 1;; [21] RBCR Latched attn; [22] RBCT Latched att
   [5[RW 4RW 32] teral attn19; [PMAP 		A24
/nter/*ieee_FUNC_0_OUT_6			h 32as doevent1;W timers attn_4tion 1 oarity erroglue/PXP VPD evtion 1 ointerond 32event0; [13] PCIEtion 1 oe DMDinter
/*bcm output for function 1 oBed; tput0.ma09_1				 0xa19c
/tion 1 oFEC7; [6] Geneab attn4; [3]
   General atALARn7; [6]0x9 SW timers attn_4tion 1 oLA2] revent1;9y error; [15] MIS9] General attn1n13; [1upt; [12] Doorbtion 1 oTGeneral attn15orbellQ Parity  [11] Generalarity e3; [1neral attn19; [tion 1 o_1_OIDADINFIER	 zerneral
   attn12; [11] Gety error; [10] c8tn_2 func0; [23]0] Main power
  ; [17]
  c80LE3_FUNC_0_OUT_al attn16; * [RW_DOWefinca12] General attn14; [13]CMU_PLL- 12 fun] RBC attn; [23] RBCN
   Latrbell attn13;ca0aattn; [23] RBCN
   Latne Hention; [2
#define MISC_REeserved access a]
   Ge ROM FIFO ParM MISC1 0 - the ackMCP Latched rom_parity; [MICRU_ENr; [1] 018] GRC Latched reserved M805MISCGne ME9] Cca SW timers attn_ity; */
#define M3 mc_REG_AEU_;
   [14] General attn1 [8]rd 3ttentio attn; [23] RBCN
   LatUT_2			eneraca1] GRC Latched reserved EDC_FF
   I24] RB1neral attn10; [9] GeneratcheANDWID [30]ca1CCM Hw intUT_5			 0xa168
#dss attentio function 1; [9d timeout attenti0ntercaE_INT_MASK	tn12; [11] GenRrupt; [6T_7	3; [1] CSEMI Hw tion 1 ouDRdefine MISC_REG4al attn8;
   [7] Generat attenti
   Gca8upt; [18] Debug lows: [0]FP_TWO_WIRpciClockeneral attn16; [15] ; [2] General attn4; [3]
nterrupttn; [26]_INT_EVENT_1attn3; [2] General attn4; timers ID4] TimP Hw interrupt; [4] General attn9; [8] Generalw interrupt; [ attn17; [16] General al attn9; [8] General NG_GOGion ss attn_2 func0; [23]ral attn14; [13] General attFAIpt; [General attn8;
   [7] General attn9; [8]  Lat5c
/*eneraENABLE4_FUNC_1_OUT_0			neral attn4; rityINK_Lenera	 0xa188
/* [RW 32] fou8726al attn4; erruptUFatcheention for functn; [22] RBCT Latched attn;  Pari9;
    Latched attn; [24] RBCU LXU_ENABLE; [27attn2; [1] General attnC
   LatchedUNC_1_O01] General attn3; [2] Ge8727ttn9; [8] GLAVne CSEMeneraneral attn19; [18] Gene Latched rom_pttn; [23] RCSEMatched ump_rx_parity; [30] MCP
   Latched[25] RBCP Latched attn; [26] GRC
 7ut attention; 836] GRC
   Latched timeou Latchatched timeoutENABLE4_FUNC_1_OUT_0			IG_1				 0xaed access  32] fourth 32b for enabliPCS_OPOM event1;upt; [21] RBCR Latched att_0				 0x10llows: [0]e* [RW 32] fourth 32 Gene073_CHI0] G*/
#: [0] attention; [27] GRC LatattnTimers
   Parity erroBCN
   Latched attn; [24] RattnXAUI_WComm[7] 4CIE glue/PXP VPD General710UPB [2] Q0xine  attn11; [10] General
   7 arbiCNTterru_parity; [31] MCP Latched   at			 0xa130on for funcn15;
   [14] General UNC_1_027l attn4; [3]
   General 481_PMDB Hw innter8   timers attn_1al attn19; [1LED1; [6] Gea8CSEMI Hw inneral attn21; [20] Ma2n power
   P Latched attn; [26] GRC[20] Ma3n power
  ue/PXP VPD attn; [23] RBCN
  EMI Parhed attn; or close the gate pxp.ma[20] ion Rneral attn3b/* [RW 32] fourtWIRQ_Rfor enabateseneral attn6; [5] Gerve] General 4; [13; [12] General attnatched rom_pa; [17]
   Gener close the gatCved access a	 0xa188
/* [RW Cched rarity error;N
   Latched attnarity; ttn18; [17]
   General attn19; [1arity;    atDS27. */
#	0xrrorne MISC_REG_AEU_ENABLE4_PXPSPI0x101a8
/*ral attn16; [15]general attentionsione CSD0xE12y
   error; [11it 94 in the aeu
29] MCdefi(11 */
#definne MISC_REG_AEU_GENER			 08 bit ve0tor */
#define MISC_REG_AEU_GENERNERAL_ATT of tim2060
#d 0xc(0064
/* [RW efine MISC_REG_AEU_GENERAL_ATTNBULK_ERupt; MDce (RCefine CSEM_Refine MISC_REG_AEU_GENERAL_ATTNnterr;
   _3 f
#deo the creditne MISC_REG_AEU_GENER LatM Pane CNSFtn18ATTN_1			EU_ENABLE1_FUNC_0Xved access aorbellQ Parity eS68
#dSEQUENCor; imers Parity error; [S_SFX   at06c
/ PCI1[11] Ga#define MISC_REG_A8706 1; [5] GPIO4 ctioninterrupt; [21]TTN_6				 0xa018
#d MIS8_AG_C_REG_AEU_GENERAL_ATTN_7				 0xa2 SRC Pfine MISC_REG_AEU_GENERAL_ATTN_8		3 TSEMIfine MISC_REG_AEU_GENERAL_ATTN_8		Afine Mctched ump_tx_paANed access a7ing the output for funcASC_RE7; [6] Gene* [RW 1] set/clrnput foarity error; glue/PXP VPD ev:
   0= do no_1			 4 func1;ne MISC_REG_AEU_ENABnput fo0.mapped
or; [15] NIG Hw interrtion0; [1] NIG a NIG attentror;
   [17] Vaux; [2] GPIO1 mcp; eral attn10ty error; [27] UPB H; [2] GPIO1 mcp; neral cond 32b for enablin; [2] GPIO1 mcp; 8]
   GPIIO3 function 1; [9] GPIO4 f interr   timers attn_1tion0; [1]enerao
  ; [11] PCIE glue/PXP LPn17; [16]n18; [REG_AEU_ENABLE4_nput foMI Par[30] MCP
 002lingeneral attn6; [5] GPCIE glu   Parity  [1183ttn13; [12] Genenput forinteABLE   error; [11] DMAE Hwfor mcp; [17ttn;rs attn_4 func1; [30for mcp; [17FC_] Thx1084I/X indication for mcp; [17nter XCM ffeatched ump_tx_panput foal atiClocktimertput0. mapped
   Parity [20] MGAC_OUTal attn BRB Parity error; [19] BRBTSDM Parity 0_OUT_upt; [20] PRS
   Parity errorTSDM  ParOM event1;ffeLE3_FUNC_0_OUT_pt; [28] TSEEXPANPB HwU_ENAD_RWrror;for close the gaw interrupt; [30] PBF Pari		 0xa1a8fffarity; [31] MCP; [26] TCM
   ParitSHADG_GOVERTEK				 0xa61 Fet PCIE   Pw interty error; [1 Fet is a eve  interrupt; [8] C32b for iParin; [23][9] CDU Hw in32b for iPRODmess each bglue/PXP V32b for iT 					 0x10Deach bunction1; s follows: [0] PBClunc0ach b_INT_MASK	s follows: [0] PBClmmanach ba440
/* [R32b for iCOALESCess rror02for close t32b for iSIM [8]
   GPI2n; [17] Fla interrupt; [6NOtn; [26]2parity; [3132b for inve_Cty; [202
#define MIrity error; [ is aLOCM Hw
. mapped as followsXSEMI ParHICM Hw
ty
   error; [1] PBr; [		 0x2M Hw
ue/PXP Expa10] GenC_0			 0x	   interrupt; Parity erro E glue/PXP Verror; [N NIG		ty
   error; [6] Vauxutpuue/PXP Expapt; [24] Uterrupn; [XP
 .mapped
   a core Hw intvert; 1= [18] Da440
/* [R core Hw int: [0] PBClien18] Dws: [0] NIG20] USDM Parity error[2] 8] Dfine MISC_errupt; [22] UCM ParitCLtes nterrupt; [4]errupt; [22] Parity erro[18] D			 0xa0f4
errupt; [22]t; [6] XSD;
   [2SDM Hw interrupt; [22]DM Hw inter#defixa12cllQ Hw interruritya23c
/*  interrup interrupt; [30]  evenCCM Paritg the input for funinterruptUPPor; [3fine 7P Latched U_INVERTER_2_40
#definFUNC_0		CP Lff; [31] CCM Hw intePBAerrupt; */
#deach bit:
   0= do n10] [7:0] = mFUNC_0			ach bit0
#define MISCMD_2] SW inputMPnvert; 1= invert. mapped as fo10] [7:0] = m_INVERTER_2_FUG Pari	 0xa240
/* [RW ved.errupt;mask 8 attenrupt; */
#deISC_REG_AEU_MASKFUNC_tionISC_REG_AEU_MASK_ATT + /
#deBcore CID  * ENT_7			ORonseroughTH			 EXT_STORE_ISC_REG_AEU_MASK_INVERTER_2_FUNCNC_160
#define MISC_REG_Etn_1rt; 1=xa23c
/* [Ror; [19] Debthe AEU when a systet a system kill owhen a system
  define MISC_REG_AEU_SYS_KILL_OCCURRED				 0xa610
/* [RW 32] Reprwhen a syste_INVERTER_2_FUNll o90
#define MISC_REG_fine DOReset in po kill orrupt; [12]PIO3 mcp; [5] GPI ROM_2_FUNC_1		5a interrupt; [1 mcp; [5] GPI0209ion 1; [8] GP CDU Hw int Fetity erSR_MDPC_W The of the inp5a; [2] QM Parit VPD event function0LSBp; [3] GPIOration elemPD event
   function1; [M2] PCIE glue/P Timers Hw int VPD event funcinter0; [11] PCIE 6 PCIE glue/PXP VPD_INVERTER_2_FUNC_ GPIO	 0x] UPB PariDeventne DNUMug H0 waAG102094
/* [or function 1; [18		 (AG 4R 32] for St equ-to-rruprev_l[31:8] = CID (orit240
#de)] PRS Parity error; [21]7:4 Hw R3] Th] PRS Parity error; [21] each= Typ[25]ver.
 *
 *or fupt r; [13(_cid, isteron, _age ation(TCM P)r
  8) | M Hw 3] Th)&0xf)U_AF; [28]  [27]  Par)EXT_STORE_or fCRC8TCM Parity error; [27] TCMcalc_crc8(terrupt; [26] TCM Parity error; [27], RBCP Hw interrupt; RSRV [31]UEctivi_ TCM Parity error; [27] TCMc
#d; [28pt; [30] PBF Parity error; [31) & (HC)STATUS_0				 0xa600
#define MISCB(
   LL_STATUS_1				 0xa60 [29] TSEMr
  3 error4
#define MISC_REG_AEU_SYS_KILL_STATUSTATUS_0				 0xa600
#INupt rAn20;ONTine defin(_val) 0xaC_REG& ~c
#d8 cle0
/* [R 8] These bits indicate the metal revision of the chip. This value
   25] D} */
#dion:
 *w inCalculsem icrc 8FC_Raorit0				ue: polynom/
#d0-1-2-8r each abin was transayer		 0xc2VeriloginterRm theor e
/* [R 16] These bits indicate the part number for the chip. */
#define MISC/
al aic inl*
 *u8 F Hw
   inu32rrupt,		 0xrc)
{
	u8 D[32];icateNewCRC[8 base ron of the crc_resbase ri;

	CPR_pliG_XX_Fable - CRD	10
#define	nals(i = in i < 32; i++) {
		D[ir; [(u8)(able & 1);
		able =e A0 t>> 1;
	}s at 0x0 for theape-ape-onitialincrements by one for8ach
   all-Cayer tape-&P_REV	ape-nt 16 HIP_REV				evisionor; [D[31] ^can 0e contb_eled by3one dribe cont19 One in one
tcut iD[y mane in4that th2that t one dr7that t that tolledC[is d represCriate C[7 basevision1nt can olled by each
 y one driis driviver onlyr contry. On represen2olled b  bitth aapproprt that th5that this drivever  representr contr each
  that thate client (Eient (Ex: b C[ame it 5
   is ns this dr2nt canient number 5). aame resaddr1 = sr contrs will gie clients tstatus.
   to addrID control the  that tpropriatate clientit 5
   iient (Ers isol all the ns this dr3ver control client numbhe otherrite comver onladdresses tha representlt = status.l rers is c each
 approprver only drivers iit 5
   iitten t gainthe clthat their
   aRC Hwan be controlled byapproprsn't freeis drive each
  appr representame result = stapropriate bit in all tis drivuest to gai) one will be wrt it alver o is set means this dr5MISC_REGISTERSy one driGENERIC_rite comm both addresseso.
   if the aame resute bit is set (the driame res   free all 
   clit 5
   i~MISC_R all theans this dr6propriate bit (in the whe other. One in each
   will give t  if the appropriapropriate bit i that t controls thir appropx: bit 5
   iat driver re7ver control client numbGENERIC_0 =
   cm both addresses wilr request to free a client
   it appropriate clready c~MISC_Re clients that th
s anlue
by onng driver registers(1...1ers any cor|= (evisioni]r
  i)reseom thes value
  }


