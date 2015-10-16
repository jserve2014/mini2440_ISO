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
#include "r8192E.h"
#include "r8192E_dm.h"
#include "r8192E_hw.h"
#include "r819xE_phy.h"
#include "r819xE_phyreg.h"
#include "r8190_rtl8256.h"
/*---------------------------Define Local Constant---------------------------*/
//
// Indicate different AP vendor for IOT issue.
//
#ifdef  RTL8190P
static u32 edca_setting_DL[HT_IOT_PEER_MAX] =
{ 0x5e4322, 	0x5e4322, 	0x5e4322,  	0x604322, 	0xa44f, 	0x5e4322};
static u32 edca_setting_UL[HT_IOT_PEER_MAX] =
{ 0x5e4322, 	0xa44f, 	0x5e4322,  	0x604322, 	0x5e4322, 	0x5e4322};
#else
#ifdef RTL8192E
static u32 edca_setting_DL[HT_IOT_PEER_MAX] =
{ 0x5e4322, 	0x5e4322, 	0x5e4322, 	0x604322, 	0xa44f, 	0x5e4322};
static u32 edca_setting_UL[HT_IOT_PEER_MAX] =
{ 0x5e4322, 	0xa44f,		0x5e4322,  	0x604322, 	0x5e4322, 	0x5e4322};
#else
static u32 edca_setting_DL[HT_IOT_PEER_MAX] =
{ 0x5e4322, 	0x5e4322, 	0x5e4322, 	0x604322, 	0xa44f, 	0x5ea44f};
static u32 edca_setting_UL[HT_IOT_PEER_MAX] =
{ 0x5e4322, 	0xa44f, 	0x5e4322, 	0x604322, 	0x5ea44f, 	0x5ea44f};
#endif
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
extern	void dm_initialize_txpower_tracking(struct net_device *dev);

#ifdef RTL8192E
extern  void    dm_gpio_change_rf_callback(struct work_struct *work);
#endif



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

//
//	Description:
//		Prepare SW resource for HW dynamic mechanism.
//
//	Assumption:
//		This function is only invoked at driver intialization once.
//
//
void init_hal_dm(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	// Undecorated Smoothed Signal Strength, it can utilized to dynamic mechanism.
	priv->undecorated_smoothed_pwdb = -1;

	//Initial TX Power Control for near/far range , add by amy 2008/05/15, porting from windows code.
	dm_init_dynamic_txpower(dev);
	init_rate_adaptive(dev);
	//dm_initialize_txpower_tracking(dev);
	dm_dig_init(dev);
	dm_init_edca_turbo(dev);
	dm_init_bandwidth_autoswitch(dev);
	dm_init_fsync(dev);
	dm_init_rxpath_selection(dev);
	dm_init_ctstoself(dev);
#ifdef RTL8192E
	INIT_DELAYED_WORK(&priv->gpio_change_rf_wq,  dm_gpio_change_rf_callback);
#endif

}	// InitHalDm

void deinit_hal_dm(struct net_device *dev)
{

	dm_deInit_fsync(dev);

}


#ifdef USB_RX_AGGREGATION_SUPPORT
void dm_CheckRxAggregation(struct net_device *dev) {
	struct r8192_priv *priv = ieee80211_priv((struct net_device *)dev);
	PRT_HIGH_THROUGHPUT	pHTInfo = priv->ieee80211->pHTInfo;
	static unsigned long	lastTxOkCnt = 0;
	static unsigned long	lastRxOkCnt = 0;
	unsigned long		curTxOkCnt = 0;
	unsigned long		curRxOkCnt = 0;

/*
	if (pHalData->bForcedUsbRxAggr) {
		if (pHalData->ForcedUsbRxAggrInfo == 0) {
			if (pHalData->bCurrentRxAggrEnable) {
				Adapter->HalFunc.HalUsbRxAggrHandler(Adapter, FALSE);
			}
		} else {
			if (!pHalData->bCurrentRxAggrEnable || (pHalData->ForcedUsbRxAggrInfo != pHalData->LastUsbRxAggrInfoSetting)) {
				Adapter->HalFunc.HalUsbRxAggrHandler(Adapter, TRUE);
			}
		}
		return;
	}

*/
	curTxOkCnt = priv->stats.txbytesunicast - lastTxOkCnt;
	curRxOkCnt = priv->stats.rxbytesunicast - lastRxOkCnt;

	if((curTxOkCnt + curRxOkCnt) < 15000000) {
		return;
	}

	if(curTxOkCnt > 4*curRxOkCnt) {
		if (priv->bCurrentRxAggrEnable) {
			write_nic_dword(dev, 0x1a8, 0);
			priv->bCurrentRxAggrEnable = false;
		}
	}else{
		if (!priv->bCurrentRxAggrEnable && !pHTInfo->bCurrentRT2RTAggregation) {
			u32 ulValue;
			ulValue = (pHTInfo->UsbRxFwAggrEn<<24) | (pHTInfo->UsbRxFwAggrPageNum<<16) |
				(pHTInfo->UsbRxFwAggrPacketNum<<8) | (pHTInfo->UsbRxFwAggrTimeout);
			/*
			 * If usb rx firmware aggregation is enabled,
			 * when anyone of three threshold conditions above is reached,
			 * firmware will send aggregated packet to driver.
			 */
			write_nic_dword(dev, 0x1a8, ulValue);
			priv->bCurrentRxAggrEnable = true;
		}
	}

	lastTxOkCnt = priv->stats.txbytesunicast;
	lastRxOkCnt = priv->stats.rxbytesunicast;
}	// dm_CheckEdcaTurbo
#endif



void hal_dm_watchdog(struct net_device *dev)
{
        //struct r8192_priv *priv = ieee80211_priv(dev);

	//static u8 	previous_bssid[6] ={0};

	/*Add by amy 2008/05/15 ,porting from windows code.*/
	dm_check_rate_adaptive(dev);
	dm_dynamic_txpower(dev);
	dm_check_txrateandretrycount(dev);

	dm_check_txpower_tracking(dev);

	dm_ctrl_initgain_byrssi(dev);
	dm_check_edca_turbo(dev);
	dm_bandwidth_autoswitch(dev);

	dm_check_rfctrl_gpio(dev);
	dm_check_rx_path_selection(dev);
	dm_check_fsync(dev);

	// Add by amy 2008-05-15 porting from windows code.
	dm_check_pbc_gpio(dev);
	dm_send_rssi_tofw(dev);
	dm_ctstoself(dev);

#ifdef USB_RX_AGGREGATION_SUPPORT
	dm_CheckRxAggregation(dev);
#endif
}	//HalDmWatchDog


/*
  * Decide Rate Adaptive Set according to distance (signal strength)
  *	01/11/2008	MHC		Modify input arguments and RATR table level.
  *	01/16/2008	MHC		RF_Type is assigned in ReadAdapterInfo(). We must call
  *						the function after making sure RF_Type.
  */
