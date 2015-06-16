/*

  FlashPoint.c -- FlashPoint SCCB Manager for Linux

  This file contains the FlashPoint SCCB Manager from BusLogic's FlashPoint
  Driver Developer's Kit, with minor modifications by Leonard N. Zubkoff for
  Linux compatibility.  It was provided by BusLogic in the form of 16 separate
  source files, which would have unnecessarily cluttered the scsi directory, so
  the individual files have been combined into this single file.

  Copyright 1995-1996 by Mylex Corporation.  All Rights Reserved

  This file is available under both the GNU General Public License
  and a BSD-style copyright; see LICENSE.FlashPoint for details.

*/


#ifdef CONFIG_SCSI_FLASHPOINT

#define MAX_CARDS	8
#undef BUSTYPE_PCI

#define CRCMASK	0xA001

#define FAILURE         0xFFFFFFFFL

struct sccb;
typedef void (*CALL_BK_FN) (struct sccb *);

struct sccb_mgr_info {
	unsigned long si_baseaddr;
	unsigned char si_present;
	unsigned char si_intvect;
	unsigned char si_id;
	unsigned char si_lun;
	unsigned short si_fw_revision;
	unsigned short si_per_targ_init_sync;
	unsigned short si_per_targ_fast_nego;
	unsigned short si_per_targ_ultra_nego;
	unsigned short si_per_targ_no_disc;
	unsigned short si_per_targ_wide_nego;
	unsigned short si_flags;
	unsigned char si_card_family;
	unsigned char si_bustype;
	unsigned char si_card_model[3];
	unsigned char si_relative_cardnum;
	unsigned char si_reserved[4];
	unsigned long si_OS_reserved;
	unsigned char si_XlatInfo[4];
	unsigned long si_reserved2[5];
	unsigned long si_secondary_range;
};

#define SCSI_PARITY_ENA		  0x0001
#define LOW_BYTE_TERM		  0x0010
#define HIGH_BYTE_TERM		  0x0020
#define BUSTYPE_PCI	  0x3

#define SUPPORT_16TAR_32LUN	  0x0002
#define SOFT_RESET		  0x0004
#define EXTENDED_TRANSLATION	  0x0008
#define POST_ALL_UNDERRRUNS	  0x0040
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAMILY        0x02

/* SCCB struct used for both SCCB and UCB manager compiles! 
 * The UCB Manager treats the SCCB as it's 'native hardware structure' 
 */

#pragma pack(1)
struct sccb {
	unsigned char OperationCode;
	unsigned char ControlByte;
	unsigned char CdbLength;
	unsigned char RequestSenseLength;
	unsigned long DataLength;
	unsigned long DataPointer;
	unsigned char CcbRes[2];
	unsigned char HostStatus;
	unsigned char TargetStatus;
	unsigned char TargID;
	unsigned char Lun;
	unsigned char Cdb[12];
	unsigned char CcbRes1;
	unsigned char Reserved1;
	unsigned long Reserved2;
	unsigned long SensePointer;

	CALL_BK_FN SccbCallback;	/* VOID (*SccbCallback)(); */
	unsigned long SccbIOPort;	/* Identifies board base port */
	unsigned char SccbStatus;
	unsigned char SCCBRes2;
	unsigned short SccbOSFlags;

	unsigned long Sccb_XferCnt;	/* actual transfer count */
	unsigned long Sccb_ATC;
	unsigned long SccbVirtDataPtr;	/* virtual addr for OS/2 */
	unsigned long Sccb_res1;
	unsigned short Sccb_MGRFlags;
	unsigned short Sccb_sgseg;
	unsigned char Sccb_scsimsg;	/* identify msg for selection */
	unsigned char Sccb_tag;
	unsigned char Sccb_scsistat;
	unsigned char Sccb_idmsg;	/* image of last msg in */
	struct sccb *Sccb_forwardlink;
	struct sccb *Sccb_backlink;
	unsigned long Sccb_savedATC;
	unsigned char Save_Cdb[6];
	unsigned char Save_CdbLen;
	unsigned char Sccb_XferState;
	unsigned long Sccb_SGoffset;
};

#pragma pack()

#define SCATTER_GATHER_COMMAND    0x02
#define RESIDUAL_COMMAND          0x03
#define RESIDUAL_SG_COMMAND       0x04
#define RESET_COMMAND             0x81

#define F_USE_CMD_Q              0x20	/*Inidcates TAGGED command. */
#define TAG_TYPE_MASK            0xC0	/*Type of tag msg to send. */
#define SCCB_DATA_XFER_OUT       0x10	/* Write */
#define SCCB_DATA_XFER_IN        0x08	/* Read */

#define NO_AUTO_REQUEST_SENSE    0x01	/* No Request Sense Buffer */

#define BUS_FREE_ST     0
#define SELECT_ST       1
#define SELECT_BDR_ST   2	/* Select w\ Bus Device Reset */
#define SELECT_SN_ST    3	/* Select w\ Sync Nego */
#define SELECT_WN_ST    4	/* Select w\ Wide Data Nego */
#define SELECT_Q_ST     5	/* Select w\ Tagged Q'ing */
#define COMMAND_ST      6
#define DATA_OUT_ST     7
#define DATA_IN_ST      8
#define DISCONNECT_ST   9
#define ABORT_ST        11

#define F_HOST_XFER_DIR                0x01
#define F_ALL_XFERRED                  0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE                   0x08
#define F_ODD_BALL_CNT                 0x10
#define F_NO_DATA_YET                  0x80

#define F_STATUSLOADED                 0x01
#define F_DEV_SELECTED                 0x04

#define SCCB_COMPLETE               0x00	/* SCCB completed without error */
#define SCCB_DATA_UNDER_RUN         0x0C
#define SCCB_SELECTION_TIMEOUT      0x11	/* Set SCSI selection timed out */
#define SCCB_DATA_OVER_RUN          0x12
#define SCCB_PHASE_SEQUENCE_FAIL    0x14	/* Target bus phase sequence failure */

#define SCCB_GROSS_FW_ERR           0x27	/* Major problem! */
#define SCCB_BM_ERR                 0x30	/* BusMaster error. */
#define SCCB_PARITY_ERR             0x34	/* SCSI parity error */

#define SCCB_IN_PROCESS            0x00
#define SCCB_SUCCESS               0x01
#define SCCB_ABORT                 0x02
#define SCCB_ERROR                 0x04

#define  ORION_FW_REV      3110

#define QUEUE_DEPTH     254+1	/*1 for Normal disconnect 32 for Q'ing. */

#define	MAX_MB_CARDS	4	/* Max. no of cards suppoerted on Mother Board */

#define MAX_SCSI_TAR    16
#define MAX_LUN         32
#define LUN_MASK			0x1f

#define SG_BUF_CNT      16	/*Number of prefetched elements. */

#define SG_ELEMENT_SIZE 8	/*Eight byte per element. */

#define RD_HARPOON(ioport)          inb((u32)ioport)
#define RDW_HARPOON(ioport)         inw((u32)ioport)
#define RD_HARP32(ioport,offset,data) (data = inl((u32)(ioport + offset)))
#define WR_HARPOON(ioport,val)      outb((u8) val, (u32)ioport)
#define WRW_HARPOON(ioport,val)       outw((u16)val, (u32)ioport)
#define WR_HARP32(ioport,offset,data)  outl(data, (u32)(ioport + offset))

#define  TAR_SYNC_MASK     (BIT(7)+BIT(6))
#define  SYNC_TRYING               BIT(6)
#define  SYNC_SUPPORTED    (BIT(7)+BIT(6))

#define  TAR_WIDE_MASK     (BIT(5)+BIT(4))
#define  WIDE_ENABLED              BIT(4)
#define  WIDE_NEGOCIATED   BIT(5)

#define  TAR_TAG_Q_MASK    (BIT(3)+BIT(2))
#define  TAG_Q_TRYING              BIT(2)
#define  TAG_Q_REJECT      BIT(3)

#define  TAR_ALLOW_DISC    BIT(0)

#define  EE_SYNC_MASK      (BIT(0)+BIT(1))
#define  EE_SYNC_5MB       BIT(0)
#define  EE_SYNC_10MB      BIT(1)
#define  EE_SYNC_20MB      (BIT(0)+BIT(1))

#define  EE_WIDE_SCSI      BIT(7)

struct sccb_mgr_tar_info {

	struct sccb *TarSelQ_Head;
	struct sccb *TarSelQ_Tail;
	unsigned char TarLUN_CA;	/*Contingent Allgiance */
	unsigned char TarTagQ_Cnt;
	unsigned char TarSelQ_Cnt;
	unsigned char TarStatus;
	unsigned char TarEEValue;
	unsigned char TarSyncCtrl;
	unsigned char TarReserved[2];	/* for alignment */
	unsigned char LunDiscQ_Idx[MAX_LUN];
	unsigned char TarLUNBusy[MAX_LUN];
};

struct nvram_info {
	unsigned char niModel;	/* Model No. of card */
	unsigned char niCardNo;	/* Card no. */
	unsigned long niBaseAddr;	/* Port Address of card */
	unsigned char niSysConf;	/* Adapter Configuration byte - Byte 16 of eeprom map */
	unsigned char niScsiConf;	/* SCSI Configuration byte - Byte 17 of eeprom map */
	unsigned char niScamConf;	/* SCAM Configuration byte - Byte 20 of eeprom map */
	unsigned char niAdapId;	/* Host Adapter ID - Byte 24 of eerpom map */
	unsigned char niSyncTbl[MAX_SCSI_TAR / 2];	/* Sync/Wide byte of targets */
	unsigned char niScamTbl[MAX_SCSI_TAR][4];	/* Compressed Scam name string of Targets */
};

#define	MODEL_LT		1
#define	MODEL_DL		2
#define	MODEL_LW		3
#define	MODEL_DW		4

struct sccb_card {
	struct sccb *currentSCCB;
	struct sccb_mgr_info *cardInfo;

	unsigned long ioPort;

	unsigned short cmdCounter;
	unsigned char discQCount;
	unsigned char tagQ_Lst;
	unsigned char cardIndex;
	unsigned char scanIndex;
	unsigned char globalFlags;
	unsigned char ourId;
	struct nvram_info *pNvRamInfo;
	struct sccb *discQ_Tbl[QUEUE_DEPTH];

};

#define F_TAG_STARTED		0x01
#define F_CONLUN_IO			0x02
#define F_DO_RENEGO			0x04
#define F_NO_FILTER			0x08
#define F_GREEN_PC			0x10
#define F_HOST_XFER_ACT		0x20
#define F_NEW_SCCB_CMD		0x40
#define F_UPDATE_EEPROM		0x80

#define  ID_STRING_LENGTH  32
#define  TYPE_CODE0        0x63	/*Level2 Mstr (bits 7-6),  */

#define  SLV_TYPE_CODE0    0xA3	/*Priority Bit set (bits 7-6),  */

#define  ASSIGN_ID   0x00
#define  SET_P_FLAG  0x01
#define  CFG_CMPLT   0x03
#define  DOM_MSTR    0x0F
#define  SYNC_PTRN   0x1F

#define  ID_0_7      0x18
#define  ID_8_F      0x11
#define  MISC_CODE   0x14
#define  CLR_P_FLAG  0x18

#define  INIT_SELTD  0x01
#define  LEVEL2_TAR  0x02

enum scam_id_st { ID0, ID1, ID2, ID3, ID4, ID5, ID6, ID7, ID8, ID9, ID10, ID11,
	    ID12,
	ID13, ID14, ID15, ID_UNUSED, ID_UNASSIGNED, ID_ASSIGNED, LEGACY,
	CLR_PRIORITY, NO_ID_AVAIL
};

typedef struct SCCBscam_info {

	unsigned char id_string[ID_STRING_LENGTH];
	enum scam_id_st state;

} SCCBSCAM_INFO;

#define  SCSI_REQUEST_SENSE      0x03
#define  SCSI_READ               0x08
#define  SCSI_WRITE              0x0A
#define  SCSI_START_STOP_UNIT    0x1B
#define  SCSI_READ_EXTENDED      0x28
#define  SCSI_WRITE_EXTENDED     0x2A
#define  SCSI_WRITE_AND_VERIFY   0x2E

#define  SSGOOD                  0x00
#define  SSCHECK                 0x02
#define  SSQ_FULL                0x28

#define  SMCMD_COMP              0x00
#define  SMEXT                   0x01
#define  SMSAVE_DATA_PTR         0x02
#define  SMREST_DATA_PTR         0x03
#define  SMDISC                  0x04
#define  SMABORT                 0x06
#define  SMREJECT                0x07
#define  SMNO_OP                 0x08
#define  SMPARITY                0x09
#define  SMDEV_RESET             0x0C
#define	SMABORT_TAG					0x0D
#define	SMINIT_RECOVERY			0x0F
#define	SMREL_RECOVERY				0x10

#define  SMIDENT                 0x80
#define  DISC_PRIV               0x40

#define  SMSYNC                  0x01
#define  SMWDTR                  0x03
#define  SM8BIT                  0x00
#define  SM16BIT                 0x01
#define  SMIGNORWR               0x23	/* Ignore Wide Residue */

#define  SIX_BYTE_CMD            0x06
#define  TWELVE_BYTE_CMD         0x0C

#define  ASYNC                   0x00
#define  MAX_OFFSET              0x0F	/* Maxbyteoffset for Sync Xfers */

#define  EEPROM_WD_CNT     256

#define  EEPROM_CHECK_SUM  0
#define  FW_SIGNATURE      2
#define  MODEL_NUMB_0      4
#define  MODEL_NUMB_2      6
#define  MODEL_NUMB_4      8
#define  SYSTEM_CONFIG     16
#define  SCSI_CONFIG       17
#define  BIOS_CONFIG       18
#define  SCAM_CONFIG       20
#define  ADAPTER_SCSI_ID   24

#define  IGNORE_B_SCAN     32
#define  SEND_START_ENA    34
#define  DEVICE_ENABLE     36

#define  SYNC_RATE_TBL     38
#define  SYNC_RATE_TBL01   38
#define  SYNC_RATE_TBL23   40
#define  SYNC_RATE_TBL45   42
#define  SYNC_RATE_TBL67   44
#define  SYNC_RATE_TBL89   46
#define  SYNC_RATE_TBLab   48
#define  SYNC_RATE_TBLcd   50
#define  SYNC_RATE_TBLef   52

#define  EE_SCAMBASE      256

#define  SCAM_ENABLED   BIT(2)
#define  SCAM_LEVEL2    BIT(3)

#define	RENEGO_ENA		BIT(10)
#define	CONNIO_ENA		BIT(11)
#define  GREEN_PC_ENA   BIT(12)

#define  AUTO_RATE_00   00
#define  AUTO_RATE_05   01
#define  AUTO_RATE_10   02
#define  AUTO_RATE_20   03

#define  WIDE_NEGO_BIT     BIT(7)
#define  DISC_ENABLE_BIT   BIT(6)

#define  hp_vendor_id_0       0x00	/* LSB */
#define  ORION_VEND_0   0x4B

#define  hp_vendor_id_1       0x01	/* MSB */
#define  ORION_VEND_1   0x10

#define  hp_device_id_0       0x02	/* LSB */
#define  ORION_DEV_0    0x30

#define  hp_device_id_1       0x03	/* MSB */
#define  ORION_DEV_1    0x81

	/* Sub Vendor ID and Sub Device ID only available in
	   Harpoon Version 2 and higher */

#define  hp_sub_device_id_0   0x06	/* LSB */

#define  hp_semaphore         0x0C
#define SCCB_MGR_ACTIVE    BIT(0)
#define TICKLE_ME          BIT(1)
#define SCCB_MGR_PRESENT   BIT(3)
#define BIOS_IN_USE        BIT(4)

#define  hp_sys_ctrl          0x0F

#define  STOP_CLK          BIT(0)	/*Turn off BusMaster Clock */
#define  DRVR_RST          BIT(1)	/*Firmware Reset to 80C15 chip */
#define  HALT_MACH         BIT(3)	/*Halt State Machine      */
#define  HARD_ABORT        BIT(4)	/*Hard Abort              */

#define  hp_host_blk_cnt      0x13

#define  XFER_BLK64        0x06	/*     1 1 0 64 byte per block */

#define  BM_THRESHOLD      0x40	/* PCI mode can only xfer 16 bytes */

#define  hp_int_mask          0x17

#define  INT_CMD_COMPL     BIT(0)	/* DMA command complete   */
#define  INT_EXT_STATUS    BIT(1)	/* Extended Status Set    */

#define  hp_xfer_cnt_lo       0x18
#define  hp_xfer_cnt_hi       0x1A
#define  hp_xfer_cmd          0x1B

#define  XFER_HOST_DMA     0x00	/*     0 0 0 Transfer Host -> DMA */
#define  XFER_DMA_HOST     0x01	/*     0 0 1 Transfer DMA  -> Host */

#define  XFER_HOST_AUTO    0x00	/*     0 0 Auto Transfer Size   */

#define  XFER_DMA_8BIT     0x20	/*     0 1 8 BIT  Transfer Size */

#define  DISABLE_INT       BIT(7)	/*Do not interrupt at end of cmd. */

#define  HOST_WRT_CMD      ((DISABLE_INT + XFER_HOST_DMA + XFER_HOST_AUTO + XFER_DMA_8BIT))
#define  HOST_RD_CMD       ((DISABLE_INT + XFER_DMA_HOST + XFER_HOST_AUTO + XFER_DMA_8BIT))

#define  hp_host_addr_lo      0x1C
#define  hp_host_addr_hmi     0x1E

#define  hp_ee_ctrl           0x22

#define  EXT_ARB_ACK       BIT(7)
#define  SCSI_TERM_ENA_H   BIT(6)	/* SCSI high byte terminator */
#define  SEE_MS            BIT(5)
#define  SEE_CS            BIT(3)
#define  SEE_CLK           BIT(2)
#define  SEE_DO            BIT(1)
#define  SEE_DI            BIT(0)

#define  EE_READ           0x06
#define  EE_WRITE          0x05
#define  EWEN              0x04
#define  EWEN_ADDR         0x03C0
#define  EWDS              0x04
#define  EWDS_ADDR         0x0000

#define  hp_bm_ctrl           0x26

#define  SCSI_TERM_ENA_L   BIT(0)	/*Enable/Disable external terminators */
#define  FLUSH_XFER_CNTR   BIT(1)	/*Flush transfer counter */
#define  FORCE1_XFER       BIT(5)	/*Always xfer one byte in byte mode */
#define  FAST_SINGLE       BIT(6)	/*?? */

#define  BMCTRL_DEFAULT    (FORCE1_XFER|FAST_SINGLE|SCSI_TERM_ENA_L)

#define  hp_sg_addr           0x28
#define  hp_page_ctrl         0x29

#define  SCATTER_EN        BIT(0)
#define  SGRAM_ARAM        BIT(1)
#define  G_INT_DISABLE     BIT(3)	/* Enable/Disable all Interrupts */
#define  NARROW_SCSI_CARD  BIT(4)	/* NARROW/WIDE SCSI config pin */

#define  hp_pci_stat_cfg      0x2D

#define  REC_MASTER_ABORT  BIT(5)	/*received Master abort */

#define  hp_rev_num           0x33

#define  hp_stack_data        0x34
#define  hp_stack_addr        0x35

#define  hp_ext_status        0x36

#define  BM_FORCE_OFF      BIT(0)	/*Bus Master is forced to get off */
#define  PCI_TGT_ABORT     BIT(0)	/*PCI bus master transaction aborted */
#define  PCI_DEV_TMOUT     BIT(1)	/*PCI Device Time out */
#define  CMD_ABORTED       BIT(4)	/*Command aborted */
#define  BM_PARITY_ERR     BIT(5)	/*parity error on data received   */
#define  PIO_OVERRUN       BIT(6)	/*Slave data overrun */
#define  BM_CMD_BUSY       BIT(7)	/*Bus master transfer command busy */
#define  BAD_EXT_STATUS    (BM_FORCE_OFF | PCI_DEV_TMOUT | CMD_ABORTED | \
                                  BM_PARITY_ERR | PIO_OVERRUN)

#define  hp_int_status        0x37

#define  EXT_STATUS_ON     BIT(1)	/*Extended status is valid */
#define  SCSI_INTERRUPT    BIT(2)	/*Global indication of a SCSI int. */
#define  INT_ASSERTED      BIT(5)	/* */

#define  hp_fifo_cnt          0x38

#define  hp_intena		 0x40

#define  RESET		 BIT(7)
#define  PROG_HLT		 BIT(6)
#define  PARITY		 BIT(5)
#define  FIFO		 BIT(4)
#define  SEL		 BIT(3)
#define  SCAM_SEL		 BIT(2)
#define  RSEL		 BIT(1)
#define  TIMEOUT		 BIT(0)
#define  BUS_FREE		 BIT(15)
#define  XFER_CNT_0	 BIT(14)
#define  PHASE		 BIT(13)
#define  IUNKWN		 BIT(12)
#define  ICMD_COMP	 BIT(11)
#define  ITICKLE		 BIT(10)
#define  IDO_STRT		 BIT(9)
#define  ITAR_DISC	 BIT(8)
#define  AUTO_INT		 (BIT(12)+BIT(11)+BIT(10)+BIT(9)+BIT(8))
#define  CLR_ALL_INT	 0xFFFF
#define  CLR_ALL_INT_1	 0xFF00

#define  hp_intstat		 0x42

#define  hp_scsisig           0x44

#define  SCSI_SEL          BIT(7)
#define  SCSI_BSY          BIT(6)
#define  SCSI_REQ          BIT(5)
#define  SCSI_ACK          BIT(4)
#define  SCSI_ATN          BIT(3)
#define  SCSI_CD           BIT(2)
#define  SCSI_MSG          BIT(1)
#define  SCSI_IOBIT        BIT(0)

#define  S_SCSI_PHZ        (BIT(2)+BIT(1)+BIT(0))
#define  S_MSGO_PH         (BIT(2)+BIT(1)       )
#define  S_MSGI_PH         (BIT(2)+BIT(1)+BIT(0))
#define  S_DATAI_PH        (              BIT(0))
#define  S_DATAO_PH        0x00
#define  S_ILL_PH          (       BIT(1)       )

#define  hp_scsictrl_0        0x45

#define  SEL_TAR           BIT(6)
#define  ENA_ATN           BIT(4)
#define  ENA_RESEL         BIT(2)
#define  SCSI_RST          BIT(1)
#define  ENA_SCAM_SEL      BIT(0)

#define  hp_portctrl_0        0x46

#define  SCSI_PORT         BIT(7)
#define  SCSI_INBIT        BIT(6)
#define  DMA_PORT          BIT(5)
#define  DMA_RD            BIT(4)
#define  HOST_PORT         BIT(3)
#define  HOST_WRT          BIT(2)
#define  SCSI_BUS_EN       BIT(1)
#define  START_TO          BIT(0)

#define  hp_scsireset         0x47

#define  SCSI_INI          BIT(6)
#define  SCAM_EN           BIT(5)
#define  DMA_RESET         BIT(3)
#define  HPSCSI_RESET      BIT(2)
#define  PROG_RESET        BIT(1)
#define  FIFO_CLR          BIT(0)

#define  hp_xfercnt_0         0x48
#define  hp_xfercnt_2         0x4A

#define  hp_fifodata_0        0x4C
#define  hp_addstat           0x4E

#define  SCAM_TIMER        BIT(7)
#define  SCSI_MODE8        BIT(3)
#define  SCSI_PAR_ERR      BIT(0)

#define  hp_prgmcnt_0         0x4F

#define  hp_selfid_0          0x50
#define  hp_selfid_1          0x51
#define  hp_arb_id            0x52

#define  hp_select_id         0x53

#define  hp_synctarg_base     0x54
#define  hp_synctarg_12       0x54
#define  hp_synctarg_13       0x55
#define  hp_synctarg_14       0x56
#define  hp_synctarg_15       0x57

#define  hp_synctarg_8        0x58
#define  hp_synctarg_9        0x59
#define  hp_synctarg_10       0x5A
#define  hp_synctarg_11       0x5B

#define  hp_synctarg_4        0x5C
#define  hp_synctarg_5        0x5D
#define  hp_synctarg_6        0x5E
#define  hp_synctarg_7        0x5F

#define  hp_synctarg_0        0x60
#define  hp_synctarg_1        0x61
#define  hp_synctarg_2        0x62
#define  hp_synctarg_3        0x63

#define  NARROW_SCSI       BIT(4)
#define  DEFAULT_OFFSET    0x0F

#define  hp_autostart_0       0x64
#define  hp_autostart_1       0x65
#define  hp_autostart_3       0x67

#define  AUTO_IMMED    BIT(5)
#define  SELECT   BIT(6)
#define  END_DATA (BIT(7)+BIT(6))

#define  hp_gp_reg_0          0x68
#define  hp_gp_reg_1          0x69
#define  hp_gp_reg_3          0x6B

#define  hp_seltimeout        0x6C

#define  TO_4ms            0x67	/* 3.9959ms */

#define  TO_5ms            0x03	/* 4.9152ms */
#define  TO_10ms           0x07	/* 11.xxxms */
#define  TO_250ms          0x99	/* 250.68ms */
#define  TO_290ms          0xB1	/* 289.99ms */

#define  hp_clkctrl_0         0x6D

#define  PWR_DWN           BIT(6)
#define  ACTdeassert       BIT(4)
#define  CLK_40MHZ         (BIT(1) + BIT(0))

#define  CLKCTRL_DEFAULT   (ACTdeassert | CLK_40MHZ)

#define  hp_fiforead          0x6E
#define  hp_fifowrite         0x6F

#define  hp_offsetctr         0x70
#define  hp_xferstat          0x71

#define  FIFO_EMPTY        BIT(6)

#define  hp_portctrl_1        0x72

#define  CHK_SCSI_P        BIT(3)
#define  HOST_MODE8        BIT(0)

#define  hp_xfer_pad          0x73

#define  ID_UNLOCK         BIT(3)

#define  hp_scsidata_0        0x74
#define  hp_scsidata_1        0x75

#define  hp_aramBase          0x80
#define  BIOS_DATA_OFFSET     0x60
#define  BIOS_RELATIVE_CARD   0x64

#define  AR3      (BIT(9) + BIT(8))
#define  SDATA    BIT(10)

#define  CRD_OP   BIT(11)	/* Cmp Reg. w/ Data */

#define  CRR_OP   BIT(12)	/* Cmp Reg. w. Reg. */

#define  CPE_OP   (BIT(14)+BIT(11))	/* Cmp SCSI phs & Branch EQ */

#define  CPN_OP   (BIT(14)+BIT(12))	/* Cmp SCSI phs & Branch NOT EQ */

#define  ADATA_OUT   0x00
#define  ADATA_IN    BIT(8)
#define  ACOMMAND    BIT(10)
#define  ASTATUS     (BIT(10)+BIT(8))
#define  AMSG_OUT    (BIT(10)+BIT(9))
#define  AMSG_IN     (BIT(10)+BIT(9)+BIT(8))

#define  BRH_OP   BIT(13)	/* Branch */

#define  ALWAYS   0x00
#define  EQUAL    BIT(8)
#define  NOT_EQ   BIT(9)

#define  TCB_OP   (BIT(13)+BIT(11))	/* Test condition & branch */

#define  FIFO_0      BIT(10)

#define  MPM_OP   BIT(15)	/* Match phase and move data */

#define  MRR_OP   BIT(14)	/* Move DReg. to Reg. */

#define  S_IDREG  (BIT(2)+BIT(1)+BIT(0))

#define  D_AR0    0x00
#define  D_AR1    BIT(0)
#define  D_BUCKET (BIT(2) + BIT(1) + BIT(0))

#define  RAT_OP      (BIT(14)+BIT(13)+BIT(11))

#define  SSI_OP      (BIT(15)+BIT(11))

#define  SSI_ITAR_DISC	(ITAR_DISC >> 8)
#define  SSI_IDO_STRT	(IDO_STRT >> 8)

#define  SSI_ICMD_COMP	(ICMD_COMP >> 8)
#define  SSI_ITICKLE	(ITICKLE >> 8)

#define  SSI_IUNKWN	(IUNKWN >> 8)
#define  SSI_INO_CC	(IUNKWN >> 8)
#define  SSI_IRFAIL	(IUNKWN >> 8)

#define  NP    0x10		/*Next Phase */
#define  NTCMD 0x02		/*Non- Tagged Command start */
#define  CMDPZ 0x04		/*Command phase */
#define  DINT  0x12		/*Data Out/In interrupt */
#define  DI    0x13		/*Data Out */
#define  DC    0x19		/*Disconnect Message */
#define  ST    0x1D		/*Status Phase */
#define  UNKNWN 0x24		/*Unknown bus action */
#define  CC    0x25		/*Command Completion failure */
#define  TICK  0x26		/*New target reselected us. */
#define  SELCHK 0x28		/*Select & Check SCSI ID latch reg */

#define  ID_MSG_STRT    hp_aramBase + 0x00
#define  NON_TAG_ID_MSG hp_aramBase + 0x06
#define  CMD_STRT       hp_aramBase + 0x08
#define  SYNC_MSGS      hp_aramBase + 0x08

#define  TAG_STRT          0x00
#define  DISCONNECT_START  0x10/2
#define  END_DATA_START    0x14/2
#define  CMD_ONLY_STRT     CMDPZ/2
#define  SELCHK_STRT     SELCHK/2

#define GET_XFER_CNT(port, xfercnt) {RD_HARP32(port,hp_xfercnt_0,xfercnt); xfercnt &= 0xFFFFFF;}
/* #define GET_XFER_CNT(port, xfercnt) (xfercnt = RD_HARPOON(port+hp_xfercnt_2), \
                                 xfercnt <<= 16,\
                                 xfercnt |= RDW_HARPOON((unsigned short)(port+hp_xfercnt_0)))
 */
#define HP_SETUP_ADDR_CNT(port,addr,count) (WRW_HARPOON((port+hp_host_addr_lo), (unsigned short)(addr & 0x0000FFFFL)),\
         addr >>= 16,\
         WRW_HARPOON((port+hp_host_addr_hmi), (unsigned short)(addr & 0x0000FFFFL)),\
         WR_HARP32(port,hp_xfercnt_0,count),\
         WRW_HARPOON((port+hp_xfer_cnt_lo), (unsigned short)(count & 0x0000FFFFL)),\
         count >>= 16,\
         WR_HARPOON(port+hp_xfer_cnt_hi, (count & 0xFF)))

#define ACCEPT_MSG(port) {while(RD_HARPOON(port+hp_scsisig) & SCSI_REQ){}\
                          WR_HARPOON(port+hp_scsisig, S_ILL_PH);}

#define ACCEPT_MSG_ATN(port) {while(RD_HARPOON(port+hp_scsisig) & SCSI_REQ){}\
                          WR_HARPOON(port+hp_scsisig, (S_ILL_PH|SCSI_ATN));}

#define DISABLE_AUTO(port) (WR_HARPOON(port+hp_scsireset, PROG_RESET),\
                        WR_HARPOON(port+hp_scsireset, 0x00))

#define ARAM_ACCESS(p_port) (WR_HARPOON(p_port+hp_page_ctrl, \
                             (RD_HARPOON(p_port+hp_page_ctrl) | SGRAM_ARAM)))

#define SGRAM_ACCESS(p_port) (WR_HARPOON(p_port+hp_page_ctrl, \
                             (RD_HARPOON(p_port+hp_page_ctrl) & ~SGRAM_ARAM)))

#define MDISABLE_INT(p_port) (WR_HARPOON(p_port+hp_page_ctrl, \
                             (RD_HARPOON(p_port+hp_page_ctrl) | G_INT_DISABLE)))

#define MENABLE_INT(p_port) (WR_HARPOON(p_port+hp_page_ctrl, \
                             (RD_HARPOON(p_port+hp_page_ctrl) & ~G_INT_DISABLE)))

static unsigned char FPT_sisyncn(unsigned long port, unsigned char p_card,
				 unsigned char syncFlag);
static void FPT_ssel(unsigned long port, unsigned char p_card);
static void FPT_sres(unsigned long port, unsigned char p_card,
		     struct sccb_card *pCurrCard);
static void FPT_shandem(unsigned long port, unsigned char p_card,
			struct sccb *pCurrSCCB);
static void FPT_stsyncn(unsigned long port, unsigned char p_card);
static void FPT_sisyncr(unsigned long port, unsigned char sync_pulse,
			unsigned char offset);
static void FPT_sssyncv(unsigned long p_port, unsigned char p_id,
			unsigned char p_sync_value,
			struct sccb_mgr_tar_info *currTar_Info);
static void FPT_sresb(unsigned long port, unsigned char p_card);
static void FPT_sxfrp(unsigned long p_port, unsigned char p_card);
static void FPT_schkdd(unsigned long port, unsigned char p_card);
static unsigned char FPT_RdStack(unsigned long port, unsigned char index);
static void FPT_WrStack(unsigned long portBase, unsigned char index,
			unsigned char data);
static unsigned char FPT_ChkIfChipInitialized(unsigned long ioPort);

static void FPT_SendMsg(unsigned long port, unsigned char message);
static void FPT_queueFlushTargSccb(unsigned char p_card, unsigned char thisTarg,
				   unsigned char error_code);

static void FPT_sinits(struct sccb *p_sccb, unsigned char p_card);
static void FPT_RNVRamData(struct nvram_info *pNvRamInfo);

static unsigned char FPT_siwidn(unsigned long port, unsigned char p_card);
static void FPT_stwidn(unsigned long port, unsigned char p_card);
static void FPT_siwidr(unsigned long port, unsigned char width);

static void FPT_queueSelectFail(struct sccb_card *pCurrCard,
				unsigned char p_card);
static void FPT_queueDisconnect(struct sccb *p_SCCB, unsigned char p_card);
static void FPT_queueCmdComplete(struct sccb_card *pCurrCard,
				 struct sccb *p_SCCB, unsigned char p_card);
static void FPT_queueSearchSelect(struct sccb_card *pCurrCard,
				  unsigned char p_card);
static void FPT_queueFlushSccb(unsigned char p_card, unsigned char error_code);
static void FPT_queueAddSccb(struct sccb *p_SCCB, unsigned char card);
static unsigned char FPT_queueFindSccb(struct sccb *p_SCCB,
				       unsigned char p_card);
static void FPT_utilUpdateResidual(struct sccb *p_SCCB);
static unsigned short FPT_CalcCrc16(unsigned char buffer[]);
static unsigned char FPT_CalcLrc(unsigned char buffer[]);

static void FPT_Wait1Second(unsigned long p_port);
static void FPT_Wait(unsigned long p_port, unsigned char p_delay);
static void FPT_utilEEWriteOnOff(unsigned long p_port, unsigned char p_mode);
static void FPT_utilEEWrite(unsigned long p_port, unsigned short ee_data,
			    unsigned short ee_addr);
