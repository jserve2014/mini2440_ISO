/*
 * Copyright 2006-2007 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/****************************************************************************/
/*Portion I: Definitions  shared between VBIOS and Driver                   */
/****************************************************************************/

#ifndef _ATOMBIOS_H
#define _ATOMBIOS_H

#define ATOM_VERSION_MAJOR                   0x00020000
#define ATOM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSION (ATOM_VERSION_MAJOR | ATOM_VERSION_MINOR)

/* Endianness should be specified before inclusion,
 * default to little endian
 */
#ifndef ATOM_BIG_ENDIAN
#error Endian not specified
#endif

#ifdef _H2INC
#ifndef ULONG
typedef unsigned long ULONG;
#endif

#ifndef UCHAR
typedef unsigned char UCHAR;
#endif

#ifndef USHORT
typedef unsigned short USHORT;
#endif
#endif

#define ATOM_DAC_A            0
#define ATOM_DAC_B            1
#define ATOM_EXT_DAC          2

#define ATOM_CRTC1            0
#define ATOM_CRTC2            1

#define ATOM_DIGA             0
#define ATOM_DIGB             1

#define ATOM_PPLL1            0
#define ATOM_PPLL2            1

#define ATOM_SCALER1          0
#define ATOM_SCALER2          1

#define ATOM_SCALER_DISABLE   0
#define ATOM_SCALER_CENTER    1
#define ATOM_SCALER_EXPANSION 2
#define ATOM_SCALER_MULTI_EX  3

#define ATOM_DISABLE          0
#define ATOM_ENABLE           1
#define ATOM_LCD_BLOFF                          (ATOM_DISABLE+2)
#define ATOM_LCD_BLON                           (ATOM_ENABLE+2)
#define ATOM_LCD_BL_BRIGHTNESS_CONTROL          (ATOM_ENABLE+3)
#define ATOM_LCD_SELFTEST_START									(ATOM_DISABLE+5)
#define ATOM_LCD_SELFTEST_STOP									(ATOM_ENABLE+5)
#define ATOM_ENCODER_INIT			                  (ATOM_DISABLE+7)

#define ATOM_BLANKING         1
#define ATOM_BLANKING_OFF     0

#define ATOM_CURSOR1          0
#define ATOM_CURSOR2          1

#define ATOM_ICON1            0
#define ATOM_ICON2            1

#define ATOM_CRT1             0
#define ATOM_CRT2             1

#define ATOM_TV_NTSC          1
#define ATOM_TV_NTSCJ         2
#define ATOM_TV_PAL           3
#define ATOM_TV_PALM          4
#define ATOM_TV_PALCN         5
#define ATOM_TV_PALN          6
#define ATOM_TV_PAL60         7
#define ATOM_TV_SECAM         8
#define ATOM_TV_CV            16

#define ATOM_DAC1_PS2         1
#define ATOM_DAC1_CV          2
#define ATOM_DAC1_NTSC        3
#define ATOM_DAC1_PAL         4

#define ATOM_DAC2_PS2         ATOM_DAC1_PS2
#define ATOM_DAC2_CV          ATOM_DAC1_CV
#define ATOM_DAC2_NTSC        ATOM_DAC1_NTSC
#define ATOM_DAC2_PAL         ATOM_DAC1_PAL

#define ATOM_PM_ON            0
#define ATOM_PM_STANDBY       1
#define ATOM_PM_SUSPEND       2
#define ATOM_PM_OFF           3

/* Bit0:{=0:single, =1:dual},
   Bit1 {=0:666RGB, =1:888RGB},
   Bit2:3:{Grey level}
   Bit4:{=0:LDI format for RGB888, =1 FPDI format for RGB888}*/

#define ATOM_PANEL_MISC_DUAL               0x00000001
#define ATOM_PANEL_MISC_888RGB             0x00000002
#define ATOM_PANEL_MISC_GREY_LEVEL         0x0000000C
#define ATOM_PANEL_MISC_FPDI               0x00000010
#define ATOM_PANEL_MISC_GREY_LEVEL_SHIFT   2
#define ATOM_PANEL_MISC_SPATIAL            0x00000020
#define ATOM_PANEL_MISC_TEMPORAL           0x00000040
#define ATOM_PANEL_MISC_API_ENABLED        0x00000080

#define MEMTYPE_DDR1              "DDR1"
#define MEMTYPE_DDR2              "DDR2"
#define MEMTYPE_DDR3              "DDR3"
#define MEMTYPE_DDR4              "DDR4"

#define ASIC_BUS_TYPE_PCI         "PCI"
#define ASIC_BUS_TYPE_AGP         "AGP"
#define ASIC_BUS_TYPE_PCIE        "PCI_EXPRESS"

/* Maximum size of that FireGL flag string */

#define ATOM_FIREGL_FLAG_STRING     "FGL"	/* Flag used to enable FireGL Support */
#define ATOM_MAX_SIZE_OF_FIREGL_FLAG_STRING  3	/* sizeof( ATOM_FIREGL_FLAG_STRING ) */

#define ATOM_FAKE_DESKTOP_STRING    "DSK"	/* Flag used to enable mobile ASIC on Desktop */
#define ATOM_MAX_SIZE_OF_FAKE_DESKTOP_STRING  ATOM_MAX_SIZE_OF_FIREGL_FLAG_STRING

#define ATOM_M54T_FLAG_STRING       "M54T"	/* Flag used to enable M54T Support */
#define ATOM_MAX_SIZE_OF_M54T_FLAG_STRING    4	/* sizeof( ATOM_M54T_FLAG_STRING ) */

#define HW_ASSISTED_I2C_STATUS_FAILURE          2
#define HW_ASSISTED_I2C_STATUS_SUCCESS          1

#pragma pack(1)			/* BIOS data must use byte aligment */

/*  Define offset to location of ROM header. */

#define OFFSET_TO_POINTER_TO_ATOM_ROM_HEADER		0x00000048L
#define OFFSET_TO_ATOM_ROM_IMAGE_SIZE				    0x00000002L

#define OFFSET_TO_ATOMBIOS_ASIC_BUS_MEM_TYPE    0x94
#define MAXSIZE_OF_ATOMBIOS_ASIC_BUS_MEM_TYPE   20	/* including the terminator 0x0! */
#define	OFFSET_TO_GET_ATOMBIOS_STRINGS_NUMBER		0x002f
#define	OFFSET_TO_GET_ATOMBIOS_STRINGS_START		0x006e

/* Common header for all ROM Data tables.
  Every table pointed  _ATOM_MASTER_DATA_TABLE has this common header.
  And the pointer actually points to this header. */

typedef struct _ATOM_COMMON_TABLE_HEADER {
	USHORT usStructureSize;
	UCHAR ucTableFormatRevision;	/*Change it when the Parser is not backward compatible */
	UCHAR ucTableContentRevision;	/*Change it only when the table needs to change but the firmware */
	/*Image can't be updated, while Driver needs to carry the new table! */
} ATOM_COMMON_TABLE_HEADER;

typedef struct _ATOM_ROM_HEADER {
	ATOM_COMMON_TABLE_HEADER sHeader;
	UCHAR uaFirmWareSignature[4];	/*Signature to distinguish between Atombios and non-atombios,
					   atombios should init it as "ATOM", don't change the position */
	USHORT usBiosRuntimeSegmentAddress;
	USHORT usProtectedModeInfoOffset;
	USHORT usConfigFilenameOffset;
	USHORT usCRC_BlockOffset;
	USHORT usBIOS_BootupMessageOffset;
	USHORT usInt10Offset;
	USHORT usPciBusDevInitCode;
	USHORT usIoBaseAddress;
	USHORT usSubsystemVendorID;
	USHORT usSubsystemID;
	USHORT usPCI_InfoOffset;
	USHORT usMasterCommandTableOffset;	/*Offset for SW to get all command table offsets, Don't change the position */
	USHORT usMasterDataTableOffset;	/*Offset for SW to get all data table offsets, Don't change the position */
	UCHAR ucExtendedFunctionCode;
	UCHAR ucReserved;
} ATOM_ROM_HEADER;

/*==============================Command Table Portion==================================== */

#ifdef	UEFI_BUILD
#define	UTEMP	USHORT
#define	USHORT	void*
#endif

typedef struct _ATOM_MASTER_LIST_OF_COMMAND_TABLES {
	USHORT ASIC_Init;	/* Function Table, used by various SW components,latest version 1.1 */
	USHORT GetDisplaySurfaceSize;	/* Atomic Table,  Used by Bios when enabling HW ICON */
	USHORT ASIC_RegistersInit;	/* Atomic Table,  indirectly used by various SW components,called from ASIC_Init */
	USHORT VRAM_BlockVenderDetection;	/* Atomic Table,  used only by Bios */
	USHORT DIGxEncoderControl;	/* Only used by Bios */
	USHORT MemoryControllerInit;	/* Atomic Table,  indirectly used by various SW components,called from ASIC_Init */
	USHORT EnableCRTCMemReq;	/* Function Table,directly used by various SW components,latest version 2.1 */
	USHORT MemoryParamAdjust;	/* Atomic Table,  indirectly used by various SW components,called from SetMemoryClock if needed */
	USHORT DVOEncoderControl;	/* Function Table,directly used by various SW components,latest version 1.2 */
	USHORT GPIOPinControl;	/* Atomic Table,  only used by Bios */
	USHORT SetEngineClock;	/*Function Table,directly used by various SW components,latest version 1.1 */
	USHORT SetMemoryClock;	/* Function Table,directly used by various SW components,latest version 1.1 */
	USHORT SetPixelClock;	/*Function Table,directly used by various SW components,latest version 1.2 */
	USHORT DynamicClockGating;	/* Atomic Table,  indirectly used by various SW components,called from ASIC_Init */
	USHORT ResetMemoryDLL;	/* Atomic Table,  indirectly used by various SW components,called from SetMemoryClock */
	USHORT ResetMemoryDevice;	/* Atomic Table,  indirectly used by various SW components,called from SetMemoryClock */
	USHORT MemoryPLLInit;
	USHORT AdjustDisplayPll;	/* only used by Bios */
	USHORT AdjustMemoryController;	/* Atomic Table,  indirectly used by various SW components,called from SetMemoryClock */
	USHORT EnableASIC_StaticPwrMgt;	/* Atomic Table,  only used by Bios */
	USHORT ASIC_StaticPwrMgtStatusChange;	/* Obsolete, only used by Bios */
	USHORT DAC_LoadDetection;	/* Atomic Table,  directly used by various SW components,latest version 1.2 */
	USHORT LVTMAEncoderControl;	/* Atomic Table,directly used by various SW components,latest version 1.3 */
	USHORT LCD1OutputControl;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT DAC1EncoderControl;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT DAC2EncoderControl;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT DVOOutputControl;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT CV1OutputControl;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT GetConditionalGoldenSetting;	/* only used by Bios */
	USHORT TVEncoderControl;	/* Function Table,directly used by various SW components,latest version 1.1 */
	USHORT TMDSAEncoderControl;	/* Atomic Table,  directly used by various SW components,latest version 1.3 */
	USHORT LVDSEncoderControl;	/* Atomic Table,  directly used by various SW components,latest version 1.3 */
	USHORT TV1OutputControl;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT EnableScaler;	/* Atomic Table,  used only by Bios */
	USHORT BlankCRTC;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT EnableCRTC;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT GetPixelClock;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT EnableVGA_Render;	/* Function Table,directly used by various SW components,latest version 1.1 */
	USHORT EnableVGA_Access;	/* Obsolete ,     only used by Bios */
	USHORT SetCRTC_Timing;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT SetCRTC_OverScan;	/* Atomic Table,  used by various SW components,latest version 1.1 */
	USHORT SetCRTC_Replication;	/* Atomic Table,  used only by Bios */
	USHORT SelectCRTC_Source;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT EnableGraphSurfaces;	/* Atomic Table,  used only by Bios */
	USHORT UpdateCRTC_DoubleBufferRegisters;
	USHORT LUT_AutoFill;	/* Atomic Table,  only used by Bios */
	USHORT EnableHW_IconCursor;	/* Atomic Table,  only used by Bios */
	USHORT GetMemoryClock;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT GetEngineClock;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT SetCRTC_UsingDTDTiming;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT ExternalEncoderControl;	/* Atomic Table,  directly used by various SW components,latest version 2.1 */
	USHORT LVTMAOutputControl;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT VRAM_BlockDetectionByStrap;	/* Atomic Table,  used only by Bios */
	USHORT MemoryCleanUp;	/* Atomic Table,  only used by Bios */
	USHORT ProcessI2cChannelTransaction;	/* Function Table,only used by Bios */
	USHORT WriteOneByteToHWAssistedI2C;	/* Function Table,indirectly used by various SW components */
	USHORT ReadHWAssistedI2CStatus;	/* Atomic Table,  indirectly used by various SW components */
	USHORT SpeedFanControl;	/* Function Table,indirectly used by various SW components,called from ASIC_Init */
	USHORT PowerConnectorDetection;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT MC_Synchronization;	/* Atomic Table,  indirectly used by various SW components,called from SetMemoryClock */
	USHORT ComputeMemoryEnginePLL;	/* Atomic Table,  indirectly used by various SW components,called from SetMemory/EngineClock */
	USHORT MemoryRefreshConversion;	/* Atomic Table,  indirectly used by various SW components,called from SetMemory or SetEngineClock */
	USHORT VRAM_GetCurrentInfoBlock;	/* Atomic Table,  used only by Bios */
	USHORT DynamicMemorySettings;	/* Atomic Table,  indirectly used by various SW components,called from SetMemoryClock */
	USHORT MemoryTraining;	/* Atomic Table,  used only by Bios */
	USHORT EnableSpreadSpectrumOnPPLL;	/* Atomic Table,  directly used by various SW components,latest version 1.2 */
	USHORT TMDSAOutputControl;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT SetVoltage;	/* Function Table,directly and/or indirectly used by various SW components,latest version 1.1 */
	USHORT DAC1OutputControl;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT DAC2OutputControl;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT SetupHWAssistedI2CStatus;	/* Function Table,only used by Bios, obsolete soon.Switch to use "ReadEDIDFromHWAssistedI2C" */
	USHORT ClockSource;	/* Atomic Table,  indirectly used by various SW components,called from ASIC_Init */
	USHORT MemoryDeviceInit;	/* Atomic Table,  indirectly used by various SW components,called from SetMemoryClock */
	USHORT EnableYUV;	/* Atomic Table,  indirectly used by various SW components,called from EnableVGARender */
	USHORT DIG1EncoderControl;	/* Atomic Table,directly used by various SW components,latest version 1.1 */
	USHORT DIG2EncoderControl;	/* Atomic Table,directly used by various SW components,latest version 1.1 */
	USHORT DIG1TransmitterControl;	/* Atomic Table,directly used by various SW components,latest version 1.1 */
	USHORT DIG2TransmitterControl;	/* Atomic Table,directly used by various SW components,latest version 1.1 */
	USHORT ProcessAuxChannelTransaction;	/* Function Table,only used by Bios */
	USHORT DPEncoderService;	/* Function Table,only used by Bios */
} ATOM_MASTER_LIST_OF_COMMAND_TABLES;

/*  For backward compatible */
#define ReadEDIDFromHWAssistedI2C                ProcessI2cChannelTransaction
#define UNIPHYTransmitterControl						     DIG1TransmitterControl
#define LVTMATransmitterControl							     DIG2TransmitterControl
#define SetCRTC_DPM_State                        GetConditionalGoldenSetting
#define SetUniphyInstance                        ASIC_StaticPwrMgtStatusChange

typedef struct _ATOM_MASTER_COMMAND_TABLE {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ATOM_MASTER_LIST_OF_COMMAND_TABLES ListOfCommandTables;
} ATOM_MASTER_COMMAND_TABLE;

/****************************************************************************/
/*  Structures used in every command table */
/****************************************************************************/
typedef struct _ATOM_TABLE_ATTRIBUTE {
#if ATOM_BIG_ENDIAN
	USHORT UpdatedByUtility:1;	/* [15]=Table updated by utility flag */
	USHORT PS_SizeInBytes:7;	/* [14:8]=Size of parameter space in Bytes (multiple of a dword), */
	USHORT WS_SizeInBytes:8;	/* [7:0]=Size of workspace in Bytes (in multiple of a dword), */
#else
	USHORT WS_SizeInBytes:8;	/* [7:0]=Size of workspace in Bytes (in multiple of a dword), */
	USHORT PS_SizeInBytes:7;	/* [14:8]=Size of parameter space in Bytes (multiple of a dword), */
	USHORT UpdatedByUtility:1;	/* [15]=Table updated by utility flag */
#endif
} ATOM_TABLE_ATTRIBUTE;

typedef union _ATOM_TABLE_ATTRIBUTE_ACCESS {
	ATOM_TABLE_ATTRIBUTE sbfAccess;
	USHORT susAccess;
} ATOM_TABLE_ATTRIBUTE_ACCESS;

/****************************************************************************/
/*  Common header for all command tables. */
/*  Every table pointed by _ATOM_MASTER_COMMAND_TABLE has this common header. */
/*  And the pointer actually points to this header. */
/****************************************************************************/
typedef struct _ATOM_COMMON_ROM_COMMAND_TABLE_HEADER {
	ATOM_COMMON_TABLE_HEADER CommonHeader;
	ATOM_TABLE_ATTRIBUTE TableAttribute;
} ATOM_COMMON_ROM_COMMAND_TABLE_HEADER;

/****************************************************************************/
/*  Structures used by ComputeMemoryEnginePLLTable */
/****************************************************************************/
#define COMPUTE_MEMORY_PLL_PARAM        1
#define COMPUTE_ENGINE_PLL_PARAM        2

typedef struct _COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS {
	ULONG ulClock;		/* When returen, it's the re-calculated clock based on given Fb_div Post_Div and ref_div */
	UCHAR ucAction;		/* 0:reserved //1:Memory //2:Engine */
	UCHAR ucReserved;	/* may expand to return larger Fbdiv later */
	UCHAR ucFbDiv;		/* return value */
	UCHAR ucPostDiv;	/* return value */
} COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS;

typedef struct _COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_V2 {
	ULONG ulClock;		/* When return, [23:0] return real clock */
	UCHAR ucAction;		/* 0:reserved;COMPUTE_MEMORY_PLL_PARAM:Memory;COMPUTE_ENGINE_PLL_PARAM:Engine. it return ref_div to be written to register */
	USHORT usFbDiv;		/* return Feedback value to be written to register */
	UCHAR ucPostDiv;	/* return post div to be written to register */
} COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_V2;
#define COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_PS_ALLOCATION   COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS

#define SET_CLOCK_FREQ_MASK                     0x00FFFFFF	/* Clock change tables only take bit [23:0] as the requested clock value */
#define USE_NON_BUS_CLOCK_MASK                  0x01000000	/* Applicable to both memory and engine clock change, when set, it uses another clock as the temporary clock (engine uses memory and vice versa) */
#define USE_MEMORY_SELF_REFRESH_MASK            0x02000000	/* Only applicable to memory clock change, when set, using memory self refresh during clock transition */
#define SKIP_INTERNAL_MEMORY_PARAMETER_CHANGE   0x04000000	/* Only applicable to memory clock change, when set, the table will skip predefined internal memory parameter change */
#define FIRST_TIME_CHANGE_CLOCK									0x08000000	/* Applicable to both memory and engine clock change,when set, it means this is 1st time to change clock after ASIC bootup */
#define SKIP_SW_PROGRAM_PLL											0x10000000	/* Applicable to both memory and engine clock change, when set, it means the table will not program SPLL/MPLL */
#define USE_SS_ENABLED_PIXEL_CLOCK  USE_NON_BUS_CLOCK_MASK

#define b3USE_NON_BUS_CLOCK_MASK                  0x01	/* Applicable to both memory and engine clock change, when set, it uses another clock as the temporary clock (engine uses memory and vice versa) */
#define b3USE_MEMORY_SELF_REFRESH                 0x02	/* Only applicable to memory clock change, when set, using memory self refresh during clock transition */
#define b3SKIP_INTERNAL_MEMORY_PARAMETER_CHANGE   0x04	/* Only applicable to memory clock change, when set, the table will skip predefined internal memory parameter change */
#define b3FIRST_TIME_CHANGE_CLOCK									0x08	/* Applicable to both memory and engine clock change,when set, it means this is 1st time to change clock after ASIC bootup */
#define b3SKIP_SW_PROGRAM_PLL											0x10	/* Applicable to both memory and engine clock change, when set, it means the table will not program SPLL/MPLL */

typedef struct _ATOM_COMPUTE_CLOCK_FREQ {
#if ATOM_BIG_ENDIAN
	ULONG ulComputeClockFlag:8;	/*  =1: COMPUTE_MEMORY_PLL_PARAM, =2: COMPUTE_ENGINE_PLL_PARAM */
	ULONG ulClockFreq:24;	/*  in unit of 10kHz */
#else
	ULONG ulClockFreq:24;	/*  in unit of 10kHz */
	ULONG ulComputeClockFlag:8;	/*  =1: COMPUTE_MEMORY_PLL_PARAM, =2: COMPUTE_ENGINE_PLL_PARAM */
#endif
} ATOM_COMPUTE_CLOCK_FREQ;

typedef struct _ATOM_S_MPLL_FB_DIVIDER {
	USHORT usFbDivFrac;
	USHORT usFbDiv;
} ATOM_S_MPLL_FB_DIVIDER;

typedef struct _COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_V3 {
	union {
		ATOM_COMPUTE_CLOCK_FREQ ulClock;	/* Input Parameter */
		ATOM_S_MPLL_FB_DIVIDER ulFbDiv;	/* Output Parameter */
	};
	UCHAR ucRefDiv;		/* Output Parameter */
	UCHAR ucPostDiv;	/* Output Parameter */
	UCHAR ucCntlFlag;	/* Output Parameter */
	UCHAR ucReserved;
} COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_V3;

/*  ucCntlFlag */
#define ATOM_PLL_CNTL_FLAG_PLL_POST_DIV_EN          1
#define ATOM_PLL_CNTL_FLAG_MPLL_VCO_MODE            2
#define ATOM_PLL_CNTL_FLAG_FRACTION_DISABLE         4

typedef struct _DYNAMICE_MEMORY_SETTINGS_PARAMETER {
	ATOM_COMPUTE_CLOCK_FREQ ulClock;
	ULONG ulReserved[2];
} DYNAMICE_MEMORY_SETTINGS_PARAMETER;

typedef struct _DYNAMICE_ENGINE_SETTINGS_PARAMETER {
	ATOM_COMPUTE_CLOCK_FREQ ulClock;
	ULONG ulMemoryClock;
	ULONG ulReserved;
} DYNAMICE_ENGINE_SETTINGS_PARAMETER;

/****************************************************************************/
/*  Structures used by SetEngineClockTable */
/****************************************************************************/
typedef struct _SET_ENGINE_CLOCK_PARAMETERS {
	ULONG ulTargetEngineClock;	/* In 10Khz unit */
} SET_ENGINE_CLOCK_PARAMETERS;

typedef struct _SET_ENGINE_CLOCK_PS_ALLOCATION {
	ULONG ulTargetEngineClock;	/* In 10Khz unit */
	COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_PS_ALLOCATION sReserved;
} SET_ENGINE_CLOCK_PS_ALLOCATION;

/****************************************************************************/
/*  Structures used by SetMemoryClockTable */
/****************************************************************************/
typedef struct _SET_MEMORY_CLOCK_PARAMETERS {
	ULONG ulTargetMemoryClock;	/* In 10Khz unit */
} SET_MEMORY_CLOCK_PARAMETERS;

typedef struct _SET_MEMORY_CLOCK_PS_ALLOCATION {
	ULONG ulTargetMemoryClock;	/* In 10Khz unit */
	COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_PS_ALLOCATION sReserved;
} SET_MEMORY_CLOCK_PS_ALLOCATION;

/****************************************************************************/
/*  Structures used by ASIC_Init.ctb */
/****************************************************************************/
typedef struct _ASIC_INIT_PARAMETERS {
	ULONG ulDefaultEngineClock;	/* In 10Khz unit */
	ULONG ulDefaultMemoryClock;	/* In 10Khz unit */
} ASIC_INIT_PARAMETERS;

typedef struct _ASIC_INIT_PS_ALLOCATION {
	ASIC_INIT_PARAMETERS sASICInitClocks;
	SET_ENGINE_CLOCK_PS_ALLOCATION sReserved;	/* Caller doesn't need to init this structure */
} ASIC_INIT_PS_ALLOCATION;

/****************************************************************************/
/*  Structure used by DynamicClockGatingTable.ctb */
/****************************************************************************/
typedef struct _DYNAMIC_CLOCK_GATING_PARAMETERS {
	UCHAR ucEnable;		/*  ATOM_ENABLE or ATOM_DISABLE */
	UCHAR ucPadding[3];
} DYNAMIC_CLOCK_GATING_PARAMETERS;
#define  DYNAMIC_CLOCK_GATING_PS_ALLOCATION  DYNAMIC_CLOCK_GATING_PARAMETERS

/****************************************************************************/
/*  Structure used by EnableASIC_StaticPwrMgtTable.ctb */
/****************************************************************************/
typedef struct _ENABLE_ASIC_STATIC_PWR_MGT_PARAMETERS {
	UCHAR ucEnable;		/*  ATOM_ENABLE or ATOM_DISABLE */
	UCHAR ucPadding[3];
} ENABLE_ASIC_STATIC_PWR_MGT_PARAMETERS;
#define ENABLE_ASIC_STATIC_PWR_MGT_PS_ALLOCATION  ENABLE_ASIC_STATIC_PWR_MGT_PARAMETERS

/****************************************************************************/
/*  Structures used by DAC_LoadDetectionTable.ctb */
/****************************************************************************/
typedef struct _DAC_LOAD_DETECTION_PARAMETERS {
	USHORT usDeviceID;	/* {ATOM_DEVICE_CRTx_SUPPORT,ATOM_DEVICE_TVx_SUPPORT,ATOM_DEVICE_CVx_SUPPORT} */
	UCHAR ucDacType;	/* {ATOM_DAC_A,ATOM_DAC_B, ATOM_EXT_DAC} */
	UCHAR ucMisc;		/* Valid only when table revision =1.3 and above */
} DAC_LOAD_DETECTION_PARAMETERS;

/*  DAC_LOAD_DETECTION_PARAMETERS.ucMisc */
#define DAC_LOAD_MISC_YPrPb						0x01

typedef struct _DAC_LOAD_DETECTION_PS_ALLOCATION {
	DAC_LOAD_DETECTION_PARAMETERS sDacload;
	ULONG Reserved[2];	/*  Don't set this one, allocation for EXT DAC */
} DAC_LOAD_DETECTION_PS_ALLOCATION;

/****************************************************************************/
/*  Structures used by DAC1EncoderControlTable.ctb and DAC2EncoderControlTable.ctb */
/****************************************************************************/
typedef struct _DAC_ENCODER_CONTROL_PARAMETERS {
	USHORT usPixelClock;	/*  in 10KHz; for bios convenient */
	UCHAR ucDacStandard;	/*  See definition of ATOM_DACx_xxx, For DEC3.0, bit 7 used as internal flag to indicate DAC2 (==1) or DAC1 (==0) */
	UCHAR ucAction;		/*  0: turn off encoder */
	/*  1: setup and turn on encoder */
	/*  7: ATOM_ENCODER_INIT Initialize DAC */
} DAC_ENCODER_CONTROL_PARAMETERS;

#define DAC_ENCODER_CONTROL_PS_ALLOCATION  DAC_ENCODER_CONTROL_PARAMETERS

/****************************************************************************/
/*  Structures used by DIG1EncoderControlTable */
/*                     DIG2EncoderControlTable */
/*                     ExternalEncoderControlTable */
/****************************************************************************/
typedef struct _DIG_ENCODER_CONTROL_PARAMETERS {
	USHORT usPixelClock;	/*  in 10KHz; for bios convenient */
	UCHAR ucConfig;
	/*  [2] Link Select: */
	/*  =0: PHY linkA if bfLane<3 */
	/*  =1: PHY linkB if bfLanes<3 */
	/*  =0: PHY linkA+B if bfLanes=3 */
	/*  [3] Transmitter Sel */
	/*  =0: UNIPHY or PCIEPHY */
	/*  =1: LVTMA */
	UCHAR ucAction;		/*  =0: turn off encoder */
	/*  =1: turn on encoder */
	UCHAR ucEncoderMode;
	/*  =0: DP   encoder */
	/*  =1: LVDS encoder */
	/*  =2: DVI  encoder */
	/*  =3: HDMI encoder */
	/*  =4: SDVO encoder */
	UCHAR ucLaneNum;	/*  how many lanes to enable */
	UCHAR ucReserved[2];
} DIG_ENCODER_CONTROL_PARAMETERS;
#define DIG_ENCODER_CONTROL_PS_ALLOCATION			  DIG_ENCODER_CONTROL_PARAMETERS
#define EXTERNAL_ENCODER_CONTROL_PARAMETER			DIG_ENCODER_CONTROL_PARAMETERS

/* ucConfig */
#define ATOM_ENCODER_CONFIG_DPLINKRATE_MASK				0x01
#define ATOM_ENCODER_CONFIG_DPLINKRATE_1_62GHZ		0x00
#define ATOM_ENCODER_CONFIG_DPLINKRATE_2_70GHZ		0x01
#define ATOM_ENCODER_CONFIG_LINK_SEL_MASK				  0x04
#define ATOM_ENCODER_CONFIG_LINKA								  0x00
#define ATOM_ENCODER_CONFIG_LINKB								  0x04
#define ATOM_ENCODER_CONFIG_LINKA_B							  ATOM_TRANSMITTER_CONFIG_LINKA
#define ATOM_ENCODER_CONFIG_LINKB_A							  ATOM_ENCODER_CONFIG_LINKB
#define ATOM_ENCODER_CONFIG_TRANSMITTER_SEL_MASK	0x08
#define ATOM_ENCODER_CONFIG_UNIPHY							  0x00
#define ATOM_ENCODER_CONFIG_LVTMA								  0x08
#define ATOM_ENCODER_CONFIG_TRANSMITTER1				  0x00
#define ATOM_ENCODER_CONFIG_TRANSMITTER2				  0x08
#define ATOM_ENCODER_CONFIG_DIGB								  0x80	/*  VBIOS Internal use, outside SW should set this bit=0 */
/*  ucAction */
/*  ATOM_ENABLE:  Enable Encoder */
/*  ATOM_DISABLE: Disable Encoder */

/* ucEncoderMode */
#define ATOM_ENCODER_MODE_DP											0
#define ATOM_ENCODER_MODE_LVDS										1
#define ATOM_ENCODER_MODE_DVI											2
#define ATOM_ENCODER_MODE_HDMI										3
#define ATOM_ENCODER_MODE_SDVO										4
#define ATOM_ENCODER_MODE_TV											13
#define ATOM_ENCODER_MODE_CV											14
#define ATOM_ENCODER_MODE_CRT											15

typedef struct _ATOM_DIG_ENCODER_CONFIG_V2 {
#if ATOM_BIG_ENDIAN
	UCHAR ucReserved1:2;
	UCHAR ucTransmitterSel:2;	/*  =0: UniphyAB, =1: UniphyCD  =2: UniphyEF */
	UCHAR ucLinkSel:1;	/*  =0: linkA/C/E =1: linkB/D/F */
	UCHAR ucReserved:1;
	UCHAR ucDPLinkRate:1;	/*  =0: 1.62Ghz, =1: 2.7Ghz */
#else
	UCHAR ucDPLinkRate:1;	/*  =0: 1.62Ghz, =1: 2.7Ghz */
	UCHAR ucReserved:1;
	UCHAR ucLinkSel:1;	/*  =0: linkA/C/E =1: linkB/D/F */
	UCHAR ucTransmitterSel:2;	/*  =0: UniphyAB, =1: UniphyCD  =2: UniphyEF */
	UCHAR ucReserved1:2;
#endif
} ATOM_DIG_ENCODER_CONFIG_V2;

typedef struct _DIG_ENCODER_CONTROL_PARAMETERS_V2 {
	USHORT usPixelClock;	/*  in 10KHz; for bios convenient */
	ATOM_DIG_ENCODER_CONFIG_V2 acConfig;
	UCHAR ucAction;
	UCHAR ucEncoderMode;
	/*  =0: DP   encoder */
	/*  =1: LVDS encoder */
	/*  =2: DVI  encoder */
	/*  =3: HDMI encoder */
	/*  =4: SDVO encoder */
	UCHAR ucLaneNum;	/*  how many lanes to enable */
	UCHAR ucReserved[2];
} DIG_ENCODER_CONTROL_PARAMETERS_V2;

/* ucConfig */
#define ATOM_ENCODER_CONFIG_V2_DPLINKRATE_MASK				0x01
#define ATOM_ENCODER_CONFIG_V2_DPLINKRATE_1_62GHZ		  0x00
#define ATOM_ENCODER_CONFIG_V2_DPLINKRATE_2_70GHZ		  0x01
#define ATOM_ENCODER_CONFIG_V2_LINK_SEL_MASK				  0x04
#define ATOM_ENCODER_CONFIG_V2_LINKA								  0x00
#define ATOM_ENCODER_CONFIG_V2_LINKB								  0x04
#define ATOM_ENCODER_CONFIG_V2_TRANSMITTER_SEL_MASK	  0x18
#define ATOM_ENCODER_CONFIG_V2_TRANSMITTER1				    0x00
#define ATOM_ENCODER_CONFIG_V2_TRANSMITTER2				    0x08
#define ATOM_ENCODER_CONFIG_V2_TRANSMITTER3				    0x10

/****************************************************************************/
/*  Structures used by UNIPHYTransmitterControlTable */
/*                     LVTMATransmitterControlTable */
/*                     DVOOutputControlTable */
/****************************************************************************/
typedef struct _ATOM_DP_VS_MODE {
	UCHAR ucLaneSel;
	UCHAR ucLaneSet;
} ATOM_DP_VS_MODE;

typedef struct _DIG_TRANSMITTER_CONTROL_PARAMETERS {
	union {
		USHORT usPixelClock;	/*  in 10KHz; for bios convenient */
		USHORT usInitInfo;	/*  when init uniphy,lower 8bit is used for connector type defined in objectid.h */
		ATOM_DP_VS_MODE asMode;	/*  DP Voltage swing mode */
	};
	UCHAR ucConfig;
	/*  [0]=0: 4 lane Link, */
	/*     =1: 8 lane Link ( Dual Links TMDS ) */
	/*  [1]=0: InCoherent mode */
	/*     =1: Coherent Mode */
	/*  [2] Link Select: */
	/*  =0: PHY linkA   if bfLane<3 */
	/*  =1: PHY linkB   if bfLanes<3 */
	/*  =0: PHY linkA+B if bfLanes=3 */
	/*  [5:4]PCIE lane Sel */
	/*  =0: lane 0~3 or 0~7 */
	/*  =1: lane 4~7 */
	/*  =2: lane 8~11 or 8~15 */
	/*  =3: lane 12~15 */
	UCHAR ucAction;		/*  =0: turn off encoder */
	/*  =1: turn on encoder */
	UCHAR ucReserved[4];
} DIG_TRANSMITTER_CONTROL_PARAMETERS;

#define DIG_TRANSMITTER_CONTROL_PS_ALLOCATION		DIG_TRANSMITTER_CONTROL_PARAMETERS

/* ucInitInfo */
#define ATOM_TRAMITTER_INITINFO_CONNECTOR_MASK	0x00ff

/* ucConfig */
#define ATOM_TRANSMITTER_CONFIG_8LANE_LINK			0x01
#define ATOM_TRANSMITTER_CONFIG_COHERENT				0x02
#define ATOM_TRANSMITTER_CONFIG_LINK_SEL_MASK		0x04
#define ATOM_TRANSMITTER_CONFIG_LINKA						0x00
#define ATOM_TRANSMITTER_CONFIG_LINKB						0x04
#define ATOM_TRANSMITTER_CONFIG_LINKA_B					0x00
#define ATOM_TRANSMITTER_CONFIG_LINKB_A					0x04

#define ATOM_TRANSMITTER_CONFIG_ENCODER_SEL_MASK	0x08	/*  only used when ATOM_TRANSMITTER_ACTION_ENABLE */
#define ATOM_TRANSMITTER_CONFIG_DIG1_ENCODER		0x00	/*  only used when ATOM_TRANSMITTER_ACTION_ENABLE */
#define ATOM_TRANSMITTER_CONFIG_DIG2_ENCODER		0x08	/*  only used when ATOM_TRANSMITTER_ACTION_ENABLE */

