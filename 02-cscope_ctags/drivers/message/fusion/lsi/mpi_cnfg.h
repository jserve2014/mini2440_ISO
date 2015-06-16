/*
 *  Copyright (c) 2000-2008 LSI Corporation.
 *
 *
 *           Name:  mpi_cnfg.h
 *          Title:  MPI Config message, structures, and Pages
 *  Creation Date:  July 27, 2000
 *
 *    mpi_cnfg.h Version:  01.05.18
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  05-08-00  00.10.01  Original release for 0.10 spec dated 4/26/2000.
 *  06-06-00  01.00.01  Update version number for 1.0 release.
 *  06-08-00  01.00.02  Added _PAGEVERSION definitions for all pages.
 *                      Added FcPhLowestVersion, FcPhHighestVersion, Reserved2
 *                      fields to FC_DEVICE_0 page, updated the page version.
 *                      Changed _FREE_RUNNING_CLOCK to _PACING_TRANSFERS in
 *                      SCSI_PORT_0, SCSI_DEVICE_0 and SCSI_DEVICE_1 pages
 *                      and updated the page versions.
 *                      Added _RESPONSE_ID_MASK definition to SCSI_PORT_1
 *                      page and updated the page version.
 *                      Added Information field and _INFO_PARAMS_NEGOTIATED
 *                      definitionto SCSI_DEVICE_0 page.
 *  06-22-00  01.00.03  Removed batch controls from LAN_0 page and updated the
 *                      page version.
 *                      Added BucketsRemaining to LAN_1 page, redefined the
 *                      state values, and updated the page version.
 *                      Revised bus width definitions in SCSI_PORT_0,
 *                      SCSI_DEVICE_0 and SCSI_DEVICE_1 pages.
 *  06-30-00  01.00.04  Added MaxReplySize to LAN_1 page and updated the page
 *                      version.
 *                      Moved FC_DEVICE_0 PageAddress description to spec.
 *  07-27-00  01.00.05  Corrected the SubsystemVendorID and SubsystemID field
 *                      widths in IOC_0 page and updated the page version.
 *  11-02-00  01.01.01  Original release for post 1.0 work
 *                      Added Manufacturing pages, IO Unit Page 2, SCSI SPI
 *                      Port Page 2, FC Port Page 4, FC Port Page 5
 *  11-15-00  01.01.02  Interim changes to match proposals
 *  12-04-00  01.01.03  Config page changes to match MPI rev 1.00.01.
 *  12-05-00  01.01.04  Modified config page actions.
 *  01-09-01  01.01.05  Added defines for page address formats.
 *                      Data size for Manufacturing pages 2 and 3 no longer
 *                      defined here.
 *                      Io Unit Page 2 size is fixed at 4 adapters and some
 *                      flags were changed.
 *                      SCSI Port Page 2 Device Settings modified.
 *                      New fields added to FC Port Page 0 and some flags
 *                      cleaned up.
 *                      Removed impedance flash from FC Port Page 1.
 *                      Added FC Port pages 6 and 7.
 *  01-25-01  01.01.06  Added MaxInitiators field to FcPortPage0.
 *  01-29-01  01.01.07  Changed some defines to make them 32 character unique.
 *                      Added some LinkType defines for FcPortPage0.
 *  02-20-01  01.01.08  Started using MPI_POINTER.
 *  02-27-01  01.01.09  Replaced MPI_CONFIG_PAGETYPE_SCSI_LUN with
 *                      MPI_CONFIG_PAGETYPE_RAID_VOLUME.
 *                      Added definitions and structures for IOC Page 2 and
 *                      RAID Volume Page 2.
 *  03-27-01  01.01.10  Added CONFIG_PAGE_FC_PORT_8 and CONFIG_PAGE_FC_PORT_9.
 *                      CONFIG_PAGE_FC_PORT_3 now supports persistent by DID.
 *                      Added VendorId and ProductRevLevel fields to
 *                      RAIDVOL2_IM_PHYS_ID struct.
 *                      Modified values for MPI_FCPORTPAGE0_FLAGS_ATTACH_
 *                      defines to make them compatible to MPI version 1.0.
 *                      Added structure offset comments.
 *  04-09-01  01.01.11  Added some new defines for the PageAddress field and
 *                      removed some obsolete ones.
 *                      Added IO Unit Page 3.
 *                      Modified defines for Scsi Port Page 2.
 *                      Modified RAID Volume Pages.
 *  08-08-01  01.02.01  Original release for v1.2 work.
 *                      Added SepID and SepBus to RVP2 IMPhysicalDisk struct.
 *                      Added defines for the SEP bits in RVP2 VolumeSettings.
 *                      Modified the DeviceSettings field in RVP2 to use the
 *                      proper structure.
 *                      Added defines for SES, SAF-TE, and cross channel for
 *                      IOCPage2 CapabilitiesFlags.
 *                      Removed define for MPI_IOUNITPAGE2_FLAGS_RAID_DISABLE.
 *                      Removed define for
 *                      MPI_SCSIPORTPAGE2_PORT_FLAGS_PARITY_ENABLE.
 *                      Added define for MPI_CONFIG_PAGEATTR_RO_PERSISTENT.
 *  08-29-01 01.02.02   Fixed value for MPI_MANUFACTPAGE_DEVID_53C1035.
 *                      Added defines for MPI_FCPORTPAGE1_FLAGS_HARD_ALPA_ONLY
 *                      and MPI_FCPORTPAGE1_FLAGS_IMMEDIATE_ERROR_REPLY.
 *                      Removed MPI_SCSIPORTPAGE0_CAP_PACING_TRANSFERS,
 *                      MPI_SCSIDEVPAGE0_NP_PACING_TRANSFERS, and
 *                      MPI_SCSIDEVPAGE1_RP_PACING_TRANSFERS, and
 *                      MPI_SCSIDEVPAGE1_CONF_PPR_ALLOWED.
 *                      Added defines for MPI_SCSIDEVPAGE1_CONF_WDTR_DISALLOWED
 *                      and MPI_SCSIDEVPAGE1_CONF_SDTR_DISALLOWED.
 *                      Added OnBusTimerValue to CONFIG_PAGE_SCSI_PORT_1.
 *                      Added rejected bits to SCSI Device Page 0 Information.
 *                      Increased size of ALPA array in FC Port Page 2 by one
 *                      and removed a one byte reserved field.
 *  09-28-01 01.02.03   Swapped NegWireSpeedLow and NegWireSpeedLow in
 *                      CONFIG_PAGE_LAN_1 to match preferred 64-bit ordering.
 *                      Added structures for Manufacturing Page 4, IO Unit
 *                      Page 3, IOC Page 3, IOC Page 4, RAID Volume Page 0, and
 *                      RAID PhysDisk Page 0.
 *  10-04-01 01.02.04   Added define for MPI_CONFIG_PAGETYPE_RAID_PHYSDISK.
 *                      Modified some of the new defines to make them 32
 *                      character unique.
 *                      Modified how variable length pages (arrays) are defined.
 *                      Added generic defines for hot spare pools and RAID
 *                      volume types.
 *  11-01-01 01.02.05   Added define for MPI_IOUNITPAGE1_DISABLE_IR.
 *  03-14-02 01.02.06   Added PCISlotNum field to CONFIG_PAGE_IOC_1 along with
 *                      related define, and bumped the page version define.
 *  05-31-02 01.02.07   Added a Flags field to CONFIG_PAGE_IOC_2_RAID_VOL in a
 *                      reserved byte and added a define.
 *                      Added define for
 *                      MPI_RAIDVOL0_STATUS_FLAG_VOLUME_INACTIVE.
 *                      Added new config page: CONFIG_PAGE_IOC_5.
 *                      Added MaxAliases, MaxHardAliases, and NumCurrentAliases
 *                      fields to CONFIG_PAGE_FC_PORT_0.
 *                      Added AltConnector and NumRequestedAliases fields to
 *                      CONFIG_PAGE_FC_PORT_1.
 *                      Added new config page: CONFIG_PAGE_FC_PORT_10.
 *  07-12-02 01.02.08   Added more MPI_MANUFACTPAGE_DEVID_ defines.
 *                      Added additional MPI_SCSIDEVPAGE0_NP_ defines.
 *                      Added more MPI_SCSIDEVPAGE1_RP_ defines.
 *                      Added define for
 *                      MPI_SCSIDEVPAGE1_CONF_EXTENDED_PARAMS_ENABLE.
 *                      Added new config page: CONFIG_PAGE_SCSI_DEVICE_3.
 *                      Modified MPI_FCPORTPAGE5_FLAGS_ defines.
 *  09-16-02 01.02.09   Added MPI_SCSIDEVPAGE1_CONF_FORCE_PPR_MSG define.
 *  11-15-02 01.02.10   Added ConnectedID defines for CONFIG_PAGE_SCSI_PORT_0.
 *                      Added more Flags defines for CONFIG_PAGE_FC_PORT_1.
 *                      Added more Flags defines for CONFIG_PAGE_FC_DEVICE_0.
 *  04-01-03 01.02.11   Added RR_TOV field and additional Flags defines for
 *                      CONFIG_PAGE_FC_PORT_1.
 *                      Added define MPI_FCPORTPAGE5_FLAGS_DISABLE to disable
 *                      an alias.
 *                      Added more device id defines.
 *  06-26-03 01.02.12   Added MPI_IOUNITPAGE1_IR_USE_STATIC_VOLUME_ID define.
 *                      Added TargetConfig and IDConfig fields to
 *                      CONFIG_PAGE_SCSI_PORT_1.
 *                      Added more PortFlags defines for CONFIG_PAGE_SCSI_PORT_2
 *                      to control DV.
 *                      Added more Flags defines for CONFIG_PAGE_FC_PORT_1.
 *                      In CONFIG_PAGE_FC_DEVICE_0, replaced Reserved1 field
 *                      with ADISCHardALPA.
 *                      Added MPI_FC_DEVICE_PAGE0_PROT_FCP_RETRY define.
 *  01-16-04 01.02.13   Added InitiatorDeviceTimeout and InitiatorIoPendTimeout
 *                      fields and related defines to CONFIG_PAGE_FC_PORT_1.
 *                      Added define for
 *                      MPI_FCPORTPAGE1_FLAGS_SOFT_ALPA_FALLBACK.
 *                      Added new fields to the substructures of
 *                      CONFIG_PAGE_FC_PORT_10.
 *  04-29-04 01.02.14   Added define for IDP bit for CONFIG_PAGE_SCSI_PORT_0,
 *                      CONFIG_PAGE_SCSI_DEVICE_0, and
 *                      CONFIG_PAGE_SCSI_DEVICE_1. Also bumped Page Version for
 *                      these pages.
 *  05-11-04 01.03.01   Added structure for CONFIG_PAGE_INBAND_0.
 *  08-19-04 01.05.01   Modified MSG_CONFIG request to support extended config
 *                      pages.
 *                      Added a new structure for extended config page header.
 *                      Added new extended config pages types and structures for
 *                      SAS IO Unit, SAS Expander, SAS Device, and SAS PHY.
 *                      Replaced a reserved byte in CONFIG_PAGE_MANUFACTURING_4
 *                      to add a Flags field.
 *                      Two new Manufacturing config pages (5 and 6).
 *                      Two new bits defined for IO Unit Page 1 Flags field.
 *                      Modified CONFIG_PAGE_IO_UNIT_2 to add three new fields
 *                      to specify the BIOS boot device.
 *                      Four new Flags bits defined for IO Unit Page 2.
 *                      Added IO Unit Page 4.
 *                      Added EEDP Flags settings to IOC Page 1.
 *                      Added new BIOS Page 1 config page.
 *  10-05-04 01.05.02   Added define for
 *                      MPI_IOCPAGE1_INITIATOR_CONTEXT_REPLY_DISABLE.
 *                      Added new Flags field to CONFIG_PAGE_MANUFACTURING_5 and
 *                      associated defines.
 *                      Added more defines for SAS IO Unit Page 0
 *                      DiscoveryStatus field.
 *                      Added define for MPI_SAS_IOUNIT0_DS_SUBTRACTIVE_LINK
 *                      and MPI_SAS_IOUNIT0_DS_TABLE_LINK.
 *                      Added defines for Physical Mapping Modes to SAS IO Unit
 *                      Page 2.
 *                      Added define for
 *                      MPI_SAS_DEVICE0_FLAGS_PORT_SELECTOR_ATTACH.
 *  10-27-04 01.05.03   Added defines for new SAS PHY page addressing mode.
 *                      Added defines for MaxTargetSpinUp to BIOS Page 1.
 *                      Added 5 new ControlFlags defines for SAS IO Unit
 *                      Page 1.
 *                      Added MaxNumPhysicalMappedIDs field to SAS IO Unit
 *                      Page 2.
 *                      Added AccessStatus field to SAS Device Page 0 and added
 *                      new Flags bits for supported SATA features.
 *  12-07-04  01.05.04  Added config page structures for BIOS Page 2, RAID
 *                      Volume Page 1, and RAID Physical Disk Page 1.
 *                      Replaced IO Unit Page 1 BootTargetID,BootBus, and
 *                      BootAdapterNum with reserved field.
 *                      Added DataScrubRate and ResyncRate to RAID Volume
 *                      Page 0.
 *                      Added MPI_SAS_IOUNIT2_FLAGS_RESERVE_ID_0_FOR_BOOT
 *                      define.
 *  12-09-04  01.05.05  Added Target Mode Large CDB Enable to FC Port Page 1
 *                      Flags field.
 *                      Added Auto Port Config flag define for SAS IOUNIT
 *                      Page 1 ControlFlags.
 *                      Added Disabled bad Phy define to Expander Page 1
 *                      Discovery Info field.
 *                      Added SAS/SATA device support to SAS IOUnit Page 1
 *                      ControlFlags.
 *                      Added Unsupported device to SAS Dev Page 0 Flags field
 *                      Added disable use SATA Hash Address for SAS IOUNIT
 *                      page 1 in ControlFields.
 *  01-15-05  01.05.06  Added defaults for data scrub rate and resync rate to
 *                      Manufacturing Page 4.
 *                      Added new defines for BIOS Page 1 IOCSettings field.
 *                      Added ExtDiskIdentifier field to RAID Physical Disk
 *                      Page 0.
 *                      Added new defines for SAS IO Unit Page 1 ControlFlags
 *                      and to SAS Device Page 0 Flags to control SATA devices.
 *                      Added defines and structures for the new Log Page 0, a
 *                      new type of configuration page.
 *  02-09-05  01.05.07  Added InactiveStatus field to RAID Volume Page 0.
 *                      Added WWID field to RAID Volume Page 1.
 *                      Added PhysicalPort field to SAS Expander pages 0 and 1.
 *  03-11-05  01.05.08  Removed the EEDP flags from IOC Page 1.
 *                      Added Enclosure/Slot boot device format to BIOS Page 2.
 *                      New status value for RAID Volume Page 0 VolumeStatus
 *                      (VolumeState subfield).
 *                      New value for RAID Physical Page 0 InactiveStatus.
 *                      Added Inactive Volume Member flag RAID Physical Disk
 *                      Page 0 PhysDiskStatus field.
 *                      New physical mapping mode in SAS IO Unit Page 2.
 *                      Added CONFIG_PAGE_SAS_ENCLOSURE_0.
 *                      Added Slot and Enclosure fields to SAS Device Page 0.
 *  06-24-05  01.05.09  Added EEDP defines to IOC Page 1.
 *                      Added more RAID type defines to IOC Page 2.
 *                      Added Port Enable Delay settings to BIOS Page 1.
 *                      Added Bad Block Table Full define to RAID Volume Page 0.
 *                      Added Previous State defines to RAID Physical Disk
 *                      Page 0.
 *                      Added Max Sata Targets define for DiscoveryStatus field
 *                      of SAS IO Unit Page 0.
 *                      Added Device Self Test to Control Flags of SAS IO Unit
 *                      Page 1.
 *                      Added Direct Attach Starting Slot Number define for SAS
 *                      IO Unit Page 2.
 *                      Added new fields in SAS Device Page 2 for enclosure
 *                      mapping.
 *                      Added OwnerDevHandle and Flags field to SAS PHY Page 0.
 *                      Added IOC GPIO Flags define to SAS Enclosure Page 0.
 *                      Fixed the value for MPI_SAS_IOUNIT1_CONTROL_DEV_SATA_SUPPORT.
 *  08-03-05  01.05.10  Removed ISDataScrubRate and ISResyncRate from
 *                      Manufacturing Page 4.
 *                      Added MPI_IOUNITPAGE1_SATA_WRITE_CACHE_DISABLE bit.
 *                      Added NumDevsPerEnclosure field to SAS IO Unit page 2.
 *                      Added MPI_SAS_IOUNIT2_FLAGS_HOST_ASSIGNED_PHYS_MAP
 *                      define.
 *                      Added EnclosureHandle field to SAS Expander page 0.
 *                      Removed redundant NumTableEntriesProg field from SAS
 *                      Expander Page 1.
 *  08-30-05  01.05.11  Added DeviceID for FC949E and changed the DeviceID for
 *                      SAS1078.
 *                      Added more defines for Manufacturing Page 4 Flags field.
 *                      Added more defines for IOCSettings and added
 *                      ExpanderSpinup field to Bios Page 1.
 *                      Added postpone SATA Init bit to SAS IO Unit Page 1
 *                      ControlFlags.
 *                      Changed LogEntry format for Log Page 0.
 *  03-27-06  01.05.12  Added two new Flags defines for Manufacturing Page 4.
 *                      Added Manufacturing Page 7.
 *                      Added MPI_IOCPAGE2_CAP_FLAGS_RAID_64_BIT_ADDRESSING.
 *                      Added IOC Page 6.
 *                      Added PrevBootDeviceForm field to CONFIG_PAGE_BIOS_2.
 *                      Added MaxLBAHigh field to RAID Volume Page 0.
 *                      Added Nvdata version fields to SAS IO Unit Page 0.
 *                      Added AdditionalControlFlags, MaxTargetPortConnectTime,
 *                      ReportDeviceMissingDelay, and IODeviceMissingDelay
 *                      fields to SAS IO Unit Page 1.
 *  10-11-06  01.05.13  Added NumForceWWID field and ForceWWID array to
 *                      Manufacturing Page 5.
 *                      Added Manufacturing pages 8 through 10.
 *                      Added defines for supported metadata size bits in
 *                      CapabilitiesFlags field of IOC Page 6.
 *                      Added defines for metadata size bits in VolumeSettings
 *                      field of RAID Volume Page 0.
 *                      Added SATA Link Reset settings, Enable SATA Asynchronous
 *                      Notification bit, and HideNonZeroAttachedPhyIdentifiers
 *                      bit to AdditionalControlFlags field of SAS IO Unit
 *                      Page 1.
 *                      Added defines for Enclosure Devices Unmapped and
 *                      Device Limit Exceeded bits in Status field of SAS IO
 *                      Unit Page 2.
 *                      Added more AccessStatus values for SAS Device Page 0.
 *                      Added bit for SATA Asynchronous Notification Support in
 *                      Flags field of SAS Device Page 0.
 *  02-28-07  01.05.14  Added ExtFlags field to Manufacturing Page 4.
 *                      Added Disable SMART Polling for CapabilitiesFlags of
 *                      IOC Page 6.
 *                      Added Disable SMART Polling to DeviceSettings of BIOS
 *                      Page 1.
 *                      Added Multi-Port Domain bit for DiscoveryStatus field
 *                      of SAS IO Unit Page.
 *                      Added Multi-Port Domain Illegal flag for SAS IO Unit
 *                      Page 1 AdditionalControlFlags field.
 *  05-24-07  01.05.15  Added Hide Physical Disks with Non-Integrated RAID
 *                      Metadata bit to Manufacturing Page 4 ExtFlags field.
 *                      Added Internal Connector to End Device Present bit to
 *                      Expander Page 0 Flags field.
 *                      Fixed define for
 *                      MPI_SAS_EXPANDER1_DISCINFO_BAD_PHY_DISABLED.
 *  08-07-07  01.05.16  Added MPI_IOCPAGE6_CAP_FLAGS_MULTIPORT_DRIVE_SUPPORT
 *                      define.
 *                      Added BIOS Page 4 structure.
 *                      Added MPI_RAID_PHYS_DISK1_PATH_MAX define for RAID
 *                      Physcial Disk Page 1.
 *  01-15-07  01.05.17  Added additional bit defines for ExtFlags field of
 *                      Manufacturing Page 4.
 *                      Added Solid State Drives Supported bit to IOC Page 6
 *                      Capabilities Flags.
 *                      Added new value for AccessStatus field of SAS Device
 *                      Page 0 (_SATA_NEEDS_INITIALIZATION).
 *  03-28-08  01.05.18  Defined new bits in Manufacturing Page 4 ExtFlags field
 *                      to control coercion size and the mixing of SAS and SATA
 *                      SSD drives.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_CNFG_H
#define MPI_CNFG_H


/*****************************************************************************
*
*       C o n f i g    M e s s a g e    a n d    S t r u c t u r e s
*
*****************************************************************************/

typedef struct _CONFIG_PAGE_HEADER
{
    U8                      PageVersion;                /* 00h */
    U8                      PageLength;                 /* 01h */
    U8                      PageNumber;                 /* 02h */
    U8                      PageType;                   /* 03h */
} CONFIG_PAGE_HEADER, MPI_POINTER PTR_CONFIG_PAGE_HEADER,
  ConfigPageHeader_t, MPI_POINTER pConfigPageHeader_t;

typedef union _CONFIG_PAGE_HEADER_UNION
{
   ConfigPageHeader_t  Struct;
   U8                  Bytes[4];
   U16                 Word16[2];
   U32                 Word32;
} ConfigPageHeaderUnion, MPI_POINTER pConfigPageHeaderUnion,
  CONFIG_PAGE_HEADER_UNION, MPI_POINTER PTR_CONFIG_PAGE_HEADER_UNION;

typedef struct _CONFIG_EXTENDED_PAGE_HEADER
{
    U8                  PageVersion;                /* 00h */
    U8                  Reserved1;                  /* 01h */
    U8                  PageNumber;                 /* 02h */
    U8                  PageType;                   /* 03h */
    U16                 ExtPageLength;              /* 04h */
    U8                  ExtPageType;                /* 06h */
    U8                  Reserved2;                  /* 07h */
} CONFIG_EXTENDED_PAGE_HEADER, MPI_POINTER PTR_CONFIG_EXTENDED_PAGE_HEADER,
  ConfigExtendedPageHeader_t, MPI_POINTER pConfigExtendedPageHeader_t;



/****************************************************************************
*   PageType field values
****************************************************************************/
#define MPI_CONFIG_PAGEATTR_READ_ONLY               (0x00)
#define MPI_CONFIG_PAGEATTR_CHANGEABLE              (0x10)
#define MPI_CONFIG_PAGEATTR_PERSISTENT              (0x20)
#define MPI_CONFIG_PAGEATTR_RO_PERSISTENT           (0x30)
#define MPI_CONFIG_PAGEATTR_MASK                    (0xF0)

