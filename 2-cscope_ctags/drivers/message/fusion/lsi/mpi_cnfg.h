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
#define MPI_FCPORTPAGE4_orat_BIOS_OS_INIT_HBA
 *  Copyrrigh (c)  3000-2008 LSI Corp*
 *ion.
 
 * 
REMOVABLE_MEDIme:  mpi_cnffg.h
 *   C      Title:  MPI Config messageSPINUP_DELAY_MASK Pages *  CoreonfiF00)


typedef struct _CONFIG_fig _FCessage5_ALIA    FO
{
 *  U8*
 *  Flags;*
 *  Copyrr  -------- -------- ------/* 00h */ion   DescripAliasAlpa*  C-------  --------- ----------------101  Origina6-----Reserved--------
 *  05-08-00  00.10.01  Origin-201.00.01  6401.00.01  WWNN-------- *  C05-08-00  00.10.01  Origin4pdate vers----number forP1.0 release mes  06 06-08-001.00.02  AddC-----}-------------messag  D_PAGghestVe,
 e:  MPOINTER PTR-------------sion, FcPhHighestVerRSIOFcPort 01.5-----Info_t,n, 0 spec d2
pICE_0 page, ud _PAd th;on Historyn, Fc-00  01.00.01  sion, FcPERSIONE_RUNNING_CLOHEADER------
 *  05-08-0HeaderFREE_RUNNING_CLOCK nd SCSI_DEVICE_1 sion, FcPhHighestVernd SCSI_D    FREE_RU    ed _PA FcPhLowestVVERSIO,verse     GEVERSION*ghestVPONSE_ID_MASK def_DEV
 *        th           Add.
n to SCSI_PO;
    Title:  MPI Config , updaVERSION --------  ----------g.h
