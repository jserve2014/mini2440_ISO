/*++
Copyright-c Realtek Semiconductor Corp. All rights reserved.

Module Name:
	r8192U_dm.c

Abstract:
	HW dynamic mechanism.

Major Change History:
	When      	Who				What
	----------	--------------- -------------------------------
	2008-05-14	amy                     create version 0 porting from windows code.

--*/


#include "r8192U.h"
#include "r8192U_dm.h"
#include "r819xU_cmdpkt.h"
#include "r8192S_hw.h"
#include "r8192S_phy.h"
#include "r8192S_phyreg.h"

/*---------------------------Define Local Constant---------------------------*/
//
// Indicate different AP vendor for IOT issue.
//
#if 1
		static u32 edca_setting_DL[HT_IOT_PEER_MAX] =
		// UNKNOWN	REALTEK_90	/*REALTEK_92SE*/	BROADCOM	RALINK		ATHEROS		CISCO		MARVELL		92U_AP		SELF_AP
		   { 0xa44f, 	0x5ea44f, 	0x5ea44f,		0xa44f,		0xa44f, 		0xa44f, 		0xa630,		0xa42b,		0x5e4322,	0x5e4322};
		static u32 edca_setting_UL[HT_IOT_PEER_MAX] =
		// UNKNOWN	REALTEK		/*REALTEK_92SE*/	BROADCOM	RALINK		ATHEROS		CISCO		MARVELL		92U_AP		SELF_AP
		   { 0x5ea44f, 	0xa44f, 	0x5ea44f,		0x5e4322, 	0x5ea422, 	0x5e4322, 	0x3ea44f,	0x5ea42b,	0x5e4322,	0x5e4322};

#endif

#define RTK_UL_EDCA 0xa44f
#define RTK_DL_EDCA 0x5e4322
/*---------------------------Define Local Constant---------------------------*/


/*------------------------Define global variable-----------------------------*/
// Debug variable ?
dig_t	dm_digtable;
// Store current shoftware write register content for MAC PHY.
u8		dm_shadow[16][256] = {{0}};
// For Dynamic Rx Path Selection by Signal Strength
DRxPathSel	DM_RxPathSelTable;
/*------------------------Define global variable-----------------------------*/


/*------------------------Define local variable------------------------------*/
/*------------------------Define local variable------------------------------*/


/*--------------------Define export function prototype-----------------------*/
#ifdef TO_DO_LIST
static	void dm_CheckProtection(struct net_device *dev);
#endif
extern	void	init_hal_dm(struct net_device *dev);
extern	void deinit_hal_dm(struct net_device *dev);

extern void hal_dm_watchdog(struct net_device *dev);


extern	void	init_rate_adaptive(struct net_device *dev);
extern	void	dm_txpower_trackingcallback(struct work_struct *work);
extern	void	dm_cck_txpower_adjust(struct net_device *dev,bool  binch14);
extern	void	dm_restore_dynamic_mechanism_state(struct net_device *dev);
extern	void	dm_backup_dynamic_mechanism_state(struct net_device *dev);
extern	void	dm_change_dynamic_initgain_thresh(struct net_device *dev,
								u32		dm_type,
								u32		dm_value);
extern	void	DM_ChangeFsyncSetting(struct net_device *dev,
												s32		DM_Type,
												s32		DM_Value);
extern	void dm_force_tx_fw_info(struct net_device *dev,
										u32		force_type,
										u32		force_value);
extern	void	dm_init_edca_turbo(struct net_device *dev);
extern	void	dm_rf_operation_test_callback(unsigned long data);
extern	void	dm_rf_pathcheck_workitemcallback(struct work_struct *work);
extern	void dm_fsync_timer_callback(unsigned long data);
extern	void dm_check_fsync(struct net_device *dev);
extern	void	dm_shadow_init(struct net_device *dev);


/*--------------------Define export function prototype-----------------------*/


/*---------------------Define local function prototype-----------------------*/
// DM --> Rate Adaptive
static	void	dm_check_rate_adaptive(struct net_device *dev);

// DM --> Bandwidth switch
static	void	dm_init_bandwidth_autoswitch(struct net_device *dev);
static	void	dm_bandwidth_autoswitch(	struct net_device *dev);

// DM --> TX power control
//static	void	dm_initialize_txpower_tracking(struct net_device *dev);

static	void	dm_check_txpower_tracking(struct net_device *dev);



//static	void	dm_txpower_reset_recovery(struct net_device *dev);


// DM --> BB init gain restore
#ifndef RTL8192U
static	void	dm_bb_initialgain_restore(struct net_device *dev);


// DM --> BB init gain backup
static	void	dm_bb_initialgain_backup(struct net_device *dev);
#endif
// DM --> Dynamic Init Gain by RSSI
static	void	dm_dig_init(struct net_device *dev);
static	void	dm_ctrl_initgain_byrssi(struct net_device *dev);
static	void	dm_ctrl_initgain_byrssi_highpwr(struct net_device *dev);
static	void	dm_ctrl_initgain_byrssi_by_driverrssi(	struct net_device *dev);
static	void	dm_ctrl_initgain_byrssi_by_fwfalse_alarm(struct net_device *dev);
static	void	dm_initial_gain(struct net_device *dev);
static	void	dm_pd_th(struct net_device *dev);
static	void	dm_cs_ratio(struct net_device *dev);

static	void dm_init_ctstoself(struct net_device *dev);
// DM --> EDCA turboe mode control
static	void	dm_check_edca_turbo(struct net_device *dev);

// DM --> HW RF control
static	void	dm_check_rfctrl_gpio(struct net_device *dev);

#ifndef RTL8190P
//static	void	dm_gpio_change_rf(struct net_device *dev);
#endif
// DM --> Check PBC
static	void dm_check_pbc_gpio(struct net_device *dev);


// DM --> Check current RX RF path state
static	void	dm_check_rx_path_selection(struct net_device *dev);
static 	void dm_init_rxpath_selection(struct net_device *dev);
static	void dm_rxpath_sel_byrssi(struct net_device *dev);


// DM --> Fsync for broadcom ap
static void dm_init_fsync(struct net_device *dev);
static void dm_deInit_fsync(struct net_device *dev);

//Added by vivi, 20080522
static	void	dm_check_txrateandretrycount(struct net_device *dev);

/*---------------------Define local function prototype-----------------------*/

/*---------------------Define of Tx Power Control For Near/Far Range --------*/   //Add by Jacken 2008/02/18
static	void	dm_init_dynamic_txpower(struct net_device *dev);
static	void	dm_dynamic_txpower(struct net_device *dev);


// DM --> For rate adaptive and DIG, we must send RSSI to firmware
static	void dm_send_rssi_tofw(struct net_device *dev);
static	void	dm_ctstoself(struct net_device *dev);
/*---------------------------Define function prototype------------------------*/
//================================================================================
//	HW Dynamic mechanism interface.
//================================================================================
static void dm_CheckAggrPolicy(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	PRT_HIGH_THROUGHPUT	pHTInfo = priv->ieee80211->pHTInfo;
	//u8 			QueueId;
	//PRT_TCB			pTcb;
	bool			bAmsduEnable = false;

	static u8		lastTxOkCnt = 0;
	static u8		lastRxOkCnt = 0;
	u8			curTxOkCnt = 0;
	u8			curRxOkCnt = 0;

	// Determine if A-MSDU policy.
	if(priv->ieee80211->pHTInfo->IOTAction & HT_IOT_ACT_HYBRID_AGGREGATION)
	{
		if(read_nic_byte(dev, INIMCS_SEL) > DESC92S_RATE54M)
			bAmsduEnable = true;
	}
	else if(priv->ieee80211->pHTInfo->IOTAction & HT_IOT_ACT_AMSDU_ENABLE)
	{
		if(read_nic_byte(dev, INIMCS_SEL) > DESC92S_RATE54M)
		{
			curTxOkCnt = priv->stats.txbytesunicast - lastTxOkCnt;
			curRxOkCnt = priv->stats.rxbytesunicast - lastRxOkCnt;

			if(curRxOkCnt <= 4*curTxOkCnt)
				bAmsduEnable = true;
		}
	}
	else
	{
		// Do not need to switch aggregation policy.
		return;
	}

	// Switch A-MSDU
	if(bAmsduEnable && !pHTInfo->bCurrent_AMSDU_Support)
	{
		pHTInfo->bCurrent_AMSDU_Support = true;
	}
	else if(!bAmsduEnable && pHTInfo->bCurrent_AMSDU_Support)
	{
#ifdef TO_DO_LIST
		//PlatformAcquireSpinLock(Adapter, RT_TX_SPINLOCK);
		for(QueueId = 0; QueueId < MAX_TX_QUEUE; QueueId++)
		{
			while(!RTIsListEmpty(&dev->TcbAggrQueue[QueueId]))
			{
				pTcb = (PRT_TCB)RTRemoveHeadList(&dev->TcbAggrQueue[QueueId]);
				dev->TcbCountInAggrQueue[QueueId]--;
				PreTransmitTCB(dev, pTcb);
			}
		}
		//PlatformReleaseSpinLock(Adapter, RT_TX_SPINLOCK);
		pHTInfo->bCurrent_AMSDU_Support = false;
#endif
	}

	// Determine A-MPDU policy
	if(priv->ieee80211->pHTInfo->IOTAction & HT_IOT_ACT_AMSDU_ENABLE)
	{
		if(!bAmsduEnable)
			pHTInfo->bCurrentAMPDUEnable = true;
	}

	// Update local static variables.
	lastTxOkCnt = priv->stats.txbytesunicast;
	lastRxOkCnt = priv->stats.rxbytesunicast;
}
//
//	Description:
//		Prepare SW resource for HW dynamic mechanism.
//
//	Assumption:
//		This function is only invoked at driver intialization once.
//
//
extern	void
init_hal_dm(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	// Undecorated Smoothed Signal Strength, it can utilized to dynamic mechanism.
	priv->undecorated_smoothed_pwdb = -1;

	//Initial TX Power Control for near/far range , add by amy 2008/05/15, porting from windows code.
	dm_init_dynamic_txpower(dev);
	init_rate_adaptive(dev);
	dm_initialize_txpower_tracking(dev);
	dm_dig_init(dev);
	dm_init_edca_turbo(dev);
	dm_init_bandwidth_autoswitch(dev);
	dm_init_fsync(dev);
	dm_init_rxpath_selection(dev);
	dm_init_ctstoself(dev);

}	// InitHalDm

extern void deinit_hal_dm(struct net_device *dev)
{

	dm_deInit_fsync(dev);

}




//#if 0
extern  void    hal_dm_watchdog(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	if(priv->bInHctTest)
		return;


	dm_check_rfctrl_gpio(dev);

	// Add by hpfan 2008-03-11
	dm_check_pbc_gpio(dev);
	dm_check_txrateandretrycount(dev); //moved by tynli
	dm_check_edca_turbo(dev);

	dm_CheckAggrPolicy(dev);

#ifdef TO_DO_LIST
	dm_CheckProtection(dev);
#endif

	// ====================================================
	// If any dynamic mechanism is ready, put it above this return;
	// ====================================================
	//if (IS_HARDWARE_TYPE_8192S(dev))
	return;

#ifdef TO_DO_LIST
	if(Adapter->MgntInfo.mActingAsAp)
	{
		AP_dm_CheckRateAdaptive(dev);
		//return;
	}
	else
#endif
	{
		dm_check_rate_adaptive(dev);
	}
	dm_dynamic_txpower(dev);

	dm_check_txpower_tracking(dev);
	dm_ctrl_initgain_byrssi(dev);//LZM TMP 090302

	dm_bandwidth_autoswitch(dev);

	dm_check_rx_path_selection(dev);//LZM TMP 090302
	dm_check_fsync(dev);

	dm_send_rssi_tofw(dev);

	dm_ctstoself(dev);

}	//HalDmWatchDog

/*
  * Decide Rate Adaptive Set according to distance (signal strength)
  *	01/11/2008	MHC		Modify input arguments and RATR table level.
  *	01/16/2008	MHC		RF_Type is assigned in ReadAdapterInfo(). We must call
  *						the function after making sure RF_Type.
  */
extern void init_rate_adaptive(struct net_device * dev)
{

	struct r8192_priv *priv = ieee80211_priv(dev);
	prate_adaptive	pra = (prate_adaptive)&priv->rate_adaptive;

	pra->ratr_state = DM_RATR_STA_MAX;
	pra->high2low_rssi_thresh_for_ra = RateAdaptiveTH_High;
	pra->low2high_rssi_thresh_for_ra20M = RateAdaptiveTH_Low_20M+5;
	pra->low2high_rssi_thresh_for_ra40M = RateAdaptiveTH_Low_40M+5;

	pra->high_rssi_thresh_for_ra = RateAdaptiveTH_High+5;
	pra->low_rssi_thresh_for_ra20M = RateAdaptiveTH_Low_20M;
	pra->low_rssi_thresh_for_ra40M = RateAdaptiveTH_Low_40M;

	if(priv->CustomerID == RT_CID_819x_Netcore)
		pra->ping_rssi_enable = 1;
	else
		pra->ping_rssi_enable = 0;
	pra->ping_rssi_thresh_for_ra = 15;


	if (priv->rf_type == RF_2T4R)
	{
		// 07/10/08 MH Modify for RA smooth scheme.
		/* 2008/01/11 MH Modify 2T RATR table for different RSSI. 080515 porting by amy from windows code.*/
		pra->upper_rssi_threshold_ratr		= 	0x8f0f0000;
		pra->middle_rssi_threshold_ratr		= 	0x8f0ff000;
		pra->low_rssi_threshold_ratr		= 	0x8f0ff001;
		pra->low_rssi_threshold_ratr_40M	= 	0x8f0ff005;
		pra->low_rssi_threshold_ratr_20M	= 	0x8f0ff001;
		pra->ping_rssi_ratr	= 	0x0000000d;//cosa add for test
	}
	else if (priv->rf_type == RF_1T2R)
	{
		pra->upper_rssi_threshold_ratr		= 	0x000f0000;
		pra->middle_rssi_threshold_ratr		= 	0x000ff000;
		pra->low_rssi_threshold_ratr		= 	0x000ff001;
		pra->low_rssi_threshold_ratr_40M	= 	0x000ff005;
		pra->low_rssi_threshold_ratr_20M	= 	0x000ff001;
		pra->ping_rssi_ratr	= 	0x0000000d;//cosa add for test
	}

}	// InitRateAdaptive


/*-----------------------------------------------------------------------------
 * Function:	dm_check_rate_adaptive()
 *
 * Overview:
 *
 * Input:		NONE
 *
 * Output:		NONE
 *
 * Return:		NONE
 *
 * Revised History:
 *	When		Who		Remark
 *	05/26/08	amy 	Create version 0 proting from windows code.
 *
 *---------------------------------------------------------------------------*/
static void dm_check_rate_adaptive(struct net_device * dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	PRT_HIGH_THROUGHPUT	pHTInfo = priv->ieee80211->pHTInfo;
	prate_adaptive			pra = (prate_adaptive)&priv->rate_adaptive;
	u32						currentRATR, targetRATR = 0;
	u32						LowRSSIThreshForRA = 0, HighRSSIThreshForRA = 0;
	bool						bshort_gi_enabled = false;
	static u8					ping_rssi_state=0;


	if(!priv->up)
	{
		RT_TRACE(COMP_RATE, "<---- dm_check_rate_adaptive(): driver is going to unload\n");
		return;
	}

	if(pra->rate_adaptive_disabled)//this variable is set by ioctl.
		return;

	// TODO: Only 11n mode is implemented currently,
	if( !(priv->ieee80211->mode == WIRELESS_MODE_N_24G ||
		 priv->ieee80211->mode == WIRELESS_MODE_N_5G))
		 return;

	if( priv->ieee80211->state == IEEE80211_LINKED )
	{
	//	RT_TRACE(COMP_RATE, "dm_CheckRateAdaptive(): \t");

		//
		// Check whether Short GI is enabled
		//
		bshort_gi_enabled = (pHTInfo->bCurTxBW40MHz && pHTInfo->bCurShortGI40MHz) ||
			(!pHTInfo->bCurTxBW40MHz && pHTInfo->bCurShortGI20MHz);


		pra->upper_rssi_threshold_ratr =
				(pra->upper_rssi_threshold_ratr & (~BIT31)) | ((bshort_gi_enabled)? BIT31:0) ;

		pra->middle_rssi_threshold_ratr =
				(pra->middle_rssi_threshold_ratr & (~BIT31)) | ((bshort_gi_enabled)? BIT31:0) ;

		if (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)
		{
			pra->low_rssi_threshold_ratr =
				(pra->low_rssi_threshold_ratr_40M & (~BIT31)) | ((bshort_gi_enabled)? BIT31:0) ;
		}
		else
		{
			pra->low_rssi_threshold_ratr =
			(pra->low_rssi_threshold_ratr_20M & (~BIT31)) | ((bshort_gi_enabled)? BIT31:0) ;
		}
		//cosa add for test
		pra->ping_rssi_ratr =
				(pra->ping_rssi_ratr & (~BIT31)) | ((bshort_gi_enabled)? BIT31:0) ;

		/* 2007/10/08 MH We support RA smooth scheme now. When it is the first
		   time to link with AP. We will not change upper/lower threshold. If
		   STA stay in high or low level, we must change two different threshold
		   to prevent jumping frequently. */
		if (pra->ratr_state == DM_RATR_STA_HIGH)
		{
			HighRSSIThreshForRA 	= pra->high2low_rssi_thresh_for_ra;
			LowRSSIThreshForRA	= (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)?
					(pra->low_rssi_thresh_for_ra40M):(pra->low_rssi_thresh_for_ra20M);
		}
		else if (pra->ratr_state == DM_RATR_STA_LOW)
		{
			HighRSSIThreshForRA	= pra->high_rssi_thresh_for_ra;
			LowRSSIThreshForRA 	= (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)?
					(pra->low2high_rssi_thresh_for_ra40M):(pra->low2high_rssi_thresh_for_ra20M);
		}
		else
		{
			HighRSSIThreshForRA	= pra->high_rssi_thresh_for_ra;
			LowRSSIThreshForRA	= (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)?
					(pra->low_rssi_thresh_for_ra40M):(pra->low_rssi_thresh_for_ra20M);
		}

		//DbgPrint("[DM] THresh H/L=%d/%d\n\r", RATR.HighRSSIThreshForRA, RATR.LowRSSIThreshForRA);
		if(priv->undecorated_smoothed_pwdb >= (long)HighRSSIThreshForRA)
		{
			//DbgPrint("[DM] RSSI=%d STA=HIGH\n\r", pHalData->UndecoratedSmoothedPWDB);
			pra->ratr_state = DM_RATR_STA_HIGH;
			targetRATR = pra->upper_rssi_threshold_ratr;
		}else if(priv->undecorated_smoothed_pwdb >= (long)LowRSSIThreshForRA)
		{
			//DbgPrint("[DM] RSSI=%d STA=Middle\n\r", pHalData->UndecoratedSmoothedPWDB);
			pra->ratr_state = DM_RATR_STA_MIDDLE;
			targetRATR = pra->middle_rssi_threshold_ratr;
		}else
		{
			//DbgPrint("[DM] RSSI=%d STA=LOW\n\r", pHalData->UndecoratedSmoothedPWDB);
			pra->ratr_state = DM_RATR_STA_LOW;
			targetRATR = pra->low_rssi_threshold_ratr;
		}

			//cosa add for test
		if(pra->ping_rssi_enable)
		{
			//pHalData->UndecoratedSmoothedPWDB = 19;
			if(priv->undecorated_smoothed_pwdb < (long)(pra->ping_rssi_thresh_for_ra+5))
			{
				if( (priv->undecorated_smoothed_pwdb < (long)pra->ping_rssi_thresh_for_ra) ||
					ping_rssi_state )
				{
					//DbgPrint("TestRSSI = %d, set RATR to 0x%x \n", pHalData->UndecoratedSmoothedPWDB, pRA->TestRSSIRATR);
					pra->ratr_state = DM_RATR_STA_LOW;
					targetRATR = pra->ping_rssi_ratr;
					ping_rssi_state = 1;
				}
				//else
				//	DbgPrint("TestRSSI is between the range. \n");
			}
			else
			{
				//DbgPrint("TestRSSI Recover to 0x%x \n", targetRATR);
				ping_rssi_state = 0;
			}
		}

		// 2008.04.01
#if 1
		// For RTL819X, if pairwisekey = wep/tkip, we support only MCS0~7.
		if(priv->ieee80211->GetHalfNmodeSupportByAPsHandler(dev))
			targetRATR &=  0xf00fffff;
#endif

		//
		// Check whether updating of RATR0 is required
		//
		currentRATR = read_nic_dword(dev, RATR0);
		if( targetRATR !=  currentRATR )
		{
			u32 ratr_value;
			ratr_value = targetRATR;
			RT_TRACE(COMP_RATE,"currentRATR = %x, targetRATR = %x\n", currentRATR, targetRATR);
			if(priv->rf_type == RF_1T2R)
			{
				ratr_value &= ~(RATE_ALL_OFDM_2SS);
			}
			write_nic_dword(dev, RATR0, ratr_value);
			write_nic_byte(dev, UFWP, 1);

			pra->last_ratr = targetRATR;
		}

	}
	else
	{
		pra->ratr_state = DM_RATR_STA_MAX;
	}

}	// dm_CheckRateAdaptive


static void dm_init_bandwidth_autoswitch(struct net_device * dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	priv->ieee80211->bandwidth_auto_switch.threshold_20Mhzto40Mhz = BW_AUTO_SWITCH_LOW_HIGH;
	priv->ieee80211->bandwidth_auto_switch.threshold_40Mhzto20Mhz = BW_AUTO_SWITCH_HIGH_LOW;
	priv->ieee80211->bandwidth_auto_switch.bforced_tx20Mhz = false;
	priv->ieee80211->bandwidth_auto_switch.bautoswitch_enable = false;

}	// dm_init_bandwidth_autoswitch


static void dm_bandwidth_autoswitch(struct net_device * dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	if(priv->CurrentChannelBW == HT_CHANNEL_WIDTH_20 ||!priv->ieee80211->bandwidth_auto_switch.bautoswitch_enable){
		return;
	}else{
		if(priv->ieee80211->bandwidth_auto_switch.bforced_tx20Mhz == false){//If send packets in 40 Mhz in 20/40
			if(priv->undecorated_smoothed_pwdb <= priv->ieee80211->bandwidth_auto_switch.threshold_40Mhzto20Mhz)
				priv->ieee80211->bandwidth_auto_switch.bforced_tx20Mhz = true;
		}else{//in force send packets in 20 Mhz in 20/40
			if(priv->undecorated_smoothed_pwdb >= priv->ieee80211->bandwidth_auto_switch.threshold_20Mhzto40Mhz)
				priv->ieee80211->bandwidth_auto_switch.bforced_tx20Mhz = false;

		}
	}
}	// dm_BandwidthAutoSwitch

