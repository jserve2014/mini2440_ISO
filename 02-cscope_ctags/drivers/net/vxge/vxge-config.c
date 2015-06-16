/******************************************************************************
 * This software may be used and distributed according to the terms of
 * the GNU General Public License (GPL), incorporated herein by reference.
 * Drivers based on or derived from this code fall under the GPL and must
 * retain the authorship, copyright and license notice.  This file is not
 * a complete program and may only be used when the entire operating
 * system is licensed under the GPL.
 * See the file COPYING in this distribution for more information.
 *
 * vxge-config.c: Driver for Neterion Inc's X3100 Series 10GbE PCIe I/O
 *                Virtualized Server Adapter.
 * Copyright(c) 2002-2009 Neterion Inc.
 ******************************************************************************/
#include <linux/vmalloc.h>
#include <linux/etherdevice.h>
#include <linux/pci.h>
#include <linux/pci_hotplug.h>

#include "vxge-traffic.h"
#include "vxge-config.h"

/*
 * __vxge_hw_channel_allocate - Allocate memory for channel
 * This function allocates required memory for the channel and various arrays
 * in the channel
 */
struct __vxge_hw_channel*
__vxge_hw_channel_allocate(struct __vxge_hw_vpath_handle *vph,
			   enum __vxge_hw_channel_type type,
	u32 length, u32 per_dtr_space, void *userdata)
{
	struct __vxge_hw_channel *channel;
	struct __vxge_hw_device *hldev;
	int size = 0;
	u32 vp_id;

	hldev = vph->vpath->hldev;
	vp_id = vph->vpath->vp_id;

	switch (type) {
	case VXGE_HW_CHANNEL_TYPE_FIFO:
		size = sizeof(struct __vxge_hw_fifo);
		break;
	case VXGE_HW_CHANNEL_TYPE_RING:
		size = sizeof(struct __vxge_hw_ring);
		break;
	default:
		break;
	}

	channel = kzalloc(size, GFP_KERNEL);
	if (channel == NULL)
		goto exit0;
	INIT_LIST_HEAD(&channel->item);

	channel->common_reg = hldev->common_reg;
	channel->first_vp_id = hldev->first_vp_id;
	channel->type = type;
	channel->devh = hldev;
	channel->vph = vph;
	channel->userdata = userdata;
	channel->per_dtr_space = per_dtr_space;
	channel->length = length;
	channel->vp_id = vp_id;

	channel->work_arr = kzalloc(sizeof(void *)*length, GFP_KERNEL);
	if (channel->work_arr == NULL)
		goto exit1;

	channel->free_arr = kzalloc(sizeof(void *)*length, GFP_KERNEL);
	if (channel->free_arr == NULL)
		goto exit1;
	channel->free_ptr = length;

	channel->reserve_arr = kzalloc(sizeof(void *)*length, GFP_KERNEL);
	if (channel->reserve_arr == NULL)
		goto exit1;
	channel->reserve_ptr = length;
	channel->reserve_top = 0;

	channel->orig_arr = kzalloc(sizeof(void *)*length, GFP_KERNEL);
	if (channel->orig_arr == NULL)
		goto exit1;

	return channel;
exit1:
	__vxge_hw_channel_free(channel);

exit0:
	return NULL;
}

/*
 * __vxge_hw_channel_free - Free memory allocated for channel
 * This function deallocates memory from the channel and various arrays
 * in the channel
 */
void __vxge_hw_channel_free(struct __vxge_hw_channel *channel)
{
	kfree(channel->work_arr);
	kfree(channel->free_arr);
	kfree(channel->reserve_arr);
	kfree(channel->orig_arr);
	kfree(channel);
}

/*
 * __vxge_hw_channel_initialize - Initialize a channel
 * This function initializes a channel by properly setting the
 * various references
 */
enum vxge_hw_status
__vxge_hw_channel_initialize(struct __vxge_hw_channel *channel)
{
	u32 i;
	struct __vxge_hw_virtualpath *vpath;

	vpath = channel->vph->vpath;

	if ((channel->reserve_arr != NULL) && (channel->orig_arr != NULL)) {
		for (i = 0; i < channel->length; i++)
			channel->orig_arr[i] = channel->reserve_arr[i];
	}

	switch (channel->type) {
	case VXGE_HW_CHANNEL_TYPE_FIFO:
		vpath->fifoh = (struct __vxge_hw_fifo *)channel;
		channel->stats = &((struct __vxge_hw_fifo *)
				channel)->stats->common_stats;
		break;
	case VXGE_HW_CHANNEL_TYPE_RING:
		vpath->ringh = (struct __vxge_hw_ring *)channel;
		channel->stats = &((struct __vxge_hw_ring *)
				channel)->stats->common_stats;
		break;
	default:
		break;
	}

	return VXGE_HW_OK;
}

/*
 * __vxge_hw_channel_reset - Resets a channel
 * This function resets a channel by properly setting the various references
 */
enum vxge_hw_status
__vxge_hw_channel_reset(struct __vxge_hw_channel *channel)
{
	u32 i;

	for (i = 0; i < channel->length; i++) {
		if (channel->reserve_arr != NULL)
			channel->reserve_arr[i] = channel->orig_arr[i];
		if (channel->free_arr != NULL)
			channel->free_arr[i] = NULL;
		if (channel->work_arr != NULL)
			channel->work_arr[i] = NULL;
	}
	channel->free_ptr = channel->length;
	channel->reserve_ptr = channel->length;
	channel->reserve_top = 0;
	channel->post_index = 0;
	channel->compl_index = 0;

	return VXGE_HW_OK;
}

/*
 * __vxge_hw_device_pci_e_init
 * Initialize certain PCI/PCI-X configuration registers
 * with recommended values. Save config space for future hw resets.
 */
void
__vxge_hw_device_pci_e_init(struct __vxge_hw_device *hldev)
{
	u16 cmd = 0;

	/* Set the PErr Repconse bit and SERR in PCI command register. */
	pci_read_config_word(hldev->pdev, PCI_COMMAND, &cmd);
	cmd |= 0x140;
	pci_write_config_word(hldev->pdev, PCI_COMMAND, cmd);

	pci_save_state(hldev->pdev);

	return;
}

/*
 * __vxge_hw_device_register_poll
 * Will poll certain register for specified amount of time.
 * Will poll until masked bit is not cleared.
 */
enum vxge_hw_status
__vxge_hw_device_register_poll(void __iomem *reg, u64 mask, u32 max_millis)
{
	u64 val64;
	u32 i = 0;
	enum vxge_hw_status ret = VXGE_HW_FAIL;

	udelay(10);

	do {
		val64 = readq(reg);
		if (!(val64 & mask))
			return VXGE_HW_OK;
		udelay(100);
	} while (++i <= 9);

	i = 0;
	do {
		val64 = readq(reg);
		if (!(val64 & mask))
			return VXGE_HW_OK;
		mdelay(1);
	} while (++i <= max_millis);

	return ret;
}

 /* __vxge_hw_device_vpath_reset_in_prog_check - Check if vpath reset
 * in progress
 * This routine checks the vpath reset in progress register is turned zero
 */
enum vxge_hw_status
__vxge_hw_device_vpath_reset_in_prog_check(u64 __iomem *vpath_rst_in_prog)
{
	enum vxge_hw_status status;
	status = __vxge_hw_device_register_poll(vpath_rst_in_prog,
			VXGE_HW_VPATH_RST_IN_PROG_VPATH_RST_IN_PROG(0x1ffff),
			VXGE_HW_DEF_DEVICE_POLL_MILLIS);
	return status;
}

/*
 * __vxge_hw_device_toc_get
 * This routine sets the swapper and reads the toc pointer and returns the
 * memory mapped address of the toc
 */
struct vxge_hw_toc_reg __iomem *
__vxge_hw_device_toc_get(void __iomem *bar0)
{
	u64 val64;
	struct vxge_hw_toc_reg __iomem *toc = NULL;
	enum vxge_hw_status status;

	struct vxge_hw_legacy_reg __iomem *legacy_reg =
		(struct vxge_hw_legacy_reg __iomem *)bar0;

	status = __vxge_hw_legacy_swapper_set(legacy_reg);
	if (status != VXGE_HW_OK)
		goto exit;

	val64 =	readq(&legacy_reg->toc_first_pointer);
	toc = (struct vxge_hw_toc_reg __iomem *)(bar0+val64);
exit:
	return toc;
}

/*
 * __vxge_hw_device_reg_addr_get
 * This routine sets the swapper and reads the toc pointer and initializes the
 * register location pointers in the device object. It waits until the ric is
 * completed initializing registers.
 */
enum vxge_hw_status
__vxge_hw_device_reg_addr_get(struct __vxge_hw_device *hldev)
{
	u64 val64;
	u32 i;
	enum vxge_hw_status status = VXGE_HW_OK;

	hldev->legacy_reg = (struct vxge_hw_legacy_reg __iomem *)hldev->bar0;

	hldev->toc_reg = __vxge_hw_device_toc_get(hldev->bar0);
	if (hldev->toc_reg  == NULL) {
		status = VXGE_HW_FAIL;
		goto exit;
	}

	val64 = readq(&hldev->toc_reg->toc_common_pointer);
	hldev->common_reg =
	(struct vxge_hw_common_reg __iomem *)(hldev->bar0 + val64);

	val64 = readq(&hldev->toc_reg->toc_mrpcim_pointer);
	hldev->mrpcim_reg =
		(struct vxge_hw_mrpcim_reg __iomem *)(hldev->bar0 + val64);

	for (i = 0; i < VXGE_HW_TITAN_SRPCIM_REG_SPACES; i++) {
		val64 = readq(&hldev->toc_reg->toc_srpcim_pointer[i]);
		hldev->srpcim_reg[i] =
			(struct vxge_hw_srpcim_reg __iomem *)
				(hldev->bar0 + val64);
	}

	for (i = 0; i < VXGE_HW_TITAN_VPMGMT_REG_SPACES; i++) {
		val64 = readq(&hldev->toc_reg->toc_vpmgmt_pointer[i]);
		hldev->vpmgmt_reg[i] =
		(struct vxge_hw_vpmgmt_reg __iomem *)(hldev->bar0 + val64);
	}

	for (i = 0; i < VXGE_HW_TITAN_VPATH_REG_SPACES; i++) {
		val64 = readq(&hldev->toc_reg->toc_vpath_pointer[i]);
		hldev->vpath_reg[i] =
			(struct vxge_hw_vpath_reg __iomem *)
				(hldev->bar0 + val64);
	}

	val64 = readq(&hldev->toc_reg->toc_kdfc);

	switch (VXGE_HW_TOC_GET_KDFC_INITIAL_BIR(val64)) {
	case 0:
		hldev->kdfc = (u8 __iomem *)(hldev->bar0 +
			VXGE_HW_TOC_GET_KDFC_INITIAL_OFFSET(val64));
		break;
	default:
		break;
	}

	status = __vxge_hw_device_vpath_reset_in_prog_check(
			(u64 __iomem *)&hldev->common_reg->vpath_rst_in_prog);
exit:
	return status;
}

/*
 * __vxge_hw_device_id_get
 * This routine returns sets the device id and revision numbers into the device
 * structure
 */
void __vxge_hw_device_id_get(struct __vxge_hw_device *hldev)
{
	u64 val64;

	val64 = readq(&hldev->common_reg->titan_asic_id);
	hldev->device_id =
		(u16)VXGE_HW_TITAN_ASIC_ID_GET_INITIAL_DEVICE_ID(val64);

	hldev->major_revision =
		(u8)VXGE_HW_TITAN_ASIC_ID_GET_INITIAL_MAJOR_REVISION(val64);

	hldev->minor_revision =
		(u8)VXGE_HW_TITAN_ASIC_ID_GET_INITIAL_MINOR_REVISION(val64);

	return;
}

/*
 * __vxge_hw_device_access_rights_get: Get Access Rights of the driver
 * This routine returns the Access Rights of the driver
 */
static u32
__vxge_hw_device_access_rights_get(u32 host_type, u32 func_id)
{
	u32 access_rights = VXGE_HW_DEVICE_ACCESS_RIGHT_VPATH;

	switch (host_type) {
	case VXGE_HW_NO_MR_NO_SR_NORMAL_FUNCTION:
		if (func_id == 0) {
			access_rights |= VXGE_HW_DEVICE_ACCESS_RIGHT_MRPCIM |
					VXGE_HW_DEVICE_ACCESS_RIGHT_SRPCIM;
		}
		break;
	case VXGE_HW_MR_NO_SR_VH0_BASE_FUNCTION:
		access_rights |= VXGE_HW_DEVICE_ACCESS_RIGHT_MRPCIM |
				VXGE_HW_DEVICE_ACCESS_RIGHT_SRPCIM;
		break;
	case VXGE_HW_NO_MR_SR_VH0_FUNCTION0:
		access_rights |= VXGE_HW_DEVICE_ACCESS_RIGHT_MRPCIM |
				VXGE_HW_DEVICE_ACCESS_RIGHT_SRPCIM;
		break;
	case VXGE_HW_NO_MR_SR_VH0_VIRTUAL_FUNCTION:
	case VXGE_HW_SR_VH_VIRTUAL_FUNCTION:
	case VXGE_HW_MR_SR_VH0_INVALID_CONFIG:
		break;
	case VXGE_HW_SR_VH_FUNCTION0:
	case VXGE_HW_VH_NORMAL_FUNCTION:
		access_rights |= VXGE_HW_DEVICE_ACCESS_RIGHT_SRPCIM;
		break;
	}

	return access_rights;
}
/*
 * __vxge_hw_device_host_info_get
 * This routine returns the host type assignments
 */
void __vxge_hw_device_host_info_get(struct __vxge_hw_device *hldev)
{
	u64 val64;
	u32 i;

	val64 = readq(&hldev->common_reg->host_type_assignments);

	hldev->host_type =
	   (u32)VXGE_HW_HOST_TYPE_ASSIGNMENTS_GET_HOST_TYPE_ASSIGNMENTS(val64);

	hldev->vpath_assignments = readq(&hldev->common_reg->vpath_assignments);

	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpath_assignments & vxge_mBIT(i)))
			continue;

		hldev->func_id =
			__vxge_hw_vpath_func_id_get(i, hldev->vpmgmt_reg[i]);

		hldev->access_rights = __vxge_hw_device_access_rights_get(
			hldev->host_type, hldev->func_id);

		hldev->first_vp_id = i;
		break;
	}

	return;
}

/*
 * __vxge_hw_verify_pci_e_info - Validate the pci-e link parameters such as
 * link width and signalling rate.
 */
static enum vxge_hw_status
__vxge_hw_verify_pci_e_info(struct __vxge_hw_device *hldev)
{
	int exp_cap;
	u16 lnk;

	/* Get the negotiated link width and speed from PCI config space */
	exp_cap = pci_find_capability(hldev->pdev, PCI_CAP_ID_EXP);
	pci_read_config_word(hldev->pdev, exp_cap + PCI_EXP_LNKSTA, &lnk);

	if ((lnk & PCI_EXP_LNKSTA_CLS) != 1)
		return VXGE_HW_ERR_INVALID_PCI_INFO;

	switch ((lnk & PCI_EXP_LNKSTA_NLW) >> 4) {
	case PCIE_LNK_WIDTH_RESRV:
	case PCIE_LNK_X1:
	case PCIE_LNK_X2:
	case PCIE_LNK_X4:
	case PCIE_LNK_X8:
		break;
	default:
		return VXGE_HW_ERR_INVALID_PCI_INFO;
	}

	return VXGE_HW_OK;
}

enum vxge_hw_status
__vxge_hw_device_is_privilaged(struct __vxge_hw_device *hldev)
{
	if ((hldev->host_type == VXGE_HW_NO_MR_NO_SR_NORMAL_FUNCTION ||
	hldev->host_type == VXGE_HW_MR_NO_SR_VH0_BASE_FUNCTION ||
	hldev->host_type == VXGE_HW_NO_MR_SR_VH0_FUNCTION0) &&
	(hldev->func_id == 0))
		return VXGE_HW_OK;
	else
		return VXGE_HW_ERR_PRIVILAGED_OPEARATION;
}

/*
 * vxge_hw_wrr_rebalance - Rebalance the RX_WRR and KDFC_WRR calandars.
 * Rebalance the RX_WRR and KDFC_WRR calandars.
 */
