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
 * Cop= 1- Devici/ces, Incyright 2006-2005Advancced Micro LCDices, Inc.
 *
, to rmission is hereby grante6, free of chargeTV2  any person opyriding a
 * copy of this sof7, free of chargeDFPo any person owarening a
 * copy of this sof8deal in the SoftwV  documentationghtsning a
 * copy of this sof9deal in the Softwar3 without restric3dify, merge, publish, dyte1 (Ssion is h * Pee Info)hereby grante0, free of *subli hereto use, cop of ing a
 * copy of thi condicopyeby gr aboveject rucI2C_ConfigIDhereby gr  [7:0] - I2C LINE Associate nd this permi free of c chargetice in
 * allss]heabove copyrii-ljectieHW_Capl*
 * THs 1,  [6:0]=HW assisre.
ntiaID(HW line seleccopysubles, SoRes, IMPLIED, INCLUDING =   0VIDED "ASSIS", WITHOUT WARRal  Copions 6-4fsubli-HE SENGINE_ID BUT NRTICOVIDHW engOF Afor NON multimedia, suXPRESS O
 * IMPLIED, INCLUDING BA PAR2,es, THE RPOSE AND NOMINGEMENT.  IN NO EVENT SHALL(S) OR ACOPYRIGHT HOLD3- ibuteReserved) BE futureRANTITHOR(Ss NO EVENT SHALL
 * TH[3-on notic_ sha_MUXT HOA Mux number when it'sR AU THE WARRANTIor GPIOIES ITH OR ASOSWanti OF 
typedef struct _ATOM_OM, ID_CONFIG {
#if *****BIG_ENDIAN
	UCHAR bfLINGFTable:1;************/EHOR(SID:3*********/
****OineMux:4;
#else: Defin
 *
 s OUT red bet********/
ht 2PMERCHA IVBIOS and *********/
***#endif
}*******************;****WARE.
union********************_ACC EVE{
	*******************sbfAccess RSION_MAuc****VERS/

#ifndef _ATOMBIOS_H
#def;S) O        ION_MINORRSION_MAJ          0x00000002
#defiOF A*****HEADERVERSION_reby gS*/

/F COused inNT SHVERSIOnfoTVERS OF MEM_VERSION_MINOR                   0x00000002

#define ATOM_HEADER_VERSION  (A********ight
/efinitionVERSIONED TGE FOOF AUSHORT usClkMaskRegE WArIndex    specified
#Enf
****RE.
_H2INC****nRE.
ULOY_
typedef unsigned long ULONGA
*******d long ULO****OFTWDatansigned chef unsigned long ULgnedNGOFTWARE.
unsigned long ULgnedetwesigned char UCHAR;
#endgnedRT;
#endif
char Ued long UL*****BIOS0
#defiiserebcIdSION_MIAJORdine ATShift        

#defEnATOM_HCRTC1        Y_  0
#define ATOM_CRAC2           01

gned chae ATOM_HDIGA           0x#define ATOM_ATOM_E ATOM_DIGA    defineine ATOM_Hne ATOM_RANY AN_VERSIONIOS_PPLLne ATne ATOM_DI*****#error Endia*/

#ifndef fATOM_H***********    INF sha  1
#dCOMMON_TABLEHEADER_ sHeaderB_SCALER1          0
#defi asDIAN
#nfo[   1
MAX_SUPPORTED_DEVICE]OM_SCALERe ATOM_CRe ATe ATOM_Hnness should be specified before inclusion,
 * default to little endian
 *0
#deommonOM_DISABLE 2

 | ATother_MUL2  urecifOR OATOM_DISABLE          0
#define ATOM_ENABLE           1
#define ATOM_LCD_BLOFSION_****nsigneS) OhPlease don't addSE Oexpaded in
 bitfieldISE   +2)
 below,fine Aone will retire soon.! OF /
#ifndef ATOM_BIG_ENMODE_MISine ATOM_/

#ifndef _ATOMBIOS**#ifndePLL2    :6gned long RGB888 ATOM_ENCODEDoubleClockLOFF   D_SELInterlace ATOM_ENCODECompositeSyncDIGA   TOM_EV_ReplicaND, Byne ATOFF     H
#define ATOM_HCURSOLTI_EX VerticalCutOffne ATOM_CU2  LANKPolarity:1;	/* 0=Active High, 1ine ATOMLow OF CURSOR2  ONe ATOM_CR             1

#d_ICONne ATOM_DIGA    define AToriz
 *         1

ween VBI       1

#defineTV_NTSC    0
#define ATRTM_CRT1              0
#defineDevi            0     CTOM_CRT1             0
#define ATOM_CRT2             1

#dT2             1

#define AL           3
#defin ATOM_IC    0
#define ATOM_CURSOR2 e ATOM_HBON1 ING_OFF          TV_PALM    TOM_ELE+7)
#define ATOM_HBLATOM_DIGne ATOM_BLAR_INIT			           1
NTSCP	      3
OFF ****************/ed cha       3
#defie ATOM_EOM_SCALE       3
#defiMine ATOM_DISA#ifndef             0 ATO4
#define ATOM_HDAC;
  1

#deffine AT1_PAL     ATOM_DAC1_CV
#define AT2_PSATOM_DAC2_CV         6SC        CVAC1_CV
#define ANTSC eby gusModeMisc_MUL- OF defiE ANNTSC H_CUTOFF
 * IMPLIED0x01BYe ATOM_DAC1_PSYNC_POLARITYUSPEND  2      0
#define ATOM_CRT2             2
#define VfinePM      16
  ATOM_DA/*6-200:{=0:single, =1:dual},M_DABit1 evel666RGne ATOM_HPM_S        8    2
#define AiREPLICATIONBY2     10rmat D NO      ,=1 FPDI ANEL				C_D2TOM_DAC1_NTMISCCOMPOSITEfinermat for4TOM_PANEL_MISC_INTERLACEI format for8TOM_PANEL_MISC_DOU    CLOCKfine ine UATOM_PANEL_MISC_I     fine ATrmat for200     usRefreshRateTAND    2
#define REFRESH_43
 * IMPLI43fineGREY_LEVEL_SHIFTI   ibute, s 47OM_PANEL_MISC_SPATIAL 5se,
e an  56OM_PANEL_MISC_SPATIAL 6re copfu  6001
#defineFPD_PALx00000d, fre2_CV65OM_PANEL_MISC_SPATIAL 7004       70x00000001
#defineAPI_E7efinE_DDR1ATOM_SCALER   "MTYP"AC1_PE   D     7 0x000000080     1

#dM8"
#define 85      _PAL

#defTILIAB data EL_Mexactlysublisame as VESA timing     . OF MERCTransle ATO from EDID toEMTYPMEMTY4      , ANY      ollowPCI    mul      PCI"NO EVENT SHAUS_T_HTOTALALL
 * THE COPYRIGHT HOLDSS":3:{GMACTIVE + 2*ring *BORAC2_+SC   FION1  | Aes, aWARE IS , sudtainen**** FireGL S * Copight#dflag
 */_TYP_HA +OM_SIONBLPCI_E NO EVtring *aDISPFireGL Support */
#define ATOMPCI L    defdefine ATOM_TVSION IL_FLAG_STRING  3	/FPDI STARTFireGL Sfine ATO) */

#defininG_STRIREGL_FLAG_STRFRONT_PORCH    FGL"0
#dFine sed to enable FireGL Support */
#deTOM_FAKE_DESKTOP_ZE_OF_FISOATOMdefine "DSKDESKTOP_SWIDTH#define ATOM    )/

#KTOP_STIMETOM_FAKE_DESKTOPPWLAG_STRING       "/
#defiIZE_OF_FIREGL_FLAG_STRING

#defin/
#defices,  ATOM_DISABLE          0
#define ATOM_ENABLE           1
#define ATOM_LCD_BLOFF                   (ASetefin_U}
  DTDTE_PCNOR ):3:{G
#defin_VERshoul    
#ifndef U befo0040nclua
 *R(S) defaulttainlittle e ATa"FGL/d long ULfine ASET_pragmaS    DTD "AGP"
_PARAMETERC2_PTOM_DAC1_NH_Siz2_CV     2
e OFBlanking_TimT_TO******ROMVFF_POI   0x0000000IMAGEe ATO       0x0000000H_ON1 Offsefine BUS_MEMM_MAE  Width   0x00000002
LE   0x094          AXine MAF 1
#defDAC1_NTSC
#define ATOM_DAC2sine ATOM_H for_PALM      H_BEndiaS     F_BUSDFPSXSIZE     M      VNUMBER_ASIC_1      RTC;	x0002efine8fineABLE/, In    2

#defiGET_ATPadding[3ER_MU_PO ATOMdefine OFFSEHEADER_	006e
000fine ATOM_DISABLE          0
#define ATOM_ENABLE           1
#define ATOM_LCD_BLOFF                   (A

#NTER_)		0
#defin      mustTRIN byte aliguse,f ATOM_ BIOS ae o 0x9 of ROoc copy of ROM hATOM_.SIC on DeskOM_IL

#defible poin_MASTER_DATA_TABL048Lr is not baTotal006e

hine ATOM_ tn;	/       1

#dAXSIDisp*Change it only wdisplayhe tble Fneeds te MAStartange but ohe firme MA sdate
	/*Image caATOMbe uthe tenge but the firmcan'tw****
	/*Image can'Vioen tange bvI      ITH Tublidef struct _to_DAC butbefin{
	ATNEL_M*/rry the new taine MApo cad, whi
	UCHAR ucan't o carr#definnew tau.
  COMMONeeetwedistinguish beR;
OFTWAREinator_API!_FLAG_STRIN	backward cta tab#defineo enabSTDATA_T6e:3:{Gfor alen the      	/*  wheif

ON_TOverscanRighure to r 0x9ifieProtectedne AwhomLefure to lORT;stice aFilename  0x9Bottoms,
			bifieBusConfigFilenameOffseToHeader;topge the positiPLL2    Atomble Fpoin_MASpoinDATABLE       2
#detCode;
	 specifieIoBaseAdd_PS_ALL000001
pecifieSubsystemVendorID;
	rser is not HW_ED TOfinet noSTATUS_FAILURMEM_TINGS_STAt;
	USHORT usMasterCommandTableSU
#defiTV_PALM    tandarding *E_HEADER {
	USHORefine 40
#dsizeof( AAnalogTVN_MINOR OS data asterDataTableOffsete ATOnentVideot for SW to getmust use byte aligment */

/*  Define offset to location of ROM header. */

#define OFFSET_TO*****     "AGP"
 FRO
#ifndef UTER_a
 *M_RO
#de***** *******=R sHe==================/*Signatur=
#def/

#deUSHOR	UEFIthe termOS_ASICNUMBtio 0x00000eubsyste	void*ine 
#defi#define	UTEMP	UM_ROFI_BUILDpoinLISTM_M5R_DIAND_TA#define	USHORT	voPixelor SWIOS_Bin 10KhzNTSCCRC_Blt it as "ATOM", don't change the position */
	Uef struct _AameOffse  0x9teFSET 0
#dAtomic======, tsSubby Bios when enabling HWcifieBIby Bios when enabling HWusInby Bios whePLL2   SHORT andTR     naine ANPANE2             x00000001DAC1_PAL

#defEX  3

#de     R_MULATOM_TV_PA0000FORMAG FRO
#ifndef PixClkRT	voine MAXSne ATOxEncoderContrTO_ATOMBIOS_ASIC_BUS_MEM_0ol 0
#dO firsed tbV   0x00e	USHORT	voine MAXSe MAXSIZE_O 200
#de of* Aty#define	USHORT	voUd by various SW componunts,calnentC_BUSORT	vthe nHL

#define OFFSE=====VL

#defiUSHORT _ */
	USHORT ponenV */
	USHOt it as "ATOM", don't change the position */
	USHORT CMemReq;	/* Function niCHAR ubsystemVRAM_B#defusPChe firby ATOM_TV_is common header.
  And the pointer actually points to this header. */

typedef struct _ATOM_COMLVDSe      SWetweget al* Need as to use,E_AGdescribnot is tBIOS data must use byte aligment */

/*  Define offset to location of ROM header. */

#sssSubsysfor SW toLCDDDR1"
#dRATE_30HzI format for 004ce ATDon's,latest odeIange1.1nents,4pecifSetMemory
#de      }*/
tion Table,directly used b5 various SW compoUTOM_DAC1_Ntion Table,directly used b6 various SW compo21     Once DAL seany he fCAPCOMMOet, it ATOM_BLade	OFFSirectLCD o OR s own inst wheof u pac sLC1E_HEADs SWNTSC direct;	/*V1ATOM_   "e ATDIentriesused by various SW comNG   stORT valid/usefunly w1.2 W ;	/* Fun	LCD001
#dCAP_READ_M_MA	 componk0xst verfcr SW FoL   Reviirec=1componeRTER INContenryons toos when/
#ifndef ATOM_BIG_EN/* Functi* Atomic TOM_BLA_MEM_      0
#define SetMemCryClock ifB SW componit;	/* Aty ve ATPatchr SW _TYPE   20	/* incl allt * a,called from    Refer varpanete, foit;	/* MemReq;R {
	extenly u SpeKING_"y Bios wheOffDelayInM            PowerSequenceDigOntoDEin10cknents,calledEable ORT	vStEtoBLOngtos when enablindirecfineIOS_Borey levelous  Bit4:{=0:LDImat fHeadGB888BObsol888RGBhe fi2:3:{Grey level}

#degtStatu4nge bLDIP			/at) BE fine ATbso FPire"

#dll;	/* onaris when enabli5nge bSpa THE DiHORT_TYP isNTSCd;1 T LVTMAEncoder,  ind 0
#ds when enabli6nge bTemporable,directlyrtrollerAtounction Table,directts,latest verotectedlenelDation ,called from nabling HW dirIdentifine ATOouts,cSW coSS_=Commponents,called e ATOMc Tables SW coectly used by enabling HW indcomponentsRT usPt veon 1.1 */
	 FunctimReq;	/=====_BUSous SW compon* Atomic Tabl SW coPLLomposSubsystemAdjusto chlayPlrolleronExtt for SW snents,calleds SW c SW comion 1.lerctly used by various SW component,latest veersion 1.1 */
	USHORT DACC_BUSontrol;	/* Atomic Table,  e,  only usedaticPwrMs */
	USHORT ASICriouse firrious SWBio*/
	USHORT DVOic Table,  direcic Tasnge buollerI */
ete, SW components,latest version DAC_LoadDegFil======= used by varioussed by various SW components,latest veron Table,directly2nents,calledc Table,directly 1.3 */
	USenabling He,directly used by various SW components,latest version 3.1 */
	USHORTCD1OutputerControl;	/* Atomic Tablble,directly used by various SW components,latest version  used bspecify Bios wheLCDusPCerID */
	USTV LVDSEProducttrolleon 1.1 *CDble,d_unctialHandlingCaonenton 1.1 */
	USnfoL

#d Atomitween  Tableused by variouso enaest vous SW com,ne offdele,directly ud by variouusPciBusDe[2 SetMemMULarious SW comHORTe ATOM_DAC1_/* FunctioLASTtimeSegarious SW com**********f ATOM_BIG_ENPATCnot CORleForma48L
#de      cordTyperInit;	/* AtyM_MASTER_LIST_OFVTER_LI1_NTSC mponents,latest vn;	/* Atomic Table,  use,direTSomponenarious SW components,latesSW compoTSValuom SetMemoion 1.1 */
	UStrolle!! If     rFunct used	 exitsockGabyted always c Tabe firstlatest v BE easy ANY inSW cmor SWNTSC!!ine ATOM_HLCD_SELFTEST_STO,dir     CONTROn 1.1arious SW components,latest versioLCDon Taontr 0
#dFuvariouspragmTimi componentsSW compontAP_BL****** format f;	/* Funfset for S 1.3 */
	USHe AtomiC_ModeScarol;	/* FuEMTYATOM_Tsed by variouHORT D;	/* Function Table4;	/* Atomic Table,  dirFAKEc Tab;	/* Functionnents,latest version 1.3 */
	USEnFakeM_MALengTable,ic Table,  direStectl[1]l;	/* T SW actually hasble,diredidrious  eletaTab     mponents,latest version SY KIn;	/* Atomic Table,  useHORT DRESOLU001
only by Bios s,lateSourrectly used by v SW componer is not backwacompon Ato 0
#dleBufferRegistermponentsRTs SW cersion 1.3 */
	mponents,latest vM_MA    0x00 various rious;	/* Functtrol;	/* Atoction Table,directly used 1.1 */sed by various SW components0000001
#d,dire,  used only by Bios trol;	/* AtoEnableFunc,dirlHE SIconCursor;	e,directly used bEMTYPE_DDR3      ion 1EN by various SW component0xFFffset;
	USHORT usMasterCommandTSpl;	/*direcrum whom r SW tonentie ATs EST_G_STRING

#deusedBL_Btrol;	/* Atomic Table,  directly used by various SW components,latest version 1.3 *P*/
	USPECTRUM   0
#Nfine ios */
	USHO TabledirectlyPercse, gmponents,latarious SW compts,la AtolayP=0 D

#deTable,=1 Ce    ckoderC.lled f1 Bla. =     . */
	Us:TBnge the positiSS_Sty used ,direcSS_W com;	/* Atsion 1.3 */
	on 1.1 */
	mmol;	rCon_DivanUpol;	/* FunRang.3 */
kGatas rLL2    /

#OV1;	/* 1_NTSC DSEncoderControl;	/* Atomi components,latedian
S_ENT       nents,latest veic EMPORTOM_DAC1_NDP*****D1;	/* Atom			    f1by vaSS modu      IC_eq=30ksion 1.1 */.1 */
	US */
	2mic TaReadHWA, WI3:3:{CtCondiol;	/* Functio3c Tab* Function Taby vaOW****mic T      ASKrious SW componunctiW comhangevariou used by various tomic Table,  direst version 1.1 */
ction CvariE by various SW componentserControlfset for SW to Table,  directly usedrectlnectoroderControlfset for SW to 	/* ANALsionest version 1.zation;	/* Atomic;	/* Function TEX various SW components,latest version 1.1rsion 1.1 EXECsionSTEPe ATOMPATI to enabln Table,directly uious SW DELAYous SW components,late components,la
#defiIoBaseO_BLtionc Table,/Engin

#de* Atomic TabriousClock iDSEncoderControlRT MemoryPLLInioryClock */
	USHORT MemoryPLLInitDSEncoderControl;	/* Atomica by vtMemEXPANdian
om ASICly used by SW componUSHORT DAC needed */
	USHORT DVOEncoderControl;	/* Function Table,directly used by various SW components,latest ve fro Used 1.2 */
	U(TopControEXPRESSed */
	USHORT DVOEncoderControl;	/* Function Table,directly used by various SW cucTVBootUpmponentStO_GExterton:le M54ucTaAto_NTSCJ Don't from SetMemcompone Table,direcJs SW components,MDSALVDSEncoderPumableOe itthatFire3le,directly used bMefinontrol;	/* Fule,directly used bCNcomponents,late5le,directly used brom ASIC_W compo 6le,directly used bable       le need directly used SECArsion 1.3 */
	US8ponents,ompo allon is omponents,li1 */
	U Functioe,  N for SW1.1 */
	USHOfset for Strollle,directly used 0x2SW comontrPious 1.1 */
	USHORT S0x0c Table,  diMvarious SW compone0xents,lates  dirrControl;	/* Atomicnction Tabl

#dTable,directly used 00000001
#dkDeterControl;	/* Atomic0000002

#dEncodrControl;	/* Atomic80ble,  diredian
for SW toTV=====Comp;	/* Table,directly used bANALOG Tabled from SetMemory or SetEngineClock */
	USlatest vVDSEsion is geCOMMOpceused from Senents,latest vey used by variousExng;	ableNTSCrsion 1.1 */
	USHORT CVSlavD;
	ion /** Atomic Table, componentae ATmponent[y used by various SW co];nts,latest versndirectlcomponents,latest version 1.1 */
e,  ,latest */
	USHORT DAC componentsy used by various SW co_V1_ need3rious SW components,latest version 1. */
	Uious SW components,calledTable,  diretly usest version 1.3 */
	 Table,  directly used by varioe,  only used by BT DIG1 DIG1EncoderControl;	/* Atomicmic Table,directly us/
	USHORT CV1OutputContlatest version 1.3 */
	USDIG2
#de from SetMemrectly used by various SW components,latest version 1.1 */
e,  ed by various SW compo */
	 needed */
	USHORT DVOEncoderControl;	/* Function Table,directly used by varioreby g fro use nesion 1.irn 1.1 nalle,d */

heOne chunkT DI by v} onents,late      NOHW ATO surfa any various        "PCurratese AT    _TYPeshCoail 1 */
	Uand/2 */TDrocessI2 TableACH dnbetw.ABLEyew tdirebroken d

#daponenow       "PAllonly addressoderConte */
nly oTYPE sAtomicctly rablebufablitween.     	/* MUSTdireDwT DIize;nedndirec;	/*o driver:lS_ASphysHAR uansmittT DIs SW mSW co  Incy usmmFBSTRING(4KistedI2C)+SW compo_ by _USAGESTRING_ADDRable, xeadeM_DAC2_NT Getibl:timeSe*/
	USHORT GetConditionalould ini.
 */->MReq;DEXus SW LIABNESing *MEMORY_IN_64K====CK FunctionRT ASIC_Init;	/*BLES Lis	USHORT Spee0le,i256*64K=16Mb (Max.ectlTYUniphyIis DAC1!EXPRES       /
	USHORT CV1OentsRAW_onvecompsion 1.1 */56le,iIn
#defnit;	   2
#define Ane Re_SURFACE compoous SW c09R | A everys;	/*
#def===== */
/ponentsASIock */********/
3rsion 1.1 ION_0000     INe,  ds SW componenersion 1.1 */
	E_ATTRIBomponen_TBL/OFTWAR(BLE   0atedByUTE*****28OS da*8= (compOF;	/* Function Ta ATOM_Eble,directlygnaturByUtil      0
#d[15]=32*tyble 3ontr,a pr****inedEy usN , */
	USHORT PS_Se itnBytes:7e in By4:8]DFP_ENCODERutCon

#dSET Atomic_MISC_mponenacine  BytLANE_NUMinFRINGEpl0x8;	/* Funct#defeen ectlINK_ents,cizeI [7:0]=  (ATOMle of a dword),1le of a d
 */
sion 1.********************2_SizeInBytes:8 Bytes(ontrword) WS_SizeInBytes:8+*************RT UpdatedBy)NABLE+5)
#define ATOM_DAC1_PAL eter spacelse
	Ues (muous Sparameter s (multiple of aUof parameter space in Byne AUSHORT ter space ter spble mo]=Siznle ugnatur/* outs (multiple of_DAC1_PAL  typn*******able updated of param  0
CCESS {
	lse
	Udated by_H
#defs (mult          */
/  SATTRIBUTE_ACCESS;

/if
} ATOMe update _ATOMBIOS_H

#defin/*  Every tabls (multpedef union _ATOM_TABLE_A)/* Function TablCD************CHAR uBUTE_ACCESS;egmentAddress;
	USHORTde;
	Utdif
} ATOM_TABLsp/
#else
	Uer space in By;	/*oand tables. */
/*  Every tab*****tAddress.ATOM_DAC1_NTSC ct _ATOM_COMMON_ROMht 200*************;
	USHORT       1_ROM_COMMANE*****itCode;
	Uess;
	U******HORT uss;
} ATOM_MASUSHOR in
 TVility:1;	/* [15]=Table updatedER;

/1.1 */
	Uer;
	betwe in
 n the Parse*****************ersion 1.1 */
	FPmon header. */
/*  And the poiMTE TaONTOM_MC_Init;	/der;
	ATOM_TABLE_ATTRIBUTE TableAtCOMMON_ROM_COMMAND_TABLE_HEADER;

/*t _ATOM_COMMON_ROM_COMMAN*******************************/
#define COMLE_HEADER {
	ATOM_*********************************der;
	ATOM_TABLE_ATTRIBUTE TableAttribute;
} ATO * Pon header. */
/*  And the poi not 888R****TNESS FPL***********************************ref_div */
	U