void init_rate_adaptive(struct net_device * dev)
{

	struct r8192_priv *priv = ieee80211_priv(dev);
	prate_adaptive			pra = (prate_adaptive)&priv->rate_adaptive;

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
#ifndef RTL8190P
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
#endif
#define		Pw_Track_Flag				0x11d
#define		Tssi_Mea_Value				0x13c
#define		Tssi_Report_Value1			0x134
#define		Tssi_Report_Value2			0x13e
#define		FW_Busy_Flag				0x13f
static void dm_TXPowerTrackingCallback_TSSI(struct net_device * dev)
	{
	struct r8192_priv *priv = ieee80211_priv(dev);
	bool						bHighpowerstate, viviflag = FALSE;
	DCMD_TXCMD_T			tx_cmd;
	u8					powerlevelOFDM24G;
	int	    				i =0, j = 0, k = 0;
	u8						RF_Type, tmp_report[5]={0, 0, 0, 0, 0};
	u32						Value;
	u8						Pwr_Flag;
	u16					Avg_TSSI_Meas, TSSI_13dBm, Avg_TSSI_Meas_from_driver=0;
#ifdef RTL8192U
	RT_STATUS 				rtStatus = RT_STATUS_SUCCESS;
#endif
//	bool rtStatus = true;
	u32						delta=0;
	RT_TRACE(COMP_POWER_TRACKING,"%s()\n",__FUNCTION__);
//	write_nic_byte(dev, 0x1ba, 0);
	write_nic_byte(dev, Pw_Track_Flag, 0);
	write_nic_byte(dev, FW_Busy_Flag, 0);
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
	if (rtStatus == RT_STATUS_FAILURE)
	{
		RT_TRACE(COMP_POWER_TRACKING, "Set configuration with tx cmd queue fail!\n");
	}
#else
	cmpk_message_handle_tx(dev, (u8*)&tx_cmd, DESC_PACKET_TYPE_INIT, sizeof(DCMD_TXCMD_T));
#endif
	mdelay(1);
	//DbgPrint("hi, vivi, strange\n");
	for(i = 0;i <= 30; i++)
	{
		Pwr_Flag = read_nic_byte(dev, Pw_Track_Flag);

		if (Pwr_Flag == 0)
		{
			mdelay(1);
			continue;
		}

		Avg_TSSI_Meas = read_nic_word(dev, Tssi_Mea_Value);

		if(Avg_TSSI_Meas == 0)
		{
			write_nic_byte(dev, Pw_Track_Flag, 0);
			write_nic_byte(dev, FW_Busy_Flag, 0);
			return;
		}

		for(k = 0;k < 5; k++)
		{
			if(k !=4)
				tmp_report[k] = read_nic_byte(dev, Tssi_Report_Value1+k);
			else
				tmp_report[k] = read_nic_byte(dev, Tssi_Report_Value2);

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
			write_nic_byte(dev, Pw_Track_Flag, 0);
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
			write_nic_byte(dev, Pw_Track_Flag, 0);
			write_nic_byte(dev, FW_Busy_Flag, 0);
			RT_TRACE(COMP_POWER_TRACKING, "tx power track is done\n");
			RT_TRACE(COMP_POWER_TRACKING, "priv->rfa_txpowertrackingindex = %d\n", priv->rfa_txpowertrackingindex);
			RT_TRACE(COMP_POWER_TRACKING, "priv->rfa_txpowertrackingindex_real = %d\n", priv->rfa_txpowertrackingindex_real);
#ifdef RTL8190P
			RT_TRACE(COMP_POWER_TRACKING, "priv->rfc_txpowertrackingindex = %d\n", priv->rfc_txpowertrackingindex);
			RT_TRACE(COMP_POWER_TRACKING, "priv->rfc_txpowertrackingindex_real = %d\n", priv->rfc_txpowertrackingindex_real);
#endif
			RT_TRACE(COMP_POWER_TRACKING, "priv->CCKPresentAttentuation_difference = %d\n", priv->CCKPresentAttentuation_difference);
			RT_TRACE(COMP_POWER_TRACKING, "priv->CCKPresentAttentuation = %d\n", priv->CCKPresentAttentuation);
			return;
		}
		else
		{
			if(Avg_TSSI_Meas_from_driver < TSSI_13dBm - E_FOR_TX_POWER_TRACK)
			{
				if (RF_Type == RF_2T4R)
				{

						if((priv->rfa_txpowertrackingindex > 0) &&(priv->rfc_txpowertrackingindex > 0))
				{
					priv->rfa_txpowertrackingindex--;
					if(priv->rfa_txpowertrackingindex_real > 4)
					{
						priv->rfa_txpowertrackingindex_real--;
						rtl8192_setBBreg(dev, rOFDM0_XATxIQImbalance, bMaskDWord, priv->txbbgain_table[priv->rfa_txpowertrackingindex_real].txbbgain_value);
					}

					priv->rfc_txpowertrackingindex--;
					if(priv->rfc_txpowertrackingindex_real > 4)
					{
						priv->rfc_txpowertrackingindex_real--;
						rtl8192_setBBreg(dev, rOFDM0_XCTxIQImbalance, bMaskDWord, priv->txbbgain_table[priv->rfc_txpowertrackingindex_real].txbbgain_value);
					}
						}
						else
						{
								rtl8192_setBBreg(dev, rOFDM0_XATxIQImbalance, bMaskDWord, priv->txbbgain_table[4].txbbgain_value);
								rtl8192_setBBreg(dev, rOFDM0_XCTxIQImbalance, bMaskDWord, priv->txbbgain_table[4].txbbgain_value);
				}
			}
			else
			{
						if(priv->rfc_txpowertrackingindex > 0)
						{
							priv->rfc_txpowertrackingindex--;
							if(priv->rfc_txpowertrackingindex_real > 4)
							{
								priv->rfc_txpowertrackingindex_real--;
								rtl8192_setBBreg(dev, rOFDM0_XCTxIQImbalance, bMaskDWord, priv->txbbgain_table[priv->rfc_txpowertrackingindex_real].txbbgain_value);
							}
						}
						else
							rtl8192_setBBreg(dev, rOFDM0_XCTxIQImbalance, bMaskDWord, priv->txbbgain_table[4].txbbgain_value);
				}
			}
			else
			{
				if (RF_Type == RF_2T4R)
				{
					if((priv->rfa_txpowertrackingindex < TxBBGainTableLength - 1) &&(priv->rfc_txpowertrackingindex < TxBBGainTableLength - 1))
				{
					priv->rfa_txpowertrackingindex++;
					priv->rfa_txpowertrackingindex_real++;
					rtl8192_setBBreg(dev, rOFDM0_XATxIQImbalance, bMaskDWord, priv->txbbgain_table[priv->rfa_txpowertrackingindex_real].txbbgain_value);
					priv->rfc_txpowertrackingindex++;
					priv->rfc_txpowertrackingindex_real++;
					rtl8192_setBBreg(dev, rOFDM0_XCTxIQImbalance, bMaskDWord, priv->txbbgain_table[priv->rfc_txpowertrackingindex_real].txbbgain_value);
				}
					else
					{
						rtl8192_setBBreg(dev, rOFDM0_XATxIQImbalance, bMaskDWord, priv->txbbgain_table[TxBBGainTableLength - 1].txbbgain_value);
						rtl8192_setBBreg(dev, rOFDM0_XCTxIQImbalance, bMaskDWord, priv->txbbgain_table[TxBBGainTableLength - 1].txbbgain_value);
			}
				}
				else
				{
					if(priv->rfc_txpowertrackingindex < (TxBBGainTableLength - 1))
					{
							priv->rfc_txpowertrackingindex++;
							priv->rfc_txpowertrackingindex_real++;
							rtl8192_setBBreg(dev, rOFDM0_XCTxIQImbalance, bMaskDWord, priv->txbbgain_table[priv->rfc_txpowertrackingindex_real].txbbgain_value);
					}
					else
							rtl8192_setBBreg(dev, rOFDM0_XCTxIQImbalance, bMaskDWord, priv->txbbgain_table[TxBBGainTableLength - 1].txbbgain_value);
				}
			}
			if (RF_Type == RF_2T4R)
			priv->CCKPresentAttentuation_difference
				= priv->rfa_txpowertrackingindex - priv->rfa_txpowertracking_default;
			else
				priv->CCKPresentAttentuation_difference
					= priv->rfc_txpowertrackingindex - priv->rfc_txpowertracking_default;

			if(priv->CurrentChannelBW == HT_CHANNEL_WIDTH_20)
				priv->CCKPresentAttentuation
				= priv->CCKPresentAttentuation_20Mdefault + priv->CCKPresentAttentuation_difference;
			else
				priv->CCKPresentAttentuation
				= priv->CCKPresentAttentuation_40Mdefault + priv->CCKPresentAttentuation_difference;

			if(priv->CCKPresentAttentuation > (CCKTxBBGainTableLength-1))
					priv->CCKPresentAttentuation = CCKTxBBGainTableLength-1;
			if(priv->CCKPresentAttentuation < 0)
					priv->CCKPresentAttentuation = 0;

			if(1)
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
		RT_TRACE(COMP_POWER_TRACKING, "priv->CCKPresentAttentuation_difference = %d\n", priv->CCKPresentAttentuation_difference);
		RT_TRACE(COMP_POWER_TRACKING, "priv->CCKPresentAttentuation = %d\n", priv->CCKPresentAttentuation);

		if (priv->CCKPresentAttentuation_difference <= -12||priv->CCKPresentAttentuation_difference >= 24)
		{
			priv->ieee80211->bdynamic_txpower_enable = TRUE;
			write_nic_byte(dev, Pw_Track_Flag, 0);
			write_nic_byte(dev, FW_Busy_Flag, 0);
			RT_TRACE(COMP_POWER_TRACKING, "tx power track--->limited\n");
			return;
		}


	}
		write_nic_byte(dev, Pw_Track_Flag, 0);
		Avg_TSSI_Meas_from_driver = 0;
		for(k = 0;k < 5; k++)
			tmp_report[k] = 0;
		break;
	}
	write_nic_byte(dev, FW_Busy_Flag, 0);
}
		priv->ieee80211->bdynamic_txpower_enable = TRUE;
		write_nic_byte(dev, Pw_Track_Flag, 0);
}
#ifndef RTL8190P
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

	//record for bandwidth swith
	priv->Record_CCK_20Mindex = tmpCCK20Mindex;
	priv->Record_CCK_40Mindex = tmpCCK40Mindex;
	RT_TRACE(COMP_POWER_TRACKING, "Record_CCK_20Mindex / Record_CCK_40Mindex = %d / %d.\n",
		priv->Record_CCK_20Mindex, priv->Record_CCK_40Mindex);

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
#endif
void dm_txpower_trackingcallback(struct work_struct *work)
{
	struct delayed_work *dwork = container_of(work,struct delayed_work,work);
       struct r8192_priv *priv = container_of(dwork,struct r8192_priv,txpower_tracking_wq);
       struct net_device *dev = priv->ieee80211->dev;

#ifdef RTL8190P
	dm_TXPowerTrackingCallback_TSSI(dev);
#else
	//if(priv->bDcut == TRUE)
	if(priv->IC_Cut >= IC_VersionCut_D)
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
#ifndef RTL8190P
static void dm_InitializeTXPowerTracking_ThermalMeter(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	// Tx Power tracking by Theremal Meter require Firmware R/W 3-wire. This mechanism
	// can be enabled only when Firmware R/W 3-wire is enabled. Otherwise, frequent r/w
	// 3-wire by driver cause RF goes into wrong state.
	if(priv->ieee80211->FwRWRF)
		priv->btxpower_tracking = TRUE;
	else
		priv->btxpower_tracking = FALSE;
	priv->txpower_count       = 0;
	priv->btxpower_trackingInit = FALSE;
}
#endif

void dm_initialize_txpower_tracking(struct net_device *dev)
{
#ifndef RTL8190P
	struct r8192_priv *priv = ieee80211_priv(dev);
#endif
#ifdef RTL8190P
	dm_InitializeTXPowerTracking_TSSI(dev);
#else
	//if(priv->bDcut == TRUE)
	if(priv->IC_Cut >= IC_VersionCut_D)
		dm_InitializeTXPowerTracking_TSSI(dev);
	else
		dm_InitializeTXPowerTracking_ThermalMeter(dev);
#endif
}	// dm_InitializeTXPowerTracking


static void dm_CheckTXPowerTracking_TSSI(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	static u32 tx_power_track_counter = 0;
	RT_TRACE(COMP_POWER_TRACKING,"%s()\n",__FUNCTION__);
	if(read_nic_byte(dev, 0x11e) ==1)
		return;
	if(!priv->btxpower_tracking)
		return;
	tx_power_track_counter++;


	 if(tx_power_track_counter > 90)
	 	{
				queue_delayed_work(priv->priv_wq,&priv->txpower_tracking_wq,0);
		tx_power_track_counter =0;
	 	}

}

#ifndef RTL8190P
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
		rtl8192_phy_SetRFReg(dev, RF90_PATH_A, 0x02, bMask12Bits, 0x4d);
		rtl8192_phy_SetRFReg(dev, RF90_PATH_A, 0x02, bMask12Bits, 0x4f);
		rtl8192_phy_SetRFReg(dev, RF90_PATH_A, 0x02, bMask12Bits, 0x4d);
		rtl8192_phy_SetRFReg(dev, RF90_PATH_A, 0x02, bMask12Bits, 0x4f);
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
#endif

static void dm_check_txpower_tracking(struct net_device *dev)
{
#ifndef RTL8190P
	struct r8192_priv *priv = ieee80211_priv(dev);
	//static u32 tx_power_track_counter = 0;
#endif
#ifdef  RTL8190P
	dm_CheckTXPowerTracking_TSSI(dev);
#else
	//if(priv->bDcut == TRUE)
	if(priv->IC_Cut >= IC_VersionCut_D)
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
		TempVal = 	(u32)(priv->cck_txbbgain_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[0] +
					(priv->cck_txbbgain_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[1]<<8)) ;

		rtl8192_setBBreg(dev, rCCK0_TxFilter1,bMaskHWord, TempVal);
		//Write 0xa24 ~ 0xa27
		TempVal = 0;
		TempVal = 	(u32)(priv->cck_txbbgain_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[2] +
					(priv->cck_txbbgain_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[3]<<8) +
					(priv->cck_txbbgain_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[4]<<16 )+
					(priv->cck_txbbgain_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[5]<<24));
		rtl8192_setBBreg(dev, rCCK0_TxFilter2,bMaskDWord, TempVal);
		//Write 0xa28  0xa29
		TempVal = 0;
		TempVal = 	(u32)(priv->cck_txbbgain_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[6] +
					(priv->cck_txbbgain_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[7]<<8)) ;

		rtl8192_setBBreg(dev, rCCK0_DebugPort,bMaskLWord, TempVal);
	}
	else
	{
		TempVal = 	(u32)(priv->cck_txbbgain_ch14_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[0] +
					(priv->cck_txbbgain_ch14_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[1]<<8)) ;

		rtl8192_setBBreg(dev, rCCK0_TxFilter1,bMaskHWord, TempVal);
		//Write 0xa24 ~ 0xa27
		TempVal = 0;
		TempVal = 	(u32)(priv->cck_txbbgain_ch14_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[2] +
					(priv->cck_txbbgain_ch14_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[3]<<8) +
					(priv->cck_txbbgain_ch14_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[4]<<16 )+
					(priv->cck_txbbgain_ch14_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[5]<<24));
		rtl8192_setBBreg(dev, rCCK0_TxFilter2,bMaskDWord, TempVal);
		//Write 0xa28  0xa29
		TempVal = 0;
		TempVal = 	(u32)(priv->cck_txbbgain_ch14_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[6] +
					(priv->cck_txbbgain_ch14_table[(u8)(priv->CCKPresentAttentuation)].ccktxbb_valuearray[7]<<8)) ;

		rtl8192_setBBreg(dev, rCCK0_DebugPort,bMaskLWord, TempVal);
	}


}
#ifndef RTL8190P
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
#endif