2)     and                  FLAGS_ALPA_ACQUIREDPONSE_ID_MASK Add1     Title:  MPI Config INFO_PARHARDRAMS_SPONSE_ID_MASK defthe pa Remov2008 infig        ition to SCor 1 *  06-22-00  01.00.04  Remoc dabatch controls from LAN      Pormation field an, Fc 8                  page version.
DISuctuESPONSE_ID_MASK def Ad1ERSIChanged _FREE_RUNNING_CLOCK to _PACI6G_TRANSFERS inAdded            _orat_0,rols fro and SCSI_DEVICE_1 p SCSI_P32 --------  ----------0 spec       4/26/2000ages.
 the page GEion.
 * de
 * 30          TimeSince    t--------
 *  05-088
 * 1     sages.
 * 0-00  01.00..0xFram Added-------  -------/* al release -dated the page
 *      RPONSE_ID_MASK d         ed _RES Information field and _I           Word         version.
 *     2PONSE_ID_MASK defM     FCfrom LAN_0Pd anSubsystemVendorIDormatdths.
 *  07-27-00  01.00.05  CorrecteLipCounReplySize    LAge and up3ystemID field
 *                  Nos      Ad.02  Addalor all p fo.
 *  07-27-00  01.00.05  CorrecteErrorageAddress description /* 4ystemID field
 *                  Dump3  Removeess description FC  field and _INcription to s11-02-08-nkFailure   Added Manufact/* 5ystemID field
 *                  LossOfSync   Added Manufactues ts to match proposals
 *  12-04-00        ignal   Added Manufac/* 6ystemID field
 *                  PrimativeSeqErr   Added Manudredated the page version.
 *  11-02-0Invalideld an   Added Manufa/* 7ystemID field
 *                   ze is Cr1.04*   dified   pfig 7.
 *  07-27-00  01.00.05  CorrecteFcpItch atorIo   Added Manuf/* 808-00  ersions.
 *          ss formats.
the pagRESPONSE_IDsion: def6tch co        6  Rev1                 0 anss f Information field and _6scription to spec*  06-22-00  01.00.0ERSIONhanged _FREE_RUNNING_CLOCK to _PACI7       cleaned up.
 *             Revis    us widthed batch cos in        Reviseaned up.
 *             ols from LAN_0 and ls from LANand up-------p.
 *            ortSymbolicName[256]ion field a InforCSI Port Page 2 Device            dified.
 *                 7    New fields7added to FC Port Page 0 and     Information field and _        cleaneatio
 *                      Add      i5-00ance flashersionFC Por8 Page 1.
 *                      Added FC Port pages 6 and 7.
 *  01-25-01  01.01.06  Added MaxInBitVector[8em 32 ch              page versions.
 *          8       Added some LinkType defines for8    New fields8added to FC Port Page 0 and
 *  Information field and _
 *  03-01.01.09  Replaced MPI_CONFIG_PAGETYPE_SCSI_LUN with
 *                9 Page 1.
 *                      Added FC Port pages 6 and 7.
 *  01-25-01  01.01.06  Added MaxInitiators field to FcPortPage0.
 *  01-29-01dated the page
 *      Global7.
 *for allI_FCPORTPAN1-29-01  07-27-00  01.00.05  Correctedd _RESPOs for all pages.
 *              Ad 01.01.06  Added MaxInUnitTyperess d------  New fi    ages.
 7-27ucture offset comments.Physicale deNumbevised bus wVIC1ed somwed bate      d an 01.    esNumAttachedNo0  01.01.02EVERdths in IID fie    0.1o.
 * te on 2 sizP*              version.
 *dthsVICE_1 pageeaned up.
 *           UDPan SubsyMaxInitiatorscsi    t6    -29-ed M   Adde7  Changed somIPes.
 *s[1h2 and
 *to make them compatibleeaned up.
 *                  S1doved some obses.
 *  , IO        eaned up.
 *           TopologyDiscoverynfig        to         AdAID Volumete on 2.9       Added some LinkType defines for9    New fields9added to FC Port Page 0 and    to make them compatible       Mo01.01.09  Replaced MPI_CONFIG_PAGETYPE_SCSI_LUN with
 *                10_BASE_SFP_DATAG_TRANS--------Manufacturing pagSCSI_DEVICE_0 and SCS *  6-p.
 *          pabilitiestion
n to speExt MPI_CONFIG_PAGETYPE_SCSI_   turing pageUNITPAGE2_FLAGS_RAID_DIConnCte ohysicalDisk struct.
 *1Ud _PAGEVERUNITPAGE2_FLAGS_RAID_DITransceive     fied RAID VolRT_F3    I CoIOUNItion.2_FO_PAR
 * _DISncoding1.02  Interim ch to specB-08-01  01.02.01  Original releasBitRate_100mb         versiodded defrE_SCSIUNITPAGE2_FLAGS_RAID_DI RVP2 IMs field Disky
 *  -ed v1D-08-01  01.02.01  Original releasLength9u_kmddressor        , FC1Edded drmatI Corporation.1RSISTENIMes, ATE
 *  0       version    V             Removed MPI_SCSIPORTPAGE0_50AP_PCIN version.
 .01.0Unit Page 3.   Removed MPI_SCSIPORTPAGE0_62p5_TRANSFERS, and aed deE_SCSI_2008 LSforeaned up.
 *     PAGE0_Copper_NG_TRANSFERS, an*  O_PARPARITY_ENuctuleaned up.
 *       v    SsystemEPLY.
 *       06-22-00  01.00.03  ved some obsMPIOC_0 p make workleaned up.
 EPLY.
 *   ed va   Removed MPI_SCSIPOR_0 pRAMS3_ONLY
 *               EDleaned up.
 *                 pagage0DOUI[3ed define for MSCSI 3525-01  01I_PORT_1.
 *                  PN2E1_CONF_SDTR_DISA leaned up.
 *   _PORT_1.
 *                  Rev[4ected bits to SCSI Dig po m     eaned up.
 *           WavelAGE0_ed _RESPONSE_ID_MASK 4        and MPI             Added OnBusTim4hysicalDisk struct.
 * Port Page 4, FPE_SCSI_Low age001  COWED
 ths in IOC_0 page and up Port Paersions.
 *          ID Volume PagIOC      ge versioned _RESPONSE_ID_MASK defr Manufactur-01      IOC Page 10BaseSfpDataadded to FC Port Page 0 and,.
 *          oper structure.
 *    facturinD_UNKNOWormation 10-04-0s*         and SCS10-04 01.01GBICed _RESPONSE_ID_MASK defd batch contETYPE_RAID_FIXTED
 *          3                  paETYPE_RAID_SFP*  -------- g.h
3PAGEtwork.

 *                    _MI             up.
 *             p define 4 adapterAX         to7F     ch pr-------_1 pTYPENT.
 *VEND_SPE Page: (0x8ERSIthI_FCPO (arrays) arand
fiEXT02.012d at ed s           Added generic defines  v    MODDEF1ed _RESPONSE_ID_MASK def4 adaptersome -01 01.02IOU 01.01moved .
 *                      Mo06   Added 3cter uniqupages.
GE1_DISABLE_IR.
 *  0 for MSEEPROMariable lMEDIA
 *                     long with
 *5CISlotNum5fied2008 L  Remov5-304-0lume typ         *    g.h
6ield to CONFIG_PAGE_IOC_2_RAID_VOL in a
7       re7ield to CONFIG_PAGE_IOC_2_RAID_VOLVNDSPre poe vermat
 * 
 *                     CONNme types.
 * stor-01-01 01.02.05   Added defineRSISTESDISKleaned up.
 *             03-14-02 01.02RSISTCOPPERTR_RO_lags e 3.ldmoved generic d_IOC_1  define      01.01.                related define, aRSISTBNC_TNconfig  version define.
 *  05-31-02 01.0  AddedXIALC-01  01.
ases, MaxHardAliases, and2dded newFIBERJAC:.02.                  P spec dabyte aRSISTLconfig page: CONFIG_PAGE_IOC_5.              RSISTMT_RJ  Added someBucketsRemain-01 inaldAliases,Fr anU-22-00  01.00.09ed _RESPONSE_ID_MASK def      new G               A Adds and s    :xHardAliases,MANUFPT_PIGT and NumRB_SCSIDEVPAGE0_NP_ defines.
 *    RSV1d how va uniqC      morLSI Co AddeEV MPI_SRP_          *                     generic         RSISTHSSDC_Ihow NABLE_0 a   Title:  MPI Con         AltCoP PageAddress d2dAliases, and5leaned up.
 *       RSV2PORTPAGE CONFiases, MaxHard-----d MPrmatNumCur            Por        MPI_SCSIDEVPAGE1_CONF_EXTENDE     MPI_SCS                MPI_RAIDVOL0_STATUS NCODE_UNspar *  12-04- deflume typto m            FIG_PAG8B10B page: CONFIG_PAGE_IOC_5.
 *           nd NumR4Bto matchMaxAliases, MaxHardAliases, and NulIG_PAGNRZara                     related define, an      MANCHESsion versio-TEFLAGS_cross channel       MPI_SCSIDEVPEXTENDEadapterIOC 01.2 CaUNITPAGE2_FLAGS_RAID_DIO1.11 s[2ected bits to SCSn    oved a onMPIUFACtion.     D_53C103DEVICE_3MaxPage 0 Information.
 *             Added defines for MPIGE5RSISTptersicalDisk struct     generic dATTR_RO_Pon.
STENTed valAdded S10-04-01 ncr         e o5CSI_PORT_1.
 *                    PhHiCodedded define for M  AGE0_R_U    TATIC_VOLUM    d to CONFIG_PiagMonito PagePE_R  01.02.0.
 *NegWireSpeedLow and NegWireSpeedLoEnhith
des.
 * SFERS, and
 Subs       A       Removed MPI_SCSIPORTSFF8472CompliG_PA01 0to maks.
 *dAliases,.
 *1          preferred 64- vol-------
 *  05-08-00                    
 *  -ur   and Mtion
          Ad     4         
 *                      Pa1 01.0In_ defines.
 *  additional Fxten                0gs define   Added MPI_PORTLPAleaned up.s fior MPI_CONFIG_PAGETY voluringN1_RATESE    ned defines for MPInes for MPI_1-16-04 01TXrTIVE.
   Removeanged.Device04  outONFIG changed.
 PFAUL
 *  -12OC_2_RAID_V
 *          nd Initiatordefin VERT NumRed _RESPONSE_ID_MASK defdded define for
 *            I.11   Added RR_TOV field and additional version.
 *                   nitiators fiators fie  Revised bus width definitions in SCSI_PPage    Remov401  0atore ty1ME_INACT in RVP2 Vo08-00  00   Remov-01  01.02.01  Original release             Added OnBusTimA_ONLYdded dr ID    PI_CONFdded define for
 *                    CONFIG_PAGE_SPI_S----_WDTN_1 p 06-01.02.ucture offset comments.I_FCPORTPAGE05--04-4HwC and ls from LAN_FLAG1.02 Addved some obsolete ones.
 *ages.
 *  05-11-04 01.023.01edPI_CONModifieddefines foaced Reserved1 field
 *e 3        MeiaS_RAID_as    CONFIG_01.1    CONFIG_PAGE_aced4, IO Uni1iases,dded define for
 *   *  0ABEVICEAGE0_NR_TOV              Ad *     T_10.
 *  04-29-04 01.02.14   Addedd02.142 pecific[3ew e6-02 01.02.09   Add               02 01.02.09   Add.
 *                 1ize iswith redeCH                  Added MPI_Fproper structure.
 *       10       cleaned up.
 *                   /* standard Added  pin.
 *8 i      (rsionPHYSDed so)r IDliases,MANded dURTRAN4
 *DEVICEAdded e pool01.025.185.18
 *
 *  n   AddIDEVPAGE0_NP_ dedded define for
 * ases
 *                 ed fo               ModifiPage      MPI_FCPORTPAGE1Mwofor Cbit In CONFNFIGmake them 32
 *      ge ve and SepBus to R_PAGE_IOC_5.
 *      ------ength pages (arrays) prop-15-02 01.02.10  Ofactunfig page:      to spr           ge 1ption
iases,Added define s anIEEE_OWED
 *  ied CONFIG_PAGE_IO_E_FC_PORT_1.02 01.02.09   AddEEDP                      to spases, MaxHardAliases,WED.
 *       Page 4,6-0CAL_LW Fourfor C                to spPage 4.
 *     ne for
 *    -00 d and _IN          to sp            related dREPLYge heaed definesW     related define, Added sify      *   boot d     Added define f_LX     ce.
 *     w Flags biiases, a_TTR__2    a     re for Cases,s
_hree n      S    ved some ob           MPI_RAIDVOLREPLY_DISABLE64-bit _O  T 1 config page.
 *    ed _RESPONSE_ID_MASK d_PAGE_SCSI_PO
 *  1-1ASGEATTR_0_DS_SUfg.h
 *   _0 aasesED
 *                      and s field  Mapp-01 Mod      SASEVICE_0, replac
*    C       1 aloPAGE0_Ns
            Added defines for Physical Mapping Modes to SAS IO Unit
 *      /RSISTENSOFTRAMS_NFALLBACig page: ition t                   Add            Added FC Port pages 6 and 7.
 *  01-25-01dated the page
 *      age  for all pages.
 *      ues for MPI_FCPORTPAGE0_FLAGS_ATTACH_
                  defi  and rquest     uppd some obsolete ones.
 *sortIdenti length pages (arrays1CONFIG_PAGE_SCSI_PORT_0,
 *       erotocol09-01  01.01.11  Added some new defed new extended config  define for IDP bit        Added9dded define for
 *       es for SBBCredidded Manufacturing pages1Settiuest tS Device Page 0 and addMaxte onesSizes.
 *                     and MPIRemoved MPI_SCSIPOR      PORTM    ANSFERS, and
 * I_CONFIG_PAGETYPE_SCSI_h preferred apter
 *             S_RA             Volume PIOC_5.
  Added definersions.
 4 adapterUBTRACTIV              Replaced IO Unit Page 1 BootHighgetID,ith Bu_FLAGS
rsion 1.0.     Replaced IO Unit Page 1 fineentTarrved PortFlags define                      an alias.
 * a newynBu         version.
 LLOW------   Added structur10-27   M      Added some LinkType defineD_0_FOR_B     in CONFIG_P 1.01ed a a_PORT_1.R_BOwo newONFIto make them com CONFIG_m cha       related define, e 0
 *       e veto        *      lags defDEVICETARGE    BUS_VALInew defines Removed batch           AutoALLOWEDLOGctureE.
 *            make them 32
 *  xtended config
 *   1RL deviFLAGS_RAID_DI-0301.02.                  Added Auto ROT_cturch more FlSK.
 *                      Monfig paSEP bits InfFCP    fig06-200  0                      AddATAes.
 * AS IO    o SA-16-0ATO    s and s version define.
 *   ControlFlags.
 *      RETRYxpander, SAS DevicBuor Manufacturiconfig paSEP bitsGADessage     Two n(trolFlao SAS ID            fgs.
 * tBus,UNIT
 *      ev*       FORM      strS IO TTR_          Mod15-ONFI     nit P1*  0Ct deviF     or CONFIG dataNSAS_Dd MP          aault 01.05datumCurrentAlirate and resync rate to
 *           ag dTd MPI Manufacturing Page 4.
 *                     Added new defines forD 01.    Two ne  Added defaults forNases, Max    rate and resync rate to
 *       Pag     *  hysical M*             BT         fig
 *    Added define for
 *       AddSHIF       lDisk strS IO Unit
         Flag    Page 0.
 *                     1 01.   AddsicalDisk strlags to controE_SCSI_PORT            isge veuse S Con       .
me types.
 (0xFFfines                    Added define for
 *                      MPI_SAS_DEVICEings.
 *       fig
 *   ne for
 *                          and MPI_               Replaced IO UniASfrom LA0RSISTENw extSELEC1-16-IDCo0_PHY PendK request tDevice Page 0 and addfine       SCSI_DEVICE_0 and 08  Removed the  new extended config page    *   Map RAID Volume
 *  LAGS_PARITY6-02 01.02.09   AddEnclosure/ Fla bNuNSFERS, and
for e0 Added2_eto cons cPortP 0 IBOOT
 *           e 0
 *    Statu05.18
  RaidVol0New statPORT_1
 *        ld)Added define forto make them cded TlumeStat 0 I_PRIMAre for extended config pS IOUNIT
 *     ac sizeStateAdded SECONED
 *                RTPAGE1_s, Maxs foExpanRT_0 Volumeg anUS
 *                      CONFIGfield to RAID Volume PaAcc       *                      Added Enclosurag Res.
 *            Adufacturing pages,*    e CDBBucketsRemad anBIOS f      sion        us
 LAGS_Pags
 
 *         Added EED.09  Added EED(     -24ata ases,lue fo----u 2.
 *        Ne *  lu1 IOC       ;ases, r RAID Physical                   VoesDEVPAGE0_Ns (5 aber flag      A  Pag_EN    e for SAS IOUNGE1_nber fl
 *     Meefinitl *  Delay sags QUIESCoated d SAS Dev *                  A         ad Block TabRESYNC_INpporGRES       version define.
 *                    ADConfig NACTIV      Add7-12-02 01.02.08                       ABcontLOCK_T     F defin            MPI_FCPORTPAGE1res.S
 *     NACTIIM         Vo        and MPI_SAdded ge.07  Changed some f SADEGRADl define to RAID            MPI_FCPORTPAGE1        lf TesFAItl define to RAID  SAS          Added define for
 *    f TesMISSINd Taddch coa        Addelags
 ROT_FCP_Rag RAIIOC PaETTINits defined for IO UDiscoveryS staandlags bfine for I 1.
 *                      Added EnclosurHotSparePoage 0 ufacturinpage: CONFIG_HOagesARE_POOL_                      Added Enclosur       SCSI_DEVI        lags
 *     nit Added derge CDB9ded new BIOS IVE.
 IOPI_IOC to SAS Dev *  01 devic*                 mo         His           ds to SAS IO Flag        Fixed the value for      Enge veIO Fla_WRITE_CACH*   ngfor new 01.00.03  Remove
 *               rub*   OFFLINE_ON_SMARo control S
g.h
 * Added Direct Attach Starng PageAUTOAccessSUR      Added   g.h
 *     ew defines for SAS Ing PagePRIO      Remov  character uniq (s field
 *               ng PageFAST 01.0_SCRUBB*   010asesg.h
 20)g Slobsolet     tus field
 *             Physid CO_METAe PagFIZ      Addeg.h
      :  July 27,efine.
 *        64M12-09                    NHa3-11-05  01.05.08 e.
 *  evng Page512                      Adg.h
 4it Page dded define for
 *ng PageUSEpporDUnder p UFFS_ENABLEreHand         Volume         08 pagea scrDEtBus,             Added Bu     IT1_CONTROL_DEV_SATA_SU  mpping of SAS IO , also    d
 * IVE.
t Page _PAG b01.05.10  Removed IS     Owne    ved redundant N       *                      and to       Added mor            to add a Flags fe.
 *  irect                    Exr SAS IONBAND  Added 08_PAGE_     Added NumDevs              Expn CONFon     ne cha     from
 *   
 *                 Added moated the page
 *      ryO Unit -05  01.05.08  Rnal relLogEntry  TarField two     Added IOC
 *             12  Added two new Fl         Manufactuew fie(0x   Re_FC_*                      and to CONe 3, IOC Page 3, I7.
     *
 *  Host code (d    As,FC949, utTPAGE    etg coshould leav
 * i8-03 for
se*   er d Con  Addheck AddSAS.nit PAGE0_ at runtime.
S IOUifnged          VOL      a RAID PhysMAXyStatus field
 *               LBArese fis, MaxBi new endif_FLAGS_PORT_SELECTOR_ATTACH.
 *  wapped  define or p              and rm fielufac1.05.05t Page 2.
 *                      Add Flags fi
 *    for CONFIG_PAGE_SCSI_PORT_0,
 *       d
 *              fied RAID Volate         ect04  .01.06  Addg Pangd Blo
 for
 *     e typd Manufacturing pafor MPI_     CON                    A defiefinndlootDevclosure Pag SAS eS_ deForceWWIpanderS       to make th      N    Added Ior ManuDEVICE_       Dtructures for the ndded define for
 * *          MPI_RAIDVOL  SAS IO Unit,  off Addcomments.rAS IOPORTFlagrmat  Added NACTIucture offset comments.Strip to FOR_BO002 01.02or CONFIG_PAGE_INBAND_0.
 *  0Manub15-00  3, IOC Page nes for 01.02 *       onPORTPAOnBus04  rVAdded              Replaced IO Unit Page 1 NumNew stat field.
 CSI_PORT_1.
 *                    olumecrub    ;      d
 *       ort orta s cleaned up.
     c Noge 1 Init bit 10-11-06  01.05.13  Added NumFoenacTPAGE1       abl 5.
             3, Status
 *     New stat[*              irect Attach Star];             Added struco control BOOT
 *                      d UniUned meOC Page 1.umge CDB En      cRate  MExcee         in ical Mt, and Hiber fla       related define,                     A     AddePage 4.CONTROL_DEV_SATA_Ssync ra         Added .05.10  Removed ISDat      e types.
olFlags
 *      *                      and to dated    TA      ed config
 *   irect Attac                 Added       OREIGNnded config
 *   irect Attac                   Add    Fl defiDeCIEN    SOUROWED
 *                     related define, CLON       ronous NotificatioManufactur  Added NumDevsPerios Page 1.
 *     ddedf Teselds.lFlaNumRnes foeRTPAGE5_XT_REPLY_DISABREVIOUSLY.h VET new definese 0 and I_SCSIPORrsion fields to SAS IO Unit Page0.
 *  0sical mapping mode in SAS Ie for SAlesync raion
MPI_FcRate         fields to SAS IO U              Add
 * SettinMist Page 1.FLAGS_IOag for SAS IO Unitdded define for
 *       ines for nFlags to control SIG_PAGETY-04-8-08- new 3 Status  Page       S0t Page 1.
 .
 *  05-2onalControlFlags field.
GUID[systemID fiin
 *  ines     Addedturing  ConLink1 tofiel *  1e and updnit Page 3.             Added InacWW                           A4                 these p       s field nd         a on       Expander Page 0 Flags fiting05.18
    gWir        N       evice L MSG a reserved byte in CON      ettingL1mit  SAS IO
 *    1nCONFIit Page o DeviceIO           CONFIG_PAGE_FC                              Flags fieldgs de       ication2    Ca scre CDB7ORTPAGE1_ Unit PaCAP_FLAGS_MUsent bit to
 *                 Adde 4        Add
 *         WWnal ConnI_RAID_PHYS_DISK1_PAT 0 Information.
 *             ddefines f     ases, Max PhysDiskStatusStatus
 *0_ERROR 01.0ield to SAS Device Page 0 and arolFladbByor SAS IsicaENCLOhree nags, MaxTargetPortConnectTime,
 *     SenseKeyd MPI_SIDP.
 * .
 * field
 *                     ew fitiators field to FcPortPage0.
  BIOS Page 2.
 *                          Added Manufacturing       ReportDeviceMissingDelay, and IODevicAScontrol  pages 6 and 7.
                 bit to AdditionalC-  Ad01.Q        Added SoURENumR               MPI_FCPORTPAGE1ew fSmarBLE.
 *         t, and (_SAT       HardAdded BIO      Added EEng ofwo new b07  d CONF         _IOCPAGE1_fSSD drivPORTPAGEnd SCSI_DEVICE_fine.
 *                01 01.ge 0.
 f     Added new config page: CON Added EEDP 

/*******************OC PageNew stat                           ***********
*
define.
OWED
 Added t_FREE_RU**************_INOTIAY          Manufacturing Page 4.
 *            IDdded define for M 01-15-07  01.05.17  Added additi        NroductIDed Page VSDTR_Disk
 *         -----------------------U
 *  03-27RevLev              A SAS and SATA
 *    IO Flags   ModiTPAGEge 1.
 *    
 *   FIG_PAGE_FC_POCNFG_H
**********************U8                      PageType;  2----f SASU8 e s s a g eg e  Inquiry    o n f i 1 01.M e  Adda g e 
 *           t r u c t u rINTE    r_t, MPI_P  Added WWID field             AdSepsent bit to
 *
History
 *  ---------- the      Se1.07  Change0.03  Rem                         AdAdded more dedefiinesIO UnsSBLE.
 *              Added mo               Addedr ExNew stat *  01iceSet
 *             Vt,     served2
pConU8                      PageType;               Addvices    nion,
C HEADER, MPI_POINTER PTR_CON;on HistorDERSlotAS IOU_CNF_PORT__erUnion,
  CAdded define for
 *           pysical Mm       and MPI_S           Ca*                      Added Enclosurre.
IG_PAGE_HEAD Added S U8            ;
} Co      Added new config page: CON         Addv           Added 
 *   *      ;pe; nfigPageRUnitONrUnion,
  CONFPTR_COL_DEV_SATA_Sgs dD Confield
 *                                  Added PageTor RAID
 *                   fin RV                RAID Phys            Ad PCOF_ags to connit P Page 4 28-    re.
* 07     }        Flagle Full                                    TENDED_PAGE_HEADER,
nous Noti     Add page version define.
 * pConfigExtendedPageHeN               .
 *                     ***********************O InitiU8            ED
 *                pConfigExtendedPags fi.09  Replaced MPI_CONFIG_PAGU8                      PageTefiniIVE.
 *         SAgth;             ***********U8     figPar_t,COMPATI  to *                 PageVersioONFIG_PfigardALPA.PaAdded define for
 *                       rel pConfigExtendedPle
 *  IZ#define MPI_CONFIG_ATTR_READ_ONLY               (0x****t, MPI_REQU fieDiskStat       ases, MaxHardAld EEDP 0x2000-2008 LSI ric defineO Unit 05.08  Removed the MANUxFrsio-2008 LSI CoTHERded genes defined for IO Unit Multi-     Do.08         Cse SATA********************** Added defines for nd ansub        -11-05  01.05.08  Removed the  new extended config pages ty         ent bit to
ge 4 ExtPAGE_HEAD-25-01  01.01.06  Added MaxInt settings, Enab     Page 1 AdditionalControlFlags field.
ric define new ex control ERSION        Man008 LSI Cod generic define         acturs  Added mredPa  page  co       UNION;  U8 (0x05)           UNION;

typed         Page 4 Extor SAS IOne SATA Init bit1-04 005)
#define dded structued config page structures for BIOSNT.
 *  0ABo SASne to RAIdded E.
 *        EATTR_RO_PERSISTENT.
 *   SAS      generic definE1_COed some new denous Not 04-    ig page: CONFTPAGE ConfigPagto make them compatibl3          for Reserved2      (0x09)
#defi*         Rate     (0TYPEID_PHYSDISK           (0x0A)
#d    Caage 1 meta Page  Addeed de 3U8                      PaSD drives.              05)
#define I_SAS_EXPANDER1_DISfine MPI_.
 * D_PHY_DISABLED.
 *  08-07-07  01c defineber;            ID f      _CAP_FLAGS_MULTIPORumber; XPAN                   UnitRAID Phyge 2.
 *                    ed MPI_SCSIPORPORT_SELECTPAGETYPE_RAID_#defTH MPI_CONFIG_T---------TYPE_SAS_EXgPageHeader_t;
eryStatus field
 *      ight400-2008 LSI CoE_RAID_VOLUME                    ight6apabilities Flags.
 *              andEXTPAGETYPE_ENCLOSURE       LAGS_PARITYSION deines fotatus er_t, MPI_PPORTns.
 *       values for MPI_FCPORTPAGE0_FLAGS_ATTACH_
URING      Ad***************PE_IOC ne SA14)
#define MPI_CONFIGB00-2008 LSI MPI_********Added define for
         Nums field appiedIDPANDER (Volumeags b         HEADEield to SAS Device Page 0 and add
#define        Reserved1;     1       0F)


/********x13)
#U8                      PageType/******EXTPAGETYPE_LOG1Pathef struct _CONFIG_EXTENDED_PA         (6EXTPAGETYPE_LOG           1-2008 5-07  01.05.17  Added additio      008 LSI 1*******ROKEormation fie.
 *      EATTR_RO_P1_SSettings.          to Expander Padisable usDREer dG Support in
 *                  s to SAS 601-15-07  01.05.17  Added additiorevith   01.0Forliases, MaxHardAlior0 sptck Tab)
#des000000 *   dded WWID field to RAIDMPI_CONFIG_PATA    hyscial Disk Page Versi-2008 LSI CoSOWED
 * Disks v.
 * EVERSIONags field.
 *  05-24-0r_t, MPI_PAP_FLAGS_Mdded define for
 *    NCLOSURE         age0.
 *  0define MPI_CONFIG_EXTPAGETYPE_ENCLOSURE        Added#define MM        ******5EXTPAGETYPE_ENCLOSURE                     (08T             FORM_BUS_TID                (0x00000000)
#olumeSettings
 *Modified        Manufacturing Page 4theefine*     00000000)
#ONFIG_PAGETYPU8                           (PE_LOS       ath_SCSI_DEVIHIFT               FF00)
#      ersions.
 *  onfiT_MOT_MAS(0xC              0)

#d1FT             BLE     for      ine MPI_CONFGE6_CAPRSISTENMULTrred05  0IFT       EXtion.ONFIG_EXT         (16)

# to CONFIG_P_FORM_MASK      S Page 4 structure.
 *                      Added MPI_RAID_PHYS_DISK1_PATH_MAX defLAs field to MaeAddress ficiAdded Die MPI_SCSI_D       CONFIG_5.1         ne for SAl.
 * ved some obs   SSD drivnd SCSI_DEVI000)
#define M_CNFighto SAS _     Ced AdditionalControl DVt Pa modSettings and added
 *                xRxEVICE_Type;      )
#define MPI_SCSI_DEVI2h */
    U8                  P           #define MAcce MPI_FC_PORT_PGAD_FORM_INDEX     Pa2 01PrePaure fields to SAS DevD_ND_PO_SAS_EXPANDER     BOOT
 *                      dS_ SAS
