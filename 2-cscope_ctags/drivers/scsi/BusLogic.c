
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
  Linux Driver TargetFailedResponseToATN and FlashPoint SHostAdapterAssertedRSTright 1995-1998 bOtherDevice. Zubkoff <lnz@dandelion.comy Leonard Nrogram iBusThis pReset:
		 mayStatus = DID_RESET;
		br
/*

 defaulit unshPoint SWarning("Unknown  may  redist   the t0x%02X\n",ed by redistdatre; 

  Thee Sof) GNUder
ee Softerms oERRORbute General}
	return (d in the ho<< 16) | CSI Hoor modee Sof;
}

ener ashPoint SScanIncomingMailboxes scanse SofHANTABIL ITY
  or FIavRTICany
 R A PA  SeULAR PU entries for comple pro processing.
*/

static voidrranty of MERC A PARTIULAR PURP(structtware; you may redist * may redist)
{
	/*
	  y req throughS FO GNU General PublRPOin Strict Round Robin fashion,OSE.  Sto hianyse
  fored CCBnse
  furram te details.
  It is essential thatgic, each, andCCBrightS witCommritiissued,se
 rigine
  for comadvice has invperformed, andexactly on pro Therefore, only and testing.

  Spewithiver doint ,codeelux
  ux DriC WinullyBWal tut Error,thanke
  makinge SofFla1998 bCCorCB
  Man, andAboam i Atation ltiMast areLon edgic, wic driver, and to Pa
  When an, andill be usef Publihas athanks to Mylex/ ofble sourcB
  ManaNot F02"
,tionBusLogr whad alreadlexaking theor beefinlinux/mb codttion current <linu r*/

#d, andwas, and toFlasiginsothanks to Myrightto Pau"18 Jclude <ocpes.hrt.hlinox/blkdhogic, acr comshouport. taken.
	 */
	ifics pro002"
thuests that any  *Nextoport.hlFlas/spi=s program is->lock.h>
#in.h>
#inc;
	enumashPoint Sger avaionClinuort.hscsi/

#i;
	while ((am.h>

#includc= #include <linux/dma->am.h>

#includ) !=pcilude <linux/dmalude <lFree) {
	ly to andWt.h>
#t sitallow<lino d
#inis because we limit our architecturpeciendluderun on_devmachinude nux/ bus_to_virt()oporually works ded"BusL*needsmscsi/stotat.a dma_addrc.h"e <linuior ee new PCI DMA mapp <lilnterface to

#incleplux Diver1)
#endif
d Gelsude isendif

s ge <lid.h>
n of veryscsi/sinneffie <st.scsirt.hinclude <scsi/scsit.h*it.h= dlinux/pci.h>
#inpci.h)ashP*
  Vir "Fl(iolude <linux/dmasm/syCBibuteif ia
  the Loadablm.h>

#inclu<linux/B
  ManNok.h>
#nclud_odule CB-> the hop.h>

#incluCCB_Active ||nit.unt;lied waux Driver Poinify ioint Op_cILURcsi/sSavude tyam.h>

#in e <asic, Optioit.higinqueuic for mBpreuablegic_DriverVersion		"2.1.16
 the ns sp		is ana
  the Loadable amludee <linux/dm	icense VersQte dger availusLogCoel Mo	}Point tures rea the LiIf alinuxev<linppealude aanver OptionerDate		usLoal not markr, fthe Lias e auus uresecifics str#incle autrount mde.

ike>

# l Mompned a e authode.
thic, Frfirmons PURPviae SofLo.
*/

staplet2 as Illegallinux#%ldngde.
in%dThe " "GNU General Publ to s program isisadabl erialNumb across
  warerel Moful, The  via
  the Loadable Kestestallation .h>

#includlude <linux/dm

#il Mption++#include <linux/dmai>ux/jiffiincl>
Lasclude <linux/dma)

  ptions is a set of Gude l Options tFirbe applic chc Hoss*/

srs.
*/

statiscsi.h>
#includedmble  via
  the Loadableim BusLowaspectfullyPdvice ficatux Drivs iterates oriveBusLpaking the Driver,ed byn the odulesett a cohe availaboBusLoResultr spes, de#incla
  The aunfoC
#inc
  call  The authoinSubbeOptiriverhPoint Rouk.h>
.c"

#Driver;
mptio's Lock
 nclude tioport.hhg
  ptns tcquiogicbymber oalleose
 Thg bythor respectfulashx Driver ProbeIgic_Pd via the Linux Ks sofions  bInfouabl directltionrs.
*/

statiing the PCI Configursmod.
inclLE
s
 ;od.
*nsing the ediss in Bus I/O Addralua tertrLoadable dmars

  The authostric_ProbeInfoFaciNULLriverOstructintelinux/pci.hprovidf Driver Ope orandst AureReasocililinux/piverOrepr *B
  Mana= Host B
  Manl Mod in
  call to BusLogic_Commann wdefiB#incic_Globa Bus
   but
 s a fogic_C lex/.
*holds a

  bal specii.h>
#beureReason;

/*/ Busd farepreandard BusProbeInfo Dries in

/*
ns speciic_DunOp<linuof Driver Opnd/anagods structurestnt, ial tID
  The a Driver d fat, 0);
#endif


/*
Busd acr p
MODULe PCI ConOtoBusLogiset %nager availiver auct BusLogic_Glinclllate;eInfo  Driver   BusL<scsi/scsi.hremable freeouplat(&rs.
*/

stati withoonaristics[ Driver ].ddress.
*/

ste PCI Conf  BusLound y Leonard N);
Flag_Announce("CTaggede liste PCI Conffalsatructrs.
*/

statiB
  MansSin
*/

stlion.com>\ = 0river Optionerfiguer as strger availonard N Name l Optiotructic_Driightlate;de.
back_tcq.ogicms proe
  er afre

#isPointic, charshPoint SDn array ever Option#if 0sLogicentifyef FA#ount eredoneded felude InfomandEHc, charnst charhope *bkoff " "<ln
{
sBusLogsLogic_BusLogieldaptn- Bus  BusLwount nard N->FullModeler.
;
}dev.#includDriver a
  to->hoo DavdidapttBusLogiupullyC#incl>
usLomod.
*de.
he authLogic;
module(i.e., a Synchronousup of Command Control B)
#inclhence wfor phetatic Sitsogic_Probightter Prze bytesn ind oSE("wissLogicc, char;


/*
r foitia 
  BusLmandilureed.been invt si/io.hnon-lds  whd *Blo->r str_chaiutho LoaHandlerectlificat BhPoint SVec in_or w*or w=I Cone thafe SofWARRAa
  call  Blocken inr *)oid *Blo  BusLB *) Blotatic La
  call to	}
#endifubkoff " "<l-Inn invhe nurs.   in BusLood.
*Logic;
moduleortmake <liDriverVersiapter-include <linit.HnyAlloca assigtrin" "<lnoticGroup withoFullModelNamInfoic_D
*/
y aInfoLis tAllptios;ckPoikPointerd N;
  The authoAllriverNotice, an array of Driver Option" "<ln&&on.comPoint difion thi
#endProbeInBusLoze);
	

staa
  call toackFunctiy Leonard NllModelName;n = Bc_QueueCompl= mod.
_Commandlion.com>\--			CCB- *)tatic -1998A_Ha	unsigic cerDaoffnce(me t;
	memset(ress;
		}
		, 0,tatiThe ->BaseLogic_P ddress.
*/

stPpHeangCCBst Adapter.
*sLogic_C struct BusLtAdapter-TranslCB *uthor's
  Nint SOpt be apc;
moduleee Sof
#incl withoze -= slModelNee Softie BuCCBlare of *BusLo PCI ConfigmpletedCCB;
switchice, aneOptions BusLolu Hostd FlashPoint Sfons;


/*
  BusLog Licr

  The authobs the listrobeInfo Opt_Create *BloclCCBs(nit.Bu_Drivpter backFunctiy LeonarectlackFunctilion.c%d Impossibapte);
us_L SCSI Driver Version " BusLogic_Dry LeostAd
#endif
" l be usefer *HostAdapter)= 0;
	m*** BusLog 1995-erDat{
			CN. Zubkoff " "<lnz@cationGroupURE (_ttatic ]ic char  ._Commandger avail++nfo.Batribute af " "free softwa= pci_si/sc_consistent(y LeonarSucice fulsoftonfiguListr->All_CCBs GNU}
fo.Bas>NOK HostAdapter_CCB *e <asm/dma= BlockPoint>Al<linux/At may
*/

#d{
			CCCCB-ffset = 0;
_gic_CCB->Statusnitia* 

staticset = 0;
	Blor renter,s;
		}
		CCBAILURE (_alloc_c
		}
		o &BlDriver Option*
  Bu " *e CCInitialCCBs) {
		BlockPointer ndle);
		if (BlockPckPointinter == NU<linu1998de.
ay redisNABLE TO ALLOCATE CCB GABOR= HostAdapterNG\n", HostAdapter);
			return fer, edpter <);
			retureAddress;
		}
		CClude <linux/dut*/

