#define DRV_NAME "advansys"
#define ASC_VERSION "3.4"	/* AdvanSys Driver Version */

/*
 * advansys.c - Linux Host Driver for AdvanSys SCSI Adapters
 *
 * Copyright (c) 1995-2000 Advanced System Products, Inc.
 * Copyright (c) 2000-2001 ConnectCom Solutions, Inc.
 * Copyright (c) 2007 Matthew Wilcox <matthew@wil.cx>
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * As of March 8, 2000 Advanced System Products, Inc. (AdvanSys)
 * changed its name to ConnectCom Solutions, Inc.
 * On June 18, 2001 Initio Corp. acquired ConnectCom's SCSI assets
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/isa.h>
#include <linux/eisa.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>

#include <asm/io.h>
#include <asm/system.h>
#include <asm/dma.h>

#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_tcq.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>

/* FIXME:
 *
 *  1. Although all of the necessary command mapping places have the
 *     appropriate dma_map.. APIs, the driver still processes its internal
 *     queue using bus_to_virt() and virt_to_bus() which are illegal under
 *     the API.  The entire queue processing structure will need to be
 *     altered to fix this.
 *  2. Need to add memory mapping workaround. Test the memory mapping.
 *     If it doesn't work revert to I/O port access. Can a test be done
 *     safely?
 *  3. Handle an interrupt not working. Keep an interrupt counter in
 *     the interrupt handler. In the timeout function if the interrupt
 *     has not occurred then print a message and run in polled mode.
 *  4. Need to add support for target mode commands, cf. CAM XPT.
 *  5. check DMA mapping functions for failure
 *  6. Use scsi_transport_spi
 *  7. advansys_info is not safe against multiple simultaneous callers
 *  8. Add module_param to override ISA/VLB ioport array
 */
#warning this driver is still not properly converted to the DMA API

/* Enable driver /proc statistics. */
#define ADVANSYS_STATS

/* Enable driver tracing. */
#undef ADVANSYS_DEBUG

/*
 * Portable Data Types
 *
 * Any instance where a 32-bit long or pointer type is assumed
 * for precision or HW defined structures, the following define
 * types must be used. In Linux the char, short, and int types
 * are all consistent at 8, 16, and 32 bits respectively. Pointers
 * and long types are 64 bits on Alpha and UltraSPARC.
 */
#define ASC_PADDR __u32		/* Physical/Bus address data type. */
#define ASC_VADDR __u32		/* Virtual address data type. */
#define ASC_DCNT  __u32		/* Unsigned Data count type. */
#define ASC_SDCNT __s32		/* Signed Data count type. */

typedef unsigned char uchar;

#ifndef TRUE
#define TRUE     (1)
#endif
#ifndef FALSE
#define FALSE    (0)
#endif

#define ERR      (-1)
#define UW_ERR   (uint)(0xFFFF)
#define isodd_word(val)   ((((uint)val) & (uint)0x0001) != 0)

#define PCI_VENDOR_ID_ASP		0x10cd
#define PCI_DEVICE_ID_ASP_1200A		0x1100
#define PCI_DEVICE_ID_ASP_ABP940	0x1200
#define PCI_DEVICE_ID_ASP_ABP940U	0x1300
#define PCI_DEVICE_ID_ASP_ABP940UW	0x2300
#define PCI_DEVICE_ID_38C0800_REV1	0x2500
#define PCI_DEVICE_ID_38C1600_REV1	0x2700

/*
 * Enable CC_VERY_LONG_SG_LIST to support up to 64K element SG lists.
 * The SRB structure will have to be changed and the ASC_SRB2SCSIQ()
 * macro re-defined to be able to obtain a ASC_SCSI_Q pointer from the
 * SRB structure.
 */
#define CC_VERY_LONG_SG_LIST 0
#define ASC_SRB2SCSIQ(srb_ptr)  (srb_ptr)

#define PortAddr                 unsigned int	/* port address size  */
#define inp(port)                inb(port)
#define outp(port, byte)         outb((byte), (port))

#define inpw(port)               inw(port)
#define outpw(port, word)        outw((word), (port))

#define ASC_MAX_SG_QUEUE    7
#define ASC_MAX_SG_LIST     255

#define ASC_CS_TYPE  unsigned short

#define ASC_IS_ISA          (0x0001)
#define ASC_IS_ISAPNP       (0x0081)
#define ASC_IS_EISA         (0x0002)
#define ASC_IS_PCI          (0x0004)
#define ASC_IS_PCI_ULTRA    (0x0104)
#define ASC_IS_PCMCIA       (0x0008)
#define ASC_IS_MCA          (0x0020)
#define ASC_IS_VL           (0x0040)
#define ASC_IS_WIDESCSI_16  (0x0100)
#define ASC_IS_WIDESCSI_32  (0x0200)
#define ASC_IS_BIG_ENDIAN   (0x8000)

#define ASC_CHIP_MIN_VER_VL      (0x01)
#define ASC_CHIP_MAX_VER_VL      (0x07)
#define ASC_CHIP_MIN_VER_PCI     (0x09)
#define ASC_CHIP_MAX_VER_PCI     (0x0F)
#define ASC_CHIP_VER_PCI_BIT     (0x08)
#define ASC_CHIP_MIN_VER_ISA     (0x11)
#define ASC_CHIP_MIN_VER_ISA_PNP (0x21)
#define ASC_CHIP_MAX_VER_ISA     (0x27)
#define ASC_CHIP_VER_ISA_BIT     (0x30)
#define ASC_CHIP_VER_ISAPNP_BIT  (0x20)
#define ASC_CHIP_VER_ASYN_BUG    (0x21)
#define ASC_CHIP_VER_PCI             0x08
#define ASC_CHIP_VER_PCI_ULTRA_3150  (ASC_CHIP_VER_PCI | 0x02)
#define ASC_CHIP_VER_PCI_ULTRA_3050  (ASC_CHIP_VER_PCI | 0x03)
#define ASC_CHIP_MIN_VER_EISA (0x41)
#define ASC_CHIP_MAX_VER_EISA (0x47)
#define ASC_CHIP_VER_EISA_BIT (0x40)
#define ASC_CHIP_LATEST_VER_EISA   ((ASC_CHIP_MIN_VER_EISA - 1) + 3)
#define ASC_MAX_VL_DMA_COUNT    (0x07FFFFFFL)
#define ASC_MAX_PCI_DMA_COUNT   (0xFFFFFFFFL)
#define ASC_MAX_ISA_DMA_COUNT   (0x00FFFFFFL)

#define ASC_SCSI_ID_BITS  3
#define ASC_SCSI_TIX_TYPE     uchar
#define ASC_ALL_DEVICE_BIT_SET  0xFF
#define ASC_SCSI_BIT_ID_TYPE  uchar
#define ASC_MAX_TID       7
#define ASC_MAX_LUN       7
#define ASC_SCSI_WIDTH_BIT_SET  0xFF
#define ASC_MAX_SENSE_LEN   32
#define ASC_MIN_SENSE_LEN   14
#define ASC_SCSI_RESET_HOLD_TIME_US  60

/*
 * Narrow boards only support 12-byte commands, while wide boards
 * extend to 16-byte commands.
 */
#define ASC_MAX_CDB_LEN     12
#define ADV_MAX_CDB_LEN     16

#define MS_SDTR_LEN    0x03
#define MS_WDTR_LEN    0x02

#define ASC_SG_LIST_PER_Q   7
#define QS_FREE        0x00
#define QS_READY       0x01
#define QS_DISC1       0x02
#define QS_DISC2       0x04
#define QS_BUSY        0x08
#define QS_ABORTED     0x40
#define QS_DONE        0x80
#define QC_NO_CALLBACK   0x01
#define QC_SG_SWAP_QUEUE 0x02
#define QC_SG_HEAD       0x04
#define QC_DATA_IN       0x08
#define QC_DATA_OUT      0x10
#define QC_URGENT        0x20
#define QC_MSG_OUT       0x40
#define QC_REQ_SENSE     0x80
#define QCSG_SG_XFER_LIST  0x02
#define QCSG_SG_XFER_MORE  0x04
#define QCSG_SG_XFER_END   0x08
#define QD_IN_PROGRESS       0x00
#define QD_NO_ERROR          0x01
#define QD_ABORTED_BY_HOST   0x02
#define QD_WITH_ERROR        0x04
#define QD_INVALID_REQUEST   0x80
#define QD_INVALID_HOST_NUM  0x81
#define QD_INVALID_DEVICE    0x82
#define QD_ERR_INTERNAL      0xFF
#define QHSTA_NO_ERROR               0x00
#define QHSTA_M_SEL_TIMEOUT          0x11
#define QHSTA_M_DATA_OVER_RUN        0x12
#define QHSTA_M_DATA_UNDER_RUN       0x12
#define QHSTA_M_UNEXPECTED_BUS_FREE  0x13
#define QHSTA_M_BAD_BUS_PHASE_SEQ    0x14
#define QHSTA_D_QDONE_SG_LIST_CORRUPTED 0x21
#define QHSTA_D_ASC_DVC_ERROR_CODE_SET  0x22
#define QHSTA_D_HOST_ABORT_FAILED       0x23
#define QHSTA_D_EXE_SCSI_Q_FAILED       0x24
#define QHSTA_D_EXE_SCSI_Q_BUSY_TIMEOUT 0x25
#define QHSTA_D_ASPI_NO_BUF_POOL        0x26
#define QHSTA_M_WTM_TIMEOUT         0x41
#define QHSTA_M_BAD_CMPL_STATUS_IN  0x42
#define QHSTA_M_NO_AUTO_REQ_SENSE   0x43
#define QHSTA_M_AUTO_REQ_SENSE_FAIL 0x44
#define QHSTA_M_TARGET_STATUS_BUSY  0x45
#define QHSTA_M_BAD_TAG_CODE        0x46
#define QHSTA_M_BAD_QUEUE_FULL_OR_BUSY  0x47
#define QHSTA_M_HUNG_REQ_SCSI_BUS_RESET 0x48
#define QHSTA_D_LRAM_CMP_ERROR        0x81
#define QHSTA_M_MICRO_CODE_ERROR_HALT 0xA1
#define ASC_FLAG_SCSIQ_REQ        0x01
#define ASC_FLAG_BIOS_SCSIQ_REQ   0x02
#define ASC_FLAG_BIOS_ASYNC_IO    0x04
#define ASC_FLAG_SRB_LINEAR_ADDR  0x08
#define ASC_FLAG_WIN16            0x10
#define ASC_FLAG_WIN32            0x20
#define ASC_FLAG_ISA_OVER_16MB    0x40
#define ASC_FLAG_DOS_VM_CALLBACK  0x80
#define ASC_TAG_FLAG_EXTRA_BYTES               0x10
#define ASC_TAG_FLAG_DISABLE_DISCONNECT        0x04
#define ASC_TAG_FLAG_DISABLE_ASYN_USE_SYN_FIX  0x08
#define ASC_TAG_FLAG_DISABLE_CHK_COND_INT_HOST 0x40
#define ASC_SCSIQ_CPY_BEG              4
#define ASC_SCSIQ_SGHD_CPY_BEG         2
#define ASC_SCSIQ_B_FWD                0
#define ASC_SCSIQ_B_BWD                1
#define ASC_SCSIQ_B_STATUS             2
#define ASC_SCSIQ_B_QNO                3
#define ASC_SCSIQ_B_CNTL               4
#define ASC_SCSIQ_B_SG_QUEUE_CNT       5
#define ASC_SCSIQ_D_DATA_ADDR          8
#define ASC_SCSIQ_D_DATA_CNT          12
#define ASC_SCSIQ_B_SENSE_LEN         20
#define ASC_SCSIQ_DONE_INFO_BEG       22
#define ASC_SCSIQ_D_SRBPTR            22
#define ASC_SCSIQ_B_TARGET_IX         26
#define ASC_SCSIQ_B_CDB_LEN           28
#define ASC_SCSIQ_B_TAG_CODE          29
#define ASC_SCSIQ_W_VM_ID             30
#define ASC_SCSIQ_DONE_STATUS         32
#define ASC_SCSIQ_HOST_STATUS         33
#define ASC_SCSIQ_SCSI_STATUS         34
#define ASC_SCSIQ_CDB_BEG             36
#define ASC_SCSIQ_DW_REMAIN_XFER_ADDR 56
#define ASC_SCSIQ_DW_REMAIN_XFER_CNT  60
#define ASC_SCSIQ_B_FIRST_SG_WK_QP    48
#define ASC_SCSIQ_B_SG_WK_QP          49
#define ASC_SCSIQ_B_SG_WK_IX          50
#define ASC_SCSIQ_W_ALT_DC1           52
#define ASC_SCSIQ_B_LIST_CNT          6
#define ASC_SCSIQ_B_CUR_LIST_CNT      7
#define ASC_SGQ_B_SG_CNTL             4
#define ASC_SGQ_B_SG_HEAD_QP          5
#define ASC_SGQ_B_SG_LIST_CNT         6
#define ASC_SGQ_B_SG_CUR_LIST_CNT     7
#define ASC_SGQ_LIST_BEG              8
#define ASC_DEF_SCSI1_QNG    4
#define ASC_MAX_SCSI1_QNG    4
#define ASC_DEF_SCSI2_QNG    16
#define ASC_MAX_SCSI2_QNG    32
#define ASC_TAG_CODE_MASK    0x23
#define ASC_STOP_REQ_RISC_STOP      0x01
#define ASC_STOP_ACK_RISC_STOP      0x03
#define ASC_STOP_CLEAN_UP_BUSY_Q    0x10
#define ASC_STOP_CLEAN_UP_DISC_Q    0x20
#define ASC_STOP_HOST_REQ_RISC_HALT 0x40
#define ASC_TIDLUN_TO_IX(tid, lun)  (ASC_SCSI_TIX_TYPE)((tid) + ((lun)<<ASC_SCSI_ID_BITS))
#define ASC_TID_TO_TARGET_ID(tid)   (ASC_SCSI_BIT_ID_TYPE)(0x01 << (tid))
#define ASC_TIX_TO_TARGET_ID(tix)   (0x01 << ((tix) & ASC_MAX_TID))
#define ASC_TIX_TO_TID(tix)         ((tix) & ASC_MAX_TID)
#define ASC_TID_TO_TIX(tid)         ((tid) & ASC_MAX_TID)
#define ASC_TIX_TO_LUN(tix)         (((tix) >> ASC_SCSI_ID_BITS) & ASC_MAX_LUN)
#define ASC_QNO_TO_QADDR(q_no)      ((ASC_QADR_BEG)+((int)(q_no) << 6))

typedef struct asc_scsiq_1 {
	uchar status;
	uchar q_no;
	uchar cntl;
	uchar sg_queue_cnt;
	uchar target_id;
	uchar target_lun;
	ASC_PADDR data_addr;
	ASC_DCNT data_cnt;
	ASC_PADDR sense_addr;
	uchar sense_len;
	uchar extra_bytes;
} ASC_SCSIQ_1;

typedef struct asc_scsiq_2 {
	ASC_VADDR srb_ptr;
	uchar target_ix;
	uchar flag;
	uchar cdb_len;
	uchar tag_code;
	ushort vm_id;
} ASC_SCSIQ_2;

typedef struct asc_scsiq_3 {
	uchar done_stat;
	uchar host_stat;
	uchar scsi_stat;
	uchar scsi_msg;
} ASC_SCSIQ_3;

typedef struct asc_scsiq_4 {
	uchar cdb[ASC_MAX_CDB_LEN];
	uchar y_first_sg_list_qp;
	uchar y_working_sg_qp;
	uchar y_working_sg_ix;
	uchar y_res;
	ushort x_req_count;
	ushort x_reconnect_rtn;
	ASC_PADDR x_saved_data_addr;
	ASC_DCNT x_saved_data_cnt;
} ASC_SCSIQ_4;

typedef struct asc_q_done_info {
	ASC_SCSIQ_2 d2;
	ASC_SCSIQ_3 d3;
	uchar q_status;
	uchar q_no;
	uchar cntl;
	uchar sense_len;
	uchar extra_bytes;
	uchar res;
	ASC_DCNT remain_bytes;
} ASC_QDONE_INFO;

typedef struct asc_sg_list {
	ASC_PADDR addr;
	ASC_DCNT bytes;
} ASC_SG_LIST;

typedef struct asc_sg_head {
	ushort entry_cnt;
	ushort queue_cnt;
	ushort entry_to_copy;
	ushort res;
	ASC_SG_LIST sg_list[0];
} ASC_SG_HEAD;

typedef struct asc_scsi_q {
	ASC_SCSIQ_1 q1;
	ASC_SCSIQ_2 q2;
	uchar *cdbptr;
	ASC_SG_HEAD *sg_head;
	ushort remain_sg_entry_cnt;
	ushort next_sg_index;
} ASC_SCSI_Q;

typedef struct asc_scsi_req_q {
	ASC_SCSIQ_1 r1;
	ASC_SCSIQ_2 r2;
	uchar *cdbptr;
	ASC_SG_HEAD *sg_head;
	uchar *sense_ptr;
	ASC_SCSIQ_3 r3;
	uchar cdb[ASC_MAX_CDB_LEN];
	uchar sense[ASC_MIN_SENSE_LEN];
} ASC_SCSI_REQ_Q;

typedef struct asc_scsi_bios_req_q {
	ASC_SCSIQ_1 r1;
	ASC_SCSIQ_2 r2;
	uchar *cdbptr;
	ASC_SG_HEAD *sg_head;
	uchar *sense_ptr;
	ASC_SCSIQ_3 r3;
	uchar cdb[ASC_MAX_CDB_LEN];
	uchar sense[ASC_MIN_SENSE_LEN];
} ASC_SCSI_BIOS_REQ_Q;

typedef struct asc_risc_q {
	uchar fwd;
	uchar bwd;
	ASC_SCSIQ_1 i1;
	ASC_SCSIQ_2 i2;
	ASC_SCSIQ_3 i3;
	ASC_SCSIQ_4 i4;
} ASC_RISC_Q;

typedef struct asc_sg_list_q {
	uchar seq_no;
	uchar q_no;
	uchar cntl;
	uchar sg_head_qp;
	uchar sg_list_cnt;
	uchar sg_cur_list_cnt;
} ASC_SG_LIST_Q;

typedef struct asc_risc_sg_list_q {
	uchar fwd;
	uchar bwd;
	ASC_SG_LIST_Q sg;
	ASC_SG_LIST sg_list[7];
} ASC_RISC_SG_LIST_Q;

#define ASCQ_ERR_Q_STATUS             0x0D
#define ASCQ_ERR_CUR_QNG              0x17
#define ASCQ_ERR_SG_Q_LINKS           0x18
#define ASCQ_ERR_ISR_RE_ENTRY         0x1A
#define ASCQ_ERR_CRITICAL_RE_ENTRY    0x1B
#define ASCQ_ERR_ISR_ON_CRITICAL      0x1C

/*
 * Warning code values are set in ASC_DVC_VAR  'warn_code'.
 */
#define ASC_WARN_NO_ERROR             0x0000
#define ASC_WARN_IO_PORT_ROTATE       0x0001
#define ASC_WARN_EEPROM_CHKSUM        0x0002
#define ASC_WARN_IRQ_MODIFIED         0x0004
#define ASC_WARN_AUTO_CONFIG          0x0008
#define ASC_WARN_CMD_QNG_CONFLICT     0x0010
#define ASC_WARN_EEPROM_RECOVER       0x0020
#define ASC_WARN_CFG_MSW_RECOVER      0x0040

/*
 * Error code values are set in {ASC/ADV}_DVC_VAR  'err_code'.
 */
#define ASC_IERR_NO_CARRIER		0x0001	/* No more carrier memory */
#define ASC_IERR_MCODE_CHKSUM		0x0002	/* micro code check sum error */
#define ASC_IERR_SET_PC_ADDR		0x0004
#define ASC_IERR_START_STOP_CHIP	0x0008	/* start/stop chip failed */
#define ASC_IERR_ILLEGAL_CONNECTION	0x0010	/* Illegal cable connection */
#define ASC_IERR_SINGLE_END_DEVICE	0x0020	/* SE device on DIFF bus */
#define ASC_IERR_REVERSED_CABLE		0x0040	/* Narrow flat cable reversed */
#define ASC_IERR_SET_SCSI_ID		0x0080	/* set SCSI ID failed */
#define ASC_IERR_HVD_DEVICE		0x0100	/* HVD device on LVD port */
#define ASC_IERR_BAD_SIGNATURE		0x0200	/* signature not found */
#define ASC_IERR_NO_BUS_TYPE		0x0400
#define ASC_IERR_BIST_PRE_TEST		0x0800	/* BIST pre-test error */
#define ASC_IERR_BIST_RAM_TEST		0x1000	/* BIST RAM test error */
#define ASC_IERR_BAD_CHIPTYPE		0x2000	/* Invalid chip_type setting */

#define ASC_DEF_MAX_TOTAL_QNG   (0xF0)
#define ASC_MIN_TAG_Q_PER_DVC   (0x04)
#define ASC_MIN_FREE_Q        (0x02)
#define ASC_MIN_TOTAL_QNG     ((ASC_MAX_SG_QUEUE)+(ASC_MIN_FREE_Q))
#define ASC_MAX_TOTAL_QNG 240
#define ASC_MAX_PCI_ULTRA_INRAM_TOTAL_QNG 16
#define ASC_MAX_PCI_ULTRA_INRAM_TAG_QNG   8
#define ASC_MAX_PCI_INRAM_TOTAL_QNG  20
#define ASC_MAX_INRAM_TAG_QNG   16
#define ASC_IOADR_GAP   0x10
#define ASC_SYN_MAX_OFFSET         0x0F
#define ASC_DEF_SDTR_OFFSET        0x0F
#define ASC_SDTR_ULTRA_PCI_10MB_INDEX  0x02
#define ASYN_SDTR_DATA_FIX_PCI_REV_AB 0x41

/* The narrow chip only supports a limited selection of transfer rates.
 * These are encoded in the range 0..7 or 0..15 depending whether the chip
 * is Ultra-capable or not.  These tables let us convert from one to the other.
 */
static const unsigned char asc_syn_xfer_period[8] = {
	25, 30, 35, 40, 50, 60, 70, 85
};

static const unsigned char asc_syn_ultra_xfer_period[16] = {
	12, 19, 25, 32, 38, 44, 50, 57, 63, 69, 75, 82, 88, 94, 100, 107
};

typedef struct ext_msg {
	uchar msg_type;
	uchar msg_len;
	uchar msg_req;
	union {
		struct {
			uchar sdtr_xfer_period;
			uchar sdtr_req_ack_offset;
		} sdtr;
		struct {
			uchar wdtr_width;
		} wdtr;
		struct {
			uchar mdp_b3;
			uchar mdp_b2;
			uchar mdp_b1;
			uchar mdp_b0;
		} mdp;
	} u_ext_msg;
	uchar res;
} EXT_MSG;

#define xfer_period     u_ext_msg.sdtr.sdtr_xfer_period
#define req_ack_offset  u_ext_msg.sdtr.sdtr_req_ack_offset
#define wdtr_width      u_ext_msg.wdtr.wdtr_width
#define mdp_b3          u_ext_msg.mdp_b3
#define mdp_b2          u_ext_msg.mdp_b2
#define mdp_b1          u_ext_msg.mdp_b1
#define mdp_b0          u_ext_msg.mdp_b0

typedef struct asc_dvc_cfg {
	ASC_SCSI_BIT_ID_TYPE can_tagged_qng;
	ASC_SCSI_BIT_ID_TYPE cmd_qng_enabled;
	ASC_SCSI_BIT_ID_TYPE disc_enable;
	ASC_SCSI_BIT_ID_TYPE sdtr_enable;
	uchar chip_scsi_id;
	uchar isa_dma_speed;
	uchar isa_dma_channel;
	uchar chip_version;
	ushort mcode_date;
	ushort mcode_version;
	uchar max_tag_qng[ASC_MAX_TID + 1];
	uchar sdtr_period_offset[ASC_MAX_TID + 1];
	uchar adapter_info[6];
} ASC_DVC_CFG;

#define ASC_DEF_DVC_CNTL       0xFFFF
#define ASC_DEF_CHIP_SCSI_ID   7
#define ASC_DEF_ISA_DMA_SPEED  4
#define ASC_INIT_STATE_BEG_GET_CFG   0x0001
#define ASC_INIT_STATE_END_GET_CFG   0x0002
#define ASC_INIT_STATE_BEG_SET_CFG   0x0004
#define ASC_INIT_STATE_END_SET_CFG   0x0008
#define ASC_INIT_STATE_BEG_LOAD_MC   0x0010
#define ASC_INIT_STATE_END_LOAD_MC   0x0020
#define ASC_INIT_STATE_BEG_INQUIRY   0x0040
#define ASC_INIT_STATE_END_INQUIRY   0x0080
#define ASC_INIT_RESET_SCSI_DONE     0x0100
#define ASC_INIT_STATE_WITHOUT_EEP   0x8000
#define ASC_BUG_FIX_IF_NOT_DWB       0x0001
#define ASC_BUG_FIX_ASYN_USE_SYN     0x0002
#define ASC_MIN_TAGGED_CMD  7
#define ASC_MAX_SCSI_RESET_WAIT      30
#define ASC_OVERRUN_BSIZE		64

struct asc_dvc_var;		/* Forward Declaration. */

typedef struct asc_dvc_var {
	PortAddr iop_base;
	ushort err_code;
	ushort dvc_cntl;
	ushort bug_fix_cntl;
	ushort bus_type;
	ASC_SCSI_BIT_ID_TYPE init_sdtr;
	ASC_SCSI_BIT_ID_TYPE sdtr_done;
	ASC_SCSI_BIT_ID_TYPE use_tagged_qng;
	ASC_SCSI_BIT_ID_TYPE unit_not_ready;
	ASC_SCSI_BIT_ID_TYPE queue_full_or_busy;
	ASC_SCSI_BIT_ID_TYPE start_motor;
	uchar *overrun_buf;
	dma_addr_t overrun_dma;
	uchar scsi_reset_wait;
	uchar chip_no;
	char is_in_int;
	uchar max_total_qng;
	uchar cur_total_qng;
	uchar in_critical_cnt;
	uchar last_q_shortage;
	ushort init_state;
	uchar cur_dvc_qng[ASC_MAX_TID + 1];
	uchar max_dvc_qng[ASC_MAX_TID + 1];
	ASC_SCSI_Q *scsiq_busy_head[ASC_MAX_TID + 1];
	ASC_SCSI_Q *scsiq_busy_tail[ASC_MAX_TID + 1];
	const uchar *sdtr_period_tbl;
	ASC_DVC_CFG *cfg;
	ASC_SCSI_BIT_ID_TYPE pci_fix_asyn_xfer_always;
	char redo_scam;
	ushort res2;
	uchar dos_int13_table[ASC_MAX_TID + 1];
	ASC_DCNT max_dma_count;
	ASC_SCSI_BIT_ID_TYPE no_scam;
	ASC_SCSI_BIT_ID_TYPE pci_fix_asyn_xfer;
	uchar min_sdtr_index;
	uchar max_sdtr_index;
	struct asc_board *drv_ptr;
	int ptr_map_count;
	void **ptr_map;
	ASC_DCNT uc_break;
} ASC_DVC_VAR;

typedef struct asc_dvc_inq_info {
	uchar type[ASC_MAX_TID + 1][ASC_MAX_LUN + 1];
} ASC_DVC_INQ_INFO;

typedef struct asc_cap_info {
	ASC_DCNT lba;
	ASC_DCNT blk_size;
} ASC_CAP_INFO;

typedef struct asc_cap_info_array {
	ASC_CAP_INFO cap_info[ASC_MAX_TID + 1][ASC_MAX_LUN + 1];
} ASC_CAP_INFO_ARRAY;

#define ASC_MCNTL_NO_SEL_TIMEOUT  (ushort)0x0001
#define ASC_MCNTL_NULL_TARGET     (ushort)0x0002
#define ASC_CNTL_INITIATOR         (ushort)0x0001
#define ASC_CNTL_BIOS_GT_1GB       (ushort)0x0002
#define ASC_CNTL_BIOS_GT_2_DISK    (ushort)0x0004
#define ASC_CNTL_BIOS_REMOVABLE    (ushort)0x0008
#define ASC_CNTL_NO_SCAM           (ushort)0x0010
#define ASC_CNTL_INT_MULTI_Q       (ushort)0x0080
#define ASC_CNTL_NO_LUN_SUPPORT    (ushort)0x0040
#define ASC_CNTL_NO_VERIFY_COPY    (ushort)0x0100
#define ASC_CNTL_RESET_SCSI        (ushort)0x0200
#define ASC_CNTL_INIT_INQUIRY      (ushort)0x0400
#define ASC_CNTL_INIT_VERBOSE      (ushort)0x0800
#define ASC_CNTL_SCSI_PARITY       (ushort)0x1000
#define ASC_CNTL_BURST_MODE        (ushort)0x2000
#define ASC_CNTL_SDTR_ENABLE_ULTRA (ushort)0x4000
#define ASC_EEP_DVC_CFG_BEG_VL    2
#define ASC_EEP_MAX_DVC_ADDR_VL   15
#define ASC_EEP_DVC_CFG_BEG      32
#define ASC_EEP_MAX_DVC_ADDR     45
#define ASC_EEP_MAX_RETRY        20

/*
 * These macros keep the chip SCSI id and ISA DMA speed
 * bitfields in board order. C bitfields aren't portable
 * between big and little-endian platforms so they are
 * not used.
 */

#define ASC_EEP_GET_CHIP_ID(cfg)    ((cfg)->id_speed & 0x0f)
#define ASC_EEP_GET_DMA_SPD(cfg)    (((cfg)->id_speed & 0xf0) >> 4)
#define ASC_EEP_SET_CHIP_ID(cfg, sid) \
   ((cfg)->id_speed = ((cfg)->id_speed & 0xf0) | ((sid) & ASC_MAX_TID))
#define ASC_EEP_SET_DMA_SPD(cfg, spd) \
   ((cfg)->id_speed = ((cfg)->id_speed & 0x0f) | ((spd) & 0x0f) << 4)

typedef struct asceep_config {
	ushort cfg_lsw;
	ushort cfg_msw;
	uchar init_sdtr;
	uchar disc_enable;
	uchar use_cmd_qng;
	uchar start_motor;
	uchar max_total_qng;
	uchar max_tag_qng;
	uchar bios_scan;
	uchar power_up_wait;
	uchar no_scam;
	uchar id_speed;		/* low order 4 bits is chip scsi id */
	/* high order 4 bits is isa dma speed */
	uchar dos_int13_table[ASC_MAX_TID + 1];
	uchar adapter_info[6];
	ushort cntl;
	ushort chksum;
} ASCEEP_CONFIG;

#define ASC_EEP_CMD_READ          0x80
#define ASC_EEP_CMD_WRITE         0x40
#define ASC_EEP_CMD_WRITE_ABLE    0x30
#define ASC_EEP_CMD_WRITE_DISABLE 0x00
#define ASCV_MSGOUT_BEG         0x0000
#define ASCV_MSGOUT_SDTR_PERIOD (ASCV_MSGOUT_BEG+3)
#define ASCV_MSGOUT_SDTR_OFFSET (ASCV_MSGOUT_BEG+4)
#define ASCV_BREAK_SAVED_CODE   (ushort)0x0006
#define ASCV_MSGIN_BEG          (ASCV_MSGOUT_BEG+8)
#define ASCV_MSGIN_SDTR_PERIOD  (ASCV_MSGIN_BEG+3)
#define ASCV_MSGIN_SDTR_OFFSET  (ASCV_MSGIN_BEG+4)
#define ASCV_SDTR_DATA_BEG      (ASCV_MSGIN_BEG+8)
#define ASCV_SDTR_DONE_BEG      (ASCV_SDTR_DATA_BEG+8)
#define ASCV_MAX_DVC_QNG_BEG    (ushort)0x0020
#define ASCV_BREAK_ADDR           (ushort)0x0028
#define ASCV_BREAK_NOTIFY_COUNT   (ushort)0x002A
#define ASCV_BREAK_CONTROL        (ushort)0x002C
#define ASCV_BREAK_HIT_COUNT      (ushort)0x002E

#define ASCV_ASCDVC_ERR_CODE_W  (ushort)0x0030
#define ASCV_MCODE_CHKSUM_W   (ushort)0x0032
#define ASCV_MCODE_SIZE_W     (ushort)0x0034
#define ASCV_STOP_CODE_B      (ushort)0x0036
#define ASCV_DVC_ERR_CODE_B   (ushort)0x0037
#define ASCV_OVERRUN_PADDR_D  (ushort)0x0038
#define ASCV_OVERRUN_BSIZE_D  (ushort)0x003C
#define ASCV_HALTCODE_W       (ushort)0x0040
#define ASCV_CHKSUM_W         (ushort)0x0042
#define ASCV_MC_DATE_W        (ushort)0x0044
#define ASCV_MC_VER_W         (ushort)0x0046
#define ASCV_NEXTRDY_B        (ushort)0x0048
#define ASCV_DONENEXT_B       (ushort)0x0049
#define ASCV_USE_TAGGED_QNG_B (ushort)0x004A
#define ASCV_SCSIBUSY_B       (ushort)0x004B
#define ASCV_Q_DONE_IN_PROGRESS_B  (ushort)0x004C
#define ASCV_CURCDB_B         (ushort)0x004D
#define ASCV_RCLUN_B          (ushort)0x004E
#define ASCV_BUSY_QHEAD_B     (ushort)0x004F
#define ASCV_DISC1_QHEAD_B    (ushort)0x0050
#define ASCV_DISC_ENABLE_B    (ushort)0x0052
#define ASCV_CAN_TAGGED_QNG_B (ushort)0x0053
#define ASCV_HOSTSCSI_ID_B    (ushort)0x0055
#define ASCV_MCODE_CNTL_B     (ushort)0x0056
#define ASCV_NULL_TARGET_B    (ushort)0x0057
#define ASCV_FREE_Q_HEAD_W    (ushort)0x0058
#define ASCV_DONE_Q_TAIL_W    (ushort)0x005A
#define ASCV_FREE_Q_HEAD_B    (ushort)(ASCV_FREE_Q_HEAD_W+1)
#define ASCV_DONE_Q_TAIL_B    (ushort)(ASCV_DONE_Q_TAIL_W+1)
#define ASCV_HOST_FLAG_B      (ushort)0x005D
#define ASCV_TOTAL_READY_Q_B  (ushort)0x0064
#define ASCV_VER_SERIAL_B     (ushort)0x0065
#define ASCV_HALTCODE_SAVED_W (ushort)0x0066
#define ASCV_WTM_FLAG_B       (ushort)0x0068
#define ASCV_RISC_FLAG_B      (ushort)0x006A
#define ASCV_REQ_SG_LIST_QP   (ushort)0x006B
#define ASC_HOST_FLAG_IN_ISR        0x01
#define ASC_HOST_FLAG_ACK_INT       0x02
#define ASC_RISC_FLAG_GEN_INT      0x01
#define ASC_RISC_FLAG_REQ_SG_LIST  0x02
#define IOP_CTRL         (0x0F)
#define IOP_STATUS       (0x0E)
#define IOP_INT_ACK      IOP_STATUS
#define IOP_REG_IFC      (0x0D)
#define IOP_SYN_OFFSET    (0x0B)
#define IOP_EXTRA_CONTROL (0x0D)
#define IOP_REG_PC        (0x0C)
#define IOP_RAM_ADDR      (0x0A)
#define IOP_RAM_DATA      (0x08)
#define IOP_EEP_DATA      (0x06)
#define IOP_EEP_CMD       (0x07)
#define IOP_VERSION       (0x03)
#define IOP_CONFIG_HIGH   (0x04)
#define IOP_CONFIG_LOW    (0x02)
#define IOP_SIG_BYTE      (0x01)
#define IOP_SIG_WORD      (0x00)
#define IOP_REG_DC1      (0x0E)
#define IOP_REG_DC0      (0x0C)
#define IOP_REG_SB       (0x0B)
#define IOP_REG_DA1      (0x0A)
#define IOP_REG_DA0      (0x08)
#define IOP_REG_SC       (0x09)
#define IOP_DMA_SPEED    (0x07)
#define IOP_REG_FLAG     (0x07)
#define IOP_FIFO_H       (0x06)
#define IOP_FIFO_L       (0x04)
#define IOP_REG_ID       (0x05)
#define IOP_REG_QP       (0x03)
#define IOP_REG_IH       (0x02)
#define IOP_REG_IX       (0x01)
#define IOP_REG_AX       (0x00)
#define IFC_REG_LOCK      (0x00)
#define IFC_REG_UNLOCK    (0x09)
#define IFC_WR_EN_FILTER  (0x10)
#define IFC_RD_NO_EEPROM  (0x10)
#define IFC_SLEW_RATE     (0x20)
#define IFC_ACT_NEG       (0x40)
#define IFC_INP_FILTER    (0x80)
#define IFC_INIT_DEFAULT  (IFC_ACT_NEG | IFC_REG_UNLOCK)
#define SC_SEL   (uchar)(0x80)
#define SC_BSY   (uchar)(0x40)
#define SC_ACK   (uchar)(0x20)
#define SC_REQ   (uchar)(0x10)
#define SC_ATN   (uchar)(0x08)
#define SC_IO    (uchar)(0x04)
#define SC_CD    (uchar)(0x02)
#define SC_MSG   (uchar)(0x01)
#define SEC_SCSI_CTL         (uchar)(0x80)
#define SEC_ACTIVE_NEGATE    (uchar)(0x40)
#define SEC_SLEW_RATE        (uchar)(0x20)
#define SEC_ENABLE_FILTER    (uchar)(0x10)
#define ASC_HALT_EXTMSG_IN     (ushort)0x8000
#define ASC_HALT_CHK_CONDITION (ushort)0x8100
#define ASC_HALT_SS_QUEUE_FULL (ushort)0x8200
#define ASC_HALT_DISABLE_ASYN_USE_SYN_FIX  (ushort)0x8300
#define ASC_HALT_ENABLE_ASYN_USE_SYN_FIX   (ushort)0x8400
#define ASC_HALT_SDTR_REJECTED (ushort)0x4000
#define ASC_HALT_HOST_COPY_SG_LIST_TO_RISC ( ushort )0x2000
#define ASC_MAX_QNO        0xF8
#define ASC_DATA_SEC_BEG   (ushort)0x0080
#define ASC_DATA_SEC_END   (ushort)0x0080
#define ASC_CODE_SEC_BEG   (ushort)0x0080
#define ASC_CODE_SEC_END   (ushort)0x0080
#define ASC_QADR_BEG       (0x4000)
#define ASC_QADR_USED      (ushort)(ASC_MAX_QNO * 64)
#define ASC_QADR_END       (ushort)0x7FFF
#define ASC_QLAST_ADR      (ushort)0x7FC0
#define ASC_QBLK_SIZE      0x40
#define ASC_BIOS_DATA_QBEG 0xF8
#define ASC_MIN_ACTIVE_QNO 0x01
#define ASC_QLINK_END      0xFF
#define ASC_EEPROM_WORDS   0x10
#define ASC_MAX_MGS_LEN    0x10
#define ASC_BIOS_ADDR_DEF  0xDC00
#define ASC_BIOS_SIZE      0x3800
#define ASC_BIOS_RAM_OFF   0x3800
#define ASC_BIOS_RAM_SIZE  0x800
#define ASC_BIOS_MIN_ADDR  0xC000
#define ASC_BIOS_MAX_ADDR  0xEC00
#define ASC_BIOS_BANK_SIZE 0x0400
#define ASC_MCODE_START_ADDR  0x0080
#define ASC_CFG0_HOST_INT_ON    0x0020
#define ASC_CFG0_BIOS_ON        0x0040
#define ASC_CFG0_VERA_BURST_ON  0x0080
#define ASC_CFG0_SCSI_PARITY_ON 0x0800
#define ASC_CFG1_SCSI_TARGET_ON 0x0080
#define ASC_CFG1_LRAM_8BITS_ON  0x0800
#define ASC_CFG_MSW_CLR_MASK    0x3080
#define CSW_TEST1             (ASC_CS_TYPE)0x8000
#define CSW_AUTO_CONFIG       (ASC_CS_TYPE)0x4000
#define CSW_RESERVED1         (ASC_CS_TYPE)0x2000
#define CSW_IRQ_WRITTEN       (ASC_CS_TYPE)0x1000
#define CSW_33MHZ_SELECTED    (ASC_CS_TYPE)0x0800
#define CSW_TEST2             (ASC_CS_TYPE)0x0400
#define CSW_TEST3             (ASC_CS_TYPE)0x0200
#define CSW_RESERVED2         (ASC_CS_TYPE)0x0100
#define CSW_DMA_DONE          (ASC_CS_TYPE)0x0080
#define CSW_FIFO_RDY          (ASC_CS_TYPE)0x0040
#define CSW_EEP_READ_DONE     (ASC_CS_TYPE)0x0020
#define CSW_HALTED            (ASC_CS_TYPE)0x0010
#define CSW_SCSI_RESET_ACTIVE (ASC_CS_TYPE)0x0008
#define CSW_PARITY_ERR        (ASC_CS_TYPE)0x0004
#define CSW_SCSI_RESET_LATCH  (ASC_CS_TYPE)0x0002
#define CSW_INT_PENDING       (ASC_CS_TYPE)0x0001
#define CIW_CLR_SCSI_RESET_INT (ASC_CS_TYPE)0x1000
#define CIW_INT_ACK      (ASC_CS_TYPE)0x0100
#define CIW_TEST1        (ASC_CS_TYPE)0x0200
#define CIW_TEST2        (ASC_CS_TYPE)0x0400
#define CIW_SEL_33MHZ    (ASC_CS_TYPE)0x0800
#define CIW_IRQ_ACT      (ASC_CS_TYPE)0x1000
#define CC_CHIP_RESET   (uchar)0x80
#define CC_SCSI_RESET   (uchar)0x40
#define CC_HALT         (uchar)0x20
#define CC_SINGLE_STEP  (uchar)0x10
#define CC_DMA_ABLE     (uchar)0x08
#define CC_TEST         (uchar)0x04
#define CC_BANK_ONE     (uchar)0x02
#define CC_DIAG         (uchar)0x01
#define ASC_1000_ID0W      0x04C1
#define ASC_1000_ID0W_FIX  0x00C1
#define ASC_1000_ID1B      0x25
#define ASC_EISA_REV_IOP_MASK  (0x0C83)
#define ASC_EISA_CFG_IOP_MASK  (0x0C86)
#define ASC_GET_EISA_SLOT(iop)  (PortAddr)((iop) & 0xF000)
#define INS_HALTINT        (ushort)0x6281
#define INS_HALT           (ushort)0x6280
#define INS_SINT           (ushort)0x6200
#define INS_RFLAG_WTM      (ushort)0x7380
#define ASC_MC_SAVE_CODE_WSIZE  0x500
#define ASC_MC_SAVE_DATA_WSIZE  0x40

typedef struct asc_mc_saved {
	ushort data[ASC_MC_SAVE_DATA_WSIZE];
	ushort code[ASC_MC_SAVE_CODE_WSIZE];
} ASC_MC_SAVED;

#define AscGetQDoneInProgress(port)         AscReadLramByte((port), ASCV_Q_DONE_IN_PROGRESS_B)
#define AscPutQDoneInProgress(port, val)    AscWriteLramByte((port), ASCV_Q_DONE_IN_PROGRESS_B, val)
#define AscGetVarFreeQHead(port)            AscReadLramWord((port), ASCV_FREE_Q_HEAD_W)
#define AscGetVarDoneQTail(port)            AscReadLramWord((port), ASCV_DONE_Q_TAIL_W)
#define AscPutVarFreeQHead(port, val)       AscWriteLramWord((port), ASCV_FREE_Q_HEAD_W, val)
#define AscPutVarDoneQTail(port, val)       AscWriteLramWord((port), ASCV_DONE_Q_TAIL_W, val)
#define AscGetRiscVarFreeQHead(port)        AscReadLramByte((port), ASCV_NEXTRDY_B)
#define AscGetRiscVarDoneQTail(port)        AscReadLramByte((port), ASCV_DONENEXT_B)
#define AscPutRiscVarFreeQHead(port, val)   AscWriteLramByte((port), ASCV_NEXTRDY_B, val)
#define AscPutRiscVarDoneQTail(port, val)   AscWriteLramByte((port), ASCV_DONENEXT_B, val)
#define AscPutMCodeSDTRDoneAtID(port, id, data)  AscWriteLramByte((port), (ushort)((ushort)ASCV_SDTR_DONE_BEG+(ushort)id), (data))
#define AscGetMCodeSDTRDoneAtID(port, id)        AscReadLramByte((port), (ushort)((ushort)ASCV_SDTR_DONE_BEG+(ushort)id))
#define AscPutMCodeInitSDTRAtID(port, id, data)  AscWriteLramByte((port), (ushort)((ushort)ASCV_SDTR_DATA_BEG+(ushort)id), data)
#define AscGetMCodeInitSDTRAtID(port, id)        AscReadLramByte((port), (ushort)((ushort)ASCV_SDTR_DATA_BEG+(ushort)id))
#define AscGetChipSignatureByte(port)     (uchar)inp((port)+IOP_SIG_BYTE)
#define AscGetChipSignatureWord(port)     (ushort)inpw((port)+IOP_SIG_WORD)
#define AscGetChipVerNo(port)             (uchar)inp((port)+IOP_VERSION)
#define AscGetChipCfgLsw(port)            (ushort)inpw((port)+IOP_CONFIG_LOW)
#define AscGetChipCfgMsw(port)            (ushort)inpw((port)+IOP_CONFIG_HIGH)
#define AscSetChipCfgLsw(port, data)      outpw((port)+IOP_CONFIG_LOW, data)
#define AscSetChipCfgMsw(port, data)      outpw((port)+IOP_CONFIG_HIGH, data)
#define AscGetChipEEPCmd(port)            (uchar)inp((port)+IOP_EEP_CMD)
#define AscSetChipEEPCmd(port, data)      outp((port)+IOP_EEP_CMD, data)
#define AscGetChipEEPData(port)           (ushort)inpw((port)+IOP_EEP_DATA)
#define AscSetChipEEPData(port, data)     outpw((port)+IOP_EEP_DATA, data)
#define AscGetChipLramAddr(port)          (ushort)inpw((PortAddr)((port)+IOP_RAM_ADDR))
#define AscSetChipLramAddr(port, addr)    outpw((PortAddr)((port)+IOP_RAM_ADDR), addr)
#define AscGetChipLramData(port)          (ushort)inpw((port)+IOP_RAM_DATA)
#define AscSetChipLramData(port, data)    outpw((port)+IOP_RAM_DATA, data)
#define AscGetChipIFC(port)               (uchar)inp((port)+IOP_REG_IFC)
#define AscSetChipIFC(port, data)          outp((port)+IOP_REG_IFC, data)
#define AscGetChipStatus(port)            (ASC_CS_TYPE)inpw((port)+IOP_STATUS)
#define AscSetChipStatus(port, cs_val)    outpw((port)+IOP_STATUS, cs_val)
#define AscGetChipControl(port)           (uchar)inp((port)+IOP_CTRL)
#define AscSetChipControl(port, cc_val)   outp((port)+IOP_CTRL, cc_val)
#define AscGetChipSyn(port)               (uchar)inp((port)+IOP_SYN_OFFSET)
#define AscSetChipSyn(port, data)         outp((port)+IOP_SYN_OFFSET, data)
#define AscSetPCAddr(port, data)          outpw((port)+IOP_REG_PC, data)
#define AscGetPCAddr(port)                (ushort)inpw((port)+IOP_REG_PC)
#define AscIsIntPending(port)             (AscGetChipStatus(port) & (CSW_INT_PENDING | CSW_SCSI_RESET_LATCH))
#define AscGetChipScsiID(port)            ((AscGetChipCfgLsw(port) >> 8) & ASC_MAX_TID)
#define AscGetExtraControl(port)          (uchar)inp((port)+IOP_EXTRA_CONTROL)
#define AscSetExtraControl(port, data)    outp((port)+IOP_EXTRA_CONTROL, data)
#define AscReadChipAX(port)               (ushort)inpw((port)+IOP_REG_AX)
#define AscWriteChipAX(port, data)        outpw((port)+IOP_REG_AX, data)
#define AscReadChipIX(port)               (uchar)inp((port)+IOP_REG_IX)
#define AscWriteChipIX(port, data)        outp((port)+IOP_REG_IX, data)
#define AscReadChipIH(port)               (ushort)inpw((port)+IOP_REG_IH)
#define AscWriteChipIH(port, data)        outpw((port)+IOP_REG_IH, data)
#define AscReadChipQP(port)               (uchar)inp((port)+IOP_REG_QP)
#define AscWriteChipQP(port, data)        outp((port)+IOP_REG_QP, data)
#define AscReadChipFIFO_L(port)           (ushort)inpw((port)+IOP_REG_FIFO_L)
#define AscWriteChipFIFO_L(port, data)    outpw((port)+IOP_REG_FIFO_L, data)
#define AscReadChipFIFO_H(port)           (ushort)inpw((port)+IOP_REG_FIFO_H)
#define AscWriteChipFIFO_H(port, data)    outpw((port)+IOP_REG_FIFO_H, data)
#define AscReadChipDmaSpeed(port)         (uchar)inp((port)+IOP_DMA_SPEED)
#define AscWriteChipDmaSpeed(port, data)  outp((port)+IOP_DMA_SPEED, data)
#define AscReadChipDA0(port)              (ushort)inpw((port)+IOP_REG_DA0)
#define AscWriteChipDA0(port)             outpw((port)+IOP_REG_DA0, data)
#define AscReadChipDA1(port)              (ushort)inpw((port)+IOP_REG_DA1)
#define AscWriteChipDA1(port)             outpw((port)+IOP_REG_DA1, data)
#define AscReadChipDC0(port)              (ushort)inpw((port)+IOP_REG_DC0)
#define AscWriteChipDC0(port)             outpw((port)+IOP_REG_DC0, data)
#define AscReadChipDC1(port)              (ushort)inpw((port)+IOP_REG_DC1)
#define AscWriteChipDC1(port)             outpw((port)+IOP_REG_DC1, data)
#define AscReadChipDvcID(port)            (uchar)inp((port)+IOP_REG_ID)
#define AscWriteChipDvcID(port, data)     outp((port)+IOP_REG_ID, data)

/*
 * Portable Data Types
 *
 * Any instance where a 32-bit long or pointer type is assumed
 * for precision or HW defined structures, the following define
 * types must be used. In Linux the char, short, and int types
 * are all consistent at 8, 16, and 32 bits respectively. Pointers
 * and long types are 64 bits on Alpha and UltraSPARC.
 */
#define ADV_PADDR __u32		/* Physical address data type. */
#define ADV_VADDR __u32		/* Virtual address data type. */
#define ADV_DCNT  __u32		/* Unsigned Data count type. */
#define ADV_SDCNT __s32		/* Signed Data count type. */

/*
 * These macros are used to convert a virtual address to a
 * 32-bit value. This currently can be used on Linux Alpha
 * which uses 64-bit virtual address but a 32-bit bus address.
 * This is likely to break in the future, but doing this now
 * will give us time to change the HW and FW to handle 64-bit
 * addresses.
 */
#define ADV_VADDR_TO_U32   virt_to_bus
#define ADV_U32_TO_VADDR   bus_to_virt

#define AdvPortAddr  void __iomem *	/* Virtual memory address size */

/*
 * Define Adv Library required memory access macros.
 */
#define ADV_MEM_READB(addr) readb(addr)
#define ADV_MEM_READW(addr) readw(addr)
#define ADV_MEM_WRITEB(addr, byte) writeb(byte, addr)
#define ADV_MEM_WRITEW(addr, word) writew(word, addr)
#define ADV_MEM_WRITEDW(addr, dword) writel(dword, addr)

#define ADV_CARRIER_COUNT (ASC_DEF_MAX_HOST_QNG + 15)

/*
 * Define total number of simultaneous maximum element scatter-gather
 * request blocks per wide adapter. ASC_DEF_MAX_HOST_QNG (253) is the
 * maximum number of outstanding commands per wide host adapter. Each
 * command uses one or more ADV_SG_BLOCK each with 15 scatter-gather
 * elements. Allow each command to have at least one ADV_SG_BLOCK structure.
 * This allows about 15 commands to have the maximum 17 ADV_SG_BLOCK
 * structures or 255 scatter-gather elements.
 */
#define ADV_TOT_SG_BLOCK        ASC_DEF_MAX_HOST_QNG

/*
 * Define maximum number of scatter-gather elements per request.
 */
#define ADV_MAX_SG_LIST         255
#define NO_OF_SG_PER_BLOCK              15

#define ADV_EEP_DVC_CFG_BEGIN           (0x00)
#define ADV_EEP_DVC_CFG_END             (0x15)
#define ADV_EEP_DVC_CTL_BEGIN           (0x16)	/* location of OEM name */
#define ADV_EEP_MAX_WORD_ADDR           (0x1E)

#define ADV_EEP_DELAY_MS                100

#define ADV_EEPROM_BIG_ENDIAN          0x8000	/* EEPROM Bit 15 */
#define ADV_EEPROM_BIOS_ENABLE         0x4000	/* EEPROM Bit 14 */
/*
 * For the ASC3550 Bit 13 is Termination Polarity control bit.
 * For later ICs Bit 13 controls whether the CIS (Card Information
 * Service Section) is loaded from EEPROM.
 */
#define ADV_EEPROM_TERM_POL            0x2000	/* EEPROM Bit 13 */
#define ADV_EEPROM_CIS_LD              0x2000	/* EEPROM Bit 13 */
/*
 * ASC38C1600 Bit 11
 *
 * If EEPROM Bit 11 is 0 for Function 0, then Function 0 will specify
 * INT A in the PCI Configuration Space Int Pin field. If it is 1, then
 * Function 0 will specify INT B.
 *
 * If EEPROM Bit 11 is 0 for Function 1, then Function 1 will specify
 * INT B in the PCI Configuration Space Int Pin field. If it is 1, then
 * Function 1 will specify INT A.
 */
#define ADV_EEPROM_INTAB               0x0800	/* EEPROM Bit 11 */

typedef struct adveep_3550_config {
	/* Word Offset, Description */

	ushort cfg_lsw;		/* 00 power up initialization */
	/*  bit 13 set - Term Polarity Control */
	/*  bit 14 set - BIOS Enable */
	/*  bit 15 set - Big Endian Mode */
	ushort cfg_msw;		/* 01 unused      */
	ushort disc_enable;	/* 02 disconnect enable */
	ushort wdtr_able;	/* 03 Wide DTR able */
	ushort sdtr_able;	/* 04 Synchronous DTR able */
	ushort start_motor;	/* 05 send start up motor */
	ushort tagqng_able;	/* 06 tag queuing able */
	ushort bios_scan;	/* 07 BIOS device control */
	ushort scam_tolerant;	/* 08 no scam */

	uchar adapter_scsi_id;	/* 09 Host Adapter ID */
	uchar bios_boot_delay;	/*    power up wait */

	uchar scsi_reset_delay;	/* 10 reset delay */
	uchar bios_id_lun;	/*    first boot device scsi id & lun */
	/*    high nibble is lun */
	/*    low nibble is scsi id */

	uchar termination;	/* 11 0 - automatic */
	/*    1 - low off / high off */
	/*    2 - low off / high on */
	/*    3 - low on  / high on */
	/*    There is no low on  / high off */

	uchar reserved1;	/*    reserved byte (not used) */

	ushort bios_ctrl;	/* 12 BIOS control bits */
	/*  bit 0  BIOS don't act as initiator. */
	/*  bit 1  BIOS > 1 GB support */
	/*  bit 2  BIOS > 2 Disk Support */
	/*  bit 3  BIOS don't support removables */
	/*  bit 4  BIOS support bootable CD */
	/*  bit 5  BIOS scan enabled */
	/*  bit 6  BIOS support multiple LUNs */
	/*  bit 7  BIOS display of message */
	/*  bit 8  SCAM disabled */
	/*  bit 9  Reset SCSI bus during init. */
	/*  bit 10 */
	/*  bit 11 No verbose initialization. */
	/*  bit 12 SCSI parity enabled */
	/*  bit 13 */
	/*  bit 14 */
	/*  bit 15 */
	ushort ultra_able;	/* 13 ULTRA speed able */
	ushort reserved2;	/* 14 reserved */
	uchar max_host_qng;	/* 15 maximum host queuing */
	uchar max_dvc_qng;	/*    maximum per device queuing */
	ushort dvc_cntl;	/* 16 control bit for driver */
	ushort bug_fix;		/* 17 control bit for bug fix */
	ushort serial_number_word1;	/* 18 Board serial number word 1 */
	ushort serial_number_word2;	/* 19 Board serial number word 2 */
	ushort serial_number_word3;	/* 20 Board serial number word 3 */
	ushort check_sum;	/* 21 EEP check sum */
	uchar oem_name[16];	/* 22 OEM name */
	ushort dvc_err_code;	/* 30 last device driver error code */
	ushort adv_err_code;	/* 31 last uc and Adv Lib error code */
	ushort adv_err_addr;	/* 32 last uc error address */
	ushort saved_dvc_err_code;	/* 33 saved last dev. driver error code   */
	ushort saved_adv_err_code;	/* 34 saved last uc and Adv Lib error code */
	ushort saved_adv_err_addr;	/* 35 saved last uc error address         */
	ushort num_of_err;	/* 36 number of error */
} ADVEEP_3550_CONFIG;

typedef struct adveep_38C0800_config {
	/* Word Offset, Description */

	ushort cfg_lsw;		/* 00 power up initialization */
	/*  bit 13 set - Load CIS */
	/*  bit 14 set - BIOS Enable */
	/*  bit 15 set - Big Endian Mode */
	ushort cfg_msw;		/* 01 unused      */
	ushort disc_enable;	/* 02 disconnect enable */
	ushort wdtr_able;	/* 03 Wide DTR able */
	ushort sdtr_speed1;	/* 04 SDTR Speed TID 0-3 */
	ushort start_motor;	/* 05 send start up motor */
	ushort tagqng_able;	/* 06 tag queuing able */
	ushort bios_scan;	/* 07 BIOS device control */
	ushort scam_tolerant;	/* 08 no scam */

	uchar adapter_scsi_id;	/* 09 Host Adapter ID */
	uchar bios_boot_delay;	/*    power up wait */

	uchar scsi_reset_delay;	/* 10 reset delay */
	uchar bios_id_lun;	/*    first boot device scsi id & lun */
	/*    high nibble is lun */
	/*    low nibble is scsi id */

	uchar termination_se;	/* 11 0 - automatic */
	/*    1 - low off / high off */
	/*    2 - low off / high on */
	/*    3 - low on  / high on */
	/*    There is no low on  / high off */

	uchar termination_lvd;	/* 11 0 - automatic */
	/*    1 - low off / high off */
	/*    2 - low off / high on */
	/*    3 - low on  / high on */
	/*    There is no low on  / high off */

	ushort bios_ctrl;	/* 12 BIOS control bits */
	/*  bit 0  BIOS don't act as initiator. */
	/*  bit 1  BIOS > 1 GB support */
	/*  bit 2  BIOS > 2 Disk Support */
	/*  bit 3  BIOS don't support removables */
	/*  bit 4  BIOS support bootable CD */
	/*  bit 5  BIOS scan enabled */
	/*  bit 6  BIOS support multiple LUNs */
	/*  bit 7  BIOS display of message */
	/*  bit 8  SCAM disabled */
	/*  bit 9  Reset SCSI bus during init. */
	/*  bit 10 */
	/*  bit 11 No verbose initialization. */
	/*  bit 12 SCSI parity enabled */
	/*  bit 13 */
	/*  bit 14 */
	/*  bit 15 */
	ushort sdtr_speed2;	/* 13 SDTR speed TID 4-7 */
	ushort sdtr_speed3;	/* 14 SDTR speed TID 8-11 */
	uchar max_host_qng;	/* 15 maximum host queueing */
	uchar max_dvc_qng;	/*    maximum per device queuing */
	ushort dvc_cntl;	/* 16 control bit for driver */
	ushort sdtr_speed4;	/* 17 SDTR speed 4 TID 12-15 */
	ushort serial_number_word1;	/* 18 Board serial number word 1 */
	ushort serial_number_word2;	/* 19 Board serial number word 2 */
	ushort serial_number_word3;	/* 20 Board serial number word 3 */
	ushort check_sum;	/* 21 EEP check sum */
	uchar oem_name[16];	/* 22 OEM name */
	ushort dvc_err_code;	/* 30 last device driver error code */
	ushort adv_err_code;	/* 31 last uc and Adv Lib error code */
	ushort adv_err_addr;	/* 32 last uc error address */
	ushort saved_dvc_err_code;	/* 33 saved last dev. driver error code   */
	ushort saved_adv_err_code;	/* 34 saved last uc and Adv Lib error code */
	ushort saved_adv_err_addr;	/* 35 saved last uc error address         */
	ushort reserved36;	/* 36 reserved */
	ushort reserved37;	/* 37 reserved */
	ushort reserved38;	/* 38 reserved */
	ushort reserved39;	/* 39 reserved */
	ushort reserved40;	/* 40 reserved */
	ushort reserved41;	/* 41 reserved */
	ushort reserved42;	/* 42 reserved */
	ushort reserved43;	/* 43 reserved */
	ushort reserved44;	/* 44 reserved */
	ushort reserved45;	/* 45 reserved */
	ushort reserved46;	/* 46 reserved */
	ushort reserved47;	/* 47 reserved */
	ushort reserved48;	/* 48 reserved */
	ushort reserved49;	/* 49 reserved */
	ushort reserved50;	/* 50 reserved */
	ushort reserved51;	/* 51 reserved */
	ushort reserved52;	/* 52 reserved */
	ushort reserved53;	/* 53 reserved */
	ushort reserved54;	/* 54 reserved */
	ushort reserved55;	/* 55 reserved */
	ushort cisptr_lsw;	/* 56 CIS PTR LSW */
	ushort cisprt_msw;	/* 57 CIS PTR MSW */
	ushort subsysvid;	/* 58 SubSystem Vendor ID */
	ushort subsysid;	/* 59 SubSystem ID */
	ushort reserved60;	/* 60 reserved */
	ushort reserved61;	/* 61 reserved */
	ushort reserved62;	/* 62 reserved */
	ushort reserved63;	/* 63 reserved */
} ADVEEP_38C0800_CONFIG;

typedef struct adveep_38C1600_config {
	/* Word Offset, Description */

	ushort cfg_lsw;		/* 00 power up initialization */
	/*  bit 11 set - Func. 0 INTB, Func. 1 INTA */
	/*       clear - Func. 0 INTA, Func. 1 INTB */
	/*  bit 13 set - Load CIS */
	/*  bit 14 set - BIOS Enable */
	/*  bit 15 set - Big Endian Mode */
	ushort cfg_msw;		/* 01 unused */
	ushort disc_enable;	/* 02 disconnect enable */
	ushort wdtr_able;	/* 03 Wide DTR able */
	ushort sdtr_speed1;	/* 04 SDTR Speed TID 0-3 */
	ushort start_motor;	/* 05 send start up motor */
	ushort tagqng_able;	/* 06 tag queuing able */
	ushort bios_scan;	/* 07 BIOS device control */
	ushort scam_tolerant;	/* 08 no scam */

	uchar adapter_scsi_id;	/* 09 Host Adapter ID */
	uchar bios_boot_delay;	/*    power up wait */

	uchar scsi_reset_delay;	/* 10 reset delay */
	uchar bios_id_lun;	/*    first boot device scsi id & lun */
	/*    high nibble is lun */
	/*    low nibble is scsi id */

	uchar termination_se;	/* 11 0 - automatic */
	/*    1 - low off / high off */
	/*    2 - low off / high on */
	/*    3 - low on  / high on */
	/*    There is no low on  / high off */

	uchar termination_lvd;	/* 11 0 - automatic */
	/*    1 - low off / high off */
	/*    2 - low off / high on */
	/*    3 - low on  / high on */
	/*    There is no low on  / high off */

	ushort bios_ctrl;	/* 12 BIOS control bits */
	/*  bit 0  BIOS don't act as initiator. */
	/*  bit 1  BIOS > 1 GB support */
	/*  bit 2  BIOS > 2 Disk Support */
	/*  bit 3  BIOS don't support removables */
	/*  bit 4  BIOS support bootable CD */
	/*  bit 5  BIOS scan enabled */
	/*  bit 6  BIOS support multiple LUNs */
	/*  bit 7  BIOS display of message */
	/*  bit 8  SCAM disabled */
	/*  bit 9  Reset SCSI bus during init. */
	/*  bit 10 Basic Integrity Checking disabled */
	/*  bit 11 No verbose initialization. */
	/*  bit 12 SCSI parity enabled */
	/*  bit 13 AIPP (Asyn. Info. Ph. Prot.) dis. */
	/*  bit 14 */
	/*  bit 15 */
	ushort sdtr_speed2;	/* 13 SDTR speed TID 4-7 */
	ushort sdtr_speed3;	/* 14 SDTR speed TID 8-11 */
	uchar max_host_qng;	/* 15 maximum host queueing */
	uchar max_dvc_qng;	/*    maximum per device queuing */
	ushort dvc_cntl;	/* 16 control bit for driver */
	ushort sdtr_speed4;	/* 17 SDTR speed 4 TID 12-15 */
	ushort serial_number_word1;	/* 18 Board serial number word 1 */
	ushort serial_number_word2;	/* 19 Board serial number word 2 */
	ushort serial_number_word3;	/* 20 Board serial number word 3 */
	ushort check_sum;	/* 21 EEP check sum */
	uchar oem_name[16];	/* 22 OEM name */
	ushort dvc_err_code;	/* 30 last device driver error code */
	ushort adv_err_code;	/* 31 last uc and Adv Lib error code */
	ushort adv_err_addr;	/* 32 last uc error address */
	ushort saved_dvc_err_code;	/* 33 saved last dev. driver error code   */
	ushort saved_adv_err_code;	/* 34 saved last uc and Adv Lib error code */
	ushort saved_adv_err_addr;	/* 35 saved last uc error address         */
	ushort reserved36;	/* 36 reserved */
	ushort reserved37;	/* 37 reserved */
	ushort reserved38;	/* 38 reserved */
	ushort reserved39;	/* 39 reserved */
	ushort reserved40;	/* 40 reserved */
	ushort reserved41;	/* 41 reserved */
	ushort reserved42;	/* 42 reserved */
	ushort reserved43;	/* 43 reserved */
	ushort reserved44;	/* 44 reserved */
	ushort reserved45;	/* 45 reserved */
	ushort reserved46;	/* 46 reserved */
	ushort reserved47;	/* 47 reserved */
	ushort reserved48;	/* 48 reserved */
	ushort reserved49;	/* 49 reserved */
	ushort reserved50;	/* 50 reserved */
	ushort reserved51;	/* 51 reserved */
	ushort reserved52;	/* 52 reserved */
	ushort reserved53;	/* 53 reserved */
	ushort reserved54;	/* 54 reserved */
	ushort reserved55;	/* 55 reserved */
	ushort cisptr_lsw;	/* 56 CIS PTR LSW */
	ushort cisprt_msw;	/* 57 CIS PTR MSW */
	ushort subsysvid;	/* 58 SubSystem Vendor ID */
	ushort subsysid;	/* 59 SubSystem ID */
	ushort reserved60;	/* 60 reserved */
	ushort reserved61;	/* 61 reserved */
	ushort reserved62;	/* 62 reserved */
	ushort reserved63;	/* 63 reserved */
} ADVEEP_38C1600_CONFIG;

/*
 * EEPROM Commands
 */
#define ASC_EEP_CMD_DONE             0x0200

/* bios_ctrl */
#define BIOS_CTRL_BIOS               0x0001
#define BIOS_CTRL_EXTENDED_XLAT      0x0002
#define BIOS_CTRL_GT_2_DISK          0x0004
#define BIOS_CTRL_BIOS_REMOVABLE     0x0008
#define BIOS_CTRL_BOOTABLE_CD        0x0010
#define BIOS_CTRL_MULTIPLE_LUN       0x0040
#define BIOS_CTRL_DISPLAY_MSG        0x0080
#define BIOS_CTRL_NO_SCAM            0x0100
#define BIOS_CTRL_RESET_SCSI_BUS     0x0200
#define BIOS_CTRL_INIT_VERBOSE       0x0800
#define BIOS_CTRL_SCSI_PARITY        0x1000
#define BIOS_CTRL_AIPP_DIS           0x2000

#define ADV_3550_MEMSIZE   0x2000	/* 8 KB Internal Memory */

#define ADV_38C0800_MEMSIZE  0x4000	/* 16 KB Internal Memory */

/*
 * XXX - Since ASC38C1600 Rev.3 has a local RAM failure issue, there is
 * a special 16K Adv Library and Microcode version. After the issue is
 * resolved, should restore 32K support.
 *
 * #define ADV_38C1600_MEMSIZE  0x8000L   * 32 KB Internal Memory *
 */
#define ADV_38C1600_MEMSIZE  0x4000	/* 16 KB Internal Memory */

/*
 * Byte I/O register address from base of 'iop_base'.
 */
#define IOPB_INTR_STATUS_REG    0x00
#define IOPB_CHIP_ID_1          0x01
#define IOPB_INTR_ENABLES       0x02
#define IOPB_CHIP_TYPE_REV      0x03
#define IOPB_RES_ADDR_4         0x04
#define IOPB_RES_ADDR_5         0x05
#define IOPB_RAM_DATA           0x06
#define IOPB_RES_ADDR_7         0x07
#define IOPB_FLAG_REG           0x08
#define IOPB_RES_ADDR_9         0x09
#define IOPB_RISC_CSR           0x0A
#define IOPB_RES_ADDR_B         0x0B
#define IOPB_RES_ADDR_C         0x0C
#define IOPB_RES_ADDR_D         0x0D
#define IOPB_SOFT_OVER_WR       0x0E
#define IOPB_RES_ADDR_F         0x0F
#define IOPB_MEM_CFG            0x10
#define IOPB_RES_ADDR_11        0x11
#define IOPB_GPIO_DATA          0x12
#define IOPB_RES_ADDR_13        0x13
#define IOPB_FLASH_PAGE         0x14
#define IOPB_RES_ADDR_15        0x15
#define IOPB_GPIO_CNTL          0x16
#define IOPB_RES_ADDR_17        0x17
#define IOPB_FLASH_DATA         0x18
#define IOPB_RES_ADDR_19        0x19
#define IOPB_RES_ADDR_1A        0x1A
#define IOPB_RES_ADDR_1B        0x1B
#define IOPB_RES_ADDR_1C        0x1C
#define IOPB_RES_ADDR_1D        0x1D
#define IOPB_RES_ADDR_1E        0x1E
#define IOPB_RES_ADDR_1F        0x1F
#define IOPB_DMA_CFG0           0x20
#define IOPB_DMA_CFG1           0x21
#define IOPB_TICKLE             0x22
#define IOPB_DMA_REG_WR         0x23
#define IOPB_SDMA_STATUS        0x24
#define IOPB_SCSI_BYTE_CNT      0x25
#define IOPB_HOST_BYTE_CNT      0x26
#define IOPB_BYTE_LEFT_TO_XFER  0x27
#define IOPB_BYTE_TO_XFER_0     0x28
#define IOPB_BYTE_TO_XFER_1     0x29
#define IOPB_BYTE_TO_XFER_2     0x2A
#define IOPB_BYTE_TO_XFER_3     0x2B
#define IOPB_ACC_GRP            0x2C
#define IOPB_RES_ADDR_2D        0x2D
#define IOPB_DEV_ID             0x2E
#define IOPB_RES_ADDR_2F        0x2F
#define IOPB_SCSI_DATA          0x30
#define IOPB_RES_ADDR_31        0x31
#define IOPB_RES_ADDR_32        0x32
#define IOPB_SCSI_DATA_HSHK     0x33
#define IOPB_SCSI_CTRL          0x34
#define IOPB_RES_ADDR_35        0x35
#define IOPB_RES_ADDR_36        0x36
#define IOPB_RES_ADDR_37        0x37
#define IOPB_RAM_BIST           0x38
#define IOPB_PLL_TEST           0x39
#define IOPB_PCI_INT_CFG        0x3A
#define IOPB_RES_ADDR_3B        0x3B
#define IOPB_RFIFO_CNT          0x3C
#define IOPB_RES_ADDR_3D        0x3D
#define IOPB_RES_ADDR_3E        0x3E
#define IOPB_RES_ADDR_3F        0x3F

/*
 * Word I/O register address from base of 'iop_base'.
 */
#define IOPW_CHIP_ID_0          0x00	/* CID0  */
#define IOPW_CTRL_REG           0x02	/* CC    */
#define IOPW_RAM_ADDR           0x04	/* LA    */
#define IOPW_RAM_DATA           0x06	/* LD    */
#define IOPW_RES_ADDR_08        0x08
#define IOPW_RISC_CSR           0x0A	/* CSR   */
#define IOPW_SCSI_CFG0          0x0C	/* CFG0  */
#define IOPW_SCSI_CFG1          0x0E	/* CFG1  */
#define IOPW_RES_ADDR_10        0x10
#define IOPW_SEL_MASK           0x12	/* SM    */
#define IOPW_RES_ADDR_14        0x14
#define IOPW_FLASH_ADDR         0x16	/* FA    */
#define IOPW_RES_ADDR_18        0x18
#define IOPW_EE_CMD             0x1A	/* EC    */
#define IOPW_EE_DATA            0x1C	/* ED    */
#define IOPW_SFIFO_CNT          0x1E	/* SFC   */
#define IOPW_RES_ADDR_20        0x20
#define IOPW_Q_BASE             0x22	/* QB    */
#define IOPW_QP                 0x24	/* QP    */
#define IOPW_IX                 0x26	/* IX    */
#define IOPW_SP                 0x28	/* SP    */
#define IOPW_PC                 0x2A	/* PC    */
#define IOPW_RES_ADDR_2C        0x2C
#define IOPW_RES_ADDR_2E        0x2E
#define IOPW_SCSI_DATA          0x30	/* SD    */
#define IOPW_SCSI_DATA_HSHK     0x32	/* SDH   */
#define IOPW_SCSI_CTRL          0x34	/* SC    */
#define IOPW_HSHK_CFG           0x36	/* HCFG  */
#define IOPW_SXFR_STATUS        0x36	/* SXS   */
#define IOPW_SXFR_CNTL          0x38	/* SXL   */
#define IOPW_SXFR_CNTH          0x3A	/* SXH   */
#define IOPW_RES_ADDR_3C        0x3C
#define IOPW_RFIFO_DATA         0x3E	/* RFD   */

/*
 * Doubleword I/O register address from base of 'iop_base'.
 */
#define IOPDW_RES_ADDR_0         0x00
#define IOPDW_RAM_DATA           0x04
#define IOPDW_RES_ADDR_8         0x08
#define IOPDW_RES_ADDR_C         0x0C
#define IOPDW_RES_ADDR_10        0x10
#define IOPDW_COMMA              0x14
#define IOPDW_COMMB              0x18
#define IOPDW_RES_ADDR_1C        0x1C
#define IOPDW_SDMA_ADDR0         0x20
#define IOPDW_SDMA_ADDR1         0x24
#define IOPDW_SDMA_COUNT         0x28
#define IOPDW_SDMA_ERROR         0x2C
#define IOPDW_RDMA_ADDR0         0x30
#define IOPDW_RDMA_ADDR1         0x34
#define IOPDW_RDMA_COUNT         0x38
#define IOPDW_RDMA_ERROR         0x3C

#define ADV_CHIP_ID_BYTE         0x25
#define ADV_CHIP_ID_WORD         0x04C1

#define ADV_INTR_ENABLE_HOST_INTR                   0x01
#define ADV_INTR_ENABLE_SEL_INTR                    0x02
#define ADV_INTR_ENABLE_DPR_INTR                    0x04
#define ADV_INTR_ENABLE_RTA_INTR                    0x08
#define ADV_INTR_ENABLE_RMA_INTR                    0x10
#define ADV_INTR_ENABLE_RST_INTR                    0x20
#define ADV_INTR_ENABLE_DPE_INTR                    0x40
#define ADV_INTR_ENABLE_GLOBAL_INTR                 0x80

#define ADV_INTR_STATUS_INTRA            0x01
#define ADV_INTR_STATUS_INTRB            0x02
#define ADV_INTR_STATUS_INTRC            0x04

#define ADV_RISC_CSR_STOP           (0x0000)
#define ADV_RISC_TEST_COND          (0x2000)
#define ADV_RISC_CSR_RUN            (0x4000)
#define ADV_RISC_CSR_SINGLE_STEP    (0x8000)

#define ADV_CTRL_REG_HOST_INTR      0x0100
#define ADV_CTRL_REG_SEL_INTR       0x0200
#define ADV_CTRL_REG_DPR_INTR       0x0400
#define ADV_CTRL_REG_RTA_INTR       0x0800
#define ADV_CTRL_REG_RMA_INTR       0x1000
#define ADV_CTRL_REG_RES_BIT14      0x2000
#define ADV_CTRL_REG_DPE_INTR       0x4000
#define ADV_CTRL_REG_POWER_DONE     0x8000
#define ADV_CTRL_REG_ANY_INTR       0xFF00

#define ADV_CTRL_REG_CMD_RESET             0x00C6
#define ADV_CTRL_REG_CMD_WR_IO_REG         0x00C5
#define ADV_CTRL_REG_CMD_RD_IO_REG         0x00C4
#define ADV_CTRL_REG_CMD_WR_PCI_CFG_SPACE  0x00C3
#define ADV_CTRL_REG_CMD_RD_PCI_CFG_SPACE  0x00C2

#define ADV_TICKLE_NOP                      0x00
#define ADV_TICKLE_A                        0x01
#define ADV_TICKLE_B                        0x02
#define ADV_TICKLE_C                        0x03

#define AdvIsIntPending(port) \
    (AdvReadWordRegister(port, IOPW_CTRL_REG) & ADV_CTRL_REG_HOST_INTR)

/*
 * SCSI_CFG0 Register bit definitions
 */
#define TIMER_MODEAB    0xC000	/* Watchdog, Second, and Select. Timer Ctrl. */
#define PARITY_EN       0x2000	/* Enable SCSI Parity Error detection */
#define EVEN_PARITY     0x1000	/* Select Even Parity */
#define WD_LONG         0x0800	/* Watchdog Interval, 1: 57 min, 0: 13 sec */
#define QUEUE_128       0x0400	/* Queue Size, 1: 128 byte, 0: 64 byte */
#define PRIM_MODE       0x0100	/* Primitive SCSI mode */
#define SCAM_EN         0x0080	/* Enable SCAM selection */
#define SEL_TMO_LONG    0x0040	/* Sel/Resel Timeout, 1: 400 ms, 0: 1.6 ms */
#define CFRM_ID         0x0020	/* SCAM id sel. confirm., 1: fast, 0: 6.4 ms */
#define OUR_ID_EN       0x0010	/* Enable OUR_ID bits */
#define OUR_ID          0x000F	/* SCSI ID */

/*
 * SCSI_CFG1 Register bit definitions
 */
#define BIG_ENDIAN      0x8000	/* Enable Big Endian Mode MIO:15, EEP:15 */
#define TERM_POL        0x2000	/* Terminator Polarity Ctrl. MIO:13, EEP:13 */
#define SLEW_RATE       0x1000	/* SCSI output buffer slew rate */
#define FILTER_SEL      0x0C00	/* Filter Period Selection */
#define  FLTR_DISABLE    0x0000	/* Input Filtering Disabled */
#define  FLTR_11_TO_20NS 0x0800	/* Input Filtering 11ns to 20ns */
#define  FLTR_21_TO_39NS 0x0C00	/* Input Filtering 21ns to 39ns */
#define ACTIVE_DBL      0x0200	/* Disable Active Negation */
#define DIFF_MODE       0x0100	/* SCSI differential Mode (Read-Only) */
#define DIFF_SENSE      0x0080	/* 1: No SE cables, 0: SE cable (Read-Only) */
#define TERM_CTL_SEL    0x0040	/* Enable TERM_CTL_H and TERM_CTL_L */
#define TERM_CTL        0x0030	/* External SCSI Termination Bits */
#define  TERM_CTL_H      0x0020	/* Enable External SCSI Upper Termination */
#define  TERM_CTL_L      0x0010	/* Enable External SCSI Lower Termination */
#define CABLE_DETECT    0x000F	/* External SCSI Cable Connection Status */

/*
 * Addendum for ASC-38C0800 Chip
 *
 * The ASC-38C1600 Chip uses the same definitions except that the
 * bus mode override bits [12:10] have been moved to byte register
 * offset 0xE (IOPB_SOFT_OVER_WR) bits [12:10]. The [12:10] bits in
 * SCSI_CFG1 are read-only and always available. Bit 14 (DIS_TERM_DRV)
 * is not needed. The [12:10] bits in IOPB_SOFT_OVER_WR are write-only.
 * Also each ASC-38C1600 function or channel uses only cable bits [5:4]
 * and [1:0]. Bits [14], [7:6], [3:2] are unused.
 */
#define DIS_TERM_DRV    0x4000	/* 1: Read c_det[3:0], 0: cannot read */
#define HVD_LVD_SE      0x1C00	/* Device Detect Bits */
#define  HVD             0x1000	/* HVD Device Detect */
#define  LVD             0x0800	/* LVD Device Detect */
#define  SE              0x0400	/* SE Device Detect */
#define TERM_LVD        0x00C0	/* LVD Termination Bits */
#define  TERM_LVD_HI     0x0080	/* Enable LVD Upper Termination */
#define  TERM_LVD_LO     0x0040	/* Enable LVD Lower Termination */
#define TERM_SE         0x0030	/* SE Termination Bits */
#define  TERM_SE_HI      0x0020	/* Enable SE Upper Termination */
#define  TERM_SE_LO      0x0010	/* Enable SE Lower Termination */
#define C_DET_LVD       0x000C	/* LVD Cable Detect Bits */
#define  C_DET3          0x0008	/* Cable Detect for LVD External Wide */
#define  C_DET2          0x0004	/* Cable Detect for LVD Internal Wide */
#define C_DET_SE        0x0003	/* SE Cable Detect Bits */
#define  C_DET1          0x0002	/* Cable Detect for SE Internal Wide */
#define  C_DET0          0x0001	/* Cable Detect for SE Internal Narrow */

#define CABLE_ILLEGAL_A 0x7
    /* x 0 0 0  | on  on | Illegal (all 3 connectors are used) */

#define CABLE_ILLEGAL_B 0xB
    /* 0 x 0 0  | on  on | Illegal (all 3 connectors are used) */

/*
 * MEM_CFG Register bit definitions
 */
#define BIOS_EN         0x40	/* BIOS Enable MIO:14,EEP:14 */
#define FAST_EE_CLK     0x20	/* Diagnostic Bit */
#define RAM_SZ          0x1C	/* Specify size of RAM to RISC */
#define  RAM_SZ_2KB      0x00	/* 2 KB */
#define  RAM_SZ_4KB      0x04	/* 4 KB */
#define  RAM_SZ_8KB      0x08	/* 8 KB */
#define  RAM_SZ_16KB     0x0C	/* 16 KB */
#define  RAM_SZ_32KB     0x10	/* 32 KB */
#define  RAM_SZ_64KB     0x14	/* 64 KB */

/*
 * DMA_CFG0 Register bit definitions
 *
 * This register is only accessible to the host.
 */
#define BC_THRESH_ENB   0x80	/* PCI DMA Start Conditions */
#define FIFO_THRESH     0x70	/* PCI DMA FIFO Threshold */
#define  FIFO_THRESH_16B  0x00	/* 16 bytes */
#define  FIFO_THRESH_32B  0x20	/* 32 bytes */
#define  FIFO_THRESH_48B  0x30	/* 48 bytes */
#define  FIFO_THRESH_64B  0x40	/* 64 bytes */
#define  FIFO_THRESH_80B  0x50	/* 80 bytes (default) */
#define  FIFO_THRESH_96B  0x60	/* 96 bytes */
#define  FIFO_THRESH_112B 0x70	/* 112 bytes */
#define START_CTL       0x0C	/* DMA start conditions */
#define  START_CTL_TH    0x00	/* Wait threshold level (default) */
#define  START_CTL_ID    0x04	/* Wait SDMA/SBUS idle */
#define  START_CTL_THID  0x08	/* Wait threshold and SDMA/SBUS idle */
#define  START_CTL_EMFU  0x0C	/* Wait SDMA FIFO empty/full */
#define READ_CMD        0x03	/* Memory Read Method */
#define  READ_CMD_MR     0x00	/* Memory Read */
#define  READ_CMD_MRL    0x02	/* Memory Read Long */
#define  READ_CMD_MRM    0x03	/* Memory Read Multiple (default) */

/*
 * ASC-38C0800 RAM BIST Register bit definitions
 */
#define RAM_TEST_MODE         0x80
#define PRE_TEST_MODE         0x40
#define NORMAL_MODE           0x00
#define RAM_TEST_DONE         0x10
#define RAM_TEST_STATUS       0x0F
#define  RAM_TEST_HOST_ERROR   0x08
#define  RAM_TEST_INTRAM_ERROR 0x04
#define  RAM_TEST_RISC_ERROR   0x02
#define  RAM_TEST_SCSI_ERROR   0x01
#define  RAM_TEST_SUCCESS      0x00
#define PRE_TEST_VALUE        0x05
#define NORMAL_VALUE          0x00

/*
 * ASC38C1600 Definitions
 *
 * IOPB_PCI_INT_CFG Bit Field Definitions
 */

#define INTAB_LD        0x80	/* Value loaded from EEPROM Bit 11. */

/*
 * Bit 1 can be set to change the interrupt for the Function to operate in
 * Totem Pole mode. By default Bit 1 is 0 and the interrupt operates in
 * Open Drain mode. Both functions of the ASC38C1600 must be set to the same
 * mode, otherwise the operating mode is undefined.
 */
#define TOTEMPOLE       0x02

/*
 * Bit 0 can be used to change the Int Pin for the Function. The value is
 * 0 by default for both Functions with Function 0 using INT A and Function
 * B using INT B. For Function 0 if set, INT B is used. For Function 1 if set,
 * INT A is used.
 *
 * EEPROM Word 0 Bit 11 for each Function may change the initial Int Pin
 * value specified in the PCI Configuration Space.
 */
#define INTAB           0x01

/*
 * Adv Library Status Definitions
 */
#define ADV_TRUE        1
#define ADV_FALSE       0
#define ADV_SUCCESS     1
#define ADV_BUSY        0
#define ADV_ERROR       (-1)

/*
 * ADV_DVC_VAR 'warn_code' values
 */
#define ASC_WARN_BUSRESET_ERROR         0x0001	/* SCSI Bus Reset error */
#define ASC_WARN_EEPROM_CHKSUM          0x0002	/* EEP check sum error */
#define ASC_WARN_EEPROM_TERMINATION     0x0004	/* EEP termination bad field */
#define ASC_WARN_ERROR                  0xFFFF	/* ADV_ERROR return */

#define ADV_MAX_TID                     15	/* max. target identifier */
#define ADV_MAX_LUN                     7	/* max. logical unit number */

/*
 * Fixed locations of microcode operating variables.
 */
#define ASC_MC_CODE_BEGIN_ADDR          0x0028	/* microcode start address */
#define ASC_MC_CODE_END_ADDR            0x002A	/* microcode end address */
#define ASC_MC_CODE_CHK_SUM             0x002C	/* microcode code checksum */
#define ASC_MC_VERSION_DATE             0x0038	/* microcode version */
#define ASC_MC_VERSION_NUM              0x003A	/* microcode number */
#define ASC_MC_BIOSMEM                  0x0040	/* BIOS RISC Memory Start */
#define ASC_MC_BIOSLEN                  0x0050	/* BIOS RISC Memory Length */
#define ASC_MC_BIOS_SIGNATURE           0x0058	/* BIOS Signature 0x55AA */
#define ASC_MC_BIOS_VERSION             0x005A	/* BIOS Version (2 bytes) */
#define ASC_MC_SDTR_SPEED1              0x0090	/* SDTR Speed for TID 0-3 */
#define ASC_MC_SDTR_SPEED2              0x0092	/* SDTR Speed for TID 4-7 */
#define ASC_MC_SDTR_SPEED3              0x0094	/* SDTR Speed for TID 8-11 */
#define ASC_MC_SDTR_SPEED4              0x0096	/* SDTR Speed for TID 12-15 */
#define ASC_MC_CHIP_TYPE                0x009A
#define ASC_MC_INTRB_CODE               0x009B
#define ASC_MC_WDTR_ABLE                0x009C
#define ASC_MC_SDTR_ABLE                0x009E
#define ASC_MC_TAGQNG_ABLE              0x00A0
#define ASC_MC_DISC_ENABLE              0x00A2
#define ASC_MC_IDLE_CMD_STATUS          0x00A4
#define ASC_MC_IDLE_CMD                 0x00A6
#define ASC_MC_IDLE_CMD_PARAMETER       0x00A8
#define ASC_MC_DEFAULT_SCSI_CFG0        0x00AC
#define ASC_MC_DEFAULT_SCSI_CFG1        0x00AE
#define ASC_MC_DEFAULT_MEM_CFG          0x00B0
#define ASC_MC_DEFAULT_SEL_MASK         0x00B2
#define ASC_MC_SDTR_DONE                0x00B6
#define ASC_MC_NUMBER_OF_QUEUED_CMD     0x00C0
#define ASC_MC_NUMBER_OF_MAX_CMD        0x00D0
#define ASC_MC_DEVICE_HSHK_CFG_TABLE    0x0100
#define ASC_MC_CONTROL_FLAG             0x0122	/* Microcode control flag. */
#define ASC_MC_WDTR_DONE                0x0124
#define ASC_MC_CAM_MODE_MASK            0x015E	/* CAM mode TID bitmask. */
#define ASC_MC_ICQ                      0x0160
#define ASC_MC_IRQ                      0x0164
#define ASC_MC_PPR_ABLE                 0x017A

/*
 * BIOS LRAM variable absolute offsets.
 */
#define BIOS_CODESEG    0x54
#define BIOS_CODELEN    0x56
#define BIOS_SIGNATURE  0x58
#define BIOS_VERSION    0x5A

/*
 * Microcode Control Flags
 *
 * Flags set by the Adv Library in RISC variable 'control_flag' (0x122)
 * and handled by the microcode.
 */
#define CONTROL_FLAG_IGNORE_PERR        0x0001	/* Ignore DMA Parity Errors */
#define CONTROL_FLAG_ENABLE_AIPP        0x0002	/* Enabled AIPP checking. */

/*
 * ASC_MC_DEVICE_HSHK_CFG_TABLE microcode table or HSHK_CFG register format
 */
#define HSHK_CFG_WIDE_XFR       0x8000
#define HSHK_CFG_RATE           0x0F00
#define HSHK_CFG_OFFSET         0x001F

#define ASC_DEF_MAX_HOST_QNG    0xFD	/* Max. number of host commands (253) */
#define ASC_DEF_MIN_HOST_QNG    0x10	/* Min. number of host commands (16) */
#define ASC_DEF_MAX_DVC_QNG     0x3F	/* Max. number commands per device (63) */
#define ASC_DEF_MIN_DVC_QNG     0x04	/* Min. number commands per device (4) */

#define ASC_QC_DATA_CHECK  0x01	/* Require ASC_QC_DATA_OUT set or clear. */
#define ASC_QC_DATA_OUT    0x02	/* Data out DMA transfer. */
#define ASC_QC_START_MOTOR 0x04	/* Send auto-start motor before request. */
#define ASC_QC_NO_OVERRUN  0x08	/* Don't report overrun. */
#define ASC_QC_FREEZE_TIDQ 0x10	/* Freeze TID queue after request. XXX TBD */

#define ASC_QSC_NO_DISC     0x01	/* Don't allow disconnect for request. */
#define ASC_QSC_NO_TAGMSG   0x02	/* Don't allow tag queuing for request. */
#define ASC_QSC_NO_SYNC     0x04	/* Don't use Synch. transfer on request. */
#define ASC_QSC_NO_WIDE     0x08	/* Don't use Wide transfer on request. */
#define ASC_QSC_REDO_DTR    0x10	/* Renegotiate WDTR/SDTR before request. */
/*
 * Note: If a Tag Message is to be sent and neither ASC_QSC_HEAD_TAG or
 * ASC_QSC_ORDERED_TAG is set, then a Simple Tag Message (0x20) is used.
 */
#define ASC_QSC_HEAD_TAG    0x40	/* Use Head Tag Message (0x21). */
#define ASC_QSC_ORDERED_TAG 0x80	/* Use Ordered Tag Message (0x22). */

/*
 * All fields here are accessed by the board microcode and need to be
 * little-endian.
 */
typedef struct adv_carr_t {
	ADV_VADDR carr_va;	/* Carrier Virtual Address */
	ADV_PADDR carr_pa;	/* Carrier Physical Address */
	ADV_VADDR areq_vpa;	/* ASC_SCSI_REQ_Q Virtual or Physical Address */
	/*
	 * next_vpa [31:4]            Carrier Virtual or Physical Next Pointer
	 *
	 * next_vpa [3:1]             Reserved Bits
	 * next_vpa [0]               Done Flag set in Response Queue.
	 */
	ADV_VADDR next_vpa;
} ADV_CARR_T;

/*
 * Mask used to eliminate low 4 bits of carrier 'next_vpa' field.
 */
#define ASC_NEXT_VPA_MASK       0xFFFFFFF0

#define ASC_RQ_DONE             0x00000001
#define ASC_RQ_GOOD             0x00000002
#define ASC_CQ_STOPPER          0x00000000

#define ASC_GET_CARRP(carrp) ((carrp) & ASC_NEXT_VPA_MASK)

#define ADV_CARRIER_NUM_PAGE_CROSSING \
    (((ADV_CARRIER_COUNT * sizeof(ADV_CARR_T)) + (PAGE_SIZE - 1))/PAGE_SIZE)

#define ADV_CARRIER_BUFSIZE \
    ((ADV_CARRIER_COUNT + ADV_CARRIER_NUM_PAGE_CROSSING) * sizeof(ADV_CARR_T))

/*
 * ASC_SCSI_REQ_Q 'a_flag' definitions
 *
 * The Adv Library should limit use to the lower nibble (4 bits) of
 * a_flag. Drivers are free to use the upper nibble (4 bits) of a_flag.
 */
#define ADV_POLL_REQUEST                0x01	/* poll for request completion */
#define ADV_SCSIQ_DONE                  0x02	/* request done */
#define ADV_DONT_RETRY                  0x08	/* don't do retry */

#define ADV_CHIP_ASC3550          0x01	/* Ultra-Wide IC */
#define ADV_CHIP_ASC38C0800       0x02	/* Ultra2-Wide/LVD IC */
#define ADV_CHIP_ASC38C1600       0x03	/* Ultra3-Wide/LVD2 IC */

/*
 * Adapter temporary configuration structure
 *
 * This structure can be discarded after initialization. Don't add
 * fields here needed after initialization.
 *
 * Field naming convention:
 *
 *  *_enable indicates the field enables or disables a feature. The
 *  value of the field is never reset.
 */
typedef struct adv_dvc_cfg {
	ushort disc_enable;	/* enable disconnection */
	uchar chip_version;	/* chip version */
	uchar termination;	/* Term. Ctrl. bits 6-5 of SCSI_CFG1 register */
	ushort control_flag;	/* Microcode Control Flag */
	ushort mcode_date;	/* Microcode date */
	ushort mcode_version;	/* Microcode version */
	ushort serial1;		/* EEPROM serial number word 1 */
	ushort serial2;		/* EEPROM serial number word 2 */
	ushort serial3;		/* EEPROM serial number word 3 */
} ADV_DVC_CFG;

struct adv_dvc_var;
struct adv_scsi_req_q;

typedef struct asc_sg_block {
	uchar reserved1;
	uchar reserved2;
	uchar reserved3;
	uchar sg_cnt;		/* Valid entries in block. */
	ADV_PADDR sg_ptr;	/* Pointer to next sg block. */
	struct {
		ADV_PADDR sg_addr;	/* SG element address. */
		ADV_DCNT sg_count;	/* SG element count. */
	} sg_list[NO_OF_SG_PER_BLOCK];
} ADV_SG_BLOCK;

/*
 * ADV_SCSI_REQ_Q - microcode request structure
 *
 * All fields in this structure up to byte 60 are used by the microcode.
 * The microcode makes assumptions about the size and ordering of fields
 * in this structure. Do not change the structure definition here without
 * coordinating the change with the microcode.
 *
 * All fields accessed by microcode must be maintained in little_endian
 * order.
 */
typedef struct adv_scsi_req_q {
	uchar cntl;		/* Ucode flags and state (ASC_MC_QC_*). */
	uchar target_cmd;
	uchar target_id;	/* Device target identifier. */
	uchar target_lun;	/* Device target logical unit number. */
	ADV_PADDR data_addr;	/* Data buffer physical address. */
	ADV_DCNT data_cnt;	/* Data count. Ucode sets to residual. */
	ADV_PADDR sense_addr;
	ADV_PADDR carr_pa;
	uchar mflag;
	uchar sense_len;
	uchar cdb_len;		/* SCSI CDB length. Must <= 16 bytes. */
	uchar scsi_cntl;
	uchar done_status;	/* Completion status. */
	uchar scsi_status;	/* SCSI status byte. */
	uchar host_status;	/* Ucode host status. */
	uchar sg_working_ix;
	uchar cdb[12];		/* SCSI CDB bytes 0-11. */
	ADV_PADDR sg_real_addr;	/* SG list physical address. */
	ADV_PADDR scsiq_rptr;
	uchar cdb16[4];		/* SCSI CDB bytes 12-15. */
	ADV_VADDR scsiq_ptr;
	ADV_VADDR carr_va;
	/*
	 * End of microcode structure - 60 bytes. The rest of the structure
	 * is used by the Adv Library and ignored by the microcode.
	 */
	ADV_VADDR srb_ptr;
	ADV_SG_BLOCK *sg_list_ptr;	/* SG list virtual address. */
	char *vdata_addr;	/* Data buffer virtual address. */
	uchar a_flag;
	uchar pad[2];		/* Pad out to a word boundary. */
} ADV_SCSI_REQ_Q;

/*
 * The following two structures are used to process Wide Board requests.
 *
 * The ADV_SCSI_REQ_Q structure in adv_req_t is passed to the Adv Library
 * and microcode with the ADV_SCSI_REQ_Q field 'srb_ptr' pointing to the
 * adv_req_t. The adv_req_t structure 'cmndp' field in turn points to the
 * Mid-Level SCSI request structure.
 *
 * Zero or more ADV_SG_BLOCK are used with each ADV_SCSI_REQ_Q. Each
 * ADV_SG_BLOCK structure holds 15 scatter-gather elements. Under Linux
 * up to 255 scatter-gather elements may be used per request or
 * ADV_SCSI_REQ_Q.
 *
 * Both structures must be 32 byte aligned.
 */
typedef struct adv_sgblk {
	ADV_SG_BLOCK sg_block;	/* Sgblock structure. */
	uchar align[32];	/* Sgblock structure padding. */
	struct adv_sgblk *next_sgblkp;	/* Next scatter-gather structure. */
} adv_sgblk_t;

typedef struct adv_req {
	ADV_SCSI_REQ_Q scsi_req_q;	/* Adv Library request structure. */
	uchar align[32];	/* Request structure padding. */
	struct scsi_cmnd *cmndp;	/* Mid-Level SCSI command pointer. */
	adv_sgblk_t *sgblkp;	/* Adv Library scatter-gather pointer. */
	struct adv_req *next_reqp;	/* Next Request Structure. */
} adv_req_t;

/*
 * Adapter operation variable structure.
 *
 * One structure is required per host adapter.
 *
 * Field naming convention:
 *
 *  *_able indicates both whether a feature should be enabled or disabled
 *  and whether a device isi capable of the feature. At initialization
 *  this field may be set, but later if a device is found to be incapable
 *  of the feature, the field is cleared.
 */
typedef struct adv_dvc_var {
	AdvPortAddr iop_base;	/* I/O port address */
	ushort err_code;	/* fatal error code */
	ushort bios_ctrl;	/* BIOS control word, EEPROM word 12 */
	ushort wdtr_able;	/* try WDTR for a device */
	ushort sdtr_able;	/* try SDTR for a device */
	ushort ultra_able;	/* try SDTR Ultra speed for a device */
	ushort sdtr_speed1;	/* EEPROM SDTR Speed for TID 0-3   */
	ushort sdtr_speed2;	/* EEPROM SDTR Speed for TID 4-7   */
	ushort sdtr_speed3;	/* EEPROM SDTR Speed for TID 8-11  */
	ushort sdtr_speed4;	/* EEPROM SDTR Speed for TID 12-15 */
	ushort tagqng_able;	/* try tagged queuing with a device */
	ushort ppr_able;	/* PPR message capable per TID bitmask. */
	uchar max_dvc_qng;	/* maximum number of tagged commands per device */
	ushort start_motor;	/* start motor command allowed */
	uchar scsi_reset_wait;	/* delay in seconds after scsi bus reset */
	uchar chip_no;		/* should be assigned by caller */
	uchar max_host_qng;	/* maximum number of Q'ed command allowed */
	ushort no_scam;		/* scam_tolerant of EEPROM */
	struct asc_board *drv_ptr;	/* driver pointer to private structure */
	uchar chip_scsi_id;	/* chip SCSI target ID */
	uchar chip_type;
	uchar bist_err_code;
	ADV_CARR_T *carrier_buf;
	ADV_CARR_T *carr_freelist;	/* Carrier free list. */
	ADV_CARR_T *icq_sp;	/* Initiator command queue stopper pointer. */
	ADV_CARR_T *irq_sp;	/* Initiator response queue stopper pointer. */
	ushort carr_pending_cnt;	/* Count of pending carriers. */
	struct adv_req *orig_reqp;	/* adv_req_t memory block. */
	/*
	 * Note: The following fields will not be used after initialization. The
	 * driver may discard the buffer after initialization is done.
	 */
	ADV_DVC_CFG *cfg;	/* temporary configuration structure  */
} ADV_DVC_VAR;

/*
 * Microcode idle loop commands
 */
#define IDLE_CMD_COMPLETED           0
#define IDLE_CMD_STOP_CHIP           0x0001
#define IDLE_CMD_STOP_CHIP_SEND_INT  0x0002
#define IDLE_CMD_SEND_INT            0x0004
#define IDLE_CMD_ABORT               0x0008
#define IDLE_CMD_DEVICE_RESET        0x0010
#define IDLE_CMD_SCSI_RESET_START    0x0020	/* Assert SCSI Bus Reset */
#define IDLE_CMD_SCSI_RESET_END      0x0040	/* Deassert SCSI Bus Reset */
#define IDLE_CMD_SCSIREQ             0x0080

#define IDLE_CMD_STATUS_SUCCESS      0x0001
#define IDLE_CMD_STATUS_FAILURE      0x0002

/*
 * AdvSendIdleCmd() flag definitions.
 */
#define ADV_NOWAIT     0x01

/*
 * Wait loop time out values.
 */
#define SCSI_WAIT_100_MSEC           100UL	/* 100 milliseconds */
#define SCSI_US_PER_MSEC             1000	/* microseconds per millisecond */
#define SCSI_MAX_RETRY               10	/* retry count */

#define ADV_ASYNC_RDMA_FAILURE          0x01	/* Fatal RDMA failure. */
#define ADV_ASYNC_SCSI_BUS_RESET_DET    0x02	/* Detected SCSI Bus Reset. */
#define ADV_ASYNC_CARRIER_READY_FAILURE 0x03	/* Carrier Ready failure. */
#define ADV_RDMA_IN_CARR_AND_Q_INVALID  0x04	/* RDMAed-in data invalid. */

#define ADV_HOST_SCSI_BUS_RESET      0x80	/* Host Initiated SCSI Bus Reset. */

/* Read byte from a register. */
#define AdvReadByteRegister(iop_base, reg_off) \
     (ADV_MEM_READB((iop_base) + (reg_off)))

/* Write byte to a register. */
#define AdvWriteByteRegister(iop_base, reg_off, byte) \
     (ADV_MEM_WRITEB((iop_base) + (reg_off), (byte)))

/* Read word (2 bytes) from a register. */
#define AdvReadWordRegister(iop_base, reg_off) \
     (ADV_MEM_READW((iop_base) + (reg_off)))

/* Write word (2 bytes) to a register. */
#define AdvWriteWordRegister(iop_base, reg_off, word) \
     (ADV_MEM_WRITEW((iop_base) + (reg_off), (word)))

/* Write dword (4 bytes) to a register. */
#define AdvWriteDWordRegister(iop_base, reg_off, dword) \
     (ADV_MEM_WRITEDW((iop_base) + (reg_off), (dword)))

/* Read byte from LRAM. */
#define AdvReadByteLram(iop_base, addr, byte) \
do { \
    ADV_MEM_WRITEW((iop_base) + IOPW_RAM_ADDR, (addr)); \
    (byte) = ADV_MEM_READB((iop_base) + IOPB_RAM_DATA); \
} while (0)

/* Write byte to LRAM. */
#define AdvWriteByteLram(iop_base, addr, byte) \
    (ADV_MEM_WRITEW((iop_base) + IOPW_RAM_ADDR, (addr)), \
     ADV_MEM_WRITEB((iop_base) + IOPB_RAM_DATA, (byte)))

/* Read word (2 bytes) from LRAM. */
#define AdvReadWordLram(iop_base, addr, word) \
do { \
    ADV_MEM_WRITEW((iop_base) + IOPW_RAM_ADDR, (addr)); \
    (word) = (ADV_MEM_READW((iop_base) + IOPW_RAM_DATA)); \
} while (0)

/* Write word (2 bytes) to LRAM. */
#define AdvWriteWordLram(iop_base, addr, word) \
    (ADV_MEM_WRITEW((iop_base) + IOPW_RAM_ADDR, (addr)), \
     ADV_MEM_WRITEW((iop_base) + IOPW_RAM_DATA, (word)))

/* Write little-endian double word (4 bytes) to LRAM */
/* Because of unspecified C language ordering don't use auto-increment. */
#define AdvWriteDWordLramNoSwap(iop_base, addr, dword) \
    ((ADV_MEM_WRITEW((iop_base) + IOPW_RAM_ADDR, (addr)), \
      ADV_MEM_WRITEW((iop_base) + IOPW_RAM_DATA, \
                     cpu_to_le16((ushort) ((dword) & 0xFFFF)))), \
     (ADV_MEM_WRITEW((iop_base) + IOPW_RAM_ADDR, (addr) + 2), \
      ADV_MEM_WRITEW((iop_base) + IOPW_RAM_DATA, \
                     cpu_to_le16((ushort) ((dword >> 16) & 0xFFFF)))))

/* Read word (2 bytes) from LRAM assuming that the address is already set. */
#define AdvReadWordAutoIncLram(iop_base) \
     (ADV_MEM_READW((iop_base) + IOPW_RAM_DATA))

/* Write word (2 bytes) to LRAM assuming that the address is already set. */
#define AdvWriteWordAutoIncLram(iop_base, word) \
     (ADV_MEM_WRITEW((iop_base) + IOPW_RAM_DATA, (word)))

/*
 * Define macro to check for Condor signature.
 *
 * Evaluate to ADV_TRUE if a Condor chip is found the specified port
 * address 'iop_base'. Otherwise evalue to ADV_FALSE.
 */
#define AdvFindSignature(iop_base) \
    (((AdvReadByteRegister((iop_base), IOPB_CHIP_ID_1) == \
    ADV_CHIP_ID_BYTE) && \
     (AdvReadWordRegister((iop_base), IOPW_CHIP_ID_0) == \
    ADV_CHIP_ID_WORD)) ?  ADV_TRUE : ADV_FALSE)

/*
 * Define macro to Return the version number of the chip at 'iop_base'.
 *
 * The second parameter 'bus_type' is currently unused.
 */
#define AdvGetChipVersion(iop_base, bus_type) \
    AdvReadByteRegister((iop_base), IOPB_CHIP_TYPE_REV)

/*
 * Abort an SRB in the chip's RISC Memory. The 'srb_ptr' argument must
 * match the ASC_SCSI_REQ_Q 'srb_ptr' field.
 *
 * If the request has not yet been sent to the device it will simply be
 * aborted from RISC memory. If the request is disconnected it will be
 * aborted on reselection by sending an Abort Message to the target ID.
 *
 * Return value:
 *      ADV_TRUE(1) - Queue was successfully aborted.
 *      ADV_FALSE(0) - Queue was not found on the active queue list.
 */
#define AdvAbortQueue(asc_dvc, scsiq) \
        AdvSendIdleCmd((asc_dvc), (ushort) IDLE_CMD_ABORT, \
                       (ADV_DCNT) (scsiq))

/*
 * Send a Bus Device Reset Message to the specified target ID.
 *
 * All outstanding commands will be purged if sending the
 * Bus Device Reset Message is successful.
 *
 * Return Value:
 *      ADV_TRUE(1) - All requests on the target are purged.
 *      ADV_FALSE(0) - Couldn't issue Bus Device Reset Message; Requests
 *                     are not purged.
 */
#define AdvResetDevice(asc_dvc, target_id) \
        AdvSendIdleCmd((asc_dvc), (ushort) IDLE_CMD_DEVICE_RESET, \
                    (ADV_DCNT) (target_id))

/*
 * SCSI Wide Type definition.
 */
#define ADV_SCSI_BIT_ID_TYPE   ushort

/*
 * AdvInitScsiTarget() 'cntl_flag' options.
 */
#define ADV_SCAN_LUN           0x01
#define ADV_CAPINFO_NOLUN      0x02

/*
 * Convert target id to target id bit mask.
 */
#define ADV_TID_TO_TIDMASK(tid)   (0x01 << ((tid) & ADV_MAX_TID))

/*
 * ASC_SCSI_REQ_Q 'done_status' and 'host_status' return values.
 */

#define QD_NO_STATUS         0x00	/* Request not completed yet. */
#define QD_NO_ERROR          0x01
#define QD_ABORTED_BY_HOST   0x02
#define QD_WITH_ERROR        0x04

#define QHSTA_NO_ERROR              0x00
#define QHSTA_M_SEL_TIMEOUT         0x11
#define QHSTA_M_DATA_OVER_RUN       0x12
#define QHSTA_M_UNEXPECTED_BUS_FREE 0x13
#define QHSTA_M_QUEUE_ABORTED       0x15
#define QHSTA_M_SXFR_SDMA_ERR       0x16	/* SXFR_STATUS SCSI DMA Error */
#define QHSTA_M_SXFR_SXFR_PERR      0x17	/* SXFR_STATUS SCSI Bus Parity Error */
#define QHSTA_M_RDMA_PERR           0x18	/* RISC PCI DMA parity error */
#define QHSTA_M_SXFR_OFF_UFLW       0x19	/* SXFR_STATUS Offset Underflow */
#define QHSTA_M_SXFR_OFF_OFLW       0x20	/* SXFR_STATUS Offset Overflow */
#define QHSTA_M_SXFR_WD_TMO         0x21	/* SXFR_STATUS Watchdog Timeout */
#define QHSTA_M_SXFR_DESELECTED     0x22	/* SXFR_STATUS Deselected */
/* Note: QHSTA_M_SXFR_XFR_OFLW is identical to QHSTA_M_DATA_OVER_RUN. */
#define QHSTA_M_SXFR_XFR_OFLW       0x12	/* SXFR_STATUS Transfer Overflow */
#define QHSTA_M_SXFR_XFR_PH_ERR     0x24	/* SXFR_STATUS Transfer Phase Error */
#define QHSTA_M_SXFR_UNKNOWN_ERROR  0x25	/* SXFR_STATUS Unknown Error */
#define QHSTA_M_SCSI_BUS_RESET      0x30	/* Request aborted from SBR */
#define QHSTA_M_SCSI_BUS_RESET_UNSOL 0x31	/* Request aborted from unsol. SBR */
#define QHSTA_M_BUS_DEVICE_RESET    0x32	/* Request aborted from BDR */
#define QHSTA_M_DIRECTION_ERR       0x35	/* Data Phase mismatch */
#define QHSTA_M_DIRECTION_ERR_HUNG  0x36	/* Data Phase mismatch and bus hang */
#define QHSTA_M_WTM_TIMEOUT         0x41
#define QHSTA_M_BAD_CMPL_STATUS_IN  0x42
#define QHSTA_M_NO_AUTO_REQ_SENSE   0x43
#define QHSTA_M_AUTO_REQ_SENSE_FAIL 0x44
#define QHSTA_M_INVALID_DEVICE      0x45	/* Bad target ID */
#define QHSTA_M_FROZEN_TIDQ         0x46	/* TID Queue frozen. */
#define QHSTA_M_SGBACKUP_ERROR      0x47	/* Scatter-Gather backup error */

/* Return the address that is aligned at the next doubleword >= to 'addr'. */
#define ADV_8BALIGN(addr)      (((ulong) (addr) + 0x7) & ~0x7)
#define ADV_16BALIGN(addr)     (((ulong) (addr) + 0xF) & ~0xF)
#define ADV_32BALIGN(addr)     (((ulong) (addr) + 0x1F) & ~0x1F)

/*
 * Total contiguous memory needed for driver SG blocks.
 *
 * ADV_MAX_SG_LIST must be defined by a driver. It is the maximum
 * number of scatter-gather elements the driver supports in a
 * single request.
 */

#define ADV_SG_LIST_MAX_BYTE_SIZE \
         (sizeof(ADV_SG_BLOCK) * \
          ((ADV_MAX_SG_LIST + (NO_OF_SG_PER_BLOCK - 1))/NO_OF_SG_PER_BLOCK))

/* struct asc_board flags */
#define ASC_IS_WIDE_BOARD       0x04	/* AdvanSys Wide Board */

#define ASC_NARROW_BOARD(boardp) (((boardp)->flags & ASC_IS_WIDE_BOARD) == 0)

#define NO_ISA_DMA              0xff	/* No ISA DMA Channel Used */

#define ASC_INFO_SIZE           128	/* advansys_info() line size */

#ifdef CONFIG_PROC_FS
/* /proc/scsi/advansys/[0...] related definitions */
#define ASC_PRTBUF_SIZE         2048
#define ASC_PRTLINE_SIZE        160

#define ASC_PRT_NEXT() \
    if (cp) { \
        totlen += len; \
        leftlen -= len; \
        if (leftlen == 0) { \
            return totlen; \
        } \
        cp += len; \
    }
#endif /* CONFIG_PROC_FS */

/* Asc Library return codes */
#define ASC_TRUE        1
#define ASC_FALSE       0
#define ASC_NOERROR     1
#define ASC_BUSY        0
#define ASC_ERROR       (-1)

/* struct scsi_cmnd function return codes */
#define STATUS_BYTE(byte)   (byte)
#define MSG_BYTE(byte)      ((byte) << 8)
#define HOST_BYTE(byte)     ((byte) << 16)
#define DRIVER_BYTE(byte)   ((byte) << 24)

#define ASC_STATS(shost, counter) ASC_STATS_ADD(shost, counter, 1)
#ifndef ADVANSYS_STATS
#define ASC_STATS_ADD(shost, counter, count)
#else /* ADVANSYS_STATS */
#define ASC_STATS_ADD(shost, counter, count) \
	(((struct asc_board *) shost_priv(shost))->asc_stats.counter += (count))
#endif /* ADVANSYS_STATS */

/* If the result wraps when calculating tenths, return 0. */
#define ASC_TENTHS(num, den) \
    (((10 * ((num)/(den))) > (((num) * 10)/(den))) ? \
    0 : ((((num) * 10)/(den)) - (10 * ((num)/(den)))))

/*
 * Display a message to the console.
 */
#define ASC_PRINT(s) \
    { \
        printk("advansys: "); \
        printk(s); \
    }

#define ASC_PRINT1(s, a1) \
    { \
        printk("advansys: "); \
        printk((s), (a1)); \
    }

#define ASC_PRINT2(s, a1, a2) \
    { \
        printk("advansys: "); \
        printk((s), (a1), (a2)); \
    }

#define ASC_PRINT3(s, a1, a2, a3) \
    { \
        printk("advansys: "); \
        printk((s), (a1), (a2), (a3)); \
    }

#define ASC_PRINT4(s, a1, a2, a3, a4) \
    { \
        printk("advansys: "); \
        printk((s), (a1), (a2), (a3), (a4)); \
    }

#ifndef ADVANSYS_DEBUG

#define ASC_DBG(lvl, s...)
#define ASC_DBG_PRT_SCSI_HOST(lvl, s)
#define ASC_DBG_PRT_ASC_SCSI_Q(lvl, scsiqp)
#define ASC_DBG_PRT_ADV_SCSI_REQ_Q(lvl, scsiqp)
#define ASC_DBG_PRT_ASC_QDONE_INFO(lvl, qdone)
#define ADV_DBG_PRT_ADV_SCSI_REQ_Q(lvl, scsiqp)
#define ASC_DBG_PRT_HEX(lvl, name, start, length)
#define ASC_DBG_PRT_CDB(lvl, cdb, len)
#define ASC_DBG_PRT_SENSE(lvl, sense, len)
#define ASC_DBG_PRT_INQUIRY(lvl, inq, len)

#else /* ADVANSYS_DEBUG */

/*
 * Debugging Message Levels:
 * 0: Errors Only
 * 1: High-Level Tracing
 * 2-N: Verbose Tracing
 */

#define ASC_DBG(lvl, format, arg...) {					\
	if (asc_dbglvl >= (lvl))					\
		printk(KERN_DEBUG "%s: %s: " format, DRV_NAME,		\
			__func__ , ## arg);				\
}

#define ASC_DBG_PRT_SCSI_HOST(lvl, s) \
    { \
        if (asc_dbglvl >= (lvl)) { \
            asc_prt_scsi_host(s); \
        } \
    }

#define ASC_DBG_PRT_ASC_SCSI_Q(lvl, scsiqp) \
    { \
        if (asc_dbglvl >= (lvl)) { \
            asc_prt_asc_scsi_q(scsiqp); \
        } \
    }

#define ASC_DBG_PRT_ASC_QDONE_INFO(lvl, qdone) \
    { \
        if (asc_dbglvl >= (lvl)) { \
            asc_prt_asc_qdone_info(qdone); \
        } \
    }

#define ASC_DBG_PRT_ADV_SCSI_REQ_Q(lvl, scsiqp) \
    { \
        if (asc_dbglvl >= (lvl)) { \
            asc_prt_adv_scsi_req_q(scsiqp); \
        } \
    }

#define ASC_DBG_PRT_HEX(lvl, name, start, length) \
    { \
        if (asc_dbglvl >= (lvl)) { \
            asc_prt_hex((name), (start), (length)); \
        } \
    }

#define ASC_DBG_PRT_CDB(lvl, cdb, len) \
        ASC_DBG_PRT_HEX((lvl), "CDB", (uchar *) (cdb), (len));

#define ASC_DBG_PRT_SENSE(lvl, sense, len) \
        ASC_DBG_PRT_HEX((lvl), "SENSE", (uchar *) (sense), (len));

#define ASC_DBG_PRT_INQUIRY(lvl, inq, len) \
        ASC_DBG_PRT_HEX((lvl), "INQUIRY", (uchar *) (inq), (len));
#endif /* ADVANSYS_DEBUG */

#ifdef ADVANSYS_STATS

/* Per board statistics structure */
struct asc_stats {
	/* Driver Entrypoint Statistics */
	ADV_DCNT queuecommand;	/* # calls to advansys_queuecommand() */
	ADV_DCNT reset;		/* # calls to advansys_eh_bus_reset() */
	ADV_DCNT biosparam;	/* # calls to advansys_biosparam() */
	ADV_DCNT interrupt;	/* # advansys_interrupt() calls */
	ADV_DCNT callback;	/* # calls to asc/adv_isr_callback() */
	ADV_DCNT done;		/* # calls to request's scsi_done function */
	ADV_DCNT build_error;	/* # asc/adv_build_req() ASC_ERROR returns. */
	ADV_DCNT adv_build_noreq;	/* # adv_build_req() adv_req_t alloc. fail. */
	ADV_DCNT adv_build_nosg;	/* # adv_build_req() adv_sgblk_t alloc. fail. */
	/* AscExeScsiQueue()/AdvExeScsiQueue() Statistics */
	ADV_DCNT exe_noerror;	/* # ASC_NOERROR returns. */
	ADV_DCNT exe_busy;	/* # ASC_BUSY returns. */
	ADV_DCNT exe_error;	/* # ASC_ERROR returns. */
	ADV_DCNT exe_unknown;	/* # unknown returns. */
	/* Data Transfer Statistics */
	ADV_DCNT xfer_cnt;	/* # I/O requests received */
	ADV_DCNT xfer_elem;	/* # scatter-gather elements */
	ADV_DCNT xfer_sect;	/* # 512-byte blocks */
};
#endif /* ADVANSYS_STATS */

/*
 * Structure allocated for each board.
 *
 * This structure is allocated by scsi_host_alloc() at the end
 * of the 'Scsi_Host' structure starting at the 'hostdata'
 * field. It is guaranteed to be allocated from DMA-able memory.
 */
struct asc_board {
	struct device *dev;
	uint flags;		/* Board flags */
	unsigned int irq;
	union {
		ASC_DVC_VAR asc_dvc_var;	/* Narrow board */
		ADV_DVC_VAR adv_dvc_var;	/* Wide board */
	} dvc_var;
	union {
		ASC_DVC_CFG asc_dvc_cfg;	/* Narrow board */
		ADV_DVC_CFG adv_dvc_cfg;	/* Wide board */
	} dvc_cfg;
	ushort asc_n_io_port;	/* Number I/O ports. */
	ADV_SCSI_BIT_ID_TYPE init_tidmask;	/* Target init./valid mask */
	ushort reqcnt[ADV_MAX_TID + 1];	/* Starvation request count */
	ADV_SCSI_BIT_ID_TYPE queue_full;	/* Queue full mask */
	ushort queue_full_cnt[ADV_MAX_TID + 1];	/* Queue full count */
	union {
		ASCEEP_CONFIG asc_eep;	/* Narrow EEPROM config. */
		ADVEEP_3550_CONFIG adv_3550_eep;	/* 3550 EEPROM config. */
		ADVEEP_38C0800_CONFIG adv_38C0800_eep;	/* 38C0800 EEPROM config. */
		ADVEEP_38C1600_CONFIG adv_38C1600_eep;	/* 38C1600 EEPROM config. */
	} eep_config;
	ulong last_reset;	/* Saved last reset time */
	/* /proc/scsi/advansys/[0...] */
	char *prtbuf;		/* /proc print buffer */
#ifdef ADVANSYS_STATS
	struct asc_stats asc_stats;	/* Board statistics */
#endif				/* ADVANSYS_STATS */
	/*
	 * The following fields are used only for Narrow Boards.
	 */
	uchar sdtr_data[ASC_MAX_TID + 1];	/* SDTR information */
	/*
	 * The following fields are used only for Wide Boards.
	 */
	void __iomem *ioremap_addr;	/* I/O Memory remap address. */
	ushort ioport;		/* I/O Port address. */
	adv_req_t *adv_reqp;	/* Request structures. */
	adv_sgblk_t *adv_sgblkp;	/* Scatter-gather structures. */
	ushort bios_signature;	/* BIOS Signature. */
	ushort bios_version;	/* BIOS Version. */
	ushort bios_codeseg;	/* BIOS Code Segment. */
	ushort bios_codelen;	/* BIOS Code Segment Length. */
};

#define asc_dvc_to_board(asc_dvc) container_of(asc_dvc, struct asc_board, \
							dvc_var.asc_dvc_var)
#define adv_dvc_to_board(adv_dvc) container_of(adv_dvc, struct asc_board, \
							dvc_var.adv_dvc_var)
#define adv_dvc_to_pdev(adv_dvc) to_pci_dev(adv_dvc_to_board(adv_dvc)->dev)

#ifdef ADVANSYS_DEBUG
static int asc_dbglvl = 3;

/*
 * asc_prt_asc_dvc_var()
 */
static void asc_prt_asc_dvc_var(ASC_DVC_VAR *h)
{
	printk("ASC_DVC_VAR at addr 0x%lx\n", (ulong)h);

	printk(" iop_base 0x%x, err_code 0x%x, dvc_cntl 0x%x, bug_fix_cntl "
	       "%d,\n", h->iop_base, h->err_code, h->dvc_cntl, h->bug_fix_cntl);

	printk(" bus_type %d, init_sdtr 0x%x,\n", h->bus_type,
		(unsigned)h->init_sdtr);

	printk(" sdtr_done 0x%x, use_tagged_qng 0x%x, unit_not_ready 0x%x, "
	       "chip_no 0x%x,\n", (unsigned)h->sdtr_done,
	       (unsigned)h->use_tagged_qng, (unsigned)h->unit_not_ready,
	       (unsigned)h->chip_no);

	printk(" queue_full_or_busy 0x%x, start_motor 0x%x, scsi_reset_wait "
	       "%u,\n", (unsigned)h->queue_full_or_busy,
	       (unsigned)h->start_motor, (unsigned)h->scsi_reset_wait);

	printk(" is_in_int %u, max_total_qng %u, cur_total_qng %u, "
	       "in_critical_cnt %u,\n", (unsigned)h->is_in_int,
	       (unsigned)h->max_total_qng, (unsigned)h->cur_total_qng,
	       (unsigned)h->in_critical_cnt);

	printk(" last_q_shortage %u, init_state 0x%x, no_scam 0x%x, "
	       "pci_fix_asyn_xfer 0x%x,\n", (unsigned)h->last_q_shortage,
	       (unsigned)h->init_state, (unsigned)h->no_scam,
	       (unsigned)h->pci_fix_asyn_xfer);

	printk(" cfg 0x%lx\n", (ulong)h->cfg);
}

/*
 * asc_prt_asc_dvc_cfg()
 */
static void asc_prt_asc_dvc_cfg(ASC_DVC_CFG *h)
{
	printk("ASC_DVC_CFG at addr 0x%lx\n", (ulong)h);

	printk(" can_tagged_qng 0x%x, cmd_qng_enabled 0x%x,\n",
	       h->can_tagged_qng, h->cmd_qng_enabled);
	printk(" disc_enable 0x%x, sdtr_enable 0x%x,\n",
	       h->disc_enable, h->sdtr_enable);

	printk(" chip_scsi_id %d, isa_dma_speed %d, isa_dma_channel %d, "
		"chip_version %d,\n", h->chip_scsi_id, h->isa_dma_speed,
		h->isa_dma_channel, h->chip_version);

	printk(" mcode_date 0x%x, mcode_version %d\n",
		h->mcode_date, h->mcode_version);
}

/*
 * asc_prt_adv_dvc_var()
 *
 * Display an ADV_DVC_VAR structure.
 */
static void asc_prt_adv_dvc_var(ADV_DVC_VAR *h)
{
	printk(" ADV_DVC_VAR at addr 0x%lx\n", (ulong)h);

	printk("  iop_base 0x%lx, err_code 0x%x, ultra_able 0x%x\n",
	       (ulong)h->iop_base, h->err_code, (unsigned)h->ultra_able);

	printk("  sdtr_able 0x%x, wdtr_able 0x%x\n",
	       (unsigned)h->sdtr_able, (unsigned)h->wdtr_able);

	printk("  start_motor 0x%x, scsi_reset_wait 0x%x\n",
	       (unsigned)h->start_motor, (unsigned)h->scsi_reset_wait);

	printk("  max_host_qng %u, max_dvc_qng %u, carr_freelist 0x%lxn\n",
	       (unsigned)h->max_host_qng, (unsigned)h->max_dvc_qng,
	       (ulong)h->carr_freelist);

	printk("  icq_sp 0x%lx, irq_sp 0x%lx\n",
	       (ulong)h->icq_sp, (ulong)h->irq_sp);

	printk("  no_scam 0x%x, tagqng_able 0x%x\n",
	       (unsigned)h->no_scam, (unsigned)h->tagqng_able);

	printk("  chip_scsi_id 0x%x, cfg 0x%lx\n",
	       (unsigned)h->chip_scsi_id, (ulong)h->cfg);
}

/*
 * asc_prt_adv_dvc_cfg()
 *
 * Display an ADV_DVC_CFG structure.
 */
static void asc_prt_adv_dvc_cfg(ADV_DVC_CFG *h)
{
	printk(" ADV_DVC_CFG at addr 0x%lx\n", (ulong)h);

	printk("  disc_enable 0x%x, termination 0x%x\n",
	       h->disc_enable, h->termination);

	printk("  chip_version 0x%x, mcode_date 0x%x\n",
	       h->chip_version, h->mcode_date);

	printk("  mcode_version 0x%x, control_flag 0x%x\n",
	       h->mcode_version, h->control_flag);
}

/*
 * asc_prt_scsi_host()
 */
static void asc_prt_scsi_host(struct Scsi_Host *s)
{
	struct asc_board *boardp = shost_priv(s);

	printk("Scsi_Host at addr 0x%p, device %s\n", s, dev_name(boardp->dev));
	printk(" host_busy %u, host_no %d, last_reset %d,\n",
	       s->host_busy, s->host_no, (unsigned)s->last_reset);

	printk(" base 0x%lx, io_port 0x%lx, irq %d,\n",
	       (ulong)s->base, (ulong)s->io_port, boardp->irq);

	printk(" dma_channel %d, this_id %d, can_queue %d,\n",
	       s->dma_channel, s->this_id, s->can_queue);

	printk(" cmd_per_lun %d, sg_tablesize %d, unchecked_isa_dma %d\n",
	       s->cmd_per_lun, s->sg_tablesize, s->unchecked_isa_dma);

	if (ASC_NARROW_BOARD(boardp)) {
		asc_prt_asc_dvc_var(&boardp->dvc_var.asc_dvc_var);
		asc_prt_asc_dvc_cfg(&boardp->dvc_cfg.asc_dvc_cfg);
	} else {
		asc_prt_adv_dvc_var(&boardp->dvc_var.adv_dvc_var);
		asc_prt_adv_dvc_cfg(&boardp->dvc_cfg.adv_dvc_cfg);
	}
}

/*
 * asc_prt_hex()
 *
 * Print hexadecimal output in 4 byte groupings 32 bytes
 * or 8 double-words per line.
 */
static void asc_prt_hex(char *f, uchar *s, int l)
{
	int i;
	int j;
	int k;
	int m;

	printk("%s: (%d bytes)\n", f, l);

	for (i = 0; i < l; i += 32) {

		/* Display a maximum of 8 double-words per line. */
		if ((k = (l - i) / 4) >= 8) {
			k = 8;
			m = 0;
		} else {
			m = (l - i) % 4;
		}

		for (j = 0; j < k; j++) {
			printk(" %2.2X%2.2X%2.2X%2.2X",
			       (unsigned)s[i + (j * 4)],
			       (unsigned)s[i + (j * 4) + 1],
			       (unsigned)s[i + (j * 4) + 2],
			       (unsigned)s[i + (j * 4) + 3]);
		}

		switch (m) {
		case 0:
		default:
			break;
		case 1:
			printk(" %2.2X", (unsigned)s[i + (j * 4)]);
			break;
		case 2:
			printk(" %2.2X%2.2X",
			       (unsigned)s[i + (j * 4)],
			       (unsigned)s[i + (j * 4) + 1]);
			break;
		case 3:
			printk(" %2.2X%2.2X%2.2X",
			       (unsigned)s[i + (j * 4) + 1],
			       (unsigned)s[i + (j * 4) + 2],
			       (unsigned)s[i + (j * 4) + 3]);
			break;
		}

		printk("\n");
	}
}

/*
 * asc_prt_asc_scsi_q()
 */
static void asc_prt_asc_scsi_q(ASC_SCSI_Q *q)
{
	ASC_SG_HEAD *sgp;
	int i;

	printk("ASC_SCSI_Q at addr 0x%lx\n", (ulong)q);

	printk
	    (" target_ix 0x%x, target_lun %u, srb_ptr 0x%lx, tag_code 0x%x,\n",
	     q->q2.target_ix, q->q1.target_lun, (ulong)q->q2.srb_ptr,
	     q->q2.tag_code);

	printk
	    (" data_addr 0x%lx, data_cnt %lu, sense_addr 0x%lx, sense_len %u,\n",
	     (ulong)le32_to_cpu(q->q1.data_addr),
	     (ulong)le32_to_cpu(q->q1.data_cnt),
	     (ulong)le32_to_cpu(q->q1.sense_addr), q->q1.sense_len);

	printk(" cdbptr 0x%lx, cdb_len %u, sg_head 0x%lx, sg_queue_cnt %u\n",
	       (ulong)q->cdbptr, q->q2.cdb_len,
	       (ulong)q->sg_head, q->q1.sg_queue_cnt);

	if (q->sg_head) {
		sgp = q->sg_head;
		printk("ASC_SG_HEAD at addr 0x%lx\n", (ulong)sgp);
		printk(" entry_cnt %u, queue_cnt %u\n", sgp->entry_cnt,
		       sgp->queue_cnt);
		for (i = 0; i < sgp->entry_cnt; i++) {
			printk(" [%u]: addr 0x%lx, bytes %lu\n",
			       i, (ulong)le32_to_cpu(sgp->sg_list[i].addr),
			       (ulong)le32_to_cpu(sgp->sg_list[i].bytes));
		}

	}
}

/*
 * asc_prt_asc_qdone_info()
 */
static void asc_prt_asc_qdone_info(ASC_QDONE_INFO *q)
{
	printk("ASC_QDONE_INFO at addr 0x%lx\n", (ulong)q);
	printk(" srb_ptr 0x%lx, target_ix %u, cdb_len %u, tag_code %u,\n",
	       (ulong)q->d2.srb_ptr, q->d2.target_ix, q->d2.cdb_len,
	       q->d2.tag_code);
	printk
	    (" done_stat 0x%x, host_stat 0x%x, scsi_stat 0x%x, scsi_msg 0x%x\n",
	     q->d3.done_stat, q->d3.host_stat, q->d3.scsi_stat, q->d3.scsi_msg);
}

/*
 * asc_prt_adv_sgblock()
 *
 * Display an ADV_SG_BLOCK structure.
 */
static void asc_prt_adv_sgblock(int sgblockno, ADV_SG_BLOCK *b)
{
	int i;

	printk(" ASC_SG_BLOCK at addr 0x%lx (sgblockno %d)\n",
	       (ulong)b, sgblockno);
	printk("  sg_cnt %u, sg_ptr 0x%lx\n",
	       b->sg_cnt, (ulong)le32_to_cpu(b->sg_ptr));
	BUG_ON(b->sg_cnt > NO_OF_SG_PER_BLOCK);
	if (b->sg_ptr != 0)
		BUG_ON(b->sg_cnt != NO_OF_SG_PER_BLOCK);
	for (i = 0; i < b->sg_cnt; i++) {
		printk("  [%u]: sg_addr 0x%lx, sg_count 0x%lx\n",
		       i, (ulong)b->sg_list[i].sg_addr,
		       (ulong)b->sg_list[i].sg_count);
	}
}

/*
 * asc_prt_adv_scsi_req_q()
 *
 * Display an ADV_SCSI_REQ_Q structure.
 */
static void asc_prt_adv_scsi_req_q(ADV_SCSI_REQ_Q *q)
{
	int sg_blk_cnt;
	struct asc_sg_block *sg_ptr;

	printk("ADV_SCSI_REQ_Q at addr 0x%lx\n", (ulong)q);

	printk("  target_id %u, target_lun %u, srb_ptr 0x%lx, a_flag 0x%x\n",
	       q->target_id, q->target_lun, (ulong)q->srb_ptr, q->a_flag);

	printk("  cntl 0x%x, data_addr 0x%lx, vdata_addr 0x%lx\n",
	       q->cntl, (ulong)le32_to_cpu(q->data_addr), (ulong)q->vdata_addr);

	printk("  data_cnt %lu, sense_addr 0x%lx, sense_len %u,\n",
	       (ulong)le32_to_cpu(q->data_cnt),
	       (ulong)le32_to_cpu(q->sense_addr), q->sense_len);

	printk
	    ("  cdb_len %u, done_status 0x%x, host_status 0x%x, scsi_status 0x%x\n",
	     q->cdb_len, q->done_status, q->host_status, q->scsi_status);

	printk("  sg_working_ix 0x%x, target_cmd %u\n",
	       q->sg_working_ix, q->target_cmd);

	printk("  scsiq_rptr 0x%lx, sg_real_addr 0x%lx, sg_list_ptr 0x%lx\n",
	       (ulong)le32_to_cpu(q->scsiq_rptr),
	       (ulong)le32_to_cpu(q->sg_real_addr), (ulong)q->sg_list_ptr);

	/* Display the request's ADV_SG_BLOCK structures. */
	if (q->sg_list_ptr != NULL) {
		sg_blk_cnt = 0;
		while (1) {
			/*
			 * 'sg_ptr' is a physical address. Convert it to a virtual
			 * address by indexing 'sg_blk_cnt' into the virtual address
			 * array 'sg_list_ptr'.
			 *
			 * XXX - Assumes all SG physical blocks are virtually contiguous.
			 */
			sg_ptr =
			    &(((ADV_SG_BLOCK *)(q->sg_list_ptr))[sg_blk_cnt]);
			asc_prt_adv_sgblock(sg_blk_cnt, sg_ptr);
			if (sg_ptr->sg_ptr == 0) {
				break;
			}
			sg_blk_cnt++;
		}
	}
}
#endif /* ADVANSYS_DEBUG */

/*
 * The advansys chip/microcode contains a 32-bit identifier for each command
 * known as the 'srb'.  I don't know what it stands for.  The driver used
 * to encode the scsi_cmnd pointer by calling virt_to_bus and retrieve it
 * with bus_to_virt.  Now the driver keeps a per-host map of integers to
 * pointers.  It auto-expands when full, unless it can't allocate memory.
 * Note that an srb of 0 is treated specially by the chip/firmware, hence
 * the return of i+1 in this routine, and the corresponding subtraction in
 * the inverse routine.
 */
#define BAD_SRB 0
static u32 advansys_ptr_to_srb(struct asc_dvc_var *asc_dvc, void *ptr)
{
	int i;
	void **new_ptr;

	for (i = 0; i < asc_dvc->ptr_map_count; i++) {
		if (!asc_dvc->ptr_map[i])
			goto out;
	}

	if (asc_dvc->ptr_map_count == 0)
		asc_dvc->ptr_map_count = 1;
	else
		asc_dvc->ptr_map_count *= 2;

	new_ptr = krealloc(asc_dvc->ptr_map,
			asc_dvc->ptr_map_count * sizeof(void *), GFP_ATOMIC);
	if (!new_ptr)
		return BAD_SRB;
	asc_dvc->ptr_map = new_ptr;
 out:
	ASC_DBG(3, "Putting ptr %p into array offset %d\n", ptr, i);
	asc_dvc->ptr_map[i] = ptr;
	return i + 1;
}

static void * advansys_srb_to_ptr(struct asc_dvc_var *asc_dvc, u32 srb)
{
	void *ptr;

	srb--;
	if (srb >= asc_dvc->ptr_map_count) {
		printk("advansys: bad SRB %u, max %u\n", srb,
							asc_dvc->ptr_map_count);
		return NULL;
	}
	ptr = asc_dvc->ptr_map[srb];
	asc_dvc->ptr_map[srb] = NULL;
	ASC_DBG(3, "Returning ptr %p from array offset %d\n", ptr, srb);
	return ptr;
}

/*
 * advansys_info()
 *
 * Return suitable for printing on the console with the argument
 * adapter's configuration information.
 *
 * Note: The information line should not exceed ASC_INFO_SIZE bytes,
 * otherwise the static 'info' array will be overrun.
 */
static const char *advansys_info(struct Scsi_Host *shost)
{
	static char info[ASC_INFO_SIZE];
	struct asc_board *boardp = shost_priv(shost);
	ASC_DVC_VAR *asc_dvc_varp;
	ADV_DVC_VAR *adv_dvc_varp;
	char *busname;
	char *widename = NULL;

	if (ASC_NARROW_BOARD(boardp)) {
		asc_dvc_varp = &boardp->dvc_var.asc_dvc_var;
		ASC_DBG(1, "begin\n");
		if (asc_dvc_varp->bus_type & ASC_IS_ISA) {
			if ((asc_dvc_varp->bus_type & ASC_IS_ISAPNP) ==
			    ASC_IS_ISAPNP) {
				busname = "ISA PnP";
			} else {
				busname = "ISA";
			}
			sprintf(info,
				"AdvanSys SCSI %s: %s: IO 0x%lX-0x%lX, IRQ 0x%X, DMA 0x%X",
				ASC_VERSION, busname,
				(ulong)shost->io_portsys"
#define ASC_VERSION "3 + fineIOADR_GAP - 1sys"
#boardp->irq, ASC_VERdma_channel);
		} else {ys"
if (asc_dvc_varp->bus_type &n */

S_VL)ers
 *	 "advan = "VL"SCSII Adapte
 * Copyright (c) 1995-2000 AdvancedEISAystem Products, IncInc. * Copyright (c) 2000-2001 ConnectCom Solutions, PCIystem Pr
 * matthew@wil.cx>
 * All Rights Reserv_ULTRA)ys"
#    ==re; you can rediststem Prroducts, IncPCI Ultra * CopI Adapters
 * the terms of thGeneral  Copyright tem Products, Inc?GeneralASC_V_printk(KERN_ERR Driver, "unknownE "a "Licens"-2000%d\n", Copyright (c) 1995-200 SCSI the Fsn 2 of(infosys"
#"AdvanSys SCSI %s:. (AdIO 0x%lX-
 * c, IRQ)
 *X"sys"
#fine DRV_NAME "advans define ASC_VERSION "3.4"	/* AdvanSys Driver Version */

/*
 * advansys.c - Linux Host SCSI 
I Adapters
 /*
		 * Wide Adapter Informatione <lie <linMemory-mapped I/O is used instead of<linuspace to accesse <linthe aing.h>ME "t displayde <llinuPersirange. Thel.h>
#ie <linlinuaddressux/tt.h>
#ied throughde <ldriver /proc file.e <li/
		advyright (c = & Linux Hright (.ude <linux/SCSI
 * Cde <linux/i->chip5-2000 andDV_CHIP_ASC3550t underwidects, Inc GNU -ux/s * Coyright (c) 2e <linux/isa.h>
#include <linux/eisa.h>8C080include <linux/pci.h>
#in2clude <linux/spinlude <linux/pci.h>
#in3clude <linu
2000 Advanced Syste Products, Inc. (Ad the
#incluMEM)
 * changed its name to Confine DRV_NAME<linux/pc.
 * On .h>
#include <iop_basnsys"
csi/scsi.h>
#include <scsi/sc +  Linux HCopyn_er Versiansyough all <linux/}
	BUG_ON(strlennced ) >and/or NFO_SIZE SCSfineDBG(1, "enr ve SCSreturn ced ;
}

#ifdef CONFIG_PROC_FS
/*
 *sion.prt_line()
 sing If 'cp'ux/tNULL n 2 oopore <lconsole, otherwiseh are illea buffer.and virR proce0 (c)n 2 oingillegal under
 *     the APl procee <lnumber ofing bytes writtenillegal ire queue procNote:rt_tany single irt(ux/tgrea.h>
thann */
PRTLINEdma_med to ae <lstacking will bl unrrupted. 's[]us() definnux/on a esn't work revert to Ieue /
static re ibus_to_virt()char *buf,Keep buflen, countefmt, ...)
{
	va_list args;
	re iret;
	counts[esn't work rever];
 In tstart(meou, fmtstill p = v00 Advanhen pr,imeou SCSes havea mepproprit work rever SCS
 * bufde <whict unde(void)n 2 of d mod a mess0g plAdapters
 a messminget the irerint 	memcpyget , slure
 *  } In tended thstill proceion i}
 using bus_to_v Linu_devices) and virPare iude <liced lude <l for simulta attachy?
 * appinLinukaround. Test nomory mapping.shouldn a      If it doesn't work rever,ing cf. bus_to_virt() eue processing  this.
 *  2.interacters copies.h>toto_bu. No more it ding 'cplen'ine ADVANSYS. Can a teTATS
* Enablet working. Keep an intertiple simultanstruct Scsi_Host *e, or
 countecpn
 *  /
#uner. I2-bit lCopyrt ar * Linux =Driveron 2v(ASC_Vfor tnt leftlent functto* types mus types mush>
#isng oidt functis noe
 * ty = * for;
	t be u =n Liheck Dtype8, 1an interrupt cp,ne
 * tysys"
   "\nDimult
#include <laram Products, Inc.r poi%d: vers
 * anASC_VERes, tnoap.. APIPRT_NEXT(), an
 * fineNARROW_BOARD( Linux)t unde char, short =ough all righcfg.Copyrighress char, short, aA mapping fVADDR __u32		/* Virtual add<linux/blkdev.h */
#define ASC_D and 32 bits respectively. Pointer "Target IDs Detected: stil		/* Physical/Bu	ram (iheck  i < <linuMAX_TID; i++t undetargeLinux Honit_tidmask0 AdDV#ifn_TO#ifnMASK(ine ASC_ Signed Data count type. */

typed %X,", iMarch #ifndef TRUE
#defmodul* Signed Data count type. */

typed (%X=r poiring.h>) vers char, short_u32		/* Physical/Bus  to fix  be use safe agaD.h>
#inux/stBdefinBIOS
#include <lData Types
 *
 * Any inst.h>
bio 32-bit long or pointer type is assumed
 * for precision or HW defined structures, the following define
 * types must be used. In Linuxushersimajor,ons _SG_lettert types
 * are all consistent at 8, 16, and 32 bits respectively. Pointernd lROM
#defiVersion: ar;

#ifndef TRUE
#de
lude virt_te <l#defisaved a valid signature,ble nroc_l into bele to obtcode segme*    Altab.h>
#.to b/r targeLinux Hine __Q pointe != 0x55Ait undestructure will have to be changedDisabled or Pre-3.1er stilval)   ((((uint)vald 32 bits respectively. Pointers
 ** and#defiei   tlude unsigned int	/*.rt_titux/tpint	/*r from a newer v_SRB2S port address size  */
#define inp(port)                inb(port)
cann a found atstrucConnectCom FTP siest ftp://ftp.cne ASCcom.net/pub port address size  */
#dA mapping fONG_S =
#define ASC_SRnpw(por >> 12) & 0xF*  6.IST hort

#define ASC_IS_ISA    8    (0x0001 suppohort

#define ASC_IS_ISA   (0xFefinetr)

#define PortAddr             %d.%d%c.
 */
#d* anONG_SG_LIST tULTRA   e ASC_I>= 26 ? '?' :C_IS_PCM+ 'A'rt address size  */
#nclude <linCurrine avail uns nd the ASrele Altis 3.1Iaram UWe <linand 3.2e ASC_I2W.h>
iYS_Sde doesn'upt.e quentiatee <linUWSCSI_U2Wort arG_SG>
#incl
 * ned sh< 3 ||x8000)

#= 3 &&_LIST  < 1) ||ENDI  e ASC_CHIP_MIN_VER_VL  = 1IN_Ve ASC_I< ('I' -fine 1)
#define UW_ERR   (uint)(0xFFFF)
#defLicens andNine inpw(pordefind the ASisSC_IS_VL   )

#define ASC_MAX_SG_QUEUE port adval)   ((((uint)valefine inp(port)                inb(poP_MAX   7
#define ASC_MAX_SG_LIST     255val)   ((((uint)val) & (1100
#define PCI_DEVICE_IDAdd serialis.
 *  toAdd module_pabar (c)B2SCSIQ(srAAhing is (port)inER_Pbit 15-9 (7 (0xs)defiword 1eue procSe ASC_N.
 *  undeistR_AS 12 alpha-numeric digitnot wing CHIP_V1 - Prodit l-2000(A,B,C,D..)  Word0:21)
13 (3ine ASSC_CHIP_VE2 - MFG Loc 64 bifine ASC_CHIP_VER_PCI2-10RA_3050  (ASC_CHI3-4_PCI | 0x02ID (0-99)CHIP_VEIP_VER_PC9-0 (103050  (ASC_CHIP_5_PCI | 0x02revi    ((A-JA (0x_VER_PC " (0x47)
#" (ASC_CHIP_VEISA S2SCSIQ(sr#define ASC_MAX__VER1PCI_U#define ASSC_CHIP_VE6 - YearER_EIA (0x47)
#C_MAX_VL_DMA_C8-6RA_3050   &VL_DM2PCI_ (1 (0x7FFFFFFL)7-8 - Weekdefie <lye ASC1-52A (0L_DMA_C5SC_C6 (0x07FFFSC_CHIP9-1VER_         0x08
(A001-Z9ISA  ASC_MAX4SC_CH5efine ASC_SCS Tes 1: Only p | 0x0   (c_BIG_DEBUGhave aine ASC_CHIP_Vkaround. Tes 2:- 1) + 3)
#is m poi_Q pificant fine A (0xFEver /proc statis. HanTRUE0)
#de ASC_CHIP_VE(port     altered to fine ASCFALSEData Types
 *
 * Any get_eeprom_string(_VERY_L*SE_LENnum, ue is asser. I_VERY_Lw,is.
Bus addr(w boards [1]
#defiE<lin!= (
 * Nar)0xAA << 8ne ASC_l proce 14
#defiCS_TYPE  unsigude <linFirstHIP_VE- 6RA_3150  NDIAN   wcturde boards0as no	/alle| 0x02)
#de- 1supt.git.IAN   (0x8(*cuctu'A' + ((w
#defE0<lin    3))de <'H't underine MS_WDTR_LEN is P=Proto-2000#incl	_LIS+_ptr8<asm/dmacp++
#defineManufactu

/* l 0x03)
#- 2nd02

#define A*efinST_PER_Q   7
#def1Ce QS_FRE0   (0x00_CHIP_MAX_VE- 3rd, 4thRA_3150 ne QS_num = 7
#def3Fx0001
#define 0R_Q  CK  / 10fineBACK  %=
#dene QC_SG_SWAP_QUEUE 0x02
#dine QS_DONE      e ASC_CHI- 5ine QC_N   0x08
#define QS_ABOQC_SGATA_IN     e <linSecondHIP_Vefine MS_SDTR_LEN    0x13
#define <linne AS- 6A_OUT    inux/kernelIf (0x21)FL)

#irMSG_OU    setr from th0200)
#la0x02

#dFL)

#define 
 *     If it do7_ENDIAN   (0x8_LEN    0x2 * ext8ne QSrs
 *
#define 8S_ABORTED     0QS_FR6 SCSI Adapters
 *C_SG_SWAP_QUEUEefine QD_ABORTED_BY_HO#defineFFFFFL)
fine - 7th, 8ine QC_NO_CALLBACK   0x01
#d003ine QC_SG_SWAP_QUEU 0x02
#dine QC_SG_HEAx81
#define QD_INVALI  (0x0020)
#dTne QCSG_Sefine MS_SDTR_LEN    0xfine QD7Ffinedefine       s.
 *  - 9A_OUT      0x10
#define QC_URGw02
#deA_IN      10  0x11  0x12ine QC_NO_CALLBACK   0x0    AD       0x04
#defin 0x02
#deine QC_SG_HEAD       0x04
#definALID_DEVICE    0x82
#define QD_ERR_INTERNAL    _LIST_P\0';S_DONull Terminat traport

/*_CALLBAne ASC_MAX__MAXt_spi safe against mulr HW defiIME_US neous callers
  definEEPnd tconfigur_DEVICE_I */
#warning this driver is still not properly converted to the DMA API

/* Enable driver /proc statistics. */
#define ADVANSYS_STATS

/* Enable driver tracing. */
#undef ADVANSYS_DEBUG

/*
 * Portable Data Types
 *
 * Any instHSTA_D_HOST_ABORT2-bit long or pointer type is assumed
 * for precision or HW defined structures, the following d APIsVC_VAR *Copyright (cg define
 * types must be used. In LinuxASCEEP_l
 *   *eQHSTA_M_i; internal
 *   ISAEQ_SCSIsa_or Aspeed[] = { 10, 8, 7, 6, 5, 4, 3, 2 };
#endif /*ET 0x48
#dene QSly sup_LEN  str[13as noCopyright (cnit.h>
#include <linuopyright (;
	e     0x01
#deeep_QHSTA_C_FLAeG_REtypes
 * are all consistent at 8, 16, and 32 bits respectively. Pointers
 * and ldefine Set wilR_AS on Alpha and UltraSPARC.
 */
#define ASC_PADDR __u32		/* Physical/Bus addr_HOLD_TIME_US  60

/*

 * Narro)&ell oing.h>_ced [0],#define AS)
te it and/or_MAXsrb_ptr)

#x01)
#d0001) != 0)

#define PCI_VEN         0x08AdvaPCI_ULTRA G_FLAG_EXT 255

#define ASC_CS_TYPE  unsig
 *   0x80
#define AS5]de <0xBBCHIP_MIN_VER_PCI     (0x09)
#define ASC_CHIP_MAX Default     0x10
Upes.#defdefine-l>
#iring.h>.R_ISA     (0x27)
#define ASCAdapters
 *INT_HOST 0x40
#define ASC_SCSIQ_CPY_BEG            0x08
 1) + 3)
#Notd insent       2
#define ASC_SCSIQ_B_2		/* Signed Data count type. */

typs
 * andltraSP Inc.ID: %u,ltraSPQueue Size      ong typdefine ASC_SCS.
 */
#defifine QHSGETux/eisID(ep),   0xmax_total_qngSC_SCSIQ      8
#agne AQ()
 * macro re-defineQNO                3
#define ASC_SCSIQ_Bcntl)
 *x, no_scamQ_DON vers  0xSCSI22
#deINFO_BE      12
#define ASC_SCSIQ_B_SENSE_LEN         20
#de " ef unsign:#define ASCar;

#ifndef TRUE
#define TRUE     (1)
#SCdif
#ifndef FALSE
#ne UW_ERR   (uint)(0xFFFF)
#define dodd_word(

#define ASC_CS_TB structure will have to be changed aIQ()
 * macro re-definedefine ASC_SCSIQ_B_TARGET_IX      Disne ASC_sfine ASC_SIQ_B_CDB_LEN           28
#define ASC_SCSIQ_B_TAG_CODE          29
#define ASC_SCSIQ_W_VM_ID    cI_ULTRA   fine ribute itdiFLAGnVL   f

#define ERR      (-1)
? 'Y' :ULTRA   'Ne ASC_IS_MCA          SIQ_DONE_STATUS         32
#define ASC_SCSIQ_HOST_STATUS         33
#define ASC_SCSIQ_SCSI_STATUCommCSI_defiingfine AASC_SCSIQ_CDB_BEG             36
#define ASC_SCSIQ_DW_REMAIN_XFER_ADDR 56
#define ASC_SCSIQ_DW_REMAIN_XFER_CNT  60
#definuse_cmdne A_B_FIRST_SG_WK_QP    48
#define ASC_SCSIQ_B_SG_WK_QP          49
#define ASC_SCSIQ_B_SG_WK_IX          50
#define ASC_SCSIQ_W_ALT_DC1           52
#define ASC_SCScurr Motor4
#define ASC_SCSIQ_CDB_BEG             36
#define ASC_SCSIQ_DW_REMAIN_XFER_ADDR 56
#define ASC_SCSIQ_DW_REMAIN_XFER_CNT  60
#definccurr_mG    ASC_SGQ_B_SG_LIST_CNT         6
#define ASC_SGQ_B_SG_CUR_LIST_CNT     7
#define ASC_SGQ_LIST_BEG              8
#define ASC_DEF_SCSI1_QNG    4
#define ASC_MAX_SCynchronous Transferhar;

#ifndef TRUE
#define TRUE     (1)
# ASC_SCSIQ_DW_REMAIN_XFER_ADDR 56
#define ASC_SCSIQ_DW_REMAIN_XFER_CNT  60
#defin    (sdt
#define ASC_STOP_ACK_RISC_STOP      0x03
#define ASC_STOP_CLEAN_UP_BUSY_Q    0x10
#define ASC_STOP_CLEAN_UP_DISC_Q    0x20
#deUS_RESET 0x48
#defin * Copyright (c) 1995-2000 Advancednc.
 * Cofine inp(port)                inb(port)
ltraSPOR_HDMA RAM_Cfine%d MB/SPCI_ULTRA   HSTA_D_LRAM_CMASC_SCSIQ_D_DMA_SPDDR  ]SG_WK_QP          49
#de_M_MICRO_CODE_ERROR_HALT1100
#define PCI_DEVICE_ID	0x1300
#defiD_HOST_ABORT_FAILED       0x23
#define QHSTA_D_EXE_SCSI_Q_FAILED       0x24
#define QHSTA_D_EXE_SCSI_Q_BUSY_TIMEOUT 0x25
#define QHSTA_D_ASPI_NO_BUF_POOL        0x26
#define QHSTA_M_WTM_TIMEOUT         0x41
#define QHSTA_M_BAD_CMPL_STATUS_IN  0x42
#define QHSTA_M_NO_AUTO_REQ_SENSE   tus;
	uchar q_noTA_M_AUTO_REQ_SENSE_FAIL 0x44
#define QHSTA_M_TARGET_STATUS_BUSY  0x45
#define QHSTA_M_BAD_TAG_CODV        0x4de <linux/ig define
 * types must be used. In Linux thei if the *termst_SCS0xA1
#define ASC_FLAG	uch QHS>
#iSTA_M_HUNG_DB_LE de comASC_MAX_CDBude <lN];
	uchar y_ude <lst_sg_list_qp;
	ucha16y_working_sg_qp;
	uchst_sg_listbyte comort, a * Narrort xscsi_VERY_LTYPELRAM_C, 16, anude <linux/init.h>
#include <linux/blkdev.h>
#nclude <linux/isa.h>
#include <linux/eisa.h>
#includer y_first_Q   0x02
#define ASC_dvDB_LENG_BIOnux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/fg_qp;
	uchar d3;
	uchar q_status;
	uchhar y_w_no;
	uchar cs;
	ASC_DCrt x_rein_bytes;
} ASC_QDONE_INFO;	ucha_no;
	uOS_ASYNC_IO    0x04
#define ASC_FLAG_SRB_LINEAR_ADDR  0x08
#define ASC_FLAG_WIN16            0x10
#define ASC_FLAG_WIN32            0x20
#define ASC_FLAG_ISA_OVER_16MB    0x40
e <linux/isa.h>
#include <linux/eisa.h>
#includertn;
ASC_r y_fir->_LEN  _s.
 * _rt x1;
	uchar cntl;
	uchar sense_len;
	uchar extra_bytes;
	uchar res;
	ptr;
	ASC_SG_ude <lD *sg_head;
	ushort remain_sg_edef struct asc_scsi	uchD *sg_head;
	ushort remai   0x40
#define ASC_FLAG_DOS_rtn;
TAG_FLAG_EXTTES               0x10
#define ASC_TAG_FLAG_DISABLE_DISCONNECT        0x04
#define ASC_TAG_FLAG_DISABLE_ASYN_USE_SYN_FIX  0x08
C_MAX_TID)
#define ASC_TIX_TO_LUN(tix)          1
#define ASC_SCSIQ_B_STATUS         _QP          49
#de

typedef struct asc_q_done_info {
	ASC_SCSIQ_2 d2;
	ASC_MAX_TID)
#define ASC_TIX_TO_LUN(tix)                       4
#define ASC_SCSIQ_B_SG_QUEUE_CNT       5
#defiCSIQ_DG_HEAD 80
#defi, shortefine ASC3 i3;
	AS  8
es, te ASC_Q;

typedef righ       5

#define ASC_CS_TYPE  ulock.h>
#include <linux/dma-mapping.h>
#include <linux/fSI_BIOS_REQ_Q;

typedef struct asc_risc_q {
	uchar fwd;
	uchar bwd;
	ASC_SCSIQ_1 i1;
	ASC_SCSIQ_2 i2;
	ASC_SCSIQ_3 i3si_req_qC_SCSIQ_4 i4;
} ASC_RISC_Q;
si_req_qdef struct asISC_SG_LIST_Q;

#define 	uchar seq_no;
	uchar q_no;
	uchar cg_cur_list_cnt;
} ASC_SG_LIST_Q;

typedef struct asc_risc_sg_list_q {
	uchar fwd;
	uchar bwd;
	ASC_SG_LIST_Q sg;
	ASC_SG_LIptr;
	list[7];
} ASC_RISC_SG_LIST_Q;ptr;
	ine ASCQ_ERR_Q_STATUS      ON_CRITICA	uchar seq_no;
	uchar q_no;
	u_scsi_q {
	ASC_SCSIQ_1 q1;
	ASC_SCSIQ_2 q2;
	uchar *cdbptr; =C_Q;

typeuct  0x2io conin_sg_entry_cnt;
	ushort next_sg_index;
} ASC_SCSI_Q;

typedef stru00
#defsi_req_qSC_WARN_IO__lvASC_DCNT  __u32002
#define Aptr;
	N_IRQ_MODIFIED     
	switch _3 r3ERRORc Alt1ASC_uct asc IncLow Off/High   0 * Cobreak if  Alt2CMD_QNG_CONFLICT     0x0010
#nfine ASC_WARN_EEPR3CMD_QNG_CONFLICT    n
#define ASC_WARN_CFGd      :RN_EEPR0CMD_QNG_CONFLICAutoude cfine ASC_WARNC_MAX_CDB_LEN];
	uchar sense[ASC_MIN_SENSE_LEN];
} ASC_SCSI_BIOS_REQ_Q;

typedef struct asc_risc_qSC_WARN_IO_     (%s), SC_SRctrl:G       2C_SCSIQ_3 i3;
	ASSC_WARN_IO_,	/* mstdefine ASC3 i3;
	ASm error *seq_no;
	uchar q_no;
	uchar cntl;
	uchar sg_head_qp;
	uchar sg_list_cnt;
	uchar sg_cur_list_cnt;
} ASC_SG_LIST_Q;

typedef str/* micro code check sum error */
#define ASC_IERR_SET ASC_WARN_IRQ_MODIFIED 
#define ASC_IERR_STARsi_req_qP_CHIP	0x0008	/* start/stop chip failellegal cable connection */
#define ASC_IERR_SINGLE_END_DEVICE	0x0020	/* SE device on DIFF bus */
#deWARN_AUTO_CONFIG     D_CABLE		0x0040	/* Narroptr;
	P_CHIP	0x0008	/* start/stop chip2
#define ASC_SCSIQ_B_TARGET_IX         26
#define ASC_SCSIQ_B_CDB_LEN           28
#define ASC_SCendif
#ifndef FALSE
#ne UW_ERR   (uint)(0xFFFF)
#define i        30
#define ASC_SCSIQ_DONE_STATUS         32
#define ASC_SCSIQ_HOST_STATUS      arn_code'.
 */
#define ASC_WARN_NO_ERROR             0x0000
#define Ae ASC_SCSIQPORT_ROTATE       0x0001
#define ASC_WARN_EEPROM_CHKSUM        0x0002
#define ASC_WARne ASC_MIN_FREE_Q    0004
#define ASC_WARN_Ane ASC_MIN_FREE     33
#define ASC_SCSIQ_SCSI_STATUS         34
#define ASC_SCSIQ_CDB_BEG             36
#definee ASC_IERR_BIST_RAM_TEST		0x1000	/* BIST RAM test error EMAIN_XFER_CIP_VEB_FIRST_SG_WK_QP    48
#define AQ_B_SG_WK_QP          49
#define ASC_SCSIQ_B_SG_WK_IX          50
#define ASC_SCSIQ_W_Aarn_code'.
 */
#define ASC_WARN_NO_ERROR             0x0000
#define ASagqng_MIN_FREE_Q        (0x02)
#define ASC_MIN_TOTAL_QNG     ((ASC_MAX_SG_QUEUE)+(ASC_MIN_FREE_y supports a limited 0004
#define ASC_WARN_AU supports a li_ALT_DC1           52
#define ASC_SCSIQ_B_LIST_CNT          6
#define ASC_SCSIQ_B_CUR_LIST_CNT   NG  20
#define ASC_MAX_INRAM_TAG_QNG   16
#define ASC_IOADR_GAP   0x10
#define ASC_SYN_MAX_OFFSET         0x0F
#define ASC_DEF_SDTR_OFFSET        0x0F
#define ASC_SDTR_ULTRA_PCI_10MB_INDEX  0x02
#define ASYN_SDTR_DATA_FIX_PCI_REV_AB 0x41

/* The narrow chip onl      0x01
PORT_ROTATE       0x0001
#define ASC_WARN_EEPROM_CHKSUM        0x0002
#define ASC_WAR		struct {
			uchar s0004
#define ASC_WARN_A		struct {
			uDEF_SCSI1_QNG    4
#define ASC_MAX_SCSI1_QNG    4
#define ASC_DEF_SCSI2_QNG    16
#define ASC_MAXNG  20
#define ASC_MAX_INRAM_TAG_QNG   16
#define ASC_IOADR_GAP   0x10
#define ASC_SYN_MAX_OFFSET         0x0F
#define ASC_DEF_SDTR_OFFSET        0x0F
#define ASC_SDTR_ULTRA_PCI_10MB_INDEX  0x02
#define ASYN_SDTR_DATA_FIX_PCI_REV_AB 0x41

/* Theine ASC_STOP_HOST_REQ_RISC_HALT 0x40
#define ASC_TIDLUN_TO_Ival)   ((((uint)valine TRUE     (1)
#endif
#ifndef FALSE
#AX_INRAM_TAG_QNG   16
#define ASC_IOADR_GAP FER_CNTG_HEAD IT_ID_TYPR x_saCSIQ_B_FIRST_SG_WK_QP    48
#deIT_ID_TYP      0x0F
#deval)   ((((uint)val) &_DONE_STATUS         32
#define ASC_SCSIQ r3;
	uchar cdb[ASC_MAX_CDB_LEN];
	uchar sense[ASC_MIN_SENSE_LEN];
} ASC_SCSI_BIOS_REQ_Q;

typedef struct  "e GNU ASC_TIDLUNefine ASC_S u_ext_msg.mdp_b0

typedef struct asc_dvc_cfg {
	ASC_SCSI_BIT_ID_TYPE can_tagged_qng;
	ASC_SCSI_BIT_ID_TYPE cmd_qng_enabled;
	uGNU SCSI_BIT_ID_TYPE disc_enable;
IT_ID_TYP        0x0F
#der_enable;
	uchar chip_scsi_id;
	uchar isa_dma_speed;
	uchar isa_dma_channel;
	uchar chip_version;
	ushort mcode_date;
	ushort mcode_versio0x0000
#define AwSC_SCSI_PORT_ROTATE       0x0001
#define ASC_WARN_EEPROM_CHKSUM        0x0002
#define ASC_WAR008
#define ASC_INI0004
#define ASC_WARN_A008
#define A     33
#define ASC_SCSIQ_SCSI_STATUux/st	uchar sdtr_peru_ext_msg;
	uchar res;
} EXT_MSG;

#define xfer_period     u_ext_msg.sdtr.sdtr_xfer_period
#define req_ack_offset  u_ext_msg.sdtr.sdtr_req_ack_offset
#define wdtr_width      u_ext_msg.wdtr.wdtr_width
#define mdp_b3          u_ext_msg.mdp_b3
#define mdp_b2          u_ext_msg.md;
	ucha(0x0fine UN_BSIZE		64

struct asc_dvc_var;		/* For	uch sg_cur_list_cnt;
} ASC_SG_LIST_Q;

typedef str40
#define ASC_TIDLU Sed_da(Mhz):\neriod_offset[ASC_MAX_TID + 1];
	uchar adapter_info[6];
} ASC_DVC_CcounteRAM_C 60
fine x02
#iSABLEt under  x_saved_dataion. */

typed x_saved_dremainux/spinlockPE us4_tagged_qng;
	ASC_SCSI_BIT_ID_TYPE unit_not_r2ady;
	ASC_SCSI_BIT_ID8_tagged_qng;
	ASC_SCSI_BIT_ID_TYPE unit_not_r3ady;
	ASC_SCSI_BIT_ID    agged_qng;
	ASC_SCSI_BIT_ID_TYPE unit_not_r4arch 8, 2000008
#d x_saved_daf

#deif
#ifnE sdtr_dn {ASC/AD2000SC_SCSI Inc#define e ASC_WARN
	uchar_CMD__q_shortage;
	u  5rt init_state;
	uchar OM_REvc_qng[ASC_MAX_T10rt init_state;
	uchar RECOVvc_qng[ASC_MAX_T2	ASC_SCSI_Q *scsiq_busy4head[ASC_MAX_TID + 14	ASC_SCSI_Q *scsiq_busy5head[ASC_MAX_TID + 18	ASC_SCSI_Q *scsiqs are set d[ASC_MAX_TID + Unkrt init_state;
	uthe FIS_PCI          (0x0004)
#define AX:%s odd_,) >> A 60
SC_CHIP_MIN_VER_ISA     (SI_BIT_ID7chip_no;structure will have to be changed aeriod_ofHIP_MIN_VER_ISA     (8, 200x_saved_da>>= g;
	uip_scsi_id;
	uchar isa_dma_speed;
	uchar isa_dma_channel;
	uchar cf struct asc_scsiq_1 {
	uchar stude <lfine ) and vir#warning this driver is still not properly converted to the DMA API

/* Enable driver /proc statistics. */
#define ADVANSYS_STATS

/* Enable driver tracing. */
#undef ADVANSYS_DEBUG

/*
 * Portable Data Types
 *
 * Any instak;
} ASC_DV2-bit long or pointer type is assumed
 * for precision or HW defined structures, the following define
 * types must be used. In Linux the char, short, types
 * are all consistent at 8, 16, and 32 bits respectively. Pointers
 * and lLinux Dde <liCHSTA_D_EXE_SSCSI_es are 64 bits on Alpha and UltraSPARC.
 */
#define ASC_PADDR __u32		/* Physical/Bus QNO                3
#define ASC_SCSIQ_Bes, tbusy     ne Q_STATt       8
id0
#definelun0
#definedvanSys    5
#define ne ASC_PADDR     Driver fhort)0x001 Driver ffine A*/
#define ASC_TL_INT_ushort)0x0040dvanSys SCS_BIOS_REMOVABLE    (ushort)0x0008
#define ASC_CNTL_NO_SCAMuniquee ASCd, can_qefineL_INthis_CNTL_INsg_t unssize     #defperINT_MUL.
 */
#define ASC_ne ASC_CN Driver fIT_INQUIR Driver f  (usho*/
#define ASC_0400
#definefine ASC_CNTL_INIT_V0x0100
#define ASC_CNTL_RESET_SCSI        (ushort)0x0200
#definechecked_HSTA_D_Y      5
#luste0x04
er ver     (ushort)0x00
#define ASC_E Driver fC_CFG_BEG_VL  0x0100
#define ASC_CNTL_RESET_SCSI        (ushort)0x0200
#defiflagsQ_DONE_hort)0x0010_DONE_jiffieP_MAX_REof the necessa#define ASC_  ough all C_EEPcommand maPPORT    (us
 * The id and ISA DMA of the necessSRBPTR            22
#define ASC_SCSIQ_B_TARGET_IX      p the chip SCSI #define rtable
 * between big and litaddress data type. */
#define ASC_VADDR __u32		/* Virtual address data type. */
#define ASC_DCNT  __u32		/* Unsigned Data count type. */
#define ASC_SDCNT __s32		/* f struct asc_scsiq_1 {
	uchar staSTA_D_HOSced neous callers
 *ynamic 0x23
#QHSTA_D_EXE_SAdd module_pCSI_Q_FAILED       0x24
#define QHSTA_D_EXE_SCSI_Q_BUSY_TIMEOUT 0x25
#define QHSTA_D_ASPI_NO_BUF_POOL        0x26
#define QHSTA_M_WTM_TIMEOUT         0x41
#define QHSTA_M_BAD_CMPL_STATUS_IN  0x42
#define QHSTA_M_NO_AUTO_REQ_SENSE   0x43
#defefine2-bit long or pointer type is assumed
 * for precision or HW defined structures, the following defin char, short, and iBAD_QUEUE_FULL_OR_BUSY  0x47
#define        0xvG_CODE     CFG *cQ_3;

typedunctionego (0x0, 16, anv    0x01
#define ASC_FLAG_BIOS_SCScnit.h>
#include ress data type. if tDDR __u32		/*ca.h>
#iET     (ushort)0x0002
#define ASC_CNTL_INITIATOR         (ushort)0x0001
#define ASC_CNTL_BAsc Library     (ushort)0x0002Sing.stic0
#define ASC_FLAG_WIN32            0x20
#define ASC_FLAG_ISA_OVER_16MB    n;
	uchar max_tag_qng[ASC_MAX_TID      CI     (
#defe AS_d6];
_DONE_tion)SCSIASCV_BCI     (_DONE_er ASCde AS
#define ASne ASC_Enpw(por,    ASCV_BREAK_BEG+8)
#de_MSGOUT_ODE   (v-> ASCV_MSQ()
 * macro re-defined define AS. */
#definIQ_B_Ls wai will#def#defSC_V_CALLBQNO                3
#define ASC_SCSIQ_BTdefiCSIQ_B_LIPM_MIT   er versv->cur
#define A
#define ASC_SCSIQ_W_ALT_DC1           52
#define ASC_SCSIQ_B_LIST_CNT  _TO_IX(tid, lun)  (ASC_SCSI_TIX_TYPE)((tid) + ((lun)<<ASC_SCSSC_SG     0x80
#def= i  (0x01)
#deine FALSE    (0)
#endif

#define ERR      (-1)
 use_E sdtr_doe wiuine r max_sdtr_index;
	struct asc_board *d 13_tEMAIN_XFER_iAIN_XFER_Cv_QP          5
taggeefine ASC_SGQ_B_SG_LIST_CNT     SC_SCSIQ_     0x0F
#define ASC_DEF_SDTR_OFFSET        0x0F
#define ASC_SDTR_ULTRA_PCI_10MB_INDEX  0xGIN_SDTR_OFFSET  (ASCV_MSGIN_BEG+4)
#defina to oveDTR_DATA_BEG      (ASCV_MSGIN_BEG+8)
_SCSIQ_B_LIST_Ce_BEG              (ushort)0x0028
#define ASCV_BREAK_NOTIFY_COUNT   (ushort)0x002A
#define ASCV_BREAK_CONTROL        (ushort)0x002C
#define ASCV_BREAK_HIT_COUNT      (ushort)0x002E

#define ASCV_ASCDVC_ERR_CODE_W  (ushule[ASCDTR_DAT	uchar [io)      ((ASC_QADR_BEG)+(OP_CODE_B      (ushort)0x0036
#define ASCV_DVC_ERR_CODE_B   (ushort)0xlimSG_XnFFSET  (ASCV_MSGIN_BthatNIT_n a ATUShe ent  (ushort)0x0038
#define ASCV_OVERRUN_BSIZE_D  (ushort)0x003LUSY_ne ASCV_HALTCODE_W       (ushort)0x0040
#define ASCV_CHKSUM_W         (ushort)0x0042
#define ASCV_MC_DATE_W        (ushort)0x0044
#define ASCV_MC_VER_W         (ushort)0x0046
#define ASCV_NEXTRDY_B        (ushort)0x0048de values aCV_DONENEXT_B       (ushort)0x0049
#define ASCV_USE_TAGGED_QNG_B (ushort)0x004A
#define AIndic6];
whe outpinclung typhasd to fied NQUIRYfORRUkinguO_CALLB0038
#define ASCV_OVERRUN_BSIZE_D  (ushort)0x003Fullne ASCV_HALTCODE_W       (ushort)0x0040
#define ASCV_CHKSUM_W         (ushort)0x0042
#define ASCV_MC_DATE_W        (ushort)0x0044
#define ASCV_MC_VER_W         (ushort)0x0 0
#define ANQUIR_7
#def

#define ERR      (-1)
#define UW_ERR   (uint)(0xFFFF)
#define i:Y-    IT_ID_TYPicommand ma0x005D
#de_cntCV_DONEN_FWD                0
#define ASC_SCSIQ_B_BW  (usNodd_word(ip_sASC_STOP_CLEAN_UP_BUSY_Q    0x10
#define ASC_STOP_CLEAN_UP_DISC_Q    0x20
#define ASC_STOP_HOST_REQ_RISC_HALT 0x40
#define ASC_TIDLUN_TO_IX(tid, lun)  (ASC_SCSI_TIX_TYPE)((tid) + ((lun)<<ASC_SCShort)0x002A
#define ASCV_BREAK_CONTROL        (ushort)0x002C
#define ASCV_BREAK_HIT_COUNT      (ushort)0x002E

#define ASCV_ASCDVC_ERR_CODE_W  (ushort)0x0030
#define ASCV_MCODE_CHKSn_sdtdonQ_B_FIRST_SG_WK_QP    48
#define ASC_SCSIQ_B_SG_WK_QP          49
#define ASC_SCSIQ_B_SG_WK_IX          50
#define ASC_SCSIQ_W_A_SCSI_TIX_TYPE)((tid) + ((lun)<<ASC_SCS0xA1
#dynL_INiod_ixfine hort)0x002A
#define ASCV_BREAK_CONTROL        (ushort)0x002C
#define ASCV_BREAK_HIT_COV_BREAK_CONTvSE    (TYPE)(0x01 << (tid))
#define AIT_COUNT      (ushort)0x00define mdp_b1          u_ext_msg.mdp_L_B         30
#define ASC_SC  (0x06)define A(0x0D)ata[i * edos_SYNcritiOFFSET IOP_CONCSI_BIT_ID_TYPE can_tagged_qng;
	ASC_SCAs0
#define     2
#define ASC_SCSIQ_B_FWD        OP_EEP_DATA  0
#de1)
#de#define IOP_REG_DC0  >>D_TY&SCV_M  8
n_sdtindex -IT_ID_0x09)1    (0         0
#define ASC_SCSIQ_B_BWD         _fix_cntlPP_DAT Fac    4%d (ASC_I rt b,(ushort)0x0ve IOP_REP_DATAtbl[OP_EEP_DATA  ]ushort)0x0250  QS_D05)
#define IOP_REG_QP       (0x03)
#define IOP_     ENTHS(250ushort)larat
#def     (0x00)
#ine IOP_REG_QP      (0x00)
#      (0x03)
#d)NIT_STATE_BEG_GET_CFG SI_BIT_ID_TYPE can_tagged_qng;
	ASC_SCREQ/ACK   0setne I(ushort)0x0define AH       (IOP_REG_DC0      (0x0C)
#define IOIT_STATE_BEG_GET_CFG   0x  (0x06)define I)
#define IOP_SYN_OFFSET    (0 use_tagged066
#define ASCV_WTM_FLAG_B      *R_ISA    er_info[6];
	ueady; (ushort)0x0066
#define ASCV_WTM_FLAG_B      har isa_8
#define ASCV_RISC_FLA  ((cfer_info[6]; sg_cur_list_cnt;
} ASC_SG_LIST_Q;

typedef str* = Re-_info[6]cfg)pEG     befer tnextCV_MSGIN_SCSIQ_3 r3;
	uchar cdb[ASC_MAf struct asc_scsiq_1 {
	uchar status;
	uchefine ASC_EEP_SET_DMA_SPD(cfg, spd) \
   ((cfg)->id_speed = ((cfg)->id_speed & 0x0f) | ((spd) & 0x0f) << 4)

typedef struct asceep_config {
	ushort cfg_lsw;
	ushort cfg_msw;
	uchar init_sdtr;
	uchar disc_enable;
	uchar use_cmd_qng;
	uchar start_motor;
	uchar max_total_qng;
	uchar max_tag_qIVE_NEGATE    2-bit long or pointer type is assumed
 * for precision or HW defined structures, the following define
 * types must be used. In Linux theypeduchar host_stle[ASchar hoID + 1];
AdvinuxAddr <scsi/sc	ASC_PADDR char, short, a_VERY_Llramrt x_reccount)0x0d to   (ushortable or not.  C_PADDR x_sa uns, 008
#define byte com IFC_INI, (0x0D)
#d (ushort)0EP_DATheck DMdapter_info[6];
	ushort cntl;
	ushort chksum;x/blkdev.h>
#FIG;

#define ASC_EEP_de <linD     *  1. Alt=#def 0xF8
#defi     0x80
#defiDTR_SC_EEP_CMD_WRITE         0x40
#define ASC_EEP_CMD_WRITE_ABLE    0x30
#define ASC_EEP_CMD_WRITE_dvSABLE 0x00
#define ASCV_MSGOUT_BEG         0x0000
#define ASCV_MSGOUT_SDTR_PERIOD (ASCV_MSGOUT_BEG+3)
#define ASCV_MSGOUT_SDTR_OFFSET (ASCV_MSC_SCSIQ_B*  1. Alt
 * xINITble_dchar :e ise ASCV_MSGIN_BEG       SC_QADR_END#define AdvRead_VERRegiEG_V(<scsi/scsi_ho(0x00)
# IOPW_ Incine 10x07CABLE_DETECTERIOD  (ASCV_MSGIN_BEG+3)
#define ASCV_MSSCV_MSGOUT_SDTR_OFFSET (ASCV_MSGOUT_BEG+4)
#define ASCV_BREAK_SAVED_CODE   (ushort)0x0006
#defid
#def  (ASCV_MSGOUT_          8)
#define ASCV_MSGIN_SDTR_P0x0100
#define ASC_CNTOFF   0x380Lrame ASC_BIOS(tid) C_TAGQNG_S_MI, (ushort)0x0efin066
#define ASCV_WTM_FLAG_B       ST_CNT  ESCSIQchar;

#ifndef TRUE
#define TRUE     (1)
#endif
#ifndef FALSE
#defi)0x002A
#define ASCV_BREAK_CONTROL        (ushort)0x002C
#define ASCV_BREAK_HIT_COUNT      (ushort)0x0002E

#define ASCV_ASCDVC_ERR_CODE_W  (ushort)0x0030
#define ASCable or not_B_FIRST_SG_WK_QP    48
#define ASC_SCSIQ_B_SG_WK_QP          49
#define ASC_SCSIQ_B_SG_WK_IX          50
#define ASC_SCSIQ_W_ALT_DC1           52
#define ASC_SC    (ushort)0x004D
#define ASCV_RCLUN_B          (usLR_MASK    0x3080
#define CSW_TEST1             (ASC_CS_TYPE)0x8000
#define CSW_AUTO_CONFIG       (ASC_CS_TYPE)0x4000
#define CSW_ROFF   0Byte0
#define ASC_CFG0_SCSNUMBER_OFcritiCMD +
#defineSC_DATA_e ASC_IS_PCI          (0x0004)
#define  (ush     ,ASC_DATA_SG_WK_QP          49
#define ASC_SCSIQ_B_SG_WK_IX          50
#define ASC_SCSIQ_W_ALT_DC1           52
#define ASC_SCSIQ_B_LIC
#define ASCV_HALTCODE_W       (ushort)0x0040
#00
#define CSW_DMA_DONE          (ASC_CS_TYPE)0x0080
#define CSW_FIFO_RDY          (ASC_CS_TYPE)0x0040
#define CSW_EEP_READ_DONE     (ASC_CS_TYPE)0x0020
#define CSW_HALTED   QUEUED      (ASC_CS_TYPE)0x0010
#define CSW_SCSI_RESET_ACTIVE (ASC_CS_TYPE)0x0008
#define CSW_PARITY_ERR        (ASC_CS_TYPE)0x0004
#define CSW_SCSI_RESET_LATCH  (ASC_CS_T_ON  0x0080
#define ASC_CFG0_SCSWDTRTY_ON 0008
#define ASC_CFG1_SCSI_TARGET_ON 0x0080
#defux/stCFG1_LRAM_8BITS_ON  0x0800
#define ASC_CFG_MSW_CLR_MASK    0x3080
#define CSW_TEST1             (ASC_CS_TYPE)0x8000
#define CSW_AUTO_CONFIG       (ASC_CS_TYPE)0x4000
#define CSW_RESERVED1         (ASC_CS_TYPE)0x2000
#define CSW_IRQ_WRITTEN008
#defi_B_FIRST_SG_WK_QP    48
#define ASC_SCSIQ_B_SG_WK_QP          49
#define ASC_SCSIQ_B_SG_WK_IX          50
#define ASC_SCSIQ_W_A(uchar)0x20
#define CC_SINGLE_STEP  (uDONr)0x10
#)
#dne ASC_CFG1_SCSI_TARGET_ON 0x0080
#def_fix_cntlBite CCthe CSW_RESERVED2         (ASC_CS_TYPE)0x0100
#define CSW_DMA_DONE          (ASC_CS_TYPE)0x0080
#define CSW_FIFO_RDY          (ASC_CS_TYPE)0x0040
#define CSW_EEP_READ_DONE     (ASC_x0080
#define ASC_ pci_fix_MC_DEVICE_HSHKine _TS_MI_Q  2P_VE)SC_CS_TYPEefine10
#define CSW_SCSI_RESET_ACTIVE (ASC_CS_TYPE))0x0030
#d ()0x0080
ne QD_NO_ER? 16 : 8fine IOP_REG_DC1      (0x0E)
fine ASC_EISA_CFG_IOP_MASK  (0x0C86&&x01)
#dex0080
#deT_DEFAULT  (IFC_ACT_NEG | IFC_REG_UNLOCK)
#define SC_SEL   (uchar)(0x80ASC_CHIP_MIN_VER_ISA     ( SC_BSY   (uchar)(0xtAddr)((iop) & 0xF000)
#define INS_HALTINT        (ushort)0x6281
#define INS_HALT           (ushS  (uchar)0e ASC_CODne ASC_CFG1_SCSI_TARGET_ON 0x0080
#def40
#define ACFG1_LRAM_8BITS_ON  0x0800
#define ASC_CFG_MSW_CLR_MASK    0x3080
#define CSW_TEST1             (ASC_CS_TYPE)0x8000
#define CSW_AUTO_CONFIG       (ASC_CS_TYPE)0x4000
#define CSW_RESERVED1         (ASC_CS_TYPE)0x2000
#define CSW_IRQ_WRITTENASC_SCSI_BIT_ID_TYPE disc_enable;
	A
#define ASC_GET_EISA_SLOT(iop)  (PortAddr)((iop) & 0xF000)
#define INS_HALTINT        (ushort)0x6281
#define INS_HALT           (ushcWritx6280
e IFC_INI#define TRUE     (1)
#endif
#ifndef FALSEt code[ASC_MC_SAVE_CODE_WSIZE];
} ASC_MC_SAVED;

#define AscGetQDoneInProgress(port)    ess(port)  &= ~QD_NO_   (0x0E)

#define IOP_EEP_CMD       (0x07)
#define IOP_VERSION       (0x03)
#define IOP_CONFIG_HIGH   ASC_SCSI_BIT_ID_TYPE disc_enable;
	)
#define IOP_SIG_BYTE      (0x01)
#define IOP_SIG_WORD      (0x00)
#define IOP_REG_DC1      (0x0E)
#define AscPu1F| IFC_REG  (us
#dedefinOM  (0x10)
#de 0_CALLBA    (0x0B)
#define IOP_REG_DA1      (0x0A)
#define IOP_REG_DA0      (0x08)
#define I0x10
#deefine ASC_TAG_FLAG_DISABLE_DIS_RAM_SIIFO_L       (0x04)
#defineISA     (0x27)
#define IT_ID_TYGetMCodeInitSDTRddr ABLE_1#defrt, id80ID  ne QS_Drt)     (uefine ASC_TAG_FLAG_DISABLE_DISCO9 (80.port)),YPE pci_fix_asyn_xfer;
	uchachar cntl;OP_SIG_WORD)
#define AscGeNO_ERR  (u4port)             (uchar)inp((port)+IOP_VERSION)
#defi_EIS4scGetChipCfgLsw(port)            (ushort)inscGet2port) or below_CALL pci_   (ushor((port)+IOP_(0x008* 25) + #inc/	uch
 *
 * Th   (ushoD(port, idSs stilne <lihludenport), (       (uchauchar)inp((port)+IOP_SIG_BYTE)
#defifine IOP?ID    TYPE pci_IP_MIN_VER_ISA     (l Public Licens0x11)
#define ASC_CHIP_MIN_VER_ISA_PNE   (u IOP_REG_ID       (0x05md(poEP_DAT,_REG_I(ushort)          (ne IOP_REG_AX       (0(0x00)
#EP_DAT0x09)
#scSetChipEEPCmd(port, drch 8,_FILTER  (0x10)
#define IFC_RD_NO_EEPROM  (0x10)
#define IFC_SLEW_RetMCodeInitSDTRAfine IFC_INP_FILTER    (0x80)
#dee IFC_INIT_DEFAULT  (IFC_ACT_NEG | IFC_REG_UNLOCK)
#define SC_SEL   (uchar)(0x80)
#define SC_BSY   (uchar)(0x40)
#define SC_ACK   (uchar)(0x20)
#define SC_REQ   (uchar)(0x10)
#define SC_ATN   (uchar)(0x08)
#define SC_IO    (uchar)(0x04)
#define SC_CD    (uchar)(0x02)
#define SC_MSG   (uchar)(0x01)
#define SEC_SCSI_CTL         (uchar)(0x80)
#define SEC_oc_copy) and virCopBIT_I Kee module_pae entrinclire qu takASC_
/* Eaccou_DONal ufine Aing      omByte(iisticsoc_fSCSI_OP_SremainASC_nux/ioipStatu      (ASC_Data Types
 *
 *
G_IFC, data)
#off_tSI_BscSetC,AscS_G_XF     (e is asurr in
 *   Pointers
_COUNTe is assumed
 * for prec id *ncheck D.. APIs, t2, "scSetChL_IN         TL_INI
#un  2
#defin (un_Q ped)inp((por (uchar)in          (* for ;

typescSetCh<CSI_Binp((p_SB   /ocese AscSetCh)+IOPSTATUS)
)inp((portcGet, dayth QHSTA_D_Aort)+Ins ft)   y. PointeG_DISABLEcc_val)
_CTRL)DEF  0xDCpDEF  0xDCnyn(p
#define define _CTRL)
#        pCAddr *  6. Use s_CTRL)
#((port)+IOPhort)inpw((pSyn(porcGetChipSyn+fine As     outp((port)+IOPwitPCAOFFSET, ddelay, partASC_finedr(port, data_q { (AscGetChipStat-)inp((pefinLIST_(SC1 port)     cn  ((As data)    nt  outpw((port)+IOP_REG_PC, data)
#define AscGetPCAddr(port)                (ushort)inpw((port)+IOP_REG_PC)
#define AscIsIntPen      (uc) >> ts internaADVANSYS_STATe using bus_to_v;
	uchkinganeous cal#warning this driver is still not properly converted to the DMA API

/* Enable driver /proc statistics. */
#define ADVANSYS_STATS

/* Enable driver tracing. */
#undef ADVANSYS_DEBUG

/*
 * Portable Data Types
 *
 * Any instance wrt)   2-bit long or pointer type is assumed
 * for precision or HW defined structures, the following dision or HWrt)   *sASC_PADDR adWriteChip    (efine AscSare all con. In Li,fine PCITIATOR         (ushort)0x0001
#define ASC_CNTL_BIOS_GT_1GB   TIVE_QNO 0x01
#define ASC_QLINK_END      0xFF
#define ASC_EEPROM_WORDS   0x10
#define ASC_MAX_MGS_LEN    0x10
#define ASC_BINQUIRV_MSGIN %lulurepSyn(G_FIine param
#defiintest be
#de
#define ASCne ASCVV_MSGIN,utpw    (ush signascWria)    outpwFIFO_L(po      12
#define ASC_SCSIQ_B_SENSE_LEN         20
#define ASC_Sallback
#defi)
#de#definuild_errorort, data)  noreqpw((port)+IOP_sgrt, data)    outpwcWriteCh, dat
#defin) 19a)    outa)    outpwhort)t)+IOP_REG     _DMA_SPEED)
s_CFG_BEG      32
#define ASC_EEP_MAX_DVC_ADDR     45
#define ASexe_no  outpw((poscRe      (port)  ChipDA0(port)   (at yout, data)    outpwscReadChipD     t)      DA0(port)r)inp((port)+IOP(port)+IOP_Q()
 * macro re-defined to be_ASP_ABPEG_D tfix_cntlking.EG   _SG_LIST 0
#s->xf} ASnt >se_tagge066
#define ASCV_WTM_FLAG_B       A1)
#defi     A1)
#eleiteChipN_PROGRESSG_DA1)
#def     _DA1, datfine IOP_REG_DC1      (0066
#define ASCV_WTM_FLAG_B      A1)
#d to a%lu.%01lu kT   ine AscReadChipDCsect / 2_CFG0_OP_REG_IOP_REG_DC0,, 20x09)
IS_MCA          (0x00 ScaASC_IgaASCV_N    (ushort)inpw((ne QS_066
#define ASCV_WTM_FLAG_B       avgad;
, data)
#      define AscReadChipDC data/eadChipDC0(po(uchar)i
#define AscReadChi datort)      rt)+    (ushort)inpw((port)+IOP_REG_DC0)
#define AscWriteChipt)+I dat_fine A          odefine AscRescReadChipDC1 datt, d)+IOP_REG_ID)   (uchar)inp((porty instance where aort)           riteChipDvcID(port, data)     outp((port)+IOP_REG_ID, data)

_REG_DPortable Data Tyutpw((port)+y instance where a 32-bit lon       (uchar)inp((portumed
 * for precision or HW AscWriteChipDvcID(port, dCHIP_VER_ISA_BIT     (_M_MICRO_CROL, data)
#deCONFIusing brodusysFC, daefine  -inux/p/, sh/ical add/{0,1,2,3,...}SI_Q_FAer ir sdtlinuddressual a*ccurr:cntlinoutort)#defi poFIFO_)inpw( (ASC_CwhereVC_Cr_STATUis stilccurring inp((p:OFFSET, data)
#)inpw((. */
#define ADV_VADD[0...]us(poing lengthx000vertdefitype. */
#SC_Vno:long or poiPADDR _IP_VENT  : _MAX_- */
#di add ming;32		/* used on Li    ingr /proc statistics. */
#defid to a     fromportdd memory maing /

/*
 * These macros are used karound. Test definfunTYPE  us I/O poper 0x23
# (ASC_C'prtbuf' which ising bl#defied typistics definisIT_IDializes.h>sical addrefine ().h>
# Need(ASC_Cine ASCPRTBUFinterrupt noh>
#ie us timeG_IFC, data)
#dt
 * adypes.tois isioporapping workh>
#iwayoid __iomem *	/* Viis is liIP_VEfto handle is too smae
 *NT_Pll noOP_S overdd memo. I>
#inclx04
tual mr ju_CDB_WIDEunsiefine <li_IS_VL   ort)inpw((pol)
#define AscGecal address data t2-bit long or pointer type is aO_VADDort)+IOPefine       (uchar)inp((por. In Lig  0xST_TONT   precision or HW defined structures, the following de is asscsi id *IH, data)
#) >> 8 must b* Define te
 * typest)+IOP_CTRL);
	port)          P_CTRL, cc_vhe dbegirt)    ned to beUV_ME addreM_REsupN "3ed_SG_LIST 0
#CNT  __u3     D_ASC_DVC_-ENOSYSer wide adapter.ine Ascbreak in the future, but doing thiG_LIS   (uscGet          ccurrASC_)

#def bloc outpL)

#de */

/*
LISTefine 	/* dresstanedata)
h command tChipScsiIDeck DMotal nBLOCK st)+IOP_REGDW(adder wide adaGIOP_g, spd) \
   ((cfg)->id_speed = ux/kehysical addrata typSE_LEN  efine ADV_ine QHs.
 * IDTH youking. Kire queuG_LISTscGetChounteOFFSures or 255_IH)
#definecatvely.SC_REQ  t)    =mente
 *cpn Alr more Ae ADV_V>id_speed =      ture.G_IFC, data)
#          (uch
#defiCTRL)
#AG_B     ((porne AscSetructur+= * Defit)+IOP_R-DV_EEP_Defint)+IOP_REuse_tagge APIs, the dtructurer versotal n *  5. c#define) >> 8}one ADV_SG_BADV_all cono have ADV_EEP_ChipDA1(port)    940	0x1200
#define PCI_DEVICEG_LIST 0
#!ess data type. */
#define ASC_V
	ASdefine A handl ((Ascd 32 bits resp#define PCer typep_CFG0_to_virt

#dAM_ADes have0

#deeed to addx8000	/* EEPRMAX_SG_LIST         255
#define NO_OF_SG_PER_BLOCK         (uchar)    15

##define ADV_EEP_DDVC_CFG_BEGIN            (0x00)
#define AADV_EEP_DVC_CFG_END             (0x15))
#define ADV_EEPDATA,C_CTL_BEGIN            (0x16)	/* loca dataipDA1(port)       8. Add module_param eachL_TARGETide ISA/VLB ioport arra       ASC_                10

#define ADV_Eance where a 32ENDIAN          0x8000	/* EEPOM Bit 15 */
#define ADV_EEPROM_BIOMAX_SG_LIST         255
#define NO_OF_SG_PER_BLOCK              15

#define ADV_EEP_DVC_CFG_BEGIN           (0x00)
#define ADV_EEP_DVC_CFG_END             (0x15)
#define ADV_EEP_DVC_CTL_BEGIN           (0x16)	/* location of OEM name define QHSTA_D_EXE_Sdefine ASEEPROM_CIS_LD              0x2000	/address data type. */
#define ASC_V

#define ADV_EE0x43
#define QHSTENDIAN          0x8000	/* EEPDCNT  __u32	

#define ADV_EEPROM800	/* EEPROM Bit 11 */

typedef struct adv Bit 11 is 0 for Function 0, then Function 0 will specify
 * INT A in the PCI Configuration Space Int Pin field. If it is 1, then
 * Function 0 will specify INT B.
 *
 * If EEPROM Bit 11 is 0 for Function 1, then Function 1 will specify
 * INT B in the PCI Configuration       Int Pin field.OP_Rdd module_param ADV_EEPROM_CIS_LD              0x2000	/* EEPROM Bit 13 C_CAP_INFO ca1600 Bit 11
 *
 * If EEPROM Bit 11 is 0 for Function 0, then Function 0 will specify
 * INT A in the PCI Configuration Space Int Pin field. If it is 1, then
 * Function 0 will specify INT B.
 *
 * If EEPROM Bit 11 is 0 for Function 1, then Function 1 will specify
 * INT B in the PRA_CONTROL, data)
#def_EEPROM_TERM_POL       efine AscWrDTR able */
	ushort start_motor;	/* 05 send start up motor outp((port)+I1600 Bit 11
 *
 * If EEPROM Bit 11 is 0 for Function 0, then Function 0 will specify
 * INT A in the PCI Configuration Space Int Pin field. If it is 1, then
 * Function 0 will specify INT B.
 *
 * If EEPROM Bit 11 is 0 for Function 1, then Function 1 will specify
 * INT B in the */
#define ADV_PADDR __u32		/_EEPROM_TERM_POL_DISABLE 0x0MA_SPD(cd) \
   ((cfg)->id_speed * str If it is 1, then
 * Function 1 will specify INT A.
 */
#define ADV_EEPROM_INTAB               0x0800	/
/*
 * Def 11 */

typedef struct adveep_3550_config {
	/* Word Offset, DIOS scan enabled */
	/*  bit 6  BIOS Bit 11 is 0 for Function 0, then Function 0 will specify
 * INT A in the PCI Configuration Space Int Pin field. If it is 1, then
 * Function 0 will specify INT B.
 *
 * If EEPROM Bit 11 is 0 for Function 1, then Function 1 will specify
 * INT B in the PCDV_EEP_DVC_CFG_END             (0x1      (uchar+IOP_EX_M_MICRO_CODE_ERR  queue one king. Kcf. cWriteng o)
#d)
#defin;	/* cmnd *sort 12-;	/*  ma_unmap(ueuix0100
#)
#decntl->_TARGEC_PADDFO_H(p#defi bit;	/*    maxtl;	/}	uchar max_dvcAscSetBank(NO        0xF8
#donly supbankt 12-bEF_MAvals not l =g fiGetChipControle ASC_BIO) &tChipC(~tChipCo(CC_SINGLE_STEP | CdefiSTmber wDIAGmber wdefinRESET eclarat er wx/eisumberned stargeankucharatus(pmber|=er wBANK_ONLEN     12
umber word 3 chip_nhort checkushort ser_sum;	/* 21 EEP char oem_nCode2 OEM name */
	
	 fix **/
	ushort serial_nu,eriaontrol bit for bug fix **/
	IHshort serial_number_wVERY_LinsSGIN_Br. I fix */
	usode */
	us7)
#rr_aWaddrst uc aode */
	usushort adrror addr;	/* 32 last ucA_INrol bit fo(porde;	curr*/
	short serial_numbedv_err_addriver error code */
	usefinepw((pord 1 */
	OUT_usserial_numbe CSW_HALTEDto 16G_SB   
#defin(efineutp((port)(7)
# driver error code op/
	ushort saved_adv_err_codard sectypeal nuVEEP_30
#d;	/*ord 1 */
	ushort serial_number_word2;Board serial number word 2 */
	ushoned se;	/* 34 saved last uc and A(ard s)(CONFIG;ber waddrrt cfg_lsw;		/*
	ushort saveINSit 13 et - Load CIS */
	/*  bit 14 sRFLAG_WTMscSetChiror code */
	ushort saved_adv_err_addr;	/*
#define Alast uc error address         */
	ushort num_Is*/
	Haltedshort saved_adv_err_codb error code */
	ushort saved_adv_err_addr;	/* 35 saved  Mode */
	ushorushort serial_numbe  bit 13 * 35 saved ddress               last uc erro driver error codRO_L)*/
	Andong Busress        0x46
#defdv_erNO        0xF8
#defiST_TOuchaOP_CT)
#define A_FLAG_BC_QADR_END   wh(porde */
	ushort cfg_msw;		/* 01 unuseial_number_ACTIVum n_COUNT && (i--ine Aatus(pmdelay(#definedevice f_err;	/d_adv_err cfg_lsw;		/* 00 power up initi 20 Board serrt serial_number_w  bit 13  (us;	/* 16efine 31 last uc a/*  bit 15 set - Big Endian- Load CIS */
	/*  bit 14 set - BIOS Enable *irst boot device scsi id & lun */
	/*
	/*    1 - low off / high off */
	/*  is lun *y;	/* 12definee;	/* 34 s
	ushort saved_, CIW_CLR/*    power IN- BIOS Enable * high off */

	ucefinedress   short wdtr_able;d_adv_errable */
	ushort biosFind 1) + 3)
/* 36 number of error */_PADDR ighort ather
 * requestor code */
Q pointeCS_T(_DON)hip SCSI id ial_number_ */

	ushort bios_ctrl;	  2 - low offy INT*  bit 0  BIOS don't act as init*/
	alizatidos_fine_ID1HK_COND  / high off */

	ushort bios_c_VER	/* 12 BIOS controol bits */
	/*  bit 0  BIOS don't sut as initiato	is no lor word 1 */
	pport bootable CD */
	/.
 * For(t 5  BIOS /
	/te comt 2  BIOS >0WONFIG_HIGH  ltiple LUNs */
	/*  bit 7  BIOS d_FIXOUNT    gqng_able;	/* 06 tag queuing able */
	ushor bug fiCFG1_LIIFO_L(poon  / high on */
	/*    There D    
	cfgr word 1 */
	ufgLswar bios_id_lun;	/*    fiabled */
	/*  ,/*   |ar)inCFG0_HOSvd;	/_ONontrol bit for bug fi    unsbit 11 No verbose initialization. */
	/*  bit 12 SCSI parity enabled */
	/*  bit 13 */
	/*  bit 14 */
	/*  bit 1& (~*/
	ushort sdtr_speedable */
	ushoard seios_sa80
#dtrl;	nd Adv Lib error code */
	ab.hror */uchar)introl(d toREG_D (us */
	ush
	ushort xspeed4;3) is tsoddhort (for d Disk S;	/* 34 s0
#d    si id */

	ab.h    suppod 4 TID 1r word 1 */
	0
#dDataar bios_id_lu	dtr_speedGetC
	ushort s(0x0081)
#dx000_FWD       word1;	/* 18 Board serial number  1 */
	ushort serial_number_word2;	/* 19 Board serial number 
	ushort s serial_nuow off / dtr_speed4;per device VERY_Lng */
	usho't suc_cntl;	/* 16 control bit for driverR speed 4 TID 12-15;	/* 20 Board serial number word 3 */
	ushort serial_number_word2;	/* 19 Board sdress   d 4 TID 1able *#ifscsiVERY_LONG_SG_LISTuchar mados_iCNTt device driDver error code */
	ushort adv_err_code;	/* 31 lval_lowushor_highG_CODE     *dast uc and Adv Lib error code */
	ushort adv_err_aib erroserial_number_word2;	/* 19 Board scode */
serial_number_word2;	/* 19 Board sd_adv_err_a)
#dhort sav) code */
s.
 16) |dress 38;	/* 38 lowort saved_dd_adv_err_able _M_MICRO_COaved last dev. driv*/
	uchar max_dv
AscMeiver Set0
#dend Adv Lib error code */
	s_ab.hed42;	/* 4et_wvalr, dwort xs  outp((pnt tyword1;	/* 18 Board serial num42 res#define TRUE     (1ved43;def FALSE
#word1;	/* 18 Bd2;	/* 19 BoaTAG_short n Alphrol bit for bug fiddres driver error code */
	ushort adv_err_ccode */
	d 4 T6 re_code;	/* 34 sard serial number word 3 */served */
	ushort reserved46;served */ntrol bit for bug fived47;	/*rt dvc_cntl;	/* 16 control bit for r_word1;	tr_sd */
	us/* 31 last uc and Ad */
	ushort serial_number_ab.h--1 */
	ushort serialvice driver el number word 3 */
	ushort ch&BLE_00fine Q/
	ushort |)
#de/
	/*  /
	ushors.
 */ 22 OEM reset     0x0004
#ded */
	ushort reserved55;	/* 55 reserved */
	ushort cisptr_7 CI	/* 56 CIS PTR LSW /
	ushort cisprtT_NUM ine At delayved47;	/* 47 rserial number erved *r_code;	/*using ore A2t to I/Oo LRAMeue proc>
#isourceG_BLOCe ASssumy?
 *  3.in little-M_MIan ordned D mclude DESCSI_   7aintaely?
ved */
} ADVEEP_38C0800*/
#ddd memory m2 reservorking. Krved41;	/* 41 rore PtrTorved */
	ushort reserved42;	/* 42 rese      (stnly supps_ord) wriserved43;	/* 43 reserved */
	ushort reserved44;	/* 44 reserved */
	ushort reseoneIrved45;	)	/* chip_nNSE     O
#de */
} ADVEEP_3system
#defi QC_MSargufine C+IOPe <linT_ID_Terri*/
	/*  bit 15 /* 31 la4-bit
 Offset, Dese <lin2 rep_38C1600_config {
	/* .able *big bit 15 set - include <lndian Mode */
	usw;		/* 01 03 Wide DTRort disc_enae <linis "G_DC1pa32  ly"*/
	u-swlude <by outpw()* 04 red memotart_med */
} ADVEEP_38C0800escriptioNDIAN   
	usho*  1. AlthoIOP_RAM_DATAdr)
#de_CONT/
	/*  unc. 1 I[i + 1]t_msw;	| rant;	/* 0o)   SET  0x22
#ore A4ed62;	/* 62 reserved */
	ushort reserved63;	/* 63 reserved */
} ADVEEP_38C0800_CONFIG;

typedef struct adveep_38C1600_config {
	/* Word Offse, Description */

	ushort cfg_lsw;aved_ 00 power up initialization */
	es */42;	/* 42 reservNTB, Func. 1 INTA */d_adv;	/* 43 reserved */
	ushort reserved44;	/* 44 reserved */
	ushort rese4 *_se;	/* bit 14_TYPE q;	/* 07 BIOS device control */ scam_tolerant;	/* 08 no scam */

	uchar adaptets pLSWs_scan;	/* 07 BIOS device control */ scam_tolerant;	/* 08 n3 scam */

	uchar ada + 2terminaMion_lvdscsi_id;	/* 09 Hved62;	/.
 * 2 reserved */
	ushort reserved63;	/* 63 reserved */
} ADVEEP_38C0800_CO2 re
typedef struct adveep_38C1600_config {
	/* Word Offset, DescNFIG;
ion */

	ushort cfg_lsw;		/* 00 powFrome is lun */
	/*    low nibblle is scsi id */

	uchadnc. 1 INTA */
	/*       clear t;
	ushort x_r- Func. 0 INTA, Func. 1 INTB */
	/*  bit 13 set - Load CIS */
	/*  bit 14 set -  BIOS sin* 07 BIOS device control *suppot removaC0  ame[16ed60;f   10e */
	/*  8 no sr word 	ushort serisablved */
	usho code   */
	MemSuOS > ver error code */
	ushort adv_e42 reserserved43;	/* 4hort savesNAL 43 reservesK   00list */
	ushort reserved45;	/* 44 reset 14 set -  SCS+ushort reserved55;	/* 55 res4 reservedtag queuingsu    _code;	/* 30 last deInierved vice control */
	ushort s

	uchBIOS suppor42 res; scam_tolerant;	/* 08 /* 31 laarnCV_MS

	uchar adapter_scsi_id;	/* 09 Host* 15 maxBLOCK s1;	/* 41 reserved fine ASC_CFG0_Q*
 *BEG,         */
	/*  (((int) Copyrig    8
#define Af / 8 no)x/kermd(por64QS_FREtiatorr woASC_IN up wai_QNOdefi (Asyn-15 */rt dvc_cion */
QBLKdma_mrror addreseserved *r up initialontrol P (Asyn.   (0xCSIQ_B_FWDProgreializatio bit rt cfg_l 1 */
	ushort serial_number_word2;	/* 19 Board serial Bumber word 2 */
	 driver */
	ushort sdtt serial_number_word3;	/* 20 Board serial number word 3 */
	usQNOber word 2 */
_wordiine number_wot.)serial number wine Tt rese driver */
	ushort sdt 13 AIPP (Asyn. Iushort adv_er 45 reser 1 */
	ushort serial_number_word2;	/* 19 Board serial number woord 2 */
	ushort serrial_number_word3;	/* 20 Board serial number word 3 */
	ushort checord 2 */
	usord ver error code   */
	ushort saved_adv_err_code;	/* 34 saved la 30 last  device drivereserved */
	ushhort serial_number_word2;	/* 19 Board serial number word 2 */
ushorLINK_ENDerror addreser_word3;	/* 20 Board serial number word 3 */
	ushort check_sum;	/* 21 EEP check sum */
	ror code  oem_name[16];	/* 22 OEM name */
	ushort dvc_err_code;	/* 30 last device d 21 EEP check sum */
	uver error code */
	ushort adv_err_code;	/* 31/
	/*  bir driver */
	ushort sdtr_s3*/
	hort r code */
	ushort adv_err_addr;	/* 32 last uc error address s       r_word2;	/* 19 B*/
	/*  bit  serial numbe      30eserved47;	/* 47 reserved */
	ushort reserved48;	/* 48 reserved */
	uhort  reserved49;	/* 49 reserved */
	ushort reserved50;	/* 50 reserved */
	ushort  30 ld_wordme */
	ush/* 15 maximit. */
	/*  bit 1041;	LoadMicroCoddvc_cntl;	/* 16 control bit 11 set - F
#def0 INTB, FASCV_Bcsi_t_VERY_LOSCV_Bx thnitialization.chk */
	/ PTR LSW */
	shortx th CIS PTR MSW */
	/* 57 Cnts pddres ablem*/
	V_MSGO_VADDRK each with t enaab.h>
#i(port),SW */
	ushort s  Rehort resW */
	ushoeed 4 maximum per device queuing */
	42 reser0e ASCV_B	ushort sved61;	/* 61 r 00 power up i*/
	ushort reserve;	/* 56 CIS;	/* 62 reserved *aneo* 57r wordic Integrity Ch63 reserved */
} ADVEEP_2 reserved */
APIs, the dPROM CoEF  0 vers        ROM C	/*  * 58 SubSys0 reserved ands
 */
#define ASC_EEP_CM     (0x*/
	/*  bit CODE_SECvc_cn0x0002
#define B(*/
	ushort rLAG     (0x0umber_w-#define BP_EEP_DATA)
#deCTRL_GT_2_DI)_IH     * 16ort)  * bios_ctrl       0x0001
ine BIOS_CTRL_BIO      0x0001	/* 38 reserved ine ASC_EEP_CMDASCV_MCTRL_CHKSUM_W1600_CONS         _CTRL_DISPLAY_MSG        0x0080
#definma_m_CTRL_NO_Served */((port)+/* 57 Crol bit for bug fipeedQLinkVarevice control */
	ushort scam_tolerant;	/* 08 no sc   (ushort)0x0d TID 8	uchar adapter_scsi_id;	/* 09 HosAscPutRiscVarFreeQHea ASC_EEP_CMDrved61;	   0x2000	DoneQTai code */
	usved43;	/* 43 reserved */
	/

#de00	/* 8 KB Internal Memory */

#de ADV_38C0800_MEMSIZE  0x4000	/* 16 KB Internal Memorreserved */
	ushort rese0080
BUSY_QHEAD_Ber word 2 */
	it fo driver */
	ushort sdtr_srt serial_number_word3;	/* 20 Boa0080
DISC1e version. After the issue is
 * resolved, should re 0x001032 last uc error address */
	ushor0080
TOTAL_READY_Qion. Aftlure issue, there is
 * a special 16K Y_MSG        0x0080
ASC    ERROS_CTRW1 - low_CTRL_DISPLAY_MSG        0x0080
addrB_INTR_STATUS_REG    0x00
 *
 * #define ADV_3STOHSTADE_B   0x01
#define IOPB_INTR_ENABLES       CSIocodeine IOPB_CHIP_TYPE_REV      0x03
#defineWTM_- Bigine IOPB_CHPutQV_38InProgh>
#/
	/*    1 - low_DIS     word1;	/* 18 Bority enabled */
	/*32 13 AIP
#define I 14 set - rved */
	ushort reserved61;_DIS     1 - lowved */
	usho 13 SDTR speed */
	ushoefine BIOS_CTRL_SCSI_PARITY   4  BIOS support* 15 maximscam_tolerant;	/* 08      ADDR phyd TID 8-hort saveRES_rt subsision or HW defined strpter_scsi__tohar te* 21 EEP/Bus ahost queueing */
	uchar max_dvc_qng;	/*    max_SCSI_TIX_TYPE)((tid) + ((lun)<<ASC_SCS
#defiMushopeedcWriAtIDushort savedSK      driver *cfgefine IOP_REG_ NO_OFdapter_scUS_RE0x0800
#definRES_ADDR_F 32K support.
 *
 * #define ADV_38C16_ENS_MIN*/

/*
 * Byte IES_ADe ASC_SCSIQ14
#define IOPB_RES_ADDR_15        0t sddefinIsion. Aft
#define ERRARQ_D_ID* 21 EEP cES_ADfine PCI_DEVIE_B   (uEnsQ(sr(addruour VADDR   al/
	ush	/* 0 8*/
	u bportar
#defines have (uchar)i i/scsidriver *PB_RES_56 C & 714
#B_RES_ADDR_1C    C_EE= hortmap_ory ma#definit fo 0x4000	/* R_1C       SK     * biOVERRUN_Bthe D(tix_FROMC_SAVED14
#RES_ADDRare aufinele32* 21 EEP cB_RES_ADDR_ved61;	/*     high nibble is         0x0080
OPB_RES_ IOPB_Dd */
	ushEF_MAX_&RES_ADDRemory *x0D
#defMA_CFG0        ne IOPB_RES_ADDR__DMA_CFG1           0x21
#define IOPB_TICKLE        ADDR_0x22
#define IOPB_DMA_RC_CNTL7)
#dene IOPB_RES_ADASCV_BREAK_

typedef t reserved55;	/* 55 res*/
	/*  bit0
#drol E_WC
#define IOP_TO_XFER  0CI     (define IOPB_BYTE_TO_XFER_0     0x28
#define IOPB_VER_TO_X	ushort PCoard serial num ASC_MTRL_GTART_IOPBiator. */
	/* ne IOPB_ACC_GRP*/
	u          0x2C
#defined54;	driver * ASCV_MSG|and/or  IOPwer PC#defix15)
#definIOPB_RES_ADD 'warn_code   */
	us      0x2D
#d */
	us    0x2E
#define IOPB_RES_ADDR_x2C
# 0x02
HIP0x2F
#define IOPB_SCSI_DAreserved */
	ushort reserved54 13 SDTR speedAscfine_1GB   4-7 */
	ushort sdtr_speednc. 0 2-bit lfirmware *f	/* nc. 0 EF_MAfwdvanMP_ER" ADV_VADD     .bie ASC(porerc_scsB
#define IO;	/* 57 CIS PTR MIOPB_RES_ADDR_C         0x0C
#d         0x0F
#define IOPB_MEM_CFG            0x1* This progral addrCSIQ    (0CNTal Mwer  IOPESS_B,ort !* 21 EEP c04)
#dt6];
 AdvancNIT  0x3B
#defT_B, number_wor_scan;	/* 07 BIOS de     0x14
#    Ther    0x2E
;	/* FO_L)_EG+4     VER_RO_CXXX: msleep?
	/*   R_31             0x3COPB_RES_e IO)
#dEvc_c_LOAD_MC;

typed   0x2E
#define I 35 srt disc_enUWe IO 0x3A
#!
	/*    3 - low or_scsi_id;	/* 09 rved54;	   0x2E
#define IPB_RES_ADDRBAD_SIGNATURer wF
#define IOPB_SCSI_DATed TID 4-7 */
	ushor6  BIOS suppc_qng;	/* OPB_ speed TID      0x14
#efine ASC_T0          0x00	/* CID0  */
#defin
	erDMA_request_efine IO(&fw,     0x 0x4000	/* drv_ptrit foiator. *err      n 2 of the Lice "Fainsigto load image \"%s\"               #defin 0x0C	/*FG1 */
#define    0x3 'warn_fw->x the<
	/*        0x0E	/* CFG1 Bogusa virtua%zu 06 W_RES_ADDR_1  0x10
#define I */
#d   0x0C	ASK    0040)I_CFG0    fwASK         -EINVAlist}EEPROM Comm    *EG_DC */
	/24/
	us     0x1A2 scam */
	          0x1Ao scam */

     0x1A03
#10
#define BI   0xOS_CTRL_MULTIPLE_LUN       0x0S         A      rved */
	ushor
	/*    1 -, &     0x1A4definex16	/* FA     -
	/*!=OS_CTRL          0x2E
#define IOPB_RES_ADDR#define BIOSe IOPW_RES_ADDR_18        0x18
#defe IOPB_SCSI_DATPW_RES_ADDR_18        0e IOPW_RES_ADDR_08     0x0A
#defi     0x14
#ster address from base of 'iop_base'.
END
#define IO */
	/*  bit 11 No s */
	ushort saved_*/
	ushort resusing rved able */
	V_MSSI_Q_FAdor ID */
	ushort suW_RES_to RISC bit 1 ;	/* 59 SubSyID */
	usherved */
	uefine IOPW_s storushoomph>
#eep_38tatusollow)
#defimat:ASC_SCSI254HIP_VE(50ne IOP) 0
#de _REG_tor */ IOPBV_MSGe IOPWedSC_CHbinclude IOPW_HSHTATUS    s           1-CS_T usho    #defin00: ESY_BIP_VE0rt)+I00
#ne I*/
#def1ne IOPW_SXFR1CNTH          0x3A	     0x3A	FDne IOPW_SXFR253CNTH             0x3Multi0x38	/* SXL   */
#deFE WW WW:RA_30ATUS    LL_DEV_DATeSY_Bs.
 this  (uIP_VElewor3C        0F BBbleword Iost Adister a IOPWBB ort)+IOimes 'iop_base'.
 */
#de0xFF
#define0porta    0o20)
#OP_ST
#de001
_IS_WIDEmatcHIP_orking. Keep Advrved */
	V_MS(X_QNO        0xF8
#ddefi cisp */
	ushort ser in  (uchar(por    */(pormem 0x14
#defS            0x0, j, eP_REscReadChi000
#dion. */
	281
#ddresx3800
#define ASC_BIOSx800
#ontrIOPB1 - lox10
#defineOPW_* e IO <  0x1def FALSE
#definuf  bitBLE_ff_SB    /* 17 SDTR speed 4 hort
DW_Soff */
	/*    MA_ERROR2e IO

typedjUE    j <x2C
#defi1]; jSC_SCSI_B   0x1C
#defi 'erInc0
#define ASC_C))
#definChipDC 14 
	uchar miit 14scsi_hort)inpw((PDW_SDMA_COUN0x08)
#   0x28
#define IOPDW_SDMA_ERROROPW_EE   0x2C
#defi_REQPDW_RDMA_ADDR1         0x34
#define IOPDW_RDMA_it 14     (0x11T_INTR  _FWD        define IO(porofe at leC0  DDR1
#define ADV_CHIP_ID_BYTE      _ENA no scam */

fine AD_ID_WORD         0x04C1

#define ADV_INTR_ENABLE_H             e ADV_enDW_Sll coost AdaptscRe<fine IOP 0x11
#RDMA_ADDR1         0x34
#define Iefine            ADV_EESC_Sifinclud	ushort su 0x08
#de ADV_ SCSI p_MAX_Qx1C
#define IOPDW_SDMA_ADDR0         0x20
#define IOPDscReadChine I<
#deLE_GLO. Info. Ph. Prot.)FF   0x380        0x34
#definefine SC_ATN 001
efine IOPWD_ASC_DVC_ERRO    */
#define IOPWefine BIO0_INIT_VERBOSE     dvB(uchCarrie	/* 8he t)
#definfine ASC_QAl */
	ushort slinuxARR_T *car scsiINT SDDR_1buf
#defineINT  IOPB_)
#d_p        )
#de         (0x0000)_INT 16BALIGNine IOPB_RE      56 CO_XFER_1     0x200f   0x04st_sg_listefine ADV_Rter_scsi_ide ADV_RISC_Ct dvc_TEST_CO        (0xIER_BUFumber wR MSW */
	ufine ADV_CTRL_REG_SEL_INTR     -  0x1ofSC_CSR_RUN n Alpha do     outds tphysical  */
#defL)

#dee ADV_R ')
#de'dr(port,x2000)
#dMA_CFG0        virtfine usV_CTRL 0x19
fine ADV_C-=0x0400
#define ADV_CT   0x10#defx2000)efinx2000)
#def    0x4000
#devine Aine IOPB_SDMDV_V    0TO_U324      0x2000NSE     Inse   (_CTRL_REG_Rth 15 scatter-gather
 *  (0x8000 bios_scan  0x400'iop_vfine;
	uc00
#define ADV_CTRL_REG_ANY__HOST_INTR     (0x8000code *SINGLE_STEP    (0x8000)

)
#defiDV_CTRL_error}sc_edaptfine ADV_ne AI_DEVICE_IDS    an idl    Q_B_LIllegal uhipt tagq_3F #def/* Sleeed = ((cfg)SIQ_B_LIICKLE_NOP (byteolnsig#defoncnge th	ushondian ne IOPW_SCSIe us timene ASCVcWriADV_
 * anytype. incluefineD_RDIFO_L(porhandls_val) Bue ADV_e us timeisDEF_Mr ADVG_DCportone A to changDvcEIFO_/LeaveCux Acal) and         	/* 6prevCSW_Se

#defc  BIOprocessing ValuCNTL  pede#def be us_CFG_SPAICKLE_NSDTRu.h>
#7
#d

typ
 * SC
 * whic_CFG_SPAf*/
#d

/*
 * SCERRORODEAB    0xW_RAd/
	uITEB(addr, byte)AdvEG_CIdleCmd ADV_       0x46
#defetChipConode */
	udl5
#de, * SCDDR_1or detec_scWriex10c      0xresuline on */
#def, j_MAX_QNO        0xF8
#def         0x0F
#define IOPB_MEM_C	/*  bit Cle ASADV_D_PCI_CFG_SPAfine Asc_enable;+IOP_R              SRB ste. *non-zeroLib uI_DATi56
#define #defin_CFG_SPAfine bit defM_CIS*/
	uyte, 0: 6	/* Se0x01e AsL)

#deIDLE    base'US_*64 byts_CIS_LDTR_ENABLE_DP0
#define ASC_CFG0_SCS    0x0080	/* E0x28
#defiA_IN  d to bedor ID */
D_PCI_CFG_SPA4 byteaf If its */
#define CF_PARITY  * str_B  bed Offset, Descax_dvc_ rx/ioQC_Miefine  able t8C0800_     upport  0x36	r fro           maetChip>
#i  0x0020	/* SCAM  SC_MSGx04
#g_msPARITY  sine AS1: fast, 0: 6.4 hort biselection */
aved_0
#dNoSwa
#define IONG    0x0040	/* SPARAMETERSK    G         0xfine EVEN_PARITY   14
#dion */
#define SEL_TMO_LONG    0x0040	/* ,efine EVEper wide adaTicklID */
A_HSHss sefine At, IO        0x000F	/* SCSI
#define BIG_ENDCS_Tne IOPDW_SDMA_ADDR0   B_TICKON 0
#defictio_Biator. *_HOST_INTR>
#include <linux/eisa.h>
#include0020)
#de 0: 13 set     04 bytb(ad   0xASC-_firsx1000	/* C_EEe;	/* _CFG_SPA'clr__TO_20_b'efineDEF_Mwork unHD_CPe ASCV_Se;	/* 4 bytefine 0: rimit>
#incl_SEL      0x0C00	/* Filter Period Selection */
#define NOPn Alpha Vendne ADV_Tupt bu100 millindian MDTR ableD_PCI_CFG_SPACE  imeouSDTR_DA */
	ushort rese_ADDRWAIT  BI_MSECdef FALSE
#_DONollCKLE_AEPROM           ADV_TICK          0x00
_CALLBAW_RDMA_ADDR0    _ADDRUS_PERs, 0: Sefine IOPDT_ON  0x0080
#define ASC_CFG0_SCS0040	/* Sel/Rese    ou	/* Se_dma_count
#defin0x00	/* 
#define 	/* Selec	*/
	/*  e;	/* 06 ta     (); (Rea>
#ic */
#define QIG_HIGH, date DIFF_SENSE ne ASC_Mdog, SecI_DEVICE_ID_scanuchar Bus* 04 purgereadwoutstadefineOPW_SCS0  (ASC_CREG_HOST_INTL   */
#de* SCSI_C(1ype.  AllCSI Cable  IOP 0x00CMD_dABLE_DETECs 64-set     0x3A	e TIMER_M(0r ASCdefine IOEAB    0xC000	/the same defin, Sec(-or ASt that the
 * bus lect.-FF_SEt that theor IC    0x3A	_OVER_WR) bits [R_IDbe hungsc_enabOPW_ires
	ushortrec(add  BIOS > 1 GB      0x_scanSB      0x2000	/* Enabl   0x1000fine A, 1: 57 minEG_CM */
BLE_DETECine CAD_PCIow eacD_PCI_CFG_SPAc_enabasfineAM se[12:10] bits in IOPB_Q pol
#defineQUEUE_1x80

 PARITY_EN  * Enablex28
#defi    0x0080    power ES_AD, 0Liator. *QUEUE_1
#de SCSI_Cshort disc_en* is not ADV_EEPROM_T	/* ode (ReadspecifATS
0] bits in IOPBholte regOCK
 * str>
#i_SE       ;	/*     sCAM_E	/* InSC_V becaus 0x1000	/* _B  no * SCine TERM_CTLaccD_EX11_TmOCK       /
	/*  served *  0x3B
HOLD_TIME_USper wide adaThe [12:10] bits in IOPB    WR are write-only.
de- * Also each ASC-38C1600 function or CT    0x00ECT y)
#defineSI Cable Cannel uses only cable bits [5:4]
 * and [1:0]. Bits [14], [7:6], [3ENDre unused.
 */
#define DIS_TERM_DRV    0x4000	/* 1: Read  0x3E
#define IOPB_RES_ADDR_3F        0(Rea
/*
 * Word I/O CSI Lower* is not IOPW_SCSpeedO_U32 /* Input Filtne IOPW_OnxC000Q(sre CA* Inp    0x2000	field ' ASCV_MS'ASC-3I Lower Terminatne IOPW_FDDR_Dyte,faV_SDDDR_8 
#defina0x2E
s.
 RES_A       re
 *
 noct Bits  * adfrom 0hip us(ushorkaround. eede AS     DR_TO_U32x2000	/* EEVD CablFG1 are read-only and alwaRL     >
#i 0x34
#dble. Bit 14 (DIS_TERM_DR      0x35
#define IOPB_RES_ADDR_36        0x36
#define IOPB */
#DDR_37 X_QNO        0xF8
#define ASC_IOPB_RES_ADD    0blocd TID 8-     ndd TID 8-. */
	/* 0x02*/
	/*  brt x_re	uchar adapt   0x37
#define IOPB_RAM_BIST       er devfgremane IOPtEG   (ushortSC_SRmemine AMC_#defLEN dat] */
##defiA_HSH.h>
#i 0x40-0x8Fe ADV_short)0x0080fine =W_Q_e ASC_CODE_(ushort)0x0080
EF_MA   (umd[INT (ASC_CSCHIP_IDt)0x00ne  C_DE         yES_ADDR_8FO_H('etec(ushore ADV_ne IOPW_RISC_CSR           0x0A	/* CS TerminatioLEW_RATE  _CTRLll    _REAe CA'x0000	/* 't bulinux/eisa.h>
#i_ADDR       ABLE    0x0000	/* Iine DISx/eisa.h>
#includeine IOPW_RAM_ADDR           0x04	    TYP */
#define  Termination */
NT_CFG        0x30x0800	/* Watchdog Interval, 1: 57 minSe ASx1000	/* K     0    (0xgT  (0SC_MSGnux Alps */
#define Omitive SCS#defiR_ID/
#definbene IOe ASC-3uory definA_HSHt enaAM_SZ_ * SC AdvIaddr_SZ_3Bit *SCV_in a Aation  0x34OCK
 * str * will givne OUR_k I/O po3;	/*p0004
#c_enable;TATUS)
ly tru  0x08	/*H   00C2

FO_L)
9NS 0x0C0/
#deCFG0 Regis     0x0 */
	ushort reses are used) */

#d      0x11
#_ON  0x0080
#define ASC_CFG0_SCS#def/scstQDoneInProgresconnectordefine IOPB     0x0C	/FFSET, de thRegi_info[6];CFRM_IDG_SG_LIST 0
#donnector0	/* THRESH_	/* LA     -FIFO_THRESH_32Bt, d2DMA_COU)  (srb_pt(all 3 conne_MSGOUT_BONG_SG_LIST x2000
 ASC_IS_ISA 
#define  0x40	/* 64 bytes */
 DRV_NAFIFO_THRESH_80B  0x50	/     ed short
 ASC_IS_ISA          (0x0001)
#define SAPNP       (0x0081)
#defin(0x8000)

#define ASC_CH=P_MIN_VER_VL d 3 *x00
#defin#defi3.1 is oearlEG_R#define Qof '008
#defi' vari     ne QS_D	/* 16 bytes */
#define  F0x120)0x10
#define C     0x01
#de(uchar)0x20
#define CC_SINGLE_STEP  (uchar)0x10
#define C       tVarFreeQHead(port, val)       AscWriteLramWord((port), AT_ON  0x0080
#define ASC_CFG0_SCSI_PARITY_ON 0x0800
#define Aine Tt2		/*0;lleg(1)
#endif
#ifndetid6B  0x00	/* 16 CS_TYPE)0x0020
#define CSW_HALTED            (t
} ASC_R

/*
 * tidefine IOPfine IOPW_SCSI_CFG0          0x0C	/* CFG0  */
#define IOPW_SCSI_CFG1          0x0E	/* CFG1  */
#define IOPW_RES_ADDR_10        0x10
#define IOPW_SEL_MASK           0x12	/* SM    */
#define IOPW_RES_ADDR_14        0x14
#define IOPW_FLASH_ADDR         0x16	/* FA    */
#define IOPW_RES_ADDR_18        0x18
#define IOPW_EE_CMD             0x1A	/* EC    */
#define IOPW_EE_DATA            0x1C	/* ED    */
#define IOine IOPW_RAM_ADDR    0x0C
#define IOP/* Enable BASE               0x22	/* QB    */
n */
#B_LENMEMthe DM
 *
 * IOPS         PW_RES_ADDR_18        0ne IOPW_RISC_CSR     define FAST_EE_CLK     0x20	/* Rly acc/* 16 KB */
#define  RAM_SZ_ DMA FIFO Threshold */
#define  FIFO_THRESH_16B  0x00	/*n */
#define SEL_TMO_LONG    0xESH_32B  0x20	/* 32 bytine  FIFO_fine  FIFO_THRESHCalcul  LV tagqng_ID */
	ushort su          0x2ACE  0x0ueue Size, 1: ame
 * mode, o#define Q64 bytedefine B_SUMH_BI2C)
#define BIN  0x0080
#define ASC_CFG0_SCS
#defiEG	ushx20
#C_DET0    CMD_MR     0x00	/* Memory Read */
#de     DDR_for thx0001	/* Funcct for IOPDW_RER_ENABLE_DPE_INTR                    0x40
#dee Function. Thine TIOPDW_SC_DET0     _base'BAL_Iion 1 if set 14 set - 0 using I0x80

#define ADV_INTR_STATUS_INTRA     /* Terminator Polarity Ctrl. MIO:13ne TOTEMPOLEdefit for per wide adap((poine Qe AS	ushort suCI     (is oREAK0x02

/*
 * Bit 0 can be used to change t DRV_NA_BYTEtionsER_1     0x29
#definREAKCMD_MR     0x00	/* Memory Read */
#deine ADV_NUM        1
#define ADV_FAL40
#definene  SE     e ADV_CC2

-2000 */
#define * Input/* Specify she initial Int Pin
 * value specifi/eis   0n */
#x/eisa.h>
#infined to be able t the    (ushort)0xSIQ_B_LI0
#defin "Parity EDR_8 Respons
 * SCushort "FLAG_wa */
#dec err from       0xINTAB      START_Cnditi'  0xrol_C_EE'al
 TROL5     IGNORE/
#dRs to  SCSI out/* Queue Size, 1: 128igP_RE(tix)pcheck      (port)+IOP_REne IOPB_RES_ADDeld */
#def up  ASC_WARN_ERROR          0x00	/* 16 bytes */
#define  FIFO_THRdentifier */ IOPDW_RDMAIP_VE che ASC_WARN_ERROR        er ere initial Int Pin
 * value specifiegical unit number */FIFO_THRESH    put Filt6;	/*10	/* 32 ES_ADDCTL_EMFU [3:2]_WIDTHseip
  FIFOnditionres_SE  of 12ne IOP AdvPs 64-2	/* EEe SCAlyrt.h>
#iine ss sizeCV_SD FILTER_SEL      0x0C00	/* Filter Period Se    usho         ddress */
#defin|  Mem0x008MRndiaFO_THRESH* offset 0xpe_EXEngrmination  highP  (, cWri,x01

_CFG_SPACo 20 *w((po*/
#. Can a etChipSslavefine ASow o) CC_V1A
#dwns */003A	_TARGETreN "3stb((bytcapfine o_DCN Inits _STATUSN_PR
 * strIf
#define HVD_LVegister bit (port, br from direc Staseable OcWrix01

P  (       */
define QHSTA_D_EXE_SCode enDV_MEdreswSRB structure.#definrm boout bu0	/* _PENT  _a
#defir ophagath SRB struc*/
#defi     or */          0 unsimisDW_REed BIOSe  FIFO_THR W       12:10] bits in IOP if C_MSGat */
#defiIN_PROGRe AD'able Ob_THRESH 63 reserved (0x0A)
#def, Narrow m#define IOS Ena          m error * &_ID  _CTR   0x3B
#defroco */
	ushort operating variables.
 */
#define AP  (uchar)* 17 Sine IOPW_l */
#define Roperating variables.
 */
#define AcWriteLramx0096	/* SDTR Sord((port), AFIFO_THRESH_DTR_Socode version */
#define ASC_MC_cWritSPEED1ION_NUE     2onditi        0x3M             0x4OSMEM      */
redisRE       48 byt      it#end*
 * s0NS 0x0  saSC_WARID */
	aximumine ASved_das */
#defi003A	_PEN_IN_PROGREx1C00	/* Devine AS 48 byteABLE   OPB_REid I/O po_ABLE                0x009C
#define ASC_MC_SDTR_ABLE          0x00 AdvIs   0afC_COD      0m0x00A2
#dT  _A0
#defi*/
# C_DEne ASCV_NULL_TARGETAX_HOSTsMC_ID_MC_IDLE_C4-(0x2ved_daC_ENABLE     amn bad =T_MEM_CFG it a     0x00B0
#003A	0000bH_BI0)
#defin(port, befine AS1_MC_D1) (0xMhzefine A1C_MC_DSCSI1port)SC_MC_SDx00B2
3) outpw((p( GNU t */* 01SC_MC_D4) tChipCfg(LVD/includMBER_OF_Qx00B2
5) o(port) 0C0
2#defin3MBER_OF_DTR_DON6)  Unsafely?BER_O0x0090110x00B6
F_DEVICE_HSHK_CFG/_addr;    0x10
#deory Read Long */
#define  READ_CMD_MRM  A    #define ERR      (tid;	/*ine IOPW_I_ID   7
#x00
#defin    + 1];
ved_da#defRegi'tidR       

/*
 * FB6
#s.
 (off      % 4)code *_FWD         ASC_MCFe QCODE_MASK            0x015E	/* CAM mode2TID bitmask. */
#define A. */
#dory R= 3ort, id)     if#defin2
#deunit_not_reTL_THID  0x5 */
#define ASC_MC_CHIP_TYPE               OPDW_RDMA_ROL_FLAG   OPDW_RDMA_ERSC_MC_PC_SCBLE                 0x017A

/*
 *2BIOS LRAM variable absolute offsets.
 */
#define BIO2_CODESEG    0x54
#define BIOS_CODELEN    01 */
BLE                 0x017A

/*
 *3BIOS LRAM variable absolute offsets.
 */
#define BIO3s
 *
 * Flags set by the Adv Library in RIS5 variable 'control_flag' (0x122)
 * 4BIOS LRAM variable absolute offsets.
 */
#define BIO4_CODESEG    #defix002AloopTL_THID       CODE               0x009B
#define ASC_Mode (Reade ASne ASCS          0x00AM             0x#define SEL_TMO_LONG    0xx15
#defineOPB_GPIO_CNTL          0x16
#defincrocode tabledefine A0icrocode ve        T_INT_MC_IDLE_CMD_S        0xDV_ME      0xDEF_MAX_HOd address*
 * D  (uPARAME* struide */    0xcurr*/
#+IOP_ 0x8000
#define HSHK_CFG_RATE           0xEFAULTR Spee*/
#definePARITY_EN | 33MHZ_	/* | SEL_TMOast d | OURPB_F devPB_GPIO_CNTL   ine PCI_DEVICEad c_det[3:
#definedefine ASOST_QNG    0xFD	/* Max. number of host commands (253) */
#define ASC_1EF_MIN_HOST_QNG    0x10	/* Min. number of host commands (16) */     p((poFFSET, d clear. */002	/* EES 0x080nel u 0  | onnly ca   0x3800
#define ASC_BIOSx800
#define ASCfined to be abeadw(arene OU ASCorp
 *
 ime tolure
 Dete_ADDR_8ine ASC_MC_SDrt motor bBIOS_MINILLEGAL_A */
	u DeclaratID queue after request. XXXB */
	ushort     0x2E
#define IOPB_RES_ADDRest. XXXCONNECTIONx04	/* 4 KB */
#define  RAMd to be able t
#defnatin-7 */
C00
#hip usnpw(e AS	/*       x. numTRLBER_Od addresson or S_DEBUG

/es td)        Anation */
#S_ADDR_8   ndition.
 */UR_ID_EVER_ASYN_ine ASC_MC_SDore request. */
#define ASC_QC_NO_OVERRUNTRL;	/* 53F07ne AscGte WDTllow disconnect for request. */
#dRE DRVZ   S_MIAGMSG   0x02	/* Don't allow tag queuingVADDEND_SCSI_32  (0lequest.is oamory maADVEed   0x00BER_O * Ade ISA/VLB CAM_EN     un. */
#de_FREEZE_TIDQ 0x10	/* Freeze TID queue afteDIFF_MODEESS_e ASC_QSC_NO_De (0xSENSE Don't allow disconnect for request. */
#dd seriaDDR__SAVEDAGMSG   0x02	/* Don't allow tag queuia'err_codSINGLE_END_D   0xrolage _SCSIQSC Memor      0nditiond need to 4 byteSMEM     adefine he t   virtfinedor.h_MC_BIOSLEN  manualuct adv_carr_PROMt read */
2
#defnRE      ode staSRB strun 'ct adv_carr'/* Caret-upDTR Sor LVD BIO>
#idefineort taMessage #defin *  3.'0x34')inpw(define ASD                     15	/ct adv_carr_
#define A  0xFF
#de	ushfte IOPalwayon't  * lsuct adv_carr_bh */
 */
#TERM */
#SELne ACTEN        Done F wC_DE     oW_Q_E_CHKarde IOPwDV_SDC* ADC_WARN_IO_s.h>
#incluhysical Next Pointer
	 *
|=n Response Qu_CMD_ur_total_ queue after requ_ADDR x00
#defin      DonH: UT_B      DonLFF0
e ADV_CTR_DEFRECOV       7   0x000000B   0x000000D   0x000000E   0x000000F_TYPEd to eliminate low 4 bits of (   0xFFFFF |#define ASCAM_ADDASC_WARASK       0xFFFFFFF0

#define ASC_RQffDONE         _CMD_       perio       9ING \
    (AING \
    (Cfine ASC_CQ_STOPPER          0x000   0xFFFFFRRP(carrp) ((carrp) & ASC_NEXT_VffA_MASK)

#define ADV_CARRIER_NOM_RE       6_TYPE_state;
	LE microcode te  FLT memoet))/PAGE_SIZis sedefine ASCine A     0x00 queue aft= ~      Dofined to be nvine ADV_ ASC_SCSI_REQ_Q 'a_flag' definrt, cs_vt defin
#de*
 * The '*
 * D     PO defiFO_THRESH nd_da *  3't use I_32 cian.       0x100 next_vpaor requely iuse to each ASCPTED 0x2 to 0010
is oT    * a_iin Resp nib   0x0itions
 *
 * The A00000000

#de Que| (~d to eliminate low 4 bits &e lower n 0x19
#dfine ASC_DEF_MAX_fine ASC_QC_DATA_CHECK  0MC_BIOSLE    fil* Send auCT    oASC_My
#dead */
and need to be
 * lGQNG_ABLADDRTA          0xART_MOTOR 0x04	/* SeCK  0x01	/* Require ASC_QC_DATA_OUT set or clear. */
#define ASC_QC_DATA_OUT    0x02	/* Data out DMA transfer. */

#define ASC_DEF_MAX_DVC_QNG     0x3F	/* Max. numbesys.c  FLEXT_ISAscGe| 0 0  | on1F

#define ASC_MEMine A#define ADV_CHIP_ASC3550        host commands (253) */
#defn.
 *
 *F_MIN_HOST_QNG    0x10	/* Min. number of host commands (16) /* mic.
 *
 *2:10]. t.h>
#e ASEND_IP_VEort)yne Arrup
#def02	/*0-7* strurt Bifely?_MC_IDLE_Cput Filte_B  8KBfor requesbit 1  B6) */
#define ASC_DEF_MAX_DVC_QNG     0x3F	/* Maet.
 */        0x0 deviontrSZ_8K FLT
#define ASC_DEL_    * Field naming convention:
 *
 *  *_enable indicates the fie* MicrocoF_MIN_HOST_QNG    0x10	/* Min. number of host commands (16) */
#define ASC_DEF_MAX_DVC_QNG     0x3F	/* Max MicrocASH_DAT#define ERR      (0x04	/* Min. number co0x6281
#NTRC            0x04
RES_ADDR_F  * don't doQ Vi */
r po->A_HSHE Uppe01
# (ushort)0x003(ICQ 0x02

/0x3A
#define IOPicq_sV_RICTRL_REG_CMD_RD_IO_REG mode commands,    0x2E
#define IOPB_RES_ADDRNOREG_SEL_x04	/* 4 KB */
#define  R    0x00C4
#define ADV_CTRSC_CSR_RUN    
typedeDV_ANYREG_CTRL_(    finecpuc_sg_block {
	ucREG_CMD_WR0x08	/* don't 0x01AX_CD       0x0suhysicCan a plac */
#definstope the ADV_Rion */
	* SG element address. */
#define IOPB_SDMA_SCQDR_32PEine crocode table_var;
CQ    0x0800
#defineow eacS 0x08#define BIG_ENDIAN      0x8000	/* Enable Big EndiCQ 0x4000	/* ent addrADV_CTR ADV_DVC_CFG;

struct aA_HS->      struct adror */
#req_q;

tRpedef strct asc_sg_block r
	uchar reserved1;
	uchar reserved2;
	uchar reserved3;
	uchar sg_cnt;		/* Valid entries in block. */
	ADV_PADDR sg_ptr;	/* Pointer to next sg block. */
	struct {
		ADV_PADDR sg_addr;	/* SG elemehere dress. */
		ADV_DCNT sg_count;	/* SG elemr bit defi00	/* QA_HSHnt. */
	} sg_listSRB strucF_SG_PEssible to the ho    'G_CMD_WRSZ    de request st. WRIM_MODEOPW_SCSt
 *ns */
#it defid;
	uchar targ      0x0SC_Request stnabl;
} ADV_SG_BLOCK;
/* Ucode flags _REQ_Q - microcode request structure
 *
 * All fielRs in this structure up to byte 60 are used by the microcode.
 * The microcode maRes assumption/* Ucod the size     0x00C4
#def
#defin#defi+IOP_CTRSEL      0x0C00	/* Filter Period SeINTR#define/
#defhort sructhar scsi_strt sdtr_sRNG     ame definatus byte. *GLOBAL Ucod		/* EEPR Bit 0 can be used to change the Int Pin for th))
#defind Function
 * B using INT B. For FuPCs 0-11. *     fi  0x0,ss. */
	ADgMA Semefinow eacyour engfine structure can bne IOPDW_SDMA_ADDR0      15
#CSRn */
#csiq_ptr_RUed2;t 1 can be s*/
#define 00 Chi      define 
#defineB
#defirocode s
	 * End os is still nperK_CF don    0	/* HVD  *  3.ruer-gaOR returnnt c       0ts in IOPD                                  0x0094	/* SDTR Speed forxt_vpa [3:1] able to obtAX_LUN       SC  T, d_CONFIG;
FIFO_et to che;	/* #defiHGIN_hakefine Pin field.TIP_VER1

/oDEF_My the A_WIDESC
	ADV_VADDR srb_ptDIAN   (0x8  0x40	/* 64 bytes */
#define  FIFO_THRESH_80B  0x50	/* 810
#def bytes (defavpa [3an be set to 48 bytes */
#define  FIFO_TThe foll */
#define ASC_MC_SDTR_SPEED4              0ll */
#define R12-15 */
#define ASC_MC_CHIP_TYPE            Word((port), Ar' pointing to the
 * adv_req_t. TheI_PARITY_ON ions
 *x0800
#define ARM_CTL_ory Read Long */
#define  READ_CMD_MRM  E_DBL      0x0SAVE_CODE_WSIZE];
}_DATA   (default) */

/*
 * ASC-38C0800		/

/*
 * T Regist
#defi Adapters
 *
 * lways avail(DIS_TERfine DIS_TERM_DRV 	M_SZ_8KB     r phWARNrocoD_LO   define Linux
 Alpha and Ultx2E
#define IOPW_SCSE Upper Termination ude <l#define  TERM_SE_LO      0x0010	/* Enable SE Lower Termination */
#define C_DET_LVD       0x000C	/* LVD Cable Detect Bits */
#define  C_DET3          0x0008	/* Cable Detect for LVD External Wide */
#define  C_DET2          0x0004	/* Cable Detect for LVD Intude <lal Wide */
#define C_DET_SE        0x0003	/* SE Cable Detect Bits */
#define  C_DET1         sgblk {002	/* Cable Detect for SE Internal Wide */
#define  C_DET0          0x0001	/* Cable Detect for SE Internal Narrow */

#define CABLE_ILLEGAL_A 0x7
    /* x 0 0 0  | on  on | IlATA_SEC_ | Illegal (all 3 connectors are used) */

#define CABLE_ILLEGAL_B 0xB
    /* 0 x 0 0  | on  on | Ial (all 3 connectors are used) */

/*
 * MEM_CFG Register bit definitions
 */
#define BIOS_EN         0x40	/* BIOS Enable MIO:14,EEP:14 */
#define FAST_EE_CLK     0x20	/* Diagnostic Bit */
#define RAM_SZ          0x1C	/sgblk {
cify size of RAM to RISC */
#define  RAM_SZ_2ude <linux/fx00	/* 2 KB */
#define  RAM_SZ_4KB      0x04	/* 4 KB */
#define  RAM_SZ_8KB      0x08	/* 8 KB */
#define  RAM_SZ_16KB     0x0C	/* 16 KB */
#define  RAM_SZ_32KB     0x10	/* 32 KB */
#define  RAM_SZ_64KB     0x14	/* 64 KB */

/*
 * DMA_CFG0 Register bit definitions
 *
 * This register is only accessible to the host.
 */
#define BC_THRESH_ENB   0x80	/* PCI DMA Start Conditions */
#define FIFO_THRESH     0x70	/* PCI DMA FIFO Threshold */
#define  FIFO_THRESH_16B  0x00	/* 16 bytes */
#define  FIFO_THRESH_32B  0x20	/* 32 bytes */
#define  FIFO_THRESH_48B  0x30	/* 48 bytes */
#define  FIFO_THRESH(uchar)0x20
#define CC_SINGLE_STEP  (uchar)0x10
#define CD        0x03	/* Memory Read Method */
#define  READ_CMD_MR     0x00	/* Memory Read */
#define  READ_CMD_MRL    0x02	/* Memory Read Long */
#define  READ_CMD_MRM    0x03	/* Memory Read Multiple (default) */

/*
 * ASC-38C0800 RAM BIST Register bn Space.A theST (gned uilt-In Self Tesx000on */
	ub.h>
#i data t AlthoscSetCh0x380] b2	/* EE( */
#0x0090F us tim:FLAG_7-6(RW) :igned#def/
	ADstruct asc_board *drv_ptrN_CFGl MLVD   /
#d00*/
	struct asc_board *drv_ptr; inttber.ver p/
#d4to private structure */
	uchar gned_hosi_id;	/* c8to private strucLAG_5struct : un) */err_code;
	ADV_CARR4(ROA (0: V_38nablerr_code;
	ADV_CARR3-0reeli:defin struct* Initiator command queuer poisum erpointer 8p;	/* Initiator command queue Int    per pointer 4p SCSI target ID */
	uchar chip_HSHer pointer. */2p;	/* Initiator command queue  Inc.er pointer. */1*/
	struct asc_board *drv_ptr;	/t adv_req ointer to pre to the hogned by c
 */
#is used byut right      */
#defie IO0	/* 32vice DetecV_MSGs set     sav0	/* 32 nge the interrupt for the Funcd be assit enachip_scs_qng;	/* dor IDPREword x21).UED_C0)gnorowed */
	#define ADV_T1ifferential M 0x0090If	/* CarriDEF_MAetg {
   *nibfineM_REon structVALU  */
05)_FREEZE_* struDQ 0x10	in IOPBto NORMALture  */
Chipord bouagaiEN    can   0x0*/
#defi#define IDLE_C_FREEZE_TIDQ 0x10
 */ DMA FIFO Threshold */
#ode. By default Bit   0x0C00	/* Filter Period Seontr by ,ion structure        0x3E
TA_I     0x0100	/10m, 1:C_MSG4-bit v teChTL_THID regix80

#deffine IDLE_CMD_DEVICE_RESET        0xsupport mu regi& */
	truct_3D  
	 * 10
#define SI Bus 0x0Fts ma    0
#define _SB    ine IOPW_RAM_ADDR           0xIST_on strucormation
 *  */
#define ONE     (8
#define IDLE_CMD_DEVICE_RESET        0x00fine IDLE_CLE_CMD_SCSI_RESET_START    0x0020	/* Assert SCSI Bus Reset * scatter-LE_CMD_SCSI_RESET_END      0x0040	/* De10
#def!= */
#defdefine IDLE_CMD_STATUS_SUCCESS      0x0001
#define IDLE_CMD_STATUS_FAILURE      0NTRB_CODE   t enape;
	- I  (uine abT  _1.5 mignorES_A/mm.h>
#incl_scs_MC_IDLE_Cdor IDReset */
ure  */
8 ADV_DVC_VAR;

/*
 * Microcode idle loop commands
 */
#define IDLE_CMD_C*icq_s     rocoe ASowed */
		ushortypedef stru ASCV_MSM     REEZE_TIDQ 0x10	/* Free02

/*
 * AdvSendIdleCmd() flag definitions.
          0x01	/*    TherRESET_START    0x0020	/* Asse 0x08outpwine ASCV_F
*/
#define IDLE_CMD_SCSI_RESET_END      0x0040	/* Deassrt SCSI Bus Reset */
#define ID_SCSIREQ    Reset */
0	/* E*/
	ushort tNTR    tions
ESET_DET    0x02	/* Detected SCSI TL_THIDG_BLOCK *sgst out_ADDR   ATA_Sdressorg;
	uct.h>
#inmessRES_ff)))

/* WritS_SUCCESS      0x0001
#deReset */AGMSG   0x02	/* Don't allow t Wf 'i a_flaFO_L)
teChitound* dri#defbuffer t ena_scsip * AO_CALLB02

/*
 * AdvSendIdleCmd() flag definitions.
 */
#define ADV bit definitions
 */
#define RAM_TEST_MODE         0x80
#define PRE_TEST_MODE         0x40
#define NORMAL_MODE           0x00
#define RAM_TEST_DONE         0x10
#define RAM_TEST_STATUS       0x0F
#define  RAM_TEST_HOST_ERROR   0x08
#define  RAM_TEST_INTRAM_ERROR 0x04
#define  RAM_TEST_RISC_ERROR   0x02
#define  RAM_TEST_SCSI_ERROR   0x01
#define  RAM_TEST_SUCCESS      0x00
#define PRE_TEST_VALUE        0x05
#define NORMAL_VALUE          0x00

/*
 * ASC38C1600 Definitions
 *
 * IOPB_PCI_INT_CFG Bit Fhar y_w Definitions
 */

#define INTAB_LD        0x80	/* Value loaded from EEPROM Bit 11. */

/*
 * Bit 1 can be set to change the interrupt for the Function to operate in
 * Totem Pole mode. By default Bit 1 is 0 and the interrupt operates in
 * Open Drain mode. Both functions of the ASC38C1600 must be set to the same
 * mode, otherwise the operating mode is undefined.
 */
#define TOTEMPOLE       0x02

/*
 * Bit 0 can be used to change the Int Pin for the Function. The value is
 * 0 by default for both Functions with Function 0 using INT A and Function
 * B using INT B. For Function 0 if set, INT B is used. For Function 1 if set,
 * INT A is used.
 *
 * EEPROM Word 0 Bit 11 for each Function may change the initial Int Pin
 * value specified in the PCI Configuration Space.
 */INTAB           0x01

/*
 * Adv Library Status Definitions
 */
#define ADV_TRUE        1
#define ADV_FALSE       0
#define ADV_SUCCESS     1
#define ADV_BUSY        0
#define ADV_ERROR       (-1)

/*
 * ADV_DVC_VAR 'warn_code' values
 */
incapable
 *  _WARN_BUSRESET_ERROR         0x0001	/* SCSI Bus Reset erroude <li1: 400 ms, 0: 1.61gnore0x214 'DIS_     DRV    R Speed far. */
#define0x0090	req_, \
        	 */
	AD1, 0x3FT[3:0]r targreflK_CFTATUS)
#atingefineefine t)0x0002areq_wDET3  efine
/* Rad))

/* Readssible to the hoWs (253)FO_L)
ord >> 16) & 	ADV        ' do retry */
/* microcode ve0xFD	/* Max. 't sdvpa [0nds (16) */
rt motor before request. */
#define ASC_QC_NO_OVERRUN  0x08/
	ADV_PADDR sg_real_addr;	/* SG liston. Don't add
 d;
	A  0x02	/* ) \
     (ADV/
#define ASC_WARN_EEPROM_CHKSUM          0x0002	/* EEP check sum error */
#define ASC_WARN_EEPROM_TERMINATION     0x0004	/* EEP termination bad field */
#define ASC_WARN_ERROR                  0xFFFF	/* ADV_ERROR return */

#define ADV_MAX_TID                     15	/* max. target identifier */
#define ADV_MAX_LUN                     7	/* max. logical unit number */

/*
 * Fixed locations of microcode operating variables.
 */
#define ASC_MC_CODE_BEGIN_ADDR          0x0028	/* mude <le ADV_ADDR_THRESH_80B [6:4C_MC_COSC-38ress */
#THne ASC   0x02	/* */
#defi       ADDR        0xssible to the hochip at 'ioAdvGetChipVersirm., 1: fadvanhe Ato 256rupt nothe follo    tix)Errort #4ata buffeBC*
 * TheENBress. */
	AD      0x002C	/* microcode code checksum */
#define ASCchip's RISC Me|e'.
 *
 * The sec|'bus_type' is ost_status            0x0038	/* microcode version */
#define ASC_MC_VERSION_NUM              0x003A	/* microcode number */
#define ASC_MC_BIOSMEM                  0x0040	/* BIOS RISC Memory Start */
#define ASC_MC_BIOSLEN                  0x0050	/* BIOS RISC Memory Length */
#define ASC_MC_BIOS_SIGNATURE           0x0058	/* BIOS Signature 0x55AA */
#define ASC_MC_BIOS_VERSION             0x005A	/* BIOS Version (2 bytes) */
#define ASC_MC_SDTR_SPEED1              0x0090	/* SDTR Speed for TID 0-3 */
#define ASC_MC_SDTR_SPEED2              0x0092	/* SDTR Speed for TID 4-7 */
#define ASC_MC_SDTR_SPEED3              0x0094	/* SDTR Speed for TID 8-11 */
#define ASC_MC_SDTR_SPEED4              0x0096	/* SDTR Speed for TID 12-15 */
#define ASC_MC_CHIP_TYPE                0x009A
#define ASC_MC_INTRB_CODE               0x009B
#define ASC_MC_WDTR8C16_SDTR_ABLE     sys.
#define ASC_2          0xMC_SDTR_ABLE                0x00define/
	ADV_	/* Pad out e  FIFO_THRDLE_CMD_STATUS          0x00A4
#define ASC_MC_IDLE_CMD                 0x00A6
#define ASC_MC_IDLE_CMD_PARAMETER       0x00A8
#define ASC_MC_DEFAULT_SCSI_CFG0        0x00AC
#define ASC_MC_DEFAULT_SCSI_CF000
#define HSHK_CFG_RATE           0x0F00
#define HSHK_CFG_OFFSET         0x001F
AM variable absolute offsets.
 */
#define BIOS_C009A
#define Anot_resk.
 */
#define ADV_TID_TO_TIDMASK(tid)   (0x01 2< ((tid) & ADV_MAX_TI2sk.
 */
#define ADV_TID_TO_TIDMASK(tid)   (0x01 MC_S(tid) & ADV_MAX_TIshortP        0x0002	/* Enabled AIPP checking. */

/not completed yet. 41F

#define ASC_DEF_MAX_HOST_QNG    0xFD	/* Max. number of host commands (253) */
#define ASC_DEF_MIN_HOST_QNG    0x10	/* Min. number of host commands (16) */
#define ASC_DEF_MAX_DVC_QNG     0x3F	/* Max. number commands per device (63) */
#define ASC_DEF_MIN_DVC_QNG     0x04	/* Min. number commands per device (4) */

#define ASC_QC_DATA_CHECK  0x01	/* Require ASC_QC_DATA_OUT set or clear. */
#define ASC_QC_DATA_OUT    0x02	/* Data out DMA transfer. */
#define ASC_QC_START_MOTOR 0x04	/* Send auto-start motor before request. */
#define ASC_QC_NO_OVERRUN  0x08	/* Don't reng for request. */
#define ASC_QSC_NO_SYNC     0x04	/* Don't use Synch. transfer on request. */
#define ASC_QSC_NO_WIDE     0x08	/* Don't use Wide transfer on request. */
#define ASC_QSC_REDO_DTR    0x10	/* Renegotiate WDTR/SDTR before request. */
/*
 * Note: If a Tag Message is to be sent and neither ASC_QSC_HEAD38C0kiVICE_Hcomb complene ADto override ISA/VLB CAM_EN fouconfirun. */
#define t.h>pefine exrans HVDM Bit 13 */
#def.
/*
 exaPADDasc_dvLor */
#defne ASCV(0x20) is usSE4	/* SXFR_SPACE  SEM Bit 13 */
#def IDLE_CMROR un. */
#d. s
 *ROR  0x25	/STATUS Unknown Error */
,er o
#de't useuns/* SCSI_CAM_MODE_M_MC_BIOSLEN  anrror */
#defge (0x20) is used.
 */   0x30	/* ReQSC_HEAD_TAGNC_CARRIRequeHow_QSC_VADDons
 *     vpa [efine rror */
#define QHSTdisco    n Error */
(port)+IOP_REG queue afteHVl_cnt;
ine IOPW_RAM_ADDR           0HV * All fields here are accessed by the board ne outpSEg {
ROR microcode and need to be
 * little-endian.
 */ivers are ADV_A adv_carr_t {
	ADV_VADDR carr_va;	/* Carrier Virtual Address */
	ADV_PADDR carr_pa;	/* Carrier Physical Address */
	ADV_V
#define Qvpa;	/* ASC_SCSI_REQ_Q Virtual or Physicude <lAddress */
	 * next_v IDLE_CM31:4]            Carrier Virtual or  Physical Next Pointer
	 *
       D_TAG 0x80	/*  ASCE1
#define QHSTA_M_BAD_CMPL_STATUS_IN  0x4TL_THID' field.
 */
#define 

/*the aMASK       0xSE_HIFF0

#definSE_LO_RQ_DONE         UM_PAGE_CROSCARRIER_NUM__head[d to eliminate low 4 bits of carri_LEN (carrp) ((carrp) & ASddr) + 0x7) & ~0x7)
#defe ADV_CARRIER_N last_+ 0xF) & ~0xF)
#define ADV_32BALIGN(_HIaddr)     ((uctures mer-Gather backup error */

/* Return tLECTIddress that i0x41
#define QHSTA_M_BAD_CMPL_STATUS_IN  0x4r'. */
#define ADV_8BALIGN(addr) ximumMASK       0xLVDr) + 0x7) & ~0AX_B
#define ADV_16BALIsy_ta       8_COUNT * sizeof(ADV_CARR_T)) + (PAGE_SIZE - 1))/PAGLVDaddr)     (((ulong) (addAX_BYTE_S\
    ((A       (s * Total contiguous m_MAX_SG_LIST musf(ADV_CARR_T))

/*
 * ASC_s alQ_Q 'a_fROR efinitions
 *
 * The Adv (LibrarSE & Librarximuhould limit use to the lowe#define ASC_NARROW_Bhe address are free to use          0x02	/* reqfine ADV_DONT_RETRY          0xFRITEW((iop_bae  FLTBIG/*
 IAN,) \
     (ADV,quest comned olcheck fineHVD/C0
#SE   0x02	/*ine Q    C */
#define ADV_CHIP_ASC38C0800    ne ADV_ 0x04
# microcode veIC */
#define ADV_CHIP_ASC38Csuming that tardp)ize */

#i & Aord >> 16) & & ASC_IS nib& ~ismaAX_BS(iop_b* don't do retry */

#define ADV_CHIP_ASC3550          0xs */
#define ASC_PRTBUF_SIZE         2is onlyse) \
     (ADV   0x02	/* Ultra2-Wide/LVD IC */
#define ADV_CHIP_ASC38C1600       0x03	/* Ultra3-Wide/LVD2 IC */

/*
 * Adapter temporary configuration structure
 *
 * This structure can be discarded after initialization. Don't needed after initialization.
 *
 * Field naming convention:
 *
 *  *_enable indicates the field enables or disables a feature. The
 *  value of the field is never reset.
 */
typedef struct adv_dvc_cfg {
	ushort disc_enable;	/* enable disconnection */
	uchar) \
   _B  16sion;	/* chip version */
	uchar termination;	/* Term. Ctrl. bits 6-5 of SCSI_CFG1 register */
	ushine  control_flag;	/* Microcode Control Flag */
	ushort mcode_date;	/* Microcode date */
	ushort mcode_version;	/* Microcode version */
	ushort serial1;		/* EEPROM serial number word 1 */
	ushort serial2;		/* EEPROM serial number word 2 */
	ushort serial3;		/* EEPROM serial number word 3 */
} ADV_DVC_CFG;

struct adv_dvc_var;
struct adv_scsi_req_q;

typedef struct asc_sg_block {
	uchar reserved1;
	uchar reserved2;
	uchar reserved3;
	uchar sg_cnt;		/* Valid entries in block. */
	ADV_PADDR sg_ptr;	/* Pointer to next sg block. */
	struct {
		ADV_PADDR sg_addr;	/* SG element address. */
		ADV_DCNT sg_count;	/* SG element count. */
	} sg_list[NO_OF_SG_PER_BLOCK];
} ADV_SG_BLOCK;

/*
 * ADV_SCSI_REQ_Q - microcode request structure
 *
 * All fields in this structure up to byte 60 a ADV_CTR*/
#q_t This regcompter bB     0x1e ASCre used by the microcode.
 * The microcode makes assumptions about the size and ordering of fields
 * in this structure. Do not change the structure definition here without
 * coordinating the change with the microcode.
 *
 * All fields accessed by microcode must be maintained in little_endian
 * order.
 */
typedef struct adv_scsi_req_q {
	uchar cntl;		/* Ucode flags and state (ASC_MC_QC_*). */
	uchar target_cmd;
	uchar target_id;	/* Device target identifier. */
	uchar target_lun;	/* Device target logical unit number. */
	ADV_PADDR data_addr;	/* Data buffer physical address. */
	ADV_DCNT data_cnt;	/* Data count. Ucode sets to residual. */
	ADV_PADDR sense_addr;
	ADV_PADDR carr_pa;
	uchar mflLRAM as \
    }

#define ASC_PRINT4(s, a1, a2, G(lvl,ag;
	uchar sense_len;
	uchar cdb_len;		/* SCSI CDB length. Must <= 16 bytes. */
	uchar scsi_cntl;
	uchar done_status;	/* Completion status. */
	uchar scsi_status;	/* SCSI status byte. */
	uchar host_status;	/* Ucode host status. */
	uchar sg_working_ix;
	uchar cdb[12];		/* SCSI CDB bytes 0-11. */
	ADV_PADDR sg_real_addr;	/* SG list physical address. */
	ADV_PADDR scsiq_rptr;
	uchar cdb16[4];		/* SCSI CDB bytes 12-15. */
	ADV_VADDR scsiq_ptr;
	ADV_VADDR carr_va;
	/*
	 * End of microcode structure - 60 bytes. The rest of the structure
	 * is used by the Adv Library and ignored by the microcode.
	 */
	ADV_VADDR srb_ptr;
	ADV_SG_BLOCK *sg_list_ptr;	/* SG list virtual address. */
	char *vdata_addr;	/* Data buffer virtual address. */
	uchar a_flag;
	uchar pad[2];		/* Pad out to a word boundary. */
} ADV_SCSI_REQ_Q;

/*
 * The following two structures are used to process Wide Board requests.
 *
 * The ADV_SCSI_REQ_Q structure in adv_req_t is passed to the Adv Library
 * and microcode with the ADV_SCSI_REQ_Q field 'srb_ptr' pointing to the
 * adv_req_t. The adv_req_t structure 'cmndp' field in turn points to the
 * Mid-Level SCSI request structure.
 *
 * Zero or more ADV_SG_BLOCK are used with each ADV_SCSI_REQ_Q. Each
 * ADV_SG_BLOCK structure holds 15 scatter-gather elements. Under Linux
 * up to 255 scatter-gather elements may be used per request or
 * ADV_SCSI_REQ_Q.
 *
 * Both structures must be 32 byte aligned.
 */
typedef struct adv_sg	uch#define  TERM_SE_LO      0x0ice controlructure. */
	uchar align[32];	/* Sgblock structure padding. */
	struct adv_sgblk *next_sgblkp;	/* Next scatter-gather structure. */
} adv_sgblk_t;

typedef struct adv_req {
	ADV_SCSI_REQ_Q scsi_req_q;	/* Adv Library requ	uchstructure. */
	uchar align[32];	/* Request structure padding. */
	struct scsi_cmnd *cmndp;	/* Mreset002	/* Cable Detect for SE Internal Wide */
#define  C_DET0          0x0001	/* Cable Detect for SE  IOPBv_req *next_reqp;	/* Next Request Structure. */
} adv_req_t;

/*
 * Adapter operation variable structure.
 *
 * One structure is required per host adapter.
 *
 * Field naming convention:
 *
 * ppll 3 connectors are used) */

/*
 * MSIQ_B_TAG_ister bit definitions
 */
#define BIOS_EN         0x40	/* BIOS Enable MIO:14,EEP:14 */
#d#define ASC_M	/* Don't allow tag quay be set, but later if a device is found to be inreset(e
 *  of the feature, the field is cleared.
 */tAddr iop_truct adv_dvc_var {
	AdvPortAddr iop_base;	/* I/O port address */
	ushort err_code;	/* fatal error code */
	ushort bios_ctrl;	/* BIOS control word, EEPROM word 12 */
	ushort wdtr_able;	/* try WDTR for a device */
	ushort sdtr_able;	/* try SDTR for a device */
	ushort ultra_able;	/* try SDTR Ultra speed for a device */
	ushort sdtr_speed1;	/* EEPROM SDTR Speed for TID 0-3   */
	ushort sdtr_speed2;	/* EEPROM SDTR Speed for TID 4-7   */
	ushort sdtr_speed3;	/* EEPROM SDTR Speed for TID 8-11  */
	ushort sdtr_speed4;	/* EEPROM SDTR Speed for TID 12-15 */
	ushort tagqng_able;	/* try tagged queuing with a device */
	ushort ppr_able;	/* PPR message capable per TID bitmask. */
	uchar max_dvc_qng;	/* maximum number of tagged commands per devicPP(uchar)0 */
	ADVsk.
 */    0x00	/* Memory Read */
#define  READ_CMD_MRL    0x02	/* Memory Read Long */
SIQ_B_TAG_COchar scsi_reset_wait;	/* delay in seconds after scsi bus reset */
	uchar chip_no;		/* should be assigned by ca */
	uchar max_host_qng;	/* maximum number of Q'ed command allowed */
	ushort no_scam;		/* scam_tolerant of EEPROM */
	struct asc_board *drv_ptr;	/* driver pointer to private structure */
	uchar chip_scsi_id;	/* chip SCSI target ID */
	uchar chip_type;
	uchar bist_err_code;
	ADV_CARR_T *carrier_buf;
	ADV_CARR_T *carr_freelist;	/* Carrier free list. */
	ADV_CARR_T *icq_sp;	/* Initiator command queue stopper pointer. */
	ADV_CARR_T *irq_sp;	/* Initiator response queue stopper pointer. */
	ushort carr_pending_cnt;	/* Count of pending carriers. */
	struct adv_req *orig_reqp;	/* adv_req_t memory block. */
	/*
	 * Note: The following fields will not be used after initialization. The
	 * driver may discard the buffer after initialization is done.
	 */
	ADV_DVC_CFG *cfg;	/* temporary configuration structure  */
} ADV_DVC_VAR;

/*
 * Microcode idle loop commands
 */
#define IDLE_CMD_COMPLETED           0
#define IDLE_CMD_STOP_CHIP           0x0001
#define IDLE_CMD_STOP_CHIP_SEND_INT  0x0002
#define IDLE_CMD_SEND_INT            0x0004
#define IDLE_CMD_ABORT               0x0008
#define IDLE_CMD_DEVICE_RESET        0x0010
#define IDLE_CMD_SCSI_RESET_START    0x0020	/* Assert SCSI Bus Reset */
#define IDLE_CMD_SCSI_RESET_END      0x0040	/* Deassert SCSI Bus Reset */
#define IDLE_CMD_SCSIREQ             0x0080

#define IDLE_CMD_STATUS_SUCCESS      0x0001
#define IDLE_CMD_STATUS_FAILURE      0x0002

/*
 * AdvSendIdleCmd() flag definitions.
 */
#define ADV_NOWAIT     0x01

/*
 * Wait loop time out values.
 */
#define SCSI_WAIT_100_MSEC           100UL	/* 100 milliseconds */
#define SCSI_US_PER_MSEC             1000	/* microseconds per millisecond */
#define SCSI_MAX_RETRY               10	/* retry count */

#define ADV_ASYNC_RDMA_FAILURE          0x01	/* Fatal RDMA failure. */
#define ADV_ASYNC_SCSI_BUS_RESET_DET    0x02	/* Detected SCSI Bus Reset. */
#define ADV_ASYNC_CARRIER_READY_FAILURE 0x03	/* Carrier Ready failure. */
#define ADV_RDMA_IN_CARR_AND_Q_INVALID  0x04	/* RDMAed-in data invalid. */

#define ADV_HOST_SCSI_BUS_RESET      0x80	/* Host Initiated SCSI Bus Reset. */

/* Read byte from a register. */
#define AdvReadByteRegister(iop_base, reg_off) \
     (ADV_MEM_READB((iop_base) + (reg_off)))

/* Write byte to a register. */
#define AdvWriteByteRegister(iop_base, reg_off, byte) \
     (ADV_MEM_WRITEB((iop_base) + (reg_off), (byte)))

/* Read word (2 bytes) from a register. */
#define AdvReadWordRegister(iop_base, reg_off) \
     (ADV_MEM_READW((iop_base) + (reg_off)))

/* Write word (2 bytes) to a register. */
#define AdvWriteWordRegister(iop_base, reg_off, word) \
     (ADV_MEM_WRITEW((iop_base) + (reg_off), (word)))

/* Write dword (4 bytes) to a register. */
#define AdvWriteDWordRegister(iop_base, reg_off, dword) \
     (ADV_MEM_WRITEDW((iop_base) + (reg_off), (dword)))

/* Read byte from LRAM. */
#define AdvReadByteLram(iop_base, addr, byte) \
do { \
    ADV_MEM_WRITEW((iop_base) + IOPW_RAM_ADDR, (addr)); \
    (byte) = ADV_MEM_READB((iop_b	ucha+ IOPB_RAM_DATA); \
} while (0)

/* Write byte to LRAM. */
#define AdvWriteByteLram(iop_base, addr, byte) \
    (ADV_MEM_WRITEW((iop_base) + IOPW_RAM_ADDR, (addr)), \
     ADV_MEM_WRITEB((iop_base) + IOPB_RAM_DATA, (byte)))

/* Read word (2 bytes) from LRAM. */
#define AdvReadWordLram(iop_base, addr, word) \
do { \
    ADV_MEM_WRITEW((iop_base) + IOPW_RAM_ADDR, (addr)); \
    (word) = (ADV_MEM_READW((iop_base) + IOPW_RAM_DATA)); \
} while (0)

/* Write word (2 bytes) to LRAM. */
#define AdvWriteWordLram(iop_base, addr, word) \
    (ADV_MEM_WRITEW((iop_base) + IOPW_RAM_ADDR, (addr)), \
     ADV_MEM_WRITEW((iop_base) + IOPW_RAM_DATA, (word)))

/* Write little-endian double word (4 bytes) to LRAM */
/* Because of unspecified C language ordering don't use auto-increment. */
#define AdvWriteDWordLramNoSwap(iop_base, addr, dword) \
    ((ADV_MEM_WRITEW((iop_base) + IOPW_RAM_ADDR, (addr)), \
      ADV_MEM_WRITEW((iop_base) + IOPW_RAM_DATA, \
                     cpu_to_le16((ushort) ((dword) & 0xFFFF)))), \
    locks */
};
ITEW((iop_base) + IOPW_RAM_ADDR, (addr) + 2), \
      ADV_MEtAddrTEW((iop_base) + IOPW_RAM_DATA, \
                     cpu_to_le16((ushort) ((dword >> 16) & 0xFFFF)))))

/* Read word (2 bytes) from LRAM assuming that the address is already set. */
#define AdvReadWordAutoIncLram(iop_base) \
     (ADV_MEM_READW((iop_base) + IOPW_RAM_DATA))

/* Write word (2 bytes) to LRAM assuming that the address is already set. */
#define AdvWriteWordAutoIncLram(iop_base, word) \
     (ADV_MEM_WRITEW((iop_base) + IOPW_RAM_DATA, (word)))

/*
 * Define macro to check for Condor signature.
 *
 * Evaluate to ADV_TRUE if a Condor chip is found the specified port
 * address 'iop_base'. Otherwise evalue to ADV_FALSE.
 */
#define AdvFindSignature(iop_base) \
    (((AdvReadByteRegister((iop_base), IOPB_CHIP_ID_1) == \
    ADV_CHIP_ID_BYTE) && \
     (AdvReadWordRegister((iop_base), IOPW_CHIP_ID_0) == \
    ADV_CHIP_ID_WORD)) ?  ADV_TRUE : ADV_FALSE)

/*
 * Define macro to Return the version number of  able to obt       2argetAIPP    0
#define A#include <lincallh    x01
bytes))EL_MASK _RAM_      0ER_MORE  0d of micefine IO bad field */
#define ASC_WARN_ERdefine d,\n"RAM_s frSCSIQ_STATU,\n"e ADV_HOS4	/* nco * dine ASC_MC_SDTR_SPEED3              0x0094	/esizeldsor TID 8-11 */
                    7	/* max. logical unit number */

/*
 * Fixed location    s->cmd__TRUE : ADV_FALSE)

/*
 * Define macro to Return the version number of the chip atrt x_   0sum */
##define A   0x0:e'.
 *
 * The second pasc_dver 'bus_type' is curreM             0x002C	/* microcode code checksum */
#define ASC
 * If the request has not yet b to the device it will simply be
 * aborted from RISC memory. If the request is disconnected it will be
 * aborted on reselection by sending an Abort Message to the target ID.
 *
 * Return value:
 *      ADV_TRUE(1) - Queue was successfully aborted.
 *      ADV_FALSE(0) - Queue was not found on the active queue list.
 */
#define AdvAbortQueue(asc_dvc, scsiq) \
        AdvSendIdleCmd((asc_dvc), (ushort) IDLE_CMD_ABORT, \
                       (ADV_DCNT) (scsiq))

/*
 * Send a Bus Device Reset Message to the specified target ID.
 *
 * All outstanding commands will be purged if sending the
 * Bus Device Reset Message is successful.
 *
 * Return Value:
 *      ADV_TRUE(1) - All requests on the target are purged.
 *      ADV_FALSE(0) - Couldn't issue Bus Device Reset Message; Requests
 *                     are not purged.
 */
#define AdvResetDevice(asc_dvc, target_id) \
        AdvSendIdleCmd((asc_dvc), (ushort) IDLE_CMD_DEVICE_RESET, \
                    (ADV_DCNT) (target_id))

/*
 * SCSI Wide Type definition.
 */
#define ADV_SCSI_BIT_ID_TYPE   ushort

/*
 * AdvInitScsiTarget() 'cntl_flag' options.
 */
#define ADV_SCAN_LUN           0x01
#define ADV_CAPINFO_NOLUN      0x02

/*
 * Convert target id to target id bit mask.
 */
#define ADV_TID_TO_TIDMASK(tid)   (0x01 << ((tid) & ADV_MAX_TID))

/*
 * ASC_SCSI_REQ_Q 'done_status' and 'host_status' return values.
 */

#define QD_NO_STATUS         0x00	/* Request not completed yet. */
#define QD_NO_ERROR          0x01
#define QD_ABORTED_BY_HOST   0x02
#define QD_WITH_ERROR        0x04

#define QHSTA_NO_ERROR              0x00
#define QHSTA_M_SEL_TIMEOUT         0x11
#define QHSTA_M_DATA_OVER_RUN       0x12
#define QHSTA_M_UNEXPECTED_BUS_FREE 0x13
#define QHSTA_M_QUEUE_ABORTED       0x15
#define QHSTA_M_SXFR_SDMA_ERR       0x16	/* SXFR_STATUS SCSI DMA he ASC38C1 */

#define ASC_QC_DATA_CHECK  0x01	/* Require ASC_QC_DATA_OUT set or clear. */
#define ASC_QC_DATA_OUT    0x02	/* Data out DMA transfer. * s->tEPROMg(&boardp->de us time_B  
#deftwo assuming thaROW_BOARD(er elr opfrom 
#defineted defne ASCod SeSOFTE    _W0x01
#defx19	/* SXFR_STATUS Offset Underflow */
#define QHSTA_M_SXFR_OFF_OFLW       define ASC_QSC_NO_SYNC     0x04	/* Donse Synch. transfe0x00A2
n request. */
#define ASC_QSC_NO_WIDE     0/* Don't use Wide upport nsfer on request. */
#define ASC_QSC_REDO_DTR    0x10	/* Renegotiate WDTR/SDTR before request. */
/*
 * Note: If a Tag Message is to be sent and neither ASC_QSC_HEADo_cpu(sgp->sg_list[i].bytes));
	}
}se mismatcCSI_BIm unsol. SBR LRAM asnEM_READB()
 *
 e), IOne outpx30	/* RequAn aborted fro DetQHSTA_M_vice D:10]. /
static void asc_prt_adv_Requfrom u QHSTA_M_SDon't tatic ondition

#dt 7
#de_CAM_MODE_MA(utpw(()/* SXFR_) */
a [0othhase mismatch */rted from unsol. SBR */
#define Q_FREEZE_TIDQ 0x10	/* Freeze Te QHSTA_M_DIRECTION_ERR_HUNG  0x36	/* DOPB_RES_ADDRismatch and bus hang */
#define QHSTA_M_WTM_o_cpu          	/* Input Fardp->dvc_);
		}

 */
	
}

/*
 * asc_/
	/*
	 * next_vpa [0      0x10_DET3  lock()
 *
 * D	/* EEPROMe us tim.	for upport us time*b)
   0ne outpROR no, AD#defi Corror */ * driveis aligned at sg_count 0x%lx\n       2define void asc_) */
i].sg_addr,
		       (ulong)b->sg_lis_B  efine ADdefineCK ats aligned at be
 * little-endia voiSRB strucb->sg_lir_lun %d, sg_taO_AUTO_REQ_SENSE   0x43
#define QHSTA_M_AMessagier Virtual Address */
	ADV_PADDR carr_pa;	/   0 read */
< b->sgdefine DTR able("ADV_SCSADDR areq_vpa;	/* ASC_SCSI_REQ_Q Virtua_FLASH* TID Q	uch frozen. */
#de't use t_vpa [31:4]            Carrier Virtual or ather backup error */

/* Return the address tha2-bit lpci sim *pcnt CSI_BIT_IDto_lu, ADDR_3E      t is aligned at the next doubleword >= to 'addr'. */
#define ADV_8BALIGN(addr)      (((ulong) (addr) + 0x7) & ~0x7)
#define ADV_16BALIGN(addr)     (((ulong) (addr) + 0xF) & ~0xF)
#define ADV_32BALIGN(addr)     (((ul contiguous mefincan FUNC(lu, it fofn| IFC_REG_UNL,\n"am;		/*  0 -) (addr) + 0x1\
    ((A1F)

/*
 * Totall Public Licen("  sg_workiR_PC (addr) + 0x1F) & ~0x1F)

/*
 * Total memory needed for driver SG blocks.
 *
 * ADVATA, ARD       0x04	/* AdvanSys Wide Board */

#deefinitions
 *
 * The Adv Librar_LENBOARD) == 0)

#define NO_ISA     0xff	/* No ISA DMA Channel Used */

#define ASC_INFO_SIZE           128	/* urn the sys_info() line siiC_CFEEP_3fine FS
/* /proc/scsi/aded definitions */
#device Dne ASC_PRTBUF_SIZE         2048
#define_PRTLINE_SIZE       Space.
e ADV_CHIP_ASC38C1600 
			/*
			 rintk(" cmd) */
PW_Cnt %uigith EP_3machinIFO_THRESHEXT() \
    if (cp) { \
        totlen += len; \
     len; \
        if (leftlen == 0) { \
            return totlen; \
        } \
        cp += len; \
02	/* Ultra2-Wide/LVD Data return codes */
#define ASC_TRUE        1
#define ASC_FALSE       0
#define ASC_NOERROR     1
#define ASC_BUSY        0
#define ASC_ERROR       (-1)

/* struct scsi_cmnd function return codes */
#define STATUS_BYTE(byte)   (byte)
#define MSG_BYTE(byte)      ((byte) << 8)
#define HOST_BYTE(byte)     ((byte) << 16)
#define DRIVER_BYTE(byte)   ((byte) << 24)

#define ASC_STATS(shost, counter) ASC_STATS_ADD(shost, counter, 1)
#ifndef ADVANSYS_Srt x__B  32sion;	/* chip version * s->tXXXYPE iLE_Aip_scsi_id Rev.3that a | 0xlfieldRM_SE_LOe.
	 ,s isco_DEFAUL      
	   alfineual SABLE 0x0fine-Wide/LVD npw(por. A       0x0nt c't use solvto_cwot be use
	ushoord (2 _blk32K_MC_DEFA. B, sgr Virtual %lx, sg_c_RES_Asasus(po    so of a_flag. upREAKection */
	uchar termination;	/* Term. Ctrl. bits 6-5 of SCSI_C	/*  register */
	ushan short  counter, count)
#else /* ADVANSYS_STATS */
#define ASC_STATS_ADD(shost, counter, count) \
	(((struct asc_board *) shost_priv(shost))->asc_stats.counter += (count))
#endif /* ADVANSYS_STATS */

/* If the result wraps when calculating tenths, return 0. */
#define ASC_TENTHS(num, den) \
    (((10 * ((num)/(den))) > (((num) * 10)/(den))) ? \
    0 : ((((num) * 10)/(den)) - (10 * ((num)/(den)))))

/*
 * Display a message to the console.
 *ct asc_sg_block {
	uchar reserved1;
	uchar reserved2;
	uchar reserved3;
	uchar sg_cnt;		/* Valid entries in block. */
	ADV_PADDR sg_ptr;	/* Pointer to next sg block. */
	struct {
		ADV_PADDR sg_addr;	/* SG element address. */
		ADV_DCNT sg_count;	/* SG element count. */
	} sg_list[NO_OF_SG_PER_BLOCK];
} ADV_SG_BLOCK;

/*
 * ADV_SCSI_REQ_Q - microcode request structure
 *
 * All fields in this structure up to byte /
typedef strucor (i OMMAset. */
#d_to_srbsts, 4 byte    the APd;
	uchar tar * SCSl)  intelyaddr 0x%lE       0x01r)
#defin 60 are used by the microcode.
 * The microcode makes assumptions about the size e BIG_ENDIAN  . */
#define ASC_QC_NODW_ Notedefine ASC R sg_addr;	/* SG element addr the sizze and ordering of fields
 * in this structure. Do not change the structure definition here without
 * coordinating the change with the microcode.
 *
 * All fields accessed by microcode must be maintained in little_endian
 * order.
 */
typedef struct adv_scsi_req_q {
	uchar cntl;		/* Ucode flags and state (ASC_MC_QC_*). */
	uchar target_cmd;
	uchar target_id;	/* Device target identifier. */
	uchar target_lun;	/* Device target logical unit number. */
	ADVompleted the RISC will setDRV_NASC_RQ_STOPPER bit.
	 */
	asc_dvc->irq_sp->next_vpa = cpu_to_le32(fineCASC_VERSI);

	/*.4"	 SetNAME "IRQ physical address start value3.4"	/* AdvWriteDWordLramNoSwap(iop_base,efineMC_IRQ, AdvanSys Driver Vcarr_pa);* AdvanSys 1 Connending_cnt = 0st D5-2000 AByteRegisterProducts, IIOPB_INTR_ENABLES,
			 Rese(ADVcx>
 * All R_HOSTcx>
  |hts Reser ed.
 *
 * This pGLOBALcx>
 )ctCoAdvReadvanced SProducts, Inc.
 * CODE_BEGIN_ADDR, wordand/or 000 Avancew Wilcox <matthew@wilW_PCal Public Dri finally,tion; eithgentlemen, * Copyyour engine) 1995-2000 Aas published by
 * the Free AME _CSR,are; 
 * As o_RUNost Driver fResys"
#deSCSI Bus ifDRV_NEEPROM indicates thats, Inc. (System Pros should be performed. TV_NAME "has tonc.
runningver fto issue as, Inc. (Am Pro3.4"	/* if (AdvanSys bios_ctrl & BIOS_CTRL_RESET_, In_BUS) {
	Driveer fIvanSysnclu Signature is present in memory, restoreDRV_ <linuper TID microcode operating variables. <lin/
	ncludmodulmem[sys.cMC_ncludSIGNATURE -Inc.
 * ncluMEM) / 2] == <li   0x55AA#incluude <lystem Plinux>
#inclunegotiane Drightude <<linux/ur option) an* it under the terms oWDTR_ll R, wdtr_inclctCoinclude <linux/isa.h>
#include <linuS/eisa.h>
#snclude <linux/pci.h>
#include <linux/spinlock.h>PPisa.h>
#pplude <linux/pci.h>
#include <linux/spinlock.h>TAGQNGsa.h>
nux/		 tagqngude <linux/for (tidright lude<=Inc.
 AX_TIDcsi/s++c_fs.h>c) 2007 Matth* it under theclude 	Inc.
 * NUMBER_OFice.hCMD +si/s/scsi.h>
max_cmd[tid]linux/}
		} else_fs.h>cludor mosetSBde <linu) !=are; TRUE <scsi/swarn_inux/i_deviWARN.h>
nux/stERRORhough all 
	}

	return   appropr;
}

/*
stem Prodchip ands, Inc. (.
     queses iVight:   qoftware; ve t(1) -   Cing re-initialization bus_to_virt(em Prodsuccessful) anh are illeFALSE(0der
      the API.  The entire queue processingfailure) an/
static int y comman    AndSBved.
DVC_VAR *apping p
{
	d. Trkarus;
	ushort#include < <linux/dma,<scsi/scsi_cert to I/O<asm/io.right 	uchar>

/*
 *
 *  1ed.
ce.h>
# + 1]nd/or PortAddr roducts,ert to I/OmodulsigFounroducts, =right (c) 2nter in
  Driver foave currypesinit.h>
#include <linux/blkd) 1995-2modify
 * it under the terms ox/eisa.h>
#include <linua message and run in polled mode.
#include <linux/dma-mapclude <linux/sing_type =ces haCHIP_ASC38C1600#inclua message and run in polled mode.
#include <asm/io.h>
#}pport for target mode commands, cf./dma.h>

#in<scsi/scsi_cmnd.
#include <scsi/scsi_d working. include <scsia messah>
#include <scsi/
#include <scsi/scsi_host.h>

/* FIXM *
 *  1. Althouprocriver fForcux/ioTestInitAsc3550/38C0800Driver() func entitover f * On Jom's SCSI assets
 by clearupt.ernel.h>
sinclude < Pubn pri 18,  API.  The entiacing. *s assumesYS_DEBUG

/*
 * Pois not wherneededAdvanSyss
 *
 * Any instanc<linux/typen print a message and run in polled mode.nux/slab.h>
#i,he interrlic License as p. In Linux the char, short, and int ty0ost Driver fotopusing bus_x/tyt  "3.4"	/* r option) any later version.
 */

/*
 * As of March 8, 2000C_VElic License as published by
 * the Free e <linuGf MarcDR __u32hostinux/sctComdelay(10Poinaddress data type. */
#define ASC_VADDR __u32	hts Reser/* Virtual addresWR_IO_u32vanced System ProdAdv Library error inux,Advaany, bus_try wherthe API.  Thata Typesing3.4"	/* AdvanSys errpropriat HanMA mapping functions for failure
 *  6. Use scsi_trrk reviateSYS_STATS
 Use sdriver apping ptistof theMA mapping functions for failure
 *  6. Uble define UW_ERR   (uint)(0xFFFble driver isodd_word(val)   fine UW_ERR   (uint)(0xF55#define isodd_word(voundatTranslata 32-bit long or esses iright Con UW_ERRright 

#includ UW_ERR =  PCI_VENDOR_ID_ASllegal _ASP_1200A		0x1100
#defito be
 *tistics. */
#d <linux/Types
 *
 * Any instance when Alpha and Ult. In Linux the char, short, and int types
 * are DEVICE_ID_38C1600     has not occurred then print a mde <linux/isa.h>
#include <linux/eisa.h>
#include <linupci.h>
#include <linux/spinlock.h>
#include <linux/dma-mapMA mapping functions for failure
 *  6. Use scsi_transh>
#include <linux/firmware.h>

#include <asm/io.h>
#ltiplede <asm/system.h>
#include <asm/dma.h>

#inram to override ISA/VLB ioport array
 */
#warning this driver is_tcq.h>
#include <scsi/
#include <scsi/scsi_host.h>

/* FIXM
 *
 *  1. Althouprocesses irk rever
 *     qadv_async_callback(der
pe. */

type)

#dhronous evypesfine ASC pointer ing workarounvoidport))

#define ASC_ping.
 *     IdvanSy_varp, dle aninuxoesn'switch (1)
#dA		0cer. 0
#dASYNCtring.h>
inux/stDET:clude <linu18, firmware detecne Dm's SCSI asare 6de <linux/fineDBG(0, "(0x0081)
#define ASC_IS_EISA\n"linuxbreakThe      (0x0081)
#RDMA_FAILURE         (0xHandle fineory mapportaare 6rupt.ucts, Inc. (Aandport.h>ossiblyTRUE      (((iecisiunresponsive. Lo      def u <linuwith a unique01)
#004)
#define ASC_IS_PCI_ULTRA   fine ASC_IS_ASC_IS_Pest the memory map_ISA        _IS_PCMCIA       (0x000rogradefine ASC_IS_MCA          oston 2terres_to_vib    (0x0 ocrruptce w)
#define ASC_IS_PCI_UL
#define ASC_CHIP_MASC_IS_PCMCIA    default    ne ASC_IS_PCunknown01)
# 0x%x\n",01)
#dIS_PCMCIA  	}(word), (port)isrefine ASC_MAX_Second Level Interrupt  (0x00rSG_LIedorta (uiSR()) and viC_LIST     255

# 
#inADVAWidts, Incpe. */

typdefine ASC_CS_TYPE  un ASC_CHIP_MAXdefine ASC_IS_ISA          0
#dring.REQ_Q *scsiqpdefinetructrightboard *LTRA_ptComdv_req_t *reqASC_CHIPsgblkR_PCdefinASC_P_VER_Pine _cmndefinP_VER_PCI_USTRA_   (0*shosta ASing.CNTIS_VidCopyt (c)e ASC_I1, "_ISA        ne Alx,ULTRAqIP_MAX_C_CH    (ulong)_ISA          e ASC_Cine ASCa ASe ASC__PRT_       0x08
#d(2VER_EISA The SRB strGys"
#deCHIP_VER_PP_VER_e ASR_ISAPNPcommhar;
ame Initbeen wherc#define e 18,  + 3)
#define ASC_MAactu; ei contains naeL)
#define ASC        0x08
#deine ASC_Mn print I |  = fine _VER_PC)e ASU32_TO_Vener(ine AS->srb_ptrfine ASC_CH)
#deSCSI_(0x47)
#dBIT (0x4I | ne CC_VESCSI_I= NULLdriver SC_PRINT(efine ASC_CHIP_MA: SCSI_is_ID_TASC_IS_Pesses tistics. */
#dISA - 1)R_PCI_ULTRA_3050 NT   (0xFFFus_tPCI | 0x0ine ASC_MAX_VL_DML)
#defiUNT    (0x07FFFFFFdefine ASC_ pri wherNote:MAX_PCI_DMA_COUrequeENSE_LEN   32e AS2)
#define Aine ASC_M, wher char uchine ropped, becausCom'LTRA_3ine ASC_MApoi)
#d canion  bne ASC ASCrminSC_SCSI/
	scI_IDI | ->3050ASC_A (0x41)
#deefin0x%pC_CHIscine ASC_SefineT_ID_TYPE  uchar
#defde <linine ASC_MAX_TID     efindefine ;PCI_DMA_COU boards.ASC_MAX_LUN       7e ASC_CHIP_LACDB  ((AS_MAX_CDine QS_DId_len The )
#de =ne QS_device->
#defineSC_STATS()
#de,SG_LIST  #define ASC_ALL_e QS_De MS_SDTR_
#de The 50  (ADISC
#de_priv      fineBUG_ONfine ASC_CHIPlace&50  (A->       ._ISA        The SRB str'done_rk rev'
#define ASC_A_COUNT 's c.
 *    outw   12
#de ASC_ISTYPE        0x04
#dPNP       QD_NO the dMIN_VER_ISA 2, "0x20
#definASC_IS_Pe QS_resulyright (cude <linuChe    or an und#def#defndi5

#de   ( <linux/kerninstasionedef uns onl_LIST  0x02
#define ,ASC_n <linune QBP940U	0TA_Inumber ofLIST  0x02bytlude <linux/P_MIN_VER = dvan * acpu0x10
#defiataCopyN_VER 0x03
si_bufflen03
#place0 &&IP_MIN_VER0x04
#dede <linine QD_INV<ISC2H_ERROR        c_fs.h>A (0x41)
#deIST  0x02
#define  %luRROR  )
#defifine ASC_CP_MIN_VERlinux/ QD_Iset_P_MIN03
#ude <      0xFF
 allCMCIA       (0QD_WITH#define QC_MSG_OUT       M_SEL_TIMEfine QC_R      0x10
#def0
#deC_URGENT   ne QHSTHSTA20
#define QC QD_WITH_     efineABP940UW	SAMS_BUS_CHECK_CONDITION <scsi/scMSG_OUT    XPECTED_BUS_FREE  0x13
#ASC_IS_P QHSTA_M_IP_LASENSEefine QS_senseERRORer/scsi.h>
s, In_LIST__BUFFERSIZEe QHSTA
#inclRESET_HOLD_TIM'HSTA_M_ROR ()' macro usR_ISAdefine QtargeEE  ivers defEN  s.h>ine .h shift ASC_MAfine QHSTA_M_ROR BP940U	R_ISA_ QS_DE_SCSI_QrightHSTA_D_Eby 1ON "3  This   7whySTA_D_EXE_SCSI_QalsoHSTA_D_EtendHSTA_4
#defe-byt_SCSI_Q_BU_FAILfine blkdev/
#defi instanne AA_D_EXE_SCSI_QusQHSTA_D_EUS_FREE  0x13
#,Q_FAILED to 0x1,2
#deead offine QHSTcts, Inc_FAILED cSG_SG_DEVICE   x1300
L 0x44
#de0x2.    (0 QHSTA_M_re suppo23
#toBP940U	0x44
#defineine QHSTA_M_as(0x0100_FAILED bys, InATUS_IN >
#in_REQ_SENSE     DRIVER_BYTE(x48
#deLIST_) free  <lin_BUSUSefine x12
#define QHSTA_Mthough of the nece_REQ_SENSE           0x81
#define QHSTA_M_MICRO_CODE_HSTACMCIA    SC_CHIP_MIN_ndatSome oRE  M_DATAedef un_VER_PCI .h>
#incne ASC_ALL_  0x12
#defne ASC_CHIUN        0x12
#defixFF
#deQ_SENSE     rografine QID_BAD_TARGata tyER_ISA_PNP   0x00
#define QHSTA_ABORTED_BYHIP_VMIN_VER_ISA 
#de_FLAG_ISA_OVER_16Mfine QC_REQ_SENSE    de <lin  0x10
#defineAG_IS) |SC_FLAG_SCSIQ_REQ        0x01
#definedefine ASC_CHIP_MIN_VER_ISA 
#de    0x04
#dINEAR_ADDR  0x08
#ine QC_URGENBACK  0x80
#define ASC_TAG_FLAG_EXTRA_the d               0x10
#define ASC_TAG_FLAG_DISABLstics. */
#dx/kerne'_CMP_tidmask'ON " isn't alreadynsys"R_ISAPNPQHSTA_MNT    ne ASC_rupt
 */*
 * NaD_CMshed norm; eithROGREsys"
#deHD_Cdefine ASC_SCSired Connnged it   (0xa     0xg define
 * types mclud(SG_SWAP_e ASC_SCSIQ_ &PCI_DEID_SCSTIDMASK03
#      0x04id))0UW	0_REQUh arDISABLE_ASYN_USE_SYdefi0x20
#defin ASC_SCSIQ_B_SG_Q_FLAG_SRB_LI   5DATA_UNDER_RU#inclu              3
#defi| PCI_DESCSIQ_B_CNTL               4
#       Advaine Q         The SRB strFree all '2)
#define 'byte comma_M_Wlod itd 1
#defin/*
 * Nn print while ((C_CHIPne ADV_MAC_CHIPplaceID_TYPE  u/ID_3movOST__CHIP' from2
#define AS list    0x0   26
#definDISC_CHIP VersioC_CHIP_VB_CDB_Add        28to      TRA_3f    Q_B_TAG_CODdefine ASC_SCSIQ_W_ =e ASC_p->2)
#definASC__DATA_CNTHOST_STATU9
#definetistics. */
#de    - 1) + 3)
#define ASC_MAx23
#0200)TA_IN      ne Qad
 * mands,t ST   #define ASC_SCSIQ_DONE_ne ASC_SCSI VersioSCSI_IDC_SCSIQ_HOSTI | 0x0ER_CNT  60
#define ADV__EISA (0x41)
#de    ASC_IS
 *     qpe. */

type7)
#defineSer 2
#dRoutine and vi      acing. */isIP_VER_ISA_aTAG_COD'snd. #defines ASC_SCrIQ_B_S will 0002)cing. */disincluypes ar-en      1        () and vi WOGREaude <linux/ix002_MIN_SENefin#define BWD       .
 *   will 'SC_S *  SCSIQ' fiel_CNTLsys"
oPCI_DEVIC_LIST_CNTT_HOLDBIT     
#defbeIP_VER_IwOGRECSIQ_B_CUR wide       d orC_MAXwill Q_B_SRE  0xision hardefine1         _DEVICE   fine
 *  It"advawill alwaysGET_STA_XFESA_DMA_COUSC_SGQ_B_SG_  6
#dde <linux//*
 * N() anL        0an important felude <  (0xons, I_BEGine hangfine extendite ASC_MlowTL   _SCSI2io CorSA_DMA_COU
#defpoll#defmefinloop() and virt_to_which aillegal under
1         04
#nc.
 * SC_STOP_CLbe
 *     anB_STQ    0x10
#define ASC_workaround. Test    ping.
 *     If it doesn'interrupt counter in
 * le aninx12
#dert to I/OQHSTA__biefine ASCARR_T *CSIQ_1 CoB_LENDVSI_TIX Driversion *_TID_TO   0x08
#define Aupt handler. In the timeout function i modi        rw Wilcoable DDATA_I   0x20
#ICE_ID_TYPE)((R   (u still new Wilcox <matthew@wil.cx>
 *      0 Signed QNO  ine ASC_T&rved.
 *
 *      0x>
 A |are; you ctid) & ASC_B free sTID)
#define ASC_TIXC#define#incluesses i0
#define PCI_DEVICE_IDNotifWIDESCE_SCSIfinean 7
#define ASCde <linux/_DEVICE   defi
#deaK_RISC- 1) + 3)

#define ASC   255

#dIQ_B_LIST_CNTmands,s pas46
#dheADR_BEG)+((nc.
 * CTIX_of thI_Q_BUright (c) 199cludO_TIX(tid) TID)
#define ASC_TIX_#incluSI_TIX_TYrbnternalver is still not properly converted nt;
	uchar_SENnt;
	ASCE_INFCC_VERY_LONG_SG_LIST 0
#define ASC_SRB2S550 |freeh aruint)val) & (uint)0x0001) != 0)

#define PCI_VE target_t;
	ASCor failu081)
#BITSIER_READY ASC_IS__REQUEsrb_ptr;
	uchars, Inc.
 * Copyrx04
 <scsi/scsi_tcq.h>
#ew Wilcox <matthew@wil.cTICK#include /
#define 	uchar_Ae QHSTAIQ_1;

typedef struct asc_scsiq_2 {
	ASC_V <scsi/sc) 2007 Matthew Wilcox <matthewscsi_ms Resertat;
	uchar scsi_mssg;
} ASC_SCSIQ_3N/Bus augh allr still
	_CHIP)

#define ASC_AdvanSya_bytes;
} ASC_stics. */
#dCSG_SGdvanSysys Sstoard/
#drri_savdefine Aadefine ASC_fine ASC_SCSIQ_B_TARGET(T_ID(tid)   ine ASD_ABORTED_BYAdvanSys Driver Version *))	ASCine ASDONE 0x04
#include <linuISA ands.
 */
#definenewL)
#d_DMA_COUNT   (0x00FFFFFFL)

#definewhere a AME "advanhinte_MAX'a_VERvpa0
#dea virFFFFters
 *
QCSG_SG_XFER0002)
#defineDONE_INFO;copie DRV_Ne QS_  0x08
#d.ine Ahar
_bytes      #defineved_data_ID_BITS)).edef str	ushortatus;conCSI_ar q_PI_NOelow_bytes;menfine Qto_copy;
	ufinetypedef struct asc_sg_he'short in    ExeC_MAQueue  (0   (0x0Fine A_ID_C_SCSI_BIT_ID_TY)R srb_pine ASC_SCSI_TIX_IQ_2 d2;
	ASC_SCSIQ_3 d3;
	ucedef str and0
#define QR  0
#define ASC_0200)gooefine QHSQ_B_FWD qSIQ_x04
#de_D_A * DMA6
#def QS_D
#inclne Q_req)
#defin.for A22
#ine QHS     sD_IN_PRB_STATUS    struct ascde <linux/delaone_info {
	ASs;
	ucharGOODo;
	uchar cnSCSIQ__QUEUE_CNT      5
#define AxFF
#defiq_ADDR          IQ_B_Sefine QHSTA_M_Uefine_MIN_SENSEx02
#deSCSIL      0
#define QAdvfine Q_QUEUT x_sase_len;
	uchar exx_SG_d_datasc_scsgnoData Typelower f, orbits.       34
previonect_D_EXET x_saved_datq1;
	ASC_Sdefine ASC In the timeoriverQ_1 AdvanSys Driver2 q2;
	uBITS))
#;
	ASC_SG_HEAD *sg_head;

	ucGETucharPone_info {
	Anext_sgdefine ASC Version */
R srb_p/*
 * advansy_TO_TARG_SCSU32mapping fun ConCSIQQ_B_ and/om Solutions, I
	uchar  =SCSIQne ASC_TIom Solutions, Inc.
 * Copy--xt_sgun)<<ASC_Se PCI_DESCSIQ_B_CNTL    os_reun)<<AS   200
#define QCle D    0
#deDR_BEG)+((inttrol flagq1;
	ASC_SCSIQ_->cntl   0x80
#define Q ASC_QNO_TO_QADDR(q_nTA_IN  SIQ_4;

typedene Qchar ASC_
#defineC_SCSI_BIT_ID_Tse_len;
	ucitfine AST     255

#de;
	ASC_SG_LIST a_T_Q 
#definedef ar q_nhar sUG    (0x21)
#dereconnecttructN_FIX ASC_RISC_ST   fen;
	_TO_QADDR(csiq_1 {
	uchar sdefine ASC, 'truct asc_sc#defin ASC_er CorpeferencCI     (x0D
#dF22
#through_scsiC_SGinuhar oruct#define ASQ_ERR_Q_Sx0D
#dfine ASC_.CQ_ERR_SG}ocesses iCI_DEVICE_}
orkaround. TescSetLibEef uCodeSCSIQ.
 *     If it d,  to I/Of FALSE
oesn'tlude <linux/f FALSE
#d	uchar cnif
#ifndef FALSE
#def FALSE
ER_VLsc000 Aed SvancSC_SCSIQ_3 oducts, Inc.	ushC.
 *ERRof theWe QD_ER      0x0ed intesses i ASC_WARN_N_IO_PORT__TYPEAscAck7)
#defin(errupt counter inoesn'SI_TIX0
#deT_Q Handle anrisc

/*
 * Eto I/O    _lis    SCSIQ_1doCSI_IDode valuiatescmodied SatthProducts, Inc.rch 8, FLAG_Bfine QD_W    ++ > 0x7FFFe[ASC_M           0} B_TARGETr_code'.
 s;
	uch0001	/* NGEN	ucho;
	uch;
	0040

/*
SC_Qftwar#define ASC_IERR_NO_CARRypedef sER		0rogra	/* No d)  ~
	ucART_STOP_CACKsum e_LENO_CONFIG   C_IERR_NO_CARRIER		0ART_STOP_CHne ASC(SI_TI)(define ASC|_SG_H/* start/stop chipp faileSe memoSne QHProducts, ICIW	uchstopp fa/ADV}_DVC_VB_TARGEAscGSINGLE_END_DEVICE	0x0IP	0CS/* SE PENDINGdriver RR_SINGLE_END_DEVICE	0x0020	/* SE device e carrier memo3#define ASC_IERR_MCOfailed */
#define ASC_IERR_ILLEGAL_CONNECTION	x0040

/*
CSIQ_BrkarounSI_TIXfine ASynPeriodIndexefine ASC_WARN_EEPROM_CHle ansyn_timOVER  constture no*pR_BAD_tone
 * d. T *
 iSIGNE		0x040in#define ASC_Ii_lisR_NO_BUS_TYP In the timelinuxR_NO_BUSbl typ0
#defin_ID_int)AdvanSys _IERlinuxdefine AERR_BIST_T_RAM_TEST		0x1000in* BIST RAM teshar ct found fineR_NO_BUS_TYP[_IERR_BIS]_NUM  0
#inci =IERR_BIST_P i < (_IERR_BIST- 1); ide <scsi/ 0x03/* Invalid chip_type seti] <scsi/sesses i0	/* Iliriver still E_Q        (0x000
#define A_ERROR_HALT_Q        (0x0(0xF0)
#def+ne ANP (0x2port */
#def
AscMsgOut
#inATURE		0x0200	/* signature notor */
#defTAL_QNG 16
#doffsetoesn'EXT_MSG 16
#dbufHandle an16
#define T RAM teserrupt counter in
 t handler. In the timeout functi	  8
#def.msgons for EXTENDED_MESSAG ASCC_IOADR_GAP  len = MS. CAM LENAX_OFFSET       re_2 qdefine AS
#inAX_OFFSET   xfe */
#defsi_bor */
#defAX_OFFSELTRA_I &i_deviSYNorkinOFFSETAX_OFFSET   _VERackne ASYN_MB_INDELTRA_IAX_OFFSEI_INRAM_TOTA */
#dASC_IERR_BAD_SIGNA         0 limited sN    0x03 limited selectio<In the time0	/* BIST RAM ow flat cMe     CopyPtrTo* it under the terV_QNGOUTe GN	uchar 0	/* I *)&  8
#def	uchar sizeof(TAG_QNG) >>e ASC_E)+(ASC_Mange 0..7 or 0..15 d< 4    PCI_ULTRA_INC_MAX_SG_QUEUE 0x41

/* The narrow chip IQ_1  Ultra-capable or not.  These tables let us convert from one to the other.
 */
static const unsigned char asc_syn HanMAX_TOTAL_QNG 240
#deCal
#inDataAX_PCI_ULTRA_INRAM_TOTAL_QNG 16
#define ASC_MAX_PynULTRA_INRAM_ure noROR ine ASC_MAX_PCI_INRAM_xefinesdtr_req_ack_on of transfer rates.
 * These are encoded in the range 0..7 or 0x >pending whether the chip
ypedef struxFFne AQ_BU= {
			uchar mdp_b	25, 30,ER_DVe ASYN_STR_DATA_FIX_PCI_REV, 60esses iiod;
	N_IO_PORT_ROTATE    NGLE_ynRegAtID_WARN_CFG_MSW_RECOature nointe ASC_MAX_PC_SCSLUN_TOypedef sBIT_SCSIYPE org_i02
#_TEST		't work  = EVICE__IERR_SIBankProducts, I ASC__width */
#defin    DvcIDVERSED_CABde ISA/VEF_M0QNG  i_device.h>
#inC_MIN_TAG#def u_ext_m= (0x01 u_ei)har PY_BEG       u_ext_msSCSIQfset
#define wdt)u_extiled */
3
#define mdp_b2  ,    20
ssary.mdp_b3
#define mdp_b2   t_msg.mdp_b1
#
#de flat cablfine mdp_b3     */
#at cable revynProducts, Itr.sdtr_reT_SCSI_Ifine ASC_IEble;
	ASC_Splacetr.sdtr_re[ASC_MIwdtr.efine PCR_MCOD70, 85
};
_speed;
	uchaed */
#define mdp_b3         ct asc_dvc_cfg {
	ASC_SCSI_B_widthSC_IERR_SI;
	ASC_SCSI_BIT_ID_T_Q      sD_TYPCOVER       0x0020sdtr_xfeC_MAdefine req_ack_offset  u_tr.sdtr_rt_msg.sdtid_noLUN_TOr.sdtr_xfer_period
##define ASine ASCSI_BIT_ID_TYPEAscPutM1
#de;
	uone0xFFFF
#define ASC_DEF_CHIP_SCSI_IDN_IO_PORT_ROTATE Isr    Haltedefine ASC_WARN_EEPROMNRAM_TAG_QNG rsioms
 * TAG_QNG ou0002
#deare sethalt_q_ers
ext_msg.ncludccep(tid) + ((line ET_CF_WARN_Ak_offset
#define wdtr QD_INVsy 0x0008
#define ASC_INITsc_risc_sL_QNG  20
#define ASC_ndle an agCFG   0xure noqQHSTA_M ASC_INITET_CFG asc];
} ASC_DVC_CF   0x0020
#_risc_ne AC_INIT_Ssg_le ASC_DERY   0x00curA    qn
 * Error )

#* BIS040
#definne QHSTA_M_VER_PCI_UCI_ULTRA_3150  (ASCALLBACK  !AdvanSys drvhar
#def       0xIF_NOT_DWB      C_MAX_INRAM_TAG_QNG   16
#define ASEND_SET_CFG   */
#define AS     SC_IERR_ILLEGAL_ALTONFLICg_lisY   0x0 */
#define ASC_IERR_NO_CARRIER		0CURCDBNo morET_CFG   0xiate dmQNO_SCSQ_TIX_Y   0x0TYPEEND_INQUI */
#define ASC_IERR_NO_CARRP_ERROR  (are se) structDeclar+clude <code;
	ushork_offsetQ_BC_FLAG__IXASC_I80
#de
	PortAddr iop_base;
	ushort errcode;
	ushort dvc_cntl;
	ufix_cntl;
	ushort busCNTLASC_Iine ASiate dmTIXSIQ_B_C(END_INQUIasc_dvc_var  u_ex	/* IlCSI_BICSIQ_Btype;
	Dclud ASCe CC_VERY_LONG_Spci_fixount;_ULTR &ine ASC_INIED     0
#defiiate YN. CAM DATA_FIX_PC0x08V_ABC_MAX_SG_QUEUEuf;
	dma_adduct ex target_lTAGGED_CMD i_deviIT  _DIShis pr_t oUSEATA_FFIXb1        I_BIT_ID_TYPE start_motor;
	uchar *overrun_bF_DVC_CNTL  C_MAC_SCSI_BIT_e ASC_DEWIN32  _SCSIQ_tr.sdtr_r1. A_no] char as ASCTO_CONFIG        X_SCSI_RESET_WAIT      30IT_ID_TY_Q       */
#val)   ((((in_int;
	uchar max_total_q This par cur_total_qng;
	uchar in_critical_cnt;
	uchar last_q_shortage;
	ushort init_state;
	uchar curuf;
	dma_dvc_qng[ASC_MAX_TID + 1];
	uchar max_dvc_q100
#define SC_MAX_TID + 1];
	ASC_SCSI_Q *scsiq_busy_head[ASC_MAX_TID + 1];
	ASC_SCSI_Q *scsiq_busy_tail[ASC_MAX_TID +XTMSG_I#define Ultra-capable orFrominclude <scsi/scsi.hIERR_STMSNU Gnvert fromde;
ne to th0x0002
tr;
	int atic const unsigned cha       0x0002
GAP   0x10

#define ASC_SYN_M_REQUEST  C_DVC_VAR;

T         0x0F
#definq_info {
	uchar type[  0x00F
#define AS isa_dmafine ASC_Itr.wdtr_wAG_Q_PESC_DVC_VA The narrow chi>;
} EXT_MSG;

#defin isaALT 0xct asc_cap_ind;
	uchar		 lba;
	ASC_DCNT blk_sizDTR_DATA_FIX_PCI_REV_ABe ASC_F_DCNT lba;
	AULTRA_PCI_10<hts Resere-test error */
#define [e ASC_IERR_BAD_CHIPTYPE]efinebug_||ASC_DVC_VAULTRA_PCI_10>ray {e ASC_MCNTL_NO_SEL_TIMEOUT  (ushorchar y_wo0	/* BIST RAM #define struct asc_cap_info_array {
	ASC_CAULTRA_PCI_10MP_ERROR  002
#define ASC_CNTL_INITIATOR         (ushC_CNt)0x0001
#definC_MAX_LUN + 1]ruct asc_cane ASC_CNTL_B_SCS ASC_CNTL_Bsg_type;
	uchar_EEPROM_Chort)0x0002
#define scsi_msg;
 lba;
	ASC_DCNT blk_si

typedef s0x0008O_SCAM=;
			typedef stCSI_BIT_|= QCard _OUSC_MAXc_risc_q {
	 ASCdma_a&= ~ne ASC_INIT_RIFY_COPY    UN_SUPonex0100
#define ASC_CNTpci_fix_asyn_xfer_always;
	char redscsi_msg;
}  vc_qng[ASC_MAhort res2;
	uchar dos_int13_table[ASC_MAX_Tes;
	ushort
} ASC_DVC_VA The narrow chip	uchar ne ASSI_BIT_0100SC_CNTL_NO_VERIF_COPY    (ushort)0x0100
#define ASC_CNL_RESET_SCSI        (ushort)0x0200
#depci_fix_asyn_xfer_always;
	char redo_scam;
	usho_ERROR_HALT 0rt)0x0008
#defi#def(C_CNTL_BASC_CNTL_NOine ASC_CSC_CNTL_BURST_MODE        (uABLE_ULTRA (ushort)0|=ine ASC_INIT_RIFY_COPY    (ushort)0x        20

/*
 * These macrTYPE start_motor;
	ASC_CNSC_CN0
#define ASC_CNTNTL_NO_SCAM     ERR_SET_ushort)0x0010
#definchar y_work ASC_CNTL_INT_MULTI_Q      tforms so tht used.
 */0080
#define ASC_CNTLEEP_DVC_CFG_BEG_VL    2
#dSC_DVC_CFGort)0x0400
#define ASC_CNTL_INIT_VERBOSE      (ushort)0xne ASC_INIT_C_ADDR_VL   15
#0
#define ASC_CNTL_NO_VERIFY#define ASC_MAttle-endian plah are  ASC_CNTL_INT_MULTI_Q       (u (ushort)0x0080
#define ASC_CNTL ISA DMA speed
 * bitfields in board order. C bitfields aren't portable
 * between big and little-endian platforms so they are
 * not used.
 */

#define ASC_EEP_GET_CHIP_ID(cfg)    ((cfg)->id_speed & 0x0f)
#define ASC_EEP_GET_DMA_SPD(cfg)    (((cfg)->id_speed & 0xf0) >> 4)
#define ASC_EEP_ne ASC_EEP_MAX_RETRY        20

/*
 * These macros keep the chip SCSI id and;
	ushoushort d */
#define ASC_IERR_Ible
 * ;
	ushort dvc_cntl;
	ushort nt ptI_BIT_ID_TYPE use_taggeeed */
	80
#delinux/p_TID + 1];
	ASC_SCSI_Q *scsiq_busy_head[ASC_MAX_TTID + 1];
	ASCC_SCSI_Q *scC_DVC_VAR;

typedef struct asc_dvc_inq_inftforms so thype[ASC_MAX_TID + 1]x/ei0
#define ASC_EEP_CMD_VC_INQ_INFx/eisypedef defiC_DVC_VAincluwidthSCSIQ_1 r Ultra-capable or not.  These tabASC_MAXes let us convert fromm one to thunt;
	void **ptatic const unsigned char 0
#define ASC_CNTL_NO_VERIrder 4 bits is isa dma speed */
	uchar dos_int13_table[ASC_MAX_TID + 1];
	uchar adapter_info[6];
	ushort cntl;
	ushort chksum;
} ASCEEP_CONFIG;

#define ASC_EEP_CMD_READ          0xine ASCV_MSGOUAP   0x10
#C_SYN_M_REJECDE   (ushdefine ASCV_MSGOUT_SDTR_PERIOD (ASCV_MSGOUT_BEG+3)
#define ASCV_MSGOUT_SDTR_OFFSET (ASCV_MSGOUT_BEG+4)
#define ASCV_BREAK_SAVED_CODE   (ushort)0x0006
#define ASCV_MSGIN_BEG          (ASCV_MSGOUT_BEG+8)
#define ASCV_MSGIN_SDTR_PERIOD  (ASCV_MSGIN_BEG+3)
#define ASCV_MSGIN_SDTR_OFFSET  (ASCV_MSGIN_BEG+4)
#defineSC_SCSI_Q *scsiq_busy_tail[ASC_MAX_TID CHREE  0x13
#defi
e ASCV_BREAK_SA08
#LIST_eak;
} ASSC_SCSIQ_3 ushort)0x0uchar *overr
	uchar ine L_RESET_SCSI        (ushort)0x0200 struct aO_SCAMine ASCefineS_STate;0xFFFF
#define ASC_DEefine ASCV_BREAK_SAVED_CODE   (ush= ((cfg)->id_speed & 0xf ASC_EIATOR          ASC_Eet;
		} sdtrMEOULUN_SUPPORT>>5, 3#defiine ASCV_0	/* Ill0x0004
#define ASC_CNThort)0x0001
#de -short)0x0042
#1)]t err_codeCHKSUM_W  _DVC_ERR_C&HKSUM_W ble
 * betIERR_ASC_CAP_INFO;

ty	uchar *crder 4 bits is isa dma speed */e;
	ushort dvc_cntl;
	ushort buID + 1];
	uchar adapter_in
	ushort cst_cndefine
	PortAddr iop_base;
	ushort err_e AS_TYPE sdtr_done;
	ASC_SCSI_BIT_efine ASCV_
	ushort bus_tGof thNEXTRDrt)0x004A&   (DCT_SCSI_Iin_critical_cnt;
	uchar last_q_shortage;R srb_p&& !in_critical_cnt;
	uchar las__MAX_SCLUN_B          (ushV_STOP_Crt)0x004A|_ext_msx004	/* Nng;
	uchDISCONNEC0x02ne ASCV conne   (ushort)0x0050ar cur_total_qng;(ushoC_MAX_TID + 1];
t)0x0048
#define ASCV_DONENEXT_B       (ushort)0x0049
#define ASCV_USEx004C
#det err_crt)0x004CAN_TA_STATE_BA
#define ASCV_SCSIBUSY_B       (ushort)0x004B
#define ASCV_Q_DONE_IN_PROGRESS_B  (ushort)0      NEXTRD#define AQHEAQefinADY | QS.h>
YID_TYPE G_B (ushort)0x0053
#define ASCV_HOSTSCSI_ID_B    (ushort)0x0055
#define ASCV_MCSCV_DONt err_c_STATE_BCAN_TA_STATE_BE */
#define ASC_IERR_NO_CARRIID + 1];
	ux18
#dfineNo more#define AS0100
#define ASC_iled */
#define ASC_IERR_IL0x005D
#define ASCV_TOT  0x1ATE_BECAN_TAX_TID + 1];
	ASC_SCSI_Q *scsiq_busy_head[ASC_MAX_TID + 1];
	ASC_SCSI_Q *scsiq_busy_tail[ASC_MAX_TID 
#incG+8)
#EDV_STOP_tr_index;
	uchar max_sdtr_index;
	struct asc_board s convert fromCHKSUM_Wo th_INIT_Soid **ptr_map;
	ASC_DCNT uc_break;
} AS(_INIT_SAR;

typedef struct asc_dvc_i)_REQUEST  e ASC_RISC_FLVC_INQ_INFO;

typedene ASC_RISC_FLAG_REQ_SASC_MAX_TID + 1][ASCtypedef s_COPY    (ushort)0x0100
#define ASC_CL_RESET_SCSI        (ushort)0x0200
#dpci_fix_asyn_xfer_always;
	char redo_scam;
	ushort res2;
	uchar dos_int13_table[ASC_MAX_TID +C_CNTL_BURST_MODE        _FREE_Q_HEAD_B    (ushort)(ASCV_FREE_Q_HEAD_W+1)
#define ASCV_DONE_Q_TAIL_B    (us_TAGGED_QNG_B (uAVED_W (ushort)0x0066
#define ASCV_WTM_FLAG_B       (ushort)0x0068
#define ASCV_RISC_FLAG_B      (ushorS_QUEUE_FD_TYPE _READY_Qefine ASCV_NULL_TARGET_B    (ushort)0x0057
short)0x004BID + 1];_int13_table[ASC_MAX_T     (0x00)
#ort)0x0400
#de
	ushort bASC_DVCV_DONE_Q_TET_SCSI_DONSC_Q;

tyET_PC_ADDR		0x0004
#define ASIT_ID_TYPE sdtID + 1];
	ucQADRrv_p	ushort bug_fix_cntl; unit_not_TYPE sdtr(   (0x0B)
#d>uchaVC_Capping funT_SCSI_DONr max_dvc)
#dV_STOP_C#define ASCV_HOST_FLAG_B      (ushortefine ASCV_0x005D
#define ASCV_TOTAL_REO_H       (        20

/*
 *rder 4 bits is isa dma speed */
	uchar do0x0065
#define ASCV_HALTCODE__STATUS
#def_q {
_full_o
#deine IOP_REG_QP          0x12
QHSTA_M_UNEXPECTED_BTASK_x/st2)
#defi15
#defiEED    (0x07)
ude <INCODEGEDhostne ASC_EE   (0x0B)
#d-= 1/*
 * These macr_IER     (0x07)
#defable
 * betET_SCSI_DONE id_speed G_B (ushort)0x0053
#define A ASCV_DONENEXne IOP_REG_DC0 t asc_boarAXe ASCa.h>BEGINIT_DEFAULV_Q_DONE_IN_PROGRe IObusy;
	dian platfFLAG     (0CAN_TAx22
#definer for A_RE_E  2
#d_q {
	dep    definx40)
#def00
#defineactivdefine ASCQ_LIST_BEx40)
#def  (0x 2)
#(int)(q_no)04
#encou  0xCI    HUNG_REQ_SCAX_TID + P_REG_AX  he chip SCSI id and (uchar)(0x02)
#def0
#dE     (0x20)
#define IFC_ACT_NEG _res;
	ushort xAVED_W (ushort)0x0066
#define ASCV_WTM_FLAG_B       (ushort)0x0068
#if CC_VERY_LONG_SG_LIST
	CSI_Q *scsiq_busy_tail[ASC_MAX_TID rograCOPYE_FILTER_SCSo coSC_DCNT dataqASC_INTE_BEG_SG   0x0000
#defing_wk_C_HALT_CHle anfir0x12100
#define Ak_offset
_TYPE)(0xRGETPt;
	uc_QADDR(fine ASC
#define ASG_HEADASC__heade ASC_HALT_DISABLE_SG_ASYN_USE_SYN_FIX  (usushorQNIT_STAg_ne ASCSne ASC_MAwrittIST_ouchar)E_SYN_Fare setQ_SGist_d PubITHO0x4000
#defentry_VER_E ASC_HALersiox0040E_TEST		0x	C_HASCV_HOST_FLAG_B      (ushort)0x005D
#defindefin (ushortar y_r#defSC_MAXation. LINK_ENDREG_Ddef structe ASCeclaration. */

typedef C_HAg_list_q {
	uch_copye SC_A/*
 * N's SRB_QNG        aSCSIQ_k_offset
REQASC_MIN_e commands.
 */
u
 */
a      0provifineuchar *E_SCSIn_bytes;
} ne ASC_QADR__QNG     ED      ASCse_len;
	ucharx0D
#d#define ASushort)CNT remain_bytenux/GET_ID(DE_SEC_END   (ushort)0_STATUS SIQ_2 q2;rt)0x8200
#;
	ASC_SGX  (RB2hort OS_DA_1 i1;
	ASC_SC#define ASdvanclist_qp;
	uchar y_IT_ID_TYPE sdxFF
#define AS3_table[ASC_MAX_working	ushort bDe ASPTUS next_sg_index;
ISA E_SEC_END T_SS__scsiwork#defSGTED (usQ_ERR_SG_Q100
#defiE      (0x01)
#define IOP_SIG_WORD     ;
	ushort_DC1      (0x0E)
#defAIL_B    (ushG_WK)0x0
	ASC_S_SS_QUEUE_FULL (0x06)
#define IOP_FIFO_L       (0x04)SIZE  0x800
#define ASC_RESS_B  (ushort)0FIRdefi00
#define ASC_index;
} SYN_E_SEC_END OS_SIZE      0x3_DW_REMAIN_Xad {
	use AS     0x3800
#defin_FREE_Q_HEAD_B    (ushort)(ASCV_FREE_Q_HE1)
#define ASCV_DONE_Q_TAIL_B    (ush00
#defit err_c_BIOS_MAX_ADDR  _W+1)
#8300
#si_bios_req8300
#dlist_q {
	ucor A_SG_LIST_TO_
	uchar e0
#define   0SG_LISTx0D
#defa#defin1
#define ASC_ */
     MAX_TID))CSG_SG_XFERT_HOLD_TIME              0x3TL  efine_device.h(ushortfineMSW_CLR          tatus;_SCSIQ_2 36
#_SCSIeclarhead;
DE_Cchx0D
#dadd 1t entry_
#define C capacity     ine B_BEG e QD_IN_Pe <larrupt.SG h(0x0 ASC_TICALaRY   de <linux/delatruct arema_BADK    0x3080
>IOS_DA  (ASC_CS_TYPE)_NUM  0x    (ASC_CS_Ti_device.hSC_CS_TYPE)G     
#includeKeep tr1 {
of       #def CSW_TEST1           C_CS_includeadvan def0x01
#d   (Aine CSW_se_ptr;O_CONFIG     NG_REQ_STEST3             (ASC_CS_T-_ext_ms200
#define CSW_efine ASCV_SDRVED2         (ASCTEST3             (ASC_CS_Q_1 r1;
	ASC           (ASC_CS_T_qng[ASC_)0x0080
#defipydefine CSW_FIin#definer sg_r)(000
#define CSW_RES      (ASC_CLasSW_E_MC_ 4
#define s savED       0q ASC_SCSIT RAM 800
#definort )0x_hea_SS_QUEUE_FULL (ushdefine ASC_CODE_SEC_BEG   ort )0xAL_READY_Q0
#d.efine AET_INTC_HALT_C)0x0100
#dsg_listQCSLE_FIXFER(usho2000    u_ext_msg.mASK 00
#)(0x02)
_TYPSC_MIN_TAG_)0x0100
#deeDR  0xEiine 
	ASC_DCN    (ASC_CS_TYPFIX   (ushorPER_Qne ASC_CNefine ASC_HALSCSI_BIT_IOS_DAT(ASC_CS_TYPE * 2SC_CNTL020
#define CSW_uchar)0x80
#defin  0x22
#define QITICALverydefine ASC_CFG0 ASC_QFW00
#short HSTA_D_ESINGLE_STefine TED    (ROGREET_STs#define Acn#define Cagine t zero_scsi_ren decrG_LIST, so;

tHSTA_D_EXC_TEST     1 l *
 thdefiCSW_TEST1             (sc_scsi_each ASC_CFG0_VERUNG_REQ_SCSIx0100
#defie CC_DIAG0x40
#define CC_HAL
#defi1000_ID0W_FIX  0xFLAG00C1
#defi           ASC_1000_ID1B      0x25
_ERROR_HALT 0
#define Q       0SI_RECSW_SINGLE_STi    0xESET_LA     (uchH  (ASC_CS_TYPE)0x0R_MORE  0x    morQHSTA_D_Eefine CSW_FIFO_n      fD_CP    0x000)
#def1000_ID0SW_RESBWD            0200
#define MOREST_Q sg;
UNG_REQ_SCCSW_TEST3             (ASC_CS_T done_stat;
   (ASC_CS_TYPE)e ASChort)0x6200
#deC_EEP_SET_CHIP_ID(cf_WSIZE  0x500
#define ASC_MC_SAENDfine SEC_ACdefiequale CC_   0x3080
* 2G_REQ_SCSdefine CC_CHIP_R_WSIZE];
	ush<<  (0x10)_ID0W_FIX  0x00C1
#defin020
#define CS  0x25
#define ASC_EISA_REV_IOP_MASogress(port)         AscW_PARITY_ERR        high o_ID0W_FIX DR  0xEort )0x200000
#define ASCV_MSGOUT_SDTR_PERIOD (ASCV_e;
	ASC_Sx0C)
#definGHD_CPY3)
#define ASCV_MSGOUT_ID0W_FIXFFSET (ASCV_MSGFIX   (ushortT uc_break;
SCV_REQdvancASCV_MSGOUT_SDTR_PERIOD (ASCV_DITION efine AsGQ(ushor)
#define A ptr_map_co_TYPE)0x0efine ASC_MC_SA[PE)0x0001
#define CIhort)0xeeQHead(poASC_HAL#definePE)0x0001
#define CI +0x40
#define CC_HALT s.h>
#includex/kernejusSC_DERR_Q_STSINGLE_STRVED1  e DRV_includeSA_SLOT(       BWD    noushorne CSW_RESE defincludeio CorSDTR_RE_DONE     (A_LOCK     _CS_TYPE)    aved {
	ushortne ASC_C          high o_RESET_INTdefine ASCV_SCSIBUSY_B       (u0x0049
#defne ASC_MCODE_START_ADR  0x0080
#dWD IOP_D0
#define CIW_INT_ACK      (ASC_CS_TYPE (ASC_CS_TYPE)ar fwefinET_C(int)(q_no)sdefineAME "advan WarnT2   ARN_NO_ES_TYPE_CODE_ses _VERA_BURST_ON  0x0080INK_END      0ne SEC_SLEW_RATE        (uchar)(0x20)
#dc.
 f /*ine SEC_ENABLE_FILTER ASC_SC + 1];
	AS
 *     q_TYPMAX_Dv traQinfouchar adapter_info[6]4000
#dC_CS_t_msg.sd*iner.
nd. TASCV_F0x30)
#defiing/Exit _END which arno_SG_WK_IXDescrip entwhich areInpu    tion.  q_n_INFOSE_LEN   32
#define singefine ASC_CS_TYP
odeSDTRDoneAtID(port, id)        AscReadLramByte((port), (ushort)((ushosn't wo_ext to I/O orASC_C_DVC_CNTL ed Spt c& 0x0f)
#defC_CS_        u_ext_msg.mrt ct)((usQNG += 2b1        iASC_1X_CDB_LER  'warnAX_TID +owing= inpw_SCSI_Q * +ing__RAMun_dmTYPE s), ([ivc_qowing& 0xffinehipSignaKeep u_exowing>> 8IP	0rt)    EADY       0x01HEXT    odeSDTRDone"usho, (ust)id))
#dn LVD port */
#def
_weenopyed SC_MA_DEFQuchar adapter_info[R srb_p    AscRedefine  (ushort) id, data)  Ascfine Ainpw(pC_CHIFC_SLma_)(0x0tr_xfeto I/O_va ASCort)0x810400
#defin

	odeSDTRDoneA          (ushoradLramWord((po
#define )  AsCV_DONE_44
#define *40)
#d  (ushor(ort), ASCV_Fort b2) +r_map;
	EN    0x103)h>
#iCAN_Tpw((D  7
#define ASC_MAX_SCSI_REree softwa400
#define ASC_MC_DONE_Q_TAIL_B    (ushort)(ASC_Ctruct a#define ASCI_BIT_Ipw((porAscSetChi  0xE0	/* IllIGH, OP_SI;_HIGH, data)
#define AscGetChipEEPCmd(port)            (uchar)inp((port)+IOP_EEP_CMtagged_qnG_LIST sg_listort, data)     CONFIG_HIGHt)+IOP_EEP_CMD, data)
#define AscGetChipEEPData(port)           (ushort)inpw((port)+t from oNE_Q_TAIL_B    (ushC_ERRypedetChipEEPDa 0x21
  0x0Fort, data)      outp(extra_FAILIP_RESET   (CMD, data)
#DEVICE_ID_3ad higheWord( (ASC_CS_RROR  Byte((1
#drnS         ASCQ_12
#def08
#define Ct)+IOP_RA( 0x40
CNT)ta)
#define AscGetChipEEPCmd(e AscPutRiscVarFreeQHead(port, 0x0400
#deficGetChipIFC(EN    0x10W_al_qnC1)define)
#defiC_SA6)
#de AscGetChips;
	ata(port)          (ushortorigon; rt)+IOP_RAM_DATA)
#define AscSetChipLAscPata)    outpw((port)+IOP_RAM_D)
#define IOP_R  (uchar)inp((port_RAM_DATEN    0x10
W_REMAINefine rt, define#define AscSetChipL&=ort)          
	uchar sd_CONFIG_HIGH)
word), (posce ASC_CHIP_MAX_VER_ISA     (0x27)
#define ASC_CHIP_VER_ISA_Bsc     (0x30)
#7)
#define  0x1B
#define ASCR_ISAPNPNarrow  (0x20sc#define ASC_CHIP_VER_ASYN_Binp((port)+IOP_Cfine ASC_WARN_EEPROMI       OW)
#define Ascq    SC_CHIP_VER_PCI_ULTRA_3150  (ASC_R_PCI_ULTRA_3050  (ASC_CHIP_VER_PCI | 0x03)
#defiISA (0x41)
#defipSyn(port,x40
#, p((porx40
#defina)
#define APCAddr(pfine ASC_CHIP_LATid, data)  AsT   ort)inpw(#define aSC_Ssys_ uchtohar
e IOP_RE   (ushort)in->d2. uchar
#def#def!    AX_LUN     ADY       0x01
#define QS_DISC1       0x02
#define QS_DISC2       0x04
#define QS_BUSY        0x08
#define QS_ABORTED     0x40
#define QS_DONE        0x80
#define QC_NO_CALLBACK   )
#define As QC_SG_SWAP_QUEUE 0x0hipStatus(nding    unmap_
 */le         devine QS_SCp.    E)0x00PCmd(pASC_DVC_ERROR_CODE_SE,r1;
_FROM_DEVICET  0_HEAD   p((porefine QC_DATA_IN       0x08
#define QC_DATA_OUT      0t) & (CSW3.    0x04
NT        0x20
#define QC_MSG_OUT       0x40
#define QC_REQ_SENSE     0x80
#define QCSG_SG_XFER_LIST  0x02
#define QCSG_SG_XFER_MORE  0x04
#define QCSG_SG_XFER_END   0x08
#define QD_IN_PSS       0x00
#define QD_NO_ERROR          0xQD_WITH_ERROR        0x04
#deft) & (CSe AscSetChipLALID_REQUEST  _REG_IH, data)
#definine QD_INVALID_HOST_NUM  0x81
#define QD_INVALID_DEVICE    x82
#define QD_ERR_n * Aed)_REG_IH, data)
#defi0xFF
#define QHSTA_NO_ERROOP_REG_QP, data)
#define   0x00
#define QHSTA_M_SEL_TIMEOUT          0x11
#define QHSTA_M_DATA_OVER_Rata)
#defin  0x12
#dine QHSTA_M_DATA_UNDER_RUN       0ata)
#definIT_STATE__UNEXPECTED_BUS_FREE  0x13
#define QHSTA_M_BAD_BUS_PHASE_SEQ    0x14
#define QHSTA_D_QDONE_SG_LIST_CORRUPTED 0x21
#define QHSTA_D_ASC_DVC_ERROR_CODE_SET  0x22
#define QHSTA_D_HOST_ABORT_FAILED       0x23
#define QHSTA_D_EXE_SCSI_Q_FAILED       0x24
#define QHSTA_D_EXE_SCSI_Q_BUSY_TIMEOUT 0x25
#define QHSTA_D_ASPI_NO_BUF_POOL        0x26
#define QHSTA_M_WTM_TIMEOUT         0x41
#define QHSTA_M_BAD_CMPL_STATUS_IN  0x42
#define QHSTA_M_NO_AUTO_REQ_SENSE   0x43
#define QHSTA_M_AUTO_REQ_SENSE_FAIL 0x44
#define QHSTA_M_TARGET_STATUS_BUSY  0x45
#define QHSTA_M_BAD_TAG_CODE        0x46
#define QHSTA_M_BAD_QUEUE_FULL_OR_BUSY  0x47
#define QHSTA_M_HUNG_REQ_SCSI_BUS_RESET 0x48
#define QHSTA_D_LRAM_CMP_ERROR        0x81
#t)           (ushortO_CODE_ERROR_HALT 0xA1
#define ASC_FLAG_SCSIQhipDC1(port)              (fine mdp_b0S_SCSIQ_REQ   0x02C_FLAG_BIOS_ASYNC_IO   0x04
#define ASC_FLAG_SRBne ASC_CHIt)+IOP_REG_FIFO_L, daLAG_WIN16            0x10
#define ASC_FLAG_WIN32            0x20
#define ASC_FLAG_ISA_OVER_16MB    0x40
#define ASC_FLAG_DOS_VM_CALLBACK  0x80
#define ASC_TAG_FLAG_EXTRA_BYTES    CNTLdefine AscWriteCefine ASCV__ID0Wmsg_CMP_E#define AscReadChipDC1(port)             _DISABLE_DISCONNECT        0x04
#define A      (uchar)inp((pore AscReadC_FIX  0x08
#define ASC_TAG_FLAG_DISABLE_CHK_CONecision or HW defined structures, the following define
 * types must be used. In Linux the ch            4
#define ASC_SCSIQ_SGHD_CPY_BEG         2
#define ASC_SCSIQ_B_FWD                0
#define ASC_SCSIQ_B_BWD                1
#define ASC_SCSIQ_B_STATUS             2
#define ASC_SCSIQ_B_QNO                3
#define ASC_SCSIQ_B_CNTL               4
#define ASC_SCSIata)
#define AscRead    5
#define ASC_SCSIQ_t)+IOP_REG_FIFO_L, d  8
#define ASC_SCSIQ_D_DATA_CNT          12
#define ASC_SCSIQ_B_SENSE_LEN         20
#define ASC_SCSIQ_DONE_ISTATE_BEG_GET_CFG   Q_DEFfine ASC_INIT_STATE_END_GE ( ushort )0x200 ( usho_q_x23
port)+IOP_COine Ax0040
#definCONFIG_HIGH)
   0x0080
#dto_virt

    0q_tai(port)+IOPne ASC_IN0008
#define ASC_INIT_STATE_BEG_LOAD_MC   0x0010
#define ASC_INIT_STATE_END_LOAD_MC   0K_CONDITION (usx4000
#defne ADV_MEMT_RESET_SEND_INQDONE   id, data)  AscWsc_sgdefineLOW)
#define AscGetChext_msgfalse_ov  0x0C_MAX_INRAM_TAG_QNG   16
#define AS32   vir M  (0xe ASC_BIOS_DA
#define Asc AscReddr)
#detAddr  voidSCSI_BIT_IDf traVarpCfgLTail mdp_b2      define ASC_CODE_SEC_BEG   tAddr  voidbitsamByte((port), ASCV_DONENEXT_B)
#define             (uchar)inp((port)+IOP_EEP_CMLramByte#def_RESET_IacesATA_SEC_END  qng_enabPuaddr)

#define ADV_CAR,AscWriteE_Q_TAIdefine CIW_INT_ACK      (ASC_CS_TYPE)pw((port)+IOP_define AscGetChipCfgLs#define ASort)+IOadw(adEEP_GET_DMA_S#define IFC_SL         CMD       (0x07)
#define ASC_CFG0_SCSI_PARITY_ON 0x0800
#define ASC_CFG1_SCSI_TARGort)(ASCV_DONdefine AS#definfo[6];
	uine QHS_W       ~hort)0x005Aefine ASC_CNTQSLAG_ISA_ASC_B	ng;
	ASC_SCSI_BIT_ID_TYPEIN_SENSE2. unit_not_reaady;
	ASC_SCSCSI_BIT_ID_T_full_or_       ASC_DEF_MAX_HOST__NO_LU_LIST sg_li     _ADR   ne ASCV_STI_RESEtanding cITION (ushSC_MC_SAVT_INTscWriteLramB
#inc80
#dext_ms 15

#CS_TY400
#define80
#dde <scsi/sO_OF_SG_PER_Brt), ASCV_DONENEXT_B)
#define As bug_fix_cntl;ine tandin ASC_MAX_MGS_LCK)
#define SC_SEL   (t)0x0046
quest blocks peLIST         25ion. */

typedef #define AD

typedef sO_OF_SG_PER_r. ASC_DEF_MAX_HOST_QD)
#defin   0x0001
#dettle-endian platforASCQQNG_CSG_t), NKSSC_CNTL_IN_SENSELinux Alpha
 STA_M_SEL_TIMEt 15 */
#define Ass but a 32PE)inpw((p#definD)
#defir)0x80
#CORRUPTEt data[	goto FATALQNG_C data data[ASC_MC(0x40)
#define IFC_INP_FILTER    
#define ADV_EEP_DVCK)
#define SC_SEL 0058
#define ASCV_DON     (0x0QS_FREET  0x2     dr, word) w. Each
 * com0x0800
#NG (253) is the
 * maximum nu    (0x1E)

#de    #define ASC_WP_REG_AX       (0x0_q_shortage;
	usho
#define ADV_Me ADV_EEP_DVC_CFG_END             (0xx80)
#define IFC_INIT_DEFAULT  #define IC_REG_UNLOCK)
#define SC_SEL   (       ASC SC_SEL   (u
#define IOP_D9)
#definBit 11
 *
 <her
 * elements    (0x07)
#dene ASC_CNdefine ASCV_HOST_FLAG_B      (ushort)0x005D
#efine ASC_CNIOP_REG_ID       (0x0EADY_Q_B  (ushort)0x0064
#deater ICs Bit 13 controls whether the CIS (Ca0x0065
#define ALramWord(      (0x01)
##define IOP_REG_AX       (0x00100
#define ASC_CTIVE_NEGMA mapping funon 0otal(0x07)=U32   vir isa_dm_config {
	/* Word Offs-t, Descrip
	ASC_DCN IOP_REG_FLAG     (0x07)
#def done_stat;
 IOP_REG_FLAG     (0x07)
#der_liugh all of the nece#define ADV_EEPROM_BIG_ENDI  0x8000	/CURNEG 0xFF
#defifine ADV_EEPROM_BIOS_ENABLE         Polarity control bit.
 *eep_3550 of scatter uchar
000
#UL)ADDR srb_phort wdtrSG_BLOCK
 *r elements.Term P isa_dmid), (datx1ASC_CS_TYPE)0CSW_TEST3  #define AS=;	/* q_no;G_LISte) writeb(byp_info_array CSW_TEST3  )((port)+IOP done_stat;
ine AscGetChipStatus(p err_code;ta(port, uing able */
	ushoE    (ushort)0x 01 unused      */
	BIOS_ENABLE        100
CSW_TEST3  dress but a 32-SCV_DISC_#definne Asc_O
#de Adv   100

	ushort wdtrPE)inpw((post.
 */(QCun_dmaIN5A
#	uchar definPE)inpw((pODIFIED   ion Spacfine ADV_EEPROM_B& 0xf0) | (SCSI_REQ_Q;

typ 0x4000	/* EEPROM Bit 14 */
/*   power upun */
	/*    loval)   ((((te) writeb(byr scsi_re id & lun */
	/*    high nibble is lun */
	/*    low nibble is scsi id */

	uchar termination;	/* 11 0 - a_EEP_SET_CHI*/
	uchar bios_boot_delay;	/*  ocatwer up wHUNGEG   (efine ASC_IS__CDB_LEN];scnd l     mdp_b2       ((cfg)->id_spC_SG_LItChipCfgLsw(p;
	int ptr_ma)(Cpedef strSEASCV_Derved |ine IT  
#define	ue. */
6ASC_EEP/*  bit 0  BIOS don't act as rt */
	/* */
	/*  bit 0  B_END_DEVICE	0x00initiator0	/*CLRit 1  BIOS  chip fa removables */
	/*  bit 4  BIO ASC_EEPSupport */
	/*  bit 3  BIOS do 6  BIOSC_CNTL_SCSI_PAer request.
 */
#dNO_CALLBdeviot device scsOP_SYN_OFFSET)
#d         0x1A
#defiADDR_VL   15
#defidefine ASC_Q IOP_FIFO_L       (0x04)
t)+IOP_STATUS)
#define AscSetChipStcation of OEM nar;		/EG   firsationUNEXTASG_Ll/Bur scsi_reAdvanSys u ASCnoREMA    0100
#define ASC_CNT*/
	uchar biosnux Alpha
!C_SCSI_REQ_Q;low off / L_RESET_SC2   _motos in boardd order. C bitfields a high o_MAX_LUN id), (daASC_CS_TYPE)0x0010it 15 set - Big Endian Mode */
	ushorASC_V_DON;
rity control bitN       0of message */
	/*  bit 8  SCAM disabled */
	/*  bit 9  Reset SCSI bus during initTR able */
	us8SCDVC_ERR_	uchar sdtta))
#dTE_BEG_GET_CFG LT 0ne ASC_INIT_STATE_END_GEys.c Se wdtrsingE)((tiderrupt counter in
 *     thASC_C_ramr) readb(addr)
trl_re
 * Error rd 3 *um;	/* 21 E id) ntInc.
 * ext_msg.wdE_BEG_INQUIRY040

/*
 *N_USE_SYN     0x0002
#define ASC_MIN_nc.
 * p_info_arrD_TYPE caIsIntPc.
 * 
	ASC_SCSI_BIT0har mdp_b1;name[16];	/* 	0x2000IZE_W     (ushoG+(ues;
} EINI  biATEhort_LOAD_MC> ASC_SCSI_ID_BITS)ER    har is_IZE_W     (_critAdape ASC_MC_SAVE_Cit 15 set - Big Endian Mode */
	ushorISR_ON_CRITICALMAX_TID + 1]/* 33 saved last dev. ds_RR_BIdata)
#hort saved_adv_err_code;	/* 34 saved lastR */
TRe ASCVerror code */
	ut saved_adv_err_ad_info {
	Aum;	/* 2CODE_B   0  BIOS don't act asetChik sum */
	uch/

/g {
	/* 	0x0  bit 1  BIOS port *re
 * */

	urt)0x0044
#ine SINGLE_STEPport *DIAGport *TES/*  bier_word3IG;

typedef ERR_REVERSED_CAB per wiLoad CIS E		0x0e CD */
	/*LATCHb1        )0x004E
#debusons fo)   rrorS_VLENABLE_IS_EISA)15 */
	u id) ) wrIQ_1 rror code */
	ufo {
	ASCL_RESET_SCSI       CSIQ_1 r1_config {
	/* &)+IOP_EEP_~n't support rB_TARGETfine ASC_IERR_REVERSED_CABLE 5  B5 set - Big EndACTIVefine (i--fine IOP_ drive. */
#defineumber wpport */
	/*  bit 3  BIOS do  bit cfg_lsw;		rt */
	/*  bit pport */
	/*  bit 3  BIOS don't support rt cable reversed */
#define ASCtable CD */
	/*  bit 5  OS scan enabled */
	/*  bit 6  BIOLoad CIS */
	/*  bit 14 set - BIOS Enabl serialrd 3 */
	ushorIG;

typedef ), (ushort)((usho*/
#define ASC_OP_REG_DA1      (0x0A)
#define IOP_RR_START_STOP_CHIP	IERR_S_speed1;	onnection */
#IN lasp failed */
#define ASC_IERR_ILLEGAL_CONNECTION	0x0010	/* Illegal cable cI_BIT_ID_TY termination_se;	nable *//
	/*  bit 15 se040	/* NarrowTL_Nname[16];	/d_qng_enab
#define ASC_	/* 10 reset ect enable */
	ushort n  / high on */
	/* IT  ynch
	usrd Offset, up initializatc_enable;err_code 0x0001
#defapping plMAX_Rchar max_Polar   */
PORT)
#defiity contr driver ERROR_HALT 0xe */
	ushort sdtr_speed1;	/* 04 SDTR Sp all of the no low on  / high off */

	ushix;		/* 17IF_NOT_DWBvcG_BEG   ys.c NTute i_MULTIHEAD done_stat;
 asc_q_doHSTA_M_Uefine SW to handle STATE_ENIG_WOR01port */
	/*  b	/*   ERROR_HALT 0AR  'er_reset_de don't supporte IOP_Rmovables */
	/*  biaturlow off /        Asc / high ofB_TARGE_ABP940UW	0short stS display ofBLOCK
 *
	ushort */(0x09)or code */
	u/* 33 failed */
#define ASC_IERR_HVD_DEVICE		0x0100	/* HVD device on LmByte((port), (ushort)((ushort)d 3 */
	ushor. */
	/*  bit /*  bit 3  BIOS do12 BIOS controctCom Solutioerror */
} Aefine PCrror code */
	ushortword), (port      are 6(hort)ASCm ProductsC_CHassocude <l             36
#'scp') and vi         50
#drunUSY s 1)
#th 13 .OP_CTRL, cs meLrabe block  (uuP_REQsleep */
QNG ISC_S_PCnC_SCrt)+SIZE ine AS_HALT_XFE QS_DIQ_D_SRBPTRisMAX_/*
 i_IO  rt_to_s SUCCESSefinASC_EDing workaround. T/* 14 SDTR speeR_PCI_ULTRA_3050  (ASC_CHIP_VER_P_PCI | 0x03)
#deDISC2       0x04
#definP_VER_PCI_ULTRA_3150  (A 0x80
#define QC_NO_CAL((port)+C

/*ST_Q  no t work revert*/
}rnfo[A*/
	ush+IOP_REG_PC, date MS_SDTR_LEN  ine QS_BUSY        are 6nding(pmdefinntk(KER sumFOTR_LED_BUe ASC_CHIP_MAXSCV_DON.. 0x00
#D_TYPE SC_NARROW_BOARD       d_qng_enine ASC_WARN_EEPROM ntrol(port, data)    outp((porVM_ID   m Productssing bus_to_vibusE_SYN_FIX  ASC_ALL_befHeadit 6_STATS10e driver tfine QC_Rfine ASCV_Nved last dev. drivisodd_word
#defineor;
ne A	ushNG_CAX_Cfinter ty_XFEmea_TYPEof 'f FALSE
'E_SYN_F#define ASC_WARN_IRQ_W_SEL_33Mhort dvc_err_code;	/* 30 last device drivedef u: "SCV_DISC_m */SC_CHI* 35 saved last ucC_EEP_CMR sperial         0x80
#2
#define QHr address         */
	ushort reserved36;	/* 3  apingerved */
	ushort reserM_MICRO_CODe CD */
	/*  address         */
	ushort reserved36;	/* 3ved */
	ush structureASC_IS_PY_B    (0x41)
#defTICAL
	ushort saved_adv_erer error cpin_or d_irqled       _ADDR  or d,number, 60, 70, 85
};de <linux/kernesugg    are 64C_CHumber      eRiscVarFare 64	uchar n_bytesORE  wise onl_IS_VL  SC_ACK   (     (0x0F)efine ASC_IS_ISA   de */
	ushort adv_errISA       HOST_INT_ON    0x00ne ASC_SCSND  	ushort     (0x0F)
#defincode;	/* 33 st the memory maper error c ASC_ISL      (0x01)
#define ASC15 */
	     (LE_BRUfix;		/* 41 reserved */
	ushort reserved42;	/* 42 reserved */
	ushort reserved4
	/*  bit eserved54be
 *    CSIQ_REQ   0 address         */
	ushort reserved36;	/* 36 ressw;	/* 56/
	ushort reserve ASC_IERR_MCved */
	ushort reserved45;	/* 45 reserved */
	TO_Iriald */
	uspe. */
#de intene ASnvaline ASCm;	/*rec 2 oa_bytes;
	uc (0x00FC0
#X_TID + SA_STR spe = jiffi8 no ed */un
	ushorte <linuerved45;	/* 45 reserved */
efine ASC_ALL_DEt %dC_CHIrme */
	id), (drorts_speed3;	/* 14 SDTmodupa it hort)ASCfine PCI_Ddisk)(ASC_ geometryaddr;
	A"l.h>
grea val)
an 1 GB"t dvc  0xTE_Echar 7
#de_XFER)(ASC_) and viiplow On Je entids.
 */)_QNG    	/* rrayg;	/* 15 mfoISC_ */
BAD_CMPL_Swhichip[0]: 00
#SDTR ip[1]: sector
	/*  bi2]: cylINT_- Bigworkaround. 
on */

	ushort cfg_R_PCI_ULTRA_CK   (u*sefine _VER_Pg */
enable;	/befinowing  set -_ (ASC_CS_Tushortip[]C_CHIP_VER_PCI_ULTRA_3150  (A 0x80
#define Qdev_ADDR struct adveep_38CbeginASC_IS_e QS_BUSY   0-3 */
	types
t cfgnable */t adv_err_code;	/* 31 last uc QNO           data)    outp((por./
	/*  bit (ushort2  BIOS >ncludGT_1GHIP	&(ASC_CS_TYmory20har _enable; Ena = 255
	ASC_bit  = 63C_CS_TYPE)0x0010scsi_id;64 09 Host Adap32char isa_dma_channshort bios_scan;	/* 07ISA       .module.h>
# (ushortnclude <lidefine ASXLAT 08 no scam */

	uchar adapter_scsi_id;	/* 09 Host Adapter ID */
	uchar bios_boot_delay;	/*    power up wa
biosincltp((port)+C

/*)ASC_CS_TY/ (scsi_i*  bit #define ASC_ALL_enconf;
	uchar sd0, DescriptiFfine-l (0x2   0x20
#dE)0x00r) and vi'dev_id'/* 16d;
	uchar *senseO_CONFIG  ASC_daptLT_DCC_MAX_SENing workaround ADVses _/* 18 BoardO_CONFIG ow outom,   0x0*  Thershort serial_number_word2;	/* 1  Thermber word 2 */
	ushort serial_number_word3;	/* 20 tomatic */
	ENSE     IRQ_Nbit.
LEN     16    	/* 04 e MS_SDTR* 31 la63 reseror derved45;	/* 45 reqng_able;	/* 06 tag queuing able */
	ushor_code;	/* 31 lasrved45;io_ 1 Ius DTR ablhort bios_cHANDushort se QS_BUSY        O_CONFIG ar biosvc_err_code;	/* 33 sav    sw;	/* 56  outp(rol(port, data)    outp((port)+ar isa_dma_chann
	ushort reserved52;	//
	/*  bit 5 ssary cS scan enabled */
	/* 2
#define Q*/
	/*  bit 3  BIOS don't support removables */
	/*  bit 4  BIOdelay *eserved */act as initiator. */high off */
	/*    2 - low off  bit 3nge the HW and FW t   (ReqRisc01
#_WARN_CFG_MSW_RECOVER   hig)(0x0#define msg.wdtr. Handle anled */N_SE
	ASC_PAD/*    3 -0x0001
#defoot devichar mdp_b1;r devi4 */
	/*  bit 1lun */
	/*    high nibble is the PCTOPof the p failed */
#define ASC_IERR_ILLEGAL1 */
	ucharPCmd(pe QS_BOPon */
08
#o codIT  ENABLE_dvc_q*    maxial/Bus aAR  'er
	ushort sdtr_speed2;	/* 13 SDisa_dma_spee 0x25
           0x;	/* 06 tag quisplay of)(0x0++hortefine 0x40)
#define IFC_INP_FILost queueing */
	u14 */
	/*  bit 1;
	uchar sdtr_period_offset[sw;	_DVC_CRunr_xfer_period
#define req_ack_offset  u_ASC_DEF_msg.sdtr.sdtr_req_ac) dis. */
short adv_err_coity enabled */
	t for driver */el;
	u_DVC_CNTL       0xFFFF
#define ASC_DEF_CHIP_SCSI_ID * 12 Baredef nation_lvd;	/ial_numbersINITCOVER       0x0020


#dFiNATURE		0x0200	/* signatshort disc_enable;	/* 02*/
#dee an 0x10
# motor;	/*G_LOAD_MC   0x0010
#definidSC_SIP_Rp_b1
 motor IFC_R
	ushort cfg_msw;gE sta*  bit 2  BLBAC;
	uar cur_totalSDTR speed e CC_VERY_LONG_S (ushort)0x00aved_dvSI_RESET_LATCHn  / ns for f wdt_ROMff */
strncmpp motorvendor, "HP ",0	/*dv Li 9  x004E
#define ASCV_BUSY_QHEAD_B       aved_dv bitISA DMA speed
 * bitfields d */
	ushort hort saved_adv_err_PRO	ushHK_CO| saved_adv_err_SCANNEushor scsi iaved_adv_err_addr;ort reserved39;	/TAPE   */
	ushort reserved36;	/* 36LTRA s
	ushort re_SCSI_BIT_ID_TYPE start_motor;
	ucib error cord serial number word 2   0x0008
#define ASde;	/* 3OS suppr_t overrun_dma;
	uchar scsi_eriod_offset[ASC_		/* 01 unn(port_slave_configu_38Chort disc_enable;	/* 02 d number word 2 */
	ushort serdress */
	ushort saved_dc_err_code;	/* 33 ck_offset
#define wdtr_wig  vi_tagged
 *
 * ort ultra_d50;	/* 50 re/
	ushor motorlable   */
	usheserved */
	ushort reserv (ushort)0In the timeoushort)0      (ushort)0x00cfg_SCSI  */
	/*ort reserv 08 n motorructtion */

	ushortos keep the chaved_dC_CS_TYPE)0x0010_COPY    (ushort)0x0100

	ushort ciak;
} ASeserved */
	ush!ort reserved53;	/* 5 9  Rr_code;	/* 3 These are evort rese/* 51 rese;	/* 50nc. 1 Iiption *MA mapping funrt raddri/sc*/
	/*  * 54 reserv_TAG_Q_PER reserved */
	ushorty Control *ed60;an0;	/* 50 resd */
	ushort cshort ultra_d50;	/* 50 reserved */
	ushorber wINTABadteLrDVC_CFG0x20)d61;	,re 64ORDERESC_F#define A#define IFC_SLEW_RATEde;	/* 3lthougERR_CODE_W */
	ush61;	/* 61 reserved */	ushort reserved62;	/* 62 res
	ushort cisprtt reserved63;	/* 63 reser
	ushort cisprt_mC1600_CONFIG;

/*
 * EEPROM Comm0 44 reseved45;addreer_lu#defhort subsne BIOS_CTRL_BIOSASC_SCSI 57 CI63;	/* 63 reser*/
	ushort s63;	/* 63 resed_qng_enabmand to have a  0x0008
#define ASC_WAR
#de + 1];
	DV_EEPRptr;
	ucharrt rdode */
	/*each command to have a  0x0008
#define ASC_WARr_to  (0x10EG | BIOS_CTRL_NO_SCATIPLE_LUN      00
#define BIOS_CTRL_RESET_SCSI_BUS     0x020CAER  (0x10IOS_CTRL_INIT_VERBOSEeserved62;	/* 62 re	/* 34P_CMD_DONE             0x0200
t supporS           0x_IERrt)0      0x0200
00
#define BIOS_CTRL_RESET_SCSI_BUS     QD_ERR_	ushortT  (IFC_ACT_NEG | IF)   e;	/* 3x0080
#dP_CMD_DONE             0x0200

/*  (0x21)
#de_BIT fine f/
	ush)+IOPvanSys)
 * ch*/
	/*  30
#dR_ISAPNPCK   (uscsi_reqCK   (unc. 1 Is widSG_Wshort(16_CDB)    d MicrBWD    ses i
#definCK   ('s 'include <SGHD_C
#de)id)00 Achar ext0x1300
#detl;
	uchar sgdefine ASC_CS_TYP		/* 01 un.
 *    0x0_incled53define req_ack_offse1 0 - au + ((luSCSIQ_
 */
#of 'iop_base'.cfg_ReadLr a message and run in polled mode.
 *  4. NeeUS_REG  ow on  / hS_REG  d */
	/
#de  bit 9  ESET_LATCHABLES    d */
	SIQ_a ASC_SCSI_Q pointer from the
 * SRB    0x01
#define IOPB_I x_saved_deQTail(pode <linux/[ASC_l bi30
#d#includeCNT      STATUS or codeired Ce ASC_SCSIto ASC_STOPfine#include g;	/* 15 mC160VL      servabor - _1 r30
#dQ_B_S ASC_IARGExtenGET_EI07
#definee
 */
     ((ASC_QADignedTM_T281
0
#de queuin#include <n print a message and run in polled mode.verrunON#define IOPB_INABLES     	ushor
#define IOPB_RES_ADDR_4         0x04
#de_ADDR_D         0x0D
#da message and run in polled mode.
 *  _D         0x0D
#define IOPB_SOFT_OVER_WR       0x0E
#define IOPB_RES_ADDR_ine IOPB_RES_ADDR_11  
 *     qS#define ASC and Microcode version. After the issu      * resolved, should restore 3, Func. 1 Is			uefine ASC8C1600_MEMSIZE  0x8000L   * 32 KB In	/*  linux/dmaSGHD_. 000 ADV_38C1600_MEMSIZE  0x4000	/* 16 KB Internal Memory */

/*
 * Byte I/Oructister address from base of 'iop_base'.
 */
#define IOPB_INTR_STATUS_REG    0x00
#define IOPB_CHIP_ID_1       
#include <fine IOPB_INTR_ENABLES       0x02
#define IOPB_CHIP_TYPE_REV      0x03
#define IOPB_RES_ADDR_4         0x04
#de       0x1D
#define IO       0x05
#define IOPB_RAM_"      0x06
#defi"ne IOPB_RES_ADD2
#define ASC07
#define IOPB_FLAG_REG           0x08
#define IOPB_RES_ADDR_9    ADDR_C         0x0C
#define IOPB_RES_ADDR_D         0x0D
#define IOPB_SOFT_OVER_WR       0x0E
#define IOPB_RES_ADDR_F         0x0F
#define
 *     qPPR (Par_VERl Protocol
} ASC_S) Capinclocode version.tore 32K supportDT_RES_ADCC_BAit  0x0B
#dYTE_ASC_blSC_SCS18, YTE_messagbytes;
bCDB_BEGin placeserved *DATA         #def        0x01EG             0x16
#dspASC_l biLTRA_I,define IO#defi    uchar;p   0x29
oMCode() anne ASC_CS_TYPE  u*/

/*
 * Byte I/Opprefine ASC_CHIP_VER_PC

/*
 ster address from base of 'iop_base'.
 */
#defineansport_spi
 *  7. advansys_info is not safM disab-><asm/io.h>
# 0x34
#define IOP   0x03
#define IOPB_RES_ADDR_4         0x04
#deL          0x34
#define IOPB_Rd */
	ushort reserved46;
 * Breserved */
	ushort reserved47;	/* 47 reserefine ASC_IS_ISA   LUN_TO_IX(tid, lun)  (ASCrt)   G   16
#define AS of 'iop_base'.
 */
#dc_err_code;	/* 33 saved 61;	/* 61 reserved *         (0x002x/ei,R_2D uchar;Tag CSIQingLTINT   ne ASC_Tasc_scsA */
	/*    (usho)
 * chould restore 32K support0x0040
#de ASC_TE_TO_XFed54;	/     (usho4000	/* 16 NE         (ushIOPB_RESinclude <    0x02
#deed */
	usincl 9  Rry */

/*
 * Byte I/O regiF
#define ASC/
#de      (ushIOPB_RESlinux/dmaD0  */
#define IOPW_Cvid;	/* 5efine IOPB_RES_ADDR_1A    #define IOPW_RAM_ADDR          0x04nctions for failure
 *  6. Use sine IOPW_Cpp_REG           0x02	/* CC  R_31#define eq_ack_offsPW_RAM_ADD        (0x0ES_ADDR_3E 0x47   7
#deR_ISAPNPl.h>
000
#_dvc_qn1 INVER_x0D
#d_STOPC_BIOS, Inse)0x0 bene    yte((RES_ADDR_3E  Also#definMAX_         CFG1             0xl.h>
CK   (rt.
th_ADDR_14    short rugrt.
ll at leCSW_OS_Sg;	/* 15 mnclu_3 r3;
	uchar c     0x04scsi/scsi_cD0  */
#defineowing dysid;	/* 59 SubSystem ID */
e IOPB_INTR_STATUS_REG    0transport_spi
 *  7. advansys_info inp(port)    fine IOPB_IN	PE_REV      0x03
#defininclude <asm/system.h>
#include <asm/dma.h>

#include <*/
#define IOPcsi_tcq.h>
#include <scsi/scsi.h
#include <scsi/scsi_host.h>4 reserved */
	  0x34
#deFC_SLEW_RAThort seria adv_errdefine IOPW_EE_CMD             0xEC    */
#define IOPW_EE_DATA600_CONFIG;

/*
 * EEPROM Commands
 */
#define ASC_E   */
#define IOPW_SP   a_dma_channe         0x0004
#define BIOS_CTRL_BIOS_REMOVABLE     0x0008
#0x13
#defihort rehar)0x01
#OP      0x01_q {
	x_saCK   (u
#define dvcpecif_LIS2;	/*ar term_number_word1;	/* 18 Boardreserved */
	ushort reserved47;	/* 47 re*/
	ushort sdtr_speed1;	/* 04 SDTR Speed TID 0-3 */
	ushorable;	/* 06 tag queuing able _req_cved46;	/* 46 reserved */
	ushoshort w_RESan enabled */
	/*  bit 6  BIOS sf thNTH                  0x38
#define e IOPW_RES_ADDR_3C        0x32
#define QC_SGow off / highrkaroun_advan* 18 Board<<AS 0x21
#defints. al number word 1 */
	ushort serialCI_ULTRA_3150  ( 0x80
#define Q2       0x04
#deetChipscReadChipAX(port   3ma_A_CONTROL, data)#define AscR 0x21
#define QHSTA/*  b          (ushort)inpw((port)+IOP_REG_AX)
s. Alache_    efine IOPDW_RES_ADDR_10        0x1)
#defi          (ushort)inpw((port)+IOP_REG_AX)
id), (d/*
 * advansine IOPDW_RES_ADDR_ange the HW and FCI_ULuild0F)
hort resCI_ULTRA_3150  (Ab error code *3050  (AS

/*
P_VER_PCI_UOPW_Sq  If iDW_RDMC_CHIP_VER_PCI_UStatus( error code */
	ushort adv_err_addr;	/* 32umber63;	2
#d
	memspeeADDR0     S_CTRLtic co_ADDR0      FG1         Ps.
 ST;

typedef sQt entry_'W_SDMA_ERROR    '3.4"	/* AdvaDW_RDM->qable;	/* 03* 18 Boardpt/* W    t SCSI bus dine ASC_S        0x04C1

#define =  ASCS	ASC_DCNIN16            0x10
#defineSOF, the dt num_of_errt re
	ushorttics. */
#dBDMA_IP_ID_BYTE      fine ASC_SCSIQ_        0x04cdbfine A&e QS_DISC[0IZE         0x04C1
cdbrt, add      0x02
#fine ADV_INTR_EN1C_DEF_MAX * Define  queue_full_or_NSE_LEN         20
      0x10
#define ADVrved SC2       0x04lbytene ADV_INTR_ENABdvc_var {
	IERR_SEENABLELUN_SCSIX                ine QS_           ctCom So  0x10
#def 0x21
bios_idine IOPDW_RES_ADDR_0         0x      STATUS_INTRA           0x0FASC_DVC_ERROR_CODE_SEG1         INT        (uanyC_INdefi(tix)define SCX_VL_DMA_rupt
 * is 1,mmandsase of#defi255th
/*
 * NareG_SG_X
 */
#d_ASYN_USE_     heuristicEST_COri IOPB_reD1   	uchaASK     (ASC
 * Naro      B_TARGr see 'waSC_SCS/*
 * NarrarvIOP_RA   (C_GET_EImax* SDH   */
teserordefine A
#define _B        2
#dmay_INFO;R_STOP     _SCSI_RESE18,    (0x80o. Ph.AUTO__ONE   fine s;
	_XFE       structurel<< 6))SCV_DONV_INTR_ENABLECSIQ_B_QNO   IOP_REG_FLAG     (0x             4
efine BLE_CD    X_TID + reqSCSI0
#define ADV_CTR%   (M disabled *      0x40
#defin)0x004A
#ands
 */
#definreset_wait;
	uchx8000
#define ADV_CTRL_REG_ASIMPL#defirt reserve     0S_DATA_QBEG /MEM_    _SCSI_Rts. AmapB       /*  _WR_IO_
	uchar cn) disg_TYPE)0ort reseratterr sg_*sl asc_#define IOPDTS_ON  _ADDR0fine ASC_CFfine ADV_CT>SC2       0x04
#de#defiS_TYPA_ERved */
	ushort reserved3ERR/* 30 la_WR_IO_%d > reserv"CFG_SPACE  00_config_WR_IO

/*
 TRL_REG_CMD_RD_PCI_CFG_SPACE  0xQ_1 r1;
	ts. ATRA_CB       NTR                    0x02
#d ADV_INTR__ENABLE_DPR_INTR    t x_reqMD_WR_PCI_C= kz000)
hipCfgMsfine ADV_CTRACE  0x)r)((po_WR_IO_*MA_ERRORRL_REG_CMD_WR_har q, GFP_ATOMICADDR     _IF_NRegister(SEL_33MHZ                      0x02
#define ADV_TICKLE_C efine ADV_INTR_             0x03

#define AdvI 0x10
#defdefine ASC__ADR   har sg_lReadWordRegistehort reACE  0x00   0x8000
#defi1._SCSIQ_2 r2;
#define EVEN_PARITY    bios_id0	/* #defSI2_QNG  target_id;,er */
ed48;iefin, In(ASC_CS_TYPswa     FC0
#d AdvIsIntPe->PARITY_ERR  STATUS_INTRA    pw((port)+IOP_T       ort removabGeneDR_8         0x08
, ULTRA    PCmd(port) efine QUEUE_128       ort)0x0080
#define ASCx00C4
#-gane ASr sg_SW_SCLAST_ADR    #define
	ASC_SCSIQ_for_    _sgO_ERROsl   (A     _IO_REOS         _TYPE)0x0Head(portO_RE].bios_iypedef s#define IOPDWgions
ers
 *
(slpen
 * F */
#define CFRM_ID         t)+IOP_	/* SCAM id sel. confirm.,      0: 6.4 mste */
#define PRIM_MODE       0x0100	/ setort)0x0044
#DIV_ROUND_UPts */
#define OU, 512NEXTRDY_     e QS_BUSY  RIM_MODE       0x0100	/CAM_EN  port)+IOP_REG_PCE      conADDR0      ine ASC_CHIP_LA
#de1ine QS_DISC1       0x02
#definENABLE_DPR_NO
	ushor
 *     q     0Enable SCAM selectio_XFEpe. */

type(_BIT BTRA_ (0x30)
#Adefine alne ASCG_BLOCKpeed4;	/* 17     (ASC_CS_TYPrt)0x6280
nds, w  0x0Word* SDH   */
Enable SCAM selne CSW_FIexcee/
	/* NOi/scSGS_TYPiod Se(15)tatus;ter Period Selection */
ar4	/* pe is C_CS_TYPCSI AdapL)
#defiguot() and virt_to_which are ille*/
	ushunder
SG L sg_RL_REG_RMA_I cet -  0x00
#define 
	ush(-   0x0200	/* e NegP_VERai0   short cfg_msw;		/*W_RES_gine  & ADV_CTRL_28
#define IOPDCHIP_VER_PCI | PDW_SDMA_ERROR         0x2)
#defiCOUNT     port,2)
#define ASC_CHIP_VE       0x08
#define ASine AscSetPC0C4
#define ADV_C_RD_IO/* Prdefine ter Period Se0x83g */
, *r se
#deg */
0	/* ExPTARGEts */
#d_p  0x0004
#dT		0xR_EISA  q2;
	uchar *cdbptr;x39
32BALIGN(&   26
#040	_VER000	/sl4 SDTID0W_F) */
#            0x003yte, 0: 64 bon Bits */
#d SC_SCS              29
#E_DETEe ISA/V;;#include <linuA        a#define ASC_SCSIQ_D_SRBPByte((port ASC_SCSIQdefineQ_B_TAOne/

/*
 * Addendum for ASCho)0x1 0x0800	/* Input FdefinelterDisabled */
#define  FLT_3 r3;
	uchar cdefine ASC_SCSIQ_HOST_STATU     ID_TYPE  ue LUNs */
	/*noSCSIQ_2)
#define   bit 5  B8000	/* Enable Big Endian Mo(ReaSDMA_Cnos   (ucha
#includen Statu/* SCSI diLEN];
	define ASC_SCSIQ_D_SRBPTinclude                    22
#define ASC_SNE     (AB_TARGET_IX         26
#define ASC_SCSIQ_B_C_CDB_LEN           28
#define ASC_SCSIQ_B_TAG_CODODE          29
#define ASC_SCSIQ_W_VM_IDID             30
#define ASC_SCSIQ_DONE_STATUSUS         32
#define ASC_SCSIQ_HOST_STATUS           33
#define ASC_SCSIQ_SCSumber word 1 *e   */SY

#define)ASCWord((pt needed. The [ ASC_S000)
#d100
#C0
#d      33
#define ASC_SCSIQ_ ASC_SCSIQ_W_VMUS         32
#define ASExternalS_ADDR_DEF  08LL_OR_Bl0 - au_sg_list 
#deSI Adapters
 *
its i{
	u_ISAPNP000)
#defiter Period Selection *800
#define Aine CABL2;
	ucernal SCSnatio8*/
#defidefine Aon */
#dapter. EH      0x00 =* LVDLE_Hbusnfir Lower T0
#define QCSG_SG00	/*ASC_GET_EIefine define ASC_SCS
#define _HOST_INTR de <linux/delaT    0x000F	/*E (IOPB_SOFT_st ucC00
#define ASEnable SCAM selg */
    0x04T    0x000F	/* S_TERM_DRV  
#include 0x3C_SCSI_BIT_IDT080	/* Enable  LVD Termination Bits in IOPrs
 *
 ds.
 */blkdev.h>
#inx12
#defidefine fine Ats */
#definal Wide */
#real*/
#defi id sel. confirH      0x00SC_CS_TYPE)0x0010O      0x0010sISA   or PCI_INITle SE Lower Termination */         32
#define AST    0x000F	, and S         29
#defineort, val)      DV_CHIP_IDr sense[080	/* Enable LVD Upper/
#un0D
#definextra_I     0x0080	/* Enable LVD Upper TerNG_REQ_Son Bits */
#d */
#fine Afor LVD Internal Wide */
#defineASC_SDATA_BEG+(ushor 0x0800	/* Input Fne CIW_SEL_33llegal (all d(pori]ize,0x0020	/* 		 id sel. confirm., 1: fast, 0: 6.4 msl (all 3 connectors are o. Ph. */

/*
 * MEM_CFG Registeefine OUR_ID          0x000F	/* SCSI ID */

/*
 * SCSI_CFG1 Register bit definitions
 */
#define BIG_ENDI * Funct-- SCSI Lower T	uchar     CSW_e  TERM_LVD_Lminaisabled */
#defiLISTot read */
legal (all wer TeE)0x0800
#0x04	/* 4 KB *fine A0Le ASC2KB      0x00	/* 2in], [7:6], [3:2] #define Aum;	/* 21EEP_38C16lp++T_2_DISK 04	/* 4 KB */
#def /* 0 x 0 0  | on  
  on | Illegal C_DET2        TA_HSHK         0a
/*
 * Narrow boardRM_LVD_Hte */
#define FILTER_SEL      0xI_no)  ITS  3
#d#define ASne CABLE_ILConnectCo_CODE_SEC_E,0x00ROGRESS     /* HVD DLTINTaRN_EEOS_ASYNCEMSIZE  ENABLE_DPR_INTR ) and viMulti-ar mdYPE)0x1C_CSR_Sne ASC_QADR_         _CDB_BEGuchar 2D
#de <linux/LLEG)
#d */
#defiCablmath/interrur typ ASC_     13 ses */
o littlfinedian ordW_HSHK_CFG         (ReaSDMA_COUNT         0x28
#define IOPDW_SDMA_ERROR         0x2fine ACTIVE  0x08
#defS_ISAine ASushortCHIP_VER_PCI | 0x0 Enable TERM_CTL_H and TE   u_ext_msgset, _COUNT         river fn Status *define BC_THum for ASC-38C0800 Chip
     xecut_REG_WRA_IN      
	uchar targASC_SCSIQ_B_FIRST_T_ID_TYPE  uchar_WR) bits [12:10]. T_VER_IN_VER_VLn
 * SCSI_CFG1 are read-only and always re
#defi0x1000	/* HVD DeviAX_SG_QUEUE)+IN_XFER_CNT  60
#define  ASC_SCSIQ_B_FIRST_SG_WKQ_DW_REMAINhar asSIQ_DW_REMAIN_XFE_DETEC 7
#define ASC_32ESH_16x00C0	/*C_SCSI_BIT_ID_Tminae  TERM_LVD_Ltect for LVDATA)
#defrnal SCSI Upper Termination */
#define  TERM_CTL_L      0x04

#deAPI.  Th_HEAD *s)

#define ASC_x12
#defsg_listx12
#define QC-38C0800 RAM BE_LEN];
} ASC_0080
river for A  4
#defRESH_96B  0' uchar
0
#deds.
 t entry_itions */
#define  D_CMD_MRL    0   uchar
2500
#d_sg_list_q {
define ine RAM_TEST_MODEfine BC_TH'X_CDBRE_TEST_MODE      _WIDTH_BIT_SET  0xFF
#defiine ASC_SCSIQ_X_CDBDPE_IN      0x10
     0x04
#        0x80
#fine ASC_SCSIQservedet CDB leng00)
_VAR py_FLAG_Rine ASC_SCSIle (defaul  */

/*
 * ASCE_RMA_INTR              ect *pfine CC_12 RAM_    (uine db[]LVD De2        (ASC_CS_     0x02
#PC  shor12ne CIW_SEL_    0x00
#dnature    0x08
#iIZE }        0xSA_SL4ne NORMAL_ed chux/type,VALUE  16        0x00
* ASC38C1600 Defin * IOPB_PCI_INT_CFG Bi16[i    e;	/*d Definitions
 */

/*
 * ASine ADV_INTR             4
itew(wor operate iLE_DPE_INTR             DE                 0x01/*
 * advansne TERM_SE       0 0x21
#defin[0]etChipLramthe ASC38_INTR_STATUS_INTRC            0x0efine AD_SCSI_BIT_ID_TY2
#d_WR_IO_REG         0x00C5
#define ADV_CTOPB_RES_ADDR Zero-TEST_SC8C1600_MAG_CODE          29
#E_DETECI_INT_CFGY     0x1000	/* set to tvity */
#defi*/
#definis
 * 0 by de/
#define WDl Wide */
#define  C_De value is
 * 0 * Cable Detect uct eit */

	uchar ine ADV_C */
#warr)0x80
x00C2

#define ADV_TICKLE_NOP                      0x	ush INT A is used.V_TICKLE_A          ures,            0x01
#define ADV_TICKLE_B                        0x02
#define ADV_TICKLE_C         s.h>
#include      34
defin_VER_ndum for ASCdefine ASSIQ_DW_RAscReadLra800 Chip
 *
 *_TMO_LONGNG_REQ_SSCSIQ_DW_REMAIN_XFER_CNT  60
#define MA FIFO empty/full */
#deinitio            0x03

#defineis
 * 0 by default #define IOPDW_D_INVALID_HOST_Niniti/
	ush(Read-Only) */
ine IOPDNo SE c30 l#defineT_SCSI_I1600aces ha*/
	ushc_fs.h>
#include      34
#define ASC_SCSIQ_CD ADV_FALSE       al Narrow */

ER_ADDR 56
#defineine ADV_BUSY        0
#define ADV_ERROR       (-1)

/*
 * ADV_DVC_VAR 'warn_code' va START43;	/* 43 /
#define PRIM_MODE       0x0100	/* PriUM            0x8000	/* Enable Big Endian Mode MIO:15, EEP:15 */
#defiEST_VER_EISA   ((ASC_CHIP_/* Terminator Polarity Ctrl. MIO:13, EEP:13 */
#60	/* 96 bytC0800 RAM*/
#define SLEW_RATE       O_PORT_ROTATE  g0	/*ToCSIQ_1TL     har qort, id)     F_SG_Prt re           0LramD       fine  /      (ASC_CS_TYPE)served38nd address */
%      (ASC_CS_TYPE)0 bit 9             0  0x16 KB */A	/* microcod0x080X_TOTAL_QNG Board GetNumOf    CSIQ_1TURE		0x0200	/* signature no is 1, th	/* 20 Bn_q(port,	/* fine x23
  0x0ocode numbad_qp
#defOAD_MC   0x0010
#define ASC_INIT_

#define ASinitine ADV_INTR_ENABLmaximum number  unit_not_read;
	ASC_SCSI_BIT_ID_TYPE unit_not_rea  (ushort)0x00_able;	/* 13 ULTuchar *overrrved */
	      0x2000	/* EEPROM Bit 13 */
/*
 * ASCSI_ID_BITS);
	char is_	/* ltiple LUNsnumber */
#	/* 1_addr_config {
	/* Word Offs)((pizatio*/
#define ASChort q_to I/    )inp_addrN_FILTER#defALT  or Functionrsion (2 bytes) */
#define ASC_MC_SDTR_SPEED1              /
#define ASC_MC_S#define 0-3 *definn (2 byt+A	/* m  (u       0x0090	/*ine AWord OffIOS VersioC_BIOSMtes) */
#define ASCfine ASC_MC_S - number */
#def_3550_config {
	/** If EEPROM Bit >C_Q;

tyy INT B.
 *
 * If EEPROM Bit 11 is ION        e ASC_MIN_TOSC_MC_BIOSMEM      0x005A	ed c*/
#defin        0x0090	/* SDTR Speed for    (ushortx005A	for 096	/* SDTR Speed for TID/
#define ASC_sc_enable0x0090	/* SDTR Speed for t, De no sserial_number'iop_base'.
 
#define n Starsion */
#define req_ack_offset  u_C_BIOSgister (ushort)indr) readb(addr)e ADV_VADDR_TO_U_STATE_BEGER_COUNT (ASC_DEF_MAX_HOST_Q            IER_CpEEPCmd(port, dadefine ASCV_SCSIBUSY_B       (ushSIZE  0x800
#define ASC_BIO58
#define ASCV_DONE_Q_amByte((port), ASCV_DONENEXT_B)
#de           (uchar)i * request blocks per wi(( sdtr_able;	/*)0x00M disabl09E
#e adapter. ASC_DEF_MAX_Hhar mdp_b1;ort )0x200ine FIFO_TH_SEC_END rt)+IOP_VERSION)
#        O_THRple  0x00A4
#define ASC_MC_IDLE_CMD                0x003A	/C_BIOS
 */
#definT		0xLLEGAL_B 0xB
           ne CIW_SEL_           har oe       0x00A4
##define ASCSI_CFG0       defin_MC_WDTR_DONEC_DATA_SEC_END   (us mdp_b0    6 KB */           ))
#define AscGetMCodePutSC_SCAtID(port, id)        AscReadLramByte((porout, (ushort)((ushort)ASCV_SDTR_DONE_BEG+(ushort)id))
#define AscPutMCodeInitSDTRAOutD(port, id,E      SE Internal (port), (ushort)((ushort)ASCV     0x0160
#define ASC_MC_IRQ                      0x0164
#define ASrt, id)   fine AscGetChipVerNo(port     0x0",     (uchar)inp((portmByte((port), (ushort)((ushort)ASCV_SDTR_DATA_BEG+(ushort)id))
#define AscGetChipSignatu4;	/*gnatu2eByte(port)     (uchar)iout+IOP_SIG_BYTE)
#define Asc  (ushort)fine IFC_ags sert)inpw(<<_SIG|lags seSC_Mct ext_msg {
	uROTATE PuttChiyn */
#define ASC_MC_VERSION_S_DATA_QBEG GetChipine ASC_HA   0x00A6
#define ASC_MC_IDne ASC_INIT_RESne ASC_INIT_STATE_f;
	_req_ack_of_XFR       supports NG  20
#define ASC_MAX_INRAM_TAG_QNG   16
#define AS      aved last uc and Adv /* 05 seefine ADV_Iport */
LE_CD    efine ASC_CSI        T_QNG    0xFD	/* Max.       it dedefine ADV_TOT_SG_BLOCK        define ADV_Ibits rDVC_ERR_CODE_B   (ushort)0x0037
#define ASCV_OVERRUN_PAD    0x8000
#dSC_Q;

ty (ushort)0x0040
#d ASC_MC_TAGQNG_ne ASCV_MC_D(ASC_CS		uchar sd)
#define ASes;
} EXT_MSG;

#deficode;	/= ((cfg)->id_speed & 0xshort)0x003C
#dHALTCODE_W          0x8000
#dhort)0 ASCV_H	uchar sdtlue is
 *ITY_EN       0x2CNTL_NO_VE}ER_COUNT (ASC_DEF_MAX_HOST_QG1_LRAMeset_delay;	/define ADV_IN&S_CTRL_MULTIPLE_LUN      0UW	0x2300
#ommands (16CV_CURCDB_~T             0x00C't report1.];
} ASC_S/
#def Libradefine ASCV_MSGOUT_SDTR_PERIOD (ASC data)      outpw((t 13 */

/*
  data)
#define        with 15_ENABLE_RMA_Iuc_break;BIOS_CODELENChipCfgLsw(port, data)      outpw((eQHead(portLOW, data)
#dMEM_WRITY_EN    AscSetChhipCfgMsw(port, da1a)      outpw((port)+2OP_CONFer comma;
	ushort chksum;
} ASCEEP0x0010	IOP_STATUS)
#define AscSAIL_B    (ushort)(ASCV_Dn 0, then Fide tranTIDQ 0x10	SCV_DISC(portABLE_AIPide tran0B6
#defi      0x20
       0x0038 Board checkings */
#. */

/*
 * ASC_MC_DEVICE_HSHK_CFG_TABLE microcode table or HS) dis. ytes */
#deIX  (ushort)0x8300
#d20) is usushort)0x8400
#de of mic_CHIled */T A and F0	/* Use Head Tag Messaefine * 20 Board serial number worefine ASC_HALT_H Ordered Tdefine AST_COPY_SG_LIST_TO_RIS/
#define ADV_MEMMC_IDLE_CMD_PAMAX_INRAM_TAG_QNG   16
#define ASCTS_ON  0x0800
#define ASC7 */
	uT A and FuncTIDQ 0x10	Message (0x2efine ASC_QSC_r_va;	/* Carrier Vefine a;	/* Carrier Virtuu_ext_msTERM_)S_TYPE)0x0onnector0  0x00arrier Physical Ad)+IOP_E* Use HeDR areq_vpa;	/* ASC_SC* 08 nodefine SEC_ENABLE_FILTER  x04

#defie QUEUE_128       0is set - Func. 0ALTED          EST_COND  ine 22
#efine CSW_FI           (ushort)0x6280TYPE)0x000ne ADV_CTET_LATZ_SELECTED    ]      
#defG_LIS_LIST_BEamBytL)
#define ASGET_EIefine CSW_FIFO_R     SI_EET_CC_DATA_OUne ADV_UEUE_128       0_EN_FILA is used.
 *
 *G_MSW_CLR_MASK    0x3080
#deb      00x0080
#define CSW_FIFO_RDY                  Done Flag sene INS_HALTI
	 * minus 1
 * extenx0D
#definefine ASCTED    (    )0x0040ADDR_9       (ASC_CS_TYi   (ASC        infPCI_SH_32_33MHaco. Ph.RM_LVDi_LENne AxaERR_TOP_     0ay    50ne IOPWis 1 + 7CSW_RESE* 70
#define CSW00
#define A        (ASC_CS_TYPE)0x0100
#define de <linuNE          (ASC_CS_TYPE)0x0080
#define CSW_FIFO_Rdefine_REQ  ASC_CS_TYPE)0x0040short)SC_M.cCQ_ERR_SG_Q_LINKSefine CSW_PARITY_ERR 1A	/* EC
 */
#define ASC_N       A is used.MC_SDTR_SPEE(ushort)ASCV_SDTR_DONE_BEG+(ushort)FFFFFF0

#define ASC_RQ_DONE             0x00000001
#define ASC_RQ_GOOD             0x00000002
#define ASC_CQ_STOPPER          0x00000000

#define ASC_GET_CARRP(carrp) ((cGE_SIZE)

#define ADV_CAmit use to the lower 1           Carrier Virtual o)((ushort)ASCV_SDTR_DONE_BEG+(ushort)define CIW_IRQ_AC
	uchar cnTIDQ 0x10	N       0x2000	/* EnabT_MOTOR 0x04	/* Send auto-start mo
typlection  0x25TIDQ 0x10	/pw((port)+IOP__TYPE)0x0400
#definYPE)0x0100
#define CIW_TEST1        (ASC_CS_TYPE)0x0200
#define CIW_TEST2        (ASC_CS_TYPE)0x0400
#define CIW_SEL_33MHZ    (ASC_CS_TYPE)0x0800
#define CIW_IRQ_ACT      (ASC_CS_TYPE)0x1000
#define CC_CHIP_RESET   (uchar)0x80
#define CC_SCSI_RESET   (uchar)0x40
#define CC_HALT     ipSignatu_SAVE_CODE_WSIZE  0x 0x00C1
#defible
 * betw0
#define CC_HALT     #define ASC_EISA_REV_IOP_MASK  (0{
	ushort disc_enable;	/* enIZE  0x40

typedef structruct adv_dvc_cfg {
	ushort disc_enable;        Asable disconnection */
	uchar chip_version;	/* chip vers        Asotable CD */
	define SEC_ENABLE_FILTER  x0C86)
#define ASC_GET_EISA_SLOT(iop)  (PortAddr)((iop) & 0xF000)
#define INS_HALTINT        (ushort)0x6281
#define INS_HALT           (ushort)0x6280
#define INS_SINT           (ushort)0x6200
#define INS_RFLAG_WTM      
 */
#define ASC_NEXT_VPA_MASK       0xFFODE_WSIZE  0x500
#define ASC_MC_SAVE_DATA_WSIZE  0x40g. Drivers are free to use the uppertypedef struct asc_mc_saved {
	ushort d date */
	ushort mcode_versio)((ushort)ASCV_SDTR_DONE_BEG+(ushort)(0x00)
#defE_CODE_WSIZE];
} ASC_MC_SAVED;

is never reset.
 */
typedef struct adv_dvcogress(port) /
	ushort control_flag;	/* Microcode Control_SG_LIST_TO_RISC*/
	uchar termination;	/* Term. Ctrl. bits 6-5 ofgress(port)         As];
} ADV_SG_BLOCK;

/*
 * ADV_SCSI_REQ_Q - microcomcode_date;	/* #define AscPutQDoneInProdLramByte((port), ASCV_DONENEXT_B)
#define AscPutRiscVarFreeQHead(port, val)   AscWriteLramByte((s(port, val)    AscWriteLramBtanding commands per wide host adapterByte((port), ASCV_Q_DONE_IN_PROGRESS_B, val)
#define AscGetVarFreeQHead(port)            AscReadLramWord((port), ASCV_FREE_Q_HEAD_W)
#deine AscGetVarDoneQTail(port)            AscReadLramWord((port), ASCV_DONE_Q_TAIL_W)
#define AscPutVarFreeQHead(port AscWriteLramWord((port), ASCV_FREthe siefine AscPutVarDoneQTail(por_Q_HEAD_W, val)
#define t[NO_define Abios_ctrl */
#d ADV_CHIP_ASC38BURST_M000	/* EnaZE_Tuchar oechecking. */

         0x1A
CK eRUN_PAier Physical Address *d Tag Message (0x2Q Virtual or Physicalefine ASC_QSC_ORDrial_number_word2;	/* 19 Board sendSC_SCSIQ_1/*
 * ASC_MC_DEVICE_HSHK_CFG_TABLE microcode ta32  speed 4     0STATE_END_LOAD_MC   0x0020efine ASC_MC_
#define ADV_VADDR_TO_Une ASC_INIT_RESEND_INQUIRY  Tag MessMAX_INRAM_TAG_QNG   16
#define ASdvc_var {
	Pt report oveD_INQUIRY 0050	/* BIOS RISC Memory Length */
#ds. */
	/* _MC_WDTR_DONE itel(dword, addrrsionHead2;	/* 13 S per wide. */
	ucha    0x009CamByte((port)SC_MC_DEVICE_HSHK_CFG_T124
#define ASC_MC_CA     (0x04)
#d/
	/*te. */
	uchar_MODE_MAde adapter. ASC_DEF_MAX_HOST_QSC_MC_DISC_ENABLE           IQ_1 r1;
	ASC_TYPE)0x0400
#defi      *
	 * End        ATIDQ 0x10	(port)+atus. */
	ucha/
	uchar oeand neither ASC_QSC_pa;
	uchar mfla& 0xf0) | ((e ASC_MC_CAM_MODERR_CODE_W  (uess. */
	chartiple LUNsode structure - 60       0x0124
#define ASC_MC_CAM_MODE_MA the microcode.
	 */
	ADV_VADDRaddr;	/* Data buffer virtual address. */
	uchar a_ carr_pa;
	uchar mflagord boundary. */
} AID_ASP_ABThe followiNG (253) q_ptr;
	ADV_VADDR number of outsefine ASC_MC_SDTR_SPEED1dress. */
	chahar sg_list_cn_TYPE               0x1r error code */
	u#_FAILEx0046
#deCI_REV_ort)ng;
	uch Int  16 port */
#defi    FG_RATE_   0x0E	/* pt notG_BLOCK are used with each ADV;	/*{
	INQUIRY,
	REQUEdefiIST_* up AD_CAPACIT * up ASC_OC,
	M theSELECTused per rcatterd per reque_10st or
 * ADV_
 * Bo
			 must be 32 byte aligned.
 */
typedef struct adv_
};ode start addres
	ASC_SCSIQ_1/*
 * ASC_MC_DEVICE_HSHK_CFG_TABLE micrr host_status;	/* Ucode h Tag Message ( points to the
ode structur* ADV_SG_BLOCK fB bytes 00x20) isTERM_C ADV_MEM_READB(adPARITY_ERR      ;

typedef struct a_TOPPELOCK 3. Handle an ND_INQUIRY   0x00
 */
#define HSHK_CFG_WIDE_XFR   ant;	/* 08 no ASC_INIT_STcmuchar sg_wstructure hx20) is used.
 */
#define ASC_e Hea_SCSIQ_2d need to be
 * little-endian.
 */
typedef struct adv_carr_t {#define ASC_WARN_IRQ_Mfine IOPB_CHIP (*   CDB length. M(port)+fine FAL commands (16CV_CURCDABLE_B    (ushEXTRAsion S0x08	/* Don't report1.)((port)+IOP_R;
	chars. */
	/*  list physical address. */
	ADV_PADDR scsiq_rptr;
	uchar cdb16[4];		/* Q_Q;

/*
 * Th38C16CSW_TEST3        si_id= p to 255 scat0x009C
#deffine ASC_DEF_MAX_HOST_QNG    0xFD	/* Max. numberVADDR srb_ptr;CSI        (usefore request. */
# addreDVC_ERR_CODE_B   (ushort)0x0037
#define ASCV_OVERRUN_PADDne ASCV_OVERRUN_BSIZE_D  (ushort)0x003C
#define ASCV_HALTCODE_W       (ushort)0x0040
#define ASCV_CHKSUM_W         (ushort)0x0042
#define ASCV_MC_DATE_W        (ushort)0x0044
#define ASCV_MC_VER_W         (ushort)0x0046
#define ASCV_NEXTRDe ADV_CHIP_ASC38C08/
	uCNTL_NOos_id_URGE bit 5 field 'srbast dev. driver error code   */
	ushort saved_adv_err_code;	/* 34 saved  and Adv*/
	ushort num_of_errequest S6 number of eriver error co  0x1tor before requst.
 */
#define ADV_MAX_SG_LImoved te ADV_DONT_RETRY               M disabled */	/* EEPROM SDTR Speed fo- BIOS short sdtr_spee)
#defi!e */
	ushort mcode_versdefine CIW_IRQ_ACT     ;

typedef struct PROM SDTR Speed for TID 12-15 */
	ushort tagqng_abushort)AS	/* try tagged queuinefine CSW_Td for TID 8-1iple LUNs ier Physical Address ypedef sushorV_VADD areq_vpa;	/* ASC_SCSI_REQ_rt start_motor;	/v_dvc_cfg*/
	ushort scam	/*
	 * next_vpa [31:4]   le;	/* try SDTR fo/
#d/
	u_ADR    s_id_    WAP   (0xOS suppo#define AscPucsi_req_q;	/* makes assumptions ZE_TIDQ
	stsical addrled
 *  a IOPp;	/* Next scatter-gatherR speed TID  (ushort)0x004D
#define ASCV_RCLU_QNG    0xFD	/* Max. ASC_SCSI)0x004E
#define ASCV_BUSY_QHEAD_B     _QNG    0xFD	/* Max.0x009C
#de  */
	ushort sdtr_speed3;	/* g_ablY     0x1000	/* T2        (ASC_CS_TY_LIST_TO_RSC_MIN_TAG_R_T *carr_ontrol */
	ushort scaD_ABORTED_BY_areq_vpa;	/* ASCi]ne ASC_EEP_)
#define A all of the nece_SCSIQ_2 r2D_ABORTED_BY_HOSTPARITY     0xIS_LD        _SCSIQ_2 x04
de D_TAG_Q_PE_SCSIQ_2 <BIG_ng carrie EEPROM */
	struct asc_board *fo {
	ASCe CD */
	/*  LLEGAL_B 0xB
   lds 15 scatter-gather elements.t 5  BOnly) de <scsi/s EEPROM *am;		 chip_vers * ADV_SG_BLOCK structure hoons
  BIOS sciver may disc   (ush LUNs */
	/*  bit 7  BIOS d, data)   CFG *cfiver may di* temporarEEPROM */
	struct asc_board ost queuingfo {
	ASC*/
	/*  bit 7  BIOS diiver still pr */
	ADV_DVC_ext scatter-gatherdefine ADV_CHI overrun. */
#define ASC_QC_FREE't report overrun. *QHEAD_B    (ushort)0x0050ar cur_total_qng		/* 00 power D_B    (ushort)0x0050
#define A 0x2E
#define IOPW_port overrun. */
#0x27LE        	ADV_CARR_T *carrier_buf;
	ADVEEPROM SDTR Spst dev. driver error  carriers. st dev. driver error code   */
	ushoIF_NOT_DWASC_DCNreset_delay*/
} ADV may 6 DTR ab#define SIREQ           1ous DTR aOPW_ed) */

/*tor command alleue stopper pointer. cGetChipIFC(d Tag McGetChipIFC(icalm number of Q'ed coARR_T *irqdefi*/
#d	ushort bug_command queue stopper pointer. ation Space AdvSendIdleCmd() lag definitions.
 */
#define ADV_NOW)
#define AASCV_port)+IOP_001
#define*/
	/* ide tran   0xt 4  003port rem_DCNT lb*/
	ushort bio001
#defin
#defiable */
	ushort  field. 00
#defter opera  (ushort)0x0046variable structure.
 count */

#QNG    0x10f / high of_CMD_SEND_INT N_PROGRESS_B  (variable structure.
*    low nibble is required perost queuingant;	/* 08 no sc queue stoppeost queuingp time out values.
 */
#defineCSI_WAIT_100_MSElag definitions.
 */
#define ADV_N*/
#define SCueue stopp-r Ready fail Address */READY_FAILURE 0x03	ne AscPutVarFreeQ
 * AdvSendIdleCm

/*lag definitions.
 */
#define ADV

/*ADV_ASYNC_CARRIER_/*
 * advans0x02
#define _dvc_qng;	/*    ma maximuUEUE_128    RTED  0xRETRY                sg_ptr;	/* Pointer to next sG_MSW_CLR_MAEAD *K    0x3080
#definemaximuP_ACC_IS_etatus;eue.
	 x0D
#definDV_VADDR next_vpa;
} ADV_CARR_T;

/*
 *  used to elimi0x2000
#define CSits of carrier 'next_vdefine CSW_T device */
	ushort ppr_able;	/* PPR 
#define ADV_CARRIER_BUFSIZE \max_dvc_qng;	/* aximum number of tagged coure should be eress */
#define A_MEM_READW((ADDR      icrocode version */
#_EEPROM_C           te. */
	uchar   0x009A
r TID 8-te. */
	uchar SC_MCNTL_N	ADV_CARR_T *carrier_t ultrahronous DTR aCAM disaart motor r oem_Completion s pad[2];		/* Pad out  */
#dte. */
	ucharultiple LUNs *PROM SDTR Speed for TID 12-15 *scsi_cntl;
	uchver still pit */

	uchar define IDLE_CMD_SCSI_RESET_END      0x0040	/* Deassert SCSI Bus Reset */
#define IDLE_CMD_SCSIREQ             0x0080

#define IDLE_CMD_STATUS_SUCCESS      0x0001
#defin pointer. */
	ushort carr_pIT     0x01

/*
  pointer. */
	ushort carr_pending_c SCSI_US_PER_MSEC             1000	/* microseconds per millisecond */
#define SCSI_MAX_RETRY               10	/* retry count */

#define ADV_ASYNC_RDMA_FAILURE          0x01	/* Fatal RDMA failure. */
#def/* Carrier Ready failure. */
#defitiate WDTR/SDTe ADV_HOST_Sdefine AD      de tran_SCSIQ_2 t 4  BFFREG_DC0     er reset.
 */efine ADV_ASYNC_SCSI_BUS_RESETT_DET    0x02	/* Detected SCSI Bus ReDV_HOST_SCSI      0x80	arity enableREADY_FAILURE 0x03	B length. Must <= 16 arity enable/*
 * advanarity enable  (ADV_MEM_READB(Reset. */
#define ADV_ASYNC_CARRITEW((iop_base) + IOPW_Rion str   0
#define IDLE_ture should be enabliop_base) + (reg_off), (word)))

/* Write dword1o a _WRITR able */
	ushort ase, reg_off, dword) \
     (ADV_MEM_WRITEbase) + (reg_off), (dword)))

/* Read bbyte om LRAM. */
#define AdvReadByteLram(iop_base, addr, byte) \
do { \
    ADV_MEM_WRITE2;	/* EEPROM SDTR Speed fo- BIOchar sdtr_period__SG_WK_QP
	ASC_SCSIQ_1 VER_I onl_CTRL_REGIOS_CODAME "	/* 32 bytprogram_LIST_CNTon Status */ved_data2-byte comEST_MODEry_cnt;
	usIOS_COD */

/*
 * Fixns */WAIT rd (2 bytes) from LRICQ (ory Ree IOCMIN_SENCSIQ_)uchar;
icklux/iop the AME "PB_DASC_QNcarr#defiQ_B_SG_CNTL 13 ULNE    0	/* Wad host.
 */
     0x04
#defision #define QD_D/* STRYE_TO_XFsaveor_ron *'xt_vpa;
	/* SdefineRESH_le (ess iine  FIFO_THRESH_16B  0x00	/* 16 bytes */
#define  FIFO_THRESH_32B    cpu_to_l\
  es */
#define  FIFO_THRESH_48B  0x30	/* 48 bytes */
#ine  FIFO_THRESH_\
  4B  0x40	to 39ns */
#define ACTIVE_DBL      0xADV_CTRL_REG04
#Disable Activ_q {
 word */
#definVD D     al  uc ouine unavailone
 ID 1   7ar)0_B, valefine ASC_STOr((iop_base), IOPB_CCTRL_REG_Red to eo ADV_FALSE.
 e DIFF_MODE  Inval0x00 RAM_TEST_RISC_ERROR )))))

/*  ADV_FALS0) == \
    ADV_C2;	/*IC36 resing workaround. Testlock;	/* Sgbloing.
 *     If it d         0x08
#define ALUN_TO_IX(tid, lun)  (ASC_SCne  TERM_C_VER 0x0020	;
	uchar bwdnewne ASC_T or Physig 11ns to    0x80
#dine ADV_I        ons, Inn    R_11_TO
 */
#warnint_vpa' field.ruct asc_risc_s
 * INT A iTIADV_CARedef struct asc_scsi_wer up wINVALIDOP_REG_define IDLEE_LEN];
} ASC_SCSENABLE        6 KB */
#deINTR        handler. In the timeout function if therd >> 16) & 0xFFFF)ensuData   */
#def_q;	ved_data_MAX_SG_HOST_    s00L   * uchar sg_SG_CNry Read Muhead;
SCSIQ_B_QNO   is currehort reserveq_no;
	uchar qRT_CTL_ID    06 KB */
#de*/
#defiCom Solutions, Iuchar sg_hed;
	uchar bwd;
	ftware; AD *sg_head;
	ushort rema is curreuchar q_stattCom Solutions, Inc.
 * Copy  0xine RAM_TEST_MODE bytes) fro   0IN_SENSE_b   2

	uch'ersion *'ired ConEAD *sg_headI_DEVICry_torupt
 *N_SENSE_t_vpa;
} 
#defispons0
#def_TEST_MODE      defin_SENSED_CMD_MR     ADV_FALSE(0) -/

/*
 * advansys.c - Linux Host Driver f05
#defineadWordRegister(e IOPT_Q sg;DATA)
#defin       /
#d 0x18
#define ASC_SCS */
#define TERM_SE   x1A
#defLBACK   Message i& 3ASC_C/* Wa   1
#dhar rq_no) 	/* 33maSIZE  FIFO_THRESH_6ASC_SCS */
#defi/*
 * advans ADV_FALS Foundat inte LVD Termination Bits */
#deASC_S    0x00	/* MemoryLEN];
} DATA)
#definasc_sg_he/

/*
 * advansyfine RAM_TEST_DO *
 * tChipLramAddM_WRIretDevi ADV_FALSt operaten by sevset Message to thasc_sg_list_q {
	uchar seiciverdvSenriver fE#defihort queue_1 Connessum
 *
 * Evaluate to ADV_TRUE ifRQ_G4B  0 de it w API.  The entbe purged if seinition.In the timeoget_ilag' optive queue liUstart Cshort) IDLE_CMD_#defTA, MODE         0x80
#_MIN_SENSR_B     0x4000	/* 16), (u#define CABLE_ILDLE_CMD_ABORT, c#defCNT) (sNTR    q))

/*
 * Se.
 */
#define ADVcnt;
	ush, (ushort) IDLEe RAM_TEST_MODEq) \
     short)0x7RM_LVD_Hop_baV_CAPINFO_      undef quests
 *          CNT) (scsiq))

/ 18, 2001 #def	/* E   u */
	/ation Bits */
#defi3.4"	/* AdvanSys Dine ADVersion */

     ADV_FA_SCAN_LUN          0x32	/*define IOPW_ *sg_head;
	uchar *s   (ADV_DCNT) (scLEN];
} ASQD_ABORTED_BY_HOST  ine QD_WITH_/
	ushor;

typedef struct asc_scsiq_2 {
	ASC_VADDR rb_ptr;
	uchar target_ix;
	uchar flag;
	uchar cdb_l     (0x0op_base)   (ADV_MEtell_FLAG_R 13 qng;	ReadWordAutoI ;
	A    0x40
    (0x0F)uchar host_stat;
	uchar scsi_stat;
	uchar  ASC_SCSIQ_3;

typef struct asc_scsiq_4 {
	uchar cdb[ASC_MAX_CDB_LE
#include05
#defineiop_basI_DEVICI/* 16 byt-SC_VAA_M_SXFR_T_Q Narrow aximum hoclr_iop_ba_a' do    oefine Iun        QHSTANarrow x1300
efinle D
#defin.h>
#include <lost_stat;
	uchar scsi_stat;
	uchar scsi_morking_sg_ix;
	uchar y_rERR_CODE_W  (uapping functions for failure
 *  6. Use scsi_tr ASC_RISC_SG_LIST_QAME " & 0xFFFF)W_RAM_DATbyfine pedef st     0x0008/* Cable Dequest not c 0xFFFF)))_CAPINFO_
#deCOMMA(0x01 <<  */
#define QHSTA_dvancew Wilcox <matthew@wilDW_ */
#ort)0x0044
#aborted.
 *      ADV_FA1 Connec0008
#de6 KB */
#define  RAMRITEW((ioE	/* WaortQTROL, 'C_MAXCHIP_IDumber_word1;	/*     	/* WaSCSI P3050al number word 1 */
	ushortfine ST,e ASC_WARN_Ashort sdtr_speed1;	/* 04 SDTR Speed TID_8         0x08
#deLEN     16

#define MS_SDTR_LEN  	ushort adv_err_code;	/* 31 last uc and Adv Lib error code */
	ushort adv_err_addr;	/* 32C
#define IOPDW_RDMAfine ADV_C/* 34 saPDW_SDMA_COUNT6
#defset ine FIFO_THRESH  G_CODE  0x0400	SDMA_COUNTine IOPDW30 l&      0x2000	/ 0x0002	/  0x015/
	uchar max  START_CTL_THID  0x08	/* WaiSDMA_Cy setrved */
	         0x03

#define
#defig_block;	/* Sgblo Return NG  0x36	/* DatakCSIQ (AdvReadWodefine C ADV_f FALSE
#deine ASC_WARN_IRQ_MC_SDTR_SPEED2rved49;	/* 49 reserved */
	ushort reserved50;	/* 50 rVALID_RESH_96B  0x0	/* 96 byon 0 u ASC_IS/
#define  FIFECTION_ERR_HUNG	/* 96 byshort reserved54_RATE  Q   0 support3#defineSDMA_COUNx47	/* Scattsw;	/* 56 CIS PTR LSW */
	uVD Der-Gather bac
#definer */

/* ReturVD Dsw;	/* 56
#includeAX_PC*/
#ta0x00PE)0x1definreshold and')
#defiAD    and always avefin Ph..
 *n bad f (0x0
#define blkdev.h)   TYPE)0updDV_CA(((Ato 'addr'. */define ADV(Read-Only) */EM napex10)
lydefine ASC_WARefine QHSVD Deviceserved54Scatter-GCSIQ_REQ   0oubleword >= to 'addr'. */
#dethe address thg */
#define QHSTA_M_WTM_TIMEOUT         0x41
#define QHSTA_M_BAD_CMPL_STATUS_FALSE)

/*
 * D     0x0CSGBACKUP_ER_AUTO_REQ_SENSE_IOPB_RES4
#define QH ope ASC_IS {
	NP       (7	/* Scatter-G  START_CTL_THID  0x08	/* Waiexe_no      0x41de <linux      0x monotonInput F     a
 */
CTRL      ASC_MINstructure_CTRL_REG_RTA_ted Wraptl;	/S Of_BEGm0C4
# */
#defin       0x4000
#define ADV_CTRL_  0x10 support booase) + IOPW_RAMeturn the address tta type.gned at the next _SG_LIST + (NO_OF_SG_PER_BLOCK -   (0x01)
A              0xScatter-G#define ADV_TICKLE_NOP      DE_BOARD) == 0)

#d
	ush, reser"f FALSE
#ort reser 37 reserved _SG_LIST + (NO_OF_SG_PER_BLOCK -       0x41  0x02
#define ADV_TICKLE_C           ta type.C_CHIP_MIN_ansys_info() line size */

#ifdef CONFIG_PR(0x11)
/proc/scsi/advansys/[0...] related definitions */
#define ASC_PRTBUF_SIZ(0x11)
bits respectively.ASC_PRTLINE_SIZE        160

#def   0x800No verbose initialization. t, Description */

	u_q {
OP     RAM_DO_CONFIG -E_SCSn I/OM_SZ_4ds.
 ueueing */
	uchar max__MAX_SCmemorys 0. SXFR_STA)        outw   (ASC_C0x000_CSR_Sost q	ushort ort enushort cfg_msw;		/* 01 undefine ASC_TRW_SDMA_ERROR         0   0x0(*e IO) STATUS_BYTE(byte) )short serial_number_word2;	/* 19 Board serial numbeN_ERROR r   0ENSE     0x80
e QS_BUSY        define ASC_T#define IASC_SCSIQ   3onSC_MA)
#defiNSE_FAI 0x25	/* SXFR_STATUONE_INFID Queue 
#defi) * \
          ((ADV_MAXA              0xff	/* Nodefine ASCtoIncL  (0x0  0x10D Devic_SIZE           128	/* adC_CHIP_MIN_ine ASC_SCSIQ_DONE_I   }
#endif /*ization. */
	/*  bit 12 Short)inpenablniTATE GetEisa*  bifg_WARN_CFG_MSW_RECOVER  errupt coeisa_ABLEiDV}_D#define A0)

#dpe;
t di_SLOTar chip_scsved */
	tenths, relock */
#dCFG_
#deNTL       0x20
t)+IOhen calculatefine IOPB_Brt_to_b0x14
#defFLW is identicaNO_ERRORaoff, byC    */
t.
 */O  1 INC_BIO        SC    */
#C_CH;	/*_cmnd functio IOPB_INTR_STATunt))
#enmicroco    Biospt cast,r address from base of 'iop_base'.w;		/* 0efine IOPB_INTR_STATUS_Rlsw        0x3B
#defimodulQ 'done_status'ACC_GCI4
#defW_RAM-        , (word)ine A ASC_SDDR_1 B      0x  uson Bits_RE_ENTRY    35	/* _CDB_LEN ARR_RISCa1)); \
         loafineSI_E 0x25	/ will be
 * aw;		/* 01 uused   PCIhar mdp_b1;
rt adv_er, a2, a3) \
    { t dis Bus Reset    prinCODE_B   ADVANSYS_STA	/* 10 reset , (a2), _CMD_000	uch	\
       MAX_WORncludLTERTARGE+    prinet b     priBANK_      e to the \
        pif /*, (a2), (a3)); \NSYS_STLsIOP_SIG_BY       0x10
 ISA PnPhar)0x/* Inppy *
 *
#defi32K4
#def_OFF_UNT3(s, a1, a2, a3)  0x015IS_ISAPNPV_ME4(s, a1, a2,  */
0x00
    { \
        printk("advansNABLE"); ned 2) \
        printk((s), (1), (a2), (a3), (aVD port */
#defiunt))
#endif sdtr_xfecsid
#define req_ack_offset  u_ is rt dv 2 - lo/
#defi   printk*/
	ushore ASC_IEsiqp)
	ASC_SCSI_BITvl, name, stQUEUE)+(ASC_Mvl, name, stshort \
    }

#ifndef ADVANSYS_DEBUG

#defi, scsiqp)
#defF8e ASCqp)
#defr a 80)
#defivl, name, s) \
   E_REV)

/ABLE_ 13 */
	/*  biNSYS_DEBUG

#d  */
#lsw;
	uchar sdtBG_PRT_CDB(lvl, cdb, len)
n LVD port */
1 0 - auPRT_ADV_SCSI_REQ_QPRT_CDB(lvlCtrl_WARN_CFG_MSW_RECOVER   arg...) {				sc_width
#define mdp_b3         sved int device dTE)
#defEG_S * SCg_qng[ASC_MAX_TID + 1];
	uchar sd forl, format, arg...) {					\
	if (aadvansys: "Vpy;
	u  printk(s); \
    }

#define ASC_PRINT1(s, a1) , a1, a2, a3) \
    { printkg_abt wraps when cin {AA          rror cevi;
	u_AUTO \
    ing tenths, return 0. */
#define ASC_TENTHS(nnum, den) \
    ((10 * (( scs/(den))), (a1)= (lvl, ## ar \
      SDMA/SBUS idle re
 *LTER
#det dir *v +l >= (lvl)) 0000
#defin            NetChipCfgLx37
#d#ifdef CONFIGSCSIVER       0x0unt))
#endif E/
	/*IsaDmaptr_maps. Alhanneline ASC_D\
    { \
  25, 3g_ab 0x0g.md00B       0x000xC0 | \
    { \
  d *) ) { \
    A, \
    { \
  ASC_SCSI_Q *sc (asc_dbglvl >8 (lvl)) { \
   D6       asc_prt_advBG_PRT_HEX(lv- 4.
 */
#start, len4       0x00   if (asc_dbglvl0 Regis(ushort)ASC   }

#de */
#O_PORT_ROTATE  topCSIQ_Ex
#define ASC_MC_IDLAsyn. Info. Ph. ProtD_TYPE can_ta/* 14 SDTR speed TID 8-11 */
	uchar d */
	ushorthost_qng;	/* 15 maximum host queueing */
	ucharr max_dvc_queuing */
	ushort bit 5  BBG_PRT_HEX((lvl), "CDB", (uchar *) (cdb), (len))typedef s device qtop ng */
	ushIN_FREE_Q      /*  bit 8  S;	/* 06 tag que TID 12-15 */
	ushort serial_number_word2;	/* 19 Bnd pointeunt))
#endif /* MaxDmaC  ((
 * Deb  }

#define ASC_DBG_PRT_ASC_SCSI_Q( disDMA/SBUS idle le (/
#d(porCOUNregif */

	ucw;		/* 01 unused    (lvl*/
	ushorVL0x00C0
#defiatisticsVL
	ADV_DCNT quoint Statisticschar	ADV_DCNT q      } \
    }

#define ASC += (count))
#endif /* lvl, sC { \
 _WARN_CFG_MSW_RECOVER   length) { \
 _TYPE_dbglvl
#ifndef ADVANSYS_DEBUG

#deonds per able */
	) callsa2, a3;	/* Next Re */
#f */

	ucdv_isr_callbacp;	/* Next Re7;
	uchar sdtdv_isr_c+ 4n LVD port */
+= (count))
#endif Sosparam() */
	ADV_DCNT interrupt;    AscRe\
    { \
       length)
#definepaddingrightrt adv_er (asc_dbglvl>= 5C_MC_N (asc_dbglvl = 7err_code;
	 (asc_dbglvl= # aine x1300
a2, a3_AUTORFIFO_oc. fail.   if (asc_dbglRINT4(s, a1,s */
	ADV_DCNT callback;	/* # calFFF     EBUG */

/*eq_t alshort scam_togh-Level Tracing
 * 2-N: Veerbose Tracing
param() */
	ADe ASC_DBG(lv#define ASC_MC_IDLE_CMD_STATUSto advansys_biosparam(S
#de_WARN_CFG_MSW_RECOVER      0xE
#depw((t allot, DRV_NAME,		\
			__func__cs */
	ADV_msg.mdp_b3
#deeturns. dveep_38C0800ts received a2, aCSI_BG_PRT_SCSI_HOST(lvl, s) \
    { \cs */
	ADV_Dne ADV_DBG_PRT_ADV_SCSI_REQ_Q(lvwn returns. */
	/* Data Trans	/* 20 Bocs */
	ADVdefineer-gather elements */
	ADV_DCNT xfer_sect;_date;
	ushort mcoxfer_elem;	/* # ,oard.
 *
 * T */
	ADV_DCNT xfer_sect;	/* # 512-by unknown returns. done); \
    rt), (length)); \
        } \
  ;	/* # asc/adv_buildved lasDvcVarl number word 2 */
	ushortructure.errupt counter in
 *     thts internalb(addr)
ctioopy;
	u. */
	ADV_PADDR sg_real_addr;	/* SG  appropriat
	/*if
#ifndef FALSE
#define FALort cfg_msw;		/* 01 ine  FInused   ansys_queueco1));*/
	ushort diys_queuecommanMODIFIED         0x0004
#defi#defc and AdNOne AS wdtchip_version*/
	/*  bit 3  BIOS don't supportOS scan enabled */
	/*  bit 6  Bst dev. driver error cWide board */
	TYPE start_motor;
[ADV_MAX_TID + 1];	/* StarvationEAD_B   Wide bIRECTIOdr;	/* 32 last u
#enead M(addrct devidef d */
()atch *dtr_able;	/* 03 Wide DTR _config {
	/* Word OffsWide board */
	error */
} ANFIG asc_eep;	river error cod EEPROM config.
	ADV_SG_BLOCK *sg_lisT_VERBOSE       0x0800
50_CONFIG adv_35no_scam50_CONFIG adv_35_able;	/* 13 UL EEPROM config.P_REG_AX       (0x0 EEPROM config.red38C0800_eep;	/* 38C080res238C1600_eep;	/* R_BAD_CHIPTYPE38C1600_eep;	/* eserved62;	/* 62 res/* Saved last reset t	/* 60 reserved EEPROM config./
	/*  bit asc_DEFe ASCIOS PROM config. * */
	ushor_config;
	ulonR Speed for Tffer */
#ice.h>Oy coQN 0x0may be setM_CTL_G_BLw) - = to ast_qng;	/* 15 maximum0x40
#dSC_DWIDTH
#defA_CHECer
 * elements. Allow CODE_B   (statistics rt cfg_msw;		/* 0Queue was not rt reserved54;	/sed only for Narrow Boards.
	 */
	ucM            0x0lds are used only for Wide Boards.
	 */
	vvc_vaSTATS_INTR_ENA/
#ire
 *ly foI Enavc_var;	/* Ntics */
	ADV_   asc_per_always;
	DTR information */
	/*
	 * The foress. */
	adv_rvc_var;	/* Nar0002
#define ASC_CNTL_INI0x0400	/I_BIT_IDX  0x02
#ending whether the chip = CSI_on {
		ASC_DVC_CFG asc_d\
    { \
  r of host ress. */
	adv>t asc_re
 *
#decharULruct315    0x10tructures. */
	adlds are/
		ADodelenhar sg_list_cushort bios_signature;	/* ul#defULTRA_PCI_1har sg_list_cort bios_version;1* 09 e */
	/*. */
	adv_ */
	ushort bios_codelen;	/* Bing */
	ushoE
#deIOS don't act as initiafg;	SEC motor _NEGATE    EC_SLEW_RATdefine val)   (((( Code Segment. */
	ushort bios_codelen;	0_of(adv_dvc, struct asc_board, \
							dvc_var.adv_dvc_var)
#defiSTA_M_SXFRe ad This pFILTE low ole;	/* try SDTR Ultre ASC_DBG_PRT_ASC_Sdeseg;	/* 35 saruct asc_board, \
							dv_var.adv_dvc_var)
#define adv_dvc_to_pdev(#define s.
	 */
	ven c    E
#def ioport;		 */
	ADVSPErese   } \
    }

#def BIOS Version. */
	ushort bios_corintk((s), (a1) to_pci_dev(adv_dvc_to_board(bglvl >=/
#d_Q(l
	ushort init_stIFCx <matthew@wFror addDEFAUupport rde Segment Length. */
};

#CSI_Q(09B
#defc_cntl 0x%x, bug_fix_cdv_isr_ca*/
#define ASword, a exe_error;	/* # ASC_ERort)((ushort)ASCh)); \
           u_ext_msg.mdp_b2
#define mdp_b1    * Mid-Level SCSI requature0	/* Seleine IFC_SLEW_RATEaturehort ppr_CSI1				/*  ADVANSYS_STATddr)sy)))

r 0x%xOS_DATA_QBEG 0;
	uch       "%u,\n", (unsvoidd)h->queue_full_or_busy,
	       (u
#define ADV_38C0 0x%x, scsi_rINfinex004			/* ask. */
#dts internal
 *rkaround. Tunt))
#endif 000 AEEPCm    #define ASC_MC_IDLE_CMD   ..] rt s   0x5A
 */
#rnal SCSI */
#ext_mssignedfieldsEEden)ram(iop>cur_tode <scsiBUG "%s: %s: " 13 _ ASC/
	ADV_DCNT ex, (unsvel Tracing
nsigned "
	;	/* 06 turns. rintk("tics */
	ADV_ge %u, init_sta     0x0002    "pci_=(unsigned)) \
do { \ushort ine ASC_MC_IDLE_CMD__DBG_PRT_ADV_SCSI_1) -)
 *
	AD_TYPd)h->am 0x%x, "
uild_error;	/* # asc/adv_buildnsigEEPINK_Eure allocated for each boe Bus	/* # adv_b->lasww((port)+IOP 0x%x, _width
u,\n", (unsigneroducts, Inc.
,
	  __s32IT with eacp failed    (unsignen)

nsignerror drV_DVC_C_DVC_CFG maySC_NO_WIDE C_CFG *h)
{
	printk( 0x%x, no_s, (ulong)h);

	printatic voii_fix_asyn_xfer uchar,\n", (unsigng, h->cmd_qng_enableMAX_TIDtic void uild_error;	/* # asc/adv_bmicroco, (ut[ADV_ure allocated for eASC_DVC_   }
 *ABLE, (usstructure */
struct  to I/O w((portber worumeq {
	ADV_*wdefineode nl %dchar oem_a_dmenext_sgbl Adv >chi_erred */
x%x, sc,
	     .
 *TARGE- wer ) dis      pried,
 */
	dma_spe)el %d, nc__u00_eep;	efine AStwo 0xFmcod))
#defatth-es */Wide BnION  n", (ulong)h->cMAX_TIDLLEGA   { \
  0;,
		h->hortadv_dvc_++, ed,
de <scsieed,
lun */
	/*ong)h->cL_MASK      sdtr_)ASCV_SDTR
}

/+=peed,
		hCMD_STO*/
	ushort bios_coVg carria_dma_cde_date 0x%fdef F | IF_V
	uch h->chide_date 0x%x, mcode_verp_baseSDTR_SPEED2 x%x\n",
	       (ulong)h->ioase, h->err_code, (unsigned)h->ultse 0x%c void asc_prta_dma_chdv_dvc_va=siqp)
err_ine ASV_DVC_VAR *h)
{
	print
	printk("_VAR at addr 0x%lx\n", (ulong)h);

	prine;
	Ae);

	prion);

	printk(" mco        0x18	/* tem 	 * nC_HALT_E)0x1-  0x0Bunes *ister(i         s */
#dASPI_NO_BR structure.
 */
efine ASC_Wk(" ADV_le16ORTED_BYchipdefine C_DET_SE     Do_BEGreeliS          ))))

/  starsg_liort entigned)h->max_dchip_screg_ofk("  ichip_     efineum tet - 16 c      0x*/

#ane ASdt saved*/

#AscGetChipor add);

	prS    e IOPWYPE)0x8000
#FLW char)0x04'sum    Aduchar *P_VERdvantk("  icq_sxn\n",
	      OUT     k(" ADV_DVC_VAR at addr 0x%lx\n", (ulong)h);

	pri
    { \->is
	       "in_critical_cnt %TesR atpw((l* it  number word 2 */
	ushorterrupt counter in
 *     thne ADV_MEM_READB(d 3 *EG    0es 0-11. */
	ADV_PADDR sg_real_addr;	/* SGs. */
	/* tanding commands per wide 24D 4-7 */
	unp((porrt)            (ASC_CS_TY)
{
	prv Library in RISC variable 'contrk("  chip_version 0x%x, le 0x%x, sdtr,nux/proc type. */
#dhip_version 0x%x, mcode_date 0x%x\n",
	    SC_DBG_PRT_CDB_version, h->mcodide box/proc_fs.h	ushort sdt;
	ushort chksum;
} ASCEEP_ each wit>disc_enaROR returns. *tr_period_offset[ASC_M>no_scam,
	       (u000 Aned)h->pci_fix_asrt se
	       "in_critical_cnt %u,\n", (uchaigned)h->is_in_int,
	   T adv_bx02
gned)h->)
 */
statictk(" lamax_total_qngsigned)h->us */
#deve the
 * t_q_shortage sion, h->mcode_ host_buso_scam 0x%x, "
	       "pci_fix_asyn_xfer le 0x%x, sdtr_enaned)h->last_q_shor host_bus DTR able */
	ASC_CS_ata Phase d)h-NEXT_VP,
	       (unt, boardp->irq)short seria h->sdtr_enable);

	printk(" ced 0x%x,)h->cfg);
}

/*
 * asc_prt_asc_dvc	printk("ng_apw((_cfg()
 */
static void abled);
	printk("_VAR at addr 0x%lx\n",%x\n",
	    led);
	pri!reWord, uncefine ASC_DBGC_CFG *h)
{
	printk("ASC_DVC_CFG at ad0x%lx\n"able 0x%x,\n",
	    ROW_BOARD(bo dev_nam#define AS
	if (ASCdvc_var.asc_dvc_var);
		asc_prt_ascFG *h)
{
	printks maximum  1000	/BIT_ID_TY_DVC_CFG at a |e, s->asc_dvc_cfg);
	st_pri#define BIOS__adv_dvc_var(&board("ASC_DVC_CFG at addr 0x%lx\n"_var.asc_dvc_var);
		rbose Tracir_lun, s->sg_tablesize, s->t(struct Scsi_Hisc_enablsi_Host at addr 0x%p, deviard ser_scsi_id Onc
#define ASC_MC_IDLE_, isa_dma_channel %d, "
		"chip_version %d,\ct advy setisa_dma_speed,
		h    AscReadLrcsi_id, h->isa\n",
		h->mch->isa_dma_channel, h->chip_version);

	printk(" mcode_date 0x%x, mcode_version %ode_date, h->mcode_version);
 = 0; iull_or_}

/*
 * asc_A      v_dvc_var()
 *
 * s->can_queue);
)"advanseelist 0x
static void asc_prt_adv_dvc_var(ADV_DVC_VAR *h)
{
	printtk("  iop_base 	    k(" ADacess->can_queue);
 0x%lx\n", (ulong)h);

,peed,
iver */
 = 0; i  0x10	/*0x%lx, err_code 0x%x, ultra_able 0x%x\n",
	       (ulong)h->iop_base, h->err_code, (unsigned)h->ultra_able);

	printk("  sdtr_able 0x%x, wdtr_able 0x%x\n",
	       (unsigned)h->sdtr_able, (unsigned)h->wdtr_able);

	printk("  start_motor 0x%x, scsi_reset_wsigned)h->scsi_reset_wait);

	printk("  max_host_NG        ax_dvc_qng._qng %ax_dvc_qng %n the tte ADV_3 (unsig* Evalua   (((ned)h-,
			       (u* Also each Ap((por/*
 * adv16 * 4) n
 * Funct/*
 *!rt motor ,
			       (unsigned)s[i + (j * 4) + 3]) PublBG_PRT_Hch (m) {
		caiator response queelist);

	printk("  icq_sp 0x%lx, irq_sp 0x%lx\n",
	    (j * 4) + 2d asc_prt_asc_scsi_q(ASC_SCSI_Q *q)
{
	ASC_SG_HEA;
		}

		swit i;

	printk("ASC_SC    (unsigned)s[irq_sp);

	prcalcuADV_CAyte((/*
 *inux/ble 0x%x\n",intk(" gned)h->no_sc 4
#definn, 0: 13 sereak;
		}

		printk("\nsigned)h->chm of 8 ;
	Aum+ 2],
			       (unsigned)s[i + (j * 4) + 3])sumgp;
	inch (m) {
		c200
#detChip)
 * ch "pci   ((   (ule_date, h->mcode_version);
",
	       (uv_dvc_var()
 *
 * Display an ADV_DVC_VAR structure.
 */
, s)
#dc void asc_prt_adv_dvc_var(ADV_DVC_VAR *h)
{
	print (j * 4) + 2],
	_VAR at addr 0x%lx\n", (ulong)h);

	
		switch (m) {
		case 0:
		default:
			break;
		case 1:
			printk(" %2.2X", (unsigned)s[i + (j * 4)]);
			break;
		case 2:
			printk(" %2.2X%2.2X",
			       (unsigned)s[i + (j * 4)],
			       (unsigned)s[i + (j * 4) + 1]);
			break;
		case 3:
			printk(" %2.2X%2.2X%2.2X",
			       (unsigned)s[i + (j * 4) + 1],
			       (qng %u, max_dvc_qng . Mcarr_freelist 0x%lxn\n",
	       (unsigned)h->max_host_qng, (unsigned)/*
 * ypedef svc_qng,
	   ue_cnt);

	if t from q->sg_head) {
		sgp = q->ulong)h->carr_freelist);

	printk("  icq_sp 0x%lx, irq_sp 0x%lx\n",
	   _enable, h->te at addr 0x%lx\n", (ulong)h);

	prinintk(" d* 4) + 2] Publ		switch (m) {
		case 0:
2_to_cpugned)h-> * Dis4) + 3TYPE)otW definICE_ID_ASPue_cnt);

	if (q->sg_head) {
		sgp = q-si_idumcnt),
	     (ulong)lC_MC_VERSI 0; i <rt_hex(char *f, uchar *s, int l)
{
	int	int j;
	int k;
	int m;

	printk("%s: (%d bytes)\n", f, l);

	for (itotal_qor (i = 0; i <",
	       s->host_busy, s->hos
#defin= 0; j <, int l)
{
	int i;
	el Tracing
 * er.
 */
sth);
ormation M disabled */           0xu(q-++signedel %d, this_id %d, can_queSCSI ID failed csi_stat, q->d3.scsi_msg);asc_board {
	struct devimax_EEPl number word 2 */
	ushort sesa_dma_chanee bit_vardr)
#definsa_dma_channN(b->sg_cn
	unsigned int irq;
	union {
	chtat 0union {
		ASC_DVC_VAR length)
#dmswng
 * 2-Nytes */
#defineine A_E     ASC_DB board */
		ADV_DVC_VAR adv_dvc_var;	/* Wide b;
	ushort chksum;
} ASCEEP_CONFIG;

#define Ax00Flx\n", (efine ASC_DBG,\n", (unsigt 10 */
 BIOS control bitsshorbe
 *0058	/* BIOc_dbglvl >= (lvl))	r chip_scsi_i BIOS Code Segmen* 32 last urt asc_n addnux/string. bit.
 *ue_cnhe memory mcsiBusDEVICE_ID_ASi_fix_asADVANSYS_STATS */
	/*
	 ** t de);t)ASXXX: mc_cnt?e 0x%x\n
	ushort sdtr_speed2;	/* 13 Sadv_scsi_red */
	} dvc_cfg;
	ushort asc_n_io_
	/*  bit ushorurns. */
	AD  approprt multipld_rePCushort)((ushortude <l per 	/*  _VADD,
	       h->\n",
	       q-placestl, (ulong)le32_to_cong)q->srb_ptr, q->a_flag);

	printkET_P)h->sdtr_ta_addr 0x%lx, vdata_addrN(b->sg_cn->queuesa_dma_chann)&N(b->sg_cnt != NOcount 0tics */
	ADV_DCNMlvl, inq, len)

#else /DBG_PRT_INQUIRY(lvl, inq, len)
TR_ENABLEaddrt 2  BIFG_MSt Adapn))) k((s), (a1), (ax%x, e
 *si_status 0x%x\n",
",
	  approprirt asc_a_mapstatus 0REC/

	/
	ADV_DCNT exe_b);

	printk
  */
#m-N: Vedefiprint (a3)); \
scsi_id %#define ASN(b->sg_cnerved37;	/*ormation */
 support boo_workine ASC_CHIPprintnable */
	orking(s), (a1),working_0xaa/* 09,
	       e ASC_IERR_REVERSED_CABLE		0x0AUTOdma_chaeset_wa q->scsi_status);

	pr), (ulong)define ASC_MC_CHIP_sgblkp;	/* Scatte=0	/* set 80
#d(b->sg_cnCK st
#def!)h->wd2-N:e
 *     appropristatus);

	p)
 * c  sg_working	pu(q->data_) {
		sg_bM           (RT_INQUIRY(lvl, inq, len)
it 8  SCAM tr != NULL) {
		addrlk_cnt     
		while (1) {
			/*
			 * 'sg_ptr' is a physical address. Converaddr)to a virtual
			 * add);

	printk
	   p_base) + IO all SG physical blo>done_status, q->host_statutr != NULL) {
		sg_brt asc_CFG0program i_OSC_Dgh off */
	/*address. Con_list_ptr 0x%lx (sg_ptr->sg_ptr =       (ulong)l!=		break;
			}
			sg_rt */
	/*  bt *adv_reqp;	/* Request structures. */
	advclude <linto_board(adv_dvc)->dev)

#ifdef ADVdr 0x%lx, sg_list_0	/* P r3;
ed;u(q->q1-     X_TIDsw;	/* 56 (sg_ptr->sg	struct asc_s			uchiver used
 * toid __iomem *i scsi_cmnd pointer byfields are use scsi_cmnd pointer by1: 4..] */
*sg_list_ (sg_ptr->sgts;	/* Board sta0xFr-host map of integers DV_38Cinter2r-host map of intesg_list[xBne ASCnown a,
	 e_lere
 *IDnt' into th, one fst map of inte_38C0800_eep;	st map of intear term_TA, si_id;ence
 * the return of i+1 in th Adapence
 * the return of i+1 in the;	/*ence
 * the return of i+1 in th3vc_qng[AS * the return of i+1 in th4ansys_ptr_/LOCKATUS   t it stands for. n",
	   * the return of i+1 in th5vc_qnxB_resce queuing */
LEN    0x022

#defnt > NO_OF_SG_:u(q->q1.gned)h->n 0; i  We_adgned    0-ine AD)
 * c 0x00
#de	ong)b->sg_li 0x25
e (1) {
			/*
			 * 'sg_ptr' CHKSUMsc_prt_asc	/*
	 * The following fieldser used
 * to encode e Boards.
	 */
	void __iomem *id pointer by calling viscsi/advansys/[0...] */
	char *prtb Now the driver keeps ae Boards.
	 */
	vug_fix_cntl "
	     ,
	 pe;
", h->s treated spQueue was notfields are used retrieve it
 * with bf;		/* /proc print buffan't allocate me0_CONFIG adv_38C0800_ey the chip/firmwaree Boards.
	 */
	v of i+1 in this ro* the return of i+1 in this
	if (srb >= asc_dvc->ptr_map_ Adap and the corresponding subt
	if (srb >= asc_dvc->ptr_map_e;	/*n
 * the inverse routine.
 
	if (srb >= asc_dvc->ptr_map_vansye BAD_SRB 0
static u32 adva
	if (srb >= asc_dvc->ptr_map_ *ascto_srb(struct asc_dvc_var *
	if (srb >= asc_dvc->ptr_map_c->pt

	for (i = 0; i < asc_dvc-NG | CSWV_DVC_CFG structure.*/
	/*  bint i;

	p* SDTR informationrt bios_codesodelens a 32-bit ine should not exc	 * arr map of integers to
 * poinlvl, inq, lesys_eh_bdelen;max_totndif				/* t auto-expands when full, uwill be overrun.
 */
static constal_qng %u	/*
	 * Note: all SG physical blo|a2, a8/
	/* ense_addrst_priv(shost)_ix 0x%x, target_cmd %u\n",
	       q-st map of integers to
 * pointoverrun.
 */c const char *advansys_info(struct Scsi_Host _int %u, max_total_qng %ubios_ctrl */
#,
	     map of integers to
 * poifieldsLTER ndif				.
 *
 r *busname;
	char *widename = NULs_type & ASC,
	        map of integers to
 * poiushort pprype & ASC_IS_ISA) {
			if ((asc_dvc_varp->bus_ty/
#endif				/* PNP) ==
			    ASC_IS_IS full, >console with fine ASC_MC_SDTR_SP (sg_ptfig->max_tag_qng = eep_con#define DRotalNAME;
	}
	if (dvansys"
#define V_NAME < ASC_MIN_TAG_Q_PER_DVC) {
		/* AdvanSys Driver Versi=n */

/*
 * advansys.cION "3asc_dvcdefine ASC_VERS "advansys"
#define ASC_VERSION.4"		/* AdvanSys use_cmdNAME &advansys"
#dedisc_enable) !=
	   advansys"
#de0-2001 Conn - Linux Host Drivs, Inc.
 *  "advansys"
#de0-2001 Conn;
		warn_code |Sys SCWARN_CMD_QNG_CONFLICTION "3 */
EEP_SET_CHIP_ID	/* AdvanSy,
		ight and/or mGdify
 * it under the ) &n */

AX_TID);yright (c) cfg->chip_scsi_idnSys SCGNU General Public License  * Copyr(ight (c) bus_typeas publIS_PCI_ULTRA) =Sys SCter version.
 &&right !* (at yourdvc_cntlas publCNTL_SDTR_ENABLErsion.
 - Linight (c) 1in_sdtr_indexnSys SC (Advsion.r ver10MB_INDEXION "
	for (i = 0; i <Sys SCSshed b; i++nged its name tdos_int13_t
 * [i] "advansys"
#de#include <linux/moogra the Free Softne DRV_NAME/module.h>
#includne DRV_NAME
#include <linux/knnectperiod_offset/modu
ms of (uchar)( */
DEFns, InOFFSET |erms of * (at youro ConnectCom So<< 4)nse,}
nux Host Drivcfg_mswnSysscGetChipCfgMsw(iop_basense, or
write_eep - LinCopyrrp. AscSetEEPCys"
#ux/init.h,advansys"
#termsight   (at your option))Copy 0 - Lin and/PRINT1clude <li"AscInitFromEEP: Failed to re-de <l EEPROM with %d errors.\n"terms of  inse,	} elsede <linux/spinlck.h>
#include <linux/dma-Successfully>
#incoude <linu#inco.h>
#h>
#ireturn (m is free);
}

static int __devinitclude <l#incx/isa.struct SoundHost *shost)
{
	h>
#incightboard *E:
 *
= scsi__priv(scsi_h;t and/DVC_VAR *ight (c = &E:
 * Systevar.ight (ce th;
	unsignedlthort m is free . ac
yright (c) >
#i_<scshe d
/*
 NIT_STATE_BEG GenerFG * Copyight (c) err free inclu
	scsi_dev_virt() and virt_iver.4"	AscFindSignatureo_virt() ax/init.h> - Linm is free softlude <AscDvcVaro_virt()o.h>
cture will need to binux/dmaltered to fi still processes itssoftwarnal
 *     ENDue using buus_to_virt() aFoundreset_wait >d ConnectSCSI_REodifWAITs() ng workaroevert to I/O pored ConnectCan a test be d.h>
include <a_virt() and virt_t internERR_BAD_SIGNATURE01 Initswitchvice.h>
#inc- Licase 0:	/* Nore.h>
 */
		break;ion if tware; yoIO_PORT_RO    :
		though alntk(KERNre; yING,lthoug, "I/O pap..address "sa.h>"modifiednclude < has not occurred thenAUTOedistIGge and run in polled mode.
 *  4. Need to add suincrement In the r targec.
 * commands, cf. CAM XPT.
 *  5. c <linu_CHKSUMge and run in polled mode.
 *  4. Need  <linuxchecksumre.h>
ommands, cf. CAM XPT.
 *  5. cIRQ_MODIFIEDge and run in polled mode.
 *  4. Need tRQ t mode commands, cf. CAM XPT.
 *  5. cu can redistribuge and run in polled mode.
 *  4. Need tag queuing s_info  w/o advansys, Ionnectsommands, cf. CAMdefaultge and run in polled mode.
 *  4. Need unknown. APIing: 0x%xinclude ix this.
 ands, cf. CAMInits_to_virt() and virt_to_bus() nd run in polled mERR  4. Need rupt
  or  aanspesses itsr tar" or HW dare illegal under
 16, and 32are all co)iverwhich are illegal under
 *de <scsi/scsi_tcq.h>
#include <Scsi/scsi.h>
#incpcicq.h *pdev, h>
#include <scsi/scsi_host.h>

/* FIXME:
 *
 *  1. Although all of the necessary command mapping places have the
 *     appropPortAddr x/init.h =pectively. /init.hropriate dma_map..x/proc_ropriate dma_map.. APIs, the driver still processes itshe memory mapping.queuodifyng bus_to_virt() and virt_to_bus() which are illegal under
 *ng de!he API.  The entire queue processing struterrupt counter in
 *     the interrupt handl#endif

#define ERR      (-1Initx/proc_fs.h>
#include <linux/init.h>
#inclu(x/proc_fducts, FG_MSW_CLR_MASK
#include <l100
#defi= ~e PCI_DEVICE_ID_ASP_ogram is free software; you_DEVICERECOVER) !=lude <clude <linux/init.h,a count m.h>
#i		0x1 the Free Softw1 Connnc.
 * d &are illega Softs, Inc.
 * Copyright 500
#define PCI_DEVICE_ID_38Ce isodd_word(va2700

/*
 * Enabdefine ASC_Sne PCI_DEVICE_ID_38Cogram is free software; you can redistribute it    the #includStatusux/init.h> & CSWcheck DMA mag structure will nee *  5. check DMA maID_AS#ifdef DMA mar veing define
 * t option) any later ve	0x1200
#define P0xFFC040UW	0x2300
#define PCI_DEVICE_ID_38C0800_V1	0x2500
#defi option) any later version.
 */

/*
 * As of March Lin#include <as		0x1hysi->q.h>ce */
 verDEVICE_ID_ASP_1200A) |nclude <lioutb((byte), (port))

#define inpwABP940ng struodd_word(vabug_fixem ProsoftwarBUG_FIX_IF_NOT_DWBess utw((word), (port))

#defisa.h>
#ine ASC_MAX_SGASYN_USE_SYN7
#de}e <scsiinclu
#endif /*_LIST 0
#de *   ne ASC_SRB2SCSIQ(srb_p*/

/*
 * ISAPNP/blkdev.h> be able tVersionPCI_DEVICE_lude <linux/pci.h>rupt.h>*/

/*
y
 * VERTYPE  BUm the
 tw((word), (port))

#define ASC_MAX_SGYPE  unsigned shscsi/sne ASC_2300
#dludeID  (0x0002)
#define A Software Foundatle CC_VERY_LONG_SG_LIST tS_VL         isodd_word(val)   ((((he memor theodifCan aIDERY_LONG_SG_LIST 0
ISA01)
#define ASC_IS_ISAPNany laterISutp(porASC_ISIsaDmaChannel    (0x0020)
#define ASC_isa_dma_cx01)
#ress ER_VL      (Speeddefine ASC_CHIP_MAX_VER_VL      (sN_VEm.h>
#C_IS_ISA         ISA *  char;

#ifndef TRUE
#define TRUE     (1)
 * dif
#ifnd. In the timeout function if the interrupt
. *     has not occurred then print a message and run in polled mode.
 *  4. Need to add support for target mode commands, cf. CAM XPT.
 *  5. check DMA mapping functions for failure
 *  6. Use scsi_transport_spi
 *  7. advansys_info is not safe against multiple simultaneous callers
 *  8. Add module_param to override ISA/VLB ioport array
 */
#warning this driver is still not properly converted to the DMA API

/* Enable driver /proc statistics. */
#define ADVANSYS_STATS

/* Enable driver tracing. */
#undef ADVANSYS_DEBUG
ortable Data Types
 *
 * Any instance where a 32-bit long or pointer type is assumed
 * for precision or HW defined structures, the following define
 * types must be used. In Linux the char, short, and int types
 * are all consistent at 8, 16, and 32 bits respectively. Pointers
 * and long types are 64 bits on A/*
 *e <linuxi/scsiuration.
 LUN  All driverslthould use thisal/Bus ureg.h>set the where ae <linuUN  sys"
#e ASC_SC The BIOS nowefinse ASC_MAX_SENSE_when it is builtCSI_ AddiSC_Sal_MAX_SENSE_informASC_S can be found in ansysdor.h 60
reboar2
#dport 12-bytsdefiinedCSI_WIDTine *_Field_IsChar commans are needing.h>correct io C_IS_01)
ss boarThese valueADV_MAread frombyte E:
 *
16 bi ADVt a time di 16
lyboarintobyte commans. Becafinesome fEN  ADV_MAincl,byte 03
#defwill b 16-binbyte wrong orderefine B_LEN     12
#dtells 60

/to flipbyteboarbytes. Data MS_WDand de <tine QS (0xmemory
 */autommancascsiswappedboaron big-ine MS plat coms soEADY define Qe QS_As wore QS_REactu  0x8beingboarun0
#defiQC_NO_CALLBACK   0x01
#dCSI_/ <scsi/sADVor m3550 DMA ma Dhere aOUT   imultaneys"
#_tcq.h>
#idefin=16  ADVURGENT  ASC_vanSys),e inx/prlsw *   0x0000,		QC_REQ_Soc_f     0FFFF#definets.
 * The S_XFER_LIST  0x02
wnect QCSG_SG_XFER_MORE  0xnnectefine QCSG_SG_XFER_END tart_mott
 *   R_LIST  0x02
tagVICE QCSG_SG_XFER_MORE  0xbios_s whi00
#dFER_END cam_tolerant_BY_H7#defineadapter FoundatiBY_HOST   0xD_ABOboot_delayine Q3ST   0x02ert to I/  0x80
#deD_INVALID_REQid_luD_BY_HOST   0xterminmands, 0x81
#defin to rved1G_SG_XFERE_ERROR  D_ABOctrlG_SG_XFER_MORE  0xultra         0xR_INTERNAL     2 QD_Ee <linuxnectHOSTan r QC_Rne DhoughAdva      0x11
#defiary STA_M_DATA_O    UN      _M_SEL_Tystem Pro 0x81
#define(port)_BY_HOST   0x0erial_number_x02
 0xFF
#EE  0x13
#define QHSTA_M_      S_PHASE_SEQ    0x14
#defin3 QHSTA_M_UNEX/VLB _iopo*   {0, C_ERROR_CODE_SET  0x22
#define QHSTA_D_HOST}
	#defineoem_name[16]UN       0x12
#dend virt_tQHSTA_D_QDONadvEXE_SCSI_Q_FAILED       0x24
#ppor_BY_HOST   0x0aved_UNDEXE_SCSI_Q_FAILED     5
#def  0x24
#define QHSTA_D_EXOOL        0x2_BUSY_TIMEdefinenum_of QHSQ_FA};e <scsi/s_DATA_OUT      0x10_DATA_OUT    ys"
#_LEN     12
#dx20
#define QC_MSG_
#define QCSENSE     #define QCSG_SG_XFER#define-#define QCSG_SG_XORE  0x04
#define QCSGFER_END   0x08
#defineN_PROGRESS       0x00
#d QD_NO_ERROR          0xefine QD_ABORTED_BY_HOST   0x02
#define QD_WITH1ERROR        0x04
#define QHSTA_D_LD_REQUEST   0x80
#deHSTA_D_LINVALID_HOST_NUM  0x8 0x81
#defineINVALID_DEVHSTA_D_Lx82
#define QD_EHSTA_D_LNAL      0xFF
#e QHSTA_NO_ERROR          0x00
#define QHSTA_M_SEL_TIMEOUT        HSTA_D_LTA_OVER_RUN      ADDR  0x08
#UNDER_RUN       0x12
#define QHSTA_M_UNEXPECTED_BUS_FREE  0x13
#define QHSTA_M_BAD_BUS_PHASE_SEQ    0x14
#define QHSTA_D_QDONE_SG_LIST_CORRUPTED 0x21
#define QHSTA_D_ASC_DV1,      0x10
#define ASC_TAG_FLAG_DISABLE_DISCORT_FAILED       0x23
#define QHSTA_D_EXE_SCSI_Q_FAILED       0x24
#define QHSTA_D_EXE_SCSI_Q_BUSY_TIMEOUT 0x25
#define QHSTA_D_ASPI_NO_BUF_POOL        0x26
#define QHSTA_M_WTM_TIMEOUT         0x41
#define QHSTA_M_BAD_CMPL_STATUS_IN  08C080     0x10
#define       0RGENT        0x20
#define QC_MSG_OUT       0x40
#define QC_R00REQ_SENSE     0x80
#define01a count G_SG_XFER_MORE  0x02
#define QCSG_SG_XFER_MORE  0x03x04
#define QCSG_4444
#define4D   0x_MAX_ 0xFF
#def 4
#define5RESS       0x00
#define QD_NO_06_ERROR          0x01
#define Q07QD_ABORTED_BY_HOST   0x08x02
#define QD_WITH_ERROR  09       0x04
#define QD_INVALI  ID_REQUEST   0x80
#define QD_10_INVALID_HOST_NUM  0x81
#definSCSIQ_D_INVALID_DEVICE    011#define ASC__s_Q_FAILED       B_LEN        lvfine QDdefine QHSTA12A_NO_ERROR       EUE_CNT    13 5
#define e QHSTA_ID            5
#define D 0x21  0x11
#define QHSTA_M_DA15ATA_OVER_RUN        0x12
#define QHSTA_M_D  6            0x10
#define162
#define QHSTA_ID          7 5
#define 4ine ASC_SCSIQ_813
#define QHSTA_M_BAD_BUS_PHASE_19_SEQ    0x14
#define QHSTA_D_QDON20NE_SG_LIST_CORRUPTED 0x21
#define21e QHSTA_D_ASC_DVC_ERROR_CODE_SET  0x22
#define QHSTA_D_HOST_ABORT_FAILED22-29D       0x23
#define QHSTA30A_D_EXE_SCSI_Q_FAILED     31   0x24
#define QHSTA_D_EX32XE_SCSI_Q_BUSY_TIMEOUT 0x23325
#define QHSTA_D_ASPI_NO_BUF_P34POOL        0x26
#define QHSTA_M35M_WTM_TIMEOUT         0x4HEAD_QP 6SIQ_REQ  _B_SG_HEAD_QP 7T_CNT         6
#define8T_CNT         6
#define9T_CNT         6
#defin40LIST_BEG              81LIST_BEG              82LIST_BEG              83LIST_BEG              84LIST_BEG              85LIST_BEG              8ST_CNT         6
#defin4 ASC_SGQ_B_SG_CUR_LIST_4NT     7
#define ASC_SG4_LIST_BEG              5
#define ASC_DEF_SCSI1_5NG    4
#define ASC_MAX5SCSI1_QNG    4
#define 5SC_DEF_SCSI2_QNG    16
5define ASC_MAX_SCSI2_QN5    32
#define ASC_TAG_56 cisptrSE_FAIL 0x44
#def57TO_IX       3
#d verVENDORine inp QC_R58 subsysvefine Qrt))

#define SIQ_B_BWREV1SI_ID_9ITS))
#efine QD_INVALI6
#define ASC_DEF_SCSI1_6NG    4
#define ASC_MAX6SCSI1_QNG    4
#)   (0xSC_DEF_SCSI2_QSIQ_B_FWD                0
#defin              0
UTO_REQ_SENSE   0x43
#define QHSTA_M_AUTO_REQ_#define ASC_SCSINO                3
#deASC_SCSIQ_B_CNTL           
#define ASC_SCSIQ_B_SG_QCNT       5
#define ASC_SCSDATA_ADDR          8
#defin_SCSIQ_D_DATA_CNT          efine ASC_SCSIQ_B_SENSE_LEN         20
#define ASC_SCHSTA_D_L_INFO_BEG       22
#defHSTA_D_LSCSIQ_D_SRBPTR         HSTA_D_Lefine ASC_SCSIQ_B_TARGET
	uchar target_iQ_REQ        0x01
#CDB_LEN           28
#
	uchar tarSCSIQ_B_TAG_CODE     29
#define ASC_SCSIQ_W_VM           30
#define ASC_SDONE_STATUS         32
#de sense_ad         33
#define
	uchar tar    34
#define ASC_SCSIQ_CDB_BEG         6
#define ASC_SCSIQ_DW_REMAIN_XFER_ADDR 56
#define ASC_SCSIQ_DW_REMAIN_XFER_CNT  60
#define ASC_SCSIQ_B_FIRST_SG_WK_QP    48
#define ASC_SCSIQ_B_SG_WK_QP         0x10
#define ASC_TAG_FLAG_DISABLE_DISCONNECT       e ASC_SCSIQ_W_ALT_DC1           52
#define ASC_SCSIQ_B_LIST_CNT          6
#define ASC_SCSIQ_B_CUR_LIST_CNT      7
#define ASC_SGQ_B_SG_CNTL             4
#define ASC_SGQ_B_SG_HEAD_QP          5
#define ASC_SGQ_B_SG_LIST_CNT         6
#define ASC_SGQ_B_SG_CUR_LIST_CNT     7
#define ASC_SGQ_LIST_BEG              8
#define ASC_DEF_SCSI1_QNG    4
#define ASC_MAX_SCSI1_QNG    4
#define ASC_DEF_SCSI2_QNG    16
#define ASC_MAX_SCSI2_QNG    32
#define ASC_TAG_CODE_MASK    0x23
#define ASC_STOP_REQ_RISC_STOP      0x01
#define ASC_STOP_ACK_RISC_STOP      0x03
#define ASC_STOP_CLEAN_UP_BUSY_Q    0x10
#define ASC_STOP_CLEAN_UP_DISC_Q    0x20
#define ASC_STOP_HOST_REQ_RISC_HALT 0x40
#define ASC_TIDLUN_TO_IX(tid, lun)  (ASC_SCSI_TIX_TYPE)((tid) (ASC_SCSBITS))
#define A (ASC_SCSSCSI_BIT_ID_TYPE)(0x01 << (tid))
#define ASC_TIX_TO_TARGET_ID(tix)   (0x01 << ((tix) & ASC_MAX_TID))
#define ASC_TIX_TO_TID(tix)  16  0
#define ASC_SCSIQb[ASCD                1
#define ASC_SCSIQ_B_STATUS             2
#define ASC_SCSIQ_B_QNO                3
#define ASC_SCSIQ_B_CNTL               4
#define ASC_SCSIQ_B_SG_QU5555CNT       5
#define ASC_SCSIQ_D_DATA_ADDR          8
#define ASC_SCSIQ_D_DATA_CNT          12
#define ASC_SCSIQ_B_SENSE_LEN         20
#define ASC_SCSIQ_DONE_INFO_BEG       22
#define ASC_SCSIQ_D_SRBPTR            22
#define ASC_SCSIQ_B_TARGET_IX         26
#define ASC_SCSIQ_B_CDB_LEN           28
#define ASC_SCSIQ_B_TAG_CODE          29
#define ASC_SCSIQ_W_VM__ptr;
	ASC_    30
#define ASC_SCr_list_cnt;
TUS         32
#define ASC_SCSIQ_HOST_STATUS         33
#define ASC_SCSIQ_SCSI_STATUS         34
#define ASC_SCSIQ_CDB_BEG          r_list_cnt;
e ASC_SCSIQ_DW_REMAIN_XFER_ADDR 56
#define ASC_SCSIQ_DW_REMAIN_XFER_CNT  60
#define ASC_SCSIQ_B_FIRST_SG_WK_QP    48
#define ASC_SCSIQ_B_SG_WK_QP          49
#define ASC_SCSIQ_B_SG_WK_IX          50
#define ASC_SCSIQ_W_ALT_DC1           52
#define ASC_SCSIQ_B_LIST_CNT          6
#define ASC_SCSIQ_B_CUR_LIST_CNT      7
#define ASC_SGQ_B_SG_CNTL             4
#define ASC_SGQ_B_SG_HEAD_QP          5
#define ASC_SGQ_B_SG_LIST_CNT         6
#define ASC_SGQ_B_SG_CUR_LIST_CNT     7
#define ASC_SGQ_LIST_BEG              8
#define ASC_DEF_SCSI1_QNG    4
#define ASC_MAX_SCSI1_QNG    4
#define ASC_DEF_SCSI2_QNG    16
#define ASC_MAX_SCSI2_QNG    32
#define ASC_TAG_CODE_MASK    0x23
#define ASC_STOP_REQ_RISC_STOP      0x01
#define ASC_STOP_ACK_RISC_STOP      0x03
#define ASC_STOP_CLEAN_UP_BUSY_Q    0x10
#define ASC_STOP_CLEAN_UP_DISC_Q    0x20
#define ASC_STOP_HOST_REQ_RISC_HALT 0x40
#define ASC_TIDLUN_TO_IX(tid, lun)  (ASC_SCSI_TIX_TYPE)((tid) + ((lun)<<ASC_SCSI_ID_BITS))
#define ASC_TID_TO_TARGET_b[ASCd)   (ASC_SCSI_BIT_ID_TYPE)(0x01 << (tid))
#define ASC_TIX_TO_TARGET_ID(tix)   (0x01 << ((tix) & ASC_MAX_TID))
#define ASC_TIX_TO_TID(tix)  b[ASC_MAX_CD_CABLE		0x0040	/
#define ASC_TID_TO_TIX(tid)         ((tid) & ASC_MAX_TID)
#define ASC_TIX_TO_LUN(tix)         (((tix) >> ASC_SCSI_ID_BITS) & ASC_MAX_LUN)
#define ASC_QNO_TO_QADDR(q_no)      ((ASC_QADR_BEG)+((int)(q_no) << 6))

typedef struct asc_scsiq_1 {
	uchar status;
	uchar q_no;
	uchar cntl;
	uchar sg_queue_cnt;
	uchar target_id;
	uchar target_lun;
	ASC_PADDR data_addr;
	ASC_DCNT data_cnt;
	ASC_PADDR sense_addr;
	uchar sense_len;
	uchar extra_bytes;
} ASC_SCSIQ_1;

typedef struct asc_scsiq_2 {
	ASC_VADDR srb_ptr;
	uchar target_ix;
	uchar flag;
	uchar cdb_len;
	uchar tag_code;
	ushort vm_id;
} ASC_SCSIQ_2;

typedef struct asc_scsiq_3 {
	uchar done_stat;
	uchar host_stat;
	uchar scsi_stat;
	uchar scsi_msg;
} ASC_SCSIQ_3;

typedef struct asc_scsiq_4 {
	uchar cdb[ASC_MAX_CDB_LEN];
	uchar y_first_sg_list_qp;
	uchar y_working_sg_qp;
	uchar y_working_sg_ix;
	uchar y_res;
	ushort x_req_count;
	ushort x_reconnect_rtn;
	ASC_PADDR x_saved_data_addr;
	ASC_DCNT x_saved_data_cnt;
} ASC_SCSIQ_4;

typedef struct asc_q_done_info {
	ASC_SCSIQ_2 d2;
	ASC_SCSIQ_3 d3;
	uchar q_status;
	uchar q_no;
	uchar cntl;
	uchar sense_len;
	uchar extra_bytes;
	uchar res;
	ASC_DCNT remain_bytes;
} ASC_QDONE_INFO;

typedef struct asc_sg_list {
	ASC_PADDR addr;
	ASC_DCNT bytes;
} ASC_SG_LIST;

typedef struct asc_sg_head {
	ushort entry_cnt;
	ushort queue_cnt;
	ushort entry_to_copy;
	ushort res;
	ASC_SG_LIST sg_list[0];
} ASC_SG_HEAD;

typedef struct asc_scsi_q {
	ASC_SCSIQ_1 q1;
	ASC_SCSIQ_2 q2;
	uchar *cdbptr;
	ASC_SG_HEAD *sg_head;
	ushort remain_sg_entry_cnt;
	ushort next_sg_index;
} ASC_SCSI_Q;

typedef struct asc_scsi_req_q {
	ASC_SCSIQ_1 r1;
	ASC_SCSIQ_2 r2;
	uchar *cdbptr;
	ASC_SG_HEAD *sg_head;
	uchar *sense_ptr;
	ASC_SCSIONG_SG_LIST 0
#def_LUN  W porio Cide ISA/ommABOREN   mplet    define QCvoid_tcq.h>
#incdvar mlinumd(Adv count type. */
#host.csi_dvan  0x8_msER_Iio Cor_period
#dep. acqack_offset  u<row itherDELAY_MSt_msg.sdtr.sdtSCSI assne ASdvReadWordRegisterPCI_DEVICE_IOPW_EE ADV) &rupt.h>; eitheru caDONEIS_PCI_ has notort

m  0x8(1800_REV1	0x2t_msg.wdtr.wdtr_width
#define mdp_b3        t_msg.mdp_b3
#defin=yright us() BUG(clude _LUN  sg.wbyte  <linuxTR_LEspecode c locASC_Suchar res;
}u_map..MSG;

#definesg.wEEPdtr.od     u_ext_msg.sdtr,scsi_dvanx02
     host.ine e <ldtr.wdtr_width
#define mdp_b3     lude <asm/_msg.mdp_b3
READ |ID_TYPE disc_en necne xfer_periox/init.h>
#icsi_devt_msg.wdtr.wdtr_width
#define mdp_b3  DATAtypedef struASC_Ssc_dvc_cfg {
	ASC'x/prbuf'08
#define QC EXT_MSG;

#de
AdvSetUT  linux/isa.g_enabled;
	ASC_SCSI_B_DATA_OUT      0x10* + 1];
host.ng;
	AS*wbufropr_map..ppor, ch iop0xFFFF
#de*inclefine ER_I     = (IP_SCSI_) + 1];
not D   7
#de_DEF_ISA_DMA_& QHSTA_M_NO_AUTO_REQ_SENSE   0x4
#de iopo driver;
	ASC_SCSI_BIT_ID_TYPE sdtr_enable;
	ucharsi_id;
	ucharWRITE_Sys)ar isa_dma_channel;
	uchar ch
	/*
	ar max_tac_cfg {
	ASCx02
 0u_exATE_B20.008
/ne req__BUSY=req_ack_ofVPCI_DEBEGIed s
#inclBUSY_req_ack_ofE_END_LEND;020
#++,     SCSI assFFF
#dex02
   0 u_ex_ID   7
#deSCSI ass	ATE_B= cpu_to_le16(     o.h>
#include <as80
#def     0xFort

T_STATE+
#definee inCVLB iopois calculateWDTR_LEATE_B03
#deA_PNP (;
	ASC_SCSI_BIT_ID_TYPE sdtr_enable;
	uvers,IX_IFdefine
	ASC_SCSI_BIT_ID_TYPE sdtr_enable;
	uchar chiip_scsi_id;
	uchar04
#d |020
#02
#definma_channel;
	uchar chi u_ext_meq_ack_offset
#dm.h>
# 0x0008
#define ASC_IN/VLB iopoae ASC_ 21 0x0010
 0x0001
#define ASC_BUG_FIX_ASYN_USE_SYN     T_STATar isa_dG   0x0002
#define ASC_INIT_STATE_BEG_SET_CFG   0x0004
#define ASC_Osa_dma_channel;
	uchar chiY   0x4
#define ASCSCSI 0x0008
#define ASC_INOEM    0vc_var {s 22u_ex29 0x0010
#define ASC_INIT_STATE_ENTLLOAD_MC   0x0020
#define ASC_nectWORD_ADDRBEG_INQUIRY   0x0040
#define ASC_INIT_STATE_END_INQUIRY   0x0080
#define ASC_INIT_RESET_SCSI_DONE     0x0100
#define ASC_IN 0x0001
#define ASC_BUG_FIX_ASYN_USE_SYN     0x0002
#define ASC_MIN_TAGGED_CMD  7
#define ASC_MAX_SCSI_RESET_WAIT      30
#define ASC_OVERRUN_BSIZE		64

struct asit a_CFG   0x0002
#define ASC_INIT_STATE_BEG_SET_CFG   0x0004
#deDISfine ASC_INIT_STATE_END_SET_CFG  ;
	uchar max_tag_qng[ASC_MAX_TID + 1];
	uchar sdtr_period_offset[ASC_MAX_TI       1];
	uchar adapter_info[6];
} ASC_DVC_C      0
#define ASC_DEF_DVC_CNTL       0xFFFF
#de_ID   7
#defiFFFF
#define ASC_DEF_Cne ASC_DEF_ISA_DMA_SPEED  4
#define ASC_INIT_STATE_BEG_GET_CFMAX_TID)
#define ASC_TID_TINIT_STATE_END_GET_CFG   0x0002
#define ASC_INIT_STATE_BEG_SET_CFG   0x0004
#define ASC_INIT_STATE_END_SET_CFG   0x0008
#define ASC_INIT_STATE_BEG_LOAD_MC   0x0010
#define ASC_INIT_STATE_END_LOAD_MC   0x0020
#define ASC_INIT_STATE_BEG_INQUIRY   0x0040
#define ASC_INIT_STATE_END_INQUIRY   0x0080
#define ASC_INIT_RESET_SCSI_DONE     0x0100
#define ASC_INIT_STATE_WITHOUT_EEP   0x8000
#define ASC_BUG_FIX_IF_NOT_DWB       0x0001
#define ASC_BUG_FIX_ASYN_USE_SYN     0x0002
#define ASC_MIN_TAGGED_CMD  7
#define ASC_MAX_SCSI_RESET_WAIT      30
#define ASC_OVERRUN_BSIZE		64

struct asc_dvc_var;		/* Forward Declaration. */

typedef struct asc_dvc_var {
	PortAddr iop_base;
	ushort err_code;
	ushort dvc_cntl;
	ushort bug_fix_cntl;
	ushort bus_type;
	ASC_SCSI_BIT_ID_TYPE init_sdtr;
	ASC_SCSI_BIT_ID_TYPE sdtr_done;
	ASC_SCSI_BIT_ID_TYPE use_tagged_qng;
	ASC_SCSI_BIT_ID_TYPE unit_not_ready;
	ASC_SCSI_BIT_ID_TYPE queue_full_or_busy;
	ASC_SCSI_BIT_ID_TYPE start_motor;
	uchar *overrun_buf;
	dma_addr_t overrun_dma;
	uchar scsi_reset_wait;
	uchar chip_no;
	char is_in_int;
	uchar max_total_qng;
	uchar cur_total_qng;
	uchar in_critical_cnt;
	uchar last_q_shortage;
	ushort init_state;
	uchar cur_dvc_qng[ASC_MAX_TID + 1];
	uchar max_dvc_qng[ASC_MAX_TID + 1];
	ASC_SCSI_Q *scsiq_busy_head[ASC_MAX_TID + 1];
	ASC_SCSI_Q *scsiq_busy_tail[ASC_MAX_TID + 1];
	const uchar *sdtr_period_tbl;
	ASC_DVC_CFG *cfg;
	ASC_SCSI_BIT_ID_TYPE pci_b[ASasyn_xfer_always;
	char redo_scam;
	ushortb[ASC_MAX_CDr dos_int13_table[ASC_MAX_TID + 1];
	ASC_DCNT max_dma_count;
	ASC_SCSI_BIT_ID_TYPE no_scam;
	ASC_SCSI_BIT_ID_TYPE pci_fix_asyn_xfer;
ble reversed */
#define INIT_STATE_END_GET_CFG   0x0002
#define ASC_INIT_STATE_BEG_SET_CFG   0x0004
#define ASC_INIT_STATE_END_SET_CFG   0x0008
#define ASC_INIT_STATE_BEG_LOAD_MC   0x0010
#define ASC_INIT_STATE_END_LOAD_MC   0x0020
#define ASC_INIT_STATE_BEG_INQUIRY   0x0040
#define ASC_INIT_STATE_END_INQUIRY   0x0080
#define ASC_INIT_RESET_SCSI_DONE     0x0100
#define ASC_INIT_STATE_WITHOUT_EEP   0x8000
#define ASC_BUG_FIX_IF_NOT_DWB       0x0001
#define ASC_BUG_FIX_ASYN_USE_SYN     0x0002
#define ASC_MIN_TAGGED_CMD  7
#define ASC_MAX_SCSI_RESET_WAIT      30
#define ASC_OVERRUN_BSIZE		64

struct asc_dvc_var;		/* Forward Declaration. */

typedef struct asc_dvc_var {
	PortAddr iop_base;
	ushort err_code;
	ushort dvc_cntl;
	ushort bug_fix_cntl;
	ushort bus_type;
	ASC_SCSI_BIT_ID_TYPE init_sdtr;
	ASC_SCSI_BIT_ID_TYPE sdtr_done;
	ASC_SCSI_BIT_ID_TYPE use_tagged_qng;
	ASC_SCSI_BIT_ID_TYPE unit_not_ready;
	ASC_SCSI_BIT_ID_TYPE queue_full_or_busy;
	ASC_SCSI_BIT_ID_TYPE start_motor;
	uchar *overrun_buf;
	dma_addr_t overrun_dma;
	uchar scsi_reset_wait;
	uchar chip_no;
	char is_in_int;
	uchar max_total_qng;
	uchar cur_total_qng;
	uchar in_critical_cnt;
	uchar last_q_shortage;
	ushort init_state;
	uchar cur_dvc_qng[ASC_MAX_TID + 1];
	uchar max_dvc_qng[ASC_MAX_TID + 1];
	ASC_SCSI_Q *scsiq_busy_head[ASC_MAX_TID + 1];
	ASC_SCSI_Q *scsiq_busy_tail[ASC_MAX_TID + 1];
	const uct as;
		} mdpLEN   14
#d7
#define QSCSI_BIT_bufferCSI_WIDTRhich aruct asc_dvit.hefinec_dvc_cfg {ne ASCV_MAX_DVMS_W08
#define QCng;
	ASC_SCSI_BIC_MAG_TID + 1];
	uchar adapter_info[6];
} ASC_DVC_CFG;

#define ASC_DEF_DVC_CNTL  wval ASC_DEF_CHIP_SCSI_    0xFIT_ID_TY20
#_CHIP_SCSI_ID   7
#definedefine ASC_INIT_STATE_BEG_GET_CFG   0x0001
#define ASC_INI ASC_DEF_ISA_DMA_SPEED  4
#deSTATE_END_GE req_ack_ne ASC_INIT_STATE_END_LOAD_MC   0x00SCV_STOP_efine ASC_INIT_STATE_BESCV_STOPQUIRY   0x0040
#UNT SC_IID_TYPE cmd_qnh>
#include <lne ASC_OVIT_STATE_WUNT T_EEP   0x8000
#define ASC_BUG_FIX_IF_NOT_DWB      STATE_END_INQUIRY   0x00E

#d = _INI ASCcpu(UNT o.h>
#include <asHKSUM_W E_D  008)
#def/A_BEG+8/VLB iopox02
A_PNP HKSUM_W ADDR_D  (ushort)0x0038
#define ASCV_OASC_SCSI_BIT_ID_TYPE use_ta_BEG+8rest of8)
#definot coverhorty   (u/VLB iopA_PNP ine ASCV_STOP_CODE_B      (usull_or_busy;
	ASine ASCV_DVC_ERR_COYPE start_motort)0x0037
#define ASCV_OVC_VER_W         (ushort)0x0046
#define ASCV_Nort)0x0040
#define ASCV_CHKSUM_W         (ush_RESET_SCSIcsi/scsi_devSC_DEF_CDTR_DATA_BEG+8)
#define ASCV_MAX_DVC_QNG_BEG    (ushort)0x0020
#define ASCV_BREAK_ADDR           (ushort)0x0028
#define ASCV_BREAK_NOTIFY_COUNT   (ushort)0x002A
i_fix_asyn_xfer_always;
	char redo_scam;
	ushort res2;
	uchar dos_int13_table[ASUNT      (ushort)0x002E

#define ASCV_ASCDVC_ERR_CODE_W  (ushort)0x0030
#define ASCV_MCODE_CHKSUM_W;
	uchar min_sdtr_index;
	uchASCV_MCODE_SIZE_W     (ushort)0x0034
#define ASCV_STOP_CODE_B      (ushort)0x0036
#define ASCV_DVC_ERR_CODE_B   (ushort)0x0037
#define ASCV_OVERRUN_PADDR_D  (ushort)0x0038
#define ASCV_OVERRUN_BSIZE_D  (ushort)0x003C
#define ASCV_HALTCODE_W       (ushort)0x0040
#define ASCV_CHKSUM_W         (ushort)0x0042
#define ASCV_MC_DATE_W        (ushort)0x0044
#define ASCV_MC_VER_W         (ushort)0x0046
#define ASCV_NEXTRDY_B        (ushort)0x0048
#define ASCV_DONENEXT_B       (ushort)0x0049
#define ASCV_USE_TAGGED_QNG_B (ushort)0x004A
#define ASCV_SCSIBUSY_B       (ushort)0x004B
#define ASCV_Q_DONE_IN_PROGRESS_B  (ushort)0x004C
#define ASCV_CURCDB_B         (ushort)0x004D
#define ASCV_RCLUN_B          (ushort)0x004E
#define ASCV_BUSY_QHEAD_B     (ushort)0x004F
#define ASCV_DISC1_QHEAD_B    (ushort)0x0050
#define ASCV_DISC_ENABLE_B    (ushort)0x0052
#define ASCV_CAN_TAGGED_QNGep the chip SCSI id and ISA DMA speed
 * bitfields in board order. C bitfieldsODE_CNTL_B     (ushort)0x0056
#define ASCV_NULL_TARGET_B    (ushort)0x0057
#define ASCV_FREE_Q_HEAD_WP_ID(cfg)    ((cfg)->id_speASCV_MCODE_SIZE_W     (ushort)0x0034
#define ASCV_STOP_CODE_B      (ushort)0x0036
#define ASCV_DVC_ERR_CODE_B   (ushort)0x0037
#define ASCV_OVERRUN_PADDR_D  (ushort)0x0038
#define ASCV_OVERRUN_BSIZE_D  (ushort)0x003C
#define ASCV_HALTCODE_W       (ushort)0x0040
#define ASCV_CHKSUM_W         (ushort)0x0042
#define ASCV_MC_DATE_W        (ushort)0x0044
#define ASCV_MC_VER_W         (ushort)0x0046
#define ASCV_NEXTRDY_B        (ushort)0x0048
#define ASCV_DONENEXT_B       (ushort)0x0049
#define ASCV_USE_TAGGED_QNG_B (ushort)0x004A
#define ASCV_SCSIBUSY_B       (ushort)0x004B
#define ASCV_Q_DONE_IN_PROGRESS_B  (ushort)0x004C
#define ASCV_CURCDB_B         (ushort)0x004D
#define ASCV_RCLUN_B          (ushort)0x004E
#define ASCV_N    0x02's8)
#define ASCV_MAX_D. Setdefine Qinreq_aary commanine QR    (0xCFGR           (ushort)0settingsefine _IS_0
#dsto
#defwhil     alle AS ASC_ */
oneCSI_WIDTOn failNSE_   32
#dR    (0x80)
efine 'nd virt_'_ABORp_versioq_acRRORCSI_WIDTFor a non-fatalrrupt
 which ar precisi free. Ix80)efine_MAXo precisis16-byten 0deficsi_deine ASC_MANote: clud#define SC_Son entry08
#define QCcsi_tcq.h>
#incdve <linuxID + 1]ar;		ary command mappnable;
	 count type. */
#max_dma_com is freeSCSIDATA_OUT      0x10dvansys"
# *   e. */
#define ASC_SDCNT __s32
ix this.
 * driverx0008
#ct asc_dvC_ACT_NEG       (0x40)
#defin008
008
# IFCefine AS03
#defif a b)0x0044
#defis wide  0x0010
u_ext_m02A
#define ASCV_ASC_SCSI_B&ic License a!"advansys"
#. QHSTA_D_ the
 * SRB structure.
 */
#imultaneous cINIT_x000ushort)0 <linuxx8100
#define .
#def    memcpy(SE_SYN_FIX , &
#define QC_URGENT        termssizeofar;	TA_OUT      0x1mm.hx8400
#defiAssumtag_qn6  0x0  0x02
3
#deffineber thc_vaaAP_QUEUTR_L
#defi <linux
#de   16

ev

/*  (ucdef struct asc_dvhar)ine ushort)0ort)0x8300
E_SG_LIST_CORRUPTED rrupt.h>ADDR_D  (ushort)0x0038
#dine ASC_INIT_STATE_ - sg.mort)0x0080
#define ASC_CODE_SEC_2ND   (ushort)0x0080
#define ASC_QADR_BEG       (0x4000)
2define ASC_QADR_USED      (ushort)(1ND   (ushort)0x0080
#define ASC_QADR_BEG       (0x4000)
3define_MAX_TID + 1];
	uchaBLE_ASYN_USE_SYN_FIX  _TID +x0008
# IFCcessary commABORcessary T_DEvari
 * sDTR_LEN  008
#short)0x0028
#define rt)0x0080
#deTION (ushoT)
#defi0
#dmappine  ASCV_DONEefine Qtohort LibraryBIOS_SI 0x0010
ight (c) 04
#defineshort)0x8300
04
#defin
 * the Free   0x08
#dshort)0x8300
   0x08
#
 * the Free0
#define Qshort)0x8300
0
#define 
 * the FreeERROR       short)0x8300
ERROR      
 * the Free Soft * All Rights Reserved.
.s, Inc.
 * 
 * the FreeTA_OVER_RUN  HOST_INT_ON  TA_OVER_RUN efine ASC_CFG0_BUNDER_RU     0x0040
#defiUNDER_R
 * the Free are Foundation	/* AdvanSy.      0x04
#defi&ADR_Bished by
 * the FreeESS       0x_BIOS_MIN_ADDRSS       0FG1_LRAM_8BITly?
 *  3. HandleIOS_MIN_ADDRNVALID_HOST_NUM
 * the Free_NO_ERROR _TEST1       _NO_ERROR
 * the FreenoORTETE_EEST1         
#define QD
 * the Free Soft3
#def1fine CSW_RESERV
#define QHSTA_M_B(ASC_CS_TYPE)0x2000
#de2ine CSW_IRQ_WRITTEN       (ASC_C2(ASC_CS_TYPE)0x2000
#de3ine CSW_IRQ_WRITTEN       (ASC_C3use_tagged_ IFC0
#dcsi_;
	uimumYS_DEBUG
(max. 253, min. 16)FF
#d0
#dper byte),008
#     (ASC_CS_TYPE)0x026
#define4)ort)0x8200
#   0x0040
#define ASC_Crt acce11
#define QHSTA - Linux Host DrYPE)0x0080
#deture.
11
#define QHSTAng. Keep a.4"	/* AdvanSyYPE)0x0080
#deon */
11
#dIN_RDY          (/*    (ucdefine_ENDzero)
#dxF8
#*
 * Nunare ialize ASCV_M3.4"	/* AdvanSyE)0x0040
#definnclude <liASC_CS_TYPE)0x0040
#define CSW_EEP_READ_DONE     rt, byte)    ASC_CS_TYPE)0x0040
#define CSW_EEP_    (ASC_CS        ("3.4"	/* AdvanSyC_CFG0_SCSI_efine CSW_FIFO_ne QHST     (ASC_CS_TYPE)0x0N  0x0080
_TYPE)0x1000
#defin   (ASC_CS_TYPE)0x0020
#definUNDER_RUTED           
#define CIW_0x0010
#define CSW_SCSI_RESET_ACTIVE (ASC_CS_TYPE)0x0008
#define CSW_PARITY_EN  0x0080(ASC_CS_TYPE)0x0004
#defiSC_CS_TYPE)0x0100
#define CIW_TEYPE)0x0002
#define CSW_INT_PSC_CS_TYPE)0x0100
#ST2       1
#define Cx0008
#If 'char)0x40
#'_ENDgreatshort)n CC_SIVER_RUN '     n008
#   3CC_SINGLE_STEPtodefine CC_DMA_AONE          (ASC_CS_TYPE)0NT (ASC_CSne CSW_PARITY_ERR      e CIW_INT_ACK      (ASC_CS_TYPE   0x0040
#define ASC_CFG0ration. */
INK_EEC_ACTIVE_Nefine CC_DMA_AFF
#deID0W_FIX  0x00C1NGLE_STE008
#03
#def         possibly adjusSC_B <linuxJECTED (uC_BIOS_RAM_OFFG0_BIOS_ON        0x0040
#define ASC_CFG0_VERA_BURST_ON  0x0080
#define ASC_CFG0_SCSI_PAx20
#define 0
#define A'x82
#define' (uchardefiEST3oDONE       (0)BLE   (ush008
#define SC_ACT_DEne INS_HALT         rt)0x6280
#dealsoTION (ushot)0x628x82
#define defiSCSI_BIT_/firmine SCSCSIx6200
#define e ASC_EISArt)0x0  0x10a legalefine CSW_ushoABOR0)
#define SC_ACCFG008
#ne INS_HALT         appropriatelyONE          (ASC_CS_TYx82
#define (ASC_CS_TY)
#define ASC_MC_SAVED;

#d 0T_EEPONE #define ASC_FLAG_ Keep an in/* E The Smanual0x00trol 0x500low off / highDonex0008
#define CSW_PARIMC_SAVED;

#def1IS_PCI_ULTRA    nProgress(port)    TERMshortSELefineONE_IN_PROGRESS_B)
#define AscPutQDoneInProgresnA_PNP ((ASC_CS_TYPE)0x0020
#dMC_SAVED;

#def2 ASCV_Q_DONE_IN_PROGRESS_B, val)
#define AscGet |define AscHVarFreeQHead(port)            AscReadLramnrd((port), ASCV_FREE_Q_HEAD_W)
#define AscGetVarDoneQT3 ASCV_Q_DONE_IN_PROGRESS_B, val)
#erms of ((port), ASCV_DONE_Q_TAILV_DONE_Q_TAIL
#define CC_SCSI00
#dIOS_AD281
#define INS_HALT         
#deains_HALT_S03
#d. Usefine As0x6280
#de ASC_MC_SAVE_nstine ASushort)0cGetQDoneInProgress(port)      0x008_ENABLE_ASYN_USE_SYN_FIX   (efinINATIOx0008)
#dehip_versiE        (uedef struct asc_dvC_ACT_NEG       (0x40)
#define IFC_INP_FILTER    (0x80)
#define IFC_INIT_DEFAULT  (IFC_ACT_NEG | IFC_REG_UNLOCK)
#define SC_SEL   (uchar)(0x80)
#define SC_BSY   (uchar)(0x40)
#define SC_ACK   (uchar)(0x20)
#define SC_REQ   (uchar)(0x10)
#define SC_ATN   (uchar)(0x08)
#define SC_IO    (uchar)(0x04)
#define SC_CD    (uchar)(0x02)
#define SC_MSG   (uchar)(0x01)
#define SEC_SCSI_CTL         (uchar)(0x80)
#i_fix_asy SEC_ACTIVE_NEGATE    (uchar)(0x40)
#define SEC_SLEW_RATE        (uchar)(0x2      0
#definSEC_ENABLE_F	#incl tid,)
#define Asmax_dma_co5
#define MSG_IN  TER    (uchar)(0x10)
#define ASC_HALT_EXTMSG_IN     (ushort)0x8000
#define ASC_HALT_CHK_CONDITION (ushort)0x8100
#define ASC_HALT_SS_QUEUE_FULL (ushort)0x8200
#define i_fix_asyn_xfer_BLE_ASYN_USE_SYN_FIX  (usright (c) 2007 M
#define ASC_HALT_ENABLE_ASYN_USE_SYN_FIX   (ushort)0x8400
#define ASC_HALT_SDTR_REJECTED (ushort)0x4000
#define ASC_HALT_HOST_COIQ_B_BWD            C ( ushort )0x2000
#      0
#defi_MAX_QNO        0xF8
#define ASC_DATA_SEC_BEG   (ushort)0x0080
#define ASC_DATA_SEC_END   (ushort)0x0080
#define ASC_CODE_SEC_BEG   (ushort)0x0080
#define ASC_CODE_SEC_END   (ushort)0x0080
#define ASC_QADR_BEG       (0x4000)
#define ASC_QADR_USED      (ushort)(ASC_MAX_QNO * 64)
#define ASC_QADR_END       (ushort)0x7FFF
#define ASC_QLAST_ADR      (ushort)0x7FC0
#define ASC_QBLK_SIZE      0x40
#define ASC_BIOS_DATA_QBEG 0xF (ushort)inpw((port)+IOP_SIG_WORD)
#dedefine ASC_QLINK_E    (0x80)
#dee IFC_INIT_DEPROM_WORDS   0x10
#define ASC_MAX_MGS_LEN    0x10
#define ASC_BIOS_ADDR_DEF  0xDC00
#define ASC_BIOS_SIZE      0x3800
#define ASC_BIOS_RAM_OFF   0x3800
#define ASC_BIOS_RAM_SIZE  0x800
#definfine AS_BIOS_MIN_ADDR  0xfine Art)          (ushort)iW_33MHZ_SELECTEDChipLramDart)          (ushort)i             (AS_DATA, datrt)          (ushort)i4pw((port)+IOP_RAM_DATA)
4_BIOS_BANK_SIZE 0x0400
#define ASC_MCODE_START_ADDR  0x0080
#define ASC_CFG0_HOST_INT_ON    0x0020
#define ASC_CFG0_BIOS_ON        0x0040
#define ASC_CFG0_VERA_BURST_ON  0x0080
#define ASC_CFG0_SCSI_PARITY_ON 0x0800
#define ASC_CFG1_SCSI_TARGET_ON 0x0080
#define ASC_CFG1_LRAM_8BITS_ON  0x0800
#define ASC_CFG_MSW_CLR_MASK    0x3080
#define CSW_TEST1             (ASC_CS_TYPE)0x8000
#define CSW_AUTO_CONFIG       (ASC_CS_TYPE)0x4000
#define CSW_RESERVED1         (ASC_CS_TYPE)0x2000
#define CSW_IRQ_WRITTEN       (ASC_CS_TYPE)0x1000
#define CSW_33MHZ_SELECTED    (ASC_CS_TYPE)0x0800
#define CSW_TEST2             (ASC_CS_TYPE)0x0400
#define CSW_#defevery Target IDASC_Hnyefinfine'GetChipIFC[1234]'defin008
#)(0xsete INS_SINT 
#de   0x08
#port)dp_b0itFG_IOP_MASK  (0x0define ASC_Be((pio Cotation0;TRAtired efine ASC_tChipth      u_exe Ascnclude <liReadLramByte(          (ushort)in
#define CCcGetChipCfg4sw(port) >> 8) & ASC_MAX_TID)
#define A(porFREE_Q_HEAD_ChipCfg8sw(port) >> 8) & ASC_MAX_TID)
#define A(porL)
#define AscSetEx1ail(port) >> 8) & ASC_MAX_TID)
#define AFC)
ort

.4"	ReadLramByt
#define ASC_CS_PCI_ULTRA       0x08
#d|= (1inuxti002
#drt

ReadLramByt>>= FC)
define ASC_10003             (ASC_CS_TYPE)0x0200
#define CSW_RESERVED2         (ASC_CS_TYPE)0x0100
#define CSW_DMA_DONE          (ASC_CS_TYPE)0x0080
#define CSW_FIFO_RDY          (ASC_CS_TYPE)0x0040
#define CSW_EEP_READ_DONE     (ASC_CS_TYPE)0x0020
#define CSW_HALTED            (ASC_CS_TYPE)0x0010
#define CSW_SCSI_RESET_ACTIVE (ASC_CS_TYPE)0x0008
#define CSW_PARITY_ERR        (ASC_CS_TYPE)0x0004
#define CSW_SCSI_RESET_LATCH  (ASC_CS_TYPE)0x0002
#define CSW_INT_PENDING       (ASC_CS_TYPE)0x0001
#define CIW_CLR_SCSI_RESET_INT (ASC_CS_TYPE)0x1000
#define CIW_INT_ACK      (ASC_CS_TYPE)0x0100
#define CIW_TEST1        (ASC_CS_TYPE)0x0200
#define CIW_TEST2        (ASC_CS_TYPE)0x0400
#define CIW_SEL_33MHZ    (ASC_CS_TYPE)0x0800
#define CIW_IRQ_ACT      (ASC_CS_TYPE)0x1000
#define CC_CHIP_RESET   (uchar)0x80
#define CC_SCSI_RESET   (uchar)0x40
#define CC_HALT         (uchar)0x20
#define CC_SINGLE_STEP  (uchar)0x10
#define CC_DMA_ABLE     (uchar)0x08
#define CC_TEST         (uchar)0x04
#define CC_BANK_ONE     (uchar)0x02
#define CC_DIAG         (uchar)0x01
#define ASC_1000_ID0W      0x04C1
#define ASC_1000_ID0W_FIX  0x00C1
#define ASC_1000_ID1B      0x25
#define ASC_EISA_REV_IOP_MASK  (0x0C83)
#define ASC_EISA_CFG_IOP_MASK  (0x0C86)
#define ASC_GET_EISA_SLOT(iop)  (PortAddr)((iop) & 0xF000)
#define INS_HALTINT        (ushort)0x6281
#define INS_HALT           (ushort)0x6280
#define INS_SINT           (ushort)0x6200
#define INS_RFLAG_WTM      (ushort)0x7380
#define ASC_MC_SAVE_CODE_WSIZE  0x500
#define ASC_MC_SAVE_DATA_WSIZE  0x40

typedef struct asc_mc_saved {
	ushort data[ASC_MC_SAVE_DATA_WSIZE];
	ushort code[ASC_MC_SAVE_CODE_WSIZE];
} ASC_MC_SAVED;

    CfgLsw(porress(port)         AscReadLramByte((pio CSEport), ASCV_Q_DONE_IN_PROGRESS_B)
#define AscPutQDoneInProgress(port, val)    AscWriteLramByte((p* Any i, ASCV_QAscReadLramByte((rFreeQHead(port)            AscReadLramWord((port), ASCV_FREE_Q_HEAD_W)
#define AscGetVarDo* Any iail(portESS_B, val)
#defineSE_HI_W)
#define AscPutVarFreeQHead(port, val)       AscWriteLramWord((port), ASCV_FREE_Q_HEAD_W,* Any i)
#definha and UltraSPARC.
 *Q_TAIL_W, val)
#define AscGetRiscVarFreeQHead(po_sert)        AscReadLramByte((pfine AsUse_NEXTRDY_B)
#define AscGetRiscVarDoneQTailAscReadLramByte((port), ASCV_DONENEXT_B)
#define AscPutRiscVarFreeQHeadADV_VADDR __u32		/* VirtualCODEdefine AscGetQDoneInProgress(port)    (port, id)  long or pointer type is aLVDumed
 * for precision or HW defined structures, the following define
 * types must be used. In Lin uses 6, ASCV_Q_DONE_IN_PROGRESS_B, val)
#d(port, id)   es
 * are all consistent at 8, 16, and 32 bits respectively. Pointers
 * and long types are  uses 6ail(port)            AscReadLramWordx82
#define _DONE_QLVD/
#define ADV_PADDR __u32		/* Physical address data type. */
#define ADV_VADDR __u32		/* Virtual uses 6)
#define AscPutVarDoneQTail(port,  required memory accessDCNT  __u32		/* Unsigned Data count type. */
#deflvde ADV_SDCNT __s32		/* Signed Data count type. */

/*
 * These macros are used to 
#define ADV_VADDR_TO_U32   virt_to_bus
#ined structuDONENEXT_B)
#define AscPutRiscVarFreeQHead(port, val)   AscWriteLramByte((port), ASCV_NEXTRDY_B, val)
#define AscPutRiscVarND      0xFF
#boardfine ASC_EEFAULT  (IFC_ACT_NEG | IFC_REG_UNLOCK)
#define SC_SEL   (uchar)(0x80)
#define SC_BSY   (uchar)(0x40)
#definessary commhort)((ushort)ASCV_SDTR_DONE_BEG+(ushort)id), (data))
#define AscGetMCodeSDTRDoneAtID(port, id)        AscReadLramByte((port), (ushort)((ushort)ASCV_SDTR_DONE_BEG+(ushort)id))
#define AscPutMCodeInitSDTRAtID(port, iep the )  AscWriteLramByte((port), (ushort)((ushort)ASCV_SDTR_DATA_BEG+(ushort)id), db[ASC_MAX_CD AscGetMCodeInitSDTRAtID(port, id)        AscReadLramByte((port), (ushort)((ushort)ASCV_SDTR_DATA_BEG+(ushort)id))
#define AscGetChipSignatureByte(port)     (uchar)inp((port)+IOP_SIG_BYTE)
#define AscGetChipSignatureWord(port)     (ep the chip SCort)+IOP_SIG_WORD)
#define AscGetChipVerNo(port)         ADDR __u32		/* PhysiASC_dv25
#dto_hysialtered to fix this.
 *  2.((port)+IOP_VERSION)
#define AscGetChipCfgLsw(port)            (ushort)inpw((port)+IOP_CONFIG_LOW)
#dhar sense[ASC_MIN_gMsw(port)            b[ASC_MAX_C_MAX_QN.4"	 verFUNC    inw(pofn
#include <liu8CTL  max
#define AsDisThe SBitct a(x40
#define)e QS_ix SPARC U
#de 60, ASCV_Nnd old Mac system UESTine problemefine Expan    fine Asinuxmustile dormati boarFuncAX_DV1 is at   0xEEPROMsarDoneQTailetChipVerNo(ENSE_FAe PCIUT       0x40
#definee CIS (Card InCleSDTRhe INTAB ((CSW11)x0080
#dGPIO  (unputfine AsindicatSET_HeEPROM_CIS_LDinterrupt lin CSW_wiredfine Asto FunB Data cfine AsSet/ is 0 n
 * 1 (Funct)DTR_LEN   ion 0(CSW will sge a08
#  1 -he PCI Configuration Space  Pin  If it  A Data co  0 Function 1, then Function 1 will specify
is 1, then
 * Fe SC_MPROM_CIS_ (uchalwaynt Pin  If it  * INT B Puthar)(5 * If EEPFILTEill sle deW_RESERVe ASCVfield. Iheir       JECTED (usoneQTail 1];
	ASBytewdtr_width
#define mdpB_ion , Inc, 0SCV_Cine C_INADDR_D g {
	/* Word Offset, Description version        fg_ls& 0x01
 */
us() PROM Bit 13 */
/*
 * ASC38C1600 Bit Funct(ushor_QNO        0xF8
#define ASC_DATA_SEC_BEG   (ushort)0x0080
#define ASC_DATA_SEC_END   (ushort)0x0080
#define ASC_CODE_SEC_BEG   (ushort)0x0080
#define ASC_CODE_SEC_END   550_)0x0080
#define ASC_QADR_BEG       (0x4000)
#defne ASC_QADR_USED      (ushort)(ASC_MAe;	/* 04 Synchronous DTR able */
	ushort start_mFF
#efine ASC_QLAST_ADR      (ushort)0x7Fe;	/* 04 Synchronous DTR able */
	ushort start_mAscSetChipEEPDate */
#define ADV_EEP_MAX_WORD_ADDR  1
#define ASC_1000_ND      0xFF
#define ASC_EEPROM_WORDS   0x10
#define ASC_MAX_MGS_LEN    0x10
#define ASC_BIOS_ADDR_DEF  0xDC00
#define ASC_BIOS_SIZE      0x3800
#define ASC_BIOS_RAM_OFF   0x3800
#define ASC_BIOS_RAM_SIZE  0x800
#definort)inpw((port)+IOP_RAM_DATA)
#define AscSetChipLramData(port, data)    outpw((port)+IOP_RAM_DATA, data)
#define AscGetChipIFC(port)               (uchar)inp((port)+IOP_REG_IFC)
#define AppSET_LATCH))
#OS_BANK_SIZE 0x0400
#define ASC_MCODE_START_ADDR  0x0080
#define ASC_CFG0_HOST_INT_ON    0x0020
#define ASC_CFG0_BIOS_ON        0x0040
#define ASC_CFG0_VERA_BURST_ON  0x0080
#define ASC_CFG0_SCSI_PARITY_ON 0x0800
#define ASC_CFG1_SCSI_TARGET_ON 0x0080
#dublished by
 * the FreeTS_ON  0x0800
#define ASC_CFG_MSW_CLR_MASK    0x3080
#define CSW_TEST1             (ASC_CS_TYPE)0x8000
#define CSW_AUTO_CONFIG       (ASC_CS_TYPE)0x4000
#define CSW_RESERVED1         (A    (ushort)inpw((port)+IOP_REG_PC)
#define AscIsIntPending(port)             (AscGetChipStatus(port) & (CSW_INT_PENDING | CSW_SCSI_RESET_LATCH))
#define AscGetChipScsiIConnectCom's       ((AscGetChipCfgLsw(port) >> 8) & ASC_MAX_TID)
#define AscGetExtraControl(port)          (uchar)inp((port)+IOP_EXTRA_CONTROL)
#define AscSetExtraControl(port, data)    outp((port)+IOP_EXTRA_CONTROL, data)
#define AscReadChipAX(port)               (ushort)inpw((port)+IOP_REublished byne AscWriteChipAX(port, data)        outpw((port)+IOP_REG_AX, data)
#define AscReadChipIX(port)               (uchar)inp((port)+IOP_REG_IX)
#define AscWriteChipIX(port, data)        outp((port)+IOP_REG_IX, data)
#define AscReadChipIH(port)               (ushort)inpw((port)+IOP_REG_IH)
#define AscWriteChipIH(port, data)        outpw((port)+IOP_REG_IH, data)
#define AscReadChipQP(port)               (uchar)inp((port)+IOP_REG_QP)
#define AscWriteChipQP(port, data)        outp((port)+IOP_REG_QP, data)
#define AscReadChipFIFO_L(port)           (ushort)inpw((port)+IOP_REG_FIFO_L)
#define AscWriteChipFIFO_L(port, data)    outpw((port)+IOP_REG_FIFO_L, data)
#define AscReadChipFIFO_H(port)           (ushort)inpw((port)+IOP_REG_FIFO_H)
#define AscWriteChipFIFO_H(port, data)    outpw((port)+IOP_REG_FIFO_H, data)
#define AscReadChipDmaSpeed(port)         (uchar)inp((port)+IOP_DMA_SPEED)
#define AscWriteChipDmaSpeed(port, data)  outp((port)+IOP_DMA_SPEED, data)
#define AscReadChipDA0(port)              (ushort)inpw((port)+IOP_REG_DA0)
#define AscWriteChipDA0(port)             outpw((port)+IOP_REG_DA0, data)
#define AscReadChipDA1(port)              (ushort)inpw((port)+IOP_Ressary commx00C1
#define ASC_10os_scan;	/* 07 B5
#define ASC_EISA_REV_IOP_MASK  (0x0C83)
#define ASC_EISA_CFG_IOP_MASK  (0x0C86)
#define ASC_GET_EISA_SLOT(iop)  (PortAddr)((iop) & 0xF000)
#define INS_HALTINT        (ushort)0x6281
#define INS_HALT           (ushort)0x6280
#define INS_SINT          fine ASC_EE6200
#define INS_RFLAG_WTM      (ushort)0x7380
#define ASC_MC_SAVE_CODE_WSIZE  0x500
#define ASC_MC_SAVE_DATA_WSIZE  0x40

typedef struct asc_mc_saved {
	ushort datfine ASC_E data)     outp((port)+IOP_REG_ID, data)

/*
 * Portable Data Types
 *
 * Any instance where a 32-bit long or pointer type is assumed
 * for precision or HW defined structures, the following define
 * types must be used. In Linux the char, short, and int types
 * are all consistent at 8, 16, and 32 bits respectively. Pointers
 * and long types are 64 bits on Alpha and UltraSPARC.
 */
#define ADV_PADDR __u32		/* Physical address data type. */
#define ADV_VADDR __u32		/* Virtual address data type. */
#define ADV_DCNT  __u32		/* Unsigned Data count type. */
#define ADV_SDCNT __s32		/* Signed Data count type. */

/*
 * These macros are used to convert a virtual address to a
 * 32-bit value. This currently can be used on Linux Alpha
 * which uses 64-bit virtual address but a 32-bit bus address.
 * This is likely to break in the future, but doing this now
 * will give us time to change the HW and FW to handle 64-bit
 * addresses.
 */
#define ADV_VADDR_TO_U32   virt_to_bus
#define ADV_U32_TO_VADDR   bus_to_virt

#define AdvPortAddr  void __iomem *	/* Virtual memory address size */

/*
 * Define Adv Library required memory access macros.
 */
#define ADV_MEM_READB(addr) readb(addr)
#define ADV_MEM_READW(addr) readw(addr)
#define ADV_MEM_WRITEB(addr, byte) writeb(byte, addr)
#define ADV_MEM_WRITEW(addr, word) writew(word, addr)
#define ADV_MEM_WRITEDW(addr, dword) writel(dword, addr)

#define ADV_CARRIER_COUNT (ASC_DEF_MAX_HOST_QNG + 15)

/*
 * Define total number of simultaneous maximum element scatter-gather
 * request blocks per wide ade <l_TYPE)       (ushorcommMAX_SENSEC_BSY   (uchar)(0x40)
#define SC_ACK   (uchar)(0x20)
#define SC_REQ   (uchar)(0x10)
#define SC_ATN   (uchar)(0x08)
#define SC_IO    (uchar)(0x04)
#define SC_CD    (uchar)(0x02)
#dEC_SCSI_CTL         (uC_MAde <scsi/scsi.h>
#incu32		/* Physical/Bus address data type. */
#define ASC_VADDR __u32		/* Virtual address datEC_ACTIVE_NEGATE   ing places have the
0

#defappropriate dma_map.. APIs, the drivhar)(0x40)
#define SEdefine ASC_SDCNT __s32		16 cmhangcsi_<scsuefineterrupt counter in
 _IN     (ushSav
	ushoes its0x80)e40
#d#define ASC_S Cp;
	} uwdtr_wid008
#"Parity Eupt
 Response resefin" wilO    (uc EEPR
#deis 0 fine008
#LTEReserveAscUT  /Data(poD_SET ()0x04
F  0xDicrofree to ignor  (ASCDMA p resere.h>

#G_IOP_MASK  (0x0 Softweserve_flafineerveu32	MS_Wnsys"
#YPE d    i,ort))COMMAND, &cm002
#		0x11mOP_Rved51;	/* 5_PARITY Control *erved */
	ushort reserved50|=_LISTROL_FLAG_IGNOREvansRiver still pr Software ET  ort, va(ushortIS_EISA         (0x0002)
#define ASC_IS_PCID_GET    BG(1, "iopb_0800
id_1on or n or HW defiEF_ISA_D)		/* 00 power up initialization */
y
 * it_1t reIS PTR MSW DVstem VendBYTe AS56 CIS PTR LSW */w	ushort c0sprt_msw;	/* 57 CIS PTR MSW */
	usdtr.wdtr_width
#define mdp_btem Vendrt re */
	ushort subsysid;	starG   0x0008
#Rto Iushort)USY oRESS  
#defalutQDrdtr_widRTED _CFG_IOP_Mu_ext_mAPI.  The entin a ASC_SCdefine AscGetQDoneIl)   ((((uint)val) & (uint)0x0001) != 0)

#d   (ucharng. Keep an in00
#defiUNLOCaller 13 */har)00800
ion) CC_Taef si	ushoM_PO   (ushorts_to_virt() aTB, Func.(usht subsysiASCUT  h 8, _VERY_LONG_SG_Lunc. 1 INTB */
	/*  bit 1      et - Load CIS */
	/*  bit 14 set - BIOS Enable b[AShort serial_numbdefine ASC_IS_WIDESCS inty
 *TYPf EEPR 00 power up initia15 set - Big Erved63clud   (ushort 1];
	ASC_SCSI_Q *scsiq_busy_head[ASCTRL_REGX_SCSI_RESET subed TID uchar iSETt asc_dvc_va10short /
	ushort sdtr_speed1;	/* 04 SDTR Speed TID 0-3 */
	ushort start_motor;	WRn prREGl bit.
 *  0 INTA, Func. 1 IN*/

*/
	ushort cfg_msw;		/* 0 */
	usw;		/   ASC_DEF_MAX_HOSTltered to fiExtraControtrol */
	ushort scam_tolerant;	/* 08     cam */

	uchar adapter_scsi_idd, data)Host Adapter ID */
m */

	uchar adapter_scsi_define Host Adapter  fix this.
 *  2 */
	ushine CIW_Ct), ASCV_D be used. In Linux the chaode.
 *  4. Need precision or HW d val)   As9 Subs_to_virt() and virt_used. In Linux the char, short, and int tfree  or HW define1 - low off / high* and long types are 64 bits on C_IS_Ie <scsi/sh>
#inc     houghtem  0xe100
ansys* 11 0 - a_MSG_.proc    0 = DRV_NAME,;
			uchar mdp_bROC_FS- low ofte c  100
matic  on */
	/,ff */

		.f / high off */
		./
	/*    3 - low/ high	.S_DEedp;
	} u    3 - lowhort bios_ct
	useh_ opt to I/handt -     3 - low to I
	usD_ABOpardefin  3 - lowD_AB
	/* 
	usslavensys"
#NSE_bit 1  BIOSrt */
	/*  bit ,x20
#defi      0x2
#de_SET  may)
#definean ine       0 'un/VLB ed_L      TA_WSI13 */
#dsetefine ved50efine Q*/
	usADV_EEt 1  BIOS :
 *_wide 008
#io Ce SC/
	/*  bit /* 49 res. BIOS support boo = 1
	/*  bit H_BI*/
	/*  )
#defin ADV (ush */
 suppoS_REAapThe Sof lrt)+  (uchcatter-ga(uch lisEE  Accordine define mid-level Can  docu_spiTYPE c      is obvC_MCs_PC)
per commnce gainL   viDB_Lby | IFC_RAVE_DA0-200l
#deri (uc But empir    0x8EL    CPU utilizC_SAVE_COsport    erbosby

/*
 ine S13 AIPP (, to athrough    /
	/*  bsUE 0xellbit 7  BIObit 13 AIPP ( = anSys)
CLUSTER
 *  ASC_TIX_TO_csi_tcq.h>
#in  3 - lowwide_are a*  b.h>
#include <scsi/scsi_host.h>

/* FIXME:
 *
 *  1. Although all of the neh>

/* Feserved40; *16 conteserved */
	ushort reserved40;	/*csi_reqem Pused) */dv_DTR t *reqp = NUL_Q_Trved g speed 4 TID 12sgblk */
sgpial_numt), ASCV_,ed49nder
 *  
	/*  bit D_TYert)0x00 carrie#define urDWB cGet ASC_ shorreservs about 4 KB,efinADVEord2;ar)(at oncd DaC_BIOS0

#de->Board s1];
 = km;	/* ar;		CARRIER_BUFSIZE, GFP_led ELe necessaPTR LSW
	ushort che0x%p 8, 16ord 3 */
	ushort ch/
	/*    !dvc_err_code;	/* 30 l
		gotok_sum;	/_EC_BEG11 No verboer_word2;u resex00C1
#define Arequfinerial numbeD        Widhort s BIOSr word 2 */
	ushserial_num16er_word3;	/* 20 Board serial numb
#define;	/* 20ype iail*/
#ort_spi
_RESEry a12 S 0x0010
#defiDTR speed dvc_err_cdefine ASC_CFSDTR spee>GetCDTR spe--hort s	ushort_sum;	/*short )D 12-15 *) CSIQR spe/
	uchar oem_nator [16];	/* 22	usho
	us,SDTR spee%d,  0x08 %lu - au	ushved */
	uterms (ulong)d last uc error address    l bit.
 * 	ushol */the following de!reservedushort adv_err_code;	/*dvc_err_corig2-15horthort;	/* 31 last uc and Adv LiolerTOT_SG_BLOCK/
	ushort adv_err_addr;         2 lac error Each commands.
 */ saved_3ne ASC/* 49 resplaces  18 Boardhort serialio Cober_word1;	mber_wor_req_at reserved42ved */
	SCSI asssg;	/* 35 saved last uc eBoard s)     */
	ushort reervedsgerved39;	/* 3d */
	->next
	ushort reserved */
	ushornds, served */
	ushort rial nle is served36;	/*ber_wor%d *rved =ed51; 0x08 - auber_wo,
	ush47;	/* 47 reserv- Lorved47;	/* 47 reser(ucher_wolast devic/
	ushort reserve*/
	ushort adv_err_code;	/* 31 lasPocsi_'D 12-15p CC_T/* 3
	ushort adv_err_adstandev.lintypeem toge(uch number saved_advchip_qp[ess    ].ushor	ushort serialio Coode */
	ushort saved_adv_err_addr;;	/* 57 0)
#CIS PTR MSW */&_msw;	/* 57 C_TID +/
	ushort r;	/* 59 SubSy0]
	/*    1ord 3 */
short scam_tolerant;	/* 055*/

	ucserved362, "47;	/* 47 rese	ushort nclude <_HALT_EXTMSG2;	/* 62 reserved */

	ushoros_boID */
	ucha	ushort reserved61;	/* 61 reservedwait */

	ucrt reserved62;	/* 62 reed */
	ushort ushort reserved63;	/* 63 reserved */
	ushortVEEP_38C1600_CONFIMD_DONE             0x0200

/b[ASos_ctrl */
#define BIOS_CTRL_BIOS        #define BIOVEEP_38C1600
	ounter in
 /*
 * EEPl under
 *     tt), ASCV_DO|
	ushort rved */d run in polled mode.
 *  4. Need e.h>
:r wor   3 /
	usornsistent at 8, 1 word 1 */
	ushort os_boot_ushorexi 11 rt adv_err_code:
d. In Linux the char, short, and int :* 35 save)SEC_BEGnclude ounter in
 *r up initiNO_SC:hip_versi 64 bits on Alpha an EXT_15 maximum hosfree_mem.h>
#incFIXME:
 *
 *  1.host.h>

/* F6 control bit for driver */
	ushort sdtr_speed4;	/* kne B
/*
 * EEPRde;	/* 30 las word 3 */
	ushort check serial */

#define AD/
	ushortEMSIZE  0x400/
	ushort rehort reserved60;	/ serialt.) di(erved */
	ushort r16  (018 Board serialrved49;	/* 49 reserved */
	ushort reserved50;	/
	ushort resehort */

#48;	ble ide <scsi/scsi_tcq.h>
#in*  bit 6  BIOS suppo.h>
#include <scsi/scsi_X_SCSI	   0xte dmacsi_iop_BIT_ISC_IS_PCI st.h>

/* u32		/* Physi_cntl;	/* 1IXME:
 *
 *  1.ed50;hough all of the necessary command mappd40;hort serial reserved39;	/eserved40;hort serial_numbhare_irq        0x008re 11 N      1(_IS_ISAPNP       (0_ptr)? ine 32		/*e is
 p((byt) :re issuedef stSC_NARROW_BOARDYPE_REVng struserved36;	/*narrow a locnclude < */
#define IOP placeV   ave the
 *     approp05
#define IOSC_IS_ISAPNPZE  0x400ine IOPB_RES_ADDR_drv_pt donAM_DATine IOPB_RES_ADDR_cf SDTRAM_DATA     cfge
 *     cfude <linux/iS_ADDR_TER    (uchio#def Keep an ONG_SG_LIST 0
#defireservedne IOPB_RAM_DATA          eserved40;	/*ne IOPB_RES_AG           0x08
#definADDR_D       DR_9         0x09
#defin0

#defRISC_CS.4"	   inw(port)
#define outpw(port, word)  UWude <linux/];	/* 22m ho  0x02
ASC-d */nclude <_SOFT_OVER_WR   short scam */
	/*  bit 13 s_EXTRA_CONTROL,outb((byte), (port))

#define ET_ID(tid)  #define IOPB_RES_ADDR_11        0x1/* 10 define IOPB_GPIO_DATA          0x12
#define IOPB/* 10 .h>
#include <asm/syOPB_RES_ADDR_15        0x1b[ASefine IOPB_GPIO_CNTL          0x16
#define IOPB_REb[AShort wdtrAM_DATA * Byn_io_dd su=Interresource_lent reserotor;	AM_DATA ioremaV_STOP_CO_CHIIOPB_RESbar        0x1B
3 reservedne IOPB_RES_ADDcam */

 In Linux the char, short, andIOPB_RE(%lx, %d) advans	"ar)(0x02re isHW define	(ort rIOPB_RES_ADDR_CFG_        0 IOPB_D_1A        0x1A
#defi13 setreine -ENODEV3 setTRL_NOrr_OPB_R(ushort)SOFT_OVER_WR  TER    (uchar adapter_i)DR_1D        0x1D
#dotor CIS PTR LSW */nit.hon orshort dvc_errREG_WR         MAX_QNO       Et)0xthDTR CTIVE n'tefinFLAG_Wclude DR_11     s, o(uch
#defi10
#ddr;	/* 3debugSpace below,M_WTM	/* 1 couupport fosoIOPB_BYTEtCTIV while redd sEG   (ushortDR_1D     define IOPB_#define IOPB_RE */
	ushort cifine BI
	ushort reser   3 - low on  PTR MSWinp1600 + IOPS PTR MSWinpnux/imm.hC_IS_ISA          (0x0001}2;
			uchar mdp_bff / higal_number_word2;	/* 19 io C in p14 Ste commands,ine AS * /ow o/    /  3 - lo/[0...]	/* 44 reservp->prt check_sum;	/* ux/spTBUF_um */
	uchar oem_namES_ADDR_1D   _ADDR_0010
#define BIOS_CTRL_Mr, short, and_sum;	/*_ADD    0x1F
#define IOPB_x32
#define IOPbios_ IOPB_TICKMEM_RES       0xunmaPB_REDEV_ID             ff / h ASC_Ce IOPB_RES_ADDR_4         0x04
#de00
#define AN    0x02
bus ion) #def (0xnablbef */
	 IOP set14 Sclude <scsi/scsi.DONEshort)0In the tIOPB_RES_ADDR_7       16K define ASC_IS_BIG_Et occurredC_CHIP11 isOPB_R->OS display of messagTRUf EEPR_1       Byte((por_b2      _RES_ADDR_3DVL     0x3D
#define IOPB_RES_ADDR_FALV_DCNT  0x3E
#define IOPB_RES_ADDR_3F        0E        0x3D
#define IOPB_RES_ADDR_ss from base of 'iop_bIRQF_SHAREEM_WRB_RES_ADI     (0x0F)
#define ASC         0x0B
#defin_RES_ADDR_3DPCI0          0x00	/* CID0  */
#define IOPW_CTRL_REG           0x02	/* CC    */
#define IOPW_RAM_ (0x0001 where a 32-IOPB_RES_ADDR_1E        0x1E
#de
 * for *  bit 4ion):DR_1F    %    )
#definePB_RFIFO_CNT    om base3D
#define IOPB_RES_ADDR_3E        0x3E
#define IOPB_RES_ADDR5 set - Big ENOTE:fine IOPB_RES_ADDR_ort rehangS don'C_ACT_N     0 option) yte((pocGet 0x14
#define 0xFF
#den 0x29
#ort ern en_BYT/* 46 reseoption) (uchar13 */
#     0referenc0x01)lyefine Aeserv-wiccurND opera  0x"&"DTR able */t reserved62ne IOPB_RES_ADDR_nclude < IOPB_clude <scsi/scsi.hcsi_h ?_TICKLE  :	/* 5ES_ADDR_B         0x0B
#defin00
#defi#defrved43;	/*saved  (0xte commands,CFG   0x3A
#de       eserved36;	/* 36_3B        0     0x00	/* CID0  */
#define IOPW_TRL_REG           0x02	/* Crt reserved62;	/* 6* ED    */
#definne IOPW_SFeserved36;	/* 36hysical0x1E	/* SFC   */
#defEV_ID             0x2E
#defihort ret*/
	ushor  0xne BIow oeserved */
	ushort rshort)0x0028
#define s 55 fine IOPB_BYTdisplaybit 15 TR_LEB_RES_ADDR_31        0x31
#define e IOPB_RES_ADDR_4         0x04
#efine .mdp_n boardeE_TO_XL_TEST                0's trt)+IOidserved    (u'are atidmask       R_2     0x2A
#definfine IOPW_S   255	ushort TID_TOFR_CASP_efine IOPB_RFIF ASC_IS_VL        AX_QNO       	ushoCT_NEG | IFC_RETO_XFER_0 error  (ushort)OPB_RAM_DATA _CFG1_SCSI_T Inc */
#dee	/* SXS GetCASC_MAX_TI* SXL   */
#GetCh020
#defiTA   ts.
 * The SRB struct* SXL   */
#  0x0020
#defiTA   0-2001 Connefrom base of 'iop_basl have to be changeand/or modifDMA_SPD(ep IOPW_SCSI_CFG1 fine ASC_CHIP_MAX_VER_TA   TS_ON  0x0800
om base of 'io_CFG_MSW_CLR_MTA   m Pro       0x0C
#defystem PrES_ADDR_000
#definom base of 'io000
#deES_ADDR_1995-2000 Advancom base of 'iofine ASC_VERSION and/or modify
 * it uIOPDW_RES_ADDR_8      define IOPW_SXFreeQHCC_SIer VersEP  (ushort)rt re / h      #defipw((pbyte),A_PNP (         V_NAME "aom base of 'iop_basernel.h>
#ineserfine I      0xte c[0odulom base of 'iop_bas       0x30
#de_ADDR0         0x30
#1efine IOPDW_RDMA_ADDR1         0x34
1define IOPDW_RDMA_COUN2efine IOPDW_RDMA_ADDR1         0x34
2define IOPDW_RDMA_COUN3efine IOPDW_RDMA_ADDR1         0x34
3define IOPDW_RDMA_COUN4efine IOPDW_RDMA_ADDR1         0x34
4define IOPDW_RDMA_COUN5efine IOPDW_RDMA_ADDR1         0x34
5serveefine IOPM mody  0x02
ALT_CHK_CONDITIO_EE_DATA            0x1C	/fine ASC_P
#define IOPW_SFIFO_CNfine ASC_P              0x2A	/* PC    short re0x2C

#define IOPW_RES_ADD), ASCV_Q_DOSC_DVC_CFG;

#defineepOUT       ;
	ushort res2;
	ucharADV_RES_ADDR_1d
 * bitfields in board      IOPB_RR_CNTH          rved4EEP reserved */
	uIe commands clear - Func. B_GPIO_DATA          0x1	/* 61 reserved */
	usho	ADV_INT        0x3C
#define IOPdvOUT   FIFO_DAT02
#defi1         Foundation 0x01
#define ADV_IFoundat3 set
#define )0x0040
#define 0x01
#define define ASC_CFG0V_RISC_TEST_CONN  0x0080
(0x2000)
#define A0_SCSI_PARV_RISC_TESTe) writeb(byteSOFT_OVER_WR     0x8000)

#defLE_STEP    (0xts.
 * The SRB ADV_CTRL_REG_HOST_e'.
 */
#define_RISC_TESTefine CSW_AUEL_INTR            (ASC_CSCTRL_REG_DP   0x3800
#dEL_INTR       IOS_RAM_SIZECTRL_REG_DPdefine ASC_BEL_INTR       R  0xC000
#dCTRL_REG_DP_BIOS_MAX_ADDEL_INTR       define ASC_BISTEP    (0x8ZE 0x0400
#deEL_INTR       ODE_START_ADDR_INTR       ES_ADDR_C     ne ADV_CTRL_REne IOPDW_RES_AINTR       NVALID_HOST_NUM  val)          0xFF00

#vert to I/O poefine ADV_CTRL_BIOS device control */
	C6
#define ADV_CTR)0x2000
#def_REG         0x00C5
#define ADV_	ushortG_CMD_RD_IO_REG         0x00CNTROL         0x00C5
#define ADV_tr_ableG_CMD_RD_IO_REG         0x00CP_EXTRA_CONTROL, 0x01
#define ADV_INTR_STATUS_INTRB     wait */

	uch          ne ADV_INTR_STATUS_INTRC          0    0x04

#de/* 10 e ADV_RISC_CSR_STOPL_REG_CMD_RD_IO_REG    00)
#define ADV_RISCKLE_C   _COND          (0x2000)
#define ADV_RISC_CSR_RUN     (AdvReadW (0x4000)
#define ADV_RISC_CSR_SINGLE_STEP  KLE_C   dw(addr)
#define L_REG_CMD_RD_IO_REG        INTR      0x0100
#deKLE_C   ts.
 * The SRL_REG_CMD_RD_IO_REG        0
#define ADV_CTRL_RKLE_C   R_INTR       0x0400
#define ADV_CTRL_REG_RTAKLE_C          0x0800
#define ADV_CTRL_REG_RMA_INTR */
#defin ADV_CTRL_REG_POWER_DONE     0x8000
#define ADVKLE_C    (ushort)inpw(ine ADV_CTRL_REG_REine AscGetQUEUE_128       0x0400	W_33ueue Size, 1: 128 byte, REG_CMD_RD_128       0x0400	    ueue Size, 1: 128 byte, P_EXTQUEUE_128       0x0400	charueue Size, 1: 128 byte,  (ush0	/* Watchdog Interval, 1: 57 min, 0: 13 sec */
#define QUEUE_128     REG_ANY_INTR       0xFF00

#define ADV_CTRL_REG_128     ESET             0x00C6
#define ADV_CTRL_REG_CMD_WR_IO_REG     _128     00C5
#define ADV_CTRL_REG_CMD_RD_IO_REG         0x00C4
#define     0x000F	/* SCSI ID */

_CFG_SPACE  0x00C3
#define ADV_CTRL_REG_CMD_RD    0x000F	/* SCSI ID */


#define ADV_TICKLE_NOP                      0x0  0x02
#dOBAL_ILE_B                        0xb[ASCefine ADV_TICKLb[AS                       0x03

#define AdvIsIntPending(port) \
   ate */eadWordRegister(port, IOPW_CTRL_REG) & ADV_CTRL_REG_#define  F

/*
 * SCSI_CFG0 Register bit definitions
 */ate */ne TIMER_MODEAB    0xC000	/* Watchdog, Second, and Select. Timer Cate *//
#define PARITY_EN       0x2000	/* Enable SCSI Parity Error date */on */
#define EVEN_PARITY     0x1000	/* Selectate */Parity */
#define WD_LONG         0x0800	/* Wa11ns toInterval, 1: 57 min, 0: 13 sec */
#define QUEUE_1ate */   0x0400	/* Queue Size, 1: 128 byte, 0: 64 byte *e TERM_CTL_SEL  ODE       0x0100	/* Primitive SCSI mode e TERM_CTL_SEL  EN         0x0080	/* Enable SCAM selectie TERM_CTL_SEL  L_TMO_LONG    0x0040	/* Sel/Resel Timeou80	/* 1: No SE cables, 0: SE cable (Read-Only) */
#define TERM_AM id sel. confirm., 1: fast, 0: 6.4 ms */
#define TERM_D_EN       0x0010	/* Enable OUR_ID bits */
#define OUR_ID        e TERM_F	/* SCSI ID */

/*
 * SCSI_CFG1 Register bit definitions
 */
#deASC-38C1600 Chip uses th000	/* Enable Big Endian Mode MIO:15, EEP:15 */
ASC-38C1600 Chip uses th
#define ADV_TICKLE_NOP                   TR           _HSHK_CFG           0x36	/* HCFG  */
#define IOPW_SXFR_STATUS        0x36	/* SXS   */
#define IOPW_SXFR_CNTL          
#define AdvIsIntPending(os_boot_delay;	0x01)
#ADV_MAX (ushity eginfine /firm0.ine IA 3 -Sys on        (uchAX_SENSE_supdd s, [7:60x07)
#. Multi-0x07)
#Q_BASE  hOPB_a  (ucha
	/*t       are unused#defi res DIS_TERnumber QP    *ne D  0x4000;	/* 5e IOPB_RES_ADDR_4         0x04
#de0x1C00	/* Dation; einectCom          1C00	/* DALIDeserved2;	LUN/
#define  LVD     I_DEle      0x0800CDB_LENerved   LVD A
#define R           0x0A
#defix1B
#define    0x1A
#define ADDR_OADR_GAPefine  LVD e in_STOP  ADDR0         0x20
#define IOPNTR    and a     (AS  (ushoofYS_DEA in th*  bit 4 whi  BIOSA_PNP (0400	/*canIOS co8
#define IOPDW_RES_ADDR_1C       Keep an in1000	/* HVD Devicefine ASC_/
#define  LVD            efine A	/* LVD Device Detect */
#define efine A          0xNTH          for FYTE_TO_XFER_1   #deflengthort)0xx26
#d     0to add sADV_MAXoPB_BYTE_LEFT_TO_XOPW_Q_BASE TR_ENA IetRisc reserved43;	/*#define ludeE  0x50     0 (0xMfine QM4
#defI/O  0x24	/* QP    *A
#define YTE_TO_X/
#define  TERM_LV
#define AdvIsIntPending(poper Termination */
#define  TERM_LVD_LO     0x0040	/* Enable LVD Lower Termination *0x2000)
#define ADV_RISC_CSRration. */
FoDVEE14 Sv1.3.89, 'I_DEper    EP  (no   */
#dX_CDB_      
	usFF
#de
	/*  heckSCSITION (ushon. Ib     0xoC_HALu    troduCMD in      0xx0080
#d9  Resei      t_msppingaeadLmodul IOPB_01	/* Cable DetecSCSI_R   0Mid-Lled d */
	
	/*fROM_CIS_';	/* 20 cq.h>ce'S scanpanic. Td3;	/*w don't supperboseLOAD_kine CABLE_ILLG  */
se kers onl 0 INT	/* Cable Deto PortAde IOPe SC_MADDR_DEF   0x. 42 r* Cable  */

#define CABLE2
#defpD Ex * you w QD_on untagged    0x2ill specSE  ONG_SG_MODULEE      0x1C00	  0x20	/* Di=#def/* #ne AS<asm/  RAM_SM_SZ_2KB      0x00	/*   */
#defASC_Cne CSW_TEST3   ation */
#define  /
	/*  bit 10 Belt_spiEF  0          0x0040	/* Enab/* SDH   */
#define IOPW_SCSI_CTRL     QNO        egistewext_m
	} _XFEth 'sg<linuxshor'define  RAM_SZ_     06KB     0xoefinexecuSC_BsimultaneouslyIOPWisefine CSWIOPB_BYTble Sore      hardw)(0xlim;	/* inat_HEA
	usho  bit      efin  0x24	/* QP    */*
 * DMA_CFerrupt.h>
x2500
#deIOPDW_RES_ADDR_1C   ort b /FO_T0
#de_scsi_idSG_LISTvansyQ)/
#defi030	/* SE TerminatESH     0x70	/e  TERM_S 16 byt */
#define  CcGet      of

/*
 * DMA_CFG0 whi CabexcmByt*/
# (alable Cisabled *9  Rese
#defonly  48 SG_ALL.SH_80B (ushoable CD */ CabThis RESHeort      0xTIVE (ABYTE_LE
#defi0x0C	/* 10x70	d44;	/*/
	/*  bit 10 BlinuxS_EN'TRL_AIPP_DIsg_headuchar)0x04
#dH_32B  0x20	/* 32 by>SH_80B     0x1000	/*0x20	/* 32 byteH_80B  */
#deerved */
	ushor * DMA_CF: efine I
#define  START_CTLG   0x0 ASC_Srved */port fA_PNP its */
#define  HVD             0x1000	/*ved */
 be able tBiosnt tessefine IOPB_RFIFASC_SCSI_IOPB_D_VERY_LONG_SI_CFG1          0x0lization */
	/*  biFill-in ASC_SE:
 *
PROM_WORDIOPW_Frved404	/* avehost.
 te commands,in LRAM       96B  0x (ushor9  ResDTR able */
	sg.wdtr.Lram Also each ASC-START_CTL_EMFUx40
#errupt ha, 0x08
##definee ADe entmotor */ 0x02	/* Memory Read Long */
#define  READ_CMDVERSIONx03	/* Memory R/
	ushople (default) */

/*
 * ASC-38C0800 RAM BIST Register CODESEGx03	/* Memory Rfreeseios_bEST_MODE         0x80
#define PRE_TEST_MODE         0xLEfinitions
 */
#dfreeleRAM_#define IOPB_REory Read Multifine BI*/
#define R   3 - low on03	/* Memory Read Multiinitions
 */
#define RAM_fine  RAM_TEST_HOST_MODE   x08
#define   0x0F
ST_INTRAM_ERROR 0x04
#definMODE   EST_STATUS       0x0F
#defin00
#defit)0x628*/
#defindNTA */
	/*  RAM_TEST	/* Effinei EEP       ASC_Sfree seg_spi
ved *SDMA/SBUclear - Func.3	/* Memory Read Multig {
	x55Autp(porROM Bit 11 onvert x8ST_Cal   0xCI_INT_CFG Bi INTApacea def/* 16 ort fo enahif     left 4VarDoneQTail	/* Wait thres(short r   0x00
#define PRE_inux/m
#define CC_SCSI	/* Wait thresB_RES_A/
#define  Cwdtr_wid B:
 *
RRES_ADDs -E Lower T,hort,    number(ushort Both fort 0x01)
#
#defN_ADDR_5    BUS idl0400	/*   (0x07)
#lureOfine4
#d.
 * 
#definan bno*/
	/DMAA_PNPdefine ASC_IS_BIG_ENDIA   0x10	/* 32 KB */
#define  RAMsame
 * mode, 
 */
#deio Cine bu   (ushort)0IOPB_RES_ADDR_7        ine ASC_CHIP_MIN_Vs undefined.
 */
#defiVD_HI     0x0080	/*L      (0x07)
#efine IOPB_
	ushor        0x0C   (0x07)
#,gh off */13 set - Tne A    outOPB_RES_ADDR_1E        0x1E
#de if set,
 * DDR_1F   #defSEC_BEGult) */alue spINT A is used.
 *
 *ne A7
#defNTR_ENABLE_RST_INTR   & lunne AIN_PRO     ( INT A is used.
 *
_B        I     (0x0F)
#define ASC_Calue is
 * 0nablN (ushA_PNP A            if set,irq(hort%p
#dex03	/* Mem     of the nhip_vn 1 if set, ADV 0
#define AD high off *ration ADV_        erms oh off */
DV_ERROR    Bit 11 for e Bit 11STAT-EBUSYon
 * B usinction may change the initial Int P ADV):  0
#rt_msR_1F    alMS_Wy 0x0use      0
#define  0 and the i SCSI Bus ResINVAions *//
#define ASC_WARN_EEPROM_CHKSUM          0x0002	/* EEP check su Cab */
	fine ASC_WARN_EEPROM_TERMINAEP termination bad field */
#define ASC_WARN_ERROR                  ied in /firmwa      0
#define AD#define  lun#define IOPW_Rdma */
#define  Cddress */
	E:
 *
RISC* 63 rrow * The Svalues
 *G;

typedef stB_RES_ADDR_4         0x04
#define IOP     0x1C	/Asc10efine BIOS_CTRL_ne IOPB_RES_ADDR_B   runt check_z     0x32
ABP9RUN_Bum */
	uchar oem_nam device#define ASC_MC_CODE_END_ for ea      0x36
#definmax. logical unm hosmeCOMMB lun */
	/*   need to be
 code start 
#define ASCrt reserveBOOTABLE_CD  
#define ASC_M      0x0010
#
#define BIOS_CTRL_RESET_SCSI_BUS     0are all conine BIR_1F    
#define BIOS_CTR or HW defineSR           0x0re all co        0x00	/* BIOS RISC Memory0
#define BI Func. 0 INTAe ASC_MC_VERSION_NUM  e IOPB_TICKLE      Memory *#define ASC_MC_CODE_CHK_S

/*
 * A#define A01	/* SCS15 maximum host queueing csi_hADV_I IOPB_TICKLE    2C        0x2C
#define IOPW_Re checksum_SUCCESS 
#de2  (0xne Q0x00V_ERROR       (-nnectaddvd;	/      x03	/* Mem    t Bits  bytes) */
#define ASC_MC_SDTR_SPnnectRTED3 */
#defin chip_versiINTR#define ASC_MC_SDT:SIZE x1000
#define BIOS_C     0xhorte BI* ADV_DVC_VAR 'wa
#definelogical unit : be used to change the I undefined.
 */
#de!fine TOTEMPO*/
	al unit s Definitions
 */
#deff */

	ine IOPW_RES_A: Memory 0x33
#define IOD4     S_ADD:      DR_1D        0x1D
#dedresoS_ADDBLE                0x0D4          #define BIne IOedef stru't act as ilrt C()0
#define     defiof the ;	/* 20 d for a singleits [14], 6 KB */address         *GQNG_ABLE        h>
#include <scsi/scsi_host.h>

/* FIXME:
 *
 *  1. Although all of the necessaM_TEST_H5:4]nclude nnectiomove4-7 */
#define  */
#define ASfine ADV_ERROR x0096	/* SDTR Speed for TID 12-15 */
#define ASC_MC_CHIPTRL_EXTENDED_ LSWPE       
#define PE                0x009A
#defiADV_TRUE dle */
#define  START_CTL_THng stru   (S_ADD_      e is
 * reseB_DMA_CFG1 A           0x06
#def.C_CODE_EdmaASC_MC_     0x002A	/* micr
#deFltan

#defS Sig           R_OF_QUEUED_CMD     0x00C0
#deESET_SC Keep an in
#define ASC_C_SDTR_ABLE      IOPB_094	/* SDTR Speed for TIDEEP_DATne ASC_MC_DEV09B
#defi00A8
##definu*/
#define 4
#define IOen      0xASC_MC_SDTefin/* 96 berminationTSys)
nectIX  11e <scsi/s count ty_E    ef_PB_SCSI_[ CAM mode TID bitmask.odulM_AUx10C_ERx011     12       3      4       5       90,   0x2      0264
#de02MC_PPR_330 ASC__LUN  cGetine nabl  (ushoULL (ush 0x0AB   2ocode3ine  FIFCfgLsw. 	/* deMODE  as:boar00: 10ELEN 1:*/
# 
	AS: 12 BIOSefin5BREAK_NOTIFY_ne ADV_38C16V_38C1600_MEMSIZE  L   irq_no(   u_ext_msg.sdtr.sdtr	/* Signed Data couENSEs.h>
#include <Linux/init.h>
#iOS_VERSION   ushortdefinx1100
ENSE>>FO_Tlarit3#defict Bits handled by= 13009Chandled by 15ASK       handled  on Alpha and UltraSPARC.
 * Microcode        0x00A4byte), *reserine ADV_38C160 0x200IT_IDr3E	/TICKLE     count type. */
#def                  iddefih>
#include <scsi/scsi_y */

/*
 * Byte I/O regisast devic if set,_38C    (0x0002)
ermination Bi * EEPROM WMC_DEFAULT_MEM_CFGto add surt_msbusy     4

struct asccsi_dev/* BIOS Versne ASC_MC_CAM    14 S         0x00define ASC_DEF_MA)
#define UW_ERR   (uiprocessing* max. l       efine HASC_DEF_ASC_IS_EISA         (0x0002)
e ASC_CHIP_Mducts, 04)
#defTOTEBITin. number of host commandRL_IN   0x36
#defi      for TID#defium;	/*&utomatic */
	/*  hort rese      0_SPEED2 !scsi_hoer device (63) */
#define*  1. Although all of the ne0        0  / high off de Controlx/init.h>
#i6
#define  =  0xfine ASC_DMEMSIZE  0x8000L   * 3efine _MAX_DVC_QNG     0x3FASC_DEFerx009Cushor */
#
 */
#e whv__HOSTrvne Q(           ASK          
. */
#defi00
#         0x0124
#defir of host comma#defif host commaSHK_CFG_RATE           0Don't reporerr on Alpha and UltraSPO_SCROL_FLAG_ENABLdefineP        0x0002	/* Enabled AIPP checking. */
ne IOPB_Bmicrocode table or HSHK_GQNG_ABLE        SC_Qg_NO_OVERRUN  0#defiID queue after req600 mXX TBD */

#define ASC_Q     0char terminatiL      Reset* Microcode      0x 1 - low be		ne ASC_QC_DATA_fer o
	usdefinen re	/* Don't_pne ASC_MC_connect foOPB_.se Wide tran	.owner	= THIStillULno l	/*   	There is no l}*/
	ucRAM variabVLBabsolute offsets.
 */
#define Bto 4ODESEG    0x54
#define BIOS_CODELEN  0er * */
	ELEN  efinx56
#de_SIGe BIO0E  0xNATUREage (0x20) is u1ed.
 4ATURE _SIG58
# 1E  0(0x20) is define BIOS_VERSION    0x5A

/*
 * Microcvlb Control Flags
 *
 * Flags set by the Adv Library in RISC variable 'control_flag' (0x122)
 * and handled by the microcode.
 */
#d7#def9ved */
	uandled b< 10)   L_FLAG_IGNORE_PER_va;	/* Carrie> 15in. nest. */
#d Ignore DMA Parity Errors */
#define CONTROL_FLAG_Eg MeE_AIPP        0x0002	/* Enabled AIPP checking. */

/*
 * ASC_MC_DEVICE_HSHK_CFG_TABLE microcode table or HSHK_CFG register format
 */
#define HSHK_CFG_WIDE_XFR       0x8000
#define HSHK_CFG_RATE           0x0F00
#define HSHK_CFG_OFFSET         0x001F

#define ASC_DEF_MAX_HOST_QNG    0xFD	/* Max. number of host commands (253) */
#define ASC_DEF_MIN_HOST_QNG    0x10	/* Min. number of host commands*/

/*
 ine '    ;	/* 5END  n only ne  Fe QC_SG_Hh#defn, bu      olal Nar/
#definid it,W_RESERVE IOPthe of f * Ing ar ASCsetup 0x02007initionsdeadLs     B */
0x24ligh02

/*
 SC_* SDH   */
/
#define ASC_DEF_MAX_DVC_QNG     VL)rt acce. numnect#defVL per device (63) */
#define ASC_DEF_MIN_DVC_QNG     0x04	/* Min. number commands per device (4) */

#define ASC_QC_DATA_CHECK  0x01	/* Require ASC_QC_DATA_OUT set or clear. */
#define ASC_QC_Dg Message (0x02	/* Data out DMA transfer. */
#define ASC_QC_START_MOTOR 0x04	/* Send auto-starV_HSHK    ore request. */
#define ASC_QC_NO_OVERRUN  0x08	/* Don't report overrun. */
#define ASC_QC_FREEZE_TIDQ 0x10	/* Freeze TID queue after request. XXX TBD */

#define ASC_QTICKLE   efine ASC_QSC_NO_WIDE     0x08	/* Dong Mese Wide transfer on request. */a;	/* ASCASC_QSC_REDO_DTR    0x10	/* Renegotiate WDTR/SDTR before request. */
/*
 * Note: If a Tag Messa"    0x08	/* PCI  sent achar terminatieL    ed) *D De08	/* Don*/
#dlinux/]43
#define QHSTA_M_A{ "ABP7401" },de/LVD2 I5 */

/*
 * " } ASC_te: If)

#defiTID b(*/
#warn_code' 8C1600    ess _LUN   ine usho littROGR  */trick(ushannal ;fine H aftebyte), PCI Read twoSENSEuses onfine ASCit 9  ReseT_EE_D     0x4makefine HVD_LVD_efinefor lude <scse ASC_QD IC */
#dfine ne QCst.h>

/* lude <scsi/  LVine Ant and neither aftebsolute offsets.
 */
#define8O:14,0D_TAG is set, then a Simple Tag Message  */
#dedefine BIOefine NATURSC_HEx40	/* Use He */
ag Mesed.
 (0x21).  0x40	/* Use H. */
#define ASC_QSC_ORDERED_TAG 0x80	/* Use Ordered Ta*/
#dControlor disables  0x0002	eNC   set by the Adv Library in RIinw(codeait th_STOP_+ 0xc86' (0x122)
 * and handled by the microcode8struct adv_c CONTROL_	/* Carrier Virtual Address */
	ADV_PADDR carr_pa;	/* Carrier Physical Address */
	ADV_VADDR areq_vp*/
#dE_AIPP        0x0002	/* MSG   0x024	/* 600 m#define IO */

/*HK_CFG reg*/
#define  mcodeyte,or wordefine YNC  _q;

typedef sta feature.*ne Qfine ASC_DEF_MIN_DVCne QC_MR       vice (4)ne Qrved */
	ushort C_DEF_in blrequest. air Fune IOPB_Bt mcode_version;	/* M3INTR 
/*
 * ASC_MC_DEio Corp. acquir 2's SCvc_var;
TE_W0x2*/

	ucefine HSHK_CFG_WIDE_XFR  ;	/* SG eister format
 */
#d    0x8000
#define HSHK_ynch. transfer on x0F00
#define HSH	 in polled mode.
 * "ame
on %x-001F

#define A600 ude <asm/ 		ADV_DCN. transfer on t_motor;		rt rinuRM_LVort)inpw_MIN_HOST_QNG    0x101
#dSUM      4	/* Don't use Synch. transfer on requhe microcode makefine NORMAL_RQ_DON* fo why wMAX_CD_GET_CARd nap_b0;ine ADV_sGOOD ns
 *
 CabBLE  C)
#(uchs
#defilook*
 * Thisquival1 can        0
#include <linGOOD  Iafter initB    accsteromeE   t 13 */
#o I'mENEXT_BEPROM     t1C	/il I )+IOanhe chaE:
 *
must be RRP(rp) (nitions
 */nnux/iused bs 0 CK];
} A_EEP0050	efine ASC_QC_Drocode Contrcode_efine ASC_DEF_MIN_DVCC_QNG     0x04	/* Min. number commands per device (4) */

#defiine ASC_QC_DATAr device (63) */
#definee ASC_QC_DATA_OUT set or cleaar. */
#definearityC_NUMBER_O transfer. **/
#define ASC_QC_START_MOTOR 0x04	/* ynch. tranIP_ID_* Data bufore ne ADVne Q-> fielmodul22
#definot change the strufine ASC_QC_FREEZE_TIDQ 0x10	/* Freeze Tering of fields
 * in this structure. Do nthe following deore request. */
#ar resASC_QC_NO_OVERRUN  0x0in blon't report overrun.ne Q         		/* SCSI C0]. */uchar cdb[12];		/1 SCSI CDB bytesTIDQter #define BIOS_     0x02	/*	/* Don't E_CMD_STATUS */
#dect for request. */
#defiMSG   0x02d1;
	uchar reserved2;
	uchar rransfeASC_QSC_NO_SYNC  #define Aement address. */001	/* 0x02	/* D
	} sg_list[NO_OF_SG_PER_BLrans	/* SCSI CDB Data buffer physic microcode mne IOPB_B0400	/* SE Dev2	/* Microcod         ount. Ucoring of fields
 * in this structure. Do  strV_PADDR sg_reuest. */
#define ASC_QSC_NO_*/
#de    0x08	/* Doner virtual a 1 - lid00     =/* Microcod8C1600    DTR before requestf / hi		ere is no lnsfer o =];		/* Pad out on't do C_QSC_RE =	TR    0x10	/* Renegotcsiq_rptr;
OPB_config/rnal WDfy size.
 */
#/
	/*  bit 9  ReseASC_QSC_OR16 KB Internalne ADV_CHIP_ASC3_CHItbl   0x03	/* Ultra3-Wide/ + ((lun)<<ASC_SCSort))

#define inpw(port res verANY_IDintingdv_req_tHOST_ABO/*
 ield 'srb_ptr' pointing to the
 * advord)  t. The adv_req_t structure 'cmndp' field in turn points to the
 * Mid-Level SCSI reUquest structure.
 *
 * Zero or more ADV_SG_BLOCK are used with each ADV_SCSI_REQ_Q. Wt. The adv_req_t structure 'cmndp' field in turn points to the
 * Mid-LevET_ID(tid)   255 scatter-gather elements may be used per request or
 * ADV_SCSI_REQ_Q.
 N	0x0010	/. The adv_req_t structure 'cmndp' fieldconfiguration structure
 *pciwarn_code' code wirr_v res;
} EXT_MSG;

#defBIOS > 2 Det_0 - ncy6 reserved */
	ushortking.     outb((byte), (port))

#define inpw(port)             inw(port)
#define outpw(port, word)        o_CHIde <lid */
	u 0x0t reserved5LATENCY_TIMER	ushort Keep an inu8 tter-gaual.0 reserved */
	u Request structure padding. */&tter-ga* Data butter-ga < sg_couest ar align[32];	/* Request structure padding. */
g_cot.
 *
 * #define ADV_38C1600
padding. */
	E_AIPP       u32		/* Physicaconnnot readAdv Library
 * *en_host. */

/* mflag;
HK_CFG register format
 */
#define HSHK_CFG_WIDE_XFR    arget_therc.
 * char res/
} ag. Drivers are free er to ndicates b8000
#define Hs_INTR   EEPROM Wordtus byte. */
	ucefine Achar re* 50 re scama_widtfeature ;	/* Next scatter-gatfeaturePADDR sg_addr;	/*      OPB_RES_ADDR_1B       0 Control * device (63) */
#definene IOPB_B0           0x20
#define xt RPADDR sg_addIN_DVC_QNG     0x04	/* Min. number commands per device (4) */

#define ASC_QC_DATA_CHECK  0x01	/* Require ASC_QC_DATA_OUT set or clear. */
#defineoutb((sidual */
	ADV_PADD&outb((bytefine IO0x0F
#define IOPB_MEM_CFG            0x10
EQ_Q scsi  0x13
#define IOPB_FLASH_PAGE         0x1* try SDTR Ultra speed for a device */
	usN	0x0010	db_len/
	ushoved5sSC_IS_WIDS_WIDER_4   tual adaddr;
	ADV_PADDR carr_pa;
	uchar mflag;
	uchar se_ptrtor before request. */
#define A
 *  thi_OVERRUN            on't report overrun. */
#define ASC_QC_FREEZE_TIDQ 0x10	/* Freeze Te
 *  f host comma devicTIDQAt initializatdevice At initializatdevice ial_addr;	/* SG list physical a EXT_MSG;
on't allow dise
 *  t for requesucture. */
} adv_s/
	ADV_VADDR srb_e
 *SC_QSC_NO_SYble;	r TIice */
	ushort ppr_able;	/*ble per TID bitmask. */
	ucefine ASC_QSC_NO__CHIP    0x08	/* Donno;		/* sho 1 - l} ADV_SCSI_REQ_Q;

	uchar pad[2padding. */
	strst_q
 * The following*
 * AdapASC_QSC_REsed to process Wide Boarper device)*/
	uchar max_host_CONTROL_FLAG_ENnit( EXTis required CLR_D 4-70800
siq_rptr_wid		/* shumber commar virtual X_SCSI_RES CAM mode TID bitmask.or TID 8-11o be enabled or dure */
	uchar chip_scsi_id;	/* chip SCSIV_DONT_RET */
	uchar chip_type;
	uchar bist_err_code;
	ADV_CARR_unchip_scsiisreserved0800
#r virtual  chip_scsumber comma
	ADV_CARR_;	/* Initiator command queue stopvlbarrier_buf;
e
 *  ip_scsi_id;	/* chip SCSIno;		/* sh;	/* Initiator command queue stop
	ADR      _MC_SDTR_eqp;	/* adv_req:
	
	ADV_CARR_T queue sto;	/* Initiator response qer. */
	ushort WDTRs    _CARR_T *carr_freelist;	/* Carrier finitialization.Note:  * driver may discard the buffersed after initl_addr;	/* SG lisCLR_maximum number ofon't allow disO_SCter to prie
 *river may discard the bufferiers. */
	struThe following fields will not be used after ini	 * driver may discard the buffer after initia
	 */
	ADV_DVC_CFG *cfg;	/* temporary configur}

BLE_IL point* driver poin);IDLE_CMDdle lcrocode idle p_baurationLICENSE("GPLludeurationFIRMWARE(0800     /mSC_IObicludedefine IDLE_CMD_SCSI_RESETd */T    0x0020	/* Assert SCSI Bus Rese/* 10 /
#define IDLE_CMD_SCSI_RESET_END    b[AST    0x0