PORT_its in Status field of _PGAD_BT_sical Page 0 In      Added WWID field to RAID Volume Pam   Added BI000FF00-2008 LSI Corp_RETURN_LOOP                       Flags fefineEADSI_DE PHYSDISK_PSUPP          NUMsion: )


/******: CONFIG_PAGE_IOC_5.
PHYSDISK_PGSKNUM_SentAlia 1 config paORT_SHIFT         NDo SAS EHIF    (0x14)
#d(2    AP_FLAGS_ISK_Pition t     N *         define MPI_CONFIG0FNDLE ne MPI_PHYSDISK_Pefine MPI_SAS_EXPANhanged.onfigPageHeICE_0 pagefor CONFIG_PAGE_SCSI_PORT_0,
 *                   /*   PageNumber;F0000n Status Hidedefines for SArt pth Non-IntA_ONLY
 *              _CONFIG_PAGEdded new fields to the s0)
#defi       Adde.
 * 4 U16         NH_SHIdded define for
 *      FT_HANDL    (16(FT                             field of RAID Vo    war.
 * v1.Low values
**.
 * PAGE1*      dded define for
 *      (16T        UNITPAGE2_E2_FLAGSLAGS_MULTI (0x000000FF)
#define MMaxWireSpe, and RA MPI_CO              and EXPAND_PGAD_HPN_MASK_ VAND_PGAD_H_M_PGAD_H_MAND_age 1 AdditionalControlFlags fo De
 *    Bed th.02.08 ines for CO (0x0AD_FORM_MASK  XT_HAHANDLET              1.01 00FF0000)
#define MASAS_CSI_PORT_1.
AD_HPN_SHIFT_PHY       eg_PGAD_H_SHIFT     Hsion: (0xn field and  PhysicalPort LE    15-0_    NEXT_HLE    (0ND_PG (0x002D_INDEX_SHIFT              **************     (28)
#defiFC_     _PGAD_Ffine MPI_FC_DEVICET         _PORT_  (0x00000000)
#d_DEVICE_PGAD_FORM_MASK       CONFI      Added EED0xF0000000)
#defiDEVew fiNuRESO             *device id definefine MPI_0000)
#define MPI_SIOPERA   Mo control irect       efine to 4 IO Unit, FORM_BUS_TID             (0x0100    T.
 *fact*  0K1_PAT(0x0     InbID_S      Added WWID field to RAID Volume Page 1.
 *                      Added PhysicalPort field to SAS ExpandOR_ATTACH.
 D_PORT                             Added AdditionalCont   (0x00FFFFFF)
#define MP:  MeroAtta 4.
 Ao contrfine MID,BootBus, and
 *PI_C000)
#define MPI_SAS_ORM_Bag dTIT_MASKMaximumBufdded_t     uct;                d the mepage and epBu SAS IRD_ALPA_ONLY
 *              r IDP biersions.
 *  PhysicalBOOT
 *                      dfine MPI_
FD_PORT_its in Status field of#definE_MASK sical Page 0 InD_PGAD_                      to add a Flags f0)
#de (0x000(0x00000000)
#define MPI_SAS_ORM_BARGETD (28)
#000000)
#defin0s for      I_SAS_DEVICE_PGAD_FBT   (AND_PGAD_FORM_SHIFTFT              S_DEVICE_PGAD_FH (0x00FND_PGAD_FORM_GET
#define M:  Mdevic                     Manufacturing Page1000000)
#dePortPage0.
             /* 01h */
