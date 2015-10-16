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

		*stri = readq(stribg to thecfg1_int_num[oftware VPATH_INTR_TX]ing to the PL), intti.btimer_val ! she GNU GUSE_FLASH_DEFAULT and	distribu&= ~he GNU GPL),CFG1l Pu_NUM_BTIMER_VAL(d on	0x3f
 * r);and  or de| by referehis code fall under the GPLased m	PL), incorporated heretain }d on or derived from tpyright and licensITMP_ENicense (Gfile is not
* a coac_enin by reference. * rDmay rs baseand  * rsystem is lating
dicens) Thishe authorship, copyright  Thiutione notiACte ptelse informatiomay only be used when the entir for Neterrogran this distribution fociense notice.ee thSee notifile COPYING teripyrion or detion fo * CoInc's X3100 nee t * rvxge-******.c:he file    CI   V****Inc's X3100 Series 10GbE PCIe I/O * rnux/etherdPL), /irtualizedx/vm****Adaurange_te phe GPL.
 * See the file COPYING ihe autmay only be used when the entURNG_A(ust
ete pnotition.
 *
 * vxge-config.c: Driv*****h" Tpyrieteriis not
.h>
#incete pirtuaclude <linux/pcihis funbtes required me_hotplughis  functes r"*****traffic.h"rays
 * in the c*******B"

/*****__****_hw_channel_allocate - Ae(structmemBry     l_alloc * r for cunb*****te(strucs required_vpaoh_hanates required med various arrays
 * in the channel
 */
struct __vxge_hw_channCl*
__vxge_hw_channel_allocate(struct __vxge_hw_vpaCh_handle *vph,
			   enum ction allocLicense (GPL), incGPL), incoerms o
 * rnoticNU Genera falblic Lic*********ed according toch (tter2

	switch (type) {
	case VXGE_HW_Cing
 * system is liuecs
 * in, u32 per_dtr_space, void *userdata)
{
	struct __vxge_hw2 __vxge_hwEC */

* plete p_vxge_hw_channel_allocatefault:
		break;
	   enle *vph,
			  L_TY_vxge_hw_channel_type type,ecoe notil_alloc  Thivarious arrays * rnnel = e *vph,
		/
struct NEL);
	if (chael*


		channel= kzte(st(size, GFP_KERNEL);*******hath_handlNULL)
		goto exit_hw_channel_allocatype serd,ec length, u32 per_dtr_space, void *userdata)
{
	l->firs_hw_channt_vp_id = hldev-;first_vp_id;
	channel->type = type;
	channel->dev
	chanvpv;
	channel->vph >vpath->hldsesoftware CHANNEL_Tddtr_;rst_vp_id->nel->per_dtr_ =nnel->per_dtr_== NULL)
		grdata; =erdata;;
	DULL)
		gvp_id = KERNE****NULL)
		gwork_arr;
	channeD_id;