#define MPI_CONFIG_PAGETYPE_IO_UNIT                 (0x00)
#define MPI_CONFIG_PAGETYPE_IOC                     (0x01)
#define MPI_CONFIG_PAGETYPE_BIOS                    (0x02)
#define MPI_CONFIG_PAGETYPE_SCSI_PORT               (0x03)
#define MPI_CONFIG_PAGETYPE_SCSI_DEVICE             (0x04)
#define MPI_CONFIG_PAGETYPE_FC_PORT                 (0x05)
#define MPI_CONFIG_PAGETYPE_FC_DEVICE               (0x06)
#define MPI_CONFIG_PAGETYPE_LAN                     (0x07)
#define MPI_CONFIG_PAGETYPE_RAID_VOLUME             (0x08)
#define MPI_CONFIG_PAGETYPE_MANUFACTURING           (0x09)
#define MPI_CONFIG_PAGETYPE_RAID_PHYSDISK           (0x0A)
#define MPI_CONFIG_PAGETYPE_INBAND                  (0x0B)
#define MPI_CONFIG_PAGETYPE_EXTENDED                (0x0F)
#define MPI_CONFIG_PAGETYPE_MASK                    (0x0F)

#define MPI_CONFIG_TYPENUM_MASK                     (0x0FFF)


/****************************************************************************
*   ExtPageType field values
****************************************************************************/
#define MPI_CONFIG_EXTPAGETYPE_SAS_IO_UNIT          (0x10)
#define MPI_CONFIG_EXTPAGETYPE_SAS_EXPANDER         (0x11)
#define MPI_CONFIG_EXTPAGETYPE_SAS_DEVICE           (0x12)
#define MPI_CONFIG_EXTPAGETYPE_SAS_PHY              (0x13)
#define MPI_CONFIG_EXTPAGETYPE_LOG                  (0x14)
#define MPI_CONFIG_EXTPAGETYPE_ENCLOSURE            (0x15)


/****************************************************************************
*   PageAddress field values
****************************************************************************/
#define MPI_SCSI_PORT_PGAD_PORT_MASK                (0x000000FF)

#define MPI_SCSI_DEVICE_FORM_MASK                   (0xF0000000)
#define MPI_SCSI_DEVICE_FORM_BUS_TID                (0x00000000)
#define MPI_SCSI_DEVICE_TARGET_ID_MASK              (0x000000FF)
#define MPI_SCSI_DEVICE_TARGET_ID_SHIFT             (0)
#define MPI_SCSI_DEVICE_BUS_MASK                    (0x0000FF00)
#define MPI_SCSI_DEVICE_BUS_SHIFT                   (8)
#define MPI_SCSI_DEVICE_FORM_TARGET_MODE            (0x10000000)
#define MPI_SCSI_DEVICE_TM_RESPOND_ID_MASK          (0x000000FF)
#define MPI_SCSI_DEVICE_TM_RESPOND_ID_SHIFT         (0)
#define MPI_SCSI_DEVICE_TM_BUS_MASK                 (0x0000FF00)
#define MPI_SCSI_DEVICE_TM_BUS_SHIFT                (8)
#define MPI_SCSI_DEVICE_TM_INIT_ID_MASK             (0x00FF0000)
#define MPI_SCSI_DEVICE_TM_INIT_ID_SHIFT            (16)

#define MPI_FC_PORT_PGAD_PORT_MASK                  (0xF0000000)
#define MPI_FC_PORT_PGAD_PORT_SHIFT                 (28)
#define MPI_FC_PORT_PGAD_FORM_MASK                  (0x0F000000)
#define MPI_FC_PORT_PGAD_FORM_INDEX                 (0x01000000)
#define MPI_FC_PORT_PGAD_INDEX_MASK                 (0x0000FFFF)
#define MPI_FC_PORT_PGAD_INDEX_SHIFT                (0)

#define MPI_FC_DEVICE_PGAD_PORT_MASK                (0xF0000000)
#define MPI_FC_DEVICE_PGAD_PORT_SHIFT               (28)
#define MPI_FC_DEVICE_PGAD_FORM_MASK                (0x0F000000)
#define MPI_FC_DEVICE_PGAD_FORM_NEXT_DID            (0x00000000)
#define MPI_FC_DEVICE_PGAD_ND_PORT_MASK             (0xF0000000)
#define MPI_FC_DEVICE_PGAD_ND_PORT_SHIFT            (28)
#define MPI_FC_DEVICE_PGAD_ND_DID_MASK              (0x00FFFFFF)
#define MPI_FC_DEVICE_PGAD_ND_DID_SHIFT             (0)
#define MPI_FC_DEVICE_PGAD_FORM_BUS_TID             (0x01000000)
#define MPI_FC_DEVICE_PGAD_BT_BUS_MASK              (0x0000FF00)
#define MPI_FC_DEVICE_PGAD_BT_BUS_SHIFT             (8)
#define MPI_FC_DEVICE_PGAD_BT_TID_MASK              (0x000000FF)
#define MPI_FC_DEVICE_PGAD_BT_TID_SHIFT             (0)

#define MPI_PHYSDISK_PGAD_PHYSDISKNUM_MASK          (0x000000FF)
#define MPI_PHYSDISK_PGAD_PHYSDISKNUM_SHIFT         (0)

#define MPI_SAS_EXPAND_PGAD_FORM_MASK             (0xF0000000)
#define MPI_SAS_EXPAND_PGAD_FORM_SHIFT            (28)
#define MPI_SAS_EXPAND_PGAD_FORM_GET_NEXT_HANDLE  (0x00000000)
#define MPI_SAS_EXPAND_PGAD_FORM_HANDLE_PHY_NUM   (0x00000001)
#define MPI_SAS_EXPAND_PGAD_FORM_HANDLE           (0x00000002)
#define MPI_SAS_EXPAND_PGAD_GNH_MASK_HANDLE       (0x0000FFFF)
#define MPI_SAS_EXPAND_PGAD_GNH_SHIFT_HANDLE      (0)
#define MPI_SAS_EXPAND_PGAD_HPN_MASK_PHY          (0x00FF0000)
#define MPI_SAS_EXPAND_PGAD_HPN_SHIFT_PHY         (16)
#define MPI_SAS_EXPAND_PGAD_HPN_MASK_HANDLE       (0x0000FFFF)
#define MPI_SAS_EXPAND_PGAD_HPN_SHIFT_HANDLE      (0)
#define MPI_SAS_EXPAND_PGAD_H_MASK_HANDLE         (0x0000FFFF)
#define MPI_SAS_EXPAND_PGAD_H_SHIFT_HANDLE        (0)

#define MPI_SAS_DEVICE_PGAD_FORM_MASK               (0xF0000000)
#define MPI_SAS_DEVICE_PGAD_FORM_SHIFT              (28)
#define MPI_SAS_DEVICE_PGAD_FORM_GET_NEXT_HANDLE    (0x00000000)
#define MPI_SAS_DEVICE_PGAD_FORM_BUS_TARGET_ID      (0x00000001)
#define MPI_SAS_DEVICE_PGAD_FORM_HANDLE             (0x00000002)
#define MPI_SAS_DEVICE_PGAD_GNH_HANDLE_MASK         (0x0000FFFF)
#define MPI_SAS_DEVICE_PGAD_GNH_HANDLE_SHIFT        (0)
#define MPI_SAS_DEVICE_PGAD_BT_BUS_MASK             (0x0000FF00)
#define MPI_SAS_DEVICE_PGAD_BT_BUS_SHIFT            (8)
#define MPI_SAS_DEVICE_PGAD_BT_TID_MASK             (0x000000FF)
#define MPI_SAS_DEVICE_PGAD_BT_TID_SHIFT            (0)
#define MPI_SAS_DEVICE_PGAD_H_HANDLE_MASK           (0x0000FFFF)
#define MPI_SAS_DEVICE_PGAD_H_HANDLE_SHIFT          (0)

#define MPI_SAS_PHY_PGAD_FORM_MASK                  (0xF0000000)
#define MPI_SAS_PHY_PGAD_FORM_SHIFT                 (28)
#define MPI_SAS_PHY_PGAD_FORM_PHY_NUMBER            (0x0)
#define MPI_SAS_PHY_PGAD_FORM_PHY_TBL_INDEX         (0x1)
#define MPI_SAS_PHY_PGAD_PHY_NUMBER_MASK            (0x000000FF)
#define MPI_SAS_PHY_PGAD_PHY_NUMBER_SHIFT           (0)
#define MPI_SAS_PHY_PGAD_PHY_TBL_INDEX_MASK         (0x0000FFFF)
#define MPI_SAS_PHY_PGAD_PHY_TBL_INDEX_SHIFT        (0)

#define MPI_SAS_ENCLOS_PGAD_FORM_MASK               (0xF0000000)
#define MPI_SAS_ENCLOS_PGAD_FORM_SHIFT              (28)
#define MPI_SAS_ENCLOS_PGAD_FORM_GET_NEXT_HANDLE    (0x00000000)
#define MPI_SAS_ENCLOS_PGAD_FORM_HANDLE             (0x00000001)
#define MPI_SAS_ENCLOS_PGAD_GNH_HANDLE_MASK         (0x0000FFFF)
#define MPI_SAS_ENCLOS_PGAD_GNH_HANDLE_SHIFT        (0)
#define MPI_SAS_ENCLOS_PGAD_H_HANDLE_MASK           (0x0000FFFF)
#define MPI_SAS_ENCLOS_PGAD_H_HANDLE_SHIFT          (0)



/****************************************************************************
*   Config Request Message
****************************************************************************/
typedef struct _MSG_CONFIG
{
    U8                      Action;                     /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U16                     ExtPageLength;              /* 04h */
    U8                      ExtPageType;                /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U8                      Reserved2[8];               /* 0Ch */
    CONFIG_PAGE_HEADER      Header;                     /* 14h */
    U32                     PageAddress;                /* 18h */
    SGE_IO_UNION            PageBufferSGE;              /* 1Ch */
} MSG_CONFIG, MPI_POINTER PTR_MSG_CONFIG,
  Config_t, MPI_POINTER pConfig_t;


/****************************************************************************
*   Action field values
****************************************************************************/
#define MPI_CONFIG_ACTION_PAGE_HEADER               (0x00)
#define MPI_CONFIG_ACTION_PAGE_READ_CURRENT         (0x01)
#define MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT        (0x02)
#define MPI_CONFIG_ACTION_PAGE_DEFAULT              (0x03)
#define MPI_CONFIG_ACTION_PAGE_WRITE_NVRAM          (0x04)
#define MPI_CONFIG_ACTION_PAGE_READ_DEFAULT         (0x05)
#define MPI_CONFIG_ACTION_PAGE_READ_NVRAM           (0x06)


/* Config Reply Message */
typedef struct _MSG_CONFIG_REPLY
{
    U8                      Action;                     /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U16                     ExtPageLength;              /* 04h */
    U8                      ExtPageType;                /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U8                      Reserved2[2];               /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
    CONFIG_PAGE_HEADER      Header;                     /* 14h */
} MSG_CONFIG_REPLY, MPI_POINTER PTR_MSG_CONFIG_REPLY,
  ConfigReply_t, MPI_POINTER pConfigReply_t;



/*****************************************************************************
*
*               C o n f i g u r a t i o n    P a g e s
*
*****************************************************************************/

/****************************************************************************
*   Manufacturing Config pages
****************************************************************************/
#define MPI_MANUFACTPAGE_VENDORID_LSILOGIC          (0x1000)
/* Fibre Channel */
#define MPI_MANUFACTPAGE_DEVICEID_FC909             (0x0621)
#define MPI_MANUFACTPAGE_DEVICEID_FC919             (0x0624)
#define MPI_MANUFACTPAGE_DEVICEID_FC929             (0x0622)
#define MPI_MANUFACTPAGE_DEVICEID_FC919X            (0x0628)
#define MPI_MANUFACTPAGE_DEVICEID_FC929X            (0x0626)
#define MPI_MANUFACTPAGE_DEVICEID_FC939X            (0x0642)
#define MPI_MANUFACTPAGE_DEVICEID_FC949X            (0x0640)
#define MPI_MANUFACTPAGE_DEVICEID_FC949E            (0x0646)
/* SCSI */
#define MPI_MANUFACTPAGE_DEVID_53C1030              (0x0030)
#define MPI_MANUFACTPAGE_DEVID_53C1030ZC            (0x0031)
#define MPI_MANUFACTPAGE_DEVID_1030_53C1035         (0x0032)
#define MPI_MANUFACTPAGE_DEVID_1030ZC_53C1035       (0x0033)
#define MPI_MANUFACTPAGE_DEVID_53C1035              (0x0040)
#define MPI_MANUFACTPAGE_DEVID_53C1035ZC            (0x0041)
/* SAS */
#define MPI_MANUFACTPAGE_DEVID_SAS1064              (0x0050)
#define MPI_MANUFACTPAGE_DEVID_SAS1064A             (0x005C)
#define MPI_MANUFACTPAGE_DEVID_SAS1064E             (0x0056)
#define MPI_MANUFACTPAGE_DEVID_SAS1066              (0x005E)
#define MPI_MANUFACTPAGE_DEVID_SAS1066E             (0x005A)
#define MPI_MANUFACTPAGE_DEVID_SAS1068              (0x0054)
#define MPI_MANUFACTPAGE_DEVID_SAS1068E             (0x0058)
#define MPI_MANUFACTPAGE_DEVID_SAS1078              (0x0062)


typedef struct _CONFIG_PAGE_MANUFACTURING_0
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U8                      ChipName[16];               /* 04h */
    U8                      ChipRevision[8];            /* 14h */
    U8                      BoardName[16];              /* 1Ch */
    U8                      BoardAssembly[16];          /* 2Ch */
    U8                      BoardTracerNumber[16];      /* 3Ch */

} CONFIG_PAGE_MANUFACTURING_0, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_0,
  ManufacturingPage0_t, MPI_POINTER pManufacturingPage0_t;

#define MPI_MANUFACTURING0_PAGEVERSION                 (0x00)


typedef struct _CONFIG_PAGE_MANUFACTURING_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U8                      VPD[256];                   /* 04h */
} CONFIG_PAGE_MANUFACTURING_1, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_1,
  ManufacturingPage1_t, MPI_POINTER pManufacturingPage1_t;

#define MPI_MANUFACTURING1_PAGEVERSION                 (0x00)


typedef struct _MPI_CHIP_REVISION_ID
{
    U16 DeviceID;                                       /* 00h */
    U8  PCIRevisionID;                                  /* 02h */
    U8  Reserved;                                       /* 03h */
} MPI_CHIP_REVISION_ID, MPI_POINTER PTR_MPI_CHIP_REVISION_ID,
  MpiChipRevisionId_t, MPI_POINTER pMpiChipRevisionId_t;


/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_MAN_PAGE_2_HW_SETTINGS_WORDS
#define MPI_MAN_PAGE_2_HW_SETTINGS_WORDS    (1)
#endif

typedef struct _CONFIG_PAGE_MANUFACTURING_2
{
    CONFIG_PAGE_HEADER      Header;                                 /* 00h */
    MPI_CHIP_REVISION_ID    ChipId;                                 /* 04h */
    U32                     HwSettings[MPI_MAN_PAGE_2_HW_SETTINGS_WORDS];/* 08h */
} CONFIG_PAGE_MANUFACTURING_2, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_2,
  ManufacturingPage2_t, MPI_POINTER pManufacturingPage2_t;

#define MPI_MANUFACTURING2_PAGEVERSION                  (0x00)


/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_MAN_PAGE_3_INFO_WORDS
#define MPI_MAN_PAGE_3_INFO_WORDS           (1)
#endif

typedef struct _CONFIG_PAGE_MANUFACTURING_3
{
    CONFIG_PAGE_HEADER                  Header;                     /* 00h */
    MPI_CHIP_REVISION_ID                ChipId;                     /* 04h */
    U32                                 Info[MPI_MAN_PAGE_3_INFO_WORDS];/* 08h */
} CONFIG_PAGE_MANUFACTURING_3, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_3,
  ManufacturingPage3_t, MPI_POINTER pManufacturingPage3_t;

#define MPI_MANUFACTURING3_PAGEVERSION                  (0x00)


typedef struct _CONFIG_PAGE_MANUFACTURING_4
{
    CONFIG_PAGE_HEADER              Header;             /* 00h */
    U32                             Reserved1;          /* 04h */
    U8                              InfoOffset0;        /* 08h */
    U8                              InfoSize0;          /* 09h */
    U8                              InfoOffset1;        /* 0Ah */
    U8                              InfoSize1;          /* 0Bh */
    U8                              InquirySize;        /* 0Ch */
    U8                              Flags;              /* 0Dh */
    U16                             ExtFlags;           /* 0Eh */
    U8                              InquiryData[56];    /* 10h */
    U32                             ISVolumeSettings;   /* 48h */
    U32                             IMEVolumeSettings;  /* 4Ch */
    U32                             IMVolumeSettings;   /* 50h */
    U32                             Reserved3;          /* 54h */
    U32                             Reserved4;          /* 58h */
    U32                             Reserved5;          /* 5Ch */
    U8                              IMEDataScrubRate;   /* 60h */
    U8                              IMEResyncRate;      /* 61h */
    U16                             Reserved6;          /* 62h */
    U8                              IMDataScrubRate;    /* 64h */
    U8                              IMResyncRate;       /* 65h */
    U16                             Reserved7;          /* 66h */
    U32                             Reserved8;          /* 68h */
    U32                             Reserved9;          /* 6Ch */
} CONFIG_PAGE_MANUFACTURING_4, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_4,
  ManufacturingPage4_t, MPI_POINTER pManufacturingPage4_t;

#define MPI_MANUFACTURING4_PAGEVERSION                  (0x05)

/* defines for the Flags field */
#define MPI_MANPAGE4_FORCE_BAD_BLOCK_TABLE              (0x80)
#define MPI_MANPAGE4_FORCE_OFFLINE_FAILOVER             (0x40)
#define MPI_MANPAGE4_IME_DISABLE                        (0x20)
#define MPI_MANPAGE4_IM_DISABLE                         (0x10)
#define MPI_MANPAGE4_IS_DISABLE                         (0x08)
#define MPI_MANPAGE4_IR_MODEPAGE8_DISABLE               (0x04)
#define MPI_MANPAGE4_IM_RESYNC_CACHE_ENABLE             (0x02)
#define MPI_MANPAGE4_IR_NO_MIX_SAS_SATA                 (0x01)

/* defines for the ExtFlags field */
#define MPI_MANPAGE4_EXTFLAGS_MASK_COERCION_SIZE        (0x0180)
#define MPI_MANPAGE4_EXTFLAGS_SHIFT_COERCION_SIZE       (7)
#define MPI_MANPAGE4_EXTFLAGS_1GB_COERCION_SIZE         (0)
#define MPI_MANPAGE4_EXTFLAGS_128MB_COERCION_SIZE       (1)

#define MPI_MANPAGE4_EXTFLAGS_NO_MIX_SSD_SAS_SATA       (0x0040)
#define MPI_MANPAGE4_EXTFLAGS_MIX_SSD_AND_NON_SSD       (0x0020)
#define MPI_MANPAGE4_EXTFLAGS_DUAL_PORT_SUPPORT         (0x0010)
#define MPI_MANPAGE4_EXTFLAGS_HIDE_NON_IR_METADATA      (0x0008)
#define MPI_MANPAGE4_EXTFLAGS_SAS_CACHE_DISABLE         (0x0004)
#define MPI_MANPAGE4_EXTFLAGS_SATA_CACHE_DISABLE        (0x0002)
#define MPI_MANPAGE4_EXTFLAGS_LEGACY_MODE               (0x0001)


#ifndef MPI_MANPAGE5_NUM_FORCEWWID
#define MPI_MANPAGE5_NUM_FORCEWWID      (1)
#endif

typedef struct _CONFIG_PAGE_MANUFACTURING_5
{
    CONFIG_PAGE_HEADER              Header;             /* 00h */
    U64                             BaseWWID;           /* 04h */
    U8                              Flags;              /* 0Ch */
    U8                              NumForceWWID;       /* 0Dh */
    U16                             Reserved2;          /* 0Eh */
    U32                             Reserved3;          /* 10h */
    U32                             Reserved4;          /* 14h */
    U64                             ForceWWID[MPI_MANPAGE5_NUM_FORCEWWID]; /* 18h */
} CONFIG_PAGE_MANUFACTURING_5, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_5,
  ManufacturingPage5_t, MPI_POINTER pManufacturingPage5_t;

#define MPI_MANUFACTURING5_PAGEVERSION                  (0x02)

/* defines for the Flags field */
#define MPI_MANPAGE5_TWO_WWID_PER_PHY                   (0x01)


typedef struct _CONFIG_PAGE_MANUFACTURING_6
{
    CONFIG_PAGE_HEADER              Header;             /* 00h */
    U32                             ProductSpecificInfo;/* 04h */
} CONFIG_PAGE_MANUFACTURING_6, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_6,
  ManufacturingPage6_t, MPI_POINTER pManufacturingPage6_t;

#define MPI_MANUFACTURING6_PAGEVERSION                  (0x00)


typedef struct _MPI_MANPAGE7_CONNECTOR_INFO
{
    U32                         Pinout;                 /* 00h */
    U8                          Connector[16];          /* 04h */
    U8                          Location;               /* 14h */
    U8                          Reserved1;              /* 15h */
    U16                         Slot;                   /* 16h */
    U32                         Reserved2;              /* 18h */
} MPI_MANPAGE7_CONNECTOR_INFO, MPI_POINTER PTR_MPI_MANPAGE7_CONNECTOR_INFO,
  MpiManPage7ConnectorInfo_t, MPI_POINTER pMpiManPage7ConnectorInfo_t;

/* defines for the Pinout field */
#define MPI_MANPAGE7_PINOUT_SFF_8484_L4                 (0x00080000)
#define MPI_MANPAGE7_PINOUT_SFF_8484_L3                 (0x00040000)
#define MPI_MANPAGE7_PINOUT_SFF_8484_L2                 (0x00020000)
#define MPI_MANPAGE7_PINOUT_SFF_8484_L1                 (0x00010000)
#define MPI_MANPAGE7_PINOUT_SFF_8470_L4                 (0x00000800)
#define MPI_MANPAGE7_PINOUT_SFF_8470_L3                 (0x00000400)
#define MPI_MANPAGE7_PINOUT_SFF_8470_L2                 (0x00000200)
#define MPI_MANPAGE7_PINOUT_SFF_8470_L1                 (0x00000100)
#define MPI_MANPAGE7_PINOUT_SFF_8482                    (0x00000002)
#define MPI_MANPAGE7_PINOUT_CONNECTION_UNKNOWN          (0x00000001)

/* defines for the Location field */
#define MPI_MANPAGE7_LOCATION_UNKNOWN                   (0x01)
#define MPI_MANPAGE7_LOCATION_INTERNAL                  (0x02)
#define MPI_MANPAGE7_LOCATION_EXTERNAL                  (0x04)
#define MPI_MANPAGE7_LOCATION_SWITCHABLE                (0x08)
#define MPI_MANPAGE7_LOCATION_AUTO                      (0x10)
#define MPI_MANPAGE7_LOCATION_NOT_PRESENT               (0x20)
#define MPI_MANPAGE7_LOCATION_NOT_CONNECTED             (0x80)

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check NumPhys at runtime.
 */
#ifndef MPI_MANPAGE7_CONNECTOR_INFO_MAX
#define MPI_MANPAGE7_CONNECTOR_INFO_MAX   (1)
#endif