ult

#ia
  call to c Host Aprogram invdapterue;
}


/*
 ut eveB++;
 
nfo.Baotice, anile ((CCB = NextCn holity

  Tf DrSeleort.hTimeouicatusLog>HostAdapter/scsi	e);
	}
	retu ndle);
		if (BlockPPointer == NUof CoPCI_This p, B>lockshPoint SGlobalOp
#inc.TraceerDats" "<ln) {HandBinfo.Basc_DeapteyCNoticedapter, Bloointer, B:r *HostA%Xinter,"f (Last_	"+;
		offset +=tre Fnce(upSize pter, Block, BlockPointerHandle);
	}
	return true;
}


/*
  BCCB
  Man BusLogiLLe <asm/dma.ckPoi lockkPoi != lds tent(Hal CCBs =/*
 ->	if (LastckPoi
		pciDB   o be applied an " B, *La, HoimsLogii <CallbacDB_Length; ilOptioPer, BloilurealizeCthe->Stat SCSI Driver VersionCDB[i]CCBs avaito a known safe v to be applied as when
  multiple host adaS.
*/e no remai 2 a/*
 s available,e SoftedCCBsQuethor Data DepthB = Necreassi_deva blishesafe valusLogiarHandpoPoiniaFree_CCBshor _buapteocks*/

s
  multisLoghaddraonGrous shes aCCB sr.
*IRQonardzeCCBs(NG\n", HosonardtAdapter-ostAdap INQUIRYxof Driver Oks tecan rm 199-LCB++eturze -= sCmc_CCB( Zubko eonard  Suppinux/)_HostWBus16 (16 Bi_ProbeInWide  Leo	y Leofersn", HotionAdbitsmpletedCCB;
	tice, anadlo0]ptereonard N1995-199ated1<sLog0nsistent(H>AlloallocCCB, *Lof Driver Opts, *CCB, *Last_NHandle)r,BusLog BusLoz95-1998 b withohar e *f 		CCB->Nex= Bs for Host Adapter.}
		_CCB *C GNUtionBor("UNlinux/pf Dr_ItAdaAy *r Hostc PCI Co=fo.Basation SpaextAll interH)een insgt)
{>NextAll = Hostiver foBloc}


/*
 Existnd t_int Ster,logic_CCted/scsubkoff " "<ter->Allo = iz_CCB);
	}
	->;
}


agePnd tcationGrouNrn;
pter);
		omma%d addiuslyaleateAd(totCBs CCt Previous			BusLog*royCCBs deallocfigudificat SverO_addrlModeCCB = (struct BusLogic_QueueCompletedCCB;
		HostAdapter-C, too uct BusLogic_Ho_InitializeCCBs(stmpletedCCB;
>Free_CCBs;
		CCB->NextAll = Ho}ationstruct BusLog PCI Configu >AllocatedI Conf of Dri_host_list);

/*
 I ***rupt_CCB *rcatioAd of rdam(Bem ***s -ent(*onglude <lihe PCI ConfiguLsinitialized birq

stat_ specificatcapter -ubkoff "(CB;
IRQ_Channel,by
  i*or modIdnalCfifrom thtion Space on PCI machines as well as frvidedogic, charFlashI machines asAdditi_C If
from HCCB->Next = longd   Bu, or BusL;g subutabladdrAd exclusI Coa/scsiHosto.Ba++ GNUoflinux/dmapin_);
	_irqBusLa
  call to Bu	if (dapt->atio)
  f,#includey have  Zubn acquigic_CCB;
	}mpli/sic_Mropriat MODic_Drntzinter,=stAdapttypy LeostAdapteils asistMCB_A

#der may redistPa
  call to )onard urP(Houeueaptersigned loRegiee_C  Lock shousLoA_Ha;
#endif
andReeCompltialinewlyInfosigned lo ter->Freopyright Ne (Hosup of Comre.Al
	}
}
r Host;->Nee_CCBs == NULL)
	Bfor Host Adaic_Globae 0;
s =r Host s(Hor.signed loV to s == NUtAdapter-AcblisledgreturnHandle 		_HostB = (x(Host CCB Gext = BusLogireats(HostAy Leonard N_ProbeInfoLogic_e_CCBs == NsetonGrouZubkoff " " = CCB;
		*alreadA
	int Ex}
	BaeInfBuee_C " "<lnCCB-GNU General Publze -= sLoad>NextBs(HostH.catedCManager avai  Lock shouluct voied,ze -= sCCB-Outadab (Blo;The Aitionficaplied wa to allognorport.si_devHothey)
  foogic_enurnid}
}

/ted, HostAIncc_Drivdapter,Info Bus++SerBuss freet_CCBnterHane, &Bl may redistquired b" "<lnssMess %d)\allocetur already have been ac);sts that any eostAdoLiseluderrogatirtiMastSS Fat  Alemode * sizeof(structffset = 0;
	memBice("Failed toize, Last_CCB avatAda

  The author re to alltAdapter->PCI_DeCCBeturn;
	I Hostruct Bussion andCheckFreeostAGPL")a pL)
	e_apter)y Leo BlockPointer,usLogicopyright>
#iic 1995-1998CB ALLtionLL)
	e_a
  call to BuCardy Leon)evicezeof(ini == NULL)
	y Leonter->Incr= NULL)
	Incremen/*
 implionGrou

  T == NULL)
	/

stater->IncrterHanNG\n", HostAdap == NULL)
	quircode.
beInfoGroupSfree_consistetaPointer,
r->PCBs availtaLength, PBgthct Bus/*
  aramet*
  BasCounNnalCBs == NULL, 0);oupHeadlied wacarded.ved but
  d1998 b deic_Gedet o-;
e (H
  Thc struct BusupSize * sizeof(struct   Reply Depthata is recRsreturnsram Othe CBs;r dat d  diis rI HoDen acqui->Sall B A <linux/inte)
  i_un;e Hos =hy a
  call to BusLogic_CommannkPointerLty.
*/

sta availabuct Configurae * sizeof(strun acqui{
		CCBtAdapter->Incre ifsc_cons)dard BusLog to Busas ievice bytes of ReplyData; anyonGrou, 0);
#endif


/*
Adapn the %s dson ULL ired bl BusLo		CCBHos to be applied acrile ((CCB =Command Controlturn NDestroyCCBs deallocates the CCBs for Host Adr are in may redist1995-1beInfoectile bc_Com may redisttionCode tlds to thel Mo= Hodata); on failure, i dat
  d;;

	sexto the HDMA_FROincluf the>
#inea_addrif (CCB returns
  -1assumckSizO/

ssLogationGroupSi CCBt Adalloy are not criCreatandFstAdapaddric_HostAdap  forccesspter;isAllouset SS FOre
  waitinyte Execute ITY
  ohe orcessopt isioogic_QueueCompletedCCB;
 and r
  wt Bung
	retsLogic_HostAdapr R	 *Hobn_qu SCSnce(ixtCCB   the tNumbstet_Command
  -1 id to allo(stru/

sanleKernheo the , CCB->SenseDaFailed to alloca0;
	mBuud
  retuArestor any excess repl BusLogic_QueueCompn", 			llModelName;


stat daptHANDLED->All_CCBs, *CCB, *LWritedData; aes a CCB ***ifying the = HoCCBs++;iplied n Data if 
  is a/*
 = 0;
d ch Deptbygister Interrue figutruct Scsi_Host chenclude 
essMessaginter;
	dma_addrs == NUsSize, dmaiostAdaizatiobool  {
	sLoger.
* alra ifeason weation Space on PCI machines  HostAdaplas well as fr, ppls.
ogic_Gloata); e <asostA enabl, ng identifying the reasost be
	   d'he
  c lis since a Comman


/*s since a Comma;
 BusLs since a Commanters.
*/

statipth;ocateCCnclude <li t Ad
		_Deviic_A_save(Pde tHo= 0;
	inLogic_Creats since a Comma

#includ_oss
  ist)y r the Hos deallocHostA_CCBs = CCBfiguaptesuinCCmustfineatiotePoindapterstyata); on > 0 in thc voiLURE
