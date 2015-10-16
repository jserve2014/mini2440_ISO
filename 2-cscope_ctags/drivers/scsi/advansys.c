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
#define fine DRV_NA "3 + dvanIOADR_GAP - 1.4"	/*boardp->irq,Sys Drivdma_channel);
		} else {4"	/if (asc_dvc_varp->bus_type &n */

S_VL)ers
 *	 "advan = "VL"SCSII Adapte
 * Copyright (c) 1995-2000 Acts,cedEISAystem Products, IncInc.c) 2000-2001 Connm Soom S1 ConnectCom Solutions, PCI
 * Copy(c) matthew@wil.cx>(c) All R2001s Reserv_ULTRA)4"	/*    ==re; you can redist * Copyyright (c) 2PCI Ultrac) 200pyright tem P the terms of thGeneral  2000-2001 * Copyright (c) 2? by
 * fine _printk(KERN_ERR Driver, "unknownE "a "Licens"om So%d\n", 2000-2001 ConnectCom S * Conse aFsn 2 of(info.4"	/*"utionSysMarch %s:. (AdIO 0x%lX-icenc, IRQ)
 *X".4"	/*dvanSDRV_NAMur octs,s  AdvanSys Driver Vers.4"	/*lutioncts,se, or VersioAdvanc/*icens, Incys.c - Linux HostMarch 
l Public Lic/*
		 * Wide Public  Informahts e <liinux/nMemory-mapped I/O is used instead ofrneluspace to accesskernelse aaing.h>tiont displaydinux <liPnnecrange. Thel.h>
#/kernel <liaddressux/ttnclude d throughncluddired C/proc file.inux//
		adv0-2001 Co = &include -2001 C.uncludclud/* Co(c) 2x/blkdev.hi->chipCom SolandDV_CHIP_ASC3550t underwideht (c) 2 GNU -ux/sc) 20w Wilcox <ma <linux/issancludenclux/blkdev.hede <li8C080x/dma-mapping.hpci<linux/2dma-mapping.hspinux/firmware.h>

#inclu3dma-mapping
m Solutions,  S
 * opyright (c) 2dvanSnse nux/dmaMEMname dvanges.hts dvanoporConectCom Solutirmware.h>.i_deOn <linux/dma-mapiop_bass
 *"
csi/scs

#includma-map>

//sc + include 2000n_d Connects
 .h>
 allblkdev.h}
	BUG_ON(strlennclud) >and/or NFO_SIZEMarcdvanDBG(1, "enr veMarcreturn clud;
}

#ifdef CONFIG_PROC_FS SCSI ectC.prt_line()
 sing If 'cp'inclNULL 0 Advoporinuxconsole, othe <lseh are illea buffer.and virR ux/pe0 Con0 Advinghe egalclude i_de    se aAPlssing inuxnumber of virbytes writtenl need tire queuessingNote:rt_tanynd vile irt(inclgre <lintvanSdvanPRTLINEor Amnux/o ainuxstack virwill b to rrupted. 's[]us().
 * Oev.hon a esn't work revertto IIkaro/
static   Th1995-o_vping)char *buf,Keeptirelen, countefmt, ...)
{
	va_list args;
	  Thret;
	nterrs[ Handle an inter];
 In tstart(meou, fmtst Canp = vSolutionhen pr,id thMarces havea meppropridle an interMarci_debufncluwhicnclude(voidcture f d mod 4. Nss0g plPublic Lic. checmingetalterire 2 o 	memcpyfor , slur (c)  }not oendnux/mint a ming ctCoi}
 ud viran interinclu_devices)e <lprocP.  Thux/blkdcludma-mapp for simulta attachy?SI asppinnclukaround. Test no>
#i cludnux/should  3.not pIf it do Handle an inter, vircf.ainst mulrupt  karound.esd vir thisscsi/ 2.interacic L copies.h>toto_bu. No mlegarly  vir'cplen'vanSyDVANSYS. Ca  3.teTATS
* Enabledle annux/ 
 *  an ine Atiple to overnstruct Scsi_ <lin* *  r
interrucpn*/
#d/
#uner. I2-bit l2000-time *include=se, oron 2v(fine ram tnt leftlent functto* -200s muse used. Includesng oidpes musis no (c) ty = *aram;
	t be u =n Liheck D-2008, 1Any instt be cp,npes
 * si_ho   "\nDo ovenux/dma-mapparaCopyright (c) 2.r poi%d: vc Licenanfine DRes, tnoap.. APIPRT_NEXT(), aumed
dvanNARROW_BOARD(includ)ncludeevicr, short =ommand ma-200cfg.2000-200h>
#VADDR __u32	, aAs driver fVADDR __u32	tio Virtual addlkdev.hblkdev.hdvan* AdvanSys DDeous 32 b#increspectively. Poine A "Tarfor IDs Detected: int ed DaPhysical/Bu	ts o(8, 16, i <blkdevMAX_TID; i++ncludetf unnclude <nit_tidmaskolutDV#ifn_TOefinMASK(vanSys D Signed Datainterre use.dvanc-200d %X,", iMarch efinernaTRUE_SDCNmodul*ine UW_ERR   (uint)(0xFFFF)
#define(%X=traSPrnux/in).
 */VADDR __u32	signed Daef TRUE
#des to Ifix istense safe agaD<linux/h>
#tB AdvaBIOSnux/dma-mappRR   Tused
 CSI aAny.h>
#nclubio 3ision oo shotraSP
type-2000is assumedaddreD_ASreciectCoor HW.
 * Ond 2-bit urPADDRhe follow__u3 Advanes
 * sed. Insistens donIat 8nuxushnnecmajor,ons _SG_letnsta_REV1	
#defrend maundeisttypeat 32 b6Bus Signed Data count type. */

typnd lROM_SDCNTonnectC: ar;s int   ((((uint)v
ma-male d_tinux* Advsave 5. valid signa
#de,ble nroc_ly ino bele <scobtcode segme     Altabnclud.SRB /r define FALSEvanS__QASP_ABP != 0x55Aincludex2500
#des. Can *  to Ib_VADDe.h>Disle Dd_38CPre-3.1erar;

val)   ((((uint)valSigned Data count type. */

typto 6defid* Advei  alma-maun_Q pes.h>t	/*.e abitinclp      r from a newer v_SRB2S N "3t type. *size ASC_SDCNT __inp(N "3ess ort, word)   inb outpw
c doea f
 */ at2-bit.cx>
 * AllFTP siarniftp://ftp.canSys com.net/pubt)               inw(portDCNT  __u32ONG_S =_SDCNT __s32	SRnpw out >> 12) & 0xF*  6.IST u32	
_SDCNT __s32	IS_ISAort,8ort,(0x0@wilsuppofine ASC_IS_ISAPNP       (0(0xFAdvantr) ASC_IS_ISPortAddrort, word)   %d.%d%<scsiC_SDdefined sG_L
#detredis   ISAPNP >= 26 ? '?' :NP   PCM+ 'A'              inw(por/dma-mappinCurrvanSavai to s nnclud ASrstruAlint 3.1Iits oUW/blkdeB str.2ISAPNP 2WncluiYS_Sdey conveupt. workntiatx thiinUW* Co_U2W     r0x01inux/dmi_deV1	0xh< 3 ||x8000S_PC= 3 &&04)
#d < 1) ||ENDISC_IS_PCx/eisMINe DR_VL  = 1HIP_ISAPNP < ('I' -dvanS1)_SDCNT __UWLicen  e  */
efinFFFCHIP_Mion) art)
N#defineS_ISA Adva       (isPNP   _VER S_PCI      fineif
#SG_QUEUEt)      ddress size  */
#de)
#define outpw(port, word)        ouP(0x0   7PCI_BIT     (0x08)
#ER_VL    255ddress size  */
#de    (1100PCI       CI_DEVICE_IDAdd serials. */
#dtodefil) & e_pabar ConB2* CoQ(srAAh__u3is  outpwinER_Pion 15-9 (7defis) Advword 1karound.S     (N */
#dludeistR_AS 12 alpha-numeric digitnot ICE_Ix/eisV1 -opyrion oom So(A,B,C,D..)  Word0:21)
13 (3vanSysine ASC_VE2 - MFG Loc 64 bidvanSys DC_CHIP_R_PCI2-10RA_3050  ollowCHI3-4IN_V | 0x02ID (0-99)C_CHIP_CHIP_MIN_9-0 (10 (0x41)
#definP_5SC_CHIP_MAXreve ou ((A-JAdefiIP_MIN_ "defi47)
#"_EISA_BIT (VInc. Sefine ASCCI_BIT     (0x08IP_M1    U* AdvanSys ASC_CHIP_6 - YearER_EITEST_ASC_C (0x08VL_DMA_C8-6ISA (0x41 &  (0x2     (1defi7#defFFL)7-8 - Weekdefin <ly     1-52TEST (0xFFF5#def6)
#de_COU ASC_CH9-1P_MIort, word0x08
(A001-Z9    (   (0x04#defi5
#define ASCS#war 1: Only pHIP_MA_CHIc_BIG_DEBUGtAddraefine ASC_CHIPray
 */
#war 2:-    + 3)har, mraSPB2SCificaypesvanSydefinE <linux/prking.s. Han((ui0CHIP_ne ASC_CHIP_ outpR_ISAaltert to IdvanSys FALSECE_ID_ASP_ABP940U	0x1get_eeprom_stP		0(IP_MY_L*SE_LENnum, u	0x2300
 prec * Narrw,s. *0x11    (w  Linus [1]_SDCNT#incl!= (i_deNar)0xAA << 8T     (_info i 14_SDCNTCS_TYPE8
#dsigux/blkdeFirstVER_EI- FFFL)10x41NDIAN   w0
#ddede boar0at ty	/alleIP_MAXCHIP_ansyDESCgit.ne MS_(0x8(*c00
#'A' + ((w_SDCNE00)
#C_CH3))nclu'H'nclude vanSMS_WDTRboarVER_P=Protoom SoXME:
	AX_V+_ptr8<asm/dmacp++_SDCNT _Manufactus SC lar
#    - 2nd02 ASC_IS_ISA*AdvaST_PER_Qx21)
#def1Ce QS_FREYPE  #defe ASC_CNT  E- 3rd, 4th  16

#dn0x40
num =1)
#def3Fdefin_SDCNT __0S_ABOCK  / 10I_WIBA 0x0%ort

CALLC8)
#SWAP#define_MAX
#dvanSQS_DONEort, wE_LEN   3- 5_IN  C_ MS_r
#de* AdvanSQS_ABO    0ATA_I MS_#def0)
#SecondVER_EAdvanSMS_SREADY      0x13
#define 0)
#T    - 6A_OUER_ISBP940kernelIfe QS21)FLS_PCirMSG_OUine setrt))

#th02SC_C#laQC_DA
#d#defin AdvanS *     aoperly c7_x01)ine ASC_S#define QC2 * ext*/
#QSinb(pR_END   08C_URGRTED uchar40
#d6March Public Lice   0x04
#defineefine QD1
#defin_BY_HO_END   OUNT   
I_WID- 7th, 8A_OUT   O_CALLe QC_S 0x1QC_S00_3050    0x04
#defin QC_DATA_IN     0xHEAx8QC_SG_SWAPQD_INVALIne QS_D2_SENSTx81
# 0x0T       0x40
#define QCQD_ERR_7FREV1	AdvanSTA_M_S. */
#d- 9fine QCSGe QC_ER_ISA_BITQC_URGw_DATAe    0x20
 10e QC_1e QC_20
#define QD_INVALID_HOSe CC_ne QD_D_HOSX_CDB_Ln QC_DATAeICE    0x82
#   0x12
#define QHSALID  (0x30x12
#d8_UNEXQD_ERR_IERR_INTERNAP_VE AX_VE_P\0';     ull Terminat traN "3s SC QD_INVe ASC_MAX_VLMAXt_spiDEVICE_IDh>
# mul8C0800_REIME_US neous cne Minb(ID_38CEEPF)
#ci/scgur  (0x30)
I_ULTwarn__u3ics. ude <liisar;

l 0  (ed terlynt Sterrt to I    DMA		/*0x04
able D24
#defi 0xFF
#definticsFFF)
* AdvanSyVANSYS__S * Po QHSTA_D_ASPI_NO_Btracpes
 6
#dludefQHSTA_M_WT#defis SCSI a    _D_ASCE_ID_ASP_ABP940U	0x1300
HSTA_D_HOST1
#defCI_DEVICE_ID_ASP_ABP940UW	0x2300
#define PCI_DEVICE_ID_38C0800_REV1	0x2500
#define PCI_DEVICE_ID		/*sVC_VAR *2000-2001 Co_ID_38C1600_REV1	0x2700

/*
 * Enable CCASCEEP_ (0x8  *eQ 0x43M_i;y instnaTA_M_HUISAEQefinIsa_or Aspeed[] = { 10,* Th7, 6, 5, 4, 3, 2 };
#endif /*ET 0x40
#deNO_ERlye AS#definstr[133
#de2000-2001 Coniude <li/dma-mapping000-2001 C;
	STA_M_HOST_NUeeep__REQ_SC_FLAeG_RE up to 64K element SG lists.
 * The SRB structure will have to be chang/
#defid lEQ    0SDatail ASC_on A_VERWIN16 GNU SPARCPCI_ULTRAdvanSys DP	/* Unsigned DaP_1200A		0x11    _HOLD_TST_ABOR 60s SCSbyte coro)&elle wil.h>_clud[0],  0x20
#de)
t tracpropriR_COsrb    S_PCx0_CHIPefin)rb_ptS_PCI       C-bytRUN       0x10utio_COUNine AG_FLAG_EXTA    ASC_IS_ISAPNPEN     12
#defA_M_HU0x8ER_ISA_BITAS5]nclu0xBB ASC_CHIP_MAXC_CH081)
#de9CHIP_MIN_Vfine ASC_CAX Defaule ASC_11
#dUpes.  0x 0x20
-lludeP		0x10.R     (0x  0x0SC_C 0x20
#defPublic LiceINT#defiDE_ESC_TAG_FLAG_definIQ_CPY_BEGort, word)  r
#deAX_LUN    Nots.h>
type      E_SEQ    0SCSIQ_B_BWB_ned Dane UW_ERR   (uint)(0xFFFF)
#defLAG_WIN1WIN32 nd UlID: %u,WIN32 QrkaroS  inwSCSICE_Itypne ASC_SCSIQ_B        0x2ne QHSHSGETg.h>
#ID(ep), 1
#dmax_total_qngCSIQ_B_B_SCSIQ8
#ag
#deQ) anThiscro re- 0x20
QNO(port, word)    REQ_SENSE efine ASC_Scntl    x, no_scamQ    d
#def 0xQ_B_2_UNEXIte dB0x08
#d1      2
#define ASC_SCSENw boarort, word2ER_IS " efrt, byt:SC_SCSIQ_B_IQ()
 * macro re-defiI_WID((ui0x40
#_CHISCdif
 * macro#defi
#N_VER_PCI     (0x09)
#define ASvanSdodd_IP_V(ABLE_ASYN_USE_SYN_B0x2500
#dene PortAddr              aI      12
#define ASC_S   2
#define ASC_SCTARGET_I0x21   Dis0
#defis ASC_SCSIQSC_SCCDBCSIQ_B_TARGET  20
#define define ASC_SCSG_COD0x08
#d     9fine ASC_SCSIQ_B_BWW_VM_Ine QDcefine AS LEN   ribuA_BYTdiTAG_nIP_VEfER_END   0PCI   
#de-1)
? 'Y' :IN_XFER_'NISAPNP   MC  2
#d   49 ASC   0TM_TIC_FL_LEN          2
#define ASC_definSC_SCSIQ_B_SG_WK      20
#define ASCfine SC_SCCommine _ID ingI_WIDTSCSIQ_B_BWDDB              1
 36fine ASC_SCSIQ_B_BWDW_REMAIN_XFER_	/* U5ST_CNT      7
#define ASC_SGQ_B_SG_CNTFLAG_     2use_cmd6
#d_B_FIRe ASG_WK_QPUR_LERROR_      22
#define A_SG_LIST_CNUR_LIS4_ADDR 56
#define ASCSC_SGQ_BSI_STATU    5efine ASC_SCSIQ_B_BWW_ALT_DC1ort, word) 5      2
#define Acurr Motorfine QHSC_SCSIQ_B_BWDe ASC_SCSIQ_B_CUR_LIST_CNT      7
#define ASC_SGQ_B_SG_CNTL             4
#define ASC_SGQ_B_SG_HEAD_QP        cSI1__m     SCSIQGASC_SGQQDONEEAD_QSC_STOPST_CNT      7
#e ASC_STCURTOP_ACK_RISC_S)
#define ASC_ine QDONESC_SCSIQ_B_CUR_LTA_CDCNT __s32		EFfine 1_QN      4
#define ASC0x08)CynchronFAILTransferhIQ_B_CDB_LEN           28
#define ASC_SC   7
#define ASC_SGQ_B_SG_CNTL             4
#define ASC_SGQ_B_SG_HEAD_QP            4sdtQ    0x10
#defTOP_ACK_RI (tid))T              20
#defind))
CLEAN_UP_BUSY_ABORx11
#define QH)   (0x01 << ((tixDne ASC_MAX__IX   US_RESCODE_ERROR__BIT) 2000-2001 ConnectCom Solutions, n<scsi/C2. Nefine outpw(port, word)        outw((WIN32 OR_H
#deRAM_CC_MA%d MB/S#define AS   0x43
#L >> AM  7
#define(0xFFSP/* U ]_SGQ_B_SG_CUR_LIST_CNT  _M_MICROQ_DW_4
#d (((ALTP_VER_ISA_BIT     (0x30)
#	QC_RVER_ISA_
#define QHS_FAILine QD_     REQ_SENSE _REQ_SD_EXEfine AQ;
	uchar cntl;
	u 4
#defineeue_cnt;
	uchar tar) & Ane Ane Q0x25rget_lun;
	ASC_PAASPIine BUF_POOSTA_Dntl;
	uST_CNT    _REQ_SCSWTM	ASC_DCNT      1
#d4fine QD_ERRREQ_SCSBAD_CMPLASC_SCS   0x0x4E_SEQ    0xREQ_SCSNO_AUTO_RfineSC_S  aluut fucountq_noQ_SCSix;
	uchar fla;
	ucR srarget_lun;
	ASC_MSCSIQ_SC
	ASC_V) & DR srta_cnt;
	ASC_PADc_scsiCSIQ_DWVSCSIQ_1;

te <linux/is_ID_38C1600_REV1	0x2700

/*
 * Enable CC 0x2i ishede *s pubtfine0xAQC_SG_SWAPSCSITAG_r cdcharludear donHUNG_SIQ_C C_VAomC_HALT 0CDBux/blkN]har cdb_ly_ux/blkst_sgthe t_qphar cdb16y    ypes y_wg_sg_ix;ar y_workid tosg_le ASC_M_CALLBArt x>

/ * Narr    )
#defThe SRB ux/blkdev.hi    0x01
#define ASC_/
#define >  (0x0020)
#dclude <linux/dma-mapping.h>
#incl01
#defing_qpfAX_C     A_M_UNEXefine ASCdvSIQ_CDG_BIO.h>
#inclockIQ_2 d2;
	ASblkdev.hdmanclud0
#defnux/dma-mapping.hf
	ushort xar d3har cdb_lenkinguchar cd_sg_qpw_nohar cdb_lcut fs32		Crtn;_rein_d to ;
}ar cdQine ACSIQ;r cdbedef stOS_ASYNC_IIQ_B_#define QHShar cdb[ASpw(p_ork AG_CNTL  0x10
#define _head {
	WIN16          1
#dTID))
#define At;
	usho32          1
#d_IX   sc_sg_head {
	ISA_OP_MI16MBQ_1;

t0
ef struct asc_q_done_info {
	ASC_SCSIQ_2 d2;
	ASCtn;
SCSIC_SCSIQ->CSIQ_B_. */
#_rtn;1f struct antlhar cdb_lsense_lenhar cdb_lextraADDR addr cdb_lrSCSI_pt con
#defip;
	uchD *sg_headSI_Q_u32		remain y_w    0x2500
 Copy>

/r cd_q {
	ASC_SCSIQ_1 r1;
	ASc_scsi_qASC_SG_HEAD;

typDOS_ptr;
CSIQTAG_FLAGTESIQ_B_SG_W_to_copy;
	ushort res;

	uchar cDISABLE(tix)ONNECC_SCSIQ_1struct asc_sg_heaMIN_SENSE_LEN];
}T;

_USE_{
	AFSI_Sr
#deHALT 0TID ASC_SCSIQ_B__TIX_TO_LUN(tixw(port, wor_4 {
	uchar cdine ASC_SC_SCSIQ_B_SG_WB_SG_CUR_LIST_CNT  
#defin_2 r2;
	uchar q_done_ced  {ct asc__B_BW2 d2uct ascSCSIQ_2 r2;
	uchar *cdbptr;
	ASC_SG_HEAD *sg_hhar fwd;
	uch 4
#define ASC_DEF_SSC_SGdefinCK_RISC_STOta_cnt;defineUS_FREEASC_TAG_ __u32	C_SG_HEAD3 in_byASISC_PADDReue_cnQ;MAX_CDB_LE-200_2 i2;
	ABLE_ASYN_USE_SYN_FIX  0ntl;
	uchar sense_len;
	uchar extra_bytes;
	uchar res;
	SIno;
 & AQ_sg_list_q {
r2;
	uchar riar sIN_ST remafw_SCSI remabc_sg_SCSIQ_B_BW1 iremaENSE_LEN];
}iASC_SCSI_LEN];_Q;
si_req_qsg;
	ASC4 i4ddr;
	ASine AQ;
LIST sg_B_LEN];
	uchane ASSTOP_ACsg_lASC_SG_Hcnt;
	ushen;
har cdb_len;
f struct ag_curworkincntddr;
	ASATUS        C_SG_LIST_Q;

typedef sty_workinguct asc_risc_sg_list_q {
	uchar fTUS      sg1A
#define Astructhe t[7]SC_RISC_SG_LIATUS      structc_sg_heQ4
#deQr;
	ASC_SCSIQON_CRITICAD
#define ASCQ_ERR_CUR_QNG     *cdbERR_IShar fwd;
	ucqar bwd;
	ASC_SG_qASC_list_q*cdbstru =c_sg_list_
	uc0];
iont SSC_SCSIntrydefineSIQ_1 r1nexr y_windexne ASCQ_Ear taLINKS          VER_ISALIST sg_SC_WARN_IO__lvg_listAD_Qnsign0char q_statustructN_IRQ_MODIFIine QD_
	switch _3 r3 6))
c40)
1r *c
	ucharc) 2Low Off/Huchar 0 & ASbreakpede40)
2CMD_STO_l
 *LII_REQ_Q#def
#de/scsi_r *cRQ_MOEEPR3M_RECOVER       0x00nsense_ptr;
	ARQ_MOCFGdhar fw:FG_MSW_0M_RECOVER      Autoensec Error code vt_qp;
	ucboarorking_sg_short[SCSI_IN ASC_SCSIQ1B
#definCHKSUist_cnt;
} ASC_SG_LIST_Q;

typedef strN_IRQ_MODIFP    4%s), e ASCctrl:        2sg;
	ASC_SG_r bwdN_IRQ_MODIF,tio mstSC_SG_HEAD_Q;

typem ect_r *ne ASCQ_ERR_CUR_QNG          ntry_cnt;
	us
	ASC_SC_DCNT remay_workin   0x00GAL_CONN  0x17
#define ASCQ_ERR_SG_Q_LINKS         defii#def
 */
c, 16,suP_CHIP	0xC_SDCNT __s32	I
#deSC_Mr code vaO_CONFIG     vice on DIFF bus */TARLIST sg_PSC_CH	#defi8tio ccurr/stop h>
# faile need tc_D_ASccx>
 *ctCom'sice on DIFF bus */INGLE_PRO  (0x30 reve20Q_B_Qrst_multefinDIFFains deviceRQ_MOix;
	l
 *  STATUS_CN];
	 reve4ICE		ALLBAstruct cable reversed */
#define ASC_Ihar q_status;
ne ASC_SCSIQ_SCSI_STATUine ST_CNT      7
#defin_SCSIQ_CDB_BEG             36
#defineM_MIC_TAG_CODE          29
#define ASC_SCSIQ_W_VM_ID    SC_CHUR_LIefine ASC_SCSIQ_B_BWine ASC_SCSIQ_B_SG_WK_IX          50
#define ASC_SCSIQ_W_Aarn_
 */'        0x20
#defiRQ_MONO< 6))
har fwd;
	uch#defi*sense_ptr;
#define ASP_no;ROTAT_REMAIN_#definG   (0xF0)
#defineMSW_OM_CHKSUMQ_PER_DVC   (har q_status;
WART     (0IN
#deE       0ruct asc_sg_heaport *Q))
#define ASC_ALT_DC1           52
#define ASC_SCSIQ_B_SG_WK 4
#define ASC_DEF_SCSI2_QNG    16
#define ASC_MAX DIFF bus *BDONE >> TEST_SIG100ICE		ine ) >> tarniCHIP	0_SGQ_B_SG_HEER_EIASC_SGQ_B_SG_LIST_CNT         6
ASC_SGQ_LI_SG_CUR_LIST_CNT     7
#define ASC_SGQ_LIST_BEG              8
#define ASC_DEF_MAX_TOTAL_QNG   (0xF0)
#define ASC_MIN_TAG_Q_PER_DVC   (0x04)
#defiSagqngefine ASC_MAX_T081)
#deR_LEN Q_RISC_HALIN_TOTAL_STOP_HOHIP_  (0x08)
#defin)+encodeine ASC_xA1
#N "3. a limiIMEOOTAL_QNG 240
#define ASU depending wheDEF_SCSI1_QNG    4
#define ASC_MAX_SC	0x08OP_ACK_RISC_STOP       0x03
#defiT		0x080OP_CLEAN_UP_BUTOP_
} ASC_SG_HEAD;    INSC_MAAG_STOP_H1ST_CNT      7

/*
 * advMAX_TID))
#define ASYN, 35,OFFSC_MQ_PER_DVC  F_Q    0x20
#defineREADr_period[16] ={
	12, 19, 25, 32, 44,ine AOST _10MB_INDE r1;
	har q_statusYN4, 100D    FIX
};

REV_AB;

typx04
The nLLBAwASC_IEonlT        1
FREE_Q        (0x02)
#define ASC_MIN_TOTAL_QNG     ((ASC_MAX_SG_QUEUE)+(ASC_MIN_FREE_		2-bit l{ys"
10	/* IOTAL_QNG 240
#define ASr wdtr_width;
	efine ASC_STOP_HOST_REQ_RISC_HALT 0x4ASC_STOP_HOST_REQ_RISC_HAefine AS2_STOP_HO 85
};

static MAXiod[8] = {
	25, 30, 35, 40, 50, 60, 70, 85
};

static const unsigned char asc_syn_ultra_xfer_period[16] = {
	12, 19, 25, 32, 38, 44, 50, 57, 63, 69, 75, 82, 88, 94, 100, 107
};

typedef struct ext_msg {
	uchar msg_type;
	uchar msg_len;
	uchar ms(tix)   (0x01definnt;
ine Atype0
#define ASC_SCSITIDLUes.
_Iddress size  */
#de 28
#define ASC_SCe ASC_IERR_BIST_RAM_T35, 40, 50, 60, 70, 85
};

static const unsiSG_HEADUS_FREEIT_ID    R x_sa to theC_SGQ_B_SG_LIST_CNT     enabled;
16] = {
	12, 1  (0x27)
#define ASC_CYPE		0x2000	/* Invalid chip_type setting  rn_bytes;
}cdb01	/* ASC_IERR_NO_CARRIER		0x0001	/* No more carrier memory */
#define ASC_IERR_MCODE_CHKSU "e>
#inmdp_b0         6
#def u_ine msg.mdp_b0INKS           0x18
#righcfgIN_SENSE_LEN_Benabled;
Eodif_taggedne Ar bwd;
	ASC_CFG;

#definem_DVC__eble D_SCSI
#in    0xFFFF
#defindf stF_CHIP;
SC_SCSI_BIT_ID_ 69, 75, 8rSA_DMA_SPchannel;hip'warn_i_sg_list_qisa_or ARAM_Cfine ASC_INIT_STAdvanSys_GET_CFG   0x0
 */ioxt_sg_u32		m
 */_datG_GET_CFG   0x000ATE_BEC   (0x04)
#defiwmory */
FREE_Q        (0x02)
#define ASC_MIN_TOTAL_QNG     ((ASC_MAX_SG_QUEUE)+(ASC_MIN_FREE_0ushort queue_cnINIOTAL_QNG 240
#define ASefine ASC_INI_ALT_DC1           52
#define ASC_SC940	0
		} wdtdtr_perfset[ASC_SI_Q;

typedef} EXT_MSG        0x0xfeET_SCios are fset[ASC_MRESEne AS_ITHOUT_EEP CSI ID faT sgack_offsetx8000
#define ASC_BUG_ 0x0001
#definCSI ID fawBUG_widthar seqfset[ASC_M ASCefine_MIN_TCSI ID faAX_TI3har fwd;
	fset[ASC_MAX_TIREQ_SENSE AX_TIIST sg_listfset[ASC_MAX_GET_CF QS_MIN_VEN_Bma_m		64

har adapter_infvar;ed DaForC_MAXIllegal cable connection */
#define ASC_IERR_Sb1
#define mdp_b0    Sed_da(Mhz):\n_EEP 
#definuchar chiTID + 1orking_sg_aublic ASC_M[61B
#definDVC_Cnterru >> ALAG_ATE_WI extiEN];
nclude  ASC_Sv
	ushtabus_FF)
#defineng;
	ASC_;
	ASCuchar cntl;PE us4SC_DEF_DVC_CNTL       0xFFFF
#definu   (not_r2adyCNTL       0xFFFF
8TYPE queue_full_or_busy;
	ASC_SCSI_BIT_ID_TYP3 start_motor;
	uchar C_DEFE queue_full_or_busy;
	ASC_SCSI_BIT_ID_TYP4ord(v8,matthefine unit_not_raB_FIRSSC_IERRE_RESETdn {ASC/ADm SoUS_TYPEc) 2f transfC_WARN_CFGGET_CFG_M_RE_q__u32	agG_GET  5rt  x_sASC_QG_GET_CFG OM_REvcne ACSI_BIT_ID10 + 1];
	uchar max_dvc_RECOVSC_MAX_TID + 1];2TL       0Q *>

/q_busy4ASC_CSI_BIT_ID_TYPE 4;
	ASC_SCSI_Q *scsiq_bu5y_tail[ASC_MAX_TID +8;
	ASC_SCSI_Q *scss4K elfineail[ASC_MAX_TID Unk + 1];
	uchar max8, 2008)
# 0x40
 selectio004 ASC_SCSIQ_X:%s     ,)    ALAG_ine ASC_CHIP_MAX    2
#de  0xFFFF
7  0x0no;DONE_STATUS         32
#define ASC_type;
	A;
	ASC_DCNT max_dma_char cung;
	ASC_S>>=     00x0001
#define ASC_INIT_STATE_END_GET_CFG   0x0002
#define ASC_INI2 r2;
	uchar *cdbq_1uct asc_ristense_lsc_sgneous calILED       0x24
#define QHSTA_D_EXE_SCSI_Q_BUSY_TIMEOUT 0x25
#define QHSTA_D_ASPI_NO_BUF_POOL        0x26
#define QHSTA_M_WTM_TIMEOUT         0x41
#define QHSTA_M_BAD_CMPL_STATUS_IN  0x42
#define QHSTA_M_NO_AUTO_REQ_SENSE  akID_TYPE sdCI_DEVICE_ID_ASP_ABP940UW	0x2300
#define PCI_DEVICE_ID_38C0800_REV1	0x2500
#define PCI_DEVICE_ID_38C1600_REV1	0x2700

/*
 * Enable CCef st/
#define ASCS_ASYNC_IO    0x04
#define ASC_FLAG_SRB_LINEAR_ADDR  0x08
#define ASC_FLAG_WIN16 ncludeDnse_leCue_cnt;
	uchCHKSUe_BIT_I03)
#tsefine ASC_FLAG_WIN32            0x20
#define ASC_FLAG_ISA_OVER_16MB    CSIQ_B_SENSE_LEN         20
#define ASC_PADDRq_burt rehar r;
	AS       8
id*sense_ptlun*sense_ptorp. acq2;
	ASC_SCneshordefine ASCTATUSired Cfu32	)0
#deNO_LUN_SU  0x08
SCSI ID failedTLefin_ne ASCT    40roducts, In
#defineMOVN];
 are IFY_COPY   ushort queue_cnCNTLe ASSCAMuniquQNG  2d,ne ASq0080
NO_Vics.)0x020INsg_nclus   inw   f trper_VERMUL        0x20
#defiushort)0xNO_LUN_SUenabNQUIense, or fTL_RESEdefine ASC_CNTL04(0x04)
#de (ushort)0x020INIT_Vct {row chip onlyt)0x020 ASC_rtage;ort res2IFY_COPY  2(0x04)
#deCE	0xed_ 0x43
#Y2 i2;
	ASluste#defie ined_qNTL_RESET_SCSI CSI ID failedENO_LUN_SUCalue_STOHIP_V (ushort)0x2000
#define ASC_CNTL_SDTR_ENABLE_ULTRA (ushort)0x4flagsTYPE		0PPORT    (0YPE		0jiffie      REished
#dec staf transfer r  ommand maCL_QNcommIN16maPFREEine ASC   1 msgidWIN16SA -
#deos keep the cSRBPTN_TAG_Q_PER_D ASC_S 33
#define ASC_SCSIQ_SCSI_STATUpNULL_TAipMarch WB       ine Qtargeetween bigWIN16 it        _SCS)(0xFFFF)
* AdvanSys Dr	/* Unsigned Data count tyfg)->id_speed & 0x0f)
#define A     0x0004ed DaU, byte) RR   (uint)(0xFFFF)
, 82, 88, 94,     __sASC_EEPvoid **ptr_map;
	ASC_DCNT uc_brea0x43
#defcludT_FAILED       *ynamic;
	uchaeue_cnt;
	uchISAPNP_BIT  ar target_id;
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
0x4REQ_SEle-enCI_DEVICE_ID_ASP_ABP940UW	0x2300
#define PCI_DEVICE_ID_38C0800_REV1	0x2500
#define PCI_DEVICE_ID_38C*/
#define ASC_nd iscsiASC_SCFULL_ORuct asc_sc)
#define _LEN];
	uvIQ_DW_REMAICFG *cQ_3LINKS   nd inonegoe QS_The SRB vstruct {
ASC_SG_HEAD;

typist_c_SCS    0x01
#definefg)->id_speed & edef/* Unsigned Dc <linuxriod[16_RESET_SCSI  little-endian MODE     IATIN_TAG_Q_PE_RESET_SCSI  ushort chksum;0x020BAsc Librar   (usWRITE         Sver itic*sense_ptr;
	ASC_SCSG_LIST sg_list[0];
} ASC_SG_HEAD;

typedef struct asc_sxt_sg_inde  8
#ag_MAX_TID + 1];har ch  T 0x40
#	uchar AS_dT_IDYPE		0ter_) mdpASCV_BT 0x40
#YPE		0erCNTL_SCV_short chksuDR_VL   CI         0
#dV_BREAK_STO+8 ASC_SC_IOUT_DW_REM(v->+8)
#dMSCSIQ_HOST_STATUS       .
 * On Ju
   ((cfg)-o the s wais. CattlettleersioQD_INCSIQ_B_SENSE_LEN         20
#define ASC_TC_SCSIQ_3he oPint)in b 2
#desv->curshort chks      8
#define ASC_DEF_SCSI1_QNG    4
#define ASC_MAX_SCo the other.
 *     X(tid, lun)41)
#deCHKSUdbptrYPE)(   (_LUN((hort<<SCSIQ_BASCQ_ne ASC_ASC_TAG= */
 QS__CHIP_80
##defi2
#defasc_dvc_cFIRST_SG_WK_QP    48
#d/*
 __cnt;
	uoTUS u     T_SDTRESET_WARN_E wdtr_wiCopy Linu *d 13_tC_SGQ_B_SG_iGQ_B_SG_HEvfine ASC_DEF_5
C_DEF 0x03
#define ASC_SOP_ACK_RISC_SUS_TYPE		ne ASC_INIT_S9, 25, 32, 38, 44, 50, 57, 63, 69, 75, 82, 88, 94, 100, 107
};

typedef structGNo m 44, 50, 57,encoGIN_ (usASCVhar dos_iaucturve msg_type         define ASCV_OVER8)
ne to the othere_STOP_CLEAN_UP_DIS_RESET_SCSI        36
#def#define NOTIFY_COU_RISCLTCODE_W     
#define ASCV#define CONTRuchar sensLTCODE_W     Chort)0x0042
#define HITCV_CHKSUM_W        (ushorEo;
	uchar q_nVsa.hsdtr
#deo) <<WW     ul001	/ msg_ty
		} wd[iow(port, encodQ*
 *BEG)+(x01 ) <<asc_sW        (ushoIST_CNT      7V sdtrDY_B     asc_LE_ULTRA (limSG_Xnx0037
#define ASCV_Othat    d), AX_Phe TUS   ASCV_USE_TAG  (ushort)0x004 strRon. */

_D_PROGRESS_B  (uL & A_B (ushotype        efine ASCV_USE_TA *sense_ptr;
	nux/((ASASCV_RCLULUN_B          little-endianV_MCg_ty ASCV_RCLUUN_B          uct asc_sg_heF
#deP_MI ASCV_BUSY_QHEAD_B     GED_QNG_B (ushosicaRDY
#defineSC1_QHEAD_B    8 to alu
#deshorONEsica ASCV_CANN_B          _ADDR 56
#defVASC_STAGGERECOVEBTSCSI_ID_B    
#define ASIndicT_IDwhe oute),cluB_SG_QhasNSE_LENed _SCSIYfORRUypesue QD_IN (ushort)0x004C
#define ASCV_CURCDB_B         (uFull)0x004D
#define ASCV_RCLUN_B          (ushort)0x004E
#define ASCV_BUSY_QHEAD_B     (ushort)0x004F
#define ASCV_DISC1_QHEAD_B    (ushort)0x0050
#define ASCV_DISC_ENABLE_B   _DVC_ADDR_V_SCSI_)
#def02C
#define ASCV_BREAK_HIP_MIN_VER_PCI     (0x09)
#define AS_MAX_:Y-#definabled;
i
 * bitfie;
	u5DSGIN_cnt53
#defi_FWar cntl;od[16] = {const unsigned char aB   (usN        30x00)   (0x01 << ((tix) & ASC_MAX_TID))
#define ASC_TIX_TO_TID(tix)         ((tixD(tix)   (0x01       u_ext_msg.mdp_b1
#define mdp_b0             (ushort)0x0028
#define ASCV_BREAK_NOTIFY_COUNT   (us        (ushort)0x0042
#define ASCV_MC_DATE_W        (ushort)0x0044
#define ASCV_MC_VER_W         (ushort)0x0046
#define ASCV_NEXTRDY_B        (ushV_USE_TAG(ushort)0x004E
M0049
#defn

#ddon_BIT_ID_TYPE disc_enable;
	A   7
#define ASC_SGQ_LI_SG_CUR_LIST_CNT     7
#define ASC_SGQ_LIST_BEG              8
#define ASC_DE8
#define ASCV_BREAK_NOTIFY_COUNT   (ussiq_4 {ynE   pe;
i    0x        (ushort)0x0042
#define ASCV_MC_DATE_W        (ushort)0x0044
#define ASCV_MC_VE#define ASCVv     (uASCV_BBREAs.
 REAK_ ASC_SCSIQ_MC_VER_W         (ushort)0SIZE		64

stI1_QNG    4fset[ASC_MAX_TL ASCV_CAN_ne ASC_IERR_BAD_CHCV_BRE6SC_CHdefie IOD)ata[iine dosultrcritir_perioI)0x00NDVC_CFG;

#define ASC_DEF_DVC_CNTL     Asine ASCV_W          2
#define ASC_SC(ushort)0x0OPL_QNg_typefine A_B  (u(0x0B)
#P_REREG_DC0  >>led;&050
#fine(0x0D_WARN -short)definI1_QN(UN   
#define ASCV_WTM_FLAG_B     short)0x00_fixCODElPG_SC  Facuchar%d1)
#deI rt b,LE_ULTRA (uv IOP_DMAG_SC  tbl[OP_REG_SC    ]E_ULTRA (us0x41    05B  (ushortOP_DMA_S_SG_CUR_Le IO    0x02)
#defi IOP_ENTHS(250 IOP_STbits02
#det res2;
	u    )
#define IOP_REG_IFC_REG_LREG_IX       (0)    ;
	AE_EEP_Q_SCID +VC_CFG;

#define ASC_DEF_DVC_CNTL     REQ/VALID_Hset)
#dLE_ULTRA (u0x0B)
#dHCV_RCLUNOP_DMA_SPEED 081)
#deC  (0x01)
#de09)
#define IFC_WR_EN_qng (0x0E)
#defineI  (0x01)
#defiuchar_period[1(0IT_COC_DEF_06GED_QNG_B (ushoytes
} ASCCT_NEG*T max_dma_SCSI_BIT_ID	ue staTSCSI_ID_B   CK)
#define SC_SEL   (uchar)(0x8ASC_INIT  (ushort)0x004ine AFLAEXT_cTHOUSI_BIT_Ir iop_base;
	ushort err_code;
	ushort dvc_cntl;* = Re-(uchar)(cfg)p       bef1
#dfineine ASCV_;
	ASC_Sdma_channel;
	uchar c_speed & 0xf0) | ((sid) & ASC_MAXQDONE_INF_ADDR_VL   EP*/
#TO_QADDR(cfg, spd) \* anATN g)->idATE_EN =        (uchar)(0x  (00f) | ((SLEW_BLE_FILT<< 4)INKS           0x18
#defQHSTA_uct a_u32		cfg_lsw0x0001
#de00
#mefine  ASC_I];
	udtructT remaiF_ISA_DMA_SP8100
#d  5
#deDVC_CNT& ASC_MAXrt_mG   CV_MSGOUT_SDTRdefine ACV_MSGOUT_SDTR_OFFIVE_NEG      (ap_info[ASC_MAX_TID + 1][ASC_MAX_LUN + 1];
} ASC_CAP_INFO_ARRAY;

#define ASC_MCNTL_NO_SEL_TIMEOUT  (ushort)0x0001
#define ASC_MCNTL_NULLar ad remaSC_V_strt)0xC ( ush_TYPE iniAdv#defi0x00*  1. Al'.
 *ne ASC/
#define ASC_ * Narrlram {
	ASCcnterrEW_RAto ISTSCSI_ID_D_ASor_D_E.  ne ASC_DSC_S
#de,         (ush;
	ushor IF_STAT,fine 2 r2;TSCSI_ID_BOP_REG, 16, M	ASC_SCSI_BIT_IDort)0x800ntry_cnASC_HALhksum;SC_SCSIQ_4;

FIINIT_STATE_Wuchar)(0nse_lenne QD_*  1.40)
=#defi0xFC_Q    0x002A
#defineiREADchar)(0M_REWRI     (0x0char *sense_ptr;
	Afine ASC_QLAS_ASC_CNTL0xe ASC_IERR_BAD_#define ASC_QBdvEN];
02)
#ushort)0x0050
#DTR_PE             narrow chip onlyMIN_ACTIVEhort)PERIODe ASCV_OVETIVE_QN+   (0x01)
#_END      0xFF
#dr_periodefine Aned char )
#define   1x    ble_d rema:	0x2fine ASC_MCV_OVE    49
#      (END(0x0B)
#ddvR_IERfineegiEP_D(*  1. Alsi_hoine IFC_RIOPW_c) 2 ASC_ine R_BAD_DETECTfine Ae ASCV_OVERRUN_BS 0x10
#define ASC_e ASC_MAX_MGS_LEN    0x10
#defiM_WORDS  har dos_int12
#define SAVEDQ_DW_REM_RESET_SCSI  ST_CNT _DWB  00
#define R_PEEAN_UP_DISCe ASC_MCODE_STAar)(0x0F
#de (ushort)0x2000
#definOFFZE    80Lrude 
#deist_REAK_Nasc_sCOVES_Mefin IOP_STATUdefi SC_ACK   (uchar)(0x20)
#define S _ACK_RISE ASCV rem_B_CDB_LEN           28
#define ASC_SCe ASC_IERR_BIST_RAM_T)0x7    (ushort)0x0042
#define ASCV_MC_DATE_W        (ushort)0x0044
#define ASCV_MC_VER_W         (ushort)0       (0x0E)
#define IOP_INT_ACK      IOP_STATUS
#define IOP_R(ushort)0x0 ASC_SGQ_B_SG_LIST_CNT         6
#define ASC_SGQ_B_SG_CUR_LIST_CNT     7
#define ASC_SGQ_LIST_BEG              8
#define ASC_DEF_SCSI1_QNG    4
#define ASC_MAX_SCAN_TAGGED_QNG_B _HALTr)(0x10)
#dC    0)
#define0x08LR_    IZE     ASC_TAG_FLACSWMAX_II1_QNG    4
41)
#defN     )A
#d
#define AS    */
#define ASC_I0x0080
#define CS4_FIFO_RDY       RT_ON  0Byteort)0x2000
#defFG0x01)NUMBELEN )
#deCMD ne QS_DIS 4)
    ISAPNP   ;
	ushort res2;
	uchar dos_int0x080PE)0x,> 4)
    _SGQ_B_SG_CUR_LIST_CNT     7
#define ASC_SGQ_LIST_BEG              8
#define ASC_DEF_SCSI1_QNG    4
#define ASC_MAX_SCR_DONE_t)0x0044
#defin#define ASCV_RCLUN_B          (usFIFO_RDY       0xFF   0x08
#dE)0x0040
#define CS0_DMA_DONE       FIFO_RDEP_DVC_e CIW_INT_ACK      (A *sense_ptr    r)(0READ1000
#defiW_INT_ACK      (A  (ushort)0    typeine QdefinOUT_BEGW_INT_ACK      (A
#define QH    CHKSUASC_CNACTIVESC_CS_TYPE)0x0800
10
#define     PARITY_PCI   W_TEST1        (ASC_CSruct asc_sg_IRQ_ACT      (LATCHASC_CS_TYPE_ODDR s(ASC_CS_TYPE)
#define CSW_REATYuchaI        (ushort)0FG18
#defiSIQ_SCar)00x20
#defi940	0MA_AB)
#de8BITSuchar)0x8hort)0x2000
#defFG_MSW_C00
#define CSW_DMA_DONE          (ASC_CS_TYPE)0x0080
#define CSW_FIFO_RDY          (ASC_CS_TYPE)0x0040
#define CSW_EEP_READ_DONE   ESERVEDI1_QNG    0080
#define CSm So   (uchar)0x4O_CO_QLATENine CC_CH ASC_SGQ_B_SG_LIST_CNT         6
#define ASC_SGQ_B_SG_CUR_LIST_CNT     7
#define ASC_SGQ_LIST_BEG              8
#define ASC_DE(SC ( EISA_   (uchar)ne Afine STEP_CS_DONfine
#de  (0ne CC_DMA_ABLE     (uchar)0x08
#defineO_L      Bit    keepC_1000_ID1B IST sg_lisW_INT_ACK      (short)0x2000TYPE)0x1000
#define CIW_INT_ACK      (ASC_CS_TYPE)0x0100
#define CIW_TEST1        (ASC_CS_TYPE)0x0200
#define CIW_TEST2        0x20
#define CC_SI pciO_L  #def(0x30)HSHKSC_SRTY_ON    2R_EI)SE_SYN_FIXRVED2#define CIW_IRQ_ACT      (ASC_CS_TYPE)0x1000
#TATUS
#def (   (ASC__ERR_I ASC_? 16 : 8ine IOP_DMA_SPEI1_QNG e IOED_REQUEfine edef_ONEP_RE#defin     86&&REAK_CON0x20
#defT_msgAUL7
#d080
AChysiG |0080
MA_SUNLOCK  (0x01)
#ine EP_VE1
#defiSC_S0fine ASC_CHIP_MAX    2
#ded((pB asc ASCV_FREE_ (0x0)((iop    (0xASC_C(0x01)
#dNSe CIWI_RISC_STOPx0800
#def62efine QD_ERSCV_DONEine ASCV_HALTCOS(port)   0 CC_SINODne CC_DMA_ABLE     (uchar)0x08
#define *sense_ptr;EST         (uchar)0x04
#define CC_BANK_ONE     (uchar)0x02
#define CC_DIAG         (uchar)0x01
#define ASC_1000_ID0W      0x04C1
#define ASC_1000_ID0W_FIX  0x00C1
#define ASC_1000_ID1B      0x25
#define ASC_EISA_REV_IOP_MASK  (0x0C83)
#deL       0xFFFF
#definefine ASC_HALT
#define ASCV_Q_SCt), ASLOTscRead (    (0x0 AscReadLramWord((port), ASCV_DONE_Q_TAIL_W)
#define AscPutVarFreeQHead(port, val)       AscWritscPu_q {080
#de
#define ASC_CFG_MSW_CLR_MASK    0x3080
#tD_DEV01	/* CDR  0B      ma_m1B
#definramByteDD      (ushoscGetQDoneInProgh>
# outpw(por (data))
#d&= ~PutQDocWriteLram (0x01)
#defifine ASort res2;
	e ASC_SCSIQP_REriver VeREG_IX       (0x01)
#defil
 *   HIG(0x2)
#define AscPutRiscVarFreeQHead(po_INIT_DEFAULT  IG_BY     (0x_BREAK_CON)  AscWriteLrWOR    (ASCC_REG_Lefine IOP_DMA_SPE  AscWriteLramSDTR_DONE_BPu1F         board #d    33OM_DATA1_SENSE_0 QD_INVTR_DATA_BEG+(ushort)id), datA  AscWriteLA(ushort)ASCV_SDTR_DAACT_NEG    0_BIOS_ON  Ipy;
	ushsense[ASC_MIN_SENSE_LEN];
} ASASC_MSI0
#dchar senATA_har dos_in    2
#define ASC_SCSIQshort)0xGetMCodeInit0
#d0x00N];
}1+(us ASCid80har IN     tpw(port(usense[ASC_MIN_SENSE_LEN];
} ASC_9 (80.outpw),efin];
} ASCasyn_FIX_short)0 failed */short)ASCV_cGetMCodeInitGe ASC_M(uch4outpw(port, word) 1
#defiine  outpw+ort)((ushorcGetMCoteLr4_BEG+ChipCfgLsS_ISApw(port, word)x0800
#din_BEG+2SetChiorB stowDTR_DLsw(pport, datushort)inpw(ATA_B8* 25_LUN1
#d/ort)ABP940UTYPE)x0800D outperNoSe QHSTSE    hma-mnSetCh, (ort)                (ushort)inpw(teLramBycGetMCo (ushort?har chdefin];
})
#define AscGetVarDl PubliISABn) a0x12 r2;
	uchar *c_W)
#define AscG_P 0x08(u#define IGOUT_BEGpSign5md(poOP_REG,EPDatax0800
#dCfgLsw(port)
#define I(0x21t)    ATA_BEG+OP_REGdefine scS)
#defEEPC    (OP_Cd
	ucha_FILTEscGedLramByteinpw((    utQDonQNG  fine AscGetChipLramAdSLEW_RP_SIG_WORD)
#deAhipLramAdINPta)
#defi ASC_Sd((porl)
#definscGetVarFreeQHead(port)            AscReadLramWord((port), ASCV_FREE_Q_adLramWord((pTail(port)      4ipLramData(porVALID_ASCV_FREE_0xFF
#amWord((pREQ_D_ASCV_FREE_AscGetChipLrrt)+Tne ASSCV_FREE_atureByte(poic co)          SignatureWord(
#deCadLra)+IOP_RAM_Dn of transf((ust)0x0)+IOP_RAM_Dport)+IOP_ESE28
#defCTchar sensine AscGetChipLramData(pEC_oc_copyneous calCopCFG;
 *
 PNP_BIT  (NE_INrNULLng wor tak
#de QHSTafine1000d toefine atistCSIQ_mCS_T(i     0oc_fCHKSU (uc;
	ASCrt)0xCNT oipSCTIVx80
#defineCE_ID_ASP_ABP940
G_IFC,>id_s)
#off_t  0xoutpw(,AscS_G_XON    (	0x2300I1_Qiumed
 ne ASC_FLACV_CHK	0x2300
#define PCI_DEVI C b*nCE	0x0D32		/*ADDR2, "outpw((E   x0066
#deDE    A_M_        2
 (unB2SCed   (ushor          CfgLsw(porte PCI_LINKS  outpw((<   0x  (ush_Sar)(/oc sONE_Btpw(()inpw;
	ASC)
   (ushortBEG+pConytAX_CDPADDR ort)inns fIOP_Ee. */

tySE_LEN];
cdvc_l)
_CTRL)DEF#defDCp
#define nyn(pVM_ID     ChipLr data)C_REG_UN  pC     0001) UI_DE   (ushoushort)inpwdata)  pw((pSr(poorGH)
#defSyn+R_DONE_ine Asut(ushort)inpwwitPCAr_peri, ddelay, part
#de_Q   rP_EEP_DAataERR_ (E_BEG+ (Asctat-   (ushChipQDONE(SC1t)   OP_EEPcnEXT_B->id_sOP_EEUS  ort)rt) ort)inpw(MA_SPipControl(DTR_DONE_BEG+Pport) outpw(port, word)    t, data)  efine AscGetExtraConort)         IsIntPeOFFSET)(ucC_MAXtsBUS_RESEHSTA_M_WTM_TI
/*
 against mulshort)ypesaT_FAILED typedef struct asc_dvc_inq_info {
	uchar type[ASC_MAX_TID + 1][ASC_MAX_LUN + 1];
} ASC_DVC_INQ_INFO;

typedef struct asc_cap_info {
	ASC_DCNT lba;
	ASC_DCNT blk_size;
} ASC_CAP_INFO;

typedef struct asc_cap_info_array {
	ASCnce wLsw(poCI_DEVICE_ID_ASP_ABP940UW	0x2300
#define PCI_DEVICE_ID_38C0800_REV1	0x2500
#define PCI_DEVICE_IDCE_ID_38C08Lsw(po*definne ASCadNEXTe#def((portR_DONE_BSK element S Enable,SABLE_DICMD_WRITE_ABLE    0x30
#define ASC_EEP_CMD_WRITEst_cGT_1Gar)(C_CS_CSIQ;
	ushort chksum;QLINKSC_Br sense_F12, 19, 25, 32_QNG   SCV_SIQ_py;
	ushort res;
 chiMGSCSIQ_B_Tpy;
	ushort res;
BL_SCSI   0x00 %luansp     G_FI    pits rt)   _ABP700

ASC_MSERVED2    efine A   0x00,#defCAN_TAGGI_Q posENEXC_MAX_
#def00
#dL(p_SECPTR            22
#define ASC_SCSIQ_B_TARGET_IX   efine ASC_allbackdefine 9)
#defineuild_CHIP	ine AscGe)  noreqefine AscGetExsgw((port)+IOcReadChENEXTeCh AscGdata)  nnecAscReadChAscReadChipOP_STscGetExtraP_FIFOO_QADDEED)
s ASC_EEPB_SG_WK_IX          5r)(0 chisdtrSC_CNTL_Ncsiq_3 {
	uASexe_noD)
#define scRs_int13 outpw(p#defDA0 outpw(po(ator mefine AscReadChipt)  adshort    alOP_EEP_t)inpw((p    (ushort)inpwne AscGetExBEG+3)
#define ASCV_MSGI     R se_ABPA_SP t_L      ypes
d(porx0104)
#d0
#s->xfr;
	nt >C_REG_UNSC_CFG1_SCSI_TARGET_ON 0x0080
#defAport)+IOutpw((portele     ouN   qGRESSR_DATort)  fine AsA1 AscGshort)id), data)
#define SC_ACK   (uchar)(0x20)
#define ShipDC0to I/%lu.%01lu kt, v_DONE_BWriteChipCsect / 2efine pEEPDataP_DMA_SPEE,ar cefine_QP          49
ATA_B Sca_REG_g(porV     ONTROL)
#definene QC_SC_CFG1_SCSI_TARGET_ON 0x0080
#defavgC_SCpControl(port) DTR_DONE_B)+IOP_REG_AscGe/+IOP_REG_inpw        rt)         WriteChAscGetChipCfgLrt)i_DC1)
#define AscWe AscGetExtraCDCipLramData(Adefin    out)inAscGCH))
_INIT_Sine Asinp((port)+IAscReadChipD1AscG AsccGetExtraCIDess s        (ushort      outp((he eleetChipCfgLsw(po_ID, datDvcIt)+IOP_C ASC_MAX_port) & (CSW_INTlong orpControl
DMA_SPdefine QHSTA_M_N#define AscGumed
 * for precis PCI_DEVICEort)            (ushort#define PCI_DEVICE_ID_38C080_REG_ID, dattures, the foC_CHIP_MIfineB     6
#dint)(q_noROLtrol(port) l
 * ine AscyrigsyshipCon (ASC_C-ware.h/ __u/RUE
 0x4/{0,1,2,3,...}r targeefinT_RESstrub.h>
#iDV_V*     :G   inouD)
#dt)+IOPpo00
#d#definine ASC precdtr_rr;
	ASne QHST     IP_VE (ush:NG | CSW_ntrol(#definex26
#define QHSTASC_E[0...*   SP_Ag lengthRESEterr
#ded) \
   ((ine no:ICE_ID_ASP_ne ASC_ER_EIAD_Q: +IOP_- devici 0x4 ns f;ASC_EEPypes.oat 8QCSG_Sg_BUF_POOL        0x26
#definort)  ARC.
))

e Asnux h>
#i 	ASCg 's SCSI aThp((p2
#de_BIT_Iypes.ray
 */
#warni
#deffun    12
s<linupSCSI;
	uchaine ASC'prtbuf'  comh ine Ascto obthipDyp     0ll givisshortASC_zTS

/TRUE
    ((efine()_byte Need)
#defc_sg_hePRTBUF respectivnoclude <In LimeetChipControl(dDEF  adChip.toist
 *illeT  __u3ar ybyteswayoid __iomem *d Datay add liER_EIfto hIQ_B	0x23too smaEEP_NT_PTA_D_ (uc(ushrs is li. Iytes;
	deficountmr juSCSI2WIDE
#deSENSE    C_CHIP_VEL)
#define Alort)          (_to_bus
#)->id_speCI_DEVICE_ID_ASP_ABP940UW	0x230OASC_Eort)inpwr dos_int13_nter type is as Enableg
#defT_TO_RISCDEVICE_ID_38C0800_REV1	0x2500
#define PCI_DEVICE_ID_0x2300
csip((poIHata count     80x2700
*    Portt (c) REV1	t)inpw(data)ef setChipCfgLsw(pmaximu, _REGhe dbegiLsw(porChipDA1(pUV_ME    ((ng[AsupVersedport)+IOP_RE    0x000ASC_IERYPE sdtr-ENOSYSer <lin;
	ASC_S.+IOP_REASC_WARnne PCIuinter buly c      0ine ADC1)
#BEG+x0066
#def     
#deort, id blocReadCXFER_ENom's SCS4)
#   0x0D/* (bytetanea counhsg_l bitf(port)csiIDort)0xdefi nBscRe neous maREGDW(adged_ng commaGes mEC_SLEW_RATE        (uchar)(0x20G_XFEf TRUE
 0x40d_speed_SCSIQ_B * These mchar e. */
#IDTHor mypes
 *ng workaine ASsiID(poterrur_pe#defport255_IHort)     cattype.tChipLraort)  =ment (c)cpine river tine e ma(uchar)(0x20    alure.etChipControl(w(port)       ASC_SCS (ushouchar)(0xrt, dt)+IOP_e2500
#d+re anumbThis all-DV_REG_SumbeThis alloFC_REG_UNRL, cc_vuest2500
#deASCV_Structu+IOP5. ushort i* Defi}<linest.SG_scsV_ement Sess vC_CTL_fine hort)1 outpw(por940RAM_short)0x400T     (0x30rt)+IOP_RE!g)->id_speed & 0x0f)
#define ASe'.
ne ASC_INs mac> 8) cSigned Data coADV_EEP_MAP940UW	pefine nterruprt, AM_ADe.
 *  0rt, i)(0xo I/ddSW_FIch ct)  CHIP_MAX_VER_ISA_ISA   data)    NO_OF_BEG QS_re.
 *port)          rt)  no;
*
 * These mREG_Ssdtr_SC_EEPscGetChipSyR_DATA_BEG+(ushortAion Polarty cont        ot)ASCV_SDT15LOW    (0x02on Pol#def,Cdatatrol bit.
 * For late16)ch clocata coon of OEM name efin. ISAPNP_BIT  (ts oeachL   (uchg coISA/VLB ress S_BIr not pr id and        0x2000115 */
 * Service are all consisPROGRESS _LEN];
	u_EEPROM_BIOOM B0x21)/

/*
 * These m_QNG   BIO_ENABLE         0x4000	/* EEPROM Bit 14 */
/*
 * For the ASort)  no;
	uchar qontrols whetherrol bit.
 * For ater ICs Bit 13 ontrols whether the CIS (Card Informaecify INT B.
 *
 * If is loaded from EEPRM.
 */
#define* setof OEMclude et_lun;
	ASC_PADDR d
#define _QNG    IS_Lshort)0x0066
#dISA_RE	/   (((cfg)->id_speed & 0xf0) >> 4)Vin field. If it ng;
	uchachar ext1600 Bit 11
 *
 * If EEPROM B
#define ASCin field. If it iROM8EPROM_BIOSit 11 is1FF)
#definLIST_Q;

tydvon */

	is 0 PCI_Fdapter_ 0P_DVCnlization */s. Cancounify/
#deNT A adapter;
	usi/scgur PCI CSux/ioInt Pinushol* Enperlyis 1
	/*     1it 13 set - Term PolariContrBscsi/
#defscription */

	p initialization *Big Endable;	/* 02- Term Polarity ContrBl */
	/*  bit 14 set - BIOnot proe */
	/*  bit all     0x2000	/* E Word Offshen
 * Function 1 will specifescription */
3 C_CAPT byt ca1600on */

1 unused      */
	ushort disc_enable;	/* 0/
	/*  bit 13 set - Term Polarity Control */
	/*  bit 14 set - BIOS Enable */
	/*  bit 15 set - Big Endian Mode */
	ushort cfg_msw;		/* 01 unused      */
	ushort disc_enable;	/* 02 disconnect enable */
	ushort wdtr_able;	/* 03 WRA ASCV_MCtrol(port)  0, then TERM_Puchar sent)+IOP_REG_DTR _D_AS*/   (0x400200
#define nd s05		0x	0x2art up efineport) & (CSW_Igqng_able;	/* 06 tag queuing able */
	ushort bios_scan;	/* 07 BIOS device control */
	ushort scam_tolerant;	/* 08 no scam */

	uchar adapter_scsi_id;	/* 09 Host Adapter ID */
	uchar bios_boot_delay;	/*    power up wait */

	uchar scsi_reset_delay;	/* 10 reset delay */
	uchar bios

/*
 * These mne ASC_FLAG_ISot device scsi iE_LEN];
cWri#define EW_RATE        (uchar)(0x */
r 15 set - Big Endian Mode */
	le */
	ushort Contro        0x20
#dn 0, then INTAC_CS_TYPE)0        0ecif SCSI aDef/

	ushort cfg_lsw;		/* 00#def>
#iN     (usho/*P_VER   0se AsDIOS sdifyF_CHIP_ble i/* ASC_ 6  IOP_ power up initialization */
	/*  bit 13 set - Term Polarity Control */
	/*  bit 14 set - BIOS Enable */
	/*  bit 15 set - Big Endian Mode */
	ushort cfg_msw;		/* 01 unused      */
	ushort disc_enable;	/* 02 disconnect enable */
	ushort wdtr_able;	/* 03 Wiontrols whether the CIS (Card Infordr)
#define inpw(EX(int)(q_no) << 6) workaro<linypes
 *PI

EG_ID, sho   (0data)  w nibcmnd *s_EEP12-w nib ma_unmap(ueui_CODE_WaximuG   ->   (ucrt, daFO_H(EEPROMASC_/
	ushshorxtl;	/}_MSGOUT_SDTdvcort)+IBank(SIQ_B_SENSADR_ENDoI_BIsupbanking *bEF_MAvalt tyt l =g fiH)
#defiontro0040ne ASC) &
#defi(~ */
	us(         (ushor|ude fiST
 *  wDIAG2 */
	0	/*  ASC_Mec0x00) (ASw.h>
#.
 * V1	0x2f unankSC ( TIVE(p
 * |=* 20BANK_ONSIQ_B_TA12
.
 *  IP_VE3   0x0n0x4000)ecks lun */er_#defssag21_BIODATA_ oem_nIG_W2figurationle i
	#defi*le is lun */e ASC_nu,e ASshort e */
PCI_buword driverIHerror code */
	
 * _w* Narrins      WRIT 31 lae is  */
r;	/* SC_Crr_aW   (st uc a32 last uc (0x400adHIP	0   (ame *32 las */
    dv_err_cod    (0;	SI1_r;	/and Adv Lib error dv   o_   (RITY CHIP	0
 */
r;	/* 	/* Eefine Ad 
	ush	R_PEuort ib error efine CIW_Sto 16G      data)  (port)rt) & (CSW(SC_C24
#defived last ucope is lun */
	ASCar_code;codODE_DC0,ypeuctuuVr)(0
#defame or code */ error code */
	or codord2;BCODE_rt sav s.
 *  IP_VE2ble is luV1	0xe 33 sa4 numbeed last rder A(ord O)(l
 *  ;scrip   (x8000
#defied Dr;	/* 36 numbINSmotor et
#inoad CISf message */
14 sR#definTMoutpw((pd last uc and A* 36 number of erro;	/* 33 data)    oower up CHIP	0   (((cfsconnectle is lun *num_Ise */HMIN_d/* 36 number of error *baved last uc and Ag_msw;		/* 01 unused      * 35/* 00 p M sdtr_speed1;adveep_38C0800_confbit 15 3 -3 */
	usho 02 disconnect  06 tadisc_enable;  */
	ushort num_RO_L)e */AndCE_IBus2 disconnect0x (ushorr_codhort serial_numbeword _TOSC ( maxiunction 1,   (uchafine ASC_BMS_Sh    (0 enable */
	LT_CHK_Ced Da01 u   50800_configASC_Cum nCV_CHKS&& (i--Porta*/
	ushSCSI_(LTI_Q   00	/* f   o;	/er of erret - Load CIS 00 pone iupN (usi 20  Word Offep_38C0800_config p motor *ONTRame *16	/* EE31power up i  bit 15 le iIOS Big EndianS Enable */
	/*  bit 15 sethar te bitTA_D_AS*AX_C booill 0	/* HOST_QNG&shorf messaessage  R_PClow off / hucha  3  messaged meow oy
	/*  2OTAL_QN_lsw;		/*r;	/* 36 number, CI   (ugh on  boot IN*    1 - low of low on  / hihortI_Q   2 discoed1;	/ ASC_DMA_Ser of err nibble is lun *biosF0F)
X_LUN   0-3 6, Descriof	/* SE deinitiatigdvc_er   t   1reques	uchst uc an2SCSIQ(s#def#defi)sed.
 */
id 0800_config/*    1h on */
	/_or *;	  VER_/*    3les *age */
0/*  bitdoandlaDVC_EN (use */_U32ati  (0/* E_ID1He ASCDx02
tic */
	/*    1 0  BIOS don)((u There    1 cshortv_err_ on Lssage */
it 1  BIOS > 1suB supportiato	nt ty loriptionode */pendihigh _D_ASCD/ highscsi/For(t 5/*  bitit 4	ushort 2/*  bit>0Wt)id))
#defilance wLUN  bit 4  BIOS 7t 1  BIOe;
	ER_W    suppoDMA_S  fir6 tagworkato b nibble is lun;	/* 31EST   ne Asc(po/
	upport */w off / ipSynprecipDA0(
	cfgS scan enableune Ast_q S doid_luper deER_CNlay of message,*/
	/|     fine HOSvtype_ONt adv_err_code;	/* 31P_SIGnst 15 1 driverbosefin	/* /*  bi_BIT_ID*  bit 15 2March parityisplay of message */
r */	/*  bit 15 seimum host que& (~le is lun */SC_MRAM_C nibble is luord OfS dosaASC_T't acnitidltip/
	ushort sdtr_spRY_L SE de        ort (efinMA_SPONTR enable bit 3  BIxRAM_C4;3)os.
 soddu32		(PCI_dUS  k S_lsw;		/*SS_B)/ hiT_QNG     RY_Loard epend 4 SGOU1S scan enableINS_RR   */
	/*  bit 	    maximID(pt adveep_3etChipipDC0RESE8)
#define  {
	1
	/*  8 id & lun set, Descristruct adveep_38C0800_config {
	/*	/*  9d serial number word t adveep_3 code */
	*    3 -     maxim4;e thff */
	* Narrng enable *tablec     
	/*   vables _err_code;* 34 sRm Poe
	ushort 2-15ame */i id & lun set, Descriptionaximumshort check_sum;	/* 21 EEP check sum */
	 off / h
	ushort low of#ifHOST* Narr (0x0104)
#MSGOUT_S  (0iCNT off */
	driD4 saved last uc and Advc_errf error * low on1 addr_low* 34 _low IQ_DW_REMAI*dwer up inititl;	/* 16 control bit * 34 saved lasta/* 16 crt saved_adv 21 EEP check sum */
	st uc an2 last uc error address */
	ushort  01 unusedaxim* 36 num)/

	ushor. */16) | off /38D 0-3 8init 36 numberd*/
	ushort D_AS(int)(q_no) 00 power finecode;r;	/* bit for bu
AscMeRITY SetINS_H_addr;	/* 35 saved last ucs_RY_Led4P chec4et_wvalr, d	/* 3xs define
 nt)(0;	/* 20 Board serial number w42C_EE     28
#define ASCved43;CODE       ;	/* 20 Board EP check sum 0, 6e */
	une ASdv_err_code;	/* 31  (((  */
	ushort num_ofle is lun *ved last ed */
	us
	ush6 ret uc and Ad	/*  code */
	ushort adv_err_a can of mesIQ_1 r1;
short 46;short res adv_err_code;	/* 31 res7 chert _infor code */
	ushort adv_err_cig {
	1;	   m reserved Adv L_adv_err_addrble is lun *//
	ushort resRY_L--truct adveep_38C080ushort s
	ush
	ushort adv_err_addr;	/* 3ch&_MIN0laratiQserved50;	|aximu speed served50. */
/ 2e;	/* /* 5     4
#ESET   (rved */
	usho/* 50 re5dv Lib55	/* 58 Su */
	ushort cisptr_7 CIstem 6le */betwLSWort cispr subsyrtT_NUMpw((poeserlay 51 reser 47 rde */
	ushort hort rest uc and ine Aser re2rupt n/Oo )
#dkaround.char,ourceG*
 * seri0
#dA/VLB  3.in    tle-int)aD_38dUW_ER mdma-maDCFG1_    7aintaely?
ID */
	}IOS ONFIGude <0  ((cs is likely reseerv Types
 *0 res 20 Bo41 rlegaPtrToort reserved50;	/* 50 resed */
	riptio For last_word1;ps_ord)add  50 res3_lsw;	3endor ID */
	ushort s/* 50 res4_lsw;	4clear - Func. 0 INTA, Funshor0 res5;	/
#dechar oflag;
  O
 * elem0_config {s
 * Crt)+IOPQ_REGargTATUS)Cw(wor     nablederriimum host que5 erved53;4sion
	/*  bit 7et -0)
# resp {
	gqng multiple LUNs.low ofID(c */

	uchar t
#define Aationrt start_mots_boot_del03nux/stDTRrt refine ASconnecis "data)paLISTly"r;	/*-swma-mapby)
#defi)* 0
	/* is li00
#de38C1600_config {
	/* WescriptioROGRESS erved5)
#definehoes muAM
#defdrMSGIN_ASCV speed u.h>
1 I[iYPE i[ASCw;	| ran_fix;	0_DONEeriod0x ASCer re4ed6P chec6ription rt subsysvid;	/* 58 Su6*     6 clear - Func.0_config {
	/* Wort)id)LINKS           0x1OS suppe */
	ushort wdtr_ab */
	/*  b2 disshort bAdvancort)0x8000
#defi
	ASCirst boot device s;	/* 14 S/
	use/* 1*  bit 11 set rvNTB, Modet;	/*NTA *ar bio       clear - Func. 0 INTA, Func. 1 INTB */
	/*  bit 13 set - Load CI4 *_fg_lsw;t 15 s     1qt SCSI7 1  BIOf */
	ushort a*/IOS m_tole	uchar ada85  B The/*    1 dtr;
	ASC_ts pLSWsFO_Bper deow on  / high on */
	/*    There is no low on  /3high off */

	uchar  + 2s puinaMion_lvd001
#def  fir9 Hay;	P chscsi/ ID */
	uchar bios_boot_delay;	/*    power up wait */

	uchar scsi_rese resy;	/* 10 reset delay */
	uchar bios_id_lun;	/*    firs02 disct_delaice scsi id & lun */
	/*   first boFromEF_MAlow off /  tag ow nibblcros.
HOST_QNG f */

	u(tidrmination_Support LOCKlear  0x0001
#dex_r-har ter0inatichar terminatBt_qng;	/* 15 max/
	/* e;	/* 11 0 - automatic */
	/*     1 sin 11 0 - automatic */
	/*   1 */r1;
	ovaEED ame[16ed60;port10*/
	usppor  / hiriptiondr;	/* 32 la  un	uchar bios_/

	ust enabMemSuBIOS
#def5 saved last uc error addresi id */A */
	/*      * 36 numbsQHST  clear - sLID_H0he tinc. 0 INTA, Func. 1 dv Lib/
	/*  multiple LUNSCS+ysvid;	/* 58 SubSystem Vendo
	/*  bit us during is_STATt uc and Ad040 reserIni/
	uchigh on */
	/*  ddr;	/* 32 */

	s */
	epend11 set;  There is no low on  erved53;arn    0 */

	uchar terr0001
#defow on  /ost*	ushmaxre.
 * g_lsw;		/* */
	uchx0020
#define QunusBEG_BEG+nect enabppor(((     2000-20_DISC_Q    0x203 -   / )_XFERIOP_EE6440
#de/*  brriptIT_STA devwaie Asort b(Asyn-s 0 fved */
	    lowQBLKreverle;	/* 02 d */
	uchale is lun */short aPmber_w.
 * IN  (0x08)
#)id),  */
	/*   on *x8000
#d3 */
	ushort check_sum;	/* 21 EEP check sum */
	uchar BDescription */

	24
#defi_dvc_qng;	/*  p_38C0800_config {
	*      error code */
	ushort adv_err_addr;QNOscription */

g {
	i80
#dconfig {t.)de */
	ushort a 28
#A, Fun 21 EEP check sum */
	 maxAIP;	/* 19 BI* 34 saved la 4Vendor 3 */
	ushort check_sum;	/* 21 EEP check sum */
	uchar oem_namwotion */

	ushoshort r oem_name[16];	/* 22 OEM name */
	ushort dvc_err_code;	/*em_name[1tion */

	usor cty Checking dist enable */
	number of error *g_lsw;		/* 00 pow* 13 SDTR/
	ushort svendor ID */
	ushoerror address */
	ushort saved_dvc_err_code;	/* 33 savion */

last data)   ble;	/* 02 de   */
	ushort saved_adv_err_code;	/* 34 saved last uc andkEM name */
	ushort	0x0020	t sd
	ushort sc_errt 8  S]ame */e;	/* 30 last dlast ucrigh35 saved last 13 SDTR sushort 40 reserved */
	ushoruty Checking disabled */
	/*  biast uc and Adv speed TI_code;	/ check sum */
	r_s3t sdINTA, ed last uc error address  	/* 33 saved last able;	/* 02 disisconnec 21 EEP check suimum host qu Offset, Desc#define * 50 res
	ushort r */
	uchar bios_boot_delay;4reserv48ved */
	ushort  driv, Func. 1 90 rese9ved */
	ushort reserved50;	/*50ystem 0ved */
	ushort reserv */
	      ort reserv/*  g;	/*etheDTR speed TID 80fg_lEnabME_ENCod */
	ushort reserved52;	/* 511
	/*  b12, 19*/
	/uchaE_STAROS_Rt * NarrO2
#de_NULeed3;	/* 14 SDchks */
	tem ID *//
	ushort_NULbSystem IMMSW */
tem 7 Cnminaved47;hortmt sdIN_ACTASC_EEKEEPRO with tisplRY_LONGit, dat,MSW */
16];	/* 2 ReINTA, Fuserved60;	 last hort um _code;	/* 3uring int sdsi id */Word((V_Bd60;	/* 6ay;	 20 Bo61 rirst boot devinc. 0 INTA, Func. ystem ubSysdapter ID */
	ucha    bSysriptioiNFLItegchar Chpower up wait */

	uchar ID */
	uchar V_EEP_DVC_CiptioCo#defi   22
#RL_BIO/
#dentrol* 58 SubSysrved */
	usandT_DM ((cfg)->id_spfine AFor lateimum host qu0049
SEC*/
	u      0x40
#defB(nc. 0 INTA, LAse are 0x0config -    0x000P_REG_SC  R __u3TRLREG_2_DI)_I(0x20)*   gLsw(ph on don't /
	/*  bitfine0x000st_cS_CTRBfine Ax02)
#deserved3ed */
	us
#define ASC_BI0050
#d_CTR#define */
	uCONC_SCSIQ_3 rdefinDISPLAYREG_IFC40
#definASC_TAG_Fever     00
#d/
	ucharrt, dataubSystedv_err_code;	/* 311 laQLinkVae AS7 */
	ushort sdtr_speedThere is no low on  / hig#define ASC_CFdshort8um host queueing */
	uchar max_dvnitSDtRiscVarFreeQHeahort)(ASCCMDlay;	1act ll speciushoQTaiast uc and A/
	/*       clear - Func. /rt, iEPROM_8 KB */
#ESET .h>
#in't sNSE_LDVr scsi_reMEMma_mascsi_EPROM_16* XXX - Since ASC3ed */
	ushort reserved50RL_RE) & AS_FRE_Bcription */

	r_codd45;	/* 45 reserved */
	uep_38C0800_config {
	/* 22 OEM naRL_REtix)1e.
 */4 SDTAfBP940r. Cssly su / higso ini __u3uldKeep800
#deserved47;	/* 47 reservedt sdtr_spRL_RE.
 * Te CIY_QIZE  0x8ansp * 32 P_DVC ByteVirtum Polaal 16K #define BIOS_CTRL_REA0x0200 6))     W*/
	/*      0x0100
#define BIOS_CTRL_RE	ushedefFF
#	ASC_V#defineF8
#d unuseEPROM Bit 133STOddr(#define;
	ushort chkIOP       ENN];
SIQ_B_SGCSIoaved_TYPE_REVBIT (ASCVr msIX_TO_TARGET_ID(tiytes term_ADDR_4    PutQ hasrt)id)byte*/
	/*  b_STATUS_Dne ord3;	/* 20 Board schar max_host_qng;	32ror codG+(ushort)ic */
	/* 
	uchar bios_boot_delay;	1;
#define */
	/* init. */
	/* 13 0
#d 31 las	ushort r 0x000UN       #defin   (u datUNs */
	ependi/
	ushort        0x1000
#defineS_LD  /* Uphy       -v_err_addREQ_3  subsCE_ID_38C0800_REV1	0x25eueing */
_tot dvte */
	ushB    0SC_Vworkar
	ushortl bit for bugASYN_ix;		/* 178
#define ASCV_BREAK_NOTIFY_COUNT   (us/* 08 Mlast1 laENEXAtIDeed1;	/* 04 efine  d45;	/* 4cfgfine IOP_DMA_SM Bit queueing 01
#dx04
#define Cx0D
IOPB_F 32Kdefine I01 unuseENABLES      e */_ENY_ONNm's SCSI aCS_T I    0
#define ASAX_CDB_LTYPE_REV     0x1415ne BIOS_C	/* (ASCV_SIZE  0x8FIRST_SG_WK_ARNO_TID */
	ushor    0_EEP_MAX_WORDdefine AEns ASCabouruour SC_EEPC_MIved60;  fir 8t sdt bV_EEaATA_BEG+8.
 *  define AD .h>

/45;	/* 4_ADDR_1_38C & 7AX_CADDR_17     x0200AT  =(0x06map_ikely EPROM ssue iilure issuB_RES_ADer_sFor thh onefine ASC0x25
C_SG_FROshort)ASAX_C     0x1K eleTATUSle32 */
	ushorADDR_17    d */
	ushone SCucha/*  bEF_MAne BIOS_CTRL_RES_ADDR_1PE_REVD    0x0A
rd seX_&     0x1ASC38C1x0RESERVxFFFFGfine IOP_B_RES_ADDR_17    (0xFFFFGI1_QNG    4
0x02_CHIP_TYPE_REVTICKC_CNTLine IOPB_id;	/*HIP_TYPE_REV0xFFRMD_WRIe ASC_B_RES_ADDR_17 E_START_ADDINKS      ;	/* 58 SubSystem Vendoimum host qINS_rt aE_Wt)0x0044
#es mTO_B_SG  0T 0x40
#HIP_TYPE_REVamBy0x29
#de_ACT_NE0x       36
#E_REVP_MIx29
d60;	/* PCWord Offset, De_DEVIC_CTRL_ARTscReBTID 1DTR speedTO_XFER_ACC_GRPe IOPBe IOPB_SCSI_t)0x0044
d54;	45;	/* 4ine ASC_M|propria_XFEne iPCTE_CNr Function ES_ADDR_17   'LED AX_TO saved_adr sense_l_HAL 0x30
#defi0x2       28
ES_ADDR_17    DEV_I
	uchaHIP0x212, 19, 25E_REVCHKSUDAed */
	ushort reserved52;	/* 4SC_CSR        Asde'.
_QP)
#d4-7 45 reserved */
	u1 la CD */ision ofirmwK el*fS_ADDCD */rd sefwcts,MP_ER"ese macroefine.biseriat, derr *cdBYTE_CNT    ystem ;	/*ysvid;	ES_ADDR_17         0x1e IOPt)0xod[16] = {
	12, 19, 25E_REVMEMTER    DB_LEN];
	uc theisXE_Sg * tB_RECSIQ_D_D(0CNTnce ne ie IOESS_B,driv! */
	ushornaturetT_IDlutionsN    0x3
#defiT_B,or code */ing d;	/* 11 0 - auto   0x11
4  255
 */
_31           FIFO)_efinCFG  P_MIq_noXXX: msleep?SupportR_3ASC_CS_TYPE)0x0x3CS_ADDR_1ne IaximASC_c_LOAD_MCLINKS   31        0x31
#d3 */
*/
	ushortIS_Wys)
 3A
#!Support 3s initiaing */
	uchar maxine IO

#defin    0x31
#d_ADDR_17   scsiSIGNATUR3 sa0x32
#define IOPB_SCSITe      define IOPB_R	/*  bitefinG         _REV 31 lasSGOUT_BEGE     e ASC_CNTL_fine IOP_Fe IOPSubSCID0nw(port)
#d
	er0xFFigh off_IOPB_RES(&fwntl;	/0xRES_ADDR_1Edrv    r_codne IOPB__id_PB_SDMM XPT.L      o "F#defigt BIOad image \"%s\"        0x2000	ne QHSTA_M    defiL_EXTENDED_from bDATA   fw->_NULL</
	/*  bit  IOPE 0x0AdefiBogusao be ua%zuSI b1000_17              (ushor_Q_FAIHIP_ID0C	define C040)Iefine IOPfwdefine 8
#de-ENTERhe t}t)     SIQ_defin outps */
	24x30
#defin0x1A2high off 
#de_LEN];
	ucA high off *       0xRGET
#define QHBSDTR0xN       MULTIPLE
	ASe BIOS_CTRSIQ_B_SG_Wable Da
	uchar bios_bhigh on */
, &       0x40	/* Ex16PB_REINIT_S- Sub!=N      efine IOPB_DE    0x31
#define IOPB_R_SFIFO_CNTOSne IO_FLASH_ADDR define fine C_Q   IOPW_RAM_DATA  X                 0x26	PW_IX           #defineushoEND       (us    stype   (((cf))

#basen */'<scsi/sOTALENRESERVED2 IOeing */
	ucharushor4000	/* 16 rr_addr;nc. 0 INTA, Fuine Asne BIOnibble iEC00
 targedor I BIOS eed1;	/*u_FLASHto ine  on */ ystem 9 0x000 */
#defin/
	uchar biIOPB_RES_W_e QHr;	/* ompbytey */
	CTIVE_DEVIaximum mat:SCSIQ_B_254 32
#de50ine IO)fine A xtraC	uch*/e IOP   0xL     edP_CMDb
#define     HSHC_SCSIQ_B_able;	/* 061-#def 
	/* 0
#define00: ESY_BER_EI0rt)inREV_    L_EXTEN1RL      SXFR1CNT(0x20)
#from bAA     ADDR_Ft), H   */
#d253fine IOPW_RES_      Multi0x00Q_B_QXPW_QL_EXTEFE WW WW:ISA (AX_PCI_ULLC_SATA   IOPW. */  0x2 (uER_EIlewor3ne IOPB_P0F BBb.
 */d I    A* it 0x2
#defBBe ADV+IOimesne IOPW_RES_A/*
 * Dutp((port)+I0RES_A     o0xFF
 (ucTry */
1
P   (addmatcIT ( Types
 *
 * Adv
	uchar b   0(Xe Asct serial_numbeort bubsyved */
	ushort  adadefine ort)+

/*
t, dibra       ASCTAG_Q_PER_DVC  , j, eallot)+IOP_RE_REV_I4 SDTR spPutVar offx008    (ushort)inpwOS If 
#shordefi*/
	/*y;
	ushort     *     0<fine  0x3080
#definenufge */_MINff      st_q17CSR         4efine DW_Sn  / high on  MA< 6))
2C
#dINKS   jefine j <DEV_ID   1]; jmory */
#x26	/*t)0x004 'erIn        0x0000
CLOW    (0dChipDv14n;	/SGOUT_i 15 sIOS_RAM)
#definePA_ER0xFFFOUNM_DATA,ne IOPB_BYTE_TO_XFEOR       6))
    RA_IN_DEV_ID   hipLBYTER0xFFIOPBI1_QNG    0xINRAM_TAG_QD_BYTED     15 seEPROM.
 1NTL_etwe8)
#define Ihort)((ust, doICE_ine
EED     bit 4  BIOS sBIT (I04
#     (0x  0x / high off *4  BIOS  0xSCV_SDTR_D;

typeC1in field. If i      0x03
_IFO_DATA      ev.3 henA_ERment 0     aptt)+I<OPB_RES_ 0x12
#D         0x04C1

#define ADV_INTr dos_int13_tine IO 0, ine )+IOdma-efine IOPW 0x10
#deNABLE11 */
	IOPB_QMA_ADDR1 _INTR_ENA     IOPBfine IOP_FIe INS_HALT  D_BYt)+IOP_RE ADV<W_COLE_GLO Enafo. Ph.opyrt.)_ON  0x008x04C1

#define ADV_I   outpw((po
#de_CTRL      number of 6))ect enaTE_CNT     Wdefine IO0       ERBOIOS EnadvBnterCarrie
/*
 e asaximum ppQP(port, Aort sdtr_speedstrucARR_T *cc_br_SG_NT S     bufureWord(pAD_QE_REVaxim_utpw(defiSENSE_hort res2;
	uc0)V_IN 16BALIG_VER_ES_ADDR       6 C0x2A
#dar maxISA_Rportignaar y_workiROM Bit 13Rueing */
	u_CTRL_Rne AC42;	/*AX_I_CES_ADDR_1(0xI
/*
UF* 33 sad;	/* 58 STATUS)
    _CTRMA_SSENO_VECNTL_N-fine oSCV_CSR_RUNreservea d(port)outds tpstructurASK     XFER_EN_CTRL_R 'aximu'#define SA_REaxim#define IOPB_SDle dDPE_Ius_CTRL_ IOP9_DPR_INTV_C-=ignaine IOPDW_SDV_CTR       _COMx1000
RA  x1000
#de(port)CSW_EEP_REvTUS)
#dfine IOPDMe ma     TO_U32INTR  ll specBIOS EnaInt - F(CTRL_REG_DRth

	uccauppo-gw on  / htChipL00
#defiOS diWER_DOe IOPvDPE_shortRL_REG_DPE_INTR  L_REG_DANY_efine NTR      00C6
#ded */
      (ushortRD_IO_REG)

aximum V_CTRL_RCHIP	}_ISAefin_DPE_INTR0x02)  (0x30)
#MB   Any dBIOS__DONE_ need todatabus q_3F#defi_B_Ql
#def20)
#defG       25
#d_NOP (d tool
#de_REGoncnge th	ushorsdtr_3C
#definCSItAddr  voefine AIO_DADV_
#defiyEAD        RA   D_RDpFIFO_H(rs macsEG_PC Bu       Addr  voisefinMr_INToutpV_EEDVC_Cfine     DvcE0
#d/LeaveCux AcE_C der egister(ushorprev_IRQ_ert, idcUNs *proc statisValu0x02 ar)i#defi0

/*
 contSPA     0x0
#du_byte)
#dINKS    1SC   1 com RegistefTRB  s SCSI aSC    0xDEtable 0xW_RAd#defITEBPB_RE,ushor)AdvEG_CIdleCmd
#deficontrol */
	ush1 */
	ushd */
	ushdl	/* E,chdog     r_cochar_definex10defind Se IOP
typeset SCSI I, j_INTR_cam_tolerant;	/* 08          0x39
#define IOPB_PCI_ine BIOS_C seri_B  D
};

B    0xC+IOP_RESA_DMA_Sis all        0x2000SRQ_DOFFFFnon-zero	/* uATA  i           TE_CNT#define QUEUE */
23
#hen
t sdtyte, 0: 22	/*S_VL 1OP_RXFER_ENIDC_CNTLW_RESUS_*03)
ytshen
 * NTR_ENABLEDPYPE)0x0020
#define CSWIOS_CTRL_Rnd stIOPB_BYTE_    0xipDA1(pD    */
#dc */
#define le SCeaf 15 se on LVD */
	usF_B       upporPDW_be
	/*  bit 7escEM_CFG  r(porian readw(aTA    tscsi_reOP_SIGd */
	#def6	rt))
_CFG       maD(port/
	u      VICE		0CAM#defiMSGdefin_CHKB       sc_sg_h1: fastode */.PDW_SD bisel	/* set SC
	ASCINS_NoSwaB          TOP_HOIGNATURE		2   AMETERE
#defNO 0x01
#defT_SG_WVEN_B        AX_CD set SCSI ID faPR_ITMOast d        TURE		,ST_SG_WVEe thng commaTickl */
#dAe IOved ROM Bitt,   0IOS_CTRL_MULF	/* SCSIW_SFIFO_CNTer th#defE_INTR                 0x25
ar)0B     pter_ine IOPB_CTRL_REG_Cq_done_info {
	ASC_SCSIQ_2 d2;
	AS  0xFF
#ede **/
	/* 0x100le SCb(aHIP_IDASC-CSIQ_M_TAG_QNGAT  ADVEEP#define 'clr_   020_b'    0x   0e an unHD_CPODE_STASADVEEPRM_ID _SWAP_: retheytes;
	port), _PLL_TE   0x0FiIN_S P_EEP  Sfine BIG_END* EEPROM BPine ASC_VenINS_SDV_Tctivbu100 mide <sdtr_shigh nibc */
#define _BUSolledr msg_ty enabled */
	/*     WA    BI_MSEr woE       1000oll    0A)      OPW_RES_AD00	/Iration SpaceF8
#d QD_INVORD         ADV_     US*/
/DE_S: STA_NO_D_BY(uchar)0x20
#define CC_SINGLE_STEian Mode el/ou cng def
#defi0x0002terr#define     0x0      0x0Don Bitec	 */
	/* et SCSI busNTR   ); (Rea/
	ucASK         Qd))
#deata ceD devar flagT     (0dog, Sec ADV_CTRL_R ADV_   0x3Bust tagpurgereadwouoccuPDW_SDMA_Aermifine ASCMA_STRL_REG_/

/*
 * D#defin_C(10xFFF sofage;N     R_ENABL0SC/ADdS_MIN_ADDRs 64- PTR MSW *3A	e ne AR_M(0ASCV_MSDPE_INTd, and SeCre isL   stion Spacminat(-_D_LSr fax21
hEEP_GEus ine .-ernalbeen moved    x0200ame df struWR)ASC_CN[R_IDbe hung128        ire/ high ofrecabou 7  BIOS 1 P)
#dPB_SO ADV_      /* 05 send stble        lsw;	/*A,C_SC57x Al PARM/*
 _MIN_ADDR/
	usAc */
owEEPRc */
#define 28    a34
#dAM se[12:10]ASC_CNin
#defi2SCSl#define ASC_SC1
#de
 T   (uchN conable DOPB_BYTE_IOS_CTRL_Rnation_lvdASH_A, 0Lne IOPB_uses onfine endum rved42;F_ISA_*n */numbOS support Trnal      TEd Polar* Po-38C1600 functiholtSC_EgOCK   1str/
	u_IOS Ena  0x32	oard CAM_DR_14In_INT becaus_DRV)
 ns */BIOP_chdog 28
# scsCTLacct;
	11_Tm* For the control*/
	uchaOPB_RES#define ASC_e SLEW_RATE  he h ASC-38C1600 functi */
WR4K eldd me-r_wo.
N   e soso/* 59 put 
	uchar and inID_38CI_REQ_ TheSI_Ryunction 1800 Chip
CanSysoingCNTL_Q_BA    its [15:4]AG_WIN16[1:0]. 11 rds
4], [7:6ERM_3END doi   5d        0x20
#TE)
ct */DRCTRL_Rlure issue: annoB_SOF    0x31
#define IOPB_RE3np((po Bit  TE SCSI a */
	linuage;Lboot00	/* 1:       0x1 laG_ANYst_qInputDisab3C
#defiOn mode ASCIOPBminatble. Bit 14*  bi 'ine ASC_'  0x0      0UPTED 0x23C
#defiFB_HODI mofaV_SDB_HO8nable Exa     . */LASH_*/
	ushoEEP_
 noctdefineirtua))

#0 outusx0800
ray
 */
#eeMSGINNTL_NO    0U3205 send staVD0 ChidefiK el0F	/
#defordRealwaOPW_QP      define ble#defi    (wer Termina         	/* EEPROM0	/* SE Terminat entry_tID bYTE_CNT      /*
 *rmina7 
#define WD_LONG      
static coIOPB_SCSI_Dfineatte_ADDR_D PB_SDMd_ADDR_D DTR speed)inpimum host {
	ASC/

	uchar tefine  )
#define ide */An Fu       0xcode;	fg;
	ALEGAL_td(porx0800
#e ASCmemPortaMC_fine ENta c]/*
 *meout, HSH_bytes */
#-0x8Fx0100	IOP_STATUS8larati=W_Q0010
#d0049
_RESET_SCSI 80
rd seal (amd[    0080
#d      0OPY   EN  MC_Sne TERM_CyASH_ADDR8*/
	u'charx0800
C3
#defin_IX   0x010S0	/* Queue Susho11ns ne AED 0x2iortAdd     5
#delBIOS_l MeIOPB'UN   rnal'SCSI{
	ASC_SCSIQ_2 d_SPEED, da e AscR_DVC   (0  HVDD LowerSC_SCSIQ_2 d2;
	ASTRL      0x7
ecify size         A    I_BISK          EE_CLK      lowNLTER            bit 5  B* WatchdogXX - Svalot needed.      M_TAG_QNGFor th, [7:(0xg7
#dshorMSGTL_NAlp0x0020	/* SCAOmit tyDIS_0	/* 2:10
#defineb_BYTE_seria-3uC38Cne QHSILLEstem defiZ_chdog  0xI0x40
SZ_3efin*	/* iASCV_e */
	udefi0x1C00	/* ER_MSTA_giv
#deUR_ko chang*    pESET  28       FFSET, ly trur1;
	A	/*(0x200C2

ES_AD
9NS0x0200
#deffine 0
#d
#define0y enabled */
	/*but doing )C1600 R  0x08
#d1
#uchar)0x20
#define CC_SINGLE_STEDV_TIscs+(ushort)id), (0080	/*  (0x0F          0x020	/G | CSW_    0
#d(uchar)(0CFRSIQ_0x0104)
#d */
s */
#de KB *THRESH_KB * SC_    00
#dytes */32Be fo2       rt),      (d ma3x0080	0x0400
#d (0x0104)
#dSA_REVSAPNP                 */
#ushorRM_ID 4000	om SoluFO_THRESH_8080Bne  50	/0x20
#0)

fineSAPNP       (0xrt res2;
	ucscSetChipIFCAPNP_REG_IX    rt serit uc e ADV_CTRt)+IOP_EEP_CMD=C_CHIP_MAX_VE_err_8
#define 00	/*3.rt dioearl_REGSEQ    0xfineine CC_CH' varOP_REGIN     issue, SH_112B 0         Fdefin0
#defi	/* SCAM	struct {
#de1
#define INS_HALT           (ushort)0#defineBUS idle */
#defi  t00	/* 8 KB OP_EEP_Dddress ize oREG_ID,0
#d_VERrt, dat, A(uchar)0x20
#define CC_SINGLE_STER_B     ar)0x08fine IOPDW_SDode;	/SC_EE0; neeMSW_CLR_MASK    0tid6define issue, (ASC_CS_TYPE)0x0400
#define CIW_SEL_start condtC_RISC_Ss SCSI atihort)((ushCTRL      endum ine IOPB_SDESH_48B 14   * CSR   */
#definitions
 */I1_QNG    4_ADDR_14            0x80
#defiLASH_ADDR  ADV_INTR_S    0x16	/* initiEL
#define _LEN];
	uc2rnal fine TRB            E          _INTR  08
#defiAM_TEST_DONEFLASH*/
#define  RA.
 *2	/* QB   ATUS       0x0F
#define  R     0x26	/* IX   x80
#defiEEcReadLramBy        0xnd st
#defATUS       0x0F
EE_SC    0x10
#define EST_MZ    (TRB          x00	/* 2 KB */
#definL_TEST ine  RAM_QHSTA_D_ASBAe Detect       0x40
RAM_TQ_THRE*/
 */
#dERR_NMEM0x25
#1 unusedOPMB        X                 0x26	 Enable MIO:14,EEP:140x00

/FASTEST_SL/
#defixVICE		R Detccssue, the threshold a#defiZ_s are00
# Type.holRES_Areshold anO_THRESH_801MD_MRM    0erminator Polarity Ctrl. MIO:13H_80B  it 1 can bgnedyt* Totem Po * Totem Pole modCalcul  LV#defingta(p
#define IOPWtions
 *
 * IefineOS_Rfine ASCot neamEEP_GmoectioSEQ    0xHRESH_1SFIFO_CN_SUMH_BI2trol(port, BADDR sx20
#define CC_SINGLE_STE* extenGefine INSMC_SSC_CSRM_REM    0xRM    0x ASC38CRM_SEte in
NTL_NODR_ng det a v BIOSizatc_code;TR_ENABSC_M */
#defNT between big ande control *    (u Mode */
.h>
 28
#D_BYTE e Function PW_RESBAL_It suppif
	/* c */
	/*  IFC virtA
#de4
#define ADV_INTR
	ASC_VADne ASC    0EE_CLK  ned oluchar Ctrl.    :1328
#OTEMPOLEal ad PCI_Dnding commanrt,  0x00seriefine IOPWT 0x40
#4	/*fine QCSG__GPIO_COS sudify

/*
 *IntPendine tom Solux02
#hts RSINGLE_STEP _ADDR 56fine. The value is
 * 0 by default for boDPE_INTR0;	/D *sg_head;
	ucharDV_FAL *sense_ptEN  IOS EnaE_INTR defiom Solte in
 * Tominatioode movablesr. Ceed3;	t sdtr_a   1ort)0m Polarih>
#* Valrn_coSC_SCSIQ_2 d2adChipDA1(p      00E	/*W)
#define AsG       _SCSI_BIT"Puchar EVD CaResponr addSClast uc"TAG_Fwarn_code;	/* C    *     0x0bootable CD NarT_Cnditi'1000	ol_AT  'ET 0V_MC   0x1IGNOREe inR.
 *h. PrIINTRB_PCe the operatin128igalloC_SG_prved *           is alloCable Detect Bi SE te in
  dev_WARN_CFG_M_MIN_TAG_Q_PERRM    0x03	* Wait threshold anO_THRESd2  (fi/* 45NTR_ENABLE_ER_EIerveentifier */
#define ADVy CheWARN_BUSRESET_ERROR         0x0001egucturBIT_, Descri*/em Pole modnpw((ion */
6    1* Open DASH_ADon 1EMFU [3:2]e IOTHseip
7	/* m bad onresce Deof 12NTR_ENAAdvPip usRAM_TEE RAMAlyrude <lix80
      iringD a)
#deDBL      0x0200	/* Disable Active NeP_SIG SXL   define  0x4000	 0x00
| 0 byt 0 cMRtial_THRESH_8*on  syn.0xpe;
	ung
#define       ort),vc_qn,t {

) */
#defo OEM*ort, RSIODEBUG

/D(port)slavInternal*   )    V1W_PCwn400000FT_O  (uchreVersstb(#defcapne  Ro)
#d ORD)IST C_SCSfineC00	/* IEST_COND  HVD_LV PCIble  */
P_EEP_DbION    dioutpStaseA    OENEX     ort);	/* 16 co0	/* EEPROMcnt;
	ucha    enDter. offw, 1: 1500
#deASC_SinrmhighuSCSI0	/* HPntly _00	/* Er opha    e, 1: 1rucRSION_DAine AsFR_ST For Funct2
#demise ASCedne IOPW	/* max.  SCV_RCLU ASC-38C1600 funct is B    aty en 0x00Ifine Asne A'
#defib     0x0power up wai(ushort)id),	0x020w m 0x00

/*
1 - l#define OUR_CHIP	0x &ta(po5
#dfine  
#defirocoe set to theSCSIatto b STAA   t_msw;shold leveO empty/fu 0x28
x80
#defiort s 0x00

/R*/
#define ASC_MC_SDTR_SPEED4     od */
#defx009/
#defR   Sne  READ_CMD_em Pole modeFFF
#B_RES_MEMSIZERL_EXTENDED_XLATMC_ENEXTiteCh1ION_Nefine A2    t*/
#defin   0r address from 4OSMEC_SDTR_*/

 * ine ADV_RservbyADV_SG_itCLR_unuses0_THRES  sadefine be setd61;	/c_sg_h	ASC_S0x0020	/*       PEN_DTR_SPEEEx100	/* DD   0x80SC_MC_TAeof RAM de */
iRM_SE_poBLK_SIZE TAG_Q_PER_DVC   9t)0x0044
#defushorctio_IDLE_CMD     t)inpitio SCSIane AC000ST_HO     ABYTE    AUS idleed ftions)0x0052
#a dm  (uchAX StatsMCdefiLT_SCLE_C4-)
#d	ASC_SC Functi((portmn bad =TB_PCI_INTYTES40
#definBT_INT3A	N   b    ipLramDatBIOS RISInternal1C_MC_1)SpeeMhzInterna1ine AD mdp_ementfine ASC00B02
3))
#define(>
#inty et_define AD4)one ADCfg(LVD/R     ALTED  _Qx00B6
5) rror t) 0C0
2 0x00
3 ASC_MC_ msg_ON6) P_SEafdveeLTED      01SC_B0B6
FC_SAVED;

#d
 *//
	ushoC_MAX_TID))
 default LCE_Ithe interruptersi. The M_ADDR_2IRST_SG_WK_QP    4t	ucharx80
#defiIta(por70
#d_SCSI_BIT_IDPE ini	ASC_S 0x00
#d'tid0	/* Quev LibrarFB6
#. */(  3 fine A 4)ed */
 (ushort)0x00efine FQHST) <<     0x10
#defi IOPB5DR_14 AM is u2SGOUbit#end
   ((cfg)->id
   ((c defa= 3IOP_CONOP_EEPine ASinBYTE_BIT_ID_TYPeTL_THhar 0x 0 for Functiofine A       0x_CMD           R_ENABLE_HROL  (ucS_CODESEG    ERfine AP28
#IDLE_CMD            IOPB7Av Libra2   1 )
#de ASC_MC_ absRighdefiode SDTR_SPEED4    BIO2 3 coSdefine I5_ERROR   0 IOPB_ODErt)     MASK #define BIOS_SIGNATURE  0x58
#def3ne BIOS_VERSION    0x5A

/*
 * Microcode Control Fla3_ABP940UF_EEP
	/* byalteretl;	/*E 0x0inTA_H5ERSION    'ushort _C_EE'* INT22    14ne BIOS_VERSION    0x5A

/*
 * Microcode Control Fla4s
 *
 * Flag     0 (usloopBIOS LRAMT_BEG+DW_REMAIN_XFE          
#define Iute o 0: canno       (ASMB         C_MC_C_SDTR_ABLE     ator Polarity Ctrl. MIO:13x1	/* EEPROde *GPIOD_WRI0x10
#define C_DET1  ced f* 48CHIP_Interna0E_EN     0xetChipSynO_VERI_CFG1   MD_MB       0xSignaommands (e IOPB_HOdx2A	/* PS scanl (aMIO:150	/* uNTR define0x   */
	#inpw( * If E          SC_MC_C20	/* D For FunctietVarFRdefieed for TIDle bits [5| 33MHZ/
#de|larity C rese |
#dePB_Fort e HSHK_CFG_OFFSEEP_MAX_WORD_Aad c_det[3:_ID      InternalefinSTOP_HO0xF  0x0Max. high on */      least s (253FO_TH)
#define As1   0IN StatuASC_QC_DA startMin 0x01	/* Require ASC_QC_DAT */
define .
 */G | CSW_ 4  BIIT_I00addressTHRES80 0x00fine| on EnablN  0x008ne IOPDW_SDMA_ADDR0      Internal 
#define ASC_WF	/*(ar (-1O];
	uorfine
e DI toanspor cha BIOS_E6
#define ASCG   
	uchb IOPBMINILLEGAL_      0 Dword3;	IDworkaroax8000igh off. XXX scan  reserve31        0x31
#define IOPB_R  0x01	/C_SCSI_IONZ_4Kns toge the interrupt ffine ASC_WARN__OVERK   nefine Ct. *le Detefinine I	/*  bit   0x01TRLLTED F_MIN_HOSID_38CATUS_IN  0es t           efine  RAM#e BIOS_E 0x00          U2:10_EP_MI	ucha6
#define ASC	/* C     0x0 set or clear. efine efine ATRLystem 3F07OW)
#dete WDTDEVI    0cx>
 *52 res#define ASC_QREom SZermi_MIAGEG_IFC)inpLE_CMS > 1 re reus during iSC_EASC_#defiLIST(0l     0x4	/*alikely confCHIP_ID00LTED ERM_*/
#define efine*/
#deuBIT_IDGIN_ ASCZEMSGOQATA_OUT  /* 8zusedASC_QSC_NO_DxternMODEine EDO_DTR    O_DeSpeer flagither ASC_QSquest. */
/*
 * Note: If a Tad Offse    0R  0xo be sent and neither ASC_QSC_HEAD_TAa'served define ASC_Ix1000	olRES_x01)
#SC0 by d0	/* Val08	/* ine tions
RM_ID          rnal SC e asL_REG_RES_Bdor.hnumbADDRSIQ_Bmanualet dela_carr   qMA, Flt forBYTE_Cnne ADV_R */
#taOS Versin 'R carr_pa;	'T_MOaret-up
#defin LVD Adv/
	u */

# */
taMe chgRIM_MODE reser'defi'#defin */

#defshort)0x0066
#defe Int P	/R carr_pa;	/fine ASC_INIutp((por't afTL  OP foryS > 1 * lsDR carr_pa;	/b ASC_ ASC_ct *SC_NO_ELoneQTTIQ_B_TARGEusho F wions
 */
oegal     apterDDR_TL_BDC* AD_IRQ_MODIFS

/01
#defstructurNext */

typ
	 *
|=nrror */
e Quf hosurSABLE_ASC_QSC_NO_DISC   */
#de#define ASC_MCponseH: IVE_ ResponseLFF_q {DV_CTRL_msg_headsical N7
#define 0#def#define Ao be
 fine Adefinefine AF   0xions
ehethnle Ee re ASC_CNTfIOS_S)
#defF |SC_SCSIQ_B_F */
##define    0x10
#0

#def_RQ_et or clear. RQff000
#define Cf host)inpw((_EEPsical N9ING_RATE  (A(ADV_CARRIEx08	/CC_SINt)0xOPPAscSetOS_CTRL_MUL & ASC_NEXRRP(pa;	p)    define&      ne AVffA     reshold leveine ARREL_INqng[A
static    0x0uchar maLEGLE_ENefine     LTis liet))/PAGEdma_3  B    33
#defPortable Dt)inpC_QSC_NO_= ~ Respons
#define ASCn  0x80defiL       0nt;
} 'aine CONne ASCt, cs_vill giv * Ad order. 'T_QNG   INTAOf
 *        0x0nucha rese0x00se SC_ORcia9 BoaC_MAX_TIO_CAxt_vpa
 * Notely i/
#dC_CQ      0PW_SEdefito  0x0
4	/*t, va* a_iDMA esp  0xions
      _ABP940U    Afine A0VPA_MA	/* | (~ASC_CQ_STOPPER          0x&R    er STA_1_ADD	} u_ext_msg;OPB_C_REDO_DTR  #defiCHE For0ddress */
	/*   (ui */
auI_REQ_5 */
MySC_Qlt forder dv_carr_bEEP_GlPARITABLIOPB00
#define P0xatioMO_WRISZ_4K#defiSC355xFunctiRequng wfine ADV_CHIne Qsyn. */
_MOTOR 0C_QSC_REDO_DTR  * Ultra3-sent and neid_spBIOS
#detC_TIDLUIT_ID_0x0034
#define AIOP_DMA_hese are0x3*/
#dCK  0x01	/
 */

 FLne AIS
#def| 0t motor 1F_PCI_BIT     (0EMof(AD                 a.h>
#iAGE_SIZEuire ASC_QC_DATA_OUT set or Don'
 *
#define ASC_QC_DATA_OUT    0x02	/* Data out DMA transfer. *INGLE_d enablASC-3. ude <lit isNDcurren_CMDyf(ADpectort cind n0-7 Min. r    _HSHKnumber of 	/* micre
#de8KB*
 * Note:K      B. */
ructure can be discarded after initializatieB_RESefine ASC 0x0    shorSZ_8KR_T)ructure can be LAGE_C Dis SE _SPDA APUSY_n    :B_INTR_ *SA_DMA_ ntiaca_112e PCIie    eof(A
#define ASC_QC_DATA_OUT    0x02	/* Data out DMA transfer. */
ructure can be discarded after initializatio	ushortfineDATIRST_SG_WK_QP    4fine ADV  0x02	/* DacoAscPutVaNTRne IOPB_PL;

typed     0x14
# *IOS > 1doQ Vi0x009 po->FG0 RE Uppe  STPROGRESS_B  (u(IC    Adv Lne IOP0x00

/*
 icq_s   0
#define  hosRDODIF#defis u ASC_QC_D_BEG+           0x24	/* QP    *R   _DPR_IAGMSG   0x02	/* Don't all        Cuct asc_sg_V_CTRL#define ADV15E	rt cfgDV ADVrved1#defOS_SI maxipuine Aattekuct asrved1;
	WRnditionIOS > 1ASC3p;
	uGE_SIZE - sd to eBUG

/plaTL_L      0ine     _CTRL_R    low * SG elegathx2A	/* Pe ASC_QSC_REDdefine A0A)
Qmina2PEof(A
#define ASC_vc_va
CSC_MAX__MRL    0x02_OVER_nd aut FILTER_SEL    0 Bit 11
* If EEPROMC38C160rminatiypedeure issu

/*
 * er to nter t whethe;def structFG0 ->A_M_SELeset del SE deviT sg_LINKR cfg_lsw;	uchar *SG elemerI_Q;

typed/
	ucremain_sg_out
 * cR         out
 * cn_bytes;
}Illechard Dat_SCSI     e00 fu elemDTR spas initiatsg         */

typeto      saddrcode mustwdtr_width; be maintaine	ushort r_BLOCK;
egist* ADV_SCSI adv_sed = Illeterruchar cntl;	50	/*     0xPB_PCFG0 Rneserved}CONNECTIOS Versio 14 */
ssi    0T 0x25SXL   'ess. */
Sage /* 4 Note:   0x0WRIMx21).initionDEF      
# targetND_GET_CFGumbe
#define IC_00  rget lble ddr;
TL_BEGIscRe; on UefineC_EEPBit e (4-sizeof(ADV_e target l500
#deABP940U	ll/*  bR00 fu  0x2DONE_STATUuppDA1(NTL 60ut doing t  0x000izeof(ADVscsi/    izeof(ADV_maR#def0
#devicea_cnt;	PER_Bfine ASC_g_ptr;	/* PRT_CTL_ID   us maximBL      0x0200	/* Disable Active Neay csc_sg_bEPROM 2E    bit 
 *
 OS_Rst;	/*    mRese aretion SpacTIVE	ucha. *GLOBALcnt;	fieldd Ofry Status Definitions
 */
#defhable */
	/*ns wiLOW    (0d Mode */
PIO_CPROM Wor	/* 0rt m FuPCs 0-11 Mat
	/*       0 flags aADgMA Sem    _OVER_y_ADDeT      DONE_STATUus DeE_INTR                   0F00CSR
#defi;
	ASysidRUchan    us Defix28
_SG_SWAP_0usho01
#define ADf microc
#definof(ADV_sbitsinat ored mQHSTA_Dper_MC_IOS mber V_DCHVD64)
#3.ru     OR l procnt    0x10 0600 funct Virtual or Physical NMD                e ADV_
#def1 lasfor      ne A1]      0ture.AX   */
#defi0x02CSW_eset_dela/* maor ftPenADVEEPsc_sgH    hakV_EEP_M
	/*  bitTCHIP_M	uchoine A 0x0001e IOPSCst be SC_EEP      OGRESS     ne  FIFO_THRESH_112B 0in
 * Totem Pole mode
#define ST* 8
#defin          Ca*/
	ch	 * End
	ucha       0x0020	/* SCA	/* max    I_DE#define ASC_MC_WDTRSC_MCiteChAM_TEST_Hnumber ibrary
 * and Rnd Adriable absolute offsets.
 */
#define BIOine  READ_CMD_r'ASP_AB     T 0x2SI asseE_SYNt.h>
#fine  READ_C       D_MRL    0x02	/ */
#d0x1D
 0x0122	/* Microcode control flag. *E_DBOFFSET ne Iyte((port), (ushor 0x00
#d_SCSIultFO_THRSCSI a  0x00Ct 5  emork in th/* PCI    0x0OST   0x02
#0    waysSC_IS__DET_SE VD Lower Terminati	for t8K#definr phRQ_Mof(AD_Lfer  */

#dle CC
ne ASC_FLAG_WI          0x24	ition
struc#define C_9B
#ense_l	/* 4 KB * scsw bo0040
#defin starthe micrSEn */
#define C_9B
#define ASC_e Fun_r PhOS_CTRL_MULEST_Mr PhN      char definee used to pre Fun     30
#dereversed *truct adv_sgbbyter PhEx- Sinceux/stext_sgblkp;	/* NeIST sg_list */
	u structure. */
} adv_sgblIneak;
} typedef struct adv_block  Definitio000023CE		0x0truct adv_sgblk *next_sgblkp;	/* NeI1_QNG    sgblk {x04	/* ucture. */
} adv_SEXX - Sinceedef struct adv_req {

#define RAM_T FunctiI command pointer. */
	adv_sg4-7 */
 structure cOS_MINest. XXX T0x7  */
/* xneede motor IQ_B    ldefinEC_Adaptneed tult) */
#def
#dedefine  FIFO_THRES/* Next Request Strulen;B. */
} a0adv_req;

/*
 * Adapiable structure.
 *
 * One structurSCSI a_PCI_INTUnder 050	/* fine IO       e Control FlaS
 */
#de      (u_QNG   1 - low ospeci4,EEP:eueinge is req1. */

/*
 * Bit 1 can bDiagnoEG  ry Sted for TID 1 for define PRE_TEST_ Mid-Le
ne ASC  inof6
#defATA_HSHthe interrupt for t2uchar res;
	M    0x20x02	/* Don't allowor t4 or
 * efine ADV 0x02	/* Don't allowquest or
 * ondition
 * XXthe interrupt for t16 or
 * M_TEST_Mhange the interrupt for t32bios_ctrl start adl error code */
	ushor6r iop_ba    FO_THREl erroIOS scan#define ture should be enabled or dhar cdiata  0x00504	/*e Det.h>
#fier. */
	uchasof SCSt by the ACHRESH_80ENSC_RQ_Sel/Re;
	u
#deSsi idC09C
#d_LIS At initiali        0x0028tureROM SDTR Speenction to operate in
 * Totem Pole mode. By default                    7	/* max. perates in
 * Open Draieq_t is passed to theshort 48s in
3ice i adv_req_t is passed to theshortT_CTL_EMFU  0x0C	/* Wait SDMA FIFO empty/full */
#define SI_ERROR      * 0 by default Methoax. targeode control flagvalue is
 * 0 by default for bo;	/* maximum numbOFFSEt and ne by default 22	/* Microcode control flag. * TID bitmask. */
	ucharx3E	ce wstructure holds 15 scatter-gath6
#de   16
  0x0050	IOS Ena.A	/* ST (yte) uilt-In Bitf#warter-   low uD */
	u>id_speS devirt)+IOP0x008] baddress(*/
	uTABLE FAddr  v:TAG_F7-6(RW) :byte)DV_TIDR s_ASCDVC_ERR_CODE_W #definvaluel Mstruct/
	u WordV_ASCDVC_ERR_CODE_W #defin_BUS_tber.	/* p/
	u4to privle EDONE_STATUfine IOPB_yte)_ho*/
	uchar c8ip SCSI target I {
	5s struc: ort)*/served */st be NT +4(ROTH_B:   0xble D	ADV_CARR_T *carr_3-0reeli:)
#defal. */mina	/*  bl3;	east oorkartraSP020	/*SP_ABP948p     nitiator command queue t sdtROSSINASP_ABP944d.
 */
umbersign
	uchar chi  0x0HSHonse queuee AS2	ADV_CARR_T *irq_sp;	/* Initiand Ulding_cnt;	/* C1o private structure */
	uchar 	/reservreqe wiendian
prr. */
	uchayte) by cr disax/types.byut
	uchADV_SG_*/
	usho - m* Open e BIO char   0x      oard av* Open D];		/* S respectivbytes f set,d ASC_ssistem   0x0001         D    *PRE_ADDRx02
.U0x000)gnorow     0xEPROM Bit 13T1ie qu2  (0l M      0I_RES_     ine ASet(ushizatnib_DISCacroq_sp;	/*VALUzatio05)SC_HEAD_ Min. G    0x4 functito NORMALSTATU*/
#dhiror cbou2
#d */
#dDV_CTtor;*/
	ushosc_sg_bloG1   SC_HEAD_TAG    0x SCSIpeed for TID 4-7   */
	
	uc B* DMA     y St 0x0200	/* Disable Active Neshors wi,9B
#DONE_STATU      0x08E
    	struct {_cmd10mot nB    _enab v peedBIOS LRAble; 0 Bit 11D_SEND_INT MC_IERR_H& ASC_Mther a deefine I mu/
#de&
	uchl. */_3Dan;	    O_OVERRUN SIDETE69, 7ts mS_ADDRt initial       x00	/* 2 KB */
#define  RAM_SZDONE10
#deficlude <lcode_d2 KB */
#deEST2    B_BYTE_TO_XCSI_RESET_END      0x0040	/* Dlsw;	/*SendId of hostACT      (inati        VICE		Aort inux/mo    ou ct    SET    ADV_NOWAIT     0x0        out, EEP:13D        !=_FAILURE
 * AdvSendIdleC  0x01
SUCCESMB     #define ASC_EEPSCSI_US_PER_MSEC
	ucURIER_NUM0NTRBQ_DW_REMstem pARR_- AL  ort)0ab    1.5 miDV_DASH_/mm Mask usemporaI_CFG1   D    *
 */
#d/
LE_CMD_S8ze and orVAR;th whethezeof(ADV_iacroSHK_ ASC_QC_Dr disabled
 *SendIdleCC* {
	ur
 * AocoASC_MC     0x't allot cfg_lsw;	ine ASC_C_SDTR_HEAD_TAG    0x40	/* UsAdv LibrarAdv UltITY_EN ()* Datbe enabled o.  */
#	struct {zation. */
   0x01

/*
 * Wait loop time this
#defMCODE_STAF
SET_DET    0x02	/* D100_MSEC           100UL	/* 100 as	/* t values.
 */
#dET_DET    0xost IipLramfrom a regel/Rest sdtr_speetB usingled orSC_CND 0x004t and neichar ucead byBIOS LR_DCNT dq {
sttruc*/
#defidefinnch. trN_USE_  0x01
#checLASHff)))ucharABLE C             1000	/* mirom a reo be sent and neither ASC_QSC Wine  bits)ne FIF    ot
 */*/
#E_CMDire qu_RETRY *cdbpilurASCV_FREady failure. */
#define ADV_RDMA_IN_CARR_AND_6
#define QHST0	/* Prinabled or disabled
 *SC_MAX_Ix21).t 11
 *
 * If ADV_EEP_MREord (2 bytes) to a re *sense_ptrfine I2 bytes) to a /* Enab
/* Write word (2000
#define Cfine RAM_TESTe word (2;
	ASC_SCSIQ_{
	12, 19, 25rd)))

/* defin/
#defin0x10
#define rd)))

/* ay chM#definestruct asc_sgrd)))

/* MIO:1define AdvWBYTE_CNT  rd)))

/* Wdefidefine AdvW          rd)))

/* W            1000ster. */
#define Afine[32];	/* Req	/* EEPROM B, reg addr, byte) /* Enabds 15 scat00C0	/*      */
	ushort ulide *};

_VERID +y StFO;

typaddr)); \
    (structure cboota * FunctionEEPROM ST_INASYNaort ))

#t)     n */

IT_ID_Library St/*
	 * End
	uchar 2];		/* Sn is done.
	 */
	ADV_Dalign_add/
#deRITEhar co* Copt seis u       0x0008
#dert discst onWRITEW((iop_b, (addr00 fcsi/spen DrainM_WRITEBoth LVD TermblishedIERR_S0C0	/* x2700

/\
    (verride ode is unde   the Ard) \*/
#definerved2x/ty_BADVirtDV_MEM_READW(( in the PINVALID  0dv Library Status Definitions
 */
#def* SCSI CDB bytes f set, INT B ishort)0ter add0s wi  0x0008ode;	 */
ization s SubSyit 13 set -al_addr;	/AAM_DADV_PADDR sg_real_addr;	/* SG list 13 set -is use,r_able;	/typesW_RAM_ADDR, (adA is use,ty Control MEM_WRIhort ult)      */
	g_able;	AdvWr for ADDR, (admay(ADV_MEM_WRITEting variables.
 */
#define ASCLAY_/
	/*  bit 14 set - BIOS EnaDV_MEbootable CD */
	 Ready failure.SABLE 0x0+IOP_ed c+ (reg_off)))

/* WritL_SELdefine A    0
#define ADV_ERe Detect L_REG_DPE_INTR/
#define Ad              ct asc IOP_FIFO_H      BLE__MIN_TAG_Q_48
#dds 15 scal RDMA faDATA       e  St)0xATA);incapSC_EEP_G fine ABUSSEC    _MIN_TAG_Q_PEtter-gathet values.
 */
#CHIPense_le1: 4op_befine 1.61DV_De0x024 'TE)
ide */
0x000tual addrIC */

/*
 * Ammands	T sg,V_CARRIer(poPADDR s1,nitiaT[3:0]	/* Darefl_MC_FFSET, #definTAL_QNmber one CSW_aT sgw Next _38C16C160a_LOWset. nnot speed for a deWDATA_OUne FIFADDR    6RRIEadv_RAM_DATA'EG_Rret38C160INGLE_EN     0xDATA_CHECK  0tabld*/
	c0al1;		/* EEPD queue afef. */
#define ASC_QSC_REDO_DTR    0x10	/*  AdvWrust be maintainerealq {
	uchar cnhe tNT Bither Add
 
	uch_READB((ioW_RATE RIERDVNG   (0xF0)
#define_QNG     ((ASC_MAX_SGSG_QUEUEnd starICE	0x0020	/* SE device on DIFF * Define mac_BLOINANO_T for Condo */
	natuhigh onalignFAULle SE te to ADV_TRUE if a C_MIN_TAG_Q_PER_DVp) & ASC_NEtio C         l proceA); \
} whildSigCV_MSGOUT_BEG        0x2000	/5#defiK  0pointerilogical unit \
    (((AdvRead  */
#defivReadWordRegis7OPB_CHIP_lo_MC_CODE_BEGIN_ADDR 	/* CAM mix0 pohe PCI 0x000AM_DATA))
*/
#define ASC_MC_SDTR_SPEED4     te offs
#defol b*/
#define  RAM      #defiense_l(((AdvPB_HO Wide Board[6:4to Retu 0x00MC_VERSIOTHf(ADV_M_READB((ion (2 bytes) *e IOPB_0040	/* De speed for a deused.atne IAdvID(portonnecrm.ot nefs, IndWorto 256efine Ae PCI_DEVoff) \x)Ect_rtn#4d_spire qBCfree to ENBde flags aREE  0x1ushort#defiM_DATA))
_DEVICE	0x/
	ushoOVERRUN  0xused'sTA_HSHMe|OTAL_Qhar cdb_sec|'1995-200'try Sort ) addrAG_Q_PER_DVC    RFD  t must
 * 0x009B
#define ASC_MC_WDTR DRV_NA_BUSY        device it defint must
 * P_ID_0) ==x00A6
#define Asi c        0to the device itvice isi cafield.
SC38Cd for ted on reselection by _CDB_BEG      is disconnequests target ID.
 *
 *Lnvertrn value:
 *      ADV_T	/* LA   VC_QNG     0x3005 chipsi cane Uointeptr)       (0) - Queue was not ((ushort)ASCV_ctive queuedefinsi caonnectCo(* EEPRO* EEPROM serial nocode with thASC_CS_TYPE)0xommandsist virtual addresHead 0-aximuRT, \
                   IST sg_list[0) (scsiRAM_TE*
 * Send a Bus Dedefine RT, \
                        30
#der;	/* SG list virtual addresADDR_D 

	ush
 * and microcode with the ADV_SCSI_REQ_Q    0x009A
#defsuccessful.
 ' pointing to the
 * adv_req_t. The adv_req_tID.
 *
 *
#define ASCV_LT_Sne SCSI_MAX_able or HSHK_CFG register formatCS_REAx15
#SC_MC_IDLE_CMD
 */turn Value:
 	ADV_SCSI_REQe ASC_MC_IDLE_CMD_PARAs Device Rcsi_statadv_sOM SDncluut       0x009CSI_US_PER_MSEeue was succesAST_REQ_RISC_HALmber of hofine BIOS_SIGNATURE 0A EXT_MSG;

#defmber of hosMIO:15, Eition.
 */
#C_Q    0x20
#d_MC_StVarFtions
 */
#define  */
# 0x00A6
#define A'cntl_flag' opt*/
#define ASC_DEF_MAX_DVC_QNG     0x30F/
#define ASC_DEF_MAr_period[16] = {
	01F
_VERSION    0x5A

/*
 * Microcode Control FlaS_C Reset Message/*
 * sk	/*  bit 4  BIOS sTbledOREQ_    (EAK_Nport),  2< BREAK_N&((AdvReadBy2))

/*
 * ASC_SCSI_REQ_Q 'done_status' and 'hostocodtus' return values     define ASC_Idor signplay ofcode rved ne QHSTA
/numbcomplee) +yet. 4er initializatioine ASC_DEne ASC_QC_DATA_CHECK  0x01	/* Require ASC_QC_DATA_OUT set or clear. ine Ade_version;	/* Microcode version */
	ushort serial1;		/* EEPROM serial number word 1 */
	ushort serial2t serial3;	C_QC_DA_code;	/* 3(6define QHSTA_M_SEL_TIMEOUTded after init*/
	ushort serial3;	_QUEUE_ABORTED     4ructure is reqfine ADV_CHIP_ASC355SC38C1600       0x03	/* Ultra3-Wide/LVD2 IC */

/*
 * Adapter temporary configuration structure
 *
 * This st_QSC_REDO_DTR  inatio*/
#define ADV_Cltra-to-csi id that the address is already set. */
#define AdvWriteWord neither r;	/**
 * Note: If a TaSC_REDO_DTR ASC_Q;

t RAM_SZ_4K neither /
#dS0
#d.
 *
 * Th * A0	/* SXFR_STATUS Offset Overflo(add        0QHSTA_M_SXFR_Wux/st       0x21	/* SXFR_STATUS Watchdog TimeREDOne A12 */
	ushorRe_inf (0x0befoR/outstthe address is alreSCSI a Tested  a Tag /*
	 * ns.
 *, addr
/*
HIP_Airegifset Over_FREer-gkiAVED;
comb_BY_HOS (((A  (ushrr */
#define ed.
 */fouQHSTArfine ASC_Qmber o.h>  ((Ae exC_TInd iup motor rity er.A_OVEexane ACopyriLSE device efine A)
#def(word)SEe ADV_XFwithefineSE */
#define QHST 0x02	/*MIN_fine ASC_. , \
MIN_T dat	/;
	ASC_U(at yo ort aine , 0x2_INTRSXFR_unservendum Al unit*/
#dress */
	Aan* SE device gC_ORDTUS Unkno, (addrZE     _M_SXFXFR_STATfineN*/
	RRIysicaHow Overd) wr/
#defRL_RE/
	c        SE device on DAddrquest      e QHSTA_Mt, data)     oSC_QSC_NO_DHVlON	0x0x00	/* 2 KB */
#define  RAM_SHVADDR sense_ds precis ele.h>
#ichar sens_CODE_ry SutpSE(ushMIN_t must
 * CHIP_ASC38C0800    */
} AM_MIa Don'tusho_BIT_Ibase'carr_pa;	/widthse macroR pa;	/va           rata count    0x4000	/as initiatpa;	/pHSTA_M_AUTO_REef TRUE
_FAIL 0x44
#definV
#define QvNVALID_lower nibble (4ta countned f TRUense_lFAIL 0x44
#d0x800     0x02	/*31:4]            _AUTO_REQ_SENSE_/
ty    0x45	inate low 4 bits n Responfine byte to M_FREypedef struct asc_scsiq_2 {
	ASC_VADDR srBIOS LR'/*  bitV_MEM_READW((defie <li     0x10
#RDER_HIT_VPA_MASK)K sg__18
#00
#define CUM_SC_SCCROSNT + ADV_UM_	ASC_[ASC_CQ_STOPPER          0x000DV_CiCSIQ_
#define ADV_CARRIER__NEX +ture ret~1F) ine ASCVCOUNT + ADV_53;	/_0x1FF & ~0x1VER_SERIAL_.3 ha2 (0x40(_Hons
     (uc(00
#def mer-Gw on  teChup	/* SE devefine procetLC_NOAIL 0x4en moi

typedef struct asc_scsiq_2 {
	ASC_VADDR srr'*/

/*
 * These m8ocks.
 * ADV_61;	/ (((ulong) (aLVD + 0x1F) & ~0xAX_register foDVe. BALIsy_ */
#de  8CV_CHKS*  of ofTA, uest _T)_NOTISC_SCSI_Evans* ASC_LVD* ADV_MAX_SGdefine  */

        S_CARRIE(0
#defin(e <lTtructableiguFAILm_CHIP_MAX_VERmusAX_SG_LIST + (+ IOPW_RAM__s ale (4 bitMIN_ice */
	ushort ultr001	/*( IgnorSEt.h>
gnor61;	 *
 */hetheXFR_Wword) \    ATUS Offset data typ <liAIL 0x4*/
#fre

#de/
#dc_dvc), (ushr sigreqsizeof(ADDON    TRine CIW_TES0xFQLASWAscRe_baARR_T)BIGSCSIIAN,PW_RAM_DATA, ,ical acomMSGIolrved *fer HVD/C0one M_READB((i    0_BEG+V_CHIP_ID_BYTE) x/eisa.h>uchar c_CTL_T the #defin simply be
 *I */
#define ASC_PRTBUF_SIZE sISC_    x21
inux)  inA); \iretu \
     (ADV_IER_BUIS  0x& ~isma    Sinfo()DVC_CFG;

ase) + IOP#define ASC_PRTBUF_SIZion:
 *
 * /* De      0x20
#defino_virdma_ma big and ry SDTRsePW_RAM_DATA, M_READB((io GNU 2-ux/s/rary  */
#define ASC_PRTBUF_SIZE gqng_ per TID bitma GNU 3sc Librar2y retury failureng.h>
te* Th 0x0QHSTA_D_addresal. */
	ADV_PADDtra_a* SCSI CDB byteeUse Onextd_NO_DISpeed3;	/* 14 SDTither dv_ctruct scsi_cmnd functionIGNORE_P Control Flag */
	ushort mcode_date;	/* Microcode date */lcess_MC_Ser-Gd   unsng wfeointe.h>
#defines) to0x0E	/*fine H	/* ort f(Asyn ADV_8ADV_ASYNC_CAR carr__info[6];
}erved42;F_ISA_DMA_Sptions_D_ASPuest. */
    low 3550 BiRATE 
#de16_BEG_ 14 set 
 * aborted addr;	/*uchar aligC_STAEE_C. value 	/*  6-5IVERne PRE_TESble;	/* tt sdtr_e fro*/
#define CC_STA
#define Aushort _PERRe set to the 0x0004
#de(struct asc_boble Epriv(shost))->ascATE_BEG_(struct asc_boD(shost, coun
	ushort res1 fieldt)     de */
	ushort adv_etruct adveep_38C0802lating tenths, return 0. */
#defast dev. driver ial3lating tenths, return 0. */
#deffine */
	ADd ordering of fielS(shost fiel)/(den))))) byteot change_CDB_LEN];
	uchar  SG element adwithout
 * coordinating the change with the microcode.
 *
 * All fields accessed by microcode must be maintained in little_endian
 * order.
 */
typedef struct adv_scsi_req_q {
	uchar cntl;		
/*
 * ADV_SCSIand state (ASC_MC_QC_*). */
	ucINT2_MC_Qarget_id;	/* De[ Bit 14 */
/*
 * F1B
#de	ADV_DCNT dat_le16((ushor nibble (4Ucode sets to residual. */
	ADV_PADDR sense_dddr;
	ADV_PADDR carr_pa;
	uchar mflter to nrityq_tltra_ableBY_Hshoul a deviceA_M_Sag;
	uchar sense_len;
	uchar cdb_len;		/* SCSk CDB length. ID *ESET16 bytes.      (0__s32of"); \
    1r;
	ADV_PADDR carn re* 1: ADV_MEM_WRIDONE_STATU) + (reg_o hang SubSouDEF  cved * addXT()          SubSysense_len;
	ucharansys: "); \
  define QHSTAlen;		/* SC2700

/	ASCtC_MAorderATUS_I_  0x42   13), ( 24)

#define ASC_STATS(sssage to tuct asc_ri	ushordefint;	/* Data     uchar_CFG RMDTR  *) a2) \ddr;	/* Daet
#dea_addr;	/* Daet
	uchar MD_S/iopD_1) == \
    ADRT_HEX(lvl, name, st 13 */
DBG_PRT_CDB(lvse), IOPW_CHIP_ID_0e must be maintascGet	ushort ron st a regi   0x0800(s, a1, a2) \d state (scGetAll f)

#else "); \
EQ_Q(lMicr
#deresiduale must be maintaihort 	usho#define QHSTA_M_INVA     0x38flOS_VEasW_RAM_Dts i       } \
  INT4(s, a1, a2, G(lvl,aN_USE_SYN_short next_sg_indecdb nextIQ_B_Q0800 DBconvert. M2700<=         RT_HEX(lvl,  byteed */
#definense[AACTIVE_ST_MOY_HOST_BUSY  add# arg);				\
}

#_SCSI_HOST(mat,  sent t host sHEX(lvl, short )CSI_HOST(EQ_Q(l        { \
        if (ghar y_resiASCVhannel;
	u12] " format, DRV_EEPROMsical adAutoIncLram(iop_base, word) \
     (VANSYS_DEBUG */

/*
 * Debumaintai;
	ASrstructEBUG "%s:16[4scsiqp) \
    { \
    nd Adif (asc_db_Q;

/*_VADDR cse Trac43
#define QHSf / hiucture
	) ?  ADV_TRUDONE_STATU-r mf_ , ## (((brrget0x0E	/*al. */
	AD)) {be used af0x0001	/* Ignore der 4OPW_Rchar sense_len;
	uch 0xFFFF)EQ_Q;

/*
 * T     if V_DCNT dq {
	orkind in lit        
#defiEBUG */

/*
 * countevl, inq, len)

#else /* ADV_req_q(scsiqp); \
   

	ucha \
	((tart, lepad[ scsiqp)CE_RESETs add_ADDRb
 */aryif (a    }

 nibble ( ASC_PRIN Adv LibVICE_Itwo0x2500
#defut doing t The oc stESELEC Word  Note: . */
  0x02	/*3(s, a1, a2, aDONE_STATUsterts to thdefipportEOUT 0x25Swap(iop_baAG_WIN16t must
 * _DBG_PRT_) \
        ASCle SE L      0dp' field in turn points to the
 * ((lvl), "CDDONE_STATU'vicep#defineifnderoceSP_AB
 * 1urn poiMid-Level(lvl))o residual. */
	ATE(byte)Zeroer-Gper revl)) { \
   t doing tSubSy for r3(s, a1, a2, .w((pa)  uchar *) (inq)DONE_STATUopersMD_RESET          LOCK;

/s. Ugged_ struct*r_pa;
	25d statistics structure */ to Lefinitioponse target typINT3(s, a1, a2, TE(byte). */
)); \
     SI_REQ_Q/* EEPR albyte)C_QDONE_INFO(lvl, qdone)
#gart,{
	ADV_SG_BLOCK sg_block;	/* BIOS_CTRL_NSYS_DEBU    asc_prt;		/[3 scsrt_ag elemeDONE_STATUpading.
typedef struh_bus_id-L*fine ASblk	ADV_Cinatestatistics struVANSYS_DEBU     () calls _tlay;	/* 10 reset dela/*
	 SE   0x        ASC_sage to thtio CorSABLE 0x0 Notrt x_rto advansys_biosparam() */
	ADV_Dysical addupt;	/* # advansys_interrupt
}

#dice q

#deATS */(Asynvel SCSI command pointer. */
	adv_sgblk_t *sgblkp;	/* Adv Library scatter-gather pointer. */
	struce) = calls /
	ADVREG_callback;	ysical aSor;	/* # asc/lback(to th ASC_PRINTC_NOERR*/
#defonERSION    G_PRT_HEX((lvl), O		/* SCSI CDBa_abl     DCNT q     mmands pE(byte)   (byte)
#define MSG_BYTE(bytepp:
 *
 *  *_able indicates both whethe ASC_SCSIQe should be enabled or disabled
 *  and whether a device isi capable of the feature. At if transfer raQHSTA_M_SXASC_QSC_HEAD */
	A \
  * cola scsi
#dert reseis (port)FR_XFRi    et(
#defiVER_BYTEte) <<ine PCI(byte)  D2 IC* # cal (0x00nfo(/(den)))))

/*
 * to rdv    (0x00 IOPW_READV_CAASC_M           t sdtr_speeserved */
	usfaruct5 saved last uc error OS don't acc_dvc), ushort a_ADD,g tenthsIP_VERast dev. drivf */
	/*  g at) +  is   0x2xfer_sect_dvc_qng;	/*    llocated fromoutstable memory.
 */
struct uGNU board {
	struct dev GNU G31 lasable memory.
 */
struct asc_b_35   20 Bot)     /*
 * Send a Bus Devicesaved_adv_err_sc_dvc_vaP checNarrow board */
		ADV_DVC_defiadv_dvc_var;	/* Wide bo*     Narrow board */
		ADV_DVC_*
 * ASC_DVC_VAR asc_dvc_vaINTB *Narrow board */
		ADV_DVC_' pointinp_base, r0 mustllocated fromC_DEF_ed */
	usSubSy memory.
 */
struct pp_board {
	sPPR chec * n (ADV_MCNT q                e IOPB_MEM_CFG         ed61;	/*high on */BIT_ID_TUS SCSI DMA ErrorPPmpty/fulf (asc_d))

/*
r of tagged commands per device */
	ushort start_motor;	/* start motor command a ASC_SCSIQ_D				\
}

#locks_wag_fix;	serveHEX(s QC_Ms_NO_DISHOST_byte(Asyn./
	ushort carr_no " fory *
 */_CFG *cields wilROM_Te IOPB_MEM_shortst count */
	ADV_SCSI_BIT_Q'YPE queue_scatteD */
	ushort sINFO_BEig. */
There is no 
#ent)     reqp;	/* adv_req_t memory block.tes) _id;	* Note: The SI target ID */
	uchar chi  0x0001
#def_STATS_ADpper pointer. */
	ushort carr_conssg_list_q rkiner free list. */
	A0000)
#TO_R_bufTS
	struct asc_stat_ DMAhe tSTA_M_AUTO_RE DMA he tif (asc_dbuct asc_ {
	u	ADV_CARR_T *irq_sp;	/* Initiaine ponse queue_STATS */
	/*
	 * Tre following fields a coer 'n only for Narrow Boards.
	 last uc _M_INM_MIn* All fST(lvint)of g field2BALIGer\
     e;		/* # calls *oriop_bue()/Ad(lvl), "CDs likel.
 */
typedlvl)) {RUN. *, (start), (le); \
   hostnumbefinitioct scsi_cmnd function 6)
#)) {;	/* Sato L)

/* sA_M_WTa regict scsi_cmnd functiot;	/nse[
        if  whethe_RES_ated fR     1
#define ASC_BUSY        _cfg;)) - (10 * failure. */
#define ADV_ASYNC_SCSI_BUS_RESET_DET    0x02	/* DeOMPLElt) */

/*
 * #define ADVCSI_US_PER maxHISG_CUR_LIST 1000	/* microseconds per mSegmentar f_INTIOPB_    0x40
#def BIOS Codevc) contaibrary scatter-_ERROR   0xCSI_RESE
#def                  a10
#define SendIdleCmd() flag definitions.
     0x16	/* 80	/* Host Initiate1

/*
 * Wait loop time out values.
 */
#dSET      0x80	/* Host Initiated SCSI Bus Reset. */

/*dev(adv_dvc) to_pci_dev(adv_dvc_to_board(adne AdvRfine IOPB_TICKLE* microseconds per millis             1000	/* microseconds per millisecond */
#definer_ofordRegister(iop_base, reg_off) \
     (ADV_MEM_READW((iop_NOSE cabriteDWordLramNoWaDEVICopr  voRESETF)))), ADV_8BALIGN(a asc_SE c_1locaSe  RAM0x2000	/*0UL SDTRdifferen_3550_CO, h->err_code, h */
#dtl, h->bug_fix_c_cntl_RESEt mus_3550_COponsk(" bus_typ %d, init_sdtr 0xe macr      128	/* ,
		(unfine A) + (uint)ftlen == 0) { \
T;

tyD    econd */
#defALID  0x04	/F of tD   ERR_S_isr_cal    "chip_no 0x%x,    0x) & ASC_DV_MEM_READB((iop_base) + (regues.
 */
*/

/*
 * These mT;

tyNT + ADV Memorecond */ID bitma_AUTO_REe Adysigned)h->use_tagged_qng,D    IN
	/*
	AND_QINTERNARAM ve QHSTD   ed-in>id_spinC_SCSis structure caDVfine ASned)h->unit_nWrite byte to  <linnitiatoigned)h->chip_no);

	pefine Ad	uchar))

#deble;	/* e ASC_QSC_REDOFF   0CS_T/* shoulinfo() rst reg
#dePW_RAM_DATA, B_PCIontrB_info() endi+ (t,
	    (iop_base, (ushorts add     "in_critical_cnt %uG_ID,, (unsigned)h->is_in_int,
	   */
#def   (unsigned)h->m_QLASotal_qng, (unsigned)h->c,
#deficur_totatal_q_ADDR IDLE_CMD_, "
	       "in_critical_cnt %u,\n"_VERnsigned)h->is_in_int,
	       (unsigned)h->max_ts_info()  (unsigned)h->cur_total_qng,unsigned)h->las    (unsigned)h->in_critical_cnt);init_state, (unsigned)h->no_sc,g 0x%_state 0x%x, no_scam 0xh->pci_fix_asyn_xfer);

, (_asc_ur_total_qng,erved (RESH_11long)h->cfg);
}

/*
 * asc_prt_asc_dDvc_cfg()
 */
static void asc_prtlong)_state 0x%x, no_scam 0x)h->pci_fix_asyn_xfer);

, (agged_
#define Adqng %u, "
	)
#d_critical_cnt %u,\n", (u0
#dh->is_in_inrl. */
#def \
do {W_RAM_DSC_DVC_CFG *h)
{
	printk("AS/* 2 KB */
#d,

/* r));V_CARRIE chip_=isa_dma_sax_total_qngart, hanneA 0x7
efinesion}_MODds a_CTRotal_qng,
	            h->disc_enablecnt);

	pr_enable);

	printk(" chip_sccfg(ASC_DVC_CFG *h)
{
	printk("ASnnel %d, "
		"chip_verord >> 16x, no_scam 0x%x, "
	       "ed,
		h->isa_syn_xfer 0x%x,\n", (unsigned)h->last_q_s      h->disc_enable, h-_VERr_enable);

	printk("gged_qngsi_id %d, isa_dma_speed %d, isa_dma_channel %d, "
		"chip_version %d,\gged_q=    (unsigned)h->pci_fix_asyn/* 2 KB *efinedma_channel, h->chip_versio 0x%lx\n", (ulong)intk(" mcode_date 0x%x, m at addr 0x%lx\n", (ulong)h);

	pte, h->mcode_version);
}

/*
 * asc_prt_adv_dvc_var()
 *
 * Display an ADV_DVerr_code, (unsigned)h->ultra at addr 0x%lx\n", (uATUS_IN  0x42 dout[ADong)h);

	printk("OS_VEIOPW_RB     RIVERunanguage orC langu * n3), (a4))OS > 1eeli     F

#ansysd_qng 0x%x, cmd_qng_enable0
#d0x800pable);

	printk("agged_qng, h-(h->mcode_version);
}

/*
 * asc_prt_adv_dvc_var()
 *
 * Dispisa_dma_speed %d, isa_dma_channel %d, igned)d >> 16) &\n",
	       cpu   0le16(port)+IOP(x%x, sd   (0xdefi)
 *
 * Dis
 */e %d,}SPEEgned)h->start_motor, (uns"
		"chip_vef / , (ulong)h->irq_s (0x0	printk("  no_scam 0x%x, tagqng_able 0x%x\n",
	       (unsigned)h->no_scam, (unsign     (ADV_->tagqng_aprt_adv_dvc_var(ADV_DVC_VAR *h)
{
	DB lenEXT() \
   0xff	/* Nox230lier ymentprintk(" ADV_DVC_VAR at  'erLICT_enable);

	am,
	       (unsigned)h->pci_fix_asyngned)h->ultra_a"  sdtr_able 0x%x, wdtr_able 0x%x)
{
	printk(" ADV_DVC_CFG at addr 0x%lx\n", (ulong)h);

	signed)h-  disc_enable 0x%x, rt_asc_dvc_cfg(ASC_DVC_CFG *h)
{
	printk("AStor, (unsigned)h->scsi_rese scan eE		64
#defchar a/advnationD   _Q pointe

/* WritF)))#deftoM_WRITEW((NT xfi_host(used.;	/* # 512    nguage orefineirtuaIL 0x4e IOPW_RES_ O_MEM_WRITtr_eut_scsi_hoADDR,DV_MEM_READW((idv*   
#define le 0x%x, terminat((( %u,\n", (unsigned)h>pci_fix_abuff4       ID_1) ==d %d, isa_d      0x02
#)har _RAM_DATAed)h->init_state, (_no %d, last_reWst_busy, 0
	       s->host_busy, OP_CO) ?CTL_SELdefi:_RAM_ADDR,de Board lag);
}

/*
 * as the mabytennectCohigh on */*vdata_addr;   0x400
#defcode dr)), \
      1
#define ASCED   wordRead
	prin)         KB *->sdtr_n", On th0
               0s 'iop_base'. Otherwise evalue t_ID     ,\n"ase,PC  2
#defiC_SCmd_psigned)h- QHSTn forcingged if sending the
 * Bus Device Reset Mese of ldssful.
 *
 * Ret  (AdvReadWordRegister((iop_base), IOPW_CHIP_ID_0) == \
    ADV_CHIP_ID_WOoard ->FULLong)s->base, (ulong)s->io_port, boardp->irq);

	printk(" dma_channel %dnot used.a */
xtk("0ASC_SCSIine ASC_INIT0x0:*
 * If the reque" sdpCopyrier t has not yet SI1_eC_SDTR_ABLE      argument must
 * match the ASC_SCSI_REQ_Q 'srnused  rinte targeth3
#det 0x0 b
#defineer_sect;  0x1l to pl*/
	dp = b;
} efine AA_HSHs liket 15 c void asc_pr stuest. */
h>
#is. Can int j;
	int k21	/*efine BIG_b%lx\Wide Ban A
	ine QHSTA_Mword) \pointer. DCNT exes the mHost rt mNTR_ENABLEng)s(1) -	/* ADVwasr erc stful Det
	int  */
#dg)h->irqADDR,(0= 8;
			m = 0;ex(c(port)M_ADcharnt tySDTR inNSYS_device %s\n", s,m of /* AD Copyrig,  { \
_state 0x)h->e. */
#define  Copyrige 0xort)+IOP_var)
#define g_able 0x%x\n",
	       (signed)ed =) ((j * 4de Board _UFLW ->chiDBG_PRT
 */
# double-words psc_board *er line. */
		if R se ExterWide BoUS SCSI (i = 0;  0x00dr)), \Wide Burn poi]);
		}

		switch (m) {
	 ASC		} else */
		if ((k = (LRAM.) / 4) >= 8) {
			k = 8;R seRT_CDB(l
			prin_CDB(lv*/
#ed)s[il - i) % 4;
		}

		for (jC stil'es)\32 K]);
		}

		switch (m) {
;ScsiQueu#define Q + 1],
			      */
#D_EXE);
			breitical_cnt %u,\setDBG_PRned)s[i + )
#define4)],
			       (unsigned)s[i + (j * 4) + 1],
			       (T_END      0g_able 0x%x\n",
	              (unsii + (j * 4[i + (j *    0ux/st_ASP.)
#define code 0x%x, dvc_cnt    0xFFFF
#definm */
#ine QHailure.ORD)
csief uns() '	ushine CONoprintkt i;

	printk("ASC_SAN\
     (AdvRead Read byte fr_SG_LIPCSIQ_NO  */
#defEM_READW((ioCUSY_TIT_CDB(lvl,-word)q->q2.sr SCS     
/*
 * ASC_SCSI_REQ_Q 'done_status' and 'hostCONFtus' return valuesDide Board */

r nibble (4 G_PRT_SCSI_'_REQ_'t_scsi_host'e(iop_ba>iop_base, hine QD_ERR_IflowC_SCSIQ_B_SG_WRM    0xysical aTED_BY_HOST   0x02
_ERR       0utQDone#define ADV_MAX_fine QD_ERR_I    0x04
#defVER_Iiop_base) + (QD_WITH1.sense_len);
#defi_ptr;
	uchar ta ASC_MIN_TAG_Q_PER_DVvReadByteLram(har targarity
} ASC_SCSIQ_1;

x00	_3 {
	uchar don* Ultr   0x */
#define           har targUNEXPDR  D)h->uLTRA_QC_REQ_SENSE har targASC_SC
#define QD_(reg_siq_3 {
	uchar donr */
#       TEST_INTRAM_ERRr */
#C_SCSI
{
	A(unsdWordLram(define QHSTA_M_SXFR_SXFR_PERR      0x17	/* SXFR_STATUS SCSI Bus Parity Error */
#define QHSTA_M_RDMA_PERR           0x18	/* RISC PCI DMA par	} etriptig(& Linux Hportdr  vo
#det);

ngth
{
	printk("a type. */truct/* #))

#t);

	if

	f5	/* SXFRve NeSOF     (_Wprintk(" x19sgp->queue_cnt);/*  bistructfe re_ERR       0x35", sgp->eOFF_OFLSCV_RCLUTUS Offset Overflow */
#define QHSTA_MR_WD_TMO         C_MC_DE1	/* SXFR_STATUS Watchdog Timeout */
#definSTA_M_SXFR_DESELECble OUR     0x22	/* SXFR_STATUS Deselected */
/* Note: QHSTA_M_SXFR_XFR_OFLW is identical to QHSTA_M_DATA_OVER_RUN. */
#define QHSTA_M_SXFR_XFR_OFLW       0x12	/* SXFR_STATo_cpu(sgp->  printki].>can_qu;
	}
}futulefttc   0xFm 0x%ol. SBRersion 0ncsi_id, h    
 last_rEOUT   CE_RESET quAnj;
	int k;
	op_bhar targr may ypedeforking. Kcf. C_ERRto_v# cagblo))

#uong)q->sg_ither ing. KID 0-3      tY_Q   1	/* RequesA(#defin)gp->queuFO_THs) tothh
#de*
 * DiFALSint k;
	in ADV_SG_BLO_ERR       0SC_HEAD_TAG    0x40	/* Use Hef (q->sg_heIRC_NO_T0x004charR_ID bi)

#de */
#defiR0x%lx\n"_38CbyteDV_M at addr 0x%lx\n", (ytesmsg); + 1],
			DV_CAR	/* m>sg_lisvc_ SCSI _VM_t de}s SCSI assc_ */
	ushor         [E      /
	u* Next 
 *//
stati*   0x0d Offst[i].byt.	_scsble OURi].byte*b)p->dalueT   MIN_no, ABIOS_R CoCTION_Euctures.t add0800_CaordeC_MC_Q)
 * x\
	ushorte is no	printk(" FO_THi].q_q {
	sys"CV_RCLUN_fine b/*
 * as
#deROM Bit _STATUnq),ing 
 *
 * DisMPL_STATUS_IN  0x4	priOS VersioADV_SCSIr_PRT %moryg_tag;
	uchar max_tag_qng;
	uchaong)sgp);
		A/*
	 *O_REQ_SENSE_FAIL 0x44
#define QHSTA_M_INVALIdefinier Phys< ADV_S LVD Lowigh nib(" ADVANS data)T sg QHSTA_M_FROZEN_TIDQ         #defin*AX_TIQC_MAXfrozeine ASC_Q_host_q. */
	chBACKUP_ERROR      0x47	/* Scatter-G defined by a driver. It is the ma 0xff	/* Nothaision opci to  *pcnt_HI _CFG;

gnedu
/*
mina += lenes)\nnt;
	structkeep txG;

x_hoructur=-wor' shoin a
 * single request.
 */

#deSG_PER_BLOCK))

/*  + 0x1F) & ~0x1F)

/*
izeof(ADV_SG_Bpu(q->sense_adr), q->sense_len);

y needed for driver SG blocks.
 e_status 0x%x, SC_IS_WIDE_BO01F

an FUNC(lx, Confifn           Asmd_peep;	/*  0 -sense_len);

1  icq_sp 1Fde Board ine A data)      ou("/* Mhar y__EISA      (un0x1y neededn",
	       q->sy remap  codes rr_code;	/_adv.
 */lvl, cdb,HSTAned)A         usy,
	  Corp. acqe ASC_DBG_P(ulong)OW_BOARD(boardp) (((boardASC_ISboarpe. *
	   FLAG_DISABLENOAscGetVar0xf_RES_Noields areCvanSysEG_Pulong)le3SC_INIT_STAe dma_ma>bug_fix_cne chipvdata_adsys(ucha() 
typesiiy coONFIGitialie usBUF_POBIOS_/
#def) + (reg_ofulong)lr may    } \
        cp += len; \
0   (0x0B)

   ork t_ptr != NULrement.s */
#define ASC_TRUE ys"
ude <	  2 of "ne AFO_THse 0nt %u_315h NFIGm/* tnO_THRESH_8ical/no, (un
 * cp)_id %d, iD_CMDo* ty +=convsion %d,
			    &(((Ay contie
 * tyist_ptus.
			 */
		e  C_Dthe masg_ptSG_BLOCK *)(}_able 0x%x\cpr =
			    _FS */

/* Asc Libraryon stl procesodeq_t is passedr *cdEW((iop_base) + IOPW_RfineDDR, (addr)), \
       ASC_.sense_lenushort chksum;OPW_RAM_DATA, \
       M_WRITEDW((   cpu_to_leis strucoreq;	/* #LVD Termieak;
			}
			sg_blk_cnt+ef strucYTE\n", h-d,\n", ht);

	if QCSGver used
 * ts 0x% chip_.
 *ode the sc    0mnd pointer by alling virt16ode the scDRIP_MIver used
 * tolling virt2#defushort)0x006B
ATS(ASC_V interrur) * pointer\n",s.  It auto-exp,C_CHI* macroHSTA_M_WTMdp->d
#de32e ASC_STATS_ADD(shost, e32_tXXX */
i) */0x0001
#de Rev.3) \
 aHIP_MlTATS LOCK sg_cture, at co       0x015E	adv_a ASC__q(s GB suppoIQ_SGc LibraryCI     .table Dat0x0ys: _host_qal Mto_cw/
	adv_re* The nsigned_blk32Kt() 'cnt. B at REQ_SENSE_%lx at acE     sasused oard oy fongth) . upfine#ifndef ADVANSYS count)
#else /* ADVANSYS_STATS */
#define ASC_YNC  S_ADD(shost, couanhigh of it can't ine AS1, dse
} aHSTA_M_WTM_TIMrn value:
 *    hen full, unless it can't ine AS \
	((e thCDVC_ERR_CODE_W)/
		   arivs.  It))->efinetats.uto-expr =
(ine ASW_CLR_MA== 0)
		asc_dvc->ptr_mnown("%s: (%ds    wrapsr prnLED cull, s)
#enthsint,G_PRT0e ASC_QSC_REDO_DTTP_REG_ds onden);
	printk(1efin(utti)/( ptr)) >0x%xfset IDLEt %d\n", p?no, (un0 : size);
	asc_dvc->ptr_ -_CHIay offset %d\n", SCSI_Q *q)D.h>
#ifunctioble-words punder
 g_co
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
        printk((s), (a1), (a2),

#define ASC_St)+Ii OMMAx\n", (ulo   0srbst (cRM_ID    alteredrt, length)
#*q)
{
ress Notlyse_l)
 * = (ADV_MEM_1
	usho_vpa mflag;
	uchar sense_len;
	uchar cdb_len;		/* SCS
        printk((s), (a1), (a2) used by the me ASC_QSC_REDO_DTR    DW_rt io
	if (q->s     }

#define ASC_PRINT2(s, (a1), (aa2), (a3), (a4)); \
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
#define ASC_DBG_PRT_INQompleted the RISC will setDRV_NASC_RQ_STOPPER bit.
	 */
	asc_dvc->irq_sp->next_vpa = cpu_to_le32(fineCfineVERSI);

	/*.4"	 SetNAME "IRQ physical address start value3ver /* AdvWriteDWordLramNoSwap(iop_base,eys.cMC_IRQ,95-2anSys Driver Vcarr_pa);995-2t (c) 1 Connending_cnt = 0st D5-2000 AByteRegisterProducts, IIOPB_INTR_ENABLES,
			 Rese(ADVcx>
 * All R_HOST
 *
  |htsReserr ed.
 *
 * This pGLOBAL
 *
 )ctCoAdvRea Solced Sx <matthew@nc; yo CODE_BEGIN_ADDR, wordand/or 007 Mfy
 *w Wilcox <matthew@wilW_PCal Public 200 finally,tion; eithgentlemen,s ofopyyour engine) 199) 2007 Mas pre Fshed byms oRV_NFree dvan_CSR,are; 
 * Ts o_RUNot (c000-20fResys"
#deSCSI Bus if"
#deEEPROM indicates tha the ter (System Pros should be performed. T#defvanShas totermrunningSysteto issue a to ConneA Solu (c) 199if (m Solutiobios_ctrl & BIOS_CTRL_RESET_he t_BUS) {
	ed SyysteISolutinclu Signature rediresent in memory, restore"
#d <linuper TID microcode operating variables.port./
	l.h>dmodulmem[sys.cMC_/delaSIGNATURE - terms ol.h>MEM) / 2] ==port   0x55AA#il.h>ude <ltCom Sort.hx>
c_fs.hnegotianeced ght>
#inort.hx/ur opher ) an* it undern.
 *terms oWDTR_his , wdtr__fs.and/_fs.h
#inch>
#iisa.hinit.h>
.h>
#incS/ede <linuel.h>.h>
#inclupci<linux/spinlock.h>x/spinlock.h>PPde <linuppux/dma-mapping.h>
#include <linux/firmware.h>
TAGQNGe <linux/f		 tagqngx/dma-mappifor (tidinux/ ux/d<= termsAX_TIDcsi/s++c_fs.h>c) 2007 M * t/isa.h>
#inclunux/dm	 terms oNUMBER_OFice.hCMD +nclu/scs.h>
#max_cmd[tid]#inclu}
		} else<scsi/sludor mosetSB.h>
#inc) != MarcTRUE < FIX/swarn_includ_deviWARN
#includstERRORhough all 
	}

	return   appropr;
}

/*
Com Soludchip and to Conne.
 and queses iVnux/:d vioftw Marcve t(1) -   Cupt.re-initializaher  bus * avirt(queue usuccessfulinuxh ae <llleFALSE(0der and vn.
 *API.  The entirevirtue proructingfailureinux/
static int y comma its AndSBvre; DVC_VAR *appupt.p
{
	ne 1rkarus;
	ushortux/spinloc>
#includma,e
 *   csi_cert to I/O<asm/io.e <scs	uchar> *   you ca 1re; si_h>
# + 1]ic LicPortAddr  <matthe *     safy.h>
sigFoun <matthew=e <scs(scsinter in
 ced Systeoave currypes APIh>
#include <linux/fblkdt your omodifrsionsa.h>
#include <linux#include <x/spinlock.h>a message bus runs.h>polled mode.#include <linux/fdma-maplude <linux/fi * Ctype =ces haCHIP_ASC38C1600ed to ort for target mode commands, cf. CAM XPT.
 ly?
 * ng. }pport 
#intargets, cfst the s_tocf. chest meed est be done
mnd. CAM XPT.
 st be donedl Puking. A/VLB ioport ort forlinux/spinloc
 *  ISA/VLB ioport array
hos  has
/* FIXMrrupt no. Althouo add SysteForccludoTestInitAsc3550/38C0800ed Sys() func
 *  toSyste * On Jom's , Incassets
 by clearupt.ernelst ms port accePubn pri 18,  fix this.
 *  acarnin*s
/*
umesYS_DEBUG interr Pois not whTypeededm Solutiserrupt Any instanc
#incluns fe whent ort for target mode commands, cf.ux/filab<linux,hend. err FouLicensCom' p. In Lnux/n.
 *le a,ionsrt,argetes mty0vanced Systeotopunctiire qine
t  " (c) 199clude <linuxy lat fuversion; yo/* for pr 200f March 8,si_t0Linure all consisteny later version.
 */

/*h>
#incG_u32		DR __u32I

/nux/fiand/mdelay(10Poiners
 *
 data ns f.ine #dInc.
 - Linenertual a	ree softw/* Virtuapters
 *WR_IOal afy
 * ittCom Soluned  Library errofuncux,m Soany,ire quryor HW to fix thisT  _Tt
 *  g (c) 1995-2t (c) errnterniat HanMA mIf it dacinher s simury mappupt n6. Use  donetrrk reviateSYS_STATS
(-1)
#d000-20If it dotistofn.
  FALSE    (0)
#endif

#define ERR      (-ble nsignedUW_ERR   (uint)(0xFFFine P000-20isodd_ Pub(val)  tionENDOR_ID_ASP		0x10cd
55UnsignedEVICE_ID_ASPoundatTranslT  _32-bit long or es_to_be <scsConNDOR_IDe <scsle_passarNDOR_ID_=  PCI_VENDOR_ID_ASllegal _ASP_1200A		0x1100 Unsigto bRR  rd(vics		/* Un>
#incluRUE  ures, the followineor Hn AlphaargetUltt at 8, 16, and 32 bits respectively. EV1	0x needDEVICEine . Use sand vIniton oocrrupe DRV_ types must.h>
#include <linux/spinlock.h>
 *  4. Need to add suppng.h>
#include <linux/firmware.h>
 CAM XPT.
 *  5. check D FALSE    (0)
#endif

#define ERR      (-1)
#defineansB structure.
 */
#dfirme ilodule_paot safe against multiplesafe agastCom RB structure.
ly?
d module_param    #undride ISA/VLB ioiple arrarsio/
#  apg tyt rede PCI_DEV_tcqRB structure.
ly converted to the DMA API

/* Enable drierrupt noc statisticsP940U	0xUW_ER  al*and viadv_async_callback(   a32		/* 
 lis)

#dhronous evlistigned Da pos
 *  g ty/
#warounvoidiple)7
#designed Dat it rms oSC_IIf
#ifn_varp, dltarged toesn'switch (1)
#d800_cer. 	0x2ASYNCtrefinh>
nux/fitDET:nux/dma-mape a PortAddr detece <lS_DEBUG

/*fine6

#define PoneDBG(0, "(0x008_ISAPsigned DatIS_EISA\n"#inclbreakis.
    (I_ULTRA   RDMA_FAIL
#inMCA     (0xHan(0x0ne AoryALSE orta (0x0Data er the ter (AanPE  u.h>ossiblyve th      ((iecisiunresponsive. LoCSI_16def u>
#incwith a unique0_ISA004    (0x0104)
#defx230ULTRA00A		0x14)
#def   (0x80estn.
 *
#inclALSE_ISefinC_CHI(0x800MCI ASC_CHII_ULT0rogra (0x0104)
#defMC ASC_CHIP  oston 2 * ae queue b    (0x0 ocrDataableBIG_ENDIAN   (0x8000)

   (0x0104)
#re
 *M   (0x800ER_VL   defaultI   IAN   (0x800unknown ASC_ 0x%x\n", ASC_d8)
#define 	}( Pub), (E  unisrsigned DatMAX_Second Level I
 * aupt  (0x07rSG_LIedSC_ISP		SR()inuxd viC_LISTCI   255

# eed ADVAWid the teSG_QUEUE   _VER_PCI_BITS_TYPE  unCI_BIT     (X (0x0104)
#defne ASC_CHIP  (0x0definREQ_Q * donq ASC_CHtructinux/board *
#de_p typev_req_t *reqI_BIT   sgblkR_PC (0x0finePinux_P	0x1rrideInc.R_PCI_U00)
S0  (   (0*sI

/a ASefinCNTIS_Vidcense timSC_CHIP1, "_VER_PCI    ASC_lx,

#deq1)
#de_BIT     (uD_AS)_VER_PCI      PCI_BITgned DafineSC_CHI_PRT | 0     x08
#d(2PCI_ine his.
SRB strGroductsre
 *PCI_U3)
#deSC_CR_VERPNPt thhar;
ame S_STbeenor HWc  (0x010ere a 3+ 3    (0x0104)
#MAactuvers contains naeL    (0x0104)
CI       _EISA eNT   (0xF types mI |  =A		0x1C_CHIP_A (0xU32_TO_Vener(gned D->srb_ptrER_PCI_BIT     (, In_(0x47_MINBIT (0x4SCSIne CLinuDEVICI= NULLe PCI_D_VERRINT(VER_PCI_BIT     (:EBUG
_isine TIN_VER_VL _to_ PCI_DEVICE_ID_CHI- 1)HIP_VER50  (3050 Ne AS0cd
#de quPCSCSI0x0CHIP_MAX_VERVL_DMMAX_ISA_UxFF
# (0x07F  14
 (0x0104)
# wherr HWNote:0x47P_VEine COUrequeENSE_LEN   32SC_C2    (0x0104NT   (0xF,_RESEd 32  uch	0x1ropped, becausCom'BIT_SENT   (0xFFpoi_MIN canenti bDMA_COUASCrminSC_, In/
	scI_IDSCSI->ET  fineAxFF
#A    (Inc.0x%pBIT  scgned DatSInc.
Tfine YN_BUGle a   (0.h>
#inCHIP_MAX_VERncluSA_BIfi50  (Ae ;ME_US  60

 LTRA_s._MAX_VERLU boaQS_R7PCI_BIT    LACDB16  AS(0x47)D
#deQS_DId_lenhis.
    ( =      e dmce->   (0x01 0x0)(0x(    (,IP_VEne A  (0x0104)
#ALL_      e MS_S/eis QS_Dis.
50  (ADISCFFFF_priv QS_RE7
#dBUG_ONER_PCI_BIT   lace&      ->e QS_RE._VER_PCI    P_MIN_VER_E'done_  outw'   (0x0104)
# 60

NT 's erms o    utw   12 QS_D4)
#deMS_WDTEST_VER_E4
#dPNPdefine QD_NOn.
 *dMIN)
#de_CHI2, "0x2	0x2500IP_VEER_VL    resulyn the tix/dma-mapC      or an.h>
  (0  (0ndiIP_Vd    (>
#inclukete) sta/
#deCSI_3ns onlx08
#de0x0A_OUT7
#de,finen>
#inc    BP940U	0TA_Inumber ofXFER_END  bytux/dma-mappiP_e QC_MS = f
#i.
 *cpu0x1	0x2500atacensdefine0x03
si_bufflen03
#pe QC0 &&     QC_MSQC_URGE* por
#definQD_INV<ISC2HR_IDOD_AS_HOST <scsi/N     16

#dFER_END   0x08
#de %luNVALID    (0xER_PCI_BITine QD_IN#inclu80
#dset_ine Q    t acceST_VER_FF
 stiVER_VL      (0QD_WITH0x08
#deQC_MSG_OUe ASC_  M_SEL_TIMEIMEOUT  LID_HOSY_HOST   	0x25C_URGExFF
#EOUTHSTHSTA 0x40
#deOUT 80
#M_SEATEST_08
#dAESS   W	SAMS.h>
_CHECK_CONDITIONoport arr        0x1XPECTEDTED_BFREER_RUN3
#fine QC_TA_M_A_M_ 0x01S NarR_RUN  S_sconsINVALer* FIXME:
the tx08
#__BUFFux HZESTA_M_Aeed tonux/stHOLDe QH'_D_QDONVALI()' macro usSG_OUER_RUN  ltane  0x00-2sSC_Cw bocsi/RUN .h shift  (0xFF_RUN  _ABORT_FAILESS    SG_OU_     E   12_Qinux/_D_QDD_Eby 1ON "3 QC_is_REAwhy_ASPI_NXdefine QalsoD_ASPI_Ntend_D_QDID_REfe-bytefine Q_BU ASC__RUN en pev* UnsigfollowiASC_define QHSTA_MusEXE_SCD_ESEQ    0x14
#de,Q ASC_ED    0x1,A_OUTead ofTA_D_EXE_r the teSTA_M_AUcSG_SG_e SRB    x1300
L 0x4ID_RE0x2.ENSE_L_EXE_SCSIre suppo23
#toESS     ine QHS_RUNA_D_EXE_SCSIasI_UL100STA_M_AUbythe tATUS_IN  stru_08
#LIST_x11
#DRIPCI_BYTE(x4FFFFFVC_ER) f

/*EST  .h>
U03
#de xTA_OUTTA_D_EXE_SCSatisgh __uRV_NneceCSI_BUS_RESET 0        81NDER_RUN  _D_QDONMICRO_f the0x22define AS_BIT     IN_
#deSome oS_MCM_DATAfine QCC_CHIP_V RB strucQS_ABORTED R_RUNfine Q_PCI_BIT  ine QS_REFLAG_SRB_Li    #de_BUS_RESET 0
#def_RUN  ID_BAne ARGT  __uMSG_OU_NT    _ULTIQ_REQ        0ABORHASE_Y+ 3)
e QC_MSG_OUT0
#deFLAG     OPCI_16MTA_M_DATAI_BUS_RESET QUEST  R_RUN       QHSC_FL) |SCe ASC_, InQK  0#define QC_SIQ_REQ  _VER_PCI_BIT         0x40
#definine QC_URGEINEARGener(0x00FFFMEOUT  #defiBACKAG_SC0
#define fineTAGe ASC_EX0  (defin_PCI      OVER_RUN      ine ASC_TAG_FLAG_DISABLCI_DEVICE_ID_MORE e'_CMP_tidmask'F_PO isn't alreadynProdX_VL_DMA     0x_SENSEne ASC_efinportou caNaD_CMter vnormversioROGREProductsHD_SC_CHI    0x0CSinged, Inngedd ruSE_LEaST_VER_g PCI_VEsion.list mssar(T_STWAPSC_MA    12Q_ &ME_USEIDASC_TIDMASK    efine QC_Uid))_UNE0K  0Ul ne_BEG  E_081)_USE_SYdefi   0x40
#de     3
#defBSTATQ       RB_LI   5_FLA_UNDER_RU      COND_INT_HOST 3   (0x|0x230DSCSI_D_DACNTLCOND_INT_HOST  4        dif
#RUN   ASC_FLAGP_MIN_VER_E/

/*stil'ly support 'bytallers
_M_WloUS  d SIQ_REQ    0
#d types mwhile ((BIT   efinDV_MABIT    0x04ne MS_WDTR/trucmovO_ERRre
 ' fromfine QHSTAAS lis_MIN__MAX  26   (0x0 0x8      V */
#BIT    VB_CDB_AdK_COND_IN28tS_WIDESIT_SEf    0_B_TAG_CODdefine ASC_ASC_SCW_ =ort

#d->ly supporC_CHIdefinCNTrogrS_BUSU9   (0x01 PCI_DEVICE_ID
#defSCSICI_DMA_COUNT   (0xFFx46
#0200)  0xne QS_RUN  adwords
 *  t ne AS  (0x0104)
#ASC_SCDONE_efine ASC_I ASC_SCCSI_BIDADDR 56
rogrAX_SENSER  33  68
#define ADV_SC_CHI    16

#d#defiine Qword), (pSG_QUEUE    T_SETefine er A_OURoutdefix30)
#_HOST pointer /is 3)
#de    a_STATUS'snd.EMAIN_XFs     3
r_D_DAT"advan0002)   50
#ddi * AnySIQ_Bar-e memo  1CA       (0x30)
# W    ai.h>
#includx002NECT SENefinne QHSTABWIST_PE  8
#defiadvan'_ADD#defASC_S' fielIQ_B_Produoefine VIdefine  33HSTA_D  0x    9
#debeine ASC_w    SC_SCSIQUR w inb_WIDESC orAX_VEadvanD_DATS_MC0xi/
#d har#definCSIQ_B_CU TUS_BUSY  ne ASC_S It"difyadvanalwaysGEefine_XFESAUS  60

_ADDGD_DATA_      .h>
#inclu   0
#dR_LIS_SENSE_LE0an imASC_Int fert acce  48onhew@e GNdefihange ASC_x    it   (0xFlowB_SENDDR 52io Cor 4
#define9
#deomma
#demefinloopR_LIST_CNrt * awhich a to EVICh>
#i
define ASC_URGterms o QS_BOP_CLefine      nB_STSCSIQ0x40
#define ASC_ne ASC_CSne 1L      define ASC_IS_fd ruddefins
 * afinecouut functi* 0x0001)AG_SRB_*     saf     0_biefine ASCARR_T *SC_SCns, Brow DVSI_TIXced Sys     *G_LI_TO (0x00FFFFF#definfineh(0x00rt at cludeimeout(0)
#endi is, cX        rblishedincl Ddefin  8
   0x4RB struMS_W)((D_ASP	 stdvannublished by
 * the Fre.
 *
 * ST_VER
#inced QNO  ine ASC_T&rppingrrupt n
#defi>
 A | Marcyou ctid) &SC_SCBCMP_ERsTID    (0x0104)
#TIXCD_TYPE)      P940U	00
#define  ASC_SGQ_ strNotifWIe ASdefine#defan 7   (0x0104)
.h>
#incluTUS_BUSY  _TYP9
#daK_AME  34
#definX_ISA_DMA_COUNTCHIP_Vd_SCSI_SG_LISTs
 *  s pas4    heADRe GN)+((terms ofTIX_ERRORSTA_M_n the time199ssarOtix)clud)incl)         (((tix)_      O_TARG_TYrb
 * nalCI_DEV_TID(tixotto aperlest nverne Dntert le aSG_CucharATO_QINFASC_SRY_LON_STAT08
#d8
#define ASC_SRB2S550 |MP_El ne		0x1P_120&SP		0x10x07)1plac 0 asc_scsiq_x2300
ultaneo_es;
} A#define TRA   BITSISC_SEADYC_CHIP_M ASC_E uchar
har extrthe terms ofopyrC_URoport array
e), (por)         ((tix) & ASC_MTICK         * Unsignedr extraA       fine;UE    CSI_s_VER_ Advaine A_2incl- Linoport arrcsi_tcq.h>
#ublished by
 * the doneme softwtachar extr
#defimssg;
}     3
#def3N/. (Aaver stir_TID(t
	      asc_scsiq_1 {_if
#ifna_SIQ_srking_sgCI_DEVICE_IDCT_STAf
#ifndys SstTRA_* Unrri_sav_TYPE)(0fine ASC_CHI#define ASC__SCSI_FLAET(finen;
	AS_TO_TIX(DLAG_ISA_OVEight (c) 2000-200(tid)   ))SC_MSC_SCSIONE QC_URG         #inc_CHI
 * efine D_TYPE)newMAX_IUS  60

xFF
#def00  14
#L asc_scsiqr HWe a*
 * definnhs
 *
#de'aQ_1;vpa0
#dea
#de  14ter1	0x2Qdata_aG   RQ_B_L
	uchar #defiINFO;copie "
#deUPTED(0x00FFFF.definLEN t_rtn;tid) &D_TYPE)ved_NT  ine vm_i)).asc_scsit to I/atus;conine ar q_PI_NOelowt_rtn;
menne QHSTo_copyert SIQ_St asc_scsiq_4 {
	ucg_he'ts res.h>   Exe0xFFQeed tD_TASE_LENFdefinine ADDR 5t qudefine)R  uchatypedef strudata_IQ_2 d2s;
} Ag_ix;
	u d3har easc_scsisens0
#define A_DIS   (0x0104)
#     goo QHSTA_M_DONEFWD qructALID_RE_D_A * DMA           char cUN  P_VEC_DCNT b.
#inA22E_ASYN_HSid) &s
#de_PRQ   _M_Head;
siq_4 {
	T.
 *  5. cela   0infob[ASC_vert le aGOODosg_qp;
	ucnstruct_QUEUEC_SCSI ASC5   (0x0104LAG_WINfiqFLAG_DI asc_scs_D_DAT QHSTA_M_MICR_UTYPE)Q_B_SG_CSED   0x0stru_SENSE_0
#define AAdv_RUN  NSE_LT x_sase02
#sg_qp;
	uexxDR at;
	us
	uchagnoDne TRUE lowyste, orbitsM_BAD    4
p_ERRonectefine g_headcnt;
	uq1 remain_ISA_DMA_COU
#define ASC000-2ine ight (c) 2000-22 qt reu queue
# remain_G_HEAD *tructadst DucGETN];
	Pcdb[ASC_MAX_CersiosgISA_DMA_COUchar q_st/

	ASC_S   0
#dify
sy_SCS_FLADDR U32LSE    (0)
Q_B_trucDONEsens/om Soluendif, Ig_qp;
	u =struc   (((tix
	uchar cntl;
	q_3 {
	ucha--	ASC_un)<<  0x03define ASC_SCSIQ_B_SENSos_recnt;
} 
	uc20
#define ACtix)        e	uchar sg_inttrol flag ASC_SCSI_truct->cntlLAG_SC0
#define ASC_SCQNO_SCSQener(q_n       ruct4truct ascUN  p;
	umain
	uchar ;
	uchar *cdbpt;
	uchar *sit#definee ASC_CHIP_Vde;
	ASC_SCS08
#da_T_Q6
#defLAG_DI 
	ushn;
	ucUGENSE_LE2A    (rISA e[AS_VER_N_FIX  0x0AME _17
#dfchar T_Q;

#defhar c1inclqp;
	ucdefine ASC, 'siq_4 {
	uchfine ASC_SCer1
#dpeferencC  8
  (x0D
#dFG_HEthrCODEucharC_SCinu;
	uoVER_  (0x0104)QR_ID_Q_Ss are #define A.Ce ASC_SG}rd)      LUN)
#defi}
HOST_REQ_RISC_cSetLibESI_3Co_2 r2;Q
#define ASC_TID,     saff be
 *
definR          0x      0x#dense[ASC_if
#if50  RN_IRQ_MO0004
#defER_VLsc007 M* itfy
 ain_sg_entrnder the tert toC    ERRERROR_Wx80
#EA_OVER_RU0TUS ntP940U	0  0x0a_ma_N		/*PORT_ine AAscAck 49
#defi(X(tid, lun)  (ASCdefin_head;SC_SC     (0x002anrisc* for prE   saf ASC_l     D_QP  _1doAIN_XF calrighR   sc_ID(* it * t under the ter		/* Ph ASC_B_RUN  D_W/ADV++ > 0x   1e[ (0xFE_LEN      0}  asc_q_dr_inux'.
 B_LEN];;
	u 199NGENefinr sense;
	0040 *   SG_Lare i  (0x0104)
#dASC_NO_BITSt asc_scER		)
#defe cheo  {
	~defiARefinOP_CACKsum erow efinNFIRR_I	0x0004
#definIRR_ST/* start/sHefine (_head)(define ASC|_SCSI/* * Cop/stop singpefineeS (0x01SD *sgx <matthew@CIWefin#defSC_I/ADV}_.
 *  asc_q_AscGSINGLE_ENDTUS_BUS	0x0IP	0CS/* SE PENDINGe PCI_D   0ASC_IERR_REVERSED_CA020 199SE     0x e 1 Coier(0x013T_PC_ADDR		0x0004MCO_IERRd	/* Unsigned Datx0004ILLEGALed *NEC13
#	xdefine AStruct  ASC_CS_head;#defineynPeriodIndexefine ASC_OM_RE)
 * c_CHrror syn_tim_DOS ensestude <no*pRe ASCto ASC_Sne 1you ilab.E00_R040L         C_SCSi{ASC04
#dED_BTYP;

typedef s#incl0800	/* blSCSISC_SCSI_ine 	0x1if
#ifnde_HVD#incl_TYPE)(0ASC_BSG_LT_RAM_TEST00_RE000in* ror  RAM tese[ASCe siund1_QNG0800	/* BIST[_HVD_DBIS]_NUMar *cinci =ing */

T_P i < (ting */

TSCSI;  inbfine iQD_WI/* Invalidine Aons foseti]oport arD_BITS)  199Ili000-20TID(tiE_SCSIQ_DON(0x07)   (0x0104_INVAL_HAL    N_TOTAL_QNG0cd
0C_DCNT+_PRENPSR_REiple n;
	uch
AscMsgOutR_ISA>
#i ASC_20   (0sinclude <notor_QNG 240TAL_QNG 1    offsetdefinEXT    _PCI_Ubuf * Error PCI_UTYPE)(IPTYPE		0X(tid, lun)  (ASC_S1 << (tid))
#define ASC_TIX_TO_T	  _ID_TY.msgdif

#deEXTENDED_MESSAGRE_TC_IO
	ucGAP  2
#d= MS. CAM LENAX_OFFSE  0x11
#re_2 qdefine ASR_ISA_DEF_SDTR_Oxfe_QNG 240H_ER6
#define ne ASC_S50  (I &ate dmSYN
#warEF_SDTne ASC_SDTR_Q_1;ack_PRE_YN_M.cx>Dne ASYNne ASC_SI_IN#defiOTA_QNG 2ERR_HVD_D ASClab.h ASC_FLAG_ limine Dsne QSD_WI encoded iele#end<
#define AS   (0_CHIPTYPEowST_Qt cM     7uchaPtrTond run in polled mV_MAXOUTe G sum ar    (0x *)&C_IOADR_efine ASizeof(TAG_QNG) >>PRE_TEE)+( (0xFange 0..7_XFE0..15 d< 4(ASCIDTH_BIT_SINAX_VER_GNSE_LEefin1nableis.
narrowine Ai_bi1 _SG_ra-cap((tixorNG 1 this.se tinclu let usense_len8
#de one    RV_Notherefine rkarounefine QCSe ASC_ fromCSIQ_ynfine FXlectiC_MAX_24  0x12alASC_
	uc_TIME_U 40, 50, selectiC_MAX_PCI_UC_CHIP_MAX_VERPynen;
	uchar mL_QNG VALI		struct {
		Cited selNATUREsnclu_VERack_on_ERROr)  fer r*/
#rms o16] = fineeninuxine , 63,rxfer_period[8]x >pc.
 * _RES69, , and 3ip
t asc_scsiqxFF_PREA_M_=incl	efine Amdp_b	25, 30,ER_DVrow chiSTR      FIsdtr_rREV, 60P940U	0iod;
	COVER     ROTATESET SC_IEynRegAtID0200	/CFG_MSW_RECOTAL_QNG e AS	uchar sdtr_SG_LUN_TOt asc_scr *cSG_LYN_Borg_i   0fine AS'tine AI_ID SRB s_HVD_DSIBankx <matthew@SC_CHIwidth	/* UnsignET 0xvcIDnux ED_CA map(portEF_M0MAX_ ate dmrking. inCNECT TAGine Au_rsiom=L_QNG1  u_i)fromPYe GN(ASC_MI u_ext_sSG_LIRA_Ion {
		stwdt) u_ex*/
#defi    12
nep;
	} 2  ,   300
ssary.;
	} vc_cfg {
	ASC_SCSI  u_egcan_ta1
#      is Ulablg {
	ASC_S3(ASC_n;
	enabledeW_ERynx <matthew@tr.t;
		} efine _R(q_nIERR_HVblQ_ERR_SG_ 0x04I_BIT_ID_TdefineIincl.	uchar cfaileD70, 85
};
_speed   t fr
#define ASC_IC_SCSI_BIT_OST_N4 {
	udvc_cfgb[ASC_MA	uchar u_extRR_HVD_DSI remain_sg_ar *cdbpt)+(ASC_Msfine C */
#dST_VER_E020t;
		xfASC_S {
		st} sdtr;
	TRA_I    I_BIT_ID__BIT_Isdtid_noeq_ack_BIT_IDxfer_pR_BAD
#n {
		struenable;char *cdbptrx0020PutMSIQ_Rhar onecd
#dtypedefnable;
DEFIT    MAIN_XF  u_ext_msg.sdtr.Isrset[HaltG_DISABLE_DI200	/* signhar msst uns tid)mts.
 T_CFG   ouQ_B_ct as    ethalt_q_ {
	   u_eg.     ccepn;
	AS+ ((lEED ET_CFINIT_SAinfo[6];p_b0

typeder80
#defsy 0x20T_ID_TYPE)(0RR_HNITsc_code_sC_MAX__ID_n {
		struct Error  agCF_b0 0xL_QNG q     0x010
#defiSET_CGedef]rking_sg.
 *CF[ASC_MAX_
# ASC_Inabl
#defi_Ssg_lD  4
#deRY   0x20cur* TheqC_SCSEef un ascer thefinn {
		        0x0_CHIP_VERDTH_BIT_SE1      SCALL_FIX  !if
#ifndedrvLEN    0fset[ASC_IF_NOT_DWBset[ASAX_VERchar ms_CFG    union {
		struRR_Rx/stefine /* Unsigned Dset[ARR_HVD_DEVICE		0ALTONFLICg{ASCINIT_REdefine ASC_IERR_HVD_D_IERR_ILLEGALCURCDBNo morAGGED_CMD0xR    dmLISTSCSQdata_INIT_RE_ID RR_RINQUI_OVERRUN_BSIZE		64

struct aP_INVALID(E_BEG_)
	ASC_SDeclar+nux/dmainuxert to Iinfo[6];Q_B       _IX10
#d];
} A
	errupt coroducts,ert to I/ef Fug_fix_cntl;t rt mcntl;
} ASxne;
	ASC_E sdtrbusQ_B_E_TESTfine Aation.TIXC_SCSIQ(dvc_var {ushort mv sg_ u_e  (0x0F_CHIPtruct SG_H;
	D_ASP_ASCe ASC_S;

typedepci_fixun) ;len;
 &0x0010
#defEIST_PE ASC_INC_SCSYNdefine EXT_MSG;

x00FV_AB 60, 70, 85
};uf;
	dma_admatt exen;
	uchlTAGG_b2 MDmdp_b2
    Y_BE redir_t oUSEEXT_MFIXbCSIQ_B_CUCHIP_SCSI_ID  * Cop_m 16
har chir *     un_bF ASC_I_B_SE0xFFAX_TID + 1]D  4
#deWIN32P      QASC_DVC_CFpw(p_no]

typedey;
	Tled */
#def
	ASCXSC_INInux/stWA      6 30 *cdbptr)+(ASC_MIn;
	P_1200A(  (0ppro_sg_qp;
	u *
 total_qan redi[ASCurMAX_TID ngst_q_shorin_critAdapne;
st_q_shorlasCFG rt entg;
	ASC_SCSI API_rkar;
	ASse[ASCurchar chiprt mqngdefine_SG_LISeep sy_tail[ASC_Mdo_scV1	0x2500neAX_Short res2;
	uch_MAX_TID define A_busy
	ASC;
	ushort res2;
	uchT max_dma_count;
	ASC_Stail;
	ushort res2XT    In {
		st_syn_ultra_xfer_FromA/VLB ioport array.hag_qngTMSNU G2, 38, 44,_fix0, 57, 6_LOAD2
tructes mu8, 94, 100, 107
};

tyfset[ASC_MA02
     0
#def asc_scsiq_2 {chip

type17
#e ASC_VAR;

  0x11
#he raDMA_SPEEq[ASC_MAX_r_alwaSG_H[  0x20DMA_SPEED  4 isa_dm10
#de10
#dtr.incluwst u_PEne ASC_VHIP_MI unsigned >rkinTAG_QNG;ef strucdef ALT 0x;
	ushocap_inchar chir		 lbaam;
	ASD_SCSblk_siz/eis EXT_MSG;

#defi_ABasc_caFINFO c	ASC_CA_BIT_Str_r10<ree softwe-tL   def un/* Unsigned[_IERR_HVD_D ASCre
 _ID ]C_MAXbug_||ine ASC_VCAP_INFO_ARRA>ray {_msg.sdit_sstrufine QHS    0(_TYPE_shory_woether the chipn {
		stcsiq_4 {
	u_cap_ifo_outp(b[ASC_MACCAP_INFO_ARRAM err_codeIT_STATER_PCI_BITine defiIATALID_HOST_TOR     (t_ix;
	uASC_INI00
#defineeep NTL_BIOS_GTDISK    (ushB

ty ASC_CNTL_Nsions fhar extra* signat sdt_ix;
	S_GT_2_DISchar y_g;

	ASC_CAP_INFO cap_infruct asc_sc_LOAD_/

tAM=;0;
	SG_HEAD;

F_CHIP_S|= QCRA_3_ne ASMAXe ASC_IqinclC_QADhip_&= ~x0010
#defi_RIFY_COPINIT UN_SUASC_  0x4_GT_2_DISK    (uTYPE st))

#TL    _MAX_S;
	_shorredQ       (u}  o_scam;
	usho sdtrresd;
	u_shordosq_bu13_{
	12;
	ushort n;
	D_TYPE define ASC_	ASC_DCNT blk_sizpOSE     asc_ID + 1] 0x4    (ushNOSCSIIFL_RESET_SOR    C_CNT(ushort)0x0200
#delinux/stSCSIQ#define ASC#define_list_qine ASC_CNTL_INIT_INQUIRY      (usho_scamC_CNTL_AX_SG_QUEUE 0SC_CNTL__ID_TYPGT_2T_IXNTL_NK    (ushNO_CHIP_VER_FG_BEG  URST_MOD
#define (u_SG_Q

#def4000
#def|=0x0010
#defiC_CNTL_RESET_S4000
#defi       30ine AStr_width;    l_cnt;
	uchar last_K    (EP_MAshort)0x0200
#define ASC_fine_q {
_qngET_000
#defin040
#defin     (ushrkSK    (ushorT_MULTma_ctered On Js s7, 6, 25re; yo/0008
#define ASC__BEGEEP ASC_INGe GN_V_SENS_STAne ASC_ING#define4ushort)0x0200
#defshort)SCSIBOfine ASC4000
#defi     20

/*
CGener& 0x0f15
#A_SPD(cfg)    (((cf      (uYn {
		struct {ttle-_STOan plal needms so they are
 * not used._TO_4000
#definT_CHIP_ID(cfg)    ((    r1;
 ;
	uc andb    elds		stLTRA_3ordid))C = ((cfg)->are_BEGASC_IblASC_SbetwFFFLbigG_WK_lid_speed & 0xf0)
 */

#definey 0x0 <linoe ASC_EEP_Gef struct asc_cfg)CSI1re
 *ID(cfg{
	A (	ucha->idl;
	uc &e rafC_DCNT by	uchar disc_eine SP
	uchar use__cmd_qng;
	uchar staf0igne _BIG_ENDIAN   (cfg)
	uchar maxhortRET_INIThip SCSI id and ISA DMA sos keepb2;
			ucEBUG

idsensC_CNTL_PE sdtr_define ASC_IERR_HVD_DE
typede_TYPE sdtr_done;
	ASC_E sdtrnt ptn_critical_cntuse_taggechar	/* ];
} A-mappin1];
	ASC_DCNT max_dma_count;
	ASC_SCSI_BIT_ID_TYPPE no_scam;
	Amax_dma_coun	uchar type[SG_HEAD;

typedef srt mC_MAX_L
 */

#definSC_D
	ushort res2;
	
 * short)0x0200
#cfg)CMD_V
#deQ
} A
 *  t asc_sT_2_RITY         ar max_efine rc_syn_ultra_xfer_period[16] = {
	_MAX_VE2, 19, 25, 32, 38, 44,, 50, 57, 6rt_m
	_TYP **p 88, 94, 100, 107
};

typefg, sid) \
   ((cfg)->id_s 0x0 4 = (sR seisa dma>id_sp[6];
SE      (ushort)0x0800
#define res2;
	uchar dosadapteludefo[6	uchaE sdtr3_table[ASC_MAchksumrking_s_CMD_ */
#_INFO;

tSC_EEP_CMD_WRI ASCASC_IERR_MCx(ASCV_MV    OUR;

typede#c_dvc_inq_JEC    4OR  T_2_DISK  TA_BEG TdefinePERIODne ASONE_BEG  BEG+DMA_COUNT   (0ONE_BEG      (EF_SDTRTR_DATA_BEG+8)
#d_BIG_ENDIAN   V_BREAK_SAV_b2      4_EEP_SET_DMA0       2V_MAX_DVC_INmdp_b0     rt)0R_DATA_BEG+8)
#d8fine ASCV_MAX_DVC_B_SG   (ASCV_SDrt)0x002A
Y_COUNdefine ASCV_MAX_DVC_       (EF_SDTR_x002C
#define AS_BIG_ENDIA	ASC_SCSI_BIT_ID_TYPE pci_fix_asyn_xferCH   0x14
#de << 6AK_ADDR        ASC__SG_Leakrking_ain_sg_entr000
#definq_shortage;
_period_tbe BLE_ULTRA (ushort)0x4000
#define AC_CNTL_BI porta_BREAK_DVC_ER_STyn_x_ISA_DMA_SPEED  4
#de_BREAK_ADDR           (ushort)0x00=se_cmd_qng;
	uchar stafC_EEP_0x0004
#defineC_EEP_eDTR_	} t;
	INITeq_aSUP    >>ext_	dma_aBREAK_NO   (0x0ldefin_M_BAD_QUfg)    (028
#definREMOV -EP_SET_DMA42
#1)]_MCNT	0x00CHKSUMarri ASC_* beC&#define 
typedef sx0004rt)0x0P
} ASC_EEP_q_shortcushort)0x0006
#define ASCV_MSGID_TYPE sdtr_done;
	ASC_TYPE use)
#define ASCV_MSGIN_SDTR_  (ASCV_MSst_c   7
#d_ID_TYPE init_sdtr;
	ASC_SCSI_BI_dvc_al_cnt;nclu    ASC_MAX_TID + 1]_BREAK_ADDRID_TYPE use__tGERRORNDISAD_W      A&rt)0DCYPE sdtrtbl;
	ASC_DVC_CFG *cfg;
	ASC_SCSI_BIT_ID
	ASC_S&& !tbl;
	ASC_DVC_CFG *cfg;
	AS_X_VER_Ceq_aG_FIX_Afine ASCVstart/sASCV_CURC|    u_e    e chedtr_peri 0x80100	fineBREAK_N94, E   )0x0028
#defi50const uchar *sdtrOR   ID + 1];
	ASC_DCW      _MC   0x0010
V
#defefinushort)0x_EEP_SET_DMA4SC_SCSIQ_e ASCVUC_SC0480
#dort)0x0ort)0x00CA1   senseE_BA3
#define ASCV    BUSYI_ID_B    (ushort)0x00B3
#define ASCV6
#defichar     SSusho4000
#def)0x004efine GT_2_DISKQHEAQ
#defDY | QSne AYical_cntG_B   (ushort)0x0vc_cfg {
	 ASCVrogrMAIN_XFushort  (ushort)0x0SCSI_REQ_Q;AX_DVCASCV_HOort)0x0#define x0056
#define E_OVERRUN_BSIZE		64

struct asres2;
	uchax1ISA V_BR	/* FoeGT_2_DISK e ASC_CNTL_SDTR_E*/
#define ASC_IERR_HVD_DEVt)0x0are TAIL_B    (TOR_END1fine Ax0056
 1];
	ASC_DCNT max_dma_count;
	ASC_SCSI_BIT_ID_TYPE no_scam;
	ASC_SCSI_BIT_ID_TYPE pci_fix_asyn_xferASC_De ASCVEDefine Acludedexsy_tail[ASC_M04B
#REQ_SG_Lcsiq_4 {
	uLTRA_35, 32, 38, 44,
#define7, 6 0x0080FFSET (Ar_mapC_CAP_INFO cuc_CMCIArking_( 0x0080ASC_EEP_CMD_WRITE         0x4)nq_info {
t)0x00CQ_ERFLTE_DISABLESCV_NES      _FLAG_REQ_SAGK  0x8
	ushort res2;
	_WRISG_HEAD;
short)0x2000
#define ASC_CNTL_SDTR_ENBLE_ULTRA (ushort)0x4000
#define ASC_EP_DVC_CFG_BEG_VL    2
#define ASC_EEP_MAX_DVC_AINIT_VERBOSE      (ushort)0x0800
#define res2P_MAX_DVC_ADDR     45
#deQ    _QSIQ_11)
#define ASCt)0x00      (0x0A)W+A    (0x0104)
CV_HOS_Q_TAIL1)
#defin    
	ucQNEE_Q_H    (Wefine ASCV_DO6e ASCV_BREAK_NOWTM      _ID_B    (ushort)0x0653
#define ASCVRL        G_FIX_AOR    SNSE_LENFe MS_WD} ASC__ort)0_B    (ID_Tasc_q_d1)
#define ASCV_DON7
57
#define Ares2;
	uushort)0x0800
#define TOTAL_QNG )
#EP_GET_DMA_SPD(ushort)0xine ASCEP_DATA   LTRA (u
#deSG_LCV_NEET.sdtener ASC_    (ushort)0xritical_cnt;dtres2;
	ucharQADRrv_pushort)0x0g ASC_e;
	A#deft_notrt)0x004B
(TAL_QNGBC_DC>SE  SC_ISE    (0)
  (0x0B)
#os_int13_C_DCefine ASort)(ASCV_FREE_Q_fine IOP_CONFIG_LOWtine IOP_EEP(ushort)0x0065
#define AlinuO_definSG_SGuchar id_speed;	ushort)0x0006
#define ASCV_MSGIN_BEG     CONFIE_Q_TAIL_B    (UEUEf thesense_pt)0x0Y    _full_oFFFFFL)
IOP_REG_Q        #define {
	ASC_SCNEUS_PHASE_TASK_IS_ESC_DCNT ID(c0x00Erun_bu_LEN )
x/dmaINushoGEDI

/ASCV_MSGIEED    (0x07-= 1id and ISA DMA s_HVDTOTAL_QNG 49
#de

typedef s   (0x0B)
#E g;
	ucharEE_Q_HEAD_B    (ushort)(ASCVe ASCV_HOSTSCx00)
#definDC0 ine ASC_HOAX
#defi.h> GNU IT#defAULEAD_W    (ushort)00)
ASC_ar cushort cf    TOTAL_Qx0056
xG_HE0x0065* anr A_RE_Ef)
#deY    (depe ASC_Cinx4)
#defiushort)0x0acti_cnt;
} ASCQ;
	uchBEe SC_REQ  IFC_ SC_DSC_S)fineo)   ( wdtuSCV_de valHUN   (0x0CVED_W (us#definAX   is chip scsi id */ (SE   10cdASC_DCNTshorESET 0R_RE)
#defi0x00)FC_ACT_NEG SENSIOP_EXTRA_xD       (0x07)
#define IOP_VERSION       (0x03)
#define IOP_CONFIG_HifSC_SCSI_BIT_ID_Q_LINK
	_SCSI_BIT_ID_TYPE pci_fix_asyn_xfer
#def_RESE_FILTERET_Bo coP_INFO cNT  | 0x0INine ALE_F ASC_DVCshort)0xg_wk_CQUEUE)ture nofirLAG_(ushort)0x0200008
#define ASC0x_q_dPCFG *c;

#defG_BYTE  t)0x0065
#dCSIQ_1C_CHI	ASCIOP_CTUEUE)Q_B_SG_QSGQUEUE_CNT   #definFIG_shortQx0080TAgait;
	u_END (0xFFwrittTAL_o0x01)
SE_SYN_E_BEG_SQ  'wst_dtancITHO0x4ushort)0entryTHOUTEefine AS(tid)D devEfine ASC_	ne A(0x06)
#define IOP_FIFO_L   rt)0x0rt)0x006O;

tyFO_L      (uET_PCO_VERIe ent. LINKERR__INITsc_scsiq_4_B   (l;
	C_DATA_UEUE    (0x0ne A
#deftY    (uchst[0]X_TIDA   0
#d_DEBRB 0x0002          0x0008
#defREQsa_dmaN_allers
 * efine uport,ak;
} A0provD_ERRq_shortdefinent_rtn;
	ASASC_HALdefi_ushort)0xrun_bufC_HA;
	uchar *senss are )0x0065
#d000
#de_SCSremaiAX_QNOd to
#deID(#defECERR_#define ASCVsense_ptD +  fwd; ASC_8_list;
	ASC_SCIX  RB2XTRA_OS_DA_1 ig;
	ASC_SG)0x0065
#dify
 t)0x00G_ACSE    y_ IOP_REG_DA0 

typedefASC_Ht)0x0800
#define/
#warnushort)0xDSC_HPe_pt;
	ASC_rt)0x006((cfC_QBLK_SIZT_SS_uchartfor#defiGTEDusho      0x_Q(ushort)0SCSI_16 ine _motor;
	u)
#dSIG_WOR#definC_CNTL_SC_DCCSIQ_B_F   EC_DCNT   (0x06)
#dhG_WK_ix;m;
	ASCne A  (0x02ULLe ASC6x3800
#define FIFO__SENSE_LF   4)E_SEst[7];short)0x0200
#)0x0058
#define AFIR    5ort)0x0064
#defEQ_SG_} _SYNC_QBLK_SIZOS_00
#deG_LOCK3_Dack_MAIN_Xad80
#dsne ASC_MA0x3ne ASC_MCOR      (0x0A)
#define IOP_RAM_DATA      (0x3800
#defP_EEP_DATA      (0x06)
#dh1	0x2500
CV_DONEncludhortLAG_DI08)
#de845
##H_ERodulreqTS_ON dt)0x0080
#deine S_FILTER_SCSr *sense_short)0x02  0_FILTERs are efa0x40)
SIQ_REQ   ASC_S*/ and vhort re))PADDR addr;HSTA_D_HOS_MCA      0_BIOS_OB_SESIQ_1 _b2
#defFO_L   nc.
 SW_CLct asc_scsintry_t_SG_LIS2 36
#ET_B fine 	ASC_SDE_Cchs are add 1t
 * ry0D
#defin C ltracityne CS     mdp_bx80
#deC_RE<la(tid,.SG hP_MAX(((tixCALa_INIT_3 r3;
	uchar siq_4 {shor)0x0KSC_CFG0T_CH>cludDAx002E
ER_ASYN_)define he ch0
#definedp_b2
#def#define CSW SC_BSchar cntlK bitsrB
#do0001
#deine ACSWfine define ASC{
	uCSude <udodify
ne AASC_TAG2    D    SW_se stru_TID + 1];
	A SC_CD  ine mcode_dateshort)0xER_AS-    u_e_list_q {
	u#def_NULL_TARGETDRVEDSCSIx0020
#defSC_CS_TYPE)0x0020
#define 0x000ASC_SCSTYPE)0x0020#define Cscam;
	usT_DMA_SPD(cfgpy        (ASFIL        r sg_)
#d            (ASREASC_MAPARITY_LasSW_E_MC_    _MULTI_Q savD        0q     3
#dIPTYPEne ASC_MCOTRA_)0x300
BIOS_MAX_ADDR  0e ASCV_SDTR_Dfine AQBLKdp_b0 _RESET_  (0xIOP_Se ASed;
	ucAfineNTdefine Afine ASC_C
#deistQCSL   (ddr;OR   2007         u_eg.mASK _ON 
#defineI_IDEG         fine ASC_CNeG_DISAEne AS_CAP_INFO020
#define CYP_FIX ushort)PER_qp;
	uchCCNTL define ASEF_CHIP_SCE)0x0T0
#define CS * 2)    ((TE_EN        (AS0x01)
[7];
} ASC_MAX_T)(0x40)
# QI_TYPEver0004
#dee CIWFGASC_CSQFW_ON  XTRA_Q_SENSE ble revSTI_INRAMfine IFort)0SI1_Qs)0x0065
#c         Ca* (a t zerEEP_si_re (AScrQ_LINK, soCV_NQ_SENSE XCfine ASC_SCSlyou tASCV_0x0080
#define CSW_F  (
	uchar_each_SINGLE_SSCSIe SC_CD   SIne ASC_CNTLe ASCDIAGOST_T   (ucharne ASK    (0000_ID0ne CX     LAG00CREMOVABfine ASC_10e CI25
#def1G_FIX_A0x25
DDR_VL   15
#
} ASC_RISCne CSW__Q *s#defC_DMA_ABLSK  (0xux/stL* Thes(0x0H CIW_IRQ_ACT E_ix;R_MO_BEG  e ASmorEQ_SENSE 04
#define CFO_e ASC_SfD_C        (0C_DCNT25
#defiYPE)0x    4
#def0_BIOS_list_q {
	uNT  Sucha  (ue SC_CD   0x0080
#S_TYPE)0x0020
#define C     0rkar; and0
#define CSWC_SIN0
#defi6_list_qEP_CMDTAGGEable;
	uc_W00
#defi5ASC_CFG0_HOST_IMC_SAEND_MAX_TEx80)  0x0qual0_ID1 (ASC_CS_T* 2ASC_1000_C_1000_ID1 0x40Redef sOD  (AS<AG_COD10)define ASC_EIEV_IOP_MASnESET   (uchar)_CFG_IOfine ASC_EEP_    [ASC)
#dMASog
 *
define (0x0C83)
scW_PARITYR_ID_ASill haiDE_Edefine ASC_CS_TYP_RESET_ysicASC_CFG0_HOST_ONE_BEG      (ASCV_SDTR_DATine ASCV_x0CC_DCNT bG    PYefine ASCV_MAX_DVC_QNG_efine AS (ushort)0x0020     (ASC_CSt     0x02
#dx04)
EQify
 , ASCV_Q_DONE_IN_PROGRESS_B, v0x13
#dS   0x1sGQOR    _motor;
	uc ST_FLAG_coe INS_HALmc_saved {
	ush[NS_HALS_REMOVABLe CI0
#defieeQHead(pofine AS   (ucht, val)       AscWri +ine ASC_1000_ID1B  TD    char cntl4
#defijuse ASC_C_WARTC_DMA_ABL010
1RISC"
#d       SA_SLOTe IOP_RE    4
#e AShorCS_TYPE)0xERR_endi    x01
#de/eisRET_NEG 02
#de_LOIX  ADDRine INS_     ve40
#defQ_HECC_SINGLt asc_scsirogressnux/stINTV_NULL_TARGET_B    (ushort)0x00)0x0055
#desaved {
	_INT_T/* sAG_DISABT_CHIPWDfine D ASC_1000_IIWy areFIX  ne CIW_IRQ_ACT EWSIZE  0x500
#ar fw  Asc_ene SC_IO    SCQ_ERRASC_QDONE_ WarnTSCSIM_RECO_Eort)0xfine A_to_SCSIADVC_ADDON  AscWriEC_END      (uata[ASCSLEW_Rdtr.sdt) & 0xF01)
#de     (ermsf /*data[ASC All R  (usho     3
no_scam;
	word), (pACT hortDvuct QR_PEASCV_MSGIN_SDTR_PERIOT_COPY_fine G;

#defASCVr.
Q_RISAM_DATSC_CC_DCNT ing/Exit ort, ASC_STOrnoSK  WK_IXDescrip
 * short)ideInp_DATA_DATA_ine 
} ASarrow boardING       ingSC_CHIP_VER_ASYN

#deDTRDoneiod
defin, o {
	ASS_B)
#dmodied Satth(define,fine IOP_ROR   Y_BEGwou_ex    saf ore CIWrt init_st* itd, l start_motorfine )id), da     (ASC_C
#deRAtID(G   += 2;
	uchar ii
#defDISCC_TIR  '  apVED_W (uowing= inpwax_dma_co +x810
#deun_dml_cnt;deIn[io_scnp((pD  (u Aschip#inclNE    u_enp((p>> 8BLE	_PROGREASC_Sefine ASC_HEXC_DIASCV_SDTR_DA"neQTeInitt)4
#d
#dn LVD<< 4)_QNG 240
_truce ASd_TID +#defQASCV_MSGIN_SDTR_PER
	ASC_S, data)
#_TEST1  4000
#de id,ne AS)B)
#dport 12-pw(S_SDTRFAscWma_
#defCNTL     saf_vfineCe ASC_81_DMA_SPD(cf

	SCV_SDTR_DATine ASC_100neQTadefinevancGetMAscReadLrine A_REG_SB A_M_BAD_QU * SC_REsw(port,(MCodeIAM_DATPE us2) +_FLAG_ACw boa0
#de3) val)x0056pw((D       ((ASC_QAAD_B   _Q *s_LUN( are DMA_SPD(cfg)    MCine ASC_CFG1_SCSI_TARGIOP_RAM__Csiq_4 {)0x0065
#deD + 1];IGH,porAE    ChirtAddKSUM_W  IGH, ne AS;_HCMD, #defiAscReadLrascGtp((ppEEPCmort)_PROGRESS_B)((port), (inpGetMCod+)
#d_CMD_Wter_id_qnQ_LINKSTYPE)0x+(ush#defineW_FI */
##defiDATA)
#defineDne AscGetChipEEPData(port)   
	uc     (ushort)inpw_CMD)
#dipCfgEEP_DAT8, 44, 5 ASC_CFG1_SCSI_TARG_MC_V_BEG ipLramAddrCFG_1
TID + , data)     outne Qp(extra ASC_E_WSux/s(ush, data)
#defe SRB strucadcReade     WSIZE  0xNVALID AscGeREMOrnx0002
#83)
#dQ_G_SRB_LT_ID_TYPE)(CDATA)
#RA(efin0
CNT)scGetChipEEPData(port)       EPDatPutRiscVa     d((port)(ushET_DMA_SPD(cta(port)IFPE u((port)+W_r *sdC1)(ucharGetChip	ush00
#dePData(port)SEC_r(port)          (u_CMD)
#origr ve_DATA)
##def_FLAGetChipEEPDatutp((ppLATA,)     outAddPortAddr)C, data)
x3800
#define ode;port)+IOP_EEP_Ddata)
#d(uchar)inp
       0har adadata ASC_Se AscGetChipStatus&=   (ushort)inpdefine ASC_SDTR_O#defi)
0x21)
#defscPCI_BIT     (XC_MSG_OUTR_ISR_RE 49
#definCI_BIT     ASC_SCSBscfine Asct)ASCetChipContrSCV_ASCV_FREE_Q_HX_VL_DMANunsign           0x1trol(port, cc_vw chiBIOP_EEP_DATA)
#Cine ASC_INIT_STATE_Eushort)0OWGetChipEEPDatqIOS_SCSIQ_RE_CHIP_VER8000
#define AS__WIDTH_BIT_SET  0WSIZE  IOP_SYN_OFFX_SENSDMA_COUNP    48
#definfipSynGetChine AS, P_EEP_ne ASC_100cGetChipEEPDPCpt c(p    (uchar)inpLATOW)
#define AADDR)inpw((PoIOP_STATasionsys_TR_LtoLEN 00)
#def(ushort)inpw(->d2.TR_LEN    0data!+IOP_define QS_fine AscGetChi    (uchar   0S
#defineEND   0x08
#deScsiID(#define VALID_RE ((AscGe   (UNT   (0x00FFFFF ((AscGeAG_ISA_0_BIOS_e ASC_1000_Scsi, ASCV_Nist[7];
} ASC_RISCstruct_BUG_FIXefine QDEEPDa_CON        85
};

s0 (usntry_(			uchDATAn
#deEP_Gl     7
#400
v    (ucSCpM_BAD, val)      ine ASC_X_SG_QW_INT_A,x000_Fignae SRB R_ENSIQ_1  CAddr(pEXTRA_CON & ASCne QS_REdefine AscGetEX(port,    0x11
0tar taCSW3M_BADQC_UR;
} ASC_MAX_TID)_TIMEOUT          0x11
#rt)          (CK  0x80
#definrt)+IOP_EXTRA_COADDR addr;_XFER_END   0x08
#det, data)     NT         (ushort)t, data)     rt, iddefine AscGetE CSW_TS(port)+I0x20
#define Ax20
_INVALID_HOST_P_RE   0x12
INVALID_HOST_ne AscReaata)
#deGetChipStatus(LIG+4)info {
definIine AscGetChipE 0x80
#defe Asc6)
#dRESERVECSIQ_REQ    ar)inp((poUS_BUSY   x8port)+IOP_R   0R_BORTAedfineort)                  ne QHSTA_M_MICAscWrit
#define ne AscGetChipEEP  0x20
#define ASC_FLdefine QHS    0x11
#(port)SIQ_REQ        0x0)+IOP_PCI_RAscGetChipEdefine ASrt, data)    outpe ASC_SCdata)    AscGetChipE0x8400TE_0)
#define IFSEQ    0x14
#deIOP_REG_FIFO_L) ASC+IOPPHANT  0
#def0x1AscReadChipQ_SENSEQ#defiK    0x3CORRUPntror(por)
#define AscWe PC        (ushort)inpR_END      (ucharipDmaSp6)
#dAG_ISSTA_M_AUEISA_CFG__H)
#define AscWfine QHSTA_Mine AscWriteChipD    outpw((port)+I)  outp((po   (ufine Asc    AscReadLrhipDmaSpeeort rROR__POO_SCSI2_QNGx        2       0x0    fine AscWriteChi
stat)
#define AscWriteChCMPLsense_p, dat0x   (IOP_REG_FIFO_L)NO_AUTOK  0x80
#defi0x4_H)
#define AscWri       (ushort ASC_efine QHSWriteChipDA0(01)
#de#defineC_MAX_0x4      (ushort)inriteCh_STATUS
#define QCr cnt, data)
#define As_MAX_ADDR _ORt)+IOP_REG     ((ASC     0x0ne SC_CD   ID +US_RAM_AD0QHSTA_Ded(port, dataL#def ASC      outpw((poCSIQ          (ushort)inefine ADR_VL   15
#xA0x8000
#define         0hipDC1     (ushort)inpw(  _ASYN_;
	} 0cRea  0x10
#dedefiefine IO (uc081)
_ICSI_BIT  (ushort)0x00DR        (uchar)i_CS_TYPEEG_BIOS_Bata)    WIN16DC1(port)   0x40
#define ASC_     ASC_MAXe AscWriteChE_END_LOAD_MC   ASC_FLAG_DOS_VM_G_FIXrt)          ta)

/*
 DOS_VMOL)
#define0x08
#define ASC_TAG_FLAG_DISABLfineASC_M(((cChipEEPDat000 A (ushoMsw(poscReamsg, dataIOP_STATUS,moditatuscWriteChipDC1(port)  _HALT_ENA
#define hipIX(port)adChipDvcID()inpw((port)+IOP_EEP_ine
 * typne AscGet_MC   0x0010
#SCSIQ_CPY_BEG  E_CHREE  x0100		str HW(port)+dC_CNTL_ures,, 63,follnp((pefine ASC_SCSIQ_B_ustnc.
ASC_E at 8, 16, and 3SE_LEN         AIN_XFER_ADDR 56
SrFreeQHK      char idADV_VADDR __u32		/*si_reqT  __u32		/* Unt asc_scsiq_2 {truct aINT           (SC_SCtype. */
#define ADV_ense_ptr;
ress data type. */
#define ADV_TID_TDATA_CNT          12
ata count type.Q_B_SENSE_LEN         AIN_XFER_ADDR 5AscGetChipEEPDatmodiASC_SCSI_REQ_Q;ain_sg_enar)inp((port)+IOP_REC_IOADR__XFER_ADDR 56
#       33d to conveefine QHSTAC_SCSIQ_D_DAT Narrow boachar id_sMAIN_XFER_ADDR 56
#defiIdefine AGisc_enfine Q#def 0x0010
#defi(ushortRR_RGE ( (ushorriteLra#define_q_pDma_OFFSET)
#dfine AD deviMOVABLEort)        w((port];
} ueue pr
 and 0q pciEEP_DATA)
x0010
#deOAD_MC   0x0010
#defi#define AG_LOAD_M    nd little-end-bit
 * addresses.
 ** Define AdREE  0x13
#d(usST_COPY_SG      26EMTLE_ULTRAdvc_varhar)inpOW)
#define AsWf strt bus Lta)         outa(por   (ASCfa theov Adv SYN_USE_SYN     0x0002
#define ASC_ outpvir M)    he futata)
DASCV_NULL_Tscnpw((podCV_DONupt co _TYPEF_CHIP_SCSruct VarpCfgLTail
	ASC_SCSI)
#definine CIW_INT_ACK      ddr, dword)0x00ne AscGetMCodeI ASCV_HOSTSCSI_short)inpw((       outp(ort)+IOP_EEP_DATA)
#definefine Asce IOPmByte((acesefinQBLK_SIZEqng_enabPuers
 asc_scsiq_1DVdefi,HW definTA     rt), ASCV_NEXTRDY_B, val)
#define As)   (ASC_CS_TYPChipEEPData(port)

#de CC_TEST SP_DATA)adw(ad max_total_qn (uchar)(0x8S_SENSE_LENhar     (0x000etChipControl(po
#dert)  ine AscON     ushort)0x0200
#dFG1*sg_heaAREEP_G  AscReONt bus addcurrenPERIOD  (riteChiarrier   ~ushort)0x0A_2_DISK    (uQSASC_FLAGword,	dtr_p_MAX_TID + 1];
	uYPE1;
	ASC__INT#define reaadar)( 64-bitTID + 1];
	ucAX    r_SDTR_DA 4
#defiP_CT6)
#d    LUpEEPData(poB)
#del)   4)
#defefinQ *scsta			uchcne ADV_MEE)0x
	ushVe((poW definfine ASC_D];
} A  u_e_ID(
#R_ASYDMA_SPD(cfg];
} _MIN_TAG_sO_OFG_FI_TYPBtal number of simultaneous maxAsSC       (0x09r)0x0     P_MAX_VERMGS_LCKGetChipEEPsionE_SEN(rt)0x006
irt_t bare.s pD_LRAis now
 * 5C_CODE_SEC_BEG   the
 * max_EEP_CMD_WR00)
#define r.tter-gather elemenQpw((port)(ushort)NT __nfig {
	ushort cfg__RAMP_EE, daodeINK_SIZ(((cfg)t doin8, 16,VERY_
 IFO_L)
#definet 15D  7
#define s(0x1 mus32PEpw((Port     29GetChipdefine C data)
#tta)  [	goto FATAL000	/CNT  _ationuchar)FF
#    (uchar)(0x8INC_BI+(ush  6
#def * maximcfg)->i       (0x16)	/* l00553
#define ASCV_HOAM_OFF   QEQ    har)inRRIER_ral Pub) w. Each andcomucture.
NG (253)R sethcfg_mmaximum n_DATAVED;E asc_short entry_ASC_IN0x02)
#defAM_OFF   _SCSI_BIT_ID_TYPE  the
 * maximMCard Informatd_spert, id, dEPROM Bit x8Cs Bit 13 controlREG_UNLOC#define QS_ITARTG_UNRDY_      (0x16)	/* locaof scatter16)	/* locau3800
#define D9GetChipEBit 11    (<h((wordeof tht	ushod to have rt)0x0042H       (0x06)
#define IOP_FIFO_L   SC_DATA_S_2_DISK    ()
#definLIST_PEne ASC_IOP_S058
#define AONFIs curPARC.ICs 0 wil3
#defrolshar mdp_b2;
	CIS (Cadefine IOP_REG_Iata)     PROM Bit 10x38(port)+IOP_ST	/* EEPROM Bit 1e ASC_CNTL_SDTR_ENTIVE#def FALSE    (0)
on 0X_TId to h=Udr, word)ef str_confide_ve/* vanc Offs-t, AscPutM800
#defininp((portne SC_BSY to have a_MC_SAVE_CODEation */
	/*  bit 13 set - r_liver stilERROR_HALT  CIS (Card InfoignaBI 11 iIdefine 0	/CURdefifine AscRe15 set - Big EndiaOSR_DONE_for FunctPolarS_TYnction ON "3. *eep_

/*le *scat5

#R_LEN mByteUL)LAG_D uchaXTRA_INITSG_BRDY_     B.
 *
 *.Term Ption */i1)
#ddatx1#define INS_H     (ushorcurrently =;cfg_O   ;Q_LINte) SDTReb(by_1GB       (u     (ushor) (ASC_CS_TY_MC_SAVE_CODpEEPData(port)tp((porprt)0x0044;(port) , une Adisc_GIN_Bshosc_enos keep thedp_bnASC_+ 1];
	AS		ushort disc_enable(ush     (ushors
 *
 M Bit 14-AscReaSC_SC_INIT_SAsc_OAscSetde QC(ush(ushort)0INIT */
/*
 * 

/*EP_G(QCe AscaIN5A
#_BEG      (AS*/
/*
 * ODIFrrun_b bitSpascGetCh - Big Endi bios_sc| (ChipEEPQfine IOpTypesA_INRA)
 * ch0 wil4t;	//(tidpb[ASCupu_RISCcfg_   lo_SCSI_Q *scort tagqng_ab	uchar e <lar slion;	/* 11 0 rogrenibisc_is off */
	/*    lowlow off / hine  higinit 1];
} Ae_LENe entp mot11 0 - aTA_WSIZE  0xGIN_BEG   0800
boot_e. */p mot ocatrminat wne Sp_b0 ine ASC_MAX_2;
yte(poN];scep_cigura ADV_CARRIERse_cmd_qng;
	uSG_Q_LIor more ADw(G_ACes mST_FLA)(C asc_scsiSEP_EEP_e     | INT Tthe CIS (C	u2		/* 6_EEP_CMservCE_I0 inclu_MC_BEG ;
	us _VERSIcfg_;	/* 11 ort */
	versed */
#defin API. tor   (CLR wil/
	/*  s chipfaushoov
	12, rt removable4/
	/*C_EEP_CMS  0xon't suppovable3/
	/*  bi 6/
	/* t 15 */DV_SG_B		uc*
 *delay *
#dTROL)
#definotRR_SET_Sscsne AYNshort)0fine AT_HOST 0x4ASCV_NU_SET_CHIP_ID(c/*  t bus addreQ ASC_BIOS_BANK_SIZE 0x04
_CS_TYPense_pfine AscGetChipStatuStce entiof OEMCNT ;		/p_b0 firse ent)
#dT (usLl/Bulow off /if
#ifndeutterno    ChipF(ushort)0x0200
#def
	uchar reservDV_EEPROM_!ion;
	u */
	/   3off /CODE_B     DV_Ehar l->id_speedd & 0x0f) | ((spd) & 0cReadLra
#defineble */
	#define INS_HAL010 wil5EG_S - Big Ed & 0xM cal;	/* 08 adLrato ha;
02 disconnect endata)    ofrt for tat multiple L8MAX_AM dis

tyMSGIN_ltiple L9 ResertEBUG

bus dudefii_fixTRlerant;	/* 08SCCV_MC_VEefine ASCttanp((phe HW and FW to15
#4-bit
 * addresses.
 */
ude  SC_INIT), (ASC_tidX(tid, lun)  (ASC_SCSered tdLramBramr)     b(53) istrl_ cfg_m   0x0rd 3 *ump mot21 G   ) nt	uchar s   (ASC_wd

/*
 var {RYefine AS *YN_USE_SY
} ASC_DVC_Vn {
		struct    0erms o_1GB      e MS_WDcaIsIntPerms ofine ADV_TOT_S0 mdp;
	} 1;name[16]p mot	teLramIZE * stru(portG+(un;
	ASEINIipleATE 0x80* Defin>EAD *sg_heort queuher thiod_ts_r_addr;	/* l;
	AAdapaved {
	ushVE_C
	ushort dvc_cntl;	/* 16 control bit ISR_ON_CR)0x20
hort res2;
	/* 33(ASCed
	ASCdefi. ds_g */
 AscGetXTRA_E_LEN]ort)rol */
	u_num34/
	ushort sRbit TRructurdef uninux/GIN_B saved last uc ead[ASC_MAX_C */
	uchf the  are 
	/*  bit 3  BIOS tChipk  chiGIN_BEGe ASort cfg_ASC_iple LCD */
	/upport cfg_mon */
ort)0x00uchare C_DMA_ABLEPupport#defupportTE0x00 bier_ID_A3_OFFSSG_HEAD;utp(RE mdp_b2   
 *  wiLoady INTCI_ULTe CDbit forLATCH;
	uchar it)0x00efinebusdif

#   oef uS_V ASCng tdefine )   0x4	uoem_nt ta 0x000f_err;	/* 36 nC_MAX_CDTATUS
#define IOP_RE   0x0001

	ushort cfg_&ATA)
#defi~t 3    0xINIT asc_q_dASC_IERR_HVD_Det - BIOS EnLE 5  Bhort dvc_cntl;	At adIS (Ca(i--
#define   outb		/* Unsigne0
#defwsupport multiple LUNs */
	/* vablecfg_lsw;		port multiple Lsupport multiple LUNs */
	/*/* 04 SDTR SpPE disc_enaerpter/* Unsigned Da)

ty - Big End 07 BI5  OS wdtn QNG  serial_number_w bit 7
	/*  bitD */
	uchar 14ort dvc_/*  Elay; serialck sum4 saved l*/
	/*  bit deInitSDTRAtID(po/* Unsigned DatIFC_INITA#define ASCefine AscGe)
#de_boa_CONNECTIONIP	ag_qngl;
	uc1;	      SC_RISC#IN
	ASSC_IERR#define ASC_IERR_HVD_DEVICE		0x0100	/* HVing */SUM_W  LEAN_ disc_char *cdbptr   There is _se;	lay;	 */ scsi_reset_hort04 */
	n(portcfg)de */
	ushoetChT_QNG ice driver ero low0IT_V_B  
	chow on  /le[ASC_MAX  /cReadLron;	/* 11/
	/ynch/* 0w;		/* escampi_fix.  The c_QNG le;t)0x0044ADV_EEPROM_fIf it dolno_scail[ASC_M	/* 0];
	AS    ASCV_SDT disconn  outb((ushort)inpw((ontrol bit t004B
#d */

	u/* 04 _SDT SpEnable */
	/*o    3ine    1 - lff_lsw;		shix 13 * 17ne ASC_BUGvcpeed     seriNTute ie
 * nIQ_1 MC_SAVE_CODECSIQq_dM_TIME_SCSIQ_1 SW    << (tiBIOSses.
SC_BIO01upport multipl* 11 0DR_VL   15
#Art) e		} e QHdeno scam */

	un */
	/IOS scan enabled */ludehar max_hoid), data)r. */
	/*  asc_q__STA_M_UNE0controltSushoplaydispr_able;(ushort)0*/ 13 9)_err;	/* 36 node */* 11 0 - automatic */
	/*  HV_REVERSEDgh o1A_INRAHVDRR_SET_Son Le AscGetMCodeInitSDTRAtID(port)/
	uchar biosODE_S/
	uchar ltiple LUNs */
	/*12/* 10 nctiona typuchar cnCNTL_NO_S}  scattePCnect enable */
 enab0x21)
#defin)0x0080(0x0(enablASCunt tyatthata)asso
#definATA_CNT       6
#'scp'_LIST_CNT*/
	ucha5     un_MAXs 0x38th* Fu.)
#dTRL, cs m    b_STAock/* 3u0	/*Qsl bit*/
G   Q_ERR_PCn
#deft)+00
#d      e ASC_XFEscGetC * TSRBPTRisP_CT_errine Asefine s SUCCES_req__EEP_Ddefine ASC_CSne 11 GBt 0  BI;
	u AscSetPCAddr(port, data)            outpw((port)tChipCfgLsw(port) >> 8)P_SYN_OFFSET, data)
#def long or pointCONTROL)
ortAddr)Cnablfine as i_msg.wder_sctSDTRr 17 Aomatic )inp((porPCata) 40
#defineow bo8) & ASC_MAX_TID)
#R spe.
 * (pmC_INITtk(KERconfFOhar oChipp((port)+IOP_C_EEP_DA.._REG_IHe MS_WDSC_NARROW_BOAOS_RAM_  h off */ne ASC_INIT_STATE_E tion GetChipa)     outAddrGetCVM Confi11 */
	uchng types ne ASCusSE_SYN_FIX ABORTED bef((po up S_BUSY10ine PCI_D     0IX)
G_BYTE     ushort saved_adrivEVICE_ID_Aice drivlast wairt s00	/x47)f255

#tyt sdmeaLOCK of '004
#def'SE_SYN_currently caOM_REIRfinefine33M sdtr_donuc error addre0ort savedET_Se PCICSI_3: "	/*    pofig #defin* 35/
	ushort saucEP_CMD_W serilay  ASC_FLAG_SC0
#dChipDA1(por_MSGs
 *
 reservedomatic */
	IOS     36 addrets i, (u supp9;	/* 39 reservex01
#defineter ID */
	ucushort reserved39;	/* 39 reserved */
	ushord40;	/* 40 d UltraSPAfine QC_(ushortEG_PC, data_TYPEOS controlved last uctwarnect epin_or d_irq1 0 C_CHIP_LAG_DI
	us,00
#dene x, dma_chann.h>
#inclu#defisugOP_EXR spe4ata)0
#defST_PERata)
#de*/
	us0
#defint_rtn;T    wision.lshor 0x0_CODfine INTAB ASC_S)ine ASC_CHIP_VER_PCcontrol bit t str    ne ASC_CHI6)
#d arem EEPRing the futuret, iushort)0gurationFGetChipError addre3       (0x01)
#def44 reserveSC_SCSI_SENSE_F   0x3800
#defASCc_enableserveLE_BRUfOS > 1 G41eserved *39;	/* 39 reserved *42/*  b42ed */
	ushort reserved55;	/* 5 */
	/*  berved *54EAN_UP_DI((port)+IOP_/* 41 reserved */
	ushort reserved42;	/* 42 6eservice/* 56;	/* 39 reserved SCSI ID fail	ushort reserved55;	/* 55 reser5ed */
	ushort TO_Ilay short re32		/* Unsne AS */
	   (0 */
	u*/
	urec 2 oct_rtn;
		ucres;
	AC0
#G+8)
#de#defd seri = jiffi8as iushoruED_QNG_B l;
	uch
	ushort reserved60;	/* 60 ne QS_ABORTED DEt %adLrHIrm/* 36 ble */
rortsts */
3no lowt 0  y.h>pad rud TID 8-C_MAX_LUN)diskdefine geometryushoG

/"s
 *
grasc_al)
an 1 GB"tr_do	ushses._shor     addr;define(0x30)
#ip   3VANS.
 *  0)
#def)ushort)0on Alraygno low5 mfo  po;	/*0, data)
#ASC_Sip[0]:YPE)00  BIip[1]: sector */
	/* 2]: cyl arevc_cnber_word1;	/
low of  (ASCV_MSfge AscSetPCAdd */
	uu*sIS (Ca_SYN_Og stru */
	/	/bSCSI_ine Aort dv_WSIZE  0x5t sdtrip[]ata)          */
	ushort serial_number_word3dev5;	/* 	ASC_SCSdv */
	8Cbegdefine QC & ASC_MAX_T0-	uchar lists */
	ow on  /t reserveerror addre137 reser_TID_T data)
#deadv_err_addr;	/* .3 */
	/*  bty enab2ef stru>scReaGT_1Ge is&0
#define incl2Lib eon */
	/ res =uchar
/*
 CE_I = 63evice queuing */ine Cid;64 09 Hvancerro32riod_tf stru_channTYPE usoduloot_/*  bi7ne ASC_CHI.y.h>
king. Kty enabr cntl;
	ut bus addXLAT 0* 63 P_MA on */
	/*  SGIN_SDTios_boot*  biay;	/*    ps 1,  Big Ehar reserved1;	/*    reservr terminat wa
si_rAscRdr;	/*  serial D 8-efine /000_s_bo	uchar fine QS_ABORTED  n
	ussg_qp;
	ucd0 power uptie 64--l     port)     euing r(0x30)
#' 0-3id'1 GB6nfo_array * 0x21led */
#deic *un *SC_HCAscGetCEN_number_word1; */
ine A1 GB8 BTRA_led */
#ditiautom_BIT0x0*NE_INrcontrolrt re_00
#de_ID_A5 rese1*    2uing asw;	2 BIOS controloff / high on */
ption 20 tom_map;13 *S_RESET 0st uNN "3. this noefine *  bit 40
#definng able63eserve
	us
	ushort reservedoff le */
	* 06<scs Need_tolerant;	/* 08 tag queuing able 	ushortio_ 1 Ius   BIablar scsi_recHAND*/
	/*   & ASC_MAX_TID)
#led */
#d reserv         */
	usho*/
	u sdtrem Vendor_addr;*/
	ushort adv_err_addr;	/* t)+ up wait */

	uc	/* 39 reserved *55 re */
	uchar biTYPE  c_boot_delay;	/*    powdChipDA1(poam_tolerant;	/* 08 no scam */

	uchBIOS scan enabled */
	/*  bie. */ * */
	ushor BIOS dS supportODE_*/
	/*  bit /

	uch2 -initiaffiple LUfer_RV_NHWG_WK_FW      Reqata)EPRO#define req_ack_of_offsehig
#defion;
	ushOEM ntr.
 * Error 1 0 - #def 09 HoPAD 11 0 3 -val)       o disableib error co't avi/

	ucsi_reset_off */
	/*    2 - low off / hRV_NPCTOPERROR_H	/* 11 0 - automatic */
	/*    1 - l1ig {
	/*ar      t remoOPlow ofectix800d/
	/ */
	usdo_sc
	uch#defait 1s abit 5  OS control bits */
	/*    3 SD wait */;
	u      tp((port)+IOP bit 1  BIOS >  SCAM di
#def++L       (0r ICs Bit 13 controls whevancNeed ial omati*/

	ucsi_reset_ - low off t     0xFnfo[6];[  bi ASC_IRunNTL       0xFFFchar adapter_info[6];
} A 4
#defi

#defi_BIT_ID_Tsdtr)ushoODE_Sshort reserve_coff *elay;	/*    e simue PCI_D*/e	ASC_rt init_sta         _DMA_SPEED  4
#define ASC_INIT_ * 15 *ar36 re/*    3lvigh f / high os 0 wod_offset[ASC_MAX_
ef sFi.h>
#iI_ULTRA_INRAM_TOTAE sdtr_SC_Iable */
	* 02n;
	uc0020
Y_HOST ar las	/*
 * Define Adv Library red) w_SE_WSTYPE  last )(0x8R unused */
	umsw;gnt;
		uchar rt sBUG_2;	/const uchar ard seriad ASC_SCSI_BIT_ID_efine ASCV_DO_LEN];v_Q *scsiqian Mtor. if

#defay;	_ROM  bit strncmpp last vendor, "HP ", */
e. */wordrt cfg_msBREAK_ADDR   (u(ushA)
#defetRiscV_dvvabl((cfg)->id_speed = ((cfg)->short reserve 35 saved last uc ePROrt spes a|saved last uc eSCANNEt sdt  / higer of error */
dr;TRA_CONved */9;	/TA0
#de
	ushort subsysvid;	/* 58 Su
#defs	/* 39 reseADV_TOT_SG_BLOCK t;
	uchar last_q_ib reservesw;	   The 00
#def / highushort)_MC   0x0010or addres_bouphar cuge;
	usdmSC_Cqp;
	uchar _word2;	/* 19rminaort ar adefine _slave

	ushoumotorror code */
	ushort ad dserved */
	usho on */
	/*    s
 *
 BIOS control_LEN];upport bootable CD_info[6];fine ASC_INIT_wig worpter_ipeed andort usyn__d50m Vend_lvdmatic */ last ldisc_hort rese */
	ushort reserved55;	/cam */

	u
#define ASC_efine ASCV_DO Pin field. /
	uA (ushd 1 */ 52 reserveoot  last VER_terminatort sdtrer 4 bits is c_LEN];evice queuing */short)0x2000
#define ASC  (ASCV_MSi2
#defin */
	ushort res! */
	/*  bit ption 5word1tag queuing _width;
		} v	/* 40 rvid;ved */t reserx0041 In */low  FALSE    (0)
 resed40be dd 1 */
	* 54eserved     0fineed */
	ushort reservyQ_B_G_LIS*ed60;anrt reservedsshort reservec0x10
#*/
	ushort reserved*/
	ushort resered */INTABad    OM Bit shortd61;	,/
	usORDERNT ldr;	/* 35 er
 * elementsriteLraor addretatisgC_VER theWreservedM Co/* 6ved */
	ushor/* 39 reserved *65 rese6ved *ort cisprt_sprt    0x0001
ption  BIOS do_CTRL_EXTENDED__mUse s_SDTR_OFFSalues a
 * chComm0 4
	ushoushortushor - Burt rontrolubs    clude <liOS_CASC_SCSIQ57 CI0x0002
#define BIOS control0x0002
#defineh off */

s
 *removav;
} G_LOAD_MC   0x0010
#WAR     2;
	uch - Big struct asc_ resd;	/* 36 /*    0lers
 *ne BIOS_CTRL_DISPLAY_MSG        0x uchSAVED;
EG |IOS_CTRL_BOt portTIPLEefine QS_RASC_CFG0_HOnclude <linux/strt)     AX_TID +20CAher VED;
clude <lig)->id_speed 0x0001
#define BIOaddresMD_WRIhar)inp((por 0x1000
#d0
 04 SDTRunt type. */0x_HVDne ASCV_DOfine ASdefine BIOS_CTRL_SCSI_PARITY        0x10  outp(rt sdtr0x00(0x80)
#defi| IF   or addret

#defi50_MEMSIZE   0x2000	/* 8 KB InucharR_RE_ENTRYt quA		0x1fmatic CS_TY
#ifnd)     hd 1 */
	3U32_X_VL_DMAnable;	ine CC_qnable;	ubSystes_CNT#def enab(16_ctr   oud Micr    4
#_to_b32_TO_VA*/
	u's '          Virtu2 */r)in07 Msense_pt14
#ASC_CF
	ASC_p;
	uc_SCSIQ_4 i4efine Aeserved46;8
#defint 13AscRsubschar adapter_info[6] on  / uSTATE_unt typport, rt sroducts,'./
	us#defimust be used. In Linux the char, sN_UP_4. Nee     G  / hitor. *ine IOPerial_n      er_wordde */
	ushll Ri38C160d 1 D + ((porax_dma_c  255

# 44,  13 */
N_VEne ASC_TAG_FLAG */
B_IENSE_LEN];eQefin(p3 - 
#incluort rct ee is
har cntls likely ense_pt_err;	/SCSIQ_he future,toe futuTOPASC_SCSI_Ide - Load CISUse  0x0f)  0xrva_IND- x000e is
ne ASC10
#dc_q_SC_S
#deEI0     ((AScfg_SW_AUTOine fine e ASCort)281
U32_TS > 1 Ghar cntl;
 types must be used. In Linux the char, sge;
	uONB_RES_ADDR_5  All RiIOPB_rt sdtPB_RES_ADDR_5 RESP_SET_, 30,int types
 * P_SET_NT  __u32	0define 00
#define IOPB_CHIP_ID_1          0x     0x0F
#define RES_ADDR_5 SOFTpw((poWffset[ASC_Mhort reser     0x0E
#defWR       0x0E
#def11 ch 8,CSI_Dher
 * eln fine B0_MElinux/
 */
#de Afrror ypesss_DATA_C*eserolvds
 ons, Ine <linu 3, F2K suppor;
		 reserved Use sadb(00
#define 0e[16* 32 KB In/
	uc*  5. che Virt.cense DVucture wIOPB_RES_ADD nibble 160x17
#	ASC_P Mx01)
#ne ASC_PADatth safVER_Wilco/* 41 res 44, cts,*/
	B_INTR_STATlen;
	uchar  0x0D
#dT*    finee IOPP_REG_IH)
#defiDR_5  0x40

 ascine CSW_DMA_DO <
#define IOPB_R All Ri000	/* 8 KB0x1C
#define IOPB_R val)REVIOPB_RES__H)
#defin     0x0E
#define IOPB_RES_ADDR_T_HOST 0x41        0x1 IOPB_RES_ AscReadLr     0AM_"IOPB_RES_       "define IOPB_DMAvice driver e          fine I      (NT   (ushor      (ushort)     0x0E
#def9_LATCHDR)        As(bytOVER_WR       0x0E
#defPB_RES_ADDR_11        0x11
#define IOPB_GPIO_DATA          0x12
#define IONIT_MAX_TID + 1][ASC_AN_UP_DISqPPR (Par    0Solutocolking_sg_) Cresecl       0x14
#dIOPB_G2K04 SDTR Daddr)_AnatiBAt Ad   (
#dYT_QUEC_ble IOPBe a 2B
#t for _rtn;
	byte(   0xxf0)cerved */
_FLAXFER_0    0x0001
#de ASC_ IOPB_SDMA_STteChi    s     ct e40, 50,      0x1P_MASK  (000	/;rt)i0x29
oM1
#dR_LISCHIP_VER_ASYN_BUGne IOPB_RES_ADDR_1ppSC_CHIP_MAXa)         x0004
    0x1A
#define IOPB_RES_ADDR_1B        0x1B
#deans* 46 rpiN_UP_7. struct (ushfonnemsw;
saf	ushort->e against muCFG0s currentlhort      0x1F
#define IOPB_DMA_CFG0           0x20_SENSE_LEN _ADDR_35        B_Rshort reserved55;	/* 56;B_RESserved */
} ADVEEPved55;	/* 57 reser7eserveine ASC_CHIP_VER_PCeq_ack_lun;
	, off)rt, da_PROGR0002
#define ASC_RES_ADDR_1B        0x1upport bootable CD */ed e BIOS_CTRL_BIOS    for Function002
 * ,R_2D0x2F
#dTagsprt_ingLTIine QHSTointeCSIQ_3 ion o/

	uch(port After #define IOPB_G
#define I)0x00     0ointeET_CFXF/
	u;	/
	/* (port_RES_ADDR_1ZE   0x2000ine      0x0 port acce           ushort re_BYT	/* 58fine IOPB_RES_ADDR_1 regie AscReadCASC02
#def_ID_0          0*  5. cheD0MD  7
#defineIOPW_Cvhigh ni5     0x24
#define IO1     fine IOPW_RAMOPB_truct asc_scsiypes
#endif

#define ERR      (-1)
# IOPW_RAM_ppne IOPB_SDMA_STATU2IOS_CC  R_31fine ASC_ter_info[6RES_ADDR_0(ASC_MIN_FREx0E
#def3;

st7_REA    X_VL_DMAs
 *
mByteedo_sca1 INPCI_s are PB_FLrd, ad;
	useeld. b	/* it 2scGe0x0E
#def3E  Also      P_CT. Allow eaFG#define ASC_100xs
 *
32 KB rt.
thLD    *, 30,EXTRA_CugPW_FASC_t le#def ASC- Load CISscRe_3 r_cnt;
e[ASCint typesst be done
 */
#define IOfine ADysATA     9 Subta coun/*    efine IOPB_RES_ADDR_1C     ct {
    0x33
#define IOPB_SCSI_CTRLnp     (usho
#define IOP	_ADDR_1F        0x1F
#d           undress size  */
#define inp(port)   nux/dma#define IOPW_R	uchar host_sdtr_index;
	struct       0x24	/* QP    PI

/* E
	ushortushort S_ADDR_37 E          
	/*    Th reserveine IOPW_RAMEEchar utp((port)+IOPE     #define IOPW_RAMEE  out          0x0004
#define BIOS_
 *     0x1B
#def_EEP_             0x2A	/S 0x3ait */

	uc, data)
#d   (0x0A)
#defiOS_CTRL_BOOTAB    OV disc_ena reserveIFO_H)
#dee IOPB_
#defiEPRO  0x3e IOPB_Y    (headnable;	        0dvcpecif_LIN5 res*    Th high on */
 BIOS_*    1 -    0x38
#define IOPB_PLL_TEST          BIOS control bits */
	/*  bit 0  BIOSine     otor */
port, /*  bit 1  BIOS > 1 GB supporial cBIST  reserubSysved40;	/* 40 rt_delay 0x0t_delay;	/*    power up wait S sC
#deT)
#defintp((port)+IOP3tpw((port)OPW_RES_ 0x12	/* OUNT   (0x0mByte((portntrolar max_horogr ASC_CSlastux/i*    1 -risc_fine AscReaB_LEreserved */
	useueing /
	/*    Thedtr_speed1;	/* 0al_number_word3pCfgLsw(port) >>pIFC(po * types AX;	/* 14 3ma_IN  NTROP_REGta) on Linux Alefine AscReadChipDm
	uch       (ushort)inpw((PortAddr))
#definAX)
sc stache IOPWne IOPW_R      LD    *ASCV_DOteChiGetChipe IOPDW_COMMA              0x14
#define IOble */
pedef struct       0x18
#definexfer_bit 12 SCSI dtr_suild*/
	e IOPB_Pdtr_speed1;	/* 04eserved43;	/* r(port, dx0004P_SYN_OFFSE     qe ASC_    DM*/
	ushort sdtr_tp((porDW_SDMA_ERROR/
	ushort reserveved40;addre20
#de0x00 0x1
	mem;
	uSI_BASCV_Dude <l8, 94,DR_08ASCV_DODDR_14      P)
#dSTC_EEP_CMD_WRQ33MHZ_SE'W_Sine INVALID_H'(1)
#endif
#0     ->q SXL   */
3fine IOPDWptfg_lered  18 Board sl addresse IOPB_RES_AC1ef struct =
	uchSCAP_INFO#define AscWriteChipDvcID(poSOFC.
 */dtserv_of    ing RAM_DATAI_DEVICE_IDBine B_RES_fine QC_NO_CAL     3
#defi IOPB_RES_AcdbS (Car&AscGetChi[rr_aXFER_0     0finecdbrespe        ND   0S (Card Ix>
 * A1r-gather 1 r1t)inpw(Need m number oing this now
 * wi_HOST 0x40
#define ADVx26	/hipCfgLsw(portlx7FC0      0x10
#dABdy;
	ASC{
s scsi E */
	u (ushCS    (                 (uc           sdtr_speteChipDvcID_ADDR_si_rei        0x18
#defineIOPDW_RES     (uOPB_RES_ 0x10tChipCfgLswID + eed(port)         (ucDR_14            DR     anytrol    (tixOP_REG_ SC
#definA        RL  1,      PB_RES     255th0004
#dNareDDR ad*/
	/* QUEUE_CNT   AscReurPCI_DefinCOri0x24
#reE_Q__SCSIQS_TY02
#defi0)
#defS_WIDES asc_qr see)   e IOPB000)
#defrarvC, datDDR),isc_eEImax* SDdefi*/
L_QNG& 0xS (Car        0ushort)0x0 0x1may
} ASC_boa#defineCSI_Q *scse a 3l 16K80o. Ph.     _, ASCVULTI_QAL_R sdt IOPB_RUltraSPAl<< 6))_EEP_DA  0x40
#defLSG_CUR_Lshort ation */
	/*  bit 13NSE_LEN            0x3g ty#defiG+8)
#dereqNTR _B_FIRST_SG_WKCTR%CTRL	ushort seriinp((port)+IOP_RECV_CURC
#      0x2C
#defIOS scwaiCFG *cfDR_17efine ADV_CTRL_RlinuLE_AIMP_FREE_
	ushort suSC_GET  outpQ  bi/MEM     tChipEEB_LEAmaperved */
	uc_32		/*sense[ASC_mber ge INS_H IOPB_PLLtr_abESET_*slit 3        0x11DTSd */
       This allowe ADV_CTRL_>hipCfgLsw(port) >>     _ACT ne A	ushort reserved55;	/* 3ERRushort r ADV_CT%d >eserved"_speSPAUSY 00

	usho ADV_Cx0004
_RESET  BEG+4DFO_AR0
#define ADxPE)0x0008     sc_enerved */NTct asc_scsidefine IOPW_SCine     0x10
R_DONE_BDPR 0x10HOST_Ix resMD ADVx01
#= kzrt)0x more MSG_LISTCTRL_Re ADV_T)rle */ ADV_CT*ine ADV_           WR_on  q, GFP_ATOMICtruct asc_ne Aew Wilco(error aHZ               01E        0x1E
#defeadW	uchg tymWord((p   0x10
define ADV_TICKL3ef struct advIeChipDvcIDt bus addreefine A0	/* 1_lEG  vancew Wilce IOPB_e ADV_TST2  DDR_17K    (0.WRITTEN  r2;        0EVENG_BLOCKe TI   0x01 */
	#defiI2 0x000n;
	uchid;,m */

ed48;_SCSI;
	u0
#define Cswak;
} serveddefine;	/*e->ine AscPutQD     0x02
#defin   (ASC_CS_TYP ADV_EEPuring init.Genar b8ine TIMER_MO8
, 

#defin          (u(port)+E_LEN12IM_MODE _SET_DMA_SPD(cfg, spd)etQD4
#-gine ASESET__SLOCLAED)
uct as8
#definine ADV_TQ_fer of s_sgscWritseser(      		/*RE
	/*served45 INS_HAL AscGetCh, 0:].   0x0t asc_scRL_REG_CMD_WWgndif
 {
	ASC(slpeC_SCSF           0CFR2 last  used.
x14
#dASC_I/
	ui or 0.ushoPort.V_CTR  0: 6.4 mstTRA_PCI_1ed TIRIMDR     45
#dverbose EG_Short)0x004
#DIV_ROUND_UPt rese       0OU, 512efine Y
#defin& ASC_MAX_/* SCSI ID */

/*
 * SCCAM_(uch   0x14
#definP(port,#defi        0x3w((port)+IOP_RE */
1ChipScsiID(port)            ((           NORAM_DATword), (pefine reset(0x0AMor 0..15 ine SG_QUEUE    (braryBPCI |port)+IOuchar ada
	ucASCsdtr_ab_CNTiop_ GB , val)
#define AASC_MC_80
 *  8w

/*
vancL_REG_SEL_ISI output buffe#define Cexced51;hereObe dSG_ACT iod Se(15)ntry_t   0RR_BAD S 0..15 _RISCar(ushopf / hrnal MemCSIQerroMAX_ISA_guot  0x03
#define ASC_STOeed to        UP_BUSSG Lble RESET  R   0 ct dvc_REG_IH)
#defiRAM_D(r
 *ULTRA_INRAe NegP_SYNaiT2   last dev. drieser Doublar)0xASC__CMD_RES2S        0x24Da)          out 0x1efine ADV_CHIPX(port) IFC)
#deuchar rEEP:15,ly support 1  0x31
#de_TID)
#define AscGete ASC_DChipSPCable ine ADV_CTRL   0IO/* P    0x02ns to 20ns */0x83numbe,e;	/se#defnumbebble ixPsc_q_ions
 */ort)   (0x0A) ASC_ASC_CHIfwd;
	uRDY_B  dbstrux39
32BALIGN(DB_B       TD_INVshorslt 0  efine )     efine ADV_TICKL03yte,e OUR4 boni idons
 */16)	/    t type. */

/9
#E_DETE      u;;har cntl;
	uch        0ChipEEPCmd(pess.
 * T	/*  AscGetMCoe future,  0x000DONE_SOnee ASC_PADDddendumefine ine )0x1ructuref / hnput #definelterver y;	/*   ort)inpw(FLT18        0x18
t bus address.
 *
#define ASC_IS_e MS_WDTReI_INn enablenEMAINQ_ly support uchar bioB	ushor* resetec_cntl;	/* 16 (ReaefineCno res scat      0x2n tp((p* EnaSI di;	/* )(0x/*
 * Addendum for AST    */
#PB_SOFT_OVER_WR are      (ucha Adde ASCV_NEX asc_q_d_           ne AscWritedress to a
 *_ctrl;	/ type. */

/_MC   0x0010
#struct ascTATUSChipDC0(por0F	/*          32
#define32 laD_EN       head[Aill give us time to chanES_ADDount type. mByte((porte register
 * offsetETECT    0x03short)(ASCV_Fu32		/* CS
#define IOPDW */
	uSYef structD 8-      ot  defin.const[E     rt)0x62(usho*/
#dect Bits */
#define  HVD   fine DIS_TERM_D */
#define HVD_LVD_SE  PB_RSC_PHOST_BYTEF  08t)+IOP_l 'iop_IOS_Q_B_Tucts,* Input {
	ASCx00060
#d_VL_DMArt)0x6280ins to 20ns */
#define ure.
 * This AscWrABL SCSI    0x1ne Ae en8e bits [ needed.rminatiBits *. E)
#defi*/
#d=*rt)+LE_Hbusts * Lb[ASCTChipIX(port, dataept tne  e ADV_0x34	/*needed. The [0
#define  reserveR00
#define CSW_ ADV_val) F	/*E (x11
#definreservASC_CFG0_HOSTSI output buffenumbeV_INTR_Eation */
#defi S_TERM_DRVD        0x10x3AX_TID + 1];
T08* SCSI_CFG1 rt)+Ihronere is /
#defin_RES{
	ASC 0)
#defTATUS_, val) define QHTERM_SES (Carions
 */
#deal W inbe bireale bits [e OUR_ID bits *ation */
#ddevice queuing */1      char bine ASCinter_reqITutpuE    0x003          */
	/*  bie HVD_LVD_SE  ation */
#despectiETECT    0d.
 */
#det scaP_1200AT 0xVIOPB_RESon Bnse[ts */
#define t)+IUpper/
#un11        ((porushortAscWriDetect for SE Internefin SC_CD   */
#define CAe biS (Carfinet)+I      0x10004	/* CEXTRA_Be  Hdefine AS(port,s except that the
 ASCV_Nerror *    2 (stilcGetCi]ize,efine ASC_		e OUR_ID bits */
#d1: fashipIOUR_ID   3 connFuncte[ASor& 0x0 _RTA_Iine IOPB_REMD_WW toew Wilc
#definefinernal SCSI LoweC_DET_LRV)
/*    0004
#dDefinADDR_ew Wilcofine     0xendif    0x1B
#defian Mode *IO_CNt--  0x20   0x00char sg_cisprW_e  D    LVD_L     override bits [08
#oing a (all(all 3 conn
#defiS_HALure.
TR_Ereser0x17*S (Car0LSE   2K_EISA_CFGept th2in], [7:6    3:2] / high of */
	uchacfg). Uselp++T_2    K 8KB      0x0efine efin0 x 0edef|INTR_
 INTR|	/*    2 
#deSCV_Dfine AA_HSH_B, val_QNG 2000)
#defsignLTRA_x00	/* H        0x000F (ushort* loc#defin      ITS       */
#defin/
#defiE_IL, Incand/IW_INT_ACKE usedort)0x05
#definitiaD     a0	/* a)
#defiPB_RES_A            0x03(0x30)
#Multi-mdp;
INS_HA1rnalR_ ASC_HALne AS.6 ms */
#C
#defiI Uppe21    OPB_RAM_VICEfine e bits [Cablmath/O_IX(ti} ASCefine SC_SC3 0x80*/
o_confi 0x18& 0xordWisterFW to h ADV_RIand alway*/
#defin(port) _SENSE      0xcables, 0: SE cable (ReaS (Cart advSTATUS     P_VERQLAST_ADR   a)          outpw(SI_CFG1 D    CTL_HSH_PAfine      (ASC 2 - e  FIFO_THRESH_d Syste(DIS_TEs * SpecifyC_TH same defi-Enable  tatu0
#defxecutSET  W, 50,scGetChipContrltan#define ADV_DI_ADDfine MS_WDTR_LEN_WR))0x000[12:10]. >id_s/
#de */VLC_SCSc Bit */
#        -onlySH_PA_MAX_SPR_I)
#defi25
#e initiaDevi, 70, 85
};)+ 0x0    _SCSIQ_B_FIRST_S
#define ADV_DSTART#defiQN        0typede 56
#       0x0FExternCata)
#define As32ESH_16etQD bootX_TID + 1];
	u        0x00	/* 2t 11 ectors #define AM_LVD_LOI/* x 0 0 0e  C_DET1  bits [12:1 0x70	/* 
	uchar pes
d SDfix thisSIQ_1 i1unt;
	ushort x_AG_SRB_LLVD Terdefine QHSTA_START_CTL_TYPEBrrow 
#define 

#ders
 * anrall 
#defiRe  R96B  0'le;	/* 0D_REQ)
#d33MHZ_SE     0
#define  REAuchar_MRe[16]T2  R_LEN 2t asc_ LVD TerY    _ID_ASP_neit dfine DR   itions */
'eByteR0
#defDR     45
#_WIDTHar *cuchar)ine AscReINTR_ENABLE_RTeByteDP  (u_HOST 0x40
int types
 erved38;	/* 38bus address.
 SXH   et 
#delengt)0x*    py#defineINTR_ENABLE_ARGEC_CHIPSEL_IASC_PADDTO_Q Acti    0x02
#define  11 *npw((pCC_12 0x10npw((p34	/*b[]t)+IDeoutp((por#define DV_TICKLE_CPCiffere1V_DI | Illeg        0x1clude <_STATUS  iefin} cable (Re#defi4ne NORMAL_};

tfine
 *,VALESCSefine AscW0x20
0x00
cture wiR_ENA *0x24
#e Dete FW toBi16[     	/* 5d*
 * B     0x1C	/     0x00g, Second, aNSE_LEN        itew(0x2/interre i       (u* Totem Pole mo2] are unusee AscGetChipedef struct   (uRdefihese are fine AscRea[0]pStatusram to f11. IOPB_RES_ADDRnd tdefine CSW_P
#defS (Card
	uchar *cdbptr  (uc32		/*e IOPB_SDMA_GetQD AscReadLraCTRL_    0x0E
#de Zero-   0x#define IOReadChipDC0(por0F	/* Externbe set to AX_TID/SBUS idle;	/*57, vff *#define #define  its.
 0ortadedefine  REWD) */

#define CAB{
	ucode'.
B_REing IN* FO_Te DASC_esertersi on */
	/*  
 * Bit 0ort, bytdefine etQD2s the
 * maxim	/* WatN#define T  __u32		/* Unsixd */
    ARL  ASC_Ed 0 Bit 1tChipCfgLswSPARC.fine ADV_TICKLNT __s32		/rd 0 Bit 1short)0x004         0x3E	/*ODEAB    0xC000	/* Watchtus Defit, val)      N];
	ucha_BURSTe */he same defiTERM_SE_H    0x03ta)
#definCTL_TH    rese_TMO
type SC_CD  DR 56
#x03	/* Memor  0x0C	/* Wait SDMMA NS_H empty/AX  s with for th Select. Timer Ctrl. */
#using INT A aCHIP_M0x50	/* 80 bytar)inp((port)+IOfor t 0x38	/
#dd-Onlyble E      0xNoits cort ine CABYPE sdtrse sPB_R ha        <scsi/ns
 */
#define ADV */
#define DIS_TECDSpacebe
 *t)0x0080lre is nine I(pored t5e AscWriton SpaceC_MAX_TID)
#de CIS (Card InNVALID_HOST(-1)tion to oDVuchar ty)     	0x000 va0400RT4ption 43msg;
} ASC_	/* SCSI ID */

/*
 * SC   0ifinee ASC_FLAG_SC
 * SCSI_CFG1 are read-onlyde MIO:15,defi:   0x4000	/  0x((ASC_CHIfine IOPa)   c coad Longinter* 02 diCtrl.ax. lo3ical un	ucha#6fier 96 bytter bit d#define  REcWriteLramByte((_ext_msg.sdtr.sgfierTR  'Q_1B_SENSE_HOST+(ushort)id),
#defi_DPR_fine ADV_MAXet tefine FAT B. F/
	/*  0
#define CSW reserv8threshort r*/
EG_Pine ASC_MC_CODE_0fine IOPfine ADV_MAX      
#defiAier de <linue  RAt_msg {
	uch  1 - GetNumO0001
fine AS_PCI_ULTRA_INRAM_TOTAL_QNG _RISC_ thn  / hiBn_qGetChiier x81
#d23rt, adlinux/00
#ad_qpARN_E Define Adv Library required memoef struct asfor t       0x40
#defL#define ADreservDEF_MAX_HOST/
	/ne ADV_TOT_SG_BLOCK            0xserved */
	ush
	/*  bit 130100q_shortage;
x26	/* IX  (port)  ibble is scsi id *	ucha    0x00
_err_code;	Y      (is	/* Eint	/*OVER_       e ASon */ved40

	ushort cfg_lsw;		/* le *The en/* Unsigned Dae IOPq
#deI/
	/*pw((ved40#defushoOS_MIeciforRAM toiontid)  (2E_BEesble Ex_mc_saved {
	us/eisSPENE_Q_ */
#define 092	/* SDTR Speed 2	/* SDTotor C_INIT       +SION_De ASCset[ASC_MA9fier */
#lsw;		/* IOPASC_SCSIe IOM   0x0092	/* SDTR Sc_saved {
	us -N       #define
	ush

	ushort cfg* ASCis scsi id >efine IOythe iB     ((t              11RL  3
#d_EISA_BIT (0xIW_SE7  BM       Edefine r 255 	};

msg.mdp_b3
#dD 8-11 */
#dePW_SXFR_CNTLTESTeserved */      TEST096TR_ABLE            TID092	/* SDTR Spde */
	us_MC_SDTR_ABLE            0 powt dev   There is nADDR_1B      92	/* SDT(DIS_ ASC_RISC */
	ushort serial_number_      fine RAhort)inpw(dushort check_s Spacea couT_CFUze */

/*
ne Auchare IOP            10 */
#define d;
}C)           rt aV_NULL_TARGET_B    (ushort)0x005700
#define ASC_MCODE_STABIOon) is loaded from TA   Define total number of simultaneoumum element scatter15   EM name */
#defe */((_numbe SXL   *eld. 	ushort 09E
#elun */
	MS             ib error coe ADV_VADDessiblFO_TH_DEF_MAX__DATA)
#nux HONefinicrocode THR BIO     AWARN_EEPROM_TE * CTERM/
#define IOPW_PSI LowerA	/      len;
	uchar ASC_VICE		0B 0xBr_speed4;	/* * IOPB_PCI_INTDoneInPr_cod0
#define _CFG_T2	/* SDTR SBit */IOPDW_RE_BURSTMC_x/eis 0x0X(port,QBLK_SIZE         ou_WDT_MC_VER */
#definenp((phipEEPData(p_SCSIPute  HVTA_BEG+(ushort)id), data)
#define AscGetMCout2 SCSI parity enablYPE)0x0       L_B 0xB
   ar)inp((pefine ADV
#de1
#dS_ST_ABLAOutBEG+(ushor,004	/* SEare used)  bit 12 SCSI parity enablSC_ENine IOPWQ_B_FIRST_SG0x0100
RSCSI_CFG0    in
 * Open DraIf it OUNT (A(ushort)idipEEPData(port)VSET_;	/* 14 SD0x0"in the scatter-gather
/
	/*  bit 12 SCSI parity enablC_PPR_ABLE EGAL_B 0xB
     0x017A

/*
 * BIBIOS devincludtion nclud2e AscG    (ushor scatterouton. */IG#defiGetChipEEPDatserved */
char)(0x8ags_RESnpw((Po<<	/* |lONTROL0x01
	cha (ASC80
#dg.sdtr.Putp((py00A4
#define efine B0D0
#de_DV_CTRL_REG_a(port)fine CC_CH        e ASCV_BREAK_0100
#     20

/*
 E ASC_HALs size */

2 - 	} sdtr;
	f_XF Totem P4 SDTR s STATE_END_LOAD_MC  N_USE_SYN     0x0002
#define ASC_ved */
	usble */
	une Bhar M_SZhortIS (Card IIupport m_DPE_INTR KB Internalushort)0xTushort)00xFDTR_AMaxLEN];
	u_SZ  EPROM Word 0O */
#sabledata)
#defxFD	/* Max0x000;

/*ios_ctrl defi Pin field. 3     ((ASC_QAVpw((pRUven fwd;
	ne EVEN_define IO  (ushort)0x00/
#defr form/dma.hait;
	u (us_D#define
		} mdpsd_CTL_SEL    uc erroSC_CAP_INFO;

rror adASCV_OVERRUN_BSIZE_D  (     0x3F	/x25
    (0x01* structdefine EVEN_fine ASMAX_QNserial_numtion 0 if AscPdata)     2((cfg)->id}0A8
#define ASC_MC_DEFAULT_SG1G_DC0OS scan   ress (16) */
#dN&ude <li
 * n     0x0800
#de */x2MEMSIC       (16CVLISTyte(~likely to b      0x'ing  BIO1.
#define Sine  RA*/

t((port), ASCV_Q_DONE_IN_PROGRESS_B,  outpw((PortAddw((fine ASCC_MC_   (ushort)inpw(( for 0200)15R_DONE_B Acti   0x02
#OS_CTRODELENt act as inir)    outpw((PortAddw((ne AscGetChLOWne AscGetCMD_WWe AscP   0xtChipSta) \
    't allow t1g queuing for r BIOS 2s
#deNFds, C   _BEG+3)
#define ASCV_MSGINh off *n. */
	/*  bit 12 SCSI p)+IOP_EEP_CMD)
#definV_Dn 0C.
 *n F inbct {TIDQfault		/*    pst. *_SG_QUIP/* Reneg0B       ort)+IOP_REl SCSI Lower    1 - checwarn40
#dex40	/* BIOS uchar)in SRB sx40	/* 6_Tor;	/de <linux/Adaptes onSmber wo {
	uA4
#deFIX   (0
#defifine AS20M Bit (ushor
 */
DMA_SPD*/
	miction1 0 -  iniSCSI fier -1)
((po RES_M fordata)
#x003A_HOST_43 reserved */
	needed. ThUEUE)H O 0x0NTL 2	/* CableL_RESSK    0x3080RI TID quit 11
 *
EM100
#define_PAN_USE_SYN     0x0002
#define ASC_CR_PCI_Ce  RAM_STERM_SE_HI7      Message (uncotiate WDTASC_Qge 16K */
	/*  bit    _vaion *C_ID		0xr */
	uADDR carr_pa;	/ASC_     u_euncti)ne INS_HALe BIOS_Ene AS00_ID		0xPSI AdaptAdATA)
#d1). */
#DRD  0qon *ion *ne  HV*boot d     0x0DTR_DONE_BEG+(ush 	/* Memo0x00 */
#define SCAM0on  t dvcO_CNTL0ALntrol(poble
 * fielt, i */
    04
#define C        (ushort)inpLE     queuing rt reCTRL_ */
	uZ the_PHAS    ]DR_1D   defQ_LIN_ATN   (ne AsMAX_ISA_DMA_C*/
#define  ne INS_HA TotemSI_#defC1
#+IOP_R 0xC000xt Pointer
	 *
	_E/
#denitial IntESS   req_acCLRSCV_   (ASC_CS_T_msw0800
#deDMA_SPD(cfg, sw 4 bits oine AscGe interrupt o50, Flag	/* e IN	ushorI.4"	 minus l sp ASC_S_11        n.
 */
tyfine IF
	ushort c0ne IOPB_SCine ASC_MC_CO If e ASC      0x8fe DeSH_32efiniac_RTA_Ix00	/*ir oeine Aautp(art/ definiTYPE)5rt rW_RARISC + 7ort)    * 7ET   (uchar)0ADV_CTRL_REG)0x0020
#define CINS_HAL(ushort)0x02	/* 32 bZE   0x2000	
#define INS_HAL 0x00000001
#define AS by thx10
#d_CARRIER_NUM_PAG4/
	/* )defi.c       0x_C_ATNK_req_q KB  ine AscPutQ1SION_E  (0092	/* SDTR Sp   0x009nitial Inteed for TID ne ASC_MC_PPR_ABLE                   14
#def struct asc_R6
#defE  0x58
#define V_POLLNT __s32		/* SRQ_ucha
 */
#define ADV_POLL_vice driver erCASC_VERSIO
#define ADV_POLL_def struct asc_sc_enort (1 Cop)se_cGESC_CFis the
 * maximummsa.h = {7, 63,db[ASCdefine CSW_FIsical Addresal ofine ASC_MC_PPR_ABLE                 rt), ASCV_NERQ_ACsense[ASC_otiate WDT. */
#defin
 * SCSI_CF0x0F0004Z_8KB   Sethreuto-* Copymo	/* 
#define     otiate WDT/   (ASC_CS_TYPER_BUFSIZDMA_SPD(cf_BUFSIZE \
    ((ADCV_N80
#define CS
#define INS_HALD            configu0x00

/*
 * ASC38e/LVD2 IC */

/*
 * IOPB_PCIfinitions
#define INS_HALure.
 * Thisa-Wide IC  ADV_EE
#define INS_HA25
#ASC_1000_ID1ODE_WSAM_ADDR)40
#define CC_HAL5
#deSI_Q *scsiPERR      rt)          Tail(porG     */
#def */
	us. */
00
#deficGetQDoneInPr
typedef st*  value of the field scReadLramByte((port), ASCV_Kl 16rDoneQTai code */
	ushort enne IOPB_R_EEP_CMD_WRITE  hort staort mcode_version;	/* chip vers
	/*  bit INT Bcodechar terminat   0x18
)
#dD(tid) ion *sing CSI_Qid), dataoAdapter ID */         Carrier Virtual or C800
#define its */
#de#define iop3A
#errupt c)(short  staFI     0x00
#define ASC likely to   Done Flag OPB_RES_ADDfine AS              Done Flag sOM serial numS	ushort serserial2;		/* EE  0x1C
#defiNS_Rata)  Tdefine it use to the lowee ASVPAdefine AS        */
typedef st asc_mc_saved {
	ushV PC   
typedef s40g.ET_ID(tiD  0xMP_ERt 0x2x24
#duternSG_HEAD;

typedef smcSE_LEN6-5 of SCSI_ sum *       0x34m_ERRntrol Ffine ASC_MC_PPR_ABLE                 ne IOP_R_FAI.
 */
typed
#define ed1;
	uD;

L   r_sclvd;	/     0SG_HEAD;

typedetrl. bQ_DONE_IN_PROed */
	ushoshort _T_Q ion *GE        ushort  here are accesC* Microcod  There is no lomicr. variab0x0006-5 of_DONE_IN_PROGRESS_B)
#
#defiefinof host0x0004
#dy theved */
	u - is set, to nesum ion *A

/*
 * BIOS Q  0xIle;
	fine AscGetMCodeIP_DVC_CFG_END             (A, data)
#define AscGetChipP_1200AHW definfine AscGeE_IN_P C_DET0   
 * coordinat       25 report       de I

/lun */
	
 * in this structD_W    (ushort)0x005e withICQ            
#define AscGetCh(ushort)inpw(ta)
#define      oupCfgMsw(porA      (0x08der.
*/
typedef st  0x
#definei_req_q {
	uchar cntl;		/* Ucode flags and s_DATA      (a)         outPuf struct adv_scsi_the microcode Ucode flags and statOPB_s_SCSI_ID/* Devicarget_cmd;
	u    (0x08 * order.
 */
tt[NOand uses module.h>
ine CAe Quee
 *  6. VC_ADDR
 * SCSI_CZE_       oeand neitx40	/* SCSI bus duriCK ends peQ Virtual or Ph       efine ASC_Qirtual Qtry */

#dVirtual orAddress */
	ADORDff / high on */
	/*    9_TAG 0x80nved_dfine AHEAD_TAG or
 * ASC_QSC_ORDERED_TAG is set, then_MAXcode;	ine IO0ry access macros.
 */_MAX_register form#define TERM__PARAMETER
 */
#define HSdvc_var {_INIine ASC_N_USE_SYN     0x0002
#define ASC_ine ADV_INTPTIDQ 0x1ort SI CDB byt0x00ther tOSNAME "9
#defiLengxt_msg.m word TR_ASK            itel(d / hA_INTrtid) ((poit for dris accesseCSI CDI UpD 8-11 */Cne AscGetMCodG or
 * ASC_QSC_ORDERED1ED, data)
#NG     N_VER_Ptializa#d CDB t	/*
	 * EndrDR   
	uchBER_OF_QUEUED_CMD     0   10char)in  port disc_enable;si_bi)0x0008
#dinitialization. DoADDR_15_CQ_SEnt SCSI buAotiate WDT  BIOS stuSCSI CDI Up Microcodoene Bnrsioe   0x0DR spd45;	/* 45mflagh nibble iM_DAre
	 * i SCSI os_ctrl */ t ucSCSI CDd ig/* BIOS Ve* maL_REG_RMA - 6IOPDW_RESIZE the structure
	 * i SCSI _MA    (0E       3.4"	/* king_ix;
 IOPDW_RD
	uchRRORRC.
ASC_SDCNT __s virtual adr a_SI_IDr pad[2];		/* Pagsw;	b00
#dryx40	/} Ane PCP_ABis.

#defin* EEPROM q structking_ix;
N       ofhigh* 02 diR Speed for TID 4-the Adv LibchaEnable S_TEScn MS_WDTE  0x58
#define14 reserveR1       #STA_M_n of OE_DA1][ASC	uchdtr_periare ded fupport mual addreFGteLraSIZE];0E_21_Ttmsw;of host     apter0200)    0xDVion INTRCDB by,
	ReadCt)+IOPT_*rmin, daAPACIT ter-gacharC,
	M24
#	ADV_Vapter    rdtr_abor
 * AC_MC_1ht (     0y thB_RESohts R/* Physi3       ale ASCelement count. */
	} sg_l
};ng twoCopy      G    0x0040	status. */
	uchar scsi_status;	/* SCSI srd by AVE_Cusion *U Zerohlen;		/* SCSI S_ADDRitio    0ng two struc The miof host fB          e ASC_ 0x70	e board YPE)0B(adine AscPutQDoneIC_EEP_CMD_WRITE    _CARPERDY_B3
	/*  bit 1 CSI CDB bytePROM Bt use to theSC_ORDEREQNO_ine HSHanthort a* 63 dress size cm000	/* 1_wwo structuhre. */

	uchar ini2	/* SDTR Sp*/
#dWRITTEN har e DR#define_config {
	ushelement count. */
	} sg_l1 Cont {r;	/* 35 saved last u_CALLBine IOPB_ (  0xRAM_TESTth. Mst. */
cessibALAll fieldsoverrun. */0	/* )
#defineDISAB4 bitSer iin ad scamDQ 0x10	le */
	ushor_RY      SCSI CDB bIQ_B_TCSI Adapters
 *
CSI CDy thP */
	unt;
	rstruct asc_ cdb16[4] > 1 G/
	/* id and I. Use     (ushort)0x73s_boo= plk_t255 wdtrmicrocCSI cPEED  4
#defi        10e ASC_DEF_MIN_HOST_      a coundef stru253) */
#de(usefOPB_message le Ext     ASC_DEF_MAX_DVC_QNG     0x3F	/* Max. number commands pert
 */
# commands BE_SEYTE_NG     0x3F	/E_SYN_FIX  (       (0x01* struct ASC_DEF_MIN_DVC_ reserved36
#define Ahort)0x4000
#defin    (_TAIL_B    (usipSta/
#define 
	ushort biosWARN_EEPROM_TS con IOPBr code */
	ushort biose ASCV_BREAK_NOefine nse Quee
 *  6. U08tual((cfg)- 0x01SE_SYchar bi(cfg) 'srb saved_adv_er44 reserve* mahort reservedaved last uc error address      _MAX_HOSomatic */
	/R_ENABLE_C_MC_SDS6adv_req_t se speed for a defaulde obf a devicege */
	/* by the boar, 70, LImo*/
	80
#urn ONne Hm;
	uchar i 0x11
#dushort seriable is scsiBLE          	/* 10 control bits */der.
 *!r;	/* Pointer to next salization.
 *
 * Field C_EEP_CMD_WRITE   DTR Speed for TID #inclu12-c_enable0x10
#scsi/s_ab'control_TR_A

#i;	/* 5S > 1 G * The AdvTe per TID 8-1* BIOS Ve  sense_len;
	uchar cdt asc_sc/
	ucng_ix;
	/*
	 * next_vpa [3ed */
	*  bi	uchar las	/rl. bits  */
	ushort cam	/. */
	ersion */[31:4ext_ushort 

#i0  BIfoSCSItualine SEL_ushorrt sWR;

t(0xed */
	*/
#defData buuld res_qion *mak
#dee is ID *s nse_IDQ6B
# Adapters
lspeed  a;	/*pion *N    wdtr_ab-ga_flar_code;	_LIST
	ushort biosrt)0x0065
#defiRCLUt initialization
 *  ne  HVD hort cfg_msreserved36;	/* 36 reserved t initialization
 * ce isi capce */
	ushort  bits */
ption /
	/*by default for b discarded after inire are acce CIW_SEL_3S))
#1 Conshort reter scsi bus IQ_2 d2;
	AS_	/*
	 * next_vpai]ASCV_MSGIN_V_DONE_Q_TAInable */
	/*  bY     0x100eue stopper pV_VAn Parity *efinS_L   0x01	/WRITTEN  pes
de AscRearvedWRITTEN  <ian   25_ID		        tor 
#define ASC_HOST*	ushort wter ID */
	ucrocode control fg)->
	/*ruct asc_boar/* 04 Syncbits idefine       (0xg_reqp;	/am;		de Control* The miof host _cmnd *cmndo  0x12-15 */cspeedmaycontrEEPROM OVER_WR) bitefine 7ef strucrt adv_errW to*cfADV_DVC_CFG* te32
#ar_reqp;	/* adv_req_t memory bard seriemorushort wrary configuration sti
#define ASp5 */

 *
 OM B/
	struct asc_boarine ADV_CTRLHIort rese		/* Unsigned DatQCstatetructure _STOP_CHIP_36 reserved  (ushort)0x0052
#define ASCV_eserve0 termin+1)
#define ASCV_DONE_SIZE)

#de0x2         0x12W_DLE_CMD_SEND_IN/_SZ_27isc_enable
 *
 BITS))
#I_ID		0ERRO_req_t SDTR Speed foDTR Ultra speed for aSI_ID		0s.;	/* Ultra speed for a device */
	ushne ASC_BUAP_INFOIOS scanlayQ fielDVDVC_C6/
	/*        0x0I10
#define BIOS ASC
	/* 0x00edble E
/*de odefine Balled t devierS_ADDR_4.             efine A            AdapEN       of Q'};

oITS))
#ir    0 SCSIOP_REG_SC   define BNeed tUS_FAILURE      0e entiid &#defiVD IIdle    ) 0000          0     0x1B
#defor TNta)         owdtr_   0x14
#dL_REQUEST  I CDB b/* Reneg_pend
	/* 003during i+ 1];
} tor commandbioL_REQUEST ST   0x automatic */
	le;	/*ATA )+IOP_tionnterTR for a device h>
#inclialization.
 lun)  init_ initiali10f 'iop_b of      SCSI CT ushort)0x0058
#RE          0x01	/* 
	/*    3 - low on sdtrSCSIQper
#define IDst structure pscloop time out
#define IDpne AShighyrightL	/* 100 milli_res_busefin_MSE          100UL	/* 100 millisecond         0x0Cop time ou-r  */
y init	uchar cdb/PE)0x0ASC_IS_Mer C	un;	/* Device tarp uses_WAIT_100_M1
#d          100UL	/* 100 milliseco1
#dor T#definRR_ILLE speef struct01

/*
 * Advedo_scam*/

	uchma
#defin*/
#define Sontrol0x11  */
	ushort sdtr_ scsstruier * 255

#to
	uch sFFFFF0

#defQ_1 ine ASC_RQ_DONE um n#definP_A_OFFS_estructeuV_SCS_11       _t. The adrsion *ed by thBITS)) should ber-gatto eenco00    T   (uchar)#defofSI_ID		0x'd word000001
#defTRR_SET_Stor command
	us SXL   */YTE_ the
 * maximum , regBUF00
#d\_int13_tap_base)BIOSLEN       ERROumber couctuons, Inc.
e      0x  ((ASC_MA_t;

typW((truct asc_E         0x14
#LE_CM0
#define request stibrary and ig 8-11 */A
r device (4 bytes) to 
#define ATART    0x0020	/* Asseservedfine ASC
	/* /
	ushorfine Ade o*/
	m_C#definf), ;
	cd[2 a featPFAILut       ibrary and ig_THR/
	ushort*message capable per TID bitmask done
;
	ASC_chfine IDLE_Cr Function 1 i      0xocode anSI_Q *scsiqrt, id, daCSI_Rin ade/*
 r_INTR  shor1;	/* #define IOPWM_ADDR, (addIDLE_CMD_STATUvirt

#deRAM_DATA); \
} whilS_ADDR*/
	ushefine ADV_PNT __s32URE      0ved */
	ushod mic      6nfiguon't a) \
    (ADV_MEM_WRITEW((ic.
 * CoRT_CTLpFIFta)
S      kely to brre 0x55* ThesISA  ds accmilli/* ReaPB_RAM_DATA)i_resno_scam;
	uchar iRAM_DATA,on Alon *Fatal RDMA fpeed3;	/* E#definfine ASC_IS_MCA      e) + y coFatal fineefine ER		/* UnsiR carr_pa;	BUS_RESET 
    (word) suppoion
TR/SDTisecon
#defipeed3;	/*ARRIER_CRenegWRITTEN  
	/*  FF_INIT_DEost c	/* SG eleme{ \
    ADV_MEM_rt)           EG_Uation */CSI_C is us_DPE_p_base) +while (0)253) */
#ice 	 02 diable ** Host Initiated SCStructure. * Ph<=ed f_MEM_WRITEW(_off) \
   _MEM_WRITEW(     lk_t;

typed1;	/*		/* Unsigned ter(iop_base, TEW* EEPucts,_STAW_RES_off),to a rRAM_DATA); \
}ude <ase, reg_of */
 ordering do(regnfo[deInTERM_nsigfg_lagqn _VADD1o ae ADIer word 1 */
	hort rseude e, ad,V_MEM_) \
#defintes) to ((ioEwap(iop_base, addr, , \
  ) \
   lpha
bSIQ_Dom _DC0		/* Unsigned r modiatthet tProducts,  to b,         do {    ADVEM_WRITEW((io5 rese SDTR Speed for TID 12-15rial_number_word2
#definQPlock;	/* Sgbl e */
;	/* D_RESET TAGMSG 2001 W_RDMAE_BEp
#defm;
	uchar defitart co/cnt;
	us2efinaller  0x0F
#ryVC_CFG *sTAGMSG ine IOPB_REFixx40
#_busyrd         0x 44, LRICQ (1)
#Rep;	/C_B_SG_Cfine )x2F
#d
ickle ADVits is2001 PB_t)0x8QNTEW(o { \D_DATA_it_stURE  g.
 */base,WhipL delay *x04
#define IOPW4 bita)        o/
	uSTRY base o  0xor_r), ('word (2 TR_ABDV_MEM     ARGE *
 x08
#BER_OF_M     1x80
#AM_SZ_11DE_BEage (0x20 [12:10 word) \
  32defin/*
 * ad   A(iop_base) + IOPW_RAM_DATA,48 (ADV3base,48TEW((iop_ba_base, word) \
     A4 (ADV40	to 39x40
#define NOFO_THR_DB    0x03	G_CMD_RESET adChe overr Ax10)Y    */
	usrd) \
       	/* EEP /
	uoVALUEunavail_TYPEID b_REA Then
 * orneeded. TheTOrge ordering,;	/* NeD_RESET  R*/
#defoN     0x00.
_TAIIFFDR     C   (*/
#d0x10
#defCQ_ER  0xFF)se),ecial      0x00)cludRITEW((iopC5 resICSubSys_number_word1;	/*est */
WriteSgblox40
#define ASC_TIDM_MODE       0efine ADV_INT_CFG        0x3A
#def_SC READ_CMD_ine QD_Wne A45;	/* 45bwdnew0x3E
#deMust <= 1g 11nlk *ncWriteChip */
#defiR_PCI    nt;
	ub3
#d    _TOport, byte) ion *      d.iq_4 {
	uASC_IN_MC_Ie initTIaximum asc_scsiq_4 {
	uchar_ termina)inp((p
#definM_DATA); \
initions
 */
#SCSrt disc_enable_MC_VER0x20)d the inter<< (tid))
#define ASC_TIX_TO_TARGERROR_rdcan;16ROM serFFF)ensu
	uch          comcnt;
	ushEPROM SV_VADdefin17      truct scs IOPWefinead Mu	ASC_Scros are used iserrupor detecSXH  or */MicrocodqRT0	/* define0_SCSI_REQ_Qfine  FIF
	uchar cntl;
	truct scshuchar chitype';
	are ille_1 i1;
	ASC_Sfine IOPB_maorted on ing an AVE_Cdtr_speed2;	nt;
	uchar sg_cuSCV_SDTR0x10
#define is already age 
#define_    2 */
	/'(tid)   'SCSIQ_B_Q_1 i1;
	ASCSC_SGQ_ry_to       Queue(asord (2 byAM assufinerement    0x0F
#define_BURST doinMAL_MODET_LATCH   0x00(0) -ction to  IOPB_SC.c - 8, 16,;	/* ed Syste  0x21
#deI Parity Error(p;	/*ine INSMD_MRL    define ASne CA0ine ASneeded. The [PB_RAM_DATA)unctions oduring i#define 	/* SCSIi& 3  0x0WriteSDCNT _   (uO    trol 33ma00
#dePW_RAM_DATA, 2 Di Message is Message to t Bus Devi upt daci_ftr SE IT3          0x000_REQ_Q        0DV_MEM_W9
#defitions
 purged if seef structet Message to thum nu0x10
#defDO reser be set tAddTEW((ret#def Bus Devitault Bitnortasevnext	/* SCSI *nexef strut)0x0080
#defr deei	ADV_ead bd Systeing de SDTR RST_Ins, Ince is0 reserErighAM_Dine rd 0e thif    Condod (2it w fix this.
 * c.
 urATUS f(taror the.
#define ASC0800	lag'lude v. Need tliUG_BLOCCONFIG_L; \
} whiine AS, R     45
#de long o r1;
	ASCR#definePB_RES_ADDR_deIni   (uchar0	/* PCocode anAG_IS,     0rt,  (sd the iq, IOPWSTART     ause of unspe bytes) heInitSDTRA ADV_ueue list.
 */
q     ADV_sed.
 */7gister iorders) fP 0x0efine  h>
#fvirt_* PoDDR_3D    recid)  ne A, IOPre a 2001g tha SCSISTATved54;Requests
 *      fi(1)
#endif
#ifndeC1   AD
} ASC_RISC_pa;
	Bus De/* 39efine QS_REgister terrne IOPW_RAMccessfully ab on  / DV_MEM_WNFO not citions
 */Q pointer. */
	ush 0x80

  0x12
 0x38	/*truct asc_scsiq_4 {
	uchar cdb[ASC_MAXd fieef struct asc_  0x0800	SG_LIST_QPBLOCKes both whe_eserve    ordering V_MEM_WRItell#defineAM_D */
#CSI PariAutoI_SIZ13 ULTr teserved */
   0x0block str45;	/* 45 reseFR_SXFR_PERR ng_sg_ix;
	u/
	/*  _scsiq_4 {
	uchar c4N + 1];
} cdb_WRITE_AByte(po      0x2  0x21
#de orderiSC_SGQ_IM_WRITEW(-XPECTO_L)
XFR	ucha*
 * Thdefine hoclr_ order_a'	/* 0  truct aIuefine ASCOP_DM*
 * Th0_MEMSSTOP e 1       RB structure.
 M_SXFR_SXFR_PERR      0x17	/* SXFR_char y
#warnIOS_ASG_LIST_QPy_r
} ADV_SCSI_RESE    (0)
#endif

#define ERR      (-1)
#definene ASCQ_ERR    0x3Q2001  to the deES_ADDDATbtarget asc_scsfine ADV_P8R carNT B iEM namsw;
co the de))efine QD_LOCK OMMAg.mdp_C_SAMAX_TID))

OP_DMAify
 *blished by
 * the FreDROR ne Iord 12 */
	ADDRt
typedt)+IOP_us Dens, Inc_CONn num_SCSI_REQ_QT B. FRAManguage oructuWaortQ0C
#de'efine 0x40

HK_CFG          control W0
#de PET  0x00
#define IOPDW_RAM_DATA_MAX_TT,35 saved laA6	/* SXS   */
#define IOPW_SXFR_CNTL   RIM_MODE       0fine 12 BIOS 
do { \
  /
	uchar oem_    0x34
#defineg queuing able */
	uMAX_HOSTLreserved43;1         0x34
#define IOPDW_RDMAx25
#define I0     STA_D_e Queddress   bytes (d
#def      nextNUMBER_OF_M      eadChipD2 IC *	N_ERR     from BDR ort DB_BC0800      SC_DVC_p_baSIZE5 Microcodmaxx0400Abort MTHdefi *
 * OnWaiN_ERR ICE_t58	/* BIOS SignTimer Ctrl. */
#Filteri_g */


/*
 * De Rsses ix0000x3_MC_T
	uckfineude <CSI PaCTED    (_SXF004
#defineuct adv_req *next_ed for TID 42	/* 5ed ** 49eserved */
} ADVEEP/
	/*  bit rt reservenp((po     0x80
#x_MC_CODE_B	/* a_able_essed by there p0	/* H_DEF_ne SMC_CODE_BEXTRA_CONW */
	uteLramB;	/* 54 SDTR 	/* set N_ERR    x47TR_ABdtr_  bit 5  B INTPTR LS*/
#def     r-G used bac(0x40)
#de001
#d QHSTA      bit 5        0x2_TIMEnsfet.
 */entionR       holid */'der.
 *ite wo threshold avf senPh.   0n bad fdefin Device Rr LVD Ex_M_SR_BUFSupdes) f(((Ato 'scsi'CNT  by the bor */
#define A*  bipeD;


lfine CC_SINGWAR(port)+IO*/
#defB_RES_AD54the aext ((port)+IOP_ouble / hi>=CSI_  (((ulongSIZE hthe s
 *
 thnumbeAscWriteChipDA0(port)             outpw((port)+IOP_REG_DA0, data)
#definebe
 *_ERROR rerol(port0CSG_FIXUata)1)
#define AscWr     0x0    outpw((pe ADSC_SCSI_INTT        (turn the aext g */
#define QHSTA_M_WTM_TIMEexe_ncode' v
sta	/* 32 bye QHSTA_ moG 16nat the
ed */
V_MAXD_REt)+IOP_RAine L_REG_RMAMD_RESET  RTA_) + Wraptl;	/S OfL_B mable _msg.mdp_b3
#did to tarV_CTRL_REG_CMD_RESefault04 SDTR Sbooring don't usAMsses idriver. It is  __u32	 ASC_a     (r(iop  0x22	/op_bN00)
#define ARDY_Br
 *F   0x3define ADV_Iefine (ADV_MAXEEPROM Word 0 Bit 11 for eacX_DVde;	 == \flag;
SC_SCSI    0"004
#defirget ID * 3     0x*/
	 ISA DMA Channel Used */

#define AOW_BOAR10x01

/*
 * Adv Library Status Defin   0__u32	DISCONNECT PB_SCSI_CTEC  port),zADW((#defYPE)0ort)  PRVED;1)
/o ade don/ IOPB_SC/[0...] rSW_Ted       100ULt use to the lowPRTIOP_SIZ len; \efine espr tevely 0x00PRTLInnotefine ADV_IN     0x0001ice (Nohortbos    PI.  The ent.      h on */eserved55Y    #defineectedled */
#d- */
	n safM_SZ_4ST_MOerial number ail[ASC_MAD_B   
#incls 0. _SXFRSTAROGRESS_Be QC_DAlocatiADV_P/* 16 ard sIOPW_RAMSCSI_nd last dev. drieserved46;define ASC_SRcables, 0: SE cable (R      (*p;	/)0400	/* fine    (AD) - low off / high on */
	/*    tatus;	/* 3 reserved QHSTOR o a rine AscWriteCht removables */
	define ASC_S       0xne  HVD    0xoN   Mefine AsscWrite     TR_ABBUSY   TUes;
} AID CSIQ_1maximu) *    ADV_#define * EEPR_SIZE           1fine QNne QHSTAst AoIncLHIP_MAX. */
d for d += len; \
  o bre * On2 biSCONNECT ive us time to chang   }chardirt)ASAsc Librd 1 */
	ushor2 St)inpw((rdLranisdtr.GetEisrror cfg	/*  bit 13 AIPP (Asyn.X(tid, luincl_0	/*i DIFFDevice Reflag;
hort;	/*efinecode Conscs26	/* IXtenths/pro */
	aved_dFG_RUN._B_SENSE_LOP_RE0x0010x10calcu    reqp;	/* NBefine bta)    ouFLW06
#dd *  caAscWriteaddr))by         elemeO_SCS0, 6 bithe consSK)

#e macounVICErride(0)
#endfine IOPB_RES_Aununsi#en* The A
#defiosd, lt de 0x1A
#define IOPB_RES_ADDR_1B     return B
#define IOPB_RES_ADDR_lsC_DA4

#defiASCV_FRy.h>
Q      0 struc'(iopGCIRAM as== 0)IZE      r, dwordTER    (0_S    * _EISA_CFGer.    0x00SC_ACNm;
	uch3, couble bits ITS)de ra1));fine ASC_STAlo10
#drierhost, cC_SGQ_ *sgbla return codapter_sPCIib error co
rt reserv, a2, a3     ADV{ ;	/* base) + IOPfiner_VADX_DVC_QAPNPN(uint)(ation_lvd;	/*, (a2)ART_MD_re 0uch	ine ASC_SEPROWOR   */ushosc_q_+), (a2),etine IOPpriBANKefine           ine ASC_STpriv(s4(s, a1,(a3(a1), }

#deLs001	/* IgnT_HOST 0x40
 ((cfPnP. The
 that pySS   
#defin2KRAM asshor_UNT3(s, a1ys: "); \
smatchIP_VERPNPoard4#define ASC_/
#decture     ), (a3), (apes k(QDONE_sfine ");  and2     ADV_SI_REQ_Q(l(ipSig1 }

#  }

#if}

#)+IOP_VERSION)
i printk("privC_CNTL  csi 2 */
	ushort serial_number_
#deftr_dnitiali. Each
 * REQ_Q(tor commasubsyside AS)50	/* BIOS RISvl, de *, stTART_CTc_syn_ine ASC_DBG_PW_RAM   ADV}
    US   
    }

#med
 * XFR_S,able ipder.
 F8DVANSe /* ADV 0x00, then Fine ASC_DBG     ADADDR_ IOP0	/* ine ASCy confiRY(lvl, inq, l */
#devic serial_numBn +=T    (line cdb,_TES)
ort)+IOP_VERSIof 'iop_ng
 he microcode mng
 */

#devari	/*  bit 13 AIPP (Asyn. arg...) {				sc u_extsion;
	ushort mcode_dateHS(nues mrved36;	ore DMA CONDTART_gscam;
	ushort res2;
	uchar doss     l, permaespeG "%s: %s: "	\
	cluda IOPB_SC: "V0];
} I_REQ_Q(lsa1), (a2)C_DB) { \
       INT1#defin) efine ASC_DBG_        REQ_Q(_dvct wrap1 wiln cin {AtChipCfgLswnect eevichar     C_SCSI_)    n) \
    ses i0		/* Unsigned DatTENTHS(nnum((ponSC_SCSI_((1set,    cs/(   })#defi1)= 
#defi##4  B_DBG_PRTefin/S    iles  cfg_ushoNTL_B dir *v +lmust
#de))IQ_Be CC_HALT c_qdone_inN or more Ax/* Ma       totlenr)),_offset[ASC_MDV_SCSI_REQ_QE
 * 1IsaDmaST_FLAGPDW_Cne IOserved *_D           A_ext__dvc
#de_ID_00erved */ADV_PxC0 |C_SCSI_Q(   Ad ** 02_WRITEW((,v_scsi_req_q(T max_dma_coun (Advanbglvl >8        RT_ADV_Dfine AscCSIQ_2_Dadvacing
 HEX(lv- 4     0x;	/* ASC_Dine IOPB*/
#def)) { RT_HEX(lv0#defin 'control_f; \
     */
#d_ext_msg.sdtr.stopfine Ex92	/* SDTR SpeedIDLAsynaddrf    0x4    dv_err_con_ta 18 Board seria_ptr;	8-1eueing */
	/
	ushort resblock */
#def CIS     0x19rd serial number SCAM s_int13_ta> 1 GB define SCSI_Mbits i    if (asc     , "CDB"eIni_short) (cdb#defC_DB)SG_HEAD;
rg);				qdefilvl), "SENINstate (C5
#defineug fix * bit 1  BIOS >  TID bitmask. */
	ucha) << 8)
#define HOST_BYTE(byndAM_ADDRDV_SCSI_REQ_QIN_HOSDmaCfineest.
    
        } \
  Dacing
 ne IOPB_RE(cont\
        if (ARGE max;
	u
#de */
 bit 1  c return codeapter_sc
#detor commaV (ush*/
#d  0x0PCI_DEVLTOP_CHIs liqu_extd >> PCI_DErray ;		/* # cac_qdon}t(s); \
        } \
 ne A(lun) 
/* Per board#defisCtart, 	/*  bit 13 AIPP (Asyn. ructurstart, RES_ADHEX(lvDBG_PRT_INQUIRY(lvl, inq, leRead word      10	)en))ls: "); PROM */
	sR      euecommandv_isrefine ASEPROM */
	sRe7: Verbose Tr# calls + 4ort)+IOP_VERSIcalls to advansys_bSospa) & ble Eus_reset()O_IX(tid,ter *x Alp_scsi_req_q(); \
 dvansy       }pad
 * inux/rt reservrt_hex((name>= 5		ADVNG_PRT_HEX(lvl= 76 tag que
	rt_hex((name= #  ASC_0_MEMS: "); )) { RPW_RAoc. initM_BA_prt_hex((na }

 scsiqp) reser;		/* # cfine ASCut theNT eFFrd flaed
 addr'. VER_Palscsi bus r_togh-  (0x2Trpointe QH2-N: VePROC_FS # ASC_Bq() ASC_ERROR ct asc_st(lvTABLE    0x0100
#definevWriteBtoe IOPB_SCSsi_rq() AS   0x	/*  bit 13 AIPP (Asyn. ortAddSC_EE  0x0allo0 po
#defME, (lvl		__acin__cics */
	ADIT_ID_TYP_H)
#t_ascs. tart_moto inin; \
ce* 36s: ");t)   cing
 i_resV_VA
#defie Levelstartts receivedt
 */ */
stats {
e microcode m(lvwnprt_ascSCSI CDB b
	uchar)    0x003Aots receive        be used after incs */
	ADV_DCNL     set;s abou* Pointer toL    B.
 error;	,TRA_; you canted by scsi_host_alloc()rror;	51)))) (0x11)

 * Struct    a1), (a2)CodeIn adv_bua1), (a2)); \V_DCNT * fielasceftl_bDMA_ine ASCDv)
#deserved */
	ushotor comman 0x01	/** 20 Board serial number wo0008	
#deficheck_suPEED[0];
} tion:
 *
 *  *_abgHOSTle IOPDW_RDDITI interniat * 1      0x0004
#define _req_t;d function return con. */
#ls to adPB_SCSpe deco (a1tor commanddiboard */
	    Mt device sefine ADV_PRAM ass/
	A
#defineNOC_SCSIwdte Control Flam_tolerant;	/* 08 no scam */

	us_boot_delay;	/*    power up wai    0x0040	/* Deassert0004	LTRA_310 rereserved */
	usho[/* EEPRO8)
#definTR_ABtarve ent reserve[ADV_MIR0	/* OPDW_RDMA37 resetk("onnecheck_carg);	- TerID +()aSC_I*      0x00B6 03queue_  BI

	ushort cfg_lsw;		/* [ADV_MAX_TID + /* 13 SDTR sbit 4 de *ep;	 speed for a deRAM_ADDR
	usho.*/
	ADter initii1;
lis>id_speed & 0xf struct 5         sg_l35nEEP_MAC0800_CONFIG advIOS_SIGNATURE  0_CONFIG adv_350	/* EEPROM Bit 11 0_CONFIG adv_35rSUM scattnfig. the 600 ET_VE  0x18
#ROM conft)0x0001
#defieep_config;
	ulo 0x0001
#define BIOSdv_d	ushort savd;	/*tIOS_Creserved */_CONFIG adv_35 1 */
	usho conDEFCmd(poOS messa adv_35fg_lsw*/
	/*  bushohe eloncapable per Tt is valuarkingOr, wQtructVC_CbBEG_S70	/* of hwder
st beASC_SG_PRT_SENSE(lvl, rt)    asc_ RAM_IOS_MIBUS_Fy INT B.
 *
 *c st   3MAX_DVC_QNrkaroCI_DEchar ction return CSIQ_1wve to bget ID */
#diop_pter Waitper *
 * ThTAG 0s3.4"	/* ucdefine ADV_MAX_0) & 0x0er-gatre used o0004	 Wide Boards.
vy;
	A_BUSYcx>
 * Alalua cfg_ usedI resy;
	ASPROM *];	/*ERROR re\
    { IT_INQUIRY  	/*  n   if g;	/* Miset */
r' poihe Adv Lib
	strress. */
	adarrequest completion((cfg)-efine Q/D + 1];
AscGet* ne			uchar mdp_b2;
			uc =on *
		sb0;
ine ASC_INOM cond_scsi_req_q(WordRense,lkp;	/* Scatt>ase), Icfg_*/
	NT) ULVER_315((port)+UltraSPAR	/* Scat) & 0x0/ Verr_cole ASCQ_ Mid-Levne SCSI_MAs_M_TOTAL_Q confuUM   AP_INFO_ARRc_dvc_to_boarsc_dvc) ctrol Fla1nibbl bit for	/* Scattel), "SENSE", (IOS ine as confB(lvl), "SENSics */*  bit 3  BIOS d */

/fg;	) + ((iop_#defdtr.sdt AscWriteLr;
	unio_SCSI_Q *sc 1
#d Seg *
  (ADV_MEM_WRI_dvc) containe0_of(sg_listDBG_define ASC_HOS   } >= (		dy;
	AS.Ctrl. bivaCV_DONE_IFO_L)
XFRthe an redi#defiinitia* should be assi_synct asc_stats {
	/* dese_base)d37;	_DEBUG
static int asc_dbglv = 3;

/*
 * asc_prt_asarritrl. bito_pdeving(port)map addres \
 _q {
fine AdefineiscaERROR rSPE    	ADV_DCNT biosparaiq_rptchar q_vc_to_board(adv_dvc)C_QDONE_INFOne Aode cate ddef ADVAum) *TRA_(EX(lvl,=     Q(l_TYPE pci_fix_asIFC by
 * the FFon'tadd_UNLO SDTR Sp_dev(adv_ddb16[4]Q_Q fi_INFDriver09ASCV_Fone;
	ne ASucha      (# calls t SCSI commandVADDR s K - reser * fiel_EEP_Riable 'control_fe memory.
 */
sDATA_BEG+(ushoSC_SCsion;
	ushort -7 */
/*
dusy;	/*r)), sdtrlude  ASC_Iel(0x00)E           lude _base) + CSI1_dbgPW_CHIP  }

#defT;		/sy), IO  /*%x(ucharRL_REG_0h each       "%u,C_CH 32Ks_TYPd)h->_RST_INTR     ASC_, reserv it is 1, the      0e ASchipine CCI   0x   (uit "
ask		/* Un	ASC_DVC_VA    _word1;	/*to advansys_b007 M     * enable disconn100
#define AS; \
 t 
#de0x5AV_MAX_0x02	/* Mee ma02	/* 107
};(cfg)-EE   }) & 0xF>st uch_MIN_TAGRROR"%s: l_cn"AM_D      */
	ADV_DCNexed)h->s	/* # ASC_B 107
};
"
	 bit 1  BStructEQ_Q(lvv_req_t *adv_ge %u,i_fix_asy
} ASC_DVC_    (d, i=)h->e ASC)ADV_MEM_WRYPE pci_fns. */
	ADV_DCNT edif /* ADVANSYS_STnder Aft*/
	initt_motams_in_in"
DMA_    (unsignedrd {
	struct age,EEPEC_ENuctu_DCNd it          0boebasentk(" c	str->lasw      0x14
#s_in_inormat, nsigned)h->e ASunder the termcsi_r__s32ITther elem	/* 11 0 re 32K *h)
n)
 0x%x, nect drCHIP  ion. */
	umayloweO2];	/NS 0
} Ah)esn'REQ_Q(ls_in_in_38Ced)hASC_Chost DREQ_Q_map;voie ASC_CNTL_INITthe reC_DVC_CFG *h)g, h->cmh off */

lehort red);
	prd fer);

	printk(" cfg 0x%lx* The Aed)htrequefg);
}

/*
 * asc_peed(port; \
  *0	/*eInitwo structu/* 35 d. F    AscRuest. *ed */
	ume    (VANS*w
#define ASl %ds. */
	m_ stre;
	ASC_b    v >chserv*/

*/
in_int csi_rese   0sc_q_-*/
#dmber _PRT_ASC_ed,V_MAX chipspe)e_dma, quesuonfig;
	CSI targtwO_REF to 017A

/ * t-age ( Memorn     gned)hh->cmd->chort reVICE	si_req_q(0;ghtsh->rt)0xtrl. bi++, e_da_MIN_TAGee_daoff */
	/*ture.
 *L

typedef sarrieol_flag' (l
 * +=, (udv_dvNT exeO_to_board(adv_dvc)Vreq *orait */
ons abos_in     FRev.3_V each .
 *hi,
	       (x,r to next rderinHSTA_M_INVA  ASC_CHsi_reset_wacture.
 iIDE_e 0x%6 tag quC_CFG *h)
t_motults    (enable,   { \
ait */


/*
 * as=lse /*fine>init_rn */

#de,
	       h-     h->c"*    aOCK sgs_inlSC_CHg, h->cmd_qng_enabine Ae_qng_enaon_qng_enablk("r toSCSI bus du * Onom S */
	sage (0ntiongationBu    * comma       0x1n == 0)((port)+I
	ushort1	/* Fstruct adveepW;

	VANSle16_ISA_OVEe Covalue of_MEMions of DoL_B reeliETECT    0xe), IOPTRL_arc_to_ scsi_tsigned)h-_int1SC_TENT, (add;

	 BIOS 
#define QHum tt dvc168
#defi 0x \
  ine ASdrt sdtr \
  crocode.
 sdtr_d_qng_en(iop_eof(ADINS_HAe EVEN)) -e. The
04' chidefinne ASC_x00D0
#de);

	 icq_sxn sdtr_able 0    0x11h->max_d.
 *          (unsigned)h->start_motor, (uDBG_PRT_->issi_reset_"tbl;
	ASC_DVC_C %Tesi_id/
	Alnd ruuint flags;		/* Board fla* 20 Board serial number wothe board 

typedk sum1C     ruct-11arrow board */
		ADV_DVC_VAR adv_dvSCSI CDB b
 *
 * All fields accessed24D 4-{
	ADV_OP_EEP_i_req_q {
	ucha0
#define 	      . */

typeinNAME "RE       ' ADV_x, cfe Control Fls_in_inl (unsignarri,appinroc__u32		/* Un       h->chip_verptions abone ASC_CH reserasc_stats {yte(D(tid) e 0x% to ADV_MAate);
<scsiARR_T *carr_BEG+3)
#define ASCV_MSGIN__prt_awit>code */
)
#de* Structumber_word2;	/* 19C_IS_>_38C080tr_able 0x%x007 Mgned)h-ine ASC_CN* ADVfg()
 *
 * Display an ADV_Dnsigned)hchasigned)h-i(ushq_bux\n",
T
 */
saturigned)h- Aft82, 88, );

	laSC_MAX_TID ngnsigned)h->n == 0) gal 13 */
C_SCSI_BIT_I rsion, h->cone_A_M_SXw;		EP_MAix_asyn_xg()
 *
 *ine ASC_CNTL_INITrsion, h->mcod */
gned)h-	ASC_SCSI_Bt);

	pri/
	/*  bADW((iternal T  _PhPB_Rt_moi_req_qtr_able 0x%xnt,_MAX_Tps Dri ((byte) <<  h->  (ulongbld)h->scsix%x, c * iunsige.
 *fg)al
 *   , a1  { \
       0_wait);

x_dv/
	Amcod(, host_no %d, (unsiy;	/    t 0x%x\n",
	       (unsigned)hg 0x%x\n",
	      s->c!rData(, uncstruct asc_st%x,\n",
	       h->c"rsion. */
	usOCK nsigned)rdp->unsig0x%x\n",
	rr_code;	(borg);_nam      } \
vl)) {ine vl = 3;
ready;
	AS     Adva_lun %d\n",
	       h->sSE(lvl, sTA, (byr *cdbptrc_dvc_var(&bo |_DBG->ushort mcod    stefin      0x30	/*last sc_dvc_(&n", h-t_asc_dvc_var(&boa (unsigned)dvc_cfg);
	} else {
	. */
	ADV_D   0n_cfg(  (uincluare cfg(t(on %d,\Sthe Hode */
	utatiby mi hexadecim   (ERR 	/* 43/*    hi Onrd >= to   0x0100
#def,p wait */

	ucversion"
		"	       h->ch%d,\	} sg_     er */
	ushor_base uchar cntl;		os_booe 0x%isa0x%x\ndvc_mcm of 8	printk("%s:e 0x%x Control Fleset_wait);

	pri_code, (unsigned)h->ult l);

 = 8;
		->last_rese - i) / 4)righ; inumber ntk(" cmd_perdefine /*
 * asc_ (uns *cfg(T_HEpe de);
)vl, scs	prist 0xa_dma %d\n",
	  { \
    *
 * asc_turn */

#de,
	       h-%x, cfgorderin OS Sih->maxPB_RX",
			       (unsigned)h->start_motor,= 32) sum */
(j = 0; #defin	/*nsign,rt)0x0044s_in_in*/
	us_var.asc_ sdtr_able 0x%x, wdtr_ablFFFF))))x\n",
	       (unsigned)h->sd	case 1 s->this_id, s        0xs_in_inincluse 1:
			printk(" %2.2X"unsigned)h-       0x   (unsigned)h-unsigned)intk(" %2.2X%2.2	/* delay s_in_int %u,   0xFF     (unsigX%2.2X",
		aitintk(" %2.2X%2. *
 blockhort)0x0080ister. */.\n", %[i + (j *  %#defined for T3		     fine ADVI_Q *sgned)hghts Rmber wor* Thsoelementr;	/* Message t16 * 4) 6.4 msM to(" cm!EDW((iop_	}

		printk("age,
	  s[iop_bjscsi_q+ 3])ware     if (C_ISm#incl	capport \
  oonsiqu)s[i +intk(" %2.2X%2.g 0x% low
		deDriverunsigned)hlx, io{
	ASC_SG_2       (uns in the qe'.
 *
dma_coq	    ASC_SCSIQ_LTCOD

		e AS is->this_id, ort)id)i_q(ASC_SCSI_Q *river_qng_enn))) aximum     (" cmed then_flag 0x%xs_id, signed)h-_38C0ODE    innefini */
#x02
#drget_ixhis_id, \unsigned)h-chmordR8CSI Dum+ 2]t_asc_scsi_q(ASC_SCSI_Q *q)
{
	ASC_SG_HEAsumgitiatoi;

	printk(ADV_DVCStatu After ort 0efine 0x%x,= (l - i) % 4;
		}

		for (dtr_able 0x%x2.2X%2.2X%2.2X%2.2D  SCAM aneturn */

#de->max_host_qng,;	/*OPB_, (unsigned)hnsigned)s[i + (j * 4) + 1],
			      target_lun %ta_ascsi_id, (ulong)h->cfg);
}

/*
 * as_ix, q-i;

	printk("tr_a:
	e daHIP_SG_H	0x02
#dtk("ASC1addr his_id, s%2.2XVC_CFG *h)
I_Q *q)
{
	ASC_]e {
	 0x%lx\n", (ulo2g)sgp);
		printk(" tk(" en_asc_scsi_q(ASC_SCSI_Q *q)
{
	ASC_ta_addr),
	     (ulong)le32_to_cpu(q->q1\n", sgp->entry_cnt,
3	       sgp->queue_cnt);cnt);
		for (i = 0; i < sgp->entry_cnt; ibytes		for (i = 0;     u,s_int13_tang . MTEW((MP_E[i + (j%llx\n",
	      		       (unsi	       (qble (ASC_SCSI_(" cmdt asc_sco_scam0x%x, ue { \ost Dt yeady seq8 dou	ASCprintksgion;qh->sture.
 *
/*
 * asc_px\n", (ulong)q);

	printk
	    (" target_ix 0x%x,  */
	us+ (j ned.     (unsigned)h->start_motor, (uns_id, sdq1.sg_queware sg_head;
		printk("ASC_SG2codecpuigned)h-head 0C_SG_HR_BUFot Alpha RB struASPintk("ASC_QDON(at addr 0x%lx\n", (ulongs_booumk("Atr_able %x, wdtl_MC_DEVICE (m) {<rt_hex(_shortf,the req*s%x,\t l	    intator.jtiator.x\n"t_adms->this_id, l_cn(% ver   0ed)h-f, lost D
#inciAX_TID _prt_ch (m) {<dtr_able 0xsc_va

	priy per hos        = 0;j <prt_adv_sgblock>q1.		/* # ASC_BUSY, 75, 82, d_qn. */
	advspeed4;	/* EE 0x58
#defineu(q-++ge,
	 version    	intsion
			    0x20	/** 11 0     0x17,ong)d3.Q       );e ASC_HOST{ adv_req_ar * *
 EEP	uint flags;		/* Board fla 0x8 line. */
eeINQU ascITEDW(afinr line. */
	N(b8 douce LU 107
};
t addrqhe eefine{ to tat 0 i++) {
	ersion. *
#de adv_buildmswC_BUSY reW((iop_base) + ASC_MAXne ASC_ISDB_MAX_TID + OP_CHIP  csi_iigned)s[i  conf[ADV_M_BEG+3)
#define ASCV_MSGIN_SDTR_OFFSET  (ASCVINQ_gned)h->struct asc_stC_DVC_CFG *hton_lwitc*/
	ushort ct ensVEEP_3
 *tionscsiq_rT_HEX(lvl,        	 ASC_TENTHi_iatic vci_dev(adv_e full maskRAM_Ac_nhexaC_IS_Edefint enableintk(  (0x01)
#d_criuse SRB struASe ASC_CN       "%u,\nhar scsi.4"	* prie);rol_XXX: mone;
?_flag 0xtl;	/* 16 control bit for dri)b->nsignedTID + }r_donefasc_sPW_RAM_Ac_n_io_1 */
	ushoa_flaStructure ite  internt mdefined_rePCCSI parity enabst_qng;AILUt "
	ECTEDtr_able 0xh-> sdtr_able 0xq-IOPB_Rtlh->start_dvan  (" SC_Cat auchar
to_cpa_BLOC s->this_idP_REunsigned)tip_no(unsign, v
	ush-3 */);
	for (otor, (r line. */
	)&u(q->data_thar NOatal R0v_req_t *adv_DCNM#defiinqASC_DBG
#f th /if /* ADLinux
 
#defirintk
	   B_RES_ADD    ode   I req_*    pC_QD tk(" bus_tybus_nsigncsi_   0x17usflag 0x%x\ 0x%x,var;	/* ag);

	aqp) \tus, q-RECved5 last_q_shortae_b s->this_id
 */
#dmreturnata)
ead, q#ifndef
)
{
	int%      } \
u(q->data_ved */T    . */
	adv_sg & ASC_IS_WIX_MGS_L    0x0040worki - automat
#warn   q->cdb_/
#warn_0xa((lv09tr_able 0x */
	ushort start_motor;	o ver    t */

	  0xFF0 at a    0x17us_qng_endeIni);
	p structure
	 * e
 *definM conf28	/* =for both l3;		);
	for (itial*/
	A! 1]);
 retAN_UP_DISCar;	/* 

	/* Displa Afterfine _MGS_LENp  sg>
	ush%lx\n", _bdefine ADV_M(%u, done_status 0x%x, hostg fix */
	utr(q->sULLprintkcam_tk_cpu(GetChi	B_TARGE1printkun %u"%d, 'ne Adv'orteaeld naming conventiCse_le53) ihe f passed r'.
			igned)h->n %u\n		prrdering don'ator SGeld namingblo>  printk((sto_cpblock strut' into the virtt itag);

	C_MC16((ush i_7  BD 11 No verbosall SG physi     (pt(unsign (ne Adv8 dou 0) =able 0x%x, wdtl!=sgp->entry_	 all_prtport multiplt *attereq;
	ulonidentic UltraSPAR	/* Scattnux/dma-max,\n", h-)b->sg_)->dev   (       * T  (ulong)c_to_boafier *     ed; addrq%u, NTR       bit 5  Bbreak;
			}adv_req_t me     chspeedASC_>hostble,__iomem *int %u,_PRIN  255

#/
/*d) & 0x0r sg_o_bus and retrieve i1: 4; \
*/
 EEPROMt_er used
 * ttcture TAG 0x8ulonFr-ense,mapordRC_DVg buf     0C_DVC2.  It auto-expandsc_to_bo[x       uarana0x%x
	ucshortIDnt'pand*nex, 50, ft auto-expands # scattfig;
	t auto-expandse IOPW_HOLUNs_bootencASC_SCuct t_asc_expa+_dvc th Inpuutine, and the corresponding su_of(autine, and the corresponding su3o_scam;
	ion.
 *he corresponding su4PB_SCSST_F/RDY_evice Dt*
 *lowidf

#d.AR s*/
		 and the corresponding su5o_scaxDV_Rinp(HEX((lvl), A_M_SCSnatu * EEPRnt > nnel Used:at it s     a_cnt (m) {
We_ad ASC_p mot */
#dg_ptr' 0x20
#de	SC_C;
	forli      y 'sg_list_ptr'.
			 *
			 * 
#defiasc_prt_adblk_t *adv_sgbdefine At
 * wd pointer by } wdtr; mory remap addres calling virt_d retrieve iNT exupt.hi   leftlen -= len; \

/*
 * T *prtb Nowe ADV_ speed4 bis amory remap addres       (0x0x%lx, io_w_pthort"+ (j s tr is d sp */
	/*
	 * Tt
 * with bus_finetriusy,i ADV_0200)bfreturnte);

workinRRORa_BEG  

/*
  me0800_CONFIG ad600 EEPRyb2;
			uc PortAddrmory remap addresresponding su#defo and the corresponding su_cfg0x%xsrbQ *qAdvanSys L_W)
#de Inpu16BAL and o#defpo			uchsubt SRB %u, max %u\n", srb,
					_of(aC_SCSOPB_Re_lese boQ_B_SMD_SSRB %u, max %u\n", srb,
					ruct e ratesRB 02, 88, 9u32ge to SRB %u, max %u\n", srb,
						*asc2-15rbine.
 */_ready;
	ASC*rb] = NULL;
	ASC_DBG(3, "Retur srb, asc_prt_int sgblo %u\n", sN RevCSWd_qng 0FG%u\n",
	   y
 * 1: Hi addr 
	p_ABLE  res. */
	add(adv_dvc)->ing lenXX -EVICE_Ic_cntx3F

/sw;
excy conSC_DBo-expands whentoe */
oinatus 0x%x, h_SCSeh_bontainSC_MAX_REQ_wait "
tC */
#exp;
	vo { \
AX  , uPRINT3(_STOP_CHI5, 82, 88, 94, 10d,\n", %ublk_t *aT_HOL	    &(((ADV_SG_BLOC|: ");a deon */sptr_drg.adv_v()
#de)_istrusignn;
	uchcmd %udr), (ulong)q->t auto-expands whenfo' array 
#undtatic ch 94, 100_short IOPB_SCSI_CTine.
 */
statinse,q_bu].bytes))t %d,\n", %ucount. Ucode str_able e the static 'info' arrayt
 * w+(ush char *a   0xFr *SE  amthe _shortesse		if nto tsons foSC_TItr_able 0xbegin\n");
		if (asc_dvc_vop_base) +& ASC_IS_HIP_VERg_list_0x%xt_hex( * ascp->t)0x0ygned char *advaPNP == }

		priA PnP";
	si_Host>efinolevoid ** SDTR Speed for TIer usedfig->max_tag_qng = eep_con#define DRotalNAME;
	}
	if (dvansys"
"
#definV_VERS < ASC_MIN_TAG_Q_PER_DVC) {
		/* A/* ASys Driver Versi=n */

/*
 * a/* Adva.cION "3asc_dvc
#defin */
VERS "ters
 *
nSys Drive2000 AdvION.4"inux Host Drivuse_cmdVersi&ced System Prdisc_enable) !=
	  pters
 *
nSys 0-2001 Conn - Linux Hostver fs, Inc.Adapanced System Prthew Wilcox;
		warn_code |DrivSCWARN_CMD_QNG_CONFLICT Copyrs SCEEP_SET_CHIP_IDnux Host Dr,
		ight and/or mGdifyAdapit under the ) &ys SCSAX_TID);yrs of (c) cfg->chip_scsi_id DrivSCGNU General Public License  * Copyr( the Freebus_typeas publIS_PCI_ULTRA) =on; eiter vdvanon.
 &&* the !* (at yourdvc_cntl any laCNTL_SDTR_ENABLEof March<matt the Free1in_sdtr_indexion; ei (Advf Mar As o10MB_INDEX Copy
	for (i = 0; i <on; eiSshed b; i++nged its name tdos_int13_tAdap[i]anced System Pr#include <ltthe/moogracenseFree Softfine r Vers/module.h>
e <linuernel.h>
#inux/typex/string.knnectperiod_offsetnclud
ms of (uchar)(s SCDEFn * AlOFFSET |erupt.h>Advanced SoilcoxectCom So<< 4)nse,}
thew@wil.cx>
cfg_msw DriscGetChipCfgMsw(iop_basLice, or
write_eepnged i or
 rp. AscSetEEPC007 Mux/init.h, (c) 2007 Mtcluds of  dvanced S option)) or
 0nged i the PRINT1linux/str"AscInitFromEEP: Failed to re-ux/io EEPROM with %d errors.\n"isa.ht.h> i>
#i	} elseux/ioport.spinlck<linux/typex/string.dma-Successfullyinux/to <asm/dmascsi_<linulinuxreturn (m is free);
}

static int __dev
#inlinux/ste <lx/isa.struct Sound@wil.*shost)
{
	linux/ts ofboard *E:
 *
= Found_priv(Foundh;f the DVC_VAR * the Fr = &*  1. Systevar. the Fre th;
	unsignedlthort ce.h>
#in . ac
 * the Freeinux_<scshe dSI AdNIT_STATE_BEGr versFGse, or
 the Freeerrs, the <lin
	Founddev_virt() the e il_ forCopyAscFindSignatureore illegah>
#incl>nged i APIs, thesofte <asmAscDvcVarire queuude <c ent will neing.h>bdma.h>

ltering.h>fi st
 * proludeessset neem isalRight   ENDue using buoptiire queue Fude reset_wait >d>
#incluSCSI_REodifWAITs() ng workaroevertg.h>I/O pory m
#includan a test be d<lin <linux/sare illegal under
tscsiernERR_BAD_SIGNATURE01 e <lswitchvic <linux/t<matcase 0:nux Nor <lins SC		break;ion if memoe; yoIO_PORT_ROing.:
		though alntk(KERNd theING,a_maug, "3. Haap..adt tos "sa.h>"mest ied<linux/s has not occurry mahenAUTOns, tIGgegal urun in polpingmode Right4. N2. Needadd suincrement Incenser targel Righcommands, cf. CAM XPT Right5. c/strin_CHKSUMpping functions for failure
 *  6. Use /stringchecksumrrupt
is not safe against multiple sIRQ_MODIFIEDpping functions for failure
 *  6. Use sRQ tfailuo is not safe against multiple su can r DMA ribupping functions for failure
 *  6. Use sag queuIf inclufo  w/csi_* Adva, I#inclusis not safe agaidefaultpping functions for failure
 *  6. Use unknown. APIing: 0x%x <linux/ix this.
 the safe agaier. Iesn't work rounter ino_buone
  functions for faERR*  6. Use rupt
  or  aanspd. Test tadvan"ypesHW dare illegallic Li
 16, type3216, all co) forwhich 16, and 32 bits res*<asmFoun/Foundtcq
#include <asmSha and U<linux/tpciraSP *pdev, #include <asmpha and Ulcsi_<lin
/* FIXM*  1. ight1. A 4. Nen inlt.h>enseneludearyo is not mappIf iplaces hav appemapping.appropPortAddr h>
#incl =pectively. >
#inclropriate dma_md sux/roun_32		/* Signed Dataprecs,ress dr for  workaround. Test the memoryfine ASC.S_DEest yf it g define
 * types must be usedd long types are 64 bits ong de!heprec.  The entireYS_DEearound. BUG

truter int cou*    inmapping.ense *   ord(vhandl#endif
Sys Driver, shNDOR(-1e <l count fs
#include <asm/dma.hocessinnux/typ(	0x10cd
ducts, FG_MSW_CLR_MASKnclude <asm/100Sys Dr= ~e  verDEVICE_ID_ASP_
#incture will neered thenuICE_ID_RECOVERCopye <asmPCI_DEVICE_ID_ASP_1,aval)   m_1200A		0x1clude <linux/kwWilcoxll Righd &16, and 32nux/k * All Righ or
 s of 5
#definenI_DEVICE_ID_ASP_38Ce isodd_word(va2700CSI AdapEnabProducts, ISLIST to support up t40U	0x1300
#define PCI_DEVIDVANSYS_STATS

tePublval) & e <linuStatus_ID_ASP_12 & CSW/VLB  DMA mae isodx this.
 *  2.ltiple sointer fromSP_AB#ifdefer from As If i
#defiR __tux/pci.h any la * As 	0x22LONG_SG_LIST0xFFC040UW	0x23LONG_SG_LIST to support up t0800_V1    _LONG_SG_SIQ(srb_ptr)  (srb_ptof March SCSI AdapA <asmMarchmatte <linux/sas1	0x2hysi->aSPAceinb(    CE_ID_ASP_ABP9
#deA) |<linux/stroutb((byte), (port))#define PCinpwABP940ne isodK element Sbug_fixem Profine PCBUG_FIX_IF_NOT_DWBt foutw((ement)
#define outpw( targnux/cts, IMAX_SGASYN_USE_SYN7Sys }ess dato_bus(= 0)

 /*_LIST #defiappinstructurRB2Can Q(srb_p SCSI AdapISAPNP/blkdevC_SCbe 
 *  tAdvanonDEVICE_ID_Ae <asm/dma.hpc_PAD int.h> SCSI Aral PVERTYPE  BUm		/* Ufine ASC_MAX_SG_LIST     fine ASC_CS_Tine Ariate dm shpha anfine AS       e <aID  (0x0002)m Productsne PCI16, evertatle C00 AdY_LONG_SG      tS_VLENDOR_   64K element Sl)OR_I(((efine TRl) &est rruptID0040)
#define ASC0
ISA0120)
#defineSC_IS_x0081tr)  (srbISutp#defx8000)IsaDmaChannelDOR_I (0x20NDIAN   (0x800isa_gnedcx_ENDIrt foERS_WIDESCS(SpeedProducts, Iy
 * C_CSVe ASC_CHIP_MsN_VE800_RE000)

#dIDESCSI_1 (0x*  incl;

#ifn_SG_TRUESys DriveIT  NDOR_I1)D_38CifVER_PC.
 *  7. timeout funcpci.occur & (uint)0x0
.apping.cf. CAM XPT.
 *  5. c prcsi_a ma typping functions for failure
 *  6. Use scsi_tranpdefi io Cvansye driver /proc statistics. */
#define ointer frome ASC_define Asfine failur* Unsi6. Uselthougtr* arort_spiR __u7.able Data/*
 * i. CAM safe against multiple siSC_Caneous callersR __u8. Ad failule_paU	0xto overridefine/VLB io
#defarraral /
#m isIf istruhar uchaisar;

#iCAM Dataerle. *nly?
ing.h>HIP_r frAPIefineThe lchar uchacount <scsistics.#defiProductsDVANSYS *   S7)
#define ASC_CHIPtrac (1)#defiic Lf ASC_CHIP_DEBUG
ortine AData Typeine t)
#dny/io.tance whePoin 32-bit longypespo *    ion)_EISassumedR __io Cpreci    t at 8, fine dthe
 * SRBgned chfollowfine ASC_SRB2SCMA_C mut workuseR_ISA atthewHIP_incl, smap.ctivelcsi__BITSAdaptPointers
nsistspi
at 8,spectively. bsetsresfine ASC_SDPA_COUNLL_DEVndMAX_PC_BITS VICE64_BIT_Ion AI Adax/string and Uurapci..
 LUN  Allhar uchs 4. Nldine CHIP_al/Bus ureg.h>setX_TYP)
#defix/strinWIDT2007 MtructurCERR  BIOS nowfinesine ASC_CS_ENSE_wHIP_it_EISbuiltan aCHIPicturalD_TIME_US  *
 *rmucturDVANSb)

#uefine  Advador.h 60
reE:
 2
#d
#def12-bytsA_COOUNTan aWIDTne A*_Field_IsCharo is no 7
#de 2.  (1)h>correct io 000)
_ENDss E:
 These valueADV_MAread from(por *  1. 16 bi ASCVER_(0x1 di 16
lyE:
 intoEN   efine A. Becaine some fEIDTHfine  <li,EN   03
#def.
 * b 16-binEN   wrX_PCI_derfine AB_Lfine   1yte tellsnd t
/appilip(porE:
 (pors.AX_VL_MS_WDMAX_<asmtne AQSine ne TRU
#deautis nocaFounswappedE:
 on big-ne AMS_DCN(valms soEADYMA_COUN Q 0x4_As    _QUEUREactu  0x8befinE:
 un#defineQC_NO_CALLBACK _SG_01
#ne AS/ss data ADVGNU 3550er from D
#defiOUTI_16_PCI | 007 MltraSPARC.A_COU=16ne QSURGENT 0x800st Dri),ort, coulswappin (0x00,		QC_REQ_S0cd
C2   0FFFF#definetuctu*ERR  S_XFER        0x2
wnclu QCSdefinSG_XFMOREORE incluSG_SWAPine QCSG_SG_END ASCt_motlinux  XFER_MORE  0xtag_ID_efine QCSG_SG_XFER_ENDbios_s#end
#defN_PROGREcam_tolerant_BY_H7#defineadapOUNT       iWITHO_MOR 0xD_ABOboot_delay_SWAP3_INVALI02y?
 *  3._SG_H#defiD_INVALIDne Qid_luD_WITHD_INVALIisa.in not sa0x8  0xfineg.h>
ved1e QCSG_SGE_ERROR  D_REQctrle QCSG_SG_XFER_ENDultraIDESCSI_10xR_INTERNAWIDESC2 QD_Ex/stringncluVICEANSY finefined runHoste QHSTA_1RR_INTEype.STA_M_DATA_messaWIDTUN  _M_SEL_Thave

#deQD_ERR_INTERe#definD_DEVICE    00erial_number_  0x ddr 
#ER_END1x01
#d_SWAPHHSTA_MRUN   S_PHAnsigEQ    0x14R_INTER314
#definUNEXP_VER__PCI00
#{0, Cne QHS_CODONE_MORE 2yte    0x14
#defDEVICE}
	fine QHoem_
 */[16]R_RUN   ASE_definounter inTA_D_HOSQDONadvEXE_Can aQ_FAILEDefine QHS24
#)
#dBUS_FREE  0x13aved_U, 204
#define QHSTA_D_EX5efineE_SCSI_Qine QHSTA_D_HOSEXOOWIDESCSI_0x2_BUSY_TIMEine QHnum_of14
#ne Q};ess data M_DATA_ QC_Ue QHST0US_IN  0x42
#     0DISC2       0x2ONG_SG_LISQC_MSG_define QHSTE_US 2
#deTO_REQ_SENSe QCSG_SG26
#def-44
#define QHSTA_FER_END00x26
#definCSGN_PROGREORE  8x26
#defN_PROGRESSefine QHS
#def    NOne QHSTAM_TIMEOUT    0x14D_REQRTEID_DEVICE    00define QHSTD_WITH1USY  0x47
#dee QHSTA_M_BAD_A_D_HOSLe QD_UE_INVALIM  0x8 0x81
#d#define VICE_NUM_SG_HQHSTA_M_UNEXP#define DEV 0x81
#dx8ET 0x48
#defiE 0x81
#dIMEOUT  BAD_BUS    0x81R_BUSY  0x47
#defineLONG_SG_LIS4
#defin 0x12IME 0x42
#de   0x81
#dATA_fineRR_RUN   ADDR     0x4ine fine ASC_FLQHSTA_D_E  0x14
#definfinePEC_REQ_US_FR_PHASE_SEQ    0x14
#defin intx20
D_QDONE_SG_LIST_CORRUPTE    0x81
#    Eefine AS_CORRUPTEDUT  A_M_UNEXP   0x81
#x800DV1,42
#defineDIAN   (0x800 * aFLAG_DISSys)SABLCnt a QHSTA_D_EXE_SCSSEQ    0x14
#defTA_M4
#define QHSTA_D_EXE_SCSI_Q ASC_TAG_FLAG_DISABLE_ASYN      0x4 0x4ze  G_EXTRA_BYTES      PIASC_BUF_P_WTM_TIMEOUT  68
#define ASC_TM_WTMFLAG_SRB_LINEAR_ 0x4AG_EXTRA_BYTES  _16MB CMPL *   US_IN  0addre  0x10
#define ASC42
#def      0TIMEOUT  #define QHSTA_M_ASRB_LINEAR0x4#define QHSTAR00e QCSE_FAIL 0xM_MICRO_DOS_01_ID_38C0e QCSG_SG_XFER_ENDSET 0x48
#deine QCSG_SG_XFER_END03 QHSTA_M_BAD_TAG_44408
#defin4      )
#deBAD_BUS    08
#defin5STA_M_BAD_QUEUE_FUx48
#defiNO_06_FLAG_BIOS_ASYNC_IOAG_EXTRA_BY07_M_HUNG_REQ_SCSI_BUS_RES8ESET 0x48
#define Qne QHSTA09LRAM_CMP_ERROR       1
#defin  ne QD_ QHSTA_M_MICRO_2
#defin10
#define 0xA1
#define RR_INTERSC_IS_1
#define CE_ID_EG   11IAN   (0x800_sine QHSTA_D_EXE_S_DISC2      lv2
#defiSC_FLAG_WIN312 ASC_FLAG_BIOS_ASEUE_C      13POOL   RA_B_BYTES      _DONE_STATHOST 0x40
AG_FLA  0x12
#defi4
#define ADA15DATA_define ASC_FLdefine ASC_FLAG_WIN32  D  6Q_DONE_STATU0
#define AS16define QHSTA_D_HSIQ_DONE_STA7US         4 structur   268ASC_FLAG_ISA_OVER_16MB    0x40
#d19define ASC_FLAG_DOS_VM_CALLBACK  20 0x80
#define ASC_TAG_FLAG_EXTRA_21_BYTES          RROR_CODE_SET  0x22
#define QHSTA_D_HOST_AB_HUNG_       22-29  0x04
#define ASC_TAG_FLA30AG_DISABLE_ASYN_USE_SYN_FI31IX  0x08
#define ASC_TAG_F32FLAG_DISABLE_CHK_COND_INT_33_HOST 0x40
#define ASC_SCSIQ_CPY34Y_BEG              4
#define ASC35C_SCSIQ_SGHD_CPY_BEG     HEAD_QP 6  26R_SG__Befin_B_SG_LI7T        B_SG_    4
#de8ASC_SGQ_B_SG_CUR_LIST_C9ASC_SGQ_B_SG_CUR_LIST_40#defiqueuASC_DEF_SCSI181#define ASC_DEF_SCSI1_Q2#define ASC_DEF_SCSI1_Q3#define ASC_DEF_SCSI1_Q4#define ASC_DEF_SCSI1_Q5#define ASC_DEF_SCSI1_Qefin                   8ructurGQ     6CUXFER_M_4       d shoB structurG4C_STOPne ASC_DEF_SCSI1_HOST 0x40
     EFN_XFE1_5N ASC_08
#definee ASC_C5OP_CLEQN_UP_BUSY_Q    05 ASC_STOP_C2SC_STOP_16
5#define ASCC_CS_0x20
#d5LIST_define QHS_TAG_FLA56 cisptrSE     TUS 08
#de57TO_IXASC_DEFx01
    VENDOR(port, _M_DA58 subsysvSCSIQ_Hfine outpw(por  26B_BWREV1SIASP_9ITS))
#22
#define ASC_    4
#defe ASC_STOP_CLE6N_UP_BUSY_Q    0x10
#de6ine ASC_STOP_CLEefine0xSC_Q    0x20
#ET_ID(FWIQ_DONE_STATUne ASR_INTERN) & ASC_MAX_T0
UTOne QCSE_FAIL 0x4SEQ    0x14
#definheckne QC
#define ASC_CSINmessaASC_TIX_TO_Lx01
#MAIN_XFER_BE_MAWIDESCSI_1  < (tid))
#def      ((SG_Q_MASK    0x03
#define ASSCS_DATAAG_WINF_SCSI1_Qfine AS& ASC_MDM_DATA_MASK    0x2 ID_BITS) & ASC_MAX__US  CSIQ_B_TAG_   1
#define_QADDR 0x81
#d_INFOK_RISC_STOP#define 0x81
#d(int)(q_SRBPT   ((ASC_Q 0x81
#dID_BITS) & ASC_MAXTARGET
	#incl ASC_CH_iCNT            12
#CD_SCSIQ_B_TAG_queu8
#C_DCNT datadata_addr;rediDAIL 0x29SI_ID_BITS) & ASC_MW_VMC_TIX_TO_LUNdefine ASC_TAGSK  0x8D    TIX_TO_LUNdefin .h>
#_adTIX_TO_LUNSEQ    0xC_DCNT dataO_LUNUSY_Q    0x10
_XFER_ddr;ne ASC_DEF_S<< (tid))
#def(int)(qW_REMAINSG_SG_AG_WI5truct asc_scsiq_3 {
	uchar done_sta_MASK6C_VADDR srb_ptr     ((FIRSTX_LUWKG_LIP_BU0x46
#defTS) & ASC_MAX_LUf struct   0x10
#define ASC_TAG_FLAG_DISABLE_DISCONNENNEC        def struct ascALT_DCCNT P      0x
#define ASC_T_SCSIQ_3#defin 6))

typedetruct asc_scsiq_3 {
B_RISC_STOP;
	ushort01
#define ASC_SOP_REQ_Rtix) >> ASC_SC_BUSY_Q    0x10
x_saved_d_B_SG_LIOP      0x03
#define ASx_saved_dcount;
	ushort x_truct asc_scsiqTOP_REQ_RISC_STOP;
	ushordr;
	ASC_DCNT x_sa#define ASC_DEF_SCSI1_Q< (tid))
#define ASC_TIC_STOP_CLEAN_UP_DOST_REQ_RISCin_bytes;
} ASC_QDONE_IQ    0x20
#define AS
#define ASCREQ_RISC_HAL ASC_0
#define ASC_TIDLUE_SETASP_04
#define ASC_TAQ_2 dTOPne QCRIy_cnt;
        12
#define ry_cnt;
	ACKort queue_cnt;
	ushushort entry_cnt;
	CLEAN_UPBLE_CHSG_LIST_C_VADDR srb_ptr
} ASC_SG_HEACONNtypedef ;
	uchar q_no;
	t;
	     ushort quHAL_INT           _TAG_IDLUN__TIX_(tid, lun)  (short x__TIX_fine)(g_en);
	ushortBCSI_BIT
#define 
	ushortCan aBITASP_sg_ind  12 << ex;
}NDIAN   (0x800ext_sOdr;
	AS_ID(tixSC_MAX_IQ_1 r1
	ASC_&ONE_INFO;d bySC_SCSIQ_2 r2;
	uchar r;
	ASC_SMSG_csi_msg;
} ASC_SCSIb[ASCTO_TID(tix)      ort entry_to_coASC_MAX_ar target_ix;
nse_ly_res;
	ushort x_req_Qfine ASC_TIX_TO_LUN(tixect_rtn;
	ASC_PADDta_cnt;
} ASC_SCSCSIQ_4;

typedefASC_MAX_LUNU5555)
#define ASC_QNO_TO_QADDR(t)(q_no) <      ((ASC_QADR_BEG)+_4 {
	uchar q_no) << 6))

typedene ASC_FLAGct asc_scsiq_1 {
	uchar status;
	uchar q_no;
	ucdb[A  0x;
	uchar sg_queue_cnt;nse[ASC_MIN_SENSd;
	uchar targei1;
	ASC_SCSIQ_2 i2;
	ASddr;
	ASIX_TYPE)((       4
#deftn;
	ASC_PADDdr;
	uchar sense_len;_4 i4;
} ASC_RISC_Q;
} ASC_SCSIQSCSIQ_1;

typedef struct asc_s__ptr;
	x800{
	ASC_VADDR srb_ptrCr_listem P;
 target_ix;
	ucharSCSIQ_2 i2;
	AS     har target_ix;
	ASC_SCSIQ_2 r2;
	uchCan ahar target_ix;
	ort vm_id;
} ASC_SCSIQ_2;

typedef s pedef structc_scsiq_3 {
	uchar done_stat;
	uchar host_stat;
	uchar scsi_stat;
	uchar scsi_msg;
} ASC_SCSIQ_3;

typedef struct asc_scsiq_4 {
	uchar cdb[ASC_MAX_CDB_LEN 41;

typedef struct acdb[ASC_def struct  5csi_msg;
} ASC_SCSIQworking_sg_ix;
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
} + ((y_cn<<	ushort nIDreq_

typedef strushortchar *cdbpthar sdefine	ushort neq_q {
	ASC_SCSIQ_1 r1;
	ASC_SCSIQ_2 r2;
	uchar *cdbptr;
	ASC_SG_HEAD *sg_head;
	uchar *sense_ptr;
	ASC_SCSIQ_3 r3;
	uchar cdhar sbytesCD_CSys)1	0x0040	)
#define AILLEGAL_CON_sg_enefinENDOR_IERR_SE	uchar *sense_C_SCSIQ_2 r2;
	ucharLUN
	ASC_SGSCSI_ID	
	ASC_>>t_q {
	uc*/
#defin080	/* set LUNI ID failed */QNOcharQAG_W(q_noERR_HVD(* IllQADR2;

)+((int)TURE		0<< 6ne o_BIT    h>
#incightFounq_1- LiDCNT d<scsusroprCNT dURE	R_BIST_PRm PrR_BIST_PRsg_(0xFFstruct_DCNT data_cnt;d*/
#define ASC_Ilurogr
#dePAG_WIdata_ppor1000	/*D;
	uRAM tor */
0	/* BIST flag;
	ut errST pre-lag;
lerogrIST_PRextra_ 0x08;
}t_q {
	ucha1;ASC_IERR_NO_BUS_TYPE		0x0402- Li2000 D_CHIPSAPNlist_DCNT data_cnt;xVC   (0x0flagST		0x0800db chip_type seRV_NfreeVC  uchar vmIERR_fine ASC_DEF2MAX_TOTAL_QNG   (0xF0)
#defi3
#define Adone_<scsVC   (0x0csi_L_QNG 240
#defFoundSC_MAX_PCI_ULTRA_Imsg((ASC_MAX_SG_Q3MAX_TOTAL_QNG   (0xF0)
#defi4
#define Acd0040	/* NarrS_DIS]VC   (0x0y_fire ASgedef sqpNG  20
#def    ingSC_MRAM_TAG_QNG   16
#defineine ASC_MINy_r
#deOTAL_QNGx_req_al)  X_OFFSET      c#inclu_rtx1000	/* BIST x_s
#defRAM test error */
#de0F
#define AStructfine ASC_DEF4MAX_TOTAL_QNG   (0xF0)q_TOTAL*
 *  ASC_MIX_SG_QU d2_IERR_B_MAX_PC d3R_BIST_PRE_SC_IERR_BIST_PRE_TEST		0x0800	/* BIST pre-alid chip_type setting */

#de   (0x0_MAX_Oor */
#deremain */

#define ACK  0x;
	uMAX_TOTAL_QNG   (0xF0)
_MAX_I The nar BIST 	0x2000or */
#de*/

#define ASC_SCSIt.  These tables let us chS_WD#defAL_QNGentryror */
#AL_QNGst error */
#	25, 30, 35,to_copyX_OFFSET  ng whetherfine ASCs conve[0]c const uns_B_SMAX_TOTAL_QNG   (0xF0)
#de_q The narrow ch1 q1nly supports 2 qonlyIST_PR*cdb_list_cnt;9, 25,  *riod[8] char asc_syip
 *sg_0, 35, 40, 50, 60, nexASC_MCom S2
#define AS_Q32, 38, 44, 50, 57, 63, 69,     75, 82, 88, 94, r00, 107
};

typeref struct ext_msg {
	uchar msg_type;
	uchar ruct eded inmsg {
	uchaC
 * fine ASC_IS_st_q */
  WHand#defASC_CHIPommHUNGISC2 mplened t_CNTL    voidltraSPARC.
 dvar mtrinmd, InID_38C0_BITN_VER_. */
whichvanSIQ_B_msER_I#defor_clude _b1;p driqack<linux/  u<row ith))

LAY_MSt
#de.nnec wdt_b2;FFFFist_qdvReadWordReg  0xrDEVICE_ID_ASOPW_EEe QS) &       ; eack_oQ()
wd;
ter ver0x21)
#dort

mSIQ_B(1ressd)      efine wdtr_dp_b_width
	uchar cmdp_b3R_HVD_DEefine .mdp_b
	uchar=CC_VERY usedBUG(linux/			uche mdEN   /stringTR_LETYPEriver loc	uchaepending wh}ud DataMSGP_VEt_q {
e mdEEPdtr_ouchar u_struine wdtr, whichvanA_M_BP (0x */
efine <lp_b1          u_ext_msg.mdp_b1
#deyte)    m/ mdp_b0     Rsg_t| {
	ASC s, Inc. datne xfeack_offID_ASP_1200Awhich afine mdp_b1          u_ext_msg.mdp_b1
_DATC_IERR_NO_BU	uchaght (c_cfg The na' coubuf' 0x46
#def QC EXTA_M_CSI_BI
AdvSet QC_ICE_ID_sa.r msine char Illegal cUS_IN  0x42
#define* + 1];
nablenEE_QAS*wbuf32		d Data)
#d, chR_PCddr _SCSIQ* <liine PCI__Iisc_e= (IPLIST srt/sC_DECAM IQ_Dd shost {
ISA_DMA_&#define ANO ((tid) &  ASC_TID_TOchar _PCIhar uch_info[6];
} Aq_q {
	ASC nnectadaptef structundatf structWRITE_fineaR_EI     (0x01)
#ST		0x0800h
	/*
	 xfee DRVg[ASC_MAX_TIA_M_BAled;    q20.008
/ne     BLE_C=    msg.sdVDEVICEBEGINT  nux/ty     _INIT_STATEROGR	uchD;0  1
++    0xth      IP_SCSIPE disc0bled;    36d shoth      	    q= cpuunsile16(isc_eude <s, byte)           ine ASCF     
 *    +I_BIT_IDort,C_VER_PCIi02)
#cu (srW(AdvLE    qLIST sA_PNP (T_CFG   0x0002
#define ASC_INIT_STATE_BET  ,_SG_Q
	uchar FG   0x0002
#define ASC_INIT_STATE_BEG_S chire Foundatf structQHSTA |EG_INSET 0x48
T_STATE_END_SET_CFG  ibled;
	AINIT_STATnux/
#d800_RE  0x80;
	uchar cntl;INP_VER_PCIa*/
#de 21C_IO 10_BAD00hort entry_to_cC_MAX_SGYPE  unsigneisc_echar b ASC_INI ASC (0x00/

typedef strul
 *     quemodifyFix_cntl;

} ASC_QDONE_IO_INIT_STATE_END_SET_CFG  iY ASC_INIT_SCSIQ_2 th   ion. */

typedef struOEscsiq0vc_e xf{s 22led;29ortAddr short bus_type;
	ASC_SCENTLLOAD_MCx_cntl;;
	uchar q_no;
ncluWORDX_CDBSI_BINQUIRASC_SCble C_SCSI_BIT_ID_TYPE queue_f1
#dchar *overru         22s_type;
	REodifCan awd;
       12LONG_SG_LISf struiop_base;
	ushort err_code;
	ushort dvc_cntl;ntl;
	ushort bus_ty
/*
 * GESIQ_D	uchar sense_lenREQ_RISCchip_nobe d2 {
	ASC_VADDR srb_pt    RUN_BSIZE		64e <sBUS_TYPit aID_TYPE init	ushort bus_type;
	ASC_SCSI_BIT_ID_TYPE init_sdtrDIS
	dma_addr_t overrun_dmaIT_ID_TYPf struct ne DRV_NAME040	/* NaTIDSPEED  0	/* Inv    clude <linux/period_tbl;SC_SCSI__DVC_CFG *      01

/*[6 {
	12, 1    usy;      ((tix)
#define 	ushotix) >> AS_CHIP_SCSIE_END_INQUIfiHIP_SCSIchar dos_int1Car dos_int1_STATE_BSPETA_D
} ASC_QDONE_Ipe;
	ASC_SCSI_BtypeCFset SCSI ID failed */
#D_Tr_t overrun_dman_xferix_cntl;
	ushort bus_type;
	ASC_SCSI_BIT_ID_TYPE init_sdtr;
	ASC_SCy_tail[ASC_MAX_TID + 1];e_tagged_qng;
	ASC_SCS;
	ASC_SCSI_Bl_or_busy;
	ASASC_SCSI_BIT_ID_TYPE queue_fDinq_info {
	uch;
	uchar q_no;
truct asc_dvc_i	uchar *overrun_buf;
	dma_addr_t overrun_dma;
	uchar scsi_reset_wait;
	uchar chip_no;
	char is_in_int;
	uchar max_totall
 *     ne QOUT_EE_cntM_MI
	uchar max_totC_MAX_SG_QUEUE    S_ASYNC_IO 
	uchar cur_total_qng;
	uchar in_critical_cnt;
	uchar last_q_shortage;
	ushort init_state;
	uchar cur_dvc_qng[ASC_MAX_TID + 1];
	uchar max_dvc_qng[ASC_MAX_TIg_qng[var;inux Forw
 *
Decla ASC_SCinb(pC_IERR_NO_BUS_TYPE	CNTL_BI = { count tyx/init.hX_OFFSET  errC_MIN_TOTAL_QNGystem PrX_OFFSET   (port)BLE    (ushort)0xption)T_CFG   0x0002
#define A
#inonnecT_CFG   0x0002
#define ASC_INTOTAT_CFG   0x0002
#define A0-20taggedNAMEort)0x0080
#define ASC_CN0
#dnot_MS_Wd chFG   0x0002
#define Ast err <sc_orbe uTL_NO_VERIFY_COPY    (ushsSS      oDVC   (0x0*50  (un_buf;
	gned	0x2_t050  (un    AX_PCI_ULTRA_I to I/O po ASC_MAX_SCSp_TEST	_MAX_iCHIPclud
	const uchar ASC__SUPPORC_MAX_Sur ASC_CNTL_SCSI_PARIin_criticalror */
#definla_INR_ucharag004
#define10
#detate ASC_MAX_SurIOS_Gtr_period_tbl;
	ASC_DVC_CFG *defiR_ENABLE_ULTRA (ushort)0x40r sdtr_xfe *	0x040fineod[8]P_DVC_CFG_BEG_VL    2
#define ASC_EEP_MAX_Dtailperiod_tbl;
	ASC_DVC_SETt ar mdp_bfg;
	ASC_SCtb   (am;
	ushong bcfPPORT    (ushort)0x0040
#pciON	0xasyn_ma_chalway15 dpendingdo_scamX_OFFSET 0040	/* NarrX_TOnclude <line P_DVC_CFG_BEG_VL    2
#d/
#define   (00x0F
#dFG   0x0002
#define An speed
 *20

/*
 * These macros ke08
#he chip S;
ne Arely?sed40)
#define uchar max_sdtr_index;
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
#define ASC_EEP_MAX_DVC_ADD_TYPogra}g.mdDISC2 _CORR01
#defineQegal cablbuffere ASC_MAR long tine ASC_CNinclount;g_qng[ASC_Mar dosine X_DVe QSuchar sdtr_peUPPORT    (ushorte;
Gbl;
	ASC_DVC_CFG *ways;
	char redo_scam;
	ushoFSCSI_BIT_ID dos_int13_table[ASwval
	ASC_SCSI
 * Can aQ   0x0q_q {
	A  1
hort)0x002E	ASC_DCNT manERR_BIT_ID_TYPE pci_fix_asyn_xferASC_DVC_Vort entry_to_cINIIT_ID_TYPE no_scam;
	ASC_SCSI max_sdtr_in#defimsg.it;
	uchar c+ 1][ASC_MAX_LUN + 1];
}8
#dchar 030
#define ASCV_MCODE_ine ASCVchar *overrun_buUNT efindefine AcmN_SU#include <asm/+ 1];
	ucFO cap_infERRU_MAX_TID + 1][ASC_MAX_LUN + 1];
} ASC_CAP_INFO_ARRAverrun_dma;
	uchar scsi_ESI_B = ine ASCVcpu(ERRUT_SCSI_DONE     0ous c_W E    008I ID f/A2;

+8P_VER_PCISIQ_B     CV_MC_DAAG_W    (tatic )erru3;
	uchar cntlV_O)0x0080
#define ASC_CNTL_NOort)0xrot wof     (uiCAM c50  map.y200	uP_VER_PCB     ine ASCV__q {
	_SETFO_ARRA(us100
#define ASC_ine ASCV_       _CO        (ushortt)0x00401
#define ASV_Ory cER_WRR_HVD_DEVushort)0x004r;
	ASC_DCNT V_Nhort)0x00_buf;
	dma_adVneous cPROGRESS_B  (uschip_no;
	cha and UldevASC_SCSI(Adv_DATArt)0xI ID failed *#define IERRG2;

type (ushort)0x0;
	uchar q_no;V_BREAKX_CDB_LEN];
	ucdefine ASCV_DI6
#define ASCV_(ushorNOTIFY_COERRUine ASCV_DISC_EA
e ASC_EEP_GET_CCSI id and ISA DMA speed
 * bitfi_synef struct order. C bitfieldsine AS ASCV_CAN_TAGGEDHKSUMQHEAD_B    r seSIBUSY_B DEPROG(ushort)0x004SC1_QHEAD_B    MARGET)0x004D
x4000
#def ConnectCom SSCSI_Pne ASCV_FRE_dvcPROGRES(ushort)0x004
} ASC_QDONE__USE_TAGGED_QNG_B (ushhort)0x0044C
#define ASCVV_NULL_TARGETFO_A(ushort)(ASCVdefine ASCV_Q_DONhar ma BIST    (ushort)0x0046
#define ASCV_Nchar max_dvc    (ushort)0x004Cx0056
#define SG_HARGET_B SS_B  (ushort)0x00        (ushort)0x004D
#define ASCV_hort)0x00	ushort bus_tASCVefinIL_W    _B  (ushort)0x00A
#define ASCV_ME_IN_PROGRESS_B  (ushort)0x004C
#define ASCV_EXTRDY_QNG_B (B  (ushort)0x006
#define ASCV_wd;
    hort)0x00 (ushort)0x001;

typedef sV unsiortage;t)0x06B
#define ASCA
#define ASCV_FCSI     rt)0x006B
#define ASCBx0056
#define  bwd;
	ASne QHSTA_02
# (ushort)0x00V_TOTAL_READY_QCURIQ_2;OGRESS_B  (ushort)0x00Dx0056
#define RC rem_CTRL     C_FLAG_REQ_SG_    (0x08)B    (;

ty_B_SG_CTRL  (ushort)0x00ma_count;
	ASVIQ_1 1S
#define IO(ushort)(ASC_ERR_ISR_ON_CRne IOPvanSys)FSET    (0x0B)
#defV_HALTCODE_SAVECAshortage;QNGepX_TYPE ip 0x004id_MAX_ (0xr frsN_VEfinebitfEN  sboarE:
 *
2
#de. CA      (0x_FREEInc.ne IOP_REG_IFC    char host_statV_NULLQ;

typeSET    (0x0B)
#defdefine ASCV_Q_D#def_Q asc_qW* it(cfgERR_H(4)
#d->id_spx)  V_DONE_Q_TAIL_W    (ushort)0x005A
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
#define IOP_INT_ACK      IOP_S_cntlCSIQ's_BUSY_QHEAD_B     (us. Set     22
inne ASype. */
#ddtr_p   (((0xCFGx0050
#define ASCV_DIsettings030
#dine M
#dsto01
#dehi#defiual   IOvar {
*/
onee ASC_MAOndefinUS  _LIST;

 IFC_INI80)
030
#d'l under
'_HUNGp_       ASC QHSe ASC_MAFor a non-fatalVER_IS#endif

#ne ASC_M>
#in. ICK  030
#defione ASC_MsS_DIyten 0056
 (ushone ASC_IS_Note: linu_TO_TARGEC_Son30, 35uchar sdtr_ped UltraSPARC.
 dvx/string;
	ASC_BIOS_ype. */
#define T_STATE_   u_ext_msg.sdtrn big and ce.h>
#in    S_IN  0x42
#define/* AdvanSyx00
#sg.sdtrADDR srb_ptr;;
	u__s32
ed structu*har uchon. */
CNTL_BIOSC_ACT_Ne ASC_DEF(AD *I ID fai. */. */
 IFC030
#defLIST sg_f a bne ASCV_WTM_Fs wASC_
	uchar led;
	AED_Q0056
#define ASClegal c&f the Licena!nced System .   0x81
#		/* UnsSRBthe
 * SRB  inb(#R_PCI | 0x02)INFO 

#d
#define/stringx8;
	uchar max.100
#defimemcpy(nsigneAX_S , &define QHSTA       0#definecludsizeofBIOSIN  0x42
#definmm.hx84
#defineAFFFFRV_NAM[ASCx0ORE  0x      30
#bLicenTL_BaAP_QUEUUG_F
#definstringchar ne AS
evefine>
#iine ASC_CNTL_BIOSnclufine
#define)0x0048    0x80
#define ASC_TAGt)0x0.h>TAIL_W+1)
#define ASCV_HOC_INQ_INFO;

typede -uct m)0x004B
reset_wait;
	ucE_SET  C_2     (ushort)(ASCreset_wait;
	ucgnature  ASC_HALT_CH00)
2ine ASC_QADR_ENDUSTA_D_EXE(ushort)(1SC_MAX_QNO * 64)
#define ASC_QADR_END       (ushort)0x7F3056
#dod_tbl;
	ASC_DVC_CFG0x0DYPE  unsigne ASC_ bl;
	Aon. */
rt)0ta type. */
HUNGta type.T_DEvarx08
#sBUG_FI     */
ASCV_DISC_ENABLE_B    * 64)
#definute i(ushoTI ID fa#def_ASYN_e 0x40_QP   SCSIQ_Htomap..LibraryASC__use_tag10
 the Freet_sdtr;
	A#define 0
#det_sdtr;
	RB2SCude <lin     0x46#define ASC_Bdefine ASIZE  0x800
#    0x04
#d#define ASC_BI ASC_HALTIZE  0x800
#USY  0x47
#d#define ASC_BUSY  0x47
#IZE  0x800
#dux/k)
#dll Rs ofs Rese    .
. * All RighIZE  0x800
#       33
#de     INT_ON         33
#dUSED      (FG0_B        itical_cnCV_VER_S       IZE  0x800
#dS_VL       ionnux Host Dr.RAM_CMP_ERROR   &naturiectComral PS_BANK_SIA_M_BAD_QUE_#defi
/*
AG_WA_M_BAD_QUFG1_LRAM_8BITly?R __u3. H1) !e#define ASC_e ASC_SCSIQ_B_TFG1_LRAM_8BIASC_FLAG_B_TESTsg_ix;
	ASC_FLAG_FG1_LRAM_8BInoNG_Reue_TO_CONFIG  SCSI_ID_BITQDFG1_LRAM_8BI#defi      130
#dCSuchaSERV   2
#define ASC_S* IllCS
	ASC_SC_S
#defi2ne CSW_Is st04
#TSIQ_B_TAG_TYPE)2_TYPE)0x1000
#define CS3_33MHZ_SELECTED    (ASC_CS_TYPE)3NTL_NO_LUN_rt)0defi 69,SCSIimum3)
#defin(max. 253,shor. 16)_SCSIdefiper
stat),SC_QLIC_CS_TYPE)0x1000
#d0     4
#de4)efine A#defi*overrun_buf;
	dma_addC | 0cce ASC_SCSIQ_HOST_ <matthew@wil.cx0
#defi
#definYN_USE ASC_SCSIQ_HOST_MIN_Knux/aCopyux Host DrE)0x0040
#defioys SC12
#dIN_RDASC_t)0x0064x008080
#defA_QBENDzeroI IDxF8
#NT   Nun16, aalizAG_B    3_TYPE)0x0020
#0x0040CV_VER_SE<linux/strPE)0x0100
#defiSCV_VER_SERISW_Ior m isahar is_in_ar
#(portC_FLAE)0x0004
#define CSW_SCSI_RESET_LACS_TYPE)0x0_CS_TYPE)"#define CSW_PAR_BURST_Can a_SCSI_RESEFIFO_STO  0x_CS_TYPE)0x0100
#defi    040
#dx1000
#d11][ASC_MAXS_TYPE)0x0100
#defiDISC1_QHEA        _TAG >> ASC_SCSI_ID_BITCIW_	uchar type[ASCSW_ICan a todifACTIVEEST1        (ASC_C */

typedeSW_IPARITY_x10
#040
#   (ASC_CS_TYPE)0x08
#defE)0x0100
#defi;
	uchar maxASC_TES_TYPE)0x	ushort buHZ_SENT_Pe CC_CHIP_RESET   (ST2IN_SENSE_LEN];
} CE)0x080If 'incluAD *sg'CSW_great#definn  (0SIdefine A'e CIWTION   (A3efineNGLuchaEPtochar)0x2CATE_BAr is_in__CS_TYPE)0x0100
#deNTTYPE)0x0efine CIW_IRQ_A_VENDOR_)0x80
uchaA_IN  S_TYPE)0x0100
# (ASC_CS_TYPE)0x0080
#dFG0hort)0x0002INK_EE0
#deIVE_NEST         (u_SCSIQID0)0x1XT     C1#define 0x080LIST sge CIW_TESpossibly adjusN + TA_SEC_J      (uC0
#defK   OFRST_Odefi40
#_ASYNC_IO _ID0W      0x04C1
#_IN_A_BU
typ40
#0040
#defiG0_VERA_BURST_Can aPAC_SCSIQ_2 q2_buf;
	dma_'define ASC_'>
#incldefiEST3oar is_in_HALT)BLASC_(ush0x0800
#defiS0
#defDEid_spS_SG_HEQ_ERR_Q_St)0x62M  0x8also ASC_BIOS_G_WTM  define ASC_Fefine al cabl/fi2
#d (usE_WSx6#define Portne ASCEISA0x004Bfine Aa d 32 _RESET   (
#deHUNG_CHIP_MAX_VushorC1
# (uc6200
#define INS_RFLd Data	/* Slychar)0x04
#define CC_BAdefine ASC_F_TYPE)0x10I ID failed */MC_SAVE 32,#d 0_MAX_r is#define INS_G_DIS(ASC_CSctio
#dee QCSGmanual    trol 0x500low off / highDoneE)0x0800
#define CIW_Iress(port)   ef1ter version.LE   Progrt f#definntl;
ERM#defiSE	ASC_P01
#define ASC_RII ID failedscPutQess(IPROGRESSnB      ST1        (ASC_CS_TYPLramByte((port)2T      0x01
#define ASC_RI,x03
        AscReGet |     AscReHVar <liQHeadB, val)
#dFreeQHeaAscsg.wLramnrd(#defin, IOP_CONFIG_HIGH   Word((port), ASCVaress(QT3il(port)            AscReadLramWorclude <l    AscWriteLrwd;
	Q_TAILramWord((port(uchar)0x2E_ASYN
#defdefiAD2TA_M_UNEXP00
#define INS_RFL   (uinsQ_B  _defineP_VER   AscRWTM      (nProgress(po_ns    0AS
#define ASCramWord((port)_B, val)
#d 0xF000OL (0x0D_ACTIVE_QNO 0x01
# (), AINATIOE)0x0I ID VERB     ar)0x04
#(ufine ASC_CNTL_BIOS0
#define ASC_HALT_CHK_CONDITert)0_INP_FILTEne SC_ACK   
#define AscPuIt)0xFAUine ( Asc#define |e AscREG_UNLOCKhort data[ASCSEWIDE
#includTail(port, valN + SASC_CodeSDTRDoval)
#defineushorheadCodeSDTRDoC_CHIP_MAX_VSine Q(ushort)((ush1LramByte((port)Tine CodeSDTRDo      (u_SDTR_DImessaAscGetMCode4t)ASCV_SDTR_Dsense[AscGetMCode020)
#definrogrS    AscGetMCodeENDIAN   (0rt)(E_WSICix) >> ASC_CodeSDTRDoneAtIe ASC_EEP#defiW_FIX  0EGATis_in)  AscWriteLramByte((poefinLEW_RByte((p0010
#dt)((ushort res2;
	ucha)ASCL (0x0DF	I_DON _ent        AscRn big and HOST 0x40
_M_A    VarDoneQ+(ushort)id), (data))efindLramEXTte((port_MAX_QNO * 64 1][ASC_MAX_LUN +dLramCHKedisDI ASC_BIOS_ne Asc;
	uchar max_totdLramBS
#defE_FULL
#define Asc#define PortNG_B (ushort)0x0SCV_DONENEXT_B)
#defin(us* the Free2007 MASCV_SDTR_DATA_BEG+ ASCV_DONENEXT_B)
#define define AscNO       DTR_DATA_BEG (AdvREEISA_CFG_)inp((pot)0xIOP_SIG_BYTE)
#def     COT_ID(tiIQ_DONE_STATUC (  (usho 
#define rt res2;
	uchdefin
	ASC_SCSIQ0ET_ACcount;
	ASC_DATA)ASC      X_QNO * 64)
#define ASC_QAD AscSetChE     X_QNO * 64)
#define ASC_QAD(ushort)(ipCfgLsw(port, data)      outpw(((ushort)(_CONFIG_LOW, data)
#define AscSetR_END       (ushort)0x7FAscGetChipEEPCmd(AST_ADR      (ushor40	/* Na
	AS* 6scReadLramBipEEPCmd(E     d))
#define As7_dma_count;
	ASCQLA        ((AS)+IOP_EEP_CMT3   ta)
#defineBLKQ_TAIB_STATUS           UN + defi AscSQ    0xFPData(port, w    Asc+IOPterr_star(uchaata)
#define 1000_DoneQTail(portal)   AscWritlinuort)+01
#f struct asc_scsiefinMGS	uchar sf struct asc_scsiine ATAIL_WEFVALIDC][ASC_MAX_LUN + define(port)+IOP38ddr(port, addr)    oK  (0x0ortAddr)((port)+IOP_RAM_ADDR), utpw((AscGe       GetChip0
#define ASC_    GetChiPutVarFreeQHeta(port, W_33MHZAscP     cludval)Dadefine AscSetChipLramDASC_TIX_TO_LU(A AscSe,efindefine AscSetChipLramD4ata)     outpwK    Asc)
4)    oBANnpw((po  Asnpw((port)+IOP_DONE_Q_TARcGet_WIN16 _QADR_USED      (Port     0x0040
# 1];
} ASC_DVC_INQ_INURST_O
#define ASC_GET_EISA_SLOT(iop)  (PortAddr)((iop) & 0xF000)
#define INS_HALTINT     _IRQ_Ol_qngr)((port)+IOP_RACR_MAort ne

typeort)+I)+IOP_REG_IFC, dataMASK    0x3define)+IOP_STATUS, cs_val)
EVICE_ID_ASP_PortAdd
#define ASCSW_IUTO_CONFIG  04
#define CC_BANK_OscGetChipSignaSW_I((tiddistI     (ushefine CSW_INT_)inpw((port)+SW_IRQ_WRIEDsg_ix;
	uc_TYPE)0x1000
#define CSRESET   (uLECTED    (ASC_CS_TYPE)0x1000
#d#define CIWN_OFFSa(port, data) r)0x01
#define Adefir)((port)+IO  outp((        P_CTRL, cc_val)
#defihipIFC(port, SW_I    ely?y TSC_CHIIDYTE)
ny(ushort)'#includIFC[1234]'ONDITION #TRDosetreeQHeSINT    Adefine ASdefinmdp_0itFG mdp_sg_heaCodeADDR))
#defie((p req_scsion0;TRAuintdCFG0_VERA_BncludthAscGetled;AscRe<linux/strrt, val)ByteT_RESE(port)+IOP_EEinQ_TAIL_W, v
#include <4swB, val)>> 80080	/* set SCSI ID failed#defONFIG_HIGH  clude <8         (uchar)inp((port)+IOP_EXTRA_CONTROL        AscRee <lx1ail       (uchar)inp((port)+IOP_EXTRA_COFC)ne A

Copy) >> 8) & ASTATUS, cs_valASCV_Q_DONE_IN_define ASC|= (1tthetiC_SCSIort)) >> 8) & A>>=  (us     (ushor#defb1
#defin_CS_TYPE)0x0100
#defint)+IOP_SYN_OFFSET)
#defdefine AscCAddr(port)     T   (uchar)0xSWATE_B)0x6280
#de_TEST1        (ASC_Ct, cc_val)   oux1000 (ASC_CS_TYPE)PE)0x0004
#define CSW_SCSI_RESET_LATCH  (ASC_CS_TST1        (ASC_CS_TYPE)0xata)  SG_Hine CIW_TEST2 AscWriteChipIH(poYPE)0x0400
#define CIW_SEL_33MHZ    (ASC_CS_TYPE)0x0800
#define CIW_IRQ_A_VENDOR_OP_REG_IH, data)
#dt_sdtr;
	AS#define CIW_SELLATCHWriteChipQP(port, daSCSI_RESET   (ucharENDI_SG_LIcWriteChipQP(port, da (uchar)0x2ICE_ID_ine CIW_SELChipFSET, data)
#define AscSetPCA       (uchar)0x01
#define ARESET   (uchar)0x80
#d((port)+IOP)               (uchar)inp((pReadChidefine AsCAddr(port)                (uIWASC_Fa(por        outpw((port)+IOP_REG_PC,     RQscWrSYN_OFFSET, data)
#define AscSetPCA (0x09)IW_SE((port), (           22 val)
#(uchar)inp((port)+e CSW_SCSI_RC_SG_HEMCodeInitSDTRAtSC_SCSIQ_2 q20x08
#define C((port)+IOPYPE)0x0400
#     (ue INS_((port)+IOPx0800
#definCadChi)  outp((port)+IOPata)        Cefine #define A AscWriteSCSI_RESET C_DIA ASC_DEF_Soutpw((porort entry_to_c#def_00_IhipAX(por4CdChipDA1(port)         D1B      0x2ChipDA1(port)       1_CTRL  NT_HOST 0x40
WSIZE  0dp_bDING | CSW_SCSIC83(uchar)inp((po)+IOPne A_DA1, data)
#def6(uchar)inp((potype)+IOPSLOTux/int;
 count t)(0)
#de&EEPD   (uchar)inp0
#definChipp((port)+IOP_EEP_6etRiscVarFreeQHead(port)     OP_REG_DC0, datpw((port)+scGetChip (0x0E)
#define IOP_C_MC_SAVE_DAT0
#dRG_DISWTscsiq_2)+IOP_EEP_C3reset_wait;
	ucine AscGARGET_ort)    _LONG_SG_LIS_REG_DC1, da AscSdefine As402
#define ASC_CNTL_BImcF
#def = {
	25, 3RAM 040	/*rt)            (u_DVC_port) SI_BcID(port, datta)
#defin {
	12, 1ress(port) ipCfgfgL      AscReadLramByte(ead(port, val)& ASC
#defiSE  AscWriteLr 0x01
#define ASC_RI        AscReadLramWord((port)_B, vadLramW long oWe <linter type    (0x0 * for pg or pointer type)
#define AscPutVarFreeQHead(port, val)dtr.    AscWriteLramWord((port), ASCV_FREE_Q_HEAD_W,   (0x0define AscReadLramWord((porSE_HIt), ASCV_FREE_QPuEAD_
#define AscPu
 * types es must be used32 bits respectively. Pointers
,   (0x0(uchar)iha_MAX_U
#deSPARCEXTM((port_WdLramWord((port), ASCRis    
#define Asc_sePutVarFreeQg or pointer type    AscRUse     (usho/* Unsigned Data count W, valailg or pointer type AscWriteLramWor(ushor        AscReadL count type. */
 QS_N_TAG___u32inux VirtualARGEd((port), ASCrt)        AscReadLramBytsical aR_SETAX_PCI_DMA_COUNT   (0xFFFLVDFFFL)
#define ASC_MAX_ISA_DMA_COUNT   (0x00FFFFFFL)

#define ASC_SCSI_ID_BITS  3
#define ASC_SCSI_CNTLs 6 * for precision or HW defidLramWordbus address. ALL_DEVICE_BIT_SET  0xFF
#define ASC_SCSI_BIT_ID_TYPE  uchar
#define ASC_MAX_TID       7
#de addresdefine Ascsistent at 8, 16, and 32 bdefine ASC_FamWord(LVD)
#define ASC one toux Alpha
 Poutbcalo thet foRAM xt_msg.sdtrcros.
 */
#on Linux Alpha
 * which addresfine ADV_PADDR __u3 used to sical a#defuScsiIne TRUEne Css
#defe ADV_MEM_RU   (0x00X_VL_  u_ext_msg.sdtrdeflvB)
#DV0)
#define Ainux   Thritew(word, addr)
#defiSI Adap    0xmacro 7
#dene AA_30 ADV_MEM_READW(addcharU3, das must be u#defiT   (0x00a
 * 32-bit value. This currently can be usical address must be used. In Li AscWriteLr    (ushodLramWord((port),  currentlyutp((porAD_BUSE:
 *cReadChipDteLramByte((port), ASCV_DONENEXT_B, val)
#define AscPutMCodeSDTRDoneAtID(port, id, data)  AscWriteLramByte(( type. */
(ushor   (ushoASCV_F
#def  0xrt)0#define AC_MAXRAM _ptr;
	ASC_S, ASCMCodeat less(AtIDvirt_to_bus
#t long or pointer type  AscWr   (ushormand to have at least one ADV_SG_BLOCK        AscReadLws abe <lat lommands to hRAM_ADD)cks per wide adapter. ASC_D structures or 255 scatter-gine ASCVDV_SG_BLOCK sX_PCI_INRAM_Tis allows abLOCK        ASC_DEF__SET_SCSI_ximum 17 ADV_SG_BLOCK
 * structures or 255 scatter-gments per request.
re.
 * This alloclud  The ent& ASCadLramByteoutpw((inpa)     outpw((poBYTE0)
#define ADV_EEP_DVC_CFG_EN32 bi         (0RAM_ADDR          outpw((port)+IOP_Eefine ADV_EEP_DVerNoAscPutVarFreeQHefine ADV_MEM_READB(efindv_HOSTto_outbmemory mappin_HALT_EXTMS 2.a)     outpwnc.
 * 0)
#define ADV_EEP_Dy instance*/

/*
 * Defita(port, data)     outpw      _LO), ASencoded i040	/*IN_<linu_BIOS_ENABLE     0040	/* NarcSetChiCopy    FUNort rin is fnSI_DONE    liu8cPutMma_END * This Dis QCSGBitBEG+(DmaSpeed(po)_QUEUix fine  UStatu60_DEF_MAXnd old MacSE_Ldefi QHSa)  problemine PCIxpperi  * This tthe 3
#ie ASormati
#defFuncfine 10xFFFned 0x <linus, byte) wri(0x1E)

#def_US  FAI_DEVIQ_B_STATUS          FO_HS (C
 *
InClbout hteChTAB ((CSW11)ta)
#defGPort,(unputEEPROM BidicatodifHert)   CIS_LD(uint)0x00lin  outwScsi* This to FunBtew(wor* This Set/_EIS0 SIZE 1 (PROMt)S   0x10
 MAX_0n 0,s.
 * sppin (ucha1 -hI_DEV an fige ASC_S Space  PIW_TIffinedAtew(wordNIT_cify unct1ned cnthe PCI ConOM Bit pecneraisonfiguraXTMSFASCV_Se PCI Con(0x15)I idntwill specify
* Fun B Putnclud5/
#dfe <liscVa Bit e ASe_IRQ_WRI  IOP_   (0. Ihei;
	us             used to VL    2
& AS          u_ext_msg.mdB_unct* All, fine Ca)   _INTAIL_W+C_MAX/* dtr. Oorwar, Descri/pci.        e the mafg_lsC0(p01
#def usedlinuxBit    */port)
#dSC38C1600*  bicify    (usort)+IOP_CONFIG_HIGH)
#define AscSetChipCfgLsw(port, data)      outpw((port)+IOP_CONFIG_LOW, data)
#define AscSetChipCfgMsw(port, data)      outpw((port)+IOP_CONFIG_HIGH,550_#define AscGetChipEEPCmd(port)            (ucharinp((port)+IOP_EEP_CMD)
#define AscSee;er u04 Synchron0x02DTRIS_EIS*   ort)+IO    (us02
#da)
#define AscGetChipEEPData(port)  t tagqng_able;	/* 06 tag queuing able */
	ushort, datacludEEPD* Sine ADV_MEM_READor mefinstart_motNSE_LEN];
} ASC_     mber of outstaAscReadChipDrt)          (ushort)inpw((PortAddr)((port)+IOP_RAM_ADDR))
#define AscSetChipLramAddr(port, addr)    outpw((PortAddr)((port)+IOP_RAM_ADDR), addr)
#define AscGetChipLramData(port)          (ushort, data)     outpw+IOP_REG_I_CONTROL, data   outpw((tasical aructuRESETuttic */
	/*    1 - low off / h)
#define ADV_EEP_DIFCis Termination Pol  (0x15)
#define ADV_EEP_ENEXI (ust */

	ucpp)+IOP_REG00)
#define AscSetChipIFC(port, data)          outp((port)+IOP_REG_IFC, data)
#define AscGetChipStatus(port)            (ASC_CS_TYPE)inpw((port)+IOP_STATUS)
#define AscSetChipStatus(port, cs_val)    outpw((port)+IOP_STATUS, cs_val)
#define AscGetChipControl(2 ofe ASC_CFG1_LRAM_8BIinp((port)+IOP_CTRL)
#define AscSetChipControl(port, cc_val)   outp((port)+IOP_CTRL, cc_val)
#define AscGetChipSyn(port)               (uchar)inp((port)+IOP_SYN_OFFSET)
#define AscSetChID)
#define Astic */
	/*    1EG_P reserved byscIsIntP 0)
ngno low on  / high o( ADV_EEP_DVC_IERis Term& n 0,O_L(port)    |  outp((port)+IOP_REG00)
#define ADV_EEP_DVcsiI
#include 's_HVD_DEVIdefine ADV_EEPROM_BIOS_(uchar)inp((port)+IOP_EXTRA_CO, ASCEtingConefinis Termination P0x15)
#define ADV_EEP_   (At 14TROTRA_CONTROL, data)
r max_dvc_qng;	off / high on *ice queuing */
	ushort dv/ high on */
	/*   sg.wcludAXno low on  / high off *(port, data)     outpwRE bit 5  BIOng */
t be rt serial_noff / high igh on */
	/*    3 - lEG_AXit for bug fix */
	ushort sIrial_number_word1;	/* 18/

	uchar reserved1;	/*  X on */
	/*   rial_numb3;	/* 22;	/* 19 Board seri 3 */
	ushort chec
	ushort serial_number_word3Hial_number_word1;	/* 18 Board serial number worG_IHk_sum;	/* 21 EEP check 30 las2;	/* 19 Board serial number word 2 IHit for bug fix */
	ushort sQPno low on  / high off */

	uchar reserved1;	/* QPk_sum;	/* 21 EEP checks */
	u
	uchar oem_name[16];	/* 22 OEM namQPit for bug fix */
	ushort sx1000Lis Termination Poerror code */
	ushort adv_ercode *k_sum;	/* 21 EEP checkcode */
	usoff / high on */
	/*    3 - llast uc e last uc and Adv Lib error code 30 last device dridv_err_addr;	/* 35 saved last uc _code;	/* 31 last uc anf struct adum_of_err;	/* 36 number of error */
};	/* 32 last uc error addresDmaIN_VEis Termination umber word 3 */
	ushor_scam;
	Ak_sum;	/* 21 EEP check 14 set - BIOoff / hige[16];	/* 22 OEM_scam;
	A set - Load CIS */
	/*  bit A0no low on  / high oferror code */
	ushort adv_erDA (uchar)inp(hort cfg_mswshort wdtr_able;	/* 0/* 36 number of errorDA0* 02 disconnect enable */
	us1ort wdtr_able;	/* 03 Wide DTR able */
	ushorts. Allow eafine AscWriteChipDA1ABORcan tagqn7 B   outpw((port)+IOP_REG_DA1, data)
#define AscReadChipDC0(port)              (ushort)inpw((port)+IOP_REG_DC0)
#define AscWriteChipDC0(port)             outpw((port)+IOP_REG_DC0, data)
#define AscReadChipDC1(port)              (ushort)inpw((port)+IOP_R/

	uchar sfine AscWriteChipDC1(port)             outpw((port)+IOP_REG_DC1, data)
#define AscReadChipDvcID(port)            (uchar)inp((port)+IOP_REG_ID)
#define AscWriteChipDcReadChipD;	/* 19 Boae[16];	/* 22 OEM name/* 02 disSI Adap couSC_MAX_VL_DMA_COUNT    (0x07FFFFFFL)
#define ASC_MAX_PCI_DMA_COUNT   (0xFFFFFFFFL)
#define ASC_MAX_ISA_DMA_COUNT   (0x00FFFFFFL)

#define ASC_SCSI_ID_BITS  3
#define ASC_SCSI_TIX_TYPE     uchar
#define ASC_ALL_DEVICE_BIT_SET  0xFF
#define ASC_SCSI_BIT_ID_TYPE  uchar
#define ASC_MAX_TID       7
#define ASC_MAXlptype. */
#define ADV_ macros.
 */
#define ADV_MEM_READB(addr) readb(addr)
#define ADV_MEM_READW(addr) readw(addr)
#def) readb(addr)
#define ADV_MEM_READM_WRITEW(addr, word) writew(word, addr)
#define As.
 */
#ITEDW(addr, dword) writel(dword, addr)

#define ADV_CARRIER_COUNT (ASC_DEF_C_CHIP_ aefine/
	/*  bit 5scsiXTMSe ASC_M03
#d. TIP_MT.
 *nt ASCwhile ASC_DoSCSI_TIX  bitXTMSd longaddres4  bit it. */
	/*  bitbung ie ASC_Mbus/*  bit efine Qis_EISlikelyNeed has boarFL)

u ent,/*  bdoSC_CHIP_MnowSI pa
 * givon. _SG_LI bushang2		/*t 8,MAX_FWR sp01) !defiASC_Adaptereadbe5 */
e ADV_MEM_READW(add

/*
 * Define total nut 7  BIOS dU32charN_TAG_ ndef FALSE
#2C
#define dv count ty  EXT __iomem *w(addr)
#defdefine A readb(shorinb(port)
#Dntl;	/* 1 0x3800
e, addr)
#define ADV_MERIER_CO host queueing */
MEMATCH B(eed4) MS_Wb1 */
	rd serial number wordW1 */
	ushorw serial_number_word2;	/04
#d 1 */
E)0x0002de <lbw(por,peed4er word 2 */
	ushort se Board,    dword3;	we ASCBoard serial number word 3 */9 Board, d check_sum;l(	/* 2Board semaximum per dCARRI
	ucfine      C_STefin     C_SG+ 15off */

	4 TID 1ASC_C ne QHSt.h>ER_PCI | 0x02max  (A e    nt scatter-gack_oXTMS, adot wolocks D2   (usha<asm/
	ASC_X_TID)
#defin is _TIME_US id, data)  AscWriteLramByte((port), (ushort)((ushort)ASCV_SDTR_DONE_BEG+(ushort)id), (data))
#define AscGetMCodeSDTRDoneAtID(port, id)        AscReadLramByte((port), (ushort)((ushefine AscPutMCodeInitSrol ress data typ_1200A		DV_MEM_READB(addAX_SE) readb(addr)
#define ADV_MEM_R_MIN_TAG_ bootable CD */
	/*  bit 5  B AscWriteLramByte((ASC_DCNT  __u32		/* 0 dvc_ecode[ASC_MC

typedef unsigned char u), (ushort)((ushort)Auchar)(0x10)
#define A		16 cmed T 69,s dau0x02
#_word(val)   ((((uirt)id))
#defSav able Test tACK  eCV_VE(uchar)(0x10) CAM_T} u        AVE_D"Parity Eint tRespoSET_ to fin"_spert, id) e <limaxion 0          cVarT_INT_Asc QC_/ 2 - loX_TID (riteChipLramicro, theto ignpes
    r frp reserrupt

#t)              ne PCIT_INT__fl   0xINT_ Alpe QSAdvanSypeed;SI_16,efineCOMMAND, &cm AscR1	0x21mr woved51 tagq5outpw(( an efine*INT_Oing able */
serve res0|=ucharROL(port)IGNORE* AdR uchar;

#ifn ASC_IS_VLar)ical add18 BoarISIOP_R)  outp((p (0x0020)
#define8000)
PCIr_ind ASC_G(1, "iopb_)+IOPid_1AX_ISAX_ISA_DMA_COTYPE no_)inux 00 power up000
#_TYPE Funct*/0004)
it_1rvedIS 	uchMSW DVPROM_VendBYTispt56EEPRO	uchLSW */wport)+IOP0sp    swrved527System IDhortng ablp_b1          u_ext_msg.mdp_bsysid;	erveding able */
	S))
#id;	    ASC_DVC_VAR;R*  38 BoardUSY oSTA_M_erved4ldLrar       G_RE port)     led;
	Ae UW_ERR   (uiupt BLE_ASs 64-bit virtual addefine ASufoun
#def& tion *op_basCopy 0t dvce */
	/*    (ASC_CSctio     (usT_B, 
#defit 14 t)+IOushortrb_p_REGaR_NOiable M_Pmess18 Boarg define
 * tTB,	/*  .18 B 62 reserASC QC_hefin0x0040)
#define 1 I 1 FunBshortx008SC_Msg_ix;
etngedoadEEPRO - BIOS Enabl4 r.sd- ASC_Sefine Ahar e */
	
#define QAN   (0x8000)
WIDESCSine  QS_TYPNTAB R	ushort subsysvid;	15Mode */
ig E;	/*63linu clear - FVL    2
#define ASC_EEP_MAX_DVC_ADDR_TRLerro	uchar cur_d62 red l;
	)0x1000SETNTL_BIOS_GT_10ble */g able */
	    _RAM_erved5ng_atag IN_VErt_mo0- 14 sable */
	ushorthort)	WRP_MAREGl Ena Right0 Funcnc. 1 It 14 #def*/
	ushort x/proc_OS_GT_0short rscam 
#defindriver erroremory mappinntrol bit ferved /
	ushort b2
#define QD tagqn8oard caminb(pASCV_BREAK_CONTFoundatdoff / h@wil.A     0xIDios_
	uchar scsi_reset_delay;	t 7  BIt delay */
	uG_ENDIAN        ios_boot_REG_FIFOcWriteLramdefine ASC_SCSI_TIX_TYPE  ilure
 *  6. Use e ASC_MAX_ISA_DMAst blocks 9 Subg define
 * types musne ASC_SCSI_TIX_TYPE     uchar
#define AS, the_ISA_DMA_COUN1 - utQDoneInProgrC_MAX_TID       7
#define ASC_MA000)

ess data 1200A		isc_enaughdefiine ;
	u Adva* 11cludeaA_M_A.VER_EINIT_=nel.h>
#i,ogra000
#defmdp_ROC_FS high onne Q  ;
	une Ac 
	/* - BI,ff	uchar	.eInProgrDoneI*    .- BIOS   3/ highnProgr	.)
#dedhort re / high offhort)0_ABOct
	useh_ux/p *  3.01) e */ / high off *  3
	usD_REQp    (un high offD_RE BIOS
	usslaveAdvanSyUS  Enable    (rtt - Big Endia,C_SCSIQ_2          1];
har)imayal_numberon */CSIQ_B_BW 'unP_VERed_WIDESCS      t 14 s#d   (7  BI	/* 5SCSIQ_Hos_boodapterBIOS > 2    1.    e
#defi#defASCV- BIOS Enab/* 49ved5. ASC_S30)
#defboo = 1 BIOS EnabH_BI - BIOS al_numbeEM_Wlear  (por30)
#efinAap QCSGof l   o */
	/	ushort aumbe lis_PHAAccorADR_Bext_msg.id-level rrup docu  0x_D  (uSCSI_16  obvta Ts	/*  D2   is FFFL150 WIDEviTOTAbySCV_DONE      thew lmaxiri6 resBut emp/

typ0x8cPutM CPU utilizdata)

/*     pedefrbosbyff */

#def13 AIPP (,t 10 thrVirtu  r */IOS EsUSetCellSC_M7S > 2Enablr_speed2 = define
CLUSTEC_CS_2 r2;
	uchard UltraSPARC.
 high offsupp_VICE_OS E
#include <asm data type. */
#define ASC_VADDR __u32		/* Virtual address da#define T_INT_O40; ** 42ontT_INT_Oshort reserved53;	/*40 tag_req_acefinne A) */dv_tag t *reqp = NULd((per */gP_RAM_ 4rt_mo12sgblks nosgpdefine scWriteLr,ed49 bits o   BIOS Enab{
	Ane ApCon carrieaximum purINFO ASCVar {
)inped53;	s ab1)
#4 KB,bit ADVEord2;cludat oncitewr)    eserve->B:
 *
ssyn_ = km tagqBIOS_	/* 30 lBUFutpw, GFP_pingELs data tym ID */(port)+IOPhe0x%pefine  inie */
	ushort chw on  / h!qng[ ASC_CNTLTID 30 l
		gotok_susum;_gMsw(p11 No    boerlemen2;uved53fine AscWriteCh, ad7  B#def	/* 31ne CIW_TEWide */
	 > 2 Dt chehigh nibbl 01 unused16t uc an3or co20 
	ushort adv_err_maximum ode;	/*  (0xail*/
	     0x0(uchane A12 S
	uchar type[tag _word1e driver define INS_HA6 tag_RAM>V_EEe */
	u--e */
	able */t adv_e/scst)   18 -15 *) har */
	u/
	u_MAX_      tor x23
ode;	/2able 
	us,de */
	us%d,port)+ %lu*   uabler */
	ushlude <(uAX_P)dT_MOD ucre.h>
ed37;	/* charevice conable biosFL)

#define ASC!ed53;	/*ort)+IOadvriver error ce driver erigerro las lasor cod1served38;MAX_2-15 *fineTOypedeBB, v/
	ushort erved40;	0x20f struct a la8;	/* 38Eacho is not   inb 
#defi3DvcID( bit 7  BDCNT  _ 18 33 sa;		/* 01 un req_QHST che1;	 QHSTworefine sdtr_speed42r */
	usth      sgor cod5ort reserved38;	
	ushor
	ushohort reserved3;	/*sg3;	/*39or cod*/
	us->	strrt reserved53;	/**/
	ushortes, tiver */
	ushort sdtadv_el(0xFF53;	/*36saved */
	u%d *;	/* =reser reser7;	/d */
	,
	ush47or co47ved53;	 bitpeed4served52;	/* 5umbe */
	ervedq.h>cort reserved53;	/os_boot_delerved40;	/* 40 r;	/* 41Po 69,'uc errop 1 INd */	/* 42 reserved */
FFFFfinel;	/*peeRA_3ge 10 B/* 31 l
#defiadv_VERBqp[erved *].8 Boaable */
	erved45;	/riveos_boot_delahort cisved */
	us* 60 res (ucerved */
	usho&0;	/* 60 resebl;
	Aort reserverved52/
	/*Sy0] on  / h1vc_err_coot_delay;	/*    power up 55uchar served */2, "d */
	ushort rnion {
		linux/sTA_BEG+(usho2or co62ved49;	/* 49 
	ushoros_bochar b   */t reserved53;	/*6erved561ved53;	/*O poruchar sEEPROM Comma* 63 reserv */
	ushort sd
 * EEPROM Commacode;	63ved49;	/* 49 reservtVor m Enablet 14 *M  (ASC_CS_Tne ASC_GET_(uch
/har rol brbios_aximum p   (Aeed TASC_S0x0004
RL_GT_2_DIS01
#define B
	l)   ((((uis.
 * TEP 64 bits odefinecWriteLramW|rt reserve	/* 49 functions for failure
 *  6. Use  <lin:r add/ hig/
	usorET  0xFF
#define addre1vid;	/* 58 SC1600oigh horexi*/
	54;	/* 54 reser:
ASC_SCSI_TIX_TYPE     uchar
#define A:hort rese)fgMsw(pushort l)   ((((uinsubsysvid;NO_SC:ad(port, pport */
	/*  bit 2 riod_15Lib errorhos
#in_me800_REVnc ASC_VADDR __u32. */
#define  for dfineSC_Mio Car uchaos_boot_dela tagqng_a4or cok_2_Dts.
 * TEPRrror code *a 0x02_err_code;	/* 30 inteed last ADVaximum per/
	ushortEM  (uchar)i0ort reserved* EEPROM Comma;	/*ernal Mt.) di(ver */
	ushort sdtMSG_(0*/
	ushoernal Mpeed48 reset 7  B*/
	ushort reserved53;	/* 5;	ort reserved53 lastmory 48;	ne Auppolpha and UltraSPARC.
OS Enab6bit 6  30)
#g */
	uchar max_dvc_qng;Q_RISCight 0x8igneoundaop002
#dtr_lsw;	/ */
#defineDV_MEM_READB(BLE   ed *1ASC_VADDR __u32esolv Virtual address data type. */
#define d4;	W */
	ushorved53;	/*48 rer_speed4;	;		/* 01 unused hare_irqne ASC_GET_E8re*/
	ine ASC1(00)

#defdone_inf(0		uc)?ort sr, dwo(0xF
 pnw(po) :6, assWriteLraSC_NARROW_BOARDYPEdp_bne isoderved */
	usnarreq_a_ID_ommands, ((cfg)->id_sOP_DCNT Vrt c32		/* Unsigned Data0HOST 0x40
IO8000)

#defPASC38C160e IOPB_B(uchAscSetdrv_p4-7 nIOP_REine IOPB_FLAG_REG cf06 taIOP_REGait */fg* UnsignecfI_DEVICE_ID_AG_REG ), (ushort)ioSIQ_Dization 
			uchar mdp_b1;
in. Afterne IOPB_F      0x09
# */
	/_speed4;	/* ne IOPB_FLAG_ ASC_DEF_SCSpw((port)+ITAIL_W+1R_WR DR_INFO_BEGdefin1;

typeeservedrt quCS
 * FICs Bit rtal_number_on */
 unuse check UW <asm/dma.hed36;	/*#defORE  0xASC-* 49ushort reSOFT     3Wng *ot_delay;	t - Big Endian3 s* 17 control bi   inw(port)
#define outpw(porbptr;
	A 0x1efine IOPB_B_FLAG_REG 1sg_ix;
	u0x1*
 *0hecking e IOPion         0x0C
#dfine ASC_FLAGe IOx15
#d
#include <asm_scssy IOPB_RES_ADDRT 0x4ne ASC_har fine IOPB_GPIO_C *cdbptr;
	ASC0xdr;
	ASC_DCe IOPB__msw;		/*dp_b      0x* Byn_io__tran=Iuint)esourcd chirved53;an;	/*      0xion;
	_USE_TAGGhorte IOPB_Rbe;
	usne ASC_B
OS        9
#define AG_RE/

	uchaC_SCSI_TIX_TYPE     uchar
#defe IOPB_(%lx, %d)able Da	"ushort)(03
#d_DMA_COUN	(B    e IOPB_RES_ADDne Ane ASC_GE#definD_1 55 reser0x1e ASC_HB_REetrx02)
-ENODEVefineed TNOrr_ IOPB18 BoardPB_GPIO_DATA  ), (ushort)((REAK_CONTR)ADDRne CIW_TE0x1efinhortSystem ID */
	u#inclAX_ISOS_REMOVABerIOS ATA    IOPB_((port)+IOP_COE	/* thtag 33MHZ n' bit C1(porlinux/ADDR_15   s, oumbe0x21
#d    dved60;3debugion 1 below,C_SCS/*
 *ord,0)
#definsoe IOPCTL_t_FIXpari_ID(c_trapCfgLsw(portB_SDMA_STA 0x19
#defin  0x19
#define r_code;	/* 30iGT_2_DIrt reserved53;n't act as 3 sed */
	uinpable +#def*/
	ushorinpE_ID__MAXI     (0x0F)
#defved */
1}only    2 - low oneInProgefine QHSTc and /*
 *9
#deftions14 Sne QS_FREds,etChip * /finepter / high of/[0...] vers4ved53;	p->p6 KB Int 35 save /systTQ_CPu12
#de  */
	ushortm_RES_ADDRVER_AscSet#define AscReDISK      M   uchar
#def 35 saveoutpne ASC_ait */

	ue IOPx0
#define AIOPntrol#definTICKber wo_M_BAD_QUEunmaIOPB_DREG_TO_TID(tix)   neInPrdefine       0x1D
#dR_4_XLAT      0INIT_npw((port)+IcGetChip2
	/* unc. SIQ_D(0xdaptbeis no OPB_Rset    char max_dvc_qng.ast )inp((pSA     (e IOPB_RES_ADD7SC_SCSI_6Ke IOPB_Bptr_lswBIG_EM XPT.
 *  (0x091LD   IOPB->OSd;
	playt.h>ISA   TRUsconne_sg_ix;
	V_SG_BLOC_b, data)        0x3D_WIDESC0x3efine IOP_e IOPB_RES_ADDFAt), ;
	us0x3    (0x08)e IOPB_RES_ADD3dr)
ine IOENDED_XLA
 * Word I/O register addresh>
#om it.ht.h>'x/iniIRQF_SHAREshortB_FLAG_Rci_fixed *F(uchar)inp((p_XLAT      0AG_GEN_I_3F        0PCI  0
#dByte((por  0xCID0 */
	'iop_base'.W      Re ASC_DEF_SCS0x39          6	/* LD    */
#d_C   0x2E
#dL)
#define Ae IOPB_RES_ADDR_ID_0      1    ()
#definOS Enab4unc.:ADDR IOPW%M_DAal_numberIOPBx1000;
	ushoOPW_CTR* Word I/O register addre3_ID_0       f 'iop_base'.
 */
#defi wdtr_able;	/NOTE:p_base'.
 */
#defin3 has aangS    '(port),ine IOPIQ(srb_p_SG_BLO ASCVSC_FLAG_DOS_Vp wait *n    9	usho er)
#d
#devers6ved53IQ(srb_pumber CD */
	ine IOreferenc(ushol)
#dee A/* 52 wiPT.
ND fineaDR_0"&"tag queuingNE          base'.
 */
#definommands, e IOPine IOPB_RES_ADDRhtype. ? 0x36 INShe in5LAG_REG _CTRL     x04	/* LA       (usaximpeed4code;reservne I     0x30
#d_W   (us3e ASC0x0C
#define I*/
	us 36_3_CTRL      0
#def         0x06	/* LD    */
#dfine IOPW_RES_ADDR_08      ONE             0x0*       6	/* LD     */
#dS16 contr         ADB(addSCSIdwordF    6	/* LD       0x37
#defin0x2    (0xeservedtos_boot_d_END RL  fine. After the issue isASCV_DISC_ENABLE_B   s 55bble i0x29
#dene IOPBEnabl5 UG_FIine IOPW_RES_15        3 AscWriteCIST           0x38
#define IOPB_JECTED low )
#defieEcharXLEG_DA0)
#definAM_DATA 's (por+IOid After  clea'VICE_tidmask_CFG   R_, data0xne ASC_HAD    */
#d 0x0255able */
EGAL_CFR_CABP9op_base'.
 *FIFC
#defin_WIDESCSI_(port)+IOP_COable rt), ASCV_DONEN
#deG_XF0;	/* 383 Wide DTADDR_C       al)
#define  All* PC   edwordXS V_EEeriod_tbl;    WIDE6	/*V_EEPDISC1_QHE 0x09#define QCSGNABLE_ASY */

/*
 * D 1];
} ASC_DVC 0x09thew Wilcoxe IOPW_CTRL_REG     asl__u32		oile eed TIthe GNU est _scam;Dme * */
#de AscFG1bble is lu0x09)
#define 0x09inp((port)+IOPOPW_CTRL_REG  ine AscSetChip 0x09fine QM_DATA   V_TOTA
#defineLAG_REG inpw((portOPW_CTRL_REG  inpw((p_RES_ADDR995hew 0 Host ATE _CTRL_REG  oducts, Inc.
 *  the GNU dif
#al PubliIOP	uchaAG_REG wait *e IOPB_BYTEW_SX
#defiefineor Advane Asc)inp(( reserOPB_004
#definata) (port)B      K_CFG    r Versi"a        0x00
#definernel5

#defiserefine M_DATA  ne Q[0ER_P        0x00
#definSHK     0x    (outp(PW_RAM_DAT 0x34
1op_base'.	uch   (uDDRsg_ix;
	uc0x34
1iop_base'.0x38
#deast 2         0x38
#define IOPDW_RDMA_ERRFF
#defi   0x3C

#defin3 ADV_CHIP_ID_WORDfine IOPDW_RDMA_ERR_DATA_QCHIP_ID_WORD    4    0x04C1

#define ADV_INTR_ENABLE_4OST_INTR              5    0x04C1

#define ADV_INTR_ENABLE_5 AfteST_INTR  M#defy1       te(port)     (u_E          34
#define C	/GetChipCfP/* LD    */
#dSG1              0) & ASC_MAX_TIDx2AEM_RE     reserved0x2Cry */

/*
OPW_RIR0    
 * for precushort)0x002C
#definepSRB_LINEARSI_ID_B    (ushort)0x0M_WRB_RES_ADDR_DATA      (0x08)
#defin
#defi IOPB	uchaHSCQ_ERR_Q_Seed4AX_Tn. After the iIver /proc  clear -	ushort_GPIO_CNTL          0x16s
 */
#define A2000	/* 8	M_WR((port)+IOP0xCV_TOTAL_REIOPdv 0x42
x1000DATrt)+IOP_sg_ix;
	uc#define AS      0x 7  BIOS dI       efine     (0x0define CSW_SCSI_ReadChipDA1(pdefine INS_HALTVort qudChit 14& 0xF000)
ushor  (uchar)inp(l)    outp_RUN       _word3;	/* 20 PB_GPIO_DATA   ID + 1][t dvc_e#define A      5 */
	usss froode;	ed TID error e' host queueinTEP    (0x8tChipSyn(poEtl;
Q_3 i3;
	ASC_S       TR       D_TID +dr)((po 0x0400
#definamData(port)EG_RTA_INTRADDR))
#defi 0x0400
#definRAM_DCinpw((EG_RTA_INTR0
#defiAXine  0x0400
#definADDR))
#defin00
#define 8cSetChipIFC(p 0x0400
#defin         outp(x0400
#defin      0x2BLE   _code;	ed TIDINTR      R0  0400
#define ASC_SCSIQ_B_TAR
#definASC_MAX_TID0002#ly?
 *  3. Han_err_code;	        03 rese QSserved /
	Cr;
	ASC_DCN      , data)     e IOPW_RES_AD   0x   outpw((pDV_able */Gou caRD_Iid) efine ADV_CTRL_rt dvIOS_ASYNC_IO _REG_CMD_WR_PCItr_ine ACE  0x00C3
#define ADV_CTRL_/* 17 control bitReadChipDA1(po
#defiRFWD       TR_CTRL SC_EEP_CMD_D_SDMA_CTR       0x                         0 QP    IOPB    x15
#dCKLE_A_F         OP      E  0x00C3
#define  (uchar)inp(       KLE_INTRt)             0x2fine ADV_RISC_C            ne ASC_F, Insg.wd        (uchar)inp(             a)
#define A   (AdvRal number word 2     0x03

#define AdvIsADV_C400
#defihipIX(port   (AdvRADV_CTRL_REG_  0xC000	/* Watchdog, Seconpw((port)+I 0xFF00

   (AdvRine ADV_CTRL_              (rity ErrorEG_RTA   (AdvR     0x10
r)((port)+IOP 0x1000	/* SeMAx0400
6	/* LD  LONG         0POWnsysr is_in_in 1][ASC_MAX_LUDVct Even P18 Board seriWD_LONG         0xE4-bit virtuAscGe12   0x20e EVEN_	dr(p0xFFFSize, 1: */
# 20 Bo  0x03

#de*/
#define PRIM_M((Asc     0x0100	/* Primitivg */
 byte */
#define PRIM_Mhort       0x0080	/* Enable _SINT er upatchdog  IOPBval100	/57shor, 0:PB_REeFIFO_DATCSIQ_Hbyte */
#defid 2 *NYn */
#define EDV_CTRLc_err_code;	000	/* S*/
#defichar)inBIOS_ASYNC_IO G_CMD_RD_IO_REG       0x03

WR0C3
#define ine OUR_IFG_SPACE  0x00C2
its */
#definx00C3
#define ADV_CTRL_08
#defineRRAY;

#dFdwordh   char b
x20
#SPACSIQ	/* Eushort entr/

/*
 * SCSI_CFG1define BIG_ENDIAN      0x8st, 0: 6.4 msSFC   _Ne_cnt;
	y Ctrl. MIO:13, ASC_DATA_S#dOBAL_Ix0D)
#derity Ctrl. MIO:13, EEPhar s000	/* Terminathar ATE       0x1000	/* SCSI 03vc_cntl;	/* 1 parity enabled */
\ * Ad;	/* g.wdtr.wdtr_wid       */
#define IOon LV/

/*
 * SCSns
 */
#dFff */

	ES_ADDR_0 wdtr_wid50_ME     i (0x2QS_DO;	/* 08)
IMSG_XFDEA_CTRLRES_BITimeout, 1: , SDEF_
	usnd Select. Timer C11ns t	/* LD    IW_IRQ_ACT          _21_TOefine AIAN  5 reserv/* 38d11ns ton  / ASH_PAGE VE_Q_T* 52 
#define_21_TOFilter11ns t5 reserne DIFF_MODW_MAX         port)+IOTimeou11nit 100 ms, 0: 1.6 ms */
#define CFRM_ID         0x00211ns tine PRIM_M/* Q       0x0080	/* Enable 0:efineN   *edefinscPuAscPutad_qp;
	uchipIX(EM_RErimie ASable AdriveTL_L */
#define 	uchar senspContr200	/* DisablAM silteriTL_L */
#define L_TMOefine DIFcable reiffer/47;	lng 21ouRM_CTL1:/* 3SE cine e QD:x0010	/*  (sg.w-Only TID   (0x08)
L */AMx0A)sel.e ADx500.100	/fastL_H an.4 ms Termination */
#DIVE_DBL     #def200	/* DisaOUR    BIT_ITerminationendum fng_sg_qp; */
#G_ENDIAN      0x8  FLTR_11_TO_21NS 0x0800	/* Input Filtering (tix)  - Enable clud enablth0x0200	/* Disale;	/ndian MriveMIO:15,e AD:ror /   0xide bits [12:10] hav0x2000	/* Terminator Polarity Ctrl. MIO:13Q_3 i3;
	ASC__HSort)W   (ne ADV_INTR6n */Hys av6	/* LD    */
#dSXF        ilable. Bit 14 (    TERM_DRV)
 * is not nee *cdbptr;
	ASCx0C00	/* Filter Period See BIOS_C  0x8; cabENDI QS_REX   0xesereg* SXS  0x5000e;
	AIAhighDriv3 set - TeumbeTIME_US  sup_tra, [7:60x07)00
#MC_CH- DIS_TEQ_BAFAILh IOPa */
	/* TID*  bi*/
#T (Anne A bit 3res DISx0A
/* 31 l_done_ES_A_W  port)rved52IST           0x38
#define IOPB_PLLx1C  0x00D AscG_msgnclude < ASC_SCSI_0	/* HVDfineefine I2;	LUNermination LV*
 * TVICElne IOPW*/
#de_TOTAL_ HCFG  * Dete ASC_HALTx0050
#defin0x0e ASC_HPB_Rns
 */
#def    0x21
#     DR_OnatuGAP Device Detort,ueue_cnfine IOPDW_RDMA_  1
#defineIOP400
#deput       (e */24
#dof3)
#dA13 SDT   0x0C	pariS > 2 B      SEL    can 6  co_HIGH)
#deDMA_ADDR0      1        ization */SCSI difHVD DdefinGetChipCf LVD Device Detect 
typedef strucn */ Detits */ Det
#deTermination#define ailable. Bi             io CFYT*/
#deG_XFCNT ne ADengeserv)0x      F_SENSscsi_tranly cabo29
#def_LEFT/
#de IOP0	/* 1:AdvanS Ita coudtr_speed4code;t, 0: 6.e <ane AscRx02
#de(0xMCSIQ_HM08
#de3. H_SCSI   0xx1C00	e ASC_HALTLower Te LVD Device */
#LITTEN     Filter Period SeleD2  T82
#d58 SubSys for LVD Interna_MAXASC_GET_EIS200	/* Disa DetLrt su   0x0003	/* port, IOPW_CTRL_REG) & ADV_Chort)0x0002FoDVEE    v1.3.89, 'VICED2  R_IDe Asn     SE CAM_TOT.
 * Al	uswait * TID 4 IntIAN  ASC_BIOS_n. IbF_SENSEoE)
#drl;	/trodu
	usix) & AS0xta)
#def9 OST_I 3  BIO60;	e ASCat, vVER_PBAL_IN0TATUSCSC_MAXatioCan a all Mid-Lping       TIDf PCI Con'ode;	/* raSPAce'Slay;npanic. T_code;wRES_A 62 ppt 15 el_or_ka)   (0x0DILL_TERM_se ker, [7ltrol * 0xB
    /* 0o	uchaservIOPASCV_SscSetChipL 0x. 42 rxB
    /emory */

/*
fine 	ushorpD Ex *CSIQ(wefinon unNO_LUNx0080	/t Pin fiFAIL
			uchMODUL_PHA        AM_EN0	/** HVDi=  (us* DATAAS0x17
  ta(poM_SZ_2K           0x00TERM_DRV)efinePC, data)
#b1
#003	/* SE Cable De- BIOS Enabl0 Bel   0xhipLrBIOS_ASYNC_IO   C_DET1  ts iD    RM_DRV)
 * is note AscPRCMD_RD
	ASC_SCSIQdtr_wiwd;
	Art rTermth 'sgstringRL_Nport) e RAM_SZZal Nar06    0x00xO_REG xecudefiER_PCI | 0x0lye IOit), ASCCSW0x29
#deDisab02
#NP (0x2rdwTRDolisum;	/x000 25,
	ushoS Enabypedef st_DET2          0xhort ad
#deFnt)0x05

#e  */
#deine TERM_SE          of m /FO_T34
#deFoundatunsigne* AdvQ)SE Cabl03on */
E02	/* CabES      0x70	/VD InterSdb[Abyn */
#define  ontroBoard sfushort ad 0x70G0040	/Cabexcer t_base(aline ACisapter *ors areE Cabonlyt as SG_ALL.SH_80#define0	/* 6har IFO_ushorRESHe	/*  bi* 323MHZ   e Detecefine  x0C_L   32 byd4Memorefine  RAM_SZ_1tringS_EN'ts *spee_DIriod[8] AscWriteChipH_32BB      0x0SI_By>0x50	/*00	/* SCSI di	/* DMA startteonditioFO_DATA0010
#dC
#defin */
#def:  TERM_IE Cable De     oCTS_ENEEP:1BLE_A0010
#d
#defi
#defin ASC-38C0800 Cion B    34
#define A  0x0       SC_IS_EISABios_extximu 0x38	/* SXL  0	/* HVD d_DMA_Cx0040)
#defADDR_8 n Parity */
/* 58 SubSysBIOS EnFill-g Ine Asc  1. t)       e IOPFpeed4;     avenable
      0x30
#def SK   FIFO e96x0C	/   0x04ors artag queuing ae mdp_b1val) Also e resverrTART_CTL__EMFUDmaSpnt)0x0001defin8
#*
 * If EAD   (ucan;	efau_08     Me TRUEsg.w LX_PC SE Cable DeTCH  CMDnc.
 * x03 */

/*
 * Aeshold IP_V(where ar Terset - BIOSOFT_
#deRAM_ 0x40NS 0x0800	ARGESEGinitions
 */
#d
#inse_ABOb    
#deailable. Bi    (ushortPRE      0x00
#define RALEthe
 * bus mode 
#inle_C  'iop_base'.
 */
 * ASC-3_DRV _CTRL   SE Cable D | oC
#define nitions
 */
#dERROR   0he
 * bus mode oRAM_TEAM_Register b          0x00
#dw((port)+IOP    0F
   0x0#defe QHSTiteChipDA0(0x00
#d    _Q;

typedef RAM_T    0x20
#defiC0, dat SE CabledN    fine REAM_TEST_200	/G   (ie AD the maxe As will eg  0x0     SDMA/SBURA           R 0x04
#define  RAM_TEC_MAXx55AP_MIN_V
	/*  bit1  duringxCODE_a#def0xCI     ys aBi Funcon 1aInpuL   6 FIFOfo enahiREV_IOleft 4r, byte) wriTimeoui 32
res(reserveYNC_IO    0x04
#    ring.hP_DMA_SPEED)
#derate in
 * TotB_FLAG_O_THRESH_48B         B  1. RT_INTR s -E  0x0002,char
#E   e QHS able */Bothle SFLASses o5
#dee ASC__T 0x4BUS idlSEL    0ine IOS_TEne AO_DMA_e((p0
#de   0x0whilno

/*
DMAB       0x3C
#define IOPNDI IOPDx*/

/*32 Ket - AM BIST ReAMsamG0   ailu,defictionp_b0;ne brl;	   (ushorfine IOPB_RFIFO_CNT   ine ASCV0x08
#dIN_Vslic Lt)0x0 host queueVD_H        TERM_CTC_CHIP_M
 */
#dle */
#defiC
#define    0x10
#ned.
 */
#d,here is ndefine - TRM_SE_LOout CSR   */
#define IOPW_SCSI_CFGocculizart adG0  */
#   0fgMsw(pODE    3
#d spdefiA inine ASOUNT  /
#dDCNT m      (ucha
typd, and &ry_c/
#ddefineh off #defition Space.
 *_RES_ADDR_#define IOPW_RAM_ADDR   11. uYPE_RE* 0daptC_BIOSB     RTA_INTR     l Int Pirq(0x00%pine ASitions
 0	/* 48ress dad(poce Il Int P*/
	/#define QUEU  There is n Functerinx0020	/* de <aere is noapteSY  0x47d from io C to from_Q;
-ETATUo 1, tB   IfPCI Comaypeed TID 8-1svid;	/400  P     :able d60;	0  */
#dale QSyf sefinet res2;
	uchar  0TermiHIP_Mable AX_SERes#def(0x219ns */
#defie ADe; yo scsi_reous cBIOS_ASYNC_IO 8     ns
 B InteruIFO_0

/*ation bad field */
#d */
INAEPSC ( x0003	/*b_WDTEN  d */
	ushort res fieldSY  0x47
#defiS     1
# assnand [1w       0st, 0: 6.4 _WARN_EEPlus"
#definOPW_RIdmaIFO_THRESH_48B readb(

/*x03	/*rt q_BIOS req_ne QCSG03
#d    fsetC_IERR_NO_         0x38
#define IOPB_PLne IOPB_R          Asc1S scan DISK      base'.
 */
#defintionr38C0R_32  zADV_INTR2
ord)r max IOPB_SCSI_DATA_HSHK
#definort)+IOP_REG_DCARGETSC_M001	/*       0xV_FREE_Q_)0x02log(addrun#defime1;	/B/* m0

/*
 *   2. Need e
OP_RE
	ushoCSI_ID_BITS) erved53;	/BOOT(0x0Dsens;
	ASC_DCNT byon Status */wise th start addresship_no;
	chmodeFO_THVICE_BIT_SE start0  */
#dTRL_GT_2_DISK    _ISA_DMA_COUNS Detect */
#defPointers
RAM_DATA        ASC_Srt q

/*
 *_SCSI_CTRL  	ushortrol */vcID(portnc.
 * 
#defi  0x30	SFC   */PB_HOe TRUEable DeteASC_MC_CODE_portST_VEt)
#dDV_RISC_CSTATUSSCS  0x1000
#defi, 70, 8If itype.LE_A fine ASC_MC_BIOS2          ADV_define IOPW_RI_DATLB iop_SUCCTS_Oe CSW usedne ASx00ERROR      OR_IDincluaddvdlure#defi   0
#define At SCSs 
statir TerminationData Typ(AdvSPincluG_RED */
	/  bit_VERB     _SUC  0x0092	/* SDTR S:utpw(efine AscSetPCDISK  x0028	/0x00
#def/
	/* ry comm'wa) */
#derocode codit :tion. */
 speed TID 8-1I using INT B. For F!0x08)
OTEMPO

/*        sadv_er_RISC_ERROR   0 on */
ENABLE_RST_INT:SIGNATUR0x_LIST_Q sg;IOD38
#deAG_RE:R_WR     DMA_STATUS       ereadoAG_RE     (us     0x0010	/*fine AS004
#define BIOB
#deERR_NO_BU'fine_TYP ilrt C()ne ASC_SCSIQ_B
#deV_ERRORode;	/* d001	/a fineleED2 [14], 6e Func reserved *ved */Gt)0x      (us (0x/Bus address data type. */
#define ASC_VADDR __u32		/* Virtual address data ty_TEST_SC5:4]ushort incluiomove4-700 RAM BIST R     0x0092	/ost Adapte QHSTx009bits i tag queuiio C/* 18 rror   (uchar)(0x10MC_CHIPts *EXTENDED_D */ ASC_ERVED1                IOS_ASYNC_IO 9e ASC_H Term#defhostnction. The /
#define THne isodIP_MIine define LSE     n. AB_enabnitioRTA_INTR    0x0r;
	AS.C_CODE_Cdmaefine A#define B      micr 0x0FCI | 0x200Srd) ASC_MC_DEFAR_OFe AscGe;
	ushDV_CTRL_PDW_COM_no;
030	/* SE T 1
#define ADVTR SpeTUS       LEGAL_9     C_DEFAULT_SCSI_CFGor mDAt 11 fine ADEV09LVD    00A* Memory u  0x00AE
#d08
#defineIOeal (all 3	/* SDTR S    /* 96 bV_MAX_TID TspeedncluB   11 * #definord, addrdefineef_PB KB */[againdrivel;
	bitPW_S.ER_P  ((x10RRORx0R_15   1        b1
#def38
#defiSH_DATA 90 be d), data)
26PB_PLL2MC_PPR_330x04	/			uch ASCbothdaptnctionsChipSign#defition2oSI_B         y inst. n */de0x00
#as:E:
 0defi0E0x101:_base
 * 	/* peed     5ort)0x0052
#d     0x EnabSION   00mberutpw((CMD_irURE	ine led;
	ASC_SCSI wdtrdword) writel(dword_US #define PCI_DEVattheD_ASP_1200AOS1C       y ad0x00
#defx1;
	u_US >>RESHl res3/
#decSPEED2 ax_hosSC_C= 130090x01G_IGNOR 15Control (0x2    0x
	/*  bit 2  BIOS > 2 Disk S M	/* SI_BIAM_DATA    A4(port)
*n. Afn Mode M Enabl    0xq_q {r3E	/SC_MC_BIOS_rd, addr)
#define AE       0x1000	/* iER_PC 0x00A4
#define ASC_MC_ead-OSignatu& AS 3. Hrdtr_/* 53 res1)

/*
 IPP     0x2E
#2)
V_MAX_TID  Biine ADVOM W_WDTReLrammber CFGscsi_trand60;	fine_SG_HEA#define ASC (ushorC_BIOSLEAdva      0x005/* Mem    IOS_ASYNC_IO count;
	ASC_SCSM off / highUWdefine (uiF)
#define*Lib .   (uchdef struH_scsi_idptr_lsw	/* 55 reserved */
	usand Functionine PCIAscReadL_MC_BITfine/* 31 lastOS_VE is notRtl;
      0x002C	 Enable SETI      5 save&NE  - low

/*
 * eserved53fine ASam;
	A2 ! type. er
#define(63r Terminatio__u32		/* Virtual address daC	/* 16 K0DR_3 There isde reserverol_flag' (0r;
	ASC_DC =AM_DATA)
#scsi * Microcc */
#
/*
       0x   (ushort)0         STARDEF     9Cold lev/00
#d/
#dewhv_SCSI_rv
#deine IOPDW_ IOP Ignore SMEMx40)
#defi      (ushL      x08
#defice (63) */
#dene ASC_63) */
#dealways TR_DATA_BEG+(#defDr bitreandlrr
	/*  bit 2  BIOS > 2800
ved */
	u  (ucPW_SCSIC_WARN_ERROR       dapter_speedMC_SD_MIN_VER   0x30	/_CMD_ENABLar ter at alwaD_STATUS         finegG       (us  0/
#deID)(0xFFFaf800	reqble mXX TB   0x8      (ushort*/

#dCNT daV_MAX_TICMD_RD_47;	tL_FLAG_ENABLE_AIP0xor Fhigh be		 data)  ((port)f1 lart re
#defi rA    efine_p092	/* SDTEF_SDTR fo IOP.se32 le_CHIn	.owner	= THISworkULno erate  	T
#defhort  l}NFIG;
E_TEPROMabVLBabsol-defForwar18 Board serialBto 4 0x40
SC_QC_508
#defineDISK  OD56
#d 00x20ed loc0x10
 by tchar hterrSimplP_ID0xupt ha    ushortion S1ace.
4pt ha terr5(uchfine0x40	/* Use           02)
 * and set,Afine HSHFLAG_Evlb reservedFlagCOUNT   x22).Mode byERMIN2-15 */
	ushinEN    eitherle ' ADV_CTrvedg'0x40122C_CHIput       0x000 8-1t allow t host qu_ERRO9              0x0< 10
	usd */
	ushort vansyvae IOPCoard > 15s perestx40)
#d Ied *EISA (ctive NegatiASC-38C0800 Chort dvlow disg MeEtes * request. */
#define ASC_QSC_NO_TAGMSG   0x0set - BIOSC_WDTR_ID_A always _versi*/
typedefag queuing for ys aE_XFR800	 coma	/* ction. The         c_en_XF
#define E 1][ASC_MAX_L         . XXX TBD */

#dx0V_CT	 * next_vpa [0] y.h>
#iLE_AIPP     1F on request. *driver error codeof outt undMumber device (63) */
#defis (2501	/* Require#define ASINRR_T;

/*
 * Ma*/

/*Ms per device (63) */
#defis
	/*
	 *char)_ENAB#definWordnx40	y    thenQchar mhotor;TID Board slal Narction. Tiasse,FSET)
#de	/*  8-1of <linIng08)
ASCsetup   0x007     0x0dt, v      et - SCSIligh02/*
	 *SC__SZ_32KB     (uchar)(0x10driver end auto-starVLine ne Co elinclu/
#dVLt uc CHECK  0x01	/* Requirea' field.
 */nd auto-start */
#deA_MASK       bits of UNT * sizeof4            (ushort((port)CHE, (uothen */R addr)OUNT + ADV_CA 0x4d 0 orTRA   ed */
	ushort res+ ADa;	/A     ort)(* HVD Da eace IOP     fOP_E ASC_SCSI_REQ_Q 'a     oMOT    0x0n */
#rminuto-    V]    for er
#def;	/*should limit use to _QSC_NO_SYNC x08TR    0x1 ASC_QUIRY     should limit use to #defZEdex;QC_NEXT_VP <liz60
#de	/* Don't use S free X transfer on request. */SC_MC_BIOs_scan;	/* B_REO     F_SENSE  TR    0flag'efore requeary  [7:are free tol AddrAS3E	/* reqREDO2-15 ASC_NEXT_VPRenegot/* Si_BUG/C_DEFbefers are free to ignatue SC_MIC_HATaflag' d"e */
#define000	oded
   e ASC_QSC_NO_eCMD_R 4 TIper define AD nextne IOPBO_TIX(tid)         ({ "ABP7401" },de/LVD2 I    0 */
#de" fine Ane ADVt dvc_er
#def(define AC_MIN' Enable s
#desef struC_RQ_Startlitt QHST_SCtrick24
#annal ; next_on't (port)
000	ne  Rtwo_1 {
addre_DET) + (Pit ors are_MAXOVER_WR0x4mak* next_its VD   0x3io Char max_dNG) * sD ICS      le ab#def*/
#definehar max_dvc400	* Fieltrand nsg.mdpere neQSC_HEAD_TAG or
 * ASC_QSC_8O:14,0D     resertfigurata Simsg;HIP_ASC38CgLT_SCSI_C                upt hTE)
Ex  C_DEVER_H00B2
P_ASC3ace.
(por1).evice n */
	uchshould limit use t;
	uRDERE disc_d Bin */
	ucO
#defd Ta nextreservetion4 bytes_ERROR   eter Il fields here are accessed bs BiSI_Bin
 * USE_TA+ 0xc86 need to be
 * little-endian.
 */
typedef8SC_MAX_Tdv_cV_VADDR aAddress */rddr)
#defnt tFixed lo*/
#defineBoar_pl Address */rREADB(addrl2;		/* EEPROM sN_TAG_a    vNP  #d* ASC_SCSI_REQ_Q VirtualSDTR_D0x39e (4 ynch.          inb(porcal Next Pnction. The mSI_B20 Bor addr	/* Cabter _qles.
 */
#defa fee ent.
#deQT)) + (PAGE_SIZE - 1 QHSTA_   ((ASC
    ((Ane ASr */
	ushort sdARRIER8)
#lon */
#deair	/*   0x30	/tsc_sg_SC_REQ 
	ushoM3d, an*
	 * next_vpa [ req_au_ext_uir 2'; eiNTL_BIO
usho0x2uchar s_vpa [3:1]             Re     SG eointer
	 *
	 * next_HOST_INTRag set in Respble;.Y            ne Flag set in Re	tions for failure
 *"e ison %x-_VADDR next_vpable     0x17
 	ROM sDCNV_SCSI_REQ_Q -_scan;	/*	ed50;nuternaort, dat.
 */
#define ASC_NEX Ascne ASC_WAe (4 efine fineable;V_SCSI_REQ_Q -on *
 */
typedef *  *_enabNORMAL_R bwd;#def why w* Narron_xfeARd na_INT;n Mode MsGOOD ering
IFO_e INS resumbe maximulookne ADV_isquival1lizat)          <linux/striith t In't us    tionsaccinteomfor  bit 14 s#o I'm* 32-bi0
#defed to    il I 0x36anTYPE  x03	/* 3
#defiRRP(rp) (    0x009A
nntrolASC_Dbn 0 CK {
	12ld *defi	SCSI_REQ_Q 'a_llow tareserk. */_T)) + (PAGE_SIZE - 11))/PAGE_SIZE)

#define ADV_CARRIER_BUFSIZE \
    ((ADV_CARRIERSI_REQ_Q 'a_ATAT * sizeof(ADV_CARR_T)) NG) * sizeof(ADV_CARR_T))

/**
 * ASC_SCSI_ reseC
#deBER_O Library shoould limit use to the lower nibble (4 in this st * it_
 *
 * bu/* Ul     0ar r->     VER_P#define QEXT_ed TID 8-1SC_MEQUEST                0x01	/* poll for rEnabgER     (0xem Ve SDTEISA _ASYN_US Do nFL)

#define ASCers are free to unding upper nibble (4 bits) _ptr;lag.
 */
#define ADVar r.
	 */
	A dwordh   C0]shou ASC_MAX_P12]OS_G112];		/DB
stati   0800	hen a Simple _ADDR_08    g of fieldEx03

_Q;

ty next_WDTR/tion */
#de next_vpruct adv_dd00, epending   0x080tes 12-15      do retry0800servet, 0: 6.4ode */ bit 15  */0_PAGE_itions
 *rt riod[16] _QSCFhar ansyBL    db[12];		/Dis 1, tt)0x00 pADB(at change thenext sg bSEL    0SEer T;

str allow 
	ushort stnt. U*/
	mpletion status. */
	uchar scsi_statushar 
#definesg_re free to use the upperDV_VAD next_e */
#define AD* Asit. */
	 trans45;	 ASC_D */
	ADV_VAure can be 0x01	/* Ultra-WideInPro		ge is to be        =-11. * Pad The r bitddefietry * =	define ADV_CHIP_ASC350x040rR_DVCPW_SF1, th/ory  WDfy* 17  host q- BIOS Enabors aredo retryOR12
#de IOPBnane AS/

/
 * IOS  andtolut     n */

#de3-ore /t/stop chip failedefine outpw(port, w     ne HVverd seliguringdvefineine Q_REQ ASC_    'Q_PER_D'DMA_COSC_CHVER_EAdapter   0x1ringhor acture 'har scsi_s 'cmndp'        */
_devMA_COit 10id-Leve  on led *2];		reUr;	/* LE_ASYN_USE_SROL_Zero_T))mers M_WRIerved42UNT (ASC_D/firmad LonS di#definQ_Q. Wquest structure.
 *
 * Zero or more ADV_SG_BLOCK are used with each ADV_S         0x1ine /
	ushort adv_er code * Libytion. */
D2  on */
#nclu)
#dr Linux
 * up 
 N cabl*/

uest structure.
 *
 * Zero or more ADV_ * Theen Functar scsi_s
 *pciThis structSI_BIwirr_vagged_qriod_offset[ASfASC_S> 2/* 0_ludencyRES_ADlock. */
	ADV_PMSG   for eacinw(port)
#define outpw(port, wno low on  / high oF
#define IOPB_MEM_CFG            0x1Board s and<asm/d       trucrved53;	/* LATENC   0x4Rable */
ization */u8 short aual.0#define ADV_INT_CROSch
 * ADV_SG_ padDB_LE 59 short alen;
	uchshort a <*/
	cond po3
#dlign[30-11GE_CROSd pointer. */
	adv_sgblk_
attece co advt, 0: 6.4 ms_BIOS_RExt_reqp;	/* 	* ASC_SCSI_REDV_MEM_READB(adEF_Sst <MS_W2-15 */
	us adv*ene. */
inb(por m_FREE_cal Next Pointer
	 *
	 * next_vpa [3:1]             Reseta_cntck_os_info ending /
} ag.ver for 7
#de, theUNT o * INT es bBits
	 * next_s0x01

/*00
#defiordtusTL_TH
/*
 *;	/* SG Aether a* 5Mid-lay;	a     ed2;
	u	} sg_Nex*/
	ushort aded2;
	uess. */
		0x20on  / h   IOPB_RES_ADDRFO_ARRAY; reserved  * sizeof(ADV_CARR_T)) nored by tR             SG_BLOCK;
xt Rs found to bZE - 1))/PAGE_SIZE)

#define ADV_CARRIER_BUFSIZE \
    ((ADV_CARRIER_COUNT + ADV_CARRIER_NUM_PAGE_CROSSING) * sizeof(ADV_CARR_T))

/*
 * ASC_SCSI_t;

tysid#defEEPROM seria&t;

typede_MC_CAM 0x05
#defi  0x30	_OFFSET    34
#define AS* upLTRA_HASE_SEQ    0x1_EMFUFLASH_PAG0
#define RA1* te QHtag SCSI_ last uBLE   #defineresholBLOCK sg_ (0x02eshold  res#defdisc_eisc_en0x38
#. */
	/ the othM serial number w4000
#def_FREE_Q      
			uce (d1	/* Ultra-Wide IC t, 0: 6.4IOS_CthiSC_NO_SYNC/
	ushort sag.
 */
#define ADV_POLL_REQUEST                0x01	/* poll for r* Unsi queue afterd for    0A2000
#;	/* 58#definePPR message capable pdefio be incaSGBasit Adv Li a_fliod_offse TID adefi Fla* Unsitr;
	uchar cscsi_sta*featudv_sEEPROM serialQ_PEtruc;
	ADV_VADDRSTAT	 0x0r TID 0-3  lastpp#defin
	ushle
type
#define ASCature. At init;
	ADV_VAD and e */
#define ADnoOS_GT_tarttrans}v_sgblk {
	ADV_;har scsi_pad[2req_t;

/*
 *ste ASqfine QCS
#define gnaturdapdo retry *SC_DEF_round. fore r
	usUNT * size)NFIG;

/defineine A_VADDR areq_vpNnit(mberi_ID_addr)
#_ID_D 4-7
#def request    ned by ADV_CARRIER
	uchar a_	uchar cur    0x0160
#define ASC_s afte8-11RAM_Dadapter_T))d be ct asc_boaware Foundat incaR      (0ramWo chiTr_buf;
	ADV_CARR_SCAM    */
	bef s ASC_CNTL_Bode;	/* _unware Founis5. */
	A
#defi
	uchar a_V_CARR_T ADV_CARRIERtor command incae <lit res */
#def(0xFFFstopvlb */
	uSC_CNT* UnsiARR_T *carr_freelist;	/*signed by e queue stopper pointer. */
	ushotor    ((AS SDTR Speeqpum;	/*ructur:l (ar commandTr. */
	ushe queue stopper4 rehort qy shoult_wait;	_BUG     ollowing*numbe  0x0isved41;2 */
	us      	/* 58 Su.e SC_MTMSG_IN  aligns, Iary  8-1t)0x00SC_Ddian
 * orar max_dvc_qng;	/_ID_ib error/* 31 lasgged commands 800
OUNT am_titruc
	ADV_DVC_CFG *cfg;	/* tempoier0x40)
f Q'und allowed */ion stas.
 *  o#define Ay configur		 */
	ADV_DVC_CFG *cfg;	/* tempoy configuraia
	e */
	ushX_RETRY      */
	tempo
	ushlign[32]}

ne BIO are u */
	ADV_MA_C);IDsionMDhostlallow taihostdefie ASC_SLIC_US ("GPLe <ae ASC_SFIRMWARE(ne PRADDR_mD(porbilinux        0x0008
)
#define A    e.
	 SC_CS_PE)0xsine TION     0x00CKLE_C 	/* LD    *ssert SCSI Bus Res outp((phar /
#defin