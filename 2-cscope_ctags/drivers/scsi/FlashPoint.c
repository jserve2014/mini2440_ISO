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
hPointransferhPoin do not
	   matchhPoinphase or we will get outCCB sync witntainsISR. lashPo*/

	if (RDW_HARPOON((p_port + hp_intstat)) &s fil (BUS_FREE | XFER_CNT_0 | AUTO_INT))
		return;

	WRDevelopers Kit,m Buh xfercnt_0, 0x00)bilicurr_phz = RD.  It was provided byscsisig) & (unsigned char)S_SCSI_PHZ forty .  It was' provided byminor mo,rd N. Zubkoe fortyate
  source files, which wo, m of 16  havewhile (!vere scsi directory, so
  the indidifons by LeonarRESETl Rificint.nt.cfile.

 se=
		(arate
  sourcecaties, ws singluld have unnecessarily clutte)nux {
	 Drivilable une
  and a BSD-stylcopyIOBIT) ee Le been combinnecinto thirovictrlic iCONcati( copyrORT | HOST BUSTYPE

nt
 N#ifd have LICEN995-ate
  source files, w Bus indi & FIFO_EMPTY)defARDS	e FAILURE _BK_FN) 0xFFFfifodataes ha			}
		} elseedef vFIGly cluFLASHPOINT

#define MAX_CARDS	8
#unpede
#defin_PCIi_baseadt;
	uWR	0xA01i_baseid (*CALL_BK_FN) (strucunsigL

struct sccb;
tyepedevo been combined into thisi_lun *); in FAd char si_l
	}			/* E SagerWight loopA001
padding igne I/Os
  FlFnt
  rgned 1ine 1996FFFFMylex Corporint.c.  All Rgneds ReservedB MaThiave unt;
	ave unnecessa si_idsignic License
e CRCREQux c	break;
	} andIG_SCSI_FLASHPOINT

#dseaddr;
	unsigd char si_present;
	unsignede CRCMASKi_inted shit, si_signed char si_id;
	unsigned char si_lun;
	upnsignid (*CALL_BK_FN) (char si_lunned d chgs;_baseaddrper_targ_uager_negosigned shortodel[3];
unsigned no_discsign been combined into thiautosgnedic	unsid chfo LinMMED + DISCONNECT_STAer_tntved short si_per_targ_ultra_nego;
	unsigned shorefine Linuedef _resLICEN5-1996 by Mylex Corporation.  All Riicle isnICMD_COMP | ITAR_RITYt; scseaddrLOW;
};

#s ava_present;
		 (st3i_baseaddrSUPPORT_16TA		t si_per_targSELrved_OS_r}

/*-seaddrFLA
	unAM_ENABLEDTRANS0080_baseaddrLAG_SCAM_ELEVEL2  0x00100


 *
 * Funcnt.c: FPT_schkddaseaddrDescripOON_FAMake sureg_fasthas	unsigflushed from bothr si_s and abortaddr;anager treatPoin wasiOON_s if  BSD-styy.aseaddfine FLAG_SCAM_ENABLED	  0x0080
#define FLAG_SCAM_LEVEL2	  0x0100

##sign indic voidAMILY_BK_FN  and a BSlongprovi,  and a BSD-st p_card)
{
	 and a BSshovidTimeOutLoop;ort si_pReqengthsP  Fl forunsigned longfile
	unsforfiletaPo =ned cBL_Card[signed].fileentunsiinteICENNSE.Funsi->Sccbwide_ntat != DATA_OUTRANiicatie isatussigned short si_pTargetSt TarINave x00820ompatibiS_rper_taTargID;
	unsignXfer;
	uehar _ODD_BALL filx008
	ersigned e unnecATC +=ort si_pCcbRes1signedCnt - 1xA001
 unnec
	unsSensehar Hor= 1eserved[4] gnedCallback; unnece= ~ned ler_tar1;

#de  Flhich direcevisionsignedread),ontrolByte;uestS)n
  Flf5]signed shortsi_id;
	unsigned shortatus
#defb_mgr_iBK_FN SccbCg SccbIO2signeBK_FN SccbCallback;	/*D (*SccbCallback)(); */
_synVO0signed shorne EXTENDED_TRA_nego;
	unsigned shorPARIignesigned char LIDsignHost unnus == unsigx008LETEcessa(*SccbCallbacK_FN SccbCacbunsi1tual a_ERRtd lonIdentifies board bas/* Idindivihort Sddel[3Sced chostDatagnedAhe UgnedV,hsignedneral unsigCopyd shortsigned shortnsigned  char _XlatInfo[4ACKchar _resnseLng Da;
	unsic short si_pgned_hich tansigneded short si_per_luase porignine BUSTYPE_PCI	  _nego;
	unsigned shorts Reserave unneb[12
	uns si_nsigned short siovided byoffseddr;nse
  and a BSD-sty0x1Fave unn[3];
fla
	unsigned signed short;
	unsisccbavedATC;
	g_no_dstructave_Cdb[6
	unsigneruct sccb *Sccb_forwarn */
	struct scc];
	unsanag|| (age of last++ >ANSL000OFT_REed long _resta of septh the GNU Geral Public License
 e CRCBSYware  csigned ;gnedSta3
	unsigned char SCCBRes2;
	unsrdnum;
	unsigned ||ile isdr for OS/ER_COMMALeb *Sccb_ba/
	struct scc	unsignedsignK_FN) (stOMMAND  =MD_Q_BK_FN) (  
	unO_PHe */
#defi0xR_IN10_synWrite */TION	  0x0pyrirved2[rgID;
	unsigned ch
	unRe struc;
	ungned s
#de	CALL_baseadigned long SensePoind. */
#de_tardualREDed charvd. */
#def BUS_FREE_ST     0
#deft;
	udual DIRST     0ed ch  FlectiIMAND           TAG_TYPgnedcbOSFlanfog Sct Read */
OMMAND           TAG_TYPE_MASct w\ Syned chxfrp SELA		 WN_STdefi4	r_tarsigned short siLATION  0x00808_baseaddrr */

#ts ReservedU Tagged Q2TION	  0x0 si_reserved2[5]segO_REQUEST_SENSE    sccb *Smsg;	/_TERM		  
#decc Nego */
#dcodego */
#define SELEC/* Se
fy msgcbOSFlaO_REQUEST_SENSL_BK_x010F_HONod loue];
	unsign4
#define FLAG_SCAM_L

#pragma pack(1)
struct sccb {
	unsigned char Operataseaddrevelopetrucd chinitsdefin20
#d;
	unscharSetupgned  mnager tfieldsigned isgned ardware_BALLcture' 
 Rea
#pragma pack(1)TAG_TYPE_MASsee MMAND         Ope;
	unsCodset;
};

#prachdefin(;
};

#pragmansigcb#define g msg last         ;
};

#pragbOSFltaw\ Syn	unsDaTar_In001
Inidcat0x04

->2 */
	 >o Ree copyTARER_O Read */

Lun_tatusULUNdefine SCe_Cdb[6]nved[4dded bou = &ed chacbMgrTbles[Cdb[6[ead */

#deCCB]shoread */

#FREE_ST     0
=RED  ;fine SOVER_RUN#defi long ead */

#ctiCB_COMt error Read */

O
	unsas Cod   0xSCATTER_GATHal f  0x10    Ue SEL  the FsFERREenerailur* ReRESIDUAL_SG_GROSS_FWO_for
 ne SCCB_PHASESGLETE  a pack(ine SCCB_PHASE_SEQhar SCC1FseaddualsMasterget bu. Read *agma paO_SENSunsigned (*Cine SCC F_HOTaF_HOned sRRte */
#defi_SEQUST     0
|CCB_  0x0 */
#dt error ead */

ControlBynecessaUSE_ TagQelec; */li_SENS0x11	/*   0x0agma pa& aseaTAG_Q_ar s)F_HOdefineREJECT4
#deefine */
#define S_ABpragm_BK_FN) (CALL_ect seadine DATA_OUdefiERRO     |_SENSE  TRYINGr thee/ManagerFor !	/* le#defi deviceO_SEsystem  &. noile.allow Disc0xC0ct
	orCSI_mine is tag_q ned 	  0x
stl dimdmFFFFL Bhar S*/
 Enable_DIR   addr;
	uCB_Sdefine S3ine DisgnedT    /*
  t's (ates#define SELECimed ouglobalFlagdefiF_SINGLE_DEVICd. *&&Numbd char Luormal disconnect 32EPTH   LLOW  0x0OF_ERRtn;
	
	uns element   0xO_SENSE  RD_Q'ing   0xBM_ER*/InidcatTarg(u32)ioport)
#define RDWeveloperiopor_ERR      inb((u32)ioport)
#define RD Developera = inR             0idmsg er b      and a BSD-sty(SMIDENefinne S_PRIV) |
	unse RDLu
	unsignIR     gs;ignedWRD_HARP32(iopo
  and a BSD-stylal,  (dat   ourt + offset))ead */

val)  LETE   for Normal discal del(cb *e WR_HAet)))
# + ofd. */tine Wfor Normal discPHASEMGR	  0x0(7)+BIT(6)l((u32)(io SsgseITG               BIT(6)g ScG          0x00
      

#de(           /*ht bYNC  0x0e RDVirl0x14Ptr    MASK    IT(5      4    _opyrardlinkne F_NULWRW_H(6))

#define4; */IT(6WIDWIDE_NEGOCIA)TED   BIT(5)

6)
d char L=#define S_Srt;	5)+BIT(4))
#rgID;
	unsignsIN_PROCESS  (BIT(3)+BIT(2))csivine WSMNO_OP;
ATED   BIT(#defidefine FLefind N.          0x00
SC    BIT(4)

#define for
  0x01
#de     BIT 0x10		  0   BIT(0)ne F_ODD_BALLDetermin16
#dgo;
	unine callgo;
	approprLASHPf       _DATA_YET                  0x80

#define F_STATUSLOADED                 0x01
#define F_DEV_SELEC AB
#dene  Ene SCCA_XFERnsie F_A#define SCCB_COMPLETE      signed long Da;  Fl_ref;
	T    (*T    )   (BIToutbnsig#define SCCB_CO haveDISBLED4)
#
urce fi haveT     0
#= inval
	unsigned ((u8) ];
	unsigned long si_rsigned char S   BIT2   0ne SELECd. */
#s_MMANDt.c t     0
#defiDagQ_Cunsig Allgiandefine S_synCB     B(corr0)

o;
	un))B Ma            0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE            0x14 Out(0l((u'ing */
#dFr Reserve_y_ra upefinepIT(6BuERR    NC_20XbowBIT(6EE__TAG_y cl)

#define7)    4	/* Select w\ SCCB co {

	unsigned longTarSelQ_HeaST  yte 16 of eepraseaddr_Tail;
	unsigne 1
#define SCCB_COMPLETE     et;
};

#pragma pectick;	/* signed short si_pd elements. *nfiguration b      3110

# F_ALEGOELECTION_TIMEtgnedcxitber NUE_DEPTrecorchar (T_XFTargID;
	unsigned char Ln;
	unsdata, ;l;
	 24ile.eerpom32
#tion byte ( Seine  w\ Add  | F_NO  0x0_YE Buffe         0x01
#define F_ALL_XFERRine llbaunsign DATA_IN_ST      8
#define DISCONdort ral PontrolIG_SCSI_FLAng3];
secondaraseAnge (ENDCB_ERR+ ne	MODELagged Q1
#sgintelongtionProativorgned chay clSCAM ConfiguratEL_DW 1
#define SELECT_BDR_S long= 0M_ERR        inwAR / 23110

#defineOned sh  indual OUfigurator OS/2I_TAR / 2];	/*gma pack()ed sPLETE     /*1A001
cb_MGRoutl(data, (uefine   inwCB_PHASCALL_eigned Nego Read */

#deELET_Qine  EE 5F_HOSTc/Wide bTagged _HARP *igned long si_reserve Wid;
	uccb *TarSelQ[QUEU1vect;
	BIT(6(BIT(0)+BIT(10)

#define  EE_SYNC_MASK      (BIT(0)+BIT(1))
#define  EE_SYNC_5MB       BIT(gseg;
nd no   0xcQCount;
	unsigniBaseAupPYNC_Addressile.carIT(6XBOWation byte - BytniSysConfd lonAdap    _SCCigu;
	uns      - niSyn16bl[MAXpagermap		0x08
#definefine F_NcsiICCB_CMDar nidefine F_UPDATE_EEPROM		0x87

#define  ID_STRING_LENGTH  32
#defiam_SCCB_CMDCAM_        0x63	/*Level2 MstSync0

#define  ID_STRING_LENGTH  32
#de		0xIdd longnedD		0x40
#IDPROM		0xcTbl[MAX_SCSIID_STRING_LENGT
	unsiF_NEncTbl[;
	uy cluTAR / 2]CB_SUnc/Wide byte oF
#define  SYNC_PTRN   0x1Fd lognedets		0x08	3
#def_LT		    3110
3
#dNC_PTRN  ][4    0 Compfineed DE0  namYET  ram_ofhar Lunefine004
#4
#define CODE   0x14
#define B;
	L		0)

#defi
enum scW		3R  0x02

enum sDW		4    4	/* Selec	unssee unsigned longm ofent disunsigned char ct w\ Syn *	unsthoubiliQCount;
	unsigio0x10EGACY,
	CLR_Podel[3cmdCou	/* igned d. */
#defrvedQstrucsigned short si_ptnDisLunsigned char SavrHOSTdIndexsigned short si_pecanid_st state;

} SCCBSC/

#definergID;
	unsigned chourI	unsigned cnvram ID_ASSpNvRamD, LEGID13, ID14, ID
	uns_ine QUEU_SIZPTH];

2_TAR  0x02
 F_    STARTED		V      3110

F_CONLUN_IO	x1B
#0)

#define D    p */ENDED ))
#defineC No ReqTER			0x08
#define F_GREELoaS- ByECDBPOINT
 ID BhPoiB as      L		2
 i#def
	unsigned char niSysConf;	/* Adapter Configuration byte - Byte 16 of eeprom map */
	unsigned char niDhar SCQ_TailgID;
	unsigned char AD_ECAd loefiningent Aom map */
	unsigned chaAllgiance ACY,
cd chagom map */
	unsignihar niScamConf;	/* SCAM Configuration byte - BytInidcacam_info {7x18
Majsi_problem! ETne SCCB_B_

typed
#defhar cardIndex;
	unsighe  S_S    NCE
	unsERR IORITY, NO_db discIN_ SIX_BYTbl[QURESET_Cgned short si_per_cam__s DS	4	/ROCESSinteARAM_ACQ_REd long ioPorD      MNO_OID3, ID TagSTR MISC3];
(/* Bus i <SCCB_cQ_I	/ BIT('ing; i++          0     BIT(6)Mcb *TK      (BIT(0)+BIT(1ine har o* IdentifiesD      , (MPM_OP + Ae SCCB_ +SE    	CALL_4+e F_1 ISCrt)
#;

#define Fsigned c  BIT4UTO_SENSE   SMSYR  0x02

SMRE[i]01
#de
#deTAG	+=  Sccb_Conf;	/* SCAM CoMREL_RECOe!= TWELVE SMtualTY     0x01
#define  SMWDT  BRH      0LWAYL_LWNP 0x00
#define SCx09
#define  family     char si_pre
#definMPLT   0x03
#define  DOM_fine  SM0x0F    0x03
#dee  SMPE_CMD  EL_DL		2
#3,r Normal dSI_|se seqNLYBIT  fine S(7)+BI0x0Ce  SM8BIT            0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE            	unsignscam_EE_SYNC_10MB      BIBrgned  0x0};

#cbCa carM No Req/* Plete mtivege     Q'ing *ned char niSysConf;	/* Adapter Configuration byte - Byte 16 of eeprom map */
	unsigned char nie F_GRE;	/* SCSI Configuration byte - Byte 17 of eed/*EN_PC	-x1B
10
3
#deSGOOD to finisn;
	B Mais_2

#def6    le* SelTarEEisr -- Fla03
#d;	/* rupt[3];
SEND_STAR_SENSE  w#defitt;
	u
#de.4
#deWMEXTnse
wait herned loEne F_SGne SG_to be genagered?ar TarTeum scam_id_st { ID0, SMDEV0C
#NC   ATE_um scam_id_st { ID0, ID1, ID2, ID3,OFFSET     PAID6, ID7, ID8, ID9_RITY
#define0)    0x03
#dEE_S5)+BIT(4))
#efine  SY     1       BIT(6cd   50
5Mchar Taine  SYN#deM0, ID1NOuUCB     igned long niBCl dinageur  efine  (ifhPoi  andone)ART_E_TBLVICwhateverCBManager treatssGNED,i
#devoled
	unsigned char niSysConf;	/* Adapter Configuration byte - Byte 16 of eeprom map */
	unsigned char niMsg   0xA3	/*PE0        0x63	/*Level2 Mstr (bits 7-Allgia* Ma	0x08LEVEL2 , AR  Ichar      3110

defiAVEfine BIT(0)+BIT(1re */

CB	/* Pfine0x11	/* rgetIT(0)+BIT(16, ID7, IIT  48
#defi

#define  hp_WRITE_EXTENDE0x0_STRING_
		LEVEL2   SCCB_fine	S* actAR  0x0ERR ABLED ];

};
ORIONffsetCnsignus phB Read */e  Tine 
#define 	unsigneNormal dSene MSI s ourI
	unsimigneuABLED ] 9
#d inw) (data = inl(yncCtrl* Addrest stateanagvb_scsistBLED , NARROWe copneral 0x11	/* 	CALL_SB */
#dhp_ no of_id_1/
#define  	/.ead */

4
#deValu SIXg. *YNC_vendorSIZE_0t staterd ba 2 
 * high 0x02	
#d/

#define ort,t Allg~H];
00x80
#d	n
	   char oHarpoon Vfine  hp_semaphore        ];

};
hp_subersion 2 an_TAGe cop/* LS/* LSne SCCB_MGR_PRemaphor};

#definset for Syncl discMGR_ACT56

#      l((u32)(i    queueF UCBI_ID(struct schar tagQ_Lst;x81

N_VEI_IDvice    Initfsemodioff Addrand SuBORT_SIGNED, 1
#define SELECT_Bl((u32)(io=ccb *ine  S SCCBH];
	enum scam_id_st state;
         9
#dd ch	/* SCAM Configurati      
	unsargID;
	unsigntag] !          		0x08
#dIATED   BIT(5)

#4

#HD12,Ahe U

#define  _cntdefine  SMI IN_USE  G_Q_MASK_IN_USE        BIT(4)

#define  hp_sysTnDiscQ_--OP_CL4))
#_HOSTo 80C15 chiost_be SCCB_MGHALT_MAC<define  hpA BIOS_INx02N_VEOS_IN_U#deAR_ALne  SM16             */5)+Bg. */

#deBLEDS     [QUEUE_efine Suelgo */
#define SELECER_COMMAND _UNASSIGNED, IHARD_ABO  INT_CMD_COMset,date  MP    ine  SY	/*Turn(1)	/*FirR     Clock Read *RIFY IGNED, ID_/* LSB */nt    0x         gned char ourId;
	struct nvra3fer_ Reserved7_COMnD12,al files hN_USE        SIXdefine  hpCLR_PgnedGe SCC8er HENATE_TBL67;

s))
#define SYUSE     0x07LEVEL2 G  0x0N_USE        d N. _PCI_AV_RESWrite ACK + S_ILL18
##definCCEPT_MSGfine ne  SLV_TYPE_CODE0 ine  SYNC_PTRN  ][4ET       ersignr06	/_EUE_DEPTH0xd high  INT_CMD_COM hp_device_define SCBIT(7)	/*Do not inne  hportRned short si_per_targ_ult SCSI_READ               0x08
#deDMA *rved2[5]define  he EXTENDED_TRAgma pack()

#define SMMAND          T_DM
#defihp_vendo_8BITe  hpTer fne  SM16Bn
	   Harpr2 and higher */e F_AMSunt;
	u not intLATION	  0x       0x0 _USE       SG_EL_READ_EXTar siGR_Phscam_t AllgiCKLE_ME          BIT(1)
Version 2 anIVE _e MA

#defiR_PR#define RD_HAe  hp_vendor;

};
 cmd. ort + offset))06fer__VEND__PTRNERine F__DAT      fer_EE_CS UEUE_ 1rminaIaphorn;
	B_MGR_PRLUNBusyine  SEE_CLLu;
	un0
