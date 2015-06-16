/*

  Linux Driver for Mylex DAC960/AcceleRAID/eXtremeRAID PCI RAID Controllers

  Copyright 1998-2001 by Leonard N. Zubkoff <lnz@dandelion.com>
  Portions Copyright 2002 by Mylex (An IBM Business Unit)

  This program is free software; you may redistribute and/or modify it under
  the terms of the GNU General Public License Version 2 as published by the
  Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  for complete details.

*/


#define DAC960_DriverVersion			"2.5.49"
#define DAC960_DriverDate			"21 Aug 2007"


#include <linux/module.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/blkpg.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/reboot.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/random.h>
#include <linux/scatterlist.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "DAC960.h"

#define DAC960_GAM_MINOR	252


static DAC960_Controller_T *DAC960_Controllers[DAC960_MaxControllers];
static int DAC960_ControllerCount;
static struct proc_dir_entry *DAC960_ProcDirectoryEntry;

static long disk_size(DAC960_Controller_T *p, int drive_nr)
{
	if (p->FirmwareType == DAC960_V1_Controller) {
		if (drive_nr >= p->LogicalDriveCount)
			return 0;
		return p->V1.LogicalDriveInformation[drive_nr].
			LogicalDriveSize;
	} else {
		DAC960_V2_LogicalDeviceInfo_T *i =
			p->V2.LogicalDeviceInformation[drive_nr];
		if (i == NULL)
			return 0;
		return i->ConfigurableDeviceSize;
	}
}

static int DAC960_open(struct block_device *bdev, fmode_t mode)
{
	struct gendisk *disk = bdev->bd_disk;
	DAC960_Controller_T *p = disk->queue->queuedata;
	int drive_nr = (long)disk->private_data;

	if (p->FirmwareType == DAC960_V1_Controller) {
		if (p->V1.LogicalDriveInformation[drive_nr].
		    LogicalDriveState == DAC960_V1_LogicalDrive_Offline)
			return -ENXIO;
	} else {
		DAC960_V2_LogicalDeviceInfo_T *i =
			p->V2.LogicalDeviceInformation[drive_nr];
		if (!i || i->LogicalDeviceState == DAC960_V2_LogicalDevice_Offline)
			return -ENXIO;
	}

	check_disk_change(bdev);

	if (!get_capacity(p->disks[drive_nr]))
		return -ENXIO;
	return 0;
}

static int DAC960_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct gendisk *disk = bdev->bd_disk;
	DAC960_Controller_T *p = disk->queue->queuedata;
	int drive_nr = (long)disk->private_data;

	if (p->FirmwareType == DAC960_V1_Controller) {
		geo->heads = p->V1.GeometryTranslationHeads;
		geo->sectors = p->V1.GeometryTranslationSectors;
		geo->cylinders = p->V1.LogicalDriveInformation[drive_nr].
			LogicalDriveSize / (geo->heads * geo->sectors);
	} else {
		DAC960_V2_LogicalDeviceInfo_T *i =
			p->V2.LogicalDeviceInformation[drive_nr];
		switch (i->DriveGeometry) {
		case DAC960_V2_Geometry_128_32:
			geo->heads = 128;
			geo->sectors = 32;
			break;
		case DAC960_V2_Geometry_255_63:
			geo->heads = 255;
			geo->sectors = 63;
			break;
		default:
			DAC960_Error("Illegal Logical Device Geometry %d\n",
					p, i->DriveGeometry);
			return -EINVAL;
		}

		geo->cylinders = i->ConfigurableDeviceSize /
			(geo->heads * geo->sectors);
	}
	
	return 0;
}

static int DAC960_media_changed(struct gendisk *disk)
{
	DAC960_Controller_T *p = disk->queue->queuedata;
	int drive_nr = (long)disk->private_data;

	if (!p->LogicalDriveInitiallyAccessible[drive_nr])
		return 1;
	return 0;
}

static int DAC960_revalidate_disk(struct gendisk *disk)
{
	DAC960_Controller_T *p = disk->queue->queuedata;
	int unit = (long)disk->private_data;

	set_capacity(disk, disk_size(p, unit));
	return 0;
}

static const struct block_device_operations DAC960_BlockDeviceOperations = {
	.owner			= THIS_MODULE,
	.open			= DAC960_open,
	.getgeo			= DAC960_getgeo,
	.media_changed		= DAC960_media_changed,
	.revalidate_disk	= DAC960_revalidate_disk,
};


/*
  DAC960_AnnounceDriver announces the Driver Version and Date, Author's Name,
  Copyright Notice, and Electronic Mail Address.
*/

static void DAC960_AnnounceDriver(DAC960_Controller_T *Controller)
{
  DAC960_Announce("***** DAC960 RAID Driver Version "
		  DAC960_DriverVersion " of "
		  DAC960_DriverDate " *****\n", Controller);
  DAC960_Announce("Copyright 1998-2001 by Leonard N. Zubkoff "
		  "<lnz@dandelion.com>\n", Controller);
}


/*
  DAC960_Failure prints a standardized error message, and then returns false.
*/

static bool DAC960_Failure(DAC960_Controller_T *Controller,
			      unsigned char *ErrorMessage)
{
  DAC960_Error("While configuring DAC960 PCI RAID Controller at\n",
	       Controller);
  if (Controller->IO_Address == 0)
    DAC960_Error("PCI Bus %d Device %d Function %d I/O Address N/A "
		 "PCI Address 0x%X\n", Controller,
		 Controller->Bus, Controller->Device,
		 Controller->Function, Controller->PCI_Address);
  else DAC960_Error("PCI Bus %d Device %d Function %d I/O Address "
		    "0x%X PCI Address 0x%X\n", Controller,
		    Controller->Bus, Controller->Device,
		    Controller->Function, Controller->IO_Address,
		    Controller->PCI_Address);
  DAC960_Error("%s FAILED - DETACHING\n", Controller, ErrorMessage);
  return false;
}

/*
  init_dma_loaf() and slice_dma_loaf() are helper functions for
  aggregating the dma-mapped memory for a well-known collection of
  data structures that are of different lengths.

  These routines don't guarantee any alignment.  The caller must
  include any space needed for alignment in the sizes of the structures
  that are passed in.
 */

static bool init_dma_loaf(struct pci_dev *dev, struct dma_loaf *loaf,
								 size_t len)
{
	void *cpu_addr;
	dma_addr_t dma_handle;

	cpu_addr = pci_alloc_consistent(dev, len, &dma_handle);
	if (cpu_addr == NULL)
		return false;
	
	loaf->cpu_free = loaf->cpu_base = cpu_addr;
	loaf->dma_free =loaf->dma_base = dma_handle;
	loaf->length = len;
	memset(cpu_addr, 0, len);
	return true;
}

static void *slice_dma_loaf(struct dma_loaf *loaf, size_t len,
					dma_addr_t *dma_handle)
{
	void *cpu_end = loaf->cpu_free + len;
	void *cpu_addr = loaf->cpu_free;

	BUG_ON(cpu_end > loaf->cpu_base + loaf->length);
	*dma_handle = loaf->dma_free;
	loaf->cpu_free = cpu_end;
	loaf->dma_free += len;
	return cpu_addr;
}

static void free_dma_loaf(struct pci_dev *dev, struct dma_loaf *loaf_handle)
{
	if (loaf_handle->cpu_base != NULL)
		pci_free_consistent(dev, loaf_handle->length,
			loaf_handle->cpu_base, loaf_handle->dma_base);
}


/*
  DAC960_CreateAuxiliaryStructures allocates and initializes the auxiliary
  data structures for Controller.  It returns true on success and false on
  failure.
*/

static bool DAC960_CreateAuxiliaryStructures(DAC960_Controller_T *Controller)
{
  int CommandAllocationLength, CommandAllocationGroupSize;
  int CommandsRemaining = 0, CommandIdentifier, CommandGroupByteCount;
  void *AllocationPointer = NULL;
  void *ScatterGatherCPU = NULL;
  dma_addr_t ScatterGatherDMA;
  struct pci_pool *ScatterGatherPool;
  void *RequestSenseCPU = NULL;
  dma_addr_t RequestSenseDMA;
  struct pci_pool *RequestSensePool = NULL;

  if (Controller->FirmwareType == DAC960_V1_Controller)
    {
      CommandAllocationLength = offsetof(DAC960_Command_T, V1.EndMarker);
      CommandAllocationGroupSize = DAC960_V1_CommandAllocationGroupSize;
      ScatterGatherPool = pci_pool_create("DAC960_V1_ScatterGather",
		Controller->PCIDevice,
	DAC960_V1_ScatterGatherLimit * sizeof(DAC960_V1_ScatterGatherSegment_T),
	sizeof(DAC960_V1_ScatterGatherSegment_T), 0);
      if (ScatterGatherPool == NULL)
	    return DAC960_Failure(Controller,
			"AUXILIARY STRUCTURE CREATION (SG)");
      Controller->ScatterGatherPool = ScatterGatherPool;
    }
  else
    {
      CommandAllocationLength = offsetof(DAC960_Command_T, V2.EndMarker);
      CommandAllocationGroupSize = DAC960_V2_CommandAllocationGroupSize;
      ScatterGatherPool = pci_pool_create("DAC960_V2_ScatterGather",
		Controller->PCIDevice,
	DAC960_V2_ScatterGatherLimit * sizeof(DAC960_V2_ScatterGatherSegment_T),
	sizeof(DAC960_V2_ScatterGatherSegment_T), 0);
      if (ScatterGatherPool == NULL)
	    return DAC960_Failure(Controller,
			"AUXILIARY STRUCTURE CREATION (SG)");
      RequestSensePool = pci_pool_create("DAC960_V2_RequestSense",
		Controller->PCIDevice, sizeof(DAC960_SCSI_RequestSense_T),
		sizeof(int), 0);
      if (RequestSensePool == NULL) {
	    pci_pool_destroy(ScatterGatherPool);
	    return DAC960_Failure(Controller,
			"AUXILIARY STRUCTURE CREATION (SG)");
      }
      Controller->ScatterGatherPool = ScatterGatherPool;
      Controller->V2.RequestSensePool = RequestSensePool;
    }
  Controller->CommandAllocationGroupSize = CommandAllocationGroupSize;
  Controller->FreeCommands = NULL;
  for (CommandIdentifier = 1;
       CommandIdentifier <= Controller->DriverQueueDepth;
       CommandIdentifier++)
    {
      DAC960_Command_T *Command;
      if (--CommandsRemaining <= 0)
	{
	  CommandsRemaining =
		Controller->DriverQueueDepth - CommandIdentifier + 1;
	  if (CommandsRemaining > CommandAllocationGroupSize)
		CommandsRemaining = CommandAllocationGroupSize;
	  CommandGroupByteCount =
		CommandsRemaining * CommandAllocationLength;
	  AllocationPointer = kzalloc(CommandGroupByteCount, GFP_ATOMIC);
	  if (AllocationPointer == NULL)
		return DAC960_Failure(Controller,
					"AUXILIARY STRUCTURE CREATION");
	 }
      Command = (DAC960_Command_T *) AllocationPointer;
      AllocationPointer += CommandAllocationLength;
      Command->CommandIdentifier = CommandIdentifier;
      Command->Controller = Controller;
      Command->Next = Controller->FreeCommands;
      Controller->FreeCommands = Command;
      Controller->Commands[CommandIdentifier-1] = Command;
      ScatterGatherCPU = pci_pool_alloc(ScatterGatherPool, GFP_ATOMIC,
							&ScatterGatherDMA);
      if (ScatterGatherCPU == NULL)
	  return DAC960_Failure(Controller, "AUXILIARY STRUCTURE CREATION");

      if (RequestSensePool != NULL) {
  	  RequestSenseCPU = pci_pool_alloc(RequestSensePool, GFP_ATOMIC,
						&RequestSenseDMA);
  	  if (RequestSenseCPU == NULL) {
                pci_pool_free(ScatterGatherPool, ScatterGatherCPU,
                                ScatterGatherDMA);
    		return DAC960_Failure(Controller,
					"AUXILIARY STRUCTURE CREATION");
	  }
        }
     if (Controller->FirmwareType == DAC960_V1_Controller) {
        Command->cmd_sglist = Command->V1.ScatterList;
	Command->V1.ScatterGatherList =
		(DAC960_V1_ScatterGatherSegment_T *)ScatterGatherCPU;
	Command->V1.ScatterGatherListDMA = ScatterGatherDMA;
	sg_init_table(Command->cmd_sglist, DAC960_V1_ScatterGatherLimit);
      } else {
        Command->cmd_sglist = Command->V2.ScatterList;
	Command->V2.ScatterGatherList =
		(DAC960_V2_ScatterGatherSegment_T *)ScatterGatherCPU;
	Command->V2.ScatterGatherListDMA = ScatterGatherDMA;
	Command->V2.RequestSense =
				(DAC960_SCSI_RequestSense_T *)RequestSenseCPU;
	Command->V2.RequestSenseDMA = RequestSenseDMA;
	sg_init_table(Command->cmd_sglist, DAC960_V2_ScatterGatherLimit);
      }
    }
  return true;
}


/*
  DAC960_DestroyAuxiliaryStructures deallocates the auxiliary data
  structures for Controller.
*/

static void DAC960_DestroyAuxiliaryStructures(DAC960_Controller_T *Controller)
{
  int i;
  struct pci_pool *ScatterGatherPool = Controller->ScatterGatherPool;
  struct pci_pool *RequestSensePool = NULL;
  void *ScatterGatherCPU;
  dma_addr_t ScatterGatherDMA;
  void *RequestSenseCPU;
  dma_addr_t RequestSenseDMA;
  DAC960_Command_T *CommandGroup = NULL;
  

  if (Controller->FirmwareType == DAC960_V2_Controller)
        RequestSensePool = Controller->V2.RequestSensePool;

  Controller->FreeCommands = NULL;
  for (i = 0; i < Controller->DriverQueueDepth; i++)
    {
      DAC960_Command_T *Command = Controller->Commands[i];

      if (Command == NULL)
	  continue;

      if (Controller->FirmwareType == DAC960_V1_Controller) {
	  ScatterGatherCPU = (void *)Command->V1.ScatterGatherList;
	  ScatterGatherDMA = Command->V1.ScatterGatherListDMA;
	  RequestSenseCPU = NULL;
	  RequestSenseDMA = (dma_addr_t)0;
      } else {
          ScatterGatherCPU = (void *)Command->V2.ScatterGatherList;
	  ScatterGatherDMA = Command->V2.ScatterGatherListDMA;
	  RequestSenseCPU = (void *)Command->V2.RequestSense;
	  RequestSenseDMA = Command->V2.RequestSenseDMA;
      }
      if (ScatterGatherCPU != NULL)
          pci_pool_free(ScatterGatherPool, ScatterGatherCPU, ScatterGatherDMA);
      if (RequestSenseCPU != NULL)
          pci_pool_free(RequestSensePool, RequestSenseCPU, RequestSenseDMA);

      if ((Command->CommandIdentifier
	   % Controller->CommandAllocationGroupSize) == 1) {
	   /*
	    * We can't free the group of commands until all of the
	    * request sense and scatter gather dma structures are free.
            * Remember the beginning of the group, but don't free it
	    * until we've reached the beginning of the next group.
	    */
	   kfree(CommandGroup);
	   CommandGroup = Command;
      }
      Controller->Commands[i] = NULL;
    }
  kfree(CommandGroup);

  if (Controller->CombinedStatusBuffer != NULL)
    {
      kfree(Controller->CombinedStatusBuffer);
      Controller->CombinedStatusBuffer = NULL;
      Controller->CurrentStatusBuffer = NULL;
    }

  if (ScatterGatherPool != NULL)
  	pci_pool_destroy(ScatterGatherPool);
  if (Controller->FirmwareType == DAC960_V1_Controller)
  	return;

  if (RequestSensePool != NULL)
	pci_pool_destroy(RequestSensePool);

  for (i = 0; i < DAC960_MaxLogicalDrives; i++) {
	kfree(Controller->V2.LogicalDeviceInformation[i]);
	Controller->V2.LogicalDeviceInformation[i] = NULL;
  }

  for (i = 0; i < DAC960_V2_MaxPhysicalDevices; i++)
    {
      kfree(Controller->V2.PhysicalDeviceInformation[i]);
      Controller->V2.PhysicalDeviceInformation[i] = NULL;
      kfree(Controller->V2.InquiryUnitSerialNumber[i]);
      Controller->V2.InquiryUnitSerialNumber[i] = NULL;
    }
}


/*
  DAC960_V1_ClearCommand clears critical fields of Command for DAC960 V1
  Firmware Controllers.
*/

static inline void DAC960_V1_ClearCommand(DAC960_Command_T *Command)
{
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  memset(CommandMailbox, 0, sizeof(DAC960_V1_CommandMailbox_T));
  Command->V1.CommandStatus = 0;
}


/*
  DAC960_V2_ClearCommand clears critical fields of Command for DAC960 V2
  Firmware Controllers.
*/

static inline void DAC960_V2_ClearCommand(DAC960_Command_T *Command)
{
  DAC960_V2_CommandMailbox_T *CommandMailbox = &Command->V2.CommandMailbox;
  memset(CommandMailbox, 0, sizeof(DAC960_V2_CommandMailbox_T));
  Command->V2.CommandStatus = 0;
}


/*
  DAC960_AllocateCommand allocates a Command structure from Controller's
  free list.  During driver initialization, a special initialization command
  has been placed on the free list to guarantee that command allocation can
  never fail.
*/

static inline DAC960_Command_T *DAC960_AllocateCommand(DAC960_Controller_T
						       *Controller)
{
  DAC960_Command_T *Command = Controller->FreeCommands;
  if (Command == NULL) return NULL;
  Controller->FreeCommands = Command->Next;
  Command->Next = NULL;
  return Command;
}


/*
  DAC960_DeallocateCommand deallocates Command, returning it to Controller's
  free list.
*/

static inline void DAC960_DeallocateCommand(DAC960_Command_T *Command)
{
  DAC960_Controller_T *Controller = Command->Controller;

  Command->Request = NULL;
  Command->Next = Controller->FreeCommands;
  Controller->FreeCommands = Command;
}


/*
  DAC960_WaitForCommand waits for a wake_up on Controller's Command Wait Queue.
*/

static void DAC960_WaitForCommand(DAC960_Controller_T *Controller)
{
  spin_unlock_irq(&Controller->queue_lock);
  __wait_event(Controller->CommandWaitQueue, Controller->FreeCommands);
  spin_lock_irq(&Controller->queue_lock);
}

/*
  DAC960_GEM_QueueCommand queues Command for DAC960 GEM Series Controllers.
*/

static void DAC960_GEM_QueueCommand(DAC960_Command_T *Command)
{
  DAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  DAC960_V2_CommandMailbox_T *CommandMailbox = &Command->V2.CommandMailbox;
  DAC960_V2_CommandMailbox_T *NextCommandMailbox =
      Controller->V2.NextCommandMailbox;

  CommandMailbox->Common.CommandIdentifier = Command->CommandIdentifier;
  DAC960_GEM_WriteCommandMailbox(NextCommandMailbox, CommandMailbox);

  if (Controller->V2.PreviousCommandMailbox1->Words[0] == 0 ||
      Controller->V2.PreviousCommandMailbox2->Words[0] == 0)
      DAC960_GEM_MemoryMailboxNewCommand(ControllerBaseAddress);

  Controller->V2.PreviousCommandMailbox2 =
      Controller->V2.PreviousCommandMailbox1;
  Controller->V2.PreviousCommandMailbox1 = NextCommandMailbox;

  if (++NextCommandMailbox > Controller->V2.LastCommandMailbox)
      NextCommandMailbox = Controller->V2.FirstCommandMailbox;

  Controller->V2.NextCommandMailbox = NextCommandMailbox;
}

/*
  DAC960_BA_QueueCommand queues Command for DAC960 BA Series Controllers.
*/

static void DAC960_BA_QueueCommand(DAC960_Command_T *Command)
{
  DAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  DAC960_V2_CommandMailbox_T *CommandMailbox = &Command->V2.CommandMailbox;
  DAC960_V2_CommandMailbox_T *NextCommandMailbox =
    Controller->V2.NextCommandMailbox;
  CommandMailbox->Common.CommandIdentifier = Command->CommandIdentifier;
  DAC960_BA_WriteCommandMailbox(NextCommandMailbox, CommandMailbox);
  if (Controller->V2.PreviousCommandMailbox1->Words[0] == 0 ||
      Controller->V2.PreviousCommandMailbox2->Words[0] == 0)
    DAC960_BA_MemoryMailboxNewCommand(ControllerBaseAddress);
  Controller->V2.PreviousCommandMailbox2 =
    Controller->V2.PreviousCommandMailbox1;
  Controller->V2.PreviousCommandMailbox1 = NextCommandMailbox;
  if (++NextCommandMailbox > Controller->V2.LastCommandMailbox)
    NextCommandMailbox = Controller->V2.FirstCommandMailbox;
  Controller->V2.NextCommandMailbox = NextCommandMailbox;
}


/*
  DAC960_LP_QueueCommand queues Command for DAC960 LP Series Controllers.
*/

static void DAC960_LP_QueueCommand(DAC960_Command_T *Command)
{
  DAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  DAC960_V2_CommandMailbox_T *CommandMailbox = &Command->V2.CommandMailbox;
  DAC960_V2_CommandMailbox_T *NextCommandMailbox =
    Controller->V2.NextCommandMailbox;
  CommandMailbox->Common.CommandIdentifier = Command->CommandIdentifier;
  DAC960_LP_WriteCommandMailbox(NextCommandMailbox, CommandMailbox);
  if (Controller->V2.PreviousCommandMailbox1->Words[0] == 0 ||
      Controller->V2.PreviousCommandMailbox2->Words[0] == 0)
    DAC960_LP_MemoryMailboxNewCommand(ControllerBaseAddress);
  Controller->V2.PreviousCommandMailbox2 =
    Controller->V2.PreviousCommandMailbox1;
  Controller->V2.PreviousCommandMailbox1 = NextCommandMailbox;
  if (++NextCommandMailbox > Controller->V2.LastCommandMailbox)
    NextCommandMailbox = Controller->V2.FirstCommandMailbox;
  Controller->V2.NextCommandMailbox = NextCommandMailbox;
}


/*
  DAC960_LA_QueueCommandDualMode queues Command for DAC960 LA Series
  Controllers with Dual Mode Firmware.
*/

static void DAC960_LA_QueueCommandDualMode(DAC960_Command_T *Command)
{
  DAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandMailbox_T *NextCommandMailbox =
    Controller->V1.NextCommandMailbox;
  CommandMailbox->Common.CommandIdentifier = Command->CommandIdentifier;
  DAC960_LA_WriteCommandMailbox(NextCommandMailbox, CommandMailbox);
  if (Controller->V1.PreviousCommandMailbox1->Words[0] == 0 ||
      Controller->V1.PreviousCommandMailbox2->Words[0] == 0)
    DAC960_LA_MemoryMailboxNewCommand(ControllerBaseAddress);
  Controller->V1.PreviousCommandMailbox2 =
    Controller->V1.PreviousCommandMailbox1;
  Controller->V1.PreviousCommandMailbox1 = NextCommandMailbox;
  if (++NextCommandMailbox > Controller->V1.LastCommandMailbox)
    NextCommandMailbox = Controller->V1.FirstCommandMailbox;
  Controller->V1.NextCommandMailbox = NextCommandMailbox;
}


/*
  DAC960_LA_QueueCommandSingleMode queues Command for DAC960 LA Series
  Controllers with Single Mode Firmware.
*/

static void DAC960_LA_QueueCommandSingleMode(DAC960_Command_T *Command)
{
  DAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandMailbox_T *NextCommandMailbox =
    Controller->V1.NextCommandMailbox;
  CommandMailbox->Common.CommandIdentifier = Command->CommandIdentifier;
  DAC960_LA_WriteCommandMailbox(NextCommandMailbox, CommandMailbox);
  if (Controller->V1.PreviousCommandMailbox1->Words[0] == 0 ||
      Controller->V1.PreviousCommandMailbox2->Words[0] == 0)
    DAC960_LA_HardwareMailboxNewCommand(ControllerBaseAddress);
  Controller->V1.PreviousCommandMailbox2 =
    Controller->V1.PreviousCommandMailbox1;
  Controller->V1.PreviousCommandMailbox1 = NextCommandMailbox;
  if (++NextCommandMailbox > Controller->V1.LastCommandMailbox)
    NextCommandMailbox = Controller->V1.FirstCommandMailbox;
  Controller->V1.NextCommandMailbox = NextCommandMailbox;
}


/*
  DAC960_PG_QueueCommandDualMode queues Command for DAC960 PG Series
  Controllers with Dual Mode Firmware.
*/

static void DAC960_PG_QueueCommandDualMode(DAC960_Command_T *Command)
{
  DAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandMailbox_T *NextCommandMailbox =
    Controller->V1.NextCommandMailbox;
  CommandMailbox->Common.CommandIdentifier = Command->CommandIdentifier;
  DAC960_PG_WriteCommandMailbox(NextCommandMailbox, CommandMailbox);
  if (Controller->V1.PreviousCommandMailbox1->Words[0] == 0 ||
      Controller->V1.PreviousCommandMailbox2->Words[0] == 0)
    DAC960_PG_MemoryMailboxNewCommand(ControllerBaseAddress);
  Controller->V1.PreviousCommandMailbox2 =
    Controller->V1.PreviousCommandMailbox1;
  Controller->V1.PreviousCommandMailbox1 = NextCommandMailbox;
  if (++NextCommandMailbox > Controller->V1.LastCommandMailbox)
    NextCommandMailbox = Controller->V1.FirstCommandMailbox;
  Controller->V1.NextCommandMailbox = NextCommandMailbox;
}


/*
  DAC960_PG_QueueCommandSingleMode queues Command for DAC960 PG Series
  Controllers with Single Mode Firmware.
*/

static void DAC960_PG_QueueCommandSingleMode(DAC960_Command_T *Command)
{
  DAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandMailbox_T *NextCommandMailbox =
    Controller->V1.NextCommandMailbox;
  CommandMailbox->Common.CommandIdentifier = Command->CommandIdentifier;
  DAC960_PG_WriteCommandMailbox(NextCommandMailbox, CommandMailbox);
  if (Controller->V1.PreviousCommandMailbox1->Words[0] == 0 ||
      Controller->V1.PreviousCommandMailbox2->Words[0] == 0)
    DAC960_PG_HardwareMailboxNewCommand(ControllerBaseAddress);
  Controller->V1.PreviousCommandMailbox2 =
    Controller->V1.PreviousCommandMailbox1;
  Controller->V1.PreviousCommandMailbox1 = NextCommandMailbox;
  if (++NextCommandMailbox > Controller->V1.LastCommandMailbox)
    NextCommandMailbox = Controller->V1.FirstCommandMailbox;
  Controller->V1.NextCommandMailbox = NextCommandMailbox;
}


/*
  DAC960_PD_QueueCommand queues Command for DAC960 PD Series Controllers.
*/

static void DAC960_PD_QueueCommand(DAC960_Command_T *Command)
{
  DAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  CommandMailbox->Common.CommandIdentifier = Command->CommandIdentifier;
  while (DAC960_PD_MailboxFullP(ControllerBaseAddress))
    udelay(1);
  DAC960_PD_WriteCommandMailbox(ControllerBaseAddress, CommandMailbox);
  DAC960_PD_NewCommand(ControllerBaseAddress);
}


/*
  DAC960_P_QueueCommand queues Command for DAC960 P Series Controllers.
*/

static void DAC960_P_QueueCommand(DAC960_Command_T *Command)
{
  DAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  CommandMailbox->Common.CommandIdentifier = Command->CommandIdentifier;
  switch (CommandMailbox->Common.CommandOpcode)
    {
    case DAC960_V1_Enquiry:
      CommandMailbox->Common.CommandOpcode = DAC960_V1_Enquiry_Old;
      break;
    case DAC960_V1_GetDeviceState:
      CommandMailbox->Common.CommandOpcode = DAC960_V1_GetDeviceState_Old;
      break;
    case DAC960_V1_Read:
      CommandMailbox->Common.CommandOpcode = DAC960_V1_Read_Old;
      DAC960_PD_To_P_TranslateReadWriteCommand(CommandMailbox);
      break;
    case DAC960_V1_Write:
      CommandMailbox->Common.CommandOpcode = DAC960_V1_Write_Old;
      DAC960_PD_To_P_TranslateReadWriteCommand(CommandMailbox);
      break;
    case DAC960_V1_ReadWithScatterGather:
      CommandMailbox->Common.CommandOpcode =
	DAC960_V1_ReadWithScatterGather_Old;
      DAC960_PD_To_P_TranslateReadWriteCommand(CommandMailbox);
      break;
    case DAC960_V1_WriteWithScatterGather:
      CommandMailbox->Common.CommandOpcode =
	DAC960_V1_WriteWithScatterGather_Old;
      DAC960_PD_To_P_TranslateReadWriteCommand(CommandMailbox);
      break;
    default:
      break;
    }
  while (DAC960_PD_MailboxFullP(ControllerBaseAddress))
    udelay(1);
  DAC960_PD_WriteCommandMailbox(ControllerBaseAddress, CommandMailbox);
  DAC960_PD_NewCommand(ControllerBaseAddress);
}


/*
  DAC960_ExecuteCommand executes Command and waits for completion.
*/

static void DAC960_ExecuteCommand(DAC960_Command_T *Command)
{
  DAC960_Controller_T *Controller = Command->Controller;
  DECLARE_COMPLETION_ONSTACK(Completion);
  unsigned long flags;
  Command->Completion = &Completion;

  spin_lock_irqsave(&Controller->queue_lock, flags);
  DAC960_QueueCommand(Command);
  spin_unlock_irqrestore(&Controller->queue_lock, flags);
 
  if (in_interrupt())
	  return;
  wait_for_completion(&Completion);
}


/*
  DAC960_V1_ExecuteType3 executes a DAC960 V1 Firmware Controller Type 3
  Command and waits for completion.  It returns true on success and false
  on failure.
*/

static bool DAC960_V1_ExecuteType3(DAC960_Controller_T *Controller,
				      DAC960_V1_CommandOpcode_T CommandOpcode,
				      dma_addr_t DataDMA)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandStatus_T CommandStatus;
  DAC960_V1_ClearCommand(Command);
  Command->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->Type3.CommandOpcode = CommandOpcode;
  CommandMailbox->Type3.BusAddress = DataDMA;
  DAC960_ExecuteCommand(Command);
  CommandStatus = Command->V1.CommandStatus;
  DAC960_DeallocateCommand(Command);
  return (CommandStatus == DAC960_V1_NormalCompletion);
}


/*
  DAC960_V1_ExecuteTypeB executes a DAC960 V1 Firmware Controller Type 3B
  Command and waits for completion.  It returns true on success and false
  on failure.
*/

static bool DAC960_V1_ExecuteType3B(DAC960_Controller_T *Controller,
				       DAC960_V1_CommandOpcode_T CommandOpcode,
				       unsigned char CommandOpcode2,
				       dma_addr_t DataDMA)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandStatus_T CommandStatus;
  DAC960_V1_ClearCommand(Command);
  Command->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->Type3B.CommandOpcode = CommandOpcode;
  CommandMailbox->Type3B.CommandOpcode2 = CommandOpcode2;
  CommandMailbox->Type3B.BusAddress = DataDMA;
  DAC960_ExecuteCommand(Command);
  CommandStatus = Command->V1.CommandStatus;
  DAC960_DeallocateCommand(Command);
  return (CommandStatus == DAC960_V1_NormalCompletion);
}


/*
  DAC960_V1_ExecuteType3D executes a DAC960 V1 Firmware Controller Type 3D
  Command and waits for completion.  It returns true on success and false
  on failure.
*/

static bool DAC960_V1_ExecuteType3D(DAC960_Controller_T *Controller,
				       DAC960_V1_CommandOpcode_T CommandOpcode,
				       unsigned char Channel,
				       unsigned char TargetID,
				       dma_addr_t DataDMA)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandStatus_T CommandStatus;
  DAC960_V1_ClearCommand(Command);
  Command->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->Type3D.CommandOpcode = CommandOpcode;
  CommandMailbox->Type3D.Channel = Channel;
  CommandMailbox->Type3D.TargetID = TargetID;
  CommandMailbox->Type3D.BusAddress = DataDMA;
  DAC960_ExecuteCommand(Command);
  CommandStatus = Command->V1.CommandStatus;
  DAC960_DeallocateCommand(Command);
  return (CommandStatus == DAC960_V1_NormalCompletion);
}


/*
  DAC960_V2_GeneralInfo executes a DAC960 V2 Firmware General Information
  Reading IOCTL Command and waits for completion.  It returns true on success
  and false on failure.

  Return data in The controller's HealthStatusBuffer, which is dma-able memory
*/

static bool DAC960_V2_GeneralInfo(DAC960_Controller_T *Controller)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V2_CommandMailbox_T *CommandMailbox = &Command->V2.CommandMailbox;
  DAC960_V2_CommandStatus_T CommandStatus;
  DAC960_V2_ClearCommand(Command);
  Command->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->Common.CommandOpcode = DAC960_V2_IOCTL;
  CommandMailbox->Common.CommandControlBits
			.DataTransferControllerToHost = true;
  CommandMailbox->Common.CommandControlBits
			.NoAutoRequestSense = true;
  CommandMailbox->Common.DataTransferSize = sizeof(DAC960_V2_HealthStatusBuffer_T);
  CommandMailbox->Common.IOCTL_Opcode = DAC960_V2_GetHealthStatus;
  CommandMailbox->Common.DataTransferMemoryAddress
			.ScatterGatherSegments[0]
			.SegmentDataPointer =
    Controller->V2.HealthStatusBufferDMA;
  CommandMailbox->Common.DataTransferMemoryAddress
			.ScatterGatherSegments[0]
			.SegmentByteCount =
    CommandMailbox->Common.DataTransferSize;
  DAC960_ExecuteCommand(Command);
  CommandStatus = Command->V2.CommandStatus;
  DAC960_DeallocateCommand(Command);
  return (CommandStatus == DAC960_V2_NormalCompletion);
}


/*
  DAC960_V2_ControllerInfo executes a DAC960 V2 Firmware Controller
  Information Reading IOCTL Command and waits for completion.  It returns
  true on success and false on failure.

  Data is returned in the controller's V2.NewControllerInformation dma-able
  memory buffer.
*/

static bool DAC960_V2_NewControllerInfo(DAC960_Controller_T *Controller)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V2_CommandMailbox_T *CommandMailbox = &Command->V2.CommandMailbox;
  DAC960_V2_CommandStatus_T CommandStatus;
  DAC960_V2_ClearCommand(Command);
  Command->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->ControllerInfo.CommandOpcode = DAC960_V2_IOCTL;
  CommandMailbox->ControllerInfo.CommandControlBits
				.DataTransferControllerToHost = true;
  CommandMailbox->ControllerInfo.CommandControlBits
				.NoAutoRequestSense = true;
  CommandMailbox->ControllerInfo.DataTransferSize = sizeof(DAC960_V2_ControllerInfo_T);
  CommandMailbox->ControllerInfo.ControllerNumber = 0;
  CommandMailbox->ControllerInfo.IOCTL_Opcode = DAC960_V2_GetControllerInfo;
  CommandMailbox->ControllerInfo.DataTransferMemoryAddress
				.ScatterGatherSegments[0]
				.SegmentDataPointer =
    	Controller->V2.NewControllerInformationDMA;
  CommandMailbox->ControllerInfo.DataTransferMemoryAddress
				.ScatterGatherSegments[0]
				.SegmentByteCount =
    CommandMailbox->ControllerInfo.DataTransferSize;
  DAC960_ExecuteCommand(Command);
  CommandStatus = Command->V2.CommandStatus;
  DAC960_DeallocateCommand(Command);
  return (CommandStatus == DAC960_V2_NormalCompletion);
}


/*
  DAC960_V2_LogicalDeviceInfo executes a DAC960 V2 Firmware Controller Logical
  Device Information Reading IOCTL Command and waits for completion.  It
  returns true on success and false on failure.

  Data is returned in the controller's V2.NewLogicalDeviceInformation
*/

static bool DAC960_V2_NewLogicalDeviceInfo(DAC960_Controller_T *Controller,
					   unsigned short LogicalDeviceNumber)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V2_CommandMailbox_T *CommandMailbox = &Command->V2.CommandMailbox;
  DAC960_V2_CommandStatus_T CommandStatus;

  DAC960_V2_ClearCommand(Command);
  Command->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->LogicalDeviceInfo.CommandOpcode =
				DAC960_V2_IOCTL;
  CommandMailbox->LogicalDeviceInfo.CommandControlBits
				   .DataTransferControllerToHost = true;
  CommandMailbox->LogicalDeviceInfo.CommandControlBits
				   .NoAutoRequestSense = true;
  CommandMailbox->LogicalDeviceInfo.DataTransferSize = 
				sizeof(DAC960_V2_LogicalDeviceInfo_T);
  CommandMailbox->LogicalDeviceInfo.LogicalDevice.LogicalDeviceNumber =
    LogicalDeviceNumber;
  CommandMailbox->LogicalDeviceInfo.IOCTL_Opcode = DAC960_V2_GetLogicalDeviceInfoValid;
  CommandMailbox->LogicalDeviceInfo.DataTransferMemoryAddress
				   .ScatterGatherSegments[0]
				   .SegmentDataPointer =
    	Controller->V2.NewLogicalDeviceInformationDMA;
  CommandMailbox->LogicalDeviceInfo.DataTransferMemoryAddress
				   .ScatterGatherSegments[0]
				   .SegmentByteCount =
    CommandMailbox->LogicalDeviceInfo.DataTransferSize;
  DAC960_ExecuteCommand(Command);
  CommandStatus = Command->V2.CommandStatus;
  DAC960_DeallocateCommand(Command);
  return (CommandStatus == DAC960_V2_NormalCompletion);
}


/*
  DAC960_V2_PhysicalDeviceInfo executes a DAC960 V2 Firmware Controller "Read
  Physical Device Information" IOCTL Command and waits for completion.  It
  returns true on success and false on failure.

  The Channel, TargetID, LogicalUnit arguments should be 0 the first time
  this function is called for a given controller.  This will return data
  for the "first" device on that controller.  The returned data includes a
  Channel, TargetID, LogicalUnit that can be passed in to this routine to
  get data for the NEXT device on that controller.

  Data is stored in the controller's V2.NewPhysicalDeviceInfo dma-able
  memory buffer.

*/

static bool DAC960_V2_NewPhysicalDeviceInfo(DAC960_Controller_T *Controller,
					    unsigned char Channel,
					    unsigned char TargetID,
					    unsigned char LogicalUnit)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V2_CommandMailbox_T *CommandMailbox = &Command->V2.CommandMailbox;
  DAC960_V2_CommandStatus_T CommandStatus;

  DAC960_V2_ClearCommand(Command);
  Command->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->PhysicalDeviceInfo.CommandOpcode = DAC960_V2_IOCTL;
  CommandMailbox->PhysicalDeviceInfo.CommandControlBits
				    .DataTransferControllerToHost = true;
  CommandMailbox->PhysicalDeviceInfo.CommandControlBits
				    .NoAutoRequestSense = true;
  CommandMailbox->PhysicalDeviceInfo.DataTransferSize =
				sizeof(DAC960_V2_PhysicalDeviceInfo_T);
  CommandMailbox->PhysicalDeviceInfo.PhysicalDevice.LogicalUnit = LogicalUnit;
  CommandMailbox->PhysicalDeviceInfo.PhysicalDevice.TargetID = TargetID;
  CommandMailbox->PhysicalDeviceInfo.PhysicalDevice.Channel = Channel;
  CommandMailbox->PhysicalDeviceInfo.IOCTL_Opcode =
					DAC960_V2_GetPhysicalDeviceInfoValid;
  CommandMailbox->PhysicalDeviceInfo.DataTransferMemoryAddress
				    .ScatterGatherSegments[0]
				    .SegmentDataPointer =
    					Controller->V2.NewPhysicalDeviceInformationDMA;
  CommandMailbox->PhysicalDeviceInfo.DataTransferMemoryAddress
				    .ScatterGatherSegments[0]
				    .SegmentByteCount =
    CommandMailbox->PhysicalDeviceInfo.DataTransferSize;
  DAC960_ExecuteCommand(Command);
  CommandStatus = Command->V2.CommandStatus;
  DAC960_DeallocateCommand(Command);
  return (CommandStatus == DAC960_V2_NormalCompletion);
}


static void DAC960_V2_ConstructNewUnitSerialNumber(
	DAC960_Controller_T *Controller,
	DAC960_V2_CommandMailbox_T *CommandMailbox, int Channel, int TargetID,
	int LogicalUnit)
{
      CommandMailbox->SCSI_10.CommandOpcode = DAC960_V2_SCSI_10_Passthru;
      CommandMailbox->SCSI_10.CommandControlBits
			     .DataTransferControllerToHost = true;
      CommandMailbox->SCSI_10.CommandControlBits
			     .NoAutoRequestSense = true;
      CommandMailbox->SCSI_10.DataTransferSize =
	sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T);
      CommandMailbox->SCSI_10.PhysicalDevice.LogicalUnit = LogicalUnit;
      CommandMailbox->SCSI_10.PhysicalDevice.TargetID = TargetID;
      CommandMailbox->SCSI_10.PhysicalDevice.Channel = Channel;
      CommandMailbox->SCSI_10.CDBLength = 6;
      CommandMailbox->SCSI_10.SCSI_CDB[0] = 0x12; /* INQUIRY */
      CommandMailbox->SCSI_10.SCSI_CDB[1] = 1; /* EVPD = 1 */
      CommandMailbox->SCSI_10.SCSI_CDB[2] = 0x80; /* Page Code */
      CommandMailbox->SCSI_10.SCSI_CDB[3] = 0; /* Reserved */
      CommandMailbox->SCSI_10.SCSI_CDB[4] =
	sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T);
      CommandMailbox->SCSI_10.SCSI_CDB[5] = 0; /* Control */
      CommandMailbox->SCSI_10.DataTransferMemoryAddress
			     .ScatterGatherSegments[0]
			     .SegmentDataPointer =
		Controller->V2.NewInquiryUnitSerialNumberDMA;
      CommandMailbox->SCSI_10.DataTransferMemoryAddress
			     .ScatterGatherSegments[0]
			     .SegmentByteCount =
		CommandMailbox->SCSI_10.DataTransferSize;
}


/*
  DAC960_V2_NewUnitSerialNumber executes an SCSI pass-through
  Inquiry command to a SCSI device identified by Channel number,
  Target id, Logical Unit Number.  This function Waits for completion
  of the command.

  The return data includes Unit Serial Number information for the
  specified device.

  Data is stored in the controller's V2.NewPhysicalDeviceInfo dma-able
  memory buffer.
*/

static bool DAC960_V2_NewInquiryUnitSerialNumber(DAC960_Controller_T *Controller,
			int Channel, int TargetID, int LogicalUnit)
{
      DAC960_Command_T *Command;
      DAC960_V2_CommandMailbox_T *CommandMailbox;
      DAC960_V2_CommandStatus_T CommandStatus;

      Command = DAC960_AllocateCommand(Controller);
      CommandMailbox = &Command->V2.CommandMailbox;
      DAC960_V2_ClearCommand(Command);
      Command->CommandType = DAC960_ImmediateCommand;

      DAC960_V2_ConstructNewUnitSerialNumber(Controller, CommandMailbox,
			Channel, TargetID, LogicalUnit);

      DAC960_ExecuteCommand(Command);
      CommandStatus = Command->V2.CommandStatus;
      DAC960_DeallocateCommand(Command);
      return (CommandStatus == DAC960_V2_NormalCompletion);
}


/*
  DAC960_V2_DeviceOperation executes a DAC960 V2 Firmware Controller Device
  Operation IOCTL Command and waits for completion.  It returns true on
  success and false on failure.
*/

static bool DAC960_V2_DeviceOperation(DAC960_Controller_T *Controller,
					 DAC960_V2_IOCTL_Opcode_T IOCTL_Opcode,
					 DAC960_V2_OperationDevice_T
					   OperationDevice)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V2_CommandMailbox_T *CommandMailbox = &Command->V2.CommandMailbox;
  DAC960_V2_CommandStatus_T CommandStatus;
  DAC960_V2_ClearCommand(Command);
  Command->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->DeviceOperation.CommandOpcode = DAC960_V2_IOCTL;
  CommandMailbox->DeviceOperation.CommandControlBits
				 .DataTransferControllerToHost = true;
  CommandMailbox->DeviceOperation.CommandControlBits
    				 .NoAutoRequestSense = true;
  CommandMailbox->DeviceOperation.IOCTL_Opcode = IOCTL_Opcode;
  CommandMailbox->DeviceOperation.OperationDevice = OperationDevice;
  DAC960_ExecuteCommand(Command);
  CommandStatus = Command->V2.CommandStatus;
  DAC960_DeallocateCommand(Command);
  return (CommandStatus == DAC960_V2_NormalCompletion);
}


/*
  DAC960_V1_EnableMemoryMailboxInterface enables the Memory Mailbox Interface
  for DAC960 V1 Firmware Controllers.

  PD and P controller types have no memory mailbox, but still need the
  other dma mapped memory.
*/

static bool DAC960_V1_EnableMemoryMailboxInterface(DAC960_Controller_T
						      *Controller)
{
  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  DAC960_HardwareType_T hw_type = Controller->HardwareType;
  struct pci_dev *PCI_Device = Controller->PCIDevice;
  struct dma_loaf *DmaPages = &Controller->DmaPages;
  size_t DmaPagesSize;
  size_t CommandMailboxesSize;
  size_t StatusMailboxesSize;

  DAC960_V1_CommandMailbox_T *CommandMailboxesMemory;
  dma_addr_t CommandMailboxesMemoryDMA;

  DAC960_V1_StatusMailbox_T *StatusMailboxesMemory;
  dma_addr_t StatusMailboxesMemoryDMA;

  DAC960_V1_CommandMailbox_T CommandMailbox;
  DAC960_V1_CommandStatus_T CommandStatus;
  int TimeoutCounter;
  int i;

  
  if (pci_set_dma_mask(Controller->PCIDevice, DMA_BIT_MASK(32)))
	return DAC960_Failure(Controller, "DMA mask out of range");
  Controller->BounceBufferLimit = DMA_BIT_MASK(32);

  if ((hw_type == DAC960_PD_Controller) || (hw_type == DAC960_P_Controller)) {
    CommandMailboxesSize =  0;
    StatusMailboxesSize = 0;
  } else {
    CommandMailboxesSize =  DAC960_V1_CommandMailboxCount * sizeof(DAC960_V1_CommandMailbox_T);
    StatusMailboxesSize = DAC960_V1_StatusMailboxCount * sizeof(DAC960_V1_StatusMailbox_T);
  }
  DmaPagesSize = CommandMailboxesSize + StatusMailboxesSize + 
	sizeof(DAC960_V1_DCDB_T) + sizeof(DAC960_V1_Enquiry_T) +
	sizeof(DAC960_V1_ErrorTable_T) + sizeof(DAC960_V1_EventLogEntry_T) +
	sizeof(DAC960_V1_RebuildProgress_T) +
	sizeof(DAC960_V1_LogicalDriveInformationArray_T) +
	sizeof(DAC960_V1_BackgroundInitializationStatus_T) +
	sizeof(DAC960_V1_DeviceState_T) + sizeof(DAC960_SCSI_Inquiry_T) +
	sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T);

  if (!init_dma_loaf(PCI_Device, DmaPages, DmaPagesSize))
	return false;


  if ((hw_type == DAC960_PD_Controller) || (hw_type == DAC960_P_Controller)) 
	goto skip_mailboxes;

  CommandMailboxesMemory = slice_dma_loaf(DmaPages,
                CommandMailboxesSize, &CommandMailboxesMemoryDMA);
  
  /* These are the base addresses for the command memory mailbox array */
  Controller->V1.FirstCommandMailbox = CommandMailboxesMemory;
  Controller->V1.FirstCommandMailboxDMA = CommandMailboxesMemoryDMA;

  CommandMailboxesMemory += DAC960_V1_CommandMailboxCount - 1;
  Controller->V1.LastCommandMailbox = CommandMailboxesMemory;
  Controller->V1.NextCommandMailbox = Controller->V1.FirstCommandMailbox;
  Controller->V1.PreviousCommandMailbox1 = Controller->V1.LastCommandMailbox;
  Controller->V1.PreviousCommandMailbox2 =
	  				Controller->V1.LastCommandMailbox - 1;

  /* These are the base addresses for the status memory mailbox array */
  StatusMailboxesMemory = slice_dma_loaf(DmaPages,
                StatusMailboxesSize, &StatusMailboxesMemoryDMA);

  Controller->V1.FirstStatusMailbox = StatusMailboxesMemory;
  Controller->V1.FirstStatusMailboxDMA = StatusMailboxesMemoryDMA;
  StatusMailboxesMemory += DAC960_V1_StatusMailboxCount - 1;
  Controller->V1.LastStatusMailbox = StatusMailboxesMemory;
  Controller->V1.NextStatusMailbox = Controller->V1.FirstStatusMailbox;

skip_mailboxes:
  Controller->V1.MonitoringDCDB = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V1_DCDB_T),
                &Controller->V1.MonitoringDCDB_DMA);

  Controller->V1.NewEnquiry = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V1_Enquiry_T),
                &Controller->V1.NewEnquiryDMA);

  Controller->V1.NewErrorTable = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V1_ErrorTable_T),
                &Controller->V1.NewErrorTableDMA);

  Controller->V1.EventLogEntry = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V1_EventLogEntry_T),
                &Controller->V1.EventLogEntryDMA);

  Controller->V1.RebuildProgress = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V1_RebuildProgress_T),
                &Controller->V1.RebuildProgressDMA);

  Controller->V1.NewLogicalDriveInformation = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V1_LogicalDriveInformationArray_T),
                &Controller->V1.NewLogicalDriveInformationDMA);

  Controller->V1.BackgroundInitializationStatus = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V1_BackgroundInitializationStatus_T),
                &Controller->V1.BackgroundInitializationStatusDMA);

  Controller->V1.NewDeviceState = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V1_DeviceState_T),
                &Controller->V1.NewDeviceStateDMA);

  Controller->V1.NewInquiryStandardData = slice_dma_loaf(DmaPages,
                sizeof(DAC960_SCSI_Inquiry_T),
                &Controller->V1.NewInquiryStandardDataDMA);

  Controller->V1.NewInquiryUnitSerialNumber = slice_dma_loaf(DmaPages,
                sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T),
                &Controller->V1.NewInquiryUnitSerialNumberDMA);

  if ((hw_type == DAC960_PD_Controller) || (hw_type == DAC960_P_Controller))
	return true;
 
  /* Enable the Memory Mailbox Interface. */
  Controller->V1.DualModeMemoryMailboxInterface = true;
  CommandMailbox.TypeX.CommandOpcode = 0x2B;
  CommandMailbox.TypeX.CommandIdentifier = 0;
  CommandMailbox.TypeX.CommandOpcode2 = 0x14;
  CommandMailbox.TypeX.CommandMailboxesBusAddress =
    				Controller->V1.FirstCommandMailboxDMA;
  CommandMailbox.TypeX.StatusMailboxesBusAddress =
    				Controller->V1.FirstStatusMailboxDMA;
#define TIMEOUT_COUNT 1000000

  for (i = 0; i < 2; i++)
    switch (Controller->HardwareType)
      {
      case DAC960_LA_Controller:
	TimeoutCounter = TIMEOUT_COUNT;
	while (--TimeoutCounter >= 0)
	  {
	    if (!DAC960_LA_HardwareMailboxFullP(ControllerBaseAddress))
	      break;
	    udelay(10);
	  }
	if (TimeoutCounter < 0) return false;
	DAC960_LA_WriteHardwareMailbox(ControllerBaseAddress, &CommandMailbox);
	DAC960_LA_HardwareMailboxNewCommand(ControllerBaseAddress);
	TimeoutCounter = TIMEOUT_COUNT;
	while (--TimeoutCounter >= 0)
	  {
	    if (DAC960_LA_HardwareMailboxStatusAvailableP(
		  ControllerBaseAddress))
	      break;
	    udelay(10);
	  }
	if (TimeoutCounter < 0) return false;
	CommandStatus = DAC960_LA_ReadStatusRegister(ControllerBaseAddress);
	DAC960_LA_AcknowledgeHardwareMailboxInterrupt(ControllerBaseAddress);
	DAC960_LA_AcknowledgeHardwareMailboxStatus(ControllerBaseAddress);
	if (CommandStatus == DAC960_V1_NormalCompletion) return true;
	Controller->V1.DualModeMemoryMailboxInterface = false;
	CommandMailbox.TypeX.CommandOpcode2 = 0x10;
	break;
      case DAC960_PG_Controller:
	TimeoutCounter = TIMEOUT_COUNT;
	while (--TimeoutCounter >= 0)
	  {
	    if (!DAC960_PG_HardwareMailboxFullP(ControllerBaseAddress))
	      break;
	    udelay(10);
	  }
	if (TimeoutCounter < 0) return false;
	DAC960_PG_WriteHardwareMailbox(ControllerBaseAddress, &CommandMailbox);
	DAC960_PG_HardwareMailboxNewCommand(ControllerBaseAddress);

	TimeoutCounter = TIMEOUT_COUNT;
	while (--TimeoutCounter >= 0)
	  {
	    if (DAC960_PG_HardwareMailboxStatusAvailableP(
		  ControllerBaseAddress))
	      break;
	    udelay(10);
	  }
	if (TimeoutCounter < 0) return false;
	CommandStatus = DAC960_PG_ReadStatusRegister(ControllerBaseAddress);
	DAC960_PG_AcknowledgeHardwareMailboxInterrupt(ControllerBaseAddress);
	DAC960_PG_AcknowledgeHardwareMailboxStatus(ControllerBaseAddress);
	if (CommandStatus == DAC960_V1_NormalCompletion) return true;
	Controller->V1.DualModeMemoryMailboxInterface = false;
	CommandMailbox.TypeX.CommandOpcode2 = 0x10;
	break;
      default:
        DAC960_Failure(Controller, "Unknown Controller Type\n");
	break;
      }
  return false;
}


/*
  DAC960_V2_EnableMemoryMailboxInterface enables the Memory Mailbox Interface
  for DAC960 V2 Firmware Controllers.

  Aggregate the space needed for the controller's memory mailbox and
  the other data structures that will be targets of dma transfers with
  the controller.  Allocate a dma-mapped region of memory to hold these
  structures.  Then, save CPU pointers and dma_addr_t values to reference
  the structures that are contained in that region.
*/

static bool DAC960_V2_EnableMemoryMailboxInterface(DAC960_Controller_T
						      *Controller)
{
  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  struct pci_dev *PCI_Device = Controller->PCIDevice;
  struct dma_loaf *DmaPages = &Controller->DmaPages;
  size_t DmaPagesSize;
  size_t CommandMailboxesSize;
  size_t StatusMailboxesSize;

  DAC960_V2_CommandMailbox_T *CommandMailboxesMemory;
  dma_addr_t CommandMailboxesMemoryDMA;

  DAC960_V2_StatusMailbox_T *StatusMailboxesMemory;
  dma_addr_t StatusMailboxesMemoryDMA;

  DAC960_V2_CommandMailbox_T *CommandMailbox;
  dma_addr_t	CommandMailboxDMA;
  DAC960_V2_CommandStatus_T CommandStatus;

	if (!pci_set_dma_mask(Controller->PCIDevice, DMA_BIT_MASK(64)))
		Controller->BounceBufferLimit = DMA_BIT_MASK(64);
	else if (!pci_set_dma_mask(Controller->PCIDevice, DMA_BIT_MASK(32)))
		Controller->BounceBufferLimit = DMA_BIT_MASK(32);
	else
		return DAC960_Failure(Controller, "DMA mask out of range");

  /* This is a temporary dma mapping, used only in the scope of this function */
  CommandMailbox = pci_alloc_consistent(PCI_Device,
		sizeof(DAC960_V2_CommandMailbox_T), &CommandMailboxDMA);
  if (CommandMailbox == NULL)
	  return false;

  CommandMailboxesSize = DAC960_V2_CommandMailboxCount * sizeof(DAC960_V2_CommandMailbox_T);
  StatusMailboxesSize = DAC960_V2_StatusMailboxCount * sizeof(DAC960_V2_StatusMailbox_T);
  DmaPagesSize =
    CommandMailboxesSize + StatusMailboxesSize +
    sizeof(DAC960_V2_HealthStatusBuffer_T) +
    sizeof(DAC960_V2_ControllerInfo_T) +
    sizeof(DAC960_V2_LogicalDeviceInfo_T) +
    sizeof(DAC960_V2_PhysicalDeviceInfo_T) +
    sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T) +
    sizeof(DAC960_V2_Event_T) +
    sizeof(DAC960_V2_PhysicalToLogicalDevice_T);

  if (!init_dma_loaf(PCI_Device, DmaPages, DmaPagesSize)) {
  	pci_free_consistent(PCI_Device, sizeof(DAC960_V2_CommandMailbox_T),
					CommandMailbox, CommandMailboxDMA);
	return false;
  }

  CommandMailboxesMemory = slice_dma_loaf(DmaPages,
		CommandMailboxesSize, &CommandMailboxesMemoryDMA);

  /* These are the base addresses for the command memory mailbox array */
  Controller->V2.FirstCommandMailbox = CommandMailboxesMemory;
  Controller->V2.FirstCommandMailboxDMA = CommandMailboxesMemoryDMA;

  CommandMailboxesMemory += DAC960_V2_CommandMailboxCount - 1;
  Controller->V2.LastCommandMailbox = CommandMailboxesMemory;
  Controller->V2.NextCommandMailbox = Controller->V2.FirstCommandMailbox;
  Controller->V2.PreviousCommandMailbox1 = Controller->V2.LastCommandMailbox;
  Controller->V2.PreviousCommandMailbox2 =
    					Controller->V2.LastCommandMailbox - 1;

  /* These are the base addresses for the status memory mailbox array */
  StatusMailboxesMemory = slice_dma_loaf(DmaPages,
		StatusMailboxesSize, &StatusMailboxesMemoryDMA);

  Controller->V2.FirstStatusMailbox = StatusMailboxesMemory;
  Controller->V2.FirstStatusMailboxDMA = StatusMailboxesMemoryDMA;
  StatusMailboxesMemory += DAC960_V2_StatusMailboxCount - 1;
  Controller->V2.LastStatusMailbox = StatusMailboxesMemory;
  Controller->V2.NextStatusMailbox = Controller->V2.FirstStatusMailbox;

  Controller->V2.HealthStatusBuffer = slice_dma_loaf(DmaPages,
		sizeof(DAC960_V2_HealthStatusBuffer_T),
		&Controller->V2.HealthStatusBufferDMA);

  Controller->V2.NewControllerInformation = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V2_ControllerInfo_T), 
                &Controller->V2.NewControllerInformationDMA);

  Controller->V2.NewLogicalDeviceInformation =  slice_dma_loaf(DmaPages,
                sizeof(DAC960_V2_LogicalDeviceInfo_T),
                &Controller->V2.NewLogicalDeviceInformationDMA);

  Controller->V2.NewPhysicalDeviceInformation = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V2_PhysicalDeviceInfo_T),
                &Controller->V2.NewPhysicalDeviceInformationDMA);

  Controller->V2.NewInquiryUnitSerialNumber = slice_dma_loaf(DmaPages,
                sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T),
                &Controller->V2.NewInquiryUnitSerialNumberDMA);

  Controller->V2.Event = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V2_Event_T),
                &Controller->V2.EventDMA);

  Controller->V2.PhysicalToLogicalDevice = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V2_PhysicalToLogicalDevice_T),
                &Controller->V2.PhysicalToLogicalDeviceDMA);

  /*
    Enable the Memory Mailbox Interface.
    
    I don't know why we can't just use one of the memory mailboxes
    we just allocated to do this, instead of using this temporary one.
    Try this change later.
  */
  memset(CommandMailbox, 0, sizeof(DAC960_V2_CommandMailbox_T));
  CommandMailbox->SetMemoryMailbox.CommandIdentifier = 1;
  CommandMailbox->SetMemoryMailbox.CommandOpcode = DAC960_V2_IOCTL;
  CommandMailbox->SetMemoryMailbox.CommandControlBits.NoAutoRequestSense = true;
  CommandMailbox->SetMemoryMailbox.FirstCommandMailboxSizeKB =
    (DAC960_V2_CommandMailboxCount * sizeof(DAC960_V2_CommandMailbox_T)) >> 10;
  CommandMailbox->SetMemoryMailbox.FirstStatusMailboxSizeKB =
    (DAC960_V2_StatusMailboxCount * sizeof(DAC960_V2_StatusMailbox_T)) >> 10;
  CommandMailbox->SetMemoryMailbox.SecondCommandMailboxSizeKB = 0;
  CommandMailbox->SetMemoryMailbox.SecondStatusMailboxSizeKB = 0;
  CommandMailbox->SetMemoryMailbox.RequestSenseSize = 0;
  CommandMailbox->SetMemoryMailbox.IOCTL_Opcode = DAC960_V2_SetMemoryMailbox;
  CommandMailbox->SetMemoryMailbox.HealthStatusBufferSizeKB = 1;
  CommandMailbox->SetMemoryMailbox.HealthStatusBufferBusAddress =
    					Controller->V2.HealthStatusBufferDMA;
  CommandMailbox->SetMemoryMailbox.FirstCommandMailboxBusAddress =
    					Controller->V2.FirstCommandMailboxDMA;
  CommandMailbox->SetMemoryMailbox.FirstStatusMailboxBusAddress =
    					Controller->V2.FirstStatusMailboxDMA;
  switch (Controller->HardwareType)
    {
    case DAC960_GEM_Controller:
      while (DAC960_GEM_HardwareMailboxFullP(ControllerBaseAddress))
	udelay(1);
      DAC960_GEM_WriteHardwareMailbox(ControllerBaseAddress, CommandMailboxDMA);
      DAC960_GEM_HardwareMailboxNewCommand(ControllerBaseAddress);
      while (!DAC960_GEM_HardwareMailboxStatusAvailableP(ControllerBaseAddress))
	udelay(1);
      CommandStatus = DAC960_GEM_ReadCommandStatus(ControllerBaseAddress);
      DAC960_GEM_AcknowledgeHardwareMailboxInterrupt(ControllerBaseAddress);
      DAC960_GEM_AcknowledgeHardwareMailboxStatus(ControllerBaseAddress);
      break;
    case DAC960_BA_Controller:
      while (DAC960_BA_HardwareMailboxFullP(ControllerBaseAddress))
	udelay(1);
      DAC960_BA_WriteHardwareMailbox(ControllerBaseAddress, CommandMailboxDMA);
      DAC960_BA_HardwareMailboxNewCommand(ControllerBaseAddress);
      while (!DAC960_BA_HardwareMailboxStatusAvailableP(ControllerBaseAddress))
	udelay(1);
      CommandStatus = DAC960_BA_ReadCommandStatus(ControllerBaseAddress);
      DAC960_BA_AcknowledgeHardwareMailboxInterrupt(ControllerBaseAddress);
      DAC960_BA_AcknowledgeHardwareMailboxStatus(ControllerBaseAddress);
      break;
    case DAC960_LP_Controller:
      while (DAC960_LP_HardwareMailboxFullP(ControllerBaseAddress))
	udelay(1);
      DAC960_LP_WriteHardwareMailbox(ControllerBaseAddress, CommandMailboxDMA);
      DAC960_LP_HardwareMailboxNewCommand(ControllerBaseAddress);
      while (!DAC960_LP_HardwareMailboxStatusAvailableP(ControllerBaseAddress))
	udelay(1);
      CommandStatus = DAC960_LP_ReadCommandStatus(ControllerBaseAddress);
      DAC960_LP_AcknowledgeHardwareMailboxInterrupt(ControllerBaseAddress);
      DAC960_LP_AcknowledgeHardwareMailboxStatus(ControllerBaseAddress);
      break;
    default:
      DAC960_Failure(Controller, "Unknown Controller Type\n");
      CommandStatus = DAC960_V2_AbormalCompletion;
      break;
    }
  pci_free_consistent(PCI_Device, sizeof(DAC960_V2_CommandMailbox_T),
					CommandMailbox, CommandMailboxDMA);
  return (CommandStatus == DAC960_V2_NormalCompletion);
}


/*
  DAC960_V1_ReadControllerConfiguration reads the Configuration Information
  from DAC960 V1 Firmware Controllers and initializes the Controller structure.
*/

static bool DAC960_V1_ReadControllerConfiguration(DAC960_Controller_T
						     *Controller)
{
  DAC960_V1_Enquiry2_T *Enquiry2;
  dma_addr_t Enquiry2DMA;
  DAC960_V1_Config2_T *Config2;
  dma_addr_t Config2DMA;
  int LogicalDriveNumber, Channel, TargetID;
  struct dma_loaf local_dma;

  if (!init_dma_loaf(Controller->PCIDevice, &local_dma,
		sizeof(DAC960_V1_Enquiry2_T) + sizeof(DAC960_V1_Config2_T)))
	return DAC960_Failure(Controller, "LOGICAL DEVICE ALLOCATION");

  Enquiry2 = slice_dma_loaf(&local_dma, sizeof(DAC960_V1_Enquiry2_T), &Enquiry2DMA);
  Config2 = slice_dma_loaf(&local_dma, sizeof(DAC960_V1_Config2_T), &Config2DMA);

  if (!DAC960_V1_ExecuteType3(Controller, DAC960_V1_Enquiry,
			      Controller->V1.NewEnquiryDMA)) {
    free_dma_loaf(Controller->PCIDevice, &local_dma);
    return DAC960_Failure(Controller, "ENQUIRY");
  }
  memcpy(&Controller->V1.Enquiry, Controller->V1.NewEnquiry,
						sizeof(DAC960_V1_Enquiry_T));

  if (!DAC960_V1_ExecuteType3(Controller, DAC960_V1_Enquiry2, Enquiry2DMA)) {
    free_dma_loaf(Controller->PCIDevice, &local_dma);
    return DAC960_Failure(Controller, "ENQUIRY2");
  }

  if (!DAC960_V1_ExecuteType3(Controller, DAC960_V1_ReadConfig2, Config2DMA)) {
    free_dma_loaf(Controller->PCIDevice, &local_dma);
    return DAC960_Failure(Controller, "READ CONFIG2");
  }

  if (!DAC960_V1_ExecuteType3(Controller, DAC960_V1_GetLogicalDriveInformation,
			      Controller->V1.NewLogicalDriveInformationDMA)) {
    free_dma_loaf(Controller->PCIDevice, &local_dma);
    return DAC960_Failure(Controller, "GET LOGICAL DRIVE INFORMATION");
  }
  memcpy(&Controller->V1.LogicalDriveInformation,
		Controller->V1.NewLogicalDriveInformation,
		sizeof(DAC960_V1_LogicalDriveInformationArray_T));

  for (Channel = 0; Channel < Enquiry2->ActualChannels; Channel++)
    for (TargetID = 0; TargetID < Enquiry2->MaxTargets; TargetID++) {
      if (!DAC960_V1_ExecuteType3D(Controller, DAC960_V1_GetDeviceState,
				   Channel, TargetID,
				   Controller->V1.NewDeviceStateDMA)) {
    		free_dma_loaf(Controller->PCIDevice, &local_dma);
		return DAC960_Failure(Controller, "GET DEVICE STATE");
	}
	memcpy(&Controller->V1.DeviceState[Channel][TargetID],
		Controller->V1.NewDeviceState, sizeof(DAC960_V1_DeviceState_T));
     }
  /*
    Initialize the Controller Model Name and Full Model Name fields.
  */
  switch (Enquiry2->HardwareID.SubModel)
    {
    case DAC960_V1_P_PD_PU:
      if (Enquiry2->SCSICapability.BusSpeed == DAC960_V1_Ultra)
	strcpy(Controller->ModelName, "DAC960PU");
      else strcpy(Controller->ModelName, "DAC960PD");
      break;
    case DAC960_V1_PL:
      strcpy(Controller->ModelName, "DAC960PL");
      break;
    case DAC960_V1_PG:
      strcpy(Controller->ModelName, "DAC960PG");
      break;
    case DAC960_V1_PJ:
      strcpy(Controller->ModelName, "DAC960PJ");
      break;
    case DAC960_V1_PR:
      strcpy(Controller->ModelName, "DAC960PR");
      break;
    case DAC960_V1_PT:
      strcpy(Controller->ModelName, "DAC960PT");
      break;
    case DAC960_V1_PTL0:
      strcpy(Controller->ModelName, "DAC960PTL0");
      break;
    case DAC960_V1_PRL:
      strcpy(Controller->ModelName, "DAC960PRL");
      break;
    case DAC960_V1_PTL1:
      strcpy(Controller->ModelName, "DAC960PTL1");
      break;
    case DAC960_V1_1164P:
      strcpy(Controller->ModelName, "DAC1164P");
      break;
    default:
      free_dma_loaf(Controller->PCIDevice, &local_dma);
      return DAC960_Failure(Controller, "MODEL VERIFICATION");
    }
  strcpy(Controller->FullModelName, "Mylex ");
  strcat(Controller->FullModelName, Controller->ModelName);
  /*
    Initialize the Controller Firmware Version field and verify that it
    is a supported firmware version.  The supported firmware versions are:

    DAC1164P		    5.06 and above
    DAC960PTL/PRL/PJ/PG	    4.06 and above
    DAC960PU/PD/PL	    3.51 and above
    DAC960PU/PD/PL/P	    2.73 and above
  */
#if defined(CONFIG_ALPHA)
  /*
    DEC Alpha machines were often equipped with DAC960 cards that were
    OEMed from Mylex, and had their own custom firmware. Version 2.70,
    the last custom FW revision to be released by DEC for these older
    controllers, appears to work quite well with this driver.

    Cards tested successfully were several versions each of the PD and
    PU, called by DEC the KZPSC and KZPAC, respectively, and having
    the Manufacturer Numbers (from Mylex), usually on a sticker on the
    back of the board, of:

    KZPSC:  D040347 (1-channel) or D040348 (2-channel) or D040349 (3-channel)
    KZPAC:  D040395 (1-channel) or D040396 (2-channel) or D040397 (3-channel)
  */
# define FIRMWARE_27X	"2.70"
#else
# define FIRMWARE_27X	"2.73"
#endif

  if (Enquiry2->FirmwareID.MajorVersion == 0)
    {
      Enquiry2->FirmwareID.MajorVersion =
	Controller->V1.Enquiry.MajorFirmwareVersion;
      Enquiry2->FirmwareID.MinorVersion =
	Controller->V1.Enquiry.MinorFirmwareVersion;
      Enquiry2->FirmwareID.FirmwareType = '0';
      Enquiry2->FirmwareID.TurnID = 0;
    }
  sprintf(Controller->FirmwareVersion, "%d.%02d-%c-%02d",
	  Enquiry2->FirmwareID.MajorVersion, Enquiry2->FirmwareID.MinorVersion,
	  Enquiry2->FirmwareID.FirmwareType, Enquiry2->FirmwareID.TurnID);
  if (!((Controller->FirmwareVersion[0] == '5' &&
	 strcmp(Controller->FirmwareVersion, "5.06") >= 0) ||
	(Controller->FirmwareVersion[0] == '4' &&
	 strcmp(Controller->FirmwareVersion, "4.06") >= 0) ||
	(Controller->FirmwareVersion[0] == '3' &&
	 strcmp(Controller->FirmwareVersion, "3.51") >= 0) ||
	(Controller->FirmwareVersion[0] == '2' &&
	 strcmp(Controller->FirmwareVersion, FIRMWARE_27X) >= 0)))
    {
      DAC960_Failure(Controller, "FIRMWARE VERSION VERIFICATION");
      DAC960_Error("Firmware Version = '%s'\n", Controller,
		   Controller->FirmwareVersion);
      free_dma_loaf(Controller->PCIDevice, &local_dma);
      return false;
    }
  /*
    Initialize the Controller Channels, Targets, Memory Size, and SAF-TE
    Enclosure Management Enabled fields.
  */
  Controller->Channels = Enquiry2->ActualChannels;
  Controller->Targets = Enquiry2->MaxTargets;
  Controller->MemorySize = Enquiry2->MemorySize >> 20;
  Controller->V1.SAFTE_EnclosureManagementEnabled =
    (Enquiry2->FaultManagementType == DAC960_V1_SAFTE);
  /*
    Initialize the Controller Queue Depth, Driver Queue Depth, Logical Drive
    Count, Maximum Blocks per Command, Controller Scatter/Gather Limit, and
    Driver Scatter/Gather Limit.  The Driver Queue Depth must be at most one
    less than the Controller Queue Depth to allow for an automatic drive
    rebuild operation.
  */
  Controller->ControllerQueueDepth = Controller->V1.Enquiry.MaxCommands;
  Controller->DriverQueueDepth = Controller->ControllerQueueDepth - 1;
  if (Controller->DriverQueueDepth > DAC960_MaxDriverQueueDepth)
    Controller->DriverQueueDepth = DAC960_MaxDriverQueueDepth;
  Controller->LogicalDriveCount =
    Controller->V1.Enquiry.NumberOfLogicalDrives;
  Controller->MaxBlocksPerCommand = Enquiry2->MaxBlocksPerCommand;
  Controller->ControllerScatterGatherLimit = Enquiry2->MaxScatterGatherEntries;
  Controller->DriverScatterGatherLimit =
    Controller->ControllerScatterGatherLimit;
  if (Controller->DriverScatterGatherLimit > DAC960_V1_ScatterGatherLimit)
    Controller->DriverScatterGatherLimit = DAC960_V1_ScatterGatherLimit;
  /*
    Initialize the Stripe Size, Segment Size, and Geometry Translation.
  */
  Controller->V1.StripeSize = Config2->BlocksPerStripe * Config2->BlockFactor
			      >> (10 - DAC960_BlockSizeBits);
  Controller->V1.SegmentSize = Config2->BlocksPerCacheLine * Config2->BlockFactor
			       >> (10 - DAC960_BlockSizeBits);
  switch (Config2->DriveGeometry)
    {
    case DAC960_V1_Geometry_128_32:
      Controller->V1.GeometryTranslationHeads = 128;
      Controller->V1.GeometryTranslationSectors = 32;
      break;
    case DAC960_V1_Geometry_255_63:
      Controller->V1.GeometryTranslationHeads = 255;
      Controller->V1.GeometryTranslationSectors = 63;
      break;
    default:
      free_dma_loaf(Controller->PCIDevice, &local_dma);
      return DAC960_Failure(Controller, "CONFIG2 DRIVE GEOMETRY");
    }
  /*
    Initialize the Background Initialization Status.
  */
  if ((Controller->FirmwareVersion[0] == '4' &&
      strcmp(Controller->FirmwareVersion, "4.08") >= 0) ||
      (Controller->FirmwareVersion[0] == '5' &&
       strcmp(Controller->FirmwareVersion, "5.08") >= 0))
    {
      Controller->V1.BackgroundInitializationStatusSupported = true;
      DAC960_V1_ExecuteType3B(Controller,
			      DAC960_V1_BackgroundInitializationControl, 0x20,
			      Controller->
			       V1.BackgroundInitializationStatusDMA);
      memcpy(&Controller->V1.LastBackgroundInitializationStatus,
		Controller->V1.BackgroundInitializationStatus,
		sizeof(DAC960_V1_BackgroundInitializationStatus_T));
    }
  /*
    Initialize the Logical Drive Initially Accessible flag.
  */
  for (LogicalDriveNumber = 0;
       LogicalDriveNumber < Controller->LogicalDriveCount;
       LogicalDriveNumber++)
    if (Controller->V1.LogicalDriveInformation
		       [LogicalDriveNumber].LogicalDriveState !=
	DAC960_V1_LogicalDrive_Offline)
      Controller->LogicalDriveInitiallyAccessible[LogicalDriveNumber] = true;
  Controller->V1.LastRebuildStatus = DAC960_V1_NoRebuildOrCheckInProgress;
  free_dma_loaf(Controller->PCIDevice, &local_dma);
  return true;
}


/*
  DAC960_V2_ReadControllerConfiguration reads the Configuration Information
  from DAC960 V2 Firmware Controllers and initializes the Controller structure.
*/

static bool DAC960_V2_ReadControllerConfiguration(DAC960_Controller_T
						     *Controller)
{
  DAC960_V2_ControllerInfo_T *ControllerInfo =
    		&Controller->V2.ControllerInformation;
  unsigned short LogicalDeviceNumber = 0;
  int ModelNameLength;

  /* Get data into dma-able area, then copy into permanant location */
  if (!DAC960_V2_NewControllerInfo(Controller))
    return DAC960_Failure(Controller, "GET CONTROLLER INFO");
  memcpy(ControllerInfo, Controller->V2.NewControllerInformation,
			sizeof(DAC960_V2_ControllerInfo_T));
	 
  
  if (!DAC960_V2_GeneralInfo(Controller))
    return DAC960_Failure(Controller, "GET HEALTH STATUS");

  /*
    Initialize the Controller Model Name and Full Model Name fields.
  */
  ModelNameLength = sizeof(ControllerInfo->ControllerName);
  if (ModelNameLength > sizeof(Controller->ModelName)-1)
    ModelNameLength = sizeof(Controller->ModelName)-1;
  memcpy(Controller->ModelName, ControllerInfo->ControllerName,
	 ModelNameLength);
  ModelNameLength--;
  while (Controller->ModelName[ModelNameLength] == ' ' ||
	 Controller->ModelName[ModelNameLength] == '\0')
    ModelNameLength--;
  Controller->ModelName[++ModelNameLength] = '\0';
  strcpy(Controller->FullModelName, "Mylex ");
  strcat(Controller->FullModelName, Controller->ModelName);
  /*
    Initialize the Controller Firmware Version field.
  */
  sprintf(Controller->FirmwareVersion, "%d.%02d-%02d",
	  ControllerInfo->FirmwareMajorVersion,
	  ControllerInfo->FirmwareMinorVersion,
	  ControllerInfo->FirmwareTurnNumber);
  if (ControllerInfo->FirmwareMajorVersion == 6 &&
      ControllerInfo->FirmwareMinorVersion == 0 &&
      ControllerInfo->FirmwareTurnNumber < 1)
    {
      DAC960_Info("FIRMWARE VERSION %s DOES NOT PROVIDE THE CONTROLLER\n",
		  Controller, Controller->FirmwareVersion);
      DAC960_Info("STATUS MONITORING FUNCTIONALITY NEEDED BY THIS DRIVER.\n",
		  Controller);
      DAC960_Info("PLEASE UPGRADE TO VERSION 6.00-01 OR ABOVE.\n",
		  Controller);
    }
  /*
    Initialize the Controller Channels, Targets, and Memory Size.
  */
  Controller->Channels = ControllerInfo->NumberOfPhysicalChannelsPresent;
  Controller->Targets =
    ControllerInfo->MaximumTargetsPerChannel
		    [ControllerInfo->NumberOfPhysicalChannelsPresent-1];
  Controller->MemorySize = ControllerInfo->MemorySizeMB;
  /*
    Initialize the Controller Queue Depth, Driver Queue Depth, Logical Drive
    Count, Maximum Blocks per Command, Controller Scatter/Gather Limit, and
    Driver Scatter/Gather Limit.  The Driver Queue Depth must be at most one
    less than the Controller Queue Depth to allow for an automatic drive
    rebuild operation.
  */
  Controller->ControllerQueueDepth = ControllerInfo->MaximumParallelCommands;
  Controller->DriverQueueDepth = Controller->ControllerQueueDepth - 1;
  if (Controller->DriverQueueDepth > DAC960_MaxDriverQueueDepth)
    Controller->DriverQueueDepth = DAC960_MaxDriverQueueDepth;
  Controller->LogicalDriveCount = ControllerInfo->LogicalDevicesPresent;
  Controller->MaxBlocksPerCommand =
    ControllerInfo->MaximumDataTransferSizeInBlocks;
  Controller->ControllerScatterGatherLimit =
    ControllerInfo->MaximumScatterGatherEntries;
  Controller->DriverScatterGatherLimit =
    Controller->ControllerScatterGatherLimit;
  if (Controller->DriverScatterGatherLimit > DAC960_V2_ScatterGatherLimit)
    Controller->DriverScatterGatherLimit = DAC960_V2_ScatterGatherLimit;
  /*
    Initialize the Logical Device Information.
  */
  while (true)
    {
      DAC960_V2_LogicalDeviceInfo_T *NewLogicalDeviceInfo =
	Controller->V2.NewLogicalDeviceInformation;
      DAC960_V2_LogicalDeviceInfo_T *LogicalDeviceInfo;
      DAC960_V2_PhysicalDevice_T PhysicalDevice;

      if (!DAC960_V2_NewLogicalDeviceInfo(Controller, LogicalDeviceNumber))
	break;
      LogicalDeviceNumber = NewLogicalDeviceInfo->LogicalDeviceNumber;
      if (LogicalDeviceNumber >= DAC960_MaxLogicalDrives) {
	DAC960_Error("DAC960: Logical Drive Number %d not supported\n",
		       Controller, LogicalDeviceNumber);
		break;
      }
      if (NewLogicalDeviceInfo->DeviceBlockSizeInBytes != DAC960_BlockSize) {
	DAC960_Error("DAC960: Logical Drive Block Size %d not supported\n",
	      Controller, NewLogicalDeviceInfo->DeviceBlockSizeInBytes);
        LogicalDeviceNumber++;
        continue;
      }
      PhysicalDevice.Controller = 0;
      PhysicalDevice.Channel = NewLogicalDeviceInfo->Channel;
      PhysicalDevice.TargetID = NewLogicalDeviceInfo->TargetID;
      PhysicalDevice.LogicalUnit = NewLogicalDeviceInfo->LogicalUnit;
      Controller->V2.LogicalDriveToVirtualDevice[LogicalDeviceNumber] =
	PhysicalDevice;
      if (NewLogicalDeviceInfo->LogicalDeviceState !=
	  DAC960_V2_LogicalDevice_Offline)
	Controller->LogicalDriveInitiallyAccessible[LogicalDeviceNumber] = true;
      LogicalDeviceInfo = kmalloc(sizeof(DAC960_V2_LogicalDeviceInfo_T),
				   GFP_ATOMIC);
      if (LogicalDeviceInfo == NULL)
	return DAC960_Failure(Controller, "LOGICAL DEVICE ALLOCATION");
      Controller->V2.LogicalDeviceInformation[LogicalDeviceNumber] =
	LogicalDeviceInfo;
      memcpy(LogicalDeviceInfo, NewLogicalDeviceInfo,
	     sizeof(DAC960_V2_LogicalDeviceInfo_T));
      LogicalDeviceNumber++;
    }
  return true;
}


/*
  DAC960_ReportControllerConfiguration reports the Configuration Information
  for Controller.
*/

static bool DAC960_ReportControllerConfiguration(DAC960_Controller_T
						    *Controller)
{
  DAC960_Info("Configuring Mylex %s PCI RAID Controller\n",
	      Controller, Controller->ModelName);
  DAC960_Info("  Firmware Version: %s, Channels: %d, Memory Size: %dMB\n",
	      Controller, Controller->FirmwareVersion,
	      Controller->Channels, Controller->MemorySize);
  DAC960_Info("  PCI Bus: %d, Device: %d, Function: %d, I/O Address: ",
	      Controller, Controller->Bus,
	      Controller->Device, Controller->Function);
  if (Controller->IO_Address == 0)
    DAC960_Info("Unassigned\n", Controller);
  else DAC960_Info("0x%X\n", Controller, Controller->IO_Address);
  DAC960_Info("  PCI Address: 0x%X mapped at 0x%lX, IRQ Channel: %d\n",
	      Controller, Controller->PCI_Address,
	      (unsigned long) Controller->BaseAddress,
	      Controller->IRQ_Channel);
  DAC960_Info("  Controller Queue Depth: %d, "
	      "Maximum Blocks per Command: %d\n",
	      Controller, Controller->ControllerQueueDepth,
	      Controller->MaxBlocksPerCommand);
  DAC960_Info("  Driver Queue Depth: %d, "
	      "Scatter/Gather Limit: %d of %d Segments\n",
	      Controller, Controller->DriverQueueDepth,
	      Controller->DriverScatterGatherLimit,
	      Controller->ControllerScatterGatherLimit);
  if (Controller->FirmwareType == DAC960_V1_Controller)
    {
      DAC960_Info("  Stripe Size: %dKB, Segment Size: %dKB, "
		  "BIOS Geometry: %d/%d\n", Controller,
		  Controller->V1.StripeSize,
		  Controller->V1.SegmentSize,
		  Controller->V1.GeometryTranslationHeads,
		  Controller->V1.GeometryTranslationSectors);
      if (Controller->V1.SAFTE_EnclosureManagementEnabled)
	DAC960_Info("  SAF-TE Enclosure Management Enabled\n", Controller);
    }
  return true;
}


/*
  DAC960_V1_ReadDeviceConfiguration reads the Device Configuration Information
  for DAC960 V1 Firmware Controllers by requesting the SCSI Inquiry and SCSI
  Inquiry Unit Serial Number information for each device connected to
  Controller.
*/

static bool DAC960_V1_ReadDeviceConfiguration(DAC960_Controller_T
						 *Controller)
{
  struct dma_loaf local_dma;

  dma_addr_t DCDBs_dma[DAC960_V1_MaxChannels];
  DAC960_V1_DCDB_T *DCDBs_cpu[DAC960_V1_MaxChannels];

  dma_addr_t SCSI_Inquiry_dma[DAC960_V1_MaxChannels];
  DAC960_SCSI_Inquiry_T *SCSI_Inquiry_cpu[DAC960_V1_MaxChannels];

  dma_addr_t SCSI_NewInquiryUnitSerialNumberDMA[DAC960_V1_MaxChannels];
  DAC960_SCSI_Inquiry_UnitSerialNumber_T *SCSI_NewInquiryUnitSerialNumberCPU[DAC960_V1_MaxChannels];

  struct completion Completions[DAC960_V1_MaxChannels];
  unsigned long flags;
  int Channel, TargetID;

  if (!init_dma_loaf(Controller->PCIDevice, &local_dma, 
		DAC960_V1_MaxChannels*(sizeof(DAC960_V1_DCDB_T) +
			sizeof(DAC960_SCSI_Inquiry_T) +
			sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T))))
     return DAC960_Failure(Controller,
                        "DMA ALLOCATION FAILED IN ReadDeviceConfiguration"); 
   
  for (Channel = 0; Channel < Controller->Channels; Channel++) {
	DCDBs_cpu[Channel] = slice_dma_loaf(&local_dma,
			sizeof(DAC960_V1_DCDB_T), DCDBs_dma + Channel);
	SCSI_Inquiry_cpu[Channel] = slice_dma_loaf(&local_dma,
			sizeof(DAC960_SCSI_Inquiry_T),
			SCSI_Inquiry_dma + Channel);
	SCSI_NewInquiryUnitSerialNumberCPU[Channel] = slice_dma_loaf(&local_dma,
			sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T),
			SCSI_NewInquiryUnitSerialNumberDMA + Channel);
  }
		
  for (TargetID = 0; TargetID < Controller->Targets; TargetID++)
    {
      /*
       * For each channel, submit a probe for a device on that channel.
       * The timeout interval for a device that is present is 10 seconds.
       * With this approach, the timeout periods can elapse in parallel
       * on each channel.
       */
      for (Channel = 0; Channel < Controller->Channels; Channel++)
	{
	  dma_addr_t NewInquiryStandardDataDMA = SCSI_Inquiry_dma[Channel];
  	  DAC960_V1_DCDB_T *DCDB = DCDBs_cpu[Channel];
  	  dma_addr_t DCDB_dma = DCDBs_dma[Channel];
	  DAC960_Command_T *Command = Controller->Commands[Channel];
          struct completion *Completion = &Completions[Channel];

	  init_completion(Completion);
	  DAC960_V1_ClearCommand(Command);
	  Command->CommandType = DAC960_ImmediateCommand;
	  Command->Completion = Completion;
	  Command->V1.CommandMailbox.Type3.CommandOpcode = DAC960_V1_DCDB;
	  Command->V1.CommandMailbox.Type3.BusAddress = DCDB_dma;
	  DCDB->Channel = Channel;
	  DCDB->TargetID = TargetID;
	  DCDB->Direction = DAC960_V1_DCDB_DataTransferDeviceToSystem;
	  DCDB->EarlyStatus = false;
	  DCDB->Timeout = DAC960_V1_DCDB_Timeout_10_seconds;
	  DCDB->NoAutomaticRequestSense = false;
	  DCDB->DisconnectPermitted = true;
	  DCDB->TransferLength = sizeof(DAC960_SCSI_Inquiry_T);
	  DCDB->BusAddress = NewInquiryStandardDataDMA;
	  DCDB->CDBLength = 6;
	  DCDB->TransferLengthHigh4 = 0;
	  DCDB->SenseLength = sizeof(DCDB->SenseData);
	  DCDB->CDB[0] = 0x12; /* INQUIRY */
	  DCDB->CDB[1] = 0; /* EVPD = 0 */
	  DCDB->CDB[2] = 0; /* Page Code */
	  DCDB->CDB[3] = 0; /* Reserved */
	  DCDB->CDB[4] = sizeof(DAC960_SCSI_Inquiry_T);
	  DCDB->CDB[5] = 0; /* Control */

	  spin_lock_irqsave(&Controller->queue_lock, flags);
	  DAC960_QueueCommand(Command);
	  spin_unlock_irqrestore(&Controller->queue_lock, flags);
	}
      /*
       * Wait for the problems submitted in the previous loop
       * to complete.  On the probes that are successful, 
       * get the serial number of the device that was found.
       */
      for (Channel = 0; Channel < Controller->Channels; Channel++)
	{
	  DAC960_SCSI_Inquiry_T *InquiryStandardData =
	    &Controller->V1.InquiryStandardData[Channel][TargetID];
	  DAC960_SCSI_Inquiry_T *NewInquiryStandardData = SCSI_Inquiry_cpu[Channel];
	  dma_addr_t NewInquiryUnitSerialNumberDMA =
			SCSI_NewInquiryUnitSerialNumberDMA[Channel];
	  DAC960_SCSI_Inquiry_UnitSerialNumber_T *NewInquiryUnitSerialNumber =
	    		SCSI_NewInquiryUnitSerialNumberCPU[Channel];
	  DAC960_SCSI_Inquiry_UnitSerialNumber_T *InquiryUnitSerialNumber =
	    &Controller->V1.InquiryUnitSerialNumber[Channel][TargetID];
	  DAC960_Command_T *Command = Controller->Commands[Channel];
  	  DAC960_V1_DCDB_T *DCDB = DCDBs_cpu[Channel];
          struct completion *Completion = &Completions[Channel];

	  wait_for_completion(Completion);

	  if (Command->V1.CommandStatus != DAC960_V1_NormalCompletion) {
	    memset(InquiryStandardData, 0, sizeof(DAC960_SCSI_Inquiry_T));
	    InquiryStandardData->PeripheralDeviceType = 0x1F;
	    continue;
	  } else
	    memcpy(InquiryStandardData, NewInquiryStandardData, sizeof(DAC960_SCSI_Inquiry_T));
	
	  /* Preserve Channel and TargetID values from the previous loop */
	  Command->Completion = Completion;
	  DCDB->TransferLength = sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T);
	  DCDB->BusAddress = NewInquiryUnitSerialNumberDMA;
	  DCDB->SenseLength = sizeof(DCDB->SenseData);
	  DCDB->CDB[0] = 0x12; /* INQUIRY */
	  DCDB->CDB[1] = 1; /* EVPD = 1 */
	  DCDB->CDB[2] = 0x80; /* Page Code */
	  DCDB->CDB[3] = 0; /* Reserved */
	  DCDB->CDB[4] = sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T);
	  DCDB->CDB[5] = 0; /* Control */

	  spin_lock_irqsave(&Controller->queue_lock, flags);
	  DAC960_QueueCommand(Command);
	  spin_unlock_irqrestore(&Controller->queue_lock, flags);
	  wait_for_completion(Completion);

	  if (Command->V1.CommandStatus != DAC960_V1_NormalCompletion) {
	  	memset(InquiryUnitSerialNumber, 0,
			sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T));
	  	InquiryUnitSerialNumber->PeripheralDeviceType = 0x1F;
	  } else
	  	memcpy(InquiryUnitSerialNumber, NewInquiryUnitSerialNumber,
			sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T));
	}
    }
    free_dma_loaf(Controller->PCIDevice, &local_dma);
  return true;
}


/*
  DAC960_V2_ReadDeviceConfiguration reads the Device Configuration Information
  for DAC960 V2 Firmware Controllers by requesting the Physical Device
  Information and SCSI Inquiry Unit Serial Number information for each
  device connected to Controller.
*/

static bool DAC960_V2_ReadDeviceConfiguration(DAC960_Controller_T
						 *Controller)
{
  unsigned char Channel = 0, TargetID = 0, LogicalUnit = 0;
  unsigned short PhysicalDeviceIndex = 0;

  while (true)
    {
      DAC960_V2_PhysicalDeviceInfo_T *NewPhysicalDeviceInfo =
		Controller->V2.NewPhysicalDeviceInformation;
      DAC960_V2_PhysicalDeviceInfo_T *PhysicalDeviceInfo;
      DAC960_SCSI_Inquiry_UnitSerialNumber_T *NewInquiryUnitSerialNumber =
		Controller->V2.NewInquiryUnitSerialNumber;
      DAC960_SCSI_Inquiry_UnitSerialNumber_T *InquiryUnitSerialNumber;

      if (!DAC960_V2_NewPhysicalDeviceInfo(Controller, Channel, TargetID, LogicalUnit))
	  break;

      PhysicalDeviceInfo = kmalloc(sizeof(DAC960_V2_PhysicalDeviceInfo_T),
				    GFP_ATOMIC);
      if (PhysicalDeviceInfo == NULL)
		return DAC960_Failure(Controller, "PHYSICAL DEVICE ALLOCATION");
      Controller->V2.PhysicalDeviceInformation[PhysicalDeviceIndex] =
		PhysicalDeviceInfo;
      memcpy(PhysicalDeviceInfo, NewPhysicalDeviceInfo,
		sizeof(DAC960_V2_PhysicalDeviceInfo_T));

      InquiryUnitSerialNumber = kmalloc(
	      sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T), GFP_ATOMIC);
      if (InquiryUnitSerialNumber == NULL) {
	kfree(PhysicalDeviceInfo);
	return DAC960_Failure(Controller, "SERIAL NUMBER ALLOCATION");
      }
      Controller->V2.InquiryUnitSerialNumber[PhysicalDeviceIndex] =
		InquiryUnitSerialNumber;

      Channel = NewPhysicalDeviceInfo->Channel;
      TargetID = NewPhysicalDeviceInfo->TargetID;
      LogicalUnit = NewPhysicalDeviceInfo->LogicalUnit;

      /*
	 Some devices do NOT have Unit Serial Numbers.
	 This command fails for them.  But, we still want to
	 remember those devices are there.  Construct a
	 UnitSerialNumber structure for the failure case.
      */
      if (!DAC960_V2_NewInquiryUnitSerialNumber(Controller, Channel, TargetID, LogicalUnit)) {
      	memset(InquiryUnitSerialNumber, 0,
             sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T));
     	InquiryUnitSerialNumber->PeripheralDeviceType = 0x1F;
      } else
      	memcpy(InquiryUnitSerialNumber, NewInquiryUnitSerialNumber,
		sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T));

      PhysicalDeviceIndex++;
      LogicalUnit++;
    }
  return true;
}


/*
  DAC960_SanitizeInquiryData sanitizes the Vendor, Model, Revision, and
  Product Serial Number fields of the Inquiry Standard Data and Inquiry
  Unit Serial Number structures.
*/

static void DAC960_SanitizeInquiryData(DAC960_SCSI_Inquiry_T
					 *InquiryStandardData,
				       DAC960_SCSI_Inquiry_UnitSerialNumber_T
					 *InquiryUnitSerialNumber,
				       unsigned char *Vendor,
				       unsigned char *Model,
				       unsigned char *Revision,
				       unsigned char *SerialNumber)
{
  int SerialNumberLength, i;
  if (InquiryStandardData->PeripheralDeviceType == 0x1F) return;
  for (i = 0; i < sizeof(InquiryStandardData->VendorIdentification); i++)
    {
      unsigned char VendorCharacter =
	InquiryStandardData->VendorIdentification[i];
      Vendor[i] = (VendorCharacter >= ' ' && VendorCharacter <= '~'
		   ? VendorCharacter : ' ');
    }
  Vendor[sizeof(InquiryStandardData->VendorIdentification)] = '\0';
  for (i = 0; i < sizeof(InquiryStandardData->ProductIdentification); i++)
    {
      unsigned char ModelCharacter =
	InquiryStandardData->ProductIdentification[i];
      Model[i] = (ModelCharacter >= ' ' && ModelCharacter <= '~'
		  ? ModelCharacter : ' ');
    }
  Model[sizeof(InquiryStandardData->ProductIdentification)] = '\0';
  for (i = 0; i < sizeof(InquiryStandardData->ProductRevisionLevel); i++)
    {
      unsigned char RevisionCharacter =
	InquiryStandardData->ProductRevisionLevel[i];
      Revision[i] = (RevisionCharacter >= ' ' && RevisionCharacter <= '~'
		     ? RevisionCharacter : ' ');
    }
  Revision[sizeof(InquiryStandardData->ProductRevisionLevel)] = '\0';
  if (InquiryUnitSerialNumber->PeripheralDeviceType == 0x1F) return;
  SerialNumberLength = InquiryUnitSerialNumber->PageLength;
  if (SerialNumberLength >
      sizeof(InquiryUnitSerialNumber->ProductSerialNumber))
    SerialNumberLength = sizeof(InquiryUnitSerialNumber->ProductSerialNumber);
  for (i = 0; i < SerialNumberLength; i++)
    {
      unsigned char SerialNumberCharacter =
	InquiryUnitSerialNumber->ProductSerialNumber[i];
      SerialNumber[i] =
	(SerialNumberCharacter >= ' ' && SerialNumberCharacter <= '~'
	 ? SerialNumberCharacter : ' ');
    }
  SerialNumber[SerialNumberLength] = '\0';
}


/*
  DAC960_V1_ReportDeviceConfiguration reports the Device Configuration
  Information for DAC960 V1 Firmware Controllers.
*/

static bool DAC960_V1_ReportDeviceConfiguration(DAC960_Controller_T
						   *Controller)
{
  int LogicalDriveNumber, Channel, TargetID;
  DAC960_Info("  Physical Devices:\n", Controller);
  for (Channel = 0; Channel < Controller->Channels; Channel++)
    for (TargetID = 0; TargetID < Controller->Targets; TargetID++)
      {
	DAC960_SCSI_Inquiry_T *InquiryStandardData =
	  &Controller->V1.InquiryStandardData[Channel][TargetID];
	DAC960_SCSI_Inquiry_UnitSerialNumber_T *InquiryUnitSerialNumber =
	  &Controller->V1.InquiryUnitSerialNumber[Channel][TargetID];
	DAC960_V1_DeviceState_T *DeviceState =
	  &Controller->V1.DeviceState[Channel][TargetID];
	DAC960_V1_ErrorTableEntry_T *ErrorEntry =
	  &Controller->V1.ErrorTable.ErrorTableEntries[Channel][TargetID];
	char Vendor[1+sizeof(InquiryStandardData->VendorIdentification)];
	char Model[1+sizeof(InquiryStandardData->ProductIdentification)];
	char Revision[1+sizeof(InquiryStandardData->ProductRevisionLevel)];
	char SerialNumber[1+sizeof(InquiryUnitSerialNumber
				   ->ProductSerialNumber)];
	if (InquiryStandardData->PeripheralDeviceType == 0x1F) continue;
	DAC960_SanitizeInquiryData(InquiryStandardData, InquiryUnitSerialNumber,
				   Vendor, Model, Revision, SerialNumber);
	DAC960_Info("    %d:%d%s Vendor: %s  Model: %s  Revision: %s\n",
		    Controller, Channel, TargetID, (TargetID < 10 ? " " : ""),
		    Vendor, Model, Revision);
	if (InquiryUnitSerialNumber->PeripheralDeviceType != 0x1F)
	  DAC960_Info("         Serial Number: %s\n", Controller, SerialNumber);
	if (DeviceState->Present &&
	    DeviceState->DeviceType == DAC960_V1_DiskType)
	  {
	    if (Controller->V1.DeviceResetCount[Channel][TargetID] > 0)
	      DAC960_Info("         Disk Status: %s, %u blocks, %d resets\n",
			  Controller,
			  (DeviceState->DeviceState == DAC960_V1_Device_Dead
			   ? "Dead"
			   : DeviceState->DeviceState
			     == DAC960_V1_Device_WriteOnly
			     ? "Write-Only"
			     : DeviceState->DeviceState
			       == DAC960_V1_Device_Online
			       ? "Online" : "Standby"),
			  DeviceState->DiskSize,
			  Controller->V1.DeviceResetCount[Channel][TargetID]);
	    else
	      DAC960_Info("         Disk Status: %s, %u blocks\n", Controller,
			  (DeviceState->DeviceState == DAC960_V1_Device_Dead
			   ? "Dead"
			   : DeviceState->DeviceState
			     == DAC960_V1_Device_WriteOnly
			     ? "Write-Only"
			     : DeviceState->DeviceState
			       == DAC960_V1_Device_Online
			       ? "Online" : "Standby"),
			  DeviceState->DiskSize);
	  }
	if (ErrorEntry->ParityErrorCount > 0 ||
	    ErrorEntry->SoftErrorCount > 0 ||
	    ErrorEntry->HardErrorCount > 0 ||
	    ErrorEntry->MiscErrorCount > 0)
	  DAC960_Info("         Errors - Parity: %d, Soft: %d, "
		      "Hard: %d, Misc: %d\n", Controller,
		      ErrorEntry->ParityErrorCount,
		      ErrorEntry->SoftErrorCount,
		      ErrorEntry->HardErrorCount,
		      ErrorEntry->MiscErrorCount);
      }
  DAC960_Info("  Logical Drives:\n", Controller);
  for (LogicalDriveNumber = 0;
       LogicalDriveNumber < Controller->LogicalDriveCount;
       LogicalDriveNumber++)
    {
      DAC960_V1_LogicalDriveInformation_T *LogicalDriveInformation =
	&Controller->V1.LogicalDriveInformation[LogicalDriveNumber];
      DAC960_Info("    /dev/rd/c%dd%d: RAID-%d, %s, %u blocks, %s\n",
		  Controller, Controller->ControllerNumber, LogicalDriveNumber,
		  LogicalDriveInformation->RAIDLevel,
		  (LogicalDriveInformation->LogicalDriveState
		   == DAC960_V1_LogicalDrive_Online
		   ? "Online"
		   : LogicalDriveInformation->LogicalDriveState
		     == DAC960_V1_LogicalDrive_Critical
		     ? "Critical" : "Offline"),
		  LogicalDriveInformation->LogicalDriveSize,
		  (LogicalDriveInformation->WriteBack
		   ? "Write Back" : "Write Thru"));
    }
  return true;
}


/*
  DAC960_V2_ReportDeviceConfiguration reports the Device Configuration
  Information for DAC960 V2 Firmware Controllers.
*/

static bool DAC960_V2_ReportDeviceConfiguration(DAC960_Controller_T
						   *Controller)
{
  int PhysicalDeviceIndex, LogicalDriveNumber;
  DAC960_Info("  Physical Devices:\n", Controller);
  for (PhysicalDeviceIndex = 0;
       PhysicalDeviceIndex < DAC960_V2_MaxPhysicalDevices;
       PhysicalDeviceIndex++)
    {
      DAC960_V2_PhysicalDeviceInfo_T *PhysicalDeviceInfo =
	Controller->V2.PhysicalDeviceInformation[PhysicalDeviceIndex];
      DAC960_SCSI_Inquiry_T *InquiryStandardData =
	(DAC960_SCSI_Inquiry_T *) &PhysicalDeviceInfo->SCSI_InquiryData;
      DAC960_SCSI_Inquiry_UnitSerialNumber_T *InquiryUnitSerialNumber =
	Controller->V2.InquiryUnitSerialNumber[PhysicalDeviceIndex];
      char Vendor[1+sizeof(InquiryStandardData->VendorIdentification)];
      char Model[1+sizeof(InquiryStandardData->ProductIdentification)];
      char Revision[1+sizeof(InquiryStandardData->ProductRevisionLevel)];
      char SerialNumber[1+sizeof(InquiryUnitSerialNumber->ProductSerialNumber)];
      if (PhysicalDeviceInfo == NULL) break;
      DAC960_SanitizeInquiryData(InquiryStandardData, InquiryUnitSerialNumber,
				 Vendor, Model, Revision, SerialNumber);
      DAC960_Info("    %d:%d%s Vendor: %s  Model: %s  Revision: %s\n",
		  Controller,
		  PhysicalDeviceInfo->Channel,
		  PhysicalDeviceInfo->TargetID,
		  (PhysicalDeviceInfo->TargetID < 10 ? " " : ""),
		  Vendor, Model, Revision);
      if (PhysicalDeviceInfo->NegotiatedSynchronousMegaTransfers == 0)
	DAC960_Info("         %sAsynchronous\n", Controller,
		    (PhysicalDeviceInfo->NegotiatedDataWidthBits == 16
		     ? "Wide " :""));
      else
	DAC960_Info("         %sSynchronous at %d MB/sec\n", Controller,
		    (PhysicalDeviceInfo->NegotiatedDataWidthBits == 16
		     ? "Wide " :""),
		    (PhysicalDeviceInfo->NegotiatedSynchronousMegaTransfers
		     * PhysicalDeviceInfo->NegotiatedDataWidthBits/8));
      if (InquiryUnitSerialNumber->PeripheralDeviceType != 0x1F)
	DAC960_Info("         Serial Number: %s\n", Controller, SerialNumber);
      if (PhysicalDeviceInfo->PhysicalDeviceState ==
	  DAC960_V2_Device_Unconfigured)
	continue;
      DAC960_Info("         Disk Status: %s, %u blocks\n", Controller,
		  (PhysicalDeviceInfo->PhysicalDeviceState
		   == DAC960_V2_Device_Online
		   ? "Online"
		   : PhysicalDeviceInfo->PhysicalDeviceState
		     == DAC960_V2_Device_Rebuild
		     ? "Rebuild"
		     : PhysicalDeviceInfo->PhysicalDeviceState
		       == DAC960_V2_Device_Missing
		       ? "Missing"
		       : PhysicalDeviceInfo->PhysicalDeviceState
			 == DAC960_V2_Device_Critical
			 ? "Critical"
			 : PhysicalDeviceInfo->PhysicalDeviceState
			   == DAC960_V2_Device_Dead
			   ? "Dead"
			   : PhysicalDeviceInfo->PhysicalDeviceState
			     == DAC960_V2_Device_SuspectedDead
			     ? "Suspected-Dead"
			     : PhysicalDeviceInfo->PhysicalDeviceState
			       == DAC960_V2_Device_CommandedOffline
			       ? "Commanded-Offline"
			       : PhysicalDeviceInfo->PhysicalDeviceState
				 == DAC960_V2_Device_Standby
				 ? "Standby" : "Unknown"),
		  PhysicalDeviceInfo->ConfigurableDeviceSize);
      if (PhysicalDeviceInfo->ParityErrors == 0 &&
	  PhysicalDeviceInfo->SoftErrors == 0 &&
	  PhysicalDeviceInfo->HardErrors == 0 &&
	  PhysicalDeviceInfo->MiscellaneousErrors == 0 &&
	  PhysicalDeviceInfo->CommandTimeouts == 0 &&
	  PhysicalDeviceInfo->Retries == 0 &&
	  PhysicalDeviceInfo->Aborts == 0 &&
	  PhysicalDeviceInfo->PredictedFailuresDetected == 0)
	continue;
      DAC960_Info("         Errors - Parity: %d, Soft: %d, "
		  "Hard: %d, Misc: %d\n", Controller,
		  PhysicalDeviceInfo->ParityErrors,
		  PhysicalDeviceInfo->SoftErrors,
		  PhysicalDeviceInfo->HardErrors,
		  PhysicalDeviceInfo->MiscellaneousErrors);
      DAC960_Info("                  Timeouts: %d, Retries: %d, "
		  "Aborts: %d, Predicted: %d\n", Controller,
		  PhysicalDeviceInfo->CommandTimeouts,
		  PhysicalDeviceInfo->Retries,
		  PhysicalDeviceInfo->Aborts,
		  PhysicalDeviceInfo->PredictedFailuresDetected);
    }
  DAC960_Info("  Logical Drives:\n", Controller);
  for (LogicalDriveNumber = 0;
       LogicalDriveNumber < DAC960_MaxLogicalDrives;
       LogicalDriveNumber++)
    {
      DAC960_V2_LogicalDeviceInfo_T *LogicalDeviceInfo =
	Controller->V2.LogicalDeviceInformation[LogicalDriveNumber];
      unsigned char *ReadCacheStatus[] = { "Read Cache Disabled",
					   "Read Cache Enabled",
					   "Read Ahead Enabled",
					   "Intelligent Read Ahead Enabled",
					   "-", "-", "-", "-" };
      unsigned char *WriteCacheStatus[] = { "Write Cache Disabled",
					    "Logical Device Read Only",
					    "Write Cache Enabled",
					    "Intelligent Write Cache Enabled",
					    "-", "-", "-", "-" };
      unsigned char *GeometryTranslation;
      if (LogicalDeviceInfo == NULL) continue;
      switch (LogicalDeviceInfo->DriveGeometry)
	{
	case DAC960_V2_Geometry_128_32:
	  GeometryTranslation = "128/32";
	  break;
	case DAC960_V2_Geometry_255_63:
	  GeometryTranslation = "255/63";
	  break;
	default:
	  GeometryTranslation = "Invalid";
	  DAC960_Error("Illegal Logical Device Geometry %d\n",
		       Controller, LogicalDeviceInfo->DriveGeometry);
	  break;
	}
      DAC960_Info("    /dev/rd/c%dd%d: RAID-%d, %s, %u blocks\n",
		  Controller, Controller->ControllerNumber, LogicalDriveNumber,
		  LogicalDeviceInfo->RAIDLevel,
		  (LogicalDeviceInfo->LogicalDeviceState
		   == DAC960_V2_LogicalDevice_Online
		   ? "Online"
		   : LogicalDeviceInfo->LogicalDeviceState
		     == DAC960_V2_LogicalDevice_Critical
		     ? "Critical" : "Offline"),
		  LogicalDeviceInfo->ConfigurableDeviceSize);
      DAC960_Info("                  Logical Device %s, BIOS Geometry: %s\n",
		  Controller,
		  (LogicalDeviceInfo->LogicalDeviceControl
				     .LogicalDeviceInitialized
		   ? "Initialized" : "Uninitialized"),
		  GeometryTranslation);
      if (LogicalDeviceInfo->StripeSize == 0)
	{
	  if (LogicalDeviceInfo->CacheLineSize == 0)
	    DAC960_Info("                  Stripe Size: N/A, "
			"Segment Size: N/A\n", Controller);
	  else
	    DAC960_Info("                  Stripe Size: N/A, "
			"Segment Size: %dKB\n", Controller,
			1 << (LogicalDeviceInfo->CacheLineSize - 2));
	}
      else
	{
	  if (LogicalDeviceInfo->CacheLineSize == 0)
	    DAC960_Info("                  Stripe Size: %dKB, "
			"Segment Size: N/A\n", Controller,
			1 << (LogicalDeviceInfo->StripeSize - 2));
	  else
	    DAC960_Info("                  Stripe Size: %dKB, "
			"Segment Size: %dKB\n", Controller,
			1 << (LogicalDeviceInfo->StripeSize - 2),
			1 << (LogicalDeviceInfo->CacheLineSize - 2));
	}
      DAC960_Info("                  %s, %s\n", Controller,
		  ReadCacheStatus[
		    LogicalDeviceInfo->LogicalDeviceControl.ReadCache],
		  WriteCacheStatus[
		    LogicalDeviceInfo->LogicalDeviceControl.WriteCache]);
      if (LogicalDeviceInfo->SoftErrors > 0 ||
	  LogicalDeviceInfo->CommandsFailed > 0 ||
	  LogicalDeviceInfo->DeferredWriteErrors)
	DAC960_Info("                  Errors - Soft: %d, Failed: %d, "
		    "Deferred Write: %d\n", Controller,
		    LogicalDeviceInfo->SoftErrors,
		    LogicalDeviceInfo->CommandsFailed,
		    LogicalDeviceInfo->DeferredWriteErrors);

    }
  return true;
}

/*
  DAC960_RegisterBlockDevice registers the Block Device structures
  associated with Controller.
*/

static bool DAC960_RegisterBlockDevice(DAC960_Controller_T *Controller)
{
  int MajorNumber = DAC960_MAJOR + Controller->ControllerNumber;
  int n;

  /*
    Register the Block Device Major Number for this DAC960 Controller.
  */
  if (register_blkdev(MajorNumber, "dac960") < 0)
      return false;

  for (n = 0; n < DAC960_MaxLogicalDrives; n++) {
	struct gendisk *disk = Controller->disks[n];
  	struct request_queue *RequestQueue;

	/* for now, let all request queues share controller's lock */
  	RequestQueue = blk_init_queue(DAC960_RequestFunction,&Controller->queue_lock);
  	if (!RequestQueue) {
		printk("DAC960: failure to allocate request queue\n");
		continue;
  	}
  	Controller->RequestQueue[n] = RequestQueue;
  	blk_queue_bounce_limit(RequestQueue, Controller->BounceBufferLimit);
  	RequestQueue->queuedata = Controller;
  	blk_queue_max_hw_segments(RequestQueue, Controller->DriverScatterGatherLimit);
	blk_queue_max_phys_segments(RequestQueue, Controller->DriverScatterGatherLimit);
	blk_queue_max_sectors(RequestQueue, Controller->MaxBlocksPerCommand);
	disk->queue = RequestQueue;
	sprintf(disk->disk_name, "rd/c%dd%d", Controller->ControllerNumber, n);
	disk->major = MajorNumber;
	disk->first_minor = n << DAC960_MaxPartitionsBits;
	disk->fops = &DAC960_BlockDeviceOperations;
   }
  /*
    Indicate the Block Device Registration completed successfully,
  */
  return true;
}


/*
  DAC960_UnregisterBlockDevice unregisters the Block Device structures
  associated with Controller.
*/

static void DAC960_UnregisterBlockDevice(DAC960_Controller_T *Controller)
{
  int MajorNumber = DAC960_MAJOR + Controller->ControllerNumber;
  int disk;

  /* does order matter when deleting gendisk and cleanup in request queue? */
  for (disk = 0; disk < DAC960_MaxLogicalDrives; disk++) {
	del_gendisk(Controller->disks[disk]);
	blk_cleanup_queue(Controller->RequestQueue[disk]);
	Controller->RequestQueue[disk] = NULL;
  }

  /*
    Unregister the Block Device Major Number for this DAC960 Controller.
  */
  unregister_blkdev(MajorNumber, "dac960");
}

/*
  DAC960_ComputeGenericDiskInfo computes the values for the Generic Disk
  Information Partition Sector Counts and Block Sizes.
*/

static void DAC960_ComputeGenericDiskInfo(DAC960_Controller_T *Controller)
{
	int disk;
	for (disk = 0; disk < DAC960_MaxLogicalDrives; disk++)
		set_capacity(Controller->disks[disk], disk_size(Controller, disk));
}

/*
  DAC960_ReportErrorStatus reports Controller BIOS Messages passed through
  the Error Status Register when the driver performs the BIOS handshaking.
  It returns true for fatal errors and false otherwise.
*/

static bool DAC960_ReportErrorStatus(DAC960_Controller_T *Controller,
					unsigned char ErrorStatus,
					unsigned char Parameter0,
					unsigned char Parameter1)
{
  switch (ErrorStatus)
    {
    case 0x00:
      DAC960_Notice("Physical Device %d:%d Not Responding\n",
		    Controller, Parameter1, Parameter0);
      break;
    case 0x08:
      if (Controller->DriveSpinUpMessageDisplayed) break;
      DAC960_Notice("Spinning Up Drives\n", Controller);
      Controller->DriveSpinUpMessageDisplayed = true;
      break;
    case 0x30:
      DAC960_Notice("Configuration Checksum Error\n", Controller);
      break;
    case 0x60:
      DAC960_Notice("Mirror Race Recovery Failed\n", Controller);
      break;
    case 0x70:
      DAC960_Notice("Mirror Race Recovery In Progress\n", Controller);
      break;
    case 0x90:
      DAC960_Notice("Physical Device %d:%d COD Mismatch\n",
		    Controller, Parameter1, Parameter0);
      break;
    case 0xA0:
      DAC960_Notice("Logical Drive Installation Aborted\n", Controller);
      break;
    case 0xB0:
      DAC960_Notice("Mirror Race On A Critical Logical Drive\n", Controller);
      break;
    case 0xD0:
      DAC960_Notice("New Controller Configuration Found\n", Controller);
      break;
    case 0xF0:
      DAC960_Error("Fatal Memory Parity Error for Controller at\n", Controller);
      return true;
    default:
      DAC960_Error("Unknown Initialization Error %02X for Controller at\n",
		   Controller, ErrorStatus);
      return true;
    }
  return false;
}


/*
 * DAC960_DetectCleanup releases the resources that were allocated
 * during DAC960_DetectController().  DAC960_DetectController can
 * has several internal failure points, so not ALL resources may 
 * have been allocated.  It's important to free only
 * resources that HAVE been allocated.  The code below always
 * tests that the resource has been allocated before attempting to
 * free it.
 */
static void DAC960_DetectCleanup(DAC960_Controller_T *Controller)
{
  int i;

  /* Free the memory mailbox, status, and related structures */
  free_dma_loaf(Controller->PCIDevice, &Controller->DmaPages);
  if (Controller->MemoryMappedAddress) {
  	switch(Controller->HardwareType)
  	{
		case DAC960_GEM_Controller:
			DAC960_GEM_DisableInterrupts(Controller->BaseAddress);
			break;
		case DAC960_BA_Controller:
			DAC960_BA_DisableInterrupts(Controller->BaseAddress);
			break;
		case DAC960_LP_Controller:
			DAC960_LP_DisableInterrupts(Controller->BaseAddress);
			break;
		case DAC960_LA_Controller:
			DAC960_LA_DisableInterrupts(Controller->BaseAddress);
			break;
		case DAC960_PG_Controller:
			DAC960_PG_DisableInterrupts(Controller->BaseAddress);
			break;
		case DAC960_PD_Controller:
			DAC960_PD_DisableInterrupts(Controller->BaseAddress);
			break;
		case DAC960_P_Controller:
			DAC960_PD_DisableInterrupts(Controller->BaseAddress);
			break;
  	}
  	iounmap(Controller->MemoryMappedAddress);
  }
  if (Controller->IRQ_Channel)
  	free_irq(Controller->IRQ_Channel, Controller);
  if (Controller->IO_Address)
	release_region(Controller->IO_Address, 0x80);
  pci_disable_device(Controller->PCIDevice);
  for (i = 0; (i < DAC960_MaxLogicalDrives) && Controller->disks[i]; i++)
       put_disk(Controller->disks[i]);
  DAC960_Controllers[Controller->ControllerNumber] = NULL;
  kfree(Controller);
}


/*
  DAC960_DetectController detects Mylex DAC960/AcceleRAID/eXtremeRAID
  PCI RAID Controllers by interrogating the PCI Configuration Space for
  Controller Type.
*/

static DAC960_Controller_T * 
DAC960_DetectController(struct pci_dev *PCI_Device,
			const struct pci_device_id *entry)
{
  struct DAC960_privdata *privdata =
	  	(struct DAC960_privdata *)entry->driver_data;
  irq_handler_t InterruptHandler = privdata->InterruptHandler;
  unsigned int MemoryWindowSize = privdata->MemoryWindowSize;
  DAC960_Controller_T *60/AcceleR = NULL Mylunsigned char DeviceFunction = PCI_ Copyr->devfnAID Controllers

 ErrorStatus, Parameter0  Portions 1AID Controlleint IRQ_Channel Mylvoid __iomem *BaseAddress Myln IBi;

  eXtremeRAID Pkzalloc(sizeof(ex DAC960/AcceleRAI), GFP_ATOMIC) softf (eXtremeRAID  PCI R) {
	ex DAC9elion("Unable to te anateu may rediststructure for "
  program is distribute"eXtremeRAIDat\n",ense V;
	returnPCI RAID }you may redis->60/AcceleRNumbAID Pex DAC960/AcceleRCount Mylex DAC960/AcceleRs[ the implied warranty ++] =u may redis MylHOUT ANY WARBus8-2001 by Leonabus->nout ee the GNU GeneraFirmwareType = priv/*

  

#define DAe the GNU GeneraHardefine DAC960_DriverVe960_DriverDae the GNU Genera Copyrven Copyright 1998>> 3te details.

*/

ght 1998-2clude <linux/ty& 0x7e the GNU GeneraPCIle.h>
#in001 by Leo Mylstrcpyral Public inuxllModelName, "ex DAC"); youenerpci_eed by_dCopyr(001 by Leo))
	goto Failure; youswitcheral Public g 2007"


#inc)
 ersicasen the imGEM960/AcceleR:
	blkdev.h>
#includ_is freeC960ci_resource_start.h>
#includ, 0ll b  break;/dma-mapping.hBAinclude <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/slab.h>
LPnclude <linux/smp_lock.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/reboinclude <linux/smp_lock.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/rebPGh>
#include <linux/jiffies.h>
#include <linux/random.h>
#include <linux/scatterlist.h>
#include <asm/Dinclude <linux/interrupt.h>
IOclude <linux/ioport.h>
#include <linux/mm.h>
#incnterrupt.h>
#include <linux/ioport.h>
#include <linux/mm.1atterlist.h>
#include <asm/ller_T *DAC960_Controllers[DAC960_MaxControllers];
static int DAC960_ControllerCount;
static struct proc_dir_entry *DAC960_ProcDirectoryEntry;

static
  W
 ux/ioset_drriverde <linux/mm.()

  *)((long)HOUT ANY WARRANTY, without e)GNU G  Th(i = 0; i <n the imMaxLogicalDrives; i++Versiinux/blkpg.hdisks[iR PUte an_driv(1<<nfo_T *i =Parti 199sBitsll bener!Information[drive_nr)
	linux/hdreg.h>
eInformation[drive_nr->0_Drate_/*

 =ive_nr].
			LogiiGNU G WITinit_waitqueue_head(&HOUT ANY WARRAmmandWaitQk = GNU Gedisk *disk = bdev->bd_disk;
	DACHealth.com>
troller_T *p =spin_lock_ dis>bd_disk;
	DACsk = ba;

GNU Gex DAC9AnnounceV2.Lorral Public GNU G/*s proMap th Software FouRegister Driver.
  */

#incLinux Driver for < PAGE_SIZE)
	C960_V1_LogicalDr=ve_Offlinee the GNU GeneraLinux Mappedlude <lin
	ioremap_nocachelinux/blkpg.h#include <li&ve_OffMASK, Linux Driver forGNU Ge GNU Generalam is free
			960_V2_LogicalDeviceInfo_T *i =
+<linux/blkpg.hon[drive_nr];
~		if (!i GNU General Public icalDeviceInfo_T *i =
	cense Vinux/d1_Control published by themaprive_nr].
		    LogicalDriv
  This		ributed in the hope that i>V1.LogicalDr	  inux/hdreg.h>
uct geV2_LogicalDevte == DAC960_V2_LogicalDivatelude <linux/blkpg.h>
#include <linux/dma-mapping.h>
#include <linux/ipping.h>
#iDisd byInterrupts(V2_LogicalDdisk =pping.h>
#iAcknowledge960_DrivMailbox.com>
geo->heads = p->V1udelay(100h>
#incwhile dify it >
#iInitializa 199InProgfreePgeo->heads = )sk =urn 0;
calDenercylinders =Readelion.com>
geo->heads =, &elion.com>
 
	V2_LcalD&Portions CoplDeviceInf1) &&t hd_ex DAC9Reportds * geo->se60/AcceleR,delion.com>
 _V2_LogicPortions Copyright 200nr].

static int DACicalDrnslationSdisk =uct 	DriveS!ex DAC9V2_Eed byalDevice->secller)faceviceInformanr].
			LogicalDr}

static int DAC960_geteak;
	| i->Lo DAC960_ V2_Geometuct h	geo"  Thry *geo)
{
	struct gendisk *disk =
			y_128_32:
			geo->h>sectcylinders =eak;
	ller) {
		geo->heads = p->V1->disks[drivler_T960_Conven the im>
#iDeviceSize /= i->Configurable>heaplied warrannfigurgical
			 = 255;
			V2->hea;
}

static int DAC960_tors);
	}
	
	return 0clude c int DAC960_media_changed(struct gsk->queue->queuedattors);
	}
	
	returnnforsk->queue->queuedata;
	int drive_nr = (alDriveInitiallyAccessi= i->ConfigurableDevic>heaWritceSize /
	dia_changed(struevalidate_disk(struct
#include <linux/slab.h>
#include <linux/smlab.h>
#in_Controller) {
		geo->heads = p->V1.GeometBAranslationHeads;
		geo->sectors = p->V1.GeometryTranslationSectors;
		geo->cylindeBA= p->V1.LogicalDriveInformation[drive_nr].
			LogicalDriveSize / (BA->heads * geo->sectors);
	} else {
		DAC960_V2_LogicalDeviceInfo_T *i =
			p->V2.LogicalDeviceInformation[drive_nr];
		switch (i->DriveGeometry) {
		case DAC960_V2_Geometry_128_32:
			geo->heads = 128;
			geo->sectors = 32;
			break;
		case DAC960_V2_Geometry_255_63:
			geo->heads = 255;
			geo->sectors = 63;
			break;
		default:
			DAC960_Error("Illegal Logical Device Geometry %d\n",
					p, i->DriveGeometry);
			returBAEINVAL;
		}

		geo->cylinders = i->ConfigurableDeviceSize /
			(geo->BAds * geo->sectors);
	}
	
	return 0;
}

static int DAC960_media_changed(struct gendisk *disk)
{
	DAC960_Controller_T *p = disk->queue->queuedata;
	int drive_nr = (long)disk->private_data;

	if (!p->LogicalDriveInitiallyAccessible[drive_nr])
		return 1;
	return 0;
}

static int DAC960_revalidate_disk(struct gendisk *disk)
{
	DAC960_Controller_T *p = disk->queue->queuedaoot.h>
#include <lddress == _Controller) {
		geo->heads = p->V1.GeometLPranslationHeads;
		geo->sectors = p->V1.GeometryTranslationSectors;
		geo->cylindeLP= p->V1.LogicalDriveInformation[drive_nr].
			LogicalDriveSize / (LP->heads * geo->sectors);
	} else {
		DAC960_V2_LogicalDeviceInfo_T *i =
			p->V2.LogicalDeviceInformation[drive_nr];
		switch (i->DriveGeometry) {
		case DAC960_V2_Geometry_128_32:
			geo->heads = 128;
			geo->sectors = 32;
			break;
		case DAC960_V2_Geometry_255_63:
			geo->heads = 255;
			geo->sectors = 63;
			break;
		default:
			DAC960_Error("Illegal Logical Device Geometry %d\n",
					p, i->DriveGeometry);
			returLPEINVAL;
		}

		geo->cylinders = i->ConfigurableDeviceSize /
			(geo->LPLeonard N. Zubkoff "
		  "<lnz@dandelion.com>\n", Controller);
}


/*
  DAC960_Failure prints a standardized error message, and then returns false.
*/

static bool DAC960_Failure(DAC960_Controller_T *Controller,
			      unsigned char *ErrorMessage)
{
  DAC960_Error("While configuring DAC960 PCI RAID Controller at\n",
	       Controller);
  if (Controller->IO_Address =a;
	int unit = (long)disL->private_data;

	set_capacity(disk, disk_siLe(p, unit));
	return 0;
}

static const struct block_device_operations DAC960_BlockLeviceOperations = {
	.owner			= THIS_MODULE,
	.open			= DAC960_openL
	.getgeo			= DAC960_getgeo,
	.media_changed		= DAC960_media_changed,
	.revalidate_disk	= DAC960_revalidate_disk,
};


/*
  DAC960_AnnounceDriver announces the Driver Version and Date, Author's Name,
  Copyright Notice, and Elec1Error("%s FAILED - DETACHING\n", Controller, ErrorMessage);
  return false;
}

/*
  init_dma_loaf() and slice_dma_loaf() are helper functions for
  aggregating the dma-mapped memory for a well-knoDate " *****\n", Controller);
  DAC9ty(p->disks[drivV1.Dua
#inc	case DAC960_V2_Geometr].
			hat are of different lengths.

  Th Leonard N. Zua_base);	geo-els Software Fores allocates and initializes the auxilSingle
  data s;
	}
	
	return 0;
}

static int DAC960_media_changed(st1uct gendisk *disk)
{
	DAC960_Controller_T *p = disk->queue->queuedata;
	int drive_nres(DACng)disk->private_data;

	if (!p->LogicalDriveInitiallyAccessible[drive_nr])
	es(Durn 1;
	return 0;
}

static int DAC960_revalidate_disk(struct gendisk *disk)
1
	DAC960_Controller_T *p = disk->queue->queueda/io.h>
#include <aerGatherPo_Controller) {
		geo->heads = p->V1.GeometPGranslationHeads;
		geo->sectors = p->V1.GeometryTranslationSectors;
		geo->cylindePG= p->V1.LogicalDriveInformation[drive_nr].
			LogicalDriveSize / (PGoaf, size_t len,
					dma_addr_t *dma_handle)
{
	void *cpu_end = loaf->cpu_free + len;
	void *cpu_addr = loaf->cpu_free;

	BUG_ON(cpu_end > loaf->cpu_base + loaf->length);
	*dma_handle = loaf->dma_free;
	loaf->cpu_free = cpu_end;
	loaf->dma_free += len;
	return cpu_addr;
}

static void free_dma_loaf(struct pci_dev *dev, struct dma_loaf *loaf_handle)
{
	if (loaf_handle->cpu_base != NULL)
		pci_free_consistent(dev, loaf_hPGdle->length,
			loaf_handle->cpu_base, loaf_handle->dma_base);
}


/*
  DAC960_CreateAuxiliaryStructures allocates and initiaPGzes the auxiliary
  data structures for Controller.  It returns tC960_Command_T,and false on
  failure.
*/

static bool DAC960_CreateAuxiliaryStructures(DAC960_Controller_T *Controller)
{
  int CommandAllocationLength, CommandAllocationGroupSize;
  int CommandsRemaining = 0, CommandIdentifier, CommandGroupByteCount;
  void *AllocationPointer = NULL;
  void *ScatterGatherCPU = NULL;
  dma_addr_t ScatterGatherDMA;
  struct pci_pool *ScatterGatheroller_T *DAC960_Cors =request_regionp->disks[drivAC960_MaxC, 0x80eGeom,
				x/completion.h>
#include )Versiion 2 as publisIO nfor 0x%d busyV2_Loin the hope that Error("ce, sizeof(Dct gendisk *nse",
		Contdiskry_128_32:
			geo-RE CREATION (DCPU = NULL;
  dma_addr_t RequestSenseDMA;
  DranslationHeePool = NULL;

  if (Controller->FirmwareType == DAC960_D1_Controller)
    {
      CommandAllocationLength = offsetof(DAC960Doaf, size_t len,
					dma_addr_t *dma_handle)
{
	void *cpu_end = loaf->cpu_free + len;
	void *cpu_addr = loaf->cpu_free;

	BUG_ON(cpu_end > loaf->cpu_base + loaf->length);
	*dma_handle = loaf->dma_free;
	loaf->cpu_free = cpu_end;
	loaf->dma_free += len;
	return cpu_addr;
}

static void free_dma_loaf(struct p
  Free SDMAtgeoped mak;
		 == NULL)
	    return DAC960_Failure(Controller,
			"AUXILIARY STRUCTURE CREATION (DEINVAL;
		}

		geo->cylinders = i->ConfigurableDeviceSize /
			(geo->PDds * geo->sectors);
	}
	
	return 0;
}

static int DAC960_media_changed(stes(DAC960_Controller_T *Controller)
{
  int CommandAllocationLength, CommandAllocationGroupSize;
  int CommandsRemaining = 0, CommandIdentifier, CommandGroupByteCount;
  void *AllocationPointer = NULL;
  void *ScatterGatherCPU = NULL;
  dma_addr_t ScatterGatherDMA;
  struct pci_pool *ScatterGathernsePool = pci_pool_create("DAC960_V2_RequestSense",
		Controller->PCIDevice, sizeof(DAC960_SCSI_RequesSense_T),
		sizeof(int), 0);
      if (RequestSensePool == N   ULL) {
	    pci_pool_destroy(ScatterGatherPool);
	    return DAC960_Failure(Controller,
			"AUXILIARY STRUCTURE CREATION (SG)");
      }
      Controller->ScatterGatherPool = ScatterGatherPool;
      Controller->V2.RequestSensePool = RequestSensePool;
    }
  Controller->CommandAllocationGroupSize = CommandAllocationGroupSize;
  Controller->FreeCommands = NULL;
  for (CommandIdentifier = 1;
       CommandIdentifier <= Controller->DriverQueueDepth;
       CommandIdentifier++)
    {
      DAC960_Command_T *Command;
      if (--CommandsRemaining <= 0)
	{
	  CommandsRemaining =
		Controller>DriverQueueDepth - CommandIdentifier + 1;
	  if (CommandsRemaining > CommandAllocationGroupSize)
		CommandsRemaining = CommandAllocationGroupSize;
	  ComandGroupByteCount =
		CommandsRemaining * CommandAllocationLength;
	  AllocationPointer = kzalloc(CommandGroupByteCount, GFP_ATOMIC);
	  if (AllocationPointer == NULL)
		return DAC960_Failure(Controller,
					"AUXILIARY STRUCTURE CREATION");
	 }
      Command = (DAC960_Command_T *) AllocationPointer;
      AllocationPointer += CommandAllocationLength;
   uct geeInform Acquire shared accalDetoon[drIRQ siness eState =BM Business 8-2001 by LeonairqNU Genereate("DAirq(M Business ,:
			D {
	Handci_poIRQF_SHAREDCommandice, sizeof(DAC960_SCSI_Requct gendisk *d < 0eturn 0on 2 as published by the
atherCPerGatherLis %dller->FreeCommands = CommandL) {
	    pci_pool_destroy(Sc Business disk bdev->bd_disk;
	DAC


/*
  DAC960_DestroyA =BM Business Unitol_destroy(Scp->V1.960_Con.960_ConIdentifiAID P02 byr Controller.
*/

static voidtremeRAID PRPOSE.  See the GNU Genera960_Cons[0R PUbd_disk;
	DAC.
*/

static vte details.

*/

receSize /llererGatherPool = Controller->Scate usefuRPOSE.  See therCP
hdreg.h:acity(p->disks[drivAC960_MaxCon=;
	sit_table(CommanPCI l Pu%dice.h>
r_t ux/miscde%d I/O ogicalDeN/Auct hd_geo dma_ogicalDe0x%Xtruct gendisk *
    }
  e GNU General Ppool_destroy(Sinux/mmFirmwareType == DAC9ux/miscdpool_destroy(S#include <lGNU GtrucuestSenseCPU;
  dma_addr_t RequestSenseDMA;
  DAC960_Command__Erro"

  /bio NULL;
  

  if (Controller->Firce_Offline)
	60_V2_Controller)
        Rce_Offline)
	 Controller->V2.Requese",
		Contr;

      if (ComtSensePool;

  Cex DAC9DetectCleanup->V1.LogicalDriv the implied warranty --ULL;
  voidCI RAI}

eInfoex DAC9 p->V1.LoueueremeRAID disGatherseType == DA.
*/

static bool 

	  ScatterGatherDMA = Commdify it under
  the D/eXtremeRAI)
{h>
#incalDeviceI gendisk *disk)
{
	DAC960ry_255_63:
	V2.Lds = 255;
			void *terGatherCPU = (void *)Command->V2.ScatterGatherListCreateAuxiliarySdation.
ve_nr];
		sw.ScatterGatherList;
upSize;
  int CommandCommand->V2.ScatterGatherList;
	  SRequestSenseDMA = Command->V2.RequestSenseDMA;
     LogiBa;

clude ry_255_63:
			
			LoherCPU/*
	tterGatheron[drMonitoring TimListherCPU = ScalDridisktf (R>bd_disk;
	DACMA);
     if (RGNU Gommand->V2.Requel_free(RequestS.expireDevicjiffie}

	nfo_T *i_free(RequestSller)va Unitool, RequestSenseCPU, RequestSenbdev, fmControlle	Logid *ScatterGatherCPU RequestSenseCPU, RequestSenfx/miscdevicmmand->CommandIdentifux/miscd% ControaddL)
          pci_pool_free(RequestSensePool, RequestSensDMA = CommtterGather/
		truisk;
ree e usefudon't free WITe usefufalseatherrList;
	  ScaFinatherDMA = Commafxt groutterGatherListDMA;
	  Req)

  f the next group.
	    */nseDMA = (dma_addr_t)0;
      } else {
   ber the beginning of the group, b ScatterGather 1) {
	   /*
 flags; youtherCPU }
  re*rGather    e /
releas    a;

 here eliminates
      kfra very low probability raceequestSe {
      kfrThe code benedS
  Free s cMA = CommacSize /
ndation.
 Controllerfromon[drfree list without hold    n[drULL;
    }
a;

LL;
       This is safe assumerGathereareTno oV1_C activfer on
      kfrtherPool);
  ifatstSen)
  ntroller->F
      kfrBut,_V1_Conmight be a mA);
      
  if (Scaill
      kfrinStatInfor.  SetterGatherShutdowner !=
		geo-catterGif (RequestSentatusensGath thULL)
	Controlle0; i < DAC960_MaxLDrives; i++) n[driler) {
	 he_T *) currently,ller-anyn[i] = NULL
      kfr
  if (alDeviccomplet 	pcLL)
 ispci_p on will NOT reacheif (RequestSei}

  if (ScatterGatV2.Scattpci_pool_LL;
      /ULL)
   e_data;

	irqsavep->FirmwareType == DAC96,er != ensePool, RequestSens.Logical>CommandIdentifAuxiliarr->V2.InqunuiryUnitrestorialNumber[i]);
      Controller->V2/*
  DAdelL)
   _sync       pci_pool_free(RequestSensePool,ty(p->disks[driv

#define DAC9he
	    *V1960/AcceleR)
	n 0;
}

statNottter"Flush    CalDe...uct gendisk *disk == cpu_end;
xecutne DA3e_nr];
		switommand_T *lbox_m.h>
#inc0_V1_CommandMaidonetruct gendisk *dissectors inux/blkpg.h>
#include <60_Command_SensePool = pr].
			e reombieAC960_V2_RequestSense",
		Controllerdisk WITrollontrolDAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.Comman2 by LeoOpeMA = Command->V2.dMailbox, 2_Pauseands[i];

 ia_changed(strucAICommand clearAC960_V1_CommandMailbox_T));
  Command->V1Control WIT
	    *UnC960ool_free(ScatterGatherPool,e == DAC960_VstroyuestSenseCPU = (void *)Command-0;
}


/*
  DAC960ProcEntriallocates a Command structu1_Controller) {
	  Scatteginning of the nProbe>Comstro NULL;
    }'s exCommnceller gendisV1.Scattn[dr
	     	if (pV2_Lo2.PhysiGatherListDMA;
	  Reqn IBenseCPU izati(ndatioux/iodev *dev,atiostScatter_Commandice_nr].entryelse {
nt driv Mylex DAC960/AcceleRAID/eXtremeRAIy.h>
#inc the implied warranty 60_Command_MaxABILITY
  o	sg_init_table(CommanMon[i]ha  DACtee thaABILITY
  o dg dried -his program is distributed ign      ceInfequestSensePool == program is distributedCI RdMailbox,  if (Command =l be usefu-ENODEVurn p->V1er_T *Control DAC960_V1_Con     }
    *DAControlNU GenerigurableDev)t to Controller's
mand(DACnseCPU = NULL;
	  RequestSeny_255_63:
		terGait_tableommand;
      }
   eof(DAC960_V2_to Controller's
  free 2_LogdrivcalDevds;
 eInfo_T *i =
			p->V2.LogicdrivlDevic programicalcapacitnux/completiondrive_driv],
}


_/or e_nr];
		switdrivC960_Vr gather di == waits for a wake_up on struct gestDMA;
	  Reqom Controller's
  free liste usefu0beginning of the next grou
	   kfree(arantee that commup);
	   CommandGroup = CRemove DAC960_Command_Th>
#includeler_T
			60/AcceleRAhout even		Logix/iogcalDriveInformation[dommand struc60/AcceleRAID/eXtremeRAID PERCHANTABILITY
  orqueue_lock);
}

/]NU General Public L!IO;
	retur = 255;
			and->Request = NULL;
  Command->ginning of the nScatterGatherDMA;
  stru prepaicalontrsk = s a 
	  /_disk m *ContrforControlle V1 

#definCommands = Cup);
	   CommandGroup = CScatterGatherDMA;
  strummand = Co0_Con_t)0;
0_Conntrollex DAC960/AcceleRAID/eXtremeRAID Px_T *Neginning of t Mylex DAC9T *Co0_ConDAC960_ilbox_T *Nedefault:ci_poxtCommaV1oid DAC9DAC960_

  CommandMaiScatterGaeInfSegment_t)0iteCommandMaiLol_d=_V2_Lomand->CommanmmandMailbox, Comclude <ateCsteCommool_dommandMa Comma.NextCommaif (ControV2.Pre
  CommandMaillear
  DAC96x_T *Nex DAC960_Cmand->Comlbox(Ne->FreeCom1 ScatterGather      DAC960_DmaDiret 1998-x/bio.hMA_FROMDEVICe)
	lbox->Common.C->e DA5oid DAC9Oprrent_Command_T *
	  e.
*/

struct.
*/

stamandMailbox2 =
      Controller->V2.PreviousComm_disk for DAC9mandMailbox2 =
      CLD.TransferLength 0 ||
      free(anty of M+NextCommandMailbox > Control
			p->V2.Lohout evenmand->Com.FirstCommandMailbxtCommandMailbox = Controller->			p->free(Controller_TMailbox)
   xtCommandMailbox = NextCommandMailboBusogicalDevic		960_open,trollers.32_T)sg_dma_as free(troller->V2);	ommandMaiontrowCommand(Contware; you(ControllerBaseAddress);

  Controller->V2.PreviousCommandMailbox2 =
      Controller->V2.PreviousCommandMWithiteCommandMaiailbox1;
  CommandMailbox2 =
      Controller->V2.PreviousComm_diskV2_CommandMailbox_T (++NextCommandMailbox > Controller->V2.LastCommandMailbox)
      NextCommandMailbox = Controller->V2.FirstCommandMailbox;

  Controller->V2.NextCommandMailbox = NextCommandMailbox;
}

/*
  DAC960_BA_QueueCommand queues Command for DAC960 BA Series Controllers.
* ||
      Controllerilbox, ComDMACommandMailbox =
    ControlleriteCommandMai->FreeCQueueCommaGEM_MemoryMaCommandMa2_LogicalDeviceIewCommand(ControllerBacalD, troller->V2box2 =
    ilbox, ComlDevice	mmandMailbox, Comnd(ControDataPoDAC96
*/

static void DAC960_BA_QueueCommand(DAC960_Command_T Mailbox1;
  Controller->V2.PrBysk(sFreeC/

static voidMailboxBA_QueueCommlenxtCommandMailboControCommandMailbox_T)DeviceSize /ords[0] == ommand->Controlle{
	DAC960_Controller_T rollerBaseAddress = Controller->BaseAddress;
  DAC960_V22CommandMailbox_T *CommandMailbox = &Command->V2{
	DAC960_Controller_T 60_V2_CommandMailbox_T *NextCommandMailbox =
      Controller->V2.NextCommandMailbox;

  CommandM2ilbox->Common.CommandIdentifier = Command->Comm2ndIdentifier;
  DACousCommandMailbox1->Words[0] == 0 ||
      C2ntroller->V2.PreviousComm2ndMailbox2->Words[0] == 0)
mandMailbox2 =
 SCSI_10Controller->V2.PreviousCo2_mandMailiarySer->V2.NextCommandMailbox;
  id DAC9turn.evioler->V2.id DAC960_ToHoommanontrllerBaseAddress);

  Controller->V2.PreviousCmandIdentifier = Command->CoDAC960_LP_WrXIO;
	NextCndMailbox)
      Ne <eInfo_T *free( forturnmandIdentifier = Command->CoRate("DSense== 0 ||
      Controlle2_LP_MemoryMaindMairoller->V2.NextCommandMailPhys	p->Ve.h>
#i ||
   af_handle->22.FirstCommanToVirtulbox2 =
[

  Controller->V2.NextCommCommards[0] == 0)
    DAC960_LP_MemoryMaiXIO;
	}/or modify it mandMLP_MemoryMai_Tller->V2.PreviousCommandMailbCDBLastCommaommandIdentifier = Command->ComandMCDB*ScatNextCommandMailbox, CommandMailbox);
  if (Contro ? 0x28 :
/*
Aller->V2.PreviousCommandMailbx;
  Cont2R PURPueCommand queues Cypes24LA_QueueCommandDualMode queues Comman3 for DAC960 LA Series
  Cont16LA_QueueCommandDualMode queues Comman4 for DAC960 LA Series
  Cont8LA_QueueCommandDualMode queues Comman5 for DAC960 LA Series
  LA_QueueCommandDualMode queues Comman7 for DAC960 LA Serilbox)ontroller = Command->Controller;
  void _860_V1_CommandMailbox_T = 0)
      DAC960_GEM_MemoryMailboxNewCommand(Con>V2.PreviousCommandMailbox1->Words[0Linux is free= NULL) {C960_BA_Memorylbox(Ne *ScmmandIdenti>V2.PreviousCommandMatatic void DAC960_64ilbox;
  if (++NextCommandMailboCommandMailbox = Contrlbox;
  CommandMailbox->Common.CommandIdentifier = Command->CommandIdentifier;
  DACndMailbox)
  >V2.PreviousCommandMailbox1->Words[0] ==til we've {
  DAC960_Controilbox->CommteCommandMailbox(NextCommandMailbox, Comine void ler_T *Controller = CommanGEM_MemoryMai> 2
  DA program immandMailbox, Commaox;
  DAC960_V2_Comm   Controlle
  faentifier = Command->CommandIdentifier;
 Error.Add;
		ralmmandMailbox, Comeak;
		t don't ilbox > Controller->V1.LastCommandMailbox->Common.Comma.Extended>V1.PreviousC (Controller->V2.P0LastCommandMailbox(ControllerBasndMailbox;
  Controller->V1.NextCommandMailbox = NextCo	 mmandMailbox;
}


/*
  DAC960_LA_QueueComogicalDevic||
      Cont
  if (++NextCommandndMaiControllers.
*/ailbox1 = NextCommandMailbo->V1.PreviousCommandMailbox1->Words[0] == 0 ||	ntifier = Command->CommaseAddress);
  Controller->V2.PreviousCommandMailbox2 =
    Controller->V2.PreviousCommandMailbox1;
  Controller->V2.PreviousCommandMailbox1 = NextCommandmmandMailbox, CommandMailbox);
  ix > Controller->V2.LastCommandMailbox)
    NextCommandMailbommandMailboxer->V2.FirstCommandMailbox;
  Controller->V2.NextCommandMailbox = Ne fail.
*/


	    *prond->_sk = nseDMA = (dma_addr_t)0;
      } ,ailbox =eate("DAsk =  *req_qntro	PreviousComman *LP_Memo;uestSensemmandMailbox_T *NeComman		geo(1VersiLP_Memo = blk_peek_eate("DSens_q i->ConfiLP_Memometre usefu1V1.CeSize /
			(geo->A  Free extCommandf(DAC960_V2_      DAC96NXIO;
	retControlle0V1.CestSeq*bdev_dir(x2 =
   ontrREADandMailerBaseAddress);

  Conroller->V2.PreviousommoNextCommand0_Cone DAC96       Scatteer_T *p };
  Co> Controller->V1.LastCommandMailbox)Tler'xtCommandMailbox = Controller->V1.Firontroller_T *p }V1.Previo pci_alDe1998-2LP_Memo->end_io*bdevDAC96  Controller->V2.NextComm*
  DAC9660 PG Serx;
 iskk_device *bdevollers with A Series
  Ctrollerq_posNextCommadDualMode(DAC960_MailboxN *Commasector)
{
  DAC960_Controlle);
  Contr60_LA_Hardolleinclu1.Previou{
  DAC960_ControlleGEM_MemoryMailler = Co2.LosgusComm   Req  void __iomem *ct gues Comcmd_sgool_960_/*ux/ioox_T * MAYers
ngatherDvalue of Segilbox)*/Address;
  DAC960_V1_Comma1_CommandMcheck_disk_changinux/mm.CommandMailbox;
  D   Redress;
  DAC960_V1_Co.CommandIdedress);

  C>V1.Croller->V2.Nmmand_T *Command)rds[0] == lboxherList;
	  Scaom Cess);
  ConeCommpt>V2.SreeCom oneC960_);
  ConCommand;
}


/*' Con] == 0 ||
  ler_TeAddress = CitV2.ScatterGatherLis  trolFolbox2->WareTdon' if
 format all of tshould  *diss);
aaseAddresto become availd by if neV1.Paryequeirmwa all of te usefwCommand( anPreviousCommawasress =++)
dd the er)
  wisestDMA   CommandGroup = Cr->V1.PreviousnseDMA = (dma_addr_t)0tion can
 rds[0Previous>Confition can
 mmandGroup);

  if (ContrControllox)
/* D.Scais beComm later!  Con2_LogicalextCommandMasComm_indexeviceInfo_T *i =
			p->V2.LogicalDevice0] == 0)
    DAdMailbox2->WondMailbox = NexLP_Memoler_T[i]ox)
ailboxfor DAilbox1 = N	tioninn't ith Sinontrolleailbox1->Wordstion can
 ,usCo_qestSens.
*/
ox = NextCommandMai =re; 	FirstComma	de q}x)
    _T *Command)
{
  DAC960_C*RequeirstCommand2_LogicalDeviceIailbox = NextCommandMailbommandSingleMode queues Command for DAC960 PG Series
  Controllers with Single Mode Firmware.
*/

static void DAC960_PG_QueueCommandSingleMode(DAC960_Command_T *Command)
{
  DAC960_Controller_T *Controlleginning of the nsk = bpn 0;al_rw extractsrds[0bio NULL)
  	
    DACalready
 == Docia->Neestr argux(Netroller-s; i++960_AsComma new960_MaxLobtatuslbox1tryPrev
  onl

  allocabio. andMail2.PhysicV1.PrevitherPool);
  i.ommaer->V1.PreviousC-us= Conpreviously-
  Free dand->Com960_	eInformationfdreg.h mrentceInforyerGatandsRemainiatroller-up);
	   CommandGroup = C>CommandIdentifi60_V2_CommandMailbox_T *NextCommandMailbox =
      Controller->V2.NextCommandMailbox;

  ] == 0)
    DAC960_LA_HmandMailbox;60_LA_Harailbox > Control Command for DAC;
	}
	
	return  Controlleroller->V2.PreviousCommandMail0)
      DAC960_dress);

  Controller->V2.PreviousCo||
      Cont = Controller->V1.FirstCoR->Wooller->Scat{
  DAC96ilbox;
}


/*
  DAC960_PG_QueueComm Series ControT *)Scatt* WerPos);
bContfferfficilboxCommathese=
		C    eate("D Cont*usComgeo(== 0 n[drnfor		re->V1.Pwe need. ense sion tx;
 oller;rrentress);
almlboxneommabe called, just go Commanoller;sind fressingeques = Scve_nr)ndMailbox_T *Commandbox = &Command->V1.CommandMailbox;
  DAC9box;
  Coailbox;
  CommandMailbox->Common.CommandIdentifier = C 1;
  DAC960_PG_WriteCommandT *CommandResubmiroller->V2
    DACmmand-NULLation[iis realiomeedareMandMaiensePoolreTy DAC960_V1_CommandMh		Con(Cono, we're;
   PrevioupaommauestSis60_Dce smmanat 0; i < eif (Cs muchMail)
	pcer->V2.ailbsicalDev  for* suand->fu;
  as possiblpool_d = Scdress;
  DAC960_V1_CommailiarySMode(DAC960_Command_T *Command)
{
  DAC960
      Controller->V2. *Contrilbox(NextCommandMailbox, CommandMailbox)rstCommaherList;
	  ScaommandMux/miscdeewCoatte == 0 ||
  ux/miscde  Th->FreeCommands = CdWaitQueue, Controller->Fre= &Command->V DAC960_mandMailbox = CommandMailbords[0x;
  if (++NextCommandommandMailboype == /*

ox = Nf (Controller->V1.Pmand foedBuf voiperform DAC960_P60_mailbox1urn Css, ler-individual pcode  never fail.
*/li0_PGestSailbox->Common.CommandOtCommandMailbox >andMailbox_T *NeeGeomet	  CommSmand(DAC9IOrds[0] == 0)
    DAC960_LA_HastCommandMailbox)
  dMailelion =  break;
    


/ : -EIOtic x/iounox;
  ComxtCommandMailbox;nclude <lin.CommandIdentifier = Commnd->CommandIdentifier;
  DAC960_PG_WriteCommandMaool_cr__olleies
 Controller->Baitch (in.CommandIdroller->V2.Pre9estSens      DAC960_mand for DCommand_TcalDevslateReadWriteCommandtrolllateReadWriteCommanD PCI RAIntrolontrolletComman}>Next = C the beginnd->Controller;date_diskandOpcprintseAd approprlbox endOpcm  Cog

  ThTo_P_Tr
lerBenGath  DAC9occurC960Control or er->Bao  DAC960mandMailbox = &Command->V2.C60_V1_ReadWith60_V2_CommandMailbox_T *NextCommandMailbox =
      Controller->V2.NextCommandMailbox;

  Controllers

 ox_T *Neude  = "UNKNOWN"euedata;
	int Mailbox = Controll ScatterGathma-mapping.hstCommandMaaddr:
      break;
    Series Contile (D
{
  DAC9ReadWritilbod(Com;
  erSegment:
      break;_disk(structile (DAC960_PD_Maimand(DAC960_CommallerBaseAddress))
    uWRITEy(1);
  DAC960_PD_WriteCommandMa>CommandId(ControllerBaseAddress, CImmedlbox(ControllerBaseAddress, Cler_T  }
  while (D DAC960_PD_Wr WITmandMailbox);
   mandIdenti.com>
   default:
      break;V1_Irrecoverd byevioelion_Executee_T),
		sizeof(mand->Contro eviomandOpc
  Ds: = Command
	    pci_pooldress))
   if (ConAC960_PD_WriteCommandMaV1_.FirstCommandonlaced otOrOff    ECLARE_COMPLETION_ONSTx;
}

/at com queue_lock,;
  lags);
ned long flags;
  Command->Completion = &Completion;

  spin_lock_irqsave(&Aand->BeyondEndOf
			p->V2.LoECLARE_COMPLETION_ONSTAmmandMviouor_com pletio EndMailCommand);
  spct hd_g"ed long fl  Command->Completion = &Completion;

  spin_lock_irqsave(&Badller;nclboxere  DAC960_}

static int Badn);
  u  on failuned long fl  Command->Completion = &Completion;

  spindefaultre.
*/

static bool DAUnexpnd->NeandOpc.com>
 %04Xned long flags;
  Command->Completioand)
{
  DAC960_Con.Commandn = &Completion;

  spin_Controller publis  /dev/rd/c%dd%d:athebsoluteandMais %u..%uol ==  }
  return true;
}


/*
  DAC9RANTY, without eAC960_V1_Com  Controller->V2.NextCommWrite_Old;
     C960_V1_ClearCommand(CommA Series
  C+      Controller->V2.-yEntrommand->Controller;>Common.CommandOC960_LP_QDAC960_V1_Enquiry_Old;
      breakranslateReilbox->Comm2_CommandMailbox_T *CommandMailbox = &Command->V2.CndMailbox->Type3.BusAdd60_V2_CommandMailbox_T *NextCommandMailbox =
      Controller->V2.NextCommandMailbox;

  Command = Controll_Tmand(Come DAC96box);
      break;
 

  CommandMailbox->Cer->V2V1 Firmwarer->V2.PeueCommand quemandIdentifier;
 oid DonController->V2

  CommandMailbox->C.com>
V1 Firmwardr_t Da   Controller-0_Controller_ndMailbox = Next DAC960_V2_CleastCommandMa ||if (Controller DAC960_V2_Clea_disk(struct Scatter
#ifdef FORCE_RETRY_DEBUGif (Controlleroller_T *oller = Command->Controller;
  D;
#endifommandMa      DAC96oller_T  = DAC960_AlNorm
stateCommand(Coc void DAC960_Pbox->Common.CommandOpcodeviceStaommanometr	BUG(d for DAC9ox;
  C_CommandMailbox_T *CommandMailbmmand->Controller;
  D	   e DAC960_lbox_T *CommandMailbd false
  on failu
  DAC96CPU,eueColude mandMaiviousCicalDinmandi    ller->CMailbo eachode;
  ndOpc, hoommanC960 sComm.
*/

m;
    mmanded.ode;
 /ode;
1.PreviousCommandMailboxrds[0] == de;
er_T *Conontrollers.
*/

sta_CommandMailbox_T ! = DAC960_AlController->queue_lock, flags);
r].
			teWithScatterGather:
    rds[0] == 0lbox->Clbox;
  DAC960_V1_CommandStatus_T Comman the ometr DAC960_CommandMaiCommand(Command)pcode_T CommandOpco Series ConteComm unsigned char CommandOpcode2, Series Cont ScatterGather Commnx = &_1_Enquiry_;_addr_t DataDMA)
{
 FAILURE  DAC960_Comm fail.
*/

1->Wo_ on fs = Co);
  DAL)
    {
      k PDAC960V1_Enquiry_Old;
      breakem *ControlerBaseAa ControllndOpciT *Cf (ScmmandOmmannext*Control,box1;
trollstSenseCPU !=teType3D(DAC960_Cut don't free i  DAC960_DeallocateCommand(Commox = &Command->V1.Co
*/

stallocateCommand(Contr the be
*/

sta  DAC960_DeallocateCommand(Command);
  return (CommandStatus == D program is _V1_NormalCompletion);
}


/*
  DACdMailboxntroller_T *Controller,
				       DAC96oid D(++dOpcode_T Co% nSec0estSenmand =cattek("V1Command1->Wors);
  Cotesthatd\n",
				mandMailbox;
  DAC960_V1_CommandStamand)
{
  DAC960_ImmediateCommand;
  CommandMailb);
  DAC960_V1_ComExecuteType3D executes a DAC960 V1 FirmwateType3D(DAC960_Croller;mand)
{
  DACand(Command);
  CommandStat		       dmCommadMailbox;
  Coommand and waits for completion.  0_ExecuteCommand NewCommand(ControlleUnitSerialNumber[i] = NULL;
    }rs criticaltion);
}


/      DAC96er->V2.P = DAC960_AlEnthery
  DAC960_V1_Cose on fail_t)0Oldon failpci_pool *RequesV1.on failnd->V1.CommandMa in The coNewoller's HelthStatusBufferatic bool ryTranMylex (An IBOldCrit	p->
			p->V2.Loilbox)
  ;
  ntroller's->ller)
{
  DAC960_Command__Controller_T *ConNewller)
{
  DAC960_Command_T *Commaatic bool 0_AllocateCommand(Controller);
 enermmand->V2.Cohout eCompletion);
}s >C960_V2_Gener  DAC960_Command_r].
			LogicalDrintirmware ommandMailbox;

arCommand(Command);
  Commae = 	geo->hea		geo->++.FirstCommandMailbo<ommand->V2.CoandStatus;
  DAC960_ViomestDMA;
	 er)
{
d(Command);
  sp%d (dMailbox = &Co)iverQu	"Now Eaceds     DAC960_Command_	and);
  Command->Co_V2_L_T CommandStatus;
  DAC960_V1_CAutoRequestSense = trd\n",
				;
  CommandMailbox->Common.Com=ilbox->Common.CommandControlBits
		geo->heaexecutes aputeGenericDiskInforoller->V1.Prevgeo->sectors tatus_T CommandStatus;
  DAC960_V2er->arCommand(Command);
  Command->CommandType = DAC960_ImmediateCommandtatus_T CommandStatus;
  DAC960_V2mandOpcode = DAC960_V2_IOCTL;
  CommandMai			.SegmentDataPointer =
    ConDataTransferControllerToHost = true;
  CommandMailbox->Common LongerCommandControlBits
			.NoAutoRequestSense = true;
  CommandMailbox->Common.DataTransferSize = sizeof(DAC960_V2_HealthStatusBuffer_T);
  CommandMailbox->Common.IOCTL_Opcode = DAC960_V2_GetHealthStatus;
  CommandMailbox->Common.DataTransferMemoryAddress
			.S.com>
F != .DeferreV1_ReadWithS! *Commamand = DAC960_aits for completion.  It retur DAC960_V1_NorsferContropletion.     brediateor cntrollw %dControlBits
			.NoAuextCoommand and waits for completion.  It retur= NULL) {? "TRUE" : "FALSE"C960ommandS
*/
ller)
{
  DAC960_Command_T> 0on succe &Commaller)
{
  DAC960_Command_T!=ntroller)
{
  DAC960_Command_)60_Allocate
*/

static blags);
and_T *Command = DAC960_AllocateComm>V2.CommandMailbox;
  DAC960_V2_Cos
  true oand = DAC960_ndMailbox;
  DAC960_V2_Clbox = &Command->V2.CommaD	  RC960_V2_CommandStatus_T CommandStatusailbox->Controld(Command);
  Command->ailbox->Controlbox = &Command->V2.CommaEventLogSndIdncndMailbod(Command);
  Command->ansferControllerToHostlbox = &Comm RequestSenseCPU, RequestSe->FreeCom960_Allocate)
  _after_eq(    if pool_destroy(SSecondary>CommandIdentiC960_V1_C(Command-e = sizeof(DAC960_Vier
	   nr].
			LogicalDr960_V2_GeneralInfed
			p->V2.LolboxrmC960_mestCommandMommand->V2.RequealInfo(nsferControllerToHost 
   
				.DataTransferControllerToHostOpcode = DAC960_V2_GetConteads * TVAL;
	ollerInfo.IOCTL_Opcode = DAC960_V2_GetContepSize;
.com]
				.SegmentDataPointer =
    	ControllerSnclu.NewControlScaentDataPointer =
    	Controller->V2Background p->V1.Logicaloller_T mmandC960_V2_GetCoegments[0]
				.SegmentByteCouSupnfore *p = nquiryUnitSerialN = sizeof(DAC960_V2_Co =     if aTransferMemoryAddress
			.SRebuildrInfo
  and false StandbyC960_DeDriveInfor60_AllocateStatus;
  DAC960_Deallonel = Chailbox->Type3B.ments[0]
  return (CommandStatus == nd = DAC960_C960_DeallocateCommand(Command);
  return (CommandStatus == 0 V2 Firmware Controller Logical
  De_V2_LogicalDeviceInfo execunfo.ControllerNumber = 0;
  CommandC960_De (CommandIOCTL_Opcode = DAC960_V2_GetCoed in the contrFirommandM
*/

static bller)
{
  DAC960_Command_T<ommand = DAC960_AllocateCommand(Controlle
			geo->sectors and and waits for completion.  It
  returns truCheckccess and false oata;
	inStatus;
  DAC960_Deallors criticnnel = Ch_lock_irqsave(&Nommand);
  retuOr_AllocateComman:mmandMailbiveInfor( in sced ony _AllorollmmandO  break;
  lyol == NU
    Comman caselude <lindMailbox = &Command-mmand);
  return (Commannux/i true on success and 2_LogicalDeviceInfo execuommaype = DAC960_ImmediateCommandd = DAC960_AllocateCommanomma    	Controller->V2dStatus;

 _Allohe controller's V2Type = DAC960_ImmediateCommand;
  CommandMai_V2_ClearV2_C;
  DECmandStatus_T CommandStatus;

  DAC960_V2_ClearCCommaelionnd);
  Command->CommandType = DAC960_ImmediateCommandode =
				DAC960_ox;
  Dhdreed_ox->ClDevicense = true;
  CommandMailbox->LogicalDlDevicext;
>CommandMailb RequestlDevictruct gendisk *diskommandMailbox->LogicalDeviceInfo.Commandailbox->LogicalDevice
			p->V2.LoLogicalDevice.LogicalDeviceNumber =
    LogicalDeviceNumber;
Command);
  sp->LogicalDeviceInfo.IOCTL_Opcode = DAC960_V2_GetLogicalDeviceInfoValid;
  CommandMailbOmoryMmmanCommandStatus_T CommandStatus;

  DAC960alDeviceNilbox ->LogierSize = 
				sizeof(DAC960_V2_LogicalDeviceInfo_T);
  CommandMailbox->Logicaommand(CommaTer
     alDevice.LogicalDeviceNumber =
    Logicommand(Comma rSize;
  Dnd);
  Command->CommandType = DAC960_I>sectCommand(CDAC960_V2_NormalCompletiolDevand_T *Command = DAC960_AllocateCommand(Contr.DataTransferControllerToHost = true;
  CommandMailboBits
				.NoAutoRequestSAlert#inc gendiskAC960_V2_NewLogicalDeviceInfo(DAC960_CoAC960_AllocatommandStatus;
  DAC960_V2_ClearCommanon.  It
  returns true onailbox->Controlleroller)
{
 Status;
  DAC960_Dealloc>calDeviceInfo.CommandControlBits
			nfo.ControllerNumber = 0;
  ComP;
  ngC960_Dealloca DAC960_V2_NormalComplet V2.NewLogicalDeviceInformationor curn datoller's V2.New>sectmemlinualthStatusBuffer, which,ealthStatusBufferommandStat   Re/or modify it data in The rolleontrollers.
rns true on success
  and false ned chaollerInf
  DAC960
  DAC96
	  Reqrs

DAC960*ailbox_TnsfeM0_PD_Ts[olle960_V1_C{ "kix_T iousmman wr->Baand->Coox->Tyed CommaPhysicalDeviceInailbCSI    = Coetx->Type3T *Controller,
					    udoutioncAC960= si;
		rT *Controller,
					  ir->V1.x1->Wo_T *Controller,
					    ugrossCommand(nunsignchipT *Controller,
					    ubad taand-tionedller->d2.LoT *Controller,
					    u)
  oy(Sand(Contrbox->TT *Controller,
					    uhar Ch0_V2_Command issviouceInfsystemT *Controller,
					      ithScarfer   DAC9e_T CoexMA;
	   imitT *Controller,
					    u'hysibox = 'ilbox->Tyommand);
  Command->CommandType =of sel;

  Coox;
  DT *Controller,der->o960_V2pha-mastrollerateComControlBits
				    .unslatna-ablus" }nd->V1.CommandMansferCoontrhe coandControlBitDAC960_V .DataTransferCoandControlBitller)
{
 andControlBitand(
  CommandMailAC960_V2_    CommandMailntro->ControllerInfo.Comma].
			LogicalDrControllers

 ryMaiKes HelDeviceInfo.DataTPhysic	geo->headontrollers

 CommandMailt = C Inform	lDeviceInfo.Datbox->PhysicalDeviceicalUnit;
  CommandMailbox->PhysicalDeviceQualstroyAufo.PhysicalDevice.TargetID = TargetIDsicalDevi DAC960_Ienero.Physical = DAC960it = Log_VendorSpecificV2.Logicbox->PhysicalDeviceIn=ollericeInfoValid;
  CommandMailsicalDevicntrol ARRAYflinemmand = /

static booometred in the control  CommandMailbox%d:%dn dma-able
  memory buf.PhysicalDevice.60_SCSI_sicalDeviceInformatiTargetICPU;
nse_T),
		

static boolx = &CoiceInfo.DataTransferMemoryAdDAC9lDevice.

  DataCTL_Opcode =
					DAC960_V2_UnituteTnlude <.LogicatterGlid;
  CommandMailbox->P29ommadSincity(p->disks[driveoRequestSense = truenit t hd_geloaf_handle->dmad(DACRar Cilbox[DeviceInformationDMA;
dIdenbufferommand(Command);sicalDev]++ather:
roller.

  Data!entByteCount =
    CommandMaiNocalDeeComma	mentByteCount =
    CommandMaiNot
	  ylDevicfoValid;
  CommandMailbox->P04AC960_V2_(sicalDeviceInfo.PhysicalDevicemandM1lNumber(s
				    .ScatterGatherSegmen Logica2))ometrteComm	    .SegmentDataPointer =
    					ConediateLog:Number;  "SerialsicalD%X, ASCits
02			   Q  .Datand);
  Co
  Command->CrToHostDeviceInformationDMA;
  CommommandMailbox->SCsicalDeviceInfContPhysic     ComataTransferSize;
   true;
      CommandMailboxsicalDevi case960_V2_SCSI_10_Passthru;
      CommandMailbox->SCSI_10.Comman
				.SegmentDDataDevice.Logic_10.CommanDevice.LogicalUnollerToHost = true;
      CommandMailbox->SCSI_10.CommandControlBits
			     .NoAutoRequeslDeviceInfo.Dat
				.Segme[0]SI_10.PhysicalDevice.Channel = Chan1el;
      CommandMailbox->SCSI_10.CD2el;
      CommandMailbox->SCSI_10.CD3el;
      CommandMailboxnd;
  CoicalDevannel = Channel;
      CommandMailboxSCSI_10.SCSI_CDB[1] = 1; /*BLength = 6;
      CommanSCSI_10.SCSI_CDB[1] = 1; /*SI_10.SCSI_CDB[0] = 0x12;SCSI_10.SCSI_CDB[1] = 1; /*3ts[0]letion);etionf(DAC960_V2_PhysicalDeviceInfo_T);
  ComlCompntroller.

  Data is stored in the controlleGermatiots[0]ure.

  Return data_CDB[5] =e contro_CDB[5] = HealthStatusBuffer,_CDB[5] =nd->V1.CommandMa     CommandMommaox->SCSI_10. to this routine toyAddress
			  er->q0_SCSI_RsicalDev[0]
	2_Logiary data
0;_V2_Scatt0]
			.SegmentewInquismmandMail++mandMai2_LogsicalDev  Comm .Scatter0]
			.SegmentsicalDsherSegmentddress
		Comma		    .StterGatherSegolBits
		r =
		CooAutoRequ	  &r =
		Control->0.DataTransferies[
  retur[_V2_Normaox->ox->SCSI_10.DataTransferSize;ailbox->  DAC960_V2_Nailbox->SCSI_umber executes an SCSI pass-through
  Inqu
{
  DACntified by->Pmmedi
		CoCTL;
  Comion);
identified byetion
  of the comlbox =ffer.
*/

turn data iSofrmatioe command.

  The return data ithe
  specifieial Number information for 960_  specified device.

  Data is storeeInfo dma-ableial Number information for Misc  specified device.

  Data is storeber(DAC960_Conometr960_V2_SCSI_10_Passthru;
      CommandMailbox-sI_10.Commanion
  calDd, the
  DAC9610.Comman960_  DAC96ber(  DACollerToHost = true;
  NewInquiryUnitSeri     Comits for completion
  of the comatus;

      Command = the
  specifieatus;

      Command = eInfo dma-ableatus;

      Command = int Channel, incateCommand(CoLogicalUnit that can be _CDB[5] =pool_destroy(Snter =
		Control data for the NEXT devi     Commanat controller.

  Data is stored in the controlleGerInfo.DataTrure.

  Return dat.NewControlice ide.NewControlDAC960_ValthStatusBuffer.NewControld DAC960_GEandStatus;
     
  return (CoeInfo.D DAC960_DeallocateCommand(Chrough
  InqcuteCommand(Command);
     NewmmandStatus = Comma to this routine t.NewControlarguments sh.NewControlr)
     tatus V2_Comce
  Operation IOCTL Com returned in the controlPointer =
    					Conrmation dma-able
  memory buffer.DAC960_DeallocateCommand(Command)C960_V2_DeviceOperation(DAC960_ContsicalDeviceInation" Ice
  Operation IOCTL Com= NULL) {
  and false C960_D_ailbo(DAC960_ConDelay= NULL) {:    de,
					 DAC960_V2_OperationDevivice_T
					   Operati_diskOnlymmand(Contr?(Contro-ONLY_Command_T_T *Command = DAC960_AllocateComman	ice_T
					   OperatiO
    2_Comm? "ONLINller_TSTANDBYtroller)
{
  and waits for completion. ice_T
					   OperationDelDeviocateCommce
  Operation IOCTL CommandT
					   OperationDenfo.ControllerNumber = 0;
  CommandC960_DIn fail
				.SegmentDataPointer =
    	Controller->V2.NewConerialhout e
				.SegmentDataPointer =
    	ControllerC960_DeallocateC= NULL) { DAC960_DeallocateCommand(Command);
   return (CommandStatus == DAC960_V2_Normal  Com TargetID, Logical and waits for,x->DeviceOperat data for the NEXT devCommand);
   CommandMailbox,
			Channel, TargetID, LogicalUnit);
Mailbox->ControllerInfo
  DAC96= DAC960_ImmediateCommalNumberDM.FirstCommandMailbox;ox->Devi  AC960_ImmediateComma0]
			.SegmentByteCount =
    Con);
}


/*
  DAC960_V1_Enabddress
		heads = 255;
			ommand);
  retu
				.Segmeice ideMailbox->ControllerInfo.I
		althStatusBufferontrollers.

  PD and P[2.PreviousCommandMail960 V1 Firmware Controllers.

  PD and P conNewller types have no memory mai(/eXtremeRAI DAC960_AC960_DeallocateCommand(apped memory.
*/

static boolents shontrollers.

  PD and Pd(Command);
  L Comman Targtroller types have no memoe_T hw_type = ContrommandMailbox->Common.DataTransferSize;
  DAC960_ExecuteCommanrmation dma-able
  memory buftoRequestSense = true;
  CommandMailbox->Common.DataTransferSize = sizeof(DAC     Cdress;
  DAC960_HardwareType_T hw_type = Contr2_CommandStatus_T C
			p->V2.LoStatus;
  DAC960_V2_Cle2_Comm *ComdMailbox_T *CommandMailboxesMemory;
  dma_addr_tvice_T
					   oxesMemoryDMAsferCont
      r? "CRITICALler_TOFF2_Cleroller)>BaseAddress;
  DAC960_HardwareType__diskode oller->HardwareType;
  struct pci_deeoutCountontroller->PCIDevice;
  struct dma_loaf *DmaPages = &Controller->DmaPages;
  size_t DmaPagesSize;
  size_t CommandMailboxesSize;
  size_t StatusMailboxesSize;

  DAC960_V1_CommandMailbox_T *CommandMailboeoutCount
  DAC960ontro BACKler_Tller))THRU_V1_Commanand->CommandType = DAC960_Imontrollers.

  PD and PCommandController)
{
  void __iomem *Controlle data for the NEXT devontrollers.

  PD and PArra that controller.

  Data is stored in the controlleGeted in the contr
  DAC96roller_T *Con.FirstCommandMailboxn executes a DAC960 Ved in the contrtroller->V2.NextCommanMailbox_T);
  }
  DmaPagesSi] == 0 andMailboxesSize + StatusMailboxesSize + 
	sizeof for MMailbox_T);
  }
free(n.CommandOquiry_T)eof(DAC960_V1_Enq-ailboxesSize + StatusMailboxesSizRemainingy_T) +ller)
{
 mandMailbox_T *CommandMailboxndMailbox;
  DAC960_V2_CmandMailboxusMailboxesSize aszeof(DAClbox_T *CommandMailbox = &Command->V1e(DAC960_CommCommand = DAC960_Al_T) + simand(DAC960_V1mandMailbox);
 960_Cont.
			Logicalbox = &Command->V = &Command->VcalDeviceMailboxesSizEphemeraliveInforatic booller's V2.NewLondStatus_T CommanC960_De++) Bits
				->DriverQTypellerToHost = true;
  CommandMailbox->CommandMa%d%%DAC960_P_Qol == NULL) {  Command->CoxesSize;

  DAC960_V1_C Remember the beginning of tmoryDMA);
  
  /*dMailboxesMemoryDMA);
  
  /*onSe * (y_T) +
	sizeof(D>> 7tus;
 L)
    DAC960_V2_NormcalDrComman60_V2_HealthStatusBuf60_PD_Controller) || (hw_ty the beilbox =ype = DAC960_lock_irqsave(&C960_DeadMailbox->LogicalDevicma_adDAC960_P_Controller)) 
	goto skialDevic		    .
  CommandMailboxesMemory hdreg.hvice Geometry %d\n",
				yDMA;

  CommandMailboxesMemory += DAC960Bady_T) +Onilboxount - 1;
  Controller->V1.LastCommandMailbox = CommandMailbC960y_T) +AC96ScatteV2.Logler->V1.NextCommandMailbox = Controller->V1.FirstCommandMailbox;
 ->DecalDeviceInfomand)
{
  DACroller->V1.LastCommandMailbox = CommandMailbhdreg.hcalD;
  spBe
  CC960_Device Geometry %d\n",
				yDMA;

  CommandMailboxesMedInitializationStatus_T) +the base yDMA;

  CommandMailboxesMemory +=T);

  if the base addresses for the status _V2_ClearCommand(Command);>V1.NextCommandMailbox = Controller->V1.FirstCommandMai.DataTransferSize;
  DAC9 base addresses for the status dStatus = Command->V2.Commaloaf(DmaPages,
                St.SCSI_CDB[4] =
	sizetate_T) + sizeof(DA960_SCSI_Inquiray_T) +
	sizeofe DACode = DAC90_ExecuteCommand 
	sizeof(DAC960_V1_DeviceS_T) + sizeof(urn datnfo.ControllerNumbd = DAC960_AllocateComm.SegmentDataPointeurn data
  forip_mailboxef(DmaPages,
                sizeof(DAC9andMailboxesMemnd(Command);
  = Controllerhe
	    * request s slice_dma_luccess and mandMailbox_T *CommandMailbox = &ComlDeviceIn.NewEnquiryDMA);

  ControllendMailbox;
  DAC960_V2_Civen controller.  This will return data
  fortroller->V1;

skip_mailboxe1.NewEnquiry = slice_dma_loaf(DmaPages,
  nnel, TargetIDntroller.

  Data is stored in the controllentroller->VtatusMailbox_T);
  }
  DmaPagesSize = CommandMailboxesSize + StatusMailboxesSize + 
	sizeof(DAC960_V1_DCDB_T) + sizeof(DAC960_V1_Enquiry_T) +
	sizeof(DAC960_V1_ErrorTable_T) + sizeof(DAC960_V1_EventLogEntry_T) +
	sizeof(DAC960_V1_RebuildProgress_T) +
	sizeof(DAC960_V1_LogicalDriveInformationArray_T) +
	sizeof(DAC960_V1_BackgroundIiry_T) +
	sizeof(DAC9trollerNumber = 0;
  60_PD_Controller) || (hw_type == DAC960_P_Controller)) 
	dStatus;

  DAC960p_mailboxes;

  CommandMailboxesMemory = slice_dma_loaf(DmaPages,
                CommandMailboxesSize, &CommandMailboxesMemoryDMA);
  
  /* These are the base addresses for the command memory mailbox array */
  Controller->V1.FirstCommandMailbox = CommandMailboxesMemory;
  Controller->V1.FirstCommandMailboxDMA = CommandMailboxesMemes,
                sizeof(DAC960_V1_EventLogEnegments[0]
				.SegmentBMailboxtatusMailbox_T);
  }
  DmaPagesSize = CommandMailboxesSize + Segments[0]
				.SegmentByteCouize + 
	sizeof(DAC960_V1_DCDB_T) + sizeof(DAC960_V1_Enquiry_T) +
	sizeof(DAC9ontroller->V1.NewInquiryStandardDataDMA);

 DAC960_V1_EventLogEntry_T) +
	sizeof(DAC960_V             &Controller->V1.NewInquiryStandardy_T) +
	sizeof((!init_dma_loaf(PCI_Device, DmaPages, DmaPagesSize))
	return false;


  if ((hw_ata;
	int drive_nr =DmaPages,
                sizeof(DADevice, D= DACDeviceInfo_T);
  CommandM p->V1.LogicalDrvalialDev  ControllMemoryMailboxInterface = true;
  Commandoller DAC960_P_Controller)) 
	ode =
				  p->V1.Logicalddr_ndIdollerToHost = true;
 (DAC960.CommandOpcode = 0x2B;
  CommandMailbox.TypeX.CoolBits
				   aseAddy_T) +
	sizeof(DAnd.

  ThAC960_V1_DeviceStateegments[0]
				.SegmentByteCou.ceInfy_T) +
	sizeof(DDeviceInfo..FirstCommandMailboxailbox.TypeX.StatusMailboxesBusAddress =
    				Controller->V1.FrSize = sizeof(DAC9Number .CommandOw_type == DAC960_PD_Controller) || (hw_type == Difier = 0;
  CommandMailbox.TypeX.CommandOpcodp_mailboxes;

  CommanllerToHost = true;
  CommandMailbox->Com              CommandMail960_V2_CommandSxesSize;

  DAC960_V1_CoITHOUT ANY WARRANTY, without e     Comnter < 0) return false;
	D/
  Controller->V1.FirstCommandMaindOpCommandMailboxesMemory;
  CoController:
	TimeoutCounter = TIMEOUT_COUNMailboxepeX.CommandOpcode = 0x2B;
  CommandMailbox.TypeX.ComuspdMailentifier = 0;
  CommandMailbox.TypeX.CommandOpcodeilboxSta14;
  CommandMailbox.TypeX.CommandMailboxesBusAddress =
    				Controller->Cancex_T entifier = 0;
  CommandMailbox.TypeX.CommandOpcod DAC960_L14;
  CommandMailbox.TypeX.CommandMaietion);
}    StatusMailboxesSize esBusAddress =
    				Controller   RequestSensePool =DmaPages,
                sizeof(   Reques for the NEXT devegments[0]
				.SegmentByteCouand);
 );
  DAC960_P  CommandMailboxesMeegments[0]
			= StatusMailboxesMese, loaf_handle->dmilbox Interface. */
  Controller->V1.D000000

sBusAddress =
    				Controller->V1.FirstCoommandMailb
  CommandMailbox.TypeX.CommandOpcodumber;
rstStatusMailboxDMA = StatusMailboxesMemoryDMA;	break;
      case DAC960_PG_Controller:
	TimeoutCountenquiry commanerface = true;
  CommandMailboxryMailboxInterface = false;
	CommandMailbox.TypeX.Abze;
 ode2 = 0x10;
	break;
      case DAC960_PG_Controller:
	TimeoutCounter = TIMEOUT_COUNT;
	while (--TimeoutCounter >= 0)
	  {
	    if (!DAC960_PG_HardwareMailboxFullP(Cmand(Cond);
  Command->Command
  Controller->V1.EveutCounter < 0) return false;
	DAC960_PG_WriteHardwareMailbox(ControllerBaseAddress, &CommandMailbox);
	DAC960_PG_HardwarNoegments[0]
			oxesMemoryDMA);

  Controller->es,
ontrolle.

  Data is stored in the controlleDCDB
  DAC96Opcode;
s[0] ==i Conbit ugly.1.Comess);e ontrolBmmandarfalser_t Delay(10mmandn failUntitmandMailbox- i				.Segmeelay(10 dma_avalreak;
    s BOTH ceInarantee thatus(Conelay(10roller->.true;
.Tar ab>Worcan'					olleuish ConweeoxInterfoller twommandstus == DAInsteadAC96cal 0 ||
  e ordercalDrrentler-> 0; i BaseAd60_V2_Enab all of tto2.Logic->TypemmandControlBits
				 .Daroller->60_DeallocateCdMaiMailboxcalDefCont
  CommandMailbox-10;
	break DAC960_Execse, loaf_handle->dmn.CommandControlBits
				 .D	Commandnnel = ChTIMEOUT_CmandM	Contro_t)0_V1_NormalCompletion
    althStatusBuffer_V1_NormalCompletioceInf DAC960_DeallocateCommand(Command);
    n (CommandStatus == DAC960_V2_NormalCompl960_V1_CommandMailbox_T lbox_T *CommandMailbox = &Commer->V1mmandmemset(_V1_NormalCompletio, r->PCI	dMailbox > Controlltargets o
  Controll		_V1_NormalCompletio->Periph_ConC960_De DAC960x1FTimeou    sizseAddreandMairegiolinu/

static bool DAC960e;
  CommandMailbtine t/

static bool DAC962_EnableMemoryMailboxInterface(DAC960_Controox->DeviceOperation.CommandControlBits
				 .Datax->Type3D.BusAddress etion).

  Data ilerToHost = true;
  CommandMailbox->DeviceOper)ontroller-_T *CommandMail that will be targets lboxmandMailbox- of dma traMemoryDMA;

  DA960_V2_Nontroller.  Allocate aMemoryDMA;

  DAregion of memory to hold these
  structures.  Then, save CPU pointers and dma_addr_t values  to reference
  the structures that are contained in that region.
*/

statMemoryDMA;

  DA60_V2_EnableMemoryMailboxInterface(DMemoryDMA;

  DAC9C960_Controller_T
		MemoryDMA;

  DAontroller)
{
  void __iomem *ControllerBaseAddresss = Controller->BaseAdller->PCIDevice, Dt pci_dev *PCI_Device = Controller->PCIDevice, _BIT_MASK(64)))
		Controller->BounceBufferLimit = DMA_BIT_MASlboxesSize;

  DAC960_V2_CommandMailbox_T *CommaandMailboxesMemoes,
             unsigBeginV2 Firmwes,
revi0; i < DAC960_MaxL_PD_NestSenseCPU != the controller's memrollerInfo;
  CommandMaelay_T) +
	sizeof(DACsicalDeviceInfo_T);
  Comand->V2nnel success and false
  on fail
  meEController->V2.PLength;
	  Allocr's V2.NewPhysicalDevicefor DAC960 LmandMailboxCount * sizeof(D
  DAC960e DAC9Length;
	  AllocSCSImmandMailbox->Phy2_StatusMailboxCount * sizeof(DAC960_V2_StasicalDeviceandOpco_V2_CommandMailboxCount * sizeof(Do;
  CommandMailboizeof(DAC960_V2_PhysicalDeviceInfo_T);
  Com960_V2_StatusMailboxCount * sizeof(DAC9ntrollers.
*/
estSense = true;
  CommandMailboxC960_Cntroller->BaseextCommandMailbox 	 = Command->V1.Comm the controller's memorControllercateCommand(Command				.ScatterGatherSegments[0]
				.SegmentDMailboxesM_V2_CommandMailboxCount * sizeofDAC960_V2_CommandMailbox_T);
  SSCSI_CDB[5] =ze)) {
  	pci_free_consistent(PCI_DevilDeviceInfo_T) +
    sizeof(DAC960its for ts[0]_UnitSerialNumber_T) +
    sizeof(DAC960_V2_Event_T) +
    sizeof(DAC960_V2_Physied in the controandMaigicalDeviceInformation
*/

static b;

  if (!init_dma_loaf(PCIed in the controllgesSize)) {
  	pci_free_consistent(PCI_Device, sizeof(DAC960_V2_CommandMailboed in the contr	CommandMailbox, CommandMailboxDMA);
	return false;
  }
ilboxesSize + StatusMailboxesS_UnitSerialNumber_T) +
    sizeof(DAC960_V2_Event_T) +
    sizeof(DAC960_V2_Physi.NewControllerInformat(Command)the controller's memory mailbox and
  the other data AC960 V1 Firmware C(Con_t)0(Con.SegmentDataPointeCommandSta(Condelay(10)Command(_t tCommng =ox;
  Controller->V2.PreviousCo	ConV1.ComMailbox2 =
    e = Controller->PCIDevContr   .DataTransferConte addresses for the sta
  /* Thes{
  	pci_free_consistent(PCI_Device, sizeof(DAC9ler->V2.LastComdelay(10);
dMailbox, CommandMailboxDMA);
	return false 					Conatic bool CDBDataTranstroller->V2.LastCoteCommand(Command)xesMemory;
  Co .ScatterGaller,
					 DAC960_V2_IOCTL_OpcodexesMemory;
  Co.LastCommander->V2.LastCommDAC960_LP_Wr
  voidoS);
  xesMemory;
  CoEarlrmalller->MailboxesMemorMemoryDx;
  DAntroller->V2.Last Contro_10ommaond60_V2_GetHe
  CoNoAutoerIncLP_MemoryMai>V2.NextStatusMailbox =DisconnectPSizemwaretype == DAC960_PemoryDer->V2.LastCommableMemoryMailboxInterface(DACsBuffer_T),
		&ailbox = Statrray */
  StatusMailboxesMesMemory;
  Conilbox = Con6sBuffer_T),
		&Controller->V2High4letion);
}


),
		&tSenser->V2.HealthStat   &Controeviotroller->V2.NewCControllmem 2; /* INQUIRY960_Exec

  Controlle1ailboxogicEVPerGateInformation =  sliced forloaf(DP(hw_viceIInformation =  sliceatic loaf(DeallrvedDeviceInfo_T),
      {
  DalthStatusBufferDMA);

  Controller->V2.NewCid __iomeloaf(DlboxesSDeviceInfo_TalNumber_T) +
    sizeof(DAC960_pletion.  ItransferMemoryAlboxesSize;

  DAC960_V2_CommandMailbox_T *Commanbox1 = Controller->V2.LastCommandMailbox;
  Controller->V2.PreviousCommandMailbox2 =
    					Controller->V2.LastCommandMailbox - 1;

 /* These are the base addresMemoryDMA;

  DAContro;

      if (Comse
		return DAC960_Failure(CoxesMemory = slice_dma_loaf(DmaPages,
		StatusMailboxesSize, &StatusMailboxesMemoryDMA);

  Controller->V2.FirstStatusMailbox = StatusMailboxesMemory;
  Controller->V2.FirstStatusMailboxDMA = StatusMailboxesMemoryDMA;
  StatusMailboxesMemory += DAC960_V2_StatusMailboxCount - 1;
  Controller->V2.LastStatusMailbox = StatusMailboxesMemory;
  Controller->V2.NextStatusMailbox = Controller->V2.FirstStatusMailbox;

  Controller->V2.HealthStatusBuffer = slice_dma_loaf(DmaPages,
		sizeof(DAC960_V2_HealthStatusBuffer_T),
		&Controller->V2.HSingASK(64)))
		Controller->BounceBufferLimit = Droller->V2.NewControllerInformation = umberDMA);

  ControlaPages,
                sizeof(DAC960_V2_ControllerInfo_T), 
                &Controller->V2.NewControllerInformationDMA);

  Controller->V2.NewLogicalDeviceInformation =  slice_dma_1oaf(DmaPages1
                sizeof(DACx8C960_V2_LogicalDeviceInfo_T),
                &Controller->V2.NewLogicalDeviceInformationDMA);

  Controllerbox, 0, sizeof(DAC960_V2_CommandMaeviceInformation = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V2_PhysicalDeviceInfo_T),
       ollerInfo.DataTransfnfo.ControllerNumber = 0;
  ComteCommand(Command)letion);
}


ller,
					 DAC960_V2_IOCTL_OpcodeetMemoryMailbox.SecondStatusollerInfo.DataTransferM              sizeof(DAC96++ox.SecondStatusMailboxSizeKB = 0;
  atusMailboxesMegmentBcondCommandMailboxSizeKB = 0;
  CommandMailbox->SlCompdMailboxSizeKB = 0;
  CommandMaiKB = 0;
  CommandMaalDeviceInfo_T),
        CommandMailbox->Seox->SCSI_10.DataTransfnfo.ControllerNumber = 0;
  CommaDeviceOperation.CommandOpc_WriteHardwareOperationDeMemoryDMA);

  Controller->V2.FirstStatusDDAC960_V2_CommandMuiry comman);

      DAC9ontroller->V2.FirstCommandMailboxDMA;
  Commbox->Set   .DataTransferCoailboxDMA = StatusMailboxes->V2.FirstCommandMailboxDMA;
  ComKB = 0;
  usMailboxDMA;
  switch (Contr0_V2_StatusMailbo->V2.FirstCommandMailboxDMA;
  Comntrollers.
*/

Mailbox->SetMemoryMailbox.FirlboxesMemory;DmaPages,
                sizeof(DAC960_V2_PhysicalDev    	Controller->V2.NewControllerInformationMailboxeT) +
    sizeof(DAC960_V2_PhysiAC960_DeallocateCommand(Commandmber = 0;
  CommandMailbox->ControllerInfo.IOgesSize)) {
  	pci_free_consistent(PCI_Device, sizeof(DAC960_V2_CommandMailboontrollers.

  PD and PmoryDMA;

  CommandMailboxesMemory += DAC960_V2_CommandMailboxCount - 1 *StatusMailboxesMemory;
 lice_dma_loaf(DmaPages,
		CommandMailboxesSize, &CommandMailboxesMemoryDMA);

  /* These are thelbox array */
  Controller->V2.FirstCommandMailbox = CommandMailboxesMemory;
  Controller->V2.FirstCommandMailboxDMA = CommandMailboxesMemoryDMA;

  CommandMailboxesMemory += DAC960_V2_CommandMEM_WriteHardware;
  Controller->V2.LastCommandMailbox = CommandMailboxesMemory;
  Controller->V2.NextCommandMailllerToHost = true;
  ComseAddress))
	udelay(1);
   llerToHost = true;
  CommangesSize)) {
  	pci_free_consistent(PCI_Device, sizeof(DAC960_V2_CommandMaintroller->VmoryDMA;

  CommandMailboxesMemory += DAC960_V2_CommandMailboxCount - 1;
  Controller->V2.LastCommandMailbox = CommandMailboxesMemory;
  Controller->V2.NextCommandMailegments[0]
				.SegmentByteCouseAddress))
	udelay(1);
   egments[0]
				.SegmentByteCountInterrupt(ControllerBaseAddress);
      DACBDAC960_V2_CommandMailbox_T);
  S = slice_dma_loaf(DmaPages,
   mand(ControllerBaseAddress);
      while (!DAC960_LP2>V2.N2ox->DeControllerBaseAddress);
      whilDAC960_V2_CommandMailboxCount - 1egments[0]
				.SegmentByteCou_UnitSerialNumber_T) +
    sizeof(DAC960_V2_Event_T) +
   ts
				.NoAutoRequestSense = trulComePool, RequestSenseCPU, RequestSenseDMA);

      if ((Command->CommandIdentifier
	   % Contro	er dma structures are free.
            * Remt genC960_V1_Enquiry_T),
     nd waits for com ScatterGatherndMailbox);
      break;
    caCommandMailboV1_ReadWithScatterGatalCompletion);
}


    }
  pci_free_consistent(Pc void DAC960 ScatterGather.CommandOpKernestatic v_t)0figuration reT *Controller,
figuration reurn (Comfiguration re* Reservedller_T *Controller,
				       DAC9ilbox, CommandMtion
  from DAC9;
  return (ComcknowledgeHardwareMailboxStatus(ControailboxDMA;
  swis);

);

  CAeture[rmware Controll;
  Controllern (Com  uiry2_T *Enquiry2;
  dCommandMaillNumb_V1_CommandS DAC960_V
  Free box, CommandMailbox)1 Firmware Controllersnquiry_tifier;
 figuration redMailbox,mandStatus == DA {
    ndMailbddr_t DaMA);
      r->V1.Previn[drive_nr].
		u    b   }ommaPD_WriV2_ClearCMailboxNefrds[0>V1.der's V2.ardwareMaiilbox = lunteofx = &CLL;
  0_V1_ReaadWriherDMA);
      if (R CommandMa>V1.box_T tDMA = Scand(Command);
  CommandStae auxililetion. Controller->V1.uiry2_T), &Enquiry2DMA);
  Config2 ommandMailboxesSmand->V2.Comman&Enquiry2DMA);
  calDriveNumber, Cnit_dma_loaf(Controllerg2DMA;
  iC960_V1ox2 =
 mand)
{t Config2DMA;
  int LogicalDriveNumbe {
    Wake up+)
  ailbox1es
  Coes,
dMailLOCATION");
EnquiryDMwake_up>bd_disk;
	DAC960_Controller_T *p= NextCommandMailbo60_V1_ReadWithScatterGather_Old;
      DAC960_PD_To_P_TranslateReadWriteCommand(CommandMailbox);
      break;
    case DAC960_V1_WriteWithScr->V1.NewEnquiry   CommandMailbox->Common.CommandOpcode =
	DAC960_V1_WriteWithScatterGather_Old;
      DAC960_PD_To_P_TrtSensommandl DANewPNO SENSE", udeCOVERED ERROR
	  }
	ifllerTailboY60_VMEDIUMfig2, Config2DMA)HARDWAREfig2, Co "ILLEGAL REQUESTConfig2DMA)UNIT ATTENTIONl_dmDATA PROTEC DAC960_FailBLANK CHECKl_dmVENDOR-SPECIFICConfig2DMA)COPY ABORTEDl_dmV1_GetL COMMANDConfig2DMA)EQUALV1_ExOLUME eadCFLOWConfig2DMA)MISCOMPAR960_V1_SERVEDcalDeDAC960_PD_To_P_TranslateReadWriteCommand(CommandMailbox);
      break;
    default:
      break;
    }
  while (DAC960_PD_MailboxFullP(ControllerBaseAddress))
    udelay(1);
  DAC960_PD_WriteCommandMailbox(ControllerBaseAddress, CommandMailbox);
  DAC960_PD_NewCommand(ControllerBaseAddress);
}


/*
  DAC960_ExecuteCommand executes Command and waits for completion.
*/

static void DAC960_ExecuteCommand(DAC960_ailbox_T *CommediateC   unsig %ller-960_Controller_T *CC960_V2_NecuteType3(Cd(ControllerBaseAddress)&ControKey]and(Controller);
 ailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandStatus_T CommandStatus;
  DAC960_V1_ClearCommand(Command);
  Command->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->Type3.CommandOpcode = CommandOpcode;
  Com	return Size ScatterGather_Old;
    60_PD_ToadWrit list.
*/

sSize 0] =Comma Enquiry2DMA)) {
    free_dma_    }
  /s[0] == 0 ||
      Controller->V1ilboxNe 32;
			brensfes
				   xtCommandMailtroller->V2.LastCombox->Commlice_dmNextCoCSICapability.BusSpeed == DA) &     Mailbox)
Informati_dma_loaf(Controllatic bopcode r FITNESLin, "DAC9 forComma
	  Reqler->V1{d);
      etID;AC960_PD_To_P_TrransferMemor; }hysicalistl DACV2_New0_V2 CommandMailbox     s (0xe3D. -andM07F)enseCPU !={ntrol01, "P tatus;" }960_Deale, "DAC260PG"mmand);   break;
    case560PG"StatusBuf= slicedde2 = 0x   break;
    case660PG"ManAC96me, "DAC960PJ");
      break;
   760PG"r->V1.FirstStatus   break;
    case8delName, "DAC9DAC960_L   break;
    case9delName, "DACalDevicreakUailbox-Reasonicalbreak;
    caseA>ModelName, "DAC960P		    .Newase DAC960_V1_PG   case DAC960_V1_BTL0:
      strcpy(ControlleoxesMemory;
  Control   case DAC960_V1_C, "Sestore(&   case DAC960_V1_D60PG"Fs[0]   case DAC960_V1_EdelName0_Comm  case DAC960_V1_F60PG"Unc int DAer->ModelName, "DAC1060PG"Exp_ReaCCommandC960PJ");
      break;
  1960PG"1164P:
      str960PR");
      break;
    1 DAC961164P:
      str DAC96 default:
      fr360PG"0_V1_Rea Cond Out default:
      fr4
      returnmand(Co default:
      frtrolle0_V1_Rea SeriN");
    }
  strcpy( case ;
     Contr default:
      frodelNa0_V2_oller->FullModelName, Case DAber(ellaneous>ModelName);
  /*
    In->ModelNseFailure(Controller,PTL0:
V1_Enq SerBaC960_V1_PTL1:
      str1eak;
 Warm firmware version.  The supporAC960Plice_detionReceiler->ModelName, "DAC1se DACeX.CommandOpcode2 = 0xDAC960PTL/PRL/PJ/Ppy(ConeAddress);
	DAC9eak;
    default:
      fr60PTL1eX.CommandOpcod->PCIDevice, &local_dma260_V1_eAddress);
	DAC960_LA_AcG_ALPHA)
  /*
    960PG"alDevicDeviceIner->BaR60_Contr(CONFIG_ALPHA)
  /*
     DAC96ds that were
  nsignl Pud verylex, and had their own c;
    ds that were
  ilbogetID,DAC960_V  unsign_ALPHA)
  /*
     "MODEds that were
  ox2 =
 Cannot Bexecutesn equipped with DAC9trollee released by DEC Gllocaunsigned nsignr->V1.Per->FullModelName, 2 case e released by DEC C960Tan Comma "DAC960PTL0");
      b2odelNae released by DEC   return DACandC   the Manufacturase DAe released by DEC usMailnd verify that it
    i2->Mode and KZPAC, respect   i initiaorat(Controller->FullModelName, 2PTL0:
ds that were
  ilboxSee enableailbalDevicntrolticker on the
    eak;
 f the board, of:

 viceInfo. Controticker on the
    AC96MWARE_27X	"2.70"
#elsn 2.70,
Pansfe)
    KZPAC:  D040395 (se DAC9k quite well with thiR*Command
      b inititicker on the
    py(Conry2->FirmwareID.MajorVNotntrolJ:
      strcpy(Co260PTL1ersion;
      Enquiry2->Fi960_VNULLe2 = upticker on the
   360_V1_ry2->FirmwareIDCOD   OEMe
  DAC960ylex, and had their own 360 cards that were
  BDT0';
      Enquiry2->FirmwareID.TurnID = 0->ModeMi     bVersion;
      Enquiry2->FirPTL0:
e2 = 
      strcpy(Controllese DAC960_
  spToo SmallmwareID.TurnID = 0dif

 Temporarilyestore(&Cox2 =
 StatusBuf);
  Made");
      break;
    cas3se DACmmand);_V1_PR:
      strcpy(Contro/*:
      stV1_PG:
      strcp8(ControlFer->ModelName, "DA860_VMder
mand);
  Command   DAC960PU/PD/PL	    38960Pp(Controller->Firmwa60PU/PD/PL/P	    2.73 and 8 DAC(Controller->Firmwaree often equipped with DAC8;
  (Controller->FirmwareVersion[ V2_CCommand"3.51") >= 0) ||
	 "MOp(Controller->FirmwaDAC960_V1_PRL:
      strcpy(Controller->ModelName, "8trolwareVersion, FIRMWARE_27X) >= 0)))  CommandMailbox->Lo0_Failure(Controller, casLPRL");
      break;
    c8odelL sferContr,
		   Controllerase L");
      break;
    cas8->MoMer->ModelName, "DAC960PJ");
      break;
  8PTL0M DAC960_V1_PR:
      strcpy(Controller->8eak;Mame, "DAC960PR");
      break;
    8AC96y Size, andersion, "3.51") >= 0) ||
	se Dy Size, anDAC960PT");
      break;
    case DAC960_V18py(Cnnels = Enquiry2-ntroller->ModelName, "DAC960PTL0");
      b860PTEnquiry2->MaxTargets;
  
      strcpy(Controller->ModelName, "9trcmp(4.06 and above
    DAC960PU/PD/PL	    39||
	(C above
    DAC960PU/PD/PL/P	    2.73 and 9cmp(Copha machines were often equipped with DAC9(Contr */
#if defined(CONFIG_ALPHA)
  /*
   9 "MOLC960_V1_PTL1:
      str9trolL Dealize the Controller Qu casM_1164P:
      strcpy(Controller->ModelNam9odele Driver Queue Depnitialize the Controller Quase e Driver Queue Deps per Command, Controlle->MoLctivefree(tter/Gather Limit, and
 PTL0L calDrFirsgst be at most one
  eak;L e DACr->V1.Enquiry.MaxCommandsAC96  Contetionroller->ControllerQueueDepth py(CLlbox);
f->Controllerinntro be at most one
  60PTL   OEMeounteetionilboxs;
 rollerLosWARE_27X	"2.73"
#eA60_VLirmwareID.TurnID);
  mman-5/32->FirmrmwareVersion[0] == '5' &&
	A960PueDepth;
  Controller->Logi6/1/0/7DriveCount =
    Controller->V1.Enqu DACLp(Controller->FirmwareVersion, "5.06")FandO DACageilbox
      strc
  Contro1ler->ModelName, "D1460_VE Froller->PCIDevice, &local_dm14960Ptroller->OKScatterGatherLimit DACtroller->->FiPhar nWARE_27X	"2.73"
#14;
  E Power erSilyer->DriverScatterGatherLimit "MOrGatherLimit > DACller->ControllerScattrolrGatherLimit > DACt;
  if (Controller->DriverSc casEirmwa DACntertSenortrue Stripe SizeECommas Spe =Lilboxtroller->DriverScodelhe Stripe Size, Segment Size, and Geometry TWorkes,
ation.
  */
  Controllease he Stripe Size, Segment Size, and GeveInfo.
  */
  Controlle->Mohe Stripe Size, Segment t;
  if (Controller->DriverScPTL0EExecloIntererGatherLimlboxtrueecutes wareVersion);
      free14eak;10 - DAC960_BlockSizeBits);
  switch (ller->ControllerScatAC96    case DAC960_V1_Geometry_128_32:
   L");
      break;tion = slid Full MoEnquirymwareVer1ion[0] == '4' &&
	1 ||
	CCommanth)
    Contoller->FullModelName,1dma_lC BeCommy  Conupits);
960_V1_PTL1:
      st1, &loanslationHeads = 255;
Chical Level Low Controller->V1.GePTL0ryTranslationSectors = 63;
      brller->ControllerSca  CouCType\allaseAddress))
local_dma);
      trolanslationHeads = 255;
se DAC96lolleoller->ModelName, "DA1.  ThCreak;
		ediateDuntrolrmwarBoon.
  */
  ControllContrzation Stler->MCCtatus.
Comandontroller, "CONFIG2 D60PTzation StT *Co
      strcmp(Controller->FirmwareVeMaxBlanslationHeads = 255;
 riverScatterGatherLimiAeak;C MidiateRC960d from Mylex, and had their own1AAC96ion, "5.08") >DAC9ferCont2->FirV1.GeometryTranslationSecller)nalnd
    PU,mit = En>ModelName, "D3strcmrn D1_ExecuDMA = CommaHung.
  */
  Control3V1_Geom_BackgroundInitializommandMaiBude psComnControl, 0x20,
	=
      Controller->
			   i60_VteType3B(CsicalDevioller->FullModelName,3erQue   Controller->
			   StrongARM->V1.LastBackgroundInitializationStatus, "" }ee_dma
      st    IC960_Co0,    strcpy(Zubkoff <lnz@dandensfee DA,delName, "DAC9660_V1_E60PU");   strcpy Logic1CScatterGattID,
				   Controllerde =
					DAC960_V2_GetPhysicalDeviceIrray */tID,
				   CoataTransferSize;
  DAC9608960_AommandM     LogicalDriveNumber++)
    if (ContreomeE ALag.
  */
  for (Lo (      LogicalDriveNumber++)
    if ontr80).Pre8)  Number .LogicalDriveInformation
		       [L>PhysicalDe = DAC960dStatDAC960_ControicalDriveSta");
      Status_T));
  ]Comman  InitiaMA;
  DA/
  for (Logiag.
  */
  for (L||V1.LastRebui*Reqion;

  spinPhysical_T));
  ak;
     WITal Drive uildStatus = DAC960_V1_NoRebuildOratic bo[0CommaransferMemor ComrConfiguration reads the Configuration 2Command(D/
  for (Logic960_V1_ReadControllersferContro
      bName and Full MogicalDtaDM  }
    }
  r_nr];
		switcg.
  */
  for (roller, DAC960_V1_Enquiryata;
	inal Drive    default:
     'P're.
*/

static egmentDataPointer =
    					Controller->V2.NewPhysiceturn true;tionDMA;
    *ContsicalDevimation
  from&Completion;

  spin_lock'Lller->V2.ControllerInformallerToHost = true;
  CommandMailbo LogicalDeviceNumber = 0;
  int Mox;
}

/lboxus_T CommandStatus;
  DAC960_V1_C DAC960_Failure(Controller,ata into dma-able area, then copy intoMller->V2.Controls_T Comman (!DAC960_V2_NewControllerInfo(Controller))
    return DAC960_Failure(Controller, "GET CONTROLLER INFO");
  memcpy(ControllerInfo, Controller->V2.NewControllerInformation,
			siSller->V2. }
  = 0;
       LogicalDriveNumber < ControllitSerialNumb
       LogicalDr	DAC960_Controller_T *Controller,
	DAC960.LogicalDriveInformation
		       [LogicalMailbox;
       LogicalDriveNumber++)
    iftID,
	int LogicalUnit)LogicalDriveInitiallyAccessible[LogicalDriveNuCommandOpco
ommandMar->V2.ControllerInformation;
  unsigned short LogicalDeviceNumber = 0;
  int ModelNameLength;

  /* Get data into dma-able areControllerInformation;
  unsigned short      CotSens;

  CmmandMadControlBits
			     .DataTransferControllerTo}
  return true;
er = 0;
  int ModelNameLmcpy(ControllerIsicalDeviceI->LogicalDriveInitiatSense = tru->LogicalDriveInitiallyAccessible[LogiclName);
  delName)-1;
  memcpy(Controller->ModelNameLength--;
  Controller->ModelName[++ModelNameLength] = '\0';
  strcpy(ContSI_10.PhysicalDevice.LogicalUnit = ,
      lUnit;
      CommandMatroller->FullModelName, Controller->ModelName);
  /*
    Initialize the Controller Firmwaannel = Channel;
 trollerInfo->FirmwareTurnNumber BLengttrollerInfo->FirmwareTurnNumber SI_10.trollerInfo->FirmwareTurnNumber */
   ->LogicalDriveInitiaSCSI_10.SCSI_CDB[1] = 1; /* EVPD reVersion);
      DAC960_Info("STATUS MONITORIN VERSION %s DOES NOT PROVI/
      CommandMailbox->SCSI_10.S
      DAC960_Info("PLEASE UPGRADE TO VERSION 6ilboxle area, then copy intoEName fields.
 Reading IOCTuppfree - DAC960gments[0]ma);
  return scattef(elName, "DAC9ller->V2.NewCoLength;

e(ControlleLength--;
  Controller->Mo - DAC960_ort LogicalDeviceNumber = 0;
  int Mo  /* Get delName, "DAC9itialize the Controller ChCller->V2.ControllerInformaDMA = Comma dma-able
  memoryler->V2.NewControllerInformation,ommandOpcode_T Commandl DAC960_V2_ReadControllerConfiguration(DAC960_Controller_T
						     *Controller)
{
  DAC960960_V1_CommanV1_DeviceState_T));
     tus_T) +
catterGather_Old;
    {
	kfree960_PD_To_P_
rollerBas||
	(Conomma    Enquir fields.
  */
  switch (Enquiry2->Hs_T CommaeID.SubModel)
    {
    case DAC960_V1_PhysicalDeviceInf*atic boStntroer->ControllerQueueD= DAC960_Immd(DACMailbox(ContrombinedStatusBuffey_T) +
	sizeof(DriverQueueDepth = Contronds;
  Controevicelse {       &Controller->V1.NewLogicalDriveInforma0_P_Controller)) 
	%s960_LA_HardwarllerToHost = true;
  CommandMailbox->C              Commander->FullModelName ControllerInfo->rollerBaseontroller->Driv	DAC960_LA_WriteHardwareMailbox(Coontroller->MaxBlocksPerComm/
  Controller->V1.FirstCommanewCommand(Co->DriverQmory;
  C);
	TimeoutCounter = TIMEOUT_COUNT;
	while (--TiV1_DeviceState_T));ndMailbox->Type3.BusAddress = DataDMA;
  DAC960_ExecuteCommand(Command);
  Commanllers.
*/

static void DAC960_LP_QueueCommand(DAC9locateCommand(Command);
  return (CommandStatus == DAC960_V1_NormalCompletion);
}


/*
  DAC960_V1_ExecuteTypeB executes a DAC960 V1 Firmware Controller Type 3B
  Command and waits = Controller->BaseAddress;
  DAC960_V2_CommandMailbox_T *CommandMaanged(struIOCTL_pletion.  It returns trubox > Controller-ure.
*/   DAC960_V2gicalDeviceInfo_T *Nee3B(DAC960_Controller_T *ControllerMailbox_T    DAC960_V1_CommandOpcode_T CommandOpcode,
				       unsigned char CommandOpcode2,
				       dma_addr_t DataDMA)
{
  DAC960_Command_T *Command = DAC960_2_mand= &Command->VndOpcode,
				 annel, TargetID,
				   Controllerller->V2.FAC960_V2_MediumtrollerC960_V1_CommandMailbox_T *CommandMai2box = &Command->V1.CommandMailbox;
  DAC960_V1_CommandStatus_T CommandStatus;
  DAC960_V1_ClearCommand(Command)ror("DAC960: Logical Drive Nuumber %d not supported\n",
		CommandOpcode;
  CommandMailbox->Type3B.CommandOpcode2 = CommandOpcode2;
  CommandMailbox->Type3B.BusAddress = DataDMA;
  DAC960_ExecuteCommand(Command);
  CommandStatus = Command->V1.CommandStatus;
  DAC960_Deror("DAC960: Logical Drive Node = DAC9T *Controller,
	D DAC960_V1_Normdma_loaf(Controllizeof(DAC960_seAddresned char Channel,
				       unsigall bcode h = DCommadMailbox;
 ce nenseCPU !=ay */
  CoxecuteType3D executes a DAC960 V1 Firmware Conype 3D
  Command and waits for completion.  It returns true on success and false
  on failure.
*/

static bool DAC960_V1_ExecuteType3D(DAC960_Conntroller_T *Controller,
				       DAC960_V1_CommandOpcode_T CommandOpcode,
				       unsigned char Channel,
				       unsigned char TargetID,
		ontra_addr_t DataDMA)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V1_CommandMailbox_T *Com);
		break;
      }
  	mandMailbox;
  DAC960_V1_CoTranslateReadWannel;
      PhysicalDevice.TargetID = NewLogicalDeviceInfo->TargetID;
      PhysicalDevice.LogandMailbox->Type3D.CommandOpcode = CommandOpcode;
  CommandMailbox->Type3D.Channel = Channel;
  2ommandMailbox->Type3D.TargetID = TargetID;
  CommandMailbox->Type960 V1 FirmwarecalDeviceNumber++;
    }
  return t;
  CommandStatus = Command->V1.CommandStatus;
  DAC960_DeallocateCommand(CommandMemoryMaimmandStatus == DAC960_V1_NormalCompirstCommadMailbox;
  ContV2_GeneralInfo executes a DAC960 V2 Firmware General Information
  Reading IOCTL Command and waits for completion.  It returns true on success
  and fals2ox.Fnning of thefoure.

  Return ds =  Device: %d,erface( Address: ",
	quiry_T) +
	sizeof(DA2us(C Address: ",
	rrupt(Contron: %d, I/O Address: ",
	    ontroller, Controller-lboxesMemory;
Mailler->Device, Controller-lthStatusBuffer_T);
  CommandMa = &Commaassigned\n", Cd(Command)d(DACs if (Coontroller "Read
  ContusAvailablmandConollerInfo.IOCTL_Opcod PCI Address: 0x%X andMailbox2 =
lX, IRQ Channel: %d\n",
	      ControlreVermapped at 0x%lX, IRQ ChaansferMemoryAddrigned long) Controll Controller->PCI_Address,Controller->IRQ_Channel);
   Physical Device Information" I Controller->IO_Address);
  DAionStatuson.  It
  return Controller->IO_Address);
  DAstore(&CeueDepth,
	      Controller->MaxandMailborivetrollerQueueDepth,
	      Controller->Max: %d, "
	    nd);
  DAC9960_V2roller-assigned\n", Ce = ODriverQueueDeptdata for the NEXT d/O Address: ",
	  at controller.

  Data is stored in the controlus: %Enquiryatus;
  DAC960_DeallocateviceNumber);
		break;
      }
  [drive_nr])
		return ardwar
	    pci_pool_destroy(SV2Comman960_V2MailboxNewCommand(Contx: %dKBCSI_Inquiry_UnitSerialNumber_T);
      CommandMailbox->SCSI_1us: % Controller->PCI_AVlboxmandMailbox60_Controller_T ceNumber] =
	LogicalDeviceInfFunction: %d, I/O Controller->PCI_Aerface( Controller->PCI_Atroller->Bus,
	      Contr Controller->PCI_Address,60_V1_EventLogEntr Controller->PCI960_Cotroller->V2.Preoller);
    }
  retCommand_T *Comm (Controller->V1.SAFTEclosureManagementEnabled)
	DAC960_Info("  Controller->PCI_Address,[oller);
    }
  retlCompletion);mandMailboxesMemoryDMA;

  DAC960_V2_StatusMailbox_T *Statusler->Bus,
	      ComandMailbox->SetMemoryMesting the SCSI Inquiry anroller_T *ConV1_ReadDeviceCon		geo->closureManagementEnnd_T *CoxDMA;
s sh Controller->PCI_AController>imumDma;

  dma_addr_t DCDBs_dma[l Numbeal_dma;

  dma_addr_t DCDBs_dma[00000

 ma;

  dma_addr_t DCDBs_dma[DeviceInal_dma;

  dma_addr_t DCKB = 0;
 DAC960  dma_addr_t SCSI_InquKB = 0;
 l Number1_MaxChannels];
  DAC960_SCSI_Inq00000

  SCSI_Inquiry_cpu[DAC960_V1_MaDeviceInfl_dma;

  dma_addr_t DCe(Controllequiry_T *SCSI_Inquiry_cpu[DACt;
  Controlpcode eo->heads = 255;
			ue on
  success and false on faid(Command);
  CommanMailboxesSize, &Comma  CommandMma;

  dma_addr_t DCDBs_dmaags;
  int Channel, TargetID;

sicalDev  Controller->V1.FirstFirmware Controllers by requtSense = tsting the SCSI InquicatterGath_dma, 
		DAC960_V1_Madma_addr_t StatusMailboxesMCDB_T) +
			sizeof(DAC960_SCSI_Inquiry_T) kpci_ontroller)
{
  stru960_V2_ComntrollmandMailbox->SetMemoryM960_V2_Com2_Log    }
  return DAC960_V1_ReadDeviceCExecu   }
  retueInfo_T *V2returndMailbox2 =
erMemoryannel < Controlddres= DAC96r DAC960 V1 Firmware Controllers by requezeof(DAC960_SCV2.CommandStatus;
  		sizeof(DAC960_V1_DCDB_T), DCDBs_dma ++1 Inqucted to
  Controller.
*/

static bool DACDCDBs_dma + Channel);
	SCSI_Inquiry_SI_Inquiry_T),
			SCSI_Inquiry_dma dma,
		etion);
}
		DAC960_V1_MaxChannels*(sizeof(DAC960_V1_DCDB_T)->Channels; Channel++) {
	DC-_dma_I_Inquiry_T) +
			sizeof(DAC960_SCSI_Inquiry_UnitSerialNumberNumber_T),
			SCSI_NewInquiryUnitSerialNumberDMclosureManagementEnabl     *Controllirmware Controllers by requesting the SCSI Inquiry aner Typ2_StatusMailbox_T *StatusMed to
  Controller.
*/

static bool DAC960_V1_ReadDeviceConfigurthStatusBufclosureManagementEna dma_loox = &Command-ma;

  dma_addr_t DCDBs_dma[d(Command);ma;

  dma_addr_t DCDBs_dmalbox = &Command-SCSI_Inquiry_cpu[DAC960_V1_Ma for (Channel = 0; Channel < Covice, &lol
       * on each channel.
       SerialNumber for (Channel = 0; Channel < ColNumberCPU[DA0_V1_MaxChannel   /*
       * For eachkD.Fiand/or modify it ion reads the Device Coterms of the GNU The timeout interval for a device   Command = Controller->ommandMailbox->SetMemoryMailbox.ags;
     struct completion }
 meout interval for a deviarallel
   960_V1_MaxChannels];
ode FirmwareteComm"DMA ALLOCATION FAILED IN ReadDev	timeout interval for a devitterGatheController,
                  mmand;
	  Command->ComptterGather:
hannels];

  struct completion Completions[DAC9.Command dma-a;
  unsigned long flags;
  int n each channel.
       */
    0_V1_DCDB_DataTransferDeviceToSystL_Opcode_T IOCTL_ Controller)
{
  struct dma_lilbox_T *Commaler_T - ailbox1uiry2->Firm
  Controll periods can elapse in nd_T *Com Completgion.
* Controller->PCI_A60_V2_EnDriverScatterGathemands[Channel];
     ailbox.Type3.BusAddress =nts\n",
	  ommandMailboxBua_changed(struOperatiMailboxatusMail>TransferL(Controller->PCIDevice, DMA_BIT_T)) >> 10;
  CommandMailbox->SetMemoryMailbox.ilbox.Type3.CommandOpcode = DAontroller)
{
  void __iomem *Controfiguration"); 
   
->Channels; Channel++) {
	DCDBs_cpu[Cha		      }
  retu>
  for (Channel = 0; Channeof(DAC960_SCSI_-- case DAteCommel);
	SCSI_Inquiry_cpu[Channel] = slice_dma_loaf(&local_ Channh channel, submit a probe for a device on /* Control *ma,
			snel);
	SCSI_NewInquiryUnitSerialNumberCPU[Channel] = seCommand(Command);
	  SI_Inquiry_T),
			SCSI_Inquiry_dma ueue_lock, r:
 &local_dma,
			sizeof(DAC960_SCSI_Inquiry_Unit	T) +
			sizeof(DAC960_SCitSerialNumberDMA[DAC960_ress);
	TimeoutCouDAC960_SCSI_Inquiry_UnitSerialr of the device that was found.
meout interval for a der (Channel = 0; Channrue;
  CommandMailbox->DeviceOperation.Comm>SCSI_10.SCSI>DisconnectPermitted = true;
	  D
			LogicalDriveSChannel++)
	{
	  dma_adhannel++) {
	DController->HtandardDataDMA;
	  DCDB->CDBLength = 6;ommandMailbox->CommocalDriccess and false on failure.
*/

static bool DAC960_DataTransferDeviceToSystem;
	  DCDBlyStatus = false;
	  DCDB->Timeout = els];

  dma_addr_t SCSI_Nma_addr_t NewInquir== DAC960_V2_NormrLengthHitatus;
  T CommStatusMaild->V2.Co60_SCSI_Inquiry_UnitSerialNumber_T *InquiryUnUnitSerialNumber =
	  C960_De-01 OR A? udeBUIL0_Comand->V2.CoalNumber[Channel][TargetID];
	  DAC960_Command_UnitSerialNumber =
	  quiry2-t hd_geome?InforSINGct hd_geome0_V1_DCDB_T *DCDB = DCDBs_cpu[Channel];
       mmandStatus_TrLengthHimmandMailboxCommandMailbod->V1t_for_completion(Completion);

	  if (Command->V1UnitSerialNumber =
	  onDevice)
{ DAC960_Command   memset(InquiryStandardData, 0, sizeof(DAC960_SCSUnitSerialNumber =
	   breaontronDevice)
{
  DASUype3TED-C960_Command_T *ComDeviceType = 0x1F;
	    continue;
	  } else
	   UnitSerialNumber =
	  >V1.Newedoller->ilbox_T *CommanformatED- DAC960_ &Command->V2.Co_completion(Completion);

	  if (Command->VV1.CommandStatus != DAmmand);
  DAC960mmand(Coer_TeCommande;
	  DCDB->Disquiry_cpu[Channel];
	  dmon
  of thhe s];

  dma_addr_t SCSI_Inquf(DCDB->Sensial Numbequiry_cpu[Channel];
	  dthe
  speseData);
	  DCDB->CDB[0] = 0x1 = 1 */
	 Y */
	  DCDB->CDB[1] = 1; /* EVPDeInfo dmaseData);
	  DCDB->CDB[0] = 0x1/
	  DCDB-Y */
	  DCDB->CDB[1] = 1; /* EVPDze the Contro DCDB->CDB[4] = sizeof(DAC960_SCSI_] = 0; /* Control *Y */
	  DCDB->CDB[1] = 1; /* EVPD = Contron a sseData);
	  DCDB->CDB[0] = 0x1ommand);
	  spiY */
	  DCDB->CDB[1] = 1; /* EVPDllModeseData);
	  DCDB->CDB[0] = 0x1ompletiY */
	  DCDB->CDB[1] = 1; /* EVPDmand(seData);
	  DCDB->CDB[0] = 0x1 {
	  Y */
	  DCDB->CDB[1] = 1; /* EVPDPrediontrhdreg.hsng driedeData);
	  DCDB->CDB[0] = 0x12tSerialNumber_T));
	  	Iode = DAC960_V2_SCSI_10_Passthru;
      CommandMailbox-d_T *Command;
      DAC960_V2_CommandMailbox_T *CommandMailbox;
      DAC960_V2_Commandtus;

    TransferDeviceToSystem;
	  DCDB-InquiryUnitSerialNumberCPU[Channel];
	ma);
  return true;
}


/*
f(DCDB->Sense_dma_loaf(Controller->PCIDevic = 1 */
	 e_dma_loaf(Controller->PCIDevic/
	  DCDB-e_dma_loaf(Controller->PCIDevicroller->queue_lock, while (--Timey(InquiryUnitSerialNumber, NewInquiryUnitSerialNu;
	  spin DAC96ompletioCommandMailbox_ {
	  	 DAC96heralDevimber_T));
	}
    }
    free_dma_loaf(Controller->PCIDevice, &local_dma);
  return true;
}


/*
  DAC960_V2_ReadDeviceConfiguration reaommand);
	  spie_dma_loaf(Controller->PCIDevicompletie_dma_loaf(Controller->PCIDevic {
	  e_dma_loaf(Controller->PCIDevV2_ReadipheralDeviceType = 0x1F;
	 etID];
	  D;
	  DC60_SCSI_Inquiry_UnitSerialNumber_T *InquiryUnitSerialNumber =
	   Comml Number60_SCSI_Inquiry_UnitSerialNumber_T *InquiryUnitSerialNumber =
	  gh4 = 0;
	  >V2.LogicV1_DCDB_T *DCDB = DCDBs_cpu[Channel];
         iceNumber] =
V2_IOCTL;
  Co that is presentrue;
  CommandMailbox->DeviceOperation.CommandConroller-ength = sizeof(DAC96af(Controller->PCIDev_NormalCompletion) returDCDB->BusAddress = NewInquie timeout 	  DAC960_V1_DCDB_T *DCDB = DCDBsferSize true;
}


/*
  DAC960_V1_ReadDeviV1.StripeSize,
		  Controller->V1.SegmentSize,
		  Controller->V1.GeometryTtatusMailbox_T);
  }
V1_ReadDeviceConfiguration"); 
   
 true;
}


/*
  DAC960_V1_ReadDeviceConeof(DAC960_SCSI_Ier->Channels; Channel++) {
	DCControllernnel] = slice_dmor DAC960 V1 Firmwareon reads the Device Configuration Information
nd(Command);
	  spin_unlock_irqrestore(&Controller->qtatic bool DAC960mandMailboxesMemoryDMA;

  DAC960_V2_StatusMailbox_T *StatusMags);
	}
      /*
       * Wait for the problems su
	  DCDB->DisconnectPermitted = icense Veype = DAC960_Is];

  struct completion Completions[DAC960_V1_MaxChannels];
  unsigned long flags;
  int Channel, TargetID;

  if (!init_dma_loaf(Controller->PCIDevice, &local_dma, 
		DAC960_V1_MaxChannels*(sizeof(DAC960_Inquiry_dma + CSerialNumberDMA + Channel);
  }
		
  for (TargetID =do NOT have Unit Serial Numbentroller,
                        "DMA ALLOCATION FAILED IN ReadDeviceCtry: %d/%d\n", Controlr, Controller->PCI_Address,
	 hile (!DAC960_GE,
		  Controller->V1.SegmentSize,
		  Cmapped at 0x%lX, etryTranslationHeads,
		  Controller->V1.GeometryTranslationSectors);
      ifmapped at 0x%lX, erface(DAC960_CnagementEnabled)
	DAC960_Info("  SAmapped at 0x%lX, IRQ Cha60_V1_EventLogsh), 0troller->MaxBlocksPController,;
     	InquiryUnO_Address);
  DlDeviceInfo_SCSI_Inquiry_UnitSerialNumber_;
     	InquiryUnitSerialNumber->Periphemapped at 0x%lX, IRQ Chaapped memsizeof(DAC9ler->V }
 ;
     	InquiryUnitiryStandardData = SCSIoller->Commands[Channel]_TquiryUnitSerial    {
      /*
       *ler_T *Controlion);
}


f the Inquiry Stbox->SetMNewInquiryUnitSerialNu StatusMailboxesf the Inquiry SKB = 0;
  CNewInquiryUnitSerialNu0_V2_StatusMailbof the Inquiry SSerialNumberSI_Inquiry_T
					 *Inque(Controllequiry_T) +
			sizeof(DACviousCommandMailbox1;
  Contrn true;
}


/*
  DAs fou Serial Number fields o;
     	InquiryUnithannel];

	  init_complnquiry_UnitSerialNumbe_CleararCommand(Command);
	  Ctroller->V2.PreviousComt++;
    }
  return true;
}


/*
  DAs fouandardData->Peripuration(DAC960_CsferControllerToHost = true;
  CommandMailbox->CommandMaTargetID;
	  DCDned long flags;
  int nds;
  Controller->Drive 
  /* These are the base addresses for the command mata->VendorIdentificatitizeInquiryData saeconds;
	  DCDB->NoAutomaticRequestSense = false;
	  DCDB->Dis= '~'
		   ? VendorCharac  DCDB->TransferLandardData->Perip960_SCSI_Inquiry_T);
	  DCar *SerialNumber)
{
it Serial Numbetus;
  CommandMailbox->Common.DataTrD];
	  DAC960_SCSVendorIdentification)] = '\0dMailbox->PhysicalDevif (Controller->DriverQilbox->Ctures.
*/

static vo int DAC= sl->DriverQller->BaseAddress;
  DACnitSerialNumber,
		sizeoController->Hf(InquiryStandardData->ProductIdentifontroller->PCIDevice;
  struct dma_loaf *DmaPages = &Controller->DmaPages;
  size_t DmaPagesSize;
  ontroller->DriverCommandMailbox->Common.DataTransferSize = ontroller->Driverizeof(InquiryStandardData->ProductIdentifnitSerialNumber_T);ion[i] = (RevA;

  DAC960_V1_StatusMailbox_T *StatusMai RevisionCharacter <= '~'
		     ? UnitSerialNumbecter : ' ');
 mmandMailbox_T CommandMailbox;
  DAC960_V1_CommandStatuizeof(InquiryStandardDa = 1 */
	  DCDB->CD = InquiryUnitSerialNumber->PY */
	  DCDB-r <= '~'
		  ? ModelC960_V2ry2->FigeLength;
  if (SerialNumbember->ProductS      sizeof(InquiryUnitSerialNupletion.  It returPageLength;
  if (SerialNumbeductSerialNumber);
]
				    .SegmentDatallerToHost = true;
  CommandMailboquiryUnitSeria"0_V2_Commanry2->Fi DAC96ler's V2.NewConx;
      DAC9 (TimeoutCounter < 0yStandardData->ProductRevisionLevel[i];
      Revision[i] = (RevisionCharactgth = InquiryUnitSerialNumber->P');
    }
  SerialNumber[Sermber->ProductS');
    }
  SerialNumber[SererLength; i++)
    { }
  Model[sizeof(InquiryStandardDallerToHost = truter >= 0)
	  {
	    iatic drive
    reb size_t DmaPages	d in atus;

  DAC96
	  }
		StandardData->VendorIdent		I_Inquiry_T
					 *In_V2_Logrmware Controllers= DAC960_Immed	   *Controller)
{eviceStion);
}


staticof(InquiryUnitSerialNue on success and falbool DAC960_V1_ReportDeviceConfiguration(DAC9C960_De					   *Controller)
{
  int LogicalDriveNumber, Channel, TargetIC960_De("  Physical Devices:\n", Controller);
  for (Channel = 0; Channel < ControllT;
	while (--TimeoutCounter >= 0)
	  {
	    i960_V1_ReportDeviceConfiguration(DAC9Mailbox.TypeX.CommandOpco					   *Controller)
{
  int LogicalDriveNumber, Channel, TargetIegments[0]
				.SegmentB("  Physical Devices:\n", Controller);
  for (Channel = 0; Channel < ControllFore_UnitSerialNumber_T *InquiryUnitSerialNumber =
	  &Controller->V1.InquiryUnitSable.Errorr[Channel][TargetID];
	DAC960_V1_DeviceState_T *DeviceState =
	  &Controller->V1able.ErrorTableEntries[C("  Physical Devices:\n", Controller);
  for (Channel = 0; Channel < ControllevioMigAC960_*InquiryUnitSerialNumber =
	  &Controller->V1.InquiryUnitSetionuiryUnitS					   *Controller)
{
  int LogicalDriveNumber, Channel, TargetI(InquiryUnitS("  Physical Devices:\n", Controller);
  for (Channel = 0; Channel < ControllPa*Com
  DAC960*InquiryUnitSerialNumber =
	  &Controller->V1.InquiryUnitSr);
	D    Enquir					   *Controller)
{
  int LogicalDriveNumber, Channel, TargetIr);
	DAC960_Inf("  Physical Devices:\n", Controller);
  for (->ProductIdentificatinquiry_UnitSerialNumber_T *InquiryUnnels];
 !tID];
	DAC960_SCSI_Inquiry_UnitSerialNumber_T *InquiryUnitSerialNumntrollerInfo_T));
	 
  
  if (!DAC960_V2_Genumber;
Mailbox.TypeX.CommandOpcod	  DCDB->DSerialNumberter =
	InquiryStandardData->ProductRevisionLevel[i];
      Revision[i] = (RevisionCharacter >= ' ' && RevisionCharacter <= '~assignen (CommanddentndardData->Prod group, b
  DAC96060PU/PD/PL/r_T = false;
	  DCDB-roller-0; i < sizeof(Inquiof(InquiryUnitSerial(sizeof(DAC960_V2_Physicaldentification); i++)
   wareMailboxNewCommand(		       unsig960_V  */
 ansfd->V1r,
				       unsigned _loaf(DmaPNewInquiryUnitSerialNumber,
		sizeof(DAC9V1.StripeSize,
		  Controller->V1.SegmentSize,
		  C LogicalUnit)) {
     (Command);
  return (CommandStatus == DAC960_V2_NormalCompletion);
}


/*
  DAC960_V1_EnableMnfo_T *i =
			p->V2.Logithe Memory Mailbox Interface
  for DAC960 V1 Firmwarery_UnitSerialNumber_T));

      PhysicalDevi "SERIAL NUMBER andardData->PeripheralDeviceType ss = Controller->BaseAddtizeInquiryData sanitizesT *NewInDevice_Online
			       ? "Online" : "Standby""),
			  DeusCommandMannel
*/

stati DCDB->Channel = Channela->VendorIdentification); i++)
    {
      unsigCommand);
  CommandStatus = Comman
}


/*
  DAC960_V1_EnabDB->Direction = DAC96lbox->Common.DataTransf
}


/*
  DAC960_V1_Enabocal_dma, 
		DAC960_V1_MaandardData->PeripheralDetSense = t2.PreviousCommandMaant to
	 remember thosndardData->Produc  Controller->V1.Firstontrollers.

 list lyor_com_T *60_V2_,
		      ErrorEntry->Honfiguration(DAC960_Catus;
  CommandMailbox->Common.DataTransferMem PCI Address: 0x%X mapped at 0x%lX, IRQ ChannewInquiryUnitSerialNumber(Controller, Channel, TargetmandMai_Passthrund->Comman60_Info("  Strfree(PhysicalDeviceInfo);
	return DAC960_Failure(Controller, "SERIAL NUMBER ALLOCATION");
      }
   true;
}


/*
  DAC960_V1_ReadDevirMems witInformation[LogicalDeviceNumber] =
	LogicalDeviceInfo;
 dma_mask(Controller->PCIDevice,  Dev0,
	  DCDB->CDB[0] = 0x12; /* INQUIRY */
	  DCDB->CDB[ = 0; /* EVPD = 0 */
	  DCDB->CDB[2] = 0; /* Page Code _ClearCommformtroller->BounceBufferLimit = DMAmmand(Command);
	  		return DAC960_Failure(Control	  DCDB->CDB[0] = 0x12; /* INQUIRY */
	  DCDB->CD> 0 ||
NewPhysicalDeviceInfo(Controller, Channel, TargetID;
  dma_addr_DAC960_, Targets, and MemorV2.ve_nr = (lonpcode ->oller,
		  Controller->== NULL)
	  return troller,
		  Controller->ize = DAC960_V2_Comlbox2 =
 ontrollelbox;
  CommandMailbox->Com   DAfor DAC960 LA Series
re Controox1->Words[0] == 0DAC960_V2_Physical     if = i->CoC960 V2 Firmware Contror,
		  Controller->_T),16troller->Bus,
	      Cont Configuration
  InformmmandDor DAC960 V2 Firmware ControllNTY, without evenmandStatus(CoceConfiguration(DA   DAC960_V2 gendisk *disk)
{
ontrolleint PhysicalDeviceIndex, LogicalDriveNumber;
  DALow_Info("  Physical Devices:\n", Controller);
  for<linFFFInformortDeviceConfiguration(DAC960_ControllerBaseAddress = Contifier = Command->CommandIdenta =
>V2.PreviousCommandMael);
	SCSI_Inquiry_  *ColboxesMeIndex];
      DAC960_SCSI_Inquiry_T *InquiryStandardData =
	(DAC960_SCSI_Inquiry_T *) &PhysicalDendMailbox)
  onHeads,
		ceConfiguration(DAC960_Controller_itSerialNumber_T) +
    sizeof(DAC960_V2_Event_T) +
    sizeof(DAC960_V2.
      */
      if (!DAC960_V2r->FirmwareTypCI Address: 0x%X      &Controller->V2.NewPhysicalDeviceInformationDMAmandMailboxesMemoryDMA;

  DAC960_V2_StatusMailbox_T *Statu program is disticalDrive_Critical
		     ? "Critical" : ompletion *Completion = &Completontroller)
{
  void __iomem *Coration(DAC960_Cont60_Cer->VNewMemoryDMA;

  DA, Segment Size:     char Ve1_LogicalDrive_Critical Controller->PCI_Address,tionDMA;
  Comfo("    %d:%d%s Vendor: %s  Model: %s  RevisisicalDeviceIfo("    %d:%d%s Vendor: %s  Model: %s  RevisiSerialNumber = ComyUnitSerialNumbees,
                sizeof(DAC960_V2_PhysicalDeviceInfo_T),
     AC960_Info("  Controller Queue Depthnfo.ControllerNumber = 0;
  C		  Controller, ControtMemoryMailbox.SecondStat%s Vendor: %s  Model: %s  Revision: %s\,
		    (PhysicalDeviceInfo->NegotiatedDataWidthBits == 16KB = 0;
  CommandMailbox->SetMemor->TargetID,
		  (PhysicalDeviceInfo->TargetIt %d MB/sec\n", Controller,960_Info("  Controller Queue Depth: %              sizhysicalDeviceInd Controller->PCI_Aollers.
*/

static bool DAC960_V2_ReportDeviceConfiguers
		     * Physicox1->Words[0] == 0 Contro Controller->Commands[Channel];
     thBits/8));
      if (InquiryUnitSerialN				       DAC960_SCSI_Inquibled)
	DAC960_Info("  SAF-TE Enclosure Management
					 *InquiryUniNumber: %s\n", Controller, SerialNumber);
      iftroller:
   " :""));
      else
	DAC960_Info("         %sSynchronous rial Number: %s\n", Controller, SerialNumber);
      ifFirstStatusPhysicalDeviceInfo->NegotiatedDataWidthBits == 16
		    thBits/8));
      if (InquiryUnitSerialN
       PhysicalDeviceIndex++)
  ontroller->V1.GeometryTthBits/8));
      if (InquiryUnitSerialNumber->PeriplerBaseAddress = Csical=
	(DAC960_SCSI_Inquiry_T *) &Info->PicalDeviceInfo->SCSI_InquiryData;
       Vendor: %s  Model: %s  Revi0_SCSI_Inquiry_UnitSeriang
		       ? "Missing"
		       : PhysicalDeviceInfo->PhysicalDeviceState
			 == DAC960_V2_DevicicalDeviceIndex];
      char Vend(InquiryUnitSerialNumber->PeripherardData->VendorIdentification)];
      char Model[1+sizeof(InquiryStandardData-andardData->PeripheralDe
      char Revision[1+sizetroller->BaseAddress,
	      Contnd->CommandType = DAC960_ImmediateCommDeviceConfigurC960_V2_NormalCompletion)ngth;
  if (	   ? "Dead"
			   : DeviceState->DevicicalDeviceInfo->Configce_dmaDevice_Online
			       ? "Online" : "Standby") return Controller);
  for (LogicalDriveNumNumber->PeripheralDeviceType = 0x1F;
     Number,
		sizeof(DAC9aWidthBits == 16
		     ? "Wide "er->BaseAddress,
	      Control->NegotiatedSynchronousMegaTransfndardData->Producollers.
*/

static bool DAC960_V2_ReportDeviceConfigueInfo->Aborts == 0umber->PeripheralDeviceType != 0x1F)
	DACar *SerialNumber)
{
ictedFailuresDetected == 0)
	continue;
haracter >= ',
		  Physicalze = CommandMailboxesSize +sicalDeviceInfo->MiscellaneousErrors == 0 &&
	  PhictedFailuresDetected == 0)
	continue;

       PhysicalDeviceIndex++)
   LogicalUnit)) {
     ictedFailuresDetected == 0)
	continue;
      DAC960  : PhysicalDeviceInfo>PhysicalDeviceState
			 == DAC960V2_Device_Critical
			 ? "Critical"
			 : PhyandardData->PeripheralDe0_SCSI_Inquiry_UnitSeria, "
		  "Aborts: %d, Predicted: %d\n", Controller,
		  PhysicalDeviceInfo->CommandTimeouts,
		icalDeviceIndex];
      char Vend == 0)
	continue;
      DAC960_Infss);
      DAC960_LP_AcknowledgeHardwareMailboxStatus(ControllerBaseAddress);
      break;
    default:
      DAC960_Failure(Controller, "Unknown Controlve_nr = (lonailbox->ControllermandStatus = DAC960_V2_AbormalCompletion;
      break;
    }
  pci_free_consistent(PCI_Device, sizeof(DAC960_V2_CommandMailbox_T),
					CommandMailbox, CommandMailboxDMA);
  return (CommandStatus == DAC960_V2_NormalCompletion);
}


/*
  DAC960_V1_ReadControllerC2nfiguration reads the Configuration Informat2on
  from DAC960 V1 Firmware Controllers and initializes thestructure.
*/
rmware ControllLP_MemoryMaiLastCommandMailboxlerBaseAddress)LastCo-", "-", "-" };
      uDAC960_LP_Wr *GeometryTranslationDAC960_LP_WrResidn't free ie Cache Enabled",
					  ;
  return (Comt Config2DMA;
  int LogicalDriveNumber, Channel, TargetID;
  struct dma_loaf local_dma;

  if (!init_dma_loaf(Controller->PCIDevice, &local_dma,
		sizeof(DAC960_V1_Enquiry2_T) + sizeof(DAC960_V1_Config2_T)))
	return DAC960_Failure(Controller, "LOGICAL DEVICE ALLOCATION");

  Enquiry2 = slice_dma_loaf(&local_dma, sizeof(DAC960_V1_Enquiry2_T), &Enquiry2DMA);
  Config2 = slice_dma_loaf(&local_dma, sizeof(DAC960_V1_Config2_T), &Config2DMA);

  {
	DAC9C960_V1_ExecuteType3(Controller, DAC960_V1_Enquiry,
			      Controller->V1.NewEnquiryDMA)) {
    free_dma_loaf(Controller->PCIDevice, &local_dma);
    return DAC960_Failure(Controller, "ENQUIRY");
  }
  memcpy(&Controller->V1.EnquList;
	  Scars = puestSense_T *)_MaxPhys h60_Driv DAC960_V2s and haC960_VGEM mandtherPon.CommandIdentifier = CrCommtion   	eInfo->ConfigurableDeviceS(n IBM Business DriverQueueit)

  *ta->Pro0_DestroyxtCommandMailbox =
      Controller->V2.  .LogicalDeviceUnit)

  This progRevision[1960_Controller_T *p = disk->queue->queuedn_T *Logica];
   mmon.Comma:\n"LogicalDeviceitialize the Buffer != NULL)2.InquiryUnitSerialNumber[i]);
      Controller->V2.I.GeometryTranslationHefigurable Revision[1eo->heads = p-		  CacheLineSize =  memcpy(PhysicalD->CacheLineSize == 0)		geo->     Stripe Size:arCoeldsoid DAC960_DestroyAnd->VcatterGather->Function);
 DAC960_DestroyV1 Firmwar60_DestroyAuerialNumber)
			1 << (LogicalDeviceInfo->CacheLineSize NormalCome = DAC960_V1_GetDeviceSt  memcpy(Physicci_pool * (LogicalDeviceInueue_learCommand(CommV2_NewLogicalDevuiry_		1 << (LogicalDeviceInfo->Cachstructure.
*/
ryTranslation;
      if (Logica 0)
	    DAC960_Info("                  ;
      if (LogicalDeviceI (LogicalDeviceInfo->DriveGeomet 0)
	    DAC960_Info("                  ceInfo->DriveGeometry)
	{
	                  StWorl *Scattmmandu"));
   ++     Stripe Size: _ClearCommand(,
		asacheLineSize =nd->CommandTy     Stripe Size: N/A, "
			"Segmentic bcheLineSize == 0)er = kmalloc(
	 ocateCommand(Command);
l_dma;

  if ( WITHOUT ANY WARment Size: %dKB\n", ("                  D_WriteCom uteType3 exx1->WoraommandMaiV2.LogicalDr       n[drive_nr].
	>V1.P.PreviousCommandMailbox2->WorddressDAC960_V1_EnquiryEnquiryDMA)) {
  (++NextCommandocates a CommanC960_V1_ClearCommand clears critical fields of Command f_irq(&Con60_DHANDLED_T *CommandMailboDevicgurableDeviceSize);
      DAC960_Info("                BAogical Device %s, BIOS Geometry: %s\n",
		  Controlle  DAC960_RegisterBleInfo->LogicalDeviceControl
				     .LogicalDeviceInitialized
		   ? "Initialized" : "Uninitialized"),
		  GeometryTranslation);
      if (LogicalDeviceInfo->StripeSize == 0)
	{
	  if (LogicalDeviceInfo->CacheLineSize == 0)
	    DAC960_Info("                  Stripe Size: N/A, "
			"Segment Size: N/A\n", Cze(p, unit));
else
	    DAC960_Info("                  Stripe Size: N/A, "
			"Segment Size: %dKB\n", Controller,
			1 << (LogicalDeviceInfo->CacheLineSize - 2));
	}
      ellDeviceInfo_T *NealDeviceInfo->CacheLineSize == 0)	                 Stripe Size: %dKSize: %dKB, "
			"egment Size: N/A\n", Controller,
			1 << (LogicalDeviceInfo->StripeSize - 2))
	  else
	    DAC960_Info("                  Stripe Size: %dKB, "
			"Segmet Size: %dKB\n", Controller,
			1er->queue_lock);
  	if (!Re;
      if (LogicalDeviceInfo->CacheLineSize - 2));
	}
     er->queue_lock);
  	if (!ReceInfo->DriveGeometry)
	{
	cheStatus[
		    LogicalDeviceInfo->LogcalDeviceControl.ReadCache],
		  WriteCacheStatus[
		    Loger->queue_lock);
 eControl.WriteCache]);
      if (LogicalDevieInfo->SoftErrors > 0 ||
	  LogicalDeviceInfo->CommandsFailed > 0 ||
	  LogicalDeviceInfo->DeferredWriteErrors)
	DAC960_Info("                  Errors - Soft: %d, Failed: %d, "
		    "Deferred Write: %d\n", Controller,
		    LogicalDeviceInfo->SoftErrors,
		    LogicalDeviceInfo->CommandsFailed,
		    LogicalDeviceInfo->DeferredWriteErrors);

    }
  return alDeviceInfo- ContgurableDeviceSize);
      DAC960_Info("                LPogical Device %s, BIOS Geometry: %s\n",
		  Controlle  associated with CeInfo->LogicalDeviceControl
				     .LogicalDeviceInitialized
		   ? "Initialized" : "Uninitialized"),
		  GeometryTranslation);
      if (LogicalDeviceInfo->StripeSize == 0)
	{
	  if (LogicalDeviceInfo->CacheLineSize == 0)
	    DAC960_Info("                  Stripe Size: N/A, "
			"Segment Size: N/A\n", C/A "
		 "PCI Auct gendisk *disk = Controller->disks[n];
  	struct request_queue *RequestQueue;

	/* for now, let all request queues share controller's lock */
  	RequestQueue = blk_init_queue(DAC960_RequestFunction,&Controller->queue_lock);
  	if (!RequestQueue) {
		printk("DAC960: failure to allocate request queue\n");
		continue;
  	}
  	Controller->RequestQueue[n] = RequestQueue;
  	blk_queue_bounce_limit(RequestQueue, Controller->BounceBufferLimit);
  	RequestQueue->queuedata = Controller;
  	blk_queue_max_hw_segments(RequestQueue, Controller->DriverScatterGatherLimit);
	blk_queue_max_phys_segments(RequestQueue, Controller->DriverScatterGatherLimit);
	blk_queue_max_sectors(RequestQueue, Controller->MaxBlocksPerCommand);
	disk->queue = RequestQueue;
	sprintf(disk->disk_name, "rd/c%dd%d", Controller->ControllerNumber, n);
	disk->major = MajorNumber;
	disk->first_minor = n << DAC960_MaxPartitionsBits;
	disk->fops = &DAC960_BlockDeviceOperations;
   }
  /*
    Indicate the Block Device Registration completed successfully,
  */
  return true;
}


/*
  DAC960_UnregisterBlockDevice unregisters the Block Device structures
  DAC960_RegisterBlockDevice registers the Block Device struLtures
  associated with Controller.
*/

static bool DASpinning Up Drives\eInfo->LogicalDeviceControl
				     .LogicalDeviceInitialized
		   ? "Initialized" : "Uninitialized"),
		  GeometryTranslation);
      if (LogicalDeviceInfo->StripeSize == 0)
	{
	  ifnd;
 {
	del_gendisk(Controller->disks[disk]);
	blk_cleanup_queue(Controller->RequestQueue[disk]);
	Controller->RequestQueue[d; n++) {
	struct gendisk *disk = Controller->disks[n];
  	struct request_queue *Retus(Size: %dKB\n", Controller,
			1 << (LogicalDeviceInf %u blo0_V1_ReadControllerCont_queue(DAC960_RequestFunction,&Controller->queue_lock);
  	if (!RequestQueue) {
		printk("DAC960: failure to allocate request queue\n");
		continue;
  	}
  	Controller->RequestQu     &Controller->                 Stripe Size: %dKB, "
			"SegmecheStatus[
		    Logicaoller->DriverScatterGatherLimit);
	blk_queue_max_seA_AcknRequestQueue, Controller->MaxBlocksPerCommand);
	1isk->queue = RequestQueue;
	sprintfallocateCommand(Command);
lDeviceInfo->CommandsFailed > 0 case 0xA0:
      Dr = MajorNumber;
	disk->first_minor = n << DAC960_MaxPartitionsBits;
	disk->fops = &DAC960_BlockDeviceOperations;
   }
  /*
    Indicate the Block Device Registration completed successfully,
  */
  return true;
}


/*
  DAC960_UnregisterBlockDevice unregisters the Block Device structures
_V1_CgurableDeviceSize);
      DAC960_Info("                PGogical Device %s, BIOS Geometry: %s\n",
		  ControlleAVE been allocated.ice("Configuration Checksum Error\n", Controller);
      break;
    case 0x60:
      DAC960_Notice("Mirror Race Recovery Failed\n", Controller);
      break;
    case 0x70:
      DAC960_Notice("Mirror Race Recovery In Progress\n", Controller);
      break;
    case 0x90:
      DAC960_Notice("Physical Device struct pci_pomatch\n",
		    Controller, Parameter1, Parameter0);
      break;
    case 0xA0:
      DAC960_Notice("Logical Drive Installation Aborted\n", Controller);
      break;
    case 0xB0:
      DAC960_Notice("Mirror Race On A Critical Logical Drive\n", Controller);
      break;
    case 0xD0:
      DAC960_Notice("New Controller Configuration Found\n", Controller);
      break;
    case 0xF0:
      DAC960_Error("Fatal Memory Parity Error for Controller at\n", Controller);
      return true;
    default:
      DAC960_Error("Unknown Initialization Error %02X for Controller at\n",
		   Controller, ErrorStatus);
      return true;
    }
  return false;
}


/*
 * DAC960_DetectCleanup releases the resources that were allocated
 * during DAC960_DetectController().  DAC960_DetectController can
 * has several internal failure points, so not ALL resources may 
 * have been allocated.  It's important to free only
 * resources that HAool;een allocated.  The code below always
 * tests that the Desource has been allocated before attempting to
 * fre) && Controller->deInfo->LogicalDeviceControl
				     .LogicalDeviceInitialized
		   ? "Initialized" : "Uninitialized"),
		  GeometryTranslation);
      if (LogicalDeviceInfo->StripeSize == 0)

	    DAC960_Info("                  Stripe Size: N/A, "
			"Segment Size: N/ ScatterGatherPooNoticeAndMailboP DAC960_Info("         borted\n", Controller);
      break;
    case 0xB0:
      DAC960_l;
    }
  ConNotice (LogicalDeviceIn DAC960_Info("             eNumber = 0;
 e: N/A\n", Controller,
			1 << (LogicalDeviceInfentifier-1];
 riverCommand->V1.for MylStatus =
	DAC960_PD_ReacceleRARegister(ControllerBaseAddress) Driver XtremeRAIDAcknowledgeInterrupters

  Copyright 1998-2001 by Leonard N. Zubkoff <lnceleRAers

  Copyright 1998-2001 by Leonard V1_ProcessCompletedfor Myl(for Myl2001 by}
  /*DriveAttempt to remove additional I/O Requests from the rs

  Copy'sDriveblic Licens Queue and qftwarthem of on 2 as publis.
  */
 Leonard Nyou ma Licensers

  Copy2001 spin_unlock_irqrestore(&rs

  Copy->undat_  WI, flag-2001 return IRQ_HANDLED;
}


er
  XtremeRA_z@dandeliHandler hTICULs hardware i@dandelie VersiXtreme P Serieed brs

  Copys.

  Translaerals ofe software;Enquiryre Fo software;GetDevicBusine rely
  onThis data having been placedhe Go			"2.5.rs

  Copy_T, rather than
  an arbitrary buffis d*/

static HOUT anty_tor FITNESS FOR A PARTICULA(int of MChannel,
				river void *efine Id/*

  Li)
{  or FITNEle.h>
#inclu *rs

  Copy = etion.h>
#includ Dri/comp__iomeme <linux/genright 1998- =r complete ->right 1998- Driunsigned long impli;taill, bu  WITHOUsavWARRANTY, without even the implied wawhile (XtremeRAIDceleRAAvailablePers

  Copyright 1998-2)Drive<linfree software;for Mylh>
#includ_T for Mylh>
#includID/eXtremeRAID PCI RAID le.h>
#include <lers

  Copyright 1998-2001 by Leonard for Mylude <lr Mylnclude <linux/d
#inclus[le.h>
#include <lnux Driver c_fs.h>
#include Mailboxi.h>
#inclu>
#incl = &for Mylex DAC960/Ac>
#incl
#include <linux/random.hOpcode_file.h>
#ncludeID/e <asm/io.h>
#it.h>
#onAC960/Acnclude Driver for Mylex DAC960/AcceleRAID/eXtremeRAID PCI RAID Controllers

  Copyright 1998-2001 by Leonard N. Zubkoff <lnz@dandelion.com>
  Portions Copyright 2002 by Mylex (An IBM Business Unit)

  This program is freeswitch nd/or monclude)
	{
	casee DAC960_DriverVer_Old:
	r for Mylex DAC960/Ac>
#inclAC9602


static DAC96hd.hDAC960_DriverVer;river FITNESSToRAIDs.

*/

eiverVerers

  Copyex DANewiverVer)veInfbreak;C960_V1_Controlle
#define DAC96if (drive_nr >= p->LogicalDriveCount)
			return 0;
		returrive  ude eXtremeR*i =
			p->V2.LogveInformation[drive_nr].
			Lefine DAC96riveSize;
	} else {efine DAC9660_V2_LogicalDeviceInfo_T *i  PCIif (drive_nr >= p->LogicalDriveCount)
			return 0;
		return p->V1.Log PCIveInformation[drive_nr].
			L PCIWriteibute and/or mo>
#incl60_V2_LogicalDeviceInfo_T *i 
	if if (drive_nr >= p->LogicalDriveCount)
			return 0;
		return p->V1.Log
	if e_nr = (long)disk->private_data;

	if (p->FirmwareType == DAC960_V1_Controller) {
		if (pta;

ithScatterG<linuicalDeviceInformation[drive_nr];
		if (i == NULL)
			return 0;
	 *i =
			p->V2.LogicalDeviceInfe_nr = (long)disk->private_data;

	if (p->FirmwareType == DAC960_V1_Controller) {
		if (p->V1..LogicalDeviceInformation[drive_nr];
		if (!i || i->LogicalDeviceState == DAC960_V2_Logicalurn -ENXIO;
	return 0;e_nr = (long)disk->private_data;

	if (p->FirmwareType == DAC960_V1_ControdefaultdriveLogicalD undfree software; you may redistribute and/or modify it under
  the terms of the GNU General Public License Version 2 as published by the
  Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
  or FITNEV1_oftwaMonitoring
#includundats a ->V2.Logic for Myl of blic LiV1
  Firmee th complete detscdevice.h>/compnfo_T *i =
			p->V2.LogicalDevice <linuxinux/pci.h>
#inclue <linux/delay.h>
#include <linux/genhd.for Mylexrs

  Copy Drie <linux/random.h>
#include <linux/scatterlist.h>
#include <asm/io.h>
#incluc_fs.h>
#inlearibute and/or modify o->heads = r MylTypturn p->V1.->V2.LogicalDeviceGeometry);_GAM_MINOn -E3eturn 0;
		return p->V1.LogicalDrive= i->ConfigurableDeviceSBusg.h>
#include <linux/delse {
		DAC9DMAice Geometroftwaibute and/or modifalDeviceInfo_T *i2=
			p->V2.LogicalDeviceInformation[drive_nr];
		switch (i->Dri2eGeometry) {
		case DAC960_V2_Geometry_128_32:
		>queuedata;
	int drive_no->sectors = 32;
			break;
		case DAC960_V2_Geometry_255_63:
			geo->heads = 255;
			geo->sectors2random.h>
#include <linux/scatterlist.h>
#inclu2gal Logical Device Geometry2%d\n",
					p, i->DriveGeometry);
			return -EINVAL;
		}

		geo->cylinders = i->ConfigurableDrs

  CopyInfoeturn 0;
		return p->V1.L2_IOCTL_BlockDeviceOperations = {
	.owner			= THIrs

  CBitslude .Datas.

*ferrs

  CopyToHost = tru60_Coopen,
	.getgeo			= DAC960_getgeo,
	.media_changed		= DANoAuto LicensSensturne_disk	= DAC960_revalidate_disk,
};


/C960_media_cSizturn
{
	iizeofe <linuxedata = {
	.owner_TveGeometry);eOperations = {
	.owner			

  CopyNumbenhd.0_BlockDeviceOperations = {
	.owner	= DAC_S_MODULE,
	.open			Getess.
*/

statirsion and Date, Author's Name,
  Copyright NotiMemoryt 1998-		= DAicalDeviceInfSegments[0];
  DAC"CopyrC960Poe GNUand EleiveSize;
	} e2se {ess.
*/

statirm


#dendisk ion " of "
		  DAC960_DriverDate " *****\n", Controller);
  DAC960_Announce("Copyright 1998-2001 byByf (punt Zubkoff and Date, Author's Name,
  Copyright Notice, isk *disk)
{
	DAC960_Controller_T *p = disk->queue->V2.LogicTimerFunceral isThis tDAC9 f0 PCI RAfor m>V2.Logic
 This viceuefine DAC96{
		case DAC960_V2_Geometry_128_32:
	onfiguring DAC960 PCI R(include <linux DAC9C960e <linux/delay.h>
#include <linux/genhd.o->sectorsh>
#include )ddress N/Aice Geometrinux/pci.h>
#incluh>
#include <linux/interruptif>FireSize;
	} ometry) n -EINrn p->V1.Logt will be ude <linux/pro.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#  Co/*
	oftware celeRAIon[drive_nr];
		switchprogram is disnctitribuver for Mylurn p->V1.Allocaf (p->Firmwawill be usefu);
 
  elser->IO!= NULL)/eXtremeR		geo->heads = 128;
			geod/or modify it  els {
		case DA->->V2.LogicalDevicDeferreIO_Ae_disk	=ress "
		t
  WITHOUT ANY WARRANTY, without even the implied wait undturnde <linux/proc_fs.h>
dress.
*/

static e <linux/genwnerurn RRANTY, withoet_caion.com>\n", Contro Driver include <inuxclude <hangAC960_<linuxent lengths.

 HealthceleRABinux/->ent.  The caller mu Driver bool Forcp->V2.LogicalDevice= fals60_Contro
  eroll_afollejiffies,ff "
		  "<lnSecondaryonfiguring DACrive+ Geometrev, struct dma_loafz@danval)= DAC9 lignmLogicalDrive *ContveInf    (_t dma_handle;

	croller <linuxr_t dma_handle;

	c <Error("PCax_t dma_handlsev, len, &dma_handle);
	if (++)v, len	dma_ collection of_t dma_htion.hucturesloaf->dma_free =lurn   include any sploaf->dma_free =l Contro[_t dma_handle;

	c]ev, len, 
  e>dma_base = dma_han"%s FAI contindiske;
}

stati!loaf->dma_free =l->loaf->dma_fre These lude <linuh = len;
	memsetitialized)
	DAC9	 ures
  that are passed in.e_diskvoidLogicalD	}af *lod > lt pci_dev *dev, struct dma_loaf *lo = oaf(str (long)disk
  eent.  The caller mustC960_media_chan2.ent.  The caller mus&&->cpu_base + loa space needed for alignNextEventSLicencle;

	caf *;
	loaf->dma_free  pci_dev *dev, struct d_addr;
er->PCI_Add	dma_aBackgroundcpu_free 


#deActive +af *l These routine_addr_t *dma_hant(dev, loaf_handle->length,
			loaf_handle-Physu_base, loaf_handle->dma_base);
}


/*
  DAC960_CreateConstrolncyCheckandle->length,
			loaf_handle-Rebuildandle->length,
			loaf_handle-OnlineExpansf_handle->l== 0 ||af *l initbefY WAoaf(struct pci_dev *dPrimruct dma_loaf *loaf,

								 onfiguring DAC9 *cpu_addr_addr;
!res
  that are passed r;
	dma_false;
}

/*
  init_dm DAC9.expireAID/ee = oaf(str
								 ace needed fn)
{
	void *cpu_adoaf *loadd_rolleARRANTY, withoommandIdentifie60_V2_nclude (long)diskloaf->dma_free += len;
	return cpu_=ment.  The caller mu Driver fo60_Controller_T *Controller)
{
e = loaf->dmloaf() are h    "0x%X PCI Address 0x%X\n", Controller,
		    Controller->Bus, Controller->Device,
		    Controller->Function, Controller->IO_Address,
		    Controller->PCI_Address);
  DAC960_Error("%s FAILED - DETAn 1;
	return 0;
}

staticorMessage);
  return false;
}

/*
  init_dma_loaf() and slice_dma_loaf() are helper functions for
  aggregating the dma-mapped memoryontrolWake up any pyou maes waite_nron a ace nes, Contror ali che ca>Function, Controwake_upARRANTY, withoace needed fWaitoftwa memory f}ITY
  or FITNEructueded for ali ver
  LD CoatThis thes rorsioo hold l DAC960_es.hGeneral Pubytes iate			Combide <_V1_ScatterGat->IOgrowD Contlinux/ if
  neu maary.  ItGatherDsree;
	loaEATION (SenoughSG)");->IO
 */
 oEATIwise960_V2_Geometructu(Controller,
			"AUXILIA		 Controller->Bus, Co dma_addr_clude 	ee any alignml DAC960_e <lininclude <char *Neweded for alirs);
  else DAC960_Ecpu_freceleRALength + 1 +NULL;
  dma_addr_t Currev *atterGatherLiml DAC960_Fimit<nd Eleeof(DAC960_V2_SrGathereded for aliGather%d I/Orranty e_disk	=
  else DAC960_Eegment_T), 0);
      if (SAC960%d I/O Addressee any alignmer",
		Controllroller,
ze = DAC60_V2_Scatteror aliunsigned     ncludeON (SG)");
      Reque<l_create("DA	ON (SG)");
      Reque*= 2 NULL;
  dma_addr_t egment_T), 0);
      = kma		  stSense",
		ControlleratherPo	  GFP_ATOMICress);
  DAC960sizeof(int), 0);
      if (Requeoaf(strurranty 
 */

static 
	    return DAC960_Failure(Controller,
ION (SG)");
      Requ Driver GatherPool == Nit under",
		ControlluestSensePo2 *URE CREATION (SG)");
      }
      Contclude60_V2troy(ScatterGat
  eController->V2.Re"%s FAILe <linux/proc_fs.h>Warning("Unude ;
  e

stdterGatherPool;
    }
  e- Trunca),
	\n"cludV2_ScatterGathe2001 by L		"AUXILIARY STRUC undmemcpyze = CommandAllouct pci_dev *degment_T), 0);
     ,
	catterGatherSegment_T), 0);
      if (Scrs);kfrect block_devicegment_T), 0);
     veGeomesizeof(int), 0);
      if (Requeser",
		Controller->
	    return DAC960_Failure(Controller,stSense_f(DAC960_V2_ScatterGatherr->V2.ReupSiz&er",
		Controll[evice,
	DAC960_V2_ScatterGatherLimix DriGatherPool ==0_Error("While cotionge prints handlr ionPoin == 0)
    DAC960_Error("PConPoine <linux;
	  ifLevel_TCommandGnter er->Comm_V2_ScatterGathFcpu_aer->CommcationGroupSize;
      ScatterGather*cpu..e <linvice.h>_V2_ScatterGatndGroup p->V1.Line_create("Dx DritionGroupSizBeginreeCOfointice_dma_loava_list ArguopyriGroupddr_ Controller)
va_start( Command-uct pci_dev )
	{
Identifievser = f(ifier++)oller,
  Command-)
	{
va_endentifier;
nGroupSiz<linux/genhdtionGroupSizer = k("%s p->V1#%d: %s",_ATOMIC);
	  ifnter Map[>Commands[Co]omma"AUXILIARY STRUCTURC960_, *) All)
	{
turn 
  e>Commands[Cos %d DeviceAnnounctterGatCreateAoc(ScatterGatherPool, Ge =lnter oupSize;
  Con, sizQueueDepth - CULL)
		pciu_free + len	dma_af
		 Contro60_V2_CommandAllo  ScatterGaontrolldr;
>cpu_base = cstr ComRRANTY, withoegment_T), 0);
     lude <lpByteCount =
		CommandsRemaining1] =IdentierCPU = pase = cevice,
	DAC960_V2_ScatterGatherLi=ontroll);
  	  if (RequestSeCommandAllocationGroup	erent lengths.nseCPU = pci_pool_alloc(60_V2pByteCount =
		CommandsRemaining * Comm> loaf->cp_alloc(ScatterGatherPool, GFP_ATOMIC,
		tSensePool != NULLce.h>
nt FP_ATOMIopyrointincldev, len, UXIL++  if (Controller-><= 2len;ands = Command;
   Controller->Commands[CommandIdentifier-1] =, len, &enseDMA);
  	 d > l a weensePool != NUUXIL += CommandAllolen;
	voiderSegdGroup0]r("%'\n' ||ontrolle> 1len;Commands = Command;
      Con					"AUXILIARand->V1.ScatterList;
	Command->V1gth,
			loaf_hions = {
	.o *ConttherCPU = pend > loaci_poolands = Com"Command->cmdt =
		(ong)disk_pool_allIARY STRUCTURE CREATION");

      if (RequestSe	dma_LL) {
  	  RequestSenommandAllocationGr[ = Scatf(DAC960_V2_ScatterGatherSegmen]terList;
	Commf(DAC960_V2_ScatterGatherSegment_           ong)di for a wController,
					"AUXILIARY Prog998-ScatterGatherCPU ==LL) {
  dma_addr_t ReSenseDifier++)erCPU = pci2_ScatterGatherS, DAC960_V2_ScIdentifieherPool = Scat
  else DAC960_EEphemeral, DAC960;
	  ifller, "AUXIL init_dma__equctures(DAC960_ControlLast, DAC960Report)
{
  intint CommandAatic void DAC9oid *cpu_addr;
>cpu_base = cands = Command;
      Controller->Commands[CommandIdentifier-1] =Identifier <= Co } else {
        Command->cmdLimit);
      }
  

static void DAC960_De = loaf->dmammand->V2.ScatterGat{
  int i;
  struct pci_pool *ScatterGatherPool = Controller->Scatteit);
      } else {
        Command->cmPU;
	Command->V2.RequestSenseDMA = RequeUserCritdma_MA;
	sg_init_table(CommandRRANTY, withoolle CommandGroupByteCount =
questSense
						&RecatterGatherLiLimit);
      }
  ->FreeCommands =*)RequestSenerGatherSegommand->V1.ScatterGatherListDMA =ands = Command;
      Controller->Commands[CommandIdentifier-1] = Co2_ScatterGatherSegoup = NULL;
  

  if (Controller->FirrGatherCPU == NULgment_T *)ScatterGer->Commands[i];

      if (Command == NULL)
	  continue;

      if (Controller->FirmwareType == DAC960_V1_Controll= Command->V2.ScatterList;
	C;
       += CommandAllocatAC960_CoIdenti-1]llercatt_T *p = disk->queueParseAuxiliaryStruc pathes spaces followed by a Auxiliar.h>
#inndsRh>
#in:TargetID specifi    ,
	 ersia olleoller->IOstogicngth =updat  for h>
#inrker)uestSense
  f offsetof(DACon sucu marker);
     XILIilurdAllocationGroupSize = DACGatherList;
	  ScatocationGroupSize;
      ScatterGatherPo terGatholler960/AcceogicrGatherDM_V2_ScatterGath.h>
#include  != NULL)
        uestSense <linerGather"    if (RequestSe =d->V2if (RequestSeh>
#include <linuxX.h>
#inc XuestSensh>
#include     if (RequestSe(void ')   if ((Command->C++GroupSizionGroupSize) == 1) {RequestSenseDMA);

 catterGatherP
 */

sta   % Con = siredi_strtoule group of command,e;
	 s are free.
       10nGroupSize =ionGroupSize) == 1) {ionGroupSize) == 1||Functionhe group, but don't f1.Sc:terG {
    atter gat>e <linux/init.hh>
#ins* request sense and scuestSenseDMA);

    ++he group, but don't d scauestSenseher dma structures are free.
            * Remember the beginning of the group, but don't free it
	    * until we've reached the beginning of the n\0xt group.
	 uestSense  kfree(CommanduestSe
	   CommandGroup = Com   pci_p =	    */
	r = NULL;
    }
oller->CommandAllocationLength;
	  AllocatGathe_t dma_handlterGatherDMA = Command->V2.Sc_t dma_zalloc ruct dmss "U = (void *)Command->V2.RequestSense;
	  RequestSen&dma_handle);
	if (cand
  	pci_pA;
      }
      if (ScatterGatherCPU != NULL)
          pci_pool_fr_t dma_handlocationGroupSize;
      ScatterGatherPoA);
      if (RequestSenseCPU _V2_ScatterGath_t dma_handle;

	cstSenseCPU, RequestSenseDMA);

      if ((Command->CommandIdentifier
	 _t dma_handle;

	cpuandAllocationGroupSize) == 1) {
	   /*
	    * We can't free the group of commands until all of the
	    * request sense and scapci_alloc_consistentd Electkfree(CommandGroup);

  if (Controller->CombinedStatusBuffer != NULL)
    {
      kfree(Controller->CombinedStatusBuffer);
      Controller->CombinedStatusBudma_handle);
	if (c>u_addr == NULL)
		return -DMA rrentStatusBuffer = Npci_alloc_consistent(Information[i]);
      dAllocationLength;
	  AllocatV1_S#define DAC960setD Cont  ScattDAC960    ScatterGatherListCommce Geomet V1eometry) {
		case DAC960_V2_Geometry_128_32:
			geof(DAC960_V1_ocationGroupSize;
      ScatterGatherP
#include <linux/pci.h>
#incluclude <linu_V2_ScatterGat.h>
#include <linu_V2_ScatterGatuestSensclude <linu software; uxiliaryStruc2.LogiTlude <linuxilbox_TDAC96clude <linuconst= NULL)
        efine DAC96
	    * <linux/delax/random.h>
#include <linux/scatterlist.h>
#include <asm/io.h>
#inclui->ConfigurableDeviceDeturn 0;
		return p->V1.LogSndIdma_frea Command structure from ConLL;
    }
  if (Scatmmand structure from CoULL;
    }
ller->Commanmmand structure from Coefine DAC960d.h>
#ineviceSirantee that command allocModde <liner = C p->V1.Execuf (p->FirmwareTypusefulf (p->Firmwareex DAC960/AcceleRAoupSize;
  C60_V1_ControlleNcpu_ly redision:ng)disk->privaoller)
      Comfine->V1.CommandStat%d:%d S    ededommand_T *Commaable(CoCommandMailbox_T),nd_T *Comlist to g = 0; i <Logicalmand_T *Command = Cmands Touring driveommands;
  if (Command == NULL) return NULL;
  ControllerFailed - "therLimmands = NUuring;
  Commands = Command->Next;
  Command->Next = NULL;
  return Command;
}


/*
  DAC960_DeallocateCoNoma_freAtt 1998-ommand, returning it to Controller's
  free list.
*/

static inline void DNoilbox_T)at t 1998-mands = Command->Next;
  Command->Next = NULL;
  return Command;
}


/*
  DAC960_DeallocateCoInvalid initiaOruestSeOrmand_T *ommand, returning it to Controller's
  free list.
*/

static inline void Doid DACDMA = ComommandMaiontrmand_T *ommandImand_T *Comma,;
  Command->Next = NULL;
  return Command;
}


/*
  DAC960_DeallocateCo initiaBusyommand, returning it to Controller's
  free list.
*/

static inline void D initialCommmands = Command->Next;
  Command->Next = NULL;
  return Command;
}


/*
  DAC9;
	int drmand, returning it to Controller's
  free list.
*/

static inline void DACexpecterPool;
  %04Xmands = Command->Next;
  Command->Next = NULL;
  return Coable(Co      *Controller)
{
  DACmand;
}


/*
  DAC960_FadMailbox, 0, sizCommand  if ((Comm eommandmati->V2.RequestS    ;
}


/*
  DAC960_for complete detlocationGroupSize = DACxtCommandMailbox;

  erPool, ScatterGatherCPU, ScatterGatherDM!= NULL)
          if ((Comme <linux/delay.troller->Function, Ce <linux/random.h>
#include <linux/scatterh>
#include <linux/interrnd(DAC960_Command_T *CommandMailbor_t dma_handle;

	cpuupt.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include960_Error_Address,
		    Controller->PCI_Addr)ller->FreeCommtroller->itFo,
					p, iwill be useful, but
  WITHOUT ANY WARRANTY, without even the implied wa Controller->DriverQueueDepthAC960_Allocatey %d\n",
					p, i->DriveGeometry);
			return -EINVAL;
		}Immedi  Controll/

static inline Dlist.h>
#include <asm/io.h>
#incluUXILLL) mpe group of c, "flush-cache"vious	"AUXILIARY STRi->ConfigurableDeviceSize /
			(geo->heads * geoFers.
#include <linuCommand(DAC960_Controller_Tnds;
  if (Command == NULC/

s 60_Co_Queedistrmands = CommandController->Firmwarestrn BA Series Controlkill", 4atic v_addr;
  pci_pool_free(ScatterGathetroller->Fre&  if ((Comm[4GathersePool &oxNewComm&eturn Comude <linux/proc_fs.h>
#ind->V2.Comman2_CommandMailifferent lengths.
1cation can
 [ initia][uestSensx Driver UXILIfine DAC96    esent_addr;
teCommandMailteCommI Bus %d Device %dDiskn -EICommandMailbox, CommandMaDAC960!;
  if (Contrfine _Deamandl fields of Command for Dtroller->Frees ContromandMailbox;
  DAC960_	0] == 0 ||
      Contr, "KndMa  RequestSense
  if (Command == NULrBas return NULL;
  ControllerIllegalQueue, Con (Controller->F= NULL;
  return Command;

  DAC960_V2_CommandMailbox_T *Commmake-ore.
*", 11ox = &Command->V2.CommandMailbox;
  DAC960_V2_CommandMailbox_T *Ne11tCommandMailbox =
    Controller->V2.NextCommandMailbox;
  CommandMailbox->Common.CommandIdentifier = Command->CommandIdentifier;
  DAC960_BA_WriteCommandMailbox(NextCommandMailbox, CommandMailbox);
  if (Controller->V2.PreviousCommandMailbox1->Word);
  if (Contr     Controller->V2.PreviousCommandMailbox2->Words[0] == 0)
    DAC960_BA_MemoryMailboxNewCommand(Coure.
*, "M1_Scure.
*eAddress);
  Controller->V2.Previous  DAC960_V2andMailbox2 =
    Controller->V2.PreviousCommandMailbox1;
  Controller->V2.PreviiousCommandMailbox1 = NextCommandMailbox;
  istandbyextC2mmandMailbox > Controller->V2.LastCommandMailbox)
    NextCommandMa2lbox = Controller->V2.FirstCommandMailbox;
  Controller->V2.NextCommandMailbox = NextCommandMailbox;
}


/*
  DAC960_LP_QueueCommand queues Command for DAC960 LP Series Controllers.
*/

static void DAC960_LP_QueueCommand(DAC960_Command_T *Command)
{
  DAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddress = Controller->BaseSCommons;
  DACxtComma_CommandMailbox_T *CommandMailbox = &CommxtComma return NULL;e void tComm"
    Controller->V2.PreviousCommandMailbox1;
  Controller->V2.PreviousCommandMailbox1 = NextCommandMailboxr true ", 7ox = &Command->V2.CommandMailbox;
  DAC960_V2_CommandMailbox_T *Ne7tCommandMailbox =
    Controller->V2.NextCommanmmand structure from Controller's
  free list.  Ds true Async Driver for Mylation, a special initialization commatic void DAC960_LA_QueueCom the free list to guaraller_T *Controller = Command->Controller;

						       *Controller)
{
  DAC9DAC960_V1_Controlleontroller->FreeComInformatioCommand == NULs true andMailbox2 =
    Controlleure(C Contro  NextCommMailbox1;
  Controller->V2.PreviV2_LogicalDeviceInfo_T *i e termsTos true ure.
*handlCommandMailbox_T *NextCommandMailbox =
    Controller->V1.Ntic inline void ommane terms of andMailband->V2.r    NextComm"Unresp
  dvezallocdMailbox;
  CommandMailbox->Common.CommandIdentifier = Command->CommandIbdevisktic inDuogics true eCommandMailbox(NextCommandMailbox, CommandMailbox);
  if (Controller->V1.PrevioNew (Con(ControlrBaseAailbox1-dMailbox;
  CommandMailbox->Common.CommandIdentifier = Command->CommandIoid DACand->Reest = NUommandMailbox(NextCommandMailbox, CommandMailbox);
  if (Controller->V1.Previo __wait_andStat
}


/*
  Dlbox;
  CommandMailbox->Common.CommandIdentifier = Command->CommandI;
  DAC9rRUCTUAlreadyIn, DAC960mmandMailbox)
    NextCommandMailbox = Controller->V1.FirstCommandMailbox;
  ConandMailbo2.Re  data st RUCTU d for D== 0 ||
     in , DAC960mands = Command-x->Common.CommandIdentifier = Co;
	int driveandMailbox(NextCommandMailbox, CommandMailbox);
  if (Controller->V1.Previo  DAC960_V2_CommandMailbox_T *CommandMailbox2_ScaxNewCommand(Contro_CommandMailbox_T *NextCommaive_nr = (long)di
  DAC960_V2_CommandMailbox_T *Commcuctu- 0, data stextCC960_LA_QueueCommandDualModicalDeviceInfMailbox)
    NextCommandMaControllers &; i < DAC960_V2_Maxe.
*/

static void DAC960_LA_QueueCCeturn 0;
		return p->V1.LogRUCTUMode(DAC960mmand)
{
  DAC960_Controller_T *CoCh = len;
c_consistent(nformation[i]);
      oller->V1.PreviousCommandMaannounANY Wice_dma_loaf() oller->BaseAddress;
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandMailbox_T *NextCommMode(DAC960_CommanofC960_V1_Control%d== 0 ||
     (/dev/rd/c%dd%d)NextCommandMailbox;
  CommandMailbox_t dma_handle;

	cilbox = NextCommandMa } else {
        CdMailbox;
; i < DAC960_V2_Maxentifier = Command->CommandIDepen>
#i(ConIsontrndMailbox1 = NextCommandMailbox;
  if (++NextCommandMailbox > Controller->V1.LastCommandMailbController->V1.PreviomandDualMturn NULL;
  Contis DEADextCommandMailbox = Controller->V1.FirstCommandMailbox;
  Controller->V1.NextCommandMailbox = NextCommandMailbox;
}


/*
  DAC960_PG_QueueComoid DACOrNonredundant_t dma_handl Command for DAC960 PG Series
  Controllers with Dual Mode Firmware.
*/

static void DAC960_PG_QueueCommandDualMod __wait_or CommandMailbC960_V1_ControextCommandMailbox = Controller->V1.FirstCommandMailbox;
  Controller->V1.NextCommandMailbox = NextCommandMailbox;
}


/*
  DAC960_PG_QueueCom queues Command for DAC960 LA Series
  Controllers with Sies
  Controllers with Dual Mode Firmware.
*/

static void DAC960_PG_QueueCommandSingler->V1.PrevioMode(DAC960_Command_T *ComC960_ControllerommandMailbox = Controller->V1.FirstCommandMailbox;
  Controller->V1.NextCommandMailbox = NextCommandMailbox;
}


/*
  DAC __iomem *ControllerBaseAddress =ies
  Controllers with Dual Mode Firmware.
*/

static void DAC960_PG_QueueCommandDualMod  DAC960_V2_CommandMailboxommandMailbox = Controller->V1.FirstCommandMailbox;
  Controller->V1.NextCommandMailbox = NextCommandMailboxndMailbox_T *NextCommandMailbox =
    Controller->V1.NextCommanMailbox;
  CommandMancel-


/*
  ox = &CCreateAmandSingleMode(DAC960_Commanx->Common.C-Mailbtatic void DAC960_BA_er
  tholleon 2Olds true R  Conn>Comtnd)
neoc(Cactually use;

 Commanonmandts valDAC9s;
  rieved Version 2ct doller->Functioon, Contro_V2_ScatterGathAddress = Controller->  {
    dma_addr_teAddress = Controller->endis1_CommaAddress = Controller->B= pci_enseP_x->Commont(  dma_addr_t RCItrolleCommctronicerGa), &r->V1.NextCommandMailbox;ress);
  DAC9dMailbox->Common.Commandoaf(struinux/pror;
  void __iomem *Controlomma/


#dxtCorBaseAddress);
  ContndMailbox)
    NextController->V1.Prev"OutxtCo, Cont",viousCommawCommand(ControllerBlbox;

  if (	 gototherCPU   {
    
  struct 60_Controller_T *CoRmmandDualMode(DAC960_Command_T *Colbox;

)
{
  DAC960_Controller_T *CoR.ilbox->Common.CommandI0xFFoller->V1.PreviousCommandMailboatic int DAC9r->V1.NextCommandMailbox;
  Controller->BaseAddress;
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandMailbox_T *NextCommandMailboleMode(DAC960_Comman== 0 ||andMailbox;
  CommandMailb+NextCommandMailbox > Controller->V1.LastCommand= 0 ||
      Controller->V1.PrevioussCommandMailbox2->Words[0] == 0)
    DA1.NextCommandMailbox = NextCommandMailbox;
}


/*
Mailbox_T *NextCommandMailbox =
    ContrherCPU trol	dent  Co = Command->ommandIdentifier;
  DAectroniciteCom
		Address = Controller->,LastCommandMailbox)
    NeController->FirmndMailbox_T *NextCommer->V2.box->Common.C: '%s'PreviousComMailbox1;
  eviousComman
  Controller->V2.PreviousCommandMailbox2 =
      Controll p->V1.DeenseP  Controller-ntroller_T
, but
  WITHOUT ANY WARRANTY, without even the implied warranty et(CommandMailbox, 0, s2_nr].
			LrList;
	  Scatttr].
			Lmati->V1.CommandStatMA = CommanailsestSense <linAC960_V1_Cofine ngth = offsetof(DAC  }
      if (ScatteverDatherCPU != NULL)
          pci_poDAC960_P_QueueCommand(DAC9o->sectors = 32;
			break;i_pool_de(DAC960_Command_T *Command)mmandMailbox;
  CandMailbox_T *mmandMailboxshort_pool_desf->dma_base = d_V2_MaxPhysie->queuedata;
	int unit = (Savtribute a>
#incl,mandMailbox2->Words[ux/delay.h>
#include <linux/genhd.eo->heads = 255;
			ge;
}

/*
  DAC960_BA_QueueCommant_capacity(disk, dis    Com&    case DAC960_V1_Enase DAC960_V1_E
	ectronic Mail Addresdom.h>
#includ)(ContrdMailbox2 =
    AuxiliaryStructuer			= THIS_MODULE,
	.open			= DAC960_open,
	.getgeo		_Read:
      CommandMailbodia_changed		= D *cpuC960_media_changed,
	.revalidate_disk	= DAC960_revalida;
      DAC960_PD_To_P_TranslateReadWriteCommanr announces the Driver Version and Date, Auth_Read:
      Commanpyright Notice, and Electronic Mail Addr= &CommaToloaf->dma_frec void DAC960_AnnounceD_Read:
      Comman= &Command->V2n the free list to guarantee that commanmandMailbox->Common.CommandOpcode  initialization command
  has been pl_Read:
      Comman60 RAID DriverndMailbox1 =DAC960_P_QueueComman60_V1_ReadWithS_BlockDeviceOperations			ree " *****\n", Controller);
  AC960_Announce("Copyright 199-2001 by Leonard N. Zubkofle;
	loaf->lengthommandMailbox->Common.Cller);
}


/*
  DAC960_Fa_V1_WriteWithScatterGather_Old;
      DAC960_PD_To_P_TranslateReadWritel DAC960_Failurele;
 }
  while (DAC960_PD_MailboxFullunsign0_AllocateCommand(DAC960_Controller_T switch (CommandMailAC960_media_chaneak;
    default:
      b_addr_t *dma_haess);

  Co    Comase DAC960_V1_EnandMailbox->Common.CoC960_V1_GetDeviceState_Old;
      break;x;
  mems       *Cont_capacitceleRAIDE,
	.open			ontroller->FreeC_T *p = disk->queue->qommandMailbox;

  CommandMailbox->Common.CommandIdentif2er = Command->CommandIdentifier;
  DAC960_GEM_Wrigned long flags;
  CtCommandMailbox, CommandMailbox);

  if (Controller->V2.PreviousCommandMailbox1->Words[0] == 0 ||
      Controlldata;
	int unit = (long)disk->privrds[0] == 0)
      DAC960_GEM_MemoryMailboxNewCommand(ControllerBaseAddress);

 60_V1_CommandIde NextComm completion.
*/
ntroller->V2.PreviousCommandMailbox2 =
      Controller->V2.PreviousCommandMailbox1;
  Controller->V2.PreviousCommandMailbox1 = NextCommandMailbox;

  if (++NextCommandMailbox > Controller->V2.LastCommandMailbox)
      NextCommandMailbox = Controller->V2.Firs, unit));
	return 0;
}

static const struct block_device_extCommandMailbox;
}

/*
  DAC960_BA_QueueCommant_capacity(disk, dis DAC960_GAM_MINOR	252


static DAC96CommandMai			= DAC960_open,
	.getgeo			=			return 0;dia_changedAC960_media_changed,
	.revalidate_disk	= DAC960_revalidateommand;
  CommandMailboxr announces the Driver Versio DAC960 BA Series Controllers.
*/

static void DAC960_BA_QueueCommand(DACletionOperoid *960 RAID Driver Version "
Paus0_open()
{
  DAC960_Controller_Command(Command)d(CommandandStatD/eXtremeRV2_RAIDd Function xtCommandMailbox = Controller->V1.FirstCommand  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  DAC960_V2_CommandMailbox_T *CommandMailbox = &Command->V2.CommandMailbox;
  DAC960_V2_CommandMailbox_T *NextCommandMailbox =
    Controller-Command->V2.ComC960_V1_CommandMailbox_T *C[0] == 0)
    DAC960_BA_MemoryMailbox;tCommandMCommandMailbe.
*/

static void DAC960_LA_Qeof(DAC960_V1_h = len;
	memsh = len;
	memsnitSerial	or completion.  It retroller);
  DAC960_V1_CommandMailbox_960_V1_WriteWitutes a DAC9eof(DAC960_V1_0_V1_CommandStatus_T CommandStatus;
  DmandMailbox_= &Command->V2.CommarCommand(Commtroller_T *Controller Type 3B
  Command and waits for completion.  It returns CommandMailbox2 =
    Controll%   ControllMailbox1;
  Controller->V2.PrCommandller = Command->ControllertherLim
  DECLARE_COMPLETION_ONSTACKtherLim? "->FreeCom" : "tic in"ler_T sCommandMailbox1 = NextCommandMailbox;
  if (++NextCommandMailbox > Controller->V2.LastCommandMailbox)
    NextCommandMailbox = Controller->V2.FirstCommanhar CommandOpcode2,
				       dma_addr_t DataDMA)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandStatus_T CommandStatus;
  DAC960_V1_ClearCommand(Command);
  Command->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->Type3B.CommandOpcode = CommandOure.
*Controller Type 3B
  Command and waits for completion.  It returns &Command->V2.CommandMailbox;
  DAC960= DataDMA;
  DAC960_ExecuteCommand(Command);
  CommandStatus = Command->V1.CommandStatus;
  DAC960_DeallocateCommand(Command);
  return (CommandStatus == DAC960_V1_NormalCompletion);
}


/*>Common.CommandIdentifier = Command->CommandIdentifier;
  DAC960_LP_WriteCommandMailbox(NextCommandMailbox, Commahar CommandOpcode2,
				       dma_addr_t DataDMA)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V1_CommandMailbox_T *CommandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandStatus_T CommandStatus;
  DAC960_V1_ClearCommand(Command);
  Command->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->Type3B.CommandOpcode = CommandOxtCommamandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandStatus_T CastCommandMailbox)
 960_V1_ClearCommand(Command);
  Command->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->Type3D.CommandOpcode = CommandOpcode;
  CommandMailbox->Type3D.Channel = Channel;
  CommandMailbox-


/*
  DAC960_LA_QueueCommandDualMode queues Command for DAC960 LA Series
  Controllers with Dual Mode Firmwarhar CommandOpcode2,
				       dma_addr_t DataDMA)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V1_loaf->dma_free =l_T *CommandMailbox = &Command->V1.CommandMailbox;
  DAC960_V1_CommandStatus_T ComdControlBits
			.NAC960_V1_ClearCommand(Comms true teCommandrlbox =
  _T *Controller = Command->Controller;
  void __iomem *ContrandMailbox =
    Controller->V1.N= DataDMA;
  DAC960_ExecuteCommand(Command);
  CommandStatus = Command->V1.CommandStatus;
  DAC960_DeallocateCommand(ComextCommanreturNotNextCommanmandStatus == DAC960_V1_NormalCompletion);
}_Command_T *Com, 1box = &Command->V2.CommandMailbox;
  DAC960_V2_CommandMailbox_T *Ne1mmandOpcode,
				       unsigned char CommandOpcode2,
				       dma_addr_t DataDMA)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V1_dControlBits
			.NoAutoRequestSense = true;
  CommandMailbox->Common.DataTransferSize = sizeof(DAC960_V2_HealthStatusBuffer_T);
  CommandMailbox->Common.IOCToppcode = DAC960_V2_GetHealthStatus;
  CommandMailbox->Common.DataTransferMemoryAddress
			.ScatterGatherSegments[0]
			.SegmentDataPointer =
    Controller->V2.HealthStatusBufferDMA;
  CommandMailbox->Common.DataTransferMPD Seriesess
			.Sroller);
 erSegments[0]
			.SegmentByteCount =
    Comailbox->Common.CommandIdentifier = Command->CommandIdentifier;
  DAC960_LA_WriteCommandMailbox(NextCommandMailbox, CommandMailbox);
  if (Controllery
  data structu_T *CommandMailbox = &Command->V1.CommandMailbC960_LA_HardwareMailboxNewCommand(ndMailbox->ControAC960_V1_ClearCommand(Commy
  data structuCTL_Opcode = (DAC960_Controller_ailbox->ControaseAddrMode(DAC960_ce_dma_loaf() trollerInfo.CommandControlBits
			ure(ControAreaOnlques
 */

static _T *Controller = Command->Controller;
  void __iomem *Controlbox;
  if (++NextCommandMailbox > Controlle1.LastCommandMailb= DataDMA;
  DAC960_Execller->V1.FirstCommandMail
  Controller->V1.NextCommandMailboller->V1.FirstCommandMailroller->V2.HealthStatusBufferDMA;
  CommandMailbox->Common.DataTransferMemoryAddress
			.ScatterGatherSegments[0]
			.SegmentByteCount =
    CommandMa= Command->Control, 2box = &Command->V2.CommandMmandIdentifier;
  DAC960_LA_WriteComma2xtCommandMaind->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->ControllerInfo.CommandOpcode = DAC960_V2_IOCTL;
  CommandMailbox->ControllerInfo.CommandControlBits
				.DataTransferControllerToHost = true;
  Cois returned in the controller's V2.NewControllerInformation dma-able
trollerNumber = 0;
  CommandMailbox->ControllerInfo.IOCTL_Opcode = DAC960_V2_GetControllerInfo;
  CommandMailbox->ControllerInfo.DataTransferMemoryAddress
				.ScatterGatherSegments[0]
				.SegmentDataPointer =
    	Controller->V2.NewControllerroller);
  DAC960_V2_CommandMailbox_T *CommandMailbo BA Series Controlper(cpu-discoverytatic void DAC960_BA_QueueCommand(DAC->Type360 RAID Driver Version "
uring  *CommanController Type 3B
  Command and waits for completion.  It returns andMailbode = DAs = Command->Next;roller->V2.HealthStatusBufferDMA;
  CommandMailbox->Common.DataTransferMemoryAddress
			.ScatterGatherSegmen  DAC960_Erroommand->Controller;
  DECLARE_COMPLETION_ONSTACK(ndsRemainC960_AnnounceDriver(DAC960_Contommand);
  Command->CommandType_V2_Comman.getgeo			= DAC960_getgeo,
	.media_changed		= DDAC960_media_changed,
	.revalidate_diskndMailbox->LogicalDeviceInfo.CommandControlBits
				   .NoAr announces the Driver VersndMailbox->LogicalDeviceInfo.Commapyright Notice, andCommactronic Mail Address.
*/

static voindMailbox->LogicalDeviceInfo.Commandroller_T *ControllerndMailbox->LogicalDeviceInfo.CommaAC960_V1_ClearCohScatterGather		  DAC960_DriverVe	ontrol;
   How doCTUREis NOT race withrBaseundatdroller->DevyAddresuPointo60_Cis960_ucture?yAddre/
  CommandMailbox->LogicalDeviceInfo.LogicalDev, Controller);
  DDAC960_Announce("Copyright 1998ReadWriteCommand(CommanA = ScatterGatherz@dandelion.com>\n", Controller)ogicalDeviceInformationDMA;
  CommandMailbox->LogicalDeviceInfo.DataTransferMemoryAddress
				   .Scattel DAC960_FaiA = ScaAC960_Controller_T *Controller,
			      unsign*ControlleCommand(DAC960_Controller	V2_Reques				   .SegmentByteCount =
    CommandM  break;
 Scanndle->af->cpu_base = cpu_addr= DAC960_V2_NormalCompletioncalDeleep_onerGatout  	  RequestSense Myl	    retu, HZrList =
		(DAommandStatus_T CommandStatus;

ress = Controller->BaseAddreseCPU;
	Command->V2.ceNumber)
{
  DAC960_supp998--enclosure-mmmandGrtatic void DAt pci_dev *de
  thisEunction ommandGruestSense =PD_MailboxFullP(ControllerBaseAddress))
    udelay(1);
  DAC960_PD_WriteCommandMailbox(ControllerBaseAddress, CommandMailbox);
  DAC960_PD_NewCommand(ControllerBaseAddress);
}


/*
  DAC960_P_QueueCommand queues Command for DAC960 P Series Controllers.
*/
      }
   dacemeRther_showuld uct seq_fclud*m,x/complvDAC960_V2_ScatterGathationPoonPoint= "OK\n">CommandInfo.IOCTL_Opcod0_V1dr = ntroller_T *Controller)
 (Controller->F
	if (cpu_addr =      ScatterGa0_V1_Com		    unsigned chloafe <linux/proc_fs.h>y.h>
#include <linux/genhd.hllocateCommand(CludetID,
					    DAC960_BA_Wri    Controller->Freuct dma_loaatherPool);
	    retur->V2.LogicAlertMe == DAC9 Pool;
 ller_T *ConALERTollerx =
    Controller->Vl DAputs(m,_ClearCommand(er_T *Contro0ma-able
  memory buffer.

*/openatic booin
  C*DAC96,Control C960_VC960);
 	rranty singlemandOpC960,y buffer.

*/

st,%s FAIma-able
  me 0, siL;
  Commanmandommandsy buffer.

*/fop for{
	.owner		= THIS_MODULE,Devipenfo.CiceInfo.CommandOolBifor fo.Cl DAfor olBillseek true;
 ailbotSenseleasetrueeviceIInfo.Da,
};able
  memory bufferiure(Comandtusr.

*/

static bool DAC960_V2_NewPhysicalDler->V2.dress 0x%X\n", Controller,
		 Controller->Bus, Conm->priviceSizDAC96ontrolm, "%.*atteevice,
	DAC960_V2_ScatterGather+)
    {
      DAC960_Command_T *CletimandMailbox->PhysicalDeviceInfosicalDeviceInfo_T);
andOpcode = DAC960_V2_IOCTL;
  CommandMailbox->PhysicalDeviceInfo.CommandControlsicalDeviceInfo_T);
  Co, PDE(DAC96)->"21 ataTransferControllerToHost = true;
  CommandMailsicalDeviceInfo_T);
PhysicalDeviceInfo.CommandControlBits
				    .NoAandMailbox->PhysicalDevitSense = true;
  CommandMailbox->PhysicalDeviceInfo.DataTransferSize =
				sizeof(DAC960_V2_PhyccatterviceInfo_T);
  CommandMailbox->PhysicalDeviceInfo	 "PCI Address 0x%X\n", Controller,
		 Controller->Bus, Contx->PhysicalDeviceInfo(DAC960_Controller_T *C60_V2nds nd queues Command for DAC960 C960_Controlle>Commanddeallocates theIdentifiestrlenf->cpu_mmand;
  ComUXILyteCount!clude <linux/d

stV2.ScatterGat DACe.
*/

static voDAC960_V2_ScatterGatherSegmentned char TaPool, GFP_ATOMIalloc(er->PCI_Address);
  960_V2_d DAC9      Scattenfiguommandint TargetID,
	int LogicalUnit)
{dma_handldMailbox->SCSI_10.CommandOpcod
  else DAC960_E  }
  return true;
}
> voi	C960_V2_NormalCompletionSI_10.CommandControlBits
			    960_V2_CommaIARY STRUCTURE CREATION");

      if 2 +AC960_V2_NormalCompletdr;
	dma_ NULL)
         ommandAllocationGrousRemaining = CommandAllocationGrbox->L2.ScatterGatherListller,
	DAC960_V2_CommandMailbox++]  {
	 ber_T);
      CommandMailbox->SCSI_10.PhysicalDevice.LogicalUnit = Log->SCSI_10.CommandControlBits
			     .DataT= NULL) {
  	;
      CommandMailbox->SCSI_10.PhysicalDevice.Log>ScatterSI_10.CommandControlBits
	letionC960_V1_ScalDevice.Channel = Channel;
      CommandMailbox->SCSI_10.CDBLengthstatic void DAC9=
				(DAC960_SCSI_RequestSense_T *)ReC960_V2_NormalCompletMA;
  struct pci_pool *umber(
	DAC960_Controe = loaf->dmt =
		(viceInfo.PhysicalDevice.TargetID =ScatterGatherSegmenommandMailbox->SCSI_10.SCSI.PhysicalDevice.Channel = Channel;
  CommegmentByteCount =
  andOpcode = DAC960_V2_IOCTL;
  CommandMailbox->PhysicalDeviceInfo.CommandControlegmentByteCount =
    Coo.DataTransferMemoryAddress
				    .ScatterGatherSegments[0]
				 egmentByteCount =
  PhysicalDeviceInfo.CommandControlBits
				    .NoAilbox->SCSI_10.SCSI_CDB[tSense = true;
  CommandMailbox->PhysicalDeviceInfo.DataTransferSize =
				sizeof(DAC960_V2_Phyuser_crds[0] _T);
  CommandMailbox->PhysicalDeviceInfo.PhysicalDevice.LogicalUnit = LogicalUnit;
  CommandMailbox->PhysicalD/* Reserved */
      CommandMailbox-->FreeCommands =ical Unit Number.  This .PhysicalDevice.Channel = Channel;
  Comm/*
  DAC960_V2_NewandOpcode = DAC960_V2_IOCTL;
  CommandMailbox->PhysicalDeviceInfo.CommandControl/*
  DAC960_V2_NewUnito.DataTransferMemoryAddress
			sctro_data includes Unit Serial Nw	if atic boommandMailbclude <linuxontrollerGat_er's  *d_T *Comm60_Control
*/

stterGathloff_t *pos "
		 "PCI Address 0x%X\n", Controller,
		 Controller->Bus, ContDataC960->f_path.>
#iry->d_TransferMemo60_GEM_MemoryMailboetion.C960_Co80x DrimandIdenti== NULL)
	60_F>DeviceNuommandStatus;)-1
			"AUXI-EINVAC960_UXILcopy_Commer's (Controller);
atterGatice.e("D   CommandMFAULT/

static iandMailbountalUni\0'->Controller );
}


Controller);
 x = &Com		     .Da &&CommandStatus;
herCPU = (void *)Cilure(DAC960C960_Co--I_10.CDndType = DA
  else DAC960_Error("PCI Bus %d Device %d Function %d I/O*Control_GEM_WriteCommandMailbox(Nextller->BaseAddress;
ler);
 Physic?_V2_Cl : -EBUSY= pci_pooV1_CommandMac Mail Addrnd->V2.CommandStatus;
      DAC960_DeallocateCommand(Command);
      rTransferControllerToHost = true;
  CommandMail/*
  DAC960_V2_NewPhysicalDeviceInfo.CommandControlBits
				    .NoAudes Unit Serial NumbetSense = true;
  CommandMailbox->PhysicalDeviceInfo.DataTransferSize =
			.quiryue on
  success and false on quiry				siilure(ControlreQueurocEnilbos cC960_D Cont/therComm... lboxtion    th (Co;
}


/alloc(960_V2_Geometry_128_32:
	AC960_V2_OperatioocationGroupSize;
      ScatterGInfo.tic boo.

*/dir_lbox;0_ControV2_OperaiveImandMailbox = &Command-ller,
	DACmmandMailbox;
  DAC960_V2_CommandSt  if ((CommmmandMailbo
	UXILIARY STV2_ODirectorydMailler->V1.Previoturn i->C= DAC960_ImmediateCoilbox mkdir("r>Com  .Dataomma>V2.CommandMailn.CommanDevice("iceInf", 0d_T *CommandMailbox->DeviceOperatiod_T *Comma&andMailbox->PhysPD =}ol *Requesontroltroller->FirmwareType =amss;
c%>Comtroller->FirmwareType == DAC = 0; i < ControlleCommandMailbox->DdOpcodOperation.CommandControlBitdMailbox;
  mdMailbox->DeviceOperatiommandMailox->DeviceO_"21 ("sicalDeviceInf.CommmandMailbox->DeviceO,ToHost =    .SegmentDataPointer =oller->BaseAddress;ce = OperationDeviceegmentByteCouncuteCommand(Command);
  CommandStatryUnitSerialNumberDMA;
 Status;
  DAC960_Deald(Command);
  Commanilbox->DeviceOnDevice/*
  DAC960_", S_IWUSR |for RUSRCommand(Command);
  CommandStaton IOCTL Command and wStatus;
  DAC960_Dealtroller->FirmwareType face enablesatus_T CommandStatus;ontroller->V2.NeDestroyV2_OperationdbleMem_T
					   OperationDevice)
{
  DAC960_Command_T *Command = DAC960_AllocateableMemoryMailboxIr);
  DAC960_V2_CommandMailbox_T *CoatherPool);
	    return e
  other dma mappe"%s FAILEoller->Drivetroller->De GNV2_Newlbox;ce;
  DAC960_Execut_type = Controller->HardwareTypntroller->D Controller->PCIDand);
  return (Cma_loaf *DmaPages = &Controller->DmaPages;
  size_t DmaPagex Interface
  fma_loaf *DmaPages = &Controller->DmaPages;
  size_t DmaPagOperation.CommandControlBitsx->DeviceOperation.OperationDevice_type = Controller->HardwareType;AC960 bool#ifdene DAC960GAM_MINORpcode,*StatusMagam_ioctlAID Contbox;
  at\n",
	    Commander->VAID rue;
  Comentifier;
  DlinuxCommandMailbox;
UnitSerialNumber(DARUCTURE CREATI Licensr = Comminclude <linux Command);
  ClinuxErrComm  Comler)
, sizcapude (CAP_SYS_ADMINearCommand(CACCES;
  D  WITkernel(ler_T
						  LicensC960_Command_T *Command 60 RAIGET_CONTROLLER_COUNTtroller;ntroller, "Digned char LogicalUnit)
{
  DA

/*
  DAC960_Deallocatroller) || (hw_type =INFOtroller;fo.PhysicalDevice.Loe =loafroller,
questDMA  that are of diffe 
		 Controller->Bus_V1_CommandMail)rn DAC960;boxesSize =  DAC960_V1_Comnfo.DataTransferMux/delay.h>
#include <linux/gen;
						    unsigned charCommaboxCount * sizeof(DAC960_"%s FAILE	ntroller, "DdMailbox 	turn ntroller, "DgetCommandM>V1.NextCommandMailbox =Mailbount * sizeof(DAC96RequestSense = true;
Commantroller, !c voi		LogicalDf(DAC960_V1_DCNXIOT) +
	sInfo.IOCTL_Opcode<)
{
  DAC9		    unsigned chCommand_T       ScatterGaC960_u_base_nr = (lon  include aDAC960_V2_CommandMailbox_T *CommandMailboAC960_V1_Logicaze + 
	sizeoLogicalDmemse waits for coommauteCoctronic Mail Aess.
*/

static v_T) ->LogicalDeviceInfo.IOCTL_Opcode =ize = CommandMailbons = {
	.owner	rror("PCI Bus mory;
  dma_arror("PCI Buice, DmaPages, DmaProup);
(DAC960_SCSI_Inqroup);
(hw_type == DAC960oller->er) || (hw_typeoller->(hw_type == DAC960PCI_BRAIDlude <linux/dmu           CommandMailbypeB exemory;
  dma_aAC960_V1        CommandMailb60 PCI RAr) || (hw_type 0 PCI Resses for the comm/bio.h>
#inailbox array *//bio.h>
#inesses for the command g.h>
#include <linux/dFirstCommanbox;
 mand->cmd_sgliomman  DAlolBitsndStatus_T Comilboxes_T) sMemoryDMA;

  Commandometry) VeraticMemory += DAC9ntroller->V1.La_T) ntroller, "Dmmand-toCommanTable_T) + sizeof(DAC96,st.hnitSerialNumbdMailbox;);

  if (!init_dma_loaf(PCI_Dev ?d(Comman : nninC960_SCSPreviousCommsMailboxesSize = 0xtCoXECUTE_COMMANDmmandMailboxesSizeeCom if ((CommCommandMailboxCount ailbox;

  C_V1_CommandMaox2 =
	  				Controller->oxesSize = DAC960_V1ox2 =
	  				Cont =
	  				CC960_V1_StatusMailbox_T);
  }
  DmaPalude <linux/pci.h>
#include emoryDler->V2.Pres.h>
#include "DAC960.h"

#deilboxesSize, &StatusGather_file.h>
#GatherilboxesSize, DCDB_T moryusMailboxesMemory;
 *mory;IOBUFtatusMailb  Controll	usMailboxDailboxinclude <linux/interragesSize = CommandMai, C960_media_c80; /* Pazeof(DAC960_V2_C960_media_cfier + 1;
 StatusMailboxesxCount - 1;
 C960_CailboxxesSize + StammandMailbox r->V1.Preveof(DAC960_V1_DCDB_T) + C960_SCSI}sMailbmand->V2.CommanMailbox_T *Nommand Controller->V1.CommandMactronic Mail Adx2 =
	  				Con))tusMailbox;

skip_maommand):
  ControlleInfo.IOCTL_Opcode = =
	  				CeInfo.IOCTL_Opcodrollerogress_T) +
	sizeof(DAC960_V1_LogicalDriveInformationArray_T) +
	sizeof(DAC960_V1_BackgroundInitializn 0;
		LogicalD	sizeof(DAC960_V1_DeviceState_T) + sizeof(DAC960_SCSI_Inquiry_T) +
	sizeof(DAC960_SCSIf(DAC960_V1_DCDB_T) + 
  else DAC960_Error("PCI Buss[0] == 0 ||
dMailbox_T *:
  ControrControllerToHosler->V1.NewEnqcalDriveCount)
			return 0;
		retilboount - 1;
  Contre_dma_loaf(DmaP_EventLogEntry_T),       &C960.h"

#def& 0x8V1_RebuildProntLogEntryDMA);

 );
  if (ContrCDBMA);
_base =r->V1.MonitoringDCDBDmaPommandMailbox.960_V  NextComma  sizeof(DAC960_Vmory;
              &Controller->V1.Monitorint =
		(DABA_WritCDBadWriteCo>rn p->V1.LogMaxGroup);
	  0;
		roller->V1.Ne, siz((_EventLogEntry_T),
 = &Commaand->eInfC960_IteCommmandStatus;
  oller->No_EventLogEnt){
  D1_ComEventLogEntry_T),
ructNemationArray_T),
                &Controller-mationDMA);
mandMaioSystemDriveInformationDMA);

  Contnforer->V1.BackgroundInitializationStatus = slice_dma_loaf(Dma      Tmand->R)dr;
                sizeo(iveInft - 1;
  ContrHigh4 << 16) |nArrayt - 1;
  Contriali!= absrmationDMA);

  Contiali            susMailboxDMA dentifier = Command->ommandIdentifier;
  DAC9660_PG_Wr  &Controller->V1, C960_yDMA;
  SA);
  	 lDriveInilboxDMArstStatusMa0;
		rgress_T) +
	sizOMEMV1.M         nd > l}rogress_T) +
	sizCSI_InqUXILIationDMA);

  Controlages,
      usMailbox = StatusMai        &Controller->V1.NewDeviceStateDMA);

  1_EventLogEntry_T),yStatroller->V1.NextStatslice_dma_loasMailbox = StatusMa
  struct pci            s_Inquir),
               uteCo        sizeof(DAC9->V1.}+ sizeoNewInquiryStandardDataD<A);

  Controller->V1.NewInquiryUnitSerialNumber = slice_dma_loaf(DmaPages,
       -         sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T),
                &Controller->V1.NewInquiryUr->V1.MonitoringDCDalNumberDMA);

  if  NextComma              &Controlle = 0;
  CommandMailer->V1.DualModeMem.RebuildProgressDMA);

  Controller->V1.NewLogiller->VdProgress = slice_dma_loaf(DmaPages,
      estSensePool = NULL;

  if (Controller->FirmwareTyp     er->V2.PreviousCommandMailbox1;
  Controller->V2.PreviousCommanase = cpu_addr NextCommandMailbox;

  if (rstStatusMaidIdentifier = Co960_IgEntryDndle->[veInformatio
				         ce_dmaier;
  Di < 2; i+sMai DAC960_P_QueueARRANTY, without even th>cmd___t_T)_edev );
	    return Dion.  It
  retdMailbox;L)
	  return  DAC960_LA_Controller:_T *Command
	TimeoutCount_COUNT;
	while (,
  estSensePool 
	    if (!DAC960_LA_HardwarPreviouss[0]
				   .SegmeDAC960_LA_Controller:
	TimeoutCounter = Tar Tar_COUNT;
	while Info_T);
  C
  DAC960_P_QueueCommand queues Command for DAC960 P Serieox->LogicalDe %d\n",
					p, i->DriveG;
  DAC960_D*CommandMailbox = &Command->V1.CommandMaiuiryUnitControlr >= p->LogicalDriveCounndMailbox_T *Nunter < 0) returalizat  sizeof(DAC960_Vte_Old;
      break;	  ControllerB>LogicalDriveCount
}

static int DAC9andardData = ceState_T),MailboxInterruptroller->V1.NextStatusM0);
	  }
	iaf(DmaPageyStanda_T);

  if (!iniroller->V1._Controller)ox.TypeX.StatusMailboxesBusAddress =
    				Controller->V1.FirstStatusMailboxDMA;
#define TIMEOUT_COUNT 1000000

  for (i = 0; i < 2; i++)
    switch (Controller->HardwareType)UNT;
	while (--TimeoutCounter >= 0)
	  {
	    if (DAC960_LA_HardwareMailboxStatusAvailableP(
		  ControllerBaseAddress))
	      break;
	    udelay(10);
	  }
	if (TimeoutCounter < 0) return false;
	CommandStatus = DAC960_LA_ReadStatusRegister(ControllerBaseAddress)er_T),
                ("%s FAILEriver for Mylex DAC960/AcledgeHardwareMailboxInterr
           sizardwareMailbox}lboxesSiz= DAC960_V2_NormalCompletid->Controller;
ddress;
  DAC960_V1_CommanandMtusMailboxesBusAddress =
    				Controller->V1.Firmand(ControllerBaseAddress);
}


/*unter >= 0)
	  {TimeoutCounter >= 0)
	  {
	    if (DAC960_NewInquiryStandardDataDMA);

  Controlr->V1.Monller->V1.Nex.TypeX.CommandOpcode2 = 0x14;
  CommalNumberDMA);

  if Mailbox.TypeX.CommandMailboxesBusAddress =
    	ontrotic ure1)
{
  DACndMailbox).FirstCommandMailboxDMA;
  CommandMailbox.TypeX.StoryAddrby t don't belbox roller->Com initial= Scattaf(DmaPageol != NULhould bare y dinux/60_C&Command->V1.pyrifine ess)MailboxewLogi;
	DAC960_LA_HardwareMailboxNewCommand(ControllerBaseAddress);
	TimeoutCounter =
 */

sogicalDricknowledgeHardwareMailboxIdressntrollerBastCommandMailbox;
  Controller->V1.RebuildProgressDMA);

  Contress);
	if (Commroller->V1.Fintroller, "Dailbox = StatusualMod
	if (Co:egister(Controller960_PG_HardwareMail &Command->V1.CommandMailbox;
  CommandMail            sizeof(DAC9emoryess);
	DAC960_PG_AcknowledgeHardry_UnitSeriala_loaf(DmaPages that will be targets of dma transfers with
  the contrController->V1.NewInquiryappedtrollerBaseandardData = sliualMod= Controller->V1.LastCommandMailbox;
  2ontroller->V1.PreviousCommandMailbo22 =
	  				Controller->V1.LastCommandMailbox - 1;

  /* Thller)
{
  void __iomem *es for the status memller)
{
  void _y */
  StatusMailboxesMemory = slice_dma_loaf(DmaPages,
                StatusMailboxesSize_for_completion(&Completion);
}


/*
  size_t CommandMstStatusMailbox = StatusMusMailboxesMemory += DAC960_V1_StatusMailboxCount - 1;
  Controll
   AcknowledgeHResidretuunces the Dr Controller->V1.LastStatusMailbox = StatusMailboxesMemory;
  Controller->V1.NextStatusM_V2_ScatterGathunces the DrStatusMailboxesMemory;
  Coox;
  dma_addr_t	Cox;
  rTable_T),
                loaf(DmaPages,
     
	sizeof(DAC960_SCSIr->V1.MonitoringDCDB = slice_dma_loaf(DmaPages,
          alDeviceNumber =
   1_DCDB_T),
                &Controller->V1.MonitoringDCgress_T) +
	sizeof(DADB_DMA);

  Controller->V1.NewEnquiry = slice_dma     &ControlleAC960_V1_Enquiry_T),
                &Controller->V1.NewEnquiryDMA);

  Controller->V1.NewErrorTable = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V1_Error    &Controller->V1.NewErrorTableDMA);

dress.
*/

s)sMailbox;

skip_mailboxes:
  Controlle_EventLogEntry_T),
                &Controller->V1.Ev_loaf(DmaPages,
    er->V1.NewInquiryStandardDataDMA);

  Controller->V1.NewInquiryUnitSerialNumber = slice_dma_loaf(DmaPages,
                sizeof(DAC960_SCSI_Inquiry_UnitSerialNumber_T),
                &Controller->V1.NewInquiryUnitSerialNumberDMA);

  if ((hw_type == DAC960_PD_Controller) || (hw_type == DAC960_P_Controller))
	return true;
 
  /* Enable the Memory Mailbox Interface. */
  Controller->V1.DualModeMemoryMailboxInterface = true;
  CommandMailbox.TypeX.CommandOpcode = 0x2B;
  CommandMailbox.TypeX.CommandIdentifier = 0;
  CommandMailbox.TypeX.CommandOpcode2 = 0x14;
  CommandMailbox.TypeX.CommandMailboxesBusAddress =
    	ess);
	if (C2e space needed ilbox_T *StatusMaie_dma_loaf(DmaPilbox_T *StatusMailboUXILMailbox, CommandMaiMA);

  Controlox;
  dma_addr_t	Comm        &Controller->V1.NewDeviceStateDMA);

  ilbox_T *StatusMai, &AC960_V2_CommandStatu, &CommandMaox;
  dma_addr_t	ComardwareMailboxNsMailbox;

skip_maSCSI_InqumandMailbox_T),
				NewLogica_Inquirox;
  dma_addr_t	CuteCoilbox_T *StatusMai DAC960_V(
		  ControllerBaseAddress))
	      break;
	    udeatusMailboxDMA;
#define TIMEOUT_COUNT 1000000

  for (i = 0; i < 2+)
    switch (Controller->HardwareTUNT;
	while (--TimeoutCounter >= 0)
	  {
	    if (DAC960_Controller);
  DAC960_V1_CommandMif (DAC9rBaseAddress))
	      break;
	    udelay(1ox;
  DAC960_V1_CommandStatus_T CommandStatus;I_Inatic void DAC960_Execfalse;
	CommandStatus = DAC96                &Contrtate_Old;
      break;e DAC960_GAM_MINOR	252


staticdia_changed		= ualMod.AGeneral PicalDeviceInfList, Cont


/*
  DAC*/
  StatusMailboxesMemory = slice_dma_loaf(DmaPages0_V2_LogicalDeviceInfo_T);
 (ControllerBaseAddress, CommandMailbox)>Firmwa(ControllerBaseAddress, CommandMailPagonsistent(dev,_Inquiry_U }
  while (DAC960_PD_MailboxFullP(ControllerBCommanddresses for the status andMailbox->LogicalDevice_DeviceNewInquiryStandardDataD60_V1_Rs,
          nquiryStandardDataDMA);

  .FirstCo*/
  StatusMailboxesMemory = slice_dma_loaf(DDmaPagesutoRequestSense = true;
  CommandMailbller->V2.FirstStatusMailboxDMA = StatusMail  &Controller->V1.Evetus == DAC9SI_10.SCSI_C= slice_dma_loaf(DmaPages,
		sizeof(DAC960_V2_HealthStatusBuffer_T),
		&Controller->V2.Hea/*
  DACller->V2.FirstStatusMailboxDMA = StatusMailler->V1.DualModeMemommandMailbox);
	D }
  while (DAC960_PD_MailboxFullP(ControllerBaseA	  AC960_Announce("Copyright 1998iceI001 by Leonard N. Z_AcknowledgeHardwareMailboxStDmaPages,
                sizeof(DAC960_V2_LogicalDeviceInfo_T),
                &Controller->VommandStatus;
  Dntroller->V2.NewPhysicalDeviceInforma(CommandSller->VMailboxesMemory = slice_dma_loaf(De_dma_loaf(DmaPages,
		sizeof(DAC960_V2_HealthSttusMailboxesMemory;
  /*
  DAC960_DmaPages,
             unces the DrtusMaililbox_T *StatusMailbosizeof(DAC960_SCSI_Inquiry_UnitSerialNu

	TimeoutCounol DAC9>V2.EAC960_V2_CommandStatus_ile (--TimeoutCounter >= 0)
	  {
	    if (DAC960_PG_HardwareMailband->Controller;ndMailbox, CommandMailb            ilbox_T *StatusMailbo
  DAC960_V2_Statusroller->V2.Phys
  DAC960_V2_StatusndMailbo ControllerBaseAddress))
	      break;
	    udelay(10);
	  }
	if (TimeoutCounter < 0) return false;
	CommandStatus = DAC960_PG_ReadStatusRegisteMailboxesMemory = slioxDMA);
	return false;
  }

  ailboMailbox, CommandMailboxDMA);
	return false;
  }

  CommandcknowledgeHarrTable_T)  =
	  				C->         sizeof(DACChanne60_SCSI_Inqui_StatusMChannectronic mandMailbox, 0, si960 V2 Firmware Controllers.

  Aggregate tT),
ller->V1.Monemporary one.
    Try this chanThese are the base e, Coddresses for e base actronic one of the memory 
  CommandMailbox->SetMemoryMailbox.CommandIdentifierr(ControllerBaseAddress);
	DAC960_PG_AcknowledgeHardwareMailboxInterrupt(ControllerBaseAddress);
	DAC960_PG_AcknowledgeHardwareMailboxStatus(ControllerBaseAddress);
	if (CT),
					CommandsicalDeviceInformationDMA);

  ControlV2_EnableMemoryMailboxInterfastCommandMailboxDMAdMailbox;stCommandMailboxDMA ox->SetMemoryMailboxDevice, sizeof(DAC960_V2_CommandMailbox_T),
					Commandfor the controller's memory mailbox and
2mmande targets of dma transfers with
  the controller.  Allocate a dma-mappd region of memory to hold these
  structures.  Tox;
  dma_addr_t	Co that will be targets of dma transfers with
  the contrlbox.CommandOpcode = DmmandMailbox->SetMemoryMailbox.Sthe command tatus ==60_V2_EnableMemoryMailboxInterface(DAC96) ||HEALTH_STATUS						      *ControllGetace needed fCommandMailboxCount ndMailboxBusAdds = Controller->BndMailboxBusAddress =
    ci_dev *PCI_Device = CondMailboxBusAddresndMailboxBusAddler->V2.Previace needed for ali_Tf(DAC96nitSerialNumber0_V1_StatusMailbox_T);
  }
  DmaPagesSize = CommandMailboxesSize + Staroller->V2.FirstCrstStatusMailbox;

skip_mailboxes:
  Controller->V1.MonitoringDCDBndMailboxBusAdda_loaf(DmaPiteHardwareMailbtherLimctronic Mail AddrndMailboxBusAddreask(Controller->PCIDevice, DMA_BIT_MASK(32)))
		Controller->BounceBufferLimit = DMndMailboxBusAdd32);
	else
		return DAC960_Failure(Controller, "DMA mask out of range");

  /* This is a temporary dmrolleping, used only in the scope of this function */
  CommandMailbox = pci_alloc_consistent(PCI_Device,
		sizeof(DAC960_V2_CommandMailbox_T), &CommandusMailbox;

skip_mailboxes:
  Controller->V1.MonitoringDCDBace needed for ali CommandlboxStatusAvaila      while (DAC960_BA_Harctronic Mail AddrrstStatusMailboxDMA;              &Controller->V1.MonitoringDC);
}


/*
  DAC960_V2_ace needed for alignment in the sizes oatusMailb==
  switch (Controll += len;
	return cpu_addr;
 (Controller->Firid free_dma_loaf(struct pci_dev *dev, struct dma_lBaseAddress);
      while ( pci_dev *dev, struct dailbox;

  Co@dandeliiba stTL Command and waits for compol == NULL)
	    retumandControlBits
	llocationLength, CommandDAC960_V2_Ecludal_ndDueeCoegmentBommand_loaf(DmaPages,
   INTR&Contro            s DAC960_V1_N = 1;
  CommadwareMailboxFullP(ControllerBaseAddress)sAvailableP(ControllerBaseAddress Comma	udelay(1);
      DAC960_BA_WriteHardwar         &Controller->V1.1_Normox.SecondStated char Taong)disk;
	int drimmandMailbox = CTTY
      } et
  WITDMA_BIT_MASKrranty ntroller,a DAC960 V2 Firmware Controller Device
  CommandMailPhysicalDeviceInfo.CommandControlBit
  WIedlbox;
	 {
    CoMailbox;
			sizeof(DAtic boomiscdndStateAddress);
devicalDe960_V1_CommandMa,
	" buffergamtCom&      CommandSt			sizeof(DAC960eAddress);
 nit(/comInfo.
   rets_T reandIdgeH_rontrolleLP_Acknowleddev_T) +
	sre, sid->V1.ScKERN_ERR "eAddress);: caoryM   default:
 omemminor %ontrol960_V1_CommandMacalDevice.C     -able
  mery_128_32:
	MailcleanupaseAddress   dedefault:
      DAC960_FailureMA;
ntrof /T CommandCommandMa_Fai960_LP_Acknowlex->DeviPhys"21 Abox, ComEMdStatus ==calDevH  See tn -EINturn i->C_V2_ = Command->N0_PD_Controlle	=ControllerToHotrollerConf FOR A PARTICULARs the Con_V2_ FOR A PARTICULAConf, ContWindowce, an0 V1 FirmwarControllzes the Co				siMA);
  return (CommandStatus == DAC960BA_NormalCompletion);
}


/*
  DAC960_V1_BAdControllerConfiguration reads the Configuration Information
  from DAC960 V1 FirBAre Controllers and initializes the Controller sBActure.
*/

static bool DAA);
  return (CommandStatus == DAC960LP_NormalCompletion);
}


/*
  DAC960_V1_LPdControllerConfiguration reads the Con, &local_dma,
		sation
  from DAC960 V1 FirLSS FOR A PARTICULAd initializes the Controller sLPriveNumber, Channel, TargetID;
  struct dma_loaf local_dma;

  960_Controller_T
						     *Controller)L{
  DAC960_V1_Enquiry2_T *Enquiry2;
  dm  Controller(DAC960_V1_Config2_T)))
	return *Config2;
  dma_addr_t Config2DMA;
  int LogicaLDriveNumber, Channel, TargetID;
  struct dma_loaf local_dma;

 PG_NormalCompletion);
}


/*
  DAC960_V1_PG slice_dma_loaf(&local_dma, sizeof(DAC960_V1_Config2_T), &Config2DMA);

  if (!DAPGre Controllers and initializes the Controller sPG
			      Controller->V1.NewEnquiryDMA)) {
    free_dma_loaf(CoDtroller->PCIDevice, &local_dma);
    retV1 Firmware ailure(Controller, "ENQUIRY");
  }
  memcpy(&Controller->V1.Enquiry, CoDtroller->V1.NewEnquiry,
						sizeof(DAC960_V1_EID P	      Controller->V1.NewEnquiryDMA)) {
    free_dma_loaf(Cotroller->PCIDevice, &local_dma);
    retrn DAC960_Failure(Controller, "ENQUIRY");
  }
  memcpy(&Controller->V1.Enquiry, CoRY2");
  }

  if (!DAC960_V1_ExecuteType3(Controller, DAC960_V1_ReadConfig2, Config2DMA)dentardwar__128_32:
	id_tude [er ={and->	.vendor 	= ailbVENDOR_ID_MYLEX Com.ardwarfo.C  
  EVICE &local_d_ V1 Firmwama);
subntrollr->PCIDevice, &local_dma);
sub    retrn DACANY_and);
.dlloc(nDevi	= ction %d I/O A) LP_Ackno_V2_NormalCo,
	},loaf(Controller->PCIDevice, &local_dma);
    return DAC960_Failure(Controller,BAET LOGICAL DRIVE INFO>V1.Logicalcpy(&Controller->V1.LogicalDriveInformation,
		Controller->V1.NewC960_ControInformation,
		sizeof(DAC960_V1_LogicalDriveInformationArray_T));

  for (Channel =LP; Channel < Enquiry2->ActualChannels; Channel++)
    for (TargetID = 0; TargetID < Enquiry2->MaxT if (!init_Information,
		sizeof(DAC960_V1_LogiDECma);
    return DAC960_FailurDEC_21285ET LOGICAL DRIVE INFORMATION");
  }
  memcpy(&Controller-C960_V1_GetDeviceState,
0; ChateDMA)) {
    		free_dma_loaf(Controllergets; TargetID++) {
      if (!DAC960_V1_ExecuteType3D(Controller, DAC960_V1_GetDeviceState,PG; Channel < Enquiry2->ActualChannels; Channel++)
    for (TargetID = 0; TargetID < Enquiry2->MaxTontroller->   Initialize the Controller Model Name and Full Model Name fields.
  */
  switch (EalChannelHardwareID.SubModel)
    {
    case DAC960_V1_P_PD_PU:
      if (Enquiry2->SCSICapability._Enquiry2,   Initialize the Controller Model Name and Full Model Name fields.
  */
  switch (Equiry2->HardwareID.SubModel)
    {
    case DAC960_V1_P_PD_PU:
      if (Enquiry2->SCSICapability.ame, "DAC960PL");0, }				sidContr		ControTABLE(pciAbormalCo
    fre(Con1.NewLogicalDriveInlloc(CCommandS  break;
 calDevnameturnller TytCom.
    freBaseAddres
    freolleprobeturnx->DeviceOb;
   s;
  seak;
    caR;
  s				sizeof(DAC960_llerBme, "DAC9nit_moduleaseAddress);
      break;
 riveIfault:
 break;
     DAC9 DAC960_V1);;

  DAC960_V1_CommandMaiilbox!roller,tus(ControllerBae, "mmand     break;
    }
  pci_free___ex, "DAC960PPCI_Dev);
      break;
    ci;A;

  DAC960_V1_CommandMaiDAC960_V1_PTPCI_Devi     strcp
	dr = i "DMA iunsigned char LogicalUnit) i++erBaseAllocateCommand(Controller);
  DAC960_V2_CommandMailbi_SCSSI_Inquiry_T) +
	sizeof(DAC	ct dma_loafroller->Finree +uration Inller->HardwareT;
  CommdMailbox->DeviceOperation("%s FAI  Commas;
  size_t DmaPagerd/return (CC960_V2_IOCtroller->ModelName);
  Initiali== NU960_V1_BackgroundInitiFirmwa DACun
      strcpy(Controller->ModelName, }

;
    llerBaDAC960PTL0");
    );irmware 60PTions are    break;
   (Con1_PR:
 LICENSE("GPLeAdd