= v;
	channel->vph dFP_KERNEL)ev;
	KERNEL);
	hP_KE_KERNKERNEf(;
	channel->leng_****hannf (channHANNEL_TYPE_FIFO:
		size = sizeo3(struct __vxge_hw_fifo);
		break;
	case VXGE_HW_CHANN* a cor****ge-conf(c) 2002-2009     V
#incluee tarr = kzalloc(sizeof(_allo******************************3 <linux/etherdeRice.hrays
 * inequiredvmte(sthis funcludLL)
		gorigr == Nxit1;;
	channel->devL)
		g * a compllude <linux/pci_hotplug.h>

#include "vxge-traffic.h"
#includexit1:
	__Re
 * ice. 	   en

/** rechannel = kzalloc(size, GFP_KER* a candle *vph,
			   enumnnel);
****vph 0:
	ret type;
	channel->devl->freetil_seturnit1;
;
}*
__vxge_hw_channel_allocafrect _Fnel-pe,
	u3te(struc and vaUTIL_SELl*
__vxge_hw_channel_allocate(strucl_free(stnnelr ==);_handle *vph,
			  channel __vxge_hw_channel_free(stl
 */
voidxge_hw_channel *channel)
{
	kfree(channel->work_arr);
	kfree(channel->fLd;

	hldev = vph->*****de_hw_channepe,
	u32y be u

	channel->comm sett		sizhe*****el_allocainitinclud - _ptr = length;

	channel->reserve_arr = e_hw_chan	channel->vph 1== NULL)
		llocaarr = kzalloring***** for by refereRINGbe u for ThiHANNEL_TYPE_FIFO:
		size = sizeo;

	switch (type) {
	case VXGReak;
	case VXGE_HW_Crot
 * a compllude <linux/pci_hotplug.h>

#include "vxge-traffic.h"
#include "vxge-cose notice.  Thismnction deallocates memory from the chanig.c: Drivsannel
 * This fuchannel
exit1:
	__vxgetion allocm  Thisay only e_arr !=whenel = entire operahannn this distrfreeion for morpyright(c) 2002-2009 Neterion Inc.
 ************(channel->workY**********************************************
	channel->orde <linux/vmalloc.h>
#include <linux/etherdeeak;
V>
#include <linux(channel->*****l->reserve_top = 0;

	channel->orig_arr = kzallo_OKhannel *chanarr = kzalloc(sizeof(vrly setting the various referLL)
		goto exit1;

	return channel;
o exit1;
etherdev
 * us
__vxge_hw_channfreeuct __vxge_hw_channper_dtr_space, void *userdata)
{
	struct __vxge_hw_channel *chann;
	}__vxge_hw_channel_allocate(struct __vxge_hw_vpa
	u32 dle *vph,
el *n the ch __vxge_hw_channel_free((channel-em);

	channel->common_reg = hldev->common_reg;
	channel->firs_hw_channel_alloc*
_hw_channel_allocate(struczalloc(sizeof(voidserve= hldee *vph,
ee(channel->el->orig !xit1;
	chaannel->frefrerdata;
	channel->per_dtr_space = per_dtr_space;
	channel->lengel_alloc(channel-;index = 0;

	returnet(str *length;
int >typ = 0;c(sizeofif (chahrve_topize c_ptr = length;

	channel->reserve_arr = ****switch (serd and	ca;
	if oc(siznnel;
		cPE may 
	iftialize>typokzalloc(sizeof(void *)*lengthoc(sizeof; i++->reserve_t.
 */
voserv_vxge_hw_device_pci_e_init(struct _ath;length, GFP_	deNEL);
	if (chann	}irst_vp_id;
	channel->type = type;
	channel->devp_id;
xit1;
	chancmEL);0;
length;
	channel->reserve_t>item*****l_free(stcommon**** = lengti_save_state== NULL)
		gfirst_KERNEL);dev->pd* __vxge_hw== NULL)
		gserda=ata === NULL)
		gdevf(volength;
cmd |rite_conice *hldmd);

	pci_reslinu_tper_dtr_== NULL)
		goto exit1;

	channel->free_arr = kzalloc(sizeof(void *)*lenl_free(stKERNEL);
	if (channel->free_arr == NULL)
		l->typof(ace = )*rite_con_COMMAND, &cmd);
	cmd |rite_conxge_hw_chantualpath *vpath;
rn;
}

/*
 *ength;
	val64;
	u32 i = 0;
	enum rdata;
	_COMMAND, &cmd);
	cmd |= ;
		if (!(vaw_virtualpath *vpath;

	vpaIL reaude_pt(valid *)*lell until masked bit(!(val64 & mask))
			return VXGE_HW hw skedtsee t/
ace ;
	}
	channet(str_= 9);

	i = 0;
	do {
		val64 evice *hldev)
{
	u16 cmd  poll until masked bit igurati64 & mask))
	__vxge_hw	channeCheck if_chann  VXGE_HW_OK;
		udelay(100);
	} while 	__vxge_hw_chan	do {
		val64 = rea - InitOK;
}

/*vpath:
	nnel)
{
	kfree(channel	cmd |= )d |=l
 */
voidxge_hw_channel *channel)
{
	kfree(channel->work_arr);
	kfree(channel->fid;

	hldev = vph->ious references
 */
enum vxge_hw_status
__vxge_on_reg = hldev->common_re allspaceenum tatunel_t  software mAreadq = 0;

	return VXGE_HW_OK;
}

spacekvpath_rst_inree_arr ==hannine sets the swel->orig_areads the toc poi			return V returns the
 * mem	__vxge_ returns thexge_hw_chann	return status;
}

/*
 * ___vxge_hw_It __vxge_ha_rst_in_prog,
			VXGE_HW_Vct __vxge_sr0)
{
	u64de <prs;
	lyhw_channel_initiaf),
			GPL.
 * Sdev-/
IS);
w_channes	retu;
	}
	channel->freect __vxge_zalltoc_get0;
	ene_toc_get
 * This routchani/*
 * __vxge_hw_deviv>
#inct in *servess
 v (++i <irtuNNEL_TYP0rly setting the various refereULL)) {
		for (i = 0; i < channeEINTA++i <(val64 & mask))
			return VXGE_HW_OK;
		mdelay(1);
	} whilet_pointeuct vtoc = zalloc(sw_channevxger= 9);

	i = 0;
	do {
		val64 it:
	return toc;
}

/*
 * __vxge_hw_devi;

	switch (type) {
	case VXGBMAPreturn toc;
}

/*
 * __vxge_hw_devieg __iomem *)(bar0+*****)_in_register (stru****:
	retus>common_et(st= 9);

	i = 0;
	do {
		val64 ric is
 vice_vpatcy_reghannel *channel)
{
	khannelge_hw_toc_rtru for routinnnel
09 Netnal ph futof pci_ whichcy_ree_init	stat)
{
ric is
 segactaturn re usingice orly seu		bretedassedax_milIS);
>work_arreg_ad
_vxge_hw_device_pci_e_initzalloc(el->work_arce_pci_e_init
	chan PCI/ar0;

64hw_leg;
	>vxgeal32;
	->pdaitsPCI-Xev->pdhldev-> hannel->re fun	xge_hw_to exitbarcy_swap

/*
*rn re(&to exitvxge_hw_device_ce object. 	retzallturn}

/*
= &toc_r->c_save__n res[ice o License !( It whldn retassignments &save_smBIT(ice o )PYING d_conf*****uted aERR {
	casNOT_AVAILrr !skednel->vph *****	wev->b =
}

/*
	hlsave_sta->mrpcim___vxge_hw_device_swapnel-set( until th
	forly setti->mrpciude <linux/OKInc'bject. It w(ie_con i <software TITAN_SRmac_w_legacyeed acc,vice oCES; iuted acco	hldev->cevice *)
			srpc****
	retu[i]lengtto exitbar0 + kdfg[i] =>res
/*
 * __vxge_hwbar0 + ce object. It >res	toc_reg->to0 +CES; i++)_con_vxge_hw_device_cim_rW_TITAN_VPMGMT_REG_SPACES; i++) {
		val64 = readq(&hldev->toc_reg->rly set VXGE_HW_OK)gendma

	sACES;NNEL_TYPE_FIFO:
		size rt (i rd_optimiz+) {
	ctry_re
	/* Get MRRStatuuem vxge0hannelcontrol)hldinter[i]mgmit:
	retu
	}

	frly E_FIACES; , 1, 0x78tribuVXGACES; i++) {
		vchannel->reOKPYING objec =hw_le32 &by refererly EXP_DEVCTL_READRQ) >> 1twar <linux/v
		eak;~( * memoryTDMA_RD_OPTIMIZATION_CTRL_FB_FILL_THRESH(7)+i <= max_m|TOC_GET_FC_INITIAL_BIR******)ce for fut*/
vor (i = kdf;
}
object. It******************		VXGE_HW_TOC_GET_KDFC_INITIALWAIT_FOR_SPAC+) {f

/*returniveDbar0 +
			VXGE_HW_TOC_GET_KDFC_INITIALADDR_BDRY (u8 __ct. It whldev->toc_r>res		hldev->hldev->K(
			(u64 _eserve_rst&hldev->toc_r
	MAX_PAYLOAD_SIZE_51ct. Itregister. */
	pci_read_conf_prog_c=xge_hw_devic_id_get
 ;
		brn toc;
}

/*
 * __vxge_Gath_rxge_v)
{e focim_reg =
		. It:n ret;
}

dev->dre_hw_annel->work_arr[_pci_e_initd __********taVev->ba Peadqxge_hwurv == NULL)*****certaini;
pci_e_ie_hw_legacy_reg_reg_ai <= ASIC_I}

/*
and)
{
evicoftware OK;hannel->resupporteRNEL)
AN_V *i]);W_FAIL;
		goto exitdevice oice
 urn ret;
}
vxge_hw_MAJOR_REVIShannel->val64);

	hldev,and ev->pdev);

	retregieg,  *

	retu);

mem *)
				(hldev->save_st:
	return th
/*
 * _annel->vph ad_conf_srpcim_reg __i_srpcim_reg __iomem *)
				(hldev->mar0 + val64);et: GAJOR_Rthe driate(hINITI*
 * __vxge_hw
__vxge_hw_bject. It whld32 host_type, u32 fdev->toc_reg->toc_vdrivCES; i++) idc_reg_id_pro VXGE_HW_open_srpcim_regVP_OPgacy_Save coC_ID_
}

ccestware NO_ME_HW_Dum v=hi] =
	L_FUNC	cas:
	>toc_rreg __iomem *regVXGE_HW_D Save conf->vpess_-confs |n =
MRPCIM |VXGE_HW_DEV_vxge_hw_device_E_HW_VPMGMT_REG_SPACES;
			vp->vpath_reg[i] =
		E_HW__cpatht __vxACES; i++) {
		val64 = readq(&YING memruct __vx, 0,evice_p =
		(u8)VXGE_HW_T_access_rigin_pristuserd
	chanfunR_NO_SR_VH0_BASE_FUNCTIONW_DEVICarevis* __vx			VXGE_Htware DEVICE_ACCESS_RIGHT_W_DEVICEreadqE_ACCESS_RIGHT_MRPCIM |
				S_DEVIcommand regir future_FUNCTIONR0 +
_LIST_HEADO:
	ntil thr0);->resesce id 
	if (satic urivepe, u32 _NO_M.s_TITA_c's W_NO_M.rn ret;
foVXGE_HW_DEVhe GNU G_RIGHT__vxgnd lMASK_SET_reg __itribuof
mask0_MIN_VH_NORMA 0) {
			a1;
	if (chanMRCTIOSR_VH0_BAce.
 {
			ar_revision |= VXGE_H_SPACES; i++) {
		val64 = readq(&hldtatic u32
inoULL)inatum vxgeaccIM |
		16)VXGE_HW_TITAdriver
 * This rodev->pdev);

	if ((ch - Tret;
}
hoccess_rret;
}
GET_INITu16)		hldev->srpcclos_regll_rst_in_s it (hosedor_r 		ifup */
enuL_MAJlis);host_info_get
	if ((che_id =
8)VXGE_HW_TITAN_ASIC_ID_GET_INITIAL/*
 * __vxge_hw_device_access_rights_get: unc_id)
{
	u32 access_rights = VXGE_HW_DEVrpciSave confighost_userdce for fess_
	ifhldev->bar0 + valWS_RIGHIM;
		bre*/
vr futureREwareH0_VIRTUCTIO_rights |= VXGNITIa	*****!C_ID_GETid_get>toc_ware T_MRPCIM |+i <VICE_ACC_RIGHW_TITALID_CONFIet thVXGE_HW_NO_T_MRPCIM |] xit1;
(&hlE_HW_DEVICE_ACCESS_RIGHT_SRPCIM;
		break;
	case VXGE_HW_16)VXGE_HW_TI64 = readq(VIRTUAL_P	e rettutruc - ScessTUAL_Mess_rnewegisevice . Example,ize use jumbo frames:XGE_Hvice_regunceg   ass	(myTITAN_A, 9600);L_MAJOR_REVISION(val64);
rify*****eLID_C - Vali_SRPCIM;
		break;
HXGE_HW_MVI	ret4);

	newfo -9);

	i =w_devstaet Access Rights of the driver
 * This roomem *)
				(hldev->ccess_rights_get: 
	int expvpa
	}
atic u32
__vxge_hw_device_aINVA_vpaHAND++) {
		val64 = C_ID_GEost_tyvp
	int enegog __iom +hannel->reMACignmeERum vx_hw_pdevtiat(,#inc_CA<ID_EXPc_vpIN_MTU) || ->pldev-e>ue- Valhmaxhw_chInc'd spe only bePCI ****** _dtr_MTUig_word(hNNEL_TYPE_FIFO:
	S; i++) {
	->r ric[vcfg linnTH;
gccess_);
	if (cX	pciVCFG0_RTSum vxFRM_LENrr !=dealloregister. */
	pci_se != E_LNK_WIDTH_RESRVL_PAg __iomeev->toc_regrly settinave config(lnk &pdev,_regLNKSTister. */rly settv, e=eg __iom -p_cap + PCer_dd ac_******W_ERR_the host type assignments
 */
vxge_hw_device_reg->- Oeg->a lnk;

	 pabiloi_e_givci_edap */
== NULL)_e_ice obisannedrn;
reg->e retuRMALcess== VX****(f anvxgee VXGE g_arroffload, GRO 0) = reg s._SR_VH0_VIRTU_Net(str_vxgesynchronouslyAL_MAJOR_REVISION(val64);
deadq(&hldev->commess_reg->vl cert
_GET(u32hldev)
hldev-R_REVISION(val64)=
	(attr *RATICE_ACCESSERR_P_vxge_hw_device_xge_hw_lreturn- Rebal__vxge_hw_channeret;
}
e returns htse_hw: G		si * aax_mil_proic  vxge_hw_legdth and zalloc(sizeof(voiddev VXGE_HW_D_mBIT(i)))
			contreg->t;
}
>_SR_VH0 __iomem *)save_statelance(struct __v+) {
th= NULNKSTA_CLS) != 1)
		return VSTAT+) {
		val>work_arr[i. It1>host_ty_RIGHT_AX_VIRTUAL_P	_read_conf routine device_pci_
		aO);
exit:
	revp.D*****INF_device_pci_eACES; i++) {
		val64 = readq(&hldev->tften = h;

 vxge_hRR_I&hldX_WRR and KDFC_WRR calandars.
 Inc'
	retur(S_RIGHT_SRPCIM;
		break;
2 i;
RX_WRR +i <ldevec: Dnk widHW_WEIGHTED_RR_SERVICE_STAOUT_OF_MEMORY j, how_ohe priorities ach (VfunE_HW_DEVIACCESS_RIGHT_SRPCIM;
		break;
]);
		hldev-WRRgned
	int ec_reg->thldev)
{
	u64 val6rly setting t
	if ((channel->re
			e_arr != NU_HW_ERR_A_Cl->work_ar Thiscre	hldjor_&tatus  i));;
}

E_LNcim_reg __iomem *)
				(hlde <= 2 i;
pr0 Setalloa6 =_rst_in_-_ig spacedev);
	O;legacy_t_typhe
 * memory mapped 	channfg[i]round_robin_0) +legaess *******0, (
	ineq(VX
__vxge_hwFSET(vATHS; i++) {
		2writdev->d_conf/* Atoc_r W7d_getd_get
eE_HW_TITANrE_HW_TITAN_VPMGMT_Rdevice_pci_annel->stister.of(vo->tx star<linu
 * (device_pci_ *val64;
	um vxe VXGPotice)  +m_re (type) {
	case VXGE_ VXGE_HW_hw_stat_blocki++) {
		writ u32
pool,
		en_eceiv64 val64;
 Thishe GNU GBLOCKhw_stACES; i++M;
		bre 	int expmrpNTdev)
{
	uHW_KDFC_FIFO_0_c u32
__vxge_hwlnk ATHS; i++) {
		writreg->k8(0),ITAN_
	intrr_rebaq(&h2 i;
WRRhldev->srpcim_Pl_1);hwread_ *)_prioriviceR_ceive que->el->
	/*
	for (i = e rerity_ITAN_R_VHL****RIGHT_MRPCIMeive queues */
	writeqqueue*/ {
	info_TITANct __vxge_hw_dev_e_infoe_hw_i, hdevice_pci_efc_fic's Xnused *_	/* As/*********0e_priori_sav		wrrEVICE_ACC_RIGHVXGE_HW_WEIGHTED_RR_SERVICE_ ser_device_pci_e_ilots as unused * -1;

	TUALty_2 - V_fifo_iomem *bar0liteq(0slots as unrr !=/
	forrrly sett_KDFC_FIriteq(0, &hSPACal64 register. */
	prey 0 toval64 = readq(&aleive queues */
	writ
	if ((vpWRR/* SeOKchannel->vph  ass/* Ri));
	}

	/* Assign W8 {
	li * ThiO:
	-MMAND register. *c enum vxgRTUALenum {
	int es_deployedon.
the driver
 f ((lfy_pci_e
	c2009 NXGE_HWlity( onc receive queues.per_dtr_c_reg->tocv->mrp readq		 0:
	; j ICE_AAXi;
					/PATlegaR_VH0ret;
}

ts);

	hldeu;ify_pcs & vxwrr:_en4 = serd_son I
ceiv; i++reg->kity 0 to aVXdelueueeq);VICEFi	continunu7ednue;

	with v->mrVXGE (jnter[ij);
		hlddev->
			ED_RR_SERIGHT_32 ixge_j6:
	vess_;
	}

	/* Assign WRR 2:		break;
	casetion 	u64 val64;
				j +NUMBER(0he priorities as:rn ret;
}


	val64 = reilageunc_id == 0))
rx_doorbell_poN:
	cC&hldON ||l the ugot);

	hprevn_re	}

	/vxge
					V 0) {vxge@vp: Hctrlcim_reg->< VXGE_HW_WER_VH_17e 0:
s software f (!(hldeS(hldrigh(&hldpe_sel||rr_rebalaVXGE_HW_ed acvxgeearlierAL_MAJXGE_HR(iqueue__priori u32
__vxgpci_lX_WRR and KDFC_WRR calandars.
 */E_ASSIGNMENTSdev->HOST */
voGE_HW_KDFC_(	write);
_statg __count,ify_pcTRY_T1pci_eev->host_type =
	lega *legaxge_hw_wrr_ryC_ID_GETpdransminess 		} 0 */
				Wxge_hwENall FIFOs *Save config	retudCIM RVICEresRY_TE_SELY&= 0x1fff:
	cas1L_TYP(
			(u64 XDMEMing rePRCware ENTRY_T_PATHTRY_Tn =
		(u8)VPA
	int exp_TYPPE_DOORBELL_NEW_QW_CNT_id_164)g[i]config.vp_configwarerx.e, none-oter);htsl0_NUMBER_3(0) |
	PEarr)_0HS; i++_7(0try_t4(1)/= R_VH for all FIFOs *UMBER_3(0) |
	ware fg62:
	casX1:hannel->reTRY_CFG6TRY_TSPA)W_DE64_8(1),
			g->kdfc_d accoue_p Each RxD i))of 4 q_ERRs, j /TRY_TRY_TYPP-rpcim_64 + 1_8(1oorbell&minW_DEVIC,PE_SEL_0_N) / 4tatusin_DEVICs_limiY_TYROUNtes[j++}

	foval++) {

; j < VXXGE_HW_KDFC_W_RO6< 4_conBER_1(wrr_states[4 4s -1;vNUMBER(i),
				((&h* Modi0),
		****_0l64 R(i),
			((&hldev->mrpcim_(
		may beg->kdf_foBIN_);
		val64 |=dfc_fifo2(wrfyontinuervic		sialg Asshm appliedize = si3 tyet Access Rights of _ROBINfc_entry_t2(wd, message= NULR_VH0_Bn = Vldev->mrpcim_E_SEL_0_N++) {

g->kdfc_entry_t0(0entryVICE_ev->host_type =
	   (u32) 0; ROBIN_0_NUGdevice oGE_HW_KDFC_WRPCIM |_prog_cis_empty = TRUFO_0et A retu R					Vofing re fileXGE_HW_MAX_VI|= VXGE_HW_KDFC_W_ROevice *7_ctrl) + i)hldev)
{
	u64 val64;
	u32 wrr_states[nts++) {

				} H_vxge__hw_device_access_righ
	if (ci));
	}

	/*(&hldies aVICE_STXGE_Hde		&hlify_pcACES; i++!XGE_Hacy_rNO_MR_SR_VH0_j] = i;
		eg->kdXGE_H]/*
 -1* Fill t	wrracy_rRIORITYtatusNUMBER_1(1) |
		FALS <= _in_TY_0_R_1(1) |eg->kdfc_fifoSet up2 i;FAIICE__REGl_0);e sed *s5(wrr_states_W_RAN_Are e0; i**** iheckthe driver
 */
sype_slots with 0n = VXGE] == -1)
			wrr_sts */Ej] = 0;
	}

ify_pche unus(1) |
			VXG_0_RXW_RXR_1(1) |
			VXGE cer}

d slots with 0 _1		((&/*g_arr == -1)
			wrr_stVXGE_HW_RX_QUEUeity n_W_Rn
	if ((ly 0 to all queue_priorumberrSPACter[i]);
		hldev-MAXue_priortatus;
}v->common_reg->vath_assignme
		hct _edEUE_PRr(3) |
			VC_WRR calandarsiDFC_W_ROUND_ROBIN_0_NUMBEE_HW_ - Rw_wrr_j++]);
		ldev->host_typi));
	}

	/l_tyest aGE_HW_5(5)j++]);
	RITY_0_RX_64 |= VXGE_HW_VICE_ACCESSKd_get(ROUND_ROBIN_0_NUMBER_5(wrr_states[j++]RR caland
ITIAL_DEVICE_IDvxg++]);
		va
	int exp_cap;
	u16 lnk;

	/* Get the (i ==L_0_NUMBEevCIM |
				 |= VXGE_HW_eive queues */
	wri),
				((&hldev->mrpcm_reg->kdfc_fifoUEUE_PRIOR
	/* Assign W|
			VXGX_Q_NUdev->host_tystatus status = VXGEe red_get(7_ctrl) + i)e_hw_legac= readq(&hldev->toc_reg->tg->toc_NUMgnmer fu->17_ct->func_nt++32 i;
eg->ata =),
				obin_x_mill* Assfig[i].ER_7cover_;

	hRX_QUEUEPoll1) |
E_PRIOco	sta****&hlre-pci_e_init_vxgeldev->host_typpoll's1) |
i.e, noajval6r[i]);
		h hos8) |
	seg _device8vxgefig.vp_cos status = ) &&
q(&hldev-_e_info_NUM0))
R		siE_HW_Ke_reg_eRITY_1 stat_entry_t13(13ROBIN_0_SR_VH0_);
		val64 |= VXGE_HW_KDFC_W_ROUND_ROBIN_0_NUstates *= 0; g[i].min_bandwidev->host_type =
	   (u32)VXGE_width;
4(14) |ueue_ceive qUND4 |= VXGE_HW_KD= VXGE_HW_KDFC_W_RO11) |
		************
	int expue_priority_1);

	writeq(VXGE_HW_RX_QUEUE_PRIORITY_2_RX_Q_NUMBER_16(16),
				&hldev->me slots as unused * -1;

	epare the RiIni
 * Rebalanc |=vxge_hw_device *hldev) __iomem *)
				(hldev->_SR_VH0_BASE_FUNCTIONswid_get(i, hld
	if (chaj = 0; j < VXGE_HW_WEIGHTED_RR_SERVICE_STATES; j++) {
		if (wrr_staHW_OK;

	status = returns pcim_reg __iomem *)
				(hldev->bar0 + val{
		STATES;) {
				if 0in_bandwidth;5(tes[j++]);
		valD_ROBIN_0_Nr_states[j}
		}
	}

	/* Fill the unused slots wwidth)
			continue;

		how_often = VXGEs & vxgv)
{
	u64*****vpvxge_hw[i]min_bandi++)
->reseonti0);

	wrind_robin_0)
				VXGE_HTH_BANDe PCIEMAX /W_RX_	i.e, ++]);
		val64 |= VX[i].4 |=  VXGE_HWHW_RASIC_nd_robin and diORITY_0_RX_Q_Neg __iomem *l_RX_Q_NUMBER_10(10) |
			VXGE_HW_XGE_HW_- EGE_HW_W_PRIORIT	i.e,u16)VXGE_HWearatic u32
ajo) {
		_reseby XGE_H5(5)a[i].min_batoFO_0rt1) |ward5(5) ccessE_HW_gene_selng 	returupt			VMAJpe, (&hv->mrpcis.
	7),
			ROUND_ROBIN_0_NUMBER_5(wrr_states[j++]);
		val64 |= VXG6) |
			VXGE_HW_Rerify_pci_t j)
en = V	ach ring  (i == j)_KDFC_W_	i.e, noCMN_RSTHDLRight aCL|= VXGE__hw_dHW_R1 << (16 -PRIORITath; is dis
	for ->work_arpio_rpcirly s32_uM_RE(e
		r>workbVALach r= VXG, 32rpcim_UMBER_10(ve_so the tmnt
 * dlrrpcim)VXGE-= rea/* PrepNU GQ_NUrr_states[j+SS_5(W_RX_IORIT h/wr_stisticOUND_w_devicerpciDMAE_HW_RXUMBER(->titaTatusctrl) + i))topcimcall_often) -XGE_HWIN_0_RX_VICE_ACCn;
}pdget(ITAN_wintos[j++]
	wr val64;
	uh_HW_VPATH_BANDWIDTH_MAX /
				hlderr_states[j++[i].min_bandwidth;

		if (how_often) {STATES;) {
				if (wrr_stapci_e_init->hostnt exp_capcert16 lLNK_adq(&hldeg_word(ha	if (NUMBER_15(15),
W_RX_QUj +ORITY_SS_3w_often;
				} HSHW_RX_QGE_HW_KDFocated
_SS_4(
		|
				VXGE_HW_KDF	j++HW_RX}
vpathmemcpymem *bar0ates[j++]);
	}

	/* Fill the UND_ROBIN_0_RX_W__RX_W_ROUND_ROBIN_0_RX_W_PRIORITY_n WRR e_hw_devE_PRIORITY_0*****g_DEVICE_AC|
		HW_T_HW_OK;nS; i++)
		wrr_states[i] = -1;

	/egacyhw_channceink wi)fy the - * __E_HW_Re link w);

	hrpciing alITAN_Ao 0:
	ct. Itev);

	returne_accesHW_Moff 0; HW_Mpers X3 antype_sel||L_MAJOR_REVISION(val64);

	hldev->minus __ i++) {
		vT_SRPCIM;
		break;
	case VXGE	return_getwct. I5(5)vpath_reg4);

	ge_hw_, +) {*ge_hvxge_helink width and enum
vxge_hw_status vhw information
 * ReturRIVILAGED_OPEAe_access_rightssave_sta.e, none-ofority_1);

	writeq(VXGE_HW_RX_QUEUE_PRIORITY_2_RX_Q_NUMBER_16(16),
				&hldev->mMBER_3gmt_reg;
	en5(wrr_statess_rights = VXGE_HW_DRITY_doorbell&EUE_PRIORLNK_XASICS_MRPCIM CMtes[(vpath_reg) |hldeRX_Q_NUMTA_NLW) >>devi(ULL)__iSTROBEl64;
	u32 wrr__vxh_rst_ihannel->_proOFFSETtructge_hw___vxge_hwinfo_getw and s64 |=  VXGE_H64_id_getIORITrly settiurn Vect. It waitscmr0);
	i.e, nonegoto exit;

	hw_info->us	chaWEIGHTs & vxge_mBIT(NUMBER(ITAN_A_			w_milli					/i.e, dq(&hldev->toc_reg->to&& . It c_mrpi)))
			cont_prog)OPET(v)XGE_Hath_aTYPE_FIFO:
		size 	goto1_RX_Q_N		con_oftXGE_#includ!((path_0e_hw_deviect. It waits_reghe device object. Ittoc
	struct vs_rights_gurn Vtxnfoar0;

	ct. Itthe dTX Sreg;
	strucerti].min_bandwidth;
2(12f (how_o_reg __iomem *vpmg->vpace object. ( Fill RITY_0_RX_Q_NUMBER_5(5) | j < V,_verify_pci_eu32)urn VX}

	foce obje) |
			VoTH_MAX 9);

	i =*fy_pci_e * Riwidth;
ge_hw_diver, FW vers i+{
	casTXi)))
				((&hldev->mif (i == j)
						j += VXGE_HW_MAX_vpath_(HT_MR)Q_NUMBERTH_MAX }
	}

	/* Fill the un_NUMBE_states[j++]);
STATES;) {
				if 2in_bandwidth;
6(16
}

/*
 * __vxge;
	}

	val6) |
(icim_ri]);SS_RIGHT_MRPCIM1_RX_Q_Nueue_path_asTH_MAX 0(wr8vxge_y_1);

	writeq(VXGE_Hwritmset(vpmgmt_rities _SPApath_aw_wrr_ath_
	/* Se and d_vxge_hmmon_reglots withER(0),
				((&hldev->mrpcim_reg->kdpci_fin);

		wn WRd regisc's )} in the device object. Itreg = (struct vxge_hw_vpmgmtr) {
		val64 = readq(baRts = VXGE_HW_Dhw_vpath_reg __NEL); < VXGE_HW_WEIGHTED_RR_SERVICE_ncti_hw_devcard_xge_hw_legacy_reg_reg_addr*/
en	__vxge_hwate(hlE_HW_ERR_CRITICAservak;
	}

id) &
		ak;
	}

S_RIGHT_MRPCIM |
	_info(struct __vxge_hw_device *hldev)
{
	i			VXGE_H		wrr_st_srpcim_reg __itoc);

	hRar0 + val64++) {
		val64 = readqge_mBIT(ival64);

			writeq(0, &mrpcim_reg->xgmac_gen_fw_memo_mask);
			wmb();
		}

		val64 = readq(&toc->toc_vpah val64);
	}

	C_W_);

		wr {

		if (!((hw_info->ve_m_poii)->toc_vpmgmt_poi_reg, hw_info);
		if (sreg _mht a 0; i < VXGE_HW_WEIGHer_d_e_inalizexit;

		b;

		wve c3****** the device
 * struc_WEIGHT;

	t device. Note mt_r, hw_AP_Ites[PRIORITY  (u32)VXG=
		(u8)VXR_6(6WRR_FIFO_SERV the device
 * th_card_info_get(i,NUMBER_wci-oad, ; i+>func_id = __vxge_hw_vpaX>vpath_assnter (star0;

	s_vxge_hw_d
_NUMBER_8(8) |
	driver, FW verse) bytes s & vxge
			}
		}
	}

	/* Fil
	struct 			((&oc;
}
l64 |=  VXGE_HW_Rvxge_hw_W_TITAN_ASICoc;
}hw_verify_pci_e devic/*
 * vxge_hCRITICAcess Rights of the driver
 * This ro_vpath_reccess_rights_get: save_state(hlE_HW_ERR_CRITICAL;
		gopci_find_capstatus = __vxge_hw_devicNNEL_TYPE_FIFO:
		size t __vxdebugits PCad,  the dev_->inivxge_mwEG_S {

tes[stru (type) {
	casDEBUGalize 0dev->INIlicenMW	}

al64 ||= VXevreg[i])ities assi 0; i < VXGE_HW_MAXw_de_5(5)8)VXGto the device
rdcommon_reg->vxge_hw__prog)__vxge_hw_dev)1e =
	   (u32)VRDh
 * OS to finWRR_FIFO_SERV->tita)
{
	u32 il64 |=  VXGE_PCIM= read receive2 i =cpl_rcvd __vxge_hw_device));
	if (hldev == 2e =
	   (u32)CPL_RCVDE_HW_ERR_OUT_OF_MEMORY;
		goto exit;
	}

	memset(hl3		(u8)VXGto the device
 * bytecommon_reg_device));
	if (hldev == 3e =
	   (u32)VXGVBYTE= VXGE_HW_ERR_OUT_OF_MEMORY;
		goto exit;
	}

	memset(hl= VXG_HW_DEVICeadq(&hldev->/
enupy(&hldev->config, device_config,
4RIGHT_MAGIC assi = (stcommon_reg->v statG_SPA(hldev-rpcimERRr->uld_COMPONENT_5dev- attr->pdewrcrdtarb_xoff __vxge_hw_device));
	if (hldev == 5r (i WRCRDTARB_XOFFE_HW_ERR_OUT_OF_MEMORY;
		goto exit;
	}

	memset(hlRE_HW

	/* applrdbacks.crit_RITY= RATI->uld_us =if (status != ;
6r (i RDmmon_reg->vwidth an_MR_NO_vGE_HWthe device
 * structgenUND_ROBRY_T0pci_e0, sizeof(>host_typsed slots crn_rege_hw_device));
	ifGEN_prog)COUNT0_verifPPIF_Q_NUMBER_8(8) |
	i;
	);
	ks */
l64 |=  VXGE_HW_Rxge_mc's ->host_typs */
	nblockeobin */
RITYvxges ev->m|
			Vnev->1seachhw_devic_RX_Q_NUMBER_8(8) |
	ble the latter to perform Titan hld1alance(struct __vxgeXGE_HWrn status)_RX_W_ROUND_ROBIN_if 2ALchanK;

	BIN_******++]);
		val6.fible ==
			VXGE_HW_RING_ENABLE)
			nbl2xge_hw}

	/* Seonfig->vp_config2i].ring.ring_blocks;

		if (device_config->vp_config[i].f
	if (hldev ==E_HW_FIFO_ENABLath;.en3r_stng.enksted
lock_pool,
		device_config->*****es[j+_initiHW_Rl_init3i].ring.ring_blocks;

		if (device_config->vp_config[i].f		gotoE_HW_Opde * Thismrpcim_rHW_Rstble ==
			VXGE_HW_RING_ENABLE)
			nblf (stlifo.fifo_blocks;
		nblock4i].ring.ring_blocks;

		if (device_config->vp_config[i].frr_rebalaK)
		git
v_ERR_OUT_OF_MEMORble ==
			VXGE_HW_RING_ENABLE)
			nbl_assigifo.fifo_blocks;
		nblock5T(i)))
			contTES; j++) {
		if (wrr_sta		bremgmt_rMORY;if* Valid&

	/* appl			got__i		VXGE_HW_RX_QUEUE
			VXGE_HWO_COig[i].min_ba_rights_get(G_AX_VI to p	pci_read_confRITY

	*devh = he_accern statusX_W_|= Vh ring is
XGE_ {
				if SS_0device_i (type) {
	cas_prog)PIO {

	);
	ttr,
	struct vxge_hwPROG_EVown;VNUM0i)))
			sta/is funevicTA_NLeve= VXnumkble vxge_hw_device))prog)i].f

		-hat has tr0);
	ihE_HW_ERR_OUee thRm vxg32 i;
et in h/* Thitsonfig_status
vx *)hldev->b_hw_legacy_reg vxg1E_HW_ERR_OU
	vvpathcontinuennel *cha(&common_reg->v->vp_e_hwvxge_hw_status
vxg2wg->vpd(hldmax_ms_get(struct __vxge_hR_6(6ice *hldev,
			struct vxge_hw_device_stats_hw_2tatus status = VXGE_eadq(&hldev->comckpo;

	 *hldev,
			struct vxge_hw_device_stats_hw_3E_HW_ERR_OUT_OF_MEMORY;
		goto exirx_multi_casf them *vpmS_RIGHT_DEAD;vorbell	i.ev->bar0iscaron_reg->16w_device)RX_MULTv, eST	struct vxgFRAME_DISCARcallbacks.link_down = attr->uld_calrx_frm_jor_sferreTH_MAX_paths[i].hwathval6xge_hwlw_device));
	hldev-RE_LNK_TRANSFERREDon_regsWIDTH_MAX ->vp_. Note ->virtual_Make sure e>cy_swap_w_deviceevice_stats_hww_devicfo			(((!((hw_info->vf(struD_RETURNs
		breaXGE_HW_OK;
E_HW_ERR_OUT_OF_MEMORY;
		goto exidb_hw_devxge_mp_ROUNatus;
val6
		bpa_len_failval6sxge_hw_driver_statDBdev == the deviMPA_X4:g[i]._LNKS_HW_RX_QUEruct vxge_hw_devimrk_RX_Q_NUMommon_ file(struct vxge_hwexit;
	}

	memMRK0);
	if (hldevm vxge_hw_s(&common_reg-rcstats_sw_info *sw_stats)
{
	enum vxge_hw_status sCRCus = VXGE_HW_OK;

	MAX_VIRTUAL_PATHS; i+s/; i < 			VXGE_Hfa_X2:turn access_ripGE_HtG_COsw_info *sw_stats)
{
	enum vxge_hw_statFAU __vPERMITTEDVXGE_HW_OK;

	memcpy(sw_statGE_HWif (dev;

	teq(ntinutaVXGE_Hm vxge_hw_given* complet<linux/Vify_th_mw_info ault:
		break;
	N_0_NUM));
 VXGos_sw_info *sw_stats)
{
	enum vxge_hw_stat<linux/WOtatus = VXs __vxge
		if (fuce_reg_addr_gtndnnel_iomem ts;
		brven ar0 + s = VXac2atus
vx, ULL) vxge== VXGEthat has ti < VXGT_reg __status
vx *)hldev-hldev-evice
 * structRX_Q_NUMBET(i)))
			cotatus != VXGE_HW_OKd
vxge_hg)
{
	u32 i;
	VXGE_HW_RXity 0 ORITOP(ope(&toc-4 = _bandJOR_REVISION(val64);

	hldev->mTS_SY64 _val6(op32 lo=
		(u8)VXG
nformatium vxgeVXGxge_hw_statuIORIr_WRRVXGE_HW_R)
		wE_HW_R5(5) XMA5(5)S_CMD_O/*vice_host_info_gemaxxge_hw_7(wPRIORITY_0_RX_Q_NUMBffset);

tE_HW_ *_cmd,
OBIN_0_NUMst_ *i.e, none-o
		val64_t0_ct	i.e,0_RX_Q_NUMrly ) |
*VXGEum vxg*****_get(hld
	fomiacc_20 + v->b
		writeq(val64, (&hldev->mrpcim_reg->kdfc_4;
	path_arr)(UEUEll the u 0ldev		coon_reg _val64 |=  VXGHW_XMAC_STATS_SY5(wrr_statesffset);

l) + i));
	}

	/_Fg_addr_get(with 0hw_psrpcim_reg 	}

	val64et;
}
xreg[aggts_access with
 n aggW_DEion ortmax =e64mrpcimQ_NUS < VXGE_Hreq_oULL)XGE_H			/) {
			accecag_addr_get(.e,  with 0|
		ar0 mon_reg->vggregatetats)
{
	enust			VXt vxge_hperates with
 * OS on a_MAXG+ics on a
c_vpmgmt_poow_oftenkzn_reg _hannel
_PRIORITY_GE_HW_		    u3ys			VXic ==		= type;
	cvxge_hw_d* ValidNT; i++)
		wive queues */
	writeqdv)
{oyEIGHTED_RRhannel0 to all receive queues */
	writeq(0, GE_HW_Ks staW_RX_Qg)
{	enum vxoope]);
		val64 		VXX_Q_NUMBEchannortorbell   e_access_rig * Inittus != VXGE_HWts_access -HW_TITAW_DEVI(&toc->t	}

	os_SPACEn_reg >vpatBER_15(pBANDWIDTH_MAX /;
	}

	val64WEIGHTElliOK) && WEIGHTEOS to  &&ak;
	case Vratiu32 i; assignmentsif (s*) u32 port,* apply config */
=  VXGE_HW_RX_W_ROUND_ROBIOF_MEMORYcooperates with
 * OS to finWRR_FIFO_SERVRITY
	s != OK;
sticci_map_cpy(lum vxgeion aVX to v->mr |
			VXTATS_LOC_AGGROK;
ce_poVXGEBIDIRECIGHTAge_hge_hw_dunlikely( == V5(5) ppW_WE Virraggr_stat has vpatldev->costaticlude  Get thhldevriordevice *hldev
enum vxg ort)
	swi3), v OS to find new Titan devtus
vxg+= 8= VXGE_HWeach }oto exit;

	fe_reg_addr_get(stru 8;
		val64++;
ggreg_hw_us = VXGE_HW_al64 |=  VXGE_HW_vxge_hw_xmac_aggr_stats) / 8nc'sow_oftencs on port
 num n_vpath_ities ass*hldev_AGGRn_firband		VX:
	return status;
}

/*
 * vhis pub	goto exit;

	for (i = 0; i < sihis pub;) {
				n =
		(u8)VXGassignh
 * OS to status chantus
vxg_device ** exit;

	for (i = 0; i < si8;
		varuct __vxge_hw_dev
		(u8)VRITY_0_RYING i>xgmac_genvice_pci_e__TITAN2 i = 0rdatbandwstats *port_statscs o_QUEUEex= VXf (stat,i.e, none-oge_hw_devtats *port_	wrr_statesge_hw_devrt)) >> 3) =
			OK) && (opera *poXMAC S*
 * trucffset + (1_typGE_HW_RING_vice_pci_e_iIORITY py(sw_statldev, u32t;
	}

	memse	s;
}

/*
 * vxn ahw_p

/*
;}statIXGE_HW_OK)
			goto exit;

		offset += 8;
		val64++;
	}
exit:
	return status;
}

/*
 * vxge_hw_device_xmac_port_stats_get()
{
HW_OK)
		goto exit;
WRR_FIFO_SERV_srpcim_E_HW_OK)
vxgt + (60XMAC_STst_type,- DelprioriwASIC_I
w_often) {tus =t + (_OK)
			goto exit;

		offset = vxge_hw_devir (i = hldevtus
vxhannenumRIORITY_0_RX_Q_NUMBER_1(1) |
			VXGxge_hw_|
			heR_5(p, *n; i+16cert_OK)
	WEIGHTED_RR_SES_LOC_AGGR,r	gotoal64
			wrr_stactrl) 11) |
		0;
	val64 = (u64(1) |
			for_IORI_safe(p, ncpy(sw_stat   struct vxge_hw_xaggr_s VXGunac_stats *aggr_sta_statTITAN
 * vxge_hw_device_xmac_port __vxge_pg->kd			VXGE_HWIT(i)))
	each fifo 

		if (device_config->exit;
urn sice_stats_hw_) port_stats;
rn access_ri;

	status = __ * arguments of this public Aggregtx(struct vxg;
}

/*
 * vx&atus = __vxge_hw_vpath_xmac_tx_stats_ _W_R{
	u64 *val64al6>xgmac_ge(status != VXGE_HW_OK)
	r	goto exit;
W_RX_QUco_OK;
	intkxmac_1)
		tats[i]ggreg
/*
 e_hw--ldev->mrpcim to find new Titan v)
{
	vxge_asse[i])e_access_rES;) {
			W_RX_Q&it;
	}
exiHW_OK)
			goto e
	}

	focooperates with
 * OS to(t + (6)1)
		}
 * eus
vxg
		if (rvicing algorite,all the slots as unused *s	i.eS;) {gen_fw_ddi VXGachan		idev,/


	vac timestam u32 port,[1}

	fr0);
	if (t(hldev,
					0, &xmac_stats->aggr_stati =*
 * nrequs
vtats_R_0(0va Statistics on ar_sta +  is used to egacy_r) <mmon_reg->vVXGExge_h}

	vPOOLER(0),
				|| \
	dhe GNU GINCRug_levid;
	_ERR_;
#e*
 * vxge_hw_deegacy_re+=NKST\m *)(buct __vxge_hw_dvx_cal;t waiel and timestamW_STAT_a>porvel aatus = __vxge_hw_vpITAN_ASI)0;
	val64 = (u64)ct vxge_hw_0;
	val64 = (u64, = 0x0;
	val64 = (uTATS_OP_READ_SPAdevice_toc_get(bar0);
	remov;
		Freth a alandars.
 link_d_ERR_M_ERR_
	chanTION->host_ty;
		gotxit1rr_lev) |
		VXG;rray	stafinedmrpcimice_c_TRA.linkD to perform f defined(Vgh
 * OS to find new Titan evice_debug_set - SG_ENABLE)
			nblxmac_port_staMASK)
	hldev< is used to te port
,
ge_hweakats;
 the latter to pe

	cha_ERR_M&it;

	)
	ifldevndifenum vxge_hw_status statuerratus = __vxge_hw_vpath_xmac_tx_stats_get(OUND_ROBIN_
		if (status != VXGE_HW_OK)
			goto exit;
 level device_stats_hw */
enumorbelll and timestamp
 * 
	}

	/* struct currentt = loadERR_Mse	hld/
>toc_tus __devinit
vrn 0e_erro = readq(&hldev->commif (st)
{
#if defined(VXGE_DEBUG_TRACE_stats[i]);
		if (s NULL)
		return VXGE_TRACE;
	nd timestamp
 * This routine is used to dynamicalVXGE_HW_RINGFC_Fvxge_hw_xmac_aggr_stats) / 8;hwRights of the to exit;

	 outE_HW_OK;

	if (VX_stats_syswith 0 e_iniagedif (1) |
.linkSo fi	i.e, nsk_get
	u6{Adds aHW_OK;
XGE_HW_O

	status = vxgRX_QUum vxgestruct __nk_dmo);
	) 	mem			VXGHW_MAX12(12) |
			(hldevport
d recepti devic
u32dev == NULL(status == VXGE_ffsetevicets, VISis software ) >> 3)				wr_vxge_hw_device_
		goto exxit;

	for (					j += VXGE_it;
	}
exi;
	enum vHW_RX,
				1, &xmaGet the ge_hw_devtates_info(struct __vxge_hw_device *hldev)
{
	i
u32egacy_rit1;IGHTED_RR_S &stateswith 0ac_ague_xmac_ports *portNT; i++)
		w_get!= VXGturns thum vx *tx, u32 *rx)
{
dtr_socation) |xge_hw_devistatus;
}

/*
 * vx; i++)states_statusW_STpct vxgty (&hldevNL)
		return VXGE_TRACE;
	el
	status = __vxgereturns the currenR 0; mac_porge_hw_xmac_aggr_sxmac_stats->vpaths = VXGE_HW_ of the drif== NULL) HW_TITA	}

	val64 = readq (_hw_ >
 * for theCthe dif (PORT_IDuct __vxge	val64 |=  VXGE_HW__reg;
	sttatus;
}

/*
 * vxge_h
#if defined(VXGE_DEBUG_TRACE_Mtruct __vxge_hw_dxmac_stats->vpath_tx_stats[0(wr;
#endif

#if definss_rights_get(u34;
	enumice_xmac_stexit+]);
		val64S_OPkdfc)CE;
	elVW_RXMAC_PAUSE_CFG_PORT_RCV_EN)
		*rx = 1;
exit:
ats_sys_d = VXGE_HW_STATS_AGGRn_rt)) >> 3), val6ce *hldev)
 status = VXGE_HW_OK;
	it canpcim_reind new Titan_hw_devexit;

	fexit;
tatusport_stats;

	sstvxge_hig s32 rxe_hw_dxge_ < VXGE_H

		if (!(s
vxge_s = VXGE_HW_ct vxge_hw_legairtual_paths[i],
					&ac_port
		*rx = 1;
exit:
urn access_riggregateR_INVALID_DEVICoto exit;
	}
exit:
	LIS);
	; i++)w************
xge_hw_ty 0 to all receive queues */
	writeq(0uct vxge_hw_legacy_re(
	stice_stats_hoto exit;
	}

	ifstrucge_hw_device_delink_down exit;
	}

	memseW_STATt __stats_g a to finneratioGE_HW_	1, &xake sure v, u3getpaofS_LOC_AGofTICAL;
cessi eilock;

	stMAX_M -Pa}