I_FC_DEVICE Reserved1;                  (0x00BytesPh     I_SASVottingdeNonZ     efine to /* 01h */
 Nego    ed (0)fierSCSI_DEVI        IO Unit
 *     esync rlerPh Added rmatix0000FFFF)
#define MPI_SAS           (0x    Wags f    _NUMB    PHY_TBL_INDEXI_SAS_ND_P MPI_D_H_HANDnd SCSI_DEVICE_1ucture offseSE     s********************_INDEX_D_FORM_SHIFT             BOOT
 *           U8                      PaID
 as_DRIVE_SU         ExrUnion,
 U8               AddedSI_DEVICE_TM_RESPOND_ID_MASK          (0x000000FF)
#define MPI_SCSI_DEVICE_TM_RESPOND_ID_SHIFT         (0)
#defineBUS_MASK                 (0x0000F***********_SCSI_DEVICE_TM_ spec dine MPI_CONFS_ENCag dM#define MPI_SCSI_DEVICE_TM_INIT_ID_MASKReserved;  .09  Added EEDP      In CVICE_3.
 *          ded FC Port pages 6 and T_HANDLE  (0x00000000)
#define MPI_SAS_EXPAND__PGAD_FORMPI_S4 adaptDuring_BUS_TARGET_I_SAS_(0x0000FFFF)
#define MPI_       (0x00* 04EXTPAGETYPE_LOG  P adasHigh4 adapters and sidth definitions in SCSI_PO        ND_PGAD_FORM_GET-------
 *  05-08-00  00.10.LE    INDED_PGAD_FORM_GET_NEBL_INDEX (0x0000T               000000-------------------- for CONFIG  U8                      PageType;   *e 0.
 * MaxHardAliasdefine MPI_SAS_E          Reserved;        ne M_PORT_1)
#    _SCSIh */
    U8        _MASK_PHY        rsions.
 *                     ( SAS Device, and SAS PH     *********************its in Status field of    (0_t;       sical Page 0 InSAS
    Udefine.
 *             e 1
 *  ord1AP_FL)
#deAD_FORM_H(0)        Added So        Added Poh */
    U8  sage     PendCOVERYrevious     T1_C MPI_CONFIG_PAGEATTR_READ_ONLY            0U8     , andNACTPAGE_DEVMASK             (0U8                     00FF00)ASK            PaONge vCONFIG_PAGEATTR_READ_ONLY            E*  0e MPI      a        Added BIOld values
*********** Message
**********U8                      PageTHY********
         0x20)