static enum
vxge_hw_status vxge_hw_wrr_rebalance(struct __vxge_hw_device *hldev)
{
	u64 val64;
	u32 wrr_states[VXGE_HW_WEIGHTED_RR_SERVICE_STATES];
	u32 i, j, how_often = 1;
	enum vxge_hw_status status = VXGE_HW_OK;

	status = __vxge_hw_device_is_privilaged(hldev);
	if (status != VXGE_HW_OK)
		goto exit;

	/* Reset the priorities assigned to the WRR arbitration
	phases for the receive traffic */
	for (i = 0; i < VXGE_HW_WRR_RING_COUNT; i++)
		writeq(0, ((&hldev->mrpcim_reg->rx_w_round_robin_0) + i));

	/* Reset the transmit FIFO servicing calendar for FIFOs */
	for (i = 0; i < VXGE_HW_WRR_FIFO_COUNT; i++) {
		writeq(0, ((&hldev->mrpcim_reg->kdfc_w_round_robin_0) + i));
		writeq(0, ((&hldev->mrpcim_reg->kdfc_w_round_robin_20) + i));
	}

	/* Assign WRR priority  0 for all FIFOs */
	for (i = 1; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {
		writeq(VXGE_HW_KDFC_FIFO_0_CTRL_WRR_NUMBER(0),
				((&hldev->mrpcim_reg->kdfc_fifo_0_ctrl)  + i));

		writeq(VXGE_HW_KDFC_FIFO_17_CTRL_WRR_NUMBER(0),
			((&hldev->mrpcim_reg->kdfc_fifo_17_ctrl) + i));
	}

	/* Reset to service non-offload doorbells */
	writeq(0, &hldev->mrpcim_reg->kdfc_entry_type_sel_0);
	writeq(0, &hldev->mrpcim_reg->kdfc_entry_type_sel_1);

	/* Set priority 0 to all receive queues */
	writeq(0, &hldev->mrpcim_reg->rx_queue_priority_0);
	writeq(0, &hldev->mrpcim_reg->rx_queue_priority_1);
	writeq(0, &hldev->mrpcim_reg->rx_queue_priority_2);

	/* Initialize all the slots as unused */
	for (i = 0; i < VXGE_HW_WEIGHTED_RR_SERVICE_STATES; i++)
		wrr_states[i] = -1;

	/* Prepare the Fifo service states */
	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {

		if (!hldev->config.vp_config[i].min_bandwidth)
			continue;

		how_often = VXGE_HW_VPATH_BANDWIDTH_MAX /
				hldev->config.vp_config[i].min_bandwidth;
		if (how_often) {

			for (j = 0; j < VXGE_HW_WRR_FIFO_SERVICE_STATES;) {
				if (wrr_states[j] == -1) {
					wrr_states[j] = i;
					/* Make sure each fifo is serviced
					 * atleast once */
					if (i == j)
						j += VXGE_HW_MAX_VIRTUAL_PATHS;
					else
						j += how_often;
				} else
					j++;
			}
		}
	}

	/* Fill the unused slots with 0 */
	for (j = 0; j < VXGE_HW_WEIGHTED_RR_SERVICE_STATES; j++) {
		if (wrr_states[j] == -1)
			wrr_states[j] = 0;
	}

	/* Assign WRR priority number for FIFOs */
	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {
		writeq(VXGE_HW_KDFC_FIFO_0_CTRL_WRR_NUMBER(i),
				((&hldev->mrpcim_reg->kdfc_fifo_0_ctrl) + i));

		writeq(VXGE_HW_KDFC_FIFO_17_CTRL_WRR_NUMBER(i),
			((&hldev->mrpcim_reg->kdfc_fifo_17_ctrl) + i));
	}

	/* Modify the servicing algorithm applied to the 3 types of doorbells.
	i.e, none-offload, message and offload */
	writeq(VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_0(0) |
				VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_1(0) |
				VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_2(0) |
				VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_3(0) |
				VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_4(1) |
				VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_5(0) |
				VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_6(0) |
				VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_7(0),
				&hldev->mrpcim_reg->kdfc_entry_type_sel_0);

	writeq(VXGE_HW_KDFC_ENTRY_TYPE_SEL_1_NUMBER_8(1),
				&hldev->mrpcim_reg->kdfc_entry_type_sel_1);

	for (i = 0, j = 0; i < VXGE_HW_WRR_FIFO_COUNT; i++) {

		val64 = VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_0(wrr_states[j++]);
		val64 |= VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_1(wrr_states[j++]);
		val64 |= VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_2(wrr_states[j++]);
		val64 |= VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_3(wrr_states[j++]);
		val64 |= VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_4(wrr_states[j++]);
		val64 |= VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_5(wrr_states[j++]);
		val64 |= VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_6(wrr_states[j++]);
		val64 |= VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUMBER_7(wrr_states[j++]);

		writeq(val64, (&hldev->mrpcim_reg->kdfc_w_round_robin_0 + i));
		writeq(val64, (&hldev->mrpcim_reg->kdfc_w_round_robin_20 + i));
	}

	/* Set up the priorities assigned to receive queues */
	writeq(VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_0(0) |
			VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_1(1) |
			VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_2(2) |
			VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_3(3) |
			VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_4(4) |
			VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_5(5) |
			VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_6(6) |
			VXGE_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_7(7),
			&hldev->mrpcim_reg->rx_queue_priority_0);

	writeq(VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_8(8) |
			VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_9(9) |
			VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_10(10) |
			VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_11(11) |
			VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_12(12) |
			VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_13(13) |
			VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_14(14) |
			VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_NUMBER_15(15),
			&hldev->mrpcim_reg->rx_queue_priority_1);

	writeq(VXGE_HW_RX_QUEUE_PRIORITY_2_RX_Q_NUMBER_16(16),
				&hldev->mrpcim_reg->rx_queue_priority_2);

	/* Initialize all the slots as unused */
	for (i = 0; i < VXGE_HW_WEIGHTED_RR_SERVICE_STATES; i++)
		wrr_states[i] = -1;

	/* Prepare the Ring service states */
	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {

		if (!hldev->config.vp_config[i].min_bandwidth)
			continue;

		how_often = VXGE_HW_VPATH_BANDWIDTH_MAX /
				hldev->config.vp_config[i].min_bandwidth;

		if (how_often) {
			for (j = 0; j < VXGE_HW_WRR_RING_SERVICE_STATES;) {
				if (wrr_states[j] == -1) {
					wrr_states[j] = i;
					/* Make sure each ring is
					 * serviced atleast once */
					if (i == j)
						j += VXGE_HW_MAX_VIRTUAL_PATHS;
					else
						j += how_often;
				} else
					j++;
			}
		}
	}

	/* Fill the unused slots with 0 */
	for (j = 0; j < VXGE_HW_WEIGHTED_RR_SERVICE_STATES; j++) {
		if (wrr_states[j] == -1)
			wrr_states[j] = 0;
	}

	for (i = 0, j = 0; i < VXGE_HW_WRR_RING_COUNT; i++) {
		val64 =  VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_0(
				wrr_states[j++]);
		val64 |=  VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_1(
				wrr_states[j++]);
		val64 |=  VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_2(
				wrr_states[j++]);
		val64 |=  VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_3(
				wrr_states[j++]);
		val64 |=  VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_4(
				wrr_states[j++]);
		val64 |=  VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_5(
				wrr_states[j++]);
		val64 |=  VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_6(
				wrr_states[j++]);
		val64 |=  VXGE_HW_RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_7(
				wrr_states[j++]);

		writeq(val64, ((&hldev->mrpcim_reg->rx_w_round_robin_0) + i));
	}
exit:
	return status;
}

/*
 * __vxge_hw_device_initialize
 * Initialize Titan-V hardware.
 */
enum vxge_hw_status __vxge_hw_device_initialize(struct __vxge_hw_device *hldev)
{
	enum vxge_hw_status status = VXGE_HW_OK;

	if (VXGE_HW_OK == __vxge_hw_device_is_privilaged(hldev)) {
		/* Validate the pci-e link width and speed */
		status = __vxge_hw_verify_pci_e_info(hldev);
		if (status != VXGE_HW_OK)
			goto exit;
	}

	vxge_hw_wrr_rebalance(hldev);
exit:
	return status;
}

/**
 * vxge_hw_device_hw_info_get - Get the hw information
 * Returns the vpath mask that has the bits set for each vpath allocated
 * for the driver, FW version information and the first mac addresse for
 * each vpath
 */
enum vxge_hw_status __devinit
vxge_hw_device_hw_info_get(void __iomem *bar0,
			   struct vxge_hw_device_hw_info *hw_info)
{
	u32 i;
	u64 val64;
	struct vxge_hw_toc_reg __iomem *toc;
	struct vxge_hw_mrpcim_reg __iomem *mrpcim_reg;
	struct vxge_hw_common_reg __iomem *common_reg;
	struct vxge_hw_vpath_reg __iomem *vpath_reg;
	struct vxge_hw_vpmgmt_reg __iomem *vpmgmt_reg;
	enum vxge_hw_status status;

	memset(hw_info, 0, sizeof(struct vxge_hw_device_hw_info));

	toc = __vxge_hw_device_toc_get(bar0);
	if (toc == NULL) {
		status = VXGE_HW_ERR_CRITICAL;
		goto exit;
	}

	val64 = readq(&toc->toc_common_pointer);
	common_reg = (struct vxge_hw_common_reg __iomem *)(bar0 + val64);

	status = __vxge_hw_device_vpath_reset_in_prog_check(
		(u64 __iomem *)&common_reg->vpath_rst_in_prog);
	if (status != VXGE_HW_OK)
		goto exit;

	hw_info->vpath_mask = readq(&common_reg->vpath_assignments);

	val64 = readq(&common_reg->host_type_assignments);

	hw_info->host_type =
	   (u32)VXGE_HW_HOST_TYPE_ASSIGNMENTS_GET_HOST_TYPE_ASSIGNMENTS(val64);

	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {

		if (!((hw_info->vpath_mask) & vxge_mBIT(i)))
			continue;

		val64 = readq(&toc->toc_vpmgmt_pointer[i]);

		vpmgmt_reg = (struct vxge_hw_vpmgmt_reg __iomem *)
				(bar0 + val64);

		hw_info->func_id = __vxge_hw_vpath_func_id_get(i, vpmgmt_reg);
		if (__vxge_hw_device_access_rights_get(hw_info->host_type,
			hw_info->func_id) &
			VXGE_HW_DEVICE_ACCESS_RIGHT_MRPCIM) {

			val64 = readq(&toc->toc_mrpcim_pointer);

			mrpcim_reg = (struct vxge_hw_mrpcim_reg __iomem *)
					(bar0 + val64);

			writeq(0, &mrpcim_reg->xgmac_gen_fw_memo_mask);
			wmb();
		}

		val64 = readq(&toc->toc_vpath_pointer[i]);

		vpath_reg = (struct vxge_hw_vpath_reg __iomem *)(bar0 + val64);

		hw_info->function_mode =
			__vxge_hw_vpath_pci_func_mode_get(i, vpath_reg);

		status = __vxge_hw_vpath_fw_ver_get(i, vpath_reg, hw_info);
		if (status != VXGE_HW_OK)
			goto exit;

		status = __vxge_hw_vpath_card_info_get(i, vpath_reg, hw_info);
		if (status != VXGE_HW_OK)
			goto exit;

		break;
	}

	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {

		if (!((hw_info->vpath_mask) & vxge_mBIT(i)))
			continue;

		val64 = readq(&toc->toc_vpath_pointer[i]);
		vpath_reg = (struct vxge_hw_vpath_reg __iomem *)(bar0 + val64);

		status =  __vxge_hw_vpath_addr_get(i, vpath_reg,
				hw_info->mac_addrs[i],
				hw_info->mac_addr_masks[i]);
		if (status != VXGE_HW_OK)
			goto exit;
	}
exit:
	return status;
}

/*
 * vxge_hw_device_initialize - Initialize Titan device.
 * Initialize Titan device. Note that all the arguments of this public API
 * are 'IN', including @hldev. Driver cooperates with
 * OS to find new Titan device, locate its PCI and memory spaces.
 *
 * When done, the driver allocates sizeof(struct __vxge_hw_device) bytes for HW
 * to enable the latter to perform Titan hardware initialization.
 */
enum vxge_hw_status __devinit
vxge_hw_device_initialize(
	struct __vxge_hw_device **devh,
	struct vxge_hw_device_attr *attr,
	struct vxge_hw_device_config *device_config)
{
	u32 i;
	u32 nblocks = 0;
	struct __vxge_hw_device *hldev = NULL;
	enum vxge_hw_status status = VXGE_HW_OK;

	status = __vxge_hw_device_config_check(device_config);
	if (status != VXGE_HW_OK)
		goto exit;

	hldev = (struct __vxge_hw_device *)
			vmalloc(sizeof(struct __vxge_hw_device));
	if (hldev == NULL) {
		status = VXGE_HW_ERR_OUT_OF_MEMORY;
		goto exit;
	}

	memset(hldev, 0, sizeof(struct __vxge_hw_device));
	hldev->magic = VXGE_HW_DEVICE_MAGIC;

	vxge_hw_device_debug_set(hldev, VXGE_ERR, VXGE_COMPONENT_ALL);

	/* apply config */
	memcpy(&hldev->config, device_config,
		sizeof(struct vxge_hw_device_config));

	hldev->bar0 = attr->bar0;
	hldev->pdev = attr->pdev;

	hldev->uld_callbacks.link_up = attr->uld_callbacks.link_up;
	hldev->uld_callbacks.link_down = attr->uld_callbacks.link_down;
	hldev->uld_callbacks.crit_err = attr->uld_callbacks.crit_err;

	__vxge_hw_device_pci_e_init(hldev);

	status = __vxge_hw_device_reg_addr_get(hldev);
	if (status != VXGE_HW_OK)
		goto exit;
	__vxge_hw_device_id_get(hldev);

	__vxge_hw_device_host_info_get(hldev);

	/* Incrementing for stats blocks */
	nblocks++;

	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpath_assignments & vxge_mBIT(i)))
			continue;

		if (device_config->vp_config[i].ring.enable ==
			VXGE_HW_RING_ENABLE)
			nblocks += device_config->vp_config[i].ring.ring_blocks;

		if (device_config->vp_config[i].fifo.enable ==
			VXGE_HW_FIFO_ENABLE)
			nblocks += device_config->vp_config[i].fifo.fifo_blocks;
		nblocks++;
	}

	if (__vxge_hw_blockpool_create(hldev,
		&hldev->block_pool,
		device_config->dma_blockpool_initial + nblocks,
		device_config->dma_blockpool_max + nblocks) != VXGE_HW_OK) {

		vxge_hw_device_terminate(hldev);
		status = VXGE_HW_ERR_OUT_OF_MEMORY;
		goto exit;
	}

	status = __vxge_hw_device_initialize(hldev);

	if (status != VXGE_HW_OK) {
		vxge_hw_device_terminate(hldev);
		goto exit;
	}

	*devh = hldev;
exit:
	return status;
}

/*
 * vxge_hw_device_terminate - Terminate Titan device.
 * Terminate HW device.
 */
void
vxge_hw_device_terminate(struct __vxge_hw_device *hldev)
{
	vxge_assert(hldev->magic == VXGE_HW_DEVICE_MAGIC);

	hldev->magic = VXGE_HW_DEVICE_DEAD;
	__vxge_hw_blockpool_destroy(&hldev->block_pool);
	vfree(hldev);
}

/*
 * vxge_hw_device_stats_get - Get the device hw statistics.
 * Returns the vpath h/w stats for the device.
 */
enum vxge_hw_status
vxge_hw_device_stats_get(struct __vxge_hw_device *hldev,
			struct vxge_hw_device_stats_hw_info *hw_stats)
{
	u32 i;
	enum vxge_hw_status status = VXGE_HW_OK;

	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & vxge_mBIT(i)) ||
			(hldev->virtual_paths[i].vp_open ==
				VXGE_HW_VP_NOT_OPEN))
			continue;

		memcpy(hldev->virtual_paths[i].hw_stats_sav,
				hldev->virtual_paths[i].hw_stats,
				sizeof(struct vxge_hw_vpath_stats_hw_info));

		status = __vxge_hw_vpath_stats_get(
			&hldev->virtual_paths[i],
			hldev->virtual_paths[i].hw_stats);
	}

	memcpy(hw_stats, &hldev->stats.hw_dev_info_stats,
			sizeof(struct vxge_hw_device_stats_hw_info));

	return status;
}

/*
 * vxge_hw_driver_stats_get - Get the device sw statistics.
 * Returns the vpath s/w stats for the device.
 */
enum vxge_hw_status vxge_hw_driver_stats_get(
			struct __vxge_hw_device *hldev,
			struct vxge_hw_device_stats_sw_info *sw_stats)
{
	enum vxge_hw_status status = VXGE_HW_OK;

	memcpy(sw_stats, &hldev->stats.sw_dev_info_stats,
		sizeof(struct vxge_hw_device_stats_sw_info));

	return status;
}

/*
 * vxge_hw_mrpcim_stats_access - Access the statistics from the given location
 *                           and offset and perform an operation
 * Get the statistics from the given location and offset.
 */
enum vxge_hw_status
vxge_hw_mrpcim_stats_access(struct __vxge_hw_device *hldev,
			    u32 operation, u32 location, u32 offset, u64 *stat)
{
	u64 val64;
	enum vxge_hw_status status = VXGE_HW_OK;

	status = __vxge_hw_device_is_privilaged(hldev);
	if (status != VXGE_HW_OK)
		goto exit;

	val64 = VXGE_HW_XMAC_STATS_SYS_CMD_OP(operation) |
		VXGE_HW_XMAC_STATS_SYS_CMD_STROBE |
		VXGE_HW_XMAC_STATS_SYS_CMD_LOC_SEL(location) |
		VXGE_HW_XMAC_STATS_SYS_CMD_OFFSET_SEL(offset);

	status = __vxge_hw_pio_mem_write64(val64,
				&hldev->mrpcim_reg->xmac_stats_sys_cmd,
				VXGE_HW_XMAC_STATS_SYS_CMD_STROBE,
				hldev->config.device_poll_millis);

	if ((status == VXGE_HW_OK) && (operation == VXGE_HW_STATS_OP_READ))
		*stat = readq(&hldev->mrpcim_reg->xmac_stats_sys_data);
	else
		*stat = 0;
exit:
	return status;
}

/*
 * vxge_hw_device_xmac_aggr_stats_get - Get the Statistics on aggregate port
 * Get the Statistics on aggregate port
 */
enum vxge_hw_status
vxge_hw_device_xmac_aggr_stats_get(struct __vxge_hw_device *hldev, u32 port,
				   struct vxge_hw_xmac_aggr_stats *aggr_stats)
{
	u64 *val64;
	int i;
	u32 offset = VXGE_HW_STATS_AGGRn_OFFSET;
	enum vxge_hw_status status = VXGE_HW_OK;

	val64 = (u64 *)aggr_stats;

	status = __vxge_hw_device_is_privilaged(hldev);
	if (status != VXGE_HW_OK)
		goto exit;

	for (i = 0; i < sizeof(struct vxge_hw_xmac_aggr_stats) / 8; i++) {
		status = vxge_hw_mrpcim_stats_access(hldev,
					VXGE_HW_STATS_OP_READ,
					VXGE_HW_STATS_LOC_AGGR,
					((offset + (104 * port)) >> 3), val64);
		if (status != VXGE_HW_OK)
			goto exit;

		offset += 8;
		val64++;
	}
exit:
	return status;
}

/*
 * vxge_hw_device_xmac_port_stats_get - Get the Statistics on a port
 * Get the Statistics on port
 */
enum vxge_hw_status
vxge_hw_device_xmac_port_stats_get(struct __vxge_hw_device *hldev, u32 port,
				   struct vxge_hw_xmac_port_stats *port_stats)
{
	u64 *val64;
	enum vxge_hw_status status = VXGE_HW_OK;
	int i;
	u32 offset = 0x0;
	val64 = (u64 *) port_stats;

	status = __vxge_hw_device_is_privilaged(hldev);
	if (status != VXGE_HW_OK)
		goto exit;

	for (i = 0; i < sizeof(struct vxge_hw_xmac_port_stats) / 8; i++) {
		status = vxge_hw_mrpcim_stats_access(hldev,
					VXGE_HW_STATS_OP_READ,
					VXGE_HW_STATS_LOC_AGGR,
					((offset + (608 * port)) >> 3), val64);
		if (status != VXGE_HW_OK)
			goto exit;

		offset += 8;
		val64++;
	}

exit:
	return status;
}

/*
 * vxge_hw_device_xmac_stats_get - Get the XMAC Statistics
 * Get the XMAC Statistics
 */
enum vxge_hw_status
vxge_hw_device_xmac_stats_get(struct __vxge_hw_device *hldev,
			      struct vxge_hw_xmac_stats *xmac_stats)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	u32 i;

	status = vxge_hw_device_xmac_aggr_stats_get(hldev,
					0, &xmac_stats->aggr_stats[0]);

	if (status != VXGE_HW_OK)
		goto exit;

	status = vxge_hw_device_xmac_aggr_stats_get(hldev,
				1, &xmac_stats->aggr_stats[1]);
	if (status != VXGE_HW_OK)
		goto exit;

	for (i = 0; i <= VXGE_HW_MAC_MAX_MAC_PORT_ID; i++) {

		status = vxge_hw_device_xmac_port_stats_get(hldev,
					i, &xmac_stats->port_stats[i]);
		if (status != VXGE_HW_OK)
			goto exit;
	}

	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & vxge_mBIT(i)))
			continue;

		status = __vxge_hw_vpath_xmac_tx_stats_get(
					&hldev->virtual_paths[i],
					&xmac_stats->vpath_tx_stats[i]);
		if (status != VXGE_HW_OK)
			goto exit;

		status = __vxge_hw_vpath_xmac_rx_stats_get(
					&hldev->virtual_paths[i],
					&xmac_stats->vpath_rx_stats[i]);
		if (status != VXGE_HW_OK)
			goto exit;
	}
exit:
	return status;
}

/*
 * vxge_hw_device_debug_set - Set the debug module, level and timestamp
 * This routine is used to dynamically change the debug output
 */
void vxge_hw_device_debug_set(struct __vxge_hw_device *hldev,
			      enum vxge_debug_level level, u32 mask)
{
	if (hldev == NULL)
		return;

#if defined(VXGE_DEBUG_TRACE_MASK) || \
	defined(VXGE_DEBUG_ERR_MASK)
	hldev->debug_module_mask = mask;
	hldev->debug_level = level;
#endif

#if defined(VXGE_DEBUG_ERR_MASK)
	hldev->level_err = level & VXGE_ERR;
#endif

#if defined(VXGE_DEBUG_TRACE_MASK)
	hldev->level_trace = level & VXGE_TRACE;
#endif
}

/*
 * vxge_hw_device_error_level_get - Get the error level
 * This routine returns the current error level set
 */
u32 vxge_hw_device_error_level_get(struct __vxge_hw_device *hldev)
{
#if defined(VXGE_DEBUG_ERR_MASK)
	if (hldev == NULL)
		return VXGE_ERR;
	else
		return hldev->level_err;
#else
	return 0;
#endif
}

/*
 * vxge_hw_device_trace_level_get - Get the trace level
 * This routine returns the current trace level set
 */
u32 vxge_hw_device_trace_level_get(struct __vxge_hw_device *hldev)
{
#if defined(VXGE_DEBUG_TRACE_MASK)
	if (hldev == NULL)
		return VXGE_TRACE;
	else
		return hldev->level_trace;
#else
	return 0;
#endif
}
/*
 * vxge_hw_device_debug_mask_get - Get the debug mask
 * This routine returns the current debug mask set
 */
u32 vxge_hw_device_debug_mask_get(struct __vxge_hw_device *hldev)
{
#if defined(VXGE_DEBUG_TRACE_MASK) || defined(VXGE_DEBUG_ERR_MASK)
	if (hldev == NULL)
		return 0;
	return hldev->debug_module_mask;
#else
	return 0;
#endif
}

/*
 * vxge_hw_getpause_data -Pause frame frame generation and reception.
 * Returns the Pause frame generation and reception capability of the NIC.
 */