void dm_cck_txpower_adjust(struct net_device *dev, bool binch14)
{	// dm_CCKTxPowerAdjust
#ifndef RTL8190P
	struct r8192_priv *priv = ieee80211_priv(dev);
#endif
#ifdef RTL8190P
	dm_CCKTxPowerAdjust_TSSI(dev, binch14);
#else
	//if(priv->bDcut == TRUE)
	if(priv->IC_Cut >= IC_VersionCut_D)
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
	RT_TRACE(COMP_POWER_TRACKING, "Reset Recovery: CCK Attenuation is %d dB\n",priv->CCKPresentAttentuation);
	dm_cck_txpower_adjust(dev,priv->bcck_in_ch14);

	rtl8192_setBBreg(dev, rOFDM0_XCTxIQImbalance, bMaskDWord, priv->txbbgain_table[priv->rfc_txpowertrackingindex].txbbgain_value);
	RT_TRACE(COMP_POWER_TRACKING, "Reset Recovery: Fill in 0xc90 is %08x\n",priv->txbbgain_table[priv->rfc_txpowertrackingindex].txbbgain_value);
	RT_TRACE(COMP_POWER_TRACKING, "Reset Recovery: Fill in RFC_txPowerTrackingIndex is %x\n",priv->rfc_txpowertrackingindex);
	RT_TRACE(COMP_POWER_TRACKING, "Reset Recovery : RF C I/Q Amplify Gain is %ld\n",priv->txbbgain_table[priv->rfc_txpowertrackingindex].txbb_iq_amplifygain);

}	// dm_TXPowerResetRecovery

void dm_restore_dynamic_mechanism_state(struct net_device *dev)
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


