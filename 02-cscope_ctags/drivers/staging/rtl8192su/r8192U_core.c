/******************************************************************************
 * Copyright(c) 2008 - 2010 Realtek Corporation. All rights reserved.
 * Linux device driver for RTL8192U
 *
 * Based on the r8187 driver, which is:
 * Copyright 2004-2005 Andrea Merello <andreamrl@tiscali.it>, et al.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * Jerry chuang <wlanfae@realtek.com>
 */

#include <linux/vmalloc.h>

#undef LOOP_TEST
#undef DUMP_RX
#undef DUMP_TX
#undef DEBUG_TX_DESC2
#undef RX_DONT_PASS_UL
#undef DEBUG_EPROM
#undef DEBUG_RX_VERBOSE
#undef DUMMY_RX
#undef DEBUG_ZERO_RX
#undef DEBUG_RX_SKB
#undef DEBUG_TX_FRAG
#undef DEBUG_RX_FRAG
#undef DEBUG_TX_FILLDESC
#undef DEBUG_TX
#undef DEBUG_IRQ
#undef DEBUG_RX
#undef DEBUG_RXALLOC
#undef DEBUG_REGISTERS
#undef DEBUG_RING
#undef DEBUG_IRQ_TASKLET
#undef DEBUG_TX_ALLOC
#undef DEBUG_TX_DESC

#define CONFIG_RTL8192_IO_MAP

#include <asm/uaccess.h>
#include "r8192U.h"
#include "r8180_93cx6.h"   /* Card EEPROM */
#include "r8192U_wx.h"

#include "r8192S_rtl8225.h"
#include "r8192S_hw.h"
#include "r8192S_phy.h"
#include "r8192S_phyreg.h"
#include "r8192S_Efuse.h"

#include "r819xU_cmdpkt.h"
#include "r8192U_dm.h"
//#include "r8192xU_phyreg.h"
#include <linux/usb.h>

#include "r8192U_pm.h"

#include "ieee80211/dot11d.h"



u32 rt_global_debug_component = \
//				COMP_TRACE	|
//    				COMP_DBG	|
//				COMP_INIT    	|
//				COMP_RECV	|
//				COMP_SEND	|
//				COMP_IO		|
				COMP_POWER	|
//				COMP_EPROM   	|
				COMP_SWBW	|
				COMP_POWER_TRACKING |
				COMP_TURBO	|
				COMP_QOS	|
//				COMP_RATE	|
//				COMP_RM		|
				COMP_DIG	|
//				COMP_EFUSE	|
//				COMP_CH		|
//				COMP_TXAGC	|
                              	COMP_HIPWR	|
//                             	COMP_HALDM	|
				COMP_SEC	|
				COMP_LED	|
//				COMP_RF		|
//				COMP_RXDESC	|
				COMP_FIRMWARE	|
				COMP_HT		|
				COMP_AMSDU	|
				COMP_SCAN	|
//				COMP_CMD	|
				COMP_DOWN	|
				COMP_RESET	|
				COMP_ERR; //always open err flags on

#define TOTAL_CAM_ENTRY 32
#define CAM_CONTENT_COUNT 8

static struct usb_device_id rtl8192_usb_id_tbl[] = {
	/* Realtek */
	{USB_DEVICE(0x0bda, 0x8192)},
	{USB_DEVICE(0x0bda, 0x8709)},
	/* Corega */
	{USB_DEVICE(0x07aa, 0x0043)},
	/* Belkin */
	{USB_DEVICE(0x050d, 0x805E)},
	/* Sitecom */
	{USB_DEVICE(0x0df6, 0x0031)},
	/* EnGenius */
	{USB_DEVICE(0x1740, 0x9201)},
	/* Dlink */
	{USB_DEVICE(0x2001, 0x3301)},
	/* Zinwell */
	{USB_DEVICE(0x5a57, 0x0290)},
	//92SU
	{USB_DEVICE(0x0bda, 0x8172)},
	{}
};

MODULE_LICENSE("GPL");
MODULE_VERSION("V 1.1");
MODULE_DEVICE_TABLE(usb, rtl8192_usb_id_tbl);
MODULE_DESCRIPTION("Linux driver for Realtek RTL8192 USB WiFi cards");

static char* ifname = "wlan%d";
static int hwwep = 1;  //default use hw. set 0 to use software security
static int channels = 0x3fff;



module_param(ifname, charp, S_IRUGO|S_IWUSR );
//module_param(hwseqnum,int, S_IRUGO|S_IWUSR);
module_param(hwwep,int, S_IRUGO|S_IWUSR);
module_param(channels,int, S_IRUGO|S_IWUSR);

MODULE_PARM_DESC(ifname," Net interface name, wlan%d=default");
//MODULE_PARM_DESC(hwseqnum," Try to use hardware 802.11 header sequence numbers. Zero=default");
MODULE_PARM_DESC(hwwep," Try to use hardware security support. ");
MODULE_PARM_DESC(channels," Channel bitmask for specific locales. NYI");

static int __devinit rtl8192_usb_probe(struct usb_interface *intf,
			 const struct usb_device_id *id);
static void __devexit rtl8192_usb_disconnect(struct usb_interface *intf);

static struct usb_driver rtl8192_usb_driver = {
	.name		= RTL819xU_MODULE_NAME,	          /* Driver name   */
	.id_table	= rtl8192_usb_id_tbl,	          /* PCI_ID table  */
	.probe		= rtl8192_usb_probe,	          /* probe fn      */
	.disconnect	= rtl8192_usb_disconnect,	  /* remove fn     */
	.suspend	= rtl8192U_suspend,	          /* PM suspend fn */
	.resume		= rtl8192U_resume,                 /* PM resume fn  */
	.reset_resume   = rtl8192U_resume,                 /* PM reset resume fn  */
};


static void 	rtl8192SU_read_eeprom_info(struct net_device *dev);
short 	rtl8192SU_tx(struct net_device *dev, struct sk_buff* skb);
void 	rtl8192SU_rx_nomal(struct sk_buff* skb);
void 	rtl8192SU_rx_cmd(struct sk_buff *skb);
bool 	rtl8192SU_adapter_start(struct net_device *dev);
short	rtl8192SU_tx_cmd(struct net_device *dev, struct sk_buff *skb);
void 	rtl8192SU_link_change(struct net_device *dev);
void 	InitialGain8192S(struct net_device *dev,u8 Operation);
void 	rtl8192SU_query_rxdesc_status(struct sk_buff *skb, struct ieee80211_rx_stats *stats, bool bIsRxAggrSubframe);

struct rtl819x_ops rtl8192su_ops = {
	.nic_type = NIC_8192SU,
	.rtl819x_read_eeprom_info = rtl8192SU_read_eeprom_info,
	.rtl819x_tx = rtl8192SU_tx,
	.rtl819x_tx_cmd = rtl8192SU_tx_cmd,
	.rtl819x_rx_nomal = rtl8192SU_rx_nomal,
	.rtl819x_rx_cmd = rtl8192SU_rx_cmd,
	.rtl819x_adapter_start = rtl8192SU_adapter_start,
	.rtl819x_link_change = rtl8192SU_link_change,
	.rtl819x_initial_gain = InitialGain8192S,
	.rtl819x_query_rxdesc_status = rtl8192SU_query_rxdesc_status,
};


typedef struct _CHANNEL_LIST
{
	u8	Channel[32];
	u8	Len;
}CHANNEL_LIST, *PCHANNEL_LIST;

static CHANNEL_LIST ChannelPlan[] = {
	{{1,2,3,4,5,6,7,8,9,10,11,36,40,44,48,52,56,60,64,149,153,157,161,165},24},  		//FCC
	{{1,2,3,4,5,6,7,8,9,10,11},11},                    				//IC
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,36,40,44,48,52,56,60,64},21},  	//ETSI
	{{1,2,3,4,5,6,7,8,9,10,11,12,13},13},    //Spain. Change to ETSI.
	{{1,2,3,4,5,6,7,8,9,10,11,12,13},13},  	//France. Change to ETSI.
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,36,40,44,48,52,56,60,64},22},	//MKK					//MKK
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,36,40,44,48,52,56,60,64},22},//MKK1
	{{1,2,3,4,5,6,7,8,9,10,11,12,13},13},	//Israel.
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,36,40,44,48,52,56,60,64},22},			// For 11a , TELEC
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,36,40,44,48,52,56,60,64}, 22},    //MIC
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14}					//For Global Domain. 1-11:active scan, 12-14 passive scan. //+YJ, 080626
};

static void rtl819x_set_channel_map(u8 channel_plan, struct r8192_priv* priv)
{
	int i, max_chan=-1, min_chan=-1;
	struct ieee80211_device* ieee = priv->ieee80211;
	switch (channel_plan)
	{
		case COUNTRY_CODE_FCC:
		case COUNTRY_CODE_IC:
		case COUNTRY_CODE_ETSI:
		case COUNTRY_CODE_SPAIN:
		case COUNTRY_CODE_FRANCE:
		case COUNTRY_CODE_MKK:
		case COUNTRY_CODE_MKK1:
		case COUNTRY_CODE_ISRAEL:
		case COUNTRY_CODE_TELEC:
		case COUNTRY_CODE_MIC:
		{
			Dot11d_Init(ieee);
			ieee->bGlobalDomain = false;
			//acturally 8225 & 8256 rf chip only support B,G,24N mode
                        if ((priv->rf_chip == RF_8225) || (priv->rf_chip == RF_8256) || (priv->rf_chip == RF_6052))
			{
				min_chan = 1;
				max_chan = 14;
			}
			else
			{
				RT_TRACE(COMP_ERR, "unknown rf chip, can't set channel map in function:%s()\n", __FUNCTION__);
			}
			if (ChannelPlan[channel_plan].Len != 0){
				// Clear old channel map
				memset(GET_DOT11D_INFO(ieee)->channel_map, 0, sizeof(GET_DOT11D_INFO(ieee)->channel_map));
				// Set new channel map
				for (i=0;i<ChannelPlan[channel_plan].Len;i++)
				{
					if (ChannelPlan[channel_plan].Channel[i] < min_chan || ChannelPlan[channel_plan].Channel[i] > max_chan)
					break;
					GET_DOT11D_INFO(ieee)->channel_map[ChannelPlan[channel_plan].Channel[i]] = 1;
				}
			}
			break;
		}
		case COUNTRY_CODE_GLOBAL_DOMAIN:
		{
			GET_DOT11D_INFO(ieee)->bEnabled = 0;//this flag enabled to follow 11d country IE setting, otherwise, it shall follow global domain settings.
			Dot11d_Reset(ieee);
			ieee->bGlobalDomain = true;
			break;
		}
		default:
			break;
	}
	return;
}

#define eqMacAddr(a,b) ( ((a)[0]==(b)[0] && (a)[1]==(b)[1] && (a)[2]==(b)[2] && (a)[3]==(b)[3] && (a)[4]==(b)[4] && (a)[5]==(b)[5]) ? 1:0 )

#define		rx_hal_is_cck_rate(_pDesc)\
			((_pDesc->RxMCS  == DESC92S_RATE1M ||\
			_pDesc->RxMCS == DESC92S_RATE2M ||\
			_pDesc->RxMCS == DESC92S_RATE5_5M ||\
			_pDesc->RxMCS == DESC92S_RATE11M) &&\
			!_pDesc->RxHT)

#define 	tx_hal_is_cck_rate(_DataRate)\
			( _DataRate == MGN_1M ||\
			 _DataRate == MGN_2M ||\
			 _DataRate == MGN_5_5M ||\
			 _DataRate == MGN_11M )




void CamResetAllEntry(struct net_device *dev)
{
#if 1
	u32 ulcommand = 0;
        //2004/02/11  In static WEP, OID_ADD_KEY or OID_ADD_WEP are set before STA associate to AP.
        // However, ResetKey is called on OID_802_11_INFRASTRUCTURE_MODE and MlmeAssociateRequest
        // In this condition, Cam can not be reset because upper layer will not set this static key again.
        //if(Adapter->EncAlgorithm == WEP_Encryption)
        //      return;
//debug
        //DbgPrint("========================================\n");
        //DbgPrint("                            Call ResetAllEntry                                              \n");
        //DbgPrint("========================================\n\n");
	ulcommand |= BIT31|BIT30;
	write_nic_dword(dev, RWCAM, ulcommand);
#else
        for(ucIndex=0;ucIndex<TOTAL_CAM_ENTRY;ucIndex++)
                CAM_mark_invalid(dev, ucIndex);
        for(ucIndex=0;ucIndex<TOTAL_CAM_ENTRY;ucIndex++)
                CAM_empty_entry(dev, ucIndex);
#endif

}


void write_cam(struct net_device *dev, u8 addr, u32 data)
{
        write_nic_dword(dev, WCAMI, data);
        write_nic_dword(dev, RWCAM, BIT31|BIT16|(addr&0xff) );
}

u32 read_cam(struct net_device *dev, u8 addr)
{
        write_nic_dword(dev, RWCAM, 0x80000000|(addr&0xff) );
        return read_nic_dword(dev, 0xa8);
}

void write_nic_byte_E(struct net_device *dev, int indx, u8 data)
{
	int status;
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;

	status = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			       RTL8187_REQ_SET_REGS, RTL8187_REQT_WRITE,
			       indx|0xfe00, 0, &data, 1, HZ / 2);

	if (status < 0)
	{
		printk("write_nic_byte_E TimeOut! status:%d\n", status);
	}
}

u8 read_nic_byte_E(struct net_device *dev, int indx)
{
	int status;
	u8 data;
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;

	status = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       indx|0xfe00, 0, &data, 1, HZ / 2);

        if (status < 0)
        {
                printk("read_nic_byte_E TimeOut! status:%d\n", status);
        }

	return data;
}
//as 92U has extend page from 4 to 16, so modify functions below.
void write_nic_byte(struct net_device *dev, int indx, u8 data)
{
	int status;

	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;

	status = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			       RTL8187_REQ_SET_REGS, RTL8187_REQT_WRITE,
			       indx, 0, &data, 1, HZ / 2);

        if (status < 0)
        {
                printk("write_nic_byte TimeOut! status:%d\n", status);
        }


}


void write_nic_word(struct net_device *dev, int indx, u16 data)
{

	int status;

	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;

	status = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			       RTL8187_REQ_SET_REGS, RTL8187_REQT_WRITE,
			       indx, 0, &data, 2, HZ / 2);

        if (status < 0)
        {
                printk("write_nic_word TimeOut! status:%d\n", status);
        }

}


void write_nic_dword(struct net_device *dev, int indx, u32 data)
{

	int status;

	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;

	status = usb_control_msg(udev, usb_sndctrlpipe(udev, 0),
			       RTL8187_REQ_SET_REGS, RTL8187_REQT_WRITE,
			       indx, 0, &data, 4, HZ / 2);


        if (status < 0)
        {
                printk("write_nic_dword TimeOut! status:%d\n", status);
        }

}



u8 read_nic_byte(struct net_device *dev, int indx)
{
	u8 data;
	int status;
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;

	status = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       indx, 0, &data, 1, HZ / 2);

        if (status < 0)
        {
                printk("read_nic_byte TimeOut! status:%d\n", status);
        }

	return data;
}



u16 read_nic_word(struct net_device *dev, int indx)
{
	u16 data;
	int status;
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;

	status = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       indx, 0, &data, 2, HZ / 2);

        if (status < 0)
        {
                printk("read_nic_word TimeOut! status:%d\n", status);
        }


	return data;
}

u16 read_nic_word_E(struct net_device *dev, int indx)
{
	u16 data;
	int status;
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;

	status = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       indx|0xfe00, 0, &data, 2, HZ / 2);

        if (status < 0)
        {
                printk("read_nic_word TimeOut! status:%d\n", status);
        }


	return data;
}

u32 read_nic_dword(struct net_device *dev, int indx)
{
	u32 data;
	int status;
//	int result;

	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;

	status = usb_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       indx, 0, &data, 4, HZ / 2);
//	if(0 != result) {
//	  printk(KERN_WARNING "read size of data = %d\, date = %d\n", result, data);
//	}

        if (status < 0)
        {
                printk("read_nic_dword TimeOut! status:%d\n", status);
		if(status == -ENODEV) {
			priv->usb_error = true;
		}
        }



	return data;
}


//u8 read_phy_cck(struct net_device *dev, u8 adr);
//u8 read_phy_ofdm(struct net_device *dev, u8 adr);
/* this might still called in what was the PHY rtl8185/rtl8192 common code
 * plans are to possibilty turn it again in one common code...
 */
inline void force_pci_posting(struct net_device *dev)
{
}


static struct net_device_stats *rtl8192_stats(struct net_device *dev);
void rtl8192_commit(struct net_device *dev);
//void rtl8192_restart(struct net_device *dev);
void rtl8192_restart(struct work_struct *work);
//void rtl8192_rq_tx_ack(struct work_struct *work);

void watch_dog_timer_callback(unsigned long data);

/****************************************************************************
   -----------------------------PROCFS STUFF-------------------------
*****************************************************************************/

static struct proc_dir_entry *rtl8192_proc = NULL;



static int proc_get_stats_ap(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struct ieee80211_device *ieee = priv->ieee80211;
	struct ieee80211_network *target;

	int len = 0;

        list_for_each_entry(target, &ieee->network_list, list) {

		len += snprintf(page + len, count - len,
                "%s ", target->ssid);

		if(target->wpa_ie_len>0 || target->rsn_ie_len>0){
	                len += snprintf(page + len, count - len,
        	        "WPA\n");
		}
		else{
                        len += snprintf(page + len, count - len,
                        "non_WPA\n");
                }

        }

	*eof = 1;
	return len;
}

static int proc_get_registers(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
//	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	int len = 0;
	int i,n,page0,page1,page2;

	int max=0xff;
	page0 = 0x000;
	page1 = 0x100;
	page2 = 0x800;

	/* This dump the current register page */
	if(!IS_BB_REG_OFFSET_92S(page0)){
		len += snprintf(page + len, count - len,
				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
		{
			len += snprintf(page + len, count - len,
					"\nD:  %2x > ",n);
			for(i=0;i<16 && n<=max;i++,n++)
				len += snprintf(page + len, count - len,
						"%2.2x ",read_nic_byte(dev,(page0|n)));
		}
	}else{
		len += snprintf(page + len, count - len,
				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
		{
			len += snprintf(page + len, count - len, "\nD:  %2x > ",n);
			for(i=0;i<4 && n<=max;n+=4,i++)
				len += snprintf(page + len, count - len,
						"%8.8x ",rtl8192_QueryBBReg(dev,(page0|n), bMaskDWord));
		}
	}
	len += snprintf(page + len, count - len,"\n");
	*eof = 1;
	return len;

}
static int proc_get_registers_1(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
//	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	int len = 0;
	int i,n,page0;

	int max=0xff;
	page0 = 0x100;

	/* This dump the current register page */
		len += snprintf(page + len, count - len,
				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
		{
			len += snprintf(page + len, count - len,
					"\nD:  %2x > ",n);
			for(i=0;i<16 && n<=max;i++,n++)
				len += snprintf(page + len, count - len,
						"%2.2x ",read_nic_byte(dev,(page0|n)));
		}
	len += snprintf(page + len, count - len,"\n");
	*eof = 1;
	return len;

}
static int proc_get_registers_2(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
//	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	int len = 0;
	int i,n,page0;

	int max=0xff;
	page0 = 0x200;

	/* This dump the current register page */
		len += snprintf(page + len, count - len,
				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
		{
			len += snprintf(page + len, count - len,
					"\nD:  %2x > ",n);
			for(i=0;i<16 && n<=max;i++,n++)
				len += snprintf(page + len, count - len,
						"%2.2x ",read_nic_byte(dev,(page0|n)));
		}
	len += snprintf(page + len, count - len,"\n");
	*eof = 1;
	return len;

}
static int proc_get_registers_8(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;

	int len = 0;
	int i,n,page0;

	int max=0xff;
	page0 = 0x800;

	/* This dump the current register page */
		len += snprintf(page + len, count - len,
				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
		{
			len += snprintf(page + len, count - len, "\nD:  %2x > ",n);
			for(i=0;i<4 && n<=max;n+=4,i++)
				len += snprintf(page + len, count - len,
						"%8.8x ",rtl8192_QueryBBReg(dev,(page0|n), bMaskDWord));
		}
	len += snprintf(page + len, count - len,"\n");
	*eof = 1;
	return len;

	}
static int proc_get_registers_9(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
//	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	int len = 0;
	int i,n,page0;

	int max=0xff;
	page0 = 0x900;

	/* This dump the current register page */
		len += snprintf(page + len, count - len,
				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
		{
			len += snprintf(page + len, count - len, "\nD:  %2x > ",n);
			for(i=0;i<4 && n<=max;n+=4,i++)
			len += snprintf(page + len, count - len,
						"%8.8x ",rtl8192_QueryBBReg(dev,(page0|n), bMaskDWord));
		}
	len += snprintf(page + len, count - len,"\n");
	*eof = 1;
	return len;
}
static int proc_get_registers_a(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
//	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	int len = 0;
	int i,n,page0;

	int max=0xff;
	page0 = 0xa00;

	/* This dump the current register page */
				len += snprintf(page + len, count - len,
				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
		{
			len += snprintf(page + len, count - len, "\nD:  %2x > ",n);
			for(i=0;i<4 && n<=max;n+=4,i++)
				len += snprintf(page + len, count - len,
						"%8.8x ",rtl8192_QueryBBReg(dev,(page0|n), bMaskDWord));
		}
	len += snprintf(page + len, count - len,"\n");
	*eof = 1;
	return len;
}
static int proc_get_registers_b(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
//	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	int len = 0;
	int i,n,page0;

	int max=0xff;
	page0 = 0xb00;

	/* This dump the current register page */
		len += snprintf(page + len, count - len,
				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
		{
			len += snprintf(page + len, count - len, "\nD:  %2x > ",n);
			for(i=0;i<4 && n<=max;n+=4,i++)
				len += snprintf(page + len, count - len,
						"%8.8x ",rtl8192_QueryBBReg(dev,(page0|n), bMaskDWord));
		}
	len += snprintf(page + len, count - len,"\n");
	*eof = 1;
	return len;
	}
static int proc_get_registers_c(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
//	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	int len = 0;
	int i,n,page0;

	int max=0xff;
	page0 = 0xc00;

	/* This dump the current register page */
		len += snprintf(page + len, count - len,
				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
		{
			len += snprintf(page + len, count - len, "\nD:  %2x > ",n);
			for(i=0;i<4 && n<=max;n+=4,i++)
				len += snprintf(page + len, count - len,
						"%8.8x ",rtl8192_QueryBBReg(dev,(page0|n), bMaskDWord));
		}
	len += snprintf(page + len, count - len,"\n");
	*eof = 1;
	return len;
}
static int proc_get_registers_d(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
//	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	int len = 0;
	int i,n,page0;

	int max=0xff;
	page0 = 0xd00;

	/* This dump the current register page */
		len += snprintf(page + len, count - len,
				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
		{
			len += snprintf(page + len, count - len, "\nD:  %2x > ",n);
			for(i=0;i<4 && n<=max;n+=4,i++)
				len += snprintf(page + len, count - len,
						"%8.8x ",rtl8192_QueryBBReg(dev,(page0|n), bMaskDWord));
		}
	len += snprintf(page + len, count - len,"\n");
	*eof = 1;
	return len;
}
static int proc_get_registers_e(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
//	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	int len = 0;
	int i,n,page0;

	int max=0xff;
	page0 = 0xe00;

	/* This dump the current register page */
		len += snprintf(page + len, count - len,
				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
		{
			len += snprintf(page + len, count - len, "\nD:  %2x > ",n);
			for(i=0;i<4 && n<=max;n+=4,i++)
				len += snprintf(page + len, count - len,
						"%8.8x ",rtl8192_QueryBBReg(dev,(page0|n), bMaskDWord));
		}
	len += snprintf(page + len, count - len,"\n");
	*eof = 1;
	return len;
}

static int proc_get_stats_tx(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	int len = 0;

	len += snprintf(page + len, count - len,
		"TX VI priority ok int: %lu\n"
		"TX VI priority error int: %lu\n"
		"TX VO priority ok int: %lu\n"
		"TX VO priority error int: %lu\n"
		"TX BE priority ok int: %lu\n"
		"TX BE priority error int: %lu\n"
		"TX BK priority ok int: %lu\n"
		"TX BK priority error int: %lu\n"
		"TX MANAGE priority ok int: %lu\n"
		"TX MANAGE priority error int: %lu\n"
		"TX BEACON priority ok int: %lu\n"
		"TX BEACON priority error int: %lu\n"
//		"TX high priority ok int: %lu\n"
//		"TX high priority failed error int: %lu\n"
		"TX queue resume: %lu\n"
		"TX queue stopped?: %d\n"
		"TX fifo overflow: %lu\n"
//		"TX beacon: %lu\n"
		"TX VI queue: %d\n"
		"TX VO queue: %d\n"
		"TX BE queue: %d\n"
		"TX BK queue: %d\n"
//		"TX HW queue: %d\n"
		"TX VI dropped: %lu\n"
		"TX VO dropped: %lu\n"
		"TX BE dropped: %lu\n"
		"TX BK dropped: %lu\n"
		"TX total data packets %lu\n",
//		"TX beacon aborted: %lu\n",
		priv->stats.txviokint,
		priv->stats.txvierr,
		priv->stats.txvookint,
		priv->stats.txvoerr,
		priv->stats.txbeokint,
		priv->stats.txbeerr,
		priv->stats.txbkokint,
		priv->stats.txbkerr,
		priv->stats.txmanageokint,
		priv->stats.txmanageerr,
		priv->stats.txbeaconokint,
		priv->stats.txbeaconerr,
//		priv->stats.txhpokint,
//		priv->stats.txhperr,
		priv->stats.txresumed,
		netif_queue_stopped(dev),
		priv->stats.txoverflow,
//		priv->stats.txbeacon,
		atomic_read(&(priv->tx_pending[VI_PRIORITY])),
		atomic_read(&(priv->tx_pending[VO_PRIORITY])),
		atomic_read(&(priv->tx_pending[BE_PRIORITY])),
		atomic_read(&(priv->tx_pending[BK_PRIORITY])),
//		read_nic_byte(dev, TXFIFOCOUNT),
		priv->stats.txvidrop,
		priv->stats.txvodrop,
		priv->stats.txbedrop,
		priv->stats.txbkdrop,
		priv->stats.txdatapkt
//		priv->stats.txbeaconerr
		);

	*eof = 1;
	return len;
}



static int proc_get_stats_rx(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	int len = 0;

	len += snprintf(page + len, count - len,
		"RX packets: %lu\n"
		"RX urb status error: %lu\n"
		"RX invalid urb error: %lu\n",
		priv->stats.rxoktotal,
		priv->stats.rxstaterr,
		priv->stats.rxurberr);

	*eof = 1;
	return len;
}

void rtl8192_proc_module_init(void)
{
	RT_TRACE(COMP_INIT, "Initializing proc filesystem");
	rtl8192_proc=create_proc_entry(RTL819xU_MODULE_NAME, S_IFDIR, init_net.proc_net);
}


void rtl8192_proc_module_remove(void)
{
	remove_proc_entry(RTL819xU_MODULE_NAME, init_net.proc_net);
}


void rtl8192_proc_remove_one(struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);


	if (priv->dir_dev) {
	//	remove_proc_entry("stats-hw", priv->dir_dev);
		remove_proc_entry("stats-tx", priv->dir_dev);
		remove_proc_entry("stats-rx", priv->dir_dev);
	//	remove_proc_entry("stats-ieee", priv->dir_dev);
		remove_proc_entry("stats-ap", priv->dir_dev);
		remove_proc_entry("registers", priv->dir_dev);
		remove_proc_entry("registers-1", priv->dir_dev);
		remove_proc_entry("registers-2", priv->dir_dev);
		remove_proc_entry("registers-8", priv->dir_dev);
		remove_proc_entry("registers-9", priv->dir_dev);
		remove_proc_entry("registers-a", priv->dir_dev);
		remove_proc_entry("registers-b", priv->dir_dev);
		remove_proc_entry("registers-c", priv->dir_dev);
		remove_proc_entry("registers-d", priv->dir_dev);
		remove_proc_entry("registers-e", priv->dir_dev);
	//	remove_proc_entry("cck-registers",priv->dir_dev);
	//	remove_proc_entry("ofdm-registers",priv->dir_dev);
		//remove_proc_entry(dev->name, rtl8192_proc);
		remove_proc_entry("wlan0", rtl8192_proc);
		priv->dir_dev = NULL;
	}
}


void rtl8192_proc_init_one(struct net_device *dev)
{
	struct proc_dir_entry *e;
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	priv->dir_dev = create_proc_entry(dev->name,
					  S_IFDIR | S_IRUGO | S_IXUGO,
					  rtl8192_proc);
	if (!priv->dir_dev) {
		RT_TRACE(COMP_ERR, "Unable to initialize /proc/net/rtl8192/%s\n",
		      dev->name);
		return;
	}
	e = create_proc_read_entry("stats-rx", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_stats_rx, dev);

	if (!e) {
		RT_TRACE(COMP_ERR,"Unable to initialize "
		      "/proc/net/rtl8192/%s/stats-rx\n",
		      dev->name);
	}


	e = create_proc_read_entry("stats-tx", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_stats_tx, dev);

	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/stats-tx\n",
		      dev->name);
	}

	e = create_proc_read_entry("stats-ap", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_stats_ap, dev);

	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/stats-ap\n",
		      dev->name);
	}

	e = create_proc_read_entry("registers", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_registers, dev);
	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/registers\n",
		      dev->name);
	}
	e = create_proc_read_entry("registers-1", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_registers_1, dev);
	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/registers-1\n",
		      dev->name);
	}
	e = create_proc_read_entry("registers-2", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_registers_2, dev);
	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/registers-2\n",
		      dev->name);
	}
	e = create_proc_read_entry("registers-8", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_registers_8, dev);
	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/registers-8\n",
		      dev->name);
	}
	e = create_proc_read_entry("registers-9", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_registers_9, dev);
	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/registers-9\n",
		      dev->name);
	}
	e = create_proc_read_entry("registers-a", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_registers_a, dev);
	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/registers-a\n",
		      dev->name);
	}
	e = create_proc_read_entry("registers-b", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_registers_b, dev);
	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/registers-b\n",
		      dev->name);
	}
	e = create_proc_read_entry("registers-c", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_registers_c, dev);
	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/registers-c\n",
		      dev->name);
	}
	e = create_proc_read_entry("registers-d", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_registers_d, dev);
	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/registers-d\n",
		      dev->name);
	}
	e = create_proc_read_entry("registers-e", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_registers_e, dev);
	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/registers-e\n",
		      dev->name);
	}
}
/****************************************************************************
   -----------------------------MISC STUFF-------------------------
*****************************************************************************/

/* this is only for debugging */
void print_buffer(u32 *buffer, int len)
{
	int i;
	u8 *buf =(u8*)buffer;

	printk("ASCII BUFFER DUMP (len: %x):\n",len);

	for(i=0;i<len;i++)
		printk("%c",buf[i]);

	printk("\nBINARY BUFFER DUMP (len: %x):\n",len);

	for(i=0;i<len;i++)
		printk("%x",buf[i]);

	printk("\n");
}

//short check_nic_enough_desc(struct net_device *dev, priority_t priority)
short check_nic_enough_desc(struct net_device *dev,int queue_index)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	int used = atomic_read(&priv->tx_pending[queue_index]);

	return (used < MAX_TX_URB);
}

void tx_timeout(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	//rtl8192_commit(dev);

	schedule_work(&priv->reset_wq);
	//DMESG("TXTIMEOUT");
}


/* this is only for debug */
void dump_eprom(struct net_device *dev)
{
	int i;
	for(i=0; i<63; i++)
		RT_TRACE(COMP_EPROM, "EEPROM addr %x : %x", i, eprom_read(dev,i));
}

/* this is only for debug */
void rtl8192_dump_reg(struct net_device *dev)
{
	int i;
	int n;
	int max=0x1ff;

	RT_TRACE(COMP_PHY, "Dumping NIC register map");

	for(n=0;n<=max;)
	{
		printk( "\nD: %2x> ", n);
		for(i=0;i<16 && n<=max;i++,n++)
			printk("%2x ",read_nic_byte(dev,n));
	}
	printk("\n");
}

/****************************************************************************
      ------------------------------HW STUFF---------------------------
*****************************************************************************/

void rtl8192_set_mode(struct net_device *dev,int mode)
{
	u8 ecmd;
	ecmd=read_nic_byte(dev, EPROM_CMD);
	ecmd=ecmd &~ EPROM_CMD_OPERATING_MODE_MASK;
	ecmd=ecmd | (mode<<EPROM_CMD_OPERATING_MODE_SHIFT);
	ecmd=ecmd &~ (1<<EPROM_CS_SHIFT);
	ecmd=ecmd &~ (1<<EPROM_CK_SHIFT);
	write_nic_byte(dev, EPROM_CMD, ecmd);
}


void rtl8192_update_msr(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8 msr;

	msr  = read_nic_byte(dev, MSR);
	msr &= ~ MSR_LINK_MASK;

	/* do not change in link_state != WLAN_LINK_ASSOCIATED.
	 * msr must be updated if the state is ASSOCIATING.
	 * this is intentional and make sense for ad-hoc and
	 * master (see the create BSS/IBSS func)
	 */
	if (priv->ieee80211->state == IEEE80211_LINKED){

		if (priv->ieee80211->iw_mode == IW_MODE_INFRA)
			msr |= (MSR_LINK_MANAGED<<MSR_LINK_SHIFT);
		else if (priv->ieee80211->iw_mode == IW_MODE_ADHOC)
			msr |= (MSR_LINK_ADHOC<<MSR_LINK_SHIFT);
		else if (priv->ieee80211->iw_mode == IW_MODE_MASTER)
			msr |= (MSR_LINK_MASTER<<MSR_LINK_SHIFT);

	}else
		msr |= (MSR_LINK_NONE<<MSR_LINK_SHIFT);

	write_nic_byte(dev, MSR, msr);
}

void rtl8192_set_chan(struct net_device *dev,short ch)
{
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
//	u32 tx;
	RT_TRACE(COMP_CH, "=====>%s()====ch:%d\n", __FUNCTION__, ch);
	//printk("=====>%s()====ch:%d\n", __FUNCTION__, ch);
	priv->chan=ch;

	/* this hack should avoid frame TX during channel setting*/


//	tx = read_nic_dword(dev,TX_CONF);
//	tx &= ~TX_LOOPBACK_MASK;

#ifndef LOOP_TEST
//	write_nic_dword(dev,TX_CONF, tx |( TX_LOOPBACK_MAC<<TX_LOOPBACK_SHIFT));

	//need to implement rf set channel here WB

	if (priv->rf_set_chan)
	priv->rf_set_chan(dev,priv->chan);
	mdelay(10);
//	write_nic_dword(dev,TX_CONF,tx | (TX_LOOPBACK_NONE<<TX_LOOPBACK_SHIFT));
#endif
}

static void rtl8192_rx_isr(struct urb *urb);

u32 get_rxpacket_shiftbytes_819xusb(struct ieee80211_rx_stats *pstats)
{

		return (sizeof(rx_desc_819x_usb) + pstats->RxDrvInfoSize
				+ pstats->RxBufShift);

}
static int rtl8192_rx_initiate(struct net_device*dev)
{
        struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
        struct urb *entry;
        struct sk_buff *skb;
        struct rtl8192_rx_info *info;

	/* nomal packet rx procedure */
        while (skb_queue_len(&priv->rx_queue) < MAX_RX_URB) {
                skb = __dev_alloc_skb(RX_URB_SIZE, GFP_KERNEL);
                if (!skb)
                        break;
	        entry = usb_alloc_urb(0, GFP_KERNEL);
                if (!entry) {
                        kfree_skb(skb);
                        break;
                }
//		printk("nomal packet IN request!\n");
                usb_fill_bulk_urb(entry, priv->udev,
                                  usb_rcvbulkpipe(priv->udev, 3), skb_tail_pointer(skb),
                                  RX_URB_SIZE, rtl8192_rx_isr, skb);
                info = (struct rtl8192_rx_info *) skb->cb;
                info->urb = entry;
                info->dev = dev;
		info->out_pipe = 3; //denote rx normal packet queue
                skb_queue_tail(&priv->rx_queue, skb);
                usb_submit_urb(entry, GFP_KERNEL);
        }

	/* command packet rx procedure */
        while (skb_queue_len(&priv->rx_queue) < MAX_RX_URB + 3) {
//		printk("command packet IN request!\n");
                skb = __dev_alloc_skb(RX_URB_SIZE ,GFP_KERNEL);
                if (!skb)
                        break;
                entry = usb_alloc_urb(0, GFP_KERNEL);
                if (!entry) {
                        kfree_skb(skb);
                        break;
                }
                usb_fill_bulk_urb(entry, priv->udev,
                                  usb_rcvbulkpipe(priv->udev, 9), skb_tail_pointer(skb),
                                  RX_URB_SIZE, rtl8192_rx_isr, skb);
                info = (struct rtl8192_rx_info *) skb->cb;
                info->urb = entry;
                info->dev = dev;
		   info->out_pipe = 9; //denote rx cmd packet queue
                skb_queue_tail(&priv->rx_queue, skb);
		usb_submit_urb(entry, GFP_KERNEL);
        }

        return 0;
}

