/*
* Copyright (C) 2008-2009 QUALCOMM Incorporated.
*/
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include "msm_vfe8x_proc.h"
#include <media/msm_camera.h>

struct msm_vfe8x_ctrl {
	/* bit 1:0 ENC_IRQ_MASK = 0x11:
	 * generate IRQ when both y and cbcr frame is ready. */

	/* bit 1:0 VIEW_IRQ_MASK= 0x11:
	 * generate IRQ when both y and cbcr frame is ready. */
	struct vfe_irq_composite_mask_config vfeIrqCompositeMaskLocal;
	struct vfe_module_enable vfeModuleEnableLocal;
	struct vfe_camif_cfg_data   vfeCamifConfigLocal;
	struct vfe_interrupt_mask   vfeImaskLocal;
	struct vfe_stats_cmd_data   vfeStatsCmdLocal;
	struct vfe_bus_cfg_data     vfeBusConfigLocal;
	struct vfe_cmd_bus_pm_start vfeBusPmConfigLocal;
	struct vfe_bus_cmd_data     vfeBusCmdLocal;
	enum vfe_interrupt_name     vfeInterruptNameLocal;
	uint32_t vfeLaBankSel;
	struct vfe_gamma_lut_sel  vfeGammaLutSel;

	boolean vfeStartAckPendingFlag;
	boolean vfeStopAckPending;
	boolean vfeResetAckPending;
	boolean vfeUpdateAckPending;

	enum VFE_AXI_OUTPUT_MODE        axiOutputMode;
	enum VFE_START_OPERATION_MODE   vfeOperationMode;

	uint32_t            vfeSnapShotCount;
	uint32_t            vfeRequestedSnapShotCount;
	boolean             vfeStatsPingPongReloadFlag;
	uint32_t            vfeFrameId;

	struct vfe_cmd_frame_skip_config vfeFrameSkip;
	uint32_t vfeFrameSkipPattern;
	uint8_t  vfeFrameSkipCount;
	uint8_t  vfeFrameSkipPeriod;

	boolean  vfeTestGenStartFlag;
	uint32_t vfeImaskPacked;
	uint32_t vfeImaskCompositePacked;
	enum VFE_RAW_PIXEL_DATA_SIZE       axiInputDataSize;
	struct vfe_irq_thread_msg          vfeIrqThreadMsgLocal;

	struct vfe_output_path_combo  viewPath;
	struct vfe_output_path_combo  encPath;
	struct vfe_frame_skip_counts vfeDroppedFrameCounts;
	struct vfe_stats_control afStatsControl;
	struct vfe_stats_control awbStatsControl;

	enum VFE_STATE  vstate;

	spinlock_t  ack_lock;
	spinlock_t  state_lock;
	spinlock_t  io_lock;

	struct msm_vfe_callback *resp;
	uint32_t extlen;
	void *extdata;

	spinlock_t  tasklet_lock;
	struct list_head tasklet_q;

	int vfeirq;
	void __iomem *vfebase;

	void *syncdata;
};
static struct msm_vfe8x_ctrl *ctrl;
static irqreturn_t vfe_parse_irq(int irq_num, void *data);

struct isr_queue_cmd {
	struct list_head list;
	struct vfe_interrupt_status        vfeInterruptStatus;
	struct vfe_frame_asf_info          vfeAsfFrameInfo;
	struct vfe_frame_bpc_info          vfeBpcFrameInfo;
	struct vfe_msg_camif_status        vfeCamifStatusLocal;
	struct vfe_bus_performance_monitor vfePmData;
};

static void vfe_prog_hw(uint8_t *hwreg,
	uint32_t *inptr, uint32_t regcnt)
{
	/* unsigned long flags; */
	uint32_t i;
	uint32_t *p;

	/* @todo This is causing issues, need further investigate */
	/* spin_lock_irqsave(&ctrl->io_lock, flags); */

	p = (uint32_t *)(hwreg);
	for (i = 0; i < (regcnt >> 2); i++)
		writel(*inptr++, p++);
		/* *p++ = *inptr++; */

	/* spin_unlock_irqrestore(&ctrl->io_lock, flags); */
}

static void vfe_read_reg_values(uint8_t *hwreg,
	uint32_t *dest, uint32_t count)
{
	/* unsigned long flags; */
	uint32_t *temp;
	uint32_t i;

	/* @todo This is causing issues, need further investigate */
	/* spin_lock_irqsave(&ctrl->io_lock, flags); */

	temp = (uint32_t *)(hwreg);
	for (i = 0; i < count; i++)
		*dest++ = *temp++;

	/* spin_unlock_irqrestore(&ctrl->io_lock, flags); */
}

static struct vfe_irqenable vfe_read_irq_mask(void)
{
	/* unsigned long flags; */
	uint32_t *temp;
	struct vfe_irqenable rc;

	memset(&rc, 0, sizeof(rc));

	/* @todo This is causing issues, need further investigate */
	/* spin_lock_irqsave(&ctrl->io_lock, flags); */
	temp = (uint32_t *)(ctrl->vfebase + VFE_IRQ_MASK);

	rc = *((struct vfe_irqenable *)temp);
	/* spin_unlock_irqrestore(&ctrl->io_lock, flags); */

	return rc;
}

static void
vfe_set_bus_pipo_addr(struct vfe_output_path_combo *vpath,
	struct vfe_output_path_combo *epath)
{
	vpath->yPath.hwRegPingAddress = (uint8_t *)
		(ctrl->vfebase + VFE_BUS_VIEW_Y_WR_PING_ADDR);
	vpath->yPath.hwRegPongAddress = (uint8_t *)
		(ctrl->vfebase + VFE_BUS_VIEW_Y_WR_PONG_ADDR);
	vpath->cbcrPath.hwRegPingAddress = (uint8_t *)
		(ctrl->vfebase + VFE_BUS_VIEW_CBCR_WR_PING_ADDR);
	vpath->cbcrPath.hwRegPongAddress = (uint8_t *)
		(ctrl->vfebase + VFE_BUS_VIEW_CBCR_WR_PONG_ADDR);

	epath->yPath.hwRegPingAddress = (uint8_t *)
		(ctrl->vfebase + VFE_BUS_ENC_Y_WR_PING_ADDR);
	epath->yPath.hwRegPongAddress = (uint8_t *)
		(ctrl->vfebase + VFE_BUS_ENC_Y_WR_PONG_ADDR);
	epath->cbcrPath.hwRegPingAddress = (uint8_t *)
		(ctrl->vfebase + VFE_BUS_ENC_CBCR_WR_PING_ADDR);
	epath->cbcrPath.hwRegPongAddress = (uint8_t *)
		(ctrl->vfebase + VFE_BUS_ENC_CBCR_WR_PONG_ADDR);
}

static void vfe_axi_output(struct vfe_cmd_axi_output_config *in,
	struct vfe_output_path_combo *out1,
	struct vfe_output_path_combo *out2, uint16_t out)
{
	struct vfe_axi_out_cfg cmd;

	uint16_t temp;
	uint32_t burstLength;

	/* force it to burst length 4, hardware does not support it. */
	burstLength = 1;

	/* AXI Output 2 Y Configuration*/
	/* VFE_BUS_ENC_Y_WR_PING_ADDR  */
	cmd.out2YPingAddr = out2->yPath.addressBuffer[0];

	/* VFE_BUS_ENC_Y_WR_PONG_ADDR  */
	cmd.out2YPongAddr = out2->yPath.addressBuffer[1];

	/* VFE_BUS_ENC_Y_WR_IMAGE_SIZE */
	cmd.out2YImageHeight = in->output2.outputY.imageHeight;
	/* convert the image width and row increment to be in
	 * unit of 64bit (8 bytes) */
	temp = (in->output2.outputY.imageWidth + (out - 1)) /
		out;
	cmd.out2YImageWidthin64bit = temp;

	/* VFE_BUS_ENC_Y_WR_BUFFER_CFG */
	cmd.out2YBurstLength = burstLength;
	cmd.out2YNumRows = in->output2.outputY.outRowCount;
	temp = (in->output2.outputY.outRowIncrement + (out - 1)) /
		out;
	cmd.out2YRowIncrementIn64bit = temp;

	/* AXI Output 2 Cbcr Configuration*/
	/* VFE_BUS_ENC_Cbcr_WR_PING_ADDR  */
	cmd.out2CbcrPingAddr = out2->cbcrPath.addressBuffer[0];

	/* VFE_BUS_ENC_Cbcr_WR_PONG_ADDR  */
	cmd.out2CbcrPongAddr = out2->cbcrPath.addressBuffer[1];

	/* VFE_BUS_ENC_Cbcr_WR_IMAGE_SIZE */
	cmd.out2CbcrImageHeight = in->output2.outputCbcr.imageHeight;
	temp = (in->output2.outputCbcr.imageWidth + (out - 1)) /
		out;
	cmd.out2CbcrImageWidthIn64bit = temp;

	/* VFE_BUS_ENC_Cbcr_WR_BUFFER_CFG */
	cmd.out2CbcrBurstLength = burstLength;
	cmd.out2CbcrNumRows = in->output2.outputCbcr.outRowCount;
	temp = (in->output2.outputCbcr.outRowIncrement + (out - 1)) /
		out;
	cmd.out2CbcrRowIncrementIn64bit = temp;

	/* AXI Output 1 Y Configuration */
	/* VFE_BUS_VIEW_Y_WR_PING_ADDR  */
	cmd.out1YPingAddr = out1->yPath.addressBuffer[0];

	/* VFE_BUS_VIEW_Y_WR_PONG_ADDR */
	cmd.out1YPongAddr = out1->yPath.addressBuffer[1];

	/* VFE_BUS_VIEW_Y_WR_IMAGE_SIZE */
	cmd.out1YImageHeight = in->output1.outputY.imageHeight;
	temp = (in->output1.outputY.imageWidth + (out - 1)) /
		out;
	cmd.out1YImageWidthin64bit = temp;

	/* VFE_BUS_VIEW_Y_WR_BUFFER_CFG     */
	cmd.out1YBurstLength = burstLength;
	cmd.out1YNumRows = in->output1.outputY.outRowCount;

	temp =
		(in->output1.outputY.outRowIncrement +
		 (out - 1)) / out;
	cmd.out1YRowIncrementIn64bit = temp;

	/* AXI Output 1 Cbcr Configuration*/
	cmd.out1CbcrPingAddr = out1->cbcrPath.addressBuffer[0];

	/* VFE_BUS_VIEW_Cbcr_WR_PONG_ADDR  */
	cmd.out1CbcrPongAddr =
		out1->cbcrPath.addressBuffer[1];

	/* VFE_BUS_VIEW_Cbcr_WR_IMAGE_SIZE */
	cmd.out1CbcrImageHeight = in->output1.outputCbcr.imageHeight;
	temp = (in->output1.outputCbcr.imageWidth +
		(out - 1)) / out;
	cmd.out1CbcrImageWidthIn64bit = temp;

	cmd.out1CbcrBurstLength = burstLength;
	cmd.out1CbcrNumRows = in->output1.outputCbcr.outRowCount;
	temp =
		(in->output1.outputCbcr.outRowIncrement +
		 (out - 1)) / out;

	cmd.out1CbcrRowIncrementIn64bit = temp;

	vfe_prog_hw(ctrl->vfebase + VFE_BUS_ENC_Y_WR_PING_ADDR,
		(uint32_t *)&cmd, sizeof(cmd));
}

static void vfe_reg_bus_cfg(struct vfe_bus_cfg_data *in)
{
	struct vfe_axi_bus_cfg cmd;

	cmd.stripeRdPathEn      = in->stripeRdPathEn;
	cmd.encYWrPathEn        = in->encYWrPathEn;
	cmd.encCbcrWrPathEn     = in->encCbcrWrPathEn;
	cmd.viewYWrPathEn       = in->viewYWrPathEn;
	cmd.viewCbcrWrPathEn    = in->viewCbcrWrPathEn;
	cmd.rawPixelDataSize    = (uint32_t)in->rawPixelDataSize;
	cmd.rawWritePathSelect  = (uint32_t)in->rawWritePathSelect;

	/*  program vfe_bus_cfg */
	writel(*((uint32_t *)&cmd), ctrl->vfebase + VFE_BUS_CFG);
}

static void vfe_reg_camif_config(struct vfe_camif_cfg_data *in)
{
	struct VFE_CAMIFConfigType cfg;

	memset(&cfg, 0, sizeof(cfg));

	cfg.VSyncEdge          =
		in->camifCfgFromCmd.vSyncEdge;

	cfg.HSyncEdge          =
		in->camifCfgFromCmd.hSyncEdge;

	cfg.syncMode           =
		in->camifCfgFromCmd.syncMode;

	cfg.vfeSubsampleEnable =
		in->camifCfgFromCmd.vfeSubSampleEnable;

	cfg.busSubsampleEnable =
		in->camifCfgFromCmd.busSubSampleEnable;

	cfg.camif2vfeEnable    =
		in->camif2OutputEnable;

	cfg.camif2busEnable    =
		in->camif2BusEnable;

	cfg.irqSubsampleEnable =
		in->camifCfgFromCmd.irqSubSampleEnable;

	cfg.binningEnable      =
		in->camifCfgFromCmd.binningEnable;

	cfg.misrEnable         =
		in->camifCfgFromCmd.misrEnable;

	/*  program camif_config */
	writel(*((uint32_t *)&cfg), ctrl->vfebase + CAMIF_CONFIG);
}

static void vfe_reg_bus_cmd(struct vfe_bus_cmd_data *in)
{
	struct vfe_buscmd cmd;
	memset(&cmd, 0, sizeof(cmd));

	cmd.stripeReload        = in->stripeReload;
	cmd.busPingpongReload   = in->busPingpongReload;
	cmd.statsPingpongReload = in->statsPingpongReload;

	writel(*((uint32_t *)&cmd), ctrl->vfebase + VFE_BUS_CMD);

	CDBG("bus command = 0x%x\n", (*((uint32_t *)&cmd)));

	/* this is needed, as the control bits are pulse based.
	 * Don't want to reload bus pingpong again. */
	in->busPingpongReload = 0;
	in->statsPingpongReload = 0;
	in->stripeReload = 0;
}

static void vfe_reg_module_cfg(struct vfe_module_enable *in)
{
	struct vfe_mod_enable ena;

	memset(&ena, 0, sizeof(ena));

	ena.blackLevelCorrectionEnable = in->blackLevelCorrectionEnable;
	ena.lensRollOffEnable          = in->lensRollOffEnable;
	ena.demuxEnable                = in->demuxEnable;
	ena.chromaUpsampleEnable       = in->chromaUpsampleEnable;
	ena.demosaicEnable             = in->demosaicEnable;
	ena.statsEnable                = in->statsEnable;
	ena.cropEnable                 = in->cropEnable;
	ena.mainScalerEnable           = in->mainScalerEnable;
	ena.whiteBalanceEnable         = in->whiteBalanceEnable;
	ena.colorCorrectionEnable      = in->colorCorrectionEnable;
	ena.yHistEnable                = in->yHistEnable;
	ena.skinToneEnable             = in->skinToneEnable;
	ena.lumaAdaptationEnable       = in->lumaAdaptationEnable;
	ena.rgbLUTEnable               = in->rgbLUTEnable;
	ena.chromaEnhanEnable          = in->chromaEnhanEnable;
	ena.asfEnable                  = in->asfEnable;
	ena.chromaSuppressionEnable    = in->chromaSuppressionEnable;
	ena.chromaSubsampleEnable      = in->chromaSubsampleEnable;
	ena.scaler2YEnable             = in->scaler2YEnable;
	ena.scaler2CbcrEnable          = in->scaler2CbcrEnable;

	writel(*((uint32_t *)&ena), ctrl->vfebase + VFE_MODULE_CFG);
}

static void vfe_program_dmi_cfg(enum VFE_DMI_RAM_SEL bankSel)
{
	/* set bit 8 for auto increment. */
	uint32_t value = (uint32_t) ctrl->vfebase + VFE_DMI_CFG_DEFAULT;

	value += (uint32_t)bankSel;
	/* CDBG("dmi cfg input bank is  0x%x\n", bankSel); */

	writel(value, ctrl->vfebase + VFE_DMI_CFG);
	writel(0, ctrl->vfebase + VFE_DMI_ADDR);
}

static void vfe_write_lens_roll_off_table(
	struct vfe_cmd_roll_off_config *in)
{
	uint16_t i;
	uint32_t data;

	uint16_t *initGr = in->initTableGr;
	uint16_t *initGb = in->initTableGb;
	uint16_t *initB =  in->initTableB;
	uint16_t *initR =  in->initTableR;

	int16_t *pDeltaGr = in->deltaTableGr;
	int16_t *pDeltaGb = in->deltaTableGb;
	int16_t *pDeltaB =  in->deltaTableB;
	int16_t *pDeltaR =  in->deltaTableR;

	vfe_program_dmi_cfg(ROLLOFF_RAM);

	/* first pack and write init table */
	for (i = 0; i < VFE_ROLL_OFF_INIT_TABLE_SIZE; i++) {
		data = (((uint32_t)(*initR)) & 0x0000FFFF) |
			(((uint32_t)(*initGr)) << 16);
		initR++;
		initGr++;

		writel(data, ctrl->vfebase + VFE_DMI_DATA_LO);

		data = (((uint32_t)(*initB)) & 0x0000FFFF) |
			(((uint32_t)(*initGr))<<16);
		initB++;
		initGb++;

		writel(data, ctrl->vfebase + VFE_DMI_DATA_LO);
	}

	/* there are gaps between the init table and delta table,
	 * set the offset for delta table. */
	writel(LENS_ROLL_OFF_DELTA_TABLE_OFFSET,
		ctrl->vfebase + VFE_DMI_ADDR);

	/* pack and write delta table */
	for (i = 0; i < VFE_ROLL_OFF_DELTA_TABLE_SIZE; i++) {
		data = (((int32_t)(*pDeltaR)) & 0x0000FFFF) |
			(((int32_t)(*pDeltaGr))<<16);
		pDeltaR++;
		pDeltaGr++;

		writel(data, ctrl->vfebase + VFE_DMI_DATA_LO);

		data = (((int32_t)(*pDeltaB)) & 0x0000FFFF) |
			(((int32_t)(*pDeltaGb))<<16);
		pDeltaB++;
		pDeltaGb++;

		writel(data, ctrl->vfebase + VFE_DMI_DATA_LO);
	}

	/* After DMI transfer, to make it safe, need to set the
	 * DMI_CFG to unselect any SRAM
	 */
	/* unselect the SRAM Bank. */
	writel(VFE_DMI_CFG_DEFAULT, ctrl->vfebase + VFE_DMI_CFG);
}

static void vfe_set_default_reg_values(void)
{
	writel(0x800080, ctrl->vfebase + VFE_DEMUX_GAIN_0);
	writel(0x800080, ctrl->vfebase + VFE_DEMUX_GAIN_1);
	writel(0xFFFFF, ctrl->vfebase + VFE_CGC_OVERRIDE);

	/* default frame drop period and pattern */
	writel(0x1f, ctrl->vfebase + VFE_FRAMEDROP_ENC_Y_CFG);
	writel(0x1f, ctrl->vfebase + VFE_FRAMEDROP_ENC_CBCR_CFG);
	writel(0xFFFFFFFF, ctrl->vfebase + VFE_FRAMEDROP_ENC_Y_PATTERN);
	writel(0xFFFFFFFF, ctrl->vfebase + VFE_FRAMEDROP_ENC_CBCR_PATTERN);
	writel(0x1f, ctrl->vfebase + VFE_FRAMEDROP_VIEW_Y_CFG);
	writel(0x1f, ctrl->vfebase + VFE_FRAMEDROP_VIEW_CBCR_CFG);
	writel(0xFFFFFFFF, ctrl->vfebase + VFE_FRAMEDROP_VIEW_Y_PATTERN);
	writel(0xFFFFFFFF, ctrl->vfebase + VFE_FRAMEDROP_VIEW_CBCR_PATTERN);
	writel(0, ctrl->vfebase + VFE_CLAMP_MIN_CFG);
	writel(0xFFFFFF, ctrl->vfebase + VFE_CLAMP_MAX_CFG);
}

static void vfe_config_demux(uint32_t period, uint32_t even, uint32_t odd)
{
	writel(period, ctrl->vfebase + VFE_DEMUX_CFG);
	writel(even, ctrl->vfebase + VFE_DEMUX_EVEN_CFG);
	writel(odd, ctrl->vfebase + VFE_DEMUX_ODD_CFG);
}

static void vfe_pm_stop(void)
{
	writel(VFE_PERFORMANCE_MONITOR_STOP, ctrl->vfebase + VFE_BUS_PM_CMD);
}

static void vfe_program_bus_rd_irq_en(uint32_t value)
{
	writel(value, ctrl->vfebase + VFE_BUS_PINGPONG_IRQ_EN);
}