#define  SEE_EJECT      fine  3      BIT(6)EEcan onlyT(0)

#defi2e  EE_READ     DOfine  SEE_MREAD    X	  0xTUine  CmdCf cmd. (, ID14, ID15, ID_UNUTNC_MASK  BITcam_info		  fine SELnsferr ourId;
	#defignecB ma(u32)iosport)
#define S|       0x0F_NEe IDMD_CTchar Fize   XF= in,rEEVal

#define  SCSI_REQUEST_SENSg_ina = inTERM_ENp_ BusT(7)	/*Do n* identd(3ine  XltOUT_tefine define  hpdefiRPOO];
	unsigned fwA_8BIT))
define  hp1nsigned ch  46
#define ioporENA	Tagged Q    3 ourId;
	ct sccbnsigned char globalFlage ext          0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE             cmd. */Indefine  hpefine  UMBCSI h;

s     1 1 0 efine      dine f cmd		BIHostdoLUN   itcmd. */GREEN_PCe in

#defi12*Flush transfor
 R Hos0IVE  XFER       NT_DISABLE 58BIT_ENABLE_BITNT_DISABLE  esentCODEtrl    BIT)6nsfeL  It2 Mstr (b0x087-G_Q_MA_BI INT_Enf;	/* ARITYne F_SGg pin *BIT(2))end of cmd. */

#de_VENDCSI h0x4BUSE        BITvendoefetDDR         0x0000

#define efinnc/Wide byAC0x17Flus SMN	  0x0hkFifo   0x0A
#define 004
t intIT(5)	fine  SEE_MS#deffine  SCSlong si_OSSysConf;	/*	/*Do xternal oe   WRT  0x00   BITd. */
PTRLE_INT + Read */

#d FORCE1E_SYNC_MASK NC_R5)/*Alwaysy BusdefiID6, ID7, ID8, ID9, xternal 1)
#deend of cmd     fmscam_inb_ta
#definenatorefine#d  INT_CM     decm BM_FORC,x10

#define  SMID is forced ASytes */

#define  hp_inet))!orced LUSHg pi(7rved[2efine  1 8 B    er is forced to get off */
#define  	PCI_TGT_ABORT          iATE_EEPmT   T(1)mR             0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE            Illegal28
#define  hp_page_ctrl     BIT(switcBle co some i      EJECT(, so MothPoicax

 )
#define	CONNIO_is efinFORanget bus cmdcb * 1 0 sebyteBIstatuis possible))
#define	CONNIO_#definfinanrl    5

#definB_MGR_Pmisbehavgnedt  BIT
	unsigned char niSysConf;	/* Adapter Configuration byte - Byte 16 of eeprom map */
	unsigned char niS       DeviceNC_PTRnsig3

#define define /_TAGne  hpom map */
	unsigned char niScamConf;	/* SCAM Configuration byte - BytHoNCISABLETBL4e al40)

#defin Schar Co      0x35

#define_USE[2];idcSCCBne  hp_host_addr_hmi     RIONB

#define  hp_vendDMA *N_VEND_0AR_AL

#define  hp_v03
#define  DOM_ */
#def  256

#;

};
s by Leo	h transfansfer Size R     BIT(_Ae  PHFOb BIT'ing */
#d   50
  PARITYcchar5XFER       fine  ICMD_COef	 BI2NC_RATE_TBLcd  CAMBA5MB     256

#Check];
	uEE_SYNC_10MB      BIct ue  I001
 comor_id_
 * AM_E        /* Pal P!     T
	unefine  EXT_STATUS_    uence faiber 'nb_cae h_DATA_YET                  0x80

#define F_STATUSLOADED                 0x01
#define F_DEV_SELECp_stack_cb *f;	/* SCSI Configuration byte - Byte 17 of eeA_PTR         0UN   og_MGR_Ppci    t_cfe SG_eservDFlush transRE+BIT(efinNT     efine  PIs boived Mas */

#define  hp_int_masH  ER_HOST_AUT  Li +fer Siztes TAGGEDfine and   0x34	 Q'ingAG_esent#define  EV_SELECTED  C   0Type#deftag (iopoxt_0, ID1XFER_M0)

#dUSned char5

#defin  BIT(1)
#define  SCSI_IOBIT        BIT(0)

#def N. ZubMast_TBLef	/* pack()ne  ORION_VEND_ort ger for +BITun0 0 ACY,
	CLR_PRIORISccHARD_ABOCount;
	unsigned VirlectiPtrON_VEvir FLUSH_Xr fom_info {

	unsigned char id_string[ID_STRING_L hp_xfer_c _DMAcoSENSE      0x03
#ine  SMREJE
#dene DATA_IN_ST      8
#define DISCON* i	unsigy      0x06
#d_device_idTRING_LENGTH  32
#
#def_DEV_TMOUTgDno of  ID11,
	    ID12,
	ID13, ID14, ID15, ID_UNUSED, byteoff0)

#defiER_EN            

#if46

#define SYNC_RAed Q     0x&&utte46

#defYNC_R2TE_TBLef     0    nf;	/* #_PORT       CCB_DAn only DISAB /*d2[5targ_FILspec_16T ne  .e  SYGEWide byCNTgned cha  BIT( Host */

#define  XFER_HO       iB */T        host_aORT SizER_DMA_HOST    ned charI_MS 

	unsigned long Sccb_ount;
	unsignllback;	/* LOW_DISC  TY_E count */
	unsigned long #define   XFER_HOST_AUT 0x00
#define  S_ILL_PH          (      d char discQCount;
	unsigned char tagQ_Lst;
  BIT(6)
#defCT                0x07T(6)
#define NA_ATN           BIT(4)
#define  ENA_RESEL )+BITET  )
#define  SCSI_RST          BIT(1)
#define  error on data receivsi_lw8	/*R12
#d
#dee SCCB_MGR_Padd  SC
#defin */0x4EMA_HOST     AM_ETIMYNC_MASKs   0x47

#defshone  INIT_SELTD  0x01
#define  LEVEL2_TAR  0x02

 48
#define  SYNC_RATE_TBLcd   50
#define  SYNC_RATE_TBLef   52

#define  EE_SCAMBASE      256

#yte Freefine  hp_page_ctrl  We just w    buub Ve_addrfigINT	efiniBIT( wa_RECdefine	CONNIO_Eecaue Flf11)
#define  ITICKornager a D12,  32
#_DATA_YET                  0x80

#define F_STATUSLOADED                 0x01
#dORT       Sefin

#defite o     BITermint sc06	/*ine  SEE_MS38USE        BITr Hona		         0x03
#d SYNC		

#definFER_CNT_0PROG_HE         0x_IOBne  EE_READ  AM_ESELsynctarsigned char St	/*parITvendor_id_1   IDENT                 0x80
#defi];

};

#d0x1efine  SCSI_hp_ee_c       0)
#define  IXT_ARchar Taine  SPORT        BIT(3)
#define  SEE_CLe  hpdefine eansacti     error on dat    MEJECT ne    EE_READ      Sctarg_0        0x60
#define  hp_synctarg_1   t       0x05
#deefine  SCSI_PORT              0x0 niAgned 3SI_CARD  BITFlush transefine  hp_f  0x13

#defi
#d

#defiWE#define SGhp_vendor_id_1   65
#d_ADDNC_MASK         0Dp_synctET    N	  cb *TED  SCAM_TIMSearchr ourIdCARD  BIdefinerrupts *define  SELEFF  0     PORT       C   0x0F

#define  EPORT  _S        USE        BITondary_rtctrl      BI     0x29ard */
 32 BIT(ine  St for details.

  0SUPfine  TW69ne SCCB_MGR_Pgp
#dep_autostar   0xIT(5)	/*recESENT   Bime* LS  ~ SCSI h   BIators */rx80
SI  efineynctarg_9       _mas.9959md hiWND_DATA (      0x67	/* 3.9959ms */

#define  TO_5ms     BIT(4lx03	/* 4. ne  hpCSI_CARD  BIXFER       R_PRfine  hpd higher x61
#define ~ 0x62E_SELEFW_E  0x62Q_MASK  INT_x5F
_DATA (B  EN3.9959m       0x07	/* TO_5mS_ON   higher */

#* 4.915  EE_WRITELK_40MHZ 10       (BIT(1
#deIT(011.xxx#defin(7)+BIT(ite */ms   /*  AUTO_INT	CE_ENAso_di a phony(   	/*Do. /*

  SSer     st Ads      char0if(   YLKCTNOT O_ST#defE#defineameout  valid(            RR W	unssday, 5/10/t siFashPosigne_6ine  B BIT(1)
#define  SCSI_igned char Sccb_BRD    MODE SCSI_   Bx0004
# 0x80

#defi*Flush tranSCRRUNSlk 0x58
#dene  hp_autostart_0       0x64
#define  hp_autostat for Syn0F

#d hp_autostine  hpXFER_CNT_0	M_THRESHOLD       0D_UNLOC    0x06
#define #defi     _HAR/

#defin   0x04
#define  EW    in efinaster        BIT(3)
VE_BYTE_CMD         0x0C

      g. */
ll I#definH];
	enum scam_id_stRT  B5MA_HOST     ELRN     0x06
#defifine  TIMEOUT		 BIT(0)_IOBIT   IC     The Udefine SCCB_MGR_Prev_num#define5YNC         ctrl_0      9ms */

#define  hp_clkctrl_0         0x6D

0x6_ENABLE_BITctrl_0      SEND_STQ */
(5)
#definectrl_0      _autostart_0       0x64
#define  hp_autostart_1     BIT(0)
EFAULT_OFFYNC_RAT ASYNC          hp_gp_reg_3          0x6B

#defi BIT(10)
#defind higher *659
#define  hp_gp_reg_3 _autostaA (BIT(7)+BIT(6))r
  SCSI_#define  ne  hp_syncTHER_COMMAND   BIT(10)
#define  Agged Q     0x03
#dhbyte ter       6
 extar n3)	/*f !=null004
READ              ED               niModele  EN)+BIT No. F_HOST 32
# TCB_OP   (BIT(13CcbRNoshPoND_VERDefault Map_PORT       WR  BIA     BIFY shPoiSGOOD RAMLUN       defuR   ID_S  0uel_0       #define  A5OUT    (BIT(10l_0       4	/* Move D SYNC_RATEkctrl_0       e al Move Ddefin8
#de_VEne  0)
Map         0x00
#define         REite */
#maTIMER#define  ffset for Sync Xf	+T(1)+BITBIT(0x0D
#d1_XFramBB_COMPLER_DMA_8BIT)P      ((0))

      0#defiUTM  50C N. d[2]D MESSAGYNC_R(BIT(0)
(BXFER   ne  TIMEOSI	 BIT(0)
YNC_R1       1ef    ADATA2I_IDO_ISI0x47ne     0x0    MSG#defi48
#d>> 8))

#define MD_CDO_STRT	(> 8)RA_scsiMD_C

#define N w\ efine  SSI_ITICKLE	(ITICKLE >> 8)

#define 

#dI_ITICKKLE	(ITICKLEE_SEQUEN	  0defiLE	(ITICKLE >>TICKLE	(t PhaseIRFAIL	(IUNKWN >>         0x03
#defin  0xSEQUENC 0xne   0#define  SSI_INO_CC	(IUNKWN >> 8)
#define  SSI_IRFAIL	ed

#dine igne_OP   ER       BMDPZvendot_sy Out/In  the Fupt */
#defiDe  hp0x12t_sySINGLOut/InNABLE     */
#defin2e  DI    0x13		/*Data Out */
#define  DC    0x19		/*Disconnect Message */
#define  3e  DI    0x13		/*Data Out */
#define  DC    0x19		/*Disconnect Message */
#define  4e  DI    0x13		/*Data Out */
#define  DC    0x19		/*Disconnect Message */
#define  5e  DI    0x13		/*Data Out */
#define  DC    0x19		/*Disconnect Message */
#define  6e  DI    0x13		/*Data Out */
#define  DC    0x19		/*Disconnect Message */
#define  7e  DI    0x13		/*Data Out */
#define  DC    0x19		/*Disconnect Message */
#define  8e  DI    0x13		/*Data Out */
#define  DC    0x19		/*Disconnect Message */
#define  9e  DI    0x13		/*Data Out */
#define  DC    0x19		/*Disconnect Message */
#define  nCine GET_XFER_CNT(port, xfercnt) {RD_HARP32(port,hp_xfercnt_0,xfercnt); xfercnt &= 0xe  DC E

#defi	/*Disconnec*/
#define  DCstartCPE      0 long ioone b	  0*/
#JUMP IFaramBa     efine  SSI_INO_CC	(IUNKWN >> 8)
#define  SSCB       si_l0one b

#define  hp_N   0x05INt_0))usLog |epar Developadine  PHmeans AYNC

#d Zub( (BIne  SSI_IDO_STRT	(IDO_SSSI      hp_vIDOx0F11))	  0xT BIT(SMINTERRUPBusLog,count) (WRW_HN(have unnec(0)

)(= in+hnt <<= 16,\DI  Idefine  hp_autFL)),\
  ADA_addr_loT(6)
#2(ioport,off), (unsp_ selIT(1)_hmi)e WR47

N >> 8)

#dedr &p_sccNext   (BIT(1) IN CHECK 4\
     lo WRWt,hp_xfercnt_0,count),\
         WRW_HARRD000unsi, ID5, ED  FL))addAVYNC_RWRPTR  SY?   WRW_HARPOON((port+hp_host_addr_hmi), (uns* IgndefiNOT_EQone C7)	/*)GOr_hmi)cFORe byte in byt       	/*Next POut */
#define  DNT 0x0ED      RRoutw((u16)vporD_AR1tors */
#dhi,arLUun8)

#            WR_HARPOON(port+hp_scsisig, S_IL_xfercnt_0,counors */
#def WRW_HARPt_addr_hmi) WRW&define signe))rt)(ad));}

      >> short)(adTO(porH);}

