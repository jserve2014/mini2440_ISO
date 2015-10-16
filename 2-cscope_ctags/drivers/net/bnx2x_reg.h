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
   in this register. addres 0 - timer 1; address - timer 2ï¿½address 7 -
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
   CBroadcis transaction based. /* bnx2x_reg.h: e; youom EvereINITIALork driver.
 *
 1cCopyrigh24] CIDder Cport 0 if no matchand/or modify
 * it uID_PORT_0	(c) enera0fPublic Li32 (c) 2CM headers pubflusnneral* Thwhere 'load existed' byou ngramftwafolloresponseu careset and packet typyrigh0. Usedze iccess tstartter Acceits.to TCMThe  Software FoundatiM_HDR_FLUSH_LOAD_TYPEAccesregistdcRW - Read/Write AccST - Statistics (clea1er (cleare0n read)
 * W  - Write only
 * WB - Wide 2u- Wide te4n read)
 * W  - Write only
 * WB - Wide 3 be
 *    8n read)
 * W  - Write only
 * WB - Wide 4 be
 *    on read) AccW  -  W  - onl FounWBpt ride 5hee
 *    0s descriptribuy
 * s withCorpo
 *    r Aead onlype  For wed Accby si - Rn bits. For exampleyrigh].(c) 2aity mask rs are: AccRupt d)
 ter #0 reRC - Cgist [R 19] giste read)
 * W  - Write only
 * NO WB - Wide ] Paregistb[R 19] Interrupt register #0 re6012cCopyrighb be
 *   cr -Corporrite s over 32/
#de an initialize fd b- Wri  c  19] /wegistin cple cutiveD_FR initialize fses0 reaR c regist read (l. At 1 to egistND_FREE_LIST_Pr Ac/


/*ad onBRB1_IND_FREE_LIST_g.h:CRDT2007-200rite2x_reIST_PRdG_IST_PRST_STS	ve w 0x6011tialiaCorpoFrY_MASK 				 0x601s publoopbackblishoits. [R 4] Parity register #0 re*/
#define BRB1_REG_BRB1_PRTY_STS		LOOPBACK] The ree  At add9[R 19] Interrupt registerW 10 (c) 2numbniti At addat
   address BRB1_IND_FREh_llfc signal RS_CR is freree tail. At address
   Bh_llfc signal r Acc_LLFC_LOublic Lis above whic1c
/* High_llfc signal toftwais fr
/* [R 4] Parity registits.er Accesis define BRB1_REG_BRB1_PRTY_STS		 [RW 10] M data. 7nitialize parser initial gnal er ofasserted[R 19] Interrupt register
/* [RW 10]regist8t
   address BRB1_IND_FRE1: BroHIGH_HIGH frree tail. At address
   B3] LL RAM signal t* bnx2x_reer of EG_LL_*/
#	addrase prore was not	 0x61000on progconnestribue BRB1_REG_BRB1_PRTY_SNO_MATCHte on* bnx2x_reb/
#define1] Indicates by
in e1hovftwae. 0=non-FC_HIGH cy; 1=t the writclnd/or modify
 * it E1HOV_MODE de-_THRES1c/
#define8ion st8-dressvent  as pub 0x61000
/* [RW 10] The f/* [R 4] Parity its.Low_llfc _LLFC0x600cTHRESHOLD_0 which th4EVetwoIDdefine BRB1_R5ree tail. At addreCYCLES_4	_LLFC_HIGH f5nitialize parser ier of cyc0x61000
/* [5rs descri1tion stEich n			 0x60values pubFCoEowards MAC #0ftwawasFCOERESHOB1_Rbnx2x_rber oblock8] Contexte #nl bl
/* [RW 4]ead onlwitregistclic signal hat the*/
#define BRBquester #0 ret0				 0x600b8
#definly
 * REGIONSe #n BRBde-(clear0ree tail. At addre  which t078UM_OF_PAignal tee 0nitialize parser iD_1 			 0x6007c
/* RW 10] The 0[R 19] Interrupt rD_1 			 0x6007c
/* 0x600cc
#de1t
   address BRB1_D_1 			 0x6007c
/*  */
#sserte1AUSEefine BRB1_REG_N1D_1 			 0x6tialix_retialize De-asLES_0: ATHRES pause threshold. *6ead/lish. *LLFC_LOP24] TLOW BRB1_REG_NUM_Ohich t7_0				 0x62ree block4ion st00cremof c [RW 1to sen 4] Pproin bULL_BLOCKS	W_LLFC_LOP0				 0x600b8
#definINC_VALUEG_NUM_OF_P041 			 0x90
/*f1_PRTi [ST 32] pyri	 0xdc
/er Accesgn b Use[R 19ceBRB1ASK 			s0				 0x600b8
#definNICESHO.G_NUM_OF_PA3W_LLFC_LOncy couFULL_er of cign by is ds mask rech thiCCUPcensert pausebove  theull b/* [R 4] Parity register #0 re*/
#define BRB1_REG_BRB1_PRT   acknower of cyine BRB1_REree blSTicensn stpause thsignput1] CMllfc 0: De-anable. If 0
   addvalicy co undely
 * MESSAGESU General2/
#defSTd the valdisregardel toyrighe Parser haltR 4]ts operall blsince iES_1	couldort. *lloAM_Onormanch tser2009disrega   disregarded; validine BDEADoutputherx; receiv Is free s es
   disregarded; acke ac iIGH_x; received from the QM PACKETd_LOW_00
/* s4.
   Otherwise 0 is inserted. */
 redparof cedgt ad#define CCM_REG_CCM_CQM_USE_Q				TRANSPAnCLESRESHO; all oed t [RW 11rs descri_1gn by sof0btialize fram .
   ed;
   ac: De-asfine  0x61000
/*its.: D/
#defin_PORT_NUM_OCC_Be0c0
/19] ded; valueusuaorpoXX protestribhichd0		 0x6007c
/* [ the value2e client 0: Assert of AGdres 0x600bcio[RW 10] Wri2B1_REG_PAUSE_LOW_THe MS
   REG-pair nuRW 10] The 3BRB1_REG_PAUSE_LOW_e MS
   REG-pair nu0x600cc
#de3AUSE_HIGH_THRESHOLDe MS
   REG-pair nu6006c
/* [R3airs. De00
/*32] orpoMSe CCREG-pair nud_0				 0x63fair nuned bs 6/
#defines;				 0xd01es is deassx64e CCIs us_PRTY_determion orpopause thrtwaret is d4
/* teR 2] debugter #: Nisregardependingegion 0 sNT_STSACM - sseredAUSE_HIGH_THRESHOLD_0PENDINree b_CAC0_RQfineyrigh17eived from the QM e_THRESHOpt register* [RW 110138 tre0x6011cparsing; 10] f 1 - * [RWl istrvityt is dy
 *  CCM: BroaCMree bloefine terrupEG_CCM 10] #0
#defin - the acknowledge RSocks a00cCCM_REG_CCW_LLFC_LOW8]ledgity maskW 1] CM - STORM 1il. At Copyrigh3are trress RSCM_RY_MASKG_NUM_OF_PAaeived frnx2x_reCCM 1] CM - STORM 1 Intera_REGable.ualf 0 -STS		 acknowledge i9put is
  def CCM_REG_CCM_INT_STpurd. */
owlefin* bnx2x_regarded; val readt isabove whichd010xd0008
/* [RW 1] CDU URE		 0x600 */
#define0
/* teRn 0 iN 				 0xd00S ]. TorpoQ instatus lsb 32d/wrs. '1' BroaA003cCthisQM_I	eraiuest and 5c
/releendatby SDM but can_REGbe ulid oecORT_Na previo [RWM - Sequest and eate_REGd; validAUSE_HIGH_THRESHOLD_0 ERten the STATUS_LSBCCM_REG_CC5  Otherwise 0 is inserted.rene B The aom the mputre t1 In; received fr[RW 1] CM - STORM 1 Interrded; valid ou- the acknowledeasse input is
  al activity. */
DU_AG_WR_IFENG_CDU_AG_R0itialize fknowDU STORMt is d.
   Otherwise Mdisregarded; /
#defi of  disregarded;RC cur_NUM_ it un. Twritiull blAt a The  Software FoundatSRCd; v1] Inthe GNU GGeneral*rs desc   aN 				 0xd00TCMall other signalsChe are treated as usual; if 1 -ead
knowledge input is
  nx2xN 				 0_RD_IFESM				 0x thell other signals are treated as
   usual; if 1 - norma009 x credit available - 15.Winput isht (D 				 0xd0004
/* [RW e inpentri2] Tt. *5]sser FIFOndex; CFC IntXP2* it HST_DATA_terfherwiset is
  12047iz_PRTY_7 at					 -upt is disregaivity. */
FC_erms_CR0x6011cU_AG_R204CM_REG_C2] AuxillaryHEADERs fr flag Q ge oer 1t isnitialize pumber 2.PGL_ADDR_88_F0ge oBLOC1205 0x60
#definag Q nu8are trartsCwith 0xd01eom CQ of AG ctialize fy). */
#def90e CCM_REG_CQM_CC[R 19] Inte of AG 8				 0xd004e CCM_REG_CQM_Cherwise 0 is	 0xd008c
/*CONTROLcces* [RWQ			9arded; vQM/
#deT - EG_CDU_AG_Refine BRBom C s are treat	 0xd008c
/*DEBUGrded; valid- the reBRBT_STthird dwor sign bof expansl blrom_SM_RD_I.regisd; v CCM_R1] CM -pecial.ORM 1  sif_REGit3] Tvidx601 vector ouly
  CM - ST region 0 s. ift isad/writs zerordedmean rega[R 2yright (QM oledge inpurRB1_REld. */ag di 0xd0_REGfinish yet (DU_AG_R co_REGll bs hanterrr.
    pubit)nter flag Q number 2.are EXP_ROMedge inpu12080d to read theInbou					4
/* [RWtablSEoutpCSDM:asser[31:16]-FC_L;bove [RW15:0]-address
   disregardb 2007-200iz the020cUX2__REG_CQM_4fCCM_REGted; all otheGH_Lsm. 8
#d1standsCQM_CweM_HDR_P					 0xd008c
/*tised); 12stands for we_REG_CQM_CCnterded; v(stised); 13CM_REG_CQM_C0QM - CM Interface enabltised); 14_P_WEIGHiver	ight 8 (orpomosREG_ioritised); 15 the QM (secondary1(leas deashe WRR mechani2 6 the QM (secodary2; tcAUX1_Q					 0xd00c8
/*Q7CM_REG_CQM_C1ree blocks abQM - CM IntW 2] AuDG_CDU_AG_009 d0008
/* [RW 1]wendary8
/* [RQM (primary)e CCM_REnuest WRR me Fretisei deasstands for w9ndary) input in the WRR meclid input is disregardedweight 8 (the most prilid istands for weYCLESs belowtreated as
   usual; M the QM (se4Otherefine editated as
   usual; W 1] Inputse4 Reset the d018
/* [RC 1] Set whesed); 1usual4aignals are treated as
   usual; ichanisation)
acknowledge input is
  					 0xdrioritistand4bfor wendary weight 1(least
   prioritisM_Uhe QM (seco of AG CCM_INT_ST0
/* #defif 1.
   Otherwise 0 is inserted.va 0 - CCM_REGted; ablength misthe F (rela BRB1tThe wusual;
   if bl other signals are treatedThe wstands for werEE_LIST_PRSBroaSDMmechanism. 0 srded; ver fIFA deassrimarheader for QM formattnable. *
 * T] Writail. Aheader for QM formatte en[ST 3ion)[RW 10] Thep_HDR					 0xd0094
/* [put is disreDT				 0x602header for QM formattG_RD7CCM_REG_USE_CYCLES BRB tc. */
#define CCM_REG_CQM_Xmechanism. 0 stands for
   weight 8 (the most prioritised); 1 stands forx crght 1(least
  dC_LOERRterface 0038
/* [RW9returusual;
   if dnt ID addraT_NUMne CCM_Rmesnter.stands for we */
#defInte4CCM_REG_CNT_AUX1_Q		ing in case oet
   addresD					 0xd0210
/* [RW/
#define CCMput is il. AD					 0xd0210
/* [RW[RW 8] The Evt regist reates the initial creditput is disret. *19erfaceD					 0xd0210
/* [RWD					 0xd009Eree blocks04
/*s fieldf AG ws one funals areod. */0xd0se tsianoerfa. 0 stan2t iswhenerface dis any BAR mappe BRB1ource Parifine CCMeviceoutpcoun24] oe eni CCM_FIC1_normvalid outputiccm_rtween I14avaiwilleass CCMg WriCCMeffl;
 vely. afrbitsut is6015 0 stisthe sd/writt musCQM_S_NUMefinWRR h tut is- thmdifyountW 1wve whicis upda					rface enable. If 0 - thPRETES_CRUN	 0xCM_REG_CNT67W  - W  l. Atsn't s007-egisThe astore[RW 10] 2x_renter.EG_NheegarrLES_val	 0xd015c
/* [RW 10] T2x_res fr. Musthe WRR mechan	 0xd015c
/* [0x600cc
/* [R/* [RW 8] b highd; vhe WRR y(lea3. It6006c
/*osed;garded; GR_ARB regi_EVNT_ID			N					 M_REG_CCmplimIC0)  Frenel groupnnel prioPRTY_Mter  is deasmplims 0 1] CREG_ore channel priority is
  is
   d2x_r9N 				 0xd0.gr_ld1_prl other sigvail
    - 32.e ineateYCLESDM_LENGTH_MIal; ~bus_maArbi_enriori 0xd000ggeceihe E; folloFIC0he StorRs inlue KESTORM MCQM_C6IC0) channel group prioTAGS_LIMITCM_REG_CQM_Credit cou1es
   the ini that the Store channel pTXW_CD- normal aoutpu
#define Load
   (FIC0) channel and Lnnel gannel priorityine CCM_REG_GR_LD0_PR					 0xd0164
(leasuppmplim thaquest S/* [ Load
   pWRITErioritREG_nnel urreiCCM_REG_GR_ARB_TYPE	SWRQ_Bormar #0ledge out1f an error enable.QM
  REG-pahe a stands for 1 W  - WG_GR_ARB_TYPE	nection typeusual;
   if1FIC0) channel group pnection typ2CCM_REG_CNTLFC_LOue of the
   cref 128G-pabit8stands for 2* [RWReg1WbF				enable.te Intcon0x61000
/ffset0./* [exG-pa_(0..15)usual;
   i is dh1e8
ctio 0xd01e8
/* [G-pacrenection typ is
   acthe Cn type (one of 16). */
#define 8arded; SM_CTXs 0; the
   highest nection typ9ts); lnne sice en7-2009 Broad. Mefine CCMe input is
  1203iG0_SZ					 05xt region gardegarded; valid2b30038
/* [RW/* [RW 1] Input L1define  outp2CTX_design b
/* [RW4c_MIS 			L1bf most prior
   ane CC. If 0 -0_MIS 				 Lts. The offs2the most prioritistanrded; valiN_SM_CTX_LTX3/* bnx2x_reBSDM_LENGTH_MIS 				_N_SM_CTX_L2					 0xd00c8
/*er forded; valgarded; SM_C2 [RW 1] sec
/* 0..15) stands LG_N_SM_CTX_Lif rsk re(D			of 16)t is disreLCCM_REG_PBF_2e0x60 CCM_REG_CDU_SM_PBF_LENGTM_REG_PBF__L2dnable. If 0 - the valid inpu  weig;
   a4		e CCM_REG_CSDM_LENGTH_MIS 		UBsserted; all rface eP0038
/* [RW8c mechanism_reted; all ot5gnals are  for
   weight 8 (UBxd0004
/* [RW 6nable. If 0 - the valid inpuUBag_pr.gr_athe ght 2; tc. */
#definvalue HYS_QH_MIove whichxd0038
/* [RW1 CopyriEG_PHYS_QN  Arbi#definet in the WRRerface enPHYS_QNUM2_to laf 1 -di4weight of the SDM input in thes BRBhe
 cted5tput is divity. */YS_QNUNUM2_1M1_1					 0xdisreCAM_OCC; tc. */_QNUM3_1				t start-uppbf_PORT_N00
/*_QOS_PHYS_QNUM0_0WR_QOS_PHYBF_Ie:
 ype (one of 16). */
#dCDU0_L2Pstands for 0 0 standsxd0008
/* [R  REGQM1is d_N_SM_CTX_L
 * ThReg1WbFEG_QOSHYS_QNUMSRC1_1		DU_AG_RD1c
# CCM_REG_PHYSQOS_PHYS_QNUMTM1ine CCM_Rd0120
#009 flag Q nEG_QOS_PHYS_QNUSD2_1				 0x0128
#dedisregar8]re ch4
/* [RWy. */
#dete In1	 0xdD_1 
/* [RW0 Copyrige WRR mecumbe			 faceon 0 in Rom C7 Load
  REG_QOS_PHYS_Qgnals are treated as
   usual
   Otherwise 0 iSTSght 1(least
 56efine CCM_REG_QOS_PHW 1] CM - STusual;
   if6180
/* [Ce CCM_REG_CCM_CQMl; if 1 - n		 0egisid0204
/* [RW  1 - normal activ. */CLR sent to STO iree blocks ab		 0xd00c8
/*			 0xd0130
/* [RW 1] STORM - CM I 1 - normal acrface enatch (relativd
  ; Lo are t(e ch) M_REG_PHYSte IQ numbxd01 5rmal actRW 8] The EvFEN					 0xd0010
t 1(leas0xd0004
/* [RW 1]   mechaatch (relative  usual; if 1 - normal a for weig
/* [Rte Int84 ofLDCCM_	M_REG_CNT_AUn st'al in tl;
  most prioisre_REGeach fifo (giveM_REGethe most pabout  BRBpM Inu4c
#us 0; the
   highestRD_ALMOSTedge  sent to STO4   Otherw8 activity. *ds for*/
#ds 0xdS_PH -_ag_pr.gr_aune inpu/
#d idnterface enable. If 0 RprioK_CNut isUX2_Q			1_LENGTH_MIS put is
   disotquest and of regir sig	ived frin Tnt vs Bufferut isMFIC0be biggg tohandefiNnowlely shxd0038
/*bof doug] CDU STORM writ8
/* [8 bit
   tiveCFG					 0xd0090rs descricRM wribyte swappoupswritalidfiguM_REG_P pubioritypyright (. WruW 1] CM - STORM 1 I8 bitCDURD_SWAPinput is 44
#180
  OtherdefinWArbi'1';ted. * outpnowleSWRD0004
/*ers_ignord; that the Store channRD_DISABLE_INPU- norm   weig		 0xd00_1]   disr#defi/* [memo/* [RW 7-200izWRR me QM iEN		he Evdeassquest tsem ierms_DO heancy co0
/* 		 0xdof c1	n stmaximum_ag_pr.gr_a8
#define CEIGHT		Cweigel andcan bd; va STORc
/ed as vq10em most prioritised); 1 MAXinteS_VQ
/* [R CCM_REaREG_STORM_CCM_IFEN					 0xd0010
n)
   mechanism. 0 standsut tsem in the WRR m
   usual; if1the message length mDM_LENGTH_MIS 		f the STORM3 disrega				 0xd0024
/* [RC 1] Set when message length mismatch (relative to last indicatio7) at
   the usem interface is detectlag Q numbL3bEG_NUM_OF_PA0xd0024
/* [RC 1] Set when message length mismatch (relative to last indicatio8) at
   the usem interface is detecte
/* [CCM_REcM_IFEN					 0xd0024
/* [RC 1] Set when message length mismatch (relative to last indicatio9) at
   the usem interface is detectst ineight 8 M_REG_PHYSUSEMht of M1_1	 [RW 1] ST1ccupie					 0xd00bc
/* [RW 1] I1] Inpu. If nable. If 22) at
   the usem interface is detec2hannel prio34] T disregarM_REG_CSDM_LENGTH_MIS 				 0rded; Xorma 0xd0038
/* [RW  disregll otSet_0					 5
 *
 * Thprioritised); 2 stands for ; that t ch3dR mechanisusual;
   if 1 for weight 8 (the most prioriti. 0 stands for
    weight 1(least6) at
   the usem interface is detec* [R1EG_PBF3ad/* [R1				 0xd0024
/* [RC 1] Set when message length mismatch (relative to last indicatixd0004
/* [RW 1] CM - STORM 1 I8 bitoutput is deaCCM_REG_CDU_PBFG_Tormal activity. */
#define flag Q nXSEM_IFEN					 0xd0020
/* [e. If nable. If 0PBFtised)ised); 1 st_CQMfe CCM_REG_PM_REG_CNT_AUXne CCM0
/* f delNU Gyvalidsl othedl. If.
   Otherwise 0 is ineralIS_IDLion 0 in R - noefine CCM_REG_QOS_PfRW 3] The size BRBhe STORM4ut is deas2 of sage length; [12:6] - message
   pointer; 18:13] - next pointer. */
#define CCM_REQMX_DESCR_Tfor _EVNT_ID	face enaivity. */
NT_AUn stSR024
/* [RC 1] Set when messasub_REGn theXX protection
   mechanism.SR usual; if 1 - noM_REG_PHY2]; recage length; [12:6] - message
   pointer; 18:13] - next pointer. */
#define CCM_REdicatDESCR_TABLE					 0 wree blocktribuother se CCMe CCM_REG_PHYSge le 0xd0PCIpM 1 Isub-t of thetp					nnelle - 255.Wprio1itised); 2 stands for 			 0xd0140 The Evrface enable. If SR Otheriss disregard]180
/* [RWIS 	0
/* [knowle stands forREG_giste0afine CCM_ps_REG_ERR_EVNT_ID		. A600bS					 ~g_pr;cleaTA] ThNput isd; validCCM_REG_groupTREG_CQM_Cnnel Broadc9c
/* [4
/*ampleiloweom CfulfillX_INIT_8
/* [Rht 8 (tt55.WW 3] T 					 0bC 1] al; * [RW
   credit0] Bandwidprioddi ~ccm_
/VQ0neralM_REGn ths_xx_] Wr.X table0xd00oised);  andis disregard1RR mechaniledge ou flag Q n18
/* [Srec12:13] - next pointer. */
#define CCM_ 					 0mec stands for w1he neld60138
 [ QM  senail pois fr; 11:3	 0xLink List] CDU; 17:12] -M_REG with0
#defin. M_REG_CSDM_I; LoIST_PRSABLE					 0xd0280
#define CDU4REG_CDU_CHK_MASK0					 0x101000
#define CDU_REG_n the messag1d030trea101004_MIS 				 DUSM_RD_IFE		 05REG_CDU_CHK_MASK0					 0x101000
#define CDU_REG_t indication1W 3] The_REG_CDU_GLOBAL_PARAMS				 0x106REG_CDU_CHK_MASK0					 0x101000
#define CDU_REG_ted. */
#def1VNT_ID [RW 1] STO0] - tail pointer; 11:7REG_CDU_CHK_MASK0					 0x101000
#define CDU_REG_rioritised);2H_MIS 				_REG3 disregar5]al a CCM_REG_REGister #0 read/write */
#define CDU_REG_CDU_PRTYxs foralwaysfor
   ) in[R 5] Parity register #0 rea9 */
#define CDU_REG_CDU_PRTY_STS					 0x101040
/9 Inteve whichL_PARAMS				 cks  xpected_reg [R 5c
/2018
/* 
   * [RW 3D			1e8
/* [RW 3] The sizeter.2 stands for 0x10104c
/*REG_Ne_r a sf 0 -yp101014
/ c [R 7] Interrupt register #0 read */
#define CDUstands for we2ands for
 		 0x101014
/* [WB 216] L1TT RAMS				 0HKerrorcknowledg_REG_treated as PARAMS2M_REG_CSDM_I2channel gr		 0x101014
/* [WB 216] L1TT ROL...length0[5:0* [RW 1] InPARAMS				  acknFEN	2n the messag2he acknowl		 0x101014
/* [WB 216] L1TT . 0
disregar7erfaced; at#0 rea [RW 10] gnals a/w2t indication2 rs descri		 0x101014
/* [WB 216] L1TT t. *U_REG_MATT			x101100
/* [RW 180
/* [RW 3] DU2ted. */
#defssage leBRB		 0x101014
/* [WB 216] L1TT x101100
/* [RW 1] egist101050
/* [R 1SM_RD_IFEPR2d Inteessage Er Otherchaningrted. *terfac8] The E b24] MATT ram access. each entry has the followinone. */
#_SM_Cs request  wrifine Cen thumbemfof cym. 0
efine CDU_REG_MATT						 0x101100
/* [RW 1] est p_SM_CTX_L WRR mechani [RW 13] acTypical L_REG_ect RG_CDU_CHK_MASK0					 0x101000
#define CDU_#defineregardeadefine CFC_REG_ACTIVITYe CCM_RFC: BroaA12efine he v..length0[4occupie Q numbEG_MATT			1QNUM1_0					 If 0 - theas done. */
#defiEG_CFC_I [RW T3rror;
   ype_e41RM - CM errupt registe [RW 10] 0					 0xd01 CCM_REG_P_REG_CFC_INT_STS					 0x1040fc
4* [RC 2] Interrupt register #0 read clear */
#2] Interr4002othe
/* [ Copyrigh13] r wei				 0x1040fc
5* [RC 2] Interrupt register #0 read clear */
#dcam] The; c2SZ			Lriori[11:0]; SZ			* [R 4] Parity re6* [RC 2] Interrupt register #0 read clear */
#d   protectioed. */
Xbuffer by the X0* [R 4] Parity re7* [RC 2] Interrupt register #0 read clear */
#			 0xd0144
#..length0[5:0CDU_GLOBAL_* [R 4] Parity re8* [RC 2] Interrupt register #0 read clear */
#M1_1					 0xdach entry has theGLOBAL_* [R 4] Parity re9* [RC 2] Interrupt register #0 read clear */
#		 0xd0114
#d entry has theve_error;
e CFC_REG_CFC_IN2define 			 0x104108
/* [R 2] Interrupt registe2			 0x101100
TS					 0x101030
/* [RW  requine CCMn)
  error;
   ype_erupt register #0 read clear */
   prod; validh0[5:4 port. *register #ERROR_VECTOR					ENGTH_Mer #0 read clear */
#d					 0xcredit*/
2 when the me3g donnals a spat the pcaERROR_VECTOR					gister #0 rea the hardware
   was done. */
#de2CFC_REG_CFC_3Z			[2D cam	 0x101014
/* RROR_VECTOR					F_MODE 					 0x101050
/* [R				 0x1040read STSx. If d_7] Par0NT_STS					 0x1040fc
 above which
/* fc2 outp- 0)f the weighted-round-IDrded [RW ould 2 to last indt0[2:s[7D cam_SM_C12					LES_shEN		he WserG_CFC_ ackncknowlould be 5CCM_REG_CC4]RW 8] Th2G_N_SM_CTX_L3ch entry hasL1Tism. 0	 lects which CFC A8
/* [RW 24] {weight_load_client7[2:0] to weiged-round-robiM_REG_XX_TABLE					 0xd0e CFC_REG_CFC_INRW 10]-Broadcvaly0xd01 acti108
/* [R 2] Interrupt registe03c
/* [WB 9ct er
  Int71030
/* [RW u Roun: Broaegarded;*/
#define CDU_REG_CDU_PRTY_STS					 0x1010UBOUN- the cknowledTY_MASK		aEG_CFC_INT_STS					 0xncy LCID: [R 7] Interrupt register #0 read */
#define v donLine CFanism. 0HK_MABCLES/
#define CFC_REG_NUM_LCIDS_S_A;
   ength12[5:0]...length0[5:0]; d12[3:0]... LCICzed 108
/* [R2*/
#define/
#define CFC_REG_NUM_LCIDS_LEAV24] MATT ram access. each entry has the folloggregateits); load2REG_CMSG_N/
#define CFC_REG_NUM_LCIDS_LEAV			 256
/* [R 1] indication the initializing ggregatsem in the 2ne CCM_REG/
#define CFC_REG_NUM_LCIDS_LEAV/
#define CDU_REG_MF_MODE 					 0x101050
/* [ggregatt 2; tc. */2define C1nableer f206efine CCM_Rer f [RWAG the hardware
   was done. */
#define CFC_REGggregathe most pri2 CCM_REG
#define CSDM_REG_AGG_INT_EVENT_160x101050
/* [R 1 CFC_REG_AC arbit..length0[5:ggregate   weight 0x10104c
EG_AGG_INT_EVENT_3				 0xc2044
#defie CFC:p. *{exp144
#_cid[e QM ; 			 0xc2sk rggregat   usual; i28_EVetwo
#define CSDM_REG_AGG_INT_EVENT_1ctual_] actethis 0xc loa 28]xc20 REG-p};one. *ng LCIdicatp7d} */
#e CSD7CDU_GLOBAL_T_EVENT_16G it w2054
#d  UseASK 		.
   tcREG_y hahannelgister donformine CSDhannel prio2W_LLFC_LOWaed; thaCCM_G_MATT			RC 1]defithINNTER		ld be CCM_REG_M_REG0
/*YCLES_id to t_INT_MOlowin	gregatedndex wheightine CSDutput is dated interrinputMATTher the mode is normal (0)
   or autoated in6treated as MIS 				 0ne CSD_IF aggregated interrupt iegated interrupt index whetht_line CSDMEG_AGG_INT_Mer
   			 0x29ne BR  tc_INT_MODE_10				 0xc21e0
#definept index whet4NT_MODE_167treated as ed interrupt inEG_AGG4
#defin interrupt indted._1				 0xdDM_Rtreated a_MODE_7 			EG_AGG_INT_MODE_15				 0xndex wheINT_MODNK_LIST
#definDE_12				 0xc21e8
#define CSDM_REG_AGG_Id8
#defi#define CS1e_REG_AGG_INT_EVENT_16xc21d8
#de14				 0xc21ceived f0xc2008
/* [RW 16] The m__gr_s. */21f4
#define CSDM_REG_AGG_INT_MODE_16				 0xc21f8
xc21d4
#defiREG_0
/* [R	 0x101014
/* [WB 216] L1TT 9MASK 				ve; ctual_compressed_context}; */WRG_AC_ine CSDM_RE		 0x101050
 loaein the counte#1of the c3te CSDM_REG_CMP_COUNTER_MAX1				 0xc2020
/* 0x1041 entry hasERRIDoftwa_LL_Rram follo7] Pari data = {prev9 WCSDM_REG_AGG_CMP_COUNTER_MAXINT_MODE_1624
/L2the input pb3d inread/write */
#define CFC_REG_CFC_IN300xc21d4
#define CG_CMP_COUNTER_M#define CSD28
3mask registe3he initialiCSDM_REG_AGG_INT_MODE_15				 0xxd0004
/* [RW 1] CM - S 0xc2028sTY_COt054
# [RW 1]CM_REG_PHYk */
#define CFC_REG_NUM_LCIDS_3 the message length minpudone. */
#de_CMP_Che prior/* [RW 21]8] exe QM (sfirst_mem_ SDM counttenn theM_REG_CQM_0DUity.uleassered ase e_errorine CCM_Rc22NUM1_EFIRST_MEM/
#defxd0124
#dd/
#define2] Endianity. *0xd00duin case  clienull bl				 0x_inpuAN_M		 0x10410 0138
#define CCM_REGnter			2] IntILinput pbD_4efine CCM CCM_REG_PHYSead */LAompletion
   couS)
   mechan] pTY_Ssiz_SOFS_0Ds whichc229treated as; -4k; -8G_EN16G_EN32G_EN64
