/*

  Linux Driver for Mylex DAC960/AcceleRAID/eXtremeRAID PCI RAID Controllers

  Copyright 1998-2001 by Leonard N. Zubkoff <lnz@dandelion.com>

  This program is free software; you may redistribute and/or modify it under
  the terms of the GNU General Public License Version 2 as published by the
  Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  for complete details.

  The author respectfully requests that any modifications to this software be
  sent directly to him for evaluation and testing.

*/


/*
  Define the maximum number of DAC960 Controllers supported by this driver.
*/

#define DAC960_MaxControllers			8


/*
  Define the maximum number of Controller Channels supported by DAC960
  V1 and V2 Firmware Controllers.
*/

#define DAC960_V1_MaxChannels			3
#define DAC960_V2_MaxChannels			4


/*
  Define the maximum number of Targets per Channel supported by DAC960
  V1 and V2 Firmware Controllers.
*/

#define DAC960_V1_MaxTargets			16
#define DAC960_V2_MaxTargets			128


/*
  Define the maximum number of Logical Drives supported by DAC960
  V1 and V2 Firmware Controllers.
*/

#define DAC960_MaxLogicalDrives			32


/*
  Define the maximum number of Physical Devices supported by DAC960
  V1 and V2 Firmware Controllers.
*/

#define DAC960_V1_MaxPhysicalDevices		45
#define DAC960_V2_MaxPhysicalDevices		272

/*
  Define a 32/64 bit I/O Address data type.
*/

typedef unsigned long DAC960_IO_Address_T;


/*
  Define a 32/64 bit PCI Bus Address data type.
*/

typedef unsigned long DAC960_PCI_Address_T;


/*
  Define a 32 bit Bus Address data type.
*/

typedef unsigned int DAC960_BusAddress32_T;


/*
  Define a 64 bit Bus Address data type.
*/

typedef unsigned long long DAC960_BusAddress64_T;


/*
  Define a 32 bit Byte Count data type.
*/

typedef unsigned int DAC960_ByteCount32_T;


/*
  Define a 64 bit Byte Count data type.
*/

typedef unsigned long long DAC960_ByteCount64_T;


/*
  dma_loaf is used by helper routines to divide a region of
  dma mapped memory into smaller pieces, where those pieces
  are not of uniform size.
 */

struct dma_loaf {
	void	*cpu_base;
	dma_addr_t dma_base;
	size_t  length;
	void	*cpu_free;
	dma_addr_t dma_free;
};

/*
  Define the SCSI INQUIRY Standard Data structure.
*/

typedef struct DAC960_SCSI_Inquiry
{
  unsigned char PeripheralDeviceType:5;			/* Byte 0 Bits 0-4 */
  unsigned char PeripheralQualifier:3;			/* Byte 0 Bits 5-7 */
  unsigned char DeviceTypeModifier:7;			/* Byte 1 Bits 0-6 */
  bool RMB:1;						/* Byte 1 Bit 7 */
  unsigned char ANSI_ApprovedVersion:3;			/* Byte 2 Bits 0-2 */
  unsigned char ECMA_Version:3;				/* Byte 2 Bits 3-5 */
  unsigned char ISO_Version:2;				/* Byte 2 Bits 6-7 */
  unsigned char ResponseDataFormat:4;			/* Byte 3 Bits 0-3 */
  unsigned char :2;					/* Byte 3 Bits 4-5 */
  bool TrmIOP:1;					/* Byte 3 Bit 6 */
  bool AENC:1;						/* Byte 3 Bit 7 */
  unsigned char AdditionalLength;			/* Byte 4 */
  unsigned char :8;					/* Byte 5 */
  unsigned char :8;					/* Byte 6 */
  bool SftRe:1;						/* Byte 7 Bit 0 */
  bool CmdQue:1;					/* Byte 7 Bit 1 */
  bool :1;						/* Byte 7 Bit 2 */
  bool Linked:1;					/* Byte 7 Bit 3 */
  bool Sync:1;						/* Byte 7 Bit 4 */
  bool WBus16:1;					/* Byte 7 Bit 5 */
  bool WBus32:1;					/* Byte 7 Bit 6 */
  bool RelAdr:1;					/* Byte 7 Bit 7 */
  unsigned char VendorIdentification[8];		/* Bytes 8-15 */
  unsigned char ProductIdentification[16];		/* Bytes 16-31 */
  unsigned char ProductRevisionLevel[4];		/* Bytes 32-35 */
}
DAC960_SCSI_Inquiry_T;


/*
  Define the SCSI INQUIRY Unit Serial Number structure.
*/

typedef struct DAC960_SCSI_Inquiry_UnitSerialNumber
{
  unsigned char PeripheralDeviceType:5;			/* Byte 0 Bits 0-4 */
  unsigned char PeripheralQualifier:3;			/* Byte 0 Bits 5-7 */
  unsigned char PageCode;				/* Byte 1 */
  unsigned char :8;					/* Byte 2 */
  unsigned char PageLength;				/* Byte 3 */
  unsigned char ProductSerialNumber[28];		/* Bytes 4-31 */
}
DAC960_SCSI_Inquiry_UnitSerialNumber_T;


/*
  Define the SCSI REQUEST SENSE Sense Key type.
*/

typedef enum
{
  DAC960_SenseKey_NoSense =			0x0,
  DAC960_SenseKey_RecoveredError =		0x1,
  DAC960_SenseKey_NotReady =			0x2,
  DAC960_SenseKey_MediumError =			0x3,
  DAC960_SenseKey_HardwareError =		0x4,
  DAC960_SenseKey_IllegalRequest =		0x5,
  DAC960_SenseKey_UnitAttention =		0x6,
  DAC960_SenseKey_DataProtect =			0x7,
  DAC960_SenseKey_BlankCheck =			0x8,
  DAC960_SenseKey_VendorSpecific =		0x9,
  DAC960_SenseKey_CopyAborted =			0xA,
  DAC960_SenseKey_AbortedCommand =		0xB,
  DAC960_SenseKey_Equal =			0xC,
  DAC960_SenseKey_VolumeOverflow =		0xD,
  DAC960_SenseKey_Miscompare =			0xE,
  DAC960_SenseKey_Reserved =			0xF
}
__attribute__ ((packed))
DAC960_SCSI_RequestSenseKey_T;


/*
  Define the SCSI REQUEST SENSE structure.
*/

typedef struct DAC960_SCSI_RequestSense
{
  unsigned char ErrorCode:7;				/* Byte 0 Bits 0-6 */
  bool Valid:1;						/* Byte 0 Bit 7 */
  unsigned char SegmentNumber;				/* Byte 1 */
  DAC960_SCSI_RequestSenseKey_T SenseKey:4;		/* Byte 2 Bits 0-3 */
  unsigned char :1;					/* Byte 2 Bit 4 */
  bool ILI:1;						/* Byte 2 Bit 5 */
  bool EOM:1;						/* Byte 2 Bit 6 */
  bool Filemark:1;					/* Byte 2 Bit 7 */
  unsigned char Information[4];				/* Bytes 3-6 */
  unsigned char AdditionalSenseLength;			/* Byte 7 */
  unsigned char CommandSpecificInformation[4];		/* Bytes 8-11 */
  unsigned char AdditionalSenseCode;			/* Byte 12 */
  unsigned char AdditionalSenseCodeQualifier;		/* Byte 13 */
}
DAC960_SCSI_RequestSense_T;


/*
  Define the DAC960 V1 Firmware Command Opcodes.
*/

typedef enum
{
  /* I/O Commands */
  DAC960_V1_ReadExtended =			0x33,
  DAC960_V1_WriteExtended =			0x34,
  DAC960_V1_ReadAheadExtended =			0x35,
  DAC960_V1_ReadExtendedWithScatterGather =	0xB3,
  DAC960_V1_WriteExtendedWithScatterGather =	0xB4,
  DAC960_V1_Read =				0x36,
  DAC960_V1_ReadWithScatterGather =		0xB6,
  DAC960_V1_Write =				0x37,
  DAC960_V1_WriteWithScatterGather =		0xB7,
  DAC960_V1_DCDB =				0x04,
  DAC960_V1_DCDBWithScatterGather =		0x84,
  DAC960_V1_Flush =				0x0A,
  /* Controller Status Related Commands */
  DAC960_V1_Enquiry =				0x53,
  DAC960_V1_Enquiry2 =				0x1C,
  DAC960_V1_GetLogicalDriveElement =		0x55,
  DAC960_V1_GetLogicalDriveInformation =	0x19,
  DAC960_V1_IOPortRead =			0x39,
  DAC960_V1_IOPortWrite =			0x3A,
  DAC960_V1_GetSDStats =			0x3E,
  DAC960_V1_GetPDStats =			0x3F,
  DAC960_V1_PerformEventLogOperation =		0x72,
  /* Device Related Commands */
  DAC960_V1_StartDevice =			0x10,
  DAC960_V1_GetDeviceState =			0x50,
  DAC960_V1_StopChannel =			0x13,
  DAC960_V1_StartChannel =			0x12,
  DAC960_V1_ResetChannel =			0x1A,
  /* Commands Associated with Data Consistency and Errors */
  DAC960_V1_Rebuild =				0x09,
  DAC960_V1_RebuildAsync =			0x16,
  DAC960_V1_CheckConsistency =			0x0F,
  DAC960_V1_CheckConsistencyAsync =		0x1E,
  DAC960_V1_RebuildStat =			0x0C,
  DAC960_V1_GetRebuildProgress =		0x27,
  DAC960_V1_RebuildControl =			0x1F,
  DAC960_V1_ReadBadBlockTable =			0x0B,
  DAC960_V1_ReadBadDataTable =			0x25,
  DAC960_V1_ClearBadDataTable =			0x26,
  DAC960_V1_GetErrorTable =			0x17,
  DAC960_V1_AddCapacityAsync =			0x2A,
  DAC960_V1_BackgroundInitializationControl =	0x2B,
  /* Configuration Related Commands */
  DAC960_V1_ReadConfig2 =			0x3D,
  DAC960_V1_WriteConfig2 =			0x3C,
  DAC960_V1_ReadConfigurationOnDisk =		0x4A,
  DAC960_V1_WriteConfigurationOnDisk =		0x4B,
  DAC960_V1_ReadConfiguration =			0x4E,
  DAC960_V1_ReadBackupConfiguration =		0x4D,
  DAC960_V1_WriteConfiguration =		0x4F,
  DAC960_V1_AddConfiguration =			0x4C,
  DAC960_V1_ReadConfigurationLabel =		0x48,
  DAC960_V1_WriteConfigurationLabel =		0x49,
  /* Firmware Upgrade Related Commands */
  DAC960_V1_LoadImage =				0x20,
  DAC960_V1_StoreImage =			0x21,
  DAC960_V1_ProgramImage =			0x22,
  /* Diagnostic Commands */
  DAC960_V1_SetDiagnosticMode =			0x31,
  DAC960_V1_RunDiagnostic =			0x32,
  /* Subsystem Service Commands */
  DAC960_V1_GetSubsystemData =			0x70,
  DAC960_V1_SetSubsystemParameters =		0x71,
  /* Version 2.xx Firmware Commands */
  DAC960_V1_Enquiry_Old =			0x05,
  DAC960_V1_GetDeviceState_Old =		0x14,
  DAC960_V1_Read_Old =				0x02,
  DAC960_V1_Write_Old =				0x03,
  DAC960_V1_ReadWithScatterGather_Old =		0x82,
  DAC960_V1_WriteWithScatterGather_Old =	0x83
}
__attribute__ ((packed))
DAC960_V1_CommandOpcode_T;


/*
  Define the DAC960 V1 Firmware Command Identifier type.
*/

typedef unsigned char DAC960_V1_CommandIdentifier_T;


/*
  Define the DAC960 V1 Firmware Command Status Codes.
*/

#define DAC960_V1_NormalCompletion		0x0000	/* Common */
#define DAC960_V1_CheckConditionReceived	0x0002	/* Common */
#define DAC960_V1_NoDeviceAtAddress		0x0102	/* Common */
#define DAC960_V1_InvalidDeviceAddress		0x0105	/* Common */
#define DAC960_V1_InvalidParameter		0x0105	/* Common */
#define DAC960_V1_IrrecoverableDataError	0x0001	/* I/O */
#define DAC960_V1_LogicalDriveNonexistentOrOffline 0x0002 /* I/O */
#define DAC960_V1_AccessBeyondEndOfLogicalDrive	0x0105	/* I/O */
#define DAC960_V1_BadDataEncountered		0x010C	/* I/O */
#define DAC960_V1_DeviceBusy			0x0008	/* DCDB */
#define DAC960_V1_DeviceNonresponsive		0x000E	/* DCDB */
#define DAC960_V1_CommandTerminatedAbnormally	0x000F	/* DCDB */
#define DAC960_V1_UnableToStartDevice		0x0002	/* Device */
#define DAC960_V1_InvalidChannelOrTargetOrModifier 0x0105 /* Device */
#define DAC960_V1_ChannelBusy			0x0106	/* Device */
#define DAC960_V1_ChannelNotStopped		0x0002	/* Device */
#define DAC960_V1_AttemptToRebuildOnlineDrive	0x0002	/* Consistency */
#define DAC960_V1_RebuildBadBlocksEncountered	0x0003	/* Consistency */
#define DAC960_V1_NewDiskFailedDuringRebuild	0x0004	/* Consistency */
#define DAC960_V1_RebuildOrCheckAlreadyInProgress 0x0106 /* Consistency */
#define DAC960_V1_DependentDiskIsDead		0x0002	/* Consistency */
#define DAC960_V1_InconsistentBlocksFound	0x0003	/* Consistency */
#define DAC960_V1_InvalidOrNonredundantLogicalDrive 0x0105 /* Consistency */
#define DAC960_V1_NoRebuildOrCheckInProgress	0x0105	/* Consistency */
#define DAC960_V1_RebuildInProgress_DataValid	0x0000	/* Consistency */
#define DAC960_V1_RebuildFailed_LogicalDriveFailure 0x0002 /* Consistency */
#define DAC960_V1_RebuildFailed_BadBlocksOnOther 0x0003	/* Consistency */
#define DAC960_V1_RebuildFailed_NewDriveFailed	0x0004	/* Consistency */
#define DAC960_V1_RebuildSuccessful		0x0100	/* Consistency */
#define DAC960_V1_RebuildSuccessfullyTerminated	0x0107	/* Consistency */
#define DAC960_V1_BackgroundInitSuccessful	0x0100	/* Consistency */
#define DAC960_V1_BackgroundInitAborted		0x0005	/* Consistency */
#define DAC960_V1_NoBackgroundInitInProgress	0x0105	/* Consistency */
#define DAC960_V1_AddCapacityInProgress		0x0004	/* Consistency */
#define DAC960_V1_AddCapacityFailedOrSuspended	0x00F4	/* Consistency */
#define DAC960_V1_Config2ChecksumError		0x0002	/* Configuration */
#define DAC960_V1_ConfigurationSuspended	0x0106	/* Configuration */
#define DAC960_V1_FailedToConfigureNVRAM	0x0105	/* Configuration */
#define DAC960_V1_ConfigurationNotSavedStateChange 0x0106 /* Configuration */
#define DAC960_V1_SubsystemNotInstalled		0x0001	/* Subsystem */
#define DAC960_V1_SubsystemFailed		0x0002	/* Subsystem */
#define DAC960_V1_SubsystemBusy			0x0106	/* Subsystem */

typedef unsigned short DAC960_V1_CommandStatus_T;


/*
  Define the DAC960 V1 Firmware Enquiry Command reply structure.
*/

typedef struct DAC960_V1_Enquiry
{
  unsigned char NumberOfLogicalDrives;			/* Byte 0 */
  unsigned int :24;					/* Bytes 1-3 */
  unsigned int LogicalDriveSizes[32];			/* Bytes 4-131 */
  unsigned short FlashAge;				/* Bytes 132-133 */
  struct {
    bool DeferredWriteError:1;				/* Byte 134 Bit 0 */
    bool BatteryLow:1;					/* Byte 134 Bit 1 */
    unsigned char :6;					/* Byte 134 Bits 2-7 */
  } StatusFlags;
  unsigned char :8;					/* Byte 135 */
  unsigned char MinorFirmwareVersion;			/* Byte 136 */
  unsigned char MajorFirmwareVersion;			/* Byte 137 */
  enum {
    DAC960_V1_NoStandbyRebuildOrCheckInProgress =		    0x00,
    DAC960_V1_StandbyRebuildInProgress =			    0x01,
    DAC960_V1_BackgroundRebuildInProgress =			    0x02,
    DAC960_V1_BackgroundCheckInProgress =			    0x03,
    DAC960_V1_StandbyRebuildCompletedWithError =		    0xFF,
    DAC960_V1_BackgroundRebuildOrCheckFailed_DriveFailed =	    0xF0,
    DAC960_V1_BackgroundRebuildOrCheckFailed_LogicalDriveFailed =   0xF1,
    DAC960_V1_BackgroundRebuildOrCheckFailed_OtherCauses =	    0xF2,
    DAC960_V1_BackgroundRebuildOrCheckSuccessfullyTerminated =	    0xF3
  } __attribute__ ((packed)) RebuildFlag;		/* Byte 138 */
  unsigned char MaxCommands;				/* Byte 139 */
  unsigned char OfflineLogicalDriveCount;		/* Byte 140 */
  unsigned char :8;					/* Byte 141 */
  unsigned short EventLogSequenceNumber;		/* Bytes 142-143 */
  unsigned char CriticalLogicalDriveCount;		/* Byte 144 */
  unsigned int :24;					/* Bytes 145-147 */
  unsigned char DeadDriveCount;				/* Byte 148 */
  unsigned char :8;					/* Byte 149 */
  unsigned char RebuildCount;				/* Byte 150 */
  struct {
    unsigned char :3;					/* Byte 151 Bits 0-2 */
    bool BatteryBackupUnitPresent:1;			/* Byte 151 Bit 3 */
    unsigned char :3;					/* Byte 151 Bits 4-6 */
    unsigned char :1;					/* Byte 151 Bit 7 */
  } MiscFlags;
  struct {
    unsigned char TargetID;
    unsigned char Channel;
  } DeadDrives[21];					/* Bytes 152-194 */
  unsigned char Reserved[62];				/* Bytes 195-255 */
}
__attribute__ ((packed))
DAC960_V1_Enquiry_T;


/*
  Define the DAC960 V1 Firmware Enquiry2 Command reply structure.
*/

typedef struct DAC960_V1_Enquiry2
{
  struct {
    enum {
      DAC960_V1_P_PD_PU =			0x01,
      DAC960_V1_PL =				0x02,
      DAC960_V1_PG =				0x10,
      DAC960_V1_PJ =				0x11,
      DAC960_V1_PR =				0x12,
      DAC960_V1_PT =				0x13,
      DAC960_V1_PTL0 =				0x14,
      DAC960_V1_PRL =				0x15,
      DAC960_V1_PTL1 =				0x16,
      DAC960_V1_1164P =				0x20
    } __attribute__ ((packed)) SubModel;		/* Byte 0 */
    unsigned char ActualChannels;			/* Byte 1 */
    enum {
      DAC960_V1_FiveChannelBoard =		0x01,
      DAC960_V1_ThreeChannelBoard =		0x02,
      DAC960_V1_TwoChannelBoard =		0x03,
      DAC960_V1_ThreeChannelASIC_DAC =		0x04
    } __attribute__ ((packed)) Model;			/* Byte 2 */
    enum {
      DAC960_V1_EISA_Controller =		0x01,
      DAC960_V1_MicroChannel_Controller =	0x02,
      DAC960_V1_PCI_Controller =		0x03,
      DAC960_V1_SCSItoSCSI_Controller =		0x08
    } __attribute__ ((packed)) ProductFamily;		/* Byte 3 */
  } HardwareID;						/* Bytes 0-3 */
  /* MajorVersion.MinorVersion-FirmwareType-TurnID */
  struct {
    unsigned char MajorVersion;				/* Byte 4 */
    unsigned char MinorVersion;				/* Byte 5 */
    unsigned char TurnID;				/* Byte 6 */
    char FirmwareType;					/* Byte 7 */
  } FirmwareID;						/* Bytes 4-7 */
  unsigned char :8;					/* Byte 8 */
  unsigned int :24;					/* Bytes 9-11 */
  unsigned char ConfiguredChannels;			/* Byte 12 */
  unsigned char ActualChannels;				/* Byte 13 */
  unsigned char MaxTargets;				/* Byte 14 */
  unsigned char MaxTags;				/* Byte 15 */
  unsigned char MaxLogicalDrives;			/* Byte 16 */
  unsigned char MaxArms;				/* Byte 17 */
  unsigned char MaxSpans;				/* Byte 18 */
  unsigned char :8;					/* Byte 19 */
  unsigned int :32;					/* Bytes 20-23 */
  unsigned int MemorySize;				/* Bytes 24-27 */
  unsigned int CacheSize;				/* Bytes 28-31 */
  unsigned int FlashMemorySize;				/* Bytes 32-35 */
  unsigned int NonVolatileMemorySize;			/* Bytes 36-39 */
  struct {
    enum {
      DAC960_V1_RamType_DRAM =			0x0,
      DAC960_V1_RamType_EDO =			0x1,
      DAC960_V1_RamType_SDRAM =			0x2,
      DAC960_V1_RamType_Last =			0x7
    } __attribute__ ((packed)) RamType:3;		/* Byte 40 Bits 0-2 */
    enum {
      DAC960_V1_ErrorCorrection_None =		0x0,
      DAC960_V1_ErrorCorrection_Parity =	0x1,
      DAC960_V1_ErrorCorrection_ECC =		0x2,
      DAC960_V1_ErrorCorrection_Last =		0x7
    } __attribute__ ((packed)) ErrorCorrection:3;	/* Byte 40 Bits 3-5 */
    bool FastPageMode:1;				/* Byte 40 Bit 6 */
    bool LowPowerMemory:1;				/* Byte 40 Bit 7 */
    unsigned char :8;					/* Bytes 41 */
  } MemoryType;
  unsigned short ClockSpeed;				/* Bytes 42-43 */
  unsigned short MemorySpeed;				/* Bytes 44-45 */
  unsigned short HardwareSpeed;				/* Bytes 46-47 */
  unsigned int :32;					/* Bytes 48-51 */
  unsigned int :32;					/* Bytes 52-55 */
  unsigned char :8;					/* Byte 56 */
  unsigned char :8;					/* Byte 57 */
  unsigned short :16;					/* Bytes 58-59 */
  unsigned short MaxCommands;				/* Bytes 60-61 */
  unsigned short MaxScatterGatherEntries;		/* Bytes 62-63 */
  unsigned short MaxDriveCommands;			/* Bytes 64-65 */
  unsigned short MaxIODescriptors;			/* Bytes 66-67 */
  unsigned short MaxCombinedSectors;			/* Bytes 68-69 */
  unsigned char Latency;				/* Byte 70 */
  unsigned char :8;					/* Byte 71 */
  unsigned char SCSITimeout;				/* Byte 72 */
  unsigned char :8;					/* Byte 73 */
  unsigned short MinFreeLines;				/* Bytes 74-75 */
  unsigned int :32;					/* Bytes 76-79 */
  unsigned int :32;					/* Bytes 80-83 */
  unsigned char RebuildRateConstant;			/* Byte 84 */
  unsigned char :8;					/* Byte 85 */
  unsigned char :8;					/* Byte 86 */
  unsigned char :8;					/* Byte 87 */
  unsigned int :32;					/* Bytes 88-91 */
  unsigned int :32;					/* Bytes 92-95 */
  unsigned short PhysicalDriveBlockSize;		/* Bytes 96-97 */
  unsigned short LogicalDriveBlockSize;			/* Bytes 98-99 */
  unsigned short MaxBlocksPerCommand;			/* Bytes 100-101 */
  unsigned short BlockFactor;				/* Bytes 102-103 */
  unsigned short CacheLineSize;				/* Bytes 104-105 */
  struct {
    enum {
      DAC960_V1_Narrow_8bit =			0x0,
      DAC960_V1_Wide_16bit =			0x1,
      DAC960_V1_Wide_32bit =			0x2
    } __attribute__ ((packed)) BusWidth:2;		/* Byte 106 Bits 0-1 */
    enum {
      DAC960_V1_Fast =				0x0,
      DAC960_V1_Ultra =				0x1,
      DAC960_V1_Ultra2 =			0x2
    } __attribute__ ((packed)) BusSpeed:2;		/* Byte 106 Bits 2-3 */
    bool Differential:1;				/* Byte 106 Bit 4 */
    unsigned char :3;					/* Byte 106 Bits 5-7 */
  } SCSICapability;
  unsigned char :8;					/* Byte 107 */
  unsigned int :32;					/* Bytes 108-111 */
  unsigned short FirmwareBuildNumber;			/* Bytes 112-113 */
  enum {
    DAC960_V1_AEMI =				0x01,
    DAC960_V1_OEM1 =				0x02,
    DAC960_V1_OEM2 =				0x04,
    DAC960_V1_OEM3 =				0x08,
    DAC960_V1_Conner =				0x10,
    DAC960_V1_SAFTE =				0x20
  } __attribute__ ((packed)) FaultManagementType;	/* Byte 114 */
  unsigned char :8;					/* Byte 115 */
  struct {
    bool Clustering:1;					/* Byte 116 Bit 0 */
    bool MylexOnlineRAIDExpansion:1;			/* Byte 116 Bit 1 */
    bool ReadAhead:1;					/* Byte 116 Bit 2 */
    bool BackgroundInitialization:1;			/* Byte 116 Bit 3 */
    unsigned int :28;					/* Bytes 116-119 */
  } FirmwareFeatures;
  unsigned int :32;					/* Bytes 120-123 */
  unsigned int :32;					/* Bytes 124-127 */
}
DAC960_V1_Enquiry2_T;


