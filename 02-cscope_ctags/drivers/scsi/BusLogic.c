
/*

  Linux Driver for BusLogic MultiMaster and FlashPoint SCSI Host Adapters

  Copyright 1995-1998 by Leonard N. Zubkoff <lnz@dandelion.com>

  This program is free software; you may redistribute and/or modify it under
  the terms of the GNU General Public License Version 2 as published by the
  Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY, without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
  for complete details.

  The author respectfully requests that any modifications to this software be
  sent directly to him for evaluation and testing.

  Special thanks to Wayne Yen, Jin-Lon Hon, and Alex Win of BusLogic, whose
  advice has been invaluable, to David Gentzel, for writing the original Linux
  BusLogic driver, and to Paul Gortmaker, for being such a dedicated test site.

  Finally, special thanks to Mylex/BusLogic for making the FlashPoint SCCB
  Manager available as freely redistributable source code.

*/

#define BusLogic_DriverVersion		"2.1.16"
#define BusLogic_DriverDate		"18 July 2002"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/stat.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/dma-mapping.h>
#include <scsi/scsicam.h>

#include <asm/dma.h>
#include <asm/io.h>
#include <asm/system.h>

#include <scsi/scsi.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <scsi/scsi_tcq.h>
#include "BusLogic.h"
#include "FlashPoint.c"

#ifndef FAILURE
#define FAILURE (-1)
#endif

static struct scsi_host_template Bus_Logic_template;

/*
  BusLogic_DriverOptionsCount is a count of the number of BusLogic Driver
  Options specifications provided via the Linux Kernel Command Line or via
  the Loadable Kernel Module Installation Facility.
*/

static int BusLogic_DriverOptionsCount;


/*
  BusLogic_DriverOptions is an array of Driver Options structures representing
  BusLogic Driver Options specifications provided via the Linux Kernel Command
  Line or via the Loadable Kernel Module Installation Facility.
*/

static struct BusLogic_DriverOptions BusLogic_DriverOptions[BusLogic_MaxHostAdapters];


/*
  BusLogic can be assigned a string by insmod.
*/

MODULE_LICENSE("GPL");
#ifdef MODULE
static char *BusLogic;
module_param(BusLogic, charp, 0);
#endif


/*
  BusLogic_ProbeOptions is a set of Probe Options to be applied across
  all BusLogic Host Adapters.
*/

static struct BusLogic_ProbeOptions BusLogic_ProbeOptions;


/*
  BusLogic_GlobalOptions is a set of Global Options to be applied across
  all BusLogic Host Adapters.
*/

static struct BusLogic_GlobalOptions BusLogic_GlobalOptions;

static LIST_HEAD(BusLogic_host_list);

/*
  BusLogic_ProbeInfoCount is the number of entries in BusLogic_ProbeInfoList.
*/

static int BusLogic_ProbeInfoCount;


/*
  BusLogic_ProbeInfoList is the list of I/O Addresses and Bus Probe Information
  to be checked for potential BusLogic Host Adapters.  It is initialized by
  interrogating the PCI Configuration Space on PCI machines as well as from the
  list of standard BusLogic I/O Addresses.
*/

static struct BusLogic_ProbeInfo *BusLogic_ProbeInfoList;


/*
  BusLogic_CommandFailureReason holds a string identifying the reason why a
  call to BusLogic_Command failed.  It is only non-NULL when BusLogic_Command
  returns a failure code.
*/

static char *BusLogic_CommandFailureReason;

/*
  BusLogic_AnnounceDriver announces the Driver Version and Date, Author's
  Name, Copyright Notice, and Electronic Mail Address.
*/

static void BusLogic_AnnounceDriver(struct BusLogic_HostAdapter *HostAdapter)
{
	BusLogic_Announce("***** BusLogic SCSI Driver Version " BusLogic_DriverVersion " of " BusLogic_DriverDate " *****\n", HostAdapter);
	BusLogic_Announce("Copyright 1995-1998 by Leonard N. Zubkoff " "<lnz@dandelion.com>\n", HostAdapter);
}


/*
  BusLogic_DriverInfo returns the Host Adapter Name to identify this SCSI
  Driver and Host Adapter.
*/

static const char *BusLogic_DriverInfo(struct Scsi_Host *Host)
{
	struct BusLogic_HostAdapter *HostAdapter = (struct BusLogic_HostAdapter *) Host->hostdata;
	return HostAdapter->FullModelName;
}

/*
  BusLogic_InitializeCCBs initializes a group of Command Control Blocks (CCBs)
  for Host Adapter from the BlockSize bytes located at BlockPointer.  The newly
  created CCBs are added to Host Adapter's free list.
*/

static void BusLogic_InitializeCCBs(struct BusLogic_HostAdapter *HostAdapter, void *BlockPointer, int BlockSize, dma_addr_t BlockPointerHandle)
{
	struct BusLogic_CCB *CCB = (struct BusLogic_CCB *) BlockPointer;
	unsigned int offset = 0;
	memset(BlockPointer, 0, BlockSize);
	CCB->AllocationGroupHead = BlockPointerHandle;
	CCB->AllocationGroupSize = BlockSize;
	while ((BlockSize -= sizeof(struct BusLogic_CCB)) >= 0) {
		CCB->Status = BusLogic_CCB_Free;
		CCB->HostAdapter = HostAdapter;
		CCB->DMA_Handle = (u32) BlockPointerHandle + offset;
		if (BusLogic_FlashPointHostAdapterP(HostAdapter)) {
			CCB->CallbackFunction = BusLogic_QueueCompletedCCB;
			CCB->BaseAddress = HostAdapter->FlashPointInfo.BaseAddress;
		}
		CCB->Next = HostAdapter->Free_CCBs;
		CCB->NextAll = HostAdapter->All_CCBs;
		HostAdapter->Free_CCBs = CCB;
		HostAdapter->All_CCBs = CCB;
		HostAdapter->AllocatedCCBs++;
		CCB++;
		offset += sizeof(struct BusLogic_CCB);
	}
}


/*
  BusLogic_CreateInitialCCBs allocates the initial CCBs for Host Adapter.
*/

static bool __init BusLogic_CreateInitialCCBs(struct BusLogic_HostAdapter *HostAdapter)
{
	int BlockSize = BusLogic_CCB_AllocationGroupSize * sizeof(struct BusLogic_CCB);
	void *BlockPointer;
	dma_addr_t BlockPointerHandle;
	while (HostAdapter->AllocatedCCBs < HostAdapter->InitialCCBs) {
		BlockPointer = pci_alloc_consistent(HostAdapter->PCI_Device, BlockSize, &BlockPointerHandle);
		if (BlockPointer == NULL) {
			BusLogic_Error("UNABLE TO ALLOCATE CCB GROUP - DETACHING\n", HostAdapter);
			return false;
		}
		BusLogic_InitializeCCBs(HostAdapter, BlockPointer, BlockSize, BlockPointerHandle);
	}
	return true;
}


/*
  BusLogic_DestroyCCBs deallocates the CCBs for Host Adapter.
*/

static void BusLogic_DestroyCCBs(struct BusLogic_HostAdapter *HostAdapter)
{
	struct BusLogic_CCB *NextCCB = HostAdapter->All_CCBs, *CCB, *Last_CCB = NULL;
	HostAdapter->All_CCBs = NULL;
	HostAdapter->Free_CCBs = NULL;
	while ((CCB = NextCCB) != NULL) {
		NextCCB = CCB->NextAll;
		if (CCB->AllocationGroupHead) {
			if (Last_CCB)
				pci_free_consistent(HostAdapter->PCI_Device, Last_CCB->AllocationGroupSize, Last_CCB, Last_CCB->AllocationGroupHead);
			Last_CCB = CCB;
		}
	}
	if (Last_CCB)
		pci_free_consistent(HostAdapter->PCI_Device, Last_CCB->AllocationGroupSize, Last_CCB, Last_CCB->AllocationGroupHead);
}


/*
  BusLogic_CreateAdditionalCCBs allocates Additional CCBs for Host Adapter.  If
  allocation fails and there are no remaining CCBs available, the Driver Queue
  Depth is decreased to a known safe value to avoid potential deadlocks when
  multiple host adapters share the same IRQ Channel.
*/

static void BusLogic_CreateAdditionalCCBs(struct BusLogic_HostAdapter *HostAdapter, int AdditionalCCBs, bool SuccessMessageP)
{
	int BlockSize = BusLogic_CCB_AllocationGroupSize * sizeof(struct BusLogic_CCB);
	int PreviouslyAllocated = HostAdapter->AllocatedCCBs;
	void *BlockPointer;
	dma_addr_t BlockPointerHandle;
	if (AdditionalCCBs <= 0)
		return;
	while (HostAdapter->AllocatedCCBs - PreviouslyAllocated < AdditionalCCBs) {
		BlockPointer = pci_alloc_consistent(HostAdapter->PCI_Device, BlockSize, &BlockPointerHandle);
		if (BlockPointer == NULL)
			break;
		BusLogic_InitializeCCBs(HostAdapter, BlockPointer, BlockSize, BlockPointerHandle);
	}
	if (HostAdapter->AllocatedCCBs > PreviouslyAllocated) {
		if (SuccessMessageP)
			BusLogic_Notice("Allocated %d additional CCBs (total now %d)\n", HostAdapter, HostAdapter->AllocatedCCBs - PreviouslyAllocated, HostAdapter->AllocatedCCBs);
		return;
	}
	BusLogic_Notice("Failed to allocate additional CCBs\n", HostAdapter);
	if (HostAdapter->DriverQueueDepth > HostAdapter->AllocatedCCBs - HostAdapter->TargetDeviceCount) {
		HostAdapter->DriverQueueDepth = HostAdapter->AllocatedCCBs - HostAdapter->TargetDeviceCount;
		HostAdapter->SCSI_Host->can_queue = HostAdapter->DriverQueueDepth;
	}
}

/*
  BusLogic_AllocateCCB allocates a CCB from Host Adapter's free list,
  allocating more memory from the Kernel if necessary.  The Host Adapter's
  Lock should already have been acquired by the caller.
*/

static struct BusLogic_CCB *BusLogic_AllocateCCB(struct BusLogic_HostAdapter
						 *HostAdapter)
{
	static unsigned long SerialNumber = 0;
	struct BusLogic_CCB *CCB;
	CCB = HostAdapter->Free_CCBs;
	if (CCB != NULL) {
		CCB->SerialNumber = ++SerialNumber;
		HostAdapter->Free_CCBs = CCB->Next;
		CCB->Next = NULL;
		if (HostAdapter->Free_CCBs == NULL)
			BusLogic_CreateAdditionalCCBs(HostAdapter, HostAdapter->IncrementalCCBs, true);
		return CCB;
	}
	BusLogic_CreateAdditionalCCBs(HostAdapter, HostAdapter->IncrementalCCBs, true);
	CCB = HostAdapter->Free_CCBs;
	if (CCB == NULL)
		return NULL;
	CCB->SerialNumber = ++SerialNumber;
	HostAdapter->Free_CCBs = CCB->Next;
	CCB->Next = NULL;
	return CCB;
}


/*
  BusLogic_DeallocateCCB deallocates a CCB, returning it to the Host Adapter's
  free list.  The Host Adapter's Lock should already have been acquired by the
  caller.
*/

static void BusLogic_DeallocateCCB(struct BusLogic_CCB *CCB)
{
	struct BusLogic_HostAdapter *HostAdapter = CCB->HostAdapter;

	scsi_dma_unmap(CCB->Command);
	pci_unmap_single(HostAdapter->PCI_Device, CCB->SenseDataPointer,
			 CCB->SenseDataLength, PCI_DMA_FROMDEVICE);

	CCB->Command = NULL;
	CCB->Status = BusLogic_CCB_Free;
	CCB->Next = HostAdapter->Free_CCBs;
	HostAdapter->Free_CCBs = CCB;
}


/*
  BusLogic_Command sends the command OperationCode to HostAdapter, optionally
  providing ParameterLength bytes of ParameterData and receiving at most
  ReplyLength bytes of ReplyData; any excess reply data is received but
  discarded.

  On success, this function returns the number of reply bytes read from
  the Host Adapter (including any discarded data); on failure, it returns
  -1 if the command was invalid, or -2 if a timeout occurred.

  BusLogic_Command is called exclusively during host adapter detection and
  initialization, so performance and latency are not critical, and exclusive
  access to the Host Adapter hardware is assumed.  Once the host adapter and
  driver are initialized, the only Host Adapter command that is issued is the
  single byte Execute Mailbox Command operation code, which does not require
  waiting for the Host Adapter Ready bit to be set in the Status Register.
*/

static int BusLogic_Command(struct BusLogic_HostAdapter *HostAdapter, enum BusLogic_OperationCode OperationCode, void *ParameterData, int ParameterLength, void *ReplyData, int ReplyLength)
{
	unsigned char *ParameterPointer = (unsigned char *) ParameterData;
	unsigned char *ReplyPointer = (unsigned char *) ReplyData;
	union BusLogic_StatusRegister StatusRegister;
	union BusLogic_InterruptRegister InterruptRegister;
	unsigned long ProcessorFlags = 0;
	int ReplyBytes = 0, Result;
	long TimeoutCounter;
	/*
	   Clear out the Reply Data if provided.
	 */
	if (ReplyLength > 0)
		memset(ReplyData, 0, ReplyLength);
	/*
	   If the IRQ Channel has not yet been acquired, then interrupts must be
	   disabled while issuing host adapter commands since a Command Complete
	   interrupt could occur if the IRQ Channel was previously enabled by another
	   BusLogic Host Adapter or another driver sharing the same IRQ Channel.
	 */
	if (!HostAdapter->IRQ_ChannelAcquired)
		local_irq_save(ProcessorFlags);
	/*
	   Wait for the Host Adapter Ready bit to be set and the Command/Parameter
	   Register Busy bit to be reset in the Status Register.
	 */
	TimeoutCounter = 10000;
	while (--TimeoutCounter >= 0) {
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (StatusRegister.sr.HostAdapterReady && !StatusRegister.sr.CommandParameterRegisterBusy)
			break;
		udelay(100);
	}
	if (TimeoutCounter < 0) {
		BusLogic_CommandFailureReason = "Timeout waiting for Host Adapter Ready";
		Result = -2;
		goto Done;
	}
	/*
	   Write the OperationCode to the Command/Parameter Register.
	 */
	HostAdapter->HostAdapterCommandCompleted = false;
	BusLogic_WriteCommandParameterRegister(HostAdapter, OperationCode);
	/*
	   Write any additional Parameter Bytes.
	 */
	TimeoutCounter = 10000;
	while (ParameterLength > 0 && --TimeoutCounter >= 0) {
		/*
		   Wait 100 microseconds to give the Host Adapter enough time to determine
		   whether the last value written to the Command/Parameter Register was
		   valid or not.  If the Command Complete bit is set in the Interrupt
		   Register, then the Command Invalid bit in the Status Register will be
		   reset if the Operation Code or Parameter was valid and the command
		   has completed, or set if the Operation Code or Parameter was invalid.
		   If the Data In Register Ready bit is set in the Status Register, then
		   the Operation Code was valid, and data is waiting to be read back
		   from the Host Adapter.  Otherwise, wait for the Command/Parameter
		   Register Busy bit in the Status Register to be reset.
		 */
		udelay(100);
		InterruptRegister.All = BusLogic_ReadInterruptRegister(HostAdapter);
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (InterruptRegister.ir.CommandComplete)
			break;
		if (HostAdapter->HostAdapterCommandCompleted)
			break;
		if (StatusRegister.sr.DataInRegisterReady)
			break;
		if (StatusRegister.sr.CommandParameterRegisterBusy)
			continue;
		BusLogic_WriteCommandParameterRegister(HostAdapter, *ParameterPointer++);
		ParameterLength--;
	}
	if (TimeoutCounter < 0) {
		BusLogic_CommandFailureReason = "Timeout waiting for Parameter Acceptance";
		Result = -2;
		goto Done;
	}
	/*
	   The Modify I/O Address command does not cause a Command Complete Interrupt.
	 */
	if (OperationCode == BusLogic_ModifyIOAddress) {
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (StatusRegister.sr.CommandInvalid) {
			BusLogic_CommandFailureReason = "Modify I/O Address Invalid";
			Result = -1;
			goto Done;
		}
		if (BusLogic_GlobalOptions.TraceConfiguration)
			BusLogic_Notice("BusLogic_Command(%02X) Status = %02X: " "(Modify I/O Address)\n", HostAdapter, OperationCode, StatusRegister.All);
		Result = 0;
		goto Done;
	}
	/*
	   Select an appropriate timeout value for awaiting command completion.
	 */
	switch (OperationCode) {
	case BusLogic_InquireInstalledDevicesID0to7:
	case BusLogic_InquireInstalledDevicesID8to15:
	case BusLogic_InquireTargetDevices:
		/* Approximately 60 seconds. */
		TimeoutCounter = 60 * 10000;
		break;
	default:
		/* Approximately 1 second. */
		TimeoutCounter = 10000;
		break;
	}
	/*
	   Receive any Reply Bytes, waiting for either the Command Complete bit to
	   be set in the Interrupt Register, or for the Interrupt Handler to set the
	   Host Adapter Command Completed bit in the Host Adapter structure.
	 */
	while (--TimeoutCounter >= 0) {
		InterruptRegister.All = BusLogic_ReadInterruptRegister(HostAdapter);
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (InterruptRegister.ir.CommandComplete)
			break;
		if (HostAdapter->HostAdapterCommandCompleted)
			break;
		if (StatusRegister.sr.DataInRegisterReady) {
			if (++ReplyBytes <= ReplyLength)
				*ReplyPointer++ = BusLogic_ReadDataInRegister(HostAdapter);
			else
				BusLogic_ReadDataInRegister(HostAdapter);
		}
		if (OperationCode == BusLogic_FetchHostAdapterLocalRAM && StatusRegister.sr.HostAdapterReady)
			break;
		udelay(100);
	}
	if (TimeoutCounter < 0) {
		BusLogic_CommandFailureReason = "Timeout waiting for Command Complete";
		Result = -2;
		goto Done;
	}
	/*
	   Clear any pending Command Complete Interrupt.
	 */
	BusLogic_InterruptReset(HostAdapter);
	/*
	   Provide tracing information if requested.
	 */
	if (BusLogic_GlobalOptions.TraceConfiguration) {
		int i;
		BusLogic_Notice("BusLogic_Command(%02X) Status = %02X: %2d ==> %2d:", HostAdapter, OperationCode, StatusRegister.All, ReplyLength, ReplyBytes);
		if (ReplyLength > ReplyBytes)
			ReplyLength = ReplyBytes;
		for (i = 0; i < ReplyLength; i++)
			BusLogic_Notice(" %02X", HostAdapter, ((unsigned char *) ReplyData)[i]);
		BusLogic_Notice("\n", HostAdapter);
	}
	/*
	   Process Command Invalid conditions.
	 */
	if (StatusRegister.sr.CommandInvalid) {
		/*
		   Some early BusLogic Host Adapters may not recover properly from
		   a Command Invalid condition, so if this appears to be the case,
		   a Soft Reset is issued to the Host Adapter.  Potentially invalid
		   commands are never attempted after Mailbox Initialization is
		   performed, so there should be no Host Adapter state lost by a
		   Soft Reset in response to a Command Invalid condition.
		 */
		udelay(1000);
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (StatusRegister.sr.CommandInvalid ||
		    StatusRegister.sr.Reserved ||
		    StatusRegister.sr.DataInRegisterReady ||
		    StatusRegister.sr.CommandParameterRegisterBusy || !StatusRegister.sr.HostAdapterReady || !StatusRegister.sr.InitializationRequired || StatusRegister.sr.DiagnosticActive || StatusRegister.sr.DiagnosticFailure) {
			BusLogic_SoftReset(HostAdapter);
			udelay(1000);
		}
		BusLogic_CommandFailureReason = "Command Invalid";
		Result = -1;
		goto Done;
	}
	/*
	   Handle Excess Parameters Supplied conditions.
	 */
	if (ParameterLength > 0) {
		BusLogic_CommandFailureReason = "Excess Parameters Supplied";
		Result = -1;
		goto Done;
	}
	/*
	   Indicate the command completed successfully.
	 */
	BusLogic_CommandFailureReason = NULL;
	Result = ReplyBytes;
	/*
	   Restore the interrupt status if necessary and return.
	 */
      Done:
	if (!HostAdapter->IRQ_ChannelAcquired)
		local_irq_restore(ProcessorFlags);
	return Result;
}


/*
  BusLogic_AppendProbeAddressISA appends a single ISA I/O Address to the list
  of I/O Address and Bus Probe Information to be checked for potential BusLogic
  Host Adapters.
*/

static void __init BusLogic_AppendProbeAddressISA(unsigned long IO_Address)
{
	struct BusLogic_ProbeInfo *ProbeInfo;
	if (BusLogic_ProbeInfoCount >= BusLogic_MaxHostAdapters)
		return;
	ProbeInfo = &BusLogic_ProbeInfoList[BusLogic_ProbeInfoCount++];
	ProbeInfo->HostAdapterType = BusLogic_MultiMaster;
	ProbeInfo->HostAdapterBusType = BusLogic_ISA_Bus;
	ProbeInfo->IO_Address = IO_Address;
	ProbeInfo->PCI_Device = NULL;
}


/*
  BusLogic_InitializeProbeInfoListISA initializes the list of I/O Address and
  Bus Probe Information to be checked for potential BusLogic SCSI Host Adapters
  only from the list of standard BusLogic MultiMaster ISA I/O Addresses.
*/

static void __init BusLogic_InitializeProbeInfoListISA(struct BusLogic_HostAdapter
						       *PrototypeHostAdapter)
{
	/*
	   If BusLogic Driver Options specifications requested that ISA Bus Probes
	   be inhibited, do not proceed further.
	 */
	if (BusLogic_ProbeOptions.NoProbeISA)
		return;
	/*
	   Append the list of standard BusLogic MultiMaster ISA I/O Addresses.
	 */
	if (!BusLogic_ProbeOptions.LimitedProbeISA || BusLogic_ProbeOptions.Probe330)
		BusLogic_AppendProbeAddressISA(0x330);
	if (!BusLogic_ProbeOptions.LimitedProbeISA || BusLogic_ProbeOptions.Probe334)
		BusLogic_AppendProbeAddressISA(0x334);
	if (!BusLogic_ProbeOptions.LimitedProbeISA || BusLogic_ProbeOptions.Probe230)
		BusLogic_AppendProbeAddressISA(0x230);
	if (!BusLogic_ProbeOptions.LimitedProbeISA || BusLogic_ProbeOptions.Probe234)
		BusLogic_AppendProbeAddressISA(0x234);
	if (!BusLogic_ProbeOptions.LimitedProbeISA || BusLogic_ProbeOptions.Probe130)
		BusLogic_AppendProbeAddressISA(0x130);
	if (!BusLogic_ProbeOptions.LimitedProbeISA || BusLogic_ProbeOptions.Probe134)
		BusLogic_AppendProbeAddressISA(0x134);
}


#ifdef CONFIG_PCI


/*
  BusLogic_SortProbeInfo sorts a section of BusLogic_ProbeInfoList in order
  of increasing PCI Bus and Device Number.
*/

static void __init BusLogic_SortProbeInfo(struct BusLogic_ProbeInfo *ProbeInfoList, int ProbeInfoCount)
{
	int LastInterchange = ProbeInfoCount - 1, Bound, j;
	while (LastInterchange > 0) {
		Bound = LastInterchange;
		LastInterchange = 0;
		for (j = 0; j < Bound; j++) {
			struct BusLogic_ProbeInfo *ProbeInfo1 = &ProbeInfoList[j];
			struct BusLogic_ProbeInfo *ProbeInfo2 = &ProbeInfoList[j + 1];
			if (ProbeInfo1->Bus > ProbeInfo2->Bus || (ProbeInfo1->Bus == ProbeInfo2->Bus && (ProbeInfo1->Device > ProbeInfo2->Device))) {
				struct BusLogic_ProbeInfo TempProbeInfo;
				memcpy(&TempProbeInfo, ProbeInfo1, sizeof(struct BusLogic_ProbeInfo));
				memcpy(ProbeInfo1, ProbeInfo2, sizeof(struct BusLogic_ProbeInfo));
				memcpy(ProbeInfo2, &TempProbeInfo, sizeof(struct BusLogic_ProbeInfo));
				LastInterchange = j;
			}
		}
	}
}