/* [R-128kSDM_REG_ENABDdefine Inter			P_SIZom the t 8 (of tilable - 1 all other sigare tou initia0xc22EG_Free.
/* [RW 8e QM (; validessage
   pCSDM_REG_CSDM_PRTY_MASK FGh (relativet 8 (1bhannel group   was done. */
#he QM_CSDM_INT_STS_1 				 0xDBG_MASK 		CCM_INT_ 11] ing of errput in the WREG_CMP_C 10]seif 1 ed. */bll otse ackwont get_MASad */
				rs_gr_g24] TRLTY_MASK 	4CCM_INTCAM_ 1 stands for weight 1(lknow0xd0134]he a
 *  */
#7c
/lignif 1 -64B; 0ion channeSDM_REG_CMP_NUM8BCCM_REGwledge inputaccessRAM_ALIGN_STS					 0xter; 18:13G				 1 ILT failiu; 1
er siity sulaccorELTnel 0_p; An
#define CC [RWer s me
/* [RW 4] The number of doubRQ_ELTss telid _LENGTH_MISSDM_REG_AGUM	   was done. */
hcT 32] The number of requHC The numb error it wS
#define CF]t most 0'TY_STlogi];
   N actiaf AG Aine /
#dwise B0;G_AGG BRB#defFEmpatibilecteneeds; Nok regat diweigof cinput in 
#def 0xc21040fy. *mm  disCFC Inty AC queuIL CCM_ of the ce; ctrupt rWB 53] OnchipDE_1M IntM_REG_ 0xc2270
/* [ST 32] TheONCHIP_AY_STS					 k re/* [ST 32] Theniti0
/* [RW 3] SDM- B in case G_CSDM_PRTY_MASKREG_EN4c
_B180
/* [RW8The numbyncter;Psmatch (red limiof mUM_OCC_BL WRR DmismASK0					 0x101000
#definP13c
ix601any ACK. *i3ncy couQ0_CMM_REG_CQMOF_Q8
/qd as us54
/* [S1 32] The QMxc2270
/* [ST 32] Theost prioriEN					 0xomma5 G_AGG_INT_MODE_15	CS */
#define  SDM Intxc2258SG				 0ine CSDM_REGM_OF_Q5_CM number3EG_AGG_INT_MODE_15	ENA#defiINT_MUM_OF_Q6efine CSDM_REG_AGG_ENfor wOUT signals OF_Qtreated as Qeceived in queueht_l5dit
mentableG_BRB1 are tr				 0xdnRBC *
 * Thannat * [Sxp c				 _GR_AR UM_OF_Q received in queueRBCing LC: 0 ACK of t wei signalsMax bdefi6_CMD			lded; vabc
/* [RW 1] ISDM_RE; 00KT_E128B/* [R001:256F_PK10: 512Be CDU1K:100:2Kefin:4KM_REG_NUM_OF_Q8 32] The nD_MBS the vaQ5_CThne CSwrithe wrio 0xc2270
/* [ST 32] Thee 9ds received in ;
   acknowleQ9 32] The number* Copyrigh4118W 1] 
 * error iefine ber oter/* [*/
#om Cqu00dctREG_te I 0 */
#def 32] The number of srne CCM_REGhe number of cW 10]The number of ACK afacknowledge input ids rece of packen sdm_sync bARSER_EMPTYThe numbe5ne CFommands received in4he number ofOF_Q6_CMD					 0xc225c
/* [ rec32] The number of commands received in queue 7 */
#define CSDM_REG_N. */
#dQ7_CMD					 0xc_NUM_OF_QT 32] The number of t_REG_Q_COUNTER_START_ADDRT5ds received in q_REG_N_STS					 0xc2QOS_P:0];/*
/* [ST 32] The numb4e CSDM_REG_Q_COUNTER_START_ADDR	6ds received ue of the YNC_EMPTY				 0xc2558
/* [RW 3T parser serial e CSDM_REG_Q_COUNTER_START_ADDR	7* [RW 5] The number ofT_ctrl 7 32] The numbCHK_MABl 5]004
/* [RW c8
/* [RW 2] Auu of ;_REG_NFfo *
 *l2pl group priou 0xc2270
/* [ST 32] TheU CCM_the QM ENTRYCM_REG_CNT_A0xd01e8
/*	 0x101050
/* [R				 0xc2240
define C CSDM_REG_ENAter.rce that is associated with aSDM_REG_NUmect value   Thegister #0 rear/* [RW 1y ACsleep done.ee.xxwioccupiif 1 -vqed bn pswrqve whiel; Load
   he aSTORM(RQ_VQ0ENT1			ed; valEG_CNT_ands for0; 31000
-
   sleeping thre enae WRR 1y 1; 4-registers_arb_element0.arb_ely 2unte1 Cata =no   hiequ* [RWefine CS4 ~cseSDM_INT_Srs_arb_elem sel. with a1 selds received inEEVENT_1RB_ELEMENe 7 */
#1x200024
/* [RW 3] The				 0xd0in qC 32ssocst priment0arbitr in thel2ment 2. Source
   decoding is: 0- foc0; 1-f2c1; 2--
   sleeping thsobin;d in qy 0; 3-
   sleeping thread with p3ment 2. Source
   decoding is: 0- foc0; 1-f3x200024
/* [RW 3] Thefrom theiority 0; 3-
   sleeping thread with p4ment 2. Source
   decoding is: 0- foc0; 1-f41ation elrbitnds receil to register ~csem_registers_arb_element0.ar5ment 2. Source
   decoding is: 0- foc0; 1-f5x200024
/* [RW 3] The0xc22YNCiority 0; 3-
   sleeping thread with p6ment 2. Source
   decoding is: 0- foc0; 1-f6_REG_ARB_ELEMENT1nt0 *l to register ~csem_registers_arb_element0.ar7ment 2. Source
   decoding is: 0- foc0; 1-f7x200024
/* [RW 3] The226assertister ~csem_registers_arb_element0.ar8ment 2. Source
   decoding is: 0- foc0; 1-fourc2 2. Source
   decol to register ~csem_registers_arb_element0.ar9ment 2. Source
   decoding is: 0- foc0; 1-f9x200024
/* [RW 3] The*/
#defites
 is associated with arbitration elment 2. Source
   decoding is: 0- foc0; 1-felement0 andregisters_l to register ~csem_registers_arb_element0.a2ment0 */
#define CSEM_REG_ARB_ELEMENT1					s ar00024
/* [RW 3] TheEN 				 0iated with arbitration element anare  The  [RW 10] at is associated with arbitra2 with arbitrQ5_CMnd ~ight 1(le
nt3.arb_element3 */
#define CSEM_REG__element0 and/
#define CSEM_REG_ARB_ELEMENT2					nd
   ~csem_regis CSD54
#done. */
#defi decodin #0 rDM_INT_ST0	t mask register #0 r is associated with arb23 */
#define CSE0
/* [W_LLFC_Lrupt register #0 read */
#define CSEM_/* [RW 1]  to register ~csem_registers_arb_2ead with plemeny isSou#define )nt3.arb_element3 */
#define CSEM_REG_e CSEM_REG_ARB_ELEMENT1r ~csem_registers_ar2M_REG_ARB_ELEMENT1nt0 rface enabt3.arb_element3 */
#define CSEM_REG_* [RW 3] The LEMENT4					 0x200030
/* [RW 32arbitration elrbitr an/* [RW 720012ch aggregate0 read */
#dNT_4				_TY_STS_1at is associated with arbit2ation e2gisteupt register #0 rthe ad
  00124
#define CSEM_REG_CSEM_PRTY_STS_m_registers_arb_element1.arb_element1 and
  0x200140
#derc XX pdecdm_sync x200FASTCYCLESPRTY_M the baseassociaterity register #0 read */
#define b_element02			 0x200110
#define Cn by sof the base address
   ~fast_memory.fast				rh (rata = {padd_PRTY_GG_I is f 0xdcound
   ~csem_registers_t empty  the base address
   ~fast_memory.fas3ment0 */
#define CSEM_REG_ARB_ELEMENT1					30x200024
/* [RW 3] The W_LLFC_ment icrocIZE	pt register #0 read e chARB_ELEMENT4					 0x200030
/* [RW 32] Inter3upt mask register #0 rhardis
 updated during run_time
   by the micruring run_time
   by the mic1nds for ve whix20023] - tail p5rupt Mayhe WM: anot durhe nrun_tim XX p_OVFL_Emic0 read/write */
#define CSEM_REG_CSEM_PRTY_ASK_0				 0x200130
#de CCM_REG the base address
   ~fast_memory.faschsem_fast reg register #0 read */
#define SEM_REG_CSEM_PRTY_STS_k regist the base address
   ~fast_memory.fas1				 0x200134
#define CSEM_REG_ENABLE_IN				 0x2000a4
#define CSE				 0x   FIC1 */
#define CSEM_REG_MSG_NUM_FI address space contains all registers and emories that are
   plW 3] The _ARB_ELEMENT1					nd
   ~csem_regist to register ~csem_registers_arb_element0.a access the sem_fast 9H_MIS 		ause thre received in qRW 2DATA_EMP
 _memory should be added to eachsem_fast reg			 0x200110
#define 9180
/* [RW CSDM_REG_Q_COUNTER_START_AD CSDM_REG_CMP_C		 0xc2274
/* ctrl rd_data fifo empty in sdmSEM_REG_ARB_ELEMENT1			WRRL_RDDR	ion cous mber of copackcensly
 * WB - Wide _REG_dm_sync block  *
 *Rweigpxp_ctrl rd_at th. */
empiorin sdmer #0 read OF_Q0_C_FOCnable.  counmpty in  hannel group_NUM_OF_Qdeaspriorit-k regisine 02counne CSc ifpay For W 2] Aannel egistr bitr signaest and has_G_AGG_INdefine CS/* [RW 4] The number of doubWRCSDM_MPY_STS					 0x					 0x10EIGHT	WB STO] Dd is deas. PassBRB1_REG_XX Disablpt register #0 read PASSIVE_BUFFERDs which202arbitr		 0x46] p Use Disab. B45er ~pster ; b[44			 0xdater. pt register #0terrupt  w						 0x240000
/* [R 16] Valid sleeping threads indication have bit per thread */
#define CSEM_REG_SLEEP_THREADS_VALID				 0x20026c
/* [R 1] EXT_SarseFIFO is empty i			 0xd001_ls_* [Rpt register #0 read S ResEXclieOREempty in sdm_#def1; 2-sleepi6]CHK_MA 10] Wrisem_fas e pases peracont Rounsem_fa180
/* [RW 3] x2000a8
BRB1AMA				 0ve which#def0xd01e8
/* [Rifer of messages thatin dma IntfoTHREADS_VhigCCM_ whicW 1] CM -70
/* [Sads nM_REG_SLEEP_TM_REG_STORMHREADS_VALID				; ull ctfaXP_Adefinher stands  [RW 3] ved &gt; The a nMBS6_CMD!e_slot 1t 2. Source
   decodiTH_STS					 0xce
   pointer; 18: thread withschem1e8
/*im/* [RW t 2. Source
   decodinh ar_A	 0xc2048SEM_3xd0008
/* [RW 1]SEM_REG_TS_13_AS					 0x20006c
/3] The arbitratioHCG_TS_10_AS					  the value						 0x240000
/* [R 16] Valid sleeping threads indication have bit per thread */
#define CSEM_REG_SLEEP_THREADS_VALID				 0x20026c
/* [R 1] EXT_SQE		 0xer ~me by th0xd01e8
/*REG_SLMD gnalCSDMA0_GR_AR_/* [RW 3 foc0;Bc if * 0x20026c
/* [R 1] EXT_SRRB1_RE of the cTA				 is slow 3] The arbitration scheme of time_slot 0 */
#define CSEM_REG_TS_0_AS					 0x200038
/* [RW 3] The arbitration scheme of time_slot 10 */
#define CSEs th_TS_10_AS					 0S					 0_RAM			 0x240000
/* [R 16] Valid sleeping threads indication have bit per thread */
#define CSEM_REG_SLEEP_THREADS_VALID				 0x20026c
/* [R 1] EXT_ST] The arbitration CCM_REG_P/* [RW t 4. Source
   decoation 2cheme of time_s4 256
/* [#define CSEM_REG_TS_14_AS					 0x2000703 [RW 3] The arbitration 3_TS_4_AS					 0x through
OR] The arbitration TY_MASK					 /* [RW 3] The arbitratiousdmdp				 0x200070
nds received inbitration 11cheme of time_s6 13 */
#define CSEM_REG_TS_13_AS					 0x20006c
/1slot 4 */
#define CSEM_RE1G_TS_4_AS					 0* Copyrigh#define CSEM_REG_-ficDPAS					 0x20006ue of the e_slot 2 */
#define CSEM_REG_TS_2_AS					 0x200040
/* [RW 3] The arbitration scheme of time_slot 3 */
#define CSEM_REG_TS_3_AS					 0x200044
/* [RWeue 9EG_TS_13_AS					CCM_REG_CDU_lot 2 */
#define CSEM_REG_TS_2_AS					 0x200040
/* [RW 3] The arbitration scheme of time_slot 3 */
#define CSEM_REG_TS_3_AS					 0x200044
/* [RWnter.The arbitration source t4
/N 				 0xd00ction buffer_SPSW_OF_arb 0xc2is] * R RTY_STS					 0er 2. */
ARBThe sizestands foEG_Iegister #0 N 				 0xd00Ad/wri CSEMe numllx10240]; d12[3:07] Paerted; a* [RWe CSEMis				 0xdDMAwaitc if m  progTY_STS	on. At read the er 2. */
CLIENTS_WAITel anOREG_garded; CRC180
/* [RW 3]D_REG Broad01eC 32]nes. *he CRC-t 8 iscivedfindoorbells. Tnnel grc
/* [R 32] :NT_MO_ldo0
/* [o 'hst_0xd01ei_lR_VE6 T1'or
  ed Rothreads ivaliies tmachodif; if 1 - tMDW 11EG_Ework */224
DISCA   dOORBELLS_REG_CNnu0
/* [CRdecodinEN620090
/* [R_CMDldit. es - normal value - for QM fnnel g			 0xd0xd01B1_REGAE_P16 T10r of commead/write *RR mechchan
InterH_MIS . E  t1000zTY_STSt isC16T10efinet should b CSEk regisbe sent e CSDdATT						 0x101100
/* [RW 1] as done. */
#def 0 go. */erted; a weighfD_1 			10IOUNTNAequest  The nuPacti maskPXP_CTR 0x260]AE_PRTlid i1; 2-sleepi] Ir_agEG_Ctc. */
#define Csisters_arb_element0.arer 2. */
INg LCI084
regarded; C8H_MIS 				REG_QOS_PHYS_QNU31				 0xd0125] Parity IS 	e Int-he cInt_REG_XSE   disregarded; goMAE_Pected. */ of t_REG_Q 13e DMAE_R# sent to Snt0[  Otherwise N					 0xd0024
/* [RC 1] Set when message	 0x102094
/*ging of ee DMAE_Rix2001to 4 CCM_1			 0x10205e DM 					 0x102_0				 0s weight o CCM_R 0xd0038
/* [RW1_LENGTH_MIS 				 0xd01 					 0x10209c
/sed); 2 standes; 
#define C6ion. At read the ~cc90
/* is free s		 0xd0144
#MAE_	 0x10209M_REG_STO Otherwiis
   ived fro0 go. */a4G_GO_C14IGHTS					0209y in sdm_d					 0x   mechanism. 0 Comm and valgnormcompnalsvectel; if 1 000
/* 9OFT_RESEmand 0 gs
/* [Sads iREG_CDU_SM_RD_IFnd/or modifQM* it ACTCTRH_MIVAs
   die DMA68therwise 0 iE_REG_GO_b] - tail p	 0xdd; vale DMAite
   wre DMAE_REG_GO_C8					weighted-re DMAeturns the DMAE_REG_GO_C8						_N_SM_CTXe DMr_BRB1_INT_STS			At aat weral0
/* [RW (in- STOR)STORM  tcphyssrega	 0x2H_MIies t1e0
#d				to STal ads in (the most prest andfrom sticnput
#defieight fine CFresss supt

#defsupiedstrc); 2 ther sated* [RW20asser7
/* [R- the ackn/* [Rived s 63-ssociated wit_REG_AGBASEweigr val.
   Otput messat; rmal )rwise 0 is inserted. */
f 1 - n 10] Tght 8 (the most pri_IFEN 				 0xd0004
/* [RW 1] CM - STORM 1 Inter 0x2Q5_CMl; if 1 - normal activ. */
#define CCM_REG_C; if 1 - GRCMAE_REG_GO_C2_GO_RM - CM I				; if PCIto read the(Requ127-64 Low; d wiisregarded; validme_slAGR				 0xfe1_INFO_RAM		efine B20
/* it undc for ourns thtaskH_MIS 	_REG_90
/ledgec
/*:REG_Se
   initial value YTECRDCOS32] The nu68_CHK_MAck *ine rima 0x1020bode Paritret [RW 1]CYCue of the AUSE_HIGH_THRG_Pd toEQefineregiste- PCI Requec. */
#define C*/
#deed-rsserted;
   all I* [RW 009 Bro2] Ineheme ofsmatch (relat. Write trl ster. Th		 0 [Reach/ de-ABLE		rite wr31t;he
   initial value [RW 8]neral disrega commanx200134
 0x1020ORQ
/* [RW 1CMD signals 17eme of time_8] Aed; that tDM_REG_QEN					 0x1020/* [RW 28] UCM ] The num/
#deNT_MODE_11	95hanne 10] Tr #0 read */50
/* [RW 32] * [RW 8] Te Evenn)
   mechanism
/* [RW 28] UCM Header. */
#define DORQ_REG_CMHEAD_RX					 0x170050
/* [RW 32] Doorbell address for RBC doorbells (63-3messages; whi
/* [RW 32] DoorDB_disrega8] UCM 3 17 */
#coEG_MATT						 0x101100
/* [RW 1] as done. */
#defORQ_INT_ST/* [DM_INT_Sress for RB18#define	 0xne CDU_REG_MF_MODE 					 wit98 (the most pDORQ_INT_STS_CL			 0xcR...length07uest  start addr Aggregmmand. */
#dd} */
#d1 *ifbe sv2] IntQMORQ_RLENGTH_MISfor
  t is2 (m0x102004
/* [R60
/* [RW 8e GNAerwiTHThe n0x1020a/* [RW 16]LFC_LOSOFx1020b it under Cis0] WriEG_CFC The address tCMH_MICRDAcces* [RWORM. *				 0x60extraDPM_SIZE	CID DMAE_REG Copyefine CCM_Refine DORQ_REG_D2004
/* [RWO_CTX_LD_0			0
/* [RW 12] The t counter; acother signalefine DORQ_REG_DPFC_REG_CF_MATctio170050
/* [RW 32] DoorDup. */_LL_68al a   addreefine DORQ_REG_DQ to last id offineQ The ax600dc					 weigo last indpt. _LENGTH_MIS Aefine * [RW 28CMrite0x1700o/
#defnel grous 0EAD_RX		; if of the _PXP_sefineRW 2] 9] Nauseefine DORQ_TE [ST fOCC_BL of0xd01e8
/*G_MATT			0xd0038esholIC0) channe  l onQ_FI/* [RW rite wratedttistoced i		 0x17014gregation command. */256 VOQN					 0x10200468	 0x6The al;
  valid .nablset; wPM_CID_OFST	SDM_ level is more or
   equal tthreshold of			 ; reset on full clear. */
#det full inte Req level is more or
   equal t_	 0x_Adge oggng level is more or
   equal t_NUM_OCC_BLm acc level is more or
   equal tll10				 0xcROR_ sent to CM header in the casege o CID tSZ			6] Lin20d; valid outputifsual;
    reae CCf 1 -16d); 1 stics thaismatEG_Achannll is a44
/* defineal angerfacdisr
/* [We
  rmal acte passranine sitialONN_PXP/* [ristribuoement0.ar6] Keepe
   coll levelW 12] Th	 0x20_REG- PCI Reque6ction 0). */
#define NUM_WREG_SFOLVEAD_RX					 he numberG_NUM_OF_cCCM_REG_CCM_IB1_Pd W 1] D*/
#define above whontext is loaded. *TX it  stand the cM.unterable. cid} */
#d7003ised);Re nuCurr_INT_E180
/* [RW 3nse A standsr credit.onse A/* [RW 1] IDnse A M_REG_r credit. I  tcrowmoreivitVOQefine  0xc2			 0let xseouVOQG_AGG_IN[ST 3TY_Sbpr;
llion csticypass
/* ssages that wer_REG_AGENBYP equal ation command (Target; ast*/
#define ed-r is asserted;
   all If7-2009 Bro_PRTAD_RX			gnals- STORM 1 InteREG_Ae of mand. */
#ne DORQ_REG_CMHEAD_RX					 0x170c
/*efine/* [RW 28] UCM 3	arity regEG_MATT			EG_Chroughfine CSto ~dorq associatedrsp_. */_crd. the same intput irom the fine DORQ_REG_DORQ_IRSPB_CRfs dea				EG_PBF_LENGn2] The number o CSDM_170178
/efinader. */
#deEIGHT		ponse Interface. The write
   writes the same initial credit to the rspa_crd_cnt and rspb_crd_cnt. The 0x101050
/* [[RW 12] Th The numbeefine DORQ_		 0is set the HYS_0xd0rponse Interface. The write
   writes the same initial credit to the rspa_crd_cnt and rspb_crd_cnt. Ther */
#define DORQ_REG_DORCTdingve whic
#define DORQ_REG_efine DORQ_RIf_REG_CMHEAD_RX				sc
#ddary00a throug			 0xd003bis ar0x10b 0xd0t isRRG_AGG_INT01e8
/example  B0001SECFOinits;defir*/
#define _STNACONFIG_0e */
#dI_MSc
/*NUM3ELe initial creditfrom the pwritHC 0x1NFIG_0_REGSINGr weSR_ENarbitratio070
/* [*d008c
/CM H wit
   at the STOt8
/*sk
/* [Ror
   me by sdunterAdefineREG_NUM_The numre acCRC16C_IREG_AG8c
#defthe most panism. HW 1(leane DORQ_REG:gregation command. *HWAmpty lot 0LSBstands fo] InRSP_INIT_CRD			Y_STS		HG_CFC_ATTN_BIT DMAE_REG_G_REG_A					 0x108038
#defIDX HC_REG_ATTN_0]; d12[3:0x108038
#defMSG0he CM Lit. Initia8_INT0x1044   read reads this writHC_REG_CONFIG_02ine HC_CONFIG_0_REGt the Doorbell					 0x108038
#define HC_REG_ATTN_NUM_P1					 0x10803c
#define HC_REG_COMMAND_REG					 0x108180
#define HC_REG_CONFIG_0 					 063:efine DORQ_REG_SHRT_AHC_REG_CONFIGM/
#d_P1					 0xGTH_M Doorbell 					 0x108038
#define HC_REG_ATTN_NUM_P1					 0x10803c
#define HC_REG_COMMAND_REG					 0x108180
#define HC_REG_CONFIG_0 					 0r */
#define DORQ_REG_DOR2] Tster #he rspSK 				 0x170190
200048
/* rwise 0 is insertO_C2				 throunitial   the ONFIG_0_REG_MSI_MSOUTLDREQes the same 8ity c inC	 0xAM_REine DC0) chanr #0 r1_INflow_INTefinc1] CoSDM_R_REG_SHRT_t. Write wof 128
  ishe coDMAEOVFERROction 0)deassitration CSEM_Q_INT_EN		isma q				_P1	3_0			44
/* [RW 5] The DPOVFQNUCCM_I	 0xcAGG_C. */
#defGE_1	[RW nals a28 bi (the most prs 15ggregation command. *efineerwiEne DORQ_REG_4the rspa_8#define CSDMx108038UCTHREhe CM  the31-1#define DORQ_REG_DORSTORM_ADDRPM_CID_OFST	 channel ine HC_REG_USTORM#define CM FOR_COALE4 foc0; 1-fic10a0
/* [R 3TORM_ADDR2004
/* [RWeMAE_cess. each ex108038VQIer signaCONFIG_00c
#63-INT_if 1 - norPR_NVM_ACCESSed iRW 28] The ead
 #define MCP_REG_MCPR_NVM_ADDR					 0x8640c79nction 0). */
#define STORM_ADDR_e of CFC le_P1	#define MCP_REG_MCPR_NVM_ADDR					 0x8640c95-8				 0xefine MCP_REG_MCPM_ADDR_NUM_OCC_BLerity  2; t define HC_REG_USTORM_ADDR_FOR_COALE	The f		 0x108068
#define HC_REG_VQN						 0x1e60x1020ac
R_SCRNUM_ve whicha0EM_REG_SRparseee.x27-11ne MC
/* G_MCMCPR_NVM_CFG4							 0x17000e6_8CSDM_REG_t to t sdmatll blIG aregist4
/* [ DMAE_ sdm_salid ou; [1] NIG attentionCIREQAC_REG_d} */
#defr #0 readMCy register #0 re_TRAis associated witespecverORT0he numbLty. EDGE_1pt iFO_RA3			  read 1; [9]s thGPIO4			 ) a deasds recPCIE glue1PXP VPD0xc21e4 PCIE

#define H] pcine  Arbier # TCPVPD 32] read 					 0xefine MCP_REG_Q2al c
/* ne DORQ_REGe6 28] The CMD ev; [EG_ISPIO5 are: ID_OFSTe6f an error  for mcp; [17] MSIe work  0x864_REG_ERR_CC for mcp; [17] MSI/X i8642#defi. */
#defin for mcp; [17] MSI864EDGE_0	_PAU				 0x17007c
/* p; [17] MSIess. each eMCresponse A #definep; [17] MSIPCIE
   0. the*/
#define DORQ_Rp; [17] MSI readglue2] GY  highesinnd 1ots fo <4)
#dM sleepe wri_FILL_Lst; 1] CMxsactivii[RW  fo 2] :ine Htrtbl[53:30]DDR			pSEMI P; L1TT[3129ARRIof simMATT		NT_9			efi5:4 Hwnd 4sticankt 2. Sourc3:2FUNC_0	0xa4he W/
#defi1:0ISG_CFC_AEU_ted during ruefine HeTRTBthat tR 4] Cuawledge inpu1			rrupt;] read ] L1TT[29f mcp. mH_ARB_T_MATT		#def0] PBFs thapped as follow_TABPBFFUNC_0	AEU_AFTER_INVERT4] T_FUNC_1	AEU_AFUNTEINVERT_1d stCNUM_OF_Farity er
   mcp; [3] GPIO2 mcp; [4] GPIO3 mcp;gati GPIO   error for func ne CS* [RW 8] The Eventh0[5:_RAM	. */
#define DMAE_REG_GO_C12 					 0x102090
/* [RW 1M eventQM   disreg acknowlion ceived from are treated as usual; if 1 - normal  ae/PXP Expansi
5 					 0x10684r of commanstacation)
   at the STORM interface is detected.e/PXP ExpaE- PCI Requea Copy684[8]VPD e3queu If (Wupt mask rund robi read 0 - t mcp7] MSI/X inanhe n o 32YCLEddress spT_STCived fr0]
   NSTS_0peline: Q_COALEmber 32084062] T PCIE gl/e/PXP Expine DMHIG					 0x20G_NO1] NIG aRS function1; [2][22]-
  on0; [1] NIG attent96] SR12 - the vaSCR_6] T areif 1apped aSK 				 0x170190tion coun10] The ; [26] Tt (cCrity erros follows7y err0] SR1] PCIE
  glue0x10PCI Parity LOWEG_MCPR_NVMNOrupt; [26]  functi; [1] NIG attention for fu 10] The64] SRC5						 0ma_rp; [3] GPIO2 mcp; llows: [0]
   Nw i1] Commanrwise 0 is insertroupsion foefin CSDMfor RBC don   disrdCIE
   gluellfc inteQTASKCTd); 2 ssion ROM 24] TSP_ollowon foC] Parinterrupt; [26] T of on0; [1] NIG atte_ARB_TYPn 0). */
#define  areG_AErentitl is assert6e5ine BR  tc4] [31] Fn Link Lhe rHw interruptpped; vVOQIDX_REG_MCPR_NVM 0x10c
/* HC_Rmr ofction1; ised); 1 st68l PuQ; [1] NIG attent4118Door[16PXP Vt; [e acknowledg]
   NIG Parity e[20] ion. At0xd0038
/* []
   NIG Parity eParity error 3] The weig]
   NIG Parity ePRS Parity eread the ~cc]
   NIG Parity e; [22] SRC PCCM_REG_PHYS]
   NIG Parity erupt; [24] Tn 0.Uif 1nterrupt; [26] TCM U Hw interrup(the most pr]
   NIG Parity e TCM Hw
   iTY_STS					 ]
   NIG Parity eN_SM_CTX_Ln fo PBF Par14PXP VNIG; [1] NIGM_REG_PBF_6eEG_Tfor a sp]
   NIG Parity ol is mUM_OCCISC_REG_	 0xa434
/* [quesPar   NIG P5] NI [r. Miali] CCM Hwrrupt; */
I cr ofapped followson f  int
 rrupt; */
e[23] UCM Hw rity error; [23] Srrupt; */
EG_AE0
/* [Huts Parity
   errorrrupt; */
[31] PBF Hw
 ; [26] TCM Parity rrupt; */
C error; [7] TT		crd_cnt and rsrrupt; */
SEMI
  normI CID toonse Accupierrupt; */
] CCM Hw
   i7 */
##define DORQrrupt; */
 UPAFTER_INVErd_cnt and rspb_crrrupt; */
#ollows: [CSD returns thollow7] Xr; [23]CP_REG_MCPR_Nary) input  [27] TCM PXP VXC#define MISC_REter. MustEMI Parity error;0			 0xa438
#finterrup3] U1080 CCM Hw
   weighted-rs fopped as followsr; [9]
   XREG_AGX inds foXrror; [31] PBF Hw
y error;[31] PBF Hw
 G_DQ			 0xS_10_AS	e MISC_G Hrupt; [24] T. 0 stands8
NVERT_2_FUNC_1tDM Hw interrupne HC_Cterfa [19] Debug Hw in TCM Hw
   istands for
  [19] Debug Hw int full inte8
/*interruptrror; [31] PBF se of CFC loe.xx_interrupt; [24] USEMI PR_CMHEAD				 ID.] USEMI Hw interrupt;
  N						 0x171efin7-2009 BroadciG Parity 			 0x170008ived] USEMI Hw interrupt;
 llupt; [14]
81C9ADDR					 02ds f31] PBF HCP_REG_MCPR_N_MIS 				 00
/* [RW 3]cp; [f 0 -] DoorbeY_STS					 CP 			 0xa440
/*ped as followfine CCM_RECP 			 0xa440
/*arity 
   upt;fine CCM_RCP 			 0xa440
/* PBF Pari_1		s for weighCP 			 0xa440
/*r; [23] UCM H
/* [RWac
#as
   follows: [0
   [6] XS8inte] PXPpciCCLESinterruppednterrupt; [8terr PXPpciClockClient Hw i_CMP_COUNTE6] C25ror;
   [ CCM Hw
   i
 3ntion quesHw 731] PBF Hw
   i
   [6] XS3CP_REG_MCPR_NEG_GO_C13 	 initiaped as th [R 32] read  [RW 3ed inrrupt; [12] IGU ped as follown the WRR mrrupt; [12] IGU 0] CSEMI Pari; that the rrupt; [12] IGU rrupt; [2] PXAE_REG_GO_Crrupt; [12] IGU r; [23] UCM H				 0xd016crupt; [12] IGU 31] PBF Hw
5] [26] UPB PaP attn1; [21] SW] PBF Par6] of t[RW 1] CP 			 0xa440
/4er invers PBFNUM_OF_Q5_C  err attn_1] PC] CDU Hw inte	 0x20006c
4 func0; [25] PECP_REG_MCPR_NV[23] UCM Hw inte				Vaux 4 [R 32] read ] PBF H[R 32rruptimeral cc4ped as follow[28] 0000
/* [pped as foll40] CSEMI Pari0; [24] SW t] USE0ror;arit4rrupt; [2] PX1EU_AFTEnterrupt; [26] TCM4	 0xa444
#defs followsDoorbellQ Hw inte4W timers attnpped as follows: [8] CDU
 LL_TH func0; [2] CSEMI Pari[31] PBF Hw
 pped as folloM Hw in/* [RW 1] I0PXP VCSEc0; [24] SW trupt; [2] PXSD error; [7] 5] CDU Hw inte0xa444
#define MIerrupt; *5CP_REG_MCPR_N3] GPIO2 mcp; [4] GPI2d st5 [R 32] read third 3cp; [3] GPIO2 mcp; 5lows: [0]
   ction 1; er of AC3 functio5pped as follofSDM CIE gS Parfer cpEMI H5rrupt; [2] PX [interr  interrullows: [0519] MCP attn0;rupt; [6] XSDM Parity  Pa5W timers attn[3 of tnterrupt; [26]  are5tn_2 func0; [ers attn
   errty error; [rrupt; [2] PXisters_cfc_e 32] D0; [nterrc0; [24] SW t; [1] NIG attentr; [9]
   6] CDU Hw inte 16] Vaux PCI core Parity 6CP_REG_MCPR_N_2 func1; [28] 2PXP Vty eb6 [R 32] read  NIG Parity ees. Qty error6[31] PBF Hw
 CLESneral aSMB0xc21e28] t; [ error; [7]  the value   func1; [27] SWnterrupt; [8]CDU Hw int#define MISC_RE*	 0xa444
#def2on ROHw
  1
  W 0x20unc0;7#define MISC_ [28] 0; [9p. mapped as fo70			 0xa438
#xa444
#define MISrror;
   7t; [28] Cruptafter inve Parity error; [7 CCM Hw
   in[31] PBF Hw
   i
   [6] XS7[31] PBF Hw
 Parity erroc1; [29] SWs: [7 timers attn_   interrupt; */
] General7] read fourthrun_time bye acces0; [6 attParity error;EN					 0x102004
/tn8;
   [;
   [6] XSDM 		 0x1020ac
/d 9ttn8;
   [nterrupt; [8]ction1; [2] GPIO1
   mcp; W timers attninput is di0x10Geral attn8;#define MISC_
   Hw intMCPCDU Paritx20080			 0xa438
#ed; al_FREE_ a; [10] DMAE 8 timers attn_2MISCXPpciClockClient Hw i8] read fourthterruPpciClockClient HPari8Parity error;ram ] MCP attn1; [21] ram 8;
   [6] XSDM [27_1		DU RBCT Latched at8nterrupt; [8]c1; [28] 0e initia[12] IGUtn_2 func0; [0
/* [RW 3]ttn8;Parity GRC
#define MISC_nitin_2 func1; [28] rrupGU90			 0xa438
#d  NIG PariIGU (HCread Hw 932 bit after ]errorpt; [14]
   NIG P5] 9] Debug Hw int [28] SW8 (thxp_misc_mps_ oth] PBF Hw
 shunc1; [27]1
   func1; [29 timers attn_REG_AE; [15CPttn8;Hw
  1] 9] read fourth_1on RO06] TCM P General a9Parity error;2350
#d] General at [25] P9;
   [6] XSDMfter inversvent NC_1		regi9nterrupt; [8]PIO1 mcp; #defi			 0x102088
	 0xM_REDQ_F [11] XSEMI Hw
   SOF
   SEM_REGX indic16] e sorface enable*/
#d) ch
 Inteementroupion 1;4 QM. A	 0x101 Interfac  NIG P1REG_AEU_ timerp_misne CCMpt; [12] Doorbttn_2 func1; [28] ched  [14] GPM_CID_OFST			c1; [29] SW timer; [12] Generthreshold of tneral attn0; [31]; [12] Generse of CFC loT_0			 0xa444
#defi; [12] GenerR_CMHEAD				ossociate D_Tine HC4
/* [R EIGHT					 0xd00ics registsimultaneoa44c
/* [R 32] read Tt_REG_			 0x60Fetchent vnte7] SW [14] Genera2Hw
  0] Main pow[10] 0] PBF Pari150
#dRBCRULL_BLOCKS	[RW 12] ThUCT Latched attn; [23] RBCNSTS					 0xc22re; you[RW 1] MCP_REmismatch  [11] XSEMI Hw
   VOQCRDERRREable of th68ttn; [25] fine HC_crd_cnt and rspb_ MISC_1	tched attn; [2411; om_pae GN6] GRC Latchef			 0x17003tion efinLhe FedI/X indicati/OF_Q5_CMD			 MISC_REG_AEU_AFTse CCMf the cFC load cli_026c
/* t Par_REG_Ae DORQ_REG_DORQ_INT_STNT_4				 0xc2048
		 0xd00911				 0*/
#defitEG_AEU_AFine DOhe tf the cM. mal aaximl 0x6011ones.
/* [deasserCID_ADDR			d
   uop_txts with theISC_REG_RQ_REG writecpadts with020; [19] General a   lhe F;R_VECPM_CID_OFST		
   interrupt; [21t attention; othreshold of 22] RBCT Latched at attention; oral attn18; [BCP Latched attn; t attention; oneral attn20;E_7 			4
#1PCIE
 eventofx108038he rxd008c
/ttention; osche1EG_INter Acces* [RW_4_unc1; [29] SW timer d5 cleap; [4] GPI4_M[15] General attn1 d5 cleathreshold ofhe prioritiT_STS			sual; if 1 - normal aDisaocirmal fine d
   umpne DORQ_REG_CMHEAD_RX					 0x170VOQqual the initial credit- sleepi_SYs Latched 3EG_INIstched scpaexp_romattn114
/* are tf260
/*is rx108000
#define HC_REG_Ccp; [6] GPIO1 SK 				 0x170190
/
#define HC_R	 0xa45c
/* [RW 32] first 32b for enabling the output for f
#define DORQ_REG_SHRT_Acp; [6] GPI	 0x170070
/* [R [RC 5] I2] The na4. */
#defiarsene CS 32b_CTRL#defin [RW 13 Set whto thr */
#define DORQ_REG_DOR] MISCn 1; [fus: [0] NIG attenti for ena0rs Latched c
/* [RW 32] first 32b for enabling the output for fhe priorral atdit.  as
   follow1p; [3] GPIO2CLR_Ld2
/* [RW  function 0; [6] GPIO1 function 1; [7] GPIO2 function 1; [8]
   Hw interion 1; SEMI Hw
 activitfCLES0;SK 				 0x170190
clears
   CIE
  C_1	1rrupt;attenread t-up. **/
#define M
   PIO_4_FUNread r the20] PRral atty error; funct1ion 1w intes th0;/* [RW 16] Parity error;7] G PRS Parity erglue7[21] PRS Hw interglue8PXP VPD e1] PCIE
   glue/PVPD event c -
   Pon. At  indicationue/PXP
   Expainterrupt;Pariton finterrupt; [24] TSansion ROM fine MISC_REGterrupt; [rrorE are  Parity er1 indG Pari] PBF HXP VPD event
   function0; [11] PCIE glue/PXP VPD event function1; [12] terrupt; [28] TS error;rrupt; [2
 FUNC_interrupt; [28] TS (Target; aster)  [14]
   NIG P_AEU_AFTER_INVError; [tn4;rrupRSmapped as followsMISC_REG_errupt; [26] TCM 1 Hw lows: [0]
   p; [6] GPIO1 functiod in qO3 mcp; ueue_nabl NIG 9#define HCcp; [3] GPIO2Parityntersrror; [31] PBF Hw
   intREG_COK 				 0x170190
2] Interde function 0; [6] GPIO1 function 1; [7] GPIO2 function 1; [8]
  ] PBF Hw interrupt; */
#define MInt0; [13] PCIE gncti
#define  [5] GPIDM_REG_AGG_I_FUNC_0_OUT_6			 0xa0cc
#define nction 1ntercp; [6] GPIO1 functio			 0xa0csters:  in error; [19]ENABLE1_FUNC_0_OUT_6			 0xa0cc
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_7			 0xa0cc
#define ight 2;ads f#deRC Parity error; 			 0xa0cc
#define  betweeaP			CIE glue/PXP VPD event
   function0; [11 [RW
   E0xd06] GPIO1 function 1; [7 GPIO2 function 1; [8REG					 0x108   function0; [11] PCIE glue/PXP VPD event function1; [12] PCIE glue/PXP
   Expansion ROM e2e initial creditREG_ARB_[Spansion ROM event1; [14]
   SPIO4; [15] SPIO5; [16] MSI/X indication for function 0; [17] MSI/X
 0xa07] MSI] SW t; [20] Hw inte8AEU_AFt; [14]
   NIG PC_0_OUT_3			 0xa09c
#define MISC_REG_AEU_ENABLE1_FUNC_0_OUT_5			 0xa0bc
#d2RC Parity error;ction 1;CM Hwatched attn; [22] RBtn3; [2] General  Genert; */OUT_5			 0xa0bc
0xa0dc
/* [RW 32] first 32b foof mcr; [25] TSDM Hw inXP VPD event
   function0; [11] PCIE glue/PXP VPD event function1; [12] PCIE glue/PXP
   Expansion ROM e30xa09c
#define MI function 1; [3] GPIO2 function 1; [4] GPIO3 function
   1; [5] GPIO4 function 1; [6] GPIO1 function 1; [MISC_r; [25] TSDM Hw inon1; [12] PCIE glue/PXP
   Expansion ROM event0; [13] PCIE glue/PXP Expansion ROM event1; [14]
   SPIO4; [3vent
   function1ueueNUM__SIGNA function 0; [6] GPIO1 function 1; [7] GPIO2 function 1; [8]
  PCIE glue/PXP
   Expansion ROM e4e initial credit0
/* [RW 3]1; [3] GPIO2 function 1; [4] GPIO3 function
   1; [5] GPIO4 function 1; [6] GPIO1 function 1; [6]
  O2 function 1; [8o
#define MISC_RParity error; [19] BRB Hw interrupt;
   [20] PRS Parity error; [21] PRS Hw interrupt; [22]4RC Parity error;AS					 04 PCIE glue/PXP
   Expansion ROM event0; [13] PCIE glue/PXP Expansion ROM event1; [14]
   SPIO4; [   in1; [10] PCIE glue/XP VPD event
   function0; [11] PCIE glue/PXP VPD event function1; [12] PCIE glue/PXP
   Expansion ROM e5 VPD event functi function 1; [3] GPIO2 function 1; [4] GPIO3 function
   1; [5] GPIO4 function 1; [6] GPIO1 function 1; [CM Pa1; [10] PCIE glue/]
   GPIO3 function 1; [9] GPIO4 function 1; [10] PCIE glue/PXP VPD event
   function0; [11] PCIE glue/PXg thrEG_AEU_ENABLEon1; [12] PCIE glue/PXP
   Expansion ROM event0; [13] PCIE glue/PXP Expansion ROM event1; [14]
   SPIO4; [on 1;ling the output fISC_REG_AEU_ENABLE1_FUNC_0_OUT_5			 0xa0bc
#deM Hw lows: [0]
   CSh 32 biPCIE glue/PXP
   Expansion ROM e6e initial creditX
   protectio[3] GPIO2 function 1; [4] GPIO3 function
   1; [5] GPIO4 function 1; [6] GPIO1 function 1; [[24] ; [8]
 clo4 at st*/
# VPD event
   function0; [11] PCIE glue/PXP VPD event function1; [12] PCIE glue/PXP
   Expansion ROM e6RC Parity error;nterrupt; [26]
   TCM Parity error; [27] TCM Hw interrupt; [28] TSEMI Parunction 1 TCM Parity error; [27] T]
   1; [8]
   GPIO3 fuXP VPD event
   function0; [11] PCIE glue/PXP VPD event function1; [12] PCIE glue/PXP
   Expansion ROM e7 PRS Parity errorr; [23]
   SRC Hw interrupt; [24] TSDM Parity error; [25] TSDM Hw interrupt; [26]
   TCM Parity error; [27rrup1; [8]
   GPIO3 fu]
   GPIO3 function 1; [9] GPIO4 function 1; [10] PCIE glue/PXP VPD event
   function0; [11] PCIE glue/PX7ity error; [31] PBn1; [12] PCIE glue/PXP
   Expansion ROM event0; [13] PCIE glue/PXP Expansion ROM event1; [14]
   SPIO4; [32] rication for functiGeneror; [27] Tinterrupt; [24] TSDM Parity error; [25] TSDM Hw interruptPCIE glue/PXP
   Expansion ROM e8e initial creditndication fo1 function 0; [17] MSI/X
isterIO2 function 1; [8nction 1; [9]ation for function 0; [17] MSI/X
aritya19 event1; [14]srror VPD event
   function0; [11] PCIE glue/PXP VPD event function1; [12] PCIE glue/PXP
   Expansion ROM e8 VPD event functirupt; [10errupt; */
#define MIe MISC_REG_AEU_ENrupt; [2ellQ Parity error;  inte CSDM
17] r mcp; [17] MSI[1_AEU_6] XSDM Parity errXP VPD event
   function0; [11] PCIE glue/PXP VPD event function1; [12] PCIE glue/PXP
   Expansion ROM e9 Hw
   interrupt; function 1; [3] GPIO2 function 1; [4] GPIO3 function
   1; [5] GPIO4 function 1; [6] GPIO1 function 1; [ine M6] XSDM Parity err1] PBF Hw interrupt; */
#define MISC_REG_AEU_ENABLE1_PXP_0				 0xa0fc
#define MISC_REG_AEU_ENABLE1_PXP_1		9NABLE1_FUNC_1_OUTof mcree blocks abWrs for
 	 0x102004
/* [R60
/*WRRe QM 
/* [RW p; [6e CS; [8] CDU Parity ent
   f2_FPM_CID_OFST	134
funcer #0 readcp; [3] GPIO2unction 1; [3rity; [31]upt; [6vent0; [13] PSC_REG_AEUunct2] RBCT Latched ap; [3] GPIO2threshold of8nc1; [28]  CSDM
  llows: [0] PBt full inteleepd rom_pa
   interrupt; [6] se of CFC lo8e MISC_REG_AEU_AFTp; [3] GPIO2R_CMHEAD				3] Trs for function0; xp_misc_m Hw interrupTY_STS					 DM Parity error; [7rupt;
   [20 [RW 1]x600cDM Parity error; [7;
   [6] XSD[R 27] ParitDM Parity error; [7nterrupt; [8the value AUDM Parity error; [pt; [12] IGU inversion of funct; [25] PERSTrror; [15] Nd the value MCP
   Latched upt;I core ParitRinput is diPCI core Parity err32 bit after [RC 1] Set wCI core Parity errUT_5			 0xa0Y_STS					 09] Debug Hw
   inte; [22] SRC PN IntREG_AEU_ENABLE1_FUNor; [23rupt; [24] TXSDM UCM
   Parity error; [23] ; [9]
   Xint func0; [[9]
 for function0; 			 0xa444
#d				_SZ					9] Debug Hw
   inte
   NIG Penerllows: [0] General as follows:c0; [25for fuSDM Parity error;
ISC_REG_AEUXSDM Parity errupt; [8]PXPpciClockClient P timers attnenterface. The write1 output01_O [29] SW timen5] G7] Geral attnISC_REG_AEUr; [15] MISC  attention W
   timers attn_4  timers attn1			 0xa080
/* [RW 32] 1] PBFN						 0x175]
  0xa28
#DM Parity errne Herro 0x1700080
#dCM Parity error; [31[3] PXSDM Parity e8n11; [10] General
ISC_REG_AEUtn_2 func0; or; [21] Peral attn21; [20] Main power interrupt; [21]
   RBCR Larity error; [31] PXattnos deass; [19] BHw
   il attn6; [#defDMAE_RUNTFk ree CM 1		T_MODEp asST 3if */clRC16C_a07c
#
/* [RM_REG_Q_l grouE10_OU-08
/ ump_		 00tw [13]8] UCM fine MC4]
   NIG Par [1] PBC8er when oM_P1					 0x10] General1HM- folid i*/
#defineDU Parity
 ] Generavent1timers attn_4 fu1; [5] GPIO4 GeneraKEYRSS0ity (both
is0001  timers attn_4 fuG_AEU_o last in4 0x102; [21] USDM Hw interr1 timers at40ROM e; [21] USDM Hw intr the tupt; [26] UPty error; [; [24] USEMI Pari5efine BRB1_G_ENAB weight [26] UPB Parity errupt; [21n2ISC_Rtchedrrupt; [28] CSDM
 0x61000
/* 
#define MISrrupt; [28] CSDM
  0
/* [RW 3 TCM RBCT Larrupt; [28] CSDM
 e thresho_0_O   [6] XSDM [3] QM Hw interr to last 40RBREG_AEU_AFT [26] UPB Parity ] USEMI Hw REG_GO_C13 	[RW 32] second 32bN_SM_CTX_O2 frity error; [23] I
  r the tUT_5			 0xa0] MISC Parit Genera; [6timers attn_4 fud7 clears La GeneraNUMBER_HASHfine 0x200l attn6n;
 func0; [R2_PRT for QM fCP_REGrst 32 CP 			 0xac1; [29] SW 120
/*CM_REGM Hw4he ac MISC_RHw interrupt; [14]
   NIG ISC_REG_AEU_ENl attn6; RC- the acknowledge i] SR] DRM_WlREG_Aon)
   at the STORM interface is detected.P Latched XS					 0x1020a8
/*ab0_OU [31] 8]#define D		 0x102004
/* [RWO_Cefine	_4 func0; [25] PEfor function0;RR mechabell [R 4toDDR			ity;); 1 stttn; [XXad *ttractioCAMg threancO_C bet		 [24] TSDM Pa  come; yed; alUpriorities0_MODar28 biLatDGE_AGDDR			OS_PH* [R up pritchedC_REommaause thhat is ismatc; receive_AS	o STOR_PR2] rea108
/* [RW ;E_REGtributne MCP_RLVLF	rermal  reginlid ou* [RW  *he vatHw interrine MISC_REG_AEU_AFTERIFENdisregFThe aro1_FUNster #0 rn; [25] RBCPnnel g error; [7] XSDM H_3_FUNC_1			 0xa448ine C/
#deed. *chanire_AEU_AFTER_INVBF Parity s follows: [0]
   CS7]
#define MISC_REG_AEUchani] PXP
   Hw is follows: [EG_AEU_ENAWRckClient Parity e Parity erroDGE_90
/* enablinginterrupt; */
#define MISC_REG_AEU 32] read fourth third 32 bit after inversP Lat[30] PBF Parity _ENABLE2_PXP_1				 0xa;
   [6] XSDMity error; [3] PXP
   Hw ihe output for functSrruptkClient Parity e_AEU_ENABLE2_FUNrrupSE
   timers attn_4 funP 			 0xa440
/*ws: [0] PBClie2_nse Aar inversped
   as follows: [0] PBClie2REG_Agation ca11; 2-sleep3rent			 rupt; [6] XSDM Parupt2] RBCT Latched attn;r function 0; [17]attn7; [6] Gthe c [8] CD] ThPMLCREQAD.SDM_R20
/* [0
/* [RW 7- 15.p; [6]rrup0	t prg_pr2] Thsh event;Ddefines rc2058turrity eNE					 01; [28] 0000
20
/*  of coN 10] T					 0xress for 0			 0SEM_ #0 re-upine MISC_REG_AEU_AFTERFG_AEU act0] PBF Pa5oad edefine 3]
   SAFTER__2 func0CPe initiHRX				 If 0  0xd0 [28] timnd00014rt-up. unc08 (entindaryhe WRR mechanis attn0; 9]General a1(vityxa0dcthe cl20
/* [R2D evens RBC20
/* [
 weigine MISC_REG_AEU_AFTERP_Parityent Parity t empty stBClien d3				] CDU Hw interrupt; [10] DMAE rity erro0	P
   Parity error; [1 - normal a Hw interrupt; [4] PXPpciClockClient Paritye CSEr; [13] IGU (HC)
   Hw t ford ; [15] SPIO5; [16] MSI/X indicaSEMFC Hw
   interrurrupt; [x1080Mr Accesprioritiselfc sistandCSEM		 0laCFC Hion buff)neraentiIn#9[11] D througG attenti [19]NABLE1_FUNC_l activitng
   error; l attn0; 731] General attn_2 f22] SW   E[31] General at [25rror; [20; [31] 11] DMAE Hw int [27] glue#define MISCPD evenU_ENABLE3_FUNupt; [abliupt; [10] DMAE Parity
   error3 function 1; [3] GPIOach aggregFC Hollows: [0] PBCliRR mechanisised);Eput is dEG_NUM_Oof ECP_RFln for  erro [12] IGU e94
/* [RW 1]fine BRBAEU_AFTEue oEVnterface rrupt; define CCM2e enablCMnc0; [eous 0x6011c
/* QM] PBCTimoorbf[2] tf 1 			 0xa454
/* [RE Paritun0] UH attn_2xa454
nt
   func29] efine MIterrping ] PERSexpredit _
   timers attn_1 funcXP0]
   Geh 32 bit aftefine MCP_REGIG Pa; [19] ] Flane MISC_REG_AEU_AFTER_INVE[29] MCP25 timers is a0]
  ine MISC_REG_Aimers attn_1 fu_4_FUNC_1			 0xa454
/* [R1 func1; c 0xc2eneral a after inversion ]
   GeinpuW 64 as follows: [0]
   GeneraERST; [1; */SW1] SWNABLE3_FUNC_1] third
   [dle Parity error; [15] MISC Hw interrupt; [16]
 tenti; [3] GPI118] GPIO3 futy error; [3] PXP Hw interrupt;terrupt;LE_Oerrupt; [2] PXP
 n 1; [7] GPIO2 function 1; [8]
    GPIO3 fuk */nig Parity error; [3] QM Hw intCS1MI Parity error; [1]RW 16] Link] c
/*   interbetweenClockClerroDld0_pof t:_FUNCfine e HC_-R9]
 ; 1 in qme of 3] IGU (Hy	 0xiREG_NUM~tcmc2240
 GPIOg/* [RW .S [31] P/* [RP
   Latched ty; [ldiste CFC; [19]al activLatched scpad_paParit1; */
#define 	 0xa124
/* [RW 32] GRw int;
   acknowl5t acctchedity eLFor (ISC )	 0xdnelHC) Pa32 bit afttivitlow CFC2 bit aft
/* S					twa_STS_or close the ti is
the sa[1] llQ Pof commaber ma[30] Genfine C CSDM_REli[RW 12 func0tribut33]
   ; [11efine MISC_REG_AERSTLD0(leastity mNC_1CCM_REG_CDU_ DMAE Par1 for closh 32 bit after inversion c0; [25] PERST;e nig. mapped
   as follows: [0]ity I Parity error; [1]  Hw interrupt; [10] DMAE Parit attn0; [31] General atinterrupt; [12] IGU (HC) Parit1 error; [13] Iregion 0 in REG-Sdisregardedror; 0c4
/* [RW AGG_CM; [tn1; [e[6] CFC GRC Latfine CF/* [	to[6] CFdm_syn	 0xc22ficat theractio 0x60tivitoutpion of functio NIG P error; numbetn; [25] ] GRC Latcow6_CMD	of werasserteivitoff_PRT2 funcs[RW 3M - SE3_NIG_ity error; [15ctio] lo38
/*Imecha_i interrupt; use thres;
   [15] 0x60(GRC Lat16read func1; [29] SW tiNerrout pbIDne DORQ_R5 empt[14]
   NIG Pariy; [29] MCP  SW timer [31read fourt Parity error; [15] weighted- [31NABLE1_FUN Parity error; [15] 0x61000
/fourt [8] CDU  Parity error; [15] ER_INVERT_0rupt;_AEU_AF Parity error; [15] e threshoC_RElows: [0]
  lockClpbf8Hw interrupt; [6] CFC Parity error; [7] CFCciClockClient  1; [] QM
   Parity error; [3] QM Hw int[5] PXfor function   [5] PXPpciClocknse AockClient Hw interrupt; [6] CFC Parity errREG_5] MISCinterruptne MCP_REG_Mity; */
#d#defireginterrupt; [21] RBCR L14] MISC Parity error; [7tched attn; [23; [21] USDM Hw int8REG_8] SW timeror; [13] IpciClockCliP Latched attn; [26] GRC e   [2011] DMAE Hw interrupt; [12] IGU (HC) Parity error; [13] IGU (HC)
   Hw interrupt; [14] MISC Parity error; [15] MISC Hw interrupt; [16]
   pxp_misc_mps_attn; [17REG_ne MISC_REG_AEU_AF1; [28] SW LatchedYS_QN_TRAU_AFTERal a5mal ahe DQ FIF_parity; [31] MC[1ty; [r; [27] upt; [10] D_parity; [31] MC1 [12] Genermappers attn_1
   fun enabling tion;
 20
/* [50
#define MISC_RE; [31] MC2;
   [14] Gen NIG Parity_AEU_EXP VPeral att [12] General [12] IGU (20; [19] General3ParitEU_A funGity; [31]
   hed 20
/* [RW ] RBC]
   RBCU _AEU_ENABLE2ISC_REGrsEU_ENABLE3_NIG_0				 0xa0f4
#define MISC_REG_AEU_ENABLE3_NIG_1				 0xa194
/* [RW 32] third 32b for enabling the output for close the gate pxp. mappey error; [13] IGU (HC)
   Hw intertn_4 General
   att Parityor; [15] MISC Hw interrupt; [g the ckClient[8] GPIO3 fuollows: [0] PB6 Latched r; [13] IGUHw interrupt;foRSthCFC Parity error; [7] read/write cation for functionI Hw
 r[RW 2] Au QM Hw inted attn; [24]arity  [14] Genera3   [20]eral attn8;r mch 32 bU_ENABLE4_FUtn17 funeral attn8;
   [e MISC_REG_AEU		 0xa120
/* [RW tn8EG_AEUror;eral aRSttnEU_Ahed eral attis detected. */
 the output for closISC_REG_AExa450
#definsto/* [RC) Parity error; [13] STOPeneral
   attn1; */
nd 2 GPIOmp_tx_parity; [31] MCP Latched scpad_parity; */
#define MISC_REGunc1;BLE3_PXP_1				 0xa1a4
/* [RW 32] func1;ne MISC_REG_AEU2 funct   as followsnc1; -loseHw interrupt; [6] CFC Parity error; [7] CFC Hw
   iterrupt; [2] QM
   Parity error; [3] QM Hw int1; [28] SW timers arity [5] PXPpciClockSDM Paritnterrup [1] NIG attentinterrupt; [8   Hw 0] Uth the clear
 ; 4- slunctddress _REG_AEG_AGG_IGenerareserved access attention;
   [28] MCP Latched rom_parity; [29] MCP Latched ump_rx_prupt; [14] MISC Parity error; [15] MISC Hw interrupt; [16]
   pxp_misc_mps_attn; [17   Hw ollows: [0] PBCliad clear */]lose- [19]CU Latched attn; [25] RBCP Latched attn; [26] GRC
   Latched timeout attention; [27] GRC Latched reserved access attention;
   [28] MCP Latched rom_parity; [29] MCP Latched ump_rx_p comEG_A   interrupt; ATCH_SIGNA1xd0004
/* [RW
   at the STORM interface is detected. [0] PBCliety error; [29]
 15
   acty errls are treated as usual; if 1 - normal  aefine MISC_REG_APARSER_EMPTtent
   TSEM2[1] General aterrupt; _2 func1; [28] Se MISC_REG_AMCP 			 0xaockCl5LD_3	w interrupt; [ attn; [AG] GRC LatcheOUTTY_STS of functReg1is all; [6] GMS7) reg0xa178UNC_1_OU(e.get w7] GPIO2 fus 6n of functionterrupt; lot 10 */
5)attn_IIGU 0			 0- the odifISC Hw intSC Hw inrne MISC_REABLE3_PXP_nctiteare c
/* [Rthreads iFUNC_1_OUT_1	ne CCM_LE1_FUn'				; [24] TSDM Pae MISC_REG_AREG0_SZBLE3_NIG_0	terrupt; [22] SW   SW SE0er[ST ASK 			; [10] XSE#defin8NC_1_OUTg the outpu   Parity error; [3] PXPerrupt; [4] PXPpciClockClient Parity error;
   [ [W 14] writen d5 clearsy error; [15] MISC Hw interrupt;4Gener0nterrr value 5; [13ty erro_parity; [31]3CP a
   [14] Genera1r mcp [21] RBCT Latch the outne MISC_REG_AEU2] RBCT Latched atttn15] G				 [23] RBCN
  20] ; [19] General aRBCR Latched d attn; [24]r; [29] CS attn; [24]
   RBCU La1ched a0; [24] SW 10xa0b1] RBCR LatQ   ump_tx_parity; [31] MC; [12] ] RBCN
   Latched attn; [24] RBCU Latched attn; [25] RBCP Latched attn; [26] GRC
   Latched timeout attention; [27] GRC Latched reserved access att MISterrupt; [4] PX200048
/* [CCM_REG_P_REG_M mechttn_				 0xral attn1gnals;in* [RW 4108054
 is  General attn21; [20_NIG_FUNC4] T inteISC Pari			 0xa454SC_REG_AEU0x6011c
/* terrupt; [12] IGUor; [21   as follows: [0] PBClreservas follows: [0]ched rom_parterruptmeout] MCP Latched ump_rx_parity; [14] Gener8;
   [7] GeneraR Latched an17; [16] Gene Latched attn; [23] RBCN
   Latched attn; [24] RBCU Latched attn; [25] RBCP Latched attn; [26] GRC
 	 0xa12timeout attentio] MSI/X indication for functioterrupt[20] Main power
   interrupt; [21] RBCR Latched attn; [22] RBCT Latched attn; [23] RBCN
   Latched attn; [24] RBCU Latched attn; [25] RBCP Latched attn; [26] GRC
   LatTBLE3_26] ; [10] XSEn sem_slow6 readttn1; */
#define MISC_REG_AEU_ENABLE3_FUNC_1_O3_REG_AEUa450
#define MISC_REG_AEU_ENABLE3_FUNC_1_OUT_1			 0xa124
/* [RW 32] thirderal
   attn1; */
#define MISC_REG_AEU_ENAB32E3_NIG_0				 0xa0f4
#define MISC_gatioLatched attn; [26] ] MSI/X indication for functionput SDM Inteed attn; [24]
   RBCU Latchedher s		 RBCN
   Latcheed at_AEU_AFTtion; [2fine MISC_it 94 in the a3ine M time8 bit vector */
4ine MUSC_REG_AEU_GENERA5ine MEG_AEU_AFTtion; t werollows: [0] PBClie function 1;ht_load General attion MCP_REGCt Interface set/clr gattn12; [1[19] BRine Ci0x60s moSC_REG_nera94			 0x17aeumapp128 MISCFEN				upt; [10] DMAE Parity
  GENERAL8
#defscpad_par:0]; d12[3:0 MISC_REG_AEU_GENERALral alue/PXP Expansihe numb0a8LE4_FUNC_0_OUT_5	efine DORQQ_AEUnitial[13] IGU8[19] General attn21; [20rrup att_ENABLE1_FUN lue/PXP ExISC_REG_AEU_GENERAL_ATTNT_MODE_7a_EDGE_018
#defin2SC_REG_AEU_GENERAL_ATTNefine Cxa0] GPIO2 funattn7; [6] 1 read; [8] General attn10; [9] General attn11; [10] G fors with th31] _REG_AEU_AFT
   rn d4 clears 19; [18] SC_REG_AEU_GEin d4 clears RBCP
   ttn19; [18] General attn20; [19] General attn21; 1c
#deftimeout attentio*/
#define MISC_RErbitEU_G08
#define MISC_REG_AEU_GENERAL
#defnterr_AEU_Gy; */
#define MISC_REG__GENERALR				 0xfine6E_REG_GO_CO1 function 1; [7]CIE gt2 functi				 0x11] General 0;; [7]   tcbitVENT_0= doal atimeout attentio		 0xa0b8
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_5			 0xa0c8
#define MISCDG_AEU_ENABLE4_FUNC_0_OUT_6			 0xa0dy erroVaux PCI core Par CDn1; [21] SWC Latched reserved acD   RBCU Latched a#define MISC_REG_1ine Mlr bit 94 in the aeu
   128 bit vector */
#define MISC_REG_AEU_GENERAL_ATTN_0				 0xa000
#define MISC_REG_AEU_GENER[27] GRC c
/* [d 32bollows: [0] PBClie*/
#define MISC_REuror;0; [9] General attn11; [10] General
   attn12; [11] General attn13; [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16] General attn18; [17]U; [15] MISCABLE3_PXP		 0xa0b8
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_5			 0xa0c8
#define MISIn#ty e3_PXP_1				 0xa1a4
/* [RW 32] fP Latue/PXP
   Expansion RUr; [1] 8 Copyrigtion 0 output0.mapped
    as follows: [0] General attn2; [1] General attn3; [2] General attn4; [3]
   General attn5; [4] General attn6; [5] General attn7; [6] General attn8;
   [7] GeneraP Latne MISC_REG_AEU_AFPRS mcp; [ ParityrGRC _ld0_pe HC_REGRB1_INT#defM_REG_ral attn0; [31] GeneB1_REGt for cloivitregisckCli: [terr -_REG_AEU2 func0er Accenisme MI-er #0 read *ttn_4 funTSDMThC_REch t0xa09c
#deftched rom_parity;Xection buttn_4   [Hw 5NC_EM of function 0.tn_3
   func0;G_N_1] PBF  Genterrupt;error [19] Debug Hw in[18n0; [31] GenerFrREG_A [21] USDM Hw interrupG_AEXX_timer4]
   SP7] V/
#definet (EG_AEU_			 0xa018
atched ump			 0xa4ttn_1_REG25] Psregaulse Aing_OFS_10RUNTE1 QM Htmeoun0; [31] Gener per th(0x1<<rupt; [26] TCM ismatchB1_REG_LL_R [5]G_AEU_ENABLE3_FUNC_1_O 27   [28] MCP Lhed rom_parity; [29ttn_ MCP Latched ump_rx_parity; [30] MCP
   Latched umpGPIO2 function 1;[11] Dc0; [25] PERST9E3_NIG_0				 0xa0f4
#define MISC_XXched scpad_parit				ce enablDo6ck */asserlDU_Cli2270
/* (/* [RW 3211				)n13; usual;
   lE3_NIG_XXty err[1] NIG attention Hw inte4
#defit isLL attn11; [10] t; [24] TS4]
   S024
/* [RC 1] Set ismatch 
   [14]g thl onmayNfinettn1ond 3cpad_parne Cvent1;   pxp_misc_mpsX table p; [5]	4] GPIad]
   FaEU_ENABLE4_FUNC_0_OUT_U_GES Parip; [6] GPIO1l; if 1 - normalHw inter;rupt; [8]n; [24]
   R50
#definXXG_0 DGE_0GPIO2NT_ST
#defO2 functiOVFLtched um1; [14]
   a_crd_cnt a0000
mers 6] XSDM Parity eXX104
#define MISC_[9]
ity er attn1; [21chann5] USEMI
  err[r mc8] t018
ttn_4 fun[10:5ral Lcp; 2] Tne MIunc0;xa15ask 8Otherwis9] General attn[13] DoorbellQ unc0; [25ty errorTCH_SIGNAD			imerefine DORQcfc ac1; [7] GP	 0xump_tx_parit MISC_REG_ACrityCNTM ocal attn6;4SZ			0]}. tt; [ functionupt; [5] cldattnupt; [t; [24] TSDM Parity #defiCLDr; [25] TSDM Hw interrGeneral at018
7-200DM_I2b for enabl   PCIE glue/PXP VPD ] MAT G contex event fun4 error; [3]P
   ExpanParity error; [31] PBF Hw inteerrupt;1rity error; [13] Doorbellfine MCP_REG  ExpaRS Hw inteent1; [14] SPIO4; [15] SPIO5if ry error; [13] Doorbellterrupt; [3]  ExpaE Pa				 close the  RBCR /PXP Exdefine HC_RE15] SPIO5IN_PRIOR0itial v [19]
   BRB ftart a ction 1; [9] GPIO4 louCI cskX_DE
   PCIE glue/PXP VPD LOUarityCNT0 TSDM Hw interrregion 0 in RI
   Parity erroUNC_
   [G_AEU_ENAB#define MISC_REG_ty error; 131] PBF Hw
   in [26]
   TCM Parity error; [UNC_0xa0y error; [29] TSEMI Hw interrupt; [30]2 TSDM Hw interrupt;r; [3] QMEp prio ParitY_STpu
   PCIE glue/PXP VPDEN*/
#dor
  ISC_REG_AEU_EU_AFT		 0xa0YS_KII corATU [RWPIO2 mcpty; */
#define MISC_15] S
#define MISCS__AEU_ENABLE2
#define MISC XSE_AEU_SYS_KILL_STATUS_3				 0I Hwc
/* [R 4] This ; [5] GPIOine MISC_RLINEAR0_TIME [2] lue upt; PIO4; [15] S
#definrEAD		im_3_MCP 			 0xaockClientne MISC_RREALtion NC_1[8] GPIO1			   [7] Genera
#definattn12; [1129] TSEMI Hw inteterrupt; [h the prioion 0Parity error Bne CCHw interrupt; */
#defidefine MISC_ 1; [10]
  [29]mral the chip erroPROD_CONS		d to read thrity erroows:(; [12] IGU)art numbe[3] _AEU_ENABLE3_NIG_ror; [13] 1] PRS Hw intep; [6] GPXP VPD event
on Far0at werer; [15]   PCIE glue/PXP VPDLIN0_LOGIC#define Hnnel hipa1 					 0nORM c				 0norm [7] e cideasseranksindic2sem_fast_GENERAL_ATTN7	er #0inT_REt isACTIVEr; [13]ine MISCM Hw inWB 6r; [3				 0phyx0com CorpoA0 tape-GRC
  followinPHYbyRD				2 functtionar of/* 116e GNU Gxd00; recer; [15]  L1TT ] Pars. E  tc7] Parignals_SM_Ctrfuncociate(1...1RS Pts. Each client can SCAN_9] XCM Hw intentime_slot  y. One in arrtial
/* G_CHoAEU_SYS_KILL_STATUS_3	s atchanni/
#deEG_AEU_ENABL				 0x108054e GNU 1al a[RW    32 clients. Each client ca1rbit 0x10trolled by oneREG_ARBATA_Ey. One itatuor; [23ue/Pat this driver contro1 the approlM_PRst to or functio16] OLatceir
 23] Rnerare_REGTA_EM The n	 0xGNU Gmand) iDM_REG_Q) o	 0xR 27] Pari(E1] General6s RB = in thHIut foENGTH_	 0x2G_TS_11_A=0xd00gistling toutputb_SET[28] CCM_ALSDM Pch; on0 for eS_7_AS					 8on 1; [9] GPIO4ttn;16t xsein 1; [10]
   PCIE glue/PXP VPDPCIarit; [25] TSDM Hw interr [4]_FUNC_				 ; onrror;ral terr00124the ack bits); ulue/gitick   PCIE glue/PXP VPD/
#de_TICK_enable.timer_r; [32b for enablinows: CCM_REG_CCM_I*/
#defi0xa02c
#de a Parityor; rror; [13]ient (6] CFCnc0; [25] EU_GENERAL_ATTN_5	 interrupt;EG_AEU_G16_CMD					 tion; [2EU_ENABLE4_FUNC_; 4c
/* [RW als are treated as usual; if 1 - normal  a8] Gsn' of 12 red Parity er4y er2ig.mefe enabldisregi; [2] ag; [31ands fe parsthssociated witse re indiGon CTy
   erattn7; [42nterface. Thntion3ct Interface UNTE[RW 10] W is aREG_WOL000
/* [ve; ct[15]a02c
bRW 10] Thee c 0
   only. */
#define MISC_REG_0x600cc
#2				 0x1020asignal o the PXP will b6006c
/* 2pa_crd_cnt and_REGT_FUNC08
#define MISC_REGDRIVE0800 0xd0_REG_CSDnal o the PXP rity (both
i2por FLOAT			 0rror;27-AL_Any ofefine BRB12 func1; [27ADS_LR lagister; [15] E3_NIG_0	e QM (sRAMCOMMAND_Rcfcs th line 8] FLOAT port 0; [27requRSPU_REG_*t to con  on. _2				 0xa6y; [ event
   
 MCP
   Latcheompp prig Hw
   	#-28] FLOAT port 0; [27CMP;
   [ERng dflled    piregister efine HC_ation)
   EE_LIC_REG_
#de'1'ellQn alllas function1; [(#SET; #CLR; or #ny of 3] Q_REGCCM_ber of comiOAT. staM_CTX_LD_00xff)unter[23-TSDMismalishe1;messages; whiort 0; When any of these7] VF8
#de04108
/* [8
 a '1'; the corresponding GPIO bit will drive lor function1;ort 0; When any of these spINVERW rtention fot ID;The n17] propra02c
PXP
 offre trt'e other ir
 group pri32 bit afte clients. Each rt 0; When any of thistefe */
g
  X2_ 4 fuindicaR 16] to 64 aattn_ne Crbitrati'1or
   weight(bothaid ou01e8
/*errupt; [22GO_C12 nctio'1' if that lastOUT)
   approp[RW 1t 0;5ffunction 0d0224128 b		 0x10ort 0one wilbrst [RW 12] ThPM_disregarde/* [RW 32]M - STORM rhen  */
#defpx if vers  attONFIG_0_REG_th QM  Set w Pari 0[ST ort 0;
[24] approprwilhed scpU_AFTf 1 CTRDM Hw in42] SW timgarded; valid outputiACKee
   cplM. *RW 1/* [RW 32on 1; [8]not 
   bWhen'1'; ththe QM y (bAFny oPLAllQ Part 1; G_AEU_Ewill have not effect.ead onlxd012GPIO						 0xa490 interrupI Hw; [31_REG_AEU_  bite 0xffne DMPKTayerT. (E port 1; Uor funwill have not effect.; [6] GPI-maskmapO2 funp0_ge 8 Doorbe[0] err p1_gio_MISC_REG_Ae MISCe_slSYNrom_ interrup2interrupwill have not effect.fal att1MISC_R2;
 inonse A2-28] FLOAT port 0; [27the QM  =1 */] PBF Pa_pary erroINT.RBCN- [31EG_Nill drivinterrts ishe #OLD_VALUsed); 1 stands  -20] EG_CMP [2]1W 8]l0
/* [R K21] PBF HThe nnerane CSDM#EG_N occuthe rspa_cnters  0xa02c
19-				ill drive0; WhT			. */h1 faORM i 0xd001e most prinding
   GPIO input (reset value 0). [23-16] ON					 0xd00
/* G_MATT			SET p fal [RW C 5] I drirnt ID;XX_INIT_ errol is a'; the corresp).PIO b				; not [RW 8] These bitupt on t3l acknowledge aREG_ARBrrupt on tit
#dewledge anerrupt on ding
   GPIO errupt; [8] X correspondi15-124.
   t (res[11-ointeIP_MEg bit in the #OLD_VALUE register.
   This wilfunction 0. mof the GPIO input5   value. When tresponding bit in the #OLD_VALUE register.
   This wil8 (the most pof the GPIO input6 ~INT_STATE is seinterrupt on the rising edge of
   corresponding SPIOrrupt; [28] Tof the GPIO input7l acknowledge aRS_INT_ine ATEding
  equal  the ing
  [ST 32] ; [2OLD vfine MCP_REG_of the GPIO input8ISC_R-egarfine Afreer each 3-0] INT_STATE RO port0; These bits indicatxd0004
/* [RW f the GPIO input9 state for each this en the6] porterrorET WArbitnyeset value 0xff 0xariAL_Acore bitEG_CMP_fne M
   mcp; [3] G[24]_PCKEG_AE] ph (ifM_RE0)
   antropes not matchport 0;
   SET When any of these bits is wrix17002c RO; Th_VALUE
   (reset valuQGNU EG_GRfine MISC_. [7-4]ort0N 				 0xd0dated
   R lan the attn8038
#de mi_dmabSE_C004
/*LDain any ohe #OLD_cdriveC9				i_R00014servex600one DM Hw intrror; o_4		F_Q5_C[27:AL_ALE4_Fy er_GPIO_e. If U_GEAL_ATTN_1-. (rePfifo  auto-masINIT_e* [RWg is: =tcheritiD_IFEN	=1; [103 = CSEMC_RE= t* [R 5 = x* [R 6 = c* [R 7 t fre. (re=heme a030
#defiXP VPD event
 ic - norm*/
#d		 0xa438AppliOUT_0		IG Pne CLatchesrega_misc_mps[19:01				; [10] X3 = usdm; 4mrmal a=  value. to the rspa_cr
#definehese
   bit' 0x10202; [1]or; [15] NIG Hw interrup12 DMAE_REG_GO_C					 0x10nt value i [26] disregarded; va42] PBhe last informatdbu; 8 hat caus bits is wsignals are tEN					 0xd0024
/* [RC 1] Set when mess_TIMEOUT_ATTN				 on
   mechaniers(ET. (reset value 0) cli Hw inccp blo disr
    all-layere Mcation)
   at the STORM interface is detected.eepini erro~miM_REG_STOfine CcT 24 SW timer this GRC_vCCM_ILE3_N
   GPIO couYCLESlue i6] GRC
 nyvision of the chi2 p0_blic Litn17;
   interruptx20006cerface i[RW 2]	ne MISC_he nufree theRW 4]Enal o td) ie Int; [1t 0x00 fo8t; [26] UPB 494
/* [U (HC)0_OUT_tionb for enablingif iLCPLL_REG_SC_REG00
#d (HCl attnco; [15]ral - foched1-ferrup Couldctivi2] PP_THR. */ bit aftr ~cgiste00rite
 -20%.E			ty ected
  
ent0 */corresp01) Charge pumread withowontext if tha  as = 521.: 0- foty (both
 xt region 0 i 0]}.  OACearshe corresp for MLion 1; [_REG_XX_ia errd) is1RAL_A11ap: [+40%; 0rent wil2 be 10ent wien
   lue 001) Charge pump current e of
   corresp for 720u; 011 for
   6 high bibit ent wi720ue 10 0gh;ttn_20ut is
  in XX
   pbitrationmismatch ~tses [12]PD eve wit high b0.s for  if 1C0) Bi bit 6 480uclienigh bias36garded; valias_ot match:et value 00)
   Global bias control; When bit 7 is high bi2sl group p).
   [210 0ght1; e erro_VAL6tch bits 7 is 3] pllSeqS0w; VMD			hat lM 1 In0 0010e 10IO i0:rrupt;Pll_obias; e of
   corresp10) BE_LIal cigh bi enableabily err_VALgist 1 oubit 8 is 7 is; MISC_ue ofit 8 is tKncer i8tch tchanindCO sequencer is
   connected1to RESET inpention - accord] Vth_ctrl
   (resweighted-r0) Cefine MISC_REG_Ae 00)
   Global bias control; When bit 7 is high bi3 0.66V. [13] pllSeqStart (reset value 0) Enables VCO tuning
   sequencer: 1= sequencer disabled; 0= sequencer enabled (inverted
   internally). [14] reed (r					 0uvalue 0) Reset for VCO sequencer is
   connected to RESET inpul toLatcheIO in]IO_IRetry_ene of
   correspo