enum vxge_hw_status vxge_hw_device_getpause_data(struct __vxge_hw_device *hldev,
						 u32 port, u32 *tx, u32 *rx)
{
	u64 val64;
	enum vxge_hw_status status = VXGE_HW_OK;

	if ((hldev == NULL) || (hldev->magic != VXGE_HW_DEVICE_MAGIC)) {
		status = VXGE_HW_ERR_INVALID_DEVICE;
		goto exit;
	}

	if (port > VXGE_HW_MAC_MAX_MAC_PORT_ID) {
		status = VXGE_HW_ERR_INVALID_PORT;
		goto exit;
	}

	if (!(hldev->access_rights & VXGE_HW_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		status = VXGE_HW_ERR_PRIVILAGED_OPEARATION;
		goto exit;
	}

	val64 = readq(&hldev->mrpcim_reg->rxmac_pause_cfg_port[port]);
	if (val64 & VXGE_HW_RXMAC_PAUSE_CFG_PORT_GEN_EN)
		*tx = 1;
	if (val64 & VXGE_HW_RXMAC_PAUSE_CFG_PORT_RCV_EN)
		*rx = 1;
exit:
	return status;
}

/*
 * vxge_hw_device_setpause_data -  set/reset pause frame generation.
 * It can be used to set or reset Pause frame generation or reception
 * support of the NIC.
 */

enum vxge_hw_status vxge_hw_device_setpause_data(struct __vxge_hw_device *hldev,
						 u32 port, u32 tx, u32 rx)
{
	u64 val64;
	enum vxge_hw_status status = VXGE_HW_OK;

	if ((hldev == NULL) || (hldev->magic != VXGE_HW_DEVICE_MAGIC)) {
		status = VXGE_HW_ERR_INVALID_DEVICE;
		goto exit;
	}

	if (port > VXGE_HW_MAC_MAX_MAC_PORT_ID) {
		status = VXGE_HW_ERR_INVALID_PORT;
		goto exit;
	}

	status = __vxge_hw_device_is_privilaged(hldev);
	if (status != VXGE_HW_OK)
		goto exit;

	val64 = readq(&hldev->mrpcim_reg->rxmac_pause_cfg_port[port]);
	if (tx)
		val64 |= VXGE_HW_RXMAC_PAUSE_CFG_PORT_GEN_EN;
	else
		val64 &= ~VXGE_HW_RXMAC_PAUSE_CFG_PORT_GEN_EN;
	if (rx)
		val64 |= VXGE_HW_RXMAC_PAUSE_CFG_PORT_RCV_EN;
	else
		val64 &= ~VXGE_HW_RXMAC_PAUSE_CFG_PORT_RCV_EN;

	writeq(val64, &hldev->mrpcim_reg->rxmac_pause_cfg_port[port]);
exit:
	return status;
}

u16 vxge_hw_device_link_width_get(struct __vxge_hw_device *hldev)
{
	int link_width, exp_cap;
	u16 lnk;

	exp_cap = pci_find_capability(hldev->pdev, PCI_CAP_ID_EXP);
	pci_read_config_word(hldev->pdev, exp_cap + PCI_EXP_LNKSTA, &lnk);
	link_width = (lnk & VXGE_HW_PCI_EXP_LNKCAP_LNK_WIDTH) >> 4;
	return link_width;
}

/*
 * __vxge_hw_ring_block_memblock_idx - Return the memblock index
 * This function returns the index of memory block
 */
static inline u32
__vxge_hw_ring_block_memblock_idx(u8 *block)
{
	return (u32)*((u64 *)(block + VXGE_HW_RING_MEMBLOCK_IDX_OFFSET));
}

/*
 * __vxge_hw_ring_block_memblock_idx_set - Sets the memblock index
 * This function sets index to a memory block
 */
static inline void
__vxge_hw_ring_block_memblock_idx_set(u8 *block, u32 memblock_idx)
{
	*((u64 *)(block + VXGE_HW_RING_MEMBLOCK_IDX_OFFSET)) = memblock_idx;
}

/*
 * __vxge_hw_ring_block_next_pointer_set - Sets the next block pointer
 * in RxD block
 * Sets the next block pointer in RxD block
 */
static inline void
__vxge_hw_ring_block_next_pointer_set(u8 *block, dma_addr_t dma_next)
{
	*((u64 *)(block + VXGE_HW_RING_NEXT_BLOCK_POINTER_OFFSET)) = dma_next;
}

/*
 * __vxge_hw_ring_first_block_address_get - Returns the dma address of the
 *             first block
 * Returns the dma address of the first RxD block
 */
u64 __vxge_hw_ring_first_block_address_get(struct __vxge_hw_ring *ring)
{
	struct vxge_hw_mempool_dma *dma_object;

	dma_object = ring->mempool->memblocks_dma_arr;
	vxge_assert(dma_object != NULL);

	return dma_object->addr;
}

/*
 * __vxge_hw_ring_item_dma_addr - Return the dma address of an item
 * This function returns the dma address of a given item
 */
static dma_addr_t __vxge_hw_ring_item_dma_addr(struct vxge_hw_mempool *mempoolh,
					       void *item)
{
	u32 memblock_idx;
	void *memblock;
	struct vxge_hw_mempool_dma *memblock_dma_object;
	ptrdiff_t dma_item_offset;

	/* get owner memblock index */
	memblock_idx = __vxge_hw_ring_block_memblock_idx(item);

	/* get owner memblock by memblock index */
	memblock = mempoolh->memblocks_arr[memblock_idx];

	/* get memblock DMA object by memblock index */
	memblock_dma_object = mempoolh->memblocks_dma_arr + memblock_idx;

	/* calculate offset in the memblock of this item */
	dma_item_offset = (u8 *)item - (u8 *)memblock;

	return memblock_dma_object->addr + dma_item_offset;
}

/*
 * __vxge_hw_ring_rxdblock_link - Link the RxD blocks
 * This function returns the dma address of a given item
 */
static void __vxge_hw_ring_rxdblock_link(struct vxge_hw_mempool *mempoolh,
					 struct __vxge_hw_ring *ring, u32 from,
					 u32 to)
{
	u8 *to_item , *from_item;
	dma_addr_t to_dma;

	/* get "from" RxD block */
	from_item = mempoolh->items_arr[from];
	vxge_assert(from_item);

	/* get "to" RxD block */
	to_item = mempoolh->items_arr[to];
	vxge_assert(to_item);

	/* return address of the beginning of previous RxD block */
	to_dma = __vxge_hw_ring_item_dma_addr(mempoolh, to_item);

	/* set next pointer for this RxD block to point on
	 * previous item's DMA start address */
	__vxge_hw_ring_block_next_pointer_set(from_item, to_dma);
}

/*
 * __vxge_hw_ring_mempool_item_alloc - Allocate List blocks for RxD
 * block callback
 * This function is callback passed to __vxge_hw_mempool_create to create memory
 * pool for RxD block
 */
static void
__vxge_hw_ring_mempool_item_alloc(struct vxge_hw_mempool *mempoolh,
				  u32 memblock_index,
				  struct vxge_hw_mempool_dma *dma_object,
				  u32 index, u32 is_last)
{
	u32 i;
	void *item = mempoolh->items_arr[index];
	struct __vxge_hw_ring *ring =
		(struct __vxge_hw_ring *)mempoolh->userdata;

	/* format rxds array */
	for (i = 0; i < ring->rxds_per_block; i++) {
		void *rxdblock_priv;
		void *uld_priv;
		struct vxge_hw_ring_rxd_1 *rxdp;

		u32 reserve_index = ring->channel.reserve_ptr -
				(index * ring->rxds_per_block + i + 1);
		u32 memblock_item_idx;

		ring->channel.reserve_arr[reserve_index] = ((u8 *)item) +
						i * ring->rxd_size;

		/* Note: memblock_item_idx is index of the item within
		 *       the memblock. For instance, in case of three RxD-blocks
		 *       per memblock this value can be 0, 1 or 2. */
		rxdblock_priv = __vxge_hw_mempool_item_priv(mempoolh,
					memblock_index, item,
					&memblock_item_idx);

		rxdp = (struct vxge_hw_ring_rxd_1 *)
				ring->channel.reserve_arr[reserve_index];

		uld_priv = ((u8 *)rxdblock_priv + ring->rxd_priv_size * i);

		/* pre-format Host_Control */
		rxdp->host_control = (u64)(size_t)uld_priv;
	}

	__vxge_hw_ring_block_memblock_idx_set(item, memblock_index);

	if (is_last) {
		/* link last one with first one */
		__vxge_hw_ring_rxdblock_link(mempoolh, ring, index, 0);
	}

	if (index > 0) {
		/* link this RxD block with previous one */
		__vxge_hw_ring_rxdblock_link(mempoolh, ring, index - 1, index);
	}

	return;
}

/*
 * __vxge_hw_ring_initial_replenish - Initial replenish of RxDs
 * This function replenishes the RxDs from reserve array to work array
 */
enum vxge_hw_status
vxge_hw_ring_replenish(struct __vxge_hw_ring *ring, u16 min_flag)
{
	void *rxd;
	int i = 0;
	struct __vxge_hw_channel *channel;
	enum vxge_hw_status status = VXGE_HW_OK;

	channel = &ring->channel;

	while (vxge_hw_channel_dtr_count(channel) > 0) {

		status = vxge_hw_ring_rxd_reserve(ring, &rxd);

		vxge_assert(status == VXGE_HW_OK);

		if (ring->rxd_init) {
			status = ring->rxd_init(rxd, channel->userdata);
			if (status != VXGE_HW_OK) {
				vxge_hw_ring_rxd_free(ring, rxd);
				goto exit;
			}
		}

		vxge_hw_ring_rxd_post(ring, rxd);
		if (min_flag) {
			i++;
			if (i == VXGE_HW_RING_MIN_BUFF_ALLOCATION)
				break;
		}
	}
	status = VXGE_HW_OK;
exit:
	return status;
}

/*
 * __vxge_hw_ring_create - Create a Ring
 * This function creates Ring and initializes it.
 *
 */
enum vxge_hw_status
__vxge_hw_ring_create(struct __vxge_hw_vpath_handle *vp,
		      struct vxge_hw_ring_attr *attr)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	struct __vxge_hw_ring *ring;
	u32 ring_length;
	struct vxge_hw_ring_config *config;
	struct __vxge_hw_device *hldev;
	u32 vp_id;
	struct vxge_hw_mempool_cbs ring_mp_callback;

	if ((vp == NULL) || (attr == NULL)) {
		status = VXGE_HW_FAIL;
		goto exit;
	}

	hldev = vp->vpath->hldev;
	vp_id = vp->vpath->vp_id;

	config = &hldev->config.vp_config[vp_id].ring;

	ring_length = config->ring_blocks *
			vxge_hw_ring_rxds_per_block_get(config->buffer_mode);

	ring = (struct __vxge_hw_ring *)__vxge_hw_channel_allocate(vp,
						VXGE_HW_CHANNEL_TYPE_RING,
						ring_length,
						attr->per_rxd_space,
						attr->userdata);

	if (ring == NULL) {
		status = VXGE_HW_ERR_OUT_OF_MEMORY;
		goto exit;
	}

	vp->vpath->ringh = ring;
	ring->vp_id = vp_id;
	ring->vp_reg = vp->vpath->vp_reg;
	ring->common_reg = hldev->common_reg;
	ring->stats = &vp->vpath->sw_stats->ring_stats;
	ring->config = config;
	ring->callback = attr->callback;
	ring->rxd_init = attr->rxd_init;
	ring->rxd_term = attr->rxd_term;
	ring->buffer_mode = config->buffer_mode;
	ring->rxds_limit = config->rxds_limit;

	ring->rxd_size = vxge_hw_ring_rxd_size_get(config->buffer_mode);
	ring->rxd_priv_size =
		sizeof(struct __vxge_hw_ring_rxd_priv) + attr->per_rxd_space;
	ring->per_rxd_space = attr->per_rxd_space;

	ring->rxd_priv_size =
		((ring->rxd_priv_size + VXGE_CACHE_LINE_SIZE - 1) /
		VXGE_CACHE_LINE_SIZE) * VXGE_CACHE_LINE_SIZE;

	/* how many RxDs can fit into one block. Depends on configured
	 * buffer_mode. */
	ring->rxds_per_block =
		vxge_hw_ring_rxds_per_block_get(config->buffer_mode);

	/* calculate actual RxD block private size */
	ring->rxdblock_priv_size = ring->rxd_priv_size * ring->rxds_per_block;
	ring_mp_callback.item_func_alloc = __vxge_hw_ring_mempool_item_alloc;
	ring->mempool = __vxge_hw_mempool_create(hldev,
				VXGE_HW_BLOCK_SIZE,
				VXGE_HW_BLOCK_SIZE,
				ring->rxdblock_priv_size,
				ring->config->ring_blocks,
				ring->config->ring_blocks,
				&ring_mp_callback,
				ring);

	if (ring->mempool == NULL) {
		__vxge_hw_ring_delete(vp);
		return VXGE_HW_ERR_OUT_OF_MEMORY;
	}

	status = __vxge_hw_channel_initialize(&ring->channel);
	if (status != VXGE_HW_OK) {
		__vxge_hw_ring_delete(vp);
		goto exit;
	}

	/* Note:
	 * Specifying rxd_init callback means two things:
	 * 1) rxds need to be initialized by driver at channel-open time;
	 * 2) rxds need to be posted at channel-open time
	 *    (that's what the initial_replenish() below does)
	 * Currently we don't have a case when the 1) is done without the 2).
	 */
	if (ring->rxd_init) {
		status = vxge_hw_ring_replenish(ring, 1);
		if (status != VXGE_HW_OK) {
			__vxge_hw_ring_delete(vp);
			goto exit;
		}
	}

	/* initial replenish will increment the counter in its post() routine,
	 * we have to reset it */
	ring->stats->common_stats.usage_cnt = 0;
exit:
	return status;
}

/*
 * __vxge_hw_ring_abort - Returns the RxD
 * This function terminates the RxDs of ring
 */
enum vxge_hw_status __vxge_hw_ring_abort(struct __vxge_hw_ring *ring)
{
	void *rxdh;
	struct __vxge_hw_channel *channel;

	channel = &ring->channel;

	for (;;) {
		vxge_hw_channel_dtr_try_complete(channel, &rxdh);

		if (rxdh == NULL)
			break;

		vxge_hw_channel_dtr_complete(channel);

		if (ring->rxd_term)
			ring->rxd_term(rxdh, VXGE_HW_RXD_STATE_POSTED,
				channel->userdata);

		vxge_hw_channel_dtr_free(channel, rxdh);
	}

	return VXGE_HW_OK;
}

/*
 * __vxge_hw_ring_reset - Resets the ring
 * This function resets the ring during vpath reset operation
 */
enum vxge_hw_status __vxge_hw_ring_reset(struct __vxge_hw_ring *ring)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	struct __vxge_hw_channel *channel;

	channel = &ring->channel;

	__vxge_hw_ring_abort(ring);

	status = __vxge_hw_channel_reset(channel);

	if (status != VXGE_HW_OK)
		goto exit;

	if (ring->rxd_init) {
		status = vxge_hw_ring_replenish(ring, 1);
		if (status != VXGE_HW_OK)
			goto exit;
	}
exit:
	return status;
}

/*
 * __vxge_hw_ring_delete - Removes the ring
 * This function freeup the memory pool and removes the ring
 */
enum vxge_hw_status __vxge_hw_ring_delete(struct __vxge_hw_vpath_handle *vp)
{
	struct __vxge_hw_ring *ring = vp->vpath->ringh;

	__vxge_hw_ring_abort(ring);

	if (ring->mempool)
		__vxge_hw_mempool_destroy(ring->mempool);

	vp->vpath->ringh = NULL;
	__vxge_hw_channel_free(&ring->channel);

	return VXGE_HW_OK;
}

/*
 * __vxge_hw_mempool_grow
 * Will resize mempool up to %num_allocate value.
 */
enum vxge_hw_status
__vxge_hw_mempool_grow(struct vxge_hw_mempool *mempool, u32 num_allocate,
		       u32 *num_allocated)
{
	u32 i, first_time = mempool->memblocks_allocated == 0 ? 1 : 0;
	u32 n_items = mempool->items_per_memblock;
	u32 start_block_idx = mempool->memblocks_allocated;
	u32 end_block_idx = mempool->memblocks_allocated + num_allocate;
	enum vxge_hw_status status = VXGE_HW_OK;

	*num_allocated = 0;

	if (end_block_idx > mempool->memblocks_max) {
		status = VXGE_HW_ERR_OUT_OF_MEMORY;
		goto exit;
	}

	for (i = start_block_idx; i < end_block_idx; i++) {
		u32 j;
		u32 is_last = ((end_block_idx - 1) == i);
		struct vxge_hw_mempool_dma *dma_object =
			mempool->memblocks_dma_arr + i;
		void *the_memblock;

		/* allocate memblock's private part. Each DMA memblock
		 * has a space allocated for item's private usage upon
		 * mempool's user request. Each time mempool grows, it will
		 * allocate new memblock and its private part at once.
		 * This helps to minimize memory usage a lot. */
		mempool->memblocks_priv_arr[i] =
				vmalloc(mempool->items_priv_size * n_items);
		if (mempool->memblocks_priv_arr[i] == NULL) {
			status = VXGE_HW_ERR_OUT_OF_MEMORY;
			goto exit;
		}

		memset(mempool->memblocks_priv_arr[i], 0,
			     mempool->items_priv_size * n_items);

		/* allocate DMA-capable memblock */
		mempool->memblocks_arr[i] =
			__vxge_hw_blockpool_malloc(mempool->devh,
				mempool->memblock_size, dma_object);
		if (mempool->memblocks_arr[i] == NULL) {
			vfree(mempool->memblocks_priv_arr[i]);
			status = VXGE_HW_ERR_OUT_OF_MEMORY;
			goto exit;
		}

		(*num_allocated)++;
		mempool->memblocks_allocated++;

		memset(mempool->memblocks_arr[i], 0, mempool->memblock_size);

		the_memblock = mempool->memblocks_arr[i];

		/* fill the items hash array */
		for (j = 0; j < n_items; j++) {
			u32 index = i * n_items + j;

			if (first_time && index >= mempool->items_initial)
				break;

			mempool->items_arr[index] =
				((char *)the_memblock + j*mempool->item_size);

			/* let caller to do more job on each item */
			if (mempool->item_func_alloc != NULL)
				mempool->item_func_alloc(mempool, i,
					dma_object, index, is_last);

			mempool->items_current = index + 1;
		}

		if (first_time && mempool->items_current ==
					mempool->items_initial)
			break;
	}
exit:
	return status;
}

/*
 * vxge_hw_mempool_create
 * This function will create memory pool object. Pool may grow but will
 * never shrink. Pool consists of number of dynamically allocated blocks
 * with size enough to hold %items_initial number of items. Memory is
 * DMA-able but client must map/unmap before interoperating with the device.
 */
struct vxge_hw_mempool*
__vxge_hw_mempool_create(
	struct __vxge_hw_device *devh,
	u32 memblock_size,
	u32 item_size,
	u32 items_priv_size,
	u32 items_initial,
	u32 items_max,
	struct vxge_hw_mempool_cbs *mp_callback,
	void *userdata)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	u32 memblocks_to_allocate;
	struct vxge_hw_mempool *mempool = NULL;
	u32 allocated;

	if (memblock_size < item_size) {
		status = VXGE_HW_FAIL;
		goto exit;
	}

	mempool = (struct vxge_hw_mempool *)
			vmalloc(sizeof(struct vxge_hw_mempool));
	if (mempool == NULL) {
		status = VXGE_HW_ERR_OUT_OF_MEMORY;
		goto exit;
	}
	memset(mempool, 0, sizeof(struct vxge_hw_mempool));

	mempool->devh			= devh;
	mempool->memblock_size		= memblock_size;
	mempool->items_max		= items_max;
	mempool->items_initial		= items_initial;
	mempool->item_size		= item_size;
	mempool->items_priv_size	= items_priv_size;
	mempool->item_func_alloc	= mp_callback->item_func_alloc;
	mempool->userdata		= userdata;

	mempool->memblocks_allocated = 0;

	mempool->items_per_memblock = memblock_size / item_size;

	mempool->memblocks_max = (items_max + mempool->items_per_memblock - 1) /
					mempool->items_per_memblock;

	/* allocate array of memblocks */
	mempool->memblocks_arr =
		(void **) vmalloc(sizeof(void *) * mempool->memblocks_max);
	if (mempool->memblocks_arr == NULL) {
		__vxge_hw_mempool_destroy(mempool);
		status = VXGE_HW_ERR_OUT_OF_MEMORY;
		mempool = NULL;
		goto exit;
	}
	memset(mempool->memblocks_arr, 0,
		sizeof(void *) * mempool->memblocks_max);

	/* allocate array of private parts of items per memblocks */
	mempool->memblocks_priv_arr =
		(void **) vmalloc(sizeof(void *) * mempool->memblocks_max);
	if (mempool->memblocks_priv_arr == NULL) {
		__vxge_hw_mempool_destroy(mempool);
		status = VXGE_HW_ERR_OUT_OF_MEMORY;
		mempool = NULL;
		goto exit;
	}
	memset(mempool->memblocks_priv_arr, 0,
		    sizeof(void *) * mempool->memblocks_max);

	/* allocate array of memblocks DMA objects */
	mempool->memblocks_dma_arr = (struct vxge_hw_mempool_dma *)
		vmalloc(sizeof(struct vxge_hw_mempool_dma) *
			mempool->memblocks_max);

	if (mempool->memblocks_dma_arr == NULL) {
		__vxge_hw_mempool_destroy(mempool);
		status = VXGE_HW_ERR_OUT_OF_MEMORY;
		mempool = NULL;
		goto exit;
	}
	memset(mempool->memblocks_dma_arr, 0,
			sizeof(struct vxge_hw_mempool_dma) *
			mempool->memblocks_max);

	/* allocate hash array of items */
	mempool->items_arr =
		(void **) vmalloc(sizeof(void *) * mempool->items_max);
	if (mempool->items_arr == NULL) {
		__vxge_hw_mempool_destroy(mempool);
		status = VXGE_HW_ERR_OUT_OF_MEMORY;
		mempool = NULL;
		goto exit;
	}
	memset(mempool->items_arr, 0, sizeof(void *) * mempool->items_max);

	/* calculate initial number of memblocks */
	memblocks_to_allocate = (mempool->items_initial +
				 mempool->items_per_memblock - 1) /
						mempool->items_per_memblock;

	/* pre-allocate the mempool */
	status = __vxge_hw_mempool_grow(mempool, memblocks_to_allocate,
					&allocated);
	if (status != VXGE_HW_OK) {
		__vxge_hw_mempool_destroy(mempool);
		status = VXGE_HW_ERR_OUT_OF_MEMORY;
		mempool = NULL;
		goto exit;
	}

exit:
	return mempool;
}

/*
 * vxge_hw_mempool_destroy
 */
void __vxge_hw_mempool_destroy(struct vxge_hw_mempool *mempool)
{
	u32 i, j;
	struct __vxge_hw_device *devh = mempool->devh;

	for (i = 0; i < mempool->memblocks_allocated; i++) {
		struct vxge_hw_mempool_dma *dma_object;

		vxge_assert(mempool->memblocks_arr[i]);
		vxge_assert(mempool->memblocks_dma_arr + i);

		dma_object = mempool->memblocks_dma_arr + i;

		for (j = 0; j < mempool->items_per_memblock; j++) {
			u32 index = i * mempool->items_per_memblock + j;

			/* to skip last partially filled(if any) memblock */
			if (index >= mempool->items_current)
				break;
		}

		vfree(mempool->memblocks_priv_arr[i]);

		__vxge_hw_blockpool_free(devh, mempool->memblocks_arr[i],
				mempool->memblock_size, dma_object);
	}

	vfree(mempool->items_arr);

	vfree(mempool->memblocks_dma_arr);

	vfree(mempool->memblocks_priv_arr);

	vfree(mempool->memblocks_arr);

	vfree(mempool);
}

/*
 * __vxge_hw_device_fifo_config_check - Check fifo configuration.
 * Check the fifo configuration
 */
enum vxge_hw_status
__vxge_hw_device_fifo_config_check(struct vxge_hw_fifo_config *fifo_config)
{
	if ((fifo_config->fifo_blocks < VXGE_HW_MIN_FIFO_BLOCKS) ||
	     (fifo_config->fifo_blocks > VXGE_HW_MAX_FIFO_BLOCKS))
		return VXGE_HW_BADCFG_FIFO_BLOCKS;

	return VXGE_HW_OK;
}

/*
 * __vxge_hw_device_vpath_config_check - Check vpath configuration.
 * Check the vpath configuration
 */
enum vxge_hw_status
__vxge_hw_device_vpath_config_check(struct vxge_hw_vp_config *vp_config)
{
	enum vxge_hw_status status;

	if ((vp_config->min_bandwidth < VXGE_HW_VPATH_BANDWIDTH_MIN) ||
		(vp_config->min_bandwidth >
					VXGE_HW_VPATH_BANDWIDTH_MAX))
		return VXGE_HW_BADCFG_VPATH_MIN_BANDWIDTH;

	status = __vxge_hw_device_fifo_config_check(&vp_config->fifo);
	if (status != VXGE_HW_OK)
		return status;

	if ((vp_config->mtu != VXGE_HW_VPATH_USE_FLASH_DEFAULT_INITIAL_MTU) &&
		((vp_config->mtu < VXGE_HW_VPATH_MIN_INITIAL_MTU) ||
		(vp_config->mtu > VXGE_HW_VPATH_MAX_INITIAL_MTU)))
		return VXGE_HW_BADCFG_VPATH_MTU;

	if ((vp_config->rpa_strip_vlan_tag !=
		VXGE_HW_VPATH_RPA_STRIP_VLAN_TAG_USE_FLASH_DEFAULT) &&
		(vp_config->rpa_strip_vlan_tag !=
		VXGE_HW_VPATH_RPA_STRIP_VLAN_TAG_ENABLE) &&
		(vp_config->rpa_strip_vlan_tag !=
		VXGE_HW_VPATH_RPA_STRIP_VLAN_TAG_DISABLE))
		return VXGE_HW_BADCFG_VPATH_RPA_STRIP_VLAN_TAG;

	return VXGE_HW_OK;
}

/*
 * __vxge_hw_device_config_check - Check device configuration.
 * Check the device configuration
 */
enum vxge_hw_status
__vxge_hw_device_config_check(struct vxge_hw_device_config *new_config)
{
	u32 i;
	enum vxge_hw_status status;

	if ((new_config->intr_mode != VXGE_HW_INTR_MODE_IRQLINE) &&
	   (new_config->intr_mode != VXGE_HW_INTR_MODE_MSIX) &&
	   (new_config->intr_mode != VXGE_HW_INTR_MODE_MSIX_ONE_SHOT) &&
	   (new_config->intr_mode != VXGE_HW_INTR_MODE_DEF))
		return VXGE_HW_BADCFG_INTR_MODE;

	if ((new_config->rts_mac_en != VXGE_HW_RTS_MAC_DISABLE) &&
	   (new_config->rts_mac_en != VXGE_HW_RTS_MAC_ENABLE))
		return VXGE_HW_BADCFG_RTS_MAC_EN;

	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {
		status = __vxge_hw_device_vpath_config_check(
				&new_config->vp_config[i]);
		if (status != VXGE_HW_OK)
			return status;
	}

	return VXGE_HW_OK;
}

/*
 * vxge_hw_device_config_default_get - Initialize device config with defaults.
 * Initialize Titan device config with default values.
 */