/*
  BusLogic_InitializeMultiMasterProbeInfo initializes the list of I/O Address
  and Bus Probe Information to be checked for potential BusLogic MultiMaster
  SCSI Host Adapters by interrogating the PCI Configuration Space on PCI
  machines as well as from the list of standard BusLogic MultiMaster ISA
  I/O Addresses.  It returns the number of PCI MultiMaster Host Adapters found.
*/

static int __init BusLogic_InitializeMultiMasterProbeInfo(struct BusLogic_HostAdapter
							  *PrototypeHostAdapter)
{
	struct BusLogic_ProbeInfo *PrimaryProbeInfo = &BusLogic_ProbeInfoList[BusLogic_ProbeInfoCount];
	int NonPrimaryPCIMultiMasterIndex = BusLogic_ProbeInfoCount + 1;
	int NonPrimaryPCIMultiMasterCount = 0, PCIMultiMasterCount = 0;
	bool ForceBusDeviceScanningOrder = false;
	bool ForceBusDeviceScanningOrderChecked = false;
	bool StandardAddressSeen[6];
	struct pci_dev *PCI_Device = NULL;
	int i;
	if (BusLogic_ProbeInfoCount >= BusLogic_MaxHostAdapters)
		return 0;
	BusLogic_ProbeInfoCount++;
	for (i = 0; i < 6; i++)
		StandardAddressSeen[i] = false;
	/*
	   Iterate over the MultiMaster PCI Host Adapters.  For each enumerated host
	   adapter, determine whether its ISA Compatible I/O Port is enabled and if
	   so, whether it is assigned the Primary I/O Address.  A host adapter that is
	   assigned the Primary I/O Address will always be the preferred boot device.
	   The MultiMaster BIOS will first recognize a host adapter at the Primary I/O
	   Address, then any other PCI host adapters, and finally any host adapters
	   located at the remaining standard ISA I/O Addresses.  When a PCI host
	   adapter is found with its ISA Compatible I/O Port enabled, a command is
	   issued to disable the ISA Compatible I/O Port, and it is noted that the
	   particular standard ISA I/O Address need not be probed.
	 */
	PrimaryProbeInfo->IO_Address = 0;
	while ((PCI_Device = pci_get_device(PCI_VENDOR_ID_BUSLOGIC, PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER, PCI_Device)) != NULL) {
		struct BusLogic_HostAdapter *HostAdapter = PrototypeHostAdapter;
		struct BusLogic_PCIHostAdapterInformation PCIHostAdapterInformation;
		enum BusLogic_ISACompatibleIOPort ModifyIOAddressRequest;
		unsigned char Bus;
		unsigned char Device;
		unsigned int IRQ_Channel;
		unsigned long BaseAddress0;
		unsigned long BaseAddress1;
		unsigned long IO_Address;
		unsigned long PCI_Address;

		if (pci_enable_device(PCI_Device))
			continue;

		if (pci_set_dma_mask(PCI_Device, DMA_BIT_MASK(32) ))
			continue;

		Bus = PCI_Device->bus->number;
		Device = PCI_Device->devfn >> 3;
		IRQ_Channel = PCI_Device->irq;
		IO_Address = BaseAddress0 = pci_resource_start(PCI_Device, 0);
		PCI_Address = BaseAddress1 = pci_resource_start(PCI_Device, 1);

		if (pci_resource_flags(PCI_Device, 0) & IORESOURCE_MEM) {
			BusLogic_Error("BusLogic: Base Address0 0x%X not I/O for " "MultiMaster Host Adapter\n", NULL, BaseAddress0);
			BusLogic_Error("at PCI Bus %d Device %d I/O Address 0x%X\n", NULL, Bus, Device, IO_Address);
			continue;
		}
		if (pci_resource_flags(PCI_Device, 1) & IORESOURCE_IO) {
			BusLogic_Error("BusLogic: Base Address1 0x%X not Memory for " "MultiMaster Host Adapter\n", NULL, BaseAddress1);
			BusLogic_Error("at PCI Bus %d Device %d PCI Address 0x%X\n", NULL, Bus, Device, PCI_Address);
			continue;
		}
		if (IRQ_Channel == 0) {
			BusLogic_Error("BusLogic: IRQ Channel %d invalid for " "MultiMaster Host Adapter\n", NULL, IRQ_Channel);
			BusLogic_Error("at PCI Bus %d Device %d I/O Address 0x%X\n", NULL, Bus, Device, IO_Address);
			continue;
		}
		if (BusLogic_GlobalOptions.TraceProbe) {
			BusLogic_Notice("BusLogic: PCI MultiMaster Host Adapter " "detected at\n", NULL);
			BusLogic_Notice("BusLogic: PCI Bus %d Device %d I/O Address " "0x%X PCI Address 0x%X\n", NULL, Bus, Device, IO_Address, PCI_Address);
		}
		/*
		   Issue the Inquire PCI Host Adapter Information command to determine
		   the ISA Compatible I/O Port.  If the ISA Compatible I/O Port is
		   known and enabled, note that the particular Standard ISA I/O
		   Address should not be probed.
		 */
		HostAdapter->IO_Address = IO_Address;
		BusLogic_InterruptReset(HostAdapter);
		if (BusLogic_Command(HostAdapter, BusLogic_InquirePCIHostAdapterInformation, NULL, 0, &PCIHostAdapterInformation, sizeof(PCIHostAdapterInformation))
		    == sizeof(PCIHostAdapterInformation)) {
			if (PCIHostAdapterInformation.ISACompatibleIOPort < 6)
				StandardAddressSeen[PCIHostAdapterInformation.ISACompatibleIOPort] = true;
		} else
			PCIHostAdapterInformation.ISACompatibleIOPort = BusLogic_IO_Disable;
		/*
		 * Issue the Modify I/O Address command to disable the ISA Compatible
		 * I/O Port.  On PCI Host Adapters, the Modify I/O Address command
		 * allows modification of the ISA compatible I/O Address that the Host
		 * Adapter responds to; it does not affect the PCI compliant I/O Address
		 * assigned at system initialization.
		 */
		ModifyIOAddressRequest = BusLogic_IO_Disable;
		BusLogic_Command(HostAdapter, BusLogic_ModifyIOAddress, &ModifyIOAddressRequest, sizeof(ModifyIOAddressRequest), NULL, 0);
		/*
		   For the first MultiMaster Host Adapter enumerated, issue the Fetch
		   Host Adapter Local RAM command to read byte 45 of the AutoSCSI area,
		   for the setting of the "Use Bus And Device # For PCI Scanning Seq."
		   option.  Issue the Inquire Board ID command since this option is
		   only valid for the BT-948/958/958D.
		 */
		if (!ForceBusDeviceScanningOrderChecked) {
			struct BusLogic_FetchHostAdapterLocalRAMRequest FetchHostAdapterLocalRAMRequest;
			struct BusLogic_AutoSCSIByte45 AutoSCSIByte45;
			struct BusLogic_BoardID BoardID;
			FetchHostAdapterLocalRAMRequest.ByteOffset = BusLogic_AutoSCSI_BaseOffset + 45;
			FetchHostAdapterLocalRAMRequest.ByteCount = sizeof(AutoSCSIByte45);
			BusLogic_Command(HostAdapter, BusLogic_FetchHostAdapterLocalRAM, &FetchHostAdapterLocalRAMRequest, sizeof(FetchHostAdapterLocalRAMRequest), &AutoSCSIByte45, sizeof(AutoSCSIByte45));
			BusLogic_Command(HostAdapter, BusLogic_InquireBoardID, NULL, 0, &BoardID, sizeof(BoardID));
			if (BoardID.FirmwareVersion1stDigit == '5')
				ForceBusDeviceScanningOrder = AutoSCSIByte45.ForceBusDeviceScanningOrder;
			ForceBusDeviceScanningOrderChecked = true;
		}
		/*
		   Determine whether this MultiMaster Host Adapter has its ISA
		   Compatible I/O Port enabled and is assigned the Primary I/O Address.
		   If it does, then it is the Primary MultiMaster Host Adapter and must
		   be recognized first.  If it does not, then it is added to the list
		   for probing after any Primary MultiMaster Host Adapter is probed.
		 */
		if (PCIHostAdapterInformation.ISACompatibleIOPort == BusLogic_IO_330) {
			PrimaryProbeInfo->HostAdapterType = BusLogic_MultiMaster;
			PrimaryProbeInfo->HostAdapterBusType = BusLogic_PCI_Bus;
			PrimaryProbeInfo->IO_Address = IO_Address;
			PrimaryProbeInfo->PCI_Address = PCI_Address;
			PrimaryProbeInfo->Bus = Bus;
			PrimaryProbeInfo->Device = Device;
			PrimaryProbeInfo->IRQ_Channel = IRQ_Channel;
			PrimaryProbeInfo->PCI_Device = pci_dev_get(PCI_Device);
			PCIMultiMasterCount++;
		} else if (BusLogic_ProbeInfoCount < BusLogic_MaxHostAdapters) {
			struct BusLogic_ProbeInfo *ProbeInfo = &BusLogic_ProbeInfoList[BusLogic_ProbeInfoCount++];
			ProbeInfo->HostAdapterType = BusLogic_MultiMaster;
			ProbeInfo->HostAdapterBusType = BusLogic_PCI_Bus;
			ProbeInfo->IO_Address = IO_Address;
			ProbeInfo->PCI_Address = PCI_Address;
			ProbeInfo->Bus = Bus;
			ProbeInfo->Device = Device;
			ProbeInfo->IRQ_Channel = IRQ_Channel;
			ProbeInfo->PCI_Device = pci_dev_get(PCI_Device);
			NonPrimaryPCIMultiMasterCount++;
			PCIMultiMasterCount++;
		} else
			BusLogic_Warning("BusLogic: Too many Host Adapters " "detected\n", NULL);
	}
	/*
	   If the AutoSCSI "Use Bus And Device # For PCI Scanning Seq." option is ON
	   for the first enumerated MultiMaster Host Adapter, and if that host adapter
	   is a BT-948/958/958D, then the MultiMaster BIOS will recognize MultiMaster
	   Host Adapters in the order of increasing PCI Bus and Device Number.  In
	   that case, sort the probe information into the same order the BIOS uses.
	   If this option is OFF, then the MultiMaster BIOS will recognize MultiMaster
	   Host Adapters in the order they are enumerated by the PCI BIOS, and hence
	   no sorting is necessary.
	 */
	if (ForceBusDeviceScanningOrder)
		BusLogic_SortProbeInfo(&BusLogic_ProbeInfoList[NonPrimaryPCIMultiMasterIndex], NonPrimaryPCIMultiMasterCount);
	/*
	   If no PCI MultiMaster Host Adapter is assigned the Primary I/O Address,
	   then the Primary I/O Address must be probed explicitly before any PCI
	   host adapters are probed.
	 */
	if (!BusLogic_ProbeOptions.NoProbeISA)
		if (PrimaryProbeInfo->IO_Address == 0 &&
				(!BusLogic_ProbeOptions.LimitedProbeISA ||
				 BusLogic_ProbeOptions.Probe330)) {
			PrimaryProbeInfo->HostAdapterType = BusLogic_MultiMaster;
			PrimaryProbeInfo->HostAdapterBusType = BusLogic_ISA_Bus;
			PrimaryProbeInfo->IO_Address = 0x330;
		}
	/*
	   Append the list of standard BusLogic MultiMaster ISA I/O Addresses,
	   omitting the Primary I/O Address which has already been handled.
	 */
	if (!BusLogic_ProbeOptions.NoProbeISA) {
		if (!StandardAddressSeen[1] &&
				(!BusLogic_ProbeOptions.LimitedProbeISA ||
				 BusLogic_ProbeOptions.Probe334))
			BusLogic_AppendProbeAddressISA(0x334);
		if (!StandardAddressSeen[2] &&
				(!BusLogic_ProbeOptions.LimitedProbeISA ||
				 BusLogic_ProbeOptions.Probe230))
			BusLogic_AppendProbeAddressISA(0x230);
		if (!StandardAddressSeen[3] &&
				(!BusLogic_ProbeOptions.LimitedProbeISA ||
				 BusLogic_ProbeOptions.Probe234))
			BusLogic_AppendProbeAddressISA(0x234);
		if (!StandardAddressSeen[4] &&
				(!BusLogic_ProbeOptions.LimitedProbeISA ||
				 BusLogic_ProbeOptions.Probe130))
			BusLogic_AppendProbeAddressISA(0x130);
		if (!StandardAddressSeen[5] &&
				(!BusLogic_ProbeOptions.LimitedProbeISA ||
				 BusLogic_ProbeOptions.Probe134))
			BusLogic_AppendProbeAddressISA(0x134);
	}
	/*
	   Iterate over the older non-compliant MultiMaster PCI Host Adapters,
	   noting the PCI bus location and assigned IRQ Channel.
	 */
	PCI_Device = NULL;
	while ((PCI_Device = pci_get_device(PCI_VENDOR_ID_BUSLOGIC, PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER_NC, PCI_Device)) != NULL) {
		unsigned char Bus;
		unsigned char Device;
		unsigned int IRQ_Channel;
		unsigned long IO_Address;

		if (pci_enable_device(PCI_Device))
			continue;

		if (pci_set_dma_mask(PCI_Device, DMA_BIT_MASK(32)))
			continue;

		Bus = PCI_Device->bus->number;
		Device = PCI_Device->devfn >> 3;
		IRQ_Channel = PCI_Device->irq;
		IO_Address = pci_resource_start(PCI_Device, 0);

		if (IO_Address == 0 || IRQ_Channel == 0)
			continue;
		for (i = 0; i < BusLogic_ProbeInfoCount; i++) {
			struct BusLogic_ProbeInfo *ProbeInfo = &BusLogic_ProbeInfoList[i];
			if (ProbeInfo->IO_Address == IO_Address && ProbeInfo->HostAdapterType == BusLogic_MultiMaster) {
				ProbeInfo->HostAdapterBusType = BusLogic_PCI_Bus;
				ProbeInfo->PCI_Address = 0;
				ProbeInfo->Bus = Bus;
				ProbeInfo->Device = Device;
				ProbeInfo->IRQ_Channel = IRQ_Channel;
				ProbeInfo->PCI_Device = pci_dev_get(PCI_Device);
				break;
			}
		}
	}
	return PCIMultiMasterCount;
}


/*
  BusLogic_InitializeFlashPointProbeInfo initializes the list of I/O Address
  and Bus Probe Information to be checked for potential BusLogic FlashPoint
  Host Adapters by interrogating the PCI Configuration Space.  It returns the
  number of FlashPoint Host Adapters found.
*/

static int __init BusLogic_InitializeFlashPointProbeInfo(struct BusLogic_HostAdapter
							 *PrototypeHostAdapter)
{
	int FlashPointIndex = BusLogic_ProbeInfoCount, FlashPointCount = 0;
	struct pci_dev *PCI_Device = NULL;
	/*
	   Interrogate PCI Configuration Space for any FlashPoint Host Adapters.
	 */
	while ((PCI_Device = pci_get_device(PCI_VENDOR_ID_BUSLOGIC, PCI_DEVICE_ID_BUSLOGIC_FLASHPOINT, PCI_Device)) != NULL) {
		unsigned char Bus;
		unsigned char Device;
		unsigned int IRQ_Channel;
		unsigned long BaseAddress0;
		unsigned long BaseAddress1;
		unsigned long IO_Address;
		unsigned long PCI_Address;

		if (pci_enable_device(PCI_Device))
			continue;

		if (pci_set_dma_mask(PCI_Device, DMA_BIT_MASK(32)))
			continue;

		Bus = PCI_Device->bus->number;
		Device = PCI_Device->devfn >> 3;
		IRQ_Channel = PCI_Device->irq;
		IO_Address = BaseAddress0 = pci_resource_start(PCI_Device, 0);
		PCI_Address = BaseAddress1 = pci_resource_start(PCI_Device, 1);
#ifdef CONFIG_SCSI_FLASHPOINT
		if (pci_resource_flags(PCI_Device, 0) & IORESOURCE_MEM) {
			BusLogic_Error("BusLogic: Base Address0 0x%X not I/O for " "FlashPoint Host Adapter\n", NULL, BaseAddress0);
			BusLogic_Error("at PCI Bus %d Device %d I/O Address 0x%X\n", NULL, Bus, Device, IO_Address);
			continue;
		}
		if (pci_resource_flags(PCI_Device, 1) & IORESOURCE_IO) {
			BusLogic_Error("BusLogic: Base Address1 0x%X not Memory for " "FlashPoint Host Adapter\n", NULL, BaseAddress1);
			BusLogic_Error("at PCI Bus %d Device %d PCI Address 0x%X\n", NULL, Bus, Device, PCI_Address);
			continue;
		}
		if (IRQ_Channel == 0) {
			BusLogic_Error("BusLogic: IRQ Channel %d invalid for " "FlashPoint Host Adapter\n", NULL, IRQ_Channel);
			BusLogic_Error("at PCI Bus %d Device %d I/O Address 0x%X\n", NULL, Bus, Device, IO_Address);
			continue;
		}
		if (BusLogic_GlobalOptions.TraceProbe) {
			BusLogic_Notice("BusLogic: FlashPoint Host Adapter " "detected at\n", NULL);
			BusLogic_Notice("BusLogic: PCI Bus %d Device %d I/O Address " "0x%X PCI Address 0x%X\n", NULL, Bus, Device, IO_Address, PCI_Address);
		}
		if (BusLogic_ProbeInfoCount < BusLogic_MaxHostAdapters) {
			struct BusLogic_ProbeInfo *ProbeInfo = &BusLogic_ProbeInfoList[BusLogic_ProbeInfoCount++];
			ProbeInfo->HostAdapterType = BusLogic_FlashPoint;
			ProbeInfo->HostAdapterBusType = BusLogic_PCI_Bus;
			ProbeInfo->IO_Address = IO_Address;
			ProbeInfo->PCI_Address = PCI_Address;
			ProbeInfo->Bus = Bus;
			ProbeInfo->Device = Device;
			ProbeInfo->IRQ_Channel = IRQ_Channel;
			ProbeInfo->PCI_Device = pci_dev_get(PCI_Device);
			FlashPointCount++;
		} else
			BusLogic_Warning("BusLogic: Too many Host Adapters " "detected\n", NULL);
#else
		BusLogic_Error("BusLogic: FlashPoint Host Adapter detected at " "PCI Bus %d Device %d\n", NULL, Bus, Device);
		BusLogic_Error("BusLogic: I/O Address 0x%X PCI Address 0x%X, irq %d, " "but FlashPoint\n", NULL, IO_Address, PCI_Address, IRQ_Channel);
		BusLogic_Error("BusLogic: support was omitted in this kernel " "configuration.\n", NULL);
#endif
	}
	/*
	   The FlashPoint BIOS will scan for FlashPoint Host Adapters in the order of
	   increasing PCI Bus and Device Number, so sort the probe information into
	   the same order the BIOS uses.
	 */
	BusLogic_SortProbeInfo(&BusLogic_ProbeInfoList[FlashPointIndex], FlashPointCount);
	return FlashPointCount;
}


/*
  BusLogic_InitializeProbeInfoList initializes the list of I/O Address and Bus
  Probe Information to be checked for potential BusLogic SCSI Host Adapters by
  interrogating the PCI Configuration Space on PCI machines as well as from the
  list of standard BusLogic MultiMaster ISA I/O Addresses.  By default, if both
  FlashPoint and PCI MultiMaster Host Adapters are present, this driver will
  probe for FlashPoint Host Adapters first unless the BIOS primary disk is
  controlled by the first PCI MultiMaster Host Adapter, in which case
  MultiMaster Host Adapters will be probed first.  The BusLogic Driver Options
  specifications "MultiMasterFirst" and "FlashPointFirst" can be used to force
  a particular probe order.
*/

static void __init BusLogic_InitializeProbeInfoList(struct BusLogic_HostAdapter
						    *PrototypeHostAdapter)
{
	/*
	   If a PCI BIOS is present, interrogate it for MultiMaster and FlashPoint
	   Host Adapters; otherwise, default to the standard ISA MultiMaster probe.
	 */
	if (!BusLogic_ProbeOptions.NoProbePCI) {
		if (BusLogic_ProbeOptions.MultiMasterFirst) {
			BusLogic_InitializeMultiMasterProbeInfo(PrototypeHostAdapter);
			BusLogic_InitializeFlashPointProbeInfo(PrototypeHostAdapter);
		} else if (BusLogic_ProbeOptions.FlashPointFirst) {
			BusLogic_InitializeFlashPointProbeInfo(PrototypeHostAdapter);
			BusLogic_InitializeMultiMasterProbeInfo(PrototypeHostAdapter);
		} else {
			int FlashPointCount = BusLogic_InitializeFlashPointProbeInfo(PrototypeHostAdapter);
			int PCIMultiMasterCount = BusLogic_InitializeMultiMasterProbeInfo(PrototypeHostAdapter);
			if (FlashPointCount > 0 && PCIMultiMasterCount > 0) {
				struct BusLogic_ProbeInfo *ProbeInfo = &BusLogic_ProbeInfoList[FlashPointCount];
				struct BusLogic_HostAdapter *HostAdapter = PrototypeHostAdapter;
				struct BusLogic_FetchHostAdapterLocalRAMRequest FetchHostAdapterLocalRAMRequest;
				struct BusLogic_BIOSDriveMapByte Drive0MapByte;
				while (ProbeInfo->HostAdapterBusType != BusLogic_PCI_Bus)
					ProbeInfo++;
				HostAdapter->IO_Address = ProbeInfo->IO_Address;
				FetchHostAdapterLocalRAMRequest.ByteOffset = BusLogic_BIOS_BaseOffset + BusLogic_BIOS_DriveMapOffset + 0;
				FetchHostAdapterLocalRAMRequest.ByteCount = sizeof(Drive0MapByte);
				BusLogic_Command(HostAdapter, BusLogic_FetchHostAdapterLocalRAM, &FetchHostAdapterLocalRAMRequest, sizeof(FetchHostAdapterLocalRAMRequest), &Drive0MapByte, sizeof(Drive0MapByte));
				/*
				   If the Map Byte for BIOS Drive 0 indicates that BIOS Drive 0
				   is controlled by this PCI MultiMaster Host Adapter, then
				   reverse the probe order so that MultiMaster Host Adapters are
				   probed before FlashPoint Host Adapters.
				 */
				if (Drive0MapByte.DiskGeometry != BusLogic_BIOS_Disk_Not_Installed) {
					struct BusLogic_ProbeInfo SavedProbeInfo[BusLogic_MaxHostAdapters];
					int MultiMasterCount = BusLogic_ProbeInfoCount - FlashPointCount;
					memcpy(SavedProbeInfo, BusLogic_ProbeInfoList, BusLogic_ProbeInfoCount * sizeof(struct BusLogic_ProbeInfo));
					memcpy(&BusLogic_ProbeInfoList[0], &SavedProbeInfo[FlashPointCount], MultiMasterCount * sizeof(struct BusLogic_ProbeInfo));
					memcpy(&BusLogic_ProbeInfoList[MultiMasterCount], &SavedProbeInfo[0], FlashPointCount * sizeof(struct BusLogic_ProbeInfo));
				}
			}
		}
	} else
		BusLogic_InitializeProbeInfoListISA(PrototypeHostAdapter);
}