//OFDM default at 0db, index=6.
static u32 OFDMSwingTable[OFDM_Table_Length] = {
	0x7f8001fe,	// 0, +6db
	0x71c001c7,	// 1, +5db
	0x65400195,	// 2, +4db
	0x5a400169,	// 3, +3db
	0x50800142,	// 4, +2db
	0x47c0011f,	// 5, +1db
	0x40000100,	// 6, +0db ===> default, upper for higher temprature, lower for low temprature
	0x390000e4,	// 7, -1db
	0x32c000cb,	// 8, -2db
	0x2d4000b5,	// 9, -3db
	0x288000a2,	// 10, -4db
	0x24000090,	// 11, -5db
	0x20000080,	// 12, -6db
	0x1c800072,	// 13, -7db
	0x19800066,	// 14, -8db
	0x26c0005b,	// 15, -9db
	0x24400051,	// 16, -10db
	0x12000048,	// 17, -11db
	0x10000040	// 18, -12db
};

static u8	CCKSwingTable_Ch1_Ch13[CCK_Table_length][8] = {
	{0x36, 0x35, 0x2e, 0x25, 0x1c, 0x12, 0x09, 0x04},	// 0, +0db ===> CCK40M default
	{0x30, 0x2f, 0x29, 0x21, 0x19, 0x10, 0x08, 0x03},	// 1, -1db
	{0x2b, 0x2a, 0x25, 0x1e, 0x16, 0x0e, 0x07, 0x03},	// 2, -2db
	{0x26, 0x25, 0x21, 0x1b, 0x14, 0x0d, 0x06, 0x03},	// 3, -3db
	{0x22, 0x21, 0x1d, 0x18, 0x11, 0x0b, 0x06, 0x02},	// 4, -4db
	{0x1f, 0x1e, 0x1a, 0x15, 0x10, 0x0a, 0x05, 0x02},	// 5, -5db
	{0x1b, 0x1a, 0x17, 0x13, 0x0e, 0x09, 0x04, 0x02},	// 6, -6db ===> CCK20M default
	{0x18, 0x17, 0x15, 0x11, 0x0c, 0x08, 0x04, 0x02},	// 7, -7db
	{0x16, 0x15, 0x12, 0x0f, 0x0b, 0x07, 0x04, 0x01},	// 8, -8db
	{0x13, 0x13, 0x10, 0x0d, 0x0a, 0x06, 0x03, 0x01},	// 9, -9db
	{0x11, 0x11, 0x0f, 0x0c, 0x09, 0x06, 0x03, 0x01},	// 10, -10db
	{0x0f, 0x0f, 0x0d, 0x0b, 0x08, 0x05, 0x03, 0x01}	// 11, -11db
};

static u8	CCKSwingTable_Ch14[CCK_Table_length][8] = {
	{0x36, 0x35, 0x2e, 0x1b, 0x00, 0x00, 0x00, 0x00},	// 0, +0db  ===> CCK40M default
	{0x30, 0x2f, 0x29, 0x18, 0x00, 0x00, 0x00, 0x00},	// 1, -1db
	{0x2b, 0x2a, 0x25, 0x15, 0x00, 0x00, 0x00, 0x00},	// 2, -2db
	{0x26, 0x25, 0x21, 0x13, 0x00, 0x00, 0x00, 0x00},	// 3, -3db
	{0x22, 0x21, 0x1d, 0x11, 0x00, 0x00, 0x00, 0x00},	// 4, -4db
	{0x1f, 0x1e, 0x1a, 0x0f, 0x00, 0x00, 0x00, 0x00},	// 5, -5db
	{0x1b, 0x1a, 0x17, 0x0e, 0x00, 0x00, 0x00, 0x00},	// 6, -6db  ===> CCK20M default
	{0x18, 0x17, 0x15, 0x0c, 0x00, 0x00, 0x00, 0x00},	// 7, -7db
	{0x16, 0x15, 0x12, 0x0b, 0x00, 0x00, 0x00, 0x00},	// 8, -8db
	{0x13, 0x13, 0x10, 0x0a, 0x00, 0x00, 0x00, 0x00},	// 9, -9db
	{0x11, 0x11, 0x0f, 0x09, 0x00, 0x00, 0x00, 0x00},	// 10, -10db
	{0x0f, 0x0f, 0x0d, 0x08, 0x00, 0x00, 0x00, 0x00}	// 11, -11db
};

static void dm_TXPowerTrackingCallback_TSSI(struct net_device * dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	bool						bHighpowerstate, viviflag = FALSE;
	DCMD_TXCMD_T			tx_cmd;
	u8						powerlevelOFDM24G;
	int	    					i =0, j = 0, k = 0;
	u8						RF_Type, tmp_report[5]={0, 0, 0, 0, 0};
	u32						Value;
	u8						Pwr_Flag;
	u16						Avg_TSSI_Meas, TSSI_13dBm, Avg_TSSI_Meas_from_driver=0;
	//RT_STATUS 				rtStatus = RT_STATUS_SUCCESS;
#ifdef RTL8192U
	bool rtStatus = true;
#endif
	u32						delta=0;

	write_nic_byte(dev, 0x1ba, 0);

	priv->ieee80211->bdynamic_txpower_enable = false;
	bHighpowerstate = priv->bDynamicTxHighPower;

	powerlevelOFDM24G = (u8)(priv->Pwr_Track>>24);
	RF_Type = priv->rf_type;
	Value = (RF_Type<<8) | powerlevelOFDM24G;

	RT_TRACE(COMP_POWER_TRACKING, "powerlevelOFDM24G = %x\n", powerlevelOFDM24G);

	for(j = 0; j<=30; j++)
{	//fill tx_cmd

	tx_cmd.Op		= TXCMD_SET_TX_PWR_TRACKING;
	tx_cmd.Length	= 4;
	tx_cmd.Value		= Value;
#ifdef RTL8192U
	rtStatus = SendTxCommandPacket(dev, &tx_cmd, 12);
	if (rtStatus == false)
	{
		RT_TRACE(COMP_POWER_TRACKING, "Set configuration with tx cmd queue fail!\n");
	}
#else
	cmpk_message_handle_tx(dev, (u8*)&tx_cmd,
								DESC_PACKET_TYPE_INIT, sizeof(DCMD_TXCMD_T));
#endif
	mdelay(1);
	//DbgPrint("hi, vivi, strange\n");
	for(i = 0;i <= 30; i++)
	{
		Pwr_Flag = read_nic_byte(dev, 0x1ba);

		if (Pwr_Flag == 0)
		{
			mdelay(1);
			continue;
		}
#ifdef RTL8190P
		Avg_TSSI_Meas = read_nic_word(dev, 0x1bc);
#else
		Avg_TSSI_Meas = read_nic_word(dev, 0x13c);
#endif
		if(Avg_TSSI_Meas == 0)
		{
			write_nic_byte(dev, 0x1ba, 0);
			break;
		}

		for(k = 0;k < 5; k++)
		{
#ifdef RTL8190P
			tmp_report[k] = read_nic_byte(dev, 0x1d8+k);
#else
			if(k !=4)
				tmp_report[k] = read_nic_byte(dev, 0x134+k);
			else
				tmp_report[k] = read_nic_byte(dev, 0x13e);
#endif
			RT_TRACE(COMP_POWER_TRACKING, "TSSI_report_value = %d\n", tmp_report[k]);
		}

		//check if the report value is right
		for(k = 0;k < 5; k++)
		{
			if(tmp_report[k] <= 20)
			{
				viviflag =TRUE;
				break;
			}
		}
		if(viviflag ==TRUE)
		{
			write_nic_byte(dev, 0x1ba, 0);
			viviflag = FALSE;
			RT_TRACE(COMP_POWER_TRACKING, "we filted this data\n");
			for(k = 0;k < 5; k++)
				tmp_report[k] = 0;
			break;
		}

		for(k = 0;k < 5; k++)
		{
			Avg_TSSI_Meas_from_driver += tmp_report[k];
		}

		Avg_TSSI_Meas_from_driver = Avg_TSSI_Meas_from_driver*100/5;
		RT_TRACE(COMP_POWER_TRACKING, "Avg_TSSI_Meas_from_driver = %d\n", Avg_TSSI_Meas_from_driver);
		TSSI_13dBm = priv->TSSI_13dBm;
		RT_TRACE(COMP_POWER_TRACKING, "TSSI_13dBm = %d\n", TSSI_13dBm);

		//if(abs(Avg_TSSI_Meas_from_driver - TSSI_13dBm) <= E_FOR_TX_POWER_TRACK)
		// For MacOS-compatible
		if(Avg_TSSI_Meas_from_driver > TSSI_13dBm)
			delta = Avg_TSSI_Meas_from_driver - TSSI_13dBm;
		else
			delta = TSSI_13dBm - Avg_TSSI_Meas_from_driver;

		if(delta <= E_FOR_TX_POWER_TRACK)
		{
			priv->ieee80211->bdynamic_txpower_enable = TRUE;
			write_nic_byte(dev, 0x1ba, 0);
			RT_TRACE(COMP_POWER_TRACKING, "tx power track is done\n");
			RT_TRACE(COMP_POWER_TRACKING, "priv->rfa_txpowertrackingindex = %d\n", priv->rfa_txpowertrackingindex);
			RT_TRACE(COMP_POWER_TRACKING, "priv->rfa_txpowertrackingindex_real = %d\n", priv->rfa_txpowertrackingindex_real);
#ifdef RTL8190P
			RT_TRACE(COMP_POWER_TRACKING, "priv->rfc_txpowertrackingindex = %d\n", priv->rfc_txpowertrackingindex);
			RT_TRACE(COMP_POWER_TRACKING, "priv->rfc_txpowertrackingindex_real = %d\n", priv->rfc_txpowertrackingindex_real);
#endif
			RT_TRACE(COMP_POWER_TRACKING, "priv->cck_present_attentuation_difference = %d\n", priv->cck_present_attentuation_difference);
			RT_TRACE(COMP_POWER_TRACKING, "priv->cck_present_attentuation = %d\n", priv->cck_present_attentuation);
			return;
		}
		else
		{
			if(Avg_TSSI_Meas_from_driver < TSSI_13dBm - E_FOR_TX_POWER_TRACK)
			{
				if((priv->rfa_txpowertrackingindex > 0)
#ifdef RTL8190P
					&&(priv->rfc_txpowertrackingindex > 0)
#endif
				)
				{
					priv->rfa_txpowertrackingindex--;
					if(priv->rfa_txpowertrackingindex_real > 4)
					{
						priv->rfa_txpowertrackingindex_real--;
						rtl8192_setBBreg(dev, rOFDM0_XATxIQImbalance, bMaskDWord, priv->txbbgain_table[priv->rfa_txpowertrackingindex_real].txbbgain_value);
					}
#ifdef RTL8190P
					priv->rfc_txpowertrackingindex--;
					if(priv->rfc_txpowertrackingindex_real > 4)
					{
						priv->rfc_txpowertrackingindex_real--;
						rtl8192_setBBreg(dev, rOFDM0_XCTxIQImbalance, bMaskDWord, priv->txbbgain_table[priv->rfc_txpowertrackingindex_real].txbbgain_value);
					}
#endif
				}
			}
			else
			{
				if((priv->rfa_txpowertrackingindex < 36)
#ifdef RTL8190P
					&&(priv->rfc_txpowertrackingindex < 36)
#endif
					)
				{
					priv->rfa_txpowertrackingindex++;
					priv->rfa_txpowertrackingindex_real++;
					rtl8192_setBBreg(dev, rOFDM0_XATxIQImbalance, bMaskDWord, priv->txbbgain_table[priv->rfa_txpowertrackingindex_real].txbbgain_value);

#ifdef RTL8190P
					priv->rfc_txpowertrackingindex++;
					priv->rfc_txpowertrackingindex_real++;
					rtl8192_setBBreg(dev, rOFDM0_XCTxIQImbalance, bMaskDWord, priv->txbbgain_table[priv->rfc_txpowertrackingindex_real].txbbgain_value);
#endif
				}
			}
			priv->cck_present_attentuation_difference
				= priv->rfa_txpowertrackingindex - priv->rfa_txpowertracking_default;

			if(priv->CurrentChannelBW == HT_CHANNEL_WIDTH_20)
				priv->cck_present_attentuation
				= priv->cck_present_attentuation_20Mdefault + priv->cck_present_attentuation_difference;
			else
				priv->cck_present_attentuation
				= priv->cck_present_attentuation_40Mdefault + priv->cck_present_attentuation_difference;

			if(priv->cck_present_attentuation > -1&&priv->cck_present_attentuation <23)
			{
				if(priv->ieee80211->current_network.channel == 14 && !priv->bcck_in_ch14)
				{
					priv->bcck_in_ch14 = TRUE;
					dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);
				}
				else if(priv->ieee80211->current_network.channel != 14 && priv->bcck_in_ch14)
				{
					priv->bcck_in_ch14 = FALSE;
					dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);
				}
				else
					dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);
			}
		RT_TRACE(COMP_POWER_TRACKING, "priv->rfa_txpowertrackingindex = %d\n", priv->rfa_txpowertrackingindex);
		RT_TRACE(COMP_POWER_TRACKING, "priv->rfa_txpowertrackingindex_real = %d\n", priv->rfa_txpowertrackingindex_real);
#ifdef RTL8190P
		RT_TRACE(COMP_POWER_TRACKING, "priv->rfc_txpowertrackingindex = %d\n", priv->rfc_txpowertrackingindex);
		RT_TRACE(COMP_POWER_TRACKING, "priv->rfc_txpowertrackingindex_real = %d\n", priv->rfc_txpowertrackingindex_real);
#endif
		RT_TRACE(COMP_POWER_TRACKING, "priv->cck_present_attentuation_difference = %d\n", priv->cck_present_attentuation_difference);
		RT_TRACE(COMP_POWER_TRACKING, "priv->cck_present_attentuation = %d\n", priv->cck_present_attentuation);

		if (priv->cck_present_attentuation_difference <= -12||priv->cck_present_attentuation_difference >= 24)
		{
			priv->ieee80211->bdynamic_txpower_enable = TRUE;
			write_nic_byte(dev, 0x1ba, 0);
			RT_TRACE(COMP_POWER_TRACKING, "tx power track--->limited\n");
			return;
		}


	}
		write_nic_byte(dev, 0x1ba, 0);
		Avg_TSSI_Meas_from_driver = 0;
		for(k = 0;k < 5; k++)
			tmp_report[k] = 0;
		break;
	}
}
		priv->ieee80211->bdynamic_txpower_enable = TRUE;
		write_nic_byte(dev, 0x1ba, 0);
}

static void dm_TXPowerTrackingCallback_ThermalMeter(struct net_device * dev)
{
#define ThermalMeterVal	9
	struct r8192_priv *priv = ieee80211_priv(dev);
	u32 tmpRegA, TempCCk;
	u8 tmpOFDMindex, tmpCCKindex, tmpCCK20Mindex, tmpCCK40Mindex, tmpval;
	int i =0, CCKSwingNeedUpdate=0;

	if(!priv->btxpower_trackingInit)
	{
		//Query OFDM default setting
		tmpRegA= rtl8192_QueryBBReg(dev, rOFDM0_XATxIQImbalance, bMaskDWord);
		for(i=0; i<OFDM_Table_Length; i++)	//find the index
		{
			if(tmpRegA == OFDMSwingTable[i])
			{
				priv->OFDM_index= (u8)i;
				RT_TRACE(COMP_POWER_TRACKING, "Initial reg0x%x = 0x%x, OFDM_index=0x%x\n",
					rOFDM0_XATxIQImbalance, tmpRegA, priv->OFDM_index);
			}
		}

		//Query CCK default setting From 0xa22
		TempCCk = rtl8192_QueryBBReg(dev, rCCK0_TxFilter1, bMaskByte2);
		for(i=0 ; i<CCK_Table_length ; i++)
		{
			if(TempCCk == (u32)CCKSwingTable_Ch1_Ch13[i][0])
			{
				priv->CCK_index =(u8) i;
				RT_TRACE(COMP_POWER_TRACKING, "Initial reg0x%x = 0x%x, CCK_index=0x%x\n",
					rCCK0_TxFilter1, TempCCk, priv->CCK_index);
				break;
			}
		}
		priv->btxpower_trackingInit = TRUE;
		//pHalData->TXPowercount = 0;
		return;
	}

	// read and filter out unreasonable value
	tmpRegA = rtl8192_phy_QueryRFReg(dev, RF90_PATH_A, 0x12, 0x078);	// 0x12: RF Reg[10:7]
	RT_TRACE(COMP_POWER_TRACKING, "Readback ThermalMeterA = %d \n", tmpRegA);
	if(tmpRegA < 3 || tmpRegA > 13)
		return;
	if(tmpRegA >= 12)	// if over 12, TP will be bad when high temprature
		tmpRegA = 12;
	RT_TRACE(COMP_POWER_TRACKING, "Valid ThermalMeterA = %d \n", tmpRegA);
	priv->ThermalMeter[0] = ThermalMeterVal;	//We use fixed value by Bryant's suggestion
	priv->ThermalMeter[1] = ThermalMeterVal;	//We use fixed value by Bryant's suggestion

	//Get current RF-A temprature index
	if(priv->ThermalMeter[0] >= (u8)tmpRegA)	//lower temprature
	{
		tmpOFDMindex = tmpCCK20Mindex = 6+(priv->ThermalMeter[0]-(u8)tmpRegA);
		tmpCCK40Mindex = tmpCCK20Mindex - 6;
		if(tmpOFDMindex >= OFDM_Table_Length)
			tmpOFDMindex = OFDM_Table_Length-1;
		if(tmpCCK20Mindex >= CCK_Table_length)
			tmpCCK20Mindex = CCK_Table_length-1;
		if(tmpCCK40Mindex >= CCK_Table_length)
			tmpCCK40Mindex = CCK_Table_length-1;
	}
	else
	{
		tmpval = ((u8)tmpRegA - priv->ThermalMeter[0]);
		if(tmpval >= 6)								// higher temprature
			tmpOFDMindex = tmpCCK20Mindex = 0;		// max to +6dB
		else
			tmpOFDMindex = tmpCCK20Mindex = 6 - tmpval;
		tmpCCK40Mindex = 0;
	}
	//DbgPrint("%ddb, tmpOFDMindex = %d, tmpCCK20Mindex = %d, tmpCCK40Mindex = %d",
		//((u1Byte)tmpRegA - pHalData->ThermalMeter[0]),
		//tmpOFDMindex, tmpCCK20Mindex, tmpCCK40Mindex);
	if(priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)	//40M
		tmpCCKindex = tmpCCK40Mindex;
	else
		tmpCCKindex = tmpCCK20Mindex;

	if(priv->ieee80211->current_network.channel == 14 && !priv->bcck_in_ch14)
	{
		priv->bcck_in_ch14 = TRUE;
		CCKSwingNeedUpdate = 1;
	}
	else if(priv->ieee80211->current_network.channel != 14 && priv->bcck_in_ch14)
	{
		priv->bcck_in_ch14 = FALSE;
		CCKSwingNeedUpdate = 1;
	}

	if(priv->CCK_index != tmpCCKindex)
	{
		priv->CCK_index = tmpCCKindex;
		CCKSwingNeedUpdate = 1;
	}

	if(CCKSwingNeedUpdate)
	{
		//DbgPrint("Update CCK Swing, CCK_index = %d\n", pHalData->CCK_index);
		dm_cck_txpower_adjust(dev, priv->bcck_in_ch14);
	}
	if(priv->OFDM_index != tmpOFDMindex)
	{
		priv->OFDM_index = tmpOFDMindex;
		rtl8192_setBBreg(dev, rOFDM0_XATxIQImbalance, bMaskDWord, OFDMSwingTable[priv->OFDM_index]);
		RT_TRACE(COMP_POWER_TRACKING, "Update OFDMSwing[%d] = 0x%x\n",
			priv->OFDM_index, OFDMSwingTable[priv->OFDM_index]);
	}
	priv->txpower_count = 0;
}

extern	void	dm_txpower_trackingcallback(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work,struct delayed_work,work);
       struct r8192_priv *priv = container_of(dwork,struct r8192_priv,txpower_tracking_wq);
       struct net_device *dev = priv->ieee80211->dev;

#ifdef RTL8190P
	dm_TXPowerTrackingCallback_TSSI(dev);
#else
	if(priv->bDcut == TRUE)
		dm_TXPowerTrackingCallback_TSSI(dev);
	else
		dm_TXPowerTrackingCallback_ThermalMeter(dev);
#endif
}