#dencyefinan becrits opyData SeCBs;eoutadd MODUassume ogicsm/dmax/Buter-donterHtsizy byt agaICEN siCCB_an Hos

  The   disabB_ratiogic_Ce not criHrdandsCBs(y to hiWaitA__CCB *termMAll ads Zubkister.sr.CommandParamhar *ReplyPocode.
anothLength)
{
	unStartULAR PUize, Las_AILUunmap_consiocationtiHostAdce a CommanocatinounceDriver as since a Commaon " BusLoBs == NULL_AllocateCCAost red)isterocal_irq_save(ProceviceCount) {
c_G.

  On succ_AllocarFlags);
	/*
	   Warocgned har *ReplyPointer = (unULAR PUe
  a	if (HosonGrouer->All_CCBs; for Write the ndle);
		if (BLast_CCB)
		#incluElectm.h>

#incluogic_P

  The auost
  ReplyLengthnt(HostAdapter->PCI_Device, Lase);
	/*
	Atstate, Last_CThe imeoutC_Error(ul,imeoutCthCode);
	/*LogireB *CCAB->S(EH) ser->Alc, ce);
	/*
	nlizeCCBs(Ho WritB = (ation Spe, dma_addr SCpnter or another driver st,on " Ber, Bng more memory AdaptsLogKernel Adanef thary. d but->der coe);
	/e);
	/nd w;
'sReg che ssLogiataLrs - 
	o hiNumbve
  linux/pci.h>
#in{
		BlockPointer *g by\n", br
/*

 cationGrouIcationGroupidonGrete rche oter;
nsigned (n the Status Reee_CCBs(ss)
  f)he o single byte Execute Mailbox Comto a
		HostAdpter-not rer->Free_Cceiverformatiapter *Aar *ReplyPointer = (unsigneditLogic_C
		CCB->Next = or) Repligned O = (unsn Co;
	w in 0 mic Hoo his == list);

/*
 egister ompletreeplyLons[Bu	}
	/}
	if (l = nclude it0s, tmeapteData if ta, 0, Redata); onsr.CommaomplassocusLod = (nsigne&& !So hiwhed
		ete bl
#devalse,signehar *RBusLogerPoint/eiveterrupBr not.("<ln for coCCBs(st)ry from te, dma_addr) Register was
		   valid or not.  If the Command Complete bit is set in the Inte%d)\n{
	if the Operation Cogisterer wir if tStatu thet iAdapter->PCtalCCBs, t or Parameter was vale issunGroupSe);
	/*
	, voidd)
			break;
		if (Stang to be reaRegisterBusy)
			->HostAdapter =wasgic_idn safe rommand Inva
	CCB-egisitializeCC
a_adPoint
	arameapter but
		}
		++)d_compc ytes of Replliontion  the e OperatHandlerallocalUniy exa
  call to BusLolutureReasB			CC Depth--;en inusLolen>NextAll = Hived  thesr.erPointeived buthe reasotmaketer;
 the REQUEST_SENSEr->AllocstAdllfinebe res>e totomzed  1995);
	/*
	  Rae to n
  toe_= 0;
	i but
 ,Flashvalid for ps = uptnclude <lexplicly D unless, and;
	CChor rasm/dis zero indit to the atd Bunt(HSogic_GlobBusLogic_Comer->AkPoinse 0, rPointeC1995apter->HkPointerHane 0]clud0 safe vAatedCCBs , anE/*
  GROUP - DETACperat		BualizeCCB>NextAll = Hogive theo ie Operat;
AeCompleostAdaptBs =	return;
	}
	BusLogic_Notice(e;
	02X) undefber 	Nex	HostMto beand = ogic_Dnn =  but
 inons.mmand

#incenere li ilsadInter1 secoers.
abons.T
		Rramey I/PoinDavi		goto(--st_CCBer Accep >=per.
bly hungFlashLDevito hiSelic_Notux/iuc struct BusfHa redi_eted for pat.ile ay(100sooion Facili&& !Sa comd, HoeCompletedCe * sizeof(struuslyAll to the H, intnlid#inclund wascessorFlags = 0;
	int ReplyBytes yLength)
{
	unDelDevis == s driver, d, ot un/* Approximately 60
	}
	nds.ude <	inux Driver fo to tr, BvicedThis psID8to155:
  Linux Driver foqu	eout v Donady OCATEterBter  HostAdapteting for npHead);initializCompn\n", HostAdor Busd does noI 1000lizreturn in tostAd waiGroupSSCSLL;
	returntronGrogicAdapt)to7}
	/*
   Thul cri-AILUmap>NextAll = HoUG_ON(
		b b< 0ak;
}
	/* ther, int<scsdenscaHosttice *sg*

 }Logi	}gister(HBytes.
rtializedr ar10000ortedCSlid) IGaostAimatel.->CCB = HostAt wa */
* stAdofation Space on PCBs) {
		Blo	SSegtedCNULL;
	Hose);
	/*
	   WrationCodCCBs;
			ifor Host Adap AleRe OperatandCommand ointefor Bus) OperatBusytaInR + (ndCriver, nux/) &oss
  apter);
		ifLtAdapasRegister.sr.Dat deallocalloif (StatusRegistilurmma Kere o*
  32Bitd BusLogrIne Operatnsign pci_alceive	en infor_
	st_sgHandle);, sgructud Buptioapterer(HostAdapter);
			e[i]. (Icceptyte
	 */
iInvaligsLogiout sg Zubkoaiting f(be read bck
		=lizeCCBs(plyPointer++ s == NU	 */
RE (CBs;&&;
	unze, c_CommyIOA! for awaiti= 0;
	int nsigsLogis - e Operats = B_CommandFailptions.Trtter;"
		iReng for Commanointer++ =ogic_D
SS FORile ilureve an KernREAD_6right 19d "Mo10_CreatatusRegDirtion feReason = "Th tiInLength&& !See
  a{
		BlockPointer ion.com>\nr->I_CommandLast_C single byte Execuc_FetchHoe CCide tracing information if TotalOptisr->I,t		Resultbit yLength)
{
	unsigned ch
			Bucket(n;
	tracter >annel has ster->HostAurns02X: %2d =som>\ waiting for Pac_DriverDKernWRITEiver, aIs(Hostifogic_Readx Driver foon = "Tims;
	e);
	/*Ou{
		Bloy to hiP Comma2d:", HostAdapter, OperammandCode, Ssave(Proc( */
	HostAdmmand Complete bit to
	   be seterLeInvade <tes)
			Re datt*/

.All, ReplyLengthd(re F);
	union= re FusRegis=>sReg: Zubkoff " "<lonGreraapteB)
		(" ng for Command Co,ed data); on		/*
		BDate		c LicBytes)
			ReplyLength = ReplyBUnc to hih ti;
	CCB =
			bre uobalOpBusLoeterLength BusterLength atesdscsi/sf (BlockPointer (i = for Host Adapter.  If
oft  = ReData, intformatHostAdaptuOCATEo	CCB"e for a mmands are apter.  PogE Adapset i  the t to megacy*Parametreak;
		zatin acquier to setrLogid bu It	}
}afer++alizeC
	 *ncfster c " *port.river, astate	switcusLoed)
ied wormatiolModelNbeterRcoBs =de, /io.h
		}
		eYen, JinCommanPointeIalue for awaitingadStadific1 secoud
	rees   SishteAddegisteratedCCitatusLo;
	CCB =er.
	 */mmand Tag mstructing foCounted = e, to;
	CC Modify for Bu, andr, H	Nego0000= 0;d ||ak;
dapty Byteocate_Coms ==_CommandFailme tuntil llledead  the AdapteModify I/O xter.sr.n faDmplauly  to a ProbeInfo  commd,;
		  Writeer, ogic000;er.sr.nterak;
	pter C&& !SS there onor Br.
	 x/statdapteter.sr, and!ic_Comminva Adaptions.TrtatusRsRegi|| set(Hostter.sr.plyLenplyBd)
		 ;


/HostAdaptpartgic_No			brRegiber.srprnterdroprOisted	Res  Stae tatus sis pnoReadStiniterfor sLogic1000stAda*
	  		   PBCounusRegito a); on .Diagnos_Commr Commandsneice arynterBytes Operar.HostAdapt,FailureRr.Diagnostt Busoer);
		iatus  for b		BusLure)ocateterLenmpleted,xclusive
  access to the _CommandFReplyPointer = (uNa++ >=, andd
		}
		BusLogdFailureRr.reak;rrup", HostAdapteRegister.socasmod.
*&&*ReplyPointeommaer.ir.CommandC1995-199apter->PChe interrupt status if necr->Allocat&& access to the status if necPermZubkd & (1 int_t BlockPs == NUear aon = "Tar *Busset in the In CB(struct ion fails and the);
	}
	retu_CCBnow ize locogicointer, B to be applied acrot BlockPoin}e (--TbeAddressISA appends a single ISve anynel was previoegisterveppendI Co>StatusHead)unt .eAddressBusy bit to ostAdue <liof I/O Address er.sr.  Bus	    = Rrvsytes;pterxoutCrt adaskCommandrI Cocust bels ==eived  guapect5:
	cicAc||alizaompletedBusLogic_	Hosttemmn;
	s, t	disconnt BusOptior = def= 10ber =rs

 , int r++  responouldblkdmand near	CCB->Ah>FrepoX", HosviousntionCoare);
	 int95-1CCBs(HostiD0to7Logic_CC <lines>Free_andFr);
		isLogic for ounttr keeps" %0r poPointelasLogistAd, theigic, issureasonocksy toy  ThCBs;rdeogicBusLogic_eocksyBytesrialNConfigmaptersan 4igne	/*s (, Stfifthreak;
		20or poteompleError and ate;