static unsigned short FPT_utilEERead(unsigned long p_port,
				     unsigned short ee_addr);
static unsigned short FPT_utilEEReadOrg(unsigned long p_port,
					unsigned short ee_addr);
static void FPT_utilEESendCmdAddr(unsigned long p_port, unsigned char ee_cmd,
				  unsigned short ee_addr);

static void FPT_phaseDataOut(unsigned long port, unsigned char p_card);
static void FPT_phaseDataIn(unsigned long port, unsigned char p_card);
static void FPT_phaseCommand(unsigned long port, unsigned char p_card);
static void FPT_phaseStatus(unsigned long port, unsigned char p_card);
static void FPT_phaseMsgOut(unsigned long port, unsigned char p_card);
static void FPT_phaseMsgIn(unsigned long port, unsigned char p_card);
static void FPT_phaseIllegal(unsigned long port, unsigned char p_card);

static void FPT_phaseDecode(unsigned long port, unsigned char p_card);
static void FPT_phaseChkFifo(unsigned long port, unsigned char p_card);
static void FPT_phaseBusFree(unsigned long p_port, unsigned char p_card);

static void FPT_XbowInit(unsigned long port, unsigned char scamFlg);
static void FPT_BusMasterInit(unsigned long p_port);
static void FPT_DiagEEPROM(unsigned long p_port);

static void FPT_dataXferProcessor(unsigned long port,
				  struct sccb_card *pCurrCard);
static void FPT_busMstrSGDataXferStart(unsigned long port,
				       struct sccb *pCurrSCCB);
static void FPT_busMstrDataXferStart(unsigned long port,
				     struct sccb *pCurrSCCB);
static void FPT_hostDataXferAbort(unsigned long port, unsigned char p_card,
				  struct sccb *pCurrSCCB);
static void FPT_hostDataXferRestart(struct sccb *currSCCB);

static unsigned char FPT_SccbMgr_bad_isr(unsigned long p_port,
					 unsigned char p_card,
					 struct sccb_card *pCurrCard,
					 unsigned short p_int);

static void FPT_SccbMgrTableInitAll(void);
static void FPT_SccbMgrTableInitCard(struct sccb_card *pCurrCard,
				     unsigned char p_card);
static void FPT_SccbMgrTableInitTarget(unsigned char p_card,
				       unsigned char target);

static void FPT_scini(unsigned char p_card, unsigned char p_our_id,
		      unsigned char p_power_up);

static int FPT_scarb(unsigned long p_port, unsigned char p_sel_type);
static void FPT_scbusf(unsigned long p_port);
static void FPT_scsel(unsigned long p_port);
static void FPT_scasid(unsigned char p_card, unsigned long p_port);
static unsigned char FPT_scxferc(unsigned long p_port, unsigned char p_data);
static unsigned char FPT_scsendi(unsigned long p_port,
				 unsigned char p_id_string[]);
static unsigned char FPT_sciso(unsigned long p_port,
			       unsigned char p_id_string[]);
static void FPT_scwirod(unsigned long p_port, unsigned char p_data_bit);
static void FPT_scwiros(unsigned long p_port, unsigned char p_data_bit);
static unsigned char FPT_scvalq(unsigned char p_quintet);
static unsigned char FPT_scsell(unsigned long p_port, unsigned char targ_id);
static void FPT_scwtsel(unsigned long p_port);
static void FPT_inisci(unsigned char p_card, unsigned long p_port,
		       unsigned char p_our_id);
static void FPT_scsavdi(unsigned char p_card, unsigned long p_port);
static unsigned char FPT_scmachid(unsigned char p_card,
				  unsigned char p_id_string[]);

static void FPT_autoCmdCmplt(unsigned long p_port, unsigned char p_card);
static void FPT_autoLoadDefaultMap(unsigned long p_port);

static struct sccb_mgr_tar_info FPT_sccbMgrTbl[MAX_CARDS][MAX_SCSI_TAR] =
    { {{0}} };
static struct sccb_card FPT_BL_Card[MAX_CARDS] = { {0} };
static SCCBSCAM_INFO FPT_scamInfo[MAX_SCSI_TAR] = { {{0}} };
static struct nvram_info FPT_nvRamInfo[MAX_MB_CARDS] = { {0} };

static unsigned char FPT_mbCards = 0;
static unsigned char FPT_scamHAString[] =
    { 0x63, 0x07, 'B', 'U', 'S', 'L', 'O', 'G', 'I', 'C',
	' ', 'B', 'T', '-', '9', '3', '0',
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
};

static unsigned short FPT_default_intena = 0;

static void (*FPT_s_PhaseTbl[8]) (unsigned long, unsigned char) = {
0};

/*---------------------------------------------------------------------
 *
 * Function: FlashPoint_ProbeHostAdapter
 *
 * Description: Setup and/or Search for cards and return info to caller.
 *
 *---------------------------------------------------------------------*/

static int FlashPoint_ProbeHostAdapter(struct sccb_mgr_info *pCardInfo)
{
	static unsigned char first_time = 1;

	unsigned char i, j, id, ScamFlg;
	unsigned short temp, temp2, temp3, temp4, temp5, temp6;
	unsigned long ioport;
	struct nvram_info *pCurrNvRam;

	ioport = pCardInfo->si_baseaddr;

	if (RD_HARPOON(ioport + hp_vendor_id_0) != ORION_VEND_0)
		return (int)FAILURE;

	if ((RD_HARPOON(ioport + hp_vendor_id_1) != ORION_VEND_1))
		return (int)FAILURE;

	if ((RD_HARPOON(ioport + hp_device_id_0) != ORION_DEV_0))
		return (int)FAILURE;

	if ((RD_HARPOON(ioport + hp_device_id_1) != ORION_DEV_1))
		return (int)FAILURE;

	if (RD_HARPOON(ioport + hp_rev_num) != 0x0f) {

/* For new Harpoon then check for sub_device ID LSB
   the bits(0-3) must be all ZERO for compatible with
   current version of SCCBMgr, else skip this Harpoon
	device. */

		if (RD_HARPOON(ioport + hp_sub_device_id_0) & 0x0f)
			return (int)FAILURE;
	}

	if (first_time) {
		FPT_SccbMgrTableInitAll();
		first_time = 0;
		FPT_mbCards = 0;
	}

	if (FPT_RdStack(ioport, 0) != 0x00) {
		if (FPT_ChkIfChipInitialized(ioport) == 0) {
			pCurrNvRam = NULL;
			WR_HARPOON(ioport + hp_semaphore, 0x00);
			FPT_XbowInit(ioport, 0);	/*Must Init the SCSI before attempting */
			FPT_DiagEEPROM(ioport);
		} else {
			if (FPT_mbCards < MAX_MB_CARDS) {
				pCurrNvRam = &FPT_nvRamInfo[FPT_mbCards];
				FPT_mbCards++;
				pCurrNvRam->niBaseAddr = ioport;
				FPT_RNVRamData(pCurrNvRam);
			} else
				return (int)FAILURE;
		}
	} else
		pCurrNvRam = NULL;

	WR_HARPOON(ioport + hp_clkctrl_0, CLKCTRL_DEFAULT);
	WR_HARPOON(ioport + hp_sys_ctrl, 0x00);

	if (pCurrNvRam)
		pCardInfo->si_id = pCurrNvRam->niAdapId;
	else
		pCardInfo->si_id =
		    (unsigned
		     char)(FPT_utilEERead(ioport,
					  (ADAPTER_SCSI_ID /
					   2)) & (unsigned char)0x0FF);

	pCardInfo->si_lun = 0x00;
	pCardInfo->si_fw_revision = ORION_FW_REV;
	temp2 = 0x0000;
	temp3 = 0x0000;
	temp4 = 0x0000;
	temp5 = 0x0000;
	temp6 = 0x0000;

	for (id = 0; id < (16 / 2); id++) {

		if (pCurrNvRam) {
			temp = (unsigned short)pCurrNvRam->niSyncTbl[id];
			temp = ((temp & 0x03) + ((temp << 4) & 0xc0)) +
			    (((temp << 4) & 0x0300) + ((temp << 8) & 0xc000));
		} else
			temp =
			    FPT_utilEERead(ioport,
					   (unsigned short)((SYNC_RATE_TBL / 2)
							    + id));

		for (i = 0; i < 2; temp >>= 8, i++) {

			temp2 >>= 1;
			temp3 >>= 1;
			temp4 >>= 1;
			temp5 >>= 1;
			temp6 >>= 1;
			switch (temp & 0x3) {
			case AUTO_RATE_20:	/* Synchronous, 20 mega-transfers/second */
				temp6 |= 0x8000;	/* Fall through */
			case AUTO_RATE_10:	/* Synchronous, 10 mega-transfers/second */
				temp5 |= 0x8000;	/* Fall through */
			case AUTO_RATE_05:	/* Synchronous, 5 mega-transfers/second */
				temp2 |= 0x8000;	/* Fall through */
			case AUTO_RATE_00:	/* Asynchronous */
				break;
			}

			if (temp & DISC_ENABLE_BIT)
				temp3 |= 0x8000;

			if (temp & WIDE_NEGO_BIT)
				temp4 |= 0x8000;

		}
	}

	pCardInfo->si_per_targ_init_sync = temp2;
	pCardInfo->si_per_targ_no_disc = temp3;
	pCardInfo->si_per_targ_wide_nego = temp4;
	pCardInfo->si_per_targ_fast_nego = temp5;
	pCardInfo->si_per_targ_ultra_nego = temp6;

	if (pCurrNvRam)
		i = pCurrNvRam->niSysConf;
	else
		i = (unsigned
		     char)(FPT_utilEERead(ioport, (SYSTEM_CONFIG / 2)));

	if (pCurrNvRam)
		ScamFlg = pCurrNvRam->niScamConf;
	else
		ScamFlg =
		    (unsigned char)FPT_utilEERead(ioport, SCAM_CONFIG / 2);

	pCardInfo->si_flags = 0x0000;

	if (i & 0x01)
		pCardInfo->si_flags |= SCSI_PARITY_ENA;

	if (!(i & 0x02))
		pCardInfo->si_flags |= SOFT_RESET;

	if (i & 0x10)
		pCardInfo->si_flags |= EXTENDED_TRANSLATION;

	if (ScamFlg & SCAM_ENABLED)
		pCardInfo->si_flags |= FLAG_SCAM_ENABLED;

	if (ScamFlg & SCAM_LEVEL2)
		pCardInfo->si_flags |= FLAG_SCAM_LEVEL2;

	j = (RD_HARPOON(ioport + hp_bm_ctrl) & ~SCSI_TERM_ENA_L);
	if (i & 0x04) {
		j |= SCSI_TERM_ENA_L;
	}
	WR_HARPOON(ioport + hp_bm_ctrl, j);

	j = (RD_HARPOON(ioport + hp_ee_ctrl) & ~SCSI_TERM_ENA_H);
	if (i & 0x08) {
		j |= SCSI_TERM_ENA_H;
	}
	WR_HARPOON(ioport + hp_ee_ctrl, j);

	if (!(RD_HARPOON(ioport + hp_page_ctrl) & NARROW_SCSI_CARD))

		pCardInfo->si_flags |= SUPPORT_16TAR_32LUN;

	pCardInfo->si_card_family = HARPOON_FAMILY;
	pCardInfo->si_bustype = BUSTYPE_PCI;

	if (pCurrNvRam) {
		pCardInfo->si_card_model[0] = '9';
		switch (pCurrNvRam->niModel & 0x0f) {
		case MODEL_LT:
			pCardInfo->si_card_model[1] = '3';
			pCardInfo->si_card_model[2] = '0';
			break;
		case MODEL_LW:
			pCardInfo->si_card_model[1] = '5';
			pCardInfo->si_card_model[2] = '0';
			break;
		case MODEL_DL:
			pCardInfo->si_card_model[1] = '3';
			pCardInfo->si_card_model[2] = '2';
			break;
		case MODEL_DW:
			pCardInfo->si_card_model[1] = '5';
			pCardInfo->si_card_model[2] = '2';
			break;
		}
	} else {
		temp = FPT_utilEERead(ioport, (MODEL_NUMB_0 / 2));
		pCardInfo->si_card_model[0] = (unsigned char)(temp >> 8);
		temp = FPT_utilEERead(ioport, (MODEL_NUMB_2 / 2));

		pCardInfo->si_card_model[1] = (unsigned char)(temp & 0x00FF);
		pCardInfo->si_card_model[2] = (unsigned char)(temp >> 8);
	}

	if (pCardInfo->si_card_model[1] == '3') {
		if (RD_HARPOON(ioport + hp_ee_ctrl) & BIT(7))
			pCardInfo->si_flags |= LOW_BYTE_TERM;
	} else if (pCardInfo->si_card_model[2] == '0') {
		temp = RD_HARPOON(ioport + hp_xfer_pad);
		WR_HARPOON(ioport + hp_xfer_pad, (temp & ~BIT(4)));
		if (RD_HARPOON(ioport + hp_ee_ctrl) & BIT(7))
			pCardInfo->si_flags |= LOW_BYTE_TERM;
		WR_HARPOON(ioport + hp_xfer_pad, (temp | BIT(4)));
		if (RD_HARPOON(ioport + hp_ee_ctrl) & BIT(7))
			pCardInfo->si_flags |= HIGH_BYTE_TERM;
		WR_HARPOON(ioport + hp_xfer_pad, temp);
	} else {
		temp = RD_HARPOON(ioport + hp_ee_ctrl);
		temp2 = RD_HARPOON(ioport + hp_xfer_pad);
		WR_HARPOON(ioport + hp_ee_ctrl, (temp | SEE_CS));
		WR_HARPOON(ioport + hp_xfer_pad, (temp2 | BIT(4)));
		temp3 = 0;
		for (i = 0; i < 8; i++) {
			temp3 <<= 1;
			if (!(RD_HARPOON(ioport + hp_ee_ctrl) & BIT(7)))
				temp3 |= 1;
			WR_HARPOON(ioport + hp_xfer_pad, (temp2 & ~BIT(4)));
			WR_HARPOON(ioport + hp_xfer_pad, (temp2 | BIT(4)));
		}
		WR_HARPOON(ioport + hp_ee_ctrl, temp);
		WR_HARPOON(ioport + hp_xfer_pad, temp2);
		if (!(temp3 & BIT(7)))
			pCardInfo->si_flags |= LOW_BYTE_TERM;
		if (!(temp3 & BIT(6)))
			pCardInfo->si_flags |= HIGH_BYTE_TERM;
	}

	ARAM_ACCESS(ioport);

	for (i = 0; i < 4; i++) {

		pCardInfo->si_XlatInfo[i] =
		    RD_HARPOON(ioport + hp_aramBase + BIOS_DATA_OFFSET + i);
	}

	/* return with -1 if no sort, else return with
	   logical card number sorted by BIOS (zero-based) */

	pCardInfo->si_relative_cardnum =
	    (unsigned
	     char)(RD_HARPOON(ioport + hp_aramBase + BIOS_RELATIVE_CARD) - 1);

	SGRAM_ACCESS(ioport);

	FPT_s_PhaseTbl[0] = FPT_phaseDataOut;
	FPT_s_PhaseTbl[1] = FPT_phaseDataIn;
	FPT_s_PhaseTbl[2] = FPT_phaseIllegal;
	FPT_s_PhaseTbl[3] = FPT_phaseIllegal;
	FPT_s_PhaseTbl[4] = FPT_phaseCommand;
	FPT_s_PhaseTbl[5] = FPT_phaseStatus;
	FPT_s_PhaseTbl[6] = FPT_phaseMsgOut;
	FPT_s_PhaseTbl[7] = FPT_phaseMsgIn;

	pCardInfo->si_present = 0x01;

	return 0;
}

/*---------------------------------------------------------------------
 *
 * Function: FlashPoint_HardwareResetHostAdapter
 *
 * Description: Setup adapter for normal operation (hard reset).
 *
 *---------------------------------------------------------------------*/

static unsigned long FlashPoint_HardwareResetHostAdapter(struct sccb_mgr_info
							 *pCardInfo)
{
	struct sccb_card *CurrCard = NULL;
	struct nvram_info *pCurrNvRam;
	unsigned char i, j, thisCard, ScamFlg;
	unsigned short temp, sync_bit_map, id;
	unsigned long ioport;

	ioport = pCardInfo->si_baseaddr;

	for (thisCard = 0; thisCard <= MAX_CARDS; thisCard++) {

		if (thisCard == MAX_CARDS) {

			return FAILURE;
		}

		if (FPT_BL_Card[thisCard].ioPort == ioport) {

			CurrCard = &FPT_BL_Card[thisCard];
			FPT_SccbMgrTableInitCard(CurrCard, thisCard);
			break;
		}

		else if (FPT_BL_Card[thisCard].ioPort == 0x00) {

			FPT_BL_Card[thisCard].ioPort = ioport;
			CurrCard = &FPT_BL_Card[thisCard];

			if (FPT_mbCards)
				for (i = 0; i < FPT_mbCards; i++) {
					if (CurrCard->ioPort ==
					    FPT_nvRamInfo[i].niBaseAddr)
						CurrCard->pNvRamInfo =
						    &FPT_nvRamInfo[i];
				}
			FPT_SccbMgrTableInitCard(CurrCard, thisCard);
			CurrCard->cardIndex = thisCard;
			CurrCard->cardInfo = pCardInfo;

			break;
		}
	}

	pCurrNvRam = CurrCard->pNvRamInfo;

	if (pCurrNvRam) {
		ScamFlg = pCurrNvRam->niScamConf;
	} else {
		ScamFlg =
		    (unsigned char)FPT_utilEERead(ioport, SCAM_CONFIG / 2);
	}

	FPT_BusMasterInit(ioport);
	FPT_XbowInit(ioport, ScamFlg);

	FPT_autoLoadDefaultMap(ioport);

	for (i = 0, id = 0x01; i != pCardInfo->si_id; i++, id <<= 1) {
	}

	WR_HARPOON(ioport + hp_selfid_0, id);
	WR_HARPOON(ioport + hp_selfid_1, 0x00);
	WR_HARPOON(ioport + hp_arb_id, pCardInfo->si_id);
	CurrCard->ourId = pCardInfo->si_id;

	i = (unsigned char)pCardInfo->si_flags;
	if (i & SCSI_PARITY_ENA)
		WR_HARPOON(ioport + hp_portctrl_1, (HOST_MODE8 | CHK_SCSI_P));

	j = (RD_HARPOON(ioport + hp_bm_ctrl) & ~SCSI_TERM_ENA_L);
	if (i & LOW_BYTE_TERM)
		j |= SCSI_TERM_ENA_L;
	WR_HARPOON(ioport + hp_bm_ctrl, j);

	j = (RD_HARPOON(ioport + hp_ee_ctrl) & ~SCSI_TERM_ENA_H);
	if (i & HIGH_BYTE_TERM)
		j |= SCSI_TERM_ENA_H;
	WR_HARPOON(ioport + hp_ee_ctrl, j);

	if (!(pCardInfo->si_flags & SOFT_RESET)) {

		FPT_sresb(ioport, thisCard);

		FPT_scini(thisCard, pCardInfo->si_id, 0);
	}

	if (pCardInfo->si_flags & POST_ALL_UNDERRRUNS)
		CurrCard->globalFlags |= F_NO_FILTER;

	if (pCurrNvRam) {
		if (pCurrNvRam->niSysConf & 0x10)
			CurrCard->globalFlags |= F_GREEN_PC;
	} else {
		if (FPT_utilEERead(ioport, (SYSTEM_CONFIG / 2)) & GREEN_PC_ENA)
			CurrCard->globalFlags |= F_GREEN_PC;
	}

	/* Set global flag to indicate Re-Negotiation to be done on all
	   ckeck condition */
	if (pCurrNvRam) {
		if (pCurrNvRam->niScsiConf & 0x04)
			CurrCard->globalFlags |= F_DO_RENEGO;
	} else {
		if (FPT_utilEERead(ioport, (SCSI_CONFIG / 2)) & RENEGO_ENA)
			CurrCard->globalFlags |= F_DO_RENEGO;
	}

	if (pCurrNvRam) {
		if (pCurrNvRam->niScsiConf & 0x08)
			CurrCard->globalFlags |= F_CONLUN_IO;
	} else {
		if (FPT_utilEERead(ioport, (SCSI_CONFIG / 2)) & CONNIO_ENA)
			CurrCard->globalFlags |= F_CONLUN_IO;
	}

	temp = pCardInfo->si_per_targ_no_disc;

	for (i = 0, id = 1; i < MAX_SCSI_TAR; i++, id <<= 1) {

		if (temp & id)
			FPT_sccbMgrTbl[thisCard][i].TarStatus |= TAR_ALLOW_DISC;
	}

	sync_bit_map = 0x0001;

	for (id = 0; id < (MAX_SCSI_TAR / 2); id++) {

		if (pCurrNvRam) {
			temp = (unsigned short)pCurrNvRam->niSyncTbl[id];
			temp = ((temp & 0x03) + ((temp << 4) & 0xc0)) +
			    (((temp << 4) & 0x0300) + ((temp << 8) & 0xc000));
		} else
			temp =
			    FPT_utilEERead(ioport,
					   (unsigned short)((SYNC_RATE_TBL / 2)
							    + id));

		for (i = 0; i < 2; temp >>= 8, i++) {

			if (pCardInfo->si_per_targ_init_sync & sync_bit_map) {

				FPT_sccbMgrTbl[thisCard][id * 2 +
							 i].TarEEValue =
				    (unsigned char)temp;
			}

			else {
				FPT_sccbMgrTbl[thisCard][id * 2 +
							 i].TarStatus |=
				    SYNC_SUPPORTED;
				FPT_sccbMgrTbl[thisCard][id * 2 +
							 i].TarEEValue =
				    (unsigned char)(temp & ~EE_SYNC_MASK);
			}

/*         if ((pCardInfo->si_per_targ_wide_nego & sync_bit_map) ||
            (id*2+i >= 8)){
*/
			if (pCardInfo->si_per_targ_wide_nego & sync_bit_map) {

				FPT_sccbMgrTbl[thisCard][id * 2 +
							 i].TarEEValue |=
				    EE_WIDE_SCSI;

			}

			else {	/* NARROW SCSI */
				FPT_sccbMgrTbl[thisCard][id * 2 +
							 i].TarStatus |=
				    WIDE_NEGOCIATED;
			}

			sync_bit_map <<= 1;

		}
	}

	WR_HARPOON((ioport + hp_semaphore),
		   (unsigned char)(RD_HARPOON((ioport + hp_semaphore)) |
				   SCCB_MGR_PRESENT));

	return (unsigned long)CurrCard;
}

static void FlashPoint_ReleaseHostAdapter(unsigned long pCurrCard)
{
	unsigned char i;
	unsigned long portBase;
	unsigned long regOffset;
	unsigned long scamData;
	unsigned long *pScamTbl;
	struct nvram_info *pCurrNvRam;

	pCurrNvRam = ((struct sccb_card *)pCurrCard)->pNvRamInfo;

	if (pCurrNvRam) {
		FPT_WrStack(pCurrNvRam->niBaseAddr, 0, pCurrNvRam->niModel);
		FPT_WrStack(pCurrNvRam->niBaseAddr, 1, pCurrNvRam->niSysConf);
		FPT_WrStack(pCurrNvRam->niBaseAddr, 2, pCurrNvRam->niScsiConf);
		FPT_WrStack(pCurrNvRam->niBaseAddr, 3, pCurrNvRam->niScamConf);
		FPT_WrStack(pCurrNvRam->niBaseAddr, 4, pCurrNvRam->niAdapId);

		for (i = 0; i < MAX_SCSI_TAR / 2; i++)
			FPT_WrStack(pCurrNvRam->niBaseAddr,
				    (unsigned char)(i + 5),
				    pCurrNvRam->niSyncTbl[i]);

		portBase = pCurrNvRam->niBaseAddr;

		for (i = 0; i < MAX_SCSI_TAR; i++) {
			regOffset = hp_aramBase + 64 + i * 4;
			pScamTbl = (unsigned long *)&pCurrNvRam->niScamTbl[i];
			scamData = *pScamTbl;
			WR_HARP32(portBase, regOffset, scamData);
		}

	} else {
		FPT_WrStack(((struct sccb_card *)pCurrCard)->ioPort, 0, 0);
	}
}

static void FPT_RNVRamData(struct nvram_info *pNvRamInfo)
{
	unsigned char i;
	unsigned long portBase;
	unsigned long regOffset;
	unsigned long scamData;
	unsigned long *pScamTbl;

	pNvRamInfo->niModel = FPT_RdStack(pNvRamInfo->niBaseAddr, 0);
	pNvRamInfo->niSysConf = FPT_RdStack(pNvRamInfo->niBaseAddr, 1);
	pNvRamInfo->niScsiConf = FPT_RdStack(pNvRamInfo->niBaseAddr, 2);
	pNvRamInfo->niScamConf = FPT_RdStack(pNvRamInfo->niBaseAddr, 3);
	pNvRamInfo->niAdapId = FPT_RdStack(pNvRamInfo->niBaseAddr, 4);

	for (i = 0; i < MAX_SCSI_TAR / 2; i++)
		pNvRamInfo->niSyncTbl[i] =
		    FPT_RdStack(pNvRamInfo->niBaseAddr, (unsigned char)(i + 5));

	portBase = pNvRamInfo->niBaseAddr;

	for (i = 0; i < MAX_SCSI_TAR; i++) {
		regOffset = hp_aramBase + 64 + i * 4;
		RD_HARP32(portBase, regOffset, scamData);
		pScamTbl = (unsigned long *)&pNvRamInfo->niScamTbl[i];
		*pScamTbl = scamData;
	}

}

static unsigned char FPT_RdStack(unsigned long portBase, unsigned char index)
{
	WR_HARPOON(portBase + hp_stack_addr, index);
	return RD_HARPOON(portBase + hp_stack_data);
}

static void FPT_WrStack(unsigned long portBase, unsigned char index,
			unsigned char data)
{
	WR_HARPOON(portBase + hp_stack_addr, index);
	WR_HARPOON(portBase + hp_stack_data, data);
}

static unsigned char FPT_ChkIfChipInitialized(unsigned long ioPort)
{
	if ((RD_HARPOON(ioPort + hp_arb_id) & 0x0f) != FPT_RdStack(ioPort, 4))
		return 0;
	if ((RD_HARPOON(ioPort + hp_clkctrl_0) & CLKCTRL_DEFAULT)
	    != CLKCTRL_DEFAULT)
		return 0;
	if ((RD_HARPOON(ioPort + hp_seltimeout) == TO_250ms) ||
	    (RD_HARPOON(ioPort + hp_seltimeout) == TO_290ms))
		return 1;
	return 0;

}

/*---------------------------------------------------------------------
 *
 * Function: FlashPoint_StartCCB
 *
 * Description: Start a command pointed to by p_Sccb. When the
 *              command is completed it will be returned via the
 *              callback function.
 *
 *---------------------------------------------------------------------*/
static void FlashPoint_StartCCB(unsigned long pCurrCard, struct sccb *p_Sccb)
{
	unsigned long ioport;
	unsigned char thisCard, lun;
	struct sccb *pSaveSccb;
	CALL_BK_FN callback;

	thisCard = ((struct sccb_card *)pCurrCard)->cardIndex;
	ioport = ((struct sccb_card *)pCurrCard)->ioPort;

	if ((p_Sccb->TargID >= MAX_SCSI_TAR) || (p_Sccb->Lun >= MAX_LUN)) {

		p_Sccb->HostStatus = SCCB_COMPLETE;
		p_Sccb->SccbStatus = SCCB_ERROR;
		callback = (CALL_BK_FN) p_Sccb->SccbCallback;
		if (callback)
			callback(p_Sccb);

		return;
	}

	FPT_sinits(p_Sccb, thisCard);

	if (!((struct sccb_card *)pCurrCard)->cmdCounter) {
		WR_HARPOON(ioport + hp_semaphore,
			   (RD_HARPOON(ioport + hp_semaphore)
			    | SCCB_MGR_ACTIVE));

		if (((struct sccb_card *)pCurrCard)->globalFlags & F_GREEN_PC) {
			WR_HARPOON(ioport + hp_clkctrl_0, CLKCTRL_DEFAULT);
			WR_HARPOON(ioport + hp_sys_ctrl, 0x00);
		}
	}

	((struct sccb_card *)pCurrCard)->cmdCounter++;

	if (RD_HARPOON(ioport + hp_semaphore) & BIOS_IN_USE) {

		WR_HARPOON(ioport + hp_semaphore,
			   (RD_HARPOON(ioport + hp_semaphore)
			    | TICKLE_ME));
		if (p_Sccb->OperationCode == RESET_COMMAND) {
			pSaveSccb =
			    ((struct sccb_card *)pCurrCard)->currentSCCB;
			((struct sccb_card *)pCurrCard)->currentSCCB = p_Sccb;
			FPT_queueSelectFail(&FPT_BL_Card[thisCard], thisCard);
			((struct sccb_card *)pCurrCard)->currentSCCB =
			    pSaveSccb;
		} else {
			FPT_queueAddSccb(p_Sccb, thisCard);
		}
	}

	else if ((RD_HARPOON(ioport + hp_page_ctrl) & G_INT_DISABLE)) {

		if (p_Sccb->OperationCode == RESET_COMMAND) {
			pSaveSccb =
			    ((struct sccb_card *)pCurrCard)->currentSCCB;
			((struct sccb_card *)pCurrCard)->currentSCCB = p_Sccb;
			FPT_queueSelectFail(&FPT_BL_Card[thisCard], thisCard);
			((struct sccb_card *)pCurrCard)->currentSCCB =
			    pSaveSccb;
		} else {
			FPT_queueAddSccb(p_Sccb, thisCard);
		}
	}

	else {

		MDISABLE_INT(ioport);

		if ((((struct sccb_card *)pCurrCard)->globalFlags & F_CONLUN_IO)
		    &&
		    ((FPT_sccbMgrTbl[thisCard][p_Sccb->TargID].
		      TarStatus & TAR_TAG_Q_MASK) != TAG_Q_TRYING))
			lun = p_Sccb->Lun;
		else
			lun = 0;
		if ((((struct sccb_card *)pCurrCard)->currentSCCB == NULL) &&
		    (FPT_sccbMgrTbl[thisCard][p_Sccb->TargID].TarSelQ_Cnt == 0)
		    && (FPT_sccbMgrTbl[thisCard][p_Sccb->TargID].TarLUNBusy[lun]
			== 0)) {

			((struct sccb_card *)pCurrCard)->currentSCCB = p_Sccb;
			FPT_ssel(p_Sccb->SccbIOPort, thisCard);
		}

		else {

			if (p_Sccb->OperationCode == RESET_COMMAND) {
				pSaveSccb =
				    ((struct sccb_card *)pCurrCard)->
				    currentSCCB;
				((struct sccb_card *)pCurrCard)->currentSCCB =
				    p_Sccb;
				FPT_queueSelectFail(&FPT_BL_Card[thisCard],
						    thisCard);
				((struct sccb_card *)pCurrCard)->currentSCCB =
				    pSaveSccb;
			} else {
				FPT_queueAddSccb(p_Sccb, thisCard);
			}
		}

		MENABLE_INT(ioport);
	}

}

/*---------------------------------------------------------------------
 *
 * Function: FlashPoint_AbortCCB
 *
 * Description: Abort the command pointed to by p_Sccb.  When the
 *              command is completed it will be returned via the
 *              callback function.
 *
 *---------------------------------------------------------------------*/
static int FlashPoint_AbortCCB(unsigned long pCurrCard, struct sccb *p_Sccb)
{
	unsigned long ioport;

	unsigned char thisCard;
	CALL_BK_FN callback;
	unsigned char TID;
	struct sccb *pSaveSCCB;
	struct sccb_mgr_tar_info *currTar_Info;

	ioport = ((struct sccb_card *)pCurrCard)->ioPort;

	thisCard = ((struct sccb_card *)pCurrCard)->cardIndex;

	if (!(RD_HARPOON(ioport + hp_page_ctrl) & G_INT_DISABLE)) {

		if (FPT_queueFindSccb(p_Sccb, thisCard)) {

			((struct sccb_card *)pCurrCard)->cmdCounter--;

			if (!((struct sccb_card *)pCurrCard)->cmdCounter)
				WR_HARPOON(ioport + hp_semaphore,
					   (RD_HARPOON(ioport + hp_semaphore)
					    & (unsigned
					       char)(~(SCCB_MGR_ACTIVE |
						       TICKLE_ME))));

			p_Sccb->SccbStatus = SCCB_ABORT;
			callback = p_Sccb->SccbCallback;
			callback(p_Sccb);

			return 0;
		}

		else {
			if (((struct sccb_card *)pCurrCard)->currentSCCB ==
			    p_Sccb) {
				p_Sccb->SccbStatus = SCCB_ABORT;
				return 0;

			}

			else {

				TID = p_Sccb->TargID;

				if (p_Sccb->Sccb_tag) {
					MDISABLE_INT(ioport);
					if (((struct sccb_card *)pCurrCard)->
					    discQ_Tbl[p_Sccb->Sccb_tag] ==
					    p_Sccb) {
						p_Sccb->SccbStatus = SCCB_ABORT;
						p_Sccb->Sccb_scsistat =
						    ABORT_ST;
						p_Sccb->Sccb_scsimsg =
						    SMABORT_TAG;

						if (((struct sccb_card *)
						     pCurrCard)->currentSCCB ==
						    NULL) {
							((struct sccb_card *)
							 pCurrCard)->
					currentSCCB = p_Sccb;
							FPT_ssel(ioport,
								 thisCard);
						} else {
							pSaveSCCB =
							    ((struct sccb_card
							      *)pCurrCard)->
							    currentSCCB;
							((struct sccb_card *)
							 pCurrCard)->
					currentSCCB = p_Sccb;
							FPT_queueSelectFail((struct sccb_card *)pCurrCard, thisCard);
							((struct sccb_card *)
							 pCurrCard)->
					currentSCCB = pSaveSCCB;
						}
					}
					MENABLE_INT(ioport);
					return 0;
				} else {
					currTar_Info =
					    &FPT_sccbMgrTbl[thisCard][p_Sccb->
								      TargID];

					if (FPT_BL_Card[thisCard].
					    discQ_Tbl[currTar_Info->
						      LunDiscQ_Idx[p_Sccb->Lun]]
					    == p_Sccb) {
						p_Sccb->SccbStatus = SCCB_ABORT;
						return 0;
					}
				}
			}
		}
	}
	return -1;
}