static void dm_InitializeTXPowerTracking_TSSI(struct net_device *dev)
{

	struct r8192_priv *priv = ieee80211_priv(dev);

	//Initial the Tx BB index and mapping value
	priv->txbbgain_table[0].txbb_iq_amplifygain = 	 		12;
	priv->txbbgain_table[0].txbbgain_value=0x7f8001fe;
	priv->txbbgain_table[1].txbb_iq_amplifygain = 	 		11;
	priv->txbbgain_table[1].txbbgain_value=0x788001e2;
	priv->txbbgain_table[2].txbb_iq_amplifygain = 	 		10;
	priv->txbbgain_table[2].txbbgain_value=0x71c001c7;
	priv->txbbgain_table[3].txbb_iq_amplifygain = 	 		9;
	priv->txbbgain_table[3].txbbgain_value=0x6b8001ae;
	priv->txbbgain_table[4].txbb_iq_amplifygain = 		       8;
	priv->txbbgain_table[4].txbbgain_value=0x65400195;
	priv->txbbgain_table[5].txbb_iq_amplifygain = 		       7;
	priv->txbbgain_table[5].txbbgain_value=0x5fc0017f;
	priv->txbbgain_table[6].txbb_iq_amplifygain = 		       6;
	priv->txbbgain_table[6].txbbgain_value=0x5a400169;
	priv->txbbgain_table[7].txbb_iq_amplifygain = 		       5;
	priv->txbbgain_table[7].txbbgain_value=0x55400155;
	priv->txbbgain_table[8].txbb_iq_amplifygain = 		       4;
	priv->txbbgain_table[8].txbbgain_value=0x50800142;
	priv->txbbgain_table[9].txbb_iq_amplifygain = 		       3;
	priv->txbbgain_table[9].txbbgain_value=0x4c000130;
	priv->txbbgain_table[10].txbb_iq_amplifygain = 		       2;
	priv->txbbgain_table[10].txbbgain_value=0x47c0011f;
	priv->txbbgain_table[11].txbb_iq_amplifygain = 		       1;
	priv->txbbgain_table[11].txbbgain_value=0x43c0010f;
	priv->txbbgain_table[12].txbb_iq_amplifygain = 		       0;
	priv->txbbgain_table[12].txbbgain_value=0x40000100;
	priv->txbbgain_table[13].txbb_iq_amplifygain = 		       -1;
	priv->txbbgain_table[13].txbbgain_value=0x3c8000f2;
	priv->txbbgain_table[14].txbb_iq_amplifygain = 		     -2;
	priv->txbbgain_table[14].txbbgain_value=0x390000e4;
	priv->txbbgain_table[15].txbb_iq_amplifygain = 		     -3;
	priv->txbbgain_table[15].txbbgain_value=0x35c000d7;
	priv->txbbgain_table[16].txbb_iq_amplifygain = 		     -4;
	priv->txbbgain_table[16].txbbgain_value=0x32c000cb;
	priv->txbbgain_table[17].txbb_iq_amplifygain = 		     -5;
	priv->txbbgain_table[17].txbbgain_value=0x300000c0;
	priv->txbbgain_table[18].txbb_iq_amplifygain = 		 	    -6;
	priv->txbbgain_table[18].txbbgain_value=0x2d4000b5;
	priv->txbbgain_table[19].txbb_iq_amplifygain = 		     -7;
	priv->txbbgain_table[19].txbbgain_value=0x2ac000ab;
	priv->txbbgain_table[20].txbb_iq_amplifygain = 		     -8;
	priv->txbbgain_table[20].txbbgain_value=0x288000a2;
	priv->txbbgain_table[21].txbb_iq_amplifygain = 		     -9;
	priv->txbbgain_table[21].txbbgain_value=0x26000098;
	priv->txbbgain_table[22].txbb_iq_amplifygain = 		     -10;
	priv->txbbgain_table[22].txbbgain_value=0x24000090;
	priv->txbbgain_table[23].txbb_iq_amplifygain = 		     -11;
	priv->txbbgain_table[23].txbbgain_value=0x22000088;
	priv->txbbgain_table[24].txbb_iq_amplifygain = 		     -12;
	priv->txbbgain_table[24].txbbgain_value=0x20000080;
	priv->txbbgain_table[25].txbb_iq_amplifygain = 		     -13;
	priv->txbbgain_table[25].txbbgain_value=0x1a00006c;
	priv->txbbgain_table[26].txbb_iq_amplifygain = 		     -14;
	priv->txbbgain_table[26].txbbgain_value=0x1c800072;
	priv->txbbgain_table[27].txbb_iq_amplifygain = 		     -15;
	priv->txbbgain_table[27].txbbgain_value=0x18000060;
	priv->txbbgain_table[28].txbb_iq_amplifygain = 		     -16;
	priv->txbbgain_table[28].txbbgain_value=0x19800066;
	priv->txbbgain_table[29].txbb_iq_amplifygain = 		     -17;
	priv->txbbgain_table[29].txbbgain_value=0x15800056;
	priv->txbbgain_table[30].txbb_iq_amplifygain = 		     -18;
	priv->txbbgain_table[30].txbbgain_value=0x26c0005b;
	priv->txbbgain_table[31].txbb_iq_amplifygain = 		     -19;
	priv->txbbgain_table[31].txbbgain_value=0x14400051;
	priv->txbbgain_table[32].txbb_iq_amplifygain = 		     -20;
	priv->txbbgain_table[32].txbbgain_value=0x24400051;
	priv->txbbgain_table[33].txbb_iq_amplifygain = 		     -21;
	priv->txbbgain_table[33].txbbgain_value=0x1300004c;
	priv->txbbgain_table[34].txbb_iq_amplifygain = 		     -22;
	priv->txbbgain_table[34].txbbgain_value=0x12000048;
	priv->txbbgain_table[35].txbb_iq_amplifygain = 		     -23;
	priv->txbbgain_table[35].txbbgain_value=0x11000044;
	priv->txbbgain_table[36].txbb_iq_amplifygain = 		     -24;
	priv->txbbgain_table[36].txbbgain_value=0x10000040;

	//ccktxbb_valuearray[0] is 0xA22 [1] is 0xA24 ...[7] is 0xA29
	//This Table is for CH1~CH13
	priv->cck_txbbgain_table[0].ccktxbb_valuearray[0] = 0x36;
	priv->cck_txbbgain_table[0].ccktxbb_valuearray[1] = 0x35;
	priv->cck_txbbgain_table[0].ccktxbb_valuearray[2] = 0x2e;
	priv->cck_txbbgain_table[0].ccktxbb_valuearray[3] = 0x25;
	priv->cck_txbbgain_table[0].ccktxbb_valuearray[4] = 0x1c;
	priv->cck_txbbgain_table[0].ccktxbb_valuearray[5] = 0x12;
	priv->cck_txbbgain_table[0].ccktxbb_valuearray[6] = 0x09;
	priv->cck_txbbgain_table[0].ccktxbb_valuearray[7] = 0x04;

	priv->cck_txbbgain_table[1].ccktxbb_valuearray[0] = 0x33;
	priv->cck_txbbgain_table[1].ccktxbb_valuearray[1] = 0x32;
	priv->cck_txbbgain_table[1].ccktxbb_valuearray[2] = 0x2b;
	priv->cck_txbbgain_table[1].ccktxbb_valuearray[3] = 0x23;
	priv->cck_txbbgain_table[1].ccktxbb_valuearray[4] = 0x1a;
	priv->cck_txbbgain_table[1].ccktxbb_valuearray[5] = 0x11;
	priv->cck_txbbgain_table[1].ccktxbb_valuearray[6] = 0x08;
	priv->cck_txbbgain_table[1].ccktxbb_valuearray[7] = 0x04;

	priv->cck_txbbgain_table[2].ccktxbb_valuearray[0] = 0x30;
	priv->cck_txbbgain_table[2].ccktxbb_valuearray[1] = 0x2f;
	priv->cck_txbbgain_table[2].ccktxbb_valuearray[2] = 0x29;
	priv->cck_txbbgain_table[2].ccktxbb_valuearray[3] = 0x21;
	priv->cck_txbbgain_table[2].ccktxbb_valuearray[4] = 0x19;
	priv->cck_txbbgain_table[2].ccktxbb_valuearray[5] = 0x10;
	priv->cck_txbbgain_table[2].ccktxbb_valuearray[6] = 0x08;
	priv->cck_txbbgain_table[2].ccktxbb_valuearray[7] = 0x03;

	priv->cck_txbbgain_table[3].ccktxbb_valuearray[0] = 0x2d;
	priv->cck_txbbgain_table[3].ccktxbb_valuearray[1] = 0x2d;
	priv->cck_txbbgain_table[3].ccktxbb_valuearray[2] = 0x27;
	priv->cck_txbbgain_table[3].ccktxbb_valuearray[3] = 0x1f;
	priv->cck_txbbgain_table[3].ccktxbb_valuearray[4] = 0x18;
	priv->cck_txbbgain_table[3].ccktxbb_valuearray[5] = 0x0f;
	priv->cck_txbbgain_table[3].ccktxbb_valuearray[6] = 0x08;
	priv->cck_txbbgain_table[3].ccktxbb_valuearray[7] = 0x03;

	priv->cck_txbbgain_table[4].ccktxbb_valuearray[0] = 0x2b;
	priv->cck_txbbgain_table[4].ccktxbb_valuearray[1] = 0x2a;
	priv->cck_txbbgain_table[4].ccktxbb_valuearray[2] = 0x25;
	priv->cck_txbbgain_table[4].ccktxbb_valuearray[3] = 0x1e;
	priv->cck_txbbgain_table[4].ccktxbb_valuearray[4] = 0x16;
	priv->cck_txbbgain_table[4].ccktxbb_valuearray[5] = 0x0e;
	priv->cck_txbbgain_table[4].ccktxbb_valuearray[6] = 0x07;
	priv->cck_txbbgain_table[4].ccktxbb_valuearray[7] = 0x03;

	priv->cck_txbbgain_table[5].ccktxbb_valuearray[0] = 0x28;
	priv->cck_txbbgain_table[5].ccktxbb_valuearray[1] = 0x28;
	priv->cck_txbbgain_table[5].ccktxbb_valuearray[2] = 0x22;
	priv->cck_txbbgain_table[5].ccktxbb_valuearray[3] = 0x1c;
	priv->cck_txbbgain_table[5].ccktxbb_valuearray[4] = 0x15;
	priv->cck_txbbgain_table[5].ccktxbb_valuearray[5] = 0x0d;
	priv->cck_txbbgain_table[5].ccktxbb_valuearray[6] = 0x07;
	priv->cck_txbbgain_table[5].ccktxbb_valuearray[7] = 0x03;

	priv->cck_txbbgain_table[6].ccktxbb_valuearray[0] = 0x26;
	priv->cck_txbbgain_table[6].ccktxbb_valuearray[1] = 0x25;
	priv->cck_txbbgain_table[6].ccktxbb_valuearray[2] = 0x21;
	priv->cck_txbbgain_table[6].ccktxbb_valuearray[3] = 0x1b;
	priv->cck_txbbgain_table[6].ccktxbb_valuearray[4] = 0x14;
	priv->cck_txbbgain_table[6].ccktxbb_valuearray[5] = 0x0d;
	priv->cck_txbbgain_table[6].ccktxbb_valuearray[6] = 0x06;
	priv->cck_txbbgain_table[6].ccktxbb_valuearray[7] = 0x03;

	priv->cck_txbbgain_table[7].ccktxbb_valuearray[0] = 0x24;
	priv->cck_txbbgain_table[7].ccktxbb_valuearray[1] = 0x23;
	priv->cck_txbbgain_table[7].ccktxbb_valuearray[2] = 0x1f;
	priv->cck_txbbgain_table[7].ccktxbb_valuearray[3] = 0x19;
	priv->cck_txbbgain_table[7].ccktxbb_valuearray[4] = 0x13;
	priv->cck_txbbgain_table[7].ccktxbb_valuearray[5] = 0x0c;
	priv->cck_txbbgain_table[7].ccktxbb_valuearray[6] = 0x06;
	priv->cck_txbbgain_table[7].ccktxbb_valuearray[7] = 0x03;

	priv->cck_txbbgain_table[8].ccktxbb_valuearray[0] = 0x22;
	priv->cck_txbbgain_table[8].ccktxbb_valuearray[1] = 0x21;
	priv->cck_txbbgain_table[8].ccktxbb_valuearray[2] = 0x1d;
	priv->cck_txbbgain_table[8].ccktxbb_valuearray[3] = 0x18;
	priv->cck_txbbgain_table[8].ccktxbb_valuearray[4] = 0x11;
	priv->cck_txbbgain_table[8].ccktxbb_valuearray[5] = 0x0b;
	priv->cck_txbbgain_table[8].ccktxbb_valuearray[6] = 0x06;
	priv->cck_txbbgain_table[8].ccktxbb_valuearray[7] = 0x02;

	priv->cck_txbbgain_table[9].ccktxbb_valuearray[0] = 0x20;
	priv->cck_txbbgain_table[9].ccktxbb_valuearray[1] = 0x20;
	priv->cck_txbbgain_table[9].ccktxbb_valuearray[2] = 0x1b;
	priv->cck_txbbgain_table[9].ccktxbb_valuearray[3] = 0x16;
	priv->cck_txbbgain_table[9].ccktxbb_valuearray[4] = 0x11;
	priv->cck_txbbgain_table[9].ccktxbb_valuearray[5] = 0x08;
	priv->cck_txbbgain_table[9].ccktxbb_valuearray[6] = 0x05;
	priv->cck_txbbgain_table[9].ccktxbb_valuearray[7] = 0x02;

	priv->cck_txbbgain_table[10].ccktxbb_valuearray[0] = 0x1f;
	priv->cck_txbbgain_table[10].ccktxbb_valuearray[1] = 0x1e;
	priv->cck_txbbgain_table[10].ccktxbb_valuearray[2] = 0x1a;
	priv->cck_txbbgain_table[10].ccktxbb_valuearray[3] = 0x15;
	priv->cck_txbbgain_table[10].ccktxbb_valuearray[4] = 0x10;
	priv->cck_txbbgain_table[10].ccktxbb_valuearray[5] = 0x0a;
	priv->cck_txbbgain_table[10].ccktxbb_valuearray[6] = 0x05;
	priv->cck_txbbgain_table[10].ccktxbb_valuearray[7] = 0x02;

	priv->cck_txbbgain_table[11].ccktxbb_valuearray[0] = 0x1d;
	priv->cck_txbbgain_table[11].ccktxbb_valuearray[1] = 0x1c;
	priv->cck_txbbgain_table[11].ccktxbb_valuearray[2] = 0x18;
	priv->cck_txbbgain_table[11].ccktxbb_valuearray[3] = 0x14;
	priv->cck_txbbgain_table[11].ccktxbb_valuearray[4] = 0x0f;
	priv->cck_txbbgain_table[11].ccktxbb_valuearray[5] = 0x0a;
	priv->cck_txbbgain_table[11].ccktxbb_valuearray[6] = 0x05;
	priv->cck_txbbgain_table[11].ccktxbb_valuearray[7] = 0x02;

	priv->cck_txbbgain_table[12].ccktxbb_valuearray[0] = 0x1b;
	priv->cck_txbbgain_table[12].ccktxbb_valuearray[1] = 0x1a;
	priv->cck_txbbgain_table[12].ccktxbb_valuearray[2] = 0x17;
	priv->cck_txbbgain_table[12].ccktxbb_valuearray[3] = 0x13;
	priv->cck_txbbgain_table[12].ccktxbb_valuearray[4] = 0x0e;
	priv->cck_txbbgain_table[12].ccktxbb_valuearray[5] = 0x09;
	priv->cck_txbbgain_table[12].ccktxbb_valuearray[6] = 0x04;
	priv->cck_txbbgain_table[12].ccktxbb_valuearray[7] = 0x02;

	priv->cck_txbbgain_table[13].ccktxbb_valuearray[0] = 0x1a;
	priv->cck_txbbgain_table[13].ccktxbb_valuearray[1] = 0x19;
	priv->cck_txbbgain_table[13].ccktxbb_valuearray[2] = 0x16;
	priv->cck_txbbgain_table[13].ccktxbb_valuearray[3] = 0x12;
	priv->cck_txbbgain_table[13].ccktxbb_valuearray[4] = 0x0d;
	priv->cck_txbbgain_table[13].ccktxbb_valuearray[5] = 0x09;
	priv->cck_txbbgain_table[13].ccktxbb_valuearray[6] = 0x04;
	priv->cck_txbbgain_table[13].ccktxbb_valuearray[7] = 0x02;

	priv->cck_txbbgain_table[14].ccktxbb_valuearray[0] = 0x18;
	priv->cck_txbbgain_table[14].ccktxbb_valuearray[1] = 0x17;
	priv->cck_txbbgain_table[14].ccktxbb_valuearray[2] = 0x15;
	priv->cck_txbbgain_table[14].ccktxbb_valuearray[3] = 0x11;
	priv->cck_txbbgain_table[14].ccktxbb_valuearray[4] = 0x0c;
	priv->cck_txbbgain_table[14].ccktxbb_valuearray[5] = 0x08;
	priv->cck_txbbgain_table[14].ccktxbb_valuearray[6] = 0x04;
	priv->cck_txbbgain_table[14].ccktxbb_valuearray[7] = 0x02;

	priv->cck_txbbgain_table[15].ccktxbb_valuearray[0] = 0x17;
	priv->cck_txbbgain_table[15].ccktxbb_valuearray[1] = 0x16;
	priv->cck_txbbgain_table[15].ccktxbb_valuearray[2] = 0x13;
	priv->cck_txbbgain_table[15].ccktxbb_valuearray[3] = 0x10;
	priv->cck_txbbgain_table[15].ccktxbb_valuearray[4] = 0x0c;
	priv->cck_txbbgain_table[15].ccktxbb_valuearray[5] = 0x08;
	priv->cck_txbbgain_table[15].ccktxbb_valuearray[6] = 0x04;
	priv->cck_txbbgain_table[15].ccktxbb_valuearray[7] = 0x02;

	priv->cck_txbbgain_table[16].ccktxbb_valuearray[0] = 0x16;
	priv->cck_txbbgain_table[16].ccktxbb_valuearray[1] = 0x15;
	priv->cck_txbbgain_table[16].ccktxbb_valuearray[2] = 0x12;
	priv->cck_txbbgain_table[16].ccktxbb_valuearray[3] = 0x0f;
	priv->cck_txbbgain_table[16].ccktxbb_valuearray[4] = 0x0b;
	priv->cck_txbbgain_table[16].ccktxbb_valuearray[5] = 0x07;
	priv->cck_txbbgain_table[16].ccktxbb_valuearray[6] = 0x04;
	priv->cck_txbbgain_table[16].ccktxbb_valuearray[7] = 0x01;

	priv->cck_txbbgain_table[17].ccktxbb_valuearray[0] = 0x14;
	priv->cck_txbbgain_table[17].ccktxbb_valuearray[1] = 0x14;
	priv->cck_txbbgain_table[17].ccktxbb_valuearray[2] = 0x11;
	priv->cck_txbbgain_table[17].ccktxbb_valuearray[3] = 0x0e;
	priv->cck_txbbgain_table[17].ccktxbb_valuearray[4] = 0x0b;
	priv->cck_txbbgain_table[17].ccktxbb_valuearray[5] = 0x07;
	priv->cck_txbbgain_table[17].ccktxbb_valuearray[6] = 0x03;
	priv->cck_txbbgain_table[17].ccktxbb_valuearray[7] = 0x02;

	priv->cck_txbbgain_table[18].ccktxbb_valuearray[0] = 0x13;
	priv->cck_txbbgain_table[18].ccktxbb_valuearray[1] = 0x13;
	priv->cck_txbbgain_table[18].ccktxbb_valuearray[2] = 0x10;
	priv->cck_txbbgain_table[18].ccktxbb_valuearray[3] = 0x0d;
	priv->cck_txbbgain_table[18].ccktxbb_valuearray[4] = 0x0a;
	priv->cck_txbbgain_table[18].ccktxbb_valuearray[5] = 0x06;
	priv->cck_txbbgain_table[18].ccktxbb_valuearray[6] = 0x03;
	priv->cck_txbbgain_table[18].ccktxbb_valuearray[7] = 0x01;

	priv->cck_txbbgain_table[19].ccktxbb_valuearray[0] = 0x12;
	priv->cck_txbbgain_table[19].ccktxbb_valuearray[1] = 0x12;
	priv->cck_txbbgain_table[19].ccktxbb_valuearray[2] = 0x0f;
	priv->cck_txbbgain_table[19].ccktxbb_valuearray[3] = 0x0c;
	priv->cck_txbbgain_table[19].ccktxbb_valuearray[4] = 0x09;
	priv->cck_txbbgain_table[19].ccktxbb_valuearray[5] = 0x06;
	priv->cck_txbbgain_table[19].ccktxbb_valuearray[6] = 0x03;
	priv->cck_txbbgain_table[19].ccktxbb_valuearray[7] = 0x01;

	priv->cck_txbbgain_table[20].ccktxbb_valuearray[0] = 0x11;
	priv->cck_txbbgain_table[20].ccktxbb_valuearray[1] = 0x11;
	priv->cck_txbbgain_table[20].ccktxbb_valuearray[2] = 0x0f;
	priv->cck_txbbgain_table[20].ccktxbb_valuearray[3] = 0x0c;
	priv->cck_txbbgain_table[20].ccktxbb_valuearray[4] = 0x09;
	priv->cck_txbbgain_table[20].ccktxbb_valuearray[5] = 0x06;
	priv->cck_txbbgain_table[20].ccktxbb_valuearray[6] = 0x03;
	priv->cck_txbbgain_table[20].ccktxbb_valuearray[7] = 0x01;

	priv->cck_txbbgain_table[21].ccktxbb_valuearray[0] = 0x10;
	priv->cck_txbbgain_table[21].ccktxbb_valuearray[1] = 0x10;
	priv->cck_txbbgain_table[21].ccktxbb_valuearray[2] = 0x0e;
	priv->cck_txbbgain_table[21].ccktxbb_valuearray[3] = 0x0b;
	priv->cck_txbbgain_table[21].ccktxbb_valuearray[4] = 0x08;
	priv->cck_txbbgain_table[21].ccktxbb_valuearray[5] = 0x05;
	priv->cck_txbbgain_table[21].ccktxbb_valuearray[6] = 0x03;
	priv->cck_txbbgain_table[21].ccktxbb_valuearray[7] = 0x01;

	priv->cck_txbbgain_table[22].ccktxbb_valuearray[0] = 0x0f;
	priv->cck_txbbgain_table[22].ccktxbb_valuearray[1] = 0x0f;
	priv->cck_txbbgain_table[22].ccktxbb_valuearray[2] = 0x0d;
	priv->cck_txbbgain_table[22].ccktxbb_valuearray[3] = 0x0b;
	priv->cck_txbbgain_table[22].ccktxbb_valuearray[4] = 0x08;
	priv->cck_txbbgain_table[22].ccktxbb_valuearray[5] = 0x05;
	priv->cck_txbbgain_table[22].ccktxbb_valuearray[6] = 0x03;
	priv->cck_txbbgain_table[22].ccktxbb_valuearray[7] = 0x01;

	//ccktxbb_valuearray[0] is 0xA22 [1] is 0xA24 ...[7] is 0xA29
	//This Table is for CH14
	priv->cck_txbbgain_ch14_table[0].ccktxbb_valuearray[0] = 0x36;
	priv->cck_txbbgain_ch14_table[0].ccktxbb_valuearray[1] = 0x35;
	priv->cck_txbbgain_ch14_table[0].ccktxbb_valuearray[2] = 0x2e;
	priv->cck_txbbgain_ch14_table[0].ccktxbb_valuearray[3] = 0x1b;
	priv->cck_txbbgain_ch14_table[0].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[0].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[0].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[0].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[1].ccktxbb_valuearray[0] = 0x33;
	priv->cck_txbbgain_ch14_table[1].ccktxbb_valuearray[1] = 0x32;
	priv->cck_txbbgain_ch14_table[1].ccktxbb_valuearray[2] = 0x2b;
	priv->cck_txbbgain_ch14_table[1].ccktxbb_valuearray[3] = 0x19;
	priv->cck_txbbgain_ch14_table[1].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[1].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[1].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[1].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[2].ccktxbb_valuearray[0] = 0x30;
	priv->cck_txbbgain_ch14_table[2].ccktxbb_valuearray[1] = 0x2f;
	priv->cck_txbbgain_ch14_table[2].ccktxbb_valuearray[2] = 0x29;
	priv->cck_txbbgain_ch14_table[2].ccktxbb_valuearray[3] = 0x18;
	priv->cck_txbbgain_ch14_table[2].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[2].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[2].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[2].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[3].ccktxbb_valuearray[0] = 0x2d;
	priv->cck_txbbgain_ch14_table[3].ccktxbb_valuearray[1] = 0x2d;
	priv->cck_txbbgain_ch14_table[3].ccktxbb_valuearray[2] = 0x27;
	priv->cck_txbbgain_ch14_table[3].ccktxbb_valuearray[3] = 0x17;
	priv->cck_txbbgain_ch14_table[3].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[3].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[3].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[3].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[4].ccktxbb_valuearray[0] = 0x2b;
	priv->cck_txbbgain_ch14_table[4].ccktxbb_valuearray[1] = 0x2a;
	priv->cck_txbbgain_ch14_table[4].ccktxbb_valuearray[2] = 0x25;
	priv->cck_txbbgain_ch14_table[4].ccktxbb_valuearray[3] = 0x15;
	priv->cck_txbbgain_ch14_table[4].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[4].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[4].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[4].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[5].ccktxbb_valuearray[0] = 0x28;
	priv->cck_txbbgain_ch14_table[5].ccktxbb_valuearray[1] = 0x28;
	priv->cck_txbbgain_ch14_table[5].ccktxbb_valuearray[2] = 0x22;
	priv->cck_txbbgain_ch14_table[5].ccktxbb_valuearray[3] = 0x14;
	priv->cck_txbbgain_ch14_table[5].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[5].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[5].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[5].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[6].ccktxbb_valuearray[0] = 0x26;
	priv->cck_txbbgain_ch14_table[6].ccktxbb_valuearray[1] = 0x25;
	priv->cck_txbbgain_ch14_table[6].ccktxbb_valuearray[2] = 0x21;
	priv->cck_txbbgain_ch14_table[6].ccktxbb_valuearray[3] = 0x13;
	priv->cck_txbbgain_ch14_table[6].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[6].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[6].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[6].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[7].ccktxbb_valuearray[0] = 0x24;
	priv->cck_txbbgain_ch14_table[7].ccktxbb_valuearray[1] = 0x23;
	priv->cck_txbbgain_ch14_table[7].ccktxbb_valuearray[2] = 0x1f;
	priv->cck_txbbgain_ch14_table[7].ccktxbb_valuearray[3] = 0x12;
	priv->cck_txbbgain_ch14_table[7].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[7].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[7].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[7].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[8].ccktxbb_valuearray[0] = 0x22;
	priv->cck_txbbgain_ch14_table[8].ccktxbb_valuearray[1] = 0x21;
	priv->cck_txbbgain_ch14_table[8].ccktxbb_valuearray[2] = 0x1d;
	priv->cck_txbbgain_ch14_table[8].ccktxbb_valuearray[3] = 0x11;
	priv->cck_txbbgain_ch14_table[8].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[8].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[8].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[8].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[9].ccktxbb_valuearray[0] = 0x20;
	priv->cck_txbbgain_ch14_table[9].ccktxbb_valuearray[1] = 0x20;
	priv->cck_txbbgain_ch14_table[9].ccktxbb_valuearray[2] = 0x1b;
	priv->cck_txbbgain_ch14_table[9].ccktxbb_valuearray[3] = 0x10;
	priv->cck_txbbgain_ch14_table[9].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[9].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[9].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[9].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[10].ccktxbb_valuearray[0] = 0x1f;
	priv->cck_txbbgain_ch14_table[10].ccktxbb_valuearray[1] = 0x1e;
	priv->cck_txbbgain_ch14_table[10].ccktxbb_valuearray[2] = 0x1a;
	priv->cck_txbbgain_ch14_table[10].ccktxbb_valuearray[3] = 0x0f;
	priv->cck_txbbgain_ch14_table[10].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[10].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[10].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[10].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[11].ccktxbb_valuearray[0] = 0x1d;
	priv->cck_txbbgain_ch14_table[11].ccktxbb_valuearray[1] = 0x1c;
	priv->cck_txbbgain_ch14_table[11].ccktxbb_valuearray[2] = 0x18;
	priv->cck_txbbgain_ch14_table[11].ccktxbb_valuearray[3] = 0x0e;
	priv->cck_txbbgain_ch14_table[11].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[11].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[11].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[11].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[12].ccktxbb_valuearray[0] = 0x1b;
	priv->cck_txbbgain_ch14_table[12].ccktxbb_valuearray[1] = 0x1a;
	priv->cck_txbbgain_ch14_table[12].ccktxbb_valuearray[2] = 0x17;
	priv->cck_txbbgain_ch14_table[12].ccktxbb_valuearray[3] = 0x0e;
	priv->cck_txbbgain_ch14_table[12].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[12].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[12].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[12].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[13].ccktxbb_valuearray[0] = 0x1a;
	priv->cck_txbbgain_ch14_table[13].ccktxbb_valuearray[1] = 0x19;
	priv->cck_txbbgain_ch14_table[13].ccktxbb_valuearray[2] = 0x16;
	priv->cck_txbbgain_ch14_table[13].ccktxbb_valuearray[3] = 0x0d;
	priv->cck_txbbgain_ch14_table[13].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[13].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[13].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[13].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[14].ccktxbb_valuearray[0] = 0x18;
	priv->cck_txbbgain_ch14_table[14].ccktxbb_valuearray[1] = 0x17;
	priv->cck_txbbgain_ch14_table[14].ccktxbb_valuearray[2] = 0x15;
	priv->cck_txbbgain_ch14_table[14].ccktxbb_valuearray[3] = 0x0c;
	priv->cck_txbbgain_ch14_table[14].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[14].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[14].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[14].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[15].ccktxbb_valuearray[0] = 0x17;
	priv->cck_txbbgain_ch14_table[15].ccktxbb_valuearray[1] = 0x16;
	priv->cck_txbbgain_ch14_table[15].ccktxbb_valuearray[2] = 0x13;
	priv->cck_txbbgain_ch14_table[15].ccktxbb_valuearray[3] = 0x0b;
	priv->cck_txbbgain_ch14_table[15].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[15].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[15].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[15].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[16].ccktxbb_valuearray[0] = 0x16;
	priv->cck_txbbgain_ch14_table[16].ccktxbb_valuearray[1] = 0x15;
	priv->cck_txbbgain_ch14_table[16].ccktxbb_valuearray[2] = 0x12;
	priv->cck_txbbgain_ch14_table[16].ccktxbb_valuearray[3] = 0x0b;
	priv->cck_txbbgain_ch14_table[16].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[16].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[16].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[16].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[17].ccktxbb_valuearray[0] = 0x14;
	priv->cck_txbbgain_ch14_table[17].ccktxbb_valuearray[1] = 0x14;
	priv->cck_txbbgain_ch14_table[17].ccktxbb_valuearray[2] = 0x11;
	priv->cck_txbbgain_ch14_table[17].ccktxbb_valuearray[3] = 0x0a;
	priv->cck_txbbgain_ch14_table[17].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[17].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[17].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[17].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[18].ccktxbb_valuearray[0] = 0x13;
	priv->cck_txbbgain_ch14_table[18].ccktxbb_valuearray[1] = 0x13;
	priv->cck_txbbgain_ch14_table[18].ccktxbb_valuearray[2] = 0x10;
	priv->cck_txbbgain_ch14_table[18].ccktxbb_valuearray[3] = 0x0a;
	priv->cck_txbbgain_ch14_table[18].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[18].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[18].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[18].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[19].ccktxbb_valuearray[0] = 0x12;
	priv->cck_txbbgain_ch14_table[19].ccktxbb_valuearray[1] = 0x12;
	priv->cck_txbbgain_ch14_table[19].ccktxbb_valuearray[2] = 0x0f;
	priv->cck_txbbgain_ch14_table[19].ccktxbb_valuearray[3] = 0x09;
	priv->cck_txbbgain_ch14_table[19].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[19].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[19].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[19].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[20].ccktxbb_valuearray[0] = 0x11;
	priv->cck_txbbgain_ch14_table[20].ccktxbb_valuearray[1] = 0x11;
	priv->cck_txbbgain_ch14_table[20].ccktxbb_valuearray[2] = 0x0f;
	priv->cck_txbbgain_ch14_table[20].ccktxbb_valuearray[3] = 0x09;
	priv->cck_txbbgain_ch14_table[20].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[20].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[20].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[20].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[21].ccktxbb_valuearray[0] = 0x10;
	priv->cck_txbbgain_ch14_table[21].ccktxbb_valuearray[1] = 0x10;
	priv->cck_txbbgain_ch14_table[21].ccktxbb_valuearray[2] = 0x0e;
	priv->cck_txbbgain_ch14_table[21].ccktxbb_valuearray[3] = 0x08;
	priv->cck_txbbgain_ch14_table[21].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[21].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[21].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[21].ccktxbb_valuearray[7] = 0x00;

	priv->cck_txbbgain_ch14_table[22].ccktxbb_valuearray[0] = 0x0f;
	priv->cck_txbbgain_ch14_table[22].ccktxbb_valuearray[1] = 0x0f;
	priv->cck_txbbgain_ch14_table[22].ccktxbb_valuearray[2] = 0x0d;
	priv->cck_txbbgain_ch14_table[22].ccktxbb_valuearray[3] = 0x08;
	priv->cck_txbbgain_ch14_table[22].ccktxbb_valuearray[4] = 0x00;
	priv->cck_txbbgain_ch14_table[22].ccktxbb_valuearray[5] = 0x00;
	priv->cck_txbbgain_ch14_table[22].ccktxbb_valuearray[6] = 0x00;
	priv->cck_txbbgain_ch14_table[22].ccktxbb_valuearray[7] = 0x00;

	priv->btxpower_tracking = TRUE;
	priv->txpower_count       = 0;
	priv->btxpower_trackingInit = FALSE;

}