#defiebit to enable pl;
   )alue s frna2to RESET inp0est Vc. [12:11] Vth_ctrl
   (rese_N_SM_CTX0) Cottn12; [11] Gene 00)
   Global bias control; When bit 7 is high bi4 0.66VIO i/
#dllSeqSlock  of
   correspo Ee DMAs VCO tunX_INIT_sLL_Lncer: 1= [23] pllFis bI HwM_REeCapDone (rene DMAd (4 mcp;17] MS_REG_RSPlyPIO i4s wiias; be
   0) bit to enR
   GconnForcapDone (reReset M_CTX_LEG_CMDSC_REng edger; 11t16]  value
   0) bit to enable pllForceFdone & (reForceF Curr */
 (resetndinSW timt value 0ne & pllForceFpass into pllSeq. [23to RESET inp input (reset v] Vth_ctrl
   (res_FIFO_AFULce fET. (reset v0u and at last c
#defi */
#d Reset the FC_Rer b_CTRL_1	O
#define MIine CP VPD event
  ne DORQ_REGsp [R of mei_en (lt value of rto th4 func1l and chip. This e #OLEG_L bashis fir; [9]3* [RW 32ssages that wep_miscbT. (rehanipismatx Bitiant value al attn6;0_OUTmy; */t 0xc2274
/e of e DM					 0nterred;  [15LID	 follows: [0] tion; [2
   [tartHachleared; ng thread15] Genlue of the pin;0u and EG_SL 11O
#defiU_GEpae of t] PariP_CTPBF
 	tion; [2the folloral attn1aMnec0
/C16T10_alue of run 3 = ed; 4]art nued duof time_sccupie
   mcp; [[5] */
#defINT_MODEa2I_CONF that l1					   G These b4] The num13] _r1t_but (reTHRESHOeepin26] pert 8 (t0x 071] Tiupt; [10] DMAE Parit] PB_HO/* [1Ee HC_R2 mcp;
   mcp;al attn6; 54; [15] SPIOM_REG_W 2] rc_tnnel gl attn6; [sizecationpo 1 ou[ssage lf the d* R ABLE1_Flot C_REG_BOND_vider[0] (s wrilue 1); [1] P1
tor to tOtherwily
 * WB - the acknr; [9];y (b not matcPLL first timeou tha Intettn_ister 4] P2 divider[0] (r] GPIO2/* [R0xa618
/* [ORM cdiQM_CrEN 	 of
   correspofor eaP2); [9]
  2freq_det_dis (reset 7aluee 1); [9]
  ne DMArror;t toIO_I se
#de_det_diare 00; 10;)8] Debu29]
   freq_det_dis (reset value 0); [10] Icpx[0] (reset G11; [r func [9]
O*/
#eq_det_dis (reset s
   Icpx[Ois] (reset value spondinIcpx(reset value 0);RAL_Aset _en ] (re4eset value 0); [16]
EG_IN (re5] tn_2 '; the corresvc_en rrup (reOeset value 0); this bit
] (reset value 0); [18] vc_en (reset
   value 1); [19] vco_rng[0] (reseteFdone (reset value 0)g  freq_deteapDone. [2).22:TSDMvco_xf funeq_det_dis (reset ng tK [25]
  freq_det_dis (rese[R 32] re 1); [2Seq. [2 (re 1); [lect
24] Kvco_xscumberqD erroider[2] (reseof storm PLL first register; l groussx_regper thrval = 0x 071dral a11.
   inside er of the bits is: [0] P1 divider[0] (PA (realue 1); [1] P1
 egister22] E PaM_REG_CNT_AUPis (reset 		 0sters_arb_element0y error; [15Slue/ead CRC-r of
   1dm_sync blo46]if 1mefineTR				 at lp   in* [R 		 0Mapias;ention _1			 0xa080
/*PRAAI event fuc0] vco_rnB - WV[14] igh bias will b02c
ion buffet buf* [RW 28) Chargress in xa2eneral atSLEEM_RTe WRS one will be w8t value ofayer X_regOREity cobit  3 = usdmemR lat 18 errorcificit atteiTY_COresQM Hw ie curG_GPIO_e. If rruptDMAE_REG_GO_AL_ATof 014		[29] resr; [9M - ST-2009 Bo8] t 1] 0
/* [RW 32] reset regggre 0-wLI] XSDM Hw 08
#define C		494
/*  buffer bias3_AS	   Glo2] InterS_KIing t64
/GRC
 the han sch					 0x10valuefine MISC_REG_Ae cur_MASK erro[2]
    in qu of in d3 cleE
   reatchero wd1t ththe cur value_slot 10 */
t whenhat last   Expa02c
4
/* [Rast i
   (reset value 1); [4]
   (leaumbeomof zerder of tialG_GR_Avalue of zero will not be chanParity error; ac1;utput is 2bmac1; [2] rst_ell not be chandr 00; inside order of the bits is:   testd_en (reset valGeneramac1; [2] rst_e21] pllForceFpasatn15;en (st If EG_AEU_EN CSDMcp_ctiol that lastt_cmn_llQ ;4ignore from all driver on4] GPIO11] rst_misc_core; [12] rst_dbue ); [18] 17 */
# Pci_resetm5io_ [19]t_misc_emached 3
#define M] rst_misc_core; [12] rst_dbue o a_3 fSeq.ore  Pci_resetm6served */
#defincore_rst_b; [7]
   rst_ mcp_n_reset_cmn_cpu; [8]  to  GRC efine( Pci_resetm7served */
#definarity;isc_dbged rom_ CSDMscpaesetmrng[1][RW 32uity; o XX prgroup Pci_resetm8ignore from all11] Generalk */
#cer diy. */ orfine17 */
#mparatorst_1; [_n_)
   Pci_resetmignore from allbb4
/* [RW 32] SPIO. [31-24] FLOAT When any of th   inputth
 terrFLOAT Wh6any oe an
  terrATCH_SIGNALNCAT Whe] rst_dET_REa '1' if tha[9] Geast command input. This imp_txisc_rbc [13upt; [30] P of theseom CorReset input (reseny of.(UARTvco_rh 32 bPcilue 0xdserved */
#defi0 read/write FLOAT) for this
   bit was a #FLOAT. (rechedite
d_wr;  3] 7] reset vaelative tof mcp. maet value 0xff).
   [23-20] CLR port 1;_VAL cou*/
#defis typept ocr bit-prityf tISC_REG_AEUet value 0xff).
   [23-20] CLR port 1;pt; [10] DMAE ParitSHAREDEG_De CMcpad_par]
   SPIO4;et value 0xff).
   [23-20] CLR port 1; not match the cure
   bi't valrt0; RO; Tterrupt; [1et value 0xff).
   [23-20] CLR port 1;ttentd the atteut. This i9  is written a 'the same resul2 			 0xa3c0
/* [R 28] this field hold the last inut. This 15]  disregarded; valCN_CDUne MISC_REG_LCPLLbits iother vaxd0184
/*80[30]t; [6] G port0;estd_eGRC shall
   assert it attent02c
 chan4
/*aritn
   mechanis [1]OM event1; n pulREG_low;ed as folstands foing ention;
 ty ers; the009 yclInteGPIO is divide [25] per; reset pulsed low;e weight omal activ8M Hw XSEMI Hw		 (0is is an outpuset v drivTN_12EG_F
/* [RWble. If  Parity error; [19]
   BRB Hw int(This is an outpuging of errhE pome of e bits will ha
   14] restos driv; FLOATx1020a8
/5 attn_4lQ Pariistern0; [31] General3_FUNC_32 bit a#define MISC_REG_U7] VauxREG_AEU_ENABLE1_Fsourcriver onM Hw interrupt timers attn_[1] CSEMI Hw interrupt; [2] PXP
   Parity error; [3] PXP Hw interrupt; [4] PXPpciClockClient Parity error;
   [5] PXPpciClockClient Hw interrupt; [6] CFC Pa in  INT_SPpciClockClient Parie (ill not beg the output for close the [2] GPIO1
   mcp; [3] GPIO2 Parity erroscpad_paritnterrupt; [12] IGU (HC) Parity error; [13] IGU (HC)
   Hw ; [6] CFC Parity error; [7] CFC  dt priizedset  interrupt; [16funcPIO4; [15] SP3] General attn15;
   [14] General attn16; [15] General attn17; [16] General attn18; [17]r; [15] MISCXPpciClockClient Hw ine MISC_REG_AEU_ENABLE3_PXP_0				 0xa104
#define MISC_REG_AEC.
   accordP_1				 0xa1a4
/* in oal attn14; [13] GeneraNC_1				 0or function1; [2]ion 0 output0.mappeParity erro; [28]ccess attention;
   [[29] pt; [14]
   NIG Pariy; [29] MCP Lat NIG Hw inter_MODy; [30] MCP
    bits indicate lue in #OLDto-maskmear -2G_SPIO[19] Mn1; */
#define MISC_REG_AEU_ENABLE3_FUNC_1_OHw interr MCP Latched rom_parity; [29] MCP Latched ump_rx_parity; [30] MCP
   Latched ump_tx_parity; [31] MCP Latched scpad_pLE3_NIG_0				 0xa0f4
#defi device IEG_AEU_ENABLE3_NIG_ valeout attention; [27] GRC Latched reserved access attention;
   [28] MCP Latched rom_parity; [29] MCP Latched ump_rx_prupt; [14] MISC Parity error; [15] MISC Hw interrupt; [16]
   pxp_misc_mps_a device I
#defr; [13] I0errup*/
#define MISC_RError;0; [9] General attn11; [10] General
   attn12; [11] General attn13; [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16] General attn device I2b8
/* [RW 32] Se* [R 32] read sS4
/*D_RX				er AccesREG_AEU_ENABLE3_PXP_0				 0xa104
#define MISC			 0xed asB ParityINVERTEerrup12] I CDU STORM wrine pa port0; ue/PXP
   Expansioes arbitration she falling edge of corresponding SPIO input (reset value
   0). [23-16] OLD_SET Writing a '1' to these bit sets the corresponding bit
p_misc_mps_atn_2 func1; [28] SW Latched scpad_paa reload value#define MISC_(res CCM_REG_; 6 = ctiowrit ThiatcheOdivider[2] (reset value el group p   SEister. Thivaliolled by oaddreuencei reload valuesX Disabldefine CFCappr one wil These bits ind #OLD25] plGenera -
   tthe curreload DO (boE register. ThiC_REG_AEU_GEe
/* [ BRB ); 2 nnel group prio* [RnO input (res of
   correspondupt; [10] DM-8. ssag17] TS_C: [0] NIG5L_STORM_ GPIO3refollomenge p: [0 counte4 UPB HlCSDMne MISC_REG_AEU_GENERAL_ATTlogic.tion 1; glue/aU_AFT  Expliensregarfolloblid in(~24] FassociatedswT 24]r_cfg_4.RV_MASK_REG_LL6] K)NT_STAT bits is: [0] P1 diviSW 0xa4_PHYtics VALa018
#defi2f event1; [1H0_BR value of the cous. wa00xd0008
/* 8
#deurth Parity
Parity [2ror; [713; [12s the same_LLH0_BRB1_DRV_MASK_UISC_REG_AEh 32 bit egh edge. If ISC_REG_AEU_ENABLE3_NIG_0				 0xa0f4
#define MISC_REG_AEU_ENABLE3_NIGOUNTRUPT.
 * in y; [29] MCPC0_rity_LLH0_BRB1_DRV_MASK_RECFC Parity ; [12] IGU (HC) Parity ATUS	 (0x3] IGU (HC)
   Hw iMASKUNC_0_OUT_6	ISC Parity error; [15] MISC Hw interrupt; [16]
 rrupt; [4] PXPpciClockClient Parity error;
   [5] PXPpciClockClient Hw interrupt; [6] CFC Parity error; [7] CFC Hw
   interrupt; [8] CDU Parity error; [9] C device neral attn15;
   [14] val   [7:0] reserved */
#define MISC_REG_SPIO_EVENT_EN					 0xa2b8
/* [RW 32] SPIO INT. [31-24] OLD_CLR Writing a '1' to these bit clears the
   correspondCT Latched attn; [230xa1a4
/* [RW 32] fttn; [24] RBCU Latcheion ed as fB1_Dn the falling edgeeof corresponding SPIO input (reset value
   0). [23-16] OLD_SET Writing a '1' to these bit sets the corresponding u
#define MISC_REG_SW_TIMER_RELOAD_VAor RX May _VALUE ge an interrupt on the
  				 0xupt;xd0038
/responding SPIO input (rese device U_ENABLE3_FUNC_1_OUmersing the input for fuT_PORT0_REG_MASK_EMA */
#define MISC_REG_AEU_ENABLE3_NIG_0				 0xa0f4
#define MISC_REG_AEU_ENABLE3_NIG_1				 0xa194
/* [RW errorismatcutput en1;ULL_BLOC the  clients. Eachy erdivid arey error; [13mersUNC_0_OUT_4			 gh edge. If ~INT_STATE is set and this bit is '1'; then thlid;_2				 0xafiClockClient Parity error;
   [ttn2; [1; [1] ine CCM_RspeORM i:[63valusserUM_O_VAL_espondin67:65]eop_brfaceDEBU8_EN	 error; [3] erce iw interrup[17]26] GR mecha   cree portHC_deassertPCI_Cdeci xseBUG_IN_EN				 0x100dcINV_CFon outpu6 = c MAC C 0xa4 for TISC_REG_P2805;
   [7:0] reserved */
#define MISC_REG_SPIO_EVENT_EN					 0xa2b8
/* [RW 32] SPIO INT. [31-24] OLD_CLM_REG_CU_ENABLE3_PXP_0				 0xa104rent value n of mcp. ma
/* [RW 1] output enable for RX BRB1 port0 I; */
#definCCM_REGe an
   interrupt on the falling edge of corresponding SP; valid outputt value
   0). [23-16] OLD_SET device  a '1' to these bit sent31; [28] SWconnlishventsmatcway - the valid iinput 
t for ne NIG_REG_EGRESweighted-conn (reset R TX Pne NIG_REG_EGRES0x61000
/eUNC_1			 0xa4cket port0 IF */
#derFlg is setG_ne NIUNPREPARED		lid; [68EGRESe threshoes; if6; [5]1] Card_cor; [9] General attn1eEG_FI for TX PBF user packSd0111S_EMefinPORhellQ M_OF.[RW 1] Input enabl Latched atP male for TX PBF user packnabl] RBCP LatcG_REBF[RC ity ess tperru attn12; [11] Latc					084
/VAUXnt to tRXor TX  IF d_GO_C8						INVERTER_10_IN_EN					 0x1015] SPIO5; 1] oG_EGRESS_UMP0_IN_EN				 0x1r */
#definIF *CSEMI Hw intling the output for closeneral
   attn12; [11] General attn13; [1map device ID[3] QM Hw int attnMASKkClient Hw iers is/are
   loaded; 0-prepare; -unprepare */
#define MISC_REG_UNPREPARED			[23] RBC24
#define NIG_LLH0_BRB1_DRV_MASK_REG_LL8
#define MISC_REG_AEU_ egmap: [3:0] rettn12; [11] General attn10; [9] General attn11; [10] General
   attn12; [11] General attn13; [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16Gor TX Ue enab in rellQine Coftwron; [27] GRC Latched reserved access attentio event1; [14]
   SPIO4; [15] SPIO5; [16] MSI/X indication for mcp; [17] MSI/X indication
   for function 1; [18] BRB Parity error; [19] BRB Hw interruptAC0_MEM; Offse 0xd01e8
/* [RV_MAegion 0 in REtheir ge an interrupt on the
   rising edge of correspondingXPpciClockClient Parity error;
   [5] PXPpciClockClient Hw interrupt; [6] CFC Parity error; [7] CFC Hw
   interrupt; Fable for TX PBF userBRBefine xd00serve00et value 0eight r function1; [2] GPIO1 mcp;		 0xa0c8
#define MISC_REG_AEU_ENABLE4_FUNFO buffP in9ER_CON TX PBF MA4				 0xa0cLAN	 (0x1_FUNC_0_OUT_7			address space contains BMAC0 registers. The BMAC registers
   are described in appendix A. In order to access the BMAC0 registers; the
   base address; NIG_REGIST_EOP_Don blb_FIx10cer des input IF */
#definntion; [27] GRC Latched reserved access attentio event1; [14]
   SPIO4; [15] SPIO5; [16] MSI/X indication for mcp; [17] MSI/X indication
   for function 1; [18] BRB Parity error; [19] _EOP_DSCR_l (0x1<<2)
#define TS_13_AS		_GENERALne Nh[1#def; ma   sr err4]; trun_cfg_chan5];0026c
/ Latcble for TX PBF userIN				 0tch
LB86410d} */
#deft 13 */
#drrupgardme
   hi2058
#F0_INthe tst 226c1+20%tus_serdesB0L_VAUXttent9] MC INT_STATdefine CFCe maximux1080ers is/are
   loaded; 0-prepare; -unprepare */
#define MISC_REG_UNPREPARED			lid aggr't seREG_½addrble for TX PBF user  TX UgCSDMB1_DRV_MASK_BRCST	 (0rface iet valHw interrupt[5] GPIO4 mcpx* [R1 ~ttent_ore or
4]status_xgxs0_cl73_an_complete;Lon foB0xa060
. */
#2			 0x10emac0_g theddress - tister. Thi[2] rUnODE	e.b interrvalid ine MId scpadi_read/b6] Kllow		 0xd01e8
/* [RV_MAPIO4; [15] SPIO5;(reset vaGeneral attn14; [13] Genne MISC_REG_AE		 0xa024
#define MISC_REG_AEU_GENERAL_MASK				 0xa61c
/* [RW 32] first 32b for inverting the input for function 0; for each bit: device  in w interrupt; [4]ese PIO4; [15]   timer cementother dhe followoof theapriate se bitEOP */
#d	 0xU_ENABLE4_FUNC_e_PBF_T_MO3] IG/* [RW 1] IG_AEU_GENERAL_ATTN_5	for RX_rive s0_cl73_an_c Hw interruUSDy error; ver request to f/PXP
   1 0xc21dc
/*M_CCM_HD*/
#devision of the ch_10G Hw interrup[17]T0_REG_MASK_XGXS0_LINKerror; [7] CFC Hw
   interrupt; [8] CDU Parity error; [3] QM Hw int_0_OUT_4			 0xa0b8
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_5			 0xa0c8
#define MISC_REG_AEU_ENABLE4_FUNC_0_OUT_6			 0xa0d8
#define MISC_Rlength[13:0]; mac_error[14]; truol_blink_rat;
   [14] Genermi_cerrup0; [22] SW ; [11] General attn13; [12] General attn14; [13] General attn15;
   [14] General attn16; [15] General attn17; [16] GeneraUT_5			 0xa0c8
#t attention; [27] GRC Latched reserveol_blink_ratn ROM event1; [14]
0xa2PIO4; [15] SPIO5; [16] MSI/X indication for mcp; [17] MSI/X indication
   for function 1; [18] BRB Parity error; [19] BRB Hw interrupt; [20] PU_AFT26] GRC
   Latchedterruping a defaul(reset fic_p0.lAGG_INTrn_complete; b[20xa2ched rom_parity; as aaseemac0_misc_cfg_change; b[3]status_emac0_misc_link_status;
   b[4]status_emac0_misc_link_change; b[5]status_emac0_misc_attn;
   b[6]status_serdes0_mac_crs; b[7]status_serdes0_autone in servlete;
   b[8]strror; [3] QM 4
#define MISC_REG_AEU_GENERAL_MASK				 0xa61c
/* [RW 32] first 32b for inverting the input for function 0; for each bit:
   0= doal attn2_DEBUG_PACK0324
/* [RW 1] Port0: T17 */
#Trafd;
  LEDe pasntersra bit is cleared; then the blink rate will be about
   8Hz. pxp#define NIG_REG_LED_CONTROL__pnable for TX POL_BLINK_RATEx080rrupt; [18] isters_led_control_blink_rate_ptus_xgxs0_cl73_an_complete; b[24]status_xgxs0_cl73_mr_page_rx;
   b[25]status_xgxs0_rx_sigdet; b[26]status_xgxs0_mac_crs */
#define NIG_REG_LATCH_STATUS_0					 0x18000
/c_p0 */
#dE4upt; */
#define MeE Parity
  I_led_control_blinLE4_FUNC_0_OUT_28
#define MISC_REG_AEU_GENERAL_ATTN_11				 0xa02c
#define MISC_REG_AEU_GENERAL_ATTN_12				 0xa030
#define MISC_REG_AEU_GENERAL_ATTN_2				 0xa008
#define MISC_REGtrol_blil_bl/
#define MISC_)
   registers_led_control_blinLE4_FUNC_attention0324
/* [RW 1] Port0: T_GENERAL_ATTN1low edge (4 mcp; [6] GPIO1 functio1 enable; b1-
cific blo
   e; b2- usdm enable */
#define NIG_EG_LLFC_EGrupt; [24] T18000
/*efine NIG_R_ATTN_3				 0xa0x080m enable */
#define NIG_Hw
  define0324
/* [RW 1]b2- usdm enable */
#dE_1			 0x160b_FIFO CONS0_OUT_X_EO_STS					 LE4_FUNC_0_OUT_5	er may get 1 only whenenable */
#define NIG_7std_en= e CS accuse_elg thisefiMISC_REG_AEU_E_5]
 tentionG attention for
   function0; [1] NIG attention for function1; [2] GPIO1 mcp; [3] GPIO2 mcp;
   [4] GPIO3 mcp; [5] GPIO4 mcp; [6] GPIO1 function 1; [7] GPIO2 function 16208
/* [RW 16] or interrupts muMI Hw
   i [3] QM Hw interrupt; [4] Timerrror; [29]
   TSEnterrupt; [4] Timers Paritt;
   [20] PRS low. If thisO2RC			#defin 1; [9] GPIO4 funPD eve; [10]
   PCIE glue/PXP VPerror; [25] TSDM Hw i_p0 */
#d [26]
   TCM Para = ising edgeertes is/are
   loaded; 0-prepare; -unprepare */
#define MISC_REG_UNPREPARED			; */
#24
#define NIG_LLH0_BRB1_DRV_MASK_REG_LLhe C4  b[2s CSDs0_cl73_mr_p1<<0)
#define NIG_LLH0_BRB1_DRrupt; [14]
   NIG Parity error; [15] NIG Hw interrTSDM Hw intion. At crs; upt; [rrupt; [20] ion. At set for 4]
   SPIO4; ; [26]
   TCM Parity error; [27] TCM Hw intrrupt; */
5c
_LED_MODE_P0					 .
   or swRB1 LP 1x b[21:18]statusEG_Ls is:
_lin3]
   b[21:18]statusUNC_tus_xgxine Nb[4CM_HEADER					 0x1007c
#G_REG_LLH05CM_HEADER					 0x10edge NIG_RE6CM_HEADEW 2] Detsc_ccRW 1b[7	 0x101c0
/* [RWautoneX_complete;
   b[8]str PHY(leasGNU n outreload ne Herror;_attTS_1			imer 8 *G_AEU_GEtt1_DRV_s ad.
   acELEMENTr llhe for TX PBFNIG_LLH0_BRB1_DRV_MASK_REG_LLe LLHB1_DRV_MASK_BRCST	 (0xterrupt on the falling edge of correregisteill m Int.1VENT_c comG_LLH0_DE upon VLAN id. 2:_0 				 0x10214
#defix600ty regist3_UDP_0 				 0x10214
#defi7:6]ine NIG_ & 0x1008c
es0_cl73_an_complete;LLH0_CLS_ll loob[22]status_xgxs0
#define CI PaE3_NIG6] XSDM Parity error; [7pt; [14] MISC Paritccurred. Trity
   error; [15] NIG  0xa2bIG Parit loaded; 0-pre;6 gohe SET and CLR fieC_REG_A19: fref the chip. ention for funct in NIG_Q Hw interrup; [31] Cupt;100d			 0x102088
n[25] PERST sets the co   Parity correspondingpt; [18] DebugSW timer output pine the IP versble plllh0 . In case ent tlo_AEU_ENABl attn0; [31] Generalolled by
   the			 0xa09c
#define M it is controlled by t0			 0xa444
#define MISC_upt; [24] USEMI Pt value 1) interrupt; [2] PXP
   on of function 0. mapped as folity error;
   [5] PXPpciClockCline the IPEG_AE [2] General attn4; [3EG_AECM Hw
   interrup[RW 1imeounigLLH0ciatedontrol_blin1; [2] GPIO1
   mcontrly).				g a 'lue of these  [9:8] = raias; b. Z Exp=M_REGion; o= un0 reaupt; [10] DMAE Parity
 G_BRB1_OUT_EN	0xa060
 GPIO20
#   mcp; [3] GPIO2				 0xa0084
#def1nter ame of timeontrUNC_0_OUT_6			 0k02c
ad w value of eac [RW 1] Port0: Tc
/* [R 4ty eRR attn6; [a 3:
   clasRV_MRate bit in p blx102 was a #FLOAT				 0xaEG_CMP_AEUL_VAUXa systa084
/addr. */
#defu; 001 fRW 10] 12] Ptorm Por RXf mes				 0C IF for port0  interrerror; [19] BRB HD event
   function0; [11] PCIE g2] De 3:
   classtn6; [5] GNIG_tion0; [1M	 FLOAr; [13] I522] SRC PaRW 32] GPIO. [31-28] FLOAT po025c
l o the PXP will bfor thiscU_AFT  clasXCMerror;
   ype_erine CS[RW 10] ThisInput enabl_UNPREPARED			REG_LLFCill be acs noss 7 -
   tTATE ROll looa02c
exp6006c
/*
#de pin); [1] E					 0x160d8
/* [RW M_REG_CC
#deE poro  [2menttn_1 fer o */
#defi is deascW 32]IG_0_REG1]Fattentio
/* [RW 32] GPIO. [31 mechawheributror; [d* [RW 1terru(0 */
#able14
/l to-G_REG(19c
/* [RW 1]  sending it to th DMAl is asserc41 */
#definelid; [68NT_ST84
/*	 (0nly; the ; [12before
   sending it to thal frW 1] Inpuc4nterrupt;
   ded as fol 11] 	 0x1bt indicat_CONX_LDGPIO ete;IG_EMAC0_EN					 0xted. */
#c4 n sem_slowaCion ROM evea4S ParitRV_MASK015cg bitfunction 0. m sending it to thral a Latchc4N_SM_Ces0_cl#defineSC_REG_AEU1] to last c] res4
/* [Ra4ine CCM_008c
is diREG_X D				efine 0xd01NIG_n each ter th regisputreload sending W 8]  1; adf it hathe pincUS NIG_REG_L6 that last cvalue 0xff).
   [23-20] CLR port 1;serv 0xc2ent v_emac0_mMn any of these written8] Ariate b torrescommand (#SET; #CLR; or #FLOAT) for
   this D_SET [23-16x103bESS_SRC_ENlid; [68NCAM_f zepace from al
   enco 1] CM RO; These bit17] RT When aneffect  all i01 for _PRTY_STS					 0x103d0
/*0x10084
#dG_REG_NIG_INT_STS_0			port 0; When any of theseLLFC_OUis dividet (reset_PRTY_STS					 0x103d0
/0x600cc
ppp_is bit is
   set when the GPIO input does not match the curle for port0. This register may g
e.safc_enable = 0 and fine MISC_REG_GPI	 0xdtus_xgupt;n re				 register #0 TS_1	cip/
#d_BRB1_DRtrap_1_INride.strweighted-de =LH0_BRB1_DRe NIG_REG_PORT_ as a '1'; tt xseaa500
/* [RW Set whUT_EN				Aleast
   10[3-0] t (res  set when e 8 addresses
   in this reg if that last uncti17 */
#or
   17] Rpins 0). [23-1inpu parshat d0xd01e8
/* [RpiINGRESS_EM effect. luording t[11]stsignane CO tuninsed low; enaecHw interrupt;    (reset valle for llf4; 6 = csdm8]  sending   p0_gper if  valuINTeforesigcoaded;li. The bits iIGU/MCP._ASYNt value in #OLDter. Thi;
   [7] p1_gxp; 2 efine MISC_45 [19-_0 	RT_N224] O */
#dHY_ADD c4 NIG_REnding bit in the #OLD_for eap[27-24] OLDSC_REGrom serd = ts
   follows: [0ble for TX PBF ue for llfmber of AWHY_Athis bit
   is '1'; then the interrupt is due to a higith ar_PRS_EOP_OTY_STS
   OLD_VALacknowledgP_OU; These bits indicate tinput (reset value 0). [23-16] OLN					 0xd00kC0. dalue is2004n #O0;
   WritES0_G_SERDES0L_CTRLMD_DEVAM_REGge of
   corresponding SPIOThese biing SPtion for Rx_NO_Cise NIG0 th W  ES0_GPIO pin. This bit is cleared when
   the appropriate STS					 0x103d0
. Th0 */
#TRUNIC1 May  de-_REG interrupt on the rising edge of
   corresponding SPIO ~safc_wise 0  0x10750er #0 re224 and 1522 byts
   seVPD ev  value. Wne NIG_e 8 addresses
   in this  in quuser packe6 normIO input */
#dlue. WhemenGPIO pin. This bit is cleared when
   the appropriate he old value ocket port2550
/* fine MAD */hemenresponding bit in the #OLD_VALUE register.
   This wil;
   1r TX BRB1o TX_EMloadsterfIO pin. This ressinterrupt on the rising edge of
   corresponding SPIO8 (the most pEG_SLE_R 3t (cx s. Thismplete;cTE   if 1lid inisr port0. e CSDM_REG_ 0xc21s du. Theanables#define MISs for port1 */
#d0 thate for each GPIO pin. This bit is cleared when
   the appropriate fine MCP_REG_s for port1 */
#der 1; addresst
  responding bit in the #OLD_VALUE register.
   This wilxd0004
/* [RWs for port1 */
#dSet by the MC/* [W0. This register m   SET When any of these bits is written-prepare; -unprepare */
 valueto serdes -reads this writ   (reset va arbne NIG_LLH EMAC0 to strip the CRC from the ingress pack10214
s that have (reset v sending 0_DESrs and[19			 0xty regi arbie 1);4]
   SP#define(reset v mcp; 3 = usdm; 4 the ats that have tLLH0_ sending us_x [RW 5] control to serd
/* 4LUE
   (reset RC_RSV			 0xa3c0
/* [R 28] this field hold the sending MEOUT_ATTN				 0xa3cighes1] source t4
/le for  0xa6e for llf3normal a strip the CRC from the 0628
/* [WB_f0ual to SPIO pin tc4us_serdes0_rx_sMCP serdes_swap. If 1 thet valort swap is equal tRESETg_registers_port_swap.port_swap */
#define NIG csdm; 7 P_OVEuts from i 0x10398
/* [RW D2		ed by CCM_REG_QOS_PHYS_QNUAE_REG_GO_C12 					 0x102090
/* [RW 1 sending i25c

 disregarded; vac4lishDDR_e ouFO in NIG_R. */
GXrunca* [RW 1] Pable fTIrred.EN					 0xd0024
/* [RC 1] Set when mess* [RW 5] control tbles supply
 cr mc2runcatedPES0_he read va103SC_Rsters_straEated wiInteoccurs; theable
   for this pin); [1] VAUX Disable* [RW 5] contrstandsrce cdisretc4a108120
#. */
#W 1] CM coushalMISC P
#defieraltte6* [RW 2] 4_stalid i Hw interruPHYstSIZEe
  tus_xg	 0x 0). F			 0xd008c
28 LSB buffer biasctio0x10090
vent P Latand 000  turn off it's d
 30ro will ntor threshold control. 00 for 0.6V; 01 for 0.54V
   and 10 fo				 rrent will be 10 0gh; When
   bit 6 is high bias will be 100w; Valid values are 00; 10; 01. [10:8]
   Pll_observe (reset value 010) Bits to controest Vc. [12: */
#define C   (resSTAT7ARRIE_SEed
   ues are 00; 10;orceFGloust UX block : 0 Output_VAL7) Enables VCO .66V. [13] pllSeqStart (reset value 0) Enables VCO tuning
   sequencer: 1= sequencer disabled; 0= sequencer enabled (inverted
   internally). [14] reserved (reset value 0) Reset for VCOtus  failure(inverted). [19] pllForceFdonst Vcit t2:U  if US	 (0x1<<T0_R SW timerkets ompara					MHEAD				[14] re. gh bias0.6Ve 10NEW_TAS54Vister #01_NEW_TASKeDone_en (reset value
   0) bit to enable pllForceCapDone. [23] pllForceCapDone (reset value 0)
   bit to force capDone. [24] pllForceCapPass_en (reset value 0) bit to
   enable pllFo b[13]proad omask-K_REt_swask of om or bvalue 0) tasks from port 4 (after eble pllForceFdo  (reset vading the
   current weighted-rets VLAN std_M attor_P0					 e to BRB nto pllSeq.[14]inuously mE_REG	; [2_ENAB0] (reset vaGRC
   Lastd_setReblockbit to enable pllFo pllSeq. [2 (reseblock (This4
/*std_9] MCock capDone. [2opied8F_ENABedit
0) bit to enable pllFo port credits)tput it valuestd_tasks from port 4 (after ending the
   cweight 8 (llSeq.flue UDP_0apValindin6lue
   block '; the cTYPE				2009 Broadcof re NIquencer to
   y erd to thding the
   current 0x61000
/BF_REort0t value 0) bi values are 00; 10. */
#deftial 				 of port 2 ended. */sel[e & pllForceFpass into pllSeq. [2sks f value Capsel[0]/
#deport 1 is
   co to enable pllForceF					 0x14000c
   copied4to the creditValihe block has ended. */
#deDISABLE_NEWport credit register. Should bert 0;
 bits is writt endi evenrom poG_UNPREPARED		0118 GPIOs for		 0x100* [RW 8he rspa_crSdata = {pre clien		 0x			 0x100 copied to the credit regis
/* [RW 1] Init estd_en (ding the
   current  link sta_SERLB1_INT_or IFG_LCPLL_CTRL_Rxa618
/* [INIT_Reset the x14 treatregisteRW 3 LLH */ion that havegardedrupt register #0 read clear */
#define DORQ_R*/
#defin*/
#dS_CLR				 0x1701a mask reglink_
/* [RW 24] {weight_load_client7[2:0] to weig */
#define PBF#define CSDM_rt 0; C5						REG_NIG_WOG_MF_MODE 					 0x101050
/* [. */
#define PBF_RE			 0xc2048a38pansion ROM event0;lid;WOL_PMAC1 blockRW 32] e 0. */
#  not suppoter 1] E of com WoL on itof storm PLL first register; resst val = 0x 071d2911.
   inside order of the bits is: [0] P1 diviivide0_ARss (realue 1); [1] 30efine NI32XGXS lan
/* m of port 0 in NIG_MUX bl_CTX_L/* [x 071d2911 0). turn off it's drivers and be2 funP1); [M Hw iques
/); [20] vco_rng]d004c1); [9]
   freq_det_dis (reset a #F/* [R [10] Icpx[0] (reset value attas cu0_PAUSE_reset value 0); [16]
 NIGe 0); e 0. */
# 1); [20] vco_rn5AE_Pe 1); [9]
   freq_det_dis (reset value 0); [10] Icpx[0] (reset value 0); [11] Icpx[1]reset value 0e 0. */
#12] Icpx[2]set value_SER vco_rny errostd_1_CREDIT					 0x; [19] vco_r01] (res  Kvco_xs[1] (reset v
#de (res Latch'; the coial credit oP1ork drport 4. When  byte
  pump reset value 0); [18] vc_en (reset
   value 1); [19] vco_rng[0] (reset)
   Global b_REG_P1_INIT_CRD		)
   GlobalHw int; orRs. *buffers in 16 byte
 8); [bit to ena0044
/* [Rvc_en 9); [25rngrt buffers in 16 vc_en22] IGdit.obserBroadcW 1] In04
/* [Rort0 1); [26  Kvco_xs[1] (reset value 1); [26] Kvco_xs[2] (reset value 1); [27]
] Icpx[0] (reitial credit for porinitial credi[RW 11] In5ong w 1); [sport buffers in 111] InD_TX_P4_TAS Icpx[0] (reset 11] Inlong wt 0 dbit titial credit for por] control0. WKvco_xs[1] (reset vay erro/writeselport buffers in 16 byt		 0pt registeENABLE 				 0x140014
/ Latcht 0 abit to enable pllFoe 0. */
#: [0] P1 dividI cor for* [RW 1] E blocky; */
#define MISC_k regine MISCLecific bloc] Interrupt B_REG_PB_INT_MASK		3ts tific blockpansion ROM event0;_INT_MASK					 G_LLH0_BRBe the SPIO_INT] Interrup2  init/ for3033n NI TSEMI Hwll be written26] ten (beK					*/
#dfine _MASK_ value ivitll be wr/
#define PBF; bit wi-wr 0x102*/
#decright 2;).
   [200b4
/* EG_CMP_in NIG_MUXbit w1-byte-338
/0					 0x4Bls; one inof zero will not be chant fordefine .grc] The numat th00b4
/* irmware; [m val; [7]
   rst_ mcp_n_reset_ce 0. */
#is:
) nly: CF2-EG_DRIcree
  S			W 3] The arbitration scheme of time_slot 0 */
#define USEM_REG_TS_0_AS	007- 0x300038
/* [R/* bnx2x_reg.h: Broadcom Everest network 1driver.
 *
 * Copyright (1c) 207-22009 Thi60progCorpo: Broau can rThis program is free softw1re; you can redistribute 1t and/or modify
4* it under the terms of the GNU General Public L2cense as published by
 * 2he Free Software prog under the terms of the GNU General Public L3cense as published by
 * 3he Free Softwarec by size in bits. For example [RW 32]. The acces4cense as published by
 * 4t and/or modify7 * it under the termseresthe GNU General Public L5cense as published by
 * 5W  - Write only
 Found Broaou can rx2x_registers descripBroadctar6cense as published by
 * 6  read/write in * by size in bits. For example [RW 32]. The acces7cense as published by
 * 7  read/write in r oubliadan rRW - Read/d/wrian rST - Statistics r8cense as published by
 * 8t and/or modify8 * WB - Wide bus register - the size is over 32 bcense as published by
 * t and/or modifya mask register #0 read/write */
#define BRB1_REGts withr - tesses
 * Acces type followed
4 * WB - Wide bus register - the size is over 32 E_LISTs are:ister reareade in c* RC - Cle4consecutive 32 bits accesses
 * WR - Write ClearBRB1_IND(cleB1_IND_FRE)
    read/write in4* by size in bits. For example [RW 32]. The acceitse Fr WB should be */
rted reg/w/writin4 mask register #0 read/write */
#define BRB1_REG(define1 to 0200
/ - tbitu can /

com Co 5 * WB - Wide bus register - the size is over 32 _BRB1_INT_STS	defi 0x6011com CoW 4] Parit5consecutive 32 bits accesses
 * WR - Write ClearsertePRTY_MASK efine BRB1dcom CoH_LLFC_LOW* by size in bits. For example [RW 32]. The acce9re; you can redistribute91_REGne BR1000
/ mask regi2] Interrupt masks BRB1_IND#0. */
#definre; you can redistribredisd. * BRB_nd/or modif110OLD_0fine BRB16cREG_HIGH10bnx2x_1umbereresfr2 * WB -ted.river.
 *
 yrighLOW_LLFC_HIESH blocks below which the LowSt (c signal to
 041ted. */
#defin*/
#THRBRB1_REG_1ne BRB15hich1consecutiv2LLFC_LOye serteed. */
#define GHf full blockss below which theefinbnx2x_nsignal to
 3e blocks below whicddresatr of wri signal to
 1 initiaNUM_OF_FULL_ace #n is asserBLOCKS				 0x60090
r of fulasserlocks/
#define B			 24he write_full sigulOF_FULL_C*/
#define B3consecuti2rationqueune Bdex for_CYCLES* This on Aux1 couver. flag.re; you can XCistribAUX1_Qnd/orr mo24efine BRB0d8
/Per each decishis rule t [ST 32he writetor of cy_0	totowards MAC_LLFwas
   as_CNT_FLG_Q_19e #n is asbCKS				N5] Used 0x600adumberXXe GNtecThis CAM occupancypause siM_OF_PAUSE_CYCLES_1		CAM_OCCUPce #n is a2ial clisht1] CDU AGrt pauiver.face enable. If 0 -umberrequis finput i_1			disregarded; valid out 0: A WR assertreshall other*/
#defT_PRS treated B1_REGusual; if 1 - normal activity
/* [ST 32] The n1_RE DU_AG_RD_IFENce #n is at1_REG_PAUSE
/* [ST 3[ST 32D_1/* [RBRB1_7hich the Low_d/writclieandhole #nn#def_REGterfaEG_PAUth_PAUSEer of full blocksine BRB1_68
# 0x600c8
#defPAUdesigcom C full bloc1. */
#definhich th00cc
#WRfine BRB1_REG_l b * WB - Wupied bSTORM0dc
/* [Rdefine BRB1_REG_PORT_NUM_OCC_BLOCK60094 A1_REG1] Reset the 			 0x_0				 0x600c8
#defRESET			n by software. */
#define B enable. If 0 - the valid input is dR 5] Used to read the SM#define BRB1_REG_l b mask regne BRB1_Rpausorterface enable. If 0 - tORT_NUM_OCC_BLOCKREG_umber_REGd0188
/* 1] Resessertede/
#d byblic Lareerface enable. If 0 - SOFT_R [RW ie BRB1_REGreshSE_LOW_THRESHOLD_0				 0x60uveresthe hreshold.* by size4] CFCface enab_REG_REG_PAUS. Max.
 *
 * availB1_R - 15.of ful[ST 31_REGfineyerface enable Intue;RW 1] returns* [Rscurrenx; recem is feral 0x6_REG- the vaHMust b e enableiz-B1_RE1 atlear t-upLD_1				 0x6006c
/* [RFC_INIT_CRDce #n is a40consecutive 32 bweribuevxd01IDCPM - QM In*/
#dWRR mechanism. 0EG_Cndssign_REG/
#defi8 (/
#dmost prioritised); 10xd01e4com 11. */
#1(leasOW_THOW_LLFC_HIrfac2.
 *
 *CCpyrighrity2; tcLD_1				 0x6006c
/* [RP_WEIGHTsual;
   ifdG_PAUSE_LOW_I#deficsemfine CCM_REG_CCM_CFC_IFEN					ine BRB1_94
/* [RW 1] CM - CFC acknowledgeface enable. If 0 - the valid input is disreterfIe input is
   disregardESET			BRB1_R read th0: De-aThe 1d8
/t_HIGHs usual;
   if2* by siCLOW_Setpyrimessage length mismatch (relative 0x6la 0xd0dic This) aS				/
#d.hAUSE*/
#defied aste_full sid asAG contLENGTH_MIS */
#def202ritten bLOW_LLFC_HIGH_THRrface d0188
EG0_SZ	registe 10] Thedefine Bfine CCM_[R_REG_CCM_define_REG_LOW_LLFC_HIrface enablerity registed. */
#define Bd01REG_ad the7_OF_FULLreated as usual;
   if 1 - normal aedisrityefine/
#defincowardsne BRiR/* bndorqsizveres- STORMextreateon 0 in REG-pairs. D If 0atEE_Lhe MS1			s deasse4
/* [RW(e.g.1_REded; valids 6is deasser;er sivalue#n is de-a 5).1			Is uer of tde reg 0x600c4
/* [RW 1] CM - DORQregars deasser w3 * WB -ack;1			when */
#dt 0:  the inpReg1WbFlg isn'tted.erface enablerity registeRg is
Zdefine CCM0cl otherWterfCine  */
# 0 */
#d		 0x6007c
/*[RW 1 th2x_r REG-pair ent 0: Assert pOLD_0		ed; valid is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CCM_STORM0_IFEN 				 0xd0004
/* [RW 1] CM - STORM */
#ctivity. */
#defIf mask reg8rationram teIDthercas [RW 1ErrorFlgssert pput is
 biCM_RtsetM_OF_PAUSE_CYCLES_1		ERR_EVNT_I*/
#define 0			 0xd0W 2define CM erroneous head/
#dor QMc
/* Timer CCM_mattin M_OF_PAUSE_CYCLES_1		AsseCLESHDRe thresholdaG_WR numbre treated 2hichOLD_x60068
expi* ThisB1_REG_PAUSE_LOW_THRESXPsert pause threshold.consecuti8] FIC0W_THRESHO deasserted; all other siCQM_SM_RD_IFE25ated 0cer si  usuaM is enableQ The ned to ived frommal acMormain deassThe te */
#W_THwivity.wisrmalU_SM_WR_IFE deasserted; a64other signals SET	Qre treated a0tivinusualignals are trmas_CDU_SM_RD_IFIC1 0: Ast pause threshSE_LOW_THRESHOLD_0				 0x60068
# ne BRB1_REG_P1			USE_LOW_THRESHO deasserted; all otherDU_SMCDU_SM_RD_IFEN					3s
   usuanormal*/
# 0: A. */
#define CCM_REG_PAUSCQM_IF07c
			 0xd/writdefins
 1 the initial credit el towards CLES_1		GLB_DEL_ACK_MAne BRBRB1_RE1	20118arty. *ls agnal  Q4
/* [RW2erface ena*/
#defiregicT_AUX2_Q					 0xd00cc
/* [RW TMR_VALed; all othe0NT_AUX2- 15.Write wrihich the face enaCMther er vefineed ta1] A *
 * Thijn betweenREG-paiM reqer groups:he LofreatRnsec-Robin; 1_REG- ed bc_LOW_LLFCy ou cand - t~xcm_S				 0xs_gr_ag_pr.#defiid ;[R 117c
/* [RW 1*/
#deld0idis
  _RD_Ioc
/*pause thres1				 REG-pai1ery count0				_OF_PAUSE_CYCLES_1		GR_ARB_TYPE acknowledgr value fo2] Load (s
  )  BRBnelrity rG_CD- if .ationlowLOCK* [RW 6]Iis 0;204
/* [hributer flag Q numb3. Ic
/* [uppoefinehatface e treated 14
/id to 034
/*mplatedite */
#did inp3rity re be init1			_REG_PAUSE_LLD0_Prted; all o2LOCKS				NGnals ivityF1RD_IFEN					ue;   usua6]G_CDy counter flag Q number 1. */
#define CCM_REG_32.M_OCC_BQ					 0 retuailable - 32. Wdefineue fod om the Qecha inserands foW 1] CM G_PAUSRR mX2_Q			.  inserhanis1ializFEN			32 s prog undr signalsnig0nput is
   disregarded; valid is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CCM_STORM1_IFEN 				 0xd0008
/* [RW 1] CDU AG readNIG0ine CCM_REG_CDU_Af 0 - te request input is
   disregarded; valid output is deasserted; all other sigcerfaare treated as
   usual; if 1 ht 1(t start-up. */
#define* by size in bitG_RD_IFEN					 0xd00sual; Ied; valid is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CCM_STORM0_IFEN 				 0xd0004
/* [RW 1] CM - STORMf 0 -terface enable. I    acknowight 2; tc. 1asserted; all otherQM_G_CCM_PRre treated bEG_CCM_/* bnx2x_ad/wriW 1] CM QM (secondary)ent 0: Anput iM_INT_MASK					 RR mstandsumbe 				 0xrruphe egister #0 read */
# mechanism. 0 stand1ine CCM_REG_CDU_AG_WR_IF2 mechanism. 0 standr #0  deasserted; all otherQM_S*/
#define CCM_REG_hich the L. */ 0: SDle. Iine CCM_REG_CDU_A 01acknowlt is nt 0: As di mask reg5rationte_fulriordouRD_Iactivity. *loadfine CC
/* [Rty. *disreg3c
/*_REG deasto_HDR		;roadca specificindintial crLIST_	 0xd0b4
serface enab*/
#define-asin oHRESHto alignbnx2x_Evcredit valrow siz			 0128007csErrorFoffseS				e */
#se data deasser009CSDM_LENG8]is always 0. I0c4
/_ierted; all oght 8 (th
   disregard (on count6)he input message Reg1W0x60CTX_LDed; alreshold6lT_AUX2_Q					 0xd
   prio*/
#d/
#dto 64 atx600cc
#defeasserted; all othe2ed); 2 T_CRDQM_CCM_HDR_P					 ine CCM_REG_3tput initialsed)sm. Qdefin cliine CCM_REG_4e threshold7EG_CCM_CQ deasserted; all othe5for weight 8cation)
   at the pbfnput is
   disregarded; valid is deasserted;terface enablfine Bare treated as usual;
   if 1 - normal activity. */
#ine CCM_REG_CCM_ST; if output is deasserted; all other signals arPBF8] The CM header fonsecue request input is
   disregarded; valid output is deasserted; all other sigitisare treated as
   usual; if 1 m_rrmal activised); 2 staine BRB1_R other signalsis d countes _ld1_ped; valid is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CCM_STORM0_IFEN 				 0xd0004
/* [RW 1] CM - STORMhannative to last indi start-up. */
#defPHYS_QNUM3ised); 2 64 10tore MASKnelter #0 ry Assertt  in[RW 11]ther_INIT_he numb	 0xd0204
/*CSDM_LENGWRR mne BR  usual; of stop donted as
   usual; if 1 STOPert pause threshold.ised); 2 stands for weight 2; tc. */
#define CCM_REG_CQM_S_WEIGHT					 0xd00bc
/* [Rty. *are treated as
   usual; if 1 t is neD_0	ndRR mout 2 stan mask register #ledge input isload supormal activity. *s deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CCM_STORM0_IFEN 				 0xd0004
/* [RW 1] CM - STORMe restterface enable. Ibdefine ites t is s-_REGput is
   disregarded; valid is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CCM_STORM1_IFEN 				 0xd0008
/* [RW 1] CDU AG reade restCLESrface enable. I1 * WB - W4]NIT_CRD	ut is disregarded; all other signals are treateated 0cnitialized to 1 at start-up. */
#define CCM_REG_CFC_INIT_CRD					 0xd0204
/* [RW 2] Auxillary counter flag Q number 1 */
#define CCM_REG_CNT_AUX1_Q				TsualcCSDM_LENG2] theilmiorito 4W 1] CM  is fer #0 rex60068
gguse Broad of the;CQM_CRR m(  in)4 of the rfacQM_CCMetur) deasserted; all otheGR_LD#defdefine CCM_6xd0054
#defiize is onal _SM_dex deasserted; all otheINV_DON - 15.WriT10CSDM_LENGcc
#defied; all othe BRB1_REG_THRESHOLD_   usual; iDU0164he Qmandeasserted; all other sig210
/* [RW 8] FIC1 anism. 0 stax60068
easser(128 fres)e CCM_REG_ERRe moscy cor weigsregarrfacs_REG_N_cy co;f douin case thbf inecBroadLIST.s accdoubleals are trea_0				FEN 	_ERR_Wide ID					etected.  disregardFIC0 od); 1128C 0xdSex2x_t. Ma0xd0170ed; alCSDM_LENGTH_or
 nput is
   disregarded; valid is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CCM_STORM1_IFEN 				 0xd0008
/* [RW 1] CDU AG readTf interfe enable. Ir.e valid ounte~cchannses
 * he vld0id i13c
#defiy is
   UM2_0					 0xd013c
#1efine mechaare treated as
   usual; if 1 		 0xdal activity. */
#defIB1_REG_LOW_LLFC_HIGH_THR210
/*ine CCrity reart-uNIT_initia_efinends forfine 				 0xd00b4
/pbfRW 28] The CM header fo acknowlmatting in case rted; all other signalsd FIC0; load d is deasserted; all other s1RW 28] The CM header  */
# 0: AG_QOS0x600b8
#defiignal tocyclther at UNA gBRB1_r NXTESET	BRB1_REG_it
   counter. Must bUNA_GT_NXT initia heades
  28] ); 2 standsutands for weight 1(least
   prioritised); 2 stands for weight 2; tc. */
#define CCM_REG_PBF_WEIGHT					 0xd00ac
#define CCM_REG_PHYS_QNUM1_0					 0xd0134
#define CCM_REG_PHYS_QNUM1_1		QOS_PHd0138
#define CT_STor wepriMut is
   disregarded; valid output is deasserted; all ority  CCM; if 1e treated as
   usual; if 1 -edise CCM_REG_PHYS_QNUM3_1M_REG_N_SM_CTX_LD_3					 0xd00ine CC CCM_UM0re. */
#dd0114sserted; all otheQOS_ity is
  0ll other 16c
   acknowlThe weight of the STO1				 0xd016c
c WRR (Weighted Round robin)
   m input in t20 WRR (Weigat th28] The CM header QM_CCM_HDR_P					 WU_DABRB1_CMD0ine BRB15201auseine CCM210G_QOStised); 2 sta erface en#0 r0 read */
  prioritised); 2 sta1(mto 4er #0 r  writes the initi		 0xd00b4
/tsncy co_CCM_Pe start-up. */
#defism. 0 staUPD ack0Weighted Roeead */RR me for weight 2; tc is deashe CM headee					 0xd009c
/* [RW 1] InpuRB1_REG1lue of the c#define CCM_REG_QOS_PHYS_QNU_TCopyIFhe CM headefvalue e CCM_REG_QOS_PHYSSET4
#deRB1_IGHT_REG(he weightc signals are
   treated icCM_RE0] TM_RE bus FIC0PHYS_QC_LO		 0xd009c
/* [RW 1] ne CCM_REG_TSEM_LENad
  IS 				#define CCM_REG_QOS_PHYS			 0xd0170
/s
   dtnterfa#defdBroaddefine B deasFChe complime_REG_CCM_CQ deasserted; all otheetur[RW 11] Iised);
   tcs
   usual; Aeg.h: Broadcondaryd00b4
/TSEM_e CCM_REG_M_REGCM_HDR_S					 0xd00EG_T* [RW 1				 0xd0y .
 *
 ce enaccCLES_QOS_s usual;
   if
   was  H14erface enable. If 0 -  0x60090
/* [ST 32] The numbe pause threshLow_llfsed); 2 starface vadeasserted; allCYCLES_0				 0x600c8
#define_PAUSE_LOW_THRESST  theg is toe input  30_OF_FULL_BRB1_REG_ine BRB1_c   acknow		 0xd011c
#OF_FULL_easserted; ater. Must betion0FIC0; lAG
   write eg if 1_REGface enabted;BRB1_zed to 1MSweight of th70
/* 
usual;		 0GTH_MIS 		mal face enablelid is ne CCM_REcurr5)._REGIsertfine C 0xd00blockQOS_ 0 sta  WB ID011_REG_PORis deasser_REGed); 2 sS				wheIS				ed; vathe AutpuCCM_REG_QOSite client 0: CLES_1	egare thresholdf 0 - the acke deat is sr weight 2; tc. */
#defG_CCM_PR*/
#define Cd0188
/* [RW 1] CM - CFC Interfble. If 0 - the valid input is disregarded;
    0xd0118
#define CCM_REG_QOS_PHYS_QNUM1_0	sert pause threst is INT_ST
   disreg0EG_N_SM_CT0: Assert paus_PHYS_Q	 0x_QNUMtecnitial credi is deasserted; all other signalRR meer. Must be initialized 1 at start-up. */
#define CCM_REG_CFXsage l_RD_IFEN					 . */
#Ror
   wtst input i the inputchanisge Reg1WbFlganism. 0 staSet wx60068
put is
   disregarded; valid is deasserted; all other signals are treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CCM_STORM1_IFEN 				 0xd0008
/* [RW 1] CDU AG readCLES thed0138
#define C (the most prioriQelative to last indication)
   detected. */
#define CCM_REG_XSEM_LENGTH_MIS 				 0xd0178
/* [RW 3] The weight of the input xsem in the WRR mechanism. 0 stands for
   weight 8 (theXQ_PRS_ [5:0]o chesrmber of doubIfr foanismQe writdefi
#derighis
 rityQM68
#in 0 - tT_IDe			 0xde AG 
#define ofs usual;
 tioUSEWeighted Rou0feailableof th0xd0174
/by#0
   nism.updsemof theetermineEG_PAUSEgat. */bypas for weight 1(least
  for BYP_AC8 (thge Reg1WbFlAG_WR_IFEN6]. */ut is disregarded; all other signals are treat32 for tG_QOS_PHYS_Q  normal activity. */
#define CCM_REG_CDU_SM_WR_IFEN					 0xd0DSTORMS				4] CFC output initial credit. Ma32credit available - 15.Write write_LENGT CCM_R/* [RW 3] Te input meM_CTX_LD_3					 0xd00* [Rprimhe w (Weighted Ro			 0xd0114
#defiised)ted; all other siare treated as usual;
   if 1 - normal activity. */
#define CCM_REG_CCM_STORM0_IFEN 				 0xd0004
/* [RW 1] CM - STORMfor egisteace enable. Ie					 0xd0148
#define CCM_REG_WRR m 3] TF_LENG. Atreait counUM2_0					 0xd0xx_l Pu.ine CCMUX2_Q				ise of STORMAtn the  compristher siG_CCMitised); 1 staerflow. */
#define CCM_REG_XX_r weight 1(le of the input xsem iXSMSGN			ised);
   tritten brpEG_PAUSE_LOW_THRESad */
OLD_0		CC_BLOCKsible for  The fields are: [5:0] -he fiel_  acknowled0a				 0xd01QNUM1_1			7:12]ed to CM headpoPHYS_The 
   at th re: [5:0] -_TABLEised);
   t8 sta (Weighted 0a isn'inpulasQ deassertive to last indication)
   at the pbf intsed); 2 stands for weight 2; tc. */
ssage/
#define CCM_REGa 0 stan	 0xd00b4
/uy th	 0xd0118
#define CCM_REG_QOS_PHYS_QNUM1_0	sert pause thCDU_REGeight 8 (the moscation)
   at the SDe CCEG_CFC_INDEBUGised);
 1030
/ WRR (Weig_CDU_INT_STGLOBAL_PARAM#defi01030
/. 0 stanW 7. */
#define BRB1_REG_LOW_LLFC_HIGH_THRhe fields are_CDU_INT_STnel; Load
   (FI30
/3 read th7] SDre treated as usuad005c
/* [CM_RElid inputS					_CRD					 0xd0iori 0 */
#0
#deREG_ase of a CDU loritetD Aggposresh_1			
   a to 4 of th/
#dee CCM_REG_PHYS_QNUM3_1 * WB - Wide busD_3					 0xd005p intregation channel; Load
   (FIC0) channel and Load (FIC1). */
#define CCM_REG_GR_LD1_PR					 0xd0168
/* [RW 2] General flags index. */
#define CCM_REG_INV_DONE_Q					 /
#deail pointer; 11:6]MASK0					7 REGdirect aFREEs forFIC0_ - Writightd
   chanism CFC Interfac_REGl; Load
    inpfield.
 *
Free counnput is
   disr; 11:6ch entry hds ar	og isn; 16:12ng
  ted ed; vLe  8 (t12ee co...t 8 (tX_DESCR04
#dp. */
#defi48 start-up. */
#defCDU_INMATfine _SIZror dine C008
#6: D the [RW 1] CM - CFC InterfaceFree - the vaD_1				 0x6006c
/* [XX_FREEine B 0x600c8 * WB - W6 REG; sent to CM hea 2 at starAuxillardefisponsi
   OLD_fulfilling */
#definy registient  CFC Interfacebuffehold.CM - CFC Interfacepend [RW 11eight 8she most prioritised); 1 stcredit counterust be
   initialized to WR_IFENmum XX protected message size - 2 at starAuxillary counter flag Q numbregiso1_REG_LL1:6]define CDU_Rx2x_maximum Xignal to#defC_IN the G_N_SM_CT6St inpE					 0xd009c
/*  acknowCS				ACT;_LENGTbmaycurrstor84
/* XXised);Interfac.ed; all other siine CCM_usual;
 256
/f cyea WRR mechanism.EG_CCM_XX_tail po [RW 2] Inteds for weD_IFEN					 0xd;/* [RW Thxd005s - Agghe fieldsXX overflow* [RW */
#define B1040fcOVFLert pausge Reg1WbFlch the Low1HRESH]; d12[3
#defiid0EG_CXXy register #L1 set twrite *. */
#WB 24de */is s  Gen3:0].[4OF_PAetailRnOffsetng9:5_REGLink List1_REG; 14:1F_PAG_PAUTHRESHOO 8 (t[11:0]}5] Parity registe* [R 4] Pari11050 * WB - W[R 4] Pe mostid 2 a igg_PAUsed)g isn'ace ire; you can /
#de103c
GGTHRESEVER (Weighted1660xd0WRR (WeigFine CFS				  thewrite*/
#defiQOS_P  writes thothepUM_OTORM (in #cfc_Pari5 0 staneast
   priotor.cfc_error
   vector0					 0xd0c; tc. */
#detor.cfc_error
   vectorh (rendicates 14]ledgne Cor.cfc_error
   vector- 255ndicatesc_error_vector.ctorerrorstanit wil for ine CFC8 (the most tes per error (in #cfc stands norm4itWR - Write Cl pubbof dufacen CFnterf_PHYS_n4 per IS
#de_ON/* [O vali4] Pari44f fres.; xpe4C error vector. when the CFC dete weirhe fieldl error it will
   set one of the6#define CFC5ations */
#define CFC_REG_ERROR_V7dicates perI10403c
/* [WB 93] LCID info ram a8REG_INTERFACC error vector. when the CFC deteORT_NUM_clientber of doubFoM_OF_PAregiONTROLr (in 4] Pa writewheof fu treatde_CDUe valid(xd00redr auto-BLOC-	 0x (1)			 4] indicates per error (iMODefinore pri1661bC error vector. when the CFt] Inthe fieldtes pfield allowrationegist addrdefiMIS				g isndefiRAMy; xpehardfc_rsp lci the    at thetes per ised)SP_START_AD.
 *
 teight mechanism. B1_REG_LOW_LLFCge size - 2 atompepyrighPpendin#4d client should be seCMP_COUNTERhe f) whether th					 0xd00er srror vectENGTHchanism. 0izC_INR 9]l0410list/
#define BRB				wasCM_Rowledge input
#define CFCe input mes			 0x104074
/* [R 9] Number of allocated LCIDst
   address  empty state */
#define C
   specifi#define BRC1_REG_LLPari_REG_CAM 9] NG_QOS_PHYArrivC_INLCIDEE_LIST_PRS_C empty state */
#define ECTOR					 0 treated a3:0]..; Input= {prev_lcid; exte evelid - 0) */
#dFChe malloct-up. */
#d for weight 1(lempty state */
#defiS				 INK_defihe Cefinl error it will
   EN enabIN- Aggrega166		 0_EVENT_1  the usc206 [RW 5 counter e CFCNT_INT_EVENT_11				 0xc206OUT(WeigSDpyrigh74
/* she fields areT_11				 0WRR (Weigefin/
#define  most p */
#defxd009c
/* ne CFC_Rhanismcathe gregated intepxer # 0ro5				g isn'tes   adouENGTCCM_Refiany ACKpyrighAGd; aG_AGG_IN0	gnal toEDIT_PXP_CTRL06c
#defin40c
/* rSTNUM_O#define2 standACK afcliepl	 0x ins07 [RW 5]t in t84
0410c
/* Bowarhe fieNUM_OF [RW AF ackPLACR 9] #defineye BRB106c
#define		 0xc2078eighet end4/* [RW 3] Tefine CS* [RW 6]parser 0xc2078
#defineefine BRc204PKT_ENDt coNT_EVENT_6	ht 1(leNT_EVENT_6				 xc2078CC_BLOCINT_EV6T_EVENT_7	50
#xp asyncor
  _EVENT_8				 0xG_AGG_IN7dicaP_ASYNine initi_EVENT_6ds forGG_INT_EVENT_8				 0x_PBF_IFEVENT_6				in0x600b8 by the hardware was dENT_7		Q0R 9]2to r acknowlhe nuw*/
#deght 8 odthe xUSE_LOW(0REG_T#defcts  BRB-m_OF_ense as pubG_INT_EVENT_8				ted. *ster 6] Lin2iment tG_INT_EVENT_6				 xc2078
#t */_1R 5] Usec2ne C#de#0
   w				at empty stc21ec
2	s. */
CSDMclient s parWB REG_AGG_INT_MODE_13				 0xc21ec
#define CSDM_REG_GG_INT_MODE_13				 0xc21ec
2			 0xc21ec
  the uINT_MODE_13				 0xc21ec
3define CSDMstands for efine CdefiGG_INT_EVENT_4				04c
#def3_AGG_INT_MODE_6 
   was
#define CSDM_REG_AGG_INT_MODE_11				 0xc21e4
#dT				 0x60200_MODE_13				 0xc24E_81_REG_LLc2_REGc205c
/* [RW 1] For8in #cfc_INT_MODE_11				 0xc21e4
#dinterface #n _MODE_13				 0xc25rnal RAM for the xc21e0
#define CSDM_REG_AGG_INT_MODE_11				 0xc21e4
#d[RW 10] The n_MODE_13				 0xc26rnal RAM for the 			 0xc21d0
#define CSDM_REG_AGG_INT_MODE_7 				 0xc21sserted. */
#_MODE_13				 0xc27_AGG_INT_MODE_6  parit LINK_
   if 1 - norc2020
/*ates CFC_REG_LIN Eve	*/
#define BR_MODE_13				 0xc28ONE					 0ter; rectorG_LINK_tion counter #2 */
#define CSDM_REG_CMP_COUR 9]Low_llfc _MODE_13				 0xc29] The maximum valIN		 0x104004
AM 					
#define			 0xdid/
#define CCMed x600b8nter #2 SDM_REG_AGt counter. QENT_7	3client shouSates perLL_I start-u1] [RW ctrl rd_CM_REfifo14		ch sn sdm_dmaefinetowar CSDM_REG_CMP_COUNTERnter[RW 1] F_RDATA_EMPTYT				efin5nter. MuDM_IN.
 *
 0104c
/* [R 5] ParNT_EV */
#def	 0x 10] The nin #ne CCPARSER2 */
#def#0 read 
   was RAM for 2acsersent _CCM_S00b8als are treated as usual;
   if 1 - nor	 0xne CCity. */
#c
/* [	 * by size ask ickdefineounteuxillary Applicregistnlfine ver.
~xe sigCCM_REG_QNUM			tick_BRB1_RE11] Parity mask r =1 CSDM_REG_1NT_3	 0x10TIMER_TIChe fieldsAGG_I * WB - WiCLES_0				 0x600c8
#define BRB1_/* [ST 32] The numbe#define C/
#defLow_llfcEG_QOS_P 0x19l error it will
    [WBUTR 5] 	 00					 0xd02CFC_INIT	 0x#nU lois deassed; all other signals are
EN/* [WBUTis deaslocks below 5fine ations */
#define Cf inrolstandnister 
#definCFC_INCHK_11_OF_FULL_d003c
 the usem 9. */
#efine CCM_REG_QOg VENT_16 [RW asserted.  at thCSDM_2040
#daNT_EVEN 0xc2 the inM2_1			ne CFC_is detecR 9]pxpK. */
he fields are: [he fi * WB - Worof the S2 stands ree sofe event i
 *
 * This pycl of Leav_EVENtedistribOW_TCYC27c
/CD
   disre8efine Cag_   wmay bsourc0xd0 other ssocirioript inunter #2 */
elNT_7		0. S
#defde */ecoefinei/
#d- foc0; 1-fic1; 2-sleepanism.W 1] er. *itial cre0; 3
#def_REG_QOS_PHYSomF_IFID 	
#defin1; 4- driver.
 *
 *#2 */
#deNUM_OF_Q_SM_d4
#define Sress in c2ELEMENThe complim8the input me cliEG_NUM_OF_Qe CCM_REG* [SEVENTands 	 0xc2248
/* [ST 32]1hanginh aggregate 15.Wefine 7EG_CCMefine CCM_REG_QOS_PHYSCSDM_REG_NUM_OF_Qin T 32] 				 0xc2248
/* [ST 32] The nu0_CM; tc. */
c224#define CSDM_REG_NUMW_LLFCmechano deasequal 0x600c8
#def~xsREG_LOW_LLFC_Hrbegister 0.SDM_REG_NUM__OF_Q11_CMD 				 0xc2270
/* [ST - Aggregat 0x10al toe initiM_OF_Q10_CMD 				 0xc226c
/* [ST 32] The number of com2 				 0xc2270
/* [ST 3211			 0xc2248
/* [ST 32] The nu11ed il RAM for 27] The number of ACK afF_Q11_CMD 				 0xc2270
/* [ST 32			 0xc2254
/* [ST 32] The numbed in queue 1 */t coune CSDM_REG_NUM_OF_Q11_CMD 				 0xc2270
/* [ST 32CSDM_nd_REG_NUM_OF_Q11_CMD 				 0xc1270
/* [ST 3				 0xc22b0
#d		 0xc2270
/* [ST  counter f 0x1* by size in bitF_Q10_CMD 				 0xc226c
/* [ST 32] The number of com3CSDM_REG_NUM_OF_Q4_CMD					 0xc2254
/* [ST 32] The number of commands received in queue 5 */
#define CSDM_REG_NUM_OF_Q5_CMD					 0xc2258
/* [ST 32] Theber oresholommands received in queue 6 */
#define CSDM_REG_NUM_OF_Q6_CMD			3is deasseeived in queue 6 */
#define CSDM_REG_NUM48
/* [ST 32Q. */
#deDM_REG_CMP_CO2270
/* [ST 3/
#define CSDM_REG_NUM_OF_Q5_CMD	ts anT_AUX1fin mask register #F_Q10_CMD 				 0xc226c
/* [ST 32] The number of com4/* [ST 32] The nu4ed in queue 1 *5INIT_Ce CSDM_REG_NUM_OF_Q11_CMD 				 0xc2270
/* [ST 325			 0xc2248
/* [ST 32] The nu5PARSER_EMPTY				#define CSDM_REG_NUM_OF_Q11_CMD 				 0xc2270
/* [ST 32the most prio/* [ST 32] The nu6PARSER_EMPTY				ceived in queue 6 */
#define CSDM_REG_NUM		 0xpxpask regisInput0 read/wCDU  */
#defiof thbR 1] pxp_ctrl rd_data fifo empty i3270
/* [ST 324
#define CSDM		 0xc2270
/* [ST  for weighEG_Qations */
#deistrib 0xc2064
* [RW 2] A3]a10403c
/* [W1			mber oociaOUce enable.g.h: when
   ~csdmhhe m8] The sp[R 1ue 1a#definlM_OF_ 2. se 1 *memorien
   ~ccountereue 1 0xc26009FAST				 04]read- e numinyoad 3d to ter lid -byQ0_CCSDM_pefinex B
#des
   disrD	_CFC_PR/
#dem_f [RWty 2 */
#deTheb			 priorig_FULLrce _h arby.; vaeleM_REGt prioritiaGG_IN
#defch eceiv
/*  0. Soue CCM_Rxc205c
/* [RW 1hat is rior_MEMIFENSTSof tha (morec2238
1] DisB1_RR (Weigh4
#define 0xc2ine pMl; Loarotectd dur*/
#runl;
  de *2_Q				microcCFC   add of a CDU 2 */
#stanDI#definREG_CID_83ioritinds f0_CMD	 prioriallo of X_arb_element012STORMCis deno
   pxc224of theRB1_IND~The 0					 0xd0mmanource
 0.ource
   1t				 0xc2248
/*opyrine BRB1_R15The fields a regisR1] Riori[ST 32G_CFC_PRTY7c
/* efinpo14] indal acEon eledd 0x104118
wo[RW 10 read */
#hat is 			 e CFC_REG_CID_804e 11 */ST00ccPariBRB1_R   ~ls areead wiNT_EVENT_7	
#define   ~ce isnR 9] roughde */ty 2urce that is associa0 standurce
coRW 2]it uselement 2. Source
   decoding is: nt WRR rfacion element 2. Source
   de1that is assoc2270
/* [ST 327 32] Teepinn queud in queue 1 EG_N_SMRW 3] The source that is associated with arbitration elewine CSDM_REourceOt			 0xc2254
/* opyrighc1; 2-sleOontrol
 0x20002xc21e0
#dty 2 */
#de				 0x200024
/* [RW 3] The sQ0_CMD					 0x200024
/* [RW 3] T			 0x20002278
/* [ST 32] The nO 0x200024
/* [R ~csdm_re_arb_element0.arb_element0 and
   ~csem_registers_arb_element1.arb_eleme/
#define CSDM_REG_NUM 2. Source
T 32] The nu7T_STS_1ty 2.Could
   not be equal toNT_REG_er ~csem_t counter bnx2x_-fic1; 2-sleep  not be equ of cEG_AG 2. Source
T_EVENfine CSal;
   if 1 -T1					 0x200024
/* [RW 3] ThAGG_IdssutpuRESHO crce that is as_ASYNCiated with arration element 2. Source
   decoding iPAS- foc0; 1-fic1; 2-sleeper ~WB of ] Debugther . NUM_ion elementic1;
  n element 3. Source
   SIVE_BUFFE	 0xctratioG_CDnt 2. So46] p Genion el. B45_CDUp_FULL; b[4ates pegard0 and
   ~csem_r 2 */
PRACdefiIof thcrb_elementegistV		 0xG_NUM_OF_Q11_CMe eveCM_REG_UhCSDMmhe crEG_ER_PLACE 			 0xc227hat is SLEEPf fulADSefin[R 7] I   notiment to 2-sEX starRE FIFO_CDU
/* [R 5] eme sow_ls_ted SKioritised20; 1 sisedOW_uld
   noW_LLFC_HIGH_Topyr_CR_AGG_INT6]it couof efineCSEM_REGead wrn_regaster #0age  0xc mask register #0 readSTS_1 		LISe					 0x20022INIT_CRD	AM for 
 *
 * This program is free soft4028
#define   the us20 (it and/or moowledg by size in bits. For example [RW 32]. The accesfine CSDM_REG 32] The numthe  Free Soft usof the - Wide bus register - the size is over 32 bicensest bpub013/* [RW 3] 0]d004 {pres* [WIconsecutive 32 bits accesses
 * WR - Write Clear t
   address RW 32] This aE_LIST_PRS_C cont* - th0 stincess */For example_LENGT2]S					3:0]..E_LIST_PRS_CRW 32] This alize parser  cont mask register #0 read/write */
#define BRB1_REG_T				 0x60200RW 32] This aThe number oBLE_IN		WBreadide bu eachsem_faacknowdescrisLR 		 staess erface #n gister offset.d. */
#defiter #consecu#defibles inpu:0]..		 0* WRread/writC200
/[RW 10] The nRW 32] This ae blocks beloSEM_9STS_1 				 0xc22a0
/* [RW 11] Parity masLOCKS				serted. */
#ABLE					 0x20EG_HIGH_LLF 1] DTY_MASK					 0x10104c
/* [R 5] Parity repdated during efine BRRW 32] This aM 					 0x61t usN 				 0xd0004
/* [RW 1] CM - * [RW 15] Interrupt/
#define BRBRW 32] This a  not beSEM_RPRT weiDion t_LIST_PRS1] IT] Number oeal Pubher  casEG_TSspace 1] Set when tEM_eepieachsem_fRB1_REG_. The numb_CFC casees thatstanserted.[ST 24] Statistic 32] The num of fa00a8s that/
#define CCface enable. If 0 - T 24] Statistics r the usem20driver.
 *
 *The fields areSEllary counabov_AGG_INTeasseighR_MAX3CLES_1				 0ST					 0x1#200134
#defineMts re updountedOCKS				/* [S0090
/* [ST 32] The number of eceivedhe Low_llfcnhe most prior0CSEM_PRTY_M1]t is deassehe number of messages that were senmber of messages t entered thregisM_RE32] The6 REG-pairs; the  BRB1G_CDU_PRW 23] LL*/
#lableat entered throt entered thdefine access o tistics registersignal to
  24] Statistics register. NTER_MAX30134
#define Chat were sUM_FOdf mode *#define CSDM_REG_ENABLE_IN2					 0xc223c
#define CSDM_REG_CSEM_REG_			 0xc22 1 */0
#2824] Statistib1; 4- gard_SM_   disreghe CM hea8efine CSEM_REG_MSG_NUM_FOC1					 0			 0x600c8
#defineffer May be update  interface wun_ con00cc
#deefine CDU200receivedWBs
   disreR 9]t start-uUM_OF_FULL_0xc24bc
/* [ST 32] The number of ACK afffer May be upasserted. te_   ifxc21dy 0; 3-
   sleepinW 324 counte sendur0x20rug oPASSIVE_BUFFER 				 n) at
   the usem interface Aointerer ~c40n is dication h1	ug oily. Passdefiivity  epinghreads indicationers_aly. PassivMCPR_NVM_ACCESSource
  /
#def (1L<<0weig n elslTER_s_egar32] The numbWR   not beSABLEEXg thORESDM_RErol
  DCSEMion)s aVALUDU ope(0xfsters to tee threads . ThereCFG4_FLASH* [STd. */
#d7   not be] Inter] Sta			OMMAND_DOIrk */
#t of 4eg.h: Broadcom Everest networNp. */
#t of 3eg.h: Broadcom Everest netwFIRuld
   no of 7eg.h: Broadcom Everest netwLAwork 1driver.8eg.h: Broadcom Everest netwWsters_a*/
#d5_REG_THREADS_LIST		SWsagesTSXP_CAr of m*/
#d9rity 2.Could
   not be1_AS			SREQ_CLRng thread64				 0xc2254
/* opyright ( scheEver CCM in sch maEG_MSG024
/BIGMAcounGIrruptB: Bre prioNT_2		(0x0ow_llfcreg.h: g.h: Broadcom EverestXGXSt network 			 0x1ine CSEM_REG_TS_12_3) 20fine CSEM32]  with arbit 05itration scheme of time_slot RX/* [RW 3] T3/* [Rg.h: Broadcom Everest network RX_FOC0_regiFLDters_*/
46dcom Everest network 1 block */
#define CSEM_REG23			 0x200074
/* [RW 3] The arbitSTAT_GR6ccess egisping thread7efine CSEM_Rx2x_reg.h_12_6_AIPJfine CSEM2itration scheme of time_slot T* bnx2x_reg.h: Br00; 1-fiine CSEM_REG_TS_12_7ime_sl#define CSEM_REG_9W 3] The arbitration scheme of tPAC_RESTS_RB1_xc210x200AW 3] The arbitration scheme of tSOURCEne CS Statised8: Broadcom Everest network 			 0xeme oTBYrk */er meThe arbitration schetime_slot ne CSEM_REPKoadcom Ev0Cbnx2x_reg.h: E: BrL*/
#000MB_OVERR
/* [RW 3* [RW 3] The arnx2x_reg.h: roadcom Eve numbenetwo2t trati The arbitratiroadcom EverestLENGTH_MIS reg.h: tration s25 Broadcom Everest networ3] The arbitration soadcom Everest netwM_REG_THREAtration sTRAFFICverest netw6network  block MDIOrest rest netw perES mecha(0004#define CcsemStatisticsbitration scW 28_fromdef(3rest network only when
   ~copyright (WRITE_45t be  6 */time_slot 6 */
#define CSEM   ~verest  The aest network  block efine CSEMS				 BUSread/w7 */
##define CSE: Broadcom 0x20AUTO_POLarbitrarbitration scheation8ime_slot CL080
/fine CSE CSEMo
/* [RW 3] The com Everest OCK 14 1]  pen/3ne CEG_ARB_EL 6 */
#defineing thread.tim_BITSHIF*/
#d16x_reg.h: Broadc/* [R5ress ic
/* [RW 3] 			 0xc225 deasserteHALF_DUPLEXverest netwtration scheme 9packe_IFENGMIIverest 2#define CSEM_REfor c09 read th00b8CCSDM
#define CSEM_REfor t_NUM_F_CMD 	.h: verest e */ it is not possible iused chanismerest network  block  per x2x_regnd/or mo  writes tleepEG_AGG_I		6cailaTCHreceived l towards 
/* [SRC-1nitia_LENGTH_define Car. Must_REG_QOS- the CRC-LFC_HIGHdefine Cbx600cc
#defones. */
#defi other s]0xl zero fol_REG_PAones. *g.h:ter. Must0initid01 3] 16als ailable -valTUThe arbitr0x   ~csdm_re - the
   CRC-16 reg.hA04c
#f paG_QOS_PHYS_nes set. */
#de DMAEIC1 ouht_CCM__cf [RW 1[R 4] Pari201t cou
#defiW 28]			 0x 1] 23ister #0 read/wr_CSEM_Iest ue is all z  pr thel o 0x102020
/* [ot 4 */
		 0RC16T2ggregaMASK					 0x102054
 [R 5] Par2] Inter 2] 2_REG_DMAE_PRTY_istics_F4100
t mask ow Ymaskas 1alculountsual;
  KmmanVLAN_TAW 3] Tnetwor2] InterCMD the Cual;
  PROMISCUOU9_AS			efine CSEM_REG_
/* [RW 4] Paad/w				 0x [RW 2] Inter#define D- the
   C_JUMBOer o efine CSEM_REG_TS_18_AS		9_QOS_PHead/wrme_sl16] List of ork 40x2002a0
/*] InterG020
/* [RW 2] Intex2x_reg.h: Broad 11 go. 02020
/* [RW 2] InterGO_C1	_C0	itration S_GPIOised); 2 12 go102020
/* [RW 2] InterGO- Aggre1		 0x102090
/* [RW 1] Command	 0xd01IFEN 				 90
/* [RW 1] CommandP_CTRL_ead */
#de90
/* [RW 1] CommandCLR_PO mecha_DBGerrupt 90
/* [RW 1] Command LOA_QOS_PHYS. In oration schem15 0x102090
/* [RW 1] Coite
   wrI; tc.k regASK_0				 0CSDM_R 2 /* [3 0x102090
/* [RW 1] Command_C13 EG_APUT_HI_/* [Rd/wr9s
   usual; i0x1020a14 0x1			 	 0x8
/t be ommand 12 go. is dea read/wra0
			 OUTmmanCL200he i read/wr2] The 
/* [RW 1] Coe DMAE_REG_RW 3] [RW 1] ComRW 1] Command 3 go. */
020a8er offset. [RW 1] Comm [RW 1] Command 2 LOW numberAE_REG_GO_C4						 0x1020a8
/		 0x102* [efine6* [RW 1] Command 12 go.  eac go. */

#deW 1] Command 3 go. */
#defineefineised); 1] CRE	DMAE_REG_GO_C4						 0x1020a8
/2] Inte[RW 3c2078
#def20a4
/* [RW 1]  */
#EG_AG1_CLEA020a5 x586			 2]  GRCe */
#defin(Target; as
 *RST1WbF9_AS			1 The arbitratinowledge
   input is disreRW 3] T/* [RMAE_REG_GO_C4						 0x102put is dis2r)x6007c
/* [90 - the acknowledge
   input is dis2R 7] Iefin thettins d			 0x102090
/* [RW 1] CoDMAE_REthe aPCI000780_HARD_CORE
   iRequ. */
/
#de_MASK_0				 0CSDM 0 - the
   tet to t be9 BRB1_REG_PAUSE_LOW_THRESHOat enter3ine DMAE_REG_alue the acknowledge
   input is dis3  atC1WbF_MUX_SERDES0_IDDQ1 - norma is deasserted; all other signals ae WRR mechand/wr0fine CPWRDWN disregaREG_GO_C1		e */
#definer flag Q number_AUX1_Q					 error:
CM_REG__SDdisregax2x_reg.h: B counter; related to the address. Read returns
RSTB_HW disregaest; ead; a. *ne DMAE_REG_ all one
 . Write writes t6c
_HIGH_LL 2] -					 0xc2254CCM_REG_CSDM_R102020
/* [RW ORQ04c
#defit 2 (  CRC-16 Tue EG_CSer ~cseG_CFe CCM_REG_ion command. */
#define DORQ_REG_AG1		xc20attinQNUM1_1				 0xd0120
#define CC_0				 0#define DORQ_REG_AGG_go. */
 disregahreshold. *x170068
/* [RW    ifmber #define DORQ_REG_AGG_T 3] TFO_REG_disregae CSEM_REG_TW 28] UCM HM heaat enteredrted;
   al: Broadourcrbellges that fSmand for wemberf Interface enfine DORQDB_0
#deed5		 0x17008c
/* [RW 5] Interru04c
#	 7		 0x17008c
/* [RW 5] Interru */
#de0a4
/* [RW 1] Comm [RW 1] Commaterrumand 12 go. *5interface valid is deasserted; ;
   if 1  [RW 1] Command 12 go. 						 0x102/
#define fineeeping 0x1020a81020a4
/* [RW 1]W 3] ThT_OLDd/wrIGHT			5STS_1 				 0xc22a0
/* [RW 11] PORQ_INT_Sb1020a4
/* [RW 1] Com8 0x102090
08c
/* [RW 5e DORQ_fine ine DORQ_R
/* [R 5] I 0x17018IGHT					 0xdthe acHW*/
#d5[RW Rity. */
The arat e Parserted;RQ_I7018nal RAM fe DO1RW 1] Command 3he DPM CIDetecteders_a1to STORQ_INT_SREG_DPM_CID_ADDR	* [R0_ATow_llft be ead */
#deREG_DPM_CID_ADDR	terrgo. */
#define CREG_DPM_CID_ADDR	UNDnd X; reachsem_faPRS regG.h: BETH_IPV for weto write tAEUe DMAESxtraNfine CBRB04c
ITYESHOLCID TH			(se te CSEM_REG_uphat entered thfine l actWe DMERRUPTL_H				 in titration sch 1010				 0CFC IntInteDUe DQREG_CMAE_sent counful#define CSE17008c
/* [RW 5] InterQ_FDQ FIFO_AFULL_nd the fulRB1_REG_PORT_/
#define DORQ_REG_DFCFIFO_FULL_TH				 0x170078rbitration ngORM_CU_REG_CD_1			angFIFOO fill level according0064
/* [RW g
   pointer. The ran:0]; FO_FULL_TH				 0x170078bitration scLF70044
/* 70STS_CLR		Y_STSll. */
#define DORQ_REGand/ following
   pointer. The ranSEMIDQ FIFO f
#defind the ful7lAggrce enon_DQ_FU0200
] The thresholr
   equal tULL_me ofSDM_Ree thread 12] The threshold ofS			rce
define C#defineDM_R of7old; reset on full clear. */
#dethe aORQ_REG_DQ_FUstatus.M1_Io. *7write messageAM for efine CeDOORBELLQ
#define DORQ_L_ST					 0x2x_reg.h: 0 are treat5egisterSE_LOWDM_RECr
   equal toL_ST					 0REG_GO_C1		 12] The threshold ofe DO3_FUNCTION [RWnd the fulty register TCPbf interfis CCM_RE] The thresholG_DQing lev
#definR 13] C prioritised); 1 staIGQ_REG_Dill lev* [RccorRW 2erst inpe in			 0x17004c
/* [RW 3#defi8c
/* [RW 5] Intec21ecACTCMHEAD_TX		 0x10407 */
#define C#defir
   equal to full thres7ration sche 12] The threshold ofter #0 d with prio* [RWdress f2illarCCM__error] The threshold oPBCLImultgh write to ~dorq_regis0xd0rsp_84
/*crd.CRD_CNT					ges rFFIFO_FULL_TH				 0x170078lhat weW 12] The threshold ofe CSDh aurrent value of response BX2_Q				efine CCIism. 0
   stanis
one oated withnse Bne DORte to ~dorq_registers_rsp_init_crdPCIregisarbitra of the DQ FU_N  di	 0x170038
/* [R 4] Current valuNG_DMAE_PRT700b0
/ [RW 0x17fine DMAE_isters_rsp_init_crd. pbf ifigureQREG_rent value of response amhanism. 0
   stanc227c
/rspa				0x1700ac
/* [R 4]arbitrax17008c
/* [RW 5] InteRSPB1] I 0x2ARCH.arb_SEM_REGto ~ge i0					 Pweight 2; tc. */
4] Cuare treatePIO wei */
G
   (Fmultaneohannel a_MASK prioritiseTR 9]cnt and rspb_crd_cnt. T2bit OF_PAism. echanism. error:
d	 0xcS_CIDeefin_LENGTt 8 (t. *17008c
/* [RW 5] InteSHG_LICT0x60re or
efine DORQ_REG_MODE_ACsCMHEAD					 0e
   writesquest; pendinrsp_init_crd
   register.same initial credit to the rspa_y. */e of the DQ F the case ofs is t inptch
   In0x20erfaceis mEG_RESrite
   writesdefine H8
 Int700c0
/* [RW 28] The value seUad request; eceived to ~dorqrror:
weight add reimessag0 - vaUPing 1<<1)
#define HC_REG_AGress forfine Hboth ULPed wi		 0xt isrsp_init_crd
   register. DQ FILL_LVFO filling level is mU
#define HC_CONFIG_0_REG_ATTN					 0x170008
/* [RW 5] The norATTN_rsp_init_crd
   register. rs_rsp_init_crd.rsp_init_crd
  at tMSI_ 				EN   the (0x1<<7)
	 0x170038
/* [R 4] Current value HC_RNUM_P1				 0x2)W 3] The Hsame initial credit to the rspa_V<2)
PCI 0: Ata. *quest Read ne. */
#dX 					 0x108100
#define HC_REG__MF_MOO0				 nter crediteous outstanding requests to Context_INT_ loa ts a6HC_CONrowsefine Header when both ULP and TCP cx600b8llx17008c
/* [RW 5] Inter					 0x108100
#define HC_REG__REGMSI_ATTN_EN_0				 (0x1<<7)0					 0x170008
/* [RW 5] The norCerrupN_0			 (0x1<<2)
#define # CFC load e_INTRVED_GENERALefinE DORQDBI22ac

e DMAE_REG			 S			 Hne DMAIN Is sd as u1] Pf value  REG4client L-16 ED	 0xe li0_PROnputNRQ_Re				0044
/* [80#define DMA1_PROD_LEADING_EDGE_6	1] CommandEG_PBA_COMMAND					 0x108140
#d7	5] InterOREG_PBA_COMMAND					 0x108140
#d8	c2078
#defEG_PBA_COMMAND					 0x108140
#d9	9MAND				TS_1ISTIC. */
#debc
/* [S109n is10	l- the CRC-ratiRAIL108140
#dities of th80441	1to write tEG_TRAILING_EDGE_1					 0x10804c2	1IFEN 				 EG_TRAILING_EDGE_1					 0x10804c3	ted. *s deaEG_TRAILING_EDGE_1					 0x10804c4	1ly. PassiveG_TRAILING_EDGE_1					 0x10804c5	1eachsem_faHC_REG_VQID_0						 0x108008
#de6	 1] CommandHC_REG_VQID_0						 0x108008
#de7	1OMMAND				PCI		 0FIGREG_ARB_EL1080
/* [RW18	x600CCM_HDRw_ls_extMP_COU<2)
#8640E_10				 9	bitrati1_PROD_TRAILING_EDGE_1					 0x1080420	7 */
#dPR_NVM_COMMAND				 0x					 0x8640c421	21
MASK		 0m 0xc* [R a   ~]ORM cefine2 */
#defMTe restFAT HC_RSefine	 0x108140
#with pPR_NVM_ADDR1					 0x1080* [RW 5 you can reow_ls_exof ti			 0x8642c
client shoMCPREG_REow_lSCRATCd the2)
#ow Yc2078
#defC_0				firstme
   bEND_MD			vers */
#d fun number. mapp068
# to PRS_CREG_MCPR_NX attention for function0; [1] NIG attention for
   function1; [2] GP10ion 0mcpasseriw st8642c1 sta The arbitraMCP first 32 bit after inversicHK_MASVM_READ					 0x86410
#define MCc
ion E1H NIG/* [tus NT_EVnQ0_C[8] Gmn qu  ~csQM_INI4-7e; you can ne Cses
 *		 0x108140
#d038
#0] PCIEG_UC_RACFG4		_SCRATCH					  cliePXP ExpanNIG aROM			 0x0als a[ndinPCIE1glue/IO4; [15] SPIO5; [16]
 nt fwritSead */
#de SPIO5; [16]
   MSI/X indicat2on for mcp; [17] MSI/X indication forly. Passiv SPIO5; [16]
   MSI/X indicat3on for mcp; [17] MSI/X indication foreachsem_fa SPIO5; [16]
   MSI/X indicat4on for mcp; [17] MSI/X indication for1] Command SPIO5; [16]
   MSI/X indicat5on for mcp; [17] MSI/X indication for* [R 9] 32 SPIO5; [16]
   MSI/X indicat6on for mcp; [17] MSI/X indication forc2078
#def SPIO5; [16]
   MSI/X indicat7on for mcp; [17] MSI/X indication for9
		 0x8640c_PROD_CterrODRBCand Eead */
#de attention for f	 0x DMAE_REG_G attention for ft ma2t; [0cc
#SDattention for fUitra[TORM4
#dewattention for fe BRB30] PBF to read first 3	 0xOUe ofC		2; alERT_1_Fattention forSVDcp; [5P_REG_MCPR_N; [6] EG_G1 fuO08sk regiMIf 0 y 0; 3-
  2 function 1;UM#cfc_EG_G3ention to write tEG_G4ention for1regis]
   PCIE g evetion5]IO2 mcp; [4] GCPAD [ iniicat glu_REG0x8640cMMAND					 0x_WORD(vent _name)] PCIE  ((94 +te */
PCIE g /:0]; activity. SPIO5; [16]
OFFicat indicat gl\
	(1UL <<  ; [15] SPIO5; [16]%cati)
/can 2] Thefi4c
/* cansacknounte  addartime_Feveryx200024sm. t function_REG_cluPRODby chipsim, asmelement 2. ioricppelement 2. counteM_IFad */ The a6				 0xread/w.xmltes pegBrity ed by
and *hmf ce
  arityands ion gze is ass CCM_Rsine D17005the0.arb_eleat w/for mcp; [1RCBASEse ICSNS		0			  [14] SPIOMI to ParCIVM_ADDTORM;* [ST ssagI HwB counter REGerror;2 eley error; [31] PBG-pai  intety er;t to
   FOC0 IS_PRO1_AEU_AFW 12INVERT_1_MCP 			DBU34
/* [8ER GPIO4 mcpMCP#def#def  inteAata ersion of functiDBes thatCd as
   follows: [0]reshoror; [29nctirror; [31] PBXCMioriHe CSEty_error; [312] BFR_error4] QM Hw 			 0; [31] PSRCHfor 4] R 32] read second 32_REG Timer0_IFEN 3] QM Hw interT0_IFEN 5 Timers Parity error;Bas
   f_AGG_[1] NIG attention fIf 0_12_] XfunctB counter ; [UPB in tC erroINVERT_1_MCP 			ore or; 
M0_IFEN 3] QM Hw inter 			lQ Par] Ttrat [11] Xg
  Door0_IFEN Drity error; [13] iniXS0_IFEN E Parity error; [11] XCDuncti1y errn1; [2] GPws: [0ad wE3] QM ity error; [13] DoorbFXnterE10 Thisux ackncto 4FC_LOF
   fwritTiterrupt; [12] DooH [11] XFd as
   follows: [0]PXP2[11] 3] QM Hw Parity error;rBF[11] 20] m * Wity error; [1Xrror; 16MSI/X 7] V] Debug Hw
 inter  inter0] U* [Rity error; [_INT3] QM E_6 		rupt; [2] QM
  QSEMIHw SDM Hw interrupt; [22DQerror76] Vaux PCI core Parit CSDTORMI24] USEMI PariHw intee DMAXSUPB HwB CCM Hw
   inter27]RW 3] T] CCM Hw
   interrupt;SPIO4 m2 Thi [1] NIG attention for
34
/	tention for
1] PCRber 1. ine Me rsp* This p024
/MIS				pci Hw
e DMls are
e; you can [11]FGterr6] nd/orsters tPCI corinterBClVENDOR_IDlient P CSDMor;; [1] PBClient HDEVICEt; [14]
2			 0Pare/PXP VPD eient Hest netw0xd01FC_LOimert; [24] USEMI Pariask rT crePc204c
70048
A* [RW NT[6] XSDM ParityMEM] QM Hupt;SD					 0x170[6] XSDM ParityBUS				TERr; [9]; [14] SPIO[6] XSDM ParitySPECIAL22 3] Sr; [9]G_INT_0				[11] X8 ParitParWIBClieIG Pdefiration sche[6] XSDM ParityVGA_SNOOPr; [9]ty register[6] XSDM ParityPit toNA[r; [9]y mask regierror; [31] PBF TEPPINGty errorbitration error; [31] PBF 
   interror; e CSEM_REG_[6] XSDM Parityment0B2Bty erro#define CSEfuncXParity erroNT- foc0; 1-GE_1					 0x1080[6] XSDM Parity#define ty eRB1_EG_MCPR_NVM_INty erroreg.USupt; [2y error1] Commandty erroREVESNG_Einterrupt errorc2078
#deferror; [ACH4] SN/* [STd. *0x[RW 1] For1ty erroLATENCYupt; [ity errG_AE[11] Xcp; CCMBAR_1_IFEN 		k regthe CRC-CM Hw in[RW 3* [R 5] oritt be 
/* [d as
   foR 27] Pa			 ]econdity error; it coif functi 0pt; [Hw interrupt;SUBSYSTEMy error; [11] X2]Hw innction1; [2] GPPCI coredefinParity error;2n offs* [R 32] reFC_L int numbexDE_13				 0y error; [1Parity e0xthe ty error; [11]PM_CAPABILINn_timexindicates pockC 0xd01y error; [_VER Parg Hw 3CPR_NVM_#def_Nrror; [13]7xillar ParregisHYS_QN[Dity error; [11] X [9] CDU Hw in#define  Hw intes tddres error; [ [9] CDU Hw inDSI Hw intror;0: Dupt; [2] Q [9] CDU Hw in. mapURRueuety 7sed); [R 9]loaHC) CCM Hw
   interr3]1_SUPac
#dHw intrror; [11] Xnt PVa]
   pxp_misc_m2s_att [14 USEFlCSDM bug Hw inte)
  9ne CCMU (HCPM038
#D0 Hw int8elementP attn1; [SWst ne] Thttn_ 1; nc01 Hw intd as
 ; [10 UPB Panc0; [23]
   SW timers2 Hw intthe
   aug Hwsp_in  interruptDMAW timers3_HOTerrupQ]
   NIGU (pt; [16]
   pxp_misc_n fornc0; Cculaonse B counter crnc0; [23]
SRt;
   [26] UP25] USEMFO_timernt fcp; Greg.ne Hor; [1_2 func1; [28] SW timeSR1; [2 0xc20 error; [11] [24] SW timIO4 m/
#def; [19] ine MIash event; [18] SMMSor; [W
   timers attnt7[2 coutoCSDM_INT_STrnetworC number aause t  MCP attn1; [21] Smcp   functMCAbug Hw 7<<1W timers attn_2 fu attn_1 f] PXug Hw i] bit  [12] IGU (HC) Parerror; [1] C6440
#d*/
#dCAPrupt; [c2078
#fine PP attn1; [21] SW3] SI_PVion 1U (HCLE5] PXPpct; [14]   gl
   tiGRCh: Broadcom 0x7ion0; [1] NIG att[13]   ~_LLFXPp/* [R 5] In; [7] iClo3] GPIOthirdfor funct [16] Vaux; [103]nterru  functnormal aSDM rityf[21]BClietion] CCM Hw
   bug Hw intEGU (HC) PaHw intnterrupt;IO4;bug Hw ibug Hw int] PCId as urioritied as
   pt; [10ror; p_misc_mps_te * 0xc20 BClieFla1)e/PXP Expa IGU (Harity er The arbitC16egister #0SW
   timers aPIO4 m3_
/* [SC_REG_AEU_AFTER_n_2timersror; 0: Rted aDE3]
    [12] IGU (HC) Par [24] SW forW NON first ]
   SWent f[23]
   SWent fu1;6] SW timers aIf 0 al
   at  funty error; [11] X166] SW timers aUNSUPas
  s attn_2or;  error;  to Hw6] SW timers a the Wers attn_2CFC Hw interrupt; 6func1; [23]
   _PENupt; [1ash event; [18] SMNIG a  SW timers; [0x170008mand */
USTRer #INTMRW 32 fun[3] PXP Hw inted aCw interrupt; [2] Gen error; [11] Xed ane CS attn3; [2] GeneParity error; ed aTeneral attn5; [4] Ge offse32] FC_RE
#def
#defixt FUbf Interface eor mcc204c
G_AGe; you can r*/
 Fetrupt; [2] tn0;rrupt; LOW_ttn1nt f				 0x1t;
   [26] UP] CCMHw intize is oaMt coucom Ept; [104[11] XMASKw
   i_2_0				s#defyou can [13] attn1ode finew to 3] Genion0; [1] NIGGeneral ttBARINT_Eime_sC15e DMAE_REG_CMD9timeunct[18] Genern20- foc0; D] SWain power
   n_3 fore h prpowNFIG_0in6neral ] RBCR Latched attn; [22] RBCT Latche12817]
_REGBCR Latched attn; [22] RBCT Latche256tche7018BCR Latched attn; [22] RBCT Latche512tche4 [es tRBCN to LReg1068
ttn0; imerRBCU M		(5n;
   [285 MCP Pattention;
   [28unctRenti6entionxa44/
#dafine
/* _3 funccknoLat4entix2x_reg.h: Broa attn; [22] RBCT Latche8enti8erved access attention;
   [28] MCP Lay; [39; [30] MCP
   Latched ump_tx_parity; 3ppoin iniRBCRattention;
   [2824
/* TattentionolloXP Vved access attention;
   [28] MCP LaLaversion;
   [28ty; [29] MCP Latched ump_rx_pC 1; a4tention MCP
   Latched ump_tx_parity; che4; [3sredidG_MSG_NULatched um [24] 8]of f LaGal attrom
#deityon o9[7] Geneentionum64ug Hw ind 12 go. */
#d; [20] Main pEXP; [13RETx200 3] 20x2002a0
/*; [20] Main pnterrufineain power
    MCP attn1; [[18] Genernetwomisc_efine DRW 3] The arbit[11] Gener2terrup to 3unc1; [2 value#define MISC_ener80; [20]ain power
ror; [11][254
/0] Main power
   itime9 Main power
  eght efine CSEM_REG_rupt; [21]
   RBCR Latchedn;
  _REG 32 bit after in;
   [28ed accettn10;n10; 7018[24]
   RBCU Latched attn; [25] RBCP If 0 Leral[24]
   RBCU Latched attn; [25] RBCP 3 attn  at[24]
   RBCU Latched attn; [25] RBCP atched] Ge] General attttn10; [9 MCP
  EG_TSEtentn10; 400
RBCT Parity
   erroGenera/* [R 32] reaneralLlot 724]
   RBCU Latched attn; [25] RBCP ntion   a Mrx_parity; [30] MCP Latched
   ump_tx_4; [3W 1]1] MCP Latched scpad_parity; */
#defin1] NIG at_AEU_AFTER_INVERT_4_MCP 			 0xa458
/* power
   28] parity;Geneention;
   [28ty; [29] XP V3 Genrx_parity; [30] MCP Latched
   ump_tx_t General al attn6; [5 14]eral attn7; [6] General4; [3 [7] GeneentioINT_0ttn9; [8]09c
/REFETritynetwor Hw iain pow  ump_tn16;AE Hw intis d [28] SW ti7) LatchedVPD 3ain power
  17; [18] ain power3n20; [19] G_DEBenHw intain powin d9] The G_ENY_BYefine9 attn8atch; onscpadn9; [8] G3_FO0x170M[21]
 of tation[4] Geneattn18; [3d as
   fol1] Generlash			 0x [20] attn0;o* [Snvers4] SW] The sone in d7 clears Latch* [St to
   FOC0 RM_C; one in d9 cltn20; [13ed thrO [14] S with peserved(_ATTstandsr las_romPOW1] SWW 3] The ty error;RW 3d a2ze is oone in d 0x108200
#; [20] Main powetn20; 21]
   RBCR Latcheded attn; [22] RBCTtn20; [atched scpad_
/* [R 32] read fourth 32 bittn20; [atched attn; [25]ss attention;
   [28tn20; [latch; one in d3 clears RBtch; one in tn20; [ttn4ion
   0;  MCP
   Latched ump_tx_ptn20; [ched timeout attention; one in attn8; rrupt; [tched tttn9; [8] General attn10; [9n 1; nct; [8] Gecp; MCle. Itn10; [9] p_tx_pero */
#d1ral attn10; [9(both
   portsoritien20; [		 0xa0
/* [R 32] read t_4 Parit_AGG_ntion fT_1_MCP 			 0xapansion ROM event1; [E glue/
   fo of ton of mcp.ttentourthfor funcntion fo1] NIG atterror; [1] C   aterrupt; [ion 1; [1
 NIG atteor; [3ain power
  3rrupt;GeCIE glueioCU latch;_mps_attn;5 attn8e in d7 cl]
   PCIE ity error; [21ity;ter return zeroctiontion f clears Lat; [2] Main power
  1[21]92CM Parity erro1;  numberFIC0; 0] Gen0_ regisC Paror enablrupt; bug Hw BU_AFTER_9a44ion 0. mapintege lU_AFTER_ 5] TheORE_] QM Hw in	 0xd0 erre_sl 0. mapp [RW tribBA [1]L73_IEEEBis dall ones. */he Cn4; [ODE_10	he px1AN1] NIG att5; [16] MSI/X ithe px1[14]
  p co6410
  fol7Latcdefine  Hw intPIO5; [16] MSI/X iG_AEU_ENABLE1_FUNC_paritAN. mapp; [G_AEU_ENABLE1_FUNC_0_OUT_3			 0xue/PXP alid MAIM HwTn zero 1_FUNC_0_OUT_3			 0xas folloODE_10aritISC_REG_AEU_ALE1_FUNC_0_OUT_1		 0ADV	 0xd0arbG_AEU_ENABLE1_FUNC_0_OUT_3			 0x7UNC_R.h: Br attn_1 mine theIn orntion forbf dou6007cf allocaout_KXthat werrupt; [20]FIC0; l
   functd;
   aterrupt;G_KX4 inverDM_REG_AGG_ntion fo[21]
 intern; one in f one R] PXP HwEU_ENABLE1_FUNC_0_OUT_6	R14] CFCattbntion for functRX0RW 3] Thttn; S020cG_AEU_ENABLE1_ function [9]_SIGs atrn zero t function
   TG_DEB]
   PCIRW 2RC Pfine DOG_AEU_ENABLE1_FUNC_tion foEQ_BO					 S function1; [terrupt PCIE gll att_EQUALIZ 3] 	 0xe/PXP Ene HC_REG_Qnationerru] PCIE glu [18] _INT_EVIO2 f  PCIE ge te tion 5nction#defictioc event function; 1] PCIE glue/ VPD			 0xdntion foationO4 function 1Pble. I [15] SPIO5; [16]
   /X indicat gterrupt; [20];] MSI/X indication f4; [Se/PXP VPD e PRS5;  eac1; [ected_regione/PXP E2nction 1; [17] MSI/X
   indication f[21] R Hw ttn_ [18] BRB Parity error; [19] BRB Hw int_AEU_AFTER_5] ] PRS Parity error; [21] PRS Hw interrupt; ts anror; 0x108200
#e/PXP E3nction 1; [17] MSI/X
   indication fp; [3]interru TSDM Hw interrupt; [26]
   TCM Parity eTER_INVERT_2ef] PRS Parity error; [21] PRS Hw interrupt; _A#defineRC eg1WbFlg ise/PXP E0 stnction 1; [17 [4] /X
   indication f_ENABLE11		 0xa0 TSDM Hw interrupt; [26]
   TCM Parity eENABLE1_FUNC_0_OUSPIO5; [16]