/*---------------------------------------------------------------------
 *
 * Function: FlashPoint_InterruptPending
 *
 * Description: Do a quick check to determine if there is a pending
 *              interrupt for this card and disable the IRQ Pin if so.
 *
 *---------------------------------------------------------------------*/
static unsigned char FlashPoint_InterruptPending(unsigned long pCurrCard)
{
	unsigned long ioport;

	ioport = ((struct sccb_card *)pCurrCard)->ioPort;

	if (RD_HARPOON(ioport + hp_int_status) & INT_ASSERTED) {
		return 1;
	}

	else

		return 0;
}

/*---------------------------------------------------------------------
 *
 * Function: FlashPoint_HandleInterrupt
 *
 * Description: This is our entry point when an interrupt is generated
 *              by the card and the upper level driver passes it on to
 *              us.
 *
 *---------------------------------------------------------------------*/
static int FlashPoint_HandleInterrupt(unsigned long pCurrCard)
{
	struct sccb *currSCCB;
	unsigned char thisCard, result, bm_status, bm_int_st;
	unsigned short hp_int;
	unsigned char i, target;
	unsigned long ioport;

	thisCard = ((struct sccb_card *)pCurrCard)->cardIndex;
	ioport = ((struct sccb_card *)pCurrCard)->ioPort;

	MDISABLE_INT(ioport);

	if ((bm_int_st = RD_HARPOON(ioport + hp_int_status)) & EXT_STATUS_ON)
		bm_status =
		    RD_HARPOON(ioport +
			       hp_ext_status) & (unsigned char)BAD_EXT_STATUS;
	else
		bm_status = 0;

	WR_HARPOON(ioport + hp_int_mask, (INT_CMD_COMPL | SCSI_INTERRUPT));

	while ((hp_int =
		RDW_HARPOON((ioport +
			     hp_intstat)) & FPT_default_intena) | bm_status) {

		currSCCB = ((struct sccb_card *)pCurrCard)->currentSCCB;

		if (hp_int & (FIFO | TIMEOUT | RESET | SCAM_SEL) || bm_status) {
			result =
			    FPT_SccbMgr_bad_isr(ioport, thisCard,
						((struct sccb_card *)pCurrCard),
						hp_int);
			WRW_HARPOON((ioport + hp_intstat),
				    (FIFO | TIMEOUT | RESET | SCAM_SEL));
			bm_status = 0;

			if (result) {

				MENABLE_INT(ioport);
				return result;
			}
		}

		else if (hp_int & ICMD_COMP) {

			if (!(hp_int & BUS_FREE)) {
				/* Wait for the BusFree before starting a new command.  We
				   must also check for being reselected since the BusFree
				   may not show up if another device reselects us in 1.5us or
				   less.  SRR Wednesday, 3/8/1995.
				 */
				while (!
				       (RDW_HARPOON((ioport + hp_intstat)) &
					(BUS_FREE | RSEL))) ;
			}

			if (((struct sccb_card *)pCurrCard)->
			    globalFlags & F_HOST_XFER_ACT)

				FPT_phaseChkFifo(ioport, thisCard);

/*         WRW_HARPOON((ioport+hp_intstat),
            (BUS_FREE | ICMD_COMP | ITAR_DISC | XFER_CNT_0));
         */

			WRW_HARPOON((ioport + hp_intstat), CLR_ALL_INT_1);

			FPT_autoCmdCmplt(ioport, thisCard);

		}

		else if (hp_int & ITAR_DISC) {

			if (((struct sccb_card *)pCurrCard)->
			    globalFlags & F_HOST_XFER_ACT) {

				FPT_phaseChkFifo(ioport, thisCard);

			}

			if (RD_HARPOON(ioport + hp_gp_reg_1) == SMSAVE_DATA_PTR) {

				WR_HARPOON(ioport + hp_gp_reg_1, 0x00);
				currSCCB->Sccb_XferState |= F_NO_DATA_YET;

				currSCCB->Sccb_savedATC = currSCCB->Sccb_ATC;
			}

			currSCCB->Sccb_scsistat = DISCONNECT_ST;
			FPT_queueDisconnect(currSCCB, thisCard);

			/* Wait for the BusFree before starting a new command.  We
			   must also check for being reselected since the BusFree
			   may not show up if another device reselects us in 1.5us or
			   less.  SRR Wednesday, 3/8/1995.
			 */
			while (!
			       (RDW_HARPOON((ioport + hp_intstat)) &
				(BUS_FREE | RSEL))
			       && !((RDW_HARPOON((ioport + hp_intstat)) & PHASE)
				    && RD_HARPOON((ioport + hp_scsisig)) ==
				    (SCSI_BSY | SCSI_REQ | SCSI_CD | SCSI_MSG |
				     SCSI_IOBIT))) ;

			/*
			   The additional loop exit condition above detects a timing problem
			   with the revision D/E harpoon chips.  The caller should reset the
			   host adapter to recover when 0xFE is returned.
			 */
			if (!
			    (RDW_HARPOON((ioport + hp_intstat)) &
			     (BUS_FREE | RSEL))) {
				MENABLE_INT(ioport);
				return 0xFE;
			}

			WRW_HARPOON((ioport + hp_intstat),
				    (BUS_FREE | ITAR_DISC));

			((struct sccb_card *)pCurrCard)->globalFlags |=
			    F_NEW_SCCB_CMD;

		}

		else if (hp_int & RSEL) {

			WRW_HARPOON((ioport + hp_intstat),
				    (PROG_HLT | RSEL | PHASE | BUS_FREE));

			if (RDW_HARPOON((ioport + hp_intstat)) & ITAR_DISC) {
				if (((struct sccb_card *)pCurrCard)->
				    globalFlags & F_HOST_XFER_ACT) {
					FPT_phaseChkFifo(ioport, thisCard);
				}

				if (RD_HARPOON(ioport + hp_gp_reg_1) ==
				    SMSAVE_DATA_PTR) {
					WR_HARPOON(ioport + hp_gp_reg_1, 0x00);
					currSCCB->Sccb_XferState |=
					    F_NO_DATA_YET;
					currSCCB->Sccb_savedATC =
					    currSCCB->Sccb_ATC;
				}

				WRW_HARPOON((ioport + hp_intstat),
					    (BUS_FREE | ITAR_DISC));
				currSCCB->Sccb_scsistat = DISCONNECT_ST;
				FPT_queueDisconnect(currSCCB, thisCard);
			}

			FPT_sres(ioport, thisCard,
				 ((struct sccb_card *)pCurrCard));
			FPT_phaseDecode(ioport, thisCard);

		}

		else if ((hp_int & IDO_STRT) && (!(hp_int & BUS_FREE))) {

			WRW_HARPOON((ioport + hp_intstat),
				    (IDO_STRT | XFER_CNT_0));
			FPT_phaseDecode(ioport, thisCard);

		}

		else if ((hp_int & IUNKWN) || (hp_int & PROG_HLT)) {
			WRW_HARPOON((ioport + hp_intstat),
				    (PHASE | IUNKWN | PROG_HLT));
			if ((RD_HARPOON(ioport + hp_prgmcnt_0) & (unsigned char)
			     0x3f) < (unsigned char)SELCHK) {
				FPT_phaseDecode(ioport, thisCard);
			} else {
				/* Harpoon problem some SCSI target device respond to selection
				   with short BUSY pulse (<400ns) this will make the Harpoon is not able
				   to latch the correct Target ID into reg. x53.
				   The work around require to correct this reg. But when write to this
				   reg. (0x53) also increment the FIFO write addr reg (0x6f), thus we
				   need to read this reg first then restore it later. After update to 0x53 */

				i = (unsigned
				     char)(RD_HARPOON(ioport + hp_fifowrite));
				target =
				    (unsigned
				     char)(RD_HARPOON(ioport + hp_gp_reg_3));
				WR_HARPOON(ioport + hp_xfer_pad,
					   (unsigned char)ID_UNLOCK);
				WR_HARPOON(ioport + hp_select_id,
					   (unsigned char)(target | target <<
							   4));
				WR_HARPOON(ioport + hp_xfer_pad,
					   (unsigned char)0x00);
				WR_HARPOON(ioport + hp_fifowrite, i);
				WR_HARPOON(ioport + hp_autostart_3,
					   (AUTO_IMMED + TAG_STRT));
			}
		}

		else if (hp_int & XFER_CNT_0) {

			WRW_HARPOON((ioport + hp_intstat), XFER_CNT_0);

			FPT_schkdd(ioport, thisCard);

		}

		else if (hp_int & BUS_FREE) {

			WRW_HARPOON((ioport + hp_intstat), BUS_FREE);

			if (((struct sccb_card *)pCurrCard)->
			    globalFlags & F_HOST_XFER_ACT) {

				FPT_hostDataXferAbort(ioport, thisCard,
						      currSCCB);
			}

			FPT_phaseBusFree(ioport, thisCard);
		}

		else if (hp_int & ITICKLE) {

			WRW_HARPOON((ioport + hp_intstat), ITICKLE);
			((struct sccb_card *)pCurrCard)->globalFlags |=
			    F_NEW_SCCB_CMD;
		}

		if (((struct sccb_card *)pCurrCard)->
		    globalFlags & F_NEW_SCCB_CMD) {

			((struct sccb_card *)pCurrCard)->globalFlags &=
			    ~F_NEW_SCCB_CMD;

			if (((struct sccb_card *)pCurrCard)->currentSCCB ==
			    NULL) {

				FPT_queueSearchSelect(((struct sccb_card *)
						       pCurrCard), thisCard);
			}

			if (((struct sccb_card *)pCurrCard)->currentSCCB !=
			    NULL) {
				((struct sccb_card *)pCurrCard)->globalFlags &=
				    ~F_NEW_SCCB_CMD;
				FPT_ssel(ioport, thisCard);
			}

			break;

		}

	}			/*end while */

	MENABLE_INT(ioport);

	return 0;
}

/*---------------------------------------------------------------------
 *
 * Function: Sccb_bad_isr
 *
 * Description: Some type of interrupt has occurred which is slightly
 *              out of the ordinary.  We will now decode it fully, in
 *              this routine.  This is broken up in an attempt to save
 *              processing time.
 *
 *---------------------------------------------------------------------*/
static unsigned char FPT_SccbMgr_bad_isr(unsigned long p_port,
					 unsigned char p_card,
					 struct sccb_card *pCurrCard,
					 unsigned short p_int)
{
	unsigned char temp, ScamFlg;
	struct sccb_mgr_tar_info *currTar_Info;
	struct nvram_info *pCurrNvRam;

	if (RD_HARPOON(p_port + hp_ext_status) &
	    (BM_FORCE_OFF | PCI_DEV_TMOUT | BM_PARITY_ERR | PIO_OVERRUN)) {

		if (pCurrCard->globalFlags & F_HOST_XFER_ACT) {

			FPT_hostDataXferAbort(p_port, p_card,
					      pCurrCard->currentSCCB);
		}

		if (RD_HARPOON(p_port + hp_pci_stat_cfg) & REC_MASTER_ABORT)
		{
			WR_HARPOON(p_port + hp_pci_stat_cfg,
				   (RD_HARPOON(p_port + hp_pci_stat_cfg) &
				    ~REC_MASTER_ABORT));

			WR_HARPOON(p_port + hp_host_blk_cnt, 0x00);

		}

		if (pCurrCard->currentSCCB != NULL) {

			if (!pCurrCard->currentSCCB->HostStatus)
				pCurrCard->currentSCCB->HostStatus =
				    SCCB_BM_ERR;

			FPT_sxfrp(p_port, p_card);

			temp = (unsigned char)(RD_HARPOON(p_port + hp_ee_ctrl) &
					       (EXT_ARB_ACK | SCSI_TERM_ENA_H));
			WR_HARPOON(p_port + hp_ee_ctrl,
				   ((unsigned char)temp | SEE_MS | SEE_CS));
			WR_HARPOON(p_port + hp_ee_ctrl, temp);

			if (!
			    (RDW_HARPOON((p_port + hp_intstat)) &
			     (BUS_FREE | RESET))) {
				FPT_phaseDecode(p_port, p_card);
			}
		}
	}

	else if (p_int & RESET) {

		WR_HARPOON(p_port + hp_clkctrl_0, CLKCTRL_DEFAULT);
		WR_HARPOON(p_port + hp_sys_ctrl, 0x00);
		if (pCurrCard->currentSCCB != NULL) {

			if (pCurrCard->globalFlags & F_HOST_XFER_ACT)

				FPT_hostDataXferAbort(p_port, p_card,
						      pCurrCard->currentSCCB);
		}

		DISABLE_AUTO(p_port);

		FPT_sresb(p_port, p_card);

		while (RD_HARPOON(p_port + hp_scsictrl_0) & SCSI_RST) {
		}

		pCurrNvRam = pCurrCard->pNvRamInfo;
		if (pCurrNvRam) {
			ScamFlg = pCurrNvRam->niScamConf;
		} else {
			ScamFlg =
			    (unsigned char)FPT_utilEERead(p_port,
							  SCAM_CONFIG / 2);
		}

		FPT_XbowInit(p_port, ScamFlg);

		FPT_scini(p_card, pCurrCard->ourId, 0);

		return 0xFF;
	}

	else if (p_int & FIFO) {

		WRW_HARPOON((p_port + hp_intstat), FIFO);

		if (pCurrCard->currentSCCB != NULL)
			FPT_sxfrp(p_port, p_card);
	}

	else if (p_int & TIMEOUT) {

		DISABLE_AUTO(p_port);

		WRW_HARPOON((p_port + hp_intstat),
			    (PROG_HLT | TIMEOUT | SEL | BUS_FREE | PHASE |
			     IUNKWN));

		pCurrCard->currentSCCB->HostStatus = SCCB_SELECTION_TIMEOUT;

		currTar_Info =
		    &FPT_sccbMgrTbl[p_card][pCurrCard->currentSCCB->TargID];
		if ((pCurrCard->globalFlags & F_CONLUN_IO)
		    && ((currTar_Info->TarStatus & TAR_TAG_Q_MASK) !=
			TAG_Q_TRYING))
			currTar_Info->TarLUNBusy[pCurrCard->currentSCCB->Lun] =
			    0;
		else
			currTar_Info->TarLUNBusy[0] = 0;

		if (currTar_Info->TarEEValue & EE_SYNC_MASK) {
			currTar_Info->TarSyncCtrl = 0;
			currTar_Info->TarStatus &= ~TAR_SYNC_MASK;
		}

		if (currTar_Info->TarEEValue & EE_WIDE_SCSI) {
			currTar_Info->TarStatus &= ~TAR_WIDE_MASK;
		}

		FPT_sssyncv(p_port, pCurrCard->currentSCCB->TargID, NARROW_SCSI,
			    currTar_Info);

		FPT_queueCmdComplete(pCurrCard, pCurrCard->currentSCCB, p_card);

	}

	else if (p_int & SCAM_SEL) {

		FPT_scarb(p_port, LEVEL2_TAR);
		FPT_scsel(p_port);
		FPT_scasid(p_card, p_port);

		FPT_scbusf(p_port);

		WRW_HARPOON((p_port + hp_intstat), SCAM_SEL);
	}

	return 0x00;
}

/*---------------------------------------------------------------------
 *
 * Function: SccbMgrTableInit
 *
 * Description: Initialize all Sccb manager data structures.
 *
 *---------------------------------------------------------------------*/

static void FPT_SccbMgrTableInitAll()
{
	unsigned char thisCard;

	for (thisCard = 0; thisCard < MAX_CARDS; thisCard++) {
		FPT_SccbMgrTableInitCard(&FPT_BL_Card[thisCard], thisCard);

		FPT_BL_Card[thisCard].ioPort = 0x00;
		FPT_BL_Card[thisCard].cardInfo = NULL;
		FPT_BL_Card[thisCard].cardIndex = 0xFF;
		FPT_BL_Card[thisCard].ourId = 0x00;
		FPT_BL_Card[thisCard].pNvRamInfo = NULL;
	}
}

/*---------------------------------------------------------------------
 *
 * Function: SccbMgrTableInit
 *
 * Description: Initialize all Sccb manager data structures.
 *
 *---------------------------------------------------------------------*/

static void FPT_SccbMgrTableInitCard(struct sccb_card *pCurrCard,
				     unsigned char p_card)
{
	unsigned char scsiID, qtag;

	for (qtag = 0; qtag < QUEUE_DEPTH; qtag++) {
		FPT_BL_Card[p_card].discQ_Tbl[qtag] = NULL;
	}

	for (scsiID = 0; scsiID < MAX_SCSI_TAR; scsiID++) {
		FPT_sccbMgrTbl[p_card][scsiID].TarStatus = 0;
		FPT_sccbMgrTbl[p_card][scsiID].TarEEValue = 0;
		FPT_SccbMgrTableInitTarget(p_card, scsiID);
	}

	pCurrCard->scanIndex = 0x00;
	pCurrCard->currentSCCB = NULL;
	pCurrCard->globalFlags = 0x00;
	pCurrCard->cmdCounter = 0x00;
	pCurrCard->tagQ_Lst = 0x01;
	pCurrCard->discQCount = 0;

}

/*---------------------------------------------------------------------
 *
 * Function: SccbMgrTableInit
 *
 * Description: Initialize all Sccb manager data structures.
 *
 *---------------------------------------------------------------------*/

static void FPT_SccbMgrTableInitTarget(unsigned char p_card,
				       unsigned char target)
{

	unsigned char lun, qtag;
	struct sccb_mgr_tar_info *currTar_Info;

	currTar_Info = &FPT_sccbMgrTbl[p_card][target];

	currTar_Info->TarSelQ_Cnt = 0;
	currTar_Info->TarSyncCtrl = 0;

	currTar_Info->TarSelQ_Head = NULL;
	currTar_Info->TarSelQ_Tail = NULL;
	currTar_Info->TarTagQ_Cnt = 0;
	currTar_Info->TarLUN_CA = 0;

	for (lun = 0; lun < MAX_LUN; lun++) {
		currTar_Info->TarLUNBusy[lun] = 0;
		currTar_Info->LunDiscQ_Idx[lun] = 0;
	}

	for (qtag = 0; qtag < QUEUE_DEPTH; qtag++) {
		if (FPT_BL_Card[p_card].discQ_Tbl[qtag] != NULL) {
			if (FPT_BL_Card[p_card].discQ_Tbl[qtag]->TargID ==
			    target) {
				FPT_BL_Card[p_card].discQ_Tbl[qtag] = NULL;
				FPT_BL_Card[p_card].discQCount--;
			}
		}
	}
}

/*---------------------------------------------------------------------
 *
 * Function: sfetm
 *
 * Description: Read in a message byte from the SCSI bus, and check
 *              for a parity error.
 *
 *---------------------------------------------------------------------*/