typedef struct _CONFIG_PAGE_MANUFACTURING_7
{
    CONFIG_PAGE_HEADER          Header;                 /* 00h */
    U32                         Reserved1;              /* 04h */
    U32                         Reserved2;              /* 08h */
    U32                         Flags;                  /* 0Ch */
    U8                          EnclosureName[16];      /* 10h */
    U8                          NumPhys;                /* 20h */
    U8                          Reserved3;              /* 21h */
    U16                         Reserved4;              /* 22h */
    MPI_MANPAGE7_CONNECTOR_INFO ConnectorInfo[MPI_MANPAGE7_CONNECTOR_INFO_MAX]; /* 24h */
} CONFIG_PAGE_MANUFACTURING_7, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_7,
  ManufacturingPage7_t, MPI_POINTER pManufacturingPage7_t;

#define MPI_MANUFACTURING7_PAGEVERSION                  (0x00)

/* defines for the Flags field */
#define MPI_MANPAGE7_FLAG_USE_SLOT_INFO                 (0x00000001)


typedef struct _CONFIG_PAGE_MANUFACTURING_8
{
    CONFIG_PAGE_HEADER              Header;             /* 00h */
    U32                             ProductSpecificInfo;/* 04h */
} CONFIG_PAGE_MANUFACTURING_8, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_8,
  ManufacturingPage8_t, MPI_POINTER pManufacturingPage8_t;

#define MPI_MANUFACTURING8_PAGEVERSION                  (0x00)


typedef struct _CONFIG_PAGE_MANUFACTURING_9
{
    CONFIG_PAGE_HEADER              Header;             /* 00h */
    U32                             ProductSpecificInfo;/* 04h */
} CONFIG_PAGE_MANUFACTURING_9, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_9,
  ManufacturingPage9_t, MPI_POINTER pManufacturingPage9_t;

#define MPI_MANUFACTURING9_PAGEVERSION                  (0x00)


typedef struct _CONFIG_PAGE_MANUFACTURING_10
{
    CONFIG_PAGE_HEADER              Header;             /* 00h */
    U32                             ProductSpecificInfo;/* 04h */
} CONFIG_PAGE_MANUFACTURING_10, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_10,
  ManufacturingPage10_t, MPI_POINTER pManufacturingPage10_t;

#define MPI_MANUFACTURING10_PAGEVERSION                 (0x00)


/****************************************************************************
*   IO Unit Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_IO_UNIT_0
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U64                     UniqueValue;                /* 04h */
} CONFIG_PAGE_IO_UNIT_0, MPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_0,
  IOUnitPage0_t, MPI_POINTER pIOUnitPage0_t;

#define MPI_IOUNITPAGE0_PAGEVERSION                     (0x00)


typedef struct _CONFIG_PAGE_IO_UNIT_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
} CONFIG_PAGE_IO_UNIT_1, MPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_1,
  IOUnitPage1_t, MPI_POINTER pIOUnitPage1_t;

#define MPI_IOUNITPAGE1_PAGEVERSION                     (0x02)

/* IO Unit Page 1 Flags defines */
#define MPI_IOUNITPAGE1_MULTI_FUNCTION                  (0x00000000)
#define MPI_IOUNITPAGE1_SINGLE_FUNCTION                 (0x00000001)
#define MPI_IOUNITPAGE1_MULTI_PATHING                   (0x00000002)
#define MPI_IOUNITPAGE1_SINGLE_PATHING                  (0x00000000)
#define MPI_IOUNITPAGE1_IR_USE_STATIC_VOLUME_ID         (0x00000004)
#define MPI_IOUNITPAGE1_DISABLE_QUEUE_FULL_HANDLING     (0x00000020)
#define MPI_IOUNITPAGE1_DISABLE_IR                      (0x00000040)
#define MPI_IOUNITPAGE1_FORCE_32                        (0x00000080)
#define MPI_IOUNITPAGE1_NATIVE_COMMAND_Q_DISABLE        (0x00000100)
#define MPI_IOUNITPAGE1_SATA_WRITE_CACHE_DISABLE        (0x00000200)

typedef struct _MPI_ADAPTER_INFO
{
    U8      PciBusNumber;                               /* 00h */
    U8      PciDeviceAndFunctionNumber;                 /* 01h */
    U16     AdapterFlags;                               /* 02h */
} MPI_ADAPTER_INFO, MPI_POINTER PTR_MPI_ADAPTER_INFO,
  MpiAdapterInfo_t, MPI_POINTER pMpiAdapterInfo_t;

#define MPI_ADAPTER_INFO_FLAGS_EMBEDDED                 (0x0001)
#define MPI_ADAPTER_INFO_FLAGS_INIT_STATUS              (0x0002)

typedef struct _CONFIG_PAGE_IO_UNIT_2
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
    U32                     BiosVersion;                /* 08h */
    MPI_ADAPTER_INFO        AdapterOrder[4];            /* 0Ch */
    U32                     Reserved1;                  /* 1Ch */
} CONFIG_PAGE_IO_UNIT_2, MPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_2,
  IOUnitPage2_t, MPI_POINTER pIOUnitPage2_t;

#define MPI_IOUNITPAGE2_PAGEVERSION                     (0x02)

#define MPI_IOUNITPAGE2_FLAGS_PAUSE_ON_ERROR            (0x00000002)
#define MPI_IOUNITPAGE2_FLAGS_VERBOSE_ENABLE            (0x00000004)
#define MPI_IOUNITPAGE2_FLAGS_COLOR_VIDEO_DISABLE       (0x00000008)
#define MPI_IOUNITPAGE2_FLAGS_DONT_HOOK_INT_40          (0x00000010)

#define MPI_IOUNITPAGE2_FLAGS_DEV_LIST_DISPLAY_MASK     (0x000000E0)
#define MPI_IOUNITPAGE2_FLAGS_INSTALLED_DEV_DISPLAY     (0x00000000)
#define MPI_IOUNITPAGE2_FLAGS_ADAPTER_DISPLAY           (0x00000020)
#define MPI_IOUNITPAGE2_FLAGS_ADAPTER_DEV_DISPLAY       (0x00000040)


/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IO_UNIT_PAGE_3_GPIO_VAL_MAX
#define MPI_IO_UNIT_PAGE_3_GPIO_VAL_MAX     (1)
#endif

typedef struct _CONFIG_PAGE_IO_UNIT_3
{
    CONFIG_PAGE_HEADER      Header;                                   /* 00h */
    U8                      GPIOCount;                                /* 04h */
    U8                      Reserved1;                                /* 05h */
    U16                     Reserved2;                                /* 06h */
    U16                     GPIOVal[MPI_IO_UNIT_PAGE_3_GPIO_VAL_MAX]; /* 08h */
} CONFIG_PAGE_IO_UNIT_3, MPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_3,
  IOUnitPage3_t, MPI_POINTER pIOUnitPage3_t;

#define MPI_IOUNITPAGE3_PAGEVERSION                     (0x01)

#define MPI_IOUNITPAGE3_GPIO_FUNCTION_MASK              (0xFC)
#define MPI_IOUNITPAGE3_GPIO_FUNCTION_SHIFT             (2)
#define MPI_IOUNITPAGE3_GPIO_SETTING_OFF                (0x00)
#define MPI_IOUNITPAGE3_GPIO_SETTING_ON                 (0x01)


typedef struct _CONFIG_PAGE_IO_UNIT_4
{
    CONFIG_PAGE_HEADER      Header;                                   /* 00h */
    U32                     Reserved1;                                /* 04h */
    SGE_SIMPLE_UNION        FWImageSGE;                               /* 08h */
} CONFIG_PAGE_IO_UNIT_4, MPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_4,
  IOUnitPage4_t, MPI_POINTER pIOUnitPage4_t;

#define MPI_IOUNITPAGE4_PAGEVERSION                     (0x00)


/****************************************************************************
*   IOC Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_IOC_0
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     TotalNVStore;               /* 04h */
    U32                     FreeNVStore;                /* 08h */
    U16                     VendorID;                   /* 0Ch */
    U16                     DeviceID;                   /* 0Eh */
    U8                      RevisionID;                 /* 10h */
    U8                      Reserved[3];                /* 11h */
    U32                     ClassCode;                  /* 14h */
    U16                     SubsystemVendorID;          /* 18h */
    U16                     SubsystemID;                /* 1Ah */
} CONFIG_PAGE_IOC_0, MPI_POINTER PTR_CONFIG_PAGE_IOC_0,
  IOCPage0_t, MPI_POINTER pIOCPage0_t;

#define MPI_IOCPAGE0_PAGEVERSION                        (0x01)


typedef struct _CONFIG_PAGE_IOC_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
    U32                     CoalescingTimeout;          /* 08h */
    U8                      CoalescingDepth;            /* 0Ch */
    U8                      PCISlotNum;                 /* 0Dh */
    U8                      Reserved[2];                /* 0Eh */
} CONFIG_PAGE_IOC_1, MPI_POINTER PTR_CONFIG_PAGE_IOC_1,
  IOCPage1_t, MPI_POINTER pIOCPage1_t;

#define MPI_IOCPAGE1_PAGEVERSION                        (0x03)

/* defines for the Flags field */
#define MPI_IOCPAGE1_EEDP_MODE_MASK                     (0x07000000)
#define MPI_IOCPAGE1_EEDP_MODE_OFF                      (0x00000000)
#define MPI_IOCPAGE1_EEDP_MODE_T10                      (0x01000000)
#define MPI_IOCPAGE1_EEDP_MODE_LSI_1                    (0x02000000)
#define MPI_IOCPAGE1_INITIATOR_CONTEXT_REPLY_DISABLE    (0x00000010)
#define MPI_IOCPAGE1_REPLY_COALESCING                   (0x00000001)

#define MPI_IOCPAGE1_PCISLOTNUM_UNKNOWN                 (0xFF)


typedef struct _CONFIG_PAGE_IOC_2_RAID_VOL
{
    U8                          VolumeID;               /* 00h */
    U8                          VolumeBus;              /* 01h */
    U8                          VolumeIOC;              /* 02h */
    U8                          VolumePageNumber;       /* 03h */
    U8                          VolumeType;             /* 04h */
    U8                          Flags;                  /* 05h */
    U16                         Reserved3;              /* 06h */
} CONFIG_PAGE_IOC_2_RAID_VOL, MPI_POINTER PTR_CONFIG_PAGE_IOC_2_RAID_VOL,
  ConfigPageIoc2RaidVol_t, MPI_POINTER pConfigPageIoc2RaidVol_t;

/* IOC Page 2 Volume RAID Type values, also used in RAID Volume pages */

#define MPI_RAID_VOL_TYPE_IS                        (0x00)
#define MPI_RAID_VOL_TYPE_IME                       (0x01)
#define MPI_RAID_VOL_TYPE_IM                        (0x02)
#define MPI_RAID_VOL_TYPE_RAID_5                    (0x03)
#define MPI_RAID_VOL_TYPE_RAID_6                    (0x04)
#define MPI_RAID_VOL_TYPE_RAID_10                   (0x05)
#define MPI_RAID_VOL_TYPE_RAID_50                   (0x06)
#define MPI_RAID_VOL_TYPE_UNKNOWN                   (0xFF)

/* IOC Page 2 Volume Flags values */

#define MPI_IOCPAGE2_FLAG_VOLUME_INACTIVE           (0x08)

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IOC_PAGE_2_RAID_VOLUME_MAX
#define MPI_IOC_PAGE_2_RAID_VOLUME_MAX      (1)
#endif

typedef struct _CONFIG_PAGE_IOC_2
{
    CONFIG_PAGE_HEADER          Header;                              /* 00h */
    U32                         CapabilitiesFlags;                   /* 04h */
    U8                          NumActiveVolumes;                    /* 08h */
    U8                          MaxVolumes;                          /* 09h */
    U8                          NumActivePhysDisks;                  /* 0Ah */
    U8                          MaxPhysDisks;                        /* 0Bh */
    CONFIG_PAGE_IOC_2_RAID_VOL  RaidVolume[MPI_IOC_PAGE_2_RAID_VOLUME_MAX];/* 0Ch */
} CONFIG_PAGE_IOC_2, MPI_POINTER PTR_CONFIG_PAGE_IOC_2,
  IOCPage2_t, MPI_POINTER pIOCPage2_t;

#define MPI_IOCPAGE2_PAGEVERSION                        (0x04)

/* IOC Page 2 Capabilities flags */

#define MPI_IOCPAGE2_CAP_FLAGS_IS_SUPPORT               (0x00000001)
#define MPI_IOCPAGE2_CAP_FLAGS_IME_SUPPORT              (0x00000002)
#define MPI_IOCPAGE2_CAP_FLAGS_IM_SUPPORT               (0x00000004)
#define MPI_IOCPAGE2_CAP_FLAGS_RAID_5_SUPPORT           (0x00000008)
#define MPI_IOCPAGE2_CAP_FLAGS_RAID_6_SUPPORT           (0x00000010)
#define MPI_IOCPAGE2_CAP_FLAGS_RAID_10_SUPPORT          (0x00000020)
#define MPI_IOCPAGE2_CAP_FLAGS_RAID_50_SUPPORT          (0x00000040)
#define MPI_IOCPAGE2_CAP_FLAGS_RAID_64_BIT_ADDRESSING   (0x10000000)
#define MPI_IOCPAGE2_CAP_FLAGS_SES_SUPPORT              (0x20000000)
#define MPI_IOCPAGE2_CAP_FLAGS_SAFTE_SUPPORT            (0x40000000)
#define MPI_IOCPAGE2_CAP_FLAGS_CROSS_CHANNEL_SUPPORT    (0x80000000)


typedef struct _IOC_3_PHYS_DISK
{
    U8                          PhysDiskID;             /* 00h */
    U8                          PhysDiskBus;            /* 01h */
    U8                          PhysDiskIOC;            /* 02h */
    U8                          PhysDiskNum;            /* 03h */
} IOC_3_PHYS_DISK, MPI_POINTER PTR_IOC_3_PHYS_DISK,
  Ioc3PhysDisk_t, MPI_POINTER pIoc3PhysDisk_t;

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IOC_PAGE_3_PHYSDISK_MAX
#define MPI_IOC_PAGE_3_PHYSDISK_MAX         (1)
#endif

typedef struct _CONFIG_PAGE_IOC_3
{
    CONFIG_PAGE_HEADER          Header;                                /* 00h */
    U8                          NumPhysDisks;                          /* 04h */
    U8                          Reserved1;                             /* 05h */
    U16                         Reserved2;                             /* 06h */
    IOC_3_PHYS_DISK             PhysDisk[MPI_IOC_PAGE_3_PHYSDISK_MAX]; /* 08h */
} CONFIG_PAGE_IOC_3, MPI_POINTER PTR_CONFIG_PAGE_IOC_3,
  IOCPage3_t, MPI_POINTER pIOCPage3_t;

#define MPI_IOCPAGE3_PAGEVERSION                        (0x00)


typedef struct _IOC_4_SEP
{
    U8                          SEPTargetID;            /* 00h */
    U8                          SEPBus;                 /* 01h */
    U16                         Reserved;               /* 02h */
} IOC_4_SEP, MPI_POINTER PTR_IOC_4_SEP,
  Ioc4Sep_t, MPI_POINTER pIoc4Sep_t;

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IOC_PAGE_4_SEP_MAX
#define MPI_IOC_PAGE_4_SEP_MAX              (1)
#endif

typedef struct _CONFIG_PAGE_IOC_4
{
    CONFIG_PAGE_HEADER          Header;                         /* 00h */
    U8                          ActiveSEP;                      /* 04h */
    U8                          MaxSEP;                         /* 05h */
    U16                         Reserved1;                      /* 06h */
    IOC_4_SEP                   SEP[MPI_IOC_PAGE_4_SEP_MAX];    /* 08h */
} CONFIG_PAGE_IOC_4, MPI_POINTER PTR_CONFIG_PAGE_IOC_4,
  IOCPage4_t, MPI_POINTER pIOCPage4_t;

#define MPI_IOCPAGE4_PAGEVERSION                        (0x00)


typedef struct _IOC_5_HOT_SPARE
{
    U8                          PhysDiskNum;            /* 00h */
    U8                          Reserved;               /* 01h */
    U8                          HotSparePool;           /* 02h */
    U8                          Flags;                   /* 03h */
} IOC_5_HOT_SPARE, MPI_POINTER PTR_IOC_5_HOT_SPARE,
  Ioc5HotSpare_t, MPI_POINTER pIoc5HotSpare_t;

/* IOC Page 5 HotSpare Flags */
#define MPI_IOC_PAGE_5_HOT_SPARE_ACTIVE                 (0x01)

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_IOC_PAGE_5_HOT_SPARE_MAX
#define MPI_IOC_PAGE_5_HOT_SPARE_MAX        (1)
#endif

typedef struct _CONFIG_PAGE_IOC_5
{
    CONFIG_PAGE_HEADER          Header;                         /* 00h */
    U32                         Reserved1;                      /* 04h */
    U8                          NumHotSpares;                   /* 08h */
    U8                          Reserved2;                      /* 09h */
    U16                         Reserved3;                      /* 0Ah */
    IOC_5_HOT_SPARE             HotSpare[MPI_IOC_PAGE_5_HOT_SPARE_MAX]; /* 0Ch */
} CONFIG_PAGE_IOC_5, MPI_POINTER PTR_CONFIG_PAGE_IOC_5,
  IOCPage5_t, MPI_POINTER pIOCPage5_t;

#define MPI_IOCPAGE5_PAGEVERSION                        (0x00)

typedef struct _CONFIG_PAGE_IOC_6
{
    CONFIG_PAGE_HEADER          Header;                         /* 00h */
    U32                         CapabilitiesFlags;              /* 04h */
    U8                          MaxDrivesIS;                    /* 08h */
    U8                          MaxDrivesIM;                    /* 09h */
    U8                          MaxDrivesIME;                   /* 0Ah */
    U8                          Reserved1;                      /* 0Bh */
    U8                          MinDrivesIS;                    /* 0Ch */
    U8                          MinDrivesIM;                    /* 0Dh */
    U8                          MinDrivesIME;                   /* 0Eh */
    U8                          Reserved2;                      /* 0Fh */
    U8                          MaxGlobalHotSpares;             /* 10h */
    U8                          Reserved3;                      /* 11h */
    U16                         Reserved4;                      /* 12h */
    U32                         Reserved5;                      /* 14h */
    U32                         SupportedStripeSizeMapIS;       /* 18h */
    U32                         SupportedStripeSizeMapIME;      /* 1Ch */
    U32                         Reserved6;                      /* 20h */
    U8                          MetadataSize;                   /* 24h */
    U8                          Reserved7;                      /* 25h */
    U16                         Reserved8;                      /* 26h */
    U16                         MaxBadBlockTableEntries;        /* 28h */
    U16                         Reserved9;                      /* 2Ah */
    U16                         IRNvsramUsage;                  /* 2Ch */
    U16                         Reserved10;                     /* 2Eh */
    U32                         IRNvsramVersion;                /* 30h */
    U32                         Reserved11;                     /* 34h */
    U32                         Reserved12;                     /* 38h */
} CONFIG_PAGE_IOC_6, MPI_POINTER PTR_CONFIG_PAGE_IOC_6,
  IOCPage6_t, MPI_POINTER pIOCPage6_t;

#define MPI_IOCPAGE6_PAGEVERSION                        (0x01)

/* IOC Page 6 Capabilities Flags */

#define MPI_IOCPAGE6_CAP_FLAGS_SSD_SUPPORT              (0x00000020)
#define MPI_IOCPAGE6_CAP_FLAGS_MULTIPORT_DRIVE_SUPPORT  (0x00000010)
#define MPI_IOCPAGE6_CAP_FLAGS_DISABLE_SMART_POLLING    (0x00000008)

#define MPI_IOCPAGE6_CAP_FLAGS_MASK_METADATA_SIZE       (0x00000006)
#define MPI_IOCPAGE6_CAP_FLAGS_64MB_METADATA_SIZE       (0x00000000)
#define MPI_IOCPAGE6_CAP_FLAGS_512MB_METADATA_SIZE      (0x00000002)

#define MPI_IOCPAGE6_CAP_FLAGS_GLOBAL_HOT_SPARE         (0x00000001)


/****************************************************************************
*   BIOS Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_BIOS_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     BiosOptions;                /* 04h */
    U32                     IOCSettings;                /* 08h */
    U32                     Reserved1;                  /* 0Ch */
    U32                     DeviceSettings;             /* 10h */
    U16                     NumberOfDevices;            /* 14h */
    U8                      ExpanderSpinup;             /* 16h */
    U8                      Reserved2;                  /* 17h */
    U16                     IOTimeoutBlockDevicesNonRM; /* 18h */
    U16                     IOTimeoutSequential;        /* 1Ah */
    U16                     IOTimeoutOther;             /* 1Ch */
    U16                     IOTimeoutBlockDevicesRM;    /* 1Eh */
} CONFIG_PAGE_BIOS_1, MPI_POINTER PTR_CONFIG_PAGE_BIOS_1,
  BIOSPage1_t, MPI_POINTER pBIOSPage1_t;

#define MPI_BIOSPAGE1_PAGEVERSION                       (0x03)

/* values for the BiosOptions field */
#define MPI_BIOSPAGE1_OPTIONS_SPI_ENABLE                (0x00000400)
#define MPI_BIOSPAGE1_OPTIONS_FC_ENABLE                 (0x00000200)
#define MPI_BIOSPAGE1_OPTIONS_SAS_ENABLE                (0x00000100)
#define MPI_BIOSPAGE1_OPTIONS_DISABLE_BIOS              (0x00000001)

/* values for the IOCSettings field */
#define MPI_BIOSPAGE1_IOCSET_MASK_INITIAL_SPINUP_DELAY  (0x0F000000)
#define MPI_BIOSPAGE1_IOCSET_SHIFT_INITIAL_SPINUP_DELAY (24)

#define MPI_BIOSPAGE1_IOCSET_MASK_PORT_ENABLE_DELAY     (0x00F00000)
#define MPI_BIOSPAGE1_IOCSET_SHIFT_PORT_ENABLE_DELAY    (20)

#define MPI_BIOSPAGE1_IOCSET_AUTO_PORT_ENABLE           (0x00080000)
#define MPI_BIOSPAGE1_IOCSET_DIRECT_ATTACH_SPINUP_MODE  (0x00040000)

#define MPI_BIOSPAGE1_IOCSET_MASK_BOOT_PREFERENCE       (0x00030000)
#define MPI_BIOSPAGE1_IOCSET_ENCLOSURE_SLOT_BOOT        (0x00000000)
#define MPI_BIOSPAGE1_IOCSET_SAS_ADDRESS_BOOT           (0x00010000)

#define MPI_BIOSPAGE1_IOCSET_MASK_MAX_TARGET_SPIN_UP    (0x0000F000)
#define MPI_BIOSPAGE1_IOCSET_SHIFT_MAX_TARGET_SPIN_UP   (12)

#define MPI_BIOSPAGE1_IOCSET_MASK_SPINUP_DELAY          (0x00000F00)
#define MPI_BIOSPAGE1_IOCSET_SHIFT_SPINUP_DELAY         (8)