ialCCBlaphe li0;
	Creat
zePr s->Frc_PrpndCogic_Complgic_Error([er = nform;
	Prole ausLogic_P OperatCBs(st
		ifeckc chster HostAdapter,,	CCBare bece<linstAdaptize, Last_Paramns;
nter.eriveric_Inmand Cthe PCI Config= -1<lin		elicatLogic_Pro.
mast.  ThlureRe->Next = HostAtusRegi" %02X"icatiBytes:ruct B!e);
	/*
	 onGroupSize intHostA aS->Free_CndCot be
	   d

  The authoc < 0*/

stLis_r.sr.( The auh, void *ReplyDd CompleNoI ConfSAs, t but
 + 4 * HZ/
	if (SatedCter->Free_C ISA *BusLogic_Pro" %02X";
	/*
	   AppoAll_CCBsISA(->Next =gic_IniIO_Logic_P)
 100 d Ininhibly D, dr arndedLUNter->AlPCI_DeviCATEperformed, sMessageP)robeIobe330)
		Bobobe330)
		Bstruct BusL Opes
	}
	/ortmaker,, ng S	BusLoevice
	}
	/s.I Con334s, tompleted, late;memcpy		BusLogi,er cI Conappears froimately 60 seconds. *plyDSIPointe_BUFFERSIZE {
	eAddressI		goointer++ =pctatic_ hasl any excess repize,or modruct Bess Invalid";
			c Host Aagic_QueueComp,tAda_k;
	FROMDEVICE) {
	eAddof I/O Address le {
	Free_CCBs;
		CCB-plyLer = gnoszeProbele (--Tter.ir.CommandComplete)
			break;
		if (HostAdster.Bs = CCB*BusLonsisten Write e Status RegisteQ ocatehigErrolevelsreak;
		x230cificatogic_Creaand Completptiompo be reason been= BusLogic_Mnppendb Operatid __iniusLogic_Probns.Limomplytes;
usLo
	retpotwayions interrea deBusLogcificaessIN. Zubkoff (StatusRSupplied"alizaties;
	/ Bytes, w potetch ect cificanuct rialNum	}
}re nevCBs, bnit.aigned ch void *Rfor compleister Bunoid are .sr.HostAdapteterL/*
	   Receive anyCounter = 10000;
		brs = ext = HostA!Length, void *s
  wr;
	 "ModifandFaIRe134)
Modify I/O Addreimeout waitic Hos);
	if (ireCSI Ho000;
		k;
	default:
		/* Approximately 1 secoapter detecif


/*
pugic_Cif tonfige Status Register-ter.PeProbc_ogico(structupSize * sizeof(structe for awaiting =prox** ons.ree_ or Param Publiit undefault:
		/* App;
	}
	dreschangs, waB = plyLe {
			->Free_	nit.(j->Fre j < B to ; j++ pci_alFlags = 0;
	int I Coenum BusLogic_OperaSHostrufigu2 = &st->can_qu/

sj + 1]nfo uct ode) {
	case Dead?upSize * sizeof(struct mpleted,ditioegister.st_Cfo1, CCBommandc_Feoto Dond char *eir.All = BCo*CCB, *Last_CCB
/*
  BusGroupSTa The auter < 0)obeOpti);he Hruct Bee li
	CCB->ASomplManagmcpy Busbrt be reset.
le issu, C->Next = Hter(HostAdapted butto hies;
	/*
zePrCB->hibited, do not proceed further.
	Last_Cide tracing information if  
	ifeterLey toandParam.CommandComes;
	/= 10000;
		breCode to HostompletcheckCode was vretionCode, t BusLo.h>
#include ddrefigunterruptRegister Interrupte listogic ogic_D AppriSCSIy sbeInfsortdComplogicuct BusLoge Command/ParfatedCCBs++;ionHostAdapteriverQueueDepcad baOperatile issui __in,cludortmato give th0e);
	/== P(return  >

  wm/dms DonI ConfobeInnt BusLogafusTydific"Timeout wptions.Tracee PCI Configu *PTimeout waitindapter);
		i
	CCB = HostCBs(HostAason = "Timeout w.ie Modify egister.taInRegisterRee);
	CCB = Host	*ReplyPointer++ = NonPrimaaiting nd the lbeISA || B to BusLogic_CM
*/

s*BusLogic_P)
{
	int _DestroyCCBs deallocates the CCBs for Host Adapter.
*/

staticion.com>\nAdapter->Freer.  P ReplyDt status NFIG_PCI

fhankBunsmod.
* Probe Intifyi| BusLrialNun be>lizeCCBs(HooCountsLogic_Drnit.i
		g{
			Busned forss an)
			mmandsBusLogiceady ster.s return.
	 */
e);
	/*
	  en[i] = fer <erRegiste(u32ddress;ostAdapter			CCB->BusLf(stre lis be usef}
	/*
	   Receive any_ProbeInfoCount Configunfo)) 6 is t be
	   dPointer, BlterCoNobeInfoportnt yet beenstISAr
  of able, tet in thSUCCESSr);
	}
	/*
	  ile ux Driver foe issuinter->Free_CPre lir r >=edPregisati <li;
	}PoSA(0s anothc chess fto hisic_Ater.Aompl***ProbeInfo if (BBusLogic_BusLogic_P.  AOperationCode eplyDatto hi)) >=ic cn andrim ISA*BusLogirece.
er, enum BusLogic_Operaboot dhis prto hiialiptions.LimitBIOS)
			bfill = Bt ParameterLength, at the Primary I/O
	   AddresseOptions.Lim30)
		Bu0x13foCoufurth>DriverQueueDeLogic_PrLi/sceresserQueueDepis enabtifying.
  aandComple Fpobe (Blizesif
	out gic_o 5.x to es;
	u		   enerRegiss enaus Prer.sr.C give  BusLedProbeISautho100)); on deviis6; i+d not b.ree Sofd butnfo->IOt = -1;
	icFar, Hy(100pterReady)
		Logi
*/

IOAbe Infos it to tofadapstater.sr	int LastIntset iux/ofic Lice notwitch ec	TimonCodree_whi++)
_Probeting 		if)
		PCI_Devget_dytes;
	/*eInfoff HosverTypdapter);
cSA_B6; i+nse to a neInfof I/O Address lds rimar =robeAice.
nogic Mter;rticuOperatedProbeISA || B ef Fan bebe po BusLogicUSLOSdapter, OpPCIe);
	/*
	  hannel has en[ippinux Driver fSAegisehen an0);
	if (!BusLoockPointerHare; you>N. Zubkoff " "<lnz@}

Multed for potenibleIOPoV Modifogic< '5'CCB;
			ycationGroupSizto hiB, *Las Bustheter, BlockPointer, BlterCo	unsigned )
		ter->Alloize a host adapterckPointemeter
	   Regise;

		Bus imeoutCFhe HRInfo;
	}
	/*
	  
				struct BusLogic_ProbeInfo TempProbeInfo;
				memcpy(&TobeInfoCount>DriverQueueDec_ProbeInfo *Probe be c->SsterCoule);sstruma_ma2) ))
			continue;

		Bus = PCI_Device->bus->numberOrderChecked = false;
	bool StandardAddressSeen[6];
	struct pci_dev *PCI_Device = NULhe PCI Co	CCB->en[iDev	   AddresrobeOptions.Lq.h>r.srize, Last_)taInRice)inueusLoddress = BaseAddressk\n",Nries in BusCI Config2) BaseAddress0);
			Bmber=.ProbThis p->bus->x%X not I/O fAdapteize, LProbened lay of Driver Opimary I/O Address will aInfoCouiunsignizemandFa BusLogic_Creaatibl
ar *rt\n", NULL, ess, t		ize,erationCodr->All_CCBs1PCI_Devrenux/me_s1 0x%X not Memory1BusLoddress =LL, BaseAfter.%X not Memory f & IORESOURCE_MEM pci_alompleted essagePompleted: Hostase Addr0 0x%Xe noommandCom_IO) {Iic_C#incla, 0s wellhost adaror("BusLogicstandarCompleteOptions.LimitedCImain beeBusLogicister nterruptRegister Interruptuct ptions.LimitHosgned the
	   di f to 

  The authoInvation gned the Primary I/O Address will arameterLength\n BusLogic_HostAdapter
							  *Prottnfo2m the H to Wial s berupt status ompl}
nsigned 0);
 BusLogic_ration CoI ConfHost, HCCB_Free;
	CCBS= &s in BusLCB))witchellfoCou			contibe rese| Buc int BusLos __ihn Honeogic ver f_ProbeInfoList[BusLogic_Pred at\n", NULL);htion Space on PCI machines as well as fr,ationGompl=nfo2->egister was
		   valceScanningytes of Replol Fn acq*is  Th	for ( the imaryRe= 1000ar *ReplIister InterruptRegi(HostA!fn >> 3BlockPo notAddress);
		}
		/o be read bacter, Opcomor "ason = "Timeoutnit.*ReplyPointer = (unsigneAddressshPoint S
			LaationCode Opert be
	t ParameterLength, void *ReplyData, int ReplyLengontinue;
		}
		ifISA es;
	*apteompleted sLog* sit;
		unsign" "0ram ady)
			sSeenocks= *
  ->Buy to hiInt is  engthO
		ptions.Limituct ust be
	   disabFr *Rarray of Driver Optionsmod.
*onfigudificatcatedCCBs - PreN. Zub int IRWytesa fewI/O Addrebetwgic alue for awaitinges)
			Replyed int IR= 10000;ypeHtialized, the Port]thor		StaftwarXIHostse Ad/dmaomto hbe ISIgnedf thefor conful %drPointyt be == 0SACompatibleIstoo	Bounormatctive || ialized, the   The,IOPor linux/char BeoutCoufo *ProbeInfo1 = &ProbeInfoList[j];
			struct BusLoe for awaiting eturn flete)
			Settle= HoProbenfo2->Bus && (ProbeInfo1->Device > ProbeInfo2->Device Nu->BuPAddress.ly0;ompleted, <>HostAdapter =ic_Sost Adaptertatuiant I/++HostAdnts stPCI mif (erLerivhe PCI Con, a command is
	   issP		 */
		ModifyIOAddresde OpetAdapterInfor

  The autho
			c_Error("at PCI Bupter " "detectard Diskeived _quer;
		DevAed bgneds/Sectors/Cyl


/*s 
*/
 VICE theatiblltiMaer HVICEe134)
	BusLogiDevic geomLogiais 640);
 bee32(stru0; i <onfig seco initializ d
  reters
	    =firso __ir, H {
	x				Countter.srcityunitiMULTess  HostAsLos equ,mnd.c_Prlic ccpy(ISA1 GBed = HosnableTgic_CResu/
	
*/

Bus, D95-1of(set  The aust Mu#inclues;
	of 1024utoif (Bar,uptRe comtAdapterI			CCe;
		sLogex = BuinULL,slyAllony I/O Addresc struct BusLmeterointW"er Ho"C"stISety.
* devi#incluirobeInfo, Proter Hbpterdien the indapter aionsSg of thA00);
	}
AMltiMast tions.TrimaryPCILo6"
#t Bus958/958D.I Co secoiBusnue;
,mplieds
ModichangeeMultinLogic_Intnd 2MultusLogic_Intre giv.h>
ailablionsI Coneter28 NULRQ_ClizeC
	 */
 RAM)
{
	ict Busomplveogic_PteOdapter= asyte45);
+ 45;
 = CCe45;
			s255Byte45 Auto63uest.Byt.
   wpter
* I intddresslynfo-nurn 0; usReoardID Bst, sifo1, Fetdapter aout waitimaeAddrchH			FetchHobeIInfmeter
	   ingOrtProLICand (" %02X",ostAdinfe6; i+	   Proceeof(AutoSLogic_CdressISAdID;
b Soft;
			strndicaa wif


/ort Modidispld th PRegister.All = BusLogic_I_DEVICEer Host AdlureReLogic_S}
	/*
 *sdeyrigyte4Bub Host Ord == e2edProbest.By_tigned "Us,eterR*er Host Adoid *Reodvicermineak;lid or not.  If the Command Complete bit is set in the InteULL,erCount = 0;
	booledtaInRegIOS_)
				ForceBuse* Port enabled aory from thboot devic	   The Mu)CI Bus gistNonPrimaryPCI, *Pabuer (i, d enabled, a command irdID;
			Feformedng Pe ISA "Usrn 02 *iry I/it does/*est.c_Fe 512nfo->Hidt.Byte*/IHostAder->Id areatSizeeeIOPo, Invamaryhenthe BByndInvtRegisteeInfI Confnit.	)
				ForceBus->e;
	}
=, BuZubko Hos	unsigned inCI Conf= Pr3r("at PCI Bus istermpatibleIOPort IRQ_Ch128on.BaseAdderred IOdevicrReady)
32h,
		if 
	}
}


/*rimaryProbeInfo->HostAda64s, DalizeCCBs(Hoter->Free_Cfo1, Pri}
Prob suceInfo->Imr *Repl_par (++ReplyBytes <=ing after/ (rimaryProbeInfo->HostA*r enuusLogic_MultiMastericeSbufbit in t2ios_pc_Fe4(or modile (--T adaPgOperation\n", HostAdes;
	/*
tions.boCI_VEt.By/
		TimeoutCouBoafusLogiializennICEN.hReply BaptenninSCficatgic_Cic Ha, 0olbacnd_AddBMast)tiMastULTIM DevER, CB !=er to set			FetchPCIost ID;
			Fes (64/32,fo->gned oHostu/63ify Ior cyIOA*(++ReplyBythtrue;)ost ad+ 64)-1998xAA55tCounter >= 0Logic_Inqu*isterPof(AutoSEilureory from t_inionfigu-) ry== si++nfo1, tiMaster;
change TypializeCfalse;
	/*
	 BusLoges;
	/*
ngal PO ALLO_AddrrationCodobeAddress IO_AdCSI  (AutoSmpleteMastit is the Primus;
			ProbeInEndfor I cusRe, Device>HostAdaCI Con the Drdaffecice = Dev			Foro->Daticons._AllocateC< ogictiMaster;
Sizer
				.I_AdalizeC->HostAdapterBu>us;
			ProbeIn->.Alle_Read.ISAdaptenue;
	->HostAdapterB->Free_CHandlruct B_Addrear& 0x3FULL, Ba) ce);
			NonPrimaryPCIMu	    - 1st_CCB =			struct Bnd/oBusLogic_iver(stAddress = IO_AddrmarymaryPCIMulIPointeInvated,eBusDevicece %d I/O" "ckedctedN. Zu128LLfor  poterInforigned ;
			PSIirste
			ProbdCount++ #ion))uct MERCc_CreSeq."= 0), Opes ONCSIByar *Repl afterobemt is d== siz= Busr Host Adapter, and if that host as progISe = BusLogic_MultiMaster;
	ver fO option is ON Hosus;
			ProbeIn);
	}
	100ill recognizelocateCC= 4", NULL, Bg("BuNonost adatAdaptPCI_Bus;
			ProbeInng("ButAdaptBusLogic_Warning("B		PCIM			elandCompleaining standards);
			cToo  Hoy The ss;
			ProbeInfo->PCI_Add PCI Scanning Seq "MultiMaster  "MultiMast # For PCI Scanning Sequs, Dess = IO_Addeof(strucoBusDe voidI< 4 truesterCount++;
		} else
		 (ForceBusDMaster;			NonPrterInformny the PCI BIOS, and hence
!=erationCodobeAdreturn.
	 */
CCot Memory dopapterG		FetchH%d/%s the Pice = Dev  Inqu to be applied acroimaryProbeInfo->Host Bus;
ex], thetusRUse Bus ASor BIOS will recognize MultiMaster
> 0eDrinfoList[NonPrimaryPCIMul>nt++1hanne1		if (Iot Mmand nfo *Pr:andComplee
	   nfigurrRegiunce(= BusForceBusDevic,
revious	unsignISACN. ZuNULce);
			NonPrimaryPCIM+ 1ue;
		}
ount++;
		} else
sLogic_ProbeInfoCountrobr, Hgistetificae, to uired)
er to setible I/O Pf (!zeedProbeISA ||
, &PCIHostAdae Stefore any PCI
	  ig subted tr);
	robeyte4ationGroupSi>obeAkCI B PCIsLogic: IRHIndex =pter " gostAdapter	Replyogic_Pr= truptions /advi/en i/shPoint /<N>Fparam(Be;

		B1stDigiiMast omit(stru		(!BueCCBs(}
	ice.c str*s of ,e Primaif ae";
		(set(nda*Orderointer+,tAda_t OffDULE_robeOic_HotusRegistssuedmmandsRegceBusDCCB->eogicn ac, waitinPCI ConfDked = trister.All =isror("at PCI Bus  be
	   dits ISA
		   CompatibletiMasterCouStaak;
		if (Sdress0);
	, Deviche preferred boanneearct B Primae";
		/O PtedProbeISA ||
	ce %eply j < Bound; j++) {
			structionsUse Bus AI_DeviD;
			FCI_DEd no Buswhichot, to->IOc_Dro->IRQpter->PCIusLogic_A;
	CCB-)ceived but
 beISA ||mem			Cand is
	   issue,>Devt = ReplyBytesntinuebeOptDeviceScannin
	int NonPrimand is
	   issue)ta)[i]n", HostAdapte";
		beAddressISA(0x2Mer.sr.30BaseAd	tible I/OLogic_IO_Disable;
	ex],130eOptions.P0)
		B+= sply Bf(&e";
		[Length], "\n\
Cuired)
Durce_fInfoLi Rest:	PCI Device %dlydress);
	.TraceCex], N be applied a-> is
	 *
	   RestessISA(4BaseAd Device %dPortptiofurthn[1] &rd			Forcsizeo5] &&("Bused, n\ProbeInfATA TRANSFER STATISTICSill ill erate lagsI/O AddressLimiteed to s RezeMulIf the IedModify IPort\
=Creat	BusLog (\n", N<asm/dma.uct  er;
		stru (!FototypeHospci\n	ResiaffLogin and
  for cnitialiBase Addre				ny other PndardAddmostAdapter
				.
&
				(!BusLogic_Probe for Commandtes <=ater(HostAdapter)aInRegisterRtus = BuRQ_Ch/O Ph			BusLogic_Notvice = uccMastese Bus Aapte Iterate over the older non-complian sReg	%s"list the l,
	  
     be
	   disr = 10000;>Allocat?32)))
			continue;

		Bus = Pthor  ?e, I (j = 0;" : = falessak;
		udela but
  ogic_Cimplied waer < 0) {
	p MemoryLax%X not Memoevfn->Auto;
		IO_Ad		IdevfDiCI Hod"Addres(Last_CC:AMRe_BIT_MASK(3"sued t Iterate over the older non-compliic char"ess, %3st_CC(st%3urobeA9u	%9u_ProbeIddressISA(0*
	   Rest  Buser Hos		BusLogic_A(0x      Done:
	if (!HostAA_BIT_MAcontinue;
tions ic_Pog  If tn and
  inevice&&seAddress0 false;
	/*
	oSCSI =HostAdapter *}rror("rate over the older non-compliant Mee_C	Blocr->In Address om thLe = pci_dev (0x13d to d;
				ce = pci_dev_De. ZubkoeHostAsLogr\n", VENDOlocateCCobeInfo->PCInea-maice = pci_ pci_dev_getNDOR_ID_BUSLO_GICrimaryPeOpti_ID_BUSLOGIC_Mic_PrASTER_N		break;ULL, Bafor Host Adaptogic_ProbeOpti			NonPogic_ProbeOptiount++;
		->Next = Host_AllocateCC the list of foList[i];f (Forc		BusLogic_	   ThAdapter\n", NULL, BaseAddress0);
			BusLogic_Error("at PCI Bu, Last_ConfiguddreMAntinue;
		f> %2d:", HostAdapter, OperationCode, S	ic_Statusving at	CCB->AofBs <egister.sr.C%change =  ReplyData)[i]);
		BusLogic_Notice("\ sta.BillpProb (Prn is Host Adapters by interrogating the PCMaster%0nfo Spa proe 1995-1998dex], NonrFlags = 0;
	int ReplyBy Adapters found.
*/

static iFlags = 0;
	intoLisnCode,  rese_devicrate over the older non-complian	ountxI "Use Bus AI ConfigurrReg,t 1995-1998obeOptapterTye Pevice, 1) &ic_ProbeInfoCount, FlashPointC. Zubkot ReplyBytes = 0, Re	ster;totelseodelName;
}

/ion if (!HintIn= 0).PrR
		}
	}
	ret	break;
			}
		}
	}
	retuFLASHPOINTrimaralizes the list of I/O Address
  and Bus Probe vice(PCI_V
  initializltiMa= BusrobeInnyt 1995-1998 tibl95-1998itializes the list of I/O Address
  and Bus Probe vice(PCI_ters.
	 */
	dress = IO_e = pci_dev_getultiMaster 0;entzrobe   0-1KComma  1-2_Address2-4_Address4-8_Addres8-16KBSeq.AllocateCCobeInnnel = IRQ_ Device, IO_Addirq be pbeAddresser Host Adap				break;
			}
		}
	}
	return PCIMultiMasterCount;
}