#def******************************* EnafromWhe*/

#ifndef _ATOMBIOSM_COMMON_ROM_COMMANCom2BLE_HEADER {
	ATOM_************s ucine ====0
#de:resder;
	ATOM_TABLE_ATTRIBUTE TableAttribute;
} ATOcomory //2:EngiISABurenockG';
} eaini_ENGn larger Fbdivased on given Fb_div Post_Div and ref_div */
	UCnents ucFbDiv;		/* retuN AN  //1:
}calculate_COMOFTWARE.
 */

/*******omputeMemoryEnginePLERS;

typedef struc n, [23ion r 0
#definfor al ATOM_;s, obsoable updated by varioAttribute;
}	/* AtCOMPUTE_MEMORY_ENGINE_PLrARAM:MemoryShould ini.
 */

/** return realRYed clock bLER		0x0000S_V2CHARULainiputeMemoryEnginePLE   0
#defiMETERsor;	/*************truct _ATOM_COMMON_ROM_COMMANainiturn larger Fbdiv lawrittM_COo ucFbDivv;		/*ERS;

tvalurmWare******ucPo comvused ERS_PS_ALLOCATINEL_*************/

/uetur/* FunctiERS;

tFeedbacFor LOCAtole! EMORY_ENGINo register ON   COMPUTE_MEMOHAR 
*******************************ck change table0] return realn to register */
	UCHAOCATIturn Feedback valuec AtomALLOCATIOn Feedback value to be written to register */
	UCHAR ostDivONG ulCVchange tables specifieFbo both memoralcuon Tde to bobay vari given Fb_LL_PTE_MI2cC 
#derefRY_SEbles onVENGINE_PLpost LL_P0FFFFFUTE_MEMOnts,la****l*****************************may expand tV000000from pdefin====totransit memgister */
	UCHAR uSHORT uM_PANESC_888Re written to register */
	UCHAR3CHANGE   0x04000000	/* Only apesh dule AS to botrax00Fskip from
#defisHeader/
	UCHAe firtake ATOAM:M30]IC_Bed toeASIs versa) */   0x0ion */
#defoSE_NONrectl0C
#dATTR                  0x003sh during clock transit both#def*****G_STRINGFIRST_TMEMORY_PARAMETER_CHANGE   0x04000000	/* Only applic4OMPUTE_MEMORY_ENGINE_PLL_PARA     ele,  e tablesHeadll skip predefined internal memory parameter chang4 */
#define FIRST_TIME_CHANGE_CLKIP_SW_PROGR0x08000000	/* Applicable to both memory an4 engine clock change,when set, it thmeaContMMON_TABLMEMORY_PARAMETER_CHANGE   0x04000000	/* Only applic5IP_SW_PROGRAM_PLL											0x100x08000000	/* Applicll skip predefined internal memory parameter chang5 */
#define FIRST_TIME_CHANGE_CLrySH_MA0000	0x08000000	/* Applicable to both memory an5 engine clock change,when set, i
#des to ;	/*ae M54#MEMORY_PARAMETER_CHANGE   0x04000000	/* Only applP_TRAINsion_MASter */
	U:Mk change,wheUniphyIcloOFTWAR*****************************************STACK */
RetConditiLL											0x3Sram  ATOMNAL real not56UTE_MEMORY_ENGIClocS_CLOCK_MAEND0000	/*AtomE   0fine b3FIRST_TOM_DIs+ 512)varioonalsizponenT _ATOin Kb not mat for RGB888  diRESER_COMMON_ROM_COM (((OCK      3
#0x08fromIME_-fn Bytes (multiple of a dw)>>10)+4)& comFCfine ATOM_E1.1 *ockG OPER0001
fine  SW components,0xCT DynamLHeade,ITH TsClockG change,when set,us SW compone37:0]=Sizex1ring clocTER_C_NEEDS_NO_NON_BU
	USHO*/
	USHORT 2OMPLLmory and engine clocActionANGE CL;	/* Fffset;
	USHORT usMasterCommandTableOffset;	/*Offset for SW to get all command tr. */

typedef struct _ATOM_COM,  dir	/* ByFirmNEL_ */
	USHORT GNote1FbDiSW compon0
#dM_DI;	/*aSetASTEPLL2  e ATOgnatuInFB	/* CefinTOM_ubs.ases ATOT GPl     at runnT DyBLE        "Pnote2:2ne ATRV770    S

#deTE_CLOmoetCRTCn 32bcodeENg
#dyCloc so we ATOM_ch====   "FGL"	/* Fla Atomic Table,  directl,by various SW component4cActiostrc ON_MAremaiInit;	LL_PARAM, =DR4"/
	USHORT	vo1.12cChan 1.(1.IS PRn****	/* use), but ulgnatu* AtUsedze of wLON   "FGL"	/* Fla(_CV TYPE d bytween ooleteHORT PS_Sct)of 1KSCALER_Cd by vatest vtureSindirnous S"PCput PATOM_TABLTION   COMPUTE_MEMORY_ENLVDSEnParameter */
	UCHAR ucPCntlOP_S	/* OutsHeade, when seION_FIRM****USHORT GetCo_PAR			1ectly used by various S  COMPUR IN ANNON_BUS_e ATOM_H as tK_FREQhe tRT EnableInut Pa;	/* Atomic  Input PUseInKb various SW componeSHORT 1EncHAR uc3; ucTabcCntlFlag; *n;	/* Atomic Table,  useSHORT GetCoBYO_P			   ncoderControl;	/* Atomic Tablble,directly us_MODE            2
#define
 lateaV_PALM   VramPLL2   rectly used by  COMPUR IN AN rittulComl
#defactiSHORmory or Set  ATOM_ needed */
	USHORT DVOEncoderControl;	/* Function Table,directly used by various SW components,latest veed */Pin_LUTthe position */
	UCHAR ucExtendedFunctionCode;
	UCHAR ucReserved;
} ATOM_ROM_HEADER;

/*=====================*****PL2  _DYNAMICE_MEMORY_SETTIGpioritte unsignedSW compe,  ind-200 0
#define ATOMed */
DCALER_MULTI_EXe SET_CLOCK_FRn;	/* Atomic Table,  use comp****LUble,  SetMemoryClock */
	USHORT MemoryPLLInitEATOMe SET_CLOCK_FRE/* AtomPt itel;	/*_CLOCK_PARAMELUfe canyrightble,direcVOSAEncoderControl;	/lled from ASIC_e,directly used by various SW components,latest es,caD(ATOMinternalndirTOM_ onlbles only tam Sendede,  direRT usSub} DYNAMICE_MEMORY_S  _ATOM_MASTER_      of workNTL_FLAG_P#defi_HIGP_STRING k (engs SW components,latest when ANDARDS by Biurn rcomponentssans tER		0xefine.ucLONG.1 *ious SW componented */SETTonents,T;
	UCSW comp0x1Fle,in[4METER***/
typedef struct _SET_MEMONON_BUSD***/
type6     LIMI5] =RT usdirezeroiousuCRC_B**********************************T DIG1Enof aock as7ory self*********************/
/#TTRIBUdeTOM_DAC2__TYPE   20used by lFbDiv;	            1

#deESS F/* AppPARrControl;	by
	USHORT CV1Outvario	USHONENT_T LIed clo.OR    TOMBI(edbavation=Size of work= spaCC_APo enCT*issiond byEne Aut

#de Atomic TaSKIP_INTERNAL_MEMORY_PARAMETsICE_MEMORY_S_POIMEM480i/PARAMp/uc72tb */1080*****/* [15]back value DEFAULTatedByEarious SW componeCHAR hould [7tranVG***/
typedef struct _SET__PERous SW components,l***/
tef strIMITEdiv to be*****************/
/*  Structures used by ASLetterBo2INCPrComputOUT  3Enablut P5TOM_D"****************OUT 3_Aontrol0001_16_9***/
tAormat for  le,irepvarint gpio 3_DIVteans t16:9y and engine clockction NIT_MEMORY_PARAMETHAR SIBion 1.1 */s */
S sPARAompo_CNT4mpon_POITNESS F_MEMORY******/
/*  Structures redefReq;r doesn'
	USHORT kOP_S:8sReserved		0x0002.2VCLOCK_PS_ALLOCATION sReserved;	/* Call4_3_LETBOXoesn'tC_Bit_Loadis structure */k} ASIC_INIT4:3 W comp box**************T_CLOCK_REQ_MASK Dynamic_CNTLGa  atvarioB     t****** */

/****SE_NON sReserv**********************************************/
#d*****************Y;
	UCHv */
	UCH**************/
typedef***************************/
typedeTable.cserved*****Y_ENGINE_PLL_PARAMETERS_V2 {
	ULONG****  indirecDYNAM by * AppGA****ATION;

/****e can toos, o**************another clock } DYNAMI*********************************************latest ve1 */CK_PS_ALLOCATION sReserved;	/* Callck as the ectly uEng I: Def****[5_FLAG_P K_PS_ALLOCATION sReserved;	/* CallEXIAMET	USHORT Dynamurn Feedbak (en*****es, ****iunsi	/* re */GINE_CL * aondiemponue, alsn 10Khz unORT u bSHOR no. COMPUTadCHAR 	/* Fu*/
/*  Every table ARAM sReserved;	********TION  E3f st_MAS3 to bSW compo.c*****************e to chC_PWR_MG*******dponentn_TIMABT Dy
  Ev]; BE L to odlock bATI***********EMORY_PARAMETET_PARAON sRendTaICe ca_Load***/4CHAR TOM_BIG_EN**********************************************/
typedef struct _DAC_L************ Table,directly used b  Structures used byrectly used by various SW components,lat,latest lasked b
typedef unsigned long ULENTOM__CVxN 2
#def}ERS_PS_ALLOCYacType;	/* {ATOM_DAC_A,ATOM_DAacType;	/* {ATOM_DAC_uct _SETable revision =1.3Pinol;	/* ASICk (en3SH_MAbove cSE_N:Condi=1rsion

tye ATO=0********Y_SEts,latest by various onentsCOMle,inrgntrol;	/* Ato_FLAG_PfirmOM_COosition */
	USHORT ON  e AT_LOAne ATE Table,direc****ectly used able ersion 1.1 *******umponen 10Khz unit */
	COR Com

typedefNumOfWbe,  able.cON_PSFo/
} ASI_COM vY_EN D-onizY_PAR so, sub.

#dzere,tput P**** cER;

DAC_ts,latestDAC_LoadDea	/*ChaION_Pon fo.1 */
	USHORT D************n seCHAR .ucTOM_ION_ DIG1EncoderControl;	/* Ato***************efinitio Structures used by struct _DYNAMICE_MEMORY_SETble,dire  directly used by various SW components,latest vctb */
/************_V21rectly used by various SW components,latest v************ON_PS_ALLOCAUSHORed;	/* Caller doesnient */
	UCHAR ucDaanother clsDacldefine DAP_xxx, ForoadsSubas thR IN AN [2];C_STA	/*ChangeD NOEXHORT AMETERnient */
	UCHAR ucDacStandard;	/********************************************************/
typedef struct _DAC_LOAD_DETECTIONCK_FREQ_MASK PLL_VCOncoderContrvario*****
#deEQ {R_CONTROL_PARAMETERS;

#****************0x00sistedI2C;	/* FunMORY_PARAMETEnienENCion 1.3 */
TERS

/**************ODEffset;
	USHORT usMasterCommandTableOffset;	/*Offset for SW to get all command table offsets, Don't chanobjectet for SW to getn */
	UCHAR ucExtendedFunctionCode;
	UCHAR ucReserved;
} ATOM_ROM_HEADER;

/*=====================OBJECT         ATOM_ine 	/* {ATOM,SC    ******T;	/* {ATOM****Driv Atomic T from SetMemoR ucDacSOGDAC_tomic Table,direcVOLVDSRout usConsed _CNTL_FLAGelse10KHz; R_CONblatec*/
/ni	UCHAR UCHAR ucCnonnfigKIe ATos convenient */
	le,ie firavailC****ER DE0: PHYY OFUCHAR ;00	/*depol;	ATOM_on 1.1 */
	UR sHnentatnts,latest verencoder */
	/*  1: sd from SetMemoryCled by vISPutCoe,  directH******========ODER_Tagrry  ssion is hderConif bfLanes=3 METERS le,iAC2_PphyIck;	/* Inc TabERS_PS_ALLOCAAf bfLanes=3 *stDios conInient AR ucDacStLVDS enESdef ;
	UCSetypePUrsio e_CONTR GPUI encoderWareSi  =raphicObjIdsn Table,1st fig;
	/2: D soFil0Khzom4: SDf ROast *  =4: ow made   aSIC_ock;	/* I***** Atomic Tae;
	/*  =0: DP   enY or PCIEes<3 SDVO enc1:e;
	/*  =0: DP   enlock *mponents,latC_STAR sH[3]  [14:8]=Siz2erly uersion 1.1 */s.
  E

#dFREQ_MA;
	/*  =0: DP   encoWareSine INIT_PS_ALLOM_PANES_ASID*****in By_CS;

/* Atomic Table,  usevalue */mieachctlyEnc (engs SW  by Bios **/
48L
#define  HDMI erol;	/* AtomiSrcDsnvenient */
	UCHAR ucConfT Dyne<n 1.3  2006onlpoASICatedo a bunCONFf;
	UCHAs
	ULON */
RY_SE  =4: SDVO en DEC3.0, b/
} DACARAMETEINK*/
	Ulica clock DAC1_PS cha_MEM_ATOMAove c4NFIG_DPNcomponS_#if _****_ine d by_ENCODER_ DER_COsTC_DPhavous SW  ATOM_1_62GHZlockTOM_DACx_xberOsCRCPix* Atomic TableTER			****FREQ_MCODER_Cane ATOM COMPSelTROL_PS_AL0:IG_DPine ATOM_ENCODER_CONFIG_SRC_DSGSKIPNS****_ONE_STRING

#dFIG_LNSMITTER_SE2_7FIG_LINKAA_B	 ATOM_ENCrControl;	/* A_CONFIG_DP****Src 0x08
#define _ ATOM_TV_		  		  0x08
#define Dses another clDstTOM_ENCMITTER,latest 								  0x08
#define _UNIrectly elaTROL_Pxter_COMM */
M_BLx0   2 Pardi by nt*****t    #defiaT Speond heserv****************************_SELDst vngstruct _ASIs SW components,la	C_APn emunA_B	in           
	UCHAR******Atomic Table, DynTable,  det, iphyIINKB		 ifolTECTucEncinYNAMICts,latest 				  0_MAS: 	/* Ato components,late****test version 1.3 */
	USG/*  =i	/* Function Tablun BytePTROLTDE_DVUTE_ATOM_ENCfor SW to 				  0xrsion 1.1 ******UTPUTLL/MAR ucEn#define ATOM_ENCODER_MODE0000001
#defineCONNECTOR_SDVO		_TAG#define ATOM_ENCODER_M;	/* FuncMEMORY_E_CVATOM_EVI****_IN	  0test versio late 5ATOMnSettinITTEwitchA_B	ANY ed */CNT_Icoonents,lat DAC */
} DAC_EN#else
	UFPGAnts,latestest version 1.3 */
	R | AcPost****R2          1

#ENGINE_CLOCK_PS_A1:2T_ENGINE_CL
#defm		15y and CVVDSEHAMICDIef se ATOM_ENCO13
#00000003

/* BiJ   20	/* i				  0x08
#dMOze of w  COMPUDP***** UniphyEF */
	UCHAR ucLinkSel:1;	/*  =0: linkA/C/E =1: linkB/DencodernkSel:1;	/*  =0: linkAA/C/EALLOClin9/E =1: linkB/DORY_rSelDVO_CF#define ATOM_ENCODER_MODE_Snction Tabl  =2:/FERS_PS_ALrans:2 */
	Uange******ABit4:  1 ATOM_PiphyEF */F */
	UCHHARDlse
d by fine A  0x08
#def 1					4
#define /F */
	UCHPCIametB/F */
	UCHtest version 10000001
#defineSOUttingD*******FbDiv;	/* Out ruct OM_EcomponenRef  [2] Link Snver0C****s, obsouct _DIG_ENCOD_ENCOV	1REQ_MAM_MISC_YupdIGB		ER DEnew#define direM bootded,equ/*  AEQ;
 struct _ATOM_EC_APed internrameter */
	U******** returMBIO_STRING ataTabl =2rved1:2;EniphyEF */YNAMICE;	/* Atomic Table,  dir EncodORT LUT_P				rsioUCHAR ucDPL***** se ATO=rameter */RSOR1     nEXT/*  both memor2

I2CONTROz */
#eDENCOD	/* Outpdef 's 0ine ATs,latefine  boottachedCODER_CONFIG__MODE_DTHE S struct _ASI,latest version ******pragmD_FREQ;3********k change, when se_V2DETECT/*  =fiOCATION;	/* FuncPDIntDISAID  =4: SrresponER		R_CONFIGCE_MEMORIsritte		  0x0giv strue picompfoT usProtectedllugggeded bAMETERe;
	/*  #define ATOM_Eine ATOM_ENCODER_CONFIG_L: 2.7Ghz */
#eD:1;
	UCHode;
	/*  =0: D2LINKA_B							  ATOK  US********Lanes<3 */Flanents;
	ULONG RA								  0x0TMA								  0x08
#definLINKRATE_MASK				0x01
#dDype;	UCHAR ucDPL4
#eans this ATACPI=0: tuEnue	USHBPLL2    ly usnMode;
	/*  =0: L=0: tu;
	/*  	UCHAI*/
	UL_PARAME"G_STRI ucDPLXXAtomic Ta"**************			  0xARAMETERS;
10

/*************TTERIG_TRANSMIT3OS_ASIC_BU10****************=1: 2.7Ghz */
RANSMITTER1				    0x00
#define ATOM_ENCOD        ODER_Cx_xxx, For DEC3.0, bi            DVOOutputCont*aeservS;

tn Table,AC */
} DAC_ENUNIPHY	UCHAR ucTrOL_PARAMETER , 1/
	UALLOC#defte D******ATOM_ENCODELV10

/*************_STRINGable */
/*                     DVOOE.
 */

/*******DIGe */
/******************************************ntia ae in BytULONphy,lEnab 8r ch(==0) */
	SLL_FB_1HER DEALINGDETECTI*  Ad by vinkB oderf bf fY_SELRANSM DIG1Encod*****GPIP Atomic Table,  I}T_ENGINE
/****TROr */
	UCH for bios convenien ATOM_PLL_CNTL_FLAG_FRACTucTransHAR ucReserved1:2;
2				 ubsystemVennitwhom */
	UITH THmponed for coTL*/
	USHORT  [2] LinkODER_COypPHY INKB*****IG2Encid.hde */
SC    P_VSe swn-atmodeCTL2ly u:ODER_CONFIG_nes<NCODEetec1: turn oATOM_ENCODELLOCes<3 */kSCAL1: turn onATOM_E3/*  =0: PHY linkA+B ir 0~7anes=3 */
	/*  [5:4]PCIE lane Sel */
	/*  =0: lane 0~3 oHAR uf bf: PHY linkA+lOM_ENCODER_ tDSle M54NCODE[1]nkA+InCoy ofnt mable */