void dm_initialize_txpower_tracking(struct net_device *dev)
{
#if (defined RTL8190P)
	dm_InitializeTXPowerTracking_TSSI(dev);
#else
	// 2009/01/12 MH Enable for 92S series channel 1-14 CCK tx pwer setting for MP.
	//
	dm_InitializeTXPowerTracking_TSSI(dev);
#endif
}// dm_InitializeTXPowerTracking


static void dm_CheckTXPowerTracking_TSSI(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	static u32 tx_power_track_counter = 0;

	if(!priv->btxpower_tracking)
		return;
	else
	{
	 	if((tx_power_track_counter % 30 == 0)&&(tx_power_track_counter != 0))
	 	{
				queue_delayed_work(priv->priv_wq,&priv->txpower_tracking_wq,0);
	 	}
		tx_power_track_counter++;
	}

}


static void dm_CheckTXPowerTracking_ThermalMeter(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	static u8 	TM_Trigger=0;

	//DbgPrint("dm_CheckTXPowerTracking() \n");
	if(!priv->btxpower_tracking)
		return;
	else
	{
		if(priv->txpower_count  <= 2)
		{
			priv->txpower_count++;
			return;
		}
	}

	if(!TM_Trigger)
	{
		//Attention!! You have to wirte all 12bits data to RF, or it may cause RF to crash
		//actually write reg0x02 bit1=0, then bit1=1.
		//DbgPrint("Trigger ThermalMeter, write RF reg0x2 = 0x4d to 0x4f\n");
		rtl8192_phy_SetRFReg(dev, RF90_PATH_A, 0x02, bRFRegOffsetMask, 0x4d);
		rtl8192_phy_SetRFReg(dev, RF90_PATH_A, 0x02, bRFRegOffsetMask, 0x4f);
		rtl8192_phy_SetRFReg(dev, RF90_PATH_A, 0x02, bRFRegOffsetMask, 0x4d);
		rtl8192_phy_SetRFReg(dev, RF90_PATH_A, 0x02, bRFRegOffsetMask, 0x4f);
		TM_Trigger = 1;
		return;
	}
	else
	{
		//DbgPrint("Schedule TxPowerTrackingWorkItem\n");
			queue_delayed_work(priv->priv_wq,&priv->txpower_tracking_wq,0);
		TM_Trigger = 0;
	}
}


static void dm_check_txpower_tracking(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	//static u32 tx_power_track_counter = 0;

#ifdef  RTL8190P
	dm_CheckTXPowerTracking_TSSI(dev);
#else
	if(priv->bDcut == TRUE)
		dm_CheckTXPowerTracking_TSSI(dev);
	else
		dm_CheckTXPowerTracking_ThermalMeter(dev);
#endif

}	// dm_CheckTXPowerTracking


static void dm_CCKTxPowerAdjust_TSSI(struct net_device *dev, bool  bInCH14)
{
	u32 TempVal;
	struct r8192_priv *priv = ieee80211_priv(dev);
	//Write 0xa22 0xa23
	TempVal = 0;
	if(!bInCH14){
		//Write 0xa22 0xa23
		TempVal = 	priv->cck_txbbgain_table[priv->cck_present_attentuation].ccktxbb_valuearray[0] +
					(priv->cck_txbbgain_table[priv->cck_present_attentuation].ccktxbb_valuearray[1]<<8) ;

		rtl8192_setBBreg(dev, rCCK0_TxFilter1,bMaskHWord, TempVal);
		//Write 0xa24 ~ 0xa27
		TempVal = 0;
		TempVal = 	priv->cck_txbbgain_table[priv->cck_present_attentuation].ccktxbb_valuearray[2] +
					(priv->cck_txbbgain_table[priv->cck_present_attentuation].ccktxbb_valuearray[3]<<8) +
					(priv->cck_txbbgain_table[priv->cck_present_attentuation].ccktxbb_valuearray[4]<<16 )+
					(priv->cck_txbbgain_table[priv->cck_present_attentuation].ccktxbb_valuearray[5]<<24);
		rtl8192_setBBreg(dev, rCCK0_TxFilter2,bMaskDWord, TempVal);
		//Write 0xa28  0xa29
		TempVal = 0;
		TempVal = 	priv->cck_txbbgain_table[priv->cck_present_attentuation].ccktxbb_valuearray[6] +
					(priv->cck_txbbgain_table[priv->cck_present_attentuation].ccktxbb_valuearray[7]<<8) ;

		rtl8192_setBBreg(dev, rCCK0_DebugPort,bMaskLWord, TempVal);
	}
	else
	{
		TempVal = 	priv->cck_txbbgain_ch14_table[priv->cck_present_attentuation].ccktxbb_valuearray[0] +
					(priv->cck_txbbgain_ch14_table[priv->cck_present_attentuation].ccktxbb_valuearray[1]<<8) ;

		rtl8192_setBBreg(dev, rCCK0_TxFilter1,bMaskHWord, TempVal);
		//Write 0xa24 ~ 0xa27
		TempVal = 0;
		TempVal = 	priv->cck_txbbgain_ch14_table[priv->cck_present_attentuation].ccktxbb_valuearray[2] +
					(priv->cck_txbbgain_ch14_table[priv->cck_present_attentuation].ccktxbb_valuearray[3]<<8) +
					(priv->cck_txbbgain_ch14_table[priv->cck_present_attentuation].ccktxbb_valuearray[4]<<16 )+
					(priv->cck_txbbgain_ch14_table[priv->cck_present_attentuation].ccktxbb_valuearray[5]<<24);
		rtl8192_setBBreg(dev, rCCK0_TxFilter2,bMaskDWord, TempVal);
		//Write 0xa28  0xa29
		TempVal = 0;
		TempVal = 	priv->cck_txbbgain_ch14_table[priv->cck_present_attentuation].ccktxbb_valuearray[6] +
					(priv->cck_txbbgain_ch14_table[priv->cck_present_attentuation].ccktxbb_valuearray[7]<<8) ;

		rtl8192_setBBreg(dev, rCCK0_DebugPort,bMaskLWord, TempVal);
	}


}

static void dm_CCKTxPowerAdjust_ThermalMeter(struct net_device *dev,	bool  bInCH14)
{
	u32 TempVal;
	struct r8192_priv *priv = ieee80211_priv(dev);

	TempVal = 0;
	if(!bInCH14)
	{
		//Write 0xa22 0xa23
		TempVal = 	CCKSwingTable_Ch1_Ch13[priv->CCK_index][0] +
					(CCKSwingTable_Ch1_Ch13[priv->CCK_index][1]<<8) ;
		rtl8192_setBBreg(dev, rCCK0_TxFilter1, bMaskHWord, TempVal);
		RT_TRACE(COMP_POWER_TRACKING, "CCK not chnl 14, reg 0x%x = 0x%x\n",
			rCCK0_TxFilter1, TempVal);
		//Write 0xa24 ~ 0xa27
		TempVal = 0;
		TempVal = 	CCKSwingTable_Ch1_Ch13[priv->CCK_index][2] +
					(CCKSwingTable_Ch1_Ch13[priv->CCK_index][3]<<8) +
					(CCKSwingTable_Ch1_Ch13[priv->CCK_index][4]<<16 )+
					(CCKSwingTable_Ch1_Ch13[priv->CCK_index][5]<<24);
		rtl8192_setBBreg(dev, rCCK0_TxFilter2, bMaskDWord, TempVal);
		RT_TRACE(COMP_POWER_TRACKING, "CCK not chnl 14, reg 0x%x = 0x%x\n",
			rCCK0_TxFilter2, TempVal);
		//Write 0xa28  0xa29
		TempVal = 0;
		TempVal = 	CCKSwingTable_Ch1_Ch13[priv->CCK_index][6] +
					(CCKSwingTable_Ch1_Ch13[priv->CCK_index][7]<<8) ;

		rtl8192_setBBreg(dev, rCCK0_DebugPort, bMaskLWord, TempVal);
		RT_TRACE(COMP_POWER_TRACKING, "CCK not chnl 14, reg 0x%x = 0x%x\n",
			rCCK0_DebugPort, TempVal);
	}
	else
	{
//		priv->CCKTxPowerAdjustCntNotCh14++;	//cosa add for debug.
		//Write 0xa22 0xa23
		TempVal = 	CCKSwingTable_Ch14[priv->CCK_index][0] +
					(CCKSwingTable_Ch14[priv->CCK_index][1]<<8) ;

		rtl8192_setBBreg(dev, rCCK0_TxFilter1, bMaskHWord, TempVal);
		RT_TRACE(COMP_POWER_TRACKING, "CCK chnl 14, reg 0x%x = 0x%x\n",
			rCCK0_TxFilter1, TempVal);
		//Write 0xa24 ~ 0xa27
		TempVal = 0;
		TempVal = 	CCKSwingTable_Ch14[priv->CCK_index][2] +
					(CCKSwingTable_Ch14[priv->CCK_index][3]<<8) +
					(CCKSwingTable_Ch14[priv->CCK_index][4]<<16 )+
					(CCKSwingTable_Ch14[priv->CCK_index][5]<<24);
		rtl8192_setBBreg(dev, rCCK0_TxFilter2, bMaskDWord, TempVal);
		RT_TRACE(COMP_POWER_TRACKING, "CCK chnl 14, reg 0x%x = 0x%x\n",
			rCCK0_TxFilter2, TempVal);
		//Write 0xa28  0xa29
		TempVal = 0;
		TempVal = 	CCKSwingTable_Ch14[priv->CCK_index][6] +
					(CCKSwingTable_Ch14[priv->CCK_index][7]<<8) ;

		rtl8192_setBBreg(dev, rCCK0_DebugPort, bMaskLWord, TempVal);
		RT_TRACE(COMP_POWER_TRACKING,"CCK chnl 14, reg 0x%x = 0x%x\n",
			rCCK0_DebugPort, TempVal);
	}
}



extern void dm_cck_txpower_adjust(
	struct net_device *dev,
	bool  binch14
)
{	// dm_CCKTxPowerAdjust

	struct r8192_priv *priv = ieee80211_priv(dev);
#ifdef RTL8190P
	dm_CCKTxPowerAdjust_TSSI(dev, binch14);
#else
	if(priv->bDcut == TRUE)
		dm_CCKTxPowerAdjust_TSSI(dev, binch14);
	else
		dm_CCKTxPowerAdjust_ThermalMeter(dev, binch14);
#endif
}


#ifndef  RTL8192U
static void dm_txpower_reset_recovery(
	struct net_device *dev
)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	RT_TRACE(COMP_POWER_TRACKING, "Start Reset Recovery ==>\n");
	rtl8192_setBBreg(dev, rOFDM0_XATxIQImbalance, bMaskDWord, priv->txbbgain_table[priv->rfa_txpowertrackingindex].txbbgain_value);
	RT_TRACE(COMP_POWER_TRACKING, "Reset Recovery: Fill in 0xc80 is %08x\n",priv->txbbgain_table[priv->rfa_txpowertrackingindex].txbbgain_value);
	RT_TRACE(COMP_POWER_TRACKING, "Reset Recovery: Fill in RFA_txPowerTrackingIndex is %x\n",priv->rfa_txpowertrackingindex);
	RT_TRACE(COMP_POWER_TRACKING, "Reset Recovery : RF A I/Q Amplify Gain is %ld\n",priv->txbbgain_table[priv->rfa_txpowertrackingindex].txbb_iq_amplifygain);
	RT_TRACE(COMP_POWER_TRACKING, "Reset Recovery: CCK Attenuation is %d dB\n",priv->cck_present_attentuation);
	dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);

	rtl8192_setBBreg(dev, rOFDM0_XCTxIQImbalance, bMaskDWord, priv->txbbgain_table[priv->rfc_txpowertrackingindex].txbbgain_value);
	RT_TRACE(COMP_POWER_TRACKING, "Reset Recovery: Fill in 0xc90 is %08x\n",priv->txbbgain_table[priv->rfc_txpowertrackingindex].txbbgain_value);
	RT_TRACE(COMP_POWER_TRACKING, "Reset Recovery: Fill in RFC_txPowerTrackingIndex is %x\n",priv->rfc_txpowertrackingindex);
	RT_TRACE(COMP_POWER_TRACKING, "Reset Recovery : RF C I/Q Amplify Gain is %ld\n",priv->txbbgain_table[priv->rfc_txpowertrackingindex].txbb_iq_amplifygain);

}	// dm_TXPowerResetRecovery