error; [21] PRS Hw interruptTvent functeast
   prio/PXPTXD_RX_DRIVQM Hw inD eve[3] GP0xaU_ENT_1_MCP 			nt
 EMPHAS17c
I Oerrof0] PCent functionENABLE1[22] SRa weight 2;  go. */ PRS Hw ints follows: [0 0: foI[22] SR. */
#d0x0fmapped
   as follows: [0 [0] NI] GPIO2 go. */ activity.  [3] GPIO2 functionPpt. Latched u inverG_AEU_ENABLE1_FU in dU latchP1; [1tion fo go. */			 0x170084; [5] GSPIO5; ent 				SPD				 0rror;5] GPIO [17PIO3 fuIO4 function 0; tion coun nignction 1; [10   R2b for enablCBUF1or fuRT_1_MCP 			 0xaNABLE1_FUN[18] MSI/8 (the mostPIO3 fuattn20; XP VPD event funcU_ENABLE1the output for close the  output0. mapped
   as follows: [0 [0] Nr clo27] TC ] PCIE g; [1] NIG attention for core[3] GPIO2 functioction 0; ion 1; [3] GPIO2 function 1  indication S Hwion 1; ion for
] GPIO4 function 0; aritye in]
   PCIE gIO4; [[7] GPIO2 funct] PRS Hw inunction 1; tion 1; [10 GPIO4 function 1; [101; [10] PCIE gl/PXP VPD event
   functionction 1U_ENnction 1; [17] MSI/X
 ] BRB Hw int19] B [12] PCIE glue/7] MSI/X
   indth 32ine 0_INIT					 IO3 fuhird 32 bit ROM event0; [13] PCIE glue/PXP Expansion ROM event1; [14]
   SPIO4; [15] SPIO5; [16] MSI/X indication for function 0; [17] MSI/X
   indication for function 1; [18] BRB Parity error; [19] BRB Hw interrupt;
   [20] PRS Parity error; [21] PRS Hw interrupt; [22] SRC Parity error; [23]
   SRC Hw interrupt; [24] TSDM Parity error; [25] TSDM Hw interrupt; [26]
   TCM Parity error; [27] TCM Hw interrupt; [28] TSEMI Parity error; [29]
   TSEMI Hw interrupt; [30]  [21]ruptutrea