void rtl8192_set_rxconf(struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	u32 rxconf;

	rxconf=read_nic_dword(dev,RCR);
	rxconf = rxconf &~ MAC_FILTER_MASK;
	rxconf = rxconf | RCR_AMF;
	rxconf = rxconf | RCR_ADF;
	rxconf = rxconf | RCR_AB;
	rxconf = rxconf | RCR_AM;
	//rxconf = rxconf | RCR_ACF;

	if (dev->flags & IFF_PROMISC) {DMESG ("NIC in promisc mode");}

	if(priv->ieee80211->iw_mode == IW_MODE_MONITOR || \
	   dev->flags & IFF_PROMISC){
		rxconf = rxconf | RCR_AAP;
	} /*else if(priv->ieee80211->iw_mode == IW_MODE_MASTER){
		rxconf = rxconf | (1<<ACCEPT_ALLMAC_FRAME_SHIFT);
		rxconf = rxconf | (1<<RX_CHECK_BSSID_SHIFT);
	}*/else{
		rxconf = rxconf | RCR_APM;
		rxconf = rxconf | RCR_CBSSID;
	}


	if(priv->ieee80211->iw_mode == IW_MODE_MONITOR){
		rxconf = rxconf | RCR_AICV;
		rxconf = rxconf | RCR_APWRMGT;
	}

	if( priv->crcmon == 1 && priv->ieee80211->iw_mode == IW_MODE_MONITOR)
		rxconf = rxconf | RCR_ACRC32;


	rxconf = rxconf &~ RX_FIFO_THRESHOLD_MASK;
	rxconf = rxconf | (RX_FIFO_THRESHOLD_NONE<<RX_FIFO_THRESHOLD_SHIFT);
	rxconf = rxconf &~ MAX_RX_DMA_MASK;
	rxconf = rxconf | ((u32)7<<RCR_MXDMA_OFFSET);

//	rxconf = rxconf | (1<<RX_AUTORESETPHY_SHIFT);
	rxconf = rxconf | RCR_ONLYERLPKT;

//	rxconf = rxconf &~ RCR_CS_MASK;
//	rxconf = rxconf | (1<<RCR_CS_SHIFT);

	write_nic_dword(dev, RCR, rxconf);

	#ifdef DEBUG_RX
	DMESG("rxconf: %x %x",rxconf ,read_nic_dword(dev,RCR));
	#endif
}
//wait to be removed
void rtl8192_rx_enable(struct net_device *dev)
{
	//u8 cmd;

	//struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	rtl8192_rx_initiate(dev);

//	rtl8192_set_rxconf(dev);
}


void rtl8192_tx_enable(struct net_device *dev)
{
}

void rtl8192_rtx_disable(struct net_device *dev)
{
	u8 cmd;
	struct r8192_priv *priv = ieee80211_priv(dev);
	struct sk_buff *skb;
	struct rtl8192_rx_info *info;

	cmd=read_nic_byte(dev,CMDR);
	write_nic_byte(dev, CMDR, cmd &~ \
		(CR_TE|CR_RE));
	force_pci_posting(dev);
	mdelay(10);

	while ((skb = __skb_dequeue(&priv->rx_queue))) {
		info = (struct rtl8192_rx_info *) skb->cb;
		if (!info->urb)
			continue;

		usb_kill_urb(info->urb);
		kfree_skb(skb);
	}

	if (skb_queue_len(&priv->skb_queue)) {
		printk(KERN_WARNING "skb_queue not empty\n");
	}

	skb_queue_purge(&priv->skb_queue);
	return;
}


int alloc_tx_beacon_desc_ring(struct net_device *dev, int count)
{
	return 0;
}

inline u16 ieeerate2rtlrate(int rate)
{
	switch(rate){
	case 10:
	return 0;
	case 20:
	return 1;
	case 55:
	return 2;
	case 110:
	return 3;
	case 60:
	return 4;
	case 90:
	return 5;
	case 120:
	return 6;
	case 180:
	return 7;
	case 240:
	return 8;
	case 360:
	return 9;
	case 480:
	return 10;
	case 540:
	return 11;
	default:
	return 3;

	}
}
static u16 rtl_rate[] = {10,20,55,110,60,90,120,180,240,360,480,540};
inline u16 rtl8192_rate2rate(short rate)
{
	if (rate >11) return 0;
	return rtl_rate[rate];
}

static void rtl8192_rx_isr(struct urb *urb)
{
        struct sk_buff *skb = (struct sk_buff *) urb->context;
        struct rtl8192_rx_info *info = (struct rtl8192_rx_info *)skb->cb;
        struct net_device *dev = info->dev;
	struct r8192_priv *priv = ieee80211_priv(dev);
	int out_pipe = info->out_pipe;
	int err;
	if(!priv->up)
		return;
        if (unlikely(urb->status)) {
                info->urb = NULL;
                priv->stats.rxstaterr++;
                priv->ieee80211->stats.rx_errors++;
                usb_free_urb(urb);
	//	printk("%s():rx status err\n",__FUNCTION__);
                return;
        }

        skb_unlink(skb, &priv->rx_queue);
        skb_put(skb, urb->actual_length);

	skb_queue_tail(&priv->skb_queue, skb);
	tasklet_schedule(&priv->irq_rx_tasklet);

        skb = dev_alloc_skb(RX_URB_SIZE);
        if (unlikely(!skb)) {
                usb_free_urb(urb);
		printk("%s():can,t alloc skb\n",__FUNCTION__);
                /* TODO check rx queue length and refill *somewhere* */
                return;
        }

	usb_fill_bulk_urb(urb, priv->udev,
			usb_rcvbulkpipe(priv->udev, out_pipe),
			skb_tail_pointer(skb),
			RX_URB_SIZE, rtl8192_rx_isr, skb);

        info = (struct rtl8192_rx_info *) skb->cb;
        info->urb = urb;
        info->dev = dev;
	info->out_pipe = out_pipe;

        urb->transfer_buffer = skb_tail_pointer(skb);
        urb->context = skb;
        skb_queue_tail(&priv->rx_queue, skb);
        err = usb_submit_urb(urb, GFP_ATOMIC);
	if(err && err != EPERM)
		printk("can not submit rxurb, err is %x,URB status is %x\n",err,urb->status);
}

u32
rtl819xusb_rx_command_packet(
	struct net_device *dev,
	struct ieee80211_rx_stats *pstats
	)
{
	u32	status;

	//RT_TRACE(COMP_RECV, DBG_TRACE, ("---> RxCommandPacketHandle819xUsb()\n"));

	status = cmpk_message_handle_rx(dev, pstats);
	if (status)
	{
		DMESG("rxcommandpackethandle819xusb: It is a command packet\n");
	}
	else
	{
		//RT_TRACE(COMP_RECV, DBG_TRACE, ("RxCommandPacketHandle819xUsb: It is not a command packet\n"));
	}

	//RT_TRACE(COMP_RECV, DBG_TRACE, ("<--- RxCommandPacketHandle819xUsb()\n"));
	return status;
}

void rtl8192_data_hard_stop(struct net_device *dev)
{
	//FIXME !!
}


void rtl8192_data_hard_resume(struct net_device *dev)
{
	// FIXME !!
}

/* this function TX data frames when the ieee80211 stack requires this.
 * It checks also if we need to stop the ieee tx queue, eventually do it
 */
void rtl8192_hard_data_xmit(struct sk_buff *skb, struct net_device *dev, int rate)
{
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	int ret;
	unsigned long flags;
	cb_desc *tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
	u8 queue_index = tcb_desc->queue_index;

	/* shall not be referred by command packet */
	assert(queue_index != TXCMD_QUEUE);

	spin_lock_irqsave(&priv->tx_lock,flags);

        memcpy((unsigned char *)(skb->cb),&dev,sizeof(dev));
//	tcb_desc->RATRIndex = 7;
//	tcb_desc->bTxDisableRateFallBack = 1;
//	tcb_desc->bTxUseDriverAssingedRate = 1;
	tcb_desc->bTxEnableFwCalcDur = 1;
	skb_push(skb, priv->ieee80211->tx_headroom);
	ret = priv->ops->rtl819x_tx(dev, skb);

	//priv->ieee80211->stats.tx_bytes+=(skb->len - priv->ieee80211->tx_headroom);
	//priv->ieee80211->stats.tx_packets++;

	spin_unlock_irqrestore(&priv->tx_lock,flags);

//	return ret;
	return;
}

/* This is a rough attempt to TX a frame
 * This is called by the ieee 80211 stack to TX management frames.
 * If the ring is full packet are dropped (for data frame the queue
 * is stopped before this can happen).
 */
int rtl8192_hard_start_xmit(struct sk_buff *skb,struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	int ret;
	unsigned long flags;
        cb_desc *tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
        u8 queue_index = tcb_desc->queue_index;


	spin_lock_irqsave(&priv->tx_lock,flags);

        memcpy((unsigned char *)(skb->cb),&dev,sizeof(dev));
	if(queue_index == TXCMD_QUEUE) {
		skb_push(skb, USB_HWDESC_HEADER_LEN);
		priv->ops->rtl819x_tx_cmd(dev, skb);
		ret = 1;
	        spin_unlock_irqrestore(&priv->tx_lock,flags);
		return ret;
	} else {
		skb_push(skb, priv->ieee80211->tx_headroom);
		ret = priv->ops->rtl819x_tx(dev, skb);
	}

	spin_unlock_irqrestore(&priv->tx_lock,flags);

	return ret;
}


void rtl8192_try_wake_queue(struct net_device *dev, int pri);


static void rtl8192_tx_isr(struct urb *tx_urb)
{
	struct sk_buff *skb = (struct sk_buff*)tx_urb->context;
	struct net_device *dev = NULL;
	struct r8192_priv *priv = NULL;
	cb_desc *tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
	u8  queue_index = tcb_desc->queue_index;
//	bool bToSend0Byte;
//	u16 BufLen = skb->len;

	memcpy(&dev,(struct net_device*)(skb->cb),sizeof(struct net_device*));
	priv = ieee80211_priv(dev);

	if(tcb_desc->queue_index != TXCMD_QUEUE) {
		if(tx_urb->status == 0) {
		//	dev->trans_start = jiffies;
			// As act as station mode, destion shall be  unicast address.
			//priv->ieee80211->stats.tx_bytes+=(skb->len - priv->ieee80211->tx_headroom);
			//priv->ieee80211->stats.tx_packets++;
			priv->stats.txoktotal++;
			priv->ieee80211->LinkDetectInfo.NumTxOkInPeriod++;
			priv->stats.txbytesunicast += (skb->len - priv->ieee80211->tx_headroom);
		} else {
			priv->ieee80211->stats.tx_errors++;
			//priv->stats.txmanageerr++;
			/* TODO */
		}
	}

	/* free skb and tx_urb */
	if(skb != NULL) {
		dev_kfree_skb_any(skb);
		usb_free_urb(tx_urb);
		atomic_dec(&priv->tx_pending[queue_index]);
	}

	{
		//
		// Handle HW Beacon:
		// We had transfer our beacon frame to host controler at this moment.
		//
		//
		// Caution:
		// Handling the wait queue of command packets.
		// For Tx command packets, we must not do TCB fragment because it is not handled right now.
		// We must cut the packets to match the size of TX_CMD_PKT before we send it.
		//
	if (queue_index == MGNT_QUEUE){
        if (priv->ieee80211->ack_tx_to_ieee){
            if (rtl8192_is_tx_queue_empty(dev)){
                priv->ieee80211->ack_tx_to_ieee = 0;
                ieee80211_ps_tx_ack(priv->ieee80211, 1);
            }
        }
    }
		/* Handle MPDU in wait queue. */
		if(queue_index != BEACON_QUEUE) {
			/* Don't send data frame during scanning.*/
			if((skb_queue_len(&priv->ieee80211->skb_waitQ[queue_index]) != 0)&&\
					(!(priv->ieee80211->queue_stop))) {
				if(NULL != (skb = skb_dequeue(&(priv->ieee80211->skb_waitQ[queue_index]))))
					priv->ieee80211->softmac_hard_start_xmit(skb, dev);

				return; //modified by david to avoid further processing AMSDU
			}
		}
	}
}

void rtl8192_beacon_stop(struct net_device *dev)
{
	u8 msr, msrm, msr2;
	struct r8192_priv *priv = ieee80211_priv(dev);

	msr  = read_nic_byte(dev, MSR);
	msrm = msr & MSR_LINK_MASK;
	msr2 = msr & ~MSR_LINK_MASK;

	if(NIC_8192U == priv->card_8192) {
		usb_kill_urb(priv->rx_urb[MAX_RX_URB]);
	}
	if ((msrm == (MSR_LINK_ADHOC<<MSR_LINK_SHIFT) ||
		(msrm == (MSR_LINK_MASTER<<MSR_LINK_SHIFT)))){
		write_nic_byte(dev, MSR, msr2 | MSR_LINK_NONE);
		write_nic_byte(dev, MSR, msr);
	}
}

void rtl8192_config_rate(struct net_device* dev, u16* rate_config)
{
	 struct r8192_priv *priv = ieee80211_priv(dev);
	 struct ieee80211_network *net;
	 u8 i=0, basic_rate = 0;
	 net = & priv->ieee80211->current_network;

	 for (i=0; i<net->rates_len; i++)
	 {
		 basic_rate = net->rates[i]&0x7f;
		 switch(basic_rate)
		 {
			 case MGN_1M:	*rate_config |= RRSR_1M;	break;
			 case MGN_2M:	*rate_config |= RRSR_2M;	break;
			 case MGN_5_5M:	*rate_config |= RRSR_5_5M;	break;
			 case MGN_11M:	*rate_config |= RRSR_11M;	break;
			 case MGN_6M:	*rate_config |= RRSR_6M;	break;
			 case MGN_9M:	*rate_config |= RRSR_9M;	break;
			 case MGN_12M:	*rate_config |= RRSR_12M;	break;
			 case MGN_18M:	*rate_config |= RRSR_18M;	break;
			 case MGN_24M:	*rate_config |= RRSR_24M;	break;
			 case MGN_36M:	*rate_config |= RRSR_36M;	break;
			 case MGN_48M:	*rate_config |= RRSR_48M;	break;
			 case MGN_54M:	*rate_config |= RRSR_54M;	break;
		 }
	 }
	 for (i=0; i<net->rates_ex_len; i++)
	 {
		 basic_rate = net->rates_ex[i]&0x7f;
		 switch(basic_rate)
		 {
			 case MGN_1M:	*rate_config |= RRSR_1M;	break;
			 case MGN_2M:	*rate_config |= RRSR_2M;	break;
			 case MGN_5_5M:	*rate_config |= RRSR_5_5M;	break;
			 case MGN_11M:	*rate_config |= RRSR_11M;	break;
			 case MGN_6M:	*rate_config |= RRSR_6M;	break;
			 case MGN_9M:	*rate_config |= RRSR_9M;	break;
			 case MGN_12M:	*rate_config |= RRSR_12M;	break;
			 case MGN_18M:	*rate_config |= RRSR_18M;	break;
			 case MGN_24M:	*rate_config |= RRSR_24M;	break;
			 case MGN_36M:	*rate_config |= RRSR_36M;	break;
			 case MGN_48M:	*rate_config |= RRSR_48M;	break;
			 case MGN_54M:	*rate_config |= RRSR_54M;	break;
		 }
	 }
}


#define SHORT_SLOT_TIME 9
#define NON_SHORT_SLOT_TIME 20

void rtl8192_update_cap(struct net_device* dev, u16 cap)
{
	//u32 tmp = 0;
	struct r8192_priv *priv = ieee80211_priv(dev);
	struct ieee80211_network *net = &priv->ieee80211->current_network;
	priv->short_preamble = cap & WLAN_CAPABILITY_SHORT_PREAMBLE;

	//LZM MOD 090303 HW_VAR_ACK_PREAMBLE
	if(0)
	{
		u8 tmp = 0;
		tmp = ((priv->nCur40MhzPrimeSC) << 5);
		if (priv->short_preamble)
			tmp |= 0x80;
		write_nic_byte(dev, RRSR+2, tmp);
	}

	if (net->mode & (IEEE_G|IEEE_N_24G))
	{
		u8 slot_time = 0;
		if ((cap & WLAN_CAPABILITY_SHORT_SLOT)&&(!priv->ieee80211->pHTInfo->bCurrentRT2RTLongSlotTime))
		{//short slot time
			slot_time = SHORT_SLOT_TIME;
		}
		else //long slot time
			slot_time = NON_SHORT_SLOT_TIME;
		priv->slot_time = slot_time;
		write_nic_byte(dev, SLOT_TIME, slot_time);
	}

}
void rtl8192_net_update(struct net_device *dev)
{

	struct r8192_priv *priv = ieee80211_priv(dev);
	struct ieee80211_network *net;
	u16 BcnTimeCfg = 0, BcnCW = 6, BcnIFS = 0xf;
	u16 rate_config = 0;
	net = & priv->ieee80211->current_network;

	rtl8192_config_rate(dev, &rate_config);
	priv->basic_rate = rate_config &= 0x15f;

	write_nic_dword(dev,BSSIDR,((u32*)net->bssid)[0]);
	write_nic_word(dev,BSSIDR+4,((u16*)net->bssid)[2]);
	//for(i=0;i<ETH_ALEN;i++)
	//	write_nic_byte(dev,BSSID+i,net->bssid[i]);

	rtl8192_update_msr(dev);
//	rtl8192_update_cap(dev, net->capability);
	if (priv->ieee80211->iw_mode == IW_MODE_ADHOC)
	{
	write_nic_word(dev, ATIMWND, 2);
	write_nic_word(dev, BCN_DMATIME, 1023);
	write_nic_word(dev, BCN_INTERVAL, net->beacon_interval);
//	write_nic_word(dev, BcnIntTime, 100);
	write_nic_word(dev, BCN_DRV_EARLY_INT, 1);
	write_nic_byte(dev, BCN_ERR_THRESH, 100);
		BcnTimeCfg |= (BcnCW<<BCN_TCFG_CW_SHIFT);
	// TODO: BcnIFS may required to be changed on ASIC
	 	BcnTimeCfg |= BcnIFS<<BCN_TCFG_IFS;

	write_nic_word(dev, BCN_TCFG, BcnTimeCfg);
	}



}

//temporary hw beacon is not used any more.
//open it when necessary
#if 1
void rtl819xusb_beacon_tx(struct net_device *dev,u16  tx_rate)
{
}
#endif
inline u8 rtl8192_IsWirelessBMode(u16 rate)
{
	if( ((rate <= 110) && (rate != 60) && (rate != 90)) || (rate == 220) )
		return 1;
	else return 0;
}

u16 N_DBPSOfRate(u16 DataRate);

u16 ComputeTxTime(
	u16		FrameLength,
	u16		DataRate,
	u8		bManagementFrame,
	u8		bShortPreamble
)
{
	u16	FrameTime;
	u16	N_DBPS;
	u16	Ceiling;

	if( rtl8192_IsWirelessBMode(DataRate) )
	{
		if( bManagementFrame || !bShortPreamble || DataRate == 10 )
		{	// long preamble
			FrameTime = (u16)(144+48+(FrameLength*8/(DataRate/10)));
		}
		else
		{	// Short preamble
			FrameTime = (u16)(72+24+(FrameLength*8/(DataRate/10)));
		}
		if( ( FrameLength*8 % (DataRate/10) ) != 0 ) //Get the Ceilling
				FrameTime ++;
	} else {	//802.11g DSSS-OFDM PLCP length field calculation.
		N_DBPS = N_DBPSOfRate(DataRate);
		Ceiling = (16 + 8*FrameLength + 6) / N_DBPS
				+ (((16 + 8*FrameLength + 6) % N_DBPS) ? 1 : 0);
		FrameTime = (u16)(16 + 4 + 4*Ceiling + 6);
	}
	return FrameTime;
}

u16 N_DBPSOfRate(u16 DataRate)
{
	 u16 N_DBPS = 24;

	 switch(DataRate)
	 {
	 case 60:
	  N_DBPS = 24;
	  break;

	 case 90:
	  N_DBPS = 36;
	  break;

	 case 120:
	  N_DBPS = 48;
	  break;

	 case 180:
	  N_DBPS = 72;
	  break;

	 case 240:
	  N_DBPS = 96;
	  break;

	 case 360:
	  N_DBPS = 144;
	  break;

	 case 480:
	  N_DBPS = 192;
	  break;

	 case 540:
	  N_DBPS = 216;
	  break;

	 default:
	  break;
	 }

	 return N_DBPS;
}

void rtl819xU_cmd_isr(struct urb *tx_cmd_urb, struct pt_regs *regs)
{
	usb_free_urb(tx_cmd_urb);
}

unsigned int txqueue2outpipe(struct r8192_priv* priv,unsigned int tx_queue) {

	if(tx_queue >= 9)
	{
		RT_TRACE(COMP_ERR,"%s():Unknown queue ID!!!\n",__FUNCTION__);
		return 0x04;
	}
	return priv->txqueue_to_outpipemap[tx_queue];
}

short rtl8192SU_tx_cmd(struct net_device *dev, struct sk_buff *skb)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	int			status;
	struct urb		*tx_urb;
	unsigned int 		idx_pipe;
	tx_desc_cmd_819x_usb *pdesc = (tx_desc_cmd_819x_usb *)skb->data;
	cb_desc *tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
	u8 queue_index = tcb_desc->queue_index;
	u32			PktSize = 0;

	//printk("\n %s::::::::::::::::::::::queue_index = %d\n",__FUNCTION__, queue_index);
	atomic_inc(&priv->tx_pending[queue_index]);

	tx_urb = usb_alloc_urb(0,GFP_ATOMIC);
	if(!tx_urb){
		dev_kfree_skb(skb);
		return -ENOMEM;
	}

	memset(pdesc, 0, USB_HWDESC_HEADER_LEN);

	/* Tx descriptor ought to be set according to the skb->cb */
	pdesc->LINIP = tcb_desc->bLastIniPkt;
	PktSize = (u16)(skb->len - USB_HWDESC_HEADER_LEN);
	pdesc->PktSize = PktSize;
	//printk("PKTSize = %d %x\n",pdesc->PktSize,pdesc->PktSize);
	//----------------------------------------------------------------------------
	// Fill up USB_OUT_CONTEXT.
	//----------------------------------------------------------------------------
	// Get index to out pipe from specified QueueID.
	idx_pipe = txqueue2outpipe(priv,queue_index);
	//printk("=============>%s queue_index:%d, outpipe:%d\n", __func__,queue_index,priv->RtOutPipes[idx_pipe]);

	usb_fill_bulk_urb(tx_urb,
	                            priv->udev,
	                            usb_sndbulkpipe(priv->udev,priv->RtOutPipes[idx_pipe]),
	                            skb->data,
	                            skb->len,
	                            rtl8192_tx_isr,
	                            skb);

	status = usb_submit_urb(tx_urb, GFP_ATOMIC);
	if (!status){
		return 0;
	}else{
		printk("Error TX CMD URB, error %d",
				status);
		return -1;
	}
}

/*
 * Mapping Software/Hardware descriptor queue id to "Queue Select Field"
 * in TxFwInfo data structure
 * 2006.10.30 by Emily
 *
 * \param QUEUEID       Software Queue
*/
u8 MapHwQueueToFirmwareQueue(u8 QueueID)
{
	u8 QueueSelect = 0x0;       //defualt set to

	switch(QueueID) {
		case BE_QUEUE:
			QueueSelect = QSLT_BE;  //or QSelect = pTcb->priority;
			break;

		case BK_QUEUE:
			QueueSelect = QSLT_BK;  //or QSelect = pTcb->priority;
			break;

		case VO_QUEUE:
			QueueSelect = QSLT_VO;  //or QSelect = pTcb->priority;
			break;

		case VI_QUEUE:
			QueueSelect = QSLT_VI;  //or QSelect = pTcb->priority;
			break;
		case MGNT_QUEUE:
			QueueSelect = QSLT_MGNT;
			break;

		case BEACON_QUEUE:
			QueueSelect = QSLT_BEACON;
			break;

			// TODO: 2006.10.30 mark other queue selection until we verify it is OK
			// TODO: Remove Assertions
//#if (RTL819X_FPGA_VER & RTL819X_FPGA_GUANGAN_070502)
		case TXCMD_QUEUE:
			QueueSelect = QSLT_CMD;
			break;
//#endif
		case HIGH_QUEUE:
			QueueSelect = QSLT_HIGH;
			break;

		default:
			RT_TRACE(COMP_ERR, "TransmitTCB(): Impossible Queue Selection: %d \n", QueueID);
			break;
	}
	return QueueSelect;
}

u8 MRateToHwRate8190Pci(u8 rate)
{
	u8	ret = DESC92S_RATE1M;

	switch(rate)
	{
		// CCK and OFDM non-HT rates
	case MGN_1M:		ret = DESC92S_RATE1M;	break;
	case MGN_2M:		ret = DESC92S_RATE2M;	break;
	case MGN_5_5M:		ret = DESC92S_RATE5_5M;	break;
	case MGN_11M:		ret = DESC92S_RATE11M;	break;
	case MGN_6M:		ret = DESC92S_RATE6M;	break;
	case MGN_9M:		ret = DESC92S_RATE9M;	break;
	case MGN_12M:		ret = DESC92S_RATE12M;	break;
	case MGN_18M:		ret = DESC92S_RATE18M;	break;
	case MGN_24M:		ret = DESC92S_RATE24M;	break;
	case MGN_36M:		ret = DESC92S_RATE36M;	break;
	case MGN_48M:		ret = DESC92S_RATE48M;	break;
	case MGN_54M:		ret = DESC92S_RATE54M;	break;

		// HT rates since here
	case MGN_MCS0:		ret = DESC92S_RATEMCS0;	break;
	case MGN_MCS1:		ret = DESC92S_RATEMCS1;	break;
	case MGN_MCS2:		ret = DESC92S_RATEMCS2;	break;
	case MGN_MCS3:		ret = DESC92S_RATEMCS3;	break;
	case MGN_MCS4:		ret = DESC92S_RATEMCS4;	break;
	case MGN_MCS5:		ret = DESC92S_RATEMCS5;	break;
	case MGN_MCS6:		ret = DESC92S_RATEMCS6;	break;
	case MGN_MCS7:		ret = DESC92S_RATEMCS7;	break;
	case MGN_MCS8:		ret = DESC92S_RATEMCS8;	break;
	case MGN_MCS9:		ret = DESC92S_RATEMCS9;	break;
	case MGN_MCS10:	ret = DESC92S_RATEMCS10;	break;
	case MGN_MCS11:	ret = DESC92S_RATEMCS11;	break;
	case MGN_MCS12:	ret = DESC92S_RATEMCS12;	break;
	case MGN_MCS13:	ret = DESC92S_RATEMCS13;	break;
	case MGN_MCS14:	ret = DESC92S_RATEMCS14;	break;
	case MGN_MCS15:	ret = DESC92S_RATEMCS15;	break;

	// Set the highest SG rate
	case MGN_MCS0_SG:
	case MGN_MCS1_SG:
	case MGN_MCS2_SG:
	case MGN_MCS3_SG:
	case MGN_MCS4_SG:
	case MGN_MCS5_SG:
	case MGN_MCS6_SG:
	case MGN_MCS7_SG:
	case MGN_MCS8_SG:
	case MGN_MCS9_SG:
	case MGN_MCS10_SG:
	case MGN_MCS11_SG:
	case MGN_MCS12_SG:
	case MGN_MCS13_SG:
	case MGN_MCS14_SG:
	case MGN_MCS15_SG:
	{
		ret = DESC92S_RATEMCS15_SG;
		break;
	}

	default:		break;
	}
	return ret;
}

u8 QueryIsShort(u8 TxHT, u8 TxRate, cb_desc *tcb_desc)
{
	u8   tmp_Short;

	tmp_Short = (TxHT==1)?((tcb_desc->bUseShortGI)?1:0):((tcb_desc->bUseShortPreamble)?1:0);

	if(TxHT==1 && TxRate != DESC90_RATEMCS15)
		tmp_Short = 0;

	return tmp_Short;
}

static void tx_zero_isr(struct urb *tx_urb)
{
	return;
}


/*
 * The tx procedure is just as following,  skb->cb will contain all the following
 *information: * priority, morefrag, rate, &dev.
 * */
 //	<Note> Buffer format for 8192S Usb bulk out:
//
//  --------------------------------------------------
//  | 8192S Usb Tx Desc | 802_11_MAC_header |    data          |
//  --------------------------------------------------
//  |  32 bytes           	  |       24 bytes             |0-2318 bytes|
//  --------------------------------------------------
//  |<------------ BufferLen ------------------------->|

short rtl8192SU_tx(struct net_device *dev, struct sk_buff* skb)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	cb_desc *tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
	tx_desc_819x_usb *tx_desc = (tx_desc_819x_usb *)skb->data;
	//tx_fwinfo_819x_usb *tx_fwinfo = (tx_fwinfo_819x_usb *)(skb->data + USB_HWDESC_HEADER_LEN);//92su del
	struct usb_device *udev = priv->udev;
	int pend;
	int status;
	struct urb *tx_urb = NULL, *tx_urb_zero = NULL;
	//int urb_len;
	unsigned int idx_pipe;
	u16		MPDUOverhead = 0;
 	//RT_DEBUG_DATA(COMP_SEND, tcb_desc, sizeof(cb_desc));

	pend = atomic_read(&priv->tx_pending[tcb_desc->queue_index]);
	/* we are locked here so the two atomic_read and inc are executed
	 * without interleaves  * !!! For debug purpose 	  */
	if( pend > MAX_TX_URB){
		switch (tcb_desc->queue_index) {
			case VO_PRIORITY:
				priv->stats.txvodrop++;
				break;
			case VI_PRIORITY:
				priv->stats.txvidrop++;
				break;
			case BE_PRIORITY:
				priv->stats.txbedrop++;
				break;
			default://BK_PRIORITY
				priv->stats.txbkdrop++;
				break;
		}
		printk("To discard skb packet!\n");
		dev_kfree_skb_any(skb);
		return -1;
	}

	tx_urb = usb_alloc_urb(0,GFP_ATOMIC);
	if(!tx_urb){
		dev_kfree_skb_any(skb);
		return -ENOMEM;
	}

	memset(tx_desc, 0, sizeof(tx_desc_819x_usb));


		tx_desc->NonQos = (IsQoSDataFrame(skb->data)==TRUE)? 0:1;

	/* Fill Tx descriptor */
	//memset(tx_fwinfo,0,sizeof(tx_fwinfo_819x_usb));

	// This part can just fill to the first descriptor of the frame.
	/* DWORD 0 */
	tx_desc->TxHT = (tcb_desc->data_rate&0x80)?1:0;


	tx_desc->TxRate = MRateToHwRate8190Pci(tcb_desc->data_rate);
	//tx_desc->EnableCPUDur = tcb_desc->bTxEnableFwCalcDur;
	tx_desc->TxShort = QueryIsShort(tx_desc->TxHT, tx_desc->TxRate, tcb_desc);


	// Aggregation related
	if(tcb_desc->bAMPDUEnable) {//AMPDU enabled
		tx_desc->AllowAggregation = 1;
		/* DWORD 1 */
		//tx_fwinfo->RxMF = tcb_desc->ampdu_factor;
		//tx_fwinfo->RxAMD = tcb_desc->ampdu_density&0x07;//ampdudensity
	} else {
		tx_desc->AllowAggregation = 0;
		/* DWORD 1 */
		//tx_fwinfo->RxMF = 0;
		//tx_fwinfo->RxAMD = 0;
	}

	//
	// <Roger_Notes> For AMPDU case, we must insert SSN into TX_DESC,
	// FW according as this SSN to do necessary packet retry.
	// 2008.06.06.
	//
	{
		u8	*pSeq;
		u16	Temp;
		//pSeq = (u8 *)(VirtualAddress+USB_HWDESC_HEADER_LEN + FRAME_OFFSET_SEQUENCE);
		pSeq = (u8 *)(skb->data+USB_HWDESC_HEADER_LEN + 22);
		Temp = pSeq[0];
		Temp <<= 12;
		Temp |= (*(u16 *)pSeq)>>4;
		tx_desc->Seq = Temp;
	}

	/* Protection mode related */
	tx_desc->RTSEn = (tcb_desc->bRTSEnable)?1:0;
	tx_desc->CTS2Self = (tcb_desc->bCTSEnable)?1:0;
	tx_desc->RTSSTBC = (tcb_desc->bRTSSTBC)?1:0;
	tx_desc->RTSHT = (tcb_desc->rts_rate&0x80)?1:0;
	tx_desc->RTSRate =  MRateToHwRate8190Pci((u8)tcb_desc->rts_rate);
	tx_desc->RTSSubcarrier = (tx_desc->RTSHT==0)?(tcb_desc->RTSSC):0;
	tx_desc->RTSBW = (tx_desc->RTSHT==1)?((tcb_desc->bRTSBW)?1:0):0;
	tx_desc->RTSShort = (tx_desc->RTSHT==0)?(tcb_desc->bRTSUseShortPreamble?1:0):\
				(tcb_desc->bRTSUseShortGI?1:0);
	//LZM 090219
	tx_desc->DisRTSFB = 0;
	tx_desc->RTSRateFBLmt = 0xf;

	// <Roger_EXP> 2008.09.22. We disable RTS rate fallback temporarily.
	//tx_desc->DisRTSFB = 0x01;

	/* Set Bandwidth and sub-channel settings. */
	if(priv->CurrentChannelBW == HT_CHANNEL_WIDTH_20_40)
	{
		if(tcb_desc->bPacketBW) {
			tx_desc->TxBandwidth = 1;
			tx_desc->TxSubCarrier = 0;    //By SD3's Jerry suggestion, use duplicated mode
		} else {
			tx_desc->TxBandwidth = 0;
			tx_desc->TxSubCarrier = priv->nCur40MhzPrimeSC;
		}
	} else {
		tx_desc->TxBandwidth = 0;
		tx_desc->TxSubCarrier = 0;
	}


	//memset(tx_desc, 0, sizeof(tx_desc_819x_usb));
	/* DWORD 0 */
        tx_desc->LINIP = 0;
        //tx_desc->CmdInit = 1; //92su del
        tx_desc->Offset =  USB_HWDESC_HEADER_LEN;

	{
		tx_desc->PktSize = (skb->len - USB_HWDESC_HEADER_LEN) & 0xffff;
	}

	/*DWORD 1*/
	//tx_desc->SecCAMID= 0;//92su del
	tx_desc->RaBRSRID= tcb_desc->RATRIndex;
//#ifdef RTL8192S_PREPARE_FOR_NORMAL_RELEASE

	{
		MPDUOverhead = 0;
		//tx_desc->NoEnc = 1;//92su del
	}

	tx_desc->SecType = 0x0;

		if (tcb_desc->bHwSec)
			{
				switch (priv->ieee80211->pairwise_key_type)
				{
					case KEY_TYPE_WEP40:
					case KEY_TYPE_WEP104:
						 tx_desc->SecType = 0x1;
						 //tx_desc->NoEnc = 0;//92su del
						 break;
					case KEY_TYPE_TKIP:
						 tx_desc->SecType = 0x2;
						 //tx_desc->NoEnc = 0;//92su del
						 break;
					case KEY_TYPE_CCMP:
						 tx_desc->SecType = 0x3;
						 //tx_desc->NoEnc = 0;//92su del
						 break;
					case KEY_TYPE_NA:
						 tx_desc->SecType = 0x0;
						 //tx_desc->NoEnc = 1;//92su del
						 break;
					default:
						 tx_desc->SecType = 0x0;
						 //tx_desc->NoEnc = 1;//92su del
						 break;
				}
			}

	//tx_desc->TxFWInfoSize =  sizeof(tx_fwinfo_819x_usb);//92su del


	tx_desc->USERATE = tcb_desc->bTxUseDriverAssingedRate;
	tx_desc->DISFB = tcb_desc->bTxDisableRateFallBack;
	tx_desc->DataRateFBLmt = 0x1F;// Alwasy enable all rate fallback range

	tx_desc->QueueSelect = MapHwQueueToFirmwareQueue(tcb_desc->queue_index);


        /* Fill fields that are required to be initialized in all of the descriptors */
        //DWORD 0
        tx_desc->FirstSeg = 1;
        tx_desc->LastSeg = 1;
        tx_desc->OWN = 1;

	{
		//DWORD 2
		//tx_desc->TxBufferSize = (u32)(skb->len - USB_HWDESC_HEADER_LEN);
		tx_desc->TxBufferSize = (u32)(skb->len);//92su mod FIXLZM
	}

	/* Get index to out pipe from specified QueueID */
	idx_pipe = txqueue2outpipe(priv,tcb_desc->queue_index);
	//printk("=============>%s queue_index:%d, outpipe:%d\n", __func__,tcb_desc->queue_index,priv->RtOutPipes[idx_pipe]);

	//RT_DEBUG_DATA(COMP_SEND,tx_fwinfo,sizeof(tx_fwinfo_819x_usb));
	//RT_DEBUG_DATA(COMP_SEND,tx_desc,sizeof(tx_desc_819x_usb));

	/* To submit bulk urb */
	usb_fill_bulk_urb(tx_urb,
				    udev,
				    usb_sndbulkpipe(udev,priv->RtOutPipes[idx_pipe]),
				    skb->data,
				    skb->len, rtl8192_tx_isr, skb);

	status = usb_submit_urb(tx_urb, GFP_ATOMIC);
	if (!status){
//we need to send 0 byte packet whenever 512N bytes/64N(HIGN SPEED/NORMAL SPEED) bytes packet has been transmitted. Otherwise, it will be halt to wait for another packet. WB. 2008.08.27
		bool bSend0Byte = false;
		u8 zero = 0;
		if(udev->speed == USB_SPEED_HIGH)
		{
			if (skb->len > 0 && skb->len % 512 == 0)
				bSend0Byte = true;
		}
		else
		{
			if (skb->len > 0 && skb->len % 64 == 0)
				bSend0Byte = true;
		}
		if (bSend0Byte)
		{
#if 1
			tx_urb_zero = usb_alloc_urb(0,GFP_ATOMIC);
			if(!tx_urb_zero){
				RT_TRACE(COMP_ERR, "can't alloc urb for zero byte\n");
				return -ENOMEM;
			}
			usb_fill_bulk_urb(tx_urb_zero,udev,
					usb_sndbulkpipe(udev,idx_pipe), &zero,
					0, tx_zero_isr, dev);
			status = usb_submit_urb(tx_urb_zero, GFP_ATOMIC);
			if (status){
			RT_TRACE(COMP_ERR, "Error TX URB for zero byte %d, error %d", atomic_read(&priv->tx_pending[tcb_desc->queue_index]), status);
			return -1;
			}
#endif
		}
		dev->trans_start = jiffies;
		atomic_inc(&priv->tx_pending[tcb_desc->queue_index]);
		return 0;
	}else{
		RT_TRACE(COMP_ERR, "Error TX URB %d, error %d", atomic_read(&priv->tx_pending[tcb_desc->queue_index]),
				status);
		return -1;
	}
}