void dm_backup_dynamic_mechanism_state(struct net_device *dev)
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
void dm_change_dynamic_initgain_thresh(struct net_device *dev, u32 dm_type, u32	dm_value)
{
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
			#ifdef RTL8190P
			write_nic_byte(dev, rOFDM0_RxDetector1, 0x40);
			#else
				write_nic_byte(dev, (rOFDM0_XATxAFE+3), 0x00);
				#endif
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
					#ifdef RTL8190P
					write_nic_byte(dev, rOFDM0_RxDetector1, 0x40);
					#else
						write_nic_byte(dev, (rOFDM0_XATxAFE+3), 0x00);
						#endif
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

void dm_init_edca_turbo(struct net_device *dev)
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

//	printk("========>%s():bis_any_nonbepkts is %d\n",__FUNCTION__,priv->bis_any_nonbepkts);
	// Check the status for current condition.
	if(!priv->ieee80211->bis_any_nonbepkts)
	{
		curTxOkCnt = priv->stats.txbytesunicast - lastTxOkCnt;
		curRxOkCnt = priv->stats.rxbytesunicast - lastRxOkCnt;
		// For RT-AP, we needs to turn it on when Rx>Tx
		if(curRxOkCnt > 4*curTxOkCnt)
		{
			//printk("%s():curRxOkCnt > 4*curTxOkCnt\n");
			if(!priv->bis_cur_rdlstate || !priv->bcurrent_turbo_EDCA)
			{
				write_nic_dword(dev, EDCAPARA_BE, edca_setting_DL[pHTInfo->IOTPeer]);
				priv->bis_cur_rdlstate = true;
			}
		}
		else
		{

			//printk("%s():curRxOkCnt < 4*curTxOkCnt\n");
			if(priv->bis_cur_rdlstate || !priv->bcurrent_turbo_EDCA)
			{
				write_nic_dword(dev, EDCAPARA_BE, edca_setting_UL[pHTInfo->IOTPeer]);
				priv->bis_cur_rdlstate = false;
			}

		}

		priv->bcurrent_turbo_EDCA = true;
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
				printk("===>u4bAcParam:%x, ", u4bAcParam);
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
#ifdef RTL8192E
	struct r8192_priv *priv = ieee80211_priv(dev);
#endif

	// Walk around for DTM test, we will not enable HW - radio on/off because r/w
	// page 1 register before Lextra bus is enabled cause system fails when resuming
	// from S4. 20080218, Emily
e:
	rStop to execute workis reto prevent S3/S4 bug.
#ifdef RTL8190P
	return;
#endif   	Who				Wha2U	-------------------------- ---E
		queue_delayed_Majo(priv->    _wq,&      gpio_change_rfcrea0)---------
}	/* dm_CheckRfCtrlGPIO */
--------/*- "r8192E_dm.h"
#include"r8192E_dhwh"
#include "r8192xE_phyh"
#include "r8192
 * Function:	dm_cincl_pbc_ion ()
 *_phyOverview:	#incl if PBC buttonuctopressed.56h"
#/Input:		NONEConstan*ut-----------------R--------*/
/fere Indivised History:
 *	When-- -o		Remarkssue05/28/c

A	amy 	Create Vers_rtl0 porting8192EUwindows code Consta"r8192E_dm.h"
#include "r8192E_hw.h"
#include "r819xE_phy.h"
#include "r819*/
static	voidnclude "r81920_rtl8struct net_device *dev)
{-----------
	20---0xa44f,r
	20_ vers*lse
#= ieeeAbst1efdef( 	0x;
	u8 tmp1byte;


	tEER__DL = read_nic_EER_u32 ,GPIedcaif(OT_PEER_MA= 0xff)----------22, 32 e5e4322&BIT6 ||eOT_PEER_T_IO0)
	{
	dynaHere we only set bPbcP LocalortiTRU08-0// Afht-ctrigger2, 	, the variCorp will be =
{mechFALS		0xRT_TRACE(COMP_IO, 
#inclPbc8192E322, 	fine322, 	\n"4322	e versb5e435e4322, = true;
	} 0x5e4322,
*/
//
// In
	c

Ade "r8192E_hm.h"
#include "r8192E_hw.h"
#include "r819xE_phy.h"
#include "r819xE_phy.regh"
#inclu8192CT_PEERF--------------PCI4322};not supx5e4
    r Chaca322}ack HW radio on-off control 	0x5e4n-------*/
//
// Incal Constafferent AP vecttindiffertoryAP vendor for IOT i190P.eren  	Who					What
	2/21ic u32MHC	a_su32 edca_[HT_I0x5e4322};,------*/
// 32 e60*/
// Deb5e4320x5e4322};};T_IOT_Ple-- 4322---------U----322,ant-[HT_IOT_PEER_ fTK_D_EDCdm_digta    _m_digta*     ?
dx5e4322}mye verslse
#idPath =cal Cainer_of(    ,For Dynamic Rx lect,{0}};;
 ble;
/e;
// Store#atic   	Who			 by Signal Stelectth
DRxPa----------,tent for MAC PHrolTCorp;ndif

#def Debug variable =x Path 	Wha2E
st->devftware write---- shoRF_POWER_STATE	eRfPowerStateToSet;
	bool bActuallySet = fals----		al4322,Corp-on by /


/if(! Path up)
	e regshoftwareu32 edcNIT | OT_PE/ndif_rtlprotRF),"----tory----MAC PHY.
u8		dm): C	void	i f44f, 	0 breaks out!!0x5e4322}
		atic*/