#else
#define BusLogic_InitializeProbeInfoList(adapter) \
		BusLogic_InitializeProbeInfoListISA(adapter)
#endif				/* CONFIG_PCI */


/*
  BusLogic_Failure prints a standardized error message, and then returns false.
*/

static bool BusLogic_Failure(struct BusLogic_HostAdapter *HostAdapter, char *ErrorMessage)
{
	BusLogic_AnnounceDriver(HostAdapter);
	if (HostAdapter->HostAdapterBusType == BusLogic_PCI_Bus) {
		BusLogic_Error("While configuring BusLogic PCI Host Adapter at\n", HostAdapter);
		BusLogic_Error("Bus %d Device %d I/O Address 0x%X PCI Address 0x%X:\n", HostAdapter, HostAdapter->Bus, HostAdapter->Device, HostAdapter->IO_Address, HostAdapter->PCI_Address);
	} else
		BusLogic_Error("While configuring BusLogic Host Adapter at " "I/O Address 0x%X:\n", HostAdapter, HostAdapter->IO_Address);
	BusLogic_Error("%s FAILED - DETACHING\n", HostAdapter, ErrorMessage);
	if (BusLogic_CommandFailureReason != NULL)
		BusLogic_Error("ADDITIONAL FAILURE INFO - %s\n", HostAdapter, BusLogic_CommandFailureReason);
	return false;
}


/*
  BusLogic_ProbeHostAdapter probes for a BusLogic Host Adapter.
*/

static bool __init BusLogic_ProbeHostAdapter(struct BusLogic_HostAdapter *HostAdapter)
{
	union BusLogic_StatusRegister StatusRegister;
	union BusLogic_InterruptRegister InterruptRegister;
	union BusLogic_GeometryRegister GeometryRegister;
	/*
	   FlashPoint Host Adapters are Probed by the FlashPoint SCCB Manager.
	 */
	if (BusLogic_FlashPointHostAdapterP(HostAdapter)) {
		struct FlashPoint_Info *FlashPointInfo = &HostAdapter->FlashPointInfo;
		FlashPointInfo->BaseAddress = (u32) HostAdapter->IO_Address;
		FlashPointInfo->IRQ_Channel = HostAdapter->IRQ_Channel;
		FlashPointInfo->Present = false;
		if (!(FlashPoint_ProbeHostAdapter(FlashPointInfo) == 0 && FlashPointInfo->Present)) {
			BusLogic_Error("BusLogic: FlashPoint Host Adapter detected at " "PCI Bus %d Device %d\n", HostAdapter, HostAdapter->Bus, HostAdapter->Device);
			BusLogic_Error("BusLogic: I/O Address 0x%X PCI Address 0x%X, " "but FlashPoint\n", HostAdapter, HostAdapter->IO_Address, HostAdapter->PCI_Address);
			BusLogic_Error("BusLogic: Probe Function failed to validate it.\n", HostAdapter);
			return false;
		}
		if (BusLogic_GlobalOptions.TraceProbe)
			BusLogic_Notice("BusLogic_Probe(0x%X): FlashPoint Found\n", HostAdapter, HostAdapter->IO_Address);
		/*
		   Indicate the Host Adapter Probe completed successfully.
		 */
		return true;
	}
	/*
	   Read the Status, Interrupt, and Geometry Registers to test if there are I/O
	   ports that respond, and to check the values to determine if they are from a
	   BusLogic Host Adapter.  A nonexistent I/O port will return 0xFF, in which
	   case there is definitely no BusLogic Host Adapter at this base I/O Address.
	   The test here is a subset of that used by the BusLogic Host Adapter BIOS.
	 */
	StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
	InterruptRegister.All = BusLogic_ReadInterruptRegister(HostAdapter);
	GeometryRegister.All = BusLogic_ReadGeometryRegister(HostAdapter);
	if (BusLogic_GlobalOptions.TraceProbe)
		BusLogic_Notice("BusLogic_Probe(0x%X): Status 0x%02X, Interrupt 0x%02X, " "Geometry 0x%02X\n", HostAdapter, HostAdapter->IO_Address, StatusRegister.All, InterruptRegister.All, GeometryRegister.All);
	if (StatusRegister.All == 0 || StatusRegister.sr.DiagnosticActive || StatusRegister.sr.CommandParameterRegisterBusy || StatusRegister.sr.Reserved || StatusRegister.sr.CommandInvalid || InterruptRegister.ir.Reserved != 0)
		return false;
	/*
	   Check the undocumented Geometry Register to test if there is an I/O port
	   that responded.  Adaptec Host Adapters do not implement the Geometry
	   Register, so this test helps serve to avoid incorrectly recognizing an
	   Adaptec 1542A or 1542B as a BusLogic.  Unfortunately, the Adaptec 1542C
	   series does respond to the Geometry Register I/O port, but it will be
	   rejected later when the Inquire Extended Setup Information command is
	   issued in BusLogic_CheckHostAdapter.  The AMI FastDisk Host Adapter is a
	   BusLogic clone that implements the same interface as earlier BusLogic
	   Host Adapters, including the undocumented commands, and is therefore
	   supported by this driver.  However, the AMI FastDisk always returns 0x00
	   upon reading the Geometry Register, so the extended translation option
	   should always be left disabled on the AMI FastDisk.
	 */
	if (GeometryRegister.All == 0xFF)
		return false;
	/*
	   Indicate the Host Adapter Probe completed successfully.
	 */
	return true;
}


/*
  BusLogic_HardwareResetHostAdapter issues a Hardware Reset to the Host Adapter
  and waits for Host Adapter Diagnostics to complete.  If HardReset is true, a
  Hard Reset is performed which also initiates a SCSI Bus Reset.  Otherwise, a
  Soft Reset is performed which only resets the Host Adapter without forcing a
  SCSI Bus Reset.
*/

static bool BusLogic_HardwareResetHostAdapter(struct BusLogic_HostAdapter
						 *HostAdapter, bool HardReset)
{
	union BusLogic_StatusRegister StatusRegister;
	int TimeoutCounter;
	/*
	   FlashPoint Host Adapters are Hard Reset by the FlashPoint SCCB Manager.
	 */
	if (BusLogic_FlashPointHostAdapterP(HostAdapter)) {
		struct FlashPoint_Info *FlashPointInfo = &HostAdapter->FlashPointInfo;
		FlashPointInfo->HostSoftReset = !HardReset;
		FlashPointInfo->ReportDataUnderrun = true;
		HostAdapter->CardHandle = FlashPoint_HardwareResetHostAdapter(FlashPointInfo);
		if (HostAdapter->CardHandle == FlashPoint_BadCardHandle)
			return false;
		/*
		   Indicate the Host Adapter Hard Reset completed successfully.
		 */
		return true;
	}
	/*
	   Issue a Hard Reset or Soft Reset Command to the Host Adapter.  The Host
	   Adapter should respond by setting Diagnostic Active in the Status Register.
	 */
	if (HardReset)
		BusLogic_HardReset(HostAdapter);
	else
		BusLogic_SoftReset(HostAdapter);
	/*
	   Wait until Diagnostic Active is set in the Status Register.
	 */
	TimeoutCounter = 5 * 10000;
	while (--TimeoutCounter >= 0) {
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (StatusRegister.sr.DiagnosticActive)
			break;
		udelay(100);
	}
	if (BusLogic_GlobalOptions.TraceHardwareReset)
		BusLogic_Notice("BusLogic_HardwareReset(0x%X): Diagnostic Active, " "Status 0x%02X\n", HostAdapter, HostAdapter->IO_Address, StatusRegister.All);
	if (TimeoutCounter < 0)
		return false;
	/*
	   Wait 100 microseconds to allow completion of any initial diagnostic
	   activity which might leave the contents of the Status Register
	   unpredictable.
	 */
	udelay(100);
	/*
	   Wait until Diagnostic Active is reset in the Status Register.
	 */
	TimeoutCounter = 10 * 10000;
	while (--TimeoutCounter >= 0) {
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (!StatusRegister.sr.DiagnosticActive)
			break;
		udelay(100);
	}
	if (BusLogic_GlobalOptions.TraceHardwareReset)
		BusLogic_Notice("BusLogic_HardwareReset(0x%X): Diagnostic Completed, " "Status 0x%02X\n", HostAdapter, HostAdapter->IO_Address, StatusRegister.All);
	if (TimeoutCounter < 0)
		return false;
	/*
	   Wait until at least one of the Diagnostic Failure, Host Adapter Ready,
	   or Data In Register Ready bits is set in the Status Register.
	 */
	TimeoutCounter = 10000;
	while (--TimeoutCounter >= 0) {
		StatusRegister.All = BusLogic_ReadStatusRegister(HostAdapter);
		if (StatusRegister.sr.DiagnosticFailure || StatusRegister.sr.HostAdapterReady || StatusRegister.sr.DataInRegisterReady)
			break;
		udelay(100);
	}
	if (BusLogic_GlobalOptions.TraceHardwareReset)
		BusLogic_Notice("BusLogic_HardwareReset(0x%X): Host Adapter Ready, " "Status 0x%02X\n", HostAdapter, HostAdapter->IO_Address, StatusRegister.All);
	if (TimeoutCounter < 0)
		return false;
	/*
	   If Diagnostic Failure is set or Host Adapter Ready is reset, then an
	   error occurred during the Host Adapter diagnostics.  If Data In Register
	   Ready is set, then there is an Error Code available.
	 */
	if (StatusRegister.sr.DiagnosticFailure || !StatusRegister.sr.HostAdapterReady) {
		BusLogic_CommandFailureReason = NULL;
		BusLogic_Failure(HostAdapter, "HARD RESET DIAGNOSTICS");
		BusLogic_Error("HOST ADAPTER STATUS REGISTER = %02X\n", HostAdapter, StatusRegister.All);
		if (StatusRegister.sr.DataInRegisterReady) {
			unsigned char ErrorCode = BusLogic_ReadDataInRegister(HostAdapter);
			BusLogic_Error("HOST ADAPTER ERROR CODE = %d\n", HostAdapter, ErrorCode);
		}
		return false;
	}
	/*
	   Indicate the Host Adapter Hard Reset completed successfully.
	 */
	return true;
}


/*
  BusLogic_CheckHostAdapter checks to be sure this really is a BusLogic
  Host Adapter.
*/

static bool __init BusLogic_CheckHostAdapter(struct BusLogic_HostAdapter *HostAdapter)
{
	struct BusLogic_ExtendedSetupInformation ExtendedSetupInformation;
	unsigned char RequestedReplyLength;
	bool Result = true;
	/*
	   FlashPoint Host Adapters do not require this protection.
	 */
	if (BusLogic_FlashPointHostAdapterP(HostAdapter))
		return true;
	/*
	   Issue the Inquire Extended Setup Information command.  Only genuine
	   BusLogic Host Adapters and true clones support this command.  Adaptec 1542C
	   series Host Adapters that respond to the Geometry Register I/O port will
	   fail this command.
	 */
	RequestedReplyLength = sizeof(ExtendedSetupInformation);
	if (BusLogic_Command(HostAdapter, BusLogic_InquireExtendedSetupInformation, &RequestedReplyLength, sizeof(RequestedReplyLength), &ExtendedSetupInformation, sizeof(ExtendedSetupInformation))
	    != sizeof(ExtendedSetupInformation))
		Result = false;
	/*
	   Provide tracing information if requested and return.
	 */
	if (BusLogic_GlobalOptions.TraceProbe)
		BusLogic_Notice("BusLogic_Check(0x%X): MultiMaster %s\n", HostAdapter, HostAdapter->IO_Address, (Result ? "Found" : "Not Found"));
	return Result;
}


/*
  BusLogic_ReadHostAdapterConfiguration reads the Configuration Information
  from Host Adapter and initializes the Host Adapter structure.
*/