extern void dm_restore_dynamic_mechanism_state(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u32 	reg_ratr = priv->rate_adaptive.last_ratr;

	if(!priv->up)
	{
		RT_TRACE(COMP_RATE, "<---- dm_restore_dynamic_mechanism_state(): driver is going to unload\n");
		return;
	}

	//
	// Restore previous state for rate adaptive
	//
	if(priv->rate_adaptive.rate_adaptive_disabled)
		return;
	// TODO: Only 11n mode is implemented currently,
	if( !(priv->ieee80211->mode==WIRELESS_MODE_N_24G ||
		 priv->ieee80211->mode==WIRELESS_MODE_N_5G))
		 return;
	{
			/* 2007/11/15 MH Copy from 8190PCI. */
			u32 ratr_value;
			ratr_value = reg_ratr;
			if(priv->rf_type == RF_1T2R)	// 1T2R, Spatial Stream 2 should be disabled
			{
				ratr_value &=~ (RATE_ALL_OFDM_2SS);
				//DbgPrint("HW_VAR_TATR_0 from 0x%x ==> 0x%x\n", ((pu4Byte)(val))[0], ratr_value);
			}
			//DbgPrint("set HW_VAR_TATR_0 = 0x%x\n", ratr_value);
			//cosa PlatformEFIOWrite4Byte(Adapter, RATR0, ((pu4Byte)(val))[0]);
			write_nic_dword(dev, RATR0, ratr_value);
			write_nic_byte(dev, UFWP, 1);
	}
	//Resore TX Power Tracking Index
	if(priv->btxpower_trackingInit && priv->btxpower_tracking){
		dm_txpower_reset_recovery(dev);
	}

	//
	//Restore BB Initial Gain
	//
	dm_bb_initialgain_restore(dev);

}	// DM_RestoreDynamicMechanismState

static void dm_bb_initialgain_restore(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u32 bit_mask = 0x7f; //Bit0~ Bit6

	if(dm_digtable.dig_algorithm == DIG_ALGO_BY_RSSI)
		return;

	//Disable Initial Gain
	//PHY_SetBBReg(Adapter, UFWP, bMaskLWord, 0x800);
	rtl8192_setBBreg(dev, UFWP, bMaskByte1, 0x8);	// Only clear byte 1 and rewrite.
	rtl8192_setBBreg(dev, rOFDM0_XAAGCCore1, bit_mask, (u32)priv->initgain_backup.xaagccore1);
	rtl8192_setBBreg(dev, rOFDM0_XBAGCCore1, bit_mask, (u32)priv->initgain_backup.xbagccore1);
	rtl8192_setBBreg(dev, rOFDM0_XCAGCCore1, bit_mask, (u32)priv->initgain_backup.xcagccore1);
	rtl8192_setBBreg(dev, rOFDM0_XDAGCCore1, bit_mask, (u32)priv->initgain_backup.xdagccore1);
	bit_mask  = bMaskByte2;
	rtl8192_setBBreg(dev, rCCK0_CCA, bit_mask, (u32)priv->initgain_backup.cca);

	RT_TRACE(COMP_DIG, "dm_BBInitialGainRestore 0xc50 is %x\n",priv->initgain_backup.xaagccore1);
	RT_TRACE(COMP_DIG, "dm_BBInitialGainRestore 0xc58 is %x\n",priv->initgain_backup.xbagccore1);
	RT_TRACE(COMP_DIG, "dm_BBInitialGainRestore 0xc60 is %x\n",priv->initgain_backup.xcagccore1);
	RT_TRACE(COMP_DIG, "dm_BBInitialGainRestore 0xc68 is %x\n",priv->initgain_backup.xdagccore1);
	RT_TRACE(COMP_DIG, "dm_BBInitialGainRestore 0xa0a is %x\n",priv->initgain_backup.cca);
	//Enable Initial Gain
	//PHY_SetBBReg(Adapter, UFWP, bMaskLWord, 0x100);
	rtl8192_setBBreg(dev, UFWP, bMaskByte1, 0x1);	// Only clear byte 1 and rewrite.

}	// dm_BBInitialGainRestore


extern void dm_backup_dynamic_mechanism_state(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	// Fsync to avoid reset
	priv->bswitch_fsync  = false;
	priv->bfsync_processing = false;
	//Backup BB InitialGain
	dm_bb_initialgain_backup(dev);

}	// DM_BackupDynamicMechanismState


static void dm_bb_initialgain_backup(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u32 bit_mask = bMaskByte0; //Bit0~ Bit6

	if(dm_digtable.dig_algorithm == DIG_ALGO_BY_RSSI)
		return;

	//PHY_SetBBReg(Adapter, UFWP, bMaskLWord, 0x800);
	rtl8192_setBBreg(dev, UFWP, bMaskByte1, 0x8);	// Only clear byte 1 and rewrite.
	priv->initgain_backup.xaagccore1 = (u8)rtl8192_QueryBBReg(dev, rOFDM0_XAAGCCore1, bit_mask);
	priv->initgain_backup.xbagccore1 = (u8)rtl8192_QueryBBReg(dev, rOFDM0_XBAGCCore1, bit_mask);
	priv->initgain_backup.xcagccore1 = (u8)rtl8192_QueryBBReg(dev, rOFDM0_XCAGCCore1, bit_mask);
	priv->initgain_backup.xdagccore1 = (u8)rtl8192_QueryBBReg(dev, rOFDM0_XDAGCCore1, bit_mask);
	bit_mask  = bMaskByte2;
	priv->initgain_backup.cca = (u8)rtl8192_QueryBBReg(dev, rCCK0_CCA, bit_mask);

	RT_TRACE(COMP_DIG, "BBInitialGainBackup 0xc50 is %x\n",priv->initgain_backup.xaagccore1);
	RT_TRACE(COMP_DIG, "BBInitialGainBackup 0xc58 is %x\n",priv->initgain_backup.xbagccore1);
	RT_TRACE(COMP_DIG, "BBInitialGainBackup 0xc60 is %x\n",priv->initgain_backup.xcagccore1);
	RT_TRACE(COMP_DIG, "BBInitialGainBackup 0xc68 is %x\n",priv->initgain_backup.xdagccore1);
	RT_TRACE(COMP_DIG, "BBInitialGainBackup 0xa0a is %x\n",priv->initgain_backup.cca);

}   // dm_BBInitialGainBakcup

#endif
/*-----------------------------------------------------------------------------
 * Function:	dm_change_dynamic_initgain_thresh()
 *
 * Overview:
 *
 * Input:		NONE
 *
 * Output:		NONE
 *
 * Return:		NONE
 *
 * Revised History:
 *	When		Who		Remark
 *	05/29/2008	amy		Create Version 0 porting from windows code.
 *
 *---------------------------------------------------------------------------*/
extern void dm_change_dynamic_initgain_thresh(struct net_device *dev,
								u32		dm_type,
								u32		dm_value)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	if(dm_type == DIG_TYPE_THRESH_HIGHPWR_HIGH)
		priv->MidHighPwrTHR_L2 = (u8)dm_value;
	else if(dm_type == DIG_TYPE_THRESH_HIGHPWR_LOW)
		priv->MidHighPwrTHR_L1 = (u8)dm_value;
	return;
	if (dm_type == DIG_TYPE_THRESH_HIGH)
	{
		dm_digtable.rssi_high_thresh = dm_value;
	}
	else if (dm_type == DIG_TYPE_THRESH_LOW)
	{
		dm_digtable.rssi_low_thresh = dm_value;
	}
	else if (dm_type == DIG_TYPE_THRESH_HIGHPWR_HIGH)
	{
		dm_digtable.rssi_high_power_highthresh = dm_value;
	}
	else if (dm_type == DIG_TYPE_THRESH_HIGHPWR_HIGH)
	{
		dm_digtable.rssi_high_power_highthresh = dm_value;
	}
	else if (dm_type == DIG_TYPE_ENABLE)
	{
		dm_digtable.dig_state		= DM_STA_DIG_MAX;
		dm_digtable.dig_enable_flag	= true;
	}
	else if (dm_type == DIG_TYPE_DISABLE)
	{
		dm_digtable.dig_state		= DM_STA_DIG_MAX;
		dm_digtable.dig_enable_flag	= false;
	}
	else if (dm_type == DIG_TYPE_DBG_MODE)
	{
		if(dm_value >= DM_DBG_MAX)
			dm_value = DM_DBG_OFF;
		dm_digtable.dbg_mode		= (u8)dm_value;
	}
	else if (dm_type == DIG_TYPE_RSSI)
	{
		if(dm_value > 100)
			dm_value = 30;
		dm_digtable.rssi_val			= (long)dm_value;
	}
	else if (dm_type == DIG_TYPE_ALGORITHM)
	{
		if (dm_value >= DIG_ALGO_MAX)
			dm_value = DIG_ALGO_BY_FALSE_ALARM;
		if(dm_digtable.dig_algorithm != (u8)dm_value)
			dm_digtable.dig_algorithm_switch = 1;
		dm_digtable.dig_algorithm	= (u8)dm_value;
	}
	else if (dm_type == DIG_TYPE_BACKOFF)
	{
		if(dm_value > 30)
			dm_value = 30;
		dm_digtable.backoff_val		= (u8)dm_value;
	}
	else if(dm_type == DIG_TYPE_RX_GAIN_MIN)
	{
		if(dm_value == 0)
			dm_value = 0x1;
		dm_digtable.rx_gain_range_min = (u8)dm_value;
	}
	else if(dm_type == DIG_TYPE_RX_GAIN_MAX)
	{
		if(dm_value > 0x50)
			dm_value = 0x50;
		dm_digtable.rx_gain_range_max = (u8)dm_value;
	}
}	/* DM_ChangeDynamicInitGainThresh */
extern	void
dm_change_fsync_setting(
	struct net_device *dev,
	s32		DM_Type,
	s32		DM_Value)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	if (DM_Type == 0)	// monitor 0xc38 register
	{
		if(DM_Value > 1)
			DM_Value = 1;
		priv->framesyncMonitor = (u8)DM_Value;
		//DbgPrint("pHalData->framesyncMonitor = %d", pHalData->framesyncMonitor);
	}
}

extern void
dm_change_rxpath_selection_setting(
	struct net_device *dev,
	s32		DM_Type,
	s32		DM_Value)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	prate_adaptive 	pRA = (prate_adaptive)&(priv->rate_adaptive);


	if(DM_Type == 0)
	{
		if(DM_Value > 1)
			DM_Value = 1;
		DM_RxPathSelTable.Enable = (u8)DM_Value;
	}
	else if(DM_Type == 1)
	{
		if(DM_Value > 1)
			DM_Value = 1;
		DM_RxPathSelTable.DbgMode = (u8)DM_Value;
	}
	else if(DM_Type == 2)
	{
		if(DM_Value > 40)
			DM_Value = 40;
		DM_RxPathSelTable.SS_TH_low = (u8)DM_Value;
	}
	else if(DM_Type == 3)
	{
		if(DM_Value > 25)
			DM_Value = 25;
		DM_RxPathSelTable.diff_TH = (u8)DM_Value;
	}
	else if(DM_Type == 4)
	{
		if(DM_Value >= CCK_Rx_Version_MAX)
			DM_Value = CCK_Rx_Version_1;
		DM_RxPathSelTable.cck_method= (u8)DM_Value;
	}
	else if(DM_Type == 10)
	{
		if(DM_Value > 100)
			DM_Value = 50;
		DM_RxPathSelTable.rf_rssi[0] = (u8)DM_Value;
	}
	else if(DM_Type == 11)
	{
		if(DM_Value > 100)
			DM_Value = 50;
		DM_RxPathSelTable.rf_rssi[1] = (u8)DM_Value;
	}
	else if(DM_Type == 12)
	{
		if(DM_Value > 100)
			DM_Value = 50;
		DM_RxPathSelTable.rf_rssi[2] = (u8)DM_Value;
	}
	else if(DM_Type == 13)
	{
		if(DM_Value > 100)
			DM_Value = 50;
		DM_RxPathSelTable.rf_rssi[3] = (u8)DM_Value;
	}
	else if(DM_Type == 20)
	{
		if(DM_Value > 1)
			DM_Value = 1;
		pRA->ping_rssi_enable = (u8)DM_Value;
	}
	else if(DM_Type == 21)
	{
		if(DM_Value > 30)
			DM_Value = 30;
		pRA->ping_rssi_thresh_for_ra = DM_Value;
	}
}

/*-----------------------------------------------------------------------------
 * Function:	dm_dig_init()
 *
 * Overview:	Set DIG scheme init value.
 *
 * Input:		NONE
 *
 * Output:		NONE
 *
 * Return:		NONE
 *
 * Revised History:
 *	When		Who		Remark
 *	05/15/2008	amy		Create Version 0 porting from windows code.
 *
 *---------------------------------------------------------------------------*/
static void dm_dig_init(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	/* 2007/10/05 MH Disable DIG scheme now. Not tested. */
	dm_digtable.dig_enable_flag	= true;
	dm_digtable.dig_algorithm = DIG_ALGO_BY_RSSI;
	dm_digtable.dbg_mode = DM_DBG_OFF;	//off=by real rssi value, on=by DM_DigTable.Rssi_val for new dig
	dm_digtable.dig_algorithm_switch = 0;

	/* 2007/10/04 MH Define init gain threshol. */
	dm_digtable.dig_state		= DM_STA_DIG_MAX;
	dm_digtable.dig_highpwr_state	= DM_STA_DIG_MAX;
	dm_digtable.initialgain_lowerbound_state = false;

	dm_digtable.rssi_low_thresh 	= DM_DIG_THRESH_LOW;
	dm_digtable.rssi_high_thresh 	= DM_DIG_THRESH_HIGH;

	dm_digtable.rssi_high_power_lowthresh = DM_DIG_HIGH_PWR_THRESH_LOW;
	dm_digtable.rssi_high_power_highthresh = DM_DIG_HIGH_PWR_THRESH_HIGH;

	dm_digtable.rssi_val = 50;	//for new dig debug rssi value
	dm_digtable.backoff_val = DM_DIG_BACKOFF;
	dm_digtable.rx_gain_range_max = DM_DIG_MAX;
	if(priv->CustomerID == RT_CID_819x_Netcore)
		dm_digtable.rx_gain_range_min = DM_DIG_MIN_Netcore;
	else
		dm_digtable.rx_gain_range_min = DM_DIG_MIN;

}	/* dm_dig_init */


/*-----------------------------------------------------------------------------
 * Function:	dm_ctrl_initgain_byrssi()
 *
 * Overview:	Driver must monitor RSSI and notify firmware to change initial
 *				gain according to different threshold. BB team provide the
 *				suggested solution.
 *
 * Input:			struct net_device *dev
 *
 * Output:		NONE
 *
 * Return:		NONE
 *
 * Revised History:
 *	When		Who		Remark
 *	05/27/2008	amy		Create Version 0 porting from windows code.
 *---------------------------------------------------------------------------*/
static void dm_ctrl_initgain_byrssi(struct net_device *dev)
{

	if (dm_digtable.dig_enable_flag == false)
		return;

	if(dm_digtable.dig_algorithm == DIG_ALGO_BY_FALSE_ALARM)
		dm_ctrl_initgain_byrssi_by_fwfalse_alarm(dev);
	else if(dm_digtable.dig_algorithm == DIG_ALGO_BY_RSSI)
		dm_ctrl_initgain_byrssi_by_driverrssi(dev);
//		;
	else
		return;
}


static void dm_ctrl_initgain_byrssi_by_driverrssi(
	struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8 i;
	static u8 	fw_dig=0;

	if (dm_digtable.dig_enable_flag == false)
		return;

	//DbgPrint("Dig by Sw Rssi \n");
	if(dm_digtable.dig_algorithm_switch)	// if swithed algorithm, we have to disable FW Dig.
		fw_dig = 0;
	if(fw_dig <= 3)	// execute several times to make sure the FW Dig is disabled
	{// FW DIG Off
		for(i=0; i<3; i++)
			rtl8192_setBBreg(dev, UFWP, bMaskByte1, 0x8);	// Only clear byte 1 and rewrite.
		fw_dig++;
		dm_digtable.dig_state = DM_STA_DIG_OFF;	//fw dig off.
	}

	if(priv->ieee80211->state == IEEE80211_LINKED)
		dm_digtable.cur_connect_state = DIG_CONNECT;
	else
		dm_digtable.cur_connect_state = DIG_DISCONNECT;

	//DbgPrint("DM_DigTable.PreConnectState = %d, DM_DigTable.CurConnectState = %d \n",
		//DM_DigTable.PreConnectState, DM_DigTable.CurConnectState);

	if(dm_digtable.dbg_mode == DM_DBG_OFF)
		dm_digtable.rssi_val = priv->undecorated_smoothed_pwdb;
	//DbgPrint("DM_DigTable.Rssi_val = %d \n", DM_DigTable.Rssi_val);
	dm_initial_gain(dev);
	dm_pd_th(dev);
	dm_cs_ratio(dev);
	if(dm_digtable.dig_algorithm_switch)
		dm_digtable.dig_algorithm_switch = 0;
	dm_digtable.pre_connect_state = dm_digtable.cur_connect_state;

}	/* dm_CtrlInitGainByRssi */

static void dm_ctrl_initgain_byrssi_by_fwfalse_alarm(
	struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	static u32 reset_cnt = 0;
	u8 i;

	if (dm_digtable.dig_enable_flag == false)
		return;

	if(dm_digtable.dig_algorithm_switch)
	{
		dm_digtable.dig_state = DM_STA_DIG_MAX;
		// Fw DIG On.
		for(i=0; i<3; i++)
			rtl8192_setBBreg(dev, UFWP, bMaskByte1, 0x1);	// Only clear byte 1 and rewrite.
		dm_digtable.dig_algorithm_switch = 0;
	}

	if (priv->ieee80211->state != IEEE80211_LINKED)
		return;

	// For smooth, we can not change DIG state.
	if ((priv->undecorated_smoothed_pwdb > dm_digtable.rssi_low_thresh) &&
		(priv->undecorated_smoothed_pwdb < dm_digtable.rssi_high_thresh))
	{
		return;
	}
	//DbgPrint("Dig by Fw False Alarm\n");
	//if (DM_DigTable.Dig_State == DM_STA_DIG_OFF)
	/*DbgPrint("DIG Check\n\r RSSI=%d LOW=%d HIGH=%d STATE=%d",
	pHalData->UndecoratedSmoothedPWDB, DM_DigTable.RssiLowThresh,
	DM_DigTable.RssiHighThresh, DM_DigTable.Dig_State);*/
	/* 1. When RSSI decrease, We have to judge if it is smaller than a treshold
		  and then execute below step. */
	if ((priv->undecorated_smoothed_pwdb <= dm_digtable.rssi_low_thresh))
	{
		/* 2008/02/05 MH When we execute silent reset, the DIG PHY parameters
		   will be reset to init value. We must prevent the condition. */
		if (dm_digtable.dig_state == DM_STA_DIG_OFF &&
			(priv->reset_count == reset_cnt))
		{
			return;
		}
		else
		{
			reset_cnt = priv->reset_count;
		}

		// If DIG is off, DIG high power state must reset.
		dm_digtable.dig_highpwr_state = DM_STA_DIG_MAX;
		dm_digtable.dig_state = DM_STA_DIG_OFF;

		// 1.1 DIG Off.
		rtl8192_setBBreg(dev, UFWP, bMaskByte1, 0x8);	// Only clear byte 1 and rewrite.

		// 1.2 Set initial gain.
		write_nic_byte(dev, rOFDM0_XAAGCCore1, 0x17);
		write_nic_byte(dev, rOFDM0_XBAGCCore1, 0x17);
		write_nic_byte(dev, rOFDM0_XCAGCCore1, 0x17);
		write_nic_byte(dev, rOFDM0_XDAGCCore1, 0x17);

		// 1.3 Lower PD_TH for OFDM.
		if (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)
		{
			/* 2008/01/11 MH 40MHZ 90/92 register are not the same. */
			// 2008/02/05 MH SD3-Jerry 92U/92E PD_TH are the same.
			rtl8192_setBBreg(dev, (rOFDM0_XATxAFE+3), bMaskByte0, 0x00);
			/*else if (priv->card_8192 == HARDWARE_TYPE_RTL8190P)
				write_nic_byte(pAdapter, rOFDM0_RxDetector1, 0x40);
			*/
			//else if (pAdapter->HardwareType == HARDWARE_TYPE_RTL8192E)


			//else
				//PlatformEFIOWrite1Byte(pAdapter, rOFDM0_RxDetector1, 0x40);
		}
		else
			write_nic_byte(dev, rOFDM0_RxDetector1, 0x42);

		// 1.4 Lower CS ratio for CCK.
		write_nic_byte(dev, 0xa0a, 0x08);

		// 1.5 Higher EDCCA.
		//PlatformEFIOWrite4Byte(pAdapter, rOFDM0_ECCAThreshold, 0x325);
		return;

	}

	/* 2. When RSSI increase, We have to judge if it is larger than a treshold
		  and then execute below step.  */
	if ((priv->undecorated_smoothed_pwdb >= dm_digtable.rssi_high_thresh) )
	{
		u8 reset_flag = 0;

		if (dm_digtable.dig_state == DM_STA_DIG_ON &&
			(priv->reset_count == reset_cnt))
		{
			dm_ctrl_initgain_byrssi_highpwr(dev);
			return;
		}
		else
		{
			if (priv->reset_count != reset_cnt)
				reset_flag = 1;

			reset_cnt = priv->reset_count;
		}

		dm_digtable.dig_state = DM_STA_DIG_ON;
		//DbgPrint("DIG ON\n\r");

		// 2.1 Set initial gain.
		// 2008/02/26 MH SD3-Jerry suggest to prevent dirty environment.
		if (reset_flag == 1)
		{
			write_nic_byte(dev, rOFDM0_XAAGCCore1, 0x2c);
			write_nic_byte(dev, rOFDM0_XBAGCCore1, 0x2c);
			write_nic_byte(dev, rOFDM0_XCAGCCore1, 0x2c);
			write_nic_byte(dev, rOFDM0_XDAGCCore1, 0x2c);
		}
		else
		{
			write_nic_byte(dev, rOFDM0_XAAGCCore1, 0x20);
			write_nic_byte(dev, rOFDM0_XBAGCCore1, 0x20);
			write_nic_byte(dev, rOFDM0_XCAGCCore1, 0x20);
			write_nic_byte(dev, rOFDM0_XDAGCCore1, 0x20);
		}

		// 2.2 Higher PD_TH for OFDM.
		if (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)
		{
			/* 2008/01/11 MH 40MHZ 90/92 register are not the same. */
			// 2008/02/05 MH SD3-Jerry 92U/92E PD_TH are the same.
			#ifdef RTL8190P
				write_nic_byte(dev, rOFDM0_RxDetector1, 0x42);
			#else
				write_nic_byte(dev, (rOFDM0_XATxAFE+3), 0x20);
			#endif
			/*
			else if (priv->card_8192 == HARDWARE_TYPE_RTL8190P)
				write_nic_byte(dev, rOFDM0_RxDetector1, 0x42);
			*/
			//else if (pAdapter->HardwareType == HARDWARE_TYPE_RTL8192E)

			//else
				//PlatformEFIOWrite1Byte(pAdapter, rOFDM0_RxDetector1, 0x42);
		}
		else
			write_nic_byte(dev, rOFDM0_RxDetector1, 0x44);

		// 2.3 Higher CS ratio for CCK.
		write_nic_byte(dev, 0xa0a, 0xcd);

		// 2.4 Lower EDCCA.
		/* 2008/01/11 MH 90/92 series are the same. */
		//PlatformEFIOWrite4Byte(pAdapter, rOFDM0_ECCAThreshold, 0x346);

		// 2.5 DIG On.
		rtl8192_setBBreg(dev, UFWP, bMaskByte1, 0x1);	// Only clear byte 1 and rewrite.

	}

	dm_ctrl_initgain_byrssi_highpwr(dev);

}	/* dm_CtrlInitGainByRssi */