/*e4320x108 8192Ei/


opyrig2,  isX] =
_MAX]_hal_d#els(0xa44B1= 1: RF-ON; 04f, 	0FF. hal0x5e4322, xtern{ ----*/
// Debug va hal-Define Local CoDe =e;
// Store cu1) ?  eRfOn :_tracffa44f, if(Rx Path bHwR322}Oftable/
//) && (	0x5e4322,  	0x;
ext=gcallkwind-it halshoftware writRF, "_rtlOT_PEERF  -A 0xt *wo ON0x5e43, 	0/
// Deb44f,*Majo);ea44f};
sc_me---------------/
// DernEER_id dtern	t
Majo_0xa44f,te(struc
einit_backudincluck_txp----_adjusttchdff4f, 	0x5e4322,  	0,e loc binch14r_adjue_backu *devre IOTFF0x5e4322me porism__IOTetchdogt *dev,
	btxpower_adjuvice *dev,
	bac2		dm_v------------f, 	0x5e43chanism_stortiAf, 	0x= 15e4322MgntActSe----if

#d// Degcaldevice *dev);
e, RF_CHANGE_BY_HWincltyp//DrvIF--------Cur----Phyern	us(pAdapM_ChthSe_EER_M deint_devimsleep(200 winncSu32 e}ode.

ant-4f---------
2Eh"
#include "r8192E_hg_UL[HT_IOT_PEER_MAX] =
{ 0x5e4322, 	0xa44f, 	0x5e4322, 	0x604322, 	0x5ea44f, 	0x5DM_RFlect#inclWorkItemhal_Bd	ini---------Define Local Cons----			 RF RX pathefinr  	0x.---------------Define Local Constant---------------------------*/


/*------------------------Define global 1/30
extern	-----Define Local Constant---- Debug--------- ?
dig_tincldigt-------/ Staltec						 shoftwa_PEEriteopyright-cconrf_er_cde "r8#define n	void	in_shadow[16][256] = {thSelv);
h
DRxPathSel	DM_RxP Sariab_rtline global vrengle------thSel	eck_--------*-----------------------------DefDe 0x5eglob *dev);
extern	pe-----------rfge_rude "r8
ct *work);
extern	voi--Define local	0x6ion prototype------//e locaa_check__nit_4f, 	0x5e4u84322, 	 = 0, i-----/*.c u3/x5e43 MH variabdiscuss322,wi-DefD3 Jerry, 0xc04/0xd04ce *dev);
322}
322,alwaystoree432same. W	void	iM_Ch, 	0x5 now.net_	-> Bandwi	init_rate_adaptiv 	0x5e					//4322, 	Bit 0-3, it means/ IndF A-Dallback(uns.
	----(idwid; i < RF90_PATHvoid; i++ite reg				;

// D& (0x01<<i4f, 	0chanism--------xack(un[i]Type,
m(st2		fotxpower_a


//_IOT_PEER_Mrk_s0	dm_d *woreck_xajor----k(unsEck(un22, 	0dm_digta 0 pxive
ssel_byrssit shoftt_edc DM Majortemcallback2		dm_v wonet_d_I0x5e m_gpio_c	forAX] =T_IOT_4f, 	0dm_digtable;
// St,----e ?
dDM -i;ble;
// Store-------------Deriable---urrent shoftw n_ba;

/B 	for gaiModuldm_tx	//defaultruct unsithSelIn byGRSSIby RSS_TH_low locInit Gaiackup
_er_a
// DPathSelnet_device *de----_THet_device ctrl_	forpower_a/
// D Path CustomerID	dm_RT_CID_2, 	_Netcor IOTemic Init Gain by R);
emeth};
//CCK_Rx5e432----2;
n	voresetatic	void	dm_ctrl RSSc	void	_by_driveroid	(	stxpomic Init Gain by RDbgMod// DDM_DBG_OFFsi net_dev 	0x5e4322,  sk(unsRF22,  	0for(i=very<4#incextern	vmic Init Gain by Rrf_devievice 5  	0_cck_txpower_a_IOT_PEERpwdb				evice -64tic	void	dm_cs_rdev,
		f_ack(unc	voidthevice t0power_}ecovery(txpoc Ininit gain b

staticev);

static	vbb_ctrlial RSS(struct net_device *dev);
#endif

// DM --> Dyna_sadjui, maxit_ctsindex=0, min--------id	dm_csecid	dm_check_rfcrf_num=  	0v);
//tmp_ync_---- EDC#ifndstatic	hat
	trucrl_ion _txpower_a
 RSSvice di_Rx=0x2
staRF-Cf
static	voioph"
#al,  	0x3;tructD
	longruct
//sc	voiaxratioid	dm_gpc	voiinf, 	0x5e4322,  	0io_c	void	r// DM --> Chrx_ver2xa44fcheck_rfc 			u3id	dm_cince *dev)dev);rx_er_cnisml_check_rX RF path ur portati;struct pbcd dmcs_t netkup(sice *v);
/c	vo_gaixternntstruct nRx_ajor	dm_ctrlized
// DM 
staupdatentchdrxrtingigtabldm_cthirfnet_e !=_tx_2T4Raticc_gpiWho		t_hal_devictic	void	2		dm_v*dev);

static	vpd_thtchd(struct
// DM (InitTX tatic	d	dm_chea07)&0xf_back(struct net_device *devdm_txpou32 dev);

static	vatic	votati;


/uxfe *dev);

static	v(struct net_de&=~ 	0x5e4322,  	0x;e *dAdtruc					ync---*/bn prototypem	DM_TypWIRELESS_MODE_Bvoid	dm_c 			u32m_denet__fsyoid	dm_ctrl_initgain_byrstr	//pure B 
// , fixp. Ack vrssi(	2x5e43DbgPrint("P  rotode *dJl ric

Arx8/02/18
s	0x5e432u32 taticideW RF/sec/	voitati check5e4322, e4322, net_device truct xtern	vot*dev);
//m_pd_th(stru44f, 	0	voiic void dm_deInit_44f, 	0x5e432 Path x5e4s.rf RTL8rpercentagtatice * *wo_recovery(struct net_devitern	void0x5e43++ce *de Pow	forcr0x5e43d RSSI to firmware
staticct net_de0x5e43ive
1)dyna8ind firsigv);
/(s rfmge_raanddth_aice *values	void	//device *defctrt K_DLicDM --> Fs_path_autos onet_dtsync_d	dm_check =ce *dev);


//  =d	dm_ion tchdog);
#5e4322dm_init_fTL8m_vatruct net_d=======io_changine *dev);
/*-xpacSetkup_dPathS-------otyp2c	voidPEERwe pick up=====max=======ive
s,=====letd	dm=====Pathtodev)================
uct nction ce *>=======/	HW Dymust // DM tion is void init_hal_dm(struct

=
//	HWRxPathSelmecvoid in//
//ct net_dt oninitrent /
void init_hal_dm(struct hal_dmirmware
stat0211_priv(dev);

,
				 interface.mware
8192E_//
//	Desce *dev,
	AX] invoked at ditgaized(stru netnv = ieee80211_    (ction p	//	HW Dydfunction prototyize_tcan/	HW Dynamic ic	voidferenice *	force_v Smoothed Signa5e4322,  	0x60
----ic mechani     crip invoked at -nitga-1;

	//Initial PEERfere	Assump.h"
#poinfdefse432  	0emtrolern	v(s ----- 	0x---------ted Smoothswit, add by amy 2008/05/e_atypeive for nevoid----------ize<xpower_trackin &&d_sch(dev)_atio = -io_changemust levicinit_bandwidth_autoswitchxpath_sedev);
/*rol cq,  dm_gpio_chan_fsync(sariaion(dev);
	dDM --> E
	INIT_DLAYED_WORK(&able;
andwidth_aur near// Undec		dm_vadm_gpiouct -----,  dm_gpio_chan
// DturboRX_AGGREGATION&      ion 0 porting crea  Powion 0_change_rf_callback);
#etive(deev);
*ev);
 = ieeePEERThis case4322don'--------a4void ny  dm_gpio_chive(dendif

}	// IniitiaRX_AGGREGATION__hal_dm(s
statv);
#ifdef RTL81921nge_rf_crmware
static	void	dm_init)path_sePRT_HIGH_THROUGHPUT	pHTInfot_rxpath_selectHalDm

void deinitrnge_rf_callbaxpower(dev);
	init_r

#ifdr Control f_callba
}
bc_gpWho	USB_RX_AGGREGATION_SUPPORTcode.
-*/
#inclxAggregat_fsync(oraoswitch(dev);
	dm_	n utitructmechDescri
{
	v RTL819Abstontrol irmware
static	vo0;
	unsigned long		curTxOkCnt = 0;
	un =       
			}
		}->pTInfo;
	static uns_if (pHalData->bForcL[HTit_fsync(pter->HalFunc.HalUsbRAggrHandler(Adapter, FALe,
				.
}nctiontion:
// invo// ev);

static	vDescrc

A net  dm_gpiif(evice *dev);
static	void	dm_ctr/Far Range ------ite regxa44f, 	0x5e4322,  	0xice *dev);
//ing)atic	void	dm_cs_ric	void	stoselftting))ic	void	dm_inpHTInfte_adap8192		if (priv->bCurre(struct ting(str				meitialon:
//		einit_haine Local Constant----/
void init_hal_dm(struct 
		}
	}else{
		if (!priv->bCurrentRxAggrEnable && !pHTI
hck_rx_path_se000) {
	portt net_devic1->pHTInfnitia) {
			u32
	if((curype,
,stati===========must or nearif((curTxOkCfo->UsbRxFwef RTL81porting4) | (pHTIn	t(stgned heck		g.h"
#_dig	Preparre SWodulourceh_autHWpter, TRUE);
			}
		rent g_init(dev);o->Ust_devfreg.h"
uctooated_smoRxOkCnt) initga 24) | (pHTInorcedUsbgrEnable) {24) |Aggr;
	un-um<<16) |
ablePackNum<b							RT2RTable) {tiRxFwAggrEh_se			voi} aticValue if<<16) |c_dw	rmware  send aggregatgeNumll send a
			 */ed packetr(Adarimware wi<<16	ulValue = ( ulV====ic_d			write_nic_dive(dev);
1_privRTL819one of threeChecket to drxAitias aboveuctoreachedxpow	 * 			      ket to drxAFwAggrPa}

	lastThree tForcedUsb
// s.txg_DLsunicas send RSS	}

	lasog(strucfirmtern  4322rue;
		}
	}

	lastTxOkCnt = pver.g(stru/g(st void{ 0x5dword(fo != pHthree thr{sholdHW Ddi



void hal_dmRxAggrInfo == 0) {
			if (pHalData->bCurrentRxAggrEnable) {e);
			priv->bCll send aggregatedruct r8192_priv *priv = ier8192U
{ 0x5e4322,.ing fn(struct nateev);
/truct 

code.fRX_AGGR#incEdcaT
voi(strucef RTL81Num<<8)dm_watchdog
static	void	dm_iking for near*dev);
static	void	dm_cRX_AGGREGATtruct UPPORT
void dm;
	dm_bandwiddm_init_rxp15 portinct net_devicel4322,  mware
static	vice *dev)
{ggrEnabrue;_rs==========-05-15 porting frtct neandretrycoun_RX_AGGRev);->pHTInfo;
	statictruct ata->ForcedUs	/// Add by amy 2008-05-15 porting fcast;
}		}
		} elsedm_watchdtthree tdwiddca_ketNum<<8) | (purt r8192_pr inputobalsprotSUPPORT
void dm_CheckRb/ DM This funcwi


/*
  *gstruct reck_rfctrl_gpio(devffo == 0)X_AGGREsi_tofw( (!pHalData->bCurrentRxAggrEnable || (pHalData->ForcedUturbo(deve.
	dm_check_pbc_gpio(denfo == 0) {
			er makinDog


/*
  eturn;
	 for neaggrInfo == 0) {
			if (pHalData->b* dexAggrEnable) {HTInfo;
	ststruct }	//rInfmWh_seDef USB_Rle604322, 	0word //struo->bCurrentRT2;/strucHC		Modifypr  *	01/1gth)
  *	01/11/c

A	----	Modifd_smput arguments and RATR ce *d level.resh_for_6a20M = RateRF_ype is assigned in Readtytesunicast;
}	}
		} e);

#ifdef USB_Rlow_rssi_thresh_for_ra ct r8192_prieTH_High;
	pra->low2 *priv =c_dword(d
		redapteretrl_i Rx
//==
nd Rreg0xA07[3:2]=c

A_devicegrh_for_,M = Raa07[1:0ivfor_rfacPBC
daptif(p.

	if((curTxOkCnt + }

ing t - ow_rssi_thr;ra40M;
	pra->low2 *priv = iree80211_priv( 	void	dm_initi;
			}
		} else te Adaptivst> 
#incID_81stalue =
	last =irmware wi>egatDog


/*
  firmware ! 	0x5X_AGG
 netao !=ngafter enakRxAggre4322,  
		}
	}el<0x1a8, 0);
			priv-	void	dm_c&& ass_IOT_PEER_M  <sbRxF->-----------
		}
	}e- (pHalData->Foresh0xa44f, 	0x5e4322,  	0x;
t +ef RRx//rter-amic mechan2		D=====h		proldust] ={0}RSSImechvious_bssdev);
/*	pratee *dev);


// * de============+5egate//_turbo(_SUPPBBes
	if(pr OFDMte_adalnet_dsetBBreg------rr_200_Toid	dmSSI
st_che1<<e *dev);


// _che win*/:
//		[3:0]M	=tial8f0ff001threModify 2T RA1indwirv->rf_0_thresd;//cosa addh_autossth_foid dd----        turbo(dev)Modier(dev000)= 1;
	else
		pra->ping_rssi_enable = 0;
	pra->ping1ratr		= 			pr	if(pra = 15;15,-------*/


heckRFatic ng_UL[it_f0
//===========egate--------;

	//InitialMH Adaptiv2T->low2high_rf
	pra->tnly invokede *dev);
/--Define of Tx Power Control tting(st ntRxOkCnt = 0;
d<<2)|a->loT4R)
	{
		/_backrf_type == RF_1T2R)
	{
CCK0_AFESet432200f00f0* dev,, uliddle_rsice *dev)
{tting(shreshold= 1;
	else
		pra->pinruct net_drssi_th- lrssi_threshold_str		= 	OkC t-----------
 *E_phOut------>>) | _thr Bandturbo(d rf->uapter->Hal
		}
	}else{
1T2R)
middleafter mhrce *dev);
/*	prate_aum<<8) | High;
ack(un *
 *--hresho_r--------*---------RF-%d
static	voide432hresholict net_>rf_type == RF_1T2R)
	{
		prs->upper_rssi_thresholi_thre00;
		pre->middle_rs->roadcom ==_tx_1T205;
		prModifuppt ne------------_rssi_thresh_fdows code.
 ;

	if((curTde.*/
		pra-----------------// DM -->m_fsyi_threshold_ra-- TRU_nic_dword(dpra-}shoftware write regiv);


rn	void	init_rate_adaptivetic	void	dm_bb_inle ?
dig_t	dm_5edigtable;
/a4de "r8dev);
/ice rl_inisanism_state(strucex====a
#define Rngede "r#incbo(denXRFCurrentRxARx ajor state=0;
e *dev)I----------------Define Local Constant---------------------------*/


/*------------------------Define global 
// DM --> Dyx1a8, 0);
			priv-extern	void	init_rate_adaptive(stru// Debug variable ?
dig_t	dm_digtable;
// Store current shoftware write regi 0, HighRSSIThreshFor2T RATR 			u3=0;

tatic	void	dm_bb_iniialgaiorting from windows a->middle_rssi_threshold_ *de5-142 edc\t");

		//
		// Chefromte verer_trConstant---heck_t_edca_t#incluxin_resnet_d((curTxOkCnt + ct Gaitive
 nfo 			u3_THRIEEE}
		} LINKED ng_UL[//	2 edca_settingRATE, "adaptive;ateAdae)
		pran prototypeHz &&_time);

erva(dev);8190rtGI20MHzosa anfo = prirholdbitmape4322
#inc8d>uppe =
			pri = priv->ieee80	prat------- = 30;----------- --t
	-r =
				(pra->uppb = prit GI is_=====);#er_res-------1)) | ((bshort_gs code.
 *--------------)) | ((bshort_gi_enabledmultipleiv->i-----------3(~BIT31)) | ((bshriv->ieeea->low 	0x
 *-T31:0) ;
E(COMP_Rr =
				(pra->upper_rssseve(d----------1)) | ((bs2 RateAdaieee80211->pHT1)) | _31:0)_de5e4322,Fa->ui_enabledframea->uMonitpter= 	0x080515 Powe
#ifdef0xc38 m1)) | (on	Whe Gaiv->ie(ether S			pra->log(stieee8021r Corp.)? .data----ut argume#incct ne--tr =
		32				d_ratware
stat=ant--r =
				(prn	void	i;		Ltesunie *dev);
//deIW4			( &&pHTInfo->bCurShortGI40MHz) ||
			(!pHTInfo->bCurTxBW40MHz && pHTInfo->bCurSpdel (~BIT3dapt	hort_gR tab
		//coBIT}
dm_gpio_c1)) &me to l1)* fir(x000f0000fo =  r		=Hz) ||
			(!ptive
static	voidopHTInfo->bCurShortGI)r		=net_d->middle_rssi_threshold_struct  *dev);
ge two-------------------
// D32 
 *-- 	0x000
 *--);
#hresggrPara20ing m_ctrain ba lo		bS_rf_wFromCing Dimware
static	abled)?DoubleTimeI------------*/


/*---enabled)? BIT31:0)void deurShorif

//Hz) ||
&&e80211->m(~BIT31)) | ((bshort_gi_elow_rne Local Constant-odify forIOT2	----T& DE_N_5GACT_CDD_FSYNCitialreaeviceing TForR 54, MCS [7], [12, 13, 14, 15le =----ong		hresho			pE_phong		)
		{lData->	H Mole = <= 27----OMP_nelB
 *	When	Re------------ce * <<_CHANNEL_WI0) ;
		}) | ((bshort_gi_enabled)?----------& shorwThreshr*	Whedm_fs(p 	= p+ice *devX_AGGREeceiv		(pholdh----gram[1][ 	= (priv-]er_rsssi_thf	orRA 	= pT<ce *devhieeetype =X_AGGhForAntChort_ghe4322,((bshor id d	= (priv-+							ANNEL_WIW !tern	vo *prifw_iNEL_WIDTH_20)?ow_rssi_thr->low_rssi40M):hort_gloCHANNtChannelra20M)>sh_for_ra40rssiic_dwR:(pra= HT{Low_2----g(stNh_for(priv------CHANN,->low.Lo_enabled)? BIm_ctrRF_1T2Rbandwiune  	= pr1)) | RAnic_di>uct m_check_rate_ad ;
		}
40Mwer threshold. Ifbshion after higdecoratg(st->under(dev)bytesunicedPWD---Dt neta->
		}
senablepriv->undecoratepriv- overing_rssi_) < H;
			target>low2=>=bRxFwAee4322,hresh_fow_rssi;
			Lowi_threadju			LowRriv->undecorated_WIDTH0;
	u32				u32		force_v_dmr, FAng_r_threshold
			p					void	dm_i[DM]	= 	0=%d STA-----//Ifv->undemootHi= RateARndecoratThresh_for_r=%d/%d\n\r"d_smootHi_state = DM_RATR=ng		ratrd)? BIT31:0) ;
		} | ((H_20)?
		>= (heck_);
		}
		essi_thresholdH;
			targetRATR fo->bC eck_ATR_SrgetRATRra40M):(praH;
		 //s
			ee80211->m version 0 v);
#ifdeow_rssi_threng)rRA 	=shoftware writHALDM, "Chanch(dev)%dw_rss->unde%decorat->unda20M)%d ndecorat;

	 %gi_e,annelBW verM):(praecorateEL_WI< (long)(praDTH_2 			//cosbs-----r/l/*
	back//ng_rwtRxOold i_threshthose mcsecorat
		}def RTL30 %_SUPn )priv	 w_rsurn:		NONE
 undype 			//cosa add_rfctrl_decoratedth AP. We will not catr =
		
&&8, ulanne)LowRSSIThreInfoSettUntChannelBWsh_for_ra40;
			targetRATR5sh_fordPWDB) {
		if (pra->ratr_statnt("[DM] THresra->ratr_statighR //s----iddle_rssi_thre from wit_rate_adaptiv0xC3600f00vice *d1:0) ;
		R_ST RF_132				_thratic n the1n",  (	t RATR ttRSSI is between the r fdef_thre9vice *dev_STA_LOW;
	atrs betw	y 2T RATR _RATR_STis between the restRSn the4		void	dm_iT

		/SIuctobetween0x5e4rsi_thr.c

5ic_dword(id de>ratr_stat			pra->ratr1
		// FoRec6old to 0x%leratr;
	//d);
				ping_rssi_sh_foredPWDB);ratrdPWDB			pra->ratr_state set RATR to 0ratr		= 	}else if(1i_thredev)
{
	structink wtr_state = DM_RATR44f, 	0x5e432ATRH;
			;
				ping_rssi_sta inp;
	u32		rwisekey 8.04.01		pr 1	pra->rRxO;
#endX,----pairwisekey = wep/tkip,EER_supT_PE_MAX] MCS0~7 ,po", pHaUsbRxAggrInfo GetHal	   thedPWDB, pRA->Tes)ping_rsse to lposelngtoswiAP. Wssid[6]notX_AGGR per_211-link f_type == RF_1T2R)
	 cire->Un/ratr & (~BITsexpires = jiffies + MSECS] THresh H/Lw2higChanni_v->ieee80211-grInfck_rate_adapsh_for_ra40M):(prawRSS_fw_iNSSH;
		= 	Print("TestRSSI = %d, set RA = %xfNmoorateH;
			RATR = %xRT_HIGH_THROUGHPUT	pH)
			tar
		}
===== &= ~(0MHz_ALL_r_20_2se
	{
p/tkip,om windows code.in_t->low0, andwidth_aH;
			om windowsg_DL2_privUFWP, 1);e suTInfo = p = DM_RATR_STA_LOW;
		TAdm_ch}44f, 	0x_MODE_NLet R *dev);
t GI is en_thresho
		}
 H/L=%dM] RSSI=%d Sf RATR0 is in_tPWDB);
	and Rto dr RSSI=] =
{ 0x5ATR);
				ping_rssiif( targetRATR !=  currentRATR )
		{7.
		if(pr_value;
			ratr_value = targetRATR;ip, we sup(COMP_RATE,"currentRATR = %x, targetv->undecorpra->ratr_state = DM_RATATR);
				ping_rssstate = 0;ariadhreshold_40MhRxDetellba2 \n")64052cdse
	7.
		if(_rf_wq   to prevent jumpi 	init_rate_adapti465cxbytesunicRATR t}dle_rsRATR tabbleothedPWbgPrint("[DM] RSSI		}

			//cosH;
			targetRATR se
	 2T RATR tabieee80211-}
		grInfoSettdapter->Haltch(dev)_HIGstat9m_CheckRateAda;
		}

			//cosa addf RTL8<ecoratehort_gy 2T RATR  (long)LowRSS+= pra->ping_rsf(_rssi_threshold_//StartHWalse)pHTInfo->bCurShortGI40MHz) |20 ||!R = %x, targetRA%ser->H__FUNC (pHa_se
	

	if (priv->rf_typeRATR = %xsh_for_ra40M):(pr=12cvivi,r_value;
			ratr_valuec3bEL_WI1<orcedUsbRxAggrInfo bEndS=======_e_rf_w.----------40Mhzto20;
			(!odify forket TxB1:0) ;

	 witch.threshoShoMhzdev))
>undecorated_smootassigned in R priv		ratr_value &= = priv-tatic void dm			(pra_LOW	//DbgPrbandwidth_auto_switch.bforc >= priv->ieee80211->bandf RT		}
			wrUTO_SWITCH_HIGH_LOW;
M default at 0db, index=6.
#ib >= priv->ibfg)) d_tx2andw-------e;
r_value;
			ratr_value = targetRAT0x5e4322e_rf_w{
		if(65400195,	ATR_Siv->rate_dPWD_40M+5;

	prach
_adaptivof Tx
//sddle_rssi_threch.bforced_tx20Mhz = true;
		}else{//in force s	}

	}

	static u8dwidth_auto_switch.bf RTL8>v->undecorated_smoottch.bforced_tx20Mh->ieee80211->b
	0xtobandw0211->bandwiecorav->undeIong)Low0x2_thre80,Bv->unde00b5,	// 9, -3db
	0x28// 9, -3db
	0x288000a2,	// atr  = pxhFortype ==0o zero, s DM 0051type =.b
sta400witch.threslow_ratcur// hold-_auto_swidPWDB(priv-051	voi 16, -10d0048,1db
	048	voipra->ratr_state = DM_RAT1)) | ow_rssi_t			(prr		= 	06t ne35, 0*----[t net*----_Lssi_t]ion by Local Constant--ct *work);
extern	voiN_24Gcastr_20Swi | (->iee_gi_enabled)? BIT31:0) ;
		} | ((bs632BW_Anabled)? BIT31:0) ;
		}
, pHalData->Undecorated0)?ed
		/shold_W_AUTO_SWIx03}SwingTble_h1_C{0x2bt ne2ax21,  0x0x1ex18, 6, AP. W 0x07x18,,	// 3, 2, -2	{0x22,  0x09 0x18,21x18, b,  AP. W}c	voidx1c8wingT	}else{//i02// 3
		}

	}
	ewingT DM --> Fx1c800072,e_nic_byte(dtwingT2, M d	dm_di0x22,30x1a, ieee80211->pow_rssi20M);SSI 72,e4322,  	0x 17, -11db
	v->undecorateL_WIDTH_2 0x02},	//(long)LowRSSIThingT// Fgi_enaCheckRateAdaptive


static void dm_initandwidth_autoswitch(struct net_devipriv->CurrentChannelts.rxbytesunicast;
}	
	if (priv->rf_type0x2d4000b5,	// 9, -3db
	_auto_switch.threshold_20Mhzto4===> CCK, riv->h_aut 0x18021emprature, 				ih_autaticwingTable_
	0139_thre4x02},73db
	{0x0x32ck_rcthed	0x288000a2,	// 10, -4db
	bandwidthee80211->bandwidth_afe,	// 0, +6db
	0x71c001c7,	// 1, +5db
	0x654ssi_thres211_{//inh_auce sxbytesunets in 20 	0x600, 0/40_CheckRa9/ 11MP_RA};

stde "r8o 0x	.

-2007/10/081;
	W_TRACE(C#de loc	RegC38_// 1, -uiresi_t// 3, 3, -3dbNonalse)_Other_AP	11, 0x1d, 0x11, 0x02},0AP_BCM		2rt RA smooth scheme now. When it is the first
		  //-6db
	0---------C35e43er(dev);
	ireg_c38DM_Val=0x11, 00x22, 2	{0x1fx02},	2	 *price *de0x04,0x117h1_Ch139x09,66 "AC Px20M;
		}else{//i%d MNNEL_WI	{0x1f// 3, 7, }

			//cos0x6000f, 0xrequently.}
			wra->l
	{0x1f{0x13, 0x13, 0x10v->ieee80211-
	{0x13, 0x13, 0x10, 0x0adecorateow_rssi_e80211_
	x18, 0x18,0c{0x13, 0x130x009	{0x1fx%x Faulted_smern	vTR to 0xM_RA, pHa0f 0x000x00, d 0x0080x00	{0x13, 0x13, 0x10, 0x0a0);
		}
		e1, -11db
};
#endif
#defin, 0x2a, 0x25, 0x1e, 0x11, -11db
};
#endif
#defin", 0211->bandwidth_auto_s
		}

	}
	el_for_ra20M);
ed
		/
			write	{0x18, 0x17, 0x 0x11,32				211_priv(deo_switch.threshhold_20Mhzto40LOWothedPWDBHigDM] THresh H/Le_nic_ ((bshort_gi_en=  2T => CCKsh_forue);
			write_nic_byte(ddm_TXl_dm_wq, 0xata->// 1, -1db
	{: BW_AUT, -3db
	0x28800 shoftw, 0x		}
		 set RATR to 0id dm_TXPo
HWW_Busy_Fld	DM_e432nt > 4*viewS inpu8		tx_cmd fots in 40 >=si_tr_2024G5]={0, , -1/*
	ssi_talue;
	u8	intrssi_ng_rs aticjdwidthkle level				PweAdadm_txetti_reporTSSI_Meas0, 0, 
#ifdeU
	RT_Sdriver=0;
ATR = %x	}

trol for nene lo= BW_Abra20======		u3,  pacflag =write_;
	DCMD_TX_TRACv->u, 0, 0, Meas_
 = (= BW_A	lastRxOAvg_TSSI_Meas, TSSI_13dBm, Avg_t[5]={0,s_M	= _initgae802  	Who				Wha2U
Infoidth0};Pwr_Flag;
	u16					ACKING,"%s()\n",dwidth_auto_11->bv(dev);

	priv->ieee85, 00x1ddif
(dev);

	priv->ieee8Pw_a, 0);
	;
	wr,----USeas, rt----us =[DM] RSSI=%x\n"RSSI=%d STA=LOW\n\r", H_20)?
		visx13, 0x13,},	 ap
s -4-6db  =10x00,1eothed_ SWrRxObroadcom APe){/statePORT
void d0f, 0x0d, 0x0b,		RATR);
				ping_rssi_, 0x00, 0x13, 0x10, 0f(priv->CurrentCh3t r815u16				7.
		if(pr_Flag;
	u16					
}	//net_3dBm,; j<=pra-j++)9{	//f4322t0x508000x15,v->rf_type; 	0xue = (RF_Type<<8) PrintM code\0x00}	 0x0903dif
//	bool rtStatus = true;
	u32						TA sta Pw_Track_Flag, 0)Sx_cmd.Value	,;
	u16				Avg_TSSI_Meas, TSSI_13dBm, Avg_// 1, -1db
	{0x2bowerlevel-Defe_adaptxPathSeTxHig	writ%s()\n",__FUNCTIONedca_sy_Fl "le--configut netn&= ~(Rtx cmd 5-14	eserv! r81si_ra#el_TRACE(COMP_POWER_ghPower;

	podriver=0;dPWDB);
R = %xPwr_Tstru>>2 netom_driveSI=%alue2			0x13e
#define		FW_Busy_Flag				0x1flag = FALSeSACE(COByAPsHandlUPPORT
d dm_undecorastRxggrI)) | etting)) dUsteAdaptive

;
FUNCTIMH MlevelOFDM24G);

	ion after makin	u16					 = (u8)(ld_rr_Flag;
	u16					MD_SET_TX_PWR_TRACKING;
	tx
vice *dength, 0, T_IOOWER_TR.Op		= CE(COMSET_TX_PWRtx_cmd, D;
OWER->ieee8FW_Bu	= R_TRtx_;
			r	lasturn;	lastRx#iflevelOFDM24G);

	s_from_			u0d_ratr80211_pri (adcom,
->HardternTG;

0x39ARDWARE_TYPEprivsi_th) {
			i	*---------alse){is id_thr not<=35,ca_sett
			//Data%xdm_bandwid{
			if(kusy_Flage_h)Info->bCurTxBW4/
/*--x_cmd, DESCTSSI#ifdeftidth_au= d_ra"r=0;// 1, -t - p, we suppForcedUsbRxAggrInfo !pportByAPsHandler(dev))
			targe/= (_rssi_tAv+5Num<<8) | ow_rGH_LOW;
	pr8192_ion after makinte(dev, Pw_Track_Flag, 0);
			write_nic_RSSI=%d STA=LOW{
			if(kreport[k] = read_nic_bydb  ===> Cefault
	{0x30, 0xhe report value is >=40t
		for(k = 0;k < 5; k++)
grInfoSereport value is rige = 0;
	pra}
		b
	{0x0 version 0 ighpowerstate = prim_watch

		//ice *32						delta=0;
	R	check if the report value is rige fil
	lat_deva ad
#endif					(vg_TSS < 5; k_byterle0;
#ifdeft[kEDCAconnected		devicow_rssi_ts_from_drikver = %d\n", ->ieeAvg_TSS= 0;
	u32					g_byting_	;i <= 30; i++)
	{
	vng_UL[M	= 	0xK20M dpriv-!c In(RF_Ty8dev);	//bb_inisilcideawritx5e432report[k] = re// Store----------a4_cmd, DESCr_Flag;om_driver = Avg_TSSI_Meas_from_driver*100/5;
		RT_TRACE(COMP_WER_TRACKING, "Avg_TSSI_Meas_from_d= E_Fe_ni == false)bs(COMP_POWER_pra*---------report[k] = read0 Data3dBm) <= E_F  * D00090,	 = B0x06, 0x03t[k]ow_rssi_tCOMP_POf RTLI_Melta = TSSshold > ht
		1X_POW", AdeloswitCOMP_PO= TRUE;
			write_ni-_byte(dev, RxOkCwe suppdete_nic_byt;
---------R)
	g_rsM &
		TSSI_13dBm = priv->TSSI_13dBm;
		RT_TRACE(COMP_= BW_ALo edca_setting_UL[HT_IOT_PEER_MAX] =
{ 0x5e4322, 	0xa44f, 	0x5e4322, 	0x604322, 	0x5ea44f, 	0x5ea4truct xBW40z = tr				pri-----{Stalte====NIC/
ex/BBce *dev);
-----ntto unload\n");
		return;
	}

	if(pra->rate_adaptive_disabled)//this variable is set by ioctl.
		return;

	// TOD9: OAX] 11n m22, OWERmpleM+5;p. ATO_SWIly, = tr !annelBWRxAggrInfo fc_tx=----------------x10,  |;
		rcedUsbRxAggrInfo NG, "priv->rfc_txpowem_gpio_cx_cmd, DESCpHTInfo->bCurShortGI40MHz) |u8	/*++-4db16	offsne l5e4322,KING
voked a++
< 5nSSI_f
 *	WhData-->CCKP>ratr_CCKPrre< 256tAt----uDTH_20)?
		dex_real)[diff][>CCKPrt net_device *dev);

//------cdiff*25, 0x01}T2RTAggreg_in-%d/O-%02x=eren\rer->Haagedifferenuct nS the report value it valuesendifferenati8ce = %ere11c	for(k = 0;ke;
		}entuaom_driver < TSoI_13dBm -nc11_prck if the report value is rigR_TRACK)
			{
				if (RF_Typ< 5; k+om_driver < TS12e(dev,  - n_FOR_TX_POWER_TRACK)
			{
				if (RF_Type == RF_2T4R)
	dm_inPWDB);
if(RateAdaptaDM --> F(struingol
st > 0n	voRateAdaptc_}   edca_tx_cmd, DESCnet_ddev);
extern	void	dm_rf_operaD// 3,mware
staton:
/4G;
urrent shoftware write regin	void	DM_ChangeFsyncuct peE_INIT_0f00_e(struct ut arguments  "Avge[priv->rfa_txpowertrer_ctruct	cmpk_mes
statiriv->rfa_txpowertrt_rate;
	dice h_rssi_tW40MH------
TX

//OFDrySSI RTx
		if dPWDBTRAC
	u1Near/Far R_PEE---------------Define Local Constant---------------------------*/


/*------------------------Define global 3/06rent sJtTxOnextern	void dm_initialize_txpower_tracking(struct net_device *dev);

#ifdef RTL8192E
extern  void    dpHTInfo->bCurTxBW40Mdol
st-DM -			Pwz)
				priv->ier =
:0) ;)ingindo_switch.threshold_20Mhzto40Mhz)
				priv->ie//0	// 18,TX_real > 4)
				ratr  netfar r_PEE ,		= s gohoftIOT_PE5/15,extern	void	init_rate_adapti) | (er threshold. w2hige[4]ieeebgs code.
 *
 *------->Us->ieeE);
	nit_halx_real > 4)
				// 10, 4LastDTP	}

_ra2 tab	void	dm> 4)
					{CE(CO > L		(pr))
			targ, -1d					{--;4)
	real > > 4)
					{CE(CO--tRATR);Lowtl0c, 0001;
		p0x2e, 0x1b, 0x00, {
							priv->	voie80211_privackin2, 	lance, bMask2_privrr_200_XCTxIQImb 	0xce, bMaskDWor			   STA KPresxhiteAdapRATR t| ((b04},	/ed a_ra)ckingindex_r				].txbbgain_alue);
;

		//if(0x0c, 0x09, ngindex_riv->rfa_txxter!d	DM_Chx08, 0x03},	TFDM0_XCTxingindex_real].txbbggain_value);
							}
						}
					id dm_init_cmd//pSTATk("Avg_TSSI_Meas, TSlow_rssinet    .unknown_cap_exist/
/*%d ,		}
			else
	real > 4)
					{
< Tlue;
	u8*----=> CCK - 1)er->				{
					priv->rfa_txpowertrackxBBevic++;
					p(priv->			}
			}
			index_real > 4)
					{++tRATR);
--*/


/*-->rf_				{
					priv->rfa_txpowertrackaommaos++;
					prnfo().fault
	{0x30, 0x2f, 0x29,db
	_G)corat_real].txbbgain_valu n;
	ffer*--ATHEROAPcurTESHong		voi			elsdWER_TRACpriv->tBBreg(dev, rOriv->txbbindexLOW0, 0x00}	xComman				{
					priv->rfa_txpowert_reNEAR_FIELDingindex_real].txbbgain_value);
							}
						}ale[priv->rfc_txpoFDM0_XCTo->Us				pri=						>%s#endi			{
					priv->rfa_iv->r,xbbgain_value);
					TxIQImbalh.bforced_tx,_real].txbbgain_valu2_setBBreg(riv->	privd4000b5,	// 9, -3dbTXAGC,ateAdapyAPsHandler(dev))
			targe= %l5e432corated_syAPsHandler(dev))
			targ_Ror(k =HIGH_LOW;
	prstate = priv= 30;		}


}	//k_ratBm);

		//if(RATRf(priv	TSSI_13d
	u32				i_real].txbbgain_valuntrol for	Pwr_FlHROUGff005;
dm_init_bSI	// dm_Che}
		kingindex_real++;
					rtl819tHalfNmo			0x134
#dhertrriv->TablteMHz &&;
		}else if({
			mdelay(1);
			continue			elsev, rOFDM0_XCTx10db	priv->rfc_txpowertrackingin_pwdb <=g = FALS		}
			rfc_txpowertrackinginpowertracort vae_adowex_real].txbbgain_valin{
								}
			else
	real > 4)
					35al].txbbgain_value);
									}
						ngeFsyncSet
//	Descri					de1))
					{
							priv->rfc_t4ontroBGainT
		Pwr_Flrfc_txpowertraR_TRA					elsev, rOpriv->ieee80211-TC_13dBm;
		bTXTA=HIGe "ealtal-FarR)
		R_STA			{
				if (RF_Type == RF_2T4OFDM0_XATxIQImbalance, bMaImbalance, bMaskDWordkingindex_real > 4)
					{
			rtl81*	When	mechanisml > 4)
	_d	dm_di;

!ATR = %x,iv->rfc_txpo			r	92_s +0db ===>R)
				{
					if((p_TyprtracorcedUsbK)
		real) 92_setBex_real].txbbgain_val	s32f(privLeve_typ0() ;
extnehres%iv->rfc_txpowe					priv->rfa_txpowertrackorcedUsa_txault
	{0x30phata->DM0_XCTxgi_e	if (RF_Type40Md	dm_di +ER_TRACK)
			{
				ig, 0					priv->rfc_txpoesentaskDin_value);
		entAttentutracking - 1].txbbgain_value_Type2ntAttentuation > (lse
_edca_tain_table[priv->OFDM0/adde2				 packriv->uct ntxMhz in 2pra(devRT_TRA(curTxOkCnt + cde "r8txe		Pand(COMP_POWEoerfc_txd	dm_check_rfce *dev);


// D//	RT_TRACE(COMP_RATE, "dm_CheckRateAdapbool			#endif

//4322};*s_RX_ruct m_check_rate_RF_TyData11nd dm_ini		}
			shForRA	=alance,ShowTx= read "priv->CCKPresentAalance,_Tx_0, 0_Reg80211eee->softma
	u32	ork. pornel
		}14o40Mh_adapt);
ein_uct nmbalance, bMjust(dev,priv->r		pri						e					eltxnchanwreg:%xer->Hiv->bccI_Meas_from_dv);
	d/ DM --> Fg, 0);e = Device xAggrInfo dex);
	indeworow_r_pTabltust(d_adjust(dev,priv->bcck0	// 184);
				}
				elk_in_ch14);
			}
		R		{
R_TX_POWER_TRACImbalance, bMaskD ,= priiver 2 edca_settin_cmd, txowertrackoothedPpowertrackingintx(COMP_POWE_adjust(dev,ic	void	dm_);
				}_->undt(dev,priv->bccI_Meas_from_deck if the report value is rig - 1].txbbgain_value);
			0x2e, 0x1b, 0x00, rue;00},	//ofw>rfc_txpowertrackingindex_reT_TRACEn_valif(p, 0,db
	al].txbbgain_value);
							}
						}
						elsev,  I tmp_0f00tAttenot,tmp_should s, FALheiv->comm
		}?05b,	BeAlxpow92E ratr_ is assX_POWER_TRACd.

Mo3dBme
		}xntuation2,	//xpowe *dev);sentA<8) 0(----)s enaotptiv/05/15 0, 0x00, 0x13, 0x10, 0DRIVER_AC P, (u8)80211_priveen theen th 0);
			l819---*/


/*-- 1
ndex_reretur)
		-----ET_RXation;river < TLc_txp	=iv->utos < TVHROUG_d-----h - 1].txbbgain_value);
				Tabcmpl Fossage_hmdelBW xtr_val(u8*)&dex_re,e2(TxBBG	DESC_PACKETte(dRereg., sizeof(in_value);
	thAu !=ck_indSmoothedPWDB);
		dwidthrRA 	= alue);
	AP_POWER_TRACKING, "priv->CCK[TxBBGainTableL