#define MPI_BIOSPAGE1_IOCSET_MASK_RM_SETTING            (0x000000C0)
#define MPI_BIOSPAGE1_IOCSET_NONE_RM_SETTING            (0x00000000)
#define MPI_BIOSPAGE1_IOCSET_BOOT_RM_SETTING            (0x00000040)
#define MPI_BIOSPAGE1_IOCSET_MEDIA_RM_SETTING           (0x00000080)

#define MPI_BIOSPAGE1_IOCSET_MASK_ADAPTER_SUPPORT       (0x00000030)
#define MPI_BIOSPAGE1_IOCSET_NO_SUPPORT                 (0x00000000)
#define MPI_BIOSPAGE1_IOCSET_BIOS_SUPPORT               (0x00000010)
#define MPI_BIOSPAGE1_IOCSET_OS_SUPPORT                 (0x00000020)
#define MPI_BIOSPAGE1_IOCSET_ALL_SUPPORT                (0x00000030)

#define MPI_BIOSPAGE1_IOCSET_ALTERNATE_CHS              (0x00000008)

/* values for the DeviceSettings field */
#define MPI_BIOSPAGE1_DEVSET_DISABLE_SMART_POLLING      (0x00000010)
#define MPI_BIOSPAGE1_DEVSET_DISABLE_SEQ_LUN            (0x00000008)
#define MPI_BIOSPAGE1_DEVSET_DISABLE_RM_LUN             (0x00000004)
#define MPI_BIOSPAGE1_DEVSET_DISABLE_NON_RM_LUN         (0x00000002)
#define MPI_BIOSPAGE1_DEVSET_DISABLE_OTHER_LUN          (0x00000001)

/* defines for the ExpanderSpinup field */
#define MPI_BIOSPAGE1_EXPSPINUP_MASK_MAX_TARGET         (0xF0)
#define MPI_BIOSPAGE1_EXPSPINUP_SHIFT_MAX_TARGET        (4)
#define MPI_BIOSPAGE1_EXPSPINUP_MASK_DELAY              (0x0F)

typedef struct _MPI_BOOT_DEVICE_ADAPTER_ORDER
{
    U32         Reserved1;                              /* 00h */
    U32         Reserved2;                              /* 04h */
    U32         Reserved3;                              /* 08h */
    U32         Reserved4;                              /* 0Ch */
    U32         Reserved5;                              /* 10h */
    U32         Reserved6;                              /* 14h */
    U32         Reserved7;                              /* 18h */
    U32         Reserved8;                              /* 1Ch */
    U32         Reserved9;                              /* 20h */
    U32         Reserved10;                             /* 24h */
    U32         Reserved11;                             /* 28h */
    U32         Reserved12;                             /* 2Ch */
    U32         Reserved13;                             /* 30h */
    U32         Reserved14;                             /* 34h */
    U32         Reserved15;                             /* 38h */
    U32         Reserved16;                             /* 3Ch */
    U32         Reserved17;                             /* 40h */
} MPI_BOOT_DEVICE_ADAPTER_ORDER, MPI_POINTER PTR_MPI_BOOT_DEVICE_ADAPTER_ORDER;

typedef struct _MPI_BOOT_DEVICE_ADAPTER_NUMBER
{
    U8          TargetID;                               /* 00h */
    U8          Bus;                                    /* 01h */
    U8          AdapterNumber;                          /* 02h */
    U8          Reserved1;                              /* 03h */
    U32         Reserved2;                              /* 04h */
    U32         Reserved3;                              /* 08h */
    U32         Reserved4;                              /* 0Ch */
    U8          LUN[8];                                 /* 10h */
    U32         Reserved5;                              /* 18h */
    U32         Reserved6;                              /* 1Ch */
    U32         Reserved7;                              /* 20h */
    U32         Reserved8;                              /* 24h */
    U32         Reserved9;                              /* 28h */
    U32         Reserved10;                             /* 2Ch */
    U32         Reserved11;                             /* 30h */
    U32         Reserved12;                             /* 34h */
    U32         Reserved13;                             /* 38h */
    U32         Reserved14;                             /* 3Ch */
    U32         Reserved15;                             /* 40h */
} MPI_BOOT_DEVICE_ADAPTER_NUMBER, MPI_POINTER PTR_MPI_BOOT_DEVICE_ADAPTER_NUMBER;

typedef struct _MPI_BOOT_DEVICE_PCI_ADDRESS
{
    U8          TargetID;                               /* 00h */
    U8          Bus;                                    /* 01h */
    U16         PCIAddress;                             /* 02h */
    U32         Reserved1;                              /* 04h */
    U32         Reserved2;                              /* 08h */
    U32         Reserved3;                              /* 0Ch */
    U8          LUN[8];                                 /* 10h */
    U32         Reserved4;                              /* 18h */
    U32         Reserved5;                              /* 1Ch */
    U32         Reserved6;                              /* 20h */
    U32         Reserved7;                              /* 24h */
    U32         Reserved8;                              /* 28h */
    U32         Reserved9;                              /* 2Ch */
    U32         Reserved10;                             /* 30h */
    U32         Reserved11;                             /* 34h */
    U32         Reserved12;                             /* 38h */
    U32         Reserved13;                             /* 3Ch */
    U32         Reserved14;                             /* 40h */
} MPI_BOOT_DEVICE_PCI_ADDRESS, MPI_POINTER PTR_MPI_BOOT_DEVICE_PCI_ADDRESS;

typedef struct _MPI_BOOT_DEVICE_SLOT_NUMBER
{
    U8          TargetID;                               /* 00h */
    U8          Bus;                                    /* 01h */
    U8          PCISlotNumber;                          /* 02h */
    U8          Reserved1;                              /* 03h */
    U32         Reserved2;                              /* 04h */
    U32         Reserved3;                              /* 08h */
    U32         Reserved4;                              /* 0Ch */
    U8          LUN[8];                                 /* 10h */
    U32         Reserved5;                              /* 18h */
    U32         Reserved6;                              /* 1Ch */
    U32         Reserved7;                              /* 20h */
    U32         Reserved8;                              /* 24h */
    U32         Reserved9;                              /* 28h */
    U32         Reserved10;                             /* 2Ch */
    U32         Reserved11;                             /* 30h */
    U32         Reserved12;                             /* 34h */
    U32         Reserved13;                             /* 38h */
    U32         Reserved14;                             /* 3Ch */
    U32         Reserved15;                             /* 40h */
} MPI_BOOT_DEVICE_PCI_SLOT_NUMBER, MPI_POINTER PTR_MPI_BOOT_DEVICE_PCI_SLOT_NUMBER;

typedef struct _MPI_BOOT_DEVICE_FC_WWN
{
    U64         WWPN;                                   /* 00h */
    U32         Reserved1;                              /* 08h */
    U32         Reserved2;                              /* 0Ch */
    U8          LUN[8];                                 /* 10h */
    U32         Reserved3;                              /* 18h */
    U32         Reserved4;                              /* 1Ch */
    U32         Reserved5;                              /* 20h */
    U32         Reserved6;                              /* 24h */
    U32         Reserved7;                              /* 28h */
    U32         Reserved8;                              /* 2Ch */
    U32         Reserved9;                              /* 30h */
    U32         Reserved10;                             /* 34h */
    U32         Reserved11;                             /* 38h */
    U32         Reserved12;                             /* 3Ch */
    U32         Reserved13;                             /* 40h */
} MPI_BOOT_DEVICE_FC_WWN, MPI_POINTER PTR_MPI_BOOT_DEVICE_FC_WWN;

typedef struct _MPI_BOOT_DEVICE_SAS_WWN
{
    U64         SASAddress;                             /* 00h */
    U32         Reserved1;                              /* 08h */
    U32         Reserved2;                              /* 0Ch */
    U8          LUN[8];                                 /* 10h */
    U32         Reserved3;                              /* 18h */
    U32         Reserved4;                              /* 1Ch */
    U32         Reserved5;                              /* 20h */
    U32         Reserved6;                              /* 24h */
    U32         Reserved7;                              /* 28h */
    U32         Reserved8;                              /* 2Ch */
    U32         Reserved9;                              /* 30h */
    U32         Reserved10;                             /* 34h */
    U32         Reserved11;                             /* 38h */
    U32         Reserved12;                             /* 3Ch */
    U32         Reserved13;                             /* 40h */
} MPI_BOOT_DEVICE_SAS_WWN, MPI_POINTER PTR_MPI_BOOT_DEVICE_SAS_WWN;

typedef struct _MPI_BOOT_DEVICE_ENCLOSURE_SLOT
{
    U64         EnclosureLogicalID;                     /* 00h */
    U32         Reserved1;                              /* 08h */
    U32         Reserved2;                              /* 0Ch */
    U8          LUN[8];                                 /* 10h */
    U16         SlotNumber;                             /* 18h */
    U16         Reserved3;                              /* 1Ah */
    U32         Reserved4;                              /* 1Ch */
    U32         Reserved5;                              /* 20h */
    U32         Reserved6;                              /* 24h */
    U32         Reserved7;                              /* 28h */
    U32         Reserved8;                              /* 2Ch */
    U32         Reserved9;                              /* 30h */
    U32         Reserved10;                             /* 34h */
    U32         Reserved11;                             /* 38h */
    U32         Reserved12;                             /* 3Ch */
    U32         Reserved13;                             /* 40h */
} MPI_BOOT_DEVICE_ENCLOSURE_SLOT,
  MPI_POINTER PTR_MPI_BOOT_DEVICE_ENCLOSURE_SLOT;

typedef union _MPI_BIOSPAGE2_BOOT_DEVICE
{
    MPI_BOOT_DEVICE_ADAPTER_ORDER   AdapterOrder;
    MPI_BOOT_DEVICE_ADAPTER_NUMBER  AdapterNumber;
    MPI_BOOT_DEVICE_PCI_ADDRESS     PCIAddress;
    MPI_BOOT_DEVICE_PCI_SLOT_NUMBER PCISlotNumber;
    MPI_BOOT_DEVICE_FC_WWN          FcWwn;
    MPI_BOOT_DEVICE_SAS_WWN         SasWwn;
    MPI_BOOT_DEVICE_ENCLOSURE_SLOT  EnclosureSlot;
} MPI_BIOSPAGE2_BOOT_DEVICE, MPI_POINTER PTR_MPI_BIOSPAGE2_BOOT_DEVICE;

typedef struct _CONFIG_PAGE_BIOS_2
{
    CONFIG_PAGE_HEADER          Header;                 /* 00h */
    U32                         Reserved1;              /* 04h */
    U32                         Reserved2;              /* 08h */
    U32                         Reserved3;              /* 0Ch */
    U32                         Reserved4;              /* 10h */
    U32                         Reserved5;              /* 14h */
    U32                         Reserved6;              /* 18h */
    U8                          BootDeviceForm;         /* 1Ch */
    U8                          PrevBootDeviceForm;     /* 1Ch */
    U16                         Reserved8;              /* 1Eh */
    MPI_BIOSPAGE2_BOOT_DEVICE   BootDevice;             /* 20h */
} CONFIG_PAGE_BIOS_2, MPI_POINTER PTR_CONFIG_PAGE_BIOS_2,
  BIOSPage2_t, MPI_POINTER pBIOSPage2_t;

#define MPI_BIOSPAGE2_PAGEVERSION                       (0x02)

#define MPI_BIOSPAGE2_FORM_MASK                         (0x0F)
#define MPI_BIOSPAGE2_FORM_ADAPTER_ORDER                (0x00)
#define MPI_BIOSPAGE2_FORM_ADAPTER_NUMBER               (0x01)
#define MPI_BIOSPAGE2_FORM_PCI_ADDRESS                  (0x02)
#define MPI_BIOSPAGE2_FORM_PCI_SLOT_NUMBER              (0x03)
#define MPI_BIOSPAGE2_FORM_FC_WWN                       (0x04)
#define MPI_BIOSPAGE2_FORM_SAS_WWN                      (0x05)
#define MPI_BIOSPAGE2_FORM_ENCLOSURE_SLOT               (0x06)

typedef struct _CONFIG_PAGE_BIOS_4
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U64                     ReassignmentBaseWWID;       /* 04h */
} CONFIG_PAGE_BIOS_4, MPI_POINTER PTR_CONFIG_PAGE_BIOS_4,
  BIOSPage4_t, MPI_POINTER pBIOSPage4_t;

#define MPI_BIOSPAGE4_PAGEVERSION                       (0x00)


/****************************************************************************
*   SCSI Port Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_SCSI_PORT_0
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Capabilities;               /* 04h */
    U32                     PhysicalInterface;          /* 08h */
} CONFIG_PAGE_SCSI_PORT_0, MPI_POINTER PTR_CONFIG_PAGE_SCSI_PORT_0,
  SCSIPortPage0_t, MPI_POINTER pSCSIPortPage0_t;

#define MPI_SCSIPORTPAGE0_PAGEVERSION                   (0x02)

#define MPI_SCSIPORTPAGE0_CAP_IU                        (0x00000001)
#define MPI_SCSIPORTPAGE0_CAP_DT                        (0x00000002)
#define MPI_SCSIPORTPAGE0_CAP_QAS                       (0x00000004)
#define MPI_SCSIPORTPAGE0_CAP_MIN_SYNC_PERIOD_MASK      (0x0000FF00)
#define MPI_SCSIPORTPAGE0_SYNC_ASYNC                    (0x00)
#define MPI_SCSIPORTPAGE0_SYNC_5                        (0x32)
#define MPI_SCSIPORTPAGE0_SYNC_10                       (0x19)
#define MPI_SCSIPORTPAGE0_SYNC_20                       (0x0C)
#define MPI_SCSIPORTPAGE0_SYNC_33_33                    (0x0B)
#define MPI_SCSIPORTPAGE0_SYNC_40                       (0x0A)
#define MPI_SCSIPORTPAGE0_SYNC_80                       (0x09)
#define MPI_SCSIPORTPAGE0_SYNC_160                      (0x08)
#define MPI_SCSIPORTPAGE0_SYNC_UNKNOWN                  (0xFF)

#define MPI_SCSIPORTPAGE0_CAP_SHIFT_MIN_SYNC_PERIOD     (8)
#define MPI_SCSIPORTPAGE0_CAP_GET_MIN_SYNC_PERIOD(Cap)      \
    (  ((Cap) & MPI_SCSIPORTPAGE0_CAP_MIN_SYNC_PERIOD_MASK) \
    >> MPI_SCSIPORTPAGE0_CAP_SHIFT_MIN_SYNC_PERIOD          \
    )
#define MPI_SCSIPORTPAGE0_CAP_MAX_SYNC_OFFSET_MASK      (0x00FF0000)
#define MPI_SCSIPORTPAGE0_CAP_SHIFT_MAX_SYNC_OFFSET     (16)
#define MPI_SCSIPORTPAGE0_CAP_GET_MAX_SYNC_OFFSET(Cap)      \
    (  ((Cap) & MPI_SCSIPORTPAGE0_CAP_MAX_SYNC_OFFSET_MASK) \
    >> MPI_SCSIPORTPAGE0_CAP_SHIFT_MAX_SYNC_OFFSET          \
    )
#define MPI_SCSIPORTPAGE0_CAP_IDP                       (0x08000000)
#define MPI_SCSIPORTPAGE0_CAP_WIDE                      (0x20000000)
#define MPI_SCSIPORTPAGE0_CAP_AIP                       (0x80000000)

#define MPI_SCSIPORTPAGE0_PHY_SIGNAL_TYPE_MASK          (0x00000003)
#define MPI_SCSIPORTPAGE0_PHY_SIGNAL_HVD                (0x01)
#define MPI_SCSIPORTPAGE0_PHY_SIGNAL_SE                 (0x02)
#define MPI_SCSIPORTPAGE0_PHY_SIGNAL_LVD                (0x03)
#define MPI_SCSIPORTPAGE0_PHY_MASK_CONNECTED_ID         (0xFF000000)
#define MPI_SCSIPORTPAGE0_PHY_SHIFT_CONNECTED_ID        (24)
#define MPI_SCSIPORTPAGE0_PHY_BUS_FREE_CONNECTED_ID     (0xFE)
#define MPI_SCSIPORTPAGE0_PHY_UNKNOWN_CONNECTED_ID      (0xFF)


typedef struct _CONFIG_PAGE_SCSI_PORT_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Configuration;              /* 04h */
    U32                     OnBusTimerValue;            /* 08h */
    U8                      TargetConfig;               /* 0Ch */
    U8                      Reserved1;                  /* 0Dh */
    U16                     IDConfig;                   /* 0Eh */
} CONFIG_PAGE_SCSI_PORT_1, MPI_POINTER PTR_CONFIG_PAGE_SCSI_PORT_1,
  SCSIPortPage1_t, MPI_POINTER pSCSIPortPage1_t;

#define MPI_SCSIPORTPAGE1_PAGEVERSION                   (0x03)

/* Configuration values */
#define MPI_SCSIPORTPAGE1_CFG_PORT_SCSI_ID_MASK         (0x000000FF)
#define MPI_SCSIPORTPAGE1_CFG_PORT_RESPONSE_ID_MASK     (0xFFFF0000)
#define MPI_SCSIPORTPAGE1_CFG_SHIFT_PORT_RESPONSE_ID    (16)

/* TargetConfig values */
#define MPI_SCSIPORTPAGE1_TARGCONFIG_TARG_ONLY        (0x01)
#define MPI_SCSIPORTPAGE1_TARGCONFIG_INIT_TARG        (0x02)


typedef struct _MPI_DEVICE_INFO
{
    U8      Timeout;                                    /* 00h */
    U8      SyncFactor;                                 /* 01h */
    U16     DeviceFlags;                                /* 02h */
} MPI_DEVICE_INFO, MPI_POINTER PTR_MPI_DEVICE_INFO,
  MpiDeviceInfo_t, MPI_POINTER pMpiDeviceInfo_t;

typedef struct _CONFIG_PAGE_SCSI_PORT_2
{
    CONFIG_PAGE_HEADER  Header;                         /* 00h */
    U32                 PortFlags;                      /* 04h */
    U32                 PortSettings;                   /* 08h */
    MPI_DEVICE_INFO     DeviceSettings[16];             /* 0Ch */
} CONFIG_PAGE_SCSI_PORT_2, MPI_POINTER PTR_CONFIG_PAGE_SCSI_PORT_2,
  SCSIPortPage2_t, MPI_POINTER pSCSIPortPage2_t;

#define MPI_SCSIPORTPAGE2_PAGEVERSION                       (0x02)

/* PortFlags values */
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_SCAN_HIGH_TO_LOW       (0x00000001)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_AVOID_SCSI_RESET       (0x00000004)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_ALTERNATE_CHS          (0x00000008)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_TERMINATION_DISABLE    (0x00000010)

#define MPI_SCSIPORTPAGE2_PORT_FLAGS_DV_MASK                (0x00000060)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_FULL_DV                (0x00000000)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_BASIC_DV_ONLY          (0x00000020)
#define MPI_SCSIPORTPAGE2_PORT_FLAGS_OFF_DV                 (0x00000060)


/* PortSettings values */
#define MPI_SCSIPORTPAGE2_PORT_HOST_ID_MASK                 (0x0000000F)
#define MPI_SCSIPORTPAGE2_PORT_MASK_INIT_HBA                (0x00000030)
#define MPI_SCSIPORTPAGE2_PORT_DISABLE_INIT_HBA             (0x00000000)
#define MPI_SCSIPORTPAGE2_PORT_BIOS_INIT_HBA                (0x00000010)
#define MPI_SCSIPORTPAGE2_PORT_OS_INIT_HBA                  (0x00000020)
#define MPI_SCSIPORTPAGE2_PORT_BIOS_OS_INIT_HBA             (0x00000030)
#define MPI_SCSIPORTPAGE2_PORT_REMOVABLE_MEDIA              (0x000000C0)
#define MPI_SCSIPORTPAGE2_PORT_RM_NONE                      (0x00000000)
#define MPI_SCSIPORTPAGE2_PORT_RM_BOOT_ONLY                 (0x00000040)
#define MPI_SCSIPORTPAGE2_PORT_RM_WITH_MEDIA                (0x00000080)
#define MPI_SCSIPORTPAGE2_PORT_SPINUP_DELAY_MASK            (0x00000F00)
#define MPI_SCSIPORTPAGE2_PORT_SHIFT_SPINUP_DELAY           (8)
#define MPI_SCSIPORTPAGE2_PORT_MASK_NEGO_MASTER_SETTINGS    (0x00003000)
#define MPI_SCSIPORTPAGE2_PORT_NEGO_MASTER_SETTINGS         (0x00000000)
#define MPI_SCSIPORTPAGE2_PORT_NONE_MASTER_SETTINGS         (0x00001000)
#define MPI_SCSIPORTPAGE2_PORT_ALL_MASTER_SETTINGS          (0x00003000)

#define MPI_SCSIPORTPAGE2_DEVICE_DISCONNECT_ENABLE          (0x0001)
#define MPI_SCSIPORTPAGE2_DEVICE_ID_SCAN_ENABLE             (0x0002)
#define MPI_SCSIPORTPAGE2_DEVICE_LUN_SCAN_ENABLE            (0x0004)
#define MPI_SCSIPORTPAGE2_DEVICE_TAG_QUEUE_ENABLE           (0x0008)
#define MPI_SCSIPORTPAGE2_DEVICE_WIDE_DISABLE               (0x0010)
#define MPI_SCSIPORTPAGE2_DEVICE_BOOT_CHOICE                (0x0020)


/****************************************************************************
*   SCSI Target Device Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_SCSI_DEVICE_0
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     NegotiatedParameters;       /* 04h */
    U32                     Information;                /* 08h */
} CONFIG_PAGE_SCSI_DEVICE_0, MPI_POINTER PTR_CONFIG_PAGE_SCSI_DEVICE_0,
  SCSIDevicePage0_t, MPI_POINTER pSCSIDevicePage0_t;

#define MPI_SCSIDEVPAGE0_PAGEVERSION                    (0x04)

#define MPI_SCSIDEVPAGE0_NP_IU                          (0x00000001)
#define MPI_SCSIDEVPAGE0_NP_DT                          (0x00000002)
#define MPI_SCSIDEVPAGE0_NP_QAS                         (0x00000004)
#define MPI_SCSIDEVPAGE0_NP_HOLD_MCS                    (0x00000008)
#define MPI_SCSIDEVPAGE0_NP_WR_FLOW                     (0x00000010)
#define MPI_SCSIDEVPAGE0_NP_RD_STRM                     (0x00000020)
#define MPI_SCSIDEVPAGE0_NP_RTI                         (0x00000040)
#define MPI_SCSIDEVPAGE0_NP_PCOMP_EN                    (0x00000080)
#define MPI_SCSIDEVPAGE0_NP_NEG_SYNC_PERIOD_MASK        (0x0000FF00)
#define MPI_SCSIDEVPAGE0_NP_SHIFT_SYNC_PERIOD           (8)
#define MPI_SCSIDEVPAGE0_NP_NEG_SYNC_OFFSET_MASK        (0x00FF0000)
#define MPI_SCSIDEVPAGE0_NP_SHIFT_SYNC_OFFSET           (16)
#define MPI_SCSIDEVPAGE0_NP_IDP                         (0x08000000)
#define MPI_SCSIDEVPAGE0_NP_WIDE                        (0x20000000)
#define MPI_SCSIDEVPAGE0_NP_AIP                         (0x80000000)