#define ATOM_TRANSMITTER_CONFIG_CLKSRC_MASK			0x30
#define ATOM_TRANSMITTER_CONFIG_CLKSRC_PPLL			0x00
#define ATOM_TRANSMITTER_CONFIG_CLKSRC_PCIE			0x20
#define ATOM_TRANSMITTER_CONFIG_CLKSRC_XTALIN		0x30
#define ATOM_TRANSMITTER_CONFIG_LANE_SEL_MASK		0xc0
#define ATOM_TRANSMITTER_CONFIG_LANE_0_3				0x00
#define ATOM_TRANSMITTER_CONFIG_LANE_0_7				0x00
#define ATOM_TRANSMITTER_CONFIG_LANE_4_7				0x40
#define ATOM_TRANSMITTER_CONFIG_LANE_8_11				0x80
#define ATOM_TRANSMITTER_CONFIG_LANE_8_15				0x80
#define ATOM_TRANSMITTER_CONFIG_LANE_12_15			0xc0

/* ucAction */
#define ATOM_TRANSMITTER_ACTION_DISABLE					       0
#define ATOM_TRANSMITTER_ACTION_ENABLE					       1
#define ATOM_TRANSMITTER_ACTION_LCD_BLOFF				       2
#define ATOM_TRANSMITTER_ACTION_LCD_BLON				       3
#define ATOM_TRANSMITTER_ACTION_BL_BRIGHTNESS_CONTROL  4
#define ATOM_TRANSMITTER_ACTION_LCD_SELFTEST_START		 5
#define ATOM_TRANSMITTER_ACTION_LCD_SELFTEST_STOP			 6
#define ATOM_TRANSMITTER_ACTION_INIT						       7
#define ATOM_TRANSMITTER_ACTION_DISABLE_OUTPUT	       8
#define ATOM_TRANSMITTER_ACTION_ENABLE_OUTPUT		       9
#define ATOM_TRANSMITTER_ACTION_SETUP						       10
#define ATOM_TRANSMITTER_ACTION_SETUP_VSEMPH           11

/*  Following are used for DigTransmitterControlTable ver1.2 */
typedef struct _ATOM_DIG_TRANSMITTER_CONFIG_V2 {
#if ATOM_BIG_ENDIAN
	UCHAR ucTransmitterSel:2;	/* bit7:6: =0 Dig Transmitter 1 ( Uniphy AB ) */
	/*         =1 Dig Transmitter 2 ( Uniphy CD ) */
	/*         =2 Dig Transmitter 3 ( Uniphy EF ) */
	UCHAR ucReserved:1;
	UCHAR fDPConnector:1;	/* bit4=0: DP connector  =1: None DP connector */
	UCHAR ucEncoderSel:1;	/* bit3=0: Data/Clk path source from DIGA( DIG inst0 ). =1: Data/clk path source from DIGB ( DIG inst1 ) */
	UCHAR ucLinkSel:1;	/* bit2=0: Uniphy LINKA or C or E when fDualLinkConnector=0. when fDualLinkConnector=1, it means master link of dual link is A or C or E */
	/*     =1: Uniphy LINKB or D or F when fDualLinkConnector=0. when fDualLinkConnector=1, it means master link of dual link is B or D or F */

	UCHAR fCoherentMode:1;	/* bit1=1: Coherent Mode ( for DVI/HDMI mode ) */
	UCHAR fDualLinkConnector:1;	/* bit0=1: Dual Link DVI connector */
#else
	UCHAR fDualLinkConnector:1;	/* bit0=1: Dual Link DVI connector */
	UCHAR fCoherentMode:1;	/* bit1=1: Coherent Mode ( for DVI/HDMI mode ) */
	UCHAR ucLinkSel:1;	/* bit2=0: Uniphy LINKA or C or E when fDualLinkConnector=0. when fDualLinkConnector=1, it means master link of dual link is A or C or E */
	/*     =1: Uniphy LINKB or D or F when fDualLinkConnector=0. when fDualLinkConnector=1, it means master link of dual link is B or D or F */
	UCHAR ucEncoderSel:1;	/* bit3=0: Data/Clk path source from DIGA( DIG inst0 ). =1: Data/clk path source from DIGB ( DIG inst1 ) */
	UCHAR fDPConnector:1;	/* bit4=0: DP connector  =1: None DP connector */
	UCHAR ucReserved:1;
	UCHAR ucTransmitterSel:2;	/* bit7:6: =0 Dig Transmitter 1 ( Uniphy AB ) */
	/*         =1 Dig Transmitter 2 ( Uniphy CD ) */
	/*         =2 Dig Transmitter 3 ( Uniphy EF ) */
#endif
} ATOM_DIG_TRANSMITTER_CONFIG_V2;

/* ucConfig */
/* Bit0 */
#define ATOM_TRANSMITTER_CONFIG_V2_DUAL_LINK_CONNECTOR			0x01

/* Bit1 */
#define ATOM_TRANSMITTER_CONFIG_V2_COHERENT				          0x02

/* Bit2 */
#define ATOM_TRANSMITTER_CONFIG_V2_LINK_SEL_MASK		        0x04
#define ATOM_TRANSMITTER_CONFIG_V2_LINKA			            0x00
#define ATOM_TRANSMITTER_CONFIG_V2_LINKB				            0x04

/*  Bit3 */
#define ATOM_TRANSMITTER_CONFIG_V2_ENCODER_SEL_MASK	        0x08
#define ATOM_TRANSMITTER_CONFIG_V2_DIG1_ENCODER		          0x00	/*  only used when ucAction == ATOM_TRANSMITTER_ACTION_ENABLE or ATOM_TRANSMITTER_ACTION_SETUP */
#define ATOM_TRANSMITTER_CONFIG_V2_DIG2_ENCODER		          0x08	/*  only used when ucAction == ATOM_TRANSMITTER_ACTION_ENABLE or ATOM_TRANSMITTER_ACTION_SETUP */

/*  Bit4 */
#define ATOM_TRASMITTER_CONFIG_V2_DP_CONNECTOR			        0x10

/*  Bit7:6 */
#define ATOM_TRANSMITTER_CONFIG_V2_TRANSMITTER_SEL_MASK     0xC0
#define ATOM_TRANSMITTER_CONFIG_V2_TRANSMITTER1			0x00	/* AB */
#define ATOM_TRANSMITTER_CONFIG_V2_TRANSMITTER2			0x40	/* CD */
#define ATOM_TRANSMITTER_CONFIG_V2_TRANSMITTER3			0x80	/* EF */

typedef struct _DIG_TRANSMITTER_CONTROL_PARAMETERS_V2 {
	union {
		USHORT usPixelClock;	/*  in 10KHz; for bios convenient */
		USHORT usInitInfo;	/*  when init uniphy,lower 8bit is used for connector type defined in objectid.h */
		ATOM_DP_VS_MODE asMode;	/*  DP Voltage swing mode */
	};
	ATOM_DIG_TRANSMITTER_CONFIG_V2 acConfig;
	UCHAR ucAction;		/*  define as ATOM_TRANSMITER_ACTION_XXX */
	UCHAR ucReserved[4];
} DIG_TRANSMITTER_CONTROL_PARAMETERS_V2;

/****************************************************************************/
/*  Structures used by DAC1OuputControlTable */
/*                     DAC2OuputControlTable */
/*                     LVTMAOutputControlTable  (Before DEC30) */
/*                     TMDSAOutputControlTable  (Before DEC30) */
/****************************************************************************/
typedef struct _DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS {
	UCHAR ucAction;		/*  Possible input:ATOM_ENABLE||ATOMDISABLE */
	/*  When the display is LCD, in addition to above: */
	/*  ATOM_LCD_BLOFF|| ATOM_LCD_BLON ||ATOM_LCD_BL_BRIGHTNESS_CONTROL||ATOM_LCD_SELFTEST_START|| */
	/*  ATOM_LCD_SELFTEST_STOP */

	UCHAR aucPadding[3];	/*  padding to DWORD aligned */
} DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS;

#define DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS

#define CRT1_OUTPUT_CONTROL_PARAMETERS     DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define CRT1_OUTPUT_CONTROL_PS_ALLOCATION  DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define CRT2_OUTPUT_CONTROL_PARAMETERS     DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define CRT2_OUTPUT_CONTROL_PS_ALLOCATION  DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define CV1_OUTPUT_CONTROL_PARAMETERS      DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define CV1_OUTPUT_CONTROL_PS_ALLOCATION   DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define TV1_OUTPUT_CONTROL_PARAMETERS      DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define TV1_OUTPUT_CONTROL_PS_ALLOCATION   DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define DFP1_OUTPUT_CONTROL_PARAMETERS     DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define DFP1_OUTPUT_CONTROL_PS_ALLOCATION  DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define DFP2_OUTPUT_CONTROL_PARAMETERS     DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define DFP2_OUTPUT_CONTROL_PS_ALLOCATION  DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define LCD1_OUTPUT_CONTROL_PARAMETERS     DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define LCD1_OUTPUT_CONTROL_PS_ALLOCATION  DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLOCATION

#define DVO_OUTPUT_CONTROL_PARAMETERS      DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS
#define DVO_OUTPUT_CONTROL_PS_ALLOCATION   DIG_TRANSMITTER_CONTROL_PS_ALLOCATION
#define DVO_OUTPUT_CONTROL_PARAMETERS_V3	 DIG_TRANSMITTER_CONTROL_PARAMETERS

/****************************************************************************/
/*  Structures used by BlankCRTCTable */
/****************************************************************************/
typedef struct _BLANK_CRTC_PARAMETERS {
	UCHAR ucCRTC;		/*  ATOM_CRTC1 or ATOM_CRTC2 */
	UCHAR ucBlanking;	/*  ATOM_BLANKING or ATOM_BLANKINGOFF */
	USHORT usBlackColorRCr;
	USHORT usBlackColorGY;
	USHORT usBlackColorBCb;
} BLANK_CRTC_PARAMETERS;
#define BLANK_CRTC_PS_ALLOCATION    BLANK_CRTC_PARAMETERS

/****************************************************************************/
/*  Structures used by EnableCRTCTable */
/*                     EnableCRTCMemReqTable */
/*                     UpdateCRTC_DoubleBufferRegistersTable */
/****************************************************************************/
typedef struct _ENABLE_CRTC_PARAMETERS {
	UCHAR ucCRTC;		/*  ATOM_CRTC1 or ATOM_CRTC2 */
	UCHAR ucEnable;		/*  ATOM_ENABLE or ATOM_DISABLE */
	UCHAR ucPadding[2];
} ENABLE_CRTC_PARAMETERS;
#define ENABLE_CRTC_PS_ALLOCATION   ENABLE_CRTC_PARAMETERS

/****************************************************************************/
/*  Structures used by SetCRTC_OverScanTable */
/****************************************************************************/
typedef struct _SET_CRTC_OVERSCAN_PARAMETERS {
	USHORT usOverscanRight;	/*  right */
	USHORT usOverscanLeft;	/*  left */
	USHORT usOverscanBottom;	/*  bottom */
	USHORT usOverscanTop;	/*  top */
	UCHAR ucCRTC;		/*  ATOM_CRTC1 or ATOM_CRTC2 */
	UCHAR ucPadding[3];
} SET_CRTC_OVERSCAN_PARAMETERS;
#define SET_CRTC_OVERSCAN_PS_ALLOCATION  SET_CRTC_OVERSCAN_PARAMETERS

/****************************************************************************/
/*  Structures used by SetCRTC_ReplicationTable */
/****************************************************************************/
typedef struct _SET_CRTC_REPLICATION_PARAMETERS {
	UCHAR ucH_Replication;	/*  horizontal replication */
	UCHAR ucV_Replication;	/*  vertical replication */
	UCHAR usCRTC;		/*  ATOM_CRTC1 or ATOM_CRTC2 */
	UCHAR ucPadding;
} SET_CRTC_REPLICATION_PARAMETERS;
#define SET_CRTC_REPLICATION_PS_ALLOCATION  SET_CRTC_REPLICATION_PARAMETERS

/****************************************************************************/
/*  Structures used by SelectCRTC_SourceTable */
/****************************************************************************/
typedef struct _SELECT_CRTC_SOURCE_PARAMETERS {
	UCHAR ucCRTC;		/*  ATOM_CRTC1 or ATOM_CRTC2 */
	UCHAR ucDevice;		/*  ATOM_DEVICE_CRT1|ATOM_DEVICE_CRT2|.... */
	UCHAR ucPadding[2];
} SELECT_CRTC_SOURCE_PARAMETERS;
#define SELECT_CRTC_SOURCE_PS_ALLOCATION  SELECT_CRTC_SOURCE_PARAMETERS

typedef struct _SELECT_CRTC_SOURCE_PARAMETERS_V2 {
	UCHAR ucCRTC;		/*  ATOM_CRTC1 or ATOM_CRTC2 */
	UCHAR ucEncoderID;	/*  DAC1/DAC2/TVOUT/DIG1/DIG2/DVO */
	UCHAR ucEncodeMode;	/*  Encoding mode, only valid when using DIG1/DIG2/DVO */
	UCHAR ucPadding;
} SELECT_CRTC_SOURCE_PARAMETERS_V2;

/* ucEncoderID */
/* #define ASIC_INT_DAC1_ENCODER_ID                                              0x00 */
/* #define ASIC_INT_TV_ENCODER_ID                                                                        0x02 */
/* #define ASIC_INT_DIG1_ENCODER_ID                                                              0x03 */
/* #define ASIC_INT_DAC2_ENCODER_ID                                                              0x04 */
/* #define ASIC_EXT_TV_ENCODER_ID                                                                        0x06 */
/* #define ASIC_INT_DVO_ENCODER_ID                                                                       0x07 */
/* #define ASIC_INT_DIG2_ENCODER_ID                                                              0x09 */
/* #define ASIC_EXT_DIG_ENCODER_ID                                                                       0x05 */

/* ucEncodeMode */
/* #define ATOM_ENCODER_MODE_DP                                                                          0 */
/* #define ATOM_ENCODER_MODE_LVDS                                                                        1 */
/* #define ATOM_ENCODER_MODE_DVI                                                                         2 */
/* #define ATOM_ENCODER_MODE_HDMI                                                                        3 */
/* #define ATOM_ENCODER_MODE_SDVO                                                                        4 */
/* #define ATOM_ENCODER_MODE_TV                                                                          13 */
/* #define ATOM_ENCODER_MODE_CV                                                                          14 */
/* #define ATOM_ENCODER_MODE_CRT                                                                         15 */

/****************************************************************************/
/*  Structures used by SetPixelClockTable */
/*                     GetPixelClockTable */
/****************************************************************************/
/* Major revision=1., Minor revision=1 */
typedef struct _PIXEL_CLOCK_PARAMETERS {
	USHORT usPixelClock;	/*  in 10kHz unit; for bios convenient = (RefClk*FB_Div)/(Ref_Div*Post_Div) */
	/*  0 means disable PPLL */
	USHORT usRefDiv;	/*  Reference divider */
	USHORT usFbDiv;		/*  feedback divider */
	UCHAR ucPostDiv;	/*  post divider */
	UCHAR ucFracFbDiv;	/*  fractional feedback divider */
	UCHAR ucPpll;		/*  ATOM_PPLL1 or ATOM_PPL2 */
	UCHAR ucRefDivSrc;	/*  ATOM_PJITTER or ATO_NONPJITTER */
	UCHAR ucCRTC;		/*  Which CRTC uses this Ppll */
	UCHAR ucPadding;
} PIXEL_CLOCK_PARAMETERS;

/* Major revision=1., Minor revision=2, add ucMiscIfno */
/* ucMiscInfo: */
#define MISC_FORCE_REPROG_PIXEL_CLOCK 0x1
#define MISC_DEVICE_INDEX_MASK        0xF0
#define MISC_DEVICE_INDEX_SHIFT       4

typedef struct _PIXEL_CLOCK_PARAMETERS_V2 {
	USHORT usPixelClock;	/*  in 10kHz unit; for bios convenient = (RefClk*FB_Div)/(Ref_Div*Post_Div) */
	/*  0 means disable PPLL */
	USHORT usRefDiv;	/*  Reference divider */
	USHORT usFbDiv;		/*  feedback divider */
	UCHAR ucPostDiv;	/*  post divider */
	UCHAR ucFracFbDiv;	/*  fractional feedback divider */
	UCHAR ucPpll;		/*  ATOM_PPLL1 or ATOM_PPL2 */
	UCHAR ucRefDivSrc;	/*  ATOM_PJITTER or ATO_NONPJITTER */
	UCHAR ucCRTC;		/*  Which CRTC uses this Ppll */
	UCHAR ucMiscInfo;	/*  Different bits for different purpose, bit [7:4] as device index, bit[0]=Force prog */
} PIXEL_CLOCK_PARAMETERS_V2;

/* Major revision=1., Minor revision=3, structure/definition change */
/* ucEncoderMode: */
/* ATOM_ENCODER_MODE_DP */
/* ATOM_ENOCDER_MODE_LVDS */
/* ATOM_ENOCDER_MODE_DVI */
/* ATOM_ENOCDER_MODE_HDMI */
/* ATOM_ENOCDER_MODE_SDVO */
/* ATOM_ENCODER_MODE_TV                                                                          13 */
/* ATOM_ENCODER_MODE_CV                                                                          14 */
/* ATOM_ENCODER_MODE_CRT                                                                         15 */

/* ucDVOConfig */
/* #define DVO_ENCODER_CONFIG_RATE_SEL                                                   0x01 */
/* #define DVO_ENCODER_CONFIG_DDR_SPEED                                          0x00 */
/* #define DVO_ENCODER_CONFIG_SDR_SPEED                                          0x01 */
/* #define DVO_ENCODER_CONFIG_OUTPUT_SEL                                         0x0c */
/* #define DVO_ENCODER_CONFIG_LOW12BIT                                                   0x00 */
/* #define DVO_ENCODER_CONFIG_UPPER12BIT                                         0x04 */
/* #define DVO_ENCODER_CONFIG_24BIT                                                              0x08 */

/* ucMiscInfo: also changed, see below */
#define PIXEL_CLOCK_MISC_FORCE_PROG_PPLL						0x01
#define PIXEL_CLOCK_MISC_VGA_MODE										0x02
#define PIXEL_CLOCK_MISC_CRTC_SEL_MASK							0x04
#define PIXEL_CLOCK_MISC_CRTC_SEL_CRTC1							0x00
#define PIXEL_CLOCK_MISC_CRTC_SEL_CRTC2							0x04
#define PIXEL_CLOCK_MISC_USE_ENGINE_FOR_DISPCLK			0x08

typedef struct _PIXEL_CLOCK_PARAMETERS_V3 {
	USHORT usPixelClock;	/*  in 10kHz unit; for bios convenient = (RefClk*FB_Div)/(Ref_Div*Post_Div) */
	/*  0 means disable PPLL. For VGA PPLL,make sure this value is not 0. */
	USHORT usRefDiv;	/*  Reference divider */
	USHORT usFbDiv;		/*  feedback divider */
	UCHAR ucPostDiv;	/*  post divider */
	UCHAR ucFracFbDiv;	/*  fractional feedback divider */
	UCHAR ucPpll;		/*  ATOM_PPLL1 or ATOM_PPL2 */
	UCHAR ucTransmitterId;	/*  graphic encoder id defined in objectId.h */
	union {
		UCHAR ucEncoderMode;	/*  encoder type defined as ATOM_ENCODER_MODE_DP/DVI/HDMI/ */
		UCHAR ucDVOConfig;	/*  when use DVO, need to know SDR/DDR, 12bit or 24bit */
	};
	UCHAR ucMiscInfo;	/*  bit[0]=Force program, bit[1]= set pclk for VGA, b[2]= CRTC sel */
	/*  bit[3]=0:use PPLL for dispclk source, =1: use engine clock for dispclock source */
} PIXEL_CLOCK_PARAMETERS_V3;

#define PIXEL_CLOCK_PARAMETERS_LAST			PIXEL_CLOCK_PARAMETERS_V2
#define GET_PIXEL_CLOCK_PS_ALLOCATION		PIXEL_CLOCK_PARAMETERS_LAST

/****************************************************************************/
/*  Structures used by AdjustDisplayPllTable */
/****************************************************************************/
typedef struct _ADJUST_DISPLAY_PLL_PARAMETERS {
	USHORT usPixelClock;
	UCHAR ucTransmitterID;
	UCHAR ucEncodeMode;
	union {
		UCHAR ucDVOConfig;	/* if DVO, need passing link rate and output 12bitlow or 24bit */
		UCHAR ucConfig;	/* if none DVO, not defined yet */
	};
	UCHAR ucReserved[3];
} ADJUST_DISPLAY_PLL_PARAMETERS;

#define ADJUST_DISPLAY_CONFIG_SS_ENABLE       0x10

#define ADJUST_DISPLAY_PLL_PS_ALLOCATION			ADJUST_DISPLAY_PLL_PARAMETERS

/****************************************************************************/
/*  Structures used by EnableYUVTable */
/****************************************************************************/
typedef struct _ENABLE_YUV_PARAMETERS {
	UCHAR ucEnable;		/*  ATOM_ENABLE:Enable YUV or ATOM_DISABLE:Disable YUV (RGB) */
	UCHAR ucCRTC;		/*  Which CRTC needs this YUV or RGB format */
	UCHAR ucPadding[2];
} ENABLE_YUV_PARAMETERS;
#define ENABLE_YUV_PS_ALLOCATION ENABLE_YUV_PARAMETERS

/****************************************************************************/
/*  Structures used by GetMemoryClockTable */
/****************************************************************************/
typedef struct _GET_MEMORY_CLOCK_PARAMETERS {
	ULONG ulReturnMemoryClock;	/*  current memory speed in 10KHz unit */
} GET_MEMORY_CLOCK_PARAMETERS;
#define GET_MEMORY_CLOCK_PS_ALLOCATION  GET_MEMORY_CLOCK_PARAMETERS

/****************************************************************************/
/*  Structures used by GetEngineClockTable */
/****************************************************************************/
typedef struct _GET_ENGINE_CLOCK_PARAMETERS {
	ULONG ulReturnEngineClock;	/*  current engine speed in 10KHz unit */
} GET_ENGINE_CLOCK_PARAMETERS;
#define GET_ENGINE_CLOCK_PS_ALLOCATION  GET_ENGINE_CLOCK_PARAMETERS

/****************************************************************************/
/*  Following Structures and constant may be obsolete */
/****************************************************************************/
/* Maxium 8 bytes,the data read in will be placed in the parameter space. */
/* Read operaion successeful when the paramter space is non-zero, otherwise read operation failed */
typedef struct _READ_EDID_FROM_HW_I2C_DATA_PARAMETERS {
	USHORT usPrescale;	/* Ratio between Engine clock and I2C clock */
	USHORT usVRAMAddress;	/* Adress in Frame Buffer where to pace raw EDID */
	USHORT usStatus;	/* When use output: lower byte EDID checksum, high byte hardware status */
	/* WHen use input:  lower byte as 'byte to read':currently limited to 128byte or 1byte */
	UCHAR ucSlaveAddr;	/* Read from which slave */
	UCHAR ucLineNumber;	/* Read from which HW assisted line */
} READ_EDID_FROM_HW_I2C_DATA_PARAMETERS;
#define READ_EDID_FROM_HW_I2C_DATA_PS_ALLOCATION  READ_EDID_FROM_HW_I2C_DATA_PARAMETERS

#define  ATOM_WRITE_I2C_FORMAT_PSOFFSET_PSDATABYTE                  0
#define  ATOM_WRITE_I2C_FORMAT_PSOFFSET_PSTWODATABYTES              1
#define  ATOM_WRITE_I2C_FORMAT_PSCOUNTER_PSOFFSET_IDDATABLOCK       2
#define  ATOM_WRITE_I2C_FORMAT_PSCOUNTER_IDOFFSET_PLUS_IDDATABLOCK  3
#define  ATOM_WRITE_I2C_FORMAT_IDCOUNTER_IDOFFSET_IDDATABLOCK       4

typedef struct _WRITE_ONE_BYTE_HW_I2C_DATA_PARAMETERS {
	USHORT usPrescale;	/* Ratio between Engine clock and I2C clock */
	USHORT usByteOffset;	/* Write to which byte */
	/* Upper portion of usByteOffset is Format of data */
	/* 1bytePS+offsetPS */
	/* 2bytesPS+offsetPS */
	/* blockID+offsetPS */
	/* blockID+offsetID */
	/* blockID+counterID+offsetID */
	UCHAR ucData;		/* PS data1 */
	UCHAR ucStatus;		/* Status byte 1=success, 2=failure, Also is used as PS data2 */
	UCHAR ucSlaveAddr;	/* Write to which slave */
	UCHAR ucLineNumber;	/* Write from which HW assisted line */
} WRITE_ONE_BYTE_HW_I2C_DATA_PARAMETERS;

#define WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION  WRITE_ONE_BYTE_HW_I2C_DATA_PARAMETERS

typedef struct _SET_UP_HW_I2C_DATA_PARAMETERS {
	USHORT usPrescale;	/* Ratio between Engine clock and I2C clock */
	UCHAR ucSlaveAddr;	/* Write to which slave */
	UCHAR ucLineNumber;	/* Write from which HW assisted line */
} SET_UP_HW_I2C_DATA_PARAMETERS;

/**************************************************************************/
#define SPEED_FAN_CONTROL_PS_ALLOCATION   WRITE_ONE_BYTE_HW_I2C_DATA_PARAMETERS

/****************************************************************************/
/*  Structures used by PowerConnectorDetectionTable */
/****************************************************************************/
typedef struct _POWER_CONNECTOR_DETECTION_PARAMETERS {
	UCHAR ucPowerConnectorStatus;	/* Used for return value 0: detected, 1:not detected */
	UCHAR ucPwrBehaviorId;
	USHORT usPwrBudget;	/* how much power currently boot to in unit of watt */
} POWER_CONNECTOR_DETECTION_PARAMETERS;

typedef struct POWER_CONNECTOR_DETECTION_PS_ALLOCATION {
	UCHAR ucPowerConnectorStatus;	/* Used for return value 0: detected, 1:not detected */
	UCHAR ucReserved;
	USHORT usPwrBudget;	/* how much power currently boot to in unit of watt */
	WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION sReserved;
} POWER_CONNECTOR_DETECTION_PS_ALLOCATION;

/****************************LVDS SS Command Table Definitions**********************/

/****************************************************************************/
/*  Structures used by EnableSpreadSpectrumOnPPLLTable */
/****************************************************************************/
typedef struct _ENABLE_LVDS_SS_PARAMETERS {
	USHORT usSpreadSpectrumPercentage;
	UCHAR ucSpreadSpectrumType;	/* Bit1=0 Down Spread,=1 Center Spread. Bit1=1 Ext. =0 Int. Others:TBD */
	UCHAR ucSpreadSpectrumStepSize_Delay;	/* bits3:2 SS_STEP_SIZE; bit 6:4 SS_DELAY */
	UCHAR ucEnable;		/* ATOM_ENABLE or ATOM_DISABLE */
	UCHAR ucPadding[3];
} ENABLE_LVDS_SS_PARAMETERS;

/* ucTableFormatRevision=1,ucTableContentRevision=2 */
typedef struct _ENABLE_LVDS_SS_PARAMETERS_V2 {
	USHORT usSpreadSpectrumPercentage;
	UCHAR ucSpreadSpectrumType;	/* Bit1=0 Down Spread,=1 Center Spread. Bit1=1 Ext. =0 Int. Others:TBD */
	UCHAR ucSpreadSpectrumStep;	/*  */
	UCHAR ucEnable;		/* ATOM_ENABLE or ATOM_DISABLE */
	UCHAR ucSpreadSpectrumDelay;
	UCHAR ucSpreadSpectrumRange;
	UCHAR ucPadding;
} ENABLE_LVDS_SS_PARAMETERS_V2;

/* This new structure is based on ENABLE_LVDS_SS_PARAMETERS but expands to SS on PPLL, so other devices can use SS. */
typedef struct _ENABLE_SPREAD_SPECTRUM_ON_PPLL {
	USHORT usSpreadSpectrumPercentage;
	UCHAR ucSpreadSpectrumType;	/*  Bit1=0 Down Spread,=1 Center Spread. Bit1=1 Ext. =0 Int. Others:TBD */
	UCHAR ucSpreadSpectrumStep;	/*  */
	UCHAR ucEnable;		/*  ATOM_ENABLE or ATOM_DISABLE */
	UCHAR ucSpreadSpectrumDelay;
	UCHAR ucSpreadSpectrumRange;
	UCHAR ucPpll;		/*  ATOM_PPLL1/ATOM_PPLL2 */
} ENABLE_SPREAD_SPECTRUM_ON_PPLL;

#define ENABLE_SPREAD_SPECTRUM_ON_PPLL_PS_ALLOCATION  ENABLE_SPREAD_SPECTRUM_ON_PPLL

/**************************************************************************/

typedef struct _SET_PIXEL_CLOCK_PS_ALLOCATION {
	PIXEL_CLOCK_PARAMETERS sPCLKInput;
	ENABLE_SPREAD_SPECTRUM_ON_PPLL sReserved;	/* Caller doesn't need to init this portion */
} SET_PIXEL_CLOCK_PS_ALLOCATION;

#define ENABLE_VGA_RENDER_PS_ALLOCATION   SET_PIXEL_CLOCK_PS_ALLOCATION

/****************************************************************************/
/*  Structures used by ### */
/****************************************************************************/
typedef struct _MEMORY_TRAINING_PARAMETERS {
	ULONG ulTargetMemoryClock;	/* In 10Khz unit */
} MEMORY_TRAINING_PARAMETERS;
#define MEMORY_TRAINING_PS_ALLOCATION MEMORY_TRAINING_PARAMETERS

/****************************LVDS and other encoder command table definitions **********************/

/****************************************************************************/
/*  Structures used by LVDSEncoderControlTable   (Before DCE30) */
/*                     LVTMAEncoderControlTable  (Before DCE30) */
/*                     TMDSAEncoderControlTable  (Before DCE30) */
/****************************************************************************/
typedef struct _LVDS_ENCODER_CONTROL_PARAMETERS {
	USHORT usPixelClock;	/*  in 10KHz; for bios convenient */
	UCHAR ucMisc;		/*  bit0=0: Enable single link */
	/*      =1: Enable dual link */
	/*  Bit1=0: 666RGB */
	/*      =1: 888RGB */
	UCHAR ucAction;		/*  0: turn off encoder */
	/*  1: setup and turn on encoder */
} LVDS_ENCODER_CONTROL_PARAMETERS;

#define LVDS_ENCODER_CONTROL_PS_ALLOCATION  LVDS_ENCODER_CONTROL_PARAMETERS

#define TMDS1_ENCODER_CONTROL_PARAMETERS    LVDS_ENCODER_CONTROL_PARAMETERS
#define TMDS1_ENCODER_CONTROL_PS_ALLOCATION TMDS1_ENCODER_CONTROL_PARAMETERS

#define TMDS2_ENCODER_CONTROL_PARAMETERS    TMDS1_ENCODER_CONTROL_PARAMETERS
#define TMDS2_ENCODER_CONTROL_PS_ALLOCATION TMDS2_ENCODER_CONTROL_PARAMETERS

/* ucTableFormatRevision=1,ucTableContentRevision=2 */
typedef struct _LVDS_ENCODER_CONTROL_PARAMETERS_V2 {
	USHORT usPixelClock;	/*  in 10KHz; for bios convenient */
	UCHAR ucMisc;		/*  see PANEL_ENCODER_MISC_xx defintions below */
	UCHAR ucAction;		/*  0: turn off encoder */
	/*  1: setup and turn on encoder */
	UCHAR ucTruncate;	/*  bit0=0: Disable truncate */
	/*      =1: Enable truncate */
	/*  bit4=0: 666RGB */
	/*      =1: 888RGB */
	UCHAR ucSpatial;	/*  bit0=0: Disable spatial dithering */
	/*      =1: Enable spatial dithering */
	/*  bit4=0: 666RGB */
	/*      =1: 888RGB */
	UCHAR ucTemporal;	/*  bit0=0: Disable temporal dithering */
	/*      =1: Enable temporal dithering */
	/*  bit4=0: 666RGB */
	/*      =1: 888RGB */
	/*  bit5=0: Gray level 2 */
	/*      =1: Gray level 4 */
	UCHAR ucFRC;		/*  bit4=0: 25FRC_SEL pattern E */
	/*      =1: 25FRC_SEL pattern F */
	/*  bit6:5=0: 50FRC_SEL pattern A */
	/*        =1: 50FRC_SEL pattern B */
	/*        =2: 50FRC_SEL pattern C */
	/*        =3: 50FRC_SEL pattern D */
	/*  bit7=0: 75FRC_SEL pattern E */
	/*      =1: 75FRC_SEL pattern F */
} LVDS_ENCODER_CONTROL_PARAMETERS_V2;

#define LVDS_ENCODER_CONTROL_PS_ALLOCATION_V2  LVDS_ENCODER_CONTROL_PARAMETERS_V2

#define TMDS1_ENCODER_CONTROL_PARAMETERS_V2    LVDS_ENCODER_CONTROL_PARAMETERS_V2
#define TMDS1_ENCODER_CONTROL_PS_ALLOCATION_V2 TMDS1_ENCODER_CONTROL_PARAMETERS_V2

#define TMDS2_ENCODER_CONTROL_PARAMETERS_V2    TMDS1_ENCODER_CONTROL_PARAMETERS_V2
#define TMDS2_ENCODER_CONTROL_PS_ALLOCATION_V2 TMDS2_ENCODER_CONTROL_PARAMETERS_V2

#define LVDS_ENCODER_CONTROL_PARAMETERS_V3     LVDS_ENCODER_CONTROL_PARAMETERS_V2
#define LVDS_ENCODER_CONTROL_PS_ALLOCATION_V3  LVDS_ENCODER_CONTROL_PARAMETERS_V3

#define TMDS1_ENCODER_CONTROL_PARAMETERS_V3    LVDS_ENCODER_CONTROL_PARAMETERS_V3
#define TMDS1_ENCODER_CONTROL_PS_ALLOCATION_V3 TMDS1_ENCODER_CONTROL_PARAMETERS_V3

#define TMDS2_ENCODER_CONTROL_PARAMETERS_V3    LVDS_ENCODER_CONTROL_PARAMETERS_V3
#define TMDS2_ENCODER_CONTROL_PS_ALLOCATION_V3 TMDS2_ENCODER_CONTROL_PARAMETERS_V3

/****************************************************************************/
/*  Structures used by ### */
/****************************************************************************/
typedef struct _ENABLE_EXTERNAL_TMDS_ENCODER_PARAMETERS {
	UCHAR ucEnable;		/*  Enable or Disable External TMDS encoder */
	UCHAR ucMisc;		/*  Bit0=0:Enable Single link;=1:Enable Dual link;Bit1 {=0:666RGB, =1:888RGB} */
	UCHAR ucPadding[2];
} ENABLE_EXTERNAL_TMDS_ENCODER_PARAMETERS;

typedef struct _ENABLE_EXTERNAL_TMDS_ENCODER_PS_ALLOCATION {
	ENABLE_EXTERNAL_TMDS_ENCODER_PARAMETERS sXTmdsEncoder;
	WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION sReserved;	/* Caller doesn't need to init this portion */
} ENABLE_EXTERNAL_TMDS_ENCODER_PS_ALLOCATION;

#define ENABLE_EXTERNAL_TMDS_ENCODER_PARAMETERS_V2  LVDS_ENCODER_CONTROL_PARAMETERS_V2

typedef struct _ENABLE_EXTERNAL_TMDS_ENCODER_PS_ALLOCATION_V2 {
	ENABLE_EXTERNAL_TMDS_ENCODER_PARAMETERS_V2 sXTmdsEncoder;
	WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION sReserved;	/* Caller doesn't need to init this portion */
} ENABLE_EXTERNAL_TMDS_ENCODER_PS_ALLOCATION_V2;

typedef struct _EXTERNAL_ENCODER_CONTROL_PS_ALLOCATION {
	DIG_ENCODER_CONTROL_PARAMETERS sDigEncoder;
	WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION sReserved;
} EXTERNAL_ENCODER_CONTROL_PS_ALLOCATION;