static bool __init BusLogic_ReadHostAdapterConfiguration(struct BusLogic_HostAdapter
							    *HostAdapter)
{
	struct BusLogic_BoardID BoardID;
	struct BusLogic_Configuration Configuration;
	struct BusLogic_SetupInformation SetupInformation;
	struct BusLogic_ExtendedSetupInformation ExtendedSetupInformation;
	unsigned char HostAdapterModelNumber[5];
	unsigned char FirmwareVersion3rdDigit;
	unsigned char FirmwareVersionLetter;
	struct BusLogic_PCIHostAdapterInformation PCIHostAdapterInformation;
	struct BusLogic_FetchHostAdapterLocalRAMRequest FetchHostAdapterLocalRAMRequest;
	struct BusLogic_AutoSCSIData AutoSCSIData;
	union BusLogic_GeometryRegister GeometryRegister;
	unsigned char RequestedReplyLength;
	unsigned char *TargetPointer, Character;
	int TargetID, i;
	/*
	   Configuration Information for FlashPoint Host Adapters is provided in the
	   FlashPoint_Info structure by the FlashPoint SCCB Manager's Probe Function.
	   Initialize fields in the Host Adapter structure from the FlashPoint_Info
	   structure.
	 */
	if (BusLogic_FlashPointHostAdapterP(HostAdapter)) {
		struct FlashPoint_Info *FlashPointInfo = &HostAdapter->FlashPointInfo;
		TargetPointer = HostAdapter->ModelName;
		*TargetPointer++ = 'B';
		*TargetPointer++ = 'T';
		*TargetPointer++ = '-';
		for (i = 0; i < sizeof(FlashPointInfo->ModelNumber); i++)
			*TargetPointer++ = FlashPointInfo->ModelNumber[i];
		*TargetPointer++ = '\0';
		strcpy(HostAdapter->FirmwareVersion, FlashPoint_FirmwareVersion);
		HostAdapter->SCSI_ID = FlashPointInfo->SCSI_ID;
		HostAdapter->ExtendedTranslationEnabled = FlashPointInfo->ExtendedTranslationEnabled;
		HostAdapter->ParityCheckingEnabled = FlashPointInfo->ParityCheckingEnabled;
		HostAdapter->BusResetEnabled = !FlashPointInfo->HostSoftReset;
		HostAdapter->LevelSensitiveInterrupt = true;
		HostAdapter->HostWideSCSI = FlashPointInfo->HostWideSCSI;
		HostAdapter->HostDifferentialSCSI = false;
		HostAdapter->HostSupportsSCAM = true;
		HostAdapter->HostUltraSCSI = true;
		HostAdapter->ExtendedLUNSupport = true;
		HostAdapter->TerminationInfoValid = true;
		HostAdapter->LowByteTerminated = FlashPointInfo->LowByteTerminated;
		HostAdapter->HighByteTerminated = FlashPointInfo->HighByteTerminated;
		HostAdapter->SCAM_Enabled = FlashPointInfo->SCAM_Enabled;
		HostAdapter->SCAM_Level2 = FlashPointInfo->SCAM_Level2;
		HostAdapter->DriverScatterGatherLimit = BusLogic_ScatterGatherLimit;
		HostAdapter->MaxTargetDevices = (HostAdapter->HostWideSCSI ? 16 : 8);
		HostAdapter->MaxLogicalUnits = 32;
		HostAdapter->InitialCCBs = 4 * BusLogic_CCB_AllocationGroupSize;
		HostAdapter->IncrementalCCBs = BusLogic_CCB_AllocationGroupSize;
		HostAdapter->DriverQueueDepth = 255;
		HostAdapter->HostAdapterQueueDepth = HostAdapter->DriverQueueDepth;
		HostAdapter->SynchronousPermitted = FlashPointInfo->SynchronousPermitted;
		HostAdapter->FastPermitted = FlashPointInfo->FastPermitted;
		HostAdapter->UltraPermitted = FlashPointInfo->UltraPermitted;
		HostAdapter->WidePermitted = FlashPointInfo->WidePermitted;
		HostAdapter->DisconnectPermitted = FlashPointInfo->DisconnectPermitted;
		HostAdapter->TaggedQueuingPermitted = 0xFFFF;
		goto Common;
	}
	/*
	   Issue the Inquire Board ID command.
	 */
	if (BusLogic_Command(HostAdapter, BusLogic_InquireBoardID, NULL, 0, &BoardID, sizeof(BoardID)) != sizeof(BoardID))
		return BusLogic_Failure(HostAdapter, "INQUIRE BOARD ID");
	/*
	   Issue the Inquire Configuration command.
	 */
	if (BusLogic_Command(HostAdapter, BusLogic_InquireConfiguration, NULL, 0, &Configuration, sizeof(Configuration))
	    != sizeof(Configuration))
		return BusLogic_Failure(HostAdapter, "INQUIRE CONFIGURATION");
	/*
	   Issue the Inquire Setup Information command.
	 */
	RequestedReplyLength = sizeof(SetupInformation);
	if (BusLogic_Command(HostAdapter, BusLogic_InquireSetupInformation, &RequestedReplyLength, sizeof(RequestedReplyLength), &SetupInformation, sizeof(SetupInformation))
	    != sizeof(SetupInformation))
		return BusLogic_Failure(HostAdapter, "INQUIRE SETUP INFORMATION");
	/*
	   Issue the Inquire Extended Setup Information command.
	 */
	RequestedReplyLength = sizeof(ExtendedSetupInformation);
	if (BusLogic_Command(HostAdapter, BusLogic_InquireExtendedSetupInformation, &RequestedReplyLength, sizeof(RequestedReplyLength), &ExtendedSetupInformation, sizeof(ExtendedSetupInformation))
	    != sizeof(ExtendedSetupInformation))
		return BusLogic_Failure(HostAdapter, "INQUIRE EXTENDED SETUP INFORMATION");
	/*
	   Issue the Inquire Firmware Version 3rd Digit command.
	 */
	FirmwareVersion3rdDigit = '\0';
	if (BoardID.FirmwareVersion1stDigit > '0')
		if (BusLogic_Command(HostAdapter, BusLogic_InquireFirmwareVersion3rdDigit, NULL, 0, &FirmwareVersion3rdDigit, sizeof(FirmwareVersion3rdDigit))
		    != sizeof(FirmwareVersion3rdDigit))
			return BusLogic_Failure(HostAdapter, "INQUIRE FIRMWARE 3RD DIGIT");
	/*
	   Issue the Inquire Host Adapter Model Number command.
	 */
	if (ExtendedSetupInformation.BusType == 'A' && BoardID.FirmwareVersion1stDigit == '2')
		/* BusLogic BT-542B ISA 2.xx */
		strcpy(HostAdapterModelNumber, "542B");
	else if (ExtendedSetupInformation.BusType == 'E' && BoardID.FirmwareVersion1stDigit == '2' && (BoardID.FirmwareVersion2ndDigit <= '1' || (BoardID.FirmwareVersion2ndDigit == '2' && FirmwareVersion3rdDigit == '0')))
		/* BusLogic BT-742A EISA 2.1x or 2.20 */
		strcpy(HostAdapterModelNumber, "742A");
	else if (ExtendedSetupInformation.BusType == 'E' && BoardID.FirmwareVersion1stDigit == '0')
		/* AMI FastDisk EISA Series 441 0.x */
		strcpy(HostAdapterModelNumber, "747A");
	else {
		RequestedReplyLength = sizeof(HostAdapterModelNumber);
		if (BusLogic_Command(HostAdapter, BusLogic_InquireHostAdapterModelNumber, &RequestedReplyLength, sizeof(RequestedReplyLength), &HostAdapterModelNumber, sizeof(HostAdapterModelNumber))
		    != sizeof(HostAdapterModelNumber))
			return BusLogic_Failure(HostAdapter, "INQUIRE HOST ADAPTER MODEL NUMBER");
	}
	/*
	   BusLogic MultiMaster Host Adapters can be identified by their model number
	   and the major version number of their firmware as follows:

	   5.xx       BusLogic "W" Series Host Adapters:
	   BT-948/958/958D
	   4.xx       BusLogic "C" Series Host Adapters:
	   BT-946C/956C/956CD/747C/757C/757CD/445C/545C/540CF
	   3.xx       BusLogic "S" Series Host Adapters:
	   BT-747S/747D/757S/757D/445S/545S/542D
	   BT-542B/742A (revision H)
	   2.xx       BusLogic "A" Series Host Adapters:
	   BT-542B/742A (revision G and below)
	   0.xx       AMI FastDisk VLB/EISA BusLogic Clone Host Adapter
	 */
	/*
	   Save the Model Name and Host Adapter Name in the Host Adapter structure.
	 */
	TargetPointer = HostAdapter->ModelName;
	*TargetPointer++ = 'B';
	*TargetPointer++ = 'T';
	*TargetPointer++ = '-';
	for (i = 0; i < sizeof(HostAdapterModelNumber); i++) {
		Character = HostAdapterModelNumber[i];
		if (Character == ' ' || Character == '\0')
			break;
		*TargetPointer++ = Character;
	}
	*TargetPointer++ = '\0';
	/*
	   Save the Firmware Version in the Host Adapter structure.
	 */
	TargetPointer = HostAdapter->FirmwareVersion;
	*TargetPointer++ = BoardID.FirmwareVersion1stDigit;
	*TargetPointer++ = '.';
	*TargetPointer++ = BoardID.FirmwareVersion2ndDigit;
	if (FirmwareVersion3rdDigit != ' ' && FirmwareVersion3rdDigit != '\0')
		*TargetPointer++ = FirmwareVersion3rdDigit;
	*TargetPointer = '\0';
	/*
	   Issue the Inquire Firmware Version Letter command.
	 */
	if (strcmp(HostAdapter->FirmwareVersion, "3.3") >= 0) {
		if (BusLogic_Command(HostAdapter, BusLogic_InquireFirmwareVersionLetter, NULL, 0, &FirmwareVersionLetter, sizeof(FirmwareVersionLetter))
		    != sizeof(FirmwareVersionLetter))
			return BusLogic_Failure(HostAdapter, "INQUIRE FIRMWARE VERSION LETTER");
		if (FirmwareVersionLetter != ' ' && FirmwareVersionLetter != '\0')
			*TargetPointer++ = FirmwareVersionLetter;
		*TargetPointer = '\0';
	}
	/*
	   Save the Host Adapter SCSI ID in the Host Adapter structure.
	 */
	HostAdapter->SCSI_ID = Configuration.HostAdapterID;
	/*
	   Determine the Bus Type and save it in the Host Adapter structure, determine
	   and save the IRQ Channel if necessary, and determine and save the DMA
	   Channel for ISA Host Adapters.
	 */
	HostAdapter->HostAdapterBusType = BusLogic_HostAdapterBusTypes[HostAdapter->ModelName[3] - '4'];
	if (HostAdapter->IRQ_Channel == 0) {
		if (Configuration.IRQ_Channel9)
			HostAdapter->IRQ_Channel = 9;
		else if (Configuration.IRQ_Channel10)
			HostAdapter->IRQ_Channel = 10;
		else if (Configuration.IRQ_Channel11)
			HostAdapter->IRQ_Channel = 11;
		else if (Configuration.IRQ_Channel12)
			HostAdapter->IRQ_Channel = 12;
		else if (Configuration.IRQ_Channel14)
			HostAdapter->IRQ_Channel = 14;
		else if (Configuration.IRQ_Channel15)
			HostAdapter->IRQ_Channel = 15;
	}
	if (HostAdapter->HostAdapterBusType == BusLogic_ISA_Bus) {
		if (Configuration.DMA_Channel5)
			HostAdapter->DMA_Channel = 5;
		else if (Configuration.DMA_Channel6)
			HostAdapter->DMA_Channel = 6;
		else if (Configuration.DMA_Channel7)
			HostAdapter->DMA_Channel = 7;
	}
	/*
	   Determine whether Extended Translation is enabled and save it in
	   the Host Adapter structure.
	 */
	GeometryRegister.All = BusLogic_ReadGeometryRegister(HostAdapter);
	HostAdapter->ExtendedTranslationEnabled = GeometryRegister.gr.ExtendedTranslationEnabled;
	/*
	   Save the Scatter Gather Limits, Level Sensitive Interrupt flag, Wide
	   SCSI flag, Differential SCSI flag, SCAM Supported flag, and
	   Ultra SCSI flag in the Host Adapter structure.
	 */
	HostAdapter->HostAdapterScatterGatherLimit = ExtendedSetupInformation.ScatterGatherLimit;
	HostAdapter->DriverScatterGatherLimit = HostAdapter->HostAdapterScatterGatherLimit;
	if (HostAdapter->HostAdapterScatterGatherLimit > BusLogic_ScatterGatherLimit)
		HostAdapter->DriverScatterGatherLimit = BusLogic_ScatterGatherLimit;
	if (ExtendedSetupInformation.Misc.LevelSensitiveInterrupt)
		HostAdapter->LevelSensitiveInterrupt = true;
	HostAdapter->HostWideSCSI = ExtendedSetupInformation.HostWideSCSI;
	HostAdapter->HostDifferentialSCSI = ExtendedSetupInformation.HostDifferentialSCSI;
	HostAdapter->HostSupportsSCAM = ExtendedSetupInformation.HostSupportsSCAM;
	HostAdapter->HostUltraSCSI = ExtendedSetupInformation.HostUltraSCSI;
	/*
	   Determine whether Extended LUN Format CCBs are supported and save the
	   information in the Host Adapter structure.
	 */
	if (HostAdapter->FirmwareVersion[0] == '5' || (HostAdapter->FirmwareVersion[0] == '4' && HostAdapter->HostWideSCSI))
		HostAdapter->ExtendedLUNSupport = true;
	/*
	   Issue the Inquire PCI Host Adapter Information command to read the
	   Termination Information from "W" series MultiMaster Host Adapters.
	 */
	if (HostAdapter->FirmwareVersion[0] == '5') {
		if (BusLogic_Command(HostAdapter, BusLogic_InquirePCIHostAdapterInformation, NULL, 0, &PCIHostAdapterInformation, sizeof(PCIHostAdapterInformation))
		    != sizeof(PCIHostAdapterInformation))
			return BusLogic_Failure(HostAdapter, "INQUIRE PCI HOST ADAPTER INFORMATION");
		/*
		   Save the Termination Information in the Host Adapter structure.
		 */
		if (PCIHostAdapterInformation.GenericInfoValid) {
			HostAdapter->TerminationInfoValid = true;
			HostAdapter->LowByteTerminated = PCIHostAdapterInformation.LowByteTerminated;
			HostAdapter->HighByteTerminated = PCIHostAdapterInformation.HighByteTerminated;
		}
	}
	/*
	   Issue the Fetch Host Adapter Local RAM command to read the AutoSCSI data
	   from "W" and "C" series MultiMaster Host Adapters.
	 */
	if (HostAdapter->FirmwareVersion[0] >= '4') {
		FetchHostAdapterLocalRAMRequest.ByteOffset = BusLogic_AutoSCSI_BaseOffset;
		FetchHostAdapterLocalRAMRequest.ByteCount = sizeof(AutoSCSIData);
		if (BusLogic_Command(HostAdapter, BusLogic_FetchHostAdapterLocalRAM, &FetchHostAdapterLocalRAMRequest, sizeof(FetchHostAdapterLocalRAMRequest), &AutoSCSIData, sizeof(AutoSCSIData))
		    != sizeof(AutoSCSIData))
			return BusLogic_Failure(HostAdapter, "FETCH HOST ADAPTER LOCAL RAM");
		/*
		   Save the Parity Checking Enabled, Bus Reset Enabled, and Termination
		   Information in the Host Adapter structure.
		 */
		HostAdapter->ParityCheckingEnabled = AutoSCSIData.ParityCheckingEnabled;
		HostAdapter->BusResetEnabled = AutoSCSIData.BusResetEnabled;
		if (HostAdapter->FirmwareVersion[0] == '4') {
			HostAdapter->TerminationInfoValid = true;
			HostAdapter->LowByteTerminated = AutoSCSIData.LowByteTerminated;
			HostAdapter->HighByteTerminated = AutoSCSIData.HighByteTerminated;
		}
		/*
		   Save the Wide Permitted, Fast Permitted, Synchronous Permitted,
		   Disconnect Permitted, Ultra Permitted, and SCAM Information in the
		   Host Adapter structure.
		 */
		HostAdapter->WidePermitted = AutoSCSIData.WidePermitted;
		HostAdapter->FastPermitted = AutoSCSIData.FastPermitted;
		HostAdapter->SynchronousPermitted = AutoSCSIData.SynchronousPermitted;
		HostAdapter->DisconnectPermitted = AutoSCSIData.DisconnectPermitted;
		if (HostAdapter->HostUltraSCSI)
			HostAdapter->UltraPermitted = AutoSCSIData.UltraPermitted;
		if (HostAdapter->HostSupportsSCAM) {
			HostAdapter->SCAM_Enabled = AutoSCSIData.SCAM_Enabled;
			HostAdapter->SCAM_Level2 = AutoSCSIData.SCAM_Level2;
		}
	}
	/*
	   Initialize fields in the Host Adapter structure for "S" and "A" series
	   MultiMaster Host Adapters.
	 */
	if (HostAdapter->FirmwareVersion[0] < '4') {
		if (SetupInformation.SynchronousInitiationEnabled) {
			HostAdapter->SynchronousPermitted = 0xFF;
			if (HostAdapter->HostAdapterBusType == BusLogic_EISA_Bus) {
				if (ExtendedSetupInformation.Misc.FastOnEISA)
					HostAdapter->FastPermitted = 0xFF;
				if (strcmp(HostAdapter->ModelName, "BT-757") == 0)
					HostAdapter->WidePermitted = 0xFF;
			}
		}
		HostAdapter->DisconnectPermitted = 0xFF;
		HostAdapter->ParityCheckingEnabled = SetupInformation.ParityCheckingEnabled;
		HostAdapter->BusResetEnabled = true;
	}
	/*
	   Determine the maximum number of Target IDs and Logical Units supported by
	   this driver for Wide and Narrow Host Adapters.
	 */
	HostAdapter->MaxTargetDevices = (HostAdapter->HostWideSCSI ? 16 : 8);
	HostAdapter->MaxLogicalUnits = (HostAdapter->ExtendedLUNSupport ? 32 : 8);
	/*
	   Select appropriate values for the Mailbox Count, Driver Queue Depth,
	   Initial CCBs, and Incremental CCBs variables based on whether or not Strict
	   Round Robin Mode is supported.  If Strict Round Robin Mode is supported,
	   then there is no performance degradation in using the maximum possible
	   number of Outgoing and Incoming Mailboxes and allowing the Tagged and
	   Untagged Queue Depths to determine the actual utilization.  If Strict Round
	   Robin Mode is not supported, then the Host Adapter must scan all the
	   Outgoing Mailboxes whenever an Outgoing Mailbox entry is made, which can
	   cause a substantial performance penalty.  The host adapters actually have
	   room to store the following number of CCBs internally; that is, they can
	   internally queue and manage this many active commands on the SCSI bus
	   simultaneously.  Performance measurements demonstrate that the Driver Queue
	   Depth should be set to the Mailbox Count, rather than the Host Adapter
	   Queue Depth (internal CCB capacity), as it is more efficient to have the
	   queued commands waiting in Outgoing Mailboxes if necessary than to block
	   the process in the higher levels of the SCSI Subsystem.

	   192          BT-948/958/958D
	   100          BT-946C/956C/956CD/747C/757C/757CD/445C
	   50   BT-545C/540CF
	   30   BT-747S/747D/757S/757D/445S/545S/542D/542B/742A
	 */
	if (HostAdapter->FirmwareVersion[0] == '5')
		HostAdapter->HostAdapterQueueDepth = 192;
	else if (HostAdapter->FirmwareVersion[0] == '4')
		HostAdapter->HostAdapterQueueDepth = (HostAdapter->HostAdapterBusType != BusLogic_ISA_Bus ? 100 : 50);
	else
		HostAdapter->HostAdapterQueueDepth = 30;
	if (strcmp(HostAdapter->FirmwareVersion, "3.31") >= 0) {
		HostAdapter->StrictRoundRobinModeSupport = true;
		HostAdapter->MailboxCount = BusLogic_MaxMailboxes;
	} else {
		HostAdapter->StrictRoundRobinModeSupport = false;
		HostAdapter->MailboxCount = 32;
	}
	HostAdapter->DriverQueueDepth = HostAdapter->MailboxCount;
	HostAdapter->InitialCCBs = 4 * BusLogic_CCB_AllocationGroupSize;
	HostAdapter->IncrementalCCBs = BusLogic_CCB_AllocationGroupSize;
	/*
	   Tagged Queuing support is available and operates properly on all "W" series
	   MultiMaster Host Adapters, on "C" series MultiMaster Host Adapters with
	   firmware version 4.22 and above, and on "S" series MultiMaster Host
	   Adapters with firmware version 3.35 and above.
	 */
	HostAdapter->TaggedQueuingPermitted = 0;
	switch (HostAdapter->FirmwareVersion[0]) {
	case '5':
		HostAdapter->TaggedQueuingPermitted = 0xFFFF;
		break;
	case '4':
		if (strcmp(HostAdapter->FirmwareVersion, "4.22") >= 0)
			HostAdapter->TaggedQueuingPermitted = 0xFFFF;
		break;
	case '3':
		if (strcmp(HostAdapter->FirmwareVersion, "3.35") >= 0)
			HostAdapter->TaggedQueuingPermitted = 0xFFFF;
		break;
	}
	/*
	   Determine the Host Adapter BIOS Address if the BIOS is enabled and
	   save it in the Host Adapter structure.  The BIOS is disabled if the
	   BIOS_Address is 0.
	 */
	HostAdapter->BIOS_Address = ExtendedSetupInformation.BIOS_Address << 12;
	/*
	   ISA Host Adapters require Bounce Buffers if there is more than 16MB memory.
	 */
	if (HostAdapter->HostAdapterBusType == BusLogic_ISA_Bus && (void *) high_memory > (void *) MAX_DMA_ADDRESS)
		HostAdapter->BounceBuffersRequired = true;
	/*
	   BusLogic BT-445S Host Adapters prior to board revision E have a hardware
	   bug whereby when the BIOS is enabled, transfers to/from the same address
	   range the BIOS occupies modulo 16MB are handled incorrectly.  Only properly
	   functioning BT-445S Host Adapters have firmware version 3.37, so require
	   that ISA Bounce Buffers be used for the buggy BT-445S models if there is
	   more than 16MB memory.
	 */
	if (HostAdapter->BIOS_Address > 0 && strcmp(HostAdapter->ModelName, "BT-445S") == 0 && strcmp(HostAdapter->FirmwareVersion, "3.37") < 0 && (void *) high_memory > (void *) MAX_DMA_ADDRESS)
		HostAdapter->BounceBuffersRequired = true;
	/*
	   Initialize parameters common to MultiMaster and FlashPoint Host Adapters.
	 */
      Common:
	/*
	   Initialize the Host Adapter Full Model Name from the Model Name.
	 */
	strcpy(HostAdapter->FullModelName, "BusLogic ");
	strcat(HostAdapter->FullModelName, HostAdapter->ModelName);
	/*
	   Select an appropriate value for the Tagged Queue Depth either from a
	   BusLogic Driver Options specification, or based on whether this Host
	   Adapter requires that ISA Bounce Buffers be used.  The Tagged Queue Depth
	   is left at 0 for automatic determination in BusLogic_SelectQueueDepths.
	   Initialize the Untagged Queue Depth.
	 */
	for (TargetID = 0; TargetID < BusLogic_MaxTargetDevices; TargetID++) {
		unsigned char QueueDepth = 0;
		if (HostAdapter->DriverOptions != NULL && HostAdapter->DriverOptions->QueueDepth[TargetID] > 0)
			QueueDepth = HostAdapter->DriverOptions->QueueDepth[TargetID];
		else if (HostAdapter->BounceBuffersRequired)
			QueueDepth = BusLogic_TaggedQueueDepthBB;
		HostAdapter->QueueDepth[TargetID] = QueueDepth;
	}
	if (HostAdapter->BounceBuffersRequired)
		HostAdapter->UntaggedQueueDepth = BusLogic_UntaggedQueueDepthBB;
	else
		HostAdapter->UntaggedQueueDepth = BusLogic_UntaggedQueueDepth;
	if (HostAdapter->DriverOptions != NULL)
		HostAdapter->CommonQueueDepth = HostAdapter->DriverOptions->CommonQueueDepth;
	if (HostAdapter->CommonQueueDepth > 0 && HostAdapter->CommonQueueDepth < HostAdapter->UntaggedQueueDepth)
		HostAdapter->UntaggedQueueDepth = HostAdapter->CommonQueueDepth;
	/*
	   Tagged Queuing is only allowed if Disconnect/Reconnect is permitted.
	   Therefore, mask the Tagged Queuing Permitted Default bits with the
	   Disconnect/Reconnect Permitted bits.
	 */
	HostAdapter->TaggedQueuingPermitted &= HostAdapter->DisconnectPermitted;
	/*
	   Combine the default Tagged Queuing Permitted bits with any BusLogic Driver
	   Options Tagged Queuing specification.
	 */
	if (HostAdapter->DriverOptions != NULL)
		HostAdapter->TaggedQueuingPermitted =
		    (HostAdapter->DriverOptions->TaggedQueuingPermitted & HostAdapter->DriverOptions->TaggedQueuingPermittedMask) | (HostAdapter->TaggedQueuingPermitted & ~HostAdapter->DriverOptions->TaggedQueuingPermittedMask);

	/*
	   Select an appropriate value for Bus Settle Time either from a BusLogic
	   Driver Options specification, or from BusLogic_DefaultBusSettleTime.
	 */
	if (HostAdapter->DriverOptions != NULL && HostAdapter->DriverOptions->BusSettleTime > 0)
		HostAdapter->BusSettleTime = HostAdapter->DriverOptions->BusSettleTime;
	else
		HostAdapter->BusSettleTime = BusLogic_DefaultBusSettleTime;
	/*
	   Indicate reading the Host Adapter Configuration completed successfully.
	 */
	return true;
}


/*
  BusLogic_ReportHostAdapterConfiguration reports the configuration of
  Host Adapter.
*/

static bool __init BusLogic_ReportHostAdapterConfiguration(struct BusLogic_HostAdapter
							      *HostAdapter)
{
	unsigned short AllTargetsMask = (1 << HostAdapter->MaxTargetDevices) - 1;
	unsigned short SynchronousPermitted, FastPermitted;
	unsigned short UltraPermitted, WidePermitted;
	unsigned short DisconnectPermitted, TaggedQueuingPermitted;
	bool CommonSynchronousNegotiation, CommonTaggedQueueDepth;
	char SynchronousString[BusLogic_MaxTargetDevices + 1];
	char WideString[BusLogic_MaxTargetDevices + 1];
	char DisconnectString[BusLogic_MaxTargetDevices + 1];
	char TaggedQueuingString[BusLogic_MaxTargetDevices + 1];
	char *SynchronousMessage = SynchronousString;
	char *WideMessage = WideString;
	char *DisconnectMessage = DisconnectString;
	char *TaggedQueuingMessage = TaggedQueuingString;
	int TargetID;
	BusLogic_Info("Configuring BusLogic Model %s %s%s%s%s SCSI Host Adapter\n",
		      HostAdapter, HostAdapter->ModelName,
		      BusLogic_HostAdapterBusNames[HostAdapter->HostAdapterBusType], (HostAdapter->HostWideSCSI ? " Wide" : ""), (HostAdapter->HostDifferentialSCSI ? " Differential" : ""), (HostAdapter->HostUltraSCSI ? " Ultra" : ""));
	BusLogic_Info("  Firmware Version: %s, I/O Address: 0x%X, " "IRQ Channel: %d/%s\n", HostAdapter, HostAdapter->FirmwareVersion, HostAdapter->IO_Address, HostAdapter->IRQ_Channel, (HostAdapter->LevelSensitiveInterrupt ? "Level" : "Edge"));
	if (HostAdapter->HostAdapterBusType != BusLogic_PCI_Bus) {
		BusLogic_Info("  DMA Channel: ", HostAdapter);
		if (HostAdapter->DMA_Channel > 0)
			BusLogic_Info("%d, ", HostAdapter, HostAdapter->DMA_Channel);
		else
			BusLogic_Info("None, ", HostAdapter);
		if (HostAdapter->BIOS_Address > 0)
			BusLogic_Info("BIOS Address: 0x%X, ", HostAdapter, HostAdapter->BIOS_Address);
		else
			BusLogic_Info("BIOS Address: None, ", HostAdapter);
	} else {
		BusLogic_Info("  PCI Bus: %d, Device: %d, Address: ", HostAdapter, HostAdapter->Bus, HostAdapter->Device);
		if (HostAdapter->PCI_Address > 0)
			BusLogic_Info("0x%X, ", HostAdapter, HostAdapter->PCI_Address);
		else
			BusLogic_Info("Unassigned, ", HostAdapter);
	}
	BusLogic_Info("Host Adapter SCSI ID: %d\n", HostAdapter, HostAdapter->SCSI_ID);
	BusLogic_Info("  Parity Checking: %s, Extended Translation: %s\n", HostAdapter, (HostAdapter->ParityCheckingEnabled ? "Enabled" : "Disabled"), (HostAdapter->ExtendedTranslationEnabled ? "Enabled" : "Disabled"));
	AllTargetsMask &= ~(1 << HostAdapter->SCSI_ID);
	SynchronousPermitted = HostAdapter->SynchronousPermitted & AllTargetsMask;
	FastPermitted = HostAdapter->FastPermitted & AllTargetsMask;
	UltraPermitted = HostAdapter->UltraPermitted & AllTargetsMask;
	if ((BusLogic_MultiMasterHostAdapterP(HostAdapter) && (HostAdapter->FirmwareVersion[0] >= '4' || HostAdapter->HostAdapterBusType == BusLogic_EISA_Bus)) || BusLogic_FlashPointHostAdapterP(HostAdapter)) {
		CommonSynchronousNegotiation = false;
		if (SynchronousPermitted == 0) {
			SynchronousMessage = "Disabled";
			CommonSynchronousNegotiation = true;
		} else if (SynchronousPermitted == AllTargetsMask) {
			if (FastPermitted == 0) {
				SynchronousMessage = "Slow";
				CommonSynchronousNegotiation = true;
			} else if (FastPermitted == AllTargetsMask) {
				if (UltraPermitted == 0) {
					SynchronousMessage = "Fast";
					CommonSynchronousNegotiation = true;
				} else if (UltraPermitted == AllTargetsMask) {
					SynchronousMessage = "Ultra";
					CommonSynchronousNegotiation = true;
				}
			}
		}
		if (!CommonSynchronousNegotiation) {
			for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
				SynchronousString[TargetID] = ((!(SynchronousPermitted & (1 << TargetID))) ? 'N' : (!(FastPermitted & (1 << TargetID)) ? 'S' : (!(UltraPermitted & (1 << TargetID)) ? 'F' : 'U')));
			SynchronousString[HostAdapter->SCSI_ID] = '#';
			SynchronousString[HostAdapter->MaxTargetDevices] = '\0';
		}
	} else
		SynchronousMessage = (SynchronousPermitted == 0 ? "Disabled" : "Enabled");
	WidePermitted = HostAdapter->WidePermitted & AllTargetsMask;
	if (WidePermitted == 0)
		WideMessage = "Disabled";
	else if (WidePermitted == AllTargetsMask)
		WideMessage = "Enabled";
	else {
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			WideString[TargetID] = ((WidePermitted & (1 << TargetID)) ? 'Y' : 'N');
		WideString[HostAdapter->SCSI_ID] = '#';
		WideString[HostAdapter->MaxTargetDevices] = '\0';
	}
	DisconnectPermitted = HostAdapter->DisconnectPermitted & AllTargetsMask;
	if (DisconnectPermitted == 0)
		DisconnectMessage = "Disabled";
	else if (DisconnectPermitted == AllTargetsMask)
		DisconnectMessage = "Enabled";
	else {
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			DisconnectString[TargetID] = ((DisconnectPermitted & (1 << TargetID)) ? 'Y' : 'N');
		DisconnectString[HostAdapter->SCSI_ID] = '#';
		DisconnectString[HostAdapter->MaxTargetDevices] = '\0';
	}
	TaggedQueuingPermitted = HostAdapter->TaggedQueuingPermitted & AllTargetsMask;
	if (TaggedQueuingPermitted == 0)
		TaggedQueuingMessage = "Disabled";
	else if (TaggedQueuingPermitted == AllTargetsMask)
		TaggedQueuingMessage = "Enabled";
	else {
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			TaggedQueuingString[TargetID] = ((TaggedQueuingPermitted & (1 << TargetID)) ? 'Y' : 'N');
		TaggedQueuingString[HostAdapter->SCSI_ID] = '#';
		TaggedQueuingString[HostAdapter->MaxTargetDevices] = '\0';
	}
	BusLogic_Info("  Synchronous Negotiation: %s, Wide Negotiation: %s\n", HostAdapter, SynchronousMessage, WideMessage);
	BusLogic_Info("  Disconnect/Reconnect: %s, Tagged Queuing: %s\n", HostAdapter, DisconnectMessage, TaggedQueuingMessage);
	if (BusLogic_MultiMasterHostAdapterP(HostAdapter)) {
		BusLogic_Info("  Scatter/Gather Limit: %d of %d segments, " "Mailboxes: %d\n", HostAdapter, HostAdapter->DriverScatterGatherLimit, HostAdapter->HostAdapterScatterGatherLimit, HostAdapter->MailboxCount);
		BusLogic_Info("  Driver Queue Depth: %d, " "Host Adapter Queue Depth: %d\n", HostAdapter, HostAdapter->DriverQueueDepth, HostAdapter->HostAdapterQueueDepth);
	} else
		BusLogic_Info("  Driver Queue Depth: %d, " "Scatter/Gather Limit: %d segments\n", HostAdapter, HostAdapter->DriverQueueDepth, HostAdapter->DriverScatterGatherLimit);
	BusLogic_Info("  Tagged Queue Depth: ", HostAdapter);
	CommonTaggedQueueDepth = true;
	for (TargetID = 1; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
		if (HostAdapter->QueueDepth[TargetID] != HostAdapter->QueueDepth[0]) {
			CommonTaggedQueueDepth = false;
			break;
		}
	if (CommonTaggedQueueDepth) {
		if (HostAdapter->QueueDepth[0] > 0)
			BusLogic_Info("%d", HostAdapter, HostAdapter->QueueDepth[0]);
		else
			BusLogic_Info("Automatic", HostAdapter);
	} else
		BusLogic_Info("Individual", HostAdapter);
	BusLogic_Info(", Untagged Queue Depth: %d\n", HostAdapter, HostAdapter->UntaggedQueueDepth);
	if (HostAdapter->TerminationInfoValid) {
		if (HostAdapter->HostWideSCSI)
			BusLogic_Info("  SCSI Bus Termination: %s", HostAdapter, (HostAdapter->LowByteTerminated ? (HostAdapter->HighByteTerminated ? "Both Enabled" : "Low Enabled")
										  : (HostAdapter->HighByteTerminated ? "High Enabled" : "Both Disabled")));
		else
			BusLogic_Info("  SCSI Bus Termination: %s", HostAdapter, (HostAdapter->LowByteTerminated ? "Enabled" : "Disabled"));
		if (HostAdapter->HostSupportsSCAM)
			BusLogic_Info(", SCAM: %s", HostAdapter, (HostAdapter->SCAM_Enabled ? (HostAdapter->SCAM_Level2 ? "Enabled, Level 2" : "Enabled, Level 1")
								  : "Disabled"));
		BusLogic_Info("\n", HostAdapter);
	}
	/*
	   Indicate reporting the Host Adapter configuration completed successfully.
	 */
	return true;
}


/*
  BusLogic_AcquireResources acquires the system resources necessary to use
  Host Adapter.
*/

static bool __init BusLogic_AcquireResources(struct BusLogic_HostAdapter *HostAdapter)
{
	if (HostAdapter->IRQ_Channel == 0) {
		BusLogic_Error("NO LEGAL INTERRUPT CHANNEL ASSIGNED - DETACHING\n", HostAdapter);
		return false;
	}
	/*
	   Acquire shared access to the IRQ Channel.
	 */
	if (request_irq(HostAdapter->IRQ_Channel, BusLogic_InterruptHandler, IRQF_SHARED, HostAdapter->FullModelName, HostAdapter) < 0) {
		BusLogic_Error("UNABLE TO ACQUIRE IRQ CHANNEL %d - DETACHING\n", HostAdapter, HostAdapter->IRQ_Channel);
		return false;
	}
	HostAdapter->IRQ_ChannelAcquired = true;
	/*
	   Acquire exclusive access to the DMA Channel.
	 */
	if (HostAdapter->DMA_Channel > 0) {
		if (request_dma(HostAdapter->DMA_Channel, HostAdapter->FullModelName) < 0) {
			BusLogic_Error("UNABLE TO ACQUIRE DMA CHANNEL %d - DETACHING\n", HostAdapter, HostAdapter->DMA_Channel);
			return false;
		}
		set_dma_mode(HostAdapter->DMA_Channel, DMA_MODE_CASCADE);
		enable_dma(HostAdapter->DMA_Channel);
		HostAdapter->DMA_ChannelAcquired = true;
	}
	/*
	   Indicate the System Resource Acquisition completed successfully,
	 */
	return true;
}


/*
  BusLogic_ReleaseResources releases any system resources previously acquired
  by BusLogic_AcquireResources.
*/

static void BusLogic_ReleaseResources(struct BusLogic_HostAdapter *HostAdapter)
{
	/*
	   Release shared access to the IRQ Channel.
	 */
	if (HostAdapter->IRQ_ChannelAcquired)
		free_irq(HostAdapter->IRQ_Channel, HostAdapter);
	/*
	   Release exclusive access to the DMA Channel.
	 */
	if (HostAdapter->DMA_ChannelAcquired)
		free_dma(HostAdapter->DMA_Channel);
	/*
	   Release any allocated memory structs not released elsewhere
	 */
	if (HostAdapter->MailboxSpace)
		pci_free_consistent(HostAdapter->PCI_Device, HostAdapter->MailboxSize, HostAdapter->MailboxSpace, HostAdapter->MailboxSpaceHandle);
	pci_dev_put(HostAdapter->PCI_Device);
	HostAdapter->MailboxSpace = NULL;
	HostAdapter->MailboxSpaceHandle = 0;
	HostAdapter->MailboxSize = 0;
}


/*
  BusLogic_InitializeHostAdapter initializes Host Adapter.  This is the only
  function called during SCSI Host Adapter detection which modifies the state
  of the Host Adapter from its initial power on or hard reset state.
*/

static bool BusLogic_InitializeHostAdapter(struct BusLogic_HostAdapter
					      *HostAdapter)
{
	struct BusLogic_ExtendedMailboxRequest ExtendedMailboxRequest;
	enum BusLogic_RoundRobinModeRequest RoundRobinModeRequest;
	enum BusLogic_SetCCBFormatRequest SetCCBFormatRequest;
	int TargetID;
	/*
	   Initialize the pointers to the first and last CCBs that are queued for
	   completion processing.
	 */
	HostAdapter->FirstCompletedCCB = NULL;
	HostAdapter->LastCompletedCCB = NULL;
	/*
	   Initialize the Bus Device Reset Pending CCB, Tagged Queuing Active,
	   Command Successful Flag, Active Commands, and Commands Since Reset
	   for each Target Device.
	 */
	for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++) {
		HostAdapter->BusDeviceResetPendingCCB[TargetID] = NULL;
		HostAdapter->TargetFlags[TargetID].TaggedQueuingActive = false;
		HostAdapter->TargetFlags[TargetID].CommandSuccessfulFlag = false;
		HostAdapter->ActiveCommands[TargetID] = 0;
		HostAdapter->CommandsSinceReset[TargetID] = 0;
	}
	/*
	   FlashPoint Host Adapters do not use Outgoing and Incoming Mailboxes.
	 */
	if (BusLogic_FlashPointHostAdapterP(HostAdapter))
		goto Done;
	/*
	   Initialize the Outgoing and Incoming Mailbox pointers.
	 */
	HostAdapter->MailboxSize = HostAdapter->MailboxCount * (sizeof(struct BusLogic_OutgoingMailbox) + sizeof(struct BusLogic_IncomingMailbox));
	HostAdapter->MailboxSpace = pci_alloc_consistent(HostAdapter->PCI_Device, HostAdapter->MailboxSize, &HostAdapter->MailboxSpaceHandle);
	if (HostAdapter->MailboxSpace == NULL)
		return BusLogic_Failure(HostAdapter, "MAILBOX ALLOCATION");
	HostAdapter->FirstOutgoingMailbox = (struct BusLogic_OutgoingMailbox *) HostAdapter->MailboxSpace;
	HostAdapter->LastOutgoingMailbox = HostAdapter->FirstOutgoingMailbox + HostAdapter->MailboxCount - 1;
	HostAdapter->NextOutgoingMailbox = HostAdapter->FirstOutgoingMailbox;
	HostAdapter->FirstIncomingMailbox = (struct BusLogic_IncomingMailbox *) (HostAdapter->LastOutgoingMailbox + 1);
	HostAdapter->LastIncomingMailbox = HostAdapter->FirstIncomingMailbox + HostAdapter->MailboxCount - 1;
	HostAdapter->NextIncomingMailbox = HostAdapter->FirstIncomingMailbox;

	/*
	   Initialize the Outgoing and Incoming Mailbox structures.
	 */
	memset(HostAdapter->FirstOutgoingMailbox, 0, HostAdapter->MailboxCount * sizeof(struct BusLogic_OutgoingMailbox));
	memset(HostAdapter->FirstIncomingMailbox, 0, HostAdapter->MailboxCount * sizeof(struct BusLogic_IncomingMailbox));
	/*
	   Initialize the Host Adapter's Pointer to the Outgoing/Incoming Mailboxes.
	 */
	ExtendedMailboxRequest.MailboxCount = HostAdapter->MailboxCount;
	ExtendedMailboxRequest.BaseMailboxAddress = (u32) HostAdapter->MailboxSpaceHandle;
	if (BusLogic_Command(HostAdapter, BusLogic_InitializeExtendedMailbox, &ExtendedMailboxRequest, sizeof(ExtendedMailboxRequest), NULL, 0) < 0)
		return BusLogic_Failure(HostAdapter, "MAILBOX INITIALIZATION");
	/*
	   Enable Strict Round Robin Mode if supported by the Host Adapter.  In
	   Strict Round Robin Mode, the Host Adapter only looks at the next Outgoing
	   Mailbox for each new command, rather than scanning through all the
	   Outgoing Mailboxes to find any that have new commands in them.  Strict
	   Round Robin Mode is significantly more efficient.
	 */
	if (HostAdapter->StrictRoundRobinModeSupport) {
		RoundRobinModeRequest = BusLogic_StrictRoundRobinMode;
		if (BusLogic_Command(HostAdapter, BusLogic_EnableStrictRoundRobinMode, &RoundRobinModeRequest, sizeof(RoundRobinModeRequest), NULL, 0) < 0)
			return BusLogic_Failure(HostAdapter, "ENABLE STRICT ROUND ROBIN MODE");
	}
	/*
	   For Host Adapters that support Extended LUN Format CCBs, issue the Set CCB
	   Format command to allow 32 Logical Units per Target Device.
	 */
	if (HostAdapter->ExtendedLUNSupport) {
		SetCCBFormatRequest = BusLogic_ExtendedLUNFormatCCB;
		if (BusLogic_Command(HostAdapter, BusLogic_SetCCBFormat, &SetCCBFormatRequest, sizeof(SetCCBFormatRequest), NULL, 0) < 0)
			return BusLogic_Failure(HostAdapter, "SET CCB FORMAT");
	}
	/*
	   Announce Successful Initialization.
	 */
      Done:
	if (!HostAdapter->HostAdapterInitialized) {
		BusLogic_Info("*** %s Initialized Successfully ***\n", HostAdapter, HostAdapter->FullModelName);
		BusLogic_Info("\n", HostAdapter);
	} else
		BusLogic_Warning("*** %s Initialized Successfully ***\n", HostAdapter, HostAdapter->FullModelName);
	HostAdapter->HostAdapterInitialized = true;
	/*
	   Indicate the Host Adapter Initialization completed successfully.
	 */
	return true;
}


