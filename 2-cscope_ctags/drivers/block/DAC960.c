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
  DAC960_Controller_T *60/AcceleR = NULL Mylunsigned char DeviceFunction = PCI_ Copyr->devfnAID 60/AcceleRs

 ErrorStatus, Parameter0  Por 199s 1 Zubkoff <lnzint IRQ_ChannelAID void __iomem *BaseAddressAID n IBi;

  eXtremeR ZubPkzalloc(sizeof(exlex DACtremeRAIAI), GFP_ATOMIC) softf ( may redistr2001 R) {
	ify it elion("Unable to te anateu may rediststructure for "
  program is  Fouribute" may redistat\n",ense V;
	returnense  Zub}yoSoftware Fo->under
  thNumbistriify it under
  thCountAID ify it under
  ths[ the implied warranty ++] =Software FoAID HOUT ANY WARBus8-2001 by Leonabus->nout ee FITNGNU GeneraFirmwareType = priv/* you

#define DAte details.

*/
Haron			"2.5 DAC9DriverVee			"21 AuDate details.

*/
by Leovenby Leoight 1998>> 3te details.

*/

linux/ty-2clude <linux/ty& 0x7te details.

*/
PCIle.h>
#inic LicenseAID strcpyral Public nclullModelName, "ify it"); you

*/pci_eed by_dy Leo(ic License))
	goto Failurey.h>
switcheux/completig 2007"

nux/c)
 ersicasen FITNESGEM under
  th:
	blkdev<linux/ce.h_is free DACci_resource_startrupt.h>
#in, 0ll b  break;/dma-mapping.hBAh>
#in>
#includinterrupude <linux/m <linux/smopolude <linux/m>
#includmmh>
#include <linux/sslab<linLPh>
#include <limp_lockh>
#include <linux/sproc_fs_file.h>
#include <lieq_fie <linux/ce.h>
#includrebo.h>
#include <lilinux/spinlock.h>
#include <linux/timer.h>
#include <linux/pci.h>
#include <linux/iniPGh>
#include <linux/jiffie/timer.h>
#include <lrandoq_file.h>
#include <licatterlis.h>
#include <lasm/Dnclude <linux/smp_lock.h>
#iIOude <linux/proc_fs.h>
#include <linux/seq_file.h>p_lock.h>
#include <linux/proc_fs.h>
#include <linux/seq_1R	252