/****************************************************************************/
/*  Structures used by DVOEncoderControlTable */
/****************************************************************************/
/* ucTableFormatRevision=1,ucTableContentRevision=3 */

/* ucDVOConfig: */
#define DVO_ENCODER_CONFIG_RATE_SEL							0x01
#define DVO_ENCODER_CONFIG_DDR_SPEED						0x00
#define DVO_ENCODER_CONFIG_SDR_SPEED						0x01
#define DVO_ENCODER_CONFIG_OUTPUT_SEL						0x0c
#define DVO_ENCODER_CONFIG_LOW12BIT							0x00
#define DVO_ENCODER_CONFIG_UPPER12BIT						0x04
#define DVO_ENCODER_CONFIG_24BIT								0x08

typedef struct _DVO_ENCODER_CONTROL_PARAMETERS_V3 {
	USHORT usPixelClock;
	UCHAR ucDVOConfig;
	UCHAR ucAction;		/* ATOM_ENABLE/ATOM_DISABLE/ATOM_HPD_INIT */
	UCHAR ucReseved[4];
} DVO_ENCODER_CONTROL_PARAMETERS_V3;
#define DVO_ENCODER_CONTROL_PS_ALLOCATION_V3	DVO_ENCODER_CONTROL_PARAMETERS_V3

/* ucTableFormatRevision=1 */
/* ucTableContentRevision=3 structure is not changed but usMisc add bit 1 as another input for */
/*  bit1=0: non-coherent mode */
/*      =1: coherent mode */

/* ========================================================================================== */
/* Only change is here next time when changing encoder parameter definitions again! */
#define LVDS_ENCODER_CONTROL_PARAMETERS_LAST     LVDS_ENCODER_CONTROL_PARAMETERS_V3
#define LVDS_ENCODER_CONTROL_PS_ALLOCATION_LAST  LVDS_ENCODER_CONTROL_PARAMETERS_LAST

#define TMDS1_ENCODER_CONTROL_PARAMETERS_LAST    LVDS_ENCODER_CONTROL_PARAMETERS_V3
#define TMDS1_ENCODER_CONTROL_PS_ALLOCATION_LAST TMDS1_ENCODER_CONTROL_PARAMETERS_LAST

#define TMDS2_ENCODER_CONTROL_PARAMETERS_LAST    LVDS_ENCODER_CONTROL_PARAMETERS_V3
#define TMDS2_ENCODER_CONTROL_PS_ALLOCATION_LAST TMDS2_ENCODER_CONTROL_PARAMETERS_LAST

#define DVO_ENCODER_CONTROL_PARAMETERS_LAST      DVO_ENCODER_CONTROL_PARAMETERS
#define DVO_ENCODER_CONTROL_PS_ALLOCATION_LAST   DVO_ENCODER_CONTROL_PS_ALLOCATION

/* ========================================================================================== */
#define PANEL_ENCODER_MISC_DUAL                0x01
#define PANEL_ENCODER_MISC_COHERENT            0x02
#define	PANEL_ENCODER_MISC_TMDS_LINKB					 0x04
#define	PANEL_ENCODER_MISC_HDMI_TYPE					 0x08

#define PANEL_ENCODER_ACTION_DISABLE           ATOM_DISABLE
#define PANEL_ENCODER_ACTION_ENABLE            ATOM_ENABLE
#define PANEL_ENCODER_ACTION_COHERENTSEQ       (ATOM_ENABLE+1)

#define PANEL_ENCODER_TRUNCATE_EN              0x01
#define PANEL_ENCODER_TRUNCATE_DEPTH           0x10
#define PANEL_ENCODER_SPATIAL_DITHER_EN        0x01
#define PANEL_ENCODER_SPATIAL_DITHER_DEPTH     0x10
#define PANEL_ENCODER_TEMPORAL_DITHER_EN       0x01
#define PANEL_ENCODER_TEMPORAL_DITHER_DEPTH    0x10
#define PANEL_ENCODER_TEMPORAL_LEVEL_4         0x20
#define PANEL_ENCODER_25FRC_MASK               0x10
#define PANEL_ENCODER_25FRC_E                  0x00
#define PANEL_ENCODER_25FRC_F                  0x10
#define PANEL_ENCODER_50FRC_MASK               0x60
#define PANEL_ENCODER_50FRC_A                  0x00
#define PANEL_ENCODER_50FRC_B                  0x20
#define PANEL_ENCODER_50FRC_C                  0x40
#define PANEL_ENCODER_50FRC_D                  0x60
#define PANEL_ENCODER_75FRC_MASK               0x80
#define PANEL_ENCODER_75FRC_E                  0x00
#define PANEL_ENCODER_75FRC_F                  0x80

/****************************************************************************/
/*  Structures used by SetVoltageTable */
/****************************************************************************/
#define SET_VOLTAGE_TYPE_ASIC_VDDC             1
#define SET_VOLTAGE_TYPE_ASIC_MVDDC            2
#define SET_VOLTAGE_TYPE_ASIC_MVDDQ            3
#define SET_VOLTAGE_TYPE_ASIC_VDDCI            4
#define SET_VOLTAGE_INIT_MODE                  5
#define SET_VOLTAGE_GET_MAX_VOLTAGE            6	/* Gets the Max. voltage for the soldered Asic */

#define SET_ASIC_VOLTAGE_MODE_ALL_SOURCE       0x1
#define SET_ASIC_VOLTAGE_MODE_SOURCE_A         0x2
#define SET_ASIC_VOLTAGE_MODE_SOURCE_B         0x4

#define	SET_ASIC_VOLTAGE_MODE_SET_VOLTAGE      0x0
#define	SET_ASIC_VOLTAGE_MODE_GET_GPIOVAL      0x1
#define	SET_ASIC_VOLTAGE_MODE_GET_GPIOMASK     0x2

typedef struct _SET_VOLTAGE_PARAMETERS {
	UCHAR ucVoltageType;	/*  To tell which voltage to set up, VDDC/MVDDC/MVDDQ */
	UCHAR ucVoltageMode;	/*  To set all, to set source A or source B or ... */
	UCHAR ucVoltageIndex;	/*  An index to tell which voltage level */
	UCHAR ucReserved;
} SET_VOLTAGE_PARAMETERS;

typedef struct _SET_VOLTAGE_PARAMETERS_V2 {
	UCHAR ucVoltageType;	/*  To tell which voltage to set up, VDDC/MVDDC/MVDDQ */
	UCHAR ucVoltageMode;	/*  Not used, maybe use for state machine for differen power mode */
	USHORT usVoltageLevel;	/*  real voltage level */
} SET_VOLTAGE_PARAMETERS_V2;

typedef struct _SET_VOLTAGE_PS_ALLOCATION {
	SET_VOLTAGE_PARAMETERS sASICSetVoltage;
	WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION sReserved;
} SET_VOLTAGE_PS_ALLOCATION;

/****************************************************************************/
/*  Structures used by TVEncoderControlTable */
/****************************************************************************/
typedef struct _TV_ENCODER_CONTROL_PARAMETERS {
	USHORT usPixelClock;	/*  in 10KHz; for bios convenient */
	UCHAR ucTvStandard;	/*  See definition "ATOM_TV_NTSC ..." */
	UCHAR ucAction;		/*  0: turn off encoder */
	/*  1: setup and turn on encoder */
} TV_ENCODER_CONTROL_PARAMETERS;

typedef struct _TV_ENCODER_CONTROL_PS_ALLOCATION {
	TV_ENCODER_CONTROL_PARAMETERS sTVEncoder;
	WRITE_ONE_BYTE_HW_I2C_DATA_PS_ALLOCATION sReserved;	/*  Don't set this one */
} TV_ENCODER_CONTROL_PS_ALLOCATION;

/* ==============================Data Table Portion==================================== */

#ifdef	UEFI_BUILD
#define	UTEMP	USHORT
#define	USHORT	void*
#endif

/****************************************************************************/
/*  Structure used in Data.mtb */
/****************************************************************************/
typedef struct _ATOM_MASTER_LIST_OF_DATA_TABLES {
	USHORT UtilityPipeLine;	/*  Offest for the utility to get parser info,Don't change this position! */
	USHORT MultimediaCapabilityInfo;	/*  Only used by MM Lib,latest version 1.1, not configuable from Bios, need to include the table to build Bios */
	USHORT MultimediaConfigInfo;	/*  Only used by MM Lib,latest version 2.1, not configuable from Bios, need to include the table to build Bios */
	USHORT StandardVESA_Timing;	/*  Only used by Bios */
	USHORT FirmwareInfo;	/*  Shared by various SW components,latest version 1.4 */
	USHORT DAC_Info;	/*  Will be obsolete from R600 */
	USHORT LVDS_Info;	/*  Shared by various SW components,latest version 1.1 */
	USHORT TMDS_Info;	/*  Will be obsolete from R600 */
	USHORT AnalogTV_Info;	/*  Shared by various SW components,latest version 1.1 */
	USHORT SupportedDevicesInfo;	/*  Will be obsolete from R600 */
	USHORT GPIO_I2C_Info;	/*  Shared by various SW components,latest version 1.2 will be used from R600 */
	USHORT VRAM_UsageByFirmware;	/*  Shared by various SW components,latest version 1.3 will be used from R600 */
	USHORT GPIO_Pin_LUT;	/*  Shared by various SW components,latest version 1.1 */
	USHORT VESA_ToInternalModeLUT;	/*  Only used by Bios */
	USHORT ComponentVideoInfo;	/*  Shared by various SW components,latest version 2.1 will be used from R600 */
	USHORT PowerPlayInfo;	/*  Shared by various SW components,latest version 2.1,new design from R600 */
	USHORT CompassionateData;	/*  Will be obsolete from R600 */
	USHORT SaveRestoreInfo;	/*  Only used by Bios */
	USHORT PPLL_SS_Info;	/*  Shared by various SW components,latest version 1.2, used to call SS_Info, change to new name because of int ASIC SS info */
	USHORT OemInfo;		/*  Defined and used by external SW, should be obsolete soon */
	USHORT XTMDS_Info;	/*  Will be obsolete from R600 */
	USHORT MclkSS_Info;	/*  Shared by various SW components,latest version 1.1, only enabled when ext SS chip is used */
	USHORT Object_Header;	/*  Shared by various SW components,latest version 1.1 */
	USHORT IndirectIOAccess;	/*  Only used by Bios,this table position can't change at all!! */
	USHORT MC_InitParameter;	/*  Only used by command table */
	USHORT ASIC_VDDC_Info;	/*  Will be obsolete from R600 */
	USHORT ASIC_InternalSS_Info;	/*  New tabel name from R600, used to be called "ASIC_MVDDC_Info" */
	USHORT TV_VideoMode;	/*  Only used by command table */
	USHORT VRAM_Info;	/*  Only used by command table, latest version 1.3 */
	USHORT MemoryTrainingInfo;	/*  Used for VBIOS and Diag utility for memory training purpose since R600. the new table rev start from 2.1 */
	USHORT IntegratedSystemInfo;	/*  Shared by various SW components */
	USHORT ASIC_ProfilingInfo;	/*  New table name from R600, used to be called "ASIC_VDDCI_Info" for pre-R600 */
	USHORT VoltageObjectInfo;	/*  Shared by various SW components, latest version 1.1 */
	USHORT PowerSourceInfo;	/*  Shared by various SW components, latest versoin 1.1 */
} ATOM_MASTER_LIST_OF_DATA_TABLES;

#ifdef	UEFI_BUILD
#define	USHORT	UTEMP
#endif

typedef struct _ATOM_MASTER_DATA_TABLE {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ATOM_MASTER_LIST_OF_DATA_TABLES ListOfDataTables;
} ATOM_MASTER_DATA_TABLE;

/****************************************************************************/
/*  Structure used in MultimediaCapabilityInfoTable */
/****************************************************************************/
typedef struct _ATOM_MULTIMEDIA_CAPABILITY_INFO {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ULONG ulSignature;	/*  HW info table signature string "$ATI" */
	UCHAR ucI2C_Type;	/*  I2C type (normal GP_IO, ImpactTV GP_IO, Dedicated I2C pin, etc) */
	UCHAR ucTV_OutInfo;	/*  Type of TV out supported (3:0) and video out crystal frequency (6:4) and TV data port (7) */
	UCHAR ucVideoPortInfo;	/*  Provides the video port capabilities */
	UCHAR ucHostPortInfo;	/*  Provides host port configuration information */
} ATOM_MULTIMEDIA_CAPABILITY_INFO;

/****************************************************************************/
/*  Structure used in MultimediaConfigInfoTable */
/****************************************************************************/
typedef struct _ATOM_MULTIMEDIA_CONFIG_INFO {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ULONG ulSignature;	/*  MM info table signature sting "$MMT" */
	UCHAR ucTunerInfo;	/*  Type of tuner installed on the adapter (4:0) and video input for tuner (7:5) */
	UCHAR ucAudioChipInfo;	/*  List the audio chip type (3:0) product type (4) and OEM revision (7:5) */
	UCHAR ucProductID;	/*  Defines as OEM ID or ATI board ID dependent on product type setting */
	UCHAR ucMiscInfo1;	/*  Tuner voltage (1:0) HW teletext support (3:2) FM audio decoder (5:4) reserved (6) audio scrambling (7) */
	UCHAR ucMiscInfo2;	/*  I2S input config (0) I2S output config (1) I2S Audio Chip (4:2) SPDIF Output Config (5) reserved (7:6) */
	UCHAR ucMiscInfo3;	/*  Video Decoder Type (3:0) Video In Standard/Crystal (7:4) */
	UCHAR ucMiscInfo4;	/*  Video Decoder Host Config (2:0) reserved (7:3) */
	UCHAR ucVideoInput0Info;	/*  Video Input 0 Type (1:0) F/B setting (2) physical connector ID (5:3) reserved (7:6) */
	UCHAR ucVideoInput1Info;	/*  Video Input 1 Type (1:0) F/B setting (2) physical connector ID (5:3) reserved (7:6) */
	UCHAR ucVideoInput2Info;	/*  Video Input 2 Type (1:0) F/B setting (2) physical connector ID (5:3) reserved (7:6) */
	UCHAR ucVideoInput3Info;	/*  Video Input 3 Type (1:0) F/B setting (2) physical connector ID (5:3) reserved (7:6) */
	UCHAR ucVideoInput4Info;	/*  Video Input 4 Type (1:0) F/B setting (2) physical connector ID (5:3) reserved (7:6) */
} ATOM_MULTIMEDIA_CONFIG_INFO;

/****************************************************************************/
/*  Structures used in FirmwareInfoTable */
/****************************************************************************/

/*  usBIOSCapability Defintion: */
/*  Bit 0 = 0: Bios image is not Posted, =1:Bios image is Posted; */
/*  Bit 1 = 0: Dual CRTC is not supported, =1: Dual CRTC is supported; */
/*  Bit 2 = 0: Extended Desktop is not supported, =1: Extended Desktop is supported; */
/*  Others: Reserved */
#define ATOM_BIOS_INFO_ATOM_FIRMWARE_POSTED         0x0001
#define ATOM_BIOS_INFO_DUAL_CRTC_SUPPORT            0x0002
#define ATOM_BIOS_INFO_EXTENDED_DESKTOP_SUPPORT     0x0004
#define ATOM_BIOS_INFO_MEMORY_CLOCK_SS_SUPPORT      0x0008
#define ATOM_BIOS_INFO_ENGINE_CLOCK_SS_SUPPORT      0x0010
#define ATOM_BIOS_INFO_BL_CONTROLLED_BY_GPU         0x0020
#define ATOM_BIOS_INFO_WMI_SUPPORT                  0x0040
#define ATOM_BIOS_INFO_PPMODE_ASSIGNGED_BY_SYSTEM   0x0080
#define ATOM_BIOS_INFO_HYPERMEMORY_SUPPORT          0x0100
#define ATOM_BIOS_INFO_HYPERMEMORY_SIZE_MASK        0x1E00
#define ATOM_BIOS_INFO_VPOST_WITHOUT_FIRST_MODE_SET 0x2000
#define ATOM_BIOS_INFO_BIOS_SCRATCH6_SCL2_REDEFINE  0x4000

#ifndef _H2INC

/* Please don't add or expand this bitfield structure below, this one will retire soon.! */
typedef struct _ATOM_FIRMWARE_CAPABILITY {
#if ATOM_BIG_ENDIAN
	USHORT Reserved:3;
	USHORT HyperMemory_Size:4;
	USHORT HyperMemory_Support:1;
	USHORT PPMode_Assigned:1;
	USHORT WMI_SUPPORT:1;
	USHORT GPUControlsBL:1;
	USHORT EngineClockSS_Support:1;
	USHORT MemoryClockSS_Support:1;
	USHORT ExtendedDesktopSupport:1;
	USHORT DualCRTC_Support:1;
	USHORT FirmwarePosted:1;
#else
	USHORT FirmwarePosted:1;
	USHORT DualCRTC_Support:1;
	USHORT ExtendedDesktopSupport:1;
	USHORT MemoryClockSS_Support:1;
	USHORT EngineClockSS_Support:1;
	USHORT GPUControlsBL:1;
	USHORT WMI_SUPPORT:1;
	USHORT PPMode_Assigned:1;
	USHORT HyperMemory_Support:1;
	USHORT HyperMemory_Size:4;
	USHORT Reserved:3;
#endif
} ATOM_FIRMWARE_CAPABILITY;

typedef union _ATOM_FIRMWARE_CAPABILITY_ACCESS {
	ATOM_FIRMWARE_CAPABILITY sbfAccess;
	USHORT susAccess;
} ATOM_FIRMWARE_CAPABILITY_ACCESS;

#else

typedef union _ATOM_FIRMWARE_CAPABILITY_ACCESS {
	USHORT susAccess;
} ATOM_FIRMWARE_CAPABILITY_ACCESS;

#endif

typedef struct _ATOM_FIRMWARE_INFO {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ULONG ulFirmwareRevision;
	ULONG ulDefaultEngineClock;	/* In 10Khz unit */
	ULONG ulDefaultMemoryClock;	/* In 10Khz unit */
	ULONG ulDriverTargetEngineClock;	/* In 10Khz unit */
	ULONG ulDriverTargetMemoryClock;	/* In 10Khz unit */
	ULONG ulMaxEngineClockPLL_Output;	/* In 10Khz unit */
	ULONG ulMaxMemoryClockPLL_Output;	/* In 10Khz unit */
	ULONG ulMaxPixelClockPLL_Output;	/* In 10Khz unit */
	ULONG ulASICMaxEngineClock;	/* In 10Khz unit */
	ULONG ulASICMaxMemoryClock;	/* In 10Khz unit */
	UCHAR ucASICMaxTemperature;
	UCHAR ucPadding[3];	/* Don't use them */
	ULONG aulReservedForBIOS[3];	/* Don't use them */
	USHORT usMinEngineClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxEngineClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinEngineClockPLL_Output;	/* In 10Khz unit */
	USHORT usMinMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinMemoryClockPLL_Output;	/* In 10Khz unit */
	USHORT usMaxPixelClock;	/* In 10Khz unit, Max.  Pclk */
	USHORT usMinPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinPixelClockPLL_Output;	/* In 10Khz unit, the definitions above can't change!!! */
	ATOM_FIRMWARE_CAPABILITY_ACCESS usFirmwareCapability;
	USHORT usReferenceClock;	/* In 10Khz unit */
	USHORT usPM_RTS_Location;	/* RTS PM4 starting location in ROM in 1Kb unit */
	UCHAR ucPM_RTS_StreamSize;	/* RTS PM4 packets in Kb unit */
	UCHAR ucDesign_ID;	/* Indicate what is the board design */
	UCHAR ucMemoryModule_ID;	/* Indicate what is the board design */
} ATOM_FIRMWARE_INFO;

typedef struct _ATOM_FIRMWARE_INFO_V1_2 {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ULONG ulFirmwareRevision;
	ULONG ulDefaultEngineClock;	/* In 10Khz unit */
	ULONG ulDefaultMemoryClock;	/* In 10Khz unit */
	ULONG ulDriverTargetEngineClock;	/* In 10Khz unit */
	ULONG ulDriverTargetMemoryClock;	/* In 10Khz unit */
	ULONG ulMaxEngineClockPLL_Output;	/* In 10Khz unit */
	ULONG ulMaxMemoryClockPLL_Output;	/* In 10Khz unit */
	ULONG ulMaxPixelClockPLL_Output;	/* In 10Khz unit */
	ULONG ulASICMaxEngineClock;	/* In 10Khz unit */
	ULONG ulASICMaxMemoryClock;	/* In 10Khz unit */
	UCHAR ucASICMaxTemperature;
	UCHAR ucMinAllowedBL_Level;
	UCHAR ucPadding[2];	/* Don't use them */
	ULONG aulReservedForBIOS[2];	/* Don't use them */
	ULONG ulMinPixelClockPLL_Output;	/* In 10Khz unit */
	USHORT usMinEngineClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxEngineClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinEngineClockPLL_Output;	/* In 10Khz unit */
	USHORT usMinMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinMemoryClockPLL_Output;	/* In 10Khz unit */
	USHORT usMaxPixelClock;	/* In 10Khz unit, Max.  Pclk */
	USHORT usMinPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinPixelClockPLL_Output;	/* In 10Khz unit - lower 16bit of ulMinPixelClockPLL_Output */
	ATOM_FIRMWARE_CAPABILITY_ACCESS usFirmwareCapability;
	USHORT usReferenceClock;	/* In 10Khz unit */
	USHORT usPM_RTS_Location;	/* RTS PM4 starting location in ROM in 1Kb unit */
	UCHAR ucPM_RTS_StreamSize;	/* RTS PM4 packets in Kb unit */
	UCHAR ucDesign_ID;	/* Indicate what is the board design */
	UCHAR ucMemoryModule_ID;	/* Indicate what is the board design */
} ATOM_FIRMWARE_INFO_V1_2;

typedef struct _ATOM_FIRMWARE_INFO_V1_3 {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ULONG ulFirmwareRevision;
	ULONG ulDefaultEngineClock;	/* In 10Khz unit */
	ULONG ulDefaultMemoryClock;	/* In 10Khz unit */
	ULONG ulDriverTargetEngineClock;	/* In 10Khz unit */
	ULONG ulDriverTargetMemoryClock;	/* In 10Khz unit */
	ULONG ulMaxEngineClockPLL_Output;	/* In 10Khz unit */
	ULONG ulMaxMemoryClockPLL_Output;	/* In 10Khz unit */
	ULONG ulMaxPixelClockPLL_Output;	/* In 10Khz unit */
	ULONG ulASICMaxEngineClock;	/* In 10Khz unit */
	ULONG ulASICMaxMemoryClock;	/* In 10Khz unit */
	UCHAR ucASICMaxTemperature;
	UCHAR ucMinAllowedBL_Level;
	UCHAR ucPadding[2];	/* Don't use them */
	ULONG aulReservedForBIOS;	/* Don't use them */
	ULONG ul3DAccelerationEngineClock;	/* In 10Khz unit */
	ULONG ulMinPixelClockPLL_Output;	/* In 10Khz unit */
	USHORT usMinEngineClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxEngineClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinEngineClockPLL_Output;	/* In 10Khz unit */
	USHORT usMinMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinMemoryClockPLL_Output;	/* In 10Khz unit */
	USHORT usMaxPixelClock;	/* In 10Khz unit, Max.  Pclk */
	USHORT usMinPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinPixelClockPLL_Output;	/* In 10Khz unit - lower 16bit of ulMinPixelClockPLL_Output */
	ATOM_FIRMWARE_CAPABILITY_ACCESS usFirmwareCapability;
	USHORT usReferenceClock;	/* In 10Khz unit */
	USHORT usPM_RTS_Location;	/* RTS PM4 starting location in ROM in 1Kb unit */
	UCHAR ucPM_RTS_StreamSize;	/* RTS PM4 packets in Kb unit */
	UCHAR ucDesign_ID;	/* Indicate what is the board design */
	UCHAR ucMemoryModule_ID;	/* Indicate what is the board design */
} ATOM_FIRMWARE_INFO_V1_3;

typedef struct _ATOM_FIRMWARE_INFO_V1_4 {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ULONG ulFirmwareRevision;
	ULONG ulDefaultEngineClock;	/* In 10Khz unit */
	ULONG ulDefaultMemoryClock;	/* In 10Khz unit */
	ULONG ulDriverTargetEngineClock;	/* In 10Khz unit */
	ULONG ulDriverTargetMemoryClock;	/* In 10Khz unit */
	ULONG ulMaxEngineClockPLL_Output;	/* In 10Khz unit */
	ULONG ulMaxMemoryClockPLL_Output;	/* In 10Khz unit */
	ULONG ulMaxPixelClockPLL_Output;	/* In 10Khz unit */
	ULONG ulASICMaxEngineClock;	/* In 10Khz unit */
	ULONG ulASICMaxMemoryClock;	/* In 10Khz unit */
	UCHAR ucASICMaxTemperature;
	UCHAR ucMinAllowedBL_Level;
	USHORT usBootUpVDDCVoltage;	/* In MV unit */
	USHORT usLcdMinPixelClockPLL_Output;	/*  In MHz unit */
	USHORT usLcdMaxPixelClockPLL_Output;	/*  In MHz unit */
	ULONG ul3DAccelerationEngineClock;	/* In 10Khz unit */
	ULONG ulMinPixelClockPLL_Output;	/* In 10Khz unit */
	USHORT usMinEngineClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxEngineClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinEngineClockPLL_Output;	/* In 10Khz unit */
	USHORT usMinMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinMemoryClockPLL_Output;	/* In 10Khz unit */
	USHORT usMaxPixelClock;	/* In 10Khz unit, Max.  Pclk */
	USHORT usMinPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinPixelClockPLL_Output;	/* In 10Khz unit - lower 16bit of ulMinPixelClockPLL_Output */
	ATOM_FIRMWARE_CAPABILITY_ACCESS usFirmwareCapability;
	USHORT usReferenceClock;	/* In 10Khz unit */
	USHORT usPM_RTS_Location;	/* RTS PM4 starting location in ROM in 1Kb unit */
	UCHAR ucPM_RTS_StreamSize;	/* RTS PM4 packets in Kb unit */
	UCHAR ucDesign_ID;	/* Indicate what is the board design */
	UCHAR ucMemoryModule_ID;	/* Indicate what is the board design */
} ATOM_FIRMWARE_INFO_V1_4;

#define ATOM_FIRMWARE_INFO_LAST  ATOM_FIRMWARE_INFO_V1_4

/****************************************************************************/
/*  Structures used in IntegratedSystemInfoTable */
/****************************************************************************/
#define IGP_CAP_FLAG_DYNAMIC_CLOCK_EN      0x2
#define IGP_CAP_FLAG_AC_CARD               0x4
#define IGP_CAP_FLAG_SDVO_CARD             0x8
#define IGP_CAP_FLAG_POSTDIV_BY_2_MODE     0x10

typedef struct _ATOM_INTEGRATED_SYSTEM_INFO {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ULONG ulBootUpEngineClock;	/* in 10kHz unit */
	ULONG ulBootUpMemoryClock;	/* in 10kHz unit */
	ULONG ulMaxSystemMemoryClock;	/* in 10kHz unit */
	ULONG ulMinSystemMemoryClock;	/* in 10kHz unit */
	UCHAR ucNumberOfCyclesInPeriodHi;
	UCHAR ucLCDTimingSel;	/* =0:not valid.!=0 sel this timing descriptor from LCD EDID. */
	USHORT usReserved1;
	USHORT usInterNBVoltageLow;	/* An intermidiate PMW value to set the voltage */
	USHORT usInterNBVoltageHigh;	/* Another intermidiate PMW value to set the voltage */
	ULONG ulReserved[2];

	USHORT usFSBClock;	/* In MHz unit */
	USHORT usCapabilityFlag;	/* Bit0=1 indicates the fake HDMI support,Bit1=0/1 for Dynamic clocking dis/enable */
	/* Bit[3:2]== 0:No PCIE card, 1:AC card, 2:SDVO card */
	/* Bit[4]==1: P/2 mode, ==0: P/1 mode */
	USHORT usPCIENBCfgReg7;	/* bit[7:0]=MUX_Sel, bit[9:8]=MUX_SEL_LEVEL2, bit[10]=Lane_Reversal */
	USHORT usK8MemoryClock;	/* in MHz unit */
	USHORT usK8SyncStartDelay;	/* in 0.01 us unit */
	USHORT usK8DataReturnTime;	/* in 0.01 us unit */
	UCHAR ucMaxNBVoltage;
	UCHAR ucMinNBVoltage;
	UCHAR ucMemoryType;	/* [7:4]=1:DDR1;=2:DDR2;=3:DDR3.[3:0] is reserved */
	UCHAR ucNumberOfCyclesInPeriod;	/* CG.FVTHROT_PWM_CTRL_REG0.NumberOfCyclesInPeriod */
	UCHAR ucStartingPWM_HighTime;	/* CG.FVTHROT_PWM_CTRL_REG0.StartingPWM_HighTime */
	UCHAR ucHTLinkWidth;	/* 16 bit vs. 8 bit */
	UCHAR ucMaxNBVoltageHigh;
	UCHAR ucMinNBVoltageHigh;
} ATOM_INTEGRATED_SYSTEM_INFO;

/* Explanation on entries in ATOM_INTEGRATED_SYSTEM_INFO
ulBootUpMemoryClock:    For Intel IGP,it's the UMA system memory clock
                        For AMD IGP,it's 0 if no SidePort memory installed or it's the boot-up SidePort memory clock
ulMaxSystemMemoryClock: For Intel IGP,it's the Max freq from memory SPD if memory runs in ASYNC mode or otherwise (SYNC mode) it's 0
                        For AMD IGP,for now this can be 0
ulMinSystemMemoryClock: For Intel IGP,it's 133MHz if memory runs in ASYNC mode or otherwise (SYNC mode) it's 0
                        For AMD IGP,for now this can be 0

usFSBClock:             For Intel IGP,it's FSB Freq
                        For AMD IGP,it's HT Link Speed

usK8MemoryClock:        For AMD IGP only. For RevF CPU, set it to 200
usK8SyncStartDelay:     For AMD IGP only. Memory access latency in K8, required for watermark calculation
usK8DataReturnTime:     For AMD IGP only. Memory access latency in K8, required for watermark calculation

VC:Voltage Control
ucMaxNBVoltage:         Voltage regulator dependent PWM value. Low 8 bits of the value for the max voltage.Set this one to 0xFF if VC without PWM. Set this to 0x0 if no VC at all.
ucMinNBVoltage:         Voltage regulator dependent PWM value. Low 8 bits of the value for the min voltage.Set this one to 0x00 if VC without PWM or no VC at all.

ucNumberOfCyclesInPeriod:   Indicate how many cycles when PWM duty is 100%. low 8 bits of the value.
ucNumberOfCyclesInPeriodHi: Indicate how many cycles when PWM duty is 100%. high 8 bits of the value.If the PWM has an inverter,set bit [7]==1,otherwise set it 0

ucMaxNBVoltageHigh:     Voltage regulator dependent PWM value. High 8 bits of  the value for the max voltage.Set this one to 0xFF if VC without PWM. Set this to 0x0 if no VC at all.
ucMinNBVoltageHigh:     Voltage regulator dependent PWM value. High 8 bits of the value for the min voltage.Set this one to 0x00 if VC without PWM or no VC at all.

usInterNBVoltageLow:    Voltage regulator dependent PWM value. The value makes the the voltage >=Min NB voltage but <=InterNBVoltageHigh. Set this to 0x0000 if VC without PWM or no VC at all.
usInterNBVoltageHigh:   Voltage regulator dependent PWM value. The value makes the the voltage >=InterNBVoltageLow but <=Max NB voltage.Set this to 0x0000 if VC without PWM or no VC at all.
*/

/*
The following IGP table is introduced from RS780, which is supposed to be put by SBIOS in FB before IGP VBIOS starts VPOST;
Then VBIOS will copy the whole structure to its image so all GPU SW components can access this data structure to get whatever they need.
The enough reservation should allow us to never change table revisions. Whenever needed, a GPU SW component can use reserved portion for new data entries.

SW components can access the IGP system infor structure in the same way as before
*/

typedef struct _ATOM_INTEGRATED_SYSTEM_INFO_V2 {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ULONG ulBootUpEngineClock;	/* in 10kHz unit */
	ULONG ulReserved1[2];	/* must be 0x0 for the reserved */
	ULONG ulBootUpUMAClock;	/* in 10kHz unit */
	ULONG ulBootUpSidePortClock;	/* in 10kHz unit */
	ULONG ulMinSidePortClock;	/* in 10kHz unit */
	ULONG ulReserved2[6];	/* must be 0x0 for the reserved */
	ULONG ulSystemConfig;	/* see explanation below */
	ULONG ulBootUpReqDisplayVector;
	ULONG ulOtherDisplayMisc;
	ULONG ulDDISlot1Config;
	ULONG ulDDISlot2Config;
	UCHAR ucMemoryType;	/* [3:0]=1:DDR1;=2:DDR2;=3:DDR3.[7:4] is reserved */
	UCHAR ucUMAChannelNumber;
	UCHAR ucDockingPinBit;
	UCHAR ucDockingPinPolarity;
	ULONG ulDockingPinCFGInfo;
	ULONG ulCPUCapInfo;
	USHORT usNumberOfCyclesInPeriod;
	USHORT usMaxNBVoltage;
	USHORT usMinNBVoltage;
	USHORT usBootUpNBVoltage;
	ULONG ulHTLinkFreq;	/* in 10Khz */
	USHORT usMinHTLinkWidth;
	USHORT usMaxHTLinkWidth;
	USHORT usUMASyncStartDelay;
	USHORT usUMADataReturnTime;
	USHORT usLinkStatusZeroTime;
	USHORT usReserved;
	ULONG ulHighVoltageHTLinkFreq;	/*  in 10Khz */
	ULONG ulLowVoltageHTLinkFreq;	/*  in 10Khz */
	USHORT usMaxUpStreamHTLinkWidth;
	USHORT usMaxDownStreamHTLinkWidth;
	USHORT usMinUpStreamHTLinkWidth;
	USHORT usMinDownStreamHTLinkWidth;
	ULONG ulReserved3[97];	/* must be 0x0 */
} ATOM_INTEGRATED_SYSTEM_INFO_V2;