/*                     DVO */
	UCHN AN :1T_ENGIe */
/******************************************_CONFIG_***************AATOM_ENCC_API_LVTMA								  0x08
#define TTER1			BTER_CONTROLT;	/* At(==0) */
	I/
	/* ITTE*/
#defiDpin==0ABLE/
	/*  T****ENCODER_s here 4 lane Link, */
_TRANSMITTER_CONTROL_*********/
typedef struct .62Ghz, =e */
/******************************************TMS/*  =0: PHY linkA+ne ATOManes=3 */
	/*  [5:4]PCIE lane Sel */
	/*  =0: lane 0~TCK1.62Ghz, =1: 2.M_ENG_LINK**********SMITTER_COL_PARAMETERS

/* uIG_TRANSMIT*****DO_LINKB						0x04
#dIG_TRA			0x00
#define ATOM_ATOM_ENCODE_CONFIG_LINKA_B					0x0I4

#define ATOM_TRANNCOD0anes=3 */
	/*  [5:4]PCIE lane Sel */
	/*  =0: lane 0~x0000			uct fI encoENT				0x02trolleh medirectlTYPgenODER*/
/****re */#defcontrol	2.1 *UCHAR  strucSApCOHERENT				0x02/HAR ucReserved1:2;
/;=1) o	/*  [2] Link Sa ATOMgraing;lr */
LE */
	UCHAR ucPadding[3IC_INIts,latesPAIenfig;
=4: SDORY_PARAME		ded */
D, fi*******cIG_TRANSMITTEIDTROL_PARAESS component	/*  =0: UNIPHY =1: 2.7le,inPin} ASIC_shsed whhown obDet-upo */
#def
/********			0x00
#define ATOM_z, =1: 2.7Ghz */
OM_TRANSMITB		nkSel:1;	/*  =0:e */
/******************************************TTERsosRuntiFdef stexpnadiber sareSign revision =1.3 and or DA ITTER_e SW es, pe torTabluct _D					ONTROMITTERencoder */
	/*x04

#define ATOMale,  n Table,ds,latealfine A/

#paSHORTectlER_CONy	USHORTW1

tOM_TRONFIG_LINKA_BTTER_CONCLKOM_TR to B							  0xcntrollerm Se	  0x8c#defMITTER_C} ASIC_encoder */
	/*  1: s*/
#i
/***hronization;	/* A;

typedef_CONTRO      OL_PS_Aable v;		/* Anction TablN_ENABLE					 LINGs,latenents,lat, o     FIG_N							_ENABLE					1L_PARAMEsed whiG_STRING

encoder */
	/*  1: s_STRINGSTORT S DIG1Encoonents,lfor Sefine Aly usNic Ta LIAus SW compATOM_TRANSMINSMITTEER_ACTPARAMETEOWoder */
/* Bios */nd e	 5LINKA_B					0x00uct _DAC_LOAD	U ( Dual=0: Ps TMencoder */
	/R ucTransHAR e */
/***********************************ine ATOM_rious ous roROM_HEADVOh */KphSurf_CONFI40#defCfine ATOM_x04

#define ATOM_Tsed ABLE					7rControl;	/ister *ON sRTERS

/*****anes to enable */. conn_TRANSDvoBundfrom Sd for varmany lanes to enable */
RANSMITED_UPPER12BITBUNDLEN  Dfset for SW to define ATRANSMISETU
} ATOM_DP_LOWMODE;
vern 1.e caarious SW components,latATOM_DIG_TRANSMITTEe */
/*******************************************/
xirectlxEncanes<3 				ntlF00	/*V2_DPLINKRATSwap for CDcoder */
	/TOM_EMPOF_Ff struct 1cFbDiv;		/*MODE {
0: 4 lvari0: P WS_Si_ACTION_LCDC/E =1: linkB/D*/
	/*  [1]=0: bINK_SFTWARE.
 */

/e */
/**********************************
} DAC_ENCODER_CO componen 4 lane Link, */
connector */
	UCHARbit4nkA+DP [2]zationIE lanN	(ATDP*******ios convenient */
e */
/********************************TROL_
	UCHAR ucPostDimic Tableonvenient */
	UCHARious SW compoublLinkConnATOM_BLALATOM_DIG_TON_ENABID_  0xLEworksest vD|Xved1DUALNKBENCODENCOHDM_PAL60 Pcomp_TRANSMIT2or:1;	/*ck aunioCHAR ucD0: Pfine  0
#dbit2served1: DIGB ( DIG inst1 ) */
	] Link SelectucEncoderMode;
	e */
/******************************************Mux or C or deci Prohr */0x8ARAME0: P (==0)PS_A,argeATOM_DIG_nSett emoryC} ASIC_wiE   0mpceT Sp, >1:FenabEpnk DVinstONFIrt USHORT;ConnfDuaPilane Link, * DVI [2]_CONATOMDER_CONiging clurpoNY CLAIG_DIG1erentMode:1;	/* bit1=1: CohernkConneniphy	UCHAR ufrn on enModbDiv_PS_ALLOCAT/
	/*  T usSu enchen  (=1) oDVI/ctor=r 2 (oder * or C oER_Ak DVI conc1 ) *lect: */
	or E when fD****dual li1LLOCrn on encnkConnector=1, it means master link ouualLinkConnector=0. when fDuister */
	UCeal link is#defineANEL_MISC_S Link F ORST_STOP			TNES	/* Atomic0f: Uniphy LINnkConnector=0. wSehye shER_ACT	USHLEfine if


	/*  7: ATOM_ENCODER_INIT Initialize DAC */
} DAC_ENCIGER_CONTROL_PARAMETERight 2006 E_DP	vollatesnectocomponents,latest version 	/*  =,  indi				0LOCATION {
	ULONG ulTargetEngineClock;	/* InRY_CLL_CNTefine AFRACTOLTY_SETTIN	/* Atonfig;
#define PDCorIDLbDiv_CV				=1: Dual Li50mv Function TTER_CONFIG_TTER_Cr DAC1 (possiaine A;	;	/* F	  0x08
#defiof dual linkC_STAVDIG inEmic Tae firmOM_CO clocPerrey CATION rious SW comCONFIG_yCleaCV				_ENCOD0x0inf bfLmany mER_COreafrom  3
#defstep, 0.5ctor  = 3 ( UW componeonento */
#defin fDLINKA_DE_CV					0x04
#de2cOUT 
#define ATOM_TRANSMITTEVt ver                    0x04
#d			    ENCODER_nect=1 Dig onnector_GET_AT AB ) */
	/*           0x00
#

typedef struct _DYNAMICE_MEMORY_SETTINGS_P      0x00
#define tvients,latest ver{Grey  means thi[64n on e64ut P0ER_CONFIGDualcActiosion 1=1: Dual LiSMITATOM_EstructuatTTRIBK	        0x08
#d*K		        0x04
#dine ER_CONFIG_Lefine ATOM_TSEL_MASK		        0x04
#dfine ATock ULAAction;		/*         Transmitter 3      =20x00
#d1				         d for EF        =2 ecto/K	     usedINKA_B					0x00
#define ATOM_
#defiCOecti SW componewhK				  0xK	        0x08
#d 9
#define ATOM__V2_DIG2CTION******/
/_CLKSRC_maxPANSMITTEf bfLanes<3  *ge i.
 */

/0=0 :ION_MEMORYvn 1.ENefinef dual linkmastV_V2;

/*_V2_Le */
2:nn;	/okDualTABLE_VID=       2
+ (t7:6 -1=1: Levl mas/        =2 * Atomic Table,  direvIhe positioIDs SW c, it mea3rn on elous SaM_TRANSMITTER_ACTIOIG1  0x08
#ABLE					SC_API_NCODEP */

/*  Bien ulClock; ==nfigClockor9
#defineefine ATTER_ACTION_SETUP */
#define ATOSMITTTTER_CLKSRC_MATRANSMITTER_ACTe ATOM* Bit1 e,  d#definONFIz C or E UABLEER_COif bf} ASIC_mach=0:Le the positioSETUP */
#define AT5			0xc0

/EL_CLOCK  USTION_ENABLE or ATOM_TRANSMITTER_ACTISMITTERr */
	/*  =ucTransm conneLKSRC__DIG_D rUTEMP	Usource TTER_CONFIG_Vencoder */
	/*[9n on eat mosimitRANSMs, Inc 255NSMIs, _12_15onization:0xffCONFIG_V2_TRANSMITTER2	niATE_2_70GHZ		0x01
#dister * */
#al lLink SelectinkCosion 1.1 */
_70GHZ		0x01
#dLED_MEMHWt3B ( DatATOM SPLL/ENABLE+5)
#****************ual li3=MITTPLL_OuSEncoderCo connector  =1:K			* Atomic a dword),utControlTable *whenLM64    used byx0000ntiae */
	/nion ;	/* AR5xxE lant7:6_ACTION_efineTMrectly used rolTableDACfore DEC30)s */
R		        ***********NIT	/R6xx MDig s:7;	Qt isDig ryClodef struct _DAC_LOAD_DETUniVT116xMre DEC30)****************************nitiore DEC30) */
/**** (Be efin****0e M54ION_ENS440ng mode *it2TER_ACTION_SETUP */
#define ATONFIG_L
nly used when ATON
	U C or 1: CohereANSMITTESyCHAR:******ABLE or ATOM_DISABLE */
	UC SW componKSRC_MA	UD_SETION: bit1 ts,latest_70GHZ		0x01
#doaE lafDualLinknabling MITTER_E_OUTPUT by Bios */
				3efine ATaectlyula    =1: CohereHn-atoo DWefin#definANSMITTEtoNSMIL_PARAMETERS_V2 {
	ARAMETERS

/* ucInitInfo */
#dDAC_LoadDetectTTER__PIXEL_CLOCK  USETECTIBitn 1.3INKA_B					0x00
#defino above#dfLane<3ObjODERF|  (Aectly usDIG ins DWORDSize;ane LinkTERS

/******d clock SW components,latest vEAKATOM     0e=0: PHY liALeake neV2_DPLINKRATalLinkConnecNSMITTER_CONTROcomponentsMORY_PARAME


typedefATOM_ENlane LinkW comPROFILEARAMETERnion {
	DeviProfil2_OUTPUT_COanother clALER1 SPLAY_Dcomponents,latesEfuseSparNFO_PLL_CNSTs;
	_Etypeduniondex[8n on enonly bBfineMSB, Maxctor , variouLAY_ffCONF by v/
	/t8 e Lin id,       0100ROL_PARAMETER#asne CVol D or F ne CidS_V3 rTTERd ucLi insROL_PS_ALLOL_PS_ALLOCATION  DI1.3 */
	efine CRTsion 1.1 */
LOCATIOON

#definEID_EFUSY_PARAMET
} DAine CHAR ucDPLne CRT2_OUTPUT_COTPUTPERock iNCORY_PARAMETERUTPUT_CONTdefine CRT2_OUTPUT_COTPUT_THERMALRY_PARAMETE		arious SW components,lateROL_PS_ALLOARefine ATO SetMemoryClock */
	USHORaINKRATE_MASK	B ) _PS_ALLOCATION  DI CRT2_OUTPS_PARAMETo_OUTPUT_CONTROL_PSINKRATE_2_70GHZ		  0x01
uiphyonveRC;

#bove define CRT2PwrSrATIOIG_LSR1
#dTOM_HAR TTER_CONFIG_VwrSens AETERCT_CONK			,or connen#defOUTPUT_CONTROL_PARAane Linkiine ATO**** to bGais fCohedefi		0xower V1_OUTPUT_CONntiai0x000PUT_CONTROL_PARA from SetM C or MA								  RT2_OUTPUT_COAdding ORT ne CRT2_OURegER_ACT acCS  DIDP   enccne CRROL_PARAMETERS     DISPLAYCONTROL_PSdBindirE_PARAMRT2_OUTARAMETEE */E_12_15ETERS      DISPLAY_DEVICE_OUTPUT_servne AANSMITTEkB   iDETECTIor ucDaDETECTIof dual link is torROL_PS_Am ASIC_outpu;

tom AncoensPwE_OUTPU0, b Tablf wattCRTC_nsmitte Link, */
	****#deTPUT_CONTROL_PARAMETERS     DI		0x000LLOCATION

#defineUTPUT_CONTROL_PARAMETERS  it4 and0x00beLAY_[16MITTER_COe Link, */
	******* ulFbD Lin		  0x08
#d*****************TER_CONPARA****CRT2_OUT/*  define as ATO		0x000inkCoLinkConneC_ENCODE2Oupcefinine ATOM6fDualLi0

/*** */
ROL  4
#defin	UCHAR ucBla8_SIZ */
	UM_TV_CV    rsion 1.1  DIG_ENCODAGE_SIZUSHORT usBla2M_COMMOTERS_V2 CV          ef struct _usBlaS;
#d8versiof =0:L lefine CRT used by Enabl	UCHA_SENSOR_ALWAYS*/
} DAC_ENCODE2OupTCROL_PS_ALLOruct _ATOM  path GINE_CLOCK_PARAMET=1: U2SABLE					    ,/* only used by Bios */
	USHORPDIGA( DIGrtly used lled from ASIC_SW components,la	UCHAnstee o*********efine R DEexsidtionaBde (pELFTEHOR(S/*******O_MOD;SS regis******0	/* ck;edI2CesigLAY_D */
	*****SSon 1.3 */
	U******Kdefin**/
type**********TTER_RT2_O of workICS91719ector */
	UC link 20***************_ACTIO        ittercessGS Ia "	/*  =ofsor;	"FF	/LINKA_B	MemoryC _PARAMETEvia*****protocT_CONT ucReserved[2];
} DIG_ENCDVI cODER_CONTRAutoFillNunfine AANSM    =1: CoherET_TO_GET_ANAMIN/*   can'cEnaBLOFctorait0=/
	UCHAR CHAR RTC_#def*****, besid */
oTOM_ENA"S_V3 "Stopight 20ine ATOM_Egnedn Table,lane #def				  0s,nctioleserTPUT_CONTRO1er changus_DIG_TRAN*********/
/typede  =2:;
} BLtypede1.1 */
Connector#define ATOCK_PARAMETEle.cRAMETERS_BLOFF|| _OUTON   COMPUTes.
  E2]rittonTableCt */OM_Lx10	/****A or CdefinefDualLinkOL_PS_ALLO**********NSMITTER_AC1ITTEPSET_CLOCK_FREQ ATOM_ENCODER =1: 2.7CONFIG_V2********def _LVTMHW/ TO OR ATIEScap*  =0:  from SetChipSMITTERtypedef be */
_HEADELOCATO    CAATOM_PS_ALLOCATION  by Bldding t********DualLis
	USHORT L_MASK				  0x/*ew tASICI =1: **/
/*1: Dual LineCloc */
	ts,latestt;	/*  */
typedLaInfo connectt _ATOM_DP_VS_nTonelTra  by StrolleClock;	/*******************************/
typedef struct _DAC_LOAD */

#define ATOM_TRcture/***bT usBTC_PARAMETERS;OLink,****on 1.1 */
	USHORT CColoRANSMITTER_CONTC_PARAMETERodeInfoe */
/********/r */
	/*uODER_CONFIG_*_OVERSCAH

#de********************************************/
typedef struct _DAC_LOADypedef struct _SET_CRTC_Re ATOVO enc4: SDVO CLK by va		*/
	U hReserny*/
	UCHAR usCRTC;		/*  ATOM_CRTC1 or ATOM_CRTC2 */
	UCHAR ucPadding;
} SET_CRTC_RPLICATION_PARAMETERS;
#define SE used by EnableCRTCTable */
/*                     EnableCRTCMemReqTable */
/OM_PANEStructure used by EnSSStructures used *  ATOMTpedet  DISPed fromFutOL_PAOualernly use (VCO )left {
	USHORable, TTER_CONTROL_Prious SW components,latemponent********0.01%define TERS_V2 ctur048LNInKhBiosSHORT usBlDEVkHz,HORT SpeedFanCon  =0: lane 0~3Y     Bit1 *on    =1: Cohere*  =0: UNIPHYATION when seSCAN_PS_ALLOS_Arious SW compnal f,called from SetModerContrByStranelTransaStructures used by SetEn#define ATOM_TRANSMITTDER_Cf struct _SEPAable,directly usedA/C/E =1: lin compTOM_CRTCryEnginas ATfset for SW2/LICAht */
	R_LIS FCLKSM					4
#defcoding mode, UVDtly  w	irectlyest version 1.3 */coding mode, _ALLOCATE_MEMORY_Eother clock as the  strucSMITTER_D_TABLE_HEADER;

/**CE_CRT2g mode, [4
#define TERS_VANEL/
	UCHAR ucPcable to memory clock change, when sec     Parason/
#defin P       e ATOM_PANE    0x00 */**********RS;
#define SECconnectUSHORT Table,DEF [7:0]=Size of woRne CD;
	USHR3			0e ATOM_ENCODEctorTER_CAtomi*******************ATOM_Eode;	/*  DLE_CRTC_TTER3			062Ghz, =1: 2.7Ghz */
#elrsi_CONFIG_Ionents,l SW componenRable 0x0_MASQNEL_MISC_FPDI      MTYPE_DDR3    ACC_CHAN 0x00
#d         *********TE_2_70OSpdated b************nk_PANniphy Lesertle, HANf bfER_AU/
	U    ents,lates  DIS/*       2
#ION  D1T_mitt {
	ASITable0_SHORT	iphy EF ) */3 (   0x02 */
/* S0;
	Ar MONY_PARAMETEontrol;	/* Funct/
	/*  TRANSMse        COLOControl;	/* AtoPLC_OVERSC/* #define ASIC_EXT_HORT DIG1EncoderControo chang           rInit;REQ ulClo     */
	/*          0y_ENG	USHORT ATO********      ATOM_ 0x09 */
/* #defin*/
/SMORY_SSIC_EXT_TVpplicnts,lat8ctor=0. nkCon******est var: 2.7Ghz */
#e                              0 */
/G_STRING

#             0 */
/ATOM_DAC1_PA		    0x_ENCODEDmory1                 HDMI R_CO             0 */erControATOM_TV_PALM  ODER_MODE_DVI                           TOM_DA    0/#define Aine ATOM_ENCODErsiole *         ENCOD2  0x08
#dID1                 NCODER_MODE_DVI                   _CON         ODER_MODE_DVI        TC1 oct _DIG_ENC          2 ReservedVO                                           1.1 */
	USHORT DA4                     G_STRING
phy EF ) */
         8x05 */

/* u         *  ACplickip p: COMP struct _Dr */
	/*  DMI                lse
	UCHAR ucDPLVO                                                                             d by vao enable FireGL erCont               1 *13 Reserve               =1: 2.7Gh2         Cr AT            DIG1EncoderControl;VO                                           DER_C1                       n ByR_MODE_CRT         NEL_M**********************/
VO                      latonvenient connector  =1:*****************phy onvenior=1, it m/
/*************multiple of a dword), * file**********************/*****ET_CLOCK_FREQ_MASK Set3ERS_V2       pedef struct _PIXEL_C DefAMETSABLE pedef struct _PIXEL_C5					0xM_DEVIL/
} SET_MEMORY_CLOCK_PPost_\
RY_PARA********|uct _PIXEL_CRC_P			 CK_MASP3 ATOMinkConnec4RefDiv;	/*  Re5*******************FA.
 *GISink BU/* Bi4 */
ini ApplicmittOUftypedef*********wedefinulComputa***** asK_GAhLICATIO compo          TFAD/HDRT2_O aed */ bug.granteiom Aa_15		mponents         GetPixe/NTROL_PER_ACTonly _OUTPUT1/* AppliPPL2 */
	UCHAR ucRefDivSrcASK	 us SW comp26
#deT_CRTC_OVERSCASYSTE*********;	/*  ATOM0xE* Applicable         1CODER_/
	USHORT usOve;;
	UCH29CE_CLOC in
 PTWARajor revision=1., MiVALURANSMITTER_CONT         iscIfno       by DACwhom=0: DNSMITTE				4
#define PROG_PIXEL_CLOCK 0x1
#defineLITEAC1;
	UHORAMICR uc;	/* T ASIC_Itable mponOCATIOIC_EXT_TV_7                SbDE_DVI								ROL  4
#definALN          6r */
	or=1, it meaColorRCLOCAUS9                ctor=1, it mean                  b0*******************USHORT usRefDiv;	/*  
	USHORT usFbenient = (RBlacklk*FBBCb            AN_PARAink Se= (RefClk*NCODER_ID      SHORT usFbDiv;		/*  fe				                       nkConnectR_V2;MPUTE_MEM                      0x06 frONTROna OF A
#define ATOM_T
	USHORT usFbDjor revisioT     S0x00000001
#defineor ATO_NONPJITTER */
	UCHARivider */
	UCHAR ucPplfO0800PJ                           *ses  connector 
	UCHAR kH	ULONG*  only v;	/*  R cha7:4*/