#defi         CURREN***********x02****************Tield to RAID Volume PagBios Page 1.
 *      MPI_CONFIG_EXTPAGETYPERPAGE_WRITEGE      NVRA************FORM_SH   (PI_CONFIG_ACTION_PAGE000)
#define MPI_SU8                      Page02.14_VOLUME_INACTIlFlagsE_ENCLOSURE                 (0xTPAGETYPE_LOG   G_ACTION_PAGE_WRITE structAdded define for
 *      TPAGETYPE_LOG   AID_PHYSPEED_NEGh */
 MPI_SASD_PGAD_FORM*****sion/*U8     egal   Fla PagOOBage 0, a
 * Length;                    rel/****************1    In CONFIG_PAage 3, IO/
    U8                /****************3tatus field.
 *_HANne MPIe MPI_FICE_Pves.
eePagessas.h)
#de (0x03)
#define MPI_CONFIG_ACTSAS_ENCLOS************                 
    U8                      er;                     /* 14h */
    U    DRM_MAS_DETEOWED
 *          Four new Flags bi SAS IO Unit PagT
 *           NADDld va    D_0_FOd
 *                      associated ddded defines forICE_PL     PageAddress des S    MPI_IOCPAGE1_INITIATOR_COER PTR_MSG_CONFI_CONFIG_1.
 *   Replaced MPI_CONFIG_PAIG_PI_CONFIGSK                 ABLEMP_TIMEOUdef struct _MSG_COABLE_BTR  PagE_LINK          MMPI_CONFIG_ACTI    ROUT    TRI    (1          }       *         Title:  MMPI_CONFIG_ACTI    X8    EX_ACTION**************g.h
 *   ****************U8                 FUNC    _CONFIG_ACTION_P     g.h
 *       *******/       /* 02h */
    U8CT Pollin  (01.01 SISTENT                CONFIG----1EXTPAGETYPE_LO********    ;

LINND_PGAD_FORM_Sh */8     factquest Message
****_FLAGS 3, IOG_PAGE**********/*/
    U8  l */
#define 4 define hotORID_LSI ConOWED
 * UN    ORTTED
 *            RGETI_MANUFACT8I_MANUFACTPAGE_DEVICEID_FC909   field to   U16  g u r a t i o nMg.h
 * 1_MASK         (0x_MASK PORT_ine MPI_CAGE_WRDOMA.
 *    ab fieFibre Chann2******e MPI_SASND_PGAD_FORM_SHIFved;   MPI_CONFIG_A  Added SoPGAD_FORM_BUS_TARGET_0x00FF0000)INDEX_SHIF              /* 0929X            (0x0ine ine MPI_MANUFAANUFACTPAGE_DEVNION_DEVICEID_FC929X     CLOS_PGAD_GNH_HANDLE_MASK     MaxMinne MPI_SAS_DEVIFlag
 *                     GN MPI_SAS_ND_PGAD_FORM_       (0x0000FFFF)
#define MPI_SASMaxc*   ceTimeout and_BOOT
 *
#define MPI_SAS_DEVICE_PD_FC929(0x0ne MPI_SAS(0x0TBL     ield to      (5 and efine MPICE**************ILOGI   U16   es forT_HA3#definx01)
#define U8                      PageTyp U8     U8                   /n History
 *  ---MSG       
{FIG, MPI_POINTER PTR_MR PTR_MScZeroine MPI_CONFIG_TMPI_CONFIG_EXTPAGETYPE_LOG      G_PAGE_LAN_1 t */
    Uine MPfine MPI_CONFIG***********PageAddress;           ChainO     * SAS */
#define ----                 0FF0000)
#define Fun41)
/* SAS */
#define----F)


/**************** with ADISCHardE_REMMEDIAine MPI_CONFIG_----4MANUFAC             Added AccessStat#define MPI_CONFIG_Teply_t;



/**********      EATTRPI_MAFFFFFF)gh 10.
          -----------o field value
 *  featu     dd     
   66    (16)

#define 05A    VID_SAS1064             Added new fieldVICE_PGAA_ONLY
 *           MPI_FC_       AS_EXPANeneric defines for ConfiNH_HANDLexne MPI_MANUFACTPAGID_SA8EXTPAGETYTED
 *          INDEX_MU8                   (0QDep
 *  06  01.or pSwa1.07  ChaPE_SCSI_ge 1.
 *                   0031)
porfine MP SAS IO Uni    Added structeld to        Added defines for MPI_2[8]h */
      Board make16]AS_EXPA1 01.02.05   Addedefine MPI_F1030ZCFCPORTPAtPageLS and SATA
 *   BufPageAddress;               
 *            1064               UFACTPAGE_   U8        PageVersi     *************fine MPI_FC_DEVICEn     0   PageVe0000FF)
#define MPI_SCSI_DEVIDEVICE_PGAD_FORM_M       Word1 values
**   (0x01)
#define PI_PPAGE_DEVIU8                      Pa1RITETROL CONFIG_SELF_TEMsgContext;    S1078     1DEVID_5N
{
   ConfigPageFF)
#defin3_0AD_FORM_MASK_RAID_64_BI *I_MANUFACTPAGE_DEVICEID_FC909       PD[251_to match MPI rev 1      NG_1, PAGE_HEADpages (5 and 6).
 *1;

typedS IO PSERers
 cnZero Sed the
ED_PAGE_HEADManufacturingPage1_t, MTION_PA0;

tH    Fixed the valu*******************ufacturingPage1_t, MI_FC_DEVICeld
 *                 

#define MPI_MANUFACTURING1_PAGEVERP_POI167  01.0IDine MPI_CONF(ManufaAdded def                   VPDHvisionID_B   BoardAssPTR_CONFIG_PAGE_MANU**************EVID_SAS1064E 
#defionID;          I_MANUFACTPAGE_DEVID_SAS1064E  VID_SAS}rved; Hon,
 EVI.
 *_ID;

typed         deNonZero fd_t, MPI_PO
  MpiChiPOSTSAS_Ei  03-11-05  01.05.08  ;

#define MPI_MANUFACTURING1_PAGEVERion,
48BIT_LBAI_MAN0_CA     g.h
 equest Message
****LOGIC  E_RE1   PageVersATTR_PER.ine MPI_M    /* 02h */
    U8         _WORDS    (1)
#MPI_PONCQS_WORDS9X         GN8       GE_2_HW_S2.
 * edef stFFFF)
C103d Nv

FUPORT_RDS
#definsionI     FC9498)
#de      h */
SSING.
 *       HYPI_MAfr_ORLY,
HOS        Added define for
 *  ONFIC1035   I_MANU2_H************LLEGVICE_PGAD_Bped and
 *        Num          /* 04h */
FIRST_Lion.
 C   U30)
#defineI_SCSI_DEVICE_FORM_T          /* 04h */
CLEAR_AFFIL)
/* SAS */
#            /*078  sion History
 *  ----------PC1035         (0x0032)Page1_t    U8                PI_PDEVIDserved;         RT_PGAD_FORM_INDEX _PAGE_BIOS_2.D_FC9if          /*GAD_GNH_SHAddrAitioHROUNO      IFIC
/* SAS *   /* 04h */
 RING_2
{
    CONFIG_DLE_MASK IMANUONZERO
#definefinHY_IDENTPAGEpages (5 and 6).
 *2    U8       ****/

/****** Pag.
 *NLY     *****xF0000000efine.
 *             NFIG_EXTPAGETY_ACTION_PATY5 and 6).
 ER_UNION
{
   Conf*******************************/**         ine MPI_CONFIG0622)
#define M      wne MPI_S[     /* 04h */
 RING_2*                   (0x005ipI   U8        I_MAN_PAGE_3_INFOength;                 (0x00SAS_       T     )

#define MPI_SAS_EXP----ype defines _POINTER pManufacturingP         (0x0000mbly[16MANUFACTURING2_PAGEVERSION  REes.
 *0)
#deI_MANUFA      Two new C_2_RAID_V*        X            (5 and 6).
 3_      ped and
 *     _heckAI_SC2_t, MPI_POINTER pManufacturingU8                      PageType;      CONge2_t,                       ypedef struct ULT         (0x                IG_ACTION_PAGE_WRITEGEe and ISLT         (0x05)
#defiSK_PHY        nfoOffset0;    DEed dfine MPI_MANUFAC    U8                      set0;        /*_MSG_CONFIG_REP    ine MP_CONFIG_ACTION_PA                         Reserv_POINT----AEXTPAGETYPE_LO      MsgLength;   _MSG_CONFIG_REPL /* 03h */
    U16  TPAGETYPE_LOG40)
#define MPI_MANUFACTPAGE__G_PAGDEVID_53C1035ZC            (0x0EID_       ig pao#defin1ine MPI_C* 0Bh */
*****         Reserved2[8];       onId_tAN_PAGE_3_INFO_WORDS         ----Edded Disableddefine #define MCONFIG_       FIG_Plume[5       fine MPI           sgtion
 CTPAGE_DEVI      MoIS
#define MPI_S    /IN 10h *PAGE_DEVID_SAS1064             HwSege2_t, MPI_POINTER pMan* 4C/
   PAGETYPRT_0AdditionalControlFlages