/*
ulBootUpEngineClock:   Boot-up Engine Clock in 10Khz;
ulBootUpUMAClock:      Boot-up UMA Clock in 10Khz; it must be 0x0 when UMA is not present
ulBootUpSidePortClock: Boot-up SidePort Clock in 10Khz; it must be 0x0 when SidePort Memory is not present,this could be equal to or less than maximum supported Sideport memory clock

ulSystemConfig:
Bit[0]=1: PowerExpress mode =0 Non-PowerExpress mode;
Bit[1]=1: system boots up at AMD overdrived state or user customized  mode. In this case, driver will just stick to this boot-up mode. No other PowerPlay state
      =0: system boots up at driver control state. Power state depends on PowerPlay table.
Bit[2]=1: PWM method is used on NB voltage control. =0: GPIO method is used.
Bit[3]=1: Only one power state(Performance) will be supported.
      =0: Multiple power states supported from PowerPlay table.
Bit[4]=1: CLMC is supported and enabled on current system.
      =0: CLMC is not supported or enabled on current system. SBIOS need to support HT link/freq change through ATIF interface.
Bit[5]=1: Enable CDLW for all driver control power states. Max HT width is from SBIOS, while Min HT width is determined by display requirement.
      =0: CDLW is disabled. If CLMC is enabled case, Min HT width will be set equal to Max HT width. If CLMC disabled case, Max HT width will be applied.
Bit[6]=1: High Voltage requested for all power states. In this case, voltage will be forced at 1.1v and powerplay table voltage drop/throttling request will be ignored.
      =0: Voltage settings is determined by powerplay table.
Bit[7]=1: Enable CLMC as hybrid Mode. CDLD and CILR will be disabled in this case and we're using legacy C1E. This is workaround for CPU(Griffin) performance issue.
      =0: Enable CLMC as regular mode, CDLD and CILR will be enabled.

ulBootUpReqDisplayVector: This dword is a bit vector indicates what display devices are requested during boot-up. Refer to ATOM_DEVICE_xxx_SUPPORT for the bit vector definitions.

ulOtherDisplayMisc: [15:8]- Bootup LCD Expansion selection; 0-center, 1-full panel size expansion;
			              [7:0] - BootupTV standard selection; This is a bit vector to indicate what TV standards are supported by the system. Refer to ucTVSuppportedStd definition;

ulDDISlot1Config: Describes the PCIE lane configuration on this DDI PCIE slot (ADD2 card) or connector (Mobile design).
      [3:0]  - Bit vector to indicate PCIE lane config of the DDI slot/connector on chassis (bit 0=1 lane 3:0; bit 1=1 lane 7:4; bit 2=1 lane 11:8; bit 3=1 lane 15:12)
			[7:4]  - Bit vector to indicate PCIE lane config of the same DDI slot/connector on docking station (bit 0=1 lane 3:0; bit 1=1 lane 7:4; bit 2=1 lane 11:8; bit 3=1 lane 15:12)
			[15:8] - Lane configuration attribute;
      [23:16]- Connector type, possible value:
               CONNECTOR_OBJECT_ID_SINGLE_LINK_DVI_D
               CONNECTOR_OBJECT_ID_DUAL_LINK_DVI_D
               CONNECTOR_OBJECT_ID_HDMI_TYPE_A
               CONNECTOR_OBJECT_ID_DISPLAYPORT
			[31:24]- Reserved

ulDDISlot2Config: Same as Slot1.
ucMemoryType: SidePort memory type, set it to 0x0 when Sideport memory is not installed. Driver needs this info to change sideport memory clock. Not for display in CCC.
For IGP, Hypermemory is the only memory type showed in CCC.

ucUMAChannelNumber:  how many channels for the UMA;

ulDockingPinCFGInfo: [15:0]-Bus/Device/Function # to CFG to read this Docking Pin; [31:16]-reg offset in CFG to read this pin
ucDockingPinBit:     which bit in this register to read the pin status;
ucDockingPinPolarity:Polarity of the pin when docked;

ulCPUCapInfo:        [7:0]=1:Griffin;[7:0]=2:Greyhound;[7:0]=3:K8, other bits reserved for now and must be 0x0

usNumberOfCyclesInPeriod:Indicate how many cycles when PWM duty is 100%.
usMaxNBVoltage:Max. voltage control value in either PWM or GPIO mode.
usMinNBVoltage:Min. voltage control value in either PWM or GPIO mode.
                    GPIO mode: both usMaxNBVoltage & usMinNBVoltage have a valid value ulSystemConfig.SYSTEM_CONFIG_USE_PWM_ON_VOLTAGE=0
                    PWM mode: both usMaxNBVoltage & usMinNBVoltage have a valid value ulSystemConfig.SYSTEM_CONFIG_USE_PWM_ON_VOLTAGE=1
                    GPU SW don't control mode: usMaxNBVoltage & usMinNBVoltage=0 and no care about ulSystemConfig.SYSTEM_CONFIG_USE_PWM_ON_VOLTAGE
usBootUpNBVoltage:Boot-up voltage regulator dependent PWM value.

ulHTLinkFreq:       Bootup HT link Frequency in 10Khz.
usMinHTLinkWidth:   Bootup minimum HT link width. If CDLW disabled, this is equal to usMaxHTLinkWidth.
                    If CDLW enabled, both upstream and downstream width should be the same during bootup.
usMaxHTLinkWidth:   Bootup maximum HT link width. If CDLW disabled, this is equal to usMinHTLinkWidth.
                    If CDLW enabled, both upstream and downstream width should be the same during bootup.

usUMASyncStartDelay: Memory access latency, required for watermark calculation
usUMADataReturnTime: Memory access latency, required for watermark calculation
usLinkStatusZeroTime:Memory access latency required for watermark calculation, set this to 0x0 for K8 CPU, set a proper value in 0.01 the unit of us
for Griffin or Greyhound. SBIOS needs to convert to actual time by:
                     if T0Ttime [5:4]=00b, then usLinkStatusZeroTime=T0Ttime [3:0]*0.1us (0.0 to 1.5us)
                     if T0Ttime [5:4]=01b, then usLinkStatusZeroTime=T0Ttime [3:0]*0.5us (0.0 to 7.5us)
                     if T0Ttime [5:4]=10b, then usLinkStatusZeroTime=T0Ttime [3:0]*2.0us (0.0 to 30us)
                     if T0Ttime [5:4]=11b, and T0Ttime [3:0]=0x0 to 0xa, then usLinkStatusZeroTime=T0Ttime [3:0]*20us (0.0 to 200us)

ulHighVoltageHTLinkFreq:     HT link frequency for power state with low voltage. If boot up runs in HT1, this must be 0.
                             This must be less than or equal to ulHTLinkFreq(bootup frequency).
ulLowVoltageHTLinkFreq:      HT link frequency for power state with low voltage or voltage scaling 1.0v~1.1v. If boot up runs in HT1, this must be 0.
                             This must be less than or equal to ulHighVoltageHTLinkFreq.

usMaxUpStreamHTLinkWidth:    Asymmetric link width support in the future, to replace usMaxHTLinkWidth. Not used for now.
usMaxDownStreamHTLinkWidth:  same as above.
usMinUpStreamHTLinkWidth:    Asymmetric link width support in the future, to replace usMinHTLinkWidth. Not used for now.
usMinDownStreamHTLinkWidth:  same as above.
*/

#define SYSTEM_CONFIG_POWEREXPRESS_ENABLE                 0x00000001
#define SYSTEM_CONFIG_RUN_AT_OVERDRIVE_ENGINE             0x00000002
#define SYSTEM_CONFIG_USE_PWM_ON_VOLTAGE                  0x00000004
#define SYSTEM_CONFIG_PERFORMANCE_POWERSTATE_ONLY         0x00000008
#define SYSTEM_CONFIG_CLMC_ENABLED                        0x00000010
#define SYSTEM_CONFIG_CDLW_ENABLED                        0x00000020
#define SYSTEM_CONFIG_HIGH_VOLTAGE_REQUESTED              0x00000040
#define SYSTEM_CONFIG_CLMC_HYBRID_MODE_ENABLED            0x00000080

#define IGP_DDI_SLOT_LANE_CONFIG_MASK                     0x000000FF

#define b0IGP_DDI_SLOT_LANE_MAP_MASK                      0x0F
#define b0IGP_DDI_SLOT_DOCKING_LANE_MAP_MASK              0xF0
#define b0IGP_DDI_SLOT_CONFIG_LANE_0_3                    0x01
#define b0IGP_DDI_SLOT_CONFIG_LANE_4_7                    0x02
#define b0IGP_DDI_SLOT_CONFIG_LANE_8_11                   0x04
#define b0IGP_DDI_SLOT_CONFIG_LANE_12_15                  0x08

#define IGP_DDI_SLOT_ATTRIBUTE_MASK                       0x0000FF00
#define IGP_DDI_SLOT_CONFIG_REVERSED                      0x00000100
#define b1IGP_DDI_SLOT_CONFIG_REVERSED                    0x01

#define IGP_DDI_SLOT_CONNECTOR_TYPE_MASK                  0x00FF0000

#define ATOM_CRT_INT_ENCODER1_INDEX                       0x00000000
#define ATOM_LCD_INT_ENCODER1_INDEX                       0x00000001
#define ATOM_TV_INT_ENCODER1_INDEX                        0x00000002
#define ATOM_DFP_INT_ENCODER1_INDEX                       0x00000003
#define ATOM_CRT_INT_ENCODER2_INDEX                       0x00000004
#define ATOM_LCD_EXT_ENCODER1_INDEX                       0x00000005
#define ATOM_TV_EXT_ENCODER1_INDEX                        0x00000006
#define ATOM_DFP_EXT_ENCODER1_INDEX                       0x00000007
#define ATOM_CV_INT_ENCODER1_INDEX                        0x00000008
#define ATOM_DFP_INT_ENCODER2_INDEX                       0x00000009
#define ATOM_CRT_EXT_ENCODER1_INDEX                       0x0000000A
#define ATOM_CV_EXT_ENCODER1_INDEX                        0x0000000B
#define ATOM_DFP_INT_ENCODER3_INDEX                       0x0000000C
#define ATOM_DFP_INT_ENCODER4_INDEX                       0x0000000D

/*  define ASIC internal encoder id ( bit vector ) */
#define ASIC_INT_DAC1_ENCODER_ID											0x00
#define ASIC_INT_TV_ENCODER_ID														0x02
#define ASIC_INT_DIG1_ENCODER_ID													0x03
#define ASIC_INT_DAC2_ENCODER_ID													0x04
#define ASIC_EXT_TV_ENCODER_ID														0x06
#define ASIC_INT_DVO_ENCODER_ID														0x07
#define ASIC_INT_DIG2_ENCODER_ID													0x09
#define ASIC_EXT_DIG_ENCODER_ID														0x05

/* define Encoder attribute */
#define ATOM_ANALOG_ENCODER																0
#define ATOM_DIGITAL_ENCODER															1

#define ATOM_DEVICE_CRT1_INDEX                            0x00000000
#define ATOM_DEVICE_LCD1_INDEX                            0x00000001
#define ATOM_DEVICE_TV1_INDEX                             0x00000002
#define ATOM_DEVICE_DFP1_INDEX                            0x00000003
#define ATOM_DEVICE_CRT2_INDEX                            0x00000004
#define ATOM_DEVICE_LCD2_INDEX                            0x00000005
#define ATOM_DEVICE_TV2_INDEX                             0x00000006
#define ATOM_DEVICE_DFP2_INDEX                            0x00000007
#define ATOM_DEVICE_CV_INDEX                              0x00000008
#define ATOM_DEVICE_DFP3_INDEX														0x00000009
#define ATOM_DEVICE_DFP4_INDEX														0x0000000A
#define ATOM_DEVICE_DFP5_INDEX														0x0000000B
#define ATOM_DEVICE_RESERVEDC_INDEX                       0x0000000C
#define ATOM_DEVICE_RESERVEDD_INDEX                       0x0000000D
#define ATOM_DEVICE_RESERVEDE_INDEX                       0x0000000E
#define ATOM_DEVICE_RESERVEDF_INDEX                       0x0000000F
#define ATOM_MAX_SUPPORTED_DEVICE_INFO                    (ATOM_DEVICE_DFP3_INDEX+1)
#define ATOM_MAX_SUPPORTED_DEVICE_INFO_2                  ATOM_MAX_SUPPORTED_DEVICE_INFO
#define ATOM_MAX_SUPPORTED_DEVICE_INFO_3                  (ATOM_DEVICE_DFP5_INDEX + 1)

#define ATOM_MAX_SUPPORTED_DEVICE                         (ATOM_DEVICE_RESERVEDF_INDEX+1)

#define ATOM_DEVICE_CRT1_SUPPORT                          (0x1L << ATOM_DEVICE_CRT1_INDEX)
#define ATOM_DEVICE_LCD1_SUPPORT                          (0x1L << ATOM_DEVICE_LCD1_INDEX)
#define ATOM_DEVICE_TV1_SUPPORT                           (0x1L << ATOM_DEVICE_TV1_INDEX)
#define ATOM_DEVICE_DFP1_SUPPORT                          (0x1L << ATOM_DEVICE_DFP1_INDEX)
#define ATOM_DEVICE_CRT2_SUPPORT                          (0x1L << ATOM_DEVICE_CRT2_INDEX)
#define ATOM_DEVICE_LCD2_SUPPORT                          (0x1L << ATOM_DEVICE_LCD2_INDEX)
#define ATOM_DEVICE_TV2_SUPPORT                           (0x1L << ATOM_DEVICE_TV2_INDEX)
#define ATOM_DEVICE_DFP2_SUPPORT                          (0x1L << ATOM_DEVICE_DFP2_INDEX)
#define ATOM_DEVICE_CV_SUPPORT                            (0x1L << ATOM_DEVICE_CV_INDEX)
#define ATOM_DEVICE_DFP3_SUPPORT													(0x1L << ATOM_DEVICE_DFP3_INDEX)
#define ATOM_DEVICE_DFP4_SUPPORT													(0x1L << ATOM_DEVICE_DFP4_INDEX )
#define ATOM_DEVICE_DFP5_SUPPORT													(0x1L << ATOM_DEVICE_DFP5_INDEX)

#define ATOM_DEVICE_CRT_SUPPORT \
	(ATOM_DEVICE_CRT1_SUPPORT | ATOM_DEVICE_CRT2_SUPPORT)
#define ATOM_DEVICE_DFP_SUPPORT \
	(ATOM_DEVICE_DFP1_SUPPORT | ATOM_DEVICE_DFP2_SUPPORT | \
	 ATOM_DEVICE_DFP3_SUPPORT | ATOM_DEVICE_DFP4_SUPPORT | \
	 ATOM_DEVICE_DFP5_SUPPORT)
#define ATOM_DEVICE_TV_SUPPORT \
	(ATOM_DEVICE_TV1_SUPPORT  | ATOM_DEVICE_TV2_SUPPORT)
#define ATOM_DEVICE_LCD_SUPPORT \
	(ATOM_DEVICE_LCD1_SUPPORT | ATOM_DEVICE_LCD2_SUPPORT)

#define ATOM_DEVICE_CONNECTOR_TYPE_MASK                   0x000000F0
#define ATOM_DEVICE_CONNECTOR_TYPE_SHIFT                  0x00000004
#define ATOM_DEVICE_CONNECTOR_VGA                         0x00000001
#define ATOM_DEVICE_CONNECTOR_DVI_I                       0x00000002
#define ATOM_DEVICE_CONNECTOR_DVI_D                       0x00000003
#define ATOM_DEVICE_CONNECTOR_DVI_A                       0x00000004
#define ATOM_DEVICE_CONNECTOR_SVIDEO                      0x00000005
#define ATOM_DEVICE_CONNECTOR_COMPOSITE                   0x00000006
#define ATOM_DEVICE_CONNECTOR_LVDS                        0x00000007
#define ATOM_DEVICE_CONNECTOR_DIGI_LINK                   0x00000008
#define ATOM_DEVICE_CONNECTOR_SCART                       0x00000009
#define ATOM_DEVICE_CONNECTOR_HDMI_TYPE_A                 0x0000000A
#define ATOM_DEVICE_CONNECTOR_HDMI_TYPE_B                 0x0000000B
#define ATOM_DEVICE_CONNECTOR_CASE_1                      0x0000000E
#define ATOM_DEVICE_CONNECTOR_DISPLAYPORT                 0x0000000F

#define ATOM_DEVICE_DAC_INFO_MASK                         0x0000000F
#define ATOM_DEVICE_DAC_INFO_SHIFT                        0x00000000
#define ATOM_DEVICE_DAC_INFO_NODAC                        0x00000000
#define ATOM_DEVICE_DAC_INFO_DACA                         0x00000001
#define ATOM_DEVICE_DAC_INFO_DACB                         0x00000002
#define ATOM_DEVICE_DAC_INFO_EXDAC                        0x00000003

#define ATOM_DEVICE_I2C_ID_NOI2C                          0x00000000

#define ATOM_DEVICE_I2C_LINEMUX_MASK                      0x0000000F
#define ATOM_DEVICE_I2C_LINEMUX_SHIFT                     0x00000000

#define ATOM_DEVICE_I2C_ID_MASK                           0x00000070
#define ATOM_DEVICE_I2C_ID_SHIFT                          0x00000004
#define ATOM_DEVICE_I2C_ID_IS_FOR_NON_MM_USE              0x00000001
#define ATOM_DEVICE_I2C_ID_IS_FOR_MM_USE                  0x00000002
#define ATOM_DEVICE_I2C_ID_IS_FOR_SDVO_USE                0x00000003	/* For IGP RS600 */
#define ATOM_DEVICE_I2C_ID_IS_FOR_DAC_SCL                 0x00000004	/* For IGP RS690 */

#define ATOM_DEVICE_I2C_HARDWARE_CAP_MASK                 0x00000080
#define ATOM_DEVICE_I2C_HARDWARE_CAP_SHIFT                0x00000007
#define	ATOM_DEVICE_USES_SOFTWARE_ASSISTED_I2C            0x00000000
#define	ATOM_DEVICE_USES_HARDWARE_ASSISTED_I2C            0x00000001

/*   usDeviceSupport: */
/*   Bits0       = 0 - no CRT1 support= 1- CRT1 is supported */
/*   Bit 1       = 0 - no LCD1 support= 1- LCD1 is supported */
/*   Bit 2       = 0 - no TV1  support= 1- TV1  is supported */
/*   Bit 3       = 0 - no DFP1 support= 1- DFP1 is supported */
/*   Bit 4  Advan= 0 - no CRT2*
 * Cop= 1- Devici/*
 * Copyright 2006-2005 Advanced Micro LCDices, Inc.
 *
, to rmission is hereby grante6 Advanced Micro TV2 ces, Inc.
 *
ted drmission is hereby grante7 Advanced Micro DFPices, Inc.
 *
ware rmission is hereby grante8 Advanced Micro DV  documentationghts rmission is hereby grante9deal in the Softwar3 without restric3dify, merge, publish, dyte1 (S * CopyriDevice Info)ight 2006-2000 Advanced * the rightto use, copy, mrmission is hereby g conditiont 2006 above copyrucI2C_ConfigIDight 2006  [7:0] - I2C LINE Associate nd this permiAdvanced M Micro ticethis permiss]he above copyri-l copieHW_Capll copies 1,  [6:0]=HW assispyriticeID(HW line selection the
 * SoR
 * IMPLIED, INCLUDING =   0VIDED "ASSIS", WITHOUT WARRal portions 6-4f the -HE SENGINE_ID BUT NRTICOVIDHW engOF Afor NON multimedia useXPRESS OR
 * IMPLIED, INCLUDING BUT N2,
 * THE RPOSE AND NOMINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLD3- deal ReservedND NOfutureUT WAPOSE AsXPRESS OR
 * IMPLIED,[3-on notic_ sha_MUX BUTA Mux number when it'sR AU", WITHOUT WAor GPIOIES ITH THE SOSWantial p
typedef struct _ATOM_OM, ID_CONFIG {
#if *****BIG_ENDIAN
	UCHAR bfE SOFTable:1;*************EOSE AID:3***********OM, OineMux:4;
#else: Definitions  shared bet**********/
/*Portion I: Definiti**************#endif
}*******************;SOFTWARE.
union********************_ACCESS {
	*******************sbfAccess         ucTOM_VERS********************_H
#def; * TTOM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSIONht 200S*/

/F COused inOR
 *******nfoT****al porOM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSION (AFTWARE.
 */

/*******OM_VERSIOASSIGMENTine USHORT usClkMaskRegWITHrIndex    specified
#Enf

#ifdef _H2INC
#ifndef ULOY_f

#ifdef _H2INC
#ifndef ULOA
#endif

#ifndef UCHAR
typDataendif

#ifdef _H2INC
#ifndef if

NG
typedef unsigned long ULif

;
#endif

#ifndef UCHAR
typif

f unsigned char UC
#ifndef _ATOMBIOS_H
#defisght cIdRSION_MAJORd
#endiShift        2

#deEn ATOM_CRTC1        Y_ ATOM_CRTC1        AC2            1

if

#ifnne ATOM_DIGA            0
#define ATOM
#defi           1

#definfine ATOM_DIGA    R IN AN **********_PPLL2    2         DIAN
#error Endian***********f ATOM_BIG_ENDIAN
#errINFOine _ATOMCOMMON_TABLE_HEADER sHeaderB       DIAN
#error Endian asOM_VERnfo[*****MAX_SUPPORTED_DEVICE]OM_SCALER1        ne Ane ATOM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSION (ATOM_Common_VERSION_MAJOR | ATotherALER2  urORT OR OOM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSION (A****nRE.
_H2INC * ThPlease don't addSE Oexpand this bitfieldISABLE+2)
 below,efine one will retire soon.!al pFTWARE.
 */

/*******MODE_MISfine ATOM**********************specifR IN AN :6INC
#ifndeRGB888******specifDoubleClock (ATOM_DISABInterlac*******specifCompositeSync     1
#defiV_ReplicaND, By2     1
#defiH

#define ATOM_CURSOR1     VerticalCutOffTOM_CURSOR2  LANKPolarity:1;	/* 0=Active High, 1fine ATOLowal pOR1      ON1            0
#define ATOM_ICON2            1

#define Aorizont   1

#definween VBI 1

#define ATOM_TV_NTSC  #define ATOM_CRT1             0
#define ATOM_CRT2             1

#dCON1            0
#define ATOM_ICON2            1

#define          1

#define ATOM_I     0
#define ATOM_CURSOR2  

#define ATOM_CURSOR1     ne ATOM_BLANKING_OFF     NKING         1
#defiLE+7)

#define ATOM_BLA       (ATOM_DISABR_INIT			   **************P									(ATO****************/

#ifnP									(ATO_H
#define _ATOMP									(ATOMe ATOM_VERSIOspecifie                 4

#define ATOM_DAC;
        ATOM_DAC1_PAL         4

#define ATOM_DAC2_PSATOM_DAC2_CV          ATOM_DAC1_CV
#define ATOM_DA*****t 200usModeMiscALER-al p#defOF A*****H_CUTOFFR
 * IMPLIE0x01BY       1
#defSYNC_POLARITYUSPEND  20
#define ATOM_ICON2            1

Y       1
#deVTOM_PM_OFF           4

/* Bit0:{=0:single, =1:dual},
   Bit1 {=0:666RGine ATOM_PM_SUSPEND  8BY       1
#defiREPLICATIONBY2PEND 10rmat for RGB888,e ATOM_PANEL_MISC_D2AL             COMPOSITETOM_SUSPEND 4AL             INTERLACE_PM_SUSPEND 8AL             DOU   0CLOCKTOM_DSC_DUAAL                   efine ASUSPEND 200 0
#dusRefreshRateTANDBY       1
#deREFRESH_43R
 * IMPL43ISC_GREY_LEVEL_SHIFT    deal in  47ISC_GREY_LEVEL_SHIFT  5tware an  56ISC_GREY_LEVEL_SHIFT  6re is fu  6PANEL_MISC_FPDI   0x0000d, free   65ISC_GREY_LEVEL_SHIFT  70040
#defi7e ATOM_PANEL_MISC_API_E7MISCE_DDR1 2            "DDR1"
#defABLED     7  0x00000080

#define M8ABLED     85 0
#de   ATOM_DATIMING data are exactly the same as VESA timing     .al portiTranslne AT from EDID toEMTYPE_DDR4      ,  IN efinfollowPCI formul     "PCI"PRESS OR
 * US_T_HTOTAL IMPLIED, INCLUDING BUT NSS"

/* MACTIVE + 2*

/* MBORefin+ATOM_FILANKd in
 * all copie used to enable FireGL Support */
#dflag str_TYP_HA +OM_MAX_BLPCI_EXPRESS"

/* MaDISP FireGL Support */
#define ATOing */

#def
#define ATOM_MAX_SICI_EXPRESS"

/* MaTOM_PSTART FireGL flag string */

#definTOM_FIREGL_FLAG_STRFRONT_PORCH   "FGL"	/* Flag used to enable FireGL Support */
#define ATOM_MAX_SIZE_OF_FISO_STRING    "DSK"	/* FlagWIDTHEGL_FLAG_STRING ) */
* FlagTIMEefine ATOM_MAX_SPW_STRING    "DSK"	/IREGL_FireGL Support */
#define ATOM_MAXIREGL_F*
 * TOM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSION (ATOM_VERSION_MAJOR | ATSetCRTC_UsingDTDTE_PCINOR)

/* Endianness should be specified before inclusion,
 * default to little endian
 */
#ifndef ATOM_BSET_pragmaSING_DTD4      _PARAMETERC2_PAL        H_Size        2
e OFBlanking_TimT_TO_ATOM_ROMVFFSET_TO_ATOM_ROMVIMAGE_SIZE				    0x000000H_LANKOffseM_CRTBUS_MEM_TYPE  Width    0x00000002LE    0x94
#define MAXUS_MEMF_ATOMBI       4

#define ATOM_DAC2sfine ATOM_PM_S       1

#H_BorOM_S0
#d F_BUSDFPS_TYPE1

#d   1

#VNUMBER		       2

#RTC;	0x002MISC_8RTC1SE O/* Common2SET_TO_GET_ATPadding[3SCALE_POINTER_TO_ATOM_ROM_HEADER		0x00000ne ATOM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSION (ATOM_VERSION_MAJOR | AT

#pragm)			/* BIOS data must use byte aligment */

/*  Define offset to location of ROM header. */

#define OFFSET_TO_POINTER_M_HEADER		0x00000048L
#define OFTotal	0x002hfine ATOM tn;	/ 1

#define M_TYDisp*Change it only wdisplayhe table needs t_MEMStart*Change io only w_MEM sdate
	/*Image can't be uF_ATOMChange it only weeds w_ATOhe table needsVion;	/*Changv       when the table needsVto change bDER {
	ATware */
	/*Image can'US_MEMpdated, whiDER {
	ATeeds to carry the new tauding the tee to distinguish beR;

typedeinator 0x0! */
#define	OFFSET_TO_GET_ATOMBIOS_STRINGST		0x006e

/* Common header for all ROM Data tabOverscanRighed, whirffseT usProtectedModeInfoLefed, whilef usConfigFilenameOffseBottoms,
			bT usBT usProtectedModeInfoTohange btopSET_TO_GET_ATR IN AN ry table poin_MASTER_DATA_TABLE Y       tCode;
	USHORT usIoBaseAdd_PS_ALLOM_PANEHORT usSubsystemVendorID;
	*/

#define HW_ASSISTED_I2C_STATUS_FAILURE          2
#define HW_ASSISTED_I2C_STATUS_SUCCESS          1
tandard
/* M)			/* BIOS data NG    4	/* sizeof( AAnalogTVON_MINOR)

/* EnNG    4	/* sizeof( Ane ATnentVideoN_MINOR)

/* Endianness should be specified before inclusion,
 * default to little endian
 */
#ifndef ATOM_BIG_EN_DDR4       not specifiedragmsion;	/*and Table Portion=o chaand Table Portion=/*Signatur====== */

#ifdef	UEFIF_ATOMBIOS_ASIC_Bortio_ATOM_ROe	USHORT	void*
#en=========== */

#ifde;	/*SignaturTER_LIST_OF_COMMAND_TAF_ATOMBIOS_ASIC_BPixel
#defs,
		in 10Khz**** usConinator 0x0! */
#define	OFFSET_TO_GET_ATOMBIOS_SHORT	void*
#ModeInfoOffseteSize;	/* Atomic Table, t;
	UeSize;	/* Atomic Table, RT usBIeSize;	/* Atomic Table, usIneSize;	/* AR IN ANTRINGS_STARNKINGnalne ANTION        1

#de ATOM_PAN       ATOM_DA      ne ATOM_SCALER2         M_ROFORMAn not specifiePixClkSIC_BUS_MEM_Tine ATSIC_BUS_MEM_TMAGE_SIZE				    0x0000000ol;	/* Only used bVTO_ATOMBIOS_ASIC_BUS_MEM_T_MEM_TYPE   20	/* incled byF_ATOMBIOS_ASIC_BU_MEM_TYPE   20	/* inclunts,called from ASIC_ImageHFSET_TO_ATOM_ROM TablVFSET_TO_STRINGS_BIOS_STRINGS_STARVBIOS_STRIinator 0x0! */
#define	OFFSET_TO_GET_ATOMBIOS_STRINGSts,called from ASIC_Init */
	USHORT VRAM_BlockVend only by ne ATOM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSION (ATOM_VERSION_MAJOR | ATLVDSet for SW to get al* Need a documentE_AGdescribine is tOR)

/* Endianness should be specified before inclusion,
 * default to little endian
 *ss;
	USHO2
#defineLCDISC_API_RATE_30Hz_PM_SUSPEND  004components,latest version 1.1 */
	U4HORT SetMemoryClocRGB888}*/
s,latest version 1.1 */
	U5HORT SetMemoryCloUAL        s,latest version 1.1 */
	U6HORT SetMemoryClo210
#deOnce DAL sees,  onlCAP the et, itTOM_DISAadS_TYPEC_BUSLCD o THEs own inst/* Aof u pac sLC1)			/*s SW*****rsion e ATV12    "PCIOOM_DIentriests,called from ASIC_In   "DstM_DIvalid/usefuATOM 1.2 W componen	LCDPANEL_CAP_READ__TYP	oryClock0x10
#defcNOR)
FormatRevision=1_STRINGRT ReseContenryDevice;	/* AtFTWARE.
 */

/******* from ASITOM_SCALER_DISABLE   0
#define ATOM_SCALER_Cd only by BW componenly used by vne APatchNOR)
  0x94
#define MAXermit perUSHORT VRAM_0
#dRefer varpanel infoly useds,calleBIOS extenice; Spec    "Size;	/* AOffDelayInMERSION_MAJORPowerSequenceDigOntoDEin10ck */
	USHORT EnableASIC_StEtoBLOngt;	/* Atomic TabrsionTOM_s,
			Bit0:{=0: pacle, =1:dual},Bit1 hang666RGBObsol888RGB only2:3:{Grey level}SET_TgtStatu4ChangLDIPE_PCatND NO      Obso FPirectly used by vari	/* Atomic Ta5ChangSpatial DiOM_DPCI  is****d;1 T LVTMAEncoderConten;	/* 	/* Atomic Ta6ChangTemporMAEncoderControl;	/* Atoents,latest version used by variData tablenelDefaultUSHORT VRAM_Bic Table,  dirIdentiffine ATous SW compSS_    ******* from ASIne ATORT ResetMemoryDevice;	/* Atomic Table,  indirectly usl ROM various SW components,called  Tablrom SetMemoryClock */
	USHORT MemoryPLLInit;
	USHORT AdjustDisplayPll;	/* onExtN_MINOR)
s */
	USHORT AdjustMemoryController;	/* Atomic Table,  indirectly used by various SW components,called from SetMemoryClock */
	USHORT EnableASIC_StaticPwrMgt;	/* Atomic Table,  only used by Bios */
	USHORT ASIC_StaticPwrMgtStatusChange;	/* Obsolete, only used by Bios */
	USHORT DAC_LoadDetection;	/* Atomic Table,  directly used by various SW components,latest version 1.2 */
	USHORT LVTMAEncoderControl;	/* Atomic Table,directly used by various SW components,latest version 1.3 */
	USHORT LCD1OutputControl;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT Size;	/* ALCDVenderIDUSHORT TV1OutpuProductol;	/*RT ASIC_CD  dir_entsialHandlingCatly uSW componentsnfoFSET_tomic to carC_BUSLInit;
	USHORT Ato eny var,  indirec, include version 1.1 t;
	USHORT usPciBusDe[2SCALER_MUL from ASIC_InOM_D       1
#de from ASICLAST

/* Co from ASIC_InSOFTWARE.
 */

/*******PATCine CORDefine _PAL   1

#decordTyp* Only used by=========== */

V======*******ectly used by varne ATOM_SCALER2         ersioTS used bious SW components,latest    1

#dTSValuM_BlockVen SW componentsol;	/*!! Ifne ASrnentsSTART	 exitsockGashoud always le,  e firstd by varD NOeasy  IN in comm
#def****!!fine ATOM_LCD_SELFTEST_STOersiP				CONTROled fious SW components,latest version LCDlatesnder;	/* FuORT SetCRTC_Timiomponents,l componentAP_BL_ATOM_PM_SUSPEN/* Atomi     2
#de*/
	USHORT Setomic C_OverScan;	/* Atom_DDR2     */
	USHORT Se,callecomponents,latest v4SOFTWARE.
 */

/*******FAKEetMem/* Atomic Tab,latest version 1.1 */
	USHORT EnFake_TYPLengTOMBIOble,  directly StrCon[1]RT EnaT onlactually has  directdidused b ele	/* s    "******* Bios */
	USHORT Selecne ATOM_SCALER2         ,calleRESOLUPANE	USHORT SelectCRTC_Source;	/* Atomic TaW componen

#define OFFSETy usedlock;	/* leBufferRegisters;
	USHORT ersion 1.1 */
	USHORectly used by var_TYP     0x0 Table,  used componentstMemoryClockts,latest version 1.1 */
	d fromdirectly used by various SW TOM_PANEL_ersi Bios */
	USHORT SelectMemoryClockk;	/* FuncersileHW_IconCursor;	version 1.1 */
	U  0x00000080

#deed byENectly used by various SW0xFF*/

#define HW_ASSISTED_I2C_STASp	/* Aentstrum whom NOR)

D    iND, s +2)
#define ATOM_LCD_BL_B	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USPom SeSPECTRUMror EnNdian not specifietest vrsion 1.Percentagious SW comp used by vario,latetominly =0 Dtly test v,=1 CeKINGckDetec.ORT VR1 Bla. =0

#d.USHORTs:TBFSET_TO_GET_ATSS_Stetest versionSS_moryCversion 1.3 */
	USHOSW componenmmContontr_DivanUp;	/* AtomiRang
	USHOkGatas r IN AN ACTIOV1/* At*******tputControl;	/* Atomic Tabomponents,latestSION S_ENTR      erScan;	/* Atomic EMPORAL        DPion ID1oryClock 			ND  f1tomicSS moduine ASIC_eq=30ks SW componomponents */
	2SHORT ReadHWAssis3

/*CStatus;	/* Atomic Ta3le,  ponents,latestomicOWN
	USHORTP					ASKT SetMemoryClocents, Function Table,indirectly used b Table,  directlyonents,called from ASIC_ICTablEectly used by various SW tection;	     2
#define ,  directly used by verConnectorDetection;	     2
#define  ATOMNAL1.1  various SW conectorDetection;	/* Atomic TableEXly used by various SW components,called f_DDR2     EXEC1.1 STEP_SIZE_SHIFd to enabatest version 1.1 e,  indiDELAYed by various SW compovarious SW com/

#deDATA_TO_BLON SetMemory/EngineClocmic Table,  used only bytputControl;	/* ne ATOM_SCALER_DISABLE   0
#define ATOM_SCALER_CtputControl;	/* Atomic Tablas */
LER_EXPANSION n TableSCALER_MULW components,called ne ATOM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSION (ATOM_VERSION_MAJOR | AT;	/*Offset for SW t(Topction; the
 *OM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSION (ATOMucTVBootUpectly uStd      iton: ) */

/* AtoTV_NTS       	/* Atomic _STRINGtest versionJ by various SW cMDSAOutputContrPum size of that Fir3 directly used by Mlication;	/* Atom directly used by CNirectly used by5 directly used by on Table,directl 6 directly used by 00040
#defiefine MMDSAOutputContrSECA 1.1 */
	USHORT 8nents,larectermi Copyrrious SW coimponentonents,lsionN 2
#defcomponents,c     2
#deol;	/ version 1.1 */
	0x2* FunctionPsed ersion 1.1 */
	U 0x0le,  directlMy used by various 0xRGB888}*/
nctioontrol;	/* Atomic TUAL        tly est version 1.1 */
	ATOM_PANEL_test ontrol;	/* Atomic T00000002
#dutConontrol;	/* Atomic T80* FunctionSION 2
#defineTV4        verstest version 1.1 */
	UANALOGe,  ne ATOM_SCALER_DISABLE   0
#define ATOM_SCAios */
	utpu * Copyrge the pceInit;	/* Atoly used by vari indirectly used Exte,  ASI****,called from SetMemoryCSlaveAddIC_I/*LInit;
	USHORT Ay used byane A)			/*s[* Atomic Table,  indire];s,latest versio       various SW components,called from EnabBios */
onents,called omponents,l* Atomic Table,  indire_V1_ine M3used by various SW components,called  */
	Urom ASIC_Init */
	USHORT MemoryDeviceInit;	/rsion 1.1 */
	USHOR* Atomic Table,  indirectly usesion 1.1 */
	USHORT DIG1various SW components,called fsion 1.1 */
	USHORT Dm SetMemoryClock */
	USt version 1.1 */
	USHORT DIG2Tran;	/* Atomic ble,  indirectly used by various SW components,called from Enabirectly used by variou */
	ne ATOM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSIOht 200VRAM usage 
#defieirus SW nalEnco
 * TheOne chunk vars */
} ed by Bios   "DD NOHWICON surfaces,_TYPE        "PCI"Curr/* Ane AYPE_PCI 
#deDail mponentand/or STDYPE_PCI      EACH dns to. They canrsiobroken dtly aersilow    "PCI"Allon 1.addresserContro   "Dn 1.o 0x94sler;	/n 1.1r ASIbufc Tato ca.					all MUSTrsioDw varalignedfine  verso driver:l			 phys{
	ATansmitt var  onlmemory  Copce;	mmFBg used(4K        )+y variou_s */_USAGEg used_ADDR ASIC_xange
         Getibl:

/* C_StaticPwrMgtStatusChange

typedef str->MalleDEX;	/* FIGHTNES
/* MMEMORY_IN_64Kon;	CKonents,laST_OF_COMMAND_TABLES LisCStatus;	/* 0tomi256*64K=16Mb (Max.BUS_TYUniphyIis ****! the
       0m SetMemoryClo*/
	RAW_onvey uss SW compon56tomiInand tMAND_Y       1
#defne Re_SURFACEly use,  indir09ed in every command table */
/******* ASILE   0**********3_DDR2     SIONM_ROP				INticPw by various Sous SW componenE_ATTRIB used b_TBL/
typed(_TABLE_ATTRIBUTE {
#*28)

/**8= (y usOF/* Atomic Table,******st version 1.pdatedByUtility:1;	/* [15]=32*ty fla3tion,a prARE.inedECTION , */
	USHORT PS_S of nBytes:7;	/* [14:8]DFP_ENCODERtMemo_RepSETlock */_LEVEL     Dace in BytLANE_NUMin multipl0x8components*/
#else
	USINK_*/
	USizeInBytes:8*********************1********* stru1.1 */
and table */
/******2/
	USHORT PS_SizeInB( a dword), */
	USHORT PS_S+e */
/*******************)******************************/ PS_SizeInin Bytes (mue of parameter rd), */
	USHORT UpdatedByUtility:1;	/* [1CRT1****** PS_SizeInBtility flag */
#enle updated by utrd), */
	USHOR*********/
typn _ATOM_TABLE_ATTRIBUpdatedBy1;	/ PS_SizeIin BytTTRIBUTE_ACCESSrd), */**********/
/*  Sn _ATOM_TABLE_ATTRIB of param********	*********************************rd), */UpdatedByUtility:1;	/* [1)mponents,latest CDIBUTE_ACCESS {
	ATOM_TABLE_ATTCommon header for all e pointe of parameter space in Bytility:1;	/* [1commo****************************common header. *********************************/
/*  common header for all c************/
/*  Every table pointed by _ATOM_MASTER_COMMAND_TABLE has this TV************************************actually points to this header. */
/****************ous SW componenFPIBUTE_ACCESS {
	ATOM_TABLE_ATTM_COMMON_ROM_COMMAND_T pointed by _ATOM_MASTER_COMMAND_T*******************************************************/
/*  *********************************/
/*  *****n header for all command tab*********************** pointed by _ATOM_MASTER_COMMAND_TABLE has this DeviBUTE_ACCESS {
	ATOM_TABLE_ATTfine COMPUTE_ENGINE_PLo this header. */
/*****************************Clock*******************************ck;		/* When*********************************/
/*  Com2on header for all command tables ucAction;		/* 0:res pointed by _ATOM_MASTER_COMMAND_TABLE has this comck;		/* When returen, it's theto return larger Fbdivo this header. */
/******************************** ucAction;		/* 0:reserved //1:
} COMPUTE_MEMO
typedef struct _ATOM_COMMON_ROM_COMMAND_Treturn larger Fbdiv n, [23:0] rE_HEADER CommonHeader;
	ATOM_TABLE_ATTRIBUTE TableAttribute;
} ATOM_ck;		/* When returen, itrn, [23:0] rS;

typedef struct _COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS_V2 {
	ULto rMMON_ROM_COMMAND_TABLE_HEADERto register */*********************************/
/*  to rn header for all comwritten to ucFbDiv;		/* return value */
	UCHAR ucPostDiv;	/* return value */ware*****/
/*  Structures used by Creturn Feedback value to be written to register */
	UCHAR ucPostDivTERS
*******************************to register */
} COMPUTE_MEMORY_ENGINE_PLL_PARAMETEue */S;

typedef struct clock value */

typedef struct _COMPUTE_MEMORY_ENGINE_PLL_PARAMETERS {
	ULONG ulCVregister */
	USHORT usFbock value */alculated clock based on given Fb_div Post_Div and ref_div */
	UCHV* return post div to be ********emporary clo*********************************/
/*  CV000000	/* Applicable toable to memNE_PLL_PARAMETERS_PS_ALLOCATION   COMPUTE_MEMORY_ENGINE_PLL_PARAMETER3OMPUTE_MEMORY_ENGINE_PLL_PARAMesh during clock trax00FFFFFF	/* Clock change tables only take bit [2330] as the requested clock value able to memoSE_NON_BUS_CLOCK_MASK                  0x03000000	/* Applicable to both meme */
#define FIRST_TPS_ALLOCATION   COMPUTE_MEMORY_ENGINE_PLL_PARAMETER4k;		/* When returen, it's the red engine clock changx00FFFFFF	/* Clock change tables only take bit [2340] as the requested clock value KIP_SW_PROGRSE_NON_BUS_CLOCK_MASK                  0x04000000	/* Applicable to both mem it means the table PS_ALLOCATION   COMPUTE_MEMORY_ENGINE_PLL_PARAMETER5k;		/* When returen, it's the reSE_NON_BUS_CLOCK_MASx00FFFFFF	/* Clock change tables only take bit [2350] as the requested clock value ry and enginSE_NON_BUS_CLOCK_MASK                  0x05000000	/* Applicable to both memand vice versa) */
#PS_ALLOCATION   COMPUTE_MEMORY_ENGINE_PLL_PARAMETP_TRAIN1.1 ORY_PLL_PARAM:Mplicable to memory clo
typed*****************************************STACK_STORtStatusChuren, it's the 3SKIP_INTERNAL_MEMOfine56***************et, the table ENDengine uses memoet, the table will s+ 512)Tableonalsiz_START	****in Kbfine Bit1 {=0:666RGcPwrRESERV************** (((OCK									0x08	/* IME_-f a dword), */
	USHORT PS_)>>10)+4)&ly uFC**********s SW c, it OPER_PANE_FLAGby various SW c0xCnents,cLhange,when set, it icable to both md by various 3ytes:7;	/x10	/* ApplS Lis_NEEDS_NO means Subsys	USHORT DAC2OMPLL */

typedef structTOM_COMPUTE_CLcompon*/

#define HW_ASSISTED_I2C_STATUS_FAILURE          2
#define HW_ASSISTED_I2C_SERSION (ATOM_VERSION_MAJOR | ATicPwrM ATOByFirmwarer SW to get aNote1tiononly usedis fill compaSettiblR IN Ane ATpdateInFBne cCore    Subs.asessaget all dataat runnnentGEME    "PCI"note2:2f
#deRV770				e***********mo					an 32biOM_ENmitt****, so wATOM_DIchon;	d in
 * all copRT ResetMemoryDevice;	/,c Table,  indirectly us4TOM_COstrc OF COremaiMMAND_et all dataDR4"

#de ASIC_B1.12cCha1.2 (1.pies neverne cuse), but ulpdateAtomUsedelse
	ULONd in
 * all cop( (AT 0x94  usto carof******** struct)of 1KB         used by varbyte     end    "PCput Parameter */
	UCHAR ucPostDiv;	/* Output Parameter */
	UCHAR ucCntlFlag;	/* Ouchange,when setSIONFIRMWAREticPwrMgtSta ASI			1ic Table,  used only by HAR ucReservemeans thne ATOM_ULONGK_FREQ ulClock;	/* Input PUSHORT TV1Ouse
	ULONUseInKbly used by various RT DAC1EncTERS_V3;

/*  ucCntlFlag *ne ATOM_SCALER2         icPwrMgtStaBYO_MODE   oderControl;	/* Atomic Table,  directly usedTERS_V3;

/*  ucCntlFlag *
commaa        1VramR IN ANmic Table,  usCHAR ucReserved;
} COMPUlTransactiTION_DISABLE         4ne ATOM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSION (ATOM_VERSION_MAJOR | ATOM_VEPin_LUTNOR)

/* Endianness should be specified before inclusion,
 * default to little endian
 */
#ifndef ATOM_BIG_ENDIAN
PIN Atomic Table,  directlyGpio;
} Af _H2INC
   1

#gineCloBit ATOM_CRTC1     OM_VERDOM_SCALER1    *  Structures ne ATOM_SCALER2          1

#*  SLUn notSCALER_DISABLE   0
#define ATOM_SCALER_CENTER*  Structures uATOM_SCPinates*****************LUf needed */
	USHORT DVOEncoderControl;	/* Function Table,directly used by various SW components,latest veets, Don't change the position */
	UCHAR ucExtendedFunctionCode;
	UCHAR ucReserved;
} ATOM_ROM_HEADER;

/** [14:8]ock;	/* In

#de_HIGlag used 	USHORSW components,latest vehen sANDARDSORT SeUTE_Marious SW s
#def_PARAMlag *.ucSettSW ce,  indirectly usOM_VESETTINGS_BITd by y vario0x1Ftomic[4ion ******************************means tD*********6;

/*DED 5] = mustrsiozeroed ou usCo********************************** variousof a{
	ULO7******/
#ifndef ATOM_BIG_ENDIAN
#g */
#deTOM_DAC2_  0x94
#de;	/* Ato*******       1

#define AGINE_CLOCK_PARontrol;	/*by SetMemoryClockTable888RGNENT_VIDE_ENGIN.ucTOM_PM_S (edefvector:7;	/* [14:8]=SizeC  0xSTRICT* [7:0]_SELEComputeCloctomic TablLL_PARAMETERS_PS_ALLOCATION sReserved;
} SET_MEM480i/ASIC_p/uc72tb */1080i****/
typedef struct _DEFAULTATTRIBE used by various ETERS;

typ[7ableVG*************************_PERed by various SW co******7*****ED "AAND_TABLEARAMETERS_PS_ALLOCATION sReserved;
} SET_MEMLetterBox   PrMPUTE_ sha 3k;	/*put 5V    "
/************** sha3_Atrol;	_PAN_16_9******A_SUSPEND   tomirepTablnt gpio 3_DIVt/
#def16:9

typedef struct _ASIC_INIT_PS_ALLOCATION {
	ASIB SW compon3

/*S sASICInitCloc4s;
	SET_ENGINE_CLOCK_PS_ALLOCATION sReserved;	/* Caller doesn'td by varkFlag:8IC_INIT_PARAMETE2.2V

typedef struct _ASIC_INIT_PS_ALLOCAT4_3_LETBOX{
	ASIC_Bit2:3:{S sASICInitClocks;
	SET_ENG4:3  10Khz box*******/
/*  Structure used by DynamicClockGatingTableBfor Rt this structure */
} ASIC_INIT************************************************/
typedef struct _DYd by v********C_INIT_PARAMETE0********/
/*  Structure used by DynamicCloc
	ASIC_INIT_P;	/* /
/********************************RS;
#define  DYNAMIC_CLOCK_GATING_PS_ALLOCATIO need toos, oCLOCK_GATING_PARAMETERS {
	UCHAR ucRS;
#define  DYNAMIC_CLOCK_GATING_PS_ALLOCATIOd by variW coypedef struct _ASIC_INIT_PS_ALLOCAT
	ULONG ulDefaultEng3******edef[5;	/* In pedef struct _ASIC_INIT_PS_ALLOCATEXI */
 components,c;

typedef	USHO****R
 * edefi _H2ne ctClocs****** peratuseectlue, also*********ASTER b#def no.HAR ucPadETERS* Atom******************r;
	ASIC_INIT_PS_LLOCAT
	ASIC_IN3M_ENABLE3_COMPSIC_Init.ctb */
/**********, whichC_PWR_MGT_PARAMdctly une ENABnenting[3];D NOM_COModINE_PLATIC_PWR_MGT_PS_ALLOCATION  ENABLE_ASIC_STATIC nee2:3:{****4ETERS

/****************************************************************************/
/*  Structtest version 1.1 */
	UON sReserved;
} SET_ctly used by various SW components,latesl;	/* onlask SETf

#ifdef _H2INC
#ifndef ENVICE_CVx_SUPPORT} */
	UCHAR uYVICE_CVx_SUPPORT} */
	UCHAR uAVICE_CVx_SUPPORT} */
   1

#*******************Pinine ATS
	SE	USHO3 and above */
}:tatus=1 1.1 ATOh_ICON=0S;

/*  d en,latest v;
	USHORT Adt */
	COMtomicrgetMemoryClock;	/* Inly when tET_ATOMBIOS_STRINGSIC_I_DAC_LOAD_DETEtest version
/**ceInit;	/* *****USHORT ASIC_10Khz unit *       1

#define AEveryly when tNumOfWbgineBLE_ASAC_LOForMPUTE_MEMO v cha D-ConnLOCAT so, sub.tly zere,utput FTWA cTION_PS_A,latest vPS_ALLOCATalocati */
}on foomponents,calle/
/*  Struct setETERS.ucMisc */
various SW components,calle/
/*  Structure*******ON sReserved;
} SET_ol;	/* Atomic Table,  direct  directc Table,  indirectly used by various SW componentON sReserved;
} SET__V21ctly used by various SW components,latest veref struct _DAC_LOAD_DETECTION_PS_ALLOCATION {
	DAC_LOAD_DETECTION_PARAMETERS sDaclt */
	COMPERS sDacload;
	ULONG Reserved[2];e, allocation for EXT DAC */
} DAC_LOAD_DETECTION_PS_ALLOCATION;

/****************************************************************************/
/*  Structures used by DAC1EncoderControlTable.ctb and DAC2EncoderControlTable.ctb */
/************AMETomponents,latestS_ALLOCATION  DAC_ENC.1 */
	USHOS_ALLOCATION  DAC_ENCODE*/

#define HW_ASSISTED_I2C_STATUS_FAILURE          2
#define HW_ASSISTED_I2C_STATUS_SUCCESS          1objectON_MINOR)

/* Endianness should be specified before inclusion,
 * default to little endian
 */
#ifndef ATOM_BIG_ENOBJECT0
#definDEVICE_CRTx_SUPPORT,ATOM_DEVICE_TVx_SUPPORT,ATOons tomic Tab;	/* Atomic TTION_PS_OG2Enc 1.1 */
	USHORT DVOOutpRouteT usPixelClock;	/*  in 10KHz; Encodbios convenient */
	UCHAR ucConProtKIND,  usPixelClock;	/* tomionly availC;	/*ITH T0: PHY linMETERS;gine depCont onlalled from Seo chlayPatby Bios */
	US********************n;	/* Atomic Table,  usedISPtMem*******PATHmmand Table Pons toTag
	/* 
 * CopyrirContrif bfLanes=3 *USHORTtomiefineory o******* LVTMA */
	UCHAR ucAalled from Se{
	U usPixIDAC_LOCTION_PS_A usPixIES OF d by SetEngPULVDS encoder GPU encoder */
	/*  =raphicObjIdsatest ve1st fig;
	/2: D sourctteromr */
to last coder *ow madestin	SETto********** Atomic Tabl LVTMA */
	UCHAR ucY or PCIEPHY */
	/*  =1: LVTMA */
	UCHAR ucBLE   ious SW compe, alo ch[3] st version 2erice;ous SW compons.
  Etly s used LVTMA */
	UCHAR ucAa*/
	/[3] _ENGINE_CLOCCATION			  DIG_ENCODER_COne ATOM_SCALER2         ******tomieachDIG2EncUSHOR  onlSELFTEST_S*/
_PAL        LVDS enl;	/* Atomic SrcDsxelClock;	/*  in 10KHz; fnentse<3 */
	/*   onlpoOF_CTE_Eo a bunNCODfd by vashz uni comnd enoder */
	/*  t */
	COMPUTE_MEM******LINKRATE_MASK				0x01
#define BLE   tomiAbove 4ODER_CONy usedS_MPLL_NFIG_LINK_SEL_MASK				 IG2EncsTC_DPhave,  onlKRATE_1_62GHZ		0x_PARAMETERberOf usPixk */
	USHORT es.
  Everys used****** aine ATOHAR uSel */
	/*  =0:IG_DPLINKRATE_MASK				0x01
#dSRC_DSG_TRANS* [7_ONEdefine ATOMNFIG_DPLINKRATE_2_7NFIG_LINKA_B	FIG_LINKA
#define ATOM_ENCODER_CONFIGSrcENCODER_CONFIG_ne ATOM_TOM_EOM_ENCODER_CONFIGDsL_PARAMETERS DstG_TRANSMITTERBios */
ine ATOM_ENCODER_CONFIG_UNIol;	/*Rela */
	/xternalEn_STADISAx04
#dt Padil
#dntCLOCKt				R_CONas;	/*ond hATOM_****/
typedef struct _DAC_ENISABDTiming********** SW components,late	  0xn emunK_SEindfine					  by var*****;
	USHORT usPcentsUSHORT Enh memory fine B ifol
/* ucEncinR ucRey Bios */
ATOM_ENABLE:  Enableomponents,latestOM, version 1.1 */
	USHORT GetEngi* Atomic Table,  u a dwoP */
TDE_DVI											2
#define ATOM_ENC_DDR2          OUTPUT_PROT******DE_DVI											2
#definTOM_PANEL_MISC_CONNECTORe ATOM__TAGDE_DVI											2
#decomponentLOCK_PAR_CV						VIlock_INM_ENversion 1.1comma 5tomiObsoletTOM_witchK_SE IN OM_VECNTBufftly used buctures used by e in BytFPGASetCRTC_Tversion 1.1 */
	USHORed inV2 {
#if ATOM_BIG_ENDIAN
	UCHAR ucReserved1:2;
	UCHAR ucTransm		15

typeCVutpuHucReDIE_TV											13
#x00000020
#defiJ4
#define ATOM_ENCODER_MOelse
	UCHAR ucDP_CLOCV2 {
#if ATOM_BIG_ENDIAN
	UCHAR ucReserved1:2;
	UCHAR ucTransm*******
	UCHAR ucReserved1:2;A/C/E =1: lin9UCHAR ucTransmitterSelDVO_CFDE_DVI											2
#define UAL             /F */
	UCHArSel:2;	/*  =0: UniphyAB, =1  1K_FREQ {
#if AT		15

typeHARDin BS.ucMOM_DIG_ENCODER_CO 1_DDR2          		15

typePCIByUtB		15

typeversion 1.1 *1TOM_PANEL_MISC_SOUTansmDC_ENCOD****** struct _DIG_TE_MT MemoryRefs convenientnver0C
#de
	ATOM_DIG_ENCODER_CONFIG_V	1 used MgetMemoupdIGB		ITH Tnew/* ucEncoderMf a dded,equ	ATOMEQ;

	USHORT 						  0xck change,when setSION*******ReserveNUMBRING    4	/* si =2: UniphyEF */
	UCHAR ucResSOFTWARE.
 */

/*********** SelectCRTMODE_LVDS										1
#def ss bit=B            1
#definEXT_DAC          2

I2CAtomiODER_MODE	/* ;	/* OutockG's 0 LVDS /

/* ucEncf a ttached DIG_ENCODER_*****DDial p*********** Bios */
	USHORT UpdateCRTC_Do					3
#define****************S_V2;

/* ucConfig */
#dcomponentPDIntR
 *IDoder */rrespon.
  
	/*  =0eserved;Is;
} AOM_ENCOgiv
	USHe piy usfo ROM Data tablluggged SET */
} ODER_CON					3
#defineLINKRATE_MASK				0x01
#deTOM_ENCODER_MODE_TV				NCODER_CONFIG_V2_LINK_SEL_MASK				  0x04
#define0: PHY linFlalayPlKhz unit */
	COMPUTE_MEMdefine ATOM_ENCODER_CONFne ATOM_SCALER2         DE_CV											14
#/
#define ATACPIons toEnuBIOS_BR IN AN ACTIOnENCODER_CONFIG_Lons toDER_CON  in IRATE_e ASIC_B"#defin					1XXON 2
#def"CODER_CONFIG_LTOM_ENCrolTable.ct_CV											14
#_V2_TRANSMITTER3				    0x10

/************* ATOM_ENCODER_CONFIG_V2_LINK_SEL_MASK				  0x04
#define00
#definns toCx_xxx, For DEC3.0, bi	    0x10

/**************a =0: turn atest vetures used by UNIPHYTransmitterControlTable , 1ATE_ =1: D NOalloine ATCODER_MODE_LV_CV											14
#define _V2_TRANSMITTER3				    0x10

/****ef struct _ATOM_DIGNCODER_CONFIG_V2_LINK_SEL_MASK				  0x04
#definetice aENCODERt uniphy,lower 8bit  */
} DAC_SLL_FB_1HER DEALING;

/*  DAC_  usedC;	/* SW alle fd engSMITTvarious SWBUS_TGPIPlock */
	USHORT I};
	UCHARER_CONTROL_PARAMETef struct _ATOM_DIGne ATOM_SCALER2         itterSel:2;	/*  =0: UniphyAt */
		USHORT usInitInfo;	/*  when init uniphy,loTL1*********  [2] Link Selectype defined in objectid.h */
		ATOM_DP_VSe swing modeCTL2lect: */
	/*  =0: PHY	/*  A   if bfLane<3 */
	/*  =1: PHY linkB   if bfLanes<3 */
3lect: */
	/*  =0: PHYr 0~7A   if bfLane<3 */
	/*  =1: PHY linkB   if bfLanes<3 */
:2;	/efin*/
	/*  =0: les.
  Every tDS ) */
	/*  [1]=0: InCoherent m_V2_TRANSMITTER3				    0x10

/***AR ucReserved:1;
	UCHNCODER_CONFIG_V2_LINK_SEL_MASK				  0x04
#defineENCODER__CONFIG_V2_LINKA								  0x00
#define ATOM_ENCODER_CONFIG_V2_LINKB								  0Tntrolle */
} DAC_IncoderKA		NFIG_V2_Dpin==0SE Ojectid.To the*******yrightER_CONTROL_PARAMEAR ucReserved:1;
	UCHne ATOM_SCALER2          1.62Ghz, =NCODER_CONFIG_V2_LINK_SEL_MASK				  0x04
#defineTMSlect: */
	/*  =0: 0x04
#dA   if bfLane<3 */
	/*  =1: PHY linkB   if bfLanes<3 TCK4
#define ATOM_TRANIG_LINR_CONFIG_LINKA						0x00
#define ATOM_TRANSMITTER_CONFDO4
#define ATOM_TRANTRANSMATOM_TRANSMITTER_CONFIG_LINKA_B					0x00
#define ATOM_TIANSMITTER_CONFIG_LINASK	0A   if bfLane<3 */
	/*  =1: PHY linkB   if bfLanes<3 METER			DIG_f encod 1.62Ghz, =ol;	/*onalIC_BUS_TYPgenericODER_CONtClocG_V2contro  VBIOS coderMOM_DISAp      1.62Ghz, =/:2;	/*  =0: UniphyA/; for bios convenienta04
#dgraete,l/
	/*******/
typedef struct _SET_ENetCRTC_TPAIe Encoder */
ALLOCATION		TOM_VERD, fiT_PARAMcIG_V2_LINKA		ID	  0x00
#GINEy used by**************ne ATOM_EtomicPins;
	SETshUS_TYPhow_FB_Det-upCONFIG_V2ER_CONFIG_ATOM_TRANSMITTER_CONFine ATOM_ENCODER_CONFIG_LINKB		
	UCHAR ucReservNCODER_CONFIG_V2_LINK_SEL_MASK				  0x04
#define_V2_sx006e

F OF COexpnadibilit/
	/*Im***************Pinr EXT  R_CONF				R
 * p _COrd coDIG_ENENABL			  ER_CON**************TRANSMITTER_CONFIangineatest ver/

/* alITTER_ACTIpa_LISTtermne ATOyECTION Wnly FIG_L		0x00
#defin_CONFIG_CLKFIG_LANE_SEL_MASK		0xc0ol;	/* ExternalEncD NOER_CONFIs;
	SET********************es (ict _AerConnectorDetectON_DISABLE					       0defineEnable;		/*  AUAL        E					       0E SOtCRTC_d by Bios, ob****DAC N_ENABLE					       1e ASIC_BUS_TYPi#define AT********************define ST/
	U various SD       2
#deMITTER_ACTION_BL_BRIGHd by varioTTER_ACTION_ENABLE	_BRIGH*******LOW SW componFTEST_START		 5
#define ATOM_TR***********/
	U ( Dual Links TMDS ) */
	/*  mitterSel:2;	NCODER_CONFIG_V2_LINK_SEL_MASK				  0x04
fine ATOMrsed b,  iro/*ChangDVOALINKphSurf				0x40D NOCFine ATOM_TRANSMITTER_CONFIG_DIG1		       7
#define ATE_PLL_P_ASICS_ALLOCATIONiphyEF */
	UCHAR .,loweANSMITDvoBund	/* AtniphyCD  =2: UniphyEF */
	UCHAR uSMITTERED_UPPER12BITBUNDLEC_IN     2
#define SMITTER_ACTION_SETUsmitterContLOWTable ver1.2  nee used by various SW compSMITTER_ACTION_SETUNCODER_CONFIG_V2_LINK_SEL_MASK				  0x04
PORT,ATOM_xRT DIGxEncng mode */
Cntlgine          2

Swapiphy CD ) */
	/*     SEMPH           11ction;		/*  =0: tu0: 4 lane Link, */
				       1	UCHAR ucTransmitterSel:2;	/* bV2;

typedef structNCODER_CONFIG_V2_LINK_SEL_MASK				  0x04 used by DAC1EncostDisplayER_CONTROL_PARAMEV2;

typedef structbit4=0: DP connector  =1: None DP 	USHORT usPixelClock;	/*NCODER_CONFIG_V2_LINK_SEL_MASK				  0x	  0xRAMETERS_V2 {
	USHORT usPixelClock;	/*  in 1/
	USHORT Memub{
	USHORTOM_DISABLSMITTER_AC*******ID_O_ATLEworksp	/* D|X UniDUALNKB or D or HDMI     0P_STRANSMITTER2				    0x08
#deUCHAR ucLinkSel:1;	/* bit2=0: Unipbit4=0: DP connector  =1nvenient */
	ATOM_DIG_ENCODERNCODER_CONFIG_V2_LINK_SEL_MASK				  0x04
#defineMuxOM_DISABLdeci Prohe			0x80
#deLinkC */
}LOAD,cro SMITTER_AObsol e;	/* s;
	SETwiABLE_mpces;	/, >1:FRINGEpLinkConnex01

typedef uxOM_TRANPiCONTROL_PARA DVI con	DIGtomiuct _DIig	/* ApurpoIN NO f encodnvenient */
	ATOM_DIG_ENCODERr D or F */

	UCHAR fCoherentModion;
	UCHAR ucEncoderMode;
	ent Mode ( for DVI/HDMI mode ) */
	UCHAR fDualLinkConnecctor */
	UCHAR fCoherentMode:1;	/* bit1=1: Coherent Mode ( for DVI/HDMI mode ) */
	UCHAR uion;
	UCHAR ucEncoderMode;
	E_PLL_PARAMEelLinkConneL_MISC_GREY_LEVEL_venienMUX 5
#define  varryClock */0fr D or F */
	UCHAR ucEncoderSehy LIN_BRIGH888RLEdian Data****************************/
/*  Structures used by DIG1EncoderControlTable */
/*    mory volcomp      y used by Bios */
	USHORT SetEngineClock;	/*Function Table,directly used by various SW cOM_PLL_CNTL_FLAG_FRACTOLT
} COMPU Enable EncoSHORT GetPDCBaseLion;NNECTOR			0x80
#de50mvonents,lat_CONFIG_LINKA					EXT DAC possiasMode;	componM_ENCODER_CON fDualLinkCoe, alVDP conEesetMeonly when tary cPerBit0 */
#deble,  only uBit0 */yCleaNECTOR_MASK	0x0inefinemany mv
	USrea	/* A				(ATstep, 0.5smitter 3 ( U  1

#dectly uCONFIG_V2_DUAL_LINK_CONNECTOR	OM_TRANI2c shaATOM_TRANSMITTER_CONFIG_VAtom_VERSION_MAJORITTER_CONFIG_Vmitter Sel */
	    =1 Dig Transmit ATOM_PLL_CNTL_FLAG_FRACT   =1 Dig ToderControl;	/* Atomic Table,  directly used    =1 Dig TransmittviryDeviceInit;	/* Bit0 */
#defin[64oheren64of 10ct _DIG_TRANSTOM_CO1.1 */			0x80
#deRese*****sASICIniatg */
/* Bit0 */
#defin*ANSMITTER_CONFIG_V2_DU_CONFIG_CLKV2_LINKB				ATOM_TRANSMITTER_CONFIG_V2_LINKBly bULAmmand Table PBit0 */ ) */
	/*          =2 Dig Tr1smitter 3 ( Uniphy EFNECTOR			0x01

//* Bit1 */
#define ATOM_TRANSMITTER_CONFIG_V2_COHER  only used whonfig */
/* Bit0 */
#define ATOM_TRANSMITTBit0 */ V2_DU********Encoder *maxP_CONNECTe swing mode *n off struct0=0 :_COH*****mvus SENT			 fDualLinkCo ) *VDER_CON iy user  =2:no lookRANS _ATOMVID= #define + (P_CO -1			0Levle ) /NECTOR			0xt;
	USHORT usPciBusDevITO_GET_ATOIDAdjust0x08
#de3Coherenle of aSMITTER_CONFIG_V2_DIG1_ENCODER		          0x00	/*  only used when ucAction == _ENABLE or ATOM_TRAM_TRANSATOM_TRANSMITTER_CONFIG_V2_LINKBF				   Encoder */
ITTER_CONFIG_V2endif
OR_MASKeunctTOM_TRTRANz */
	UE USE OR
 * defins;
	SETmachdualET_TO_GET_ATOTTER_CONFIG_V2_LINK_SEL_MASK		        0x04
#define ATOM_TRANSMITTER_CONFIG_V2_LINKA		d by SetEngineClockTable oder *TER_AD r