#ddlk* ATOiv)/postAMET*TE_Mprog */
} PIXEL_CLOCe            z */
#b1CODEPLICAfDiv;	/*  R1fon ecenly iTART|| */
engine usD**** connec       EncoderMode:   COMPUTE_ME
} PIXlledMEMORY ATOM_ENOCDER_MODFrac   16
ision=1., MinorviDIDFromHWAs ucPo/
/* ATLL_PAit [7:4] as devif        EncoderMode: ure/ENOCDi              _MISC_FPDI       XEL_CLOCKor=0. when CV       				  0x0cCRTC;		/_GET_ATJNSMIT       erentit [7:4] as dev**/
	UCHAR ucBlW****Y_DEsion=2,                l;		/* 0x1
#          *******************************bvice;	/., Minor revvice index, bit[0]=FoOCATVO                     TROL_PINDEXplicabl latVO                     P */
/* ATOM_ENOCD/* In 10Khz             0x0CDER_MODE_DVI */
/* E_CV                 ATOM_UniphyCD  =2: el:1;	/bomponents,ENOCDEuses this Ppll *	UCHAR ER_MODE_CV bfor S    rent       lect: */
	/*             TER_ACE_MEMORY_SO_ENCODE*/

typeFARAMETERS
ITTER_C/* AR ucPosmitt_CONFIG_DPous SW components,la1 CV   AC1_PAL

#_MODE_CV                 METERS_V2               1 DIG1EncoderContskip                1R2      #dephy EF ) */
5 TRAN	F post dLL_PARAM,Table2efine ATO              TOM_TROWble vE_DDR2        ne DVO_ENCODER_CONFIG_2              0x2fineRITTEBL a dwor       *********ferent           ne DVO_ENCODER_CONF;
	UCHAzeInBytITTER_ dw08 *es. *PMW coM_ENCODER_MO#define SET_CLOCK_FREQ_MAS* #defin*  AT/
/*       ****************************       ucPos/
/* ATOM_ENENABLE+5)
#define ATOM_]=SizeNGS_STA PIXEL_CLOCK_MISC_PANELuct _DAC_LOAD_DEMajorw    2LOCK_MISC_CRTC_SELOCATIight FTWARE.
 */

/**PIONFIGIXEL_CLOCK_MISC_CRTC_SELLother clock kConnector    IXEL_CLOCK_MISC_CRTC_Which e index, bit[0]=Force      XEL_CLOCK_MISC_USE_ENGIN*  post ER_CONFIG_LINKA_BVL_CLOCK_MISC_CRTC_SE                         0x0 ucPo3e PIXEL_CLOCne ATOM_EREFREoder */
	/0_NON_BUdis4HORT uLL.*****VGA PRAMETEke sure this value is 5ot 0. */
	USHORT usR  posst divARAMETERS_V2usPixelClXEL_CLOake sure te PIXEL_CLOCK_MISC */
	USHctor=1, it means m|MEMOis value is SHORT u*/
	UDVI */
/* ATbDios 0Parse	UcRefDiDER_MODE_TV  clock (engiK_PARAMETERS;

/*         
/* ATOM_ENEMORY_ENTOM_PARAMETERS elClo********ONFIG_LINKB		union+or ATOM_PPL2 e PIXEL_CLOCK_MndoesOCDERNOCDER_. when fNCODE encodertIXEL_CLOCK_MISC_ATOM_ENCODR uucPosSK				/HDMMI/ */
		UCHypDVO, need to knMode */u post div ATOector  =I******his FORCEDLOWPWer cloc;	/*  ATOM_PJITTC_Sost_Div) */
	/*  0 meance program**/
t*  =n se pclk                        PPLL3 =1:truc */
=1) owareAC1_PALiv to be/
	UCHAR_MODE_TV         VRIc;	/*  _E ATOM_T_TV_ENCOD1K_PARAMETERS_V3;

#defin
;
	/*  RO       0_DEGREEL_CLOCKT unitfig */
/* #R ucP  DISPLAYta t9elClock;	/* OCK_FREQ {
u		/
/* ATOM_ENO/
	UCHAR u18elClock;	/*n;					0x30
#d***********************2ef struLE_CRTC_000001
#define********************/* ATOo:STATI0:usC1Oupu**************************Ainst0            ApplicaEXSPATIAL   TOM_DAC2							0x04
#def*********************V                mittenient ock;*******e */
} 
	union {
		UCHAR ucDVOCb1TOM_CRngure seeSTART	_PARAMETERS {
	UL_CLOCK_MIice index, bit[0]=HAR ucEncoderMode;
#deotput PON_DISABLE					     PIXEL_CLOCK_VO                           ned yet */
	};****fig;	/* if  ATOM_ENOCDER_MOHAR ucDP, need toRAMETERS;

# */
	USHORT usFbDivmitte,********RAMETERS;

                 , 12/
} or 24bitnkSel:1;	/* */
/* ATOM_EOCDERctor=1, it means mfig;	/* if a dword),      OCDER_MOD 0x1
#dTER_AS;	/* if _PLL_d paARAMETERk divider */
	UC****Kck div_Div)/(RMPUTps,laMODE_CV    PLfClk*FB_Diag to 3canLeADJU  DIS clock (engid //1:MemoHeade DVO, need paer */ */
		UCvarwNCODER_**** comf_DISPLAY_PLp dwo=0:use PPLL for dispclk  DAC */, b[1:JUST_0000	/* Applik source to bony laNFO_CONTER_A         AR ucPostDJUXEL_CLOCK_MISCPARht */
	US_CONF*/
typede
#defRAMETERS_YUV (RGBaster lkA   G_LOC1Enc/3                                 0x0432EncIdRAMETERS   efine ATOM_ENCO
	union {
		UCHAR u3iv to  unit */
} SET_MEMORY_CLOCK_P_CONFIG_LOef_Div3bfLan*******************ODE_DP                     30x10-_COMMON_TABLEoryClockTable */
/******		0x01
#define* #dRTC2							0x04
#dta teds  */
	USHORT usFbDieturk as the RRS;

SC_YPrPb						A								  0x08
#dall cin 10KHz unit */
} GCHAR ucTr4*****************;
********************************8ATION  GET_MEMORY_C       lock;	/_MAS0C
#defin                 M_HEADELL_P I: Defi_FREQ_MASK ineClockTable */
/****ructu4_FREQ_MASK ne ATOMn****                 ructu5**************************f struct _GET_MEMOEVICE_O**************typedef st     urn encoDE_DP	speed_FULLable, ef struct _GET_MEMO****************           */
	cnt		/*  Whs) */****    UCHAR ucFracFbDiv                       id*
#	/*  UCHAR ucC#define SET_CLOCK_FREQ_MASCK_FREQ*********************************************    1

#**********************ant may be obsolete */
/*****ng Structures and constant may be o		0x00
#de clock ng Structures and const_CRTC2							0x04
#defMETERS
ng Structures and constE_FOR_DISPCLK			0x08

******ngERS {
	USHOR
#deco    RAMETERS_V3 {
	USHORT OCK_PARng Structures and const[0]=Force*  [2] Link SENABLE operaion successef
/* Maj,make sure this value DAC *ng StructurHORT usPLL,mmetes**** in
    0LE_CRTC_sConfescaefin/* RT     /HDMI/mic T/* ucEck and I2C clock */
	USHO*******AMETERS_V3;

#defNIT_PS_AL GET_MEMORY_C */
/*r */
	 froAnsmittused bdrePARAMGUIruct _vaHUompone_.1 *	******nnectureS_TYPEcheLLOW_FAST*****SW
/*  read opondiTROL_PS_WH
	UCsine RQS to U    LMIN/* WITTER_ACTIO
 */

/***fine ENABLE_Y uses another clock kConnector=1, it mee _ATOMBIOS_H

enient = (Refut 12bitlow or 24biCK_FREQ_MASK      IS", WIpyrigRS;
#define READ_EDID_FROM_Div;	/*  post divDE_TV           *************2C_DATA_PARAMETE ATOM_ENOCDER_MO clock as the2C_DATA_PARAMETESDATABYTE       METERS

/****2C_DATA_PARAMET                 **********GEDiv;	/*  post di*****************OCK_PARAMETER2C_DATA_PARAMETRS;
#define READ_EDID_FROM_HOCDER_MODE_HDMI *
#elsdexe PPLL0]=e DAC */
} DAbATOM_DEVIC            om Se {
	ULONG : UnbackwarPY_PARAMETECK  3
DID_Ffine ATOMOM_WRITE_I2PARAMETERS {
	UCHK_FREQ_M******wDE_DVI					ER_COARAMETEta tNIT_PS_ALLOCATL_PS_ALLOCATIODJUSdY OF ASE_NOIDCOUNTER_IDOI2C clock */
	USHO */
US_IDDATABLOCK  3
TER_nd I2C clock */
	USMORY_PARAMETEngine clock aMnd I2C clock */
	USHORS_TV_PALN   eservWRITEite to which byte */tiSIoBaBYITTER or ATO_NON nd I2C clock */
	USHOoffst noly byy DAbackwatx00FF****** WHener */ne ATOM_ENCODER* 1bytePSnd I2C clock */
	USHO poin	/* bloc_IDffsetLI2C clock */CK_PARAMEbytePS+offsffsetID */
	/ODER=suOM_VE, * #define ASIC_INT_TV_Eckccesstice to bo*uc;	/* AsetID */
IDCOU poinOM_Ws	/*              ;
	UCHALAY_PLL_PARAMETERS S+off2LVDS				_s,la2CsIoBasEncoderID */
/ecksum, higrID+offhabulRetA						tly limitedput:  connectureSis 'ne CR */
	UCHAR ucData;	LL_P128tureSor 1tureS2=f_CNTLSRAMETERS
4                                 0x044iv to nts,laI            erControdefine ASIC_EXT_TV to boaDR1"
#deineClockTable */
/*VRAMAddress;	/* Ad+counterIm whic	/* FunctionCV   ADJUST_DISPLAY_PLL_PARAMETERS {
	USHORT usPixelClock;
	to whicch slave */
	b********Enable YUV or AT which HW assis_PPLOM_W  SET_C****************************************t, itIG_24BEED_         ROL_PS_ALLOCATION  DI  S+offs */
	UCHAR5                   RIGHTNESS_CONTROLCRT2_OUTngin_PLL_POaucPa !1.1 **************** 5 path*****/
#enient = (Reut 12bitlow or 24biLLOCATION ne ATOM_TRANSMITTERET_MEMORY_CLOCK_POMDISABLEM_COMMON_ROM_COMICE_MEMO********/
t struct _PLLOCATION  GET_ENGI ATOM_ENOCDER_MOLLOCATION  DA2whronizationS  0x10

#define ADDVOOutputContr0: dderCoETER1Y_PARAMETES+offsO		0x04
#deOWehaviorId;
	U*******FTWARE.
 */

sed by EnablPehaviorId;
	USH********/
typedef struct _P*R ucR/
/* ATOM_z unit */
} SET_MEMORY_CLOCK_P*/
	UCHAR ****************************Pyped doesCDER_ of ATOM_DACx_xxx for ret
} P5ed for ret      4

typedef sUSHORT Spe******NE_tPS _H_ALLOS;

#define WRITEFb_div Post_Div and ref_div *_div Post_Div and ref_div */
	UC*/
/**************ucPowerConnectorStatus;	/OWight ***************rn value 0: detected, 1:not n 10Khz unit */
} SRT Speeck;	RTC nERS_PS_ALLOCA         vice;	/ *otviorId;
	scIfno */
/* uwrB                 l:1;	/*wrBudge used N  SEuch p                 boo of RinULONG ******2				} PO                tion of ATOM_DACx_xxnnector *ineClockTable */
/emReq;	/* FunctiocDacStandard;	_WRITE_I2	        ************************_ENAB*/
/* PS_ALLOCAhaviorId;
	USH:ios d byte**** 0100] as the requested c_DISA structN_PS_ALLOCATIOLLOCATION  DAC_EumTyp/
	USHTTERRAM_tly SpreOWERL connectoWER_CONNECTOR_DETECR_Int. Others:TBD */
	UCHAR uadnector	/* Ratiotions************0:its3:2 SS_STEP_SIZE; bit 6:4VPARAail    TC_OVERSCAN_PARAMEVb	udget;	udget;	/*AMETECHAR u*******1f bfLane<3 SIZE; bit 6:4 SionB********s this Ppll */ 6:4know SDR/DDR ATO;NOCDE6:4 SS_DELAG_STENCA or C or ight */<< 8)when sLAY */6                                 0x04ucSpreadSpmOnPPL
	US********************/
/*  Structtruc_MUL Cr */
it 6:4 .s:TBD 1 */
.
/***************6_fClk*AR ucSpreadSpectant may be obsolete */
/*****6_DOCK usP*****LVDS							Clock;	/*********#define W6*****ByteEnabonization;	rPb						0xturnEngineClock;6GATINDESKTOPion=1., MinLefSpreadSpectrumRange;
	URS_PARAMPARAMETERS_V2;

LLOCATION;

/***************** 6:4 SLOCK+3)
#dbetweSS **/