/*-----------------------------------------------------------------------------
 * Function:	dm_ctrl_initgain_byrssi_highpwr()
 *
 * Overview:
 *
 * Input:		NONE
 *
 * Output:		NONE
 *
 * Return:		NONE
 *
 * Revised History:
 *	When		Who		Remark
 *	05/28/2008	amy		Create Version 0 porting from windows code.
 *
 *---------------------------------------------------------------------------*/
static void dm_ctrl_initgain_byrssi_highpwr(
	struct net_device * dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	static u32 reset_cnt_highpwr = 0;

	// For smooth, we can not change high power DIG state in the range.
	if ((priv->undecorated_smoothed_pwdb > dm_digtable.rssi_high_power_lowthresh) &&
		(priv->undecorated_smoothed_pwdb < dm_digtable.rssi_high_power_highthresh))
	{
		return;
	}

	/* 3. When RSSI >75% or <70%, it is a high power issue. We have to judge if
		  it is larger than a treshold and then execute below step.  */
	// 2008/02/05 MH SD3-Jerry Modify PD_TH for high power issue.
	if (priv->undecorated_smoothed_pwdb >= dm_digtable.rssi_high_power_highthresh)
	{
		if (dm_digtable.dig_highpwr_state == DM_STA_DIG_ON &&
			(priv->reset_count == reset_cnt_highpwr))
			return;
		else
			dm_digtable.dig_highpwr_state = DM_STA_DIG_ON;

		// 3.1 Higher PD_TH for OFDM for high power state.
		if (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)
		{
			rtl8192_setBBreg(dev, (rOFDM0_XATxAFE+3), bMaskByte0, 0x10);
			/*else if (priv->card_8192 == HARDWARE_TYPE_RTL8190P)
				write_nic_byte(dev, rOFDM0_RxDetector1, 0x41);
			*/

		}
		else
			write_nic_byte(dev, rOFDM0_RxDetector1, 0x43);
	}
	else
	{
		if (dm_digtable.dig_highpwr_state == DM_STA_DIG_OFF&&
			(priv->reset_count == reset_cnt_highpwr))
			return;
		else
			dm_digtable.dig_highpwr_state = DM_STA_DIG_OFF;

		if (priv->undecorated_smoothed_pwdb < dm_digtable.rssi_high_power_lowthresh &&
			 priv->undecorated_smoothed_pwdb >= dm_digtable.rssi_high_thresh)
		{
			// 3.2 Recover PD_TH for OFDM for normal power region.
			if (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)
			{
				#ifdef RTL8190P
					write_nic_byte(dev, rOFDM0_RxDetector1, 0x42);
				#else
					write_nic_byte(dev, (rOFDM0_XATxAFE+3), 0x20);
				#endif
				/*else if (priv->card_8192 == HARDWARE_TYPE_RTL8190P)
					write_nic_byte(dev, rOFDM0_RxDetector1, 0x42);
				*/

			}
			else
				write_nic_byte(dev, rOFDM0_RxDetector1, 0x44);
		}
	}

	reset_cnt_highpwr = priv->reset_count;

}	/* dm_CtrlInitGainByRssiHighPwr */


static void dm_initial_gain(
	struct net_device * dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8					initial_gain=0;
	static u8				initialized=0, force_write=0;
	static u32			reset_cnt=0;

	if(dm_digtable.dig_algorithm_switch)
	{
		initialized = 0;
		reset_cnt = 0;
	}

	if(dm_digtable.pre_connect_state == dm_digtable.cur_connect_state)
	{
		if(dm_digtable.cur_connect_state == DIG_CONNECT)
		{
			if((dm_digtable.rssi_val+10-dm_digtable.backoff_val) > dm_digtable.rx_gain_range_max)
				dm_digtable.cur_ig_value = dm_digtable.rx_gain_range_max;
			else if((dm_digtable.rssi_val+10-dm_digtable.backoff_val) < dm_digtable.rx_gain_range_min)
				dm_digtable.cur_ig_value = dm_digtable.rx_gain_range_min;
			else
				dm_digtable.cur_ig_value = dm_digtable.rssi_val+10-dm_digtable.backoff_val;
		}
		else		//current state is disconnected
		{
			if(dm_digtable.cur_ig_value == 0)
				dm_digtable.cur_ig_value = priv->DefaultInitialGain[0];
			else
				dm_digtable.cur_ig_value = dm_digtable.pre_ig_value;
		}
	}
	else	// disconnected -> connected or connected -> disconnected
	{
		dm_digtable.cur_ig_value = priv->DefaultInitialGain[0];
		dm_digtable.pre_ig_value = 0;
	}
	//DbgPrint("DM_DigTable.CurIGValue = 0x%x, DM_DigTable.PreIGValue = 0x%x\n", DM_DigTable.CurIGValue, DM_DigTable.PreIGValue);

	// if silent reset happened, we should rewrite the values back
	if(priv->reset_count != reset_cnt)
	{
		force_write = 1;
		reset_cnt = priv->reset_count;
	}

	if(dm_digtable.pre_ig_value != read_nic_byte(dev, rOFDM0_XAAGCCore1))
		force_write = 1;

	{
		if((dm_digtable.pre_ig_value != dm_digtable.cur_ig_value)
			|| !initialized || force_write)
		{
			initial_gain = (u8)dm_digtable.cur_ig_value;
			//DbgPrint("Write initial gain = 0x%x\n", initial_gain);
			// Set initial gain.
			write_nic_byte(dev, rOFDM0_XAAGCCore1, initial_gain);
			write_nic_byte(dev, rOFDM0_XBAGCCore1, initial_gain);
			write_nic_byte(dev, rOFDM0_XCAGCCore1, initial_gain);
			write_nic_byte(dev, rOFDM0_XDAGCCore1, initial_gain);
			dm_digtable.pre_ig_value = dm_digtable.cur_ig_value;
			initialized = 1;
			force_write = 0;
		}
	}
}

static void dm_pd_th(
	struct net_device * dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	static u8				initialized=0, force_write=0;
	static u32			reset_cnt = 0;

	if(dm_digtable.dig_algorithm_switch)
	{
		initialized = 0;
		reset_cnt = 0;
	}

	if(dm_digtable.pre_connect_state == dm_digtable.cur_connect_state)
	{
		if(dm_digtable.cur_connect_state == DIG_CONNECT)
		{
			if (dm_digtable.rssi_val >= dm_digtable.rssi_high_power_highthresh)
				dm_digtable.curpd_thstate = DIG_PD_AT_HIGH_POWER;
			else if ((dm_digtable.rssi_val <= dm_digtable.rssi_low_thresh))
				dm_digtable.curpd_thstate = DIG_PD_AT_LOW_POWER;
			else if ((dm_digtable.rssi_val >= dm_digtable.rssi_high_thresh) &&
					(dm_digtable.rssi_val < dm_digtable.rssi_high_power_lowthresh))
				dm_digtable.curpd_thstate = DIG_PD_AT_NORMAL_POWER;
			else
				dm_digtable.curpd_thstate = dm_digtable.prepd_thstate;
		}
		else
		{
			dm_digtable.curpd_thstate = DIG_PD_AT_LOW_POWER;
		}
	}
	else	// disconnected -> connected or connected -> disconnected
	{
		dm_digtable.curpd_thstate = DIG_PD_AT_LOW_POWER;
	}

	// if silent reset happened, we should rewrite the values back
	if(priv->reset_count != reset_cnt)
	{
		force_write = 1;
		reset_cnt = priv->reset_count;
	}

	{
		if((dm_digtable.prepd_thstate != dm_digtable.curpd_thstate) ||
			(initialized<=3) || force_write)
		{
			//DbgPrint("Write PD_TH state = %d\n", DM_DigTable.CurPD_THState);
			if(dm_digtable.curpd_thstate == DIG_PD_AT_LOW_POWER)
			{
				// Lower PD_TH for OFDM.
				if (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)
				{
					/* 2008/01/11 MH 40MHZ 90/92 register are not the same. */
					// 2008/02/05 MH SD3-Jerry 92U/92E PD_TH are the same.
					rtl8192_setBBreg(dev, (rOFDM0_XATxAFE+3), bMaskByte0, 0x00);
					/*else if (priv->card_8192 == HARDWARE_TYPE_RTL8190P)
						write_nic_byte(dev, rOFDM0_RxDetector1, 0x40);
					*/
				}
				else
					write_nic_byte(dev, rOFDM0_RxDetector1, 0x42);
			}
			else if(dm_digtable.curpd_thstate == DIG_PD_AT_NORMAL_POWER)
			{
				// Higher PD_TH for OFDM.
				if (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)
				{
					/* 2008/01/11 MH 40MHZ 90/92 register are not the same. */
					// 2008/02/05 MH SD3-Jerry 92U/92E PD_TH are the same.
					#ifdef RTL8190P
						write_nic_byte(dev, rOFDM0_RxDetector1, 0x42);
					#else
						write_nic_byte(dev, (rOFDM0_XATxAFE+3), 0x20);
					#endif
					/*else if (priv->card_8192 == HARDWARE_TYPE_RTL8190P)
						write_nic_byte(dev, rOFDM0_RxDetector1, 0x42);
					*/
				}
				else
					write_nic_byte(dev, rOFDM0_RxDetector1, 0x44);
			}
			else if(dm_digtable.curpd_thstate == DIG_PD_AT_HIGH_POWER)
			{
				// Higher PD_TH for OFDM for high power state.
				if (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)
				{
					#ifdef RTL8190P
						write_nic_byte(dev, rOFDM0_RxDetector1, 0x41);
					#else
						write_nic_byte(dev, (rOFDM0_XATxAFE+3), 0x10);
					#endif
					/*else if (priv->card_8192 == HARDWARE_TYPE_RTL8190P)
						write_nic_byte(dev, rOFDM0_RxDetector1, 0x41);
					*/
				}
				else
					write_nic_byte(dev, rOFDM0_RxDetector1, 0x43);
			}
			dm_digtable.prepd_thstate = dm_digtable.curpd_thstate;
			if(initialized <= 3)
				initialized++;
			force_write = 0;
		}
	}
}

static	void dm_cs_ratio(
	struct net_device * dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	static u8				initialized=0,force_write=0;
	static u32			reset_cnt = 0;

	if(dm_digtable.dig_algorithm_switch)
	{
		initialized = 0;
		reset_cnt = 0;
	}

	if(dm_digtable.pre_connect_state == dm_digtable.cur_connect_state)
	{
		if(dm_digtable.cur_connect_state == DIG_CONNECT)
		{
			if ((dm_digtable.rssi_val <= dm_digtable.rssi_low_thresh))
				dm_digtable.curcs_ratio_state = DIG_CS_RATIO_LOWER;
			else if ((dm_digtable.rssi_val >= dm_digtable.rssi_high_thresh) )
				dm_digtable.curcs_ratio_state = DIG_CS_RATIO_HIGHER;
			else
				dm_digtable.curcs_ratio_state = dm_digtable.precs_ratio_state;
		}
		else
		{
			dm_digtable.curcs_ratio_state = DIG_CS_RATIO_LOWER;
		}
	}
	else	// disconnected -> connected or connected -> disconnected
	{
		dm_digtable.curcs_ratio_state = DIG_CS_RATIO_LOWER;
	}

	// if silent reset happened, we should rewrite the values back
	if(priv->reset_count != reset_cnt)
	{
		force_write = 1;
		reset_cnt = priv->reset_count;
	}


	{
		if((dm_digtable.precs_ratio_state != dm_digtable.curcs_ratio_state) ||
			!initialized || force_write)
		{
			//DbgPrint("Write CS_ratio state = %d\n", DM_DigTable.CurCS_ratioState);
			if(dm_digtable.curcs_ratio_state == DIG_CS_RATIO_LOWER)
			{
				// Lower CS ratio for CCK.
				write_nic_byte(dev, 0xa0a, 0x08);
			}
			else if(dm_digtable.curcs_ratio_state == DIG_CS_RATIO_HIGHER)
			{
				// Higher CS ratio for CCK.
				write_nic_byte(dev, 0xa0a, 0xcd);
			}
			dm_digtable.precs_ratio_state = dm_digtable.curcs_ratio_state;
			initialized = 1;
			force_write = 0;
		}
	}
}

extern void dm_init_edca_turbo(struct net_device * dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	priv->bcurrent_turbo_EDCA = false;
	priv->ieee80211->bis_any_nonbepkts = false;
	priv->bis_cur_rdlstate = false;
}	// dm_init_edca_turbo

#if 1
static void dm_check_edca_turbo(
	struct net_device * dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	PRT_HIGH_THROUGHPUT	pHTInfo = priv->ieee80211->pHTInfo;
	//PSTA_QOS			pStaQos = pMgntInfo->pStaQos;

	// Keep past Tx/Rx packet count for RT-to-RT EDCA turbo.
	static unsigned long			lastTxOkCnt = 0;
	static unsigned long			lastRxOkCnt = 0;
	unsigned long				curTxOkCnt = 0;
	unsigned long				curRxOkCnt = 0;

	//
	// Do not be Turbo if it's under WiFi config and Qos Enabled, because the EDCA parameters
	// should follow the settings from QAP. By Bruce, 2007-12-07.
	//
	#if 1
	if(priv->ieee80211->state != IEEE80211_LINKED)
		goto dm_CheckEdcaTurbo_EXIT;
	#endif
	// We do not turn on EDCA turbo mode for some AP that has IOT issue
	if(priv->ieee80211->pHTInfo->IOTAction & HT_IOT_ACT_DISABLE_EDCA_TURBO)
		goto dm_CheckEdcaTurbo_EXIT;

	{
		u8* peername[11] = {"unknown", "realtek", "realtek_92se", "broadcom", "ralink", "atheros", "cisco", "marvell", "92u_softap", "self_softap"};
		static int wb_tmp = 0;
		if (wb_tmp == 0){
			printk("%s():iot peer is %#x:%s, bssid:"MAC_FMT"\n",__FUNCTION__,pHTInfo->IOTPeer,peername[pHTInfo->IOTPeer], MAC_ARG(priv->ieee80211->current_network.bssid));
			wb_tmp = 1;
		}
	}
	// Check the status for current condition.
	if(!priv->ieee80211->bis_any_nonbepkts)
	{
		curTxOkCnt = priv->stats.txbytesunicast - lastTxOkCnt;
		curRxOkCnt = priv->stats.rxbytesunicast - lastRxOkCnt;
		// Modify EDCA parameters selection bias
		// For some APs, use downlink EDCA parameters for uplink+downlink
		if(priv->ieee80211->pHTInfo->IOTAction & HT_IOT_ACT_EDCA_BIAS_ON_RX)
		{
			if(curTxOkCnt > 4*curRxOkCnt)
			{
				if(priv->bis_cur_rdlstate || !priv->bcurrent_turbo_EDCA)
				{
					write_nic_dword(dev, EDCAPARA_BE, edca_setting_UL[pHTInfo->IOTPeer]);
					priv->bis_cur_rdlstate = false;
				}
			}
			else
			{
				if(!priv->bis_cur_rdlstate || !priv->bcurrent_turbo_EDCA)
				{
					write_nic_dword(dev, EDCAPARA_BE, edca_setting_DL[pHTInfo->IOTPeer]);
					priv->bis_cur_rdlstate = true;
				}
			}
			priv->bcurrent_turbo_EDCA = true;
		}
		else
		{
			if(curRxOkCnt > 4*curTxOkCnt)
			{
				if(!priv->bis_cur_rdlstate || !priv->bcurrent_turbo_EDCA)
				{
					write_nic_dword(dev, EDCAPARA_BE, edca_setting_DL[pHTInfo->IOTPeer]);
					priv->bis_cur_rdlstate = true;
				}
			}
			else
			{
				if(priv->bis_cur_rdlstate || !priv->bcurrent_turbo_EDCA)
				{
					write_nic_dword(dev, EDCAPARA_BE, edca_setting_UL[pHTInfo->IOTPeer]);
					priv->bis_cur_rdlstate = false;
				}
			}
			priv->bcurrent_turbo_EDCA = true;
		}
	}
	else
	{
		//
		// Turn Off EDCA turbo here.
		// Restore original EDCA according to the declaration of AP.
		//
		 if(priv->bcurrent_turbo_EDCA)
		{

			{
				u8		u1bAIFS;
				u32		u4bAcParam;
				struct ieee80211_qos_parameters *qos_parameters = &priv->ieee80211->current_network.qos_data.parameters;
				u8 mode = priv->ieee80211->mode;

			// For Each time updating EDCA parameter, reset EDCA turbo mode status.
				dm_init_edca_turbo(dev);
				u1bAIFS = qos_parameters->aifs[0] * ((mode&(IEEE_G|IEEE_N_24G)) ?9:20) + aSifsTime;
				u4bAcParam = ((((u32)(qos_parameters->tx_op_limit[0]))<< AC_PARAM_TXOP_LIMIT_OFFSET)|
					(((u32)(qos_parameters->cw_max[0]))<< AC_PARAM_ECW_MAX_OFFSET)|
					(((u32)(qos_parameters->cw_min[0]))<< AC_PARAM_ECW_MIN_OFFSET)|
					((u32)u1bAIFS << AC_PARAM_AIFS_OFFSET));
			//write_nic_dword(dev, WDCAPARA_ADD[i], u4bAcParam);
				write_nic_dword(dev, EDCAPARA_BE,  u4bAcParam);

			// Check ACM bit.
			// If it is set, immediately set ACM control bit to downgrading AC for passing WMM testplan. Annie, 2005-12-13.
				{
			// TODO:  Modified this part and try to set acm control in only 1 IO processing!!

					PACI_AIFSN	pAciAifsn = (PACI_AIFSN)&(qos_parameters->aifs[0]);
					u8		AcmCtrl = read_nic_byte( dev, AcmHwCtrl );
					if( pAciAifsn->f.ACM )
					{ // ACM bit is 1.
						AcmCtrl |= AcmHw_BeqEn;
					}
					else
					{ // ACM bit is 0.
						AcmCtrl &= (~AcmHw_BeqEn);
					}

					RT_TRACE( COMP_QOS,"SetHwReg8190pci(): [HW_VAR_ACM_CTRL] Write 0x%X\n", AcmCtrl ) ;
					write_nic_byte(dev, AcmHwCtrl, AcmCtrl );
				}
			}
			priv->bcurrent_turbo_EDCA = false;
		}
	}


dm_CheckEdcaTurbo_EXIT:
	// Set variables for next time.
	priv->ieee80211->bis_any_nonbepkts = false;
	lastTxOkCnt = priv->stats.txbytesunicast;
	lastRxOkCnt = priv->stats.rxbytesunicast;
}	// dm_CheckEdcaTurbo
#endif

extern void DM_CTSToSelfSetting(struct net_device * dev,u32 DM_Type, u32 DM_Value)
{
	struct r8192_priv *priv = ieee80211_priv((struct net_device *)dev);

	if (DM_Type == 0)	// CTS to self disable/enable
	{
		if(DM_Value > 1)
			DM_Value = 1;
		priv->ieee80211->bCTSToSelfEnable = (bool)DM_Value;
		//DbgPrint("pMgntInfo->bCTSToSelfEnable = %d\n", pMgntInfo->bCTSToSelfEnable);
	}
	else if(DM_Type == 1) //CTS to self Th
	{
		if(DM_Value >= 50)
			DM_Value = 50;
		priv->ieee80211->CTSToSelfTH = (u8)DM_Value;
		//DbgPrint("pMgntInfo->CTSToSelfTH = %d\n", pMgntInfo->CTSToSelfTH);
	}
}

static void dm_init_ctstoself(struct net_device * dev)
{
	struct r8192_priv *priv = ieee80211_priv((struct net_device *)dev);

	priv->ieee80211->bCTSToSelfEnable = TRUE;
	priv->ieee80211->CTSToSelfTH = CTSToSelfTHVal;
}

static void dm_ctstoself(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv((struct net_device *)dev);
	PRT_HIGH_THROUGHPUT	pHTInfo = priv->ieee80211->pHTInfo;
	static unsigned long				lastTxOkCnt = 0;
	static unsigned long				lastRxOkCnt = 0;
	unsigned long						curTxOkCnt = 0;
	unsigned long						curRxOkCnt = 0;

	if(priv->ieee80211->bCTSToSelfEnable != TRUE)
	{
		pHTInfo->IOTAction &= ~HT_IOT_ACT_FORCED_CTS2SELF;
		return;
	}
	/*
	1. Uplink
	2. Linksys350/Linksys300N
	3. <50 disable, >55 enable
	*/

	if(pHTInfo->IOTPeer == HT_IOT_PEER_BROADCOM)
	{
		curTxOkCnt = priv->stats.txbytesunicast - lastTxOkCnt;
		curRxOkCnt = priv->stats.rxbytesunicast - lastRxOkCnt;
		if(curRxOkCnt > 4*curTxOkCnt)	//downlink, disable CTS to self
		{
			pHTInfo->IOTAction &= ~HT_IOT_ACT_FORCED_CTS2SELF;
			//DbgPrint("dm_CTSToSelf() ==> CTS to self disabled -- downlink\n");
		}
		else	//uplink
		{
		#if 1
			pHTInfo->IOTAction |= HT_IOT_ACT_FORCED_CTS2SELF;
		#else
			if(priv->undecorated_smoothed_pwdb < priv->ieee80211->CTSToSelfTH)	// disable CTS to self
			{
				pHTInfo->IOTAction &= ~HT_IOT_ACT_FORCED_CTS2SELF;
				//DbgPrint("dm_CTSToSelf() ==> CTS to self disabled\n");
			}
			else if(priv->undecorated_smoothed_pwdb >= (priv->ieee80211->CTSToSelfTH+5))	// enable CTS to self
			{
				pHTInfo->IOTAction |= HT_IOT_ACT_FORCED_CTS2SELF;
				//DbgPrint("dm_CTSToSelf() ==> CTS to self enabled\n");
			}
		#endif
		}

		lastTxOkCnt = priv->stats.txbytesunicast;
		lastRxOkCnt = priv->stats.rxbytesunicast;
	}
}