/*
  Define the DAC960 V1 Firmware Logical Drive State type.
*/

typedef enum
{
  DAC960_V1_LogicalDrive_Online =		0x03,
  DAC960_V1_LogicalDrive_Critical =		0x04,
  DAC960_V1_LogicalDrive_Offline =		0xFF
}
__attribute__ ((packed))
DAC960_V1_LogicalDriveState_T;


/*
  Define the DAC960 V1 Firmware Logical Drive Information structure.
*/

typedef struct DAC960_V1_LogicalDriveInformation
{
  unsigned int LogicalDriveSize;			/* Bytes 0-3 */
  DAC960_V1_LogicalDriveState_T LogicalDriveState;	/* Byte 4 */
  unsigned char RAIDLevel:7;				/* Byte 5 Bits 0-6 */
  bool WriteBack:1;					/* Byte 5 Bit 7 */
  unsigned short :16;					/* Bytes 6-7 */
}
DAC960_V1_LogicalDriveInformation_T;


/*
  Define the DAC960 V1 Firmware Get Logical Drive Information Command
  reply structure.
*/

typedef DAC960_V1_LogicalDriveInformation_T
	DAC960_V1_LogicalDriveInformationArray_T[DAC960_MaxLogicalDrives];


/*
  Define the DAC960 V1 Firmware Perform Event Log Operation Types.
*/

typedef enum
{
  DAC960_V1_GetEventLogEntry =			0x00
}
__attribute__ ((packed))
DAC960_V1_PerformEventLogOpType_T;


/*
  Define the DAC960 V1 Firmware Get Event Log Entry Command reply structure.
*/

typedef struct DAC960_V1_EventLogEntry
{
  unsigned char MessageType;				/* Byte 0 */
  unsigned char MessageLength;				/* Byte 1 */
  unsigned char TargetID:5;				/* Byte 2 Bits 0-4 */
  unsigned char Channel:3;				/* Byte 2 Bits 5-7 */
  unsigned char LogicalUnit:6;				/* Byte 3 Bits 0-5 */
  unsigned char :2;					/* Byte 3 Bits 6-7 */
  unsigned short SequenceNumber;			/* Bytes 4-5 */
  unsigned char ErrorCode:7;				/* Byte 6 Bits 0-6 */
  bool Valid:1;						/* Byte 6 Bit 7 */
  unsigned char SegmentNumber;				/* Byte 7 */
  DAC960_SCSI_RequestSenseKey_T SenseKey:4;		/* Byte 8 Bits 0-3 */
  unsigned char :1;					/* Byte 8 Bit 4 */
  bool ILI:1;						/* Byte 8 Bit 5 */
  bool EOM:1;						/* Byte 8 Bit 6 */
  bool Filemark:1;					/* Byte 8 Bit 7 */
  unsigned char Information[4];				/* Bytes 9-12 */
  unsigned char AdditionalSenseLength;			/* Byte 13 */
  unsigned char CommandSpecificInformation[4];		/* Bytes 14-17 */
  unsigned char AdditionalSenseCode;			/* Byte 18 */
  unsigned char AdditionalSenseCodeQualifier;		/* Byte 19 */
  unsigned char Dummy[12];				/* Bytes 20-31 */
}
DAC960_V1_EventLogEntry_T;


/*
  Define the DAC960 V1 Firmware Physical Device State type.
*/

typedef enum
{
    DAC960_V1_Device_Dead =			0x00,
    DAC960_V1_Device_WriteOnly =		0x02,
    DAC960_V1_Device_Online =			0x03,
    DAC960_V1_Device_Standby =			0x10
}
__attribute__ ((packed))
DAC960_V1_PhysicalDeviceState_T;


/*
  Define the DAC960 V1 Firmware Get Device State Command reply structure.
  The structure is padded by 2 bytes for compatibility with Version 2.xx
  Firmware.
*/

typedef struct DAC960_V1_DeviceState
{
  bool Present:1;					/* Byte 0 Bit 0 */
  unsigned char :7;					/* Byte 0 Bits 1-7 */
  enum {
    DAC960_V1_OtherType =			0x0,
    DAC960_V1_DiskType =			0x1,
    DAC960_V1_SequentialType =			0x2,
    DAC960_V1_CDROM_or_WORM_Type =		0x3
    } __attribute__ ((packed)) DeviceType:2;		/* Byte 1 Bits 0-1 */
  bool :1;						/* Byte 1 Bit 2 */
  bool Fast20:1;					/* Byte 1 Bit 3 */
  bool Sync:1;						/* Byte 1 Bit 4 */
  bool Fast:1;						/* Byte 1 Bit 5 */
  bool Wide:1;						/* Byte 1 Bit 6 */
  bool TaggedQueuingSupported:1;			/* Byte 1 Bit 7 */
  DAC960_V1_PhysicalDeviceState_T DeviceState;		/* Byte 2 */
  unsigned char :8;					/* Byte 3 */
  unsigned char SynchronousMultiplier;			/* Byte 4 */
  unsigned char SynchronousOffset:5;			/* Byte 5 Bits 0-4 */
  unsigned char :3;					/* Byte 5 Bits 5-7 */
  unsigned int DiskSize __attribute__ ((packed));	/* Bytes 6-9 */
  unsigned short :16;					/* Bytes 10-11 */
}
DAC960_V1_DeviceState_T;


/*
  Define the DAC960 V1 Firmware Get Rebuild Progress Command reply structure.
*/

typedef struct DAC960_V1_RebuildProgress
{
  unsigned int LogicalDriveNumber;			/* Bytes 0-3 */
  unsigned int LogicalDriveSize;			/* Bytes 4-7 */
  unsigned int RemainingBlocks;				/* Bytes 8-11 */
}
DAC960_V1_RebuildProgress_T;


/*
  Define the DAC960 V1 Firmware Background Initialization Status Command
  reply structure.
*/

typedef struct DAC960_V1_BackgroundInitializationStatus
{
  unsigned int LogicalDriveSize;			/* Bytes 0-3 */
  unsigned int BlocksCompleted;				/* Bytes 4-7 */
  unsigned char Reserved1[12];				/* Bytes 8-19 */
  unsigned int LogicalDriveNumber;			/* Bytes 20-23 */
  unsigned char RAIDLevel;				/* Byte 24 */
  enum {
    DAC960_V1_BackgroundInitializationInvalid =	    0x00,
    DAC960_V1_BackgroundInitializationStarted =	    0x02,
    DAC960_V1_BackgroundInitializationInProgress =  0x04,
    DAC960_V1_BackgroundInitializationSuspended =   0x05,
    DAC960_V1_BackgroundInitializationCancelled =   0x06
  } __attribute__ ((packed)) Status;			/* Byte 25 */
  unsigned char Reserved2[6];				/* Bytes 26-31 */
}
DAC960_V1_BackgroundInitializationStatus_T;


/*
  Define the DAC960 V1 Firmware Error Table Entry structure.
*/

typedef struct DAC960_V1_ErrorTableEntry
{
  unsigned char ParityErrorCount;			/* Byte 0 */
  unsigned char SoftErrorCount;				/* Byte 1 */
  unsigned char HardErrorCount;				/* Byte 2 */
  unsigned char MiscErrorCount;				/* Byte 3 */
}
DAC960_V1_ErrorTableEntry_T;


/*
  Define the DAC960 V1 Firmware Get Error Table Command reply structure.
*/

typedef struct DAC960_V1_ErrorTable
{
  DAC960_V1_ErrorTableEntry_T
    ErrorTableEntries[DAC960_V1_MaxChannels][DAC960_V1_MaxTargets];
}
DAC960_V1_ErrorTable_T;


/*
  Define the DAC960 V1 Firmware Read Config2 Command reply structure.
*/

typedef struct DAC960_V1_Config2
{
  unsigned char :1;					/* Byte 0 Bit 0 */
  bool ActiveNegationEnabled:1;				/* Byte 0 Bit 1 */
  unsigned char :5;					/* Byte 0 Bits 2-6 */
  bool NoRescanIfResetReceivedDuringScan:1;		/* Byte 0 Bit 7 */
  bool StorageWorksSupportEnabled:1;			/* Byte 1 Bit 0 */
  bool HewlettPackardSupportEnabled:1;			/* Byte 1 Bit 1 */
  bool NoDisconnectOnFirstCommand:1;			/* Byte 1 Bit 2 */
  unsigned char :2;					/* Byte 1 Bits 3-4 */
  bool AEMI_ARM:1;					/* Byte 1 Bit 5 */
  bool AEMI_OFM:1;					/* Byte 1 Bit 6 */
  unsigned char :1;					/* Byte 1 Bit 7 */
  enum {
    DAC960_V1_OEMID_Mylex =			0x00,
    DAC960_V1_OEMID_IBM =			0x08,
    DAC960_V1_OEMID_HP =			0x0A,
    DAC960_V1_OEMID_DEC =			0x0C,
    DAC960_V1_OEMID_Siemens =			0x10,
    DAC960_V1_OEMID_Intel =			0x12
  } __attribute__ ((packed)) OEMID;			/* Byte 2 */
  unsigned char OEMModelNumber;				/* Byte 3 */
  unsigned char PhysicalSector;				/* Byte 4 */
  unsigned char LogicalSector;				/* Byte 5 */
  unsigned char BlockFactor;				/* Byte 6 */
  bool ReadAheadEnabled:1;				/* Byte 7 Bit 0 */
  bool LowBIOSDelay:1;					/* Byte 7 Bit 1 */
  unsigned char :2;					/* Byte 7 Bits 2-3 */
  bool ReassignRestrictedToOneSector:1;			/* Byte 7 Bit 4 */
  unsigned char :1;					/* Byte 7 Bit 5 */
  bool ForceUnitAccessDuringWriteRecovery:1;		/* Byte 7 Bit 6 */
  bool EnableLeftSymmetricRAID5Algorithm:1;		/* Byte 7 Bit 7 */
  unsigned char DefaultRebuildRate;			/* Byte 8 */
  unsigned char :8;					/* Byte 9 */
  unsigned char BlocksPerCacheLine;			/* Byte 10 */
  unsigned char BlocksPerStripe;			/* Byte 11 */
  struct {
    enum {
      DAC960_V1_Async =				0x0,
      DAC960_V1_Sync_8MHz =			0x1,
      DAC960_V1_Sync_5MHz =			0x2,
      DAC960_V1_Sync_10or20MHz =		0x3	/* Byte 11 Bits 0-1 */
    } __attribute__ ((packed)) Speed:2;
    bool Force8Bit:1;					/* Byte 11 Bit 2 */
    bool DisableFast20:1;				/* Byte 11 Bit 3 */
    unsigned char :3;					/* Byte 11 Bits 4-6 */
    bool EnableTaggedQueuing:1;				/* Byte 11 Bit 7 */
  } __attribute__ ((packed)) ChannelParameters[6];	/* Bytes 12-17 */
  unsigned char SCSIInitiatorID;			/* Byte 18 */
  unsigned char :8;					/* Byte 19 */
  enum {
    DAC960_V1_StartupMode_ControllerSpinUp =	0x00,
    DAC960_V1_StartupMode_PowerOnSpinUp =	0x01
  } __attribute__ ((packed)) StartupMode;		/* Byte 20 */
  unsigned char SimultaneousDeviceSpinUpCount;		/* Byte 21 */
  unsigned char SecondsDelayBetweenSpinUps;		/* Byte 22 */
  unsigned char Reserved1[29];				/* Bytes 23-51 */
  bool BIOSDisabled:1;					/* Byte 52 Bit 0 */
  bool CDROMBootEnabled:1;				/* Byte 52 Bit 1 */
  unsigned char :3;					/* Byte 52 Bits 2-4 */
  enum {
    DAC960_V1_Geometry_128_32 =			0x0,
    DAC960_V1_Geometry_255_63 =			0x1,
    DAC960_V1_Geometry_Reserved1 =		0x2,
    DAC960_V1_Geometry_Reserved2 =		0x3
  } __attribute__ ((packed)) DriveGeometry:2;		/* Byte 52 Bits 5-6 */
  unsigned char :1;					/* Byte 52 Bit 7 */
  unsigned char Reserved2[9];				/* Bytes 53-61 */
  unsigned short Checksum;				/* Bytes 62-63 */
}
DAC960_V1_Config2_T;


/*
  Define the DAC960 V1 Firmware DCDB request structure.
*/

typedef struct DAC960_V1_DCDB
{
  unsigned char TargetID:4;				 /* Byte 0 Bits 0-3 */
  unsigned char Channel:4;				 /* Byte 0 Bits 4-7 */
  enum {
    DAC960_V1_DCDB_NoDataTransfer =		0,
    DAC960_V1_DCDB_DataTransferDeviceToSystem = 1,
    DAC960_V1_DCDB_DataTransferSystemToDevice = 2,
    DAC960_V1_DCDB_IllegalDataTransfer =	3
  } __attribute__ ((packed)) Direction:2;		 /* Byte 1 Bits 0-1 */
  bool EarlyStatus:1;					 /* Byte 1 Bit 2 */
  unsigned char :1;					 /* Byte 1 Bit 3 */
  enum {
    DAC960_V1_DCDB_Timeout_24_hours =		0,
    DAC960_V1_DCDB_Timeout_10_seconds =		1,
    DAC960_V1_DCDB_Timeout_60_seconds =		2,
    DAC960_V1_DCDB_Timeout_10_minutes =		3
  } __attribute__ ((packed)) Timeout:2;			 /* Byte 1 Bits 4-5 */
  bool NoAutomaticRequestSense:1;			 /* Byte 1 Bit 6 */
  bool DisconnectPermitted:1;				 /* Byte 1 Bit 7 */
  unsigned short TransferLength;			 /* Bytes 2-3 */
  DAC960_BusAddress32_T BusAddress;			 /* Bytes 4-7 */
  unsigned char CDBLength:4;				 /* Byte 8 Bits 0-3 */
  unsigned char TransferLengthHigh4:4;			 /* Byte 8 Bits 4-7 */
  unsigned char SenseLength;				 /* Byte 9 */
  unsigned char CDB[12];				 /* Bytes 10-21 */
  unsigned char SenseData[64];				 /* Bytes 22-85 */
  unsigned char Status;					 /* Byte 86 */
  unsigned char :8;					 /* Byte 87 */
}
DAC960_V1_DCDB_T;


/*
  Define the DAC960 V1 Firmware Scatter/Gather List Type 1 32 Bit Address
  32 Bit Byte Count structure.
*/

typedef struct DAC960_V1_ScatterGatherSegment
{
  DAC960_BusAddress32_T SegmentDataPointer;		/* Bytes 0-3 */
  DAC960_ByteCount32_T SegmentByteCount;		/* Bytes 4-7 */
}
DAC960_V1_ScatterGatherSegment_T;


/*
  Define the 13 Byte DAC960 V1 Firmware Command Mailbox structure.  Bytes 13-15
  are not used.  The Command Mailbox structure is padded to 16 bytes for
  efficient access.
*/

typedef union DAC960_V1_CommandMailbox
{
  unsigned int Words[4];				/* Words 0-3 */
  unsigned char Bytes[16];				/* Bytes 0-15 */
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* Byte 0 */
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	/* Byte 1 */
    unsigned char Dummy[14];				/* Bytes 2-15 */
  } __attribute__ ((packed)) Common;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* Byte 0 */
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	/* Byte 1 */
    unsigned char Dummy1[6];				/* Bytes 2-7 */
    DAC960_BusAddress32_T BusAddress;			/* Bytes 8-11 */
    unsigned char Dummy2[4];				/* Bytes 12-15 */
  } __attribute__ ((packed)) Type3;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* Byte 0 */
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	/* Byte 1 */
    unsigned char CommandOpcode2;			/* Byte 2 */
    unsigned char Dummy1[5];				/* Bytes 3-7 */
    DAC960_BusAddress32_T BusAddress;			/* Bytes 8-11 */
    unsigned char Dummy2[4];				/* Bytes 12-15 */
  } __attribute__ ((packed)) Type3B;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* Byte 0 */
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	/* Byte 1 */
    unsigned char Dummy1[5];				/* Bytes 2-6 */
    unsigned char LogicalDriveNumber:6;			/* Byte 7 Bits 0-6 */
    bool AutoRestore:1;					/* Byte 7 Bit 7 */
    unsigned char Dummy2[8];				/* Bytes 8-15 */
  } __attribute__ ((packed)) Type3C;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* Byte 0 */
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	/* Byte 1 */
    unsigned char Channel;				/* Byte 2 */
    unsigned char TargetID;				/* Byte 3 */
    DAC960_V1_PhysicalDeviceState_T DeviceState:5;	/* Byte 4 Bits 0-4 */
    unsigned char Modifier:3;				/* Byte 4 Bits 5-7 */
    unsigned char Dummy1[3];				/* Bytes 5-7 */
    DAC960_BusAddress32_T BusAddress;			/* Bytes 8-11 */
    unsigned char Dummy2[4];				/* Bytes 12-15 */
  } __attribute__ ((packed)) Type3D;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* Byte 0 */
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	/* Byte 1 */
    DAC960_V1_PerformEventLogOpType_T OperationType;	/* Byte 2 */
    unsigned char OperationQualifier;			/* Byte 3 */
    unsigned short SequenceNumber;			/* Bytes 4-5 */
    unsigned char Dummy1[2];				/* Bytes 6-7 */
    DAC960_BusAddress32_T BusAddress;			/* Bytes 8-11 */
    unsigned char Dummy2[4];				/* Bytes 12-15 */
  } __attribute__ ((packed)) Type3E;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* Byte 0 */
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	/* Byte 1 */
    unsigned char Dummy1[2];				/* Bytes 2-3 */
    unsigned char RebuildRateConstant;			/* Byte 4 */
    unsigned char Dummy2[3];				/* Bytes 5-7 */
    DAC960_BusAddress32_T BusAddress;			/* Bytes 8-11 */
    unsigned char Dummy3[4];				/* Bytes 12-15 */
  } __attribute__ ((packed)) Type3R;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* Byte 0 */
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	/* Byte 1 */
    unsigned short TransferLength;			/* Bytes 2-3 */
    unsigned int LogicalBlockAddress;			/* Bytes 4-7 */
    DAC960_BusAddress32_T BusAddress;			/* Bytes 8-11 */
    unsigned char LogicalDriveNumber;			/* Byte 12 */
    unsigned char Dummy[3];				/* Bytes 13-15 */
  } __attribute__ ((packed)) Type4;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* Byte 0 */
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	/* Byte 1 */
    struct {
      unsigned short TransferLength:11;			/* Bytes 2-3 */
      unsigned char LogicalDriveNumber:5;		/* Byte 3 Bits 3-7 */
    } __attribute__ ((packed)) LD;
    unsigned int LogicalBlockAddress;			/* Bytes 4-7 */
    DAC960_BusAddress32_T BusAddress;			/* Bytes 8-11 */
    unsigned char ScatterGatherCount:6;			/* Byte 12 Bits 0-5 */
    enum {
      DAC960_V1_ScatterGather_32BitAddress_32BitByteCount = 0x0,
      DAC960_V1_ScatterGather_32BitAddress_16BitByteCount = 0x1,
      DAC960_V1_ScatterGather_32BitByteCount_32BitAddress = 0x2,
      DAC960_V1_ScatterGather_16BitByteCount_32BitAddress = 0x3
    } __attribute__ ((packed)) ScatterGatherType:2;	/* Byte 12 Bits 6-7 */
    unsigned char Dummy[3];				/* Bytes 13-15 */
  } __attribute__ ((packed)) Type5;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* Byte 0 */
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	/* Byte 1 */
    unsigned char CommandOpcode2;			/* Byte 2 */
    unsigned char :8;					/* Byte 3 */
    DAC960_BusAddress32_T CommandMailboxesBusAddress;	/* Bytes 4-7 */
    DAC960_BusAddress32_T StatusMailboxesBusAddress;	/* Bytes 8-11 */
    unsigned char Dummy[4];				/* Bytes 12-15 */
  } __attribute__ ((packed)) TypeX;
}
DAC960_V1_CommandMailbox_T;


/*
  Define the DAC960 V2 Firmware Command Opcodes.
*/

typedef enum
{
  DAC960_V2_MemCopy =				0x01,
  DAC960_V2_SCSI_10_Passthru =			0x02,
  DAC960_V2_SCSI_255_Passthru =			0x03,
  DAC960_V2_SCSI_10 =				0x04,
  DAC960_V2_SCSI_256 =				0x05,
  DAC960_V2_IOCTL =				0x20
}
__attribute__ ((packed))
DAC960_V2_CommandOpcode_T;


/*
  Define the DAC960 V2 Firmware IOCTL Opcodes.
*/

typedef enum
{
  DAC960_V2_GetControllerInfo =			0x01,
  DAC960_V2_GetLogicalDeviceInfoValid =		0x03,
  DAC960_V2_GetPhysicalDeviceInfoValid =	0x05,
  DAC960_V2_GetHealthStatus =			0x11,
  DAC960_V2_GetEvent =				0x15,
  DAC960_V2_StartDiscovery =			0x81,
  DAC960_V2_SetDeviceState =			0x82,
  DAC960_V2_RebuildDeviceStart =		0x88,
  DAC960_V2_RebuildDeviceStop =			0x89,
  DAC960_V2_ConsistencyCheckStart =		0x8C,
  DAC960_V2_ConsistencyCheckStop =		0x8D,
  DAC960_V2_SetMemoryMailbox =			0x8E,
  DAC960_V2_PauseDevice =			0x92,
  DAC960_V2_TranslatePhysicalToLogicalDevice =	0xC5
}
__attribute__ ((packed))
DAC960_V2_IOCTL_Opcode_T;


/*
  Define the DAC960 V2 Firmware Command Identifier type.
*/

typedef unsigned short DAC960_V2_CommandIdentifier_T;


/*
  Define the DAC960 V2 Firmware Command Status Codes.
*/

#define DAC960_V2_NormalCompletion		0x00
#define DAC960_V2_AbormalCompletion		0x02
#define DAC960_V2_DeviceBusy			0x08
#define DAC960_V2_DeviceNonresponsive		0x0E
#define DAC960_V2_DeviceNonresponsive2		0x0F
#define DAC960_V2_DeviceRevervationConflict	0x18

typedef unsigned char DAC960_V2_CommandStatus_T;


/*
  Define the DAC960 V2 Firmware Memory Type structure.
*/

typedef struct DAC960_V2_MemoryType
{
  enum {
    DAC960_V2_MemoryType_Reserved =		0x00,
    DAC960_V2_MemoryType_DRAM =			0x01,
    DAC960_V2_MemoryType_EDRAM =		0x02,
    DAC960_V2_MemoryType_EDO =			0x03,
    DAC960_V2_MemoryType_SDRAM =		0x04,
    DAC960_V2_MemoryType_Last =			0x1F
  } __attribute__ ((packed)) MemoryType:5;		/* Byte 0 Bits 0-4 */
  bool :1;						/* Byte 0 Bit 5 */
  bool MemoryParity:1;					/* Byte 0 Bit 6 */
  bool MemoryECC:1;					/* Byte 0 Bit 7 */
}
DAC960_V2_MemoryType_T;


/*
  Define the DAC960 V2 Firmware Processor Type structure.
*/

typedef enum
{
  DAC960_V2_ProcessorType_i960CA =		0x01,
  DAC960_V2_ProcessorType_i960RD =		0x02,
  DAC960_V2_ProcessorType_i960RN =		0x03,
  DAC960_V2_ProcessorType_i960RP =		0x04,
  DAC960_V2_ProcessorType_NorthBay =		0x05,
  DAC960_V2_ProcessorType_StrongArm =		0x06,
  DAC960_V2_ProcessorType_i960RM =		0x07
}
__attribute__ ((packed))
DAC960_V2_ProcessorType_T;


/*
  Define the DAC960 V2 Firmware Get Controller Info reply structure.
*/