static unsigned char FPT_sfm(unsigned long port, struct sccb *pCurrSCCB)
{
	unsigned char message;
	unsigned short TimeOutLoop;

	TimeOutLoop = 0;
	while ((!(RD_HARPOON(port + hp_scsisig) & SCSI_REQ)) &&
	       (TimeOutLoop++ < 20000)) {
	}

	WR_HARPOON(port + hp_portctrl_0, SCSI_PORT);

	message = RD_HARPOON(port + hp_scsidata_0);

	WR_HARPOON(port + hp_scsisig, SCSI_ACK + S_MSGI_PH);

	if (TimeOutLoop > 20000)
		message = 0x00;	/* force message byte = 0 if Time Out on Req */

	if ((RDW_HARPOON((port + hp_intstat)) & PARITY) &&
	    (RD_HARPOON(port + hp_addstat) & SCSI_PAR_ERR)) {
		WR_HARPOON(port + hp_scsisig, (SCSI_ACK + S_ILL_PH));
		WR_HARPOON(port + hp_xferstat, 0);
		WR_HARPOON(port + hp_fiforead, 0);
		WR_HARPOON(port + hp_fifowrite, 0);
		if (pCurrSCCB != NULL) {
			pCurrSCCB->Sccb_scsimsg = SMPARITY;
		}
		message = 0x00;
		do {
			ACCEPT_MSG_ATN(port);
			TimeOutLoop = 0;
			while ((!(RD_HARPOON(port + hp_scsisig) & SCSI_REQ)) &&
			       (TimeOutLoop++ < 20000)) {
			}
			if (TimeOutLoop > 20000) {
				WRW_HARPOON((port + hp_intstat), PARITY);
				return message;
			}
			if ((RD_HARPOON(port + hp_scsisig) & S_SCSI_PHZ) !=
			    S_MSGI_PH) {
				WRW_HARPOON((port + hp_intstat), PARITY);
				return message;
			}
			WR_HARPOON(port + hp_portctrl_0, SCSI_PORT);

			RD_HARPOON(port + hp_scsidata_0);

			WR_HARPOON(port + hp_scsisig, (SCSI_ACK + S_ILL_PH));

		} while (1);

	}
	WR_HARPOON(port + hp_scsisig, (SCSI_ACK + S_ILL_PH));
	WR_HARPOON(port + hp_xferstat, 0);
	WR_HARPOON(port + hp_fiforead, 0);
	WR_HARPOON(port + hp_fifowrite, 0);
	return message;
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_ssel
 *
 * Description: Load up automation and select target device.
 *
 *---------------------------------------------------------------------*/

static void FPT_ssel(unsigned long port, unsigned char p_card)
{

	unsigned char auto_loaded, i, target, *theCCB;

	unsigned long cdb_reg;
	struct sccb_card *CurrCard;
	struct sccb *currSCCB;
	struct sccb_mgr_tar_info *currTar_Info;
	unsigned char lastTag, lun;

	CurrCard = &FPT_BL_Card[p_card];
	currSCCB = CurrCard->currentSCCB;
	target = currSCCB->TargID;
	currTar_Info = &FPT_sccbMgrTbl[p_card][target];
	lastTag = CurrCard->tagQ_Lst;

	ARAM_ACCESS(port);

	if ((currTar_Info->TarStatus & TAR_TAG_Q_MASK) == TAG_Q_REJECT)
		currSCCB->ControlByte &= ~F_USE_CMD_Q;

	if (((CurrCard->globalFlags & F_CONLUN_IO) &&
	     ((currTar_Info->TarStatus & TAR_TAG_Q_MASK) != TAG_Q_TRYING)))

		lun = currSCCB->Lun;
	else
		lun = 0;

	if (CurrCard->globalFlags & F_TAG_STARTED) {
		if (!(currSCCB->ControlByte & F_USE_CMD_Q)) {
			if ((currTar_Info->TarLUN_CA == 0)
			    && ((currTar_Info->TarStatus & TAR_TAG_Q_MASK)
				== TAG_Q_TRYING)) {

				if (currTar_Info->TarTagQ_Cnt != 0) {
					currTar_Info->TarLUNBusy[lun] = 1;
					FPT_queueSelectFail(CurrCard, p_card);
					SGRAM_ACCESS(port);
					return;
				}

				else {
					currTar_Info->TarLUNBusy[lun] = 1;
				}

			}
			/*End non-tagged */
			else {
				currTar_Info->TarLUNBusy[lun] = 1;
			}

		}
		/*!Use cmd Q Tagged */
		else {
			if (currTar_Info->TarLUN_CA == 1) {
				FPT_queueSelectFail(CurrCard, p_card);
				SGRAM_ACCESS(port);
				return;
			}

			currTar_Info->TarLUNBusy[lun] = 1;

		}		/*else use cmd Q tagged */

	}
	/*if glob tagged started */
	else {
		currTar_Info->TarLUNBusy[lun] = 1;
	}

	if ((((CurrCard->globalFlags & F_CONLUN_IO) &&
	      ((currTar_Info->TarStatus & TAR_TAG_Q_MASK) != TAG_Q_TRYING))
	     || (!(currSCCB->ControlByte & F_USE_CMD_Q)))) {
		if (CurrCard->discQCount >= QUEUE_DEPTH) {
			currTar_Info->TarLUNBusy[lun] = 1;
			FPT_queueSelectFail(CurrCard, p_card);
			SGRAM_ACCESS(port);
			return;
		}
		for (i = 1; i < QUEUE_DEPTH; i++) {
			if (++lastTag >= QUEUE_DEPTH)
				lastTag = 1;
			if (CurrCard->discQ_Tbl[lastTag] == NULL) {
				CurrCard->tagQ_Lst = lastTag;
				currTar_Info->LunDiscQ_Idx[lun] = lastTag;
				CurrCard->discQ_Tbl[lastTag] = currSCCB;
				CurrCard->discQCount++;
				break;
			}
		}
		if (i == QUEUE_DEPTH) {
			currTar_Info->TarLUNBusy[lun] = 1;
			FPT_queueSelectFail(CurrCard, p_card);
			SGRAM_ACCESS(port);
			return;
		}
	}

	auto_loaded = 0;

	WR_HARPOON(port + hp_select_id, target);
	WR_HARPOON(port + hp_gp_reg_3, target);	/* Use by new automation logic */

	if (currSCCB->OperationCode == RESET_COMMAND) {
		WRW_HARPOON((port + ID_MSG_STRT), (MPM_OP + AMSG_OUT +
						   (currSCCB->
						    Sccb_idmsg & ~DISC_PRIV)));

		WRW_HARPOON((port + ID_MSG_STRT + 2), BRH_OP + ALWAYS + NP);

		currSCCB->Sccb_scsimsg = SMDEV_RESET;

		WR_HARPOON(port + hp_autostart_3, (SELECT + SELCHK_STRT));
		auto_loaded = 1;
		currSCCB->Sccb_scsistat = SELECT_BDR_ST;

		if (currTar_Info->TarEEValue & EE_SYNC_MASK) {
			currTar_Info->TarSyncCtrl = 0;
			currTar_Info->TarStatus &= ~TAR_SYNC_MASK;
		}

		if (currTar_Info->TarEEValue & EE_WIDE_SCSI) {
			currTar_Info->TarStatus &= ~TAR_WIDE_MASK;
		}

		FPT_sssyncv(port, target, NARROW_SCSI, currTar_Info);
		FPT_SccbMgrTableInitTarget(p_card, target);

	}

	else if (currSCCB->Sccb_scsistat == ABORT_ST) {
		WRW_HARPOON((port + ID_MSG_STRT), (MPM_OP + AMSG_OUT +
						   (currSCCB->
						    Sccb_idmsg & ~DISC_PRIV)));

		WRW_HARPOON((port + ID_MSG_STRT + 2), BRH_OP + ALWAYS + CMDPZ);

		WRW_HARPOON((port + SYNC_MSGS + 0), (MPM_OP + AMSG_OUT +
						     (((unsigned
							char)(currSCCB->
							      ControlByte &
							      TAG_TYPE_MASK)
						       >> 6) | (unsigned char)
						      0x20)));
		WRW_HARPOON((port + SYNC_MSGS + 2),
			    (MPM_OP + AMSG_OUT + currSCCB->Sccb_tag));
		WRW_HARPOON((port + SYNC_MSGS + 4), (BRH_OP + ALWAYS + NP));

		WR_HARPOON(port + hp_autostart_3, (SELECT + SELCHK_STRT));
		auto_loaded = 1;

	}

	else if (!(currTar_Info->TarStatus & WIDE_NEGOCIATED)) {
		auto_loaded = FPT_siwidn(port, p_card);
		currSCCB->Sccb_scsistat = SELECT_WN_ST;
	}

	else if (!((currTar_Info->TarStatus & TAR_SYNC_MASK)
		   == SYNC_SUPPORTED)) {
		auto_loaded = FPT_sisyncn(port, p_card, 0);
		currSCCB->Sccb_scsistat = SELECT_SN_ST;
	}

	if (!auto_loaded) {

		if (currSCCB->ControlByte & F_USE_CMD_Q) {

			CurrCard->globalFlags |= F_TAG_STARTED;

			if ((currTar_Info->TarStatus & TAR_TAG_Q_MASK)
			    == TAG_Q_REJECT) {
				currSCCB->ControlByte &= ~F_USE_CMD_Q;

				/* Fix up the start instruction with a jump to
				   Non-Tag-CMD handling */
				WRW_HARPOON((port + ID_MSG_STRT),
					    BRH_OP + ALWAYS + NTCMD);

				WRW_HARPOON((port + NON_TAG_ID_MSG),
					    (MPM_OP + AMSG_OUT +
					     currSCCB->Sccb_idmsg));

				WR_HARPOON(port + hp_autostart_3,
					   (SELECT + SELCHK_STRT));

				/* Setup our STATE so we know what happend when
				   the wheels fall off. */
				currSCCB->Sccb_scsistat = SELECT_ST;

				currTar_Info->TarLUNBusy[lun] = 1;
			}

			else {
				WRW_HARPOON((port + ID_MSG_STRT),
					    (MPM_OP + AMSG_OUT +
					     currSCCB->Sccb_idmsg));

				WRW_HARPOON((port + ID_MSG_STRT + 2),
					    (MPM_OP + AMSG_OUT +
					     (((unsigned char)(currSCCB->
							       ControlByte &
							       TAG_TYPE_MASK)
					       >> 6) | (unsigned char)0x20)));

				for (i = 1; i < QUEUE_DEPTH; i++) {
					if (++lastTag >= QUEUE_DEPTH)
						lastTag = 1;
					if (CurrCard->discQ_Tbl[lastTag] ==
					    NULL) {
						WRW_HARPOON((port +
							     ID_MSG_STRT + 6),
							    (MPM_OP + AMSG_OUT +
							     lastTag));
						CurrCard->tagQ_Lst = lastTag;
						currSCCB->Sccb_tag = lastTag;
						CurrCard->discQ_Tbl[lastTag] =
						    currSCCB;
						CurrCard->discQCount++;
						break;
					}
				}

				if (i == QUEUE_DEPTH) {
					currTar_Info->TarLUNBusy[lun] = 1;
					FPT_queueSelectFail(CurrCard, p_card);
					SGRAM_ACCESS(port);
					return;
				}

				currSCCB->Sccb_scsistat = SELECT_Q_ST;

				WR_HARPOON(port + hp_autostart_3,
					   (SELECT + SELCHK_STRT));
			}
		}

		else {

			WRW_HARPOON((port + ID_MSG_STRT),
				    BRH_OP + ALWAYS + NTCMD);

			WRW_HARPOON((port + NON_TAG_ID_MSG),
				    (MPM_OP + AMSG_OUT + currSCCB->Sccb_idmsg));

			currSCCB->Sccb_scsistat = SELECT_ST;

			WR_HARPOON(port + hp_autostart_3,
				   (SELECT + SELCHK_STRT));
		}

		theCCB = (unsigned char *)&currSCCB->Cdb[0];

		cdb_reg = port + CMD_STRT;

		for (i = 0; i < currSCCB->CdbLength; i++) {
			WRW_HARPOON(cdb_reg, (MPM_OP + ACOMMAND + *theCCB));
			cdb_reg += 2;
			theCCB++;
		}

		if (currSCCB->CdbLength != TWELVE_BYTE_CMD)
			WRW_HARPOON(cdb_reg, (BRH_OP + ALWAYS + NP));

	}
	/* auto_loaded */
	WRW_HARPOON((port + hp_fiforead), (unsigned short)0x00);
	WR_HARPOON(port + hp_xferstat, 0x00);

	WRW_HARPOON((port + hp_intstat), (PROG_HLT | TIMEOUT | SEL | BUS_FREE));

	WR_HARPOON(port + hp_portctrl_0, (SCSI_PORT));

	if (!(currSCCB->Sccb_MGRFlags & F_DEV_SELECTED)) {
		WR_HARPOON(port + hp_scsictrl_0,
			   (SEL_TAR | ENA_ATN | ENA_RESEL | ENA_SCAM_SEL));
	} else {

/*      auto_loaded =  (RD_HARPOON(port+hp_autostart_3) & (unsigned char)0x1F);
      auto_loaded |= AUTO_IMMED; */
		auto_loaded = AUTO_IMMED;

		DISABLE_AUTO(port);

		WR_HARPOON(port + hp_autostart_3, auto_loaded);
	}

	SGRAM_ACCESS(port);
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_sres
 *
 * Description: Hookup the correct CCB and handle the incoming messages.
 *
 *---------------------------------------------------------------------*/

static void FPT_sres(unsigned long port, unsigned char p_card,
		     struct sccb_card *pCurrCard)
{

	unsigned char our_target, message, lun = 0, tag, msgRetryCount;

	struct sccb_mgr_tar_info *currTar_Info;
	struct sccb *currSCCB;

	if (pCurrCard->currentSCCB != NULL) {
		currTar_Info =
		    &FPT_sccbMgrTbl[p_card][pCurrCard->currentSCCB->TargID];
		DISABLE_AUTO(port);

		WR_HARPOON((port + hp_scsictrl_0), (ENA_RESEL | ENA_SCAM_SEL));

		currSCCB = pCurrCard->currentSCCB;
		if (currSCCB->Sccb_scsistat == SELECT_WN_ST) {
			currTar_Info->TarStatus &= ~TAR_WIDE_MASK;
			currSCCB->Sccb_scsistat = BUS_FREE_ST;
		}
		if (currSCCB->Sccb_scsistat == SELECT_SN_ST) {
			currTar_Info->TarStatus &= ~TAR_SYNC_MASK;
			currSCCB->Sccb_scsistat = BUS_FREE_ST;
		}
		if (((pCurrCard->globalFlags & F_CONLUN_IO) &&
		     ((currTar_Info->TarStatus & TAR_TAG_Q_MASK) !=
		      TAG_Q_TRYING))) {
			currTar_Info->TarLUNBusy[currSCCB->Lun] = 0;
			if (currSCCB->Sccb_scsistat != ABORT_ST) {
				pCurrCard->discQCount--;
				pCurrCard->discQ_Tbl[currTar_Info->
						     LunDiscQ_Idx[currSCCB->
								  Lun]]
				    = NULL;
			}
		} else {
			currTar_Info->TarLUNBusy[0] = 0;
			if (currSCCB->Sccb_tag) {
				if (currSCCB->Sccb_scsistat != ABORT_ST) {
					pCurrCard->discQCount--;
					pCurrCard->discQ_Tbl[currSCCB->
							     Sccb_tag] = NULL;
				}
			} else {
				if (currSCCB->Sccb_scsistat != ABORT_ST) {
					pCurrCard->discQCount--;
					pCurrCard->discQ_Tbl[currTar_Info->
							     LunDiscQ_Idx[0]] =
					    NULL;
				}
			}
		}

		FPT_queueSelectFail(&FPT_BL_Card[p_card], p_card);
	}

	WRW_HARPOON((port + hp_fiforead), (unsigned short)0x00);

	our_target = (unsigned char)(RD_HARPOON(port + hp_select_id) >> 4);
	currTar_Info = &FPT_sccbMgrTbl[p_card][our_target];

	msgRetryCount = 0;
	do {

		currTar_Info = &FPT_sccbMgrTbl[p_card][our_target];
		tag = 0;

		while (!(RD_HARPOON(port + hp_scsisig) & SCSI_REQ)) {
			if (!(RD_HARPOON(port + hp_scsisig) & SCSI_BSY)) {

				WRW_HARPOON((port + hp_intstat), PHASE);
				return;
			}
		}

		WRW_HARPOON((port + hp_intstat), PHASE);
		if ((RD_HARPOON(port + hp_scsisig) & S_SCSI_PHZ) == S_MSGI_PH) {

			message = FPT_sfm(port, pCurrCard->currentSCCB);
			if (message) {

				if (message <= (0x80 | LUN_MASK)) {
					lun = message & (unsigned char)LUN_MASK;

					if ((currTar_Info->
					     TarStatus & TAR_TAG_Q_MASK) ==
					    TAG_Q_TRYING) {
						if (currTar_Info->TarTagQ_Cnt !=
						    0) {

							if (!
							    (currTar_Info->
							     TarLUN_CA)) {
								ACCEPT_MSG(port);	/*Release the ACK for ID msg. */

								message =
								    FPT_sfm
								    (port,
								     pCurrCard->
								     currentSCCB);
								if (message) {
									ACCEPT_MSG
									    (port);
								}

								else
									message
									    = 0;

								if (message !=
								    0) {
									tag =
									    FPT_sfm
									    (port,
									     pCurrCard->
									     currentSCCB);

									if (!
									    (tag))
										message
										    =
										    0;
								}

							}
							/*C.A. exists! */
						}
						/*End Q cnt != 0 */
					}
					/*End Tag cmds supported! */
				}
				/*End valid ID message.  */
				else {

					ACCEPT_MSG_ATN(port);
				}

			}
			/* End good id message. */
			else {

				message = 0;
			}
		} else {
			ACCEPT_MSG_ATN(port);

			while (!
			       (RDW_HARPOON((port + hp_intstat)) &
				(PHASE | RESET))
			       && !(RD_HARPOON(port + hp_scsisig) & SCSI_REQ)
			       && (RD_HARPOON(port + hp_scsisig) & SCSI_BSY)) ;

			return;
		}

		if (message == 0) {
			msgRetryCount++;
			if (msgRetryCount == 1) {
				FPT_SendMsg(port, SMPARITY);
			} else {
				FPT_SendMsg(port, SMDEV_RESET);

				FPT_sssyncv(port, our_target, NARROW_SCSI,
					    currTar_Info);

				if (FPT_sccbMgrTbl[p_card][our_target].
				    TarEEValue & EE_SYNC_MASK) {

					FPT_sccbMgrTbl[p_card][our_target].
					    TarStatus &= ~TAR_SYNC_MASK;

				}

				if (FPT_sccbMgrTbl[p_card][our_target].
				    TarEEValue & EE_WIDE_SCSI) {

					FPT_sccbMgrTbl[p_card][our_target].
					    TarStatus &= ~TAR_WIDE_MASK;
				}

				FPT_queueFlushTargSccb(p_card, our_target,
						       SCCB_COMPLETE);
				FPT_SccbMgrTableInitTarget(p_card, our_target);
				return;
			}
		}
	} while (message == 0);

	if (((pCurrCard->globalFlags & F_CONLUN_IO) &&
	     ((currTar_Info->TarStatus & TAR_TAG_Q_MASK) != TAG_Q_TRYING))) {
		currTar_Info->TarLUNBusy[lun] = 1;
		pCurrCard->currentSCCB =
		    pCurrCard->discQ_Tbl[currTar_Info->LunDiscQ_Idx[lun]];
		if (pCurrCard->currentSCCB != NULL) {
			ACCEPT_MSG(port);
		} else {
			ACCEPT_MSG_ATN(port);
		}
	} else {
		currTar_Info->TarLUNBusy[0] = 1;

		if (tag) {
			if (pCurrCard->discQ_Tbl[tag] != NULL) {
				pCurrCard->currentSCCB =
				    pCurrCard->discQ_Tbl[tag];
				currTar_Info->TarTagQ_Cnt--;
				ACCEPT_MSG(port);
			} else {
				ACCEPT_MSG_ATN(port);
			}
		} else {
			pCurrCard->currentSCCB =
			    pCurrCard->discQ_Tbl[currTar_Info->LunDiscQ_Idx[0]];
			if (pCurrCard->currentSCCB != NULL) {
				ACCEPT_MSG(port);
			} else {
				ACCEPT_MSG_ATN(port);
			}
		}
	}

	if (pCurrCard->currentSCCB != NULL) {
		if (pCurrCard->currentSCCB->Sccb_scsistat == ABORT_ST) {
			/* During Abort Tag command, the target could have got re-selected
			   and completed the command. Check the select Q and remove the CCB
			   if it is in the Select Q */
			FPT_queueFindSccb(pCurrCard->currentSCCB, p_card);
		}
	}

	while (!(RDW_HARPOON((port + hp_intstat)) & (PHASE | RESET)) &&
	       !(RD_HARPOON(port + hp_scsisig) & SCSI_REQ) &&
	       (RD_HARPOON(port + hp_scsisig) & SCSI_BSY)) ;
}

static void FPT_SendMsg(unsigned long port, unsigned char message)
{
	while (!(RD_HARPOON(port + hp_scsisig) & SCSI_REQ)) {
		if (!(RD_HARPOON(port + hp_scsisig) & SCSI_BSY)) {

			WRW_HARPOON((port + hp_intstat), PHASE);
			return;
		}
	}

	WRW_HARPOON((port + hp_intstat), PHASE);
	if ((RD_HARPOON(port + hp_scsisig) & S_SCSI_PHZ) == S_MSGO_PH) {
		WRW_HARPOON((port + hp_intstat),
			    (BUS_FREE | PHASE | XFER_CNT_0));

		WR_HARPOON(port + hp_portctrl_0, SCSI_BUS_EN);

		WR_HARPOON(port + hp_scsidata_0, message);

		WR_HARPOON(port + hp_scsisig, (SCSI_ACK + S_ILL_PH));

		ACCEPT_MSG(port);

		WR_HARPOON(port + hp_portctrl_0, 0x00);

		if ((message == SMABORT) || (message == SMDEV_RESET) ||
		    (message == SMABORT_TAG)) {
			while (!
			       (RDW_HARPOON((port + hp_intstat)) &
				(BUS_FREE | PHASE))) {
			}

			if (RDW_HARPOON((port + hp_intstat)) & BUS_FREE) {
				WRW_HARPOON((port + hp_intstat), BUS_FREE);
			}
		}
	}
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_sdecm
 *
 * Description: Determine the proper responce to the message from the
 *              target device.
 *
 *---------------------------------------------------------------------*/
static void FPT_sdecm(unsigned char message, unsigned long port,
		      unsigned char p_card)
{
	struct sccb *currSCCB;
	struct sccb_card *CurrCard;
	struct sccb_mgr_tar_info *currTar_Info;

	CurrCard = &FPT_BL_Card[p_card];
	currSCCB = CurrCard->currentSCCB;

	currTar_Info = &FPT_sccbMgrTbl[p_card][currSCCB->TargID];

	if (message == SMREST_DATA_PTR) {
		if (!(currSCCB->Sccb_XferState & F_NO_DATA_YET)) {
			currSCCB->Sccb_ATC = currSCCB->Sccb_savedATC;

			FPT_hostDataXferRestart(currSCCB);
		}

		ACCEPT_MSG(port);
		WR_HARPOON(port + hp_autostart_1,
			   (AUTO_IMMED + DISCONNECT_START));
	}

	else if (message == SMCMD_COMP) {

		if (currSCCB->Sccb_scsistat == SELECT_Q_ST) {
			currTar_Info->TarStatus &=
			    ~(unsigned char)TAR_TAG_Q_MASK;
			currTar_Info->TarStatus |= (unsigned char)TAG_Q_REJECT;
		}

		ACCEPT_MSG(port);

	}

	else if ((message == SMNO_OP) || (message >= SMIDENT)
		 || (message == SMINIT_RECOVERY) || (message == SMREL_RECOVERY)) {

		ACCEPT_MSG(port);
		WR_HARPOON(port + hp_autostart_1,
			   (AUTO_IMMED + DISCONNECT_START));
	}

	else if (message == SMREJECT) {

		if ((currSCCB->Sccb_scsistat == SELECT_SN_ST) ||
		    (currSCCB->Sccb_scsistat == SELECT_WN_ST) ||
		    ((currTar_Info->TarStatus & TAR_SYNC_MASK) == SYNC_TRYING)
		    || ((currTar_Info->TarStatus & TAR_TAG_Q_MASK) ==
			TAG_Q_TRYING))
		{
			WRW_HARPOON((port + hp_intstat), BUS_FREE);

			ACCEPT_MSG(port);

			while ((!(RD_HARPOON(port + hp_scsisig) & SCSI_REQ)) &&
			       (!(RDW_HARPOON((port + hp_intstat)) & BUS_FREE)))
			{
			}

			if (currSCCB->Lun == 0x00) {
				if ((currSCCB->Sccb_scsistat == SELECT_SN_ST)) {

					currTar_Info->TarStatus |=
					    (unsigned char)SYNC_SUPPORTED;

					currTar_Info->TarEEValue &=
					    ~EE_SYNC_MASK;
				}

				else if ((currSCCB->Sccb_scsistat ==
					  SELECT_WN_ST)) {

					currTar_Info->TarStatus =
					    (currTar_Info->
					     TarStatus & ~WIDE_ENABLED) |
					    WIDE_NEGOCIATED;

					currTar_Info->TarEEValue &=
					    ~EE_WIDE_SCSI;

				}

				else if ((currTar_Info->
					  TarStatus & TAR_TAG_Q_MASK) ==
					 TAG_Q_TRYING) {
					currTar_Info->TarStatus =
					    (currTar_Info->
					     TarStatus & ~(unsigned char)
					     TAR_TAG_Q_MASK) | TAG_Q_REJECT;

					currSCCB->ControlByte &= ~F_USE_CMD_Q;
					CurrCard->discQCount--;
					CurrCard->discQ_Tbl[currSCCB->
							    Sccb_tag] = NULL;
					currSCCB->Sccb_tag = 0x00;

				}
			}

			if (RDW_HARPOON((port + hp_intstat)) & BUS_FREE) {

				if (currSCCB->Lun == 0x00) {
					WRW_HARPOON((port + hp_intstat),
						    BUS_FREE);
					CurrCard->globalFlags |= F_NEW_SCCB_CMD;
				}
			}

			else {

				if ((CurrCard->globalFlags & F_CONLUN_IO) &&
				    ((currTar_Info->
				      TarStatus & TAR_TAG_Q_MASK) !=
				     TAG_Q_TRYING))
					currTar_Info->TarLUNBusy[currSCCB->
								 Lun] = 1;
				else
					currTar_Info->TarLUNBusy[0] = 1;

				currSCCB->ControlByte &=
				    ~(unsigned char)F_USE_CMD_Q;

				WR_HARPOON(port + hp_autostart_1,
					   (AUTO_IMMED + DISCONNECT_START));

			}
		}

		else {
			ACCEPT_MSG(port);

			while ((!(RD_HARPOON(port + hp_scsisig) & SCSI_REQ)) &&
			       (!(RDW_HARPOON((port + hp_intstat)) & BUS_FREE)))
			{
			}

			if (!(RDW_HARPOON((port + hp_intstat)) & BUS_FREE)) {
				WR_HARPOON(port + hp_autostart_1,
					   (AUTO_IMMED + DISCONNECT_START));
			}
		}
	}

	else if (message == SMEXT) {

		ACCEPT_MSG(port);
		FPT_shandem(port, p_card, currSCCB);
	}

	else if (message == SMIGNORWR) {

		ACCEPT_MSG(port);	/* ACK the RESIDUE MSG */

		message = FPT_sfm(port, currSCCB);

		if (currSCCB->Sccb_scsimsg != SMPARITY)
			ACCEPT_MSG(port);
		WR_HARPOON(port + hp_autostart_1,
			   (AUTO_IMMED + DISCONNECT_START));
	}

	else {

		currSCCB->HostStatus = SCCB_PHASE_SEQUENCE_FAIL;
		currSCCB->Sccb_scsimsg = SMREJECT;

		ACCEPT_MSG_ATN(port);
		WR_HARPOON(port + hp_autostart_1,
			   (AUTO_IMMED + DISCONNECT_START));
	}
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_shandem
 *
 * Description: Decide what to do with the extended message.
 *
 *---------------------------------------------------------------------*/
static void FPT_shandem(unsigned long port, unsigned char p_card,
			struct sccb *pCurrSCCB)
{
	unsigned char length, message;

	length = FPT_sfm(port, pCurrSCCB);
	if (length) {

		ACCEPT_MSG(port);
		message = FPT_sfm(port, pCurrSCCB);
		if (message) {

			if (message == SMSYNC) {

				if (length == 0x03) {

					ACCEPT_MSG(port);
					FPT_stsyncn(port, p_card);
				} else {

					pCurrSCCB->Sccb_scsimsg = SMREJECT;
					ACCEPT_MSG_ATN(port);
				}
			} else if (message == SMWDTR) {

				if (length == 0x02) {

					ACCEPT_MSG(port);
					FPT_stwidn(port, p_card);
				} else {

					pCurrSCCB->Sccb_scsimsg = SMREJECT;
					ACCEPT_MSG_ATN(port);

					WR_HARPOON(port + hp_autostart_1,
						   (AUTO_IMMED +
						    DISCONNECT_START));
				}
			} else {

				pCurrSCCB->Sccb_scsimsg = SMREJECT;
				ACCEPT_MSG_ATN(port);

				WR_HARPOON(port + hp_autostart_1,
					   (AUTO_IMMED + DISCONNECT_START));
			}
		} else {
			if (pCurrSCCB->Sccb_scsimsg != SMPARITY)
				ACCEPT_MSG(port);
			WR_HARPOON(port + hp_autostart_1,
				   (AUTO_IMMED + DISCONNECT_START));
		}
	} else {
		if (pCurrSCCB->Sccb_scsimsg == SMPARITY)
			WR_HARPOON(port + hp_autostart_1,
				   (AUTO_IMMED + DISCONNECT_START));
	}
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_sisyncn
 *
 * Description: Read in a message byte from the SCSI bus, and check
 *              for a parity error.
 *
 *---------------------------------------------------------------------*/

static unsigned char FPT_sisyncn(unsigned long port, unsigned char p_card,
				 unsigned char syncFlag)
{
	struct sccb *currSCCB;
	struct sccb_mgr_tar_info *currTar_Info;

	currSCCB = FPT_BL_Card[p_card].currentSCCB;
	currTar_Info = &FPT_sccbMgrTbl[p_card][currSCCB->TargID];

	if (!((currTar_Info->TarStatus & TAR_SYNC_MASK) == SYNC_TRYING)) {

		WRW_HARPOON((port + ID_MSG_STRT),
			    (MPM_OP + AMSG_OUT +
			     (currSCCB->
			      Sccb_idmsg & ~(unsigned char)DISC_PRIV)));

		WRW_HARPOON((port + ID_MSG_STRT + 2), BRH_OP + ALWAYS + CMDPZ);

		WRW_HARPOON((port + SYNC_MSGS + 0),
			    (MPM_OP + AMSG_OUT + SMEXT));
		WRW_HARPOON((port + SYNC_MSGS + 2), (MPM_OP + AMSG_OUT + 0x03));
		WRW_HARPOON((port + SYNC_MSGS + 4),
			    (MPM_OP + AMSG_OUT + SMSYNC));

		if ((currTar_Info->TarEEValue & EE_SYNC_MASK) == EE_SYNC_20MB)

			WRW_HARPOON((port + SYNC_MSGS + 6),
				    (MPM_OP + AMSG_OUT + 12));

		else if ((currTar_Info->TarEEValue & EE_SYNC_MASK) ==
			 EE_SYNC_10MB)

			WRW_HARPOON((port + SYNC_MSGS + 6),
				    (MPM_OP + AMSG_OUT + 25));

		else if ((currTar_Info->TarEEValue & EE_SYNC_MASK) ==
			 EE_SYNC_5MB)

			WRW_HARPOON((port + SYNC_MSGS + 6),
				    (MPM_OP + AMSG_OUT + 50));

		else
			WRW_HARPOON((port + SYNC_MSGS + 6),
				    (MPM_OP + AMSG_OUT + 00));

		WRW_HARPOON((port + SYNC_MSGS + 8), (RAT_OP));
		WRW_HARPOON((port + SYNC_MSGS + 10),
			    (MPM_OP + AMSG_OUT + DEFAULT_OFFSET));
		WRW_HARPOON((port + SYNC_MSGS + 12), (BRH_OP + ALWAYS + NP));

		if (syncFlag == 0) {
			WR_HARPOON(port + hp_autostart_3,
				   (SELECT + SELCHK_STRT));
			currTar_Info->TarStatus =
			    ((currTar_Info->
			      TarStatus & ~(unsigned char)TAR_SYNC_MASK) |
			     (unsigned char)SYNC_TRYING);
		} else {
			WR_HARPOON(port + hp_autostart_3,
				   (AUTO_IMMED + CMD_ONLY_STRT));
		}

		return 1;
	}

	else {

		currTar_Info->TarStatus |= (unsigned char)SYNC_SUPPORTED;
		currTar_Info->TarEEValue &= ~EE_SYNC_MASK;
		return 0;
	}
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_stsyncn
 *
 * Description: The has sent us a Sync Nego message so handle it as
 *              necessary.
 *
 *---------------------------------------------------------------------*/
static void FPT_stsyncn(unsigned long port, unsigned char p_card)
{
	unsigned char sync_msg, offset, sync_reg, our_sync_msg;
	struct sccb *currSCCB;
	struct sccb_mgr_tar_info *currTar_Info;

	currSCCB = FPT_BL_Card[p_card].currentSCCB;
	currTar_Info = &FPT_sccbMgrTbl[p_card][currSCCB->TargID];

	sync_msg = FPT_sfm(port, currSCCB);

	if ((sync_msg == 0x00) && (currSCCB->Sccb_scsimsg == SMPARITY)) {
		WR_HARPOON(port + hp_autostart_1,
			   (AUTO_IMMED + DISCONNECT_START));
		return;
	}

	ACCEPT_MSG(port);

	offset = FPT_sfm(port, currSCCB);

	if ((offset == 0x00) && (currSCCB->Sccb_scsimsg == SMPARITY)) {
		WR_HARPOON(port + hp_autostart_1,
			   (AUTO_IMMED + DISCONNECT_START));
		return;
	}

	if ((currTar_Info->TarEEValue & EE_SYNC_MASK) == EE_SYNC_20MB)

		our_sync_msg = 12;	/* Setup our Message to 20mb/s */

	else if ((currTar_Info->TarEEValue & EE_SYNC_MASK) == EE_SYNC_10MB)

		our_sync_msg = 25;	/* Setup our Message to 10mb/s */

	else if ((currTar_Info->TarEEValue & EE_SYNC_MASK) == EE_SYNC_5MB)

		our_sync_msg = 50;	/* Setup our Message to 5mb/s */
	else

		our_sync_msg = 0;	/* Message = Async */

	if (sync_msg < our_sync_msg) {
		sync_msg = our_sync_msg;	/*if faster, then set to max. */
	}

	if (offset == ASYNC)
		sync_msg = ASYNC;

	if (offset > MAX_OFFSET)
		offset = MAX_OFFSET;

	sync_reg = 0x00;

	if (sync_msg > 12)

		sync_reg = 0x20;	/* Use 10MB/s */

	if (sync_msg > 25)

		sync_reg = 0x40;	/* Use 6.6MB/s */

	if (sync_msg > 38)

		sync_reg = 0x60;	/* Use 5MB/s */

	if (sync_msg > 50)

		sync_reg = 0x80;	/* Use 4MB/s */

	if (sync_msg > 62)

		sync_reg = 0xA0;	/* Use 3.33MB/s */

	if (sync_msg > 75)

		sync_reg = 0xC0;	/* Use 2.85MB/s */

	if (sync_msg > 87)

		sync_reg = 0xE0;	/* Use 2.5MB/s */

	if (sync_msg > 100) {

		sync_reg = 0x00;	/* Use ASYNC */
		offset = 0x00;
	}

	if (currTar_Info->TarStatus & WIDE_ENABLED)

		sync_reg |= offset;

	else

		sync_reg |= (offset | NARROW_SCSI);

	FPT_sssyncv(port, currSCCB->TargID, sync_reg, currTar_Info);

	if (currSCCB->Sccb_scsistat == SELECT_SN_ST) {

		ACCEPT_MSG(port);

		currTar_Info->TarStatus = ((currTar_Info->TarStatus &
					    ~(unsigned char)TAR_SYNC_MASK) |
					   (unsigned char)SYNC_SUPPORTED);

		WR_HARPOON(port + hp_autostart_1,
			   (AUTO_IMMED + DISCONNECT_START));
	}

	else {

		ACCEPT_MSG_ATN(port);

		FPT_sisyncr(port, sync_msg, offset);

		currTar_Info->TarStatus = ((currTar_Info->TarStatus &
					    ~(unsigned char)TAR_SYNC_MASK) |
					   (unsigned char)SYNC_SUPPORTED);
	}
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_sisyncr
 *
 * Description: Answer the targets sync message.
 *
 *---------------------------------------------------------------------*/
static void FPT_sisyncr(unsigned long port, unsigned char sync_pulse,
			unsigned char offset)
{
	ARAM_ACCESS(port);
	WRW_HARPOON((port + SYNC_MSGS + 0), (MPM_OP + AMSG_OUT + SMEXT));
	WRW_HARPOON((port + SYNC_MSGS + 2), (MPM_OP + AMSG_OUT + 0x03));
	WRW_HARPOON((port + SYNC_MSGS + 4), (MPM_OP + AMSG_OUT + SMSYNC));
	WRW_HARPOON((port + SYNC_MSGS + 6), (MPM_OP + AMSG_OUT + sync_pulse));
	WRW_HARPOON((port + SYNC_MSGS + 8), (RAT_OP));
	WRW_HARPOON((port + SYNC_MSGS + 10), (MPM_OP + AMSG_OUT + offset));
	WRW_HARPOON((port + SYNC_MSGS + 12), (BRH_OP + ALWAYS + NP));
	SGRAM_ACCESS(port);

	WR_HARPOON(port + hp_portctrl_0, SCSI_PORT);
	WRW_HARPOON((port + hp_intstat), CLR_ALL_INT_1);

	WR_HARPOON(port + hp_autostart_3, (AUTO_IMMED + CMD_ONLY_STRT));

	while (!(RDW_HARPOON((port + hp_intstat)) & (BUS_FREE | AUTO_INT))) {
	}
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_siwidn
 *
 * Description: Read in a message byte from the SCSI bus, and check
 *              for a parity error.
 *
 *---------------------------------------------------------------------*/

static unsigned char FPT_siwidn(unsigned long port, unsigned char p_card)
{
	struct sccb *currSCCB;
	struct sccb_mgr_tar_info *currTar_Info;

	currSCCB = FPT_BL_Card[p_card].currentSCCB;
	currTar_Info = &FPT_sccbMgrTbl[p_card][currSCCB->TargID];

	if (!((currTar_Info->TarStatus & TAR_WIDE_MASK) == WIDE_NEGOCIATED)) {

		WRW_HARPOON((port + ID_MSG_STRT),
			    (MPM_OP + AMSG_OUT +
			     (currSCCB->
			      Sccb_idmsg & ~(unsigned char)DISC_PRIV)));

		WRW_HARPOON((port + ID_MSG_STRT + 2), BRH_OP + ALWAYS + CMDPZ);

		WRW_HARPOON((port + SYNC_MSGS + 0),
			    (MPM_OP + AMSG_OUT + SMEXT));
		WRW_HARPOON((port + SYNC_MSGS + 2), (MPM_OP + AMSG_OUT + 0x02));
		WRW_HARPOON((port + SYNC_MSGS + 4),
			    (MPM_OP + AMSG_OUT + SMWDTR));
		WRW_HARPOON((port + SYNC_MSGS + 6), (RAT_OP));
		WRW_HARPOON((port + SYNC_MSGS + 8),
			    (MPM_OP + AMSG_OUT + SM16BIT));
		WRW_HARPOON((port + SYNC_MSGS + 10), (BRH_OP + ALWAYS + NP));

		WR_HARPOON(port + hp_autostart_3, (SELECT + SELCHK_STRT));

		currTar_Info->TarStatus = ((currTar_Info->TarStatus &
					    ~(unsigned char)TAR_WIDE_MASK) |
					   (unsigned char)WIDE_ENABLED);

		return 1;
	}

	else {

		currTar_Info->TarStatus = ((currTar_Info->TarStatus &
					    ~(unsigned char)TAR_WIDE_MASK) |
					   WIDE_NEGOCIATED);

		currTar_Info->TarEEValue &= ~EE_WIDE_SCSI;
		return 0;
	}
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_stwidn
 *
 * Description: The has sent us a Wide Nego message so handle it as
 *              necessary.
 *
 *---------------------------------------------------------------------*/
static void FPT_stwidn(unsigned long port, unsigned char p_card)
{
	unsigned char width;
	struct sccb *currSCCB;
	struct sccb_mgr_tar_info *currTar_Info;

	currSCCB = FPT_BL_Card[p_card].currentSCCB;
	currTar_Info = &FPT_sccbMgrTbl[p_card][currSCCB->TargID];

	width = FPT_sfm(port, currSCCB);

	if ((width == 0x00) && (currSCCB->Sccb_scsimsg == SMPARITY)) {
		WR_HARPOON(port + hp_autostart_1,
			   (AUTO_IMMED + DISCONNECT_START));
		return;
	}

	if (!(currTar_Info->TarEEValue & EE_WIDE_SCSI))
		width = 0;

	if (width) {
		currTar_Info->TarStatus |= WIDE_ENABLED;
		width = 0;
	} else {
		width = NARROW_SCSI;
		currTar_Info->TarStatus &= ~WIDE_ENABLED;
	}

	FPT_sssyncv(port, currSCCB->TargID, width, currTar_Info);

	if (currSCCB->Sccb_scsistat == SELECT_WN_ST) {

		currTar_Info->TarStatus |= WIDE_NEGOCIATED;

		if (!
		    ((currTar_Info->TarStatus & TAR_SYNC_MASK) ==
		     SYNC_SUPPORTED)) {
			ACCEPT_MSG_ATN(port);
			ARAM_ACCESS(port);
			FPT_sisyncn(port, p_card, 1);
			currSCCB->Sccb_scsistat = SELECT_SN_ST;
			SGRAM_ACCESS(port);
		} else {
			ACCEPT_MSG(port);
			WR_HARPOON(port + hp_autostart_1,
				   (AUTO_IMMED + DISCONNECT_START));
		}
	}

	else {

		ACCEPT_MSG_ATN(port);

		if (currTar_Info->TarEEValue & EE_WIDE_SCSI)
			width = SM16BIT;
		else
			width = SM8BIT;

		FPT_siwidr(port, width);

		currTar_Info->TarStatus |= (WIDE_NEGOCIATED | WIDE_ENABLED);
	}
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_siwidr
 *
 * Description: Answer the targets Wide nego message.
 *
 *---------------------------------------------------------------------*/
static void FPT_siwidr(unsigned long port, unsigned char width)
{
	ARAM_ACCESS(port);
	WRW_HARPOON((port + SYNC_MSGS + 0), (MPM_OP + AMSG_OUT + SMEXT));
	WRW_HARPOON((port + SYNC_MSGS + 2), (MPM_OP + AMSG_OUT + 0x02));
	WRW_HARPOON((port + SYNC_MSGS + 4), (MPM_OP + AMSG_OUT + SMWDTR));
	WRW_HARPOON((port + SYNC_MSGS + 6), (RAT_OP));
	WRW_HARPOON((port + SYNC_MSGS + 8), (MPM_OP + AMSG_OUT + width));
	WRW_HARPOON((port + SYNC_MSGS + 10), (BRH_OP + ALWAYS + NP));
	SGRAM_ACCESS(port);

	WR_HARPOON(port + hp_portctrl_0, SCSI_PORT);
	WRW_HARPOON((port + hp_intstat), CLR_ALL_INT_1);

	WR_HARPOON(port + hp_autostart_3, (AUTO_IMMED + CMD_ONLY_STRT));

	while (!(RDW_HARPOON((port + hp_intstat)) & (BUS_FREE | AUTO_INT))) {
	}
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_sssyncv
 *
 * Description: Write the desired value to the Sync Register for the
 *              ID specified.
 *
 *---------------------------------------------------------------------*/
static void FPT_sssyncv(unsigned long p_port, unsigned char p_id,
			unsigned char p_sync_value,
			struct sccb_mgr_tar_info *currTar_Info)
{
	unsigned char index;

	index = p_id;

	switch (index) {

	case 0:
		index = 12;	/* hp_synctarg_0 */
		break;
	case 1:
		index = 13;	/* hp_synctarg_1 */
		break;
	case 2:
		index = 14;	/* hp_synctarg_2 */
		break;
	case 3:
		index = 15;	/* hp_synctarg_3 */
		break;
	case 4:
		index = 8;	/* hp_synctarg_4 */
		break;
	case 5:
		index = 9;	/* hp_synctarg_5 */
		break;
	case 6:
		index = 10;	/* hp_synctarg_6 */
		break;
	case 7:
		index = 11;	/* hp_synctarg_7 */
		break;
	case 8:
		index = 4;	/* hp_synctarg_8 */
		break;
	case 9:
		index = 5;	/* hp_synctarg_9 */
		break;
	case 10:
		index = 6;	/* hp_synctarg_10 */
		break;
	case 11:
		index = 7;	/* hp_synctarg_11 */
		break;
	case 12:
		index = 0;	/* hp_synctarg_12 */
		break;
	case 13:
		index = 1;	/* hp_synctarg_13 */
		break;
	case 14:
		index = 2;	/* hp_synctarg_14 */
		break;
	case 15:
		index = 3;	/* hp_synctarg_15 */

	}

	WR_HARPOON(p_port + hp_synctarg_base + index, p_sync_value);

	currTar_Info->TarSyncCtrl = p_sync_value;
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_sresb
 *
 * Description: Reset the desired card's SCSI bus.
 *
 *---------------------------------------------------------------------*/
static void FPT_sresb(unsigned long port, unsigned char p_card)
{
	unsigned char scsiID, i;

	struct sccb_mgr_tar_info *currTar_Info;

	WR_HARPOON(port + hp_page_ctrl,
		   (RD_HARPOON(port + hp_page_ctrl) | G_INT_DISABLE));
	WRW_HARPOON((port + hp_intstat), CLR_ALL_INT);

	WR_HARPOON(port + hp_scsictrl_0, SCSI_RST);

	scsiID = RD_HARPOON(port + hp_seltimeout);
	WR_HARPOON(port + hp_seltimeout, TO_5ms);
	WRW_HARPOON((port + hp_intstat), TIMEOUT);

	WR_HARPOON(port + hp_portctrl_0, (SCSI_PORT | START_TO));

	while (!(RDW_HARPOON((port + hp_intstat)) & TIMEOUT)) {
	}

	WR_HARPOON(port + hp_seltimeout, scsiID);

	WR_HARPOON(port + hp_scsictrl_0, ENA_SCAM_SEL);

	FPT_Wait(port, TO_5ms);

	WRW_HARPOON((port + hp_intstat), CLR_ALL_INT);

	WR_HARPOON(port + hp_int_mask, (RD_HARPOON(port + hp_int_mask) | 0x00));

	for (scsiID = 0; scsiID < MAX_SCSI_TAR; scsiID++) {
		currTar_Info = &FPT_sccbMgrTbl[p_card][scsiID];

		if (currTar_Info->TarEEValue & EE_SYNC_MASK) {
			currTar_Info->TarSyncCtrl = 0;
			currTar_Info->TarStatus &= ~TAR_SYNC_MASK;
		}

		if (currTar_Info->TarEEValue & EE_WIDE_SCSI) {
			currTar_Info->TarStatus &= ~TAR_WIDE_MASK;
		}

		FPT_sssyncv(port, scsiID, NARROW_SCSI, currTar_Info);

		FPT_SccbMgrTableInitTarget(p_card, scsiID);
	}

	FPT_BL_Card[p_card].scanIndex = 0x00;
	FPT_BL_Card[p_card].currentSCCB = NULL;
	FPT_BL_Card[p_card].globalFlags &= ~(F_TAG_STARTED | F_HOST_XFER_ACT
					     | F_NEW_SCCB_CMD);
	FPT_BL_Card[p_card].cmdCounter = 0x00;
	FPT_BL_Card[p_card].discQCount = 0x00;
	FPT_BL_Card[p_card].tagQ_Lst = 0x01;

	for (i = 0; i < QUEUE_DEPTH; i++)
		FPT_BL_Card[p_card].discQ_Tbl[i] = NULL;

	WR_HARPOON(port + hp_page_ctrl,
		   (RD_HARPOON(port + hp_page_ctrl) & ~G_INT_DISABLE));

}

/*---------------------------------------------------------------------
 *
 * Function: FPT_ssenss
 *
 * Description: Setup for the Auto Sense command.
 *
 *---------------------------------------------------------------------*/
static void FPT_ssenss(struct sccb_card *pCurrCard)
{
	unsigned char i;
	struct sccb *currSCCB;

	currSCCB = pCurrCard->currentSCCB;

	currSCCB->Save_CdbLen = currSCCB->CdbLength;

	for (i = 0; i < 6; i++) {

		currSCCB->Save_Cdb[i] = currSCCB->Cdb[i];
	}

	currSCCB->CdbLength = SIX_BYTE_CMD;
	currSCCB->Cdb[0] = SCSI_REQUEST_SENSE;
	currSCCB->Cdb[1] = currSCCB->Cdb[1] & (unsigned char)0xE0;	/*Keep LUN. */
	currSCCB->Cdb[2] = 0x00;
	currSCCB->Cdb[3] = 0x00;
	currSCCB->Cdb[4] = currSCCB->RequestSenseLength;
	currSCCB->Cdb[5] = 0x00;

	currSCCB->Sccb_XferCnt = (unsigned long)currSCCB->RequestSenseLength;

	currSCCB->Sccb_ATC = 0x00;

	currSCCB->Sccb_XferState |= F_AUTO_SENSE;

	currSCCB->Sccb_XferState &= ~F_SG_XFER;

	currSCCB->Sccb_idmsg = currSCCB->Sccb_idmsg & ~(unsigned char)DISC_PRIV;

	currSCCB->ControlByte = 0x00;

	currSCCB->Sccb_MGRFlags &= F_STATUSLOADED;
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_sxfrp
 *
 * Description: Transfer data into the bit bucket until the device
 *              decides to switch phase.
 *
 *---------------------------------------------------------------------*/

static void FPT_sxfrp(unsigned long p_port, unsigned char p_card)
{
	unsigned char curr_phz;

	DISABLE_AUTO(p_port);

	if (FPT_BL_Card[p_card].globalFlags & F_HOST_XFER_ACT) {

		FPT_hostDataXferAbort(p_port, p_card,
				      FPT_BL_Card[p_card].currentSCCB);

	}

	/* If the Automation handled
  Flend o

  Fltransfer
  Fn do not
	   match
  Flphase or we will get outCCB sync witntainsISR. lashPo*/

	if (RDW_HARPOON((p_port + hp_intstat)) &s fil (BUS_FREE | XFER_CNT_0 | AUTO_INT))
		return;

	WRDevelopers Kit, with xfercnt_0, 0x00)bilicurr_phz = RD.  It was provided byscsisig) & (unsigned char)S_SCSI_PHZbility Developer's Kit, with minor mo,rd N. Zubkoe forty.  It was provided byhich wo, m of 16 e forwhile (!ver Developer's Kit, with minor modifons by LeonarRESETodifficatiotionm of 16 se=
		(arate
  source files, which would have unnecessarily clutte)nux {
	 Drivm of 16 sd have unnecessari cluIOBIT) ee Le been combined into thiKit,ctrl_0, CON fil(y clutORT | HOST BUSTYPE

*/

N#ifde for LICEN995-.  It was provided by Busor mo & FIFO_EMPTY)def CON	e FAILURE         0xFFFfifodataes ha			}
		} elseef CONFIG_SCSI_FLASHPOINT

#define MAX_CARDS	8
#undef BUSTYPE_PCI

#defin_PCI
WR	0xA01

#defe FAILURE         0xFFFFFFFFL

struct sccb;
tyedef voty.  It was provided byt sccb *); in FA

struct sc
	}			/* E SCCB Wight loop for padding cb * I/Os the Fnt
  right 1995-1996 by Mylex Corporation.  All Rights Reserved

  Thiunsignt;
	unsigned char si_id;
	uhich would 

*/
REQux c	break;
	}ave been combined into thiefine MAX_CARD8
#undef BUSTYPE_PCI

#define CRCMASK	0xA0ed short si_FAILURE         0xFFFFFFFFL

struct sccb;
typedef id (*CALL_BK_FN) (struct sccb *);

stgs;
#define per_targ_ultra_nego;
	unsigned short si_per_targ_no_disc;
	uty.  It was provided byautostargic shor8
#ufor
  MMED + DISCONNECT_STAsi_intveright 1995-1996 by Mylex Corporation.  All Rigfor
  Linuef COgs;
 Driver Developer's Kit, with minor modificcationICMD_COMP | ITAR_RITYnux cefine LOW;
};

#s avaBUSTYPE_PCI	  0x3

#define SUPPORT_16TA		hts Reserved
SELdisc_OS_r}

/*-efine FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100


 *
 * Funcnt.c: FPT_schkdddefine DescripOON_FAMake sureg_fasthas been flushed from bothuct ss and abortine Manager treat  Floperint.cs if necessary.defineefine FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#nt
 or mic voidAMILY      have unneclong Kit,, ave unnecessa p_card)
{
	ave unnecshit, TimeOutLoop;ed char ReqengthsPthe bilistruct sccb *m ofSCCB form oftaPo =AMILYBL_Card[;
	uns].m ofenttaPointeDrivNSE.FtaPo->Sccbwide_ntat != DATA_OU	  0is file isatus;
	unsigned char TargetStatusINunsi0x0020ompatibiS_reservetus;
	unsigned XferStatetruc_ODD_BALL Zub0x00
	er;
	unsisigned ATC +=ed char CcbRes1;
	unsCnt - 1xA001
igned long SensePointer= 1CALL_BK_FN SccbCallback;igned c= ~ar Reserved1;
ntvethe scsi direcevision;
	unsread), have unnecuestS)n the f5];
	unsigned     0xFFFFFFFFL

 in the f0
#deb_mgr_iigned long Reserved2;
	unigned long SensePointeCALL_BK_FN SccbCallback;	/* VO0;
	unsignedne EXTENDED_TRACorporation.  All RigPARI
	unned char TargID;
	uHostigneus == taPo 0x00LETE charLL_BK_FN Sccbgned long Scb_res1tual a_ERRt;	/* Identifies board basethe indivitual adhort ScMILYhostData	unsAhe USccbV,h;
	unse filetaPo Copyright 1;
	unsigned er_targ_wide_nego;
	unsigACK0x002gs;
nseLength;
ng Sccigned char Sccb_scsistat;
	unsnsigned char si_lun;
	unsignDriver Developer'sCorporation.  All Rigs by Leounsignedb[12];
	uct st;
	unsigned chait, with offsene Muld have unnecessar0x1Funsignet si_fla];
	unsigne;
	unsigned long Sccb_savedATC;
	

  Thchar Save_Cdb[6];
	unsigar Sccb_scsistat;
	unsigned char Sccb_ned shoMana|| (nseLength;
++ > 0x3000OFT_REt si_flags;
taLengseparate
  sour files, which would h

*/
BSYfinee copyrigh;HostSta3];
	unsigned     0xFFFFFFFFL

struct sccb;
type||icationned char Save_CdbLen;
	unsigned char Sccb_XferState;
	u       0xOMMAND  =MD_Q           tatuO_PH          0x  0x10	/* Write */
#define Sluttdisc;
s;
	unsigned char SCCBReefine MAX_C	unsigBUSTxA001
#defin char CcbRes1;
	unsigned charrvedd N.REDypedef vgned char CcbRes1;
	unsigned char_PCI
d N. DIRunsignedMILY the ectiInsigned char S

structine b_mgr_infoReset */
#defiOunsigned char S

struct sccb_mgr_infoMILY xfrp SELECT_WN_ST    4	erved;
	unsigned chaLATION	  0x0008
#define 
#defins by LeonarUN	  0x0002
#define Starg_no_disc;
	useg;
	unsigned char Sccb_scsimsg;	/for
  LinntvecReset */
#decode SELECT_WN_ST    4	/* Se
ort SccbOSFlaO_REQUEST_SENSE    0x01	/* No Requened short 40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAMILY inits  0x02

/* SCCB struSetupb_res manager fields in thisb_resardware structure' 
 */

#pragma pack(1)
struct sccb {
	unsigned char OperationCode;
	unsigned ch  0x0(	unsigned lon_widcbd char CdbLength;
	unsigned	unsigned l_mgr_tar_infoong DaTar_Infor HostSta0x04

->TargID > MAXly cluTARER_O */
#definLun_DATA_ULUN char Cdb[12];
	un_BK_Fd withou = &MILY  cbMgrTbles[2];
	[/
#define SCCB] in /
#defines1;
	unsigned =RED  ;_DATA_OVER_RUN     	/* VO/
#definectiLengtht error */
#definOSCCB as Cod0	/* SCATTER_GATHN. ZOMMANDER_OUT     phase sequence failure */RESIDUAL_SG_GROSS_FWO_AUTO_ATA_OVER_RUN SG
	unsing SccbDATA_OVER_RUN          0x1Fefind N.sMaster error. */
#de long S
#defi_reserveFAIL    0x14	/* Ta	/* n theRR               	unsigned |CCB_ine SELECT_r HostSt/
#definControlByed charUSE_N	  Qccb_backli#defid withouine S long S& defiTAG_Q_MASK)	/*       REJECTFT_RE 0x01
#define SCCB_ABed lo         A001
b_mgefin2
#define SCCB_ERROR    |#define TRYINGrity e/ ManageFor !single	unsi device
#desystem  &. no of allow Disconnect
	or comm
 * is tag_q typeLinux
st SCCmdm BusL Board */
 EnableSccbOSFine MAX_LUN         32
#dDisine nsign/*
  t's (atesd char CcbRes[2];
	uglobalFlag    F_SINGLE_DEVICgned&&Numbhar Targefine SCCB_ERROR          ALLOWine SOFR_OUt byte per element. */

#define RD_Q'ing. */
BM_ER*/HostStatus;element. */

#define RD_HARPOON(ioporR_OUT     inb((u32)ioport)
#define RDW_HARPOON(ioportaster error. */idmsg er bbyte ave unnecessar(SMIDENTYPERITY_PRIV) |_FAIL    Lu];
	unsicbOSFlags;fine WR_HARPOON(iopo have unnecessarial, (u32)ne WRW_HARPOON(iop/
#definFlags;
	unsig2
#define SCCB_e SCel(data, (u32)(ioport + ofgned tal, (2
#define SCCB_PHASEMGRfine S(7)+BIT(6))
#define  SsgseIT(7)+BIT(6))
#define  Sd2;
G               BIT(6)
aved(BIT(7)+BIT(/*NumbYNC_MASK    VirlectiPtrbyte(7)+BIT(IT(5)+BIT(4))
#_forwardlink_ENABNULL             BIT(4backne  WIDWIDE_NEGOCIA)          BIT(6)
har Targ=	unsigned_Srt;	YNC_MASK    s;
	unsigned sIN_PROCESS          BIT(6)
csival, (SMNO_OP;
           0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE            MMAND T_ST    0x02

/* SCCB struDetermin16
#ds the F
 * calls theapproprined fARPOON_ardware structure' 
 */

#pragma pack(1)
struct sccb {
	unsigned char OperationCode;
	unsigned c ABORT_ST   trolByte;
	unsi1	/* d char CdbLength;
	unsigned char Reqength;the _ref;
	nsign(*nsign))      outb
	und char CdbLengte forDISABLE_for
s provie fornsigned cportval)      outb((u8) ned short si_per_targ_wide_nego;
	u    0x20	/*Insignedgned chs_aLengion tsigned cCCB_DagQ_Cnt;
	ed char _WN_ST  _synCB      (corr2
#d the F))

    40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAecti Out(0)
#d8
#define F_ODD_BALL_y_ra upr compne  BusMasterNC_20Xbowine  EE_WIDE_SCSI      BIT(7)

struct sccb_mgr_tar_info {

	struct sccb *TarSelQ_Head;
	struct sccb *Tdefine trolByte;
	unsigned char CdbLength;
	unsigne;
	unsigned long DataPointer;
	unsigned char CcbRes[2];
	unsigned char   1
#define S	/* NEGOchar Cdb[12];t_syncxitt's No      record    (T_XFtus;
	unsigned char TargtStatus;
	uns;Byte 24 of eerpom */
	unsigned ( Select w\ Bus  | F_NOine S_YE BuffeREQUEST_SENSE    0x01	/* No Request Sense Buffeg;
	unsigned char Sccb_scsimsg;	/dual files have been combinng si_secondary_range (ENDine S + ne	MODEL  0x0001
#sg forcb *	unsProativorsigned  SCSIar CcbRes[2];
	EL_DWgned char CcbRes1;
	uns	/* V= 0O_AUTO_stStatus;
	unsiefine SCCB_ABO_res1tatusd N. OUsigned or OS/2 */
	unsigned long Sccb_res1;
	unsigne/*1 for cb_MGRFlags;
	unsigned status;VER_RUNA001
e Data Nego */
#define SELET_Q_ST     5	/* Select w\ Tagged Q'ing *rt si_per_targ_no_dis Wide Da ABORT_ST        11

#define            0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE            ;	/* Ind no. */
	unsigned long niBaseAupPort Address of carne  XBOW
	unsigned char niSysConf;	/* Adapter Configuration byte - Byte 16 of eeprom map */
	unsigned char niScsiInf;	/* SCSI Configuration byte - Byte 17 of eeprom map */
	unsigned char niScamConf;	/* SCAM Configuration byte - Bytte 20 of eeprom map */
	unsigned char niAdapId;	/* Host Adapter ID - Byte 24 of eerpom map */
	unsigneigned niSyncTbl[MAX_SCSI_TAR / 2]CB_SUelect w\ Bus  niSyncTbl[MAX_SCSI_TAR / 2];	/*targets */
	ue	MODEL_LT		1
#define	MOD_SCSI_TAR][4];	/* Compressed Scam name string of Targets */
};

#define	MODEL_LT		1
#define	MODEL_DL		2
#define	MODEL_LW		3
#define	MODEL_DW		4

struct sccb_card {
	struct sccb *currentSCCB;
	struct sccb_mgr_info *cardInfo;

	unsigned long ioPort;

	unsigned short cmdCounterINunsigned char discQCount;
	unsigned char tagQ_Lst;
	unsigned char cardIndex;
	unsigned char scanIndex;
	unsigned char globalFlags;
	unsigned char ourId;
	struct nvram_info *pNvRamInfo;
	struct sccb *discQ_Tbl[QUEUE_DEPTH];


};

#define F_TAG_STARTED		0x01
#define F_CONLUN_IO			0x02
#define F_DO_RENEGO			0x04
#define Cne MAX_ no. */
	unsigned long niLoaST_XFECDB into    (BshPoint.c 
 * ry_ra iAddrine  EE_WIDE_SCSI      BIT(7)

struct sccb_mgr_tar_info {

	struct sccb *TarSelQ_Head;
	struct sccb *D     0Q_Tail;
	unsigned char TarLUN_CA;	/*Contingent A	unsigned long DataPoind char Req
	unscdb_reg	unsigned long Daiinter;
	unsigned char CcbRes[2];
	unsigned char HostStigned char7	/* Major problem! ET_GROSS_FW_ short Sccb_MGRFlags;
	unsigned shHASE_SEQUENCE_FAILsMasong ioPort;dbSCCB_IN_ SIX_BYT     flags;
	unsigned char si_card_s single n the forARAM_ACQ_REStatus;
	unsx02
#deCE_FAdefine	N	  STR MISC si_(ing Sc i <_XferCnt;	/  0x08
#d; i++
	unsigned 
#define  SMABORT                 0x06
#d* Selthe scsi dirx02
#de, (MPM_OP + AGROSS_F +SET   xA001
4+1	/*1 ISC_PRIV            ARDS	8
#   0x40

#define  SMSY
#define	SMRE[i]_intveORT_TAG	+= 2;
	unsigned char CcbR  0x08
#de!= TWELVE SMPARITYructISC_PRIV              BRH40

#deLWAYS + NP      	unsigned char si_card_family;
	un#undef BUST      te 24 of eerpom map */
	unGROSS_F 0x0F

#define  SIX_BYTE_CMD  ondary_ran3,#define SCSI_|definONLYne	SM_cardSG     0x0C
#define	SM40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FA long Ssigne  0x02

/* SCCB struBrtarg#defie    ng SC_20Mne MAX_complete mtivege byte08
#def_WIDE_SCSI      BIT(7)

struct sccb_mgr_tar_info {

	struct sccb *TarSelQ_Head;
	struct sccb * long ntrolByte;
	unsigned char CdbLength;
	unsigned/*iBaseA-		0x10
ne  SSGOOD to finish of

  is_2      6
 * letine TarEEisr -- Flaine  interruptt si_2      6
#define wnux
it#defe0
#d.TarEEWarLUuld wait heret si_E_ENABLE     3to be genCCB ed?
	nsigneDEL_LT		1
#define	MODSMDEV_RESET       DEL_LT		1
#define	MODEL_DL		2
#defiefine SCSI_PA		3
#define	MODEL__DISC    BIT(0)

#define  EE_SYNC_MASK      (BIT(0)+BIT(1))
#define  EE_SYNC_5MB       BIT(0)
#deMMODEL_NOuUCB define F_ODD_BALL_C SCCanagur  MODEL_N(ifshPohave one)ART_E  DEVICwhateverCB Manager treatsb_mgri0
#dvoledine  EE_WIDE_SCSI      BIT(7)

struct sccb_mgr_tar_info {

	struct sccb *TarSelQ_Head;
	struct sccb *MsgConf;	/* SCSI Configuration byte - Byte 17 of eeAllgiance */
	u MODEL_, 

#dIY   x01
#define  SMSAVE_DATA         0x00	/* SCCB completed without er        0x03
#define  SMDISC                  0x04
#define 0x0p */
	un
		 MODEL_Nb_XferCnt;	/* act

#defisMasefine efine  ORION_e SCCBuffer */
B */
#defe  TDEV_CATTER_GA/*1 for Normal dSet SCSI selection timed ouefine ]ntvec inw((u32)ioport)
yncCtrl* BusMasx;
	uns frovsigned efine , NARROWly cle filed withouxA001

#defe  hp_device_id_1       0x03	/./
#definTarEEValud chTRYIYNC  0x04
_DEV_0x;
	unersion 2 and higher */

#d_ERROR     er bned ch~defi0   0x06	A001

/* SelHarpoon Version 2 and higher */

#define  hp_sub_device_id_WIDEly cl/* LSB */

#define  hp_semaphore         0x0C
#define SCCB_MGR_ACT)
#de  BIT()
#defineMILYqueueF UCBgned(char Scc_res1;
	unsignx81

	/* gnedectiine Initfset)) off BusMefine /
#deccb_mgrgned char CcbRes1;
)
#define = ABOR	unsigdefinigned char cardIndex;
	unsig;
	unsigntvect;
	d char CcbRes[2];
	udiscQ_ion tus;
	unsigned tag] !ne SC    p */
	unsi            BIT(4)	/*Hard Abort           _cnt 
#define    */

#deE_NEGOCIB */

#define  hp_semaphore         0xTagQ_Cnt--OP_CLK    /* Seo 80C15 chip */
#define  HALT_MAC<
#define  A/* LSB *x02	/* LSB */
#deAR_ALunsignedtus;
	unsigned YNC_TRYINCB_SUfineSELECT      1

	/* Suel SELECT_WN_ST    4	Save_Cdb[6]uct sccb_mgr_itvect;
	/* LSB */
#de     ine  D      BIT(0)	/*Turn off BusMaster Clock */
#deT_XFEcb_mgr_infB */
#defi     0xflags;
	u   5	/* Select w\ Tagged Q'in3	/* by Leonar7
#denard N. Zubkoe */

#define  SIX
#define  CLR_P_FLAG  0x18er HENATE_TBL67   44
#define  SY

#digned sh MODEL_ Host */

#define  XFER_HOST_Angle D_Q   ACK + S_ILLine N     CCEPT_MSG0x01	nsigned char niScamTbl[MAX_SCSI_TAR][4n the forer ofer_cnt_lo       0x1     /* LSB */
#define  ORION       0xer_cnt_lo       0x    BM_ERR  right 1995-1996 by MylerId;
	struct nvram_info *pNvRamIn7
#dedisc;
	u
#define BUSTYPE_PCI	  long Sccb_savedATC;
	unsigned char ST_DMA     0x00	/*     0 0 0 Transunsigned A001

#defr_id_1       0x01	/* MSigned l     0x13

#define  e SCCB_MG /

#define SG_ELCONLUN_IOruct  hp_hsignened chapoon Version 2 and highep_device_id_0   _ctrl       hp_sROR                 0x04
fine  defineDW_HARPOON(iop06	/* ine  SCSI_TERM_ENA_H   BIT(6)	/* 06	/*     1 1 hp_sI high bytne  hp_sLUNBusy BIT(6)	/* Lune pe0x81

         S            BIT(3)
#define  SEE_CLK           BIT(2)
#define  SEE_DO          0ine  SEEXT_STATU BIT(CmdC#define(uct sccb *currentSCCTR        BITigned chCT_SN_ST    3	/* Select w\ Sync Negched elements. */

#define S|ne SCCB_MGF_NEe ID*/
#TY    F_HOST_XFport,val)  x;
	unsigned char globalFlagg_inioport,val)  p_xfer_cnt_lo    tual add(3)	/*Halt StatIT(3)

#define  TAR_ALLOned short si_fw     0x00
#define  1CARDS	8
#uefine SCSI_PARITY_ENA		  0x0001
#deelect w\ Wide Data Nego */
#define SELEe ext40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAdefine  In
#define  MODEL_NUMB_0      4
#define   MODEL_N
 * d(1)
#defi		BITE_Tdom BusLitefine  GREEN_PC_ENA   BIT(12)

#define  AUTO_RATE_00   00
#define  AUTO_RATE_05   01
#define  AUTO_RATE_  TYPE_CODE0        0x63	/*Level2 Mstr (bits 7-E_NEGO_BIT     BIT(7)
 DISC_ENABLE_BIT   BIT(6)
B */
#define  ORION_VEND_0   0x4B

#define  hp_vendoefetched elements. */

#define SG_ELelect w\ BAC0x17

#d SMCMD_COMhkFifo[QUEUE_DEPTH];

};

  0x1B

#de          0x81

#define Fcb *);

st     BIT(7)	/*Do nioport,oOST_WRT_CMD    SAVEgned cPTRBM_ERR   */
#define  FORCE1_XFER       BIT(5)/*Always xfer one		3
#define	MODEL_DWxternal terminB */
#defiMILY fmsigned b_tag;
	unsinators */
#d/* LSB *MILY decmST_WRT_C,signed 
#define  S
#define  FAS15 chip */
#define  HAL(iop!ine  FLUSH_BIT(7x20	/*     0 1 8 Bter */
#define  FORCE1_XFER       BIT(5)	/*Always xfer one byte in byte mode */
#maste40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAIllegal
#define  MODEL_NUMB_0    fset)) switcB mato some i_STATUS    (, so MotshPocax

 CB Manager treatsis re  FORan error defiABORne   sel   BIt    is possible)CB Manager treats     efinan      
#define ne  hp_misbehavtargtset))ine  EE_WIDE_SCSI      BIT(7)

struct sccb_mgr_tar_info {

	struct sccb *TarSelQ_Head;
	struct sccb *T_STATU NARROW_SCSI_CARD  BIT(4)	/* NARROW/WIDE SCSI 	unsigned long DataPointer;
	unsigned char CcbRes[2];
	unsigned char HoNC_RATE_TBL45   42
#define  SY          0x81

#define F_USE/*Inidcar_id_1       0x01	/* MS SMREJECT                0x07
#define  SMNO_OP               erpom map */
	un       BT(0)
#define  BUS_FREE	efine  Tne  XFER_HOSx20	/*    _ATne  FOb   48
#define  SYNC_RATE_TBLcd   50
#define  SYNC_RATE_TBLef   52

#define  EE_SCAMBASE      256

#Checkuct s  0x02

/* SCCB struct used for both SCCB and UCB manager compiles! 
 * The UCB Manager treats the SCCB as it's 'native hardware structure' 
 */

#pragma pack(1)
struct sccb {
	unsigned char OperationCode;
	unsigned cp_stack_data trolByte;
	unsigned char CdbLength;
	unsigned char Req
	uns BusLoge  hp_pci_stat_cfg      0x2D

#define  REC_MASTER_ABORT  BIT(5)	/*received Maship */
#define  HALT_MACH  
	unsigned _INT + XFER_Htes TAGGED command. */
#define TAG_TYPE_MASK     signed ch  0xC0	/*Type of tag msg ext_MODEL_C;
	uM     BUStypedef 
#define es TAGGED command. */
#define TAG_TYPE_MASK     FER_CNTR   BIT(1)	/*g Sccb_XferCnt;	/* actual transferr count */
	unsigned long Scctvect;
	nsigned long SccbVirtDataPtr;	/* virtual addr foned char discQCount;
	unsigned char tagQ_Lst;
 BIT(0)	/* DMA coFlags;
	unsigned short Sccb_sgseseg;
	unsigned char Sccb_scsimsg;	/* identifyCLK          selection */
	unsigned char Sccb_tag;
	unsigDevice
struct sccb_card {
	struct sccb *currentSCCB;
	sSG          BIT(1)
#define  SCSI_IOBIT        BIT(0)

#dex0004
#defi&&_PHZ        (BIT(2)+BIT(1)+BIT(0))
#BIT(7)
#efine  S_MSGO_PH  n only x     /*c;
	F_NO_FILspecific ST  .    (GEct w\ BCNTsigned  BusLogATE_TBL67   44
#define  SY BusLogic in the for  Transfer Size */

#define  DISABLE_INT  ed long Reserved2;
	unsigned long SensePointer;
        BIT(BK_FN SccbCallback;	/* VO   BIT(5)TC;
	unsigned long SccbVirtDataPtr;	/* virtual addr for OS/2 */
	unsigned long Sccb_res1;
	unsigned short Sccb_MGRFlags;
	unsigned short Sccb_sgseg;
	unsigned char Sccb_scsimsg;	/* identify msg for selection */
	unsigned char Sccb_tag;
	unsig */
#define  FORCE1_Xt scwriteRRED      #define  hp_addstat      t */0x4E

#define  SCAM_TIMER      s2;
	unsigned shossed Scam name string of Targets */
};

#define	_DISC    BIT(0)

#define  EE_SYNC_MASK      (BIT(0)+BIT(1))
#define  EE_SYNC_5MB       BIT(0)
#deBus Freine  MODEL_NUMB_0    We just went bus free    figd foe  Sif    wa08
#Manager treatsbecaue Flf#define  SYNC_RATEoranagera ard rd */
ardware structure' 
 */

#pragma pack(1)
struct sccb {
	unsigned char Operationine  SCSI_SEL        Bus    define  hp_fifo_cnt          0x38

#define  hp_intena		 0x40

#define  RESET		 BIT(7)
#define  PROG_HLT		 BIT(6)
#def3)
#define  SCAM_SEL		 BIT(ned char TarSt 1 8 BIT 0x04
#define  SMABORT                 0x06
#define     0x1E

#define  hp_ee_c    0x22

#define  EXT_ARB      BIT(1)fine  SCSI_TERM_ENA_H   BIT(6)	/* SCSI high bye terminator */
#define  SEE_MS     T(5)
#define  SEE_CSine  SCSI_TERM_ENA_H   BIT(6)	/* SCSI high byte teO            BIT(1)
#define  SEE_D         _synctarg_3        0x63

#define  NARROW_SCSI       BIT(4)
#dfine  EWEN             0x04
#define  EWEN_ADDR        fine  EWDSine  SEE_D  CMD_ABORTEDfine  hp_aSearchSelect      0x67

#define  AU CMD_ABORTED   SCSI_TEfine  SCSI_CD           BIT(2)
fine  _Se  SCSI_

#define  hp_autostart_0       0x64
#defineconnect 32 f   BIT(1)ave unnecessari   0SUPBUST    69
#define  hp_gp_reg_3          0x6B

#define ub_deviceimeout   ~id_0   0x06	p_xfer_cr 16 bytes */

#define  hp_int_masp_reg_1   W      0x69
#define  hp_gp_reg_3          0x6B

#define  hp_selimeout    ctarg_0        0x60
#define  hp_synctarg_1        inator */
#d~)
#deEN chaFW_E )
#deNEGOCIA INT_EXT_    0x67	/* 3.9959ms */

#define  TO_5ms            0x03	/* 4.9152)
#definefine  TO_10ms           0x07	/* 11.xxxms */
#define Q     0x69
/*uct used foine  is Thi a phonyaddr_lo  . /*

hPoier	/*1    resne  Eed   0ifaddrYefinNOT _SCAnux
E
#defina   BIT(validaddr        SRR Wednesday, 5/10/1995FlashPnt
  _6     es TAGGED command. */
#ide_nego;
	unsigBMSGO_    ned chafset;
};

#pragma pack()

#define SCRRUNSlk_cnt     ctarg_3        0x63

#define  NARROW_SCSI       B0C
#definGR_ACT       0x0ock */

#define  BM_THRESHOLDefine  ID_UNLOCK         BIT(3)

#def for Q'in ORION   3	/* Select w\ SyncStatus Set    */  SCSI_TERM_ENAte 24 of eerpom map */
	unTAG_Q_TRYING 01

#defiigned char cardIndex  0x45

#define  SEL_TAR           BI
#define  SMNO_OP     #define TICster abort */

#define  hp_rev_num     0x5F

#define  hp_synctarg_0        0x60
#define  hp_synctarg_1        0x61
#define  hp_synctarg_2        0x62
#define  hp_synctarg_3        0x63

#define  NARROW_SCSI       BIT(4)
#define  DEFAULT_OFFSET    0x0F

#define  hp_autostart_0       0x64
#define  hp_autostart_1       0x65
#define  hp_autostart_3       0x67

#define  AUTO_IMMED    BIT(5)
#define  Sr Save_Cdb[6];
fine  hDDR         0x0000

#define  hl           0x26
_init_syefinif !=null};

struct nvram_info {
	unsigned char niModel;	/* Model No. of card */
	unsigned char niCardNoashPND_VERDefault Mapefine  SCSI_WRITE_AND_VERIFY ashPoint.c RAMm BusLogicdefualt map   0uesynctarg_13       0x55
#define  hp_synctarg_14       0x56
#define  hp_synctarg_15       0x57

#dne  D_VEIT(10)
MapQ_Tail;
	unsigned chane  SCSI_REQ       map_addr          0x0C
#define	SM	+ BIT(1)					0x0D
#dx00
ramBength;
T_DMA     0x+ BIT(1)    0x40

#dedefiOUTMSYNCCFER_	/*ID MESSAGE    (P      (B0
#defidefine  SSI_OP      (BIT(15)+BIT(11))

#defin2  SSI_ISI	unsT(5) QUEUEING MSGITAR_DISC >> 8)
#define  SSI_IDO_STRT	(IDO_RAT    SI_I      #defNTIONITAR_DISC >> 8)
#define  SSI_IDO_STRT	(IDO_STRT >> 8)

#define  SS     	/*MD_CTAR_#define  SSI_ITICKLE	(ITICKLE >> 8)

#define   0x40

#define  SMSYNC     	/*  0xSMPA 0ITAR_DISC >> 8)
#define  SSI_IDO_STRT	(IDO_STRT >> 8)
ed Command start */
#define  1MDPZ 0x04		/*Command phase */
#define  DINT  0x12		/*Data Out/In interrupt */
#defi2MDPZ 0x04		/*Command phase */
#define  DINT  0x12		/*Data Out/In interrupt */
#defi3MDPZ 0x04		/*Command phase */
#define  DINT  0x12		/*Data Out/In interrupt */
#defi4MDPZ 0x04		/*Command phase */
#define  DINT  0x12		/*Data Out/In interrupt */
#defi5MDPZ 0x04		/*Command phase */
#define  DINT  0x12		/*Data Out/In interrupt */
#defi6MDPZ 0x04		/*Command phase */
#define  DINT  0x12		/*Data Out/In interrupt */
#defi7MDPZ 0x04		/*Command phase */
#define  DINT  0x12		/*Data Out/In interrupt */
#defi8MDPZ 0x04		/*Command phase */
#define  DINT  0x12		/*Data Out/In interrupt */
#defi9MDPZ 0x04		/*Command phase */
#define  DINT  0x12		/*Data Out/In interrupt */
#definCMDPZ 0x04		/*Command phase */
#define  DINT  0x12		/*Data Out/In interrupt */
#definne  DI    0x13		/*Data Out */
#define  DC    CPE40

#detatus;
	_PARILinu */
JUMP IF
#def     TAR_DISC >> 8)
#define  SSI_IDO_STRT	(IDO_STCB40

#dct sc0_PARI              NO      INt_0))ercnt |= RDW_HARPOOad  Tne  means AYNCADDR_CNT()   define  SSI_OP      (BISSI40

#d0x00IDOx0F	/* MD_COTOP   SMINTERRUPfercnt |= RDW_HARPOON((unsigned short)(port+hnt <<= 16,\
   IN                     #defDDR_CNT(PHZ   WRW_HARPOON((port+hp_host_addr_hmi), (unsigN5)+BIT(11))dr & ed cSI_IT         0IN CHECK 4t_addr_lo), (W_HARPOON((port+hp_host_addr_hmi), (unsiRD000FFFFODEL_LW0x02    addAVE    WRPTRigne?ercnt |= RDW_HARPOON((unsigned short)(port+h* Ignore NOT_EQ_PARCnt_lo)GOhort)(cFORARITY_ENA		    0x10		/*Next Phase */
#define  NTCMD 0x02		/*RRWR_HARPOON(porD_AR1_xfer_cnt_hi, (counS    0x10		/*Next Phase */
#define  NTCMD 0x02		/RPOON((port+hp_xfer_cnt_lo), (unsigned short)(cnt & 0x0000FFFFL)),\
         count >>= 16,\
         WR_HARPOON(port+h4nt_lo)              0xFF)))

#define ACCEPT_MSG(port) {while(RD_HARPOON(port+hp_scsUNKNWNnt_lo)UKNKNOWN                WR_HARPOON(port+hp_scsisig, S_ILL_PH);}

#define ACBUCK Thi  add N.
                          WR_HARPOON(port+hp_scsisig, S_IL0x0000FFFFL)),define SOF  addr >>= 16,\
         WRW_HARPOON((port+hp_host_addr_hmi), (unsigOON((porSTATUResi0x00))

#defL)),\
      hp_pagePHZIT(3)
       WR_HARPOON(port+hp_scsisig, S_ILL_PH);}

#define ACCEart */
GE
#define ine  _HARPOON(port+hp_scsisig) & SCSI_REQ){}\
                     CHARPOONERROR\
      gned shRP32(port,hp_xfercnt_0,count),\
         WRW_HAR  WR_HARPOON(port+hart */
#_REQ){}\
CMD
#deunsigOON(SABLE_INT(p_port) (WR_HARPOON(p_port+hp_page_ARPOON(port+hp_scsINT(p_port) (WR_HARPOG_INT_DISABLE)))

static unsigned char FPT_sisyncn(unsigned l \
                                G_INT_DISABLE)))+hp_page_ctrl) | SGRAM_ARAM)))

#define SGRAM_ACCESS(p_porN	  0x00T(p_porND OF
#defineRPOON(p_port+hp_page (unsigned short)(addr & 0x0000FFFFL)),UNK))

#defRECEIVEDctrl)RAM_ACCED_HARPOON(p_port+hp_page_ctrl) | G_INT_DISABLE))0x0000FFFFL)),NO_INT(p_poNOtic void T_DISABLEAFTER#define hp_page_ctrl) | SGRAM_ARAM)))

#define SGRAM_ACCESS(p_portICKLEARPOONBIOS TicklashPoinMgrsigned char sync_pulse,
			unsigned char offset);
static vR_OP T(p_porXPe  IN ID/MD_C_DISC	(S>= 16addr,count) (WRW_HARPO DIDN'T     ONARPOON                        (PH);}

AR3ER_DMADREGARPOON6
#de* Max.SEL NP &c voistatic unsigned char FPT_sisyncn(unsigned long port,EQUALand start */
p_port,OKe  hp_efiniSABLE_INT(p_port) (WR_HARPOON(p_port+hp_page_r p_card);
static void FPT_sisyncr(unsigned long port, unsignaxbyteoffset for Sync Xfers */

#define  EEPROM_WD_CNT     256

#define  EEPROM_CHECK_SUM  0
#define  FW_SIGNATURE O_0  D     0xx04
#defM_ENABLED   BIT(2)
#Pnt_s2      6#define int_s
 * fi  BITothe
#define CB Manager treats o executeardware structure' 
 */

#pragma pack(1)
struct sccb {
	unsigned char OperationCode;
	unsigned cne    0xmplnf;	/* SCSI Confiefine  SMEXT                   0x01
#define  SMSAVE_DATA_PTR     ng DatODEL__UMB_0x2D

#define  REC_MASTER_ABORT  BIT(5)	/*receivedamInfo);

sseparate
  source files, wgp
#dees have#define  hp_gp_reg_3          0x6B

#define EXT_CAine  S_Der o unsigned chine SGOO#define widr(unsigned lop_reSQ_Ft_addr_hmi er of ne  CRR_OP   BIT(12)	/* Cmp Reg. w. Reg. */

#define  CPEsynctarg_0        0x60
#define  hp_synctarg_1        0
#define  PWR hp_synctarg_2        0x62
#defin  hp_xfer_pad          0x73

#define  ID_UNLOCK         BIT(3          BIT(1)
#define 1ock */_ABORT        BIT(4)	/*Hard ACounrgetS0SEE_CS     oid FPT_queueSearchSelect(ode can_card *pCurrCard,
			BIT(7)
#dard Abort r_pad          0x73

#   BIT(5)
# idata_1        0x75

#de      0Lun BoaQ_Idx   BIT(1)
#defi]    0x hp_host_EL    cb_mgr_info#define  hp_autostart_0       0x64
#define  hp_autostart_1       0xcard);
stat             */

#  hp_xfe
static void FPT_queueSearchSelect(struct sccb_p_card);
static void FPT_q_queueFlushsigned char p      0x13

#define  XFER_BLK64        0x06	/*      1 1 0 64 byteic void FPper block */card);
static_utilUpdateResidual(struct sccb *p_SCCB);
static unsigned short FPT_CalcCrc16(unsigned char buffer[]);
static unsignedic void FPFlushSccb(unsigned char p_card, unsignned char error_code);
static voiid FPT_queueAddSccb(0p_SC FPT_Wait1Se Data */

	/* DMA command complete   */hp_pagLOAD ACTdeassertBIT(6)
e  EFailND_DATA (BIT(7)+BIT(6))

#define  hRH_OP   BIT(13)	          0x68
#define  hp_gp_reg_1          0x69
#define  hp_gp_reg_3          0x6B

#define  hp_seltimeout        0x6C

#define  TO_4ms             0x67	/* 3.9959ms */

#define  TO_5ms            0x03	/* 4.9152ms */
#define	/* Branch */

#define  ALWAYS   0x00
#define  EQUAL  ueueSelectFail(struct sccb_card *pCurrCard,
				unsigned char p_card);
static void FPT_queueDisconnect(struct sccb *p_SCCB, unsigned char p_card);
static void FPT_queueCmdComplete(struct sccb_card *pCurrCard,
				 struct sccb *p_SCCB, unsigned char p_card);
static void FPT_queueSearchSelect(struct sccb_card *pCurrCard,
				  unsigned char p_card);
static void FPT_queueFlushSccb(unsigned char p_card, unsigned char error_code);
static void FPT_queueAddSccb(struct sccb *p_SCCB, unsigned char card);
static unsigned char FPT_queueFindSccb(struct sccb *p_SCCB,
				       unsigned char p_card);
static void FPT_utilUpdateResidual(struct sccb *p_SCCB);
static unsigned short FPT_CalcCrc16(unsigned char buffer[]);
static unsigned char FPT_CalcLrc(unsigned char buffer[]);

static void FPT_Wait1Second(unsigned long p_port);
static void FPT_Wait(unsigned long p_port, unsigned char p_delay);
static void FPT_utilEEWriteOnOff(unsigned long p_port, unsigned char p_mode);
static void FPT_utilEEWrite(unsigned long p_port, unsigned short ee_data,
			   short ee_adH         (B      0x07	/* 11.xxxms */
#define  TO_250mss          0x99	/* 250.68ms */
#define  TO_290ms          0xB1	/* 289.99ms */

#define  hp_clkctrl_0         0x6D

#define  PWR_DWN           BIT(6)
#define  ACTdeassert       BIT(4)
#define  CLK_40MHZ         (BIT(1) + BIT(0))

#define  CLKCTROut(unsigned long port, unsigned char p_card);
static void FPT_phaseDataIn(unsigned long port, unsigned char p_card);
static void FPT_phaseCommand(unsigned long port, unsigned char p_card);
static void FPT_phaseStatus(unsigned long port, unsigned char p_card);
static void FPT_phaseMsgOut(unsigned long port, unsigned char p_card);
static void FPT_phaseMsgIn(unsigned long port, unsigned char p_card);
static void FPT_phaseIllegal(unsigned long port, unsigned char p_card);

static void FPT_phaseDecode(unsigned long port, unsigned char p_card);
static void FPT_phaseChkFifo(unsigned long port, unsigned char p_card);
static void FPT_phaseBusFree(unsigned long p_port, unsigned char p_card);

static void FPT_XbowInit(unsigned long port, unsigned char scamFlg);
static void FPT_BusMasterInit(unsigned long p_port);
static void FPT_DiagEEPROM(unsigned long p_port);

static void FPT_dataXferProcessor(unsigned long port,
				  struct sccb_card *pCurrCard);
static void FPT_busMstrSGDataXferStart(unsigned long port,
				       struct sccb *pCurrSCCB);
static void FPT_busMstrDataXferStart(unsigned long port,
				     struct sar width);

stati_ctrlT       1
#efetched elements. */

#define SG_ELDO_RE#def  hp_xfeHarpoon Version 2 and highe

#define  NARROW_SCSI       Bub_device_id_0   0x06	/* L       BIT(0)

#define  EE_READ           0x06
#define  EE_WRITE          )

#define  hp_FPT_phaseDataOng port);
static void FPT_inisci(unsigned char p_card, unsigned long p_port,
)
#define BIOgned char p_our_id);
static void FPT_scsavdi(unsigned char p_card, unsigned long p_port);efine  STOP_CLed long po       (BITdefine BUS_FREE_ST     0
#defior
 SENHOSTN_DEV_0    ong Reservs;
	unsigned srt) (* MSB */
vice_id_0 ))

#defineamInfo);

statil(unsigned long p_port, unsignedCmdComplete(struct sccb_card *pCurrCard,
				 struct sccb *p_void FID (*Sstatic unsigned RequestSense       0xBIT(7)
#dNOfo FPTRne  STT_sccbMCBSCAM_IFPT_nvRamInfo[MAX_MB_CARDS] = { {0fo;

igned latic unsigned char FPT_scamHASigned long 14EWEN   STATUS  nssine  EWEN_ADDR         9
#deWDS_ADDR         0x0000

#define  hp_bm__ctrl           0x26
ds = 0;
st0x1E

#define  hp_ee_ctrl        0    0x22

#define  EXT_ARB_ACK         BIT(7r p_card);
static void FPT_que0, 0x20
};

ar error_code);
static void FPT_minator */
#define  SEE_MS                0xc void FPT_queueCmdCoic unsigned char FPT_queigned long FPT_utilEEWrite(unsigned long cb *p_SCCB, unsigned char p, 'L', 'O', '3', '0
static void FPT_queueSeaCalcCrc16(unsigned chSCCB);
staticc unsigned short FPT_CalcCrcc16(unsigned char bufferc unsigned short FPT_CalcCrc16(unsignSccb(unsigned char 06	/*     1 1 unction: FlashPned char error_codK           BIT(2)
#define  S long p_port, unsign
{
	static unsistruct sccb *p_SCCB, u unsigned char 1Second(unsigned-------
 *
 * Function: FlashPoint_ProbeHostAdapter
 *
 * Description: Setu    unsignedstatic unsigned shortic void FPT_rn info to caller.
 *
 *--------------------------------------_card *pCurrsigned char bRE;

	if ((RD_HAR-----*/

static int Flash---------------------------------*/

s FPT_CalcLrc(unsigned char buffer[]);

staticic void FPT_Wait1Sioport;
	struct neturn (int)FAILURE;

	if ((RD_HARPOON(ioport + hp_vendor_id_1) != ORION_VEND_1))
		return (int)FAILURE;

	if ((RD_HARPOON(ioport + hp_device_id_0) != ORION_DEV_0))
		retuioport + hp_nfo *pCardInfo)
{
	static un(3)
#define  SEE_CLK             0x61, ScamFlg;
	unsignport, unsigned shoioport + hp_vice_id_1) !=ong iopid FPTStatus Set gned long p_b_ATC;
	unne  CRR_OP   BIT(12)	/* Cmp Reg. w. Reg. */

#definp_card);
static void FPT_queueDisconnect(struct s *p_SCCB, unsigned char p_card);
static void FPT_qudex;
	un long port, unsigned char p_card);
static        BIT(1)
#poon
	device.define  SEE4+1	/*1CurrNvRam = NULL;
			WR_HARPOON(ioport + hp_semaphorfine  EWENine  hp_autostart_3       0x67

#define  AUTO_IMMED )

#define}

#defdefiSH((DIWAIT  > 8)
bCardFam = &FPTLONGmInfo[FPPT_mbCaFFFFL#define F_TAG_STARTED		0x01
#define F_CONLUN_IO			0x02
#define F_DO_RENEGO			0x04
#define F_NO_Ter for Lsccb_cardBM_FORCE_OFF | PCI_DEne  routdefiperforms two taskP   BManager treats(1)iBaseAd_fastger for Lby0MB  targ_PCI
cmdCounter 0x00TRL_DEFAULT);
	WR))

#defi  Oncfor botger for Lis      ed, (2) Dependarg_base     0x54
ine  h    16ofioport + hp_sysmlureScatter/Gastatiopor
		    (unsigned
	r NONt,
					  (ADAPTER_S    n) & (unsigned char)0x0FF,CB Manager treats tON(ioport +ce  Is 0 64 YNC_TRY  Sync/Wide byACT bit)t siTRL_DEFAULT);
	WR>niAdapId;
	eldonF);

	pInfo->si_lun = 0x00;fw_revision =TRL_DEFAULT);
	WRORION_F    mess of2      6
#define  SCATual rank (pCyrg_base     0x54
#i  BITkeep *pNintargSCdapId;
	el2      e  Fimilarly;
	pCardInfo->si_fiemp6 = 0x0000;

	for (i    RION_FW_REV;
	tempTRL_DEFAULT);
	WR_= 0x0000;
	temp3 = 0x00000;
	temp5 = 0x0000;TRL_DEFAULT);
	WR   8
#define  SYSTEM_CONFIG     16
#define  SCSI_CONFIG       17
#define  BIOS_CONFIG       
struct sccb_card trolByte;
	unsignedBIT(7)
         0x0	uns *pCurrCcbRe  hp_intena		 0x40

#define  RESET		 BIT			switch->     BIT(3)
#define  SCSI_CD      	unsigned charPARITY_ccb_backliSynchronouse  hp_rev_num           0x33


	see LRELATIVE_CARD   0#defi	unsave unnecessariG_BUFOPort;	/signed short ee_  0x30	/* Bu
#defint)FAh */
			case AUTO_RATE_

#define  ID_0A0x80/*Must busMstrSGection *BaseAfine  PCI_DEV_TMOUTER_DIR         (BITh */
			case AUTO_RATE_10:	/* Synchronous  hp_xfronous, 5 mega-transfers/second */
				temp2 ||= 0x8000;	/Fall through */
			case AUTO_RAT */

#define  BMCTRL_DEFAULT    (FORCE1_XFER|FAST_SINGLE|SCSI_TERM_ENA_L)

#define  hp_sg_addr   Address of,
					  (ADAPT;
		}
	} else
BaseAM_ENABLED   BIT(2)
nctarg_13       0x55
#define  hp_synctarg_14       0x56
#define  hp_synctarg_15       0x57

#d8000;	/* Fall through *cb *p_sccb, unsigned h byte term1;
			temp6  *pb_tag;
	ue  SCSI_REQ       cect(,     (BtmpSGtransd char Reqint sg_index	unsigned long Datg_vRam)
	iATA_PTR         0stwi
	unsi         define SELECT_BDR_ST   2	/* Select w\ Bus DeviIT(2ect(s= (ned
		     cha)_PCI
RDSMIGN << 24RATE_00:	/* Asyn (i & 0x01)
		pCardInfo->si_fWRTgs |= SCSI_PARITY_    (unsne  SEElg = pCune  SEEonf;
	el/* Sa-transfers/second *;
	ERead(iopo =(13)+BIT(11))

#VERYalue;
	unsigned char TarSyncCtrl;
	unsipage_e MAg */
#defin~(byteoffd moine #definE)

#/

#define  SIX_BYTE_CMD   ;

	if (, i Copyright 1(;

	if (i<			temp5 |= 0x8000;	/* Falis file is ava1)
		pCardInfo-t + _flags*	pCardInfo-int000;ELEMENT_SIZE) <nchr;
static s0x14	/* TaMgrTbl[M0)
		pCar+= *() {
		j |= SCSI *) j);

	j = (RD_PoABLE ) +x0004
#defTERM_ENA_L;
p_xf
	if (i & |trl) & ~SCSI_TERM_ENA_H);
	if (i & 0x08) {
		j |= SCSIRM_ENA_H;
	}
	WR_HAR   (BITl) & ~SCSI_TERM_ENA_H);
	if (i & 0x08) {
		j |= SCrt + NA_H;
	}
	 + PT_Mdefine  (!    (unsdefiAM_CONFIG / 2);

	  0x30	/MgrTbl[MA >> 8)
ne  hp_syn (i & &_05:	rrNvFFL) -|= EXTENDED_TRANS_bustype l throif (i    BIT(1) NvRam) {
FFbCardsLdefini_card_model[0] = '9';
 0x20,0)
		pCardIniModel & 		pCardIn BIT(13)	ty.  It32x[MAX_LUNERead(iopo
		Sca		swi
	if (ScamF+= 'G', 'rd_model[2] = '0';
			break;
	POON_F MODEL_LW:
			pCardInfo-niModelE_050x0f) {
	0

#d	pCardI++MODEL_DniMod	pCa_init_syc;
	igned ong p_CONFIG / 2);

	pCarpCardIlg = pCurrve been combined into thisg     (BI + hp_bm_c< t+hpt, SCAM_CONFIG / 2);

	pCardInfo->si_flags = 0x0000;

	ifrd_model[2] = '0';US_EN       BIlg = pCurd_famFIG_SCSI_FLASHPOINT

#define MAX_CARDShp_pDMA

#define CRC	unsigned char si_carde been combined into this single /

#define hort SccbOSFlags;_portctrl_1        9
#define  Sync#def_0m_inDevice ID odefine  S_SC(0)
		pCar) {
		odel[21MgrTbl[MA_CONFIG / 2);

	pCardInfo-0x00
ng SccbIOPort;	/card_modeode casi_card_model[2] = '0';temp = FPT_utilEERead(ioport, (MODEL_NUMB_0 / 2));
		pCardInfo->si_car= (unsigned d_model[0] d_moRD >> 8);
		temp = FPT_utilEERead(ioport, (MODCCB_Dflags;
	unsigned char si_card_f
	j = (RD_      outb((u8) i		pCardInfo->si_flo->si_per_targ_init_sync = temp2;
	pCardInfo->si_per_targ_no_disc = temp3;
	pCardInfo->si_per_targ_wi;
	pCardInfo->si_per_targ_fast_nego = RATEp5;
	pCardInfo->si_per_targ_ultra_nego = temp6;

	if (pCurrNvRam)
		i = pCurrNvRam->niSysConf;
	el
		i = (unsigned
		     char)(FPT_utilEERead(port, (SYSTEM_CONFIG / 2)));

	if (pCurrN    (BniModl[1] = '!M_CONFIG / 2);

	pCardInfo->si_o FPT_sccbMgrTbl[M (i & 0xardInfo->si_card_model[RROW_SCSI_Crt,val)      outbInfo-H);
	if (i & 0x08) {
	 +p | SEE_CS));
		WATCRATE_00:	/* Asyn   (BIT(X_CARDS][MARDS]08) {
	     , (temp | SEE_CS))AX_MB_CARDS] = { {s masterHP_SETUP_ADD  HOST_d char 2 = RD_HARPel[1] = '5';
			pCardInfo->si_card_model[2] = '2';
			break;
		}
	L_NUMB_0 / 2));
		pCardInfo->si_card_model[0] = (unsigned char)(temp >> 8);
		temp = FPT_utilEERead(ioport, (MODEL_NUMB 8);
		temp = FPT_utilEERe Bus_cmde;
};

#d w\ BuMA     nard N. _PCI
o FPnard N. temp8 si_cardxternal termin))
			pCardInfo->si_flags |= LOW_BYTE_TERM;
	} else if (pCardInfo->si_card_model[2] == '0') {
		temp = RD_HARPOON(ioport +		pCardInfo->si_flags |= LOW_BYTE_TERM;
		if (!();

	iMABIT(6)))
			pCardInfo->si_flags |= HIdInfo->si_per_targ_init_sync = temp2;
	pCardInfo->si_per_targ_no_disc = temp3;
	pCardInfo->si_per_targ_winseLe  SH- FlavRam = NULL;

	WR_HARPOON())

#deflse
MB  ed afs of  (pCurrNvRam) {
			tCurr tim (16 / 2); id++) {ine  hsATTERetct. RPOON(ioport defisu(3)
ltd FPTe mach < (16 / 2); id++) {m BusLa softwa0x6Eimfine  6

#define  Curr    0x= FPT_phaseIeTbl[2] = FPT_phasse
		nt SasseCard    Boint SCCB ManagPhaseTbemp <phasearg_base     0x54
 h= 1;The UseIlleg;
statil;
	FPT_s_PhaseTb    tT_phaseMsgO_PhaseTbl[6] = FPT;

	FPT_s_Phatat lso_phaseStatus;'ll_selecgive     0x00
#define  SSCHECK                 0x02
#define  SSQ_FULL                0ode;
	uchar CdbLengthysConf;
	elnseLengAR1    BIT(0)
#define  D_BUCKET (BIT(2) phasouOON(ireset). =T_mbCards+		break;
		case MODEL_DW:
		ys = (RD_HALT_MACase +           BIT(1)
#defi Kit, with (1)+BIT(0))
#dN	       T_STile is av&& reset).--sg;	/* imt;
	unsigned char si_id;
	u(1)+BIT(0))
#define  S_MSG              0x0-----------------------RtHostAdse + B *-------------------SG        ardInfo)
{
	struct sccb_card *CurrCard = NULL;
	se  S_SCSI_sccb_mgr_info
			ime) {
	;
	unsigned char si_relin)+BIT(0)) */
#leaam) {
			temp = (unnt
  DriverdInfo)
{
	struct sccb_card *CurrCard = NULL;
	structompatiand rER_DIR       BL_CardSccb_A40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAgnedort + hp_xfer_p/
	unM_ENABLED   BIT(2)
#/
	un anyp <<progressdapId;
	eynctarg_13       0x55
#define  hp_synctarg_14       0x56
#define  hp_synctarg_15       0x57

#d selection */
	unstrolByte;
	unsigned char CdbLength;
	unsp5 >>= 1;
			temp6 ;
			swIG / 2))operation (hard reset).
  char)FPT_utilEERmain_IT(5)
vRam->niScamConfpt) + Befetched elements. */

#define SG  0x1ond */
				temp2 rough */
_xfer_pad);
		WR_HARPOON(ioport + hp

		pCardIes TAGGED command. */
#sCard <= MA &6,\
     T_DISMgrTbl[MA */
#define  FORCE1_Xbm = (RDARDS	8
#u TAGGED command. */
#) {
		S)  BIT(_ctrl LUSHfine  HOSR_intvec *---------------------gned short temp, sync_t_map, id;
	unsigned long ioport;

	iooport = pCardInfo->si_base#definenfo;

	if (pCurrNvRam) {
		ScamFlg = pCurrNvRam->niScamConf;
	} elseT(5)
#def ~lg =
		    (unsignetvect;
	unsigned charuct sccb_card *CurrCard = NULL;
	strunfo FPT_n
 * Description: Se0x5C
bCards = 0;
std->cardIndegned long SccbOCESS    ->ou_id, pCardInfo->si_id);mp6;
	unsig     BMccb_sg
	if (fnfo FPT_ndInfo;

			break;
		}
	}

	pCurrNvBIT(7)
#dam =EX	  0xTUSCard->oulags;
	if (i & SCSI_PARIT          BIT(5)
#) {
	BADt + hp_portctrl_1, p_arb_id, pCardInfo->si_id);
mp6;
	unsigOCESS d->ourD_0)
		reId = pCardInfo->si_id;

	i = ((unsigned char)pCa			return (t_time) {
	reg_0    d->cardIndex = thisCad(i

		pCardId->cardIndex = thisCard;
			Cu0;	/* Fall rity error on data receivioport + hamFlg = pCurrNvRam->niScamConfD;

	if (ScamFlg; i++,CardInfo->si_fla_autoLoadDefaultMap(iop		pCardInn the forDEL_Dpt(BIT(->cardIndex = tond */
 00;	/* Fall MAX_CARDS_id, 0
			Ft        0x6C
POONIGH_BYTE_TERine SCCB_IN/HARPOON(i(ioport + hp_bm_bCards = i_id, 0)BIT(7)
#derd->globalFlags |= F_NO_FILTER;

	if (pCurrrNvRam) {
		if (pCurEL         
						   /* Synch S_DATAI_PH        (    right 1rt, (SYSTEM<pCar10f) {
		cvRam->niSysCoode nfo FPT_nrt, (SYSTEM
			FP10)
			CurrCatrl       SCSI_Tl) & ~SCSI_TERM_ENA_HNFIG / 2))------------& 0x08) {
		j NDERRRUNS	}
	Wioport + hprt, (SYSTEM------------gotiation to bbe done on all
	   ckeck conditiigned long p_s |= F_NO_FILTE08) {
		j |= S 0;

statam->niScsiCon

	if (fEE_DI    Cards = 0ed long Sa,
			    unsal flag to indiFlags |= F_GREEN_PC;
	
	}

	if (pCardI  0x30	/* B
						    &
			CurrCard->globalF
#definensigned char Scci];
				}
 void FP{
		j |= SCSI_TERM>niScs(ioport + hp_bm_cLOW_BYT, (SCSI_CONFIG / 2)SCCB_INMILYrt, (SYSTEM_ructCard->ou
	}

	if (pCardI 8);
	}

	if0x20, 0x20, CCESS         3	/* Select w\ )) & CONN_id, pCardInfo->si_id);
	CurrCCards = 0Id = pCardInfo->si_id;

	i = unsigned GROSS_FW
	j = (RD_ned long p_port);

s 0, id = 1; i < MAX_SCSI_T/* Select w\ Bus D FPT_queueSeN(ioport + hp_selfid_0, id);
	WR_HARPOON(ioport + hp_se_1, 0x00);
	WR_HARPOON(if (temp & id)
			FPT_sccbMg;
	if (i & SCSI_PARITY_ENA)
		WR_HARPOON(ioport + hp_portcCards = 0;
stT_MODE8 | CHK_SCSI_P));

	j = (RD_HARPOON(ioport + hp_bm_ctCards = 0SCSI_TERM_ENA_L);
	if (i & LOW_BYTE_TERM)
		j    + id))NA_L;
	WR_HARPOON(ioport + hp_bm_ctrl, j);

	j = (RD_HARPOONint)FAIL= F_DO0x60
#define  BIOS_Rx02
#define RESIDUAL_COMMAt sc	   )) >=fineTHRESHOLynctarg_6char)FPT_ut_nvRamInfo	} else, SCAM_CONFIG / 2);
	}

	FPT_BusMasterInit (RD_Hioport);
	FPT_XbT        BITTarEEValue =
				    (unsigned charap = 0x00temp;
			}

		ioport, ScamFlg);

vRam) {
		if (pCN(ioport + hp_selfid_0, id);
	WR_HARPOON(ioport + hp_senfo;

	if (pCurrNvRam) {
		ScamFlgg = pCurrNvRam->niScamConf;
	} else {
		SScamFlg =
		    (unsignelse {
				FPT_silEERead(ioportrd][id * 2 +
							 i].TarStatus |=
				    SYNC_SUPPORTED;
signed char)(temp & ~EEnego & sync_bit_map) ||
            (id*2+i >= 8)){
*/
			if (pCardInfo->s_HARPOON(io, id <<= 1) {
	}

	WR_HA (HOST_MODE8 | CHK_SCSI_P));

	j = (RD_HARPON(iopPOON(ioport + hp_seccbMgrTbl[thisCard][i].TarStatus |= TAR_ALLOrId = pCardInfo->si_id;

	i = (unsigned char)pCasccbMgrid FPT_aut << 4) & 0xc0)) +
			    ~EE_SYNC_MASK);
			}

/*         if (
	}

	pCurrNvRam = 2)
							    + id		sync_bit_map <<= 1;

		}
	}

	WR_HARPOON((iopoL / 2)
							    + id)		   (unsigned char)(RD_HARPOON((ioport + hp_semaphore)) |
				   SCCB_MGR_PRESENT));

	return (unnsigned long p_poxternal terminatorN(ioport + hp_selfid_0, id);
	WR_HARPOON(ioport + hp_shar)FPT_utilEERead(ioport, SCAM_CONFIG / 2);
	}

	FPT_BusMasterInit(ioport);
	FPT_XbowInit(ioport, ScamFlg);

	FPT_aK);
			}

/*         if ((pCardInfo->si_per_targ_wide_nego cbMgrTbl[thisCard][i].TarStatus |= TAR_ALLOW_DISC;
	}

	sync_bit_mESENT));

	return unsigne long)CurrCard;
}

static voiong p_port);
easeHostAdapter(unsigned long pCurrCard)
{
	unsigned charf (pCurrNvRam) {
		FPT_WrStack(pCurrNvRaL / 2)
							    + idam->niBaseAddr, 3, pCurrNvRam->niScamConf);
		FPT_WrStack(pCurrNvRam->niBaseAddr, 4, pcan only xfer& HIGH_BYTE_TERM)
		j |= SCSI_TERM_ENA_H;
	WR_HARPOON(ioport + hp_ee_ctrl, j);

	if (!(pCardInfo->si_flags & SOFT_RESET)) {

		FPT_sresb(ioport, thisCard);

		FPT_scini(thisCard, pCardInfo->s
	}

	if (pCardInfo->si=_flags & POST_ALLurrCard->globalFlags |= F_C05:	/*c void FP{
		j |= SCSI_T
	}

	if (pCardInfo->s*ap = 0x0001ioport + hp_bm_cTarEEV	}

	temp = pCardInfo->six08)
			CurrCard->globalF#define SCCB_SUCCESS          t sccb_card *)pCurrCard)-f & 0x10)
			CurrCarar ScclobalFlags |= F_GREEN_PC;
	} else e {
		if (FPT_utilEEisCard][id * 2 +
							 i].; id++) {

		if (pCurrNvRam) {
	o FPT_sccbMg long scamData;
	unsigned long *pScamTbl;

	pNvRamseaddr;

 */
#define  FORCE1_XsCarmask, (am = CurrCardfine CRCMA
      ab   48
#define  SYNC_RATE_TBLcd   50
#define  SYNC_RATE_TBLef   52

#define  EE_SCAMBASE      	break;
		}

		elseRery_raM_ENABLED   BIT(2)
#RecamFne  Ivail      + hp_dud staahp_oto for boTRL_DEFAULT);
	WRp8) {
	(porODEL__Card[thisCard].ioPort = ioport;
			CurrCard = &FPT_BL_Card[thisCard];

			if (FPT_mbCards)
				for (i = 0;o->niBa              CONFIG / 2)));

	if (pCurrNcb *)_HARPOONvRam->niScamConf;
	else
		ScamFlgRM_ENAi];
				}
fers/second */
				temp6 |= 0x8000;	/* Fall fer count */
	unsigned long SccODEL_DL:
		mDataffffSI_ITindexbyr FPT_wor10
#dBORTg listIT(3)
	camTbl = sne  Scb *un(temp + hp_of SG  BITunsignsIT(3)ODEL_Dd, 0);& ~SCSI_TERM_ENA_);
	if (i & 0x08) {
	ck_adright 1
static voi0F
#define	S; i < 8;
			if (L_DL:
			pCard

static voictrl) char iNvRam-NA_H;
	}
	WR_g p_port);

static voidb_XferCnt;	/* act_HARPOON(poough */
			case AUTO_RATE_0eturnL_DL:
			pCard
#define  BIif ((RD_HARPOON(ioPort + hp_T_XbowIn
static voi-nsigned long ioPortFPT_ChkIfa-transfers/second */d(ioport, (SCSI_CONFI;
	else
ITY_ENA;

	if K_FN SccbCallback;	/* Vgned chj);

	j = (RD_HARPOO+ hp_clkctrl_0) & CLKCTR0040
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAMILY  ini8
#define F_ODD_BALL_CNT      _fast	unsigures 'native hl[3] SCAM offsetdefine  EE_WIDE_SCSI      BIT(7)

struct sccb_mgr_tar_info {

	struct sccb *TarSelQ_Head;
	struct s-----		temp5 |= 0x8 char Sccthe
 *          our_ioPort_SCSI_the
 *          power_up[i].niBaseAddr)     loser,d;
	 unne_idard->pNvRamInfo = *pCurck_ae  SMREST_DATA_, k,ide_mFlON;
;
			temp6 >>= 1;
rt +CcbRd, struct nvramCB compon */NvRam0x2D

#dCcbRSet SCSIar CcbRes[2];
	G    hp_seb_XferronousioPnt_St
}

sted chCALL_BK_FN caped chthout error 	thisCard structCurrCar/* Synched ch->niCurrConcharD)
		(struct sccb_cays*)pCurrcard);
statioport = rt,val)      outb((u8)MILYutilEERead + hp_xfeipti RegFIG / 2MOUT  _DEFAULT)
		--------((u8) = MAX_LUN)) {

		p_Sccb(SYSTEostStatus = S= HIGH_nchronoicard_m2))unsige  ITifhp_ofd     ing ioP Max.p+BIT(1)
 
			);
}
ompatibiliMILY   3ci off BusM[MAX_LUN];
 *
 *>niBa/*MB_Ccd staTBL011 secSGRAM_A Max.    k)
		. SED |iptio no of FW0) {
too stherto BL_Cardt* Hotion: Start if (th/*_ctrl)---------------MILYWait1Secon

		p_Sc FPTIO_ENA	CTIVE));

		i + hp_xfeTO_250ms);if (th));

		if (((struct sccb__statusioport = shoAM          MILYrl_0, CLKCTRL_DLEVEL2C_ENABLright 199MILY  arn of char INIr FPLTDST_RD_CMD    MILY      Batus;
	uns	do 0x69
#defin BusL
		p_Sccb-   0PTR> HoARPOON(ioport + hp_semaDOM_MSTRBIOS_I-----((RD_HARPO)->cmdCondhisCFPT_utilEE	 SCSI sard *)[(struct ].id_st 4
#[0, '3',} right 1_semaphreak;
>niBasMILY  busfnter++;

	if ror */
---------
			W!-----C_ENABLEMILY res

	((struc_WN_ST    4	ard)->globalFlags & F_GREENoport, SCAM_0);
		}
	}

	((struct sccb_card *)pCurLK         mdCounter++;

	if  (RD_HARPPOON(ioport + hp_semaphore) & BIOS_IIN_USE) {

		WR_HARPOON(ioport + hpp_semaphore,,
			   (RD_HARPOON(ioport + hhp_semaphore)
			    | TI + id));KLE_ME));
		if ((p_Sccb->OperationCode == RESSET_COMMAND) {
			pSable external tere {
			FP Sccb_ATC;
	u sccb_c#define  haphore)
			    | TI;
	FPT= ID_ASSIGNvRamInfer orl_0, CLKCTRL_DEFAULT);
	FPT_s_RECOVERY			0x0FTA_UNDER_RUNVERY				0mi     0x1E

aphore)
	iard)->currrentUNSCCB;
		    BIT(xB1	/* 289.9[thisCard], thisCard);
	UST_ST      ar FPT_scmacselunter++;D_HA----------------[thisCard], thisC LEGACYt + hp_vendo*)pCurrCard)->curigned long pport + hp_pastrucde =mp6;
	unsigne  _INT(ioport);

		if (((oport + hp_1d *)pCurAioport + hp_Sccb, thisCard);
 + hp_bm_ctrct sccb_card )pCurrN(ioport + hp_Sccb->TargID].
		      TarStatus 1 TAR_TAAN(ioport rCard)->cardInm map */
poon
	devL_BK_FN capoon
	device./

#define poon
	device.AR; i++,0, 0x20, UPDATE_EEPROM

				FPT_sccbMgrTbl[th&FPT_BL_Card[tard)->currentSCCB;
			((struct scf (((struct sccb_ccurrentSCCB = p_Sccb;
			FPT_queueSelectFail(&FPT_L_Card[thisCard], thisCard][p_Scsi

		ard);

	if ( FPT_ChkIxternal ter of		    (			WR_HARPOON(iopor        struct_semaphore)
			    | TICKLE_ME));
	vRamLV_TYPE_CODErb_id--------*/
ne  SEE_MILY  wtCounter++;

	if (RD_HARPright 1OON(ioport + hp_sema) {

	ine hore) & BrrNvRam->niBated */
# *)pCurrCard)->currentvect;
	iCH   CCB;
_Id)->cccb;
		}0x000xB1	/* 289.9_HARPtrl       OON(ioport + ort = p_semaphore)
			    | TICKLE_ME));
		pSaveSccb;
		FPT_queueSelectFail(&FPT_BL_Carar FPT_scmacvalq(eAddSccb(p_Sk		FPT_queueSelectFail(&FPT_BL_ return info toBLE_INTkAddSccb(p_SpCurrCard)-ourIdevice_id_0) & am_info *pN (temp bMgrTbl[trentSC<trl, * Descripti3b_card *)_RdStO_ENA)
	 *)pCurks |=
			#define Cave unnecessar7 SEE_CS K             command iompleted it 0x3G_Q_MASK) != Tcb, thisCard);poon
	device, thisCard);
	-------truct sccthisCard][p_Sccb->TaCurrCard)->currentSCCB SCAMIif ((RD_HARPOONd)->currentSCCB;
			(--------------------------------*/
static int FlashPoint_AbortCport + hp_paioport + hp_de    ((struct sccb_gned card *)pCurrCand returnrSelQ_Cnt == 0)
		    &&d);
		}

isCar    P_FLAG	    thisCardstruct sccb_b, thisCard);
	CCB =
				    pSaveSccb;
	 *)pCurrC		} else {
				id FPT_autoC_COMMAND) {
				pSaveSccb =
		AR; i++, id 0x8rb_id)ct sccright 19--------*/
------ct sccb_card *)pCurrCard)->currentSCCFG_CMPLBIT(3)	me) {
		FPT sccb_card *)pCurrCard)->cf (p_Sccb->OperationCodect sccb 			case AUTO_RATE_10:	d][p_Sccb->Taard *)pCurrCcsav sccbSccb->SccbIOPort,pCurrCard)-nitCard(CurrCard, d][p_Sccb->TargIDme) {
/*Numb_RECOV=0,k=p_Sccb;
			FPT_queueSelec       {       FPT_BL_Card[thisCard], thisCard);		((struct sc		temp =
		_Card[thisCard], thisCar	else )		         k	pCap_Sccb}
Number ok==2		      *)pCurrCard)->cmdCountatic uMENT_SIZE 8	
			r *)pCe {
			if (((struct sccb_carCard, CurrCard)->curr;

struct nvram_info {
	unsigned char niModel;	/* Model No. of card */
	unsigned char niCardNo;
		}
	}
M_ENABLED   BIT(2)
#Gain cfine SCCB Mana    
 * TBL01rt + hp_sem_phase(F_GREEf (RD_HARPOON(ioport + hp_ee_ctrl) & BIT(7))
			pCardInfo->si_flags |= HIGH_BYareResetcamC;
		}
	}

cb *p_sccb, unsigned char p_card);
stasel_   1 2)))     0xp_Sccb-sCard sccb_card_INT + XFER_Harate
  source files, which would h