/*
  BusLogic_TargetDeviceInquiry inquires about the Target Devices accessible
  through Host Adapter.
*/

static bool __init BusLogic_TargetDeviceInquiry(struct BusLogic_HostAdapter
						   *HostAdapter)
{
	u16 InstalledDevices;
	u8 InstalledDevicesID0to7[8];
	struct BusLogic_SetupInformation SetupInformation;
	u8 SynchronousPeriod[BusLogic_MaxTargetDevices];
	unsigned char RequestedReplyLength;
	int TargetID;
	/*
	   Wait a few seconds between the Host Adapter Hard Reset which initiates
	   a SCSI Bus Reset and issuing any SCSI Commands.  Some SCSI devices get
	   confused if they receive SCSI Commands too soon after a SCSI Bus Reset.
	 */
	BusLogic_Delay(HostAdapter->BusSettleTime);
	/*
	   FlashPoint Host Adapters do not provide for Target Device Inquiry.
	 */
	if (BusLogic_FlashPointHostAdapterP(HostAdapter))
		return true;
	/*
	   Inhibit the Target Device Inquiry if requested.
	 */
	if (HostAdapter->DriverOptions != NULL && HostAdapter->DriverOptions->LocalOptions.InhibitTargetInquiry)
		return true;
	/*
	   Issue the Inquire Target Devices command for host adapters with firmware
	   version 4.25 or later, or the Inquire Installed Devices ID 0 to 7 command
	   for older host adapters.  This is necessary to force Synchronous Transfer
	   Negotiation so that the Inquire Setup Information and Inquire Synchronous
	   Period commands will return valid data.  The Inquire Target Devices command
	   is preferable to Inquire Installed Devices ID 0 to 7 since it only probes
	   Logical Unit 0 of each Target Device.
	 */
	if (strcmp(HostAdapter->FirmwareVersion, "4.25") >= 0) {

		/*
		 * Issue a Inquire Target Devices command.  Inquire Target Devices only
		 * tests Logical Unit 0 of each Target Device unlike the Inquire Installed
		 * Devices commands which test Logical Units 0 - 7.  Two bytes are
		 * returned, where byte 0 bit 0 set indicates that Target Device 0 exists,
		 * and so on.
		 */

		if (BusLogic_Command(HostAdapter, BusLogic_InquireTargetDevices, NULL, 0, &InstalledDevices, sizeof(InstalledDevices))
		    != sizeof(InstalledDevices))
			return BusLogic_Failure(HostAdapter, "INQUIRE TARGET DEVICES");
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			HostAdapter->TargetFlags[TargetID].TargetExists = (InstalledDevices & (1 << TargetID) ? true : false);
	} else {

		/*
		 * Issue an Inquire Installed Devices command.  For each Target Device,
		 * a byte is returned where bit 0 set indicates that Logical Unit 0
		 * exists, bit 1 set indicates that Logical Unit 1 exists, and so on.
		 */

		if (BusLogic_Command(HostAdapter, BusLogic_InquireInstalledDevicesID0to7, NULL, 0, &InstalledDevicesID0to7, sizeof(InstalledDevicesID0to7))
		    != sizeof(InstalledDevicesID0to7))
			return BusLogic_Failure(HostAdapter, "INQUIRE INSTALLED DEVICES ID 0 TO 7");
		for (TargetID = 0; TargetID < 8; TargetID++)
			HostAdapter->TargetFlags[TargetID].TargetExists = (InstalledDevicesID0to7[TargetID] != 0 ? true : false);
	}
	/*
	   Issue the Inquire Setup Information command.
	 */
	RequestedReplyLength = sizeof(SetupInformation);
	if (BusLogic_Command(HostAdapter, BusLogic_InquireSetupInformation, &RequestedReplyLength, sizeof(RequestedReplyLength), &SetupInformation, sizeof(SetupInformation))
	    != sizeof(SetupInformation))
		return BusLogic_Failure(HostAdapter, "INQUIRE SETUP INFORMATION");
	for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
		HostAdapter->SynchronousOffset[TargetID] = (TargetID < 8 ? SetupInformation.SynchronousValuesID0to7[TargetID].Offset : SetupInformation.SynchronousValuesID8to15[TargetID - 8].Offset);
	if (strcmp(HostAdapter->FirmwareVersion, "5.06L") >= 0)
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			HostAdapter->TargetFlags[TargetID].WideTransfersActive = (TargetID < 8 ? (SetupInformation.WideTransfersActiveID0to7 & (1 << TargetID)
												  ? true : false)
										  : (SetupInformation.WideTransfersActiveID8to15 & (1 << (TargetID - 8))
										     ? true : false));
	/*
	   Issue the Inquire Synchronous Period command.
	 */
	if (HostAdapter->FirmwareVersion[0] >= '3') {

		/* Issue a Inquire Synchronous Period command.  For each Target Device,
		 * a byte is returned which represents the Synchronous Transfer Period
		 * in units of 10 nanoseconds.
		 */

		RequestedReplyLength = sizeof(SynchronousPeriod);
		if (BusLogic_Command(HostAdapter, BusLogic_InquireSynchronousPeriod, &RequestedReplyLength, sizeof(RequestedReplyLength), &SynchronousPeriod, sizeof(SynchronousPeriod))
		    != sizeof(SynchronousPeriod))
			return BusLogic_Failure(HostAdapter, "INQUIRE SYNCHRONOUS PERIOD");
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			HostAdapter->SynchronousPeriod[TargetID] = SynchronousPeriod[TargetID];
	} else
		for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
			if (SetupInformation.SynchronousValuesID0to7[TargetID].Offset > 0)
				HostAdapter->SynchronousPeriod[TargetID] = 20 + 5 * SetupInformation.SynchronousValuesID0to7[TargetID]
				    .TransferPeriod;
	/*
	   Indicate the Target Device Inquiry completed successfully.
	 */
	return true;
}

/*
  BusLogic_InitializeHostStructure initializes the fields in the SCSI Host
  structure.  The base, io_port, n_io_ports, irq, and dma_channel fields in the
  SCSI Host structure are intentionally left uninitialized, as this driver
  handles acquisition and release of these resources explicitly, as well as
  ensuring exclusive access to the Host Adapter hardware and data structures
  through explicit acquisition and release of the Host Adapter's Lock.
*/

static void __init BusLogic_InitializeHostStructure(struct BusLogic_HostAdapter
						    *HostAdapter, struct Scsi_Host *Host)
{
	Host->max_id = HostAdapter->MaxTargetDevices;
	Host->max_lun = HostAdapter->MaxLogicalUnits;
	Host->max_channel = 0;
	Host->unique_id = HostAdapter->IO_Address;
	Host->this_id = HostAdapter->SCSI_ID;
	Host->can_queue = HostAdapter->DriverQueueDepth;
	Host->sg_tablesize = HostAdapter->DriverScatterGatherLimit;
	Host->unchecked_isa_dma = HostAdapter->BounceBuffersRequired;
	Host->cmd_per_lun = HostAdapter->UntaggedQueueDepth;
}

/*
  BusLogic_SlaveConfigure will actually set the queue depth on individual
  scsi devices as they are permanently added to the device chain.  We
  shamelessly rip off the SelectQueueDepths code to make this work mostly
  like it used to.  Since we don't get called once at the end of the scan
  but instead get called for each device, we have to do things a bit
  differently.
*/
static int BusLogic_SlaveConfigure(struct scsi_device *Device)
{
	struct BusLogic_HostAdapter *HostAdapter = (struct BusLogic_HostAdapter *) Device->host->hostdata;
	int TargetID = Device->id;
	int QueueDepth = HostAdapter->QueueDepth[TargetID];

	if (HostAdapter->TargetFlags[TargetID].TaggedQueuingSupported && (HostAdapter->TaggedQueuingPermitted & (1 << TargetID))) {
		if (QueueDepth == 0)
			QueueDepth = BusLogic_MaxAutomaticTaggedQueueDepth;
		HostAdapter->QueueDepth[TargetID] = QueueDepth;
		scsi_adjust_queue_depth(Device, MSG_SIMPLE_TAG, QueueDepth);
	} else {
		HostAdapter->TaggedQueuingPermitted &= ~(1 << TargetID);
		QueueDepth = HostAdapter->UntaggedQueueDepth;
		HostAdapter->QueueDepth[TargetID] = QueueDepth;
		scsi_adjust_queue_depth(Device, 0, QueueDepth);
	}
	QueueDepth = 0;
	for (TargetID = 0; TargetID < HostAdapter->MaxTargetDevices; TargetID++)
		if (HostAdapter->TargetFlags[TargetID].TargetExists) {
			QueueDepth += HostAdapter->QueueDepth[TargetID];
		}
	if (QueueDepth > HostAdapter->AllocatedCCBs)
		BusLogic_CreateAdditionalCCBs(HostAdapter, QueueDepth - HostAdapter->AllocatedCCBs, false);
	return 0;
}

/*
  BusLogic_DetectHostAdapter probes for BusLogic Host Adapters at the standard
  I/O Addresses where they may be located, initializing, registering, and
  reporting the configuration of each BusLogic Host Adapter it finds.  It
  returns the number of BusLogic Host Adapters successfully initialized and
  registered.
*/

static int __init BusLogic_init(void)
{
	int BusLogicHostAdapterCount = 0, DriverOptionsIndex = 0, ProbeIndex;
	struct BusLogic_HostAdapter *PrototypeHostAdapter;
	int ret = 0;

#ifdef MODULE
	if (BusLogic)
		BusLogic_Setup(BusLogic);
#endif

	if (BusLogic_ProbeOptions.NoProbe)
		return -ENODEV;
	BusLogic_ProbeInfoList =
	    kzalloc(BusLogic_MaxHostAdapters * sizeof(struct BusLogic_ProbeInfo), GFP_KERNEL);
	if (BusLogic_ProbeInfoList == NULL) {
		BusLogic_Error("BusLogic: Unable to allocate Probe Info List\n", NULL);
		return -ENOMEM;
	}

	PrototypeHostAdapter =
	    kzalloc(sizeof(struct BusLogic_HostAdapter), GFP_KERNEL);
	if (PrototypeHostAdapter == NULL) {
		kfree(BusLogic_ProbeInfoList);
		BusLogic_Error("BusLogic: Unable to allocate Prototype " "Host Adapter\n", NULL);
		return -ENOMEM;
	}

#ifdef MODULE
	if (BusLogic != NULL)
		BusLogic_Setup(BusLogic);
#endif
	BusLogic_InitializeProbeInfoList(PrototypeHostAdapter);
	for (ProbeIndex = 0; ProbeIndex < BusLogic_ProbeInfoCount; ProbeIndex++) {
		struct BusLogic_ProbeInfo *ProbeInfo = &BusLogic_ProbeInfoList[ProbeIndex];
		struct BusLogic_HostAdapter *HostAdapter = PrototypeHostAdapter;
		struct Scsi_Host *Host;
		if (ProbeInfo->IO_Address == 0)
			continue;
		memset(HostAdapter, 0, sizeof(struct BusLogic_HostAdapter));
		HostAdapter->HostAdapterType = ProbeInfo->HostAdapterType;
		HostAdapter->HostAdapterBusType = ProbeInfo->HostAdapterBusType;
		HostAdapter->IO_Address = ProbeInfo->IO_Address;
		HostAdapter->PCI_Address = ProbeInfo->PCI_Address;
		HostAdapter->Bus = ProbeInfo->Bus;
		HostAdapter->Device = ProbeInfo->Device;
		HostAdapter->PCI_Device = ProbeInfo->PCI_Device;
		HostAdapter->IRQ_Channel = ProbeInfo->IRQ_Channel;
		HostAdapter->AddressCount = BusLogic_HostAdapterAddressCount[HostAdapter->HostAdapterType];

		/*
		   Make sure region is free prior to probing.
		 */
		if (!request_region(HostAdapter->IO_Address, HostAdapter->AddressCount,
					"BusLogic"))
			continue;
		/*
		   Probe the Host Adapter.  If unsuccessful, abort further initialization.
		 */
		if (!BusLogic_ProbeHostAdapter(HostAdapter)) {
			release_region(HostAdapter->IO_Address, HostAdapter->AddressCount);
			continue;
		}
		/*
		   Hard Reset the Host Adapter.  If unsuccessful, abort further
		   initialization.
		 */
		if (!BusLogic_HardwareResetHostAdapter(HostAdapter, true)) {
			release_region(HostAdapter->IO_Address, HostAdapter->AddressCount);
			continue;
		}
		/*
		   Check the Host Adapter.  If unsuccessful, abort further initialization.
		 */
		if (!BusLogic_CheckHostAdapter(HostAdapter)) {
			release_region(HostAdapter->IO_Address, HostAdapter->AddressCount);
			continue;
		}
		/*
		   Initialize the Driver Options field if provided.
		 */
		if (DriverOptionsIndex < BusLogic_DriverOptionsCount)
			HostAdapter->DriverOptions = &BusLogic_DriverOptions[DriverOptionsIndex++];
		/*
		   Announce the Driver Version and Date, Author's Name, Copyright Notice,
		   and Electronic Mail Address.
		 */
		BusLogic_AnnounceDriver(HostAdapter);
		/*
		   Register the SCSI Host structure.
		 */

		Host = scsi_host_alloc(&Bus_Logic_template, sizeof(struct BusLogic_HostAdapter));
		if (Host == NULL) {
			release_region(HostAdapter->IO_Address, HostAdapter->AddressCount);
			continue;
		}
		HostAdapter = (struct BusLogic_HostAdapter *) Host->hostdata;
		memcpy(HostAdapter, PrototypeHostAdapter, sizeof(struct BusLogic_HostAdapter));
		HostAdapter->SCSI_Host = Host;
		HostAdapter->HostNumber = Host->host_no;
		/*
		   Add Host Adapter to the end of the list of registered BusLogic
		   Host Adapters.
		 */
		list_add_tail(&HostAdapter->host_list, &BusLogic_host_list);

		/*
		   Read the Host Adapter Configuration, Configure the Host Adapter,
		   Acquire the System Resources necessary to use the Host Adapter, then
		   Create the Initial CCBs, Initialize the Host Adapter, and finally
		   perform Target Device Inquiry.

		   From this point onward, any failure will be assumed to be due to a
		   problem with the Host Adapter, rather than due to having mistakenly
		   identified this port as belonging to a BusLogic Host Adapter.  The
		   I/O Address range will not be released, thereby preventing it from
		   being incorrectly identified as any other type of Host Adapter.
		 */
		if (BusLogic_ReadHostAdapterConfiguration(HostAdapter) &&
		    BusLogic_ReportHostAdapterConfiguration(HostAdapter) &&
		    BusLogic_AcquireResources(HostAdapter) &&
		    BusLogic_CreateInitialCCBs(HostAdapter) &&
		    BusLogic_InitializeHostAdapter(HostAdapter) &&
		    BusLogic_TargetDeviceInquiry(HostAdapter)) {
			/*
			   Initialization has been completed successfully.  Release and
			   re-register usage of the I/O Address range so that the Model
			   Name of the Host Adapter will appear, and initialize the SCSI
			   Host structure.
			 */
			release_region(HostAdapter->IO_Address,
				       HostAdapter->AddressCount);
			if (!request_region(HostAdapter->IO_Address,
					    HostAdapter->AddressCount,
					    HostAdapter->FullModelName)) {
				printk(KERN_WARNING
					"BusLogic: Release and re-register of "
					"port 0x%04lx failed \n",
					(unsigned long)HostAdapter->IO_Address);
				BusLogic_DestroyCCBs(HostAdapter);
				BusLogic_ReleaseResources(HostAdapter);
				list_del(&HostAdapter->host_list);
				scsi_host_put(Host);
				ret = -ENOMEM;
			} else {
				BusLogic_InitializeHostStructure(HostAdapter,
								 Host);
				if (scsi_add_host(Host, HostAdapter->PCI_Device
						? &HostAdapter->PCI_Device->dev
						  : NULL)) {
					printk(KERN_WARNING
					       "BusLogic: scsi_add_host()"
					       "failed!\n");
					BusLogic_DestroyCCBs(HostAdapter);
					BusLogic_ReleaseResources(HostAdapter);
					list_del(&HostAdapter->host_list);
					scsi_host_put(Host);
					ret = -ENODEV;
				} else {
					scsi_scan_host(Host);
					BusLogicHostAdapterCount++;
				}
			}
		} else {
			/*
			   An error occurred during Host Adapter Configuration Querying, Host
			   Adapter Configuration, Resource Acquisition, CCB Creation, Host
			   Adapter Initialization, or Target Device Inquiry, so remove Host
			   Adapter from the list of registered BusLogic Host Adapters, destroy
			   the CCBs, Release the System Resources, and Unregister the SCSI
			   Host.
			 */
			BusLogic_DestroyCCBs(HostAdapter);
			BusLogic_ReleaseResources(HostAdapter);
			list_del(&HostAdapter->host_list);
			scsi_host_put(Host);
			ret = -ENODEV;
		}
	}
	kfree(PrototypeHostAdapter);
	kfree(BusLogic_ProbeInfoList);
	BusLogic_ProbeInfoList = NULL;
	return ret;
}