enum vxge_hw_status __devinit
vxge_hw_device_config_default_get(struct vxge_hw_device_config *device_config)
{
	u32 i;

	device_config->dma_blockpool_initial =
					VXGE_HW_INITIAL_DMA_BLOCK_POOL_SIZE;
	device_config->dma_blockpool_max = VXGE_HW_MAX_DMA_BLOCK_POOL_SIZE;
	device_config->intr_mode = VXGE_HW_INTR_MODE_DEF;
	device_config->rth_en = VXGE_HW_RTH_DEFAULT;
	device_config->rth_it_type = VXGE_HW_RTH_IT_TYPE_DEFAULT;
	device_config->device_poll_millis =  VXGE_HW_DEF_DEVICE_POLL_MILLIS;
	device_config->rts_mac_en =  VXGE_HW_RTS_MAC_DEFAULT;

	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {

		device_config->vp_config[i].vp_id = i;

		device_config->vp_config[i].min_bandwidth =
				VXGE_HW_VPATH_BANDWIDTH_DEFAULT;

		device_config->vp_config[i].ring.enable = VXGE_HW_RING_DEFAULT;

		device_config->vp_config[i].ring.ring_blocks =
				VXGE_HW_DEF_RING_BLOCKS;

		device_config->vp_config[i].ring.buffer_mode =
				VXGE_HW_RING_RXD_BUFFER_MODE_DEFAULT;

		device_config->vp_config[i].ring.scatter_mode =
				VXGE_HW_RING_SCATTER_MODE_USE_FLASH_DEFAULT;

		device_config->vp_config[i].ring.rxds_limit =
				VXGE_HW_DEF_RING_RXDS_LIMIT;

		device_config->vp_config[i].fifo.enable = VXGE_HW_FIFO_ENABLE;

		device_config->vp_config[i].fifo.fifo_blocks =
				VXGE_HW_MIN_FIFO_BLOCKS;

		device_config->vp_config[i].fifo.max_frags =
				VXGE_HW_MAX_FIFO_FRAGS;

		device_config->vp_config[i].fifo.memblock_size =
				VXGE_HW_DEF_FIFO_MEMBLOCK_SIZE;

		device_config->vp_config[i].fifo.alignment_size =
				VXGE_HW_DEF_FIFO_ALIGNMENT_SIZE;

		device_config->vp_config[i].fifo.intr =
				VXGE_HW_FIFO_QUEUE_INTR_DEFAULT;

		device_config->vp_config[i].fifo.no_snoop_bits =
				VXGE_HW_FIFO_NO_SNOOP_DEFAULT;
		device_config->vp_config[i].tti.intr_enable =
				VXGE_HW_TIM_INTR_DEFAULT;

		device_config->vp_config[i].tti.btimer_val =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.timer_ac_en =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.timer_ci_en =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.timer_ri_en =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.rtimer_val =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.util_sel =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.ltimer_val =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.urange_a =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.uec_a =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.urange_b =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.uec_b =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.urange_c =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.uec_c =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].tti.uec_d =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.intr_enable =
				VXGE_HW_TIM_INTR_DEFAULT;

		device_config->vp_config[i].rti.btimer_val =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.timer_ac_en =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.timer_ci_en =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.timer_ri_en =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.rtimer_val =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.util_sel =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.ltimer_val =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.urange_a =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.uec_a =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.urange_b =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.uec_b =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.urange_c =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.uec_c =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].rti.uec_d =
				VXGE_HW_USE_FLASH_DEFAULT;

		device_config->vp_config[i].mtu =
				VXGE_HW_VPATH_USE_FLASH_DEFAULT_INITIAL_MTU;

		device_config->vp_config[i].rpa_strip_vlan_tag =
			VXGE_HW_VPATH_RPA_STRIP_VLAN_TAG_USE_FLASH_DEFAULT;
	}

	return VXGE_HW_OK;
}

/*
 * _hw_legacy_swapper_set - Set the swapper bits for the legacy secion.
 * Set the swapper bits appropriately for the lagacy section.
 */
enum vxge_hw_status
__vxge_hw_legacy_swapper_set(struct vxge_hw_legacy_reg __iomem *legacy_reg)
{
	u64 val64;
	enum vxge_hw_status status = VXGE_HW_OK;

	val64 = readq(&legacy_reg->toc_swapper_fb);

	wmb();

	switch (val64) {

	case VXGE_HW_SWAPPER_INITIAL_VALUE:
		return status;

	case VXGE_HW_SWAPPER_BYTE_SWAPPED_BIT_FLIPPED:
		writeq(VXGE_HW_SWAPPER_READ_BYTE_SWAP_ENABLE,
			&legacy_reg->pifm_rd_swap_en);
		writeq(VXGE_HW_SWAPPER_READ_BIT_FLAP_ENABLE,
			&legacy_reg->pifm_rd_flip_en);
		writeq(VXGE_HW_SWAPPER_WRITE_BYTE_SWAP_ENABLE,
			&legacy_reg->pifm_wr_swap_en);
		writeq(VXGE_HW_SWAPPER_WRITE_BIT_FLAP_ENABLE,
			&legacy_reg->pifm_wr_flip_en);
		break;

	case VXGE_HW_SWAPPER_BYTE_SWAPPED:
		writeq(VXGE_HW_SWAPPER_READ_BYTE_SWAP_ENABLE,
			&legacy_reg->pifm_rd_swap_en);
		writeq(VXGE_HW_SWAPPER_WRITE_BYTE_SWAP_ENABLE,
			&legacy_reg->pifm_wr_swap_en);
		break;

	case VXGE_HW_SWAPPER_BIT_FLIPPED:
		writeq(VXGE_HW_SWAPPER_READ_BIT_FLAP_ENABLE,
			&legacy_reg->pifm_rd_flip_en);
		writeq(VXGE_HW_SWAPPER_WRITE_BIT_FLAP_ENABLE,
			&legacy_reg->pifm_wr_flip_en);
		break;
	}

	wmb();

	val64 = readq(&legacy_reg->toc_swapper_fb);

	if (val64 != VXGE_HW_SWAPPER_INITIAL_VALUE)
		status = VXGE_HW_ERR_SWAPPER_CTRL;

	return status;
}

/*
 * __vxge_hw_vpath_swapper_set - Set the swapper bits for the vpath.
 * Set the swapper bits appropriately for the vpath.
 */
enum vxge_hw_status
__vxge_hw_vpath_swapper_set(struct vxge_hw_vpath_reg __iomem *vpath_reg)
{
#ifndef __BIG_ENDIAN
	u64 val64;

	val64 = readq(&vpath_reg->vpath_general_cfg1);
	wmb();
	val64 |= VXGE_HW_VPATH_GENERAL_CFG1_CTL_BYTE_SWAPEN;
	writeq(val64, &vpath_reg->vpath_general_cfg1);
	wmb();
#endif
	return VXGE_HW_OK;
}

/*
 * __vxge_hw_kdfc_swapper_set - Set the swapper bits for the kdfc.
 * Set the swapper bits appropriately for the vpath.
 */
enum vxge_hw_status
__vxge_hw_kdfc_swapper_set(
	struct vxge_hw_legacy_reg __iomem *legacy_reg,
	struct vxge_hw_vpath_reg __iomem *vpath_reg)
{
	u64 val64;

	val64 = readq(&legacy_reg->pifm_wr_swap_en);

	if (val64 == VXGE_HW_SWAPPER_WRITE_BYTE_SWAP_ENABLE) {
		val64 = readq(&vpath_reg->kdfcctl_cfg0);
		wmb();

		val64 |= VXGE_HW_KDFCCTL_CFG0_BYTE_SWAPEN_FIFO0	|
			VXGE_HW_KDFCCTL_CFG0_BYTE_SWAPEN_FIFO1	|
			VXGE_HW_KDFCCTL_CFG0_BYTE_SWAPEN_FIFO2;

		writeq(val64, &vpath_reg->kdfcctl_cfg0);
		wmb();
	}

	return VXGE_HW_OK;
}

/*
 * vxge_hw_mgmt_device_config - Retrieve device configuration.
 * Get device configuration. Permits to retrieve at run-time configuration
 * values that were used to initialize and configure the device.
 */
enum vxge_hw_status
vxge_hw_mgmt_device_config(struct __vxge_hw_device *hldev,
			   struct vxge_hw_device_config *dev_config, int size)
{

	if ((hldev == NULL) || (hldev->magic != VXGE_HW_DEVICE_MAGIC))
		return VXGE_HW_ERR_INVALID_DEVICE;

	if (size != sizeof(struct vxge_hw_device_config))
		return VXGE_HW_ERR_VERSION_CONFLICT;

	memcpy(dev_config, &hldev->config,
		sizeof(struct vxge_hw_device_config));

	return VXGE_HW_OK;
}

/*
 * vxge_hw_mgmt_reg_read - Read Titan register.
 */