typedef struct DAC960_V2_ControllerInfo
{
  unsigned char :8;					/* Byte 0 */
  enum {
    DAC960_V2_SCSI_Bus =			0x00,
    DAC960_V2_Fibre_Bus =			0x01,
    DAC960_V2_PCI_Bus =				0x03
  } __attribute__ ((packed)) BusInterfaceType;		/* Byte 1 */
  enum {
    DAC960_V2_DAC960E =				0x01,
    DAC960_V2_DAC960M =				0x08,
    DAC960_V2_DAC960PD =			0x10,
    DAC960_V2_DAC960PL =			0x11,
    DAC960_V2_DAC960PU =			0x12,
    DAC960_V2_DAC960PE =			0x13,
    DAC960_V2_DAC960PG =			0x14,
    DAC960_V2_DAC960PJ =			0x15,
    DAC960_V2_DAC960PTL0 =			0x16,
    DAC960_V2_DAC960PR =			0x17,
    DAC960_V2_DAC960PRL =			0x18,
    DAC960_V2_DAC960PT =			0x19,
    DAC960_V2_DAC1164P =			0x1A,
    DAC960_V2_DAC960PTL1 =			0x1B,
    DAC960_V2_EXR2000P =			0x1C,
    DAC960_V2_EXR3000P =			0x1D,
    DAC960_V2_AcceleRAID352 =			0x1E,
    DAC960_V2_AcceleRAID170 =			0x1F,
    DAC960_V2_AcceleRAID160 =			0x20,
    DAC960_V2_DAC960S =				0x60,
    DAC960_V2_DAC960SU =			0x61,
    DAC960_V2_DAC960SX =			0x62,
    DAC960_V2_DAC960SF =			0x63,
    DAC960_V2_DAC960SS =			0x64,
    DAC960_V2_DAC960FL =			0x65,
    DAC960_V2_DAC960LL =			0x66,
    DAC960_V2_DAC960FF =			0x67,
    DAC960_V2_DAC960HP =			0x68,
    DAC960_V2_RAIDBRICK =			0x69,
    DAC960_V2_METEOR_FL =			0x6A,
    DAC960_V2_METEOR_FF =			0x6B
  } __attribute__ ((packed)) ControllerType;		/* Byte 2 */
  unsigned char :8;					/* Byte 3 */
  unsigned short BusInterfaceSpeedMHz;			/* Bytes 4-5 */
  unsigned char BusWidthBits;				/* Byte 6 */
  unsigned char FlashCodeTypeOrProductID;		/* Byte 7 */
  unsigned char NumberOfHostPortsPresent;		/* Byte 8 */
  unsigned char Reserved1[7];				/* Bytes 9-15 */
  unsigned char BusInterfaceName[16];			/* Bytes 16-31 */
  unsigned char ControllerName[16];			/* Bytes 32-47 */
  unsigned char Reserved2[16];				/* Bytes 48-63 */
  /* Firmware Release Information */
  unsigned char FirmwareMajorVersion;			/* Byte 64 */
  unsigned char FirmwareMinorVersion;			/* Byte 65 */
  unsigned char FirmwareTurnNumber;			/* Byte 66 */
  unsigned char FirmwareBuildNumber;			/* Byte 67 */
  unsigned char FirmwareReleaseDay;			/* Byte 68 */
  unsigned char FirmwareReleaseMonth;			/* Byte 69 */
  unsigned char FirmwareReleaseYearHigh2Digits;		/* Byte 70 */
  unsigned char FirmwareReleaseYearLow2Digits;		/* Byte 71 */
  /* Hardware Release Information */
  unsigned char HardwareRevision;			/* Byte 72 */
  unsigned int :24;					/* Bytes 73-75 */
  unsigned char HardwareReleaseDay;			/* Byte 76 */
  unsigned char HardwareReleaseMonth;			/* Byte 77 */
  unsigned char HardwareReleaseYearHigh2Digits;		/* Byte 78 */
  unsigned char HardwareReleaseYearLow2Digits;		/* Byte 79 */
  /* Hardware Manufacturing Information */
  unsigned char ManufacturingBatchNumber;		/* Byte 80 */
  unsigned char :8;					/* Byte 81 */
  unsigned char ManufacturingPlantNumber;		/* Byte 82 */
  unsigned char :8;					/* Byte 83 */
  unsigned char HardwareManufacturingDay;		/* Byte 84 */
  unsigned char HardwareManufacturingMonth;		/* Byte 85 */
  unsigned char HardwareManufacturingYearHigh2Digits;	/* Byte 86 */
  unsigned char HardwareManufacturingYearLow2Digits;	/* Byte 87 */
  unsigned char MaximumNumberOfPDDperXLD;		/* Byte 88 */
  unsigned char MaximumNumberOfILDperXLD;		/* Byte 89 */
  unsigned short NonvolatileMemorySizeKB;		/* Bytes 90-91 */
  unsigned char MaximumNumberOfXLD;			/* Byte 92 */
  unsigned int :24;					/* Bytes 93-95 */
  /* Unique Information per Controller */
  unsigned char ControllerSerialNumber[16];		/* Bytes 96-111 */
  unsigned char Reserved3[16];				/* Bytes 112-127 */
  /* Vendor Information */
  unsigned int :24;					/* Bytes 128-130 */
  unsigned char OEM_Code;				/* Byte 131 */
  unsigned char VendorName[16];				/* Bytes 132-147 */
  /* Other Physical/Controller/Operation Information */
  bool BBU_Present:1;					/* Byte 148 Bit 0 */
  bool ActiveActiveClusteringMode:1;			/* Byte 148 Bit 1 */
  unsigned char :6;					/* Byte 148 Bits 2-7 */
  unsigned char :8;					/* Byte 149 */
  unsigned short :16;					/* Bytes 150-151 */
  /* Physical Device Scan Information */
  bool PhysicalScanActive:1;				/* Byte 152 Bit 0 */
  unsigned char :7;					/* Byte 152 Bits 1-7 */
  unsigned char PhysicalDeviceChannelNumber;		/* Byte 153 */
  unsigned char PhysicalDeviceTargetID;			/* Byte 154 */
  unsigned char PhysicalDeviceLogicalUnit;		/* Byte 155 */
  /* Maximum Command Data Transfer Sizes */
  unsigned short MaximumDataTransferSizeInBlocks;	/* Bytes 156-157 */
  unsigned short MaximumScatterGatherEntries;		/* Bytes 158-159 */
  /* Logical/Physical Device Counts */
  unsigned short LogicalDevicesPresent;			/* Bytes 160-161 */
  unsigned short LogicalDevicesCritical;		/* Bytes 162-163 */
  unsigned short LogicalDevicesOffline;			/* Bytes 164-165 */
  unsigned short PhysicalDevicesPresent;		/* Bytes 166-167 */
  unsigned short PhysicalDisksPresent;			/* Bytes 168-169 */
  unsigned short PhysicalDisksCritical;			/* Bytes 170-171 */
  unsigned short PhysicalDisksOffline;			/* Bytes 172-173 */
  unsigned short MaximumParallelCommands;		/* Bytes 174-175 */
  /* Channel and Target ID Information */
  unsigned char NumberOfPhysicalChannelsPresent;	/* Byte 176 */
  unsigned char NumberOfVirtualChannelsPresent;		/* Byte 177 */
  unsigned char NumberOfPhysicalChannelsPossible;	/* Byte 178 */
  unsigned char NumberOfVirtualChannelsPossible;	/* Byte 179 */
  unsigned char MaximumTargetsPerChannel[16];		/* Bytes 180-195 */
  unsigned char Reserved4[12];				/* Bytes 196-207 */
  /* Memory/Cache Information */
  unsigned short MemorySizeMB;				/* Bytes 208-209 */
  unsigned short CacheSizeMB;				/* Bytes 210-211 */
  unsigned int ValidCacheSizeInBytes;			/* Bytes 212-215 */
  unsigned int DirtyCacheSizeInBytes;			/* Bytes 216-219 */
  unsigned short MemorySpeedMHz;			/* Bytes 220-221 */
  unsigned char MemoryDataWidthBits;			/* Byte 222 */
  DAC960_V2_MemoryType_T MemoryType;			/* Byte 223 */
  unsigned char CacheMemoryTypeName[16];		/* Bytes 224-239 */
  /* Execution Memory Information */
  unsigned short ExecutionMemorySizeMB;			/* Bytes 240-241 */
  unsigned short ExecutionL2CacheSizeMB;		/* Bytes 242-243 */
  unsigned char Reserved5[8];				/* Bytes 244-251 */
  unsigned short ExecutionMemorySpeedMHz;		/* Bytes 252-253 */
  unsigned char ExecutionMemoryDataWidthBits;		/* Byte 254 */
  DAC960_V2_MemoryType_T ExecutionMemoryType;		/* Byte 255 */
  unsigned char ExecutionMemoryTypeName[16];		/* Bytes 256-271 */
  /* First CPU Type Information */
  unsigned short FirstProcessorSpeedMHz;		/* Bytes 272-273 */
  DAC960_V2_ProcessorType_T FirstProcessorType;		/* Byte 274 */
  unsigned char FirstProcessorCount;			/* Byte 275 */
  unsigned char Reserved6[12];				/* Bytes 276-287 */
  unsigned char FirstProcessorName[16];			/* Bytes 288-303 */
  /* Second CPU Type Information */
  unsigned short SecondProcessorSpeedMHz;		/* Bytes 304-305 */
  DAC960_V2_ProcessorType_T SecondProcessorType;	/* Byte 306 */
  unsigned char SecondProcessorCount;			/* Byte 307 */
  unsigned char Reserved7[12];				/* Bytes 308-319 */
  unsigned char SecondProcessorName[16];		/* Bytes 320-335 */
  /* Debugging/Profiling/Command Time Tracing Information */
  unsigned short CurrentProfilingDataPageNumber;	/* Bytes 336-337 */
  unsigned short ProgramsAwaitingProfilingData;		/* Bytes 338-339 */
  unsigned short CurrentCommandTimeTraceDataPageNumber;	/* Bytes 340-341 */
  unsigned short ProgramsAwaitingCommandTimeTraceData;	/* Bytes 342-343 */
  unsigned char Reserved8[8];				/* Bytes 344-351 */
  /* Error Counters on Physical Devices */
  unsigned short PhysicalDeviceBusResets;		/* Bytes 352-353 */
  unsigned short PhysicalDeviceParityErrors;		/* Bytes 355-355 */
  unsigned short PhysicalDeviceSoftErrors;		/* Bytes 356-357 */
  unsigned short PhysicalDeviceCommandsFailed;		/* Bytes 358-359 */
  unsigned short PhysicalDeviceMiscellaneousErrors;	/* Bytes 360-361 */
  unsigned short PhysicalDeviceCommandTimeouts;		/* Bytes 362-363 */
  unsigned short PhysicalDeviceSelectionTimeouts;	/* Bytes 364-365 */
  unsigned short PhysicalDeviceRetriesDone;		/* Bytes 366-367 */
  unsigned short PhysicalDeviceAbortsDone;		/* Bytes 368-369 */
  unsigned short PhysicalDeviceHostCommandAbortsDone;	/* Bytes 370-371 */
  unsigned short PhysicalDevicePredictedFailuresDetected; /* Bytes 372-373 */
  unsigned short PhysicalDeviceHostCommandsFailed;	/* Bytes 374-375 */
  unsigned short PhysicalDeviceHardErrors;		/* Bytes 376-377 */
  unsigned char Reserved9[6];				/* Bytes 378-383 */
  /* Error Counters on Logical Devices */
  unsigned short LogicalDeviceSoftErrors;		/* Bytes 384-385 */
  unsigned short LogicalDeviceCommandsFailed;		/* Bytes 386-387 */
  unsigned short LogicalDeviceHostCommandAbortsDone;	/* Bytes 388-389 */
  unsigned short :16;					/* Bytes 390-391 */
  /* Error Counters on Controller */
  unsigned short ControllerMemoryErrors;		/* Bytes 392-393 */
  unsigned short ControllerHostCommandAbortsDone;	/* Bytes 394-395 */
  unsigned int :32;					/* Bytes 396-399 */
  /* Long Duration Activity Information */
  unsigned short BackgroundInitializationsActive;	/* Bytes 400-401 */
  unsigned short LogicalDeviceInitializationsActive;	/* Bytes 402-403 */
  unsigned short PhysicalDeviceInitializationsActive;	/* Bytes 404-405 */
  unsigned short ConsistencyChecksActive;		/* Bytes 406-407 */
  unsigned short RebuildsActive;			/* Bytes 408-409 */
  unsigned short OnlineExpansionsActive;		/* Bytes 410-411 */
  unsigned short PatrolActivitiesActive;		/* Bytes 412-413 */
  unsigned short :16;					/* Bytes 414-415 */
  /* Flash ROM Information */
  unsigned char FlashType;				/* Byte 416 */
  unsigned char :8;					/* Byte 417 */
  unsigned short FlashSizeMB;				/* Bytes 418-419 */
  unsigned int FlashLimit;				/* Bytes 420-423 */
  unsigned int FlashCount;				/* Bytes 424-427 */
  unsigned int :32;					/* Bytes 428-431 */
  unsigned char FlashTypeName[16];			/* Bytes 432-447 */
  /* Firmware Run Time Information */
  unsigned char RebuildRate;				/* Byte 448 */
  unsigned char BackgroundInitializationRate;		/* Byte 449 */
  unsigned char ForegroundInitializationRate;		/* Byte 450 */
  unsigned char ConsistencyCheckRate;			/* Byte 451 */
  unsigned int :32;					/* Bytes 452-455 */
  unsigned int MaximumDP;				/* Bytes 456-459 */
  unsigned int FreeDP;					/* Bytes 460-463 */
  unsigned int MaximumIOP;				/* Bytes 464-467 */
  unsigned int FreeIOP;					/* Bytes 468-471 */
  unsigned short MaximumCombLengthInBlocks;		/* Bytes 472-473 */
  unsigned short NumberOfConfigurationGroups;		/* Bytes 474-475 */
  bool InstallationAbortStatus:1;			/* Byte 476 Bit 0 */
  bool MaintenanceModeStatus:1;				/* Byte 476 Bit 1 */
  unsigned int :24;					/* Bytes 476-479 */
  unsigned char Reserved10[32];				/* Bytes 480-511 */
  unsigned char Reserved11[512];			/* Bytes 512-1023 */
}
DAC960_V2_ControllerInfo_T;


/*
  Define the DAC960 V2 Firmware Logical Device State type.
*/

typedef enum
{
  DAC960_V2_LogicalDevice_Online =		0x01,
  DAC960_V2_LogicalDevice_Offline =		0x08,
  DAC960_V2_LogicalDevice_Critical =		0x09
}
__attribute__ ((packed))
DAC960_V2_LogicalDeviceState_T;


/*
  Define the DAC960 V2 Firmware Get Logical Device Info reply structure.
*/

typedef struct DAC960_V2_LogicalDeviceInfo
{
  unsigned char :8;					/* Byte 0 */
  unsigned char Channel;				/* Byte 1 */
  unsigned char TargetID;				/* Byte 2 */
  unsigned char LogicalUnit;				/* Byte 3 */
  DAC960_V2_LogicalDeviceState_T LogicalDeviceState;	/* Byte 4 */
  unsigned char RAIDLevel;				/* Byte 5 */
  unsigned char StripeSize;				/* Byte 6 */
  unsigned char CacheLineSize;				/* Byte 7 */
  struct {
    enum {
      DAC960_V2_ReadCacheDisabled =		0x0,
      DAC960_V2_ReadCacheEnabled =		0x1,
      DAC960_V2_ReadAheadEnabled =		0x2,
      DAC960_V2_IntelligentReadAheadEnabled =	0x3,
      DAC960_V2_ReadCache_Last =		0x7
    } __attribute__ ((packed)) ReadCache:3;		/* Byte 8 Bits 0-2 */
    enum {
      DAC960_V2_WriteCacheDisabled =		0x0,
      DAC960_V2_LogicalDeviceReadOnly =		0x1,
      DAC960_V2_WriteCacheEnabled =		0x2,
      DAC960_V2_IntelligentWriteCacheEnabled =	0x3,
      DAC960_V2_WriteCache_Last =		0x7
    } __attribute__ ((packed)) WriteCache:3;		/* Byte 8 Bits 3-5 */
    bool :1;						/* Byte 8 Bit 6 */
    bool LogicalDeviceInitialized:1;			/* Byte 8 Bit 7 */
  } LogicalDeviceControl;				/* Byte 8 */
  /* Logical Device Operations Status */
  bool ConsistencyCheckInProgress:1;			/* Byte 9 Bit 0 */
  bool RebuildInProgress:1;				/* Byte 9 Bit 1 */
  bool BackgroundInitializationInProgress:1;		/* Byte 9 Bit 2 */
  bool ForegroundInitializationInProgress:1;		/* Byte 9 Bit 3 */
  bool DataMigrationInProgress:1;			/* Byte 9 Bit 4 */
  bool PatrolOperationInProgress:1;			/* Byte 9 Bit 5 */
  unsigned char :2;					/* Byte 9 Bits 6-7 */
  unsigned char RAID5WriteUpdate;			/* Byte 10 */
  unsigned char RAID5Algorithm;				/* Byte 11 */
  unsigned short LogicalDeviceNumber;			/* Bytes 12-13 */
  /* BIOS Info */
  bool BIOSDisabled:1;					/* Byte 14 Bit 0 */
  bool CDROMBootEnabled:1;				/* Byte 14 Bit 1 */
  bool DriveCoercionEnabled:1;				/* Byte 14 Bit 2 */
  bool WriteSameDisabled:1;				/* Byte 14 Bit 3 */
  bool HBA_ModeEnabled:1;				/* Byte 14 Bit 4 */
  enum {
    DAC960_V2_Geometry_128_32 =			0x0,
    DAC960_V2_Geometry_255_63 =			0x1,
    DAC960_V2_Geometry_Reserved1 =		0x2,
    DAC960_V2_Geometry_Reserved2 =		0x3
  } __attribute__ ((packed)) DriveGeometry:2;		/* Byte 14 Bits 5-6 */
  bool SuperReadAheadEnabled:1;				/* Byte 14 Bit 7 */
  unsigned char :8;					/* Byte 15 */
  /* Error Counters */
  unsigned short SoftErrors;				/* Bytes 16-17 */
  unsigned short CommandsFailed;			/* Bytes 18-19 */
  unsigned short HostCommandAbortsDone;			/* Bytes 20-21 */
  unsigned short DeferredWriteErrors;			/* Bytes 22-23 */
  unsigned int :32;					/* Bytes 24-27 */
  unsigned int :32;					/* Bytes 28-31 */
  /* Device Size Information */
  unsigned short :16;					/* Bytes 32-33 */
  unsigned short DeviceBlockSizeInBytes;		/* Bytes 34-35 */
  unsigned int OriginalDeviceSize;			/* Bytes 36-39 */
  unsigned int ConfigurableDeviceSize;			/* Bytes 40-43 */
  unsigned int :32;					/* Bytes 44-47 */
  unsigned char LogicalDeviceName[32];			/* Bytes 48-79 */
  unsigned char SCSI_InquiryData[36];			/* Bytes 80-115 */
  unsigned char Reserved1[12];				/* Bytes 116-127 */
  DAC960_ByteCount64_T LastReadBlockNumber;		/* Bytes 128-135 */
  DAC960_ByteCount64_T LastWrittenBlockNumber;		/* Bytes 136-143 */
  DAC960_ByteCount64_T ConsistencyCheckBlockNumber;	/* Bytes 144-151 */
  DAC960_ByteCount64_T RebuildBlockNumber;		/* Bytes 152-159 */
  DAC960_ByteCount64_T BackgroundInitializationBlockNumber; /* Bytes 160-167 */
  DAC960_ByteCount64_T ForegroundInitializationBlockNumber; /* Bytes 168-175 */
  DAC960_ByteCount64_T DataMigrationBlockNumber;	/* Bytes 176-183 */
  DAC960_ByteCount64_T PatrolOperationBlockNumber;	/* Bytes 184-191 */
  unsigned char Reserved2[64];				/* Bytes 192-255 */
}
DAC960_V2_LogicalDeviceInfo_T;


/*
  Define the DAC960 V2 Firmware Physical Device State type.
*/

typedef enum
{
    DAC960_V2_Device_Unconfigured =		0x00,
    DAC960_V2_Device_Online =			0x01,
    DAC960_V2_Device_Rebuild =			0x03,
    DAC960_V2_Device_Missing =			0x04,
    DAC960_V2_Device_Critical =			0x05,
    DAC960_V2_Device_Dead =			0x08,
    DAC960_V2_Device_SuspectedDead =		0x0C,
    DAC960_V2_Device_CommandedOffline =		0x10,
    DAC960_V2_Device_Standby =			0x21,
    DAC960_V2_Device_InvalidState =		0xFF
}
__attribute__ ((packed))
DAC960_V2_PhysicalDeviceState_T;


/*
  Define the DAC960 V2 Firmware Get Physical Device Info reply structure.
*/

typedef struct DAC960_V2_PhysicalDeviceInfo
{
  unsigned char :8;					/* Byte 0 */
  unsigned char Channel;				/* Byte 1 */
  unsigned char TargetID;				/* Byte 2 */
  unsigned char LogicalUnit;				/* Byte 3 */
  /* Configuration Status Bits */
  bool PhysicalDeviceFaultTolerant:1;			/* Byte 4 Bit 0 */
  bool PhysicalDeviceConnected:1;			/* Byte 4 Bit 1 */
  bool PhysicalDeviceLocalToController:1;		/* Byte 4 Bit 2 */
  unsigned char :5;					/* Byte 4 Bits 3-7 */
  /* Multiple Host/Controller Status Bits */
  bool RemoteHostSystemDead:1;				/* Byte 5 Bit 0 */
  bool RemoteControllerDead:1;				/* Byte 5 Bit 1 */
  unsigned char :6;					/* Byte 5 Bits 2-7 */
  DAC960_V2_PhysicalDeviceState_T PhysicalDeviceState;	/* Byte 6 */
  unsigned char NegotiatedDataWidthBits;		/* Byte 7 */
  unsigned short NegotiatedSynchronousMegaTransfers;	/* Bytes 8-9 */
  /* Multiported Physical Device Information */
  unsigned char NumberOfPortConnections;		/* Byte 10 */
  unsigned char DriveAccessibilityBitmap;		/* Byte 11 */
  unsigned int :32;					/* Bytes 12-15 */
  unsigned char NetworkAddress[16];			/* Bytes 16-31 */
  unsigned short MaximumTags;				/* Bytes 32-33 */
  /* Physical Device Operations Status */
  bool ConsistencyCheckInProgress:1;			/* Byte 34 Bit 0 */
  bool RebuildInProgress:1;				/* Byte 34 Bit 1 */
  bool MakingDataConsistentInProgress:1;		/* Byte 34 Bit 2 */
  bool PhysicalDeviceInitializationInProgress:1;	/* Byte 34 Bit 3 */
  bool DataMigrationInProgress:1;			/* Byte 34 Bit 4 */
  bool PatrolOperationInProgress:1;			/* Byte 34 Bit 5 */
  unsigned char :2;					/* Byte 34 Bits 6-7 */
  unsigned char LongOperationStatus;			/* Byte 35 */
  unsigned char ParityErrors;				/* Byte 36 */
  unsigned char SoftErrors;				/* Byte 37 */
  unsigned char HardErrors;				/* Byte 38 */
  unsigned char MiscellaneousErrors;			/* Byte 39 */
  unsigned char CommandTimeouts;			/* Byte 40 */
  unsigned char Retries;				/* Byte 41 */
  unsigned char Aborts;					/* Byte 42 */
  unsigned char PredictedFailuresDetected;		/* Byte 43 */
  unsigned int :32;					/* Bytes 44-47 */
  unsigned short :16;					/* Bytes 48-49 */
  unsigned short DeviceBlockSizeInBytes;		/* Bytes 50-51 */
  unsigned int OriginalDeviceSize;			/* Bytes 52-55 */
  unsigned int ConfigurableDeviceSize;			/* Bytes 56-59 */
  unsigned int :32;					/* Bytes 60-63 */
  unsigned char PhysicalDeviceName[16];			/* Bytes 64-79 */
  unsigned char Reserved1[16];				/* Bytes 80-95 */
  unsigned char Reserved2[32];				/* Bytes 96-127 */
  unsigned char SCSI_InquiryData[36];			/* Bytes 128-163 */
  unsigned char Reserved3[20];				/* Bytes 164-183 */
  unsigned char Reserved4[8];				/* Bytes 184-191 */
  DAC960_ByteCount64_T LastReadBlockNumber;		/* Bytes 192-199 */
  DAC960_ByteCount64_T LastWrittenBlockNumber;		/* Bytes 200-207 */
  DAC960_ByteCount64_T ConsistencyCheckBlockNumber;	/* Bytes 208-215 */
  DAC960_ByteCount64_T RebuildBlockNumber;		/* Bytes 216-223 */
  DAC960_ByteCount64_T MakingDataConsistentBlockNumber;	/* Bytes 224-231 */
  DAC960_ByteCount64_T DeviceInitializationBlockNumber; /* Bytes 232-239 */
  DAC960_ByteCount64_T DataMigrationBlockNumber;	/* Bytes 240-247 */
  DAC960_ByteCount64_T PatrolOperationBlockNumber;	/* Bytes 248-255 */
  unsigned char Reserved5[256];				/* Bytes 256-511 */
}
DAC960_V2_PhysicalDeviceInfo_T;


/*
  Define the DAC960 V2 Firmware Health Status Buffer structure.
*/

typedef struct DAC960_V2_HealthStatusBuffer
{
  unsigned int MicrosecondsFromControllerStartTime;	/* Bytes 0-3 */
  unsigned int MillisecondsFromControllerStartTime;	/* Bytes 4-7 */
  unsigned int SecondsFrom1January1970;			/* Bytes 8-11 */
  unsigned int :32;					/* Bytes 12-15 */
  unsigned int StatusChangeCounter;			/* Bytes 16-19 */
  unsigned int :32;					/* Bytes 20-23 */
  unsigned int DebugOutputMessageBufferIndex;		/* Bytes 24-27 */
  unsigned int CodedMessageBufferIndex;			/* Bytes 28-31 */
  unsigned int CurrentTimeTracePageNumber;		/* Bytes 32-35 */
  unsigned int CurrentProfilerPageNumber;		/* Bytes 36-39 */
  unsigned int NextEventSequenceNumber;			/* Bytes 40-43 */
  unsigned int :32;					/* Bytes 44-47 */
  unsigned char Reserved1[16];				/* Bytes 48-63 */
  unsigned char Reserved2[64];				/* Bytes 64-127 */
}
DAC960_V2_HealthStatusBuffer_T;