*********************es tRGET_ID  U32                        PAGE_MANUENDED_ U32                        Msg  /* 00h I_MANUFAC                            define MPI_MANUFACTPAGE_DEVID_SAS1064E                      #define MPI_MANUFACTPAGEFACTPAGE_DEVID_SAS1066                (0x04)
#define MPI_CONFIG_PAGETYP.14 NumDevsPePI_SAS_EX_MANUFACTPMANUFACTPAGE_DEVI**********************078      pedef struct _CONFIGTE_CURnes for ExtFl          Add (0)
addd2[8                     h */
  d    S t Reserved4;d 6).
 *  EVID_5ine PN0x0000ionadefine f        12            AdNUFACTPAG62sion History
 *  ---------   IMResyncRa           U         /* 02h */
    U8ChipId;           8                  PageType;      X       ChipName[16];               /* 04h */
RAID    pping MPI_MANUFACTPAGE_DEVIn[8];       d9;          /* 6Ch */
} CONF (0x00507I_SCSI_ MPI   (0x/* 4        /* 5 and 6).
 *     on,
  CO2 PTR_CONFIG_PAGE_MANUFACTURING_0,
  Manufa2        (0*******2MPI_CONFIGge 3, IOC PagringPag2e MPI_CONFIG_P (5 and 6).
 0_P2 pages.
 *nactive Volumded             VPD[25       ChipI2* 6h */ield to RAID Volume Pa(0x0050)
#2        D_0_FORLIM*****CEEesync r_PGAPage4_t;

#defin3         IM/                  */
    U32  APP     0;          /* 09)
#define MP          ION_PAG_PixedSTllingAP3_t, MPI_POINTER_DEVID_SAS1066              EP b         8E_MANUFA0000     /* 09h */
    U8                     20FHIFT       0x14)
#define MPI_CONFI2000*********     dded PrSResE  Added        RE    /       d 6)0FFF     TE_CURRENT        (0x081) */
 PI_POINTER     A3_t, E             1_0        NFIG_EXTPAGETYPE_ENDEVICE4E  PCine MPI_Mion.
 EXTFO_PARings.
C MPISIZ_COERCION7e MPI_MANPAOEXTFLAGS__t;

#defin      Reserved9;                  /* x0180)
#neRECTGE_3_INXTFLAGS_1GB_COER    CONFIG_PE_DEVID_SAS1064E  X       4_EXTFLAGSSLO_EXTFLAGS_       InquirySize;        /* 0Ch */X       HOST_ASSIG_DEVWORX_SSD     
 *  x004ce        (0)
              (0xFK_TABLE    PORT__BOOTPAGE2_, cess030_FORHide Phys)
#define MPI_M     (0x0R PageMIX_xtPageType;   C Page es te;       /      (0x0062)


typedef [256EVID_SAS1078   #define MPC1035         (0x0032)PORT AGE__PGAD_GNH_SH.
 *  01-25-01  01.01.06  Added MaxInRAID_PHYSD (5 and 6).
 *0    U8     eneric defines for         /* 09h */
    Ares.-07-07  and strd heefine M structures for the ndded define for
 * YPENUM is fine MPI_MA MPI_FCPed metadata size bits in
 *                  
#deres.union:DiConnit Added Manufac       AddedFT_HANDLE      (0)
#define     TPAGE_DEVID_SASG_PAGE_HEA------ed defines for metadata size bits in V 04-   /ossGE_MA    hG_PAGE_MANUFACT                      field of RAID Vo2-*  00    U8               CONFIG_Perved2[8];               /* 028)
#define MPI_Fhy  (0RProblemGE_HEADER              (28)
#defiAS_DEVICE_PGAD_FORM_FC949X                     RGET_IE_DEVICEID_F                  (0x05)

3 PTR_CONFIG_PAGE_MANUFACTURING_0,
  Manufa3I_MANPAGE4_FORCE_    BLOCK_TABLE              (0380)
#define MPI_MANPAGE4_FORCE3OFFLINE_FAILOVER      ical mapping mode in SAS 5.07  Added InactiveStatus field to RAID Volume Page 0.
 *        sDiskSta)

#define MPI_SAS_ENCLOS_PGAD_FORM_MASK               (0xF0000000)
#define MPI_SAS_ENCLOS_PGAD_FORM_SHIFT          /* 04HIP_s;     ost   IMEDataScrubRate;   /* 60h */
    U8                      CONFIG_PAMEIdenti*          dres***********             _MANUFAC RAIM_FORCEWWID
#def   IMResyncRa6VID_SAS1064E   
  ManufacturingPage6_t, MPIMlume     cificInfoE_MA                  MPI_MANUFACTURING6_PAGEVERSIONRine MPI_SASANUFACTPAGE_DEVID_         eld values
************
    US#defineFLAGS_L7ID_SAS1064           ID_PHYSDISK           (0x0A)
#dFIG_E00FF)
#define MPI_SCSI_DEVICE_T                     ManringPage4_t;

#defdefine MPI_SAS01.11  Added some new defOINTER pManufacturingPage4_t;

#defPa
#defin 0Eh */
    U8    I_IOCPAGE1_            (0x0058)
#deISTENT.
 *  0A
#defi AddeTPAGE_DEp *     age4_t;

#rved)
#define MPI_MANPAGE4_FO6E_1 pa   FlalRouteIndestruct _MPI_CONFIG_PAGETYPE_SCSI_h preferred               ForceWWID[PI_MANPAGE4_E             Replaced IO Unit Page 1 AGE_NFFFF)
I_DEifndef efine MPI_MANUFACT            IMRe r a t i oPI_MANPAGE4_U8      TURING_4,
  ManufacturingPalaced Reserved1 field
 *acturing PaPI_SAS_DEVIC                _PAGE_MANUFVICE_FOERSISTENTESERV)
#define MPI_ PTR_CONFIG_PAGE_MANUFACTURING_0,
                  versio********define MPI_MANPAGE
#defi_0 a     define MPI_MANPAGE        TOR_INFO,
  MpiManPagAdded de             VPD[2008 LSI C[2          IMResyncRat_MANUFACTPAGE_D_SHIFT        (0)
#    CONFs;  00)

         IMResCh */
    U32                _SFF_8470_L3two Revi        /* 62h */
       /*                    VP_SFF_8470_L2                      a t i   Flags;     ength;          _SFF_8470_L2                (0s
********          (0  Reserved3;     _SFF_8470_L2            /* 0Ch */
    U16              
*
************SFF_8470_L1         ef struct _CgPaga 8484_PPTR_CONeader_t, MPI_POUT_CONNECTION_UNKNOWN          (0x00000001)

/* defines for th*******
*, MPI_PO MPI.02.0          (0x00000001)

/* defines for the Locatio
#define MPI_MANPAG            )
#define MPI_CONFIG_ACTIfor the Locationefine MPI_MANRNAL                  (#de035         (0x0ACTPAGE_DEVICn field */
#defiE_PGAFF_8/* Fibret r unnes forC1035         (0x0032)
n field */
             *****2                  (0x00_PINd PCSFF_8470_L Port Page 4, FC __DISRT         AGE7_PINOUT_SFF_SFF_8470_VE.
 * MPI_POINTh         c.) s               (0x08)
#lay BootDeviceF   (0xG_PAGEoSize0;                            CONNECTOR_INFO_Mk 1.0 rdefine MPI_CONFfU_FC_DETfineG_6    U8                      VPD[25E_DEVID_SAS1078   0x0002)
#define MPI_MANPAGE4_EXTFLAGS_LEGACY_def struct _CONFIG_PAGE_HEADER
{
    U8   /Sl).
 *6ength;                         (5 and 6).
 *6ost cANUFACTURING_4
{_POINTER pManufacturingPage6_t;

#define MPI_MANUFACTURINGIMuctSpecificInfo;/ringP5 */
} CONFIG_            Reserved9;      NFIG_EXTPAGETYPE_LOG     VID_SAS1064              (0x0050_t;

/*Pdded Disabled bad                             Reserved9;      /*N_PATge vnew iesProgrammT_BUS                           Reserved9;          /* 6ne MPI_SAS_DEVICE ChipName[16];               /* 04h */
Hw        


/*********46)
ge4SISTENT          ABLE             4_e MPI_SAS_Eme[16];              /* 1Ch for the Pinout f6 Port Page 4,rmation fielical mapping mode in SAS x00000100, MPI_POINTER pMae MPI_SAS_ENCLrmation fituring         IMResyncRathe Pinout field 
#definge4_t;

#defin Ext      IDEVPAGE0_N IO Unit, SAS Ex *   AGS_0h */
    U8       ORDS    (1)
#enPage 1.
 *  FLAGS_  Added N0000000)
#define MPI_SAS_DEVICE           Replaced IO Unit Page 1 7, MPI_P                ld */INTER post codI_MAN_P7    fieorted the    /* 00h */
    UP      Sed th;0x0180)
#some obsoletein    ct _MP*00)
#def                 _8484, utilities, etc.) C1030ZC8DEVICEID_F*********            7_****VICE_FOSFF_8470_sionties, etc.) sho    50000_ pMpiMaE           ved4         IMResy
#defi4DEVICEID_F      Reserved2;              /*ISlot     ChipPAGE_DEVICEIT_SFF_ACTURING_7, MPI_POINT_POINTER PTR_CO4_L    CONFIG_PAGE_FC078  _DEVICE_PGAD_FORM_MASlags;      usdefine MPI Reserved7;nd 6).
 *3,
4_EXTFLAG7ANPAGE7_          6).
 *908h */
  HW           9URING_0FF)];CTURMANUFACTURING    U32      ReseVeUFACTURING9_PAI3, IOCine.
 *  efine MPI_MANPAGE7_PINOUTGE_HEADER                CONFIG_PAge2_t, M     NumPhys;                 PageAddress deHG8E_1 pages.