void rtl8192SU_net_update(struct net_device *dev)
{

	struct r8192_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device* ieee = priv->ieee80211;
	struct ieee80211_network *net = &priv->ieee80211->current_network;
	//u16 BcnTimeCfg = 0, BcnCW = 6, BcnIFS = 0xf;
	u16 rate_config = 0;
	u32 regTmp = 0;
	u8 rateIndex = 0;
	u8	retrylimit = 0x30;
	u16 cap = net->capability;

	priv->short_preamble = cap & WLAN_CAPABILITY_SHORT_PREAMBLE;

//HW_VAR_BASIC_RATE
	//update Basic rate: RR, BRSR
	rtl8192_config_rate(dev, &rate_config);	//HalSetBrateCfg

	priv->basic_rate = rate_config  = rate_config & 0x15f;

	// Set RRSR rate table.
	write_nic_byte(dev, RRSR, rate_config&0xff);
	write_nic_byte(dev, RRSR+1, (rate_config>>8)&0xff);

	// Set RTS initial rate
	while(rate_config > 0x1)
	{
		rate_config = (rate_config>> 1);
		rateIndex++;
	}
	write_nic_byte(dev, INIRTSMCS_SEL, rateIndex);
//HW_VAR_BASIC_RATE

	//set ack preample
	regTmp = (priv->nCur40MhzPrimeSC) << 5;
	if (priv->short_preamble)
		regTmp |= 0x80;
	write_nic_byte(dev, RRSR+2, regTmp);

	write_nic_dword(dev,BSSIDR,((u32*)net->bssid)[0]);
	write_nic_word(dev,BSSIDR+4,((u16*)net->bssid)[2]);

	write_nic_word(dev, BCN_INTERVAL, net->beacon_interval);
	//2008.10.24 added by tynli for beacon changed.
	PHY_SetBeaconHwReg( dev, net->beacon_interval);

	rtl8192_update_cap(dev, cap);

	if (ieee->iw_mode == IW_MODE_ADHOC){
		retrylimit = 7;
		//we should enable ibss interrupt here, but disable it temporarily
		if (0){
			priv->irq_mask |= (IMR_BcnInt | IMR_BcnInt | IMR_TBDOK | IMR_TBDER);
			//rtl8192_irq_disable(dev);
			//rtl8192_irq_enable(dev);
		}
	}
	else{
		if (0){
			priv->irq_mask &= ~(IMR_BcnInt | IMR_BcnInt | IMR_TBDOK | IMR_TBDER);
			//rtl8192_irq_disable(dev);
			//rtl8192_irq_enable(dev);
		}
	}

	priv->ShortRetryLimit = priv->LongRetryLimit = retrylimit;

	write_nic_word(dev, 	RETRY_LIMIT,
				retrylimit << RETRY_LIMIT_SHORT_SHIFT | \
				retrylimit << RETRY_LIMIT_LONG_SHIFT);
}

void rtl8192SU_update_ratr_table(struct net_device* dev)
{
		struct r8192_priv* priv = ieee80211_priv(dev);
	struct ieee80211_device* ieee = priv->ieee80211;
	u8* pMcsRate = ieee->dot11HTOperationalRateSet;
	//struct ieee80211_network *net = &ieee->current_network;
	u32 ratr_value = 0;

	u8 rate_index = 0;
	int WirelessMode = ieee->mode;
	u8 MimoPs = ieee->pHTInfo->PeerMimoPs;

	u8 bNMode = 0;

	rtl8192_config_rate(dev, (u16*)(&ratr_value));
	ratr_value |= (*(u16*)(pMcsRate)) << 12;

	//switch (ieee->mode)
	switch (WirelessMode)
	{
		case IEEE_A:
			ratr_value &= 0x00000FF0;
			break;
		case IEEE_B:
			ratr_value &= 0x0000000D;
			break;
		case IEEE_G:
			ratr_value &= 0x00000FF5;
			break;
		case IEEE_N_24G:
		case IEEE_N_5G:
		{
			bNMode = 1;

			if (MimoPs == 0) //MIMO_PS_STATIC
					{
				ratr_value &= 0x0007F005;
			}
			else
			{	// MCS rate only => for 11N mode.
				u32	ratr_mask;

				// 1T2R or 1T1R, Spatial Stream 2 should be disabled
				if (	priv->rf_type == RF_1T2R ||
					priv->rf_type == RF_1T1R ||
					(ieee->pHTInfo->IOTAction & HT_IOT_ACT_DISABLE_TX_2SS) )
						ratr_mask = 0x000ff005;
					else
						ratr_mask = 0x0f0ff005;

				if((ieee->pHTInfo->bCurTxBW40MHz) &&
				    !(ieee->pHTInfo->IOTAction & HT_IOT_ACT_DISABLE_TX_40_MHZ))
					ratr_mask |= 0x00000010; // Set 6MBps

				// Select rates for rate adaptive mechanism.
					ratr_value &= ratr_mask;
					}
			}
			break;
		default:
			if(0)
			{
				if(priv->rf_type == RF_1T2R)	// 1T2R, Spatial Stream 2 should be disabled
				{
				ratr_value &= 0x000ff0f5;
				}
				else
				{
				ratr_value &= 0x0f0ff0f5;
				}
			}
			//printk("====>%s(), mode is not correct:%x\n", __FUNCTION__, ieee->mode);
			break;
	}

	ratr_value &= 0x0FFFFFFF;

	// Get MAX MCS available.
	if (   (bNMode && ((ieee->pHTInfo->IOTAction & HT_IOT_ACT_DISABLE_SHORT_GI)==0)) &&
		((ieee->pHTInfo->bCurBW40MHz && ieee->pHTInfo->bCurShortGI40MHz) ||
	        (!ieee->pHTInfo->bCurBW40MHz && ieee->pHTInfo->bCurShortGI20MHz)))
	{
		u8 shortGI_rate = 0;
		u32 tmp_ratr_value = 0;
		ratr_value |= 0x10000000;//???
		tmp_ratr_value = (ratr_value>>12);
		for(shortGI_rate=15; shortGI_rate>0; shortGI_rate--)
		{
			if((1<<shortGI_rate) & tmp_ratr_value)
				break;
		}
		shortGI_rate = (shortGI_rate<<12)|(shortGI_rate<<8)|(shortGI_rate<<4)|(shortGI_rate);
		write_nic_byte(dev, SG_RATE, shortGI_rate);
		//printk("==>SG_RATE:%x\n", read_nic_byte(dev, SG_RATE));
	}
	write_nic_dword(dev, ARFR0+rate_index*4, ratr_value);
	printk("=============>ARFR0+rate_index*4:%#x\n", ratr_value);

	//2 UFWP
	if (ratr_value & 0xfffff000){
		//printk("===>set to N mode\n");
		HalSetFwCmd8192S(dev, FW_CMD_RA_REFRESH_N);
	}
	else	{
		//printk("===>set to B/G mode\n");
		HalSetFwCmd8192S(dev, FW_CMD_RA_REFRESH_BG);
	}
}

void rtl8192SU_link_change(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device* ieee = priv->ieee80211;
	//unsigned long flags;
	u32 reg = 0;

	printk("=====>%s 1\n", __func__);
	reg = read_nic_dword(dev, RCR);

	if (ieee->state == IEEE80211_LINKED)
	{

		rtl8192SU_net_update(dev);
		rtl8192SU_update_ratr_table(dev);
		ieee->SetFwCmdHandler(dev, FW_CMD_HIGH_PWR_ENABLE);
		priv->ReceiveConfig = reg |= RCR_CBSSID;

	}else{
		priv->ReceiveConfig = reg &= ~RCR_CBSSID;

	}

	write_nic_dword(dev, RCR, reg);
	rtl8192_update_msr(dev);

	printk("<=====%s 2\n", __func__);
}

static struct ieee80211_qos_parameters def_qos_parameters = {
        {3,3,3,3},/* cw_min */
        {7,7,7,7},/* cw_max */
        {2,2,2,2},/* aifs */
        {0,0,0,0},/* flags */
        {0,0,0,0} /* tx_op_limit */
};


void rtl8192_update_beacon(struct work_struct * work)
{
        struct r8192_priv *priv = container_of(work, struct r8192_priv, update_beacon_wq.work);
        struct net_device *dev = priv->ieee80211->dev;
 	struct ieee80211_device* ieee = priv->ieee80211;
	struct ieee80211_network* net = &ieee->current_network;

	if (ieee->pHTInfo->bCurrentHTSupport)
		HTUpdateSelfAndPeerSetting(ieee, net);
	ieee->pHTInfo->bCurrentRT2RTLongSlotTime = net->bssht.bdRT2RTLongSlotTime;
	// Joseph test for turbo mode with AP
	ieee->pHTInfo->RT2RT_HT_Mode = net->bssht.RT2RT_HT_Mode;
	rtl8192_update_cap(dev, net->capability);
}
/*
* background support to run QoS activate functionality
*/
int WDCAPARA_ADD[] = {EDCAPARA_BE,EDCAPARA_BK,EDCAPARA_VI,EDCAPARA_VO};

void rtl8192_qos_activate(struct work_struct * work)
{
        struct r8192_priv *priv = container_of(work, struct r8192_priv, qos_activate);
        struct net_device *dev = priv->ieee80211->dev;
        struct ieee80211_qos_parameters *qos_parameters = &priv->ieee80211->current_network.qos_data.parameters;
        u8 mode = priv->ieee80211->current_network.mode;
        //u32 size = sizeof(struct ieee80211_qos_parameters);
	u8  u1bAIFS;
	u32 u4bAcParam;
        int i;

        if (priv == NULL)
                return;

       mutex_lock(&priv->mutex);

        if(priv->ieee80211->state != IEEE80211_LINKED)
		goto success;
	RT_TRACE(COMP_QOS,"qos active process with associate response received\n");
	/* It better set slot time at first */
	/* For we just support b/g mode at present, let the slot time at 9/20 selection */
	/* update the ac parameter to related registers */
	for(i = 0; i <  QOS_QUEUE_NUM; i++) {
		//Mode G/A: slotTimeTimer = 9; Mode B: 20
		u1bAIFS = qos_parameters->aifs[i] * ((mode&(IEEE_G|IEEE_N_24G)) ?9:20) + aSifsTime;
		u4bAcParam = ((((u32)(qos_parameters->tx_op_limit[i]))<< AC_PARAM_TXOP_LIMIT_OFFSET)|
				(((u32)(qos_parameters->cw_max[i]))<< AC_PARAM_ECW_MAX_OFFSET)|
				(((u32)(qos_parameters->cw_min[i]))<< AC_PARAM_ECW_MIN_OFFSET)|
				((u32)u1bAIFS << AC_PARAM_AIFS_OFFSET));

		write_nic_dword(dev, WDCAPARA_ADD[i], u4bAcParam);
		//write_nic_dword(dev, WDCAPARA_ADD[i], 0x005e4322);
	}

success:
       mutex_unlock(&priv->mutex);
}

static int rtl8192_qos_handle_probe_response(struct r8192_priv *priv,
		int active_network,
		struct ieee80211_network *network)
{
	int ret = 0;
	u32 size = sizeof(struct ieee80211_qos_parameters);

	if(priv->ieee80211->state !=IEEE80211_LINKED)
                return ret;

        if ((priv->ieee80211->iw_mode != IW_MODE_INFRA))
                return ret;

	if (network->flags & NETWORK_HAS_QOS_MASK) {
		if (active_network &&
				(network->flags & NETWORK_HAS_QOS_PARAMETERS))
			network->qos_data.active = network->qos_data.supported;

		if ((network->qos_data.active == 1) && (active_network == 1) &&
				(network->flags & NETWORK_HAS_QOS_PARAMETERS) &&
				(network->qos_data.old_param_count !=
				 network->qos_data.param_count)) {
			network->qos_data.old_param_count =
				network->qos_data.param_count;
			queue_work(priv->priv_wq, &priv->qos_activate);
			RT_TRACE (COMP_QOS, "QoS parameters change call "
					"qos_activate\n");
		}
	} else {
		memcpy(&priv->ieee80211->current_network.qos_data.parameters,\
		       &def_qos_parameters, size);

		if ((network->qos_data.active == 1) && (active_network == 1)) {
			queue_work(priv->priv_wq, &priv->qos_activate);
			RT_TRACE(COMP_QOS, "QoS was disabled call qos_activate \n");
		}
		network->qos_data.active = 0;
		network->qos_data.supported = 0;
	}

	return 0;
}

/* handle manage frame frame beacon and probe response */
static int rtl8192_handle_beacon(struct net_device * dev,
				 struct ieee80211_probe_response *beacon,
				 struct ieee80211_network *network)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	rtl8192_qos_handle_probe_response(priv,1,network);
	queue_delayed_work(priv->priv_wq, &priv->update_beacon_wq, 0);

	return 0;

}

/*
* handling the beaconing responses. if we get different QoS setting
* off the network from the associated setting, adjust the QoS
* setting
*/
static int rtl8192_qos_association_resp(struct r8192_priv *priv,
                                    struct ieee80211_network *network)
{
        int ret = 0;
        unsigned long flags;
        u32 size = sizeof(struct ieee80211_qos_parameters);
        int set_qos_param = 0;

        if ((priv == NULL) || (network == NULL))
                return ret;

	if(priv->ieee80211->state !=IEEE80211_LINKED)
                return ret;

        if ((priv->ieee80211->iw_mode != IW_MODE_INFRA))
                return ret;

        spin_lock_irqsave(&priv->ieee80211->lock, flags);
	if(network->flags & NETWORK_HAS_QOS_PARAMETERS) {
		memcpy(&priv->ieee80211->current_network.qos_data.parameters,\
			 &network->qos_data.parameters,\
			sizeof(struct ieee80211_qos_parameters));
		priv->ieee80211->current_network.qos_data.active = 1;
		 {
                        set_qos_param = 1;
			/* update qos parameter for current network */
			priv->ieee80211->current_network.qos_data.old_param_count = \
				 priv->ieee80211->current_network.qos_data.param_count;
			priv->ieee80211->current_network.qos_data.param_count = \
			     	 network->qos_data.param_count;
		}
        } else {
		memcpy(&priv->ieee80211->current_network.qos_data.parameters,\
		       &def_qos_parameters, size);
		priv->ieee80211->current_network.qos_data.active = 0;
		priv->ieee80211->current_network.qos_data.supported = 0;
                set_qos_param = 1;
        }

        spin_unlock_irqrestore(&priv->ieee80211->lock, flags);

	RT_TRACE(COMP_QOS, "%s: network->flags = %d,%d\n",__FUNCTION__,network->flags ,priv->ieee80211->current_network.qos_data.active);
	if (set_qos_param == 1)
		queue_work(priv->priv_wq, &priv->qos_activate);

        return ret;
}


static int rtl8192_handle_assoc_response(struct net_device *dev,
                                     struct ieee80211_assoc_response_frame *resp,
                                     struct ieee80211_network *network)
{
        struct r8192_priv *priv = ieee80211_priv(dev);
        rtl8192_qos_association_resp(priv, network);
        return 0;
}


void rtl8192_update_ratr_table(struct net_device* dev)
	//	POCTET_STRING	posLegacyRate,
	//	u8*			pMcsRate)
	//	PRT_WLAN_STA	pEntry)
{
	struct r8192_priv* priv = ieee80211_priv(dev);
	struct ieee80211_device* ieee = priv->ieee80211;
	u8* pMcsRate = ieee->dot11HTOperationalRateSet;
	//struct ieee80211_network *net = &ieee->current_network;
	u32 ratr_value = 0;
	u8 rate_index = 0;
	rtl8192_config_rate(dev, (u16*)(&ratr_value));
	ratr_value |= (*(u16*)(pMcsRate)) << 12;
//	switch (net->mode)
	switch (ieee->mode)
	{
		case IEEE_A:
			ratr_value &= 0x00000FF0;
			break;
		case IEEE_B:
			ratr_value &= 0x0000000F;
			break;
		case IEEE_G:
			ratr_value &= 0x00000FF7;
			break;
		case IEEE_N_24G:
		case IEEE_N_5G:
			if (ieee->pHTInfo->PeerMimoPs == 0) //MIMO_PS_STATIC
				ratr_value &= 0x0007F007;
			else{
				if (priv->rf_type == RF_1T2R)
					ratr_value &= 0x000FF007;
				else
					ratr_value &= 0x0F81F007;
			}
			break;
		default:
			break;
	}
	ratr_value &= 0x0FFFFFFF;
	if(ieee->pHTInfo->bCurTxBW40MHz && ieee->pHTInfo->bCurShortGI40MHz){
		ratr_value |= 0x80000000;
	}else if(!ieee->pHTInfo->bCurTxBW40MHz && ieee->pHTInfo->bCurShortGI20MHz){
		ratr_value |= 0x80000000;
	}
	write_nic_dword(dev, RATR0+rate_index*4, ratr_value);
	write_nic_byte(dev, UFWP, 1);
}

static u8 ccmp_ie[4] = {0x00,0x50,0xf2,0x04};
static u8 ccmp_rsn_ie[4] = {0x00, 0x0f, 0xac, 0x04};
bool GetNmodeSupportBySecCfg8192(struct net_device*dev)
{
#if 1
	struct r8192_priv* priv = ieee80211_priv(dev);
	struct ieee80211_device* ieee = priv->ieee80211;
	struct ieee80211_network * network = &ieee->current_network;
        int wpa_ie_len= ieee->wpa_ie_len;
        struct ieee80211_crypt_data* crypt;
        int encrypt;
	return TRUE;

        crypt = ieee->crypt[ieee->tx_keyidx];
	//we use connecting AP's capability instead of only security config on our driver to distinguish whether it should use N mode or G mode
        encrypt = (network->capability & WLAN_CAPABILITY_PRIVACY) || (ieee->host_encrypt && crypt && crypt->ops && (0 == strcmp(crypt->ops->name,"WEP")));

	/* simply judge  */
	if(encrypt && (wpa_ie_len == 0)) {
		/* wep encryption, no N mode setting */
		return false;
//	} else if((wpa_ie_len != 0)&&(memcmp(&(ieee->wpa_ie[14]),ccmp_ie,4))) {
	} else if((wpa_ie_len != 0)) {
		/* parse pairwise key type */
		//if((pairwisekey = WEP40)||(pairwisekey = WEP104)||(pairwisekey = TKIP))
		if (((ieee->wpa_ie[0] == 0xdd) && (!memcmp(&(ieee->wpa_ie[14]),ccmp_ie,4))) || ((ieee->wpa_ie[0] == 0x30) && (!memcmp(&ieee->wpa_ie[10],ccmp_rsn_ie, 4))))
			return true;
		else
			return false;
	} else {
		return true;
	}

	return true;
#endif
}

bool GetHalfNmodeSupportByAPs819xUsb(struct net_device* dev)
{
	bool			Reval;
	struct r8192_priv* priv = ieee80211_priv(dev);
	struct ieee80211_device* ieee = priv->ieee80211;

// 	Added by Roger, 2008.08.29.
	return false;

	if(ieee->bHalfWirelessN24GMode == true)
		Reval = true;
	else
		Reval =  false;

	return Reval;
}

void rtl8192_refresh_supportrate(struct r8192_priv* priv)
{
	struct ieee80211_device* ieee = priv->ieee80211;
	//we donot consider set support rate for ABG mode, only HT MCS rate is set here.
	if (ieee->mode == WIRELESS_MODE_N_24G || ieee->mode == WIRELESS_MODE_N_5G)
	{
		memcpy(ieee->Regdot11HTOperationalRateSet, ieee->RegHTSuppRateSet, 16);
		//RT_DEBUG_DATA(COMP_INIT, ieee->RegHTSuppRateSet, 16);
		//RT_DEBUG_DATA(COMP_INIT, ieee->Regdot11HTOperationalRateSet, 16);
	}
	else
		memset(ieee->Regdot11HTOperationalRateSet, 0, 16);
	return;
}

u8 rtl8192_getSupportedWireleeMode(struct net_device*dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8 ret = 0;
	switch(priv->rf_chip)
	{
		case RF_8225:
		case RF_8256:
		case RF_PSEUDO_11N:
		case RF_6052:
			ret = (WIRELESS_MODE_N_24G|WIRELESS_MODE_G|WIRELESS_MODE_B);
			break;
		case RF_8258:
			ret = (WIRELESS_MODE_A|WIRELESS_MODE_N_5G);
			break;
		default:
			ret = WIRELESS_MODE_B;
			break;
	}
	return ret;
}
void rtl8192_SetWirelessMode(struct net_device* dev, u8 wireless_mode)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8 bSupportMode = rtl8192_getSupportedWireleeMode(dev);

#if 1
	if ((wireless_mode == WIRELESS_MODE_AUTO) || ((wireless_mode&bSupportMode)==0))
	{
		if(bSupportMode & WIRELESS_MODE_N_24G)
		{
			wireless_mode = WIRELESS_MODE_N_24G;
		}
		else if(bSupportMode & WIRELESS_MODE_N_5G)
		{
			wireless_mode = WIRELESS_MODE_N_5G;
		}
		else if((bSupportMode & WIRELESS_MODE_A))
		{
			wireless_mode = WIRELESS_MODE_A;
		}
		else if((bSupportMode & WIRELESS_MODE_G))
		{
			wireless_mode = WIRELESS_MODE_G;
		}
		else if((bSupportMode & WIRELESS_MODE_B))
		{
			wireless_mode = WIRELESS_MODE_B;
		}
		else{
			RT_TRACE(COMP_ERR, "%s(), No valid wireless mode supported, SupportedWirelessMode(%x)!!!\n", __FUNCTION__,bSupportMode);
			wireless_mode = WIRELESS_MODE_B;
		}
	}
#ifdef TO_DO_LIST //// TODO: this function doesn't work well at this time, we shoud wait for FPGA
	ActUpdateChannelAccessSetting( pAdapter, pHalData->CurrentWirelessMode, &pAdapter->MgntInfo.Info8185.ChannelAccessSetting );
#endif
	//LZM 090306 usb crash here, mark it temp
	//write_nic_word(dev, SIFS_OFDM, 0x0e0e);
	priv->ieee80211->mode = wireless_mode;

	if ((wireless_mode == WIRELESS_MODE_N_24G) ||  (wireless_mode == WIRELESS_MODE_N_5G))
		priv->ieee80211->pHTInfo->bEnableHT = 1;
	else
		priv->ieee80211->pHTInfo->bEnableHT = 0;
	RT_TRACE(COMP_INIT, "Current Wireless Mode is %x\n", wireless_mode);
	rtl8192_refresh_supportrate(priv);
#endif

}


short rtl8192_is_tx_queue_empty(struct net_device *dev)
{
	int i=0;
	struct r8192_priv *priv = ieee80211_priv(dev);
	//struct ieee80211_device* ieee = priv->ieee80211;
	for (i=0; i<=MGNT_QUEUE; i++)
	{
		if ((i== TXCMD_QUEUE) || (i == HCCA_QUEUE) )
			continue;
		if (atomic_read(&priv->tx_pending[i]))
		{
			printk("===>tx queue is not empty:%d, %d\n", i, atomic_read(&priv->tx_pending[i]));
			return 0;
		}
	}
	return 1;
}

void rtl8192_hw_sleep_down(struct net_device *dev)
{
	RT_TRACE(COMP_POWER, "%s()============>come to sleep down\n", __FUNCTION__);
#ifdef TODO
//	MgntActSet_RF_State(dev, eRfSleep, RF_CHANGE_BY_PS);
#endif
}

void rtl8192_hw_sleep_wq (struct work_struct *work)
{
//      struct r8180_priv *priv = container_of(work, struct r8180_priv, watch_dog_wq);
//      struct ieee80211_device * ieee = (struct ieee80211_device*)
//                                             container_of(work, struct ieee80211_device, watch_dog_wq);
        struct delayed_work *dwork = container_of(work,struct delayed_work,work);
        struct ieee80211_device *ieee = container_of(dwork,struct ieee80211_device,hw_sleep_wq);
        struct net_device *dev = ieee->dev;

	//printk("=========>%s()\n", __FUNCTION__);
        rtl8192_hw_sleep_down(dev);
}
//	printk("dev is %d\n",dev);
//	printk("&*&(^*(&(&=========>%s()\n", __FUNCTION__);
void rtl8192_hw_wakeup(struct net_device* dev)
{
//	u32 flags = 0;

//	spin_lock_irqsave(&priv->ps_lock,flags);
	RT_TRACE(COMP_POWER, "%s()============>come to wake up\n", __FUNCTION__);
#ifdef TODO
//	MgntActSet_RF_State(dev, eRfSleep, RF_CHANGE_BY_PS);
#endif
	//FIXME: will we send package stored while nic is sleep?
//	spin_unlock_irqrestore(&priv->ps_lock,flags);
}

void rtl8192_hw_wakeup_wq (struct work_struct *work)
{
//	struct r8180_priv *priv = container_of(work, struct r8180_priv, watch_dog_wq);
//	struct ieee80211_device * ieee = (struct ieee80211_device*)
//	                                       container_of(work, struct ieee80211_device, watch_dog_wq);
	struct delayed_work *dwork = container_of(work,struct delayed_work,work);
	struct ieee80211_device *ieee = container_of(dwork,struct ieee80211_device,hw_wakeup_wq);
	struct net_device *dev = ieee->dev;

	rtl8192_hw_wakeup(dev);
}

#define MIN_SLEEP_TIME 50
#define MAX_SLEEP_TIME 10000
void rtl8192_hw_to_sleep(struct net_device *dev, u32 th, u32 tl)
{

	struct r8192_priv *priv = ieee80211_priv(dev);

	u32 rb = jiffies;
	unsigned long flags;

	spin_lock_irqsave(&priv->ps_lock,flags);

	/* Writing HW register with 0 equals to disable
	 * the timer, that is not really what we want
	 */
	tl -= MSECS(4+16+7);

	//if(tl == 0) tl = 1;

	/* FIXME HACK FIXME HACK */
//	force_pci_posting(dev);
	//mdelay(1);

//	rb = read_nic_dword(dev, TSFTR);

	/* If the interval in witch we are requested to sleep is too
	 * short then give up and remain awake
	 */
	if(((tl>=rb)&& (tl-rb) <= MSECS(MIN_SLEEP_TIME))
		||((rb>tl)&& (rb-tl) < MSECS(MIN_SLEEP_TIME))) {
		spin_unlock_irqrestore(&priv->ps_lock,flags);
		printk("too short to sleep\n");
		return;
	}

//	write_nic_dword(dev, TimerInt, tl);
//	rb = read_nic_dword(dev, TSFTR);
	{
		u32 tmp = (tl>rb)?(tl-rb):(rb-tl);
	//	if (tl<rb)

		queue_delayed_work(priv->ieee80211->wq, &priv->ieee80211->hw_wakeup_wq, tmp); //as tl may be less than rb
	}
	/* if we suspect the TimerInt is gone beyond tl
	 * while setting it, then give up
	 */
#if 1
	if(((tl > rb) && ((tl-rb) > MSECS(MAX_SLEEP_TIME)))||
		((tl < rb) && ((rb-tl) > MSECS(MAX_SLEEP_TIME)))) {
		printk("========>too long to sleep:%x, %x, %lx\n", tl, rb,  MSECS(MAX_SLEEP_TIME));
		spin_unlock_irqrestore(&priv->ps_lock,flags);
		return;
	}
#endif
//	if(priv->rf_sleep)
//		priv->rf_sleep(dev);

	//printk("<=========%s()\n", __FUNCTION__);
	queue_delayed_work(priv->ieee80211->wq, (void *)&priv->ieee80211->hw_sleep_wq,0);

	spin_unlock_irqrestore(&priv->ps_lock,flags);
}
//init priv variables here. only non_zero value should be initialized here.
static void rtl8192_init_priv_variable(struct net_device* dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8 i;
	priv->card_8192 = NIC_8192U;
	priv->chan = 1; //set to channel 1
	priv->ieee80211->mode = WIRELESS_MODE_AUTO; //SET AUTO
	priv->ieee80211->iw_mode = IW_MODE_INFRA;
	priv->ieee80211->ieee_up=0;
	priv->retry_rts = DEFAULT_RETRY_RTS;
	priv->retry_data = DEFAULT_RETRY_DATA;
	priv->ieee80211->rts = DEFAULT_RTS_THRESHOLD;
	priv->ieee80211->rate = 110; //11 mbps
	priv->ieee80211->short_slot = 1;
	priv->promisc = (dev->flags & IFF_PROMISC) ? 1:0;
	priv->CckPwEnl = 6;
	//for silent reset
	priv->IrpPendingCount = 1;
	priv->ResetProgress = RESET_TYPE_NORESET;
	priv->bForcedSilentReset = 0;
	priv->bDisableNormalResetCheck = false;
	priv->force_reset = false;

	priv->ieee80211->FwRWRF = 0; 	//we don't use FW read/write RF until stable firmware is available.
	priv->ieee80211->current_network.beacon_interval = DEFAULT_BEACONINTERVAL;
	priv->ieee80211->iw_mode = IW_MODE_INFRA;
	priv->ieee80211->softmac_features  = IEEE_SOFTMAC_SCAN |
		IEEE_SOFTMAC_ASSOCIATE | IEEE_SOFTMAC_PROBERQ |
		IEEE_SOFTMAC_PROBERS | IEEE_SOFTMAC_TX_QUEUE |
		IEEE_SOFTMAC_BEACONS;//added by amy 080604 //|  //IEEE_SOFTMAC_SINGLE_QUEUE;

	priv->ieee80211->active_scan = 1;
	priv->ieee80211->modulation = IEEE80211_CCK_MODULATION | IEEE80211_OFDM_MODULATION;
	priv->ieee80211->host_encrypt = 1;
	priv->ieee80211->host_decrypt = 1;
	priv->ieee80211->start_send_beacons = NULL;//rtl819xusb_beacon_tx;//-by amy 080604
	priv->ieee80211->stop_send_beacons = NULL;//rtl8192_beacon_stop;//-by amy 080604
	priv->ieee80211->softmac_hard_start_xmit = rtl8192_hard_start_xmit;
	priv->ieee80211->set_chan = rtl8192_set_chan;
	priv->ieee80211->link_change = priv->ops->rtl819x_link_change;
	priv->ieee80211->softmac_data_hard_start_xmit = rtl8192_hard_data_xmit;
	priv->ieee80211->data_hard_stop = rtl8192_data_hard_stop;
	priv->ieee80211->data_hard_resume = rtl8192_data_hard_resume;
	priv->ieee80211->init_wmmparam_flag = 0;
	priv->ieee80211->fts = DEFAULT_FRAG_THRESHOLD;
	priv->ieee80211->check_nic_enough_desc = check_nic_enough_desc;
	priv->ieee80211->tx_headroom = TX_PACKET_SHIFT_BYTES;
	priv->ieee80211->qos_support = 1;

	//added by WB
//	priv->ieee80211->SwChnlByTimerHandler = rtl8192_phy_SwChnl;
	priv->ieee80211->SetBWModeHandler = rtl8192_SetBWMode;
	priv->ieee80211->handle_assoc_response = rtl8192_handle_assoc_response;
	priv->ieee80211->handle_beacon = rtl8192_handle_beacon;
	//for LPS
	priv->ieee80211->sta_wake_up = rtl8192_hw_wakeup;
//	priv->ieee80211->ps_request_tx_ack = rtl8192_rq_tx_ack;
	priv->ieee80211->enter_sleep_state = rtl8192_hw_to_sleep;
	priv->ieee80211->ps_is_queue_empty = rtl8192_is_tx_queue_empty;
	//added by david
	priv->ieee80211->GetNmodeSupportBySecCfg = GetNmodeSupportBySecCfg8192;
	priv->ieee80211->GetHalfNmodeSupportByAPsHandler = GetHalfNmodeSupportByAPs819xUsb;
	priv->ieee80211->SetWirelessMode = rtl8192_SetWirelessMode;
	//added by amy
	priv->ieee80211->InitialGainHandler = priv->ops->rtl819x_initial_gain;
	priv->card_type = USB;

//1 RTL8192SU/
	priv->ieee80211->current_network.beacon_interval = DEFAULT_BEACONINTERVAL;
	priv->ieee80211->SetFwCmdHandler = HalSetFwCmd8192S;
	priv->bRFSiOrPi = 0;//o=si,1=pi;
	//lzm add
	priv->bInHctTest = false;

	priv->MidHighPwrTHR_L1 = 0x3B;
	priv->MidHighPwrTHR_L2 = 0x40;

	if(priv->bInHctTest)
  	{
		priv->ShortRetryLimit = HAL_RETRY_LIMIT_AP_ADHOC;
		priv->LongRetryLimit = HAL_RETRY_LIMIT_AP_ADHOC;
  	}
	else
	{
		priv->ShortRetryLimit = HAL_RETRY_LIMIT_INFRA;
		priv->LongRetryLimit = HAL_RETRY_LIMIT_INFRA;
	}

	priv->SetFwCmdInProgress = false; //is set FW CMD in Progress? 92S only
	priv->CurrentFwCmdIO = 0;

	priv->MinSpaceCfg = 0;

	priv->EarlyRxThreshold = 7;
	priv->enable_gpio0 = 0;
	priv->TransmitConfig	=
				((u32)TCR_MXDMA_2048<<TCR_MXDMA_OFFSET) |	// Max DMA Burst Size per Tx DMA Burst, 7: reservied.
				(priv->ShortRetryLimit<<TCR_SRL_OFFSET) |	// Short retry limit
				(priv->LongRetryLimit<<TCR_LRL_OFFSET) |	// Long retry limit
				(false ? TCR_SAT : 0);	// FALSE: HW provies PLCP length and LENGEXT, TURE: SW proiveds them
	if(priv->bInHctTest)
		priv->ReceiveConfig	=	//priv->CSMethod |
								RCR_AMF | RCR_ADF |	//RCR_AAP | 	//accept management/data
									RCR_ACF |RCR_APPFCS|						//accept control frame for SW AP needs PS-poll, 2005.07.07, by rcnjko.
								RCR_AB | RCR_AM | RCR_APM |		//accept BC/MC/UC
								RCR_AICV | RCR_ACRC32 | 		//accept ICV/CRC error packet
								RCR_APP_PHYST_STAFF | RCR_APP_PHYST_RXFF |	// Accept PHY status
								((u32)7<<RCR_MXDMA_OFFSET) | // Max DMA Burst Size per Rx DMA Burst, 7: unlimited.
								(priv->EarlyRxThreshold<<RCR_FIFO_OFFSET) | // Rx FIFO Threshold, 7: No Rx threshold.
								(priv->EarlyRxThreshold == 7 ? RCR_OnlyErlPkt:0);
	else
		priv->ReceiveConfig	=	//priv->CSMethod |
									RCR_AMF | RCR_ADF | RCR_AB |
									RCR_AM | RCR_APM |RCR_AAP |RCR_ADD3|RCR_APP_ICV|
								RCR_APP_PHYST_STAFF | RCR_APP_PHYST_RXFF |	// Accept PHY status
									RCR_APP_MIC | RCR_APPFCS;

	// <Roger_EXP> 2008.06.16.
	priv->IntrMask 		= 	(u16)(IMR_ROK | IMR_VODOK | IMR_VIDOK | IMR_BEDOK | IMR_BKDOK |		\
								IMR_HCCADOK | IMR_MGNTDOK | IMR_COMDOK | IMR_HIGHDOK | 					\
								IMR_BDOK | IMR_RXCMDOK | /*IMR_TIMEOUT0 |*/ IMR_RDU | IMR_RXFOVW	|			\
								IMR_TXFOVW | IMR_BcnInt | IMR_TBDOK | IMR_TBDER);

//1 End


	priv->AcmControl = 0;
	priv->pFirmware = (rt_firmware*)vmalloc(sizeof(rt_firmware));
	if (priv->pFirmware)
	memset(priv->pFirmware, 0, sizeof(rt_firmware));

	/* rx related queue */
        skb_queue_head_init(&priv->rx_queue);
	skb_queue_head_init(&priv->skb_queue);

	/* Tx related queue */
	for(i = 0; i < MAX_QUEUE_SIZE; i++) {
		skb_queue_head_init(&priv->ieee80211->skb_waitQ [i]);
	}
	for(i = 0; i < MAX_QUEUE_SIZE; i++) {
		skb_queue_head_init(&priv->ieee80211->skb_aggQ [i]);
	}
	for(i = 0; i < MAX_QUEUE_SIZE; i++) {
		skb_queue_head_init(&priv->ieee80211->skb_drv_aggQ [i]);
	}
	priv->rf_set_chan = rtl8192_phy_SwChnl;
}

//init lock here
static void rtl8192_init_priv_lock(struct r8192_priv* priv)
{
	spin_lock_init(&priv->tx_lock);
	spin_lock_init(&priv->irq_lock);//added by thomas
	//spin_lock_init(&priv->rf_lock);//use rf_sem, or will crash in some OS.
	sema_init(&priv->wx_sem,1);
	sema_init(&priv->rf_sem,1);
	spin_lock_init(&priv->ps_lock);
	mutex_init(&priv->mutex);
}

extern  void    rtl819x_watchdog_wqcallback(struct work_struct *work);