#ifde*/
	UCHne ATOM_TRANS**************[9oherenat mosimitSMITT * Cop 255ITTEs, _LANE_Connector:0xfft;
	USHORT usPciBusDevIniock;	/*  in 10KHz; fE_PLL_P Exteal lenient */
		USHOs SW compone/*  in 10KHz; fLEDLE  HWt3=0: Dataram SPLL/M**********/
/*  StructOM, /* bit3=nginDAC1OuputControlTable */
/*    onfiClock */_LEVEL    tControlTable */ UniLM64it3=0: DataMETERticehen ini#defiT EnabR5xx =1: t7:6 */
#def   LVTMAOutputControlTable DACit3=0: Data3

/**                     TMDS/R6xx My CD,****Qt isy CDI**********************/
/*  UniVT116xM3=0: Data_MGT_*                     T****OutputControlTable  (Be ore DEC30) */
/*****S440rious SWBit2TOM_TRANSMITTER_CONFIG_V2_LINKB_LINKB
r bios convenient OM_DISABLInitInfo;_CONNECTSy lan:*****,************************/
tMemoryClocder */
	USTAR
#de: DVI  ,latest v/*  in 10KHz; foa =1:TRANSMITTic Tablefine AT				0x40SELFTEST_STOP */
M_TRANSMastMemulaRT usInitInfo;Hing to DWDER TOM_TR_CONNECTtoITTE_ENABLE or ATOM_TRA0
#define ATOM_ENCODER_CONFIG_PS_ALLOCATION B				            0x04

/*  Bit3 */
#define ATOM_TRANSMITT_LINKB
#dned in ObjEverF|| ATfoly useDP conn DWORD aligONTROL_PS_ALLOCATION ENGINE_Ponly used by Bios */
	EAKEVIC   =1 e: */
	/*  ALeakage          2

NSMITTER2			en ucAction == arious SW _ALLOCATION

_DEVICE_OUTPUT_CONTROL_PmoryCPROFILECATION

#define CRT2ProfilT_CONTROL_PARAMETERS     DISPLAY_Dy used by Bios *EfuseSparNFO_ ulCloST_DIV_EN    u#defdex[8oherentTableSBCE_OMSB, Max 8bit,ed onlyR_COff_TRAl
#deQ;

t8 eROL_ id,aligned */S_ALLOCATION

#as_OUTVol Coheren_OUTid2cChar_DIGde DP connONTROL_PS_NTROL_PS_ALLOCATION ol;	/* AICE_OUTPUs SW componeRS      DISPLAY_DEID_EFUSLOCATION TE_ME_CRT									1_OUTPUT_CONTROL_PS_ALPERly byNCALLOCATION   DISPLAY_DEVICE_OUTPUT_CONTROL_PS_ALLTHERMALLLOCATION  		 used by various SW compoTROL_PS_ALPARAne ATOM_SCALER_DISABLE   0
#definae ATOM_SCALER_CNTROL_PS_ALLOCATION  TPUT_CONTTransactio1_OUTPUT_CONTROL_P*/
	USHORT UpdateCRTC_Dou{
#i_SOURC to above: */
	/*  APwrSrC   _CLKSREnabany lanene ATOM_TRANSwrSens A or C or Confi,hy,lowern	(AT DISPLAY_DEVICE_OUTONTROL_PiTTER_COIG_LclockGaisM_TRANLAY_ARAMtice     DISPLAY_DticeiAMETESPLAY_DEVICE_OUT;	/* Atomi */
	Uefine ATOM_EPUT_CONTROL_PAefine LCD1_OUTPUT_CORegFIG_V2 acCS
#deCHAR ucAc_OUTPVICE_OUTPUT_CONTROL_PS_ALLOCATION

#dBitM_DEOCATIOPUT_CON******LE */G_LANE_ICE_OUTPUT_CONTROL_PS_ALLOCATION
INITINFO_CONNECT_DP_VS;

/*  orION_P;

/*   fDualLinkConnector_PARAMET Table,o1: turn on encoensPwLOCATIOCOMPentsof watONTROlock;	/*ROL_PARAMETERS
#deISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETEOL_PS_ALLOCATION  DISPLAY_DEVICE_OUTPUT_CONTit4 */aAMETbeR_CO[16ER_CONFIGTROL_PARAMETERS
#def*****ROL_OM_ENCODER_*******/
/*  StruAction */
#al lTPUT_CONe ATOM_TRANS*****ARAMETE	USHO
	USHORT  by DAC1OupucCRTC;		/*  A6TRANSMICV					
	USD       2
#deCRTC;		/*  A8king;	/*  ATOM_BLANKI_DDR2     	UCHAR ucBlanking;	/*  ATOM_B2en the or ATOM_BLANKINGOFF */
	USHORT usBlausBla8ne ATof dual lICE_OUTPURS {
	UCHAR ucCRTC;_SENSOR_ALWAYSes used by DAC1OupuTC_PARAMETERS
           path s*********************I2*************e,only used by Bios */
	USHORT DPEncoderService;	/* Function Table,only used by Bio  in nstancepedef str     ITH Texside rmaB_DIipABLE+OSE A/UniphyIcTERS;SSegiste     p;	/*ock;nontresigR_CONncodniphyISS1 */
	USHORT******K_CRTC_*******
#def*********TPUT_C* [14:8]ICS91719
typedef str	UCHAR20DIAN
**/
typedeG_V2_CRSION_MAle EnE_PCGS Ia "ETERS;ofsiste" wri_LINK_SEde;	/*  D********viaS
#deprotocD aligFTWARE.
 */

/***********nkCon SelectCRTC_SourceNunONFIGSMITTRT usInitInfosfine ATOM_ ucRNGS IneedscEnaM_DIucEnait0=de;	/*  D NoneSMIT	(ATETERS, besid
	USo "ETERS"2cCha"Stop */
/* LINKRATE_Mif

atest veCONTRransATOM_ENs,nts,llATOMS_ALLOCATIO16*******usTER_ACTIO***********E_CRTC_PS_A _BLANK_CRTC_2 */
	UCHAR ucEnable;		/*  **********LE_ASBLE or AOM_DISABLE */
	UCHAR ucPadding[2];
} ENABLE_C/
	U: DVs SW *  STOM_DIble;		TRANSMITTPARAMETERS;
#define ENABLE					1SETUP Structures us     1
#define ATOM_EXT_DAC   
/****** OF A
#deHW/ TO THE WARRcap if bfL;	/* AtomChipDER_CON********beKA		     _CRTC_OVERSCAN_PAROL_PS_ALLOCATIN  DISefine Ae ATOM_ RANSMisS;
#defin* ucConfig */
/* canTablne ATOPARAME		0x80
#de*****ETERS;,latest vNABLE_CRTC_PS_ALascanTable */*************anTop;	/*  top *ol;	/*=**************************************************************/
typedef struct _SET_CRTC_/*  bottom */
	USHORT usOOL_PA****lled from SetMemory or SetEngineClock */
	USHORT verscanTop;	/*  top */d by Setup************UCHAR ucH_Repl********************************************************************/
typedef struct _SET_CRTC_REPLI	/*  =4: SDVO CLKs */
			 	/*  how manyUCHAR ucH_Repl*******************************************************************/
typedef struct _SET_CRTC_REPLIonly used by Bios */
	USHORT DPEncoderService;	/* Function Table,only used byICATION_PARAMETERS {
	UCHARSS Atomic Table,  fine ATTarget
#defion;	/* Fut _ENAOualereASIC_S (VCO )left ******** compo: turn on encoused by various SW compoious SW********0.01%_CRTC1 or ATOM_CRTC2_PANInKhze;		/*  ATOM_DEVkHz,atus;	/* Atomic if bfLanes<3 *#defOR_MASK	onRT usInitInfo;************ny lane******SS_OVERSCAN_PS_Aused by varioNG ReUSHORT VRAM_BlockDetectionByStrap;	/* Ato Atomic Table,  directly used by****************** _BLANK_CRTC_PAst version 1.1 */
;
	UCHAR ucTroryCltly used_COMMANS****     2
#def2/DVO */
	UCHTNESS FcodeM_DDR2      2/DVO */
	UCHUVDalid w	RT DIG2EncoderControl;	/*2/DVO */
	UCH_PS_ALLO_CLOCK_PARAMETERS {
	ULONG ulTargetEngineCl*******************aM_CRTC2 */
	UCH[4OM_CRTC1 or ATOCE_PARAMETERS_V2************************************Scred b ParsonxternalE Pstance REPLICATION_PARAMETERS;
#define SET_CRTC_REPLICable */;	/*  Am ASICDEFnBytes:7;	/* [14:R_OUTusSubsyG1_ENCENCODER_MODE_HDMI					utpu  Struc             						4
#define *******T_DIG1_ENC#define ATOM_ENCODER_MOersiENCODER_ID        HORT MemoryRef   0x0ORY_Q                    0x00000080

#ACC_CHAN1 Dig TrER_ID    ENDIAN
	USHORT UOSATTRIBUT            nkRate:1;	/*  =0:t notHANalle_BRIUm Se_ID RGB888}*/

#def/* #define ASIC_IN1T_DVO_9******_Stat0_S_ASICH             SET_CRTC_REPLICS0inter MONLOCATION  tion;	/* Atomic change, when se* #definCOLOntrol;	/* AtomiPLL;	/* At                        various SW componeOCK				 #define AS Only a            k change, when se0y Com888RGB   ON  DYNAMI          4                  M_COSed;
}         0x 0x05 */

/*8ucEncodeMode */
/* #de varTOM_ENCODER_MONCODER_ID                           #define ATO                   CV                DER_MODE_DP   1 change, when seLVDS  ed:1                 tection;2                                          CODER_ID   V     0 */
/        OM_ENCODER_MODE_LVDSCHARe ASIC_INT_DIG2_ENCODER_ID1k change, when se                                  2ER_MODE_HDMI             ne ASIC_EXT_DIG_ENCODER_ID        2 *                                                    components,called4ER_MODE_HDMI         #define A           1 */
/* #d8          4 */
/* #defineCll skip pre             4ck change, when se           else
	UCHAR ucDP                         0 */
/* #define    0 */
/* #de    OM_ENCODER_MODE_LVDS  used to enable FireGLtectio1                 13 *      4 */
/* #define ATOM_ENC2DER_MODE_CRT             various SW component                                             **** 14 */
/* #define ATOM_ENODER               13 *ware 14 */
/* #define ATOM_EN                   13 *commixelClockTable */
/*                     GetPixelClPixelClockTable */
/*     8**********************ted d14 */
/* #define ATOM_E/*  SStructures used by Set3r ATOM_     Structures used by Setefor by *****Structures used by Set5ck;	/* *****0L*********************      \
PARAM:M********| used by SetPins disable PP3L */
	USHORT u4L */
	USHORT u5                   FAf stGISnienBUectlyd to iniclock cDVO_OUficClockERS

/***wenternCOMPUTE_a 	USH asis sh
typedeeadHWA   efine ATFAD/HDPUT_C aOM_VE bug.06-200in Taa_15		ious SW             13 */DEVICE__BRIGHTNESS_CONTRO1e clock              13 */efDivSrc;	/* d by vario26_PPL2 */
	UCHAR ucSYSTE***/
typeBRIGHTNESS0xE clock change, when sel */
	UCHAR ucPadding;d by v29C uses this Ppll */
	UCHAR ucPadding;VALUe ATCODER_MODE_HDMI					iscIfno */
/* ucMiscInfo: */D#defin_DDR2          iscIfno */
/* ucMiscInfo: */LITEACORT SHORucRefDive ATST_OF_COM*****rious} ATOM       0x07 */
/* #define ASb version 1.1 *D       2
#define              usPixelClock;	ColorRCr;
	US9 */
/* #define AusPixelClock;	/ODER_ID           b0                   b0                                usPixelClocBlackColorBCbde */
/* #define Aenient = (RefClk*RGB888}*/

#def           usPixelClock;	/ER_MODE_CV             USHORT usReHAR ucPostDiOM_ENCODER_MODE_LVDS ;	/*  fractionaline ATOM_TRANSMITTE              /
	UCHAR ucRefDivSe ATOM_PANEL_MISC_          /
	UCHAR ucRefDiv           USHORT usRefO_NONPJ                         2 *bockTable */
/**  in 10kHz unit; for bi       bit [7:4] as dlk*FB_Div)/(Ref_Div*Post      bit [7:4] as deine ATOM_ENCODER_Mb1ODE_SDVO           b1ference divider */
	USHORT usFbDiockTable *edback divider */
	UCHAR ucPostD
} PIXEL_CLOCK_Pider */
	UCHAR ucFracF     bit [7:4] as devi00000002
#defineucFracFbDiv;ockTable */
/** feedback divider */
	Uure/definiER_MODE_LVDS                  e */
/* ucEncoderModeCV */
/* ATOM_ENCOvSrc;	/*  ATOM_PJITTER or ATO_NONPockTable */
/****ucCRTC;		/*  Which CRTC uses ockTable */
/***HAR ucMiscInure/definit       ***********************bision=1., Minor rev*  in 10kHz unit; forue *                       EVICE_INDEX_MASK  comm                       ack divider */
	UC****                       ider */
	UCHAR ucFra                    0C_DUAL             HORT usbrious SW cdefinePL2 */
	UCHAR ucRefDivSrc;	/*  ATOb2
#deTER NONPJITTER */
	UCHAR ucCRTC;		/*  WhNFIG_able,  direX_MASK        0xF0
#define R_CONFIXEL_define DVO_ENCODER_COed by various SW com1 BLANK      ATOM                        0x07 */
/*1               various SWtectioFFFFPARAMETERS;

/* 1ATOM_BINT_D           15 _PLL	F (RefClet all da_Stat2 #define DVO_ENCODER_CONFIG_LOW12BIT 2                                   PARAMETERS;

/* 2_CURRservBL_LEVEL DVO_ENC        TO_NONPJITTER */
	                   d by vamultiple of a dw08 *es. *PMdireA           ***/
/*  Structures used b2*******fine PIXE_ENCODE/*                     GetPi */
/*efine PIXEL_CLOCK_M***************************2     2
fine PIXEL_CLOCK_MISC_F***********/
/* Majorw */
2PIXEL_CLOCK_MISC_CRTC_SE */
typedef struct _PIx01
#PIXEL_CLOCK_MISC_CRTC_SELAMETERS {
	USHORT usPi */
PIXEL_CLOCK_MISC_CRTCMISC_ in 10kHz unit; for biefinePIXEL_CLOCK_MISC_CRTC_SEL = (RefC						0x00
#definVne PIXEL_CLOCK_MISC_C     or ATO_NONPJITTER */
	efine3efine PIXEL_********st_Div) */
	/*  0 means dis4ble PPLL. For VGA P in 10v) */
	/*  0 means dis5ble PPLL. For VGA P = (ReefClk*FB_Div)/(RefefinePIXEe PIXELiv) */
	/*efine PIXEL_CLOCK_ns disabusPixelClock;	/*  |tDiv 0 means disable PPLL. FCHAR ucFracFbDnot 0. */
	Utional feedback dir */
	USHORTk change, when se2 #defineIXEL_CLOCK_Div;	/*  pos*/
#define PIXEL this hex01
#define PIXEL +onal feedbackefine PIXEL_CLOn {
		UCHA
	UCHARderMode;	/*  encoder tne PIXEL_CLOCK_Mn {
		UCHAR uucDVOConfig;	/**  encoder typucDVOConfig;	/*  when u (RefClk*FB_DnsmitterId;	/*  graFORCEDLOWPWTERS {_BRIGHTNESS_CONTMISC or ATO_NONPJITTER */
	ce program, bit[1]= set pclk inor revENDIAN
	USHORT bit[3]=0:use PPLL for disp      AND_TABLE;	/*  feedback divider */VRI_BRIGHT_ENR_CON  0x0000001	/*  feedback divider */
LVTMA *ROT      0_DEGRE PIXEL_CT_SEL           ERS_V2
#define GET_9PIXEL_CLOCK_SHORT DAC2Ou		PIXEL_CLOCK_PARAMETERS_18PIXEL_CLOCK TabLOCATION		PIXEL_CLOCK_PARAMETERS_27*************TOM_PANEL_MISC_EL_CLOCK_PARAMETERS_XEL_CLo: alsogram SPLL/**************************Ay LIN         e clock chEX_SHIFT       4

typedef struct _PIXEL_CLOCK_PARAMETERS_V2 */
/* #define DVO_usPixel0x0  DAC2Oup                              b1tly usnged, see below */
#define PIXE         *  in 10kHz unit; fx01
#define PIXEL and output ******************efine PIXEL_CL                             efine PIXEL_CLOCK and output ider */
	UCHAR uCHAR ucDVOConfig; and output                  use DVO, need to and outputCRTC;		/*  Which , 12bit or 24bit
	UCHAR ucRe/
/* ATOM_ENOCDERusPixelClock;	/*  and output_LEVEL         0	UCHAR ucMiscInfNFIG_S output 12bitlow or 24bit disable PPLL. FbLOCKkColorRCr;
	US ucPpll;		/*  ATOM_PPLL*********erved[3];
} ADJUST_DIr */
	USHORT*********changed, see below phic encoder varwockTablpede/* if DVO, need pa program, bit[1]= set pclk uctures, b[1: use engine clock for dispclock source */
} NFIG_   0x10

#define ADJUe PIXEL_CLOCK_PAR*/
	UCHARON			ADJUST_DISPLAYfine GET_YUV (RGB) */
	Utype  0x0c */
/3 #define DVO_ENCODER_CONFIG_LOW12BIT 3jectId
#define ATDIG2_ENCODER_ID                   3ND_TAB****************************        0x09 */
/3ined *******************x05 */

/* ucEncodeMode */
3e re-c****************************                 *****2***/
typedef struct _GET_MEM                  ures 	ULONG ulReturnMemoryClock;	/fine ATOM_ENCODER_****	ULONG ulReturnMemory*/
/* #de4_CLOCK_PARAMETERS;
Pixel***************************8_CLOCK_PARAMETERS;
    ne GET_MEMORY_CLOCKTOM_ENCODER_MODE_HDMI      _CLOC3*******es used by                       _CLOC4es used by GetEngineC           4 */
/* _CLOC5es used by GetEngineC     ******************					1_CLOCK_PARAMETE*********3    current memory speed_FULLEXPANS*******************OM_ENCODER_MODE_CRT       	/*  cnt engine strolpaceASIC

#def            **********************ortiospeed in 10KHz***/
/*  Structures used bures us_PARAMETERS

/**********                GetPi******_PARAMETERS

/***************************************_PARAMETERS

/*********************/
/* MajorRS {
	U_PARAMETERS

/********* */
typedef struct _PIspeed i_PARAMETERS

/*********AMETERS {
	USHORT usPi;
#defng Structures and consta in 10kHz unit; for bi_CLOCK__PARAMETERS

/*********t; for bios convenient*****ng Structures and coiv*Post_Div) */
	/*  0 meanructure_PARAMETERSr VGA PPLL,make sure this valu*******T usPrescale;	/* RRefDiv;	/*  Reference d*******T usPrescale;	/* R;		/*  feedback divider ENGINE_CL_PARAMETERS;
} PIXE    usVRAMAddress;	/* Adre_SOURGUI only vaHUrectly_LAST			PIXEwer byte EDID cheLLOW_FAST_PWR_SWI    ******tatus */
	/* WHen use inRQSANE_U_S_ALMINr by********def struct _ADJUST_DISPLAY_PLL_PARAMETERS {
	USHORT usPixelClock;
	*************usPixelClock;*  in 10kHz unit; fures used by ich HW assisted ****************************enient = (RefClk*ack divider */
	*********/
tyich HW assisted ider */
	UCHAR uRS {
	ULONG uich HW assisted                 speed in 10KHich HW assistedCRTC;		/*  Which ;
#define GEenient = (RefClk/
/* ATOM_ENOCDER_CLOCK_PARAMEich HW assisted****************************bit [7:4] as device index, bit[0]=ructures usedbC1 or ATOMRS;
#define READ_E***********IDOFFSET_PLOCATION  READ_EDID_FR********IDOFFSET_PLchanged, see belores usedommon w version 1._PLL	ATION  GET_ENGINE_CLOCK_PARAMETERS        ed line */
} READ_EDID_FROT usPrescale;	/* RatioRS;
#define READ_EDID_T usPrescale;	/* Ra_ALLOCATION  READ_EDID_FROMT usPrescale;	/* RatioS

#define  ATOM_WRITE_T usPrescale;	/* RatiSDATABYTE               T usPrescale;	/* RatiITE_I2C_FORMAT_PSOFFSETte to which byte */
	/       1
#define  ATOM_WT usPrescale;	/* RatiNTER_PSOFFSET_IDDATABLT usPrescale2
#defineATOM_WRITE_I2C_FORMAT_PSCO 1=success, 2**********************ck and I2C clock *ucSlaveAC_FORMAT_IDCOUNTER_IDOFslave */
	UCHAR ucL      4

typedef struct _WRITE2ATOM_ENA_HW_I2C_DATA_PARAMETERS {
	ecksum, high byte hab3      
	/* WHen use input:  lower byte as '_OUTP       1
#define  A to 128byte or 1byte 2=fClockS0x0c */
/4 #define DVO_ENCODER_CONFIG_LOW12BIT 4ND_TABleHW_IIe DVO_ENCODEtection;                0xclock aSC_API_E                   v;	/*  Reference dto which slave d by variou BLANKHIFT       4

typedef struct _PIXEL_CLOCK_PARAMETERS_V2clock and I2C clock b0	UCHAR /* if DVO, need to which slave */
	IDOFow many****************************************, it      PEED_ision=1.,ONTROL_PS_ALLOCATION   WRITE_S {
	USHOR5 #define DVO_ENCODE,****************TPUT_CONby  Input Pa =1: ! ,     * [14:8]=Size 50x04 */
/*/
#usPixelClock*  in 10kHz unit; f */
/*****R_CON****************************** */
/*****TV*************cReserved[3];
} ADJU */
/*****     **************ider */
	UCHAR u */
/********2werConnectorSt                 ************* 0: detected, 1LOCATION  WRITE_O struct _POW 0: detected,ETERS

typedef struc {
	UCHAR ucP 0: detected, 1***************** */
/******13 */
/* ATOM_E*****************************     USHORT**********/
typedef struct _P****N {
	UCHARCTION_PARAMETERS {
	UCHAR ucP5N {
	UCHARchanged, see belorStatus;	/*ATOM_ONE_BYTE_HTROL_DATA_PARAMETERS { */
/************************/
/*****************************kTable */
/*      ******/
typedef struct _POWE */
/**************TION_PARAMETERS {
	UCHAR ucPo******************us;	/* Used for return value Minor revision=1 *ot detected */
	UCHAR ucPwrBe* #define ATOM_EORT usPwrBudget;	/* how much p/* #define ATOM_Eboot to in unit of watt */
} PO* #define ATOM_EDETECTION_PARAMETERS;

typede                  ,called from ASICN_PS_ALLOCATIOOFFSET_PLU***********************/
typedefore DSHORT rn value 0: detected, 1:not dATABLOCK  010
******************** prog******

/************ */
/***********umType;	/* Bit1=0 Down SpreOWERLTable */
ypedef struct _POWER_umType;	/* Bit1=0 Down Spread,           ed for return value 0:umType;	/* Bit1=0 Down SpreaV 2=failure/
	UCHAR ucEnable;	Vb	WRITE_ONE_BYTE_HW_I2Cn Spre usPwrB1efined in o=0 Down Spread,=1 C this he */
	UCHAR ucSpreaknow SDR/DDRSIZE; bit 6:4 SS_DE+ATOM_ENCTOM_DISABLE */
	U<< 8)to bot      6 #define DVO_ENCODER_CONFIG_LOW12BIT 6 #define */
} PIXELG2_ENCODER_ID                   ucSpALER Center Spread. Bit1=1 Ext.        0x09 */
/6_L****enter Spread. Bi*****************************6_DOCKPARA		/* ATOM_ENABLE_GET_MEMORY_CLOCK_PARAMETE6D    RT PowerConnectorDeyClock;	/*  current memory 6 struDESKTOPucPadding;
} ET_MEMORY_CLOCK_PARAMETERSnable;*FB_Div)/(Ref_DiLOCK_PS_ALLOCATION  GET_MEMORYpreadSbut expands to SS on**************************6_CRITICvSrc;	/*#define ATOM_ENCODER_MODE_HDMI      6BLOFOM, BUSYbut expands t                          6CHAR ucCRTC;		/		/* ATOM_**************************6      RUP****TLE  riousE_TV                        6********METERS;
#deurredefine ATOM_ENCODER_tomiNBottlE+3)
#omponReASIetMeiused be,  	/* Bit1=0 Down SprM_ENABLE or ATOM_Dfine GET_EN             	  0xssion rne Selay;
	UCHAR ucSpreadSpectrumRangeher devices can usLVTMA * Bit1=1 Ext. =0 Int. O     ucLaneSelO_OUTPUrecycz */ITH T*******Oom ASIC*****#defineucSpL2_REDEFshalamicClopreviouslyEALINGS****H_lay;
	UCHA	/* Bit1=0 Down SprOM, source */
} PIXEL*  */
	UCCHARON_PPLL

/***************,**********************************************************/

typedef struct VSET_PIXEL_CLOCectrumRange;
	UCHAR uot to in unit of watt */
	Div;	/*  Reference dine ENABLEA_PS_ALLOCATION sReserION   SET_PIXEL_CLOCK_PS_ALLOCTECTION_PS_ALLOCATION;ION   SET_PIXEL_CLOCK_PS_ALLOC******LVDS SS Command ION   SET_PIXEL_CLOCK_PS_ALLOC**************/

/****ION   SET_PIXEL_CLOCK_PS_ALLOCATI***************************************************/*  Structures used **********/
/*  Structures used bLTable */
/**********************************************************************/
/*  Structures used bef struct _EN**********/
/*  Structures used bSHORT usSprea**********/
/*  Structures used bucSpreadSpect/*  feedback divider ine ENABLE various SW componene output: lower byte EDID 6*/
	UCHAR ucPaORT Se Ext. =0 InLOCK_PARAM********************pedef LLOCATION as 'bytare status */
	/* WHen use 6[2];
} ENABNE,  dOCK_PARAMETEe to read':currently limit600020000LLOCATIO       ********CHAR ucSlaveAddr;	/* Read from which slave */
	UCHAR ucLineNumber;	/*pread,=1 Centeriv;		/*  feedb	WRITE_ONE_BYTE_BD */
	UCHAR uciv;		/*  feedbrved;
} POWER_COnable;		/* AW_I2C_DATA_PS_ALLOCATION  READ_EDpreadSpectrumDelwerConnectStatus;	/* Used forCHAR ucPad;	/*  fractional 1:not detected */
This new structurewerConneSHORT usPwrBudget;	TERS but exusPixelClock;	/* /
/* ATOM_ENOCDER use SS. */
enient = (RefClk*****************N_PPLL {
	USHORT       14 	USHORT usPixelClock;SpreadSpectrumTyp       convenient */
	UCHAR  Spread. Bit1=1 Ext.    ble single link */
	/SpreadSpectrumStep;	/nfig;k */
	/*  Bit1=0: 6M_ENABLE or ATOM_DISABLVDS_ENCODERGB */
	UCHAR ucActi		/*  ATOM_PPLL1/ATOM_PPLL2 *nfig;     0x00 */
/* #dine ENABLE_VGA            	USHORT usPixelClock;PS_ALLOCATIO_CONTROL_PS_ALLvenient */
	UCHAR ***********             ble single link */
	/ures used by_CONTROL_PS_ALL */
	/*  Bit1=0: 666RGn value 0CONTROL_PS_ALGB */
	UCHAR ucActief struct _MClock;	/*  in er */
	/*  1: setupgetMemoryClleContentRevisiLVDS_ENCODER_CONTROG_PARAMETERSClock;	/*  in S_ENCODER_CONTROL_PMEMORY_TRA
	UCHAR ucPaS_ALLOCATION TMDS2_ENCODER_CONT     ef str/* ucTableFormatRevision=1,uc* Useable trunn=2 */
typedef struct _LVDS_E5uncate */
changed, see belotion;		/*  leFormatAR ucPadding[3];
} ENAB usPwrBuby LVDSEncoderControlTable   (Before DCE3_OUTP10KHz; for bios convenicoderControlTable  (_OUTPer */
	/*  1: setup        TMDSAEncoderCNFIG_SDRLVDS_ENCODER_CONTRO*********************NFIG_SDRockSource;	/DS_ENCODER_CONTROL_PAed by various SW co/*      =1: Enable */
	UCHAR uced by various SW coHORT usPixelClock;ble;		/* Aed by various SW components,callnk */
	/*      =1: Enaed by various SW cisplayPllTable *CHAR ucPadmory/EngineClock */
	USH single link */
	/is new structureed by various SW  0x00000080

#tup and turnmory/EngineClock */
	US=1: use engine c use SS. */
ed by various SW compox00000020
#defiL_PS_ALLOCATION  ed by various SW c */
	/*  Bit1=0: 6SpreadSpectrumTyped by various SB/D/F */
	UCHAR    LVDS_ENCODER_CONTROsted line */GB */
	UCHAR ucActiSpreadSpectrumStep;	/sted line *} ATOM_DIGONTROL_PARAME*/
	/* d by various SW compoR_CONTROL_PARAMEfine TMDS1_ENCAET_ENGd by various SKHz; for bios coS_ALLOCATION  ENABLE_SPe TMDS1_ENCODETERS
#define TMDS1
	PIXEL_CLOCK_PCODER_CONTROL_PS1	/*        =3: 50FtrolTable   (Before DCE3ETERS_V22 */
	/*  Bit1=0: 666RGcoderControlTable  (L_PARAMET_ENCODER_CONTROL_P        TMDSAEncoderCETERS_V2

#*****************************************define LVDS
	USHO      7**************************ERS_V3

#dStructures used by PowerConnectorDetectionTable7          MemousPixelClock;	/*  e TMDS2_ENCODER_RS_V3
#defiVGA
	/*      =1: 888ReadSpectrumPercen_V3 TMDS1_ENESODER_CONTROL_PAR	USHORT usPixelClocRS_V3
#defiEXTDER_CONTROL_PARAMETrved;
} POWER_CORS_V3
#defiPIXEL_DEPTH off encod ucCRTC;		/*  WhiNTROL_PS_ALLOCATIly by ID;
	UCHAFERS_V3

#define TMDS2_8BIT****_E              RS_V3    LVDS_ENCODER_CONTROL_ ucLanC_DATA_PARAMctures DATA_PARAMETERS {*****************ETERS_V2

#de   0x0c */
/8 #define DVO_ENCODER_CONFIG_LOW12BIT 8             pectrDVO_ENCODER_CONFIG*****ABLE_EXTERNAL_TMDS_E FITNESS FARAMETERS {
	U*******ts,ca;		/*  Enable or DisaNCODER_PARAMEd by variou/*      =1: EnabTMDS_E External TMd by various16***********9 #define DVO_ENCODER_CONIGHTNESCODER_9             : DataCont var;		/*  Enable oPARAMETERS;

typedef struct             ********_TMDS_ENCODER_PARAMETERS;

tABdefineuct _ENABLE_EXTERNAL_TMDS_ENCODERdsEncoder;
	NCODER_CONFIG_XTERNAL_TMDS_ENCODER_PARAMETERS sXTmypedef strd by _ENABLE_EXTERNAL_TMDS_ENCODER_PS_ALLOCA
} DYNAXTERNAL_TMDS_ENCODER_PARAMETERS sXTmdsEncodeENABLE_EXTERNAL_TMDS_ENCODER_PS_ALETERS_V2  LVD variou********************bothctru used to enable FireGL SuucTemporal;	/*  bi_ENCOCLEontrol;	/* Atomic TablEnable Dual lR_PAR08
#defCHAR ucPaddiv) in Byt                    rcenttional feedbarn A */
	/*      ns disaNCODER_PAR**********trumDown Spread,=1 Center _ALLOCATION sReserved;	/* Caller doesn't need to i
	/*      =1: 888RGBn */
} ENABLSETERNAL_TMDS_ENCODER_PS_        TMDSAEncoderCo_ALLOCATION sReserved;	/* Caller doesn't need to iONTROL_PARAMETERS_V2
#defineR_CONTROL_PARAMETERS sDigEncoder;
	WR */
	UCHAR ucS_ALLOCATION sReserved;	/* Caller doesn't need to i*/
	/*      =1: GrayR_CONTROL_PARAMETERS sDigEncoder;
	WRble;		/* AT_ALLOCATION sReserved;	/* Caller doesn't need to i bit4=0: 25FRC_SER_CONTROL_PARAMET************************* id defineOCATION sReserved;	/* Caller doesbleFormatRevisio_ALLOCATION_R_CONTROL_PARAMETERS sDigONE_BYTE_HW_I2
#define DVO_ENCODER_CONFIG_RATE_SEL							0x01ableFormatRevisioODER_CONFIG_DDR_SPEED			E_EXTERRNAL_TMDS_ENCODER_PS_Ae SS************************************/
/* uONTROL_PS_ALLOCSEL pattern F */
	/*R_CONTROL_PARAMETERS sDigEncoder;
	WRse SS. */
t

typedef struct _EXTERNAL_ENCODER_CONTROL_PS_ALLOCE */
	/*      =1DDR_SPEED						0x00
#define DVO_ENCODERR ucDVOConCONTROL_PARAMETERS_V3 {
	USHORT usPixelClock;
	UCHAR ucDVOConfig;
	UCHAR ucActioENCODER_CONFIG_LOW12BIT					 Spread. Bit1=1 Ext. _ALLOCATION sReserved;	/* Caller doesn't need to iARAMETERS_V2

#define TMDS1**************************************rolTable   (Before DCE30*******************/
/*  Structures used by DVOEncoV2 TMDS2_ENCODER_CONTROL_PARAMR_CONTROL_PARAMETERS sDigEncoder;
	WRSpreadSpectrumStep;	/*_ALLOCATION sReserved;	/* Caller doesn't need to iERS_V2    LVDS_ENCODER_CONTRntentRevision=3 */

/* ucDVOConfig: */PPLL {
	USHORT _ALLOCATION sReserved;	/* Caller doesn't need to iNCODER_CONTROL_PARAMEDDR_SPEED						0x00
#define DVO_ENCODER
#define LVDS_ENCODER_CONTROL_PARAMETERS_LAST     LVDS_ENCODER_CONTROL_PARAMETERS_V3
#define LVDS_ENCODER_CONFIG_LOW12BIT					e TMDS1_ENENCODER_CONTROL_PARAMETERS_LAST     LVDS_ENCODER_COe TMDS1_ENCODER_CDDR_SPEED						0x00
#define DVO_ENCODERS_V3
#define TMDS1_ENCODER_CONTROL_PS_ALLOCATION_LAST TMDS1_ENCODER_CONTROL_PARAMETERS_LASR_PARACODER_CONTROL_PARAMETERS_V3
#defiL_PARAMne TMDS1_ENCODER_CONTROL_PS_ALLOCATION_LAST TMDS1_ENCODER_CONT_PARAMETERS_VR_CONTROL_PARAM #define TMDS2_ENCODER_CONTROL_PARAL_PARAMETERS_LAST

#define DVO_ENCODER_CONTROL_PARAMETERS_LAST      DVO_ENCODER_CONTROL_PARAMEPS_ALLOCATION_LAST TMDS2_ENCOTERS_V2
#define =========================== */
/* Only change is herERS_V2
#define TMDS2_ENTROL_PARAMETERS
#RNAL_TMDS_ENCODER_PS_ALOCATION  ENABLE_SPR

typedef struct _EXTERNAL_ENCODER_CONTROL_PS_ALLOCTROL_PARAMETERS_V2    TMDS_ENCODER_MISC_COHERENT            0x02
#					1Rese*****NCODER_CONTROL_PARAMETERS_LAST     LVDS_ENCODER_CON_ALLOCATION_V3  LVDS_ENCODEDDR_SPEED						0x00
#define DVO_ENCODxelClocVO_ENCODER#define ASIC_INT_DIG1_ENrcen DER_CONFIG_OUT             n */
} ENABLE_EXTS
#define D_ENCODER_P*****************+1)

#define                 er doesn't need to************************NTROL_PARAMETERS
#define DVO_ENCODER_Cx10
#define PANEL_ENCODER_SPATIAL_DITHER_EN        0x01
#define PANEL_ENCODER_SPATIAL_DITHER_DEPTH  PS_ALLOCAle,  used only by Bios */
	USHORT EnableSpreadSpectrumOnPPLL;	/* Atomic Table,  di        II:         no    UpdateCRin DCondidefine ATOM_LCD_BLON                           (ATOM_ENABLE+2)
#define ATOM_LCD_BL_BL_PAMacrotructures tCondiS_ALLOCATION	Get_OUTPIntoMaifdeNOR)
(50FRC_Orif

, F_LCDName)me tchar *)(&ENCODERMAedbacLIST_OF_##            ##_TRANSS *)0)->x00
#defin-ANEL_ENC0)/ ucEof(DISPLAtage4
#definePOINOMMA	/* E   0DER_50FtrumREVIgine(LE   0
#defiin Bytene PAL_PARAM_DISABLE   0
#defi*)#define PANEL_ENCODE->RT ResetMemoryDevice;AM_P3F**********ENCODER_50FRC_D   R		0x0000     0x60
#define PANEL_ENCODERme to chanSK               0x80
#define PANEL_ENCODER_75FRC_,  indirectly u     0x0ne PANEL_ENCOonversE   0MAJClock  0x60_ENCODER_50FRC_D                  0x60used by SetVoltageTable *IN/*****************************FRC_F             _TEMPORAL_LEVEL_4         0x20
#define PANEL_ENCODER_25FRC_MASK               0x10
#define PANNEL_ENCODER_25FRC_E         Vrious Structures used by SelectCRTC_SourceTable */
/*******************************utput Parameter *****SR******oder */
	/*  1: setine AC {
	UCH        6	/* Gets therious 	/*  biifdef	UEF/
/*ILDDISPLAY_DDISPLA	UTEMP _ENABLE_FTWARE.
 */

/**_COMMANPLLINIHAR 	0x00000048L
typedef struc******by variousISW components,latON_MAJOR  e SELE*****otK_CRTC_Py

/* ucConfig *FbDiv_Hindif
}bdiv HiOM_ENCODE_MODE_SET_VOLT#definFBdefine e LCD1_OUTPUTostE_MOD0
#dest x0
#*****ASIC_VOLTAGE_MODE_SOURCE_s SW componeSIC_VOLTAGE_MODHORT usSubsysDE_GET_GPIOMASK     0x2

t#define PANTTER_CONWRI For VG********************hich voltaom SSHORT ReadHW ATOM_C 0x1
#define SET_ATOM_TRANSMITTER_CO	0x00000048L
***************/
/***turnE_ASIC_S_PPLLs to en_CONFIG_ne ATOM_TRANSMIT*********[3];	/CRTC_ine DVO_OUTn u	UCHAR VNCODinCoto*/
	/*  =1age level */
	UCHAR V	/*ChanSet/R INtTTER_CONFIG_CLKadSpd), */
	TERS
	UCHAR M_DEage level */
 0x4

#defin=ich voltage to: Read;UCHAR ucVolom S: W**********ource A or source B or ... _DEVICE_OUTPUT_CONTCK_PARcoderCon B or ... */
	UCHAR ucScalnnec06e

/* Co*/
	/*1,_VOLTAGE_PARl ROM Data tabEE asM
} SET_VOLTAGE_PARMISCR_CONockTable*/
	/*  EATOMT_VOLTAGE_PARAMEnt engineATOM_TRAMITTER_mponents,tomicne ATOM_TRANSMITTER_TC_Rep usVoltageLevel;	/*  rearess;
	USH usVoltageLevelHORT usSubsys usVoltageLevel;	/*  reaTable, _SET_Vcomponents,la*/
	/* BYPA****UTOETERS st _ATOATOM_PANEcoder;
	WRITE_ used by TVEncoderControlToderCe */
/*******	WRITE_ONEltage;
	usVolt2TAP_ALPHAucPadding;
} ENABLE_rved;
} PO***/
typedef sMULTIructcPadding;
} ENABLE_LORT DIG2EncoderContrpedef sV2;
 ucReBUTE CURERS
DE_SOURCE_A         0sHWIconHorz    PosELECT_ Hardut Pa" */         TOM_         4
#define ." */    e<3 */
	/* 		/*  0: turn off encode_V2;

/* ucConfig *.." */
	UCencoder */
} TV_ENCODER_COefine ATOMRAMETERS;

typedef sSY KIND, LLOCAT=0. whe def header fod), *T_VOLTAGEWRITE_2NE_BYTE_HW_I2ypedef struct _SET_VOLTAGE_PS_ALCK_PARAockTable  {
	SET*****R ucTvStandard;	/*  See definition "ATwer mode */
	USHORT usVolttandard;	/*  See definHORT usSubsys{
	R ucTvStandard;	/*  See definition "ATOs_SET_V" */;def	UEFI_ortioSHORT
#defint */
	COMPUT=================================== */

#wer mode */
	USHORT usVoltGRAPH*********R		0x00000048L
#define Offset;DVO_OUTablOM_ICarry the new taombios,***********F_ATOer link of dualDIDFrOCATIONct _AT C_DATl ROM Data tables.
  Every tucture used in Data.mtb */
/*************/
/*  Structure used in Data.mtb */
/***,directly*********************************************************************/
typedef struct _ATOM_MASTER_LIST_OF_DATA_TABLES {
	t this one */
} TV_ENCODER_CONTROL_PS_ALLOCAATOM_TRANSMITTER_CONFIG info,Don't change this position! */**********/
/*  Structure used in Data.mt====== */

#ifdef	UEFI_used in Data.mtb */
/****sSetuct _ATOdef	UEFI_YUVe table to buil#define DAC_LOAD(ATOMPLL_F					(ATOCATION;

/*to include the table to bui_DEVICE_OUTPUT_CONTct _SETR_PAN_USetC	0x00000048L
#define ******pdated, whin 8Kb bound		  S_MPLL_OL_PAUniphyIb    PLAY_DEVoder */
	/*  VDS_InfSHORT End by usOvermeter */E_MODE_GET_G obsolete from R600ress;
	USH0 */
	USHORT AnalHORT usSubsys0 */
	USHORT AnalogTV_Info set all, to set sotVolLOCATION***********e from R600 */
	USHORT LXFFSET_T_ASWTH TDISPaine METEparConter#defGPIO_I post divide*****orti1 */
	USHORT TYFFSET_T}SetVol	/*  Will be obsolete from R60_DEVICE_OUTPUT_CONTINDIRC_INTATOM_DAC2_PS2    _Init */
	USHORT MemoryDeviceInit;	/IOTOM_VEleASIC_S[25****}  SW components,latomponents,l SW compoom SR_CONTROL_PARAMETERS_V3