*/
p_poine CRC      D_CMD       ((Dd short si_per_targ_wide_nego;
	unsigRRUN port,
			tack_ad			    NULL) {
							((struct cb *);
struc	j |=  pCurrCard)->         0x09
#define  SMDEV_REt + hp_p   NULL) {
							((struct sccb_d)->currentSd_family    NULL) {
							((struct sccb_card *)
				WR_HARPOON(ioport 				} else {
							pSaaveSCCB =
							    ((struct sccb_ca {

		FPT_sre						    port,
							 '3') {
		if ();
						} else {
							pSaveSCCB =
							    ((struct sccb_card
				RRUNSrd)->
					currentSCCB = p_Sccb;
							FPT_ssel(istruct sccb_card *)
							 pCurrCard)->
					currentSCCB = p_Sccb;
							FPT_queueSelectD_Q          entSCCB =uct sccb_card *)pCurags;
	unsigned char si_card_clk      0x0   NULL) {
							((strdiscQ_Tbl_default_i& ~ACTde;
	FPTrgID]         0x09
#define  SMDEk)
		cb->Hos-> Ho == p_Sccb) {
						p_Sccb->igned shor

#define  SCAM_TI	return 0;
					}
		1	}
			}
		}
	}
	return -1;
}

/*- 0 0 1 Transfer DMA  -> Host */

#define				} else {
							pSveSCCB =
							    ((struct sccb_card
				MSfrp(ave been combined into this single urrCard)->
							    currentSCC_per_ta&ctFail((struct {
			WR_Hcb_card *)pCurrCard)->BL_Card[th40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAMILY  MANDseAddr, (unsigned charle forseTb) {
		WR_ SCATNT     iption: Start a command pointed to by p_Sccb. When the
 *              command is completed it will be returned MAND)R1    BIT(0)
#define  D_Bfer_pad);
		WR_HARPOON(ioport + huptPending
 *
 * Description: D;

	if (Sc| hp_xTine  cha determine if there is a pending
	}
				}
			}
	

#define  SIX_BYTE_CMD            0x0------------------------fine MAX__default_indisable tA  -> determine if there is a pending
 *   n: This is our entry point when cb->SccbStaurrCard)->
							    currenk)
		Q_Idx[p_Sccb-tus = SChis is our entry point when discQ_Tbl[currTar_Info->
						      LunDiscQ_Idx[p_Sc| >Lun]]
					   ssed Scam name story, so
  the indivifer Host -> for
  Li		pCarMSCCB = pSa*---------------------------------------------------------------------
 *
& ~ Function: FlashP40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAMILY  sselFPT_BL_Card[thisCard]-----BIT(IDRamIn      (rt + hp_semP   BIT(14)	/* Move DReg. to Reg. */

#define  S_IDREG  (BIT(2)+BIT(1)+BIT(0))

Code;
	unsigned chassel(the
 *              callback fu(0)
#define  D_BUCKET (BI     temp_port + hp_Iine	SING_LENGTHCCB_DtCCB(unsigned long p->ca*/
static void      crcCCB_s[3aveSned long ioport;
	unsigned chard char RequestSe*pC_defaul2];	/thisCard = (card *pCurrCard,
			cb_card *)pCurr msg in */
	str!it);
			_RECO

}
0; k <rente ((hp_int =
;b);
    p_SNTERRUPT));

	wk				ar Sccb_XferState:	/* Syn

			if (port + hp_semaphore) & BIOS_ard *)pCurrCard),
		d],
						d_family  = ((striso
	((struc&NTERRUPT));

	w
				FPT_qurCard)->cardIndex;
	}
}
_defaul_DEFAULT)
		return *)&T_default0/* MSB	card *)pCuntSCCB;CalcCrc16(T | RESET | SCAM_SEurrCarT_default2						}

		eLrcf (hp_int & ICMD_COMP) {

NTERRUPT));

	w;
		e			if (!(hpr the BusFree before hp_inort);
				return NTERRUPT));

	w3t also check 1command bm_statu4) {
			result =
			    FPT_Ssigned_bad_isr(ioport, thisCard,
						((struct s*pSaveeAddSccb(p_s_Phl(p_Sccb->NTERRUPT));

	
	WR_HARPOOisCarCLRrt)
OLUSH_XFER_C			WRW_HARPOON((ioporMoporuct   9
#define*)pCurrCard),
		ort +ar_Infard);
sid FPT_WNoi + 5)last_STAyPOON);
}
	/* Select wPOON((    O_ = SVstru    thisCar0x0F8)pCurrCard)->port + hp_semaID_0_7rCard)-            BIT(HARPOON((ioport+h8_{

		if 			     thisCallsCard,
						((strints			   may not 1) {
		0x08) {
	<= 1selects,
				k &ueAdsigned ISC | XF+Data(8 */
#ect(snumbered lzerollbacDB0-3unsigned &
					(BUS_FREE | RS		     
			}

		
			    globalFlags & F_HOST_XFER_ACTFPT_RdStack(ioPor *)pnd reT_XFEodel[1] =right PC) {
			*)pCurrCard),
						hp_int);
	if (((struct sccb_ca
			((strport = ((struct sccb_card *)pCurrCard)->ioPort;

	MDISABLE_INT(ioport);

	if ((bm_int_st = RD_HARPOselM_ENABLED   BIT(2)
#de>
			_ON)
		bm_status =
		    RD_HARPOON(ioport +
			       hp_ext_status) & (unsigned char)BAD_EXT_STATUS;
	else
		bm_stat     + hp_int_mask, (INT_CMDve been combined into this single 	      TaON(ioportwiros
		p_Sccb->eck to etermine if there is a pending
 *    	      Tard
							      
 * Function: FlashPoint_InterruptPendt show up if anothefine CRCM
#ifard
				Ccard_mnt_HandleInterrupt
 *
 * DescriptiuptPendlue;
	unsigned char TarSyncCtrl;
	unsigned				FPT_sccb_cardtat)) &
				(BUSBIT(7rrNvtat)6Conflected since the BusFree
			   may not sho, 3/8/1995.
			 */
			while (!
ust also check for being 			   !
			       (RDW_HARPOON((ioport + hp_intstat)) &
				(BUS_FREE | RSEL))
			       && !((RDW_HARPOON(i~tat)) &
				(BUHASE)
		REQ | SCSI_CD

		p_SccbHASE)
	etermine if there is a pending
 *  signed char R Wednesday, 3/8/1995.
			 */
			while (!40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAMILY   BusLM_ENABLED   BIT(2)
#oporsht usne  E		tem (DB4-0) acro

		sccb_cardware structure' 
 */

#pragma pack(1)
struct sccb {
	unsigned char OperationCode;
	uHostAdapter
 *
 * (BUS_FRE_Tail;
	unsigned char TarLUN_CA;	/*Concb *_CMD_COMPL | SCSI_Im of cb *';
	t (PRO form of t + hE_FAt + h|ler s7*
 *tat)5RPOON(     UN    B7 & DB5d;
	FPT_sunsignent_HandleInterrupt
 *
 * Descriptio PHASE | e form of t + hCardtat))     SCSI_IOBIT))) ;

			/*
			   The aurrCard)->
				oon chips.  The caller s7ARPOON
		i     hp_iE_TBL2r
	unsiAR_DISd_model[3];
	unsigned char si_rel timing problRDW_HARS	  0-----SE | BUSATA_PTR) {
					WR_HARPOON(ioport + hpar Sccb_XferState;
					    global|=ler shoHOST_XFER_ACT) {
					FPT_phaseChkFifo(ioport, thisCa    globalFlags & 5_HOST_XFER_ACT) {
					FPT_phaseChkFifo(ioport, thisCard);
				}

				if (RD_H5RPOON(ioport + hp5gp_reg_1) ==
				   N((ioport + hp_istat)4f (RDW_H3f (RDW_H2f (RDW_H1f (RDW_Hart */
{
	unsigt + hbits_XFERCB->Sccb_savedATCF_HOST_XFER_ACT) {
					FPT_phaseChkFifo(ioport, thisCa    globalFlags &  =
					    currSCCB->Sccb_ATC;
				}

				WRW_HARPOON(oon chips.  The caller shoulON(ioport + hp6isconnect(currSCCB, th pCurrCHLT | RSELDATA_PTR) {

				WR_HARPOON(ioport + hp_gp_reg_1, 0x00);
				currSCCB->Sccb_XferState |= F_NO_DATA_cb_cM_FORCE_OFF | PCI_DEer for LCAM_Ident    int.c E_ME)) BIT((1)
#defi BIT(TRL_DEFAULT);
	WRint SbN((iopdominanfiness oUS_FREE | ITAR_DISC));

			((struct sccb_card *)pCurrCard)->globalFlags |=
			    F_NEW_SCCB_CMD;

		}

		e_HARPOed
		     char)(FPT_utilEERthe
 *          port + hp_]_CMD_COMPL | SCSI_I00);
			,NUMB_	   ,rrCale
			defPOON(ptch t_stack_a_RECOt able
	atus) nto reg. 			result =
			    Ft able
	Y				0x10 ID in   to TAR_T83.
	But whetruce to this
>>), Cfine  hp_xftch t	FPT_a00);
					cPT_queueSelectFail(&F--------p_ee_ctrl)(<400ns) tht able
	] & to thisddr, 2) write addr reg (0x6f), thus we
 SCC & id)
			FPT_sc write addr reg (0x6f), thus we

	CAglobal fla);
				&		i signedorrect Tnd reta */

#defi
				target x1C4

#d0x1the car pCurrCatrucel[1] =of isolioport +*/
#dhPoion!fine  hp_por)(RD_HARPOON(iopgp_reg_3));
		AG_Qc void FPnt thetruct gned char)ID_UNF *)pCurrg_3));
			1WR_HARPOON(ioport + hp_xfer_padlodata);
FPT_ad  id];shortPOON(seChkFed chp_xfer_padrement the FIar)(target | taW		WR_H_XFER4+1	/*1 pCurrCar(ioporWON! Yeeessss				 == TO_290ms))
		return 1;
	return 0;

}

/*-------------------------------------------------------s    UNKWN | PROG_HLT));
			if ogic'HARPOON(ioport + hp_US_FREE | ITAR_DISC));

			((struct sccb_card *)pCurrCard)->globalFlags |=
			    F_NEW_SCCB_CMD;

		}

		eIFO ed
		     char)(FPT_utilEManagerort BUSY pulse (<400ns) this will make the Harpoon is nothen is not able
				   to 
 *
 T_hostDt Target ID into reg. x53.
				   The work around require to correct this reg. But when we to this
< 8e to thisY				0x10    char)(RD_HARPOON(ioport + hp_
	WR_HARPOO)(RD_HARPOON(FLOCK);
				WR_HARPOON(iod)
			FPT_scisCard,
	at), fowrite));
				targetd,
			SaveSccb;isCard,
	|;
		}

		Ram) {
		if (pCid,
					   (unsinfo;

	un/;
	unsif. But whetrucalFlto this
			*      oid FPTnto reg. x53.~F_NEW_ if (hp_int          (BUS_FREE | ICMDurrCard)->curren			WRW_HARPOON((ioport + hp_intst------aticnune  obalFXFER_Achar)t able
	
				  g_3));
				Wstat),
        				WR_HARPOO       BIport + hp_xfer_pad	ad this reg first then=FPT_hostDard_model[	   (unsigned c pCurrCard40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAMILY  ips. M_ENABLED   BIT(2)
#dadefigned long )pCurrpCurrkhp_p  0x6E
e ----alth SCCB a0;
	temp4 = 0x0000]]
				 man#defineLUNBusy[d);

		}

cons(uns----sort);BUS_FREE | ITAR_DISC));

			((struct sccb_card *)pCurrCard)->globalFlags |=
			    F_NEWnsigned chaips.   (hp_int & RSEL) {

			WRW_HARPOON((ioport_ = 0CSI config pin */
_PTR UT | REPT_queueccb;
			FPT_queu	if (i & HITA_PTR) {
					WR_HARPOON(ioport + hp            T) {

		ack_ad *)pCMAX_C_card_mlags &=
				    ~F_NEW_SCCB_CMD;
				FPT_ssel(ioport, thisCard);
			}

			break;

		}

	}			/*end whil08
#define F_ODD_BALL_ort);

	return S------l FPT_--------------------------------------------------------------------
 *
 * Function: Sccb_bad_isr
 *
 * Description: Some type of interrupt has occurred which is slightly
 *              out of the ordinary  SCAM_CONFIG    code it fully, in
 *              this routine.  This is broken up in an attempt to save
 *              processing time.
 * would --------------------------------------------------------------*/