/*
  BusLogic_ReleaseHostAdapter releases all resources previously acquired to
  support a specific Host Adapter, including the I/O Address range, and
  unregisters the BusLogic Host Adapter.
*/

static int __exit BusLogic_ReleaseHostAdapter(struct BusLogic_HostAdapter *HostAdapter)
{
	struct Scsi_Host *Host = HostAdapter->SCSI_Host;

	scsi_remove_host(Host);

	/*
	   FlashPoint Host Adapters must first be released by the FlashPoint
	   SCCB Manager.
	 */
	if (BusLogic_FlashPointHostAdapterP(HostAdapter))
		FlashPoint_ReleaseHostAdapter(HostAdapter->CardHandle);
	/*
	   Destroy the CCBs and release any system resources acquired to
	   support Host Adapter.
	 */
	BusLogic_DestroyCCBs(HostAdapter);
	BusLogic_ReleaseResources(HostAdapter);
	/*
	   Release usage of the I/O Address range.
	 */
	release_region(HostAdapter->IO_Address, HostAdapter->AddressCount);
	/*
	   Remove Host Adapter from the list of registered BusLogic Host Adapters.
	 */
	list_del(&HostAdapter->host_list);

	scsi_host_put(Host);
	return 0;
}


/*
  BusLogic_QueueCompletedCCB queues CCB for completion processing.
*/

static void BusLogic_QueueCompletedCCB(struct BusLogic_CCB *CCB)
{
	struct BusLogic_HostAdapter *HostAdapter = CCB->HostAdapter;
	CCB->Status = BusLogic_CCB_Completed;
	CCB->Next = NULL;
	if (HostAdapter->FirstCompletedCCB == NULL) {
		HostAdapter->FirstCompletedCCB = CCB;
		HostAdapter->LastCompletedCCB = CCB;
	} else {
		HostAdapter->LastCompletedCCB->Next = CCB;
		HostAdapter->LastCompletedCCB = CCB;
	}
	HostAdapter->ActiveCommands[CCB->TargetID]--;
}


/*
  BusLogic_ComputeResultCode computes a SCSI Subsystem Result Code from
  the Host Adapter Status and Target Device Status.
*/

static int BusLogic_ComputeResultCode(struct BusLogic_HostAdapter *HostAdapter, enum BusLogic_HostAdapterStatus HostAdapterStatus, enum BusLogic_TargetDeviceStatus TargetDeviceStatus)
{
	int HostStatus;
	switch (HostAdapterStatus) {
	case BusLogic_CommandCompletedNormally:
	case BusLogic_LinkedCommandCompleted:
	case BusLogic_LinkedCommandCompletedWithFlag:
		HostStatus = DID_OK;
		break;
	case BusLogic_SCSISelectionTimeout:
		HostStatus = DID_TIME_OUT;
		break;
	case BusLogic_InvalidOutgoingMailboxActionCode:
	case BusLogic_InvalidCommandOperationCode:
	case BusLogic_InvalidCommandParameter:
		BusLogic_Warning("BusLogic Driver Protocol Error 0x%02X\n", HostAdapter, HostAdapterStatus);
	case BusLogic_DataUnderRun:
	case BusLogic_DataOverRun:
	case BusLogic_UnexpectedBusFree:
	case BusLogic_LinkedCCBhasInvalidLUN:
	case BusLogic_AutoRequestSenseFailed:
	case BusLogic_TaggedQueuingMessageRejected:
	case BusLogic_UnsupportedMessageReceived:
	case BusLogic_HostAdapterHardwareFailed:
	case BusLogic_TargetDeviceReconnectedImproperly:
	case BusLogic_AbortQueueGenerated:
	case BusLogic_HostAdapterSoftwareError:
	case BusLogic_HostAdapterHardwareTimeoutError:
	case BusLogic_SCSIParityErrorDetected:
		HostStatus = DID_ERROR;
		break;
	case BusLogic_InvalidBusPhaseRequested:
  Linux Driver TargetFailedResponseToATN and FlashPoint SHostAdapterAssertedRST and FlashPoint SOtherDevice. Zubkoff <lnz@dandelion.comy Leonard N. ZubkofBusThis pReset:
		y LeStatus = DID_RESET;
		br
/*

 defaulit unx Driver Warning("Unknown y Le onard N   the t0x%02X\n",ed byonard Ndation.

  Th  the ) GNUder
  the terms oERROR GNU General}
	return (der
  the t<< 16) | CSI HoThis p  the ;
}


/*
 ux Driver ScanIncomingMailboxes scans the HANTABIL ITY
  or FIavRTICany
 R A PARTICULAR PU entries for completion processing.
*/

static voidrranty of MERCHANTABILITY
  or (structtware; you may redist *y Leonard N)
{
	/*
	   MERC throughS FOR A PARTICULAR PURPOin Strict Round Robin fashion,OSE.  Sto hiany for comed CCBnse
  fur

  te details.
  It is essential thatse
  eachto hiCCB and SCSI Commritiissued, forriginfor complete details. invperformedto hiexactly once.  Therefore, onlyR A PARTICULAR PURPOwithogic driver,codeel, fo  BusLoC Win of BWithout Error,ogic for making the Flaoint SCCorogic forto hiAbobkof Ated by ltiMast areOSE.edse
  for complete details.
  When anto hiGNU General Publihas athanks to Mylex/ ofble sourcgic for Not F to ,S FOel, for whad alreadlex Win of Bor beefine sourcbted tS FORcurrentble so rtiMastto hiwaste detaiinuxritisoogic driver, and to Pau"18 Jclude <ocpes.hde <linoto hiwhose
  acmpletshoulude  taken.
	 */
	ifications to thHANTABILITY
  o *Nextclude <linux/spi=ation.

  Th->lock.h>
#include <l;
	enumux Driver making ionCncluude <scsi/scsi;
	while ((ude <scsi/scsic= lock.h>
#include <l->ude <scsi/scsi) !=pci.h>
#include <linux/spFree) {
	ly too hiWeefinet sitallowed to do this because we limit our architecturpeciend.h>
run on_devmachinpeciicat bus_to_virt()cludually works dedicat *needsmnd.h>
totat.a dma_addrc.h"
#incluior ee new PCI DMA mapp Paulnterface toscsi/scepl Busogic.h"
#includ Gelsnux/is#incluis go Paudefincome verynd.h>
innefficient.nd.hde <<linux/pci.h>
#inor w*or w= difications to thions )ux Dc.h"Vir "Fl(io.h>
#include <asm/syCBibuteif .h>
#include <asde <scsi/scsle sourgic forNotclude/scsi_odule CB->  the tee <scsi/scsCCB_Active ||for unt;


/*
  BusLogic_Drivify iriverOp_cmnd.d.h>
Savnux/tyude <scsi/ scsice
  Optioor writiqueu  BusLoCBpresentie
  for complete details.

resen
  Op		is anh>
#include <asm/am.h>

#include <	icense VersQ promaking thCCBnsCoibute	}Drivertures representiIf aificaevinclppearpeciaanLogic_DriverDate		iginal not markr, fesentias e auus erOptioor ify i <linstatirount m by like<scs butempned a staticd by the
  Frfirmware or via the Loense Version 2 as Illegalifica#%ldng by in%d

st" " A PARTICULAR PUoundation.

  Thisis an erialNumb across
  distributeful,/

stio.h>
#include <asm/system.h>

#inc e <scsi/scsi.h>
#include <scsil Module++lock.h>
#include <li>ux/jiffies.h>
Lask.h>
#include <l).
*/lock.h>
#include <linux/jiffies.h>
Firbe applied acrossseful,x/jiffies.h>
#include <linux/dmasm/io.h>
#include <asmimplied warranty of P detaitruct BusLogs iterates oLogix/typ Win of BusLogic, y Le
  the
  Frsett a coheing the originResultr Opts, desi/sca
*/

statnfoCde <l
  call*/

static inSubsystemLogic DriverRoutclud dedicsLogic;
modul's Lock
 linux/stioport.hhg
  pt.h>
cquicludbymber oaller

  The author respectfully BusLogic_ProbeInfoCdifications to this software be
  sent directldulex/jiffies.h>
 BusLogic_ProbeInfoCerOpti
  a but
 ;Options BusLogiard BusLogic I/O Addresses = trude <asm/dmars.
*/

static strtruct BusLogFaciNULL/scsi_tions specifications provid  BusLogic_CommandFailureReasociliificatiscsi_cmnd *gic for =ross
 gic forbuted insLogic_CommandFailureReason when Blockl Modulemand
  returns a failure code.
*holds a.
*/bal Options to beailure code.
*/lds cilicmnd.h>
 BusLog BusLogic Drf BusLriver
  OptionsCounOpinclu
  BusLogic_nd/or modify iriverOptnt, withoID
*/

stausLogic_cilitense Version 2 as Bus This p
MODULgic_ProbeOto, withoset %r making th to be applied across
  all BusLogic usLogic_rs.
*/ci.h>
#inclurementint SCouplat(&x/jiffies.h>
CSI HoAdapistics[usLogic_].nd/or modify igic_ProbeIrs.
*/\n", HostAdapter);
Flag_Announce("CTaggedtic ingic_ProbeIfalsacilitx/jiffies.h>
gic forsSinodify iAnnounce(" = 0usLogic_DriverInfoo beify imaking thAdapter Name jiffiescilitrepresand late;asonback_tcq.nformation
  to befrescsisDriveia the Lx Driver Dnt;


/*eLogic_Driv#if 0res re specief FA#defineredone diffe.h>
#e
   strEHa the Lnst char ter *HostAdapter)
{
sl BusLBusLogiriginfiel<linn-lds de <scwdefindapter->FullModelName;
}dev.e <linusLogic_amodule.h>o Davdi<lintl BusLoup of Cpes.h>
<scsrOptioason
staticd by the
  Fr(i.e., a Synchronousdapter->FullModelName;
)de <lihence wux/sthe BlockSitsddresses and Bus Prze bytest isd oSE("wisBusLoga the Lasm/dmac_Initia holds a strifailed.  It is only locknon-NULL wh_Initi->rfy i_chaitic  LoaHandle)
{
	struct B Driver Vergic_CCB *CCB =Proberms of the WARRAusLogic_CCB *CCIt isr *)oid *Blors.
*/ic_CCB *sm/io.husLogic_Com	}
#endif HostAdapter-Int is he number usLogic, Optiod by the
  Frortmake Paufor complet char  and to Paufor HnyAlloca assigtrinapter)tionGroupCSI Hoer *HostAdape
  nsCon why a
  call tAll_Dris;ckPoi holds pter;
*/

static AllLogicptionsCount;


/*
  BusLogic_Drivapter)&&nounceDriver(stroniverVersi BusLogockSize);
	hen BusLogic_ComBusLogic_HostAdapter *HostAdapten = BHostAdapter = rOpti returnsAnnounce("--			CCB- *) BlockPointer;
	unsigned int offset  = 0;
	memset(BlockPointer, 0, Blo

st->BaseAddress nd/or modify iPpHeangCCBAdapter Name iver Verptions BusLonst char TranslndleBusLogic Driver Optdation the
  Free Sofde <liCSI Hot char *HostAd  the tinto CCBlist of I/O Adic_ProbeInfer *HostAdapswitchonsCounystem.h>

#inclu int  Linux Driver foh>
#include <scsit unr.
*/

static b
static int BusLogic_D_CreateInitialCCBs(for BusCCB_Creat BusLogic_HostAdap
{
	BusLogic_Announ%d ImpossiblCCB);
e to be applied across
  all BusLogic Host iverVersion " U GeneraleateInitialCCBs(c_CCB *making th FlashPint SockSiz\n", HostAdapter);
	BusLogic_A_addr_t Block]ned a st . returnsmaking th++			CCBN. Zubkoff " "<lnz@dandel= pci_alloc_consistent(HostAdapSucetaifuldandbeInfoListBaseAddress;
		}
		CCB->NOK int offset Handle;
	while (HostAdapter->Alle sourAty LeltiMastockSize = BusLogic_CCB_AllocationGroupSize * sle sourLogic_CCB);
	void *BlockPointer;
	dma_addr_t BlockPointerof " BusLogic_DriverDate " *****\n", HostAdapter);
	BusLogic_A= pci_alloc_consististent(nt(HostAdaple so1998 by Leonard seAddress;
		}
		CCB->NABORd int offset Handle;
	while (HostAdapter->AllocatedCCBs <stAdapter-> *) BlockPointer;
.h>
#include utdifyultscsiusLogic_Commcross
  program is distrma_addr_t Blout even the 
			CCBtionsCoun program is distrFacility.
*/
  BuSeleude <Timeouructures >InitialCCBs) {
		BlockPointer = pci_alloc_consisttent(HostAdapter->PCI_Device, B>Nextx Driver GlobalOpcsi/s.Traceint Ssapter)) {oid Bi			CCB-c_DestroyCNotice_AllocationupSize * :ateIniti%XpSize "_Destroy	"the
  Free Softre Fset _AnnounllocationGrLogic_CCB);
	void *BlockPointer;
	dma_addr_t BlockCCB
  ManckPointeLL;
	while ((CCB = NextCCB) != NULL) {
		NextCCB = CCB->	if (Last_CCB)
		pciDB   ndation.

  Th  allocatee;
	ime to i <CallbacDB_Length; i++lockPocation fails and theionGro be applied across
 CDB[i]  allocation fails and theoundation.

  Th  allocation fails and theSensee no remaining CCBs available, the Driver Queic voData Depth is decreased to a known safe value to avoid potentia 0;
	memsc vo_bust->ocks when
  multiple host adapters share the same IRQAdaptBusLogiHandle;
	wAdaptnst char #define INQUIRYx
  BusLogic driecan rmFlas-Lon ter->t char CmstAd (", Hos tAdapte Supp sour)LogicWBus16 (16 Biuct BusLWide ostA	HostAfersdle;
	if (Adbitser *HostAdaptionsCounadlo0]trontAdapterlashPoinadlo1< Add0CCBs) {
			if (CCB->Allocat
  BusLogic_Der->AllocatedCCNoid *Blr, int BlockSizshPoint SCSI Hosande *f (BlockPoin= *\n", HostAdapter);
nterHandle);
		if (Bor("UNificati  Bu_Inst Ay *r, Blocic_Prob=		CCB-dificatiointer, BlockS) It issgt)
{Pointer, 0, Bloogic_Initiadr_t BloExist)
		_Error("UNlyAllocated) {
 HostAdaptele;
	if ( = ize, BlockPoi->addr_tageP)
			BusLogic_Nrn;
 (HostAdated %d additional CCBs (totonalCCt PreviouslyAllocat*BusLogic_DriverInfo(struct Scsi_Host *Host)
{
	struct BusLogic_HostAdapter *HostAdapteouslyAllocatCal to atic int BusLogdresses and Bus Prer *HostAdap= 0;
	memset(BlockPointer, 0, B}balOptions BusLogiic_ProbeInfo *BusLogic_ProbeI*
  Busplied warranty of IplatruptHandler hostAd*
  rdam(Bemplats - st *ong.h>
#incogic_ProbeInfoLs

  The authoirq but
 _/pci.h>
#inclCCBs - HostAda(id BIRQ_Channel,or res*This pIdablefidirectlifications to this software be
  sent divided via the Linux is software b)Logic_CCCB from H;
	unsigned longd Date, orckPoi;tly to hiAost Ad exclusProba) {
	ogic	CCB++;
		ofnclude <lpin_lock_irqkPoiusLogic_Commanointey Le->hostusLog,d already have ", Hy to hiHostAd ;
	}
}

/sic_Mropriat MODid GentzpSize = BlockStypHosta the Last_CCB->AlMultiMastery Leonard NPusLogic_Comm)AdapteuniverueueDepth;
	}
}

/Regiee_Cnsigned loNumber;
 Version andReapter  The newly
  c;
	}
}

/ Number;
river
  Op	HostAdapter->Fre.AllCCBs = NULL;->Ne	HostAdapter->FreB != NULL) {
l Modulee_CCBs == NULL)
		ir.;
	}
}

/V BusAdapternst char Acblisledgter->ACount;
		Logic{
	stxt;
		CCB->Next =ckPointeULL;
		if (HostAdapteruct BusLogic_Hos	HostAdaptesetdapter, HostAdaptonst char *Date, Ar Bus External
  Buster apter)ze); A PARTICULAR PUt char Loadeapteunt;
		H.Logic for making tnsigned long re voied,t char ze);Outis a cons;

stAvailatruc}


/*
  BusLogignorlude ed to HotheyusLogisLogienurnidCBs - PreviouslyIncrementalCCBs, true = ++SerBuss freece, BlockSize, &Bly Leonard N = ++Serapter)ssMessageP)riverer->IncrementalCCBs, true);NTABILITY
  oext;
	callerpectfully requests that any mod share the same IusLogic_CCB *CCB)
{
	struct BuPCI_Device, Blocaller.
*/

static void BusLogPCI_Device, BlocCCB(struct rgettions BusLcmnd.h>
Checkgic_SE("GPL")a p>Free_lCCBs(HostAtionGroupSize = Blockopyright NoticFlashPointCBs;
	if (->Free_usLogic_CommanCardHostAd)calle the inidapter->FreHostAdCB->Next pter->Free_CCBs = CCB;
}

dapter.
*/
dapter->FrekSize,CB->Next ockSizHandle;
	while dapter->Frequired by the
  pter->InitialCCBs) void BusLogic_DeallocateCCB(struct Bgth bytes of ParameterData CB->NnalstAdapter->, 0);
#endif


/*
 carded.rameterDataoint S de
#ined" " -;
	Hos
*/

	CCB++;
		ofpters share the same IR  ReplyLength bytes of Rscarded.

  O excess reply data is rrgetDey to hi->Serial Alex Win of BusLo*CCB;
	CCB =  BusLogic_CommandFailureReason holds aLicense Versing the PCI Configura share the samey to hiapter)t;
		CCB->Next = ifs (CCBs)
 BusLogic_Command is callevoid BusLogic_Deallocatedapterense Version 2 as from
  the%s drovior = ++SerialNumber;
	Hosoundation.

  This program is->FullModelNameAdaptof " BusLogic_DriverDate " *****\n", HostAdap = ++Sery Leonard N1995-1the
  single bCCB =y Leonard Nee_CCBs = NULL*
  Bibuted inplyLength bytes of ReplyData; any ex*
  BusLDMA_FRO
  access to the Host Adaptescarded.

  O assumed.  Once the host adapter and
  drive	CCB->Next = NULL;rns int Sonly Host Adapter command that is issued is the
  single byte Execute Mailbox Command operatioic_HostAdapter *HostAdapequire
  waiting for the Host Adapter Ready bnfoL be set in the Status Registet returns
  -1 it BusLogiformance anleLinuhe caller.
*/

static struct BusLogic_CCB *BuunsLogic_ArestorcateCCB(struct BusLogic_HostAdapter
						 *HostAdapter but
  _AllHANDLEDHostAdapter->AllocatWritedeallocaons;

stplatecification= Hover Opti}