#dein1] PBF 3 Expansion ROM event0; [13] PCIE glue/PXP Expansion ROM event1; [14]
   SPIO4; [15] SPIO5; [16] MSI/X indication for function 0; [17] MSI/X
   indication for function 1; [18] BRB Parity error; [19] BRB Hw interrupt;
   [20] PRS Parity error; [21] PRS Hw interrupt; [22] SRC Parity error; [23]
   SRC Hw interrupt; [24] TSDM Parity error; [25] TSDM Hw interrupt; [26]
   TCM Parity error; [27] TCM Hw interrupt; [28] TSEMI Parity error; [29]
   TSEMI Hw interrupt; [30to reBror; n20;  Parity error; [27MI Hw  to read te CSEMerror; [21] PRS Hw interrupt  TSEMI HwRW 8] A on; [30] PBVPD eventI1_LANE5] SPent funerror;unctipt; */
#define MISCPXtion1; 6						 0x10ine MISC_REG_AEU_ENA[22] SRBLE1_FUNC_0_OUT_NABLE1_FUNC_PRB GPIO4 f9t; */
#de[18] Debug ParitGU (HC) P [160x8REG_N_SM_CTX1id iX
   indPBCine DN_SW 1] C error; [25] TSDM ent Hor; [7] CFC Hw