/*
  Define the DAC960 V2 Firmware Get Event reply structure.
*/

typedef struct DAC960_V2_Event
{
  unsigned int EventSequenceNumber;			/* Bytes 0-3 */
  unsigned int EventTime;				/* Bytes 4-7 */
  unsigned int EventCode;				/* Bytes 8-11 */
  unsigned char :8;					/* Byte 12 */
  unsigned char Channel;				/* Byte 13 */
  unsigned char TargetID;				/* Byte 14 */
  unsigned char LogicalUnit;				/* Byte 15 */
  unsigned int :32;					/* Bytes 16-19 */
  unsigned int EventSpecificParameter;			/* Bytes 20-23 */
  unsigned char RequestSenseData[40];			/* Bytes 24-63 */
}
DAC960_V2_Event_T;


/*
  Define the DAC960 V2 Firmware Command Control Bits structure.
*/

typedef struct DAC960_V2_CommandControlBits
{
  bool ForceUnitAccess:1;				/* Byte 0 Bit 0 */
  bool DisablePageOut:1;				/* Byte 0 Bit 1 */
  bool :1;						/* Byte 0 Bit 2 */
  bool AdditionalScatterGatherListMemory:1;		/* Byte 0 Bit 3 */
  bool DataTransferControllerToHost:1;			/* Byte 0 Bit 4 */
  bool :1;						/* Byte 0 Bit 5 */
  bool NoAutoRequestSense:1;				/* Byte 0 Bit 6 */
  bool DisconnectProhibited:1;				/* Byte 0 Bit 7 */
}
DAC960_V2_CommandControlBits_T;


/*
  Define the DAC960 V2 Firmware Command Timeout structure.
*/

typedef struct DAC960_V2_CommandTimeout
{
  unsigned char TimeoutValue:6;				/* Byte 0 Bits 0-5 */
  enum {
    DAC960_V2_TimeoutScale_Seconds =		0,
    DAC960_V2_TimeoutScale_Minutes =		1,
    DAC960_V2_TimeoutScale_Hours =		2,
    DAC960_V2_TimeoutScale_Reserved =		3
  } __attribute__ ((packed)) TimeoutScale:2;		/* Byte 0 Bits 6-7 */
}
DAC960_V2_CommandTimeout_T;


/*
  Define the DAC960 V2 Firmware Physical Device structure.
*/

typedef struct DAC960_V2_PhysicalDevice
{
  unsigned char LogicalUnit;				/* Byte 0 */
  unsigned char TargetID;				/* Byte 1 */
  unsigned char Channel:3;				/* Byte 2 Bits 0-2 */
  unsigned char Controller:5;				/* Byte 2 Bits 3-7 */
}
__attribute__ ((packed))
DAC960_V2_PhysicalDevice_T;


/*
  Define the DAC960 V2 Firmware Logical Device structure.
*/

typedef struct DAC960_V2_LogicalDevice
{
  unsigned short LogicalDeviceNumber;			/* Bytes 0-1 */
  unsigned char :3;					/* Byte 2 Bits 0-2 */
  unsigned char Controller:5;				/* Byte 2 Bits 3-7 */
}
__attribute__ ((packed))
DAC960_V2_LogicalDevice_T;


/*
  Define the DAC960 V2 Firmware Operation Device type.
*/

typedef enum
{
  DAC960_V2_Physical_Device =			0x00,
  DAC960_V2_RAID_Device =			0x01,
  DAC960_V2_Physical_Channel =			0x02,
  DAC960_V2_RAID_Channel =			0x03,
  DAC960_V2_Physical_Controller =		0x04,
  DAC960_V2_RAID_Controller =			0x05,
  DAC960_V2_Configuration_Group =		0x10,
  DAC960_V2_Enclosure =				0x11
}
__attribute__ ((packed))
DAC960_V2_OperationDevice_T;


/*
  Define the DAC960 V2 Firmware Translate Physical To Logical Device structure.
*/

typedef struct DAC960_V2_PhysicalToLogicalDevice
{
  unsigned short LogicalDeviceNumber;			/* Bytes 0-1 */
  unsigned short :16;					/* Bytes 2-3 */
  unsigned char PreviousBootController;			/* Byte 4 */
  unsigned char PreviousBootChannel;			/* Byte 5 */
  unsigned char PreviousBootTargetID;			/* Byte 6 */
  unsigned char PreviousBootLogicalUnit;		/* Byte 7 */
}
DAC960_V2_PhysicalToLogicalDevice_T;



/*
  Define the DAC960 V2 Firmware Scatter/Gather List Entry structure.
*/

typedef struct DAC960_V2_ScatterGatherSegment
{
  DAC960_BusAddress64_T SegmentDataPointer;		/* Bytes 0-7 */
  DAC960_ByteCount64_T SegmentByteCount;		/* Bytes 8-15 */
}
DAC960_V2_ScatterGatherSegment_T;


/*
  Define the DAC960 V2 Firmware Data Transfer Memory Address structure.
*/

typedef union DAC960_V2_DataTransferMemoryAddress
{
  DAC960_V2_ScatterGatherSegment_T ScatterGatherSegments[2]; /* Bytes 0-31 */
  struct {
    unsigned short ScatterGatherList0Length;		/* Bytes 0-1 */
    unsigned short ScatterGatherList1Length;		/* Bytes 2-3 */
    unsigned short ScatterGatherList2Length;		/* Bytes 4-5 */
    unsigned short :16;					/* Bytes 6-7 */
    DAC960_BusAddress64_T ScatterGatherList0Address;	/* Bytes 8-15 */
    DAC960_BusAddress64_T ScatterGatherList1Address;	/* Bytes 16-23 */
    DAC960_BusAddress64_T ScatterGatherList2Address;	/* Bytes 24-31 */
  } ExtendedScatterGather;
}
DAC960_V2_DataTransferMemoryAddress_T;


/*
  Define the 64 Byte DAC960 V2 Firmware Command Mailbox structure.
*/

typedef union DAC960_V2_CommandMailbox
{
  unsigned int Words[16];				/* Words 0-15 */
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	/* Bytes 0-1 */
    DAC960_V2_CommandOpcode_T CommandOpcode;		/* Byte 2 */
    DAC960_V2_CommandControlBits_T CommandControlBits;	/* Byte 3 */
    DAC960_ByteCount32_T DataTransferSize:24;		/* Bytes 4-6 */
    unsigned char DataTransferPageNumber;		/* Byte 7 */
    DAC960_BusAddress64_T RequestSenseBusAddress;	/* Bytes 8-15 */
    unsigned int :24;					/* Bytes 16-18 */
    DAC960_V2_CommandTimeout_T CommandTimeout;		/* Byte 19 */
    unsigned char RequestSenseSize;			/* Byte 20 */
    unsigned char IOCTL_Opcode;				/* Byte 21 */
    unsigned char Reserved[10];				/* Bytes 22-31 */
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			/* Bytes 32-63 */
  } Common;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	/* Bytes 0-1 */
    DAC960_V2_CommandOpcode_T CommandOpcode;		/* Byte 2 */
    DAC960_V2_CommandControlBits_T CommandControlBits;	/* Byte 3 */
    DAC960_ByteCount32_T DataTransferSize;		/* Bytes 4-7 */
    DAC960_BusAddress64_T RequestSenseBusAddress;	/* Bytes 8-15 */
    DAC960_V2_PhysicalDevice_T PhysicalDevice;		/* Bytes 16-18 */
    DAC960_V2_CommandTimeout_T CommandTimeout;		/* Byte 19 */
    unsigned char RequestSenseSize;			/* Byte 20 */
    unsigned char CDBLength;				/* Byte 21 */
    unsigned char SCSI_CDB[10];				/* Bytes 22-31 */
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			/* Bytes 32-63 */
  } SCSI_10;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	/* Bytes 0-1 */
    DAC960_V2_CommandOpcode_T CommandOpcode;		/* Byte 2 */
    DAC960_V2_CommandControlBits_T CommandControlBits;	/* Byte 3 */
    DAC960_ByteCount32_T DataTransferSize;		/* Bytes 4-7 */
    DAC960_BusAddress64_T RequestSenseBusAddress;	/* Bytes 8-15 */
    DAC960_V2_PhysicalDevice_T PhysicalDevice;		/* Bytes 16-18 */
    DAC960_V2_CommandTimeout_T CommandTimeout;		/* Byte 19 */
    unsigned char RequestSenseSize;			/* Byte 20 */
    unsigned char CDBLength;				/* Byte 21 */
    unsigned short :16;					/* Bytes 22-23 */
    DAC960_BusAddress64_T SCSI_CDB_BusAddress;		/* Bytes 24-31 */
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			/* Bytes 32-63 */
  } SCSI_255;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	/* Bytes 0-1 */
    DAC960_V2_CommandOpcode_T CommandOpcode;		/* Byte 2 */
    DAC960_V2_CommandControlBits_T CommandControlBits;	/* Byte 3 */
    DAC960_ByteCount32_T DataTransferSize:24;		/* Bytes 4-6 */
    unsigned char DataTransferPageNumber;		/* Byte 7 */
    DAC960_BusAddress64_T RequestSenseBusAddress;	/* Bytes 8-15 */
    unsigned short :16;					/* Bytes 16-17 */
    unsigned char ControllerNumber;			/* Byte 18 */
    DAC960_V2_CommandTimeout_T CommandTimeout;		/* Byte 19 */
    unsigned char RequestSenseSize;			/* Byte 20 */
    unsigned char IOCTL_Opcode;				/* Byte 21 */
    unsigned char Reserved[10];				/* Bytes 22-31 */
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			/* Bytes 32-63 */
  } ControllerInfo;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	/* Bytes 0-1 */
    DAC960_V2_CommandOpcode_T CommandOpcode;		/* Byte 2 */
    DAC960_V2_CommandControlBits_T CommandControlBits;	/* Byte 3 */
    DAC960_ByteCount32_T DataTransferSize:24;		/* Bytes 4-6 */
    unsigned char DataTransferPageNumber;		/* Byte 7 */
    DAC960_BusAddress64_T RequestSenseBusAddress;	/* Bytes 8-15 */
    DAC960_V2_LogicalDevice_T LogicalDevice;		/* Bytes 16-18 */
    DAC960_V2_CommandTimeout_T CommandTimeout;		/* Byte 19 */
    unsigned char RequestSenseSize;			/* Byte 20 */
    unsigned char IOCTL_Opcode;				/* Byte 21 */
    unsigned char Reserved[10];				/* Bytes 22-31 */
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			/* Bytes 32-63 */
  } LogicalDeviceInfo;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	/* Bytes 0-1 */
    DAC960_V2_CommandOpcode_T CommandOpcode;		/* Byte 2 */
    DAC960_V2_CommandControlBits_T CommandControlBits;	/* Byte 3 */
    DAC960_ByteCount32_T DataTransferSize:24;		/* Bytes 4-6 */
    unsigned char DataTransferPageNumber;		/* Byte 7 */
    DAC960_BusAddress64_T RequestSenseBusAddress;	/* Bytes 8-15 */
    DAC960_V2_PhysicalDevice_T PhysicalDevice;		/* Bytes 16-18 */
    DAC960_V2_CommandTimeout_T CommandTimeout;		/* Byte 19 */
    unsigned char RequestSenseSize;			/* Byte 20 */
    unsigned char IOCTL_Opcode;				/* Byte 21 */
    unsigned char Reserved[10];				/* Bytes 22-31 */
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			/* Bytes 32-63 */
  } PhysicalDeviceInfo;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	/* Bytes 0-1 */
    DAC960_V2_CommandOpcode_T CommandOpcode;		/* Byte 2 */
    DAC960_V2_CommandControlBits_T CommandControlBits;	/* Byte 3 */
    DAC960_ByteCount32_T DataTransferSize:24;		/* Bytes 4-6 */
    unsigned char DataTransferPageNumber;		/* Byte 7 */
    DAC960_BusAddress64_T RequestSenseBusAddress;	/* Bytes 8-15 */
    unsigned short EventSequenceNumberHigh16;		/* Bytes 16-17 */
    unsigned char ControllerNumber;			/* Byte 18 */
    DAC960_V2_CommandTimeout_T CommandTimeout;		/* Byte 19 */
    unsigned char RequestSenseSize;			/* Byte 20 */
    unsigned char IOCTL_Opcode;				/* Byte 21 */
    unsigned short EventSequenceNumberLow16;		/* Bytes 22-23 */
    unsigned char Reserved[8];				/* Bytes 24-31 */
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			/* Bytes 32-63 */
  } GetEvent;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	/* Bytes 0-1 */
    DAC960_V2_CommandOpcode_T CommandOpcode;		/* Byte 2 */
    DAC960_V2_CommandControlBits_T CommandControlBits;	/* Byte 3 */
    DAC960_ByteCount32_T DataTransferSize:24;		/* Bytes 4-6 */
    unsigned char DataTransferPageNumber;		/* Byte 7 */
    DAC960_BusAddress64_T RequestSenseBusAddress;	/* Bytes 8-15 */
    DAC960_V2_LogicalDevice_T LogicalDevice;		/* Bytes 16-18 */
    DAC960_V2_CommandTimeout_T CommandTimeout;		/* Byte 19 */
    unsigned char RequestSenseSize;			/* Byte 20 */
    unsigned char IOCTL_Opcode;				/* Byte 21 */
    union {
      DAC960_V2_LogicalDeviceState_T LogicalDeviceState;
      DAC960_V2_PhysicalDeviceState_T PhysicalDeviceState;
    } DeviceState;					/* Byte 22 */
    unsigned char Reserved[9];				/* Bytes 23-31 */
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			/* Bytes 32-63 */
  } SetDeviceState;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	/* Bytes 0-1 */
    DAC960_V2_CommandOpcode_T CommandOpcode;		/* Byte 2 */
    DAC960_V2_CommandControlBits_T CommandControlBits;	/* Byte 3 */
    DAC960_ByteCount32_T DataTransferSize:24;		/* Bytes 4-6 */
    unsigned char DataTransferPageNumber;		/* Byte 7 */
    DAC960_BusAddress64_T RequestSenseBusAddress;	/* Bytes 8-15 */
    DAC960_V2_LogicalDevice_T LogicalDevice;		/* Bytes 16-18 */
    DAC960_V2_CommandTimeout_T CommandTimeout;		/* Byte 19 */
    unsigned char RequestSenseSize;			/* Byte 20 */
    unsigned char IOCTL_Opcode;				/* Byte 21 */
    bool RestoreConsistency:1;				/* Byte 22 Bit 0 */
    bool InitializedAreaOnly:1;				/* Byte 22 Bit 1 */
    unsigned char :6;					/* Byte 22 Bits 2-7 */
    unsigned char Reserved[9];				/* Bytes 23-31 */
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			/* Bytes 32-63 */
  } ConsistencyCheck;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	/* Bytes 0-1 */
    DAC960_V2_CommandOpcode_T CommandOpcode;		/* Byte 2 */
    DAC960_V2_CommandControlBits_T CommandControlBits;	/* Byte 3 */
    unsigned char FirstCommandMailboxSizeKB;		/* Byte 4 */
    unsigned char FirstStatusMailboxSizeKB;		/* Byte 5 */
    unsigned char SecondCommandMailboxSizeKB;		/* Byte 6 */
    unsigned char SecondStatusMailboxSizeKB;		/* Byte 7 */
    DAC960_BusAddress64_T RequestSenseBusAddress;	/* Bytes 8-15 */
    unsigned int :24;					/* Bytes 16-18 */
    DAC960_V2_CommandTimeout_T CommandTimeout;		/* Byte 19 */
    unsigned char RequestSenseSize;			/* Byte 20 */
    unsigned char IOCTL_Opcode;				/* Byte 21 */
    unsigned char HealthStatusBufferSizeKB;		/* Byte 22 */
    unsigned char :8;					/* Byte 23 */
    DAC960_BusAddress64_T HealthStatusBufferBusAddress; /* Bytes 24-31 */
    DAC960_BusAddress64_T FirstCommandMailboxBusAddress; /* Bytes 32-39 */
    DAC960_BusAddress64_T FirstStatusMailboxBusAddress; /* Bytes 40-47 */
    DAC960_BusAddress64_T SecondCommandMailboxBusAddress; /* Bytes 48-55 */
    DAC960_BusAddress64_T SecondStatusMailboxBusAddress; /* Bytes 56-63 */
  } SetMemoryMailbox;
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	/* Bytes 0-1 */
    DAC960_V2_CommandOpcode_T CommandOpcode;		/* Byte 2 */
    DAC960_V2_CommandControlBits_T CommandControlBits;	/* Byte 3 */
    DAC960_ByteCount32_T DataTransferSize:24;		/* Bytes 4-6 */
    unsigned char DataTransferPageNumber;		/* Byte 7 */
    DAC960_BusAddress64_T RequestSenseBusAddress;	/* Bytes 8-15 */
    DAC960_V2_PhysicalDevice_T PhysicalDevice;		/* Bytes 16-18 */
    DAC960_V2_CommandTimeout_T CommandTimeout;		/* Byte 19 */
    unsigned char RequestSenseSize;			/* Byte 20 */
    unsigned char IOCTL_Opcode;				/* Byte 21 */
    DAC960_V2_OperationDevice_T OperationDevice;	/* Byte 22 */
    unsigned char Reserved[9];				/* Bytes 23-31 */
    DAC960_V2_DataTransferMemoryAddress_T
      DataTransferMemoryAddress;			/* Bytes 32-63 */
  } DeviceOperation;
}
DAC960_V2_CommandMailbox_T;


/*
  Define the DAC960 Driver IOCTL requests.
*/

#define DAC960_IOCTL_GET_CONTROLLER_COUNT	0xDAC001
#define DAC960_IOCTL_GET_CONTROLLER_INFO	0xDAC002
#define DAC960_IOCTL_V1_EXECUTE_COMMAND		0xDAC003
#define DAC960_IOCTL_V2_EXECUTE_COMMAND		0xDAC004
#define DAC960_IOCTL_V2_GET_HEALTH_STATUS	0xDAC005


/*
  Define the DAC960_IOCTL_GET_CONTROLLER_INFO reply structure.
*/

typedef struct DAC960_ControllerInfo
{
  unsigned char ControllerNumber;
  unsigned char FirmwareType;
  unsigned char Channels;
  unsigned char Targets;
  unsigned char PCI_Bus;
  unsigned char PCI_Device;
  unsigned char PCI_Function;
  unsigned char IRQ_Channel;
  DAC960_PCI_Address_T PCI_Address;
  unsigned char ModelName[20];
  unsigned char FirmwareVersion[12];
}
DAC960_ControllerInfo_T;


/*
  Define the User Mode DAC960_IOCTL_V1_EXECUTE_COMMAND request structure.
*/

typedef struct DAC960_V1_UserCommand
{
  unsigned char ControllerNumber;
  DAC960_V1_CommandMailbox_T CommandMailbox;
  int DataTransferLength;
  void __user *DataTransferBuffer;
  DAC960_V1_DCDB_T __user *DCDB;
}
DAC960_V1_UserCommand_T;


/*
  Define the Kernel Mode DAC960_IOCTL_V1_EXECUTE_COMMAND request structure.
*/

typedef struct DAC960_V1_KernelCommand
{
  unsigned char ControllerNumber;
  DAC960_V1_CommandMailbox_T CommandMailbox;
  int DataTransferLength;
  void *DataTransferBuffer;
  DAC960_V1_DCDB_T *DCDB;
  DAC960_V1_CommandStatus_T CommandStatus;
  void (*CompletionFunction)(struct DAC960_V1_KernelCommand *);
  void *CompletionData;
}
DAC960_V1_KernelCommand_T;


/*
  Define the User Mode DAC960_IOCTL_V2_EXECUTE_COMMAND request structure.
*/

typedef struct DAC960_V2_UserCommand
{
  unsigned char ControllerNumber;
  DAC960_V2_CommandMailbox_T CommandMailbox;
  int DataTransferLength;
  int RequestSenseLength;
  void __user *DataTransferBuffer;
  void __user *RequestSenseBuffer;
}
DAC960_V2_UserCommand_T;


/*
  Define the Kernel Mode DAC960_IOCTL_V2_EXECUTE_COMMAND request structure.
*/

typedef struct DAC960_V2_KernelCommand
{
  unsigned char ControllerNumber;
  DAC960_V2_CommandMailbox_T CommandMailbox;
  int DataTransferLength;
  int RequestSenseLength;
  void *DataTransferBuffer;
  void *RequestSenseBuffer;
  DAC960_V2_CommandStatus_T CommandStatus;
  void (*CompletionFunction)(struct DAC960_V2_KernelCommand *);
  void *CompletionData;
}
DAC960_V2_KernelCommand_T;


/*
  Define the User Mode DAC960_IOCTL_V2_GET_HEALTH_STATUS request structure.
*/

typedef struct DAC960_V2_GetHealthStatus
{
  unsigned char ControllerNumber;
  DAC960_V2_HealthStatusBuffer_T __user *HealthStatusBuffer;
}
DAC960_V2_GetHealthStatus_T;


/*
  Import the Kernel Mode IOCTL interface.
*/

extern int DAC960_KernelIOCTL(unsigned int Request, void *Argument);


/*
  DAC960_DriverVersion protects the private portion of this file.
*/

#ifdef DAC960_DriverVersion


/*
  Define the maximum Driver Queue Depth and Controller Queue Depth supported
  by DAC960 V1 and V2 Firmware Controllers.
*/

#define DAC960_MaxDriverQueueDepth		511
#define DAC960_MaxControllerQueueDepth		512


/*
  Define the maximum number of Scatter/Gather Segments supported for any
  DAC960 V1 and V2 Firmware controller.
*/

#define DAC960_V1_ScatterGatherLimit		33
#define DAC960_V2_ScatterGatherLimit		128


/*
  Define the number of Command Mailboxes and Status Mailboxes used by the
  DAC960 V1 and V2 Firmware Memory Mailbox Interface.
*/

#define DAC960_V1_CommandMailboxCount		256
#define DAC960_V1_StatusMailboxCount		1024
#define DAC960_V2_CommandMailboxCount		512
#define DAC960_V2_StatusMailboxCount		512


/*
  Define the DAC960 Controller Monitoring Timer Interval.
*/

#define DAC960_MonitoringTimerInterval		(10 * HZ)


/*
  Define the DAC960 Controller Secondary Monitoring Interval.
*/

#define DAC960_SecondaryMonitoringInterval	(60 * HZ)


/*
  Define the DAC960 Controller Health Status Monitoring Interval.
*/

#define DAC960_HealthStatusMonitoringInterval	(1 * HZ)


/*
  Define the DAC960 Controller Progress Reporting Interval.
*/

#define DAC960_ProgressReportingInterval	(60 * HZ)


/*
  Define the maximum number of Partitions allowed for each Logical Drive.
*/

#define DAC960_MaxPartitions			8
#define DAC960_MaxPartitionsBits		3

/*
  Define the DAC960 Controller fixed Block Size and Block Size Bits.
*/

#define DAC960_BlockSize			512
#define DAC960_BlockSizeBits			9


/*
  Define the number of Command structures that should be allocated as a
  group to optimize kernel memory allocation.
*/

#define DAC960_V1_CommandAllocationGroupSize	11
#define DAC960_V2_CommandAllocationGroupSize	29


/*
  Define the Controller Line Buffer, Progress Buffer, User Message, and
  Initial Status Buffer sizes.
*/

#define DAC960_LineBufferSize			100
#define DAC960_ProgressBufferSize		200
#define DAC960_UserMessageSize			200
#define DAC960_InitialStatusBufferSize		(8192-32)


/*
  Define the DAC960 Controller Firmware Types.
*/

typedef enum
{
  DAC960_V1_Controller =			1,
  DAC960_V2_Controller =			2
}
DAC960_FirmwareType_T;


/*
  Define the DAC960 Controller Hardware Types.
*/

typedef enum
{
  DAC960_BA_Controller =			1,	/* eXtremeRAID 2000 */
  DAC960_LP_Controller =			2,	/* AcceleRAID 352 */
  DAC960_LA_Controller =			3,	/* DAC1164P */
  DAC960_PG_Controller =			4,	/* DAC960PTL/PJ/PG */
  DAC960_PD_Controller =			5,	/* DAC960PU/PD/PL/P */
  DAC960_P_Controller =				6,	/* DAC960PU/PD/PL/P */
  DAC960_GEM_Controller =			7,	/* AcceleRAID 4/5/600 */
}
DAC960_HardwareType_T;


/*
  Define the Driver Message Levels.
*/

typedef enum DAC960_MessageLevel
{
  DAC960_AnnounceLevel =			0,
  DAC960_InfoLevel =				1,
  DAC960_NoticeLevel =				2,
  DAC960_WarningLevel =				3,
  DAC960_ErrorLevel =				4,
  DAC960_ProgressLevel =			5,
  DAC960_CriticalLevel =			6,
  DAC960_UserCriticalLevel =			7
}
DAC960_MessageLevel_T;