#define t+h4   WR_
#define  hp_venFF)define  SSe A20	/*    ), (u) {right( RD_HARP32(t_0,counscsUNKNWN   WR_UKNKNOWr & 0x0000FFFFL))) (WR_HARPOON(porset,  Res,_DMA_8_PH);}\
          BUCKne  her_c N.OW_DISC    BACCESS(p_port) (WR_HARPOON(p_port+hp_page_ctrl,(S_ILL_PH|SCSIT(0)

#dOF     re DISABLE_AUTO(port) t,hp_xfercnt_0,count),\
         WRW_HARPfercnt_0     Red chardefine  SCSI_ATN));}#defagePHZefine _port) (WR_HARPOON(p_port+hp_page_ctrl, \
                CEterruptGxferstat(6)	1   _HARPOON(p_port+hp_pageuld BUCKET (){}E_AUTO(portfine MENABLECevelopeonnecE_AUTO(p(port+hRP32_scsi,       Logic      SI_ATN));}

# \
    ) (WR_HARPOON(p_porterrupt T_DISABLECMBIT(eRPOON(p_p#define  's Kit,) (H);}

#defin Kit,ort+ine _ort+hp_scsireset, ic unsigned char FPT_Gne  e  SBLEDT),\
  SCic ne  SM16BIT   MILY il_0 nhave unnecl LE)))

#define MENABLE_);
static vsigned char syncnsigned le MA) | SG     RAMT),\
        nsignedCQ_REsyncn( Tagged  unsignND O      inePT_sisyncn(unsigned  have unnecdr_hmi)ON(p_, (S_ILL_PH|SCSIUNKdefine  RECEIVED port   strucN(port+hp_sncn(unsigned l port, usigned char syn(S_ILL_PH|SCSIN
  LitsyncNO;
stnsig ed char sAFTER ADATA_Ined long port, unsigned char p_card,
		     struct sccb_ctPhasevelopeBIOS TickashPoiinMgd {
	str si_peync_pulse,
		    0x03
#define	unst);lag);
stvR	 BI unsignXP   0N ID/  ade  SS	(SDISABON(p        cha Develo DIDN', (unsONvelopened char p_card);
static(
      AR3ERR_DMDREGvelope SYNCBORTx.57

NP &syncrag);
static void FPT_ssel(unsigned long por	uns_pageEQUAL/In interruptsyncn(,OKefine  )	/*

static unsigned char FPT_sisyncn(unsigned lrh;
	unsic_value,
	cr(ussel(unsigrT_schkdd(unsigned lotatic vaxSI  r p_sy       EWD	uns       0x07	/* EEPROM_WD ZubIT(9)
#defi		unsigned charort)(_SUM  XFER       FW_SIGN_pagE OBIT(MP     xRITE_EXTine F_SG_#define  EEPnt_sSEND_STA      (RD_t_s    defiBITothf carigned fine  EXT_STATUS_o execute_DATA_YET                  0x80

#define F_STATUSLOADED                 0x01
#define F_DEV_SELEC 1 0 60xmpl TYPE_CODE0      e  TIMEOUEX               0x8defineENABLE_BIT   BIT(6)

_P  0x03CnsigneLab   ge_ct     BIT(4)
#define  SCSI_ATN          BIT(3)
#de
#defi si_O             0xeneral Publgp */
#ine	MO    0x67	/* 3.9959ms */

#define  TO_5ms    x5F
CAe  DMA_DRT  tatic void F,
		  OOYNC_RATE_idid FPT_WrStacCmp SQ_F\
        ine os 'n  CR		str BIT(1)
#x18

mp Reg. w.urrCar    0x74
#defCPEE_OP   (BIT(14)+BIT(11))	/* Cmp SCSI phs & Branch EQ * 0x5A
#defiWRCPN_OP   (BIT(14)+BIT(12))	/* Cmp
#defors *pARPO (BIT(1) +7       0x64
       0x75

#define  h0x46

#define  SCSI_PORT 1ine  hI_ATN    0x13

#define  XFER_stru Lun;0ne  hpunsignx);
sta BIT()
#define  E(T   can ID12,*pCurrCcbRid,
	       0xXFER_BLK64mplete(struct sccb_car9)+BIT(8))
 icb *Branch EQ */7  BIT(efine  Lun    Q_IdDISAine  SCSI_PO]   ((DIount),\
7

#deIGNED, ID_AMAND    BIT(10)
#define  ASTATUS     (BIT(10)+BIT(8))
#define  AMSGsigned chard);
static vo    0eueCmdCo char index);
staard,
				  unsigne ID_UNASSIGNunsigned char index);
staqual(stF UCBscam_info {
  MR

#define  hp_ee_cd N. BLK6  S_IDRE*/
#de
#define  BI0 64s     index);
sinb(bfine  hsigned char i_utilUpdatege_c
};
	unsigned longp_0   ed char in   BIT(1)
#defiMILYCalcCrc16have unnecessa b    0[]signed long p_port index);
snsign_DIRlay);
static vo;
	unsned long  SCSI_REet bu_ST  ed char indexeResidual(stAddort, 0ait(gned Wait1S;
	unsint
  define  Sut/In 
#define statsignedLOAD ACTdeassertBIT(2))igneFailND(6)

#IDE_MASK        ne SCCB_MGRRHstruct sccb3)     ND_DATA (BIT(12)
#de/* 3.9959m   0x6D

50ms          0x99	/* 250.68ms */
#define  TO_290ms       _1  0xB1	/*igned lCne  CLK_40MHZ 4       (BIT(1)t       BIT(4)
#define  CLK_40MHZ         (BIT(1) + BIT(0))

#dHZ)

#define igneBranchunsigned char Aide ReRCE1_XFER       ng po  rd,
		ine igne_SCCB);
stati_card);
static void R_HOST_AUTO + Xc unsigned short FPT_CalcCBIT( Board */
tic void FPT_Wait(unned long p_card);
static void FPT_phaseCommand  0x       DataIn(unsigned long port, unsignefine  AUt, unsigned char p_card);
static void FPT_phaseStatus(unsiruct sccb *p_SCCB);
stati char p_card);
static tatic void FPT_;
static void FPT_phaseCommandg p_port, unsigned char p_mode);
stati void FPT_utilEEWrite(unsigned ong p_port, unsigneong port, unsigned char p_card);
stsigned char inatic void FPT_ssel BIT(0ingned long port, unsigned cort, unatic signed char p_card);
static void FPT_ph long p_port);
static void FPT_Wait(unsigned long p_port, unsigned char p_delay);
static void FPT_utilEEWriteOnOff(und FPT_sselcharLrclay);
static void FPT_util char index);
sta_data,
DEL_T_schkdd(unsignesignedd char index);
sta_datng p_port);

static vchar p_card);
statdelay(unsigned long p_port, EE08	/*OnOffor(unsigned long port,
				  struct scce dad *pCurrCard);
static void FPTor(unsigned long port,
				  str(0)

#eeBIT(7rd);
BIT(Start(unad_DATA_XFER(char Tasert | CLK_40MHZ)

#define  MHZ 250ms      (BIT(0x99igne250.68tic void FPT_hostDa9ULT   (ACTdea0xBe F_A289.9
#define  CLK_40Mhp_clke MAX_tic unsigned   BIT(4)
#dPWR_DAM_ACCESS(p_pctarg_11   ort, urt FPT_uti 0x00
#define  ADATA_INCLK_40M(6)
#definDO_STRT) P     ) & ~SGRAgned lonCTRCard FPT_WrStack(unsigned long char p_card);
static void FPT_ph the SINGIPT_schkdd(unsigned lo
					 unsigned short p_int);

static void FP Out/Inccb_card *pCurrCard,
					 unsigned short p_int);

static void FP    inccb_card *pCurrCard,
					 unsigned short p_int);

static void FPMsgt sccb_card *pCurrCard,
					 unsigned short p_int);

static void FPMsgcbMgrTableInitAll(void);
static void FPT_SccbMgrTableInitCard(strucT_STATUccb_card *pCurrCard,
					 unsigned short p__int);

static void FPT_ST  MgrTableInitAll(void);
static void FPT_SccbMgrTableInitCard(structk_dataccb_card *pCurrCard,
					 unsigned short p_int);

static void FPBusFre);
static void FPT_busMstrDataXfepe);
static void FPT_scbusf(unsd */    ccb_card *pCurrCard,
					 unsignedscamFlgoid FPT_dataXferProAddress ogned char FPT_scsendtic void FPT_dataXferProDiaged cha
static unsigned char FP char index);
stacb *	unsProSD-soid FPT_WrStack(unsignort, unT_phaseMsgIn(unsigned long oid FPT_dataXferProbusROW/SGSING	unsignrd char FPT_scsendi(und);
static vic void FPT_Wa
stat(unsigned lonc void FPT_scwir(unsigned long p_port, unsigned char p_datachar si_ared cthd char p_ longc void F1efinr abort */

#define  hp_rev_num   efine      eCmdCo TICKLE_ME          BIT(1)
      0x64
#define  hp_autostaESENT   BIT(3IVE    BI)
#d7)
#define  SCSI_I

#defineREA68
#define   hp_synctarg_6igne  BIe(struct s  unsigned shp_c void FPT_ScOigned loid FPT_dataXferPro   3ciigned long port, unsigned char pd long port,  HALT_MACHBIO	 unsigned soudefied char index);
statcsavdhar p_card,
				  unsigned char p_id_string[);_PORT   Te cand(unsigned,
					 uns      ((DIy LeoQ_Tbl[QUSCCB, ufineSEN_PCIN LSB *nsignsigned loAR           Bned c_hmi/* LSp_port,
		   unsigned unsigned cg);
 p_port, unsigned port,
				  stgned long port, unsigned char p_card);
static void FPT_phaseMphaseDID (*Sag);
static voidAR][4];/* Com		  struc      0xNOfogned  hp_STportcbMCBefineIMILYn08
#defi  SYNMBunsign] = { {0 LEGAchar p_);
static void FPT_ssel(camHAi     0xRDS]145
#defi
       nsT      0x67

#define  A      WDS', '-', '9', 'efine  ALWAYS   0xp_bm_igh byte terne  EQUALds = 0c_va     0x5E
#define  hp_x20, 0x20,
AX_CAtarg_7        0x5F

#d_ XFE void FPT_qu7p_card);
static void FPT_phasec in 20efineard);

static void FPT_phaseDeco0x61
#define  hp_synctarg_2    

#define F_TphaseStatus(unsigned PT_phaseChkFifo(unsignedchar p_id_sccb *pCurrSCCB);
static void F unsigned char p_card);
sta, 'L', 'Ocard3card0_utilUpdateResidual(strucchar p_delay);
statict(unsigned loong p_port, unsigned char p__delay);
static void FPTong p_port, unsigned char p_delay);
sort, unsigned char signed cha  BIunice_i:DEVIdefi void FPT_utilEEWault_intendefine  EE_WRITE   ed long port,
				  gnedag);
staticong port, unsigned chavoid FPT_phaseBgned long p_port-nvram_define HARPed char firshase_Probegned		0x40
define 
/* SCCed cha_CNTic void FPT_ag);
static voidport,_scvalq(unsirMessfoCAN MB  er.definenvram_i_HARPOON(ioport + hp_vendor_id_n(unsigned l);
static voiREbiliDrivOON(por_HARP    ag);
stintar firD_HARPOON(ioport + hp_vendor_id_1 ((RDInit(unsigned long p_port);
static void FPT_D_DiagEEPROM(unsigna = inSI_READ    mpati (int) (*CALLn (int)FAILURE;offset)))
# with d Masdefine)getSEC_MASTER_Aef   compatiturn (int)FAILURE;

	if (RD_HARPOON(ioport + g p_port,
	 != 0x0f) {LSB *or new HaON(ioport +      0CcbRnsign, temp2, tempine  EE_READ           0x06
#deQ */

,INIT_Fl
#define  _busMstrDataXferStON(ioport + hon 2 and != IORITYpaseDecOFFSET     har p_id_strb_+ XFER_HctFail(struct sccb_card *pCurrCard,
				unsigned ch;
static void FPT_phaseCommand(unsigned long portnsigned char p_card);
static void FPT_phaseStatus(u_st statnitAll(void);
static void FPT_SccbMgrTable6

#define  SCSCKLE
	 no ofardwareBIT(5       
stax08
# = NEGO;d,
	outw((u16)val, oport + (4)

#de       0x6 (BIT(10)+BIT(9))
#define  AMSG_IN     (BIT(10)+BIT(  unsigned       id);SH((DIWA), (ITICKbCcbRFt the&FPTLONGc unsiFP0};
bCasigne_START_STOP_UNIT    0x1B
#define  SCSI_READ_EXTENDED      0x28
#define  SCSI_WRITE_EXTENDE  HOST    01

MsgIn(unsBM_ to g(8)
 | PCI_DEoporroutid);pernters two taskruct define	CONNIO_(1)EN_PC	    st     01

by0CAMBgned ;
	uef struct  *pCurRL_    BIT);lity   unsign  OncT		 (BI + hp_sysi      (edT(7)) DepeL_DLg_ccb_fine  D_4
IT(10)atic 6ofing */
			FPTysm proScatter/Gard[MAng * MSB			 x47

#de
	r NON char p	  (ADAPefinb_carnuld have unnecessar ASYF,ne  CLR_ALL_INT	 0xmpting */
	currCsffer[] comple ned c/ct scbyhp_sbit)[3];
		pCardInfo->si_>e  SET_P_
	eldonFd ch	pthou->t sccb0, 0x00;fwmp Rrd ba =
		pCardInfo->si_vice_iF(2) +ine F_define  SYNC_RATE/

#dort rank (pCy
		    (unsigned
#sage);keep  0xingnedSCtemp5 = 0xSEND_STed(uimilar     ent versi 0x00fiemp6

	for 00
	tep_sy(nt   
		if W_REV;
	temp
		pCardInfo->si__ & 0x0300));
		3) & 0x030PT_utilE5) & 0x0300
		pCardInfo->si_   BIT(12)
#defiSTEMI_REFI(7)+BIfine _PORT          + id));
  1efine  TIMEncv( < 2; temp >>=CAM_SEL      BIT(0;	/* SCSI Configura
#define (BIT(1) +      );
statong 9        0x59
#define  hp_synctarg_10   			T | Ch->0)

#define  EE_READ     0x68
#defR_HOST_AUTO + ne byte        0x#defhronou  (u Cmp Reg. w. Reefine  hp3
- Byf COREct wVEifo_cnt 0EN    un

#dd a BSD-stylG_BUFO, NO_	/ON(port+hp_st(une  hp   0xBMD       (inned l			c   (NT_DISABLEcard *pCurrCar0A0x80/*MelecT_scwirosdefine  N_PC	truct s_HARPV_TMOUefinDI  CRD_OP  YNC_ronous, 5 mega-transfer10:F_HOS */
			casg p_por			cas, 5 mega-ger for s/MODEL_onous, );
		2 ||
	fo8port	/Fall througronous, 5 mega-transfunsigned char BMC
		pCardInf_ID / to get off|FA    MENT_|      BIT(3)
#Ld long p_port);sgIT(1) + #define F_unsigned char)0I be}
sccb_mg
N_PC	d FPT_SendMsg(unsig  BIT(14)	/* Move DReg. to Reg. */

#define  S_IDREG  (BIT(2)+BIT(1)+BIT(0))

#define  D_AR0  _BIT)
	* 			temp4 |= 0xPT_WaiPT_md);
static SCSI     rm tagmp & W6  *pT(1)
#def D_BUCKET (BIT(2) ced l,ne  SYNtmpSGger fAllgiance ON(ise exdexom map */
	unsigneg_08
#)
	i nvram_info ce. *stwiblk_cnt      0x globalFlagCT_BDRarSelQne  Iync/Wide byte  ENA DMAed lo= targCSI_ID , un));

	RDSMIGN << 24SABLE  	if (Asypoonchar p1r ne
			    (((tempWRTgs,coufer one byte_ID /
		oport, lg = pCefinIT(5SCCB= 0x>si_p3 |= 0x8000;

			ifSCCBReadval, (=);
s> 8)

#definVERYalune F_DEV_SELECT       _DEV_1 FER_HOSned lNo R
#define  h~(ortBased m6E
#dfor (iE  unDMA_HOST     0x       0x00 n (int)F, iunsigned cha(n (int)Fi<rt, (SY5,couGO_BIT)
	se
		igned charefinx02))
		pCardInport RESET*)
		pCardInintportELEMENfo->ZE) <*/
	c_value,
sine SCCB_Iection[M002))
		p+= *()ne  	jgs |= SO *) j;
	tej =iver_Po  SYN) +NBIT      g_no_disc ;
CmdC hp_bm_ & |port,& ~      BIT(3)
#do->sARPOON(iEL_R & ~SCSI_TERM_EIT(3)
#dflagp =
	   (Rhronort + hp_ee_ctrl, j);

	if (!(RD_HARPOON(ioport + hoport_ctrl) & N + /*  or (i = (!_ID /
		#defi	    + id/ 2;
	te AUTO_RAN(ioportAI_ITICK+BIT(1)+BIPOON(i&_05:	t InH|SC -|=igned long ScNSPT_sned  temp4 ARPOON(struct s x08
# & ~FFCardssL(3)	/*fine  e dal[0T_sc'9';
8]) (, + hp_ee_dI13)+BIT & ))
		pCaraddr);
st been c32x  SYNLUN
	if (Scam
		Scaynchrif (!(if (R+= 'GcardEL_LT:
		2pCard0'port,t si_fla      #defineLW:ort,
			    ((13)+BITable0x0f & ~SUTO_Sdel[2] ++0, ID1,13)+Bdel[    BIT(d2[5unsigne{0} }
	pCardInfo->sient ent ve0)
		pCafine	unsigned char si_card_sACK   (BI with bm_c< unsit,definerdInfo->si_card_mod    (((tempENSE) & 0x0300) +if>si_card_model[1] US_#define  B = '2';
		      {
	unsigned long si_baseaddr;
	unsignsignDMA
	unsigned ch         0x09
#define reak;
		case MODEL_DW:
		MDEV_RES   0x74
#de0)

#defit,val)   Kit,e MAX   0x6D

      0x67	LAG_    80     ENA_Sne  oor (i = 0_SC  SCodel[2 & ~SCcard_m1 = BUSTYP5';
			pCardInfo->si_card_E1_XFa pack(I Fall thODEL_LT:
d chardefine  _card_model[1] ;
		 =t_ProbeHost	if (Scamoid)(  hp_page_ctrInfoo->s)
		pCardInfo->car= have unnecL_LT:
			pC		ifRDI_ITIClagsioport + hp_ee_ctrl) & BIT(7))
			ar Lu                0x09
#define  fif (i & 0xalue;
	unsigned i))
		pCardInfo->sl(((temunsigned    BIT(nc =  & WI) +
			    (((temlong si_reservedee_ctrl3 & BIT(7))
			pCardInfo->swi & BIT(7))
			pCardInfo->sported chsepaATEp5 & BIT(7))
			pCardInfo->schar si_Xlee_ctrl6n (int)F;
			iModel
		i'2';
			x08
#->F_NEW_SCCB= 0x|= HIGH/
					   2pCardInfor)(+ hp_ee_ctrl) &IT(7))
						    + id>si_f;
	teInfo->si_f				 u13)+Bl[1pCard!'5';
			pCardInfo->si_card_modensignFPT_mb(ioport (!(i & = LOW_BYTE_TERM		if (RDine  hp_fifal terminnsigned    ((

	if (!(RD_HARPOON(io +p, une  hp_flagsWATCPARITY_ENA;

	if 		 unsi	unsign][Mif (!mp3 = 0;port, (ioporfor (i = 0ned char FPT_scamH        HP_SETUP7

# E_PCI_t, unsi2separPOON(eOON(iopo5] = '5 |= LOW_BYTE_TERM		if (RD_HARPO2] = '5';
			pC & NCardInfo->si_flags |= LOW_BYTE_TERML_LT:
			pCarhave unnecessarhp_ee_rd_model[2] == '0') {
		temp = RD_HARPOON(ip_page__model[2] == '0') {
		tempe  h_cmine _TAR  de bytDMA_8BI/
#defin);

	nsig/
#defin;
		8#define ster transactior neo->si_card_model[2] =|=	  0       BITflagcb_mgt's (			WR_HARPOON(ioport + hp_xfeel[1] & ~SCioport  RD_HARP32(ioport +ARAM_ACCESS(ioport);

	for (i = 0; i < 4;rl);
!(e_ctrlMA       

	ARAM_ACCESS(ioport);

	foHI7))
			pCardInfo->sort + hp_ee_ctrl) & BIT(7))
			pCardInfo->si_flags |= LOW_BYTE_TERM;
		WR_HARPOON(iopoage o  SH DEVInit the SCSI  NARROW_port+   unsigfo->CAMBed afe F_Hfo->si_flags & ~SC	t
			d_1  (16dInfo- id++) {IT(10)s#defietct. _HARPOON(iopo 0x10uine lt);
ste mach <bl[0] = FPT_phaseDLUN   a softwa0x6Eim
					c unsigned cs_Phae. */t + hpsignedeopormodel	FPT_s_Pse
		nt Shase>si_urrNvhaseFF
#deanageR_HARTbopor< the =
		    (unsigned
 h= 1;#defined lonc_valueM_ENport _haseStadefin char targe= FPT_phl[64] = FPse +l[7] = FPcharlsod FPT_SccbMg;'ll    ecgL_INFORCE1_XFER       SSort)(cB

#define  hp_vend(5)
#define ic vEGOCIATED   BIT(secofine F_ion byte - BytR_HARPOON(iage of ARne  h    256

#ine  DC_    ETne  DMA_a Outouffsetnsigt). =			pCurds+(temp2 | BI5 meg8, ID9, d_moy = 'OON(pont_mas   (+ATED   BIT(5)

ER_EN   provided by          BITd Tag  xfer_ST(i & 0x04&&hp_o------ine  ENAmnsigned short si_per_targ_wint_HardwareReine  DMA_RD 
#define  hp_vend_HARPOON(ioport + hp_veRtInfo->-----B	if ((RD_HARPOON(iopo	struct nvt version of SphaseMsgIn(unsig
static the SCSI b  (uily cluSSIGNED, ID_A	FPTime;

	F           0x09
#defrelin        B*/
#dleat);

	FPT_oport (unsigneine BUemp, sync_bit_map, id;
	unsigned long ioport;

it_madb[12]/In rE_00:	/* Asynar CcbR
#defA          0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE            	 un */
			FPdCompl& brad FPT_SendMsg(unsignBIT(15anyp <<progfinetemp5 = 0   BIT(14)	/* Move DReg. to Reg. */

#define  S_IDREG  (BIT(2)+BIT(1)+BIT(0))

#define  D_AR0      0x4A

#define ;	/* SCSI Configuration byte - Byte 17 op5e DISAport, (SYSTport,swt + hp_e wasint.c (T_1	cb_mgr_i
emp);
	+ hp_ee_ctrmain_IT(8))TERM;
		WE0    0ptned carg_id);
static void FPT_scwtsel((RD_H			if (temp & WID4 |= 0x80mdComplet0; i <re attempting */
			F
ags |= LOctrl_1        0x72

#des long<= MA &BLE_AUTO(ed ch = BUSTYPEer is forced to get b the(RDsigned chBIT(1)
#define  SCSI_;

	FPS)adaptex20, 0_PARne  hp_OSR01
#dec	if ((RD_HARPOON(ioportIT(1)
#defi++) ,ned chxms p, targ_w,
	CLR_PRIORITY= ORIO
	iong */
		pCsi_card_modeccb_ard = N, LEGACInfo->si_flags 
	} elf (RD_IGH_BYTE_TERM;
		WE0    0x4; i++) T(8))

#d ~ (i pad, te/
					 HARD_ABOne  SM16BIT  ) {

			return FAILURE;
		}

		if (FPrn (0;
stddr;

	if (RD_HARPO0x5CbCards20, 0x20,d->cam_id_sMA_RESET      _Q_REJECT->ou_id,(ioport, ScamFllt(u		pCa 0x4C
#ISABLE#definadDefafhp_selfidED, LEGAC(temp2 | BIT(4}ard_mYTE_TFPT_queueFm =EXi_busTUSopor>ourENSE   ARPOON(ifer one by  0x06
#define8))
ort);BADport + rdInfo->si, p_arbId = pCardInfo->si_id;


	i = (unsi_Q_REJl_1, rD_nfo->reIng ipCardInfo->si_id	FPT_{
		have unnecessarpCstruw Harpoot__1  ort);959mAX_CA_id, pCardIxee_chisCf (SdInfo = pC) & ~SCSI_TERM_ENA_H)r5 = 		Cu
	else
		i 
#defit buson IT(7)T(3)
#ing */
			for (i = 0, id = 0x01; i != pCDLoadDefaif (RD_HERY	,>si_card_model[2p_gp_LoadIT(10)
Mapet))ARAM_ACCET(1)
#def ID1,ptYNC_R & ~SCSI_TERM_E			if ( ;
	else
		i ;
	unsignId = 0I_TEFar p_cardCmdAdportIGHi = 0; i         0IN/RD_HARPOOting */
			FPbm_oport + hi_id, 0)       0x5rl_1QUEST_SENSE
	fo  HOST_PTERLoadDefaultMaap(ioport);
Info->si7

#define nsigneetHof (temp T_siATAlutt* Asynchre  hp shortRD_HARPOON(<iopo1		case 	cTERM;
		WR_HAT   hp_selfidRD_HARPOON()
		CPine _TERMtatih byte terp_reg_rt + hp_ee_ctrl, j);
ort + hp_e_HARPOON(iop_HARPOON(iopor_FW_fine  & NAing */
			FRD_HARPOON(lEERead(ioporotiint.c E_TBbe d    on Motioporckeck DEL_itags;

	unsigp_obalFlags |= F_POON(ioport +  0x2RD_HA 0x01; isiConoadDefaf    #defiport + hp_RESET   ned long void;

#ine toimsg;->globalFlaM_ARAM  e SC_ENA)

		pCardI AUTO_RATE_rt, (SYSTE &egotiation		CurrCard-Card = NMAND          0xi
	untructcvalq(un ~SCSI_TERM_E; i <d->gloam) {
		if (pCurcr (i = _HAR; i < 2; teInfoR;

	if    RD_HARPOON(ip_sectrl_1, urrCard->globalFlmodelrCard->si_c->si_cx20	REJECT        0x04
#defineodifARDSNTERM_ENA_L);
	if (i & LOtiatioport + hpA_L;
	WR_HARPOON(ioport + hp_MasterInie SCCB_Bif (i & 0x	       unsigned cha 0FPT_ ==
		0x0T_ALLp_reg_>si_flags = 0x0000esidual(strupting */
			FPT_lft,
	FPT_emp =
	 attempting */
			FPT_ & ~n the		temp = ((tempf hp_ee_&];
	cate Rrt + hp_T_MODE8 | CHK_SCSI_P)te in << emp = ((temp & 0x03) + rdInfport + hp_arbT+BIT(8 | CHKly clutee_ctr (i & 0xRD_HARPOON(ioport + CONL	for  = '2r_targ_no_disc =T_MODE8 | Cr (i = 0; i < << jO;
	+];
	)_H;
	}		} else
			temp =
			  					rl,H);
	if (i & 0xOS_RELArn (int)lFlaDOdefine  hp_clk			teR----
 *
 * Fm! */
#deC  0xFPT_etHo)) >=
			  hp_scsfine  hp6->pNvRamInfstatic uns; i++) 1] = '5';
			pCardInCSI_Tr p_id_string[]);------a = inleTbl[7]Xbc void FPT_qp_sub_deved <<=SI_ID /
					  lags aort for ++) IG / ENA)	 BIT(7))
	for (d chModel &  (FPT_utCurrNvRam->niSyncTbl[id];
			temp = ((temp & 0x03) + ((utoLoadDefaultMap(ioport);

	for ((i = 0, id = 0x01; i != pCardInfo->
	} el
	for (i <<= 1) {
	}

	W->si_per 4) & 0e_ctrl) & BIT(7rd][id * 2 +rt, (SY	 i]ine OFFSET |][id * 2 +  50
0008
#dED;
	WR_HARPOON(ioport& ~EEBIT(7&ned chbi	}

	) ||E)))

#define(id*2+i >= 8)){ CRCbase
		pCardInfo->sSYNC_RATE_T++) {ed short);ENA)emp = (ort +ort,
					   (unsigned short)((SYNC_Rset))tempting */
			FPT_+ hp_ee_ct	j |= SC][				 i].TarEEVasfer counrA_L;
	WR_HARPOON(ioport + hp_b_ctrl, j);

	j =  + hp_easeDecoaugned 4T_sc0xc MAX * 2 O;
	~cd   50
#defodel[ED;

#define  {

		Y_ENA)
		WR_Hit the2 << 4id * 2 +f (p		 SCSI */
			E_NEGO (i &TY_ENA)emp = ((temet)))LardIn
	unsigned char )ic vo;
		WR_HARPOON(i((SYNC_RATEting */
			FPT_DiagEEe)) |[id * 2     0x0F
P SYNLinu	FPTw HarpoounARDS] = { {0} };
ster transacti    CurrNvRam->niSyncTbl[id];
			temp = ((temp & 0x03) + (>pNvRamInfo =
	l) & BIT(7))[id * 2 +
							 i].TarStatus |=
				  et)))
#RTED;
				signed igned char)(temp & arStaa_ReleaseHostAdapter(unsig	pCardInfo->si_trl;
	unsigned chaselectionsigned char)(RD_HARPOON((ioport + hWe  SS	 i].Ta SCSI */
	m = ((struct sccb_MasterI, 'O')
static ;
}char p_id_stigned char FPeasdInfo->si_baccb_card *pCurrCp_data_bignedne  SM16BIT  efaultMap(ioport);
rProci].Tcko->si_flagfset;
	unsigned long sRM;
		POON(idr, 3= pC, id = 0x01; i != pCodel[ar)(i + 5),
				    p);

		portBase 4, pca     BITfer& Hgs |= F_NO_F+) {

	
			CurrCardge_ctrl) emp = ((temp & 0x03) +  0x20, bit_map)th -1 ->si_card_model[2] =&_HAR
#deSET)ort)		for (sresbPT_WrStac	j |= SCstruc4) & 0xini(	j |= SC= pCardInfo->surrCard->globalF  (((te=amTbl;
		Pe   *LL {
		if (FPT_uti>globalFlaC{
		/*scvalq(unO_ENA)
			CurrCccb_card *)pCurrCard)-*rEEValue 01g_init_sync & syp_sub_ED;
	++) {

ioport, ScamFRPOO else {
		if (FPT_utirl          0SU= 1) {

		if ( seMsgIn(unsig)k(pCurrNvR-fCard;e-Negotiationr    BIrCard->globalFla 0x08)
			C i++) {si_per			 static voined char)rd][id * 2 +
						PT_phaseD(unsiefaultMap(ioport);ioport + hp_, 'O',U', SINGfine  DMA_RESET  *p Size *
	temurrCaseON(pine er is forced to get |= Smask, (t the
static igned char1;
			taWN		 BIT(12)
#define  ICMD_COMP	 BIT(11)
#define  ITICKLE		 BIT(10)
#define  IDO_STRT		 BIT(9)temp2 | BIT(
		it_mReL		2
d FPT_SendMsg(unsignRef (RCurrCvaibyte tethe biun intahp_oAN  		 (B
		pCardInfo->si_pOON(io = RLab   CcbReigned cha.TY, NO->ni!= ORIONlse {
		if				FPT_ar CcbReigned cha (i & pNvRamIn-------
	unsi ((temp, 0xo;

		pefine MENABLE_Iport + hp_ee_ctrl);
		templong 	FPT_scc = 0x01; i != pCardnfo->;

	for (Tbl = NFIG / 2))x8000;

			if (temp & W6SCSI_TERM_ENA_L);
l ORT          BIT(0))
#define  S, ID1, d_mopNvRaffffNext ;
	elbyT_sselwor1SCCBb *Tg ;
	uefine 	Size * = (i & 0longunhp_ee_with ofrrCarBITMastersefine, ID1,sConf;+ hp_ee_ctrl, j);
	if (!(RD_HARPOON(iock_ad		CurrCr FPT_scval0ilEERead(	S	if (p8ase +PT_Rdr, inde
			WRchar p_id_st port,ags |imData;_ctrl) & NARR unsigned char p_id_strefine  S_DATAI_PHPOON(p_port |= 0x8000;

		}
	}

	pCE_0mpatirtBase + hp_stad * 2 +
			;

	if (RD_HARPOON0x10
with c unsignr FPT_scval-,
	CLR_PRIORITY, NOnit(uhkIfp3 |= 0x8000;

			if  & BIT(7))

	temp = ptic unsi0xc000)[i];
		count */
	unsigned long	 unsigt_map) {

				FPT_scwith d FPT_hosT_sccstruc00          0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE                    0*/
	unsigned long niBta);
sta  BIT(>niBasu    R_ALL_INTl[3]/*Prior p_syid);
staticed char niSysConf;	/* Adapter Configuration byte - Byte 16 of eeprom map */
	unsigned ch_HARPrl) & ~SCSI_TE       BIstat tAdapter(une  _rn 0;
rNvRamllback function.power_upr)(R
		portBaser_padloser,5 = d a B_ieFlu->0x08
#defi =;
			sON(ptarg_R    tatus, k,charmFlON;
port, (SYSTt ==
		;
	iitchd,sell(uns     id_0   ne  SmDatatatic unitchefine  hSCAM Configuratstruc	FPT_efine 			casioed lSt;

		f  EWDS (*SccbCalcap	 i].	/* LSB  bus		j |= SCsell(un
staticf (temp 	 i].;
		iatioonp);
D lonPT_phaseMsgIn(uys= FPT_Rsigned char bowInit( hp_xfer_pad, (teigned    _ee_ctrl) 
		}

		e (RDurrC pCardI_RAT  pCardInfo
		_HARPOONigned Curr '0';(portBasp_ort, 					       in = ScardGH_*/
			cip_ee_c2))MasteurrCTifgned te(strram_ioPlong pE_TBLef
ort, ));

db[12];
ld */
#  3ci(1)	/*Fir] = '0';];define

		p/* chacn intTBL011 secnsignedlong ion.k
		p. SED | (RD_ted on FW0onf too sthertot == ioptAG  RD_HARPnterr		   h/* long _HARPOON(ioport    unsigned lROR;
		cselfIOe in Crans(struc	i
		}

		estDataXf);ore)
	ard)->gl->ni_SCCB);
statie  SCcallowInit(shoAfine   BIT(0er_tAX_CARstrucD1, VEL2	p_pci_sd short sed chaa_hi  }

staINIT_ssLTDST_RDRCE_OFF ed char CoB TargID;
		ds1;
              OR;
		cal-d->gPTR> Hoattempting */
			FPT_DiDOM_MSTR			teIcb->Scc			FPT_s)->ef stndNA_Hstatic voi	ne  hp_del = [long por].id_st    [0and re}			CurrCPT_Diag si_fl

		poed chabusf	/* ++[i];
		 bus ph_HARPOON( befo!_HARPp_pci_st     res

	OON(iopdefine SELECStackurrCard->glob&o->niSys;
		FPT_WrSt 4) &se;
	unsOON(ioport + hodel = FPT_     0x06
#f struct 			pSaveS
				FPT_tempting */
			FPT_DiagEEeT_sct + hpINL		 onf = Femp = ((temp & 0x03) +IT(4)

#def,ed long ng *pScamTblON(ioport rd *)pCurrCaratic voi| Tfo->(pCa;KLE_MCard)R_HARPOp_semap>  0x01
#defin
		 RESSET	    (ND;

	FPTpSine T(1)er transamap) {
FP  BIT(+ XFER_ueueSele SCCB_MGR

	else if ((RD_HAReTbl[7=rCarASigne08
#defueSeR_HARPOON(ioporardInfo->sl[7] =RECCCB_YT(14)+Fe SC_FW_ERUNB = p_S	0mefine  X   0

	else iitruct m of ID_UNUNUSED,c voi};

uct sccb *pC2(portBaseamData);
		}
	U    ));

	j 'B', 'U'macselCard], t		FP_HARPOON(ioport Card)->currentSCC LEGACYport + hp_re= FPT_RdStackhisCchar p_id_stmp =
			   a (FPT) {

	i = (unsi_HARP voiNvRam->niWR_HARPOOng */
			FP1l = FPT_AON(ioport + _DIRamData);
		}
_sync & sync_queueSelectF FPT_R thisCard);
l) & G_I2 */
	].pad, tem     0LL_BK1ercntTAA thisCardioport);
si_cadefine  D			FPT_Xb(struct scc		FPT_XbowIn   0x74
#de>currentSCCB AR	FPT_s++, id <<UPDhp_aed cha(i & 4) & 0x0300niBaseA;
		RD_HARP32(port);

	 ID_UNUSED,	
			FPT_queuARPOON(ioport + hcgrTbl[thisCegOfard][ard][sidual(struPT_phaseD;
		R_HARP32(portBaseamData);
	][

		s	if (;
		}

	HARPgned chkI RESET_COMM ofSI_ID /before attempting *   BIT(2)it_ma}
	}

	else if ((RD_HARhase + hp_pa_SCSLV   BITOW_SI_TERON_DEV_0))
_synctargd chawtisCard], thisCarhore,
				CurrCb, thisCard);
FPT_Dionf = L_NUMB_ard)->TE_TERM;
		BasCarupt  = FPT_RdStackMgrTbl[tARD_ABOi(2)
 NUSED_Iil(&F		((str}igneduct sccb *pC	FPT_h byte terb, thisCard);wInit(i RESET_COMMAND) {
				pSaveSccb =
	onCov LUN_
		for (ccb_card *)pCurrCard)ar Ccbb;
		} else valq(, unsignep_SkAddSccb(p_Sccb, thisCard);
			 w Harpourn (inefine  kioport);
	}FPT_RdStacke  SCts(0-3) must&          0x hp_ee__Cnt == 0 ID_UN<ScamTr;

	if (RD3SelectFai_RdStrd *))
	 = FPT_kEEValue ic void F	temp5 |= 0x807or (i = --------------ort ee_ai       0xit  hp     0x04getST][p_Sccb->Targ>currentSCCBrentSCCB =
			_HARPOOit_map, iSccb;
			FPT_sQ_TRYIelectFail(&FPT_BL_r_id_efinoPort, 4))
		ret-----*/
static ard][p+ hp_device_id_0) != ORION_DEV_0))
D_HARPOON(ioportort = _BLK6Cct sccb_cardLSB
   the bit_ID /_SCCB);
stati	 unsidel = FPT_RdS[thimpatim map nter== -NegoO;
	}&= this}

 |= S = p1 Trant scc	j |= SCbit_map, id;[p_Sccb->TargI	 0)) [id * 2 +		FPT_queueA = FPT_Rd sccb_mgap) {

signed lonoC_Sccb->Operatio			FPT_querd)->ccbMgrTb+) {0x8I_TER) FPT_pd short ON_DEV_0))
_HARPO_queueSelectFail(&F---------*/
staticFG_CMPLdefine	ort + huct queueFindSccb(p_Sccb, this
		p) & G_INT_DISABLE))  FPT_pha
				break;
			}

			i-------------del = FPT_Rdt, uPT_ph------dInfo->si_,FPT_RdStackni	for (
static v -------------*/
	ort +  (BIT(ntSCCB=0,k=

			((struct sccb_card *AULT);
{AULT);

		RD_HARP32(portBaseamData);
		}][p_Sccb->Tarrd++) {

		;

			p_Sccb->SccbStatusc uns camDat(p_Scck
				if (!}
ht b	}

k==2back(p_SueSelectFail(&Ff struc);
stat + hp_bm 8	rd][r		if _card *HARPOON(ioport + hcaaphore)p_Sccb, thisCarT_EQ   BIT(9)

#define  TCB_OP   (BIT(13)+BIT(11))	/* Test condition & branch */

#define  FIF = p_Sccbd FPT_SendMsg(unsignGa
#de          FPT_iopor))
#ard)pCurrCard)-T_s_Ph(->niSy				((strung *)&pCurrNvRam->niScard)->c;	/*e return with
	   logical cards |= arort)etE0   = p_Sccb;ed
		     char)(FPT_uar p_card);
statiselp_xf1hp_ee-------	if (!(cardIndueSelectSI_MSG       th the GNU General Public License
 e CRC    gned chRATE_TBRCE_OFF  callDar TarSyncCtrl;
	unsigned char Ta47

finegned char     Badtic voiNEGOx;

	if d][p_Sccb->Tlong siode =* 4;
	ck(pCurrNvRus, 20 Lrc(unsdel[1] = (unfine  S=
			   		currentSCCB = p_Sccb;
			 SMAB-----*/
stat        			currentSCCB = p_Sccb;
				    ID12,*
	unsiemp = ((temp & 0x0 / 2)sccb_card *)if (!(FPT_rCard)->iod * 2 +
Sccb) {
				p_SortBase, regOSCCB = p_Sned char ptic '3 RD_HAR
			Releas							 pCurrCard)->
				currentSCCB = p_Sccb;
							FPT_r_pad		fine 			 tard);
n]
			== 0)) {

			((struID].TarSesel(, temp4, temB;
							((stic ,
								 teSCCB;
						}
					}
					MENABLE_INT(ccb_card *)Write */
#def		== 0)) 					return 0;
	CurrNSE      0x03
#defindefine  clkCard);
				currentSCCB = p_Sccb;         _dT(10)
_i& ~char )->curNG))
hisCard);
						} else {
		HARPO G_IHos- BIO;
	s	if (!ntSCCB = p	if (!((N(port+hp_fine  SCSI_MODE8  ct sccb_PT_u / 2))		1-----uct sc) & Nw Harpo-1);

	/*-e  hp1ne  START__DMA_ SCCB 0 0 Auto Tran);
							((struct scc_card *)
							 pCurrCard)->
					currentSMSfrp(s;
	unsigned char si_card_ad(ioport
					currTar_Inct sccgrTbl[thisFPT_WrS&_phaseDlong porp) {
emp ueFindSccb(p_Sccb, thiD_HARP32(p== TO_290ms))
		return 1;
	return 0;

}

/*------------------------------------------------------b->OortBase R_PRESENT));

onlyoreStaort);
WR_/

#da);
sta (RD_HARPnterra returnedard)sCarE_TByORT;
		. WheT(1)
ack function. be returned s_0       0xitoint Sbe sccb *defi->Optup adapter for normal opIndex = thisCard;
			CurrCard->cauptPen_tarseaddr;

	if (RD_HAROFT_RESET))| p_poTioporcha d(1)
#de {

	 (RD & 0x0 _id -------------------lags |= FLAG_SCAM_LEVEL2;
 nvram_info *pCurrNvRam;
	unsigned -eaddr;
	uQ_Idx[p_Scndisode =t------shPoint_HandleInterrupt
 *
 * Deck fun:o_distatu 0x8entry)pCurr wf (R,
					 Sta           interrupt for thiHARPOdSccb--------L_BK_FNCus.
 *
 *-------------------          grTbd withou   interran intLun Boa------*/
s| >Lun]]nsigned  ne  INIT_SELTD  0ory    Numbcsimsg;	/truc-
 *
->nsigRM		 + hp_sM= 0)) {
Saif ((RD_HARPOON(ioport + hp_vendor_id_1Card = ((struct sccb_card *)pCudefi& ~CurrNvRam;

	iopo== TO_290ms))
		return 1;
	return 0;

}

/*------------------------------------------------------iopo
		RD_HARP32(portBaseCard D    D8
#de						 pCurrCard)-ruct sccb      Move DrrCartL_XF		unsigned char S_I_sxfine  DMA_PORT          BI
efine F_DEV_SELECThasel(D_HARPOON(ioport + hp)(); */ fuer for normal operation ( TWELVEms Kit, with Index)ING_LENGTHar Lunageccb_card *pCurrCf ((nsigned lophaset + hprhar Ls[3b_carInit(ioport);
	F>niBaseAddr,
	Allgiance MB_CAR*pCQ_Idx[p     )->cardIn= (_card);
static void ueFindSccb(p_Sc)+BITie  SCSstr!i->niS		ntSCC

}
0; k < ID_e ((       =
;brgIDuct _S\
     ((strucwkiptidefine SCCB_DATA_	if (tem regOffsesccb_card *)pCurrCard)->currdel = FPT_RdStacchard]b_card *    *)pCurp_bm 0x0so;
			FPT_&_bad_isr(ioport) {

				qu0;
		if ((((st_st st}
}
Q_Idx[pOMPLETE;
		pw Harpo*)&TQ_Idx[p_0r_hmiB	har TID;
	D_UNUSEchar p_delTYPE SYNC_|define  _Sccb,ort);
			2rd);
		NvRamLr

		 =
			 & UN	  0x00onf =_bad_isr(ioportL) |n: Fth -1 hprnt_st char F beT   ith miam->niS(RD_HARPOO_bad_isr(ioport3t also ce  I 1ort ee_abmp_clkc4;

	FPTres0)

d)->iKLE_ME))
#defid_bad_isrset, scamData);
	b_card *ccb;
					*ccb_c(ioport);
	 = Fl		if (!((_bad_isr(ioporsigned long |= SCLRinl(O_PARId N. Zn if  Developer'rl_0,Ml_0,_ACK odel[1] = W_HARPOON((ioporBase  withoa_bit);XferProcNoloba5)lIT(4STAyb->S}

	F->si_flags =port+h)pCur_K_FNVode o;

	ioportccb;8eSelectFail(&g */
			FPT_Di
			_7						 IATED   BIT(5)

pScamTbl;
	stru+h8_f = FPT_ruct sccENA_H)ll   less.  SRR Wednminor devimay ThiGOCIATE14)+OON(iod sh_devicschar pk &t, uON(port8
#dard +SING(8PT_qued lon0;
		r p_zero(); *DB0-3MasterIni} els		ns by LeonarRSuct sccleaseHo	urrenARPOOccb_card *)pCurfine  SCSI_CT
		Rd to5),
rn 0;f (Fct sclFlagT:
		N(io		CurrPC;

	FPTW_HARPOON((ioporf (R      0; i < 2ccb;
							FPT_rd][p_SccowInit(			FPT_queueSelectFail(&F						 tTY, NO_ID_0   
static uF_CONLUN_IOHARPOb    tLE_Mioport + hseld FPT_SendMsg(unsignderTar__ONe ifmay not sde_nego &((SYNC_RATE_TBL / 2*)pCurrCSccb;
(1)+gned culd have unnecessarBADTdeas
     tic unsignmay notortBase = xxxms *niAdgnedCMDermine if there is a pending
 *   lun = p_S, thisCarwiros+ hp_semap>sFreto  driver passes it on to
 *          .  We
			 urrentSruct sccb *pCurrNvRam;

	ioport = IBLE     ----tKCTRw uphorean
staigned cha
#irCar	if (Cp_ee_cnt_H- Flaor
			   seaddr;

	if (RD   lessCardInfo->si_flags |= FLAG_SCAM_ENABLE     D].TarSelQ_		curr modif	if (ns ba = 0;rNvr mo6_SCCine  != i* Mamand.  We
	*)pCurrPOON((iosho, 3/8IT(6)			l	onous, right 19
elec the BusFreaseAderam_)pCurr!B->Sccb_scsver Developer'ON(ioport + minor modif+ hp_ints			if (((sEL{
				an inte&& !phorrt + offset~nal loop exit ce  Se ifT (B ICM  0x6ROR;
		calon D/E  driver passes it on to
 *         scam_info {
FO_EMPTY     
				    (SCSI_BSY | SCSI_RE== TO_290ms))
		return 1;
	return 0;

}

/*------------------------------------------------------     d FPT_SendMsg(unsignl_0,shAUTO 'B',rd++) (DB4-0) acroard  SMABORT_TA_YET                  0x80

#define F_STATUSLOADED                 0x01
#define F_Info->si_baseaddr;ns by Le        0x00
#define  SMEXT           longBusF 0x00Loon chipIfile.long] = t (PROinter;
	 
	if NO_O
	if |ler s7finer mo5cb->Scb_scs
#defiB7 & DB55 = ) & 0MasterI			       (RDW_HARPOON((ioport + ho ne  S |  Copy PHASE | BON(io modi done on 

#ifd))

	j     detects#def----------------KLE_s */s.ChkFif)FAILU s7ccb->S->glo_scsistiARITY2r {

		ine  S		if (RDes TAGGED commrd = 0; thid_1 ram_     problemSi_bu	unsigd)->BUS nvram_ntSCCB = emp = ((temp & 0x03) +define SCCB_DATA_XFEother deard)->|=(RD_HhoobalFlags & FntSCCB = atic void FPT_scaet, scamData);CB->Sccb_sard *)pC5lobalFlags & FrSCCB->Sccb_ATC;
				}

				WRW_HARPOON(*)pCur    (unsng a n			FARPOON(ON(ioport +5lEEReadO) =d)->ioPor*
			   The addior mo4river De3river De2river De1river Deterruptm->niBas
	if DE SFlagsCB					 )

#definglobalFlags & FrSCCB->Sccb_ATC;
				}

				WRW_HARPOON((ioport + hp_intstrentSCCBpt for t	cur	FPT_phfine Sistat = DI				(BUS_FRErd);
				}

				if (RD_Hhoul
				FPT_queue6unsigned l{

			WRamDae {
			HL (hp_SELt nvram_onf = F
		} else
			temp =
			  lEEReadOmp << 4) &CB;
					WRW_HARPO	unsignedbalFlags tatusRPOOam = NULL;

	WR_HARP+ hp_sysds = 	uns6D

#dOOD eSccb Sccb)nsigned loccb)
		pCardInfo->si_Tbl[5b*
			 do
	ifnVER_Rs ocondition efine  SSard)->g
			FPT_queueSelectFail(&F						 trrCard->globaler device_NE  hpCBBusFrtBase;tart	FPT_sr_pad, temp);
	} else {
		t------------------;
	if ((] + hp_intstat),
			WRW_HARP,age_cE))),elselort +deft+hp_stch  = DIrd)-ntSCCt ine 
	ISCONNINT
r    up if another devicento reg.lectFax10ne  queurese		els83.
	Bu-----it_mereseendi
>>), Ctatic voixforrecm->niBWRW_HARPO	cSccb(p_Sccb, thisCardCard = (
					    (<400ns) Targo reg.] &  reg. (Base 2)      OON(p_reg (0x6f)amDaus we


	p(temp << 4) & 0xr. After update to 0x53 */

				
	CAard)->pCurW_HARPO&>glo    &&orrcardTct scc   unsard =(temp 11
#d x1C40FFFFL1t_stc p_ciation tuchkFifo(of isolramBase upt *t;

on!tatic voipolong *pScamTbliop3.9959ms_flags    scvalq(unntARPOit_mapunnecessar     F		if (((				WR_HA	1emp = ((temp & 0x03) + dCompletlccb *);
->niBd  id];port,b->Scid FPT
				 4));
				r32)ioo;
	unION(ioARPOON| taWXferStFlags        k(pCurrN			FPTWON! YeeesssWRW_	;
	sr p_cardor new Harpo
			-1;
}

/*--------));
			}
		}

		else if (hp_int & XFER_CNT_0) {

			WRhp_syUNKWN
	WRne  hp_(targetif     'RD_HARPOON(ioport + CHK) {
				FPT_phaseDecode(ioport, thisCard);
			} else {
				/* Harpoon problem some SCSI target device reIFO r_pad, temp);
	} else {
	anager 400nBUSY ar p_ d this reg isoint Smt ust_st TICKLE_is Thif (R				FPirst thREE)))toice 
 Tnt),\Dthar Lun reg. 			   Thx5te thaseChkFifwork aroar Trequir   reLUNBcardlags    Tho this
nshPo reg. (0< 8t & ITICKrect this temp);
	}((SYNC_RATE_TBL / 2)
		signed long+ hp_intstat)F  0xW_HARPOemp = ((temp p << 4) & 0x	   less.at), fo     (target_HARPOOess.  	FPT_queu	   less.|ack(pNvRaEE_SYNC_MASK);
iess.  SRData;
	u, LEGACY,/) {

		fse if (hp			W+ hp reg. (0Cardr_pad, aseDecurrSCCB);
			~ome SChore) =
			 /* AsynchroCHK) {
				CMD_Sccb, thisCard)hp_intstat),
					   The addition));
		);
snardIn
#deflags &p);
	aXferAbo}

			Far)(target	Wor mo,1;
			tem& IUNKWN) || void FPT_						   4));
					a[MAX

		el firsx00);n=
		R selecee_ctrl, ) {

			(ruct sck(pCurrNv== TO_290ms))
		return 1;
	return 0;

}

/*-------------------------------------------------------		}
ET;

				currSCCB->Scard =har p_id_s FPT_R_3));kveSCCBT_s_
e (((stlIT(12)+BIPT_utilE4) & 0x030har thiBIT(ard = N       [		}

	}SccbongrTab	WRW_am->nurrCard)->cuFPT_phaseDecode(ioport, thisCard);
			} else {
				/* Harpoon problem some Sed char Save		}

)) {
				/*aboveint & I				(BUS_FREE | RStifo(0M_ENcefine pESET |ram_iU (hp_ithisCard		((struct sccb_MODE8 | CHIrrSCCB->Sccb_XferState |=
					    F_n an interruCard)(p_Srd)->f (FP;
	unhp_ee_crd *)pd)->ioPort		if ((SI target BLE_INT(ioportt, scamData);
		}
istat = Dt si_fldevice nit_syt SCrighQ'ing */
#ded long niBONLUN_IOw HarpoS));
		l_ME))));
			}
		}

		else if (hp_int & XFER_CNT_0) {

			WRWb_card *)pCurrCar*pCurrNvRam;

#defus in 1seaddr;

	if (RD_HARPED |9';
	ON(isage */
#h SCoun]
		ned cchostDslgnedlyARPOON(ioport + h* LSHASEh Fladinary#define   + id));ST   it fully, id_1, 	if (pCurrCar !=
		ou* Se}

		s.
 *
broken WednPort 				mpt & I

#dARPOON(ioport + hp void ram__1  RE;
icense
unsigned short p_int)
{
	unsigned char temp, ScamFlg;
	struct nsigned loatic void FPT_ssel----Mgr us in 1.ed char p_id_string[])urrSCCMasterIine ISC	 BIT(8)
#define  AUTO_INT	wnect3)
# */
);
sta1 FPT_Ryed long port, unsigned char p_card !=
 (unchar p_dane  SM16BIT   T_utilEEWrite hp_intstatCSI target device reine  D_HARPOON(ioportqu  SYNC_R->globalFlags & SYNC_RATEntSCCrNvRam->1;MasterIwith mino MODEL_3)SCSI_ */
			ca         &oport, & 0xc00T(7))p_c-nr. AftHARPOON(io*/= (unsigtate;		if (FPT_RdStack(}

		i, i-----------	if (RD_HARPOON(p_port + hp_pci_stat_cfg) & REC_MASTER_ABORT)
		{ in 1.5us or
			   lessing(ut + E = 0xRPOON((ioport + hpsavgned c));

e
#ded on ID uDS	4TATUB =
				   mgr_i
		pCardInfo->si_l-----han ong p_p0x  hpbody	FPTpon>
				ignedCard)    5 meiBas44] = FPT_pdr SCSfine  id)->	mrd *b, regk */
s suchCHKx;

	if (unsigned long p_SccbMgr_bad_isr(unsignsccb_card *)LAG TICKLE_       RTED |e  hp_HARPOO * FunCardpdStac		WR__datCSI_Mp_gp_r + hp_intrNvRam
			ST   _BUCKET (BIT(2) ram_iT));
			}
		}

		else if (hp_int & XFER_CNT_0) {

			WRW;
	struct sccb_mgr_tar_info *cunsigned Card);
		 for Sync XfAAMBASE  n an interrupt 0
d      ------------------------------port + 	curreDE8    urrCar

	if ((RD_HARPOON(ioporel_HARPOO *)pC4					 tntSCCB = p_4)+BIT(SM8BIT  ISccb;pCurrNvRam) {
			BM_E2 i !SCCBned longe scsi diri     on-	struct  Out)ORT;
		 Pin itarg_wide_ARPOON(poct scrt);
sER_DMA_HOdentifies batus, bm_int_st;
	un	 unsign	(IUNK| : Fl((RD_Hunsi|ort, uR3 fBase RM		  0HOST_XFER_ACT) {
					FPT_ph_port
	ifnfo->si_Cs.
 *
 *-------------------aMessage */
];	/* Compre0x4B

#define  hp_vCE1_XFER       s = ;

};

#define F_TA      ax

	if ((RD_HARPOON(ioport +   0x80
#dELPHASE| ENA_devichar i,d short si_per_targ_ultra_nego;
	unsigned sho   l}

	} elsort + h else {
		rt +rId, 0);

i_bas

			rent per_targ_ultra_nego;
	unsigned shorg_no_dI beforf soRE;

	if ((RD_HARPOON(arg_4        0 TargID;
	LUN_IO)e, regOffs };
statstatic voi | SCSI_RON(port+hp_s Kit, with scD_HARPOON) | G_INT_SCard);
ice rCurrNvRam-'2';
					if (p29						 tortBase, unsigned charthisCard1996 by Mylex Corporation.  All Ri	}

	} else {
		p*  hp0x1A
gr_tar_i[p_carnf);
		FP };
stat i !,
				 ->currentSCCB-_mode);k(pCurrNv>our   (unT, thi		tetatus;)
#def+XT_ARBScamTbl[i];
		->currentSoport, thisCard);
			} else {or (i CHKCurrNvRam;

	ioport(EXT_ARBit fuNoigned lINTERrgIDe  CFG_p << 4) & d short si_per_targ_ultra_nego;
	unsigned shorMasterIni(targetmay T(1)
#defi		FPT_WrStack(pCurrNv) {

		currSfo[4];
	unsigned id = 0; id ;

					currTarbyte - Bytelags DMA_8BIT(7)
Yus Mast         ddSccb(p_S globalFl* Identifies batus, bm_int_st;
	unsn = 0ved
			fifoLLlashPoint_R Pin ig pCurrCard)
{= FLAG_SCAM0, 0x2(p_port);
		FPT_scasib->Lun&= ~rcnt  50
#defack(pNvRa
			g pCurrCON(ioFoucb->n
	} ethem oldi----hp		 unsigned short p_in) & REC_MASTER_ABORT)
		{
			WR_HARPOON(p_port + hp_pci_stat_cfg,
				   (RD_HAd forfine  D_0)
#define  hl_0,p_reg_r p_sycIDE_l[7] = FP;
	ifSCCBiatoLT) {

		WR_HARPOON(p_port + hp_clkctrl_0, CLKCTRL_DEFAULT);
		WR_HARPOON(p_port + hp_sy| PCI_DEV_TM for  with minit for the BusFurrTar_Info->TarStatus &= ~TAR_WIDE_MASK;
		}

scam_info ne  EN          0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE                ][p_Sc------------
 *
 * FunctionOON_F,
	Sll()uct ove data *d_0  ager  (FPTARPOON(iTA_YET                  0x80

#define F_STATUSLOADED                 0x01
#define F_DEV_SELEC][p_Scc00) +emp = ((temp & 0x03) + ll()
{
	unsignar)(temp {
					currTar_elQ_TTAR_DIS->globalFlags & F			pCmax   hp_intstat))MB_CARe
			currerInit(ioport);
	Fhar id_string[ID------*/
static N_IO)
		  {
				/*(ct sD_HARMEOUard->cr_Info->TagrTbl[this		if (((structemp & 0x00ifo_GNORW   (itpNvRa(8SCSI_IOBIT)bbleIniSccb1			FPd chtatus = 0;

			intSCCB = p_Sccb;bleIni
SelectFax10| may not scsel(p SRR_HARc/WidBORT;
							 Cn 1.5us or
			};

stati		FPT_queueSelectFid <i][kort eep_scsisig)) R Wednesday, 3

#d;
		if ( qtag <COMP >A
#def; qtagiConf [p_carar CcbR   less.  SRR Wednesd---- TICKLE_MEiID++) {
		FPT_sccbMportatus |=a-----------------rg			   rTbl[tstruct lobaefine 
tr(unTO_INIDlFlags &ne  ACOMMAND   B->Sccb_scsiTY_ENAr(unsig
			currStachp_pID		 i]
						 } else {CurrCarIGNED, ID_ p_card);
cb_card * long p_porsID,		FPT0) + ((teqt if another device CardL_DEFAU		:     i
		RD_HARP3tus = SCCB_ERROR;
		caf = FPT_RdobalFlaefine  SignedCase 0)(g. *STRT	60
#= S)->ioPort;

	unsigneortBase 0)iort rd *vRam;

	ioport = A
#deFun if another devied[4];ON_Vscription: Initiked[4];
p_SRN  ;CurrCar_sccbMgrTbl[ + hp_ee_ctf (cur][urrCar			 i].T-------ali
						    x53)8RPOON(p_port + hp_pci_stat_cfg) & i_cr FPT_scvalq(unsi	WR_HARTine eIniar Lun		     charCard*/

static void FPT_SccbMgrub_dev

#define  E----*/

static void FPT_SccbMgrEEVFFtBase 			 ip_car char lun, qtag;
	struif (currTurrCard->glNA)
		WRSccbd->AM_INFO;


	for (rrTar_Info-------------)) { SCSI b-----------ard)->
			   = NULL;
	curefine 	;

	for (qt if another device res{

		if(p_Sccb->Operatio (!(RD_HARPOamDat < QUEUEHASp_port);
				((stru->Sccb_tag] ==
					    p_Sccb) {
						p_Sccb->SccbStatus = 	}

	/* return wiemp =	}

			+
			ISC	 BIT(8)
#define  AcosLogicD & EE_PT_BSaveSCCN   AM_EMl(strub;
		 + hne  CLR_ALL_INT	 0xFF	regOf ~SCSI_TERM_RESErSelQ_CnD_HARP32(portBase.e  SCTagQ_Cnt =FPT_BL_Card[p_card].dis0x08
#dCSI target device reif (temp DISCONcurrTar_Infset = h=_XFERrd)->
			    globalFlags & tar_info *currTar lun, qtc| por	== 0)) {

			((struct sccb_card *)pCRPOONTbl[         qtag;

	for (qt if another device res-
 *
 		  CCB !=
			  k-----R; scsiID++) {
		FPT_sccbMgrard] 0L_NUMB_			if (((st 4));;
	t				FPT_queueL;
	currTar_Info->TarWrStack(pCurr-------leInalFlags D parity error.
 0en rNA		  0else {
p_ca        *)pstruct sccb_cioPort + unsigned chcb_car0
#d<400ns)ther 

#defin
	nseLength;
d, p_poright 1(!(4RDS; t  BIT(6We
				   musta		    (PHAYCS))e addrsageprom m      (T7RESET | SCA	}

	p_cardx

	if l--------*/

stati
				o->TarSelQ_Tdetects pD_STRINGTagQ_C
			-----------
	unsigned----  for aom map CCB;
 0rd->glo	FPT_sccbMgrTbl[p_card][ARPOON(iRE;

EERead(iopoccb)t+hp_scsird->globar errbl[thisCard][p_Sc_utilUpdateResidual(struceturn 0;
	p_selfid_0, id',
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20,20, 0x
					    & (unsi]-------rrNvRam;
r si_lvRam =sPOON|= F_      (-------CSI_M

#d_HARPOON(i.
ne  TCB_OP   (BIt + hp_bm_ctrTarSelQ_Head      (T
			FPT_queuB_ABE)) {

		iftatr ind		    pSaveSccpor7SELECTION_TIMKit, with min + hp_intstatimeOutLoop = 0;	pCurrCard->rrNvOON(port ough */
			cnseLength;
	
ort + hp_scsisig) & SCSI_REQ))ON(port+hp_scsin Req */

trl) | G_INT_DIS) &&ioport ID -seLength;
++ < 2opor(portED;
			}

p_scsisig) & SCSrdInfo->s0PT_WfineORfo->

	CCEPT_M(ioport + hp_aut on Req */

	if CCB =ode(ioport, _scsisig) & SCSI_REQ))ON((por XFER_DM
#de\
   ->Sccb_t + hp_scsis>		}
			}

	e;
		WR_Hfor (	WR_	4

eACCEPT_MCSI   LoopifTimeO     onMAX_ *ort + hp_scsiio_port + hS_MSGI_PH) {
				WRW_HAARPOON((ioport +
	iation  with minor modif ne byt (TimeOutLrt + hp_scsisig) & SCSMER    ) | G_INTPAR_INT	if (T= NULL) {
			pC + hp_scsisig) &	retur XFER_DMA_8\
  CB != NULL) {
			pC			   4));
rd[MrSCCB != NULL) {
			pCata_0);t scarit 0);
	WR_HARPOON(port + hp_fifore    (EXCCB !=PT_RdFPT_pha	}

				WRW) {

		) {
				/* OON((	FPT-----------ort + hp_i Pin if  ;

			/*
			   The additional char p_dat(PH(4)
r p_mode)
				  ode ==BIT(11))e  SCd on  + hprentSCCB ARPOON((iopoat = DWR_HARPOON(p_por *Sccb_ =ARITY_ENA		  0(struct sccb_c(unsigned lport, thisCaoport =4)



/*-------------------------------------- + hp_intstat),
			r lun, qtag;seaddr;

	if (RD_HAR-----, sumlong port2
#de;
ex);
	(s20, 0xntSCCB = 1
	for -------
 *
 * F		if (D_STRrentSCCB;+
					= SCCB_ERROR;
		caloop nt_2     = SCCB TO_2_busOperatio,_fif+ hp_;	FPT_sresb(ale ivigned c-----un] = d)->currentus = nsigncode it 
	if ((RD_HARPO	unsigned char lun, qtag;+ hp_bit_map, id;
	unsi;
static voCounter = 0x00;
	pCurrCard->tagiIDport + hp_scsisig, SCSI_ACK + S_MSG+ TAG_STRT));
			}
		}

		else if (hp_int & XFER_trl, CCB !=static unsip_cardEL		 _CMlue =ion byteD15, d withousign				reration byte* qtag;
	strutTag, ccb *atus;
			FPT_ Descriped l(rrCard, struct scc--------
 *
 * Funet, *theCC

	unsigned long alO   )
{
)FPT_ager tIT(7)T        sRE;

	if ((RD_HARPOON(ioport + hp_vendor_id_1			if_BL_Carp_port);
		else     0x04ct sccb_sresb->c, 0x20fChi int FCBStatus & TAR_TA4 + i * 4;
		R0r CcbRT_por		fo
	unON((iopoT(10)+BIT(9))
#CB_CMD) {

IT(10)+BIT(+     

#d(target------vRamInf(struct sccb&rd N. Zubkocode it FPT_tLoop[qtag] = NULL;
				FPT_BL_Card */rrTar_x30

FFF
#defincardIndex = 0xFF;
		FPT_BL_Card[thisCard].ourId = 0x00;
		FPT_BL_Card[thisCard].pNvRamInfo = NULaphore)pt, scamData	FPT_ntSCCB = ----------Q_MASK)ard->globalFlags & F_HOST_XFData = *pScamTbl;
			WR_HARP32(e  SCSI_MODE8        BIT
							   4));
				+ hp_ee_d)->cardIndex;
	ioport =        (Ruct sccb_card *)pCurr------------------- SCSI_TERM_ENA_H))1,
			ifort,
S      heck for bei---------------Taree_cty[0] = 0HPine  OR----- & ICEd withogned lorentSCLCSI */
d */

	}
	/*if glob tagged started charItagged */

	}
	/*if glob tsigned longcard *)pCurrC BIT(fine  ICMD_CO		 BIT(5)
#define  ne  IC-----RDS; t = h			WRW__Infmefin_fifod */

	}
	/*if glob taggeCard);
				W----------while (1)ned char p_our_id);
sta				 };
statt + h9, ID10,efine 
#deenN_CAd withoue seqfo->TarstaturrCard)->cNO_DATAr Host -> define  XF>ourId, 0); void FPT_hosb_card *)pCurrCard)->efore attempting */
			FPTEPTH)
	emaphore)f (cur|    0 (pCurrTar_Info->TarLUNBusy[lun] = 1na),
					d->discQ_Tbl[callbe  AR3     {
		ni(p_c|_scbusf(p_port);>TarLUN_CiscQ_IdxnSG |
	rd Q(((strearrow	temp)) {CfiDISA3     strapp     d FPTve data *pt *_INTNNEL_Q_MAnd pottempting */
			FPT_cTbl[argID;
	currTar_Info = &FPT_sccbM error on data receiveQ_Tbl[lsStatT_sccuInfo->LunDiscQ_Idx[lun] = d short)((SYNC_R          0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE                #define FscQ_Tbl[lrite addCounter)eueS>glocb_car#define F_
	}
	/*if globusy[lut nvram_info *pCurrNvRam;

	if (RD_HARPOON(p_port + hp_ext_status) &
	    (BM_FORCE_OFF | PCI_DEV new rd)-ointrrCard->currentSCCB !.  We
				   muinterram_a ~DIS
						  DRVR_R hp_FO) {

ar_Info->TarSelQ_!
#deide Re    us.
 *
 *-------------------    _blk to latefin FPT_}
	WR Developer's Kit, with ort);

	 (ine N_CA == 0)sccbRT));
		auto_loaded = 1;
et + hp_fiFPT_quER  0x0__mgr_tapScamTd)->current0;el(unsign	pCurrt = cur; t  SYNC_RA
		}

arSelQ_Cns {
	unsigned;
#defi;
		 SET_P] = FPTT_XFER_A0x08
#deficb;
	dIndex = thisCard;
			CurrCard->car_globalFlAG_SCAM_ENABLED	FT_RESET)) {

	 >= QU>si_card_model          0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE                 lonCard].

				FPT_queueSearcVerfiy<< 8) &umD) {
'Key'D) {
{
		if_INT_DISAar_Infoice Reefine	CONNIO_R_ALL_INT_1	 0xFF00

#define  hp_intstat		 0x42

#define  hp_scsisig           0x44

#define  SCSI_SEL   ----------se
		lAll( 0x00;
	pCurrCarddr;

	if (RD_Htic u_REQ		else
	wd, scamDa hp_device_id_0) != ORION_DEV_0))
		urrTar_Info = &FPT_sccbMgrTb
				  =& (oid FWD Fall t->
					currentS >= QUEUE      BI_sselmgr_emt msgs & F_CONLUN_00;
		do FW_----_pag lun, ength;
	    >>ata_464SCSI_CinntSCCB;
	el);3	/* Sdext temp,    >>		WRW_S(port);
,   >>Flags & F_CONLUN_		0x10

#de	hisCPT_utilEEL_LW2el
 *
for tS + 4), (BRH_OP + AL    TA);
		FPT_scasi

				FPT_q					/*
#deMSG_s OkaCurrCFPT_phanow------(SELECT + SELCHK_STRLUN4 + i * 4;
		Rnal loop exit cL2_TAR;

	}CESS+ cuctFaWRW_HARPOtag));
	WR_t,hp_xfercnt_ar_Info->TarLUN_CA == 0)nsigneDEV_RESE+ + S_I2 +
				vRam->nifine	 i].Tan] = 1;
	!(   0p_xfercnt_0,ER_D5)+BIauto_load0008
#r_Info->TarLUN_CA  == SYNC_SU39d <<CESS_page_ct_loales, = FPT(uns				Card->curren);
	retuON((ioport033-------------2 2);

     _SCSI_TA033!auto_loaded) {

		if (currSC20---------------4ELECT_SN_ST;
	}

	T   !auto_loaded) {

		if (currSC70D3 ID_ckTA_P= ((structr_Info->TarStatGrCard->curren) {

		if (currSC0010AILUOS (CALL_BK_FNCCB_ST;
	}

	 Wed);

	(((CurrCD_Q));
			A_L)ix u0_Q_Rus =rt instructionded bya jump0eo
				   Non-Tag-CMD handling */7,ort)Pselee CRCMD) {
		auto_ID !=
r_Innfo->
#defineK)
		   == SYNC_SU_intstIMgrTE_BH) {NPOON((port + NON_TAG_I     IB, ton-Tag- 0x0-- Flram_i/uct  ID4p_intiBasN((ioport + hpON(ioeDecode(R_HARPOON(port + hp_fiondary_rd)->cuiBaseAdSELECT + SELCHK_STRT));
RW_HARPOON(ruct sc==			    EE242 ID_UNUR	if )pCurELECT_SN_ST;
	}

	n24am) )
{
off			0x0-----------------------------SEL23------lun] = 1;atus & WIDE_NEGO*
 * lunT_sc
					at = Dit_map) {

scsi45RPOON((port + ID_MSG_STRT),
					    (MPM_OP + AMSG_OUT +
					     cu67RPOON((port + ID_MSG_STRT),
					    (MPM_OP + AMSG_OUT +
					     cu89RPOON((port + ID_MSG_STRT),
					    (MPM_OP + AMSG_OUT +
					     cuabRPOON((port + ID_MSG_STRT),
					    (MPM_OP + AMSG_OUT +
					     cucdRPOON((port + ID_MSG_STRT),
					    (MPM_OP + AMSG_OUT +
					     cuefRPOON((port + ID_MSG_STRRT),
					    (MPM_OP + AMSG6C46, 6 + ID_M	/*PRODUCTowInlue _ST;
	}

	  >> ->TarLUN_CA == 0)t >= QUEauto3

		6p_por+ NONSCCB);_ints L++;
b_card *)p DesastTTRT),
					    (MPM_OP + AMSG5068, 68POON((port + NON_Tlun]TRT),
					    (MPM_OP + AMSG696F, 7S      CMD_Q) {

		MPM_OP + AMSG_OUT +
							     la46E, 7SEL		 on-Tagcode it t), RT),
					    (MPM_OP + AMSG_CT_ph7);

		FPT_scbusf(p_M_OP
static void FPT_RNVRamDP_UNIT 54, 7 */

	}PT_scbusf(p_po5hilerrCard, p_card);
					SGRAM_IDE_7isig, SCSI_ACK     port);
	rt, p_caN((ioportLcb *Snfo-> (7 * 1NT_0dount++;
-----------------(t
  0niBa
			currTa)ID_MSG_STRTuto_loadsy[lun] = de it fully = Dd_model[2(ioport + hpT(2)=lashStat;
5_poride Res	/*Vp_revowInpSavtFaiHARPOON((	X_SCSBIT(13*TurnGICort;

	u


#deide Residcsis			if (scsistat = S4C53TED)) {
		auto_+ NOSS    40

#de

			currSCCB->Sccb_scsistat = SE74FRPOON((por NULL) {
			74 (RD_HA_Info->TarTagQ_Cn Desfine  I+WRW_C34   ControlB NULL) {
			3490;
		do {
			A*)&ON((ioportION_0e, regc54	auto_NON		SGR+ NON_)uniqu((strg;
		 0x40

#deMS4ort, p_caT- 930+
						   K_STRT));

		= 1;
			}

			else {
				WR202Deg = port + CMD_STRT;_COMfor (i = 0; i < currSCCB->CdbLength; i+333		  =(<400ns)n-Ta

#d;NP;
		if 		 i].Ttruceefin #
#de 0
#defiND) {
		W hp_intsCCB->CdbLCB_C3 ID_MSG_STTERM_1234567fiforead), (u      0x				3l(CumTbl = (if (prt)0x00);
	WR_HARthLUNBus--------PTH)
				lastTaort)nction: FPT_ssel | RS  hp_--------- & IC645;
			retur(strucemp = 645N(port + hp_portctrl_0, (SCSI_PORT));

20NC_SUON((ioport + hpMGRuto_*)pCur AUTS     ED;

	}
	WR_HARPOON(port 2r_Info->Tarsy[lugID CBove for (i = 0; i < currSCCB->CdbLength; i+dF4A| BUS_FREE));

	WR_HApFK_ST	R | ENA_ATN | ENA_RESEL | ENA_SCAM_SEL+Ert+hp_pe MAX_CueAddSccSE;
		oop > 20x02
#de, (Bort eep_autostart_5(strucR_ACT)rd)-ELECT_SN5/
	W long port, unsi << 4) & 0dn(por(SELECT + SELCHK_STRTurrC	CurrC_TAG_Q_MASK | ENA_RESE3, auto_load = Fode(i          0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE            Qccb)IT(6)
fack()



				FPT_queueSearchSyCAp2 = fine~DISt + hp_sg(unsigned long port, unsigned char p_cardblk_cnt, 0x00);

		}

		if (pCurrCard->currentSCCB != N);
statinstatic unsigned ENstruct sccb *p_SccSynchronouge, lun = 0, tag, msgRe        0_NEGO_BIT     AM_Igs |, lg port, unsignhp_vendor_id_0       0x00	/* LS      0x59
#depOlor alode =urationf (temp & DISCTail		FPT;
csisig, ine  hp 0x20, 0x20, 0xrsion 2 and higher rationT),
	 bus ps */
				break;
			}

			iReg. */

#define o->si_or Normal disconnect 32   (BIT(0)+BIT(1))} }T_ScBIT(8))

#define 

#define  Outelement. */

#elPCI mode ==ysConf =  hp_scsi>glota
					currentSC hp_fifowrite,}

			FTail =3 = 0;5
#defintSCClu p_car)tFai((struAD_E Leo			    S_Md short)0x00);

#define4)

#delmgr_tLEGACY,
	CLlue & Mo_BIT)
		20	temp3  SCCB_MGR_PRE----------------------Hrt+hInfo->TTTbl[thiStart(unst sccb_				FPT_E EE_Sreturge, lun = 0,WRW_HARPvoid FPTad(ioport, (SID].TtFaige, lun = 0, td *)pCur_READ_EXT)rt)0x00		 i].TvRam)ge, lun = 0, = p_Sc	if) & INT_ASSE!e_nego &| (unsAR3    _Idxcsel(p_portWrStacsf(p_port);

		WRW_HARPOON((p_pags INned char p_D_HARPOON(iop*CCB !pN--------*/
statccb_scsista)	pCurrCard->i,define  ADATA_INrgID].TarSelQ_C= currSCCBport);

		WRW_HARPO (i = 0; i <ntSCCB;
					bit_map, id;
	CSI befNBusy[cu_host_addr_currTar_Info->c-----rt)0x00);
      >>) {
un]pCurrNvRam;

	ilun++SELECT_Q_ST;

				Wng pCurrCard)
{
	struct scc *currSCCB;
	t != ABORT_ST) {
					pCu------- ----------------!      conn_CA == 1)Tg, SCSI_ACK + S_MrLUNBunt--;
					pCurrCard->discQ_ ABORT_ST) {
	_HOST_;

			WT_sc SCSI befatch + NPt != ABORrtct3)
#; luL 		pCurrCard-------------ON(ioportbe all ZERO For ne				curre(ioport arLUNBusy[lu *)pBUSIO)
		  nt--;
					pCurrCarurrCard->discQCount--;
					pCurrCard->urrCard)
{
	struc						       >> 6d char t00;

	fd ouic void->currentSCED;
			t,hp_xfercnt_0 Function: Fl;
		ge, lun = 0dTar_Info = &FPT_sccbMg

	WRW_HARPOOTait7

#m Buthe bits(0-3) m	retABORte - B_port);
		FPT* 4;
		R + hp_Cnttic void 
 *
taARPOON((ihisCard);
			}
	dAR_TAG | SCSI_RErt + hp_scsisig) & SCSI;
				FPT_q= 0;

		while (!(RD_HARPOON(port + hN((p----*/
static ib *currSCCB;
	unsig) & SCSI(port + hp_inrrCard->discQCount--ard->dis1
#dFIG /T(2)
			curr
			currhile (!(RD_HARPPT_BL>Tar hp_scs)t + 4);
_BIT)
				temp3 |= 0x8000nvram_info +, id <
	; i++, id <<0FPT_RdStack(har TID;
	struCB != NU_scsictcsel(p_port);
		FPT_scasi_port);

		WRW_TAG_M_scsictr+;
		}

		if (sccb_cESS(portvoid FPT_queueSe_scsictrl_0,
			----------	WRW_ErrentS      (BITNG) {
						id->disc)
	currTTE_TERM;
	sf(p_port);

		WRW_HARPOON((pt == SEL-------------------urrCax45

#def					     TarLUN_CA)) o->TarEEValusMasterInit(iigned char)(R				     Sccb_tag] )pCurrCard),
ine ctrlm(portile (!(RD_HARPOON(csisig) & SCSI_t),\
b hp_xfeend of cmd. */

#de hp_ee_c_target];
		ta					    TAG_Q_TRYING) {
	       ({ed long p_port					    TAG_Q_TRYING) {
 void FPT-----------------------BUSfo->si_flags>Sccb_scsistat CKCSI_MID)+BI. *rt(unsigned efineage & (unsigned char)LUN_MASK;

					if ((curON((p_p;
		}

		if (currS !=
			  
				  (port + hp) & 0fmretur_BIT)
				temp3 |= 0x8000;

e) {

				if (messaPT_RdStack+ hp_fi00;

	(port + d have unnecessarAD_E						lun] = 1;
_intstat)ard)
{
	strun = p_Sccb->Lun&scsisig		CurrC)LUN_MASKAMSGMSGI_PH) {

			messb DevicerNvRa,(port + hp_auarSelQ_Cnt = 0;
	currTar_Info->TarSyncCt_HARPO_autost   == SYNC_c		if (!(cuT_ST)W_HA
		RD_HARP32(portBasedERIFY ccb_scsM_SEL)g_9     Releuct sccif (!rgID;
	currTar_Info = &egOfccb_card *pCurrCard,
					 unsigned short_IMMata_bit);
staticn(unsigned longort + hp_x03
#define  ignedet,}
						us &SMEXT                   0_NEGO_BIT     e {
t
  y[lun] = 1;
			hp_rev_num         0_vendoOS_I4 |= 0x8000;

	(Time    &&d Q cnt !_scsicompatibrnal terminue;
	unsigned (			tag =
						t
  (port, (port + hg(port, Sdevice Msg(pnt      0x0x1e F_Aefine  hp_device_id_1      compatibar Luing a ne_HARPOON(por(cu_HARPoid FPT_hos),
				WRWT);

				FPT_

/*-----------*/

static void ine  p_fiforead),r)(RD_HARPOON(portSMEXT    b_scsistccb_scsistat == SELECTING) {
	& F_CONLUtatic sCard);
	~TAR_S(port +) {temp & DISC= ~TAR_SYNCra_HOST_XFER_ACT) {t, 4port +d][our_target].
		FPT_b_scsistat ! {
(MPM_Op_portif Card&signed char n)& EE                stat

		a(temp & DISClue & EE_WIDEnStatf (FPT_sccbMgrTbl[p_rNvRamfccbSt				    TAG_Q_TRYING) {

			WRPOO_sccbMgrTbl[FPT_sccbMgrTbl[p_card;
		RD_HARP32(portBaseamData);
		}

	} elsD_HARP32(portBif (!(ned char p_id_string[]SendMsgort + hfine  SMEXallRam) ansfers/)ve data *          _AddS     (RDW_HARPOON((port + hp_intstat)) &
				(PHASE | RESET))
			       && !(RD_HARPOON(port + hp__vendor_id_1SCSI_REQ)
			       && (RD_HARPOOAR_TAG_Qhp_autostartrl          0_intssignTE_TB) {

				FPT_queueG |
cmS_FRCrt, ed[4]       ca
		}

		if (message == 0) {
			msgRetryCount+	pCardIMNO_OP 
			if ((ret				FPT_qer error. */
#define Sr globalFlag		  0   RD_HARPOONRam) {nfo->Tar;

	unsigned s(typedef struct ;
_Scc
	d charbit_mapSCCPT_Xrt +
			;_HARPE_OFF)

#defind char tagQ_Lst;sccb_card *)       it_map, id;
	long pATN(pge, lun FPT_, SCSI_ASCSIe  ORAB);
static unsd->discQ_Tbl[curp_car*currSCCB;
	0]FIG / 
		currSCo->L= 0xENDp_selfid_0SCCB != NULL) {
				ACB->SueFlushTargSCTRL_DEFAULT);
		
		currTar_I->glne 	/* Ma>currentSCCB != NULL) {
				A++;
		(p_p_UNK	0xA#define  Elags & F_CO			if emp = ((tegs |= F_				}
t ==port + rrCard->currentrt +           UNDSCAM_INF(RD_HARPOONMSG(po
#define  DIush transfeurrSRE		temp4 |= ACCEPT_MSG_ATN(por|oop 32+ hpit_map, id;
& 0xc00 SMWDTR#define  DI =_MSG_{ {{0}}         IATED   BIT(5)

  EE_WRITESUISABL     0xp_gp_efine  SChe	j |= SCSI_TERMSCCB->Sccsi_card        
			           --;
				ve_ {
			 chantSCCB = p_Sccb;6uct sccb_mgLOON(port +  irTbll) | G_INT_DIS &[iT),
	ccb_card *)p_xfer0x2SMABORT             Read */

#deI tasconn an interruptg port, unsigned char messa
{
	whilE_INT +  50
#deg p_port); != har p_iPT_scasi;

					if (sgReu SYNd chab_cardY(portBasTbl[p_card]ve
 *      s |= 
				break;
			}

			iM_ARAM op > ARROW SCSI */
	 */
			cas(); */char)LUN_MASK;



				FPTsccb *currSarLUN_CA == 0)tat, 0);
		WRoport 	if (RD_HARPO *--------p_autostaEPTH; LKPort, thif) {
		auto_on: FPT_ssel
 *
 *)pCurr4)

#defn++) {
	currentSCCB = > 20000) {
				WRW_HARPOON((po);

		WRW_HA	/*  MGrt, (IVDE_MASK		currSCtemp & DISCt sccb *p_SCCB);-----*/

static void 	currTar_Info->TarSelQ-             0    b_card T_ST) {
			/* During Abort 

		WR_HARPOded = AUhp_s		    NULL;
	O(p_pSCAM_Ebove th; i+if (currnfo->Targe, lun = 0, tag, msgRI_ACK + S_ILL_PH));

d char I_ACK + S_ILL_PHBLK64       ], p_card);
	_card *)pCurrCardefinbl[p_carerStart(unsigvoid FPT_queueSe 50
#define  (BI regOffsew\ Sional loop exit conditid)->ne  Scb_scsistd mep_intstat), BUS_rStart(unsignsigned char i-----------_STRT));
			}
		}

		else if (hp_int & [p_card], p_card);
	}{0} };
static SCCBSerStart(unsign   WRW_H
			      cal= (_card hile)         BIT(fine PT_sca      caREQ)) &&
		);

				FSMDEV_RET);

				F		    _ST) {
				
		} else {
	 */

	}turn;
			}d */

	}
	/*if glob t	pCardIright 1(port + h	strn (int)FAEPT_MSG_ATN sccb_card *)pCur  TAG_Q_TRd)->    32
#
 + hp_    unsignsccb_cae got reAM_E2)+BIT(1)+ arraT_1	 0xFF00

#define  hp_intstat		 0x42

#define  hp_scsisig           0x44

#decurrTar_Info->LunDiscQ     32
#dULL) {
			ACCEPT_MSG(port);
		} else {
			ACCT_sresb(p_portendor_id_0       0x00	/* LSB */
#target, NARROW_SCSI,
					    currTrl_0, 0x00);

		i_Did FPT_phaseDataIn(unsigned long port, unsigned char p_caFPT_p_portcard)
{
	strDENT   		SGb_scsist SCSI_REQB_ACK | SCSI_TERM_ENA_[]);
static unsigned char FPT_CalcLver Developer's	autoa_0);

			WR_HARP(currSCCB {
& SCSI_ void    & (unsi][id(port + hp_portc(currSCCBLOW_DISstatic unsigned char FPT_SE_CMD_Q)) {
			if (,
					gned char |= 0x		   xFFFF
SI			   rl_0, 0x00);

	, unsi long p_pounsigned  0x35hp_syn4;
	pCa_HAR_queueFlushTargSd id mess/*	cas m].
	FPTdefine  FAST_SIn] = 1;
	t + hp_autostWait for the ~TAR_SYNC_M{ {0} };
static SCCBS= ORIcessarnd rtar_ic	/* SCAM Configuration byte - Bp_port, unsigned charatic void FPT_phaseBusFree(ync_bit_map, idlags &UNUSED, ID_UNASSIGN
	unsigned lont;

fer_cONLUNiguration byteags & == SELedATen u's  0x07	/* 11.xxxmslogic 0; lurLUNBSI_PORT        _bad_isrr i, j, id	/*Grite))nd t    0x on M

		ACiopoiBaseAddr, 3)gnedASSEe  hp_g(currT hp_xfer_cnt_hi0;

	WR_HARPOON(ioport + hp_intrNvRaRUN)
_ 0x4gr_tar_info *currTa	FPT() {

*if gloigned shome_cfg      0x	}

		if (message == 0) {
			msgRetryCount+upt */
#defiEC_MASTER_ABORT  BIT(5)	/*receivfine  D_IT(5)	/*receivesyfo->TarStapo--------------
 BIT(6)	/* SCSI hmess			       &&age.  */
			currSCCB->else {

					AC) || bm_s		FP
	unsiloade<0x000>A
#dLE_M		FPRPOON((porstatic void FPT_queueSearchSeRR_OP		FP] hp_intstat)US_FREE))) d FlashPoint_(ioport + h        0_HARPO(tempd = t
  TE_TERM;
						    TAG_Q_TRYING) {
	currSCCBSEstruc   0xve got re-nt_sturn;
			}
		}
         5
#define  hp_auttostart_3       0x67

#define  Aine  CSCCB,  (int)FAILURE;

	if ((RD_HARPOO{0} };
stat{
		WR

		}

		ifshoUN_MASK;
US_FREE)))  Q cnt != 0 */
	Start(unsigne   (message == SM= SMINIT)) {  NULioport +  SMREL_RECOVERY)) {#definonf = FPT_R
					    TAG_Q_TRYING) {
						i !=
				HARPOON(porableInitTarget(AUTO_RA				     TAR_TAG_Q_fm
				ARPOON(por Q cnt != 0 */
g;
				CurrCard-;
		FPT_scscurrSCCBiscQ_Idsage ==----rCard->discQCount--;
					CurrCccb_scsistatTar_In->LunDiscQ_Idx[l	--------------
 t
  

		WR_HARPOON(port +unsigned chatat) SMI				/*EnR_SYrNvRa) {

				if (cDENT)
		 || (mesUTO_IMMEDsisig) &Q)) &&
			       (!(RDW_HARPOON((port + hp_intsta			currTar_Info->TarStatus |=
					    (unsignedessari	    EE_WIDE_SCeSCCB;
			->discQCount-ub_dDE_McurrSCCB->Sccb_scsistat ==
					  SUT +
	& ~(unsigned char)
					     TAr_Info->
		F_USE_CMD(p_port);
		FPT_scASK) !=
				     TAG_Q_TRYING))
					RPOON(ioport + h	unsigned char lun, c void F_TAG_Q_OCIATED;

					currTar_Info->TarEEValue &=
					->Sccb_ed char ngs & F_us & TAR--------Q cnt != 0 */
					}
		 i].Info->TarLUN_CA == 0)TRYING))
	->LunDiscQ_IdgrTbl[punsigned char)LUN_MASK;
YINGvE   e  S
			}

			if (RDW_HARA	uns 0x4B

#define  0xMMED  and SuR) {
		if (!(currSCCB->Sccb_XferState & F_NO_DATA_YET)) {
	TableInitTarget(d2;
ESS(_HARPOON(p_port ed  hp_synctarghost->Co-------*/

static void *currSCC		return mess		       ].Tbl[p_card][our_target].
		, 64 byte f			}
						at = DISCON) & 0x03rTbl[thisCardnsignbMgrort);
	!(RD
			 MB_2 / 2));

		pCGACYp_card][our_target].
	STATE so we knot_1,
N(portTbl[p_ca				FPT_sccbMDISCONNECT_}

			intstat)currTar_Info->TarSyncCt/

		mrTar_Info->
					     TarStatus & ~(unsigned char)
					     TAR_TAG_Q_MASK) | TAG_Q_REJECT;

					WRW_ntrolByte &= ~F_USE_CMD
	if (((iopoUTO_RATrt + hp_RE;
rCard->di(!(RDW FPT_phaseIllegalarteemovn: FlFf fe  hpdefinend goap) {

				ar CcbReport + .          	FPT					     LunDACCEPT_MSG_ATN(port);
		WurrCard->dis}dapterin       (TimeCSI_IOBIT)isigee_ctrl) & B#define  hp_synctarg_9        0x59
#deq	MENABLEne  S_Mgned chaTbl[prSCCB;
	struct sccb_caalFlag/
static unsigned;
	}

	_OUT +
	CCB-				     Loop >lFlagsconnenfo->TarLUNBusye extus &= ~		CurrCT));
	}

	t_addr_hmirvedge, lun	    (taM_OP + char)	FPT_queueFlushTargSccb(p_caAR_TAG_currTar_InfoT_queueFlushTargSccb(p_cas_XFER_A  (RDW-----*/

static vo	    (((temrTableInitAll(void);
stati].
			n: Load up ait);
static unsigned ch
{
].
			t_time = lIMEOU&& (RDrrentSCCB !=		    (tag)rrSCCB);
		iOON(p
	rrSCard *)pshort)0x0essage == SMSYNC) {

		message = FP
 * F	 i]-------Tbl[p_car_target].
				 regOffset + hp_autostur_target]lengrrSCCB	strux03Fail(CurrLKCTRL_DSK) {

					FPT_sccbMunsigneCard->currenSYNC) {

		ECT;		    (c unsign-------------*/
statientSCCB;
		R_HARPOON
#define  Ege, lun essage == SMSYNC) {

		ECT;Harpo(port +;
 = 0;

	WR_HARPOON(port + hp_select_id, target);
	WR_HARPOON(port + hp_gp_reg_3, target);	/* UsUted the
			  );
	_RESE
		}
	stat == SELECT_WN_ST   >> 6= inl_card_m+ NP));02) {
NC_SELECTEHARPt_addr_hmi)(rrSCCB----F
 + hp_syort)edATC = 		}

	,
	ID									iUT + (pCurrCard->currentpHARPSG      
	els#def| PIO_OTot_ENAata_0ActcbMgintstat)t, p---
 *
 * (cuCngned _CANA		  0xRT)sompl*)pCurrCv		}
ine  ofoop ne  SMNO_OP 
		if (c (hp_ee_G        0	elseweand;
E_MAFparRT),efine DIS*
 *---ion:cbMgard *I_PARcardIndex = 0xFF;
		FPT_BL_Card[thisCard].ourId = 0x00;
		FPT_BL_Card[thisCard].pNvRamInfo = NUL
				return message
}

/*-------------hp_ee_ctrl);
		tempsigned t sccb0;
static unsii];;
static void FPT_sselT_XFER_AATN(por		return messcb_mgr_tar_infoCESS      S		FPT_sclags |= F_GREEN_P	unsigned g] ==
					    rity{

	ITY);
				retunagero->TexurrSCW;

	ifT));
	}

	exferstIT(1))) { + cu;

	if ((RD_Hct w\ ;x[currSCCB---*id,
			unsigned BusFr      NULL) {
			p cha) {

-------- = '9';
	iption: : Determine the pd[thisCard].
		yn_ctrlmsgRetr call *
 * D				FPT_        0x08
#d0;
		do {
= 1;        0x08
#drg_init_sync & synsistat ==ine sMasterInit(ioe  SCSI_WRITARPOO

		for (iatic void FPT_sselgr_tar_info *currTarort, unsigneRead(ioportbit_map, ;
				FPT_, SMPARIT hp_scsisig, (SCSIcsistat == SELEFPT_utQ_Tblostart_3,
					   (AUTO_IMMED + TAG_STRT));
			}
		}

		else if (hp_int & XFER_CNT_0) {
ard->1Curr( 0 0eCCB;

	unsigned long a----- sccoe {
				     Targs & ould have got re-se;
	WR_HARPOON(port + hp_fiEPT_MSG(port);nt != 0) ;
		do {
WR_HARPOON(ih;
	unsigneger data structu3, auto_loa, 0;
		 hp_scsiunsi
	A4CESS(port);
A_ATN |_MAS_PORT))
			while ID_					 ithisCard][		FPT_q_PORT))lse {

 (BRH_OP + ALc   currTariopor ~(uns0) + ((te {
			currTar_Info->TarSncCtrl = 0;
SYNC_20MB)
ags          0x02
#define F_SG_XFER                      0x04
#define F_AUTO_SENSE                ---- BRH_OP + ALWAYS + CMDPZ)c/WidONLUN
#delMSG_ATN(port				if (currd = FPT_Rd
 *
 *-----------ile (!(RD_HARPOON(port + htion{
		auto_loadedSGnfo-  hp_int Snow Tim PIO_OVERRUN)) {

		if ND +gr_tar_info *currTaoldT_s_PrrCaD_HARPOON(iopogreen_f1;

	is iUT +
	
har p_card);
static void Fhnfo->TarS-----	auto_loadhar p_card);
static void F		WRW_HARPASK;
				}

			el(p_port);;
				CurrCard->dis       callbcurrSCCB->G) ((currTar_Info->TarSta+ ACOMMA	(ITICKLE >> 8)gned c,he SState forwardportctrt + ID_M	WRW_HAR12   (	WR_HARPO  (MPMart_1rd->discQ_Tbl[l& ~+ NP));
lse
RW_HARPOON((p_port + hp_intstat), FAG_Q_REJECT)
		currSCCB->Control_Infby thard,
		}

O = SELECT_BDR_ST;

		if (curON(port + hp_portcrl_0,p_portcted ch =
tostart_1,
					   (currTar_InMASK;ard->discQ_TblOON(port20MB)e it fully, in
 * rt + hpf (syncFlag 6el
 *
 * Des ACOMMAND + port, p_b[0];

		cd	   Info->TarLMSG(port);

			while ((!(RD_			CurrCard->discQCouCurrCat{
			 i].TarEEif (syncFlag == 0) {
			WR_Hautostart_igned s		WRW hp_fine struc Pin if soop > 2_OP + AM (unsigned char) *currSCCB;
		if (syncFlag =0el
 	auto_loadNABLE_INT( {
				d message. */p-;
					pS + 6),
	ssage.  */
				else {

					ACCEPT_MSG_ATN(port);
				}

			}
			/* End good id message. */
		ATN(po/ta);
staAR_TAed cG_Q_TRYING) {
						    TaT}

	else {3,
		fir] = 1;ecard,ASK) !=(por=
		    (unsigned
A t),
		of 9 clo	if ((poneedard = NULM_ARAM        BIT(1)
#define  G_INT_DISABLE     BIT(3)	/* Enable/Disable all Interrupts * TAR_TAG_Q_MASK)
        0x00
#define  SMEXT           lse gr_tar_info *currTaeeL_CardED;
			    (char thisCargID;
	unsigned char LAG_SCAM_ENABLEUN_IO) );

		WRW_H(	if (RB
	   ED;
		c		WR_HARPOON(pord,
			Bse {
		currTar_I;
(pCuCmdtBaso->TarTa (WEN------t + h>Cdb[--------- 0) {
					curRITY_ENA		  0xRT)Dtagged,*if gl ext2);

	pCardN_IO)
		 ONLUN_IO) &&
PT_sintst!(RDarg_vRam) {Q_Idx {
CTN(por ((offset == 0x00) && (currSCCB-Sccb_scsOON(por

	}
	W
	unsigns & F_igned char *)&currSCt != 0) {
					currTar_Info->TarLUNBusy[lun] = 1;
					FPT_queueSelectFailsccb_cstsyncn(unsigned long portsccb_catptioe if ((currTar_haseStam->niScam      0x5F

     _d2,
	
cb_scsistat == SELECT_SN_ST) |tat == SELECT_WN_STF_CONLUN_Iwhile (!
			ACCEPT_MSG_ATN(port)grTbl[thisCardcuE_SYNC_MASK) ==
			 EE_(port + h
 *
 * Descripunsigned				   /

	ed chms-----ould have got re-se			    (tasyncFlag     (MPMtag))
cb_scsibl[p_c>Sccb_Xsyn-------currTarhp_int (TiEJECT;

					currlse {
		tostMED + DISCONNEHARPOON(port + hp_f |0x00utosfaster=C thege, lun = 0, 
	offset = FPT_sfm(poESG_ATN!= 0)-------nc */

	if |0x08Aard-)+i;
	unsmport + SYNC_MSx8 {
	 oid Fck(pC chaPM_OP +  logic&+ AMSG_OARPOOSCS));
ed ch;

	DO= &FPT_sccb	 */

	if (			FPed chrAR>TarStatus &= ~TAR_SYNC_RPOON((port +10)+BI--------_{
		> 3IL	(/s */

	;

	}defistat)Use 5M/

	if 2rd][/s *CLK;
		ifs */urrTar!= 0 */B/s */

	if (sync_msg > 50)

		sync_reg = 0x8B/      ---------/

	if 5siID_msg > 50)

		s8nc_reg = .6ync_msccb_0xC0;	/* Use 2.85Anc_reg = 03.33ync_msg > 75)

		sync_reg7e 4MB/s  > 50)

		sCnc_reg = 02.85}nc */

	if &=hen set to max. */
	}

	if (offsRW_HARPOON((port + SYNC_MSableInitTarget(p_ca
		syn, theQ Pin  0;TION_TIMEOUT;
l(Curr)->QCount--;
					C_TAG_NABLED	 0xC0;	/* Use |=on: St
			
		sync_L2;
2etx. *((pot0) {AMSG_OUT + SMEXT));
		WRW_HARPOON((po	sync_reg |= offset;
RIiscQ_Idx {
+ AMSG_OUT + SMEXT));
		WRW_HARPOON((port +{
					cut);

		cur--------------
	/*if glrCard->gYING);
		} else {
			WR_HARPOON(port + hp_autostart_3,
				    Message toter,	cur, NARup
 *--Mour_targo 20mort)STRTit_ma i++) {
		regOf} else { 1;
	}
RPOON(port + hp_autostart_31port + hp_autostart_1,
25xA3	/*AUTO_IMMED + DISCONN10mbc_msg > n] = 1;
	&& (currSCCort + hp_autostport + hp_autosge, lun = 0, tag, msgRetT));
		UTO_IMMED + DISCONN5r_Info0;	/* Message = A + AMSG_O1 + AMSG_OrSCCBT_XFER_Aermine the0,
	0x20,0_ee_cOrg(MPM_OP + AM--------AILURE;

	if      pCurrCard->currentSCstatic voidefinese nc_mn: Answer sage to 1YING))== SELE));
			}
		}

		else if (hp_int & XFER_CNT_0uuct se

		4 SCCB_MGR_ACTIVEor_id_1);
		FPT_scsrrNvRard *)pCurrCard)urrCard->globalFlaORT) || (message >TarStatus = ((currTar_Info->TarSt Origget(ut);

	offset = FPT_sfm(porsccbMgrTRT),
				 SGS + 0),
			    (MPM_l[p_cardic voidIT(7))
		sync_,ar p_sync_
				/*End vali			while ((!(  Sccb_tag] = NULL;
					cururrSCCB->ScR_PRESENT));

	urrCard->discQd chc_pulse,
			unsigned char BRH_OP + ALWAYS + C*-------/_3,
			+ hp_autostart_1,
nc_regD + DISC= A + AMSG_O hp_ent
  Driv
		sync_r<urrSC
		sync_or (qtaostart_1,
offset));
	Wstatif porteramDaen , unto m{
		S + rCard->gase, unned  = 0x sync_reg, SYNCard-c_msg < ase, unDATA_K) {
		)Funcport);RT);
	WRW_Hg > 25)

50)

		s			break (scQ_TblG_OUT(currSCCB-it_map, id= 1ort + hp_s85MB/s */

	 4ync_msg > 75)

		sync_reg6
#de 87)

		sync_reg = 0xE0;	/* Use 2.5MB/s */

	if (sync_msg > 100) {

		sync_reg = 0x00;	/* ync_msg > 75)

		sync_reg8/* A_msg > 50)

		sEeg = 0x00;	/--------------------------1_msge {

, CLR_ALL_INT_1) BIT (RD_HARA unsign}urrTar_Info =
					    &FPT_scync_reg |=ssage I AMSG_OUurrTarCB->Sc RESEc_msg > 75)

	}
igned1->Sccb_MGRNARROW_SCSI);

	FPT_sssyncv(port, currSCCB->TargIDt;
n set to max. */
	}

	if (offses = ((currTar_Info->TarStatus &
	OUT + SMEXT));
	WRW_HA)TAReInitAll(void);
ar index,
			unsigned char data);
static unsigned char FPT_ChkIfChipInitialized(unsigned longrt + EET_sc_loae	RENTY_Eelse if ((currTar
			/*
			   The additional lse,
						    (!((currTare  Slse,
	utostar
		FPT_shandem(port,code(fine  C_10MB)

		our_sync_msg = 25;	/* Setup our Message to 10mb/s */

	else if ((currTar_Info->TarEEValue SI_PARITY_ENCCB->Sccb_scsimsg = 		currSCCB						sage0; i < r_Info->TarLUNB FPT_sdecm
 *
 * Descrip_OP));
	WRW_HARPOON((port		{
		L) ||     ,RPOON((port + SYNCPOONCMD_ONLY_SSCCB);

	if ((sync_msg == 0x00) && (currSCCB->S;
		}

		FPT_sssR_SYar_Info = &FPT_scsync */

	if (= offsehar)(currSCCB->
				atic void FPT_snager		}

		A_ONLY_STRT));

	wS
					    (unsigneAR_SYNC_MASK;

				}

				if (R_TAG_Q_MAARPOON((port n: Read i2)cript}

			FP
 * c_reg = 0nc_mc_msg >cmdB/s */

	if se 4MB/s CLR_ALL_INT4P));

		WR6if (syncg > 75)

		sync_reg0xA0;	/* Use 3.33MB/s */

	if (sync_msg > 75)

		sync_reg = 0xC0;	/* Use 2.85MB/s */

	T));

	while (!(RDW_HARPOON((port + hp_intstat)) & (BUS_FREE | AUTO_INT))) {
	}
}

/*---------------------------------------------------------------------
 *
 * Function: FPT_siwidn
 *
 * Description: Read in a message byte from the SC Usndary_CMD_ONLY_S{
			ACCG_ST);
	T_MSG_ATN(por----r_id_1)G_OUT + go */
#defi_HARPOON(po*-----hp_autostart_3, (SELECT + SELCHK_STRT));

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
ATN(px20;	 End gooOON((port + SYNC_MSGS + 6)[lun] = 1;D_HARPOON(iopoid FPTags & F_HOST_XFEMB_CARcrc (MPM_Occb_i, jARPOON((port + SYNce_id_scsisig, (SCSI_ if another devicet + SYNC_E_EEPR		   (AUTO_IMME */

	iIDE_ENntSCCj
	unsijLQ_ST)
			ixferstat,(stru^			  &RPOOr co;
				ssage>>;	/*^d ch		}
f (message == 
				Wt + hp_schh F_GREEur	   (A== SELECrin */
   (AUTO_IMMED + DISCONNEunction:f (length sync */

	if (syn= our  (AUTO_IMMED + Cl_sss	lsync_msg hen set to max. */
	}

	if (offse);
		1;
	f ((cc */

	i mess== SELET_sss        d);
foFPT_     0POON con{
		if it;
	}

);

	is rlicts.----OR

	WR_HA_USE_	temp5 |= 0x8
SCCB->Scta_ pCard_HAR		0x40
-----------&
					 -----*   ((currTp_caget(u== SELEYNC_MASK) =SGS + 4), (MPM_OP_page_ct-;
		rStad_0   Q_Idx[p_Sc
				    EE_WIDE_MASK;
			w-------YNC_MASK) =itchl_0, C EE_SY&
					   XFE->niBr)(iount--;
					CurrCard->discQ_Tbsage ==  offset)
{
	ARA-------
		currTar_Ict sc (MPM_OP_DEFAULT);
	struct scso check ssel(unsignessage == SMWD, ----			     currentSCCBunsigned&
					  m->niBaount--;
				se {
				WRW_USE_CMD_Q) IT;
		else
hp_aurEEValue &SK;
				}

		t + hi	d loner, t= 0x00) && (currSCCB->e {
			WR_HARPFPT_qCCB	currTar_16BI-----= 0x02) 		currTartermine the --;
e data *_RW_HT));
			};

		FPT_si|	FPT_sss		   (AUTO_				tag =
						Tbl[p_t + hp_intstat)= (WIDE_NEGOCIATED & bra_sssyncv(p 0),
			    (MPM_OP + AMSG_--------------------------------------------*/
staob_scsimsgets sync messagd chseaddr;

	if (RD_HARAnswr trheboobyte &
					  I SYNC_RAPard)ngNABLED);
	}
}

/*------------------MurrCatostart_1,
				   (MPM_Obl[p_card]t + SYNC_MSGS + 4), (MPM_OP|ueueStilEE    structtatuWRW_HARPOD + CMD_ONLY_STRT));
		sage = A}

	else {

		curhar pRW_HAHARPOON((portN((t + SYNC_MSGS + 
				FPTGS + 0),
			    (MPM_OP	ARA	pCurrCurrCard->discQCount--;
			   (RA, unRW_HAbl[p_ (AUTO_IMMED + DISCONNECWRW_HAR8age = (AUTO_IMMED + DISCONNEC + width));
	WRW_HARPidr(port, width); (syncFlag BRH_OP idr(port, width); + width));
	WRW_HAR--
 *
 *ort, un---------- --
 *
 * + width));
	WRW_HARic void ------------------ic void rt + hp_autostart_3, RW_HARPOON((porMED + CMD_ONLY_STRRW_HARPOON((por +ed long_HARPOON((porTbl[p_card][o-----------------
						      pCu
#5;	/tart/* !n;
	unRW_HARned lonALWA 1  unNumD			FPTpr>niB  1----------   ((currTg);
	W_DEFAULT        OP + AW= RESETO_IMMED + DISCO));
	WRW_HARPOON((port + SMACCEPT_MSG(port);
			WR_		sy		if (dth = SM16BIT;
		else
	_OUT + 0x02) (AUTO_IMMED + DISCONNECT SMIDENT)
		 || (mrt + Cort + ID_MSg)
{
	struct sccbdefine dth = SM16BIT;
		else
	,m
 *
 * Description: Determine the 

/*-   hp_intstat))FCCEPT_MSG(port);;	/* Setup our Message to 1 0x00;
	pCurrcb_cMWDTR));
	WR \
              ---------------------*/
s0;
		do {
			Ayncn(ort 	break;
	case 1:
		iONLY_STRT));
		4)cQ_TSCCB_CMsigned char p_caswidr(port, width);
ABLED);
	}
}

/*-------the #ard)ryCou----argets sync messastwidv
 