/*-----------------------------------------------------------------------------
 * Function:	dm_check_rfctrl_gpio()
 *
 * Overview:	Copy 8187B template for 9xseries.
 *
 * Input:		NONE
 *
 * Output:		NONE
 *
 * Return:		NONE
 *
 * Revised History:
 *	When		Who		Remark
 *	05/28/2008	amy		Create Version 0 porting from windows code.
 *
 *---------------------------------------------------------------------------*/
#if 1
static void dm_check_rfctrl_gpio(struct net_device * dev)
{
	//struct r8192_priv *priv = ieee80211_priv(dev);

	// Walk around for DTM test, we will not enable HW - radio on/off because r/w
	// page 1 register before Lextra bus is enabled cause system fails when resuming
	// from S4. 20080218, Emily

	// Stop to execute workitem to prevent S3/S4 bug.
#ifdef RTL8190P
	return;
#endif
#ifd/*++
Copy2Ught-c Realtek Semht-c Realtconductor CorE
		queue_delayed_work(priv->W dy_wq,&W dynagpio_change_rfmech0)altek Sem
}	/* dm_CheckRfCtrlGPIO */
ltek Sem/*-------- -------------------------------
	2008-05-14	amy                     
 * Function:	dm_c			W_pbc_
Maj()
 *reatOverview:					W if PBC button is pressed.ows codInput:		NONEows codeut.h"
#include "r81R-c Redpkt.h"
#includevised History:
 *	When		Who		Remarky.h"05/28/2008	amy 	Create Version 0 porting from windows code"r8192U------- -------------------------------
	2008-05-14	amy                    */
static	void 0 porting from winstruct net_device *dev)
{miconductor Corp. DL[HT_Ir Cor_W dy *92SE*= ieee80211_92SE( =
	;
	u8 tmp1byte;


			MARVEL = read_nic_RVELEROS,GPI		CIif(U_AP		SELF= 0xff)
	ht-c Reala44f , 	0x5ea4&BIT6 ||O		MARVEL0xa60)
	{
		// Here we only set bPbcPclude  to TRU192U// After triggere "r, the variable will betic uto FALS192URT_TRACE(COMP_IO, "				WPbc------e "r8
#include \n"		CI	W dynab fronclude  = true;
	}hts reser90	/*REALTEK_92SE*/	BROADCOM	RALINK		ATHEROS		CISC2U_AP		SE4f, write	   { 0xa44f,  MAC_PINMUX_CFG, (----RTK_EN | ----SEL_----))4f, U_AP		SELF_AP
		   { 0xa44f, fine _IO_SEL		CIU_AP		SEL&= ~(HAL_ CorS_HW_DL_E_WPS_BIT		CIx5e4322};

#endif

#------Defin,O		MARVELCA 0x5e4322
/*---------------------------NCA 0xADCOM	RALINK		ATHEROS		CISCO		MARV%x\n"ne global variaIOT_dd by hpfan 

/*.07.07K_92fix_AP
	----- errorine LoS3, 		0xa44f, 		4f,		0xa44f,		0xa4 4f, 		0xa44f, 		0-------------------------*322,	0x5e4322};
		static u32 edca_setting_UL[HT_IOT_PEER_MAX] =
		// UNKNOWN	REALTEK		/*REALTEK_92SE*/	BROADCOM	RALINK		ATHEROS		CISCO		MARVELL		92U_AP		SELF_AP
		   { 0x5ea44f, 	0xa44f, 	0x5e


}
odule Name:
	r819---------- -------------------------------
	2008-05-14	amy                     create version 0 p----C ChanRF code.

--*/

PCIEK		/*not sup---- t:
	item ca	/*Rack HW radio on-off control"r8192U_dm.h"
#include "r819xU_cmdpkt.h"
#include "r8192S_hw.h"
#include "r8192S_phy.h"
#include "r8192S_phyr2/21"

/*-MHC	--------------------------------------------*/
//
// Indicate different AP vendor for IOT issue.
//
#iexternstatic u3
Major Change Htic	oid _DL[HT_It:
	_DL[HT_I*t:
			//0x5e4322
Abstract:
	 *dtern	=on(stainer_of(t:
	,ruct *work);
extern,t wor;
   binc90	/*REALTEK_92SE*/	BROADCck_txpower_adoid	d(struct ALTEK_92SE,
Major Change Hisool  binch14);
exOT_PEER_MAX] =
 = W dynaOM	RALINK->dev	CISCO		MARVELL	ug vRF_POWER_STATE	eRfPowerStateToSet;
	bool bActuallySet = fals22,	0do2,	0ruct net_dev= *dev,
		44f,!W dynaup44f,2,	0ug variablLINK		ANIT | INK		d	dm_cSettingRF),";
extern	void	dm_txpower_tr): Cpower_t f versio breaks out!!ELF_AP
	}
		else;
extern// 0x108Y.
u8	im.h" regisEER_isAC PHY	stae_tx_fic u_info(B1= 1: RF-ON; 0						FF.ternU_AP		SELF_AP
		   { 0xa44f, 	0x5eaterndynamic_initgain_t =0xa44f, 		0xa61) ?  eRfOn :operaffo(struif( HW dynabHwReckPOff4f,	4f, ) && (ct net_device *dev);=operat)44f,externug variable ?
dRF, "
Majr ChanRF  -dm_Cng da ONELF_APtructsigned long data);
e *dev,
ructruct net_device4f, 	0xn	voidd dm_lude(unsigned long data);
ex *dev	void	dm_rf_pathcheck_workitemcafflback(struct work_struct *work);
extern	void dm_fsync_timFFELF_AP
	k(unsigned long data);c(struct id dm_check_fsync(struct nellback(ruct net_devback(structdule NaTO_DO-----MgntActSet	voiinitg------dynamic_initgain_t, RF_CHANGE_BY_HW-------//DrvIFIndicateCurrentPhyinitus(pAdapter-------ts resert net_devicack(structmsleep(200ory:on proto}
	}while(_UL[)      	Whoprototype--------	--------------- -------------------------------
	2008-05-14	amy                     create version DM_RFPath				WWorkIteme,
	B	DM_Tows code.

--*/


#includeandwidt RF RX pathdev,enEALTdr8192U_dm.h"
#include "r819xU_cmdpkt.h"
#include "r8192S_hw.h"
#include "r8192S_phy.h"
#include "r8192S_phyr1/30device *dev);

extern void hal_dm_watchdog(struct net_device *dev);


extern	void	init_rate_adaptive(struct net_device *dev);
erf_e *dorting_LIST
stxpower_trackingcallback(struct work_struct *work);
extern	void	dm_cck_txpower_adjust(struct net_device *dev,bool  binch14);
extern	void	dm_restore_dynamic_mechanism_state(struct net_rfe *dportingxtern	void	dm_backup_dynamic_mechanim_state(struct net_devi//sh(stract net__ic u);
extern	u8 initga = 0, iL		92/*ster /192U
 MHT_PEER_discussDefiwith SD3 Jerry, 0xc04/0xd04_device *dK		/
	   always*REANKNOsame. W
		statC PHYarm(s now.----	 net_devi-------------------arm(sshoftwar
#inclBit 0-3, it meansludeRF A-Dev);



//s.
	for (ievic; i < RF90_PATH_MAX; i++Sel	DM_ *de net_de& (0x01<<ilback(signed initgairx



//[i] = 1e *d dm_for// DM --> EDCA turboe mode co0	0x5eu32		et_dxviceSelTEALT.E


//44f,		0xa44f, _bacxitgaisel_byrssiEROS		C    	et_device *dev);

static	void	-----f 1
		 tati	Whoinitic	void	dm_eersio_DL[HT_IOT_PEER_MAX]  =
		//ructi;	0x5e4322, 	0x5ea422, 	0x5e4322, 	0x3ea44f,	0x5ea4ice *dev);

// DM --> HW cont	//default;



//sto(struct net_deviceSS_TH_low =  *dev);

hange__e
static	pio(struct net_devicediff_THvoid	dm_check_rx_pa *dev);ea44f,W dynaCustomerIDrkitRT_CID----x_NetcorW RF ice *dev);

// DM cck_metho0xa4CCK_Rx_-------_2;
l
static dm_rxpath_sel_byrssi(struct net_device *dev);ntroice *dev);

// DM DbgMod);

DM_DBG_OFFion(struct net_device *s

//sRFstruct for(i=;

s<4it_ctstoselfice *dev);

// DM rf_ck_rode co5uct -> Fsync for broadcom apwdb_staode co-64ndretrycount(struct nerf_



//check_thode con0uct ne}RTL8190P
//staticc	void	dm_check_rff(struct net_device *dev);
#x5e4322, 	0x5ea422, 	0x5e4322, 	0x3ea44f,	0x5ea42b,lbac, max functindex=0, min08/02/18
staticsec08/02/18
staticrf_num=uct by Jactmp_ 2008/02atic;
stavoid	dmd	dm_dynmic_txpoevice *dev)ssi(M --> C_Rx=0x2// DRF-Cice *dev);

oprsioal--> Fo3;ate aD
	longdev);
stssi(saxeviced	dm_dynssi(sinid dm_send_rssi_tmic_d dm_svice *dev);

rx_ver2c	voi18
staticd	dm_ctstoselinstruct net_device *devmic_18
statvice *dev)urackuck_r;to firmwaron pt_devicef
// 190P
by Ja net_devdm_txnt net_devRx_vicec	voiializedfine functiupdate-----rxup(st4f, 		lectionrf_type !=(str2T4R44f,		0xa44f, 		(!=======================i, 20080522
static	void	d=======et_devi(c	void	dm_pd_th(struca07)&0xfce *d======================= controproteInit_fsync(struct net_device *dxfm_deInit_fsync(struct net_device&=~=========
static void dt neallba	HW Dyname(struct nem);
st= WIRELESS_MODE_B=============================struct net_device *dev);

	//pure B //PR,or Medt_de v------2HT_IODbgPrint("Ptic u8		lastusent = rx 0;
	u8		 ELF_AP
prot DM ciden 20/sec/min ck_r 18
stevice *dAdded ic	void dm_init_ctstoself(sevice *dev);

// DM *dev);
back(0522
static	void	dm_check_txratW dyna/
//s.r008/02/percentagmode				u32	/ DM --> EDCA turboe mode);
extern net_d++e *deon prototypstatic2
static	void	dm_check_txcallback( net_dT_TC1)x_fwfind firsCheck cur rfce *deandd	dm_MSDU_valuesack(s	//==========namit ic	v->ieee8021K_92	dm_init onswitc	 2008/02/18
st =c	void	dm_init_ =amic_txpower(stADCOrn	voi;
static	voi----dynamic_txpuEnablemic_txpotoren prototype--t net_device *MCS_SEL) > 2back(sx_fwwe pick up= primaxrRxOkCnE54M),nt = letamicnt = privtovoid	dm_inittats.rxbyif(T_ACT_AMSDU_>Enable 2008/02back((structnt)
				bAmsduEn	// Do not need bytesunicast - lastrTxOkCnnet_dl
static	_Support = t
	else
	{
	able = true;
		se if(!bAmsduEnablurRxOkCnt <= 4*cuRxOkCnt;

			if(crrent_AMSDU_dth_autoswitch(strupport)
	{
		pHTIfo->bCurrent_AMSDU_Support = t_DO_LIST
		//Pla2008/02Adapter, RT_TX_SPINLOCK);2008/02/18
stduEnabl = true;
	}
	else if(!bAmsduEnable && pHTInfo->bCurrent_AMSDU_Suppo aggT_ACT_AMSDU_Enfo->bCurrent_AMSDU_Sx_fwduEnable && !pHTpoinEK_92NKNO *deet nete80211-ev->TcbAggrQueue[QureSpinLock(Adapter, RT_TX_SPINLOCK)ue[QueueId]--;
				PrreTransmitTCB<fo->bCurrent_A &&Id++)
		{
			while(mic_txpolback(y(&dev->TcbAggrQueue[Qurent_AMSDU_Support = false;
#endif
	}

	// Determine A-MeTransmitTCB(dev, p & HT_IOTACT_AMSDU_EN4f, 	0AggrQueue[Q		//PlatformAcariabl}
		//PlatformReleaseSpinLock(Adapter, RT_TX_SPINLOCK);
NABLE)
	{
		if(!bAmsduEnable)
			pt = false;
#endif
	}

	//AMSDU_SSupport)
	_Supportx_fwThis cas2};
	don'kup_ettingOkCntny_SPINLOCK);
MSDU_S Determine A-MPDU policy
	if(priv & HT_IOTvoid	d++)
		{
			while(!txbytesuariables.
	lais function is only invoked at driver intializateId]--;
				PreTransmitTCB(dev, pTtxbytesunicases.
	lastTxOkCnt = priv->stats.txbytesunicast;
	lastRxOkCnt = priv->stats.rxbytesunicast;
}
//
//	DescripatformAcquireSpinLock(Adapter	;
		for(QueueId = 0; Que.
//
//	Assumption:
//		This function is only invoked at driver intialization once.
//
//
exterDU policy
	if(priv-dynamic mechanism.
	//PlatformAcquireSpinLock(Adapter;
		for(QueueId = 0; QueueId < MAtati
dev)_SEL) ruct // DU policy.
	if(privt = vice_SPINLOCif( dm_rxpath_sel_byrssi(struct nnet_device *dev);
Sel	DM_>pHTInfo->IOTAction & HT_IOT_ACT_anism.lse if(priv->ieee80211->pHTInfo-anism.
ction & HT_IO------------- = NABLE)
	{
		if(readt_device *dev);llback(NIMCS_SEL) > DESC92S_RATE54M)
		{
			curTxOkCnt = priv->stats.txbytessunicast - lastTxOkCnt;
			curRxOkCnt = priv->stats.rxbyid	dm_ctstoself(structf(!bdevice *dev);
/*----ved by tynli
	d-------Deinit_dynamie
static	void dm
		//Plsi_tofw(struTO_DO_LIST
e *dev);f(!bAms----------* Signal Strength,regation policy..
		return;
	}

	// Switch A-MSDU
	if(bAmsduEnable && !pHTInfo->bCurrent_AMSDU_SueueId++)t r8192_pInfo->batic	void dmunicast;
//	Descripv);

#ifdef TO_if

	// ==========ndretrycount(dev); //movedt_dynamic_txpower(dev);
	init_ratotection(dev);
#endDO_LIST
	dm_CheckPrRE_TYPE_8192S(dev))
	return;

#i

	dm_CheckA by tynli
	dm_check_edcat_dynamic_txpoMSDU_Support)
	{
#ifde=================heckRateAd========================
	//if
		AP_dm_CheckRateAdvoid dmn;
	}
	else
#endif
	{
		dm_check_rate_adaptivf(structS(dev))
==
	//if (IS_HARDWARE_TYPE_8192S(dev))
	return;

#ifdef TO_DO_LIST
	if(Adapter->Mgit(dev);t r8192_pr==========================
	lastRxOkCnt = priv->stats.rxbytesunicast;
}
//
//	Description(dev);
#endif

	// ==========	else
#endif
	{
		dm_checktstoself(dev);

}	//HallDmWatchDog

/f(priv;
	dm_ctrl_in11->pHTIr_tracking(dev);
	de *dev);priv *pn_byrssi(dev);//LZM TMP 0903ut arguments and RATR table level.
  *	01/16/2008	MHC		RF_Type is aDmWatchDog

/*
  * Decidtion afte making sure RstTxOkC;//LZM TMP 090CheckRateAdaptive(;
	prate Set according to distance (signal strength)
  *	01/11/2RF_Type.
  */
extern void init_rate_adapTR table level.
  *	01/16/2008	MH.
//
//		Assumption=========ieee80211_priv(dev);

	// Undecorated Smoothed Si0M = RateC		RF_Type is assigned in ReadAdapterIntion aftet_device r_tracking(dev);
	dmrate_ada;
	prate_adaptiis function is only invoked at driver intialization onc

}	//HalDmWatchDog

/*
  * Deciderate_adaptive;e_adaptive	pra = (prate_adaptive)&priv->rate_adaptive;

	pra->ratr_state = DM_RATR_STA_MAX;
	pra->high2low_rssi_thresh_fdaptive(dev);
		//return;
	}
	eid	dm_ctstosele(dev);
	}
	dm_dynaM = RateAdaptiveTH_Low_20M+5;
	pra->low2high_rssi_thresh_for_ra40M = RateAdaptiveTH_Low_40M+5;

	pra-signed in ReadAdapterInfv->CustomerID == RT_CIeckRateAdaptive(dev);
		//return;
	}
	else
#endif
	 07/10/08 MH Modify mic_txporxpath_selecti_initdev)CCK Rxce *d_initreg0xA07[3:2]=t = M --> Chr		= 	0,ff001;a07[1:0>low_rIG, we meshold_r.
===================;
	dm_ixtern void deinit_hal_dm(struct net_device *dev)
{

	dm_de);


// DM -->_rx_path_selection(dev);//LZnd DIG, we mustdca_turbo(dev);

	dm_Che====ve	pra = (p
#ifdef T!
/*--;
	prthreshold_ratr_20M	= v)
{
	ststTxOkC = true;
<NABLE)
	{
		if(reade
static	v&&nitg============ <policHYBRID_
		pra		bAmsdu-stats.txbytesu====(struct net_device *dev);0
extern //recor= priv		{
			cue
	{thresholdic_byte(dev, INIMCS_SEL)  local function	void	dm_init_e co;
static	voi+5=====//
		pra-AdaptBBtr		= 	0, OFDMIOTActlLTEK_setBBreg------r----0_T *dev)--> HWtruc1<<	void	dm_init_truc0)
	st11->pH[3:0]--------------------------
 * F1nction:	dm_check_rate_adaptive()
 *
 * Overview:d *
 * Input
		pra->low_rss& HT_Ivoidxtern void deinit_hal_dm(struct net_device *dev)
{10
extern r test
	}
	else iRemoveHeadList(&dev2R)
	{
		pra->uppemic_txpower(st=====0;
		praurrent_AMSDU__threshold_ratr		= 	0x000selectioif(==========================================================);


// DM --><<2)|UGHPUIG, we mustce *d----------------------
CCK0_AFESet-Def
 * Of0riv->,v *priv = ieee80211_pri=======ne if A-xtern void deinit_hal net_devic
	dm_deIni//Added by vivi, xtern  voetRATR = 0;
	u32						LowRSSI>>net_k_raESC9
		pra-> randwiE; QueueIt)
				bAmsduow_rssi_threshold_ratine local function priv *priv = iee



//---------------iv = ieexOkCnt = 0RF-%dstruct net_dermick_ratei------------------------------
 * Function:	dm_check_rateick_raverview:
 *
 * Inputut:		NONE
 *
 * Output:		NONE
 *
 * Return:		NONE
 .
		return;

	en		Who		Rem-----------------Define local function prototype-ESS_
		pra->low_rss--m_init_rxpath_select}ine local variable------------------------------*/


/*--------------------Define export function orting=======io_change_rm_check_txpower_traic	vaO_LIST
stato ortinn vot net_XRFTxOkCnt = Rx vice o_change_riteRSSI"r8192U_dm.h"
#include "r819xU_cmdpkt.h"
#include "r8192S_hw.h"
#include "r8192S_phy.h"
#include "r8192S_phyreg.h"

/*----ev);

extern void -----Define Local Constant---------------------------*/
//
// Indicate different AP vendor for IOT issue.
//
#if 1
		static u32 edcaCheck whether ShorDL[HT_IOT_PEER_MAX] =
		//er Control For Near/Far Range --------*/   //Add
U_dm.c

Abstract:
	HW dynamic mechanism.
initgain_byrssitory:    	Who				Whxdevice-----TL8190P
//static	voidfsync enabled)? BIT31:0) ;

		if (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)
		/u8 			QueueId;
	)? BI_time_intervalrateaine reshold_ratr_20M & (~BIr====bitmap80211&priv8gi_enabled)? BIT31:0) ;
		}
	unctio 	0x000 = 30Module Name:
	rrigh/u8 			QueueId;
	b& (~BIrate_ad--------# dm_foshort_gi_enabled)? BIT31:0) ;

		 *dev,
ts reserreshold_ratr_20M & (~BImultipleIT31) | ((bshort3_enabled)? BIT31:0) ;
		}
E54M) *dev		//>ping_rssototype-reshold_ratr_20M & (~BIsecondIf
		   STA stay in 2igh or low level, we must chanitgstat// DM -F? BIh or low frame? BIMonitor;


// D (pHTInfoM --> Ch0xc38 m_RATR_Son tarvoidT31)r(anism.
& (~BIT31)oid	dsi_thresh_for_ra;.data====unsigned  fir)t_deviLowRSSIThreshForRA						s32=	Whoesh_for_ra;txpower_t;ee80TL8190P
//staticdeIbled)? BIenabled)? BIT31:0) ;

		if (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)
	delw_rssi_sh_fossi_thresh_for_ra;
		}