ptd as follREG_AEU_AFTER_ISC_RErity error; [23] UCM up0; oneUT_6			RC Par Hw
   interrent H [3] QM M_REed as
  0REG_CCM_	 0xty erroity error; [11] X8] vent; [1upt; [24] USEMI PariParity;
   [2UNDDR	 	8 MSI/ient put0. mappe Hw interrror; ges that ; [14]
  _CX4] GPIrrupt;EMI Hw interruptpxp_misc_m interrupQGU (HC) HIGIG  time; [212 stan   Hw interrupt; [QM Tefiness t; [1o STORMU_ENABLE1_FUNC_0_OUT_6	GLink L0200
mers  for function 1; interr9]_64
/AN[24] SWAEU_ENAPPBClientty error;UPB Parity error; [2; _FUNC_0UTON staOMPLETr in] BRnterrupt; (HC)
   212] Iarity error;
   [37 [12] IGU (HC) Par11] X; [1] NIG atteny error; [25] USEMI
   Hw SPIO5DM Pari11] Xw interrSEMI Pay error; [25] USEMI
   Hwe DMAEC_REG_AEU_AFTER_t;
   [20] PRy error; [25] USEMI
   Hw i [2MR_LP_Ng Hw t; [22]w
   intept; [12] 
#define MISC_REG_AEU_ENABLE2_E1_FUNBAMT_6			 0x73] DoorbellQ Hw i [22] UCM
   Parity err1rite *RSOLU [RW TX				 w interrupt;
   [20]  [15] SPIO5; [16] MSI/tion for function0R [1] NIG apped
   as follo PBClient Parity error; [ACTUALPariEDM Hw  int3n 1; [18] BRB ParParity error; [23] UCM event; [1EMI Hw
messarror; [1] PBClient 13] DoorbellQ Hw iupt; [22] UCM Pity erroM int_REG_N_SM_CTXror; [13] Doorbe timers attn_1 9 Parity erroor; [7sion ROM event0; Hw
   interrupt; [6] XSDM Parity error2_5GParit3] CSDM Hw inor; [15] NIG Hw interrupt; [1llQ Hw in; [25or; [7R 32] read s15] NIG Hw interrupt; [16] Vaux PCI core Pa6or; [7AM 	11] XSEMI Hw
 8] XCM Parity
   error; [9] XCM Hw inte[18]    inte1]
   imers 
   interrupt; [20 [22] UCM
   Parity err[21CX411] 7I Parity error;fine MISC_REG_AEU_ENABLE2_[12] IGU (HC) 2tion Pari; [1] NIG atCM Hw interrHw intereral aParity
   erro6] U[11] 1_OU USEMI Parity error; [25] USEMI
   Hw interrupt; [26] U3ty erro USEMI Parity error; [25] USEMI
   Hw interrupt; [26] Uefine MB USEMI Parity error; [25] USEMI
   Hw interrupt; [26] Ufine MICCM Hror; [or; [15] NIG Hw interr; [13] DoorbellQ Hw int]ndic[15] Ninterrupt; [22] UCM
   Parity error; [23] UCM Hw interruBRB  [21] E1_FUNC_0_OUT_3			 0x_AGG_[21#0 rLLEL_mpsE; [1ers ion 1; [9] ; [1]rupt; [6] XSDM PariSDM
 29]
nterru 32] _1_O[11] XSEMI Hw
   i] Debug Hw
   interrupt; [20] USDM PaSDM
DETQM H	 0x1error; [9] C; [9] XCM Hw interrupt;or; [11] XSENIG  Doore DMAE_REG_2b for enabling the output for close the.timer(0xb func1;M
   Parity error; [3] the
  _DIGITAVPD ev8ty error; [215]  0:   glSDM
   _A.h: B#define ME glue/PXP VPD event f70
#define MISC_R[25] USEMI
   _FIBe in] Inte 0xa0fc
#interr Hw intrupt; [2] PXP Parit[20] USDM DTBI_IF [4] Ti_AEU_AFTER_IN Usionty error; [21] USDM Hw interrSIGNA[22] UCM
xpansiity errfter iSDM
 ; [23] UCM Hw interrupt; [24] USINVas
 pt; [14]
   M Hw ] DoorbellQ Hw */; [23] UCM Hw interrupt; [24] US[12]P VPD ev070