enum vxge_hw_status
vxge_hw_mgmt_reg_read(struct __vxge_hw_device *hldev,
		      enum vxge_hw_mgmt_reg_type type,
		      u32 index, u32 offset, u64 *value)
{
	enum vxge_hw_status status = VXGE_HW_OK;

	if ((hldev == NULL) || (hldev->magic != VXGE_HW_DEVICE_MAGIC)) {
		status = VXGE_HW_ERR_INVALID_DEVICE;
		goto exit;
	}

	switch (type) {
	case vxge_hw_mgmt_reg_type_legacy:
		if (offset > sizeof(struct vxge_hw_legacy_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		*value = readq((void __iomem *)hldev->legacy_reg + offset);
		break;
	case vxge_hw_mgmt_reg_type_toc:
		if (offset > sizeof(struct vxge_hw_toc_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		*value = readq((void __iomem *)hldev->toc_reg + offset);
		break;
	case vxge_hw_mgmt_reg_type_common:
		if (offset > sizeof(struct vxge_hw_common_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		*value = readq((void __iomem *)hldev->common_reg + offset);
		break;
	case vxge_hw_mgmt_reg_type_mrpcim:
		if (!(hldev->access_rights &
			VXGE_HW_DEVICE_ACCESS_RIGHT_MRPCIM)) {
			status = VXGE_HW_ERR_PRIVILAGED_OPEARATION;
			break;
		}
		if (offset > sizeof(struct vxge_hw_mrpcim_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		*value = readq((void __iomem *)hldev->mrpcim_reg + offset);
		break;
	case vxge_hw_mgmt_reg_type_srpcim:
		if (!(hldev->access_rights &
			VXGE_HW_DEVICE_ACCESS_RIGHT_SRPCIM)) {
			status = VXGE_HW_ERR_PRIVILAGED_OPEARATION;
			break;
		}
		if (index > VXGE_HW_TITAN_SRPCIM_REG_SPACES - 1) {
			status = VXGE_HW_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(struct vxge_hw_srpcim_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		*value = readq((void __iomem *)hldev->srpcim_reg[index] +
				offset);
		break;
	case vxge_hw_mgmt_reg_type_vpmgmt:
		if ((index > VXGE_HW_TITAN_VPMGMT_REG_SPACES - 1) ||
			(!(hldev->vpath_assignments & vxge_mBIT(index)))) {
			status = VXGE_HW_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(struct vxge_hw_vpmgmt_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		*value = readq((void __iomem *)hldev->vpmgmt_reg[index] +
				offset);
		break;
	case vxge_hw_mgmt_reg_type_vpath:
		if ((index > VXGE_HW_TITAN_VPATH_REG_SPACES - 1) ||
			(!(hldev->vpath_assignments & vxge_mBIT(index)))) {
			status = VXGE_HW_ERR_INVALID_INDEX;
			break;
		}
		if (index > VXGE_HW_TITAN_VPATH_REG_SPACES - 1) {
			status = VXGE_HW_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(struct vxge_hw_vpath_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		*value = readq((void __iomem *)hldev->vpath_reg[index] +
				offset);
		break;
	default:
		status = VXGE_HW_ERR_INVALID_TYPE;
		break;
	}

exit:
	return status;
}

/*
 * vxge_hw_mgmt_reg_Write - Write Titan register.
 */
enum vxge_hw_status
vxge_hw_mgmt_reg_write(struct __vxge_hw_device *hldev,
		      enum vxge_hw_mgmt_reg_type type,
		      u32 index, u32 offset, u64 value)
{
	enum vxge_hw_status status = VXGE_HW_OK;

	if ((hldev == NULL) || (hldev->magic != VXGE_HW_DEVICE_MAGIC)) {
		status = VXGE_HW_ERR_INVALID_DEVICE;
		goto exit;
	}

	switch (type) {
	case vxge_hw_mgmt_reg_type_legacy:
		if (offset > sizeof(struct vxge_hw_legacy_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		writeq(value, (void __iomem *)hldev->legacy_reg + offset);
		break;
	case vxge_hw_mgmt_reg_type_toc:
		if (offset > sizeof(struct vxge_hw_toc_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		writeq(value, (void __iomem *)hldev->toc_reg + offset);
		break;
	case vxge_hw_mgmt_reg_type_common:
		if (offset > sizeof(struct vxge_hw_common_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		writeq(value, (void __iomem *)hldev->common_reg + offset);
		break;
	case vxge_hw_mgmt_reg_type_mrpcim:
		if (!(hldev->access_rights &
			VXGE_HW_DEVICE_ACCESS_RIGHT_MRPCIM)) {
			status = VXGE_HW_ERR_PRIVILAGED_OPEARATION;
			break;
		}
		if (offset > sizeof(struct vxge_hw_mrpcim_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		writeq(value, (void __iomem *)hldev->mrpcim_reg + offset);
		break;
	case vxge_hw_mgmt_reg_type_srpcim:
		if (!(hldev->access_rights &
			VXGE_HW_DEVICE_ACCESS_RIGHT_SRPCIM)) {
			status = VXGE_HW_ERR_PRIVILAGED_OPEARATION;
			break;
		}
		if (index > VXGE_HW_TITAN_SRPCIM_REG_SPACES - 1) {
			status = VXGE_HW_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(struct vxge_hw_srpcim_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		writeq(value, (void __iomem *)hldev->srpcim_reg[index] +
			offset);

		break;
	case vxge_hw_mgmt_reg_type_vpmgmt:
		if ((index > VXGE_HW_TITAN_VPMGMT_REG_SPACES - 1) ||
			(!(hldev->vpath_assignments & vxge_mBIT(index)))) {
			status = VXGE_HW_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(struct vxge_hw_vpmgmt_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		writeq(value, (void __iomem *)hldev->vpmgmt_reg[index] +
			offset);
		break;
	case vxge_hw_mgmt_reg_type_vpath:
		if ((index > VXGE_HW_TITAN_VPATH_REG_SPACES-1) ||
			(!(hldev->vpath_assignments & vxge_mBIT(index)))) {
			status = VXGE_HW_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(struct vxge_hw_vpath_reg) - 8) {
			status = VXGE_HW_ERR_INVALID_OFFSET;
			break;
		}
		writeq(value, (void __iomem *)hldev->vpath_reg[index] +
			offset);
		break;
	default:
		status = VXGE_HW_ERR_INVALID_TYPE;
		break;
	}
exit:
	return status;
}

/*
 * __vxge_hw_fifo_mempool_item_alloc - Allocate List blocks for TxD
 * list callback
 * This function is callback passed to __vxge_hw_mempool_create to create memory
 * pool for TxD list
 */
static void
__vxge_hw_fifo_mempool_item_alloc(
	struct vxge_hw_mempool *mempoolh,
	u32 memblock_index, struct vxge_hw_mempool_dma *dma_object,
	u32 index, u32 is_last)
{
	u32 memblock_item_idx;
	struct __vxge_hw_fifo_txdl_priv *txdl_priv;
	struct vxge_hw_fifo_txd *txdp =
		(struct vxge_hw_fifo_txd *)mempoolh->items_arr[index];
	struct __vxge_hw_fifo *fifo =
			(struct __vxge_hw_fifo *)mempoolh->userdata;
	void *memblock = mempoolh->memblocks_arr[memblock_index];

	vxge_assert(txdp);

	txdp->host_control = (u64) (size_t)
	__vxge_hw_mempool_item_priv(mempoolh, memblock_index, txdp,
					&memblock_item_idx);

	txdl_priv = __vxge_hw_fifo_txdl_priv(fifo, txdp);

	vxge_assert(txdl_priv);

	fifo->channel.reserve_arr[fifo->channel.reserve_ptr - 1 - index] = txdp;

	/* pre-format HW's TxDL's private */
	txdl_priv->dma_offset = (char *)txdp - (char *)memblock;
	txdl_priv->dma_addr = dma_object->addr + txdl_priv->dma_offset;
	txdl_priv->dma_handle = dma_object->handle;
	txdl_priv->memblock   = memblock;
	txdl_priv->first_txdp = txdp;
	txdl_priv->next_txdl_priv = NULL;
	txdl_priv->alloc_frags = 0;

	return;
}

/*
 * __vxge_hw_fifo_create - Create a FIFO
 * This function creates FIFO and initializes it.
 */
enum vxge_hw_status
__vxge_hw_fifo_create(struct __vxge_hw_vpath_handle *vp,
		      struct vxge_hw_fifo_attr *attr)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	struct __vxge_hw_fifo *fifo;
	struct vxge_hw_fifo_config *config;
	u32 txdl_size, txdl_per_memblock;
	struct vxge_hw_mempool_cbs fifo_mp_callback;
	struct __vxge_hw_virtualpath *vpath;

	if ((vp == NULL) || (attr == NULL)) {
		status = VXGE_HW_ERR_INVALID_HANDLE;
		goto exit;
	}
	vpath = vp->vpath;
	config = &vpath->hldev->config.vp_config[vpath->vp_id].fifo;

	txdl_size = config->max_frags * sizeof(struct vxge_hw_fifo_txd);

	txdl_per_memblock = config->memblock_size / txdl_size;

	fifo = (struct __vxge_hw_fifo *)__vxge_hw_channel_allocate(vp,
					VXGE_HW_CHANNEL_TYPE_FIFO,
					config->fifo_blocks * txdl_per_memblock,
					attr->per_txdl_space, attr->userdata);

	if (fifo == NULL) {
		status = VXGE_HW_ERR_OUT_OF_MEMORY;
		goto exit;
	}

	vpath->fifoh = fifo;
	fifo->nofl_db = vpath->nofl_db;

	fifo->vp_id = vpath->vp_id;
	fifo->vp_reg = vpath->vp_reg;
	fifo->stats = &vpath->sw_stats->fifo_stats;

	fifo->config = config;

	/* apply "interrupts per txdl" attribute */
	fifo->interrupt_type = VXGE_HW_FIFO_TXD_INT_TYPE_UTILZ;

	if (fifo->config->intr)
		fifo->interrupt_type = VXGE_HW_FIFO_TXD_INT_TYPE_PER_LIST;

	fifo->no_snoop_bits = config->no_snoop_bits;

	/*
	 * FIFO memory management strategy:
	 *
	 * TxDL split into three independent parts:
	 *	- set of TxD's
	 *	- TxD HW private part
	 *	- driver private part
	 *
	 * Adaptative memory allocation used. i.e. Memory allocated on
	 * demand with the size which will fit into one memory block.
	 * One memory block may contain more than one TxDL.
	 *
	 * During "reserve" operations more memory can be allocated on demand
	 * for example due to FIFO full condition.
	 *
	 * Pool of memory memblocks never shrinks except in __vxge_hw_fifo_close
	 * routine which will essentially stop the channel and free resources.
	 */

	/* TxDL common private size == TxDL private  +  driver private */
	fifo->priv_size =
		sizeof(struct __vxge_hw_fifo_txdl_priv) + attr->per_txdl_space;
	fifo->priv_size = ((fifo->priv_size  +  VXGE_CACHE_LINE_SIZE - 1) /
			VXGE_CACHE_LINE_SIZE) * VXGE_CACHE_LINE_SIZE;

	fifo->per_txdl_space = attr->per_txdl_space;

	/* recompute txdl size to be cacheline aligned */
	fifo->txdl_size = txdl_size;
	fifo->txdl_per_memblock = txdl_per_memblock;

	fifo->txdl_term = attr->txdl_term;
	fifo->callback = attr->callback;

	if (fifo->txdl_per_memblock == 0) {
		__vxge_hw_fifo_delete(vp);
		status = VXGE_HW_ERR_INVALID_BLOCK_SIZE;
		goto exit;
	}

	fifo_mp_callback.item_func_alloc = __vxge_hw_fifo_mempool_item_alloc;

	fifo->mempool =
		__vxge_hw_mempool_create(vpath->hldev,
			fifo->config->memblock_size,
			fifo->txdl_size,
			fifo->priv_size,
			(fifo->config->fifo_blocks * fifo->txdl_per_memblock),
			(fifo->config->fifo_blocks * fifo->txdl_per_memblock),
			&fifo_mp_callback,
			fifo);

	if (fifo->mempool == NULL) {
		__vxge_hw_fifo_delete(vp);
		status = VXGE_HW_ERR_OUT_OF_MEMORY;
		goto exit;
	}

	status = __vxge_hw_channel_initialize(&fifo->channel);
	if (status != VXGE_HW_OK) {
		__vxge_hw_fifo_delete(vp);
		goto exit;
	}

	vxge_assert(fifo->channel.reserve_ptr);
exit:
	return status;
}

/*
 * __vxge_hw_fifo_abort - Returns the TxD
 * This function terminates the TxDs of fifo
 */
enum vxge_hw_status __vxge_hw_fifo_abort(struct __vxge_hw_fifo *fifo)
{
	void *txdlh;

	for (;;) {
		vxge_hw_channel_dtr_try_complete(&fifo->channel, &txdlh);

		if (txdlh == NULL)
			break;

		vxge_hw_channel_dtr_complete(&fifo->channel);

		if (fifo->txdl_term) {
			fifo->txdl_term(txdlh,
			VXGE_HW_TXDL_STATE_POSTED,
			fifo->channel.userdata);
		}

		vxge_hw_channel_dtr_free(&fifo->channel, txdlh);
	}

	return VXGE_HW_OK;
}

/*
 * __vxge_hw_fifo_reset - Resets the fifo
 * This function resets the fifo during vpath reset operation
 */
enum vxge_hw_status __vxge_hw_fifo_reset(struct __vxge_hw_fifo *fifo)
{
	enum vxge_hw_status status = VXGE_HW_OK;

	__vxge_hw_fifo_abort(fifo);
	status = __vxge_hw_channel_reset(&fifo->channel);

	return status;
}

/*
 * __vxge_hw_fifo_delete - Removes the FIFO
 * This function freeup the memory pool and removes the FIFO
 */
enum vxge_hw_status __vxge_hw_fifo_delete(struct __vxge_hw_vpath_handle *vp)
{
	struct __vxge_hw_fifo *fifo = vp->vpath->fifoh;

	__vxge_hw_fifo_abort(fifo);

	if (fifo->mempool)
		__vxge_hw_mempool_destroy(fifo->mempool);

	vp->vpath->fifoh = NULL;

	__vxge_hw_channel_free(&fifo->channel);

	return VXGE_HW_OK;
}

/*
 * __vxge_hw_vpath_pci_read - Read the content of given address
 *                          in pci config space.
 * Read from the vpath pci config space.
 */
enum vxge_hw_status
__vxge_hw_vpath_pci_read(struct __vxge_hw_virtualpath *vpath,
			 u32 phy_func_0, u32 offset, u32 *val)
{
	u64 val64;
	enum vxge_hw_status status = VXGE_HW_OK;
	struct vxge_hw_vpath_reg __iomem *vp_reg = vpath->vp_reg;

	val64 =	VXGE_HW_PCI_CONFIG_ACCESS_CFG1_ADDRESS(offset);

	if (phy_func_0)
		val64 |= VXGE_HW_PCI_CONFIG_ACCESS_CFG1_SEL_FUNC0;

	writeq(val64, &vp_reg->pci_config_access_cfg1);
	wmb();
	writeq(VXGE_HW_PCI_CONFIG_ACCESS_CFG2_REQ,
			&vp_reg->pci_config_access_cfg2);
	wmb();

	status = __vxge_hw_device_register_poll(
			&vp_reg->pci_config_access_cfg2,
			VXGE_HW_INTR_MASK_ALL, VXGE_HW_DEF_DEVICE_POLL_MILLIS);

	if (status != VXGE_HW_OK)
		goto exit;

	val64 = readq(&vp_reg->pci_config_access_status);

	if (val64 & VXGE_HW_PCI_CONFIG_ACCESS_STATUS_ACCESS_ERR) {
		status = VXGE_HW_FAIL;
		*val = 0;
	} else
		*val = (u32)vxge_bVALn(val64, 32, 32);
exit:
	return status;
}

/*
 * __vxge_hw_vpath_func_id_get - Get the function id of the vpath.
 * Returns the function number of the vpath.
 */
u32
__vxge_hw_vpath_func_id_get(u32 vp_id,
	struct vxge_hw_vpmgmt_reg __iomem *vpmgmt_reg)
{
	u64 val64;

	val64 = readq(&vpmgmt_reg->vpath_to_func_map_cfg1);

	return
	 (u32)VXGE_HW_VPATH_TO_FUNC_MAP_CFG1_GET_VPATH_TO_FUNC_MAP_CFG1(val64);
}

/*
 * __vxge_hw_read_rts_ds - Program RTS steering critieria
 */
static inline void
__vxge_hw_read_rts_ds(struct vxge_hw_vpath_reg __iomem *vpath_reg,
		      u64 dta_struct_sel)
{
	writeq(0, &vpath_reg->rts_access_steer_ctrl);
	wmb();
	writeq(dta_struct_sel, &vpath_reg->rts_access_steer_data0);
	writeq(0, &vpath_reg->rts_access_steer_data1);
	wmb();
	return;
}


/*
 * __vxge_hw_vpath_card_info_get - Get the serial numbers,
 * part number and product description.
 */
enum vxge_hw_status
__vxge_hw_vpath_card_info_get(
	u32 vp_id,
	struct vxge_hw_vpath_reg __iomem *vpath_reg,
	struct vxge_hw_device_hw_info *hw_info)
{
	u32 i, j;
	u64 val64;
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	enum vxge_hw_status status = VXGE_HW_OK;
	u8 *serial_number = hw_info->serial_number;
	u8 *part_number = hw_info->part_number;
	u8 *product_desc = hw_info->product_desc;

	__vxge_hw_read_rts_ds(vpath_reg,
		VXGE_HW_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_SERIAL_NUMBER);

	val64 = VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION(
			VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_READ_MEMO_ENTRY) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
			VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_OFFSET(0);

	status = __vxge_hw_pio_mem_write64(val64,
				&vpath_reg->rts_access_steer_ctrl,
				VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE,
				VXGE_HW_DEF_DEVICE_POLL_MILLIS);

	if (status != VXGE_HW_OK)
		return status;

	val64 = readq(&vpath_reg->rts_access_steer_ctrl);

	if (val64 & VXGE_HW_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS) {
		data1 = readq(&vpath_reg->rts_access_steer_data0);
		((u64 *)serial_number)[0] = be64_to_cpu(data1);

		data2 = readq(&vpath_reg->rts_access_steer_data1);
		((u64 *)serial_number)[1] = be64_to_cpu(data2);
		status = VXGE_HW_OK;
	} else
		*serial_number = 0;

	__vxge_hw_read_rts_ds(vpath_reg,
			VXGE_HW_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PART_NUMBER);

	val64 = VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION(
			VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_READ_MEMO_ENTRY) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
			VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_OFFSET(0);

	status = __vxge_hw_pio_mem_write64(val64,
				&vpath_reg->rts_access_steer_ctrl,
				VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE,
				VXGE_HW_DEF_DEVICE_POLL_MILLIS);

	if (status != VXGE_HW_OK)
		return status;

	val64 = readq(&vpath_reg->rts_access_steer_ctrl);

	if (val64 & VXGE_HW_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS) {

		data1 = readq(&vpath_reg->rts_access_steer_data0);
		((u64 *)part_number)[0] = be64_to_cpu(data1);

		data2 = readq(&vpath_reg->rts_access_steer_data1);
		((u64 *)part_number)[1] = be64_to_cpu(data2);

		status = VXGE_HW_OK;

	} else
		*part_number = 0;

	j = 0;

	for (i = VXGE_HW_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_DESC_0;
	     i <= VXGE_HW_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_DESC_3; i++) {

		__vxge_hw_read_rts_ds(vpath_reg, i);

		val64 = VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION(
			VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_READ_MEMO_ENTRY) |
			VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
			VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
			VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE |
			VXGE_HW_RTS_ACCESS_STEER_CTRL_OFFSET(0);

		status = __vxge_hw_pio_mem_write64(val64,
				&vpath_reg->rts_access_steer_ctrl,
				VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE,
				VXGE_HW_DEF_DEVICE_POLL_MILLIS);

		if (status != VXGE_HW_OK)
			return status;

		val64 = readq(&vpath_reg->rts_access_steer_ctrl);

		if (val64 & VXGE_HW_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS) {

			data1 = readq(&vpath_reg->rts_access_steer_data0);
			((u64 *)product_desc)[j++] = be64_to_cpu(data1);

			data2 = readq(&vpath_reg->rts_access_steer_data1);
			((u64 *)product_desc)[j++] = be64_to_cpu(data2);

			status = VXGE_HW_OK;
		} else
			*product_desc = 0;
	}

	return status;
}

/*
 * __vxge_hw_vpath_fw_ver_get - Get the fw version
 * Returns FW Version
 */
enum vxge_hw_status
__vxge_hw_vpath_fw_ver_get(
	u32 vp_id,
	struct vxge_hw_vpath_reg __iomem *vpath_reg,
	struct vxge_hw_device_hw_info *hw_info)
{
	u64 val64;
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	struct vxge_hw_device_version *fw_version = &hw_info->fw_version;
	struct vxge_hw_device_date *fw_date = &hw_info->fw_date;
	struct vxge_hw_device_version *flash_version = &hw_info->flash_version;
	struct vxge_hw_device_date *flash_date = &hw_info->flash_date;
	enum vxge_hw_status status = VXGE_HW_OK;

	val64 = VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION(
		VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_READ_ENTRY) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_OFFSET(0);

	status = __vxge_hw_pio_mem_write64(val64,
				&vpath_reg->rts_access_steer_ctrl,
				VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE,
				VXGE_HW_DEF_DEVICE_POLL_MILLIS);

	if (status != VXGE_HW_OK)
		goto exit;

	val64 = readq(&vpath_reg->rts_access_steer_ctrl);

	if (val64 & VXGE_HW_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS) {

		data1 = readq(&vpath_reg->rts_access_steer_data0);
		data2 = readq(&vpath_reg->rts_access_steer_data1);

		fw_date->day =
			(u32)VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_FW_VER_DAY(
						data1);
		fw_date->month =
			(u32)VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_FW_VER_MONTH(
						data1);
		fw_date->year =
			(u32)VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_FW_VER_YEAR(
						data1);

		snprintf(fw_date->date, VXGE_HW_FW_STRLEN, "%2.2d/%2.2d/%4.4d",
			fw_date->month, fw_date->day, fw_date->year);

		fw_version->major =
		    (u32)VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_FW_VER_MAJOR(data1);
		fw_version->minor =
		    (u32)VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_FW_VER_MINOR(data1);
		fw_version->build =
		    (u32)VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_FW_VER_BUILD(data1);

		snprintf(fw_version->version, VXGE_HW_FW_STRLEN, "%d.%d.%d",
		    fw_version->major, fw_version->minor, fw_version->build);

		flash_date->day =
		  (u32)VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_FLASH_VER_DAY(data2);
		flash_date->month =
		 (u32)VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_FLASH_VER_MONTH(data2);
		flash_date->year =
		 (u32)VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_FLASH_VER_YEAR(data2);

		snprintf(flash_date->date, VXGE_HW_FW_STRLEN,
			"%2.2d/%2.2d/%4.4d",
			flash_date->month, flash_date->day, flash_date->year);

		flash_version->major =
		 (u32)VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_FLASH_VER_MAJOR(data2);
		flash_version->minor =
		 (u32)VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_FLASH_VER_MINOR(data2);
		flash_version->build =
		 (u32)VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_FLASH_VER_BUILD(data2);

		snprintf(flash_version->version, VXGE_HW_FW_STRLEN, "%d.%d.%d",
			flash_version->major, flash_version->minor,
			flash_version->build);

		status = VXGE_HW_OK;

	} else
		status = VXGE_HW_FAIL;
exit:
	return status;
}

/*
 * __vxge_hw_vpath_pci_func_mode_get - Get the pci mode
 * Returns pci function mode
 */
u64
__vxge_hw_vpath_pci_func_mode_get(
	u32  vp_id,
	struct vxge_hw_vpath_reg __iomem *vpath_reg)
{
	u64 val64;
	u64 data1 = 0ULL;
	enum vxge_hw_status status = VXGE_HW_OK;

	__vxge_hw_read_rts_ds(vpath_reg,
		VXGE_HW_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PCI_MODE);

	val64 = VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION(
			VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_READ_MEMO_ENTRY) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
			VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_OFFSET(0);

	status = __vxge_hw_pio_mem_write64(val64,
				&vpath_reg->rts_access_steer_ctrl,
				VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE,
				VXGE_HW_DEF_DEVICE_POLL_MILLIS);

	if (status != VXGE_HW_OK)
		goto exit;

	val64 = readq(&vpath_reg->rts_access_steer_ctrl);

	if (val64 & VXGE_HW_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS) {
		data1 = readq(&vpath_reg->rts_access_steer_data0);
		status = VXGE_HW_OK;
	} else {
		data1 = 0;
		status = VXGE_HW_FAIL;
	}
exit:
	return data1;
}

/**
 * vxge_hw_device_flick_link_led - Flick (blink) link LED.
 * @hldev: HW device.
 * @on_off: TRUE if flickering to be on, FALSE to be off
 *
 * Flicker the link LED.
 */
enum vxge_hw_status
vxge_hw_device_flick_link_led(struct __vxge_hw_device *hldev,
			       u64 on_off)
{
	u64 val64;
	enum vxge_hw_status status = VXGE_HW_OK;
	struct vxge_hw_vpath_reg __iomem *vp_reg;

	if (hldev == NULL) {
		status = VXGE_HW_ERR_INVALID_DEVICE;
		goto exit;
	}

	vp_reg = hldev->vpath_reg[hldev->first_vp_id];

	writeq(0, &vp_reg->rts_access_steer_ctrl);
	wmb();
	writeq(on_off, &vp_reg->rts_access_steer_data0);
	writeq(0, &vp_reg->rts_access_steer_data1);
	wmb();

	val64 = VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION(
			VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_LED_CONTROL) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
			VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_OFFSET(0);

	status = __vxge_hw_pio_mem_write64(val64,
				&vp_reg->rts_access_steer_ctrl,
				VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE,
				VXGE_HW_DEF_DEVICE_POLL_MILLIS);
exit:
	return status;
}

/*
 * __vxge_hw_vpath_rts_table_get - Get the entries from RTS access tables
 */
enum vxge_hw_status
__vxge_hw_vpath_rts_table_get(
	struct __vxge_hw_vpath_handle *vp,
	u32 action, u32 rts_table, u32 offset, u64 *data1, u64 *data2)
{
	u64 val64;
	struct __vxge_hw_virtualpath *vpath;
	struct vxge_hw_vpath_reg __iomem *vp_reg;

	enum vxge_hw_status status = VXGE_HW_OK;

	if (vp == NULL) {
		status = VXGE_HW_ERR_INVALID_HANDLE;
		goto exit;
	}

	vpath = vp->vpath;
	vp_reg = vpath->vp_reg;

	val64 = VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION(action) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(rts_table) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_OFFSET(offset);

	if ((rts_table ==
		VXGE_HW_RTS_ACS_STEER_CTRL_DATA_STRUCT_SEL_RTH_SOLO_IT) ||
	    (rts_table ==
		VXGE_HW_RTS_ACS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT) ||
	    (rts_table ==
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MASK) ||
	    (rts_table ==
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_KEY)) {
		val64 = val64 |	VXGE_HW_RTS_ACCESS_STEER_CTRL_TABLE_SEL;
	}

	status = __vxge_hw_pio_mem_write64(val64,
				&vp_reg->rts_access_steer_ctrl,
				VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE,
				vpath->hldev->config.device_poll_millis);

	if (status != VXGE_HW_OK)
		goto exit;

	val64 = readq(&vp_reg->rts_access_steer_ctrl);

	if (val64 & VXGE_HW_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS) {

		*data1 = readq(&vp_reg->rts_access_steer_data0);

		if ((rts_table ==
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_DA) ||
		(rts_table ==
		VXGE_HW_RTS_ACS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT)) {
			*data2 = readq(&vp_reg->rts_access_steer_data1);
		}
		status = VXGE_HW_OK;
	} else
		status = VXGE_HW_FAIL;
exit:
	return status;
}

/*
 * __vxge_hw_vpath_rts_table_set - Set the entries of RTS access tables
 */
enum vxge_hw_status
__vxge_hw_vpath_rts_table_set(
	struct __vxge_hw_vpath_handle *vp, u32 action, u32 rts_table,
	u32 offset, u64 data1, u64 data2)
{
	u64 val64;
	struct __vxge_hw_virtualpath *vpath;
	enum vxge_hw_status status = VXGE_HW_OK;
	struct vxge_hw_vpath_reg __iomem *vp_reg;

	if (vp == NULL) {
		status = VXGE_HW_ERR_INVALID_HANDLE;
		goto exit;
	}

	vpath = vp->vpath;
	vp_reg = vpath->vp_reg;

	writeq(data1, &vp_reg->rts_access_steer_data0);
	wmb();

	if ((rts_table == VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_DA) ||
	    (rts_table ==
		VXGE_HW_RTS_ACS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT)) {
		writeq(data2, &vp_reg->rts_access_steer_data1);
		wmb();
	}

	val64 = VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION(action) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(rts_table) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_OFFSET(offset);

	status = __vxge_hw_pio_mem_write64(val64,
				&vp_reg->rts_access_steer_ctrl,
				VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE,
				vpath->hldev->config.device_poll_millis);

	if (status != VXGE_HW_OK)
		goto exit;

	val64 = readq(&vp_reg->rts_access_steer_ctrl);

	if (val64 & VXGE_HW_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS)
		status = VXGE_HW_OK;
	else
		status = VXGE_HW_FAIL;
exit:
	return status;
}

/*
 * __vxge_hw_vpath_addr_get - Get the hw address entry for this vpath
 *               from MAC address table.
 */
enum vxge_hw_status
__vxge_hw_vpath_addr_get(
	u32 vp_id, struct vxge_hw_vpath_reg __iomem *vpath_reg,
	u8 (macaddr)[ETH_ALEN], u8 (macaddr_mask)[ETH_ALEN])
{
	u32 i;
	u64 val64;
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	enum vxge_hw_status status = VXGE_HW_OK;

	val64 = VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION(
		VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_LIST_FIRST_ENTRY) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_DA) |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE |
		VXGE_HW_RTS_ACCESS_STEER_CTRL_OFFSET(0);

	status = __vxge_hw_pio_mem_write64(val64,
				&vpath_reg->rts_access_steer_ctrl,
				VXGE_HW_RTS_ACCESS_STEER_CTRL_STROBE,
				VXGE_HW_DEF_DEVICE_POLL_MILLIS);

	if (status != VXGE_HW_OK)
		goto exit;

	val64 = readq(&vpath_reg->rts_access_steer_ctrl);

	if (val64 & VXGE_HW_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS) {

		data1 = readq(&vpath_reg->rts_access_steer_data0);
		data2 = readq(&vpath_reg->rts_access_steer_data1);

		data1 = VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_DA_MAC_ADDR(data1);
		data2 = VXGE_HW_RTS_ACCESS_STEER_DATA1_GET_DA_MAC_ADDR_MASK(
							data2);

		for (i = ETH_ALEN; i > 0; i--) {
			macaddr[i-1] = (u8)(data1 & 0xFF);
			data1 >>= 8;

			macaddr_mask[i-1] = (u8)(data2 & 0xFF);
			data2 >>= 8;
		}
		status = VXGE_HW_OK;
	} else
		status = VXGE_HW_FAIL;
exit:
	return status;
}

/*
 * vxge_hw_vpath_rts_rth_set - Set/configure RTS hashing.
 */
enum vxge_hw_status vxge_hw_vpath_rts_rth_set(
			struct __vxge_hw_vpath_handle *vp,
			enum vxge_hw_rth_algoritms algorithm,
			struct vxge_hw_rth_hash_types *hash_type,
			u16 bucket_size)
{
	u64 data0, data1;
	enum vxge_hw_status status = VXGE_HW_OK;

	if (vp == NULL) {
		status = VXGE_HW_ERR_INVALID_HANDLE;
		goto exit;
	}

	status = __vxge_hw_vpath_rts_table_get(vp,
		     VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_READ_ENTRY,
		     VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_GEN_CFG,
			0, &data0, &data1);

	data0 &= ~(VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_BUCKET_SIZE(0xf) |
			VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_ALG_SEL(0x3));

	data0 |= VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_EN |
	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_BUCKET_SIZE(bucket_size) |
	VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_ALG_SEL(algorithm);

	if (hash_type->hash_type_tcpipv4_en)
		data0 |= VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_TCP_IPV4_EN;

	if (hash_type->hash_type_ipv4_en)
		data0 |= VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_IPV4_EN;

	if (hash_type->hash_type_tcpipv6_en)
		data0 |= VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_TCP_IPV6_EN;

	if (hash_type->hash_type_ipv6_en)
		data0 |= VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_IPV6_EN;

	if (hash_type->hash_type_tcpipv6ex_en)
		data0 |=
		VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_TCP_IPV6_EX_EN;

	if (hash_type->hash_type_ipv6ex_en)
		data0 |= VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_IPV6_EX_EN;

	if (VXGE_HW_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_ACTIVE_TABLE(data0))
		data0 &= ~VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_ACTIVE_TABLE;
	else
		data0 |= VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_GEN_ACTIVE_TABLE;

	status = __vxge_hw_vpath_rts_table_set(vp,
		VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_WRITE_ENTRY,
		VXGE_HW_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_GEN_CFG,
		0, data0, 0);
exit:
	return status;
}

static void
vxge_hw_rts_rth_data0_data1_get(u32 j, u64 *data0, u64 *data1,
				u16 flag, u8 *itable)
{
	switch (flag) {
	case 1:
		*data0 = VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_ITEM0_BUCKET_NUM(j)|
			VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_ITEM0_ENTRY_EN |
			VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_ITEM0_BUCKET_DATA(
			itable[j]);
	case 2:
		*data0 |=
			VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_ITEM1_BUCKET_NUM(j)|
			VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_ITEM1_ENTRY_EN |
			VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_ITEM1_BUCKET_DATA(
			itable[j]);
	case 3:
		*data1 = VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM0_BUCKET_NUM(j)|
			VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM0_ENTRY_EN |
			VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM0_BUCKET_DATA(
			itable[j]);
	case 4:
		*data1 |=
			VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM1_BUCKET_NUM(j)|
			VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM1_ENTRY_EN |
			VXGE_HW_RTS_ACCESS_STEER_DATA1_RTH_ITEM1_BUCKET_DATA(
			itable[j]);
	default:
		return;
	}
}
/*
 * vxge_hw_vpath_rts_rth_itable_set - Set/configure indirection table (IT).
 */
enum vxge_hw_status vxge_hw_vpath_rts_rth_itable_set(
			struct __vxge_hw_vpath_handle **vpath_handles,
			u32 vpath_count,
			u8 *mtable,
			u8 *itable,
			u32 itable_size)
{
	u32 i, j, action, rts_table;
	u64 data0;
	u64 data1;
	u32 max_entries;
	enum vxge_hw_status status = VXGE_HW_OK;
	struct __vxge_hw_vpath_handle *vp = vpath_handles[0];

	if (vp == NULL) {
		status = VXGE_HW_ERR_INVALID_HANDLE;
		goto exit;
	}

	max_entries = (((u32)1) << itable_size);

	if (vp->vpath->hldev->config.rth_it_type
				== VXGE_HW_RTH_IT_TYPE_SOLO_IT) {
		action = VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_WRITE_ENTRY;
		rts_table =
			VXGE_HW_RTS_ACS_STEER_CTRL_DATA_STRUCT_SEL_RTH_SOLO_IT;

		for (j = 0; j < max_entries; j++) {

			data1 = 0;

			data0 =
			VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_SOLO_IT_BUCKET_DATA(
				itable[j]);

			status = __vxge_hw_vpath_rts_table_set(vpath_handles[0],
				action, rts_table, j, data0, data1);

			if (status != VXGE_HW_OK)
				goto exit;
		}

		for (j = 0; j < max_entries; j++) {

			data1 = 0;

			data0 =
			VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_SOLO_IT_ENTRY_EN |
			VXGE_HW_RTS_ACCESS_STEER_DATA0_RTH_SOLO_IT_BUCKET_DATA(
				itable[j]);

			status = __vxge_hw_vpath_rts_table_set(
				vpath_handles[mtable[itable[j]]], action,
				rts_table, j, data0, data1);

			if (status != VXGE_HW_OK)
				goto exit;
		}
	} else {
		action = VXGE_HW_RTS_ACCESS_STEER_CTRL_ACTION_WRITE_ENTRY;
		rts_table =
			VXGE_HW_RTS_ACS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT;
		for (i = 0; i < vpath_count; i++) {

			for (j = 0; j < max_entries;) {

				data0 = 0;
				data1 = 0;

				while (j < max_entries) {
					if (mtable[itable[j]] != i) {
						j++;
						continue;
					}
					vxge_hw_rts_rth_data0_data1_get(j,
						&data0, &data1, 1, itable);
					j++;
					break;
				}

				while (j < max_entries) {
					if (mtable[itable[j]] != i) {
						j++;
						continue;
					}
					vxge_hw_rts_rth_data0_data1_get(j,
						&data0, &data1, 2, itable);
					j++;
					break;
				}

				while (j < max_entries) {
					if (mtable[itable[j]] != i) {
						j++;
						continue;
					}
					vxge_hw_rts_rth_data0_data1_get(j,
						&data0, &data1, 3, itable);
					j++;
					break;
				}

				while (j < max_entries) {
					if (mtable[itable[j]] != i) {
						j++;
						continue;
					}
					vxge_hw_rts_rth_data0_data1_get(j,
						&data0, &data1, 4, itable);
					j++;
					break;
				}

				if (data0 != 0) {
					status = __vxge_hw_vpath_rts_table_set(
							vpath_handles[i],
							action, rts_table,
							0, data0, data1);

					if (status != VXGE_HW_OK)
						goto exit;
				}
			}
		}
	}
exit:
	return status;
}

/**
 * vxge_hw_vpath_check_leak - Check for memory leak
 * @ringh: Handle to the ring object used for receive
 *
 * If PRC_RXD_DOORBELL_VPn.NEW_QW_CNT is larger or equal to
 * PRC_CFG6_VPn.RXD_SPAT then a leak has occurred.
 * Returns: VXGE_HW_FAIL, if leak has occurred.
 *
 */
enum vxge_hw_status
vxge_hw_vpath_check_leak(struct __vxge_hw_ring *ring)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	u64 rxd_new_count, rxd_spat;

	if (ring == NULL)
		return status;

	rxd_new_count = readl(&ring->vp_reg->prc_rxd_doorbell);
	rxd_spat = readq(&ring->vp_reg->prc_cfg6);
	rxd_spat = VXGE_HW_PRC_CFG6_RXD_SPAT(rxd_spat);

	if (rxd_new_count >= rxd_spat)
		status = VXGE_HW_FAIL;

	return status;
}

/*
 * __vxge_hw_vpath_mgmt_read
 * This routine reads the vpath_mgmt registers
 */
static enum vxge_hw_status
__vxge_hw_vpath_mgmt_read(
	struct __vxge_hw_device *hldev,
	struct __vxge_hw_virtualpath *vpath)
{
	u32 i, mtu = 0, max_pyld = 0;
	u64 val64;
	enum vxge_hw_status status = VXGE_HW_OK;

	for (i = 0; i < VXGE_HW_MAC_MAX_MAC_PORT_ID; i++) {

		val64 = readq(&vpath->vpmgmt_reg->
				rxmac_cfg0_port_vpmgmt_clone[i]);
		max_pyld =
			(u32)
			VXGE_HW_RXMAC_CFG0_PORT_VPMGMT_CLONE_GET_MAX_PYLD_LEN
			(val64);
		if (mtu < max_pyld)
			mtu = max_pyld;
	}

	vpath->max_mtu = mtu + VXGE_HW_MAC_HEADER_MAX_SIZE;

	val64 = readq(&vpath->vpmgmt_reg->xmac_vsport_choices_vp);

	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {
		if (val64 & vxge_mBIT(i))
			vpath->vsport_number = i;
	}

	val64 = readq(&vpath->vpmgmt_reg->xgmac_gen_status_vpmgmt_clone);

	if (val64 & VXGE_HW_XGMAC_GEN_STATUS_VPMGMT_CLONE_XMACJ_NTWK_OK)
		VXGE_HW_DEVICE_LINK_STATE_SET(vpath->hldev, VXGE_HW_LINK_UP);
	else
		VXGE_HW_DEVICE_LINK_STATE_SET(vpath->hldev, VXGE_HW_LINK_DOWN);

	return status;
}

/*
 * __vxge_hw_vpath_reset_check - Check if resetting the vpath completed
 * This routine checks the vpath_rst_in_prog register to see if
 * adapter completed the reset process for the vpath
 */
enum vxge_hw_status
__vxge_hw_vpath_reset_check(struct __vxge_hw_virtualpath *vpath)
{
	enum vxge_hw_status status;

	status = __vxge_hw_device_register_poll(
			&vpath->hldev->common_reg->vpath_rst_in_prog,
			VXGE_HW_VPATH_RST_IN_PROG_VPATH_RST_IN_PROG(
				1 << (16 - vpath->vp_id)),
			vpath->hldev->config.device_poll_millis);

	return status;
}

/*
 * __vxge_hw_vpath_reset
 * This routine resets the vpath on the device
 */
enum vxge_hw_status
__vxge_hw_vpath_reset(struct __vxge_hw_device *hldev, u32 vp_id)
{
	u64 val64;
	enum vxge_hw_status status = VXGE_HW_OK;

	val64 = VXGE_HW_CMN_RSTHDLR_CFG0_SW_RESET_VPATH(1 << (16 - vp_id));

	__vxge_hw_pio_mem_write32_upper((u32)vxge_bVALn(val64, 0, 32),
				&hldev->common_reg->cmn_rsthdlr_cfg0);

	return status;
}

/*
 * __vxge_hw_vpath_sw_reset
 * This routine resets the vpath structures
 */
enum vxge_hw_status
__vxge_hw_vpath_sw_reset(struct __vxge_hw_device *hldev, u32 vp_id)
{
	enum vxge_hw_status status = VXGE_HW_OK;
	struct __vxge_hw_virtualpath *vpath;

	vpath = (struct __vxge_hw_virtualpath *)&hldev->virtual_paths[vp_id];

	if (vpath->ringh) {
		status = __vxge_hw_ring_reset(vpath->ringh);
		if (status != VXGE_HW_OK)
			goto exit;
	}

	if (vpath->fifoh)
		status = __vxge_hw_fifo_reset(vpath->fifoh);
exit:
	return status;
}

/*
 * __vxge_hw_vpath_prc_configure
 * This routine configures the prc registers of virtual path using the config
 * passed
 */
void
__vxge_hw_vpath_prc_configure(struct __vxge_hw_device *hldev, u32 vp_id)
{
	u64 val64;
	struct __vxge_hw_virtualpath *vpath;
	struct vxge_hw_vp_config *vp_config;
	struct vxge_hw_vpath_reg __iomem *vp_reg;

	vpath = &hldev->virtual_paths[vp_id];
	vp_reg = vpath->vp_reg;
	vp_config = vpath->vp_config;

	if (vp_config->ring.enable == VXGE_HW_RING_DISABLE)
		return;

	val64 = readq(&vp_reg->prc_cfg1);
	val64 |= VXGE_HW_PRC_CFG1_RTI_TINT_DISABLE;
	writeq(val64, &vp_reg->prc_cfg1);

	val64 = readq(&vpath->vp_reg->prc_cfg6);
	val64 |= VXGE_HW_PRC_CFG6_DOORBELL_MODE_EN;
	writeq(val64, &vpath->vp_reg->prc_cfg6);

	val64 = readq(&vp_reg->prc_cfg7);

	if (vpath->vp_config->ring.scatter_mode !=
		VXGE_HW_RING_SCATTER_MODE_USE_FLASH_DEFAULT) {

		val64 &= ~VXGE_HW_PRC_CFG7_SCATTER_MODE(0x3);

		switch (vpath->vp_config->ring.scatter_mode) {
		case VXGE_HW_RING_SCATTER_MODE_A:
			val64 |= VXGE_HW_PRC_CFG7_SCATTER_MODE(
					VXGE_HW_PRC_CFG7_SCATTER_MODE_A);
			break;
		case VXGE_HW_RING_SCATTER_MODE_B:
			val64 |= VXGE_HW_PRC_CFG7_SCATTER_MODE(
					VXGE_HW_PRC_CFG7_SCATTER_MODE_B);
			break;
		case VXGE_HW_RING_SCATTER_MODE_C:
			val64 |= VXGE_HW_PRC_CFG7_SCATTER_MODE(
					VXGE_HW_PRC_CFG7_SCATTER_MODE_C);
			break;
		}
	}

	writeq(val64, &vp_reg->prc_cfg7);

	writeq(VXGE_HW_PRC_CFG5_RXD0_ADD(
				__vxge_hw_ring_first_block_address_get(
					vpath->ringh) >> 3), &vp_reg->prc_cfg5);

	val64 = readq(&vp_reg->prc_cfg4);
	val64 |= VXGE_HW_PRC_CFG4_IN_SVC;
	val64 &= ~VXGE_HW_PRC_CFG4_RING_MODE(0x3);

	val64 |= VXGE_HW_PRC_CFG4_RING_MODE(
			VXGE_HW_PRC_CFG4_RING_MODE_ONE_BUFFER);

	if (hldev->config.rth_en == VXGE_HW_RTH_DISABLE)
		val64 |= VXGE_HW_PRC_CFG4_RTH_DISABLE;
	else
		val64 &= ~VXGE_HW_PRC_CFG4_RTH_DISABLE;

	writeq(val64, &vp_reg->prc_cfg4);
	return;
}

/*
 * __vxge_hw_vpath_kdfc_configure
 * This routine configures the kdfc registers of virtual path using the
 * config passed
 */
enum vxge_hw_status
__vxge_hw_vpath_kdfc_configure(struct __vxge_hw_device *hldev, u32 vp_id)
{
	u64 val64;
	u64 vpath_stride;
	enum vxge_hw_status status = VXGE_HW_OK;
	struct __vxge_hw_virtualpath *vpath;
	struct vxge_hw_vpath_reg __iomem *vp_reg;

	vpath = &hldev->virtual_paths[vp_id];
	vp_reg = vpath->vp_reg;
	status = __vxge_hw_kdfc_swapper_set(hldev->legacy_reg, vp_reg);

	if (status != VXGE_HW_OK)
		goto exit;

	val64 = readq(&vp_reg->kdfc_drbl_triplet_total);

	vpath->max_kdfc_db =
		(u32)VXGE_HW_KDFC_DRBL_TRIPLET_TOTAL_GET_KDFC_MAX_SIZE(
			val64+1)/2;

	if (vpath->vp_config->fifo.enable == VXGE_HW_FIFO_ENABLE) {

		vpath->max_nofl_db = vpath->max_kdfc_db;

		if (vpath->max_nofl_db <
			((vpath->vp_config->fifo.memblock_size /
			(vpath->vp_config->fifo.max_frags *
			sizeof(struct vxge_hw_fifo_txd))) *
			vpath->vp_config->fifo.fifo_blocks)) {

			return VXGE_HW_BADCFG_FIFO_BLOCKS;
		}
		val64 = VXGE_HW_KDFC_FIFO_TRPL_PARTITION_LENGTH_0(
				(vpath->max_nofl_db*2)-1);
	}

	writeq(val64, &vp_reg->kdfc_fifo_trpl_partition);

	writeq(VXGE_HW_KDFC_FIFO_TRPL_CTRL_TRIPLET_ENABLE,
		&vp_reg->kdfc_fifo_trpl_ctrl);

	val64 = readq(&vp_reg->kdfc_trpl_fifo_0_ctrl);

	val64 &= ~(VXGE_HW_KDFC_TRPL_FIFO_0_CTRL_MODE(0x3) |
		   VXGE_HW_KDFC_TRPL_FIFO_0_CTRL_SELECT(0xFF));

	val64 |= VXGE_HW_KDFC_TRPL_FIFO_0_CTRL_MODE(
		 VXGE_HW_KDFC_TRPL_FIFO_0_CTRL_MODE_NON_OFFLOAD_ONLY) |
#ifndef __BIG_ENDIAN
		 VXGE_HW_KDFC_TRPL_FIFO_0_CTRL_SWAP_EN |
#endif
		 VXGE_HW_KDFC_TRPL_FIFO_0_CTRL_SELECT(0);

	writeq(val64, &vp_reg->kdfc_trpl_fifo_0_ctrl);
	writeq((u64)0, &vp_reg->kdfc_trpl_fifo_0_wb_address);
	wmb();
	vpath_stride = readq(&hldev->toc_reg->toc_kdfc_vpath_stride);

	vpath->nofl_db =
		(struct __vxge_hw_non_offload_db_wrapper __iomem *)
		(hldev->kdfc + (vp_id *
		VXGE_HW_TOC_KDFC_VPATH_STRIDE_GET_TOC_KDFC_VPATH_STRIDE(
					vpath_stride)));
exit:
	return status;
}

/*
 * __vxge_hw_vpath_mac_configure
 * This routine configures the mac of virtual path using the config passed
 */
enum vxge_hw_status
__vxge_hw_vpath_mac_configure(struct __vxge_hw_device *hldev, u32 vp_id)
{
	u64 val64;
	enum vxge_hw_status status = VXGE_HW_OK;
	struct __vxge_hw_virtualpath *vpath;
	struct vxge_hw_vp_config *vp_config;
	struct vxge_hw_vpath_reg __iomem *vp_reg;

	vpath = &hldev->virtual_paths[vp_id];
	vp_reg = vpath->vp_reg;
	vp_config = vpath->vp_config;

	writeq(VXGE_HW_XMAC_VSPORT_CHOICE_VSPORT_NUMBER(
			vpath->vsport_number), &vp_reg->xmac_vsport_choice);

	if (vp_config->ring.enable == VXGE_HW_RING_ENABLE) {

		val64 = readq(&vp_reg->xmac_rpa_vcfg);

		if (vp_config->rpa_strip_vlan_tag !=
			VXGE_HW_VPATH_RPA_STRIP_VLAN_TAG_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_strip_vlan_tag)
				val64 |= VXGE_HW_XMAC_RPA_VCFG_STRIP_VLAN_TAG;
			else
				val64 &= ~VXGE_HW_XMAC_RPA_VCFG_STRIP_VLAN_TAG;
		}

		writeq(val64, &vp_reg->xmac_rpa_vcfg);
		val64 = readq(&vp_reg->rxmac_vcfg0);

		if (vp_config->mtu !=
				VXGE_HW_VPATH_USE_FLASH_DEFAULT_INITIAL_MTU) {
			val64 &= ~VXGE_HW_RXMAC_VCFG0_RTS_MAX_FRM_LEN(0x3fff);
			if ((vp_config->mtu  +
				VXGE_HW_MAC_HEADER_MAX_SIZE) < vpath->max_mtu)
				val64 |= VXGE_HW_RXMAC_VCFG0_RTS_MAX_FRM_LEN(
					vp_config->mtu  +
					VXGE_HW_MAC_HEADER_MAX_SIZE);
			else
				val64 |= VXGE_HW_RXMAC_VCFG0_RTS_MAX_FRM_LEN(
					vpath->max_mtu);
		}

		writeq(val64, &vp_reg->rxmac_vcfg0);

		val64 = readq(&vp_reg->rxmac_vcfg1);

		val64 &= ~(VXGE_HW_RXMAC_VCFG1_RTS_RTH_MULTI_IT_BD_MODE(0x3) |
			VXGE_HW_RXMAC_VCFG1_RTS_RTH_MULTI_IT_EN_MODE);

		if (hldev->config.rth_it_type ==
				VXGE_HW_RTH_IT_TYPE_MULTI_IT) {
			val64 |= VXGE_HW_RXMAC_VCFG1_RTS_RTH_MULTI_IT_BD_MODE(
				0x2) |
				VXGE_HW_RXMAC_VCFG1_RTS_RTH_MULTI_IT_EN_MODE;
		}

		writeq(val64, &vp_reg->rxmac_vcfg1);
	}
	return status;
}

/*
 * __vxge_hw_vpath_tim_configure
 * This routine configures the tim registers of virtual path using the config
 * passed
 */
enum vxge_hw_status
__vxge_hw_vpath_tim_configure(struct __vxge_hw_device *hldev, u32 vp_id)
{
	u64 val64;
	enum vxge_hw_status status = VXGE_HW_OK;
	struct __vxge_hw_virtualpath *vpath;
	struct vxge_hw_vpath_reg __iomem *vp_reg;
	struct vxge_hw_vp_config *config;

	vpath = &hldev->virtual_paths[vp_id];
	vp_reg = vpath->vp_reg;
	config = vpath->vp_config;

	writeq((u64)0, &vp_reg->tim_dest_addr);
	writeq((u64)0, &vp_reg->tim_vpath_map);
	writeq((u64)0, &vp_reg->tim_bitmap);
	writeq((u64)0, &vp_reg->tim_remap);

	if (config->ring.enable == VXGE_HW_RING_ENABLE)
		writeq(VXGE_HW_TIM_RING_ASSN_INT_NUM(
			(vp_id * VXGE_HW_MAX_INTR_PER_VP) +
			VXGE_HW_VPATH_INTR_RX), &vp_reg->tim_ring_assn);

	val64 = readq(&vp_reg->tim_pci_cfg);
	val64 |= VXGE_HW_TIM_PCI_CFG_ADD_PAD;
	writeq(val64, &vp_reg->tim_pci_cfg);

	if (config->fifo.enable == VXGE_HW_FIFO_ENABLE) {

		***** = readq(*************cfg1_int_num[oftware VPATH_INTR_TX]******************tti.btimer_val ! software USE_FLASH_DEFAULT and	distribu&= ~oftware ****CFG1l Pu_NUM_BTIMER_VAL(d on	0x3f
 * r);d on or de| software his code fall under the GPL and m	PL), incorporated heretain }d distriburived from this code fall undeITMP_ENicense (GPL), incorpoated hac_enin by reference.
 * Drivers based on
 * system is licensed unde)and mhe authorship, copyright and licenr the ACain telse informatiorived from this code fall under for Neterrogra
 * system is licensedcinder the GPL.
 * See the file COPYING in this distribution fo * Coe information.
 *
 * vxge-config.c: Driver forCIeterion Inc's X3100 Series 10GbE PCIe I/O
 *           *****/irtualized Server Adaurange_ain by reference.
 * Drivers based on or derived from this code fall undURNG_A(ust
tain the authorship, copyright and licennfig.h" This file is not
.h>
#incete prograclude <linux/pci.h>
#inblude <linux/pci_hotplug.h>

#include "vxge-traffic.h"
#include "vxge-config.B"

/*
 * __vxge_hw_channel_allocate - Allocate memBry for channel
 * This funbtion allocates required memory foclude <linux/pci_hotplug.h>

#include "vxge-traffic.h"
#include "vxge-config.C"

/*
 * __vxge_hw_channel_allocate - Allocate memCry for channel
 * This funcete progra***************************terms of
 * the GNU General Public Licdistributed according to the ter2s of
 * the GNU General Public License (GPL), incorpouecnclude <linux/pci_hotplug.h>

#include "vxge-traffic.h"
#includ2- AllocateEC.h"

* retain the authorship, copyrightfault:
		break;
 This  channel
 * ThL_TYtion allocates required memecor the channel and various arrays
 * in the channel
 */
struct fault:
		breakel*


	channel = kzalloc(size, GFP_KERNEL);
	if (chath_handlNULL)
		goto exit__vxge_hw_channel_type type,ec length, u32 per_dtr_space, void *userdata)
{
	struct __vxge_hwfault:
		breakel;


	channel = kzalloc(size, GFP_KERNEL);
	if (cha
	u32 vpNULL)
		goto exit>vpath->hldse VXGE_HW_CHANNEL_Tddata;
	channel->per_dtr_space = per_dtr_space;
	channel->length = length;
	Dhannel->vp_id = vp_id;

	channel->work_arr = kzalloDnel == NULL)
		goto exitdvpath->hldev;
	vp_id = vph->vpath->vp_idf(struct __vxge_hw_fifo);
		break;
distributed according to the ter3s of
 * the GNU General Public License (GPL), incorpoated hr* Copyright(c) 2002-2009 Neterion Inc.
 **********************hannee information.
 *
 * vxge-conf3O
 *           R****/
#include <linux/vmalloc.h>
#includannel->orig_arr == NULRNEL);
	if (channel->rated herein by reference.
 * Drivers based on or derived from this codannel->oriRe notice.  This ust
 * retain the authorship, copyrightated for channel
 * This fuchannel);

exit0:
	retFP_KERNEL);
	if (channel->wtil_seturn NULL;
}

/*
 * __vxge_hw_channel_free - Free memory allocated for cUTIL_SEL"

/*
 * __vxge_hw_channel_allocatechannel->free_arr);y for channel
 * Thuct __vFP_KERNEL);
	if (channel->lit0:
	return NULL;
}

/*
 * __vxge_hw_channel_free - Free memory allocated for cLannel
 * This function deallocates memory from the channel and va setting the
 * v_channel_initialize - vpath->hldev;
	vp_id = vph->vpath->vp_idrr == NULL)
		goto exit1;
	channel-nnel_************ring*
 * This software RINGbe used and distributed according to the terms of
 * the GNU General PublRc License (GPL), incrrporated herein by reference.
 * Drivers based on or derived from this code fall under the GPL and mmust
 * retain the authorship, copyright and license notice.  This file is annel->orig_arete program and may only be used when the entire operating
 * system iannecensed under the GPL.
 * See the file COPYING in this distrE_HW_CHANNEL_TYe information.
 *
 * vxge-config.c: Driver for Neterion Inc's X3100 Series 10GbE PCIe I/O
 *                Virtualized ServerE_HW_CHANN * Copyright(c) 2002-2009 Neterion Inc.
 ************_OK;
}

/*
 * _***************************************************/
#include <linux/vmalloc.h>
#include <linux/etherdevice.h>
#include <linuxanne.h>
#include <linux/pci_hotplug.h>

#include "vxge-traffic.h"
#include "vxge-config.h"

/*
 * __vxge_hw_channel_allocate - Allocate memory for channel
el *channel)FP_KERNEL);
	if (channel *channelr the channel and various arrays
 * in the channel
 */
struct __vxge_hw_channel*
__vxge_hw_channel_allocate(struct __vxge_hw_vpath_handle *vph,
hannel->freefree_arr != NULL)
			channel->frelength, u32 per_dtr_space, void *userdata)
{
	struct __vxge_hw_channel *channel;
	struct __vxge_hw_device *hldev;
	int size = 0;
	u32 vp_id;

	hrve_top = 0;vpath->hldev;
	vp_id = vph->vpath->vp_id;

	switch (type) {
	case VXGl->lenHANNEL_TYPE_FIFO:
		size = sizeof(struct __vxge_hw_fifo);
		bl->length; i++)
			channeEL_TYPE_RING:
		size = sizeof(struct __vxge_hw_ring);
		break;
	default:
		break;
	}

	channel = kzalloc(size, GFP_KERNEL);
	if (channel == NULL)
		gcmd = 0;
free_arr != NULL)
			channe>item);

	channel->common_reg = hldev->common_reg;
	channel->first_vp_id = hldev->first_vp_id;
	channel->type = type;
	channel->devh = hldev;
	channcmd = 0;>length;
	channel->reserve_tuserdata;
	channel->per_dtr_space = per_dtr_space;
	channel->length = length;
	channel->vp_id = vp_id;

	channel->work_arr = kzalloc(sizeof(void *)*cmd = 0;GFP_KERNEL);
	if (channcmd = 0;_arr == NULL)
		goto exit1;

	channel->free_arr = kzalloc(sizeof(void *)*length, GFP_KERNEL);
	if (channel->free_arr == NULL)
		goto exit1;
	chanIL;

	ude_ptr = length;

	channel->reserve_arr = kzalloc(sizeof(void *)*length hw resets.
 */
void
__vxge_hw_device_rr == NULL)
		goto exit1;
	chl->length; i++)
			channeth;
	channel->reserve_top = 0;

	channel->orig_arr = kzalloCheck if vpath )*length, GFP_KERNEL);
	if (channel->orig_arr == NULL)
		goto exit1;

	return channel;
exit1:
	__vxge_hw_channel_free(channel)annexit0:
	return NULL;
}

/*
 * __vxge_hw_channel_free - Free memory allocated for channel
 * This function deallocates memory from the channel and various arrays
 * in the chprog)
{
	enum tatus ret = VXGE_HW_FAIL;

uct __vxge_hw_channel *channel)
{
	kfree(channel->work_arr);
	kfree(channel->free_arr);
	kfree(channel->reserve_arr);
	kfree(channel->orig_arr);
	kfree(ch * __vxge_hwtatus ret = VXGE_HW_FAIL;
tialize - Initialize a channel
 * This function initializes a channel by properly setting the
 * various references
 */
enum vxge_hw_status
__vxge_hw_channel_initialize(strtoc_get(void _channel *channel)
{
	u32 i;
	struct __vxge_hw_virtualpath *vpath;

	v hw resrogrstribute0******************************terms of
 * the GNU General PublEINTAw resgth;

	channel->reserve_arr = kzalloc(sizeof(void *)*lengtht_pointer);
	toc = (struct vxge_hw_toc_rrr == NULL)
		goto exit1;
	cht_pointer);
	toc = (struct vxge_hw_toc_rms of
 * the GNU General PublBMAPnter);
	toc = (struct vxge_hw_toc_reg __iomem *)(bar0+val64);
exregister location pointers in the devicrr == NULL)
		goto exit1;
	chregister
	return status;
}

/*
 * __vxge_hw_vpath_initializestruThis routine is the final phase of ice  whichstatu*hldev32 i;struregistershw_s i;
hw_de using_reg ******uration passed.
 */
enum  __vxge_reg_ad
ct __vxge_hw_device *hldev(structuct __vxge_device *hldev, u32 vp_id)
{
	u64 *****;
	>toc_al32;
	ev->bar0;

	hldev->_reg_ad  software OK;
	oc_get(hldev->barvirtual (str*hw_de(&hldev->t __vxge_hw_devreg __iomeminte(str_hw_= (str= &(hlde->c_commo_w_des[_reg ]********!(m *)(hldw_devassignments &commonmBIT(_reg  )ased o
	}

	val64 = reaERRGeneralNOT_AVAILusedresegoto exit
	if 	w_comm = = (st(&hlcommon_r
	}

	valuct __vxge_hw_devswapper_set( val64);

	fo*********
	}

	vn by refereOKe in__iomem *)((i = 0; i < VXGE_HW_TITAN_SRmac_w_legacye readq,c_reg  val64 = readq(&hldev->toc_reg->toc_srpcim_pointer[i]);
		hldev->srpcim_kdfg[i] =
			(struct vxge_hw_srpcim_reg __iomem *)
				(hldev->bar0 + val64);
	}

ct __vxge_hw_dev	val6] =
			(struct vxge_hw_srpcim_reg __iomem *)
				(hldev->bar0 + val*******0***********gendmas ofw_srpstributed according to rt (i rd_optimiz_reg _ctry_re
	/* Get MRRS = Vue from 0);
	ifcontrol)hldi = 0; imgmt_pointer[i]);
	****d acACES; , 1, 0x78**** VXGw_srpcim_reg __is software OKased o __io = ****32 &software ****EXP_DEVCTL_READRQ) >> 1GE_H X3100 Se
		    ~(nnel->resTDMA_RD_OPTIMIZATION_CTRL_FB_FILL_THRESH(7) resets.
 *|TOC_GET_FC_INITIAL_BIR(val64)) {
	case 0:
		hldev->kdfc =  __iomem *ormation.
 *
 * vxAL_BIR(val64)) {
	case 0:
		hlWAIT_FOR_SPAC_regf (status riveDFC_INITIAL_BIR(val64)) {
	case 0:
		hlADDR_BDRY (u8 __omem *)(hlev->bar0 +
			VXGE_HW_TOC_GET_KDFC_INITIAL>vpath_rsthldev->bar0 +
	MAX_PAYLOAD_SIZE_51omem *k;
	default:
		break;
	}

	status = __vxge_hw_d>vpath_rsperat);
	toc = (struct vxge_hG_SPACES; i++) {
		val64 = reaem *:w_device_reg_addr_get(struct __vxge_hwvice *hldev - I_reg->titaV_commo P(stroc_geturv)
{
	u64 val64;
	u32 i;
ice *hl vxge_hw_status statureset32 i;
= (strandstrus = VXGE_HW_OK; software supporte_id =
		(s *)hldev->bar0;

	hldev->toc_reg = __v_hw_device_toc_get(hldev->bar0);
	if (hldev->toc_reg ,d onldev->common_reg dev->vp *ev->vp == Nldev->toc_reg->toc_common_pointer);
	hW_FAIL;
		goto exit;
	}

	val64 = readq(&val64 = readq(&hldev->toc_reg->toc_mrpcim_pointer);
	hldev->mrpcim_reg =
		(struct vxge_hw_mrpcim_reg __iomem *)(hld_reg __iomem *)(hldev->bar0 + val64);

	va val64);

id0 + v_id_proval64);

openval64 = reaVP_OPid __

	swit(hlde = (hldeGE_HW_NO_M4);

	retu=hw_legaL_FUNCTION:
	bar0 +readq(&hldev-regal64);

	;

	switchmgmtess_rights |= VXMRPCIM |al64);

	vact __vxge_hw_dev4);

struct vxge_hw_srptoc_vpmgmt_pointer[i]);
	4);

_checkACES; w_srpcim_reg __iomem *)
				(hsed omem_SPACES; , 0, sizeoftoc_get(hldev->barc_common_pou8 __ist_type, u32 funtoc_vpmgmt_pointer[i]);
	MRPCIM adstruct vxghts |= VXGE_HW_DEVICE_ACCESS_RIGHT_MRPCIM |
				VXGE_HW_DEVICE_ACCESS_RIGHT_SRPCIM;
		break;
	case VXGE_HW_NO_MRINIT_LIST_HEADordial64);
_devhandlesce id e VXGEs	hldev	valem *)(hl	case.sar0);_info
	case.hw_devicfoal64);

	vaoftware DEVICEopyrifallMASK_SET readq(&**** of
mask0_MIN_VH_NORMAL_FUNCTION1se VXGE_HW_MR_NO_SR_VH0_BASE_FUNCTIONhw_device_ttruct vxge_hw_srpcim_reg __iomem *)
				(hlde
	hldev->minoterminatreturn access_rig
{
	u64 val64;

	val64 = readq(&hldev->commo
 * This  - Tdevice_hohldev->device_id =
		(u16)VXGE_HW_TITANcloses all channels it (hosedor_r freeup memory*)hldvoid

	hldev->mino
 * This roc_get(hldev->bar0);
	if (hldev->toc_reg  == Nldev->toc_reg->toc_common_pointer);
	_reg __iomem *)(hldev->bar0 + val64);

	val64 

	switch (host__type) {
	casge_he VXg->toc_srpcim_poiW_SR_VH_FUNCTION0:
	case VXGREE_HWHW_NO_MR_NO_ORMAL_FUNCTION:
		a		if (!(hldev->vpath_assigE_HW_E_ACCESS_R resE_HW_MR_SR_VHbar0);LID_CONFIG:
		break;
	casE_ACCESS_R] = NULLval6CIM |
				VXGE_HW_DEVICE_ACCESS_RIGHT_SRPCIM;
		break;
	
{
	u64 val64dr_get(struCTION0:
		accestuG_SP - SldevTU *)hdev->new>firoc_reg. Example, to use jumbo frames:ype, hldev->func_id);

		(myr0);
	i, 9600);*)hldev->bar0;

	hldev->trify_pci_e_info - Vali_ACCESS_RIGHT_SRPCHW_SR_VH_VIintev->tocnewid); == NULL) {
		staW_FAIL;
		goto exit;
	}

	val64 = readq(&hldev->toc_reg->toc_common_pointer);
	(&hldev->vpa[i])	hldev->mrpcim_reg =
		(strINVALID_HANDm_reg __iomem *)(hldev- __iomvp(&hldevnegos
__vxg + software MACON:
	ERreturthe negotiat(, PCI_CA<ID_EXP);
	IN_MTU) || ->pdev, e>ue;

		hmax_vxgee ind speed from PCI config spaceMTUig_word(hstributed accordial64);

	fo->rxreg[vcfg linn_prog_checklt:
		breX	pciVCFG0_RTSreturFRM_LEN"

/*retaink;
	default:
		brese PCIE_LNK_WIDTH_RESRV:
	cs
__vxgev->bar0 + v**********
	switch ((lnk & PCI_EXP_LNKST	default:********I_CA=us
__vxg -ID_EXP);
	pci_read_config_word(
{
	u64 val64;

	val64 = readq( __vxge_hw_dev(host- Ohosta c_commo pabiloe *hgivce *dapter)
{
	u64 funcreg _is
}

drn;
(hostaccessrn;
ldev)
{
	if ((f ant_vpt_type  for offload, GROL_FUy_reg s.VXGE_HW_NO_MR_Ndevicest_vpsynchronously *)hldev->bar0;

	hldev->td(struct __vxge_hwdev->host_type =
	   (u32)VXGE_HOC_GETldev->common_reg =
	(attr *RATI VXGE_HW_ERR_Pct __vxge_hw_devum vxge_nter);- Rebal/*
 * __vxge_hw_device_access_rights_get: Ging rate.
 */
static enum vxge_hwi_e_info(struct __vxge_hw_devval64);

	hldev->vpath_assignmentsRATI>vpmgmt_dq(&hldev->common_reg->vpath_assignme;

	fth and speed from PCI config spaceSTAT_reg __iom __vxge_hw_em *1W_NO_MR_SR_VH0_FUNCTION0:
		eak;
	}

	return ac_hw_device _MINOem *)(hlev->vp.D_PCI_INFe_hw_device *w_srpcim_reg __iomem *)
				(hldev->baften = 1;
	enum vxRR_Ial64ing rate.
 */
static enum vxge_e invmalloc(_DEVICE_ACCESS_RIGHT_SRPC the RX_WRR  restiated link width and speed from PCI confOUT_OF_MEMORY j, how_often = 1;
	enumGE_H funCIM |
			GE_HW_DEVICE_ACCESS_RIGHT_SRPCi < VXGE_HW_WRRRR_I(&hldev0 + val6dq(&hldev->common_*************
 * This software may be used andXP_LNKSTA_Cct __vxge_****_creis rtran&_hw_de i));RATI rese4 = readq(&hldev->toc_reg->teset the priorities a6 = channel-_INVALID_PCI_INFO;ath;

	if ((channel->reserve_arr != NUfc_w_round_robin_0) +ath;;
		writeq(0, ((&hleq(VXmrpcim_reg->kdfc_w_round_robin_20) + i));
	}

	/* Assign W7	breapath_reg[i] =
			rg[i] =
			(struct v_hw_device ;
	if (st	defau****h->tx_FUNr
 *  TOC_(_hw_device  *routine retur PublPhe GP)  + + ihe GNU General Publicval64);

MR_SR_V_blocknd_robin_0) +ev->mpooldfc_en_eceivis routineand moftware BLOCKig_wow_srpcim_FUNCTION &hldev->mrpNT; i++)
		writeq(0, ((&hldev->mrpcim_reg->rx_w_round_robin_0) + i));
8(0),
			((&hld
	hldev	val the WRRGE_HW_TITAN_SRP &hldhwak;
	 *)	((&hldMINOR_&hldev->m->memev->m;
		hldev->accerity_0);
	GE_HL_PADEVICE_ACCESShldev->mrpcim_reg->rx_queue*/
	foc_id =
			__vxge_hw_vpath_func_id_get(i, h_hw_device *fc_fiinforx_queue_priori/
	writeq(0ty_0);
	_savfc_fiGE_HW_MR_SR_VHxge_hw_vpath_func_id_get(i,  sere_hw_device *hlim_reg->rx_queue_priori serty_2);


	/* Initialize all the slots as unused */
	for********iteq(0, &hldev->m-> (i addrak;
	default:
		re	write********
	}

	valhldev->mrpcim_reg->r
 * Th(vpWRR_RINGOK)
		goto exit;

	/* Reset the priorities a8
	folisthldeordi->itemak;
	defaultW_SR_VH_VIRTUAL_readq(&hldevs_deployedthormrpcim_point

		hldev->fu
	ce the RX_WRRlity( onc ((&hldev->mrpc.userdata0 + val64)ev->m;)
						CTRL_WRR__HW_MAX_VIRTUAL_PATath;GE_HWdevice_r
 * This rou;) {
				if (wrr:_entry_type_selse
&hldk wid i));

		writeq(VXdeleriteq);	/* Fill the unu7ed slots with ev->m	for (j = 0; j < VXGE_ i));IGHTED_RR_SERVICE_STATES; j6:
	vdev-_RR_SERVICE_STATES; j2:IGHT_SRPCIM;
	t
 * This routine L_WRR_NUMBER(0ften = 1;
	enum :hw_device_reg_addr_get(ilaged(struct __vxgrx_doorbell_pos	hldCal64_reg 			if (got->toc_previousthe prt_vprights L_FUNt_vp@vp: Hctrl) + i));

		writeq(VXGE_H_17_CTRL== VXGE_HW_NO_MR_NO_SR_NORMALval64CTION ||
	hldev->host_typ readt_vpearlier *)hldost_tR(i),
				((&hldev->mrpcimice ling rate.
 */
static enum vxge_hwE_ASSIGNMENTS_GET_HOST_TYPE_ASSIGNMENTS(reg[i]);
NULL)s
__count,) {
		TRY_T1		staoc_get(hldev->barath; *ath;val64);

	hly(hldev->pd	GE_HWn;
				} else
				W_KDFC_END_PCI_INFO;

	switch ((lnk dmem_
	/* resC_ENTRY_TY&= 0x1fffE_LNK_1ibuteDFC_INITIAXDMEM the dPRCE_HW_KDFC_EN
	casFC_EN= VXGE_HW_VPA(&hldev->Y_TYPE_DOORBELL_NEW_QW_CNT
		b164)2);
;
	default:
		reE_HWrxdev->mrpci resghtslFO;

	switch ((lnPE_SEL_0_NUMBER_7(0MBER_4(1)/= GE_HNVALID_PCI_INFO;

	switch ((lnE_HW_fg6IE_LNK_X1: software NTRYCFG6Y_TYPSPA) |
	64IE_LNK_X1:E_SEL_0_readq(
	 * Each RxDNO_Sof 4 qwords, j /FC_ENTRY_TYP-l64 = 64 + 1_8(1),
				&min |
				,HW_KDFC_EN) / 4_hw_din|
				s_limiTYPEROUNtes[j++]);
		valTRY_TYPWRR_RINGtes[j++]);
		val6< 4= 0;tes[j++]);
		val64 4s_privilaged(struct __vxg* Modi>kdfc_fifo_0_ctrl) + i));

		writeq(VXGE_HDFC_FIFO_17_CTRL_fo_17_ctrl) + i));
	}

	/* Modify the servicing algorithm applied to the 3 tyW_FAIL;
		goto exit;_ROBIN_0_NUMBER_2(wd, message and offload */
	writeq(VXGE_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_0(0) |
				VXoc_get(hldev->bar0);
	if dev0) |
				VXGtoc_reg PE_SEL_0_NUMCCESS_Rstatus is_empty = TRU_reget Access Rights of the driver
 * This routRY_TYPE_SEL_0_NUMBER		val64HW_NO_MR_NO_dq(&hldev->common_reg->vpath_assignments);

	fUAL_PATHS; i++reg =
		(struct vxge_he VXGE_eset the prival64	enum	/* Restes[jde		&hl) {
		w_srpcim_!tes[j_statcase VXGE_HW_SR_VH_VIRTi));
	tes[j] == -1) {
					wrr_states[j] = i;
						wrr_states[FALSeset_in_TY_0_wrr_stati));
	}

	/* Set up theFAI		VXto receive queues */
	writeq(		vae sure each fifo irivemrpcim_pointer);fc_entry_type_s0 */
	for (j = 0; j < VXGE_HW_WEIGHTED_RR_SE) {
		if (wrr_states[j] == -1)
			wrr_states[j] = 0;
	}

_entry_type_sel_1);

	/* for (j = 0; j < VXGE_fc_entry_type_seity n		vanue;

		h	writeq(0, ype_seity numberr (i = 0; i < VXGE_HW_MAXity_0);

			VXGE

	switch (host_type) {
	cass assigned {
		wrueues */
	vxge_hw_device_is_privilaged(struct __vxg4);

 - R);

	hXGE_HW_KDXGE_HW_NO_MR_NO_SR_NORMALrequest a64);

E_HWXGE_HW_Kr_states[j++]);
		val64 |= VXGE_HW_K	break, message and offload */
	writeq(VXGE_atic enum
vxge_hw_status vxgGE_HW_KDFC(&hldev->toc_reg->toc_common_pointer);lity(hldev->pdevESS_RIGHT_ROBIN_0_NUMBhldev->mrpcim_reg->kdfc_w_round_robin_20 + i));
	}

	/* Set up the priorities assigned to recGE_HW_NO_MR_SR_VH0_FUNCTION0:
		acce	breakHW_NO_MR_NO_ vxge_hw_s*)
				(hldev->bar0 + val6ents & vxgN:
	case->_HW_N:
		accnt++s the host type assignments
 */
viority_2);

	/* cover_>toc_RX_QUEUEPoll_NO_SE_PRIOco	retteq(&hlre-ice *hldevst_vpXGE_HW_NO_MR_Npoll's_NO_Sldev->majo = 0; i < VXGeg _HW_MAXsion =
		(u8t_vpldev->maj0_FUNCTION0) &&
	(hldev->func_id == 0))
Ring service stateRITY_1_RX_Q_NUMBER_13(13) |
			VXGE_HW_HW_KDFC_ENTRY_TYPE_SEL_0_NUMBER_0(0) |
				VXRX_QUEUE_PRIORITY_1_RX_Q_NUMBoc_get(hldev->bar0);
	if (hldeUMBER_14(14) |,
			&hldev-UND_ROBIN_0_NUMBERY_TYPE_SEL_0_NUMBERR_NO_SR_iteq(val64, (&hldev->mrpcim_reg->kdfc_w_round_robin_20 + i));
	}

	/* Set up the priorities assigned to recrpcim_reg->rx_queue_priority_2);

	/* Iniccess_rights |=; i++) {
		val64 = readq(&hldev->toc_reg->toc_vpmgmt_pointer[i]);
	sw		break;
	case VXGE_HW; i++) {
		val64 = readq(&hldev->toc_reg->toc_vpmgmt_pointer[i]);
	ak;
	}

	return access_rig64 = readq(&hldev->toc_reg->toc_srpcim_poi_HW_RX_QUEUE_PRIORITY_0_RX_Q_NUMBER_5(eq(VXGE_HW_KDFC_FIFO_17_CTR_HW_RX_QUEm_reg->rx_queue_priority_2);

	/* Initialize all the slots as unused */
	for		if (!hldev->config.vp_config[i]min_bandwidth)
			continue;

		how_often = VXGE_HW_VPATH_BANDWIDTH_MAX /
				hldev-GE_HW_KDFC_ENTRY_TY[i].min_bandwidth;
		if (how_often) {

			for (j = 0; jus
__vxge_hw_device_is_privilaged(struct __vxg
 * Thi- E * Thiften = VXhldev)
{
	u64 vaear	hldev->majoE_PRIOthereby 
 * TE_HWaITY_1_RX_Qto_regrt_NO_wardE_HW_vxge_idth)genTION ng interruptIAL_MAJpes of doorbells.
	or (j =, message and offload */
	writeq(VXGE_HW_KDFC_ENTRY_TYP	wrr_states[j] = LL) {
		stt once */
		UND_ROBINl64, (&hl),
				&hldev->mCMN_RSTHDLR code CLtruct vx+) {


		1 << (16 -e each ring is
		*/
	foct __vxge_pio_VXGE*****32_uM_RE((u32) __vxbVALUND_RBIN_0, 32VXGE_His_privilmmon******cmn_rsthdlrl64 =)] = -1;

	/* Prepare the 

			for (j SS_5(
				wrr_s h/w	wriisticIAL_M_5(
				VXGEDMAv->majolaged(hldev Ti;
	_NO_MR_NO_Sto be call	VXGE_HW-
 * Th
		how_oE_HW_MR_n;
}pde_ho0);
	wintoifo_0_reg-mmon_reg->hTION0) &&
	(hldev->func_id == 0))


			for (j =ITY_1_RX_Q_NUMBER_13(13) |
			VXGE_HW_RX_QUEUE_PRIORITY_1_RX_Q_Nice *hldev)
{
	int exp_cap;
	u16 lnk;

	/* Get the negoapability(hldev->pdev
						j += VXGE_HW_MAX_VIRTUAL_PATHS;
					else
						j += how_often;
				} else
					j++;
			}
		}
	memcpytialize aonfig.vp_confrx_queue_priorit[i].min_bandwidth)
			continue;

		how_often = VXGETES; j++) {
		if (wrr_statwriteg|
				VXGEtes[i] = -1;

	ns the host type assignments
 */
void __vxge_hnce(hldev)TION ||-&hldeVXGE_aged(hlde->toc_VXGEv->hos0);
	ioCTRL_omem *common_reg;
	struct (&hloffPRIO(&hlperform anUNCTION ||*)hldev->bar0;

	hldev->toc_reg = __vxge_pcim_reg __iE_ACCESS_RIGHT_SRPCIM;
		breainter);ce_hwomem E_HWNCTION ||v->tochw_vpa, _rob*m_ree_hw_verify_pci_e_info(struct __vxge_hw_device *hldev)
{
	int exp_ommon_reg =
	(struct vxge_hw_common_rdev->mrpcim_reg->kdfc_w_round_robin_20 + i));
	}

	/* Set up the priorities assigned to receive qpcim_reg __i */
	writeq(v->bar0 + val64);

	for (i),
				&et up these PC32 iS_ACCESS_CMval6(NCTION ||) |OC_Geset_in_prog_check(
		(u64 __iSTROBEn_reg->vpath_rst_in_prog);
	if (statOFFSETarr);hw_vpa vxge_hw_device_hw_info _vxge_hw_devi64
		breawrr_s********* & PCiomem *)(bar0cmdeviceldev->mrpst_in_prog);
	if (status != readq(		if (!(hldev-ilaged(0);
	i_!hld_milliRTUAL_ldev-		(hldev->bar0 + val64&& em *)&comm>vpath_assigcheck(OPkdfc)I_EXPw_infuted according to eg->host_type_assiAX_VRX_Won Inc'!((hw_in0VXGE_HW_Riomem *)(bar0 + vhw_toc_reg __iomem *toc;
	struct vxge_hw_mr & PCtxnfo)
{
	u3omem *mrpciTX S_reg;
	strpe =TY_1_RX_Q_NUMBER_12(12) |
			Vtoc_reg = __vxge_pmgmt_reg __iomem( {
			for (j = 0; j < VXGE_HW_WRR_RIN,NULL) {
		status  & PCI]);
		reg __iinter);fo->func_ == NULL)* {
		staint iUMBER_1hw_vpatIRTUAL_PATHS; i+eneralTX>vpath_);

		writeq(val64, (&hldev->mrpcim_reg->kdfc_w,
				&(CE_AC)the prio->func_g->rx_queue_priority_1);

	writeq(VXGE_HW_RX_QUEUE_PRIORITY_2_RX_Q_NUMBER_16(16),
				&hldev->mrpcim_reg->NO_S(i4 = r i <W_DEVICE_ACCESSost_type,
			hw_info->func_0(wr8; i++->kdfc_w_round_robin_0) +m *vpmgmt_reg;
	enuset(hw_inf);

	hw_inHS; i++) {

	ge_hw_v_hw_devitry_type_eg->kdfc_w_round_robin_20) + i));
	em *)(hpath_reTES;eak;
	dinfo)}vxge_hw_toc_reg __iomem *toc;
	struct vxge_hw_mr & PCrreg __iomem *)
				(baR0 + val64);

		hw_info->func_id = __vxge_hw_vpath_func_id_get(i, vpmg_vpath_card_um vxge_hw_status status;

	memset(hw_infreg = (struct vxge_hw_vpat_vpath_cid) &
		_vpath_c_DEVICE_ACCESS_RIGW_FAIL;
		goto exit;
	}

	val64 = readq(&hHT_MRPCIM) {

			val64 = readq(&toc->toc_Rrpcim_pointm_reg __iomem *)
					_vpath_cg->rx_queue_priority_1);

	writeq(VXGE_HW_RX_QUEUE_PRIORITY_2_RX_Q_NUMBER_16(16),
				&hldev->mrpcim_regh_pointer[i]);

		vpath_reg = (struct vxge_hw_vpate_mBIT(i)omem *)(bar0 + val64);

		hw_info->function_mode =
			__vxge_hw_vpath_pci_func_mode_get(i, vpath_re	swi3g);

		status = __vxge_hw_vpath_fw_ver_get(i, vpath__reg, hw_AP_Irx_q;
		if (status != VXGE_HW_OK)
			goto exit;

		status = __vxg __iomem *)
				(ba_is_priwci-e link wiX_Q_NUMBER_12(12) |
			VXe_hw_info *hw_info)
{
	u32S; i++) {

 i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {

		if (!(rpcim_reg->rx_queue_p;
	struct));

	toc = __vxge_hw_device_toc_get(bar0);
	if (toc == NULL) {
		status = VXGE_HW_ERR_CRITICAL;
		goto exit;
	}

	val64 = readq(&toc->toc_common_pointer);
	common_reg = (struct vxge_hw_common_em *)(hldev->bar0 + val64);

	for (istributed according to hw_infdebugits PC lin	status _->ini
 * _mwEG_S_TYP_fifize(he GNU GeneralDEBUGi_func0_GET_INIl undMWR_SE0) |
	OBIN_ev = NULL;
	enum vxge_hw_status status = VGE_HWW_OK;

	status = __rdge_hw_device_config_check(device_config)1
	if (status !RDVXGE_HW_OK)
		goto exit;

	hldev = (struct __vxge_hw_devomem*)
			vmalloc(sizeocpl_rcvd_device_config_check(device_config)2
	if (status CPL_RCVDHW_OK)
		goto exit;

	hldev = (struct __vxge_hw_dev3GE_HW_OK;

	status = __vxgbytege_hw_devinfig_check(device_config)3
	if (status != VBYTEVXGE_HW_OK)
		goto exit;

	hldev = (struct __vxge_hw_devBIN_00, sizeof(struct __vxg	memce_hw_devinfig_check(device_config)4EVICE_MAGIC;

	vxge_hwge_hw_device_debug_set(hldev, VXGE_ERR, VXGE_COMPONENT_5ev, 0, sizeof(wrcrdtarb_xoff_device_config_check(device_config)5
	if WRCRDTARB_XOFFHW_OK)
		goto exit;

	hldev = (struct __vxge_hw_devR_8(1W_OK;

	strdbacks.crit_err = attr->uld_callbacks.crit_err;
6
	if RD_hw_device_pci_e_init(hldev);

	status = __vxge_hw_dgen[i].minC_EN0ice *)
			vmallget(hldev);

	/* Incr_devce_config_check(devGENcheck(COUNT0NULL) PPIF0; i < VXGE_HW_MAX_VIRvicev);

	__vxge_hw_device_host_info_get(hldev);

	/* Incrementing for stats blocks */
	nbloc1s++;

	for (i = 0; i < VXGE_HW_MAX_VIRTUAL_PATHS; i++) {

		if (!(hld1v->vpath_assignments & vxge_mBIT(i)))
			continue;

		if 2ALL);

	/* applnfig->vp_config[i].fis++;

	for (i = 0; i < VXGE_HW_MAX_VI2		sizeATHS; i++) {

		if (!(hld2v->vpath_assignments & vxge_mBIT(i)))
			continue;

		if (device_config->vp_config[i].ring.en3
			nblocks += device_config->vp_config[i].fifo.fifo_blocks;
		nblock3v->vpath_assignments & vxge_mBIT(i)))
			continue;

		if dev = attr->pdeerminate(hldev);
		sts++;

	for (i = 0; i < VXGE_HW_MAX_VIacks.lATHS; i++) {

		if (!(hld4v->vpath_assignments & vxge_mBIT(i)))
			continue;

		if 
	hldev->uld_cace_terminate(hldev);
s++;

	for (i = 0; i < VXGE_HW_MAX_VI
	__vxATHS; i++) {

		if (!(hld5v->vpath_assigtoc_vpmgmt_pointer[i]);
	 vpmgmt_reg);
		if			VXGE&W_OK;

	sth_reg __itates[j] = 0;
	}

	for (i = 0, j = 0; i < VXGE_HW_WRR_RING_COUNT; i++		break;
	}

	for e_terminate(structe_mBIT(i)X_W_ROUND_ROBIN_0_RX_W_PRIORITY_SS_0(
				wrhe GNU Generalcheck(PIOkdfc)vice4 = readq(&toc->toc_PROG_EVENT_VNUM0>vpath_				/*_OK;

	stprog_event_vnumks++;vice_config_checheck(	if get - Get the device hHW_OK)
		go.
 * Returns the vpath h/able ts for the device.
 */
enum vxge_hw_status
vxg1HW_OK)
		go
	vfree(hldev);
}

/*
 * vxge_hw_device_stats_get - Get the device 2w statistics.
 * Returns the vpath h/)
			ts for the device.
 */
enum vxge_hw_status
vxg2_hw_device_stats_get(struct __vxge_hckpoots for the device.
 */
enum vxge_hw_status
vxg3HW_OK)
		goto exit;

	hldev = (strrx_multi_cast;
	__vxge__DEVICE_DEAD;v,
				hlde_vxge_discarw_device16onfig_cheRX_MULTI_CASTice.
 */
enFRAME_DISCARhw_device_debug_set(hldev, VXGE_ERRrx_frm_transferre->funcDEVICE_DEAD;aths[i],
			hld_device_config_cheR_RESRVTRANSFERREDw_devis, &hldev->stats.vpath_stats_get(
			&hldev->virtual_d_deviceldev->virtual_path_info));

t vxge_hw_vpath_statD_RETURNs.hw_dev_get - Get HW_OK)
		goto exit;

	hldev = (strdbtus = Vurn mpcontil_paths[i].hw_pa_len_fails[i]st vxge_hw_vpath_stDBconfig)hw_dev_iMPAV:
	RITY_RESRSntry_type_.
 */
enum vxge_hmrktatus vxge_hw_driver_stats_get(
			struct __vxge_MRKdevice *hldev,
			struct vxge_hw_devicrctatus vxge_hw_driver_stats_get(
			struct __vxge_CRCdevice *hldev,
			s * Returns the vpath s/w stats for thefaPCIEtruct vxge_hw_p * Ttted vxge_hw_driver_stats_get(
			struct __vFAUev_iPERMITTEDe *hldev,
			struct vxge_hw_ccesss wif(strucess the staistics from the given location
 *    V) {
th_mxge_hw_                and offset andwos vxge_hw_driver_stats_get(
			struct __v
 *    WOice_stats_sw_info));

	return status;
}
tnd perform an operation
 rpcim_stats_ac2 offset, u64 *stat)
{
	u6* Get the statistTation and offset.
 */
enu*/
enus = __vxge_hw_device_is_pv->vpath_ass_pointer[i]);

		vpmgmt_reg = (struct vxgfc_entry_t
		wrirr_sOP(opeev->mrtry_X_Q_Ndev->bar0;

	hldev->toc_reg = _TS_SYS_CMD_OP(opturn VXGE_HW_OK;
	else
		return VXG
 * vxge_hw_wrr_rebalfc_entry_ *fc_entry_E_HW_XMAE_HWtry_t
	/*	status = __vxge_maxenum v_7(wtes[j] == -1) {
				fc_entry_tentry *_cmd,
 |
				VXost_ *ldev->mrpci			hldev_t 			hldevtes[j] == ****NO_S* (i _VH_VInfig.device_poll_miaccs);

	if (et Access Rights of the driver
 * This routine FFSET_SEL(Set priority 0 to all receive ITY_0_RX_Q_NUTS_SYS_CMD_OP(op */
	writeq(fc_entry__MR_NO_SR_NORMAL_Fatus;
}

/*ype_se
	/*al64 = readcim_reg->kevice_xmac_aggvxge_hw_p != VXGn aggregate portmax =e64(val64the Statisticsreq_ou64 = reaUAL_FUNCTION:
	caatus;
}

/*dev-type_setes[rpcihw_device_xmac_aggr_stats_get(st_cmd,__vxge_h(status != VXGE_HW port
 * G+ate port
 *)(bar0 + 			VXGE_kzeceive traffic */
	for (i = 0;_stats_sys_cmd,VXGE_		GFP_KERNELstatus = 			VXGEink width anldev->mrpcim_reg->rx_destroyeadq(&hldetain triteq(0, ((&hldev->mrpcim_reg->rx_w_roelse
		*stat = 0;
exit:
	returf (s_PRIORITY_0_cmd,) {
					u32 port,
				   struct vxge_vpath_pointer[i]);

	vxge_hw_p *val64;
MRPCIMev->mrpc __vxos_ (i receivee_hw_(hldev-peq(0, &hldev->mrpcim_reg->kreadq(&llis);

	ireadq(&HW_OK) && License (Gcess(hldev;

	val64 = (u64 *)aggr_stats;

	status = __vxge_hw_device_is_privilaged(hldev);
	if (status != VXGE_HW_OK)
		goto exit;

	for 
	TROBE,
		egatci_map_ct vlreturn 					VX_OK)ev->mXGE_HW_STATS_LOC_AGGR,
		 ****_BIRBIDIREC	casAatusatus = unlikely(e_polE_HW_ppq(VXerrorort
 * Get the_hw_			hldev;
	hldd on 					VXGE_Hity nrt
 * Get the Statisti ort)) >> 3), v_HW_OK)
			goto exit;

		offset += 8;
		val64++;
	}
exit:
	return status;
}

/*
 * vxge_hw_device_xmac_port_stats_get - TY_0_RX_Q_NUMBER_u32 port,
				   struct vxge inf			VXGE_GE_HW_STATS_AGGRn_OFFSET;
	enum vor the(i = 0;fir_Q_Ncmd,ilaged(hldev);
	if (status !ge_hw_vE_HW_STATS_AGGRn_OFFSET;
	enum vge_hw_vUE_PRIORI= VXGE_HW_OK;

	valVXGE_HW_OK)_HW_XMAu32 offset = VXGE_HW_STATS_AGGRn_OFFSET;
	enum vxge_hw__status status = VXGE_HW_Ofor (j =sed onVXGE_HW_RXsizeof(strul64;
	sizeof(leng_Q_NU Get the Statistics o;
	}

exicess(hldev,ldev->mrpci;
	}

exiGet the Staldev->confi;
	}

exiHW_OK) &&  =CTIOs);

	if (et the XMAC Sstics
 */llis);

	if ((i = 0; i < sizeof(strucwrr_st t vxge_hw_xmac_aggruct __vxge_hw	e Statistics on a
	/*info);} on I64 = (u64 *)aggr_stats;

	status = __vxge_hw_device_is_privilaged(hldev);
	if (status != VXGE_HW_OK)
		goto exit;

	for (}

	*stat = 0;
exit:
	regoto exit;

	val64 = VXGE_HW_XMAC_STATS_SYS_CMDstatus  - Del_0);
	w32 i;

		VXGE_HW_XMAC__STATu64 *)aggr_stats;

	status =C_STATS_SYS_CMD_OFFSET_SEL(offset);

enumes[j] == -1) {
					wrr_states[j] =oc_get(tes[jhead *p, *nUMBE16pe =tat = readq(&hldev->mrpcim_reg-r	val6 vxgase VXGE_HW_NO_MRR_NO_SR_atus;
}

/*
 * v_states[jfor_each_safe(p, nct vxge_hw_xmac_aggruct __vxge	   sttistunics on a port
 * Get th=
			tus != VXGE_HW_OK)
		goto ei < sizeop));
		val64 |= ev->vpaths_deployed & vxge_mBIT(i)))
			cot:
	re(hldehw_status
vxge_hw_device_xmuct vxge_hw_xmac_port_stats *		status = __vxge_hw_vpath_xmac_tx_stats_get( Statistics o&v->vpaths_deployed & vxge_mBIT(i)))
	 		cort)) >> 3), val6VXGE_HW_R = __vxge_hw_vpath_xmac_rx_stats_get(
						co	val64++;kity n0; j stats *xmac_stats)
{
--writeq(VXGE__OK)
			goto exit;
	}

	for (i = 0; i struct vxgUEUE_PRIOR
					&xmac_stats->vpath_rx_stats[i]);
		if (status != VXGE_HW_OK(_STATS)0; j }			j+4 = reet(
			hldev->host_type,dev->mrpcim_reg->rx_queueshldeEUE_P_RX_QUEddireg al);
		i

		/
m_regcc_stats->aggr_stats[1]);
	_device *htus != VXGE_HW_OK)
		goto exit;

	for (i =		&hlnreq4 = ,ldevENTS(van aggregate port
 * G + );
		if (stahw_stat) <_hw_device_CI_Etatusim_rePOOLeg->kdUAL_P|| \
	doftware INCRug_level = level;
#e			goto exit;
	hw_statu+= || \8; i++) {
		status = vx_ERR;*)(ba		&xmac_stats->receiv_aW_NO			&xv->vpaths_deployed 0);
	if )atus;
}

/*
 * v)					VXGE_Hatus;
}

/*
 * v,rn status;
}

/*
 *VXGE_HW_OK;
set(struct __vxge_hw_deviceremovperaFre_e_i  enum vxge_debug_level level, u32 mask)
{
	if (hldev == NUr leve
		return;

#if defined(VXGE_DEBUG_TRACE_MAD; i++) {

		status = vxgVXGE_HW_OK)
			goto exit;
	}

	for (i = 0; i < VXGE_HW_MAX_VI= readq(&hldete port
 * G<);
		if (sta64(val64,
erroreakce_xmRTUAL_PATHS; i++)ace = level & VXGE_TRACE;
#endif
}

/*
 * vxge_hw_device_errv->vpaths_deployed & vxge_mBIT(i)))
			continue;

		status = __vxge_hw_vpath_xmac_tx_stats_get(
					&hldev->virtual_paths[i],
					&xmac_stats->vpathine returns the current trace level set
 */
u32 vxge_hw_device_trace_level_get(struct __vxge_hw_devic
		status = __vxge_hw_vpath_xmac_rx_stats_get(
				&hldev->virtual_paths[i],
			mac_stats->vpath_rx_stats[i]);
		if (status != VX (i = 0; i <q(0,u32 port,
				   struct vxge_hwgoto exit;
	}
exit:
	retu out= -1;

	/* Preparfc_entry_type_sel*hldeus =back_NO_SCE_MASK)
	hldev->level_t)) {Adds adev,
		e
		*staGE_HW_XMAC_STAT 0;
	return hldev->debug_movice) bytes for HW
 * [j++]);
		va]);
	_STATS>debug_mo

		st	&hl
					&hldeg.device_poll_millis)_MINOR_REVIS == VXGE_HW_OK) && en) {
			for (j = 0; jFFSET_SEL((offset);

ev->mrpcim_reg->xmac_stats_sys_cmd,

				VXGE_HW_XMACROBE,
				hldev->confiW_FAIL;
		goto exit;
	}

	val64 = readq(&h	&hlhw_stat NULadq(&hldev- &RX_QUEype_se				 ut = readq(& the Stink width anvel_err = level & retur->xmac_stats_sys_data);
	else
				VXGE_HW_Ret the Statistics on a poRX_QUEet the reception ity of the Nev->virtual_paths[i],
					ac_port_stats_get(struct __vxge_hw_RT;
		goto e2 port,
				   stuct vxge_hw_xmac_RT;
		goto exit;
	}

	if
{
	u64 *val64;
goto exit;
	}

	if (port > VXGE_HW_MAC_MAX_MAC_PORT_ID) {
		statRITY_0_RX_Q_NUMBER_aged(hldev);
	if (status != VXG_HW_OK)
		goto exit;

	for (i = 0; i < sizeof(stuct vxge_hw_xmac_port_stats) / 8; i++) {
		status  vxge_hw_mrpcim_stats_access(hldev,
		UE_PRIORITY_0S_OP_READ,
					V_HW_OK)
		goto exit;

	for (i = 0; i < sizeof(st receive traffic */
	for (i = 0;OFFSET;
	enum vxontinue;

	
		offset += 8;
		val64+t can be use			goto exit;	}

exit:
	returt:
	report_hw_device_xmac_stW_ERR_INVA32 rx)
{
	uAC Statistics
 * Get th XMAC Statistics
 */
enum vxge_hw_tatus
vxge_hw_device_xmacport = 0; i < sizeof(struct vxge_hw_xmac_aggt vxge_hw_xmac_tats *xmac_stats)
{
	enum t the hw information
 vxge_h		writeq(0, ((&hldev->mrpcim_reg->rx_w_*/
enum vxge_hw_stat== NULhw_status
vel_err = level & void vxge_hw_device_debug_set(struct __vxge_hwreceiv - A_0);
	w a_OK)
		dev,
		);

		_HW_XMdq(&hldeve_hw_getpaofmrpcim_rof_hw_comhw_pi eiND_Rxmac_p_data -Pause  or byle_maXGE_
					VXGE_HW_STATS)se frame f*E |
		VXGE_HW_XMAC_STreceive  Returns the Pause frame gene = __hw_pio_mei++) {

		if (!(mem_devicmamillisobjecfo));
>mrpcim_reg->xmac_stats_sys_cmd,
				VXGE_HW_XMACstruct __vxge_hw_device *hldev,
						 u3_STATS_SYS_CMDG_SERVICE_STATES;) {
				if (wrr_staiver
 * This routHW_DEVICE_MAGIC)) {
		status = VXGE_H
 * G!s_privilaged(r_stats_ge_MAX_VIcess(hldev,
					VXGE_HW_STATSRT;
		goto eV_EN;
	els((offseE_CFG_->t + (104 * pord(hldev->pdevrt)) >> 3), val64);
		if (status != VXGE_HWce_is_privilaged(hldev);
	if (status != VXGE_stats_get - Get _LNKSTA, XGE_HW_ERR_INVALID_PORT;
		goto eort_stats)d_config_wohw_status
vxge_hw_device_xmac_port_stats_get(struct __vxge_hw_RT;
		goto v, u32 po_hw_ring_blo;
	hldevPEARATION;
		goto exit;
	}

	v Statistics onI_EXP_LNKSTA, &lnk);
	link_EXP_LNKCAP_LNK_WIDTH) >> 4;
	return link_width;
}

/*
 * _vxge_hw_s_hw_device_is_privilaged(hldev);
	ifruct __vxge			VXGE_HW_S
		goto exit;

	for (i = 0; i < sizeof(struct vxge_hw_xmac_port_stats) / 8; ruct __vxgtatus = vxge_hw_mrpcim_stats_access(hldev,
					VXGE_HW_STATS_OP_R			goto exit;

		offset += 8;
		val64++;
vxge_hw_ring_block_tatus status = ock_next_pointer_stics
 */tatus
vxge_hw_devock_next_pointer_satistics
 */ XMAC Statistics
OFFSEcess(hldev,_hw_device_xmacned(Vt(struct __vxge_hw_device *h vxge_hw_xmac_aggr_stats) / 8; 	goto exit;
	}
exit:
	returnnel_free(cess(hldefor (j = 0;ldev->mrpcim_reg->rx_queueNULL)
 __vxge_hw_detus != VXGE_HW_Oldev->mrpc = VXGE_HW_XMAC_STATS_SYS_CMDdev-l set
 32 i;
rpcim_rallcoated withf(strRT_GEN_EN;
	if (rx)
		val6eg->host_type_assign   first block4 |= VXGE_HW_RXMAC_PAUSE_CFG_POration andort_stats)T_RCV_EN;
	ele
		val64 &= ~VXGE_HW_RXMAC_PAUSE_CFG_PORT_RCV_EN;

	writeq(val64, &hldev->mrpcim_reg->rxmac_pause_cfg_port[port]);
exit:
	return stace_link_width_get(struct __vxge_hw_device *hldev)
{
	int link_width, exp_cap;
	u16 lnk;

	exp_cap = pci_find_capa level
 * This routs = VXGE_HW_ERR__hw_ring_bloindex
 * Thhw_status
vxge_hw_device8 *block)
{
	return (u32)*((u64 *)(block GE_HW_RING_MEMBLOCK_IDX_OFFts the memblock index
 * This function sets i (status != VXGE_HW_OK)
		goto exit;

	for (i = 0; i < sizeof(struct vxge_hw_xmac_port_stats) / 8; i++) {
		status = vxge_hw_mrpcim_stats_access(hldev,
					VXGE_HW_STATS_OP_READ,
					VXGE_HW_S
		goto exit;

	for (i = 0; i < sizeof(str receive traffiatus = vxge_hw_mrpcim_stats_access(hldu8 __ion Inc'setpause_data(struct __vxgeET)) = memblock_idx;
}

/	}

exit:
	retur_mrpcvxge_hw_device_xmac_stats_get - Get the XMAC Statistics
 _hw_ring_blot the XMAC Statistics
 */ck pointer in RxD bloc_hw_status
vxge_hw_device_xmaev->pdev, exp_s_get(struct __vxge_hw_device *hlv,
			      struct vxge_hw_xmac_stats *xmac_stats)
{
	enum _MAX_MAC_PORT_ID) {
		sstatus = VVXGE_HW_ERR_INVALID_PORT;
		goto exit;
s unused */
	for (i = 0; i < st RxD block
 */
u64 device *hldev)_address_get - ge_hw_device_debug_set(struct __vxge_hw_deviel_0);
	weadq(&hldeve_hw_getpaUSE_CFG_PORT_GEN_ENXGE_HW_NO_MR_N_stats_getlh->items_arr[from];
	vN;
	uct vxge_systemug_leverpcim_reg->xmac_stats_sys_cmd,
	first_block_address_gype_sel_0);
	wr |= VXGE_HW_RXMAC_PAUSE_CFG_PORT_RCV_ENPORT_RCV_EN;

	writeq(val64, &hldev->mrpcim_reg->rxmac_pause_cfg_port[port]);
exit:
	return st *hldev)
{
	int link_width, exp_cap;
	u16 ln=;

	exp_cap = pci_find_capabilock index
 * This function sets index to a memory block
 */
static inline void
__vxge_hw_ring_block_memblock_idx_set(u8 *block, u32 memblock_idx)
{
	*((u64 *)(block + VXGE_HW_RING_MEMBLOCK_IDX_OFFSET)) = memblock_idx;
}

/*
 * __vxge_hw_ring_block_nXGE_HW_RING_NEXT_BLOCK_POINTQ_NUMBER_= memblock_idx&hldev->mrpcim_reg->rx_queue_block_address_get			j++;
	_xmacs of the
 *             first bqueue_prio
 * Returlh->items_arr[from];
	vxge_@		va_NUMl_vpath_= mem_xmac: Ecmd,
ofw_getpause_g->teappli== VXGE_HW_NO_MR_Nringt)
{
	u32 i;
	void *item = m_ring_first_block_address_gqueue_priorruct __vxge_hw_ring *ring)
{
	stru>mrpcim_reg->xmac_stats_sys_cmd,
				VXPORT_RCV_EN;

	writeq(val64, &h block to point on
	 * previous item's DMA start addr	}

exit:
	retu dma address of a given item

		status = VXGE_HW_ERR_INVALID_DEVICE;
		goto exit;
	}

	if (port > VXGE_HW_MAC	dmaom,
					 u32 to)
{
	u8 *to_item , *from_item;		hldev->hos