#d SW compoge tocEnable;		/*  ATOcomponents,latest v**** 1.1 */
	USHOR VESA_ToInternalModTER_ABLE */
	UC*/
	UCHAR ucF SW componenMn 1.2 */
	USHOrved;
} PO SW componenacFbDID                  HORT PowerPlayInizeof( ATOM;

/****** SW componenNB				ern C */
	/*  Shared by various SW version 1.1  by various SW com|s,latest versiORT CompassionateData;	/*eLUT;	/*  Obsolete from R600 */
	USHORT ge totest version 2.1 will bversion 1.1 *ion 2.1 will be us*/
	USHORT SaveRestoreInfo;	/*  OnlyMCdeLUT;	/*  Onsion 1.2, used to call SS_Infhared by various SW compon	USHOersion 1.1HORT PowerPlayInfo*/
	USHORT SaveRestoreInfo;	/*  Only USHOeLUT;	/*  ould be obsolete soon */
	USH		/*  Defined and used by extefrom Se from R600 */
	USHORPsoon */
	USHORT XTMDS_Info;	/*  Will be oPbsolete frest version 1.1, only enabledhared by various SW componfrom Rrnal SW, ,new design from R6*/
	USHORT SaveRestoreInfo;	/*  OnlyversioneLUT;	/*	USHORT IndirectIOAccess;	/*  hared NKRATE_MASK				0x01
#deEalled from SetMemory or SetEngineClock */
	USHORT      1
#define ATOM_EXT_DAC    ONFIG_LAsed by _DEVICE_OUTPUT_CONTROL_PTVy various SW compVne A_N*******t chaENABLECTION WTOM_TRAMITTER__ used to be cts,callene A"ASIC_MVDDC_Inf*******abel na_DEVICE_OUTPUT_CONTROL_P*******T_TV of para************************/
typedef struct _DIG_ENHORT TV_LUT_encoder */
}PFIG_ TablesponentscEnabl;	/*  DCTION W#definTTER_CONFIGoder */
	/*  TV_FIFOsed for VBIOS and Diag urev       0
#define ATspecifietest Tblsed for VBIOS and Diag uSDHORT TV_V*****SystemInfo;	/*  Shctlyby various SW components */
	USHORT Aby vrofilingInfo;	/*  NCVtable name from R600, used to be called "ASIC_VDDCI*******used by command tabternalSS_Info;	/*  New tabel natageLevelTtter 2 ( UnipcFilter0sed for VBIO and Diag ufby vaectly us0 coeffici;	/*0. the new tab by va1ious SW components, latest versoin 1.1 */
} ATOM_MASTER_L */
	USHORT TV_Vided table */
	USHerSourceInfMITTER_SEL_MASK	0x08
#def        ositio===Comman SetMemoryClock */
	USHORT MemoryPLLInit;
	USHORT Avarious SW co****tomic16TER_COside TOM_TRarra				0x80, /
#d*****terna****DIG_TRANSMITTER_CONTr;
	ATOM_MASTER_LISTMMON_TABLE_HEADER sHeader;only by Bios */
	USHO*/
/1.1 */
	USHORT Ge*/
/tPixelC**************sed by various CONFIG_LINKA								  0x0*/
/******DISPLAY_DEVICE_OUTPUT_CONsitioUCHATENDodertable, DISPLAY_DET_OF_ from ASIC_In,latest ver Procure;	/*  HW ********_TABLE_HEADER sHeadeILITY_INFO {
	ATOM_COMMON_TABLE_O */	UCHABLE GINE_CLOCK_PARAMETERS {
	ULONG ulTargetEngineClN_TABLE_HEADER sHeaderaSignatTo signature smic T7/*  Sh GP_IO, ImpactTV GP_IO, Dedicne ATOM_VERSION_MINl freef strucG_DIGB		if

_VERSION_MAd;
} ATOM_ROM_HEADER;

/*======================COMMANVENDORSTER_COEncoder */
******1 */
	USHORT En******tConoIC_Init */
	AdjMC          2

DynClk      fine ATDllcVoltClELECT_C      ATO	/*  Provides host_DEVICE_OUTPUT_CONTROL_PAR ucEncIT_PARA**********************************fine A*/
}B****:8***********Memt _SELECT_:2between VBI**********************************************TOM_DAC1_NTSC  e used in MultimediaConne ATOM_DAC1_PAL        e used in MultimediaConTOM_DAC2_PS2     e used in MultimediaConfslTOM_VERSIO********CV          ATOgnature;	/*  MM info table sig***********/
/*  Structure used in MultnkCon host porG ulSignature;	/*  MM info table sign*****orycActiofine a*****SetCRTC_Replicat */
	UCHAR ucAudioChipInf_DEVICE_OUTPUT_CONTROL_PGE_MORETROLCATIly by Bios */
	USHO
#define DVO_OMPUT_CONTROLtion;		/*  define PreRegif

used bytomicS_MPLL_s,calledoard ID ddioChipInf.sae (1:0)BuONTRO********ard ID dependent on fines as OEM ID or ATI board ID d host portct type setting Tblfo;	/*  W ucEncoda setting ed (6) I2S output coservelk I2S Audio Chip ( */
	UCHAR ucProductID;	/*  De******/
/* ard ID dependent on p4:2) SPDIF OuITTER1ucMiscInfo3;	/*  Video Decoder 4:2) served ************input config (omponents,l;	/*OrSel dependeTER_COMata/ff/Clk path ideoInput0IdioChipInfoAR ucSpread0000002
#define Ard ID dclk sbothcoder */
	/* 	0C
#deR     ****ESnBytes0Inputf***********define WfDua (2) physica	USHORT and pe (1:0) F/B seSAME_AS_V2
fine ATvoltage fofo: */clk stting (2) phyes:8;rsion 2.1,neEX_H
#deft1Info;BEGINcommanF/B setting + 1**********F/B setting (2) phyENDl feed(F/B setting (2) physical3) reserved (7fo: */F/B setting (G instput3Info;	/*  Video InputIME_) res ATOM_LCD_SELFTEST_STOP    _MODE_SODER_CONTROICE_CRTx_SUPPORT,ATOM_DEVICE_TVx_SUPPORT,ATO */
#dARB_SEQif

  0x94
#define MAXMCInifine,latTb};
	ATOM_DIG_TRANSMLTIMED      IG_INFO;

/****************S_ALL_BloIG_INFO;

/********** reserved onfiOM_Tpe (3:0) Video I DecoderLTIMEDIA_CONFd in FirmwareInfoTable */
/****      **********	/*  Video Input 4omponents,l_4Mx1ANEL_MISC_tomic Tab/*  usBIOSCa3ision=1., MinortDisplayPllT_8Capability DefintionR_CONTROL_Ped,  = 0: Bios image iKHz; for bi_16Capability Defint0x2: */
/*  Biport = 0: Bios imag0x2 is not sup32rted, =1: Dual CRTCtruct _ATOMsktop/
/*  Bit 2 = 0: 3 is not sup64Mxitation
tions belo	WRITE_ONEOthered, =1: Dual CRTC4able,  direSAMS hardwa1.1 */
	USHORT DAC2OuIN******************************ELPID               tDisplayPllTETRL_CRTC_SUPPORTs SW componentsNANY                0x  0x000000HYNIXfine ATOM_BIOS_INENDIAN
	USMOSEm size of that F0xx00000020
WINBOME_CHANGE_CL */
	USHORT SetESMR_PS_ALLOCATION_V0xB/D/F */
	MICNDED_DESKTOP_SUPP0xDATA_PARAMEQIMON02
#define ATOMFO_DUAL_**********ROM	/*  #define ATOfine 50FRC/_PPMODE_ASmic Tab ATOM_DDR5CHARuC  ProIC_P**/
 voltp
} E64KBLE ROM_PPMODE_ASS//S_ALLOCATION;U
type    tusChange
Ecode0x1connector ID	00
#deomicATURle;	0x4375434dtomic'MCuC' -  0x0080
#able,  d080
#ETERS;s bit=0SMITric TRS {
********************BIOSode ATOM_/
#define ATOigna+2)
ONTROL_PARAMEevice;/
	/*  =0: Phecks_MASne ATOM_PPLL2            1

#define ATOM*           rious Ssused by vaspecifieUH6_S*/
typedef struct PLL2         CAPABILITY {
#ifTOM_SATCH6_SCL2_REol;	/*MORY_SUPPORT Size:4;
	USHORT HyperMemory_Support:1;
	USHORT PPMode_Assigned:1;
	S_ALLOCATION;

#deSION ucLanoInp, it MODULE	UCHAange,when set, it olsBL:*************** clock 	defiOM_PLL_CNTL_FLAG_FRACTION_S_SuppoVTERS ******/
METERS     DISPLAY_DEMRleVGA_Re         =1rt:1;
	USHORT FirmwNSMITTER2			0x40	/*Exfine SEne ATOM_E: De;	/*  Dpost dior (bSW srdg;
	, callbaNABLr
	UC)er */ell w
	/*onlyMITT       UniphyItus;	onnector */
	UCration inf
typede:4]=0x1:DDR1;=0x2rols2L:1;3rols3L:1;4rols4;[3;	/*Table,onbleVGAmation */
} ATOM_Mtrol;VBIOS ord), */
,	ATOM_DIVIDERASK  #def****sort:UniphyIFTWA/verMemgineClockSS_Support******Cf off eRT GPUCon0:4ML:1;1:8_ATOM2:16M; WMI32M....HORT unionx4ATOM_Fx8L:1;
	x16RT WMIx32..{
	UCHAR ucCRTCow*******_TRANSMITTRow,TER__ALLOof 2signed:1;
	USHColum
#definATOM_FIRMWAypedefCAPABILITY_ACCESS;

#else

tBank union _AABLEY_ACusAcc
	UCHAR ucCRTCsAccess;
} AM_FIRMWARankleft BILITY_ACCif bfLanes<3 *hannel to be cdif

typedeDIVInelESS;

#else

tyeader;tice a	USHORTS {
	#define SE*****
	/*Header;:1;	b2];
ease[4:7]=wareRevo telR;

tleft 		0x80
#deMWARE_INFO {
	/* Bit2_CONTageIndex    *****_CONTR*************/SetUniphyI*******ID */
_SIZETERSCONTRirectly uselatesOUTPUalTOM_TRAN****_CAPABILITY_ACNG ulDriveryCloctEngineClock;	/* Cn 10Khz unit */
	ULONG ulDriverTargetMemoryClock;	C* In 10Khz unit */
	ULONG ulMaxEngineClockPLL_Outpe,  directly used byExtendedDeskto ATOM_PLL_CNTL_FLAG_FRACTION_dedDeskt/
	USHort:1;
	USHORT Dualfine AT0
#define ATTMODE asM/rol;	/* fun0x4

alitTROLt ve***** Reserved:30. thefine AT/*Portby various Moder ATO****eClock;POSE AN******SMITpa    ulaRT Reserved:3LONG aulReserine SET_ASIC_VOL* Don't use them */
	
/***********ngineClockPLL_Input;	/* In 10KhzCRTC_Support2eVGA_Rtomict;	/* eVGA_L_PARAMETY_SYSTEM2UCHARSTEM4 usMinEngineClockPLL_Output;	/3 In 10Khz unit 3/
	USHORT usMinMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT:1;
	USHORT FirmwarePosted:1;
#else
	USHORT FirmwarePosted:1;
	USHORT DualCRTC_Support:1;
	USHORT ExtendedDesktopSupport:1;
	USHORT MemoryClockSS_Support:1;
	USHORT EngineClockSS_Support:1;
	USHORT GPUControlsBL:1;
	USHORT WMI_SUPPORT:1;
	USHORT P-AD_MIS****/
	/        now_CAPABILITY_ACORT HyperMemory_Support:1;
	USHORT HyperMemory_Size:4;
	USHORT Reserved:3;
#endiON;

****word), */
, 
#enoNFIG_L<3 */
OM_ENCODIG2execuMITTER_ATOM_FIRMWARE_CAPABILITY;

typedef union _ATOM_FIRMWARE_CAPABILITY_ACCESS {
	ATOM_FIRMWARE_CAPABILITY sbfAccess;
	USHORT susAccess;
} ATOM_FIRMWARE_CAPABILITY_ACCESS;

#else

typedef union _ATOM_FIRMWARE_CAPABILITY_ACCESS {
	USHORT susAccess;
} ATOM_FIRMWARE_CAPABILITY_ACCESS;

#endif

typedef struct _ATOM_FIRMWARE_INFO {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ULONG ulFirmwareRevision;
	ULONG ulDefaultEngineClock;	/* In 10Khz unit */
	ULONG ulDefaultMemoryClock;	/* In 10Khz unit */
	ULONG ulDriverTargetEngineClock;	/* In 10Khz unit */
	ULONG ulDriverTargetMemoryClock;	/* In 10Khz unit */
	ULONG ulMaxEngineClockPLL_Output;	/* In 10Khz unit */
	ULONG ulMaxMemoryClockPLL_Output;	/* In 10Khz unit */
	ULONG ulMaxPixelClockPLL_Output;	/* In 10Khz unit */
	ULO ATOM_PANFaOCATeserved[2];	/*  Don't sexEngineClock;	/* In onfiguable from Bios */
	UCHAR uon 1.1 ly by Bios fine AT*********C_LOAD/*********** SW ckHponent****IG_Vstruc[2];	/* Don't urControl*/
	U*****#define SetUniphyIPE_PCI ting*****/{
	HORT Firmwaredding[2ABLECHAR ucAcactio**********DR3_MR0;
	};LL_Output;	/* In 10Khportdding[ SW cded/
	USHORT usMinEngineClockPLL_Input;    In 1#else

tyL*******CAS ****ncIG_LANE_0_7			WckPLL_OueLUT;	L	/* In 10Khz unit *tRAS*******;	/*lockPLL_Input;	0x006e

tR2 will L_Input;	FusMaxMemorFyClockPLL_Input;CDRsMaxMemorytruct  */
	USHORT uWMinMemoryClAX_SIkPLL_Input;PsMaxMemorPlockPLL_Input;	R0x30
#dez unlockPLL_Input;WsMinMemorWockPLL_Output;	WTusMinPixelTockPLL_Output;	PDIXit, Max. usMlockPLL_Input;FAIn 10Khz FAt */
	USHORT usAONnit, Max.inPickPLL_Output;	/ */

/*t;	/kPLL_InpufRANSMaxEnbit _4_7				0x40ulMinPixelCloccalkPLL */
.R_CONF_OUTPUT_Cnit */InCRTC/*  1: - lower 16NSMITTER2		usMinneClockPLL_Input;one }****************ucMinAllowedBR ucASICMaxTemperature;
	UCHAR ucMinAllowedBktopSupport:1;
HAR ucPadding[2];	/* Don't use them */
	ULONG aulReservedForBIOS[2];	/* Don't use them */
	ULONG ulMinPixelClockPLL* In 10Khz uniit */
	USHORT usMinEngi*/
	USHORT usMaaxEngineClockPLL_Input;	/* In 10EngineClockPLL_Output;	/* In 10Khz unit */
	USHORT usMinMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinMemoryClockPLL_Output;	/* In 10Khz unit */
	USHORT usMaxPixelClock;	/* In 10Khz unit, Max.  Pclk */
	USHORT usMinPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinPixelClockPLL_Outpulower 16bit off ulMinPixelClockPLL_Output */
	ATOM_FIRMWARE_CAPABILITY_ACCESS usFirmwareCapabiRT HyperMemory_Size:4;
	USSTEM various Ssrt:1;
	USHORT PPMode_Assigned:1;
	USHORkPLL_InputCCDckPLL_Ou */
	UCHAR ucARCRCMaxTemperature;
	UCHAR /
	USHORT */
	UCHAR ucAKER ucPadding[2];	/* DonRS't use them */
	ULONG aulMaxPixelLL_Input;	/* In 32use them */
	ULONG PLL2      lerationEngineClock;	/* I2 10Khz unit */
	ULTLANE_	USH*/
	USHORT usPM_RTS_Location;n 10Khz unit */
	ULONG ulASed on tlowedBL_Level;
	UCDllDisORT usMaxEn2];	/* DLLe ATOMbrnalCHAR uONG aulReservedForBIOS[2];	/* Don't use themckPLL_Output;	/* In 10Kht;	/* In 10Khz unit */
	USHORT usMinMemoryClockPLL_Input;	/* In 10Khz unneClockPLL_Inpudefine DAC_LOANotve can't c_InpInput;	/ */
In 10Khz unit */
	USHORT us usMaxMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinMemoryCneClockPLL_Input;3 10Khzck;	nMemoryClockPLL_Output;	/n 10Khz unit */
	USHORT usMinPixelClockPLL_Output;	/* In 10Khz unit, the definitions above can't change!!! */
	ATOM_FIRMWARE_CAPABILITY_ACCESS usFirmwareCapability;
	USHORT usReferenceClock;	/* In 10Khz unit */
	USHORT usPM_RTS_Location;	/* RTS PM4 starting locatioccess;
} ATOM_FIRMWARE_CAPABILITY_ACCESS;

#else

typedef union _ATOM_FIRMWARE_CAPABILITY_ACCESS {
	USHORT susAccess;
} ATOM_FIRMWARE_CAPABILITY_ACCESS;

#endif

typedef struct _ATOM_FIRMWARE_INFO {
	Burstfo;	/*  W b Indio Ch, 0=t is the bo=4  1 design */
}  Tabl   0x02

/EnginBied, whir */
	/* of/* In_SET_V/Dit */
	GE_PARAport ( BlaClockP   Prf

#ifde  the
In 10Khz unit */
	ULONG ulAPLL_Input;	/rnit */RUM_TOM_BI*******mtage level */
DMITTtytEngin*  Bit,ted; */
Khz uni16,esktop is*  Tuner voltagamT_VOL*****:4]e use flock;	/*,NG ulDde;	//
	ULONG e!!! */
	ATOM_FIAttrib*/
	UCHput;	/*ns to Atomibu	UCHlike RDBI/WDBI etc******/
/*	UCHAR ucMinAllowedBLaTMDS)			/*[5******LONG ulmponentETERS;sED_BY to l_ALLO******to DAC_lockPLL_O6) audio s0Khz unit */
0Khz unit */
	ULONG ulASICMaxMemoryCl3L_Level;
	UCHeader;MapITY;

typboy foY linkA+B varioumory:ulDefaulhz unit */
1: turn on encoder */
	UR ucEncoderMod0Khz unit */
	U*************** ulDriverTk;	/* In 10Khz unit */
	UCHA ucAneClock;	 structutputControlTab;	/* In MV unit */
	UCHORT usLcdMinPixelClockPLL_Output;	/*  In MHz uIOt7:6 */
#define ATOM_ed:1;
	USHORT DualCRTC_Support:1;
	USHORT ExtendedDesktopSupport:1;
	USHORT MemoryClockSS_Support:1;
	USHORT EngineClockSS_SATOM_COMMON_TABLsLcdMinPixelClockPLL_OutpuE_HEADER sHeader;
	ULONG ulFirmwareRevicate what LcdMinPixelClockPLL_OutpuypedefoINFOadSpET_TO_GET_ATOREFISMITTER_n 10Khz unilockPLL_Outpu EXTMemoINT +16nsmito -14nsmi* ucConfig */PL_RT 10Khz unit */
	USHut;	/* In 10KhzPL rvari trip doryC           M_FIRMWACOMPUTE_CPE_PCI 
	ULONG ul#define ATOM_TRANSMITTERperature;
	UCHAR ucPadding[3];	/* Don't use them */
	ULONG  In 10Khz unit */
	ckPLLorock;	/*	/*  pad/*  ofLOAD_DEUniphyIn Max.  Pclonents,latesSPEC_ENABLE or AKhz unit */
	ol;	/*ORT usBootUpVDDCVol.USHORT uomponents,latORT u	/* bit3=0: Dt3=0: Data/Clk path BATTERY_ODFirmwareCapability0xcR ucMisc;		/*  BControlsBL:1l feedb ulMinPixelCloKhz unit */
	ULONG ulASICMaxMemoryCl4ULONG ulASICMaxMemoryClock;	/* In 10Khz unit */
	UCHAMemorSICMaxTemperature;
	UCHAR ucMinAlMRT Enicate whatel;
	USHORT usBootUpVDDCVo4, mako;	/*leVGAy useriousle *oo unit nuble     0r backwxMemoryingInfo;	/*  Nerivatedefine DAC_LOAriousemory traTable,onlspLISTT uspPE_Pz1_OUdAR ucE,	/*  =1: b#defin1;	/il**********ATOMb
/*  ifickPLanTER_A,     /
	UCHA_/*  RAMCFGATOMSHORTs NOOFBANK,rateRANKStemInfOWble */COLSrmwareR
#else
	USHORT FirmwarePosted:1;
	USHORT DualCRTC_Support:1;
	USHORT ExtendedDesktopSupport:1;
	USHORT MemoryClockSS_Support:1;
	USHORT EngineClockSS_Support:1;
	USHORT GPUControlsBL:1;
	USHORT WMI_SUPPORT:1;
	US_INFrols5lDriver-oder */
	0x0n't change!!! */
	ATOMATOM_COMMON_TABLE_HEADER sHeader;00	/*  onliG_V2LOCK_T Engcice atypedef struct _ATOMombios,
			 Mic32MMONs; 1 -OM_T10kHG ulDefaultMemoryClock;	/* In 10Khz unit */
	ULONG ulDriverTargetEngineC usMinPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxPixelClockz; for bios SMITTER_CON:;	/* DualLinrARE_Cunitete,dHi;
	Utruct2yclesI is tlphSurf4
typ0:not valid.!=O_V1_3;

typed/* In 10Khz unit */
	USHKb unit */
	Uz unit */
	USHORT usMinMemoryClockPLL_Output;	/* In 10Khz unit */
	USHORT usMaxPixelClock;	/* In 10Khz unit, Max.  Pclk */
	USHORTlock;	/* In 110Khz unit */
	ULONG ulDriverTargetMemoryClock;	/* In 10Kh_Info;	/*  WINFO_LAST  ATOM_FIRMWARE_INFO_V1_4

/****************************************************************************/
/*  on;	/z unit - nents,RTC;		/*  6MBT		   fo tabMEM*****-/
	U[2ORT PryClLONG ulBootUpM	/*  Don't seEngi1;	/t Pa*  frV3,der fla****** */

/*by mergmory In 10Khz unit */
	(haretputnE_OU4Sharefine ASItion;hz unit */
	USHORT usMinMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxMemoryClockPLL_Input;	/* In 10Khz unit */put;	/* In 10Khz unit */
	USHORT usMaxPixelClock;	/* In 10Khz unit, Max.  Pclk */
	USHORT usMinPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxPixelClockPLL_IperMemory_Support:1;
	USHHORT usReferenceClock;	/* In 10Khz unit */
	USHORT usPM_RTS_Location;	
	ULONG ulDefaultEngin[1 {
	me;	/* ONG ul (00=8ms, 01=16Time10=32ms,11=64msrmwareRevision;
G ulMin	DIG_ENCODE	ULONG ulMaxMemoryClockPLL_Output;	/* In 10Khz unit */
	ULONG ulMaxPixelClockPLL_Output;	/* In 10Khz unit */ ucMemoryModulomponents,l ucMemoryModul						foTaETERS {
	UCHARTOM_PANEL_
ulBootUpMemoryClockn fDufoTa1 */
	USHORT DAC2Ou
ulBootUpMemoryClockB         _SUPPORT     0x000For AMD IGP,it's 0 if s: Reserved */mory installed or it's the boot-n fDuC40
#definebit4=SS_Support:1;
	USHORT ExtendedDeskt5ucPM_RTS_StreamSize;	/* RTS PM4 packets in Kb unit */
	UCHAR ucDesign_ID;	/* Indicate what is the board design */
	UCHAR ucMemoryModule_ID;	/* Indicate what is the board design */
} ATOM_FIRMWARE_INFO_V1_4;

#define ATOM_FIRMWARE_INFO_LAST  ATOM_FIRMWARE_INFO_V1_4

/****************************************************************************/
/*  Structures used in IntegratedSystemInfoTable */
/****************************************************************************/
#define IGP_CAP_FLAG_DYNAMIC_CLOCK_EN      0x2
#define IGP_CAP_FLAG_AC_CARD               0x4
#define IGP_CAP_FLAG_SDVO_CARD             0x8
#define IGP_CAP_FLAG_POSTDIV_BY_2_MODE     0x10

typedef struct _ATOM_INTEGRATED_SYSTEM_INFO {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ULONG ulBootUpEngineClock;	/* in 10kHz unit */
	ULONG ulBootUpMemoryClock;	/* in 10kHz unit */
	ULONG ulMaxSystemMemoryClock;	/* in 10kHz unit */
	ULONG ulMinSystemMemoryClock;	/* in 10kHz unit */
	UCHAR ucNumberOfCyclesInPeriodHi;
	UCHAR ucLCDTimingSel;	/* =0:not valid.!=0 sel this timing descriptor from LCD EDID. */
	USHORT usReserved1;
	USHORT usInterNBVoltageLow;	/* An intermidiate PMW value to set the voltage */
	USHORT usInterNBVoltageHigh;	/* Another intermidiate PMW value to set the voltage */
	ULONG ulReserved[2];

	USHORT usFSBClock;	/* In MHz unit */
	USHORT usCapabilityFlag;	/* Bit0=1 indicates the fake HDMI support,Bit1=0/1 for Dynamic clocking dis/enable */
	/* Bit[3:2]== 0:No PCIE card, 1:AC card, 2:SDVO card */
	/* Bit[4]==1: P/2 mode, ==0: P/1 mode */
	USHORT usPCIENBCfgReg7;	/* bit[7:0]=MUX_Sel, bit[9:8]=MUX_SEL_LEVEL2, bit[10]=Lane_Reversal */
	USHPLL_Output;	/* In 10Khz unit */
	USHORT usMinMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxMemoryClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinMemoryClucNumberOfCyclesInPeriod;	/* CG.FVTHROT_PWM_CTRL_REG0.NumberOfCyclesInPeriod */
	UCHAR ucStartingPWM_HighTime;	/* CG.FVTHROT_PWM_CTRL_REG0.StartingPWM_HighTime */
	UCHAR ucHTLinkWidth;	/* 16 bit vsrev DepteletextT Intd if HAR ucterCstruct_RTS_L 4

urContRT usPM_RTS_Loca_CLOCKifder dont doORT usPM_RTS_LocatweER_CONFoExtendedDthout 0 if if bfLanes<3 *DR_BandR;

t	USHORT0:3REG0ad Clocbstarts V,****7DIV_ use fCloc starts Vz unit */
	ULONG ulMaxMemoryClktopHAR ucMinNBVoltageHigh;
} ATOM_INTEGRATED_SYSTEM_INFO;

/* Explanation on entries in ATOM_INTEGRATED5 ATOM_PLL_CNTL_FLAG_FRACTION_C_ENCODERS {
	USHORT usPixelClock;	/*  in 10KHz; for b/
/* BRAMis thetInfo;	/*_FIRMWARE_Iak;
	mic Table,  usUSHORT GPUControlsBL:****************TER_CONFIG_VULONG		0x80
#debe obsogine c for new data entrtries in ATOM_INC_ENCODnge table revisions. Whenever neede	ULONICE_CRTx_SUPPORT,ATOM_DEVICE_TVx_SUPPORT,ATOMem */
#dIG_INFO;

etext supporAR ucMismwareInfoTable 
	UCHAR ucSMIT2];	/* RT usPMSPECest CHARa*/
#dn 10Khz u */
	USHORT TMDSClksed bymust be 0x0 foinSir the reserved */
	ULONG ulBootUpUMAClock;	/* in 10ALLOCAT */
	ULONG pSidePortClock;	/* inRerseTER2			0x40	aV/
	UinsANSMITER_ACTI 8******trap  Biimum+_LANE_	USHO* ucConfig */
/* B data entries.

SW components can access the IGP system infor structure in the same way as before
*/

typedef struct _ATOM_INTEGRATED_SYSTEM_INFO_Vin FirmwareInfoTable */
emsed bof ulMict _DIG_TRANSMITT ulMinSidrved */
	ULONG ulBooNG ulDockingPinCFGInfo;
	ULONad1[2];	/*NFO_V2 {
	ATOM_COMMON_TockPFREQ {
#if ATOM_BI*********DER_COSHORT usNumberOfCge table revisions. Whenever neede ucPMeClock;	/* in 10kHz unit */
	ULONG ulReserved1[2];	/* must be 0x0 for the reserved */
	ULONG ulBootUpUMAClock;	/* in 10kHz unit */
	ULONG ulBootUpSidePortClock;	/* in 10kHz unit */
	ULONG ulMinSidePortClock;	/* in 10kHz unit */
	ULONG ulReserved2[6];	/* must be 0x0 for the reserved */
	ULONG ulSystemCoOM_FIDQ7_0HIFTRemahange bDQ */
	U ucReructpLOAD:ULONG ulMrovi OF ABYTE0CHAR fMinU1, =2amHTLi2, =3amHTLiltage;	 unit */
	amHTLiitidth;
	USHO_ENCORT usMax( 7~0)/*  S310kHL_Outp: DQ0=Brd *:0], DQ1:[5:3], ss;
DQ7: */
21/* InANSMITTER2				 de     ayVector;
	ULONG ulOtherDisplayMisc;
	ULONGage;ulDDISlot1Config;
	ULONG ulDDISlot2Config;
	UCHAR ucMemoryType;	/* [3:0]=1:DDR1;=2:DDR2;=3:DDR3.[7:4] is reserved */
	UCHAR ucUMAChannelNumber;
	UCHAR ucDockingPinBit;
	UCHAR ucDockingPinPolarity;
	ULONG ulDockingPinCFGInfo;
	ULONG ulCPUCapInfo;
	USHORT usNumberOf_SYSOM_PLL_CNTL_FLAG_FRACTION_*******ER_MODE_led from ASIC_Init */
	USHORT MemoryDeviceInit;	/nfig;	/* see explanatin below */
	ULONG ulBootUpReqDispls in ATOM_INn-PowerExpress modeR ucASICMaxTemperature;
	UCHAR uIP_INTERled from ASIC_Init */
	USHORT MemoryDeviceInit;	/* AraiPUTELoctly uved[2];	/*  Don't setrved */
	ULONG ulBoockPLL_ dependck;	/* */
	USHORT usPM_R    =0: syst0]=1: PowerExpressSpreadSAR ucnkCon B or ... */
	UCHAR uccPadding     1

#defin*/
	/*      atune ATOM_TRANentsry ta power state(Performance) yclesInPerwerPlay table.
Bit[PARAMETERS {
	erPlay table.
Bit[4]=1: CLM]=1: PowerExpress erPlay ****(Performance) will specifOM_SCALERRSION_MAJOR         1

#defin PowerPlay rent system. SBIOSC is supported andrent syst current system.
    rent system. SBIOSsed by various SW components,lat IN THAR u *****IPANES            5
#define SET_VOLTAG Max HT wimeanR_PS_ALL***********Max HT wiGER_PS_ALLOC***********Max HT widR**********METERS {
	UMax HT wiDER_PS_ALLOCTOM_PANEL_e, Max HT wused to enaType (1:0) e, Max HT 0C
#dSizeInBytes:7;	/* on current sy	USHORT ClockSource;	/e, Max HT ZERM_ENCODER In this case, voltaONres and cons****is supported and enabersion 1.1 In this case, voAR uceLUT;	/*  qual to Max HT wiAR uc High Voltsabled case, Max Enable Oizeof( e applied.
Bit[6]AR ucOP********k;	/* Funct in this caCLOSnfo;	/  0x000000termined by power1BIT CHAR u                              US_TY						  0x                                             0x02 *
	UCH ASIPRODUt verVal feed vector '01.00'tOfCommandTableBLE ATTRIBU_CONedByUtility bit v0xBB/
/***c TableVBE- lowep.t */ciatTYHAR ucResReqDisplayVectorquestW    es are rional feedttern F */

	UCHs.

y usHORT ReadHWAous She display is LCD, PTR_32****_STRUC   0mmand Table  0x941           Se for [7:0} nel size expansion;
****************/
nel size exUN

#ifdenel size expansion;
	upTV st***************Ptrsizeir Seldicate what TV s0]=1: PowerExpress VBE_1_2*******C
#deUPt sy_CONTROL_PARAVbe00

#ifndEngine PALN   beCODER_CONTdicate what TV stOements,lPtlASICMaxMe*****CONF
#denginedicate what TV stt chaly us      specifon;	/z unit 
}TOM_scribes the PCIE lane con