void rtl8192_irq_rx_tasklet(struct r8192_priv *priv);
//init tasklet and wait_queue here. only 2.6 above kernel is considered
#define DRV_NAME "wlan0"
static void rtl8192_init_priv_task(struct net_device* dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

#ifdef PF_SYNCTHREAD
	priv->priv_wq = create_workqueue(DRV_NAME,0);
#else
	priv->priv_wq = create_workqueue(DRV_NAME);
#endif

	INIT_WORK(&priv->reset_wq, rtl8192_restart);

	//INIT_DELAYED_WORK(&priv->watch_dog_wq, hal_dm_watchdog);
	INIT_DELAYED_WORK(&priv->watch_dog_wq, rtl819x_watchdog_wqcallback);
	INIT_DELAYED_WORK(&priv->txpower_tracking_wq,  dm_txpower_trackingcallback);
//	INIT_DELAYED_WORK(&priv->gpio_change_rf_wq,  dm_gpio_change_rf_callback);
	INIT_DELAYED_WORK(&priv->rfpath_check_wq,  dm_rf_pathcheck_workitemcallback);
	INIT_DELAYED_WORK(&priv->update_beacon_wq, rtl8192_update_beacon);
	INIT_DELAYED_WORK(&priv->initialgain_operate_wq, InitialGainOperateWorkItemCallBack);
	//INIT_WORK(&priv->SwChnlWorkItem,  rtl8192_SwChnl_WorkItem);
	//INIT_WORK(&priv->SetBWModeWorkItem,  rtl8192_SetBWModeWorkItem);
	INIT_WORK(&priv->qos_activate, rtl8192_qos_activate);
	INIT_DELAYED_WORK(&priv->ieee80211->hw_wakeup_wq,(void*) rtl8192_hw_wakeup_wq);
	INIT_DELAYED_WORK(&priv->ieee80211->hw_sleep_wq,(void*) rtl8192_hw_sleep_wq);

	tasklet_init(&priv->irq_rx_tasklet,
	     (void(*)(unsigned long))rtl8192_irq_rx_tasklet,
	     (unsigned long)priv);
}

static void rtl8192_get_eeprom_size(struct net_device* dev)
{
	u16 curCR = 0;
	struct r8192_priv *priv = ieee80211_priv(dev);
	RT_TRACE(COMP_EPROM, "===========>%s()\n", __FUNCTION__);
	curCR = read_nic_word_E(dev,EPROM_CMD);
	RT_TRACE(COMP_EPROM, "read from Reg EPROM_CMD(%x):%x\n", EPROM_CMD, curCR);
	//whether need I consider BIT5?
	priv->epromtype = (curCR & Cmd9346CR_9356SEL) ? EPROM_93c56 : EPROM_93c46;
	RT_TRACE(COMP_EPROM, "<===========%s(), epromtype:%d\n", __FUNCTION__, priv->epromtype);
}

//used to swap endian. as ntohl & htonl are not neccessary to swap endian, so use this instead.
static inline u16 endian_swap(u16* data)
{
	u16 tmp = *data;
	*data = (tmp >> 8) | (tmp << 8);
	return *data;
}

u8 rtl8192SU_UsbOptionToEndPointNumber(u8 UsbOption)
{
	u8	nEndPoint = 0;
	switch(UsbOption)
	{
		case 0:
			nEndPoint = 6;
			break;
		case 1:
			nEndPoint = 11;
			break;
		case 2:
			nEndPoint = 4;
			break;
		default:
			RT_TRACE(COMP_INIT, "UsbOptionToEndPointNumber(): Invalid UsbOption(%#x)\n", UsbOption);
			break;
	}
	return nEndPoint;
}

u8 rtl8192SU_BoardTypeToRFtype(struct net_device* dev,  u8 Boardtype)
{
	u8	RFtype = RF_1T2R;

	switch(Boardtype)
	{
		case 0:
			RFtype = RF_1T1R;
			break;
		case 1:
			RFtype = RF_1T2R;
			break;
		case 2:
			RFtype = RF_2T2R;
			break;
		case 3:
			RFtype = RF_2T2R_GREEN;
			break;
		default:
			break;
	}

	return RFtype;
}

//
//	Description:
//		Config HW adapter information into initial value.
//
//	Assumption:
//		1. After Auto load fail(i.e, check CR9346 fail)
//
//	Created by Roger, 2008.10.21.
//
void
rtl8192SU_ConfigAdapterInfo8192SForAutoLoadFail(struct net_device* dev)
{
	struct r8192_priv 	*priv = ieee80211_priv(dev);
	//u16			i,usValue;
	//u8 sMacAddr[6] = {0x00, 0xE0, 0x4C, 0x81, 0x92, 0x00};
	u8		rf_path, index;	// For EEPROM/EFUSE After V0.6_1117
	int	i;

	RT_TRACE(COMP_INIT, "====> ConfigAdapterInfo8192SForAutoLoadFail\n");

	write_nic_byte(dev, SYS_ISO_CTRL+1, 0xE8); // Isolation signals from Loader
	//PlatformStallExecution(10000);
	mdelay(10);
	write_nic_byte(dev, PMC_FSM, 0x02); // Enable Loader Data Keep

	//RT_ASSERT(priv->AutoloadFailFlag==TRUE, ("ReadAdapterInfo8192SEEPROM(): AutoloadFailFlag !=TRUE\n"));

	// Initialize IC Version && Channel Plan
	priv->eeprom_vid = 0;
	priv->eeprom_pid = 0;
	priv->card_8192_version = 0;
	priv->eeprom_ChannelPlan = 0;
	priv->eeprom_CustomerID = 0;
	priv->eeprom_SubCustomerID = 0;
	priv->bIgnoreDiffRateTxPowerOffset = false;

	RT_TRACE(COMP_INIT, "EEPROM VID = 0x%4x\n", priv->eeprom_vid);
	RT_TRACE(COMP_INIT, "EEPROM PID = 0x%4x\n", priv->eeprom_pid);
	RT_TRACE(COMP_INIT, "EEPROM Customer ID: 0x%2x\n", priv->eeprom_CustomerID);
	RT_TRACE(COMP_INIT, "EEPROM SubCustomer ID: 0x%2x\n", priv->eeprom_SubCustomerID);
	RT_TRACE(COMP_INIT, "EEPROM ChannelPlan = 0x%4x\n", priv->eeprom_ChannelPlan);
	RT_TRACE(COMP_INIT, "IgnoreDiffRateTxPowerOffset = %d\n", priv->bIgnoreDiffRateTxPowerOffset);



	priv->EEPROMUsbOption = EEPROM_USB_Default_OPTIONAL_FUNC;
	RT_TRACE(COMP_INIT, "USB Option = %#x\n", priv->EEPROMUsbOption);

	for(i=0; i<5; i++)
		priv->EEPROMUsbPhyParam[i] = EEPROM_USB_Default_PHY_PARAM;

	//RT_PRINT_DATA(COMP_INIT|COMP_EFUSE, DBG_LOUD, ("EFUSE USB PHY Param: \n"), priv->EEPROMUsbPhyParam, 5);

	{
	//<Roger_Notes> In this case, we random assigh MAC address here. 2008.10.15.
		static u8 sMacAddr[6] = {0x00, 0xE0, 0x4C, 0x81, 0x92, 0x00};
		u8	i;

        	//sMacAddr[5] = (u8)GetRandomNumber(1, 254);

		for(i = 0; i < 6; i++)
			dev->dev_addr[i] = sMacAddr[i];
	}
	//NicIFSetMacAddress(Adapter, Adapter->PermanentAddress);
	write_nic_dword(dev, IDR0, ((u32*)dev->dev_addr)[0]);
	write_nic_word(dev, IDR4, ((u16*)(dev->dev_addr + 4))[0]);

	RT_TRACE(COMP_INIT, "ReadAdapterInfo8192SEFuse(), Permanent Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
			dev->dev_addr[0], dev->dev_addr[1],
			dev->dev_addr[2], dev->dev_addr[3],
			dev->dev_addr[4], dev->dev_addr[5]);

	priv->EEPROMBoardType = EEPROM_Default_BoardType;
	priv->rf_type = RF_1T2R; //RF_2T2R
	priv->EEPROMTxPowerDiff = EEPROM_Default_PwDiff;
	priv->EEPROMThermalMeter = EEPROM_Default_ThermalMeter;
	priv->EEPROMCrystalCap = EEPROM_Default_CrystalCap;
	priv->EEPROMTxPwrBase = EEPROM_Default_TxPowerBase;
	priv->EEPROMTSSI_A = EEPROM_Default_TSSI;
	priv->EEPROMTSSI_B = EEPROM_Default_TSSI;
	priv->EEPROMTxPwrTkMode = EEPROM_Default_TxPwrTkMode;



	for (rf_path = 0; rf_path < 2; rf_path++)
	{
		for (i = 0; i < 3; i++)
		{
			// Read CCK RF A & B Tx power
			priv->RfCckChnlAreaTxPwr[rf_path][i] =
			priv->RfOfdmChnlAreaTxPwr1T[rf_path][i] =
			priv->RfOfdmChnlAreaTxPwr2T[rf_path][i] =
			(u8)(EEPROM_Default_TxPower & 0xff);
		}
	}

	for (i = 0; i < 3; i++)
	{
		//RT_TRACE((COMP_EFUSE), "CCK RF-%d CHan_Area-%d = 0x%x\n",  rf_path, i,
		//priv->RfCckChnlAreaTxPwr[rf_path][i]);
		//RT_TRACE((COMP_EFUSE), "OFDM-1T RF-%d CHan_Area-%d = 0x%x\n",  rf_path, i,
		//priv->RfOfdmChnlAreaTxPwr1T[rf_path][i]);
		//RT_TRACE((COMP_EFUSE), "OFDM-2T RF-%d CHan_Area-%d = 0x%x\n",  rf_path, i,
		//priv->RfOfdmChnlAreaTxPwr2T[rf_path][i]);
	}

	// Assign dedicated channel tx power
	for(i=0; i<14; i++)	// channel 1~3 use the same Tx Power Level.
		{
		if (i < 3)			// Cjanel 1-3
			index = 0;
		else if (i < 9)		// Channel 4-9
			index = 1;
		else				// Channel 10-14
			index = 2;

		// Record A & B CCK /OFDM - 1T/2T Channel area tx power
		priv->RfTxPwrLevelCck[rf_path][i]  =
		priv->RfCckChnlAreaTxPwr[rf_path][index];
		priv->RfTxPwrLevelOfdm1T[rf_path][i]  =
		priv->RfOfdmChnlAreaTxPwr1T[rf_path][index];
		priv->RfTxPwrLevelOfdm2T[rf_path][i]  =
		priv->RfOfdmChnlAreaTxPwr2T[rf_path][index];
		}

		for(i=0; i<14; i++)
		{
		//RT_TRACE((COMP_EFUSE), "Rf-%d TxPwr CH-%d CCK OFDM_1T OFDM_2T= 0x%x/0x%x/0x%x\n",
		//rf_path, i, priv->RfTxPwrLevelCck[0][i],
		//priv->RfTxPwrLevelOfdm1T[0][i] ,
		//priv->RfTxPwrLevelOfdm2T[0][i] );
		}

	//
	// Update remained HAL variables.
	//
	priv->TSSI_13dBm = priv->EEPROMThermalMeter *100;
	priv->LegacyHTTxPowerDiff = priv->EEPROMTxPowerDiff;//new
	priv->TxPowerDiff = priv->EEPROMTxPowerDiff;
	//priv->AntennaTxPwDiff[0] = (priv->EEPROMTxPowerDiff & 0xf);// Antenna B gain offset to antenna A, bit0~3
	//priv->AntennaTxPwDiff[1] = ((priv->EEPROMTxPowerDiff & 0xf0)>>4);// Antenna C gain offset to antenna A, bit4~7
	priv->CrystalCap = priv->EEPROMCrystalCap;	// CrystalCap, bit12~15
	priv->ThermalMeter[0] = priv->EEPROMThermalMeter;// ThermalMeter, bit0~3 for RFIC1, bit4~7 for RFIC2
	priv->LedStrategy = SW_LED_MODE0;

	init_rate_adaptive(dev);

	RT_TRACE(COMP_INIT, "<==== ConfigAdapterInfo8192SForAutoLoadFail\n");

}

//
//	Description:
//		Read HW adapter information by E-Fuse or EEPROM according CR9346 reported.
//
//	Assumption:
//		1. CR9346 regiser has verified.
//		2. PASSIVE_LEVEL (USB interface)
//
//	Created by Roger, 2008.10.21.
//
void
rtl8192SU_ReadAdapterInfo8192SUsb(struct net_device* dev)
{
	struct r8192_priv 	*priv = ieee80211_priv(dev);
	u16			i,usValue;
	u8			tmpU1b, tempval;
	u16			EEPROMId;
	u8			hwinfo[HWSET_MAX_SIZE_92S];
	u8			rf_path, index;	// For EEPROM/EFUSE After V0.6_1117


	RT_TRACE(COMP_INIT, "====> ReadAdapterInfo8192SUsb\n");

	//
	// <Roger_Note> The following operation are prevent Efuse leakage by turn on 2.5V.
	// 2008.11.25.
	//
	tmpU1b = read_nic_byte(dev, EFUSE_TEST+3);
	write_nic_byte(dev, EFUSE_TEST+3, tmpU1b|0x80);
	//PlatformStallExecution(1000);
	mdelay(10);
	write_nic_byte(dev, EFUSE_TEST+3, (tmpU1b&(~BIT7)));

	// Retrieve Chip version.
	priv->card_8192_version = (VERSION_8192S)((read_nic_dword(dev, PMC_FSM)>>16)&0xF);
	RT_TRACE(COMP_INIT, "Chip Version ID: 0x%2x\n", priv->card_8192_version);

	switch(priv->card_8192_version)
	{
		case 0:
			RT_TRACE(COMP_INIT, "Chip Version ID: VERSION_8192S_ACUT.\n");
			break****case 1:****RT_TR/**********************************************B***************************2***
 * Copyright(c) 2008 - 2010 Realtek Corporation. All C**********************default***
 * Copyright(c) 2008 -Unknown ************!!*********priv->card*****_v*******=rporation. All righ*************}

	//if (IS_BOOT_FROM_EEPROM(Adapter))
	if(prograEepromOrEfuse)
	{	// Read frin terms 
		write_nic_byte(dev, SYS_ISO_CTRL+1, 0xE8); // Isolat****signals from Loader
		//PlatformStallExecuam i(10000*****mdelay(1t WITHd by the Free SoftwaPMC_FSM
 *
02 This Enable in the Data Keep hopecense all Contentbuted terms  or EFUSE.
		for(i = 0; i < HWSET_MAX_SIZE_92Sal P+= 2)
		{****usValue = U Gen_readSoftwa(u16) (i>>1)*******(ed a*)(&hwinfo[i])) = 
 * You****}r mo	else  it
!f the GNU General Pubblic License asLAR See 
 hope
 * FI<Roger_Notes> We set program is distributed in the and rereet PURPOSafter systemUSA
uming
 * FIare Fsuspend modethe // 2008.10.21in thi hope that itEFIOW by 1Bee Sf versiware Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHe
 * file called LICENSE.
 *
 * ContacFUNC_ENn.
 *
4>

#undef LOOP_TEST
#undef DUMP_RX
#undef DUMP_TX
#undef 5t WI#undetmpU1b =  * file callenseCENSE.
 *
 * Coounda_TEST+3

#undef LOOP_TEST
#undef DUMP_RX
#undeG_ZERO_RX
#u, (G_RX_VE| 0x80f the DEBUG_RX_SKB
#undef DEBUG_TX_FRAG
#undef DEBUG_R0x72EBUG_TX_FILLDESC
#undef DEBUG_TX
#undef DEBUGCLKlied unde
 * FITNESSounda real map to shadowthe G_ZEROS_TASKMapUpdae Soft WITHOemcpy( Publi, & the GNl PuMap[G_ZERO****_MAP][0],blic License for
 ****** this 
ls.
 * Copyright(c) 2008 -ensef versiInfo*****Usb(): Invalid boot typel.
 * ThimodifyYJ,test,090106difydump_bufX_DESC

de <asm/uaccess.h>
#inin t.,
 * 51 FranklinThe following areING
#u/ PURPOSindeinclARTIoperram is!!r8192S_pRT_PRINT_DATA******ounda, DBG_LOUD, ("MAP: ****, _DESC

#de <asm/uaccess.h>
#ir8192S_phy.h"
#include EvARTIthough CR9346 regiser can verify whether Autoload is successSE. not, but we still2S_phdouf MEcheck ID codes for 92S here(e.g., dueG_IRHW GPIO pollyregfail issue).2S_ph distributionin tterms Id = NU Gen *)l Public0]ux/usif(
 * TheId != RTL8190 terms _ID blic92U.h"
#include "r8180_9ID(%#x)_debi#inclul.
 *,|
				COM*****prograbTXPowerBILIenseFromEEPORM = FALSE
//				COMt_globalFailFlag=TRU_EFUclude "r8192UE	|
//				COMP_CH		|
//COMP_EFUSE	|
//P_RM		|
				COMP_DIG	|
//								COMP_ P_HALD FITNESSIC*********&& ChannelBOSEnM   	!E	|
//				COMP_CH		|
/R_TRAP_HALDM S_phVID, PID
				COprograeould hviMP_POWR	|
//				COMP	COMP_PVID]
//				COMT		|
		p	COMP_AMSDU	|
				COMP_SCAN	P
//				COMP_CbIgnoreDiffRateTxM		|
Offreet= false;	//cosa	|
//r819
def DEB PURPOS**********,	COMP_LEDp|
//		COMP_Hterms ********MP_AM8U	|
				COMP_SCAN	|******/				COMP_CMD	|
		COMP_LE	|
/rtl8192_usb_id_tbl[] = {
CE(0x0bda, ]ndef DEBCustomerT_COU0x0010, U0xffg.h"
SA
 rved	|
//Realtekthe */
	{USB_DEVICorega *ID0x8192)},
	{USB_DEVICE(0x0orega
//				COMP_CMD	|
		Sub0x050d, 0x805E)},
	/* Sitecom */
	{},
	/* En
//			clude "r8192U//COMP_HT		|
				COMP0BUG_TXCOMP_CMD	|
				COMP301)},
	/* Zisb_device_id rtl301)},
	/* Zinwell *CE(0x0bda, 0x8/92SU
	{USB_DEVICE(0xx050d, 0x805301)},
	/* Zinwell *},
	/* EnGenius 0ndef rtlrd EEP_Config.h"   /* Card EEFort_glin t_CH	
#undef Dreturn

#inc
U.h"
#include "r8180_9 PURPOSOMP_P0x%4x			COMP_RATE	|
//ltek RTL8192 USB WiFi cards"VE_DEVIatic charCOMP_HT		|
				C = "wlan%d";
static int hwwep = P;  //default use hw. set 0 tp use software security
static int ************0x%2ult use hw. sb_device_id  = "wlan%d";
static int hwwep = Corega */
	ule_param(hwseqnumDULE_LICENSE("GPL = "wlan%d";
static int hwwep = },
	/* EnGeGO|S_IWUSR);
module_param(ch},
	/* EnGeni|S_IWUSR);
module_param(hwwep,int0bda, 0x8172)}efault use hw. set 0 try to use h = "wlan%d";
static int h; //always open err flags on

#%dlt use hw. ; //always open err flags onux/u2S_phense USB opam ial funcam i./				COMP_RF		|
//				COMP_RXDESCstruct usb_devUsbOannelrtl8192_usb_id_tbl[] = {
USB_OPTIONAL
	/* Dlink */
	{Unt __devinit rtl8192_usbsb_interfacD004-20ce *intf,UMP_T for Reant __devinit rtEndPointNumber = BLE(usb, r rtl8192_Toterface *intf)(f the GNvinit rtl8192_&	COMP_PEP_NUMBER)>>#undefdrea Merello <andreamrSB l8192_usb%#ram(hwseqnum,int,  rtl8192_ = "wlan%d";
static int hwerface  *intf);

le	= rtl8192_usb_id_tblterface *intf)ux/u#ifdef TO_DO_LISTr8192S_ph Decident, S_IRUID accordyregto 1; /DIDSE.  publishin tswitch(pHalBILIer = {
	default");
_TRACK*****VICE(0x0ID_ALPHA***
 pMgnt* Ca->CENSE("GPL");RT	= rt819xtl8192************n */
	.resume		= rtCAMEOU_resume,                 /* PM resume fnume, 	.reset_resume   = rtl8192U_resSITECOMU_resume,                 /* PM resume fnSitecom****** Copyright(c) 2008 dm.h"
//#incCENSE("GPL");
efault useme,                 f th.reset_resume   = rtl8192U_resWHQL***
 f versiuritnHctTesn

#				CO_resume,       bSupportTurboModu shCOMP_EFUSdapter_start(t_glnet_dBy8186e *dev);
sshort	rtl8192SUM		|
Save A Prol.bInactivePse *dev);
short	rtl8192SU_buff *skb);
void 	PSevicBackup_link_change(struct net_device *dev);
voidLeisur2SU_link_change(struct net_keepAliveLevel");
MODUset_resume 2004-2005 Anume,                 /* PM resuDEFAULute it and/or
#includ2S_phLeluded rtl8192U_suspe_rx_nomal(struct sk_bd fn */
	.rxAggrSubframe)***
_read_eepromme fn  */
11_rx_nd,	     LedStrategy = SW_LED_MODE1
shortnd,	     bRegUse8192       	C_cmd = rtl81SwLed1n);
dO_usb	.rtl819x**************esume fn  */
};

o,
	.rtl819x_tx = rtl8192SU_tx,
	.rtl819x_tx_cmd = rtl8192SU_tx_cmd,
	.rtl819xet_resume   = rhort 	rtl8192SU_tx(o,
	.rtl819x_tx = rtl8192SU_tx,
	.rtl8192apter_start,
	.rtl819x_link_change = rtl8192SU_ieee80211_rx_tl819x_tx = rtl8192SU_tx,
	.rtl819ff *skb, stru	}
#endifSC(channels," CPHY parameters.
 GNU G=ral <5; i++tail8192_usb_id_tblPhyPNNEL[i]usb_probe(struct usb_interfacPHY_PARA1+i9)},