static void vfe_camif_go(void)
{
	writel(CAMIF_COMMAND_START, ctrl->vfebase + CAMIF_COMMAND);
}

static void vfe_camif_stop_immediately(void)
{
	writel(CAMIF_COMMAND_STOP_IMMEDIATELY, ctrl->vfebase + CAMIF_COMMAND);
	writel(0, ctrl->vfebase + VFE_CGC_OVERRIDE);
}

static void vfe_program_reg_update_cmd(uint32_t value)
{
	writel(value, ctrl->vfebase + VFE_REG_UPDATE_CMD);
}

static void vfe_program_bus_cmd(uint32_t value)
{
	writel(value, ctrl->vfebase + VFE_BUS_CMD);
}

static void vfe_program_global_reset_cmd(uint32_t value)
{
	writel(value, ctrl->vfebase + VFE_GLOBAL_RESET_CMD);
}

static void vfe_program_axi_cmd(uint32_t value)
{
	writel(value, ctrl->vfebase + VFE_AXI_CMD);
}

static void vfe_program_irq_composite_mask(uint32_t value)
{
	writel(value, ctrl->vfebase + VFE_IRQ_COMPOSITE_MASK);
}

static inline void vfe_program_irq_mask(uint32_t value)
{
	writel(value, ctrl->vfebase + VFE_IRQ_MASK);
}

static void vfe_program_chroma_upsample_cfg(uint32_t value)
{
	writel(value, ctrl->vfebase + VFE_CHROMA_UPSAMPLE_CFG);
}

static uint32_t vfe_read_axi_status(void)
{
	return readl(ctrl->vfebase + VFE_AXI_STATUS);
}

static uint32_t vfe_read_pm_status_in_raw_capture(void)
{
	return readl(ctrl->vfebase + VFE_BUS_ENC_CBCR_WR_PM_STATS_1);
}

static void
vfe_set_stats_pingpong_address(struct vfe_stats_control *afControl,
	struct vfe_stats_control *awbControl)
{
	afControl->hwRegPingAddress = (uint8_t *)
		(ctrl->vfebase + VFE_BUS_STATS_AF_WR_PING_ADDR);
	afControl->hwRegPongAddress = (uint8_t *)
		(ctrl->vfebase + VFE_BUS_STATS_AF_WR_PONG_ADDR);

	awbControl->hwRegPingAddress = (uint8_t *)
		(ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PING_ADDR);
	awbControl->hwRegPongAddress = (uint8_t *)
		(ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PONG_ADDR);
}

static uint32_t vfe_read_camif_status(void)
{
	return readl(ctrl->vfebase + CAMIF_STATUS);
}

static void vfe_program_lut_bank_sel(struct vfe_gamma_lut_sel *in)
{
	struct VFE_GammaLutSelect_ConfigCmdType cmd;

	memset(&cmd, 0, sizeof(cmd));

	cmd.ch0BankSelect = in->ch0BankSelect;
	cmd.ch1BankSelect = in->ch1BankSelect;
	cmd.ch2BankSelect = in->ch2BankSelect;
	CDBG("VFE gamma lut bank selection is 0x%x\n", *((uint32_t *)&cmd));
	vfe_prog_hw(ctrl->vfebase + VFE_LUT_BANK_SEL,
		(uint32_t *)&cmd, sizeof(cmd));
}

static void vfe_program_stats_cmd(struct vfe_stats_cmd_data *in)
{
	struct VFE_StatsCmdType stats;
	memset(&stats, 0, sizeof(stats));

	stats.autoFocusEnable        = in->autoFocusEnable;
	stats.axwEnable              = in->axwEnable;
	stats.histEnable             = in->histEnable;
	stats.clearHistEnable        = in->clearHistEnable;
	stats.histAutoClearEnable    = in->histAutoClearEnable;
	stats.colorConversionEnable  = in->colorConversionEnable;

	writel(*((uint32_t *)&stats), ctrl->vfebase + VFE_STATS_CMD);
}

static void vfe_pm_start(struct vfe_cmd_bus_pm_start *in)
{
	struct VFE_Bus_Pm_ConfigCmdType cmd;
	memset(&cmd, 0, sizeof(struct VFE_Bus_Pm_ConfigCmdType));

	cmd.output2YWrPmEnable     = in->output2YWrPmEnable;
	cmd.output2CbcrWrPmEnable  = in->output2CbcrWrPmEnable;
	cmd.output1YWrPmEnable     = in->output1YWrPmEnable;
	cmd.output1CbcrWrPmEnable  = in->output1CbcrWrPmEnable;

	vfe_prog_hw(ctrl->vfebase + VFE_BUS_PM_CFG,
		(uint32_t *)&cmd, sizeof(cmd));
}

static void vfe_8k_pm_start(struct vfe_cmd_bus_pm_start *in)
{
	in->output1CbcrWrPmEnable = ctrl->vfeBusConfigLocal.viewCbcrWrPathEn;
	in->output1YWrPmEnable    = ctrl->vfeBusConfigLocal.viewYWrPathEn;
	in->output2CbcrWrPmEnable = ctrl->vfeBusConfigLocal.encCbcrWrPathEn;
	in->output2YWrPmEnable    = ctrl->vfeBusConfigLocal.encYWrPathEn;

	if (in->output1CbcrWrPmEnable || in->output1YWrPmEnable)
		ctrl->viewPath.pmEnabled = TRUE;

	if (in->output2CbcrWrPmEnable || in->output2YWrPmEnable)
		ctrl->encPath.pmEnabled = TRUE;

	vfe_pm_start(in);

	writel(VFE_PERFORMANCE_MONITOR_GO, ctrl->vfebase + VFE_BUS_PM_CMD);
}

static uint32_t vfe_irq_pack(struct vfe_interrupt_mask data)
{
	struct vfe_irqenable packedData;

	memset(&packedData, 0, sizeof(packedData));

	packedData.camifErrorIrq          = data.camifErrorIrq;
	packedData.camifSofIrq            = data.camifSofIrq;
	packedData.camifEolIrq            = data.camifEolIrq;
	packedData.camifEofIrq            = data.camifEofIrq;
	packedData.camifEpoch1Irq         = data.camifEpoch1Irq;
	packedData.camifEpoch2Irq         = data.camifEpoch2Irq;
	packedData.camifOverflowIrq       = data.camifOverflowIrq;
	packedData.ceIrq                  = data.ceIrq;
	packedData.regUpdateIrq           = data.regUpdateIrq;
	packedData.resetAckIrq            = data.resetAckIrq;
	packedData.encYPingpongIrq        = data.encYPingpongIrq;
	packedData.encCbcrPingpongIrq     = data.encCbcrPingpongIrq;
	packedData.viewYPingpongIrq       = data.viewYPingpongIrq;
	packedData.viewCbcrPingpongIrq    = data.viewCbcrPingpongIrq;
	packedData.rdPingpongIrq          = data.rdPingpongIrq;
	packedData.afPingpongIrq          = data.afPingpongIrq;
	packedData.awbPingpongIrq         = data.awbPingpongIrq;
	packedData.histPingpongIrq        = data.histPingpongIrq;
	packedData.encIrq                 = data.encIrq;
	packedData.viewIrq                = data.viewIrq;
	packedData.busOverflowIrq         = data.busOverflowIrq;
	packedData.afOverflowIrq          = data.afOverflowIrq;
	packedData.awbOverflowIrq         = data.awbOverflowIrq;
	packedData.syncTimer0Irq          = data.syncTimer0Irq;
	packedData.syncTimer1Irq          = data.syncTimer1Irq;
	packedData.syncTimer2Irq          = data.syncTimer2Irq;
	packedData.asyncTimer0Irq         = data.asyncTimer0Irq;
	packedData.asyncTimer1Irq         = data.asyncTimer1Irq;
	packedData.asyncTimer2Irq         = data.asyncTimer2Irq;
	packedData.asyncTimer3Irq         = data.asyncTimer3Irq;
	packedData.axiErrorIrq            = data.axiErrorIrq;
	packedData.violationIrq           = data.violationIrq;

	return *((uint32_t *)&packedData);
}

static uint32_t
vfe_irq_composite_pack(struct vfe_irq_composite_mask_config data)
{
	struct VFE_Irq_Composite_MaskType packedData;

	memset(&packedData, 0, sizeof(packedData));

	packedData.encIrqComMaskBits   = data.encIrqComMask;
	packedData.viewIrqComMaskBits  = data.viewIrqComMask;
	packedData.ceDoneSelBits       = data.ceDoneSel;

	return *((uint32_t *)&packedData);
}

static void vfe_addr_convert(struct msm_vfe_phy_info *pinfo,
	enum vfe_resp_msg type, void *data, void **ext, int32_t *elen)
{
	switch (type) {
	case VFE_MSG_OUTPUT1: {
		pinfo->y_phy =
			((struct vfe_message *)data)->_u.msgOutput1.yBuffer;
		pinfo->cbcr_phy =
			((struct vfe_message *)data)->_u.msgOutput1.cbcrBuffer;

		((struct vfe_frame_extra *)ctrl->extdata)->bpcInfo =
		((struct vfe_message *)data)->_u.msgOutput1.bpcInfo;

		((struct vfe_frame_extra *)ctrl->extdata)->asfInfo =
		((struct vfe_message *)data)->_u.msgOutput1.asfInfo;

		((struct vfe_frame_extra *)ctrl->extdata)->frameCounter =
		((struct vfe_message *)data)->_u.msgOutput1.frameCounter;

		((struct vfe_frame_extra *)ctrl->extdata)->pmData =
		((struct vfe_message *)data)->_u.msgOutput1.pmData;

		*ext  = ctrl->extdata;
		*elen = ctrl->extlen;
	}
		break;

	case VFE_MSG_OUTPUT2: {
		pinfo->y_phy =
			((struct vfe_message *)data)->_u.msgOutput2.yBuffer;
		pinfo->cbcr_phy =
			((struct vfe_message *)data)->_u.msgOutput2.cbcrBuffer;

		CDBG("vfe_addr_convert, pinfo->y_phy = 0x%x\n", pinfo->y_phy);
		CDBG("vfe_addr_convert, pinfo->cbcr_phy = 0x%x\n",
			pinfo->cbcr_phy);

		((struct vfe_frame_extra *)ctrl->extdata)->bpcInfo =
		((struct vfe_message *)data)->_u.msgOutput2.bpcInfo;

		((struct vfe_frame_extra *)ctrl->extdata)->asfInfo =
		((struct vfe_message *)data)->_u.msgOutput2.asfInfo;

		((struct vfe_frame_extra *)ctrl->extdata)->frameCounter =
		((struct vfe_message *)data)->_u.msgOutput2.frameCounter;

		((struct vfe_frame_extra *)ctrl->extdata)->pmData =
		((struct vfe_message *)data)->_u.msgOutput2.pmData;

		*ext  = ctrl->extdata;
		*elen = ctrl->extlen;
	}
		break;

	case VFE_MSG_STATS_AF:
		pinfo->sbuf_phy =
		((struct vfe_message *)data)->_u.msgStatsAf.afBuffer;
		break;

	case VFE_MSG_STATS_WE:
		pinfo->sbuf_phy =
		((struct vfe_message *)data)->_u.msgStatsWbExp.awbBuffer;
		break;

	default:
		break;
	} /* switch */
}

static void
vfe_proc_ops(enum VFE_MESSAGE_ID id, void *msg, size_t len)
{
	struct msm_vfe_resp *rp;

	/* In 8k, OUTPUT1 & OUTPUT2 messages arrive before
	 * SNAPSHOT_DONE. We don't send such messages to user */

	CDBG("ctrl->vfeOperationMode = %d, msgId = %d\n",
		ctrl->vfeOperationMode, id);

	if ((ctrl->vfeOperationMode == VFE_START_OPERATION_MODE_SNAPSHOT) &&
		(id == VFE_MSG_ID_OUTPUT1 || id == VFE_MSG_ID_OUTPUT2)) {
		return;
	}

	rp = ctrl->resp->vfe_alloc(sizeof(struct msm_vfe_resp), ctrl->syncdata);
	if (!rp) {
		CDBG("rp: cannot allocate buffer\n");
		return;
	}

	CDBG("vfe_proc_ops, msgId = %d\n", id);

	rp->evt_msg.type   = MSM_CAMERA_MSG;
	rp->evt_msg.msg_id = id;
	rp->evt_msg.len    = len;
	rp->evt_msg.data   = msg;

	switch (rp->evt_msg.msg_id) {
	case VFE_MSG_ID_SNAPSHOT_DONE:
		rp->type = VFE_MSG_SNAPSHOT;
		break;

	case VFE_MSG_ID_OUTPUT1:
		rp->type = VFE_MSG_OUTPUT1;
		vfe_addr_convert(&(rp->phy), VFE_MSG_OUTPUT1,
			rp->evt_msg.data, &(rp->extdata),
			&(rp->extlen));
		break;

	case VFE_MSG_ID_OUTPUT2:
		rp->type = VFE_MSG_OUTPUT2;
		vfe_addr_convert(&(rp->phy), VFE_MSG_OUTPUT2,
				rp->evt_msg.data, &(rp->extdata),
				&(rp->extlen));
		break;

	case VFE_MSG_ID_STATS_AUTOFOCUS:
		rp->type = VFE_MSG_STATS_AF;
		vfe_addr_convert(&(rp->phy), VFE_MSG_STATS_AF,
				rp->evt_msg.data, NULL, NULL);
		break;

	case VFE_MSG_ID_STATS_WB_EXP:
		rp->type = VFE_MSG_STATS_WE;
		vfe_addr_convert(&(rp->phy), VFE_MSG_STATS_WE,
				rp->evt_msg.data, NULL, NULL);
		break;

	default:
		rp->type = VFE_MSG_GENERAL;
		break;
	}

	ctrl->resp->vfe_resp(rp, MSM_CAM_Q_VFE_MSG, ctrl->syncdata);
}

static void vfe_send_msg_no_payload(enum VFE_MESSAGE_ID id)
{
	struct vfe_message *msg;

	msg = kzalloc(sizeof(*msg), GFP_ATOMIC);
	if (!msg)
		return;

	msg->_d = id;
	vfe_proc_ops(id, msg, 0);
}

static void vfe_send_bus_overflow_msg(void)
{
	struct vfe_message *msg;
	msg =
		kzalloc(sizeof(struct vfe_message), GFP_ATOMIC);
	if (!msg)
		return;

	msg->_d = VFE_MSG_ID_BUS_OVERFLOW;
#if 0
	memcpy(&(msg->_u.msgBusOverflow),
		&ctrl->vfePmData, sizeof(ctrl->vfePmData));
#endif

	vfe_proc_ops(VFE_MSG_ID_BUS_OVERFLOW,
		msg, sizeof(struct vfe_message));
}

static void vfe_send_camif_error_msg(void)
{
#if 0
	struct vfe_message *msg;
	msg =
		kzalloc(sizeof(struct vfe_message), GFP_ATOMIC);
	if (!msg)
		return;

	msg->_d = VFE_MSG_ID_CAMIF_ERROR;
	memcpy(&(msg->_u.msgCamifError),
		&ctrl->vfeCamifStatusLocal, sizeof(ctrl->vfeCamifStatusLocal));

	vfe_proc_ops(VFE_MSG_ID_CAMIF_ERROR,
		msg, sizeof(struct vfe_message));
#endif
}

static void vfe_process_error_irq(
	struct vfe_interrupt_status *irqstatus)
{
	/* all possible error irq.  Note error irqs are not enabled, it is
	 * checked only when other interrupts are present. */
	if (irqstatus->afOverflowIrq)
		vfe_send_msg_no_payload(VFE_MSG_ID_AF_OVERFLOW);

	if (irqstatus->awbOverflowIrq)
		vfe_send_msg_no_payload(VFE_MSG_ID_AWB_OVERFLOW);

	if (irqstatus->axiErrorIrq)
		vfe_send_msg_no_payload(VFE_MSG_ID_AXI_ERROR);

	if (irqstatus->busOverflowIrq)
		vfe_send_bus_overflow_msg();

	if (irqstatus->camifErrorIrq)
		vfe_send_camif_error_msg();

	if (irqstatus->camifOverflowIrq)
		vfe_send_msg_no_payload(VFE_MSG_ID_CAMIF_OVERFLOW);

	if (irqstatus->violationIrq)
		;
}

static void vfe_process_camif_sof_irq(void)
{
	/* increment the frame id number. */
	ctrl->vfeFrameId++;

	CDBG("camif_sof_irq, frameId = %d\n",
		ctrl->vfeFrameId);

	/* In snapshot mode, if frame skip is programmed,
	* need to check it accordingly to stop camif at
	* correct frame boundary. For the dropped frames,
	* there won't be any output path irqs, but there is
	* still SOF irq, which can help us determine when
	* to stop the camif.
	*/
	if (ctrl->vfeOperationMode) {
		if ((1 << ctrl->vfeFrameSkipCount) &
				ctrl->vfeFrameSkipPattern) {

			ctrl->vfeSnapShotCount--;
			if (ctrl->vfeSnapShotCount == 0)
				/* terminate vfe pipeline at frame boundary. */
				writel(CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY,
					ctrl->vfebase + CAMIF_COMMAND);
		}

		/* update frame skip counter for bit checking. */
		ctrl->vfeFrameSkipCount++;
		if (ctrl->vfeFrameSkipCount ==
				(ctrl->vfeFrameSkipPeriod + 1))
			ctrl->vfeFrameSkipCount = 0;
	}
}

static int vfe_get_af_pingpong_status(void)
{
	uint32_t busPingPongStatus;
	int rc = 0;

	busPingPongStatus =
		readl(ctrl->vfebase + VFE_BUS_PINGPONG_STATUS);

	if ((busPingPongStatus & VFE_AF_PINGPONG_STATUS_BIT) == 0)
		return -EFAULT;

	return rc;
}

static uint32_t vfe_read_af_buf_addr(boolean pipo)
{
	if (pipo == FALSE)
		return readl(ctrl->vfebase + VFE_BUS_STATS_AF_WR_PING_ADDR);
	else
		return readl(ctrl->vfebase + VFE_BUS_STATS_AF_WR_PONG_ADDR);
}

static void
vfe_update_af_buf_addr(boolean pipo, uint32_t addr)
{
	if (pipo == FALSE)
		writel(addr, ctrl->vfebase + VFE_BUS_STATS_AF_WR_PING_ADDR);
	else
		writel(addr, ctrl->vfebase + VFE_BUS_STATS_AF_WR_PONG_ADDR);
}

static void
vfe_send_af_stats_msg(uint32_t afBufAddress)
{
	/* unsigned long flags; */
	struct vfe_message *msg;
	msg =
		kzalloc(sizeof(struct vfe_message), GFP_ATOMIC);
	if (!msg)
		return;

	/* fill message with right content. */
	/* @todo This is causing issues, need further investigate */
	/* spin_lock_irqsave(&ctrl->state_lock, flags); */
	if (ctrl->vstate != VFE_STATE_ACTIVE) {
		kfree(msg);
		goto af_stats_done;
	}

	msg->_d = VFE_MSG_ID_STATS_AUTOFOCUS;
	msg->_u.msgStatsAf.afBuffer = afBufAddress;
	msg->_u.msgStatsAf.frameCounter = ctrl->vfeFrameId;

	vfe_proc_ops(VFE_MSG_ID_STATS_AUTOFOCUS,
		msg, sizeof(struct vfe_message));

	ctrl->afStatsControl.ackPending = TRUE;

af_stats_done:
	/* spin_unlock_irqrestore(&ctrl->state_lock, flags); */
	return;
}

static void vfe_process_stats_af_irq(void)
{
	boolean bufferAvailable;

	if (!(ctrl->afStatsControl.ackPending)) {

		/* read hardware status. */
		ctrl->afStatsControl.pingPongStatus =
			vfe_get_af_pingpong_status();

		bufferAvailable =
			(ctrl->afStatsControl.pingPongStatus) ^ 1;

		ctrl->afStatsControl.bufToRender =
			vfe_read_af_buf_addr(bufferAvailable);

		/* update the same buffer address (ping or pong) */
		vfe_update_af_buf_addr(bufferAvailable,
			ctrl->afStatsControl.nextFrameAddrBuf);

		vfe_send_af_stats_msg(ctrl->afStatsControl.bufToRender);
	} else
		ctrl->afStatsControl.droppedStatsFrameCount++;
}

static boolean vfe_get_awb_pingpong_status(void)
{
	uint32_t busPingPongStatus;

	busPingPongStatus =
		readl(ctrl->vfebase + VFE_BUS_PINGPONG_STATUS);

	if ((busPingPongStatus & VFE_AWB_PINGPONG_STATUS_BIT) == 0)
		return FALSE;

	return TRUE;
}

static uint32_t
vfe_read_awb_buf_addr(boolean pingpong)
{
	if (pingpong == FALSE)
		return readl(ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PING_ADDR);
	else
		return readl(ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PONG_ADDR);
}