#ifndef _ATOMBIOSAR ucSprCRITICVO_ENCOD62Ghz, =1: 2.7Ghz */
#e*****/
/*  St6BLOF****BUSYS. */
typedef;
	union {
		UCHAR ucDVOCo6ANK_CRTC_PAR		/rumDelay;
ATOM_DISABLE */
	UCHAR ucSpLAY */RU/* UsT_MEMt _PIef st1=0 Down Spread,=1 CenteECTRUM_O           rnEnTMA								  0x08
#dATOMNRT ulE+3)
#.1 */R_ENGc Tairious /*  SK	    D */
	UCHAR S							OM_DEVICEDRAMETERS_               TROL_ a
 * M_COSoryCLinkConnectt 6:4 entsn 1.ion;eh**** ATOMsew t us;
	/*  ectrumStep;	/=0 Int. nitionucrn oSelstDiTP haryct unTC_Do 10KHz u Table,******ENOCDER_SpL2_REDEFshal/
typedp****ously.h */
gine H_PPLL;

#denge;
	UCHAR ucPpll;****YUV or RGB f SpreRS {PaddiETERS;*****es and constant maRIGHTNESS_CONTROL||ATOM_L************************************Y_PLL_PARAMETERSVITE_I
	USHORT SPREAD_SPECLinkConnecby EnableSpreadSpectrumOn	usVRAMAddress;	/* AdT usOverscAATION;

/************AMETEREL_CLOCK_PS_ALCATION;

/cAction;		/*  0: turn ******************************ARAMETE    SS_ENGI**************/
/*  Structures used by #t this port***********************************AT**/
typedef struct _DIS******************************e SET_CLOCK_FREQ_MADER_INIT Initialize DAC */
} DAC_eadSpectruf encoder */
	/*  1: setup and turn on encoder */
	/*  7: ATOM_ENCODER_fine SET_CLOCK_FREQ_MASR ucSlaveAdETOM_TRANSMIfine SET_CLOCK_FREQ_MAS*/
/* ATuct _der command table definitions ***ne ENABLE_SPRo pace raw EDID */
	UT usOversc         15 */

/***e;	/* if:_HW_I2C_DATA_TYPE6ht */
	USHORT 1;
	UCLE_SPREAD_StrumRange;ETERS {
	ULONG ulTar******RY_PARAMET_PARbytEL_Ms read':currently limit6rscanLeft;	NEEMORrumRange;
	Ux00FF6:4 ':urnEngily l */
6:1;	/000*********ed */
	UCHAR u#define 	/* Atos SW cW co.1 */
*/
#define SPEEDns master leNTION PARAMntentRevir */
_PARAMETace raLVDS_SS_PARAMETEBD             RAMETERS {
	USMEMORY_S;	/* bCO_PARAMETERSAAMETERS;

#dTERS

/*******et;	/* ENABLE_SPREADDeling;
} ENAUSHORT Speeefinitio
	USHORT u/
	UCHAR ucRefDi	UCHAR u***********Tin
 s anATING_PARing;
} Emic Table**********er clS. */
ctor=1, it means truct POWER******limitSSParseDiv;	/*  post divS_ENCODER_CONTROt;
	EN*/
	UCHAR ue ATOM_4inkSel:1;	/* bit2=0: U ENABLE_SPREADTyeMode;
	[2] Link Select: */
	SpreadSpectrumStep;	/*  mitt}
   Bel */fsetID ENABLE_SPREADSte****ed tone TMDSCONTRO1e DE6		/*  ATOM_PPLL1/A_SELFVDn Ta, strGB  =0: DP   enccti***************1/*********2 *_SS_ENEXT_TV_E         T usOversc_RT u} ENABLE_SPRUCHAR ucLineNumber;	Enable singlERS     DISPLAY Link Select: */
	t _GET_MEMORY_CLOCK_PARARAMETERS
#define TMDSCK_FREQ_MASKERS     DISPLAYTROL_PS_A1_ENCODERB888ble;		/* RS     DISPLA_ENCODER_CONTROL_PAR ucSlaveAdMnient */
	UCHA=4: SDVO en1:n seup_MISC_YPrPble,  indryDevicdefine TMDS2_OUTPUT Structure unient */
	UCHA*/
	UCHAR ucActiL_PreadSpeTRDER_MODEHORT TERS

/******NIT	ne ATOM_ENlane ADJUST*  0*****variotMemoryDevice;	/,uc*  BitransiCOMP=     FTWARE.
 */

/**define5uncnclu*/
PARAMETERS {
	UCHCHAR ucRese*/
	/*  USHORT usOver******_PAR*/
	/*  byine A	/* bit4=0: DP connet:ATOHORT uCE3*****

	UCHAR nly used when : Enable spatial di(*****	/*  see PANEL_ENCO************ble,direcuctureDRow */
	UCHAR ucActiETERS {
	ULONG ulTargitheringTERSoFill;	/ */
	UCHAR ucActiT_CONus SW components,l********werCansmittht */
	USHOR 888RGB */
	/*  bitel:1;	/* bit2=0: UucMisc;		/ 888RGB */
	/*  bit1 */
	USHORT ine TMDS=0: Gray level 888RGB */
	/*  bimponents, connec
	USHORT uic Table,  indition */
	ETERS
#define TMDSon;		/*  0: turn 888RGB */
	/*  bMTYPE_DDR3    t0=0
#defurn*        =1: 50FRC_SEL werCTC;		/*  WhiROL_PARAMETERus SW components,lateRate:1;	/*  =0:*****************     =1: 75FRC_SELARAMETERS_V2 {
	US

#define TMDS1_E     =1: 75FRC_B/DUniphyEF */ Struow */
	UCHAR ucActiAMETE betwee_ENCODER_CONTROL_PA1_ENCODER_CONTROL_PS_e TMDS1_ENCOCATIONDIGane Link, */
er */
	us SW components,late4 lane Link, */
*****NIT	3			0AMETERSus SW componen=0: 666RGB */
	/AC_LoadDetectionTableSPDER_CONTROLOLOCA*******CODER_CON
*******/
/*  S */
	/*      =1ST			PI       3: 50F spatial dithering */
	/CHAR ucP      TERS_V2 {
	USHORT  =1: 888RGB */
	UCHStatus;	/  0x08
#defi    =10: Disable temporal dCHAR ucPn table revision =1.3 PS_ALLOCATION_V3  LVDSTROL_PAdefi
	UCHAN_SETUPETERS {
	ULONG ulTargetMemMODE  HAR 0kHz unit; for biosdding;
} ENABLE_erContrvario7Mode */
/* /
	/tor=1, it means mDER_COne ATOM_ENR_CONT     0GAFRC_SEL patter	USHABLE_SPREADus SW _V3ER_CONTROES*/
	/*      =1:RALLOCATION TMDS2_EN_V3 TMDS1_EEXTONTROL_PARAMETE0x00venient */
	UCHA_V3 TMDS1_ELOCK_PDEPTHNSMIT_DIG1_                S_ALLOCATION

#deock ifInfoOCDERFER_CONTROLROL_PARAME2_8BHAR uce */
	UCHAR ucLiODE  L_PARAMETERS_V2

#definL****LanERS;

#definDAC */
S;

#define WRITEefine TMDS1_ENCODEdefine LVD*****RAMETERS
8                                 0x04           ****SPRE                  in theENCOEXl memorNIT	_E FIonly vancoderID */
/* read opSHOR*******ansmitttor=isa3, strudefineus SW compoC_SEL pattern Fbr Disab*/
#defi TMus SW compon*********Padd9                        /*  ATO, stru9SDATABYTE    sed byinkA+varucMisc;		/*  Bi******************* ucSlaveAOCK_PARAMETERS {
****r Disanable Single ********ABENOCDER_SOURCE	/*  Enable or Disa TMDS2dsr bios ;
TOM_TRANSMITTER_BYTE_HW_I2C_DATA_PSTOM_DACx_xxx,XTmTWARE.
 */TOM_TARITE_ONE_BYTE_HW_I2C_DATA_PS Enable si0) *YNACaller doesn't need to init this por_ALLOCAT_EXTERNAL_TMDS_ENCODER_PS_ALLOCATIClock;
	UR_CO       FFSET_IDDATABLOCK    othPREATRING  ATOM_MAX_SIZE_OF_Fucents,larollerace DATACLErControl;	/* Atomic TaansmittER_ACl to iit meaf
	USHORT usOv)else
	UineClockTable */
/** SW c1 or ATOM_PPLrn *  =0:C_SEL pa */
	US need to i/* Major re1: EableContentReviAR ucSON;

/*******************************************iDER_CONTROL_PARAMEGBNK_CRLeft;	/STERSDS_ENCODER_PS_ALLOCA0: Disable temporal do

typedef struct _EXTERNAL_ENCODER_CONTROL_PS_ALLOane Link, */
	L_CLOCK_PARAME4 lane Link, */
	_xxx, igLLOCATION W              ION;

/**********************************OL_PS_ALLONCODER		    werCGra_CONT*********************************ucMisc;		/T*******************/
/*  Structures used by DVOEnc cloGB ( 25FRNG ul****************it */
} SET_MEMORY_CLOCK_Pid if bfL
/******************************* */
	/*      =1:VICE_OUTPUT_*************************_PARAMETERS;

*********                   */
	USE0
#dDER_CONFe */
	/*      =1:            DDR_SETER#end  EnabTERS sDigEncoder;
	WRA_PARRY_TRAINING_PARAMETERS

/****************uROL_PS_ALLOCATIunitpaucTrnth sour/**************************************L_PARAMETEtAY_PLL_PARAMETERS  Enable o   LVDS_ENCODER_CTPUT_COight *C_SEL patt#define DVO_CONFIG_LINKA_B		           AY_CONFIG_BIT								0x08

t****/
	UCHAR ucLineNumber;	/*CHAR ucDP, need toDER_CONTROL_PAo                    0x0_CONTRpreadSpectrumStep;	/*******************/
/*  Structures used by DVOEnc/
	UCHAR ucPHAR ucPaddR_CON**************************************spatial dithering */
	//
typedef ser command table definitions ***y_SET_ENGV2runcate;	/*  bit0=0 Link, */*************************************1_ENCODER_CONTROL_PS_********************/
/*  Structures used by DVOEnc

typedePARAMETERS_V2

#defiintions belon enc********PARAMETER=0: S_ALLOCATION  L*******************/
/*  Structures used by DVOEncnt mode */

/* =====ECHAR ucAction;		/* ATOM_ENABLE/ATOM_DIS*********/
/*  Structures used***********.1 *#define A_ENCODER_CONTROL_PARAMETERS_ TMDS1_ET  LVDS_ENCODER_CONONTROL_PS_ALLOCATDER_CONTROent mode */

/* =====ETERS_LAST

#define TMDS1_ENCO   TMDS1_ENCODR_CCHAR ucAction;		/* ATOM_ENABLE/ATOM_DISAMETERS_LAST ODER_CONTROL_PAROL_PS_ALLOCATION  D_LAST
ERS_LAST    LVDS_ENCODEARAMETERS_LAS to inDS1_ENCODER_CONTROL_PARAMETERS_LAENCODERMETERS_LAST    LVDS_ENCODER_CONTROL_PARAMETERS_V3
#define TMDS    0x00 */
/ntentRevision=3efine ATO coherent mode */

/* ====L_PARAMETERS_LASTHAR ucPadd               CODER_CONTROL_PS_ALLOCATIR_CONTROL_PS_ALLOCATION

/*ER_CONTROL_PARAMETERS_Vne ATOATION;

/****** M_MASTER_LIdefine PANEL_ENC structInit;sHeadercopy oL_CLOCK_PARAMET========*****************_CONFIG_LOW12BIT						TROL_PARAMETERS_V2 RONTROL_PARAMETERS_V3 {
	USHORT usPixelClock;
	UCHAROL_PS_ALLOCATION;
******************R_MODEHER, DAboth memory an2
#anTop;     0x00NCODER_CONTROL_PARAMETERS_LAST

#define TMDS1_ENCOD_CONTROL_PAR****define TMDSCHAR ucAction;		/* ATOM_ENABLE/ATOM_Dnvenien                         ENCODER_I SW      ER_CONTRRAN             _CONTROL_PAENCODCONTROL_PADENCODER_PAPS_ALLOCATION_V3 +1      1
#deerved[3];
} ADJUSures used by DVOE_PS_ALLOCATION_V3  LVDS__ENCODER_MISC_COHEENCODER_CONTROL_PS_A/
	USHORink r01
#dNCODER_PSPATIAL_DI_DEV2 */
} ENABCONFIG_LINKB	_DITHER_EN       0x01
#defineON_V3 ASK       	/*  =1: 
	USHORT Alatest version N_PARA ENABLE_SPREADOnRatitruct _DYNAMICE_MEMORY
#define I:          
	USH0GHZ		  in DCns:
ontrol;	/* Atomic ASIC_Init ER_25FRC_F         OFF   RITE_OoderControl;	/* Atomic TaENCOMacroze DAC */
ncoddiTPUT_CONTROL	Get*****IntoMaUSHOr SW (50ableOrgned, FAtomName)me tDAC_B*)(&DATA_PSMA     HORT ASI##     0 */
/* #CONFIGS *)0)->_PARAMETER-DITHER_E0)/    of(NABLE_tageode;	/*  ble _Iniion *ENCOOL_PION_VumREVI****(ock */
	USHO flag *RAL_DENCODERmory or SetEngineC*)TEMPORAL_DITHER_DEPT-> Atomic Table,  direcAM_P3L_CRTC1				NCODER_P      ****_DATA_TABPARAMET6ARAMETERS
_DITHER_EN   e PAR sHeacable to both mem Dual LiPORAL_DITHER_EN    7Tableous SW componenPARAMETEx80

/********/
/rs_D   MAJ
#defi     ER_EN    ENCODER_75*****************60ct _PIXEL_CVolefin connecINf encoder */
	/*  1: setup andableRGB},
   Bit  _Tious SW_MISC_S ucEncodeM0x3

/* Bit0:_DITHER_EN    cTablelicable to both mem*/
	USHORVDDC             2
#defin */
	UCHARVomponenpedef struct _PIXEL_RT LUT_AutoFill;
#define MEMORY_TRAINING_PS_ALLOCATION Mtput Parameter */ABLE TERNAL_LKSRC_MAee PANEL_EN*****Cected, ,=1 CenteOConGe****heompone_TMDS_EMP	USHORTne MILDN

#definNABLE_	U 1
#LE_EXTERN/* Atomic Table,ryEnginly uNITROLAR ucTableConFTWARE.
 */

OCK    SW componIs SW components,l              LR_CONNotcEncodery_SEL_MASK				  _SDVO_H SW f
}PLL_PHM_PPLL1 orcentag StaVOLT******FsEncode L_PS_ALLOCATIost_ASICARAMEst x0 table      GE_MO_ATSIC_VORAMEE_tern E */
	/_GET_GPIOMASK  stemID;
	USHODR ucSpR
 *licable t0x2

t***********ine ATOMWRI	USHORTPS_ALLOCATION_V3  LVnterIv*****/
	ious SW comp*******_ASI             
#define ATOM_TRANSAR ucTableConerved;	/* may expan**L pale.ctb ******betweeTER_CONFISrc;	/*  ATOM_PJM_TRANSMIT3];	/ctureODER_CONOUTn u_CONTROVENCOinCot*****PCIE le netion;*/
	/*     V_ROM_HESet/PLL2tRANSMITTER_ACLKBLE_ (multipER_CO_CONTROddinVOLTAGE_PARAM 0x_DAC1_CV
=*/
	UCHARrnalo:*****;CDER_MODVol*/
	:  ATOM_DEV**** or UT_COYUV or lLink...****define CRT2_OUTPumRangCONTROL_ifferen po*************cal is RuntimeSeg      1,T_GPIOMASPARRT usProtectedEE asMused byTERS_V2;

tEL_E     NCODER_M       6EeserE_PS_ALLOCATIAMEETERS;
#dS_V2 {
	ufine A1 */
	USHle,inSrc;	/*  ATOM_PJITR_efineest *******Lion;/HDMI/rea byteSEL paE_PS_ALLOCATIOstemID;
	USHOGE_PS_ALLOCATION;

/****variousVOLTAG;	/* Function       BYAR ucHUTO*******/*****00000001
**********offsREQ_MASK TVR_CONTROL_PARAMdirec*************LVDS_SS_PAVolta;
	E_PS_A2TAPALLOHActure is basedITE_ONvenient */ULONG ulReturnMULTI OutNCODER_CONTROL_PARALdirectlyR_CONTROL_PA*******INK_defind by CURR_CO    0x2

tR_CONTROL_0sHWatesHorzG_24Bos****T_ Hardt Par"lTab          ARAMETEDISPLA********.urn off *  [5:4]PCI******DE {
	UCTMDS2_ENC00
#K_SEL_MASK				  . turnage 	/*  =4: SD} TPLAY TMDS1_EN*********TMDS_ENCODER_PS_ALLOSY HY linCE_OUT=0.TRANt isE_HEADER { (mulE_PS_ALLO******2PARAMETERS;

*/

#define ATOM_TRTERS_V2;
2
#dumRangeNCODER_MORese_ACTION_1: livSe the p/HDMI/Se  if bfRAMETE"ATnnecr 2 ( UnALLOCATION**** =====================stemID;
	USHO{
	ON;

/* ==============================OsVOLTAGurn ;	USHORT
_id*
#*/
/*setup a2				    0x0
#define PANEL_ENCOefine PANEL_ENCODER_
#Data Table Portion========GRAP ATOM_TR**CHAR ucTableContentRevisUsed ;TAGE_PAabline An Atombios and omn faRIGHTNESS_COthe terdefine_CRTC_PDIDFr_PARAME
/**** ERS;
RT usProtected =0: turn off ************    ata.m_ALLOCATION  DAC_ENdefine  DYNAMIC_CLOCK_  Offest for the utONG ulTarEMORY_TRAINING_PS_ALLOCATION MEMORY_TRAINING_PARAMETERS

/*************FTWARE.
 */

/*******HORT usHORT ASIIoBaseAddrtecteCLOCK_G	(ATCONTROL_PS_ALLOCATS_ALLOCATION

#
#define ATOM_TRANSMITTdirec,MEMORY_ENGINE_Pn
 *PARAMETTOM"
/*  bit1=0: non-coherentDon't change thi
#define	UTEMP	USHORT
_e;	/*  Offest for the utisical
/***** Bios */
YUVMON_TABLwhenuil10
#definON_PS_AOFF  RANSSMITT#defi: turn off EnablSHORT S_CLOCK_MAHORT Fwer mode */
	USHORT ATOM_Tble t_Unts,AR ucTableContentReviVTMAEncnature to n 8Kb bo   1	  DER_CONOL_PA*******bG_24BT_CONTR            6ine Inf
#defines;
	USsModeeter */
ASK    HAR u oSetting.1 */
R600**********RS
#dSHORT DVOna**************ed by various SW Offs_Infor disallbtain disso****E_OUTPUTEDID_FROM_HWlogTV_Info.1 */
	USHORTXackward_ASWR DENABL#defiTROLpaectly r****R
 *_I	/* Only i*******id*
on 1.3 */
	USTYackward}******      M_DIbeSHORT AnalogTV_Infwer mode */
	USHORTINDIRODER_ATOM_DAC2_PA 0x08 by various SW components,latest verIOservedonly use[25to g} oder */
/*  ATOM_Dersion 1.1 *rn E */
*/
	ENCODER_CONTROL_PARAMETE_TO_rn E */
tageMAN_PARAMETERS ATO;	/* Function Table******ion 1.3 */
	BUS_T_ToNKINGnalMoPARA_  right */
MODE_DVI */
/oder */
/*  Mion 1.1 */
	USvenient */oder */
/*  l;		/*********/
/*  StrucInByteEnabPryCloleOffsetTOM off encododer */
/*  NTTER_ODER (==0======       SW componentstest versionssionateData;	/com|nts,latest verl:1;ne A", WoT_ENffes====eLTROL_PSenSettinge from R600 */
	USHORTtageM Table,direct2.1 Applibtest version us SW componend toserved:1;
	Uav    torOffse Bios *nlyMCdd by Bios *nersion 1irec******mReq_V2 InfCompassionateData;	/E */
	ALLOC,directly ious SW componenfAGE_P SS_Info, change to new name bec	UCHAd by Bios te aligmHORT Anal)
#dTC_PARAM******leFormdRC_SEucTransmixte1 */
	Ue from R600 */
	USHOP MclkSS_Infol:1;X

#deo new name VRAM_UsaP/
	USHORT Table,directly ;	/* onnable d		/*  Defined and used by ogTV_IGB, =SW, ,s and****nlogTV_Incall SS_Info, change to new name bec;	/* Atd by Bioonfig;
	/SW compIOTOM_VE/HDMI/      e ATOM_ENCODER_CONFIG_LEion 1.1 */
	USHORT Corizontal replication */
	UCHAucCRTC;		/*  ATOM_CRTC1 or ATO R_CONTRAious SWONTROL_PARAMETERS     DITVSW components,lat(RefA_ll be obRY_ENOL_PAR		0x80
_BYTE_HW_I2C_D*******0FFFFFcUSHORT D use"     MVDDCTOR ulFbDivabel naONTROL_PARAMETERS     DIT_DAC2_ENTVif
} ATOABLE or ATOM_DISABLE */
	UCHAR ucPadding[3]ct _D/* Atom DYN_ENCODER_CONTPrmat vario2_LI DynAN_PAR	UCHAR 		0x80
efine AANSMITTER_            6TV_FIFOfinitionE */
#defini_STRreCHAR ucMARAMETERS

/directly usedTblstart from 2.1 */
	USHORS_PARAMTV__COMMOSHORT o new nameSh* BiDefined and used by sed by vaHORT DVOsionCE_OUingo new nameNCV*Image  ASIogTV_Infoo */
	USHOoModf bfLaused b com**/
typerious SW later */
	 by vamInfo;nfo" for and /
	USHALLOCATIOT	    2d when cFilter0start from 21 */
	USHORfsionamponents0 coeffici====0.ombios and bssiona1mponents,latest ver on Table,dioCHARion 1.OCATIONrom Bioswill be used V_t ch***********_CONFrbit4=0InfW_I2C_DB							 , it mea ucFracFbPARAME===******Control;	/* Atomic Table,  directly used by various	ULONG ulDefaultEle,in16ne ATOleBuf#definarraon;		/80, ARAMjor revrnble, ER_CONFIGefine ATOMTPARAM:Engrom Bios, nDISABLE   0
#define ATOM_;VEL_4         0x20
#dnd t								2
#definend t*******
#define ATOM_E     =1: 75FRCOM_TRANSMITTER_CONNTROL_Pine MEMORYUTPUT_CONTROL_PARAMETERS ARAMECDERTENER_CO{
	US,y BlankCRTCT ASIarious SW comts,latest v* bicurI/HDMI/HWsion=1,ucBLE   0
#define ATOMILITAND_FO*/
/*  AnR_DISABLE   0ng m_CONT****S {
	USHORT usPrescale/
/* #define ASIC_INT_DACABLE   0
#define ATOM_aFI_BUITo  IndUILDe senabl7	/*  N GP_IO, ImpactTV frequenDedATION sResERSION_MINOlLED   ucSlavTER_AB		gnede ATOM_TRANALLOCATION;

/*********************************C_InitVENDORRIBUTE r bios conv
typede  0x20
#defineo be castedW componentsAdjMG_V2_DPLINKRATDynClO_NONPJ********llsed,tClction;*******ATOt _SEProt ves hW_I2C***********************/      ******* Caller doesn't need to init this ******UTEMsed b multiple of MemATOM_tion;:2bet        **********************************************C        SCJ   d to incluMINGEMENT.I conTSC        ATOM_DAC1_N
	ATOM_COMMON_TABLE_HEAnts,latest versiignature;	/*  MM info tafsl (7) */
	U*********TOM_DAC1_PAL

and vid/HDMI/MMdirecmonHeadsi Enable Encget parser info,Don't chaMON_E_CRTC****F MEthe FI_BUILDhe adapter (4:0) and vid*/
} Aory ucFbDDQ   ble, lnts,late
#definSelect: */
	/*Audio_PARInf***********/
/*  StructuMASK RE

/***********      0x20
#dx10
#define P					HORT MulCHAR ucReseVDDQ    reursof

rious SedI2CSER_CONSHORT DAoard ID dID;	/*  De.sae (1:0)B				0xDER;

/** audio epN sRkA+B *****sle  OEMudioTERS_I bFM audio ipInfo;	/tct, 12 ANYt**** by  chip is         at config ed (6) I2es used bcoITTERlkt ConctID; _PAR (scIfno */
/* ue,  diID	UCHAR SELECT_tructrambling (7) */
	UCHp4:2) SPDIF OuNSMIT1/
/*  Stro3/HDMI/t cha DeCLKSRCystalreturn,ConnectorStatPLL_PcK				 ( componentsHORT ransng (7) *IBUTE Tata/ff/********* cha_PLL_0IID;	/*  Deoefine ENABL
/* ATOM_ENOCDE A audio clk s_ENCO            	
	UCHA StrucefineS [7:0]0_PLL_ ulFbDiv;DS_ENCODERWf du (2)lGoldica be call****p reserv F/B seSAMle.cleCo*******ucVoltagfodefine (7:6onfig 2) physin m;ious SW c,neEX0
#defit1ion 1BEGIN latereserveonfig +R_DETECTION_ctor ID (5:32) physENDATOM_P(:6) */
	UCHAR ucVidicall3) seleturn,(7define:6) */
	UCHAR by sDSEn3ion 1.1 */*  Vid_PLL_ ASIe (1: only used by Bios */
CONTRSK     0HAR ucActi********************/
typedef struct _DIG_EN_PARAMARB_SEQ1:0)_TYPE   20	/* inclMuctuDQ  ts,lTb*  [ucEncoderfDualLLort *******IGC typ off encoder */
	/* TPUT_m Se********************* (1:0) F/BK			R fCD (533) rreservedeo Deco******IA     t chase
	ULONORT u*************ed */
	UCHAR ucP5:3) reserved (7 4 components_4Mx101
#define Atomic T     se	USCa3               components,T_8Capa_CONFy SharecRefLVDS_ENCODEETER =ENCO     ihe nei=0: 666RGB _16, =1:Bios image i0x2=0: PTMDS1_ort *t 1 = 0: Dual 0x2    ios *up32opyrit4:{	WRITE    */

/******sktopsupportet 2/
/*  3xtended Des64Mxi, copy
RCHANTTARTLVDS_SS_PAOdere is not supported4CE_MEMORY_SSAMST MCdwaomic Table,direc*****I**********************/
typedefELP*********/
/*  St is not PostETRLCRTC_SE2
#def from R600, useNAN   2
#deTOM_ENCODER__BUS_MEM_TYNIX1
#define A	USHIN          MOSE various SW comp0xRate:1;	/*WINBO DISHANGS_ALeserved:1;
	UCHASMLLOCATION;
BLE
#d0x_ENCODER_CMICNDine A struSKTOP0xS;

#defineQIMONr ID (5:3) rTOMFOne ATO***********OMt _SEG_STRING

#DQ        /_PPentagASenablinAL

#deDR5NTROuC*****es ue (3	UCHApNTRO64K****OM_MPMODE_ASSS/UTPUT_CONTROL;U1.1, TOM_BCOMMON_TA*****0x1 inst1 ) *ID	PARAMEy vaATURefin0x4375434dle,in'MCuC' -FO_MEMual CE_MEMOROST_WTERS sfig */0**/
rnablc) */PS_ALLOCATION_V3  LVT   OM_Teserv_FLAG_STRING )gnaEST_LLOCATION

/*ctly u PHY linkA+BhNE_Bed iTO_GET_ATdefine A       6
#define ATOM_T************omponensrious SW c#ifndef UH662GHZFTWARE.
 */

/*field structuCAPAB*  I2DIG_ENS_ALLATCATOMC*****trolleadSpeKTOP_SUSHORT bet be callHyper SW co_upport CONTROeInByteP1: U_onen    CONTR	/*  0: turn of#dedian
by ##) F/t, itMODULE_CONTange, when set, itolsBLD_BL_B*******/
/* to bo	VDDQM_TRANSMITTER_CONFIG_VANSMISupporV*****ory selfS_ALLOCATION

#definEMRtypeA_RATIAL_DITH=1t:1;
	USHORT Pse
	UalLinkConnecTPUT	/*Ex       *********VBIO	UCHAR /* Onlyor (bmoryrdRS_V,Info;baITE_rage )=4: Sell wUSHOe fiUCHARr */
	U******ORT Sinst1 ) *t _TV_rChangeinfIRMWARE:4]=0x1:MTYP;=0x2rols2L:1;3	USH3RT W4	USH4;[nfo4;TCMemReq
typeAmChangeUTEMP
#endiontroE */
#es (multi,FO;

/**T LIRicabl********srt:11;
	USH****/vrMemoITTER_CONmory * Cop*******fCONTROefinPUCon0:4MRT W1:8sourc2:16M; WMI32M....el:1;	****x4AG_STRx8RT W
	x16RTBILIx32..  BLANK_CRTC_PAowTI" */
	UDualLinkRow,USHORICE_of 2igned:1;
	UUSHColu* ObsoinAG_STRIAR uTWARE._BIG_ENDIA_H
#defPPORze of
tBank
	ATOM _AO, D_ACCusAcc              ARE_*****} ATOM_FIRMRank ucCRILITY_ACCE  =0: lane 0~3hst ulVideoModignedFTWAREperMnelSS {
	USHORT syble */ower 8USHORT ) */
           ucLink/*able */niphbscan    [4:7]=NEL_Rect _elshoul ucCR1: Dual LiR ucRC type (ners:TBTIONNTagef _H2INC
RNAL_TORT M utility to geSet1;
	USHFFSET_IDDSHORe ATO 0: tRT Mcomponents,on Ta*****aerInfOM_Fock;	/PABILITY_ACC******CondiomponC_INT_DAC						0xCSW compTTER2				 *********Condi strucSC_YPrPb					CLE_ASMemoryClockPLL_Output;	MaxSMITTER_CONcompoutETERle,directly used TION sReDeded ATOM_TRANSMITTER_CONFIG_V_PAR;	/* In _TABLErt:1;
	USHORT PER_A*******ARAMETERS

/ROL_PET_V/ 1.3 */
funMVDDQalitscInf vSELECTl flag to:3STER_L*******
/****sionateData1: UERS_Vle;		2_ENCODHOR(S)*******MITpENCODUT_CONthem */
	Uas thaHz userto set so_GET_GP*_MEMORYimittheN_PARA#define ENABLE 10Khz unit *_PLL_NABLE_AS compLOCATI * Cop2:1;
	Ule,ininEngi:1;
	ENCODER_CY_/
	UCM2******TEMth sMinIn 10Khz unit */
	UMinEn3ulMaxPixelClock3 Portion====Mi */
} GET_MEHORT usMinEngineClockPyClockPLL_Oed whPosted:1;
#else
	areTE_Md:1;
	ize of won 10Khz unit */
	USHORTKhz unit */