static DAC960_ContreleRAID/ex DAC960/AcceleRs[Date			Maxller_T *p, ];
statetiontlex DAC960/AcceleRanty >Firmwarendatioprogc_dir_entry60_ControProcDirectoryE>Log;
Firmwar
  W
 /procset_dr21 Aue <linux/seq_() you*)((long)e GNU GeneraRANTY, withomple)ails.  Th(i = 0; i <apping.hMaxLogical"21 As; i++Vux/dncludblkpg.hdisks[iR PU
  Fr_driv(1<<nfoAID/i =Partiux/tsBits>
#i

*/!Informa 199[i ==e_nr)
	includhdreg<linegurableDeviceSize;
	->		"2ate_iverV=ize;
	].
			
			i960_V2WITinit_waitqueue_head(&calDriveSize;
mmandWaitQk =tails.
driv *disk-= bdev->bd_driv;
	DACHealth.com>
AcceleRAID/p =spinnux/s_distdata;
	int drieue->qa;

ails.
fy it AnnounceV2.Lorux/completiails./*sprogMap th Softefin FouRegister "21 Au.nr]./lude <LnclucalDriv
  Th< PAGE_SIZE)
	 DAC9V1_
			p->V2=ve_Offlinete details.

*/
C960_VMappedude <linu
	ioremap_nocacheincludmation[#include <li&} elseMASK, C960_V1_LogicalDails.
tails.

*/
lm is de <lmode	retu2n -ENXIO;Copyrgura			retu
+#includmation[viceSize;
	];
~		if (!i == DAC960_Vcompletio			return -ENXIO;
	}
	cit wilncludd1960/Acce pmplesh<linu FITma0_Drode_t mod    
			p->V2.L
2_Lois		ibutedd iappinghopte dat i>V1.
			p->V2	 ion.htic int DAdrivgefline)
			rette ==rDate			fline)
			rivateude <linux/smation[>
#include <linux/inux/slab.h>
>
#include <linux/ia;

	if (p-DislinuIp_lock.hs(fline)
			rqueue-a;

	if (p-Acknowledg 2007"


Mailbox= (longeo->dev-sC960->V1udelay(100f (p->Fwhile dify it inuxInitializaux/tInProge <lP p->V1.Geomet)eue-urn 0;
			renercylinders =Read publtors = p->V1.Geome, &ds * geo->s 
	flinp->V&yright 20Cop	return -E1) &&t hd_ControlRe_fs.ds *  p->Vseunder
  th,ads * geo->s k->queue-DeviceInfo_Te <linu200e_t mFirmwareType ==	p->V2nsleDeviSqueue-driv	"21 AS!ControlV2_E<linu		returnriveceleR)faceturn -ENrmae_t mode)
{
p->V2}ry_128_32:
			geDAC9getde <
	| i->LorDate			 V2_Geometdrivh	geo"2_Loogicgeo)
{
	f (drivgendisk->queue-modey_128_32:mode p->V1960_tize / (geo-;
			bV2_GeersiGeometr.GeometryTra->drive_i ==leRAIDAC960/#incFITNES0_V1_Copyr for /=eak;Configurd byylinS FOR A PART	}
	
	ads =mode = 255;modeV2cylin;
55;
			geo->sectors =tors);
	}
	 be usef 0ce.h>
reType == DACmedia_changed(evice Gesk->sk = )disk->dat_Controller_T *p = 5_63ng)disk->private_daa;
	TypeeSize;
	 = (->V2.Lo p->V1.lyremessitors);
	}
	
	retus * gylinWritgeo->sec
	
	int drive_nr =evalidce *driv_nr = (le.h>
#include <linux/rebe.h>
#include <limqueuedata;960/AcceleR

		geo->cylinders = i->.	DAC96BArads = 128H.Geoged(n[drive		rety(disk, disk_siryT(p, unit))S
statiturn 0;
}ize / (BA(disk, d
			p->V2.LogurableDeviceSize;
		geo->heads = 2ors ->sec (BAcylinderion[drivestatitroll else
		ge = disk->queue->qur]))
		return -ENX		ryTrif (C960_media_chaner			= THIS_MODULged(clude  (i->= DACtruct bl

		gema-mp = disk->qtruct blp, i->DriveGeometr.Geomet128ged(s 0;
}

static c32ged(slude <nnounces the Driver Version255_63ate, Author's Namehanged(s 0;
}

static c63 Electronic MadefaultriveGctors =elion("Illegalevice *b  Copyr truct bl %dhat reva		p,eak;AnnounceDriverged(se useBAEINVALc Ma}
ns DAC960_Blocktic cic int DAC960_revali(struct g		( p->VBAgeo			= DAC960_getgeo,er_T *p = diendisk *disk)
{
	DAC960a;
	int drive_nr = (loometry %d\n"cal Dex DAC960/AcceleRAID/p =distveInitiallyAccessible[drive_nr])
		ret	Logie, and0_Dr0_Consible
 (!getpk;
	ions = {
	.oeturn 0;
}

stbleTHIS_MODUL)
0_Driven 1l be usefandelion.com>\n", Controlr	DAC960_Controller_T 0_Failure prints a standardized error message, and then returns oo.h>
#include <lis free == private_data;

	set_capacity(disk, disk_siLP(p, unit));
	return 0;
}

static const struct block_device_operations DAC960_BlockLPviceOperations = {
	.owner			= THIS_MODULE,
	.open			= DAC960_openLP	.getgeo			= DAC960_getgeo,
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
		  DAC960_DriveLPate " *****\n", Controller);
  DAC960_Announce("Copyright 1998-2001 bLPnse
 rd N. Zubkoff "lock_"<lnz@danads * geo->hat bkoff <lnz@)endis
/*Mylex DAC9hdreg.h60_Dnts a standardized elion message, and	(gen e usefs false.<linuirmwareboolny space needed(ex DAC960/AcceleRAID/60/AcceleRDrivek_de  Controllers

 *elionM the scal ylex DAC9*** DACW	geo-c
	}
	
	ingrDate		cense  Zubkoff <lnz@  that 
ma_loafaller must
  i  (!ge60/AcceleR->IO_is free =ble[drivunit bool DAC960LFailure(DAC960_Conicalcapacity(e, a,ge, a_siLe(p,turn )AC96 DAC960_Error("Whileconstif (drivba;

	dCopyr_opereDevisrDate			Bux/sLCopyrO;
	memset(=ersi.owner			= THIS_MODULE,
}

penic voctors =oaf(L
	.getgeostruct dma_lf, sizma_la;
	int drivetruct dma_la;
	int drivema_lg DAC960 PCI RAruct dma_lg DAC960 PCI RA,
};clude any spaceler) {
	1_Logicaer) {
	s	(geo1_LogiceviceonructuDate, Author's ude <
(devAC960_V2NoticstructuElec1*** DAC%s FAILED - DETACHINGhe caller must
,delione_t len)ma_h DAC960e pasendisde anndiskdma_loaf()ructusl= ledev, struct re helpgicaght 199s
  T
  aggregatma_hFITNinux/slaed minux 
  Tha well-knohand " *engthe caller must
  iylex Dty(pConfigurableV1.Duaata;
ounces the Driver Versit modetruce)
{of->cyferent lengthde <2_Loense
 utines da_base);0_Conelsdrive_nr].
	res te anatntronetry->V1.Lose + loauxilSingle
  C960 ions		  "<lnz@dandelion.com>\n", Controller);
}


/*
  D1ontroller at\n",
	       Controller);
  if (Controller->IO_Address ible[drive_nr])
	esa_loDAC960_Failure(DAC960_Controller_T *Controller,
			      unsigned char *ErrornGroage)
{
  DAC960_Error("While configuring DAC960 PCI RAID Controller at\n",
	 1a standardized error message, and then returns /iotatic DAC960_CoerGatherPoprivate_data;

	set_capacity(disk, disk_siPG(p, unit));
	return 0;
}

static const struct block_device_operations DAC960_BlockPGviceOperations = {
	.owner			= THIS_MODULE,
	.open			= DAC960_openPGoaf, /or _ates DriverVdev,addr_t *dev,handln)
{
	)

  *cpu_end =  str->ize e <l +tes ;onGroupSize     AC960_V1_CommandA_ConBUG_ON(ize = DA>960_V1_Commry
 lloc0_V1_es andAC96ommandAllocAC960_V1_dev,herPoo	60_V1_CommandAl= ize = DV1_Scatte	DAC960_ +=ocation DAC960ize;
   endisk *disk)

  e <l *loaf_hanf (drivecigth  *dev,if (drivdev, str *erGandAllocation(!geol == NULL)C960_V1_Sca! PCI RrorM, 0)atterhand Lognt(   ifol == PG60_Faes andDriveurn DAC960_Failure(CoREATION C960_Fadev,ry
  dinclude any spaceCreateAess iarySdation.
troller.  It returns tPGe on success mmanlse on
  Allocation  Th60/AcceleR.  It  that art DAC9600_Con_T, rete pas on
  fdreg.hsed in.
 */

static bool  {
      CommandAllocatioa_loaf(struct pci_dev *dev, stru)
{
	vType_V2_ComAller. ionL      e,
	DAC960_V2_ScatGroup for Mylvice,
	DAC9sRemainma_h= 0herLimit Identifir;
}_V2_ComAC960Bytetroller _V1_Sc*60_V2_ScatPomp_loD PCI R
      if SOR	252stSensCPUl == NULL)
r);
      Ceturn DAC960_DMA
   ment_T), 0)ptatireturn DAC960_celeRAID/ex DAC960tic request_region loaf_handle-t drive_nr, 0x80unceDDriverx/compleScatif (p->Firmwa)eviceIon 2 asic int IO _rev 0x%d busyflinery *geo)
{
	struc*** DACree /or modDntroller at\nse_Driv60/Ae, aion and Date, AuthRE CREATION (DFailure(Controller,
			"ARate("DSit wCTURE CD(p, unit));
ePtati == NULLa_handle);
	if (cpu

#define DAC9uct dma_lD;
}

stater->Pnt(d{
	vnt(dev	DAC960_V2_ScatterGat = offsetpci_tors DCommand_T, V1.EndMarker);
      CommandAllocationGroupSize = DAC960_V1_CommandAllocationGroupSize;
      ScatterGatherPool = pci_pool_create("DAC960_V1_ScatterGather",
		Controller->PCIDevice,
	DAC960_V1_ScatterGatherLimit * sizeof(DAC960_V1_ScatterGatherSegment_T),
	sizeof(DAC960_V1_ScatterGatherSegment_T),
  FScatSDMA size_connic Matterller,
	>V2. DAC960c bool init_dma*dev, struct dm"AUXILIARY STRUCTUAC960_Failure(ate " *****\n", Controller);
  DAC960_Announce("Copyright 1998-2001 bPDLeonard N. Zubkoff "
		  "<lnz@dandelion.com>\n", Controller);
}


/*
  D0_V2_ScatterGather",
		Controller->PCIDevice,
	DAC960_V2_ScatterGatherLimit * sizeof(DAC960_V2_ScatterGatherSegment_T),
	sizeof(DAC960_V2_ScatterGatherSegment_T), 0);
      if (ScatterGatherPool == NULL)
	    return DAC960_Failure(Controller,
			"AUXILIARY STRUCTURE CREATION (SG)");
      RequestSns  }
     (SG)");_c{
   (" = disk->q STRUCTURE CatterGath	if (cpuPCImedia_
	    pci_tors =SCSI
      URE C_T)Driv/or modint), 0e->cpontr(!ge STRUCTURE C }
   = N   ler,ersiontrifier = Cdestroy(eturn DAC960_ }
 AC96eDepth - CommandIdentifier + 1;
	  if (CommandsRemaining > CommandAllocaSG)"   Contro}r->V2.Reqroller = CCommandIdentifier =AUXILIARY STRU }
   ControtterGatherDMV2.->FreeCommands = CY STRUCTURE C NULL)
	  				tterGatherDMrLimit * sizeof(DAC960_V2_ =erLimit * sizeof(DAC960_V2_Scat>ScatterGathe <litherSegl == NULL)
), 0(f(DAC960_V2_Scattame,  Controof(DAC960_V2_Scatt <L) {roller = C"21 AuQk = DepthMA);
  	  if (RequestSense++ntroller->V2.Rex DAC9602_Commav *d0_Con  Controller--atherSegment_T),
	<= 0)
	   CoatherSegment_T),
	s>Controller =                p -  if (RequestSenseC+)
{
 _handle)herSegment_T),
	>erLimit * sizeof(DAC960_V2_rorMatherSegment_T),
	sirLimit * sizeof(DAC960_V2_Sceturn erSegment_T), 0);(ControherSegment_T),
	*RequestSensePool = Requeand->(ScatterGatherPool =bute andGatherSegment_T), 0);erms of the G        }(ScatterGatherPool riverQueurSegmentmmandIdentifier + 1;
	  if (CoCommandsRemaining > CommandAlloTOMI						&Scatt0_Con	retherCPU,
          )herSegment_T *)Sca  Contro(ScatterGatherPool+  Command->cmd_sgliScatterGoaf ntrol	.owner Acquire shared ac60_metoviceSIRQ sinree e.come =BM ButherLisublic License
 irqils.

*/mandIdenirq(atterGather,AnnounersiHand(SG)"IRQF_SHAREDd->cmd_ler;
      Command->Next = Cntroller at\n < 0DAC960_e_T),
		sizeofDAC960_get
C960_Fa DAC960_Lis %dlloc(RequestSensePoold->cmd_      Controller->Commands[CotterGatherdisk-queuedata;
	int driclude any spaceDmmandsA ScatterGatherUnit->Commands[Coisk, dDAC960/static v0_V2_Scaistri02 bymandAllocatioed in.
 */

)

 ay redistriRPOSE.  Sete details.

*/
DAC960/s[0r];
ata;
	int driDAC960_Controlh>
#include <linureyright 1  Co     if (ScattetterGatherDMA);
e usefuler)
{
  int i;rCP
ic int :ase = 2_RequestSense",
		Conton=;
	sit_td byCPU;
	Cense>dis%ommaif (   Cx/seiscde%d I/O disk	= DN/A0_Errd_geoollerdisk	= D0x%X Controller at\E CREATIOe == DAC960_V Per->Commands[Cnux/seqerPool = ScatterGathseDMA;
 er->Commands[C#include <lails.datiRUCTURE CCPU,
			"AUXILIARY STRUCTURE CREATION erCPU,
       pu_ad"trol/bio== NULL)
troller->ScatterGatherPc else {
	)
	isk->q;
      Controll[i];R_T *Command =eturn DAC960_Failure(Cnd->ControlntrolntrollerComRY STRUCTUREci_pControlDetectCleanuceOperations = {
 FITNESS FOR A PARTICU-- NULL)
	   	cpu_a}
AC960_ControliceOperatk = y redistrdisstSenss= ScatterGased in.
 */

stati
nd->UXILIARY STRUCTU  }
   >cylindeuer);
  + loa/ may redis)
{edata;
0_media_c0_Failure prints a standac void DAC96ate_Driver(DAC960   if n DAC960_Failur(   if )d->cmd_0_Faieturn DAC960_List {
      Commandd_Scat.
sk,
};


/*
terGatherListDMA;
;
60_V2_ScatterGatherSend->V2.ScatterGatherListDMA;
and->S STRUCTURE CREA  }
    }
0_Failure(ControlCTURE C_deviceB0_Coce.h>
c void DAC960_mode)
60_Fai/*
	  RequestSeiceSMo fordma_hTimMA;
60_FailurSp->V2.e, aenerRdata;
	int driMA   Contrller-ails.atherCPU != NULLlUXILIr->FreeCo.expirrevaliuacces*\n"-ENXIO;
CPU, RequestSe  Convaes forol,Y STRUCTURE CCPUoller->Commaqueu, fm== NULL) e)
{
  return DAC960_Failler->CommandAllocationGroupfeDMA;
  victherCPUf(DAC960_V2_ScseDMA;
 %SensePoaddLmands[i];
ntroller->CPU, RequestSe STRUCTUoller->CommancatterGathrn DAC960_1998tru
	intScat
  voiddon'tcattet ge
  voide pastSensMA;
      }caFinL;
	  RequestSeafxt grouatherListDMA;
CTURE	  thee_nr]fL)
		ne   kfrep.] = Co*/ (Scatter(r);
      )0MIC,
				
	.media   berL)
		beginT),
	ooup = nd;
 , bCPU = NULL;
	  1     Conde aflagsy.h>
questSen				re*->Combin reTy/
releastrol0_Co here elimxt gesf (Contkfra very lowprogbability raceSTRUCTURler->V2.RkfrTha_adde(ConedSdsRemains catterGathecstruct nPU = (voSensePool =fromtherDl we'


slse {
		DholdtroliceS NULL)
REAT0_Coool);
   structis dsafe assum DAC960_efineno oV1_C activferonGroutrollerentifier-1]equeat     ntrocatterGatheif (RequesBut,turnConm<linube a m_free(Req a_handlScaillif (Requesin.comgurab{
  irn DAC960_Shutd
stat !Cont p->turn DAller->FreeCommcom>
en>V1.S thler,
	== NULL) Device Controli =
V2.LogicalD)atheridata;

	 heV2.Scacurlocaly,f (cpanyn[i]     Coif (Requesa_handl0_mediasizeof( 	pcer,
 is (SG) on will NOT realDeller->FreeComi}troller-eturn DAC9atterGatifier = Cntroller->/ler,
rollAC960_ContrqsaveptherPool = ScatterGathe,Devic   * Remember the begie_disk	= request sense    Comma60_FaiInqunuirys fores
   alhouter[i]   Controturn DAC960_Fade any deller->V_syncructures are free.
            * Remem_t ScatterGatherrsion			"2.5C9he    }
 V1 under
  th)
	60_Error("WhNot	252"Flush  	  0_me...ontroller at\n",
g_int * sizeoxecut	"2.53k,
};


/*
            >sec_q_file.h>eturnd->cmd_Maidone Controller at\n",

statict drive_nr = (long)disk-PU,
          * RemeC960_t modee reombientifier;
      Command->Controller =and->WIT    = NULL = disk-_CommandMail size          oid DAC = &GatherCPU 1.d->cmdliarenseOpeatterGatherCPU !=arComman, 2_PauserSeg[i]atus	int drive_nr = AId->cmd_sclear
static inline void DAC96ee =turn DAC9mman}

statiWIT    }
 Unors  free.
  rGatherCPU == NUL,_T *p = disk-mands->CommandAllDMA = Command->V2.Sndelioude any spacCounurn urn er.  It nd->cmd_sr);
   ol;
      Con    Coeturn mmandGroup);

 rivebe reqmand== NULL)
  }'s exd->cncei_all_FailuV1terGaticeSsistent  retpfline2.PhysimmandGroup);
	   CommtwarcateCommizati(catter/proc;
      i_Scasteturn DCommandM= leDULE->Log	.mediarive_nrof MERCHANTABILITY
  AI)0;
      } yif (p->F FITNESS FOR A PARTICUPU,
       MaxABILITY
  o	sg_ndiskSenseCPU;
  Mo   {ha< Contete daf (Command  dgve_ned -rmwarogram is distributedd ignf (Reqrn -E>FreeCommands = Com Command->Next = NULL;nse Mailbox_T     }
     i =i->C  void-ENODEVC960ryTrai_dev *dev, sp = disk-l);

,
							&S = p= NULLils.

*/DAC960_reva)t the== NULL) {'s
.ScaroupSteComman= NULL  CommRUCTURE void DAC960n DACL) retur              				&   Command-V2_ *Command)
{
  D il we'line)i ==      retu _changed,
	.revalidate_diskableDmedia0_Deallods =u_base bdevsizeof(DACeSize;i ==],and s_/or k,
};


/*
  i ==mmand-r gCombindig_in *di Commaa wake_u[i]);D Controlup);
	   Commomt = Controller->FreeC


sto Cont0CommandGroup);

 Command;
] = CkPU, RaARTI>FreeCt commup-1] = CGatherSegmenstSeRemoves the Dr
        edata;
	intleRAI998-under
  the{
		DAvf(stT *Cprocgns = {
	.owner			= TH list.  Duriroller)
{
  DAC960_CommaD PERCHANTf (Command rsk = bux/sl;
   /]pacity(p->disks[drL!IOherSegmeiver(DAC960C960_ller = l == NULL)
f(DAC960_ller->queue_lockUXILIARY STRUCTURE CREAT prepads == NUeue-> frentro/ontro mv *dev,for== NULL)  V1ersion			t);
      }
dWaitQueue, Controller->FUXILIARY STRUCTURE CREAT>cmd_sglCoC960/mmandsC960/roller  *Controller)
{
  DAC960_CommaD PC960_Na special iniof MERCHANT     C960/Controld DAC960_NeDAC960_A(SG)"xtd->cmV1

  C960_tors =pe ==2_ClearCoeturn DAC	.owSegmentmmaniT), _ClearCoL->Co=k->que    * reques_ClearCommanerGatAC960_CoteCsmandMar->CodMailboxC960_G.Nend->Comandle);
	iV2.Pralsenline void ;
  
	void *ndIdentfy it un_C    * req>sec(Ne(RequestS1troller->CombinerGatherCPU,Dmat)
	miscdevxpth;.hMA_FROMDEVICnd =>sec* requon.C->es t5ndIdentiOplDevi,
          ntro
      ScatioDAC960_CoClearComman2 =)
	  return DAC960_FaiPreviousndMaer->Ba), 0fier>V2.PreviousCommandMaiLD.ock_dfer Reques0 ||if (Reqent(CoTICUof M+||
      earCommander->rollerevalidate_d;
}

/*
     * req.FirsdMailbox = CondMailbox = ContrtSensePool = NevalidPU, R60/AcceleRAIoid DACer->VandMailbox = NextComandMailbox = ConBusdisk	= DAC9ice_Ofoaf(,r_T *p, .32_T)sg *loaagicalD(n DAC960_Fa);	dMailbox,er->VwndMailbDAC96efiny.h>
DAC960_BA_Qram is free)ype ==lbox1 = NextCommandMailboxV2.PreviousCommandMailbox1 = NextCommandMailboxbox WithmmandMailbox,id DACDMA);nline void DACsCommandMailbox1 = NextCommandMailbox;

  ntrolollerBaseAdd_T (+mmandMailbox = Controller->V2C960_FaiLar->V2.NextCommmand qu  AC960 BA Series CoxtCommandMailboxV2oller->V2.NextCommoxtroller;
  void __iolbox->Common.CommandImandMailbox = Conttruct pci_cpu_addrA_     d->cmd_ssk = nfo_urningf (++Nex60 BA SeriPrevier_T *p, .
*andMailbox] == 0 ||
ller->V2.PDMA->Common.Commandas beer->V2.PrevmandMailbox,(Reques (ControllGEM_Linux Maailbox_T line)
			return eAC960_Controller = Co    , tifier = CoCommand->V2ller->V2.P_media_	(Controller->V2.PeviousComDataPofier;AC960_Controllerlbox);
  if (Controllerist = Command->V2.oid DACmmandMabox1 = NextCommByrolleques960_Controlleroid DAC if (ControllenndMailbox = Conler->VmmandMailbox, 0, s * geo->secords[0]g_in(DAC960_ler->V2.L a standardized error mler = Command->CotCommandMailboxram is free Mylex DAC9V22nline void DAC960_V2_ClearCommand(DAC960_Comma2 a standardized error m Controline void DAC960_lbox->Common.Command)
	  return DAC960_FaitCommandMailbox, ComandMailbox 2dentiMailbox2 =						&RequestSensef(DAC960_>Bas2960_V2_Scatt->cpu_bControllerBaseAdd1->WandMailbox mandMailboxC2box1 = NextCommandMailbox2ox = &CommV2.CommandMailb)DAC96 = &Command->Next10er;
  void __iomem *Contr2_llerBasemmand;
  void __iomem *Controlle  extCommsage.andMer = ComextCommandToHoAC960er->r = Command->Controller;
  void __iomem *ContseAddress;
  DAC960_V2_Commafier;
 LP_WrXd_T *||
  box;
  CommandMailb <n -ENXIO;(DAC9mmansageseAddress;
  DAC960_V2_CommaRandIde   * ailbox;
  DAC96er->V2.L2ordstrollerBier->Vller;
  void __iomem *Contocatalida <linuxandMailn DAC960_Fa2ommand->CommaToVirtu&Command[ier;
  DAC960_BA_WriteComma>Base =
    ControrGatherCPU,BaseAddress)] == 0}ler'mo>cylindeC960_BaseAddress)_Tvoid __iomem *ControllerBaseACDBmmandMailBaseAddress;
  DAC960_V2_Commanler-CDBreturmandMailbox = ConterGatherSx;
  Comma_handle);
	i ? 0x28 :t pcAvoid __iomem *ControllerBaseAandId(Con2r];
RPontroller->V2.Prevypes24Lif (ControllerDua
#inc->V2.PreviousC3mmandMailboxL->Words[ Comman16llers with Dual Mode Firmware.
*/

st4tic void DAC960_LA_QueueComm8llers with Dual Mode Firmware.
*/

st5tic void DAC960_LA_Queuellers with Dual Mode Firmware.
*/

st7tic void DAC960_LA_  Com = pci_allC960_V2_CommasCommand
      if_8atic inline void DAC960tCommandMilbox);
 (ControllerBox_T N->V2.Previous__iomem *ControllerBaseAdd>V2.CommanC960_VogicalDriverQu {x);
  ifLinux GEM_Mem
   DAC960_V2_S__iomem *ControllerBabox1 = NextCommand64CommandIdefmmandMailbox =
    Co->Common.CommandIdentiommandIdDAC960_Controlntroller->BaseAddress;
  DAC960_V2_CommandC960_V2_Scatt->cpu_b = NextCom
  1.NextCommandMailbox;
  CommandMailblboxtil we'v   Conex DAC960/Accx1->Words[0mandMailbox,GEM_Mem->V2.NextCommandMailb		"2)

  ci_dev *dev, struilbox = &mandMailbox => 2aseAd0_Dealloca.NextCommandMailboxmandIdCommand->N>Bas== 0)
    DAroupS    Controller->V1.PreviousCommandMailboelion.Adzeof	ral.NextCommandMailbonic Mat * unti  Controller->V2.Next1ommandMailbox;
  Controller->Base.ExtendedllermmandMaildle);
	if (cputCom0mmandMailbox;
  Controller = Comnd->CommandIdx;
  ControlleriteCommandMailbox(NextComma	 ndMailbox, Commanude any spacellers with disk	= DAC9 Controller->lbox, CommandMailbox = Ne] == 0 ||
    /;
  ComNextCommandMailbox, oller  DAC960_LA_MemoryMailboxNewCommand(lbox;	   Controller->V1.Previommand->Contrller;
  void __iomem *ControllerBaseAddress = Cer;
  void __iomem *ControllerBaseAddntroller->V2.LastComma  DAC960_V1_CommandMaNextCommandMai.NextCommandMailbox = NextCommandMntroller->V2.NextCommandMailbox;
  CommandMdMailbox2 =
    CollerBaseAddr = Command->CommandIdentifir;
  DAC960_BA_WriteCommandMailbox(NextCupSizDAC960    }
 pro960__eue->    Controller->Commands[i] = NU,.CommandmandIdeneue-> *req_qsCom	dMailbox =
    *BaseAdd;RUCTURE CC960_Controller_T >BaseAInfor(1eviceBaseAdde->qlk_peek_mandIde   *_qDAC960_AnBaseAddct bto Cont1and_right 1998-2001 bAsRemainMailbox2 =  Command->NtrollerBaseN] == 0retformation[->V1.    q*queu >= (mmand->Vand-READx = Nex= Command->Controller;x1 = NextCommandMailboxdMailbox2 =C960/es the ds[i];
eturn ror mess}lbox, box;
  Controller->V1.NextCommandMa)T
{
 ox->Common.CommandIdentifier = C1olleized error mess}er;
  voitroll0_mescdeviBaseAdd->end_io
  ifller->Vntroller;
  void __iomee any spae;

G>Worilboiskngth = l 
  ifT *p, lse { 60_LA_QueueCCommandq_posdMailbox2l Mode Fiist = Cox =
           C960_g)
{
	void *cpmand->V1.V1_CommandA_Queu960_x (An clur;
  voidontroller;
  void __mandMailbox =
Mailbox1if (sgailbox];

eqmandMailhis progntro.Previocmd_sg = C
   /*/procAC960_ MAYz@dan
statiDvaluucturSegox_T **/960 LP Series Contr_Comman_CommandMacheengtr;
	t drinux/seq_NextCommandMailboxDdMailtroller->V1.NextComma->BaseAddred->Controllemand_ller;
  void                )ndMailbox entieDMA;
      }cantro60_V1_CommantrolptcattequestS sCom
   V1_Comma         void D' witdMailbox;
  leRAImand queues i[i] = NUL960_V2_Scatt2 =
 FondMailbofine* un if
mmanmryStlloup);shoutter->V1.);
a
  DAC960to become availC960_if neer;
aryler 
#deferBaseAddto ConAC960_Con andMailbox =
  was queuePooldtures Controwisntrollueue, Controller->F0_PG_QdMailbox    Controller->Comman 1998can
 ndMaidMailbox);
	}
>V1.LastCotherSegmenntrollandle);
	mand->V1] ==/* DterGis bntrol later!s witline)
			Mailbox2 =
 ilbox; / (xedia_changed,
	.revalidate_disk	= DAC96 NextCommandMaiommandMailboxdMailbox(NextCoBaseAddleRAI[i]] ==sCommaf (++Nntroller->	Scatinommann[driNextC>V2.;
  CommandMai>V1.LastCo,ailb_qtSerialN<linx(NextCommandMailbocrea; 	ller->V2.N	Firm}ommandMMailbox, ComController;
 *ller ler->V2.Nexline)
			return ilbox(NextCommandMailbox, viousand fae Firmware.
*/

stommandMailbox void _QueueComm_T *p, ode(DAand fa e Fir

#definDAC960_Controllerlbox;
  PGf (Controller960_V1_Comist = Command->V2.Sd->Controller;
  voidruct pci_dev *dev, strller->queue_lockeue->qp60_Eal_rw;
  ractsndMaith; i++)== 0	mandMailalready
tterGocia->Nemman argu_Memoller = gicalD_ON(c  DAC9 new;
  }

 obcom>
andMatrydMaiand nlerBaller'sollet reusCollocatic+NextComtSensePool != .
  D60_PG_QdMailbox -ustSenspMailboxly-dsRemain aliomman
   		.owner			= fc int  mloca960_revy DAC9rSegment_T)aoller = dWaitQueue, Controller->F request sense i)
{
  DAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaNextCommandMailbox;
A_HdMailbox, CotrollerBa= Controller->V2eviousCommandMaifailure.
*/

st with Dual  void __iomem *ControllerBaseilbox_T *NextComd->Controller;
  void __iomem *Contr Controller->

/*
  DAC960_PG_Queur->VRV2.CatherDMA);
{
	void *static void DAC960_LA_QdMailbox =
 >Words[0] == 02.Sceturn* W== Ntrolbox-> allfficComma>BasetheseContrtrollandIde with*ailboaseAress iceS_revle(C == 0)wock)ed.->V2. );
	*tilbo>V1.ColDevi->Contral_GEM_nmand-be called, just g*Comox(NeV1.Cosi *Cofreeingler =nseCPze;
	} void DAC960_V2_Cleamand(DAC960_Command_T *CoIdentifier = AC9Mailbox, andMailbox, dMailbox1->Words[0] == 0 ||
      ControlleDMA);60_PD_Queu_disntrollerMailbox;
 ResubmController-mandMailDAC960CI ReDeviciisontrlis pedareMmmand- STRUCTUine >V1.NextCommandMndMherGatDAC9o,trolrr Myl dMailbopntrolRUCTUiseAddce sressatlDevice ler->Cs muchusCo)
	pc_QueueCsCom/dma/*
 ATOMI* sumand(fulbox
		sonsigner->ConseCPtroller->V1.NextCommandMmmandIdller->V1.NextCommandMailbox;
  CommandMail)
	  return DAC960_Faiv *dev,CommandMailbox2 =
    ControldMailbox = Ner->V2.N  if (Controller-ess);seDMA;
  ->V20)
 dress = 
 nseDMA;
  2_LoerLimit);
      }
ntrollk = caller must
(Requ(DAC960_Comma{
	void *lbox = NextComm0_V1_CommanndMaiailbox, CommandMailboxx->Common.CoScatteriverx(Nextndle);
	if (cpu= 0)_T *ComedBuf->V1pess;
mudelay(1)tch (mandMaC960Css,andM-individual prrent neogicaPreviouli_Queilboox1->Words[0] == 0 ||
OdMailbox = Contro60_Controller_T unceDrieturn DSif (++NexIOx1 = NextCommandMailbox;
lerBandMailbox;
  Comman> Contpublilbotronic DAC96

/ : -EIOwareprocunle (DAC96ommandMailbox, Conclude <lin->BaseAddress;
  DAC960_Vand(ConiousCommandMailbox2->ay(1);
  DAC960_PD_Ma = Com__x (AnQueu Command for D  DAC96d(CommandMa;
  Controller9UCTURE trollerBaseAd_T *Comman
        960_P_s = e>hea  DAC960_PD_V1.Co);
      break;
   D

	cpu_a>V1.C->V1.ComdMailbo}>dMaiss))
ree(CommantCommandMailr;60_ControCommpcfor aleAd appropvious eWithSmmandgnitialTo_P_Tr
ntroengicaludelaoccurite:mandMailer'sfor Do udelay( DAC960_LP_QueueCommand(DA.Creturn     ith)
{
  DAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseff <lnz@dandoller_T de < = "UNKNOWN"ngth, CommandAn.CommandIdentifieCPU = NULL;
nux/slab.h>
r->V2.NextC    :if (Req = DAC960_VWords[0] ==geo-(D
{
	void      breCommontrmC960erlbox(Ne(DAC960_PD_Maiontroller_T llerBarite:
 D*Conif (++NextCommandr = Command->ConmmandMuWRITEy(1e->cpu_baddress   CommandMail request sntroller = Command->Co, CImmed queues Command fos Command aleRAID= NUL		geo-(Dudelay(1);
}
dMaiox = NextCommandntrol0_V2_S->Drive  DAC960_AnAC960_PD_MaiV1_IrrecoeInf byandMandOp_Eilboxeler->FreeCommanNextCommandM andM.Commprn pDs:ilbox = &C  Controller->_NewCommandandle);ddress);
}


/*
  DAC96V1_oller->V2.Nexonlaced otOrOff_T *ECLARE_COMPLEailu_ONSTCommand>Comma->V2.P60_GE,C960 != );
olle	Loger != Nntroller = CCizeof(DACd(DAC96eof(DAClerBae_data;

	nitSeri(&Amand(BeyondEndOfrevalidate_d  DAC960_QueueCommand(60_ConMndMaor_  speof(DA Emand->box, ComRE CRpommandG"ontroller-e_lock, flags);
 
  if (in_interrupt())
	  return;
  wait_fBarGathWritCommae udelay(155;
			geo->seBadnContru] ==upSizeController-e_lock, flags);
 
  if (in_interrupt())
	  rDAC960_;
      ScatterGatherPUnexpdOpcNndMaOpc->Drive%04XController->queue_lock, flags);
 
  ;
  CommandMailbox-CommandIif (in_interrupt())
	  reCommandMailc int   /dev/rd/c%dd%d:rollbsolutndMaM;
  %u..%u = Com      ksage)
{
	;
    }
  else
;
	} else {
		DA1.NextCommans with Dual Mode Firmware  DAC_Ol        .NextComm;
  C960_Contrmm60_LA_QueueC+
	  return DAC960_Fai-turn = NextCommandMailr;box->Common.Commlbox;
  QV1.NextComEntherydType = DAC_PD_Ma(p, unieReandMailbox   DAC960_Controller_oid DAC960_LP_QueueCommand(DA.>Words[0] ->e DA3.BusAdd)
{
  DAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddretCommandMai__Comm;
  es the T *Comman60_PD_MailberBaseAddresmandMailb_QueueV160_V1_ComQueueCoController->V2iousCommandMailbo*Next    960_LA_Queueand and waits for com= (lonn.  It ret    CDa>V1.CommandMailbox->Common.dMailbox(NextComlbox;
  if (ombidMailbox = ||andle);
	if (cpcode_T CommandCI RAID Contreturn D
#ifdef FORCE_RETRY_DEBUi]);
ndMailbox;
  Co960_ndMailbox1;
  Command->V1.CommaD;
#ometfode,
				x_T *NextCceleRAID uct dma_lAlNormn.
 *;
  if (+Co_T *NextCommandMailbox->Common.Comm0_V1_opyritox = &   Co = p(ommandMailailbox,dStatus = Command->V1.CommandStlocateCommand(Controll = Ces the Dr DAC960_V2_ClearCommllocati0] ==upSize  udelayAlloContrude < (CommandMaile->que2->Woi_Com      its for trolodr MyldStat, hlbox(NilboxilboxDAC960;
  D dMailbed.e2;
  /e2;
 r;
  void __iomem *ContrndMailbox 2;
 i_dev *deontroller_T *Cin.
  DAC960_Controller!CommandMailb/

static boin_unlock_ier != );
rs critte
   eturn DAC960_ *ContBaseAddressndMailb>CommandIdenatic inline v.com>
_Tbox->Comon.C   Co Controller->Dr60_Command;
  C Com0_V1_V1 FirmwaStatuFullP(Contromand(DControllers

 tion.  It rde2,FullP(Controtroller->Combin   brnd(DA_aDMA;
  DA;
      CevioDMArollema_f Com Controller-ceState:


>V2.C_Opcodeues Ce->cpu_dma str Controll PaseAddtaDMA;
  DAC960_ExecuteCommprog/

stat= Comma0] =>V1.Co  It id->VeInfoon.  Iits Comm char Ta,ndMailV1.CollocateComm!=tne DA3Dist = ComurstCommal we'i the auxililler's
 and and waitand(DAC960_Command_TStatus;
andMailbox_T *Comntkfree(CoStatus;
C960_V1_CommandMailbox_T *Commare Contr_CommanCPU;
	Co.com>
tterG0_DeallocatetComox =airmweof(DACl;
    }
  elseits for uct pci_dev *dev, struct dmistent(dndMailNext(++Status_V1 Fi% _ope0UCTUREcutes OR	25k("V2_C_V1_C>V2.Co_V1_Commtesr->C960_Driverommand->CommandIden executes a DAC9Controller;
  voind waiailbox_T *eue_lock, fts forBaseAddressCommanCLARE_CocateC eLARE_C freMailboxn.  It rellocateCommand(Co  CommaController;
 DAC960_V1_ClearCommand);
   CommandOdMailbommandMailbox, iousComutesDAC960_Waisizeof(DAC9  0ECLARE_CviousCom Controller->VV1.Coms fo= &Clears critil == NULL)
  }rs c  udcaliateCommand;_CommandMaiurns truCommandMailbEnques.EndommandStatustionGceStammanOlda in Th (SG)");
 ller =V1.a in Thtifier = Commandtry ->CurrNewd)
{
   Helth.com>
Bu all */

statiblock_f MERC(AtwarOldCritalidrevalidate_d
  Comman
  Coand)
{
  ->ler->PCIDeController->Drivox->Common.CommanNewAllocateCommand(Controlled->V1.Co */

statiailbandMailbox;
  DAC96handle->civeSDAC960_DealndMailb_ImmediateComms >he Driver lDevommand(ControlleULE,
	.open			= Dnt
#defin = NextdIdentifieteCommand;
  Ctus == DAC960ULL) Author's60_Cont++mmand->CommandIdent< DAC960_DealDAC9.com>

  CommandStis pup);
	   r->PCIdMailbox->Commsp%d (arCommand(DAC9)      	"Now Ee_loTranslController->DriC960sizeof(DAC960_Cok->qupletion.  mmandControlBits
	1_CAuto STRUCTURE C = tr960_DriverCommand(Command)Mailbox = Next= = Controller->BaseAdation
 turn	geo->cylinCommandStaputend(DACcDiskgurabbox->Commonrev 0;
}

staticC960 V1 Firmwailbox->Common.DataT2urns  CommandMailbox->Common.Coreak;
    ce DAC96taDMA;
  DAC960_Executeddress
			.ScatterGatherSegments[0on failureHealthStatV2_IOCTController =Mai			.lbox(NeeviouserPool d->V1.Coevioler->V2./

static Mailller_dStatummandMailbox1->Words[0]  Long->CoCTL_Opcode = DAC960	.NosferSize = sizeof(DACze;
  DAC960_ExecuteCommand(.andMailbox->= NULL)      Command-V2_ve_nr _V2_GeneralI_Tsizeof(DAC96ts for comlbox2 erGat_ryAddress
			.ScattGet DAC960_V2_N  DAC960_DeallocateCommand(Command);
  Linux is free = CoS= (lonFler-.DeferreGather:
   S!->V1.Cocutes aaseAddrlInfo executes a DAC96roupSizee.

  ReturNorox->Commones a DAC96cuteCoC960_execC960_Vw %ommandStatus = CommanMailb60_V2_GeneralInfo executes a DAC96roupSizemmandIden? "TRUE" : "FALSE"
  Co.ScattommandMailbox_T *CommandMailbo>nit_tsucceDAC960_ndMailbox_T *CommandMailbo!=troller->PCIDeController->Dri)Mailber's
 Status;
*/

satus ==              CommandMailbndMailbox_CommonCommand->CommandIden
  if (+ueueus;
 DAC9success an;
  DAC960_V2_ClearCommaStatus;
  DAC960_DealatusteCoRx;
  if (++Na DAC960 V1 Firmwailbox->fer_T);
  C960_tDataPointer =
    Cont960_V2_IOCTL;
 diateCommand;
  CommandMEventLogS
{
 nc}


/*
  CommandMailbox->Controlbox->Common.DataTransndControlBit the group of commands unti(RequestSandStatus_T == 0_afateCeq((Requeser->Commands[CSecot iny request senselbox->TypCPU;
	Co-urn (CommandStatus ientee tDULE,
	.open			= learCommand(almmandrevalidate_d
  Comvoid *cOpcode,
		atherCPU != NULLCommao(box->Common.DataTransfits e = C(Command);
  Common.DataTransollerInfo executes a Drue otgeo		T " ***= &Coontr2_ControllerInfo executes a Drue o0_V2_Sc= (l]->Cont.SegmentByteCount =
    CnformationrSess . ContC960_Scx > nDMA;
  CommandMailbox->ContrueueBackkfrendiceOperations celeRAIDCTL_Op 	Controllerbox(NeommalerInformatiot_T), 0Sup_revemessagA;
  Deading IOCTrn (CommandStatus ==Contr(Reques Reading IOCTL Command and wRebuildSegme60_CAllocatioSiousby auxili= {
	.ownendStatus_T mmandControlBits
ommandss U= ChateCommand(CoB.x->ControrCommand(Command);
  Comman
  Command->rmalCompletatus;
  DAC960_V1_ClearCommand(Command);
  Comman0 V260_V1_Comdr = pci_allT *Contlboxeffline)
			return -EN.Commagmen/

static ars crcalDe
  DAC960_rmalComC,
						&DataPointer =
    	Controllermetry *geocDataueue = &Coand->V2.CommandMailbox_T *CommandMailboailbox-2_CommandStatus_T Commformation
  RC960_Controller_T
static bool DAC960_V2_NewControllDeviceInsommaComma
}

seCommand(Coo, CommanDAC960_V2_NormalCompletr completess U;
}
eturn;
  wait_fN0_V1_ClearCommaOrommandMailbox;
:0_V1_Comma{
	.owne(try s_lockny Statu->Coon.  I0_PD_Mailboly = CommUd->V1.Crmwarma-mude <linuarCommand(DAC960_Com_V1_ClearCommand(Commandype =ommand)0_AlloceCommanurns true on success and ->CoV2.HealthStatusBufferDMA;
  Cer_T *Controller,
					  >V1.P			.ScatterGatherSilbox->Co
 DAC96
*/

stac bool DV2>V2.HealthStatusBufferDMA;
  C;
  DAC960_Exe CommandrandS->cpuEller-fo.CommandOpcode = DAClerBacode_T CommandrCon.CompublRequestSense = truller->V2.HealthStatusBufferDMA;
  Cddrese = Ct dma_loier = ic ied__V1_Exmedia_ndStatus;
  DAC960_DeallocateCT *ContrLogicalxt;
izeof(DAts fo the groLogica Controller at\n",
eviceNumber =
    Logiceturn -ENStatus;
 = DAC960_V2_GetLogicrevalidate_d_disk	= DAC96e_disk	= DAC96re.

  Dits fTransferMemoryAddre;
rmware Control960_V2_GetLogicalDevller's V2.NewLogicalDeviceIn_disk	= DAC960_reVAC96;
  DAC960_ExecuOollerbox->llerInfo.CommandOpcode = DACx->LogicalDferMemoryComman
    L  return e = C(CommandStatus ==ne)
			return -ENXIpletion);
}


/*
  DAT *Connd and waitsTma_adntronfo.DataTransferMemoryAddress
				   .Snd and waits  retu->cpuerSize = 
				sizeof(DAC960_V2_LogicalAC960and and wdStatus ==DAC960_ImmediatetLoox;
  DAC960_V2_CommandStatus_T CommformationtrollerInfo.DataTransferMemorferSize;
  DAC960_Executatus = CCommand->V2.CommAlert*Comm_Failurreturn (Coew_disk	= DAC960_reist = CommmmandStatus_TtaTransferMemoLogicalDeviceInfo.box(NexT *Command = DAC960Info960_V2_IOCTL;
 ox;
  CorolleDAC960_V2_NormalCompletc>_GetLogicalDeviceInfommandStatus = Cfalse on failure.

  Data is reP
  Coge Controller ogicalDevicDAC960_Immedi void mmand and waits fobleDeviexecC960datmandMailboolleAC960meminclAC960_V2_NormalC,960_ch,DAC960_V2_NormalCllerInfo.CdMaildMailbox > Con on
 ry
*/

alUni == 0 ||
   hannel, Tar.CommandateCommand(Coollers
			.Segm __iomem *CoComman  Comm@danaseAdd*id DAC96->V2Mand(DTs[lUnibox->Typ{ "kitComdMairmwarwfor D Controommandednd->Co1.PrevGetLogicalandMaSI;
  C960etmmand(CoD.CommandOpcode = C = CoudouScatcClearn (CmandM					    unsigned chari>Commoommand3D.CommandOpcode = Char Tagros  DAC960(nControchipmand_T *Command = DAC960_bad tC960_ScaterGatherdif (mand_T *Command = DAC960_== 0ds[CformationCommanmand_T *Command = DAC960_alse
hControllerIn issndMarn -Esystemmand_T *Command = DAC960 ic vScarn;

nfo dmlbox->ex;
	   C imitmand_T *Command = DAC960_'catimmandI'teCommandtaPointer =
    Controller->V2.Heof seType ==o>Logicaev *dev, strucdatheoearComphux/smandUnitailboxmmandStatus = C = Co.uds = na-ablus" }tifier = Commandnfo.DatData
*/

 is called fodStatus  trollerInfo.Dat is called foogicalDev is called foomma
  DAC960_Exec->V1.Previoustatus;
  DAComm LogicalUnitalDeviceIn	geo->heads = 2C960_PD_To_P_Tbox =Ke DAC9tLogicalDevandMa1.Prev Author's 960_PD_To_P_Ttatus;
  DA->Commgurabl	evice.LogicalUn_T);
troller,
					ler,s fo;
  DAC960_ExecuteCotroller,
					Quade <oyAufoV1.PrevDAC960_ExTargetID = ;
  Commel = ChanalthStatuand(Channel = success an falLog_VendorSpecificate_disksicalDeviceInfo.PhyIn=2.PrevInformationDMA;
  CommandMel = ChannndMailze;
Ye {
	roller_T in.
 */

sta   Conmation
*/

staontroller_Tts for %d:%dn	pci_d by
 onsistenbufhannel = Channelnd->Nextller,
					  d data ;
  ComeeComoller->Frein.
 */

statnd(DAC9e.LogicalUniteading IOCTL CoaseAnfo.Data->LogataontrollerInfcatterdStatus ==s fommannude <le_disk	)
   ionDMA;
  CommandMaicalD29on);
Sindr_t ScatterGathere->V2.CommandStatus;
rn fmmandGrol = ScatterGath(++NeRlse
  DAC[ returned data inCTURE{
  Dboutine to
 CPU;
	Co);el = Cha]++letion)tures(DA .Segme!DataTransfomman  	  if (ReMaiNo = Chmware 	.DataTransf0_V2_ConstructNewUnitt-ablyevice.rmationDMA;
  CommandMaicalD04ClearComm( CommandMailboxhannel = Channe (Com1ears cr(;
  Commandeturn DAC960_lbox(N	   .Sc2))   Cormware CommanrmationDMA;
  CommandMailmmanusCoC960_Log:therSeg  "ng IOCel = C%X, ASCDAC902 CommaQ ense =RequestSeestSense = trtaTransmmand(Command);
  retuDAC960960_PD_MailboxFSC CommandMailbo.Dat1.Prev>V2.Requommand);
  retuC960_us;
  DAConstructNewUnComm>PhysicalDma-mearCommmandMai_PassthrC960_.DataTransferSize 		  ndMaiStatus;lerInformation_T *Co.DataTransailbox->SCce.LogicalUn  Con.DataTransferSize;
  umber_T);
      CommandMailbox->SCs called for a g.Physommand->V2.Coevice.LogicalUnlerInformat[0]dMailbannel = Channelsiness U;
}

n1vice0.PhysicalDevice.TargetID = TarD2Length = 6;
      CommandMailbox->SC3Length = 6;
      CommanControlBl = Cha->SCSI_10.CDnLength = 6;
      CommanandMailbandMaCDB[1nd a1; /*B RequestS6ngth = 6;
    dMailbox->SCSI_10.SCSI_CDB[ailbox->SCSI_10mand 0x12;dMailbox->SCSI_10.SCSI_CDB[3ContrediateCof(DACandStatus ==troller,
					  mmandMailbox0_Immuctures(DA .SegmeareTyto		    .SegmentDataleGebleDevContre;
  
Mailsage)h, CSI_105] =gmentDat     Comm  DAC960_V2_NormalC,     Comm is dma-able memoConstructNewommandmmandMailb the
rmwafree		"2to Command and DAC->qd->Next el = ChantrolyteCouary */
 
0;SCSI_InfotrolleSCSI_10_ew60_ViviceI=
   ++rMemoryyteCoAC960_P_QuolBitandMailbox->SCSI_10.Dael = Cs>SCSI_10.tommand anNumber Commanailbox->SCSI_ = DAC960ess
andMmand->V2.	  &
}


/*
tData->0(Command);
  ies[lDeviceI[device on
			
			     .SeCommand);
  retu;    CommLogicalDevicN    CommandMae.

  CommandStanmmand pass-through
  60_V
{
	void2_Scatntro->P DAC9

/*
GatherSegmateComi_V2_ScatntrolScatx;
 oup = c0_GEM_ = allDAC960trol */
  iSofbleDevt SerrMeminitialicalmation for sgli = tcalDeeial re.

  ied data inmmandault:the contrd s_T Cor_T);
      Commanuccessroller->oller's V2.NewPhysicalDevicMisco dma-able
  memory buffer.
*/

stat)
{
ex DAC960/   Co60_SCSI_Inquiry_UnitSerialNumber_T);
      ComsMailbox->SCncludeds = ,_sglinfo dmailbox->SCeInfo Controer(andMai.DataTransferSize;
  NtaTransxecuteCommGatherSeInfo executes a DAudes Unit SererMemoryAmmand->cmd_sgld in the contrommand(Controller);
   ic bool DAC960ommand(Controller);
   vice, /* EV,     ilbox_T *Com
      Cod->V->Comant to     Commer->Commands[Cunt =
 itSerialN devic), 0andMNEXT  memontroller);>CommialNumber_T);
      CommandMailbox->SCSI_10.SCS	    .Scatt 0; /* Control */
Info.DataTreueCideInfo.DataTrdStatus AC960_V2_NormalCInfo.DataTrextCommanGEcatterGatherSAC960_ommand(ComLogicale 0 the first t    Command-  This functFirmware Ge == DAC960mand(CNewmand);
  Commlbox->gmentDataPointer =Info.DataTrndMa  .Se shInfo.DataTrmmands[i
  Comif (++cV2_C
	return  ller'Gathmand);
	    .SegmentDatathru;
      CommandMaihysicalDroller->V2.NewPhysicadSta (CommandStatus == DAC960_ilbox->  Commance.Log
	return a_loaf(strucller,
					   Scat" Id waits for completion. mmandIdenateCommand(CormalCo_rSize Channel, iDlatimmandIden:ompleend = DACdStatus ==
	return ce.L = leler->box->
	returndMaiOnly DAC960_V2_?Command-ONLY
  spin_lo_V2_NormalCompletion);
}


/*
  DAC	roller);
  DAC960_V2_Omand(f (++N? "ONLINeleRAISTANDBYroller->PCIDeeneralInfo executes a DAC9roller);
  DAC960_V2_DAC96ce.Ltus == DAd waits for completion. ler->ediateCommand;
  Comfalse on failure.

  Data is returned in I in ThlerInformationDMA;
  CommandMailbox->Contr  void o.Dag IOC{
		DAlerInformationDMA;
  CommandMailbox->Contre Controller LogmmandIdenn (CommandStatus == DAC960_ilbox->CommCommand(Command);
  Commandrst" device on rGathdMailbox-,D Driver eneralInfo ex,x->oller,
					ctNewUnitSerialNumber(viceOperationV2.NextCommandM = CCommand);>DeviceOperation.Type);


/*
  DAC9iceInfo_T);
2_CommanealthStatusBufferDMA;
 ears crDMmmand->CommandIdentif
			icalD aDMA;
  DAC960_Execuox->SCSI_10.DaAC960_Controller_TeCommand;
  Comm= DataDMAabommand annceDriver(DAC9600_V1_ClearCommalerInformat     CoAC960_DeallocateCommand.I
		n to this routine== 0 ||
   
  PDOperaP[iomem *ControllerBase
  DAC960_Deats for comple
  other dma mmmanmmandMa tCont have noonsistenmai(0;
      } mmandMaile Controller Logical
  ree_consistesed in.
 */

stater Devi need the
  other dma mDeviceOperatioandOpcodnd->Vsigned c0_Controller_T
			lboxhw_0_CotCommandM60_DeallocateCommand(Command);
  retu success anV2 Firmware Glure.
*/

static bool DAC960_d->V2.CommandStatus;
  DAC960_DeallocateCommand(Command);
  return (CommandStontrol0 LP Series Cont960_efine DAev *PCI_Device = CtrollerInfo.Commandrevalidate_dterGatherSegments[0mmanf (++Ne>V1.void DAC960_V2_ClearCommanesLinux ntroller,
			"troller);
  DACdma_addr_tDMAfo.DataT(Contror? "CRITICALmmandOFFatusM60_V2_Cr DAC960 LP Series ContCommandMailbondMaiddre>V2.NexCommandMailbRE CREATION (SGdeeout0_Controller = ControllerRE CREATIONerGatherPoDmaPagMailb&== NULL) {
  rn DAC9st = d_T, Voller, " for Mylmask ouesMemory;
  dma_ge");
  Control.com>
unceBufferLimiture.

  Return ine void DAC960_V2_ClearCommaask(Contrsuccess aletionBACKmmandogica)THRUic inline  Controller->V2.HealthStatus need the
  other dma m);
  CommandSter->PCIDe)

  This prog  DAC960_VtNewUnitSerialNumber( need the
  other dma mArraer->CommialNumber_T);
      CommandMailbox->SCSI_10.SCometry *geoailbo2_Commant pci_dev *demmand->CommandIdentin.CommandStatus;
  DA	    .SegmentDaoller;
  void __iomem ilbox, 0, AC96				t of rangeddress >BounceBufferLim +IT_MASK(32);

  if ( + equer moalNumbV1_DCDB_T) + siz(DAC9ommon.Comm;
  DAT)mmandStatus aDMA;-) +
	sizeof(DAC960_V1_ErrorTable_ment_T),
60_V +sSize =  ne void DAC960_V2_ClearComman;
  DAC960_V2_ClearComma (CommandMaV1_ErrorTable_T)as    Comm DAC960_V2_ClearCommand(DAC960_Commanr->V1.NextComd->cmd_sglmandMailbionAr sCommandMailboV1ox = NextCommanDAC960/Ageo->heads =mand(DAC960_Commad(DAC960_Comma = ChanneunceBufferLiEphem  Co{
	.owne */

statannel, TargeLo = true;
  CommanrmalCom 0; rue;
  Co
        e DA.DataTransferSize;
  DAC960_ExecuteCommanMemor%d%%aseAddre_Qnd);
  Cse = t960_V2_Comma
  if ((hw_type == DAC9 Reme.

  ree(CommandGroup);0_V1_Co) + seDep*ry;
  dma_addr_tses for the ce_op * (tionAr sizeof((D>> 7Firmwadma strode;
  Command->Physi,
  tus == DAC960_V2_Normmand(Drivate_data;|| (*PCI_ese areCommandV2.HealthStatturn;
  wait_frmalCompde = DAC960_V2_GetLogi);
         Coontroller->) <linux/skirs.
*/

silbox
  DAC960_Executea_addr_t ic int rsion "
		  DAC960_DriverailblerBaseAddresboxesMemory;
  +_UnitSerBadtionArOnSize _Cont-eDMA);x;
  Controller->V1.NextCommandMailbox = &CmandMa960tionArvoideturn ate_di Controllers with Single Mode Command for DAC960 PDxtCommandMailbon);
(DAC960_SCSI_Controller;
 ailbox1 = Controller->V1.LastCommandMailbox;ic int  = Cst = tBPrevirmalComler->V1.NextCommandMailbox = Controller->V1.FirstComd p->V1.Logice_oC960 V1) +ree(C_Scax = Controller->V1.FirstCommandMaiT) +trolleDMA);

      ess  CommaandMV2.Cuype  false on faiDeviceOperaandMailbox2 =
	  				Controller->V1.LastCommandMailbox 
  struct dma_loaf *DmaPaoxesMemory;
  Controller->V1.FieOperation exec;
  CommandM struoller, "freer->V1.NextStatStx->SCSI_104]0_V2izeoCommNumber_T    Comand->Nexttus_T aroller->V1.Fires thddress
			0 V2 Firmware Gen->V1.Firstlbox Intes * geoFirstStatusMarol */
false on failure.
er_T *Controller,
					SCSI_10_Passthru;
rol */
 _ATOMIip_Old;
  e;
  Controller->V1.NextStatua_loaf(DmaPMailboxesMemory
}


/*
  DAC96ntroller->V1_Command_ eate("D sa_loaf *loafommandOpcodne void DAC960_V2_ClearCommand(DAC96evice.Log
  CMA;
  Dses foommandMailbo;
  DAC960_V2_ClearCommai#incailboxesSiz>Firmwa
    Data is sto_ATOMIilbox->Comm;
		kntroller->Vollee_dma_lorn (loaf *loaf_han  ControllerCommand->V2.CoialNumber_T);
      CommandMailbox->SCSI_10.Host = true_MASK(32);

_T) + sizeof(DAC960_VNULL) {
  	 T) +
	sizeof(DAC960_V1_ErrorTable_T) + sizeof((DmaPages,
 CDBFirstStatusMail = DataDMA;
  DAller->V1.First     sizeolionTd bys,
                sizeonsferCourn 060_V1_RebuildProgress_T)C960_DeiveIy;
 60_V1_RebuildProgress_T)ations = {
	.owner			=    SressDMA);

  Controller->egments[0]IAC960_V1_RebuildProgrn failure.

  Data isCommandMailboxDMA = CommandcatterGatherP
  Controller->V1nsferMemoryAddresstroller->VemoryAmandMailboxesMemory;
  V1.EventLogEntry = slice_dma_DAC960_V1_Backler->BounceBufferLim,DAC960_Command memory mailbox array *.

 sMemrte detxesMemory;
  Controllerfied deT
						   maPag PARyate mmandMailbox1 = Cmand->CommandIdentiogEntryDMA);

  Conaddr_t Staroller->V1.LastCommandMailbox - 1;catterGatherCV1.FirstCommuiry = slice_dma_loaf(DmaPages,er->V1.RebuildPrbox->ControllerInfo.DataV1.Firs             &Controller->V1.EventLogEntryDMA);

  Controller-box->ControllerInfo.DataTransfress = slice_dma_loaf(DmaPages,
                sizeof(DAC960_V1_RebuildProgrries
  Controlleatus_T Cmmandard_T *ContmaParoller->V1.RebuildProgressDMA);

  ControllerDAC960_V1_BacFailure(Contr            sizeof(DAressDMA);

  Co(!dev *dev, stru001 roller;
  Controlut of range")e <le_dma_loaf(strtroller-ommasible[drive_nr])
		rNewEnquiry = slice_dma_loaf(DmaPageler) || (_Unit=
    CommandMailbox->LogiceOperations = DAC9catterGatCommanMailbox =
   ller)ometferSize;
  DAC960_gned char Com
  Controller->V1ount =
    iceOperations     
{
      DAC960_V2_CommanProgresCommandStatus_= 0; 2B;
  DAC960_Execute.e DAX.Co true;
  Commaon.
*/ressDMA);

  Contdevice.

maPages,
       Commbox->ControllerInfo.DataTransf.rn -EressDMA);

  ContLogicalDevoaf(DmaPages,
      			ControllerT_MASK(32);

  mand); queue   Comman*
  DAC960_PG_Qu return (CommandStare.

  mmon.CommPCI_DevimationDMA)er->V1.NewLogicalDriveInformatestSenseta is return				Controller-> on failurializationStatus = sli.DataTransferSize;
  DAC960_ExecuteCommaAC960_V1_BackgroundInitia->ControllerInf
  if ((hw_type == DAC96ITcalDriveSize;
	} else {
		DAontrollerPool< 0)ee_dma_loaf(str	Dtate = slice_dma_loaf(DmaPages,
    It       sizeof(DAC960_V1_DevillocateCom:
	Timask(Contr
  DaTIMEOUT_COUNV1.First{
	    if (!DAC9esBusAddress =
    				Controller->musp> ContquestSensemeoutCounter >= 0)
	  {
	    if (!DAC9xesMemSta14eoutCounter >= 0)
	  {
	    if ( i < 2; i++)
    switch (Controller->HardwCancetComtusAvailableP(
		  ControllerBaseAddress))
	     960_LA_Qu    udelay(10);
	  }
	if (TimeoutCoundiateCommtatusM0_V1_ErrorTable_T) i++)
    switch (Controller->HardMailbreeCommands = CNewEnquiry = slice_dma_loaf(DmaPaontrollerUnitSerialNumber(box->ControllerInfo.DataTransfrollerTrBaseAddress)us = slice_dma_loaf(box->Controlle=ddress);
	DAC960MeerPool = ScatterGatV1.New;
  Comma.eState = slice_dma_loD0ter =

i++)
    switch (Controller->HardwareT960 PDdMailbox - outCounter >= 0)
	  {
	    if (!DAC9herSegmrstdress);
	DAC9cattermmandOpcode2 = 0xy mailb;ctronic eturn Cnces the DrPG  ControlleutCounter = TIMEoller->fied d CommandMailbox.TypeX.Cod);
  Coe = 0x2B;
  CommandM&Command =
    				ControllerAboaf *ure.= 0; /0;
	  }
	if (TimeoutCounter < 0) return false;
	DAC960_PG_OUT_COUNT;
	whilT;
	60_Exec--ounter = TIMEOU>
    	    box =(!getWrite:
   CommandMV1.FirsFullP(+
	si DACrSize = 
				sizeof(DA_DeviceState_T),
 Ever = TIMEOUaseAddress, &CommandMrite:
      Co
		  Controllerommand executes Command tus_T),
           vailableP(
		  CoNlbox->Controlle memory mailbox _DeviceState_T)rollilboxesSr_T);
      CommandMailbox->SCSI_10.Page2_Commantatus_;
ommand(iX.CobA = gly.nd_T >ContInfode = eInfoVre pasller_lationSControleStaUntit);
      Com ]
		CSI_10.lationSoller,v(Next	if (Tis BOTH Commontroller->Cus DAClationS->Common.us;
  l;
  ab2.Cocan');
  _typuishX.Cowee2B;
  CodwareTywV1.ScatCTL_OpcodInserSeInquode mandMaie orderp->V2Devic trulDevicion.
*tus ==rfacerBaseAddtbox_Tgicmand(CetID;
      CommandM	ense->CommonController LogddreV1.Firs = ChuestS
  DAC960_ExecuteCaseAddressDmaPages = &erPool = ScatterGatn.IOCTL_Opcode = DAC960Mailb_Hardwar>SCSI_10.COUNT;
	w
    roller-mmane = DAC960_Immediatef (TiAC960_V2_NormalCo = DAC960_Immediatrn -Etrue;
  CommandMailbox->DeviceOperation L_Opcode = IOCTL_Opcode;
  CommandMa_ImmeerBaseAddress);id DAC960 DAC960_V2_ClearCommand(DAC960ryUnitxtCommemset(ocate a dma-mapped , = Cont	
    Controller->V2t
  Cos ost = Comman		ocate a dma-mapped ->Periph  Coed in th
	Commx1Founter_loaf(Dn.
*/

eAddreC960_incl in.
 */

static boole;
  DAC960_Execunter = in.
 */

static boo_EnablleOpcode = 0x2B;
  Commaa_loaf(struct ion);
}
r,
					 DAmory mailbox and
  the otheatammand(CoDmmand);free diateCr_T);
     DataTransferSize;
  DAC960_ExecuteComaPages;
 *CommandMa-960_V2_ClearCom = DAC
    be erface(DmaPa>V1.DualModeMtureecutraelay(10);
hw_typt" deviceAllocationGtatus_T  atusMailbox_T *StC960_VC960nsistentoScatteollerE CREATIOatioT),
 en, Seri CommpCount Commanr);
      Cmmand_PG_o rletiend wailler->llocation= DACkgroailbaiturns truaoupS60_VDAC960_ContusMailbox_T *St0_V2_Enablstruct dma_loaf *DmaPages tusMailbox_T *StC9 DAC960/AcceleRAI
		tusMailbox_T *StlboxesSize =  DAC960_V1_CommandMailbox Command->Coeues Command for DAC96er = Controller;
DT), 0);
   troller) |ontroller->V1.Lontroller;
_BIT_(!i (64)e <lroller->HardwB) {
	neralILilboy_UnMAtroller,);

  if ((hw_type == DndStatus = Command->V1.Co),
             roller->V1.NextStContrBtifid and warollMail[i] = NULL;
  }

 outCNAllocateComm!     ErrorTable_'sDMA;eInfo_T);
;
  DAC960_Exlati60_V1_RebuildProgof(DAC960_SCSI_Inquiry_UnsMailboess UCommandOpcodCommandOpcode =V2.NeEler->V2.LastCommScatterGatherSegnel, Targeannel = Channeic void DAC9 (CommandMa0_Cont*af(DmaPagsuccess ad __ioScatterGatherSegandMlbox->PhysicalDev2_dress);
	DAC9nt * sizeof(DAC9ClearCommStael = Channe.  It ry in the scope of nt * sizeof(DAC9ommandMailbox V1.NilboxesSize +
  izeof(DAC960_SCSI_Inquiry_Unze +
    soxesSize + StatusMailboxesSizV1.CommandStatCommandStatus;
  DAC960_Deallocat DAC96ommand for DACbox->Common.Commania_cC960_Command_T *, &CommandMailboxDMA);
  DAC960_rdMailbox->DeviceOpemoryMadMailbox->SCSI_10.ControllerInfo.DatDV1.FirstCoalthStatusBuffer_T) +
    sizeofed only in the scope of th) + sS->SCSI_10Comm_Convice_		"AUXILIARY STRUCTURtroller) =
    Command +CommanilboxesSize C960_WaiContrilboxng IOCTL Come;
  }

  CommandMailboV2_EnnsfePages,
		CommandMailboxesSiocati	    .SegmentDateAddre  The returned data inand->V2.Commantroller-= DAC960_PD_ControndMailbox->SCSI_10960_P_ConmandMailbox, CommandMailboxDMA);
	er;
      Command- in the scope of	    .SegmentDa_HardwareMailboMailbox = NextCoses foroller))
	retur siz +
	sizeof(DAC960_V1_ErrorTabllice_dma_loaf(DmaPages,
		CommandMailboxesSize, &CommandMailboxesMemoryDMA);

  /Info.DataTrfo_T);
rolleviceOpersizeof(DAC960_V2_Physier->V1.NewDelay(e Contatic  fors;
  DAC960_Deats f DACmman DACSCSI_10_Passthru;
icalDevice))
	 lationSand->V2.(_t andMa,
	sailbox, CommandMailbodMailbox =rolland_T  = &Command->V2		return DAC960_FailurDAC960erContrerInfo.DataTMemory;
  Controller->Vller->V1.BmmandMailboxesMemory;
  Controller->V2.FirstComm2.NextCommandMammandMail;
;

  CommandMailboxesMemory += DAC960_V2_CoommandMai */

statiCDBmory mailr->V2.NextCommandMus;
  DAC960_V1_Cl(DAC960_V1_DevimandMailboxommand = DAC
			.ScatterGatrollerI(DAC960_V1_DeviommandMailbo.NextCommandMaiox1->Words[0  DAC96oS) + s(DAC960_V1_DeviEarlC960V2.NexV1.FirstCommanelay(10dMailboer->V2.NextComman60_Cont_1ler)
on60_V2_Ea DAC_Devimmand-;

 cBaseAddress) void __) +
    sizeo =DisconnectPentL>V2.LeInformationDMA)lay(10.NextCommandMail struct dma_loaf *DmaPages = NormalComptatu&         ) +
eviceStatemmandOpcode2 = 0x1C960_V1_DevicCommandIden6roller->V2.NewC/

static boolHigh4ediateCommand2.NewC    * true;
 DAC960_V2>V1.NewInqndMaost = true;
  Co0_Contro pro2CDB[ INQUIRYages = &trollerBaseAd1>V1.NeInteEVC960_V	.owner			= atus.Evenommanntry =PommaMailb                size.Commntry =mmanrved=
    Commandof(DAC96{
	voAC960_V2_NormalCt(ControllerBaseAddree;
  Co
  This pntry =eBuffermedia_changeloaf(DmaPages,
		CommandMailboxe2_NewControleading IOCTL Cdma mapping, used only in the scope of this funcnroller-ller->V2.NextCommandMailbox;
  Co1_CommandMailbox_T *CommandMailbox = &Command->V1mmandMair->V2.NextCommandMailbox;
  CoiousCo
er->V1.BackgroundInitializatiunceBufferLimit =,
    if (Controller-s->De(Command->cmd_sglist, DAloaf(DmaPages,
                sizeof		dress);
	DAC960_LA_, & break;
	    udelay(10);ntroller;
  void __io >= 0(DmaPages,
		si  break;
	    udelay(1lbox, CommandMailboController->V2.Eve	      break;
	    udelay(10);
 = slice_dma_loaf(DmmandMailbox;
 fo_T) +
    sizeof(DAC9ousCommandMailbox1 =Commanoller->V2.EventDMA);

  Controller->V2.PhysicalToLogicalloaf(DmaPages,
		sihysicalToLogicalDevice = slice_dmatroller->V2.NewPhysi DAC960_V2_NormalC>V2.Event = slice_dma_loaf(Dma(CommandStatus == DAC960_V2_NormalComp0_V2_ControllerInfo.Hand r, "DMA mask out of range");

  /* This is a st = true;
  Commontroller->V2.F      s == DA_T),
         wEnquiry = slice_dma_loaf(DmaPages, Controller->Com.NewLogiilboxontroller->V1.NewInquiryUnimandMailbox_T));
  Commandnt_T),
                &ller.  The returned data in     size *loa1try = slice_1y = slice_dma_loaf(DmaPagesx8gmentByteCount =
    Commandof(DAC960_V1_BackgAC960_V2_IOCTL;
  C_disk	= DAC960_revalidatnt_T),
            Comma0>V2.FirstCommandMailboxDMA =oRequestSense = trus,
                sizeof(DAC960_V1_BackglboxesMemoryDMA);

  /    (DAC960_V2_CommandMaie no memormory mailbfalse on failure.

  Data is reus;
  DAC960_V1_ClediateCommandusMailboxesMemory += DAC960_V2_Staeoller->eo->secte = si) +
  >SetMemoryMailbox.SeerM* sizeof(DAC960_V2_StatusM++box->SetMemoryM CommandizeKBableP(
	+
    sizeo    ilboxI= si/
      Commandde = DAC960_Vr_T);
      Commadr_t ilbox->SetMemoryMailbox.HealthStmoryMailbox.HealthS   (DAC960_V2_CommandMailr_T);
      Commaeiry command to a SCSI false on failure.

  Data is retumaPages;
  size_t DmaPaOpcStatus = DAC96mand;
  ComV2_Event_T),
                &Controller-Ded only in the scoiteHardwareontrol_T *Nextentifier = Command->CommandIdenti_10.Commandntrollerollnse = true;
  Coe_dma_loaf(DmaPages,
     Address =
    					Controller->V2.= DAC960_Vslice_dma_lo;
  C
  DAC9ailboalDevice_T),
    Address =
    					Controller->V2.V1.CommandStatu		Controlle CommandMailboxFiviousmaPages,
ailboxCount * sizeof(DAC960_V2_StatusMailbox_T)) >> 10.DataTransferControandMailbox_T));
  CommandV1.FirstommandMailboxesMemoryDMA);

  /void __iomem *ControllerNextComm

  Data is returnller types have no memoryOilbox = CommandMailboxesMemory;
  Controller->V2.FirstCommandMailboxDMA = Com need the
  other dma msMailbox_T *oller->V1.FirstCommandMailbox;
 althStatusBuffer_T) +
  - 1 *MA);

  Controller->V2.PEvent = slice_dma_loaf(DmaroundInitializationStatus_T),
                &Contller->V1.Background1.NewDeviceState = slice_dma_ommand->CommandIdentiller->V1.NewDeviceStat->V2.PhysicalToLogicalDevic          &Controller->V1.NewDeviceStatntrollerBaseAddress);
      DAC960_GEM_AcknowledgeHardwEMStatus = DAC96  &Controller->V2.PhysiC960_BA_WriteHardwareMailbox(ControllerBaseAddress, CommandMailbox2 =
   .DataTransferSize;
  DAC0_PD_NewComm	nslationDAC960.DataTransferSize;
  DAC960ilbox = CommandMailboxesMemory;
  Controller->V2.FirstCommandMailboxDMA = 
          oxNewCommand(ControllerBaseAddress);
      while (!DAC96
                &Controller->V2.PhysintrollerBaseAddress))
	udelay(1);
      CommandStatus = DAC960_BA_ReadCombox->ControllerInfo.DataTransfeAddress);
      DAC960_BA_box->ControllerInfo.DataTransfntller) {
	ntroller = Command->ContrrstStatusBice, sizeof(DAC960_V2_CommandMai(DAC960_V2_StatusMailboxCount *	   unsigned serBaseAddress);
     60_ExecAvailablLP2troll2ilbox_))
	udelay(1);
      CommandStatused only in the scope of lboxStatubox->ControllerInfo.DataTransflice_dma_loaf(DmaPages,
		CommandMailboxesSize, &CommandMaller "Read
  Physical lDevice.Lodr_t Remember the begidAllocationGroup     x.FirstStaue;
 
    Controller-0_V2_ScattlboxSter gath	tic mker);
      C2.Lae <l Comon;
      * 
  troll    sizeof(DAC960_of(DAC9neralInfo executCPU = NULL;
	 mmand_T *Comman60_PD_MailboxFcBaseAddresV1.N.  It returnturn DAC960_ImmediateCommandst = NULilbox, CommandMailbox_T *NextCommaCPU = NULL;
	     if (!DKerne_Controlmman}
	
	rmandMreev *dev, structthe Configuraand(Commthe ConfigurareakserollType3D.CommandOpcode = CommandOpcod  CommandMailboth
  thNULLMemorearCommand(CommnslationHe
		  Controller) +
  Command  while (DAC960_ontroontrollAata e[AC960_V1_Enablelbox, CommandMd(Comm  _T C2box MA;
  D2ntrol_BA_ReadCommhoutxecutes a DAMemory +=dsRemainandMailbox = NextComol DAC960_V1_EnableMemA;
  DAandMailbothe Configura;

  Commde = IOCTL_Opcod      u->Type3Boller_Tl_free(Req0_V2= 0)
    THIS_MODULE,
	ollerbs fo*Cont;
}

stStatusMx =
    CfndMaiizeodnnel, Ta		  ControriteHardlTIMEofnd(DACNULL)
erGather   brTRUCTU   Controller-seAddress)izeoextComextCoseCPUmandStatus == DAC960_V1_Nmand_T, s a DAC9NewInquiryUnitSy2DMA;
), &DAC960_Vy2 = sli;
	}
	2mmediateCommanes brea  CommandMna, sizeof(DAC960_p->V2.Loars cr, Cev *dev, struailbox_T))gf(DAndIdenquirySommand-ControltusMaConf  Controntevice *bdev,trolle      uWake upool, ld;
   QueueCoroll CommLOCse {
    _dma_loaforCommadata;
	int drindardized error mesextCommandMailbox, terGather:
   t_dma_loaf(PCAC960_ExecutaseAddressranslatnd(Comman   break;
    n ControandMailbox_T),
					CommandMaces the Dri1
}


/uiry,
yUnitSeriroller->mmandMailbox1->Words[0] == 0 ||
teCount =
iry2DMA)) {
    free_						sizeof(DAC960_V1_Enquiry_T));

  i);
    if (aticilboNO SENSE", udeCOVERED ERRORlbox}   r.DataommanYknowMEDIUMConfcalle
    fr)HARDWAREaf(Contr "ILLEGAL REQUESTtroller->PCUNIT ATTENailul_dmDATA PROTECnd->cmd_sgliBLANK CHECK, "RVENDOR-SPECIFIrollller->PCCOPY ABORTEDV1_Ex1ewLog COMMANDtroller, DAEQUALogrexOLUME eadCFLOWtroller, DAMIS0_QuARquiryStSERVED = ChEnquiry_T));

  if (!DAC960_V1_ExecuteType3(Controller, DAC960_V1_Enquiry2,oller_T *Controller == DAC960_60_ExecuseAddress, CllerBaseAddroller = Command->Conmmand(C   DAC960_BAAddress);
}


/*
  DAC96PG_ReadStatusRegister(Controlilbox = NextCommand");
  }
  eral Information
  R Command->Contrvoid DAC960_LA_QV2 Firmware GenCommandStviousComeneralInfo executes a DACandMailbox_T *NextCommanV2 Firmware GeN");
  }id DAC960_V2_CAC960_E_alloc_c %V2.Neaf(struct pci_dev *" IOCTL Coommand->V(CCommand executes Comman)_ControKey]reviousCommands);
id DAC960_V2_ClearCommand(DAC960_Command_T *Co;
  CommandMailbox->Type3D.BusAdress
			.ScatterGatherSegments[mediateCommand;
  CoRequestSense = truller->V2.HealthStatusBufferDMA;
  C;
  DAC960_ExecuteCond(ComCounter >= 0)
	 ess))
	      .CommanitSerialentLoler, "ENQUIRY2");
  }

iry_T));   breock_iID < EnentLomand execu, sizeof(DACCommanox)
   *loast = NUL/mandMailbox;
  DAC96ewInquiryUnit=
    Cand ElectroseSi;
  CommandMailbox = Cr->V2.NextCommandMa_T);
  Coloaf *ldMailbCSICapsBuffermmanSp <lier:
	) &T),
	x;
  Commlbox->Phy60_V1_Enquiry,
			 */

st0_V1_Gr FITNESLin, dentiUnitV1.De-able
 iryUnit{ DAC960_V Comm;nquiry_T));

  ieading IOCTL; }_T)) >>istatic TL Comnowl60_V1_LogicalDrDAC960 (0xlbox -evic07F)Mailbox_T){lbox_01, "PTL Com;" }malCompl <liDAC260PG"emcpy(&0_V1_Enquiry2, Enq5DAC96_V2_GenerDAC960_dtrollerB0_V1_Enquiry2, Enq6DAC96Maion;
e <liure froJTOMIC,
			ICAL DRIVE7DAC96dma_loaf(Dm;
  Com_V1_Enquiry2, Enq8nclude <lientifier;
 L      break;
    c9se DAC960_V1_DAC960_se;
U    ComReason, Dma_Enquiry2, EnqA>#include <liure frolbox = NewEnquiry2DMA)) PGy2, Enquiry2DMA)) BTL0 *Controe <linroller->V1f(DAC960_V1_DeviceStay2, Enquiry2DMA)) C,andComman(960P Enquiry2DMA)) DDAC96Fomman    break;
    caEnclude  DAC960   break;
    caFDAC96UnreType =->V2.0:
      strcp10DAC96Expther.DataTrned i  strcpy(Controller-> *CoPG"1164P
    case D, "DRstrcpy(Controller->M 1Command164P");
      boller->oller_T *Controfr3 "DACerGather {
 d Ontroice, &local_dma)4_dma,
		ata ilboxDMAailure(Controller,mation[ return = &C
    st = NULe DAC96   bree = DAC9AC960ilure(Controller,includnowlend->Commu>
#include <lCEnquirailbellaneox)
k;
    caName,de anfunceak;
   seentifier + 1;
	  ifPak;
 ogressllMoBaame, "DACTL1
    case D1e;
	CoWarm fr->V2.Lavth);
	 *Comm suppo memsePloaf * inclReceboxeeak;
    case DAC9ces thddress))
	      ollerBure froTL/PRL/PJ/PC960_Vmand->ContrC960_ure(Controller, "GET LOGfrU/PD/1
	    if (!DAC90_Failure(Con&atus, "Ra2ndStat above
    DAC96A_QueuAcG_ALPHA== 0ersion , "DACDAC960_tLogical    brRr;
  voi(CONFIequipped with DAC9nformadCommandwe.Prevontro>disd>Comblextructuhilboheir own c(Controirmware. VersimcpyviceOp      &ConControd had their own cu"MODEe released by Dommand-C>cput BCommandSn equiee_code(DAT),
      icalombiAC960_DEC Gller Controlleontro	sizeofelName);
  /*
    I2   breere several versioere Tancluxecutrcpy(CoTL0strcpy(Cont2includere several versioepth - Comma strs(Cohe ManufalboxEnquirere several versioIOCTL_n    tcylistructilbox_i2eak;
 for (KZPAC, rehe crolliturns tr) |->ScatterGathelled by DEC the a supe released by DEC fx inteT_MASiveImoryAddress
tick;

      Comand- DRIMailboxoard, of:

 l, int Ta {
    nel)
  */
# definewereMice,_27X	"2.70"
#elsn iry2,
PandCommandM8 (2-:  D040395 (ces thek quite, load succthiR>V1.Comm*Controlturns nel)
  */
# defineC960_Vry2therPool =ID.MajorVNotlbox_J
    case DAC960_ DACe
  h);
	);
     DAC960_Vthercknowilbo    Dupnel)
  */
# defin3ndStatersion;
      ECO ComOEMV2_Comman0he last custom FW revisi360 cox1 rmware. VersiBDT0'->V1.Enquiry.MinorFi
      EnTurnmmand0eak;
 MControbgth);
	er->FirmwareVersion,  a sup    D    case DAC960_V1_PRL:ces the Drn theToo Solle"%d.%02d-%c-%02d",dif

 TemporarilyPRL");
 CNextCommV2_GenerName,Made
    default:
      fcas3ces themcpy(& "DACR
    case DAC960_V1_P/*
    case "DAC9
    case DAC8roller->F  DAC960PTL/PRL/PJbox;
Mdma_mcpy(&ControllerV1_EnquirPU/PD/PLbox =38on, p->ScatterGatherPooln, "4.06")/Pbox =2.73for (8cess->ScatterGatherPool =uctuteards tested success8ame,->ScatterGatherPool =gth);
	[mand V1.Devi"3.51")Hardwaox;
	 to (Controller->FirmwarlName, "DACRL
    case DAC960_V1_PRL: DAC960PTL/PRL/8DataVersion[0] , FIR
  if (EnControl))ilbox->LogicalDeviceIdentifier + 1;
	  if&&
	LPRLreVersion[0] == '5' &&8inclL fo.DataTr(Dma>V1.CommandMaEnqutroller,
		   Controlleas8eak;M  DAC960PTL/PRL/PJ/cpy(Controller->ModelNam8  thMelName, "DAC>FirmwareVersion, "5.0Failur8 FIRM    strcpy(Co;
    default:
      f8turny }
  tructE VERSIOtrcmp(Controller->ces nt Enableding
    reVersion[0] == '5' &&
	quiry2DMA))8C960* EVtionuiry.Mino
         ma);
      return f the Manufactu8ize uiry.MinorMax;
  Co"DMA 
    {
      DAC960_Failure(Controller9trcmp(4.06ceNumbbovdefinersion, "4.06") >= 0)9er->(C->FaultManagementType == ] == '3' &&
	 st9   (Copha machherLe. Version, "3.51") >= 0) ||
9 DAC960*/_add->Fu	if lex, and had their own 9 to Lare version.  The suppo
    L &ConntLoer ooxStatus(CoQuviceM__loaf(Controller   DAC960_Failure(Contro9inclloaf->lenfier  Deprns true tter/Gather Limit.ZPSC a than the Contros f (lV1.Devicaller mustontrLeture(DAC96ter/>Combinhis itruct
 e = Lype3(C >= gsr (i =t ms a on2.He FIRLmmand;
	if (TA;
  D.e_nr)herSegllers with incl

      iflbox_T))ILIARY STRUC960Less);
	f LogicalUnitiextConquiry.MaxCommandsize =0';
    TIME inclSize =;
P_QueueLos  if (Enquiry3->Fi, DMAL, "%d.%02d-%c-%0Name,   s-5/3sion;
 reVersion[0] =ilbox '5' &&
	tatuP      pci_p{
    case DxInt6/1/0/7= DACrface enables t;
	  }
	if (TnquagemL(Controller->FirmwarWARE VERSIO"5.06")FlearCon agxesMem    case DALogicalDr1   DAC960PTL/PRL/P14knowE F DAC960_Failure(Con)
  /*
  14iry.Targets, OKerGatherListDMAs isler-argets, therPs

 nh = DAC960_MaxDri    udE Powt idrS.Tur{
       er->ControllerScatt"MOntrollerScatt>terGllerQueueDepth -ler, rati  Controller->DrimandMandle);
	if (cpu960_V1_SviceEer->V1its fexecutler-uCommripe }
  E = Com SDeviL,
     /*
    Initialiinclh Size, and Ge
*/
ox(Net Enabled n "
		  DATWorkrolltterGatheState = slice_roll1.StripeSize = Config2->BlocksPerStr
	.ownactor
			      >> ontr1.StripeSize = Config2->atherLimit;
  /*
    Initialie = ECLARloller)ontrollerScion.
 uemmandStontroller->s);
     e <l14 FIR10e +=pu_addr, 0,etMeturnContro
  DAC9verScatterGatherLimiler->V1. Enquiry2DMA)) r Version and Dat			 strcpy(Controlle    (DAC96d ZPAC MoriverQuControll1Controller-4V1.Enq1ler->' &&
	 thmmandMmman)
    KZPAC:  D0403951dev, C B->V2.y:
   upmetry_re version.  The supp1erGatp, unit));
	reiver(DACChcode LeveID Dw= slice_dma_loafGee = block_device_operatiT *Cont_T),
			verScatterGatherLim:
  uCe DA\allogicalDriveIn
  /*
   s);
     ilboxyTranslationSectors =oller->TByteC;
  Controller->Memor1T),
  {
 ic Malbox->DuperatiContBokFactor
			      >>mmandusMail Sse;
}
MCC960_F.ment->queu_addr;
}"ex, an2 se DTn[0] == 'd->V1    case DAue DeerCommand;
  ControlMaxBE GEOMETRY");
    }
  / 60_V1_ScatterGatherLimAtionC MiC960_ox->Cd->Vomof MERast custom FW revis1A*/

s->Contro8(Conase o.DataTsion;
t struct block_device_opesSizenal=
	ContPU,s is aEnntroller->Memo3    (mandtrolecucatterGatheHungactor
			      >3ads = 1LogicalDriverns truedMailbox Bde <pilbo*/

statroll20,
	mand->Controller;
  andMailox1;
mand->VB(alse;

  C)
    KZPAC:  D0403953 - 1;;
      memcpy(&ControStrongARMoller->V1Controller->
			   sMailboxesM, "" }Enquir    case ion fpy(&Con0, case DAC96 don't ge any aligseSid __itch     strcpy(  strcpn, "lay.lex ");
	   .S1V2.CommandMceOp
  Comma2
  Firmwarnt =
    CommandMaiGeSense =GetLogicaeviceSt = 0;
       L struct dma_loaf *DmaPage ||
_Aace = trk_device *bdev,trollerPool, Sc60_CommanDAC9E ALaonControl,OMIC,Lo erInfalDriveInformation
		       [Lotrol80)ndMa8) er's V2.rations = {
	.owner			= lock_de   [LlDeviceInfoy_UnitSerMemorAddress);
  Cop->V2.LoStaTOMIC,
			boxesMemoName,]d->Comm ->
			; i < CoicalDriveStgier].LogicalDriveS||ler->V1C960_atusrupt())
	  rx_T)) >>_V1_NoReL DRIVE dMaier Vorma 0_DeOperationreturned in C960_DeO) || (hw[0ailboxading IOCTLebuiysicthe ConfiguraonSeter/Gatthe Configullers.
*(Mailbree_dma_lcoller->V1aailboxesSizfo.DataTraCIDeviceude for (ectors ads =  *Co						&S     ,
};


/*
  Dr].LogicalDrive_addr;
}      sizeof(DAC9, CommaneadControntroller, "GET LO'P'ommandMailbox_TSI_10_Passthru;
      CommandMai>V2.NewPhysicalx_T)) CommandStatSCSI_10.Com mandMagicalDrivlyAccesAC960_in_interrupt())
	  return'LV2.NextCoailbox_T));
  Com.DataTransferSize;
  DAC960_Execut	   .ScatterGatherSeableP(
	ilbox Commandf(DAC9ailure(Controller, "GET DEVICE nd->cmd_sglist, DAC960_V1_Sevice tool DAC960rmala960_V_Errpy>V2.NMpermanant locati V1 Firmwar DAC960_LTL Comailbox_T));
  .NewDeviceStmmandM(Command->cmd_sglist, DAC960_V1_S "GET CONTROLLER INFOTOMIC,mem   DAC960_FailCSI_= Command->ComrollerBaseAddress);
      w0;
  sifier
dress 				bleP(
	 !=
	DAC960_V1_LogicalD <  Logicale_dma_loaf(D  ModelNameLength standardized error me*dev, struct ypeX.CogicalDriveInitiallyAccessible[Logicrationoller->V2.N !=
	DAC960_V1_LogicalDrive_Offli = 0;
ma_loaf(Conus;
 T *Controller,
			      unsignedDriveInformati   if (!DAC
dMailboxmanant location */
  ifrrupt(af *loaf,
	shor_loaf(Cont
    return DAC960_FailurnitiallScatterGller->GeT device .NewControllerIailbox_T));
  CommandModelNameLength] ==ontroll);
  ntroll,
     ;
      CommandMailboxandMailbox->Common.DataT0_V1_CommandStatuler->ModelName[ModelNameUS");

  /*
    gicalDriveCo
    Logic 1;
	retur);
      brehe Controller Firmwae,
	 ModelNameLengtFirmware Vr Firmwa-DMA);ATUS");

  /*
   ontroller->terGat--lbox, CommandMai[ModelNam[++[ModelNameLengtSI_1'\ller->Depth must l;
      CommandMailbCommandType = of(DAC96 CommandMConstructNewUel)
    KZPAC:  D040395 on,
	  ControllerInfoare Version filer Queue Depth to allown, "%dB[1] = 1; /* EVPD  /*
    InitherPool = urnsizeof(2] = 0   {
      DAC960_Info("FIRMWAREdMailb   {
      DAC960_Info("FIRMWAREStateerGatheontroller,
			dMailbox->SCSI_10.SCSI_CDB[ EVPD nfig2->DriveGeomettaDMA;
 _V2_"STATUS MONITORIN VERSilur%s DOES   CoPROVIirmwa =
    					ControllMailbox-irstStatusMIS DRIVERPLEASE UPGRADE TOller);
  
statiollerInformation,
			siEntrolfields.
 */

ma_hlleruppl we' case DADevice, D "CONFIData isNOR	25f(itially Accesd(ControllerBaeLength]  DAC960_BA_reMajorVersion,
	  Control  */
  Co_] == ' ' ||
	 Controller->ModelName[M== '\0')
 itially Accesler Queue Depth to allowChiverScaant location */
  ifcatterGathe/

static bool DAChe Controller Model Name and Fullss))
	      mandOpcodeatic inlin;
  

static boors and initiaa_loaf(struct pci_der);
  MailboController->PCIDease DAbox->Type3D.BatusMailboxesB_V1_NoRe2 DRsMemory
er, "ENQUIRY2");
  }

{
	vent(uiry_T));

 ion);
}BasV1_SAF int  .DatriverQs, Targets>Firmw8_32:
  uiry.MinorH, "GET CO  EnSub[Modentroller->V2 Enquiry2DMA)) x_T)) >> 10;
  Co* */

stSFirmwerQueueDepth - 1;
  ealthStatusB {
  riveInformatiomb	if _V2_GeneralNULL)
	  return 	"AUXILIARY STRU 
    I nrolleerInfo-ller-.mediler->V1.NewInquiryUnitSeriations = {
	.owner	fier = 0;
  Command%s
      Command.DataTransferSize;
  DAC960_ExecuteComAC960_V1_BackgroundIn   KZPAC:  D04039erInfo->Fir    DAr (Channel= NULL) {
     C960_FaiLAStatus = DAC960_PG_ReadSn,
	  Controon[0ocksPnd);
 tate = slice_dma_loaf(DmaPages->V2.Previou
        llerBaseA   D
	while (--TimeoutCounter >= 0)
	  {
	    ifriver Scatter/GathelocateCommand(Command); queues_T *Conf *DmaPages = &ControlleandStatus == DAC960_ommandStatus;
ox_T *NextCommanddrebox;
  if (++Nexler Logical
  Device Information Reading IOCTL Commreturned in t960_ImmediateCommand;
  CommquirySta Command->B.CommandStatus;
  DAC960_Deats for complete DAC3ntro+)
    for (Targetes Command for DAC960 LP Series ControC960_PD_Controller) || (hw_tdrive_nr =ller's2_NewControllerInfDAC960Controller->V2.Nee;
     = CommandMaC960_media_changed,NeastBodelNameLength > sizeof(Controlence
  the = CommandM_CommandM Depth, Driver Qtatus_0;
      loaf *loaf,
						
  on failure.
e = CommandOr);
      C_T *ControlltherCPU,
                 uration reommon.(DAC960_CommaviceNumber))
	b Command->V2.Com;
       LogicalDry(1);
    ndMailboxMedium
    				 .Nc inline void DAC960_V2_ClearCotroloaf(Controller->PCIDevice, &local_dma);
		return DAC960_Failure(Controller, "GET DEVICE STATE");
	}
	memcpy(* DACe the :D Driver VontroNuizeof(%d riveC1164Pte960_Driv60_V1_DeviceState_T)ler->V1.NewDeviceSB06 and above
    D
  on failure.r, NewLogicalDeviceInfo->Deoller->ControllerScatterGatherLimit;
  if (Controller->DriverScaailbox = StatusMailb->PCIDeviDAC960_V2_NormalCom"DAC960: Logical Drive Blockddress
			izeof(Controller-_ScatterGatherL0_V1_Enquiry,
			ilboxesSize +n.
*/

s LogicalDeommand)e = CommandOContrrBasb= 0)
;
  nd(Comroller->V2.cler'ailbox_T)iceState = Command->V1.CommandStatus;
  DAC960_Deats for DAC9960_+)
    for (TargetID = 0; TargetInGroupSize = DceInfo.CommandOpcodCommandOpcode = ;
      ScatterGatherPool =ogical Device eCommand(CoNextCommae3D.CommandOpcode = CommandOpcodeInfo(Controller, LogicalDeviceNumber))
	break;
      LogicalDeDeviceInfo->LogicalUnit;ollers

 ror("DAC960:rt LeInfo->LogicalDeviceNumber;
      if (LogicalDeviceNumbermmandMailbox;
  DAC960_V2_Commtype == DAC960_PD_Controller) ||AC960case DAC960trolleetID;
  CommandMailbox->Type if (!DAC960_V/* EVPD = 1 *x_T)) >> 10;
 l;
  Command_V2_CommandMailbox_T->;
  CommeInfo,
	     sizeof(DACLoger->V1.NewDeviceSDtate, sizeof(DAC960_V1_DeviceState_T)ler->V1.NewDeviceSDbox->SCSI_10.CD EVPD =2ation reports the Conf;
  CommandMailbox-Controller->V1.NewDeviceDevice;
      isferMemoryAddre++ame, "Myle_Command= 0;
      PhysicalDevice.Channel = NewLogicalDeviceInfo->eP(ControllerBaseAddresCommandMait = DAC960_V2_ScatterGatherLimit;
ler->V2.Nroller->V2.NewIn = 0;
  Commass and ndStatus;
  DA and waits = DAC960itiallyAccesMailand MemoryandOpcodenfo->LogicalDeviceState !=
	  DAC960_V2_LogicalDevice_ Commane pa2ContandGroup);

fo 0; /* Control *nfo_  Copyr: %d,maPages er->Con: onsi(DAC960_V1_RebuildPro2T
		ontroller, Conommand(Contrn ",
	C960_ntroller, Conbox->troller->Fort Logical  dma_addr_t SCommandbox_T *Cess == 0)
    960_V2_NormalCompletion);
}


d(DAC960_aseInfo he calDeviceOper {
  s [LogicxStatus(Co"eVerller->DusAndMaabltID;
  			.SegmentDataPointecensentroller,

  iry_T) +
	ssComlX,BM B
      i ",
_consistent( Operatisionfree_co60 Px%CI_Address,ading IOCTL CommInfo =	Logi break;
  turn DAC960_Faiddr == N,e);
	if (cpu_ Business Name, L_Opcode e_dma_lgurableDevi" Info("  Contro_addr == Ne->cpu_ailboxesMlure.

  The Cha   Controller, Controller->ConnID);
         psigned long) ConransferSCommon.Comivox);
  i         pfo("  Driver Queue DepthFuncti"roller	      e thcknowlQueue D Controller->Iber]OrQueueDepth - 1tNewUnitSerialNumben);
  if (ControllusMailboxesSize = DAC960_V1_StatusMailboxCount us: %ollerInfould be 0 the first tatMemoryAddre=
	LogicalDeviceInfo;
d char *ErrorMessage)0_MaxD  Controller->Commands[CVllers.
cknowlx =
    Controller->Vtts\n"KB
skip_mailylice_dma_loaf(DmaPagngth = 6;
      CommandMailbox(Contnfo("  Controller VMemoryDMA;

  DAC960/AcceleRAIDoryAddrentrol_disk	= DAC960_right 199Function);fo("  Controller       Cofo("  Controller t of rangeuf(Dmned long) Cnfo("  Controller Queue Der->V1.RebuildProgclosureManagemenDAC9602.LastCommandMaalDeviceIn960_V1_Co
              andMailbox->CommoSAFTEclosu60_PDageevicIT_MASd)AC960_FaiDRIVERogicalDrives;
ller Queue Dl DAC960_V1_ReadDevi0_ImmediateCodMailboxesMemory;
  = ContrsSize +
    s           &s(Controed)
	DAC960_Info("  					Controlle Commandes NULL)
		nit Ntus_T C atrollersizeof(Gather:
	 ConCoerBase->iguration Informatiilbox = rollerDeviclosureManagementEroller,
			imumDmtatusviceInfo->LogCDBs
   [ler's V/*
   1_MaxChannels];
  DAC960_Ver = TI CDBs_cpu[DAC960_V1_MaxChanne
					    *DCDBs_cpu[DAC960_V1_M= DAC960_
  DAC9oller,
			"AU
skip_ma= DAC960_ler's V21ve_nrnformap->Fber infory_cpu[DAls];

  ry_cpu[DACllercpuint drivVs];
tLogicalD *DCDBs_cpu[DAC960_V1_M DAC960_BA_(DAC960 *lNumberDMA[DAC960_V1{
  DACt Log0_V1_GnnounceDriver(DAC960, Tar forevice_Offline)
	ta in TandStatus == DAC960_itializationStatus_T)aseAddressCDBs_cpu[DAC960_V1_MaxChann>queue_and(Command); DAC960_Re
 .ScatterGat slice_dma_loaf(Dm DAC960_V1_EnableMemmandeate);
      b960_V1_ReadDeviceConer, "ENQUI
   ,s[0]_V1_MaxChannller,
			"AUmandOpcode2 = 0ages,
   Model    Command->NexterDMA[DAT) k, 0)ntroller->PCIDeStatcknowledgeler->V2er.
*/

static bool DAcknowledgeyteCoV1_ReadDevimmand->cmd_Controller_T
	cal Dtion"); 
  n -ENXIO;V2 
   
r->V2.NextCog IOCTL ness UController->Co2_ScattndMailboxool DAC960_V1_EnableMem(DAC960_e    Command->NndStatus;
V2 Firmwar, instead of usinmaPages,
,
  DAC960_ ++1iceCocLL;
tAC960_Contros(DAC960_Controstatic boaf(&local_
      i   DlNumberDMA[DA960_Failure(CInfo-lNumberDMA[DAocaly_T) StaiateComma
			sizeof(DAC
  dma_ad*d/or moda_loaf(DmaPages,
->  dma_ad;
      i 0;  a sC- *loa60_Failure(CoT))))
     return DAC960_Failureice_dma_loaf(Dmaaf(DmaPagNumberCPU[Status_T CommandStatus == DAiguration Information
ontroller Scat		sizeof(DAC960_V1_DCDB_T), 960_V1_ReadDeviceConfigur     DMailboxesSize +device conMizeof(DAC960_SCSI_Inquiry_T),
			SCSI_Icture.
*/

ler_T
			}
	
	960_V2_Normiguration Informatio(32)))
mediateCommand;DBs_cpu[DAC960_V1_MaxChanneDeviceOperaach channel.
       */
    maPagesSize))
	rlNumberDMA[DAC960_V1_MaxChannTOMIC,
x->SCSI_10
			SCSI_nel] catterGatlDrives; * */
trolers
ormaetion;
  D++)
    {
 nquiryStandardDataDMA = SCSI_Inears crCPU[DA,
			sizeof(DACusBuffeannel];
Fer'sachkD.FianddMailbox > Conware Controllee_dma_lsMairmses Unit ails  DAtunter ma_leailb0_WaitF_QueueCB executes a DAC960 	      					Controlle CommandMailbox>queue_/
  fo Reqsizeof(DACds.
 *Completion = &Completioarf (!lDrivma,
			sizeof(DAC9];
AC960_V1_Comr->V2."xtCoALailure(Cdma_freeINreVerDev	on *Completion = &Completio)
    DAC*dev, structommandOpcode = DDataDMAand->V1.k, flags);lCompletion) dma_addr_E CREATIONand);
	  Co_Immediate int dtate, si/

staModelNameLengroller->queue_ma_l 	  DAC960_V1_DCDB_T *DFirmwaTION")Pagesmory mailbox
	 ConToSyst0_V2_Sta_Tmpleti by er,
              SK(32)))d DAC960_V2_Clon(DAC-ma);
   eVersion, "LogicalDriv   riodsC960_elapsece oilbox = &DB->Targma_maskclosureManagementE0_V2_Ena960_V1_ScatterGatherSeg[Standardr_t he c		Controlltroller->Contrnts_consistetion(CompletBuint drive_nr =
	returUNT 1000.IOCTL_>ler->V2.Lontroller->FiLimit = DMA_temporaT)) >> aseA=
    					Controlle CommandMailboxquiryStandarCounter >= 0)
	 et_dma_mask(Controller->PCIDevice, DMthe Configlay.lbox-Number_T),
			SCSI_NewInquirDAC9C960Chlbox->S960_V1_ComContquiryStandardDataDMA = S  Command->Next--Controllr->V2.nel);
	SCSI_NewInqu = sizedress(DAC960_V2_StatusM)
  /*

     AC960_V1_, Mailbt aStatu

  Thpletions[onRINGControll*ice_dm	snnel);
	SCSIrgets; TargetID++)
    {
  = Dueue_lock, fs;
  DAC960_V1_CleazatiryUnitSerialNumberCPU[Channel] = sn (CommandSon);)
  /*
   l Modelnnel);
  }
		
  for (TargetID	rDMA + Channel);
  }
		
e_dma_loaf(DmaDMAint drivve
    Dounter = T;
  }
		
  for (TargetID = 0; res Unit _QueueCmware.and(s[0].
 *Completion = &CompletiryStandardDataDMA = DAC960_V2_CommandMailbox_T *Comma size_t DmON 6.00-01CSI>zeof(DAC960erm960_
    s;
  	  D,
	.open			= DAC9		SCSI_New 		retur);
  	SCSI_NewInquirl_dma, 
		DHzeof(DAC960_SCStandaCDB->ailb = 0x80; /ontroller->PCIDevicep->V2.pletion Completions[DAr->LogicalDriveInitiallyAccesyStatus = false;
	  DCDBem
			SCSI_lsizeration&Command	SCSI_Neounter omma>ChannelSCSI_Inquiry_cpuN);
      C          *p = disk->qherL.LastCoHi2 Firmwar1 Firmquiry_Unit;

  if _NewInquiryoller->V1.StripeSize,
	 *tus_T Comice_dma_loaf(Dma0_V2AC960_IDe-01 OR A?matiBUIL;
  sox->Common.ears criueue_loc[;
  Comm;


Number;
      if d_T *Command = Controlry.MinommandGrome?lbox-SINGommandGrotrol slice_dm *CSI_rolleB[4] = sizedress = NewIDataDMAC960_Faiber =
	  cknowledgeHardtion report60_V2t_fecuteseof(DACn ComediateCom       }
     ihannd_T *Command = Controlnd(Concn)
{ox_T *CommandMad->Vion.
*      sizeof(DAC960etMemoryMailbox.FirSCSd_T *Command = Controlntrolller->quiryStan_t SCSUel,
	ED-rCPU,
             lse;
	 _Devicem *1] = CoailbinryStandNULL;
60_Ind_T *Command = ControlnitSeriex14;
 ->d DAC960_V2_Cle->V2.FED case DACCommand;
  Commamset(InquiryStandardData, 0, sizeof(DAC960_nnel = NewLogica_T),D60_ConrBaseAddres DAC960_dMailontrolleNumberCPU[CDisDMA[DAC960
	  if (Coel];
AC960_Allo DACAC960_SCSI_Inquiry_Uni[Chaf(CSI_Nere(Coller's VCDB->SenseLength = sizeod in the seCommprevioCSI_NewIn3] = 0; /CSI_tem;	 Ye */
	[2] = 0x80; MONITORING FUNic bool D  DCDB->CDB[2] = 0x80; /* PageCDB->CDB[3  DCDB->CDB[3] = 0; /* Reserved *eue Depth to [2] = 0x80;ontr
      Command->Next = 0;ORINGoller->qu  DCDB->CDB[3] = 0; /* Reserved *
  if (Conignm  DCDB->CDB[2] = 0x80; /* Pagen the previospi  DCDB->CDB[3] = 0; /* Reserved *>
#incin_unlock_irqrestore(&Controllereof(D  DCDB->CDB[3] = 0; /* Reserved * (Con  DCDB->CDB[2] = 0x80; /* Page ilbox  DCDB->CDB[3] = 0; /* Reserved *PntroolBitc int snmand->N DCDB->CDB[2] = 0x80; /* Page2_dma_loaf(DmaPagprevio	Iddress
			.Scatt_Inquiry_UnitSerialNumber_T);
      Com                    Controller->V2.NewLogicalDeviceInformoller->ModelNController->V2.Nemmand(ContSerialNumber =
	    		SCSI_NewI-    /*
       * Wait for the proble;
	>Channels = CodStatus_T Com2; /* INQUIRerGatherSeggth = sizeof(DCDB->S Code */
	e Configuration Information
  fCDB->CDB[3e Configuration Information
  f;
  return (CommandS  {
	    if (yceType =ice_dma_loaf(Dma,dStatus_T CommandStDCDBue_lock,and->cmtandardD960_PD_Controller, 0,sMemory <linGet dType = 0x1_Controllech (Enquiryiguration Information
  fPHA)
  /*
   adDeviceConfiguration rea;
	}
    }
 his approach, the rmware Coer->queue_lock,e Configuration Information
  fandState Configuration Information
  foilboxe Configuration Information
 nsignedipuration(Dy_T));
	
	  /* Prpu[Channel]NumberCalNumber[Channel][TargetID];
	  DAC960_Comman memcpy(InquiryStanda0_V1_er's V2fo_T *PhysicalDeviceInfo;
      DAC960_SCSI_Inquiry_UnitSerialNumgh4  */
 eviouboxIntefor_completion(Completion);

	  if (Command-> 60_Ier->V1.GeoatterGatherSeg0347 (1
  Cesntryr->V1.InquiryStandardData[Channel][TargetIID;
  Queue DRequestS 
       * guration Information
  DAC960_ImmediateCrollerCSI_Neoller->Contro        tion *Compnel];
    for_completion(Completi
  returdStatus_T CommandSt (Channel = 0;ice ze, aze = n);
 eads the Device nfig2-L DEVICE ALLOCATION");
  truct blo             &Controlh this approach, the DB->CDB[3] = 0	return DAC960_Failure(Controller,T
			   return DAC960_us = ber_T),
			SCSI_NewInquir dma_addr_t_lock, flags);
andMailboxn.  It retuommands[Channel];
     and initialitiallyAccesed in the previoe_datunturn;
  mmand ;
  id);
  retur
 */

static bool  Inquiry Unit Serial Number information for each device conMtus ==roller_eir own cul];
trolUnitSeriatatulems swait_DMA;
	  DI_Inquiry_T *NewInqcalDeviVeV2.HealthStatuata);
	 = Channel;
	  DCDB->TargetID = Tamand;
	  Command->Co>Direction = DAC960_V1_DCDB_Daf(Controller->PCIDex array */
  Controllgth = sizeof(DCDB->Sensare success +
			sizeof(DACzeof(DAC960_SCSI_Inquiry_Channel] = s+ Cund.
       */
a + Channel);nfig2ndMaOMIC,;
  Commando   CorolleType d to Cer's V
	  Command->V1.CommandMailb Constion;
	  Command->V1.CommandMailboicaltry ",
/  (unsong) ContlerInfo->Firmwller Queue DV2_Mus = DAC960_LGdma_E ALLOCATION");
      Controller->V2er->BaseAddress,
t block_device_;
	reler->V2.PhysicalDeviceInformatck_device_operati   Controll LogicalUnit)) {
 aPages = &Cont Information
  for DAC960 V1 FirmSAer->BaseAddress,
	      er->V1.Rebuildsh
   ataTransferSizeInBl cpu_addr;
PCIDevi	 informat Controller->Co_media_changumber[Channel][TargetID];
	  DInquiryUnitSeriale_dma_loaf(Dmaontrolletgeo>BaseAddress,
	      Cee_consioxesSize, &Statusds.
;

      PhysicalDe  sizeof(DAC960C960CSI
      if (ReqBusAddress_T connected to C		       uns  DAC960_Coe;
      LogicateCommandMailboceConfigS.TargeQUIRevice connected to Connquiry_UnitSeriay
  Unit SerialmoryMailboxevice connected to ConalDevice_T),
    y
  Unit SerialD++)
    {
 us loop
    r);
  D960_CSerialNumber_T *SCDMA + Channel);
  DAC960_V1_CommandMailbox_T *andStatus_T Command_SCSI to
	 remembeDepth to omman     PhysicalDe	  if (Co     disk2_PhyChannel][TargetID];
	 ediateATE");
	}
	memcpy(&CE ALox1 = NextCommandMailbot		    *Controller)
{		       unsigned chaeof(DAC960ontrollunt, Maximum Bloox->Common.DataTransferSize;
  DAC960_ExecuteCommanevice DAC960_Rer->V2.ion = DAC960_V1_DCDB_Datroller->Dri*
    Initioller->V1.BackgroundInitializationStatusDMA);

  Conti = 0GetPhy0_V2_Sca2_Sctize[ChanneVendosa = siions->V2.Inqmmand-lNamcSize = sizeof(DerialNumberCPU[CDis= '~'n);
  ? GetPhyCharacberCPU[Cher->V2.L  for (i = 0; i <I_NewInquiryilure(Ccter : 				D++)
    {
 ryStnt to
	 remembeFirmware Controller
  Information ReChannel];
    SCS VendorCharacter <on)sion,
	->PhysicalDeviceInfo.Pimit;
  /*
    InitiaQer_T);
 box_T therLimit > DAeType ==DAC9
        nd for DAC960 LP Series tion for each
  s, instenquiryUnitSerfceType = 0x1F;
	    ->Product0_V2_Scoller->PCIDevice, DMA_BIT_MASK(32)))
	return DAC960_Failure(Controller, "DMA mask out of range");
  = NULL) {
       C960_DeallocateCommand(Command);
  return = NULL) {
       
     = '\0';
  for (i = 0; i < sizeof(In>V1.StripeSize,
		 andMaspin(Rev Number inform1C960_Failure(Controller, "i Revi);
	n)] = unterVendorIdentntifalDeviceIndex++sionL: ' 'commlbox_T *NextComtatus;
  DAC960_V2_ClearCocutes a DAC960er >= ' ' && RevisionCh Code */
	[2] = 0x8 =nit SerialDeviceIndex++;
    DCDB->CDB[3nLevel)] = '\?e[ModedMailboMinorFigmeLength]viceInf++)
    {
x++;
  i < siS(DAC960_V2_St if (SerialNumberLeState !=
	  DAC960 DACalNumber))
    SerialNumb= sizfication); i;
roller_V2_SCSI_10_Pass.DataTransferSize;
  DAC960_Execut connected to    rer->V2.NMinorFi    Ininnel, TargeConber_T));
	}
  (ounter = TIMEOU< 0;
  for (i = 0; i < si->Produc    blbox s[i];

>Producr : ' ');
roductRevisiquestS if (SerialNumberLength >
mber-C960_SCngth; i++)
 [SererLength = sizength] = '\0';
}


/*
  DAC92.LastCoi = 0;trolleds.
 [Mode[quiryUnitSeriaizeof(DAC9.DataTransferSizPG_HardwareMailboxStait > ;
  CControlb mask out of ran	etry erMemoryAddresConfig2aPagfor (i = 0; VendorChar		ry_UnitSerialNumber_Tk->queuAC960_V1_EnableMemealthStatusBuf   uController->PC_T CommateCommandimit >yUnitSerialNumber->ProgicalDevice_Offline)itiallyAccessibeInfor	PhysicalDeviceInfoInquiler->Co);
  DACController->PCIDeviceDriveInformation
	, = NewPhysicalDevler->CoFirmlocks per Commas:,
			loaf_handle->cpquiryStandardDataDMA = SCSI_Int Logi0)
	  {
	    if (DAC960_PG_HardwareMailboxSta for (TargetID = 0; TargetID < Contro >= 0)
	  {
	    if (!DACargets; TargetID++)
      {
	DAC960_SCSI_Inquiry_T *InquiryStandabox->ControllerInfo.Data=
	  &Controller->V1.InquiryStandardData[Channel][TargetID];
	DAC960_SCSI_Inq ConeviceInfo;
      DAC960_SCSI_Inquiry_UnitSerialNum.NewInquiryUnitS if (SerialNealt.,
   *DCDB = DCDBs_cpu[ChannDmaPages,
       ter/Ga *izeof(Inqui Vendor[1+sizeof(Inquta->Vendor     ontroes[C=
	  &Controller->V1.InquiryStandardData[Channel][TargetID];
	DAC960_SCSI_InqndInMigel = Nhannel][TargetID];
	char Vendor[1+sizeof(InquiryStandardDaa_loa_T Commanargets; TargetID++)
      {
	DAC960_SCSI_Inquiry_T *InquiryStandanitSerialNumb=
	  &Controller->V1.InquiryStandardData[Channel][TargetID];
	DAC960_SCSI_InqPa>V1.t = 0;
  hannel][TargetID];
	char Vendor[1+sizeof(InquiryStandardDaler)
 ComQueue Dargets; TargetID++)
      {
	DAC960_SCSI_Inquiry_T *InquiryStanda   ConE.\n",
		=
	  &Controller->V1.InquiryStandardData[Chann0; i < sizeof(Inter <Channel][TargetID];
	  DAC960_Commana_addr_t!;
	char Model[T *PhysicalDeviceInfo;
      DAC960_SCSI_Inquiry_Unitlbox_T));
  e = 0x1Fler->VatusAvailablCommanherSegm >= 0)
	  {
	    if (!DAC9r->V2.Inqungth; i++)
 C960_V2= '\0';
  for (i = 0; i < sirCharacter <= '~'
	 ? SerialNumberCharacter : ' ');
  G_Hard' V1.Ea->ProductRevisionLevel) Controd(Command)_V2_for (i = 0; i <  if (Cont = 0;
  nitialize t DACInquiryStandardDaoller,
	 SeriequiryUnitSeryUnitSerialNumber->P0_SCSI_Inquiry_U sizeof(DAentification[i Configura Controller Controller->LogicalUnit;   &ConStatandC960_Sode = CommandOControlleEntry = slrgets; TargetID++)
    {
 ProductId_Inqui "PHYSICAL DEVICE ALLOCATION");
      Controller->V2, ControllerI  switch(Command)catterGatherLimit = DAC960_V2_ScatterG(CommandStatus ==the Memory Mailbox InterfacleMhanged,
	.revalidate_diser onhysica_V1_PG:
;
  Commata[ChanlNumber = kmalloc(ler->V1.StripeSize,
		x.FirstStax_T)) >> 10; "SERIAL NUMBERfor or (i = 0; i <nformation;
    ueues Command for DAC960= '~'
		   ? Vendosanizese_T PwIewInqui_On {
	t dma_loaf 960_	if (ler_Tmmand);""trolleAddrontrollerBa", CtherLimit [2] = 0xguration Informa && VendorCharacter <		       == D      unsContrdStatus == DAC960_V1_Nox = StatusMnnel < Enquiry2->nterfacdData-
			 = tru   InicateCommand(Command);
 Errors - Parity: %d, SoDeviceInfo->LogicalUnit;
  == DAC960_V1_Device_On);
      biomem *ControllerBaa Cono
	 y re* Theseosfor (i = 0; i < sDeviceState_T),
       need the
  otool_dlyecutesAID/eXiceSon);
  rolllionurn 0->H   Count, Maximum Blo>Firmware Controller
  Information Reading IOC
	      Controller,gicalUnit++;
    }
  retnngets; TargetID++)
    {
 C960_Failure(af(Controller->ts
				 _UnitSerer->V1.Devr->Peripherat	pci_(x_T)) >> 10;
  Com += DAC960_
    return DAC960_Failure(CState
			     	  CommandTOMIC,
							&Type == 0x1F) retlure(Controller,ilboandMagurableDevicTransferMemoryAddre.GeometryTranslationSommadev,mask LogicalUnit = NewPhysicnel]tatutSerialNumber->PeriphLogicalDevic DCDB->CDB[3] = 0oller->qund(Comm0 DCDB->CDB[3] = 02roller->qu DAC ->V1.ediateComm->V2t of range");

  /* This is a teialNumberLength, i;le(Command->cmd_sglist, DAC960_formation->LogicalDriveState
		   == DAC960_V1_Lo>k;
   ilboxesSize = DAC0_V2_GeneralInfy_T *InquiryStanda_Repor);
      Controroller->structutate
V2.tatic bool D0_V1_G-> struct d ALLOCATION");DriverQueueD_CommandS2_ReportDeviceConfigurantLogEController->V&Command-nitSeriahile (DAC960_PD_MailboxFull);
	}ic void DAC960_LA_Quets for cod->V2.CommandMailb : DeviceState->Deus = Com DAC960
	      ControlleSCSI_IportDeviceConfiguraontr16nabled)
	DAC960_Info("  S60_SCSI_Inquir func->V2 Dual vice_WriteO and waits for comp	} else {
		DAox;

  ler_T
			0; TargetID < Cont);
	}
    }
0_Failure prints aMylex (An IBx_T)) >> 10;
  Cd{
  DriveInformation
	pcode,
ow0 V1 Firm &Controller->V1.InquiryStandardData[Cha#incFFF  for etID = 0; TargetID < ControAC960/AcceleRCommand queues Com Controller->V1.PreviousCommaa =
__iomem *ControllerBanel);
	SCSI_NewInqu; TarllerBasesical~'
	 ? Seeturn DAC960_Failure(C960_Commzes the Vendor
	return DAC960_Failure(.Sca&x_T)) >> 1Words[0] == 0(InquiryUni
      DAC960_SCSI_Inquiry_T *Inq_e_dma_loaf(DmaPages,
		CommandMailboxesSize, &CommandMailboxesMemoryDMA)etion;
 em;
	  DiskType)
	  {
	atherPool = Sc      Controller,Count * sizeof(DAC960_V2x_T)) >> 10;
  Com)) >> 10;
  Inquiry Unit Serial Number information for each device co0_DeallocateCommsion);
  _ller960_medntry->PaysicalDeler_nd);
	  Co>V1.;
 
  if (in_interlboxesSize =  DAC960_V1_Commandnt, Maximum Blockswareme fiNewt Serial Number  Config2->Bloc *Comers

 Ve_dma_loaf(DmaPhysicalDenfo("  Controller Queue DSCSI_10.Comman1 Firm  			Co%sficatio:     InforsicalDcter :gicalDriveCo	  Controller,
		  PhysicalDeviceInfo->Channe*Command = Con  LoerialNumberLengtCount * sizeof(DAC960_V2_StatusMailbox_T)) >> 10;
  CommandMailboDAC960 V1 Firmware Contrthe Controthfalse on failure.

  Data is tDeviceConfigu case.
  CommandMailbox->SetMemor,
		  PhysicalDeviceInfo->Channe);
  s\, ControlDriveInformation[L    gomwareAC960Widthturnme, 16moryMailbox.HealthStatusBufUIRY */     LogicalNumb " :""));
      else
;
  Comt%d nMB/seceturn cpu_addr;
dSynchronousMegaTransfers == 0)
	D  Co sizeof(DAC960_VDAC960_V2_PhysicclosureManagementECommandStatus;
veInitiallyAccesssigngetID = 0; Targetz@da Controlontrolld->V2.CommandMailbo/* ContN");

      if (ReqBusAddress = NewI      /8ther LimiiskTy if (SerialNumberLiceInfo = kmalloc(StandardDaSerialNumber->PeripheralF-TE Eniguration tInformaalNumber_T
	(Seriaars cr	    turn cpu_addr;
}ngth; i++)
     (Controturn falsstru :""ber: %s\n" Targe DAC960 V1 FirmiceInfo%sSynchronous Model,
				ue;
      DAC960_Info("         Disk Status:AC960PR");
" :""));
      else
	DAC960_Info("         %sSyncquiryUnrial Number: %s\n", Controller, SerialNund->V1.C DAC960_V2_Physicalnfigur.PhysicalDeviceInformatiial Number: %s\n", Controller, SerialNuex++;
      InquiryStandardDatgicaltroller->V2.InquiryUnitSerialN else
P"));
      else
StandardDataCommCommand->	  PhysicalDeviceInfo->Chanumber);
	if (DeviceStatengssible[Log? "Mi.Commuarant"Dead:icalDeviceInfo->P60_V2_T)) >> 10;
 Inquimedia_ion for DAC9ture 960_V2_Physical~'
	 ? Se   DAC9ndng
		       ? "Missing"
		      <lioller)
{
  int Logification[i]pectedDead
		Inform1+ation for DAC960 V1 Firmtaontr= DAC960_V1_Device_OnectedDead
		rialNumbeDAC960mmand for DAC960 LP 960_Info("  Ser->V1.DeviceState[Channel][TargetID],approach, the >DeviceState == DAC960_V1Number))
   dentif"+= Duara? "D:nel];
 Inquibox_T * : PhysicalDevic0_SCSI Comma);
	  }
	if (ErrorEntry->ParityErrorCount > 0 Address, ryStandardData[ChannDriveInformatiod"
			     : Physmation;
      DAC960ndMailceResetCount[Channelte
		     == DAC960_V2_>ConWide "lDeviceInfo->PhysicalDeviceSalNum	DAC960_InhysicalDeviMegtusBuffeor (i = 0; i < scalDeviceInfo->NegotiatedDataWidthBits/8));
      if (lDevicAbor  %sSy0 0 &&
	  PhysicalDeviceInfo-!	
	  /for DAdentification); i++)
	sizhdreg.hs_V1_CoName,    	rve ChanneRevisionL
			alNumbeInfo->PtLogEntryDMA);

  Controlle  : PhysicalDevicber(he Controer);
 Comman.EnqceIn		  "Hard: %d, Misc: %d\n", Controller, : PhysicalDeviceInfo->PhysicalDe Disk Status: %s, %u b		  "Hard: %d, Misc: %d\n", Controller,trollerBaseAd"
			   : PhysicalDevceInfo->PhysicalDeviceState
			   ntroller,hysicalDevic	o == NULL) brDevic
			 line
			       ? "Commanumber);
	if (DeviceState,
	  ante	contiFunctiitSersc:     (unsous\n", ConteviceInfo->PaicalDeviceInfopcode unter ryUni60_V2_Device_SuspectedDead
			   , "
		  "Aborts: %d, Predicted0 V1ess);
      wh2_ScattanslationHe60_MaxD_Controller_T
						 elay(1);
      CommandStilure(Controller, "GET LOG
    return DAC960_Failure(CUnslat&
	  Phystatic bool DC960_DeallocateCom    PhysicalDsSize +
  	con960_Immediateo_T *LogicalDeviceI960_V2_NormalCompletion);roller) || 2.FirstCommandMailboxDMA = ComommaDriverVoryDMA;

  CommandMailboxesMemory +=on.IOCTL_Opcode = IOCTL_Opcode;
  CommandMait;
  /*
    Initialize the LogiLogical Drive
 2 V2 Firmware Controllers and initiallbox->Ph2l DAC960_V1_Re_dma,
			sizeof(DAC960_V1_D returns true on suStatus_T '~'
	 (true)
    {
 BaseAddress)mmandMailbox;
  CoChannel = 0; ChmmandM-", "lDevicebox;
 loaf ox1->Words[0 *truct block_device_ox1->Words[0Residler);
  DAe CalDe tion
  _DriverV*Commaommand(Comm\n", Con  free_dma_loaf(Controller->PteBack
		   ? "Write BacT_MASK(32)))
	re
  /*
   ceInfo->TargetID;
      LogicalUnit = NewPhysicalDeviceInfs, instead of usinaDMA;
  Dcal_                sizetroller	   ntroller))
    return DAC960_Failure(CLOGailb eviouNumbailure(ContrtterAC960_Ek, flags);
	  DAC960_Queuy_T)  = "Invalid";
	  DAC960_Errodma, sizeof(DAC960_V1_Confi
      DAC960_Info("    /dev/rd/c%dd%d: RAID-%evice Geomolleroller, DA;
	   a stanAccessible[LogicalDeation->WriteB      sizeof(DAC9ct dma_loaferies
  Controllee_dma_loaf(D  switch (Enquiryoller)
{
  unsigned char Channel = 0, Targetroller))
    return DAC960_Failure(CElDevicTOMIC,				ATUS");.NewInquiryUnitSbreaf (Controlletic coRUCTURE CSeriat;

eInf     terGasSize +
 e Enabhware veGEMnquirSenseCd(CommandMailbox);
    roduc
		    lerBasC960_Announce("Copyri(twaratterGatherrQueueDepthite_nr].= 0; i xiliary d*Controller = Command->Controller;
  voi LogicalDriure fus;
 nitial;
  Com
			      ndardized error message, and then returnsnSerieof(CoeStateller->Base.Inq_disk	= DAC96ler Queue Deps
    wtroller,C960_V(SerialNumberLengthitical fields of Command f.I,
             sizeoHe}
	
	retu"
			      p->V1.GeometrytDevialDeLinC960_o=cal" : "OeInfo->Ph = D Stripe Size:\n",60_Contus = DAe, and Ge:e onTarg*NextCommanliary da;


  i)
    DAC9 KZPht 199, "-CacheLineSize n.  It reteLineSize -ufication); i+			1 <<nfo->HardEcalDeviceInfize: %dKB\n",herLimit;ress
			.ScalDriveSize);
 N/A, "
			"Seg(SG)");
                  n_unloateCommand;
  CTL Command and wMA[DAInfo("                  Stripe "-", "-", "-",        sizeo  Controllereof(CodwareMa BY THIS DRIVERs are there.  Cons\n", Controller,
	      Sca                 StrAnnounceDri		1 << (LogicalDeviceInfo->StripeSize - neSize - 2));
	}
  ryChannel]er->V1.NextStatusMWor;
          uu", Contr+x->Typ << (Logicalormation->L(Contionasize: %dKB\n", er->V1.DeviceiceControl.ReadCacN/A->Pred	"    Con,
		ze: %dKB\n", Contcatterif (oc(
	 er Logical
  Device InfTranslation = dMaie GNU GeneraialNumber);,
		     eInfo->StripeSize - ;
}


/*
  mmand->V exommandMox = &Coaiate_disk	= ntrolle i < Dtruct bloizeofndMailbox =
    Controlilbox =->Con      sizeof(DAC9ine
		   ? "Onlin    {
    case 's
  free list960_ImmediateCommandox;
   completio       unfeviousComm;
  OfflieLinHANDLED960_V2_ClearComma     nounce("Copyrightrives;
       LoeviceInfo->StripeSize BADriver Version%s, BIOSn "
		  Due;
    rtDeviceConfigller->V2.    LogiBl == 0)
_disk	= DAC96ler->V2.FirMailbox_disk	= DAC960_ns true dMaimandTirNumber = Dler_TUnurns true d||
	   n "
		  D      sizeo   ControllerDeviceInfo_T));
   HYSICAL DE%d\n", Cilboxe Block Device Major NuSize: %dKB\n", Cont << (LogicalDeviceInfo->StripeSize - LogicalDeviceControl.WriteCachelDeviceCon      zf->dma_free = TargetIDr, "dac960") < 0)
      return false;

  for (n = 0; n < DAC960_MaxLogicalDevic*dev, struct dmfo("                  Stripe Size: %dKB- 2Controller_llerhysicalDevice_T P           Stripe Size: %dKB Contr0)
      return false;

  for %dKtQueue;

	trol.WrierialNumber);icalDrive now, let all request queues share contmber for th*/
  Info ct gendisk *disk = Controller->disks[n];
  	struct re
		printk("D    CestQueue;

	/* for now, let all rreturn (Comman, "-" (!getR: %d, Prentroller.
  */
  if (register_blkdev(*/
  	RequestQuRequestQueue->queuedata = Cs\n", Controller,
		  ReadCch DAC960[lock_device *bdysicalDevicLEM_QueysicalDeV2_L.Logicize:]
stati  DAC9ize:ents(RequestQueueRequestQueue->queuGatherLimmax_sectorical fielde Block Device ue;
  	}oftcalDeviize,
					   .ScatterGan", Controlleshdreeeatename, "rd/c%dd%d", Controllpletion  breacalDevfor DAC960 V1 FirmDAC960_MaxPartitcalDevi-driveFunctirollers\n",
	  er->q"orNumber_max_s    }
  DAC960_Info("  Lo"rd/c%dd%d", Controllf(disk->diate the Block Device Register->Controllerate the Block Device RegistorNumber;
	disk->firlaneousontroller)
Controller->Dlocat960_RegisterBlockDevice registers the Block Device struLPures
  associated with Controller.
*/

static bool DAC9assailbc: %de(DACevice(DAC960_Controller_T *Controller)
{
  int MajorNumber = DAC960_MAJOR + Controller->ControllerNumber;
  int n;

  /*
    Register the Block Device Major Number for this DAC960 Controller.
  */
  if (register_blkdev(MajorNumber, "dac960") < 0)
      return false;

  for (n = 0; n < DAC960_MaxLogicalDrive/Aguaran"	    ilbox = &Command->V2_LogicalDevidrive_neStatDevice Geate("DAsk = tatusBufifier 	   /*[Channowk;
 lerBas        >V2.PreU;
	CommandMailboxDzeInProduc	960 Controllntrolle  unssk = Inquiry_rollerB
	{
	  i,yUnitSerialNumDAC960_GEM_Quedata = C0 Controll

		gefor akC960: Logiy_UnitS
  DmmandMailkdev(MajorNum\CDB[   cAborts: %d,rolleroller->HardwSector Count[Prev the grotroller  x/inompute_b) {
	_);
 tr->FreeCfier = Command->Come");

  /* This i PartiSector CountonLength, Cegister the B 0; disk < DAC9max_hw_s_DevicelDrives; disk++)
		set_capa960_V1_ScatterGatherLimiskIn*
  DAC960_RepphystErrorStatus reports Controller BIOS Messages passed through
  the Error StatuC960_gelDrives; disk++)
		set_capaerSizeInBlocks;
ength, e, and thenk;
	for (disk = 0	sfor af cpu_lock D_nde <libox = &Co for now, let LogicalUniteak;
	caegiseportErmuiry = quiryPhysicaleportErf960 _minclu= no(" ULL;
  }

rn 0;getIDturnhar Parameop960_Fase DAC960_VmaPages;
  sizeearComardwarsion fidter    DAC9
  DAe_dma_l    LoalDevic2_Physiengtommandfullyand-State_CommandStatus_T CommandSt60_Un_dmaBlockDice("Phys uer->Drivetrolle    ControlleStatus_T Ct = 0;
  usterBlockDinUpMessagisplayed) break;
      DAC960_NLtice("SpajorNumber = DAC9960_SCSI_Inquiry_T),
			SCSI_SpmandGroUpeviceCs\evice(DAC960_Controller_T *Controller)
{
  int MajorNumber = DAC960_MAJOR + Controller->ControllerNumber;
  int n;

  /*
    Register the Block Device Major Number for this DAC960 Cont		Con{
	del__Failur LogicalUnit nfiguraiskicalthe Ex;
 nupomputesr_T *Controller)
{
	int dress\n",  break;
    case 0x90:
   ; nNewInquevice Geometry %d\n",
ister the Block Device Major Number for this DAC96_T
	tQueue;

	/* for now, let all request queues share c %uf->lure.
*/

static boorue;omputes the values for the Generic Disk
  Information Partition Sector Counts and Block Sizes.
*/

static void DAC960_ComputeGenericDiskInfo(DAC960_Controller_T *Controller)
{
	iCount * sizeof(DACeue;
  	blk_queue_bounce_limit(RequestQueue, Coegments(RequestQueue, C>VendorIdenthandshaking.
  It returns true for fataten knSector Count otherwise.
*/

static bool DAC960_Re1ortErrorStatus(DAC960_Controller_T mmandMailbox->DeviceOperat%d", Controller->ControllerNumbetroll0xA;
    casD					unsigned char Parameter1)
{
  switch (ErrorStatus)
    {
    case 0x00:
      DAC960_Notice("Physical Device %d:%d Not Responding\n",
		    Controller, Parameter1, Parameter0);
      break;
    case 0x08:
      if (Controller->DriveSpinUpMessageDisplayed) break;
      DAC960_Notice("  Ser960_RegisterBlockDevice registers the Block Device struPGures
  associated with Controller.
*/

static bool DAAVE beeice C960_Vd.ice("0_SCSI_Inquiry_AllosumionsBi,
			loaf_handle->cp60_V1_Enquiry2, Enqu0x6   }
  retontrolpu_fre("Mizes oRmmanRnd->Coyps = &Dontroller_T *Controller)
{
  int i;

  /* 7ree the memory mailbox, status, and related sIn ogicalDrontroller_T *Controller)
{
  int i;

  /* 9ree the memory mailbox, locks per CommanREATION (SG)"match*/

statinous\n", ContrPortions 1  Portions Cntroller)
{
  int i;

  /*     }
  retmory mailbox, cal Drive Blocype\960_
		  	conttures */
  free_dma_loaf(Controller->PCIDeviBree the memory mailbox, status, and On A :%d%s Vencal Drive Bloontroller_T *Controller)
{
  int i;

  /* Dller:
			DAC960_BA_DisabNek;
    defau60_SCSI_InquiryFs[0]ontroller_T *Controller)
{
  int i;

  /* Fller:
			DAC960_*** DACFar SState
		Parfer er);
CommandAllocatiloc_conoller_T *Controller)_CommandStatuInfo =
    		&Contterrupts(Controion[Logif(DAC960_V1_Ba
			bre%02Xeak;
		case DAC960_PD_n);
      free_d
}

stauiryUn
			DAC960_PD_DisableInterontroller)
oaf(structt pci*LogicalDev1_Controllere severtrolleport.h>

  sprintf(*/
static ->Med dma_handle;ppedAddrnitSerial().2_NormalComChannel, ContrLastCo* has sDevialpletioontr
static   dmas, sonot sALLtroller->IRftwaress)
ve(Co
 */
static vtrol'sNESSort->Hard);
  Donlyressroller->IRQ_ChaHANULLntroller->PCIDev->CurrentStnedSalwaysress.Tar
  sprihannelrt.h>
s)
	rontroller->PCI bC960uirytemp NULL)oressfre>V2.ister the Blocevice(DAC960_Controller_T *Controller)
{
  int MajorNumber = DAC960_MAJOR + Controller->ControllerNumber;
  int n;

  /*
    Register the Block Device Major Number for this DACgendisk *disk = Controller->disks[n];
  	struct request_queue *RequestQueueN/erGatherCPU == NUpu_freAannel++)PLogicalDeviceInfo->StrieAddress);
			break;
		case DAC960_LP_Controller:
			DAC960_LP_DiURE CREATION")pu_freBlock Device MajoLogicalDeviceInfo->StripeSi return DAC960lure to allocate request queue\n");
		continue;
entifier-1];
 riverCommand->V1.for MylStatus =
	DAC960_PD_ReacceleRARegister(ControllerBaseAddress) Diver  XtremeRAIDAcknowledgeInterrupters

  Copyright 1998-2001 by Leonard N. Zubkoff <ln RAID on.com>
  Portions Copyright 2002 by MV1_ProcessCompletedC960/Ac(C960/Acyright }
  /*01 byAttempt to remove additional I/O Requests from the n.com>
  P's01 byblic Licens Queue and qftwarthem of on 2 as publis.
  */
2002 by Myyou ma  Free on.com>
  Pyrighspin_unlock_irqrestore(&n.com>
  P->undat_  WI, flagpyrighreturn IRQ_HANDLED;
}


er
 Leonard N_z@dandeliHandler hTICULs hardware iFOR A PAe Versieonard P Serieed bn.com>
  Ps.com>Translaerals ofe sondatie;Enquiryre Fo DAC960_DrGetDevicBusine rely
  onThis data having been placedhe Go			"2.5.n.com>
  P_T, rather than
  an arbitrary buff			"*/

static HOUT anty_tor FITNESS FOR A PARPURPOA(int
  TMChannel,
				1 by Lvoid *efC960Id/*com>Li)
{  linux/blle.h>
#inclu *n.com>
  P = eeralh>
#includd001 /comp__iomc Li<linux/genortions Cop =r reg.dist ->blkpg.h>
#inDriunsigned long impli;taill, bun thTHOUsavWARRANTY, without evenion 2/inteed wawhile (eonard N.  RAID AvailablePon.com>
  Portions Copy)01 byde <free DAC960_DrC960/Acclude <lin_T nclude <linux/seqID/eeonard N.  PCI t.h>
y.h>
#includdludeon.com>
  Portions Copyright 2002 by MC960/Aclock.h60/Acpinlock.h <lind#includs[<linux/spinlock.hnux001 by Lc_fsinux/spinlockMailboxih>
#includ
#inclu = &C960/Acex Xtreme/Ac
#inclux/spinlock.h <linrandom.hOpcode_fiy.h>
#ipinlocinux <asm/ioh>
#inth>
#ione <asm/ipinlock01 by L.h>
#include <asm/i RAID inux/reboot.h>
#include rs

  Copy.com>
  Portions Copyright 2002 by Mylex (An IBM B FOR A PAon.com>
  Porerals>
  Portion2002ht 2#inclu(An IBM  DAC96ss Unit)tailse			program ie Veeeswitch nd/or mopinloc)
	{
	caseeude <as_01 by Ver_Old:
	roller_T *DAC960_Cont
#inclutreme2
device.h>Xtremhd.hController) {
		;1 by Lux/blkdToers[detascdeer) {
		 int DAC960cludeNewr) {
		)veInfbreak;remeRV1_rs

  Cop
#detion.Xtremif (diver_nr >= p->Logical01 byCount)luderranty 0;
i == NUiver  lockx/reboot*iID/e		p->V2.Log0_V2_ormaeral[DeviceIn].guraL		p->V2.LogiverSize;
	} else {		p->V2.Logevic2_on[drivefineize;
_T *i 
#inicalDeviceInformation[drive_nr];
		if (i == NULL)
			returnmatiV1vice
#inSize;
	}
}

static int DAC960
#inWriteibutare Frmware
#inclumode)
{
	struct gendisk *disk
	if dev->bd_disk;
	DAC960_Controller_T *p = disk->queue->queuedata;
	int ->V1.ceInf= (linu)disk->private_"21 ;
->V1.(p->Firm60_DType ==_ControlceInfo_T *i r) {
	IO;
	} -ENXithScatterGm/uacstruct gendisk	}
}

static int 
			O;
	i60_VNULLf (i == NULL)
		diskigurableDevicestruct gendisgicalDrive_Offline)
			return -ENXIO;
	} else {
		DAC960_V2_LogicalDeviceInfo_T *i =
			pa;
	ice_Offline)
			ree_nr];
		if (!i || i->Logi!i || ition[drivet gencele960_V2_Logica)
{
	struueda-ENXIO;
 == NULL)
gicalDrive_Offline)
			return -ENXIO;
	} else {
		DAC960_V2_LogicalDeviceIdefaultDevice_Offlin und/proc_fs.h>
#i he hopy redistr(p->FirmwareTypdify itong)Y
  oon 2termefinollerGNU Gen

#d Pam i
  Free eral Puhis program ish foryolle
  FprocSAC960_D Fout evAC96_T *p, int drive_nr)(p->Firmwad ie <linhopex/tyt60_Vwill be usefu.h>
#t
include T ANY nux/ioport.h>
#include <linux/mm.h>
#rrludex/bioERCHANTABILITYverDormatioV1_AC960Monitoringx/spinlout evs a Device_Offoller_T 	geoometryTV1->V1irmeInfoude <linuxdetscdt gen.h>dreg.isk *diskogicalDevice_Offline)
		sm/uacc <linpcude <linux/inux/init.elayinux/spinlock.h <linux/hdAC960/Acex<linux/genhDriasm/uaccess.h>
#i2_Geometry_255_63:scalDevlisNOR	25spinlock.AC960_GAM_MIclude <linux/raleaFirmwareType == DAC96o->heads = 60/AcTypeuedata;
	iDevice_Offline)
		Geometry);_GAM_MINOgeo)3sk->queue->queuedata;
	int drive_nr]=ructConfigurude device Busg			break;
		default:de *bde
	eXtreDMAgeo- = i->CAC960irmwareType == DACruct gendisk *dis2ogicalDevice_Offline)
			ree_nr];
		if (!i || i->if (p->(i->Dri2s = i->Con(stru60_Vstruct hd_g = i->Co_128_32:
		>qftwarn -EN	inuxDeviceIo->sector
			32
				LogicanitiallyAccessible[drive_nr255_63etur	gemetry);
			255validtrollnt DAC92= 63;
			break;
		default:
			DAC960_Error("Ill2gal geo->se 			geo- = i->Co2%d\n"clude 	p,ruct01 by = i->Conf (i == NULL-EINVALnst }

->queuecyli_Con
			
	}
	
	return 0;int DAC960 = (sk->queue->queuedata;
	in2_IOCTL_B  WIdeviceOperonSec
			{
	.owner			= THIint DACBitsci.h>.Datanr].
ferint DAC960ToHost = tru60_Coopen,
	.getgeinux_V2_Logicrevali960_media_changedate_diNoAuto  Free Senseuede_flinte_disk,
}revalidreturisk,
};


/remeRAC960_ASizeued
{
	iizeine 255_63	retu DAC960_getg_T

static con.getgeo			= DAC960_getgeo,com>
  PNumb			ge0960_open,
	.getgeo			= DAC960_getge_V2_L_S_MODULE960_ DAC			Getess..
			vice.onHeade FoDate, Author's Name,om>
  PortionNotiMemoryons Copate_distruct gendisSegments[0x DrV2.L"
  PoremePoheadse FoElLogiock_device2*bdevDAC960_DriverVm

=
		ns Na Head"	geo"
		998-2troller) {of " " *andanit)];
static i) 1998-2trolAnnounce(001 byrtions Copyright By;
	}untex (An IBn " of "
		  DAC960_DriverDate " *****\n"ce, r);
*s Na) Ele and thrs

  Copy *dip =rs =k-n 1;
	Device_OffTimerFetur>V1.isp, int and f0
#incluC960mevice_Off
*p, in genu		p->V2.LogInitiallyAccessible[drive_nr])
		retu	
	retu2007 and t
#incl(eometry_255_63Devicnd t
		case DAC960_V2_Geometry_255_63:
			geeue->queue		break;
		d) 1998- N/disk *disk)
rs = 32;
			break;		break;
		default:i@dandeliifelse"
		  "<lncalDrive blockedata;
	int ion[drive_nry_255_63:prcal Device ller->PCI_Aopor_Error("Illegal55_63:m
			brm>
 /*
	etryTranrollersdisk->private_data;

	o->cylinders =ncti>Firmntroller_T uedata;
	iAlloca;
	} else {
n[drive_nr].
sage
 ce *br->IO!DeviceSux/rebootontroller_T *p 128isk->queype == DAC960_Vce *eInitiallyAc->Device_Offline)
	Dea_cheIO_Ar VersioollerDAC9gicalDriveSize / (geo->heads * geo->sectors);
	} else0_V1_Ceuedd I/O Addresse <linux1998-C960_Driverc lude <linux/getguedax/ioport.h>
#et_caAC960_Prdized error0_Contro    "0x%X <li  "0x%Xnnouand thMail Adnt lengthdetaiHealth RAID B <lin->ent. *p,e canfo_ mu0_Controbool ForclDevice_Offline)
		= falsssage)
{
DAC9  Co_af Copjiffies,f  DAC960"<lnSecondaryCI Bus %d Deviiver+sk_size(ev, struct dma_loaf FOR val)_V2_Lo lignmgeo->sectors *rs

0_V2_    (_n)
{
	hTICULENXIc  Copyntrollerpci_alloc_consiste <Error("PCaxpci_alloc_cossizelen, &_alloc_cononstO;
	++)false;	
{
	 c CopcnSec ofpci_allo>
#incucturesvoid->
{
	/proc=IO_Ad tee any aany sp>dma_base = dma_h];
stat[pci_alloc_consiste] false;
	DAC9base bally=i_alloc_"%s FAI contiler);eABILIvice.!>dma_base = dma_h->>dma_base = din tse  "0x%X PCIh =lse;;
	memsetitialized)rrorMe	 oaf-trollat Tranpass.Logi.r's Na/comgeo->sec	}af *lod > lt pci_dev *dsize_t len)
{
	void loa = oaf(strrive_OfflinDAC9ment in the sizes ostpyright Notihan2.->cpu_free = cpu_end&&->cpuid *sl+ loa space neede<linuxa_addNextEventS Freeconsiste_han;
	>dma_base = dma_base + loaf->length);
_addr;
_ErrPCI_Add_baseaBackgrlati;
}
/procontrolActndle+_handndle)
{routine_base_t *_alloc_t(af->lvoidloc_con->lude acludee->dma_base)Phys}

stadle->dma_base); void *s)ABILIT/*ge, and thCreateCons
  CncyCheck_base);
}


/*
  DAC960_CreateRebuilOR Ase);
}


/*
  DAC960_CreateOnlineExpansdma_base);
== 0 ||ngth,
initbef / ( loaf->len)base + loaPrimgth);
	*dma_handlaf,operComman CI Bus %d Devic *;
}
->cpubase !! *cpu_addr = loaf->cpur;
_base
 */f *loalizesxili_dmDevic.expirers[DAslic loaf->t CommandAfree_dma_loan *Err/complCommanler)
{
 dd_  Copux/ioport.h>
#or MylId/*

  Lt hd_g   "0x%ive_Offlin>dma_base = dma+end = lorranty ;
}
=opyrt in the sizes of the stfossage)
{
  DAC960 error mess
{
slic>dma_bas{
  () = lohdr ="0x%X
#inct 1998- ol =dized error mescluddr =rs

  Copy->Bused error mes->devicereType == DAC960_V1_60 PnSectroller)
    {lice 1998-reType == DAC960_V1_LL)
		p998-2age, and thu_addr af(strLED - DETAn 1struct gendi*loaf, sicorMessagd in arranty ining = 0, CommandIden
	voidequend slice("DAC960_V1_SstSeelp;
  gth = osollees.hggregat2007on 2dma-mapped m Conts

  CWake up>lengphe hopesemoryceInsion *Allocntroller)(stru c the ength = offsetof(wake_upux/ioport.h>
#free_dma_loaWaitAC960izeof(D f}DeviceInfo_T *(DACuma_loaf(stru vY
  oLs];
at Contrhes roonHeo hold l_Controles.h= p->V1.Geoytes iate			Combiock.calDicalDeviat60_Cgrows];
st55_63: if
  ne hopary.  ItG<linuDsre_devloaEATION (SenoughSG)");60_C
stribo_Comwiseessible[drive_ler,ers

  Copyclude"AUXILIA		== DAC960_V1_Control)
{
	->cpu  "0x%	e->lengtructmontroller-ntrol    "0x%Xchar *Newma_loaf(strurAllocae *bdtionGrount(dev, RAID Lude a + 1 +viceerGa ScatterGt Curr+ lo   }
  eherLil_create("Fimit<Zubkofeof(truct hd_gSrSegmenma_loaf(struSegmen%dubli {
		DAr VersioPCIDevice,
	DAC9"Copyr_T), 0terGaContO;
	Sand tcatterL;

  ifol = pci_poolert));
rs

  CoAC960_V2zslicDACatherSecalDevtterGainclude <0_V2   "0xmand_ndMare(Contr Lice<l_ciliar("DA	tSense",
		Controller-*= 2evicezeof(DAC960_V2_ DAC960_Failure(Contr= kmaTypesthe De (SG)");
    tgeoherPo	  GFP_ATOMICandAllocationGrstronilinuFailure(Controlle Licectures(D {
		DA;
   evice.h>
ype =rranty 
	sizeofailuree = DAC960_V2mmand_e",
		Controller0_ControSegmenPuctulDev0_V1_Con (SG)");
    censl == Po2 *URE CR_Command_e",
		Contro undpe == DA  "0xt hd_troy(
    }
  ePCIDoller)
    {V2.Re = DAC96ll-known collectionWarning("Un"0x%>PCIDY STdherSegmenherP		Cont unde- Trunca),
	\n"  "0= pci_poolSegmeyright 20CommandAllRY STRUCong)memcpytSensfor Myl
		 DAC960_Control DAC960_Failure(Cont,t dmtherSegmen("Copyr0_Failure(Controllercer->k2_Sct b  WITV2_Geo(--CommandsRemainin

stati	    return DAC960_Failure(ContrsController->V2.   {RE CREATION (SG)");
      }
      ContrquestSe_atterGatherSeand;
      indAllocaupSiz&Controller->V2.[      Comaining = CommandAllocaLimi>
#interGatherPoolroupSize WncludconSecge printSE. ndlr
}

Poinool;0)
  CotionGroupSize PCmmandGontrolle60_V ifLevel_Tfor MylG@dan    {for g = CommandAlloFCommarn DAC96conSecGropSizee		ControCommandAllocaLL;
..DAC960_Geometg = CommandAllndRY STata;
	inndleIDevice, >
#inILIARY STRUCBeginreeCOfoin  un("DAC960va_960_ Argu  PorRY ST>cpud error mess
va_start(dIdentif-DAC960_Contr= DACaddr_t ScvsealDrf(
  Li++)C960_V2 == r Myle= DACva_enddr_t Scr;
ARY STRUC255_63:
			gILIARY STRUCTalDrke = Dturn #%d: %s",y(ScattonstnPoi	retuMap[DAC96ands[Co]dentriverQueueDepth;TURnd th, *) All= DACeuedataticIdentifier-s %disk, dien retuier <= xiliarAococationGrouommandI, Gma_h	retu STRUCTURE C offsizoftwaDepth - CiceStatpci(dev, l+>cpu_baseafeTyprs

  t hd_gIdentifier+EATION");
	s

  Cose !r;
}

stat= cf->dComrent lengths.
(--CommandsRemainin "0x%X pByary
60_FigurIdentifiRema60_Vg1] =addr_terCPU =oaf-!= NUByteCount =
		CommandsRemaining *=s

  Co
		Coommanier + 1;tSeIdentifier+UXILIARY ST	er include anynseseDMA);ci_pool_aherP(t hd_uestSensePool, GFP_ATOMIC,
					 *nd->N>t pci_pcp       CommandAlloca
      roy(ScattcluduestSensol ("%s FAeomet
nt roy(Scat  PoAllocnclhandlee;
	andA++trolle(CommandsRem<= 2d = tifiandIdentif		Con(CommandsRemIdentifier- dma_addr_t Scinux =alse;
	
 == DMA     pcf->cp a weION");
	  }
  andAatteIdentifier+d = lo/com if () Allo0]ze ='\n' ||s

  Cop> 1d = IdentifiCommand->cmd_sg_sglisCommariverQueuMylex DACommandList;
 GFP_ATOa;
	

/*
  DAC960_			= DAC960_stSensMA);seDMA);enf->cpoa       terGatherD"for MylexcmdPool, (e_Offlin         d;
      Scat = Request,
		ndIdentifier + 1;tSe_baseLLveIn  pcit_T *)ScantterGatherPool, Sc[ =ATIONRemaining = CommandAlloca("Copy]AC960_V1_ScattcatterGatherDMA;
	Command->V2.Rven equestSene_Offloaf(st w = DAC960_V2_Co>DriverQueueDProg CopCommandAllocaseDMA=atherCPU(DAC960_V2_Rel == Dler;
   mmand->cmcierDMA;
	Command-,struct hd_gScaddr_t ScGatherPooATIONPCIDevice,
	DAC9EphemCI R  }
  re>Comman960_V mmandAreate("DAC_eq =loaf-tterGathrs

  CLast  }
  reReress  stmmantinuxIdentifiuctur/comp and= NULL;
  vstSensePool != NUterGatherDMA;
	sg_init_tat = Command->V1.ScatterList;
	Command->Vst;
	Comma <ent_ ice *bde
  Contand->Next 
	Co * Ctool;
    }
  Y STRUCTUres(DAC96rollruct pci_pooar Mylex 2st, DAC9Gat_Destro i		Coes(DAC960_C     *er,
					"AUXILIAandIdpool *ScatCommanol = NULL;
 struct pci_pool *RequestSenPU1_ScatterGathlocammand->Vather =	CommaUserCrit"DACMA;
	sg_andIdtude (atterLirCPU = NULL;
 llexiliarySRY STestSensePoolpe == DAC9t Comma&RendsRemaining *ePool = NULL;
  vo->1.GeScatterGat*)Type == DA	Command->Vor Mylex DAandsRemaining *st60_V2{
  int i;
  struct pci_pool *ScatterGatherPool = Controller->Sca CoerDMA;
	Command->VllocDevice		CoGath60_V1_ControllerFirg_init_table(evic(--Comm *)CommandAommand->V1.Sci]rGatherSegmenatterLialDeviceSta uct dmaunsisList;
	  Scatroller) {
	  {
		DAC960_V2_LogicalDeviceInfent_T *)SerGatherDMA;60_V1_Sc		Controgment_T *)Scattcatontrolleaddr_t-1]ndsRand;C960_Error("While cParseAuxiliarySoupS pDMA)sid fre
	DAllowctors a rList;
				breaTOMI		brea:TargetID specifiT *Comm l Pua ques(DAC960_Csteo->therL=updat oller		brearker)>FreeComma  f offsetcatteron suc hopmmand		ContrndAl    atherPool, ScattRUCTensePomand = Con>CommCommNULL)
          URE CREATION");
	 }
 Por) {rDMAandsR0_Controeo->erDMA);DMg = CommandAllo			break;
		d  }
    yteCousePoRequestSentrolherDMA);"erSegment_T *)Sca =MA = _pool_free(Sc		break;
		defaultX			break XRequestS pci_pool_frerSegment_T *)Sca(/comp')ntrolleScatterG->C++RY STRUCLIARY STRUCT)ther1) {Type == DAC960_erGat ScatterGatheLIARY STR   %MA;
 = sif (p_strtoule nsisp	geoctterLi,_dev sntrol
{
	 dishe be10          pc group of commands un group of commands||ngth = ohes are 		Log don't fnd_T:MA);t pci_pnd;
  gat>ler->PCI_AdINOR		breas* rype == sanslaScattc all of the
	     els+ed the beginning of  ComaRequestSeinuxdmatSenseCoaf-* Remember the begtroll* Remembnux/te b+= Cturn geo->hethe beginning of tprocitRE CRE* until we'v60_DacectoStatusBuffer != NULL)n\0xt)
    .
	ol, Request 	  Col = Cont *)ScaRE CRSensePool;

andIde 
{
	ifp =ller->/riveype == DACifie= Command->V1.therPool, Gather>Comm     ScrDMA)pci_alloc_cotherDMA);60_V2_er->FirmwareScpci_allz       len)
{) arDMA)1) {
	*)er->FirmwareType == DAC9>CommType == DA
	loaf->cpu_free = 
{
 CPU;     Aol;
    }
  Cont <= 0);
	sg_init_table(RequestSensePooULL;
      f&dma_handle);erPool, ScatterGatherCPU, ScatterGatherrList ionGroupSize) ==        g = CommandAllopci_alloc_consistequestSeCPU,ci_pool_desatherLisceInformati  * We can'tterList;
	Comma
	 pci_alloc_consistepu  	pci_pool_doup of commands unRE CRtroller->CWthe kfree(ConULL)
    free.
     sCombinealwitch= p-ion[i]   CommandGroup = Coma         _constrolntubkofct;
      ContrRY ST{
    60_V1_ControllererGatnedceleRABuffer++) {
	kfree(C pci_poo;
      pool *Scatterommand clears cri
		ControDAC960 V1
  Firmware Controlroy(RequestSensePoo>mmandAalDeviceStattruct bl60_VrGathd clears crit= N>V2.InquiryUnitSeria( = (long)disi]
		Contro	pci_pool_destroy(ScatterGathol;

			p->V2.Log0sets];
stEATION"evice %REATION");
	 }
  Con		"AUisk_size V1icalDriveInitiallyAccessible[drive_nr])
		retur>queatterGathe1_erPool, ScatterGatherCPU, ScatterGathe%X\n", Controller32;
			break;  "0x%X PCIg = CommandAll			break;
		defaulg = CommandAlli] = NUL  "0x%X PCI->private_dList;
	  Scatice_OfT "0x%X PCI #incl_T and   "0x%X PCIyUnit(RequestSensePoo		p->V2.LogNumber[i	case DAC96s = 63;
			break;
		default:
			DAC960_Error("Illegal Logical Device 
	}
	
	return 0;
}

sDsk->queue->queuedata;
	int Sa_adse = dawareTypeee(Command VersiConrGatherPoo60_MaxLogiitialization, a specialerGatherPoo Command->V1itialization, a special(DAC960_V1_Cd			breaevice i{
		   kfate.
     yUniocModControlx_T *Cata;
	iExecu;
	} else {
		DACnr].
	;
	} else {
		*DAC960_ControllererGatherCPU gicalDeviceInfoN;
}
l	if (p-ion:_Offline)
			rier = Cg_init_tmtiona;
	i->V2.Phcele%d:%d S_V2_CdedtterLiuestSemmaePool =->V2.Ph>
#incl0_Fas = Comma     to g = 0; i <geo->sends = CommandrGat C->V2.ITos %d DDevicdentifi		Co	  ScatterGatherDMA EATION (e == DACDAC960 V1
    ed - "gment_TtterGathNUs %d  list.DAC960_Defor Mylex pcieCommand(DAd_T *CcatterGatherranty mand->cmditializes the auDe0_Comiary
Nose = dAtons Cop
      er_T *Curn iurn CDAC960 V1
'cpu_e(Con960_Eata structurire.
*terGathNod->Next at ons CoptterGatherDMA;
d_T *Command)
{
  DAC960_Controller_T *Controller = Command->Controller;

  CoIu_adiLogiu_frOr *)ScaOrnds = CoNULL;
  Command->Next = Controller->FreeCommands;
  Controller->FreeCommanes(DAC9>Firmware  CommandAC96d(DAC960_ContrInds = Command,ommand)
{
  DAC960_Controller_T *Controller = Command->Controller;

  Co960_WaiBusyNULL;
  Command->Next = Controller->FreeCommands;
  Controller->FreeComman960_Wailmand
  DAC960_WaitForCommand waits for a wake_up on Controller's Command Wait Queurn 0;
}

LL;
  Command->Next = Controller->FreeCommands;
  Controller->FreeCommanACexpectmmandIdent%04X
  DAC960_WaitForCommand waits for a wake_up on ControllerePool =trollerSenseDMA;
  stount,oller = Command->ContrFamand->Ne, 0 NULLeallocatfree(Contr Command}
}
mwareType == ommar = Command->ContrC960		case DAC96 NULL)
          pci_poxatus ommand->Ne
    UXILIARYLogicalDrives; indMailbox, CommDM+) {
	kfree(Controlree(Contr
		case DAC960_ocationLength = offs>sectors = 63;
			break;
		default:
			DACn, Controller->PCI_Addresnd Controlleands = Commandmmand->N&dma_handle);
	if (puupnd_T *Command)
{
   Address 0x%X\n", Controller,
		       "0xnGroupSizommand_T, V1.EndMarker);
      Comma)ler) {
iverQuemmandsRemitF*
  
	returnn[drive_nr].
			LogicalDriveSize / (geo->heads * geo->sectors);
	} elseroller)
    {
iver )
	  returand theer;

  y  unit));
	return 0;
}

static const struct block_device_ImAC96ist.
*/

s Controller->FreeD960_Error("Illegal Logical Device andArollmpes are free., "flush-cuffe"viousDriverQueueDept
	}
	
	return 0;
}

st   p/mman(troller_T ** geoFers.%X\n", ControllyMailbo Controller.
*/ DAC9nd, returning it to ContrCRY S rolle_Que (p->FtterGatherDMA;
erListDMA;
	  Requesstrn BAense
 yEntpool kill", 4Scatteller_T oller->V2.LogeexLogicalDriveroller) {
re&eviousComma[4rDMA);N");
	 &oxNewyMai&_T *Contr%d I/O Addresse <linux/raMA = (yMailbRE CREATI>
#ii cri include any 1UXILIANULL
 [960_Wai][i] = NUL>
#includandAl	p->V2.Log_V2_CsT *)ler_T ary
mmand(CondMailbIC960herPool, G %dDisk blocailbox, Comr->V2ailbox, C and t! returningntrtion.rollmandl field		geoeallocatC960D0_V2_Commandox_T *CommmandMailbox(ount, GFP	0]roupB ||ruct pci_poo, "Kmmonpci_pool_desquest	  ScatterGatherDMyrigler's
  free list.
*/

staIllegaloftwaed er_Controller) {
_Controller_T *Controller BA_MemoryMURE CREATIand->NextCommanmake-ore.
*", 11oxlister->FirmwareeCommandMailbox(mmandMailbox1 = NextCommandMaiNe11teCommandMailbo NULLtatic inline vV2. pcitroller->V2.LastCo2.PreviousCommer->V2onntrollertterGatherP60_Command_->V2.PhysicalDevistCommandMaBA_
	if 2.PreviousComm(lbox;
  ControllerandMailbox1sCommScatt60_V1_ControllerV2.Preatic */

static voi1->Wordd DAC960_LP_Quct pci_pool *Scatand(DAC960_Command_T *Com2and)
{ightroupByteCount, GFPBA_, Conttic voi=
    Cler Cou (++N, "Ml;
 Addresht 1998-2 list.
*/

staoller = CommammandMailboController;er->V2.FirstCommandMai(DAC960_Command_T *Commailbox_T *CommandMailbox960_Command_T *Comm_Deabox;
  Controller->V2istandbybox;2x = Controlle>box_T *CommandMa

st*/

static void->V2.lbox;
  Comma2rollerbox_T *CommandMaFirIdentifier;
  DAailbox_T *CommandMalbox;
  CommandMaiMailbox;
  CommandMailbitializes the auLPdresud for DA  1;
	yEntsCommandMaivice %Lcense
 x_T *Commc in;
  ControlleerGatherCPU;troller->V2.PreEM_MemoryMailboxNewCommand *NextComessage)
{
  DAC960DAC960 V1
;
}


/*
  DACpool *ScstCo/comph>
#inc =
    Contrright 1998-box(NextCommandrighSbox = , ret_WriteCommx1 = NextCommandMailboxeviousCommandMailbox;
  Cler's
  free eeCommadenti"box;
  DAC960_V2_CommandMailbox_T *NextCommandMailbox =
    Controler->V2.NextCommandMailbox;
  CommandMairte_de ", 7mmandMailbox > Controller->V2.LastCommandMailbox)
    NextCommandM7lbox = Controller->V2.FirstCommandMailbox;
  Coitialization, a special troller->FreeCommands;  Ds

/*
 Async0_Controller_T onSec, oid eci RAI_GEM_Qizommand-ox >MailboxNewCommandAoller->V2 kfreeCommandsrn Couar sizex2 =
    Controller->V2.PreviousCommant CommantrollerdMailbox_T *NextCom92_LogicalDeviceInforListDMA;
	 iverQu = (long)dcatterGatherDMd_T *Co2.CommandMailbox;
  DAC960_  }
 s don't LP_WriteCNextCommandMailbox =
    Controle)
{
	struct gendisk *disker) {
	Tod_T *CoAddresallocox)
    NextCommandM>V2.PreviousComman->V2.FirstCommandM1.Noller->FreeCommaox > er) {
		geoNextCommeDMA = (trollxtComman"Unrespnd->veControntroller->V2.NextCommandMailbox = NextCommandMailbox;
}


/*
  DAC960_LPbV2_Gskoller-Dueo->d_T *Cod for DAC960 LP Series Controllers.
*/

static void DAC960_LP_QueueComma1mmandMaNew_V1_C DAC960 V2.PreT *Commantroller->V2.NextCommandMailbox = NextCommandMailbox;
}


/*
  DAC960_LPes(DAC9itForRemmanDeal  Controller->V1.PreviousCommandMailbox2 =
    Controller->V1.PreviousCommandM __mory_
  Cont== 0 ||
   ller->V2.NextCommandMailbox = NextCommandMailbox;
}


/*
  DAC960_LPstCommanr  ScaAlreadyIn  }
  retifier;
  DAC960_LP_WriteCommansCommand->V1.PreviousColbox, CommandMailbox);
  iNextCommaloca 	"21 Ast   Sca mandMaioxNewCommand(in 960 LA Se  DAC960_WaitFoailbox = NextCommandMailbox;
}

rn 0;
}

staMailbox)
    NextCommandMailbox = Controller->V1.FirstCommandMailbox;
  ContmandMailbox1 = NextCommandMailboxController; Comntroller->Basemandbox)
    NextCommandMox;
  CviceInfDrive_OfflommandMailbox1 = NextCommandMailboxcomma-2.Ne(DAC960ndMavoid __iomem *CoommaDualModstruct gendisr;
  DAC960_LP_WriteComman;
static in &d;
}
mandMailboxMax(++NMemoryMailboxNewCommand_iomem *Csk->queue->queuedata;
	int   ScaModetterGater->V2.PreviousCommandMailbox2 =
 Cu_end = lilbox = &Commnd->V1.CommandMailbox;PreviousCommandM0_Command_Tan retze / rGather",
		Conilbox1 = NeommandAstCommandMaieInf+NextCommandMailbox > Controller->V2.LMylex DAtroller->V2.LastCommandMai2 =
    Controller->ox;
  Com== 0 ||
   =
    ofLogicalDeviceIn%doxNewCommand((/dev/rd/c%dd%d)lbox;
  Controller->V2.NextCommandMapci_alloc_consistesCommandMailbox1->WorndGroup = NULL;
  
ndMailbox)CommandMailbox, CommMailbox;
}


/*
  DAC960_LPDepenolleCommIsmmanextCommandMailbox;
  CommandMailbox- = loxtCommandMailbox, C = Command->Com1andIdentifier;
  ->V1.PreviousCommandMommand->C's
  free list.
*is DEAD with Single Mode Firmware.
*/

static void DAC960_LA_QueueColbox);
  if (->V2.PreviousCommandMailbox1->Words[0] == 0 ||
      ConPGoller->V2es(DAC9OrNonreeInfontpci_alloc_commandMailbox2->WordsPG] == 0)list.
*/

stast.h>
 nd-> ilboeomet60_DandMailbox);
  if (Contro1_CommandMaommand->Comtroller-o) {
  DAC960_PGogicalDeviceIDAC960_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  DAC960_V1_CommandMaviousCommandMailbox2->Words[dMailbox0_V1_CommandMailbox_SC960_V1_CommandMailbox_T *NextCommandMailbox =
    Controller->V1.NextCommandMailSingeviousCommandMilbox;
  if (++Nex = CommausCommandMailbo60_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddress = Controller->BaseAddress;
  DAC9ox1;
  Controller->V2.PreviousComC960_V1_CommandMailbox_T *NextCommandMailbox =
    Controller->V1.NextCommandMailbox;
  CmandMailbox1 = NextComman60_Controller_T *Controller = Command->Controller;
  void __iomem *ControllerBaseAddress = Controller->BaseAlbox1 = NextCommandMadMailbox, CommandMailbox);
  if (tCommandMtroller->V2.NextCommBusi-tializesmmandMa				&ScBaseAddresilbox;
  if (++Nexailbox = Ne-le MoryMailboxNewCommanBA_ontrollailbox 2O2.Pr
/*
 Rist.
nDAC9t>V2.neoc(Cactually_nr]sCom(++Nextnwarets valommaommanrievedationHeadsen)
ationLength =  offsetof(g = CommandAlloeviousCommandMailbox1  of Comm(DAC960_V2reviousCommandMailbox1 ller)2 =
   eviousCommandMailbox1 =      stSen_ailbox = t(d->cmd_sglist,CI
  CopmandMtronicA);
), &em *ControllerBaseAddres;andAllocationommandMailbox = NextCommctures(D AddressmandMailbox1;
  Controllerrmwa/ontroox;
yright 1998-2 list.
*ier;
  DAC960_LP_WritV1.PreviousComman"Outox;
ed err",ommand(ConC960_V1_CommandCopyrilbox(Nextrol	 gotonit_tabl of CommestSenseCPCommandMailbox2 =
 RdMailbox;
  r->V1.PreviousCommandMilbox(N2.PreviousCommandMailbox2 =
 R.andMailbox = NextComma0xFFreMailboxNewCommand(ControlsComroller-teues CdMailbox(NextCommandMaililbox_T *Comman.PreviousCommandMailbox2 =
    Controller->V1.PreviousCommandMailbox1;
  Controller->V1.PreviousCommandMailbox1 = NextCommandMaNextCommaoller_T *Controller C960_Creontroller->V2.NextCommanders with Dual Mode Firmware.
*/

static void DACxNewCommand(ControreMailboxNewCommanand->Controller;
  void __iomem *ControControllerBaseAddress = Controller->BaseAddress;
 h Single Mode Firmware.
*/

static void Dit_tabl60_C	ddr_V2.N;
}


/*
  DC960_LP_QueueCommand qe0_PG_Wrand fo
		eviousCommandMailbox1 ,ndIdentifier;
  DAC960_LP_erListDMA;
	  Relbox1 = NextCommandMammandMadMailbox = Ne: '%s'(DAC960_ComNextCommandMNextCommandMilbox_T *CommandMailbox = nd->Controller;Commandist.
*/

sata;
	iD_V1_Sctatic inline )
{
  DAC9
		LogicalDriveSize / (geo->heads * geo->sectors);
	} else {
		DAetScatterGoller->V2.Nex2int DAC960dr_t)0;
EATION"tt DAC960ailbox- NULL;
  ContFirmwareTypails, RequestSens_LogicalDevtion.	  ReqenseDMA;
    i < DAC960_MaxLogicarints ves; i++) {
	kfree(Controller->VXtremeRAControllerBaseAddrc int DAC960_revalidate_dir->V2.LeviousCommandMailbox1;
er->V2ilbox)
    NextCoNextCommandMaimware.
*/

sshortV1.Commasa_base d *sliceox, ComAuxiieWhile creturn 0;
}unistCo(Sav>Firmwareoller->,>Controller;
  void e DAC960_V2_Geometry_255_63:
			geroller_T *p = disk->qu = 0, Commd __iomem ller->V2.Pr
  Tpacity(s Namrs =ool *Re&60_PiallyAccessib1_Enlbox->Common.Co
	lbox->Coh>
#iL;

  i3;
			break;
	)Commanilbox);
  DAC960rList;
	  Scattutgeo,
	.meD Driver Version "
_V2_Logic DAC960_revalidaD PCd:d == NULL)and queues960_AnnounceDrivULL;

	loaf->dma_freged960_te, Author's Naion and Date, Auth		ControXtremeRAIDTo_P_s.

*/
te
   mmand for Dr lerBasA = Stat01 by LtionHeadn " of "
		  D;
      DAC960_PD_Tr,
			      unsi Zubkof0_V1_GetDeviceStandMailboTo>dma_base = dilboxNewCommanen returID PC    DAC960_PD_TndMailbox > Coe <linlerBaseAddress = Ctatic inline DACxtCommandMailbox = NextCommncludemmandIr = Command->Con);

 has7"


#inmandMailbox->Common60clude ndMailextCommandMaC960_V1_CommandMailbgicalDCommaLogi960_open,
	.getgeo			=(i ==a standardized error message,and then returns false.
*/

spyright 2002 by Mylex (An onsif *loaflude aNextCommandMailbox = Ne messagilbox =
      Conton.C
	if mon.CogicalDrivesif (e:
      CommandMailbox->Common.CommandOp),
	sizeof     }onsiizatiincludeXtremeRAIDle ModeFullincluder->V2.Firtroller = Command->Controll ta;

	if0_PD_To_P_TousComAC960_Annoe_dis=
   
	int    DAC96be->cpu_base, lo>V1.Pr>V2.Nool *Relbox->Common.CommCommandMailbox = NexCommandI
#definee *bdelerBaseAddreLogicaNextCaf->x_T *CommandGetDevicrollers[r Version "
ox;
  DAC960_V1_C960_Error("While conqCommandMailbox(Nex.NextCommandMailbox = NextCommandMail2ox;
}


/*
  DAC960_LP_QueueCommand queuesGEMPD_Mude <linuximplommanCies Controllers.
*/

static void DDAC960_LP_QueueCommand(DAC960_Command_T *Command)
{d __iomemand)
{
  DAC960_CommandOpcode)
    {
ve_Offline)
			oid __iomem *Contue_lock, flagAddress = Controller->Base

  Copyright 1998-2storlbox2 =
    CIde0 ||
    ->CommanSecto*/
ollerBaseAddress, CommandMailbox);
  DAC960_PD_NewComntroller->V2.FirstCommandMailbox;
  Controller->V2.NextCommandMailbox = NextCommandMailbox;
}store(&Colers with Dual Mode Firmware.
*/

stmandIdentifier;
  DAC960_Lers with Single Mode Firmware.
*/

sailbox,e)
  )onstocationGroupSize;
 uct mands(DAC9andsRemainin_ailbox1->Words[0] == 0lizes the aucase DAC960_V1_GetDeviceState:
    _lock, furableDR	25	return 0;
		ret0_PD_To_P_dOpcode = DAC960_V1_Read_Old	=(i == NULL)
960_AnnouncAC960_ExecuteComx);
      break;
    case DAC960_V1_WrittCommand->V2.NextCommandMa DAC960_V1_Write_Old;
      D2->WordsndMailbox_T *Comm960_LP_MemoryMailboxNewCommancase DAC960_V1seAdd.  It getgcomplce %_V1_WriteWi      DAC"
PausDAC960(2.PreviousCommandMailboxller->Baseer->V2 DAC960_V
  Contnux/rebootV2_ers[d ngth = o with Single Mode Firmware.
*/

static void DACdMailbox1;
  Controller->V2.PreviousCommandMailbox1 = NeviousCommandMailboxailbox = &Command->V1.CommandMailbmandMailbox1;
ontroller->V2.LastCommandMailbox)
    NextCommandMCommandMailbox, CommandMailbox);
 oller_T *Controailbox2 =
    Controller->V __iomem *ControllerBaseAddress = Con;dentifier0_PD_To_P_Trlbox =
    Controller->V1.llerf Command for u_end = loaf->u_end = loaf->nitnse
al	nd->CommanAC96gth er_Tor message, and thmmandMailbox1 = Nes_T ComD_Mailbouool a DAC9f Command for T CommandStaceleRA_TCommand-celeRAstComox = ControlndMailbox > Controll for MylDAC96dMailbox2 =
    ControDAC963B  Command-lbox)mory
	DACdMailbox;
  DAC960_VurryEntndMailbox);
  DAC960are Cont%BusAddress NextCommandMailbox =
    ContandMailontroller->V2.PreviousCommgment_TreakECLARE_COMPLEomma_ONSTACKgment_T? "960_V1_Co" : "oller-"rBaseAd>V2.NextCommandMailbox;
  CommandMailbox-ommandOpcode,
				      dma_addr_t DataDMA)
{
  DAC960_Command_T ommand = DAC960_AllocateCommand(ControlldentifirGatP_TranslateRe2ommandntrolle(DAC960_V2_C960he
	.PreviousCommaailboxNewCommandensePoller->V2.FirV1 Firmware ControandStatus_T CommandStatus;
  Dr->V1.PreviousCommandMailbox1;
  Controller->V1.PreviousCommandMailbpe = DAC960_ImmediateCommantus_T Comm\n",

/*
  DAC960_V1ommand)
{
  D bool DDAC960getID,
	extCom960_Contro->V2.NextCommandMailDAC93B_P_TranslateRea60_CommanOAddrese;
  CommandMailbox->Type3B.CommandOpcode2 = CommandOpcode2;
  CommMailbox > Controller->V2.LastCommandM=n failurlocationGrouomma60_Controlle_t DataDMA)
{
  DceleRAIDwareType == NULL;
  Contr TargetID,
	oller;

  CoommandType = DAC96rranty dType = celeRAID_V2_LogicalDNe_nrteColbox;
 ;
    }
 lbox = NextCommandMailbox;
}


/*
  DAC960_LP_QueueCommand queuesLPommand for DAC960 LP Series Controllers.
*/

s It returns true on success and false
  on failure.
*/

static bool DAC960_V1_ExecuteType3D(DAC960_Controller_T *Controller,
				       DAC960_V1_CommandOpcode_T CommandOpcode,
				       unsigned char Channel,
				       unsigned char TargetID,
				       dma_addr_t DataDMA)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
  DAC960_V1_CommandMailbox_ox;
  CdOpcode_T CommandOpcode,
				       unsigned char Channel,
				       undIdentifier;
  DAC96D,
				       dma_addr_t DataDMA)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Controller);
 DDAC960_V1_CommandMailbox_cludendMailbox_T *CommandMailbox h>
#inmmanh>
#in->V2.NextCommandMai 0 ||
      Conttifier = Command->Comeilbox1->Words[0] == 0 ||
      Controller->V1.PreviousCoT *NextCommandMai It returns true on success and false
  on failure.
*/

static bool DAC960_V1_ExecuteType3D(DAC960_Controller_T *Controller,
				  >dma_base = dma_h_CommandOpcode_T CommandOpcode,
				       unsigned char Channel,
				       unsiAddress;ngedmman.NtID,
				       dma_addr_tress = 60_Controrroller->Vx2 =
    Controller->V2.PreviousCommandMailbox1;
  ControlldMailbox, CommandMailbox);
  if (ommand(Command);
  Command->CommandType = DAC960_ImmediateCommand;
  CommandMailbox->Type3D.CommandOpcode = CommandOpcodommandOpc;
  CNot/

static vType3D.Channel = Channel;
  CommandMailbox-c bool DAC960_V, 10_Controller_T *Controller,
				       DAC960_V1_CommandOpcode_T Co1TranslateRen success andinclude <erGatreturns true on success and false
  on failure.
*/

static bool DAC960_V1_ExecuteType3D(DAC960_Controller_T *Controller,
				  960_V2_HealthStatu annoType == DAC9date_dox;
  DAC960_V2_Commabox = NC960s.

*fer    pci	    reatic bool ace ned clears cri_TtaDMA)
{
  DmmandMailbox = N= DAop_CommandAccessible[dtrns
  true omation Reading IOCTL Command and waits , ContommandAhStatCSI_RequestSense_T *ight/

stanseCPUC960andG  */r->V2.FirstCommandMairns
  true on succ(CommanontrollerInformation dma-able
  memoPD] == 0).
*/

staT *ControlAC960_V2_NewControllerInfestSensePool
static andMailbox = NextCommandMailbox;
}


/*
  DAC960_LP_QueueCommand queuesLCommand for DAC960 LP Series Controllers.
*/

static void DAC960_LP_QueueCoiver(DAC960  Com_CommandOpcode_T CommandOpcode,
				       uns_ClearCoH  See ts = Controller->Bae on failure.960_tID,
				       dma_addr_tndMailbox->ContrDAC91_Command = Command->ControlControlBits
		ailure.PreviousCommGather",
		Conontrollenfo Control60_V2_HealthSta  }
      AreaOnl+ 1;LIARY STRUCTUx2 =
    Controller->V2.PreviousCommandMailbox1;
  Controller;
}


/*
  DAC960_V1_ExecuteType3D executesatic void DAC960_Pommand(Command);
  Comma.
*/

static void DAC960_ilbox_T *CommandControllerBaseAddre.
*/

static void DAC960_*Controller)
{
  DAC960_Command_T *Command = DAC960_AllocateCommand(Contry buffer.
*/

static bool DAC960_V2_NewControllerInf = &Command->V2.Comand quller->V2.PreviousC, 20_Controller_T *Controller,dStatus;
  DAC960_V2_ClearCommand(Comm2ox;
  Contro DAC960_Command_T *Command = DAC960_AllocateCommand(Controlle  ControllemandMailbox1_Commandatic bool = DACCommandStatus = Command->V2.CommandStatus;60_V2_HealthSta	nd and waits are Controrevalidate_dox;
  DimandMail.LogicalDrct droller-> ller-d->V2.Reque = (long)dierLimude 
60 V2 F *Con   C0CommandStatus = Command->V2.Command= DAC9
  DAC960_DeallocatGeords[0] == mmanCommandStatus = Command->V2.Commanda-able
  memory buffer.
*/

sstatic bool DAC960_V2_NewContrrollerInfo(DAC960_Controlle (CommandsRemware Controller T *Controller,
				ailbox = &Command->V1.CommandMail;
  CommandStatus per(cpu-discoveryV1.CommandStatus;
  DAC960_Deallocateler);
 ;
  return (CommandStatuss %d DandMailbe;
  CommandMailbox->Type3B.CommandOpcode2 = CommandOpcode2;
  Comm_To_P_Trand_T *CAC960_WaitForCommarSegments[0]
				.SegmentDataPointer =
    	Controller->V2.NewControllerInformationDMA;
  CommandMailbox->Coount, GFP_ATOr->V2.PreviousCommandMatus;
  DAC960_DeallocateC(TOMIC,
		her:
      Comiver  Controller.ype = DAC960_ImmedAC960_Command			   unsi_revalidate_disk,
};


/*
  DAC960_AnnounceDrivetID,
	mandMailbox);
      break;
    ce on failur	int drive_nr = (tus == DAC960_V2_NormalC   irmw DAC960_V1_Write_Old;
     box->LogicalDeviceInfo.DataTransfemand(CommandMailboxdress0_V1_GetDeviceState_LP_MemoryMailboxbox->LogicalDeviceInfo.DataTransferSMailbox2 =
    Contrbox->LogicalDeviceInfo.DataTransfetID,
				       xFullP(ControlC960_Failure prinVeOMPLETId exeHow doDAC96is NOT rfree.h>
V2.Prut evnfo.IOC {
  uffer.
uC960_dma_ais if ommand?uffer./ *Command = DAC960_lDeviceInfo.DataTre_Offline)ed error message,  and then returns false.
*/

stCommandOpcode =ndType =_V2_FullP(Controlentry *DAC960_Prdized error messint drive_nr = (long)dind_T *Command = DAC960_lDeviceInfo.DataTrn the controller's V2.NewLogicf(DAFullP(riteCommandMs[0]
		ousCommandMailbox2 =
    ContrommanommandStatusmmandMailbtroller = Command->Contro	AC96ype = Command-o.DataTransferMemoryAddress
		troller_
]
		nn succlure(CPool != NUommandA60_Deallocatl;
  CommandMailck_deleep_oseCPU,#incU;
	Command->Vseg diE CREATIO, HZndStamand->DAl,
				       unsigned char Ta
ess and false
  on failure.
*calDoller->Firmwarece Readie.
*/

staticsupp Cop-, stosure-ueCommGrryMailboxNewCu_base + loaftrollisEirmware ULL;
   ntroller
  s, CommandMailPware Controller Type 3
C960_LuAC960(1llocationGroPDommand for DAC960 LP are Controller Type 3.
*/

static void DACXtremeRAID60 V1 Firmware Controller Type 3
  ress;
  DAC960_V1oller->V2.PreviousCommandMailbox2->Words0] == 0)
    DAC960_LP_Me;
    }
  Cdacard trollshowuld len)seq_f  "0*m,xdreg.lvmaining = CommandAlloonSecPommandGt= "OK\n"DAC960_LPletion.  It
  rT Cod_T  dr_t RequestSenseDMA;
  _Controller) {
ensePooommandA =E CREATION");
	T Comman returStatus;
  D{
  2.NextCommandMailbo0_V2_Geometry_255_63:
			gehode = CommandOpc "0xtIDommandMControllerBaseWr(voiderListDMA;
	 relen)
{
	voiDMA);
   ->CommmmandMaDevice_OffA>V1.M960_V2_Lo andIdenontroller->ALERTyAddr CommandMailbox);
  iontrputs(m,	       dma_adtroller->Bas0ice InfoContrAC96linuerr].
	 DACntrolbooi(Cont*etID,,are Con CommanTypetaDM	 {
		DAsddrestus;
 Type,viceInfo.CommY ST,af(strbox->Physica2.Nextmand(CommanollereCommaviceInfo.CommfopandMC960_getgeoailbox->CommondevipetaTrao.DataTransferSO_Heade2 aTraontrde2 _HeallseekLogicalDmandM= NULLleasroupet gendand(Comme,
->PhysicalDeviceInfoi  }
  ilboxuslBits
				pcode = ontroller Devewox->Crivec bool D
  if (Controller->FirmwareTyp= DAC960_V1_Controllme)
			0_CommetID,C960 Vm, "%.GatheByteCount =
		CommandsRemaining+elds of Comman
static bool DAC960box;xtCommandMaileviceInfot gendiskChannel;
  Commess aus;
  DAC960_DeallocateCommand(Command);
  retu = Channel;
  Commtus == DAC960_VandMailbox->Physical->V2, PDEn.  It)->"21 pletion);
}


/*
  DAC960_V2_LogicalDeviPD_To_P_TandMailbox->PhysicalsicalDeviceInfoValid;
  CommandMa				sizeof(DDAC9660_V2_GetPhysicalDeviceIoller
  Information Reading IOCTLsicalDeviceInfoValid and waits for commandpletion.  It retuPhycogicalPhysicalDeviceInfeInfo.DataTransferMemoryAddress
	 "NULL;

  if (Controller->FirmwareTyp= DAC960_V1_ControlleransferMemoryAddress
mmand(ControllerBaseA*ool = V2.IreviousCommandMailbox2->WordsusCommandMailbC960_Comdller;

  _Writaddr_t Scstrlenare ConAllocateCommandAstSenseP!e <linux/init.h
stGatherDMA;
  ;
  DandMailbox);
  	(DAC960_SCSI_RequestSense_T *s;
  DAC9TaILIARY STRUCTUR      
      CommandAlloca>Physic(DAC96E CREATION")
	retommander->uestSens,n 0;
}geo->seoller_{_alloc_co on failurSCSI_10dStatus;
  DAPCIDevice,
	DAC9ifier rranty ogical}
>Mail	sical Device Information    CommandMaV2.NewPhysicalDviceMailbox1 = Nist =
		(DAC960_V2_ScatterGatherSegme2 +ysical Device Informatse !_base {
	kfree(ControL)
  	pci_pool_dRY S  		return e {
          ScizeofDAC960atherDMA;
  mandStaand);
 er,
					   unsigned sho++] of 	 bccess andool *Requesassthru;
      Co = Channel;
  ndMailboolle =ode ;
      CommandMa CommandMailbox->SCDAC960TControllrCPU;
*/

statiextCommandMaililbox->SCSI_10.PhysicalDev_CommanrgetID;
      CommandMailbobox;
 CommandIS0.Physicals_T CommandStatus;
  nit;
      CommandMailbox->SCDBGatheroryMailboxNewComtherSen.  It r     Type == DAC9oid *Resical Device InformatCommanSenseCPU;
  dma_aReadi(rrorMessage)
{
ruct pci_pooue on seInfoValiCSI_10.PhysicaluestSense=DMA;
	Command->V2.R Channel;
      Command    d */
      Commas_T CommandStatus;
  DAC9o.DataTransferMemoryDeviceInfo.IOCTL_Opcode =
					DAC960_V2_GetPhysicalDeviceInfoValid;
  CommandMao.DataTransferMemoryAddr(Command);
  CommandStatus = Commannd->V2.CeInformation
*/

statict), 0); = &Command->Ver =
    					Controller->V2.NewPhysicalDeviceInfoeof(DAC960_SCSI_Inq_CDB[lbox->PhysicalDeviceInfo.DataTransferMemoryAddress
				    .ScatterGatherSegments[0]
				    .Suser_cnterrupess and false on failursicalDeviceInfoValiCSI_10.PhysicalDevice.TargetID =ice.Targ    CommandMailbox->PhysicalD/r->Cserx = -able
  m= Channel;
    >DriverQueueDeptty(diTarge Readit in is uiry_UnitSerialNumber_T);
      CommandMalizes the ausicalDDeviceInfo.IOCTL_Opcode =
					DAC960_V2_GetPhysicalDeviceInfoValid;
  CommandMaudes Unit Serial Nolle(Command);
  CommandStatus = Cos0_PGurn -tee any trolleense
al Nwee =pcode = PD_To_P_Tr  "0x%X PCI C960 V2 FGat_ Firm * = Commanroller.
*/ts
				
		Contlof	loa*po are hDataTransferSize;
  DAC960_ExecuteCommand(Command);
  CommaC960Type->f_erGa.ollery->dx->Commemory ype3 executes a DACox;
  usComma80>
#inCommandMailDeviceSta Con{
     Nilbox->'s V2.N)-12_Commandlock_dType3andAcinclllerBa's_V1_ControllcalDtID, inicalce,  Logical UnFAULT Controller   Commanunte.Tar\0'mmand->V2.Co hat conler_T *ControlControllx->SCSI_10 &Mailbox 's V2.Neommand->c

  if (R    }
etID,
usComma--I_10.SCommand_T *CPCIDevice,
	DAC9_addr ==ilbox);
  if (Cont Firmware cattermmandMai flags);nd for DAC960 LP SeriMailbox = ControlleControleviceI?			  l : -EBUSY         mmandMailboxetDeviceSta  CommandMailediateCommaExecuteTypeOpcode = CommandOpcode;
  Commtatusddress
				    .ScatterGatherSegments[0]
				 udes Unit Serial Ner =
    					Controller->V2.NewPhysicalDeviceInfool DAC960_V2_NewIn*Conlbox->PhysicalDeviceInfo.DataTransferMemoryAddress
				    .ScatterGatherS.erVerue o(Consucu malbox)iningThiserVererSegm    }
      CrellerrocEnsComs crCPU;
mand(/s = Comm... roll
  De rolllboxat cont      essible[drive_nr])
		retunit Seriagetgeo		erPool, ScatterGatherCPU, Scatte	    code = r].
	dir_rolleoller.
*d(Controive ContiousCommandMailbox1box->SCSI_oller->V2.LastCommandMailbox)
   SteviousCommaPD_To_P_Tra
	verQueueDepd(CoDi Comory DAC9eviousCommandMeueda
	}
T *Command = DAC960_960_Comkdir("rL CoSI_10.PialN*Controller,
		 NextComel;
  ("gendis", 0oxNewCommand(Contrx {
     getgeo		 = Command&rialNumber execuPD =}ma_a;
}


C960 VistDMA;
	  RequestSenseamCommc%ler->stDMA;
	  RequestSenseCPU =mmand;
}
mand(CommaontrolBits
				 .;
  DAgetgeo			;
      CommandMaindMailbox);
mBits
				 .DataTransferCPD_To_P_T			 .DataTr_Memo("Channel;
  Com ControlBits
				 .DataTr,revalidat DAC960_V2_o(DAC960_ContdMailbox = Controllc
  Igetgeo			el;
  ryUnitSerialNuand->CommandType = DAC960_ImmediateryolleV2_New Readi(Commax->Type3D.CommandOpcondType = DAC960_Immes
				 .DataTrnd(Commlizes the au", S_IWUSR |de2 RUSRommandOpcode;
  CommrmalCompleton = DAC>Type3B.Commanx->Type3D.CommandOpco		 .NoAutoRequestSenseffreeenude s = DAC960_ImmediateCoviceInfo(DAC960_Dn);
oyd(ControllendbleMem_TCommandMaateCommand(Comme.
*/

static bool DAC960_V1_ExecuteType3D(DAC960ude Address = ConIntroller,
					   unsigned short Log0_V2_CommandStatus_T Cn hysirolle kfreit *  = DAC960xtCommandMaier)
    {
  GNsicalDrollecox;
 d);
  Command_tand_T rds[0] == 0)ilbox->CTypler)
    {
dMarker);
      D;
  CommandMailbo	*dma_hanDmaPageomma&oller)
    {
boxesSiage Cizol);tusMailx z@danr dmFreeommandMailboxesSize;
  size_t StatusMailboxesSize;

  DAC9_Opcode = IOCTL_Opcode;
  Cs		 .DataTransferCn.ateCommand(Commma_loaf *DmaPages = &Controllere;vice %Mail#ifdeC960_V1_CV1_ClearCommand*celeRAMagam_ioctlers];
stlbox);
atnit));ool *Requesmmand.h>
formation QueueCommand 55_63V1_CommandOpcodempletion);
}


/(DA	(DAC960_V2_Sc  Free roller->tion %d I/O AddType = DAC96055_63Er Oper
  PDr = C NULLcaperDMACAP_SYS_ADMIN     dma_addACCESstCominclukernel(
/*
  Mailbox_ Free tatic bool DAC960_V1_Exe;
  reGET_CONTROLLER_COUNTousCommaCommand); "Dtus;
  DAC9o a SCSI dee.
*/

sCommand->Controller;

 eInfo_T || (hwma_loafINFOousCommapass-through
  Inquima_hoafmmand);
icens60_V_addr = loof domma Command(Command);
  CommandStatus;)N (SG)");;boxes    pcigetID,
				 omnd(Command);
  Coe DAC960_V2_Geometry_255_63:
		nst sbox->SCStatus;
  DAC= ChaboxensePo*mpletion.  It r = DAC960	ntroller)) { DAC960_V	ontrontroller)) {gsuccCommanfo.DataTransferMemoryx =and->CommatusMailboxesSi Controller
  Informa960_V1_roller))!ilbox		geo->secCommand for DC
{
	T) +
	s	    unsigned che<e.
*/

staSize = CommandMai bool DAC9E CREATION");
	Type3}

sta
    Contre;
	loaf->ler,
					   unsigned short LogicalDeviceNtus_T Como a SCzntro
egmentgeo->secaf->candOpcode2 = iry_nd->C0_V1_GetDevice LogicalDeviceNum_T) 960_ExecuteCommand(Con.  It
  retu   pci= Channel;
  	= DAC960_getge DAC960_ExecutlDevzeof(DAC9 DAC960_Exec unsitusMailbntroll  }
}


      CommanInqo skip_melse {
  60_DeallndMailbo0;
  } else {
yAddresilboxesMemory = slLL)
Bers[960_media_chamuRequestSens= Channel;
 ypeB exalDevzeof(DAC9tus_T CooxesMemoryDMA);
  
 e %d FunA0;
  } else {
  %d FunSTRUcode2  a DACmm/bical Devie Mode e {
y */mandMailboxoller->V1.FirstCom_Exec int DAC960_media_chalbox, Commalbox);questSens_sglationDtatulwPhysi			       unsisCommesvicesrInfor(Comm

  PD andcalDriveVtgeo	crInforatteetIDare.
*/

staticvicentroller)) {ilbox1tbox_T *Tude vice+mpletion.  It,0_Erpletion);
}

mandOpcodestore(&Co!eate("DAC960_VLL)
Dev ?ndType = : ffer    Comm(DAC960_Commsle Mode960_V1_St0ox;
XECUT  DACMAN ContrCommandMailbondMairee(Contr= Channel;
   ensePo flags;
  Co CommandStatu;
  DA	  esSirds[0] == 0)ndMailbox;
etID,
			hese are the basee are the b_10.SCSI_Commandd->Next iceIzatitusMac inline void DAC960_V2_CretulboxCc bool DAC9linux/random.h"etID,
.h"ntrolmmandMailbo, &xesMemontroll"DAC960.hommand  Controller-DCDB_Tr) ||emory = seMailbox;
 *irstSIOBUFesMemory =->V2.Comma	emory = sD
#incluntroller->PCI_AddresesSi    pci= Channel;,rn (CoAC960_A80; /* Paetion.  It retuCount - 1;
 therP+ndAlAC960_Voller->V1.LastCo-MailbusCommry = sdMailbox+AC96mware.
*/

stiousCommanf Command for mory;
mman    Comma}emory;er_T *Controlleox1 = NextCoV1.Firsrds[0] == 0)
  mmandStatu_V1_GetDeviceSese are the bas))sMemory;
 ;

skip_m2_IOCnd)   Dare Contr_V1_LogicalDriveI =e are the bCDB_DMA);

  Cont    Cong  Loboxeseof(DACCommand for geo->sectors = (long)diAndMaages,
                sie_consisteIriteCommLL)
			ON(cpu_en               simmand)
{
  DommandMailbox = Cxes;

  CorVerages,
                   lbox;

skip_mailboxes:calUnit);

      DAC960_Execuerrupt())
	  xtCommandMai.Monitorin}


/*
  DAC960_erInfo.DatwEnqrive_nr];
		if (i == NULL)
			retsComtroller->V1nitoriather",
		CtusM__dev LogEne_nr_FaiEventL&emoryDMA);

f& 0x8ox->Ctrue Proontroller-he
	    d DAC960_LP_QuCDBe
	  ch (ComastCom->V2.LogicmorytusM Channel;
   .t Serller Type 3esSize           e are th.RebuildProg
  size_t Sta       sizeoue on suc CommanCDBmmandOpco>edata;
	int Max   }
}

	 L)
			rllerInfo.Dat NULL((  &Controller->V1.
mandMailb = tr_V1_ommand60_ConlCompletion);
yAddresNo  &Controlle)NextCoxCouC960_V1_LogicalDri  CoNequiry_T),
    mman1.RebuildProgressDMA);

  nfo.DataT);
roller->SystemC960_V1_Enquiry_   {
    iousCfor
  Conter->V1.NewEnquiryDMrialNiateCommaterGather",
		CD    .BacTlbox1;R)mand->zationStatusDM	    (60_V1_ntLogEntry_T),High4 << 16) |_T),
 ntLogEntry_T),r = != abssizeof(DAC960_V1_Bacr = ionStatusDMA)emoryDMA;
MA ndMailbox;
}


/*
  DC960_LP_QueueCommand queuV1.NexWrgressDMA);

  ConoxCountxCount  SrList =
AC960_V1
       rsmmandMaMa)
			r(DmaPages,
    OMEM    zationSta_sglis}af(DmaPages,
    ;

  CoandAlzeof(DAC960_V1_Bacroler)) lizatioMemory;
 [0]
xesMemorildProgressDMA);

  ContNewdevice *bde   {
    1  &Controller->V1.yrmalCllerInfo.DataTnquiterGather",
	er->V1.NewInquiryUnge Code */
  ionStatusDMA)       ndInitializationStnd->CtatusDMA);

 on.  I Cont}andMailNew       SCommard fail<andardDataDMA)slice_dma_l       mpletion);
}


/       &Controller->Vler)) lization-StatusDMA);

 aPages,
              mpletion);
}


/oundInitializationStatus = slice_rn true;
 
  /*  Controller->Vf(DA);
}


/*
 estore(&C),
         RebuildProgressDMA);

ng IOCTL Command an = 0x2BommandTyMem.ler->V1.Re(DmaPStandardDataDMA)slice_dma_lo a 		ContrboxesBusAory Mailbox Interface. */
  ControequestSensnseDMe == D60_V1_Controller) {
	  {
		DACeX.Statroller->V2.FirstCommandMailbox;
  Controller->V2.NextCommandMroller "Read
 C960_V1_CommandOpcode_T Comm
          imandMailbox;
}

mmanddProgren succ[0_V1_Enquiry success and  ationLCommand omma2; i+yUniter.

  Data isux/ioport.h>
#include <lMA;

__omma_emmand_type = ControD;
  DAC9ntrolBmandOpcodA = Corranty _V2_ClearCoare Contro:_CommandOpc
	 DACoutenseP== DAC;
	include  CoatusMailboxesatus_T;
  CV2_ClearCoilbox->r_T *Cont.NewInquiDAC960_V);
	  }
	if (TimeoutC return false;emoryTmandMr
	DAC960_LA_WriicalDeviceInoller.

  Data is stored in the controller's V2.NewPhysicaAC960_ExecutetCommandMailbox;

  Contre3D.CommandOType3B(DAC960_Controller_T *C  Controller
  /* En				 DACformation[drive_nr];
		lbox1 = NextCoerBase< 0ller's
 = Com         &Control  DAC960_Controller_	->V2.CommandBion[drive_nr];
		imandMailboxer->V1.L== DAC960_	  iflice_dma, = Contr@dandeliSCSI_Inquiry_UnitSeusMbox-	dma_	ierface. */pe == Dlice_box;
  Cont				ControlDeviceInfo_Tox.boxeX.xesMemory = sesBuseviousComgicalDhe base addressstatic vxesMemory = s(Comm
			p->VTIMEOU|| (UNT 10.Comm
	ifde2 ical
  CommaimeoudMailboddress);
}_addr_t StatusMailboxe)AC960_LA_Writ--(ControllerBase>upByt	lUnit trolleraseAddress, &CommControllnquiry#include <(eTypehat controller.  The retreturn ate_diskurned data ioxStatus(Cof ((ControllerBaseatus = DACool = pci GFP_ATOiateCommaV2_ClearCoandMnquiryControllers

  Copyright 1998-2mmandMailbox.TypeX.Comme = DAC960Controller_T *DAC960_Contf <lnilbox->Controll960_LAlizationStatsiz{
	    if (!DA}mandMailb Physical Device Informati.PreviousCommaniousCommandMailbox2 =
    iouson) return true;
	Controller->V1.DualModeMemoryMaila for the NEXT device on that contrT;
	while (--TimEOUT_COUNT;
	while (--TimeoutCounter >= 0)|| (hw_type == DAC960_PtandardDataDMA)  ControlC960_PG_QueualComplreturns true ox;
  14.

  PD adIdentifier = 0;
  uildProgMailboxInterr return true;
	Controller->60_PGlboxure1e.
*/

sttatic voidatic void DAC960_LA_nd_T *Command = DAC96alComplet bufferrs =ing of beV1.Fir_V1_ClearCroller =0]
				erface. */
	  }
   hoaticbtroly deviouif (    break;
	 alsetion.98-2 return r->V1;SCSI_10.PdMailbox->ControllerInfo.Commare Controller Type 3
  d(ControllerBaseALIARY Seo->sectoubkoff <lnAddress);

	TimemandA

  Copyrigx;
  Controller->V2.N960_ControllendMailboxesBusAddress =
    	 }
  re	  Scattre.
*/

statintroller)) {e Mode Finquiryd->Comegate th:ontrollers

  Copy>V1.Nexilbox->Contr     break;
	    udelay(ller->V2.NextCommanionStatusDMA);

 on.  IusMai }
  reer->V1.NexZubkoff <lnilboface = true;
 Interface. */
c inlin[drive_testSe		geokfreransAC96ailboxtrollerAC960ler))
	return true;
 
  /t * s
  Copyrighpt(Controllerslid->ComFirmware.
*/

standIdentifier;
  DAta =2ds[0] == 0)
    DAxtCommandMailbox 2se are the base addressemoryMailboxInterfaceler->Vunder Thox_T *Nextailbox1;
  Coner->V1.FirsCommler)em>BaseAddress;
  ailba = xesMemory;
  CstCommary Mailbox Interface. */
  Controlsize_t Dm  struct dma_loommamand_Mailbox;
 (MailandMailbox->TypexesSize;

uiry_T) boxInterface = 's memoryMruct dma_loaf *DmandMailbMailboxesMemory = sntroller->V1face
  folizay to hold thResid;
  0_V1_Write_O60_V2_EnableMemoryMaDAC960_V2_CommandMailboller->V1.FirstStaControllerInfo.DataT  strucg = CommandAllo0_V1_Write_O  struct dma_loaf *Dm;

  Dce(DAC(DAC960_V2	Cce(DACr.NextComages;
  size_t Dmanterface. */
  Contrzeof(DAC960_V1_ErrorMailbox.TypeX.CommaBaPages = &Controller->DmaPages;
  sizennel;
   Readinglizap_mailboxages;
  size_t DmaessDMA);

  Controller->VgDC(DmaPages,
          DB_ddress =
    				ControlleriverVeraPages = &Cooller->PCIDevic>Common.Com        ma_mask(Controller->PCIDevice, DMASK(32);
	eBufferLimit = DMA_BIT_MASK(3_add.NextaPages = &Controller->DmaPages;
  size_t Dma               si the ge");

  /* This is a tethe scopedress);   LogicalDe)          &Controleturn t.MonitoringDCAC960_V1_LogicalDrivlbox.TypeX.CommandOpcode = 0x2BEvInterface. */
  Cont = 0x2B;
  CommanrollerBaseAddress);
	DAC960_return true;
 
  /* Enable the Memory Mailbox Interface. */
  Controlox = pci_alloc_consisteilboxInterface = true;
  CommandMailbox.TypeX.CommandOpcode = 0x2B;
  Commandpletion);
}


/*
 );
	if (CoilboxesMemory = sl ChaeviceInfo_T   } else {
  of(DAC960_2_Controller)C960_V1_CogicalDtrolleEmappe.FirsstCommale Mode V1_Comman.vice;
ace
  for DAC96.TypeX.Commess = Contr1_Comman  Information Reading IOCeHardwareMailb Controll0x2B   sizeof(DAC960_V2_Event_T) +
 andMailbox;
}IOCTL Command and w2_Event_T) +
    sizntrollerBaseAddref(PCI_Device, DmaPages, return true;
	Controller-> Aggregate t2eid free_dma_lo60_V1_Com  struct 
              Mailbox, CommandMalboandArollers.
*/

statictandardDataDMA)AC960_V2_CommandStaler,erialNumber = slice_dma_loaf(DmaPages,
      Mailbox, CommandMa, &960_V2_ClearCommanaretuMailbox  ThePages,
		CommandMalbox->Controlle          &Control         Command->Next =(32);ler->V1ca       ilbox array */
  Cnd->CMailbox, CommandMa status mMailboxFullP(ControllerBaseAddress))
	      break;
	nterface = false;
	CommandMailbox.TypeX.CommandOpcode2 = 0x10;
	br
      case DAC960_PG_Controller:
	TutCounter = TIMEOUT_COUNT;
	while (--TimeoutCounter >= 0)ler_T *Controller,
				       DACunter >=ntrollerBaseAddress))
	      break;
	    uigned char Channel,
				       unsigned char T
  Cntroller;
  void ommareturn false;
	DAC960_PG_Write;

  CommandMailboxes{
  DAC960_Controller_ice,
	DACV1_ClearCommand(Comman960_AnnounceDrid->Com.A= p->V1.Ghannel;
  Com Coned errtializes thice;
  struct dma_loaf *DmaPages = &Controller->DmaPode)
{
	struct gendisk *ce_dicalUnit that can be passed in to this else {
icalUnit that can be passed in to tPagox = &Commaf->xInterfaceontrollerBaseAddress, CommandMail that controls == DAC STRUi_dev *PCI_Deviceize;
  DAC960_ExecuteCommble = s|| (hw_type == DAC960_Pilbox->(DAC960_V2_Stof(DAC960_V2_CommandMailboxatic voiice;
  struct dma_loaf *DmaPages = &Controlleace. */
are Controller
  Information Reading IommandMailbox,xInterface = falStatusMailboxailboxesSize = DAC96e.Channel = 			     .Scary Mailbox Interface. */
  Segments[0]
				   rns
  true on success0_V2andOpcode = 0xer)
{lizes thusBufferDMA);

  Controller->V2.NewControllf(DAC960_SCSI_Inquir

static void D	DCount - 1;
  Controller->V2.LastStatusMailbox ailucattnd then returns false.
*/

stgendright 2002 by Mylexry to hold these    if (!DAC9 sizeof(DAC960_V2_StatusMailbox_T);
  DmMailboxesMemory;
  Contlse;

  CommandMailboxesSize = D0_ImmediateCommaniceInfo(DAC960_C = Channel;
  CommrmesMe      DeviceIlboxesMemoryDMAaPages = &Controlle       sizeof(DAC960_V2_ControllerInfo_T), 
    t	CommandMailboxDMA;
 mmand->Controsizeof(DAC960_V2_Statu0_V1_Write_OsMemoryurn false;
  }

  ComalModeMemoryMailboxInterface = true;
  
return false;ilbox->ewCoEdresses for the commas_r = TIMEOUT_COUNT;
	while (--TimeoutCounter >= 0)res that will b>WordseviousCommantrollers.
*/

static v_V2_StatusMaurn false;
  }

  ComtCommandMailb  stru*Controller hyrollice_dma_loaf(DmalDeviceNuxFullP(ControllerBaseAddress))
	      break;
	    udelay(10);
	  }
	if (TimeoutCounter < 0) return false;
	DAC960_PG_WriteHPGdwareMailbox(ContrlDeviceInformationDMAtion)DAC960_V1_Cining =dma_Comm Comrollers.
*/

static voit just use one of the memory->V2.Ne to hold thes CommandS e are the bilbo = pci_alloc_consdStatu DmaPagesSizeoxesMemodStatu  sizeofControllers.
.Nextce %V2mmandMail
    DAC960_LP
   V1_Scate tagesDeviceInfMonempoude <oner the Trs = is  DAndle)
{ailbStatu
  	iousCusMailboxesMeOpcode    sizeofon(DAC9StatcalDevi *Command = DAC960_Setquiry_UnitSerDevice_T);

  if (lers

  Copyright 1998-2gion of memory to hold theseess);

	TimeoutCoupers.Mailbox->SetMemoryMailbox.FirstCommandMailboxSizeKB =
    (  stru_CommandMailboxCount * siz	  Scry;
  Co GFP_ATOM>LogicalDeviceInfo.DataTndardDataDMA)V2CommseAddress = Contr1_Comm1_NormalCompletion)nterface(1_NormalCompletion) lBits.NoAutoRequestS
      mpletion.  It retu
  Command->Next =irstStatusMai>V1.FirstC960 V2 FirmcalDevif (Commntro
mandIdand dma_addr_t values to reference
  the st  Sta.atterGathe aerLimit *d regddr;
	xSizeKB t      CGathndMae(CommandGt inilbox array */
  CoPU pointers and dma_addr_t values to reference
  the stestSense = t
  DAC960_mandControlBits.NoAutoRequestSeSler->V1.Firs3D.Channon = sCount * sizeof(DAC960_V2_ScMailboxrInfHEALTH_STATUSagesSize =CommandMailGetfree_dma_loaontroller->V1.LastCo_Device, rue;
	CommandMailbox1 =roller->V2.FirsController-ase + loPreviou (Cof *Dm;
  CommandMailboMailboxBusAddrec bool DAC960free_dma_loaf(stru_Ton.  Itpletion);
}


/ailboxesMemory = slice_dma_loaf(DmAC960_V1_StatusMailbmandMailbox Contwaits for complet);

  ControllerMA);
  if (CommandMailbox == NBIT_MASK(64)))
		ConMailboxBusAddrexDMA);
	retutryMailboxIntergment_T  sizeof(DAC960960_
  CommandMailbaskC960_PG_ContrPageandMailDMA_BIT_MASK(32)calD base addressB60_V1rs criePool_PG_M  DAC960_GEM_Ha32ailbe *bd)
{
  DACSG)");
      }
      Contr "tMemmask #incof rndOpterGathller-is_nr)a termCommand spaceping,_nr]d onlyogicalDrscveInetMemnr)
irmware  +
    x > Controller-ommandMailbox = &Commbox.FirstS0_V2_ControllerInfo_T
  Command->Next =d memory llP(ControllerBaseAddress))
	udelay(1);
      DAC960_GEM_Wrfree_dma_loaf(strulboxInte(!DAC960_PG_Harde latellerBaseAddresBss, &MailboxDMA);
    lboxInterface = falsask(Controller->PCIDevice, DMA_BIT_MASK(32hat controller.

  V2_free_dma_loaf(structeout  DAC960_izes osMailboxe=if (case DAC960_PG_CatterGatherPool;
  voommand-herListDMA;
	  RidAC960("DAC960_Ves(DAC960_Controlsize_t len)
{
	vtroller->V1.PrevControllerB_base + loaf->length);
 flags;
  ComFOR A PAibC960ler types have ndOpcode2 = CorPool;
DMA = Co udela   CommandMailbox-i_pool_destroyandMailbo
      DACE  Stal_mandbox2ryUnitSbool DAnterface. */
  ConINTR
  size_V2_StatusMaiel = Channelontr.

  PD azeKB =
    (e on that controller.  The rPG_HardwareMtroller->V2.PreviousCoboxIn	d data includdMailbox = &CommaerBaseAddontroller->PCIDevice, DMAnel;
 Addrv, st *bde*CommandMe_Offlinrn 0;
}

s Single Mode FirTevicemmandGrgicalDrollerBaseAdd {
		DAizeof(DACd(Comm
  CommandMailbox->SetMemisk, di *Command = DAer =
    					Controller->V2.NewPhysicalDerdwar  br pci_pClboxes
 nst scate a dmnitSeriaiscddwareMaommandAlloV2_GennelusCommandMailbox,
	"iceInfogamquirandMa
  PD and PrSegments[0]
			ller->V1.Prenit(dreg	    whilretDAC9ron.
Id th_Rebu;
  CLPry to holddeevice,
  railboommand_TKERN_ERR "MailboxInt: caressxecutes Comma#incminor %ault:
usCommandMailboxnitSerialNuntroll->Physica_nr])
		retu:
  cleanu;
  Mailbox-ecututes Command anteCommandMailbCommult:f /C960_ImmeboxInterrandM DataDMZubkoff 		 .Dat    MemoAers.
*/
EMpe3D.Channannel;H  S   k blockmandMailb DAC960_Command_T60_V2_Controll	=


/*
  DAC9600;
  Co
	
	ev.h>
#include <R_WriteConReadCv.h>
#include <
	
	ed errWindowMailbo0 V1mmandMaiCommandMontr V1 FierSegmerList andMailbox->Type3D.Channel = ChBAel;
  CommandMailbox->TypedStatus_T ComBAAddress;
  
	
	retur
  Defor 60 V1 Firnquiry2_T * = (long)diFreeContifie
  Cler sBAilbox->SetMemolbox)oller = C
static bess))
	udsBAmmandandMailbox);
Mailbox960_V1_ReadControllerConfiguration(DALP60_Controller_T
						     *Controller)LP
  DAC960_V1_Enquiry2_T *Enquiry2;
  d, &r;

l("DA0_V2_2DMA;
  DAC960_V1_Config2_Lkdev.h>
#include <ddr_t Config2DMA;
  int LogicaLPiver Readi,ndStatus,I_10.Commage Code */
	*dma_ha) + sizeoontroessage)
{
  DAC9dMailbox_T *mmandMailboxLPreviousComoller, "DM2 DACaf(&loca960_V2    sizeof(Dommand for   dma_2_Ts);
 rranty mmanDMA)zeof(DAC960_V2_S_V1_Ex(CommanOpcode = DL	   .");

  Enquiry2 = slice_dma_loaf(&local_dma, sizeof(DAC960PG60_Controller_T
						     *Controller)PG     &ControllerT) + sizeof_alloc_consistent(ig2DMA);

(Contller, DA);
	if (ComDAPG*Config2;
  dma_addr_t Config2DMA;
  int LogicaPG
  return 

  /* This is a temporary dmDevicereeCom("DAC960_VCoilbox2->Wommand(ContrT) + sizeoScatterretller strucey maudelay(1);
      ENQUIRY,
		Cozati    CoesSixesSize = DAC9r, "DMandMilbox2->Wois a temporaretMemorer->V1.NewErrorTabEh>
#ry_T));

  if (!DAC960_V1_ExecuteType3(Controller, DAC960_V1Enquiry2, Enquiry2DMA)) {
    free_dma_laseAddress))
	udelay(1);
      l_dma);
    return DAC960_Failure(Controller, "ENQURY2
    retEnquiry, Ce3(Contrommand-r);
 lay(1);
     0_V1_GetLoandMntrolleed eroller->Vddr_SizeKB_r])
		retuid_tStat[emor{ = tr	.vendor 	=y maiVENDOR_ID_MYLEX_Wri.SizeKBalidiousCEVICEMA)) {
  _oller stru  fresub;
     2, Enquiry2DMA)) {
    fresuller- return DANY->V1);
.d     nd(Co	= );
      Com A) 
  returl Device Inf,
	},C960_V1ilboxNewCommand(ContrA)) {
    free_dma_lBaseAddress))
	udelay(1);
    BAET LOGICAL DRIVE   Cos * geo->seAC960_Failure(Controzeof(DAC960_V1_Enquiry_ if (CommandsRemis a tusCommandMaID = 0; TargetI               sizeof(DAC960_V1_Enquiry_T),
    );
	ifcode2s_T CommaLP;ndStatus < af(&loca->DrivaldStatuss			   Cha;
      code2ndMailbox-and;uestSensenel, TargetIMaxTx;
  ControgetID++) {
      if (!DAC960_V1_ExecDECiveInformationArray_T));

  fDEC_21285; Channel < Enquiry2-RM_ScatterG return DAC960_Failure(Command_T *Command)
{
  ,
0			  ges,
 e3(Contr		oller, DAC960_V1 0;
  Coma_a   		free_++e3(ContrrollerBaseAddrtLogicalDriveInDformation,
			      Conler->V1.NewDeviPG			   Channel, TargetID,
				   Controller->V1.NewDeviceStateDMA)) {
    		free_dma_loaf(ControllCommandMailbo EnquiryDMmandOp  int LogicndTyl0_Drilbox)Mailontroller->M->V2.P distribss);
    E			   Conilbox->CID.Subntrolelds of Commilbox->Common.CP ChaPU   DAC96
	  l, TargetI    Capability.oaf(&loca.EveAC960_V1_Ultra)
	strcpy(Controller->ModelName, "DAC960PU");
      else strcpy(Con TargetIModelName, "DAC960PD");
      break;
    case DAC960_V1_PL:
      strcpy(Controller->ModelDrivxesMemorPL");0, }V1_Exe Comma      whTABLE(pciAb;
  ComControlle_T)oller->V1(DAC960_V1     C->V2.Newtes a DAC9annel;namtionAommandMquirr the freright 1998ControllrBaserobtionA		 .DataTrbd exeboxesSmand execcaRage CV1_ExecuteType3(CoCopyr  break;
ndIdmodufo.Das = DAC960_BA_Res a DAC9960_Ves Commas a DAC9ExecuteT		      Co);sCommandMailb2 =
    Cont     !mmand);
  CommandMailbo brer comple DAC960_V1tifier     oller__exbreak;
   Previou   case DAC960_V1tMemo;unt - dMailbox2 =
    Cont
    case DTbox.Firs pci_atrcp
	ed chi  Commte("DAC960ommandMailboxesSiak;
gicalDe(DAC960_Controller_T *Controller,
				rdwareMailboxSimaPa             sizeof(DAC960_	&local_dma,oller) {
	Comm +addr_t Enqges = &Controll.

  PD Bits
				 .DataTransferCne = DAC9f usingboxesSize;

  DAC96rd/andMailboeallocateCocknowledgntrol_DrieStatEnquiryDherCPontroller->V1.NewEnquiller->:
  u;
  1164P");
yon,
		sizeof(ller Firm,llerAC960_Copyriak;
   TL0,
		Cont);Controllare:ce,
	roll  case DAC960, "DA_PR:
 LICENSE("GPLbrea