static void vfe_update_awb_buf_addr(
	boolean pingpong, uint32_t addr)
{
	if (pingpong == FALSE)
		writel(addr, ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PING_ADDR);
	else
		writel(addr, ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PONG_ADDR);
}

static void vfe_send_awb_stats_msg(uint32_t awbBufAddress)
{
	/* unsigned long flags; */
	struct vfe_message   *msg;

	msg =
		kzalloc(sizeof(struct vfe_message), GFP_ATOMIC);
	if (!msg)
		return;

	/* fill message with right content. */
	/* @todo This is causing issues, need further investigate */
	/* spin_lock_irqsave(&ctrl->state_lock, flags); */
	if (ctrl->vstate != VFE_STATE_ACTIVE) {
		kfree(msg);
		goto awb_stats_done;
	}

	msg->_d = VFE_MSG_ID_STATS_WB_EXP;
	msg->_u.msgStatsWbExp.awbBuffer = awbBufAddress;
	msg->_u.msgStatsWbExp.frameCounter = ctrl->vfeFrameId;

	vfe_proc_ops(VFE_MSG_ID_STATS_WB_EXP,
		msg, sizeof(struct vfe_message));

	ctrl->awbStatsControl.ackPending = TRUE;

awb_stats_done:
	/* spin_unlock_irqrestore(&ctrl->state_lock, flags); */
	return;
}

static void vfe_process_stats_awb_irq(void)
{
	boolean bufferAvailable;

	if (!(ctrl->awbStatsControl.ackPending)) {

		ctrl->awbStatsControl.pingPongStatus =
			vfe_get_awb_pingpong_status();

		bufferAvailable = (ctrl->awbStatsControl.pingPongStatus) ^ 1;

		ctrl->awbStatsControl.bufToRender =
			vfe_read_awb_buf_addr(bufferAvailable);

		vfe_update_awb_buf_addr(bufferAvailable,
			ctrl->awbStatsControl.nextFrameAddrBuf);

		vfe_send_awb_stats_msg(ctrl->awbStatsControl.bufToRender);

	} else
		ctrl->awbStatsControl.droppedStatsFrameCount++;
}

static void vfe_process_sync_timer_irq(
	struct vfe_interrupt_status *irqstatus)
{
	if (irqstatus->syncTimer0Irq)
		vfe_send_msg_no_payload(VFE_MSG_ID_SYNC_TIMER0_DONE);

	if (irqstatus->syncTimer1Irq)
		vfe_send_msg_no_payload(VFE_MSG_ID_SYNC_TIMER1_DONE);

	if (irqstatus->syncTimer2Irq)
		vfe_send_msg_no_payload(VFE_MSG_ID_SYNC_TIMER2_DONE);
}

static void vfe_process_async_timer_irq(
	struct vfe_interrupt_status *irqstatus)
{

	if (irqstatus->asyncTimer0Irq)
		vfe_send_msg_no_payload(VFE_MSG_ID_ASYNC_TIMER0_DONE);

	if (irqstatus->asyncTimer1Irq)
		vfe_send_msg_no_payload(VFE_MSG_ID_ASYNC_TIMER1_DONE);

	if (irqstatus->asyncTimer2Irq)
		vfe_send_msg_no_payload(VFE_MSG_ID_ASYNC_TIMER2_DONE);

	if (irqstatus->asyncTimer3Irq)
		vfe_send_msg_no_payload(VFE_MSG_ID_ASYNC_TIMER3_DONE);
}

static void vfe_send_violation_msg(void)
{
	vfe_send_msg_no_payload(VFE_MSG_ID_VIOLATION);
}

static void vfe_send_async_timer_msg(void)
{
	vfe_send_msg_no_payload(VFE_MSG_ID_ASYNC_TIMER0_DONE);
}

static void vfe_write_gamma_table(uint8_t channel,
	boolean bank, int16_t *pTable)
{
	uint16_t i;

	enum VFE_DMI_RAM_SEL dmiRamSel = NO_MEM_SELECTED;

	switch (channel) {
	case 0:
		if (bank == 0)
			dmiRamSel = RGBLUT_RAM_CH0_BANK0;
		else
			dmiRamSel = RGBLUT_RAM_CH0_BANK1;
		break;

	case 1:
		if (bank == 0)
			dmiRamSel = RGBLUT_RAM_CH1_BANK0;
		else
			dmiRamSel = RGBLUT_RAM_CH1_BANK1;
		break;

	case 2:
		if (bank == 0)
			dmiRamSel = RGBLUT_RAM_CH2_BANK0;
		else
			dmiRamSel = RGBLUT_RAM_CH2_BANK1;
		break;

	default:
		break;
	}

	vfe_program_dmi_cfg(dmiRamSel);

	for (i = 0; i < VFE_GAMMA_TABLE_LENGTH; i++) {
		writel((uint32_t)(*pTable), ctrl->vfebase + VFE_DMI_DATA_LO);
		pTable++;
	}

	/* After DMI transfer, need to set the DMI_CFG to unselect any SRAM
	unselect the SRAM Bank. */
	writel(VFE_DMI_CFG_DEFAULT, ctrl->vfebase + VFE_DMI_CFG);
}

static void vfe_prog_hw_testgen_cmd(uint32_t value)
{
	writel(value, ctrl->vfebase + VFE_HW_TESTGEN_CMD);
}

static inline void vfe_read_irq_status(struct vfe_irq_thread_msg *out)
{
	uint32_t *temp;

	memset(out, 0, sizeof(struct vfe_irq_thread_msg));

	temp = (uint32_t *)(ctrl->vfebase + VFE_IRQ_STATUS);
	out->vfeIrqStatus = readl(temp);

	temp = (uint32_t *)(ctrl->vfebase + CAMIF_STATUS);
	out->camifStatus = readl(temp);
	writel(0x7, ctrl->vfebase + CAMIF_COMMAND);
	writel(0x3, ctrl->vfebase + CAMIF_COMMAND);
	CDBG("camifStatus  = 0x%x\n", out->camifStatus);

/*
	temp = (uint32_t *)(ctrl->vfebase + VFE_DEMOSAIC_STATUS);
	out->demosaicStatus = readl(temp);

	temp = (uint32_t *)(ctrl->vfebase + VFE_ASF_MAX_EDGE);
	out->asfMaxEdge = readl(temp);

	temp = (uint32_t *)(ctrl->vfebase + VFE_BUS_ENC_Y_WR_PM_STATS_0);
*/

#if 0
	out->pmInfo.encPathPmInfo.yWrPmStats0      = readl(temp++);
	out->pmInfo.encPathPmInfo.yWrPmStats1      = readl(temp++);
	out->pmInfo.encPathPmInfo.cbcrWrPmStats0   = readl(temp++);
	out->pmInfo.encPathPmInfo.cbcrWrPmStats1   = readl(temp++);
	out->pmInfo.viewPathPmInfo.yWrPmStats0     = readl(temp++);
	out->pmInfo.viewPathPmInfo.yWrPmStats1     = readl(temp++);
	out->pmInfo.viewPathPmInfo.cbcrWrPmStats0  = readl(temp++);
	out->pmInfo.viewPathPmInfo.cbcrWrPmStats1  = readl(temp);
#endif /* if 0 Jeff */
}

static struct vfe_interrupt_status
vfe_parse_interrupt_status(uint32_t irqStatusIn)
{
	struct vfe_irqenable hwstat;
	struct vfe_interrupt_status ret;
	boolean temp;

	memset(&hwstat, 0, sizeof(hwstat));
	memset(&ret, 0, sizeof(ret));

	hwstat = *((struct vfe_irqenable *)(&irqStatusIn));

	ret.camifErrorIrq       = hwstat.camifErrorIrq;
	ret.camifSofIrq         = hwstat.camifSofIrq;
	ret.camifEolIrq         = hwstat.camifEolIrq;
	ret.camifEofIrq         = hwstat.camifEofIrq;
	ret.camifEpoch1Irq      = hwstat.camifEpoch1Irq;
	ret.camifEpoch2Irq      = hwstat.camifEpoch2Irq;
	ret.camifOverflowIrq    = hwstat.camifOverflowIrq;
	ret.ceIrq               = hwstat.ceIrq;
	ret.regUpdateIrq        = hwstat.regUpdateIrq;
	ret.resetAckIrq         = hwstat.resetAckIrq;
	ret.encYPingpongIrq     = hwstat.encYPingpongIrq;
	ret.encCbcrPingpongIrq  = hwstat.encCbcrPingpongIrq;
	ret.viewYPingpongIrq    = hwstat.viewYPingpongIrq;
	ret.viewCbcrPingpongIrq = hwstat.viewCbcrPingpongIrq;
	ret.rdPingpongIrq       = hwstat.rdPingpongIrq;
	ret.afPingpongIrq       = hwstat.afPingpongIrq;
	ret.awbPingpongIrq      = hwstat.awbPingpongIrq;
	ret.histPingpongIrq     = hwstat.histPingpongIrq;
	ret.encIrq              = hwstat.encIrq;
	ret.viewIrq             = hwstat.viewIrq;
	ret.busOverflowIrq      = hwstat.busOverflowIrq;
	ret.afOverflowIrq       = hwstat.afOverflowIrq;
	ret.awbOverflowIrq      = hwstat.awbOverflowIrq;
	ret.syncTimer0Irq       = hwstat.syncTimer0Irq;
	ret.syncTimer1Irq       = hwstat.syncTimer1Irq;
	ret.syncTimer2Irq       = hwstat.syncTimer2Irq;
	ret.asyncTimer0Irq      = hwstat.asyncTimer0Irq;
	ret.asyncTimer1Irq      = hwstat.asyncTimer1Irq;
	ret.asyncTimer2Irq      = hwstat.asyncTimer2Irq;
	ret.asyncTimer3Irq      = hwstat.asyncTimer3Irq;
	ret.axiErrorIrq         = hwstat.axiErrorIrq;
	ret.violationIrq        = hwstat.violationIrq;

	/* logic OR of any error bits
	 * although each irq corresponds to a bit, the data type here is a
	 * boolean already. hence use logic operation.
	 */
	temp =
		ret.camifErrorIrq    ||
		ret.camifOverflowIrq ||
		ret.afOverflowIrq    ||
		ret.awbPingpongIrq   ||
		ret.busOverflowIrq   ||
		ret.axiErrorIrq      ||
		ret.violationIrq;

	ret.anyErrorIrqs = temp;

	/* logic OR of any output path bits*/
	temp =
		ret.encYPingpongIrq    ||
		ret.encCbcrPingpongIrq ||
		ret.encIrq;

	ret.anyOutput2PathIrqs = temp;

	temp =
		ret.viewYPingpongIrq    ||
		ret.viewCbcrPingpongIrq ||
		ret.viewIrq;

	ret.anyOutput1PathIrqs = temp;

	ret.anyOutputPathIrqs =
		ret.anyOutput1PathIrqs ||
		ret.anyOutput2PathIrqs;

	/* logic OR of any sync timer bits*/
	temp =
		ret.syncTimer0Irq ||
		ret.syncTimer1Irq ||
		ret.syncTimer2Irq;

	ret.anySyncTimerIrqs = temp;

	/* logic OR of any async timer bits*/
	temp =
		ret.asyncTimer0Irq ||
		ret.asyncTimer1Irq ||
		ret.asyncTimer2Irq ||
		ret.asyncTimer3Irq;

	ret.anyAsyncTimerIrqs = temp;

	/* bool for all interrupts that are not allowed in idle state */
	temp =
		ret.anyErrorIrqs      ||
		ret.anyOutputPathIrqs ||
		ret.anySyncTimerIrqs  ||
		ret.regUpdateIrq      ||
		ret.awbPingpongIrq    ||
		ret.afPingpongIrq     ||
		ret.camifSofIrq       ||
		ret.camifEpoch2Irq    ||
		ret.camifEpoch1Irq;

	ret.anyIrqForActiveStatesOnly =
		temp;

	return ret;
}

static struct vfe_frame_asf_info
vfe_get_asf_frame_info(struct vfe_irq_thread_msg *in)
{
	struct vfe_asf_info     asfInfoTemp;
	struct vfe_frame_asf_info rc;

	memset(&rc, 0, sizeof(rc));
	memset(&asfInfoTemp, 0, sizeof(asfInfoTemp));

	asfInfoTemp =
		*((struct vfe_asf_info *)(&(in->asfMaxEdge)));

	rc.asfHbiCount = asfInfoTemp.HBICount;
	rc.asfMaxEdge  = asfInfoTemp.maxEdge;

	return rc;
}

static struct vfe_frame_bpc_info
vfe_get_demosaic_frame_info(struct vfe_irq_thread_msg *in)
{
	struct vfe_bps_info     bpcInfoTemp;
	struct vfe_frame_bpc_info rc;

	memset(&rc, 0, sizeof(rc));
	memset(&bpcInfoTemp, 0, sizeof(bpcInfoTemp));

	bpcInfoTemp =
		*((struct vfe_bps_info *)(&(in->demosaicStatus)));

	rc.greenDefectPixelCount    =
		bpcInfoTemp.greenBadPixelCount;

	rc.redBlueDefectPixelCount  =
		bpcInfoTemp.RedBlueBadPixelCount;

	return rc;
}

static struct vfe_msg_camif_status
vfe_get_camif_status(struct vfe_irq_thread_msg *in)
{
	struct vfe_camif_stats camifStatusTemp;
	struct vfe_msg_camif_status rc;

	memset(&rc, 0, sizeof(rc));
	memset(&camifStatusTemp, 0, sizeof(camifStatusTemp));

	camifStatusTemp =
		*((struct vfe_camif_stats *)(&(in->camifStatus)));

	rc.camifState = (boolean)camifStatusTemp.camifHalt;
	rc.lineCount  = camifStatusTemp.lineCount;
	rc.pixelCount = camifStatusTemp.pixelCount;

	return rc;
}

static struct vfe_bus_performance_monitor
vfe_get_performance_monitor_data(struct vfe_irq_thread_msg *in)
{
	struct vfe_bus_performance_monitor rc;
	memset(&rc, 0, sizeof(rc));

	rc.encPathPmInfo.yWrPmStats0     =
		in->pmInfo.encPathPmInfo.yWrPmStats0;
	rc.encPathPmInfo.yWrPmStats1     =
		in->pmInfo.encPathPmInfo.yWrPmStats1;
	rc.encPathPmInfo.cbcrWrPmStats0  =
		in->pmInfo.encPathPmInfo.cbcrWrPmStats0;
	rc.encPathPmInfo.cbcrWrPmStats1  =
		in->pmInfo.encPathPmInfo.cbcrWrPmStats1;
	rc.viewPathPmInfo.yWrPmStats0    =
		in->pmInfo.viewPathPmInfo.yWrPmStats0;
	rc.viewPathPmInfo.yWrPmStats1    =
		in->pmInfo.viewPathPmInfo.yWrPmStats1;
	rc.viewPathPmInfo.cbcrWrPmStats0 =
		in->pmInfo.viewPathPmInfo.cbcrWrPmStats0;
	rc.viewPathPmInfo.cbcrWrPmStats1 =
		in->pmInfo.viewPathPmInfo.cbcrWrPmStats1;

	return rc;
}

static void vfe_process_reg_update_irq(void)
{
	CDBG("vfe_process_reg_update_irq: ackPendingFlag is %d\n",
	ctrl->vfeStartAckPendingFlag);
	if (ctrl->vfeStartAckPendingFlag == TRUE) {
		vfe_send_msg_no_payload(VFE_MSG_ID_START_ACK);
		ctrl->vfeStartAckPendingFlag = FALSE;
	} else
		vfe_send_msg_no_payload(VFE_MSG_ID_UPDATE_ACK);
}

static void vfe_process_reset_irq(void)
{
	/* unsigned long flags; */

	/* @todo This is causing issues, need further investigate */
	/* spin_lock_irqsave(&ctrl->state_lock, flags); */
	ctrl->vstate = VFE_STATE_IDLE;
	/* spin_unlock_irqrestore(&ctrl->state_lock, flags); */

	if (ctrl->vfeStopAckPending == TRUE) {
		ctrl->vfeStopAckPending = FALSE;
		vfe_send_msg_no_payload(VFE_MSG_ID_STOP_ACK);
	} else {
		vfe_set_default_reg_values();
		vfe_send_msg_no_payload(VFE_MSG_ID_RESET_ACK);
	}
}

static void vfe_process_pingpong_irq(struct vfe_output_path *in,
	uint8_t fragmentCount)
{
	uint16_t circularIndex;
	uint32_t nextFragmentAddr;

	/* get next fragment address from circular buffer */
	circularIndex    = (in->fragIndex) % (2 * fragmentCount);
	nextFragmentAddr = in->addressBuffer[circularIndex];

	in->fragIndex = circularIndex + 1;

	/* use next fragment to program hardware ping/pong address. */
	if (in->hwCurrentFlag == ping) {
		writel(nextFragmentAddr, in->hwRegPingAddress);
		in->hwCurrentFlag = pong;

	} else {
		writel(nextFragmentAddr, in->hwRegPongAddress);
		in->hwCurrentFlag = ping;
	}
}

static void vfe_send_output2_msg(
	struct vfe_msg_output *pPayload)
{
	/* unsigned long flags; */
	struct vfe_message *msg;

	msg = kzalloc(sizeof(struct vfe_message), GFP_ATOMIC);
	if (!msg)
		return;

	/* fill message with right content. */
	/* @todo This is causing issues, need further investigate */
	/* spin_lock_irqsave(&ctrl->state_lock, flags); */
	if (ctrl->vstate != VFE_STATE_ACTIVE) {
		kfree(msg);
		goto output2_msg_done;
	}

	msg->_d = VFE_MSG_ID_OUTPUT2;

	memcpy(&(msg->_u.msgOutput2),
		(void *)pPayload, sizeof(struct vfe_msg_output));

	vfe_proc_ops(VFE_MSG_ID_OUTPUT2,
		msg, sizeof(struct vfe_message));

	ctrl->encPath.ackPending = TRUE;

	if (!(ctrl->vfeRequestedSnapShotCount <= 3) &&
			(ctrl->vfeOperationMode ==
			 VFE_START_OPERATION_MODE_SNAPSHOT))
		ctrl->encPath.ackPending = TRUE;

output2_msg_done:
	/* spin_unlock_irqrestore(&ctrl->state_lock, flags); */
	return;
}

static void vfe_send_output1_msg(
	struct vfe_msg_output *pPayload)
{
	/* unsigned long flags; */
	struct vfe_message *msg;

	msg = kzalloc(sizeof(struct vfe_message), GFP_ATOMIC);
	if (!msg)
		return;

	/* @todo This is causing issues, need further investigate */
	/* spin_lock_irqsave(&ctrl->state_lock, flags); */
	if (ctrl->vstate != VFE_STATE_ACTIVE) {
		kfree(msg);
		goto output1_msg_done;
	}

	msg->_d = VFE_MSG_ID_OUTPUT1;
	memmove(&(msg->_u),
		(void *)pPayload, sizeof(struct vfe_msg_output));

	vfe_proc_ops(VFE_MSG_ID_OUTPUT1,
		msg, sizeof(struct vfe_message));

	ctrl->viewPath.ackPending = TRUE;

	if (!(ctrl->vfeRequestedSnapShotCount <= 3) &&
			(ctrl->vfeOperationMode ==
			 VFE_START_OPERATION_MODE_SNAPSHOT))
		ctrl->viewPath.ackPending = TRUE;

output1_msg_done:
	/* spin_unlock_irqrestore(&ctrl->state_lock, flags); */
	return;
}

static void vfe_send_output_msg(boolean whichOutputPath,
	uint32_t yPathAddr, uint32_t cbcrPathAddr)
{
	struct vfe_msg_output msgPayload;

	msgPayload.yBuffer = yPathAddr;
	msgPayload.cbcrBuffer = cbcrPathAddr;

	/* asf info is common for both output1 and output2 */
#if 0
	msgPayload.asfInfo.asfHbiCount = ctrl->vfeAsfFrameInfo.asfHbiCount;
	msgPayload.asfInfo.asfMaxEdge = ctrl->vfeAsfFrameInfo.asfMaxEdge;

	/* demosaic info is common for both output1 and output2 */
	msgPayload.bpcInfo.greenDefectPixelCount =
		ctrl->vfeBpcFrameInfo.greenDefectPixelCount;
	msgPayload.bpcInfo.redBlueDefectPixelCount =
		ctrl->vfeBpcFrameInfo.redBlueDefectPixelCount;
#endif /* if 0 */

	/* frame ID is common for both paths. */
	msgPayload.frameCounter = ctrl->vfeFrameId;

	if (whichOutputPath) {
		/* msgPayload.pmData = ctrl->vfePmData.encPathPmInfo; */
		vfe_send_output2_msg(&msgPayload);
	} else {
		/* msgPayload.pmData = ctrl->vfePmData.viewPathPmInfo; */
		vfe_send_output1_msg(&msgPayload);
	}
}