static char
  *DAC960_MessageLevelMap[] =
    { KERN_NOTICE, KERN_NOTICE, KERN_NOTICE, KERN_WARNING,
      KERN_ERR, KERN_CRIT, KERN_CRIT, KERN_CRIT };


/*
  Define Driver Message macros.
*/

#define DAC960_Announce(Format, Arguments...) \
  DAC960_Message(DAC960_AnnounceLevel, Format, ##Arguments)

#define DAC960_Info(Format, Arguments...) \
  DAC960_Message(DAC960_InfoLevel, Format, ##Arguments)

#define DAC960_Notice(Format, Arguments...) \
  DAC960_Message(DAC960_NoticeLevel, Format, ##Arguments)

#define DAC960_Warning(Format, Arguments...) \
  DAC960_Message(DAC960_WarningLevel, Format, ##Arguments)

#define DAC960_Error(Format, Arguments...) \
  DAC960_Message(DAC960_ErrorLevel, Format, ##Arguments)

#define DAC960_Progress(Format, Arguments...) \
  DAC960_Message(DAC960_ProgressLevel, Format, ##Arguments)

#define DAC960_Critical(Format, Arguments...) \
  DAC960_Message(DAC960_CriticalLevel, Format, ##Arguments)

#define DAC960_UserCritical(Format, Arguments...) \
  DAC960_Message(DAC960_UserCriticalLevel, Format, ##Arguments)


struct DAC960_privdata {
	DAC960_HardwareType_T	HardwareType;
	DAC960_FirmwareType_T	FirmwareType;
	irq_handler_t		InterruptHandler;
	unsigned int		MemoryWindowSize;
};


/*
  Define the DAC960 V1 Firmware Controller Status Mailbox structure.
*/

typedef union DAC960_V1_StatusMailbox
{
  unsigned int Word;					/* Word 0 */
  struct {
    DAC960_V1_CommandIdentifier_T CommandIdentifier;	/* Byte 0 */
    unsigned char :7;					/* Byte 1 Bits 0-6 */
    bool Valid:1;					/* Byte 1 Bit 7 */
    DAC960_V1_CommandStatus_T CommandStatus;		/* Bytes 2-3 */
  } Fields;
}
DAC960_V1_StatusMailbox_T;


/*
  Define the DAC960 V2 Firmware Controller Status Mailbox structure.
*/

typedef union DAC960_V2_StatusMailbox
{
  unsigned int Words[2];				/* Words 0-1 */
  struct {
    DAC960_V2_CommandIdentifier_T CommandIdentifier;	/* Bytes 0-1 */
    DAC960_V2_CommandStatus_T CommandStatus;		/* Byte 2 */
    unsigned char RequestSenseLength;			/* Byte 3 */
    int DataTransferResidue;				/* Bytes 4-7 */
  } Fields;
}
DAC960_V2_StatusMailbox_T;


/*
  Define the DAC960 Driver Command Types.
*/

typedef enum
{
  DAC960_ReadCommand =				1,
  DAC960_WriteCommand =				2,
  DAC960_ReadRetryCommand =			3,
  DAC960_WriteRetryCommand =			4,
  DAC960_MonitoringCommand =			5,
  DAC960_ImmediateCommand =			6,
  DAC960_QueuedCommand =			7
}
DAC960_CommandType_T;


/*
  Define the DAC960 Driver Command structure.
*/

typedef struct DAC960_Command
{
  int CommandIdentifier;
  DAC960_CommandType_T CommandType;
  struct DAC960_Controller *Controller;
  struct DAC960_Command *Next;
  struct completion *Completion;
  unsigned int LogicalDriveNumber;
  unsigned int BlockNumber;
  unsigned int BlockCount;
  unsigned int SegmentCount;
  int	DmaDirection;
  struct scatterlist *cmd_sglist;
  struct request *Request;
  union {
    struct {
      DAC960_V1_CommandMailbox_T CommandMailbox;
      DAC960_V1_KernelCommand_T *KernelCommand;
      DAC960_V1_CommandStatus_T CommandStatus;
      DAC960_V1_ScatterGatherSegment_T *ScatterGatherList;
      dma_addr_t ScatterGatherListDMA;
      struct scatterlist ScatterList[DAC960_V1_ScatterGatherLimit];
      unsigned int EndMarker[0];
    } V1;
    struct {
      DAC960_V2_CommandMailbox_T CommandMailbox;
      DAC960_V2_KernelCommand_T *KernelCommand;
      DAC960_V2_CommandStatus_T CommandStatus;
      unsigned char RequestSenseLength;
      int DataTransferResidue;
      DAC960_V2_ScatterGatherSegment_T *ScatterGatherList;
      dma_addr_t ScatterGatherListDMA;
      DAC960_SCSI_RequestSense_T *RequestSense;
      dma_addr_t RequestSenseDMA;
      struct scatterlist ScatterList[DAC960_V2_ScatterGatherLimit];
      unsigned int EndMarker[0];
    } V2;
  } FW;
}
DAC960_Command_T;


/*
  Define the DAC960 Driver Controller structure.
*/

typedef struct DAC960_Controller
{
  void __iomem *BaseAddress;
  void __iomem *MemoryMappedAddress;
  DAC960_FirmwareType_T FirmwareType;
  DAC960_HardwareType_T HardwareType;
  DAC960_IO_Address_T IO_Address;
  DAC960_PCI_Address_T PCI_Address;
  struct pci_dev *PCIDevice;
  unsigned char ControllerNumber;
  unsigned char ControllerName[4];
  unsigned char ModelName[20];
  unsigned char FullModelName[28];
  unsigned char FirmwareVersion[12];
  unsigned char Bus;
  unsigned char Device;
  unsigned char Function;
  unsigned char IRQ_Channel;
  unsigned char Channels;
  unsigned char Targets;
  unsigned char MemorySize;
  unsigned char LogicalDriveCount;
  unsigned short CommandAllocationGroupSize;
  unsigned short ControllerQueueDepth;
  unsigned short DriverQueueDepth;
  unsigned short MaxBlocksPerCommand;
  unsigned short ControllerScatterGatherLimit;
  unsigned short DriverScatterGatherLimit;
  u64		BounceBufferLimit;
  unsigned int CombinedStatusBufferLength;
  unsigned int InitialStatusLength;
  unsigned int CurrentStatusLength;
  unsigned int ProgressBufferLength;
  unsigned int UserStatusLength;
  struct dma_loaf DmaPages;
  unsigned long MonitoringTimerCount;
  unsigned long PrimaryMonitoringTime;
  unsigned long SecondaryMonitoringTime;
  unsigned long ShutdownMonitoringTimer;
  unsigned long LastProgressReportTime;
  unsigned long LastCurrentStatusTime;
  bool ControllerInitialized;
  bool MonitoringCommandDeferred;
  bool EphemeralProgressMessage;
  bool DriveSpinUpMessageDisplayed;
  bool MonitoringAlertMode;
  bool SuppressEnclosureMessages;
  struct timer_list MonitoringTimer;
  struct gendisk *disks[DAC960_MaxLogicalDrives];
  struct pci_pool *ScatterGatherPool;
  DAC960_Command_T *FreeCommands;
  unsigned char *CombinedStatusBuffer;
  unsigned char *CurrentStatusBuffer;
  struct request_queue *RequestQueue[DAC960_MaxLogicalDrives];
  int req_q_index;
  spinlock_t queue_lock;
  wait_queue_head_t CommandWaitQueue;
  wait_queue_head_t HealthStatusWaitQueue;
  DAC960_Command_T InitialCommand;
  DAC960_Command_T *Commands[DAC960_MaxDriverQueueDepth];
  struct proc_dir_entry *ControllerProcEntry;
  bool LogicalDriveInitiallyAccessible[DAC960_MaxLogicalDrives];
  void (*QueueCommand)(DAC960_Command_T *Command);
  bool (*ReadControllerConfiguration)(struct DAC960_Controller *);
  bool (*ReadDeviceConfiguration)(struct DAC960_Controller *);
  bool (*ReportDeviceConfiguration)(struct DAC960_Controller *);
  void (*QueueReadWriteCommand)(DAC960_Command_T *Command);
  union {
    struct {
      unsigned char GeometryTranslationHeads;
      unsigned char GeometryTranslationSectors;
      unsigned char PendingRebuildFlag;
      unsigned short StripeSize;
      unsigned short SegmentSize;
      unsigned short NewEventLogSequenceNumber;
      unsigned short OldEventLogSequenceNumber;
      unsigned short DeviceStateChannel;
      unsigned short DeviceStateTargetID;
      bool DualModeMemoryMailboxInterface;
      bool BackgroundInitializationStatusSupported;
      bool SAFTE_EnclosureManagementEnabled;
      bool NeedLogicalDriveInformation;
      bool NeedErrorTableInformation;
      bool NeedDeviceStateInformation;
      bool NeedDeviceInquiryInformation;
      bool NeedDeviceSerialNumberInformation;
      bool NeedRebuildProgress;
      bool NeedConsistencyCheckProgress;
      bool NeedBackgroundInitializationStatus;
      bool StartDeviceStateScan;
      bool RebuildProgressFirst;
      bool RebuildFlagPending;
      bool RebuildStatusPending;

      dma_addr_t	FirstCommandMailboxDMA;
      DAC960_V1_CommandMailbox_T *FirstCommandMailbox;
      DAC960_V1_CommandMailbox_T *LastCommandMailbox;
      DAC960_V1_CommandMailbox_T *NextCommandMailbox;
      DAC960_V1_CommandMailbox_T *PreviousCommandMailbox1;
      DAC960_V1_CommandMailbox_T *PreviousCommandMailbox2;

      dma_addr_t	FirstStatusMailboxDMA;
      DAC960_V1_StatusMailbox_T *FirstStatusMailbox;
      DAC960_V1_StatusMailbox_T *LastStatusMailbox;
      DAC960_V1_StatusMailbox_T *NextStatusMailbox;

      DAC960_V1_DCDB_T *MonitoringDCDB;
      dma_addr_t MonitoringDCDB_DMA;

      DAC960_V1_Enquiry_T Enquiry;
      DAC960_V1_Enquiry_T *NewEnquiry;
      dma_addr_t NewEnquiryDMA;

      DAC960_V1_ErrorTable_T ErrorTable;
      DAC960_V1_ErrorTable_T *NewErrorTable;
      dma_addr_t NewErrorTableDMA;

      DAC960_V1_EventLogEntry_T *EventLogEntry;
      dma_addr_t EventLogEntryDMA;

      DAC960_V1_RebuildProgress_T *RebuildProgress;
      dma_addr_t RebuildProgressDMA;
      DAC960_V1_CommandStatus_T LastRebuildStatus;
      DAC960_V1_CommandStatus_T PendingRebuildStatus;

      DAC960_V1_LogicalDriveInformationArray_T LogicalDriveInformation;
      DAC960_V1_LogicalDriveInformationArray_T *NewLogicalDriveInformation;
      dma_addr_t NewLogicalDriveInformationDMA;

      DAC960_V1_BackgroundInitializationStatus_T
        	*BackgroundInitializationStatus;
      dma_addr_t BackgroundInitializationStatusDMA;
      DAC960_V1_BackgroundInitializationStatus_T
        	LastBackgroundInitializationStatus;

      DAC960_V1_DeviceState_T
	DeviceState[DAC960_V1_MaxChannels][DAC960_V1_MaxTargets];
      DAC960_V1_DeviceState_T *NewDeviceState;
      dma_addr_t	NewDeviceStateDMA;

      DAC960_SCSI_Inquiry_T
	InquiryStandardData[DAC960_V1_MaxChannels][DAC960_V1_MaxTargets];
      DAC960_SCSI_Inquiry_T *NewInquiryStandardData;
      dma_addr_t NewInquiryStandardDataDMA;

      DAC960_SCSI_Inquiry_UnitSerialNumber_T
	InquiryUnitSerialNumber[DAC960_V1_MaxChannels][DAC960_V1_MaxTargets];
      DAC960_SCSI_Inquiry_UnitSerialNumber_T *NewInquiryUnitSerialNumber;
      dma_addr_t NewInquiryUnitSerialNumberDMA;

      int DeviceResetCount[DAC960_V1_MaxChannels][DAC960_V1_MaxTargets];
      bool DirectCommandActive[DAC960_V1_MaxChannels][DAC960_V1_MaxTargets];
    } V1;
    struct {
      unsigned int StatusChangeCounter;
      unsigned int NextEventSequenceNumber;
      unsigned int PhysicalDeviceIndex;
      bool NeedLogicalDeviceInformation;
      bool NeedPhysicalDeviceInformation;
      bool NeedDeviceSerialNumberInformation;
      bool StartLogicalDeviceInformationScan;
      bool StartPhysicalDeviceInformationScan;
      struct pci_pool *RequestSensePool;

      dma_addr_t	FirstCommandMailboxDMA;
      DAC960_V2_CommandMailbox_T *FirstCommandMailbox;
      DAC960_V2_CommandMailbox_T *LastCommandMailbox;
      DAC960_V2_CommandMailbox_T *NextCommandMailbox;
      DAC960_V2_CommandMailbox_T *PreviousCommandMailbox1;
      DAC960_V2_CommandMailbox_T *PreviousCommandMailbox2;

      dma_addr_t	FirstStatusMailboxDMA;
      DAC960_V2_StatusMailbox_T *FirstStatusMailbox;
      DAC960_V2_StatusMailbox_T *LastStatusMailbox;
      DAC960_V2_StatusMailbox_T *NextStatusMailbox;

      dma_addr_t	HealthStatusBufferDMA;
      DAC960_V2_HealthStatusBuffer_T *HealthStatusBuffer;

      DAC960_V2_ControllerInfo_T ControllerInformation;
      DAC960_V2_ControllerInfo_T *NewControllerInformation;
      dma_addr_t	NewControllerInformationDMA;

      DAC960_V2_LogicalDeviceInfo_T
	*LogicalDeviceInformation[DAC960_MaxLogicalDrives];
      DAC960_V2_LogicalDeviceInfo_T *NewLogicalDeviceInformation;
      dma_addr_t	 NewLogicalDeviceInformationDMA;

      DAC960_V2_PhysicalDeviceInfo_T
	*PhysicalDeviceInformation[DAC960_V2_MaxPhysicalDevices];
      DAC960_V2_PhysicalDeviceInfo_T *NewPhysicalDeviceInformation;
      dma_addr_t	NewPhysicalDeviceInformationDMA;

      DAC960_SCSI_Inquiry_UnitSerialNumber_T *NewInquiryUnitSerialNumber;
      dma_addr_t	NewInquiryUnitSerialNumberDMA;
      DAC960_SCSI_Inquiry_UnitSerialNumber_T
	*InquiryUnitSerialNumber[DAC960_V2_MaxPhysicalDevices];

      DAC960_V2_Event_T *Event;
      dma_addr_t EventDMA;

      DAC960_V2_PhysicalToLogicalDevice_T *PhysicalToLogicalDevice;
      dma_addr_t PhysicalToLogicalDeviceDMA;

      DAC960_V2_PhysicalDevice_T
	LogicalDriveToVirtualDevice[DAC960_MaxLogicalDrives];
      bool LogicalDriveFoundDuringScan[DAC960_MaxLogicalDrives];
    } V2;
  } FW;
  unsigned char ProgressBuffer[DAC960_ProgressBufferSize];
  unsigned char UserStatusBuffer[DAC960_UserMessageSize];
}
DAC960_Controller_T;


/*
  Simplify access to Firmware Version Dependent Data Structure Components
  and Functions.
*/

#define V1				FW.V1
#define V2				FW.V2
#define DAC960_QueueCommand(Command) \
  (Controller->QueueCommand)(Command)
#define DAC960_ReadControllerConfiguration(Controller) \
  (Controller->ReadControllerConfiguration)(Controller)
#define DAC960_ReadDeviceConfiguration(Controller) \
  (Controller->ReadDeviceConfiguration)(Controller)
#define DAC960_ReportDeviceConfiguration(Controller) \
  (Controller->ReportDeviceConfiguration)(Controller)
#define DAC960_QueueReadWriteCommand(Command) \
  (Controller->QueueReadWriteCommand)(Command)

/*
 * dma_addr_writeql is provided to write dma_addr_t types
 * to a 64-bit pci address space register.  The controller
 * will accept having the register written as two 32-bit
 * values.
 *
 * In HIGHMEM kernels, dma_addr_t is a 64-bit value.
 * without HIGHMEM,  dma_addr_t is a 32-bit value.
 *
 * The compiler should always fix up the assignment
 * to u.wq appropriately, depending upon the size of
 * dma_addr_t.
 */
static inline
void dma_addr_writeql(dma_addr_t addr, void __iomem *write_address)
{
	union {
		u64 wq;
		uint wl[2];
	} u;

	u.wq = addr;

	writel(u.wl[0], write_address);
	writel(u.wl[1], write_address + 4);
}

/*
  Define the DAC960 GEM Series Controller Interface Register Offsets.
 */

#define DAC960_GEM_RegisterWindowSize	0x600

typedef enum
{
  DAC960_GEM_InboundDoorBellRegisterReadSetOffset   =   0x214,
  DAC960_GEM_InboundDoorBellRegisterClearOffset     =   0x218,
  DAC960_GEM_OutboundDoorBellRegisterReadSetOffset  =   0x224,
  DAC960_GEM_OutboundDoorBellRegisterClearOffset    =   0x228,
  DAC960_GEM_InterruptStatusRegisterOffset          =   0x208,
  DAC960_GEM_InterruptMaskRegisterReadSetOffset     =   0x22C,
  DAC960_GEM_InterruptMaskRegisterClearOffset       =   0x230,
  DAC960_GEM_CommandMailboxBusAddressOffset         =   0x510,
  DAC960_GEM_CommandStatusOffset                    =   0x518,
  DAC960_GEM_ErrorStatusRegisterReadSetOffset       =   0x224,
  DAC960_GEM_ErrorStatusRegisterClearOffset         =   0x228,
}
DAC960_GEM_RegisterOffsets_T;

/*
  Define the structure of the DAC960 GEM Series Inbound Door Bell
 */

typedef union DAC960_GEM_InboundDoorBellRegister
{
  unsigned int All;
  struct {
    unsigned int :24;
    bool HardwareMailboxNewCommand:1;
    bool AcknowledgeHardwareMailboxStatus:1;
    bool GenerateInterrupt:1;
    bool ControllerReset:1;
    bool MemoryMailboxNewCommand:1;
    unsigned int :3;
  } Write;
  struct {
    unsigned int :24;
    bool HardwareMailboxFull:1;
    bool InitializationInProgress:1;
    unsigned int :6;
  } Read;
}
DAC960_GEM_InboundDoorBellRegister_T;

/*
  Define the structure of the DAC960 GEM Series Outbound Door Bell Register.
 */
typedef union DAC960_GEM_OutboundDoorBellRegister
{
  unsigned int All;
  struct {
    unsigned int :24;
    bool AcknowledgeHardwareMailboxInterrupt:1;
    bool AcknowledgeMemoryMailboxInterrupt:1;
    unsigned int :6;
  } Write;
  struct {
    unsigned int :24;
    bool HardwareMailboxStatusAvailable:1;
    bool MemoryMailboxStatusAvailable:1;
    unsigned int :6;
  } Read;
}
DAC960_GEM_OutboundDoorBellRegister_T;

/*
  Define the structure of the DAC960 GEM Series Interrupt Mask Register.
 */
typedef union DAC960_GEM_InterruptMaskRegister
{
  unsigned int All;
  struct {
    unsigned int :16;
    unsigned int :8;
    unsigned int HardwareMailboxInterrupt:1;
    unsigned int MemoryMailboxInterrupt:1;
    unsigned int :6;
  } Bits;
}
DAC960_GEM_InterruptMaskRegister_T;

/*
  Define the structure of the DAC960 GEM Series Error Status Register.
 */

typedef union DAC960_GEM_ErrorStatusRegister
{
  unsigned int All;
  struct {
    unsigned int :24;
    unsigned int :5;
    bool ErrorStatusPending:1;
    unsigned int :2;
  } Bits;
}
DAC960_GEM_ErrorStatusRegister_T;

/*
  Define inline functions to provide an abstraction for reading and writing the
  DAC960 GEM Series Controller Interface Registers.
*/

static inline
void DAC960_GEM_HardwareMailboxNewCommand(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.HardwareMailboxNewCommand = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InboundDoorBellRegisterReadSetOffset);
}

static inline
void DAC960_GEM_AcknowledgeHardwareMailboxStatus(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.AcknowledgeHardwareMailboxStatus = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InboundDoorBellRegisterClearOffset);
}

static inline
void DAC960_GEM_GenerateInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.GenerateInterrupt = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InboundDoorBellRegisterReadSetOffset);
}

static inline
void DAC960_GEM_ControllerReset(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.ControllerReset = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InboundDoorBellRegisterReadSetOffset);
}

static inline
void DAC960_GEM_MemoryMailboxNewCommand(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All = 0;
  InboundDoorBellRegister.Write.MemoryMailboxNewCommand = true;
  writel(InboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InboundDoorBellRegisterReadSetOffset);
}

static inline
bool DAC960_GEM_HardwareMailboxFullP(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readl(ControllerBaseAddress +
          DAC960_GEM_InboundDoorBellRegisterReadSetOffset);
  return InboundDoorBellRegister.Read.HardwareMailboxFull;
}

static inline
bool DAC960_GEM_InitializationInProgressP(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InboundDoorBellRegister_T InboundDoorBellRegister;
  InboundDoorBellRegister.All =
    readl(ControllerBaseAddress +
          DAC960_GEM_InboundDoorBellRegisterReadSetOffset);
  return InboundDoorBellRegister.Read.InitializationInProgress;
}

static inline
void DAC960_GEM_AcknowledgeHardwareMailboxInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeHardwareMailboxInterrupt = true;
  writel(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_OutboundDoorBellRegisterClearOffset);
}

static inline
void DAC960_GEM_AcknowledgeMemoryMailboxInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeMemoryMailboxInterrupt = true;
  writel(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_OutboundDoorBellRegisterClearOffset);
}

static inline
void DAC960_GEM_AcknowledgeInterrupt(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All = 0;
  OutboundDoorBellRegister.Write.AcknowledgeHardwareMailboxInterrupt = true;
  OutboundDoorBellRegister.Write.AcknowledgeMemoryMailboxInterrupt = true;
  writel(OutboundDoorBellRegister.All,
	 ControllerBaseAddress + DAC960_GEM_OutboundDoorBellRegisterClearOffset);
}

static inline
bool DAC960_GEM_HardwareMailboxStatusAvailableP(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All =
    readl(ControllerBaseAddress +
          DAC960_GEM_OutboundDoorBellRegisterReadSetOffset);
  return OutboundDoorBellRegister.Read.HardwareMailboxStatusAvailable;
}

static inline
bool DAC960_GEM_MemoryMailboxStatusAvailableP(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_OutboundDoorBellRegister_T OutboundDoorBellRegister;
  OutboundDoorBellRegister.All =
    readl(ControllerBaseAddress +
          DAC960_GEM_OutboundDoorBellRegisterReadSetOffset);
  return OutboundDoorBellRegister.Read.MemoryMailboxStatusAvailable;
}

static inline
void DAC960_GEM_EnableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All = 0;
  InterruptMaskRegister.Bits.HardwareMailboxInterrupt = true;
  InterruptMaskRegister.Bits.MemoryMailboxInterrupt = true;
  writel(InterruptMaskRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InterruptMaskRegisterClearOffset);
}

static inline
void DAC960_GEM_DisableInterrupts(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All = 0;
  InterruptMaskRegister.Bits.HardwareMailboxInterrupt = true;
  InterruptMaskRegister.Bits.MemoryMailboxInterrupt = true;
  writel(InterruptMaskRegister.All,
	 ControllerBaseAddress + DAC960_GEM_InterruptMaskRegisterReadSetOffset);
}

static inline
bool DAC960_GEM_InterruptsEnabledP(void __iomem *ControllerBaseAddress)
{
  DAC960_GEM_InterruptMaskRegister_T InterruptMaskRegister;
  InterruptMaskRegister.All =
    readl(ControllerBaseAddress +
          DAC960_GEM_InterruptMaskRegisterReadSetOffset);
  return !(InterruptMaskRegister.Bits.HardwareMailboxInterrupt ||
           InterruptMaskRegister.Bits.MemoryMailboxInterrupt);
}

static inline
void DAC960_GEM_WriteCommandMailbox(DAC960_V2_CommandMailbox_T
				     *MemoryCommandMailbox,
				   DAC960_V2_CommandMailbox_T
				     *CommandMailbox)
{
  memcpy(&MemoryCommandMailbox->Words[1], &CommandMailbox->Words[1],
	 sizeof(DAC960_V2_CommandMailbox_T) - sizeof(unsigned int));
  wmb();
  MemoryCommandMailbox->Words[0] = CommandMailbox->Words[0];
  mb();
}

static inline
void DAC960_GEM_WriteHardwareMailbox(void __iomem *ControllerBaseAddress,
				    dma_addr_t CommandMailboxDMA)
{
	dma_addr_writeql(CommandMailboxDMA,
		ControllerBaseAddress +
		DAC960_GEM_CommandMailboxBusAddressOffset);
}

static inline DAC960_V2_CommandIdentifier_T
DAC960_GEM_ReadCommandIdentifier(void __iomem *ControllerBaseAddress)
{
  return readw(ControllerBaseAddress + DAC960_GEM_CommandStatusOffset);
}