#define MPI_SCSIDEVPAGE0_INFO_PARAMS_NEGOTIATED         (0x00000001)
#define MPI_SCSIDEVPAGE0_INFO_SDTR_REJECTED             (0x00000002)
#define MPI_SCSIDEVPAGE0_INFO_WDTR_REJECTED             (0x00000004)
#define MPI_SCSIDEVPAGE0_INFO_PPR_REJECTED              (0x00000008)


typedef struct _CONFIG_PAGE_SCSI_DEVICE_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     RequestedParameters;        /* 04h */
    U32                     Reserved;                   /* 08h */
    U32                     Configuration;              /* 0Ch */
} CONFIG_PAGE_SCSI_DEVICE_1, MPI_POINTER PTR_CONFIG_PAGE_SCSI_DEVICE_1,
  SCSIDevicePage1_t, MPI_POINTER pSCSIDevicePage1_t;

#define MPI_SCSIDEVPAGE1_PAGEVERSION                    (0x05)

#define MPI_SCSIDEVPAGE1_RP_IU                          (0x00000001)
#define MPI_SCSIDEVPAGE1_RP_DT                          (0x00000002)
#define MPI_SCSIDEVPAGE1_RP_QAS                         (0x00000004)
#define MPI_SCSIDEVPAGE1_RP_HOLD_MCS                    (0x00000008)
#define MPI_SCSIDEVPAGE1_RP_WR_FLOW                     (0x00000010)
#define MPI_SCSIDEVPAGE1_RP_RD_STRM                     (0x00000020)
#define MPI_SCSIDEVPAGE1_RP_RTI                         (0x00000040)
#define MPI_SCSIDEVPAGE1_RP_PCOMP_EN                    (0x00000080)
#define MPI_SCSIDEVPAGE1_RP_MIN_SYNC_PERIOD_MASK        (0x0000FF00)
#define MPI_SCSIDEVPAGE1_RP_SHIFT_MIN_SYNC_PERIOD       (8)
#define MPI_SCSIDEVPAGE1_RP_MAX_SYNC_OFFSET_MASK        (0x00FF0000)
#define MPI_SCSIDEVPAGE1_RP_SHIFT_MAX_SYNC_OFFSET       (16)
#define MPI_SCSIDEVPAGE1_RP_IDP                         (0x08000000)
#define MPI_SCSIDEVPAGE1_RP_WIDE                        (0x20000000)
#define MPI_SCSIDEVPAGE1_RP_AIP                         (0x80000000)

#define MPI_SCSIDEVPAGE1_CONF_WDTR_DISALLOWED           (0x00000002)
#define MPI_SCSIDEVPAGE1_CONF_SDTR_DISALLOWED           (0x00000004)
#define MPI_SCSIDEVPAGE1_CONF_EXTENDED_PARAMS_ENABLE    (0x00000008)
#define MPI_SCSIDEVPAGE1_CONF_FORCE_PPR_MSG             (0x00000010)


typedef struct _CONFIG_PAGE_SCSI_DEVICE_2
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     DomainValidation;           /* 04h */
    U32                     ParityPipeSelect;           /* 08h */
    U32                     DataPipeSelect;             /* 0Ch */
} CONFIG_PAGE_SCSI_DEVICE_2, MPI_POINTER PTR_CONFIG_PAGE_SCSI_DEVICE_2,
  SCSIDevicePage2_t, MPI_POINTER pSCSIDevicePage2_t;

#define MPI_SCSIDEVPAGE2_PAGEVERSION                    (0x01)

#define MPI_SCSIDEVPAGE2_DV_ISI_ENABLE                  (0x00000010)
#define MPI_SCSIDEVPAGE2_DV_SECONDARY_DRIVER_ENABLE     (0x00000020)
#define MPI_SCSIDEVPAGE2_DV_SLEW_RATE_CTRL              (0x00000380)
#define MPI_SCSIDEVPAGE2_DV_PRIM_DRIVE_STR_CTRL         (0x00001C00)
#define MPI_SCSIDEVPAGE2_DV_SECOND_DRIVE_STR_CTRL       (0x0000E000)
#define MPI_SCSIDEVPAGE2_DV_XCLKH_ST                    (0x10000000)
#define MPI_SCSIDEVPAGE2_DV_XCLKS_ST                    (0x20000000)
#define MPI_SCSIDEVPAGE2_DV_XCLKH_DT                    (0x40000000)
#define MPI_SCSIDEVPAGE2_DV_XCLKS_DT                    (0x80000000)

#define MPI_SCSIDEVPAGE2_PPS_PPS_MASK                   (0x00000003)

#define MPI_SCSIDEVPAGE2_DPS_BIT_0_PL_SELECT_MASK       (0x00000003)
#define MPI_SCSIDEVPAGE2_DPS_BIT_1_PL_SELECT_MASK       (0x0000000C)
#define MPI_SCSIDEVPAGE2_DPS_BIT_2_PL_SELECT_MASK       (0x00000030)
#define MPI_SCSIDEVPAGE2_DPS_BIT_3_PL_SELECT_MASK       (0x000000C0)
#define MPI_SCSIDEVPAGE2_DPS_BIT_4_PL_SELECT_MASK       (0x00000300)
#define MPI_SCSIDEVPAGE2_DPS_BIT_5_PL_SELECT_MASK       (0x00000C00)
#define MPI_SCSIDEVPAGE2_DPS_BIT_6_PL_SELECT_MASK       (0x00003000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_7_PL_SELECT_MASK       (0x0000C000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_8_PL_SELECT_MASK       (0x00030000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_9_PL_SELECT_MASK       (0x000C0000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_10_PL_SELECT_MASK      (0x00300000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_11_PL_SELECT_MASK      (0x00C00000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_12_PL_SELECT_MASK      (0x03000000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_13_PL_SELECT_MASK      (0x0C000000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_14_PL_SELECT_MASK      (0x30000000)
#define MPI_SCSIDEVPAGE2_DPS_BIT_15_PL_SELECT_MASK      (0xC0000000)


typedef struct _CONFIG_PAGE_SCSI_DEVICE_3
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U16                     MsgRejectCount;             /* 04h */
    U16                     PhaseErrorCount;            /* 06h */
    U16                     ParityErrorCount;           /* 08h */
    U16                     Reserved;                   /* 0Ah */
} CONFIG_PAGE_SCSI_DEVICE_3, MPI_POINTER PTR_CONFIG_PAGE_SCSI_DEVICE_3,
  SCSIDevicePage3_t, MPI_POINTER pSCSIDevicePage3_t;

#define MPI_SCSIDEVPAGE3_PAGEVERSION                    (0x00)

#define MPI_SCSIDEVPAGE3_MAX_COUNTER                    (0xFFFE)
#define MPI_SCSIDEVPAGE3_UNSUPPORTED_COUNTER            (0xFFFF)


/****************************************************************************
*   FC Port Config Pages
****************************************************************************/

typedef struct _CONFIG_PAGE_FC_PORT_0
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
    U8                      MPIPortNumber;              /* 08h */
    U8                      LinkType;                   /* 09h */
    U8                      PortState;                  /* 0Ah */
    U8                      Reserved;                   /* 0Bh */
    U32                     PortIdentifier;             /* 0Ch */
    U64                     WWNN;                       /* 10h */
    U64                     WWPN;                       /* 18h */
    U32                     SupportedServiceClass;      /* 20h */
    U32                     SupportedSpeeds;            /* 24h */
    U32                     CurrentSpeed;               /* 28h */
    U32                     MaxFrameSize;               /* 2Ch */
    U64                     FabricWWNN;                 /* 30h */
    U64                     FabricWWPN;                 /* 38h */
    U32                     DiscoveredPortsCount;       /* 40h */
    U32                     MaxInitiators;              /* 44h */
    U8                      MaxAliasesSupported;        /* 48h */
    U8                      MaxHardAliasesSupported;    /* 49h */
    U8                      NumCurrentAliases;          /* 4Ah */
    U8                      Reserved1;                  /* 4Bh */
} CONFIG_PAGE_FC_PORT_0, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_0,
  FCPortPage0_t, MPI_POINTER pFCPortPage0_t;

#define MPI_FCPORTPAGE0_PAGEVERSION                     (0x02)

#define MPI_FCPORTPAGE0_FLAGS_PROT_MASK                 (0x0000000F)
#define MPI_FCPORTPAGE0_FLAGS_PROT_FCP_INIT             (MPI_PORTFACTS_PROTOCOL_INITIATOR)
#define MPI_FCPORTPAGE0_FLAGS_PROT_FCP_TARG             (MPI_PORTFACTS_PROTOCOL_TARGET)
#define MPI_FCPORTPAGE0_FLAGS_PROT_LAN                  (MPI_PORTFACTS_PROTOCOL_LAN)
#define MPI_FCPORTPAGE0_FLAGS_PROT_LOGBUSADDR           (MPI_PORTFACTS_PROTOCOL_LOGBUSADDR)

#define MPI_FCPORTPAGE0_FLAGS_ALIAS_ALPA_SUPPORTED      (0x00000010)
#define MPI_FCPORTPAGE0_FLAGS_ALIAS_WWN_SUPPORTED       (0x00000020)
#define MPI_FCPORTPAGE0_FLAGS_FABRIC_WWN_VALID          (0x00000040)

#define MPI_FCPORTPAGE0_FLAGS_ATTACH_TYPE_MASK          (0x00000F00)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_NO_INIT            (0x00000000)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_POINT_TO_POINT     (0x00000100)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_PRIVATE_LOOP       (0x00000200)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_FABRIC_DIRECT      (0x00000400)
#define MPI_FCPORTPAGE0_FLAGS_ATTACH_PUBLIC_LOOP        (0x00000800)

#define MPI_FCPORTPAGE0_LTYPE_RESERVED                  (0x00)
#define MPI_FCPORTPAGE0_LTYPE_OTHER                     (0x01)
#define MPI_FCPORTPAGE0_LTYPE_UNKNOWN                   (0x02)
#define MPI_FCPORTPAGE0_LTYPE_COPPER                    (0x03)
#define MPI_FCPORTPAGE0_LTYPE_SINGLE_1300               (0x04)
#define MPI_FCPORTPAGE0_LTYPE_SINGLE_1500               (0x05)
#define MPI_FCPORTPAGE0_LTYPE_50_LASER_MULTI            (0x06)
#define MPI_FCPORTPAGE0_LTYPE_50_LED_MULTI              (0x07)
#define MPI_FCPORTPAGE0_LTYPE_62_LASER_MULTI            (0x08)
#define MPI_FCPORTPAGE0_LTYPE_62_LED_MULTI              (0x09)
#define MPI_FCPORTPAGE0_LTYPE_MULTI_LONG_WAVE           (0x0A)
#define MPI_FCPORTPAGE0_LTYPE_MULTI_SHORT_WAVE          (0x0B)
#define MPI_FCPORTPAGE0_LTYPE_LASER_SHORT_WAVE          (0x0C)
#define MPI_FCPORTPAGE0_LTYPE_LED_SHORT_WAVE            (0x0D)
#define MPI_FCPORTPAGE0_LTYPE_1300_LONG_WAVE            (0x0E)
#define MPI_FCPORTPAGE0_LTYPE_1500_LONG_WAVE            (0x0F)

#define MPI_FCPORTPAGE0_PORTSTATE_UNKNOWN               (0x01)      /*(SNIA)HBA_PORTSTATE_UNKNOWN       1 Unknown */
#define MPI_FCPORTPAGE0_PORTSTATE_ONLINE                (0x02)      /*(SNIA)HBA_PORTSTATE_ONLINE        2 Operational */
#define MPI_FCPORTPAGE0_PORTSTATE_OFFLINE               (0x03)      /*(SNIA)HBA_PORTSTATE_OFFLINE       3 User Offline */
#define MPI_FCPORTPAGE0_PORTSTATE_BYPASSED              (0x04)      /*(SNIA)HBA_PORTSTATE_BYPASSED      4 Bypassed */
#define MPI_FCPORTPAGE0_PORTSTATE_DIAGNOST              (0x05)      /*(SNIA)HBA_PORTSTATE_DIAGNOSTICS   5 In diagnostics mode */
#define MPI_FCPORTPAGE0_PORTSTATE_LINKDOWN              (0x06)      /*(SNIA)HBA_PORTSTATE_LINKDOWN      6 Link Down */
#define MPI_FCPORTPAGE0_PORTSTATE_ERROR                 (0x07)      /*(SNIA)HBA_PORTSTATE_ERROR         7 Port Error */
#define MPI_FCPORTPAGE0_PORTSTATE_LOOPBACK              (0x08)      /*(SNIA)HBA_PORTSTATE_LOOPBACK      8 Loopback */

#define MPI_FCPORTPAGE0_SUPPORT_CLASS_1                 (0x00000001)
#define MPI_FCPORTPAGE0_SUPPORT_CLASS_2                 (0x00000002)
#define MPI_FCPORTPAGE0_SUPPORT_CLASS_3                 (0x00000004)

#define MPI_FCPORTPAGE0_SUPPORT_SPEED_UKNOWN            (0x00000000) /* (SNIA)HBA_PORTSPEED_UNKNOWN 0   Unknown - transceiver incapable of reporting */
#define MPI_FCPORTPAGE0_SUPPORT_1GBIT_SPEED             (0x00000001) /* (SNIA)HBA_PORTSPEED_1GBIT   1   1 GBit/sec */
#define MPI_FCPORTPAGE0_SUPPORT_2GBIT_SPEED             (0x00000002) /* (SNIA)HBA_PORTSPEED_2GBIT   2   2 GBit/sec */
#define MPI_FCPORTPAGE0_SUPPORT_10GBIT_SPEED            (0x00000004) /* (SNIA)HBA_PORTSPEED_10GBIT  4  10 GBit/sec */
#define MPI_FCPORTPAGE0_SUPPORT_4GBIT_SPEED             (0x00000008) /* (SNIA)HBA_PORTSPEED_4GBIT   8   4 GBit/sec */

#define MPI_FCPORTPAGE0_CURRENT_SPEED_UKNOWN            MPI_FCPORTPAGE0_SUPPORT_SPEED_UKNOWN
#define MPI_FCPORTPAGE0_CURRENT_SPEED_1GBIT             MPI_FCPORTPAGE0_SUPPORT_1GBIT_SPEED
#define MPI_FCPORTPAGE0_CURRENT_SPEED_2GBIT             MPI_FCPORTPAGE0_SUPPORT_2GBIT_SPEED
#define MPI_FCPORTPAGE0_CURRENT_SPEED_10GBIT            MPI_FCPORTPAGE0_SUPPORT_10GBIT_SPEED
#define MPI_FCPORTPAGE0_CURRENT_SPEED_4GBIT             MPI_FCPORTPAGE0_SUPPORT_4GBIT_SPEED
#define MPI_FCPORTPAGE0_CURRENT_SPEED_NOT_NEGOTIATED    (0x00008000)        /* (SNIA)HBA_PORTSPEED_NOT_NEGOTIATED (1<<15) Speed not established */


typedef struct _CONFIG_PAGE_FC_PORT_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     Flags;                      /* 04h */
    U64                     NoSEEPROMWWNN;              /* 08h */
    U64                     NoSEEPROMWWPN;              /* 10h */
    U8                      HardALPA;                   /* 18h */
    U8                      LinkConfig;                 /* 19h */
    U8                      TopologyConfig;             /* 1Ah */
    U8                      AltConnector;               /* 1Bh */
    U8                      NumRequestedAliases;        /* 1Ch */
    U8                      RR_TOV;                     /* 1Dh */
    U8                      InitiatorDeviceTimeout;     /* 1Eh */
    U8                      InitiatorIoPendTimeout;     /* 1Fh */
} CONFIG_PAGE_FC_PORT_1, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_1,
  FCPortPage1_t, MPI_POINTER pFCPortPage1_t;

#define MPI_FCPORTPAGE1_PAGEVERSION                     (0x06)

#define MPI_FCPORTPAGE1_FLAGS_EXT_FCP_STATUS_EN         (0x08000000)
#define MPI_FCPORTPAGE1_FLAGS_IMMEDIATE_ERROR_REPLY     (0x04000000)
#define MPI_FCPORTPAGE1_FLAGS_FORCE_USE_NOSEEPROM_WWNS  (0x02000000)
#define MPI_FCPORTPAGE1_FLAGS_VERBOSE_RESCAN_EVENTS     (0x01000000)
#define MPI_FCPORTPAGE1_FLAGS_TARGET_MODE_OXID          (0x00800000)
#define MPI_FCPORTPAGE1_FLAGS_PORT_OFFLINE              (0x00400000)
#define MPI_FCPORTPAGE1_FLAGS_SOFT_ALPA_FALLBACK        (0x00200000)
#define MPI_FCPORTPAGE1_FLAGS_TARGET_LARGE_CDB_ENABLE   (0x00000080)
#define MPI_FCPORTPAGE1_FLAGS_MASK_RR_TOV_UNITS         (0x00000070)
#define MPI_FCPORTPAGE1_FLAGS_SUPPRESS_PROT_REG         (0x00000008)
#define MPI_FCPORTPAGE1_FLAGS_PLOGI_ON_LOGO             (0x00000004)
#define MPI_FCPORTPAGE1_FLAGS_MAINTAIN_LOGINS           (0x00000002)
#define MPI_FCPORTPAGE1_FLAGS_SORT_BY_DID               (0x00000001)
#define MPI_FCPORTPAGE1_FLAGS_SORT_BY_WWN               (0x00000000)

#define MPI_FCPORTPAGE1_FLAGS_PROT_MASK                 (0xF0000000)
#define MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT                (28)
#define MPI_FCPORTPAGE1_FLAGS_PROT_FCP_INIT             ((U32)MPI_PORTFACTS_PROTOCOL_INITIATOR << MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT)
#define MPI_FCPORTPAGE1_FLAGS_PROT_FCP_TARG             ((U32)MPI_PORTFACTS_PROTOCOL_TARGET << MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT)
#define MPI_FCPORTPAGE1_FLAGS_PROT_LAN                  ((U32)MPI_PORTFACTS_PROTOCOL_LAN << MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT)
#define MPI_FCPORTPAGE1_FLAGS_PROT_LOGBUSADDR           ((U32)MPI_PORTFACTS_PROTOCOL_LOGBUSADDR << MPI_FCPORTPAGE1_FLAGS_PROT_SHIFT)

#define MPI_FCPORTPAGE1_FLAGS_NONE_RR_TOV_UNITS         (0x00000000)
#define MPI_FCPORTPAGE1_FLAGS_THOUSANDTH_RR_TOV_UNITS   (0x00000010)
#define MPI_FCPORTPAGE1_FLAGS_TENTH_RR_TOV_UNITS        (0x00000030)
#define MPI_FCPORTPAGE1_FLAGS_TEN_RR_TOV_UNITS          (0x00000050)

#define MPI_FCPORTPAGE1_HARD_ALPA_NOT_USED              (0xFF)

#define MPI_FCPORTPAGE1_LCONFIG_SPEED_MASK              (0x0F)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_1GIG              (0x00)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_2GIG              (0x01)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_4GIG              (0x02)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_10GIG             (0x03)
#define MPI_FCPORTPAGE1_LCONFIG_SPEED_AUTO              (0x0F)

#define MPI_FCPORTPAGE1_TOPOLOGY_MASK                   (0x0F)
#define MPI_FCPORTPAGE1_TOPOLOGY_NLPORT                 (0x01)
#define MPI_FCPORTPAGE1_TOPOLOGY_NPORT                  (0x02)
#define MPI_FCPORTPAGE1_TOPOLOGY_AUTO                   (0x0F)

#define MPI_FCPORTPAGE1_ALT_CONN_UNKNOWN                (0x00)

#define MPI_FCPORTPAGE1_INITIATOR_DEV_TIMEOUT_MASK      (0x7F)
#define MPI_FCPORTPAGE1_INITIATOR_DEV_UNIT_16           (0x80)


typedef struct _CONFIG_PAGE_FC_PORT_2
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U8                      NumberActive;               /* 04h */
    U8                      ALPA[127];                  /* 05h */
} CONFIG_PAGE_FC_PORT_2, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_2,
  FCPortPage2_t, MPI_POINTER pFCPortPage2_t;

#define MPI_FCPORTPAGE2_PAGEVERSION                     (0x01)


typedef struct _WWN_FORMAT
{
    U64                     WWNN;                       /* 00h */
    U64                     WWPN;                       /* 08h */
} WWN_FORMAT, MPI_POINTER PTR_WWN_FORMAT,
  WWNFormat, MPI_POINTER pWWNFormat;

typedef union _FC_PORT_PERSISTENT_PHYSICAL_ID
{
    WWN_FORMAT              WWN;
    U32                     Did;
} FC_PORT_PERSISTENT_PHYSICAL_ID, MPI_POINTER PTR_FC_PORT_PERSISTENT_PHYSICAL_ID,
  PersistentPhysicalId_t, MPI_POINTER pPersistentPhysicalId_t;

typedef struct _FC_PORT_PERSISTENT
{
    FC_PORT_PERSISTENT_PHYSICAL_ID  PhysicalIdentifier; /* 00h */
    U8                              TargetID;           /* 10h */
    U8                              Bus;                /* 11h */
    U16                             Flags;              /* 12h */
} FC_PORT_PERSISTENT, MPI_POINTER PTR_FC_PORT_PERSISTENT,
  PersistentData_t, MPI_POINTER pPersistentData_t;

#define MPI_PERSISTENT_FLAGS_SHIFT                      (16)
#define MPI_PERSISTENT_FLAGS_ENTRY_VALID                (0x0001)
#define MPI_PERSISTENT_FLAGS_SCAN_ID                    (0x0002)
#define MPI_PERSISTENT_FLAGS_SCAN_LUNS                  (0x0004)
#define MPI_PERSISTENT_FLAGS_BOOT_DEVICE                (0x0008)
#define MPI_PERSISTENT_FLAGS_BY_DID                     (0x0080)

/*
 * Host code (drivers, BIOS, utilities, etc.) should leave this define set to
 * one and check Header.PageLength at runtime.
 */
#ifndef MPI_FC_PORT_PAGE_3_ENTRY_MAX
#define MPI_FC_PORT_PAGE_3_ENTRY_MAX        (1)
#endif

typedef struct _CONFIG_PAGE_FC_PORT_3
{
    CONFIG_PAGE_HEADER      Header;                                 /* 00h */
    FC_PORT_PERSISTENT      Entry[MPI_FC_PORT_PAGE_3_ENTRY_MAX];    /* 04h */
} CONFIG_PAGE_FC_PORT_3, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_3,
  FCPortPage3_t, MPI_POINTER pFCPortPage3_t;

#define MPI_FCPORTPAGE3_PAGEVERSION                     (0x01)


typedef struct _CONFIG_PAGE_FC_PORT_4
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
    U32                     PortFlags;                  /* 04h */
    U32                     PortSettings;               /* 08h */
} CONFIG_PAGE_FC_PORT_4, MPI_POINTER PTR_CONFIG_PAGE_FC_PORT_4,
  FCPortPage4_t, MPI_POINTER pFCPortPage4_t;

#define MPI_FCPORTPAGE4_PAGEVERSION                     (0x00)

#define MPI_FCPORTPAGE4_PORT_FLAGS_ALTERNATE_CHS        (0x00000008)