LL_Output;	/z unit, Max.Clock;	/* In 1p
	USHORT usMinPixelrol;	/* AtoFIRMWARE_CT usMinPixelCnit */
	USsMaxPixelClockPLL_Inpuef uni) */Supp unit, Max.WMIrMemory_PLL_Output;	P-ADNEL_M_TRANS off enconoweClockPLL_OutpRT HyperMemory_Support:1;
	USHORT PyperMemory_Suze:4;
	USHORT Hthem */
	Ufine ATrn of1;
	tes (multi, BUTEo_CONTRATOM_E********IG2execuTOM_DIG__ATOM_FIRMREeClockPLL_Onnector */
ess;
} A location in ROM in 1Kb_H
#define PM_RTS_StreamSize;	/* RT e ATOM_VEunit, Max.sWARE_

#endifM_RTS_StreamSize;	/* RTS PM4 pLONG ulFirmw */
	UCHAR ucPM_RTS_StreamSize;	/* RTS PM4 packe/* Indicate what is the board design */
	UCHAR ucMemoryMsignedFTWARE.
 */

/*******ation in  type (normal GP_IO, ImpacityInfoTable */L_Output;	se
	ULONyDevice;efaultEngin****/
type */
	ULONG ullMaxPixelClockPLL_Output;	ectly uSC_YPrPb						0x01xMemoryClockPLL_Output;	/* In 10Khz  ulDefaultMemoryClock;	/* In 10Khz unit */
* In 10Khz unit */
	ULONoryClock;	/* In 10Khz unit *//* In 10Khz unit */
	USHORT usMinMemoryClockPLL_10Khz unitn 10Khz unit */xMemoryClockPLL_Output;	/* In 10Khz unitor=1, it mulMaxPixelClockPLL_Output;	/* In 10Kx00000001FaNFO_flag to ind	/*  S(ATOMse In 10Khz uneClockPLK				utransC_BUS      0x20ine ENASHORT	nt on produ#define ATOM_TRAN_PS_A************ternakH00, us  ATOG_V */

MemoryClineClocM_DP_VS_A_TAB************EL_C1;
	USHocessI2****Type ({
	n 10Khz unit usOversASIC_CONTROL_ONTRO1;
	USHORTDR3_MR0;
	};lMaxPixelClockPLL_Out) I2s.
  E       dPLL_Input;	/* IIn 10Khz unit * usMin*****n 1G ulFirmwfClk*FB_CAupportnTER_12_150_7			WG ulASICd by BLClockPLL_Output;	/*tRA00
#defioryC unit */
	USHORiosRuntitR2 AppliRT usMinEFusnit */
	Fhz unit */
	USHOCDR* In 10KhyCATION e Portion===W* In 10Khz KTOP_it */
	USHOP* In 10KhUCHARHORT usMinER0x3ARAMEoryCk;	/* In 10KhzW/* In 10KWNG ulASICMaxEngWT	/* I*****TNG ulASICMaxEngPDIXitdefin.;	/*k;	/* In 10KhzFAngineClocFmode */UCHAR ucLAONnaxPixelCn 10G ulASICMaxEngir defininEn/* In 10KfDEVICExEnle *_4nit *warePulIn 10Khz
	ULcal/* IL_Ou.      ne CRT2_OmponenIn    e PANEL-_HW_I2C16alLinkConne	/* IKhz unit */
	USHO	(AT*****************/
/*n LVTwedBroducSICMaxents} ENAr
#define ENTS_Location;put;	/* In 10Kh	USHORT usOverscaForBIOS[2];kPLL_Input;	 10Khzz unit *vedForT   amSize;	/* RTS PM4 packets in Kb Output */
	ATO/* IryClock;	/* Inponents,called;	/* In 1ule_ID;	/* Inda* In 10Khz unit * usMinEngineClo */
	ULONG ulMaxMemoryClockPLL_Output;	/* In Input;	/* In 10Khz unit */
	USHORT usMinMemoryClockPLL_Outpue boat */
	ULONG ulMaEADER sHeader;
	ULONG ulFirmwareRevisi In 10Khz unit */f struct _ATOM_FIRMWARE_INFO_V1_3 {
	ATt */
	ULONG eClockPLL_Output;	PixelCl Pclk ule_ID;	/* Indic*/
	ULONG ulASEADER sHeader;
	ULONG ulFirmwareRevisionrTargetMemoryClock;	/* In 10Khz unit */
	ULONG ulMaxverTargetMemoryC	/*  y;
	USHOLL_PAffe board design */
	moryCloe:1;	/* biS_StreamSize;	/* RTS PM4 pausse
	ULON, =1:BrmwareCapability;
	USHORT _Inp          st:1;
	USHORT PPMode_Assigned:1;
	UV1_3 /* In 10KhCCDG ulASICUCHAR ucProducRCR PM4 starting location i
	USHORT uCHAR ucProducKNTRORTS_StreamSize;	/* RSRTS PM4 packets in Kb unixEngineCryClock;	/* In 132 PM4 packets in Kb field strul} ENioIn 10Khz unieClock2axPixelClockPLL_OuT12_15nit e Portion====PM_RTS_L/*Chang;gineClock;	/* In 10Khz unASefine ableed    ATION;
	UDllDiseRevisionEnmSize;	/LLSCL2_Rby vaORT us Kb unit */
	UCHAR ucDesign_ID;	/* Indicate G ulASICMaxEngineClock;	ngineClock;	/* In 10Khz unit */
	ULONG ulDefaultMem
	USHORT usMinMemoryCign */
} ATOM_FmwareInfo;	/* Note co tablcusMa usMinEnconveOM_FIRMWARE_INFO_V1_3 {
	Avision;
	ULONG ulDefaultEngineClock;	/* In 10Khz unit */
	ULONG ulDeign */
} ATOM_FIR3ClockPONG ONG ulDefaultMemoryClock;n 10Khz unit */
	ULONG ulMaxMemoryClockPLL_OutpuHORT usMinMemoryClone b3F==========sabove coinMemoHeade!!TOM",ets in Kb unit */
	UCHAR nit */
	ULONG ulASICMaxEios ode */
#defiress;	/* verTargetEngineClock;	/* In 1 unit */
	USHORT usMinEn	USHOTS PM4 DCErnfig 	/*Chan what is the board design */
	UCHAR ucMemoryModule_ID;	/* Indicate what is the board design */
} ATOM_FIRMWARE_INFO;

typedef struct _ATOM_FIRMWARE_INFO_V1_2 {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ULONBurst I2S Audiobnge a
	UC, 0=****ersiobo=4MODERT IndiUTEMP_PARAMLE     
able, Biet;
	USHTROL_PS_o    InVOLTAG/DponentsVoltageort *(ble,ign */it */
typedef EXPREngineClock;	/* In 10Khz unART usMaxMemormponenRUSMITTE*********moltagAGE_PARAMDW_I2tyG ulDrERS_V2,ted;RE_IemoryCl16,Input;rol;  Tun		/******amhis o1;
	U:4]d to  f	ULONG u,it */
VI/HDts in Kb bit of ulMinPixev to bht */
	T usMinODER_CONTRObMETERlike RDBI/WDBI etfine SEsuppation in ROM in 1Kb LaNIT	E_HEAD[	/*  TRANthe b1 */
	UMODE_SEED_BYof ROILITYvalue */
fo;	NG ulMaxP6)  nono sMemoryClockPLeClockPLL_Input;	/* In 1S PM4 SW comp3	USHORT usMaable */Map1Kb unit boy foSel */A+B       oryC:*/
	ULONmoryClockPLITTER_CONTROL_PTART|| */nector=0. whenMemoryClockPLL_;	/* Bit1=0 Downunit */
	neClockPLL_Output;	/* In CHAoduc*/
	ULONG*  0: tue DEC30) */
/*eClockPLMVixelClockPLL_eRevisLcd10Khz unit */
	USHORT usMinPMaxPix, bIOutControlTab*********Khz unit, Max.  Pclk */
	USHORT usMinPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxPixelClockPLL_Input;	/* In 10Khz ormal GP_IO, Imp	/*  In MHz unit */
	ULONGsion;
	ULONG ulDefaultEngineClock;	/* I=1: 8w com/*  In MHz unit */
	ULONG{
	USHO typBLE_e the positioREFI**/
/*  gineClock;	NG ulMaxPixelor Dne TINT +16ectoro -14ectoL_MASK				  0PL_RTHORT usReferenceClo usMinPixelClocPL rcompotrip d comeMode */
/* #M_FIRMulComputeocessI2cn 10Khz uINKA_B					0x00
#define arting location in le spatial ze;	/* RTS PM4 packets in Kb EngineClock;	/* In  */
	orLONG uld in oad fromfPS_ALLO1;
	USHn10Khz unit Function TatrolL_PARAMETERSemoryClockPLLtrolleETERS;
y use comVolnly by  uMODE_SOURCE_B _CAPdual lire DECore DEC30)t 0 Type (BA sinY_ODIRMWARE_CAPABILITY0xc**/
/*  *******BPixelClockPLATOM_PPe board designMaxEngineClock;	/* In 10Khz unit */
4k;	/* In 10Khz unit */
getEngineClock;	/* Inding[2];	 SW cTS PM4 starting location in ROM iMefineOutput;	/*RT usMAMETERS;
*/
	ATOM_FI4, makew naignedRANSMI

type *ooULONG n+7)
PARAMEr     w unit *DDCI_Info" forPARAe vewareInfo;	/* at isDE_DP	tr*******onlspHORTevispoceszALLOdre use,]PCIE lanETERSiniphyi_PARAMETERS10Khzupporif_Outpanus SWnector
typedeREPLIRAMCFG***** by B NOOFBANK,rateRANKSlingInOWonnectCOLSlock;	/T usMaxPixelClock;	/* In 10Khz unit, Max.  Pclk */
	USHORT usMinPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMaxPixelClockPLL_Input;	/* In 10Khz unit */
	USHORT usMinPixelClockPLL_Output;	/* In 10Khz unit, er;
lClo5nit */
-lowedBL_Les ulower 16bit of ulMinPG ulFirmwareRevision;
	ULONG ulDedef structidefi*****put;	cwer 8FTWARE.
 */

/*************_ALLOMic32putes; 1 - ulMndext */
	ULONG ulDriverTargetEngineClock;	/* In 10Khz unit */
	ULONG ulDrivlDriverTargetMemoryClock;	/* In 10Khz unit */
	ULONG ulMaxEngineClockP: 666RGB */
**/
/*  Str:nit */dual lord)_CLONGing;dHi usM* Out2yclesIesignl_ENABL4ATOM0CHAR ctly .!=O */
    _ID;	;
	USHORT usReferenceCloKb in Kb unit* In 10Khz unit */
	ULONG ulDefaultMemoryClock;	/* In 10Khz unit */
	ULONG ulDriverTargetEngineClock;	/* In 10Khz unit */
	ULONG u* RTS PM4 pacryClock;	/* In 10Khz unit */
	ULONG ulMaxEngineClockPLL_Ou SS chip is CONTRLAST

_HEADER sHeader;
	 */
PUT_Cencoder */
	/*  1: setup and turn on encoder */
	/*  7: ATOM_ENCODER_INIT Initialt */
oryCloc- T DynaUCHAR ucBl6MBLFTE  4:0) a0x00
#de-ts i[2he de10Kh 10Khz ucMemo0040
#ck;	/* I ulDiphyby PUCHARV3,ADER lble, lar definbyf theE_DP	t;	/* In 10Khz uni(    RT unine 4 Comp        FbDivLL_Input;	/* In 10Khz unit */
	USHORT usMaxMemoryClockPLL_In ulFirmwareRevision;
	ULONG ulDefaultEngineClock;	/* In 10KhClock;	/* In 10Khz unit */
	ULONG ulDriverTargetEngineClock;	/* In 10Khz unit */
	ULONG ulDriverTargetMemoryClock;	/* In 10Khz unit */
	ULONG ulMaxEngineClockPLL_OuerMemory_Support:1;
	USHOSS usFirmwareCapability;
	USHORT usReferenceClock;	/* In 10Khz unit */hz unit */
	ULONG ulDe[1 */
mlGoldenthe b (00=8ms, 01=16Time10=32ms,11=64mslock;	/* In 10Khhe boar_CONFPS_ALLn 10Khz unit */
	ULONG ulMaxPixelClockPLL_Output;	/* In 10Khz unit */
	ULONG ulASICMaxEngineClock;	/* In 10Kin RDE_DPModulersion 1.1 *M_INTEGRATED_tion;	****: detected, 1:isplayPllT
1: P/2 moize;	/* RTnSK  ****define ATOM_BIOS_INUMA system memory clrameter */rMemory_SEXT_TV_EN****AMD IGP,PLL_P0ablesode;st Confi/E_DP	onnevariousr								0xd by-ock
 Cersion 1nesion=sMaxPixelClockPLL_Inputlock;	/* In 5uc
	USHORStreamy Bios whSHORT uspr tyts chaReserved1;
	UHAR ucDPT Ind_eo Decoge autput;	/* sign */
}rambFIRMWARE_Iation in RNTEGRATED_DVO_     For AMD IGP,for now this can be 0
ulMs the board desiabilityFlPPORT:;	/* In 10de or otherwisnit */
	USHORT usCapabilityFlag;	/* Bit0=1 indicates the fake HDMI support,Bit1=0/1 for Dynamic clocking dis/enablIC_VDDCI        res ntegemIndofilingInf**************************************************************/
typedef struct _DAC_LOAD mode) iIGPed frine A**************        ;	/*y:     For AMD IGP onAC_CAR********/
/*  StMVDDy:     For AMD IGP onPLICermark calculation0xHeader   For AMD IGP onPOSTISPLBY_2       4        FTWARE.
 */

/******* ATOG*/
	DemoryCler;
	ULONG ulFirmwareRevision;
	ULONG ulDefaultEnginucMemo ulDefaultMemorye index, bit[/* In 10Khz u system memory clalue for the max voltage.Set thMaxofilin to 0xFF if VC without PWM. Set this to 0x0inf no VC at all.
ucMinNBVoltage:         VORT usM _LVDSOfC/* =0:nPhat CDTiminassistedCABLE:  SION;

/= this timing d0
/* MrsionPE_PCI ic Tabp ) *C_BUSe,  * Wreal volESS usFirm =0: lisSubsystemVenus SNBPS_ALLOCowused b:1;
ectlydincluPMW    0x00FFvisiohe ucVoltage Portion====ty is 100%. loM_IC bits oTOM_Df the value.
ucNumberOfCyclesInPeriodHi: Indicatn 10KHz ulag to ind	/*  =3: HusFSB/* RTS PM4 paeraticStartDelay;	/* i, =1:Bios lag;	/* Orey =1s SW =1: ign */fmetector=
 * Coponly =0/1ector=**/
tyMemoryucNumis/nable F used bylock:2]=t 1 NoODER_een ;
	UCACthis to2:PLICAhis 0xFF if VC w4]= lane/2a Tab, =nkA+B/1a Table Portion====DER_NBCfgRegSize oPPLLsion=MUX_Sele PPLL9****alue Eine SET2e PPLL 0]=rn o_Rock;	aPARAMETSHpedef struct _ATOM_FIRMWARE_INFO_V1_3 {
	ATOM_COMMON_TABLE_HEADER sHeader;
	ULONG ulFirmwareRevision;
	ULONG ulDefaultEngineClock;	/* In 10Khz unit */
	ULONG ulDefvalue for the min voltag******G.FVTHROer bM_CENDEREG0.lue for the min voltagal voltage levM_RTS_Lat aM_ICUCHAut PWM or no VC at all.
usInte dependent PWM valueding[2];	/* DHT0: PF_ATOnit *16NOCDEvsT InDeptetingxtangetdablencoderTOM_ */

/USHORT */
;	/* Dit */
	USHORT usOCK_PAUSHO****nt donit */
	USHORT usMwe       oy SPD if nd/or ot-up  =0: lane 0~3DR_Bandshoulate how0:3sIntadefinebPM_RTs VRIGHT7ISPLnit *//* RsPM_RTs Vtput;	/* In 10Khz unit */
	ULOput;on in ROMcles when PWM ASYNC morol
ucMaxNBVoltage:       0xExplanChange TROLsetMe forget whatever th50Khz unit */
	ULONG ulASICMax********e */
	UCHAR ucLineNumber;	2_70GHZ		0x01
#d6RGBdefineRAMsign *Coherent 0
         a;	/*PHY */
	/*  =1HORT usMinPixelClockP _ATOM_MULTIMEDIA
/*********nvert1: Dual Li0 */
	U patterion USHORotechoululd allow us to *******ernal mem*****/

/s.:Engiock;;

typinver********************/
typedef struct _DIG_ENMInput;#d**********C witf  the ***/
/* **************************MITmSize;	it */
	trolis coHARs us#dgineClockwill be used MDSClk******T usSbelock;foinSirdefine Port memoage.Set this oneUMAthe value for tICE_OUT in 10kHz upSide****the value foRers/*  irmwarePoaVn 10ins****/
DIG_TRA********METERS_Vimum+10Khz **********************B_SYSTEM_INiOM_E
rom R600, used_PS_aOM_VERPeriIGP SHORT direcrGATING_PARA, sublie ASIwayle    Defi
ory and engine clockget whatever they need.
The_V************************em*****varilM0
#de**********/
/ge reguid;	/* in 10kHz unit *it */
.Set tPinCFGion 1 inverad1amSize; */
	PostDormal GP_IO, n */TOM_DIG_ENCOD unit */
	ULDE_T    */
/* ATlue for tLE_HEADER sHeader;
	ULONG ulBootUp/* IM the value for the max voltage.Set thny cyclesamSize;	*/
	ULONG ulMiePortClock;	/* in 10kHz unit */
	ULONG ulReserved2[he max voltage.Set this onefor the reserved */
oltage:         Voltage reguHORT usLinkStatusZeroTime;
	USHORT usReservset bit 2[6TLinkWidth;
	USHORT usMaxHTLinkWidth;
	USHORT uofilinCos 0
 DQ7_0ATIARemaHeader;
QSHORT Sefin OutpPS_A: 10Khz un*****/
	UtPS 0E */
	MinU1, =2am NB 2, =3th;
	U***/
t	oltageHighth;
	Uittage.cate hCHAR Revision( 7~0)lock;3SHORaxPixe: DQ0=B.
uc:0], DQ1:[5:3], _ID;DQ7=0: P2d byInualLinkConnecto*******ayVMETERefaultEnginBIOS_omponenHORT  invert*/
tulMODElot1AMETERS_V3unit */
MA Clo2AMETERS_V3;
#defiemoryCt. Others[Memo=trolsBL:2rols2;=3rols3.PIXEL_isClock;	/* in 10ORT usMULONTOM_CO _LVDS_           ckingPinCBed by Memory is not pre        Khz unit */
ckingPinCFGInfo;
	ULONGthe tPUCa(2) pode */
#defilue for emorz unit */
	ULONG ulASICMax1_ENCODERSK    y various SW components,calledmponents,latest ver24bit */
O, ne reservanneed passige.Set this oneReqomponallow us to n-SW coExpmitta Tab	/* RTS PM4 starting location internal mmode;
Bit[1]=1: system boots up at AMD overdrived* Arai			 Lo* Bit2MaxMemoryClock;	/* It;	/* in 10kHz unit *	/* Inng (7) venient 0Khz unit */
	USFirmw0:ULONGidePo SW cois boot ENABLEre usE_CRTC;	/*  real voltage le In 10Kh           1

NCODER		    at****
#define  Dynommon ponnecDCE3e(PerE_PCee o) he min vol componal mem.
VC wdefine WRITE_ONed and enabled on4Bit[3CLMBit[3]=1: Only on m.
    axNB
Bit[4]=1: CLMApplicifnde_ALLOCATI            2             1

 SW compon n encSHORT . S Bit Cloc
 * Copyriand
Bit[5]=1turnEngin5]=1: E*****
Bit[5]=1: Enable  used by various SW components,lANY Td for r  ATO_DIT             ST_STOP			 this one efineHT wiNON_LLOCATIO************ If CLMCGALLOCATION;case, Min HT width wdn 10Khz uni, etc) */
	T width w_ALLOCATION;neClock;	/*define CLMsed to enabLLOC(5:3) r
Bit[6]=1:
	UCHn Bytes (in Size oon power statate how In 10KoFill;	/
Bit[6]=1:ZER*********l, bue */
a****neCloON successefulrStatW for all drived by ,directly utage drop/thrott    =1 by Bios qC_PStod. If CLMCre usOM_IC ****ividedop/throT wiansmittOleOffsee alockedled on6]re usO0x10
#defous SW comt
	UCHAdrop/CLOSon 1.1FO_MEMORY_ectlyed bb****wer10x04ed for ssue.
      =0: Enable CLMC ased wATOM_MULTIVO                                                inkSel    PRODUUp;	/V ATOM_P LLOCAT '01.00'tOine ****t _DYNLETempted TERS rameter spis to 0xBBne MEMabling VBEity;
	p.volt incTYe */
	UCHriver wie Clock FIRST_PPL2ucMictio or ATOM_PNCODER_CONF,thisplay be ous SW compoon 1.put;mponenClocLCD, PTR_3* #decompURT u0Action;		/* TYPE             ts,lorissio} _COMariou+3)
#n 10KhLVDS_SS_PARAMETERS selection;UL_PSUSHO selection; This is 	upTV s           NTSC  tMemzeiA			 r AMD IGP,fohe s
Bit[3]=1: Only on VB
#de* #defin	UCHAUPstat*************Vb****t _SET ulDef_75F****be/*  bit0=0dStd definition;tOeuse,s,lPtn 10Khz uns the ONFDataulDefconnector (Mobileower OM_CR         cife */
	/* Bit
}icPwrTable. HighSet tHAR fcon

p UMA Clock in 1VBIOs2_0s (bit 0=1 lane 3:0;  ucAit 2ssis (bit 0=1 lane 3:0;  ucFrac(==1)ixelClock;umbeofe,  orG inst1 ) *(Mobilut;	usedUL_SELELITY,e same DDI slot/conn_MEMORYons toet thDCE3; bi(le *nt PHAR f3:0;faul SW se, ha1 lane 111:8ARAMET31 lanLVDS_SS_PARAMETERSane VER CD ) he syas:
  215:12)
			[15:8] - Lane n2)
		5:12nELEC fo;
     )_ALLPIXEL_ -p is ector iane , WIssue.
      lane eOM_DAC2_M:Mebit 1=1 lane 7:4; bit 2 (bit 0=1 16]-L_PSNNECTOR_OBJEC1U06 *axNBV****HTel *//freqLONG ulASI2OR |  compoO* In 10K;	/*  NE_A
 4]- Reserbit 1=1 lane 7:4; bit 2Fon;	/* 
	Umode) it'

#define OFFmponents,DDI slFPn informationRedBPP*****_ID_Green	/* ideort *Blu    s not ins. 8NOCDEed. DriTABLOCs   4

ct */
} GLLOC:ER sHeaderleBuporEmECTOR_OBMemode, ==0:        g: S ASIC_Bbit 1=1 lane 7:4; biOS and DrHORT usFbDiv;	fine nd PWMyInfoRT usB;	/kingPinA or it *     m Bios, needg loVoltagu******** ATOM/lled from# dwvice/F?vice/Fuse dUSHa Byt:0]-Bvector defiWinA:ng Pin; [us/ons toFunction # t****bCFGble  (Bey C1Ewindow Aet thPin; [31:   CregKhz BeegacyPinBit:     whic pin
y is not presenEL_ENCunterID it B this register toDDI slWinGranRANSM* RTS PMead this pin
ucDocostatus;
ucDockingn when gmission=1:ry clock
EL_ENCit */
	USHOn; the v2:DetehvarimberOfC3:K8, s 100%bits ithou now and must A the syGrest vmberOfCyclesInPeriod:Indicate how many cycSICMall.sthe syRANSMPWM duty iB 100%.
 ulDr 100%. lo:xelClucVoltagON_ENAlANGE_CLc;		ithBr.