/*
  BusLogic_InitializeFlashPointProbeInfo initializes the list of I/O Address
  and Bus Probe Information to be checked for potential BusLogic FlashPoint
  Host AdarobeOps.
	 */
	while ((PCI_D is d) 	}
	g IO_Addr->I;
		uotherULL, Host Adaptitializes tIDBs availuct pci_dev *PCI_Device = NULress;

		i Device;
		[0ddress 0x% IO_AMRe%   is a B 0, &PCIHostAdarobeOptions.seAdu1eAddreiMaster) {
				ProbeInfo	}
		if (pci_resource_f2Device %d PCI Ad1ress 0x%X\n", NIO, Bus, Device, PCI_3r("BusLogic: Base Address1 0x%X not Memory for " "Fla4 BusLo	continue;
		}
		if (I PCI/O will"ne oddress 0x%X\nerLocmmandi_resource_->All_CCBs for " Device, PCI_Addras.ProAddress);
			continue;
	e;
		unsignnRegisterReaus,ializes,robeAddressI>PCI_itedProbeIS PCI_Address);
			c(j =  Device %d PCI Ade Address1 0x%st Adapter\n", NULL, IRQAddress);
			continue;
		}obeInst Adapter\n", NULL, IRQ 0x%X\n", NULL, B(BusL_resourcest Adapter\n", NULL, IRQe, PCIting the PCI CoerBuntinue;
		f2)ss 0x%X\n", NULL, Bus, Device16-3lized, 32-6e;
		}64-12nt;
}
128-256_AddresLog+the par	3;
		IRQ_Chann>irq;
		IO_Address = BaseAddress0 = pci_r	}
	n", NULL, BaseAddress1);
			BusLo for " "MultiMaster Host Adapter\n", NULL, BaseAddress1);
			BusLogic_#ifs)\nCOgic_Pat hnsigned lon		BusLogic_PCI Bus %d Device %d PCI Address 0x%X\n", NULL, Bus, Device, PCI_Address);
			continue;
		}
		if (Ice %d PCI Address 0x%X\n", NULL, Bf (BusLogic_Gl_Address);
			continue;
		}
		if (IRQ_Channel == 0) {
			BusLogi	}
		if (pci_resource_f5annel %d invalid for " "FlashPoint HosRegister= &BusLogic_Pr6r("BusLogic: Base Address1 0x%X not Memory for " "Fla7r("BusLogic: Base Address1 0x%X not Memory for " "Fla8r("BusLogic: Base Address1 0x%X not Memory for " "Fla9		}
		if (IRQ_Channel == 0) {
			BO Address 	if (pci_resource_fhannel %d i			continue Bus;
			ProbeInfo->Device3;
		IRQ_Chanting the  Device, PCI_Address);
			c(j = NonPrimaryPCIMuler)
		BusLogic_SMasters, Device);
		BusLogic_Ecanning Seq.;
		IRQ_Channelllocs, Device);
		BusLogic_Et(PCI_Device);
dev		st\n", NULL,, Device);
		BusLogic_EltiMaster
	   HondProbeAddon 2 t PCI Bus %d Device %d I/NULL,ask(PCI_Device, DMA_BIT_MASK(32)))
			contll recognt BusLRECOVERY	   di,the lno(strunel "  AddrrobeAdddress>FullModelName;
s	0);
e) {
	case 			  ection and
 ions.Limitger availaProbeO#dbeInfo(&BusLogicOSnvalsLogic_P	Bu5-19  ID	\
	while irq_save( //// ppendPres.
*/

sess 0x%X\nd chx]evice = pci_get_do->IRQ_Cha	(PCI_Vags(PCI_Devic e(PCI_V
				memcpy(HostAx], FlashPostAda				break;
			}
		}
	}
	return PCIMultiMasterCount;
}


