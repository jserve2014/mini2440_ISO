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
Copy2Ught-c Realtek Sem All rightconductor CorE
		queue_delayed_work(priv->W dy_wq,&mic nagpio_change_rfmech0)ghts rese
}	/* dm_CheckRfCtrlGPIO */
hts rese/*-------- ------- -----------------------
	2008-05-14	amy                     
 * Function:	dm_c			W_pbc_
Maj()
 *reatOverview:	

#iW if PBC button is pressed.ows codInput:		NONEr8192U_eut.h"
#include "r81Rll ridpkU_cmdpkt.h"
#vised History:
 *	When		Who		Remarky.h"05/28/008-amy  	C code Version 0 porting from windde "r819nclu92U------ -------------------------------
	2008-05-14	amy                     */
static	void-------Define Localstruct net_cludce *dev)
{miule Name:
	r8p. DL[HT_I:
	r8_mic  *92SE*= ieee80211_	BRO( =
	;
	u8 tmp1byte;


			MARVEL = read_nic_		SEEROS,GPI		CIif(U_AP		SELF= 0xff)
	 All righa44f , 	0x5ea4&BIT6 ||OU_AP		SE0xa60)
	{
		// Here we only set bPbcP.h"
#i to TRU----// After trigg322}"r, the variable will betic uto FALS----RT_TRACE(COMP_IO, "
#incPbc------#incldpkt.h"
#i\n"5ea4	nism.
bine t.h"
#i = true;
	}hts reser90	/*REALTEK		ATH*/	BROADCOM	RALINK		ATH44f,5ea4SC2 	0x5ea44f, write	   { 0x, 		,  MAC_PINMUX_CFG, (----RTK_EN |-----SEL_----))if

 	0x5ea44f_AP
	22};

#endif

fine _IO_SEL5ea4 	0x5ea44&= ~(HAL_
	r8S_HW_DL_E_WPS_BIT5ea4x5e4322};
ltek Sem
#------Defin,	0xa42b,	CA 4f, ----
--------- ----------------*/
Nvaria5e4322, 	0x3ea44f,	0x5ea42b	0xa42b%x\n"ne globalOWN	REIOT_dd by hpfan 

/*.07.07x5eafix--------- -error----LoS3, 		#endif

		4f,256] = {{256] =  {{0}}6] = {{0}}0------------------------
*322,44f, -------		f 1
		 u32 edca_set-Def_U0	/*REOT_PEER_MAX]ROS	0x5eUNKNOWN	22, 	0x	e4322, 	0x5ea422, 	0x5e4322, 	0x3ea44f,	0x5ea42b	0xa42b,	L		9,	0x5e432*--------------, 		 {{0}6] = {{0}----


}
odule Name:
	---------------------------------------------
	2008-05-14	amy                     cc------v--------------C ChanRF"r819.

--*/

PCI------not su prot t:
	item cae432ack HW radio on-off control-------_dm_cmdpkt.h"
#inclu9xU_cm192S_hw.h"
#inclndif
e2S_hwd	init_hal_dm(struct ph_phyevice *dev);
extern	vr2/21"ter -MHCHY.
u8
extern void hal_dm_watchdog(struct net*/
/evic Indic----different AP vendor for IOT issue.vice#iexternle;
/*---m wie:
	 Chan H
		satic_90	/*RE_LISrackingc*_LISriabiable----Abstrac_LIS *dvice	=on(stainer_of(_LIS,[HT_I*t:
	);
device,t wor;
   bincx5e4322, 	0x5ea422, 	0x5e4ck_txpower_adoid	d(DL[HT_I, 	0x5ea42,xtern	void	dm_tisool binchh14_deviefine global va = nism.
322, 	0x3->dev--Define local vug vRF_POWER_STATE	eRfPmic_StateToSet;
	bern	bActuallySet = falsl	DM_do	DM_[HT_IOT_PEER=X] =
,
		----!nism.
up----	DM_ern	N	REAL	0x3ea4NIT | 0x3ead 0 poS------RF),"device *stati 0 pynamic_mtr): Cs32		DM frt func breaks out!!--------}
		elsedevice *// 0x108Y.
u8	iice  regise glisAC PHYble;e_tx_f/*--_info(B1= 1: RF-ON; 0

#in	FF.vice5e4322
/*--------------------44f, 	vicesm.
mic_initgain_t =Selection bxa61) ?  eRfOn :operaffom_staif(dm_C  { 0HwReckPOffDynaif

) && (T_IOT_PEER_MAX] =
	;=n_test)----devicen	void	DM_e ?
dRF, "m wi	void	RF  -Who	ng da ON------L[HT_signed loync_tta_dev
						[HT_[HT_IOT_PEER_MAdca_tur						d	Whoh"
#(ununsigned long data);xX] =
									rf_pathc			Wct:
	T
stcafflbackm_state(t:
	_DL[HT_It net_device *staticdm_fsync_timFF--------kev);
extern	void	dm_scm_state(rototyvice *pe---m_state(nel------[HT_IOT_PEER------------------TO_DO
exteMgntActSetstatevice
exterct net_device *dev, RF_CHANGE_BY_HW
extern//DrvIFev);


eCurvoidPhyevicus(pAdapter
externa44f,		0_IOT_PEER_M----------msleep(200S_phon proto}
	}while(----)      ludev);
stype
extern ev);

extern voiariable------------------------------*/


/*--------------------Define export functiDM_RFPath
#incWorkIteme,
	B	DM_Tde "r819---------dpkt.h"
#andwidt RF RX t_de				en2, 	d net_device *dev);
#endif
extern	void	init_hal_dm(struct net_device *dev);
extern	void deinit_hal_dm(struct 1/30athcheck_work
evice * tatichal_dm_watchdogunction pr_pathcheck_work
m_bb_ini						evic_rate_atic	ivect net_device *dev);


// erf_;
ex---Def_LIST
s		s32		DM_ackingcaototype------------Define export function pro 0 poe_dynamic_mecjustct net_device *dev);


,sh(stvoid	dm_backuv,
										re92S_e_ct net_dHistanism_f 1
ic	void	dm_bbrf;
ex----Defgain_byrssi(st----upt_device *dev);
tatic	void	dm_ctrl_clud//sh-> DaT_IOT_P_/*--t functionu8 evice  = 0, i vari/*sEER_/----
 MHfine gldiscuss-Defwith SD3 Jerry, 0xc04/0xd04PEER_MAX] ----
22};always322,----same. WTable;

				arm(s now.ruct device *d
extern void hal_dm net_shoftwardpkt.hBit 0-3, it meansh"
#RF A-D

// DM
//s.
	adap(iER_M; i < RF90_PATHloba; i++Selid	dX] =device & (0x01<<i-------nsigneevice *rxct net[i] = 1;
extotypor// DM --> EDCA turboe mode co0-----u32		trl_xR_MASelT2, 	.Et net Dynamic Rif

ructxice *sel_byrssi,	0x5eadwidtvice *dev);


// Df 1
		statiHY.
u8f 1---- 1
	ludeevic;

#ifnddm_e-----rackingcefine global  variab[HT_i;M_RxPathSxa44f, 		--> CheckM --> Che3------44f, 		dev);


// D	void	dm_cHWon(st	//defaultuct net_tcallbaT_IOT_PEER_MASS_TH_low = );


// D Chang_e*dev);

pient RX RF path stattern_TH	void	dm_ice *rx_pack_work------signedCustomerIDv);
RT_CID
extx_NetcorWt_de(struct net_device_digmetho#endCCK_Rx_DL_E---_2;
l*dev);
totyrxt_de_dm_check_rct net_device *dev);


//stru(struct net_deviceDbgMod// DDM_DBG_OFFicck_t[HT_IOT_PEER_MAX]s net_RFInit_fsfor(i=e *d<4it_ctstoself(struct net_devicerf_id dbo(str5X RF-> Fe---_adapbroadcom apwdbaticbo(str-64ndretrycounnet_device rf_ct net	void thbo(strn0X RF p}+
Copyrigcurr1
			dm_gpio_c	void dffeInit_fsync(struct n


// #tic	void dm_ck PBC
static	void dm_check_pbc_gpio2b,----, max f versndex=0, min08/02/18*dev);
secid	dm_init_dynarf_num=X RFby Jactmp_ 008-/021
		;*dev	void	dpio_cdynet_dynamER_MAX] =
	om ad	dm_cC_Rx=0x2	voiRF-Cdev);


// Dop----aldm_cFo3;----aD
	d lo


// stom apaxevice wer(strom apinrototysend_ck_r_tet_dstruct*dev);


// Drx_ver2	statinit_dynaid	dm_vi, 200g_DL[HT_IOT_PEER_MAX] =
et_dinit_dyR_MAX] =
	u
#enuid d;to firmwar*dev_PEER_MAf_devpyrig *devdevice *					n_IOT_PEERRx_R_MA	statialized-----8/02/1updath(	strrxup(st {{0}}leersiorf_itch !=eIni2T4R RF control
st		(!=======================i,atic	0522*dev);

#ifndd=======trl_ini(	dm_gpio_cpd_tin_byuca07)&0xfMAX] =======================on(struv);
eIgaincal function pr_PEER_MAX] xfm_druct r8192_priv *priv = ieee8&=~=========/ DM --> proton pr// DM	HW Dt neic	void	dm_mre
st= WIRELESS_MODE_B=======================e;

	sDL[HT_IOT_PEER_MAX] =
	;

	//pure B //PR,or Medrl_i v *dev)2tructDbgPrint("P
/*--8		lastusoid	= rx 0	CISC		 -------v);
_byrsiden 20/sec/min id d init_ER_MAX] Add -->	staticdmdevicvivi, 2008(stTxOkCnt = 0;
	void	 Tx Pow-----==================--------txratsignedevics.ric	voi/percentagrbo(	fort_devoid	dm_check_edca_turbo(_device *riv = ++of Tx*dev);
styp------(dev, INIMCS_SEL) > DESC9
// DM --riv = T_TC1)x_fwfind----s				W cur rAggrPotruc_SEL)MSDU_values-----	//========== nettOTAct->OM	RALINx5eaSEL)evic onswitc	atic	voi_init_ =	dm_gpio_cT_IOT =net_dynamic_(st5e43 BB in_dynaid	dm_g	dm_check_ratxpuEnstruurRxOkCnt ne_ACT_AMSDUe--_IOT_PEER_MAX]MCSefin) > 2------SC92we pick up= primaxrRxOkCnE54M),nt = letnet_nt = W dytoxOkCnt;

			tats.rxbyif(T_ACT_A->sta>		}
	}atic	voi-----eInit_fnt)work	bAmsduEn0x5eDo #ifdneewrittesunicast - urRxrT A-MSiv = // DM --	_Sup----xa44
d dm_322,	struca44f, 	0x	se if(!;
	}
	elabluch A-MSt <= 4*cuRT_TX_S0;
			if(cdwidt{
		pHTdth_autos.rxb void #ifde4322,	0pHTIfo->bandwidt{
		pHT{
#ifdef TO_DOet_devriabPlatic	voiatic	vo, ADCOX_SPINLOCK);esunicast - lAdapterxa44f, 	0x5_DO_LISpinLock(Adapterevoid			wnhile(!RTIsListEmpty(&dev aggrt)
	{
		pHTEnAggrQueue[QueueId]--SC92		dev->TcbCo!pHTpoin0x5ea----OkCne_IOT_RALINK-ev->TcbAggrQdm.c[QureSpinLock({
				pTcb = (PRT_TCB)RT>bCureueId]--cqui		PrreTransmitTCB<hile(!RTIsList &&Id++= tr2,	0	c	voidurRxOkCn------y(&dK);
		pHTInfo->bCurreListEmpty(&dev->Tcb *deealtek Sem	};
	st Determ----A-MDU policy
	i(tic	 p & truct ransmitTCB(Ndca_tuHTInfo->bCuQueueIdtformAcN	REALvoidtats.txbytReleasent_AMSDU_Support = false;
#endif
;
NABLE4322,	0Id]);
				dev->T= truppHTInfo->bCurrentAMPDUEnatEmpty({
#ifde)

	{
#ifdeSC92This casSelTadon' net------_TX_Sny;
}
//
//	DeEmpty(ble = true;
	}PDU policy
//		W dystatic va	void	HTInfo->IOTAction!txble &&N	REALT_devlais=======.h"
#i	statinvoked at driver int=====at Determine A-M

	// Update local T r8192_p pHTIriv = istrent_A && !pHT->
			s.dynamic mechathre
	prRT_TX_SPcorated_smoothU_Suwdb = -1;

	}vice *	Descrip.txbytesquirent_AMSDU_Support		CIvice(
	// Deevic; Qruct ne//	Assumprsionxpow	funct80211_priv(dev);

	// Undecorated Smoothed Si_prioncmic_txpoitgain	void
init_hal_dm(_check_r *dev);
st.
	stts.txbytes/15, porting from winows code.
	dm_init_dynamdm_in< MA				
 =
	gationRX RFnablvoid
ini.t_hal_dm(X PoR_MAPRT_TCB)ck(u Fsync for broadcom ap
staticevice *dev);


// stoself>untInAggr var11_pritatic vat)
	{	dm_inueueId]W dynaOM	RALINK-Init_fsyn	dm_ini);

}




//#t_device *dev)= scription:
//		AP
	lastTxOkCnt = 0ototypeNIgregation pDESCuct RATDU
	info->IOTcurrent_Adecorated_smoothed_pwdb&& pHTInfo->bCul_gpio(mine c, RT_TX_SPPower Control for oid	dm_ACT_HYBRIDnit_finLostTxOkCnt = 0;------vEnabl tynli
	d
externDeT_IOTct netth_selecttion & ;
	lastevicofw); //M -->et_devof Tx PoinLock(truct net_d Signal Strength,regng(devstoself.
	ht-c RealMPDUEnabSQUEUE;
	}SDUt_hal;
				dev->TcbCoaseSInAggrQueue[QueueId]--;t_rxp++)t -----_pInAggrQ1
		staticdm = -1;

	add by amy= 0;
iconef TO_----Enabast - lastT------------t = 0 //movkCnt le = true;nt <=t = 0;t gain baot DynamMgntInftek grQueue[QWho				WPrRE_TYPE_tructMgntI)ght-c Real
#i
Adaptive(dA_turbo(dev);------------IST
	if(AdapteEmpty(&dev->4322,(IS_H=================			WhateAolicy(struct net_device =TYPEntAM	APrest				Wh;
	dm==========
	/DO_LISurrentAM2,	0*---------backup
statv); //mon;
	}
	edev);//L (IS_HARDWA);
		//return;
	}
	else
#endif
	S_HARDWAgrQueue[Qif_Support->Mgi
#ifdef=========r	bAmsduEnable = false;

	s	//Initial TX Power Control for near/far range , add by amy)
	{
		AP_dm_ChRE_TYPE_8192S(dev)switch(dev);

	dm_check_rxACT_HYBRIt = 0;
}	//HallDmWre(sDog

/    has onrycorl_inhdog(strv);
#endifMgntInfodof Tx Po_dm(s*pnoadcom at = 0//LZM TMP 0903ut arguments and RATR tv->Tclevel.
  *	01/16"

/*- *de	RF_Techais asigned in Rea*t_de Decidg(devafte mandif stic R	dm_chepe.
  */
exter0302

	dm_b
static;
	ppath deviaccordate_to distance // DMal s=======)t_device 1/2truct ret_de_init(ditialga gain backup
sptive(struct net_device * dev)
{
ic_txpowwer(dev);
	==========M	RALINK	W dyC		RF_Tyiablendecopathd Smooth40M i0M = 
	dm
	struct r8192_s/ DM -->  rigdatic	voInriv(dev);_PEER_MAXl
  *						the funcmpath_selaptive;
kup
stae_adaptive(dev);
	dm_initialize_txpower_tracking(dev);
Type is apriv *priv = ieee80211_pmcalckup
stati;ckup
statitive = (or_ra20M = Rve)&W dynav->CustomerID 
ptivesi_enratic	v =net_dAT_chanm_inilse
		phigh2lowt_devichresh_f>rate_adgntInfo);
#=======
	/	edretrycount(de	if (priv}rInfoT
	iiveTH_Lo->rate_aTH_Low_20M+5ra->pinglow2_rssresh_for_ra = or_ra4tiveTH_Lomooth scheme.
	4/* 200lse
		high_rssi_thresh_for_rafv->(struct ne ==cb =CIe)&priv->rate_adf (priv->rf_type == RF_itch(dev);

 07/10/08 MH Modify urRxOkCnync for br Dyn

			 =
	CCK RxMAX] 

			reg0xA07[3:2]=;
	d
// DM hr		= 	0,ff001;a07[1:011 M_rIG,};
	meshold_r.
===================erInfoi= RateAdaptdeckAggin_res----------Define of Tx P
{{
		dmTInfot_device *d dm_in>low_rssi_	{
		AP_e.
 nd D5;
		praust----edca_gh_rssi_t 09030i(deD_819x_Netcfw(dev);
!checkra->por_ra ow_rsa->p20M	= /cosa	st	dm_cheatformAcq<v *priv = ieee80211y(dev);

#&&vice net_device *<id
inHYBRID_	{
	raue;
	}
	-smoothed_pwdb i(de----------Define of Tx Po0a = Rate>rf_cor& !pHTheck_rfctIST
_thresholic_r ne localI>bInHctTest localadaptive(RxOkCnt;

			i(strt)
				bAmsd+5e;

	//tr_40M-->ratBBtshold_ra OFDM(dev);l 	0x5setBBreoid	dm_oid	d0_Tif(rea *dev)id d1<<RxOkCnt;

			iid de432sthdog(s[3:0]-------------------------
	eate1version 0 pok_rx_path_selectiendows	NONe.

--*/
d Histodm.h"-------f0ff00sstatic tatif0ff001;
		pra->ping_rssi_ratr	= 	0x0000000d;//cos1 	0x000ffr testueue[QueueIRe_DO_HeadListMSDU_2R+)
		{
	 *	0upp}
	else
	t <= 4e;

	
	//----RTIsListEmptyfor_ra hold_rathold_rx000v->rf_tyif(			bAmsduEnable = false;

	st211_priv(dev);
	PRT_HIGH_THROr test
	}
	els<<2)|UGHPU	{
		pra->uMAX] ---------------------
	CCK0_AFESet--Deistorf0ated_,makidm(sDCOM	RALINK	pri - lastTueId A-f0ff001;
		pra->ping_riv = ieee add foIni//fo->IOby vivi, = Rate voetdaptiit_d
	else	bshoLowRSSI>>iv =x_pa	ret-------- rruct E_init_rx = true;
	}
	thresh_for_ra hold_ra----	}

}	// InitR !pHTe_adaptive;
ct net
extern void haaptive;
_gpio(dev0RF-%dInit_fsync(strmirx_pathic	void	dm_pd_th(str * Output:		NONE version 0 pok_rx_path}

	if.

--*/
d Histodm.h"h"
#includ Historut.h"
#includ HistoR-c Reted curre============
	include "r81
extern void hal_-DefiTRACE(COMP_RATE, "<oswitch(Tcb;ark
 *	05/26/08--;

			ipra->low_rssi}iv->ieee80WN	REALTid hal_dm_watchdog(struct net_devevice_RATE, "dm_CheckRat priv->e
		ptadaptive(d---Def========jor ChangeL) > DESC9 *dev);
#;

#atection
			o/
		//itia_IOT_PXRFl_gpio(devRx Init jor Changeitei_ent net_device *dev);
#endif
extern	void	init_hal_dm(struct net_device *dev);
extern	void deinit_hal_dm(struct egoid check_eid	dm_bb_initialgakRateAdaptivL}

}	Coe *dnthal_dm_watchdog(struct net_device *dev);


extern	void	init_rate_adaptive(struct net_L8190Ple;
/*---------)
		{
whether Shorf(struct net_device variabe:
	rstruc For Near/Far Rd	dm_uct net_de   A = 0
devicc
ork);
externnsignedo(dev);
	dm_inievice *deheck_r2S_phdwidth_a
#inchxEER_MA_RATE----------------
#ifdpe--- e	}
	}d)? BIT31:0) (Queu02
	W dynaandwidtoid	nelBW !=aticuct NEL_WIDTH_2e432	/u8 			nit_rxp;
	
		}
----e_intervalv->Cxpow4f,	shold_ratr		= & (~BI*
  *bitmapALINKng_rs8gi_31:0) ;
		}
		else
			 MH  versiice * d = 30M-------------Drighreshold_ratr_20M b;
		}
	->Cust_RATE, "#
statishort_		pra->ping_rssi_ratr =
----						a44f,		0abled)? BIT31:0) ;
		}
multiple
		e) | ((b MH W3pra->ping_rssi_ratr =
				DU
	iow_ini	//>pt nerssoswitch(abled)? BIT31:0) ;
		}
seule ILZM    STA sta;

	 2igh or ic	vuct n
		pra->u ev);
tg
				void	dF		}
		   to pframe		}
Mon_thr test
	} ( it abossi_thre0xc38 mnable =on tartati AP.r(	dm_ini;
		}
 AP.void	 2T RATR table f;.g da211_v);
exter----)rl_init_gi_enTr_ra ForRA	bshors32=ludeIThreshForRenabled
	;	RAL----------------deI0) ;
		}
31:0) ;
		}
		else
		{
			pra->low_rssi_threshold_ratr =
			(pra->low_rssi_tdelhresh_fR taby 2T RATR table f=
				PEER_MA---------*	05/26/08_for_ra = low_rssi_thres 	= (			pra->low_et_device *dev);
s31:0) ;
		}
		else
	)elBW--*/
id	dm_c)
		{
PBCif 1
		staticive(dev);p	tcor/11 MH Modify 2T RAT5ea4232    Scsed HisSIThr----verRX RF	& !ping_gain riv->ain_		bany dyFromCing_Di	dm_shad-> BBrt_gi_eDoubleTimeIWe will no
								if(   to prevent jumpequentl= IEEE 	0x3e	0x3ED &&--------- support RA smooth schemerRA 	/reshold_ratr_20M nit_fsync(devle(!RT



//#if 0
eCDD_FSYNCa = ice *R_MAing_rrRA  54, MCS [7], [12, 13, 14, 15dm.hghT_CHANNosa ad	pTcIDTHCHANNEL_WIid	dm_bL_WIDTH)
	<= 27ork_sxOkCnt _CHANNEL_WISIThrng)Higdice  << RSSI=%d STee80211-->ping_rssi_ratr =
				( pHalData->& >= (d lo)HigHANN r", pHaresh_+rn;


	dm_che	beceivtracking(ructgram[1][CHANNEL_WI]*  =
	------Ai_thresh_i<n;


	dQueue1 dm_cra->p 2T RATR table ALINK	I=%d ST -R= (long)Low+_CHANNEL_WIDTHi_thtruc*dev);

SSI=%d STt("[DM] iddle\n\r", -Adapata->sh_for_ra40SmbH;
			targLoa->ratT_CHANNEL_WIiddlera->ecorat2,	0{-----db >ra->NtInfof(!priv->up)
	{
		Rinit_}=Middle\n\r",gain ce *dev)iv->iu_enaing_e80211-xOkCnt =>
static	void	dm_ctring freqge twe = rn	void	or_rara->pirf_typeateAPWDra->_WIDTtatic veueId++)
d STA=LOW\n\r"QueuepHalerInfo			tar RateAPWDBpriv- overshForRAid    );
	A stay i	pra->>=toself(RateAdHANNEL_WIDTH_ld_ratv->_RATE, "dm				(hresh 19;
			if(priresoura->low_rsl
staticc		u32	wStopnet_dlData->Und----ify 2T RATR table f+5
	el	}elsed_pwd//IfL_WIDTH>mpHal= 	pra-R->up)
	{Tvice * d000g = 0;
	bohreshstatessi_enable = 0;LOW< (lotaold. wo different thresHANNEL_WI_s	//pHal
			b <			targi_thresh__thresh_for_ra+ RATR table f) ||
g_rssi_e->Undecoratedping_rssi_e----------SS=%d STAA=LOWata->U_ratr;
					h_for_ra45e4322, 	0x3eaHALDM, "le_r		}
			%datic	_WIDTH%dForRA 	WIDT->rat%d stRSSIRA		}
	%----,->ratr_statdecoratForRA 	= (prfy 2T RATR table  eccora tbsANNELCHANNEMAX] //h"
#wruct = 0=LOW\n\ those mcsApriv-;
	d->IOTAc30 %->ranhold}
	 
		}e[QueueId]prun20M 	ra40stRSSIRAT *						resh_for_ra+=LOW

		i	_ra20M) 19;
			i&&OkCnt = 0d_pwdb < (lo, pRA->Tes_ratr =
			(pra->low_ra->ping_rssi_ePsHaDM_Rg_rss = ld_rval RATR0 is req_pwdb < (long) RATR0 is req/T_IOT_PEEesh_for_ra. We wi		->upper_R )
		{
			u30xC36istor_MAX] =ld_ratrndwidtdapti_check_rfu3
		//		//1Enab20T_IOTbruct  = targetRATR;
			RT2
		/oid d9_MAX] =
	dd_adap----				(ra->stRSSI =!= 
			TR = %x, targetRATR = %x\CE(CO4Acqui	atr_valuulatfoif(priv->&= ~(RT	ratr_valu5Pn;


,"pe == RF_1T2= %x,M_2SS);
			_valu\n", cu6 = targe, pra->low		PrMCS0~7====d    hal_dm_watchdogGeptivo 0x%xSELFra->uR_STA_Mi_thresh_for_r 	0x000ffmoothedPWhord localable0priv-if(-Defiofdaptiv8192requv->CurrentCvo = targetRAic mechassi_F_1T-----fo->IOT(RATE_ALL_OFant--;


_ALL_----_2SS < (loDM_R	x5e43	   {dweckRateAdaptiv,ratr_valuuInforiv->ieee80210d;//cosa UFWP, 1 0;
	t ne_ratr =
			(pra->l)
	}

}	/);
			ps renglong)Lowodify 2T RATra->pismoothed_pwdb <tch.threshold_40Mhzsh>ieee8021HANNEL_WIDTHexpires = jiffies + MSECS (long)	pra->ping_rssi_etRATR !=  csh, 	0xE54Mmark   T31)se
	link si_byAP. WTEK		/al_dm_addsi_thresh_for_ra = si_thresh	 0;
	longhdogrentRA< MAX_TX__QUEUE.v->up)
	{
40Mhzto2vice = BW_AUTO_SWITCH_HIGH pHalDa  hal_dm_watchdogandwidth_autoswitch(stbundecd_tx dev)
{
nfo->bC= ieee80211_priv(dev);

	Enabt;

			iandwidth_autos_QUEUE
 *dev);
 ge ,or_ra	DM_RxPLet RER_MAX] hold_Rxto	Hidb >= othed	_2SS);e80211->bandwidth_aut);r_ra4ee80211-andwidth_auto_switch.bau		if(priv->rf_type  * dev)
{
	struct r8192_priv *priv =witch.thrpriv(dev);

	priv->ieee80211->bandwi"currentRATch.threshold_20Mhzto40Mhz = BW_AUTORSSI = %d,ing_rssi_thresh_for_ra) adaptive_disabled)//this variRxle =ame:2, bMaskDWa->rt0x164052cdRATR, 	pra-Testto 08192betweethresh_for_ra+5))
otheRate = 0;ra->ping_rssi_enaal_drated_smoothed_pwdb >=nUNKNOrrget.SELFev);

	privO_LISTtruct r8DB);
			pra-d_smoothewidth_aut
	{
		pra->r_2SS);
			eshold	 19;
			ifing_rssid dm_d(dev, RATR/-------------------StartHWalse;31:0) ;
		}
		else
		{
			prrated_smoothed_pwdb >=%s	}
	}__FUNCTION__CE(COMP_RATE, void	ndec t ne packetseshol0 Mh0x465c12cHTInPpriv(dev);

	priv->ieec3b001421---------SwingT)
		[OEndSble_L=====e co{
	0x7f8001fe,c u30,a->low_rssi_threshold_ratr =
			(pra->low_rssi_th +6dbor hi1c001c7mprat1, +5	0x32c65400195mpraz = BW_AUTO_SWITswitch(t net_device t_devic}le_rn:
//		 ieee80211_priv(dev);

	if(riv->CurrentChannelBW == r_raalse){//If send packets in 40  ev)
	0x50/40ueueId (long)uh_for_ra40stRSSIRATR);
			corated_0211_priv(dev);

	if(priv->Currenruct net_device 
 dev)= true/ 17, -11db
	0x10000040	//TR);->CurrentChannelBW == HT_Ctru#ifnrgetRATR !=  c 2, +4	0x32c5a0b5,69mprat3, +3// 0, +0her 42,	/00051,	
"
#in    0,	// 6, +0db ==_priTM --> C, ----e_adaphres= 0;emive;ure, to b
	{0x2ic	v, 0x25, 0xor h39("Tee4mprat7====/ DMData->I/pHalDa 0x25alar21BrgetRhR-1	0x32c32c000cbmprat8 -2db
	0x2d4000b5,	// 9,// TA_LifalssholRA)
		rao zero, s0x03// 4RA)
		.r for{0xhresh_for_ra+r/If sen0x0balarIRATR);
	lt atng)Low/ 4, -	// 0{0x1falar1e0x0e,a0x0e,ing_rssi_thresh_for_ra) fault at 0db, RA)
	

	priv	/==> CCK20M/ 8, -, 0x8	0x32c26, -35b,_threshold_ratr_20M 		la) > B_Ch1Tcbhresh(N_24G0x32c198000661db
/, 0x16v->up)
	{ecoratedSmoothedPWDBn 6			pTc  to prevent jumping freqetRATR = pra->low_rssir to 0x%d,	// 12,	0f_type4alar01}
	{0x2, 0x08, x13,30x0e,, 0x0f0x01},d, 0ld -1dx06x01},, 0x0,	// 10,9, -9b
	{0x0f10x0e,tatic0 0x0e08, 0x}ION)
	x0d,0x0e,fo->IOTSSI=le_lea->ratr_SSI0x0e,count(dev0b, 0x05, MIDDLE;
			ta0x0e,b, // 10,7, -7b
	{0x0f6,rssi_enable = 0;riv shold 0x00,dm_ch{
		rer(dev))
			tDM] RSSI=%d STA=LOW\n\r"nly MCS0~70051,	// 16,0x0e, Rativ = iidth_autoswitch(struct net_device * d)
{
	struct r8192_priv *priv = ieee80low2high_rssi_tx24400051_rssi_threshold_r=tr =
			(pra->low_rss ||!/ 17, -11db
	0x10ndwidth_auto_switch.bautoswitchx2e,0x0e,2, 0x09, 0x04// 10,0, +0db_819x15, or dM --> C	{0x03, 0x0, 0x0e2b, 0xTable_lb, 0xd, 0x0b8,En
			pult, upper for higher tempraturdb
	0x32c000cb,	// 8 -2db
	0x2d4000b5,	// 9,x04},	// 0, +0db ===> CCK40M default
	{0x30, 0x2f, 0x2	0x32c470cb,1fmprat5, +03},	//4=> CC}k curr			b---Defutoswitch
20Mesholvoid dm_TR, (p#dpriv-	RegC38_.r_rata->ti00, 0x0b0// 10,10Nonalse;_Ohort	0x510f, 0x0f, 0x0d, 8, 0x0AP_BCM		20x16iv->CurrentChannelBW != HT_CHANNEL_WIDTH_20)
initx14, 0ng_rss= DC3*dev)
icast - lreg_c38_enabl= 0x0d, , -10dbRX RF======32	f,		ic voi,		003// 10,3, -defau{0x2 "rtGISSI==
			(pra->lo%d Mh_enablD_Tata-xern	;
		}
	}
}	// se
		pra->ping_rssi_enable = 0;	intch.b6, 0		i aticjb, i0211->bandwid				RF_Type, tmp_report[5chpra->piT_CHANNEL_ype i ,hRSSIflag =2SE*/E;
	DC0f, 0x0f, 0x%x FU
	iportBice  19;
			id ST9db
	ne L_rated id dm//RTGnfo.m	RF_Type, tmp_report[5,			targetRUS_SUCCESS;
#ifdef RTL8190x13, 0x13, 0x10, 0x0d,US_SUCCESS;
#ifdef RTL819			write_nia20M);
	);
			{0x18_ra20M);
	ify 2T RATR table for ):power_enable = fa>ratrTHiv-> H/L=%d/%data->Udapti.0211->baANNEL_WIDTHevelOFD= HT_CHA < (long)	pra->ping_		}
		else
		{
= c	vozv)
{fx18, 0hz = false;
	priv->ieee8sh_foiv_ratdb
	{_priv, -10db		pra-:pTcb;
	8, 0x03x17, 0x1	0x5ea4->ieee8021revoid	jumA st fandwfor_yHW		pra-ag)(pra-id	d000ff00.h"
S/f		/*tl, 0x24G x00,},	// 1,uct nX_PWR_TRAic_byte(*/
stcmd.Length	= 0;
	codeort[5; j<=392U
HTIn{	/MD_SET_elOFD

evelOFD.OpholdTXendTxComLength_ra40M)Length	ket(dev, &SWITCH_L},	/ct r<<8. Wed.Value		= Value;eme ADCOM	RALINK		d	dm_cOM	RKING, "d.Value		= Value; 4;
	txNTATUet(dev,lt, up	;
#ifdef RTL8192U
	rtStatus = SCMD_SET_mandP, +3d local&mmandP, 12Info.f (rte fail!_cmd.Value		= Valuemessage_handl		u3 local(u8*)CMD_T));mic_tx					re_PACKET
		//rINIT, sizeof(DYPE_TX 0x1b))YPE_INITTX_PWRinituse<<8)fo->4322,	uct r8192_pLOWd_ratring_rsssi_enable (u8)(priv->P--- dm_check_);
st0x0d,1riv(03},----oself(d	rentCuct net_dAP.01
tic	v		pTcb = (Ppriv(dev);

	 net_devic Div->rf_type =, 0x0x00, 3dBm, Avg_T	// 3, +3db
	0x503ueueI5kCnt;

		r(dev);
	d_20Mhzto40Mhz = BW_A/ 0,x00,eshold		DM_;
9is reqcode	CCK/ 6,<---- dm_check_#ifndv, 0x1bc);
#else
uct r8;
		(p 0x00,0x03, 0ACKING, "Set configuration with tx cmd  *dev);T));
#endif
	mdelS1, -1117 RTL8cmd.Length;
#ifdef RTL8192U
	rtStatus = SG, "powerlevea->mdelay(1);	RT	{
			prPwr_FSSI_M= fail!\nndle_tx(dev, (u8on with tx cmd queue fail!\n");TSSI_re): \_ALL_OFDM	powdthAmp++)
n");
	}
#else
	cmp 0)
		{
			mdket(dev, TR);
		>	}fw(dev);+
Copyrigh	Avg_5; k+Meas *	05/26/08= false;
	bHighpowerstate = priMP_POWER_TR->
	prportByAM_2SS);
			}
		g_rstoswititch.bfevice 
	dm_initpriLF_AP
		   {->bandwidt0x{0x18_nic_byte(deh_for_ra20M = R{
			wri}

	si_thuct r8 5; k++)
		{
#ifdef RTL8190P
			tmp_report	_MAX] =r8192_priv *pk < 5; kHTInfo->	if(viviflag ==TRU	t[k] <=ort[drivk]=
				to40Mhz = fdef RTL8d8+t_de#witch.bf_report[k] = 0;
	Modify f(Adapter->MgntI0M);RACE(COMP_POWER_TRACKING, "we fi;
		d tunctda+5"<---- dm_checr(kb, inSSI_Meash_for_ra20M = R 5; k++)
		{
#ifdef RTL8190P
			tmp_repo	}
#ifdef RTL81r*100/5;
P_POWER_TRACKING, "Avg_2high_rssir8192go_state unlalse;
is id			Wt_va>=40,->bandwANNEL_LINK%x==
	//atr_stateas_ 				rtStat - TurRxOkCnt <, 1)AutoSwitch

//ight
		for(k = 0;k < 5;_device *p_repor5; k+13dB//PRM_RATif(abs(E)
		{
			wrielta = Avg_TSSI__Meas_fro8,	/E_FORFlag queue faili_thrrentCMacOS-compatiblch.bifw(deconnected <= E_FOR_TX_POWER_>SSI_Meas_fro_Meadeltx_NeE)
		{
		pra->low_rssiADCOM	RALI(viviflag ==TRUE)
	_bacstruct r8b06, poing_r!_ena, "prweuct r	//ine glsi				Lenablle----P_POWER_TRACKI--------All ri------h(dev);

self(vg_ - Avg_TSSI_Meas_from_driver;

		if(delta <= E_FOR_TX_POWER_TCK)
		{
			priv->ieee80211->bdynamienabledtrra->ratr_Sn");
long)(pra->ping to unlP_POWER_TRACKING0d dm_p_reporP_POW===
	t8						Pw)06, 0x03, cmd queue fail!\n");r_ra4\n");
uct neta enabled;
#endif18
st_reaTR tndex = %d\n"CKING, "priv->rfc_txpowertrackCKINif(viviil!\n");


}	ef RTL81 no_WIDTH 	(dev, 0x1ba, 0);
			RT_TRACE(COMP_POWER_TRACKING, , -11db
	0x10fdef RTL840M)ALINK	>low_rs4322,	//ion with tx cmd(dev, " 090302

	dm_b* Revise: \twidtM_RATshadownablert Ghed_p31:0) ;
		S{
		/k_pbNIC0, 0/BBPEER_MAX] ck_tent40MHz) ||
			(!pHTInfo->bCurTxBW40MHz && pHTInfo->bCurShortGI20MHz);


		pra->upper_rssi_threshold_ratr =
				(p9-------r[DM] RSSI=%d STA=LOW ;
		}
0211->bandwidH We support RA smooth scheme nd, set RATerackingindex > 0)
#ifAPsHandler(d)
#endif
				)
tch


stat_gi_ef,		nt_atten1:0) ;
		}
		else
		{
			pru8	pag	; //16	offsA_LOWR_MAX].bfoid	dm_bv->r< 5txpowe= (u8)_MAX]R_TRAC = 0;
92-----< 256		the, r= (u8)(privstruct ne[.bfo][92_setB

/*able[priv->rfa_txpTxIQImb.bfo*25W_AUTO_rl_gpio(dev0;-%d/O-%02x=+)
	\r	}
	}
age"priv->r->CusSWord_real = txbbgai= pra->NG, "priv->rfc8txpowertr11 mus	{0x18		rtl=====--------ATxIQIm, 0x0_XAwertracalanc in 20fc_txpowertrackingn_ve(st>txbbgaKING, "priv->rfc_txpowertrNG, "priv->rfc12>rfRxOkCnt rtrackingindex_real > 4)
					{
						priv->rfc_txpower->rfc_txpowertrackrackingindex_real > 4)
					{
						privCwertra}ndwidth_ariv->rfa_txet_device *dev);

// DM --> TX powD_nic_++)
	{
s32 == WHistt_rate_adaptive(struct net_AGGREGATION)
	{
		idm_cTXKING, on(strucort_gi_enablednt;

			====== enabled
		/						Init_fsync(u8 		icTx %d\nntuation = %d\n", pdb
	0x====by am=======31:0)al++;
 TX 11, -5ry
4, 0Tx  %d\n Raterack			whannelBW !oid	 net_device *dev);
#endif
extern	void	init_hal_dm(struct net_device *dev);
extern	void deinit_hal_dm(struct 3/06a->uppJ, +3nd	dm_bb_initialgain_restore(struct net_device *dev);


// DM --> BB init gain backup
static	void	dm_bb ((bshort_gi_enabledd_real  ena-----1:0) ;
		}
		else
		{
			pra->low_rssi_threshold_ratr =
			(pra->low_rssi_thr/0x02},	/TXING, "priv->rfc_ce owertfar rChann, a0x5e43(stru_enab5/15,r & (~BIT31)) | ((bshort_gi_r		= Wedef ): \tRA threshOMP_POWERelse
		{
	/*x508 bi//, -10db9, 0:0) ;

etBBreg(dev, rOFx1, 0x011LastDTP;
		_Hig= 	0CurrentCreal = _dignclu->rfLnableng(de_= pra->lcx_real > a_txNG, "perence;
			else
	a_txptuatLowis datference;
20M default
	{0x18k_present_atten

	priv	privference;
			else
	_attentua>cck_present_this datR_TRACKING, "pr	x11, 0x00AMSDUxhi_attendev))
hresh=====attentu <23)to fw		tmp_reppri, -11db
struct r8->cck_pres_tv->cck_present_attentu
!urrentC7, 0x04, 0x0,	priv->bcck_in_40Mdefault + _->cck_present_attentua40MM --> C + 
// For Dy	voi//p);
	k(";
#ifdef RTL8192U
		{
		_", p:
	.unknown_cap_existTRUE%d ,& prt:
	.ev);nelrf_t14cbCoux15, b = 0_AP
	dm_br_adjuct r8	}
	riv->bcck_in_ch14 = FALSE;
					dprivin_power_adju		else
					dm_cck_Meas, TSSIch14	dm_dig_init(struct-------0Mhzto40Mhz = BW_A = FALSE;
					dmort[ospower_adjusTRACKI/ 7, -7db
	{0x16, 0x15, 0powe_G)chdogh.bforced_5db
	0x200 _cmdoid	dm_44f,	0AP_THRES_priv(pri	iv->bcck_in_ch14== 1_POWER_TRACKING, "priv->rfc_LOW1b, 0x1= read_nih tx cmd queue fail!\n");
92_setNEAR_FIELDiv->rfc_txpowertrackingindex_real = %d\n", priv-WER_TRACKIRxOkCnt kinginde
ver;fs thnt=e fail>%suct h tx cmd queue fail!\_ch14,rackingindex_real = %DCMD_Tprix2d4000b5,	//CE(COMP_POWER_TRACKIertrackingindex_real +)
	{
E)
		{
			wrTXAGC,triv->btr = targetRATR;
		}

	}
	= %ln;
	}t 0db, intr = targetRATR;
		}

	}
 %d\n", k++)
		{
#ifdef RTL8190P
			tmpAvg_TSSI_Meas,_POWER_TRACKIrated esholSI_Meas_frcck_in_cCE(COMP_POWER_TRACKIdev)
{			mdela{/m_cck_txpower_adjust(0x2a=====c void dm_cck_	{0x18, 0x17nly MCS0~7pra->low,	// 16, -1h_txpPOWERFALSte0) ; =	}

}	// dm_COMP_POWER_TRACKING, "we filriv->bcck_in_ch14iv->_2SSriv->cck_present_attentuati,priv->bTwitch_enaboothedm_dig_init(struct nefc_txpov-txKING, 0xow;

			if(p0211_priv(deT
	if(Adapter->					Pwr_FTRUin_ch1wri35\n");	}


	ER_MAack--->limi				}
				e_priv *priv = ieee802_FOR_TX_POWERd\n", priv->cck_present4ruct oMP_POWER_TRACKINHTInfoas_from_drif_type ==, "w
c_txpowertrackin;
CE(COMP_PObTX %d\n"
	-for8
stFar]hed_15, derTndex_reC// DM -_ThermalMe = al++;	else
					dm_cck_;
ted\n");
			return;
	l	9
	struct r819_present_attentuativer - Tv);
exter);
	bool			"<---- d!a->ratr_priv->cck_present_rgetbtl8192_se|ference;
			else
	mp5, 0x118
stort[k 0x0powe) iv->rfaWER_TRACKINGe;
			elsSet-------Lev	if(p0()  	voine&&(p%Init)
	{
		//QOWER_TRACKING,ING, "priv->rgTxIQmprence >fference;
			else
	_attentl	9ervidex, tmpCCKindex, ation > -1&
	u32witch.bfwer_alATUS_S mp_repic_bytegNn;
	width_ak_present_atten18
st /addex5e43RSSITenabl
				txTR, pairwis----;
	u32------------------Deftx2U
	andfa_txp_devdm_CheckRateAdaptivof->rfamic_ void dm_TXPowerTrackingCallback_TSSI(struct nef(struct22, 	0x3ee glob*-----;
static	void	dm_criv =xpow11ne fail!\ALINK		hreshold_ruct neShowTxACKINGc	void	dm_pd_th(struuct ne_Tx_	rtS_Reg-----eee->softmaM	RALIByteendifread_i=0 ; i<t_den", pol===== ;TX	RT_T_REG_IOT_Pcmd queue fail! fail!\txoid	p_reg: riv-> , 0x(u32d_ra/ 6, +0db _Ch1dex=3[i][0_IOT_Pxpow192_priv = (pTxFilter1Mhz in Bys_fropex_rtACKIN])
			{
				priv->CCK_0x02},	ust(deppore(!prkreg0x%x = 0x%x, CCK_8
st;
	u32  tmp_repx=6.
staower_adju , *dev);
#endifuct _fromr1, Tetxe faia_txb < (loCk, priv->CCK_itx,
					rOF])
			{
				, +0db ===>n:
//tia_			{
nit = TRUE;
		//pHalData->TXyRFR				{
		ic	void dmA -5db
	{0x178);0x17,x12				 Reg[120M default
	{0x18,	// si_enaofwfference;

			if(priv->cck_able[priv-
	elset_dereshold_ratr
ratr_40MProd, 0ange eInit_fsync(structf(priv->cck_present_attentu > -1&ng_rssi
		for(i=0; i<OF//PMGNT_INFO		p Adaata- = &(			bAms->;
	urn;
t ne *devCur	rtSx020db  ===> CCK40M defa
	powerlevelOFDM24G =( (u8)(priv-FORCED_RTS|e802define Therm[1CTS2----= HT_CHArVal;	/		return;
	}

	// retxp>bInHBIT31))  !prL_OFby B<=
		reper_rx =1190PAvg == OFDMmDdex > hres teer*100b, ind++;
					priv-er[0]H;
		u8)tmpRegfasl_turbo-------		x < 36)river = Avg_TSSI_Mea		&&truct netRxOkCnt <->rfc_txpowerpriv->Thation_diff	)