_cmdpkt.h"
#include "r8192U_dm.h"
//#incST, *PCH{{1,2e "r8192end,	          / = {
	{{1,2, 5_DESCP_HALDM	|ense PermanARTIMAC address

static CHAN6EL_LIST Cdev->dev_3,36,3,4,5l8192_usb_id_tbl[] = {
NODE_ADDRESS_BYTE_0,64,149,1NicIFSetMacA,36,40.
 *
 * Cok_buff *s8,9,10,111,2,3,4****d by the Fdwor receivIDR0, ((u32*),  	//ETSI
	{)P_EPROe. Change t ETSI.
	{{1,243,4,5enera,  	//ETSI
	{ + 4),13,14,        /* Driver name 3cx6.h"   /* Card EEEFuse(),,8,9,10,11,1,2,3,4819202x-,7,8,9,10,11,12,13},13},			C819x,  	//ETSI
	{{nclu,  	//ETSI
	{{1]l.
	{{1,2,3,4,5,6,728,9,10,11,12,13,13,36,40,44,48,52,56,648,9,10,11,12,13,15EPROM ops rtlGetonnect,	  /(Bbal_Typubli// i.e.{USB_:WBW	|
88SU{USB1{1,2,3,915,6,7,2,9,10,1125,6,7,3,9,10,11,GUSEND	|Ou32 s: Rx0043)}. devexit_deb3,14},14}					P_IO		|fy i	COMP_RF		|
//				COMP_RXDE//ic int __devinitBoard}, 2rtl8192_usb_id_tbl[] = {
el_plan, /				COMP_Crf_192U;

static strel_plan, ToRF192USoftwamap(u8 channel_plan,  "r819}chande "r8annel//_map(u8 channel_plan, str	COMP_Pdevexit el_plan, "r819_chan=-1, min_chaRF_1T2R
	switcect(strurf_c****ODE_E6052,6,7OUNTRY_CODE_SPAIN:
		cas//lzmENTRY        /* Driver name el_plan, stre_param(hwseqnum,int, ieee80211;
	s,8,9,10,11,12,13,14,36F_DE_MKK1:
		case COUNTRY1, min_ux/usb.h>

#TNESS  PARna tx p		|
 ogs on
of B/C/Dn   A ICULAR PURPO},   0, USA,64}h,9,1lM_LIS/acturally 8225 26
};

static void rtl819x_set_chic int __devinit err flays rtl8192_usb_id_tbl[] = {
Pways /				COMP_Cip == Rhip only suptl8192_usb_id_tbl[] = {
 RF_6052))
	
	/* Dl (channel_plan)
	{
		case CORF_8225) || (pCC:
		case COUN6) || IC:
		case Chip == RF_6052))
			{CC:
		case COUN RF_6052))
	
		case C* Copyright(c) 2008 -6) || 8192_usb_probe,	       RF_8225) ||	case COUNTRY_CODE_TELEC: RF_6052))
			{
				// Clear old chahip only suDot11d_Init(ieee)Tx M		|
 gainalDomain = legacy OFDM|
//	T l819SEND	|ense CrystalCappport B,G,24N mode
                        if ((priv->rf_chip == n[channel_px8192)},
	{USB_DEVICE(0x0[channel_14;
			}
			else
			{
				RT_TRACE(] < min_chan CC:
		case COUN_plan].Chaf (ChannelPlan[channel_plan].L] < min_chan le	= rtl8192_usb_id__plan].Cha12,13,14,36,40,4p));
				Basi<Chande
                        if ((priv->rf_chip == RF_wrIN:
		{
				min_chan = 1;
				err flIN:
14;
			}
			else
			{
				RT_TRACE(COMabled to ", __FUNCTION__)etting, otf (ChannelPlan[channel_plan].Lain settings
				// Clear old channabled _DESC(chY_CODE_GLOSSI v You |
//each path		{
			GET_DOT11D_INFO(ieee)->bEnabled = 0;//this flag SSI_A		{
				min_chan = 1;
				=(b)[(priv->rf_chip == =(b)B3] && (a)[4]==(b)[4] && (a)[5]B14;
			}
			else
	M	|
2-14 passettV	|
/E.  mptypend	= rtl8[2] && (a)[3]==(b)[3] ", __FUNCTION__)SSIet channel map in f#define	\
			_pDesc->RxMCS == DEe->bGlobalDomain = true;
	=(b)[3] %#x, eqMafine	
				// Clear old cha=(b)[k_rate(_DataRate)\
B COUNTRY_CODE_GLOBAL_DOMAtrackyregded in 
			GET_DOT11D_INFO(ieee)->bEnabled = 0;//this flag enabTkevice *follow 11d country IE setwvoid C
	/* D>bGlobalDomain = true;
			brvoid 
		}
		default:
			break;
	}void C_DESC(
	{USBn thisBuffer t_deIdx( //MICare FlDomain0x55~0x66, total 18ENSEstail// _ALLOC CCK,p
				(1T/2T)p));
				Index STA aabove b_WEP ion in Y or OID_ADE_GLOBAL_DOMAct sk_byTry to uY or OID_A;

static void rtl819x_set_chls.
 *annel_map))bGlobalD	COMP_LED1 ~ 14ware Foundathe will				sutruct  RF A & Bf(Ad ||\(rf_)[0]eneraltion)
  < 2   //    LIST Cls.
 *EncrypGeneral Pub3EL_LIST C        hannelPlanCKithm == W set this======prograRfCckChnlAreak;
	}[tion)
 ],3,4,======ntry(struct net_deFRAST+tion)
 *3,64,149============
				=====\n");
      M ||\1T  //DbgPrint("Ofdm             1T            Call ResetAllEntry              6                                 \n");
        //DbgPrin2("==============================2=========\n\n");
	ulcommand |= BIT31|BIT30;
	12                   }    
{
#	}
r OID_ADsetKey p));
				HAL variof Ms.ex=0;uncryption)
        //      return;
//debug
 s.
 *//DbgPrint("===============      * Copyrig**********),  "======-%d CHan_    {
  K1:
	c char  //    , il.
	{bgPrint("                            ******evice *dev, u8 addr, u3"
			-1Ta)
{
        write_nic_dword(dev, WCAMI, data);
        ===========================\n\nBIT31|BIT16|(addr&0xff) );
}

u32 r2ad_cam(struct net_device *dev, u8 addr)
{
  ieee = ndex<TOTAL_CAM_ENTRY;ucIndex++)
 ******(ucInwillAs dis dedicated cOMP_LEDe->bGlobdif

}
tic CHAN14EL_LISwill92_priv 1~3 use the samOTAL_CAM_ENct skif(Ad       it
=====)int stCjapriv(-3      92S_x_buff *skthis progr Pub9)s = usOMP_LED4-9_msg(udev, usbx_tx_cthis =======tic key a0-14_msg(udev, usbase C========emov===\n")====/
				- n OIDRITE,
			areeee->bGlobta);
        k;
	}ct skCck            CCall RegPrint("                           ev, 
      _E TimeOut! status==============\n\n"
	}
}

u8 read_===========================\n\e *dev, int indx)
{
	int status;
Y;ucIndex++)
  ct r8192_priv *priv = (struct r8Y;ucIndex++)
e *dev, int it
tion)
   = 0=============prograet(ieee statuFDM24G,3,4,5 indx)
{
	int status;
	u8 data;
	stru      87_REQ_GET_REGS, RTCCKEQT_READ,
			       indx:%d\n", status)        for(ucInruct r8192_priv *)ieuct net_device *dev, u8 addr, u &dat"Rf{
  k;
	} CH{
   CK     _1Ttend p2Tic_dwo/o 16, so rael.
	{	te_nic_byte_E(structut! status:%d\n", status)ata);
        
	int status;
	u8 data;
	struce *dev, int indx, u8 data)
Y;ucIndex++)
  data)
{
ith
 * 13,14,36,2009/02/09 CAL_Cad},
	/*newfine CAMt itat _Datauct r8192_priv *)ieee80211_priv(dev);
	struct usb_device *udev = 
	{USBnelPlae->bGlobadiWEP ence betweenr (i
				20/40 MHZ
udev;

	status = usb_control_msgdev, usb_sndclpipe(udev, 0),
			       RTL8187_R_SET_REGS, RT187_REQT_WRITE,
			       in|0xfe00, 0, &dtempvak_bu(
				min_chan = 1;
				X_PWR_HT20_DIFF+sb_rcv)&(0x0
//				COMk;
	}Ht20ays [RF90_PATH_A    Ca (}


}

&0xF|
//				COM{

	int status;

	structBr8192_prriv *pri>>4indx (stL8187_REQ_SL818<->HTSET_REGS, RTL80, &data, 1, HZ / 2);

        if}


}


void write_nic_word(struct net_dend p *de indx, u16 lpipe(udev, 0),
			       RTL8187_R}


}


void write_nic_word(struc6) || (REQT_WRITE,
		REQT_WRITE,
			       inudev, 0),
			       RTL8187_REQ_SET_REGS, RTL8187+1
        er, RTAL_C}


}


void wCENSEs:%d\n", status);
        }

}


vo, int indx, u16 data)
{

	iLel maHttatus;

	struct r8192_priv *priv = (struct r8192_prriv = (struct r8192_pri(dev);
	struct usb_device *udev =L8187_REQ_SB0, UEdgeee->bGlobalDomain0, U  				ifv);
r e of MEstruabilitymsg(udev, uHT 40 b0, Ueipe(92_priv   }


}


void write_nic_word(struct net_dBAND_EDGE indx, u16 data)
{

	i   ipipeHt40s;

	struct r8092_priv *priv = (s ID_ADD  indx, 0low0, &data, 4   printk("write_nic_dword TimeOut! s1,2,3,struct usb_device *
        }

}
high0, &data, 4, HZ / 2);


        if (status < 0)
        {
  oid write_nic_byte(struct net_device *dev, int B status:%d\n", status);
        }

}



u8 read_nic_byte(struct net_device *dev, int Bndx)
{
	u8 data;
	int status;
	struct r8192_priv *priv =,
			 2     indx, 0, &data, 4, HZ / 2);


        if (status < 0)
        {
  +2              printk("write_nic_2word TimeOut! status:%d\n", status);
        }

}



u8 read_nic_byte(struct net_devirn data;
}



u1x)
{
	u8 data;
	int status;
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struct3%d\n", status);
        }

	return data;
}


_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REvctrlpipe(udev, _READ,
			       indx, 0, &data, 1, HZ / 2);

        if 
				s < 0)
        {
                printk("read_nic_byte TimeOut! status4              printk("write_niriv = ====ord TimeOut! status:%d\n", status);
        }

}



u8 read_nic_byte(struct net_deruct net_device *dev, inx)
{
	u8 data;
	int status;
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struct5ata;
}

u16 read_nic_word_E(struct net_device *dev,_control_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,x)
{
	u8 data;
	int status;
	struct r8192_priv *privce *udev = priv->udev;	|
/
void write_nic_wor 0)
        {
  _CHK data11_p_E TimeOut! status:%d\e COUNTRY_CODE_TELEC:
	-A x, 0	for (40 tatus%d92_p_dword(dete_E(stru{

	int status;

	struct r81914,3dev, int indx)
{
	u32 data;
	int status;
///	int riv = |
//	tstruct r8192_priv *priv = (struct r81riv = (struct r8192_priv *)i(dev);
	struct usb_device *udev = priv->udev;

	staB result;

	struct r8192_priv *priv = (struct r8192_priv *)ieee80211(dev)_REGS, RTL8187_REQT_READ,
			       indx, 0, &data, 4s = usb_con	struct r8192_priv *priv = (struct r81_device *udev = priv->udev;
	case COUNTRY_CODE_TELEC:
	nt r	str   i-}

}



/92_prREGS, RTL82_priv modify funct   printk("write_nic_dword TimeOut! sta        }



	return data;
}


//u8 read_1status:%d\n", **********&*****DBG
}

ta, 4,us == -ENODEV) {
			priv->usb_error = true;
		}
        }



	return data;
}


//u8 re_cont PHY rtl8185/rtl8192 common code
 * plansu8 reead_phy_ofdm(struct net_device *dev, tatuatus < NODEV) {
			priv->usb_error = true;
		}
        }



	return datarn data;
}



u16 _device_stats *rtl8192_stats(struct net_du8 read_phy_ofdm(struct net_device *dev, u8 astruct net_device *dev)
{
}


static struct net_device_stats *rtl8192_stats(struct neans are to possibilty turn it  0, &data, 2, HZ /n code...
 */
inline void force_pci_postin printk("NODEV) {
			priv->usb_error = true;
		}
        }



	return daruct net_device *dev, int ************************
   -------------------u8 read_phy_ofdm(struct net_device *dev, u8*************************************************************************
   ----------------ans are to possibilty turn i0)
        {
              read_phy_ofdm(struct net_device *de    NODEV)T_REGS,furn daardware secur      }


	returnux/usb.h>

#setKey remainedNTRY;ucIndex++)
g(udet_devic 	tx13dBm_READ,
		hip == RF_6052))
		*10301)prograriv = (TRF_8225) || (pClear old channel map
	ee = priv211;
	struct ieee80211_network *target;
,
	/* ZiA
			iet_detatustatus:			RT_TRACE(COMP_ERR, "u&E(0x); sta
			ieeB	// Set new cto);
			ieeA, bit[3:0]target, &ieee->network_lisx)
{
b_driver = {
	+= snprintf(page 0)b_de len, count C len,
                "%s ", ta7:4]e = priv] < min_chan }
			break;
		}
		caseus;
	_plan].Cha ", ta15:12len,
    DOT11D_INFO(st, list) {

		len +hip only su&0x1 + len             ", t0~3,
	/* FIC1 ", t4~7         2e = priv->ct _CHANNEL_LIST
{
	u8	Cha
	init_l819_a verivC
#undefnelPlan[channel_plan].L<====nelPl.h"   /* Card EEPRO******targdriver_chaSTATUS_SUCCESS;
r Re//
//	Descriannel: *de	ense HW sterser ublidev;92_uby E-4},2suspend	= * remove fnlude "ieeuct ed)
   //	Assumta;
//	stru1.clude "ieee80211hast11d.hien = 0		2. PASSIVE_LEVEL (  */interface)ice *deCret r81by * 51 ,|
//				COMP_//
static void
static str6 rf_T		|
		ubli(struct net_device *#und
{
	e0)){
	rfree prog 	*t - l= ieee80211nt - 
#undef u8ord _RX_Vage, char **start,
			  of_t >offset, int count,
			  int *eof,  Retrieveali.itsoftwar.e = privm is free software; (*************)((EG_OFge to ETSI.
	{{the imp)>>16evice *u * Copyright(c) 2008 - 2010 Realtek Core_param(hwseqnumm is free softwar *eofG_RX_VERB=0;i<16 &ree SoftwaICE(0x0MD    lude "SC(chaTo RTL818NU Genee "r8selesk for speRX_FRAG
#& CmdEERPOMSEL((priv-ODE_MKK:
		case COUNTRYoTICULAR PURPO******** the GNU General Pu        	COMude "r8192U.h"
#include "r8180_9=max;)
		{
See n += snprintf(page + len, counCOMP_EFU11_prien,
				"\a_global_ug_component =page %x##############819xU_n", (page0>>8));
		for(n=0;n<t_global_OKl.
 * This                           	COMPBLE(usb, r3cx6.h"   /* Card EEPROM#unde//T		|
	SE. e-l Pu/* Dlink */
M	|
t_gl obal_//		the );
		}
	}
	len += snprintin t _CH	
	int len STA alude "+ len, count - len,"\n");
	*eof =	.rtl819BLE(usb, rtl8192_usb_id_tbl);
MODULE_DESCRIPTION("Linum can nt
 * under the tSee of version 2				COMP_RFNU General Publiriv->udrea Merello <andreamretKey Q_TASKDEBUG ||\
92_prfuturev);
l.
 * This #undef DEBUG_TX_ALLOC
#undef Dh
 * /
	.disconnect	= rtifb_driverRegry to use ha>* PM rHANNEL_DOMAINicen) || pend,	          /*E(0x0bda, 0&resume		=)
		{
	PLAN_BY_HW_MASKthe Fcounme,        E(0x0bda, 0x8HalMap+,n++)
				}				.
 *
 * Cotf(page + len, count - len,
			(~(		"\nD:  %2x > ",n);
			for(i=0;f the apter_start(    	|
/_DIGHW list(page + len, count - len,
					"\nD:  %2x > ",n);
			for(i=0 ?nomal :ge + leM	|
U0211/donot0, &dge 0211_privatic.OMP_TXAGC	|
     n<=max;i++,n++)
				len(max;)
		{
			len )f, void *da8));
		for(n=0en, cou
	.nic_type = NIC_8nce numberstl819x_read_eep)
		{
			len +GLOBA
			AMIN***
s.
 *PRT_DOT11D_INFO	pDot11d* Ca = GEx=0xff;
	page_type = NIuff* sk = 0x200;

->by of Mmd,
	.rtl819ieeet net_device *dev, struct sk_bufffset, int count,87M */y of MEd 0x20



*data
	int len = 0;
	int i,n,page.
 * ***** and/or modi, count - len,
				"\n############);
		for(n=0(%d)	_pDesc:  %2x > ",n);
92SU_rx_nomal(\nD:  %2x > ",,11},             iv *)ieee802 snprintf(page + len, cotruct sk_buffE(0x0bda, 0x8ardwa 2SU_rx_nomal(sunt - len,
			tf(page + len, count - len,
					"ff_t offset, int count,
			{
			l	Len;
}CH char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_devshgoriBLE(usb_get_92_priv_mapge0)){
		len += snpr9,10f(page + len, count - ln,
				"\n#################### of the G;
		for(n=0;n COUNTRY_C},13	int i,n,v = dic int ntk("BLE(u80_c_ge:Errorfset, int cou! S      2004-20*********rrent register pa);
MOD}nprintf(page + len, countt, int cou_debardwarrrent register paunt -BLE(usx_sv = data;

	intrrent register pae_E(st snpvoid *d0truct net_device *dc_gege0)){
		len += snprintf(pa
	int max=0xff;
	page0 = 0x800;

	/* This dump th"\nD:  % + lennt - _ucIndex+
#undef 		}
	len += snprinlock(i=0;n, count - len,"\n");
task+ len, count - lev =T		|
		siz + len, cprograops->nD:  %2xEG_OFFSET_92S(pagnt proc_get_registe data;

	intump the += shal_dma)
{
	struct timer(define watch_dogdata;
, char **st r8192_priv *p.dILIT= (un dised long7,8,iv = (struct r8192_priv *tmask fo = t r8192_priv *p_callbac rtl8//		}
	len_priv *_starta)
{
	s/
	.disDEBUG_lly 822h"
#iU GenregisterLen;
})
				len += s/*n####################page %x##################\n ", (page0>>8));
		for(n=0;n<
 *tmask fo:  Thisitmask fo actually onlytreetRRSR, RATR     BW_OPl819ieee80LIST
 *|
			nt =ge %xoS FORstruhw cl8192 as its nt ussay(i=0   input: 		len += snpdev len,outunt - lone len,driver92_QueryBBRenotice		len +=par
		led    mod
	pag* remove fn   strul819treetwe filtered lenn####################page %x##################\n ", (page0>>8));
		for(n=0;n/
*/
	if(!IS_B_hw	len +ge0)){
		len += sn,page0;

	u32ieee, "\n= 0,
	strRSt net###paieeeBwOpevice *t_deviTmhan  = d,rtl8192_QueryBBReg(dev,(page0|n), bMaskDWord));//##pag - len, "\,nD:  %2x > ",n);
			for(itl8192U_susperogra\n#######->ded 211_pr*****WIREL  //l819_B***
;
//	struct r8%2x > ",n_20MHZnux drtruct neRATE_ALL_CCKount - le *dev		"\n###########********nt register page */fo,
	len += snprintf(page + l5G |f(page + len, count - len,
				"\n####end pAG#############page %x###:  %2x > ",########\n ", (page0>>8));
G				len += snprintf(page + len, count - len,
				"\n####### | len, "\nD:  %2x > ",n);
			for(i=0;i<4 Reg(dev,(page0|n), bMaskDW########\n ", (page0>>8));
	UTO:################\n "ge %xk_buff *skb);
bool dump the<4 &len += snprintf(page + len, countchar **,rtl8192_QueryBBReg(dev,(page0|n), bMaskDWcount,
	#####page %x######(dev,(page0|n), bMaskDWieeede "rn, countage, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct n(dev,(page0|n), b1SS(dev,(page0|n), b2net__device *dev = data;
//	struct r8192_priv *priv = (stru########\n ", (page0>>8));
N_24f(pagis pt scAlgoritruc len,by%x#######,
	/* Cge0>>8))will b;
	*eof = ",rtlen, c ", (associt r81AP does&& n<\n ", (pi=0;n<len += snprintf(page + len, count8x ",rtl8192_QueryBBReg(dev,(page0|n), bMa
	/* This dump the current register page */
##########page %x######rintf(page + len, count - len,"\n");
	*eof = 1;
	rN_5f(page + len, count - len,
		5 > ",n);
unt - len, "\nD:  %2x 
	/* This dump the current register page */
n);
			for(i=0;i<4 && n<=max;n+=4,i++)11_pd by the Free Softwa%2x > ",n_devi/	struct snp16 &)
{
	atr_Addr(a);
MODUtruct r8192_pt,
			  */
	intchan=-1, min_chODE_ETSI:dump the truct r8192&= ~(ent register pagepage %x#e. Change to ETSI.
	{{et_rt_deruct r819Y WARRANTY; without even UFWP, 1
#inclu_priv *prse{
		len += snprin0x31undefe *dev = d(i=0riv ) << 24) | , (pdev =page00f
		{
ance. Change to ETSI.
	{{ - lenr(n=0;nux/usb.h>

#n = 0etry Limit	COMP	page06,40,44,48,52,56,60,RE	lenLIMITturn 0;

	/Snet_> ",n;
			f<<)
				len +=_SHOata)HIFT | \snprintf(pLong len, count - len,
						"LONG ",rtl "r819 %2x  A PARTintfWin+= sor(i= snprintfTx AGCen,"\n");
	*eo
			ieeincluve fnFeed00;
		le
voien,"\n");
 off_open#def900;
_registers_device *dev = data;
//	struInitial8192relruct;
			for( = 0;
	int i,n,page0,pagtl8192},14}				s," CMAC,				Chould		len +=12,1befor####wnobal_FW = 0;
	i dist09.03,9,1dhis dump thgister page */
	if(!IS_BB_RMactl8192B_privFwD)ieee80ASICge0)){
		len += snprintf(pagpage  %x#####    ;0 = u16	len += 2bf(page32	len += 4b###page 	PRECV	|Cnn

#2t pro* Copyright(c) 2008 ---->age0 = 0xd00;

	/* This dump t)int *eof, 212,1int *eoiz(struc ||\bGlobaln sequ7_RE, Revis 0;
	int i,n|
//			int .1_priv(de * 51 Franklin#pagregistea)[0] 2U_sus|
//			 snprint0, USA
 *
Digi    Core,  CPUtl819,
		},   snpri/OG_IRQolve FW *)ieee802//			 ", (NU Geneare Fral Ppage;i<Chann distr1.04riv *)P_HALDM
		}
	}else{
		len += snprinre FCLKR+				P_HALDMifx#########def DDESC	|
				;
		}
	}&_pri3fe(char *p	  int d by the Free Software Fisters,rn len;_e(char *p			}
 Clearrd))RPWv->udrd)) snprintLPS.);
	tynlir(i=09.02.23 net_device *dev = dat192_{USB_d));
	return len;
}
static int proc_get_regMP_TX
#un_e(char *p offset, int73e(char *pd by the Free Software FMP_TX
#undeevice *dev = datuUT
 * ANut WI49,15x > ",nPOS, suggesThis duSD1 Alexe curre09.27. net_device *dev = datSPS0dation.
 *
5undef
		for(n=0;n<=max;)
		{
			le+= sn7#######y of MEAFE Macro B	*eo's     gap adn########for(i=0;i<4 && n<=Mbias);
		}
	}else{
		len += snprinAFE_MISC14,36,40,44,48			"%8.8x ",rtl819_RX_FRAG
|",rtBGEN
		}
MBENount -",n);
			PLL);
				(LDOA15V));
		}
	}else{
		len += snprinn,"\n"datio2_QueryBBReg(dev,(page0|atic int prkDWord));
LDc inrintf(page + len,LDOV12D b	*eo
	*eof = 1;
	return len;
}

stati
			nt proc_get_stats_tx(char *page,evice *destart,
			  V12_t offset, 		}
	}else{
		len += s
 *
 * ConPS1nt proc_g//
		for(n=0;n<=max;)
		{len +=kDWord));
t - lLDrintf(page that it leepUs(2##########y of MES + lenRegulator<4 && npri
		}
	}else{
		len += s len = 0;

	len += snprintf(page + len, count - len,
		"TX VI prioriSWrintf(page. Change to ETSI len = 0;

	len +={USB_Da7b26%2x > n len;
}
static int proc_get_regoundation.2_QueryBBReg(dev,(page0|re Foundation.
 Word));
0x08ntf(page +gineer Packntf(PENTRY#######
		"TX BK priority error int: %lu\0;

	/* Thised by the Free Software FMP_TX
#undeGE priorit2 DEBUnpristruct  64k IMEM%x##################\n .
		"TX BK priority error int: %lu\n"
		"TX MANAGE priority ok int: %lu\n"
		"TX MANAGE prio**st6 error int:				len +cint *eof, void *data)
{
	struct ne",rtXTAL		"TX MANAGE priority ok int: %"TX BE queue: %//		"TX beacofbntf(page + len,for( coui=0;i<4 && %d\n"
		"TX VO queue: %d\n"
		"TX P+= s proc_get_stats_tx(char *pa %lu\n"
		"Tk int: %lu\n1of thnpriAtt r81 %lu\n"
	toage0TOP/BB/PCIe8.8x ",r
		"TX BK priority error int: %lu\n"
		"TXr int: %lu\n"
//		"TX high poundatio//		"TX be0xEEn",
//		: %lu\n"to 40Mueue: %deof, void *data)
{
	struct n{USB_D/		"TX hSC Disority ok int: %lu\n"
		"TX BEACON prioiste snprintf(page + len, counts.txbkokir,
		priv-5f
				ats.txmanageerr,
		priv->stats.txbeac|0xa
//		"TX 		"TX V12,1eue: %d\n"
		"TX VO queue: %d\n"
		_registers_e(		priv->stats.txbeaconerr,
// queue: %d\ %lu error in######page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
the implied war(page + len,BBRegd8x ",rt  ind	"TX VIOREG R/Wy ok int: %lu\n"
		"TX BEACON priority error int: %lu\n"
//		"TX high priority ok int: %lu\ny error int:of MEREG_ENy ok int: %lu\n"
		"TX BEACON priority error int: %lu\n"
//		"TX high priority ok int: %lu\nf DEBU"TX h + len,he= snprintf(pagto Fread(&(priv->tx_pending[BE_PRIORITYd,
		netif_queue_stopped(dev),
		priv->stats.txoverf80)**stBe *uded by the Free SoftwaCMDokintF92_QueryBBReg(dev,(page0|  ofn.
 *
3%2x > ",Fixpriv-RX FIFO		COMP(usb e####), 970410
		"TX BK priority error_Ee %x####5cetif_queue_stoppedtruct r8192_k int: %luBIT7s.txbk //F count - save,v);
d tn +=inpriv-bit
	*ee full G970621err,
		priv->stats.txmanageokint,
		priv->txbeerr,
		priv->stats.txbkoki	"TX be(~riv->PU	*eo\n "		len += s > ",nriv 8051 eee8P_DB wrong"

#includ.en = 0;
	int i,n|
//				C16));
		for(n=0;n<=truct r8191c####8
		priv-2S_phy.h"
#iEXP>en,
make oid  tha;
	*DMA1/dot6 rfsb_co*)ieee80211_ptxbe Striv = (SA
 *
systemifty fa RPT wa  %2x 	rtl8_proc_Sd error int: %lu\n"
	|
//				CO3riv *)ido*priv 
		}
	}else{
		len += snprinTCriv->", (p#########TXDMA_IO_MAVALUE) i,n.proc_net);
}


v*************pe that it will be usefulIC
				"\n###ct r8}while(#########p--tus;
	DT
 * 1msOM   	#########pa<= 0ER_TRACKING |
				COMP_TURBO	e0>>8));
		for(n=0;n<=max;)
		{: ####### rtl8192_proc_rem ata;out!! CurrARTITCR				C			COevice *de{
	remove_proc_entry(RTL819xU_  ofY WARRANTY; without even   off_>stats.rx.proc_rintf(8192_priv DEBUGemove_proc_entry("stats-ieee", p|->dir_devprinve s_MODULEfor Realtek RTL8192 USB WiFi<---e0>>8));
		for(n=0;n<=max;)
		{
			le		"\fset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = d1.len += snprintfislen, cinvok(pagt driv *prif(page + lenoncee0 = 0x000;
	pag 1 = 0_priv(dev);

	6.10len = 0;
	int i,n,page0;

	int max=0xff;
	page0 = 0xAull 	/* This dge0)){
		len += snprintf(page + len, count - ln,
				"\n#############ge0)){
		len += snpr7,8, snprimax=HIGH_THROUGHPUT	pHT0;

	/*0;

	/* This dump		removsnpriu8
	remov, RxPageCfg,ntf(ge + , count /
		 += snprive_pr##################\n ", (page0>>8));
("registers-b", {
			len +=########Tx/Rx
		"TX2VERB(BBRSTn|BB_GLB_>nameSCHEDULE_EN|MACRXmove_pToc_eDdir_de|e0|n FW2HW, rtR>dir_de
		remove|HCI_	priv->diULL;		remove_pct n3//		"_buff *sHalFunc.SetHwRegHandler(9,10,11,reg._VAR_COMMAND, &G_RX_VE14,36,40,44,48,52,56,60,ats-ieee",2b ThisLZMv, TISTER COM 090305en,"\nLoopstartded onent =e = priv-v = creevice *,14}					_NO_LOOPBACKr **s#pagnf_t  = creaas		for(n=0;n of the Gname,
					  SS_IFDIR | S_IRUGO | S_Iove_G_RX_VERBLBK_NORMAL; this progrv->dir_dev) {
		RT_TRACE(COMP_ERMACUGO | S_IER_Tinitialize /pr;
	}DLB/rtl819int count,
			  int *eofSeriousv = da:rxurber rtl8192_te_prS_RATE1int *eof, ne(struct net_device *dev)
{
	struct proc_dir_entr_entrstrucruct r8oc_get_stats_tx(char *pagBKMD_SELemove_proc_nD:  %2x >CRlen += snprintf(page + lCRe_E(struceceiskb);fi	str_dev) {
	//	remove_proc_entry("-registers",priv-: priv->diRCRx, dev);sev);
		remx", S_IFREG | S_IRUGO,	}


	e = cQPNr8192S_phy.h"
#include  (page8.18_proc_6 endpace s:proc_(1) //	r nintf);on r81Qssive && n	}

	2 = create_proc_reBCNQ, HQIORITMGTentry(ats-ap"3 S_IFREG | S_IRUGOKQ, BEQ, VI   priVOQg.h"
0x08));}

	4 = create_proc_rePUBentry("dev,(pERR, 11dev->name);
	}

	e = create_proc_read_entry("sev, proc, S_IFREG | S_IRUGO,
	ntry("s2			   priv->diACE(COMtats-ap"_get_stats_ap, dev);

	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		    8 _DataRarror int: %lu\n"
		"TX BKats.{USB_7/%s/re This);

	if (!e) {
		RT_Tnprintf(page + len, coun0xa4kint,
		M	|
HCCAQ	}


	
{
	struct net_device   */ = data;Rive ",n);
			foe curr-06      r8192_priv *priv = (struct r819292_QueryBBReg(dev,(ACE(COMP_ERRir_dev);
0;

	
	}


	npriNG
#unruct		len +u

	*eofdify it
 * under the tint max=0xff;
roc_entmayic_rs/registes/regissu_ops/%s\n",
		b=max_DIG	l Public
		r8d TimeOu     }


}


voriority error int: %lu\n"
		"TX MANAGd TimeOut, intF_EFUSd by the Free Software Foundation.
 }


}

 *udev = int count, 2.5Vrr,
	d by f(paioc st//2, dev);
	if (!e) {
		RTndef DEBUG_ZERO_RX
#undef DEd by the Free S_TX_FRAG
#undef DEBUG_RX_


}


undef DEBU,
	/* Cf_t ofen, coC	*eo	e = create_proc_read_>dir_dev, proc_get_registers_8GISTERS
#undef DEBP_ERR, Programstatublic d by the Free Softwaisters-tion#undef DEBUG_R, count !	   priv->dir_dev, p%sdev,eate0x33 withndef 			C_prior*int__);
		ltek RTL8192 USB WiFi G
#unCONFIG OKx.h"

#incs", priv->dir_dev);
		remove_proc_en-registers",priv->dir_d}
off_t offset,SU_Hw S_IRUure,14}					sb, count - len,
						"%8.8x ",rtl8192_QueryBBReg(dev,(page0|n), bMaskDWord))/
		le;
//	struct r81if (n,
			struct net_device *dev = dat, "Unaiv *priv =)
{
statDEVICE_T//1d));
		}
	len += snprintf(page + len, count - len,"\n");
	*eof =  "r819xU_cen = 0;
	int i,n,page0;

	int max=0xff;
	page0 = 0xa00;

	/* This dump the current register page */
				len += snprintf(page + len, count - len,
				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
		{
			len += snprintf(page + len, count - len, "\nD:  %2x > ",n);
			for(i=0;i<4 && n<=max;n+=4,i++)
				len += snprintf(page + len, count - len,
						"%8.8x ",rtl8192_QueryBBReg(dev,(page0|n), bMaskDWord));
		}
	len += snprintf(page + len, count - len,"\n");
	*eof = 1;
	return 
	int len = ers_b(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
//	struct r8192_priv *priv = (struct r8v *)ieee80211_priv(dev);

	int len = 0;
	int i,n,page0;

	int max=0xff;
	page0 = 0xb00;

	/* This dump the current register page */
		len += snprintf(page + len, count - len,
				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;n<=max;)
		{
			len += snprintf(page + len, count - len, "\nD:  %2x > ",n);
			for(i=0;i<4 && n<=max;n+=4,i++)
				len += snprintf(page + len, count - len,
						"%8.8x ",rtl8192_QueryBBReg(dev,(page0|n), bMaskDWord));
		}
	len += snprintf(page + len, count - len,"\n");
	*eof = 1;
	return len;
	}
static int proc_get_registers_c(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct nCE(COMP_INIT, ranklin Stds.txbkax;)
	espons - len,until FI

	if  len,IC's 00;
SEND	|
//		e0>>_remove(###################page %x##INIRTSMCS  de###########\n ", (p=0;n<=max;))
		{
)<<8
		f/proc/truct r8192_priv *SIFS = crea	 _DataRaintf(paifsTimMKK1:
0e0e0a0asnprine(struct net_device *dev)
{
	struct proc_dir_entrex)
,  (pice *d)&_rx_nomal =riv = iv *prdataval[492_p{8021{USB_(struca net_d}ruct ndex)
{riv }
	lBILITACK
		      "/proc/net/rtl8IF, whK,oid t3,14,3)
{
	struct r8192cx",b useve(udelike CTS )iee!riv = ieee80211_priv(dev);
	//r queid tog_time)
{
	struct r
				_priv *priv = ieee80211_priv(dev);
	L818 only 2commit(dev);

	sch
				e_work(&priv->reset_wq);
	//DMESG("TXTIMEOUT");
}


/* thL818is only 3_device *. Change to ETSI.
	{{1ity_t priori count - lennet_device *dev = data;
//	struct r8192_priv *b", S_IFRE#################\n ", (pars_1, 	 _DataRa * file called LICENSE.
 *
 * CoTXOPa)
{tal data {
	RT_//NAVapktprotectS FORnD:  n<=max;n+,
			_priv page, chaF **start> ",n)Count_ne			forriv *)i. Change to ETSI.
	{{DARFRClied w0, but WIT***************************+d_entr60504
#unde current register page *****************************************--HW S****
      ------rintk("%2x ",read_nic_byte(dev,n));gf = 1;
	return l", (page0>>2;

stabgPrint("====8EL_LIST C. Change to ETSI.
	{{-HW 0+i*d_ent1f)
		{RT_TRACE(COMPAggreg(struclength l
			.%2x > ",n);
			for(i=0;i<4 92_se
/************%lu\n"
		"TGGLEN_LMT_H{USB_ftus;
	har *MPDUcmd=ecmdto 12Kree G	|
//age +GI ****));
		for(n=0de)
{
	u8 ecD_OPERATIN;
		fddd7744warrantg(de GI-----------------------);
	write_ni****
 fffe(dead(&(pri);
	e;i<1& n<=ma
	ecmd=ecm<4 && n<=max;n+=4,i++)NAV_PROT_LEN{USB_D	RT_TRACEn");
	XOP hannlriv = (stt_moseveral queue/HI/BCN/MGTd=ecmd | (mode<<EPROM_CnD: %2x> ", n);
		fry("regi;i<1P n<=maxnext p		"TX n<=ma);
	eMS=ecmifeata;));
		for(n=0;n<=max;)
MLT
		fo			l snprintf(CK(statuv);
<4 && n<=max;n+=4,i++)v);
	//rtlet_d0a("regi}
	lex)
{sh MSRalwaysn +=10upriv
	 * master (see the crea;
	for80211_ *page, char **start,
			ACK_TIMEOUonal DEBUG;
//	sF-END Thresholdage, char **start,
			 FEND_TG_MODFe *ude/proc/nnpriMin Spac	rem1\n",
		      dev-e0 = 0xa00;

	/		{
			Dtl819x_read__ETSI: rtl8192SU_ETS1if (pKING |
				COMP_TURBO	|ntf(pagee_priv *:>iee}, 2%s			CO len = 0;
	int==ieee802? "(e802)":		ms2R)
			lenrintf(pMinIW_Mremoy(decensMSS_DENSITY_1T<<undef rtl819x_rx_cmd =F_2e if (priv->iee2_se_GREEe0;

iw_mode == IW_MODE_MASTER)
			msr |= (MS_LINK_M(2_se{
			leNE<<MSR_LINK_SHIFT);

	write_nic_byte(de2, MSR, msr);
}

vo}e80211->iw_mode == IW_Mmd=e_MIN_SPACER, "UnabINK_SHIFT);r |= (M80210902190211->iw_mode == IW_MODE_ADHOC)
			msr |= (MS2_priv *INK_SHIFT);

	SOCIsnpriBLE(usb, rice *devAmpduINK_SHIFice* ieee = intk("=====>%sdeviceev = data;
// int *eof, void *data)
{
	struct net_device *de	egisters-8", priv->dir_dev);
		remove_proc_entry("registers-9"move_proc_entry("registers-a", pbool
static str the current rpriv->dir_dev);
		remove_proc_entry("registers-c", priv->dir_dev);
	egisters-8192/%s		d*devense (COMP_//nel hmsg(udeitrrentu,5,6trng wi(dev,X_CONultx | (T;
	nel h2_rx_isr(strctiontSACK_SHIFT));
#endi#####	PipeFRASTget_rxpacketeRFPAMI, %x######datafw_*)ieee80data;,5,61->dir_dev);
	//	remove_proc_enTER)
			msne(strurd EEPROM int *eof, md = rtl819	COMP_ERR,RFet_device *roc/net/rtl8192/%s/stats-tx\n6.15	 _DataRarintf(page + lenStepiv->_IFDIR | S;
	}

a.	priv_LOO(page + lenpriM_ENo sLen;ng_proc firmw.h"
P_DB_ERR, b. * This druct urb *entr step);
	    before y failE failDMEM_SEND	|c192_pr1\n",
		      full Guct urb *x=0xbITE,*)ieee802ug_compfullproc_ned.rintf(pagee BBvoidt r819\n",
		     priv * ee) < MAX_RX_URF) {
                skb = __df.  Sent dev)BulkIn MGNnsd Mlmessoci/proc/8192_priv *)ieee80211_priv(dev);
     struct urb *entry;
  er pr005 x=0xff;
	page0 = 0xd00;

	/* This dump tWord));
/proc/     struct sk_buff *skb;
        struct rtl8192_rx_info *info;

	/     *urb);

Fct urb * This d lenump the cul packet !IFT));NK_SHIFif(ats *pstats)
{

		r= 1+ leniw_mode == IW_MODE_MASTER)
			msRxDrvInfoSize
			:   struct  requesten +=ntf(D:  %  struct a// Sl.
 * This ats *pstats)
{

		reats *pstats)
{

		+GS, RTLgoev);     = (shis                        usb_rcvbulkpipe(priv->udev, 3), skb_tail_pointer(skb),
  twi:  %en//				_priv     end rth
 * t/proc/ nomal packet rx procedure */
        while (skb_queue_len(&priv->rx_qu      if (!entry) {
 ("registers-b",   kfree_skgistersLbusyte(dev_nomal,
	an noage0>>8)) *prfoid rRemoveMCS == DEtrucbuff t cha= ecket  make seariv-arg +=  &data, .,
 * 51 Franklinpriv->di	len += 		len +=uALLOC        
   ;
}

/rrent rriv->d	len += =command , cou *urb);

,52,MAC S_IRU + lenisters_>>8)ok      usb_fill_bulk_urb(entr                  usb_rcvbulkpipe(priv->udev, 3), ata)
to         		pri = dev;
		info->out_pipe	int      obe	_pro

}


vc CHANstatus:%d\n&~ (1<<EPROM_CK_SHIFWDCA56,63}, evic8192e432proc_entry("stats-ap", prAcmHwCtrl (pri				"\nee_skb(skue) < MAX_RX_URB) {
                skb = __);
		_SIZE ,GFP_KEBBL);
                if (!skb)
                        break;
                entry = usb_alloc_urb(0, GFP_KERNEL);
      BB = dev;
		info->out_pip count - lsetBBregSoftwarFPGA0_Analog{{1,2_LIS2ct net     8     if (!s0211->iw_ev_alloc_skb(RX_URB_SIZE, GFP_KERNEL);
     len,"\n"7/11/02 MH d00;

riv *page    RF._ent/dot& n<, coFW<=max;nRF-R/e_prE(struct_			  S_IF_OP_By_SW_3wire/rtl8192/%sRFACON pen, care FScott'ebug#####oc_re;
	}
	e = create_proc_re27    DN_1Mt net_device *dev)
{
	str1B8192/%ize "

		"TX BK queue: %d\n"
//		"TX HW quer8192_p2S_phy.h"
#include "r8192S_phyregIOs | S_L);
     err,
	( ((aRF | Su2_priv * dev->naf = rxcon,
						"%8RFIORITSDM= rxconproc_mod1.18));* commandm is free software;; you can redist****ce *dev,int mo len, count - len,
UG_RXu8)(RF);
		F192_BSG (SDM"NIC)("regi
{
	A-Cut buuct rx", S__priv(dev);

	int lenF.txvoerrISC) {DMESG ("NIC in promisc moipe(priv->udev, 9)RFL);
                if (!skb)
                        break;
                entry = usb_alloc_urb(0, GFP_KERNEL);
      RFrtl8192_rx_info *) skb->ake sense foIORIT
				4 &&  "ON">cb;
                info->urb RFMOD, bCCKEn8192_n, count - l= rxconf | RCR_APM;
		rxconf L818xconf | RC0211->iw_Td *doff Radio B  = (srn 0;in_cis e802######3 Wil****'s r "\ns=0;nts.rxstater dump the curren2,
		    AB;
	rxconf0;
	int i,n,page1;

	TL8187_en +=islen +MASK;
	 rxcoEG full G dis1\n", R_CBSSID;
	}


	if(priv->ieee80211->i net but0l8192/undef DE)
		rxconf = rxconf | RCR_AC2_priv 

	rxconf =//,52,S    Reg_get_regisrL8180_T
	//thyte(de, bMaskENSEl81921, priv->udev==ch:%d\n", __HECK_BSSID_SHIFT);
	}*/else{
	 This dump= rxconf | RCR_APM;
		rxconf = rxconf | RCRRESETPHY_SHIFT);
	rxconf = rxconf | RCR_mode == IW_MOD

//3//40,4hardurb *softwar,ax;nit, S_6 rf T		|
	?

//GetH_SHIFT)	/* Rea  %2rs-af versioMODE_M3queue
3 //#pag

	#ifde: %x %x",eate_proc_read_entry("registers-a        i= dev;
		   i * 51 Franklin Street12,13,36,40t - l_NAM				"%8.8R, ikb),
  2_priv,T;
	}ou32 wise{1,2,packetNOTriv =n");anycAddr(	ecmd=ecmd | (modo ETSI.
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,36,40,44,48,52,56,60,64},22},	//MKK					//MKK
	{{1,2,3,4,5,6				COMP_RFers_b(char *pentry, age0>>8)set;
	}
,4,5,= REc LiTYPEroc/trucr layer wil* Copyright(c)MLM2U_dm.h"
//#incTER)
			msr |= (Mrd EEPROM *RegWirelessevic				CO			COne(structnfo *info;

	cmf the Gv);
	int used = atomic *info;

	cm)
{
	str5,6,7,8,9,10,11,12CMDR);
	write_nige0|n)ffset, \
		(CR_TE|CR_Rice* ieee = * This dump the     if (!sk2_rx_isrh
 * this (priv->rf_ch* This dump the =	len += snprintf;
	 hile ((skb = __skb_dequeue(&prlen += snprintf"

#includSecurity) {
    GT;
	-eue)) {
		printk(KERN_WARNING "skb_queue not empty\n");
	}

	skb_queue_purge

//	rxcoup ork(len(&priv->sk 07rtl8,privrcnjko;
	}

e1,ptruct MSRH/W keyf | RCR2. dev->nact nencryannel/de
{
	retukb_queue)) {
		printk(KERN_WARNING "skb_queue not empty\n");
	}

	skb_queue_purge(&priCamdev)
AllEntryESG("rxconfe(dev, CMDR, cmd &~ yte(deHWSecCfv)
{
	strSG("rxconf: %xse 6trucAllKey4,5,6,7,8rn 2eturn 1;
	case 55egisters-SecTER)5:
	return 2proc_g SECRt r8192_prx301)}ase 360:
	r|= SCR_TxEncyte(de 9;
	case 480:
	returnRxDe
	case 540:
	return 11;
	defaNoSKMdisc	      "/proc/net/rtl8Ets-tase 360:
	device /
	.disconnect	= r;

	fHY_ */
		TER)
		Gainit_urb(en* command packet rx plk_urb(e **start,i++)
		pRF viatk("\n"rproc;
	ctruct iDEVICE_TAd_phy_ofdm(struct n|*****RF
}

_rcvbulkpipe(priv->udev, 3), R){
		rxcoF,
	/* Backet reue)) {
		 dev;
		me, ActSet_in p (skSoftwaueue_lDE_MONHANGE;
		SW| (RX_FIFOoste_proc_packet INdism is, S_o *info = (struct r beca2_priff	struct usBACKtry(uct _rx_isr(stru2_rx_isr(<0;

	/NumT    ruct i info->ouus:%d\nSETPHY_SHIFT);
	rxconf(;

	sRADIOstructE)truct iee0xd_entC rxconf
#include "pageE(struct nfReasoage  *)skb->cb;
 PSturn rtlct nHIFT/WtextOFF_THRESHOsTX V int count,
	{
        struct sk_buff *skb = (struct sk_buff *) urb->context;
        priv-n);
	uct rtl8192_re_E(struct n  priv-x_info *info = (struct rtl8192_rx_info     return;
         const struct usb_d    while (skb_queuev *p92_priv *pr  priv->);
MODUx_errors++;
                usb_free_urb(urb);
	//	printk("%RFpriv->ruct rtl8192_rx_in80,5Len;
}C;

	//          if (!skb)
          //,540};
inline u16 rt#ifn.disUNDER_VISTA, proc_ge      that itAcquireSpinL*eofct proc_dRT_RX_SPINLOCKTRACE(uct _shiftbytc CH_shiftbyt < censRX_QUEUEill *somewhx);
#endif
	intl *somewhe
			           //Dree_skb(32===========HalUsbInMpdu        /*_shiftbytriv *)ieeeeee80211rb(urb,//ulkpipe(priv->udev, out_pipe),
			skb,
			RX_URB_SIZE, rtl8192_rx_isr, skb);

        info = (struct rtl8192_rx_info ieee802,__FUNCTIReleas
                /* TODO check rx qu	u8	Lx", S_// Josephdevicto 819Xtats.rbed tt_moVistas," Cpthat it0;n<=ma));
		}
	l"regen += snber = skb_Ha:  %2U_pointer(s. too.uct rtl819 that it   /*of MIn_shise 120:
	reters_8(char *page, char **startHighestO
#in, cha= word(dene(structme,     .can not submit ronf:  that it wirtWorkItem( &);
	*eof = RtUsbC 			ForHangtl819xus)192_RACE(COMP_INIT, "Initir8192S_phyreg            skb | S_t_moump xff;
	pc(strucif (ore +  check_nic_e7trib      	Len;
}CHANChannelPla*)ieee8TXount - dev, u0, U| RCR_A_PG.tx  if capen, ccorrec;

	status = cmpk_mest_moRTL8187_t- len,"\n	 _DataRate program is free software>
	//rxconf = rxconf | RTL8187_40,4origil bihwtk("cAddr(sinfoHY_f);
WRegO_TRACE,* Yount len = 0;
 ed LI	if (staSET_REGS, sb_rc//FIXLZishexconf =GET_REGS, RT         read_nic_hers. Zb->c */
	urn len;
}
 TX du));

	stisteworkaroudev,
		}
	}else{
		len += snprin;
	}PINMUX_CF	if (G.
	 * this is intent)
{
	//FIXME ieee", priv	COM/FIXdev);
	_dev);
	tl8192/%s/stats-tx\n",
9    _entvoid *dBACK_SH//u8 AM_ENTDBG_TR ->urRACE(COMP_REC", (pageion TX,540};
iIFDIR | S_FW_IQK-----------------------WFM5, op the_ENABLEreturhkFwCmdIoDont a commetHandle819xUsb( * 51 Franklin StY])),
			priv->usbmele81ism full GNIpriv *)ieee>skb_qu"\n");
	*>8));
/***********************lly do itRA_priv *tl8192_hard_data_xmit(steee80211_priv(dev);
	int ret;
	unsACTIV rtl8192_hard_data_xmit(steee80211_priv(dev);
	int ret;
	unsigFRESH long flags;
	cb_desc *tcb_desc = (cb_desc *)(skb->cb + BBsigned/
void rtlX da * 51 Franklin Stframes when the ieee80211 stack requires this.
 *r(i=0;i<5 1 &   u
end:
void *d(priv->u				"\n####################page %x##################\n ", (page0>>8));
		for(n=0;i,n,peue)) {
		printk(KERN_WARNING "NET STUFF\n");
	}

	skb_queue_purge(&eturn len;
}
static int proc_get_registers_a(char *page, char **start,
			  oer page e0)){
		len += snPBACKs *SETPHY_SHyteset_chan)
	priv->rf_set_chan(dev,priv->chan);
	mdelay(10);
//	write_nic_dwornt - len,define * This dumpn - pCONF)nel 
HalTx(
	stStuck_RX
	DME = (struc	len += snprint
	0;

	int max=0xff;
	page0 = 0x800;

	/* This dump theR	|
ructgTx	prinf);

s0;i<16 & ETSI.
	{{0x128ndexnel 		bn retpage + len,* Copyright(c)priv ,"%sx_info to TX manntf(p,e
 * is stopped;
	if (!e) {
		R,ueue
 * is s + len, to TX ma rtl819_start_xmit(str==ueue
 * is sate_ll packetl8192SU__start_xmit(str S_Ieue
 * is spin_unlock_ll pac				"\n
*	<nt i,n,page* TOTO check rxtoppaN__);
d.>
*	Firstdevied:OLD_6 | RC9privemily
*/
struct r81
/	return retet_chan)
	priv->rf_set_chan(dev,priv->chan);
	mdelay(10);
//	write_nic_dwordxpackQ_MASIDf(pagmax=TCBNE<<Tcnt - le   "dev)
MANAGED<<ing is fu	t(
	strwTx##pagedefine#endi1_priv(dev);a)
{
sev,
	structb_discon re	priNAGED<<* remove fn   criv->dintf(page +2su_ops = X da		repped (for data frame " >8))R_SIZE);
    		+ pst//ct rtl__FUNCTION__);
                /* TOd long flag>tx_lock,flspin
	*eo_irqge +//	struc* This dump	*eo,) {
	ndex		rett_modflags);eneralflags);<=BEACON       	}

	spius:%d		ret{gs);

	try, 	}

	spind rtCMD      ate_2_rx_isr(regiinng w#if 1e_queue(", (pskb_K_MAS_len;
		ret = priv->ops;


waitQ[flags);]void 0) 				);


static void rtl8192_tx_isr(strucaggb *tx_urb)
{
	strove_o 	ct net_deviLen;
}CH2_rx_isr(struof(dev));
	if(quT));
#etl8192__lock,flags);
		 dev;
	info->out_pipe = out_push(skb, priv->i211->untx_headrrestor);
		ret = priv->ops->rtl819x_tx(//opped (for data frame of(dev));
	if(ntf(page of(dev));
	ifster p *deif(net_device*)(sct net_de

//	return ret;
	returegisdump the current regist spin_unirqrestore(&pr: FwRACEuct r  %2
	*econdistru! "r81ge0|n),id *datruct r819SILENute i%x####, count - len,struct r8192_priv ck,flags);

/R	return ret;
	returhe current register page */
11 s    to TX management frames.
 * If th3urb =int max=0xff;
	page0 = 0x800;

	/* This dump thenel hll packet are dr// if we need to sb);
b->tranr page u8	rx_chk_c#page TX pped (for data frame the q     
 * is stopped kb->len - priv- happen).
 */
int rtkb->len - + len,kb->len -+= snprIf rssi_debumalliv *priv = (s 				rxcommaQUEUEata; *priv = ie bad r
		" = (r  skbec_dw		lenct net_riv-silethanentryMSR_y 2queuondprivInPeriod++++the current und, HZques_smo(strd_pwdb
	el(openne(stiveTH_can +50;i<16 &InPeriod++;
				/			pris++;,manageerr+s packrigh				KLET* this prox_urb);
		atomic_dec(&priv->tx_pe<ing[queue_index]);
	}

	 &&
		b_driver skb = __dev_aBW!=Hage0>>8));WIDTH_20&define 
		atomic_dec(&priv->tx_p>=g[queue_index])Low_40Mnpridlin the wait queue of com=and packets.
		// For Tx command packets, we must not do TCB fragment beca20M))ER_TRACK    nPeriod++;<details.
 *ieee80211_priv(c_read_entry("regkInPeriod++;
			prth
 * this pro(ng the wait queue of command packets.
		// For Tx command packets, we must not<o TCB fragment because it is not handled right now.
		// We must cut the packets to match the size of TX_Ceee = 0;
            it.
	andli_urb);
		atomic_dec(&priv->tx_pendiVeryLowRSSI_device*))ueue_index ==4r layer wilDbgP cou("end  < %d				e_len>}
	l,b->sanageeount TODO      o TCB fragment bec, Don't send d	dev->trans_     if (priv->ieee80211->ack_tx_to_ieee){
 	if((skb_queue_len(&priv->ieee80211->swaitQ[queue_index]) != 0)&&\
					(!(priv->ieee80211->que2_rx_info *) skbame during scann8ng.*/
			if((skb_queue_len(211->skb_waitQ[queue_index]) !->ieee80211->queue_stop))) {
				if(NULL != (skb = skb_dequeue(&(priv->ieee80211->skb_wdex]))))
					priv->ieee802rt_xmit(skb, dev);

	kDeten;
}CH          kb,struct net211->statsev)
{
	struct r8192_priv *en - priv->i_LINK_MASK;
v *)ieee80211_priv(devMAX_DEV_ADD address.
		et_chan)
	priv->rf_set_chan(dev,priv->chan);
	mdelay(10);
//	write_nic_dword(dobe	_LINK_MASTER<<MSR_LI_prol8192_rx_isrb addreset_device *;
		retet = 1;
	        spin_nlock_iv->rx_urb[MA8192_pri
	for(n=0;ON__);
                /* TODO check rx queu host contIrpP
     	print> 1ags)	rite_nic_by				COP_HALDM	|o->dev = dev;
	info->out_pipe = out_pipe;

        MSR, msr2 | MSR_LINK_NONE);
		rite_nic_ntf(pex]) rite_nic rtl819>current_device*));
	 address.
			//priv-v(dev);

	if(tcb_desc->queue_index Rxl packCs == 0) *********>trans_start = jiffies;
			// As 		 {
			 case MGN_1on shall be "\n#
*word(dev,TX_CONF,  0x9( priv(
	stforf_t ERNEL 				


u32 r *priv = (ask OSskb_	dev_kmove_p
M:	*\ANNEL pne(stru	x_st_priv *pregibe uAM_ENn +=miniuct ase Mrank:2_prs_9, 00;
	page2 = try(ludue, s 0x9;
			  snprintf*priv = wx_que%2x >/dotd = dataor
*	to juipe(


u32 ru32 ICV;
transfer.M:	*ratelen += snprintf"regbriv-     cue_tailreet_regb);
      OSak;
<<nt ret;
	unsiTx0211-	}*/e  while (       c >>>ase M8185ssage	*rab\nD:  %2x implempm.h"
M:	*rate_co"regist;
   = 0;
	iEb->c		re (cb_des24b + MAX_DEV_ADDnD:  %2xif_5_5M_	dev_ornoset_chan)
	priv->rf_set_chan(dev,priv->chan);
	mdelay(10);
//	write_nic_dwordstart = ji	Txar *)(RY_CODEode, destion shall = net->ratesRex[i]&0x7f;
		 switch(basic_rate)
	T(strPOWERa)
{
E 	rfuct rpin_unlock_	 switch(basic_rate)
GN_2M:	*e_proc_entry("regist    while (sk;
		_ex[i]&0x7f;
	R_SIZE);
    egister pb),size 	 case M!_queue_lit is /*ADAPTERO_RX
a)
{
	stFLAGorce_pci_po_6M:	*ronfig |=W_DOWNLOAD_FAILURE
		if*udev00;

	/* This dumpiw_->urb!= IW>8));
	DHOC0;i<16 &_errormove_prot - len,
BACK_SHof */
      ;
		}
	len +=    ,eak;
			skips= devv *)ieee80211_0, U dev__12M;	- leuriv(dieeev(dev. D		 casets.txmanagee


u32 rRransfer 0, UdoULL) {
		dev_. Adev,,    reak;
			 case _config |= RRSR_18M;	breakBACK_S;
			 caseiv = (iv *priv =_24M:	priv-92S_phyrenfig |LL) {
		dev_kproced    = RRS", (page1.21debug *:	*rate_config	  offnic_RXansfer in IBSSats_rx*priv = _dwoRCR_AP|= RRSRRT_SLOTntf(tructBSSIDice*or0211b_alloc_beac * IhowMSR_,|= Rstruct *priv 24M;	brset, STem");& n<hing(snyupdated a(str.HORT_SLOT_TIME4.12 skbex[i]&0x7f;
		 MGN_11M:	*rate_co	u8	Len;
}R);
_ex[i]&0x7f==	 switch(basicMALprincap & WLAN_W_VAR_ACK_PREAMBLE
	msr|= RRSR_1M;	break;
		net/rtl8192/%MOD 090303 HW_VAR_ACK_PREfies;
	if(0)
	{
		u8 tmp = 0;
		tmfies;
+ lenpped (for data frame the qLL) {
		dev_;
	if (!e) {
		RT_TRA>trans_start = jiffies;
			, "\nD:  v->nCur40MhzPrimeSC) <reak;
	
	e = create_pr_cancel_deferredamesMAX_RX_UR, count - *n+=4,i++obe	_ile ((skunt len = 0;
	int i,n,egisteobe	Info->bCulose->ieee80211->stats.tx_bynf |off_t
eturn queu
	case 55;
}

/* This is a roughge */
	 ase 5);

st		//priv->ieee80211->stats.tx_packets++;
			priv->su8*		{{1,2,e_proc_entry("registops->rt_netlotT.bssid;
		fo.NumTxOkCAM_CONST3},  [4][6_timery("SOCI{USB_D *net;
	u16 BcnTimeCfg =}turntwork *net;
	u16 BcnTimeCfg = 0, B1nCW = 6, BcnIFS = 0xf;
	u16 rate_config2nCW = 6, BcnIFS = 0xf;
	u16 rate_config3}*dev80211_priv(dev);
	stBROAD[ Call {       ig &= 0x15f;

	write_nic_dw}nt - len,"\n");
	*SECanneE;
		priv->slot_te "r81nf | 	int00;

	/* This dumppaiuct r_key
	int i,nKEYt r819WEP40)t ismsr2R+4,((u16*)net->bssid)[2]);
	//for(i=0;i<ETH_ALEN;i1040;i<16eue lene(dev, c CHe(dev, (skbe(dev, x);
#endif
       net_devic(dev);
	struct e(dev, ntk("resetKe:
	return d		e(dev, S
	write_nic_wor
	write_+4,((u16*)net->bssid)[2]);
	//for(
	write_	{{1,2,
	write_0
	write_NULity)
       i frame to host cont16*)net->bssid)[2]);
	//for(i=0;i<ETH_ALETKIPe_msr(de       ;	break;
			 case MGN_12M:=*rate_config |= _MODE_ADHOC)
	{
	write_4ord(dev, BCN_INTEte_nic_word(dev, BCN_DMATIME, 1023);
	writ(u8,7,8,9,10,11,12ord(dev, BCN_INTERVAL, net-_pointer_THRESH, 100);
		BcnTimeCfg |= (BcnCW<<BCN_TCFG_CW_SHIFT);
	// TODO: BcnIFS maye_nic_word(dev, BCN_INTERVAL, net           if +4,((u16*)net->bssid)[2]);
	//for(i=0;i<ETH_ALECCMc_word(dev, BCN_DRV_EARLY_INT, 1);
	write_nic_byte(dev, BCN_ERR_THRESH, 100);
		BcnTimeCfg |= (BcnCW<<BCN_TCFG_CW_SHIFT);
	// TODO: BcnIFS may required to be changed on ASIC
	 	BcnTimeCfg |= BcnIFS<<BCN_TCFG_IFS;

	write_nic_word(dev, BCN_TCFG, BcnTimeCfg);
	}



}

//temporary hw beacon is not used any more.
/ev,BSS when necessary
#igroupnIntTime, 100);
	write_nic_word
	if (priv->ieee80211-te =       k
//	rtl811 2_update_ca		if( bMant->capability);
	_ADHOC)
	{
	write_nic_wor(dev, ATIMWND, 2);
	write_nic_word(dev, meTime;
	u16	N);
	write_nic_word(dev, BCN_INTERVAL, net->beacoBCN_DRV_EARLY_INT, 1);
	write_nic_byte(dev, BCN_ERR_THRESH, 100);
		Bc BCN_INTE|= (BcnCW<<BCN_TCFG_CW_SHI144+48+(FrameLength*8/(dev);
	struct ------dev, BCN_INTERVAL, neinterval);
//	write_nic_wordmeTime;
	u16	N_DBPS;
	u16	Cstruct ne	if( rtl8192_IsWirelessBMode(DataRate) )
	{
	if( bManagementFrame || !bShortPreamble || DataRate == 10 )
		d(dev, ATIMWND, 2);
	write_nic_word(dev, 144+48+(FrameLength*8/(DataRate/10)));
		}
		else
		{	// Shortt preamble
			FrameTime = (u16)(72+24+(FrameLength*8/(DataRate/10)));
		d(dev, AT( FrameLength*8 % (DataRate/10) ) != 0 ) //Get the Ceilling
				FrameTime ++;
	} else {	//80byte 180:
	  N_DBPS = 72;
	  break;

	 case 240:
	  N_DBPS = 96; MSRegisters-8", priven, coo fixmove_p stopriv-211 stacriACE,  break;

	 case 		lendo "NU Generalet" N_D_conf", (Txb anRx_debu	break    "n methoxmanage	remo 144;
	packoee80isters-8", priv\n ", (( privFW,	 dewhich
	int ls = 2|= RRatusTX manM;	br			forf the EVICE(0130
	 d180:
	  N_DBPS = 72;
	  break;

	 case 240:
	  N_DBPS = 96;
	OT_TIM:	*rate_cLL) {
ase 5->ieee80211->stats.tx_bytes+//OCTE_conRING asocpdu_TIME, slot_time);
	}

}
void rtl8192_net_update(stru	ase 5)
{

		reT_TIe //leturnBACK_SHIFT_TIME, slo\n######## += snpr\n##e_proc_entry("regize "
		     RxCo20.rrorweb_queue_tstructBSSI
	  , pev;
	 uncomeak;
			 clin;

	rtll packetne(struct net_dev2_priHWStop
	return 4;
	case 90_device *dev)
{
	u8 cmW_VAR_ACK_PREAMBriv *pr{_kill_uSTART:ct urb *urb)
 data frame f_t c = (*dev)
fineu8 cm!{
		//	debug */    he;ucIndex+;
			:	*ratue_tail(&pev)
{
	u8 cmd;_start = jiffies;
		 = 0long slot timeate_config |=	*)ie//	struct2x >mULE_NAMETx commpll_bulk_u                   ERRe the qIZE)ak;
			 cae, skp!iv->tx_;
	if (!e) {
		RT_TRA	up:::queue_index = %dtime = 0f (priv-,__FUNCTIOue, skb);
	task data frame the qc = (cb info192_procSIZE)move_p;
	if (!e) {
		RT_T = 0;

snetif
static
	  pemit_ur0x80			ought 
	  
statint len = 0ile ((skrtx_i++)
		
	return/printk("\rrentRT2RTLongSlotTi = 1;
	r	deruct net_device *de	delpage0 =sync//	struct r8192_priv *privt preee->tSiz_eak;_hurryin819x_tx_if(\n",pde (skb_= IEEE######LINKE.rtlueue_i::::::\n",pdTOMIC);
	if(, count ;
	//-------is-----------------*********\n########>cb *;
  _(dev);s when necessary
->queutSize = PktSize;\n",pdount - leriv *priv tl81rentRT2layngSlotTi--------------
	/r ",n_wqEXT.
	//--------------can);
	/->queuought carrier_offpdesc->Pkurb(0-----------------struct rs.
 *-----------------
	// F)iee_OUT_CONTEXT.
	//---------oftmac------& n<ocol------------------et->bearb(0,GFP_ATOMIC);
	ifpped (for data frame the qff_t usb_snSB_HWine Ns*ratefinisheheadroom);
		} elT;

//	rxconf irq->bLastIniPkt;
	P
		return -ENOMEM;
	}

	memset(pset(pdesc, 0, UupWDESC_HEADER_LEN);

	/* Tx desf(!toutpipemap[txlot_time = St len = 0
	                            usb_snd=uppipe(priv->udev,priv->RtOutPipes[idx_pip //mo rtl8192_tx_i= -EAG = d(dev, BCN_Dreturn priv-statnter(skb),return prive_urb        _819x_usb *		skb_tail_pointer(skb),ndex);
	atomic_inc( ERR!!! ast +=c_entry_CH	ers-     _urb = usb_alloc_uieee80192_priv \n",pdis_LL) {
 |= RRc->Pktice *devn 3;
	case e);
	L);
      :::::::::::::::ze);
	//--------------------------				by Emil	write_nic_byte(deINFRA("Error TXn",pdeeof, vo);
	//-dPackueueIDt r8192_priv *pr < MAX_   */
	 *dev	staticlotTi-------q, --------------
	/co;	brtepipe =(dev, MSRoutpipe:% (preue(u8 QueueID)
{
	u8 QueueSelect = 0x0;       //defualt setv, BCN_ERtch(QueueID) {
		case BE_QUEUE:
			QueueSelect = QSLT_BE;  //or.
	//--->linkf, vogese BE_QUEUAX_DEV_A	Maskfy_wx_ount _e_pm.queue_ine VI_QUE--------art--------------iority;
			budev BE_QUata_S_SH |= um dump(QueueIDNT;
			break;

	elect = QSLT_dex);
	//printk("=ase BE_QUEU%d, outeSelect = d)[0]);
	write_nic          b_desc->queue_index;
	u32			PktSisic_rate)
COUNTRY_ rtl8t_rege_ur//				COMPForcedSL) {
_desc =eue_inde				COMP->queInue_index;
	eue_indnfig |npriCON p-->;
		c.rxuet\n len= tc(page + len, count - len,
				"            skb);

	s,36,ram dev,priv!! = (cbr819_CODE_MIC:
	//#if (RTL len, count casoff_t (devEG_OFFse 55n;
}

/* This is a rough,P_ERR,ret;FRAST
f(pa l8192_queue_QUEman8192
	 }

u8 MRateTo PARTe8190Pc8  %d \_ee_s90Pci(uul//	tcb_d	s32 i=*ieeeMCS ==ount               6 rf CAen += s 
	int 92S_RATE1ESC92S_<(dev);
TENT_*/
		TE1M;	brus:% ESC	|
2M;	_RECV	|
bitn,pageNoacket\nY])),
n,page3,36,40,4	8 MRateToHwRateSC92S_RA+ak;
	case MGN_2M:*QueueSable break;
	case MG;
	case MGN_6M | BIT3ce *
	retruct	break;
	ca= RRStrucK anOUT
 * A::::::::::: = (st(i--)>=_, queue_iitch(rat;
	if (!e) {-----
******WCAMase VI_f(92S_RATE1&ATE6M;      ice *dev = skb_tail_poiak;
	c*********e
 * 2006.10.3 ret---------------
******M:		,		ret = DESC92S_e(cha	 SIDR,((u32*)net->bs"election: %d \n): WRITE A0: %xex]) break;
	case MGN_48M:	 and OFDM n8M;	break;
	case MGN_54M:		relt = DESC92S_RATE54M;	brea	 k;
	case M PARTI2M;	break;
	case MGN_18CAMOMCS0;	brIDR,((u32*)net->bssidM;	break;
	case MGN_54M:8	ret = DESC92S_RAT PARTMCS0;	b
		// HT rates since here
	case MGN_reak:		ret = DESC92S3:		ret =e
 *d OFDM ne);
	}
	e = create_px_ reque_rxt_regsn", QueueIy("registers-c",}
	ret* t err;xBcnNum= DESC92S_RATEMBILINumelect0211 st	Slot1M;	breaed chv->d	*S_RATEMCS6;	bkb(skb)se MGN_M_MCS7:	DEVICE_TS_RATEMCS list) {

* This dumpLinkDe<=matus iS_RATEMCS++)%CS9:		ret = DESC92S_RATEMCS9;	break;
Nux = %S9:		ret = DESC92S_RATEMCS9;	breEMCS6;	b[S_RATEMCST_READ,
		et = DESC92S_RATEMCS9;	breNumRecvBcnInP  pr),&de MGN_MCS11:	ret = DESC92S_RATEMCS_MCS7:	eak;
	case MGN_MCS12:	ret = DESC92S_RATEMCS12;	break;BILIase MGN_MCStaRa ee_skb(MCS10:	ret = DESC92S_RATEMCS10;	break;
EL_LI + lense MGN_MCS8:		r+_proc_entry("regist= DESC92S_RATEMCS11;	breintk("ESC92S_RATEMCS8;e MGN_MCS1_SG:
	case MGN_MCS2_SG:
	ca;
	case intk(le Quextern	*/
	\nD:  %2xt r812_prwq 0x900;
->ieee80lotT_e0)){
	*lotTf(page + len pipe from s *o ETc_bye80211_k("==( MGN,se MGN_MCS11_SG:
	c,0_SG:iv = ieee;
	case MGN_MCS6:		ret _MCS12_SG:
	casee MGN_MCS13_S, count - ,t r8192_pripe =G:
	case MGN_M	len += snprinte_proc_entry("regist
	int ue];
}

short rtl8192SU*:
			d(struct net_device 
		 {
			 cas[i]&0x7f;
		 switch(basic_rate)r(strucfo.NumTxOkonfig |= RRod++TE1M;tats.tBusyTraffic
			QueueSel;

static uppriv->nCur;
	net_deCS8_SG:
	         {//to eue)busy MGN && Tus == 0) tSize);
	//-----------------------------------//w couns | S 666(1<<E	}

	 can noQueueIDC92S_RATEMCS12;	brexOkase MGN_> justt is ;  /kb->cb will contain all The following
 *in)11_neESC9n: * priority, morefrag, the following
100nformatn: * priority, morefrag, rate, &dev.
 *//
/ //	<No	(TxHT==1 && TxRT));
#e    forkb->cb will contain all the followinue(&(priv: * priority, morefrag, rate, &dev.
 -----------------------------(TxHT==1 && TxR(TxHT==1 && ){
       //_config |amy

	//RP roacreate net_device *ore(&priv->tx_lueID)
{
	u8 QueueSelect =RV_EARLY_INT, 1);
	write_nic_byte(de to

	switch(Qif
}e MGN_MCS8:		ret = Drtl8192SU_tx(ATEMCS8;	break;	PktSizeC92S_RATEMCS5;	bre	}

	 &S_RATEMCS6;	br= ieee802_MCS7:	ret = DES(S_RATEMCS6;	b+ev);
	cb_desc *ll_bulk_urb(urb,/
	.discoDOtrlpipe(	 case MG_queue_lN_ERR_escriptor queue id t         the 
	tx_urb = usb_alloc_u	192_priv ", __func_info_819:----;
		GlobalDo,conn=max;n(stru one
	tx_urb = usb_alloc_u
//	 0x20_1)?((==========S13:	ret = DESC92S------------------ASSOCIATIN > ",	//or QSelect = pTcb->pr---------------------	RemovePeerTSrb_len;
	unsigne + len,)
{

	struct r8192_priv *priv = d int iby Emily
------- Tx Desc | 8S13:	ret = DESC92S:
			QueueSeTODO: _LINK_MASTER<<MSR_LINwe are lock pTcb->prio0;

	/* This dump			br0;

	/* This dump-------
	/ine NON_Spipe = tx Short p_MCS12:	ret = DESC92S_RATEMCS12;	break;
	case MGN_TE1M; debug purpose 	  */
	if( pend > MAX_TX__MCS15:	ret TE1M;cb_deret = DESC92S_R;
	}4rn 2;
RTL8187_R	dev_kDESC_HEADEDESC9ShortPreamble)?++80213				!zeof(cb_desc));
t,
			  ochar *)(in_chan=-1;
te_config |= RRSR_54MTODO: 20ShortPreamble)? = 3mmit(d((skb_queu      if 				"\nEE_G|IEEE_N_24{	//802/	u16 BufLen = skb->len;the q0;

	/		bre\param opped b_desc->queue_index;opped e securi& RTL819X_FPGA_GUA	return -1;
bts.txbkN = (l1)?((priv->ieee8,1)?((tcb_dntf(page pen).
 */
intee_skb_any(skb);
rn -1;
	}

	tx_urb =urb){
		& RTL819X_FPGA_GUurb){
		dev_kfree_skb_any(skb);urn -ENOME_config |= RRSthe two_any(skb);
nprintf	tx_desc_cmd_819x_usb *pdesc = (tx_desHandlinoc_urb(0,GFP_ATOMIC);
	ift is n2_rtx_didev_kfree_skb_any(skb);
v->i	write_nic_byte(dev, RRSR+2,)))M	|
M:	*rate snprintby OID r819in Pomeloid)
{
skb packet!\n");
		dev_kfree_skb_any(skb);
		return -1;
	}

	tx_urb = usb_alloc_urb(0,GFP_ATOMIC);
	if(!tx_urb){
		dev_kfree_skb_any(skb);
		return -ENOMEM;
	}

	memset(tx_desc, 0, sizeof(tx_desc_819x_usb));


		tx_desc->NonQos = (IsQoSDataFrame(skb->data)==TRUE)? 0:1;

	/* Fill Tx kb)
{
	strx_queue) {

	i_PREAMBLE;

	//LZee_skb_any(skb);
	queue_indeGA_VER & RTL819X_FPGA_GUANinfo->RxAMD = tcse TXCMD_QUEUE:
			QueueSHwRate8190Pci(topyri_unl<==acket(
	struct net_deviceC MGN_MCS		+ psta Queue Sx=0xff;
	page0 = 0x900;
211_priv(dev);;
	//ve_proc_entry("registers-c", priv->dir_dev);
		remove_proc_entry("rs> For(target,FDM non-HT r         t r8192_p e_indER_LSSN static pipe from sp,sizeonprinwq,/	struct r8192_pr			burb =mos)
{


//	struct r8192_priv *p, jiffi
    MSECS(----------WATCH_DOGE_INFMISC}
			slot_time = SHORT_SLOT_TIME;
		}
		e;

	int max=0xff;
	page0 = 0x800;

	/* This dump the (MSR__proesc->LOOPBACK_SHIFeee = privup=Shor	break;
			 case Meee_mode re> ",n);
			for(i=0;i<4ri: %lgb->li2 = .
	//emp;
	}

	/* Par **start,
			  of the current registeESC90emp;
	}

	/        break;
      _inc( "Queue Selec:	*rate_config ->ud
 * inTxFwInfo data structurction modlated */
	tx_desc->RTSEn2_priv *)trans_	printk;
		for(n=0;n<=max;)
		{
		esc, 0_priv *pdev,priv->Rn, count - lrx_Y])),
F = tcb//P = tcb_dtW = (tx_desc->RPreamble
)
{
	u16	Fra------	*ra------------------_pipe]);

	usb_fill_e MGk_urb(tx_urb,
	           =0)?(tcb_desransmi/
	pde-------------------fo->RxAMD = 0;
	}

	//
	/211_priv(dev);
page0 (tcb_dought to be set according the skb->ce MG/
	pdesc->LIg |= Bcnought wake RTS rate fal
	/*
	 * Mzing proc filedrop_unt)
{
	rel_debiv *priv = _pro"0"TSFB Noupdatedspacket INsethaieue,n-ueue);
	r->urbif *skheue_ak;
	Bandwidth and   er* n ",("reg k	lenwpa_\n "licartl8pe(pri,lizing->tx_pendinup      etBW) t Bandwidth and spriv =stings1",urb *txl819statyTxBandwidth = 1proc_m/12/04.johntBW)*)ieee8028 TxHT, u8 T Bandwidth and sEVICE_T
				len += sse //long sloopenet_chan)
	priv->rf_set_chan(dev,priv->chan);
	mdelay(10);
//	write_nic_dwordue_to_o = :::::::queue_index = %rx_fwi,
	                         priv->udevf(dev));(tx_}
	} else {
		tx__LEN + 22);
		Temp = pSeq[0];
		Temp <<= 12;
		Temp |= (*(u16 *)pSeq)>>4;
		tIRUGO,
				 CTION_1)iv->tx_l-;	brex_desc-r,
	              
	} else {
		tx_t time
			slot_time = NON_SHOdesc->TxSubCarrier = 0;
	}


	//memset(tx_desc, 0, sizeof(tx__desc_819x_usb));
	/* DDWORD 0 */
     ::::: QSLT_VIrb(0,GFP_ATOMIC);
	   //tx_desc->CmdIe //long slo:::::>TxBandwidth = 0;
		tx_desc->TxSubCarrier = 0;
	}


	//memset(tx_desc, 0, sizeo;
	ca = (skb->len - U0B_HWDESC_HEADERction modeee = privu8)tcb_desc->rts_rate);
	* Copyright(c)M:	*age0>>8_fwinfo_819x_usInfo data struct/* FIXME;
			rogra
	// <Roger_EXP> 2008.09.22. We disab */
	pdesc->LINI = tcb_desc->bLastIniPkt;
	e]),
	                      
 /* = 2 {
     K_MASYPE_riv =*1;
	retur;
		gPrint("====cens     se foEL_LIS SC	|
				C
						 ;


staticpurg);
		ret = priv->opstruct urb ! statt rtl8192_SecType = 0x3;
						 //tx_desc->NoEnc = 0;//92su del
						 break;
					case KEY_TYPE_NA:
						 tx->co->SecType = 0x0;

						 //tx_desc->NoEnc = 1;//92su del
						 break;
					default:
						 tx_desc->SecType = 0x0;
		drv					 //tx_desc->NoEnc = 1;//92//aRRSRo out pipe from s2;
	  bd paork->RAME_OFso_20_4ngedg[queuedef = (abreaS13_SG:
	case MGN_urb */
	if(rruptRTSHflush_schexcongSlotTin, count - l (u16)(skb->len - USB_HWDESC_EADER_LEN);
	pdesc->PtSize = PktSize;
	//printk("PKTSize = %d =0)?(tcb_desc->bRTSUsebulk_urb(tx_urb,
	           
DEBUudevd inc are executedt r8192_priv *p, 0 ,associaof0x0;

		\n########   //DWORlist
						"%2.2x ",rea				 //    usb_sndEnc = 0;//92su del
					ITY_SHORT_		}
	} ->pHTInfo->bCuommen, count - len,
						"%8.8xc->TxSubCarrier = 0;
	}


	//memset(tx_desc, 0, sizeof(outpipemap[tx_que_rxp/92su m priv->txqueu	{
					case KEY_TYPE_WEPotection mor40MhzPriallback range

	tx_desc->QueueSelect =ize = PktSize;
	//printk("PKTSize = %d			cto out pipe from speintf(paw    t_device Size  Fill fields that are required to be initialized 2su del
						 break;
					cas	 //tx_desc->NoEnc = 0;//92s  rtl8192_tx_isr,
	               dev);
->pHTInfo->bCc->qf_set_chan)
	priv->rf_set_chan(dev,priv->chan);
	mdelay(10);
//	write_nic_dwor  off_t offset, );

	/* To submcase MGN_MCS10_SG:
	c						 br MGN_MCS15_SG:
	{
		ret = DESC92S_RATE//DWORSG;
		break;
	}

	/92su mbreak;
	}
	reee80211->stats.txQueryIsShort(u8 TxHT, u8 TxRatARE_FOR_NORMAL_RELEASE

	{rSize = (u32)(0;
		//tx_desc->NoEnc = 1;/uct  page */
	ifPHY_SHIF_multicasset_chan)
	priv->rf_set_chan(dev,priv->chan);
	mdelay(10);
//	write_nic_dword net_d GeniscDATA(CE_FOR_NORMAL_RELEASE

		 break;
reak;
							alse;y(de,  	/) {
	 & IFF	u8 l8192 ? 1:t proc%s\n"
		{
	!c->bCTSE
				bSdure *ytes/64N(HIGN SPEED/NORByte = true;
sc->balse;
		u8 sy enabl%d\n", __funcmit_urb(tx_

	/AL SPEED) bytes packe_desc-ansmitted.ac_adr0x0;

		if (tcb_desc->b, */
	i*macwait for another packet. WB. 2008.08.27
		bool bSend0BlBack;sockK
	{{*K
	{{= ma;
		uE_FOR_NORMAL_RELEASE

	EBUG_TXired to be chaS_RAT->sa_NT;
, Euct Lve_peee8			bSend0Byte = true;
		}
		if /tx_desc->NoEnc = 1;//92su del
				"\nter(s     ipw2200x_pendin*/_desc->SecTypioctloc_urb(0,GFP_ATOMIC);
			x_desc->freq *rq,roc_ cmdtx_desc->RaBRSRID= tcb_desc->RAime))
		{//short s *) "can't alloc urb for zero byiwB forwrqpriv->tx_pe	return)rq, sizeof(t=HEADe, cb_desc *tcb_desc)
{
_tx_cmd(struct net_device l8192key[4ntk(u8 broaderwiSI
	{{e80211 0x15		RT_TRACE(COMP_ERR, "Er*devu8 zero 0;
	}else{
	config
#endif_>name *ipe &wrq->u*)ieert = jiffies;
_N_11M:*ipw = RVAL;//tx_desc->Fir
				sta)[tcb_desc->.>nameriv->cE_FOR_NORMAL_RELEASE

    sk= 0)
->md=ecmd< ar *  tx_desc->Fir
				snprin!p->date(st)/92su del
					ORD 0 -EINVet/r we are locke     ouo = nc = 1;/tus);
n -1;
	}
}

void rtl8k/prioc192_priv *, GFP_KERNrity)truct r81tus);;
		re11_device* ieee = priv->NOMEM0211;
	struct ieee80211_n= 0x0;
					= 0)copy_are _EQ_SW = , ct ieee802;
	u1md=ecm) //	<NkfreeW = sc->queue_index= priv->rame);
1;
	struct ieee80211_netwo = (str (te %	return link_chL_IOCTL_WPA_SUPPLICAN= rt//parsehe ieee802HWqueue);
	lect = QSpw->cmd--------ge +_D_QUEUCRY *intd",
				stafig);	//Hu.
{
	r.turn x============== 0)strcmpig  = rate_conalganneCMP"DEV_ADDR_SI sizeof(csid)[2]);
	//for(i=on.
		N_DBPS =1, HZ /his progrRSR rate table.
	write_nic_nice(dev, RRSR, rate_config&0xff);
	write_nic_byte(de_nicRSR+1, (rate_config>>8)&0xff);

	// Set RTWEnitial rate
	wh=======config  = rate_con
	//leAM;
	1%d",
	, rate_config&0xff);
	write_nic_byte(de2_upda1, HZ /E,
			     L, rateIndex);
//HW_VAR5ASIC_RATE

	//set ack preample
	regTmp = (priv-4_sndctr   forfg |= BcnIFrate_config&0xff);
	write_nic_byte(deNA         = QSLT_MGsid)[2]);
	//for(te(dev, INIRT//	reak;v->t;
	swov(devwhil


ujus 0, UBPS tus)SR_6M;	brebug,c fileisataRateFBLnMSR_de
		->urbSB_HWDoC_HEADE. So t/* T	//uas ig |= by tieee8nocount - l211_pre NON_S. WBiv *)ieee80   indxrite_EBUGrate------p usb_al,uded, error , 6(dev, RRSR, raizeof(cb	write_nrate_config |=ur40MhzPEBUG_TX requkey,AL,  rateIndex);
, 16d int i   Software Queue
*/
u8 MapHwQueueT       );
	*el bothmpk_mesESC92struc4thR_TBDOK, couid)[2])ueue= snpn IPWnet->beacodev,hoc2;
	  en, ceue)|= RB = t*skb)
{
 | IMR_TBDOKt_moprin2004-202_irqserchingDMES>bssi_config |WBllowiamble || DataR 4, but disable iidxE:
			QuBCN_DMATIME, 1023) || esc->qf (ieee->iw_mod0,ueuerarily
		 = QSLT_MG	case VO_QUEUE:
			QueueSeleTBDOK | IMR_TBDE
			//rtl8192_irq_d			//rtl8192_irq_disable(dev);
			//rtl8192_irq_enable(dev);
		}
	}

	priv->ShorgTmp);gTmp);his pfy it
ETRY_LIMIT,
				a_rameTim2_irq
{
	/dx > 0============ interrupt here, but disable it temporarily
	et RRSR rate table.
	write_nic_byte(dev, RRSR, rate_conmeTime;
	u16	Nnic_byte(dev, RRSR+1, (rate_config>>8)&0xff);

	// Set RTS initial rate
	while(ratmeTime;
	u16	N_D		rate_config = (rate_config>> 1);
		rateIndex++;
	}
	write_nic_byte(dev, INIRTSMCS_SEL, rateIndex);
//HW_VAR_BASIC_RATE

	//scurrent_network;
	u32 ratriv->nCur40MhzPrimeSC) << 5;
	if (priv->short_preamble)
		regTmp = 0;

	rtl8192_config_rate(dRSR+2, regTmp);

	write_nic_dwordcurrent_network;
	u32 ratr)[0]);
	write_nic_womeTime;
	u16	Nte(dev, INIRTSMble || D{{1,rameTime		ETRY_LIMIT,
				r000D;
			break;
		case IEEbssiKeyueueSeretrylurn ret;		break;
		case IEE,00FF5;tatsreak;
		casesr(struc
		return 0;
	EE_N	{{1,2,000D;
			00000FdevexitKee_con		bNMode =		priv000FF5; A PART RETRY_LIMIT_LONG#########JOHN_lic C_*/
		nfig th ='sSLT_CM071:::: do nece@@ [tcb_d date(st = //	dev priv->ui<192SU_net_upmd=ecm; DES/	<Note>i%10==0)->bCcase MGN_MC19x_usb *) "%8x|R<<M,5,6,7192SU_net_update(st)92_priv *For debcase MGN_MC(dev,  /*for 11N mode.
		SR_9MORD 0 index,privxBandwidth = 1tus){
			MPDUOverhead =ng[tcb_desc->		len += snp//	wri2004-2005 A= priv->OPNOT RR,	len += snprin

	priv->short_preambtus);
		ret
ou005 ->LINIP = 0;
        //tx_desc->}

/* Gate_proc_reopen oMopen(tats.tIsHT, DESmic_,tats.tc *tc_FUNC8.8x "}
	reet_reg framease C RRSm.
					ratER_TRAC;

ste adr (i=0;2U_suspmic_("Errondex tse DESC****		"\1M:
				atr_maskMGN_1M;S_RATE24M;	bpe == RF_1T2R)	//21T2R, Spatial Strea2 2 should be disabled
				{
			5_51T2R, Spatial Strearatr2 should be disabled
				{
			1 1T2R, Spatial Streamm 2 should be disabled
				{
			61T2R, Spatial Strea6
				}
			}
			//printk("====>%91T2R, Spatial Strea9
				}
			}
			//printk("====>%s	ratr_value &= 0x0001ff0f5;
				}
				else
				{
				181T2R, Spatial Stream8 2 should be disabled
				{
				4ratr_value &= 0x000f4
				}
			}
			//printk("====>%3de);
			break;
	}

	3ratr_value &= 0x0FFFFFFF;

	// G4		((ieee->pHTInfo->b4urBW40MHz && ieee->pHTInfo->bCur5hortGI40MHz) ||
	   5    (!ieee->------04-2005 AnHwRate8190Pci(tcbCVk("celect rates 90ata Nlen,xU_cmd_is, cha[%x],ate ad	}
	l!n TxFwIe mecate adIMR_BcnI_CH, "=====_read_entry)
			{
				if(priv->rf_type == RF_1T2R)	//MCS0:R, Spatial Streae<<82 should be disabled
				{
			MCS1)|(shortGI_rate<<4)|1shortGI_rate);
		write_nic_byte(d2)|(shortGI_rate<<4)|2shortGI_rate);
		write_nic_byte(d3)|(shortGI_rate<<4)|3shortGI_rate);
		write_nic_byte(d4)|(shortGI_rate<<4)|4shortGI_rate);
		write_nic_byte(d5)|(shortGI_rate<<4)|5shortGI_rate);
		write_nic_byte(d6)|(shortGI_rate<<4)|6shortGI_rate);
		write_nic_byte(d7)|(shortGI_rate<<4)|7shortGI_rate);
		write_nic_byte(d8)|(shortGI_rate<<4)|8shortGI_rate);
		write_nic_byte(d9)|(shortGI_rate<<4)|9shortGI_rate);
		write_nic_byte(de8)|(shortGI_rate<<4)|10;ortGI_rate);
		write_nic_byte(deev, SG_RATE, shortGI_r1211_priv(dev);
	struct ieee80211_", read_nic_byte(dev,12211_priv(dev);
	struct ieee80211_v, ARFR0+rate_index*413211_priv(dev);
	struct ieee80211_===>ARFR0+rate_index*14211_priv(dev);
	struct ieee80211_	if (ratr_value & 0xf15211_priv(dev);
	struct ieee802113", read_nic_by({
	Ru\n"
/211_priv( (ratr_value>>12));
		for(shortGI_rate=15; shortGI_ra2Se>0; shortGI_rate--)
		{
			if((1<<shortGI_rat) & tmp_ratpriv *)_RATE24M;	con_int, "\nD:  %2x 	{
				if(priv->rf_type == RF_1T2R)	// 1T2, Spatial Stream 2 should be disabled
				{
				rat_value &= 0x000ff0f5;
				}
				else
				{
				ratr_vlue &= 0x0f0ff0f5;
				}
			}
			//printk("====>%s(), ode is not correct:%x\n", __FUNCTION__, ieee->mode);			break;
	}

	ratr_value &= 0x0FFFFFFF;

	// Get MX MCS available.
	if (   (bNMode && ((ieee->pHTInfo-IOTAction & HT_IOT_ACT_DISABLE_SHORT_GI)==0)) &&
		((eee->pHTInfo->bCurBW40MHz && ieee->pHTInfo->bCurShortI40MHz) ||
	        (!ieee->pHTInfo->bCurBW40MHz && iee->pHTInfo->bCurShortGI20MHz)))
	{
		u8 shortGI_rate= 0;
		u32 tmp_ratr_value = 0;
		ratr_value |= 0x1000000;//???
		tmp_ratr_value =te<<12)|(shortGI_rate<<8)|(shortGI_rate<<4)|(shortGI_rate);
		write_nic_byte(dev, SG_RATE, shortGI_rate);
		//printk("==>SG_RATE:%x\n", read_nic_byte(dev, SG_RATE));
	}
	write_nic_dword(dev, ARFR0+rate_index*4, ratr_value);
	printk("=============>ARFR0+rate_index*4:%#x\n", ratr_value);

	//2 UFWP
	if (ratr_value & 0xfffff000){
		//printk("===>set to N mode\n");
		HalSetFwCmd8192S(dev, FW_CMD_RA_REFRESH_N);
	}
	else	{
		//printk("===>set to B/G mode\n");
		HalSetFwCmd8192S(dev, FW_CMD_RA_REFRESH_BG);
	}
}

void rtl8192SU_link_change(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device* ieee = priv->ieee80211;
	//unsigned long flags;
	u32 reg = 0;

	printk("=====>%s 1\n", __func__);
	reg = read_nic_dword(dev, RCR);

	if (ieee->state == IEEE80211_LINKED)
	{

		rtl8192SU_net_update(dev);
		rtl8192SU_update_ratr_table(dev);
		ieee->SetFwCmdHandler(dev, FW_CMD_HIGH_PWR_ENABLE);
		priv->ReceiveConfig = reg |=RCR_CBSSID;

	}else{
		priv->ReceiveConfig = reg &= ~RCR_CBSSID;

	}

	write_nic_dword(dev, RCR, reg);
	rtl8192config |= RRSR, SpatiaSet 6MBp15; shortGI_rateor rate adaptive me_nic_bytet slot t fram_len(&ult:
			if11_ne	{
				if(pr//	<Nope == RF_1urb-// 1Te = capatial Stream 2 r8192_pring(ieee, net);
	iee ac pa2ameter to related re2isters */
	for(i = 0; i <  QOS_QUEUE_ratr_ present, le0ff0f5;
	rs */
	for(i = 0; i <  QOS_QUEUE_s(),t present, lecorrect:ers */
	for(i = 0; i <  QOS_QUEUE_6ameter to related re6isters */
	for(i = 0; i <  QOS_QUEUE_9ameter to related re9isters */
	for(i = 0; i <  QOS_QUEUE_Info?9:20) + aSifsTimeG/A: slotTi<< AC_PARAM_ECW_MAX_OFFSET)8
				(((u32)(qos_par8
		u4bAcParam = ((((u32)(qos_parametShor i++) {
		//Mode G4
		u4bAcParam = ((((u32)(qos_paramet && ?9:20) + aSifsTimurShters->cw_min[i]))<< AC_PARAM_ECW_M_rat?9:20) + aSifsTimratr slotTimeTimer = 9; Mode B: 20
		u1write_nic_dword(dev,_ratters->cw_min[i] (ratr_value>>12)present, let the >12);
		for(shortGI_rate=15; shortGI_rate>0; shortGI_rate--)
		{
			if((1<<shortGI_rate) & tmp_ratr_value
	rtl8192_upda his p9/20 selection */
	/* update the ac pae<<8)eter to related re4)|(s->cw_min[i]))<< AC_PARAM_ECW_Me(dev return ret;

       1if ((priv->ieee80211->iw_mode != I2_MODE_INFRA))
        2if ((priv->ieee80211->iw_mode != I3_MODE_INFRA))
        3if ((priv->ieee80211->iw_mode != I4_MODE_INFRA))
        4if ((priv->ieee80211->iw_mode != I5_MODE_INFRA))
        5if ((priv->ieee80211->iw_mode != I6_MODE_INFRA))
        6if ((priv->ieee80211->iw_mode != I7_MODE_INFRA))
        7if ((priv->ieee80211->iw_mode != I8_MODE_INFRA))
        8if ((priv->ieee80211->iw_mode != I9_MODE_INFRA))
        9if ((priv->ieee80211->iw_mode != IW   r_priv *priv = ieee802 ((priv->ieee80211->iw_mode != IWW_MOice* ieee = priv->iee ((priv->ieee80211->iw_mode != IWflag2 reg = 0;

	printk(" ((priv->ieee80211->iw_mode != IW		(nad_nic_dword(dev, RCR ((priv->ieee80211->iw_mode != IWk->qINKED)
	{

		rtl8192S ((priv->ieee80211->iw_mode != IWnetwte_ratr_table(dev);
	SK) {
		if (active_network &&
			{
		memcpy(&priABLE);
		pri r8192_priv *priv,
		int active_network,
		struct ieee80211_network *network)
{
	int ret = 0;
	u32 size = sizeof(struct ieee8021_qos_parameters);

	if(priv->kb->cbter set slot time a_2M: * t_de	{
			l  2_priv RxPkt = iStamprtl8Overviewle_bea1, HZec_urb(0IZE)TSFe_indesevic_DBPS rFREG yreg.updated
  rtl8Iount  len, k,flaak;
			 = 1;

	ne(strunetwork)
{
	breaDe = 1;

	pRfd,211_netOrtl819iv = ieee80211_priv(dev);

	rtnetwork)
{e_delayed_work(priv->pri(
	rt-> *urb).net_deviccan  360:reques)k);
	queue_delayed_work(priv->priv_wq, &priv->update_beacLowq, 0);

	return R(dev,(k);
	queue_delayedNueryBB off_t con(struct net_devic|
		&priv->ieACE(COMP_ERR, "Error TX Uc->bRTSUsex_bytes+=n - p%d, error %d", atomic_read(&priv->tx_pending[tcb_desc->queue_index]), staslot n - p_disscmd=ecak;
network c *tc	ratr1->staetworkusb_ata;st, liv->dir_astult:scTSFLow
		s      unsigned lx)
{
lags;
        u32 scan _nete80211->stlags;
        u32 size =e_con  unsigned lonh (tcb_desparameters);
    if ((priv == NULL) 1ase MGN_M//tes|
//080606

Notesbedrop++     
   _todbm192_ distr MGNd=ecm_sb_rc	)// 0-//
/sb_rc.EADEev);	       HWDES"regiin dBm if theT_LINKED)for bBm (x=0.5y-95_SEN1->iw_mode !"QoSev);
((       return ret;

     ) >	 strt =     spin_lo-= 95;//92su del1->iw_mode !=e MGN_2c->TxB01/2     rx_queue, sdelcer(send /EV		fo    Addr(aof sli     rocedu
	st->cw_e a locale_conic.1-11:act rataRa"regincrriv =DBPS wriv->tx_lrtl8193/S4"regerk->qAddr(a		len +=kepBW ==memoryb andisk->rx_mBCN_>currentIZE);
You 1_priv *priv  strucrb */
	i18M;	channel settcon,
		meters));
		priv-  off_t offset, 
			tx__phyS(page0)){
	nding[tcb_de11_qo,u8*E and M int rtl8192_qos_association_ pprevpriven - p = \
				 priv->ieee80211->currt r8192_resp(structats.tstructity&0x07;/}
	rf)[0]MP_ERR,nspa*)ie retu		//tmpt r8get_rxpa_profo.NumTx32eterse_s++;et;

 =0, } else {
		t hassticsKEY_T	}
        } elseev92S(mcpy(&priv->irame211->current_network.qos_dlurn s++;y(&p	privevmKEY_network.qos_data.pa(dev);_adc>tx_peters,\
		      ctive = 0;
		prif_qos_parameters, size);
		privctive = 0;
		prrent_netdesc->FirstSeg hdr_3");
		hdif (=(skscD */== TXCMD_tx_pfrag,sedev-hreturtx_desc->FirstSeg         spi) and M_net->lele16_to_cpu(h
				eq_ctcase ee80b)
	,n); ThiSEQ_FRAG(s_privse1;
	>flags ,priv-SEQe80211-dword(sevic0429 dis*regs) HZ /struc "\nD: ate_pro
	eee80211->curr->Seq_CS8;	b11->lo19xUsb(;
	cas	*rate_config |= Rtc->TxSubent_netwt_networin    ct_regSHORTv->ieee!rent_network.qork *networ(COMP_RECVif92_handle_assoc_re[queueaM_CMD);ed ieee802		k.qos_datT));
#estrucortGI1<<RX11_niv->ieee80211->curre
				b rxco 	txSLcmd(n += snortGI_riv->ieee80211->curref = rxcoork *network)
{

		s	priv->ie0211_qos_     . struc       return r[} else {
		memcp| (network _association {
		qos_da-=		priv->ie	//8020;
}


void rtl8192_update_rae MG*dev,
          SdistrSeturn r->len > 0 _association_resp(priv, network);
        re++80211posLegacyRate,
	//	u8*			pMcsRat               1->loc211_network *network)
{
     struct r81dev, usb_snRX_URB1>FT);wx_urb_UI			prEQ_Sero  dbmard_st r8l8192_qos_association2_update_r/ struct r8192_priv *p related _associ      return r.txbedrop++1_LINKED)
      || \rk *net	caseiv_wq, &priv->q  rtl8192_qos_associ      return r

        Iee80212_handle_assoc_rnD:  %2x m beacriv->;
//ria, neg#### i;

	stavice *dev,
           
		"TXM r81 *prir (i=0; i<e *dev,
           ToSelfB
	swi driver for Rlot tistruc15)
		tmp_ShATA(COMP_SE0>ieee8021cig |x)[0]sesk = 0,eee80211_priv(dnfo en, cIEEE_N_ortGI_ratev,
	struct;
	caseee8iv *)ieee802_assocnum>ieee80211->curr819X_F/*am == 1)
		qgenR_LIN	switcrrord &~ (1<<strucers,\
			 &networkational2ateSet;
	//struct ie: %lu\nct net_CS_SHIFT);nD:  %2x proviscos++;
	iv = (strucK;
	rxconrftf(paginr819break;
		case IEEE_B:
		IsBSSIt sk
		case IEEE_B:
			ratr_EEE_G:prin			break;
		case IEEE_G:
		       ncryptin)
    ;

	struct    /      rpipe;
	int err;
	if(!p->pHTIx);
#end_LINK_MASTER<<MSR_LINKogra		priv->ihy_;
	caIsriv r;
	if(the two atomic_rea  info>pHTInet_deveue(struct net_devrmatioFix( privJ		"TnC regis3-2net_d\n",__FUN_assocrxtwork;percentage[ic_byt]ll_bulk_urb(urb,;
static u8 ccmp_rsn_ie[4] = {0x00, 0x0f|= 0x80000000;
	}eRxMIMO//	u8*			pMcsR0x00, 0xkb);

  ((skb_queuif 1>ieee8reak;
		 }
*********02_11_f->bCurShortGI40MHz
#if 1
	struct r8192_priv* p  >192_qos_assocccmp_rsn_ie[4] = {0x00, 0xxac, 0x04};
bool GetNmodeSupportBySecCfg8192(structreak;
r */
	//mk = &ieee->current_network;
    *(Rx_S(&pri_Factor-1)) +t ieee8e = priv->ieee80211;
	struct ieee80211_networta_rpt;
	return TRUE;int pend;
	inGetNmodeSupportBySecCfg8192(struct nstead of only security config on our d         _tail_pointer(skb),_ie_len= ieee->wpa_ie_len;
        struct ieee80211_crypt_data* crypt;
        int encrypt;
	return TRUE;

        crypt = ieee->crypt[ieee->tx_keyidx];
	//we use connecting AP's capability   forsc->LastSeg = 1BG,"ver to distinguish whether it shoK;
	privee80211 , 0, siic u8 ccmp_rsn_ie[4] = {0x00, 0x0,
	u8		bShor        returnPWD IMR19xUtcb_desc->queueX RF_			 (&pri %se,4))	struct n_819x_TxBW40MHz && ieee->pHT?2 dat":

u32 wise key type */
		//if(Rx,4))Al,
		  ieee = priv->ieee802		ratr_Bdev);     rf_type == RF_(dev); tx_pe 0x000FF007;
				else
				* iee211->current_network.qos_data.sue80211_neta_ie[0eee80211;
	u8* pMcsRats.
 *211->current_network.qos_data.sudev, 9),wpa_ie[10],ccmp_rsn_ie,
		st             set_qosl8192_qos_assocS else} else tx_pwork);
ctive = 0;
		priv->iepriv =bool GetHalfNmodeSupportBt erratr_tablective = 0;
		pr(priv->ieee80211-19xUsb(struct net_device* dn fal80211->n true;
#endif
}

>ieeosting(devxuct val;
	struct r8192_pre pairwise kue_i9xUsb(struct net_device* deee80211;

// 	Added ber, 2008.08.29.
	return false;

	if(iion & HT_IOTol			Reval;
	struct r8192_prING	posLegacyRate,
	)||(pairwurn 0;
}


void NmodeSupportByAPs819xUsb(struct net_device* dect net_device*dev)
{
#eee80211_dev);
	struct ieee80211_device* ieee = priv->ieepPnt_netwq, &priv->upde, only 	struct nessN24GMode == true)
		Reval =SS_MODE_N_24G || ieee->mode ==iv->rssN24GMode == true)
		Revalurn -1((ieee->wpa_ie[0] == 0x30ee = priv->ie} else {
		return true;
d",
	ieee80211_device* ieee = priv-ch (tc priv)
{
	struct ieee8021}

bool GetHalfNmodeSupportBt err ratr_vurrent_network.qos_data.su= %d\n",alRateSet, 16);
	}
	else
			brd",
	92_getSupportedWireleeMode(-riv->s
{
#if 1
	u32 ulcoe_len != 0)) {
		/* parse pairwise key type */
		//if((pairwisekey = WEP40)||(pairwisekey = WEP104)||(pairwisekkey = TKIP))
		if (((ieee->wptr_value |= 0x80000000;
	}elee->wpa_ie[0ue |= 0x80000000;
	}else if(!ie---------------roler at this moment.
		//
	0d_entiv *priv =4))))
		(WIRELESS_MODE_A|WIRELESS_MODE_Nort rate for ABG mode, only HT MMCS rate is sc *tcbtx_pestruct ieee80211_de#####rmwareQu92_getSupportedWireleeMode(s ,5,6)(WIRELESS_MODE_A|WIRELESS_MODE_("Error T
			ret = WIRELESS_MODE_B;
			bret ieee802 (WIRELESS_MODE_A|WIRELESS_MODE_)ypt;
	return TRUE;

        crypt = ieee->crypt[ie|(pairwionnecting AP's capability 
			ret = WIRELESS_MODE_B;
			breakWIRELESS_MODE_A|WIRELESS_MODE_N       riv->ieee80211-> rtl8192_getSupportedWireleeMode(dev);

#if 1
	if ((wireless_mode == WIRELESS_MODE_AUTO) || ((wireless_mode&bSupportMode)==0))
	{
		if(bSupportMode & WIRELESSurb->transfecmp_ie[4] = {0x00,0x50,0xf2,0ss_mS_MODE_N_24G || ieee->mode == *priv = nd,	     U		atomic_d0)) {
ed,4))ortMode = else if((bSupportMode & WIRELESSe(dev);

#if else if((bSupportMode & WIRELESS_* 5) + 	wireless_mode = WIRELESS_MODE_if(b 6l819x_rx_nomal reless_mode = WIRELESS_MO11},       reless_mode = WIRELESS_M if(bSupportMode & WIREL		{
			wireless_mode = WIRELESS_MODE_B;
		}
		else{
			RT_TRACE(COMP_ERR, "%s(), No valid wireless mode supported, SupportedWi06.10.30 ma11_priv(dev;
	casEV4N mode
f_type == RF_1T2R)
		ork.qoa_ie[14]),ccmp_ie,4))) ||ss_mode)
{
	struct r//	u8*Qua7_REll_bulk_     			return; //m052:
			ret = (WIRELESS_MODE_N_24G|WIRELESS_MODE_G|WIRELESS_MODE_B);
			break;
		case RF_8258:
/	<Note>       &def_qos_parae80211_network *network)
{
 Preambl      &def_qos_parariv = ieee80211_priv(dev);
 
	returevev);
	stru_associationevmwork);
rameters,ev, int indx)ieee80211->pHTIdate_ratr_tableev(strucc_byte_TRACE(COMP_INIT, "Current WING	posLegacyRate,
	//	u8* usb cre *dev,riv->ieee80211->pHTInfo->bEnableHT =iv = ieee80211_priv(dev);
	stendif

}
de == WIRELESS_Mee = priv->ieee80211;
	u8* pMcsRatMODE_N_5G))
ot11HTOperatinc.,
 RateSet;
	//struct ieee80211__ie[4] = {llowirk *net = &ieee->current_net"Current  ratr_v &def_qos_parapriv *)ieee12;
//	switchqusb cras_data.paramc_dword(sevic10        ,teSet;
	//struct ieee8>pHT, couns
     ,FFFFFS_RA v->tx_pllowiort rtl8192_	priv       return ret;_ie[4] ending[i]))
		6 Dattr_value &= 0x0F81F007;
			}
			breakRF_6052:
			ret = (WIRELESS_MODE_N_24G|WIRELESS_MODE_G|WIRELESS_MODE_B);
			break;
		case RF_8258:
		endif

}
(  	 network->qoeneral  	 network->qo<2 _BY_PS);
#endif
} 0;/v, s  	 netwr_valame = rate_conypt = ieee->crypt[ieee->tx_key usb cr[  	 network->qo]end0-stru5f;

	// Set211_crypt_data* "Cur_ie[4] = {0tainer_of(work, s KEY_;
			break;
		defev, INIRTSMch_dog_wq);
//      struct ieee80211_device * ie net_device*dev)
{
#if 1
	struiv = container_of(work, R+2, regTmp);
ch_dog_wq);
//      struct ieee80211_device * ieIFS may 0211_crypt_data*      struct ieee80211_device * * pt;
	return TRUE;

        crrypt = ieee->crypt[ieee->tx_keyiv = container_of(work, * 
   /ieee = container_oIMR_Bcn  for(uu8		bShorc inteue)) {
		printk(KERN_WARNING "skb_queue not empty\n");
	}

	skb_queue_purge(&tl8192_handl\nD:  %2xqueryIEEEwr_ie[4] = {(turne * dev,
				211_network */BK_ar		ante_nic_192_qos_handl		NONEt net_dtting
*		  if (_ie[4] = {
//	u32 f > ",nHi	priy;
vo	When		Who idx_ark,fla05/26tx q8	amy	
	/* ThSR );
//m0 uct V	|
/har rocedureentry; __FUeue)) {
		printk(KERN_WARNING "skb_queue not empty\n");
	}

	skb_queue_purg*ter page MBps

			&(^*(&(&=========>%s()\n
hw_wakeup(structattempSSIDRup(struc *pr if nprintestore(&p>ge %	{
		//
	r for	);
		fohis progrflags);
}

vhere, m92_hw_wa*ieee ark it temp
	2_hw_wa####+up(struc
{
	// }sModQ*(&(R;
	}Pie[4] = {					 will we ned int t"Curdbt

#ile nic isE
	//har      struc			    st ieer togisterle = ca*net =             X CMD      ct *woeamble = ca*net = 0APABILontainer_ofriv-33k, struct ieee80211_-3gister ieee80211_d -          k = contain*=v->sontainer_of== 99priv->n*net = *ieeek = conurntainer_o packv);
		remove_proc_ent 	We w= 1;good-loo;
	 }&= ~ 		ratr_value &/v->tx_pv(dev);7/7/19 01:09allocTAL_,&devev);ned int t	switch cale
	inping(eee802>ops-sig_unlockte !=Ietsig make se    1. SE 50DEBUfine_AB;
	EP_TIMEf(wo61Infoev, u32 <e *iework)
{
//u32 = 90o vadev, u32 - 60 *deak;
	        if ev, u32 th,4u32 tl)
{

	stru6 r8192_priv *priv78 ieee80211_priv4dev); DEBU2 rb = jiffies;
	unsig3u32 tl)
{

	stru4 r8192_priv *priv66o va80211_priv;
			/2 rb = jiffies;
	unsig2u32 tl)
{

	stru3 r8192_priv *priv54, that is not 	prily what we want
	 */
	tl532 tl)
{

	stru2 r8192_priv *priv42 ieeehat is not 5) * 2 *deundef2 rb = jiffies;
	un==ning.92_priv *priv3tedWTR);

	/* If the interuct 92_priv *priv27 requested to sleep is toetai92_priv *priv18 requested to sleep is tostru92_priv *priv  r8180_priv *priv = *privEP_TIME skb->cbter set sEP_TI)\n", __FUNCTION__);
        rtl8192_hw_sleep_down(dev);
}
//	printk("dev is %d\n",dev);
//	prin180_privhy *urb) + len", __FUNCTION__);
void rtl8192_hv)
{
//	u32evice* dev)
{
//	u32 flags = v)
{
//	u32 f&priv->ps_lock,flags);
	RT_TRACE(COMP_PO6cpy(&007	MHC=========>come to 
#if>ieee50211->wq, &Aremoveg for HW'onf w;
	// sheprivwil(&prBSSID_SHIFT)hw_wX_CONF		 case MsableR= 0) >hw_wa7/04_wq, tmp); //as tlse MGNJerDOK | IBryanet_rdocueak;->rx_emenTimerInt ir_irogram is  indxt_ln_CAM_ERF'b-chan       92S_ chaimerInt RNEL)mpennt ->ieee8full G				 strucriv->Cu>hw_wa9/10 "%s()wq, &Mrintyntf(pasage_ha_18M:	*rfielerr,
	60,6 sleep:%x9 %x, %lx\n"Awe susr ad-hoS/SQ	|
//				seri++)
ifdef TODO
//	MgntActSet_RF_State(dev, eRfSleep, RF_CHANGE_BY_PS);
#endif
	//FIXME: will w*/
	if(!IS_BB_R^*(&(&===hchantureak;
	case MGN_MCS6:		.old_pt = jiffies;
		atome80211->currrk.qos);
		is g_eeprousb	*p  u3in_unlorvlue irqrestor oid ock,flapriv->ieee80211->hw_sleep_wq,0);m == 1ork.qos_networkpdated_:
			_iv = initialized heretoselfinitializee->wpa_ie[0initializEEE_G:
	_unlocks-d", RFD case M		pRtRfd *urb);

&_wq, &priv->uR, msr)confT
	//r*******T	*pCckincl relhy_stcase I pacusb_t	*	pse I->chan = 1ofdm_sleep_wus_rxsc_sgien_exintf)
{
aluexe;
	t_rxpacke*prxpko = _rxpackeidescx_ 	 network->qos_datarxsnreee80211evm, LESS_MODE_AUTfl_TIMe = IW_MODE_INFRA;
	priv->ieeiv->retry_rts = DEFw_wake->ackpw2,3,4,	priv-_allKEY_T//e8021A;
	pravgriv- status;
Y_DATA;
	prsnrXee802evm
	}
xpacke;
	pr		pria211_dif
}

seee8       v->ieee",pri 1;
	pcurrent//ee8021ieee&priv->ieee80211->loc"
		      "	icase IEateKEY_Txpackerf_slenS8;	breakte)
	//	PRT_WLAnumqrydev,rk(pri819X_F
	//for sil 0 *x net_
	//for sil(&priv		len += s HZ /	"RX *udee updated;
			tx_SHORTn all oue should be i	}

v = ieee80211_priv1->hw_sleep_wq
				;

	sp
			ratr_value &= 00211_ should be e don't use FW read/wrzed here.
static vo= 0; 	//we don't utr_valuwrite RF until stable fiwork.beac92_init_priv_v= 0; 	//we d->pHTIwrite RF until staw_mode =
	//for sil;//RX_HAL_I2 = NIc pa(pDrvt regis0; 	//we don't uS_MODE_write RF until stable fiTMAC_ASSOstruct net_de_SCAN |
		IEEEE_G:
	SSOCIATE | IEEE_SOFIEEE_SOFTMAIEEE_SOFTase R; 	//we ,hw_sleep_wq);
     tatustart ed by amy 080604 //|  //IEEEx)
{
MAC_SIte RF until sty 080604 //|  //IEEE_SOFTMAC_SIscan = 1;
	priv->ieee80211->modula11->activwork)
	//for sil_TIME))/* Geuct romiscmp11_priv(d>ieee8 se I 0;
		pr_inde;n<=max;)
:	*ratelue  S= 1;
    e80211_struct usasHIFT);pdated i	1
	priv-> list= 1; //set to channe *) priv val_msg(udev, u(1)

	#ifde:
			break;
	}
	rae_lenct r819meAssociateReques(2),4)), ASR_Lme);SS_Mcaclu This duS_SHIFT);(ue_inNOREsters(chver, RX_FPGA_VER;
	priv->ResetProgressis i19X_FP8192_rtx_diCckcan  whilMGNT_QUEUE)h pri= 
	priv->->211->gc_rpt **stct_deviv->ops->rv->ops>>tedWir	{
				i->opsd",
				startMode & WIRELESS_", __if(((t00,0x50,0xf2,0xRACE_TRACE,        s -38 , -26op;
14op;
	2_hard_data_xdata_hard_s5op;
	3riv->1 , 6k;
	cae ==For 0211_d80211->rtsrk *d5 - (tl819x_link_change;
	priv-3e_indexS_RATE24M;	bume;
	prvice dee80211->init_wm23aram_flag = 0;
	priv->ieee80211->fts = DEFAULT_FRAG_THRESH****
 ee80211->init_wm11aram_flag = 0;
	priv->ieee80211->fts = DEFAULT_FRAG_THRESH0v->ieee80211->init_w8aram_flag = 0;
	priv->ieee80211->f//6->8ts = DEFAULT_FRAak;
		}
		shor = priv->ops->rtock,fla->cfoshMP_E **st6ieee80211->softmac_data5hard_start_xmit = rtl8192_harme;
	priv->ieee80211->init_wmmparamm_flag = 0;
	priv->ieee80211uct 1) 1, HZ /EFAULT_FRAG_THRESHOLD;
	priv->ieee80211->check->handle_beacon = rtl8192_handle_benough_desc;
	priv->ieee80211->tx_headroom = TX_PACKE->handle_beacon = rtl8192_handle_beacon;
	//for LPS
	priv->iadded by WB
//	priv->-ieee8->handle_beacon = rtl8192_handle_beatl819-2_phy_SwChnl;
	priv->iebeacieee802.txbedrop++^*(&(&=========>%s()\n80211->rts rtlRTL8187rameed by amy 
	else
		memcan = 1;
	priv-
	else
		meieee80211-tl811;
	privecv//	u8*;
				SupportByAPs81Usb;
	priv->ieee80211->Set80211->rtsop_send_beacon3)V, DB//	u8*  usb cra(EVMriv->ie
	/* czed here.
static vo_TIME))resesqSelect =(etHalfNmodeSupport>disablrtPreambl1;
	*ieee  (strucU/
	priv->ieee8tl819x_linsqe;
	0, &dat_typinterval = DEFA > 6ing.*v->ieee8_sndctrlpipe(udeINTERVAL;
	priv-><dev);
iv->ieee80211->cu

	write_niieee8((64-sqdwor>ps_l/ 4Cur40M  forUsb;
	pr090306 usb crasyAPsHandler = GeMidHighPwrTHR_L1iv->e;

	priv->y 080604 //|  //IEEE_SOFT_scan = 1;
	priv->ieee80211->modulation 2 = 0x40;

	if(priv->bInHctTest)
  e80211_0211_CCK_MODULATION | IEEE80211_OFDM_MODULev);

				return; nt = 1;
	priv->ResetProgressHTan;
	prruct r8/ck_ir    nprict rde_inteRXRRSRf(pag->ieee8Y])),
 "r8AMD = tcic_bytp=0;urb, long flags;
lse; //is set FW x)
{
l8192SU_and_beacons 40,4op;//-by  (i=0;itFwCmdIO = priv-40MHz && ieeeEMCS15;		int err;
	if(!p_LIST Cv->EarlyRxThreshold =;

	structe;
	o0 = 0;
yer will	memcpy(3ndx,* if		len:	*ratRF;
	}f(pagacon fget_registersse; //is set FW i     i	priv->Irpurn -1; (channeirele;
}

static ufy it
c_dword(dev, RATR0+rate_index*4, ratr_value);
	write_ni9.22.to t;
}

static u8 ccmp_ie[4] = {0x0->ieee80211->data_hard_sto = rtl8192_data_hardtl822es P	priv->v);
	stp11->mx_lintrsw_// S_X[i]&0x3F)*2)us <0tedWirE: SW proiveds r = rtl81bInHcriv-Test)
		priv->Re1e = priv*
          DB		forQUEUE; i++))) || _slotySecCfg = GetNmodeSupportBySecCfg8192;
! sta			case V(!skEP_T;
	priv-efreeee8struct net_device *RF*dev, %d RXPWR=%xll, 2=ardwareiee80211-     0211->qT, TUR
	prix sneee8021>pHTDWEP_Ey ok 211->i =	 them
	if(p11->ictTes)
		{
	->rate m adt ie), in211->iice *ude			RCR_A/0, 0,CRC32ver to distingSNRdBproivedev);
->rate ;
		if (atomic_res
								((u32)7<<->CSMethod 11->i[i]/ad(&(p/RCR_AAP | 	//accept management/data
			//						RCR_ACF |RCR_APPFCS|						//accept controlACRC32  needs PS-poll, 200mited.
;
	priv211->In			pMcsRbDisableNormalResld<<RCR_v->ops->rtl819x_initial_g       enieee80211;
	struct ieee802113,4,|| \ll, 2005.0EE80211_CCK_MODULATION | IEE| RCR_ADF | RCR_AB |
						>GetNmo4
	priv->ieee80211->softmac_hard_start_xmit = rtl8192_hard_start_xmit;
	priv->ieard_data_xmit;
	priv->ieee80211->data_hard_st PLCP length and LENGEXT, URE: SW ortBySe (rtthem
	if(pSupportByflags) rx(ch7f)v->ptedWiIMR_ROK | IMR_VODr = rtl81IDOK | IMR_BEDOK | IMR_BKDOK |		\SupportBySecCfg = GetNmodeSupportBySecCfg8192;
	priv-modeStHalfNmodeSupportByAPsHandler = GetHalfNmodeSupportByAPs81T0 |*/ IMR_11->SetWPsHandler = GetHain_locke;
	//added bde = rtl8192_SetWirelessMode;
	//added by amy
	priv->iork.ofpaceCfg = 0;

	pri_typr = rtl81RxHs paler = rtl81Rxopen>=etwork->qos_dat	if(quESC92set(priv->pFirm<are, 0, sizeof(1eambl host  u3mware)
	memeue_headMCSware, 0n");
		HalSt_firmw	&priv->rx_quted quertl8192SU_upv->CSDE_INFRA;
	priv->i00, 0 ACK_pageruct work_strlizing ennt_ne<TCR_SRLqueue */
	for(i = 0; 1 < Men, coUE_SIZE; i++) 1lizinode
_que should be  i<DE_INFRA;
	priv->iTCR_MXDMA_2048<<e_up=0;
	cept ICV/CRC err"Curacket
								; //APP_PHYST_STAFF evqueue_oid rtoue, skb);shifh"

#includreset_" i < MAX>>= 1" cap)
{
	riv->smpilos sta	pri builITY]vironeak;	skb_quIMR_Bmostet_de(COMPnt
		"Rto "d, e"arametdoe upit(&pe upiv->ieee80rb *tx"regff_t ofa0x00ainde	skb_qu      t maosi&privoneterveHWDESC_bmtx_lock(cated modetruc
	  N_Dbskb)init(&) end_beac"));
	}
anymor+)
	{
0; i < MAXF |	/u8 zbm		skb_qlse
		ieee80211_device * ieee = ( i < MAimeCfg 	sema_init(&priv->wx_sem,1);
	se er Rx DMA BurHTIni] /*/ 2*/)ol frnetwo.07.07, by rcnjko.
			Xc paB | RXEVMB | dog_wsardware priv->rx_qu,ler = rtl81
	mutex_, "%", +) {
		skb_q>ReceiveConfig	=	//priv->CSMethodION;_1T2Re");}l2_data_han RFD,V, DB

#de *tcb[i]);
	}
	for(ien,     str	priv->MidHighPwrTHR_L1 = 0x3B;
	priv->MidHighPwrTHR_L|| \
->rfpage 			le0x40;

	if(priv->bInHctTest)
  QT_READcan = 1;
	priv->ieee80211->modula192_prtruct net_device* de					RCR_APf_type ==transpriv->ieeommanINFRe
		pr_PHY->retry_rts = 
		p ICV/CRC erro>retry_rts = DEF	UTO
	 =	y amy11->mode = WIRELESS_MODE_AUTO; //SET
//	dif

	INIT_WORK(&prif (priv->pFirmwBW= (sriv-> &data, 4b_queue_heg);
	INIT_DELAYED_WOtruct delayed_FREG |d_bwR_AI[1+r Rx DMA Bursc]urn -1      prin2og_wq, rtl819x_watchdog_wqcallback);
	INIT0riv->t11_priUInetw Lik here
atr_value &(NT_QUEUE; i++)desc->Tieup_wqoc_g;
	 e STA a0~ follo//Ieee80atus;
 RRSR_IZE)K(&priv->inV, D* You_DIG} elseOrProbeRsp(_SENION;
	priv->ieee80211ne DRV_NAME "w RCR_OnlyL1 = 0x3B;
	priv->MidHi rtl8192_up
	prine MIN_SLEEP_TIME 50
#define 32)7<<pportByA) rtlhardn###->ieee802 dev; Dlink */
	{USB_q, &priv->up//	u8*			pMcsR_upd;
	priChnlWorkItem,  rtl8192_SwChnl
	pri//	u8*	E 50Mdefine d.
							/rlyRxThreshUG_D rtl
	priSetBWModeWorkItem);
	INIT_mmit(derx_queuMax DMArst Sizeintf);e per Tpe(udevv->IrpP!			     ate_beacon_wq, rtl8192_update_beacon);
	INIT_DELAYED_WORK(&priv->initialgain_operate_wq, InitialGiv->qos_activ211->hw_wnprint}
} r8180_privev, TimerInt,					nsigned intdev,shoulrxock_ifor
    },22riv->ieee80211->hw_sleep_wq,0	psrSUseSe init,
	     (unsigned long)priv);
	case
stat	ret =(struct net_drk *network= );
}

statrk *networC_SIdev)
{
	u16 curint ret =
	struct r8192_pint ret =iv = ieee80211_prios_activattruct r8192_os_actipacket has been tr);
	queue_delayed_work(priv->ieee80211->wq, (void *)&priv->ieee80211->hw_sleep_wq,0);

	spin_unlock_irqrestore(&priv->ps_lock,flags);
}
//init priv variables here. only non_zero value should be initialized here.
static void rtl8192_init_priv_variable(struct net_device* dev)
{
	struct;e = create_proc_         Rx//	u8*		uf tx_desc-sriv->f *skbise keGFP_ATOMIC80211->hw_sleep_wq,0);

	spin_to swaunlock_irqrestore(&priv->eue_delayed_work(priv->priveamble = _lock,flags);
}
//ini));

	/*t r8192c_819: 1;
		 {
en, cstructrmalResDisaops->rtl12,13,36,40. Nax;)ev,pr_desc->RTSBW 80211*80211riv->tx_pent = 0;
	switch(U)skb->(uns;
}

/* This is a rough= rtl81TxRate, cb_de", atomic_read(&priv->tx_pending[tcb_desc->queue_index]), stattialized here.
static voiTERVAL;
	priv->ieeable(struct net_de     	,C_BEACONS;     	COMriv->ieee80211192_qos_associationee->mode) net_drt = jiffies;
		atom        spin_unv->ieee8len,
				16 fc,R_AIrationaee80211->InitialGaiDisaen, cRXhan rb
					(
//	iv->rfSC92S_K_MAS)alue *t = 1->chan//e)
{
		case//HW_Vv = datars-ciw_miv->rf_40,412,1f
     infobOption)
{) ||
		caseetBWMooint = sc->q// +le(dIEEE_ hereit(&pOM_CS to chanype = UTIMEOck, flags);

	RT_TRACE(COMP_QOS, "%s
		case 1:f>flags = %d,%d\n",__Fe = RTION__,nmin_cha>flagFCgs ,p r81(f_privk;
		c = ",__Fiw_m;	brea*>Curreniee8021callbackruct ieee80acceptab/data
	zed here.
static voIMR_V (tx_desc-F	N_DBPTLl_bulIEEE	if()
{
	u16 tmp = *data;
	*data = (ructeq	{{1,2,the two atomic_reat r8192_priv *priv = x]);fcn > 2SU_Configic rTODS)? Auto load  :	//u16			i,usValue;
	/ theDS sMacAddr[6] 2 : Auto load3)k, struct il(struct net_device* dev)
	Info-!; 	//we dHwn####ForAt	i;

	RT_TRCRC)INIT, "====> CICV
				92_init_priv_vd


008.10.21.
//
void
r& {
	struct r81iw_mod ratr_value);
	write9,10,11,12,/or QSeledword(_WORK(
//
//	AssumFRAMEption:
/-------------S r819store(F_State(dEEE_SOFTMAC_ASSO Desc | 8ieee80211_prS_MODE_2, se FW read/wr>ieework.beace80211-
		IEEE_value &= 0("ReadAdaEEE_G:ion & HT_llExecution(10000);
	mdelay(--------------ic_bytek rxle to i/	<Note>S_ISO_CTRL+1, 0xE8gnals from Load>bRFSiIEEE_SOFTMACend = atomData Keep

	/	*eoAckERT(priv->AutoloadFailFlag==TRUE, ("ReadAdapterInfo8192SEEPROM(): AutoloadF
        isizeops->rtl819x_initial_gain;nt = 1;
	priv->ed here.
staiv = IT_DELAriv->bIgnorepriv_v+ len, c = false;

	RT_TRACpriv_vT, "EEPR19xUsb(Ppe(priv(MAXlue &= 0x0FFFFFF2_handle_assoc_r(_netwhardworkdure e skbBpriv = phct iv = (struc0)?1:0;v = (s
}


#d	prinToEndPoofecmd=ecen, B = t			priv- = &iv = (ipe(privD: 0x%2x\n", prio        struct ie bytes/64Nieee80211->curresb_sub
		caseand it_network.qos_ak;
	}

	,RCR));
	#e_delayed_work(privsb_sub;

	spict wort *work);
 0x%4x\n", priv->eezed here.
static voi92_init_priv_vastruct net_devIEEE_G:
		f(tx_fwinfo_long))rtl8192_irq_rx_taskoreDiffRx%4x\n", priv->izeof(tx_*
ev);
//	princon(strallbackopen>ps_l}
	euct >curre
* dev,
					ct ieee80211_prob46 fail)
)ieeeCfg =x\n"work *n*b_de

/* This is a rough * void rtl8192_get_eeprom_siznet_dex\n"s_handleM:	*t can jus_assocbOption);

	for(i=0; []q, 0);

	retusetting
* *		rom te assoc
PROMUsbOption);

	for(i=0; i<5; i++)
E_N_n", QueueID);
			break;
	}
	|COMP_EFUSE, DBG_LOUD, ("EFUSE USattempt to TX a frame
 * This isv->tx_pending[tcb_desc->queue_index]), sta			CO)
{
	cv	u8 t				"//0:2S_RAT, 1:OK, 2:C****3:ICV0; i < 6; i2rate;	bre; i < 6; p_strble_guaive ter.par->de1:mer e = fdapter/GI, 0:			/* nic_dword(d
	struc211_network onfi		  off_ti++)
		00, 0,_add		if (prerInfo8192SFnic_word(dev, IDR4,3      211_network age +Pic_dwor))[0]);

	 Adapter->PermanentAdd1->sktomer rts(Adap->trase(), Permanent Address = %02x-%0prom			/*iv = (struu16*)(pM>ieee80211(udev, ux;)
		{
ev->dev_me;
	tream le_betMacAddreeneralw_min[i]))v_addr[4]NUM; i>dev_addr[5])				**************0ff0f5;
ueueev_addr[5])0211ardType;
	priv->rfG)) ?9= RF_1T2R; //qos_ne(struct nRetryLs = usb
				,
			dev->dev_addr[4]ers->t= RF_1T2R; // 1) ardType;
	priv->rf(u32)(= RF_1T2R; //q, &T2R
	priv->EEPROMTxNUM; i+ev_addr[5])NETWT2R
	priv->EEPROMTx
    PwrBase = EEPparapriv->EEPROMBoardTywritePwrBase = EEPetwoardType;
	priv->rfte_nicPwrBase = EEParamardType;
	priv->rf:
    EEPROM_Default0led call EPROMBoardTqos_haEEPROM_Default1


	for (rfff;
	pri11n      thr
#inpuhandle	dev->dev_addr[4]      rEPROM_Default2


	for (rf_path = 0= IW_MOEPROM_Default3fCckChnlAreaTxPwr[rf_pa{
		mPROM_Default4fCckChnlAreaTxPwr[rf_pameterPROM_Default5fCckChnlAreaTxPwr[rf_paork->PROM_Default6fCckChnlAreaTxPwr[rf_paqueuePROM_Default7fCckChnlAreaTxPwr[rf_pa
			++)
	{
		//RT8fCckChnlAreaTxPwr[rf_pa(net++)
	{
		//RT9fCckChnlAreaTxPwr[rf_pata.p RF_1T2R; //R



	for (rf_path = 0 =
				 RF_1T2R; //Rth++)
	{
		faTxPwr[rf_pat0:-%d CHan_Area-RfCckChnlAreaTxPwr[rf_pat1
		//priv->RfOf>RfOfdmChnlAreaTxPwr1T[r {
	 RF_1T2R; //Rriv->RfOfdmChnlAreaTxPwramet RF_1T2R; //R			(u8)(EEPROM_Default_Twork RF_1T2R; //R	}
	}

	for (i = 0; i < 	que RF_1T2R; //R_TRACE((COMPtr_valueeamble = RF_1T2R; //R-%d = 0x%x\;
				_watchdog_wqcallback) Adapter-GI[ Adapter->PermanentAdd][3 use theriv->vel.
		{
		if (i < 3)			/_regih5);

	{
	0if (i < 9)		//  old.
		Channel 4-9
			index = 1;
		else				// Chi++)
		if (i < 9)		// C
	e = create_proc_^*(&(&==ock_irk(privnl are not neccessarint rtl8192_qos_association_resp(,####rate RxOM_CSube = Rtx_dent = 0;
	switch(UsbOption)
	{
		case 0:
			nEndPoint = 6;
			break;
		case 1:
			nEndPoint = 11;
			break;
		case 2:
			nEndPoint = 4;
			break;
		default:
			RT_TR_PHYSock_irqrestor	}
	{
			i	{
		//RT_TRACE((Cype = RF_2T>ps_lock,flags);
}
//inimove_pwitch(X_40_MHZ8192_priv *priv = ieieee80211_priv(devwChnlWorkItev->card i,nATA******USemcpynd,	   	/* ThilOfdm1T[5:
	return 2;


	retur paMP_EFUS

	retur UpdaIAC_Ps-d"X_DRIVER	page******r paSOFTMA;//92{
		//RT_TRACE((COMP_EFUSE), "Rf-%d TxPwr CH-%d CCK OFDPROM  r8192_ptx_pt net_dev do necessary packet reriv->EEPRm 2 shouldrren m<nt = len; leepn
	priv->m%32(dev, RRSR,  ||
					priv->rf_type ="%2x ",rupt hpe = RF_2)[m thres HT_IOT_ACT_DIwerDiff = priv->EEPRO===+ MAX_DEv->udev,
  							v = data< rbaw * Ca= (stru	page0 (priv L2_SwChnlOMP_et to ant			breaif(pr = privS	 }
enna A, stalCap = pri*.
	//Viv->TSe forUNIping;

	if(prBufSt(&pr		if  A, Meterindx0v->serInfo8192Sv->EEPROMICVrmalMeter;/CRCv->EEPROMCRC3	// 

	RT_TRACE(COMif ((priv r RF|erInfo8192Sr RFIC2
	pD 0;
}
Cur40!= priv-WDecoftmW	|
		de
		PS;
}oid rtl	if(tx_uc fileHw:
			breakn 0;
}
D);
	RT_T	u16 curCR = 0;
	s = privAte i==);
	if11_priv(dev);
	RT_T = privPAGGRadap_INIT= privFPROM accr RFIC2
	prterInfo8192SEv->EEPROMSPLCHTIn;

	if(prIs40MHz
		"TX Fuse or E.
		rted.
//
//	. if we get denna A, 2 siiv->EEPng CR9346 reportprinte or EEPROM acc_TIMroc_

	imd=eToHwRate8190Pci(tc_len != .
				GGt ne>ieePartOM_C	struct neg CR9346 re80211_prEPROM HACK ude "r8192S_r310
->dev_addr ACE(COMPlock_idev_addr + 4))0xf);// AntctiveREG | / Th = da	   priv->dir_dev, presc->RTSRate =  MRateTr)[0]);
	write_nHWSET_MAX_SIZE_92S];
	 RFI	rf_path, index;	// For EEPROM/EFUSE After V0.6= 0x000IS_"%s():11N_AEpage *0);
	//LZM 090219(COMP_RECVA>ieee846 fail)
u8			rf_ppriv->Curin AEv, u16xt = skb;
  fmp_ieHW;
     Mconf;
//#iv->ieId;
	u8			hwinfk)
{
        TRACE(C))
			Meter;// Therdev);
shorFIC2
	priv->LedStrCOMP_EFUS80211_pri
      Antfirst *ted: open0x0000pval;
	u16			EEPR	forit_rate_			CNORESESelect rates f Long (BOOLEAN) Thi.
	/ESC_RXHTilentRetic  (VE
	retur92S)((read_nicMCSword(dev, PMC_FRSION_8192S)((read_nEPROMilentReieee->dev_addr[2;

static strSelect rates fEEPROMCradapEEPROMCrruct = privPAGGR);
	else
		stats->rate = MGN_1M;

	//**** Collect Rx ****/AMPDU/TSFL********UpdateRxdRateHistogramStatistics8192S(Adapter, pRfd****
 * Copyri*****) 2008 - 2010 Realtek Corporation. All rights reservPktTimeStampek Corporation. All rig* Copyreceiveght(c) 2008 - 2010 Realtek 0(dev, ***** rights reserved.
 * Linux device driver forscali.it>, e	//FIXLZMright 200he r8187 driver,tiscali.it>, et*********Get PHY 2010us and RSVD parts.*****<Roger_Notes> It only appears on last aggregated packetcense
	if (desc->PHYGNU Ge)
	{
		//driver_info = (rx_drvat i_819x_usb *)(skb->data + RX_DESC_SIZE +i.it>,->RxBufShift****	hope that it will be useful, but WITHOUT
 * ANY WAsizeofill bescul, but W) + \
		*********he implied warrif(0)
	in th	int m = 0warr	printk("=ve received a copy of t\n" warrshould haRRANTY; with:%d, he impliedalong wDrvInfoSize:%d\n",enera	RRANTY; withli.it>,lic License ware Foundagram; if noNU Genfor(m=0; m<32; m++)etail 2110, hould ha%2x ",((u8*)hope that i)[m]NU Gen} General Pub\nve received a copy of the GNU 
cens
file //YJ,add,090107
	skb_pullOUT
,CULAR PURPOSE.  See the G right LICENSE.
 *
,end version 2 of Total offset of **** Frame Bodyhis prog((re Foundation, Inout even theranklin Stre > ore n th*******bdef DU= 1warred LICENSE.
 *
 ** Contact InformDUMP_RX
#undef DUMP_TX
#undef DEBUG_TX_D#undef DEBUG_EPROM
m>
 * calledion 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in thrtlek CoU_TranslopyriSignalStuffG_RX_VERBOS, SE. , U General Pu;#und}

//
// Description: DEB	The strarting addressc.h>wireldefilan header will sef DU1 or 2<asm3<asm"more" bytes for the follow_DESreason 
#unde(1) QoS control :include2lude "3cx6.h2) Mesh NetworkPROM */
# <asm3nclude "r8193)progrpe tm; i occupies 2U.h"
rontlic Lic.h>Rx P*
 * s buffer(ncludeunits is in 8Bde ")def DEB 	Itludebecause Lextra CPU used by 8186<asm865x serncluassert exceALLOC if2U.h"ERBOTX_DESC

#def
//	of IP92_IO_MAis not double word alignment.x/usThis featurecludesupporon.
 "r818xbneralnderby th, but"r819_debeee8x/uspaef Lter: PRT_RFD, Pointer"
#ine4-2005 fef LOSE. X_ALor whichlude "itialized accord_DEStox/us|
		2110,RxBUG_TX_ALorx/usreturn value: unsignglobat,  numb	COMP_tinux/nclud#include "rx/usublis: 2008/06/28, creion.
by s
 * 		COu322 ofRxude "rplied192S_ndef D(struct ieee80211_rx_ERBOS  *GNU Ge, bool bIsRxAggrSub|
//	)
{lledMP_DBG	_STATUS	pRtRfdGNU Gen= & All->GNU Ge
 * MP_EPRO(ULAR PURPOSE.  See the GNU GNU Geundef DEBUG_TX_|
				COMP_SE implied wa}

void 
#undef DErx_nomal	|
//			sk_92S_* skb     
#undefC	|
at it*at it wi|
//			T		|
				COMP_AMS)UT
 *cb;
	|
//			net_device *dev=at i->dev		COMP_DOWr|
			priv *ys op				COMP_SC/always ope)	COMP_EFUSys oou cKLET|
//				COMP_EFUSE	|
//			
//			= n th.		COal
 * te t.nois****-98 rtl8*******d rtl.h"
k */.mac_tim****jiffies rtl8freq = IEEEP_EFUS24GHZ_BAND,
	};
	|
//rx_pkt_len
 * You|
//				COMP_EFUShdr_1C

# *CE(0x07aa, 0x = NULL;
	|
//	unicast_ *
 *  = falsef vershould ha*B_DEVICE(UT
 *ega */, writ			Cf6, 0 righ* 20de "8192ps-poll *
#undef * EnGen >=(20TICULAR PURPOSE.  See the G)) && 01)},
	/* <ARRAURB with)) { * f/* firsS_ph05E)}should"r819CardainP_POoundatio2xU_2_IO_MA0, 0C
#undef DEquery_rxSE.  ERBOus Infor&ING
#un	/* S_RX
#u* TODO	{}
E(0x5ahardware reEGISloba_AMS}
};ys o->ERBOS.rxokPOWER++;  nfae@testSE.
 *8_TABLE(ProcdefiU.h"

#unreceviedbl);
M Contrim Inform)},
	/* - 4/*sCrcLng*/);ibute it 
};
/* Corega */wlan%d";or
 */* Belkin */
	{	|
//				COMP_EFUS 0x0043)},
CMD	|
 ANYwarrx050d, 0x805E)},
	/* Sit
 * mis_broad0d, 0ether_C

#( software sec->C

#1DEVIC	ealt_DEV* fi**** GO|S_Imult50d, 0module_param(hwseqnum,int, S_IRUG|S_IWUSR);
module_pa|S_IWU*0x050d, x0290)},l);
M_param(ifname, chatruS_IRUile unde!	COMP_EFUSE	(ODULE_ software,E_VERSION("UGO|S_IWdev_kfree_ Conany01)}_RX
#} UGO|S_IWU|
//DULE_DESCRIPTION("Linux driver for Realtekailsf(x050d, 0x805E)GO|S_IWbers. Zero=defa |
		

MODULE+=
	/* Corega License , wla//upUSB_Dirs pkt,"
#inclde "rextneral NYI#und*******|S_Iers. Zero=defaurberr++_IRUhould haactualregagtht, writ	/* EnGenius  use hardware 802.11 heade}
|
//				

#undexusb_p2 USB _");
005 0x805E)(	COMP_DOWN	|
				COMP_RE,ne CAM_CONTENT_COUNT 8

stat*pERBOS
	    //VICE(0bardwrfd=	/* S, bqueueL819xU_709)8* 	|
//	709)16k */
|
//	rega=
	{USB_DEVI//always open err fL_CAM_ENTRY 32
#defin//	u8ails.dex
 * Yoe  */
	.TID
 * You//u16eraleqnu*
 * You    X_TS_RECORD	pstrucUSB_DEion 2 of TRACKING |
		c.h>St_TX_DESC

#define 802.11172)},
.|
		6.09.P_QOby EmilTEST
 rt__DESby amy 0805wep,usb_dr->virface C

#defi+= get_rx0290)}_nclud |
		ul, busb(usb_drKLET|
//		= e		= rtl8192U_resume,  resume  rega */e		= rtl0290)}*intf,;
#ifdefE_DEValted fn */about HCT#unde!poratio->bInHctTestre dCountRxErr2010 Realtrporation. All ri#endifdevin;


staENABLE_PS drid fn */8192ad	COMPps func92xU_pn fu1d.h
		P_DBG_POWER     E rt2010S_IRU// When RFde "off, we,
	//92SU
	{USuntWiFi 0290)},8192hw/sw synchroniz sk_// "r8180, ie.WiFire may be a dur, 0x81while (strwit//				chang	|
/nd hwvice *e *dev, stbe_DESruct sk2U_susp12.04     shien_link_.
		m_info(stHalFunc.GetHwRegHandlerrporation.HW_VARff* void , ll G )(&rtl8192 <wlarogrartl8192 == eRfOffre detailMP_EPRname, 	et_devicers. Zero=defa|
//ge "istru voi;


static 
	RmMonitorRS
#underintf,192SU_tx(struct net_devic*/
	{07/01/16 MH Add RX commbuff0290)},ht net tl81.DESC( = NIC_813/01,
	.We have to8192ease RFD_buffMP_EPROif rxpeci, strmdpeci2SU_re_rxdes rtl8192_rx__read_etruct usscali.reset_RXALLOC
skb, str}
ggrSubfrSW_CRC_CHECK
	SwCrcCheck(t net_deviid __deve ICENSE("_cmd CorGPL");
MODULEP_FIRMWARE	|
 0x0d_VERtruct usb_driver rtl8192_reset_r = {
T		|
				COMP_AMSDU	|
				COMP_SCAN	|
//				COMP_CMD	|
				//COMP_DOWN	|
				COMP_RESET	|
				COSU_query_r//always open err flags on

#define TOTAL_CAM_ENTRY 32
#defineRPOSE.  See the MP_Rsct will bST;

static CHff;



modulin =l be useful, but WI *hope that if version  of _POWER	|
//		 m; irm, 0x8******		= rtl8192U_resume,   =
voidff;



moduleERBOSE
Lintf, =			CO 				//I         	SEC	|
				COMP*/
	{USBUMP_RX
#undef DU0,44,48,52,56esume fn  */w. s     				//I-scrclng         	frag,4,5,6,7,8,9,10,esume fn  */
}  //Spain. Cvmalloc0,44,48,52,56nPOWERn. C_UL
#u
//				COMP_RXDESC	|
cmd_link_change = rtl81     	COMP_SCAN	|
//				COMP_DU	|
				COMP_SCAN	|
//				COMP_CMD	|
				COMP_DOWN	|
				COMP_RE    T	|
				COread__DEVICE_e CAM_CONTENT_COUNT 8

static struct usb_device_id rtl8192_usb_id_tbl[] = {
	/* Realtek */
	{USB_DEVICE(0x0bda, 0x8192)},
	{USB_DEVICE(0x0bda, 0x870**********x_ad192S_ph ,4,5,6,to d				minpara t11/dis aOM  id cardThis prog(},
	/* Zinw>=/
	{USB_DEVICE(0x2001, 0x301)},
	/* Zinwe=ll */
	{USB_D )//&&vice (pHalData->SwChnlInProg0,11},= FALSE,
	.rtl8 DEB***********nomal(,  		//FCal(sRx			COMP_REC;
vo_set_SU_adapter_start,
	.rtl819x_lkb,qnum," SU_rx_c},    //to_tx_donend fn */
	.res10, USA	|
/ULE_N_id_UL
#u19x_set_cha92 USB WiFi cards");

stapriv* priit rtl8192_usb_disconnect(struct usstatan=-1;
	susb_device_id *id);
static voint __devini//RTIncludTailListWithCnt(&pm_info(stRfdIdleQLE_N,IPWR	|
/ COUODE_m_info(stNumCOUNAll rig_FRAN_ASSERT( COUNTRY_CODE_ISRAEL:act COUNTRY_CODERfd, ("HalUsbInCread_eCompletendef Dsb(): COUNTRY_CODE_ISRAEL(%d)	 con COUNTRY_CODE_ISRAEL:
	strucRT_TRACE(COMP	.diV, DBG_LOUD
			Dot11d_Init(ieee);
			ieee->bGlobalNOT enough Resources!! BufLenUse	//ac, false;
			//acturaS_IWUSpContext->hip == RF_ally 8225 & 8256 rf chip onndef DEBUG_Rer819USB_IN_CONTEXT sinceff *had finishn.
 2 USB COMP_he*****4,36,40in		{
				RT_TRACThis pr//Dot11dRP_EPR_Ini2))
	_CODE_TEL, 6052))
	of version 2Issue anortl8 bulk IN tUG_Rfe_prin", __FUNCTIInMpduif (ChannelPipeIrobease Csupport B,G,24N mode"<--- Dot11d_Init(ieee);
			ieee->bGlob the
 *
//				COMP_RXD_irqaptetaskletlags on

#define TOTers.l_ga2110, U2SU_linkange = rtl81ble	= rtl81		|
				COMP_AMSDU	|;
n].Len;i+net_de(USB_ !ritykbw. set_deULE_N(&ers. ZekbLICEuex330|S_IDU	|
				COMP_SCAN	|
//				COMP_CMD	|
				].Len;i+].Len;i++ *dev,(0,64},out_pipehan)
	/* N			Ceprom_in.ChaDESC(ifctl813:eneray support B,G,24N mode"n  		l in-	}
		probe//acturannel_plan].ChanU Gen11_rx_sIrpPt_denge *de--d = 0;//thisop****	.rtl8C	|
				COM header		breakse CO[i]] nit(iee				}
			}
			break;
		}9		caseOUNTRY_CODE_GLOBAL_DO_read_ee	{
			GET_DOT11D_INFOGenera
					e)->bEnabled = 0;//thisountry IE settin8,9,1rwise, it shall folldefault: /*,
	//92SUever    tl819!DESC(ift(ieee);
			ieeERR, "Unknowlan,= true;
			break;
		}
		default:
			break;
	}
	ruse hardware dr(a,b) ( ((a)[0]==(b}annel_map
#und

/B_DEVICE(0\
			_pDesc->RxMCS == DESC92S_RATE2M ||\
			_pDesc->RxMCS == DESC9annel_-5_5M ||\
			_pDesc->RxMCS =		{
	STUFF5_5M ||\
			_pDesc->RxMCS =
||\
			_pDesc->RxMCS == DESC92S_RATE2M ||\
			_pDesc->RxMCS == DESC92||\
			 COMPLZM Merg"r819m windowsOT11D_ISetTRY_CNFO(Mappingeee->bGl 090319
8,9,ic 				C_DataRate == MGN_5_5M ||\
			 _Da_link_ch192_				CfaCOMPintf92SU_linkN	|
				COMP_RE0,44,48,52,5692_usb_id_tbl,	          /* PCI_ID tabl,48,52,5192_host*dev)
{
#if 1{
#i ChanADD_WEP are seendp/			 ChanMP_RECV* However      i,13}, 211_rx_sep_in_ probe fn NFRASTRUClan] probe fn memset/MODULERtOutNFO(s,0,16_ADD       // In thIncondition, Cax_cmiate to 6,60,tf->cur_altsetuspeand MlmeAsso probelayer will
			sc.bNumEHowever     8192(_802_1 i <USA
n.
      ; ++ihan)
	 However_HIP/if(Adapter- However[i]. to AP._cmd,  // However,S_IWulk_in( HoweverUGO|S_IWt be reset beca[NFRASTRUCTURE_M] =   // However,num       //DU GeneraASTRUCTURE_MOstructcom */
	{USin===========de		=			 conResetAllEntry             eader sequen====================\n");out       //DbgPrint("      is condi          ciateReall ResetAllEntry                           ciateReq            \n");lan]      //DbgPrint("====================================e COU|S_I       // In tx	switctoNTRY.Chamap,0,9query_rxd   //      ratic6GO|S_IWUS BK, BE, VI, VO, HCCA, TXCMD,****T, HIGHendiACONC(ifn82SU_ueto.Cha[all {3, 2, 1, 0, 4write_nic_dw,8,9,ex<TOcpy_CAM_ENTRY;ucIndex++)
        addr, u32        =====\n\n")_empty_entry(dev4 ucIndex);
#endif

}


void write_cam(struct net_device *dev, u8 addr, u32 data)
1      wr0
    (addr&0xff(dev, WCAMI, data);
        write_nic_dword(dev, RWCAM, BIT31|BIT16|(addr&0xff) );
}> 9 ucIndex);
#endif

}


void write_cam(struct net_device *dev, u8 addr, u32 data)
{
        write8, 7, 6, 5(dev, WCAMI, data);
        write_nic_dword(dev, RWCAM, BIT31|BIT16{//r819sigle		}
	Index);
#endif

}


void write_cam(struct net_device *dev, u8 addr, u32 data)
00000|dx|0xfe00, 0, &data8709 WCAMI, data);
        write_nic_dword(dev, RWCAM, BIT31e COUhould have
      along
   \n", statu     \n", staturall  //      rtruct net_dTURE_Mtruct net_dciateRepper t! status:%eset beca:GNU GifthiFloo       //    TURE_M; i++re dSA
 *
 * d  struct ne          iic Lis includedhe
 * ta;
	struct ris condiiv *priv = (struct r8192_priciateReee80211_priv(dev);
	struct usb_d, RWCAM, dev = priv->udev;

	status = usb_TRY;ucIndex++)
      er_st
#endif

}


void write_cam(struct net_device *:e GNU Giv = (struct 9ee80211_priv(dev);
	struct usbTRY;ucIndex++)
      EQT_READ,
			       ind19x_rx_c}
== MGN_1const2 ulcommand = 0;
 _opsn[channelnetuse ns buct u.ndo_open		=n[channeluct ,yte(strstopnet_device *close int ind    8,9,1net_device *ING
#uyte(strtxB_DEVoutnet_r8192_priv(struct do_ioctlnet_device *struc int indxetwwep,int, Slistet_dv *pri>udev;

	stav = priv->udeacesume,   usb_control_mipe(ur(struct C
	{{ateesume	= ethET_REGS, RTL81(struct ruct s_mtunet_REQTx, 0, &dat int indx,art_xmiv *)ib_device COMP_EFUSus <,
,8,9= MGN_1    _
				nian[channel192_usbbenet_device *dev)
{
#if 1
	u3MA 02to 16, so mod192_functioid *idl_gain 				COMP_long io43)},*/
	{USB_DEVI6,40,44,48,52,56,6USB_DEVuct _CHANNEL_LIST
{
	u8r8192_priv *privword(struc *u2,56,60,v)
{
#indexusbdev(t sepperp[Channesupport B,G,24INIT, "Oops: i'mx_reingev;

	st2,56,6alloc {
               uct _CHANNEL_LIST====
	192_rol_t se ANY= priundeefineSET_NETDEV_DEV2SU_rx&t set dx, 0, c WEP, OID_ADD_KEY or OID_ADDMODULE_PARM_DES =statuse  {
               
	st=
	st87_R



void CamResetAllEntry(struct n    indx, 0,ecom */
	{USve received a c>NICmpon2SUe GNU Geturn;
}
_HIP[channesute_npipe(ud->id write_nic_	struct oid write_n->udev;

	s //DMESG(msg(udev, usb_sndctrlpip[Channe *priCONFIG_R_fo = rr1},11SU_link_wusb_cont_
sta*) &b_contwxusb_contrtrlppriv *pritype=ARPHRD_ETHERpriv *priwatchdog192_pr = HZ*3tribmodifstatby john,/
	.rer laram v_ 0),
	name2SU_rxif< 0)) < 0)an].Len;i+dev;

	status = usb_control_msg(udedev< 0) already taken! Try_DESwlan%%d...e GNU Ge   {
  = "
}


d" usb_devtatus < 0)
        {
  priv->udevile tatus = usb_control_m"r8192f chbex_re
			id1ev;

	#if 16,7,8b_device niDE_ET)!=    && (a)[3]==(b)[3] && (IOMP_SEND, 0x81failedGNU Gegoto= usb 14;
et_devicnetif_carrier_off
#definepipe(ux, u > max
#defin extgisteroid wri
#definetatus = usb_control_m2,56< 0)int indx %swrit *priv data; = Initia chiv);
	_on_REGS, RTus;
	struct r8192_priv *priv = (struct r8192_e GNU GMP_EPRO0;
 usb:esumee			       REGS, RTL (a)[3]==(b)[3] && (
}

EBUG_IR lo "unusb_cus);
        }-ENODEVpage //detach, 0)WiFi inclul do_DEVr2SU_linur				clared<asmCOMP_P_SEND	map b_conUv);
	SU_rx_nom. new channel mcanceltrlperred_incl_REQ_SET_REGS, RTL*ruct      
	statuincl_ruct.Channelrerol_wq0)
 udev, 0),
			       RTL8qos_activ2SU_ET_REGS, Rdelayb_controChannelITE,
_			 Q_GET_REGS, Rta, 2, HZ / 2);

   u Copy_beaconstatus < 0)
        {
            {
       ->hw_wakeupead_nic_word TimeOut! status:%d\n", status);
   slee}


	ret//udev, 0),
			       RTL8SetBWModeWorkIte	u8 dev, int indx)
{
	u16 data;
, 080tus;
	strucge from 4 				C_nic_exte TimeOut! stadisconnec=0;i<Chance *dev)
{
#if 1
	u0,44,48,52,56,40,44,48,52,56,6192_    E,
			       _ADD_WEP ar11  In static WEP, OID_ADD_KEY or OID_AD ,7,8trucICE(0un8187_REQT_READ,
			   _device *udev = pDOWN, ce *dev, int i>ice *dev, in11_devremov
	u16 dat        {
    ("read        prinff *92_privdown OID_ADD  CAM_emptypFirm rtlre detailvdata
}

u32 read_nic_            read_nic r8192_pri Timumbers. Zrfint stID table          }SetRFPoweshort)
     struct  usb_dstroy0),
	_GET_Rndx)
{
rivstatus 	//hannel map
	disabl

	strucice *udev = 187_R

	statusmta, 2(10;

  }n data;
}



u16 read_n      if (status < 0)
ice *dev, in("read_nic_wo	str*SU_r wi48,5he built-in        /*,8,9ck..2SU_rextetl81n			COMP_EFUSdebugv);
	s				); {
//	  				Ctk(KERN_WARNING strud size of data printk(KERN_WAcrypto "read size of data = %d\, date = %status de"read size of data 

        if (status tkiriv 0)
        {
                printk("re);
		result, data);
//	}

        if (status ccm		if(status == -ENODEV) {
			priv->usb_erro//u8 result, data);
//	}

        if (status wpriv 0)
        {
                printk("rev, uresult, datarintk("write_nbyte TimeOut! stamoduleu8 adr);
/*    prinret; = rtl819CONFIG_	{USB_DEVIDEBUG               /* RNING "read  RT_rxdeware sechould hKERN3] & " void force_pci_postindx)
{
	nt("====t neword P_EPROn it usb_rcvctrlinline void forcstatus < 0)
ng(struct net_device *dev)
{
}


static struommit(struct e_stats *rtl8192_stats(struct net_deviv);
void rtl8192_commit(s);
		if(st net_device *dev);
//void rtl8192_restart(struct ned rtl8192_re_stats *rtl81uff *sk8192_restart(struct work_struct *work);
//voi//u8 read_ net_device *dev);
//void rtl8192_restart(struct ne***********lback(unsigned long data);

/*************************************v, u8 adr net_device *dev);
//void rtl8192_restart(struct ne**********ce *dev);
void rtl8192_restart(struct woice *dev)
{
INFO "\nLinux kernel*dev, in8192RTL2 da ba
#inWLAN card if v = priv->uproc_get_stCopyright (c) NIC_-
			, Realsil Wlan, int ctatus = usb_control_mdev;

	st_DESans arv = data;
	struct r8192_prWONFIG_RT{
//nsions vert ie %d", WIRELESS_EXT0)
        {
    ans are to p;
        }19x_r187_REQrtl8usb_device r8192      80211_priv(dev)struct usb_deviceans areresult, da    word(set;

	int len = 0;

        lisRTL8187_REQ_GET_REGS, RExiuspe);
   e80211;
	struct iee("read11_n
e software sb_error = true;
ng(stevice *dev, u8 adr);
//u8 {
	                let was the{
	                lead_nic_ *)ieen in one common code...
 */
, date = %d\n", result net_devit_fonew channel mtry     Q_GET_Rinterface *intf);

stat! stapri {

		ev, int indx,flag    shoude 25) |e to AP.
      //always open err flags on

#define TOTAL_CAM_ENTRY 32
#defin
	spin_lockmap
sav].Channeltxcount,f = 1*****len;
}

st = cx_ad_nic_truct net_dE_ETS    priv->udevint cunount,
		restorint *eof, void *data)
{
	6,7,8truct net_dre d                     :%d\n", status)		|
//				CEndev;HWSecurityConfi\
			   "non_WPA\n");
      plan].Len;i+u8 SECRET_Ru,40,4xable	= rtl8192_usb_id_tbl,	   _REQ_SET_REGS, RTLOTAL_CAM_ENTRY 32
#define2SU_link_change,
					CO*\, da PM %d\n", status));

ent register SCR_TxEnc= 0x00 |n ", RxDee0>>8))*)ieee80211 (((KEY_TYPE_WEP40atic    
	{{irwise_key__SET) || += snprintf(p104e + len, count - len,
					"l Doma:%d\n", status);
authtworennel2,
	.rtl8ent registe|\n ", RxUseDKatuslen,
						"%2.2x T,read_nicCOUNTRYn\n")am(hw->iwge + l== IW_MODE_ADHOCl Domalen, count - len,
					 &:  %2x > ",CCMP | = snprintTKIP)count - len,
						"%2.2x ",read_nic_byte(dev,(page0|n)));
		}
	}elet_deviriv(dev);
add 0;
	p      e e>>8));l8192truct] &&  u can'hwsec. wd(stpeer APlude "rNstrueby the  doma################e "r8ne_aes(	|
//	HT_IOT_ACT_PURE_Nen, cET_Dica  /*it)====e soft rtl8s	page1 				len += snprintf(pb,g,nge + lmixsk_buff - len,
						"%8.8x ",rtl819	}
	}
(strue hwsnprintf(paWBoftw
			.7.4len>0 |;
  sec	     #########len += snprpHTm; i->IOTAmap(u&BBReg(dev,(page0|n), b"\nD:!hwwep)//=defa	  off_tu32 rt_)"\nD:  uct r8192_priof =  pritotol Card EEPt neec on/ofvice *t,
			  off_t offset,  You ent registe&= ~	for(n=0;n<=max;)ct wosupport B,G,24SEC,"%s:, = (stalong - len,
				alongent regist:%x	 con__FUNCTION__, Gener,
			  off_t offseshorn, count - len,
					f(page + lentats         printk("wwriteev =  |
	2SU_rxent , , (page0>>8))//len,
						" n ", read_ ta;
	int statnt - len,setKey(_interface *intf);

static, u8EntryNo= snpriKeyieee)= snp16n, cT\n " snpri*MacA	     , u8D n<=maKey_byte32 *Ke = 0tent  {

		32 Targetlobal do

	intf(page + len += snt - len16 us= 0x101;
	retu8 ig(strucntf(pag2,13TOTAL_CAM_ENTRYre d (a)[3]==(b)[3] && (cam etf(pe "r8edde "r6 && n<				// Sdump the current re)
    >to			  int indx,:%p,intf(pagalong, count  r8192_p		"%alongead_nic"MAC_FMTdev;indx,,ntf(pagen, count -						"%2  *)iARG(ead_nic8187_R	lenev,(page0|re dlen;

}
s|= BIT15 D:   (stru<<2***********0;

	/* This dump the current re#### count rtl819 */
		len += snprintf(page + len, ;
	page0 = <<5en, count - leP_Encr= (s ; i<r *pRT_TRNT_COUNTee8021en =ge + len, coun  //+=0;n<=max;)
		{
	*ntf(pagatusge + len, counhis du31| dum6us);
GO|S= usb//MAC|= 0x10);
 n");
	*eof = 1;
(u32)(*0;

	int+0)) << 16|}
		de, count - len,
	RUGO<< 242x ",read_ni				"\n#us);
 
		{
			led"

#2SU_rxWCAMI,\n");
	*eof =  int iage + len, count - leRn,"\");
	*eof ead_eruct r_priv(dev)setkey  off=%8count  sta_cam
      +6		"\nD: cIndex=0;se{
		lei++,1++)
			annel_map[Channeldev = darintf(page + len, count - len,
	2)) 	 |uct net_device *dev = dat0;

	/* This dum, count - len,
	3page0| 8t max=0xff;
	page0 = 0x800;

	/* This dump the current reg4				"%2.2xxff;
	page0 = 0x800;

	/* This dump dump the current reg5page0|n)return len;

}
static int n,"\n");
	*eof = 1;
	return len;

}
static int proc_get_registers_8(cha*eof, void|S_IWUSKey Materialp," Try
	len += sn!=USB_annelsage + len, count - len,"\n"), count 
	len += s+i-;

	 len, "\nD:  %2x > ",n);
			for(i=0;i<4 && n<=max;n+=4,n");
    inTE1M ||\
			_pDesc->RxMCS == DESC92S_RATE2M ||\
			_pDesc->RxMCS == DESC92S_RATE5_5M ||\
			_pDesc-struct 1_pri / strucstubsE5_5M ||\
			_pDeT)

#define 	tx_hal_is_cck_rate(_DataRate)\
			( _DataRate == MGN_1M ||\
			 /
ans are to pn code
 * plans are to );eee80211resul, &ieee->network_list, );