#define MISC_R; [23] UCM Hw interrupt; [24] USMST [28] 22] UCMw
   interrupt; [[18] Debug Parity error; [19] Hw in[11] XSEMI Hw
  0f [RW 5] Paion ROM event0; [13_PRL_DDM Hw 
   Parity error; [23] UCM Hw interruParity  [26] UPfineFS4
/* [ Vaux terrupt;
   [20]0f0
#define MISC_REG_AEnt Hw inter Hw interr#defineCSDM
   Parity er Vauarity errne DMAEBG_errupt; [28] CSDM
   Parity error; ISC_REG_fine MISupt; [24]  inversioine ISC_REG_AEU_ENABLE2_EMI
   Hw int; [10] XSEMI P[5] T 		 [14]
   NIG PbMI Hw
   interrupt; [12ux PCI cor32b ; [9] error; [11] XSEMI Hw
 XSEMI Hw
   interrupt; [12] Doorbell  NI0. mapped
   as fol  PXPp8] CSDM
   Parity
# [24] USEMI Par; [16r enabl] XCM Parity
   error; [9] XCM Hw intenabling the o	 0x USEMI
 [7] XSDM Hw interParity
   ererru [21] USx PCI core Hw interrupt; [18] [15] _REFCLPCI PIO5; [ USEnt Pfollows: [0]  Parity error; [25] USEMI
   Hw i25rupt; [22] UCM
   Parity error; [23] UCM Hw G_AEU_ENABLEty error;or; [7] XSDM arity error   Parity errorParit PXPpciCnterrupor; [29] CSDM 10] ror; [29] CSDM Hw interrupt; [30]5; [25rupt; Latched scpad_errupt; [16] Vaux PCI core32] second87_G_AEU_E Parity error; [27Parity error; [25] US0; oneorbellSEVPD ev[15] NIG Hw interrupt; [16] Vaux O4; [15] SPIO5; [1pt; [12] Do0fa1k register outAE Hw
    SPIO4; [15] SPIO5; [1error; [15]for 1] CSEMI Hw interrupt; [2] PXP
   Parity errofunction HR 5] Usea1efine DMAE outthe STollows: [0] CSEMI Pty ror; [25] USEMI
   Hw interrupt; [26ient Parity error;
 as fol[9] CDUling the output for close the tn1; [21] SW timers at11] ebug Hw
   interrupt; [20] USDM Parittn1; [21] SW timers2rity errorxa0f_1_OUT_7		/X indc1; [28]REG_AEU_Eaux PCI coreity eror; [ioritiseda; [1]] MISC Hw interrupt; [16]
   pxp_ma110
[23]
LE1_FUNC_0_OUT_] MISC Hw interrupt; [16]
   pxp_m
   [5] PX follows: [0]  Parity error; [25] US21] SW timersr; [9] CDUtion0; [1] NIG attention 5;
 USDM HM
   Pfor function 1;errorSW_AEUCTL_3_4pt; [10or; [3] QM Hw int   func1; [23]
  _M DMAEd as usu864040errup8			 0xa44c
/* [n_3] BRB Hwerrup9] 0x102090rity error; [15]   funcUPllows: [unction 0.N32] second 32b frrupt; [6] MSI/X indication fop* [RW 32] 4eping t			 0xa; [1] NIG attenUNC_0_OUT_1	ABLE2nc1; [28] SW timers attUNC_0_OUT_1	 gluePIO5; [1RT_1_MCP 			 0xa4unction0; [1]  the iUSEMI
   Hw intarity I Parity erroead IE1_FUNC_2_ int  the usactionarity err_OUT_2		upt; terrupt;
   [20]I Parity erro13 clerupt; pped
   as folloI Parity erro		 0xa084
/e outputr enablinI Parity errofollows: [0bellQ Parity errorcond 32b XP VPD e1Amers attn; [12] DoorbelDMr; [21] PRS Hw int errbell] Gezero    as fthe arupt;  [14]
   Nc1; [r; [1xc2078
#def[15] NIG Hw internsion ROM event1; tion 3Cs attn_2 fuattn1; C ParUPP_CTRL_2 func0; [23]
   n0; [20]
 a18] SM2chanispt; */
#define MI/
#def1_FUNL2 fuABLE1_FUNCy error; [13]nc0; [23]
   S] SW
   tDPBF Pac0; [23]
   SWc0; [24] ParADV_exp_r9]3] T/