static void vfe_process_frame_done_irq_multi_frag(
	struct vfe_output_path_combo *in)
{
	uint32_t yAddress, cbcrAddress;
	uint16_t idx;
	uint32_t *ptrY;
	uint32_t *ptrCbcr;
	const uint32_t *ptrSrc;
	uint8_t i;

	if (!in->ackPending) {

		idx = (in->currentFrame) * (in->fragCount);

		/* Send output message. */
		yAddress = in->yPath.addressBuffer[idx];
		cbcrAddress = in->cbcrPath.addressBuffer[idx];

		/* copy next frame to current frame. */
		ptrSrc  = in->nextFrameAddrBuf;
		ptrY    = (uint32_t *)&(in->yPath.addressBuffer[idx]);
		ptrCbcr = (uint32_t *)&(in->cbcrPath.addressBuffer[idx]);

		/* Copy Y address */
		for (i = 0; i < in->fragCount; i++)
			*ptrY++ = *ptrSrc++;

		/* Copy Cbcr address */
		for (i = 0; i < in->fragCount; i++)
			*ptrCbcr++ = *ptrSrc++;

		vfe_send_output_msg(in->whichOutputPath, yAddress, cbcrAddress);

	} else {
		if (in->whichOutputPath == 0)
			ctrl->vfeDroppedFrameCounts.output1Count++;

		if (in->whichOutputPath == 1)
			ctrl->vfeDroppedFrameCounts.output2Count++;
	}

	/* toggle current frame. */
	in->currentFrame = in->currentFrame^1;

	if (ctrl->vfeOperationMode)
		in->snapshotPendingCount--;
}

static void vfe_process_frame_done_irq_no_frag_io(
	struct vfe_output_path_combo *in, uint32_t *pNextAddr,
	uint32_t *pdestRenderAddr)
{
	uint32_t busPingPongStatus;
	uint32_t tempAddress;

	/* 1. read hw status register. */
	busPingPongStatus =
		readl(ctrl->vfebase + VFE_BUS_PINGPONG_STATUS);

	CDBG("hardware status is 0x%x\n", busPingPongStatus);

	/* 2. determine ping or pong */
	/* use cbcr status */
	busPingPongStatus = busPingPongStatus & (1<<(in->cbcrStatusBit));

	/* 3. read out address and update address */
	if (busPingPongStatus == 0) {
		/* hw is working on ping, render pong buffer */
		/* a. read out pong address */
		/* read out y address. */
		tempAddress = readl(in->yPath.hwRegPongAddress);

		CDBG("pong 1 addr = 0x%x\n", tempAddress);
		*pdestRenderAddr++ = tempAddress;
		/* read out cbcr address. */
		tempAddress = readl(in->cbcrPath.hwRegPongAddress);

		CDBG("pong 2 addr = 0x%x\n", tempAddress);
		*pdestRenderAddr = tempAddress;

		/* b. update pong address */
		writel(*pNextAddr++, in->yPath.hwRegPongAddress);
		writel(*pNextAddr, in->cbcrPath.hwRegPongAddress);
	} else {
		/* hw is working on pong, render ping buffer */

		/* a. read out ping address */
		tempAddress = readl(in->yPath.hwRegPingAddress);
		CDBG("ping 1 addr = 0x%x\n", tempAddress);
		*pdestRenderAddr++ = tempAddress;
		tempAddress = readl(in->cbcrPath.hwRegPingAddress);

		CDBG("ping 2 addr = 0x%x\n", tempAddress);
		*pdestRenderAddr = tempAddress;

		/* b. update ping address */
		writel(*pNextAddr++, in->yPath.hwRegPingAddress);
		CDBG("NextAddress = 0x%x\n", *pNextAddr);
		writel(*pNextAddr, in->cbcrPath.hwRegPingAddress);
	}
}

static void vfe_process_frame_done_irq_no_frag(
	struct vfe_output_path_combo *in)
{
	uint32_t addressToRender[2];
	static uint32_t fcnt;

	if (fcnt++ < 3)
		return;

	if (!in->ackPending) {
		vfe_process_frame_done_irq_no_frag_io(in,
			in->nextFrameAddrBuf, addressToRender);

		/* use addressToRender to send out message. */
		vfe_send_output_msg(in->whichOutputPath,
				addressToRender[0], addressToRender[1]);

	} else {
		/* ackPending is still there, accumulate dropped frame count.
		 * These count can be read through ioctrl command. */
		CDBG("waiting frame ACK\n");

		if (in->whichOutputPath == 0)
			ctrl->vfeDroppedFrameCounts.output1Count++;

		if (in->whichOutputPath == 1)
			ctrl->vfeDroppedFrameCounts.output2Count++;
	}

	/* in case of multishot when upper layer did not ack, there will still
	 * be a snapshot done msg sent out, even though the number of frames
	 * sent out may be less than the desired number of frames.  snapshot
	 * done msg would be helpful to indicate that vfe pipeline has stop,
	 * and in good known state.
	 */
	if (ctrl->vfeOperationMode)
		in->snapshotPendingCount--;
}

static void vfe_process_output_path_irq(
	struct vfe_interrupt_status *irqstatus)
{
	/* unsigned long flags; */

	/* process the view path interrupts */
	if (irqstatus->anyOutput1PathIrqs) {
		if (ctrl->viewPath.multiFrag) {

			if (irqstatus->viewCbcrPingpongIrq)
				vfe_process_pingpong_irq(
					&(ctrl->viewPath.cbcrPath),
					ctrl->viewPath.fragCount);

			if (irqstatus->viewYPingpongIrq)
				vfe_process_pingpong_irq(
					&(ctrl->viewPath.yPath),
					ctrl->viewPath.fragCount);

			if (irqstatus->viewIrq)
				vfe_process_frame_done_irq_multi_frag(
					&ctrl->viewPath);

		} else {
			/* typical case for no fragment,
			 only frame done irq is enabled. */
			if (irqstatus->viewIrq)
				vfe_process_frame_done_irq_no_frag(
					&ctrl->viewPath);
		}
	}

	/* process the encoder path interrupts */
	if (irqstatus->anyOutput2PathIrqs) {
		if (ctrl->encPath.multiFrag) {
			if (irqstatus->encCbcrPingpongIrq)
				vfe_process_pingpong_irq(
					&(ctrl->encPath.cbcrPath),
					ctrl->encPath.fragCount);

			if (irqstatus->encYPingpongIrq)
				vfe_process_pingpong_irq(&(ctrl->encPath.yPath),
				ctrl->encPath.fragCount);

			if (irqstatus->encIrq)
				vfe_process_frame_done_irq_multi_frag(
					&ctrl->encPath);

		} else {
			if (irqstatus->encIrq)
				vfe_process_frame_done_irq_no_frag(
					&ctrl->encPath);
		}
	}

	if (ctrl->vfeOperationMode) {
		if ((ctrl->encPath.snapshotPendingCount == 0) &&
				(ctrl->viewPath.snapshotPendingCount == 0)) {

			/* @todo This is causing issues, further investigate */
			/* spin_lock_irqsave(&ctrl->state_lock, flags); */
			ctrl->vstate = VFE_STATE_IDLE;
			/* spin_unlock_irqrestore(&ctrl->state_lock, flags); */

			vfe_send_msg_no_payload(VFE_MSG_ID_SNAPSHOT_DONE);
			vfe_prog_hw_testgen_cmd(VFE_TEST_GEN_STOP);
			vfe_pm_stop();
		}
	}
}

static void vfe_do_tasklet(unsigned long data)
{
	unsigned long flags;

	struct isr_queue_cmd *qcmd = NULL;

	CDBG("=== vfe_do_tasklet start === \n");

	spin_lock_irqsave(&ctrl->tasklet_lock, flags);
	qcmd = list_first_entry(&ctrl->tasklet_q,
			struct isr_queue_cmd, list);

	if (!qcmd) {
		spin_unlock_irqrestore(&ctrl->tasklet_lock, flags);
		return;
	}

	list_del(&qcmd->list);
	spin_unlock_irqrestore(&ctrl->tasklet_lock, flags);

	if (qcmd->vfeInterruptStatus.regUpdateIrq) {
		CDBG("irq	regUpdateIrq\n");
		vfe_process_reg_update_irq();
	}

	if (qcmd->vfeInterruptStatus.resetAckIrq) {
		CDBG("irq	resetAckIrq\n");
		vfe_process_reset_irq();
	}

	spin_lock_irqsave(&ctrl->state_lock, flags);
	if (ctrl->vstate != VFE_STATE_ACTIVE) {
		spin_unlock_irqrestore(&ctrl->state_lock, flags);
		return;
	}
	spin_unlock_irqrestore(&ctrl->state_lock, flags);

#if 0
	if (qcmd->vfeInterruptStatus.camifEpoch1Irq)
		vfe_send_msg_no_payload(VFE_MSG_ID_EPOCH1);

	if (qcmd->vfeInterruptStatus.camifEpoch2Irq)
		vfe_send_msg_no_payload(VFE_MSG_ID_EPOCH2);
#endif /* Jeff */

	/* next, check output path related interrupts. */
	if (qcmd->vfeInterruptStatus.anyOutputPathIrqs) {
		CDBG("irq	anyOutputPathIrqs\n");
		vfe_process_output_path_irq(&qcmd->vfeInterruptStatus);
	}

	if (qcmd->vfeInterruptStatus.afPingpongIrq)
		vfe_process_stats_af_irq();

	if (qcmd->vfeInterruptStatus.awbPingpongIrq)
		vfe_process_stats_awb_irq();

	/* any error irqs*/
	if (qcmd->vfeInterruptStatus.anyErrorIrqs)
		vfe_process_error_irq(&qcmd->vfeInterruptStatus);

#if 0
	if (qcmd->vfeInterruptStatus.anySyncTimerIrqs)
		vfe_process_sync_timer_irq();

	if (qcmd->vfeInterruptStatus.anyAsyncTimerIrqs)
		vfe_process_async_timer_irq();
#endif /* Jeff */

	if (qcmd->vfeInterruptStatus.camifSofIrq) {
		CDBG("irq	camifSofIrq\n");
		vfe_process_camif_sof_irq();
	}

	kfree(qcmd);
	CDBG("=== vfe_do_tasklet end === \n");
}

DECLARE_TASKLET(vfe_tasklet, vfe_do_tasklet, 0);

static irqreturn_t vfe_parse_irq(int irq_num, void *data)
{
	unsigned long flags;
	uint32_t irqStatusLocal;
	struct vfe_irq_thread_msg irq;
	struct isr_queue_cmd *qcmd;

	CDBG("vfe_parse_irq\n");

	vfe_read_irq_status(&irq);

	if (irq.vfeIrqStatus == 0) {
		CDBG("vfe_parse_irq: irq.vfeIrqStatus is 0\n");
		return IRQ_HANDLED;
	}

	qcmd = kzalloc(sizeof(struct isr_queue_cmd),
		GFP_ATOMIC);
	if (!qcmd) {
		CDBG("vfe_parse_irq: qcmd malloc failed!\n");
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&ctrl->ack_lock, flags);

	if (ctrl->vfeStopAckPending)
		irqStatusLocal =
			(VFE_IMASK_WHILE_STOPPING & irq.vfeIrqStatus);
	else
		irqStatusLocal =
			((ctrl->vfeImaskPacked | VFE_IMASK_ERROR_ONLY) &
				irq.vfeIrqStatus);

	spin_unlock_irqrestore(&ctrl->ack_lock, flags);

	/* first parse the interrupt status to local data structures. */
	qcmd->vfeInterruptStatus = vfe_parse_interrupt_status(irqStatusLocal);
	qcmd->vfeAsfFrameInfo = vfe_get_asf_frame_info(&irq);
	qcmd->vfeBpcFrameInfo = vfe_get_demosaic_frame_info(&irq);
	qcmd->vfeCamifStatusLocal = vfe_get_camif_status(&irq);
	qcmd->vfePmData = vfe_get_performance_monitor_data(&irq);

	spin_lock_irqsave(&ctrl->tasklet_lock, flags);
	list_add_tail(&qcmd->list, &ctrl->tasklet_q);
	spin_unlock_irqrestore(&ctrl->tasklet_lock, flags);
	tasklet_schedule(&vfe_tasklet);

	/* clear the pending interrupt of the same kind.*/
	writel(irq.vfeIrqStatus, ctrl->vfebase + VFE_IRQ_CLEAR);

	return IRQ_HANDLED;
}

int vfe_cmd_init(struct msm_vfe_callback *presp,
	struct platform_device *pdev, void *sdata)
{
	struct resource	*vfemem, *vfeirq, *vfeio;
	int rc;

	vfemem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!vfemem) {
		CDBG("no mem resource?\n");
		return -ENODEV;
	}

	vfeirq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!vfeirq) {
		CDBG("no irq resource?\n");
		return -ENODEV;
	}

	vfeio = request_mem_region(vfemem->start,
		resource_size(vfemem), pdev->name);
	if (!vfeio) {
		CDBG("VFE region already claimed\n");
		return -EBUSY;
	}

	ctrl =
	kzalloc(sizeof(struct msm_vfe8x_ctrl), GFP_KERNEL);
	if (!ctrl) {
		rc = -ENOMEM;
		goto cmd_init_failed1;
	}

	ctrl->vfeirq  = vfeirq->start;

	ctrl->vfebase =
		ioremap(vfemem->start, (vfemem->end - vfemem->start) + 1);
	if (!ctrl->vfebase) {
		rc = -ENOMEM;
		goto cmd_init_failed2;
	}

	rc = request_irq(ctrl->vfeirq, vfe_parse_irq,
		IRQF_TRIGGER_RISING, "vfe", 0);
	if (rc < 0)
		goto cmd_init_failed2;

	if (presp && presp->vfe_resp)
		ctrl->resp = presp;
	else {
		rc = -EINVAL;
		goto cmd_init_failed3;
	}

	ctrl->extdata =
		kmalloc(sizeof(struct vfe_frame_extra), GFP_KERNEL);
	if (!ctrl->extdata) {
		rc = -ENOMEM;
		goto cmd_init_failed3;
	}

	spin_lock_init(&ctrl->ack_lock);
	spin_lock_init(&ctrl->state_lock);
	spin_lock_init(&ctrl->io_lock);

	ctrl->extlen = sizeof(struct vfe_frame_extra);

	spin_lock_init(&ctrl->tasklet_lock);
	INIT_LIST_HEAD(&ctrl->tasklet_q);

	ctrl->syncdata = sdata;
	return 0;

cmd_init_failed3:
	disable_irq(ctrl->vfeirq);
	free_irq(ctrl->vfeirq, 0);
	iounmap(ctrl->vfebase);
cmd_init_failed2:
	kfree(ctrl);
cmd_init_failed1:
	release_mem_region(vfemem->start, (vfemem->end - vfemem->start) + 1);
	return rc;
}

void vfe_cmd_release(struct platform_device *dev)
{
	struct resource	*mem;

	disable_irq(ctrl->vfeirq);
	free_irq(ctrl->vfeirq, 0);

	iounmap(ctrl->vfebase);
	mem = platform_get_resource(dev, IORESOURCE_MEM, 0);
	release_mem_region(mem->start, (mem->end - mem->start) + 1);

	ctrl->extlen = 0;

	kfree(ctrl->extdata);
	kfree(ctrl);
}

void vfe_stats_af_stop(void)
{
	ctrl->vfeStatsCmdLocal.autoFocusEnable = FALSE;
	ctrl->vfeImaskLocal.afPingpongIrq = FALSE;
}

void vfe_stop(void)
{
	boolean vfeAxiBusy;
	uint32_t vfeAxiStauts;

	/* for reset hw modules, and send msg when reset_irq comes.*/
	ctrl->vfeStopAckPending = TRUE;

	ctrl->vfeStatsPingPongReloadFlag = FALSE;
	vfe_pm_stop();

	/* disable all interrupts.  */
	vfe_program_irq_mask(VFE_DISABLE_ALL_IRQS);

	/* in either continuous or snapshot mode, stop command can be issued
	 * at any time.
	 */
	vfe_camif_stop_immediately();
	vfe_program_axi_cmd(AXI_HALT);
	vfe_prog_hw_testgen_cmd(VFE_TEST_GEN_STOP);

	vfeAxiBusy = TRUE;

	while (vfeAxiBusy) {
		vfeAxiStauts = vfe_read_axi_status();
		if ((vfeAxiStauts & AXI_STATUS_BUSY_MASK) != 0)
			vfeAxiBusy = FALSE;
	}

	vfe_program_axi_cmd(AXI_HALT_CLEAR);

	/* clear all pending interrupts */
	writel(VFE_CLEAR_ALL_IRQS, ctrl->vfebase + VFE_IRQ_CLEAR);

	/* enable reset_ack and async timer interrupt only while stopping
	 * the pipeline.
	 */
	vfe_program_irq_mask(VFE_IMASK_WHILE_STOPPING);

	vfe_program_global_reset_cmd(VFE_RESET_UPON_STOP_CMD);
}

void vfe_update(void)
{
	ctrl->vfeModuleEnableLocal.statsEnable =
		ctrl->vfeStatsCmdLocal.autoFocusEnable |
		ctrl->vfeStatsCmdLocal.axwEnable;

	vfe_reg_module_cfg(&ctrl->vfeModuleEnableLocal);

	vfe_program_stats_cmd(&ctrl->vfeStatsCmdLocal);

	ctrl->vfeImaskPacked = vfe_irq_pack(ctrl->vfeImaskLocal);
	vfe_program_irq_mask(ctrl->vfeImaskPacked);

	if ((ctrl->vfeModuleEnableLocal.statsEnable == TRUE) &&
			(ctrl->vfeStatsPingPongReloadFlag == FALSE)) {
		ctrl->vfeStatsPingPongReloadFlag = TRUE;

		ctrl->vfeBusCmdLocal.statsPingpongReload = TRUE;
		vfe_reg_bus_cmd(&ctrl->vfeBusCmdLocal);
	}

	vfe_program_reg_update_cmd(VFE_REG_UPDATE_TRIGGER);
}

int vfe_rgb_gamma_update(struct vfe_cmd_rgb_gamma_config *in)
{
	int rc = 0;

	ctrl->vfeModuleEnableLocal.rgbLUTEnable = in->enable;

	switch (in->channelSelect) {
	case RGB_GAMMA_CH0_SELECTED:
		ctrl->vfeGammaLutSel.ch0BankSelect ^= 1;
		vfe_write_gamma_table(0,
			ctrl->vfeGammaLutSel.ch0BankSelect, in->table);
		break;

	case RGB_GAMMA_CH1_SELECTED:
		ctrl->vfeGammaLutSel.ch1BankSelect ^= 1;
		vfe_write_gamma_table(1,
			ctrl->vfeGammaLutSel.ch1BankSelect, in->table);
		break;

	case RGB_GAMMA_CH2_SELECTED:
		ctrl->vfeGammaLutSel.ch2BankSelect ^= 1;
		vfe_write_gamma_table(2,
			ctrl->vfeGammaLutSel.ch2BankSelect, in->table);
		break;

	case RGB_GAMMA_CH0_CH1_SELECTED:
		ctrl->vfeGammaLutSel.ch0BankSelect ^= 1;
		ctrl->vfeGammaLutSel.ch1BankSelect ^= 1;
		vfe_write_gamma_table(0, ctrl->vfeGammaLutSel.ch0BankSelect,
			in->table);
		vfe_write_gamma_table(1, ctrl->vfeGammaLutSel.ch1BankSelect,
			in->table);
		break;

	case RGB_GAMMA_CH0_CH2_SELECTED:
		ctrl->vfeGammaLutSel.ch0BankSelect ^= 1;
		ctrl->vfeGammaLutSel.ch2BankSelect ^= 1;
		vfe_write_gamma_table(0, ctrl->vfeGammaLutSel.ch0BankSelect,
			in->table);
		vfe_write_gamma_table(2, ctrl->vfeGammaLutSel.ch2BankSelect,
			in->table);
		break;

	case RGB_GAMMA_CH1_CH2_SELECTED:
		ctrl->vfeGammaLutSel.ch1BankSelect ^= 1;
		ctrl->vfeGammaLutSel.ch2BankSelect ^= 1;
		vfe_write_gamma_table(1, ctrl->vfeGammaLutSel.ch1BankSelect,
			in->table);
		vfe_write_gamma_table(2, ctrl->vfeGammaLutSel.ch2BankSelect,
			in->table);
		break;

	case RGB_GAMMA_CH0_CH1_CH2_SELECTED:
		ctrl->vfeGammaLutSel.ch0BankSelect ^= 1;
		ctrl->vfeGammaLutSel.ch1BankSelect ^= 1;
		ctrl->vfeGammaLutSel.ch2BankSelect ^= 1;
		vfe_write_gamma_table(0, ctrl->vfeGammaLutSel.ch0BankSelect,
			in->table);
		vfe_write_gamma_table(1, ctrl->vfeGammaLutSel.ch1BankSelect,
			in->table);
		vfe_write_gamma_table(2, ctrl->vfeGammaLutSel.ch2BankSelect,
			in->table);
		break;

	default:
		return -EINVAL;
	} /* switch */

	/* update the gammaLutSel register. */
	vfe_program_lut_bank_sel(&ctrl->vfeGammaLutSel);

	return rc;
}