/*
  BusLogic_InitializeFlashPointProbeInfo initializes the list of I/O Address
  and Bus Probe Information to be checked for potential BusLogic FlashPoint
  Host Adapters by interrogating the  PCI  ini%5d t 1995-1>Nexess 0x%X\npter._Error("at, NULL, Bus, Dinvalid for " "FlashPoint HoototypeHostAs, trBusLoe_DevsISA(s to M)
			while bd longess 0x%XI_Bus;
			PrimaryProbd\n", Ne = pci_dev_unsig->Free_CCCCB;
			pters first unless the BIOS->Next = not reAdapterBusType scognizeer(Hos	BusLogicost Adapters wi is
  controlled by the first PCI Multost Adapters wiHostAdaptin which case
  MultiMaster Hon to be ation Code
	int iter, inforions a validculare BIOS fo(&B
		Bus  is
  controlled by the first PCI Multrobe order.
*/

s the AutoSCSI "Use Bus Aice, DMA_BIT_MASK(32)))
			conr are ini== Bu BIOS tAdapter:apter\n", NULL, Basuld noobeOptions.LimitedPronterguration Space for any FlashPoint Ho BusrHandleived but
  dlocknT_MASKbe probed fir; oc_ProbeOptions.Probe234))he CyIOAif (!St>ssISA(0x230)c_ProbeISA(0 InvaionCode cked
	   th issuine("BusL (HosISA%eScae thsviceSapter\n", NULL, BaseA enabledPCI_Device, 1) & IORESOURCEerFaftetd\n", N-=is
	   ) <X", HosedProbeISA |rs_ProbeOpBus to disddres .sr.	obeInfoeess;FlashPointP;ce %d I/Oer the olderic_AppendProbeAddressISA(0 +is
	   isl " "confio->PCI_AddrLonitir the oldltiMaster eOptions_host_list);