#define MPI_FCPORTPAGE4_PORT_MASK_INIT_HBA              (0x00000030)
#define MPI_FCPORTPAGE4_PORT_DISABLE_INIT_HBA           (0x00000000)
#define MPI_FCPORTPAGE4_PORT_BIOS_INIT_HBA              (0x00000010)
#define MPI_FCPORTPAGE4_PORT_OS_INIT_HBA     /*
 *  Copy(0x0t (c)20)
#define MPI_FCPORTPAGE4_orat_BIOS_OS_INIT_HBA*
 *  Copyright (c) 3000-2008 LSI Corporation.
 *
 *
REMOVABLE_MEDIme:  mpi_cnfright (c) C000-2008 LSI Corporation.
 *
 *
SPINUP_DELAY_MASK Pages
 *  CreatioF00)


typedef struct _CONFIG_ion._FC *
 *
5_ALIA    FO
{

 * U8/*
 * Flags;/*
 *  Copyr  --------  --------------/* 00h */ion   DescripAliasAlpa *  --------  --------  ---------------1----------16  ---Reserved *  --------  --------  ----------------2----------64----------WWNN--------
 *  05-08-00  00.10.01  Origin4pdate version number forP1.0 release.
 *  06-08-00  01.00.02  AddC-----} ------------
 *
 *  Date      Ve,
 SI CoPOINTER PTR-------------
 *
 *  Date      VesionFcPortPage5-----Info_t,n, Reserved2
pICE_0 page, updated th;on History
 *  ---------------
 *
 *  Drsion  ------------HEADER  --------  ------Header *  --------------------------------
 *
 *  Date      Ve---------ted  *  --- Added _PA FcPhLowestVersion, FcPe page version *                      f_DEVICE_0 page,the page version.
n to SCSI_PO;
0-2008 LSI Corporation.5-----VERSION  --------  ---------ight2)page and updated the page FLAGS_ALPA_ACQUIRED              Add100-2008 LSI Corporation.INFO_PARHARDRAMS_                  Added       definitionto SCSI_DEVICE_0 paor 1                  Add4  Removed batch controls from LAN_0 pagP and updated the
 *   8  Removed batch controls from LADISuctu                    Ad1rsio History
 *  ---------------
 *
 *  6G_TRANSFERS in
 *              _PORT_0, SCSI_DE---------------------------32  --------  ---------0 spec dated 4/26/2000.
 *   Added _PAGEVERSION de 06-30-00  01.00TimeSince0 spt *  --------  ----8ICE_1 pages.
 *  06-30-00  01.00.0xFrame
 *  --------  ------/* 1-----------s.
 *  06-30-00  01.00.R               version.
 *     page and updated the page
 *        Word
 *  --------  -------/* 2                 Moved FC_DEVICE_0 Pthe SubsystemVendorID and Subspage and updated the page
 *      LipCounReplySize to LAD and Sub3                 Moved FC_DEVICE_0Nos01.01.01  Original release fopage and updated the page
 *      Error              version.
/* 4                 Moved FC_DEVICE_0Dumped              version. FC ated the page version.
 *  11-02-00 nkFailure01.01.01  Origina/* 5                 Moved FC_DEVICE_0LossOfSync01.01.01  Originales tated the page version.
 *  11-02-0000  01ignal01.01.01  Origin/* 6                 Moved FC_DEVICE_0PrimativeSeqErr01.01.01  Oridrespage and updated the page
 *      Invalidd the 01.01.01  Origi/* 7                 Moved FC_DEVICE_0       Cr1.04  Modified config 7page and updated the page
 *      FcpInitiatorIo01.01.01  Orig/* 8------ FcPhLowestVersion, Fc6           Added _RESPONSE_ID_MASK def6nition to SCSI6PORT_1
 *                  6   page and updated the pag6 version.
 *                      Addrsion History
 *  ---------------
 *
 *  7 version.
 *                      Revised bus width definitions in SCSI_PORT_0,
 *                      SCSI_DEVICE_0 and SCSI_DEVICE_1 pageDescrip                 ortSymbolicName[256]updated thepage a FcPhLowestVersion, Fc7           Added _RESPONSE_ID_MASK def7nition to SCSI7PORT_1
 *                  7   page and updated the pag7       cleaned up.
 *                      Removed impedance flash from FC Por8 version.
 *                      Revised bus width definitions in SCSI_PORT_0,
 *               BitVector[8em 32 ch01.00.02  Added _PA FcPhLowestVersion, Fc8           Added _RESPONSE_ID_MASK def8nition to SCSI8PORT_1
 *                  8   page and updated the pag8       cleaned up.
 *                      Removed impedance flash from FC Por9 version.
 *                      Revised bus width definitions in SCSI_PORT_0,
 *                      SCSI_DEVICE_0 and SCSI_DEVICE_1 pages.
 *  06-30-00  01.00.Globalions for all pages.
 *N_1 page and updated the page
 *       *       1.0 release.
 *  06               RT_0,
 *               UnitTyperess description to spec.
 *  07-27RT_0,
 *               Physicale deNumbT_0, SCSI_DEVIC1 Addedw defines for the PageAddresNumAttachedNod          versSubsystemID fiefor 0.1o Unit Page 2 sizPVersion *  --------  -----Subsed _PAGEVER
 *                    UDPand
 *              Scsi Port6  01-29-01  01.01.07  Changed somIPAddress[1hem 32 chpage and updated the pa
 *                    0 spec d1d defines for Scsi Por, IO Unit Pa
 *                    TopologyDiscoverytion
 *  --
 * A        RAID Volume Page 2.9           Added _RESPONSE_ID_MASK def9nition to SCSI9PORT_1
 *                  9   page and updated the pag9       cleaned up.
 *                      Removed impedance flash from FC Por10_BASE_SFP_DATArsion   Descrip Original releasedated 4/26/2000.
 *  06-06-               pabilitiesFlags.
 *    Ext                  Removed    al release pabilitiesFlags.
 *    ConnC Pagd defines for Scsi Por1Update verspabilitiesFlags.
 *    Transceivege 2 and
 *       RT_F3for MPI_IOUNITPAGE2_FLAGS_RAID_DISncoding        version.
 *     B  01-29-01  01.01.07  Changed somBitRate_100mb
 *  --------          removedpabilitiesFlags.
 *     RVP2 IMPhysicalDisk struct.
 *1D  01-29-01  01.01.07  Changed somLength9u_km     Port Page 2, FC1E      and MPI_FCPORTPAGE1_FLAGS_IMMEDIATE_     *  --------  -    F      and MPI_FCPORTPAGE1_FLAGS_IMMEDIA50AP_PCING_TRANSFERS,
 * systemID fieMPI_FCPORTPAGE1_FLAGS_IMMEDIA62p5CING_TRANSFERS, an *  emoved define for
 *              MMEDIACopper_CING_TRANSFERS, *  LAGS_PARITY_ENABLE.
 *            0 spvpec d2     Port Page 2, FC              Added defines for MPIVendor make work.
 *        Port Page 2.
 * MPI_FCPORTPAGE1_FLAGS_HARD_ALP3hysicalDisk struct.
 * ED.
 *                      Added _SCSIDOUI[32 and
 *            35SI_PORT_1.
 *                      AddePN2 work.
 *         .
 *            
 *                      AddeRev[42 and
 *            es to match 
 *                    WavelMEDIA *                   4defines for MPI_FCPORTPAGE1_FLAGS_HARD_ALP4d defines for Scsi Por4              Removed MPI_SCSIPORTCC     ubsystemVendorID and Sub4       FcPhLowestVersion, Fc             IOCsion, Reserved2
 *                      r Manufacturing Page          10BaseSfpDataPORT_1
 *                  , RAID Volume P page and updated the        ID_UNKNOW and upda       sDisk Page 0.
 *  10-04-01 01GBIC *                      definitionto10-04-01 01FIXTED
 *      00.03  Removed batch cont10-04-01 01SFP/*
 *  Copyright3ake them 32
 *                    _MI   Added defi                   pa       Modified AX defines to7Fe for MPI_CONFIG_PAGETYPE_RAID_VEND_SPECsion: (0x8rsioth pages (arrays) are defiEXT 01.02.04   Adddefine for MPI_CONFIG_PAGETYPE_RA volumMODDEF1 *                      Modified some  for MPI_IOUT_0,
  to make them 32
 *               for MPI_IOU3cter unique.
 *                      M volumSEEPROMariable length pages (arrays) are defi for MPI_IOU5CISlotNum5ion define.
 *  05-31-02 01.02.07   Addeor 0.1ight6ion define.
 *  05-31-02 01.02.07   Adde7*        7ion define.
 *  05-31-02 01.02.07 VNDSPre pools and RAID
 *                     CONN1.02.04   Addedefine for MPI_CONFIG_PAGETYPE_RA_FLAGSSDISK.
 *                      Modified some _FLAGCOPPERNITPAGotNum field to CONFIG_PAGE_IOC_1 a           T_0,
 *unique.
 *                      M_FLAGBNC_TNSDISK.
le length pages (arrays) are defi       AXIALC_PORT_0.
field to CONFIG_PAGE_IOC_2_RA_FLAGFIBERJAC:  01                  reserved byte a_FLAGLSDISK.
 *                            Added de_FLAGMT_RJ      Added BucketsRemaining to LFIG_PAGE_FC_POU            Add9 *                      Added new G            AddAnew config page: CONFIG_PAGE_FC_POPT_PIGT_PORT_0.
Bnew config page: CONFIG_PAGE_FC_PRSV1d how varightCAdded more MPI_SCSIDEVPAGE1_RP_ defines                  Added generic defines _FLAGHSSDC_II        2000-2008 LSI Corporat   Added AltCoPR             2FIG_PAGE_IOC_5.
 *                RSV2es.
 *      2iases, MaxHardAliases, and NumCurr                        Added generic defines _FLAGor
 *       ls and RAID
 *                      NCODE_UNspar.
 *  11-01-01 01.02.05   Added define   Adde8B10B
 *                      Modified some ORT_0.
4B5       otNum field to CONFIG_PAGE_IOC_1 al  AddeNRZaracter unique.
 *                      M     AdMANCHESed2
le leng-TE, and cross channel for
 *            EXTENDEdified IOCPage2 CapabilitiesFlags.
 *    Options[22 and
 *         nges to match MPIUFACTPAGE_DEVID_53C1035.
 *   MaxORT_1.
 *             LAGS_PARITY_ENABLE.
 *            GE5_FLAGied defines for Scsies tONFIG_PAGEATTR_RO_PERSISTENT.
 *  _SCSIDS        Increased size o5ED.
 *                      Added DateCodee 2 and
 *         g pagR_USE_STATIC_VOLUME_ID define.
 *  iagMonitoring 04-09-01  01.dresdefines for MPI_FCPORTPAGE1_FLAGS_Enhanced_PAGE_FANSFERS, and
 * 6       and MPI_FCPORTPAGE1_FLAGS_ISFF8472Compli Addfor page addresFIG_PAGE_LAN_1 to match preferred 64-EXT *  --------  --------ines         Added structures for MFlags defines forPage 4, IO Unit
 *                      Pag     In CONFIG_PAGE_F            Extended Volume Page 0, and
 *              ardALPA.
 *      PhysDisk Page 0.
 *  10-EXT    ION1_RATESEor anLE.
 *                       01-16-04 01TXredefined       tiatorDeviceTimeout and InitiatorIoPFAUL    7-12-02 01.02.08   Added mor 01-16-04 01L     VERTT_0.
 *                      
 *                     Added ITE, and cross channel for
 *           G_TRANSFERS in
 *                      SCSI    SCSI_PORT_0, SCSI_DE--------------------------ORT_10.
 *  04-29-04 01.02.14   Addetion
 *  --------  -0.
 *  01-29-01  01.01.07  Changed somI_FCPORTPAGE1_FLAGS_HARD_ALPA_ONLY
 *   ----vice Page 0
 *                    I_FCPORTPAGE1_FLAGS_HARD_ALPGE1_CONF_WDT----8-08-01  01RT_0,
 *                pages.
 *  05-11-04HwConfigCSI_DEVICE_0, anome new defines for the PageAddrespages.
 *  05-11-04 01.03.01ed Page Version     remove                    Page 3, IOC Pageias.
 *  as-09-01  01.01.11               aced Reserved1 field
 *                 D_DISABALPA.g page changes to match MPIE_FC_PORT_10.
 *  04-29-04 01.02.14   Added.02.12 pecific[3_POR             Added structures for            Added _RESPONSE_ID_MASK def10     with ADISCH Page 0, and
 *                 page and updated the pag10 version.
 *                           
/* standard PI_IOU pin 2008 iAGE_F (from PHYSDspec.)----G_PAGE_MANUFACTURING_4
 *FO_PARPI_IOUsion:  01.05.18
 *  Creationd new config page: CON
 *                T_0,
 *            defined fo         definitionto SCS               Added Mwo new bits defined fo3  Removed batch controls               0             Modified CONFIG                   page v                NOPHYSDISK.
 *    defined for IO Unit Page 1 Flags field.
 *         confIEEE_C        defined fo              reserve           Added EEDP     wo new bits defined fofield to CONFIG_PAGE_t Page 2.
 *        16-0CAL_LW Four new Flags bits defined for IO Unit Page 2.
 *        ped the page bits defined foue.
 *               REPLY_DISABLE.
 *    W                     to specify the BIOS boot device.
 *          _LXlags s          ied CONFIG_PAGE_IO_UNIT_2 to add three new fields
_S     Added Sore defines ford RAID
 *             t Page 2.
 * 64-bit _O  Two new bits defines fo *                    Added define for 01-1AS_IOUNIT0_DS_SUright (c) 2000fiel             Added defines for Physical Mapping Modes to SAS IO Unit
 *    
*ded C Device    fig pages
             Added defines for Physical Mapping Modes to SAS IO Unit
 *     /_FLAGS_SOFT_ALPA_FALLBACK.
 *    DEVICE_            Added new fields to    Revised bus width definitions in SCSI_POs.
 *  06-30-00  01.00.or 1.0 release.
 *  06-08-00I_DEVICE_1 pages.
 *  06-30-00  01.00.ions for all pages.
 *      equest to suppfines for the PageAddressortIdentifi                   1*  01-29-01  01.01.07  Changed somerotocolress description to spec.
 *  07-27E_FC_PORT_10.
 *  04-29tion
 *  --------  --------     9
 *                      CONFIG_PBBCredi1.01  Original release f1Settin                    CONFIG_PMax PageAdSiz-09-01  01.01.1ded defines for MPI_FCPORTPAGE1_FLAGS_ADISCHardAMS_ING_TRANSFERS,
 *                Removed MPI_SCSIPORTfied RAID Volume Pages.
 S,
 *                      MPI_SCSIDEVPAGFcPhLowestModified defines f *                      MPI_SCSIDEVPAGBootHighgetID,BootBus, and
  *                      MPI_SCSIDEVPAGCurrentTargetIDANSFERS, and
 *  LAGS_PARITY_ENABLE.
 *            d ResynBu
 *  --------  ----LLOWONFIG_ FcPhLowestVersio10-27-04           Added _RESPONSE_ID_MASD_0_FOR_B    w      page  Replaced a reserve04  01.05.05  page and updatedine.
 * ion. *                      to addunique.ble to FC Port Page 1
 *     FO_PARTARGEolumBUS_VALIED
 *             definiti    Added Auto Port PLOGI    fine for  01.00.03  Removed batch                Page 1RLntrolFlags.
 *    -03 01.02le to FC Port Page 1
 *      ROT_I   character                       definiti      Discovery InfFCP Config06-22-00  01.00.03  Removed batch ATA device support to S    IATOe 1 config le length pages (arraATA device support to SRETRY            Added Buc                     DiscoveryGAD *
 *
ion:  01.0(device to SAS Dash Address fsupported device to SAS Dev Pagash FORMess for SAS IOUNIT
 *          15-05  01   page 1 in ControlFields.
 *  01-15-05N 01-Dne f  Added defaults for data           page 1 in ControlFields.
 *  01-15-05ag dTne fo  Added defaults for data Page 1   page 1 in ControlFields.
 *  01-Dr MPon:  01.05 IOUNIT
 *          Nfield to R   page 1 in ControlFields.
 *  01-ag dto RAID Physical Disk
 *       BT  Added n    Page 0.
 *                      AddSHIFUnit Panes for SAS IO Unit Page 1  SAS    page 1 in ControlFields.
 *  01-or MPd new defines for SAS IO Unit Pagd define        Added disable use SATA _0 page.
1.02.04   A(0xFF fiel            Added defines for Physical Mapping Modes to SAS IO Unit
 *          RAID Volum    Page 2.
 *                      Added define for
 *                      MPI_SAS_DEVICE0_FLAGS_PORT_SELECT    _VOL0_PHY redeKrsion                  CONFIG_PAGE_0 spec dated 4/26/2000.
     CONFIG_PAGE_FC_PORT_10.
 *  04-29-04 s fiDiskMapANSFERS, and
 *  Update vers             Added Enclosure/Slot bNuRANSFERS, and
 * 0IOUNIT2_er pages 0 and 1.
            Added _e 0 VolumeStatus
 * 
  RaidVol0e/Slot bthe page version.ld).
 *            page and updat    ges 0 and1.
 _PRIMAield
 *                           definiactiveStatus.
 *  SECOND                Adde Added Ild to SAS Expander pages 0STATUSPage2 CapabilitiesFlags.
 *                     Added Acces     CONFIG_PAGE_FC_PORT_10.
 *  04-29-04 Stat-09-01  01.01.11  Adiginal release for 0.1.05.08  Removed the EEDP flags from IOC Page 1.
Updateage 0 Volumed.
 *                       (Volum-24-05 field).
 * ONFIus             New value fo *      ;field            page 0        *          esonfig pages (5 aactiveStad.
 * o Por_ENuctuED
 *         ed Inactive Volume Member flag  Delay settiQUIESCo BIOS Page 1.
 *00.03  Removed batc  Added Bad Block TabRESYNC_INry IGRES MPI_Fle length pages (ar  Added Bad Block TabVOLUME_INACTIVed the
 *ded BucketsRemainin  Added Bad Block TabBit PLOCK_TucturFUL                         Added Max Sata d.
 AddedIM*          Added define for MPI_CONF                 of SADEGRADo BIOS Page 1.
                      Added Bad Bloc of SAFAIto BIOS Page 1.
 *     me Page 0.
 *                   of SAMISSINed additionaield.
 *    Page 0 PhysDiskStatus fieldETTING                      Added Slot andSettinn
 *  ----     CONFIG_PAGE_FC_PORT_10.
 *  04-29-04 HotSparePod to Siginal re         Add_HOmpi_ARE_POOL_ONFIG_PAGE_FC_PORT_10.
 *  04-29-04 0 spec dated 4/2 Device Page 0.
 *  06-2.
 *    01.05.09  Added EEDP defineIO FlagsOC Page 1.
  in SAS                Added mo in SAS type defines to IOC Page 2.
 *  in SAS                Added Port Enable 2.
 * _WRITE_CACHRatengs to        Added                 Added BarubRateOFFLINE_ON_SMARUnit Page 1
ight (me Page 0.
 *            rubRateAUTO-------URed the
 *     ight ( to RAID Physical Disk
 *rubRatePRIORITYAdded P/*
 *  Copyright (                Added MaxrubRateFASTs for_SCRUBBRate010T_0,ight 20)    obsolete----                  Added MPI_SAfine_METANIT2_FIZed the
 * ight  Date:  July 27,     Added MPI_SA64MB          Added EnclosureHan                 Added DevrubRate512*                      Right 4us field
 *               rubRateUSEry IDUCT     UFFI       ight  *              e 1.
 *  08-30-05  01DEed de MPI_FCPO SAS
 *   8 *    pe defines to IOC Page   mapping.
        , also used in defins field  ot b        Added Port Edded OwnerDevHan            Four ne.
 *                      Addded OwnerDevHan *                           Added Direct Attacdded OwnerDevHanG_PAGE_INBAND_0.
 *  08-ines to RAID Physical Ddded OwnerDevHan      pone SATA Init bit to                 Adddded OwnerDevHan.
 *  06-30-00  01.00.ryStatus                Changed LogEntry d a Fl for Log Page 0.
 *  2000-2008 LSI Co   Changed LogEntry             Added Slot a(0x    age 4.
 *                      Aefine.anufacturing Page 7.
nd RA/*
 * Host code (dr   As,  *  , utiliti foretg coshould leave thi8-03-05  set toSSINone and checknew SAS.pageMMEDIA at runtime.
onfigifntory*        VOL------tatus.
 *  MAX                      Added MaxLBAHigh fild to Bio    endif_FLAGS_SOFT_ALPA_FALLBACK.
 *          4 01.05.03   Added defines for new SAS PHY page      CONFIG_PAGE_FC_PORT_10.
 *  04-29      to RAID Vo0.
 *  01-29-01  01.01.07  Changed som                , and
 *       rgetPortConnectTime,
 *       OCsingDelay
 8-08-01  01.02.01  Original releas       04-09-01 D Vo7               Ad    TYPEndle and EEDP defines to ed NumForceWWI *     *      page and uclosure Page 0.
 *g Page 5.
 *  in SAS Dnes for SAS IO Unit
 *                MaxLBD
 *             Added structure offset comments.r suppreseeld and F       AddedRT_0,
 *               Strip12-07-04  0ed some new defines for the PageAddreslso bumped Page         removed some obsolete ones.
 *OnBusTimerValue  *                      MPI_SCSIDEVPAGNume/Slot b        WED.
 *                      Added lumeScrub*   ;*     vice Page 0 Information.
 *       Resync Notifi.
 *  08-08-01  01.02.01  Original releasenac size       nablrceWW   Added Manufa and 1.
 losure/Slot b[ID Volume Page 0.
 *            ]; updated t FcPhLowestVerUnit Page            Added _RESPONSE_ID_ces Unmappefield).
 *um1.05.05  Added Target MExceeded bits in sical Page 0 InactiveS*                      Flags field  Added ne*   values for defines to IOC PageontrolFlags fi field        Added Port Enab*     .02.04      Page 0.
 *  0.
 *                      Added *     STAtures           Page 0.
 *      tive Volume Member flad Auto OREIGN            Page 0.
 *      me Page 0.
 *         *     INed DeCIENge, SOURC      Page unique.
 *                      CLONE            Page 0.
 *  efines to RAID Physical Disk      Added Disableeld of SAS Device NumRequestedAliasesUnit Page 2.
 REVIOUSLY.h VETTED
 *             6E1_FLAGS_SOFT_ALPA_FALLBACK.
 *          10.
 *                      Added AdditionalControlFlags, MaxTargetPortConnectTime,
 *                      ReportDeviceMissingDelay, and IODeviceMissingDelay
 *                      fields to SAS IO Unit Page 1.
 *  10-11-06  01.05.13  Added NumFo0 spec d0singDelay
  SAS IO Un             fields to SGUID[2          ucture for CONF      Added SATA Link Remake     D and SubsystemID field
 *                  WWto RAID Volume FC Port Page 4RT_0,
 *                      SPhysicalnd removed a onRT_0,
 *                      Stings
 *    NegWirEnclosure Devices Unmap1ed and
 *                      Device L1mit Exceeded bits 1n Status field of SAS IO
 *    1                 Unit Page 21*                      to add a Flags fie.
 *  02-09-05  01.05.07  Added InactiveStatus field to RAID Volume Page 0.
 *         e 4 Flags fiel      Added WWID field to RAID Volume Page 1.
 *                      Added PhysicalPort field to SAS Expander pa and 1.
 0_ERRORs for
 *                      CONFIG     CdbByIG_PAGE_SAS_ENCLOS     CONFIG_PAGE_FC_PORT_10.
 *  04-29     SenseKeyne for IDP bit for  *                      Added Slot       SCSI_DEVICE_0 and SCSI_DEUpdate vers             Added Slot      1.01.01  Original rele0.
 *  01-29-01  01.01.07  Changed som     ASit Page width definitions8-08-01  01.02.01  Original releas-08  01.Q_PAGE_SAS_ENCLOSURE_0.

 *                     Added SlotSmart               Page 0 (_SAT Page 4 ExtFlags field.
 *        ng of01.05.18  Defined new bits lags bits fSSD drives.
 *  ---------------                     to for MPage 0 Vf
 *                                       (

/*******************field).e/Slot b0     lume Page 0, and
 *  ***********
*
*       C    Changed _FREE_RU**************_INOTIAYs for
 *                      CONFIGstructuresIDe 2 and
 *       1.
 *                      Added EnclosureroductIDGE1_CONF_SDTR_D               SSD drives.
 *  -------U8         RevLevel                            Page 2.
 *         2 sizefo          Ad                reCNFG_H


/******************************************************2h */
    U8 **************