int vfe_rgb_gamma_config(struct vfe_cmd_rgb_gamma_config *in)
{
	int rc = 0;

	ctrl->vfeModuleEnableLocal.rgbLUTEnable = in->enable;

	switch (in->channelSelect) {
	case RGB_GAMMA_CH0_SELECTED:
vfe_write_gamma_table(0, 0, in->table);
break;

	case RGB_GAMMA_CH1_SELECTED:
		vfe_write_gamma_table(1, 0, in->table);
		break;

	case RGB_GAMMA_CH2_SELECTED:
		vfe_write_gamma_table(2, 0, in->table);
		break;

	case RGB_GAMMA_CH0_CH1_SELECTED:
		vfe_write_gamma_table(0, 0, in->table);
		vfe_write_gamma_table(1, 0, in->table);
		break;

	case RGB_GAMMA_CH0_CH2_SELECTED:
		vfe_write_gamma_table(0, 0, in->table);
		vfe_write_gamma_table(2, 0, in->table);
		break;

	case RGB_GAMMA_CH1_CH2_SELECTED:
		vfe_write_gamma_table(1, 0, in->table);
		vfe_write_gamma_table(2, 0, in->table);
		break;

	case RGB_GAMMA_CH0_CH1_CH2_SELECTED:
		vfe_write_gamma_table(0, 0, in->table);
		vfe_write_gamma_table(1, 0, in->table);
		vfe_write_gamma_table(2, 0, in->table);
		break;

	default:
		rc = -EINVAL;
		break;
	} /* switch */

	return rc;
}

void vfe_stats_af_ack(struct vfe_cmd_stats_af_ack *in)
{
	ctrl->afStatsControl.nextFrameAddrBuf = in->nextAFOutputBufferAddr;
	ctrl->afStatsControl.ackPending = FALSE;
}

void vfe_stats_wb_exp_ack(struct vfe_cmd_stats_wb_exp_ack *in)
{
	ctrl->awbStatsControl.nextFrameAddrBuf = in->nextWbExpOutputBufferAddr;
	ctrl->awbStatsControl.ackPending = FALSE;
}

void vfe_output2_ack(struct vfe_cmd_output_ack *in)
{
	const uint32_t *psrc;
	uint32_t *pdest;
	uint8_t i;

	pdest = ctrl->encPath.nextFrameAddrBuf;

	CDBG("output2_ack: ack addr = 0x%x\n", in->ybufaddr[0]);

	psrc = in->ybufaddr;
	for (i = 0; i < ctrl->encPath.fragCount; i++)
		*pdest++ = *psrc++;

	psrc = in->chromabufaddr;
	for (i = 0; i < ctrl->encPath.fragCount; i++)
		*pdest++ = *psrc++;

	ctrl->encPath.ackPending = FALSE;
}

void vfe_output1_ack(struct vfe_cmd_output_ack *in)
{
	const uint32_t *psrc;
	uint32_t *pdest;
	uint8_t i;

	pdest = ctrl->viewPath.nextFrameAddrBuf;

	psrc = in->ybufaddr;
	for (i = 0; i < ctrl->viewPath.fragCount; i++)
		*pdest++ = *psrc++;

	psrc = in->chromabufaddr;
	for (i = 0; i < ctrl->viewPath.fragCount; i++)
		*pdest++ = *psrc++;

	ctrl->viewPath.ackPending = FALSE;
}

void vfe_start(struct vfe_cmd_start *in)
{
	unsigned long flags;
	uint32_t  pmstatus = 0;
	boolean rawmode;
	uint32_t  demperiod = 0;
	uint32_t  demeven = 0;
	uint32_t  demodd = 0;

	/* derived from other commands.  (camif config, axi output config,
	 * etc)
	*/
	struct vfe_cfg hwcfg;
	struct vfe_upsample_cfg chromupcfg;

	CDBG("vfe_start operationMode = %d\n", in->operationMode);

	memset(&hwcfg, 0, sizeof(hwcfg));
	memset(&chromupcfg, 0, sizeof(chromupcfg));

	switch (in->pixel) {
	case VFE_BAYER_RGRGRG:
		demperiod = 1;
		demeven = 0xC9;
		demodd = 0xAC;
		break;

	case VFE_BAYER_GRGRGR:
		demperiod = 1;
		demeven = 0x9C;
		demodd = 0xCA;
		break;

	case VFE_BAYER_BGBGBG:
		demperiod = 1;
		demeven = 0xCA;
		demodd = 0x9C;
		break;

	case VFE_BAYER_GBGBGB:
		demperiod = 1;
		demeven = 0xAC;
		demodd = 0xC9;
		break;

	case VFE_YUV_YCbYCr:
		demperiod = 3;
		demeven = 0x9CAC;
		demodd = 0x9CAC;
		break;

	case VFE_YUV_YCrYCb:
		demperiod = 3;
		demeven = 0xAC9C;
		demodd = 0xAC9C;
		break;

	case VFE_YUV_CbYCrY:
		demperiod = 3;
		demeven = 0xC9CA;
		demodd = 0xC9CA;
		break;

	case VFE_YUV_CrYCbY:
		demperiod = 3;
		demeven = 0xCAC9;
		demodd = 0xCAC9;
		break;

	default:
		return;
	}

	vfe_config_demux(demperiod, demeven, demodd);

	vfe_program_lut_bank_sel(&ctrl->vfeGammaLutSel);

	/* save variables to local. */
	ctrl->vfeOperationMode = in->operationMode;
	if (ctrl->vfeOperationMode ==
			VFE_START_OPERATION_MODE_SNAPSHOT) {
		/* in snapshot mode, initialize snapshot count*/
		ctrl->vfeSnapShotCount = in->snapshotCount;

		/* save the requested count, this is temporarily done, to
		help with HJR / multishot. */
		ctrl->vfeRequestedSnapShotCount = ctrl->vfeSnapShotCount;

		CDBG("requested snapshot count = %d\n", ctrl->vfeSnapShotCount);

		/* Assumption is to have the same pattern and period for both
		paths, if both paths are used. */
		if (ctrl->viewPath.pathEnabled) {
			ctrl->viewPath.snapshotPendingCount =
				in->snapshotCount;

			ctrl->vfeFrameSkipPattern =
				ctrl->vfeFrameSkip.output1Pattern;
			ctrl->vfeFrameSkipPeriod =
				ctrl->vfeFrameSkip.output1Period;
		}

		if (ctrl->encPath.pathEnabled) {
			ctrl->encPath.snapshotPendingCount =
				in->snapshotCount;

			ctrl->vfeFrameSkipPattern =
				ctrl->vfeFrameSkip.output2Pattern;
			ctrl->vfeFrameSkipPeriod =
				ctrl->vfeFrameSkip.output2Period;
		}
	}

	/* enable color conversion for bayer sensor
	if stats enabled, need to do color conversion. */
	if (in->pixel <= VFE_BAYER_GBGBGB)
		ctrl->vfeStatsCmdLocal.colorConversionEnable = TRUE;

	vfe_program_stats_cmd(&ctrl->vfeStatsCmdLocal);

	if (in->pixel >= VFE_YUV_YCbYCr)
		ctrl->vfeModuleEnableLocal.chromaUpsampleEnable = TRUE;

	ctrl->vfeModuleEnableLocal.demuxEnable = TRUE;

	/* if any stats module is enabled, the main bit is enabled. */
	ctrl->vfeModuleEnableLocal.statsEnable =
		ctrl->vfeStatsCmdLocal.autoFocusEnable |
		ctrl->vfeStatsCmdLocal.axwEnable;

	vfe_reg_module_cfg(&ctrl->vfeModuleEnableLocal);

	/* in case of offline processing, do not need to config camif. Having
	 * bus output enabled in camif_config register might confuse the
	 * hardware?
	 */
	if (in->inputSource != VFE_START_INPUT_SOURCE_AXI) {
		vfe_reg_camif_config(&ctrl->vfeCamifConfigLocal);
	} else {
		/* offline processing, enable axi read */
		ctrl->vfeBusConfigLocal.stripeRdPathEn = TRUE;
		ctrl->vfeBusCmdLocal.stripeReload = TRUE;
		ctrl->vfeBusConfigLocal.rawPixelDataSize =
			ctrl->axiInputDataSize;
	}

	vfe_reg_bus_cfg(&ctrl->vfeBusConfigLocal);

	/* directly from start command */
	hwcfg.pixelPattern = in->pixel;
	hwcfg.inputSource = in->inputSource;
	writel(*(uint32_t *)&hwcfg, ctrl->vfebase + VFE_CFG);

	/* regardless module enabled or not, it does not hurt
	 * to program the cositing mode. */
	chromupcfg.chromaCositingForYCbCrInputs =
		in->yuvInputCositingMode;

	writel(*(uint32_t *)&(chromupcfg),
		ctrl->vfebase + VFE_CHROMA_UPSAMPLE_CFG);

	/* MISR to monitor the axi read. */
	writel(0xd8, ctrl->vfebase + VFE_BUS_MISR_MAST_CFG_0);

	/* clear all pending interrupts. */
	writel(VFE_CLEAR_ALL_IRQS, ctrl->vfebase + VFE_IRQ_CLEAR);

	/*  define how composite interrupt work.  */
	ctrl->vfeImaskCompositePacked =
		vfe_irq_composite_pack(ctrl->vfeIrqCompositeMaskLocal);

	vfe_program_irq_composite_mask(ctrl->vfeImaskCompositePacked);

	/*  enable all necessary interrupts.      */
	ctrl->vfeImaskLocal.camifSofIrq  = TRUE;
	ctrl->vfeImaskLocal.regUpdateIrq = TRUE;
	ctrl->vfeImaskLocal.resetAckIrq  = TRUE;

	ctrl->vfeImaskPacked = vfe_irq_pack(ctrl->vfeImaskLocal);
	vfe_program_irq_mask(ctrl->vfeImaskPacked);

	/* enable bus performance monitor */
	vfe_8k_pm_start(&ctrl->vfeBusPmConfigLocal);

	/* trigger vfe reg update */
	ctrl->vfeStartAckPendingFlag = TRUE;

	/* write bus command to trigger reload of ping pong buffer. */
	ctrl->vfeBusCmdLocal.busPingpongReload = TRUE;

	if (ctrl->vfeModuleEnableLocal.statsEnable == TRUE) {
		ctrl->vfeBusCmdLocal.statsPingpongReload = TRUE;
		ctrl->vfeStatsPingPongReloadFlag = TRUE;
	}

	writel(VFE_REG_UPDATE_TRIGGER,
		ctrl->vfebase + VFE_REG_UPDATE_CMD);

	/* program later than the reg update. */
	vfe_reg_bus_cmd(&ctrl->vfeBusCmdLocal);

	if ((in->inputSource ==
			 VFE_START_INPUT_SOURCE_CAMIF) ||
			(in->inputSource ==
			 VFE_START_INPUT_SOURCE_TESTGEN))
		writel(CAMIF_COMMAND_START, ctrl->vfebase + CAMIF_COMMAND);

	/* start test gen if it is enabled */
	if (ctrl->vfeTestGenStartFlag == TRUE) {
		ctrl->vfeTestGenStartFlag = FALSE;
		vfe_prog_hw_testgen_cmd(VFE_TEST_GEN_GO);
	}

	CDBG("ctrl->axiOutputMode = %d\n", ctrl->axiOutputMode);
	if (ctrl->axiOutputMode == VFE_AXI_OUTPUT_MODE_CAMIFToAXIViaOutput2) {
		/* raw dump mode */
		rawmode = TRUE;

		while (rawmode) {
			pmstatus =
				readl(ctrl->vfebase +
					VFE_BUS_ENC_CBCR_WR_PM_STATS_1);

			if ((pmstatus & VFE_PM_BUF_MAX_CNT_MASK) != 0)
				rawmode = FALSE;
		}

		vfe_send_msg_no_payload(VFE_MSG_ID_START_ACK);
		ctrl->vfeStartAckPendingFlag = FALSE;
	}

	spin_lock_irqsave(&ctrl->state_lock, flags);
	ctrl->vstate = VFE_STATE_ACTIVE;
	spin_unlock_irqrestore(&ctrl->state_lock, flags);
}

void vfe_la_update(struct vfe_cmd_la_config *in)
{
	int16_t *pTable;
	enum VFE_DMI_RAM_SEL dmiRamSel;
	int i;

	pTable = in->table;
	ctrl->vfeModuleEnableLocal.lumaAdaptationEnable = in->enable;

	/* toggle the bank to be used. */
	ctrl->vfeLaBankSel ^= 1;

	if (ctrl->vfeLaBankSel == 0)
		dmiRamSel = LUMA_ADAPT_LUT_RAM_BANK0;
	else
		dmiRamSel = LUMA_ADAPT_LUT_RAM_BANK1;

	/* configure the DMI_CFG to select right sram */
	vfe_program_dmi_cfg(dmiRamSel);

	for (i = 0; i < VFE_LA_TABLE_LENGTH; i++) {
		writel((uint32_t)(*pTable), ctrl->vfebase + VFE_DMI_DATA_LO);
		pTable++;
	}

	/* After DMI transfer, to make it safe, need to set
	 * the DMI_CFG to unselect any SRAM */
	writel(VFE_DMI_CFG_DEFAULT, ctrl->vfebase + VFE_DMI_CFG);
	writel(ctrl->vfeLaBankSel, ctrl->vfebase + VFE_LA_CFG);
}

void vfe_la_config(struct vfe_cmd_la_config *in)
{
	uint16_t i;
	int16_t  *pTable;
	enum VFE_DMI_RAM_SEL dmiRamSel;

	pTable = in->table;
	ctrl->vfeModuleEnableLocal.lumaAdaptationEnable = in->enable;

	if (ctrl->vfeLaBankSel == 0)
		dmiRamSel = LUMA_ADAPT_LUT_RAM_BANK0;
	else
		dmiRamSel = LUMA_ADAPT_LUT_RAM_BANK1;

	/* configure the DMI_CFG to select right sram */
	vfe_program_dmi_cfg(dmiRamSel);

	for (i = 0; i < VFE_LA_TABLE_LENGTH; i++) {
		writel((uint32_t)(*pTable), ctrl->vfebase + VFE_DMI_DATA_LO);
		pTable++;
	}

	/* After DMI transfer, to make it safe, need to set the
	 * DMI_CFG to unselect any SRAM */
	writel(VFE_DMI_CFG_DEFAULT, ctrl->vfebase + VFE_DMI_CFG);

	/* can only be bank 0 or bank 1 for now. */
	writel(ctrl->vfeLaBankSel, ctrl->vfebase + VFE_LA_CFG);
	CDBG("VFE Luma adaptation bank selection is 0x%x\n",
			 *(uint32_t *)&ctrl->vfeLaBankSel);
}

void vfe_test_gen_start(struct vfe_cmd_test_gen_start *in)
{
	struct VFE_TestGen_ConfigCmdType cmd;

	memset(&cmd, 0, sizeof(cmd));

	cmd.numFrame              = in->numFrame;
	cmd.pixelDataSelect       = in->pixelDataSelect;
	cmd.systematicDataSelect  = in->systematicDataSelect;
	cmd.pixelDataSize         = (uint32_t)in->pixelDataSize;
	cmd.hsyncEdge             = (uint32_t)in->hsyncEdge;
	cmd.vsyncEdge             = (uint32_t)in->vsyncEdge;
	cmd.imageWidth            = in->imageWidth;
	cmd.imageHeight           = in->imageHeight;
	cmd.sofOffset             = in->startOfFrameOffset;
	cmd.eofNOffset            = in->endOfFrameNOffset;
	cmd.solOffset             = in->startOfLineOffset;
	cmd.eolNOffset            = in->endOfLineNOffset;
	cmd.hBlankInterval        = in->hbi;
	cmd.vBlankInterval        = in->vbl;
	cmd.vBlankIntervalEnable  = in->vblEnable;
	cmd.sofDummy              = in->startOfFrameDummyLine;
	cmd.eofDummy              = in->endOfFrameDummyLine;
	cmd.unicolorBarSelect     = in->unicolorBarSelect;
	cmd.unicolorBarEnable     = in->unicolorBarEnable;
	cmd.splitEnable           = in->colorBarsSplitEnable;
	cmd.pixelPattern          = (uint32_t)in->colorBarsPixelPattern;
	cmd.rotatePeriod          = in->colorBarsRotatePeriod;
	cmd.randomSeed            = in->testGenRandomSeed;

	vfe_prog_hw(ctrl->vfebase + VFE_HW_TESTGEN_CFG,
		(uint32_t *) &cmd, sizeof(cmd));
}