I_MANPAGE4_EXTFLAG7e (drivers, BIO******istor    G_ACTION_PAGE_WRIlities, etc.) shoINTER pManufac5 and 6).
 *     d BlocCHA*      MPI_POINTER p0FF)
G_PAGE_HEADE5 and 6).
 NOMAX
#SpecifADER, MPlags;              /* 0Dh                 041)
/* SAS */
#defic     h */
    Uefine MPI_MANUFACTNE                       (0x00                       define MPI_MANPAGE7      InfoOffset0;   MANUFACTPAGE_DEVID_SAS1********************MPI_MA#define MPI_MANG_7, MPI_POINTER PTR_10      (0x*********************define MP    /* 0Ch */
    U8                     Reserved9;                                (0x10)
#me[16];      /* 10h */
                   R            5E_1 pages.
 *E_MANUFACTURING_9, MP    ANUFACTURING_8,
  M       Added                   /* 01h */
    UFACTURING5_PAGEVERSION  ne MPI_FC_PORT_PGAD_FORM_INDE_HANDLE_MASK           (0x0000FFFF)
#define MER  D_0_FOR_B.
 *         ne MPI_MANUFACTPAGE_DEV                            R PTR_CONFIfor
 *OINTER pManufacturingPage4_t;

#defSlox0626)
#define MPI_MA    s for the Flags field */
#define MPI_M  (1)
#enI_POINTER pIOUnitPage0_t;

#t, MPI_POINTER pManufE_MANUFACTURING_10,
 NTER pDEVID_53                             /* 00ht;

#define MPI_IOUNITPAGE1_4              (0x0050 ExtFlReserved2;  fine MPI_CONFIG_PAGETYPE_FC_DE0_PROT_08)
#define MRA
#define MP     UFACTURING_8,
  ManufacturingPag0x80)

/*  Worfor the LocationER pMpCONFIG_PAGErcId_tPAGE1FIG_d anmixiAGE_DEVID_SAS1064             00000ofines for the F;      /* 61h */
    U16           #definunion _CONFIG_PINGLE_F  /* 68h */;      /* 61h */
    U16           h */
} CONFIG_PAGE_MA0)
#define e MPI_Mnufac        /* 62h */
    IG_PAGE_HEA       (0180)********
*   IO Unit CoOINTER pManufacturingPage4_t;

#defo;/* 04h */
} CONFIG_PAGE_M8IO_UNIT_1, **********************************        /* 0_PAGEATTR_CHANGEuringPage8_t;

#define MPI_MANUFACTURINInfo;/* 04h */
}                   Page8_t;

#define MPI_M40ne.
 *  1OOd defaults fornactive Volum/
    U32          e Large CDB Entatus field of     (1)

#defin */
    U32                 Added new config page: CONe (drivers, BIOS, utilitie       E_FORM_TARG******************_SCSI_DEVICE_* 0BhE_PGAD_POhe Flags field */
#de} UFACTPAGE         (0)
    pteUFACTUR    U3_GNH   Ad*******************MANUFACTPAGE_DEVID_SAS1060)


typeX         CAPABILLE_QCONFIG_ACTIO                        0x0002)

typedef strE_IO_UNIT_0,ry
 LFACTURING_1,  U64              x0002)

typedef strNEEserved;     nit Pine MPI4#defi Pagdded define for
 *    cha fdifi  01K_COESIZE         (0)x0002)

typedefI  Added Device Se (0x0050)
 MPI_MA          IMDatPAGEVE  (0ADAPTENTER                      04    .
 * AGE7_PINOUT_SFF_8470     HistorIF_DIOCPAGErved;              1   U8                      VPD      IFstructNECTOR_INFO_M   Reserve;          /* 58h */
    U32         F_CHECK_PCTURIN               o Ori ExtFlags;erInt, MPI_POINTER pIOUnPIO_age 2.
 *           NFIGases, MaxHardAli     /* onfiON        U3DM/* Config Rx00000#def  (0xURING_9
{
              32            IT_2
{
     (0x000000_EXTPAGDAPTER_INFO, MPI2_FLAGS_VERBOSE_ENABLE ZONGE_MVIOLWORDS];/8h */#define MPI_IOUNIT         PAGEVERSION     PTR_C          ge2      (0x0GE_HEADER          O_PARVERBOSs field */
I_POINTER PTR_MPI    CONFIG_PAG            _COLOR_VIDEO_DI04h should leave this define set to
 *rInfo U8    uct         AddedCONNENFIG_PAG_PAGEDD ChipName[16];       (0x0  U8      PRING_7, MPI_POINTER PTRit Pa
{DISPLAeserved7 (0x000004t, MP*
 *  CTED             (0x80)

/ MPc.) .
 *            _IOUNe this define sG.
 * edef Heame[16];   icInfo;/* 04h */
}MEVolumeSett*/
#ifndef MPI_IO_UN2
{
  G    VAL0FF)
#define MPder.PageLength at ru*/
#ifndef MPI_IO_U HistDER  History
 *  -#define MP0;    GS_NL_PORTD*/
#ifndef MPI_IO_UN_IO_UNIT_0, MPI_POINTER PTR_CONF IMResyncRate;       /PI_I        mpiEL            DER, MPI_ (0x00)


typedef struct _C8            E_ENABL_RESYNC_CACapabilities Flags.
 *         x0054)
              D_0_FORMPI_MANUFACTURING6_PAGEVERSIt
    CONFIG_PAGE_HEADER          Header;G_7, */
} CONFIG_PAeserved2;        TURING_10
{
    CONFIG_PAGE_HEADETPAGE2_FLAGS_AterOrder[5UNITPAGE2_FLAdefine MPI_MANPAGE4_EXTFLAGS_SAT                             /* 00h */
    CE_TM_INIT_ID_SHIFT                   /* 00h0)


typedef struct _CONFIG_PA                   /* 04h */
} CONFIto make them compatible to MPI vers defines */
#define MPI_IOUNITPAGE1_MULTI_FUNCTION
#define MPI_IOUNITPAGE1_FORCE_32  ciBu*   b        Reserved9;   PAGE_MANU;              U32           00000000)
#define MPI_IOUNITUME_ID         (0(0x00000008)
#d
 * Host CE_FORM_TAR_MANUFA_QUEUE_FULLfine M *       (0x000002000-2008 LSI Co                    I       /* 14h */
 should leave this0020)
#define MPI_IOUNITPAGE1_DISABL     lRe1 01.02FIS[2

/*
 GPIO_SETT       IS_DEV (0x31)
#deine MPci  01.0And#define E3_GPIO_SETTING_ON      e Large CDACTURING_7, MPI_POINTE_MANUFACTURI  Reserved1;             00FFFF)
#define MPI_SAS_DEVICE_PPE_SCSI_LUN with
 *        O_UNIT_4, M   ProdOINTER pManufactuE_MAACTPAGE_DEVID_SAS1064                    ProductSpecifi
/* IO Unit Page 1 Flags defines */_MANUFACTURINGE_IO_UNIT_ring Page 4 Extinout;                 /* 00h */
  E7_FLAG_UGS_1GB_
 *            eld h */
 LAGS_EMBED*********I_POINTER PTR_CONFIG_PAGE_IO_UNIT_4,
  IO_MANUFACe Large CDBAS_Efine fNSTALLED_D                    /* 04h */
} CON/*_ 4.
 *  FAIL***/)
#d             gPage9_t, MP_MAXTPAGE2_FLAGS2       NIT_0, MPI00FF)
#define MPI_SCSI_DEVI      EN    roughgh _PAGE_MCOU_DEVID_SAS1064ANFIG_PAG       (0x02)
         IMResynMPI_MACh */
    U32      (ISPLAY       (0x00B_COERCION_SIZE      NnfigDI     /* 1Ch *g.h
 *          /*SVolumeSet               (0x0000      ***********32              (0x00B_COERCION_SIZE          C000004(0x00000002)
#deF(0x0032)
#d     1      Modifi           /* 1                          GE_IO_UNIPage0_t;

FACTURINC_0, MPI_POIN    U32                  0_t, MPI_POINTER pI Reserved7;  IMVolumeSettings;   /*ICE_TM_INIT_IIO Unit P
    U8                      VPDe[16];      /* 10h */
    U8               UNITPAGE2_PAGEVERSION       Reserved1;              ********************  (0x01)
                  Reserved2;  O Unit UFACTURIFO_PConfigLInfoR pMCONFIG_PAGE_IOC_01
  IOCPage0_t, MPI_POINTER 1fine MPI_IO_POINTER pManufactur_t;

#define ge 4.
 *             s for SAFibre Channeved1;               _****MPI_SION                   E_DEVICEID_Fserved2;              /*7,
FUNCTION                 (0x00000001)
#define M8                  Reservedits defined for IO U (0x00000004)
             R_RO_P3dif

tNG_2
{
_       49X            (0x0_HANDLE     MPIGEVERSION                     (ne MPI_IOUNITPAGE0_PANUFACTPAGE_DEVICEID_FC949X         e 1 Flags defines */
#d    NumPhys;                /* 20h */
    
} CONturingRSION 2***************** for CONFIG_PAGE_SCSI_PORT_2
 *   , MPI_POINTER pManufacturing2;              /*8    U8                      VPDs (5 and 6).
 * 08h */
                         OUNITPAGE0_PAGEVERSION            LE             7
  IOCPage0ACTURING U32             )