static inline DAC960_V2_CommandStatus_T
DAC960_GEM_ReadCommandStatus(void __iomem *ControllerBaseAddress)
{
  return readw(ControllerBaseAddress + DAC960_GEM_CommandStatusOffset + 2);
}

static inline bool
DAC960_GEM_ReadErrorStatus(void __iomem *ControllerBaseAddress,
			  unsigned char *ErrorStatus,
			  unsigned char *Parameter0,
			  unsigned char *Parameter1)
{
  DAC960_GEM_ErrorStatusRegister_T ErrorStatusRegister;
  ErrorStatusRegister.All =
    readl(ControllerBaseAddress + DAC960_GEM_ErrorStatusRegisterReadSetOffset);
  if (!ErrorStatusRegister.Bits.ErrorStatusPending) return false;
  ErrorStatusRegister.Bits.ErrorStatusPending = false;
  *ErrorStatus = ErrorStatusRegister.All;
  *Parameter0 =
    readb(ControllerBaseAddress + DAC960_GEM_CommandMailboxBusAddressOffset + 0);
  *Parameter1 =
    readb(ControllerBaseAddress + DAC960_GEM_CommandMailboxBusAddressOffset + 1);
  writel(0x03000000, ControllerBaseAddress +
         DAC960_GEM_ErrorStatusRegisterClearOffset);
  return true;
}

/*
  Define the DAC960 BA Series Controller Interface Register Offsets.
*/

#define DAC960_BA_RegisterWindowSize		0x80

typedef enum
{
  DAC960_BA_InboundDoorBellRegisterOffset =	0x60,
  DAC960_BA_OutboundDoorBellRegisterOffset =	0x61,
  DAC960_BA_InterruptStatusRegisterOffset =	0x30,
  DAC960_BA_InterruptMaskRegisterOffset =	0x34,
  DAC960_BA_CommandMailboxBusAddressOffset =	0x50,
  DAC960_BA_CommandStatusOffset =		0x58,
  DAC960_BA_ErrorStatusRegisterOffset =		0x63
}
DAC960_BA_RegisterOffsets_T;


/*
  Define the structure of the DAC960 BA Series Inbound Door Bell Register.
*/

typedef union DAC960_BA_InboundDoorBellRegister
{
  unsigned char All;
  struct {
    bool HardwareMailboxNewCommand:1;			/* Bit 0 */
    bool AcknowledgeHardwareMailboxStatus:1;		/* Bit 1 */
    bool GenerateInterrupt:1;				/* Bit 2 */
    bool ControllerReset:1;				/* Bit 3 */
    bool MemoryMailboxNewCommand:1;			/* Bit 4 */
    unsigned char :3;					/* Bits 5-7 */
  } Write;
  struct {
    bool HardwareMailboxEmpty:1;			/* Bit 0 */
    bool InitializationNotInProgress:1;			/* Bit 1 */
    unsigned char :6;					/* Bits 2-7 */
  } Read;
}
DAC960_BA_InboundDoorBellRegister_T;


/*
  Define the structure of the DAC960 BA Series Outbound Door Bell Register.
*/

typedef union DAC960_BA_OutboundDoorBellRegister
{
  unsigned char All;
  struct {
    bool AcknowledgeHardwareMailboxInterrupt:1;		/* Bit 0 */
    bool AcknowledgeMemoryMailboxInterrupt:1;		/* Bit 1 */
    unsigned char :6;					/* Bits 2-7 */
  } Write;
  struct {
    bool HardwareMailboxStatusAvailable:1;		/* Bit 0 */
    bool MemoryMailboxStatusAvailable:1;		/* Bit 1 */
    unsigned char :6;					/* Bits 2-7 */
  } Read;
}
DAC960_BA_OutboundDoorBellRegister_T;


/*
  Define the structure of the DAC960 BA Series Interrupt Mask Register.
*/

typedef union DAC960_BA_InterruptMaskRegister
{
  unsigned char All;
  struct {
    unsigned int :2;					/* Bits 0-1 */
    bool DisableInterrupts:1;				/* Bit 2 */
    bool DisableInterruptsI2O:1;			/* Bit 3 */
    unsigned int :4;					/* Bits 4-7 */
  } Bits;
}
DAC960_BA_InterruptMaskRegister_T;


/*
  Define the structure of the DAC960 BA Series Error Status Register.
*/

typedef union Driver_BA_Drive/AccelRAID/eXt
{
  unsigned char All;
  struct 01 b by Leonardint :2;	rogr/* Bits 0-1 */deliobool DrivepyrighPending:1pogram is f 2oftware;n.com>

  This5/or maodify re3-7 under}enera;
}
ntrollersermsCopyright 1998-2_T;


/*
  Define inlFounfunctions to provide an abstras of  for reate a and writthatthendertrollthe hMylex Cosion 2 a Interfaceet 1998-2sremeRAstaticndaThis
voidY WARRANTs
Hardder
MailboxNewCommand(MERCH__iomem *NY WARRANTBaseAddress)0ndelsion 2 as InboundDoorBellyinux
  Fr the hcomplete details.
off The author respectfully.All = 0 requests  be  any modificatWrite.
  or FITNESS FOR A PARTI = true reqwilleb(s software be
  sent dirion,
	A See the GNU General  +c Licensls.
he author respectfullyOffset) Vered ul, anty  LiULAR ANTABILITYAcknowledgls.
him for evalpyrighCULAR PURPOSE.  Seeinux Gntrollers sPubliported by this driver.
*/

#definerms oe authe hopspectfu detthis software be
  sent dirhis progtin ts under
 bls.
sent directdetail maximum number  LiNY WARRANTYtesting.
emeRASoftware Founf Targets per Channel Driver NY WARRANTs supportedion.
/*
 dtrollremeRA#de FounDriver_MaxAC960_V1_Ma			81 and V2 Firmwaollerste, witrupt Channels supported by DAC960
  V1 and V2 Firmware Controllers.
*/

#define DAC960_V1_MaxChannels			3
#define DAC960_V2_MaxChannels			4


/*
  Define the maximum number oannel Logicalontrby DAC960
  V1 and V2 Firmware Controllers.
*/

#define DAC960_V1_MaxTargets			16
#define DAC960_V2_MaxTargets			128


/*
  Define the maximum numb See the GReseiveaxTargets			16
Driver
  V1t it V2 Firmine tAC960_V1_Ma960_V2_MaxTargets			128
 and V2ntroline 321 and V2 Firmware Controllers.
*/

#dPhysd V2 Fevicef unsigned long DAC9 ID PremeRAID Pby DAC960
  V1 and V2 Firmware Controllers.
*/

#define DAC960_V1_MaxTargets			16
#define DAC960_V2_MaxTargets			128


/*
  Define the maximum numbMemoryor evaluation and Channels supported by DAC960
  V1 and V2 Firmware Controllers.
*/

#define DAC960_V1_MaxChannels			3
#define DAC960_V2_MaxChannels			4


/*
  Define the maximum number oee Software Founa 32 brmware Controllers.
*/

#define DAC960_V1_MaxPhysicalDevices		45
#define DAC960_V2_MaxPhysicalDevices		272

/*
  Define a 32/64 bit I you 
  Define  number of ContFullP Channels supported by DAC960
  V1 and V2 Firmware Controllers.
*/

#define DAC960_V1_MaxChannels			3
#define DAC960_V2_MaxChannels			ware;ope b(define DAC960_V1_MaxTargets			16
#define DAC960_V2_MaxTargets			12meRAIurn !s software be
  sent dirReadproghimhis devalEmptyRAIDform sizy tomeRA<lnz@dadma_loaInitializa of InPe GNUl ;
	 5-7addr_ts 5-7base;
	;			_t  length;
	MERC	*cpu_freyte Modifier:7;			/						}; and V2 Firmware CSCSI INQUIRY0/Acndard Data <lnz@dur data type.CI R<lnz@daets			1prov_Inquiry001 by Leonard N. ZPeriphers 
  DefType:5;			m isyte 0is free 4 Byt by Leon1 by Leonard NNotar*
  DefT128


/*
  Define the maximum numbf Targets per Channel suppnd V2 Firdef unsigned long DAC960_IO_Address_T;


/*
  DefineOuttrollers.
*/

#define DA2 Bit you AENC:1_VerVershar yte 3 Bit 7 */
  unsigneels			4


/*. Zuddiels	alL6 */
 VersiChannel Targets per ChannelxTargdress_T;


/*
  Define a 32  unsigned char :8;					/ By/

#define DAC960_V1_MaxTargets			16
#yte 3 Bit 7 */
  unsignegets			128


/*
  Define the maximum numbf Targets pee Software Fersion:2;	3is fre4-5Byte 3 Bit TrmIOP
  unsig1;						/* By 6Byte 3 Bit 7 */
  unsigne				/* Byte 77 2 Bits 6-7 ard N. Zu;						/* Byte 7 Bit 0 *te e 2 Bits 6-7 ard N. Z:8s16:1;					/*  Bit 4 unsyte 3 Bit Sync
  unsigte7 Bit 5 */
  SftRe WBus32:1;					/*7yte 70Byte 3 Bit CmdQutificatio[16];		/* Bytesofte 3 Bit ification[16];		/* Byte2nLevel[4];	Linkedned char ProductRevisi3 :1;					/* Bytete 7 Bit 4 */
  bool WBus16:1;					/* Byte 7 Bit 5 */
  bool WBus32:1;					/* Byte 7 Bit 6 */
  bool RelAdr:1;					/* Byte 7 Bit 7 */
  unsigned char VendorIdentification[8];		/* ed char VendorIdentificatio Bit 5 *ifier:3;			/*VendorIdentChannels	[8];ar Producs 8-1eralQualifier:3;			/*Producte;				/* Byte 116*/
  unsigned16-3onLevel				/* Byte 2 */
  unRevisionLevel[4*/
  unsigned32-3 Bit }
ECMA_Version:3;			e 0 Bits 5-7 */f { bool RMB:1;pyrighAvailabl			/* Byte 1 Bit 7 * Byte 1 Bits 0-6 */
  bool RMB:1;	it 5 */
  bool WBus32:1;					/* Byte 7 Bit 6 */
  bool RelAdr:1;					/* Byte 7 Bit 7 -2 */
  unsigned char ECMA_Version:3;				/* ByLevel[4];		/* Bytes 32-35 */
}
Dr ISO_VerRelAdr:1;					/* Byte 7 Be 2 Bits 3-5 */
  unar ANSI_Approve
DAC960_SCSI_Inquiry_Unit MylalN3 */
  bool Synar ANSI_ApproveREQUEST SENSE Sd by Keyress data type.CI Renum001 bECMA_Verd byKey_No DAC96=			0x0,8,
  DAC960_SenseKeRecovaxCoDAC960=ecif1c =		0x9,
  DAC960_SNounsigyrSpecif2c =		0x9,
  DAC960_SMediumopyAborte	0x3c =		0x9,
  DAC960_S
  or FIopyAborted 4c =		0x9,
  DAC96c =		0x9,
  DAC960_SByteProt128


/*
  Define the maximum numbEnrovend V2 Firgned charf unsigned long DAC960_IO_Address_T;


/*
  De V2 FirMask
#define DAC9eKey_DataProtect =	
#defieKey_DataProtect =	els			4
xFFunsigned char ECMA_Versionnera.Dis}
__attribute_ = fals960
 /
  unsigned charDAC96Code:7 unsision:2;				I2Oby DAC960
  V1 and V2d char ECMA_VersionReq/

#define DAC960_V1_MaxTargets			16
#deSE structure.
*/

tyets			128


/*
  Define the maximum numb 0 Bit 7 */
  uns_ ((packed))
DAC960_SCSI_Rhis so0_SenseKeree Software Foun_DataProtect =			0x7,
 2 Bits 0-2 */
  unsigned char ECMA_Version/
  bool EOM001 by Leonard N. Z			/* Byte 0 Bit 7 */
  unsitsDAC960
 3 Bit Vali_T;


/*
rsion:2;				/* 7 Bit 6 */
  bool RelSegmentN Chan0 Bit 7 */
  onLevelte 2 Bit 5 */
  bool EOM:1;		  DAC9Key:4/
  unsign 2is free 3
  unsigned char Vend  uns;				/* B5-7 Bit Byte 0 BF
			/dypeModifier:7;			/* Byte 1 Bits 0-6 */
  bool RMB:1;			2 Bit 6 */
  bool Filemark:1;					/* Byte 2 Bit 7 */
  unsigned char Infor-2 */
  unsigned char ECMA_Version:3;				/* Byte 12 */
  unsigned char Addir ISO_Versio			/* Bytes 3-6 */
  unsigned char Additio128


/*
  Define the maximum numbmber ion andor eval(sion 2 V2_endedWithScatt_T
ograare; *ee Sof0xB4,
  DAC960,1_nsign=rGathebort0xB =		0xD,
  DVadWithSceQuaxB4,
  DAC960nd V2 memcpy(&l =	6	0xB6,
  DAC960->Words[1], &AC960eatterGatheer =7c =	
	3;			ofV1_DCDB =		r =		0xB6,
  DAC) -,
  DAC9 the terms o)adAhewmb(=				te
  DAC960V1_DCDB =				0x040] = ARRANTY/AcceleRAlas			Co				x0A,
 }
atterGather =			0xC,
  DAV1_ectlyExt number of Cont Channels supported by DAC960
  V1 aea,
  DACs 5-7fier:7ands*/
  unsignDMAnd V	a type.