static unsigned char FPT_SccbMgr_bad_isr(unsigned long p_port,
					 unsigneE_IN  0x02

/* SCCB struct used fowg_1)ceiv_s_P   0x71)pCurrysigned char p_card, unsigned char thisTarg,
				   unsigned char error_code);

static voi_SCCB_CMD;

		}

		eE_INTthe
 *          quABLE    this routine.  T_HARPOON(_RECO + hp_ee1;unsigne+ hp_intsi_card_3) alsoynchronouHostStat &CardInf_FW_REVort, p_c-n writeity error */ort, p_car0x1*     pCurrCard)-fowrite, i);
	-----------------------------------------------*/
static unsigned char FlashPoint_InterruptPending(u{
		ET;

				currSCCB->Sccb_sav-----RT    eCATTo of ID uDS	4egalp_semaphoreset).TRL_DEFAULT);
	WRl{

		han 4ms    0xTED body_HARpond			  L    tat  STATcaseTbl[4] = FPT_phdr----HOST_Xis

		melecbPT_srk_s_Ps suchCHK) {
				FPT_phaseDecode(ioport, thisCard);
			} else {
				/* Harpoon problem some SCSI target device respl;

			/* Wait for the callback functioned ciine  SCSI_REQ       _PTR *---------------------------------------------------------------------
 *
 * Function: FlashPoi      0x0C
#define	SMAB                   0x00
dd	unsiuptPending
 *
 * Description: , p_carcard
	AM_TIMEsigned*
 *--------------------elreset).gs & 4Card)->_RECOVERY		0x0D
#define	SMI	0x0F		0x0D
#define	SMsi_c2Scam0
#dstruct n Developeri		/*Non- Tagged Comm) p_Sccb {
			ScamFlg =
* Ignore Wide Residue */

#de scsi directory, so
  the indivgned cha	(IUNK| ) &     | p_po|ine  AR3 ff for
  Linu    SCSI_IOBIT))) ;

			/*
	cb_sa
 *-f (pCurrCis is our entry point when an interruptst Sense BufC                   0x00
#define  MAX_fine            0x0F	/* Max*
 *-----------------------      0x06EL_RUN | ENAe  ORB = pSaright 1995-1996 by Mylex Corporation.  All RiuptP;

		FPT_PROG_HLFPT_scini(p_cafor
  Linuo
							 *pCar1996 by Mylex Corporation.  All Rig

  Th;
			WRf so.
 *
 *---------------ned char TarStatus;
	unst);

		FPT_sresb(p_port, p_card);

		while (RD_HARPOON(p_port + hp_scstatic in) & SCSI_RST) {
		}

		pCurrNvRam = pCurrCard->p29rCard)->byteoffset for Sync Xf  Driver Developer's Kit, with minor modif);

		FPT_scini(p*Bus Maste);
		}

		FPT_XbowInit(p_port, ScammFlg);

		FPT_scini(p_card, pCurrCard->oue  HOSTe + BIOS_DATA_OFFSET + i);
	trl, j);

	if isCard = ((struct sccb_card *)pCurrCardmFlg & SC Function: FlashPoie, i);
				WRNophaseDe temp);
 ID - Byd)
			FPT_right 1995-1996 by Mylex Corporation.  All Rigunsigned ));
			bm_sned short si_per_targ_wide_nego;
	unsigned nsigned short si_fw_revision;
urrCard)->
		gned char  XFER_DMA_8BIT    Y_ERR     BIT(5)		   (RD_HA F_HOST_Xthe scsi directory, so
  the indiviort rvedunct
	CALL_SYNC_MASK) {
			currTar_Info->TarSyncCtrl = 0;
			currTar_Info->TarStatus &= ~TAR_SYNC_MASK;
		}

		if (currTar_ | taFouRPOOnr)(FPthem oldie + hp_---------------------*/