#define NTER PTR_CONFIG_PAGE_IO_UNIT_    Co      T   /
} CONFIG_PAGEAda  (0FlHistory
 *x08)
#define MPI_M*/
        cleaned up.
 *    lags;              /* 0Dh annel */
#deNUFACTURING_5,
  Mefine MPI_MANPAGE7_PINOUTge 3, IOC P /* 10h *******
*   IOC Config Pages
***********ISVolumeSett*******************T_DISine MAMh */
    U16     EMBDISPLAY       (0x000h */
    U8         /
    U32                  IOC;         EVolumeSett    U8                        IG_PAGE_IO_UNIT_0,*************h */
    U8            0h */
                /* 04h */
} CONFIG_PA IM
#define MPIe MPI_MANUFACTURING6_PAGEVERSION ING_8
{
   (0xE3_GPIO_SETTIN_DEVID_SAS1066        4h */
  10
{
    CONFIG_PAGE_HEADER        Flags;          Ioc2ld).
 *   PageVe       /* 00h */
    U32               ER PTR_MPI_MANPAGE7_CONNECGE2_FLAGS_DE8
{
  B     (0x00000400
    U MPI_MA   U8                          VolumeIOC;       SVolumeSettE4_EXTFL       _efine    Reserved3;              /* 06h */
04h */
    UVOL_TYPE_IM                       (0x02)
#define MPI_RAID_V/
    U16   VOL_TYPE_IM            Reserved3;              /* 06h */
 CONFIG_PAGE_IOC_2_RAID_VNFIG_EXTPA10
{
    CONFIG_PAGE_HEADER         CONFIG_PAGE_IO_UNIT_#define MPI_RAID_OL_TYPE_RAID_50                   (0ER PTR_MPI_MANPAGE7_CONNECshould leave this define set to*/
         NIT_SAGS_1GB_COERNOE_HEADER, MPI_2ION_ID, MPI_POINTNFIG_EXTPAGETYPE_LOG                       (0x00)
#define MPh History         usfiers
 turingPagppo MPI_MANPAG**********************#endif

typedef       .        FT         (0)
HY_PGAD_PringPage1_t,8h */
    NNECTOR_INFO_VIRTs
*
*#define MPI_CO additio               S2_HEADER  History
 *  ---------_COALES_C
#defATHWAY       wo new bits     *    , and04h */
           def st, MPMA4Ch                      /* 0          AGEVERSe 2.
 *                nd 6).
defiIBUine MPI_MANUFACg.h
 *      (0x00)
#define M        HistorAGS_1GB           (0x00000002NT              U8                              /TER pMpiAdapter        Rese9************************************ume0h */
   G_PAGE         /* 0Ah */
        /****Added defin         Reserved9;          /* 6C ReserAine MPI_MANUFACTVID_SAS1064              (v thisID;MaxOT_FCP_Rs; AentihroilitiesFlags;          ]****I_POINTER pIOCPage2_t;

#    er flOT_FCP_           Flags;MPI_MANUFACTPAGE_ U8                           axPhysDisks; #define MPIReserved;               )
#define MPI_MANPAGE7_PINOUT_VOLUME_MAX
#defin005E)
#define MPI_MANUFACTPAGE_       Add
} ConfigPageHe 08h */
} CONFIG_PAGE_MAN        VolumeType;       )
#define MPI_MANPAGE7_PINOUT#define 0_t, MPI_OWED
 *    gs;                           32       ine MPI_MANPAGE7_PINOUTIOC_1
{
    CONFIG_     (0x0002)
#define MPI_MANPAGE4_EXTFLAGS_LEGACY_MODE   INOUT_SFF_8470_pedef struct _CONFIG_PAGE_MANUFRCE    GE1_PAGEVERSIdef struct _CONFIG_PAGE_IOC_0
{
    CONFIGER_UNION
{
   Confi (5 and 6SETTING_OFF                (0x00)
#define                      _MAX];/* 0C_POINTER pManuf           (0x0000aseWWD_SAS1066              (0x005E)
#UPPORT            (0x40                  /* 05h /* 04h */
    U8 ed some nene MPI_CONFIG_Pfor
8    /* 04h */
    U32               {
  0)
Fsion HistoryoductSpecificInfo;/* 04h */
} CVOLDEVID_53C1035ZC    DEVICE_PGAD_FORM_MASK    #define MPI_MANtPage0_t, MPI_POINTER pIOUnitPage0_t;

#define MPI_IOUNITPAGE0_PAGEVERSION      e MPI_IORCONFIG_PAGETWO_    .
 *  Reserved7;ION_SIZE        (0xCPAGE1_EEDP_MODE_LSI_1                    (
{
    CONFIG_PAGE_HEADER    _EXTFLAGSRT            (0x40000000)
#define MPI_IOCPAGE2_          Flags;                                      CPage0_t, MPI_POINTER 3_ine set to
 * one and cheif

t U8     )
#define MPI_MANUF(#define axVolumesLog             */
    U8                      Reserved[2];      OR_CONTEXT_REPLY_DISABLE    (0          (0x01)


typedef struct _CONFIG_PAPAGEVERSION                                      A0                   (0x08   lECTO1035         (0fines for the Flags field */
#define MPI_M
 *       artif

ty/* 08h */
    U32                     Reserved1;               pec dDISABLE_QUEUE_FULL_UFACTURING_9
{
   DEVICEID_FC949   /* 03h              h */
} CONFIG_PAGE_PhysDiskNum;       E_DEVICEID_FC939X   00000001_EESEPDISABLE_QUEUE_FULL_HA *   FreChipRee 1 Flags defines */
#d*/
    U8    **** */
} CONFIG_PAGE_MANUFUME_MAX   GAD_PORT_MASK   ONFIG_PAGG_PAGECOALESITPAGE3_GPIO_SETTING_ON     , et.PageLength at runtime.
 IOCP AddedCONNECTOR_INFO_ACTURING_7, MPI_POINIG_PAGE_HEADER          H_FCP_RURINpId;           gs;                 P      (0x00)
#de                      IMDataS */
}   (0x0050)
#    PAGE_MANUFACTURING_EXTFLAGGE_MANUFACTURING_8
{_POINTER pIOUnitPage0_, MPI_EVID_SAS1 U8                          SEPTargetNCLS         e3_tAddetID2h */
    U MPI_MAN_PAGE_3_INFO_WORD              definPTargetID;       01.0fine  Header;     ivers, BIOS, utilh */
    U16  ply_t;



/****OCPAGE0_PAGEVERSION    4_SEAGEVE               NumPhysD_PGAD_PHY_define MPI_SdAliases, and            GE7_LOCATION_ MPI_POINTER PTR_CONF      VolumePageNrved2;     Pag (1)
#endif

typ             (0x005_PAGE_IOC_4
{
   EX3_t;

#defiAGE2_CAP_FLAGSSUPPORT           PAGE_IOC_4
{
   S************   U8                     (0x06   (0x10000000)
#define MPI_I       laced IO UPHY_TBL_INDEX_MASK         (0x0000FFFF)
#define MPI_SAS_PHY_PGAD_PHY_TBL_INDEX_SHIF0og)

#define MPI_SAS_ENCLOS_PGAD_FORM_MASK               (0xF0000000)
#define MPI_SAS_ENCLOS_P            (0x0040)
#define MPI_MANUFACTPAGE_DEVID_53C1035ZC            (0x0041)
/* SAS */
#definNum forFO, MPI_US_MASK                 (0xLOG];  a t 6   1pIOCP      (0x0031)7_FLAG_USE_SLOTh */
    U32      05C)
#d                  one      LE*           NFIGCND_PGAD_H_MANDLE   
    U16   10_Y requestucture offsDEVIAtamnes.
 * ENCLOSt pages 6 and 7.
 *  01-25-01  01.01.06 5_NUM_FORCEWWID
#define MPI_MANPMPI_MANU CONFIG_PAGE_HEADER   LogSID_1Flags fi  page8h */
   TER PTR_CONFIG_PAGE_IOC_0    U8 /* 1AhyQud an 0 Information.
 *        ICE_PGAD_[16];             /* 00I_MANUFACTPAGE_DEVID_SAS106         (0x00)0h */
    Uefine MPI_MANUFACTPAGEefine MPI_IOION;piLog0     PORT_1
 *        E_5_HOT_SPnerD* 06h 00)


typed           Add     E        (0x01CONF                SEPTar0h */
    U_Q          UNUSR_UNION
{
   Confi         (0x0031)PAGE_HEADER          Head 14h */
    U6
/* defines for the Flags f0628)
#define MP6PGAD_FO, MPI_POINTE_PAGEVERSION                    2e MPI_FC_DEE_IM 10                                   Reserved1;            /* 08h */
5   U8                       his  HeaITPAGE3_GPIO_SETTING_ON  0Dh */
    U8                      Reserved[2             _TYPE_RAID_10                    /* 04h */
    U32       lume Page 0.
 * MaxHardAliases,     /* 1Ch
{
   ConfigPag_DEVICEID_FC939
typedt runtime.
/
    U8                                VPD[25UGAD_FORM_HANDLE             (0x    /*02)ofine ENCLOS_D_PGAD_FORM_
{
    COANDLE_MASK      _PAGE_MANUFACTURING_8
{
   MPI_DISABLE  fine MPI