I V1 aql(ormnels	thSca19,
V1_Gdefine DAC960_V1_MaxTar
		eral  dyte tB4,
  DAC960Busllers suets			128


/*
  Define oller Status Relate;				/*.

 rialN 2 as ifie =				Perier:Even Channels supported by DAC960
  V1 and V2 ISO_Ver

tywsigned char ECMA_Version:3;				/* BGetSDStefine ty2 =				GetPD/AccsEqual =	FEnquiry2 =				PpyrightLogOpof L0x39,
	0x7mmanARRANTY ((packed))
DAC960_SCSI_RequestSenseKey_TcorSpecif1ic =		0x9,
 Channe
  Def/AccorSpecif5nsistency and EStop + 2Channel =			0x13,
  
str		0x12,
  DAC96ma3
#ddistrata type.
EletionAC960_55sistency and Errion.com>

  N. Z*V1_CheckConConsD/eXncyAsyncAC960_Parameter0DAC960_build/AccrSpecifiCc =		01nd V2 Firmware Cpd V2sh			16
#dls.

  V1_CheckConKey_BlankChepublished by the
  DAC960_V1Exte =		Equal =	_Enquiry2 =				0x1C,
  enlockTable =			0x0B,
AC960_V1Aheif (!ockTCSI rSpecifiB,
quire:V1_CheckConte 0  a)ith Byte 0- Bit 5and ErroDAC961_GetErrorTa104,
 ency and EAd free  Bit 5x1Ec =		0x9, =l0_V1_GetErrorTable = DAhSca2ess =		0xCC960_V1_ReadBadDataTable =			0x25,
  DAC960_V1_0,
  DA		0x13,
  D,
  /* Con0_ + 06,
  AC960C960_2adWithConfig2Equal =	DEnquiry2 =				0x1C,
  DAC960_V1_WrDAC960_V  DAC960_V1
  DA1Writ	0V1 andol E,#define DAC960_V1_MaxTargets			16
#ClearBadByte1_GetErrorTa2thScaISO_VerDAC96053,
Define tion.nuxontroll LPITHOUT f DAC960 CoYgicalhout evenit 6 /* Confimplie#d_V1_Addion 2 LP_t 1998-2WindowSize960_80t 19 PCI R=			d V2 FirmwarLP#define DAC960_V2_MaxTargets		thSca20,

/*
 UpgradeumError =			0x3,
  DAC960_Sensnd ELoCdImagtError	0xdBadDataT  DAC960_V1_WriteCon	0x213a  DAC960_V1_ProgramImag3		0xB6,
  DAC960_gnosti4  DAC960_V1_Pr0x4B,
  DAC960_V1_ReadConfigura0x211c Commands */
  GetRe60_V1_Pr09omma=abel18  DAC960_V1_Prx4D,
  DAC960_V1_WriteConnd Err2EVeogOperationOnDisk =	onLabeFree SDAC960_V1_AddCon<lnz@d*  boLidConfigurat  DAC96nc =he aut hor  respet 1998-2implie0x49,
  tateNY WARRAN	0x20_V1_EnquiPARTImation ublicencyAsync =		0ubk reqmands anware;
struto him for evaluation andnd/or senty i0 under
 
struf Targets per Channel supported Old =	0x82,
 Define _V1_Wr0
  V1 and V2 FirOld =	 senty it under
 
struress data type.dOpcode_T;


/3__ ((packed))
/*
  dma_loaf is used Onds */0x82,
4
  Definef Taterm =		0:3he GNU General5nd V2 Firmwber ds */
  DA_Enquiry2 =				nsig
  DAC96* BydOpcode_T;


/quiry2 =				0x/
  unsigned charesponsechar DAC960_Ve__ ((pace;				/*er					6he GNU General2nd V2 Firmifie_MaxgOperati960_V1_Read_Old =				0x02gOpera.xx_T;


/*
  Dld =				0x02_V1_ReadCoEUnitAttchar Level[4]sistency and Errors */
  DAC0105	/ckgro =		0xD,
 yte 3 Bit 7 */
  unsigneEnquiry2 =				0x1C,dParametCommand Status CoheralQualifier:3;			/* Byte 0 Bits 			/* Byte 0quiry2 =				0x1C,
  DAC96Bytes 8-15 */
  unsign			/* Byte 0 B1;					
  DRecpeded  DA002t 7 960_onumEr_MaxTargdefine DAC960_V1_
  DAC96			032/64 bit PCI  2 Bit 6 */
  x_RebutOrOfflFoun/O */
 /*SenseKey_Miscompare =			0xE,eyondEndOf data type.
  DA105t 7 I/OC960_V1_BadD_V1_ReadCo  DA_V1_ReadCoNo
  Defit 5 */
  bool WBus32:1;		on */
#define DAC960_V1_InvalidDeviceAddress		0x0105	/		  unsignpproe DAC960_V1_InvalidParameter		0x0105	/*0_2 Bit 6 */
  bool F,
  DAC960_V1_Write_Old =				0x03,
  DA the terms of gramGNU Generale  Define _V1_Wr 0 Bit 7 */
  unsdOpcode_T;


/*
  Definedentifies  Linux AC960
  V1 and V2 Firmware60_V1_NoDevalidDeviceAddrSetDiae =	 and EUnCSI ToStarors */
ecifi*/
#def
  DefDAC960_V1_Com_V1_Repyrigh_V1_GetDeviceState_Old =		0x14,
  DACrSpecif7nsistency and60_V1_ChannelBusy			0x0106	/* Device */
#define DAC960_V1_ChannelNotStopped		0x0002	17,
  DAC960_V1_Adine DAC960_V1_AttemptToRebuildOnlineDrive	0x0002	/* Consistency */
#define DAC/
#define DAC960_V1_Resion 2.xx Firmwarenty ofEST SENS60_V1_ChannelDAC960_V1_d iigurath

tyware it
*/

 be useful, but
quiry_Old =4AC960_V1_ReadConfiguratigurael =		0ontrollers			8


/*
  DefiLPaf {
	void	*cpu_ne a 32 bit Byte Count data type.
*/

typedef unsigned int iceAteneral ecifi1/
#define AC960_V1_MaxChannels			3
#define DAC960_V2_MaxChannels			4


/*
  Define the maximum number ofd char Peripheu,
  DA		0xy DAC960
  V1 and V2 Firmware Controllers.
*/

#define DAC960_V1_MaxTargets			1iceAtAddress		0x0102	/* Co_V1_Re	128


/*
  Define the maximum nLPriteWithScatterGather_Old =	0x83
nnel =			0x1A,
  /* Commands Associated wiT;


/*ail*

 DCDB */
#0x141_RebuilDAC960_V1_CommandTermi1_GetReFailedinatBgurasOnOerGattency3#defin60_V1_BackgroundInitAborted	 5 */
  unsigned char :8;	gned long DAC960_IO_Address_T;


/*
  Define a 32/64 bit PCI Bus AddrV1128
 */
  u			1960_V1_BackgroundInitAborted		0x0005	/*Successs			3Termin1_En /* Co0
  V1 and V2 Firdef unsigned long DAC960_IO_Address_T;


/*
  Defncy */
#define DAC960_V1_BackgroundInitAborted		0x0005	/* Consistency */
#define DAC960_V1_NoBackgroundInitInProgress	0x0AddCapacityInProgress		0x0004	/* Consistency */
#define DAC96s_T;


/
  Defi		45tency */
#define acityFailedOrSuspended	0x00F4	/* Consistency */
#define DAC960_V1_CenseKey_BlankCheck lifier:3;longommandTeIO_eneral 						/* Byte ncy */
#define DAC960_V1_BackgroundInitAborted		0x0005	/* Consistency */
#define DAC960_V1_NoBackgroundInitInProgress	0x0AC960_V1_GeFailed		0x0002	/* Subsyste Thi_V1_ReaBuseneral 3 Free Software Founa 64 bit BuslAdrracityFailedOrSuspended	0x00F4	/* Consistency */
#define DAC960_V1_C
/*
  dma_loaf is used tit 7 *Coumum nd reply structure.
*/

typedef strncy */
#define DAC960_V1_BackgroundInitAborted		0x0005	/* Consistency */
#define DAC960_V1_NoBackgroundInitInProgress	0x0SoftwarialNumbeisuild		16
helsignroutine progdi is di regOperof		/* B mapped m


/*f sto smaDAC96piecesacityFailedOrSuspended	0x00F4	/* Consistency */
#defe 0 Bits 5-7a	/* Bstency0#defi* Byte Modifier:7;			/* Byte 1 Bits 0-6 */
  bool RMB:ncy */
#define DAC960_V1_BackgroundInitAborted		0x0005	/* Consistency */
#define DAC9-2 */
  unsigned char ECMA_Version:3;				/960_V1_BackgroundInitAborted		0x00052,
  DACion:2;				/* Byte 2 Bits 6-7 its 3-5 */
  un* Byfier:3;			/*MinorT;


/*
0_V1_No;	0000	/* Common */
#definypeMsent ere 0 Bites 8-11 *s free  Bit 5 */
  RMB:Level=			 01 bidDeviceAddrN0003ndby1_GetReOrCheckar DevinOnDiskd_Drtencommaild =				0x09,  0xF0,
    V1_BackgroundRRebuildO=			0idDeviceAddrBackgreduneFailed =   0xF1,
    DAC960_mmandckgroundRebuildO0000	/* Common */
#definConsistency */
#define DAC960_V1_C7_V1_NoBackgroundInitInProgcksnseKey_istency */
#*/
#defin
  DAAC960_V1_CommandTermiConC960_SCSI_Inquiry_UnitSerialNumber
{
  unsigned char PeripheralDeviceType:5;			/* Byte 0 Bits 0-4 */
  unsigned char PeripheralQualifier:3;			/* Byte 0 Bits 5-7 */
  unductIdentification[16];		/* Bytes 16-31 */
  unsigned char ProductRevi1_Loaduild =				0x09,
re DAC960_V Consistency */
#define DAC960_V1_C_attribute_Bytes 8-15 */
  unsignr ProductRevisie 2 Bit3 */
WBus16ned char ProductF,
    DAC393 */
  unsigned char0008	/* data type.
32];	yte 12 */
 14s 16-31lifier:3;			/* Byte 0 Bits 5-714 3 */
  unsignedshort vicetLsigned char :8;					/* Byte 2 */
  unsigned char PageLength;				/* Byte 3 */
  unsigned char ProductSeredef str:2Bytehar :8;			s 145-147 Bit 6 */
  bool RelDeadd char :3;			ar :8;			-2 */
    bool BatteryBackupU Byte 150 */
  struct1_GetRensigned char Chan 15s 16-31<lnz@daled_Drlifier:3;			/* 3Flags;
  stru 15C960_V1_BC960_SC9 */
  Bller yildOupentiPreximuCodeQu(packed))
DA	   ute__sion:2;				/* B	/* Byte  unsigned char age			0/* Bytes 8-11 */
  ulifier:3;			/* Byte 0 Bits 5-7C960_SCpedef struct DAC96ar :8;				1;					/* B :8;					/* Byte 2 */
  unSer*/
  } MiscFlags;
  struct {
    unsigned char Target  DAC960_V1_Starmware Enqu36 *nseKey_DataProtect =			0x7,
  DAC960_SenseKey_BlankCheck =			0x8,ount;				/* Byte 150 */
  struct {
    unsigned char :3;					/* Byte 151 Bits 0-2 */
   gicalDriveFailed =   0xF1,
    DAC960_V1_BacknseKey_Equal =			0xC,
  DAC960_SenseKey_VolumeOverflow =		0xD,
  DAC960_ey_Illegal/
  boo,
  DACc =		0x9,
  DAC960_SentiAty			  DAC96PU =			0x01 ThiLor	0x01			0x   DAC960_V1_BPTL0rror	0x0		0x01nnelBoard =		0xRL
      DA
    nnelBoard =		0x031
      DAthSca   DAC960_V1_B1164P DAC960_V60_I  } ondEndOfLogicalDriv	/* B SubModelyte 12 */
 s 16-31 by Leonard N. Zuctual ((packe   DAC960_V1_AC960_V1ckFailed_DridDeviceAddrFi,
  DAC960isuthone t_Old =60_V1_ReadCo DAC960_SenservdBadDa/
#dDAC96 DAC960_V1_EISA_Controyte 2 Bit 5 */
  bool EOM:1;						/* ted		0x0005	/*tency */
Encemark:1;					/* Byte 2 Bit 7 */
  unsigned char Information[4];				/* Bytes 3-6 */
  unsigned char Additional1_Backgrotion[4];		/* Bytes 8-11 */
  unsigned char AdditionalSenseCode;			/960_V1_RebuildBadBlocksEticMokFailed_*/
#define DAC960_V1_Subsystete 149 */
  ILIgned char :8;				 unsion[8];		3 */
EOMgned char :8  structed char A/* Major0_V1_No.x03,
0_V1_No-T;


/*
hErr-TurnID				/* Bytes 195-255 */
}
__attries 9-11 */
 ar Reserved[6e 2 BitPU =	cInformation[4];		/* Bytes 8-11 */
  unsigned char AdditionalSenseCode;			/-7 */
  u d N. Z ConfiguredCte__ ((packed)7 Bit 6}kFailed_LogicalDr 102,
 }
DAC960_SCSI_/
  bool EOM						/* Byte 2 Bit 6 Driver V1d int :24;					/* Bytes 9-11 */
  unsigned char ConfiguredChannels;			/* Byte 12gicalDriveFailed =   0xF1,
    DAC960_V1_BackgrAC960_V1_SetDiagnosticMod6,
  0_V1_ReadBadDataTa960_V1_CheckCon960_V1_ReadBController Status Re_V1_Enquiry2 =				LPAC960_V1_nt FlashMemorySize;				/*		0xB6,
  DAC960_V1_WritteWithScatterGather 
*/

#definer Status RelatethScatterGatherectly */
  strundInitializati/* I/O */
#de_V1_DCDB =				0x04,
 _V1_ReadCoDCDBrror	0x00		0xB6,
  DAC96          DAC960_V1_RamType8		0xB6,
  DAC96Flushrror	0x00		0xe DAC96ne DAC96  DAC960_V1_Enqui0_V1_InvalidDeviceAddress		0x */
  st53,ionLed char Chanes 4-31 */
PU =			_V1_ReadCoGe=			istency =			0x0F,
  DAC960_V1_CheckConsi    DAC960_V1Inier:,
  DAC9  DAC9AM =			0x2,
IOPorunsignqual =	Last =		0x7
    } __a   DAC960_0x3te 40_V1_Rea DAC960unirmwareTypbute__ (mmand/Channel =			0x13,
  DAC960_V1_StartCh /* Device=			0x12,
		0x1960_V1_Re Bytdefine  {
      DAC960_V1_ErrorCorrecti003	/* Cons Data Consistency and Errors */
  DAC960_VAC960_V1_InvalidDeviceed char Backgro_Enquiry2 =				003	/short MemorySpe2,
e 4ed char Comse45 */
  unsignedte 40 Bitsld =			Associ1_EnqV1_R Byte NoBackgroundigneDAC96mation =	0x10_VAC960_V1_InvalidDeviceAborted		0x0005	/*dStat =		packed)) ort Har				960_NoBackgroundSpecifiAC960_V1_StartChar :8;					/* BdStat =			0B,
  /* Con960_V1_GetRebuildProgress =		0x =	0x1,
   1_GetRe_BackgroundR0x2SDRAM =			0x2ackgroundInitAborted		0xt Maxte 40 Bits 3V1_ency */tion Related Command1_ReadConfig  DAC960_V1_WriteCon
      DAC96/
#define DAC960_V1_ReldBadSthScatterGather,
  DAC960_V1_BackgroundInitializationdCapacit Bytes 58-*/
  unsigned shortildOrCheck
  unsigneoardts 0-2 _V1_LoComma138 */
  unsigned {
      DAC960_V1_ErrorCorrectix4A,
  DAC960_V1_WriteConfigurationOnDisk -5 */
    bool FastPageMode:1;			*
  unsigneOnDiskerflow  unsigned shortnOnDisk =		4-75 */
  unsi;			/* BytMinsionLd chDAC960_V1_Erroogress	0x0*/
  unsigned sho/
  ne t 76-79 */
  u =0x02,
      s 66-67 Bit 6 */
  bo,
  DAC960_4AC960_V1_StartC
  u76-79 */
 Wn =			0x4C,
  DAC960_V1_ReadConfiguraoard VerMemor0x48ast =		0x7
  */
 es 76-79 */
  unsigned int Last /*_T;


/*
 =				
#define DAC960_V2_MaxTargets		*ller6c Commands */SenseKey_Equal =			0xC,
  DAC960_cy */1   DACgned char*/
    char FirmwareType;	:3;	/* By=			0xA,
 AC960_V1_Opcode1;			/* Byt   DAC965    DACgned cha0x72,
  /* Device struct 00-10AC960_VivicalDr    DACor evals 88-91 2
  unsubsyst52ent:1;			/* BytCach Byte    D3gs;
  struct 34-105 */
  struct {
    enum 4gs;
  struct 2;		axcy */
PC960t {
    enum 5gs;
  struct 5_16iveSyte 56 d)) Model;			/*6gs;
  struct 6			0x2
    } __attribute__ ((7gs;
  struct 7			0x2
    } __attribute__ ((8gs;
  struct  Bytes emByte __attribute__ ((9gs;
  struct 9_Uluted      DAd)) Model;			/10gs;
  struct A0x2m {
      DAC960_V1_EISA_Co1gs;
  struct B;		/* Byte 106 Bits 2-3 */
   ags;
  struct dInitializatioDefine cy */Facto	/* Bytes 102-103 */
 Dttribute__ ((packed)66-67 */
  unsBytes 1Ettribute__ ((p		/* Bytes 66-67 */
  unsBytes 63tes 48-51 *gned Size;		/*0_V1_NoDe/
#define DAC960_V1_InvalidDeviceAddress	WITHOUT Ar	0x00on_ECC =		0x2,
   0_V1_InvalidParameter		0x0105	 this driver.
*/

#define0_V1_IrrecoverableDataError	0x0001	/* I/O */um {
      DAC960_V1_RamdParameter60_Vype_EDO =			0x1,
      DAC960_V1_Rfine DA0x83_ ((packed)) ProductFamily;		/* Byt				/PARTI;			/*
  unsigned char MaxSpans;				/*taEncountered		0xtionReceivreply structure.
*/

typedefargetIgned char :8;			tionReceived	0x15 */
  struct {
    bool Clustering:1;					010C	/* I/O */
#define DralQuSAFTE =				0x20
  } __attri Bytes 152-194 */
  usponsigned  DCDstencyE*/
#d     char MaxCommands;				/PARTIfine DAC96AbnrorCllystenc Controllers.
*/

#define D;		/* B
  struct 12-1d charheckFailed_DriveFailed AEMIefine DAC960_V1_BadD=		0x7
    nv/* Bx0C,
  DA2 /* Co5tes 96-97 */
  unsigned short    rrnseKeyCSI Bytelag;		tency1define DAC960_V1_CommandTermi   DAC960_V1NoneBusy			0x0008	/* DCDB */
#ine DAC960_V1_CommandTermiyrigssBsponsive		0x000E	/* DCDB */
#define DAC960_V1_CommandTerminate_Lognc2];	axCo2 /* CoCdefine DAC960_V1_CommandTermi
  DefBusyr	0x00008ytes 116-119 */
  } FirmwareFe
  DefNonChan :28;					/* Bytes 116-119 */
  } FirmwareFeatures;
  unsigned int :32;					  DAC960_SenseKey_VendorSpecned int :32;					/* Bytes 124-127 */
}
DAC960_V1_Enqui =e DAC960 V1 Fied charOr */
  Oror =		  960_V05 nt :3gned char98-9 Byte 160_V1_ChannelBusy			0x0106	/* Device */
#define ved	0xed shor charNot9,
 p960 V1 cy */
#define DAC960_V1_Nene =		0xFFttemptTo0,
    Dnved	0xcoBackgrotcy */
redunDrive_0_V1_NoBackgrou Bit 7 */
  unsigned sh  DAC;7 Bit 7 */
  unsigned char Ven =		umbere 0 Bit 7 *w uns ConsiDuring1_GetReDrive_4_V1_NoBackgroundIns published by the
  buildOrCheckAlreadyInProgress 0x0106 /* Consistency */
#define DAC960_V1_DependentDiskIsDead		0x0002	/* Consistency */
#define DAC960_V1_InconsistentBlocksFound	0x0003	/* Consisten

/*
 erMemorte 151 Lo960 V1 FiOrInforddund;				ta type.
	/* Byte 5_NoBackgroundInitInProgress	0x0ed =	0,
    DAC960_V1_B
  unsigned char :8;					/* Byte 87 */e DAC960_V1_RebuildInProgress_Dataoaf {
	void	*cpu_ne a 32 bit Byte Count data type.
*/

typedef unsigned int t :32;					20-1202,
      DAkgroundInitAborted		0x0005	/* Consistency */
#define DAC960_V1_NoBackgroundInitInProgress	0x0	0x0005	/* ConsisNewError ConsiDefine the DAC960 V1 FirmSuspended	0x00F4	/* Consistency */
#2 /* Co 136 *


/*
 ne DAC960_V2_MaxTargets			128


/*
  Define the maximum nL-3 */
  unsigned char :2;			e DAC960_V1_BackgroundInitSuccessful	0x0100	/* Consisteructure.
*/

typedef struct Dypedef enti:6ar Reserved[6/* Byte07 Bit 4 lifier:3;			/* 2te__ ((packed)/* Byte6.
*/

typede/
#defiC960_V1_BackgroundInitAborted		0x
  unsignedV1_Backgro				/*  4-5 */
  unsigned char ErrorCode:7;		
  unsi	/* Byte 7 */
  unsign6d char CommandSpecificInformation[4];		/* B  DAC9har :ldFlag;		/* Byte 138 */
  unsigned char MaxCommands;				/e a 32/64 bit PCI Bus Address data type.
*/

typedef unsigned long DAC960_PCI_Address_T;


/*
  Define a 32 bit Bus Addfine DAC960_V1_ConfigurationNotSavedStateChange 0x0106 /* Configuration */
#define DAC960_V1_Subsned ch0_Sen	/* Byte 7 Bit 7 *ytes 124lifier:3;			/*atures;Specnt dIem;			/* fine the DA1_Ultra =ed char ErrorCode:7;		1_Ultra =ucturf is us/ Number9,
 OfLogicaCommand reply structure.
*/

typedefm */
#definePCIAC960_V1_SubsystemBusyLogicalDriveSi_Device_Standby =			0x10
}
__attribute__ (( struct DAC960_V1_Enquiry
{
  unsigned char NumberOfLogicatype.
*/

typedef enum
{
    DAC960_V1_Device_Dead =			0x00,
    DA2,
      DAC9pedef strucionLs[32*/
 it 7 */
  4-1e 3 */
  unsigned/* BytFct DAC9yDiskuntuiry
{
  unsigned char NumberOzes[32];			/* Bytes 4-131 */
  unsigned shm */
m */
#defineed char :64						/* By* Byte 134 Bit 1 */
    unsigned char :6;					/* Byte 134 Bits 2-7 */
  } StatusFlags;
  unsignedtype.
*/

typedef enum
{
    DAC960_V1_Device_Dead =nelBoync d inumber_T;


/*
  D/
  unsigned char MajorFirmwareVersion;			/* Byte 137 8 Bit 5 */
  boo 0 Bitd reply structur1.
*/

tyckFailed_DriveFailed efinehErryte 57 *  } Mi3l EOM:1;						s 76-7 Byte 150 */
  s Byte 2 Bits 3-5 */
  unsigned char ISO_Version:2;				/* Byte 2 Bits 6-7 its 3-5 */
  unsigQual=		   mmand reply structure.
  unsxF0,
    Cthor ret FlaopyAborteebuildFAC960ckgroundRebuildOrCheckFailed_O
    DAC960_			/* Bytes 4FasV1 FirmxFF,
    DAC960_					/* Byte Widtification[16];		/har :3 Bit 5 */
  TaggensiguingSdefine D1 Firmware Enqud char CommaBoard =		0xiguration */
nvalidT unsignon_T
	D	/*116 Bit 3 */
    unsigned iefinFrorCo Byte INQUIRY Unit SegicalDrives;			/* BECMA_Version[28];		/* Bytes14-17 Bit 6 */
  bool RelAdr:1;				0_Sen			0t;				/* Byte 150 */
  struct {
    unsigned char :3;					/* Byte 151 Bits 0-2 */
    bool BatteryBackupUnitPresent:1;			/* Byte 151 ogShis nce[4];		/* 
  struct {2-14AC960_V1_Device_Dead =ritd V2unsigned char :3;					/* Byte e 2 Bits 6-7 ionLevel[4];		/* Bytes 32-35 */
}
DAC960_SCSI_Inquiry_T;


/*
  char :8;					/ 14851 Bits 0-2 */
    bool BatteryBackupU/* Bytes 152-194 */
  unsignedar :3;					/* ByteBus32ned char ProductRevisiackgroundRebuelAdrned char ProductRevisi7 Bit 6 */
  bool RelageCode;				/* Byte 1 */
  unsigned char :8;					/* Byte 2 */
  unsigned char PageLength;				/* Byte 3 */
  unsigned char ProductSer char :8;	 Bit 2 */
PARTI
  reply:1;					/* Byte 2 Bit 7 */
  unsigned  70 */
 r :8;	/Accel001 by LeonardC960_V1_DeviceState
ool Present:1ed char Additional Thicy */
ynchronouDAC960_V1_Erro4.
*/

typedef struct D  } __at1[1 bool   unsigned ch Byte 150 */
  sC960_V1_DeviceS2tered		0x   0x00,
    DAC960_V1_BackgroundInitializatioess		0x2001 b Bytes 195-25PCI_Controller =		0x03,P_PD_PUyte 57 *__attribute__ ((AC96SIC_DAC =2,
    Dsigned short G  } __att0		0x6ber[2 */
  unsigned4te 3 */}
DAC960_SCSI_Inquirttention =		 6-9 */C960_SCSI_I60_V1_TwoChannelBoard =		0x03,
      DAC960_V1_ThreeChannelASIC_DA  DAC960_SenseKey_VendorSpecific =		0x9,
  DAC960_SenseKey_CopyAborted =			0xA,
  DAC960_SenseKey_AbortedCommand =		0xB,
  DAC960_ DAC9601_GetEEntrx00,
    DAC960_V1 8-11 */
  u1_PCI_Controller =		0x03,
 veation_T		/* Byte undInitializationStatThre Command reply struthScatterGat60_V1_DeviceNonresc2
    } SDRAM =			0x0_SenseKeBlankhar :
    } 8,		/* B char Reserved[622,
      DAC960_V1_P
  o			/* B char Reserved[6P_PD_PU =			0x01,
   60_V_T;


/*
  Define the D char MaxArms;igneDAC960_V1eEntryte 115 */
  struct {
    bool ClusteringGet_V1_ErrorTafine the DAC960provtorsionts 0-2 */
  struc8m {
      DAC9600x
DAC96acked)) ProductFamily;		/* Byte 3 */
  } HardwareID;						/_LogicalDrive_C_ErrorCorre1 */
  unsigned char ConfiguredChannels;			/* Byte 12 */
  unsigned char ActualChannels;				/* Byte 13 */
  us =			    0x03,
alChannels;				/* By Bit 4 _V1_Device_Dead annelsed char Chan Bit 7 */
  unsigned sh Initia2 Bits 0-2 */
  unsigned char ECMAInitializationSuspended =   0x05,
 dorIdentificationdInitialization char :8;	 Bit 5 */
  Filemarkned char Productr :8;	7 Bit 6 */
  bool Rel_ErrorCorre[28];	960_V1_ErrorBackgrouuct DAC960_V1_RebuildProgressLenunsigned ];
}
DA3 */
NoD0_V1nnectOnFirstnvalid d));	/* Bytes 6-9 */P_PD_PU =			0x01,
   ol EOM:1;						/C960_V13te 2  structur
  unsigned char AxArms 80-83 */
   edef struct DAC960_V1_MaxSpans 80-83 *aEncountered		0x;			/*32/64 bnkCheck =			0x8,
_Logicaut;				/* Byte 72 */
  unsigV1_ReadBadDataTable =			0x25,
  DAC960_ID_Siemens =			0x10,
    DAC960_V,
        DAC960_V1_Erro28te 3 */
  unsigned ThiFlash;


/*    DAC960_V1_ErrorCorrectiDAC960_V1_nt FlashMemorySize;	1ySize;			/* Bytes 36-39 */
  struct {
    enum {
      DAC960_V1_R0,
    DAC960_V1_OE* Byts 2-AC960_V1_RamType_EDO* Controller Status Related 
 0_V1_InvalidDeviceAddress1one =* Controller Status Related 2 ForceeChan
}
__ves];
ectl2enseKeyyCodeQpleted;				/* Bytes 3bool EnableLeftSymmetricRA3one =it 7 */			/* Byt_Enquiry_PCI_Controller =		0x03,			/* B{
  oard_riti		/* Byd)) Model;			/* Byyte 9 */
  unx1		/* Bytes 60-61 *   DAC960_V1_ErrorCorrection_ECC =		0x2,
      DdToOneSes 5-1 Firmware Enqar AdditionalSenseCodeQcharribute__ ((packe0_V1_Enquie 2 BitckFailed_DriveFailed ildOrChecknvalid ool Present:1 102-10igned char_ */
_8MHz
    } __attri0x20
 d V2        bool NoDisconnectOd:1;rSpecific =	ine the DA0_V1_;			/*C960_V1_BAC960_V1_     2AC960_V1_EISA_Controllpeed:2off har :1;	ol E8BiV1 Firm1;					igned char R Bits 0-1 */
   l Tag[1t20
  unsi 11 Bit 2 */
 x02,
   255 */
}
__attribute__ Bit 7 */Channel =			0x13,
  DAC960_Vctor:1;		r;


/*Queuing:1;				rd =		0;			/*0 Byteure.
*/

t		/* Bytes 46-47 */
  unsigned int :32;					/* Bytes nsigned char ECMA_Versi
ribute :2;
    bool FitiatorID;			/* Byte 1 }pprovCa))nnel =		x0C,
  DAs[LengBits 5-7 */-/* Byte 0 Bit 0 *pprov
  unst Firmwarl Present:1;6-  unsigned char T1 Bit 6 */
  bool Tagg48-5bled:1;				/* Byte 7t 6 */
  b  bool AEMI_OFM:1;				char VendorIdentification1_OEMID_signed short :16;					/* yte 57 */
  unsigned short :16;					/* Bytes 58-59 */
  unsigned short MaxCommands;				/* Bytes 60-61 */
  unsigned short MaxScatterGatherCommandgleEntryttribute_	/* Byte 0 Bit 0 */
 Maxd char ld =		unsigned char 4-6 Byte 8 Bit 5 */Geometry_IODescriptors1_x4D,
  DAC960_V1_WriteConfigurBytes 60-61 *ef struct rs;			/* Bytes 68-69 */
  unsigned char Latency;				/* Byte 70 */
  unsigned char :8;					/* Byte 71 */
  unsigned char SCSITimeout;				/* Byte 72 */
  unsigned char :8;					/* Byte 73 */
  unsigbool Active Bit10or20 0-1 */0x0_V1_ar SimultaneousDeviceSpinUpCount;		/edQueuingSupported:1AC960_Vits 5-7 */
  } r20MHz 2-1052-194 */
  unsignRaDisk stagned c:1;					/e 2 Bit60_ViteConfigurationOnDisk =		gress =		nsigned car :8;					/* Byte 86 *PGBit 5 */
  bool AEMI_OFM:1;					/char SimultaneousDeviceSpinUpCounPGt Firmwar  } __attribut200nt :32;					/* Bytes 92-95 PGetry_255_63 =			0x :16;				rrorcy *00oc Commands *PGd int :24;					/* Bytes 145-147k =		OrChecet Logicalical* Bytes 98-99 */
  unsigned sh00ort MaxBlocksPGecksum;				/* Bytes 62-63 */teOnly um {
 z =			0x2,
B request structure.
*/

typed02,
 100 unsigned shPGruct {
    enum ags;
  struct on[4-Byte  } _Last =		_Timeout_2ontroller =		1C960_VNarrow_8DCDB_Timeout_10_se32;					/* Bi
   bool DisByte 8d)) Model;			/* Bye 5 _32bit1evice Maxit 7 eout_10_s10_minuteA_ControlBusW100dthol EO 11 acked)) Timeout:2;C960 V1 Firmw100fine the DACacked)) Timeout:2;) Model;			/*1001_Ultra =			acked)) Timeout:2;ounte2
  960_/
  ;		/* Byte 1cked)) Timeout:2; /* Byte it 3 100-5 */
  			/					 ReadAheadd chDCDB_TDiffere100unsiQueuing:sAddress;			 /* Byt4_hournDisk 0x20C960_V1_EISA_Co0x00,
    DAC960_V1_StartupMode_pabil10111 */
  =			and:1;	ra* Bytes 108-111 */
 1011_Enquiryyte Ad :32;					/* Bytes 108-111 */
 103F0_V1_NoDevype_Last =	eBuildNumber;			/* Bytes 112-113 */
  enum {
    DAC9nit Seri7I =				0x01,
    DAC960_V1_OEM1 =				0x02,
    DAC960_DCDB_TimeAC960ransferS4-7 ,
  DAC960_V1_WThi08,
    DAC960_V1_Conner =				0x10,
    DAC960_V1_SAFTE =				0x20
  } __attribute__ ((packed)) FaultManagementType;	/* Byte 114 */
  unsigned char :8;					/* Byte 115 */
  struct {
    bool Clustering:1;					/* Byte 116 Bit 0 */
    bool MylexOnlineRAIDExpansion:1;			/* Byte 1alDriv7/* Byte 2 Bit 6 3 Define:1;					/* Byte 116 Bit 2 */
    bool Backgr Get LogicalNnt :3ynchronionDevice_  bool T;


/*
  Define the DAC96har :8;	igned 0105	s of 3 */
#define DAC960t 1 */
  dAbnormally	0x0e 87 */
}
DAC960_V1_DCDB_TDefine [60x00,
 ress;			 /*2-8 Byte 8 Bit 5 */
  bo/Accery2_T;


/*
  Define the DAC960 V1 Firmware Logical DrCDB_TimehannelB
DAC960_V1_DBit 1 */
    bool ReadAhead:1;					/* BAC96x03,
  DAC960_V1_LogicalDrive_Critical =		0x04,
  DAC960_V1_LogicalDrive_Offline =		0xFF
}
__attribute__ ((packed))
DAC960_VadBato 16 b Taggfox000efficiommaafine the DAC960 V1 Firmware Logical Drive Information structure.
*/

typedef struct DAC960_V1_LogicalDriveInformation
{
  unsigned int Logica ((packed)) Common;
  struct {
  
}
__remeRAID PCI ceStates 10-_V1_DeviceStan_T
	DAC960_V1_LogicalDriveInformationArray_Ts 0-3 */
 s;e 5 Bits 0-6 */
  bool WriteBack:1;					/* Byte _ControlDnumbeionol EO /Bit 1 */
    bool ReadAhead:1;					/* ar TargetIummMessagEnabl    DAC960_V11_Erroion_T;


/*
  Define the DAC960 V1 Firmware Get Logical Drive Information8	/*on:1;			/* Byte 116  not2id =	 0
  V1 and V2 FiBit 7 d)) Direcgned__at0:24pcode_T;


s 8struct {
  03	/* Consistenc{
    DAC960_V1_Commandgned chAC960_V1_ed char TargetIummy2	0x00,
    DAC96012-wDiskFailedDuringRebuild	0x0004	/* Consistency *0_V1_Device_Dead =DB[1buildOrCheckAlreadyInProgress 0x0106 /* Consistency */
#define DAC960_V1_DependentDiskIsDead		0x0002	/* Consistency */
#define DAC960_V1_InconsistentBlocksFound	0x0003	/* Consisten		/* Byte 0 */
    DAC1_InvalidOrNonredundantLogicalDrive 0x0105 /* Consistency */
#define DAC960_V1_NoRebuildOrCheckInPchar Status 124-127 */
}
DAC960_V1_Eed c DAC960_V1_RebuildInProgress_DatPGargetID:5;				/* Byte 2 Bits 0-4 */
  unsigned char Channel:3;				/* Byte 2CI RAID ConirmwareFeatures;M */
#define DAC960_V1_RebuildFailed_BadBlocksOnOther 0x0003	/* Consistency */
#define DAC960_V1_RebuildFailed_NewDriveFailed	0x0004	/*ld V2 Firmware Controllers.
*/

#define DAC960_V1_MaxTargets			1e 87 */
}
DAC960_V1_DCDB_TmT	0x0	128


/*
  Define the maximum nPG07	/* Consistency */
#define DAC960_V1_BackgroundInitSuccessful	0x0100	/* Consisteice_Dead =tion_T Define the DAC960 VnFirstCommand:1;	*/
  * Byte 1 Bit 2 11 Bit 7 gned short :16;					/* Bytes 10-11 */
}105	/* Consistency */
#define DAC960_V1_AddCapacnsigned r =		   ute__ (		/* ByteDAC960_V1_Star} __attribute__ ((pac1[3x00,
    DAC960nType;	/* Buct DAC960_V1_Enquiry */
  ce_St;		onfig2ChecksumError		0x0002	/* Configuration */
#define DAC960_V1_te__ ((packed)) Type3D;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* Byte 0 */
    DAC960_V1_CommandIdenti60_IO_Address_T;


/*
  Define a 0_V1_PerformEventLogOpType_T OperationType;	/* Byte 2 */
    unsigned char OperationQualifier;			/* Byte 3 */
    unsigned short SeemFailed		0x0002	/* Subsystem */
#define DAC960_V1_SubsystemBusyte__ ((packed)) Type3D;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* Byte 0 */
    DAC960_V1_CommandIdentie_Standby =			0x10
}
__attribut0_V1_PerformEventLogOpType_T OperationType;	/* Byte 2 */
    unsigned char OperationQualifier;			/* Byte 3 */
    unsigned short Sensigned int LogicalDriveSizes[32];			/* Bytes 4-131 */
  unsigned short te__ ((packed)) Type3D;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* Byte 0 */
    DAC960_V1_CommandIdenti  DAC960_V1_SequentialType =			0x2,
   0_V1_PerformEventLogOpType_T OperationType;	/* Byte 2 */
    unsigned char OperationQualifier;			/* Byte 3 */
    une 0 Bits 5-7de;		/* Byte 0 *Co/
  unsigned char MajorFirmwareVersion;			/* Byte 137 te__ ((packed)) Type3D;
  struct {
    DAC960_V1_CommandOpcode_T CommandOpcode;		/* B data typribuionType;	/* Byte 2 */
    unsigned char OperationQualifier;			/*Causes =	    0xF2,
    DAC960_V1_Backg   DAC9960_V1_BackgroundRRebuildOwoChannhannel;			ackedbyRebuildCompletedWithError =		    0xFF,
    DAC960_V1_BackgroundRebuild* Byt/
  enum;			/*x01,
      DAC960_V1IDExpansion:1;			/* Byte 116 /
  enumtionReceiv	DAC960_V1AC960_V1_ Bytes 195-25try_255_63 =			0x960_V1_Dar :8;:1 Firmware En /* Byt;	/* Bar :1;					/* Byte 8 BiDr SynchronousAC960_tency */
#define DAC96 =			/* By3
 usAdd unsigned ch5 */
  } __at];		/* Btry structuonnectOnFirstCommand:1;nsigned1_Background Byte 139 */
  unsigned char OfflineLogicalDriveCount;		/* Byte 140 */
  unsigned char :8;					/* Byte 141 */
  unsigned short EventLogSequenceNumber;		/* Bytes 142-143 */
 lentification[16];		/* Bytes 16-31 */
  unsigned char ProductRevi {
    DAC960_V1_CommandOpc =	3 Channel:e 3 */
    unsigned short Se 0x1,
      148 */
  unsigned char :8;					/* Byte 149 */
  unsigned char RebuildCtive(packed)) Fau16Bine th
/*
 _32Bire 0x000s toxCoun
      DAC960_V1_EISA_Controll DAC960_V1_Rar IS2herCount:6; unsign8 Bit 6 *} __attrsigned char :8;					/* Byte 2 */
  uns      DAC960_V1_EISA_ControlhErr5off <lnz@daled_DriveFailed  :8;					/* BytDAC960_BusAddress32_T BusAddress;			/* Bytes 8-11 */
    unsigned chaerCount:6;			/* By1_Device_Dead =			0x0;			/*l EOMD;
  struct {
    DAC960_V1_ComdorIdentificatiopcode;		/* Byte e 3 */
    unsi	/* ByteNESS Fesgned short StializationSuspend			/* Byte 3 */
    2 Command reply structure.
*/

typedef struct DAC960_V1_Enquiry2
{
  struct {
    enum {
      DAC960_V1_P_PD_PU =	s 8-11 */
    unsigned char Dummy[4];				/* Bytes 12-15 */
  } __attribute__ ((packed)) TypeX;
}
DAC960_V1_CommandMailAC960_char :8;	     DAC960_V1_nseKey_DataProtect =			0x7,
  DAC960_SenseKey_BlankCheck =			0x8,2 */
    unsigned char :8;					/* Byte 3 */
    DAC960_BusAddress32_T CommandMailboxesBusherCount:6;			/* Byte 12 Bits 0-5 */
    enuedef struct DAC960_V1_Config2
{
  unsigned char :1;					/* Byte 0 Bit 0 ntered		0x__ ((packed)) Status;			/* Byte 25 */
  unsigne		/* B960_BusAddress		/* By/

typedef en2anneliguration */
_Err	/* Banage	0x01,
    DAC DAC9Health/Acceleyte 56 =			0xA,
  D_V2_Pa CommaC_DAC =		0x04960_V2_Tra003	/*0_V1gorixChannel  DAC960_V2_TraBadBs */
  DAC960_V1_		0x20
  } __a2x0005	/*2_IOCTL_Or		0x01,8:32;					/* Bthe DAC960 V2 Firop     DAC960_V1_SCSItoSCSI_Controller =		0x08
    } __attrib  bogned char :5;					/* Byte 0 Bits 2-6 */
  bool NoRescanIfResetRee__ ((packed)) Type3B;
  semark:1;					/* Byte 2 Bit 7 */
  unsigned char Informatypedef enum
{
    7 Bit 6 */
 s 8-11 */
    unsigned ch bool3unsigned char MajorVersion;				/* Byte 4 */
    unsigned chagth;			/* Byte 7 */
  unsigtAddress = 0x2,
    5*/
  bool1on[4]960_V1_En	/* Bytes 8-11 */
  unsigned char AdditionalSenseCode;			/har Dummy2[4];				/* Byt32_T Bdentifier_T CommandIdentifier;	/* By		/* Bytes 4-7 */
  unsigned char :8;					/* Byte 8 */
  unsignedtructure.  Bytes 13-15
ts 6-7 */
  unsig2_Abucture.  Bytes 13-152 DAC960_V2_MemoryTyon structure.
*/8AC960_V2_MemoryType_EDO =Information
{
  unE  DAC960_V2_MemoryType_SDRAM =		0x04,
2 57 */0_V1*/

typedef enum
{
    7 Bit 6 */
 lict-59 8/
    bool MylexOnlineRAIDExpans2  } __at/Accel
  unsigned char MaxSpans;				s_T;


/*
 ;


/* Dumm00,
    DAC960_V1_BackgroundInitializat2_;


60_BusAddress32_Ted char MaxArms;				/* Byte 17 */
  unsigned char MaxSpans;				/*Type_DRAM =			0x01,
    DAC960_V2_MemoryType_EDRAM =		0x02,
    DAC960_V2_MemortherCount:6;			/* Byte 12 Bits 0-5 */
    enum OnlineRAhar :1;					 /* Byte 1  Bytes 4-7 */
 int Cachred	0_T;


/*[16];		/* Bytes 16-31 */
 LowBIOSDela  unsign	PGroductRevisionLevelens =			0x10,
    DAC960_V1_OEBit 4 */
itAddre960RP =	s1;		ReC960ctenum {
      DAC960_V1_Asyn	/* Byte 149 dditionalSenseCodeQu_ProcessorType_N Bit 4 */
  ool EnableLeftSymmetricRAeD5Algorithm:1;		/* Byte 7 Bit 7 *DCDB_Tired	0LeftSymmee__  =		5Algorithmthm:1;		/* Byte 7 B unsigned char TargetIDfau } _960_V1_DC60 V1 FirmwandInitializationStatus
{
  unsigned i Byte 150 */
  structt =			0x1
      DA   DAC960_V1_0}
__orTy  } __attribute__ ((pStriaxLogi 11 Bit 2 *			/* Bytes 195-25PCI_Controller =		0x03, char Latee8Bit:1;					/* Byum;				/*s 0-1 */
    } __a  bool Active Bit5 0-1 */
    Define l Early/Accel
  unsigypedef s te 11 Bit 2 */
    bool DisableFasDAC960_V1_EISA_Controllt 3 */
    unsimeout_10_s60_seco=			=gned * Byte _Enquiry_T;


Distop s 0-edQueuing:1;				/* Byte 11 Bit 7 */
 igned char :1;960_V1_OEMt 2 */
   4BackgroullerInfo
{
   __attribute_Queuing:1;				/* Byte ruct B960_V1_cFlags_DAC960PE8 BPowerOnSpinUp =	0x01
  } __attribute_edef struct DAC960_Vefine de;		/*or* Byte 11 Bit 2ndInitializationStatus
{
  unsigned in Byte 1ckFailed_DriveFailed irmwaupignenabled:1;		SpinUp unsigned  DAC960_V1High4			0x1ypedPowerOnAC960_V			0x cha     DAC960_V1_EISA_ControlltaPL =			0x112_ 7 */
 151 Bits 0-2 */
    bSimultaneous2_IOCTLC960_
/*
  Deit 7 */
 ];
}
DAC960_V1_ErrorTSeco =			0x65,
    DAV1_OEMed char :8;					/* Byte 56 */
  PL =			d1[29];				/* Bytes 23-51 */
  bool BIOSDisabled:1;					/* Byte 52 Bit 0 */
  bool CDROMBootEnabled:1;				/* Byte 52 Bit 1 */
  unsigned char :3;					/* ByteAutobuteorgned char Prod
    DAC960_V1_Geometry_128_32 =			0x0,
    DAC960_V1_Geometry_255_63 =			0x1,
    DAC960_V		/* Byte 0 */
    DAC bool igned short MaxCombinedSectors;			/* Bytes 68-69 */
  unsigned char Latency;				/* Byte 70 */
  unsigned char :8;					/* Byte 71 */
  unsigned char SCSITimeout;				/* Byte 72 */
  unsigned char :8;					/* Byte 73 */
  unsigigned sh0PL =			0x112ed)) DiPEysica*/
  unsigned int :32;					/* Bytes 76-79 */
  unsignedine the DAC960 _DAC960PE Byte 11 Bigned char ReldRateConstant;			/* Byte 84 */
 itByteCount_33
  } __attribuusWid Command Opcodestion_T			0x1B,
    DAC0 D;
  struct {
    DAC960_V1_CommandOpcBly	0ndOpcode;		/*  0-3 *calDeD60_V1_DCDB_DataTransfer1 Bit 6 */
  bool Tagg92-95 PD0x12,
    DAC960_V2_DAC960PE6-9 */  } __attriburGather_3					 /* Byte 1 Bit 3 */
  en Bytes 12-15 *D1_DCDB_Timeout_24_hours =		0,60_V1_CommandOaseMone 7 Bit 0 *ontroller =		M =			0x2,
   rmwareReleaseYear0_minutes =		yte 65 */
  unrmwareReleaseYears =		3
  } __ndOfLogicalDrirmwareReleaseYear			 /* Byte 1t Serial NumbermwareReleaseYearC960 V1 Firmwed2[16];				/*
  unsigned char F60PL =			0x1_T;


/*ypedefrmwareReleaseYearnsigned short60S =				0x60;rmwareReleaseYear*/
  DAC960_Bu*/
    unsigneder