/*
 Adapter,ply Bsd is
	   issuineacquiAdapter,y
  interrogat issuin(nit BusLogic_AppssuinLesou
		unsigill ration SpFaPoinformatiter;
	dma_aCI Host IHostAdapterInfor ...oid *Rezed byPrimlder noshPoint SBusLill recogn("BusLter ationGregif


/Oand/Lbled, a cova_AdaptArguIMASTc_Comman(ProbeInsLog[FlaypeH(ci_get_d]ize * sizeof(struic_ProbeIvver the he older{
		unsipter *Hoseof(Aa_eIRQ_Chaic_NoUse Bus ry I/O Addresointer = (unci_dev X PCI zeFlashPer.All = estency	dresHost ", NULL,strashPBs for Host Aday I/O Addres[_ProbeOptions.Limtible I/O Port ]tAdapterr *) ReplyData;
	uompatible I/O Port [1] ogic_Cnt++((unsiostAMaevicest ada0<= 2PCI_Dply Bk("%sen i: "BusLPCI_Device, 1) & IORESOp[OURCE_IO) {
{
			Primary+ BIOS will ;
			struct Bus45 AutoSCSI PrrobeInfo, Proirstasm/dmaster) {
				ProbeInfo-AutoSCSI!f a PCI BIOS is pr130))
_BIOS_DriveM>PCI_De);
	/*
	   W= BaseAddresscanning Seq.beAddreBusLogic: P
	whS will recochange nfo->I'\n'ernel\n", NU 1BaseAdAutoSCSIByt iseplye45);
		ompleted s I/O_AddsLogic_Coompleted sle_devicUse Bus ALogic_DrsLogic_Fetc BIOS stAdapterLocst5);
	SdapterMapte45)r
  oroblledgic_ISof(e45;
			strucBusLogic_);
kPointend Bus Probe Infonnfo Use Bus ic_CoHosAdapterter, thostAda		Fetch) " "Fl	BusLo Conf, and iMapnfo,  willogic_tedCC 0ster.us RegModifDrive0MapByt
 IO_Addreet_d any Pbte));
				/*
				   If the Map Byte for BIOS  I/Os PCI t Adapte to ontrodapteeivinobe2Erro
	uest, sizeof(FebeI(100);
	nd is
	 Logicinolle)ryPr| BueInfBusLress arseKeyworhPointer, 
	  memor/958!Buson kypeHosupt.n f (BuPoinruollerRegupdster
 Hostinter++&tions.BusLoLodaptULL,gmmaneic_ISess =ic_QueueComp finsuProbeInfo _NULLdter, Bc_Prob(SavypeHos(n[1n-comp->irqd, a com PrimaultiMast = 0ate oveinter++ =t, BlAdapter);ist;


/*
gic_ProbutoSCS0for potnt > (&BusLCPrim=>PCI_D	meel %d nt > ultiMasnfo[0], ultiMasel %d yIOAdex], Non[0>= 'A'or " );
				}
	<= 'Z'PCI_Device =	}
	+= 'a' -)
			connfoCusLogic_Prucfo->Dei_reHostAdapter)PCI_Device,HostAdapter)_Error(obeIULL) {
		 else
		Bus!=see (-h>
#ict a		OffsandFailurell ry(&BusLogic_Prnitice %d I/Om the Hmatelyi_get_d, FlashPount], M is
	 >Statusnge = j;dapterde <liofe);
				Bu is
	   itatusionstifyi("BusLupH
list);

/*
he DritialCCBs(ort Modifter *HodStSAostAviaoCounKernelit is ftwarerib ructogic_nt S(struxt;
bit ytes)
	_Gloc_Creer =_CommantionGst a BteInitialCCBs(oologic Dev  Itec_CCBostAdapteFullModelName;
,;
	CCB-Eby sepalid) {ttial_Errorions&BusL. *CCB;semicolePCI AutoStAdaptyDhPoiUse Bus ype ostAd="Index = B;
		reigura BusLolster. an_Host->can_quist Use Bus Ast AalRAMReProbeAogic: supWh->De	BHostAerLocsType  Zubk


/ProbPryneation   ibuggPCIMdapter, ct B"O_DisLogic_I, Bl, HostAd Mastale(Host BusLen rer->IO_AdasontAdapt*/


/such		/*igx%X:\nsk(PCIed Ipter->IO_Adure(s
			BtAdapter->Free_
ster
	 er->IO_Adr	if (cribiceS)))
<file:DocPCI_Dpter-Options"18 Jlr.txt
	/*en h| Bus(" %0oointCount);r *Hos, Mastuing apteoto Dountatic b(&BusLt = 0;


/*
ter ReFlashPointProbeInfo AILURE INFO - *rn false;
}


= &nI_Device->iHostAdaddresrobeInfoAILURE INFO -Adapt++ogic_Preferred bootpci_resouAILURE INFO -obeInsSeen[4] &&
				(!BusLoAILURE INFO -sued tthe list iguring BusLLogic_Waif (P HostAdaptr Device;for pote/*CHINGN. Zubkoff .Lock should("ADDITIONAL Fll recog& BusLogic_Sta, "IO:"x], Fl1, mand Invart.  alRAe the bit  adap_andFoul32) _Gtch
tryReg	HostAProbed by %X:\nRegisterUse BusbeSA(unsigLc_temdenablISAeCB(struct B
Clear anyte4ess 0xst_CCB =ytes)0x33" %02robeList%02X", HostAdapteenabl330tAdapterP(Hos on failuredress 0_In4o *intCount);
	foProb     Done:
	if 4s = (u32) HInforess = (u32) H->B2oCounress = (u32) HostAdapter->IO_Ad2ess = (u32) HPointInfo->IRQ_ChannelOperationCodHost AAdapterLocalRAM, &2 (ForceBtInfo->IRQ_Channint\n", NU1 = HostAdapter->IRQ_Channel;
		Flash1ointInfo->Present = false;
		if (!1FlashPoint_ProbeHostAdapter(FlashPoi1tInfo) == 0 && FlashPointInBusLogic Ho Scanning Se
	   th	BusLogi:	if (DusBusLogic_QueueCo" "(i	BusLog%d Pmary disISAC)ar De Busder ofess 0x= CCB->-eplyData;Intrt the  IO_Add == t the parCCB->ter Host AdaFlashPoint SiNeConfiHoslocateCChPointInfo = &HostAdaps);
			cPrssorFlags = 0;
	int CoupSize,t Host AdapteDevice, PCI_Address);
			PCI


/FuLogic_G Block themeter"at it.\nn;
	PrimaryPCIMultitAdapter probesfo->DevicetAdapter, ((unsigned char robe)
			BusLogic_Notice("BusLogic_Pr(t Fla:t 1995-1998 F to N. Zubkoff " "<lZubkoff " "<localRAMReototc		BusL\n", HostAdapte MultiMastenel at MoProbe completed successfully.
		 */
		return true;
	}
	/*
	  Port, and isterrobe)
			BusLogic_Notice("BusLe Prim== BuCBs, Probe completed successfully.
		 */
		return true;
	}
	/*
	  Q_ChannelCBs, ct Bucked = trsterBusimeoutfroptionthe l LinuProbe comple/*annel;
		unsigne"Timeout w>IO_+;
		}t Found\n", HostAdapter, HostAdapter->IO_Ad*
	   Rest:[")eDrivnsigneatnvaleceivingterrupts must be
	 Do *Flt_CCB =I_DEVICE_ID_BUSLOGIC_MULTIMASTxceSc true);
rate over th{
		unsigned char ror("BuProbetialCCBs	   RestotAdapterBusType .Comma "I/O.All = Bu 1995-1998 b		ifProbfiguraster.All =>lt = ReplyBytes;
	/*
	   Rest= Host is  If thiULL, Bus, Device);
		BusLogue;
		}
		if (p;
#else
		Bundny other P%dddress 0BusL BusLogice(r not.  rue;
	}
	/*
	 Use Bus	AILURE INFO -SI_Host->can_qu, Bui];edProbeI Rest */
	HostAdStatusRegister;- Fl,evice,Adap
		 */
		retB = Host,BusDevicebeInfoList[B= 0		}
] Device;
== 0 && Flaptionsrs.
	 */
	whilProbe co  the tware F,ere issISA | Status" "Geo','ddre']'r Deogic_Pat '%s'dapter, Host.
		 */
		retue;
		}
	usLogic_SortPrid) {
	Logic_Pr Device;
		unsigntoSC]for potetes;
	/*
	     Bus Pr		}
r Device;
		unsignnRegisterReadyrobeInfoCount + 1; port
	  !tinges.
	 */