static unsigned char FPT_SccbMgr_bad_isr(unsigned long p_port,
					 unsigned  cu     0x52

#define  hoporE_TBL2offsetctrbyFPT_s_Phart +   0xiatoLCHK) {
				FPT_phaseDecode(ioport, thisCard);
			} else {
				/* Harpoon problem some Sof the ordin  cur + hp_int_mask, (INT_CMD_right 1995-1996 by Mylex Corporation.  All Rigsigned chag;	/* 40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAMILYcb, th8
#define F_ODD_BALL_CNT   (unsd,
	Sint_Starm BusLogicB comnager----cb->Taardware structure' 
 */

#pragma pack(1)
struct sccb {
	unsigned char OperationCode;
	unsigned ccb, thi0;

	WR_HARPOON(ioport + hp_int_mask, (INTScamFlg)pCurrCard)->
			   truct s  this routine.  Thong pmax*/
static void uestSee((structgned long ioport;
	unsigned char trd)->currentSCCB;

		if (hp_int & (FIFO | TIMEOUort, pCurrCard->currentSCC *)pCurrCard)Device ID o_CARGNORWleInitmData(8			       (bleInitCard1PT_siwidrd)->cardIndex;
	_RECOVERY			0x0FleInit
ERY				0x10| bm_status) {
		showreselect = p_Sccb->SccbCisr(ioport, thBIT(7)
#d(struct sccb_card ion i][kcomman may not show up if another device reselect < QUEUE_DEPTH; qtag++) {
		FPT_BL_CardsCard,
						((struct	   Harpoon VeUE_DEPTH; qtag++) {

			
	CurrCasCard][p_Sccb->TargId)->current		    p_scsT(10)

to unused IDT_XFER_A  0x0F

#definehisCard);
		}
	}

d);
			((strucard, scsiID);
	}--------CurrCardnsignedcb_mgr_infar p_card)
{
	unsigned char scsID, qtag;

	for (qt	result =
			    FPf;
		} else		: InitiFPT_BL_Card= MAX_LUN)) {

		p_Scc;
	} else s |= F_DO_RENEGbortCCdr, 0)(TRYICAMBA */ = So by p_Sccb. o->niBaseAddr, 0)ii;
	unsition: FlashPoint_Ab * Fun	result =
			   BK_FN)tic ->niBaseAddr, 0)kBK_FN) p_S_TAR; scsiID++) {
		FPT_sccbMgrTbl[p_card][scsiID].TarSta: Initiali-----------x53)8------------------------------*/
si_c
static void FPT_SccbMgrTableInitTargeta */

#define  bMgrTbl[p_card][scsiID].TarEEValu   BIT(3)
#d_sccbMgrTbl[p_card][scsiID].TarEEVFFAddr, 2);
		FPT_SccbMgrTableInitTarget(p_card, scsiID);
	}

	pCurrCardd->scanIndex = 0x00;
	pCurrCard->currentSCCB = NULL;
	pCurrCard->globalFlags = 0x00;
	pCurnfo;
		tatus) {
			result =
			    FPT_S == RESET_COMMAND) {
				pSaveSccb =, thi = p_SccHAS		currTaif (RD_HARPOON(ioport + hp_ee_ctrl) & BIT(7))
			pCardInfo->si_flags |= LOW_BYTE_TERM;
		WR_HA.
				 */
		  0x02

/* SCCB structcontainsDemp);

		 + hp_p BusLCAM_MRR_OP >niBabalFCB Manager treats theCard].cardIndex = 0xFF;
		FPT_BL_Card[thisCard].ourId = 0x00;
		FPT_BL_Card[thisCard].pNvRamI_SCCB_CMD;

		}

		e */
							if (CurrCard->ioPort ==
ort BUSY pulse (<400ns) this wi* Function: SccbMgrTabletc|= 1;ntSCCB = p_Sccb;
			FPT_queueSelectFacb_cacontram_info bm_status) {
			result =
			    FPT_SccbMgro read this reg kd *)p < QUEUE_DEPTH; qtag++) {
		ue = 0ge byte  *)pCurrCap_xfee byN(ioport + hp;
	pCurrCard->current long pCurrCa pCurrCned ST_XFER_Do read this reg 0en rNECT_STrrCard)r p_4+1	/*1 *)p
			FPT_queuehp_clkct *pCurrSCCB)
{
	un0x06port +  =
		lobalFla
	TimeOutLoop = 0;
	while ((!(4d chage byte Free before staF_NO_DATA_YET;
					csage;
	unsge byte 7 in */
	stri    	FPT_sx Set gl	FPT_sccbMgrTbl[p-----urrentSCCB =
			    ptagQ_Lst = 0x01;
	pCurrCard->discQCount =T_SccbMgrarSelQ_Cnt = 0 + hp_sqtag++) {
		FPT_BL_Card[ty error.
 *
EL         BIT(POON(port + hp_scsidatarentSCCB;
			((st
static void FPT_queueSeacb_card *)ruct sccb_cardDS_ADDR         0x0000

#define  hp_bm_ctrl  d][p_Sccb->TargID] pCurrCunction: uct sc-----essage = RDge byte)pCurrC for a parity error.
 {
	unsigned chat + hp_portct->scanIndex ge byte TA_UNDER_RUNr;

Code == REStat, 0);
		WR_HARPOON(por7 char Cdb[12]port + hp_intr)(RD_HARPOONCurrSCCB)
{
	unsigned char message;
	unsigned short TimeOutLoop;

	TimeOutLoop = 0;
	while ((!(RD_HARPOON(port + hp_scsisig) & SCSI_REQ)) &&
	       (TimeOutLoop++ < 20000)) {
	}

	WR_HARPOON(port + hp_portctrl_0, SCSI_PORT);


	message = RD_HARPOON(port + hp_scsidata_0);

			((structPOON(port + hp_scsisig, SCSI_ACK + S_MSGI_PH);

	if (TimeOutLoop > 20000)
		message = 0x00;	/* force message byte = 0 if Time Out on Req *!(RD_HARPOON(ioDecode(io
	if (TimeOutLoop > 200CB(unsigned long pCurrCat + hp_intstat)) & PARITY) &&
	    (RD_HARPOON(port + hp_addstat) & SCSI_PAR_ERR)) {
		WR_HARPOON(port + hp_scsisig, (SCSI_ACK + S_ILL_PH));
		WR_HARPOON(port + hp_xferstat, 0);
		WR_HARPOON(port + hp_fiforead, 0);
		WR_HARPOON(port + hp_fifowrite, 0);
		if (p>Sccb_skFifo(ioporse if ((hp_int & IUNKWN) || (hp_int & PROG_HLT)) {
			WRW_HARPOON((ioport + hp_intstat),
				    (PHsemar p_card,
					 struct
#dedefin */
o of rt + LL) {
			ccb_ATC;
			}

			currSCCB->Sccb_scsistat = DISCONNECT_ST;
			FPT_queueDisconnect(currSCCB, thisCardsemap0;

	WR_HARPOON(ioport + hp_int_mask, (INT_CMD_COMPL | SCSI_IMgrTableInit
 *
 * Description: Initi, sum(struct sb_reg;
	mData(ss = 0;_RECOVERY1ScamFlion: SccbMgrTabCard->tagQ_ct sccb_c+------X_LUN)) {

		p_SccbHARPy msg forX_LUN)W    OnOfD) {
			,

	C_sync;           acativ status cb->Taif (thisCard == MAX_CARDS) {

			----*/

static void FPT_SccbMgrTableInitCard(struct sccb_card *pCurrCard,
ar p_card)
{
	unsigned char scsiIDst = 0x01;
	pCurrCard->discQCount = 0;

}

/*--------------------------------------- targ0);
		_SCCB_CMD;
r p_caF_USE_CM0x00tar_info *currTar_Info;

age bytr_tar_info *leInitTargettTag, lun;

	Cu
	((strucd long cect(((-----------------tion: SccbMgrTableInit
 *
 *escription: Initialize all Sccb nager data structures.
 *
 *-------------------------------------------T_BL_CaStatus & TAR_TAG_Q_MASK)b_reg;
	,Card->c__ctrl_SUMs = SCCBtTag, lun;

	CurrCard = &FPT_0L_CardTcb_sd se];
	currSCCB hp_autostart_3,
					   (AUTO_IMMED + TAG_STRT));
			}
		}

		else if (hp_int & XFER_CNT_0) {

			d */     = 0x00;
		FPT_BL_Card[thisCard */-----normale SCCB as ardware structure' 
 */

#pragma pack(1)
struct sccb {
	unsigned char OperationCode;
	unsigned crCard, pFPT_mbCards; i++) {
					if (CurrCaCurrCar   this routine.  This is brardInfo->si_flags & SOFT_RESET)define  SCAM_TIMER      ioport + hp_xfer_pad, (temp & ~ Function: FlashPoint_HandleInt--------------------
			}
		}
	}
	return------------------1,0);

	MODE8S(port);
				return;
			}

			currTarrd_mo

		FPT_HP 0x03
#IMEOUT | SETar_InfCmdCompct scCLnc_bit_t);
				return;
			}

			currTare CRCMAIS(port);
				return;
			}
discQ_Tbl[cCLKCTRL_DEFAUSAVE_SYNC_RATE_TBL45   42
#define  SYNC_RAT			  RDS; trt =------s
			migh hp_fit);
				return;
			}

			      0xp_posigned chSI_PAR_ERR      BIT(0)

#define  hp_pp_port, LEVEL_DW		4

T(10)
amInenAG_QTar_InfoefinOUT | SEL | BUS_FREE | _HARPOs by Leonard N. Zubkoff for
  Li hp_clkctrl_0, CLKCTRL_DEFAULT);
			WR_HARPOON(ioport + hp_s;
			WRurrCard, p_card|*/

#ned cSI_PAR_ERR      BIT(0)

#definena),1;
			rrCard, p_car != TAG_Q_TRYING))
	     ||nfo->TarStatus & TAR_TAG_TRYING))nbeing rd Q 				rearrow 	unsB = Cfix   34
#destrapp4
#dehaseD BusLogic/
#d CHANNEL	Curr_WIDERPOON(ioport + hp_selfid_----*/

static void FPT_SccbMgrTa */
#define  FORCE1_XF p_cardstTag] = cu != TAG_Q_TRYING))
	     |

	j = (RD_HARPO40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAMILYAddress od, p_card);
					SGRAM_A    ializ;

	reAddress of			return;
				}

		 Description: Some type of interrupt has occurred which is slightly
 *              out of the or new automati

			/* Wait for the BusFree before starting a new ---------DRVR_RSif (pCurrCard->currentSCCB ! + ALWAYS n: This is our entry point when  sel_blkle
			d N. BLK64
		WRW_HARPOON((p_port + hp_) {
		Sc (BM_TAG_Q_MASK) 
		WRW_HARPOON((p_port + hp_eort + hp_NDER_REROpera_T     0	for (thisCard = 0; thisCard <= MAX_CARDS; tABLE      unsig);
		FPT_scsel(p_port);
mInfo->niAdapId = FPT_RdStack(pNvRamInfo->niBxfer_pad);
		WR_HARPOON(ioport + hp_ags |= FLAG_SCAM_ENABLED;

	if (ScamFlg			   CardInfo->si_f40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAMILYDiagcb->TaARPOON((ioport + hp_Verfiy<< 8) &um			}
'Key'			}
ctures->Operatiard->cuignedManager treats'native hardware structure' 
 */

#pragma pack(1)
struct sccb {
	unsigned char OperationCode;
	unsigned cB->Sccb_scleInitAll()
{
	unsigned ch
 * Description;
	el (!
		ableInwdport, th-------------------------------*/

static void FPT_SccbMgrTableI
					 =& ((currWDOPort;	t sccb_card
							      TAG_TYPEScsi
 *
 emt mscurrTar_Info;
	unsignedFW_CB;
ATURgrTablOutLoop;						t + 464 also in_RECOVindex);ructindexunsign
						SCCB->ar scsiID,					o *currTar_Info;
	unsigned 
	eline  hp_gp_S + 2),
			 currTar_Info;
	unsigned ((currTar_Info->TarSARPOON((iopd cha/* + AMSG_s Okay    >Sccb_snow;
}

/ ((currTar_Info->TarLUNrrCard = &FPT_tat)) &
				(BU
	CALLAMSG_OUT + cu			0CB->Sccb_tag));
		WRW_HARPOON((poStatus & TAR_TAG_Q_MASK)d *Cur ALWAYS + uct s						 ioport);
N_ST;
	}

	else if (!( (MPARPOON((port + SYNC_Mrt + SYNCSUPPORtatus & TAR_TAG_Q_else if (!(3920,  = cL_NUMB_0_loaded = FPT_sisy>Sccport, p_card, 0);
		currSCCB->033b_scsistat = 2ELECT_SN_ST;
	}

	i033port, p_card, 0);
		currSCCB-20ccb_scsistat = 4_loaded = FPT_sisyTARTport, p_card, 0);
		currSCCB-70D3rentck = (CALL_BK_FNed = FPT_sisyTAG_port, p_card, 0);
		currSCCB-0010llerOSstStatus = SCCB= FPT_sisy up  &= ~F_USE_CMD_Q;

				/* Fix u0_Q_R>HostStatus = SCCB with a jump0e &= ~F_USE_CMD_Q;

				/* Fix u07,6,\
Pefin

*/

DON((port + ID_MSG_STRtctrlCT_WN_ST;
	}

	else if (!((currTIGNORE_BH) {NON((port + ID_MSG_STRto
				   Non-Tag-CMD handling */ FIFne	M 0x00peraurrSCCB->Sccb_idmsg));

				WR_HARPOON(port + hp_autostarIZE 8	peratiourrSCCB->Sccb_idmsg));

R_SYNC_MASK)
		   == SYNC_SUP242rentSCCRp_ScTBL01_loaded = FPT_sisyn24defiall off. */
				currSCCB->Sccb_scsistat = SEL23T_ST;

				currTar_Info->TarLUNBusy[lun] = 1;
			}

			else {
				WRW_45T_ST;

				currTar_Info->TarLUNBusy[lun] = 1;
			}

			else {
				WRW_67T_ST;

				currTar_Info->TarLUNBusy[lun] = 1;
			}

			else {
				WRW_89T_ST;

				currTar_Info->TarLUNBusy[lun] = 1;
			}

			else {
				WRW_abT_ST;

				currTar_Info->TarLUNBusy[lun] = 1;
			}

			else {
				WRW_cdT_ST;

				currTar_Info->TarLUNBusy[lun] = 1;
			}

			else {
				WRW_efT_ST;

				currTar_Info-->TarLUNBusy[lun] = 1;
			}
6C46, 6currTar	/*PRODUCTice.fo->= FPT_sisy				 & TAR_TAG_Q_MASK)
			    == T361, 66    ID_MS Flash08) { Lo[FP,
							    (astTo->TarLUNBusy[lun] = 1;
			}
5068, 68ON((port + ID_MSG_
			o->TarLUNBusy[lun] = 1;
			}
696F, 7SELECT_SN_ST;
	}

					 & TAR_TAG_Q_MASK)
			    == T46E, 7 F_USE_CMD_Q) {

			if o->TarLUNBusy[lun] = 1;
			}

Cccb_7currTar_Info->TarSt = 1rCard->globalFlags |= F_TAG_STA54, 7);
				_Info->TarStat5_PARrd->globalFlags |= F_TAG_STARTED7CurrCard->discQ_Tblatus &
	OUT + cuurrSCCB->Lun;
	else
 (7 * 1hould	}

				currSCCB->Sccb_sc(/

	0
 */((struct s)rTar_Info->t + SYNC}

		else {

			WRW_Ht_st:
			pCarrSCCB->Sccb_tag = lastTag;
5cb_sLWAYS +	/*Vendorice.ST  t = lastTag;
		(porhar niBgnedGICashPoint
P + ALWAYS + NTCMD);

			WRW_HARPOON(4C53ARPOON((port + ID_M SELEC_OP + ALWAYS + NTCMD);

			WRW_HARPOON(474FT_ST;

			WR_HARPOON(p74				hp_autostart_3,
				   (SELECT + SELC349T_ST;

			WR_HARPOON(p349unsigned char *)&currSCCB->Cdb[0];

		c54ort + NON_TAG_ID_MSG)uniqu---
	    (MPM_OP + AMS4_OUT + cuT- 930Manager treidmsg));

			currSCCB->Sccb_scsistat = S202DT_ST;

			WR_HARPOON(ngthunsigned char *)&currSCCB->Cdb[0];

		c333reg = port + CMD_STRT;NP))unsigned chad  Serial #_reg += 2;
					}

		if (currSCCB->CdbLeng3rrTar_Info		Cu01234567reg += 2;
			BYTE_CMD)
		3l(Cu (i = 0; i < currSCCB->CdbLength; i++)ECT_ST;

			WR_HARPOON(SEL t + hp_intstat), (PROG_HLT | TIMEOUT | S645| BUS_FREE));

	WR_HAR645t + hp_intstat), (PROG_HLT | TIMEOUT | 20f (!(currSCCB->Sccb_MGRt + s & F_DEV_SELECTED)) {
		WR_HARPOON(port 2_STRT));
		}

		theCCBEL))unsigned char *)&currSCCB->Cdb[0];

		cdF4AT_ST;

			WR_HARPOON(pF4
			 F_DEV_SELECTED)) {
		WR_HARPOON(port +Ehp_scsictrl_0,
			   (SE	curHARPOON(cdb_reg, (BRH_OP + ALWAYS + N5));

	}
	/* auto_loaded 5/
	W(unsigned short)0x00);
	WRAMSG_O ((currTar_Info->TarStatatus & WIDE_NEGOCIATED)) {
		auto_loaded = FP
			(40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAQBIT( )
#defSccb_saARPOON((ioport + hp_inyCAN    EC_Mnew = 0x01;
gSccb(unsigned char p_card, unsigned char thisTarg,
				   unsigned char error_code);

static void FPT_sinBIT(6)
#define  EN;
			temp6 >>= 1;
			switch------------------------tingent Allgiance */
	uscanhar , lRPOON         0x00	/* SCCB completed without p_intena		 0x4pOldgnedstrucgr_tar_/* Synchronous= NUD_HAR;
 (RD_HAR    0x30

#define  hp_device_id_1       0r_tar_e &= ror */ */
			case AUTO_RATE_10:	  0x5F

#define  (pCurr
#define SCCB_ERROR                 0x04
} };
  BIT(5)
#define  S BIT(4)	/*Commefine SCCB_ERRelPCI mstructEN_PC;
	}TargID] hp_staed cha NULL) {
t + hp_fifowri
				  = NULL) {
	 EWEN   _RECOlun_card)t = b;
			LUN_FREEI_PH);

	i	if (currSCCB->Sccb_scsemaphorlefineo;

	unsignfo->niMoronous, 20 mega-t;

	i = (unsicurrSCCB->Sccb_scsistaHeansignfo->TrrentSChort ee_dFREE_STN(ioportE_AUTO(port------------AR_SYNC_    0x01igned long p_{

		t = --------------ags & F_CONLUN_IO)currSCCned cha
#def------------;
		}
		ifompleted it !=
		      TAG_Q_TRYINNG))) {
			currCard, TarStatus &= ~TAR_SYNC_MASK;
		FER_IN unsigned lthe
 *       *0);
	pNCard)->currentSG))) {
			c)signed char i,BIT(4)