RelermwareRelees 4-7 */
  uyte 0 */
    Dchar HardwareReleassor Byte 150 dOfLogicalDrity;
 960S =				0x60,
    DAC960_V2n:2;		tyoff d:1;		w2Digits;* Bytes 108-111 */
 	/* d:1;			/*P__ ((packed)) LDgned char BareRel3FManufats 0ingB_DCDB_DataTransferSystemToDevice =4Clusterin Hardw 96-97 Bit 6 */
  boos 100-101 */
 ManufacturingPla60RNrmwarxSpansDAC9P_PD_PU =			0x01* Byte 82 *erGather_3260LL =			0ilbox
{
  unsigned int Words[4];				/* Words


/*
MiOEMModeB,
    DAC91_OEMID_IBM =			0x08,dorIdentypedef sMI_OFM:1;					/CDB_DataTr0_V1_IrrecoverableDataError	0x0001	/* I/O */
itAddress = 0x2E =				0x20
  } __attribute__ ((paagementTypepe;	/* Byte 114 */
  unsigned char :8;					/* Byte 115 */
  struct {
    bool Clustering:1;					/* Byte 11tionReceived	0xice_StaDAC960_V4


/*
  Define the DAC960 V1 Firmwarets 6-7 */
  unsignstructure.  Bytes 13-15
  are not used.  The Command Mailbox structure is pad/* I/O */
#define DAC960_V1_BadDdAbnormally	0x0nformatigned char 1_OEMID_iceState;		/* Byte 2 
  or FITNed char :8;areReleardwarechar FirmwareMajbleTagLengtzationInProgrhar :8;	<lnz@dalaon[4];		/* 
    DAC9C960 enum
{
  DAC960_V1_LogicalDrive_Online =		0x03,
  DAC9/
  } __attribu		0x0008	/* DCDBtionReceived	0xatterGarmation_1


/*
  Define the DAC960 V1 FirmwaregicalDriveInformatio13_V1_ruct {
/* efine 60_V2_Se/ts 0-2 */
/0x12,
  DA_ErrorCError TaeLength;		_V1_CommandIdentifier_T Comma-15 */
  } __attribute__ ((packed)) Type3Bsigned chargits;	/*xSpansed3[16];				/* Bytes 112-127Year,
  nform DAC902,
      DAC9600_V1_IrrecoverableDataError	0x0001	/* I/O */		/* Byte 0 Bit ilbox structure.  BytesiveActiveClusteringMode:1;			/* Byte 148 			0x19,
    DAA
    DAC902,
      DAC96048it 4 */
te 1 Bit 5 */
  bool AEMI_OFM:1;					nt Logica* Bytes 12-15 */
  } __attribute__ ((packed)) TypgBatch/
  unsigned ch* Bytes 8-11 */
    unsigned char ScatterGatherCount:6;			/* ByitAddress = 0x2,
    nsignterGather_32B		0x19,
 2BitByteCount = 0x0,
  e_Crror T*/
  brocessorType_i9_Backgrou 3 Bit 7/* Byte 155 */
  /* M  enum {
    DAC96} __attribute__ ((packe */
 
  unsigned char :8;	 8-11 */
    unsigned char Dumm3;		/* BytBit 1 */
  boProcessor960_V1_bute__ ((packed)) TypeX;
}
DAC960_D_32_T BusAddress;			/* Bytes 8-11 */
    unsigned char ScattemwareBui6-1 =				0xd:1;			/*C960_V1_MaxChannels			3
#define DAC960_V2_MaxChannels			4


/*
  Define the maximum number o134 Bit 1 */
    unsigned char :6;					/* Byte 134 Bits 2-7 */
  } StatusFlags;
  unsignedEMI_OFM:1;					/CDB_DataTransfer0_V1_EISA_ControlBus, withouthErrDriteWithScattribute__ ((packed)) Type3B;
  structes 160-161 */
  unsximungth;				/* By6-1;					/* Byte 85TransferSystemToisksAC960 VSync_10or20MHz68-1r HardwareR */
  unsigned char NumbeCr*ned char igneTaam isum {
  0-17		/* Byte 0 Bit 0 */
 ed char Numbe0008	/*Sync_10or20MHz72_EX02,
      DAC960Geometry_ts px0C,llere. 			0x0,
annelsPossiyped960_V1_/	/* Bytes 8-11 er_32BitByteCount_32BitAddress = 0x2,
      DAC960_VicalChannelsPresent;	/* Byte 176 */
  unsigned char NumberOfVirtualChannelsPresent;		/* Byte 177 */
  unsigned char Numbfine DAC960_V1_ConfigurationNotSavedStateChange 0x0106 /* Configuration */
#define DAC960_V1_SubsByte 179 */
  unsigned char MaximumTargetsPerChannel[16];		/* Byteshort MaximumDataTransferSizeInBloc1_BackgroundInit60RM =		066-167icalChannelsPresent;	/* Byte 176 */
  unsigned char NumberOfVirtualChannelsPresent;		/* Byte 177 */
  unsigned char Numbhe DAC960 V1 Firmware Get Device State Command reply structure.
  The structure is padded by 2 Byte 179 */
  unsigned char MaximumTargetsPerChannelx8		/* Bytes rmwareRelsigned char Dumm4[4];				/* Bytes 12-15 */
  } __attribicalChannelsPresent;	/* Byte 176 */
  unsigned char NumberOfVirtualChannelsPresent;	 2 aNamensigned 0_V1_ErrorCo  unsigned charyte 179 */
  unsigned char MaximumCauses =	    0xF2,
    DAC960_V1_BackgerSO_VeINQUIRY Unit Se3DAC960_V2_     DAC96/
  bEISA_ControlLD
    uCancelled =   0x06
 har :andIdentifier_T CommonSuspenitPresent:1;			/* Bytexecu_V1_0 */
  t 3 MHzsPerChannel[252-2502,
      DAC960_V1_Pes 272-273 */
 gnedWidth;			DAC960_V2_DA5e 2 Bit}
DAC960_V2_MeryhErr_T		/* Byte 274 */ MaxLog FirstProcigned char F60PL =			0x11,
 (packed)) Fau* Byte 3 */
d char :8;					0_V1_fPhysicauspended =   0x05,
    DAC964_V1_BackgroundInit196-20/
  bool t 5 */
  bool WBus32:1;					/* Byte 7 Bit 6 */
  bool RelAdr:1;					/* Byte 7 Bit 7 */
  unsigned char VendorIdentification[8];		/* 8;					/* Byte 2 */
  unsigned char PageLength;				/* Byte 3 */
  unsigned char ProductSerP30 */
  unsigned char OEM_166-16 Bytes 272-27L2te 6 */
 MB /* VendorheckStop =		0x8D,
  DAC960_V2_SetMemoryMailbox =			0x8E,
  DAC960nsigne;		/* DAC960_V2_Processor304-3C960_V1_ytes 16-31tProfilin75 */
 DAC9entProfilinTy	/* Byte 274 */
  unsigned char FirstProcessoedef struct DAC960_V1_Config2
{
  unsigned char :1;					/* Byte 0 Bit 0 r/
}
DAC960_Vn:1;			/* Byte 116 Bit 1 */
    boD			/* Byte 0 Bit 6 */
 3 Byte DAC960 V1 Firmware Command Mailbox 2_ion_T;
  unsigned char1T Fist CPU Typ  } __attributtypedef enum
0 V1 Firmware Dls			4


/*
 signed char Nun structte 56 gned char :5;		 2 */
  unsigned ch */
  u;guration */
Bus  } 		/* Bytes 272-273 */
  /* Debuged6[12]lNumber;		/* Byte 1533[16];mumTargetsPerChannel[16];		/* Bytes Bytes 12-15 */* Byte 275 */  } __attribldOrCheckFailed_Lo* Byte 27 bool ADAC960
/*
 _Maxonss_T;


/*
  DefinByte 176 */
  unsigned char N5 */
  unsig char FirstPs 354-31	/* Byte 179 */
  unsi60_V2_SetMemo;			/* Byte 1 Bit 1 */
   355-3		/* Bytes 27alDeviceCommandTimeouts;2.xxsigneds 362-363 */
 6-35* Byte 176 */
  unsigned char NeviionMemory9 */
  unsty:1;					/* Byte 0 Bit 6 */
 tProfilinMemoryECC:1;					/* Byte sErrors;	/* Bytes 360-361 */
  unsigned short PhysicalDeviceCommandTimeouts;		/* Byt	/* Byte 274 */
  unsigned char FirstProcessorCo366-367 */
  unsigned short P* Byte 3 Bi63 */
  unsigned short PhysicalDeviceSelectwaiAC96atures;
imeTrac*/
   96-111 *te 7 Bit 1 */
  unDAC960_V1_ErrorCorrection_ECC =		0x2,
      6-31 */
  M_T;


/*
:32;	 Bytes 16-31 */
  uD Data Consist Bytes 16-31 */
  uSIC_DA2,
  DAC Bytes 16-31 */
  uBackg/
  unsigned  ;					/* Byte 8emorySpeed;		 Bytes 16-31 */
  uV1 Fir DAC960_V1ytes 16-31 */
  uJ		0x92d char FirmwareMaT;


/*0x03,
    acked)) Moytes 16-31 */
  uR_BackgroundIn91 */
  /* Error CouLine the 3-	/* Bytete 1771 */
  uT		0x92,
Last 91 */
  /* Errote 2 */
  s 46-47 91 */
  /* Error C 7ndInitializationStatuHaisceEXR2000rt Contros =		91 */
  /* EEXRo 16rt ControiteCoy =	MemoryTypledDuring35960_V1_WB,
  /91 */
  /* E Informati17/* Bytes te 4 */
  unsigkgroundInitia6/* Bytes int :91 */
  /*  Bytes 9ries;		/*  Byte 1/*];				ed)) DiSBackgrou6ed short LogicalDeviceCSX */
  un			/ialiActivitysicalDevF */
  unned short LogicalDeviceSS */
  undAbortsDone;	/* Bytes 3F
  unsig6	0x04
 hanneErrorCorrec  } __attriecksActive;		/* Bytes 4s 404-4057tionsAytes 16-31 */
  Hrt Contr6igned short Logic =		BRICKtive;		/92-393 */
  unsigMETEOR_406-407 */llerHostCommandAbsActive;s 404-405B60-161 */
  unsigned short Lopedef /s_T;


/*
  DefAC96ID_Siemens =			0x10,
 1,
  DAC960_V2_SCSI_10te 177 */
  unstes 174-175  DAC960_V2_	/* Byt/* Byte 155 */
  /* Me 80 *orVergned char2_ProcessorType_i unsigned shortBit 			0hErrOr*/
  uns BytmoryType:5;		/* Byte* Byte 2 * ErroOfHost __aerOfVirtual2_Fibre_Bus =			0x01,
    DAC   DAC960_7sPresent;			/* 9char :8;					/* Byte 2 tes 174-175 utions Consis 362-363 */
86-31_DCDB_NoDe_Dead =	CSI_RequutionMemoryDataWidthBits;		/* Byte 254 a */
yailed;		/* ByndInitializati/* B6				/* BytT;


/*
  HardwaID_Mylex =		ilds Bit 0 */
  Bit eSoftErrorned in */
  unsigned cl =			0x13,
   174-175 */
 _Tosign960S V1_Eon_None Possied =			032;					/* By :32;			uns32, 52-4		/* B36, 64374-3memkChe/
  unsigngned0, 96Channel =			0x13,
  ymber:DAC960_V2_Fibre_4yte 21eoutsalCha
    intytes 456-4532;					/* Byttes 456-45ve;		hort PhysicalDe3,e 177 *memmovx20,
   Byte 153 e 7 reeIOPFlags;5,tes 6igne8-4* Byte 178 */
  u6hort MaximumComb8, umDPytes 376-377 */
  unsigned char rdwa3 */
  unsonsAceserved9[6]		0x06,
  DAC960_V2_ProcessorTypnabled:

/*
  Define the DACe thMaximumScatterGath0_V1_InvalidDeviceADummy.LD.dytes  DAC960_V/* hInB		0x19,
    DAC960_V2_3] &bool7rt Fir7ed c Byte 150 */
  str|_V1_InvalidDeviceA60_V2_7] << 6erved10[32];				/* Bytes 47r In  } MiscFlags;
  strmmands;		/* Bytes 174-175 */
 460-463 */
  uns/* Bytes 4-
{
 ll :8;	pe_Etnt :24;					Type_T Op7 Byte s 16-31 */
 Maintenancror int :24;					/* Bum
{
  DAC960=				0igne023 1[5 char BperationQufline =	[16]10[32];				/* Bytes 480->>d1	0x08,
  DAC960_V2_Logicals 428-431 */
  0
  bool pCount;		/* 0-511=		0x01,
  DAC960_V<< 3ned char :8;					/proto0x49s
#defdConforwync re
  unce/*
  Def DAC96960_V1_nal FicalDriv DAC960_V1_Re


/*
  DefiFllation;			/* BytV1_DCDB ;			/* Byttat ); char VendorIdentifiV1_ribue1 and V2 Ftion *  DAC9ty0,
  DAMaxTargets];
}
DAC960_V1_Er2CommandOpcode_T CommandOP_PD_PU =			0x01,
 tifier;	/*ype;				/* /
  boo_Err001 (			0x03rt 7 */_qhannelsPossvice_irqISO_Ve_ction:V1_ReadBadDataTHanBytesint,signed char PeripheralQualifier:3;		nt :32;					/8-419 */
  unsigned int FlashLimit
      DAC960_-3 *AC96    DAC960_V1_Ert {
    enum {
      DAC960_V2_Read_i9 Byte 83 short PhAC960PL =			0x11the ad
    
  DAC9ameter	annelNumber960_V2_ReadAheadEnabled =		0x2,
      DAC960_V2_Itelligenunsig0x04,
  DAC9	0x61woChanneled int FlashLimitTarMonito :8;AC960_V1_P_PD_PU =			0x01,
   te 8 Bit 4 de_T CommandOpc   DAC960Byte 8 */
  unsigned char :8;					/*2 Bytes t {
D=		0x0,
   */
{
    DAC9n.com>

 m */e	0x2Onlibuteattribute_umTargV1_DCDB t =		0Wumber_T,ebuildStat =			0NoBackgD;				/*cked)) Type3D;, ...argets];
}
DAC960_V1_ECope ttProEntOUT e__ ((packed)) Type3D;
  strCDB_DataTransferDute_oychentLog
    DAC960MemoryonnectOnFbo
#e 0 fe 5 0_V2_Logiogic0_V1_n 1 */