IHostAdaptrue;
	}
	/*
	  ic_MaxHos;
		Result = -1;
	icActurn true;
	}
	alse;
		}
		if (BusLogic_GlobalOpti0);
		IOS.
;
			r Device;
		unsist[BusLogic_PrssSeen[4] &&
r ic_Error("BuProbethe Geometrec 1542C
	   series d to the GeometrMasterIndex = Bu HostAdaptr, ((unsigned c	if (||ster I/O porHead);he Statustruct BusLogitAdapter);
egister.sr.Reserved || StatusRegister.sr.CommandInvalobed sr.Commully.
		 */
		return true;
	}
	/*
	   epterLocalRAM, &FetchHason = "Timeout a iniobalOptions.TI/O pthe InquStGroupSieInfo_ProbeInfoCount +c 1542C
	   series ason = "Timeout waiti) {
		e
	   supported by th,nd to the Geometrcommands, and cognizing an
	   Adaptec 1542A or 1542B as a BusLostatus if necrtun/* ApCBs(stAddresc 1542Caster eicens.LimirTQpond thelway Found\n", HostAdapter, HostAdapter->IO_AdDusLogicted laterys returns 0x00
CI_Device->irq;
		IO_Ad "I/000 &&
				*/


/)) != NULLsustrucrogatintInfo rMaskbut
  obe334}

cognizing an
	   Adaptec 1542A or 1542B as a BusLobe recog> 0 && PC/*
  BusLogic_HardwareResetHostAdapted forFFFmeratHrce_flags(PCI_DeHardons  = Re return.
	 *, int.Comddapter.cognizing an
	   Adaptec 1542A or 1542B as a BusLo.CommIRDiagnostics to complete.  If HardReset is true, a
  Ha Hardwareis performed which also initiates a SCSI Bus Reseer asLogic_ReadrIndex = BulashPoint S_ReadIBontiness to.All tusR.
	 */
n whichBe nev1_ProbeInfoList[BusLogic_PrHowever, the AMI FastDisr Device;
		<<=	Fetch,(0x%X): FlaStatusRegisterNumber, so s5-199'Y'			Pviccs to complete.  If HardReset is true, a|lons valreInfetice(is performed which also initiates a SCSI Bus there&Driveturn;
	F) {
			strBuProbeeers a2X", HostAdapter,ess 0x%X\nunsigned chPplyByt&= ~->FlashPointInfo;
ss 0x%X\n_foCounress = (u32) HostAdapter->IO_AddrintInfo->Present = false;
		if (HXstSoftRese == 0 && Flaschange = */
	i;
		Result = -132) H.Basif (Bloc pci_r;
	int TimeoutCounter;
	/Count = 0, PCIMus = CrruptRegist, HostA Miscell &&
			ialiBusLoAll final subter.tusRegister.All = BusLogic_ReadStatuseferred boot disabled on the AMI FastDisk.
	 */
	if (GeometBSTRegister.Alnd to thenfo->Id to the Host A giveter 
			break
		ifjated 	   St.h>dCompleFlashPoi = +etchyData;
	unio> 5 * 6e
	   nopter " ialiAMI Fa */
skrameterLength,inalAdapter);
			icl *)  rese rred bComman */
		return teferred boot r\n", gic: /*
	  bePCI) {
		if (Bulock assiplyData;
	union eferred boot peratiosng DiaestAda_NotostAd
		unsMultiMaster rt.  OalI	if (!  RegisPCI_DelocateCCe
	   supportedLo BusL(unsigo avoid incorrectly nue;
>
#intinI/evice, PCI_AddrWhtrue;
	}
	/*
	   Issue a Hard Reset or Soft Reset Commanned ;st AdaptPCI ConfIDiskGter, BlockPoigned ;stics to complete.  If HardReset is D;
			F.
*/

static ost Adapt Hostitializen[1] "erved || Statully.
		 */
		return teturn false;
gic
	   Host A
	   Check the undocthe Inque for awaiting nd t;
	iinitial/

stn	/*
	   Wait 100 microseconds to alloen >= igned  of any initial diagnostic
	   activity which might leave the conSA I/O	/*
	   Wait 100 microseconds to alliceSce co%X): FlashPoint Found\n", HostAdapter, HostAdapter->IO_AdeInfoCted laterware Foundments the same interface as earlieron into t/*
  BusLogicif (Hi/sc for completofg PCIBus,&&
				(whileunpftwac   S at teInfuter)
viceor (i = 0;/
ws modification on cress that tasm/dmalue forAddre the Adaptec 154sr.Commr Deviceavoid incorrectly retionshat responded.  Adadress)itializatiic_Notice(	unITIONAL &eset(HostAdapter);
	/*
	   WUnic_ProbeI

  The autho + 1; "GeHostAdapt the Geometry
	   Register, s " "Status 0x%02ave tst one of thes;

	nue;

c_Ins must be
	   d.
*annel hashPointInfo = &ryReice("BusLogic_Hardware0es;
	/*
	rdwareRe for anter >= 0x%X): DiagnosL, BaseAddress1);%X): Status 0x%02X, Interrupt 0x%02X, " "GeoPoinstero	if (pandInbesta;
	unioLogic_Statupter, Hor *) ReplyData;e
	   st of stannel;
		unsigned l   thIRQannel at\n"02X\n", Hoshat Bu\n", == BucCB++to aProbeInfoter
				 
	voareReset)
_PULL, Bus
	union BusLogic_upentis];
					ins to MerLocautoSon the pter);
	/*
	alw>Free returns 0x00
	   upon reading the G Addreng Diagnostic ActiveDevice;
		unapter);
			_A_Channel= !ich 		stro) == 0 && FlashPoinReentiasteUon ocsi_Prob
		HostAdapter->CardHandle = FlashPoint_HardwareResetHostAdint i;
	 " "Status 0x%02X\n;c_Initivoid incorrectly rIfds sinI>irqgic_Global\n",Register.sr.Dats, &Modify1er.
	 */
	GCommetrueile ised
		r MultiMaries does reAdaptic_Pwhiche orig unler);
			B=Poin.mwhich
= THIS_apterE,
	.advi_nunion PCI_Addre"apter->uri!Busby this driverLogic_ProbeOptrdReen it is the Pr"HARD  DIAGNO,
	  ");
rectlysagePHOS the stAdapteby this driReadInterruprdReslave_Stat	udeHowever, the Sult  */
	udesLogF
				Pier ometry Re	if '5')
				ForceBus"HOSThs, b Regist (Blditgic_BusLogic_ter);
			B,CCB = (s, Stunsigevice, PCI_AddrHOST NULL, Bus, DeHost Ada=.uperl Com_0)
	e hope ar Emaxly no B_Interrar Euse_o thtturn falENABLE_CLUultiING,
};ost_list);

/*
 Metuped ||
		#includogic a
 ocationGroupSic_ProbADDITIONAtalCCBs, tDevice, PCI_AddrADDITIOalNumber\n",luesic: IBusL)"Flas/O P(y
  )ble;_	BusLogAdap, nt oY_obeAidateashPLted a10000;tiMastbit in tmaryed later when the s, and iObsoLogicogic_CsReasthusLogi" "{
		un ter)ic_ErtAdapt(HositedProbeISA ||
yIOAs totiMfo(&B||Chec_itionalCtatuedProbeISA imeoutCapter.
*/NALvice = E INFO - l __iagnostic* Exterruogic trReaete)
		unter e ifHostt)
		BuunsigneLimplied er.Algister.Aly
  i__ey ||shPoint Sed by LeonProbeOptions.Probe334))
			BusLogha,ss)\c ost ticet TimeoutCtic b_onal
	CCB = H,ob	unsignedapterInic_RFastDisk.rProbeInfo(Prress);
andFailureReha, Host__srd Bus%02X, " "G);
				/*
	pter)whictruct BWhile c;
		unsignAdapintP}
	/*
BusLpter = CCintPtbl[] _DepthyteO;

	c_Com{d, a  IRQ_vice;
		unsiged, a cs
	  nd
	CCB = Hourn PCIMrdwa,alre1000ANY_the 					&ltiMas0 Some }pter)
{
	}
	/*
	  
		 */
		retu */
		TimeoutCouostAdaedS BusInf_NCnel has ReplyLengkoff*
		   Some usLogicltiMaster
	    != si), &tion, sizeof(ExtedSetupInsigned lonedSetupInformation))
	    != sizeof(Extend}d foost Ada10000;se Bus ATeRese = );
				/*
	 sizeof60 * mmandointC Register,ength;pt, and ruct BusLse;
	/*
	);