void vfe_frame_skip_update(struct vfe_cmd_frame_skip_update *in)
{
	struct VFE_FRAME_SKIP_UpdateCmdType cmd;

	cmd.yPattern    = in->output1Pattern;
	cmd.cbcrPattern = in->output1Pattern;
	vfe_prog_hw(ctrl->vfebase + VFE_FRAMEDROP_VIEW_Y_PATTERN,
		(uint32_t *)&cmd, sizeof(cmd));

	cmd.yPattern    = in->output2Pattern;
	cmd.cbcrPattern = in->output2Pattern;
	vfe_prog_hw(ctrl->vfebase + VFE_FRAMEDROP_ENC_Y_PATTERN,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_frame_skip_config(struct vfe_cmd_frame_skip_config *in)
{
	struct vfe_frame_skip_cfg cmd;
	memset(&cmd, 0, sizeof(cmd));

	ctrl->vfeFrameSkip = *in;

	cmd.output2YPeriod     = in->output2Period;
	cmd.output2CbCrPeriod  = in->output2Period;
	cmd.output2YPattern    = in->output2Pattern;
	cmd.output2CbCrPattern = in->output2Pattern;
	cmd.output1YPeriod     = in->output1Period;
	cmd.output1CbCrPeriod  = in->output1Period;
	cmd.output1YPattern    = in->output1Pattern;
	cmd.output1CbCrPattern = in->output1Pattern;

	vfe_prog_hw(ctrl->vfebase + VFE_FRAMEDROP_ENC_Y_CFG,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_output_clamp_config(struct vfe_cmd_output_clamp_config *in)
{
	struct vfe_output_clamp_cfg cmd;
	memset(&cmd, 0, sizeof(cmd));

	cmd.yChanMax  = in->maxCh0;
	cmd.cbChanMax = in->maxCh1;
	cmd.crChanMax = in->maxCh2;

	cmd.yChanMin  = in->minCh0;
	cmd.cbChanMin = in->minCh1;
	cmd.crChanMin = in->minCh2;

	vfe_prog_hw(ctrl->vfebase + VFE_CLAMP_MAX_CFG, (uint32_t *)&cmd,
		sizeof(cmd));
}

void vfe_camif_frame_update(struct vfe_cmds_camif_frame *in)
{
	struct vfe_camifframe_update cmd;

	memset(&cmd, 0, sizeof(cmd));

	cmd.pixelsPerLine = in->pixelsPerLine;
	cmd.linesPerFrame = in->linesPerFrame;

	vfe_prog_hw(ctrl->vfebase + CAMIF_FRAME_CONFIG, (uint32_t *)&cmd,
		sizeof(cmd));
}

void vfe_color_correction_config(
	struct vfe_cmd_color_correction_config *in)
{
	struct vfe_color_correction_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));
	ctrl->vfeModuleEnableLocal.colorCorrectionEnable = in->enable;

	cmd.c0 = in->C0;
	cmd.c1 = in->C1;
	cmd.c2 = in->C2;
	cmd.c3 = in->C3;
	cmd.c4 = in->C4;
	cmd.c5 = in->C5;
	cmd.c6 = in->C6;
	cmd.c7 = in->C7;
	cmd.c8 = in->C8;

	cmd.k0 = in->K0;
	cmd.k1 = in->K1;
	cmd.k2 = in->K2;

	cmd.coefQFactor = in->coefQFactor;

	vfe_prog_hw(ctrl->vfebase + VFE_COLOR_CORRECT_COEFF_0,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_demosaic_abf_update(struct vfe_cmd_demosaic_abf_update *in)
{
struct vfe_demosaic_cfg cmd;
	struct vfe_demosaic_abf_cfg cmdabf;
	uint32_t temp;

	memset(&cmd, 0, sizeof(cmd));
	temp = readl(ctrl->vfebase + VFE_DEMOSAIC_CFG);

	cmd = *((struct vfe_demosaic_cfg *)(&temp));
	cmd.abfEnable       = in->abfUpdate.enable;
	cmd.forceAbfOn      = in->abfUpdate.forceOn;
	cmd.abfShift        = in->abfUpdate.shift;
	vfe_prog_hw(ctrl->vfebase + VFE_DEMOSAIC_CFG,
		(uint32_t *)&cmd, sizeof(cmd));

	cmdabf.lpThreshold  = in->abfUpdate.lpThreshold;
	cmdabf.ratio        = in->abfUpdate.ratio;
	cmdabf.minValue     = in->abfUpdate.min;
	cmdabf.maxValue     = in->abfUpdate.max;
	vfe_prog_hw(ctrl->vfebase + VFE_DEMOSAIC_ABF_CFG_0,
		(uint32_t *)&cmdabf, sizeof(cmdabf));
}

void vfe_demosaic_bpc_update(struct vfe_cmd_demosaic_bpc_update *in)
{
	struct vfe_demosaic_cfg cmd;
	struct vfe_demosaic_bpc_cfg cmdbpc;
	uint32_t temp;

	memset(&cmd, 0, sizeof(cmd));

	temp = readl(ctrl->vfebase + VFE_DEMOSAIC_CFG);

	cmd = *((struct vfe_demosaic_cfg *)(&temp));
	cmd.badPixelCorrEnable = in->bpcUpdate.enable;
	cmd.fminThreshold      = in->bpcUpdate.fminThreshold;
	cmd.fmaxThreshold      = in->bpcUpdate.fmaxThreshold;

	vfe_prog_hw(ctrl->vfebase + VFE_DEMOSAIC_CFG,
		(uint32_t *)&cmd, sizeof(cmd));

	cmdbpc.blueDiffThreshold  = in->bpcUpdate.blueDiffThreshold;
	cmdbpc.redDiffThreshold   = in->bpcUpdate.redDiffThreshold;
	cmdbpc.greenDiffThreshold = in->bpcUpdate.greenDiffThreshold;

	vfe_prog_hw(ctrl->vfebase + VFE_DEMOSAIC_BPC_CFG_0,
		(uint32_t *)&cmdbpc, sizeof(cmdbpc));
}

void vfe_demosaic_config(struct vfe_cmd_demosaic_config *in)
{
	struct vfe_demosaic_cfg cmd;
	struct vfe_demosaic_bpc_cfg cmd_bpc;
	struct vfe_demosaic_abf_cfg cmd_abf;

	memset(&cmd, 0, sizeof(cmd));
	memset(&cmd_bpc, 0, sizeof(cmd_bpc));
	memset(&cmd_abf, 0, sizeof(cmd_abf));

	ctrl->vfeModuleEnableLocal.demosaicEnable = in->enable;

	cmd.abfEnable          = in->abfConfig.enable;
	cmd.badPixelCorrEnable = in->bpcConfig.enable;
	cmd.forceAbfOn         = in->abfConfig.forceOn;
	cmd.abfShift           = in->abfConfig.shift;
	cmd.fminThreshold      = in->bpcConfig.fminThreshold;
	cmd.fmaxThreshold      = in->bpcConfig.fmaxThreshold;
	cmd.slopeShift         = in->slopeShift;

	vfe_prog_hw(ctrl->vfebase + VFE_DEMOSAIC_CFG,
		(uint32_t *)&cmd, sizeof(cmd));

	cmd_abf.lpThreshold = in->abfConfig.lpThreshold;
	cmd_abf.ratio       = in->abfConfig.ratio;
	cmd_abf.minValue    = in->abfConfig.min;
	cmd_abf.maxValue    = in->abfConfig.max;

	vfe_prog_hw(ctrl->vfebase + VFE_DEMOSAIC_ABF_CFG_0,
		(uint32_t *)&cmd_abf, sizeof(cmd_abf));

	cmd_bpc.blueDiffThreshold   = in->bpcConfig.blueDiffThreshold;
	cmd_bpc.redDiffThreshold    = in->bpcConfig.redDiffThreshold;
	cmd_bpc.greenDiffThreshold  = in->bpcConfig.greenDiffThreshold;

	vfe_prog_hw(ctrl->vfebase + VFE_DEMOSAIC_BPC_CFG_0,
		(uint32_t *)&cmd_bpc, sizeof(cmd_bpc));
}

void vfe_demux_channel_gain_update(
	struct vfe_cmd_demux_channel_gain_config *in)
{
	struct vfe_demux_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));

	cmd.ch0EvenGain  = in->ch0EvenGain;
	cmd.ch0OddGain   = in->ch0OddGain;
	cmd.ch1Gain      = in->ch1Gain;
	cmd.ch2Gain      = in->ch2Gain;

	vfe_prog_hw(ctrl->vfebase + VFE_DEMUX_GAIN_0,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_demux_channel_gain_config(
	struct vfe_cmd_demux_channel_gain_config *in)
{
	struct vfe_demux_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));

	cmd.ch0EvenGain = in->ch0EvenGain;
	cmd.ch0OddGain  = in->ch0OddGain;
	cmd.ch1Gain     = in->ch1Gain;
	cmd.ch2Gain     = in->ch2Gain;

	vfe_prog_hw(ctrl->vfebase + VFE_DEMUX_GAIN_0,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_black_level_update(struct vfe_cmd_black_level_config *in)
{
	struct vfe_blacklevel_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));
	ctrl->vfeModuleEnableLocal.blackLevelCorrectionEnable = in->enable;

	cmd.evenEvenAdjustment = in->evenEvenAdjustment;
	cmd.evenOddAdjustment  = in->evenOddAdjustment;
	cmd.oddEvenAdjustment  = in->oddEvenAdjustment;
	cmd.oddOddAdjustment   = in->oddOddAdjustment;

	vfe_prog_hw(ctrl->vfebase + VFE_BLACK_EVEN_EVEN_VALUE,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_black_level_config(struct vfe_cmd_black_level_config *in)
{
	struct vfe_blacklevel_cfg cmd;
	memset(&cmd, 0, sizeof(cmd));

	ctrl->vfeModuleEnableLocal.blackLevelCorrectionEnable = in->enable;

	cmd.evenEvenAdjustment = in->evenEvenAdjustment;
	cmd.evenOddAdjustment  = in->evenOddAdjustment;
	cmd.oddEvenAdjustment  = in->oddEvenAdjustment;
	cmd.oddOddAdjustment   = in->oddOddAdjustment;

	vfe_prog_hw(ctrl->vfebase + VFE_BLACK_EVEN_EVEN_VALUE,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_asf_update(struct vfe_cmd_asf_update *in)
{
	struct vfe_asf_update cmd;
	memset(&cmd, 0, sizeof(cmd));

	ctrl->vfeModuleEnableLocal.asfEnable = in->enable;

	cmd.smoothEnable     = in->smoothFilterEnabled;
	cmd.sharpMode        = in->sharpMode;
	cmd.smoothCoeff1     = in->smoothCoefCenter;
	cmd.smoothCoeff0     = in->smoothCoefSurr;
	cmd.cropEnable       = in->cropEnable;
	cmd.sharpThresholdE1 = in->sharpThreshE1;
	cmd.sharpDegreeK1    = in->sharpK1;
	cmd.sharpDegreeK2    = in->sharpK2;
	cmd.normalizeFactor  = in->normalizeFactor;
	cmd.sharpThresholdE2 = in->sharpThreshE2;
	cmd.sharpThresholdE3 = in->sharpThreshE3;
	cmd.sharpThresholdE4 = in->sharpThreshE4;
	cmd.sharpThresholdE5 = in->sharpThreshE5;
	cmd.F1Coeff0         = in->filter1Coefficients[0];
	cmd.F1Coeff1         = in->filter1Coefficients[1];
	cmd.F1Coeff2         = in->filter1Coefficients[2];
	cmd.F1Coeff3         = in->filter1Coefficients[3];
	cmd.F1Coeff4         = in->filter1Coefficients[4];
	cmd.F1Coeff5         = in->filter1Coefficients[5];
	cmd.F1Coeff6         = in->filter1Coefficients[6];
	cmd.F1Coeff7         = in->filter1Coefficients[7];
	cmd.F1Coeff8         = in->filter1Coefficients[8];
	cmd.F2Coeff0         = in->filter2Coefficients[0];
	cmd.F2Coeff1         = in->filter2Coefficients[1];
	cmd.F2Coeff2         = in->filter2Coefficients[2];
	cmd.F2Coeff3         = in->filter2Coefficients[3];
	cmd.F2Coeff4         = in->filter2Coefficients[4];
	cmd.F2Coeff5         = in->filter2Coefficients[5];
	cmd.F2Coeff6         = in->filter2Coefficients[6];
	cmd.F2Coeff7         = in->filter2Coefficients[7];
	cmd.F2Coeff8         = in->filter2Coefficients[8];

	vfe_prog_hw(ctrl->vfebase + VFE_ASF_CFG,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_asf_config(struct vfe_cmd_asf_config *in)
{
	struct vfe_asf_update     cmd;
	struct vfe_asfcrop_cfg cmd2;

	memset(&cmd, 0, sizeof(cmd));
	memset(&cmd2, 0, sizeof(cmd2));

	ctrl->vfeModuleEnableLocal.asfEnable = in->enable;

	cmd.smoothEnable       = in->smoothFilterEnabled;
	cmd.sharpMode          = in->sharpMode;
	cmd.smoothCoeff0       = in->smoothCoefCenter;
	cmd.smoothCoeff1       = in->smoothCoefSurr;
	cmd.cropEnable         = in->cropEnable;
	cmd.sharpThresholdE1   = in->sharpThreshE1;
	cmd.sharpDegreeK1      = in->sharpK1;
	cmd.sharpDegreeK2      = in->sharpK2;
	cmd.normalizeFactor    = in->normalizeFactor;
	cmd.sharpThresholdE2   = in->sharpThreshE2;
	cmd.sharpThresholdE3   = in->sharpThreshE3;
	cmd.sharpThresholdE4   = in->sharpThreshE4;
	cmd.sharpThresholdE5   = in->sharpThreshE5;
	cmd.F1Coeff0           = in->filter1Coefficients[0];
	cmd.F1Coeff1           = in->filter1Coefficients[1];
	cmd.F1Coeff2           = in->filter1Coefficients[2];
	cmd.F1Coeff3           = in->filter1Coefficients[3];
	cmd.F1Coeff4           = in->filter1Coefficients[4];
	cmd.F1Coeff5           = in->filter1Coefficients[5];
	cmd.F1Coeff6           = in->filter1Coefficients[6];
	cmd.F1Coeff7           = in->filter1Coefficients[7];
	cmd.F1Coeff8           = in->filter1Coefficients[8];
	cmd.F2Coeff0           = in->filter2Coefficients[0];
	cmd.F2Coeff1           = in->filter2Coefficients[1];
	cmd.F2Coeff2           = in->filter2Coefficients[2];
	cmd.F2Coeff3           = in->filter2Coefficients[3];
	cmd.F2Coeff4           = in->filter2Coefficients[4];
	cmd.F2Coeff5           = in->filter2Coefficients[5];
	cmd.F2Coeff6           = in->filter2Coefficients[6];
	cmd.F2Coeff7           = in->filter2Coefficients[7];
	cmd.F2Coeff8           = in->filter2Coefficients[8];

	vfe_prog_hw(ctrl->vfebase + VFE_ASF_CFG,
		(uint32_t *)&cmd, sizeof(cmd));

	cmd2.firstLine  = in->cropFirstLine;
	cmd2.lastLine   = in->cropLastLine;
	cmd2.firstPixel = in->cropFirstPixel;
	cmd2.lastPixel  = in->cropLastPixel;

	vfe_prog_hw(ctrl->vfebase + VFE_ASF_CROP_WIDTH_CFG,
		(uint32_t *)&cmd2, sizeof(cmd2));
}

void vfe_white_balance_config(struct vfe_cmd_white_balance_config *in)
{
	struct vfe_wb_cfg cmd;
	memset(&cmd, 0, sizeof(cmd));

	ctrl->vfeModuleEnableLocal.whiteBalanceEnable =
		in->enable;

	cmd.ch0Gain = in->ch0Gain;
	cmd.ch1Gain = in->ch1Gain;
	cmd.ch2Gain = in->ch2Gain;

	vfe_prog_hw(ctrl->vfebase + VFE_WB_CFG,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_chroma_sup_config(struct vfe_cmd_chroma_suppression_config *in)
{
	struct vfe_chroma_suppress_cfg cmd;
	memset(&cmd, 0, sizeof(cmd));

	ctrl->vfeModuleEnableLocal.chromaSuppressionEnable = in->enable;

	cmd.m1  = in->m1;
	cmd.m3  = in->m3;
	cmd.n1  = in->n1;
	cmd.n3  = in->n3;
	cmd.mm1 = in->mm1;
	cmd.nn1 = in->nn1;

	vfe_prog_hw(ctrl->vfebase + VFE_CHROMA_SUPPRESS_CFG_0,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_roll_off_config(struct vfe_cmd_roll_off_config *in)
{
	struct vfe_rolloff_cfg cmd;
	memset(&cmd, 0, sizeof(cmd));

	ctrl->vfeModuleEnableLocal.lensRollOffEnable = in->enable;

	cmd.gridWidth   = in->gridWidth;
	cmd.gridHeight  = in->gridHeight;
	cmd.yDelta      = in->yDelta;
	cmd.gridX       = in->gridXIndex;
	cmd.gridY       = in->gridYIndex;
	cmd.pixelX      = in->gridPixelXIndex;
	cmd.pixelY      = in->gridPixelYIndex;
	cmd.yDeltaAccum = in->yDeltaAccum;

	vfe_prog_hw(ctrl->vfebase + VFE_ROLLOFF_CFG_0,
		(uint32_t *)&cmd, sizeof(cmd));

	vfe_write_lens_roll_off_table(in);
}

void vfe_chroma_subsample_config(
	struct vfe_cmd_chroma_subsample_config *in)
{
	struct vfe_chromasubsample_cfg cmd;
	memset(&cmd, 0, sizeof(cmd));

	ctrl->vfeModuleEnableLocal.chromaSubsampleEnable = in->enable;

	cmd.hCositedPhase       = in->hCositedPhase;
	cmd.vCositedPhase       = in->vCositedPhase;
	cmd.hCosited            = in->hCosited;
	cmd.vCosited            = in->vCosited;
	cmd.hsubSampleEnable    = in->hsubSampleEnable;
	cmd.vsubSampleEnable    = in->vsubSampleEnable;
	cmd.cropEnable          = in->cropEnable;
	cmd.cropWidthLastPixel  = in->cropWidthLastPixel;
	cmd.cropWidthFirstPixel = in->cropWidthFirstPixel;
	cmd.cropHeightLastLine  = in->cropHeightLastLine;
	cmd.cropHeightFirstLine = in->cropHeightFirstLine;

	vfe_prog_hw(ctrl->vfebase + VFE_CHROMA_SUBSAMPLE_CFG,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_chroma_enhan_config(struct vfe_cmd_chroma_enhan_config *in)
{
	struct vfe_chroma_enhance_cfg cmd;
	struct vfe_color_convert_cfg cmd2;

	memset(&cmd, 0, sizeof(cmd));
	memset(&cmd2, 0, sizeof(cmd2));

	ctrl->vfeModuleEnableLocal.chromaEnhanEnable = in->enable;

	cmd.ap             = in->ap;
	cmd.am             = in->am;
	cmd.bp             = in->bp;
	cmd.bm             = in->bm;
	cmd.cp             = in->cp;
	cmd.cm             = in->cm;
	cmd.dp             = in->dp;
	cmd.dm             = in->dm;
	cmd.kcb            = in->kcb;
	cmd.kcr            = in->kcr;

	cmd2.v0            = in->RGBtoYConversionV0;
	cmd2.v1            = in->RGBtoYConversionV1;
	cmd2.v2            = in->RGBtoYConversionV2;
	cmd2.ConvertOffset = in->RGBtoYConversionOffset;

	vfe_prog_hw(ctrl->vfebase + VFE_CHROMA_ENHAN_A,
		(uint32_t *)&cmd, sizeof(cmd));

	vfe_prog_hw(ctrl->vfebase + VFE_COLOR_CONVERT_COEFF_0,
		(uint32_t *)&cmd2, sizeof(cmd2));
}

void vfe_scaler2cbcr_config(struct vfe_cmd_scaler2_config *in)
{
	struct vfe_scaler2_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));

	ctrl->vfeModuleEnableLocal.scaler2CbcrEnable = in->enable;

	cmd.hEnable              = in->hconfig.enable;
	cmd.vEnable              = in->vconfig.enable;
	cmd.inWidth              = in->hconfig.inputSize;
	cmd.outWidth             = in->hconfig.outputSize;
	cmd.horizPhaseMult       = in->hconfig.phaseMultiplicationFactor;
	cmd.horizInterResolution = in->hconfig.interpolationResolution;
	cmd.inHeight             = in->vconfig.inputSize;
	cmd.outHeight            = in->vconfig.outputSize;
	cmd.vertPhaseMult        = in->vconfig.phaseMultiplicationFactor;
	cmd.vertInterResolution  = in->vconfig.interpolationResolution;

	vfe_prog_hw(ctrl->vfebase + VFE_SCALE_CBCR_CFG,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_scaler2y_config(struct vfe_cmd_scaler2_config *in)
{
	struct vfe_scaler2_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));

	ctrl->vfeModuleEnableLocal.scaler2YEnable = in->enable;

	cmd.hEnable               = in->hconfig.enable;
	cmd.vEnable               = in->vconfig.enable;
	cmd.inWidth               = in->hconfig.inputSize;
	cmd.outWidth              = in->hconfig.outputSize;
	cmd.horizPhaseMult        = in->hconfig.phaseMultiplicationFactor;
	cmd.horizInterResolution  = in->hconfig.interpolationResolution;
	cmd.inHeight              = in->vconfig.inputSize;
	cmd.outHeight             = in->vconfig.outputSize;
	cmd.vertPhaseMult         = in->vconfig.phaseMultiplicationFactor;
	cmd.vertInterResolution   = in->vconfig.interpolationResolution;

	vfe_prog_hw(ctrl->vfebase + VFE_SCALE_Y_CFG,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_main_scaler_config(struct vfe_cmd_main_scaler_config *in)
{
	struct vfe_main_scaler_cfg cmd;

	memset(&cmd, 0, sizeof(cmd));

	ctrl->vfeModuleEnableLocal.mainScalerEnable = in->enable;

	cmd.hEnable              = in->hconfig.enable;
	cmd.vEnable              = in->vconfig.enable;
	cmd.inWidth              = in->hconfig.inputSize;
	cmd.outWidth             = in->hconfig.outputSize;
	cmd.horizPhaseMult       = in->hconfig.phaseMultiplicationFactor;
	cmd.horizInterResolution = in->hconfig.interpolationResolution;
	cmd.horizMNInit          = in->MNInitH.MNCounterInit;
	cmd.horizPhaseInit       = in->MNInitH.phaseInit;
	cmd.inHeight             = in->vconfig.inputSize;
	cmd.outHeight            = in->vconfig.outputSize;
	cmd.vertPhaseMult        = in->vconfig.phaseMultiplicationFactor;
	cmd.vertInterResolution  = in->vconfig.interpolationResolution;
	cmd.vertMNInit           = in->MNInitV.MNCounterInit;
	cmd.vertPhaseInit        = in->MNInitV.phaseInit;

	vfe_prog_hw(ctrl->vfebase + VFE_SCALE_CFG,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_stats_wb_exp_stop(void)
{
	ctrl->vfeStatsCmdLocal.axwEnable = FALSE;
	ctrl->vfeImaskLocal.awbPingpongIrq = FALSE;
}

void vfe_stats_update_wb_exp(struct vfe_cmd_stats_wb_exp_update *in)
{
	struct vfe_statsawb_update   cmd;
	struct vfe_statsawbae_update cmd2;

	memset(&cmd, 0, sizeof(cmd));
	memset(&cmd2, 0, sizeof(cmd2));

	cmd.m1  = in->awbMCFG[0];
	cmd.m2  = in->awbMCFG[1];
	cmd.m3  = in->awbMCFG[2];
	cmd.m4  = in->awbMCFG[3];
	cmd.c1  = in->awbCCFG[0];
	cmd.c2  = in->awbCCFG[1];
	cmd.c3  = in->awbCCFG[2];
	cmd.c4  = in->awbCCFG[3];
	vfe_prog_hw(ctrl->vfebase + VFE_STATS_AWB_MCFG,
		(uint32_t *)&cmd, sizeof(cmd));

	cmd2.aeRegionCfg    = in->wbExpRegions;
	cmd2.aeSubregionCfg = in->wbExpSubRegion;
	cmd2.awbYMin        = in->awbYMin;
	cmd2.awbYMax        = in->awbYMax;
	vfe_prog_hw(ctrl->vfebase + VFE_STATS_AWBAE_CFG,
		(uint32_t *)&cmd2, sizeof(cmd2));
}

void vfe_stats_update_af(struct vfe_cmd_stats_af_update *in)
{
	struct vfe_statsaf_update cmd;
	memset(&cmd, 0, sizeof(cmd));

	cmd.windowVOffset = in->windowVOffset;
	cmd.windowHOffset = in->windowHOffset;
	cmd.windowMode    = in->windowMode;
	cmd.windowHeight  = in->windowHeight;
	cmd.windowWidth   = in->windowWidth;

	vfe_prog_hw(ctrl->vfebase + VFE_STATS_AF_CFG,
		(uint32_t *)&cmd, sizeof(cmd));
}

void vfe_stats_start_wb_exp(struct vfe_cmd_stats_wb_exp_start *in)
{
	struct vfe_statsawb_update   cmd;
	struct vfe_statsawbae_update cmd2;
	struct vfe_statsaxw_hdr_cfg  cmd3;

	ctrl->vfeStatsCmdLocal.axwEnable   =  in->enable;
	ctrl->vfeImaskLocal.awbPingpongIrq = TRUE;

	memset(&cmd, 0, sizeof(cmd));
	memset(&cmd2, 0, sizeof(cmd2));
	memset(&cmd3, 0, sizeof(cmd3));

	cmd.m1  = in->awbMCFG[0];
	cmd.m2  = in->awbMCFG[1];
	cmd.m3  = in->awbMCFG[2];
	cmd.m4  = in->awbMCFG[3];
	cmd.c1  = in->awbCCFG[0];
	cmd.c2  = in->awbCCFG[1];
	cmd.c3  = in->awbCCFG[2];
	cmd.c4  = in->awbCCFG[3];
	vfe_prog_hw(ctrl->vfebase + VFE_STATS_AWB_MCFG,
		(uint32_t *)&cmd, sizeof(cmd));

	cmd2.aeRegionCfg     = in->wbExpRegions;
	cmd2.aeSubregionCfg  = in->wbExpSubRegion;
	cmd2.awbYMin         = in->awbYMin;
	cmd2.awbYMax         = in->awbYMax;
	vfe_prog_hw(ctrl->vfebase + VFE_STATS_AWBAE_CFG,
		(uint32_t *)&cmd2, sizeof(cmd2));

	cmd3.axwHeader       = in->axwHeader;
	vfe_prog_hw(ctrl->vfebase + VFE_STATS_AXW_HEADER,
		(uint32_t *)&cmd3, sizeof(cmd3));
}

void vfe_stats_start_af(struct vfe_cmd_stats_af_start *in)
{
	struct vfe_statsaf_update cmd;
	struct vfe_statsaf_cfg    cmd2;

	memset(&cmd, 0, sizeof(cmd));
	memset(&cmd2, 0, sizeof(cmd2));

ctrl->vfeStatsCmdLocal.autoFocusEnable = in->enable;
ctrl->vfeImaskLocal.afPingpongIrq = TRUE;

	cmd.windowVOffset = in->windowVOffset;
	cmd.windowHOffset = in->windowHOffset;
	cmd.windowMode    = in->windowMode;
	cmd.windowHeight  = in->windowHeight;
	cmd.windowWidth   = in->windowWidth;

	vfe_prog_hw(ctrl->vfebase + VFE_STATS_AF_CFG,
		(uint32_t *)&cmd, sizeof(cmd));

	cmd2.a00       = in->highPassCoef[0];
	cmd2.a04       = in->highPassCoef[1];
	cmd2.a20       = in->highPassCoef[2];
	cmd2.a21       = in->highPassCoef[3];
	cmd2.a22       = in->highPassCoef[4];
	cmd2.a23       = in->highPassCoef[5];
	cmd2.a24       = in->highPassCoef[6];
	cmd2.fvMax     = in->metricMax;
	cmd2.fvMetric  = in->metricSelection;
	cmd2.afHeader  = in->bufferHeader;
	cmd2.entry00   = in->gridForMultiWindows[0];
	cmd2.entry01   = in->gridForMultiWindows[1];
	cmd2.entry02   = in->gridForMultiWindows[2];
	cmd2.entry03   = in->gridForMultiWindows[3];
	cmd2.entry10   = in->gridForMultiWindows[4];
	cmd2.entry11   = in->gridForMultiWindows[5];
	cmd2.entry12   = in->gridForMultiWindows[6];
	cmd2.entry13   = in->gridForMultiWindows[7];
	cmd2.entry20   = in->gridForMultiWindows[8];
	cmd2.entry21   = in->gridForMultiWindows[9];
	cmd2.entry22   = in->gridForMultiWindows[10];
	cmd2.entry23   = in->gridForMultiWindows[11];
	cmd2.entry30   = in->gridForMultiWindows[12];
	cmd2.entry31   = in->gridForMultiWindows[13];
	cmd2.entry32   = in->gridForMultiWindows[14];
	cmd2.entry33   = in->gridForMultiWindows[15];

	vfe_prog_hw(ctrl->vfebase + VFE_STATS_AF_GRID_0,
		(uint32_t *)&cmd2, sizeof(cmd2));
}

void vfe_stats_setting(struct vfe_cmd_stats_setting *in)
{
	struct vfe_statsframe cmd1;
	struct vfe_busstats_wrprio cmd2;

	memset(&cmd1, 0, sizeof(cmd1));
	memset(&cmd2, 0, sizeof(cmd2));

	ctrl->afStatsControl.addressBuffer[0] = in->afBuffer[0];
	ctrl->afStatsControl.addressBuffer[1] = in->afBuffer[1];
	ctrl->afStatsControl.nextFrameAddrBuf = in->afBuffer[2];

	ctrl->awbStatsControl.addressBuffer[0] = in->awbBuffer[0];
	ctrl->awbStatsControl.addressBuffer[1] = in->awbBuffer[1];
	ctrl->awbStatsControl.nextFrameAddrBuf = in->awbBuffer[2];

	cmd1.lastPixel = in->frameHDimension;
	cmd1.lastLine  = in->frameVDimension;
	vfe_prog_hw(ctrl->vfebase + VFE_STATS_FRAME_SIZE,
		(uint32_t *)&cmd1, sizeof(cmd1));

	cmd2.afBusPriority    = in->afBusPriority;
	cmd2.awbBusPriority   = in->awbBusPriority;
	cmd2.histBusPriority  = in->histBusPriority;
	cmd2.afBusPriorityEn  = in->afBusPrioritySelection;
	cmd2.awbBusPriorityEn = in->awbBusPrioritySelection;
	cmd2.histBusPriorityEn = in->histBusPrioritySelection;

	vfe_prog_hw(ctrl->vfebase + VFE_BUS_STATS_WR_PRIORITY,
		(uint32_t *)&cmd2, sizeof(cmd2));

	/* Program the bus ping pong address for statistics modules. */
	writel(in->afBuffer[0], ctrl->vfebase + VFE_BUS_STATS_AF_WR_PING_ADDR);
	writel(in->afBuffer[1], ctrl->vfebase + VFE_BUS_STATS_AF_WR_PONG_ADDR);
	writel(in->awbBuffer[0],
		ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PING_ADDR);
	writel(in->awbBuffer[1],
		ctrl->vfebase + VFE_BUS_STATS_AWB_WR_PONG_ADDR);
	writel(in->histBuffer[0],
		ctrl->vfebase + VFE_BUS_STATS_HIST_WR_PING_ADDR);
	writel(in->histBuffer[1],
		ctrl->vfebase + VFE_BUS_STATS_HIST_WR_PONG_ADDR);
}

void vfe_axi_input_config(struct vfe_cmd_axi_input_config *in)
{
	struct VFE_AxiInputCmdType cmd;
	uint32_t xSizeWord, axiRdUnpackPattern;
	uint8_t  axiInputPpw;
	uint32_t busPingpongRdIrqEnable;

	ctrl->vfeImaskLocal.rdPingpongIrq = TRUE;

	switch (in->pixelSize) {
	case VFE_RAW_PIXEL_DATA_SIZE_10BIT:
		ctrl->axiInputDataSize = VFE_RAW_PIXEL_DATA_SIZE_10BIT;
		break;

	case VFE_RAW_PIXEL_DATA_SIZE_12BIT:
		ctrl->axiInputDataSize = VFE_RAW_PIXEL_DATA_SIZE_12BIT;
		break;

	case VFE_RAW_PIXEL_DATA_SIZE_8BIT:
	default:
		ctrl->axiInputDataSize = VFE_RAW_PIXEL_DATA_SIZE_8BIT;
		break;
	}

	memset(&cmd, 0, sizeof(cmd));

	switch (in->pixelSize) {
	case VFE_RAW_PIXEL_DATA_SIZE_10BIT:
		axiInputPpw = 6;
		axiRdUnpackPattern = 0xD43210;
		break;

	case VFE_RAW_PIXEL_DATA_SIZE_12BIT:
		axiInputPpw = 5;
		axiRdUnpackPattern = 0xC3210;
		break;

	case VFE_RAW_PIXEL_DATA_SIZE_8BIT:
	default:
		axiInputPpw = 8;
		axiRdUnpackPattern = 0xF6543210;
		break;
	}

	xSizeWord =
		((((in->xOffset % axiInputPpw) + in->xSize) +
			(axiInputPpw-1)) / axiInputPpw) - 1;

	cmd.stripeStartAddr0  = in->fragAddr[0];
	cmd.stripeStartAddr1  = in->fragAddr[1];
	cmd.stripeStartAddr2  = in->fragAddr[2];
	cmd.stripeStartAddr3  = in->fragAddr[3];
	cmd.ySize             = in->ySize;
	cmd.yOffsetDelta      = 0;
	cmd.xSizeWord         = xSizeWord;
	cmd.burstLength       = 1;
	cmd.NumOfRows         = in->numOfRows;
	cmd.RowIncrement      =
		(in->rowIncrement + (axiInputPpw-1))/axiInputPpw;
	cmd.mainUnpackHeight  = in->ySize;
	cmd.mainUnpackWidth   = in->xSize - 1;
	cmd.mainUnpackHbiSel  = (uint32_t)in->unpackHbi;
	cmd.mainUnpackPhase   = in->unpackPhase;
	cmd.unpackPattern     = axiRdUnpackPattern;
	cmd.padLeft           = in->padRepeatCountLeft;
	cmd.padRight          = in->padRepeatCountRight;
	cmd.padTop            = in->padRepeatCountTop;
	cmd.padBottom         = in->padRepeatCountBottom;
	cmd.leftUnpackPattern0   = in->padLeftComponentSelectCycle0;
	cmd.leftUnpackPattern1   = in->padLeftComponentSelectCycle1;
	cmd.leftUnpackPattern2   = in->padLeftComponentSelectCycle2;
	cmd.leftUnpackPattern3   = in->padLeftComponentSelectCycle3;
	cmd.leftUnpackStop0      = in->padLeftStopCycle0;
	cmd.leftUnpackStop1      = in->padLeftStopCycle1;
	cmd.leftUnpackStop2      = in->padLeftStopCycle2;
	cmd.leftUnpackStop3      = in->padLeftStopCycle3;
	cmd.rightUnpackPattern0  = in->padRightComponentSelectCycle0;
	cmd.rightUnpackPattern1  = in->padRightComponentSelectCycle1;
	cmd.rightUnpackPattern2  = in->padRightComponentSelectCycle2;
	cmd.rightUnpackPattern3  = in->padRightComponentSelectCycle3;
	cmd.rightUnpackStop0     = in->padRightStopCycle0;
	cmd.rightUnpackStop1     = in->padRightStopCycle1;
	cmd.rightUnpackStop2     = in->padRightStopCycle2;
	cmd.rightUnpackStop3     = in->padRightStopCycle3;
	cmd.topUnapckPattern     = in->padTopLineCount;
	cmd.bottomUnapckPattern  = in->padBottomLineCount;

	/*  program vfe_bus_cfg */
	vfe_prog_hw(ctrl->vfebase + VFE_BUS_STRIPE_RD_ADDR_0,
		(uint32_t *)&cmd, sizeof(cmd));

	/* hacking code, put it to default value */
	busPingpongRdIrqEnable = 0xf;

	writel(busPingpongRdIrqEnable,
		ctrl->vfebase + VFE_BUS_PINGPONG_IRQ_EN);
}

void vfe_stats_config(struct vfe_cmd_stats_setting *in)
{
	ctrl->afStatsControl.addressBuffer[0] = in->afBuffer[0];
	ctrl->afStatsControl.addressBuffer[1] = in->afBuffer[1];
	ctrl->afStatsControl.nextFrameAddrBuf = in->afBuffer[2];

	ctrl->awbStatsControl.addressBuffer[0] = in->awbBuffer[0];
	ctrl->awbStatsControl.addressBuffer[1] = in->awbBuffer[1];
	ctrl->awbStatsControl.nextFrameAddrBuf = in->awbBuffer[2];

	vfe_stats_setting(in);
}

void vfe_axi_output_config(
	struct vfe_cmd_axi_output_config *in)
{
	/* local variable  */
	uint32_t *pcircle;
	uint32_t *pdest;
	uint32_t *psrc;
	uint8_t  i;
	uint8_t  fcnt;
	uint16_t axioutpw = 8;

	/* parameters check, condition and usage mode check */
	ctrl->encPath.fragCount = in->output2.fragmentCount;
	if (ctrl->encPath.fragCount > 1)
		ctrl->encPath.multiFrag = TRUE;

	ctrl->viewPath.fragCount = in->output1.fragmentCount;
	if (ctrl->viewPath.fragCount > 1)
		ctrl->viewPath.multiFrag = TRUE;

	/* VFE_BUS_CFG.  raw data size */
	ctrl->vfeBusConfigLocal.rawPixelDataSize = in->outputDataSize;

	switch (in->outputDataSize) {
	case VFE_RAW_PIXEL_DATA_SIZE_8BIT:
		axioutpw = 8;
		break;

	case VFE_RAW_PIXEL_DATA_SIZE_10BIT:
		axioutpw = 6;
		break;

	case VFE_RAW_PIXEL_DATA_SIZE_12BIT:
		axioutpw = 5;
		break;
	}

	ctrl->axiOutputMode = in->outputMode;

	CDBG("axiOutputMode = %d\n", ctrl->axiOutputMode);

	switch (ctrl->axiOutputMode) {
	case VFE_AXI_OUTPUT_MODE_Output1: {
		ctrl->vfeCamifConfigLocal.camif2BusEnable   = FALSE;
		ctrl->vfeCamifConfigLocal.camif2OutputEnable = TRUE;
		ctrl->vfeBusConfigLocal.rawWritePathSelect  =
			VFE_RAW_OUTPUT_DISABLED;

		ctrl->encPath.pathEnabled                   = FALSE;
		ctrl->vfeImaskLocal.encIrq                  = FALSE;
		ctrl->vfeIrqCompositeMaskLocal.encIrqComMask =
			VFE_COMP_IRQ_BOTH_Y_CBCR;

		ctrl->vfeBusConfigLocal.encYWrPathEn          = FALSE;
		ctrl->vfeBusConfigLocal.encCbcrWrPathEn       = FALSE;
		ctrl->viewPath.pathEnabled                    = TRUE;
		ctrl->vfeImaskLocal.viewIrq                   = TRUE;
		ctrl->vfeIrqCompositeMaskLocal.viewIrqComMask =
			VFE_COMP_IRQ_BOTH_Y_CBCR;

		ctrl->vfeBusConfigLocal.viewYWrPathEn    = TRUE;
		ctrl->vfeBusConfigLocal.viewCbcrWrPathEn = TRUE;

		if (ctrl->vfeBusConfigLocal.encYWrPathEn &&
				ctrl->encPath.multiFrag)
			ctrl->vfeImaskLocal.encYPingpongIrq    = TRUE;

		if (ctrl->vfeBusConfigLocal.encCbcrWrPathEn &&
				ctrl->encPath.multiFrag)
			ctrl->vfeImaskLocal.encCbcrPingpongIrq = TRUE;

		if (ctrl->vfeBusConfigLocal.viewYWrPathEn &&
				ctrl->viewPath.multiFrag)
			ctrl->vfeImaskLocal.viewYPingpongIrq   = TRUE;

		if (ctrl->vfeBusConfigLocal.viewCbcrWrPathEn &&
				ctrl->viewPath.multiFrag)
			ctrl->vfeImaskLocal.viewCbcrPingpongIrq = TRUE;
	} /* VFE_AXI_OUTPUT_MODE_Output1 */
		break;

	case VFE_AXI_OUTPUT_MODE_Output2: {
		ctrl->vfeCamifConfigLocal.camif2BusEnable   = FALSE;
		ctrl->vfeCamifConfigLocal.camif2OutputEnable = TRUE;
		ctrl->vfeBusConfigLocal.rawWritePathSelect  =
			VFE_RAW_OUTPUT_DISABLED;

		ctrl->encPath.pathEnabled                   = TRUE;
		ctrl->vfeImaskLocal.encIrq                  = TRUE;
		ctrl->vfeIrqCompositeMaskLocal.encIrqComMask =
			VFE_COMP_IRQ_BOTH_Y_CBCR;

		ctrl->vfeBusConfigLocal.encYWrPathEn        = TRUE;
		ctrl->vfeBusConfigLocal.encCbcrWrPathEn     = TRUE;

		ctrl->viewPath.pathEnabled                   = FALSE;
		ctrl->vfeImaskLocal.viewIrq                  = FALSE;
		ctrl->vfeIrqCompositeMaskLocal.viewIrqComMask =
			VFE_COMP_IRQ_BOTH_Y_CBCR;

		ctrl->vfeBusConfigLocal.viewYWrPathEn        = FALSE;
		ctrl->vfeBusConfigLocal.viewCbcrWrPathEn     = FALSE;

		if (ctrl->vfeBusConfigLocal.encYWrPathEn &&
				ctrl->encPath.multiFrag)
			ctrl->vfeImaskLocal.encYPingpongIrq    = TRUE;

		if (ctrl->vfeBusConfigLocal.encCbcrWrPathEn &&
				ctrl->encPath.multiFrag)
			ctrl->vfeImaskLocal.encCbcrPingpongIrq = TRUE;

		if (ctrl->vfeBusConfigLocal.viewYWrPathEn &&
				ctrl->viewPath.multiFrag)
			ctrl->vfeImaskLocal.viewYPingpongIrq   = TRUE;

		if (ctrl->vfeBusConfigLocal.viewCbcrWrPathEn &&
				ctrl->viewPath.multiFrag)
			ctrl->vfeImaskLocal.viewCbcrPingpongIrq = TRUE;
	} /* VFE_AXI_OUTPUT_MODE_Output2 */
			break;

	case VFE_AXI_OUTPUT_MODE_Output1AndOutput2: {
		ctrl->vfeCamifConfigLocal.camif2BusEnable    = FALSE;
		ctrl->vfeCamifConfigLocal.camif2OutputEnable = TRUE;
		ctrl->vfeBusConfigLocal.rawWritePathSelect   =
			VFE_RAW_OUTPUT_DISABLED;

		ctrl->encPath.pathEnabled                    = TRUE;
		ctrl->vfeImaskLocal.encIrq                   = TRUE;
		ctrl->vfeIrqCompositeMaskLocal.encIrqComMask =
			VFE_COMP_IRQ_BOTH_Y_CBCR;

		ctrl->vfeBusConfigLocal.encYWrPathEn         = TRUE;
		ctrl->vfeBusConfigLocal.encCbcrWrPathEn      = TRUE;
		ctrl->viewPath.pathEnabled                   = TRUE;
		ctrl->vfeImaskLocal.viewIrq                  = TRUE;
		ctrl->vfeIrqCompositeMaskLocal.viewIrqComMask =
			VFE_COMP_IRQ_BOTH_Y_CBCR;

		ctrl->vfeBusConfigLocal.viewYWrPathEn        = TRUE;
		ctrl->vfeBusConfigLocal.viewCbcrWrPathEn     = TRUE;

		if (ctrl->vfeBusConfigLocal.encYWrPathEn &&
				ctrl->encPath.multiFrag)
			ctrl->vfeImaskLocal.encYPingpongIrq    = TRUE;

		if (ctrl->vfeBusConfigLocal.encCbcrWrPathEn &&
				ctrl->encPath.multiFrag)
			ctrl->vfeImaskLocal.encCbcrPingpongIrq = TRUE;

		if (ctrl->vfeBusConfigLocal.viewYWrPathEn &&
				ctrl->viewPath.multiFrag)
			ctrl->vfeImaskLocal.viewYPingpongIrq   = TRUE;

		if (ctrl->vfeBusConfigLocal.viewCbcrWrPathEn &&
				ctrl->viewPath.multiFrag)
			ctrl->vfeImaskLocal.viewCbcrPingpongIrq = TRUE;
	} /* VFE_AXI_OUTPUT_MODE_Output1AndOutput2 */
		break;

	case VFE_AXI_OUTPUT_MODE_CAMIFToAXIViaOutput2: {
		/* For raw snapshot, we need both ping and pong buffer
		 * initialized to the same address. Otherwise, if we
		 * leave the pong buffer to NULL, there will be axi_error.
		 * Note that ideally we should deal with this at upper layer,
		 * which is in msm_vfe8x.c */
		if (!in->output2.outputCbcr.outFragments[1][0]) {
			in->output2.outputCbcr.outFragments[1][0] =
				in->output2.outputCbcr.outFragments[0][0];
		}

		ctrl->vfeCamifConfigLocal.camif2BusEnable   = TRUE;
		ctrl->vfeCamifConfigLocal.camif2OutputEnable = FALSE;
		ctrl->vfeBusConfigLocal.rawWritePathSelect  =
			VFE_RAW_OUTPUT_ENC_CBCR_PATH;

		ctrl->encPath.pathEnabled                   = TRUE;
		ctrl->vfeImaskLocal.encIrq                  = TRUE;
		ctrl->vfeIrqCompositeMaskLocal.encIrqComMask =
			VFE_COMP_IRQ_CBCR_ONLY;

		ctrl->vfeBusConfigLocal.encYWrPathEn        = FALSE;
		ctrl->vfeBusConfigLocal.encCbcrWrPathEn     = TRUE;

		ctrl->viewPath.pathEnabled                   = FALSE;
		ctrl->vfeImaskLocal.viewIrq                  = FALSE;
		ctrl->vfeIrqCompositeMaskLocal.viewIrqComMask =
			VFE_COMP_IRQ_BOTH_Y_CBCR;

		ctrl->vfeBusConfigLocal.viewYWrPathEn ude <li= FALSE;008-2009 QUALCOMM Incorporated.Cbcr*/
#inclde <lux/slab.h
		if (#include <linux/interrupenc
clude <li&&
		>
#include <#in.multiFrag)h>
##include Imaskde <linux/PingpongIrqnspinpTRUck.incle <linux/liude ioe8x_proc.h"
#_proc.h"
#incte8x_proc.h"
#includedelaye8x_proc.h"
#includeplatform_dev_procee8x_proc.h"msm_vfe8x_proc.h"_proc.h"
#imedia/oth t.h><linux/list msm_vfe8x_ct.h> {
	/* bit 1:0 ENC_IRQ_MASK = 0x11:
	 * bit te IRQ when deboth y and cbcr frame is ready. */

	/* biterae8x_
struct oth y and c1:
	 * geneupt. IRQ when truct vfe_irq_compositle_enable vfeModuleEnabl	} /* VFE_AXI_OUTPUT_MODE_CAMIFToAXIViaOutput2nfig		breakfe8xcaseifMM Incorpor;
	MaskLpt_/plaAndt vfe_interrudata  1: {h>
#include Camifnux/interrupcata 2BusEnablets_cmd_Statfe_bus_cfg_dfigLfeBuQUALCOMM Incordata  ats_cmdtatsCcmdus_pmpm_sta<linux/interruprawWrite_proSelect  =incltrucRAWfe_statsVIEWt (C)_PATH 2h>
#inclu_MASK = 0proc.s_cmdh"
#incla_lut_selBusPmCcmd_data     vf= 0x11:
	 * ge <linuxmm_lut_sel  vvfeGammaLutSel 200boorqCompositeMeanoleaStartAckComoleanositeBusPCO/*
* CopyrighLocalnt32_t vfeLde <linux/interrup"
#includelis;
	boolean vfeStopAckPendi ready. */

	/*camompositeMaskLboolean vfeS_data     vfe_moduats_cmd_StatsCg vfeFlag;
	boolean vfeStopAckPendinlvfeResetAesetAckPendingFlag;pShotCount;
	boopn         vfeStatsPingPoResetAtCount g;
	uint32_t     UpdateAcke_staONLYenum VFE_AXI_OUTPUT_MODE      bit 1:0 VIEW_"
#includenlock.x_proc.h"
#include dLocal;
_t  vfeFrameSkipCounspth y and cbcr frame is ready. */

	/*camaxiOutputMt msm_vfe8x_ctrl {
	/* bit 1:0 ENC_IRQ_MASK = 0x11:
	 * geis ready. */
	s both y and cbcr frame is ready. */

	/*OperationMode;

ocal;
	structtrl { vfeIrqC 1:0 ENC
* CoMASK = 0x1 vfe_module_enable vfeModuleEnableLocal;
	struce_mask_config vfeIrqC;

	sNamect vfe_ouput_path_combo  viewPath;
	struct vfe_output_path_combo  encPath;Count;
	uintirq_c	booleanta   _cM IncoleaIg;
	boolean askfe_stats_cmd_StatsCmodule_e
	stroleaMpinlo;
	strE_STATE  vstate;

	cata m_start vfeBvfedata ruct vfe_stats_cmd_  io_lock;StatsCmdfe_stats_cmd_lock;x/plaE_STATE  vstate;

	suintcmd_drt vfe1p;
	uint32_t extlen;
	2ate;

	s_pm_start vfeBusPmConfigLocal;_t  state_lock;
	sd_data     vfrStatsBusPmruct vfe_stats_cmd_
	int vfeirock;
	strBusPmConfi2_t extlenenug;
	bCmdLocal;
	oolean vfefeIdLocal;
ENCe;

	voiuint32_StatsLaBankpAckPCount;
	uint32_t              vfeRequestedSnapShotCount;
	bockPend           vfeStatatsPingPongReloadFlag;
	uint32_t           oadFlag;
	uintid *syncdata;
};mposi_skip awbStatsCoFposiSkiplist_head li  axipt_masModhotCen;
	uint8_t  vfeFrameSkipCount;
	uintOpe_enionstructist_hrol;
	struct e_parsp;
	napShotCount vfeCamifStatu2_t regcnt     questedg,
	uint32_t *inhotCount32_t regcnt)
{
	uinprog_hw(t_he8_t *hw  vfeptr, uint32_t regcnttatus   Idoid rameInfo;
	struct Pending;

	enum VFE_AXI_OUTPUT_MODE      vfeFrameSkipPattern;rqreturn_t vfe_parse_irq(int irq_nu8_t  vfeFrameSkipPeriiovesti*/
	uint3vfeTestGenframe    vfet_head list;x/plaPacked

	/* spin_unlock_ir
	booleanqrestore(m, vofConRAW_PIXEL_DATA_SIZEIZE       axiInputDataSize;
	struct vfe_irq_thread_msg          vfeIrqThreadMsgLocal;

	struct vfe_output_path_combo  viewPath;
	strtrol;
	struct vfe_stats_control awbStatsContro vfe_frame_skip_counts vfeDroppedFrameCounts;
	struct vfe_stats_control afStatsControtrol;
	struct vfe_stats_control awbStatsControl;

	enum VFE_STATE  vstate;

	spinlock_t  ack_lock;
	spinlock_t  state_lock;
	spinlock_treg);
	for estigate */oth y a_callback *retatiImas_head tasklet_q;oid *extdata;

	spinlock_t  LASTe;

	void *syENUM: *extdata;)
dMsgswitchd fu vfeISave the addresses oid)each vfe_.d fur/* oata    */
	d furfcnt = list;
	struct frag32_t t32_pcircnt vflist;
	struct ycludeurther Buffer;
	pdeso_lock, flags); */nextus   AddrBuf @todsrc = &(in->ockt vf.store(YThre_t  ments[0][0]sk(vesti(itput; i < ->io; i++proc*ead li*++ = *_unl++spinr(ststore(&restore(&-2009 istruct vfe_irqe1fig vreturn rc;
}teMaatic void
  tasetdata  ipo_urth(;
static irot_mas_pathe_stbo *v		(c,tigate */
	/*o2tput_path_combo *epath)
{
	vpath->yPath*((sPingAddress = (uibus_pi)(-2009 QUAbase +ce_enFEct vfe_ou)oid rc  vfe_output_path_combo *vpath_protruct vfe_irqenabut_path_combo *epath)
{
	vpath->yPath.hwRegPingAddress = (uint8_t *)
		(ctrl->vfebase );
	ess =  @todo T_t *)
		(ctrl->vfee		(cThis base ->y
#in.hwRegce.h = (uint8_t *)
		( *)
		l->vfebase + VF = (uint8_t *)
		(Y_WR_PING_ADDRsk(v_CBCR_WR_PONG_ADDR)o

	epath->yPath.hwRegPh->yPath.hwRegPinRQ_M(&8_t *hwreg,
	u, Address#includ)e;

	ist;
	struct aasf_info ;
	uint8_t base + VFEid vfurren*)temp = pingR);

	epath->yPatwhichdata  _VIE = 1R);

	epath->yPatid vfI
	teIndex = 2R);

	epath->yPathBUSR_PIWDR);
	epath->yPath.hwRegPongA)rPath.hwCodo Thila
#invfebase + VFEid vfBUSngAddress_BUS_EN*epath)
{
	vpatrl-truct_ir1savmbo *vpingAddress = (uiwreg,
	uirPath._Y_WR_PONG_ADDR= Addr
static iwreg,
	uile  *)
		sk(v/* sppath,
	struct wreg,
	uihwRe		(ct @tode_output_path_comb1 *vpath,
	struct vfe_o *)
		(ctrl->vfebase + VFE_BUS_VIEW_CBCR_WR_PING_ADDR);
	vpath->cbcrPath.hwRegPong vfeIforce it to burs(ctrl->vfebase + VFE_BUS_VIEW_CBCR_WR_PONG_ADDR);

	epath->yPath.hwRegPingAddress  Configuration*/
	/* VFbase + VFE_BUS_ENC_Y_WR_PING_ADDR);
	epath->yPath.hwRegPongAput_path_combo *out1,
	stput(ctrl-_base +OVFE_BUS_ENC_Y_WR_Prq_c_PONG_ Configu	epath->yPath.hwRegPingAddress = (uint8i_output(E_SIZE (C)ase + VFE_BUS_ENC_Y_WR_P = in->outR);
	epmask.8_t *)Y.imageHeding;
th andtrucbase + VFE_BUS Contrcmd
	te2Y);

	epa = 		ou_WR_PONGurther );

	r[0]oid amifCon;
	/* convert the image width andY.imageWidth + (ou	retuY_WR_PING_ADDR);
	epatd.out2YBurstLenunit of 64bit (8 byt;

	epaY.imageWiIght;
	/* convert the image ENC_Y_WR_Itrucincrement to 0wIncrelags + (out 64bit (8 bytes
	epath->yPath= (in->outMAGEues(o_locemp	/* VFEAXI pt_mas 2 Cbc	cmd.out2YImaxi_output_confi lonaxtput 2 Cbcr Configuta;
};axit8_t *)
awbStat*ing fl*/
	p;

posied fregistersvfe_ai= (uessBu
	str(ine width aER_CFG */
	cmd.out2YBurs,usLocrPowR  */
confi= (u Inco_cux/ino *out2_
#in_mdr Cbcr Config* VFThis E - 1)) /
		figLcfg cmore(memset(&cmd, 0, sizeof(cmd)ase +CDBG("putCburatme pixelsPerLinAddr%d\n", _patidth .rement- 1)) /_patt
#in (in->Wut2Cb+lineeWid1];

	/*ou *in) /
		our_WRout2YBur
#in64uct tion*/
	/*window firstremenase BUFFER_CFGtLengt.FFER_CFG *rs in->oh =	/* VFbcr.oulaR_CFG */
	crNumRoownt8_rqresutemp;
in-utCbcr.outRowCount;
	teFFER_md.o>8_t *)/
	temp  = teouBUFFER_CFcrementIntRow32_t *int temp = (in->outpRowwIncrementIn AXIngth = g *inx11:rm /
	if epoch t;
	uint8 needst2YBbe estatic/* s vfproct_patout1Y1in64bit =++, p+) vfe_ou* VFE_BUS andBUSiemp;
<oleanows = - 1)) /
		ououtpBurprocedSnapShotCount;
	boncremE.out1eModul1 + V

	/* VFE_BUSB2GE_SIZE */
	cmd.out2Y - 1)) /
		out;mp;

	/ut2YIm1geWidthin64bit = temp;1
	/*
	int vfeirq;
_SIZE */
	r ConfiguE 2 1)) /
			}+ VFE_ s, out1 fcontentout2CbcrBu_WStatsCCONFIG se};

sely*/
	cm vfeirq;
	void __iomem *vfebaseCfgFromCmdConfiguVFE_BNux/in + VFE_EFSt8_t *)Count;md.efsEndOfthIn
static_patEFS	(inendofPongt;
	ngAdinptr++t_mas1tIn64YUS_VIEWIncsthInowInc
		rImageWir.out1];

	temp = t1YRowInc2YRowut2Cbit = temp;dthIn oFE_BUS_/
		o1Ycr ConfigurtI
stat*/g *in1];

	t *it 1 Y Coh>
#t2Cb8_t *)PImageWidthIn
		essBuffer[1Ip;

	/* VInit = teR_BUFFE
	cmthIn_Y_WR_BUFFE>yPath.addressBuffer[1];= temp;Wbcr.ou	/* V
	cmd.out1YPNameLin->oFER_CCfgLastVFE_B(in->outFE_BUS_VIEWConfiit = teding;tCbcr.outFFER_
	temp = temp;

HeiFFER_CFG *out1YImageWi*/output= outcrPongAddoutput =WidthICfg		(cBUFFER_CFG *1dth and row Y_WRowIncreout2C/
	/*) /
FFER_ut;
	c
	/* VFEngAdmageWi/
		ouissuesubsample 1.out1CbcrImageHes_BUS_VIE1Cfmd.out    out;
	cmimageHeiPongAddkip 0x1it = te.imageHeight;thInRowIn64bit = temp;dthIut1YPrNumRowouutput 1 Y Co
		2md.out1C
		 (ouncrementIn2ut2.oamstLengtS_ENC_C1 CmageWidthI_CbcrThreageWidthht;
	tem
		(ctrtemp =ModWR_PING_A  */
	cmd.out2CbcrPmod	out;
	d_pm_sto *out2out2YRowIWrangth = burs lonp;

/r_WRburstrap 1 Cbcr out1PIp = (in->FE_BUS_E(.out1- 1)) /
		o = out1emp = (init = tem

	/*FFER_CFencoutpWr_t temp;
inmp;

	/*Cbcr_hwipPattern;bvfe_+Rows = I Out t - ,XI Ou_head li *)
		(c_Y_WR_PIN/
	ted.out2CbcrBufov_croput;
	cmd.outFER_CFG */
izean vp;
	uin3output1.outp;
	cmd.rDataSize;
dth +
		(oput2.iewYWrPathEePathSelect  pu#include ock;
	askLoccmd.outropatic irqrn;
	cstati= temp;
OV Corp, Ptructonfi*Cbcr		(cRowIncrementiut2CbcrPiutputcbct = temp;

	/* VFk;
	spinle image width andCFG);StatsCh)
{
	vpCbcrBurstLeng
Height;ress = (uint_Y_WR_PING_ArNumfg.VSnt32_t itCbcr.ted.
*/
#inclFFER_t vfeROP_WIDTH_CFnclun->camifCfgFr	cmd.vmd.vSyncEdgd.rawP(outDataSget_hw_version2_t)in->rawPixelyncstru    *ment;

	>camifCfgvfeHwVfCfgFrqrest4bitoutput2.outcamifC v_BUS_Efg.t32_temp =
	En    ask_lmRows = .vSyncEdShot
HW_VERSIONe + VNumRcomb2_t)in->rawP	strfgF*)&vfeSubSampleEnablee + Vout->corebSampletionver.->spinl2Out;_VIEW_nminor	cfg.capt;
	st_t  acn->c
		cfgbal;
	j2BusEnabl	;

	spinrqSampleEnhSyncath)
{
ut2CbcrBu(uint8;

	enal_variigats(- 1).scaminsigned longvfe_ire + VFElerru fg.binninW_Y_WR_BUFFE
	cmhardwareBCount;
	tetRow= 0x1;
	str;

/
		outht;
able;
rp++);
		/SubsammisrEntemp;
nloc = disigat,  1 =in64bit*/
	cmWritePatht vfeirq;t)in-w vfel(*((uze;

		

	/*  pIF_CONFIG);
}

static voi
	temp/* 0  = in->viewcfg),id vfthe image wt vfe     *rt vfeBusPmConfibcrPinregrqreturno *outrt vfeBusPmConfi (ouThis  *out2, ui vfecmdimageHeight;
	temp = (in->o= 0x11:
	 lect;

	/*  ploarWrPathEskletPiNumRoset(&cmd, 0, size	uint32_ngpon.h>
#iRe= in>camifCst= in->view&cmtrl->vf;
	cmdtatic voi<linux/interrud), ctrl->vfebase + V<linux/interru
	tCbcr.bus commfe_iput_%Pmd)ONG_AtatsPiriperl->vfe
		in->camts are pulse basded, as the control bits xt vfe_bumd cmd; the image wimd), ctrl->
	CDBG("bus command = 		in->camNam.out2CbcrPinReload        = in;
}

 */
	cmd.out
	CDBG("bus command = DroppedCMD);
	(ctsoid vfe_reg_module_cfg(;

	spick_t  acele;

0;
eEnabed.
	 * Don'rqTh    Msse based.
	 * Don't want to r  actCbcr.bpathLeaht = in-Rowsout1YProningomCmd.miutCbcr.outRowdthInAnit of 64path)
{I Output 1 Y Coemuxopf2BusEnt want to rn->dable;
	st vfe_frame_bpc_info   eDoneSp;

	romCmd.mis VFE vfe_frame_bpc_info          vfeBpctCout vfe_cmd_fradFlag;
	utic rl->vf; */

	/* spin_u2_t regcnt)
{
her investigatea.statsEnable             umct vfoutput_pEnabmd, 0, susEnaalerE,nable;e balensRol      =le =
STATE_IDLurn_inScaunmain
	stath_cohromaUpsampleEnma   = ->whitemd.mi	enaxidata  _starBalanct;
	trc = (in->outprc pulmd.hbusPurn  andinuous  vfeibusurn snapshotn->yHCmd.mi    .demO>vfebioncolorCorrecSTART_OPERATIONsPingpoONTINUOUSeEnable     l,
	uintle enle     osaichite_headte Iemp * Don't want tlensRol/* thisombobsampleEn32IrqC;
	tege1];
tCbcr.outRowCMD);Ifig */
mosaicEnablvfe_bus_ *vpath1Patd.mile  xf      =EnhanEnhanEnable  asfEnhanE   eriodtion3_BUS_ENCeple;
	ena.chromaSupp2t32_t i;
	uin     = inchromaSup;
	ena.chromaSupp2ese   omaSupprelensRol= in->chroeEnable;
	a.chromaSubsampleEnable      = iaAdapttiona->asfEnable;
	ena.chraSubsampleEnabDBG("bus ollOf	in->Y.imageWidth +, ctrl->vfebase t2YBurst	CDBG("bus }

static voiinbase + VFE, ctrl->vfebase +vfe_modbcr_ngReload	cmd.out2YBurincrement tVFE_BUS_ENCngth;
	cbcout - t3usBirrec = 5.out2YRowIncrementR  */
	cmd_BUFFER_CFG */1->cbcrPath.a/
	/* spin_unalu= 7ampleEnabl*p++ = *int1Cbcr         = in VFE_BUfaultLengbank 0able          =
	strunable    %xt;
	c */
Selst lengtEnhanPongchannelt)in->rset(&cmd, 0, sizen vfeStopA = in->vieweload pulsed.out2CbcrPble      _headpleEnlensRollOffEni_outpDMIG_ADDR  */
af_head l_off_C_Cbcr__dmi

	cmenhead lirt v;

FE	strULE_CF out uinwbin->initTabl	/* s16>vieinitGr =G((uileEnnt16TeGr;
= (uint8Rows = i_t  vfr[1];esst_t *init in->initTabl	
		
	uint16_nitB =  in->i.hSyncEdge;

	ceEnabollOffE   ubSleEnableosaicEn    nable;gsampleEnabl VFE_DMI_CFG_DeEnabAckck_t  io_lockEnable =*  prof_config = (u vfepackfebase + V*)&cmd), ct)
{
	uinead li*vpathp = (in-= in->uwDBG(l(trucDISABLE_ALLteAcS*pDeusSubsampleEnable =
uct vOMPOSITEfe_ou;
	cmdpaclea-2009 p of 64b nt16 tabl)in-oid)rc;
}

CLEARE_ROLL_OFF_INIT_T< VFEageWid
vf)adMs	dGr))32_t)(*i = in->eEnableckFF) |
			(= temp;t32_t)(>deltaTableR;

	vfe_		initR++;
		initGr++;

		wrbleR;

	)t32_t)(*iniRESET_UPON16sk(v	CMD		initR++;
		initGr++;

GLOBALB++;
		ini.hSyn