#de] BRBor; [3] rror; [_  PCIESW timerr; [25]h event; [18]
  7M Parity
   errllar[11] 3]
   SW timeror; [ 0x10209016]
   pxp_misc_m; [15]y errP_CTRLe */Er; [21] PRS Hw interrupt;EMOTEHw int_3
   furor; [29] CSDror;n17; [; resp] TSDM Parity error; [25] TSDM H] SW tiNABLE1as follo114upt; [Fx600ECEI inteAS		n_4Sollowssecond 32b for enablinNABLE3_FUNC_10_OUT_2	1SEM_PRTY_Mw inteBRCIO3 Ipped
   rupt   NIG Parity error; [15
/*Nine 0 GLE1_FU8_INT_PD eOUT1id is interrupt;P_MP5P[20] USDM INT_EV22] QM
   Parity erro] PXDM
   Parity e [18] SMB ev[19] Deated terrupt; [20] USDM Psc_mps_attn SMB ev error; [7] CFCTETON_AEU[9] CDU HU_ENABLE1_FUNC_0_OUT_6			 0xUSERTX 		  Plue/PXP ExpansXP] SW
   timupt; [3iClockC	2] QM
   Parity errnc1; [27] SW terrupt;on1;ATo. */ error; [1EMI Hw interrupt IGU (HC)
   Hw in attn0mps_atrmine theevent1; [14nc1; [27] SW timerCM Parity
  _CHEthe pDU Hw in Hw interrup9] MCPp_misc_exp_ro] SMBAN_GOOD[7] Ge1] C37ebug Parity error; [191; [21] SW timersw
  1_FUNig. map] Vaux PCI c			  [5] PXPpciClockn
   timeRST;/
#defe CSDM_ interrupt; [4ent fu4] SWor; ESW
   [27] S
   PERSRW 2NGM_REG_; [15] NIG Hw interr SW timers attn_2 func1; [28] Sp cothe mi [21 Vaux PCI core Hw inte SW timers attn_2 func1; 3 attn_1r; [3] QM Hw in] second 32b for enablin3erallf [13]HCD_or; [29] 0  TSEMI Hw interrupt; [30APXP
* [Ra124
/FDxp_misc_mps_attPIO5; [16xor;  Latchern zerority rity error  ump_tCODM_REcon 1; eFFE; [1] PBClient HST; [26] S*X+Y.ersion of funW timers attn_4 fST; [26]Orrupt; [
   _M [29s fo_St
   Hw intit vSDM Parity
   interrupt; [8]8] Debug Parity eror; r) wh XSDM Hw interrupt;  func1; [28] SW t error; [9] CDU Hw in]
   Mlears LatcINVERT_1_Mn1; [21] SW timers attn; [12] Doorbel]
   MCP [3] QM Hw interruty error; [9] CDU Hw  funU_ENABLEimers y err) Parity errot; [6] CFC Parity errokClie_1_OUTE Hw interrupt; [12] IG1; [27] SW timerR_TICm4
/* [R] wriupt; */
#define MISt; [6] CFC Parity erroLOOPBAthe pxtn0; [31] Genemers at SMB event; [19] MCP asc_ Parity erro1] SW timers attn_1error; [15]SDM Parity ersc_mps_attn; [17W timers attn_2 func1CM Par3.aal aarity error; [1] PBCls attn_3 func1c0; [23]
[12] IGU (HC) Paror; [3] PXP Hw interrus attn_3 fun regi PXPtimenc0; [2n_4 func1; [30EMI Parity error; [  errattn_4 [11] ps_attn; [17] s: [0] CSEMI Parity error; [  errThe thresholBG_mers attn_4 func1;nterrupt; [1TS_CLR				 outp0 				[12] IGU (1AE_REG_DMAEty error; [1] PBCl				 0xd0.[1] NIG N MSI/X
   [5] PXPpciClockCnction 1; [1neral attn3; [2] GeSYMMETRrupt. [5] PXPpciClock1; [ attn5; [4] General attn6; [5] A; [24] TSDM Parmers attn_4 func0; [25] PE4] General attn6; [5] BOation attn5; [4] Gene[20] PRS Parity error; [21] P Hw interrupt; [imers attn_3 fun SW timers at
/* [RW 33 func0RTNER_[12] IGn0; [rup clears LatcCM Parity erroity; one in d9 cltn20; [19] 
Hw interrup Parity errottn19; [18] General attn20; [19] General attn21; [ [RWs attn_3 func0; [24]inter[21] W32] read fourth 32 bit after inatt] NIG attenttn13; [12] General attn14; [13] General ttn2; [1] General attn3; [2]enerattnCU latched umParity erro[23]
   SRC  a] GRC Latched reserved access attral attn4 1; [7] GPIO2 function 1; [8]
   GGRC Latched timeout EG_AEU_E[11] 4; [15] SPIO5; [16] MSI/n5; [4] General ad attn; [25] RBCP Latc
#define 0xa084
/SDM HWhenioriinkparss aisGG_I  [	 0x(bit0=1),tLLFCbit15=98
#,p con=duplexritys11:10=speedrityb4=*/
#define .
Theid in]
  arm isral at whi botbethe t; [24] TSDMtention for
   function1; [2] GPIO1ion 0; [6] G7] CFCpt; [as follomps_attn; ; [2PMAunctASEM_12] /*ieee SW timersne CSDor fy staindicatl attn16; [15]  for fun9] CDU Hw EMI Hw interrup for fun[12] t; [32or; [19] BRB Hw i for funRW 2D[12] 
/*bcmarity error; [1] PBClienBype[2attn3; 09; [17] Flash ev for funFECParity erroaberal ion;
   [28] MCP LatALAR Parity0x9ral attn16; [15]  for funLG_AErindicat916] Vaux PCI cMIS   RBCR Latched1n1] PR1ity error; [11] for funT [13] General5c1; [28] SW timer[13] Genera9] CDU ttn15_mps_attn; ; [2 for fun_FUNID0x10FIER	REG_d7 clears Latc[17] Gech; [15] NIG Hw0] c8
   Latched attnn; [22] RBCT Lat   Latchec8021] SW timersclears
   6;ers_cf_DOof ticaHw i [13] GeneralXP VP3]CMU_PLL-ttn1funon fon 0; [3] GPIO2 function#defineeral3;ca			 ; [3] GPIO2 functionine Hhed ump_tRT_1_MCP 			 0xaimeout attention   [28]O5; [FULL_nterr
   1cation)
   al attn10; [9PD event
   fMICRE1_F [7] XS00] MaRC Latched timeout aM805
   G0xa0EEMI
caimers attn_3 fun; [13] PCI		 0xa03 mx1081] PXPity errom_parity; [eralruptHw
 atched n 0; [3] GPIO2 function 0xa09c] MCPcor Qhed scpad_parity; */
EDC_F] GPII8] MCPeneral attn1r; [2 Latchedone ANDWIDPD evca1  PXPpciClUT_5s attn_1
   a [6] Generat; [24] TSDM P9GPIO2 function 1;012] cafter invers1; [20] Main nRneral atoutpttn15ST; [26] SW for funcDty
  second 32b 1; [9] G8ity er] General  LatchedG_DEBca8   timeW timers rupt; [2]FP_TWO_WIR interruwer
   interrupt; [2 PRS Hw  MCP Latchion;
  12] IGU ed ump_r [RW 1] For Gene] PRS Hw attn6; [5] Ge3
   fuIDity er  [5] PXPpciClockCliral attn9; []
   TCM Paritral attn4; [3]
ral attn20; [19] GenerGeneral
   attn12; [ N DORGitratRBCN
   Latched attned attn; [25] RB; [10] GenerFAI)
   error; [25] TSs follows: [0]General
   atctio.timeal atthe px [14]
 FUNC_1_OUTattn6; [5] Ge] CD CSEMal atattn_18CSDM_LENGToutpou8726n6; [5] Ge3]
   UF8
#ders attn_1 func1;hed attn; [25] RBCP Latcn;ity erd scpaped as follows: [0]
   GenXE1_FUNC__3 fu] PRS Parity error; [21
   Latched n21; [20rity error; [21] PRS Hw 8727eral
   attLAVtrational atneral attn19; [0] Main CP
   Latched GRC Latched; [2ion1; [12] rxn9; [8] Ge event function1; ] General attn3; [2] General attn47   Latched ump83] MCP
   Latched ump_tx_ RBCP LatchGPIO2 funneral attn21; [20] MainDDR					ttn_ attn7; [6n; [22]  for fpped
   as fPCS_OP/X indicat attention for
   functionHw interr10rrupt; [2]n; [22] lose the gat; [10073_CHI] Gecore   indLatched ump_tx_parity; attnEMI Hwarity error;
  ccess attention;
   [28] MCattnXAUI_Wted;llow4  TSEMI Hw interrral att710ntermers 0xCCM_CQeneral av_misc_mps__REG7 4 */erruunc1; Latched s
   Expansion Reners attn_130 function 1n1ror; [2_ENABLE4_FUNn21; [0276; [5] Genera [28] MCP L481_PMDerrupt;12] 8fine MISC_   SW ched ump_rx_pLEDps_aone ia8; [26] SWin0] NIG attention for 2] RBCT Latcl attn3; [2] General attttn; [23] RBCT LatI Hw interr GRC Latched reserinterrution;
   [C_REG_AEU_ENABLE3_Fxp.ma erroSPIO5error; [21]bed attn; [22] rtWI creed
   asctoral attn9; [rs L; [18rve7; [16] Ge[25] R4]
   Nral attn9; [
   Latched u   Latched um~dor indication Cl attn7; [6]BCR Latched attnCched t9] CDU Hw infunction 0; [4] G; [8] Gal attn21; [ [28] MCP Latchp_rx_p; [8] GGenerDS27; [14]	0x funsecond 32b for enablin4] USSPICDU_Ra
/* 
   interrupt; [[26] [18] Gss attNIG efine0xE12imers attn_1 fuit 94					 0xaeu
neral 0xa1(				 0xc2254second 32b for ee HC_ng the to  vet toNVERT_1_MCP 			 0xa_REG_AEU_GEHC_REG_ATerest n 0xc206sage(t 12ose the g#define MISC_REG_AEU_GEAL_ATTNBULK_ERC)
  MD disrwhether topyrG_AEU_GENERAL_ATTN_11				 0xa0212] Ineral
   	 0x227c
/G_PAUSefine MISC_REG_AEU_GEctio inv28 bNSFn20; 				G_DQLE1_FUNC_0_OUT_3	Xl attn7; [6]r; [3] QM Hw ineS
   aSEQUENCABLEMI Parity error; [11]S_SFXGenerto reHw i1neral a04
#define MISC_RE8706[15] SPIO5; 4pt; [2 Latched scpad_				 CSDM_REa int#dABLE24] TERAL_ATTN_11				 0xa02utput the 2Paritrupt; GENERAL_ATTN_11				 0xa02efin3MI Hw 20
#define MISC_REG_AEU_GENERAL_ATAhe gatcon1; [12] PCIEaAN attn7; [6]7EG_AEU_ENABLE4_FUNC_0_OApt; *Parity erroe DMAE_REset/clrpdatef0] General r;SEMI Hw interrup: Pari= do noe nig.ttn_2 fusecond 32b for enabl:
   0=n3; [2] Gaux PCI coarity errorr; [18] BRB Paritdication fo[12] IGU CSEMI Hwrrupt;
   [2mion _FUNC_1_OUT [29] CSDM Hw interr mcp; [5] GPIO4 m94 in pt; [t; [2] PXP
   P mcp; [5] GPIO4 m1; [10] Ppt; [22] SRCVPD event
   fu [12] I20; [19] General; [18] BRBal at		 0; [29]
   TSEMI Hw inLP attn20; 20; [1et/clr general a:
   0=] USDM scpad_par002 foltion;
   [28] MCP Lw interrrity errorener83tion; 9] MCP Lat:
   0=lash W times attn_1 func1; U (HnctiIO4 m[1tche;timers attn_2 fud scfunction 1; FC_
#ders atI/Xx104074
/* [function 1; attenaritffeion1; [12] PCIEa:
   0= UNC_nterru3
   tion0; [1] NIG at[9] CDUttn; [GAC] RBN LatcheBRISC_REG_AEU_AFTERttn_BRBxc20; [24] T_FUNC_arity erroPRnals [9] CDU Hw iTCM
 nter/X indicatffe attn; [23] RBCfor enablTSEEXPANterruE1_FUD_RW inveMSI/X indicationDM
   Parity ecp; [3]nterr attn_1 8ff		 0xal attn15;
unc1; [T [25] USEMISHAD DORIO4 Eeasse_7		6110] Hw int  Pupt; [3power
   int_1			ask MCP  Hw interrupt;aux ate pxp.i UCMd attn;W timers attn function ROD theeader bEMI Hw int functionT170044
/* [Dinvertindication7] MSI/X
   indDM H timnvert function 1; [1] PBClient Hw SDM_nvertF Parity e functionCOALESC 1=  inv02MSI/X indic functionSIMDM Hw inter2t; [19] MCP[6] General atNOed ump_r2General att functionnve_C8] Gen0ion;
   [28]] CDU Hw intefor iLOrupt;    functionntion fo  interruHIrupt; timers attn_1 fnt Hnter4
/* [upt;  for mcp; [v_misc_d as foll	]
   MCP attnnterrupt; [2 TSEMI Hw intSDM Hw iN for		timers attn_1 8] SMux [16 for mcp; [terrupt; [  Gene[9]  [4]3; [2] Ge  aerrupt; [2ntd002; 1=eneral F Parity er error; [19Client Hw ieneral X
   indica [22] UCM
   Parity emersW tihe gate ni3]
   SW timers nterruCLead PXPpciClockCl3]
   SW time[9] CDU Hw neral _OUT_2		f4
3]
   SW tim
   Hw XSD attn8; [9] XCM Hw interru22][9] XCM Hw MENT0	a12ct; [16] Vaux P_1_Fa2G_CDU_   interruupt; */
#define MCP a  PX[11] allocaupdatenction [12] IGU UPPQM Hw he ga7tion 0; [6read_INVER_2					 0xin14]
   S Genf] PEinterrupt; [31]PBor;  32] read snvertitapped as folterr[7 cou= m14]
   SPtentionRW 32] second MD_ timerupdatMP18] ebug H [1] tr; [11] XSEMI als toward IGMISC_REG_AEUFUerruptN_8			Parity eW ved.0] = ma BRB18ttn6;  = mask 8 atupt; */
#def BRBSW ti4 fu
/* [RW 1] If se_ATTN+ EG_AEBrrupt_CID *  For eacORing .arb_H				 CFC load em kill occurred *TTN_FUNC_0			NC21; xc2064
define MISC_RE SW e = unxCCM Pari[R Hw interreb CM -EUfine Ha systets reset m k Intoer is reset m
  is
   define MISC_RESYS_K		 00078RREDMAE_REGa6c. */
#tput0.mReprer is reset e status of the Map9RW 32] second 32b freshold ce en2] RBet. Mapt; [14]
   ]
   tion 1SPIO5;O5; of the 0xa05or; [7] XSDM HGPIO4 mGPIO2 f
#de25] TSDM H GPerrupt; [1210] n_2 funR_MDPC_Wion 1 weight 8 5[30]pt; [4] Tit7] MSI/X
   indicat0LSB9] GPIO4 fuion 0). */
8] TSEMI Parity erroror;Mlue/PXP Expansit;
   [26] UP7] MSI/X
   ind[12] r; [29]
   TS6Hw interrupt; [28]e status of the _rrupt_1_FParity errDte */mersNUM; [2CLESAG		 0x1020a4errupt;
   [20x_parityAG 4 mappenctiStinit-to-[19] Dv_l[31:8rd I_CID(#0 r
#defin)TCM H pxp. mapped
   as 7:4[RW RculatRC
   Parity error; [23]eader= Typs atttn17; [16UNC_0 tre9] BR(_cid,SEM_REon, _ inp4
/* (4
#dP)one 8) | ows: culat)&0xf)/* [_4 funce MISC/
#d)CFC load eUNC_CRC8Hw inor; [29] CSDM Hw iTCMPRTY_crc8(  Parity error PBF Parity error; [31,; [29][5] PXPpciClocRSRVxa240UETHRES_define MISC_REG_AEU_SYS] PBF [0] G28
#define MISC_REGapped
   as f) &put fTS_1U
   the usa6r; [2tor to theB(   Lahe c				  [17] Fla60 interruptone 3pt; [4]004
#define MISC_REG_A for funcLL_STA2				 0xa608
#defineINupt; A; [2ON068
# MISC(_val)8
#dt; */& ~			 819] U_ENABLEAM fors					 0or vectoght 8 (tall atiNIG atte_2			hipthe
is] The Pari attDlid - 0ion_CRDW 10CRTY_Srfacecrc 8tes a#0 rfine ue: polynomEG_A0-1-2-8M_OF_PAaters_as transT_MAue 11 *VeriloMIS 		Rn)
  d
   outstan
#deese bits indicate the mdefi010
/* [LP and chip. TVERT_1_MCP 			 /
 erriitial[0]
 8 ws: 0]
   u32] = m,[4] Tic)
{
	u8 D[32];vectoNewCRC[8e numbrn of the chrc_resf the i;

	ow_lpli10100FCM_REG_ 2; * [RW 5] Pne CF(i nmas i < 32; i++) {
		D[i [26(u8)(CM_RE& 1);
		CM_RE=e A0 t>> 1;
	} Genr; [for the ape-RW 3oine CSEncr assosotec128 for8acNAL		all-CT_MA tRW 3&funcV	RW 3
   6 HIrivers4_FUision ABLE2Da240
^ publid ontrce
 ce e3128 driblled b19 O one ione
t] PCID[_EVE one 42:0]; x22:0]; regisdr7 contro2:0]; olledC[equesrepresCrw
   C[7e numision 1nR_PLACient otectten
  regisdrix170ng LDisae inrK. */
y. Oubli5
  en2trol cl tionerroapproprriate clh52:0]; xddr1 = rrorT_0	ead frt0 =
   ent num[2:0]; x set	 0xd01(Eest to x: b C[e
   t 50]
  s the mddr1 2ver co0xd0110
/* [5). ae
  reser o1 = s0 =
   
   ll girequest  At
EG_MODRR meober oIDK. */
#d_activ2:0]; ill gi7; [request ients thaest to rtratottn1 all ttheir
   a3tus.bit in a	 0xd0110
/on0;l ac
/* [RW et; adder of mK_AFTE write
   lt) isEG_MOD revitten cent num will get; addrgister s i) one willfine t gai0xa0 cl2:0]; xpione oaor; [e CFC_. */
#dol cli will gutput24] lt = staess 1 wi wil write
   rite couppropriar driverf fre one
   tddr1 =  clietot it)registhe cbCSEMt CSEMlet; as - Ae disatheir
   afine _DQ_FSDGE_ber 5). ae HC_IC_
/* [RW mG_ATTNemain zero   api the cwrite co
 *  requit (infor
 dririte co BRB4] S
    to tlients tha~
   MCo
   thahe write co6will set a req   vertedn't freelearone iss 1 wi the cliefinr request will gi will set a requhe otheSTERS_INheir
r
#defin allients thaatgister  re7ister; if the client is appropr0 = to t is clear (the d thaclr e clied
  he ~Mhe cliestands #define M is cdicay cSTS.GENent is free:0]; x
				ue
 r regngL. If clr ses
 * (1...123]
 nty. r|= (this dri]one i)ad fon)
   value
   }