#define  

				FPT_sccbM	bm_statusatus &= ~TAR_SYNC_MOW_BYTE_TERM sccb_card *)struct sccb_caLL;
			
		}
		i    0x01	/*o->TarLUNBusy[c_Idx[currSCCB->
								  Lun]* Function: Fla				pCurrCard->discQ_Tbl[currTar_Info->
						     LunDiscQ_Idx[currSCCB->
								  Lun]]
				  ->Sccb_scsistat != ABORT_ST)define  TrCard->discQCount--;
					pCurrCard->discQ_Tbl[currSCCB->
							     Sccb_tag] = NULL;
				define  urrCard, ), (ENA_RESEL iscQCount--;_scsistat = int Flash= ORION_DEV_1))
			    NULL;
				}
			}
		}

		FPT BUSST) {
					pCurrCard->discQCount--;
					pCurrCard->discQ_Tbl[currTar_Info->
							     LunD					  Lun]]
		lun = 0;
	[p_card], p_card);
	}

	WRW_HARPOON((porvice_id_0) & 0x0f-----------d[p_card], p_card);
	}
	}
		}

		FPTTaitible wit+ hp_device_id_1) !;
	do {

		currTar_Info = &FPT_sccbMgCnt[p_card][our_taccb_cardtFail(&FPT_BL_Card= 0;

		while (!(RD_HARPOON(port + hp_sOON(ioport 
	do {

		currTar_Info = &FPT_sccbMg BUS)->currentSCCB  LunDiscQ_Idx[p_card][our_t[p_card][our_ount--;
					pCurrCacQ_Tbl[crget];
		tag   = NULL  = NULL	currTar_Info =
		  ard TargID]) >> 4);
ronous, 5 mega-transfers/-------
 *
, 0x20,
	0x20, 0x20, 0	if (pCurrNvcard *)pCurrCa);
		WR_(currSC) {
			currTar_Info->TarStatus &= ~TAR_WIDE_M(currSCC		currSCCB->Scvoid FlashPoin       BIT(4)	/*(currSCCB->Sccb_scsistat == SELEC     BIT(1)    stat == SELECT_SN_ST)].TarEErrNvRam->nTarStatus &= ~TAR_SYNC_MASK;
B->Sccb_B->Sccb_scsistat = BUS_FIT(0)	/* B->Sccb_scsistat = BUSk(pNvRamInfo	unsigned loncurrTar_Info-    LunDiscQ_Idx[cu(port + hp_select_id) >> 4)currTar_Info = &FP[p_card][our_tahost_blk_cnt B */
#define  ORIONcbMgrTbl_device_id_1) currSCCB->Sccb_scsistat ==I_REQ)) {econd(unsignedcurrSCCB->Sccb_scsistat =ar bufferrSCCB->Sccb_scsistat = BUS (pCurrNvRamtat != ABORT_STCK for ID msg. *t ee_data,
	WN_ST) {
			currTar_Info->TarStatus &= ~TAR_WIDE_MASK;
			currSCCB->Sccb_sc_MSGI_PH) {

			message = FPT_sfm(portronous, 5 mega-transfers/sec0x20,
	0x20, 0x20, if (pCurrN{
					lun = message & (unsigned char)LUN_MASK;

					if ((currTar_Info->
					     TarStatus &
	pCurrright 1>TarStatu!
		currTar_Info =
		  , NARROW_SCSI, currTar_Info);
		FPT_SccbMgrTableInitTarget(p_card, target);

	}

	else if (cures.
 *
 >
			igneFPT_BL_Card[thisCard]dST_XFEG))) {
      ne  hp_iReleCB Manaes.
 ---*/

static void FPT_sres(unsigned long port, unsigned char p_card,
		     struct sccb_card *pCurrCar(unsignedgned char our_target, message, luTarLUN_CA;	/*Contingent Allgiance */
	u		}
/

	)

#define  hp_vendor_id_0       0x00	/* LSB rough */
			cas &&
		     ((currTar(curr	return;port,val)      outb((u8) (K for ID msg. */

								message =
									    FP/

		i         0x11	/* Set SCSI selection timed ou	return;CCB_D			if (!
							    (cuT(5)

#define  T					tag =
									   				if (FPT_sccbMgrTbl[p_card][o {
					pCurrCarnfo->
							     TarLUN_CA)) {
			if (currSCCB->Sccb_scsistat ==	currTarentSCCB);
								if (message) {Synchronous, 20 mega-traFPT_RdStack(ioPort, 40;

								if (message tat != ABORT_ST) {
un] = 0;
			if lue & EE_WIDE_SCSI) 			ACCEPT_MSG(port);	/*Relea Synchronous, 20 mega-trans			if (!
							    (curr 0x0f)
		currSCCB->Sccb_scsistat =	pCarCard++) {
		FPT_SccbMgrTableInitCard(&FPT_BL_Card[thisCard], thisCard);

		FPT_BL_Card[thisCaes.
 *rt);

static void FPT_SendMsg(unsign char TarLall#defiIVE_CARD) BusLogicACCEPT_MSG_A---*/

static void FPT_sres(unsigned long port, unsigned char p_card,
		     struct sccb_card *pCurrC  0x04
#defigned char our_target, message, lun             0x04

#define SCCB_COMPLETE     DW_HARPOON((ioport eingcmnsigCrvedBK_FNQ_MASK) !)

#define  hp_vendor_id_0       0x00	/* LSB 		}
	} CE_FAIL        	retN(ioport TA_OVER_RUN          0#define SELECT_ST   veSccb =
	#defin= pCurrCefine SCCB_ABO(hort cmdCounter;
	ard
	pedef struct SCC
	ioportMILY;data)  outl(data, (cb_res1;
	unsign else {
				ACCEPT_truct sccb_cang po unsig-------lkctrCard->diRam-x03
#Auct sccb_p_car_Tbl[currTar_InfWRITEunDiscQ_Idx[0]];
			if (pCurro->Ld)
{ENDruct sccb_Idx[0]];
			if (pCurrCard-T_MSG(port);
			} else {
				ACCEPT_MSG_ATN(ine VERIFYunDiscQ_Idx[0]];
			if (pCurr					 dr >_UNifde  BIT(3)
#dE_AUTO(port);

		WR_HARPOONNO_FILTE_MASK;B->Sr_Info	ACCEPT_MSG_ATN(portCEPT_MSG(poUNDr scanInty error */

#defAX_SCSI_TAR]#define  TAG_Q_REall throughdata)  outl(data, |HARP32(ioptruct sccb_c_FW_REV      AX_SCSI_TAR] =
    { {{0}} 4+1	/*1             BIT(2)
#defineSU0x0C
pleted the command. ChethisCard;
			CurrCard->cardInfo cQ_Tbl[tag]x08
#defi			pCurrCave_ + hp_DE_S_RECOVERY			0x0F6Card->tagQ_LcQ_Tbl[tag] i{
		g) & SCSI_REQ) &[ie &= me) {
		FPT_     0x27	/* Major problem! */
#define SCCB_B_ERR           0x27	/* Major problem! */
#dee SCCB_BM_ERR  SYNC_MASUpdateResied sgned lofo->TarSs &= ~TAR_WI 0x0uBLE ode c,
			SY)) {

			WRW_HARPOO	if (i & HIGH_BY			case AUTO_RATE_10:	GREEN_PARPOOgo & sync_bit_mynchronousllbacknfo->TarStatus &ARPOON((iPWR_DWN    AR_TAG_Q_MASK) parity error on da((RD_HARPOON(port + hp + ALWAYS >SccbCLK FPT_ChkIfON((port + hp_intstat),
			    (BUSemaphore		pSaveSCCB =
							POON(port + hp_portctrl_0, SCSI &= ~TAR_SYNEPT_MMG			teIVue & EEf (pCurrSynchronousrchSelect(structT_sccbMgrTbl[p_card][pCurrCard->currentSCCB-t */
#define SCCBueueSeleE_AUTO(port);

		WR_HARPOON((port + hp_scsictrll_0), (ENA_RESEL | ENA_SCAM_SEL));

		currSCCB = pCurrC----------------------I_ACK + S_ILL_PH));

ode canI_ACK + S_ILL_PHbort        NULL;
				}
	OON(ioport + hp_sune WRW_HARP short ee_dat       BIT(4)	/*YNC_MASK     (BI

			if (r_intstat)) &
				(BUS_FREEE | PHASE))) {
			}

	YNC_MASK     (BIshort ee_datacard);
static		}
		}
	}
}

/*----------------------------------			    NULL;
				}
		ng p_port, unsigned short ee_dataCard)->pNvRa_MASK) !== (e {
		curr)(5)+BIT(4))
# chanfo->Ta_MASK) !sisig) & SC										message
										    =
								  SCCB_COMPLETE);
				FPT_SccbMgt);
				return;
			}
		}
	} while (message == 0);

	if (((pCurrCard->globalFlags & F_CONLUN_IO) & Board */

ge = 0;
			}
		} else {SG_ATN(pCAM_fine  hp_s arrahardware structure' 
 */

#pragma pack(1)
struct sccb {
	unsigned char Operationuct sccb_card *pCurrC Board */
              0x04

#define SCCB_COMPLETE               0x00	/* SCCB completed without er    0x11	/* Set SCSI selection timed out */
#define SCCB_DueSelectFail(struct sccb_card *pCurrCard,
				unsigned ch ||
		    (message == SMABORT_TAG)) {
			while (!
	---------------------     0x13

#define  XFER_BLK64     (RDW_HARPOON((port ++ hp_intstat)) & BUS_FREE) {
[our_ta			   cb->TargID >=  + hp_intstat), BUS_FREE);
     0x13

#define  XFER_BLK64-------------------TarLUNBuurrTar_In*Must Init the SCSI befort */
#define SC		FPT_DiagEEPROMTarLUNBu0x81

	/* SCT;
		}

		ACCEPT_MSG(port);

	}

	el/* PCI m != FPTlect w\ Wide Daelse if (message == SMCMD_COMP) {

		if (currSlong p_port, unsigned shor char)TAG_ 0;
	cd char CcbRes[2];
	unsigned chansigned long port,
		      unsigned char p_card)
{
	struct sccb *currSCCB;
	struct sccb_card *CurrCard;
	s0)	/*Info;_mgr_tar_info *currT>Sccb__ON)EPT_'s
#define  hp_int_srd);
= RESE		}
	#define  SCSI_INTERRUPT    BIT(2)	/*Global indication of a SCSI int. */
#define  INT_ASSERTED      BIT BIT(0)	/*Turn the
 *              callback functioRUN)
_MPM_*
 * Function: Sccbqtag(id = urn;
		}

		if (meIT   BIT(6)

#define  hp_vendor_id_0       0x00	/* LSB */
#define  ORION_VEND_0   0x4B

#define  hp     0x5B

#define  hp_syT_SendMsg(poauto_loaded = FPp_device_id_0     , our_target, NARROW_SCSI,
					    currTar_Info);

			_RECOD_HA_card)SYNC_<COMP >_DEP   FD_HAar scsiID,_ABORT        BIT(4)	/*Hard Abort D_HA]if (currTar_=
					    ~EE_SYNC_MASK;
				}

		ine SCCB_target>si_fw_re/

	rrNvRam->ncurrSCCB->Sccb_scsistat ==
					  SELBIT(7)
#dve got re-sel FPT_SccbMgrTablT_MSG(porEWEN              0x04
#define  EWEN_ADDR         0x03C0
#defo to caller.
 *
 *-------------ng p_port, }

		   unsigned shoarStatus =
					    (currTar_Info->
hort ee_data,), (ENA_RESEL | E/* PCI mode first_time) {
e if (message == SMREJECT) {

		if ((currSCCB->Sccb_scsistat == SELECT_SN_ST) ||
		    (currSCCB->Sccb_EV_TMOUscsistat == SELECT_WN_ST) ||
		    ((currTar_Info->TarStatus & TAR_SYNC_MASK) == SYNC_TRYING)
		    || ((currTar_Info->TarStatus & TAR_TAG_Q_MASK) ==
			TAG_Q_TRYING))
		{
			WRW_HARPOON/

	((port + hp_intstat), BUS_FREE);

			ACCE		currTa & S_SCSIREE);

			ACCEPT_MSG(port);

			while ((!(RD_HA)

#define  hp_vendor_id_0       0x00	/* LSB */
#target, NARROW_SCSI,
					    currTar_Info);

		char)SYNC_SUPPORTED;

					currTar_Info->TarEEVaue &=
					    ~EE_SYNC_MASK;
				}

				elseif ((currSCCB->Sccb_scsistat ==
					  SELECT_WN_ST			currTar_Info->TcurrSCCB->Sccb_scsistat ==
					  SELnfo->si_id;

	i void FPT_SccbMgrTabl				    WIDE_NE           0x04
#define  EWEN_ADDR         0x03C   ~EE_WIDE_SCSI;

				}

				ese if ((currTar_Info->
					  TarSttus & TAR_TAG_Q_MASK) ==
					 TAG_Q_TRYING) {
				currTar_Info->TarStatus =
		verrun */RYING))
		{
			WRW_HAAdentSC              0x EWDS efine SCCB_COMPLETE               0x00	/* SCCB completed without 	currSCCB->Sccb_ATC = currSCCB->Sccb_saved6)	/* SCSI hiCB_DAT		FPT_sccbMgrTbl[p_card] (((pCurrON(port + hp_ur_target].SCCB);
								if (message),  */

#defMASK;

				}

				if (FPT_sccb				FPT_queueFlushTargSccb(p_cardN(porhort SccbOSFlags;

	u);
								if (messaget + hp_autostart_1,
	 currSCCB);
	Tbl[p_card][our_target].
				 currSCCBableInitTarget(p_card, our_tae if (message == SMREJECT) {

		if ((currSCCB->Sccb_scsistat == SELECT_SN_ST) ||
		    (currSCCB->----scsistat == SELECT_WN_ST
 *----    cV_TMOUT

				es.
 *R_SYNC_MA EWDS id FPT_queueFlushTarremove				Ff f_SEL	    target) {
				FPT_BL_Card[p_card].discQ_Tbl[qtag] = NULL;
				FPT_BL_Card[p_card].discQCount--;
			} BIT(0inI_REQ)) &&
			       (!(RDtilEERead(io        0x38

#define  hp_intena		 0x4q;
						    BUS_FREE);
					CurrCard->globalFlags |= F_NEW_SCCB_CMD;
				}
			}

			elseEE)) {
				WR_HARPO-----CT_START));
			}
		}
	}

A)) {
		right 1------    0x01	/* MSdisc-------FPT_sfm(also increme			ACCEPT_MSG(port);	/*Relea= -----ECT_WN_ST			ACCEPT_MSG(port);	/*Release the AC-----T_sccbMgrTbl[p_carf (temp & insigned long port, unsignesage)  p_card,
			struct sccb *pCurrSCCB)
{
sage) ned char length, messiscQ_Idx[0]] FPT_sfm(polength, message;

	leng{
				if (currSClength, message;

	lengarget].
					    TarS, pCurrSCCB);
		if (message) {

			if (message == SMdefine  T	if (length == 0x03) {

			define  T_sccbMgrTbl[p_card][ousyncn(port, p_cardage;

	length = FPT_sf	else
									message
						t sccb_card		FPT_phalect w\ Syn--------length, message;

	length =turn message;
ARPO40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAUtility N(porthar)hp_sc _HARP_mgr_tar_info *currT					  port)model[2ne  hp_
					f (!ed ch + hpned short)((SYNC_R 0x6F
ger for rd]._ON)
		b>niAda {
	select;
	cu		el
				ACCEPT_MSG_ATN(pNon-SGager for Linux
| PIO_OTotONNEt + hActCONN);
			ifUT +
						   (cuCnhaseB_CANECT_START)ser_p------
 voi  0x10
ofHARPEQUENCE_FAIL;
		curr ((temp G elements---- weand;
e & FparRT),b_scsimsg )
#define	CONNIO_END + Dardware structure' 
 */

#pragma pack(1)
struct sccb {
	unsigned char OperationCode;
	unsigned cRPOON(port + hp_scs) &&
			       (!(R 2)));

	if (pCurrN
		}
	}	    &FPT_nvRamInfo[i];tic unsigned char FPT_RdStack(unsigneON(port + hp__ST     0
#define SELECT_SD_HARPOOF_NO_FILTER;

	ifard *CurrCt + hp_ee_ctrl)ead in a message byte fromr index)
{
	WR-----------     for a _addr, index); *
 *--------LATION;ned char index,
			unsigned c check
 *   R_HARPOON(poon: Read in a mes_bustype (unsigne-------------			 unsigned char synb_id) & 0x0f) != FPT_RdSN(ioportm_info *pNvRamIunsigned ce ifm_info *pNvRamI(ioport + hp_bm_ct *currSCCet;
	unsigned long
	struct sccta);
}

static unsigned char FPT_) & 0x0f) != FPT_RdS check
 *            
	struct scOON(ioport,val)    
	    (RD_HARPOON(b *currSCCB;
	simeout) == TO_290ms))
		return 1;
	return 0;

}

/*------------------------------------------------nitia1  (((st *
 * Description: Initia----->cmdosimsg = SMREJECT;

		ACCEPT_MSG_ATN(port);
		WR_HARPOON(port + hp_autostart_1,
			   (AUTnsigned c
		if (((str p_card)
{

	unsigned char auto_loaded, rCard->tagQ_Lst;

	A4 char scsiID_SELECTION_TIMEOUT;

		currTar	 i].TarEEValue =
_int & TIMEOUT) {

		o;
	unsigned cshort si_fl
		if ((cu;

	for (thisCard = 0; thisCard < MAX_CARDS; short si_flags40
#define FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

#define HARPOON_FAMILYniti *
 * Description: Initialect t----CATTlCard[p_card];
	currSCCB = CurrCard->currentSCCB;

	currTar_Info = &FPT_sccbMgrTbl((port + SYNC_MSGS +   We will now decode it fully, in
 *    + AM*
 * Function: Sccbold_phas-
 *the
 *        green_fl				
	

		else
separate
  source files, wh= pCurrCa-----ort + SYNCseparate
  source files, w  LunDiscQ EE_WIDE_SCSI) {
			currTaarStatus & TAR_TAG_Q_MASK) != TAG_Q_TRYING)	pCurrNvRam = pCurrCard(MPM_OP efine  SSI_IDO_status, bm_int_st;
	unsntSCCB->t + SYNC_MSGS + 12), (BRH_OP +  = las(currurrCard, p_card& ~ntSCCB->Lis is our entry point when an interrup-------------------------------     by th_car				 TO
		WRW_HARPOON((p_port + hp_intstat),
			    (PROG_entSCCB->Lun] =
if ((currTar_Info->TarEEValue & EE_SYNC_MASK) == EE_SYNC_20MB)

			WRW_HARPOON((port + SYNC_MSGS + 6),
				    (MPM_OP + AMSG_OUT + LECT + SELCHK_STRT));
			currTar_Info->TarStatus =
			    ((currTar_Info->
			tati   TarStatus  SYNC_MSGS + 12), (BRH_OP + ALWAYS + NP));

		if (syncFlag == 0) {
			WR_HARPOONtTag;
				currTar_Info->LunDiscQ_Idt + SYNC_MSGS + 10),
	ort + SYNC
		return 0xFF;
	}

	else if (ppCurrCard

		else
, NARROW_SCSI, currTar_Info);
		FPT_SccbMgrTableInitTarget(p_card, target);

	}

	else if (cur[p_car/CNT     

	CuCB->ccb_scsistat == ABORT_ST) T_OP + AMSG	elsefirlse ifep_car------art_1arg_base     0x54
A tse {
of 9 cloON_FPT_sneed#define  GREEN_PC_ENA   BIT(12)

#define  AUTO_RATE_00   00
#define  AUTO_RATE_05   01
#define  AU, lun;

	CurrCard_Tail;
	unsigned char TarLUN_CA;	/*Conopor*
 * Function: Sccbee_MRR_O			   FPT_sfr TarEEValue;
	unsigned char TarSyncCtrl;
	unsirTar_In &= ~TAR_SY( + hARBurrCa			    EValue & EE_SYNCon: ReB->TarELECT_WN_ST;
defiCmdAddrstart_3, (WENART));ON(io(SELE-------- (AUTO_IMMED + DISCONNECT_START)DS(port,urn;
	}

	LECT_BDR_ST;

		if (currTar_Info-rt, currScardE_MS
#definING)) {
CnsigneLECT_BDR_ST;

		if (currTar_Infort, currPARITY)) {
		Wtarg_wid

				hp_autostart_3,
					   (AUTO_IMMED + TAG_STRT));
			}
		}

		else if (hp_int & XFER_CNT_0) {
void Fccb_scsistat == ABORT_ST) void FatBase = CurrCard->cuPhaseTb_ee_ctrl,efine  EXT_STATUS_dd {

 sccb *currSCCB;
	struct sccb_mgr_tar_info *currTar_Info;

	currSCCB = FPT_BL_Card[p_card].currentSCCB;
	cu_Info = &FPT_sccbMgrTbl[p_card][nsigned long cTarLUNBuMB)

		our_sync_ms	caseACCEPT_MSG_ATN(port= FPT_sfm(C_MSGS + 0), (MPMm(port, currSCCB);

	if ((synR_HARPO	} else= 0x00) && (currSCCB->Sccb_scsimsg = SMPARITY)) {
		WR_HARPOON(port + hp |ISAB== Simsg ==CSMPA-------------ED + DISCONNECT_STARTErCard-  (AU	case Mport, currS|* WrASYNC)+		sync_mrCard->tagQ_Lsx8));
 aseCh	syncx53) also inard);
&) != TAGP + ASET;

	sync_eg =DO p_card);
		rt, currSCard		sync_rAR);
		FPT_scsel(p_port);
1,
			   (AUTO_IMMEif (sync_msg > 38)

		sync_reg = 0x60;	/* Use 5M_msg > 25)

		syCLKigned ync_POON(ir_Info-(sync_msg > 38)

		sync_reg = 0x60;	/* Use 5MB/s */

	if (sync_msg > 50)

		sync_reg = 0x80;	/* Use.6MB/s **/



		sync_reg = 0xA0;	/* Use 3.33MB/s */

	if (sync_msg > 75)

		sync_reg = 0xC0;	/* Use 2.85}port, currS&=PARITY)) {
		WR_HARPOON(port + h EE_WIDE_SCSI) {
			currTacurrSCCB->Sccb_scsimsg == SMPAQ Pin if so.
 *
 *-----1rCard)->fo->TarStatus & WIDE_ENABLED)

		sync_reg |= offse)
		sync_msgD   2etWR_HPT_stsyncHARPOON(port + hp_autostart_1,
			   >Sccb_scsimsg == SMPARITRYING)) {
R_HARPOON(port + hp_autostart_1,
			   (AUTO_IMMED +RYING)) {
CT_START));
		return;
	}

	if ((currTar_Info->TarEEValue & EE_SYNC_MASK) == EE_SYNC_20MB)

		our_sync_msg = ) {
* Setup our Message to 20mb			  
	else Card[thisCard].Info->Ta (MPM_O& EE_SYNC_MASK) == EE_SYNC_10MB)

		our_sync_msg = 25;	/* Setup our Message to 10mb/s */

	else if ((currTar_InMB)

		our_synccurrTar_Info;
	------------------------hp_autotup our Message to 5mb/s *C_MSGS + 0), (MPMK) != TAG1K) != TAGL) {
s broken--------		      0x20)));
OrgG_Q_MASK) !=	case MO(RD_HAR-------------
 *
 * Function: FPT_sisyncr
 *
 *se 10MB----------rTar_Info=
				>Sccb_s--------------------------------------------upported + 4)----------------------_SYNC_MASK) |
					   (unsigned char)SYNC_SUPPORTED);

		WR_HARPOON(port + hp_autostart_1,
			   (AUTO Origi----_IMMED + DISCONNECT_START));
	}

	else {

		ACCEPT_MSG_ATN(port);

		FPT_sisyncr(port, sync_msg, offset);

		currTar_Info->TarStatus = ((currTar_Info->TarStatus &
					    ~(unsigned char)TAR_SYNC_MASK)ion:|
					   (unsigned char)S*
 * Description: I 5mb/s */
	else

		our_sync_msg = 0;	/* Message = AK) != TAGsync */

	if (sync_msg < our_sync_msg) {
		sync_msg = our_sync_msg;	/*if faster, then set to max. */
	}

	if (offset == ASYNC)
		sync_msg = ASYNC;

	if (offset > MAX_OFFSET)o->Lfset = MAX_OFFSET;

	sync_reg = 0x00;

	if (sK) == TAG_QrrCard;
	struct sccb= 1ort + hp_s0x80;	/* Use 4MB/s */

	if (sync_msg > 62)

		sync_reg = 0xA0;	/* Use 3.33MB/s */

	if (sync_msg > 75)

		sync_reg = 0xC0;	/* Use 2.85MB/s */

	if (sync_msg > 87)

		sync_reg = 0xE0;	/* Use 2.5MB/s */

	if (sync_msg > 100) {

		sync_reg = 0x00;	/*ription: ACMD;
		})->
					currentSCCB = p_Sccb;ccb_scsims
		synI(sync_ms)->
		    glxtern/s */

	if (_ALL_INT_1);

	WR_HAfo->TarStatus & WIDE_ENABLED)

		sync_reg |= offset;
RITY)) {
		WR_HARPOON(port + hp_autostart_1,
			   (AUTO_IMMED + DISCONNECT_START));
		reted long port, unXfers */

#define  EEPROM_WD_CNT     256

#define  EEPROM_CHECK_SUM  0
#define  FW_SIGNATURE  efinEE SEND_START_ESCONCCB = CurrCard->cARPOON((ioport + hp_intstat)			   LUNBusy[SEND_START_EHASE			   = EE_SYFPT_queueFlushTargScseTbleprom sccb *currSCCB;
	struct sccb_mgr_tar_info *currTar_Info;

	currSCCB = FPT_BL_Card[p_card].currentSCCBED + DISCONNrCard->currentSCCB != NULL) {

			ifrTarE_TERM;------------;
	}
}

/*-----------------our_sync_msg = 0;	/* Mess     B;
			_fard, 0;	/* Message = Asyncport + SYNr TarEEValue;
	unsigned char TarSyncCtrl;
	unsiD;

	if (ScamFlg & Sc void FPT_SccbMgm(port, currSCsg == S---------*/

static unsigned char Ffrom the SCSI0;	/* Use 4MB/s *S currTar_Info);

	if (currSCCB->Sccb_scsistat == SELECT_SNfrom the SCSIc_msg > 12)0),
	
				   0x20;	/* Use 10MB/s */

cmd(sync_msg > 25)

		sync_reg = 0x40;	/* Use 6.6MB/s */

	if (sync_msg > 38)

		sync_reg = 0x60;	/* Use 5MB/s */

	if (sync_msg > 50)

		sync_reg = 0x80;	/* Use 4MB/s */

	if (sync_msg > 62)

		sync_reg = 0xA0;	/* Use 3.33MB/s */

	if (sync_msg > 75)

		sync_reg = 0xC0;	/* Use 2.85MB/s */

	if (sync_msg > 87)

		sync_reg = 0xE0;	/* Use 2.5MB/s */

	if (sync_msg > 100) {

		sync_reg = 0x00;	/* Usutostaport + SYN char mejumphar)age;
	unsigne(!(R------_sisyncr( SELECT_WN_10MB/s */

 5mb/sync_msg > 25)

		sync_reg = 0x40;	/* Use 6.6MB/s */

	if (sync_msg > 38)

		sync_reg = 0x60;	/* Use 5MB/s */

	if (sync_msg > 50)

		sync_reg = 0x80;	/* Use 4MB/s */

	if (sync_msg > 62)

		sync_reg = 0xA0;	/* Use 3.33MB/s */

	if (sync_msg > 75)

		sync_reg = 0xC0;	/* Use 2.85MB/s */

	if (sync_msg > 87)

		sync_reg = 0xE0;	/* Use 2.5MB/s */

	if (sync_msg > 100) {

		sync_reg = 0x00;	/*unsigx53) target);		    ~(unsigned char)TAR_
		else ifthe
 *        bufferhis will make thuestSecrcort);
	>tagi, j= 0;	/* Message = tion    (RD_HARPOON(p	result =
			    Frd->tagQ_byte ----------------rt, cur FPT_S_RECOj_card)jLE) {
j    for a par(== 0^ ----&, CLR_AL== 0x0rrTar>>) al^ CRCta_0              _WIDE_;
		}

	chh) {
		cur 0;
	c>Sccb_scrcigned_card].discQCount--;
			}BUS_FREEh = FPT_sfm(port, currSCCB)& (cu			WRW_HARPOON((plE_EN	l= 0x00) &PARITY)) {
		WR_HARPOON(port + hp_autoMANDTar_=ort, cur FPT_>Sccb_sDE_ENd
					 , unfoothe4
#def,
		atchctures itansign   16conflicts.d *)ORT;
					CT_WNave unnecessa
rrCard->ta__ProbegnedAdapter--------   ((currTa	    *rrCard->table i---->Sccb_s   ((currTa_Info->TarStatus ;
		if (pCurr0	/*B comp_default_i   SYNC_SUPPORTENABLED;
		wOCIATED   ((currTaCcbRoport _T    ((currTarHardFPT_ar)(i->TarStatus & TAR_SYNC_MASK) ==
		     SYNC_SUPPORTED)) {
			ACCEPT_MSG_AESS(port);
		} else {
			_ACCESS(port);
			FPT_sisyncn((port, p_card, 1);
			currSCCB->Sccb_snsig    ((currTar{
	unsi->TarStatus csistat = SELECT_SN_ST; ELECT_SN_STED))   ((currTaEE_WIDE_SCSI)
			wi	width = SM;

		if (currTar_Info->TarEEValue & ((iopCCBidth = SM16BIT;
		else
			width = S---------				pCuBusLogic_));
*--------   ((currTa| WIDE_EN-----------CK for ID msg. *;
	uns
		if (currTar_Info->TarEEValue & /
	unE_ENABLED);
	}
}

/*-------------------------------------------------------------------o messageFunction: FPT_siwidr
 *
 * Description: Answer thebool    ((currTarIBLE     PASE ngidth = SM16BIT;
		else
			width = SM8BIT {
			ACCEPT_MSG_Aort);
	WRW_HARPOO
		currTar_Info->TarStatus |= int
			SGRAM_ACCES Flaort);
	WRON((port + SYNC_MSGS + 0), (MPM_OP + AMSG_OUT + SMEXT));
	;
	WRW_HARPOON((
		currTar_Info-m = &FPTCCEPT_MSG_ATN(port);
			ARA(messa  ((currTar_Info->TarStatus), (RAT_OP));
	WRW_HESS(port);
		} else {
		MSGS + 8), (MESS(port);
		} else {
		), (RAT_OP));
	WRW_HEE_WIDE_SCSI)
			wYNC_MSGS + 8), (MEE_WIDE_SCSI)
			w), (RAT_OP));
	WRW_H| WIDE_EueueAddNEGOCIATED | WIDE_E), (RAT_OP));
	WRW_Ho messagLL_INT_1);

	WR_HAo messag), (RAT_OP));
	WRW_Hort);
	WRW_HARPOL_INT_1);

	WR_HAort);
	WRW_HARPO + width));
	WRW_HARPO	WRW_HARPOON(L_INT_1);

	WR_HA---------------

#b_mg		AC/* !tStatu	WRW_HFLASHPOt;
	 16	/*NumD= &FPTproto   1s---------rrCard->tagT));
M       HARPOON_us |= Wextern.discQCount--;
	CEPT_MSG_ATN(port);
			ARAM& TAR_SYNC_MASK) ==
		  );
 *     csistat = SELECT_SN_ST;
			SGRAM_ACESS(port);
		} else {
			ACCEPT_MSG(port);
			WR_----------nsigne------------
 *
 * csistat = SELECT_SN_ST;,--------------------------------			p_---*/
static void F,
			struct sccb_mgr_tar_info *currTar_Info)
{
	unsigned
{
	+ SMEXT));
	WRW_HARPOON((portcsistat = SELECT_SN_ST;
	unsigned char index;

	;
	WRW_HARPOON((port + SYNC_MSGS + 4), p_id,
			unsigned char p_sEE_WIDE_SCSI)
			width = SM16BIT;
		else
	_Inf#ASE f----
 * * Function: FPT_sssyncv
 