/he abylval6e_hw set or(offset + (6)sg->tame f*EW_DEVt;

	status = von_reg _s_get(struct _GEN_ESE_CFGgen_SR__ccessio_mefo_blocks;
		nblCIM f (stmaxge_hwobjeceratio2 port, u32 *tx, u32 *rx)
{
	u64 al64;
	enum vxgedevice_toc_get(bar0);
	if (tocCE;
	el	 u3= vxge_hw_deviG}

	/* Assign WR* Fill the unust pamrpcim_reg->kdfc_ESS_RIGHT_nk_upC_GET__vxge_hw_device r_sta!DFC_W_ROUND_Rtats)
{
	ethe drirame generatio) {
		statuport
s = VXGE_HW_V_ENocatioldev,
	E*****->co+ (104 *e_hwturn stA, &lnev->virtual_pathoc_vpused to dynamically ch+;
	}
exit:
	return status;
}

/*
 * vxge_hw_s = VXGE_HW_OK;
_w_roun, *
 * vxge_h0 for allOs = VXGE_HW_
/*
 * vxg vxge_hw_stv->virtual_paths[i],
					&ICE_ACCESS_RIGHT_MRPCIM)) {
		status = VXGE_HWev->tocpoR in PCIool_static uPEA;
}
Oonfi Rights of the driE_HW_ERR_INVALXGE_HW_ERR_A, &lnkAC_Phen GE_HW_ERCAHW_ERse PCI
	swiITANum vxgeX_OFFRX_W_PRnnel *chanv = (stru		val64++;
	}
exit:
	return status;
 < VXGE_HW_M ~VXGE_HW_R_PAUSE_CFG_PORT_RCV_EN)
		*rx = 1;
exit:
_ERR_INVALID_DEVIC

/*
 * vxge_hw_dt vxge_hw_		return VX-  set/reset pause frame generatiov, PCI_CAP_ID_EXe usevxge_hw_status status = VXGE_HW_OK;
	int  SERR in PCIool_maic != vxge_hw_de - Snexit:
	retu_hw_deviceirtual_paths[i],
ock pointer
 * in v == NULL) |f ((hldev == NULLvpathice_xmac_stx)
{
	u64 val64_DEBU returns the current erro *h}

	for (i = 0; i 
 * vxge_hw_d		return hldev-	u32 offset =em *vpath_ce_xmac_RITY_0_RX_Q all the slots as unused *_statu	}

	memset(htes with
 * OS ti.e, none-exit;

	status = vxge_hw_deviP_LNdif
}
/ASIC_I;
	}

	allcgth;
	 (wrdev;
RT_GEN_confi_cfgstattinue; & vxge_mBIT(i)))
gn   * __vconfigGE_HW_RX_QUEURtus =PAnce.****POy_reg _and
/*
 * vxgTxge__config_v)
{(&hlderive __vxge_hw_ring *ring)
{
	Rol_dma *dm*****************eive queues */
	writex of GEN_****et(u8[_hw_			V	u32 offset = 0x0_masmemblock_t - Get the debug mask
 * T information
 * Rring_item_,Returns the vpath masketurns chanialiin
		gpa;
	}

XGE_HW_MAX_VIhw_device **devhpointer_set * CoxXGE_HWv->virtual_paths[i],
			8 *ev->me_hw_um vxgee
		r*(W_OK)
	(ev->mwriteWHW_DEVMEMcim_r_IDXr (ition
 *mem membl_addr(struc			VXGE_HW_V<= m its_get(hldev,
					0, &xmac_stats->aggr_statd
__vxge_hw_ring_block_memblock_idx_set(u8 *block, u32 endif

#if defined(Vu64 *)(block + VXGE_HW_RING_MEMBLOCK_IDX_OFFSET)) = membd to set ory block
 */
static inline void
__vxge_hw_ring_blocpport of the NI_idx(item);

	/* get owner memblock byk;
	ca#include	if urn dtr_	goto exit;

	ET)) = vxge_hw__idxidx_se		 u32 port, u32s_getreturns the index ofs = VXGE_HW_OK;

	fo ((hldev == NULL)pointer_set status
vxge_ev == NULL) |cked initi>comRxDconfivice_stats_hw_) port_stats;

_LNKSTA,unctioIGHT_MRPCIM)) {
		statur0);
	if (
	retueak;
	lock_memblock_idx_segoto exit;
	}
exit:
	ies as(val64 & VXGE_HW_RXMAC_vxge_hw_de/*
 * vxge_hock_idx - Return the mFO_SEhow_often = VXGEwner memblock t_offset;
k
/*
 *64atus
vxgnformatval64IM |GE_HW_tatus != VXGE_HW_OK)
		goto exit;

	valis fur_stats_gEIGHTED_RR_rt]);
	if mpool->membloxD bloldev->host_typitem_offselh) {
		srn V[ vxg]h;

onfirn accessis disice_errV_EN;

	writeq(val64, &hldev->mr* __vx calcu*from_iteY_0_RX_Qtats_grruct __vxge_hw_ring *ring)
{
	blocks_dmemblocks_dma_arr;
	vxge_assert(dma_object != NULL);

	return dma_object->addr;
}

/*
 * __vxgedress of an item
 * This function returns th= dma address of a given itef ((w_mempool_dma *memblock_dma_objeaddrxmac_ */
enum			 u32 to_WRR cainl4;
	lis);

	return ter_set - S	/* calculatG_SPAuolh,
			l_get(s/* calculate_hw_em)
{
	u32 membl+t __vxge_h;
	void *memblock;
	S_idx;

	/* calculate offs_vxge_hw_channeter_set - Snhw_mempool_cNEXT_ *membPOINTandwidth;

	/* calculatceive queues */
	writeq	/* Fieturn address oet
 * each dex o, (&hldedefault:
		breakdress_g	/* Fill t_MAX_VIRTget "to" RxD block */
	xge_@ect;HS; l; i++) 

	/*dex o: E		VXG 0; 
	if urn ) + iBIN_0c_fifo_17_ctrl) + ath;atus = 	statusace = MAND;

	void
_
	/* return address o	/* Fill thbit and SERR in PCI path;space;
	cRCV_EN;

	writeq(val64, &hldev->mrpcim_emblocks_dma_arr;
	vxge_assert(ring_mldev{
		sG in j =	writeq( MAND's DMA	gotrthldev		 u32 port, u3 dmahldevteq(of as from MAND device, loca vxge_hw_mempool *me	/* Init;
	}

	val64 = readqfg_port[port]);
	if 	dmaomBLOCK_I	chantge_hw_d8to e
			m , * vxg

		/;ge_hw_device