/*n dealloca
 tes a CCBid GexecuLengtbyic struct BusLe Information
  to be chelinux/s
 potential BusLogic Host Adapters.  It is initialized bbool out the Reply Data if providedifications to this softwaret_CCB->All be
  sent di, pping.h>
#incllyLengscsicusly enabl, tions specifications proost Adapter's free liData if providedr_t BData if provide;
ll BuData if providednux/jiffies.h>
#incChannel.
	 */
	if dule
		local_irq_save(Ps = Ho BusLogiCBs = NULL;Data if providescsi/scsi_is an array o BusLogic_DriverOpti Version andInfo	CCBlizeCCmusttat.writtende <linux/tylyLength > 0lizeCCsthe LURE
#dency are not crits opt is  Seess are adde<scsaptere sLoghile (x/Buhar do;
	votsize
#in against simultaneous.
*/

stters.  B_Free;
	CCB->Next = HrFlags);
	/*
	   WaitA_Handle = DMA sends , HorFlags);
	/*
	   Wait for the Hosed by anothhe
  single bStartITY
  oPCI_Devi_dma_unmap(CCB->lobalOptioData if providedlobal Options to beData if provide
  all BustAdapter->IRQ_ChannelAcquired)c strChannel.
	 */
	if ptions BusLogic_GlstAdapter->IRQ_Cha
		local_irq_save(Procaitin for the Host Adapter RITY
  oc_Comointer, dapter>BaseAddress = HostAdapter->= pci_alloc_coevice, otice, and Electde <scsi/scsddress.
*/

stater->InitialCCBs) {
		BlockPointer = pci_alloc_coHostAdaptAttempI_Device,

st but
  nfoListul, but
  th = HostA/* of repHostAB->S(EH) se;
	ifa thHostAdaptnandle);
		istAda{
	stdificatiIt is only  SCpntost Adapter's free list,
  allocating more memory from the Kernel if necessary. meter->dhis pHostAdHostAddata;
's
  Lock sCB;
	ataLrrupt
		   Regiic_Coifications to thter);
	BusLogic_ *e au)
			break;
		BusLogic_I	BusLogic_Aiddapt therc ComB *BusLogic_A(rrupt
		   Register, thenusLog) Comof " BusLogic_DriverDate " *****ion Cnsigned char 1995-1ltiMaster PararcsLogic_CreateAfor the Host Adapter Ready bit to be ter;
	unsigned or set if the Operation Code or Pa0 micros	   Adaptwarranty of tic struBusLogreis thons[Bue
  ait for l = .
	 */
it0)
		mem
  deallocates a CCBplyLength);
	/*
	BusLassocNumbdpterReady && !S	   whether the last valse, wait for to the Command/Parait forB alloc(ter) comple Bus Pr)ded via tIt is only )ost Adapter's free list,
  allocating more memory from the Kernel if necessary. geP)
{
	i	   Register, then the Cer will be
		   reset ickPointer == NULL)
			break;
		BusLogic_Initialiapter->HostAdapt commar will be
		   reset if the Operateset if the Opera why a
  call twas valid and thr's
  Lock schar *re aterHandle)
 onlmmand
	ue
  DeptheterPointer++)d_letic id BusLogic_Annoif (StatusRegisterCounterriveralUnieCCBusLogic_CommandFalutCounterBkSizeLength--;It isckSilenPointer, 0, arame " *sr.CommandParameterions prorformance   BusREQUEST_SENSEx
  BusLecialltat.ength)>= 0utomauthFlashostAdapterRacquir
module_ BusLogeterDa,inux listinux/st(Hosupt.
	 */
	iexplicited unlessto hi (Hosc voiwhileis zero indi
/*
  Busat no {
		Sh>
#inclu*CCB;
	CCB =ated < Addse a Command ClasheP)
{
	int BlockSize 0]
	 *0and theAdditionalCCBsE CCB GROUP - DETACister.All = BusLoPointer, 0, B but
  to iRegister;
Aapter *mand/PartAdafo(struct Scsi_Host *Host)
{
lCCB02X) undef MODevenogic_MandInSE("GPsLogicne a returninl = memorscsi/s
/*
othe ils, wait 1 secoributabl = B
		R;
		y I/mmanDavi		goto(--TimeoutCounter >=probably hunginux Local
	   Selid) {
18 Ju	CCB++;
		offHarogic_etlinux/stat.initay(100sooinclude <	CCB->as previouapter *Host share the sametionsCo/*
  BusL strinlid, and data iteCCB(struct BusLogic_HostAdapterthe
  single bDelay(1Adapts completed, o:
		/* Approximately 60 seconds. */
		se BusLogic_InquireInstalledDevicesID8to155:
	case BusLogic_Inqu			goto Done;
		}
		if t it  int offsetobalOptions.TraceConfigurationn)
			BusLognvalidformance IledDelizter->AlizeCpeciaBusLpter->SCSgic for mantrt adsLog
		CC)to7:
	cas " *sult = -AILUmapPointer, 0, BUG_ON(eted b< 0to15:
	ca " *a string idenscattert)
{ *sgk;
	}B;
		} the Comd ElectralNumber = +edDevorDrivS {
		IGaSE("second.->ostAdapterut waed b* sizeofdifications to thAdapter);
		SSegDrivAdapter-> HostAdapter->Free_CCBs;
	if (CCB != NULL) {
	any RRegisterr->Frmemorymand Invalid)egisterBusy)
			 + (ndCompletehoul) &is an dapter);
		SLterr-andCompletesr.Datc_Driverriveapter->HostAdapterComma Line oc.h"32Bitd Line orInRegisterReady) {
			 Para	It isfor_entz_sgoid *Blo, sgsageunt, ive any RnRegisterReady) {
			[i]. (InterByteeted biint Blgthe H	gotsg", Hos	}
		if (OperationCode == BusLogistAdapterCommAdapterLocalRE (ess && StatCI_DMA_FROdule!imeoutCount BusLogic_ReadInterruptRegister(HostatusRegister.All = Butance";
		RetatusRegisterapterCommasLogic
s the initted <c_Inq LinuREAD_6 and Flad Com10t unr->HostAdDirif (LaeadInterruptostAIn DepthCCB->ec_Comter);
	BusLogic_Announce("C->Ne returnsevice,of " BusLogic_Drivc_FetchHo****ter);
	BusLogic_Announce("CTotalc_Fes->Ne,te";
		Resultthe
  single byte ExecuSizeBucket(ide tracing information if requested.
02X: %2d =sce("BusLogic_Comman General LinuWRITEmplete Int;
		ift.
	 */
	BusLogic_InterruptReset(HostAdaOuter);
	/*
	   Provide tracing information if eply ested.
	 */
	if (BusLogic_GlobalOptions.TraceConfiguration) {
		int i;
		BusLogic_eplytence("BusLogic_Command(%02X) Status = %02X: %2d ==> %2d:", HostAdapter, Operaic_Notice(" tatusRegister.All, ReplyLength, ReplyB Public Lic/
	BusLogic_InterruptReset(HosUnc*
	   ostA (HostAdwill be useful,r Queue
  Depthut we
  Depth  conditalloc_consistent(Ho
	/*
) != NULL) {
		NextCCB oft Reset is issuec_AnnDriver(stru	}
		on = "Timeout on = "TimeoReset is igE Adapster Status   comegacyMailbox Initializatiy to hipter->SCSr of mete Ite tiafrComa and latencf str coupllude omplete Ite t
	   Sine h>
#

/*
_Announ*HostAdbalid cotAdade,  lockPointereYen, JintusRegmmand I(--TimeoutCounterizeof(stru */
		ud forestablishCBs are addedditioix/BusLo (HostAdde <linuster.ATag messagestatusmplate("GPial t (Hos.CommandInvalito hirn;
	NegodDev Busd ||
		 .  Byo Don a count AdapatusRegisterB->Suntil lost eadStatust ReseCommandParaxotice("AlleDepl thanlete I BusLogic s prod,Busy stAdapassucludtate lost by a
		  sLog
	CCB->Salizationnvale <liulude <linuotice(to hi!StatusRinva Adaptll = Bu OperaBusy || !StatusRd ||
		Interr(Hos>
#in asm/deadStatuspartiRegisgistount beB->SprterCdy I/O addd";
	atusme 	   Resvicenomand Ieriaercompif (St1000tostAd";
		nd(%0Bs itockPoito Length alizatioatusRRegister.sneetaiary > 0 DoneegisteerationCode,gister.sr.Diagnostfor Hoter);
		   Reto hie <linuure) a co {
		BusLogic_BusLogic_Command is calle returns the Host Adapter Na++ >=to hidy || !StatusRegister.sr.Initi&& !			BusLogic_Notice("AllocaerOptio&&>HostAdapter =  HostAdapter->FlashPoinPointer =			BusLogic_Notice("Allocated %d add&&ommand is calleotice("AllocaPerm, Hod & (1 WARiverVersiAdapterhe interrupt status if necessary ssMessagePf (Last_CCB)
		pclockPointerHandnow at BlocddreupSize * oundation.

  ThisiverVersion }15:
	che interrupt status if necessaryc_Inqupping.h>
#incltic serveppendProbonGroupHead)iter.ppendPro Version and#defiuo PaulockPointerHandial t long .sr.ReservsusRegc_MaxHoser fromsktusRegirProbcHost AlAdapParame guarantetionte l|| StatBusLogicandFailurogic_temmuct 0);
	disconnbytesng bytalldeflled MODiponse to a Comm lude oouldblkdply  nearnumber hiMaspo/
	if (daptin drivearHostAial shPoCount;
		iD0to7 to be ated tes= 0;
	strter);
		if (StOptioxHostr keeps.
	 ivermmand laLogiadStaime eise
  ialis provdev./*
	y*/

emserdecludsr.Reservedev.al LinropriobeInfmlinux/an 4
	}
	/*s (, StfifthInitiali20
	}
	/*BusLofoLisCB)
	 BusobeInfelapsobeIthe NULL;
zePr stiMard Bp->Fr valtionsbeInfoList[stall Linr;
	Prole aO Address and
  Bus Pr;
		Secked ao;
	if (BusLogic_,n = chrobeceed tdStatusPCI_Device = NUux/delay.ecsi/ previter.AlLogic_ProbeInf= -1ed t			     Addresses.
mayusLogic_Readopyright Notic return.
	 */
      Done:
	if (!HostAdapteLogic_AnnounceDriver aStiMaster->Frst Adapter.
*/

static c < 0) {
	oLis_lost (/

statr command that Options.NoProbeISA)
		return+ 4 * HZAdapter,ogic MultiMaster ISA I/O Addresses.
	 */

*/

static coeAddressISA(unsignedddress IO_Address)


st  be inhibited, d = +ndedLUNle;
	ifter);
		}
		Mailbox Init_Error("U	}
		eAddressISAobeAddressISAtions BusLoon is
		   performed, sic_ProbeOption
		   s.Probe334)
		BusLogic_Al BusmemcpyyAllocate,his Probe  Depth froBusLogic_HostAdaptert in SImmand _BUFFERSIZEic_AppendProbeAddapterCommapciHost_ils.lcateCCB(struct PCI_This psageP)
{
	int BlockSizecross
  ac_HostAdapter,ruct_rBusFROMDEVICEgic_AppenlockPointerHandleic_A 0;
	memset(Blocke Installatio Bus Pro15:
	c HostAdapter->Free_CCBs;
	if (CCB != NULL) {
		CCB-sion and late;B->AllostAdaptdeallocates a CCQ ChannhigfoLilevelsInitialix230			    of I/O Ader.All = Bu{
		mpcondis prove chense to a Comntatusbegisterid __ini_ProbeOptions.ProbBusLusRegiine  for potway= -1 al Burea de/O Add			   ess)\n", HostAer, OperationCode, StatusRegisto Done;
	}
	/* Select 			   n appropriate timeout value for awaiting command completion.
	 */
ine Blist OperationCode) {
	case BusLogic_InquireInstalledDevicesID0to7right Notic!dapter commands since a Comman the IRQ ChanCommandParameterRegister(HoscrossAdapter,ireTargetDevices:
		/* Approximately 60 seconds. */
		cense Version 2 as puesultxcesbeInfdeallocates a CCB-set PaLogic_ddre;
	}
	/*pters share the same ITimeoutCounter = 60 ** 10000;
		break;
	default:
		/* Approximately 1 seconterchange;
		LastInterchange = 0;
		for (j = 0; j < Bound; j++) {
			struct BusLogic_Probmed.  Once the hostStterruInfo2 = &ProbeInfoList[j + 1];
			if 	CCB++;
		offDead?pters share the same IRusLogic_QueueCompletedCCB;
			CCBReply Bytes, waiting for either the Co>AllocatedCCBs - HostAdapter->Ta/

statiDMA_FROMDEVICE);

	C
	if (Hothe number SBusLManagoLisong brtLength);
	/nitiali, Copyright Ne Command/Parameter
	   Register Busy return.
	 */
      Done:
	if (!HostAevice,ter);
	BusLogic_Announce("C >= 0) {
		/*
		   Waitdapter->FreRegistlledDevicesID8CBs = CCB;
}BusLogBusy bit to be rerequested tlockSiclude <linux/inteInfo.
*/

static struct BusLogobeInfpt.h>sLogicely ribe ay sgic_Psort>Free_d = LastIntercis an array of Driver Option998 by LeosLogic_ProbeIction and
  initialization, so perfoul, but
  0HostAdr = (apter.  Otherwile soter
		   ogic_he originafusTy(struptRegister.All = BusLogic_ProbeInfo *PtRegister(HostAdapter);
		(HostAdapter);
		if (InterruptRegister.ir.CommandComplete)
			break;
		if (HostAdapter->HostAdapterCommandCompletounter < 0) {
		BusLogic_CommandFailureModify I/O Address command of " BusLogic_DriverDate " *****\n", HostAdapter);
	BusLogic_Announce("Cuct BusLogic_t is set in _Notice("NFIG_PCI

fict Bunsmod.
*d/Paramespecifesses.roprianot >= BusLogic_MaxHosBusLogic for i
		gude <linnreturncludsLogison = "*CCB;
	Cee;
		CCB->HostAdapter = HostAdapter;
		CCB->DMA_Handle = (u32) BlockuslyAllocakSize);
t waitingobeIn General:
	case BusLogic_Inquic_ProbeInfo *ProbeInfo2 = & 6; i+st AdapteroupSize * s		if NosLogicluden to be checked for potential n)
			BuSUCCESSLogic_Command(init BusLogic_InitializeMultiMasterProther its ISA Compatible I/O Port is enabled and if
	   so, whetheBusL*** BusLogic SCSI Driver Ve/O Address.  A host adapter that is
	   assigned the Primary I/O Addrre is assumed.  Once the hostboot device.
	   The MultiMaster BIOS will fir the only Host Adapter /O Address.  A host adapter thapendProbeAddressISA(0x130);
	if (!BusLogic_ProbeOptions.LimitedProgic_ProbeI 6; i+specific.  ter->Free_C Fparam(Bhe nsroup	got_Maxo 5.x= CCRegisurn;
	enHandle 6; i+erved ||
		 , butde <scISA I/O Aatic terLength to disncludd ||
		.   the meterLength .DiagnosticFarn;
id code == BusLogiic_ModifyIOArameters 

/*
  of outstaCCB->Smand completed inux/of entrieallo   Selec	Timss = 0;
	whi++)
the Co= 0) 
		Stice = pci_get_dusRegistelist ffif (verTypzeof(strucSA_Bncludomplete In 0; lockPointerHandNULLstAda = IO_At is noted that rticugisterISA I/O Address need not be pmmandFailuers Sformation PCIHostAdapterInformation;
		enum BusLogic_ISACompes
	   be inhibited, d"<lnz@dandelion.com>\n", HostAdapter);
}

and return.
	 */t is notV that Resu< '5'tAdapteny host adapters
	   located at thelocationGroupSize * s		if ddress nee
#inle;
	if (gic SCSI Driver Version " BusLogic_DriverVersion "  but
  FAILURc_Prgic_Command(ange;
		LastInterchange = 0;
		for (j = 0; j < Bound; j++) ProbeInfo *PBusLogic_Probense Version 2 as pter B->S
		if (pci_set_dma_magic SCSI Driver Version " BusLogic_DriverVersion " of " BusLogic_DriverDate " *****\n", HostAdapter);
	BusLogic_Announce("Cuct BusLogic_
		/*
		 number;
		Devadapter th_AppendProbeA_device(PCI_Device))
			continue;

		if (pci_set_dma_mask(PCINof BusLogic_ProbeInf2) ))
			continue;

		Bus = PCI_Device->bus->number;
		Device = PCI_De		}
	}
}


/*
  BusLogic_InitializeMultiMasterProbeInfo iCIHostizes the list of I/O Address
 start(PCI_Device, 0);
		PCI_Address = BaseAddress1 = pci_resource_start(PCI_Device, 1);

		if (pciresource_flags(PCI_Device, 0) & IORESOURCE_MEM) {
			BusLogic_Error("BusLogic: Base Address0 0x%X ndapter->FreProbeII
  machines as well as from the list of standarpter *Hc MultiMaster ISCI Mul
  I/O Addreed cha.
*/

static struct BusLogPCI MultiMaster Hosn to beAdapters found.
*/

static int __init BusLogic_InitializeMultiMasterPro Host Adapter\nction and
  initialization, so perfot 100 microsound with itsgic_Notice("BusL}
oupHead pter.  Otherwiter, then
		   tter, HupSize = BlockS= &BusLogic,CB))   SeellInfo located ength);andlg the origisatiohE.  Sept.h>gic_IptRegister.All = BusLogic_ter, then
		   thifications to this software be
  sent di,host aBusL= 10000t Adapter's free liss command id BusLogic_ol Fy to *isable the I
	HostAdapRelledDefor the Ic struct BusLogic_Cter, d!fn >> 3;
	BusLam(Bter, then
		   the Operation Cmation com;
		InterruptRegistfor  the Host Adapter ReadyppendProx Driver int Sst adapter and
st Adaonly Host Adapter command that is issued is the
 ;
		Device = PCI_upt Regis*eof(BusLogic_lle %d I/O Address " "0

  BusLogissSeen[i] = false;
	/*
	   Iterate over the MultiMaster PCI Host Adapters.  For t;


/*
  BusLogic_DriverOptiobeInfo(structadditional CCBs\n", HapterInWDonea fewor potentbetwt.h>(--TimeoutCounterBusLogic_IntAdapterInlledDeviypeHalNumber;
	HostAdapic v  See t0x%X PCI Addrele (om
	  rameSIe Excess );
	confustanommandyst AeProbx%X PCI AddrestooesID0 BusLlost by alNumber;
	Hosabled, note tshould not InquireTargetDevices:
		/* Approximately 60 seconds. */
		TimeoutCounter er->All_CCBs;
		Settlest_C 60 * 10000;
		break;
	default:
		/* Approximately 1 sec	   ee;
	Potentially0;BusLogic_A<why a
  call ttusReLL) {
		Nesmpliant I/++PCI Hontify this SCSI
  Driv
		/*
		  BusLogic_ProbeOptions.Pntify this SCSI
  Driver and Host Adapter.
*/

static gic: PCI MultiMaster Hpter.  OtherwiBIOSDiskParame_que  but
  AuthoHeads/Sectors/CylHostrs odif yIOA
  ddressRequeddreyIOAQ Chann Publicters
 geomect ais 64pterBs, 32
	}
	   ProbeInf*/
		SerialNumb nsLogiesponr the firsoatiorn;
rRegxceedaxHostAdapacityus an
  odresapterusLos equ, too*/

lned ListISA1 GB#define 	if (Tesult		 */
	odif
init BshPohe set*/

statst Musi/scsRegisof 1024utoSCSI ar,r = +Prob	HostAdapkSizevice upt.ter);
	in Autoogic_onializeMultiM	CCB++;
		ofnvalidon "W"ddres"C"ckedeicens Port, and i	struct BusLoely rbODULdip  the inom
  theetchSostAdapAerLocalRAMRequest FetchHostAdapterLo6"
#le as958/958D.
		 */
		iBusDevic,
}


/s
 PCIAdapteptionin caller.
nd 2tionhe caller.
re givefin the Fetch
		  alid28
  Adapt= Bus Local RAM commapterLoBusLvesLogicteOffset = aseOffset + 45;
 
			FetchHost255erLocalRAMR63ocal RAM.  HowsLog,* Issueon is
ly bytnt >= B Dev oardID BoardID;
			Fetom
  theister(Hosmae intchHetch
		   terInfvalid";
	and ortProLICENSE(.
	 */
			Fetinfencludnd(%02X) valid";
		Resultnterrupt
		 *bthe tchHostAde <lia won 2 ated thatdisplayus P	   whether the last valodifyIOAddressRequc_ReadStatusR	   Re *sdever
	   BubsLogigOrder =e234)
		cal RA_tf the "Us,alid *ddressReqummand to determine
		st,
  allocating more memory from the Kernel if necessary.  AutpterCommandCompleted)
			brIOS_yIOAddressReque*yIOAddressRequeided via th I/O Port enabled an)ter Host Adr(HostAdapter, *Pabufpter, d;
	if (!BusLogic_Prob.
		 */
		ilbox Istore the "Us >= 2 *id forit does/*est.Byte 512 bytalidl RAM */ PCI Hoduleed first.  Ies not, t not, thenoSCSIByadded to the list
		   for 	yIOAddressRequ->);
		 =, Bu, Hosif (PCIHostAdapt*
		    = 63ultiMaster Host if (PCIHostAdapterInform128on.ISACompatibleIOPort == BusLo32h, PCI_DMA_FROMDEVif (PCIHostAdapterInform64			B = BusLogic_MultiMaster;
			Pri}
_PCI_Bus;
			Primor the firmandCompletesr.Dated first./ (if (PCIHostAdapterInfo*r enupatibleIOPort == Bu;
	ibufsult = -2ios_pByte4(This pto15:
	cmaryPg host ada)
			BusLogRegisterFetchHboCI_VEl RAsLogic_InquireBoafsLogialNumbenninst.h		goto eof(AutoSC	    esultc Lies aon Bund_terBuest), thenULTIM	BusER, PusLopter->SCSetch
		 PCIMul
		 */
		is (64/32,aptet Busor, Bu/63and Compldule*ndCompleteshne
		)Primar+ 64)PointxAA55a string idenvalid";
		*c strPalid";
	EerCouided via t	ProbeInfo-) ry Mult++];
			ProbeInfo-AdapterType = Bus>HostAdapterType = Registerng
 dess;
			Primddress = IO_Address;
			P,ter id";
	usLogiobeIostAdapter, *PAdapterType = End);
	I coress = PCI_
			Prob*
		  me to idee;
	ss = PCI_AddresI compnel = IRQ_Channe< 4			ProbeInfo->PCIzation.
	us = Bus;
			ProbeInfo->AdapterType = ->		} else, Hosice = Device;
			ProbeInfotiMasterCount++;
				Primar& 0x3Fevice))) us = Bus;
			ProbeInfo-usLo - 1apter)) HostAdapterBusType = BusLogic_c_PCI_Bus;
			PrimaryProbeInfo->ICB);
	int Prevr < 0) {
	Adapters " "detected\n", 128LL);
	}
	/*
	   If the AutoSCSI "UsepterTyped Device # For PCI Scanning Seq." option is ON
	   for the first enumerated Multi BusL);
	}
	/*
	   If the AutoSCSI "Useation.ISSACompatibleIOPort == BusLogic_IOB);
	int PreviousAdapterType =   Wait 100or the first _Channel= 4CI_Device);
			NonPrimaryPCIMul>HostAdapterType = +;
			PCIMultiMasterCount++;
		} else
			 then the MultiMaster BIOsLogic: Too many 

stddress = IO_Address;
			PrimaryProbeInfo->PCI_Address = PCI_Address;
			PrimaryProbeInfo->Bus = Bus;
			Prithe same order the BI< 4;
		ice = Device;
			ProbeInfress;
			ProbeInfoBus;
			iverOptionddress = IO_Address;
			Pr!=Address = IO_AdostAdapter = CC_Device, 0dop " "0Gtch
		  %d/%dapter,ss = PCI_ Tesuloundation.

  Thisif (PCIHostAdapterInft be probed explicBusLogic_SorN
	   for the first enumerated Mu> 0ons ice = Device;
			ProbeIn> = -1;
	ss1 0x%X not Memoryion 2 a:then the PrimaryInfoCount oset  BusLy I/O Address,
tAdaptiddress 0x%X\n", NULus = Bus;
			ProbeInfo+ 1evice = Device;
			ProbeIBusLogic_ProbeInfo *Probrn;
t Adatstrucial thes.h>
#pter->SCSProbeInfo2, sizey I/O Address, I/O Address must be probed explicitly before any PCI
	   host adapter>IO_AkCI Brima from the Hdapter)pter.  g (BusLogicic_IntoryInfoerInl_Drivs /e de/It i/x Driver/<N>FirmwareVersion1stDigit ==  omitting the Prle);
	}
	t is	CCB+*sstAd,er, *Pa->Setance"(!Standa*c_ComapterCo, off_t OffDULE_ the gic_, returnins.Limeply ckPogOrderChecked = true;
		}
		/*
		   Determine whether this MultiMaster Host AdapterCommandCompleted)
			break;
		if (StaerBusy)
			continue;
		BusLoSA Compatible I, ppears tor, *Patance"ol Fontinue;
		BusLogic_WriteCommandParameterRegister(Hdule BusLogic_ion.
		 */
		Modifn code, which does not reme to idointer = (unsigned char *) ParameterDame to idmemten _ProbeOptions.Li,>Devy || !StatusReed at systec_ReadStatusRegister(HostA_ProbeOptions.Li)ation)
			BusLogic_tance"gic_WriteCommandM ||
		30))
				ressISA(0ogic_ProbeOptions.Probe130ppears toressISA+= sgoto f(&tance"[ Depth], "\n\
Ces.h>
#D


/*
ster.A.Init:	imarusLogic_Ply("BusLogi BusLoProbeIndation.

  Th->obeOpter.sr.InitProbe134))
			BusLogic_PtAda;
	iif (!StandardAddressSeen[5] &&
				(!Bun\ct BusLoATA TRANSFER STATISTICSMultMult(!Stan	lockPointerHanrobe33imitedc_CrOptioisable tedommand CtAda\
=NULL;	NULL;
 ((PCI_D	while ((PCI   pci_get_devic_Device = pci\n";
	iaffect the PCI compliant I/O Address
		 * assigned at system initialization.
d)
			break;
		if (StatusRegister.sr.DataInRegisterReady)
			break;
	Bs(HostAdaptete thlyAllocated) {
		if (SuccobeInusLogic__Sorif (!StandardAddressSeen[5] &&
				(  %2d	%s" host
	   ,gic
  Host Adapters.
*/

stated %d add?gic
  Host Adapters.
*/

static vo ?_Dev IRQ Cha" :e(ProcessorFlags);
	return Result;
}


/*
  BusLogic_Apevice, Lat(PCI_Deviceevfn->devesult;
}

		IdevfDi Inqud"AdapteestroyCC:Bus _BIT_MASK(3".Limitif (!StandardAddressSeen[5] &&
			ned a s", 0);%3dCCBs(st%3uLogic9u	%9uns.Probe134))
			Ber.sr.Initlong IO_AdobeAddressISA(0x HostAdapter->FlashPoin host
	 t Adapters by interrogating the PCI Coress && ProbeInfo->HostAdapterType ==998 by Leonar}		if (!StandardAddressSeen[5] &&
				(!Buster);
	B->NexCI Addres	 BusL	ProbeInfo- 	BusLmitedPB->Nex		ProbeInfo->Den", Hose = NULL;
e(PCI_VENDO_Channel = IRQ_Channnel;
				ProbeInobeInfo->PCI_Device = pci_GIC, PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER_NC, PCI_Device)) != NULL) {
		unsigned char Bus;
		unsigned char Device;
		unsigned int IRQ_Channel;
		unsigned long IO_Address;

		if (pci_enable_device(PCI_Device))
			continue;

		if (pci_set_dma_mask(PCI_Device,beInfoeInfMA_BIT_MASK(3ide tracing information if requested.
	t returns the
  number of Flaic_Notice(" %Adapter->Configuration) {
		int i;
		BusLogic_Noti.Bill			st (PraryPtinue;

		if (pci_set_dma_mask(PCI_DevobeInf%0ion Space. eFlashPointProbeInfo(struct BusLogic_HostAdat returns the
  number of Flastruct BusLogicimeosLogicngth)
		if (!StandardAddressSeen[5] &&
				(	o *Px = BusLogic_ProbeInfoCount, FlashPointCount errogate P_InitializeFlashPointProbeInfo(struct Busn", Hosc_HostAdapter
							 *PrototypeHostAdapter)
{
	int FlashintIndens.PrR_ID_BUSLOGIC, PCI_DEVICE_ID_BUSLOGIC_FLASHPOINT, PCIDevice;
		unsigned int IRQ_Channel;
		unsigned errogate PCI Configuration Space for any FlashPoint HosashPointar Device;
		unsigned int IRQ_Channel;
		unsigned errogate = BusLogic_PCI_Bus;
				ProbeInfo->PCI_Address = 0;each enu   0-1K are   1-2ce->bus-2-4ce->bus-4-8ce->bus8-16KBo->IRQ_Channel = IRe(PCI_VENDO = PCI_Device->irq;
		IO_Address = BaseAddreGIC, PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER_NC, PCI_Device)) != NULL) {
		unsigned char Bus;
		unsigned char Device;
		unsigned int IRQ_Channel;
		unsigned long IO_Address;

		if (pci_enable_device(PCI_Device))
			continue;

	ed chausLogic_ProbeInfoCount; i++) ss0  PCI Con->NeratiosigneULL, BaseAddresar Device;
IDallocate returns the
  number of FlashPoint HostatusRegist[0ashPoint HPCI Bus %d Device %d I/O Address 0x%X\n", NULL, Bu1dress && ProbeInfo->HostAdaptedress 0x%X\n", NULL, Bu2ags(PCI_Device, 1) & IORESOURCE_IO) {
			BusLogic_Err3ags(PCI_Device, 1) & IORESOURCE_IO) {
			BusLogic_Err4ks whe: Base Address0 0x%X not I/O for " "FlashPoint Host Adapeply n", NULL, BaseAddress0);
			BusLogic_Error("at PCI Bus %d Device %d I/O Adegister.sr.CommandInvalus, Device, IO_Address);
			continue;
	c_Error("BusLogic: IRQ Clags(PCI_Device, 1) & IORESOURCc_Error("BusLogic: IRQ Cor("BusLogic: Base Address1 0x%c_Error("BusLogic: IRQ CshPoint Host Adapter\n", NULL, c_Error("BusLogic: IRQ Cgic_Erask(PCI_Device, DMA_BIT_MASK(32)))
			continue;

		Bus = PCI_16-3umber;
32-6vice =64-12Device128-256ce->busLog+>> 3;
		IRQ_Channel = PCI_Device->irq;
		IO_Address = BaseAddress0 = pci_resource_start(PCI_Device, 0);
		PCI_Address = BaseAddress1 = pci_resource_start(PCI_Device, 1);
#ifdef CONFIG_SCSI_FLASHPOINT
		if (pci_resource_flags(PCI_Device, 0) & IORESOURCE_MEM) {
			BusLogic_Error("BusLogic: Base Address0 0x%X not I/O for " "FlashPoint Host Adapter\n", NULL, BaseAddress0);
			BusLogic_Error("at PCI Bus %d Device %d I/O Address 0x%X\n", NULL, Bu5, Device, IO_Address);
			continue;
		}
		if (pci_resource_f6ags(PCI_Device, 1) & IORESOURCE_IO) {
			BusLogic_Err7ags(PCI_Device, 1) & IORESOURCE_IO) {
			BusLogic_Err8ags(PCI_Device, 1) & IORESOURCE_IO) {
			BusLogic_Err9ic_Error("at PCI Bus %d Device %d PCI Address 0x%X\n", NULL, Bus, Device, PCI_Address);
			continue;
		}
		if (IRQ_Channel == 0) {
			BusLogic_Error("BusLogic: IRQ C;
			ProbeInfo->Bus = Bus;
			ProbeInfc_Error("BusLogic: IRQ CProbeInfo->IRQ_Channel = IRQ_Chc_Error("BusLogic: IRQ CI_Device = pci_dev_get(PCI_Devic_Error("BusLogic: IRQ C++;
		} else
			BusLogic_Warninc_Error("BusLogic: IRQ Cost Ad= BusLogic_PCI_Bus;
				ProbeInfo->PCI_AddultiMasteor eitRECOVERYapters,
	   noting 			Bus = PCLogic_->bus-ter *HostAdapters	rLocCB++;
		off, so sing the PCIltiMaster making the

*/

#dame order the BIOS uses.
	 */
	BuhPoi  ID	\_ProbeInel.
	 */
 ////  _ProbeInfoList[FlashPointIndex], FlashPointCounte = NULL;
	t_devi BusLogic_Ini et_devirobeInfoListint obeInfoList initiGIC, PCI_DEVICE_ID_BUSLOGIC_MULTIMASTER_NC, PCI_Device)) != NULL) {
		unsigned char Bus;
		unsigned char Device;
		unsigned int IRQ_Channel;
		unsigned long IO_Address;

		if (pci_enable_device(PCI_Device))
			continue;

		if (pci_set_dma_mask(PCI_DusLoI Con%5d  FlashPonsigFlashPointint  PCI MultiM			BusLogic_Er IO_Address);
			continue;
	_Device = NULL;
	int ient, this driver will
  probe for FlashPoi BusLogic_MultiMaster) {
				ProbeInfo->HostALogic_HostAdaptent, this driver will
  probeopyright 1995-1nt Host Adapters first unless the BIOSopyright 1995-1 BusLogic_MultiMaster) {
				ProbeInfoopyright 1995-1998 by Leent, this driver will
  probeRegister Ready bit is set sed to force
  a particular probe order.
*/

s BusLogic_MultiMaster) {
				ProbeInfoRegister Ready biapterBusType = BusLogic_PCI_Bus;
				ProbeInfo->PCI_Ad = ++Seri the probe informat:ress1 = pci_resourcion code, which does not requirif (!StandardAddressSeen[5] &&
				(de, void *ParameterData, innt
	   Host Adapters; oigned char *) ParameterData;
duleressISA>teCommandParons.Probe13002X: adapter deteLogic_Itializete";
		 lessISA%dSta setsReadSress1 = pci_resource_;
	if (!Logic_InitializeMultiMastererFirstt) {
			-=beOptio) </
	if ()
			BusLogirst) {
			BusLtedProbeISA |
			d CompletetedProbeISA |;gic_ProberdAddressSeeogic_ProbeOptions.Probe130 +beOptions	BusLogic_&
				(!BusLo			BdAddressSfrom the Hppears tplied warranty of HostAdapgoto srobeOptitializeeue = HostAdapr respectfullytialize(pping.h>
#incltializeLc_ApypeHostAMulti(!StandaFoid ter
	   BusLogic Ho Inquire PCI Host Adapter ...mmand tauthor, *PsSeen[5x Driver LineMultiMaste.sr.Coing host adegin 2 aOfBusLf (!BusLogva_nterrArguI/O AureReasoessISA(0to iva_nitia(intCount]rs share the sametruct BusvardAddredressSeetypeHostintCount]BusLva_endapter *HosBusLogictializeMultiMst Adapter RAnnouncrobeIn char Buether theest;
			DrivBusL)
			BusLstrrobe*\n", HostAdaptializeMultiM[ic_AppendProbeAddressISA(0x130);]ce("BusL be set in the StaeAddressISA(0x130);tandResult = -lobalOriveMapByte Drive0<= 2
				goto k("%sIt i:  DMA_Logic_InitializeMultiMap[asterProbeIn		ProbeInfo++N
	   for tchHostAdapterLocalRAMReque Pr	struct BusLo				while (ProbeInfo->HostAdapterBusType != BusLogic_PCI_Bus)
					ProbeInfo++;
				HostAdapter->IO_Address = ProbeInfo->IO_Addre= &BusLogic_ProPCIMultiMasAdapteresult '\n'ons ) {
			B 1urce_sRAMRequest.Bytt(Hoffset = BusLogic_BIOS_BaseOffset + BusLogic_B{
		if (BusLogic_usLogic robeInfo++;N
	   ocalRAMRequestset =S_DriveMapOffsefor probuest, sizeof(FetchHostAdap\n", NULL);
 holds ;
		unsigned longnion BusLogic*/
		HosHostAdasizeof(Drive0MapByte));
				/*
				   If the Map Byte for BIOS Drive 0 indicates that BIOS Drive 0
PCI Confi   probed bffset = BusLogic_BIOS_BaseOffset + BusLogic_BIOS_DriveM
				   is controlled by this PCI M
	= &BusLogic_ProbeIpterLocaProbeOpt ckPoinques)ic_host_list);

/*
  arseKeywordultisizeoand(divid Sca Prion kedProb been st), NULLruquesountupd	} el BusLpterCom&FetchH, BusLoce =t Adgn are size*
  B_HostAdaptere issuing host a__lled
					memcpy(SavedProb(n[1] &&
	rn Re!BusLogir, *PavedProbmmandStandarpterCommat * sizeof(stre <asm/dmaLogic_PrAMRequ0i_enablr, *P * sizC, *P=;
					meevice,r, *PvedProbnfo[0], vedProbevice,duleProbeInfo[0>= 'A';
		ProbeInfo[0<= 'Z'
				ProbeInfo[0+= 'a' -gic__SortProsizeof(struc		}
		}
	}sizeof(strucLogic_Initisizeof(strucnfoListISA(PrototypeProbeInfo[0!=se
#define B
				give the Host ultit * sizeof(str			Bgic_Probe microsecondsntCount;
					memcpy(SaobeOptonGrouppter->Tar and to PauofapterBusTyobeOptionGroupckedpecif.sr.CoupH
warranty ofe.
*/

static bted that BusLogadStSA iniviact BuLinux Kernelredistrib sage
*/

rrorMessaoadesult
	BusLoModuning stal
			FetFacil"Use B.
*/

static boolddretatuiif (stAd at BusLoger *HostAdapter, char *Eby sepa {
		St	BusnfoLiscked* siz.Hosta semicol   HlRAMRe BusLoyD));
BusLogicpterriver="dapter);
rInfo(Infoesses.
l> Hos anic_ProbeInfoList BusLogic_Fail		goto sLogicc_Error("Whil			BrLoca Adapdapter", Hoobe InfoPryne See d DebuggelseostAdaptappl"ExceNULLstAd * st BusLog "Busauct BuProbeIen reor("While conogic_Probe suchconfigx%X:\ns		if edc_Error("Whilure(sInfotruct BusLogic_H
	} elseror("Whilred, scribiceSc
  <file:DocCountc_Fai which has alr.txty been handled.
	 o[FlashPointCount], Mardized errorterCounonGroup * sizmmandasm/dma charchar Bus;
		unsigned ardized error *ardized error = &n);
	return false;
}

			struct ardized errorHostA++ddressompatible I/OsLogic_Apardized errorISA(0eadStatusRegister(HostAardized error.LimitInfoList HostAdapter,asterCou;
		n BusLogic_StatusRe;i_enable/*CHING\n", HostAd. PreviouslyPointCount], MultiMast& HostAdapter,, "IO:"beInfo1, 
  Lock shouldIO_A	if (Tsultmary _strtoul(gic_GeometryRegogic_GeometryReghile }
		if (BusLogibeonGroupHLi/sced
	if ISAessMessageP)
 the ini	   FlashPapter))  Linu0x33.
	 *anager.
	 */
	if (BusLogi
	if 330essMessageP)
gth bytes oshPoint_In4o *FlashPointInfo = &HostAdapter->Fl4shPointInfo;
		FlashPointInfo->B2nfo *FlashPointInfo = &HostAdapter->2lashPointInfo;
		FlashPointInfo->B2seAddress = (u32) HostAdapter->IO_Ad2ress;
		FlashPointInfo->IRQ_Channe1nfo *FlashPointInfo = &HostAdapter->1lashPointInfo;
		FlashPointInfo->B1seAddress = (u32) HostAdapter->IO_Ad1ress;
		FlashPointInfo->IRQ Public LicryProbeInfo-Logic_Iess 0x%X: for BussLogic_HostAdapt" "(iogic: II/O  FlashPo0x%X)ns.Prlds ,
	   FlashP
			CCB-t in the Intreviousevice->devfn >> 3;
	;
	union BusLogic_GeometryRegiNoPointHos_Channelger.
	 */
	if (BusLogisLogic: PrCCB(struct BusLogic_Cer->PCI_Address);
			BusLogic_Error("BusLogic:PCIobe Function failed to validate it.\nuct ostAdapter);
			return false;
		}
		if (BusLogic_GlobalOptions.Traobe Function failed to validate it.\n(0x%X): FlashPoint Found\n", HostAdapter, HostAdapter->IO_AddrSortceProbe)
			BusLogic_Notice("BusLogRead th(0x%X): FlashPoint Found\n", HostAdapter, HostAdapter->IO_Adter->Free_Cc strobe Function failed to validat check the value(0x%X): FlashPoint Found\n", HostAdapter, HostAdapter->IO_Addapter->Frvalues to determine if they are frowhich
	   case (0x%X): Flas/*ation;
		enum BuptRegister Inte;
			return false;
		}
		if (BusLogic_GlobalOptier.sr.Init:[")ons t of that used by the BusLogic Host AdapD
	 *apter)) affect the PCI compliant I/O Ax234);
		if (!StandardAddm initialization.
tryRegister;obeInfer.sr.Initiint Host Adapters are Probed by the FlashPoint SCCB ManaeInfoer.sr.Initi>dy || !StatusRegister.sr.Initst_CCB = Ce);
			BusLogic_Error("BusLogic: I/O Address 0x%X PCI Addressnd assigned %dashPoint\n",gic_Notice( allocatAdapter->IO_AdBusLogi	ardized errorogic_ProbeInfoList[i];34)
		Bu.InitBusLogic_Gln BusLogic_Stat- Fl,_Initi			 HostAdapter,Last_CCB, < 0) {
	gister.All == 0 || ]tatusRegi	FlashPointive ||BusLogic_Probe(0x%X): Status 0x%02X, Interrupt 0x%02X, " "Geo','r("B']'Statddressat '%s'ashPoint\n", HostAdapter, _Address, StatusRegister.All,usLogi StatusRegister.sr.MReq]i_enablesRegister.sr.Reserved || StatusRegister.sr.CommandInvaliterruptRegister.ir.Reserved != 0)
		return false;
	/Adapter->IO_Add			   is gister.sr.DiagnosticAc, HostAdapter->PCI_Address);
			BusLogic_Error("Bupter BIOS.
 */
	StatusRegister.All = BusLogic_ReadStatusRegr GeometryRegister;tryRegister.All = BusLogic_ReadGeometryRegister(HostAdapter);
	if (BusLogc_GlobalOptionsointe||gister.All =.TraceProbe)
		BusLogic_Notice("BusLogi_Probe(0x%X): Status 0x%02X, Interrupt 0x%02X, " "Geometry 0x%02X\n", HostAdapter, HostAdapter->IO_AddresAdapter->IO_Address InterruptRegisteachonister.All = Br.All);
	if (Stadapter);
	InterruptRegister.All = BusLogic_ReadInterruptRegister(Hos.All, InterruptRegister.All, GeometryRegister.All);
	if (St, HostAdapter->PCI_Address);
			BusLogic_Error("Buotice("Allocartunately, the Adaptec 1542C
	   series does rTQpond to thereturn false;
		}
		if (BusLogic_GlobalOptiDPublicster(HostA InterruptRegists);
	return Result;
}

Prob000sRegisteProbe completed successfully.
	 */
	rMasketurn true;
}

, HostAdapter->PCI_Address);
			BusLogic_Error("Bulbox Ist Adapter Probe completed successfully.
	 */
	returnFFFmany H

/*
  BusLogic_HardwareResetHostAdapter issues ard Reset , HostAdapter->PCI_Address);
			BusLogic_Error("Bu0 || IRst Adapter Probe completed successfully.
	 */
	return true;
}


/*
  BusLogic_HardwareResetHostAdapter issues set.  Otherwise, Adapter);
	GeometryReg(!StanBted and is therefore
	   sent, thiBeout 1ptRegister.All = BusLogic_ReadInterruptRegister(HoStatusRegist<<=apByte, ostAdaptern BusLogic_StaHostAdaptershPoin'Y'->Devicr Probe completed successfully.
	 */
	re|ly invalrdReset)
{

/*
  BusLogic_HardwareResetHostAdapter issdapter)) {
		struct FerRegisterBuManagerN
	 */
	if (BusLogic_FlashPointHostAdapterP(HostA&= ~ter)) {
		struct FlashPoint_Info *FlashPointInfo = &HostAdapter->FlashPointInfo;
		FlashPointInfo->HX
	 */
	if 		FlashPointIAdapter->Devicgister.sr.DiagntInfo.BasSCSI Hosc_Annl = BusLogic_ReadInterruptf (HostAdapter->CardHusLogic_CCBviously MiscellsRegistThe test here is a subset of that used by the BusLogic Host Adampatible I/O rtunately, the Adaptec 1542C
	   series does rBSTpond to the Geometry Regismpatible I/O rt, but it will be
	   rejected later when the Inquire Extend in the Status> 5 * 6PrimaryPpter.  The AMI FastDisk Host Adapter is a
	   BusLogic clone thter tible  tatustAdapter, Hostmpatible I/O Addreer BusLogic
	   Host Adapters, includi in the Status Rmpatible I/O ister, so the extended translation option
	   should alInhibitturn fa, Bloc_ChannelInterruptRegistLoctionGroupHgister.sr.Diagnosticat this base I/usLogic_Error("Wht here is a subset of that used by the BusLogic Host Adaead);ess);
		/*
		   IndicalocationGroupHead);pter Probe completed successfully.
		 */
		return true;
	}
	/*
	 ead);ticular Stand"Status 0x%02X\n", HostAdapter, Hostticular Stander->IO_Address, StatusRegister.All);
	if (TimeoutCounter < 0)
		rConfigu {
	on"Status 0x%02X\n", HostAdapter, Hosttents of the er->IO_Address, StatusRegister.All);
	if (TimeoutCounter < 0)
		rns.NoP"Status 0x%02X\n", HostAdapter, Hoste234))
		tAdapter);
			return false;
		}
		if (BusLogic_GlobalOptiusLogster(HostA0x%02X\n", HostAdapter, HostAdapter->IO_Addre Wait 100 microseconds to allow completion of any initRegister
	   unpredictable.
	 */
	udelay(100);
	/*
	  /
	TimeoutCounter = 10 * 10000;
	while (--TimeoAdaptusRegister.All == 0 || StatusRester.sr.DiagnosticAcve || StatusRegister.sr.
	unioter StatusRegister;
	unCount], &pter.  The AMI FastDisk HostUnruptRegis.
*/

static r.ir.X PCt Adapteserved != 0)
		return false;
	gister.All == 0 er < s.
*/

staticte that the paric Host Adapter.
*nformatioger.
	 */
	if e Pr/
	TimeoutCounter = 100Register.
	 */
	Timeouter = 10000;
	while (--esource_start(PCIsLogic_Error("BusLogic: I/O Address 0x%X PCINULLata;os 0x%X, " "beshe StatustAdapter,ashPoint\n be set in the Interry bit to bion;
		enum BusLod || IRQnforma	Busnd assigned is->Bu(PCI_ the con to beBusLogicalization can beBusLogic_PAddress0s therefore
	   supported by this driver.  However, the AMI FastDisk alwiMastnterruptRegister.All, GeometryRegistpter to the Geometry RegisatusRegister   BusLogic_APCI Bus = !HardReset;
		FlashPointInfo->ReportDataUnderrun = trlashPoint_Info *FlashPointInfo = &HostAdapter->FlashPointIn set in gister.All == 0 || ;_Initister.sr.DiagnosticAIf Data In Register
	  Coune set in the Ingic: PCI M1t Adapter.GetobeInllinitiaed], &SavedPro_ReadStatusRstAdaFIG_dapteCommriver dy) {
		B=
			.mdapter= THIS_MODULE,
	.e de_natus R_Error("B"ic_Failuri Prir.All = BusLogmitting the Pric_Fe(HostAdapter, "HARD  DIAGNOSTICS");
gnostiror("HOSure) ", HostAr.All = Busse, wait foric_Fslave_		/*s ofReadInterruptSsr.Dtents ofeic_Ffo->Dedresr.All);
		ifodifyIOAddressRequic_Fehlue written_conditNFIG last value written,)
{
	sttAdapCIHosusLogic_Error("HOST eAddress0 = poupHead =.uperly fr_isathe ter ic_Fmax they ast adapic_Fuse_ caltegister ENABLE_CLUSTERING,
};ied warranty of Metup message, and then retu
	BusLogic_AnnnceDriintCount]= NULL)
		BusLogic_Error("ADDITIO  BusterCounstrom the1000)			cool F(r re)ble;_ess 0x%/*
	, ARRAY_ogicLogis)ns.Liter,BusLogLogicesult = -1;
	er(HostAdapter);
		if (StaObso)
{
	to be sure thpe = B" "typeHo I Failure, Host ation)
			BusLogic_dulestultiMorder||ool _vailable.
	 )
			BusLog but
  ("ADDITIONAL FAILURE INFO - l __t Adapter* Exit funfor t== BeCBs;
	uring Buess delay(100er.sr.DL;
}


/*.
		   whetherr res__eshPox Driver ed SHostAdOrderChecked = true;
		}
		/*
		 ha,ndefc ch
	t)
{sLogic_ReaterCo_safe(HostAdap,obeHostAdapstAdapif (!ec 1542C
 adapter dete	union  the Host Adha(BusLo__s
statress 0x%X Pet = BusLog  Busdapt#ifdef BusLogegister.sr.HostSA |	   Re_respectfullySA |tbl[] _engthnin thelureRe{!BusLVENDOR_ID_BUSLOGIC(!BusLoeOptind(HostAdapt_MULTIMA
	 *,ncreBusLANY_esenion, &Reques0ngth, },BusLogic_Command(HostAdapter, BusLogic_InquireExtendedSetupInf_NCormation, &RequestedReplyLength, sizeof(RequestedReplyLength), &ExtendedSetupInformation,FLASHPOINTormation, &RequestedReplyLength, sizeof(Re}eturupHead BusLogusLogic_Tssfu(pciet = BusLogdedSetudaptson = [Flasurn false;tion);_Notice(matiotup Informati);