usME OR
 *a Tab.
	UCHAWin comt 3=0) Hlue for the min voltdPinBit:     whicz; it Dockormat *****n when  In 10Khmode.
usMinN**/
/*  Sg "$*/
	th usMaxNBVoltage & usMiod:For AMD IN  SE struct} Ele,  rwiseERS_V2 ; _LVDS:ION  SET_Cwer 1nelsD_BLO{
	u10KhzIG_LINt;	/*  axPXResoLOCA**********************PUterndoro DinBit:     whicut the firmrIOMA=1
_ON_t _****n Int1
#dac unit */
	DDI slYPIOMAsMinN*******************U SWt control mode: usdistinguis & Indic 100%. lo=0tor Inween the SOCK_er toXOUTP0
#define PkingPinPolarity:Polarity of the pidependent c up ahe nege regulat */
	ULO: Yul NB voFreqEL_ENCODucMeup         idthSIC_clocClockPMin. in NB OM_TRANhEL_EW disabminNG uE_SOURDatalantus;
ucDockingPinPolarityPolarity of the pi1: Dual LiPLL_Outp is b       If CBitxplaAL_LEVth upsASYNCtor Idownp maximw_ATOMbyte al In t & uregul#define f CDLURPOFracAcn settup maximum HT link width. If CDLW dibversiosambvolt       If Cset itg lo          ax If Cled, thih. If.l to usMidis when PWMel=0. when _PARAMEinkWiONG uth usMaxNBVoltage & usg bootup.

usUMASysankwithout Parameter  to usMinHTLNTSC  agG to read this pint presW enabled, both upstiPARAMum HT link eeds thiFo is is<3 1
       gPin:0=0:ETime v***********ODER_VoltageER_CO R_CO Dirol;	Co
	UCHAtoms(TROLirMemoryCdt a p/6 for YUV/7ynr depemoryCe.Set this toD  = SW c width. If CDLW dink Frequency in 10       eadSpu cp		/*LL_Int up
	USge.Set this onRedW ena				3	/* of R1.1 */l.

uctu:SYSTEM_CONFIGBLE_0
          lsbM_MULTPUCo0b,p minimum eCloco_CONTRr [3:0]*0.1us (0.0 to 1.5us)
          0x01 f T0T (0.0[gDE_D [5:4]onfiS_ALL lin read usLinUCHA=         Memo*0.1us (0.0_SET_.5usios 0.5us (0.0 to 7.5us)
     will retire yISlot1Collu*/
	tatusZeroTime=T0Ttime [3:0]*0.5us (0.0 to 7.5us)
          bte h     if T0Ttime [5:4]=1 [5:hen usLinkStatusZeroTime=T0Ttime [3:0]*2.0us (0.0 to 30us)
          usLinkStatme [5:4]=1ZsvSHORtatusZeroTime=T0Ttime [3:0]*0.5us (0.0 to 7.5us)
          5:*******     if T0Ttime [5:4]=1Zsvthen usLinkStatusZeroTime=T0Ttime [3:0]*2.0us (0.0 to 30us)
          imisWidth;
	U OF inimum Supp poime   VidoutZeroTime=T0Ttime [3:0]*0.5us (0.0 )
           ximum ss register CPU, sa timinANGE_CLORT usMaxUane .S2.0 for K8 G_USE_PWrNGINEhysmast:en setn. voltage oltage regu 0.
   GoldeHAR rk cATOM_ICMaHighmSize;	OM_DP_VS_*****than    eeeds thiunit */
when ink width.

0040
#deice of a se- Table,dd by B_0s thanwrBu_VN OF C, terTargeNis one *otupNoTS PMUCHAR nowMin. voODER ASYNCHTL 0.
                             Thi3ency for pt1ConfanDDI slLinkWidthGPIOMA=0.0us (0.0 to 30: UnlHighVoltageHTLinusMa-up voltage r1=1 def  usM  inge=T0Ttime [nkime atequa,ne FIi    V stwaectl  If CDLW enabled, both upstethen u1=1 ataR	/* In WERLAG_STR_ELinLE               0x00000002 all, to se0x0 for K8 G_RUN_ATCAN_PDRIVE_GE        ;	/*     0x0000****t up runs in HT1, thisust be 0.
                             The [5:4(04
#define Slock;	/* In GAMETFhen usLinkStatusZeroTime=T).
ulLowVoltageHTLinkFreq:      HT link fre_CONFIwer 16bi********/
/*  Structhan or equal tOVERDRIsLinkatusZeroTi5e=T0Ttime [7:0]*2.0us (0.0 to 30us)
  e:1;	/*  =0:N_VOLTAGE        HIGHMETERS_LASTARAMETERS
OLTAGE         usM00020
#define SYSTEM_CONFIG_;	/*  TMC_HYBRIDSK    0020
#define SYSTONFIG_11b,me:Me         0UConime [0xaless than or equal tREQUESTED   imeFHAR ucPaddb0or ADDI_SLOT10Khz M00000080

#define IGP_DDI_SLOT_LANE_CONFIG_MASK                         0
#defAPne SET_VOLTAGE_TYPEVO_ENd by up COMallowHT1 if is/
	ULONGMax HTOT_CONFIG_LANE_8_11      tion;*/
	ULON   0xFER_CONTR to 4_RS_V3
#defiXT_TV_ENCOD#define IGP_DDI_SLOT_LANE_CONFIG_MASK                    define CRT1_OUTPUP_MASK          E_ENA 0x00
assionateDate usMaxHTLinkWid ulDrUpStrz; it m C1E.ol;	/*IA_COHz)00000glowedBision/* RTS PM4 paemory end      _MASK         190 dup (0EXPRE}t;	/* In 104]-idePort mreReNBVoltOLTAGE   #defLONG Re***********1
#def#define FUNSK    lse
 the n usLAlationATMemone ATOM     0x0OM_DEVI0FF****
#define ATOM_PS_ALLO1c Taeserved portion fORT VESA_ToInDVO_ENX      DVONC
#ifnd_QUERYrumPercentagD				3mDelay;
	UCM_TV_NTER_TINDEX     NDEeadSpectruHAR uTV_ENCODER_ID 000002
#define ATOM_DFP_          0x0               ATOM_TV_INT_ENion 1 efine readSpectrumB* Atomi#define ATOM_DFP_T_EN  0x2DTERS {
	USHORT usPassionateDATOM2TV_INT_EN	    0xBUGby BONFIG_LANE_12_0	UCHAR uansmitt             0xo conv  only used by Bomponents,latest0ST_STOP			 6
#dcEnablALLOCATION;
T0x1Rate:1;	/*  =0:efine ATOeContentRevisiontherinine D izeInByt, see belATOM_TV_INT_ENHORT DAor:1;	/* bi     8venient */
	UCHATOM_TV_INT_ENOL*********XT_TV_ENCODx8RTC;		/*  Which Cx000000029RAMETERS               re:8lBootU7:SHORT usRefDiv;	/*  ucTabRea	/*  Only used bAG_USE_PWM_ON_V             0xble  version 1ne ATO0x8          SUB_TV_INT_ENlane 
	UC
	/*  MORY_PARAMETLMC dised in t ver      8nk wid        0x00000002	UCHA    ETERS;
#deVICE_OUTPU0x8LEO_OUTPUTenabled.

ulBootUpReqDisplC1_ENCST_STOP			 6
#SC    Fernalfine ATd ( change to C2
#define ATOM_DFP_efine     C_OverScan0x        DAC1_ENCODER_ID							MORY_STERS {
	USHORT us8               0x0F
/*  defiPS_AL_ALLOCATION  LusS_ENC
#define ATO
/*******Atomic Table,  dirange clock       LI_ID														0x7R_ID														0xate hANEL_ENCODER_TR            Spread. Bit1=NOMASKEL0x14		0x02
NotiLE_O llSystsMaxPixelCl_IconCurxplanati;

typede							0x09
#defin*/
	USHORT Multfine ATS_ALe ATOM_ENCOONFIG5   0xHORT usOncocERS
llow be w_PARAMETERS     Donents		0
#define ATOM_DIGITA_LOW     5R_ID														0x EMORYID													0x09
#definID		eredn _ATODFFORT LVDS_IATOM_0x890000
#define ATOM_DEV_ALLOCATION;

/***_DVO_ENCODER_*****M_ADble,directly 0x9 ATOM_DIGITALL_ENCODER		at At memission is here_CONTROL_P    INDEX         30x00DER1_r the neClock;FTOM_ENMD IGPncoder ***********x0000000C
#************************************* L:DEVICE_CRT2_INDle teCD1_INDEX                     _INDEX                 d':cur_ENCODER_ID								n inF0000

#definIC_VDDC   _LCD2_IND              G*/
/* #by varH E+5)
#def */
}  */
	Uze DAC */
} DAC_EN               0x000LOCATBINDEX          0acType;05
#define ATO      *******            0x00000002ccess late*/
#deSP ASIORT    ;
	WRIINDEX                      0000_LVTCE_LCD2_INDEX                   NDEX	omponents,latenPixelC										0x00000009
#defi    16PARAMETERS;NDEX														0x000REDUE_HW_ILLOCATIONx0000000D

/*  define ASIC ASIfine ATO (NOTby various XPREOM_LCD_INT_ENCODER1RETUROM_DFP,latest version 1.1, uct _LVDS_ENC WriHORT P       GORT
		ne DVO_ENCOD ATOM_BIG/* #define ASIC_EXT_TV_ENCOD0vectoy of  DIG1EncoderControl;	/* Atomiz uniUSHOR              D IGP,for nit[3th. If R sHOuank      ed by various SW compo comOM_FIRMCapa* Caller does validne AmicomplMinSHORT DVOOutputControck:        For AS_ALLOCATOM_mdT*****used for connectz unit */
	U	/*  hmoryCloe Sel */
	t _Les;	/ ho(L_Input;	lock;	/* In 10Oy Set              *******RES2ndEDFTV_INT+1PUT_atiaV
#define      2		/* O      nPeriod_PARAMETER=1ockGFAKE_DESKTOP2
#defin

ulDDISlot1Config:W com#else
	US_PARAMETERC_F            erControl;	/* Armark cuty is
	/*  =;	/*  hION 2
#defmemory ****PS_ALIND_ENCODER_CONTROL_PS_ALLOCATI */
{            ****************/
typedef struct _DIG_ptrfine ATOM_MEDIDFry Bios wppdateCRA
       _FAKE_DESKTOP2
#definea_fine ATOM_Menabl/*  Str*******1_INDEX)Ea      (0fine ATOMARAMETERS   memory iectorStatsp/* In Priod foR_CONPLICATION_PARAMETERS {
	RVEDD_INNFIG_24BRIO   1T                          (0x1L << ATOM_DEVICE_DFP      {
 <<TOM_ENABLE+ARAMETERS     Ddef s2ort memory in PM_ENCPro7) *Auxhz unit#defin

/on
#define ATOM_herwi_PS_ALLR ) */(AUXansmittSSS_SMASS F0
#deAR ucTableContentRelpAuxfine ENixelClock;lp F/B u#define ATOM_ble */DivSrcULONG ul3DAc componplck

ue. sissu
typedePPMode_xEngineClockl;	/* Ate    (ATOM_EN2				    0x08
CV              ODER_25FRC_F           0CDLW for al< ATOM_DEVICE_DFP4_INDEX )
#definstemID;
	USHVO_ENATOM_DEVICE_DFP4_INDEX )
#define ATOM_DESC    etSDEVI2/TVOUTdefine ATOM_DEVIC]=Size of wN_BU      from R600 */
	USHORT INKksionatDEVICE_CRT2(0x1LdefiIn 10Khz unidefine Ang (7) 	USHORObsverTarine ATOM_x0000000DNABL      Tz */KG ulDrRT | \
	0x000EngineClock/MVDDQ ERSCAN_PS_A      DF used by va_BUSASICMaxMemoryClock;	/OM_CRine A        E_TV1_SUPPORT         Ke ATOP5_DEVICEsplayVHAR ucPPARAME      ne ATOP4 Int. Others:TBD */
	sed emory_S\
	(thodClocuOM_IC           FlReserv_INDEX)
#dRT  | ATOM_3   TWARE.
 2_OUTPUT_CDE_CV				XSIZEne SET_VOLTAGE_TYPPeCapat tim1: 888R_TMDS_Eon=1,u66_LCD2_SUPPS_ALVSW======EEMPC_SuppoTINDEX          _LCD2_SUPP    efine ATOMefine ATOVGmponents,latest veD2_SUPPdefincomponeION 2
#define ATOM7ATOM_DEVower 8NECTOR_TYPE_MAICE_LCx0 if nORT  | ATOLMC as  ucMiscIn1
#define ATODVIdefine DTER3v. If booLITY_ACCES***************x00001
#defin2 ATOM_DER_CONTROL_PLVDS_SS_PARAMET_DEVICE_CO 0FRC_SELe ATOM_DEVICEine DVO_ENCODER_COEX					4
#KB orE_RESERVEDD_IN          0x000000024
ine ATO888RGAe    ,  com/M/
	UCHAR ucEna00000006
#d_COMPOe piE_CONNECTORe requestedUPPORT  | ATOM_DEVIC*******ine AIND	tageMdefinHTLiontentReATOM0
#d */
	T   ,  TOM_nternal memediaConfigInRT1_I var  16

#deoInfo;	/WS_sed byT1memory paR		If Cour internal memory pa           0x00000ONG s casfine ATOHDMCONNECTIT_SEL  D IG6RGB, =eClock; ATOM80x10
#defin          utpu0000001
#defin001
#define ATOYPE_B                 0x000001erAL      0x1LNDEX		ne ATOM_DEVPATIA01               HYPE_B                 0x00000e400001
#define ATONABLE_YEVICE_DFP4_ATOM_TV_PA         (A_TV1_SUPPORT    K     ECTO3200001
#define AOM_DEV        0x00000**** */
#define ASIC_INT_DAC1_ENCOERAMETERS    4/
typedef        NDEXLONG CODER_dr;	    _SHIFT       */
NOFIG_V2_DPLINK0x00000001
#d800001
#defin      UX*****efine AditioO    OM_TV_NTSC M_DEVICE_DAC_INFO_SHIFT      6efine ATOM_DACA           DAC
#define ATOM         0x00000002

VICE_DAC_INFO_SHIFFOne SET_VOLTCA           Encode_DEVICE_DAC_INFO_NODAC         TOM_DEVICE_I2C_ID_NI_SLOT_CONFIG_******************SIC_INT_DAC1_ENCO***********NNECTOR_TYPEETERID_N7000001
#defin******************eClock      0x000000ne UN           0x00000NDEX				   0CE_DAC_INFO_SHIFT ATOM_D=0:T        NDEX )
#define ATOM_DEP1_SUine ATOM_ESpelue. M_DEVICE_DFP1_SUPPATTOM_FIRMFP3TV_INT)ode) it's EngierControby SetCECTOR_TYPEM_USEble */
/*     OM_DEcoderIERSCAN_PS_Afrom SetMemM_CRTC1 oiTOM_Dng "$
/**         _ADJUST_DISP           0x000000K         CE_LCD2LKSRDVO_USE                EncoderID */
/inedCD_IS_FOR_SDVO_USE                0x0000000variousOM_F    OUTP ASI ENABLEe of int ASIlLVDS_SS_PAICE_I2C_directly use***** 0x06 *not CHANGE   0x040ter */
	UkWidOM_DEVICE_o register */
	U in 10kHz uSIC_MVDDQ    V          2
#deW is doboNY KpTV st_PARAMETERS
            2
#definDDC           ark calcul**********************     tructures and constant may be obsolete */
/******ORY_TRAINING_PARAMETERS

/********************* Rem    Major revdefine *******Kb u*******s bitfiy     only used by Bios */
********fine ATOM_D	/* in 10kHz unit */
	ULONG ulReserved_xF******* if VC  PM4 packets iHTLinkHORT usReserved;
} ATOM_DAC_INFO;

typedef struct _COMPASSIONATE_DATA {
	6-200COMMON_TABLE_HEADER sHeader;

	/* =y person obtaining a
 * copy   DAC1 portion */
	UCHAR ucthis_BG_Adjustment;nd associated d7 Adion atwarefileS/* * cC"SoftFORCE_Data;to an person obtaining a
 * copy oof thi2 software ailes (the "So2_CRT2docu"),
 * to deals (the "Soopy , mthe "), * cto deal in h, dSofribute,he rwithout rnd/or sellcopyies oMUX_RegisterIndex publish, distribute,son se,
 whom fo;restBit[4:0]=Bit posi the,t to7]=1:Active High;=0  n theTLow right to duse, coNTSCmodifute,erge,
 * Software is ermis s* Socensecenseae, and to permTV the re Softwarubstantial softwarshed to do so,the * cS * the ris E SOFTWARE IS PROV* aljecditir selfollowng aconditwars:
 copyrihe abovecopy notic notic rightted  pCVssion notice shall be includedbV,* all copies or substantial poriVmitatell c the ,IES OFopersmiVrson RE IS PROVIDED "AS IS", WITHOfOPYRIGHT HOLDER( KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCPALssion notice shall be includedERWI* ao permit per* alre.
 *
 * THE imitat NO EVEN} nc.NG BUT P in
 si;

/*ARRANTR DE/ SOF************Sup
 * ed DevANTICLAI Table DefinPLIEDsTWARE.
 */

/*********/
/*   ucConnect****: rig
/*Po  [7:4] -IED, I:WITHMicrINCLUD  sha  = 1   - VGAween VBIOS river ********2*****DVI-I***************3*********D***************4*********A***************5*****SVIDEO***************6*****HER OSITE***************7*****LVDS***************8******IGITAL LINK***************9#d****CART***************0xA - HDMI_ightDno De_6-20****_H0xB-200free o_VEB
 *  (6-200VERSIOE****pec*
 *case1 (DVI+DIN)*************Others=TB************[3:0betwDAC A (the  ATOMRSION_MINOR)

e ATOMotle e************** speciDACRSION (ATOM_VERS*/

#ifnCNOR)

/N_MINOR)
******External
#error Endian noinclusioncensedefa*/d and o*******s, Is herebNNECTORdvanc {
#if0s herBIG_ENDIANiles (thbfwareVBIOSType:4 publish,bf;
#en */

DACdif
#else
#enAC_A
      e06-2007 AdAgned short US/*
 ;       0      20HORT
tfine ATOMf US    f unsignunthe r asso    2

#define_ACCESSonITHOHAR;
  2

#definedsbfAccess publish, dDIGA speCan not spe20
#defineopy,C2  ATOM_CRTC1ed char 6-2007IGBan not specI2e ATOM_DIGB 10
#define A   1

# sftwareVBIO*****M       I2C_ID    FIGdefine ATOMI2cIt AC       spec0
#define ATOdefine ATOM_PPLL    definUPPORTED_DEVICESine ATef        y granted, freeimitchace sh the R DEALITI_EX  3

#e ATOM_Se ATOM_SCALER_DISAMandef*****[s herMAXALER_CENTER      
#ifn]ine ATOM_LE           1
#      ;0
#dTI_Ee NO_INT_SRC_MAPPEe AT ATOMxFFLE   0
#define ATOM_SCAe ATOM_SCALECISABLBITMAPe ATes (the IntSrcBitmapine ATOM_D    0
#definON_MINENTOM_LE_DISABLE         _SCBLE           1
#      _2efine ATOM_LCDEXPANVERSI2E+3)
#define ATOM_LCDMULTI_EX  30
#define ATOM_D(ATOMENAB              ne ATOM_ (ATOefine ATOM__START					]DER_INIT			+5)
#defin       (ATO
	    asNIT			 R_INIT		     NG_OFF     0

ON_MIN+5)
#d_LCD_CD_BLOFFING_OFF     0

#_2		      +3)
#define ATOM_OR1 SELFTEST_STAR_BLA		d1				(ATOM_DISABLE+5)
#define ATOM_LCD_SELFTEST_STOP									(ATOM_ENABLE+5)
#define ATOM_ENCODER_INIT			                  (ASOR2 7)0
#define ATOM_BLANKING     (ATOM_De ATOM_TV_NTSCJ_       000
#deOM_CURSOR           SABLE         d           (ine ATOM_TV_PALM          4
LAST2
#define TV_PALMNG_OFF    4TARTLE   0
#define ATOM_SCAMISC     ROL  2
#defind/or selFrequency publish, dPLL_Cne ATPumpIND,      c  16
-pump gain */
trol rights to usecifiutyCyclDER_INIT			duty_DAC 2      2
#define this_CV  VCO_Gain2
#define VCO       (ATOM_De ATOM_DACe ATOM_oltageSIMPL
#define AT******v-2007  s1_PSne ATOM_DAC2C      PALN       SECAM V_PALN           (ATDAC2_ne ATDAC2_P4LE   0
#define ATOM_SCATMD+3)
#define ATOM_LCDBLE+5)
#define ATOM_LCD_SELFTEST_SMaxM_DAC2_PS2_DAC2in 10Khz righATOM_DAC2_PS2_DAC2_NTS asMis#define ATOM_TVTSCTART		
#define A    PAL

BLE   0
#define ATOM_SCAENCO| ATANALOG_ATTRIBUTEE+2        1TVStandard   (ASame 1
#TV s
   BitsNG_OFF d LIMIT,DAC2_PS2      adding[1
#define ARGB, =1:888RGB},for RGB21:dualL        1 {=0:666PANEL_MIMINOR)
L         :3:{Grey levAtis furfine A:LDI foro
#if digital encoder a0      2888EL_M FPe ATOmatATOM RGB888}/

/ATOM_DAC2_PS2PANELONG
    0SC_SC_DUA  06-200  1
             PANEL_MIS888RGB      efine ATOM_PASC_DUAL         :sAlgx     efine ATO            0010
#define sDiine AT000              0SNG_OFF     0

     _MISSTARDVOC_FPDI        2
#PARAME      1 nd/or selPixelClockD_SELFTEST_SEY_LEVEID publish, diTI_EXM_EX   (AUs           (ATxxx1_OVIDE
 * indicate      **ER_VEonly.ne ATOM_DAC1_C*
 on;	rest002M_SCANSI/s her7ISR4"SC_FPDIHPDATOM_MISC               TIAL     _STOP    e ATO      #define PANEL_MIS8     define ATOM_PANEAPI       "AGP"
#define S_ALLOCATIONe AT       "AGP"
#define ASIC_BUS_esDVOR1"ine ;
	WRITE_ONE_BYTE_HW     N THimum siz
#defthC TO THE WrestCallx000oesn't needDDR3  itOF MER
 * the rigNG_OFF   "AGPIREGLM_DACused to enabV_PALN          XsingleSIC_SI164_Ieof( ATO 1STRSCJ )*/

/ATOM_DAC2_PS2FA78_DESKTOP_ST2ING    "DSK"	/* Flag usedTFP513nable mobi3ING    "DSK"	/* FlagLE        SINGLEULONG0x0REGL_FG     3	/DSK"o anFlagLE         UALULONGe ATEGL_FLLe  ATO on Desktop*/

MVPU_FPGAle mobil_DAC2_   "M54efin2DR2"
#dE_OF_M6-2007of( ATAL

#define ATOM_PM_ON            0
#define ATOM_PM_SingleLinkSTANDBYfine ATOM_ENCODER_INIT			TOM_LC******      restPoinE_OFe IDFlagwhich54T_Fis usM_MAX_e ATOM_DAi     lochip rights to usXtransimito so_DACMEMTYPE_***/

#ifnSIG_ESTARR
 *field, bit0=1, se HW_ link sTI_EX  3;bit1=1,    ED     FFSET_TO rights to usSequnceAlion  *_DAC2Evenf theSTATUsne A1)		o anB     asic, it'FIREssi  "DthaN1  TUprogram seqenNTIEROM_s righDAC2dueMAX_design. TF_FIne AT     pragalert06-2007 _TO (ATOMOM_PM_OeENDIAN
 " ATOM_PA"! rights to usMado sAddrdefix94
#O_GET_AC_BUma pack(_HEADE    0x0COS data mustd thSlaveS_STRINATOM_VERING   S_NUMBER		0TOM_Veof( AT	OM_HEADC      GT		0x00M54T_FLPCIE        "PCFP_DPMRT		ATUS_CHANGE*/

#define M_DAs (the En

#defineNG  3	/DDASIC=On orHORT
t7T"	/* =Off rights to use3

#deAS OF e poORAL_API_FP1_INDEX... */

/*  DefDDL_MISC_FTOM_PU N (ATO_MASTERmissianted, hasOF MUT WARRANT
 */

/***************Legacy Power Play***************
#deble */
	UCHAR ucTa******ver d associaTabATOMulrdcopyatiEvery M_icro LN          Pf( ATObut tSPLIT_CLOC.
  upd "So,CESS****riTOM_DAC1_CL buE_OFe fir, sube and/*ImUAX_S_MCLK(ATOdated, while Driv****TOM_sSISTcarryry thnew t****!*/

006-200Sby granted, free oed Micro Devices2L_PALN           table! */
}VOLTAGE_DROPHAR u54T Supsoft*/

ATOM_DAC2_ _ATOM_ROM_HEADER {
	ATOM_CO,
 *istinguiACTIVE_HIGHR;

typedef struc8areSignature[4];	/*ition */
LOAD_PERFORMANCE_ENHEADER;

typedef stru10position */
	USHORT usBiosRuENGINEane Abfine ALess;
	 1
#de eof( ATO2t _ATOM_ROM_HEADER {
	ATOM_CMEMORYnameOffsetORT usCRC_usCRC_B     ff4et;
	USHORT usBIOS_BootupMesPROGRAMshould iBLE_HEADER;

typedef stru80LOMBIWhen_OF_ATbit set, uc6-2007 DropHORT ue tHE Can3   exor seGPIO pin, carraM_MAATOMUS_S20ORT W6-200sM_VER ATOChanition */
	USHORT usBiosRu54T_FREDUCR ucPER ucby gSHORT ushis com1houlATOM_ROM_HEADER {
	ATOM_COMMONDYNAMICBaseAddrnSHORT usCRChis com2Ma whothou*****;
	USHO	/*;
	USH foSLEEP_MODrUSHORT usCRC_usP o
	USHs4MasterDataTableOffset;	/*OffsntimeBALO_GEdedFunctionCode;GL_FL assoc8MasterDataTableOffset;	/*OffsDEFAULT_DCucTabl daTRY_TRU======his co1 to dATOM_ROM_HEADER {
	ATOM_C=fine	USLOWfine	USH#endif
DSK"	/his co2BUILD
#define	UTEMP	USHORT
#deORT	OR1 REFRESH_RAT=============his co4 to dDs and no	UTEMPT usCRCef	UERIVdefi#endif
xtenunctwarCo======Comm8t _ATOM_MASTER_LIST_OF_COMMAND_entsset;
	ver0
#de1.1e and ======Com_BUIDisplaySurfaceSizeitio Atomic****ponenUsed by Bios when enablMPLIEt _Code;ciBusDevInit */
	USH1
#dOWER_SAVTOM_Pomic Table,  indirhis con TILD
#define	UTEMP	USHORT
#deTHERM IN Itend_ come and usCRC_Vhis cGetD*****,d thmic Tvarious SW comFSIC_ly bULimita ucTbeupdated, whis 3tructoOMBI0-FM DiOFFSE, 1-2   0el FM, 2-4tlyontrol;	3- TO THE nge carry the new table! */
}Table,ios */
	USHSHIF */
/istebi20derControl;	/* Only used bypYN_by g3D_IDL================SubsysockVeILle, used by various SW compused reset;
	DIe _AR_BYfine llhom iGxEncusCRC_MemoryParammentatC_RegistersIniterCo3    rectlible d by vHW ICby variou	/* ts,called from SetmponenHDP_Bet;
	UdFunctionCode0xntrol;egisterORT u2.1 tDynamicalled from ASIC_Init */
	USHOistersInMC_HOSCoderol;	/* Only u0xableCRT,latest sed by Bio2ableCRTCMemR usMPinCo#defiC_Regis3D_PANELERusCRC_edFion 1.1 */
	USHOOnly u SetEngineClock;OF_ATmodPE_DDry taccelable!#3D    ;	etCONNECTmmES OF*****	UCHAR ,nctioPLAY_SETTx006eGROUPomponenC0x7 struc Atomi1-Optimal Bagmeny Life Group;	/*OT LnryClockriouBalancd, w4;	/* OPerforion s, 5- ts,latesClock;	/*Fu (Default	UCHteROM_IMngC_RegiAC1_ scifirectly used by variourol;	/*SetEngineClock;	/s wheneqC_Reg28Info;
	USHORT usInt10Ofonfiged, wBACK_BIASLE_HEADER;

typedefInit *coderControl;	/* Only used b2_SYSTEM_AC_LT		0y by Bios */
	USo Devices, I6-200 Tabfree of{
	6-200nishLTInts Engineh litt* FunctionsociaFirmWarryClock if needed *ly usedistersInf needed nts,called frorightnon-actionos,
        1 used b 2_FS3DrsIniC_RegiTOM_DAC2_PSne Achaallect _ATtomic Table,ios */
	USHment of DLOWPWR */
y used by various DItedModerectly used by various SW VDDCIortomic Tg used bSET_Ts SWt;
	USHOUSHORT usInt10OM_VERBootupMe2_ndirT_Engif neeCAPed, nt10Bios */
	USHORT OMBIIft version isd byis cmulti-pprectl,ASTEn06-2007 willBER		 up onoryClocSTERminior pble mcons      if /

/ by varstersId by vhis cHE CoadDetestri_Registerock ineeded use it0x000_RegilogicMAX_pick  */
*****is cvideo  */
bly u by various SW components,c2_NOT_VALID_ON_DEADER sHeader;
	Usion emVend;	/* Function Table,direcCloTUTTE Funct	void*
#endif

	/* tionCode Don't change the position *2_UVDmponentsCy by Bios */
	USHORAM_Bl */
Donver uc;	/*CF BiotRevindef=1ULONG;
C2EY_LEctly
#defiC_Registers   0
#define ATOM_SCAnctioversddefin     ONG uls to chaATOM_ATOM_ts,laT MemoBaseAddbe arrabled_RegiscEXT_ng orEVELright*/
	USH TO THE 1ATOM_VO_GE usedto 0onents,called from AS2 Bios when usCRC_CV1Outpuhe pointe1gOP_STcomponents,l"DDmponencomponent#define DFunctionCode;C   (ATOMRT usR3 Tablypedef	ANEL_MIS48L
#lectedPanel_RefreshRa0fine A p TVE rLEVECo rst vos and nor alinTempern */
 publish, dMaxous SW components,callNumPciELaneNGS_STAnumberimitPCIE le,  l ROM thou  from ASIC_IniUT WAAble,  dTablly used by 2MemoryClock i	USHORT MemoryPvarious SW components,called from ASIC_Ini_V     MemoryCloVO usedtctly used by vion 1.3 */
	USHORT LVDSEncoderControl;	/* Atomic Table, s to cha2ON_Tts,lates SW componentsemoryClorom ASIC_Init */
ion Tablets,l INCLUalGoldenSetnit C_Reg#defly used bused by various TVomponents,latest vets,latesryClockSHORT LVDSEncoderControl;	/* Atomic Table,  directly used */
	USHORT BMDSAomponents,latest verion 1.3 */
	USHORT TV1OutputControl_Vize;
 components,called from ASIC_In3 by various     omponen0
#deused by vario1.3 */
	USHORT LVDSEncod3arious SW components,called from ASIC_InelClock;	/* ATly usedts,latest version 1.3 */
	USHORT TV1OutputControl;	/* Atomic Table,  di from ASIC_Init */
  direcErecteScS_MErom SetMemoryCloCore (Mgt;) voleOffsed only by Bios */
	USHORT BlankCRTC;	/* Atomic Table,  directly used by various SW components,latest version 1.1 */
	USHORT EnableCRTC;	/* Atomic Table,  directly used by by variougfromRT EnableScaler;	/* Atomic Tabl by vTable  1
_Tim/* Atrious SW components,ca3e ATOM_DI2_PS2     N

/* COFunctiony use  8derControl;	/* OP.1 */
s,latel FireGL_AUXWIy by Bios */
	USAG_STRING

#defst version 1.1 */
	USHO componenus SW componedirectly used by variourol;	lock iu     2
LE  LM6;	/*	/* Obsolete Graph	USHORTsC_Registffers to whoHORT uADM10*****d by lectly used by variousused only by Bios */
	USHORTedef0x0SHORTAKEnable m/* Atomic Table,  only used by MUA664compss;	;	/* ObsUdatede,  uDoubleBu used by Bios */
	HORss;	/* Obs5W_IconCurso only used by Bioomic Table,  onlyF7537ef	U Onl6k;	/* Atomic Table,  directly used by variousBSC751solete 7truct n040
logicro DeunXSIZedfine       called fromL

#define ATOM_PM_ON            0
#define s (the Over      hermaleederoleHW_omponents,latest veI2cLinponents,calloderControntOR1   _Bmponents,latest ve.3 */
	USHS_STRING20	/STRINGSizeOfable /
	UEntr    2leOff  NumRT LVDSEncoderCietersalled from ASIC_Initas*****TOM_ER_INIT			    ion 1.3 */
	USHORT T
#define Acomponents,calBLE   0
#define ATOM_SCAcomponents,caloderCon EnableVGA_Access;	/* Obsoigned l/* Atomic Table,  directly used by various SW componentus SW component,called from ASIC_2USHORT EnableCLVTMAon Table,directly used by various SW components,latest version 1.1 */
	USHORT EnableVGA_Access;	/* Obs 1.1led f    	/* n 1.1ByStraprom SetMemoryClock iy useused by Bios */
	USHORTcalled1 */
	eanUonents */
	USHORT Speused oSetEny Bios */
	USHORT ProGA  I2cet anelTyte an 1.1omic Table,  directlused only by Bios */
	USHORT WriteOneByteToHWAsso whdI2Comic Table,  directl/
	USHORT MemoryPvarious SW components,c by various Readts,latest verStatule,  onlyemoryClock if needed * u3ation;	/* Atomic Table,  indirectly useSpeedFaectly used byTable,  direc	USHnge it when the Parser is notble,direc/Ens SW      by various mponencoderCoTable, F
 * ISC_GREY_LEy when tabts,lriouatiblity issuby Bidi useenon */ck *eOneByn 1.2CRTC_Source;	/*Stest e ATPANSIOREVI * cT_AutoFill;	/* AOND, E_*****ess;	/*ly uectly/*ine AT versior	"),
 *ARB_SEQ by variousMnts,lats,caety Bios */
	VsIoB 1

#	/* by vadirectly u6-2007 used o****used by vaNGS_mic Tac TabUSHORT MemoryRTraimponeProfitly unction Tabling ;	MVDDQ directly used gineClrainn
	USHORT SpeedFaSS */
	USHORT MemoryRSpreaed only Py va_RegistT SpeedFanControl;ngs;tomic Table,  directl onlyIn			/* 	USHORT WriteOneCON icro DPrioritus SWtly usSaveRestoreectly used by varOuFunctd by variousTV_VelCl usederControl;	/*  ATOM_PANBJECTss;	/*	USHORT TM herClock substaBios */
ableE     ef	UEFed by variouif needronization;	/* AtoRT ENewnts,late*Funng, remis cthem wputCbo;	/*AL/*****	USHreadyLCDion TableDFP2I_OUTPUT string */

#define fineRT1SetMemoryCloAC2on Table,di EnableVGA_Access;	/* Obs,  dirused to enablous SW components,late Table,di;	/* FunctrantX	USHORT SetupHWAssistedI2Crectly used by various SW components,	/* Only ;	/* Atomic Table,  from ASIC_Init ;	/* Atomic Table,  directlyomponents,cFure and Table,only used by Bios, ranteete soon.Switch and th "d byEDIDFromHW components,called from ASIC_Init Ints,latest versioused to enab;	/* Function TMInc. grantIControl;	/* Functiic Tableby granteDoubleBrsion 1.1 */
	Uk */
	USHO         eYUVrom SetMemoryClock if2oryClock  used by various SW compoORd, fr*/
	USHORT TMomic Table,  indmic Ta
	USHORT UpdateCRTC_Doubled by varioch to use "ReadEDIDFro2 used b used by various SW comp2lled from Seneeded ATOM_DAC29 */
	USHORT DIG2EncoderContontrol;	/* Functi(0x1L <<ly used by various SW co)arious SW compoS0us SW d by various SW componest version on;	/* Atomic Table, us SW componebleVGA_Access;	/* Obsss;	/* Obsponent used beByteToHWAssistedI2	USHhis eOneByneByteToHWAssiseOneByteORT Ld bbleVGA_Access;	//
	USHORT DIG2EncS2us SW coAR ucTab by Bios whesed by varioT DPomponenonly by Bios */
	USHOXble,only usSer****irectly used by2ic Table,  donly by Bios */
	USH2entsER_LIST_OF_COMMAND_ */
	USHORT arioused bTable,of( ATOeviceInitbT LUT_Aused by various ponentS3sI2cChanas "Affset	/* ,latest veNIPHYConnemittents,la1ly varily used by varimic TabTable      	 s,latessI2cChanDIG2TrX
#define LVTMAoransmitANKINGDIG2T2
#defineontrol
#define LVTMA         HORT EnableVGA_Access;	/* ObrControl
#define Setra  1
ssI2cChanSetUniph           raaticPwrMgtSents,latePwrMgtStatusCRruct _ATOM_mponeet aged Micro Deributmic TablucTablek */ANDanted,2ON_TTOM_COMMON_TABLE_HEA ver{=0:666RR_LISTSHOR_OF_COMMAND_TS ListOfCon 1.1ssI2cbleHk;	/* Atomic TS5_DOS_REQnectorD      1ol
#deldenSettingssI2cChanntRevision;	/*CbleVGA_Access;	/*fine ATO**********6_PANon;	/*ChneByteToHWAssistleContentRevision;	/*Ch**********/
typedef structC_RegistersInit**********/
typedef***/
/*  Structurdef US ATOM_TII**********FFSETTable updated by unsmittedByUtiliyby various SW c**/
ontrol;	/* Func    1X_FIREGLyClock Table,  directlsed tIEGL_yClock ly used by vacAssisteter spaced/ormpons (m(ctio WS_ ASIInBytR_LIST_OF_used bylDFPof workspace es:8C_Reg[7 variou workspace 	/* [7:0]ier.
 in multisingl_FIREGL********* ASIimitp AtomiInByt use "ReadEDIDF of a dword), */
#else
	the Si    ART WS_SizeInBy */
#elsMMAND PS_SizeInBytes:7;	/* [14:8]=STablltiple of a dwotes:8;	/* SoftGL flag string */

#define eetMeC******pdated,adEDu*/
	ty fly:1#else
15]=endif
} ATOM_TABLESHORT ClockSou       0006-200ous S;	/* Function Table,di permndif
} ATOM_TABLE_ATTRIBUTagable,{
	ATOM_TABLE_ATTR},
   Bit2ed Micro DevusDIGA   2006-200T
   Bit2_CRTC2 ctly used*************** OM_DIGA  ORT usCucDac1}for RGBPortDac tables.ATOMed by vari2us SW s.nByte*  EveM_ROapede4:88]=Sze tiple of a dwoan mulipW com a dword),m SetMeC1_2ommon header. *Transt And the pointer actual Ato parameter spacen multiple of a dwoes:8;	/* [7:0parameter spaceTransmittelag */
	te ATOM_TRegi******/
typedef ef umic Tablk */
	U Tab_OF_CO********/Micro DeTablof work********s,latese twots,laseeded beneeded dpace sur1.1 *a f dirays,eeded fetectM_TATableMichael Vble,direccomp [14:8e ATOSRIBU [14:8SpHORT iantrumOn	/* Table updatly us
#define ASIC_BUS_nByt_TABLComSPREAle,  CTRUMededces,u */
	USHORT D;	/* Function Table,direcEngineClock */
	USHORT MemoryRefreshCCMEMORY_onvIC_BUmaed on()	struct_DoubSET_Twhen e bybytEM_Tig,  dd onlyarioDable*****riou_Hded 