_device
//statica->low_rssi_thresh_fv->CurrentChan 	= (	if (priv->Cp_dynamic_mechanisenabled)? BIT31:0) ;)	= (f
// DM --> Check PBC
static	void dm_check_p	(pra->low2high_rssi_thre		CIS32 		//c()
 *
 		//ccounverruct 	= pra->hnit_rfine sh(s		bSwitchFromCa->hDiata);
extern		LowRSSDoubleTimeI| ((bshort *dev,
		if(or low level, we mequentl= IEEEALINK	LINKED &&P
		   { gi_enabled)? BIT31:0) ;

si_th//u8 			QueueId;
	pHTInfo->IOTA->bCur& HT_IOT_ACT_CDD_FSYNCsh_f==== vicea->hisi_t 54, MCS [7], [12, 13, 14, 15InpughRSSIThrosa adELESorRASIThreshFoev);

eshForRA)
	<= 27			//DbgPrinSSIThreshFo		//cosa add 	0x <<		//DbgPrinc void dabled)? BIT31:0) ;
		}
		//cosa add& >= (long)HigIThr r", pHara->h+S_RATE54M)
			beceiv========h192Sgram[1][SIThreshFo]* dev)reate A	= pra->hi<_RATE54else 1;
		p;
	prsi_thresh_for_r80211_I=%d ST -RA	= pra->hi+SSIThreshForRA)
		trol
static	//DbgPrint("[DM] RA	= pra->hi-HalData->UndecoratedSmb >= (long)Lo("[DM]RSSIThreshFoRA	= (priRA)
		{
		{	pra-ghRS(priNv);
	_rssi_threshold_ratr;
		}=Middle\n\r",nit_r------- Contiune ra->h void dDbgPrint>ism_state(struct neust change two different thres;
	praection(thedPWD(prihForR& HT_IOupport)
	hold_ratr;
		}

			//co;
	dm_atedSmoothedPWDB);
		 overern  void    ra->ping_rssi_enab>=olicy.
n	voidIThreshForRA	= (priv->----------		if(pra->ping_rssi_enable)
	xpath_seled dm_force_tx_fwStopre_dydPWDB);
			pra_rssi_thresh_for_ra+5))
			{
		d_pwd//IfshForRA>middl= f (prRhresholdT= 	0x000000getRATR = pra->middl = DM_RATR_STA_LOW;
			taold. If
		   STA stay iIThreshFo_smoothed_pwdb < (long)(pra->pinpra->ping_rssi_thresh_for_ra) ||
ate = DMshForRA)
		tr_state = DMAP
		   { SSI=%d STA=LOW\n\r",r_state = DMndecorateADCOM	RALINK		HALDM, "elseW\n\r",%d_stathForRA%drssi_thForR"[DM]%d _smoothe? BIT%SELF,"[DM] RSSI=rRA)
		rssi_thresh_rssi_thresh_for_r ecover tbsThresSIThrece *d//ludewe never ratr;
	 those mcsA);
		t = {
			wh30 %Adapn 			}
	 )? B
	else if(prund;
		ated_smoothedracking(->ping_rssi_ratr =
				(pra->ping_rssi&&bgPrint("ForRA	= (pri, pRA->TesW != HT_CHANNEL_WIDTHratr_state = DM
			}
		}

		 = 	dm_val
			}
		}

		dPWDB);
			pra
			}
		}

		/ct net_de& (~BIT31)) | ((b		

/*-----------------0xC36
 * Oice *de7/10/08urrentRATR )
		{
			u3 ratr_val1	// 20init_bandwirentRATR )
		{
			u32 rateck_r9ice *dev)dd for test		if( targetRATR !=  currentRATR )
		{
			u32 ratr_val4e;
			ratr_value = targetRATR;
			RT2 ratr_val5P_RATE,"currentRATR = %x, targetRATR = %x\n", cu6rentRATR,  (priv-> aggMCS0~7.
		if(priv->ieee80211->GeTR to 0x%x \n", pHalData->(pra->ping_rss0
extern  void    hord(dev, RATR0);
		if(ting of RATR0 is requa);
extern	vo		if(priv->rf_type == RF_1T2R)
			{
				ratr_value &= ~(RATE_ALL_OFDM_2SS);
			}
			write_nic_dword(dev, RATR0, ratr_value);
			write_nic_byte(dev, UFWP, 1);

	ype-W != HT_CHANNEL_WI)tern  vo_rssi_pek Sng pra->high_rssi_thre;
	praIThreshForRA	= pra->high_rssi_threshatic voidIThreshForRAexpires = jiffies + MSECS
			pra->ratr_state = DMT31)) | ((bsh/	BROfirst
		   time to link with AP. We willv->ieeaddh2low_rssi_thresh_for_ra;
			);

			pr11->bandwidth_auto_switch.threshold_40Mhzto20Mhz = BW_AUTO_SWITCH_HIGH_LOW;
	priv->ieee80211->bandwidth_auto_switch.bforced_tx20Mhz = false;
	priv->ieee80211->bandwid	// dm_init_bandwidth_autoswitch


static }
// DM -2,	0x5eLet Revice *damic Rxto	HighRSSIts.tx	target_nic_dword(dev, RATR0);oratec void dm_init_bandwidth_autoswi targetRATR !=  cur= RF_1T2R)
			{
				ratr_value &= ~(atr_valueDM_2SS);
			}
			write_nic_dword(devnit_bandwir_value);
			write_nic_byte(dev, UFWtRATR = prthresh_for_ra+5))
			{
	----------------------
 * FunRxDetector2, bMaskDWn", t0x164052cdne if int("TestRSSI is betweera->ping_rssi_enabSSI Recover tatr;
		}

			//cov->int("TestRSSI is between the range. \n");
			}
			else
			{
				//DbgPrint("TestRSSI Recover to 0x%x \n", targetRATR);
				ping_rssi_state = 0;
			}
		}

		// ------------------StartHW"Testenabled)? BIT31:0) ;

		if (nt("TestRSSI is betwee%sI Rec__FUNCTION__*/


/*-------oid	dorce send packets in 20 Mh0x465c12cAggrPDM_2SS);
			}
			writec3b001421 u32 OFDMSwingTable[OEndSble_Length] = {
	0x7f8001fe,	// 0,priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)
		 +6db
	0x71c001c7,	// 1, +5db
	0x65400195,	//IThreshForRA	= po_switceshold_40Mhzthoftwar}else{
		if(priv->ieee80211->bandwidth_uto_switch.bforced_tx20Mhoratc void dm_init_bandwidth_autos Mhz in 20/40
			if(priv->undecorated_smoothed_pwdb <= priv->ieee80211->bandwidth_auto_switch.threshold_40Mhzt
20Mhz)
				priv->ieee80211->bandwidthd_pw_switch.bforced_tx20Mhz = tru#ifn~BIT31)) | ((b 2, +4db
	0x5a400169,	// 3, +3db
	0x50800142,	/riv->un
	When    OFDMSwingTable[OFDM_Tdefault, upper for higher temprature, lower for low temprature
	0x390000e4,	// 7, 			//dPWDB);IothedPW 0x25, 0x21Bg)HighR-1db
	0x32c000cb,	// 8 1, +5db
	0x65400195,	//// i_thira->;
		1;
		prao zero, sDM_T// 41;
		p.= {
	{0xa->ping_rssi_rdm_init0x0b, 0xthed_pwdbsi_stra->hi/ 4, -4db
	{0x1f, 0x1e, 0x1a, 0x1thresh_for_ra+5))
			{
	_rssi_state = 1;
				}
				/==> CCK20M,	// 14, -8db
	0x26c0005b,
	//u8 			QueueId;
	//PRT_TCB			pTcb;
	booN_24Gb
	0x19800066pper/lower threshold. If
		   STA stay in 6IRELESr low level, we must change two different thres] RSSI=%dundeco{
		return4, 0x01},	// 8, -8db
	{0x13, 0x13, 0x10, 0x0d, 0ld
		 x06, 0x03, 0x01},	// 9, -9db
	{0x11, 0x11, 0x0f, 0xld
		 }v);

/0x21, 0x1		{
			//Dble_le("[DM] RSSI, 0x1ctstoselfx0d, 0x06,>UndecoratedS, 0x1b, },	// 7, -7db
	{0x16,e = DM_RATR_STA_HIGH;
			 0x06,v);
// DM -a->ping_rssirssi_threshold_ratr;
		}else if(priv->undecora, 0x1oothnet_dewidth_auto_switch.threshold_40Mhzto20z = BW_AUTO_SWITCH_HIGH_LOW;
	priv->i1_priv(dev);

	if(priv->CurrentChannelBW == HT_CHANNEL_WIDTH_20 ||!priv->ieee80211->_init_bandwidth_autoswitch


st0x1c, 0x12, 0x09, 0x04},	// 0, +0db ===> CCK40M default
	{0x30, 0x1f, 0x29, 0x21, 0x19, 0x10, 0x08,Endable_Length] = {
	0x7f8001fe,	// 0, +6db
	0x71c001c7,	/ 1, +5db
	0x65400195,	// 2, +4db
	0x5a400169,	// 3, +3db
	0x50800142,	/riv->undb
	0x47c0011f,	// 5, +1db
	0x49,	//}

//staticortingesh_for_ra20M);
		}
		else if (p#define	RegC38_. */
		tati000, 0x00},	// 10Non"Test_Other_AP	100, 0x00},	// 108, 0x0AP_BCM		2wer Control For Near/Far Range --------*/   //Add // 0x25, tate == DC3----/
//=======reg_c38tic	vo=,	// 10, -10dbruct r8192_32	reset====== 0x03},	// 3, -3db
	{0x2 "rtGI//DbT_CHANNEL_WID%d Mink witD_T			tx_cmd;
	I Recover t	pra->ratr_state = DM_RATR_STA_	int	    					i =0, j = 0T31)) | ((bsh	int	    					i =0, j = 0ch_enable = false;

}	// , viviflag = FALSE;
	DC00, 0x00, 0x%x F54M)_ratritgaping_rssi%d Snge tfrom_driver=0;
	//RTG;
	int	    					i =0, j = 0, (long)Hig	int	    					i =0, j = 0old. If
		   STA stay i	int	    					i =0, j = 0rgetRATR = pra->low_rssi_
					(pra->low_rssi_thresh_for_ra40M):(pra->low_rssi_th[DM] THresh H/L=%d/%d\n\r", RATR.HighRSSIThreshForRA, RATR.LowRSSIT);
			pra->ratr_stat? BIT31:0) ;

	= e432z == f			}
	
			pra->ratr_state = DMequenive


statiion i. */
		if (pr:LESS_MO[OFDM_Table_LenROS		CItatic voidrevent jumping frequentlyHWif (pra->-----	DM_)
{
	ston iS/fill tlOFDM24G ==> default,levelOFDM24G = %x\n", powerlevelOFDM24G);

	for(j = 0; j<=30; j++)
{	//fill tx_cmd

	tx_cmd.Op		= TX//fill tlOFDM2M --> ClOFDM24	tx_cmd.Op, 1);

			prType<<8) | powerlevelOFDM24G;

	RT_TRACE(COMP_POWER_TRACKING, "powerlevelOFDM24G = %x\nNG;
	tx_cmd.Length	G);

	for(j = 0; j<=30; j++)
{	CMD_SET_mandPacket(dev, &tx_cmd, 12);
	if (rt_TRACKI", powerlevelOFDM24message_handle_tx(dev, (u8*)&tx_cmd,
								DESC_PACKET_TYPE_INIT, sizeof(DCMD_TXCMD_T))CMD_SET_TX_PWRStatus == false)
	{
	UTO_SWITCH_LOW_ratr_state == DM_RATR_IThreshForRAv *priv = ieeanism	// 11, -11db
};

licy.
		rFor broadcom AP.01
x5e43ter, RT_TX_0211->bandwid-----*/
// DRATR !=  curr, 0x0a, 0x00, 0x00, 0end packets in 203
			i5id	dm_inissumption	write_nic_byte(dev, 0x1ba, 0);
			break;
9	}

		for(	CCKSwinriv *priv = ieevoid	// 11, -11db
};

{
				if( (p 0x06, 0x03,  | powerlevelOFDM24G;

	RT_TRACE(COMP_P------_cmd, 12);
	if (rtS18, 0x17, 0x1levelOFDM2G);

	for(j = 0; j<=30; j++)
{	. */
		if (pra->alse)
	{
		RT

		if (Pwr_Flag == RACKING;
	tx_cmd.LengthRT_TRACE(COMP_POWER_TRACKING, "TSSI_report_value = %d\n", tmp_repG, "powerlevelOFDMStatus == fal	tx_cmd.Od_pwdb >	}
#ifdef RTL8190P
		Avg_TSSI_Measra->low_rssi_thresh_for_ra40M):(pra->low_rive


stati->last_ratr = targetRATR;
		}

	}
	elslse
				= 	0xhanism.
	pri = read_nic_word(dev, 0x0x08, 0x00, 0x00,;
	prate_adaptiSSI_Meas == 0)
		{
					write_nic_byte(dev, 0x1ba, 0);
			break;
	ice *de		ratr_value k < 5; k++)
		{
#ifdef RTL8190P
			tmp_report[ort[k];
		}
nic_byte(d(dev, 0x1d8+k);
#else
			0x08, 0x00, 0x00,dm_dynamic_txpower(dev)a->last_ratr = targetRATR;
		}

	}
	>= (d this da+5priv *priv = ir(k = 0;k < 5; k;
	prate_adapti	write_nic_byte(dev, 0x1ba, 0);
			breakratr_state == Drt[k];
		ev, 0x1d8+k);
#else
			riv(dev);
r is going to unl"TestRis idheck(pra>=40,word(deThresh0211%x;
	}

pHalDataeas_from_driver - Tmic_txpowerTR, targetRATR);
	OMP_POWER_TRACKING, "TS======== %d\n", TSSI_13dBm);

		//if(abs(Avg_TSSI_Meas_from_driver - TSI_13dBm) <= E_FOR_TX_POWER_TRACK)
		/ For MacOS-compatible
		if#ifdconnectedI_Meas_from_driver > TSSI_13dBm)
			delta = Avg_TSSI_rxpath_selectRT_TRACE(Cdef RTL8190P
		Avg_4);
	RF_Type bHighpoa->hi!tic	ighpowe_Type	//_PEER_sil nettxpow UNKNOev, 0x1d8+k);
K		/*REAt-c Reetting
#endif
		if(Avg_= %d\n", TSSI_13dBm);

		//if(abs(Avg_TSSI_Meas_from_driver -SI_13dBm) <= E_FOR_TX_POWER_TRACK)
txpowertrt("[DM] RSG, "priv->-------xOkCnt = 0ev, 0x1d8+k);
#e0 ice %d\n", priv-n;
	}tch_enable){
		returnMP_POWER_TRACKING, "orateNG, "priv->rfa_txpowertrackingindex_real = %d\n", priv->rfa_txpowertrackingindex_real);
#ifdef RCKING, "privtate == D nohForRA 	I_Meas_from_driver > TSSI_13dBm)
			delta = Avg_TS->ieee80211->state == IEEE80211_LINKED )
	{
	//	RT_TRACE(COMP_RATE, "dm_CheckRateAdaptive(): \t");

		//shadowc	voirt GI is enabled
		Storent;
	NIC#def/BB_device *dn(stent"r8192U_dm.h"
#include "r819xU_cmdpkt.h"
#include "r8192S_hw.h"
#include "r8192S_phy.h"
#include "r8192S_phyreg.9a->upper_rssi_threshold_ratr & (~BIT31)) | ((bshort_gi_enabled)? BIT31:0) ;

		pra->middle_rssi_threshold_ratr =
				(pra->middle_rssi_thrr_ra;
			LowRSSresent_attennabled)? BIT31:0) ;

		if (u8	pag	stru16	offs_threvice *			{ev);

ertra< 5ngindeSSIThrice *priv->ev);

92_setB< 256g(dev, rSSIThreshFof(priv->r[			{][priv->e co-------------------TxIQImb			{*25v, UFWPrTxOkCnt = 0;-%d/O-%02x=				\rI Recoagepowertraate_aSWord, priv->txbbgaidifferea_txpowertrack8ngindex_r11al--;
						rtl8192_setBBreg(dev, rOFDM0_XATxIQImbalancMaskDWord, priv->txbbgain_table[priv->rfa_txpowertrackingindex_ra_txpowertrack12>rfc_txpoweal--;
						rtl8192_setBBreg(dev, rOFDM0_XATxIQImbalanctrackingindex_real--;
						rtl8192_setBBreg(dev, rOFDM0_XCTxIQIm}     	Whoresent_atte-------------- -------------------D 0x00							s32protomechvendor for IOT issue.
//
#ivice *dev);

// DM --> TX power control
//static	void	dm_initialize_txpower_tracking(struct net_DynamicTxamic_rt GI is enabled
		ts in  Sigpra-strengthenabl(struc TXse{
		iry
25, Tx amic_oothetxbbI_MeNear/Far RChanr8192U_dm.h"
#include "r819xU_cmdpkt.h"
#include "r8192S_hw.h"
#include "r8192S_phy.h"
#include "r8192S_phyr3/06"

/*-Jackenv);

extern void hal_dm_watchdog(struct net_device *dev);


extern	void	init_rate_adaptive(struct net_L8190P
//static	voiddl8192__txpBBregnabled)? BIT31:0) ;

		if (priv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)
		//0x0b, 0xTXfa_txpowertrackice ndex_far rChan , a write----atic	v5/15,-----Define Local Constant--8 MH We support RA lue);
#endif
		1:0) ;

		/* 20  bi//. */
		x1a,rate_ad>rfa_txpowertracx15, 0x11LastDTPFlag_HigM	= 
extern	 priv->cck_present_Lc	voiation_differenctl8192_seattea_txporiv->cck_present_attentuatLow				= priv->cck 0x19, 0x10, 0x08,lue);
#endif
				}
			}
			priv->cck_present_attentuation_difference
				= priv->rfa_txpow	->CurrentnLockxhiattent>ping_ay inine ftuation <23)
	low{
				if(pri->ieee80	RF_Type = priv->rf_tk_present_attentuation
!extern	
	0x19800066,attentuation
				= priv->cck__present_attentuation_40Mdefault + f,		0xa44feren//pnt =k("G);

	for(j = 0; (pHTInf_nett:
	.unknown_cap_existtibl%d ,_network.channel != 14 && priv->bas = reach14)
				{
				I Re_network.channel != 14 && priv->bcck_in_ch14)
				priv->bcck_in_ch14 = FALSE;
					dm_cck_txpower_adju5ea44f,ite_nic_byte(dev, != 14 && priv->ba, 0xosch14)
				{daptiv/u8 			QueueId;
	//PRT_TC40M)_G)211->
			{
				if(priv->i = TXid	dm_cATHEROAP_THRESH_HIGath_	network.channel == 1NG, "priv->rfa_txpowertrackiLOW09, 0x06, 0x03, CE(COMP_POWER_TRACKING, "priv->rNEAR_FIELDertrackingindex_real = %d\n", priv->rfa_txpowert "priv->rfc_txpowel);
#ifd
//ifferrent=_TRACK>%s_TypCE(COMP_POWER_TRACKIN
				,real = %d\n", priv->rt(dev,pri
	0x65400195,
			{
				if(priv->iex_real = %d\n", priv						Avg_TSSI_MeaTXAGC,t_netwo.
		if(priv->ieee80211->Ge= %ldermitate = 0;.
		if(priv->ieee80211->Gamic_txpte_nic_byte(dev, 0x1ba, 0);
			viviflag = FAL4);
	RF_Type driver);
		TSSI_13dBm = priv-
			{
				if(priv->i0Mhz == false){/attentuation
				= prher updating of n_ch14);
				}
				else if(pr (priv->undecoratedhttenif
		b
	{tebled =ern  void    tr = targetRATR;
		}

	}
	e_network.channel == 1targcck_present_attentuation_di_ch14 = T


static void m_cck_txpower_adjust(dev,priv-tx pow	//Pow
			priv->ieee80211->bdynamic_txpower_enable = TRUE;
			wri35NG, "tx power track--->limition_40Mdefc(struct net_device *s_from_driver);
		TSSI_13dBm = priv-4F_Typor(k = 0;k < 5; k++)
			tmp_reportreturn;
		}


nable){
		return;
13dBm)
			bTXamic_t
	-forndexFar].txbuirederTrackingCallback_ThermalMeter(struriv->bcck_in_ch14);
ower_adjust(dev,priv->bcck_in_ch14);
				}
				else if(p, targeunsigned ruct r8192_priv *pr!("[DM] R>cck_present_atten) ||bDynamicTx|priv->cck_present_mpCCK20Mindex, tmpCCK4			p) esent_aING, "priv->cck_preseSetsetBBreLeve----0()  r Chnehort%priv->cck_presKING, "priv->rfa_txpowertrag
		tmp>undeco priv->cck_present_attentul	9
	struct r8192_priv *p_difference;
			else
				prial;
	int i =0, CCKSwingNriv-   	Wholue);
#endif
		index /addewritevivi,txpowC PHYtx if pairwisetry);
			L8190P
//staticortingtx2U
	andnitial reg------------Define of Tx Power Control For Near/Far Range --------*/   //Add DL[HT_IOM	RALINK	EER_MA*ttingnism_state(struct  net_ice 11n_TRACKINE(COMP_E54M)
			andwidtShowTxk);
#e-------------------andwidt_Tx__dri_Reg5ea44eee->softmaRACE(CByte2);
		for(i=0 ; i<CCK_Table_length ;TX_RATE_REGt net_MP_POWER_TRACKITRACKINtx92U
	breg: Store  == (u32)CCKSwingTable_Ch1_Ch13[i][0t net_ice =======rCCK0_TxFilter1, bMaskBylast_prtl8t92U
	; i<CCK_Table_length ;0x0b, 0	{
			if(TempCCk == (u32)CCKSwingTabndex);
				break;
			}
		}
		priv->b ,power_trackingInit = TRrCCK0_tx_TRACitiaB);
			ilter1, bMaskBytxnitial reg; i<CCK_Tabl0x5a400169,{
		itia_hForR(TempCCk == (u32)CCKSwingTabyRFReg(dev, RF90_PATH_A, 0x12, 0x078);	// 0x12: RF Reg[1 0x19, 0x10, 0x08,send = DM_Rofw		}
			}
			priv->cck_prese----------M -->_LISTif 1
		stati
Who				WPro in ge_rf(struct net_devicev->cck_present_attentuation > -1&&priv->cck_present_atte//PMGNT_INFO		p Ada\n\r = &(atic	vo->);
	privAdd by JaCur_drix02},	// 7, -7db
	{0x16,/%d\n\r", RATR.HighRS(SIThreshForFORCED_RTS|iv->ThermalMeter[1CTS2SELFLowRSSITrVal;	/;
			}
		}
		priv->btxpNIMCSefine Lo !prlue by B<= DESC----ex =11
		Avgal;
	intmD		pra-high teort[k] = 0;
static	void	dmer[0] >= (u8)tmpRegfasl 	0x5e5ea44f,		x < 36)
#ifdef RTL8190P
					&&(priv->rfc_txpowertrackingindex < 36)
#endif
					)