*Inquirylumeo n f i g    M e s s a g e  ONFIG_PAGE_S t r u c t u r e s
*
***********ge 2.
 *          E_FC_PORT_10.
 Septo RAID Volume
ypedef struct _CONFIG_PAGE_HEADESep           Added    CapabilitiesE_FC_PORT_10.
    mapping.
 *   for AccessSt           Added OwnerDevHandle and Flags field to Se/Slot bs in SAS DevD Volume Page 0 Vt, MPI_POINTER pCon**********************************ge 0.
 *        derUnion, MPI_POC o n f i g    M e s s a g e ;

typedefDER,
  ConfigPageHeader_t, MPI_POINT.
 *                      New physical mdded define for IDP bit for  CONFIG_PAGE_FC_PORT_10.
 *  04-29-04 01.0ONFIG_PAGE_SAS_ENCLO   Capabilities Flags.
 *                              Added new v Device Page 0.
   PageVersion;   AGE_HEADER_UNION, MPI_POINTER PTR_Cs to IOC PageENDED_PAG*                      Add  U8            OUNIT1_CONTe 4 Flags fiel  U8           fion
                dtus.
 *  Bad Block TabOUT_OF_S IO Unit page 
 *  02-28-07  01.0* 07h */
} CONFIG_EXTEle Full define to RAIDPage 4.
 *         * 07h */
} CONFIG_EXTE    Page        variable length pages (ar* 07h */
} CONFIG_EXTEN IO Un   Page 1Added define for MPI_CONF* 07h */
} CONFIG_EXTENO1-16-0**************             Added d* 07h */
} CONFIGON
 * ed up.
 *                   *****************************mber define for SAS
 * MPI_POINTER PTR_CONFIG_EXTENDED_PAGE_HEA****COMPATIined the
 *     ader_t, MPI_POINTER pConfigExtendedPa.
 *                 cter unique.
 *         * 07h */
} CONFIG      LIZ define for SAS
 *    ******************************** 4.
 *  REQU04-0xpander Page 1
field to CONFIG      (0x20)
#define MP_PAGETYPE_IO_UNIT       CONFIG_PAGE_FC_PxF0)

#define MPI_THERI_CONFIG                  pageMulti-Port Domain bit for Discove**********            Added new fields to the subs
 *                      CONFIG_PAGE_FC_PORT_10.
 *  04-29-04 01.0e/Slot bto RAID Volufor CONFIG_PAGE_SCSI_PORT_0,
 *               e/Slot b           Aay
 *                      fields to S_PAGETYPE_FC_PORTnit Page rsion for
 *     efine MPI_CONFIG_PAGETYPE_FC_DEVICE    us value for to control coeHeaderUnion,
  CONFIG_PgPageHeaderUnion, MPI_POINTER pCre for CONFIG_PAGE_INBAND_0.
 *  08-19-04 G_PAGE_SCSI_DEVICE_0, andefines for MPI_FCPORTPAGE1_FLAGS__RAID_DISAB_PORT Page 1.
e 2 afine for MPI_IOUNITPAGE2_FLAGS_RAID_DIs to
 *   NFIG_PAGETYPE workspec.
 *  07-2    PageType;               2 size_PAGE_HEADpage and updated the p3h */
    U16         figPageHeaderUnion,          TargetConfig andIG_PAGE_INBAND_0.
 *  08-19-04 r supported metaring pages 2 and 3**************************lags field
             G_PAGE_SCSI_Enclosure Devices USCSI_PORT  ed and
 *                      DeviAGETYPE_SAS_h */
    U8    its in Status field of SASE_SAS_EXPANDER sical Page 0 Inactitus.
 * *                      FlagsRTPAGE1_FLAGS_SOFT_ALPA_Fne MPI_CONFIG_ defTH             /* 00h */
    U8          E_FC_PORT                        (0x04)
#define MPI_CONFIG_PAGETYPE_FC_POR           (0x06 *                      Added Slot and)
#define MPI_CONFIG_PAGETYPUpdate version num field values
**********resent bit to
 * SCSI_DEVICE_1 pages.
 *  06-30-00  01.00.  CONFIG_wnerresent bit to
 ETYPE_INBAND                  (0x0B)
#define M
#def  Page 1.
 *             Added MaxNumPhysicalMappedIDits in VolumeSettings
 *        d
 *                      CONFIG_PAGE_SCSI define for IDP bit for18-08-0 03h */
    U16 x13)
#********************************     (0h */
    U8    1PathC o n f i g    M e s s a g e        * 06h */
    U8           C Pa1#defin                      Added Port Edefine MP1 TargetROKE and update Added MPI_IOUNITPAGE1_SA                rolFlags.
 *             DDRESSING.
 *                      Added IOC Page 6.
 *                      Added PrevBootDeviceForm field to CONFIG_orReset settin    s_PAGE_BIOS_2.
 *                  _SCSI_DEVICE_TAh field to RAID VolumeF00)
#define MPI_SC      Added Nvdata version fields to SAS IO Unit **********atus field
 *                   _CONFIG_PAGETYPE_SCSI_DEVICE             (0x04)
#define MPI_CONFIG_PAGETYPI_SCSI_DEVICE_TMINTER pC  (0x05)
#define MPI_CONFIG_PAGETYPE_FC_DEVICE    08)
#define MPI_d
 *                      CONFIG_PAGE_SCSIlso bumped Page Version for
 *                      these pages.
 G_PAGE_SCSI_DEVICE_0, an************************x13)
#PE_LOG       ath1.
 *     F00)
#define MPI_SCdefin  Added FcPhLowestVerARGET_MOD    (0x_IO_UNIT          (0x10)
#define MPI_CONFIG_E16  AddedE_SAS_EXPANDGE6_CAP_FLAGS_MULTIPOR0000000)
#definEXTPAGETYPE_SAS_DEVICE          define.
 *                  .
 *  02-09-05  01.05.07  Added InactiveStatus field to RAID Volume Page 0.
 *    LAN                   Physcial Disk Page 1.
 *  01-15-07  01.05.17  Added additional bit defines for ExtFlags fi------------LAN           Page(0x0_PORT__t for new SAS PHY page addressing mode.
 *                  Added defines xRxM          Added ********************** Flags.
 *                      Added new value for Accfor
 *                      these PacketPrePaags from IOC Page 1.
s for Enclosure Devi8)
#d           Added _RESPONSE_ID_S_SHIF
00000.05.05  Added Target ME_PGAD_BT_ page and updat8)
#ge 2.
 *                      Added m a Flags fi000FF)
#define MPI_FC_RETURN_LOOPB                           toTTR_READ_ONLY ne MPI_FC_SUPPStatYSDISKNUM_MASK          (0                    Mne MPI_FC_SDISKNUM      Two new bits 0000)
#define MPI_ND_PORT_SHIFT            (28)
#atus fielI_FC_DEVICE_PGAD_ND_DID_MASK              (0x00FFFFFF)
#define MPI_FC_DEVICE_PGAD_ND_DID_itiators field to FcPortPage0.
 *  01-29-01  01.01.07  Changed somd Resyn      ONFIG_PAGE_SAS_.05.15  Added Hide Physical Disks with Non-IntPhysicalDisk struct.
 * control coein
 *                   in      2-07-04  01.05.04 eVersion;       in
 *                     FT_HANDLE      (0)
#define MP    removed some obsolete ones.
 * 2, warefor v1.Loworted metadata size bits in
 *                     (16)
#definepabilitiesiesFlags field of IOC Page 6.
 *         MaxWireSpeed MPI_SAS_EXPAAdded defines for metadata size bits in V MPI_SAS_EXPI_SAS_EXPAND_
 *                      field of RAID VoBu    sRemain-01 01.02.02    *             (0x0000FFFF)
#define MPI_SReplyDLE      (0)
#definALLOWED.
 *      some obsolete ones.
 * egI_SAS_EXPAND_PGAD_H_MASK_HANdated the paMPI_SAS_DEVICE_PGAD_FORM_GET_NEXT_H_PGAD_H_SHIFT_HANDL2 Added FcPhLowestVer     FT             (8)
#define MPI_FC_16  AE_PGAD_GE6_CAP_FLAGS_MULT)
#define 000000FF)
#define MPI_F define.
 *                      eld.
 *                    (0x000DEVSlot NuRESOUnit Page 1
 *ONFIG_PAGEATTR_READ_ONLY      (0)
#define MPIOPERA-04 Unit Page 0.
 *Added BIOS Page 4 structure.
 *                      Added MPI_RAID_PHYS_DISK1_PATH_MAX defInbtDev  Page 2.
 *                      Added define for
 *                      MPI_SAS_DEVICE0_FLAGS_PORT_SELECTOR_ATTACH.
 INBAND04 01.05.03   Added defines for new SAS PHY page addressing mode.
 *       I Coion.
 *r datAUnit Pafine MModified defines for Se MPI_FC_DEVICE_PGAD_FORM_BUS_TID     MaximumBufferr_t  Struct;
   U8          Added SepID and SepBus to RVP2 IMPhysicalDisk struct.
 *------- FcPhLowestVerPI_SAS_D           Added _RESPONSE_ID_fine MPI_
F0000000.05.05  Added Target M0x000000FF)
# page and updatPI_SAS_version.
 *                             (0x0F000000)
#define MPI_FC_DEVICE_PGAD_FORM_NEXT_DID            (0x00000SAS IO.
 * PI_SAS_DEVICE_PGAD_BT_TID_SHIFT            (0)
#define MPI_SAS_DEVICE_PGAD_H_HANDLE_MASK           (0x0000FFFI CoS    O_UNIT     s for
 *                    CSI_DEVICE_0 and SCSI_DE CONFIG_PAGE_FC_PORT_1     define for IDP bit for                  BytesPhs in RVP2 Vovice format to BIOS Page 2.
 *      Negoangeed 01.fiers
 *     olume PS IO Unit
 *    ControllerPhy      and ue MPI_FC_DEVICE_PGAD_FORM_              Handl   to_PHY_NUMBER            (0xANDLE_SHIFDev          ----------------RT_0,
 *    SEP bits************        Added F           (28)
#define M           Added _**************************
  SasIO
 *     AGE_HEADER, MPI_POI*****************OUNITSSING.
 *                      Added IOC Page 6.
 *                      Added PrevBootDeviceForm field to CONFIG_PAGE_BIOS_2.
 *                   (28)
#defi field to RAID Veserved;           D_BT_BUS_M Added Nvdata version fields to SAS IO        (28)               (Flags def
 *                Revised bus width definit0x00FFFFFF)
#define MPI_FC_DEVICE_PGAD_ND_DID_ASK      vdataModifieDefaulM_GET_NEXT_HANDLE_PHY_NUMBER            (0x0)
#define M* 04h */
    U8      Podifste  Modified config -----------------------------------_MASK           *  --------  --------  -----_PGAD_INDEMASK                   (0xF0000000)
#define MPI_SCSI_DEV8  Defined new bits        and ***************************************erValue to CONFIG_PA)
#define MP                     (28)
#define M0000001)
#lume1.
 *eserved;                           FcPhLowestVer*/
    U8               Added _RESPONSE_ID_ONFIG, MPI_POI************.05.05  Added Target MConfig_t;


/*** page and updatSASved;  *                      03 01.02ssStatus valueT        (0)OC PageAS_ENCLOSonfig pages (5 aeserved;     
 *
age, redeCOVERYrevious State de                   ***********************0 Config_IOC_NU          PAGEATTR_READ_ONLY ***********************1#define MPI_CONFIG_ACTION_PA               ***********************E_DIS*****------to add a Flags fieldd values
**********************
#defin*****************************HY  Page 1efindefinegExtendedPageHeader_CURRENT        (0x02)
#defi#define MTX                             Added Direct A      (0x04)
#define MR_ACTION_PAGE_READ_NVRAM              (0x03)
#define MPI_CONFIG_ACT    (0x0000FFFF)
#****************************.02.1.02.04   Added fine MPI_CONFIG_PAGEATTR_READ_ONLY  */
    U8       PI_CONFIG_ACTION_PAGE_READ.
 *                     */
    U8       FIG_PAGSPEED_NEGO    AD_FORM_MASK       (0x06)


/* Config Rep     SIT2_OOBfine                       ue.
 *         
    U16         1lags defines for Manufacts.
 *                   
    U16         3ore defines for IOCSettin_DEVID_ defiield.ee mpi_sas.hvalued values
*********************ANDLE_SHIFT        (0)
                (0x03)
#define MPI_CONFIG_ACT*******************************************DSDISKN_DETEC                  Modified CONFIG_PAGE_IO_UNIT_2               UNADDStatuctur10-27-                  to specify the BIOS                MULTIPLrDevR                Sr new Flags bits defined for                EXPANDER    ed up.
 *                  pCon    /* 06h */
    U8       DS_SMP_TIMEOU_PAGE_READ_NVRAM  DS_SUBTRACTIVE_LINK
 *        ***************NDEDROUTDEVITRIE     /* 14h */
} ight (c) 2000-2008 LSI Co***************INDEX
#defEXIS*******************ight (c)  *              *******************FUNCn;  fine MPI_CONFIG_PAGEAight (c) nd R*******/

/********************CRC      nfigReply_t, MPI_POINTER pCo1         /* 01h */
    U8   ****UBTRer_t;

LINSHIFT         (0)

#defiPHYS*****************
*   Manufactfor Di**********/
#define M**************4CTPAGE_VENDORID_LSILOGIC       UNGAD_ORTED            /* 10h ***********8CTPAGE_VENDORID_LSILOGIC       MAX      Config     /* 14h */
} Might (c10FF)
#define MPI_     Header;        ACTIONDOMAhow variabnel */
#define 2 *    GAD_FORM_SHIFT              (28)1#define MPI_SAS_ENCLOS_PGAD_FORM_GET_NEXT_HANDLE        (0x00000000)
#define MPI_SAS_ENCLOS_PGAD_FORM_HANDHANDLE             (0x00000001)
#define MPI_SAS_ENCLOSormat to BIOS Page 2.
 *      MaxMin0FFFF)
#define for RAID Volume PNCLOS_PGAD_GNH_HANDLE_SHIFT        (0)
#efine MPI_FC_DEVICE_PGAD_FORM_MaxcRate            04       (0x0000FFFF)
#define MPI_PI_SAS_PHY_PGAD_FORM_PHY_TBL_INDEX       MPI_MANUFACTPAGE_DEVICEI*************
*   Config Request 0x0032)
#de************1*******************************3)
#defi*********************/
typedef struct _MSG_CONFIG
{
    U8                      Action;                     /* 00h */
    U8                      Reserved;  GE_DEV           /* 01h */
    Udefine MP              ChainOffset;                /* 02h */
    U8   atus field
 *      Function;               /* 03h */
    U16                     ExtPageLength;              /* 04ANDLE_Stion
 *  --------  --------  Type;                /* 06h */
    U8     MaxNum    cRate ng mode in SAS IO Unit Plags bits for supported SATA featuPGAD_dd Two efin66E             (0x005A)   U8                       Added Slot and EnclosuPhysicalDisk struct.   PageAddress;       FIG_PAGETYPE_RAID_VOLUME    Context;                 /* 08h */
    ED                (0x0F)
#define MPI_CO    MaxQDep8-01 01.02.03   Swap        Removed define for
 *              0031)
por_HANDLEMissingDela             RT_FLAGS_PARITY_ENABLE.
 *            0xF00O          BoardName[16];      or MPI_CONFIG_PAGEPAGE_DEVID_1030ZC_53C1035_UNION            PageBufdefine MP                  RAID Volume_SAS1064E    INTER PTR_MSG_CONFIG,
  Config_t, MPI_POIN16  AConfig_t;


/GE6_CAP_FLAGS_MULTngPage0_t, MPI_P**************************** define.
 *        ore AccessStatus value*****************1S1066E       **************************1N_PATROLine.
 * SELF_TE**************AS1078.
 G_1
{
    CONFIG_PAGE_HEADER         3_0*           ng Page 7.
 *CTPAGE_VENDORID_LSILOGIC           VPD[251_5                   /* 0      } CONFIG_PAGE_MANUFACTURING_1, MPI_POSW   PSERotification Su      */
} CONFIG_PAGE_MANUFACTURING_1, MONFIG_A0, MPHAS                  *                E_MANUFACTURING_1, M     defin                     Ad */
} CONFIG_PAGE_MANUFACTURING_1, MPSAS 16 DeviceID;           (efines.
 *     NFIG_PAGE_HEADER      HeviceID;_BO                          /* 01h */
    U8  02h */
    U8 A****ceID;                        /* 02h */
    U8  03h */
} MPI_CHI_POIEVISION_ID, MPI_PO Added Information fEVISION_ID,
  MpiChiPOSTP    ision                    */
} CONFIG_PAGE_MANUFACTURING_1, MPI_PO48BIT_LBA_PAGEIATED
 * ight ******************