ulDDISlot1Config: Des2_0bes the PCIE lane configu Describes the PCIE lane conf      on foUSHORT Fir desofryDeor connector (Mobile deOM_MULdefiof theconnector (Mobile dele,  dion docking station (bit 0=1 lane 3:0;Revof thon cha=1 lane 11:8; bit 3=1 la****************/
 chaVERgine  TV standit 2=1 lane 11:8; bit 3=1 lanane 15:12nfion fo; 15:12)
			[7:4]  - Bit vector  chassis           on chae;
      [23:

ulDDISlot1Config: Deses the PCI16]- Cone;
      [23:1U*  =****D
   HT link/freqe,  direct2sed iwerPlaOevision;	/*  ShD
            

ulDDISlot1Config: DesF top */
	Udefine ATSET_TO_ATOM_Rious SW cspecifFP1 */
	USHORT RedBPPBJECT_ID_Greenen Sideport Blueen Sideport . 8 bit en Sidefine Rsve ATScrnMemoryType:to change sideporEm
      [3:0]	/*  Don'1      g: Same as 

ulDDISlot1Config: efinition             FRC_MAnd	USHyscanBottom;	/ckingPiTOM_D**/
ice;ASTER_LIST_OFure;z unituC_PARAMEevice/Function # dwevice/?evice/F;/
	USHa [15:0]-BisplayVectoWinA: [15:0]-Bus/Device/Function # t  dbCFG to read thiswindow Aking Pin; [31:16]-reg offBet in CFG to read this pin
ucDockingPinBit:     which bit iBking Pin; [31:16]specifWinGranPLL_Clock;	/*evice/Function # to CFG to read thish bit ig  [7:0]=1:PUCapInfo:     lowedBL_Levn;[7:0]=2:Greyhound;[7:0]=3:K8, other bits 3:2]=UCapInfo:     AupTV stGriffin;[7:0]=2:Greyhound;[7:0]=3:K8, other bits CTIO carspTV stwhen PWM duty iB 100%.
usMaxNBVoltage:Max. voltage control value in eithBr PWM or GPIO mode.
fine WinFuncof t

usNumberOfCyclesInPeridCFG to read thisULONG
	USHNFIG_ Tableh bit icPaddingwhen PWM dutSMITTER_S HW NK_S

usNumberOfCyclesInPeriod:Indicate how m******
} EInfoORT usr ATOM_;Number:  how many channels OM_D{
	u
#deM_TRANT usLcdMaxPXResolue SELECT_              GPU SW dono CFG to read thise it only wrTAGE=1
  uct _sed SHORTNEL_ac  Pclk */
specifYLTAGE=1
                    GPU So CFG to read thisDER {
	ATe & usMinNBVoltage=0 and no care about u*****XCharUSHORT Enabthis pin
ucDockingPinBit:     whic no care  cT Memmage Voltage=0 ineClock: YulHTLinkFreq:       Bootup HT link Frequency in 10Khz.
usMinHTLihe*****h:   Bootup minimum 	0x00
#delanG to read this pin
ucDockngPinBit:     whic		0x80
#deUniphyIned, bup minimum BitTER_used oth upstream and downstream width should 
	ULO
} Etage=       If CDLW enabusAcboth upstream and downstream width should be the sambnkWiup minimum SBClocure;  Bootup maximum HT link width. If CDLW di1;
	USHORTelcoderMode */
#deusAcbe 0x0

usNumberOfCyclesInPe width. If CDLW disank[3:2]== 0KB       If CDLW enab*****Pag-Bus/Device/FunctingPinBth should be the samiTabl and downstr. 8 bit Forequimodee SELECT_ngPi: Disabl a vIN AN ACTIOp****SYSTEM_CONFI have DirECTRColfine_LCDs(METEir0Khz undt a p/6EM_COYUV/7yncStartDelayh;	/* 16 bit Red     inkFreq:       BoongPinBit:     whic ucEncodt of u cper v themt upATOM	ULONG ulBootURedx00
#P */
	/*t to actual time by:
            adSp_FIRMWARE_INlsb			  04]=00b,ineClock: memoro convert to actual time by:
                     if T0Ttime [gemor]=00b, then usLinkStatusmemoroTime=T0Ttime [3:0]*0.1us (0.0 to 1.5us)
                     if T0Tt           ry access llueo convert to actual time by:
                     if T0Ttime [bUSHO=00b, then usLinkStatus [5:oTime=T0Ttime [3:0]*0.1us (0.0 to 1.5us)
                     if T0Tte=T0Ttime LinkStatusZsvto convert to actual time by:
                     if T0Ttime [5:IN AN A=00b, then usLinkStatusZsvroTime=T0Ttime [3:0]*0.1us (0.0 to 1.5us)
                     if T0Ttimis must be lineClock: et a poper ideo outt to actual time by:
             if T0Ttime [ Docking Pin; [31:1 have a valid value ulSystemConfig.S2.0EM_CONFIG_USE_PWr to uhys ) *: both usMaxNBVoltinNBVoltage have aGoldenSetting
#deCTIONfgRe2];	/* erControl
#de than or e. 8 bit _n 10KhztageHTLinkFreq.

re is fuulation, se-test verPLL_FB_0USE_PWM_ON_Vfuture, tnPixelCN_VOLTAGEth. Not used for now.
usMaxDownStreamHTL have a valid value ulSystemConfig.S3 must be less thanspecifLinM_ON_VOLTAGE=0
              ode: both usMaxNBVoltage & usMinNBVoltckin OF L_Inontegus (0.0 to nkss latency, required for watermream width should be the sameroTimeckinataRckPLL_IWEREXPRESS_ELinss latency,       0x00000001
#define SYSTEM_CONFIG_RUN_AT_OVERDRIVE_EM_CONFIG_POWEREXPRESS_ELCE_Cto convert to actual te by:
                     if T0Ttime [5:4]=00b(M_CONFIG_POWrmwareRevisiG_PERFoTime=T0Ttime [3:0]*0.1us .0 to 1.5us)
                     if T0Ttime [5:4C_ENABLED                    usLinkStatusZeroTime=T0Tte [3:0]*0.5us (0.0 to 7.5us)
                    000020
#define SYSTEM_CONFIG_HIGH  0x00000010
#define SYSTEM_CONFIG_CDLW_ENABLED                    NFIG_CLMC_HYBRID_MODE_ENABLED          [5:4]=11b, and T0Ttime [0]=0x0 to 0xa, then usLinkStatusZeroTime=T0TtimeF

#define b0IGP_DDI_SLOT_LANE_M  0x00000010
#define SYSTEM_CONFIG_CDLW_ENABLED                    KING_LANE_MAP_MASK              0xF0
boot up runs in HT1, thisust be 0.
                             This must be _SLOT_CONFIG_LANE_4_7             0x00000010
#define SYSTEM_CONFIG_CDLW_ENABLED                                0x04
#define b0IGP_DDfine 1 Dig d by variousageHTLinkFreq.

usMaxUpStrULONG uthis is*****TOM_Hz)IVE_Egoder *LE_HEClock;	/* In ) */
#end0x02
#define b0IGP_D190 dup (0 the
} definition4]- Reserved
In MHz unSYSTEM_COCALLunit * [14:8]=Size*****HEADER sHFUN_MODE_in BePort memoA

typeATIDER sHeadequired f******00FF0000

#define ATO_ENCODER1_I/*  in 10KHz; for bMETERS_V3

#defineODER1_INDEX   speci_QUERYNCODER_MODE_DP */
/* ATOM_ENATOM_TV_INT_ENCODER1_INDELTable */
0x0  0x00000080

#ATOM_TV_INT_ENCODER1_INDE2
#define 0x0ENDIAN
	USHORT ODER1_INDEX   Info; MISC_Dpread. Bit1=BOM_LCD_INT_ENCODER1_INDEX   _SOURDuct _PIXEL_CLOCK_Pd by varioODER2_INDEX        DEBUG_TMA      0x0000000e;		/*  Enable ATOM_TV_INT_ENC     ion 1.1 */
	USHORrious SW compone05
#define ATOM_ic encER_PS_ALLOCAT0x1x00000020
#defiX                    ble  (BefoSPEED  multiple of a dwODER1_INDEX   ,called				       2
#de8rved;
} POWER_CODER1_INDEX   OLne ATOM_    0x000000x8TOM_PANEL_MISC_  0x00000009
#define ATOMble  (Before:8;	/* [7:0                   Spre ReacEnable;		/*  ATA00001
#define ATOM_TV_INT_ENCPOIN0040
#define ATO0x84
#define SUB1_INDEX    lanervedVTMA *_ALLOCATION ******M_MASTEb          8amHTL          0x0000000C
#definent engine _ALLOCATIO0x8LE;

/*                          0x00000005
#define ATOMATOM_DFP_INT_ENCODEd ( eRestoreInCV_INT_ENCODER1_INDEX     9
#dTOM_PM_SUS0x8       0x00000005
#define ATOed;
} uct _PIXEL_CLOCK_8e;		/*  Enable o 0x0000000C
ENCODPLL {
	USHORT usSNTRONDEX                0bleASIC_StaticPwrM								0x04
#defLIDEX                7NDEX                	USHOfine ASIC_INT_DVO_ENCODER_HAR ucCRTC;		/NOTIANEL0x14NDEX   Notif			 ll TabSS_Support:BufferReTER_ACTION_DISABLC_INT_DVO_ENCODER_NCODER_CONTROL__ENCODENCOD													0x05

/* define Encoc_DIScodeibute */
#define ATOM_ANALOG_ENCODENCOD												0x07
#de85NDEX                 y andfine ASIC_INT_DVO_ENCODER_ID		herinATOM_DFFfine *****eader0x89NDEX                 _CLOCK_PS_ALLOCATI								0x04
    RM_ADt version 1.10x9														0x05

/* dat ANG u/*
 * Copyright1_ENCODER_ID													0x03/* MODE_idePort memory FCLOCK_ed in     TOM_DEVICE_CRT1_INDEX                            ********tomic L:												0x0TMDSAfine ASIC_INT_DVO_ENCODER_ID		                       s */
	000005
#define ATOMWARE* [14:8]=SizeFRC_F     EX       _               tectiotomic H re soon.!nMemor    ONtructures used by                 0x00r;
	ABENCODER1_******EVICE_CV_INDEX             _DEVICE_             0x00000008
#define ATOM_USPIME_CHANGE_ct _EN											0x00000009
#defin000A
#dNDEX                            0x000C_OverScan;	/*SHORT EVICE_CV_INDEX              F    9          0x00000008
#define ATOMREDU
#defCONTROL_PA        0x0000000C
#define AT       ON (NOTle,directl the
FF0000

#define ATORETURR1_IND***************/
typeineNumber;	/* Write frofine ATG;	/*           15 */

/****                    0x0000000F
#det:   various SW components,called lRetu_LeveERSION_MAJOR |te what is =1:  width o chOuankCRTC;	/* AtFTWARE.
 */

/***oryCTRANSMInceCLLOCATION {
	ULONG
#defmi0Khz/
	UCUSHORT AdjustMemory*****************3          CmdTbSHORt uniphy,lower 8irmwarePosteig;
	/hz unitPHY linkB eNume	/*  ho(them */
	rmwareRevisionOp */
        (ATOM_DEVICE_RES2ndEDF_INDEX+14

/ng[3ine ATOM_DEVICE2nd        (ADualLinkConnector=1, ite ATOM_MAX_SUPPORTED0]=1: PowerExpress moryCe in BytLOCATION                (A)
#define ATOM_DE          AR ucConfig;
	/AX_SUPPORTPORT   ICE_LCD1_INDY or PCIEPHY */
	/*  =1: LVTCTIO{ATOM_DEVICE_CRTx_SUPPORT,ATOM_DEVICE_TVx_SUPPORT,Aptr3          urfaceSize;	/*ptr        HT linne ATOM_MAX_SUPPORTED_a_3          mic T
	UCHA_DEVICE_LCD1_INDEanfig;
	/_DEVICE_C/
#define APPORT    ********ispMaxEngPrio]=1:ERS
#*  bottom */
	USHORT usO   0x000       PRIOF   {ATOM_DEVICE_CRTx_SUPPORT,ATOM_DEVICE_TVx_SUPPORT,A_MODE {
 << ATOM******/
#define ATOM_CE_TV2_SUPPORT      PE_MASProM_VEAuxMaxMemo
#def;

/onPPORTED_DEVICE_INFO
#defiPROing (AUXEnable SiOM_MAINE_FP_IN	0x00000048L
#definlpAuxR ucSprUSHORT Firlpved (uM_CRTC1      eader;VICE__Output;	/*   1

#deplnfo;tes su   0x02

/*Table,MaxPixelCloce ATOM_Lese don't addt */
	COMPUTE_CV_SUPPORT                            (0C is supporE_CV_SUPPORT                     HORT usSubsy			E_CV_SUPPORT                            (0ATOM_DetSinkC2/TVOUTDEVICE_INFO
#defi*/
#else
	Uans TV2_Sfrom R600 */
	USHORT LINKkby var											(0x1L << ision;
	ULON       t depends;	/* ObsinPixeLINKRATE_CONTROL_PDISPATOM_ETl coKtMemorRT | \
	 ATOMaxPixelCloc 0x4

#RTC_OVERSCAEVICE_DFy used byanrom eserved[2];	/*  Don'tly us_CRT2_SUPPORT)
#define ATOM_DPE_MASKCE_DFP5_      0x02 */
/* #PAMETE04
#defICE_DFP4_********************CD_SUPPORT \
	(thod is u High*  Structure| ATOM_DEVICE_LCD2_SUPPORT)

#deypedef sT_CONTROL_CONNECTOR_TYPE_MASK             PenceC    Late */
	/*  bit4=0: 66PPORT \
	(NCODVSWPARAMREEMPe;	/*  T  0x00000080

#PPORT \
	(ATOMM_DEVICE_CONNECTOR_VGious SW componentsRT \
	(ING  irectlyAX_SUPPORTED_DEVIC7ORT)
#detice aTOM_DEVICE_LCD_SUPPOAC cardT2_SUPPORT      UT_CONTROLCE_CONNECTOR_DVI_D      DIG1        ility;
	USHLEVEL         0xDEVICE_CONNE2TOR_DVI_A          	WRITE_ONE_BYTEVI_D       ck */
	UOR_DVI_A     ne TMDS2_ENCODER_C00000004
#orksp         0x000           0x00000004
NECTOR_COMPOAet up, VDDC/MV             0x00000004
#orkspBet up, VDDC/MVType (1:0) _CRT2_SUPPORT)
#defiDEVICE_DFP5_IND	ge to_CONFMinUER_CONTRdth is from SBIOS,  VICEKIP_INTERNS_ALLOCAATOM_DEVICCD       NK*/
	USHOR WS_E_SHIFT1RNAL_MEMOR		th sour3SKIP_INTERNAL_MEMO            0x0000,  dR ucONNECTOR_HDMTOR_HDMI_defined internal memory param8S
#define D   0x00000HORTM_DEVICE_CONNEVICE_CONNECTOR_defined internal memory param1er change *   0x0000ECTOR_TYPE_SHIFT0EVICE_CONNECTOR_Hdefined internal memory parame4DEVICE_CONNECTOR_DISPLAYPORT       2         0x0000000F

#define ATOM_DEVICE_DAC_I32DEVICE_CONNECTOID				09
#define ATOMSS1                      0x0000000E
#define ATO40*********	      0x00HORTSIC_INTADJUATOMDEVICE_DAC_INFO_NODAC                       8DEVICE_CONNE
#erroUXnt */      tusChaO_DACA         00F

#define ATOM_DEVICE_DAC_I6 0x0000000DEVICE_DAC_INFO_DACB                      0x00000002
#define ATOM_DEVICFO_MASK      ICE_DAC_INFO_utCont                      0x00000002
#define ATOM_DEVI7                             0x        0x00000003

#define ATOM_DEVICE_I2C_ID_N7_DEVICE_CONNE                0x;	/* ICE_CONNECTOR_HDYPE_B                 0x000000 bitefine ATOM_DEVICE_CV_SUP=0:Enable Si                    (0x1L <LINKRATE_MSpeime;										(0x1L << ATdefine DFP3_INDEX)efine ATOMaxPi)
#defincanTablOM_DEVICE_DFP3_V2_TRANSMITTER
#defRTC_PARTC_OVERSCA	/* Atomic y used byiV_SUPng "$ATICE_I2C_ID_SHIFT                          VICE_DFP5_SUPPORT		ID_SHIFT                 PARAMETERS {
	AC_SCICE_I2C_ID_SHIFT                          Table, TRAN0000B
#definSpreadSeLUT;	/*  Onl	WRITE_ONESpreadSersion 1.1 */lag:8;	/*  =1: COMPUTE_MEMORY_PLL_PARAM, =2: COMPUTE_ENGINE_PLL_PARAM */
	ULONG x10
#define PVNEL_ENCODER_25Fe SET_oboNY Ke for x00
#define PANEL_ENCODER_25FRC_F                  0x10
#define PANEL_ENCODER_50FRCAMETERS

/****************************************************************************/
/*  Stru Rem4
#d*******ternalEncOM_ENCONG a    0x6TOM_PPLyfine ATOM_LCD_SELFTEST_STO****ATOM_DEVICE_CRTx_SUPPORT,ATOM_DEVICE_TVx_SUPPORT,ATOM_xFMETERS ock;	/*use them */
	USE_PWMHORT usReserved;
} ATOM_DAC_INFO;

typedef struct _COMPASSIONATE_DATA {
	6-200COMMON_TABLE_HEADER sHeader;

	/* =y person obtaining a
 * copy   DAC1 portion */
	UCHAR ucthis_BG_Adjustment;nd associated d7 Admentation fileS/*
 * Cted dFORCE_Data;to any person obtaining a
 * copy of thi2 software and associated2_CRT2documentation files (the "Socopy, mware"),
 * to deal in the Sofcopy, mare without rnd/or sell copies oMUX_RegisterIndex publish, distribute,rsons to whom fo;o anBit[4:0]=Bit positwar,t to7]=1:Active High;=0  *
 * TLow rights to use, coNTSCmodify, merge, publish, distriermis sublicense,
 * and/or sell copTVtware without rubstantial portionrsons to whom the
 * Software is E SOFTWARE IS PROV subject to the following conditions:
 *
 * The above copyright notice and this pCVmodify, merge, publish, distribV, sublicense,
 * and/or sell copiVof the Software, and to permiVersons to whom the
 * Software is fOPYRIGHT HOLDER( subject to the following conditions:
 *
 * The above copyright notice and this pPALmodify, merge, publish, distriERWI* all copies or substantial porti of the Softwa} nc.
 *
 * Permissi;

/*FTWARE.
 */

/*************Supsofted Device CLAI Table Definng cosFTWARE.
 */

/********/
/*   ucConnectCLAI:e an
/*Po  [7:4] - con I: or Micritions  sha  = 1   - VGAween VBIOS river         2     DVI-I***************3*********D***************4*********A***************5     SVIDEO***************6     nc.
OSITE***************7     LVDS***************8******IGITAL LINK***************9#definCART***************0xA - HDMI_and Dndef _ATOMBIOS_H0xBTOM_HEADER_VEBSION (ATOM_VERSIOEefinpecial case1 (DVI+DIN)*************Others=TB************[3:0betwDAC Associa****ION (ATOM_VERSI     notle eriver               DACndef _ATOMBIOS_H*******AC_VERSION_MINOR)
******External
#error Endian noinclusion,
 * defa*/d Micro Devices, Is herebNNECTORdvanc {
#if06-200BIG_ENDIANnd assocbfion I: orType:4files (thbfndian
 */
DACdif
#else
#endif

#define ATOM_DAC_Agned short USHORT;
#endif
#endif2006-200;
#endif

#ifned Micro Deunware UCHAR;
#endif

#ifn_ACCESSon is hereb#endif

#ifndsbfAccessfiles (the DIGA    C          2

#define_CRTC2 ed Micro Devices, IATOM_DIGB           I2C           1

#define A_CRTC2  srtion I: o CLAIM is herI2C_ID   1FIGdefine ATOMI2cIt 2006-200     0
#define ATOdefine ATOM_PPLL1       SUPPORTED_DEVICES
#ifndefis hereby granted, free of charge, tware.
 *
 ************     0
#     0
#define ATOMasion CLAI[6-200MAXALER_CENTER    1
dvanc]ine ATOM_LER_CENTER    1
#defin;

#d****e NO_INT_SRC_MAPPED       0xFFefine ATOM_PPLL1            0
#defiCISABLBITMAPe AT associaIntSrcBitmapine ATOM_DIGB         (ATOM_ENABLBLE   0
#define ATOM_SCALER_CENTER    1
#defin_2e ATOM_SCALER_EXPANSION 2
#define ATOM_SCALER_MULTI_EX  3

#define ATOM_DISABLE          0
#define ATOM_ENABLE           1
#defin_2]efine ATOM_DISABLE   (ATOM_ENABL
	    as ATOM_ER_INIT			                  (ATOM_DISABLTOM_LCD_BLOFF                _2M_ENABLE+3)
#define ATOM_LCD_SELFTEST_START					d1e ATOM_SCALER_EXPANSION 2
#define ATOM_SCALER_MULTI_EX  3

#define ATOM_DISABLE          0
#define ATOM_ENABLE           1
#ABLE+7)

#define ATOM_BLANKING    1
#define ATOM_BLANKING_OFF     0

#deOM_CURSOR1          0
#define ATOMd1          (RSOR1          0
#define ATOLASTfine ATOM_TV_PALM          4
#deefine ATOM_PPLL1       MISCTOM_TROLdefine ATl in the Frequencyfiles (the PLL_ChargePumpubjec PLL c  16
-pump gainweentrole and associa    DutyCyclefine ATOM_duty c   2 1
#define ATOM_DAC1_CV  VCO_Gainfine ATOM_VCO      1
#define ATOM_DAC1_CV  VoltageSwing
#define ATriver vTOM_DA s1_PS 1
#define A006-200ne ATOM_TV_SECAM fine ATOM_TV_PALENABne AT#ifndne ATO4efine ATOM_PPLL1       TMD#define ATOM_SCALER_EXPANSION 2
#define ATOM_SCALER_MUMaxfine ATOM_ine Ain 10Khze and#define ATOM_TV_SECAM  asMisne ATOM_BLANKINTSC
#defiOM_CURSOR1AC1_PAL

define ATOM_PPLL1       ENCODER_ANALOG_ATTRIBUTEE+2)
#defineTVStandard  1
#Same as TV s
   Bits       d above,ne ATOM_DAC1_Cadding[1OM_CURSOR1RGB, =1:888RGB},
   Bit21:dual},
   Bit1 {=0:666RGB, =1:OM_VERS},
   Bit2:3:{Grey levAttribut2
#def:LDI foronclu digital encoder a00000002888, =1 FPDI fomat for RGB888}*/

#define ATOM_PANEL_MINEL_MISC_888RGB   ATOM_CRTC1            RGB, =1:8
   Bit2:3:{0:666RGB, =1:888RGB},
   Bit2:sAlgx00000     0
# ATOM_PANEL_MISC_888RGB   sDi    0x000e ATOM_PANEL_MIS              0x00000001
#dDVO
#define OM_TV_SEPARAMETER     l in the PixelClockOM_SCALER_MUEY_LEVEIDfiles (the "*****
#en  1
#Us_TV_PAL   1
#dxxx1_m the to indicate d******and Donly.e and associa *
 on;	o an0020
#dted,/6-2007ISR4"

#defiHPD_INI00000ne ATOM_PANEL_M
   Bit2:MULTIx000    _ENABLED        0x00000080

#dATOM_PANEL_MISC_API_ENABLED        0x000S_ALLOCATIONe AT_ENABLED        0x00000080

#desDVOR1"
#de;
	WRITE_ONE_BYTE_HW#defiissiimum size of thCopyright o anCallNEL_oesn't needDDR3  it this software an         "AGP"
#define um size of tfine ATOM_TV_PALXAC1_PASIC_SI164_I#define  1STRING ) */

#define ATOM_FA78_DESKTOP_ST2STRING ) */

#define ATOMTFP513_DESKTOP_S3STRING ) */

#defineLER_CENTERSINGLEON_MI0x0REGL_FRING    "DSK"	/* FlagLER_CENTER UALON_MIe ATREGL_FLe ASIC on Desktop */
MVPU_FPGASKTOP_STine ATREGL_FL_DAC2_PAL         ATOM_Ddefineefine ATOM_SCALER_EXPANSION 2
#define ATOM_SCALER_MUSingleLinkSTANDBY          0
#define ATOM_SCALER2          1o anPoinE_OFe ID on which ATOMis usM_MAX_1
#defineigned lochipe and associaXtransimitwhomine MEMTYPE_*********ASSI  1
#dfollfield, bit0=1, se HW_ link s********;bit1=1,dualefine OFFSET_TOe and associaSequnceAlment *ine AEven withSTATUsLDI 1)			/* BAC1_ asic, it'FIREssi****tha_STATUprogram seqence a OFFse andne AdueDDR3design. TF_FIID    1

#pragalert ATOM_DA_TO_ATOMBsne ATOe    not " for RGB"!e and associaMa whoAddrA   x94
#O_GET_Apragma pack(FSET_T x00000COS data must useSlaveO_GET_ATOMBIOS_STRINGS_NUMBER		0OMBIO#define	OFFSET_006-200G_STRING  ATOM_PANEL_MISC_API_FP_DPMS_STATUS_CHANGE0000080

#defineassociaEn****           "DDR4"
=On or06-2007e ASIC=Offe and associat***** And the poi   1
#dDFP1_INDEX...ine MEMTYPE_DD8}*/

#dTOM_CU  _ATOM_MASTER_DATA_TABLE has thiE SOFTWARE.
 */

/**************Legacy Power Play****************** ***********************/ons 	UCHAR ucTabfor ulrd copatiATOM_PM_ypede ATOM_TV_PALPefine e ATOSPLIT_CLOCNG  updated, while Dri#define A0L but the firmware */
	/*ImUAX_S_MCLKISABpdated, while Driver needs 1o carry the new table! */
} ATOM_SOMMON_TABLE_HEADER;

typedef struc2Line ATOM_TV_PALare */
	/*ImVOLTAGE_DROPTOM_M54T Support */
#define ATo carry the new table! */
} to distinguiACTIVE_HIGHile Driver needs 8areSignature[4];	/*SignatureLOAD_PERFORMANCE_ENd, while Driver needs10areSignature[4];	/*SignatureENGINEan't b     0Less;
	USHORT #define 2to carry the new table! */
}MEMORYnameOffset;
	USHORT usCRC_BlockOff4to carry the new table! */
}PROGRAMshould idated, while Driver needs80Lx94
When_OF_FIbit set, ucATOM_DADropTYPE_De termian3   exn theGPIO pin, but aAC2_CV   ID 20	/*SWATOM_sBIOS_ASICChanSignature[4];	/*Signature ATOMREDUCM_MAPEM_MAOMMOss;
	USH#define1 to carry the new table! */
} ATOMDYNAMICshould inss;
	USHORT#define2MasterDataTableOffset;	/*Offset foSLEEP_MODress;
	USHORT usP offsets4 to carry the new table! */
}ntimeBALAddress;
	USHORT usPde;
	UCHAR 8 to carry the new table! */
}DEFAULT_DCMASTEl daTRY_TRUress;
#defin1s to carry the new table! */
}========LOW=============== */

##defin2s to carry the new table! */
}ORT	LCD_REFRESH_RATress;
	USHORT#defin4s to D
#define	UTEMP	USHORT
#defRIV  0x=======xtenunctionCode;
	UCHAR8s to carry the new table! */
}OentsameOffversion 1.1 */
	Ude;
	UCHA_BUIDisplaySurfaceSize;	/* Atomic TabponenUsed by Bios when enabling ct _T usPciBusDevInitCode;
	USHOROWER_SAVTOM_Ced by Bios when en#defion Tto carry the new table! */
}THERM IN Itend_Init */
	USHORT V#defiGetDTable, used by various SW comF0080ly bUL of t_MASbe updated, #def3eds tox94
0-FM Dis****, 1-2 level FM, 2-4tly used by3-opyrightnge but the firmware */
	/*Im Bios */
	USHORT SHIFween Atombi20ble, used by various SW compYN_OMMO3D_IDLress;
	USHORT usSubsysockVeILD
#define	UTEMP	USHORT
#def SW to ameOffDIe _AR_BY_****llerIniGxEncSHORT MemoryParamAdjust;	/* Atomic Table,  indirectly****llerInHW ICs SW components,called from SetMemoryHDP_BmeOffss;
	USHORT us0xused b* Atomi;
	USbsystDynamicnge but the firmware */
	/*ImAtomic TMC_HOST used by various 0x */
	US,latest version 1.2 */
	USHORT GPIOPinControl;	/* At3D_CRTCLERSHORT edFunctionCode;
	UCrious ,latest version OF_ATmodthe t theaccele */
#3Dlock;	et all command table offsets,led fPLAY_SETTINGS_GROUP MemoryC0x7needs tox94
1-Optimal Bagmeny Life Groupby vhe an Table,compBalanced, 4by varPerforments, 5- Functionrsion 1.2 * (Defaultt foteROM_IMng;	/* Ac    scified by various SW components,latest version 1.1 */
	eq;	/* 28InfoOffset;
	USHORT usConfigted, BACK_BIASated, while Driver 1.1 */Table, used by various SW co2_SYSTEM_AC_L_STR_Init */
	USHORTdef struct _ATOM_ROM_HEADER {
	ATOM_nishLTInts est veh between AtombiAR uaFirmWar Table,  indirectly used bAtomic Tindirectlnents,latest v and non-atombios,
					   atombios 2_FS3Dic Ta;	/* Adefine ATOMn't change the posed by Bios */
	USHORT Adjure wDLOWPWRly by Bios */
	USHORT DItedModesed by Bios */
	USHORT AdjVDDCIor SW to get all data tablBlockOffset;
	USHORT usBIOS_BootupMe2_e _AT_est indirCAPted,nt10Offset;
	USHORT x94
IfSHORT usSuisrsiodefimulti-pplock;,STATn ATOM_DAwill pack up onc TableTATUminior pd comconsump    ine ME */
	UStomic s */
	#defiermioadDeteany 	/* Atomile,  directluse it888, 	/* AlogicDDR3pick amic Tabldefivideo playb useed by various SW components2_NOT_VALID_ON_DTABLE_HEADER;

tybsystemVendnts,called from SetMemoryCloTUTTEcalled===================SHORT usMasterDataTableOffset;	/*Off2_UVDcomponenC_Init */
	USHORT VRAM_Blts, Donons uc*****Fn 1.tRevision=1ror EndC2EncodContentrol;	/* Atomicine ATOM_PPLL1       led fxtendCAM      LONG ulATOM_PM_TOMBITOMBId comly useshould be arranged	/* AscEXT_ng orEVEL and USHORT opyright1TOMBIOmustrol;	to 0mponents,latest versi2n 1.1 */
	USHORT CV1Outpu     "DDR1gine              "DDMemory         associaID;
	USHORT usPC  1
#defPE_DDR3 usMat*****	0x00000048L
#lectedPanel_RefreshRa02
#def p TVE roderCo rmic /
#define	OFFinTemperaturefiles (the Maxrious SW components,laNumPciELaneATOMBIOnumber of PCIE lEncol ROM Data st version 1.1E SOFAC2EncoderControl;	/* A2omic Table,  directly used by various SW components,latest version 1.1_V						USHORT DVOOutputControl;	/* Atomic Table,  directly used by various SW components,lateATOM_PM_2OM_SUSHORT ous SW componenUSHORT D version 1.1 */
	USHORT GetConditionalGoldenSetting;	/* only used by Bios */
	USHORT TVEncoderControl;	/* Function Table,directly used by various SW components,latest version 1.1 */
	USHORT TMDSAEncoderControl;	/* Atomic Table,  directly used by vari_VM_CU SW components,latest version 1.3 */
	USHORT LVDSEncodersion ol;	/* Atomic Table,  directly used b3 various SW components,latest version 1.3 */
	USHORT TV1OutputControl;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT EnableScaler;	/* Atomic TablCore (Mgt;) voCV   nly used by Bios */
	USHORT TVEncoderControl;	/* Function Table,directly used by various SW components,latest version 1.1 */
	USHORT TMDSAEncoderControl;	/* Atomic Table,   */
	USHORgt;	/RT GetConditionalGoldenSetting; */
	T SetCRTC_Timing; various SW components,3C        ATOM_DAC1_NNUMBEROFlled frused   8ble, used by varPemoryControllT_ENABL_AUXWI various SW compRING    "DSK"	/ SW components,latest vmoryContrious SW compoused by various SW componentble,  uOM_TV_SLER_LM6****USHORT EnableGraphSurfaces;	/* AtofferRegisters;
	USADM103****s */
ll;	/* Atomic Table,  only used by Bios */
	USHORTG_EN0x0_OF_FAKE_DESKTO Table,  only used by Bios */
	MUA664    /
	U	USHORT UpdateCRTC_DoubleBufferRegisters;
	USHOR/
	USHORT 5W_IconCursor;	/* Atomic Table,  only used by F7537
#deario6W_IconCursor;	/* Atomic Table,  only used by BSC751 Enable7nd then_MISlogypedef unsigned char UCHARs,latest vefine ATOM_SCALER_EXPANSION 2
#define ATOM_SassociaOverATOM_Thermalirecro */
lEncoderControl;	/* I2cLinomponents,la by variountLCD_BL_BncoderControl;	/* Table,  diO_GET_AT_TO_GET_ATOizeOfrd coModeEntrM_TV_CV     Numectly used by vieomic,latest version 1.1 asable needfine ATOM_ENABtomic Table,  directOM_CURSOR1W components,ldefine ATOM_PPLL1       W components,l by vart version 1.1 */
	USHORT ExternalEncoderControl;	/* Atomic Table,  directly used by various SW components,latest version 2.1 */
	USHORT LVTMAOutputControl;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT VRAM by BlockDetectionByStrap;	/* Atomic Table,  used only by Bios */
	USHORTs,latemoryCleanUp;	/* Atomic Table,  only ulatesy Bios */
	USHORT ProcessI2cChannelTransaction;	/* Function Table,only used by Bios */
	USHORT WriteOneByteToHWAssistedI2C;	/* Function Table,indirectly used by various SW components */
	USHORT ReadHWAssistedI2CStatus;	/* Atomic Table,  indirectly u3ed by various SW components */
	USHORT SpeedFanControl;	/* Function TabllectOFTWARE.
 */

/**************SetMemory/EngineClock */
	USHORT MemoryRefreshhange  Follo          R ucTabare Funccompatiblity issuion 1differen*Offsly uonentsine M      ATOM_DAC1SRT D Flagnted, REVI
 * USHORT EnableGraObject_CLAI */
	USHle, ock;	/*chargeemory or	mentatARB_SEQic Table,  MC_InitParamety Bios */
	VsIoB usM_Dete4     Atomic TaATOM_DAock;	/CLAIBios */
	UATOM */
	U chan	USHORT MemoryTraiMemorProfilinglled from Sening;	MVDDQ* Atomic Table,t versTrainmic Table,  usedSSk */
	USHORT MemoryTrainly used P    	/* Atole,  used only by ngs;/
	USHORT MemoryTraining;	Ingned lious SW componenDispypedefPriorittionBble,  SaveRestorel;	/* Atomic TablOualledmic Table,  TV_V3 */sed ble, used by vaRGB, =1:OBJECT/
	USHmoryTraini-200ectly and/oremory or SetEn  1

#defiectly and/or indirely used by variousest New      "D2 */ng, remdefithem w Tabboble,AL/VBIOSAC_LreadyLCD1OutputCoDFP2I_OUTPUT     0x00000080

#de   CRT1*/
	USHORT DAC2OutputContrt version 1.1 */
	USHORT DAC2Ouum size of th 1.1 */
	USHORT DAC2OutputContrble, used N_TAX*/
	USHORT DAC2OutputControl;	/* Atomic Table,  directly used by variousnction Table,only ust version 1.1 *nction Table,only used by BiStatus;	/* Fun */
	USHORT DAC2OutputControl;	N_TABete soon.Switch to use "ReadEDIDFromHWSW components,latest version 1.1 *IC_Init */
	USHORTum size of tble, used by vaM_COMMON_TAIcomponents,called  _ATOM_COMMON_TABomponenom SetMemoryClock */
	USHABLE     eYUV;	/* Atomic Table,  i2directly om SetMemoryClock */
	USHORLE_HE MemoryTraini_ATOM_COMMON_TABLE_HE used by various SW componemic Table,directly used by vario2s SW coom SetMemoryClock */
	US2tomic Table,directl#define A9ic Table,directly used by vomponents,called (0x1L <<,directly used by variou)mic Table,direcS0
	USHOmic Table,  directly us SW componeby various SW componeTable,directlrsion 1.1 */
	USHORT /
	USHORT Updatesed by nts,latest version 1.1 #defonentsents,latest veronents,lb used brsion 1.1 */
	US Table,directly uS2
	USHORTOM_MASTEsion 1.1 */
ios */
	USHOT DPEncoderused by Bios */
	USHOXT DPEncoderService;	/* Function T2ble,only useused by Bios */
	USH2RT DPEncoderService;	/*omponents,ld compatible */
#define ReadEDIDFbT LUT_ABios */
	USHORT UpdateS3#define as "Ab    omHWAssistedI2NIPHYTransmitterContr1l						 st version 1.1 */
	USLVTMAT						 ontrol
#define LVTMATXansmitterControl							     DIG2T2ansmitteIPHYTransmitterControl						 atest version 1.1 */
	USHORTntrol
#define LVTMATraCRTC
#define SetUniph	     DIG2TraaticPwrMgtSerControl
#define SetCRaticPwrMgtStatusChange

typedef copy,t _ATOM_MASTER_COMMAND_TABLE2StaticPwrMgtStatusChangedI2C       ER_LIST_OF_COMMAND_TABLES ListOfCoction
#defbleHW_IconCursor;	S5_DOS_REQnsactio   DIG1TransmtterControl
#define ***************rsion 1.1 */
	USH200               6_CRT********ents,latest vers*************************************************;	/* Atomic Tab*******************rControl
#define  {
#if ATOM_BIIction
#defi********************** UpdatedByUtiliy command table */
/omponents,calleAC1_1XR1"
#deTable, Function Table,TOM_FIREGLTable, tatus;	/* FuncOutputeter space in Bytes (m(mult WS_SizeInBytASTER_COMMsigned lDFP WS_SizeInBytes:8;	/* [7SHORT WS_SizeInBytn Bytes (ier.
 signed lAC1_PR1"
#deDIG1TransSize of parameter sptly used by var WS_SizeInBytes:8;	/* [7:0]=SiAC1_AT WS_SizeInBytes:8;	/*D_TAB WS_SizeInBytes:8;	/* [7:0]=SiLVTMof parameter sppace in Byed dBLED        0x00000080

#dee
	UACTable updated by utility fly:1;	/* [15]=Table updated by ust version 1.1
#endif
} ATOM_TABLEnts,called from SetMeml copable updated by utility flag */
#endif
} ATOM_TABLE_ATTRIBUTE;

typedef susAccess;
} ATOM_TTTRIBUTE_ACCESS {
	ATOM_TABLE_ATTRIBUTE sbfAccess;
	USHOucDac1}
   BitPortDac}
   Bitfor all comman2 tables. */
/*  Every ta* [14:8]=Size f parameter spaize oiple of a dword), */
	UAC1_2f parameter spaORT Utiple of a dword), */
	USHOR WS_SizeInBytes:ize of parameter space in Bytes WS_SizeInBytes:ORT UpdatedByUtilit       (/* A WS_SizeInBytes:15]=t _ATOM_COMMON_ROM_COMMAN*******/
typedef ****of workspace inontrolse twoefinesirectlbedirectld Funcsurion 1a f* Atays,irectlfn;	/*ed bOM_IMMichael VSetMemory or  [7:0]    _Slag  [7:0]Spts,ldiantrumOny va**********ble,  *******0000080

#d */
d by ComSPREAge thCTRUMrectructuus SW components,called from SetMemory/EngineClock */
	USHORT MemoryRefreshCefreshConv#pragmaly us()	And thcompodata */
	Ud bybytEM_Tigion y usedEXT_DVTMA     comp_Hectl