*   ManPage1_t, MPI_POI    ader.PageLengt             */
    U8                      VPD[25NCQS_WORDS
#define MPGNED_PHYS_GE_2_HW_SETTINGS_WORDS    (1)
#endif

FUeader.PageLengticeID for FC949E and change


/*
 * Host code (dHYRate fr_ORLY,
HIG        .
 *                 ndef MPI_MAN_PAGE_2_H***********ILLEGUnit Page 0            Added Numndef MPI_MAN_PAGE_2_FIRST_LVERSIOC****eld
 *     Added MPI_IOUNITPAGEndef MPI_MAN_PAGE_2_CLEAR_AFFILion;         0)
#define MPI(0x00)


typedef struct _CONFIG_P MPI_MANUFACTPAGE_DEVIURING_1
{
    CONFIG_PAGE_HEAN_ID
{
  ne MPI_MANUFACTP                 th at runtime.
 */
#ifndef MPI_MAN         isionAed PHROUNO0)
#deIFICon;      I_MAN_PAGE_2_HW_SETTINGS_WORDS            HI AddONZERO_ATTACHnctiHY_IDENilitiPAGE_MANUFACTURING_2
{
    CONFIG         *****REVISIONNLYe this****I_SAS_DEV                      /* 00h */
          PAGETYNUFACTURINGtruct _CONFIG_PAGE                               /*uld leave ;             Header;             HwSettings[MPI_MAN_PAGE_2_HW_SETT         NO             ChipId;           E_MANUFACTURING_2, MPI_POINTER PTR_         ALLOW    (0xTO    (0                    ts field        ypedef struct _CONFIG_P                BoardNamURING_1
{
    CONFIG_PAGE_HEREAddresber de********sion:  01.05.102 01.02.09   Added#define MPI_MANUFACTURING3_anufac            Add_BIT_ADDREx00)


typedef struct _CONFIG_P***********************************1      (0x00)
#define MPI_CONFIG_ACTION_PAGE_READ_CURRENT         (              MPI_CONFIG_ACTION_PAGE_WRITE_CURRENT        (0x02)
#d             FIG_ACTION_PAGE_DEFAULT              (0x03)
#define MPI_CONFIG_ACTION_PAGE_WRITE_NVRAM          (0x0GE_DEVfine MPI_CONFIG_AAGEATTR_MASK                    (0   /* 0Ah */
    U8   G_ACTION_PAGE_READ_NVRAM           (0x06)


/* Config R*/
    U8    */
typedef struct _MSG_CONFIG_REPLY
{
    U8                      EID_FC949E    nfoOffset1;        /* 0Ah */0x06                (0xF0000000)
#ation ANUFACTURING_2
{
    CONFIG  /* 0Eh            ExtPageType;    _BIT_AD     InquiryData[56];    /* 10h *          MsgFlags;        (0x9       ISVolumeSettings;   /IN* 0Eh */
    U8                     0    (0x00)


typedef struct* 4Ch */
/
    U32                     figReply_t;



/**********/* 50h */
 
    U32                          /* 07h */
    U32                     MsgContext; _PAGE_MAN       /* 08h */
    U8     ffset;                /* 02h */
    U8   2                  Function;                       /* 03h */
    U16     CONFIG_PAGE_FC_PORT_10.
 *  04-29-04 01.02.14NumDevsPerEnclosurHANDLE_SHIth;                 /* 01h */
    U8   (0x0031)
#define MPI_MANUFACTPA    field to SAS Device Page 0 and addd2[8];               /* 0Ch */
    C     /* 07h */
CTURING_4
{
    AD_HPN_MASK_PHY        sgFlags; IDS Device Page     (0x0062)


typedef struct _CONFIG          /*          /*Us.
 *  AGE_HEADER      Header;                                         Added MPI_SASED                (0x0F)
#define MPI_CONFIGal mapping mode in SAS IO Unit Removed defi                             Reserved7eset seeld Mapped /* RT_FLAGS_PANUFACTURING_0, MPI_POINTE2INTER PTR_MSG_CONFIG,
  Config_t, MPI_POIN2ER pConfig_t;


/2_POINTER pManufacturingPage0_t2

#define MPI_MANUFACTURING0_P2GEVERSION             ddedAGE_HEADER              Header;2* 6Ch *                Added eserved;  2Bad Bloc10-27-0LIMI****CEEControl Fla        Reserved3;          /)
#defineENCLOSUR            MAPPACTIO_DEFAULT         (0x05)
#defi)
#define NFIG_AC_Pon.
STllingAPP               /* 03h */
    U16  )
#definescov_MODEPAGE8_DISABLE   LT              (0x03)
#define MPI_CONFIG_A20FF00)
#defin                    (0x20)
age, redefine_RESYNC_CACHE_ENABLE   NFIG_RE/*   U8     CTURing T    TA                 (0x01)

/* defi      and MAP    ufacturingPage1_0E       (0x04)
#define MPI_FO_PARU8  PCe MPI_MANPAGE4_EXTFLAGS_     RCION_SIZE       (7)
#define NOANPAGE4_E   Reserved;                   /* 01h */
    U8 
/* defineRECTGE_3_INNPAGE4_EXTFLAGS_1           /* 02h */
    U8  #define _MANPAGE4_SLOMANPAGE4_ENVRAM           (0x06)


/* Config R#define HOST_ASSIGNFO_WORX_SSD_SAS_SATA   Acce_SIZE         (0)
#define MPI_R pManu1 01 ExtF_BOOilities, e*  03-27-06  01.05.#define MPI_MANPAGEA   /RbRateMIX_s for Manufacturing P/* 5Ch */
    U8                          3         (0x0056)
#define MPI_MANUFACTPAGE_DEVID_SAS1066           ions in SCSI_PORT_0,
 *               NFIG_PAGE_MANUFACTURING_0
{
    CONFFIG_PAGETYPE_RAID_PHYSDISK           (0x0AMax       Dw defined here.
 * fines for SAS IO Unit
 *                2 size is fCONFIG_PAGDEVID_53             Added structure offset comments.eatures.unMASKDisparity             Flags field of IOC Page 6.
 *         0031)          /* 04h */
     /* 00hed some new defines for the PageAddresType fieossCONFI1.01hfined here.
 *         removed some obsolete ones.
 *2-05-00 
    U16            /* 00h *   (0xF0000000)
#define MPI_SAS_DEVICE        hy MaxRProblem*                28)
#define MPI_SAS_DEVICE_PGAD_FOR0001)
#                 /* 00h */
000000)
#defANUFACTURING_0, MPI_POINTE3INTER PTR_MSG_CONFIG,
  Config_t, MPI_POIN3ER pConfig_t;


/3_POINTER pManufacturingPage0_t3

#define MPI_MANUFACTURING0_P3GEVERSION                                Added defines for Physical Mapping Modes to SAS IO Unit
 *          T   ExpanderPI_SAS_DEVICE_PGAD_BT_TID_SHIFT            (0)
#define MPI_SAS_DEVICE_PGAD_H_HANDLE_MASK           (0x0000FFFF)
#define MHIP_G_REPLY,
                   Function;                                          IMEResyncRate;      /* 61h */
    U16           s field and
E_MANUFACTURING_          /* 62h */
    U8                              IMDataScrubRate;    /* 64h */
    U8                                    RAD_H_HANDLENDLE_SHIFT         ddress field values
*********** */
  Sfor v1.D_SAS1078              (0x006IG_PAGE_INBAND_0.
 *  08-19-04 01.0***************************       Added                             Reserv_PGAD_H_HANDLEiption to spec.
 *  07-27                             ReservPaORM_HAN                (0Flags bits for supported SATA featuLAGS_RAID_DISAine MPChang Config pag            Rest;

#define MPI_MANUFACTURING6_PAGEV     SlRouteIndex                        Removed MPI_SCSIPORT              /* 00h */
    U8       *                      MPI_SCSIDEVPAG_CONNECTOR_INFO     HIFT              (2[8];            /* 14h */
    U8      ysical mapping mode in SAS IO Unit                      Page 0.
 *    d of RAID Volume Page 0.
 * ded MPI_SAS_IOUNIT2_FLAGS_RESERVER            INTER PTR_MSG_CONFIG,
  Config_t, 000)
#definuringPserved2;


/*********************(0x00020000)
#d******************G_REPLY, *                   .
 *    AGE_HEADER        efine MPI[2];               /* 0LE                          (0x0001       IOCStatus;             * 0Eh */
    U32             _SFF_8470_L3 LogInfo;                 /*/
    CONFIG_PAGE_HEADER     _SFF_8470_L3                   /* 14h */_CONFIG_REPLY, MPI_POINTER PT_SFF_8470_L3 G_REPLY,
  ConfigReply_t, MNTER pConfigReply_t;



/****_SFF_8470_L3 ********************************************************_SFF_8470_L3         C o n f i g u r a t    P a g e s
*
*************_SFF_8470_L3 ***********************************************/

/*****_CONNECTION_UNKNO**************************************************
*_CONNECTION_UNKNOg Config pages
*************************************_CONNECTION_UN*********************/
#dePI_MANUFACTPAGE_VENDORID_LSIL_SFF_8470_L3    (0x1000)
/* Fibre Channedefine MPI_MANUFACTPAGE_DEVIC_SFF_8470_L2            (0x0621)
#defineANUFACTPAGE_D_PINOUT_SFF_8470_L4                _MIX_SAS_SATA                 (   (0x0001efine fONNECTOR_hot           _DEFAULT         (0x05)
 * one and chec    C for Di_PAGE_DEFAUL          /* 03h */
   
 * one and check N----evious State defUS_SHIFT  ING_6
{
    CONFIG_PAGE_HEADER                     (0x0056)
#define MPI_MANUFACTPAGE_DEVID_SAS1066    1.
 *                      Added Enclosure/SlRING_6, MPI_POINTER PTR_CONFIG_PAGE_MANUFACTURING_6,
  Manufac          IMDataScrubRate;    /* 64h */
    U8                      IMResyncRate;       /* 65h */
    U16 Header;                     /* 00h */
    U8         U8                      Reserved2[8];  Ph                PAGE_HEADER      Header;                     /* NumTableEntriesProgramm.
 *AGE_HEADER      Header;                             0FFFF)
#define MPED                (0x0F)
#define MPI_COHw949E            (0x0646)
ge4_t, MPI_POINTER pManufacturingPage4__H_HANDLE_M             RT_FLAGS_PARITY /* 14h */
    U64            and updated                    Added /* 14h */
    U64         _H_HANDLE_MASKand updatecation;               /* 14h */
    U8    
#def        Reserved1;  Added config page structures for BIOS_SAS1t;                               Volume Page 1, and RAID Phys        (0x0000FFFF)
#define MP                     MPI_SCSIDEVPAG pManufa Page 1.
 *         TOR_INFO,
  MpiManPage7ConnectorInfo_t, MP                ProductSnfo_t;

/* defines for the Pinout field */*/
    U32              _8484_L4                 (0x00080000)
#def SEP bitsacturingPage7_tSAS_IOUNIT  (0x0001)


#ifndef MPI_MANPAGE5_NUM_FO                rved4;              (0x00040000)
#defR PTR_CONFIG_PAGE_MANUFACTURING_0,
     Header      (0x00020000)I_POINTER pManufactur          Produ4_L1                 (0x00 define.
 *          NFIG_REPLY
usfine MPI_MPHY        ACTURING_3,
 _MANPAGE7_CONNECTO        TURING_9,
  ManufHWcturingPage9_t, MP_MAX]; /* turingPage9_t;

#define MPHY  VeingPage9_t, MPIufactu        7h */
    U32                                  1            (0x00)

    U8                      RR              HG8_PAGEVERSIOdefine MPI_MANPAGE7_PINOUT_SFF_8471h */
pedefts dPI_CONFIG_ACTION_/
#ifndef MPI_MANPAGE7_CONNECTONUFACTURING_CONF Delay CHANG    CONNECTOR_INFO_MAX   (1)
#endifNUFACTURINGNOMAX
#yncRatn f i g NFIG_REPLY
{
    U8       R              HAction;            icInfo;/* 04h */
} CONFIG_PAGE_MANUNEG                     Reserv            /* 01h */
 *******************PI_CONFIG_ACTION_PAGE           /* 02h */
  *******************FIG_PAGFunction;     TER pManufacturingPage10_t;

#def*********            ExtPageLe         /* 04h */
          Header;                 ExtPageTyp        Reserved3;           Header;                MsgFlags;         RANUFACTURING5_PAGEVERSION                  (0x02)

/* defines for the Flags field */
#d         Page 2.
 *                      Added define for
 *                      MPI_SAS_DEVICE0_FLAGS_PORT_SELECTOR_ATTACH.
 HIP_10-27-04 01.05.03   Ad        /* 00h */
    U32                             ProductSpecifi                             ReservSloM_GET_NEXT_HANDLE    (0x0on;               /* 14h */
    U8            VERSION                  (0x00)


typedef struct _MPI_MANPAGE7_CONNECTOR_INFO
{
    U32                         P       /* 14h */
    U8                          Reserved1;    CONFIG_PAGE_SCSI_PORT_0,
 *                   Phyus value for RAI_DEVICE_TARGET_ defines for the Pinout field */
#defineAccess***************_INFO  control coercion size and the mixi/
    U8                          Location;        E_FC_PORT_10.
 *  04-29-04 01.02.14cRate to RAID Volume
d1;              /E_FC_PORT_10.
 *  04-29-04 01.02.14           Added MPI_ value for MPI_MANinout;                 /* 00h */
     (0x00)

/* de                    Res                             Reserv MPI_MANPAGE7_PINOUT_SFF_848ductSpecificInfo;/* 04h */
} CONFIG_PAGE_MANUFACTURING_6, MPI_POINTER PTR84_L4                 (0x00080000)
#define MPI_MANPAGE7ncRate;       /* 65                 (0x00040D_0_FOR_BOOT
 *                      dh */
    U8  uringP04  01.05.05  Added Target M   /* 01h */
   ******************10-27- *                            _PINOUT_SFF_8470_L4       OC PageUNITPAGE1_SI**************************    /* 0A(0x10)
#d           /* 14h */
} MSG_CONFON_SIZE         (0pterInfo_t;

#deBIOS, utifine MPI_CONFIG_PAG           /* 02h */
    (0x0001)
#define MPCAPABIL to ine MPI_CONF          /* 03h */
     (0x0001)
#define MP          Hef stLIC             /* 04h */
      (0x0001)
#define MPNEEne MPI_CONFIUNIT_0
{
   4ne MPring
 *               TA  cha f1.03  ASK_COERCION_SIZE      (0x0001)
#definIF                Reserved; (0x0008)
#define MPI_M/
    MPI_ADAPTER_IN                  /* 004];  TATUS              (0x0002)

typedefIF_DIAefine MPI_CONFIG_PAGEATT1{
    CONFIG_PAGE_HEADER      HeaderIFDS
#de one and checTR_CONFIG_
    U32                     Flags; IF_CHECK_POW               FIG_oSize1;          /*PAGEVERSION         PIO_S*                  FIG_field to CONFIG_FLAGS_PAUSE_ON_ERROR   MDMA      (0x00000002)
#FIG_              r8h */
    MPI_ADAPTER_INF          (0x00000004)
#def               _FLAGS_PAUSE_ON_ERROR   ZONNG3_VIOLURING_2,
  MaFIG_                   PAGEVERSION         ****gInfo;     ge2_t;

#defi                   LAGS_VERBOSE_ENABLE                   /            I_POINTER PTR_MPI_ADAPTER_INFO,
 _MIX_SAS_SATA                 (    /* 0efine Mthis define et to
 * oeld
 *  */
 EDDED                 (0x000define MPI_INTER pManufacturingPagNIT_2
{DISPLAY       (0x00000040)


/*
 *           (0x0621)
#define MPc.) should leave this def00040)


/*
 * Host check Hea          Manufactur        ISVolumeSett00040)


/*
 * Host TINGS_GPIO_VAL_MAX
#e MPI_MAN_PAGE_2_HW_SETTINGS_00040)


/*
 * Hosttypedif

typedef structne MPI_MAN_PAGE_3_INFO_WORD00040)


/*
 * Host          Header;                        /* 0Ch */
    U32 Page 1  mpiELmPhys E_3_INnit page 2.
 *                       /* 00h */
  ISABLE _MODEPAGE8_ *                      Added Num      /* 00h */
  10-27-0                            t{
    CONFIG_PAGE_HEADER                ER pM */
    U16   ONFIG_PAGE_MANUFA7h */
    U32                    DAPTER_INFO,
  Reserved5;          /* 5Ch */
    U8                  10-27-0          (0x0056)
#define MPI_MANUF_CONFIG_PAGETYPE_SCSI_DEVICEE               (0x0001)


#ifndef MPI_MANPAGE */
    U32                         page and updated the page
 *      _CONNECTOR_INFO
{
    U32                         Pinout;                 /* 00h */
  ciBusNumber;                  Connector[16];          /* 04h */
    U8                          Location;        x00000004)
#define MPI_IOUNITPAGE1_DISABLE_QUEUE_FULL_HANDLING     (0x00000020)
#define MPI_IOUNITPAGE1_DISABLE_IR                      (0x00000040)E_FC_PORT_10.
 *  04-29-04 01.02.14 changlReg      FIS[20definber;     INTER pIOUnitPage3_t;

#     PciDeviceAndFunctionNumber;                 04  01.05.I_POINTER pManufactur4_t, MPI_POIN                      /* define.
 *                      Removed impedance flash frotPage3_t;

    IMEDataScrubRate;   /* 60h */
    U8                              IMEResyncRateef struct _MPI_MANPAGE7_CONNECTOR_Is field      ProductSpecucture for CONFIG_PAGE_INBAND_0.
 *  08-19-04 01.0        R4_EXTFL      Added    /* 1Ch */
} MSG_CONFI********     PciDeviceAndFunctionNumber;         PI_MANPA04  01.05.BAD_BLOCK_TABLE      h */
    U32                       /*_OFFLINE_FAILOVER  d a Flags fieldCTURING_3,
  ManDAPTER_INFO,2EADER      Header************************** 08h */ENre p *  igh URING3_COU */
    Udefined fo    (0x00)


typed;               U8  PC* 0Eh */
    U8    (DDED              ;                   /NUME_DIS            ight (c)7         InquiryDat                 /* 10  /* 11h */
    U32(            Reserv;                   /SAS_C MPI_M               FFAGE_DEVICEID_FC919                   /* 10     /* 18h */
    (    
  IOUnitPage0_t, MPI_POINTER pIOUnitPage0_t;

#define MPI_IOUNITPAGE0_PAGEVERSION      PHY         (0x00)


typedef struct _CONFIG_PAGE_IO_UNIT_1
{
    CONFIG_PAGE_HEADER      Header;                     /* 00h */
 WORD                    Flags;                      /* 04h */
} CONFIG_PAGE_IO_UNIT_1, MPI_POINTER PTR_CONFIG_PAGE_IO_UNITANPAGE7_FLAG_USE_SLOT_INFO INTER pIOUnitPage1_t;

#define MPI_IOUNITPAGE1_          IMDataScrubRate;      (0x02)

/* IO Unit Page 1 Flags defines */
#define MPI_IOUNITPAGE1_MULTI_FUNCTION                  (0x00000000)
#defNFIG_PAGE_MANUFACTURING_7,
 CONFIG_PAGE_SCSI_PORT_0,
 *                   8, MPI_POINTER PTR_CONFIG_PG                   (0x00000002)
#define MPI_IOITPAGE3_GPIO_SETTING_ON     )
#define MPI_SAS_EXPAND_PGAD_GNH_RSION                  (0x00)

/* defines for the F             (0x00000001)
#define MPMANPAGE7_CONNECTOR_INFO Con8                      Reserved2[8];      INFO_MAX]; /* 24h */
} CONFIG_PA       and MPI_FCPORTPAGE1_FLAGS_I)


typedef struct _CONFIG_PAGE_MANUFACTURING_8
{
    CONFIG_PAGE_HEADER      _MANUFACTURING_4,
  ManufacturingPag           nes for the Flags field */
#define nufacturingPage7_t;

#defin0)
#def CONFIG_PAGE_IO_UNer;       PciDeviceAndFunctionNumber;  PAGE1_uringPPhy */
    U16     AdapterFlypedef str******************Manufaersion.
 *             NFIG_REPLY
{
    U8       ne M********_POINTER pManufactLE                       Manufacturi  /* 0Eh */
    U8                               InquiryData/* 01h */
    U8    ****ous SAMM                EMBEDDED               /* 01h */
    U8               ExtPageType;                ISVolumeSett/* 01h */
    U8              MsgFlags;                   IMEVolumeSett/* 01h */
   4Ch */
    U32                              IMVolumeSettin                                     VolumePageNumber;       /* 03h */
    U8      50h */
    U32                      04h */
} CONFIG_PAGE_IOIoc2RaidVol_t, MPI_P       Flags;                      /* 0    /* 00h */
    U8      GEVERSION   VolumeBus;              /* 01PI_MANU  /* 0Eh */
    U8                              InquiryDatane MPI_RAID_VOL_TYPE_/
    U32                             ISVolumeSettne MPI_RAID_VOL_TYPE_
    U32                             IMEVolumeSettne MPI_RAID_V 4Ch */
    U32                             IMVolumeSettin           (0x04)
#de   U32                             Reserved3;                (0x04)
#de  U32                             Rese    /* 00h */
    U8      _MIX_SAS_SATA                 (Manufefine MG    E4_EXTFLAGS_NOEIO Unit page 2D;               /* 00h */
    U8       G_PAGE_ VolumeBus;              /* 01htypedef MPI_* 04hus Notification SuppoPI_MANUFACT4h */
} CONFIG_PAGE_MA* one and check Header. */
    ASK          (0x000000FFFACTURING_1,
  Manufac* one and cheVIRTU****H              Added PAGE_DEVICEID_FC92#endif

typedef struct _CONFIGdefineS_CA****ATHWAY*****  01.05.18
 *
 *  Versi_IOC_PAGE_2_RAID_VOLUME_MAX
#dPI_MAN* 00h */
    U32            (         Added d                      FACTURINATTRIBUPageLength;    ight (c) ;              /* 02h */
 typedefE4_EXTFFACTURIonfigReply_t, MPI_POINTER pConf  CapabilitiesFlags;             ***********            /* 09h */
 *****************************lumes;       for Di            /* 09h */
    U8LINK.
 *       der;                              _CONFIAPageLength;     U8                      RevisionID;MaxPhysDisks; Asynchro_PAGE_2_RAID_VOLUME_MAX];/*                          NumActivePhysDis              MsgLength;          /* 0Eh */
    U32             lumes;       Function;  fine MPI_CONFIG_PAGEATTR/
    CONFIG_PAGE_HEADER     * one and check He       ExtPageLength;          Added new Flags field to MPI_POINTER PTR_                ExtPageType;       NTER pConfigReply_t;



/****2)
#define MPI_IOC          MsgFlags;           defined fo_0,
     CONFIG_PAGE_HEADER      Header;           (0x0056)
#define MPI_MANUFACTPAGE_DEVID_SAS1066                   (0x0001)


#ifndef MPI_MANPAGE5_NUM_FORCEWWID
#define MPI_ucture for CONFIG_PAGE_INBAND_0.
 *  08-19truct _CONFIG_PAGE_MANUFACTU              Pinout;                 /* 0           /* 04h */
    U8     U64                             BaseWWh */
    U16                                            Flags;                                 Resspec.
 *  )

#define MPI_IOCPAGEPI_POINTER PTR_CONFIG_PAGE_IO_UNIT_0000)
F)


typedef I_POINTER pManufactur          VOL
{
    U8          define.
 *               ne MPI_MANUFACTURING5_PAGEVERSION                  (0x02)

/* defines for the Flags field */
#de       RI_MANPAGE5_TWO_WWID_PER_PHY                   (0x01)


typedef struct _CONFIG_PAGE_MANUFACTURING_6
{
    CONFIG_PAGE_HEADER   MANPAGE4_                    Flags;                      /* 04h */
} CONFIG_PAGE_IO_UNIT_1,             (0x00)
#define MPI_IOUNITPAGE3_
#define MPI_IOUNITPAGE3_GPIO_FUNCTION_SHIFT             (2)
#defi* 02h */
Log*****o RAID _MULTI_FUNCTION                  (0x00000000)
#defTURING_4,
  ManufacturingPag  Connector[16];          /* 04h */
    U8    VERSION                  (0G                                        Reservx005lone MPI_MANUFACTPAGcation;               /* 14h */
    U8            Start_1,
  IOUnitPage1_tING     (0x00000020)
#define MPI_IOUNITPAGE1_DISABervedcRate to RAID VolumFO                 (0x00000001)


typedef             R           Added MP              (0x01000000)
#define MPI_IOCPAGE1_EESEPcRate to RAID Volume
 * h */
} MPI_MANPAGE7_CONNECTOR_INFO, MPI_POINTERSEP           Added MPI_SAS (0x00000010)
#define MPI_IOCPAGE1_REPLY_COALESciBusNumber;                , etc.) should leave this define set to
 * one and cheI_POINTER pManufactuFIG_PAGE_HEADER          ysDisk_t, r;             /* 00h */
    U32   PBus;            (       R000)
#define MPI_MANPAGE7_ Reserved;     4_L1               MANPAGE4                 VolRSION                 (0x00 02h */
  etc.) should leave this define set to
NCLSOLUME_INAEP  Addo
 *Flags.
 *  PAGE_MANUFACTURING_2
{
  eave this defi    /set to
 * one andCount;   der.PageLength at runtime.
 */M             /* 06h */
    U    (0x00)


typedeAGE_4_SEP_MAX                   Rese00000FF)
#define MPI_NFIG_PAGE_IOC_4
{
   MPI_S i g u r a t i                    M                     /* 00CTIV /* 06h */
    U16                 AGE_4_SEP_MAX    EXP          /* 04h */
      U64             GE_4_SEP_MAX    SE           U16                     Reserve                 /* 0        /* 04h        MPI          (0x0F000000)
#define MPI_FC_DEVICE_PGAD_FORM_NEXT_DID            (0x000000ogPI_SAS_DEVICE_PGAD_BT_TID_SHIFT            (0)
#define MPI_SAS_DEVICE_PGAD_H_HANDLE_MASK    ******************/
typedef struct _MSG_CONFIG
{
    U8                      Action;              NumLog       _PAGE_BIOS_2.
 *             LOG_0* 14h    G10_PAG
#define MPI_P       Reserved;                 ChainOf          /* 01h * 01hNIT2_LENG          FIG_CMPI_SAS_EXPAND_PGAD          G10_Yrsion   RT_0,
 *   04  Atamt device forma width definitions in SCSI_PORT_0,
 *   _PAGE_MANUFACTURING_0
{
    CONFIG_PAGE_  Connector[16];      LogSeque     to controitPage1_t, MPI_POINTER pIOUnitPage1_t;

     /*yQuale 1.
 *                     EnclosureName[16];    Log                    /* 02h */
      *************s;         *************
*   Confr.PageLengthion,piLog0(0x01the page version.E_5_HOT_SPARE_OUNIT1tatus valueOC_4OC Page   (0x01    (0x01)

/*
 * leave this define set s;         _QE_IO_HEADEUNUSruct _CONFIG_PAGE_FF)
#define MPI_P_PAGE_HEADER             _SAS_DEVICE_PGdefine MPI_SAS_EXPAND_PGAD_FORM_SHIFT      6_SUPPORT           (0x00000010)
#define MPI_IOCPAGE2_CAP_FLAGS_RAID_10_SUPPORT          (0x00000020)
#define MPI_IOCPAGE2_CAP_FLAGS_RAID_50_SUPPORT          (0x00000040)
#defciBusNumber;            AGE1_MULTI_FUNCTION                  (0x000000        /* 00                   IO_UNIT_1, MPI_POINTER PTR_CONFIG_PAGEOnBusTimerValue to CONFIG_PAGE_FLAGS_PARITONFIG_PAGE_HEAD00000)
#define  (0x01ave this de  Reserved;           FIG_PAGE_HEADER         UFT             (8)
#define MPI_    (002)o_DEVIBT_TID_MASK        6
{
    C000000FF)
#defin                    VolumeID;MANUFACTURIded Nvda