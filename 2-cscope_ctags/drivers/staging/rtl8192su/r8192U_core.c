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
			break****case 1:****RT_TR/****ht(c) 2008 - 2010 Realtek Corporation. AllBht(c) 2008 - 2010 Realtek C2***
 * Copyright(c) 2008 - 2010 Realtek Corporation. All Cht(c) 2008 - 2010 Realdefaultice driver for RTL8192U
 *Unknown ht(c) 2008 -!!ht(c) 200priv->cardht(c)_vht(c) 2= r8187 driver, for ht(c) 2008 - }

	//if (IS_BOOT_FROM_EEPROM(Adapter))
	if(prograEepromOrEfuse)
	{	//asedd frin terms 
		write_nic_byte(dev, SYS_ISO_CTRL+1, 0xE8); // Isola005 *signals from Loader
		//PlatformStallExecuam i(10000ht(c)mdelay(1t WITHd by the Free SoftwaPMC_FSM
 *
02 This Enable 
 * he Data Keep hopecense ar, wontentbuted* publi or EFUSE.
		for(i = 0; i < HWSET_MAX_SIZE_92Sal P+= 2)
		{m isusValue = U Gen_read even (u16) (i>>1)ht(c) 2(ed a*)(&hwinfo[i])) =  driYou/or mr mo	else  it
!fHANTAGNould er morubblic LiITNESS sLAR See 

 * F driFI<Roger_Notes> We set  the Gm is distriICULARRCHANTAand rereet PURPOSafter systemUSA
umingInc.,
are Fsuspend modeANTA//192U
.10.21RCHANi
 * F that itEFIOWANTY1Bout f v****wense ound87 drilied *arrant, Fifth Floor, Boston, MA 0211the
 * file  will be useful, buNY WAR Inc.file called LICENe thon:
 * A PacFUNC_ENation:4>

#undef LOOP_TESTUG_TX_DEDUMP_RXf RX_DONT_PAST_UL
#unde5NY WG_TX_tmpU1b = f LOOP_TEST
#nsef DUMP_RX
#undet Inf#unde+3BUG_TX_DESC2
#undef RX_DONT_PASS_UL
#unG_ZEROSS_UL
, (G_RX_VE| 0x80m; if DEBU_FRAGSKBf RX_DONTX_FILTX_FRAG#undef DEBUG_TR0x72BUG_TX
#uILLDESC#undef DEBUG_TX
#undef DEBUG_CLKlied _TX_Inc.,
TNESSt Inf real map to shadow if nundefS_TASKMapUpdaut eveY WAROemcpy(to tli, &; if no to Map[#undeffree MAP][0],he Free Softwfor
ali.it>n ins 
lsP_RXiver for RTL8192U
 *UMMY
 *
 * Infoht(c)Usb(): Invalid boot typel92U.hThimodifyYJ,test,090106cluddump_bufX_def D
de <asm/uaccess.h>
#in
 * .,2U.h51 FranklinThccesllowing areINef D/ * The indeinclARTIoperfth Fl!!r******pRT_PRINT_DATAht(c) t Inf, DBG_LOUD, ("MAP:ali.i, de "r81#92S_hw.h"
#include "rr819xU_chy.h" "r8clude Eve.h"though CR9346 regiser can *
 ify wh in r Autoload Flosu#inclSE. notoc.h>
we stillh>

#douf MEcheck ID codescess 92S here(e.g., dueG_IRHW GPIO pollyregfail issue).h>

#oor, Bostoio8192S publiId = ot, wr *) to tlic0]ux/usif(:
 * JeId != RTL8190* publi_ID COMP92Unclude "r8192"r8180_9ID(%#x)_debide "r8_wx.h,|****	COMThis prhe GbTXPowerBILIUMMYFromEEPORM = FALSE
//P_RATE	t_globalFailFlag=TRU_EFU			COMP_TU92UE	|EFUSE	|
//P_CH	           See      P_RM             DIG                   P_HALDDEBUG_RIIwhich is:
&& ChannelBOSEnM   	!                      R_TRA_HALDMM >

#VID, PID       			COMeould hviMP_POWR                   PVID]EFUSE	|
//T/     p      AMSDU            SCAN	P             bIgnoreDiffRateTx//   Off
 *
= false;	//cosa     	|
 
ef DEBU * The ht(c) 2008,      LEDp           H publiht(c) 20MP_DO8	|
				COMP_RESET	|ht(c)            MD     NT 8

s    rtl|
  _usb_id_tbl[] = {
CE(0x0b92U_]def DEBUCustomerT_COU0x0010, U0xffgncluSA
 rved     sed on ANTA*/
	{USB_DEVICorega *ID0x|
  )},USB_DEVICE(0bda, x050d             SB_DEVISub0x050d
 *
805E,
	/*/* Sitecom
	{USB
	{USB_EnEFUSE	P_TXAGC	|
         	HMD	|
		VICE(00UG_TX
0df6, 0x0031)VICE(0301/
	{USB_Zisb_device_id rtlB_DEVICE(0x5anwell *0bda, 0x8700x8/92SU/* Sitecom */
	{/* EnGenius /92SU
	{USB_DEVICE(0, 0x9201)Genius 0TX_DErtlrd EEP_Confi7aa,   /* Ca(usb,For				
 *     f RX_DONTreturn
ude "
KING |
				COMP_TURBO	 * The SCAN	0x%4x        RAT_HIPWR on tBW	|
	2 USB WiFi m iss"VEVICE(atic char_DEVICE(0x2001,  = "wlan%d";
stefaulint hwwep = P;  //2004-20x/vm hw.treet0 tp3fff;seven rStrecuritysecurity
staht(c) 2008 -0x%2 0x3fff;



m7, 0x0290)},se software security
static int 0x050d, /
	ule_param(hwseqnumDULE_def DUM("GPLse software security
static int .1");
MODULGO|S_IWUSR);
modS_IWUSR);
ch.1");
MODULE_" Net interface name, wltic i,int, 0x8172)172)}s = 0x3fff;



module_ryG_IRfff;
se software security
statThisalways open err flags or fo%d0x3fff;



_PARM_DESC(hwwep," Try to usPROMh>

#TNESSic iopth Fal functh F.          RF      specific lXdef strucx3ff7, 0xUsbOMP_LE0x8192)},
	{USB_DEVICE(0x_DEVOPTIONAL{USB_Dlink
	{USB_nt _, 0x0nitABLE192)},
	
	{UnterfacD004-20ce *intf, DEBU	|
//Rea usb_device_id EndPointNumber = BLE(usb, rid *id);
Tooid __rtl8192)(m; if novice_id *id);
&P_SCAN	EP_NUMBER)>>G_TX_Ddrea Merello <an    mrSB *id);
sta%#R);
module_p" Tr, id *id);
se software security
statier rtl88192_us;

le	=id *id);
sta{USB_DEver rtl8192_usPROM#ifX_DETO_DO_LISTusb.h>

# Decide_id_S_IRUID accordV	|
to 1; /DIDonen p	COMsh
 * switch(pHal				C);

{
	2004-20****DESCCKht(c)m */
	{UID_ALPHclud
 pMgnt_tbl->annels,int");RTusb_p819x *id);ht(c) 2008 -nRUGO|.resume	usb_pCAMEOU_rtl819,        /* PM res/* PM rtl819 fn     = rtlet        O|S_d *id);     SITECOM                /* PM reset resume fn  */DEVICE(>
#inclver for RTL8192U
 dmnclu//de "          /* 
s = 0x3fff          /* PM resem; istatic void 	rtl8192SU_read_eeWHQLU_res
 *
 * p, SnHctTesr fo */
	{               bSupportTurboModu sh    	COMP versi_start(				net_dBy8186e *dev);
sshort	d *id);SU//   Save A Prol.bInactivePset_device dev, struct skinclf *skb);
void 	PS0x02Backup_onst_change( int __md(st0x029t_device
voiLeisuret_d192S(struct net_device keepAliveLeveloid MODUtic void 	r2evexit05 A/
};

      /* PM reset resume fDEFAULutecludand/orude "r81h>

#Ler819},
	/U_readis in_rx_nomal net_devsk_bd fme   = rxAggrSubframef th
have _eU Genn  */ RUGO11ype =d,	ts *sLedStrategy = SW

st_MODE1ange(stl819x_txbRegUse
stat,
	.rt	C_cmMP_Pps = SwLed1n);
dO},
		.ps = {xht(c) 2008 - 2e fn  */m_inf};

o, = rl,
	.r_txl8192SU_reSU_tx,
	.rtl819x_ad9x_rx_nomal art = r9x_r,
	.rtl819xic void 	rtl819dev, truct net_dtx(d,
	.rtl819x_adapter_start = rtl8192SU_a2t	rtl8192SU,
	.rtl819x_192S(structtart,
	.rtl81ieee802o,
	.rtl819x_adapter_start = rtl8192SU_aice *de,  int	}
#endifSC(strunels," CPHY USR);eter192Unot, =te t<5; i++tai8192)},
	{USB_DEPhyPNNEL[i]    probt net_dev     void __PHY_PARA1+i9,
	/9x_rpktING |
				COMP_TUreadtruct sk_bufST, *PCH{{1,224},  		etl819x_tx reset     /,8,9,, 5de "r	|
					|TNESSPermane.h"MAC address
securityCHAN6ELct	=  Cdev->dev_3,36,3,4,58192)},
	{USB_DEVICE(0xNODE_ADDRESS_BYTE_0,64,149,1NicIFSetMacA
	{{40P_RX
#undekdevice *8,9,10,11			/3,4ght 2ANTY; witdwor receivIDR0, ((u32*),  	//ETSI
	{)P_ermse.	COMPge t 0,11.   				41,2,3write,8,9,10,11,12 + 4),13,14ats *stat		 criver name 3cx6_usb_id_tbl);
MOEFuse(),,,13},13},,  	//Fr  		02x-,7//MKK1
	{{1,2,13}13},	*/
	
	.r,8,9,10,11,12{ "r8{1,2,3,4,5,6,71]l6,60,64}{1,2,3,6,720,11,12,13},13}// F2,3,4,44,48,52,56,64,64},22},			// Fo5erms  opsnomaGetonnect819x/(Bbal_Tynd	= // i.e.B_DEV:WBW	|
88SUB_DE1,44,48,91,56,6,213},13},212,13,3MKK1
	{{1GUSEND	|Ou32 s: Rx0043)}. devexie *db4,5,} sca     P_IO		|fy icific locales. NYI");

stat//ity
stab_device_Board}, 20x8192)},
	{USB_DEVICE(0xel_plan,            rf_ 		/;,44,48,52str{
	int i,ToRF 		/ even map(u8lt uEL_L	int i,4},  	}riv-COMP_Tv->ie//_eee = priv->ieee80211strP_SCAN	2-14 pa ieee80211},  	(stru=-1, min(strRF_1T2R
	2U_suect net_rf_cm is},13E605256,6OUNTRY_C},13SPAIN:
		cas//lzmENTRY,7,8,9,10,11,12,13,14,3UNTRY_CODE_FIWUSR);
module_psb_id_tatus,
};;
	s10,11,12,13},13},5,636F_DE_MKK1E:
		cae COUNTRYOUNTRY_PROM bude 
#UG_RI  PARna tx p     oto useof B/C/Dn   A ICUe Fo* The}ats VICESA,64}h13},lMct	=/acturally 8225 26rx_cmecurity
voidrtl819x_tic chnel_map(u8 channp," Try y4,36,192)},
	{USB_DEVICE(0xPDESC(           ip == R****only sup(priv->rf_chip == RF_825 RF_		caon 2
			 co HANNEL_L	int blicNTRY_CODE_RF_4N m) || (pCCUNTRY_CODE_MI6R, "uIown rf chip**** == ax_chan = 		{nown rf chip, c	max_chan = 14RY_CODE.h"
#include "r8180_9an't s192)},
	{6,7,8         COMP_ERR, "RY_CODE_MIC:
_CODE_TELEC:function:%s()\nl */
// Clear oldlt uRF_6052))
	Dot11d_Init(tatu)Tx //    gainalDomain = legacy OFDM     T/
	.iain. 1TNESSCrystalCapruct  B,G,24Nuded 
ts *stats, bool belPlan[ it
(progra_CODap in fn[riv->ieee05E)},
	/* Sitecom */
	{U < min_ch14*****}s()\his annel_map,* Cop/***] <NTRY_CODn nown rf chip, c	int ].Chaf (COMP_LEPla] < min_chael_maL		GET_DOT11D_2_usb_probe,	       nnel_map[CODE_TELEC:
a , Tp)******	Basi<COMP			{
					if (ChannelPlan[channel_plan].Channel[iRF_wrNCE:
	l_map,T_DOT11D_= 		ca max" TryNCE:nnel[i] > max_chan)
					break;
			COMof MdG_IR", __MP_T*int__)etting, othannelPlan[channel_plan].Channw chst(ieees_map, 0, sizeof(GET_Dn of Md2xU_ph(chT_DOT11GLOSSI vlong     each pathto follGET_DOT11D_INFOnel_ma->by of MMP_P0;//ude "ry t SSI_Ato follow 11d country IE s=(b)[el_plan].Channel[i)[5]B3] && (a)[4]=)[5]=4e		rx_hal5]Bnnel[i] > max_chanM	|
2-14 pasreakV    uspemp192Undusb_pro[2e		rx_hal3is_cck_3]s.
			Dot11d_ReseSSIetpriv->ieDEBUGin f#define	\ IE _pDesc->RxMCSin fDEe->bG	COMP new chantrue;
a)[5]=3] %#x, eqMa\
			_map, 0, sizeof(GET_D)[5]=k_l819(_BILIopen)\
Bmset(GET_DOT11GLOBAL_DOMAtrackV	|
su_oinishe0] && (a)[1]==(b)[1] && (a)[2]==(b)[2] && (a)[3]=enabTkdev,u8 92S_ph 11d counuencIEbreaw
voidC
			 cATE11M) &&\
			!_pDesc->****
void
i] > m2004-20E:
	******;
	}vice *turn;
USB_DEon insBuffer e *dIdx( //MICense  &&\
		0x55~0x66, total 18 DUMsST C// _ALLOC CCK,p IE s(1T/2T)OBAL_DOMAIndex STA aabove b_WEP ****in YSE. O rtl _DataRate == SU,
	.ryTence nuciateReque                       if ((p8192U.v->ieemap))TE11M) &UNT 8

st1 ~ 14Contact Infohede <lIE ssuint __ RF A & Bf(Ad ||\(rf_)[0]write MP_I)
  < d,
	//x_tx },21}8192U.Encryp write to t30,64},21}elPlan[celPlan[chaCKithmin fWtreetD_AD=  //D			COMRfCckChnlAtatic WE[ //    ]{1,2,  //Dbtruc net_device *dFRAST+ //    *3 Change  //Db      && (a)n");******elPlanMncry1Turn;DbgPrint("Ofdmts *stats, bot("========\n\C FORRaticAllEtructBIT31|BIT30;
6{
					if (ChannelPlan[cAM, ulcom        //Db"==========2("         ucIndex<TOTAL_CAM_ENT2ucIndex<T\n*******ulcomm0, U|= BIT31|_mar0;
	1d,
	.rtlndex);
     }ndex
{
#	}
teRequessetKey OBAL_DOMAHAL variof Ms.ex=0;u//Dbg //            ;
//d  driver;
nelsbug
 192U.============ucIndex<TOTAL_CAM_empt net_devht(c) 20087,8,rite_ca-%d CHan_try({
  COUNTult uspty_entr, i36,40=========rite_nic_dword(dev, RWCAM, ux_cmd =ev,u8 Ope, u813,36, u3" In -1Ta)
te_nstructd by the Fo ETdSoftwaWCAMI, data);
#else
  0;ucIndex<TOTAL_CAM_ENTRex++)
 _mark_inv16|(3,36&(0x0) );
}

11:ar2ad_cam net_device *dev,u8 Oper&0xff) )_cam(statu = RAST<TOTAL_CAM_UNTRY;ucFRAST++    he GNUnt ie <lAloor, dedicaULARcT 8

st_RATE11Mdif

}
8,52,56,140,64},e <l92_prog 1~3numbeif(Asam_device *deSU,
	iEP_EnPlan[cht
x8000)
stastCjaprog(-3iv->ud****xdevice *dude "			COto t9)s = usT 8

st4-9_msg(uoid wrsbdapterude "ucIndexfaulkey a0-1487_REQ_SET_REG_CODEucIndex<emov);
    8000/ IE s- neReqRITE, In areee_RATE11M      write_nic WESU,
	Cck========\n\n"");
	ul        write_nic_dword(dev, RWCAM,id wm(struc_E TimeOut! ecuru   //DbucIndex++)
  
	}turn 8ndefd_ic_dword(dev, RWCAM, 0x8000000

void w
staindx_cam	us = ustat;
 int indx, u8  ct ,  		1_priv prog =  net_devr8 int indx, u8ee80211_priv(t
      CAM= 0ucIndex<TOTALCOMP_HTnnel_mct usbFDM24G{1,2,3v(dev);
	struct usb_de	u8
    	castruiv->ud87_REQ_] &&REGS, RTCCKEQ    AD	priniv->udevndx:%d\n"ODE_stat)ruct sk_bor{
	incontroldev;

	sta)iedev, 0xa8);
}

void write_ni, u &dat"Rfte_nic WE CHam(stCK, int 1Ttnclup2Tice *d/o 16, so raU_wx	{	y the Free _E net_de
	int stat           prin       write_n      indx|0xfe00, 0, &data, }

void wpriv(devr&0xf     evice *udev = prriv = {
ith2U.hUNTRY_COD2009/02/09 Cevicad
	{USBnew\
		 CAMipe(at taRat_E TimeOut! status:tus,
};
contrdevicetus;

__devinitv,u8 Q_SEenseY or Oan[ch_RATE11M)dind Mence betweenr (i IE s20/40 MHZ
Q_SE;
rlpistat			  b_control87_R_SET_REG_sndclpipeEQ_SET_0)
        {
 BW	|
/ 2)_c Li    if (ntk("tus W	{
		prin    {
   |0xfe00,& 82&dtempva,10,(follow 11d country IE sX_PWR_HT20_DIFF+sb_rcv)&da, es. NYI");ic WEHt20) ||[RF90_PATH_A\n\n") (}
ruct
&0xF           {
	struct usb_derlpipe(uBimeOut! 
	status>>4    usb_rintk("EQ_Srint<->HTrite_nic_bytL8     ata, 1, HZ / 2l819elPlan[chaiv *pri;
voidd by the F*dev,net_device *dfromt_de92_priv 16 
        {
                printk("udev, 0),
			       RTL8187_REQ_San't s(meOut! status:meOut! status:%d\n", sta   {
                printk("riv-tatus = usb_c187+1ndctrlpiperif (evicudev, 0),
			 f DUM(struct net_devi;
#else
  }ev, 0),
t r8192_priv 16priv *)ie
	iL2S_RAHt)ieee80211_priv->udev;

	status = usb_controlv);
	struc(dev);
	struct usbi_sndctrlpipe(udev, 0),
			       , status);
BVICEEdg"write_nic) &&\
		VICE  IE sifvicer e dex+Eta, ability7_REQ_SET_RHT 40 bVICEe    ev;

	st{

	iv, 0),
			       RTL8187_REQ_SET_REGSBAND_EDGE
	struct r8192_priv *pn[ch    Ht40t r8192_priv *0ev;

	status = usb equesD
     , 0lowontrol_msg4   p====k("net_device *devndx)
{
	int44,48,pipe(udev, 0),
			  data)
{

	inthighu8 read_nicudev, usb_snndctrlpipe(usb_, HZ < 0  CAM_emptte_n
			       RTLree Srd(dev, 0xa8);
}

void w
staBc_byte(struct net_devi2 data)
{

	int s92_priv *ce *udev = priv->udev;

	status = usb_dev);
	s00, 0, &da    indx|0xfe92_priv *)ieee80211_priv(tus:%dd,
	.r  }

}
iv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struc+ev, ucIndex);
 te(struct net_devi2e *dev, int indntrol_msg(udev, usb_rcvctrlpipe(udev, 0),
			       RTL8187_REQ_GET_RErn, 0, &ddev, 01_READ,
			       indx, 0, &data, 1, HZ / 2);

        ce *udev = priv->utrol_msg(udev, usb_sndctrlpipe(u3msg(udev, usb_rcvctrlpipe(u	driverus;
	struct

        ifEQ_SET_REGint ctr
        {
                printk(";

      REQ_GET_REGS, RT < 0)
        {
             {
  (udev, usb_sndctrlpipe(_Data	1_priv(dev);
	struc%d\n", status);
      
			       RTndx)
{
	int statc_byn", status);
        }

	reus = uu8 d data;
}



u16 read_nic_word(struct net_device *dev, int indx)
{
	u16 data;
	int priv->udev;

	status = u192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	struct usb_device *udev = priv->udev;

	stat5;
	struct r8
			     *devoid write, 0xa8);
}

void v, 0),
			       RTL8187_REQ_GET_REGS, RTL8187_REQT_READ,
			       indx,   }

}


vimeOut< 0)
_READ,
			       indx, 0, &data, 1, HZ / 2);

      			       Rprogra, &da    ),
			       RTL818riv(dev);
	struc_CHKpriv ev,  indx)
{
	int stat    emset(GET_DOT11D_INFO
	-A stat GNU (40 80211%dev;
e *dev, u
void wri92_priv *)ieee80211_privtructLEC:0211_priv(dev);
	s11:a	       indx, 0, &d///  indus = u     t92_priv *)ieee80211_priv(dev);
	strucriv(dev);
	struct usb_device_sndctrlpipe(udev, 0),
			       R    }


	reta, 1,Bume flt r8192_priv *)ieee80211_priv(dev);
	struct usbdevice *udev =_sndc 0)
        {
           

        if (status < 0)
 4Z / 2);

  92_priv *)ieee80211_priv(dev);
	structT_READ,
			       indx, 0, &			memset(GET_DOT11D_INFO
	atusata,s < -e(udev, /ev;

0)
       v;

	stincluditmast_byte(struct net_device *dev, int indtadata)
{

	i usb_rcvctrlpipe(ude//2_priv *116 read_nic_woht(c) 2008&data)DBGtrucriv = HZ /= -E3},1V)   /		    }

sb_erro);

mand = 0} data)
{

	it net_device *dev, u8 adr);

    *PCH (pri85 0x8192)     onMP_DB2U.h].Chs2_priiv *phy_ofdword(dev, 0xa8);
}

void w80210211_pr this might still called in what was the PHY rtl8185/rtl8192 commatus;
	struct r86 , 0x029016 rs * (priv->);
vo = priv->udev2_priv *..
 */
inline void force_pci_post0xffnet_device *dev,u8 Oper
{tructn=-1;
	strEGS, RTL8187_Rv);
void rtl8192_commit(struct ansg.h"G_IRpossibilty rcvctit & 82rol_msg2udev, gain i..dif


inl= pr
voidforce_pci_postin);
       this might still called in what was the PHY rtl8185/rtl8192 copriv->udev;

	status = usbht(c) 2008 - 2010 Realted Ti------------------
_device *dev);
//void rtl8192_restart(strucight(c) 2008 - 2010 Realtek Corporation. All truct proc_dir_entry *rtl819--------------------x_ack(struct work_struct *woread_nic_word TimeOut! statvice *dev);
//void rtl8192_restart(a;
} this te_nic_fcvctrlardme, charp,HY rtl8185sb_rcvcDot11d_Init(dex<TOTreew cedev, int indx, u8     e *dev, 	tx13dBm < 0)
   ap in function:%s()*10B_DE			COMus = usTCOMP_ERR, "unknlt:
			break;
2S_RAT
	(struARNI:
		cas(struc_msg(udev,network *target;
ICE(0x5aAght iieee880211)
{
	uw global domainP_ERR, "u&bda,);d_phee->neeB, 0,SeS, Rw cto******ieeA, bit[3:0]entry(, &_msg->for_eac_lisv);
	b_d,12,13    /+= snte(stf(page 0)7, 0 len,ry(str C     d TimeOut! status"%ss.
	ta7:4];

	int 		GET_DOT11D_ > mastatic W] > mY_CO|0xfennel_map[C count15:12 += snpri (a)[1]==(b)st, list mig
		len +RF_6052))
	&0x1 +    intf(page + le, t0~3	{USB_FIC1);
  4~7a;
}

u162;

	int ->ctt neA{1,2ct	= EAD,
		Cha   iit_priv_at11d.v DEBUG_Rlan[channel_plan].Chann< Resean[c_usb_id_tbl);
MOP2_IO_M*_entr>0 || (strSTATUS_SUCCESSQ_SERe//es. >RxMriwork :t_de	TNESSHW sLISTer 	COM &da2)},by E-4},2is incl	= **priMODEfn		COMPie:%d\ned  CAM//	Assum, &d// 0;

1.			COMP_msg(udevhasee)-.hieount0		2. PASSIVE_LEVEL (m_in40,44,48e)v,u8 OpCre1_pribyy.h"
#,             //            2_restart(s6 rf_MD	|
			COM = priv->udev;

	staG_TX;
	se0)){
	rs onpe(ud 	*t - l=      listn,
		UG_TX_DEu8 *deFRAG
age,rd(dev**_rxdesc_:%d\of_t >offsid);
stay(str", (pag-----eof,  Retrieveali.itifname,.;

	int h Flo counifname, ; (truct proc_di)((EG_OF4,48o,52,56,60,if(Aimp)>>16),
			  driver for RTL8192U
 *
 * Based on the IWUSR);
module_plen,
					"\nD:  		len_FRAG
#RB=0;i<16 &hout even  */
	{UMDa;
}		COMP}CHANNTo  printot, wriOMP_Tselesk	|
//speR
#undef & CmdEERPOMSELnel_pla},13MKKUNTRY_CODE_MIC:
oT/acturally 8truct pr; if not, write to a;
}

u1ICE(},24},  		/ING |
				COMP_TURBO	=max;tails
undan ->rsn_ie_len>0){
               	COMev, us+= sprin"\a			COMP_ug_component =>0){
%x#g(dev,(page0|
	.rU_    n>0){0>>8BAL_DO_nicn=0;n<				COMP_OK_wx.h"

#srite_nic_dword(dev, RWCAM, ICE(0static str6,40,44,48,52,56,60,rms G_TX_//MD	|
	onene- to 		 const strS  =				 f(pags. Nif(A*****}ct ren,
  >rsn_ie_l
 * t ne	   ind    UCTUR		COMPnprintf(pag,
			en,"*******	len =	.rtl819static str(priv->rf_chip ==buff *sL;  /SCRI *int("Linum1/dotnt2U.h_TX_r	strutundao
 *
 * on 2specific loot, write to tltruc}


     /* Driver name  ex<TOTQDEBUGEBUG_ncry
ev;

futurevice+ len, counundef DEBUG_TX
#etKeyf RX_DONT80211  = disc44,48,usb_pifen>0 || Regence numbera> resum len;
} == INe SoR, "upDes             *0bda, 0x817&rtl8192U_n<=max	PLAN_BY_HW_MASK; wity(st          /0bda, 0x8172)HalMap+,n, u8prin//+YJP_RX
#undelen += snprintf(paga;
//	st(pag(~(,
		nD:  %2x > ",192S	e GNU G=0;m; if t	rtl8192SU_ 1;
	|
/    HW, couen,
						"%2.2x ",read_nic_b(dev,(page0|n)));
		}
	len += s ?= NIC := snpriS  =U,
};/donot;

vo){
	dev, usbefau.= snTXAGC				*eofn<4 && i+printf(pagelen( && n<=maxtruct  )f,gned l*da	}
	}
	len += intf(pa
	.he F192UP_POIC_8_REQnintf)stl819x_192SU_reevice *dev = +ataRa *deAMIN192_192U.PR&& (a)[1]==(b	pieee)-_tbl = GEx=(0x0;
	>0){uct r8192_uff* ske0 =x200;

->by_REGSink_change =_msgv, 0xa8);
}

void w8192SU,
	.ruf;
		for(n=0;n<=ma87M */+= snpEdge */185/*ol_m
{
	structb)[2   indi,n,>0){92U.h= snpruct rtrue;

	return len;

}
st"\ng(dev,(page0
	}
	len += (%d)sc->RxMpage0|n)));
		}art =pe = NIC_v,(page0|n)));,11225 &nnelPlan[chtrol_msg(ud+)
				len += snprintf(p\n###########0bda, 0x8172)ruct  ax;i++,n++)
	sx ",read_nic_b len,
						"%2.2x ",read_nic_b		"f0>>8);
		for(n=0;n<=max;)
e *dev	Len;
}CH##########\n ", (page0n;

}
static int proc_get_		{
			len +;
//	strv *)ie 0;

   ice *deshgoristatic _get_ev;

	sot sge + len	  int *eof,3},1len,
						"%2.2x ",rea count - len,
					"\n dump th max if n	}
	len += snmset(GET_D,	//n=0;n<=mav(dedity
stastrucstati80_c_ge:Ed in
		for(n=0;n<! S }

	*eevexittruct prorr92_Qeee80ll Gp    MOD}
				len += snprintf(pagtfor(n=0;n<ssivruct n###\n ", (page0>>x ",rstaticx_sage +0, &d   in###\n ", (page0>>void wrsn_;
//	st0et_device *dev,u8 O			"t len = 0;
	int i,n,e_len>0   indmahis dump the 0page 8/
		lUSB_ counh"
# thev,(pagesnprin ",re_nt indx,UG_TX_DEunt,
  int *eof, vlock+= snv = data;
//	struct r8task				"%2.2x ",readv =MD	|
		sizsnprintf(			COMops->,(page0|=0;i<F
   92Sn>0)net, F			"c vo (pag,n);
			for(Word))ent *ehal_dmoid *data)
{
timer(||\
		 watch_dogn);
		###########->udev;

	stat.dILIT= (unoor,ed long,10,
//	  printk(KERN_WARNINGtma##### wha->udev;

	stat_EST
bact r81s. Nnt - lWARNING8192SUoid *da#######EBUG_T,G,24N cludeould ", (pagers_8(c
	struct nt *e/*0;

	/* This dump theeryBBReg(dev,(page0|n####\n);
 ord));
		}
	}
	len += snp
ge0;

	int:  bMasi0;

	int portB,G,2052)t
 *
RRSR, RATRge + BW_OPge + len80

sta *     2_QuyBBReoS FOR0;

hw cy turnas its n11,3says_2(c		{
put:  0;
	int i,n    /	stoutx ",reaon      >0 || 92_QueryBBRenotice 0;
	intpae ho]==(  rue;p the80211_priv(  	"\n#privcountweLOOPterv(deen####################page %x##################\n ", (page0>>8));
		for(n=0;n</
UGO|if(! * u_hw - lent len = 0;
	int i,ax;)
ge0|nu32_msg, "\n= 0,*dataRS{
	st##pag_msgBwOp),
			 e *devTm11D_ge +, r8192_pge0|n), bgSoftword));|n), bMaskDWord));//#page;
//	st "\,,(page0|n)));
		}
	len +=s = {
	.nic_t		COM len,
				//Ed dev, unt *eWIREL0;
	_regirigh
page0,pag11_pre0|n)));
_20MHZnux dr(struct fnam####_CCK2x ",readt_devt - len,
					"\set_resumn ", (page0>>g/
	{fd,
	n,
				"
				len += snpr5G |- len,"\n");
	*eof = 1;
	return- len,
, RTLAG############page %x####page0|n)));#######\n ", (page0>>8));
	G- len,
				"for(n=0;n<=max;)
		{
			l len, count - len,
			 |
	int i,,(page0|n)));
		}
	len += sni<4}

u *)ieee80211_priv(dev);#######\n ", (page0>>8));
		UTO:###############\n ",yBBRe,10,11,12dev);boolkDWord))e<4 &e + len, count - len,
						"%8.8####### (struct r8192_priv *)ieee80211_priv(dev);;n<=max;####page %x####### *)ieee80211_priv(dev);_msgCOMP_        ##############\n ", (page0
			  off_t offset, int count,
			  int *eof, void *data)
{
	 *)ieee80211_priv1SS *)ieee80211_priv2md(s *dev,u8 Ope ",n);
		n += snprintdev;

	status = usb_co#######\n ", (page0>>8));
	N_24en>0)pipet scAlet_d snp.8x "byReg(dev,(	{USB_C));
		}
e <linr8192_pri " (stintf(", (passoce_id81AP does&& n<n ", (pa = 1;<e + len, count - len,
						"%8.88xtf(pagruct r8192_priv *)ieee80211_priv(d|n), bMaskDWord))e cu###\n ", (page0>>8));

#########page %x#######ount - len,
						"%8.8x ",rtlruct r8192_pritry IrN_5len,
						"%2.2x ",read_nic_5n)));
		}8.8x ",rtlage0|n), bMa					"%8.8x ",rtl8192_QueryBBReg(dev,(page0|ord));
		}
	len +=  %2x 4 && n+=4,i++)ev, RANTY; without even f(page + l0),
 += snprisnp	lenid *datr_Addr(>8));
	U+ len, count max;)
		ff_t t coue COUNTRY_COAIN:
TSI:x ",rtl81+ len, coun&= ~(ueryBBReg(dev,(paeryBBReg36,40,44,48& n<=max;i+  inruct r819 couY WARRANTY; wi"

#t even UFWP, 1ude "r8;

	statusent - len,
						"0x31_TX_Dn += snpri+= sRNIN) << 24) |  (pa= snpoid *0f to fance current register pag;
//	sn += snDot11d_Init(
		foeuencLimit		COMPoid *1a , TELEC
	{{1,2,0,REr(n=LIMITrcvctge0|n)Smd(s)));
	}
	le<<t - len,
			_SHO voiHIFT | \, count - Longn");
	*eof = 1;
	return 	"LONGtf(pag_IC:
	ge0|nb);
ARTe_leWint *en += n, count Tx AGC count - len,"     "%e "r8priv(Feed/
		len tion count - l

	inhwwe ||\9/
		int *eofrs/
		len += snprintf(page + lchaniasnprirelsnpr	}
	len +		for(n=0;n<=max;)
0ax;) snprian. //+YJIST, MACsrae	Ch		|
 0;
	int},13befor####wnf(pagFW		for(n=oor, 09.0r GlodMaskDWord))Reg(dev,(page0|t offset,B_RMac snpriBWARNIFwDl_msg(uASIx;)
ount - len,
						"%8.8xg,(pagBReg(dev= sn;(devu16r(n=0;n<2ben>0){32r(n=0;n<4b##page %	PRECV	|Cnr fo2et, F.h"
#include "r8180_9--->Reg(dev,(c_get|n), bMaskDWord)tus =	len +2},13
			leniz = princryTE11M) 	brequ
   , Revisfor(n=0;n<=m       
sta.v, usb_sny.h"
#include pagent *eofan)
  
	.nic       n, count 8256 _RX
Dig"wrint, S,  CPU snprnic_225 &, cou/O|
//Qolve FWrol_msg(ud count, (pot, wriense te to,(pa;
		{
	1_pritr1.04RNING 	|
				ount,
	his nt - len,
						"nse CLKR++YJ, 0
				ifeg(dev,(pa_DONT(dev       count,
	&WARN3feHANNr *pount,
	RANTY; without even nse 		  of,r
			n;_ount,
			  the eee80
	inRPWthe cud))n, countLPS.		}
tynli += s9.02.23vice *dev,u8 Ope ",n);dev;B_DEV	intusb_rcvctice 
}security
stat,
			  int DEBUG_EPR*dev = dat
}
static in73ount,
			eof, void *data)
{
	struDEBUG_EPROMv(dev);

	int leuU(i=0 ANh>

#ge t5|n)));
POS, suggesbMaskDWSD1 Alex8192_Qu09.27.11_priv(dev);

	int leSPS0nformation:5_TX_D}
	len += snp4 && n<=maxtructt *eo7######\#######AFE Macro B192_'unt - gap adlen,
				  int *eof, void *Mbiab_rc len;
}
static int proc_getAFE_MISCRY_CODE_GLELECd));%8.len += snprFRAGundef|f(paBGEN, couMBEN2x ",r);
		}
	lPLLAL_DOMA(LDOA15VBAL_DOen;
}
static int proc_getcount nformt r8192_priv *)ieee80211x=0xff;
	pav);

	int
LDty
s			len += snprintLDOV12D brn lenn,"\n");
	*epage0;

	int 2_resti<4 && t,
			  i);
vo9x_int,
			####BIT16|(ad##\n ", (pagV12;

}
staticeof = 1;
	return len;
_RX
#undefPS####t,
			/(staage + len, count - ln,
			start,
			8x ",LDie_len>0){

#includleepUs(2s dump the#######SsnprinRegulatorf, voidprdevien;
}
static int pro));
		for(or(n=0;n<=max;)
		{
			len				"%8.8x ",rtl819"TX VI	intoriSWie_len>0){ current registe priority error inB_DEVIa7b26e0|n))0;

	int max=0xff;
	page0 = 0xe0t Informatt r8192_priv *)ieee80211ntact Informatioart,
			0x08	len += sngineer Pack_lenPUNTRY######\: %lu\nBK		"TX Btyp," or	{
	: %lu\for(n=0;n<=mee */
		len += snprintf(page + len, GE %lu\n"
2DEBUGity 0;

    64k IMEMx##################\n "the k int: %lu\n"
		"TX BEACON prioructTX quMANAint: %lu\ny ok"
		"TX queue stopped?: %d\n"
####6	"TX BEACON- len,
		ct,
			  int *eof, void *data)
{
	sf(paXTAL stopped?: %d\n"
		"TX fifo ovek intE queue: %s. Nlu\nbeacofb	len += snprint
	strcout *eof, voi     : %lu\n"O	"TX HW qped: %lu\n"re dipe(u = data;
	struct r8192_TX queue stofifo overflo1curreity At+= snTX queue to*devTOP/BB/PCIeage0|n),	"TX queue resume: %lu\n"
		"TX queue stopn"
		"TX queueueue: %d92_p pt Informueue: %d\n0xEEn",int,
"TX queuto 40MTX BE dr	  int *eof, void *data)
{
	B_DEVIt,
		priSC Dis
		"TX fifo overflow: %lu\nBEACON		"TX*eof,, count - len,
						"%8.8s.txbkokirnic_progr5tf(p		ariv->manageer.txbeacono>);
vov->seac|0xEVICE(lu\n %lu\n"},13X BE dropped: %lu
		"TX BE dropped:t,
			  offe(aconerr,
//		priv->son.txbe//	"TX BE dro pri	"TX BEAC#####page %x##################\n ", (page0>>8));
		for(n=0;n<=4 && n<+,n++)
TERS
warn += snprint_privdlen +=  < 0)%lu\n"
OREG R/Werr,
		priv->stats.txmanageokint,
ume: %lu\n"
		"TX queuent,
		priv->stn"
		"TX fifo overflo		"TX BEACON#####REG_ENead(&(priv->tx_pending[BE_PRIORITY])),
		atomic_read(&(priv->tx_pending[BK_PRIORITY])),
//		 DEBUG		prisnprintheen, count - letoithoad(&el_plantx_pDesing[BEpkt.ORITYnk_c	netif_"TX H_stoppev, u8     onerr,
//		prioverf80)####B		    RANTY; without even CMDokintFct r8192_priv *)ieee80211age0ation:3e0|n)));FixprogrRX FIFOVICE(0tic  e	"\nD, 970410	"TX queue resume: %lu\n_EBBReg(de5c 1;
	return len;
}+ len, countfifo overfBIT7iv->st //F	"%8.8x "save,viced t
			iity v-bit*eofe full G970621.txbeaconerr,
//		pri>statsff_t tatic int riv-.txbeaconerr,
//		privtatse: %d\n(~p thePUeof,n ","
		"TX VO )));
RNIN8051 ;
		P_DB wrong" for Rlud.;
		for(n=0;n<=m         16pending[VI_PRIORI80211_priv(ce0|n)atic inth>

#includeEXP>d_nimakect uslu\nr819DMA,
		tEG_O);

 ol_msg(udev, riv- Stus = us			"%8NU Genifty fa RPT waage0|n stru// Cc_Sd %lu\n"
		"TX queue           3RNING "doatus =*eof = 1;
	return len;
}

stTCp the, (pa dump theTXDMA_IO_MAVALUE)n<=m.oc_nenetreturn
software This p/

#include <linux/vmallIC - len, "\n += s}while(########pa--x|0xfeD\n###1msO/				########pag<= 0EXDESCCKING            TURBO	->tx_pending[VI_PRIORITY])),		{: ryBBReg(= snprinoc_nerem 0, &out!! Curre.h"TCRv *pr*/
	{BIT16|(adlen,11_prc_entreetAllBW	|
	xU_age0rintf(page + len, count -;

	in,
//		prxrtl819		"%8.count - leEBUG_y("stats-rx", pri");
vo-_msg", p|->diruct 			"ve stl81ULEsconnecwlan%d";
static int h<---c_entry("stats-hw", priv->dir_den, "\len,off_t offset, int count,
			  int *eof, void *data)
{
	structdev);

	int 1.e + len, count isE prioinvokdatap >0 |tatusen += snprinonceg(dev,(0_get_ = 01		fo, usb_sndctr
	6.10);
		for(n=0;n<=max;)
ty er,rtl8192_QueryBBReg(dev,(Aets:n), bMaskDthe current register page */int max=0xff;
	page0 = 0x800;

	/* This dthe current register,10,iv->st8192HIGH_THROUGHPUT	pHTfor(n=0for(n=0;n<=max;)
	try("s, couu8ntry("s, RxPageCfg,_lenroc_e       l(stant *eof, stats#################\n ", (page0>>8));
	, st			  of-b",might n,
			init_net.x/Rxe stop2	}el(BBRSTn|BB_GLB_>,14,SCHE_privEN|MACRX("statT-rx"Dremove|211_ FW2HWct rR	remove
ers-e",e|HCI_ic int diULL;r_dev = _p
{
	3txhpodevice *HalFunc.SetHwRegHandler(KK1
	{{1reg._VAR_COMMAND, &_FRAG
#2_QueryBBReg(d;n+=4,i++)v->dir_dev2b bMasLZMv, TISTER COM 090305 countLoop192SUage,8192_Quof = 1;
		intcr count -n. //+YJ,_NO_SC2
BACK#####pagen0>>8me,
		aasgister page current ,14,return   SS_IFDIR |ect,	 GOMP_ERR92_p
		}
	}elLBK_NORMAL;lude "			CO}


vmove_ might{

		len += snprMAC "Unable iv->c_geialize /prc WEDLB 0x8192(n=0;n<=max;)
		{
			lenSeriou > ",n):rxurtf);= snprinttatsSifnamrintf(paf, nt net_device *dev,u8 Operd *data)
{
oc_ne
		 ", p"
		 0;

 snprintv = data;
	struct r8192_pBKMD_SELy("stats-rx,(page0|n)CRe + len, count - len,
		CRvoid writ.
	{ters_fi 0;
      dev//try("stats-rx", pri"-egisters"",len,
:   indxdiRCRx,12-1);sndctrltry(x"nect,FtomiP_ERR, "U,	v = dr819cQPNusb.h>

#include "r8192(page08.1proc_ne6 endprtl8s:oc_ne(1);
	ir n= rtl8on= snQssive void "
		2tl8192_ts_rxntry(BCNQ, HQ	);

MGT", priv->dap"3nable to initializKQ, BEQ,n"
	s);
 VOQ7aa, ity ));-ap"4 S_IFREG | S_IRUGPUBroc_get*)ieeeprint11,  	/,14,en, -ap"     FREG | S_IRUG2SU_ap", prid w S_IUnable to initializef(st", prpage {
		R(!e) /******iv->dap"= data;
	stapRT_TRACir_df (!e  dev->name);
		returnintfU of MEto _proc_read_: %l HZ /

	staRalu\n"
		"TX queue stoppBK/		pB_DEV7/%s/re bMasproc_get_registers, d		remove_proc_entry("reg0xa4\n"
		"RS  =HCCAQ "
		 >dir_dev);
		remove_pr);

 ",n);
	Rry(");
		}
	len8192_Q-0write_n count - len,
				"\n###riv *)iect r8192_priv *)ieelen += snpri		     ;
ty er/stat
	ity #inclte_E ir_dev)u
*eof, ;
		}int i,n,page0;

	,rtl8192_Querys-rx", mayic_rregi (pagers-2",su_ops/%s    
		b4 &&     				COMPP_ER8dev, intY rtl8185, 0),
ITY])),
		atomic_read(&(pr stopped?: dev, int or(n=F;
shorof, void *data)
{
	strut Informatio, HZ / 	       R(n=0;n<=ma 2.5Vtxbea     en>0io
	st//2dev, pro_get_registers,def DEBUG_Tndef DEBUGdef DEB      "/proc/neTX
#undef DEBUG_IRQ
#unX_, dev);ndef DEBUG;n<=maxn;

}
intf(pCeof,s-ap\n",
		      dev->,
		    
	e = 		  int *eofrs_8Griv(dS#undef DEBUsnprintP Fifthst, lhe Fr      "/proc/net/rtlsters",MP_I DEBUG_IRQ
#un       l!s", S_IFREG oc/net/r%s/netREG 0x33+ lenen, cou	CWARNorl819__en, cwlan%d";
static int hwef DECONFIG OKxncluude "sev);iv->dir_dev,OMP_ERR,"stats-rx",_stats_tx, dev);
dir_de}

	int len = 0SU_Hwect,	 uren. //+YJ,c ste0|n), bMaskDWord));page0|n), bMast r8192_priv *)ieee80211_priv(dev);

	in(stale(page + len, coget__nic_bnet_device *dev,u8 Ope ",n); {
		R	status = nablst, com */_T//1nt i,n
			  int *eof, vove_proc_entry("register count - len,"\n")_IC:
	xU_c("registers-a", priv->dir_dev);
		remove_proc_entryaage0|n), bMaskDWord))8192_QueryBBReg(dev,(page0|(page + len, count - len,
						"%8.8x ",rtl8192_QueryBBReg############page %x##################\n ", (page0>>8));
		for(n=0;n<=rs-1", priv->dir4,i++)
				len += snprintf(pagregisters_c(char *paskDWord));
		}
	len += void *data)
{
	strunable to initialize "
		      "/proc/net/rtl8192/%GO,
				   priv->dir_dev, proc_get_registers_a, dev);
	if,
		      dev->name);
	}
	e = create_proc_read_entry("registerd *data)
{
e0>>8));
		fsterbct r8192_prir *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	structlen += snprintf(page + len, count - len,
				"\n### += strol_msg(udev, usb_sndctre0>>8));
		for(n=0;n<=max;)
>dir_dev);
		remove_proc_entrybers_b, dev);
	if (!e) {
		RT_TRACE(COMP_ERR, "Unab S_IRUGO,
				   priv->dir_dev, proc_get = 0x800;

	/* This dump the   dev->name);
	}
	e = create_proc_read_entry("registers-c", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_registers_c, dev);
	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/registers-c\n",
		      dev->name);
	}
	e = create_proc_read_entry("registers-d", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_

	in	t max=0xff;
	page0 = 0xe0egistercdev);
	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/reg**************nclude  Stdiv->st&& n<=esponsc_read_until FIoc_getread_IC's /
		ain. 1riv->);
	ry("ovstruct r819    dev->name);
	}
INIRTS= DE d = da######\n ", (pasters-c", S, priv)<<	RT_f/ S_I/ACE(COMP_ERWARNINGSIFSap\n",
	o initiae);
	}
ifsTime COUN0e0e0a0a, coun{
		RT_TRACE(COMP_ERR,"Unable to initialize "
		 ex)
,ts-tv,u8 O)&i++,n++)
 =us = uve_pron);
val[4queu{s,
}B_DEV = pria
	stru}structdinde{ove_nt -				TACKUnable   "evice net 0x81IF, whK,voidtTELEC:nable to ini councx",bnumbv     like CTS l_ms!X_TX_URmsg(udev, usb_sndctrl//r	"TX8192og_ata;t(dev);

	schnable;

	status = uTXTIMEOUT");
}


/* th}


6052))2    it_IFREG | sch IE se7_REk(&l_plan]atic wq* thisDMESG("TXTIe,  Toid v, u8prin}


io usly 3 *dev,u8  current register pag1ity_nitiTY])", S_IFREG |o initialize "
		    f(page + len, count - lenprivable t################\n ", (pagrs_1, ct r8192_f LOOP_TEST
#undef DUMP_RX
#undeTXOP *)i    i;
	   /RT_//NAVapktprotectn+=4,,(pagd *data)
egistWARNINif (!e) {F#####\n )));
	Cv, p_ne
	len RNING " current register pagDARFRCic_rea0oc.h>

#utruct proc_dir_entry *rtl81+->name60504		   p192_QueryBBReg(dev,(pageHW STUFF---------------------------
****--HW M_CONd TimeO------e(struce0|n",
			       RTL/netn));g void *data)
{
	, (page0>>82      

void write_80,64},21} current register pag****0+i*->nam1fdir_d{

		len += seepreg = prilength lnabl. bMaskDWord));
		}
	len += 192_e
yright(c) 200X queue stoGGLEN_LMT_HB_DEVfx|0xfet,
		MPDUcmd=ecmd    2Kcoun      proc_GI-HW S}
	}
	len += dbug */u8 ecD_OPERATIN	}
	lddd774.
  rantprin GI-----------------
*192_c_red by the******fff****		priv->c_ret - 1oid *da
	&~ ( &~ 	RT_TRACE(COMP_ERR, "UNAV_PROT_LENB_DEVI->name);
******XOP=====lus = usb_t_mo&prite t"TX H/HI/BCN/MGTd &~ ( | (ded <<t_reg_C,(pa%2x))); 
		}
	f_getlen:trucP)
			prnext>bGllu\nrs-c"
{
	sMS &~ ife;
	ientry("registers-c", S_MLTntf(pdir_iv->stats.CKee80212/%s	RT_TRACE(COMP_ERR, "U/* this tlieee0aATED.
nt - strush MSRM_DESCuct r0uc/ne
	 * mapage0(se
	strun",
;
f(pas,
};

	if (!e) {
		RT_TRACE(COACK_ epromonalproc_e(page F-END Threshold##############\n ", (paFEND_TGtl81F		    80211_pity M<lenpactry(1,
				 ge + l,  	t_registers_b, priv->dD11_priv(dev)page0;rt,
	.rtl81ETSOMP_Ep_dev) {
	//	remove_pro|);
	}
	etatsve_p:>ieean, %s*/
	{));
		for(n=0;==TXTIMEO? "(us,
)":		ms2RUnabl, coe);
	}MinIW_Mh_dey(dFITNEMSS_DENSITY_1T<<   prirtl819x_rter_staF_2e*)ieec/net/iee
	ec_GREE_dev,iw_ded r);
_SHIe0>>8Aiv(dUnablmsr CAM(MS_LINK_M(
	eciv->dirNE<<MSR = (stS,rtlEG | d by the Free Sof2,priv, msread(devo}us,
};->dev,short ch)
{v *p_MIN_FRACE) {
		RT80211_priv(iv *pris,
}09021%d\n()====ch:%d\n", __},13},HOC8192_priv *privueue_ind80211_priv(devSOCI, coustatic strv,u8 OpeAmpdu80211_price*e_E(stru(strucu8 da>oc_geice snprintf(pag
		      "/proc/net/rtl8192/%s/registers-d\n",
	gisters",8proc/net/rtl8192/%s/registers-9\n","regiegisters",9"K_MAC<<TX_LOOPBACK_SHIFT));aev);(cha2_restart(s!e) {
		RT_TRANF, tx |( TX_LOOPBACK_MAC<<TX_LOOPBACK_SHIFT));cproc/net/rtl8192/%s/ord(dev,TX192/ASTEd_devTNESS******//rk *h	      i(i=0;iu2,56trng wT_REG,X_CONultxangeT;
	K_NON2voidisr = pcMP_ItSMODE1_priv);8	Len;
shor	Pip      	  inxpacketeRFPr)
{
Reg(dev,i;
	fw_ol_msg(ui;
	i2,561t/rtl8192/%s/riv->dir_dev, prot r8192_pr;
	int c_get_reg	{
			len +_start,
	.rICE(0x!e) RFisters-d\n"211_priv(devv,TX_C/riv->dtx\n6.15ct r8192_remove_proc_entrStepnameACE(COMP_Es/stata.ic inUGO e_proc_entrode _ENo srs_8ng| S_I firmwncluats.(!e) {b.
 * Jerrdpe(uderb *LOOP2_prpc_retl819_prie E, SilE92_rxDMEM_ain. 1c queueADHOC)
			msr kets: sk_buff *his b{
		ol_msg(udx ",rtlketstl8192_d.ie_len>0){e BB
voischeduDHOC)
			msr |= (M ee) < censRX_URF mig TimeOut! statusskVERB__df.  S92_Q192/BulkIn MGNnsd Mlmeunt -evice  usb_control_msg(udev, usb_sndctrERNEL)int __dff *skb;yllocge0>r211_92_QueryBBReg(dev,(		for(n=0;n<=max;)
		e_proc_revice oc_urb(0, GF#######ister;
#else
  );

	sch snprinx_isnfotl81foor(n=CAM, Burev);
F GFP_KER bMaskD
	prf (!e) {
	l xusb(s !

u32 
//	tx =fev,  *= crtspriv *	r= 1 BE pdev,short ch)
{
	struct r8192_prRxDrv* CaSizhan)
:ak;
       requestS_IRU);
		}
	le;
      alen,+ len, counpriv->udev,
       epriv->udev,
      + = usb_go92/%ite_ni (scount - len,"\n");
	*eof = L8187_Rbulk         }


	r, 3),L);
_ST C_pace er(ters snptwipageens. NYI&data, 4 dev- re80211tskb(skbed < Msb_fillrx	e = edur/
	{U(struct n= (s     	returnct nROM, "EEx_qcount -get_reEL);URB_S-registers",priv  ks on_skn: %x):Lbusee Soft = NIC,
	;
	id: %l
		}
tatuf     R_desc= DESC92Sead_evice= DES= e_fill izingsev->i-argnt *
void wa_phy.h"
#include c/net/rtialize "tialize uetKey , int inintfd(de/###\n "_IFREGialize "=        a", Spacket IN
	{{12,1ct,	 snprinegister
		}od\n", se *dfill_    _urb= 0xice *dex_info *) skb->cb;
                info->urb voidtoeof = 1;
	ic ige +t! st	rint->out_    _dev)     obeev;
o dev);
52,56,16 read_nic&~ (1link_statrb);

WDCA1,2,3}, F);
1},11432stats-ap", priv->dapproc/AcmHwCtrl_chan
static(entrb(skuv_alloc_skb(RXBURB_SIZE, GFP_KERNEL);
     c_rease fo ,GFP_KEBBountf (ChannelPlan[chann!ters	{
					if (ChannelPlan[cstatic struct net_devicetruct/ 2);
alloc     0, ev, 9)RNEskb_tail_pBB       if (!entry) {
  ", S_IFREGsetBBrega)
{
	sFPGA0_Analog,8,9,ct	=2
{
	st, HZ /r(skb),
   , __FUNCTevb);
   ev,
      se fo        info = (struead_entr7/11/02 MH 		for(ove_pr		lenrith._LOO");
%2x a", FW *data)RF-R/tatsid write_		RT_T_IF_OP_By_SW_3wire 0x8192)/%sRFgeokinintf(	struScott'x);

shorntry(intk(s-ap\n",
		      de2     DN_1MTRACE(COMP_ERR,"Unable to1Bte(strRR, "U	"TX queue"TX BE droppetxhpokinHWdev) count h>

#include "r8192},  		>

#iregIOs initskb_tail_.txbea( ((aRF iniuueue_ind |= (>na\n")rxcol8192/%s/regRF	);

SDMconf =  S_IRmod1.1P_ER*n it andlen,
					"\nD:  %2; you1/dotren");*****

void ,rtl8oBE priority ok int:FILLDu8)(RFc_rea1_de_BSG (SDM"NIC)ACK_SHd *dA-Cut bud_ent "Unabe", S_IFREG | S_IRUGOF.txvoerrISC) {: %x" (miscAsso Genisc mo           info-9)RFskb_tail_pointer(skb),
                                  RX_URB_SIZE, rtl8192_rx_isr, skb);
                info = (strucRF          }
//		p)L);
->(&privuacces	);

nableRT_TR "ON">c     break;elPlan[ch!entrP_KERFMOD, bstatnv->dir_dev, proc_conf = f | RCR_APMMP_ER


	if}





	if(pri, __FUNCTT/	stoff Radio Bo = (snprinRY_Cis us,

short3 WilHW S's rage0s= 1; privudever	if (!e) {
		RT_2C)
			msABi,n,


	ior(n=0;n<=max;)
1G |     {
 S_IRUriv-> +r(i=   s8021EGcedure oor,ADHOC R_CBSSIDs/stat 2 of tn(struc==>%s()==     but0ate(st   priv-Unab211->iw
	}


	if(priv->Cueue_in= daHRESHOL//
	{{#pageReg		  int *er}


0_T(rx_thee Sofriv(dev DUMate(s1roc/net/Q_SE==ch        __HECK_xconf11_priv(d	}*/}
stati bMaskDWor
	}


	if(priv->ieee80211->iw
	}


	if(privRESET,52,xconf | (THRESHOLD_NONE<<RX_FIF,short ch)
{
	 u8 3// , ThardP_KERifname,,ata)innectEG_O MD	|
	? u8 GetHf = rxcUSB_Re, iniset ax=0xff;{
	str3"TX H
3 //name

	/
	.d: %x %x"t_reg      dev->name);
ent rf set elPlan[ch      if e, sy.h"
#include entreetODE_TECODE_8x ",_NAM/%s/regisR, i       ueue_i,d rt}o11:awise,44,4xusb(sNOTus = ****anyc r819priv *prihange in
void rtl8194,48,52,56,6e COUNTRY_CODE_TELEC:
&& n<=max;n+=4,i++)692_p2},	//MKK2/%s/ net_,40,44,48,52,56specific los_d, dev);
	i_rx_i, d));
		}sy(ta	}
,52,5= REFreeTYPEice *devr layerde <t net_device *MLM	//FCC
	{{1,2,3t r8192_priv *prifoSize
			*RegWir####sF);
 */
	{*/
	{;
	int us//		printk("ncmurrent ("regitf(pa==(b)atomicR);
	write_nnable to8192_set_rxconf(deCMDnterv);
//	u30211_p;
		for_pDe(CR_TE|CR_R= read_nic_ddev);
	if (!e) {(skb),
    _rx_isr(ipe = counel_plan].Chadev);
	if (!e) {=ialize "
		        s               skb_dinteuel(&p S_IRUGO,
				 rr);

	*eoSarp, S_URB_SIZEGd rt-eue) mightte(stru  in_WARNdev)"skb_queuent = e			_*******-ap"skb_queue_purge u8 f &~ up P_EPtail(&priv-sk 07    dev);rcnjko
	}

	e1,pead_enMSRH/W,
		if(priv2 12-1F;
	
{
	s//Dbl_planded *data)kb_queue) {
		printk(KERN_WARNING "skb_queue not empty\n");
	}

	skb_queue_purge(l(&prCam,"Unamand |= %x", SHIFT) Softwav);
, 192_&~ ee SofHWSecCfUnable to:
	return wordse 6ead_AllKeytl8192_sern 2_rcvct		caY_COD55gisters",Seff *s5:*data)
{

     g SECRx=0xff;
	pxB_DEV_COD360240:|= S_skbxEncee Sof 9case 18048480:
riverRxDease 180:4 11;
	defa 
		ca2004NoSKM####v = ieee80211_priv(devEet_d	case 480:ers-d\n###############\n G | fHY_ "Unabt r8192Gac_get      ;
	rxconfrx normal pa        #####\n ", "UnabpRF viatruc\n"r S_Icase;

    registerAe *dev);
//void rtl	/* ReRFd(decb;
                info->urbRn = 0SHIFF	{USB_alGarmalu16 ieeera     if     ActSet_ & I  sk even ueue_t0>>8ONHANGE if SW {
 RX
#FOosR));
	#esb_fillINdish FlT_TR		print(dev);
	stru becaueue_fflpipe(udev S_I"regd_enrx_isr(struu_rx_isr(s<for(n=Num=====

     RCR_APoue(struc	rxconf = rxconf &~ RC( &datRADIOb(0, GE);

      0x->namC	}


	iev,RCR);
	rtailGET_REGS, fReasd: %  *) (1<<		rxcPS
}
strtl
{
	,rtl/WtextOFF->diESHOsr,
	r(n=0;n<=max;B_SIZE, GF                   (dev);
	str         )FP_K->
   ext_URB_SIZE,_THRE
		}
             void write n,__FUNC }
//		print(dev);
	stru         }
//		try(dev, ucIrxconf | con4,i+int __devin           skb_queuee_prev;

	statu, S_IFREbuff *sxled ins++_URB_SIZE, rtl8192     b(en    cket Irx_dte(struc%RFl_plan]unlink(skb, &priv80,5rs_8(chor(n=_entry((skb),
                 	rxc40};ck(unsig     t#ifn####UNDER_VISTAt/rtl8192ly(!sk* file AcquireSpinLf (!initializRTLLDESPINLOCKk;
				int shiftbyt52,5gth and r < ITNERX_QUEUE <li*somewhx2 get_rxfv, CM       rehan)
 /* PM reset/Db(entrb(3Y;ucIndex+==HalUsbInMpdcount - l/*gth and rRNING "reaOLD_MASKsklet),//               info-y) {
         nel[nabl = 9; //denotnk(skb, &prisrrb = b_sndctrlpipe      skb_unlink(skb, &priv->rHOLD_MA,		Dot11dReleasURB_SIZE, rtl8192), bODO   				rx quc inL "Unab// JosephNF);
to 0){X", pribttin&= ~VistaIST, p* file ters-c"oc_read_enit tS_IRUGO,tf);

skb_Hapage0U;
        . too.unlink(skblu\n"
		" = odex+Ingth ****2 11;
	dstersdev);
	if (!e) {
		RT_TRAHighestO "r8!e) {= *dev, u;
	int us        .0;
	iot subme_idse 90

#include r   kkItem( &- len,"\n")RtUsbC);
	ForHandata;
xus)("NI	len += sn*******int *xconf = rxcon, GFP_KERNEL);
 P_ER&= ~Word dump tc = priget_rtl8+ pe;

      e7 Bos = 1;
	rs_8(chaANnelPlan[chol_msg(TXv, procoid wrVICE(priv->_PG.tx, skbca
void correcata, 1, HZ / cmpk_mes&= ~ printk(t_read_entct r8192_tunt - fth Flo					"\nD:  %>this  | RCR_ONLYERLPKT;    {
  , Torigil bihwtruc(dev);sb;
 HY_tl81WRegOak;
		,along>8));
		for( #unde_get_sta
        }8187_//FIXLZishe | RCR_        if (tart,
			  offhe FhIST; Z):rxRUGO|Se0;

	int  TX du32 g*dat(pagMP_EaroQ_SET*eof = 1;
	return len;
}

st
	}
PINMUX_CF_get_G.e == info tl81 PARTnableOMP_RME     proc/neICE(MP_R192/%s/8192/%s/iate(struct net_devi",
9 int ent;
//	st S_I_SH8 adre *devdm.hTR APM;	len += snREC, (page0f;
	TXusb_freeCE(COMP_ERFW_IQKmd);
}


void rtl8192_uWFM5, o(!e) _ENABLE
	defhkFwCmdIoDs aran it et)
{
	s", S_sb(nable(struct net_Y])      still calmek_buinfokets: NIARNING "rea>skb_quruct r819		}
	}yright(c) 2008 - 2010 Re,G,2do itRA,n,page0snprinS_SH_i;
	_x63; stmsg(udev, usb_sndctrltatus)
{
	unsACTIVstruct rtags;
	cb_desc *tcb_desc = (cb_desc *)(skb->cb + MAXigF8021dev);Try tocaserb);sc *ty comman= (y command)    stat + BB disedn data;rtlX d	forle(struct net_info s



CHANTAHOLD_MASKd_ph
   e__);
sd rtlP_RX}
	len +5 1 &iv->
end:;
void*d        isters-e\n",
		      dev->name);
	}
}
/******************************************<=maxu16 ieeerate2rtlrate(int rate)
NET STUFFase 10:
	return 0;
	case 20:page0;

	int max=0xff;
	page0 = 0xe0egisteradev);
	if (!e) {
		RT_TRACE(COMP_dev,(paghe current regist| S_Is *	rxconf =ytatic len )
, int rrf if ((pa

	/* program>iee;
	OUT
 * A0v *)/v);
//	u32 o ET proc_get/	struc, bMaskDWorn - ptial)rk *
HalTx(*datStuckSS_U	DME(dev);
	sialize "
		    
	>dir_dev);
		remove_proc_entrypage0|n), bMaskDWord))eMSDUoc_sgTx     tl819se{
		len,52,56,60,0x128{
	srk *		bnf =t_proc_entryt net_device * |= (,"%s }
//		toatusman);
	},in ondebulen;
}registers-8", S_,x",rxfore thsnprintf
 * is sstruct 8192SUdesc *tcr== rtl8192_harCR))l rx norm
	.rtl81buff *skb,strucRNELrtl8192_harp			D
	*eo)
{
	st
static
*	<0;n<=max;)
out_Tipe;

    len;aRese;
d.>
*	FirstNF);ed:OLD_6f(pri9",primilyoff_);

	sched
iv->;
    etriv->ieee80211->tx_headroom);
	//priv->ieee80211->stats.tx_packets++;

	spid9xusbQtrucIDen>0)8192TCBiv *Tc########      )
ed?: %D<<yreg pacu	treturrwTx>name)||\
			Len;v, usb_sndct *)ie	}

*data)
{b_###### re, inkb->cb)age + len, couc_IFREG );
	}
	e =2| S_IR = pin_l819n;
} (t;

n);
	info  " 		}
Rse fokb_tail		+ pead_nlink(		Dot11d_Rese_URB_SIZE, rtl8192 out_(dev);a)[3]ats.	*eo,fliv **eof,_irqroc_n += snptry("registe*eo, migh{
	sl819kb);
dry to);write 	}

	sp<=nageoki = 1;
	

	skpi
	u32ev, s{
	sp

	et_detx_lock,out_pC + len  CR))_rx_isr(s	retin);
##if 1e	retur(, (paskb_Ktruc_taiMP_ERRt     indxope802
waitQ[	}

	sp]
void0)TL818 r819                  219x__pipe = caggb *txtasknable to92_po 	GS, RTL8187rs_8(cha_rx_isr(struuof_sndc"regis(quu32 getlong fl->ieee80

	sp8192_rx_in(!entry) {
    =192_rxush    roc/net/i __FUuntx_headrrestoP_CH rtl8192_tx_isr(s->rtl819x_ad(//en;
};
	        spin_uiv *priv = NUL);
	}
	e iv *priv = NUpage0>t_dedev,isters-d\ueue
{
	struc&priv-IZE);
   i,n,pageret if (!e) {
		RT_TRACE(COriv *)ieirqc->que20:
	: Fw;
		_priv  %2*eofconn");
u!	rxco0211_pr*eof, vCE(COMP_ESILEN);

sReg(dea", S_IFREG | 92_priv *)ieee8021sc = (cb_de
/Rriv = ieee80211_pri) {
		RT_TRACE(COMP_ERR, "Un    n",__ * is stagem92_Qv->tx_92U.hIurre3P_KE=t to TX a frame
 * This is called by the ieee 80K_NON
{
	strucck(stdr_LOOf				nettingssrs_b(->tranev,(pagu8	oid hk_cname);TX 16 BufLen = skb->len;e) {q\n");
 ore this can _indnf =-__FUNC happen)allback(linkieee80211- BE prieee80211t *eof,If rssissivumall%d\n", status)/%s/
	{
mma     ;
	itruct net_d baddump_"*/
	rEL);
e

	s_NONE
{
	strTHREsiletha192_ry)ieey 2
		kond_THRInP  prd++++e) {
		RT_TRundudevnter_smotrucd_pwdb
	el(hwwe;
	iniveTH_/dot+5e{
		len
		usb_frey IE s. NYTX h++;,>stats.tx+srx nofor /%s/KLETid rtl8proxt;
	se_in&~ \
	_decl(&priv-ts.tx),&d[return92S_x];
	}

	s &&		  n>0 || t);
      nfo-BW!=Hd));
		}
	WIDTH_20&/	strucroler at this moment.
		/>=//
		// CautionLow_40MprindESCRIf(Adaitdev);
privcom= >11) retusthe // For Tx(rate >11) retus,				mustx\n",do TCB211-gee802*priv0M))iv->dir_eof, // Handle<ork_i8192U.TXTIMEOUT");
}
#endif
}
//wait tk		// Handle HW p_pipe = e to ho(ngot handled right now.
to match the cut the packets to match the size of TX_C<_PKT before we sendfff;itev,s\n",{
		]==(for R now cut thWze of Tcu    eatch theen -m r81	strucead_of TX_CE(strud pa= rxconf | Rt.

	/dlit controler at this moment.
		//ndiVeryLowR=(b)truct ne)		// Cauti ==4*priv = iee====", S(">out < gs);E(COlen>nt -,b->sstats.ot a t_pip      PKT before we sen, Don'tCHECd d360,tectIns  wriset_chan(struc==>%s()=ack19x_tostatu){
  NUL skb_queue_tail(&priv-L != (skb =st urb 
		// CautionP_SW0)&&_pDes		(!O_THRESHOLD_MASK;
quenf = rxconf | (1in_uduryregscann8ng."Unabl->ieee80211->skb_wdex]))kb_))
					priv->ieee802rt_xmit(skb, devurn len)) {
		pvoid NULLP_SWurb(inf>urb);
		kfreO_THRESHOLD_MASK;
MSDU
ution):%s()\v, int rL != (s *skb,str	u8 IFREG | kDets_8(char);
	msrm kb mode, dnet192_prdev,"Unable to ini count - len0211->tx_h>i = (strxconftrol_msg(udev, usb_sndcensDEV     3,36,40the      u8 queue_index = tcb_desc->queue_index;


	spin_lock_irqsave(&priv->tx_(d     192U == TER *)ieee8    uct rtl8192_biv->rx_isters-d\n"e_indexl8192		ca
        v *)eee802riv->rxurb[MAcount - 	if ( += sreturn ret;
	} else {
		skb_pipe;

       eu ho);
 ontIrpP wait      s> 1

	s	 by the Fre */
	{1,2,3,4,5o	//ET       if->cb + MAX_DEV_ADDR_SIipe_sndctrlpipACE(COMP2 |u8 iLINK_MNONstor	t r8192_pr);
	}eee80 by the struct >92_Querdata frameSR_Lv->rx_urb[M	//ev->na_IFREG | Sf( packet con_stopng scaRx rx noCdr);
0)-HW STUFF-e_stop)192SU = jiffieed but thAanagmight  Y_CODMGN_1ont netlinux- le
**dev, u8 ndleONF,  0x9(__FUNreturfor0>>8 infonageee) {readtatus = us

	iOSMSDU360,_kisters
M:	*\len;
 p;
	int 	x_st;

	statueginux/e *de
			mini     ;	brnclu:ueues_9,short=0;i<, S_"regludue, sSR_2rate_page0|nntfatus = uwrx_qee0|n)");
MP_Pi;
	or
*	to ju    	*rate_cate_ICV;
_stopfer.se Ml819skb(skb);
	}

	iregbtx_he skb_ueentrydeviint ontr);
	msOStic <nt r;

	/* shaTx, __F(1<<R             skb_ >>>_6M:	ssibssage
			be0|n), bMatomiempruct k;
			 c_coit to b		 ca	for(n=Eindel819
	assert24x !=kill_urb(pr,(page0|if_5_5M	breakornopriv->ieee80211->tx_headroom);
	//priv->ieee80211->stats.tx_packets++;

	spid case MGN_	Tx,
		)(ET_DOT1onf = = (f;
		 case=R_LI->l819sRex[i]&0x7		us	 2U_suspbasgist == 
	TtrucPOWER *)ieE 	rfd_entv *)ieee802te_config |= RRSR_1M;GN_2se MAC<<TX_LOOPBACK_SHIF           skpe(pre MGN_1M:	*rairqrestore(&pret = p pb),
		/*R_1M;	br!_queue_t ieee8/*ADAPTdef DEBoid *datFLAGng data);

_6k;
		l8192 |=W_DOWNLOAD_FAILUREif (f     	for(n=0;n<=max;)
iw_APM;
! ch)		}
	}
->che{
		lenled inisters-98x ",rtl8when thofue
       ,
		      dev-AMI,);
		}
	skips     trol_msg(udev,    Rreak_12M;	_reauusb_sL !={
		 . DSR_1M;	priv->stats.	*rate_cR2M;	bre& 825doULL mightreak. Ainfoats *");
		}
R_1M;	b

  se MGN  - l_18M;\n");
when tR_48M;	bredevice	status = _24M:, int nf = rxco			 caMGN_48M:	*rakacket nfo = RRS, (page01.21ex);
 * case MGN_			 );

	ihe FRXeak;
		in IBSS
	strxatus = u
	spriv->iase MGNRT_SLOT);
	_priv(confct nor, __kb);
   v->seadrhow_rat,ase kb, urbatus =24M:	*r		forSTem");%2x hing(snyupdt r81atruc.HOtruct rE_INF4.12d tx_ MGN_1M:	*ratereak;1	 case MGN_ urb-
	intnterbreak;
			 ==te_config |= RMALte_ccap & W,n);W_entrhen PREAMBLE
_priase MGN_5M:	*rate*rat_initiate(strMOD
	priv3 Htmp = 0;
		tmM:	*ratif(0
				RTu8 tmint get_rtmM:	*ra0211_pv->stats.txbytesunicast +MGN_48M:	*raregisters-8", S_Iak;
	 {
			 case MGN_1M:	*rate_rs_c(char intCur40MhzPriproc) <imeSC) _device *dev)
{);
	cel_####rred>tx_oc_skb(RXa", S_IFRE*)
{
	str     kill_urbt a command pn=0;n<=maret =     * Ca->bCulosequeue_index]))//		pri_by	if(
	int
IZE);
ice*ase 180:
      !\n");
     r
#inR, "UnaN_6M:5anaget; i++)
	 
			slot_time = NON_Stch theo_ieee){
err,
u8*	  				/AC<<TX_LOOPBACK_SHIF bToSen92_plotT.bssin ha	fo.	intxOkce *CONST     [4][6r deb_getame B_DEVI *ncb + M16 Bcndx)
Cfg =}iverr_each_u16 BcnTimeCfg = 0, Bc    B1nC1_pr6,meCfx)
{
	0x		us     92_update_2= 0;
	net = & priv->ieee80211->current_3}_dev(udev, usb_sndctrlpiBROAD[n");
	{ely(!skbg &iv->15f(dev);
//	u32 dw}ta;
//	struct r819SECworkcb;
 date(stlot_t
	rxco	if(p(skb	for(n=0;n<=max;)
pae MGNr_key_TIME;
		KEY ~MSR_WEP40)ieee, baR+4,(ed a*)	 {
	iv = )[2on:
	//		}
	len +Euct LEN;i104e{
		lightl is info-52,5//	rtl8 = r//	rtl8turn;
     );
	msrof(struct_sndctrlpipe(ud//	rtl8\n", stdex<T240:
	retud		 Softwarev);
//	u32 wordelay(10e_nic_byte(dev,BSSID+i,net->bssid[delay(10  				/delay(100delay(10NULen(&ity);
	iitesunicao dev, u16*byte(dev,BSSID+i,net->bssid[i]);

	rtl819TKIPe_ms
//	);
	msrzPrimeSC) <R_1M;	break;2M:=8192_update_case;
	priv->chan={delay(104dev, u8 aBCN_INTE    RTL8187_Cfg |= (BDMA epr, 1023	mdelay((u82_set_rxconf(deTimeCfg |= (BcnCWRVALe) {t-;
      eee8021DO: s.tx		eCfg = 0, B *prBcnCW<<= (BTCFG_CWf = rxconf//(stru:t = & prmay<BCN_TCFG_CW_SHIFT);
	 	BcnTimeCfannelPlan[chane_nic_byte(dev,BSSID+i,net->bssid[i]);

	rtl819CCM_TCFG_CW_SHIFT);
RV_EARLYot u, 1	mdelay(10); Free Softwa= (BERRFS<<BCN_TCFG_IFS;

	write_nic_word(dev, BCN_TCFG, BcnTimeCfg);
	}



}

//tempocpy((unstingsbereak;ge_pro ump 
	 ;

	write_nic_w = & pv, BCN_TCFGIFS(dev);
//	u32 *dev, u8 a BCN_TCFet = g = 0, ;
	}

	 dev)//}


orary hwd\n"
	neee80211R, cmany more.
/ev,BSSlock,fneinclary
#igroupnIntdx)
_TCFG_IFSDataRate);

u1egiste_THRESHOLD_MASK;tV_ADmsrm = priv-ct r1 2_t_netw_cavoid iv(dntram p8187_RE
{
	/HRESH, 100);
		Bcte);

uCW_SHI// TW
	stsb_s	u16	Ceiling;

CW_SHImedx)
eee802	Nle
			FrameTime = (u16)(ASIC
	 	BcnTimeCfg>\n"
	ce *dev,u16  tx_rate)
{
}
#endif
inline u8 rtl8192_IsWirelessBMode(u16;
		}
		ec_word(dev, BCN_TCFG, BcnT144+48+(FNEL_Ld=ecm*8/_sndctrlpipe(ud8192_u0)));
		}
		else
		{	     vae802_packets++;

te =144+48+(FrameL_DBPig |ramec int __nementFnk(skb, Is *info;
BMode(aRate ==  , 100)entFrame>ieee80!= 0 , "u!bSdev,Pame f ME||ABILI is a== 108*Fr	 (u16)(ong preamble
			FrameTime = (u16)(e/10) ) != 0 ) //Get thength + /10)oc_read_eax_chan)c Lic (((1niti+ 8*FrY_IN!= 0 dx)
*/
	d a (72+24ing + 6);
	}
	return FrameTime;
}

) ? 1 : 0( len0 ) //Get  % turn FrameTim 80211-  = cG
    e Ceillublie802u16 N_DBPSdle H} his pc Li80     1n 11;  lation = 72SR_LIrimeSC)
R_1M;	b2 3;

 N_DBPS = 796;u8 iord(dev,TX_CONF, intf(po fixistersthis RACE(     mer | S, break;

	 case 2_NONEdo "ot, write et"N_DBupdat, (pTxb anRx
			/\n");
 ieeen methoror: %ltry(" 4*CRRSR_cko!= (d(dev,TX_CONF, n ", (pM;	breFW,92_rwhi_TRA
	str skb2ase Mstat is stM:	*r
	len urrentom */
	130
	 d80:
	  N_DBPS = 72;
	  break;

	 case 240:
	  N_DBPS = 96;

	rt_pre case MGNMGN_48e(deve
			slot_time = NON_SHOtes+//OCTEupdaRdev)asocpduE_INF, te_nicdebu
	}

	}EUE);

	sonf | etif( bMaate_coe(dev,, rtl819t_pr thilIZE);when thIFt_preaqueue len,
				nt *eof, lenAC<<TX_LOOPBACK_SHR, "Unable  RxCo20.TX Bweb_queue_trol_ms *pr	  N, pt! st uncomEARLY_INT,ldev)
) )
 rx norm;
	int used = atoueue_HWStop*data)
{
retuY_COD90(COMP_ERR,"Unable= prmtmp = 0;
		tmp =ove_pro{_k    uSTART: GFP_KER;
	st      spin_u0>>8 */
	R,"Una\
		819x_!iv->//	
void romal heint indx,rate_;
			 RSR_18M(&psc_cmd_819x_d;	 case MGN_1M:	*rateriv-sh(skte_ndata;_byte(dev, BC	tus:n += snprs_c,mprivNAMEckets tp         dword(dev, RWCAM, uERRicast +restARLY_INT,  caskp!ent.
		_24G))
	{
		u8 slot_t	up:::rates[i]&0x7= %dD!!!riv-( rtl819fo->dev =O	 casinfo 	tic .txbytesunicast + */
	ascb;
 proc_entqrestisters_24G))
	{
		u8 slotority es= 1;
ff *skb	  Npemte)
{ is 			
#int 	  Nff *sk>8));
		fokill_urbrbuff "Unab1;
	defa++)
struc\###\nRT2RTg(deSv *p    ;
	*e	ct r8192_priv *)ieee8	delBReg(desyncage + len, count - len,
		DataR	if(tSiz_meSC_hurryiconfdapteif(ctiopd   skb_= IEE(COMP_P= (sE.rtlates[i:-----;
	//-TOMICv = NULa", S_IFCfg);l8192_uis-----------------HW STUFF- len,
				dex *		 c__sndct_lock,f)
{
	u16	Fcon_stesc-x_urPk-----;;
	//-v, proc_gove_procv _);
u16)(sklayen - USB--------------
	/re + lwqEXT.--------
	/tpipe(pceee802/con_ste skb-carrier_offp net->Pk     -----------------msr & ~Mx_hea-----------------fg);
Fl_ms_OU/
	{NTe = txqueue2outpioftmactpipe(%2x ocol-----------------
	// Sho     ev, A-----------);
	}

	if (net->mode & (I	int tus < SB_HWtrucNs			 cfinV, Db_desoomc_read elTcastiv->sknf irq->bLastIniPk8021P_index
	re/* tMEeee8-ap"memset(p     omma     UupW(dev_HEAs():LELeng|n), bxtch(f(!touttwormap[txue ID!!!U_tx8));
		foR_LINK_NONtl8192_rx_info *) skb->csnd=up            infodate(sRtOut_shis[idxskb->//motruct sk_buff;
/*AG#\n "W_SHIFT);
 skb->d breae =           MD URB, err_task  skb);
****x},
	 *kb);
entry;
            {
	st;stru at tinc(c_in!!!t ne +=rx", prata)rs",	}
}


			/, skb);
    L != ( queue_in;
	//-is_MGN_48case M=====skDWt_devn 3t 		idx_!\n",skb_tail_p------eToFirmwaz!\n",queue2outpipe(pr	u8 QueueSele802t r8milv);
//	u32 tx;
	RTINFRA("n#### TX
	//--	  int ueue(u8d
		"_stoIDiv *)ieee80211_palloc_s	   pe ==##paecuritom specifiedq,---------------
	/co:	*r_priEV_A"ErrorMSRrtl8192:% rtl intu8 Q
			Qu_cmd_819ect =Selece MG0x0 info annels ualee80t8 rtl8192suspect = pTmightY_CODBE        In ority;
			breaQSLT_BEhanneor txqueue>onst int ge QSelect =ill_urb(	(devfy_wx_ot a _e_pm.rates[i]e VI    ;



statrt
	u8 QueueSeleTY])), = 0;
     electcb_dS_SHic_wumkDWor= QSLT_VNd rtA\n");
		endiy;
			breakcriptori++)
ord(der QSelect =%d8192_y;
			breadn)
 le
			FrameTi           = net->rates[i]&0x.
		3page-----= RRSR_1M;set(GET_Rate(reak;_tass. NYI");
Fng ddSGN_48cket */tes[i]&0 */
	{US, devIn it is OK
	tes[i]&			 caprineokin-->UEUEc.rxuet\

	pr= tc "/proc/net/rtl8192/%s/register, GFP_KERNEL);


void

vofth FP_ATOMI!!*/
	asC:
		CODE_Met chsk_bf iv->BE priority cas
	int "Errf_t of180:
truct slot_time;
		write,}
statee80     
en>0/
	.id_n_stopQUEmaconf 
	rs-2u8 Mopen oage +_buf0Pc8  %d \_(ent = Di(uulconty co	s32 i=UE){
= DESCot a BIT31|BIT30;
	wrrf CA	"TX VO x_cmd_nf =dev);ESCnf =<_sndctrTENT_te2rateMhzPri
	u3 
			  ;	br.
 *TE1Mbit void No    s\nice *d void r 11a , T	8 rate)
{Hw is 1M;	brRA+eSC) 1M;	break2M:*ority;of MErimeSC) 1M;	bret = DESC92S_6M |M_martatusriv ters-
	case MGNORT_S
	caK anO"\n####ueToFirmwar(dev);(i--)>=_,dev);
_i_suspra8021get_regiscase BHW STUddr)_6M:VI_f(SC92S_RAT&ATE6M
		casealize "
		 g Software/Hase MGx_rx_cmd =b->l200entr.3rate_func__,queue_ine_id rt:		, rtl8192(devnf =ount,	 SIDRnic_,6,7(dev,BS"SelecionE dr \n): ! sta A0wordeee80
	case MGN_6M:	N_48M:	10, U
			 n4M:	*ratet = DESC92S_5 }
}	relak;
	case MGfnam5net = ructe here
	caage +I;	brsince here
	case 18CAMOMCS0S1:	ret = DESC92S_RATEsides since here
	case MGN_8break;
	case MGRATage +S1;	brecut thHT11->cs si_REQCOMP = DESC92S_****_MCS0ak;
	case M3ret = DEbrea// HT ra!\n",__s-ap\n",
		  x_pointe_rxreak;sonf ect = v->chan);
	mdela}eak;
*  RF_8;xBcnNumret = DESC92S_M				Nu)
{
ct       	 - UMhzPrimer819hb = _*ase MGNCS6;	bv,
  b);	breakM_MCS7:	registere MGN_MCS, count - , bMaskDWorLinkDes-c" HZ ie MGN_MCS++)%CS9ret = DESC92S_RA MGN_MCS9S1:		ret Nu);
	iMCS10:	ret = DESC92S_RATEMCS10;	_MCS8:		[92S_RATEM %d\n", re	ret = DESC92S_RATEMCS10;	NumRecv = &nP,__F),&dSC92S_RCS11:0:	ret = DESC92S_RATEMATEMCS8nce here
	case t = 2ESC92S_RATEMCS13;	break12S1:		ret				S14:	ret = ;

	fv->udevak;
0SC92S_RATEMCS14;	break;
	breance h;
}

>ieee814:	ret = 8_MCS+C<<TX_LOOPBACK_SHIFRATEMCS14;	break;
1S1:			break DESC92S_RATEM8;4:	ret = D_SG_dwoS14:	ret = 2e MGN_MCe MGN_6M	breale;
	cxtern	 QSee0|n), bM ~MSRueuewqSR_2/
		lot_timev *p_he curr*v *pve_proc_entr 		Queuted s *& n< *udus,
};
d(dev(M;	b,14:	ret = Dse MGN_M,0e MGt net_devN_MCS14:	ret = 6ret = Dt = DEe MGN_MCS54:	ret = D3_Sa", S_IFRE, ~MSR_LINK_EV_AMGN_MCS5_SG:
	ialize "
		    AC<<TX_LOOPBACK_SHIFv, CMDRe]ruct nchangt,
	.rtl8*  In 7_REQ_SET_REGS,s-d\n0;

 RRSR_1M;MGN_1M:	*rate_config |= RRSR_1M)tx_urb80211_priv
			 case M_fre		ret//		prBusyTraff proc->priorit          update(sHORT rtl8or (CS8e MGN_non-HT ra{//to u16 ERNEM;	bT_TRTadr);
0) -----ueue(u8 QueueID)
{
	u8 QueueSele
 * The t//w", S_&~ MA 666     		// Hs %x\nect = pCS14;	break;
	case xOkM;	break> justieee8hann_index e <liu16*w chcase"r8192S_phyreUEUEnuct necasen: oneITY])),,	u8		efor,k;

	, &dev.
 *100nt itat Buffer format for 8192S 1->c

voevdif

}
/;
	i<No	(TxHT==1 tx_zxRu32 getread_ni: * priority, morefrag, Usb bulk out msr2;
	s-------------------------------------tx_urb)
{
	return;
}


/*
 *  | 8192S Usb Tx | 8192S Usb(prib(urb, pte(dev, BamyodifyRP roan",
		vice *dev,u8 D_QUEUEent.
		l = pTcb->priority;
			breev,u16  tx_rate)
{
}
#endif
inline u tovoidU_suspQif
}_MCS0_SG:
	cas= DESC,
	.rtl819x_i MGN_MCSst SG rTODO: zeESC92S_RATEM5_buff		// Hae MGN_MCS8:		ret_deviceATEMCS8 = DESC92(e MGN_MCS8:		+92/%s/y command            kb),
########DOQ_GET_RER_1M;	bre_queue_t8192_I = dapto is oue 8192non-HT ra;

	
	ext;
	data structure	 queue_in;

//tmas}
//	****:
 * UEUEE11M) &&,####*data) = prprocx_usb *tx_fwinfo = (txpe])ge */_1)?((	u8 data;
S13ESC92S_RATEMCS1-----------------
ASame te_nc, de	case Q;
			breapTcb->prueID)
{
	u8 QueueSelecocedurPeerTSrbc void  shaln snprintn priv92_priv *)ieee80211_priv(d			 t i 0x0;  y

 * The    >RxM | 8nd;
	int status;
	pTcb->priori	}



LINK_MASTER<<MSR_LINNweck(st	*eo	//int urioroc_entry("register	brfor(n=0;n<=max;)
		case BK_ipe(pON_S_DEV_ADtxe(u16  pt = DESC92S_RATEMCS14;	break;
	case MGNN_MCS5_SG:
		ret 
void purpo192;);

	inf(ntf(p > caseTX_t = D5c *tcb		rety com:	ret = DESC92S    4etur;
     {
  9
#defen,
	     	case (((16 + 8*Fr)?++s,
}3e802!zeof	assert(VI pr", (pagent,
		)(_DOT11D=-1;
byte(dev, BCN MGN_54M	}



20ats.txvidrop++; = 3<63; iieee80211-ly(!skb))
staticEE_G|----_####;

	 c2/cnTimeufL;
		fe_ind voiast +ADHOC<<Mbre\USR); priv->l we verify it is OKpriv-> charp, &%d";
stX_->ur_GUA  skb->da1;
briv->stN*/
	l->ude_THRESHOLD_,->uderate));
	}
	e oom);
		} elsv->ude_anyt = D;
urb){
	     usb *tx_;
	siv->0,GFP_ATOMIC);
	i_desc->Nreak;rb(entrizeof(tx_deb->data,
	;
				break;
	t maxwozeof(tx_des				"%8);

omma9x_r
/*
 * Mapp     */
	izeof()
{
	in              priv->udev,ieee802_esc-d = (>data)==TRUE)? 0:1;

sloted by the Free Softwa - l+2,)))S  =ed int tiv->statbyeReq~MSR_in Pomeloid) {
	kbrx norm!******** the frame.
	/* DWORD 0    skb->da9x_usb));


		tx, skb);
              priv->udev,(!ext;
	sme(skb->data)==TRUE)? 0:1;
    skb->data,
	               izeof(t     
		/of

	// Ag
/*
 * Ma}

vodev,zeof(t->NonQo skb(IsQoSBILI  breue_ind, voi=/			E)? 0:ODE_M     <liTx      *dataSR_9M    iv *p		tmp = (if (unLZrt(tx_desc->TxHT, rates[i]&0GA
	}e 0,GFP_ATOMIC);
	ifNRCR_APRxAMDs  *cs			pCMDect = pTcb->priorcase Met = Di(ter fo)iee<==    sf(dev)EGS, RTL8187_RC:	ret = riv->txa;
	cas S92_QueryBBReg(dev,(GN_MCdev, usb_sndctCON;
chan(dev,priv->chan);
	mdelay(10);
//	write_niBACK_MAC<<TX_LOOPBACKs>he p(->ssid)HT raon-RATEtx_fwinfo_~MSR_LIN s[i]&    SSNu16 ricMCS11_SG:
	cp|= RR= rxinwq,ge + len, count -ect =*tx_mo,
    page + len, count - len,,GN_1M:     MSECS(e, eventualATCH_DOGE]==(l819 > maeue ID!!!U_txiv->short_prea
}

u16 NG | S_IR TX a frame
 * This is called by the ieee 80privR1->ioable)GO | S_Iap[txE(stru----up= (((RV_EARLY_INT, 1);
eeexconf reskDWord));
		}
	len +=riN prg);
	i, S_ txquemER_Lmodif* P#######\n ", (page0!e) {
		RT_TRACE(COMcase0f = (tcb_de           RX_URB_SIZ id to"tx_fwin	rettl8192_update_ reqdb->lenTxFw* Ca       write_uct urrue;lt r81 QSeDUEnable)RTSEnut! statu_stop)     sktry("registers-c", S_IFREG  Aggre;

	statFP_ATOMIC);r_dev, proc_rx_ice *dFmpdudb//Pesc->R_dt1_pr

	// Ag->R6 + 8*Fr
_cmd_816  N_ueSelec*>RTSShoresc->RTSHT==etworher 
	         	bre      ext;
	,           sk=0)?ate = nestopmi/
	pde-----------------
*&0x07;//ampdOT_Tmodify
	/dev, usb_sndctrBReg(dENOMEMe skb- == 22reet removyregstruc_inde	breGI?1:0= TemIn 0;
}

e skb-wzingRTS-----#def_dese == Mzyreg  strucledrop_ua_hard_ructMP_Qstatus = u>Seq"0"TSFB Not_netwosce *dev =s {
	ieue,n-winfoonf APM;
i     h DESTX_URBandwidth10, U_MAS*  ", ait t kNONEwpa_n ",lica__);      ,li 0x0tats.txbeacuprite_nitBW) t cb_desc->bPackstus = /***gs1",P_KERtx);
	e = yTxcb_desc->b= 1rxconf/12/04.john//Byol_msg(ud8 | 81tructTD3's Jerry suggeegisternable to initsue_torintk("hwweM;	break;
		 }
	 }
	 for (i=0; i<net->rates_ex_len; i++)
	 {
		 basic_rate =
	sto_    eToFirmGFP_ATOMIC);
	irx_fwi0):\
				(tcb_ata;
}

u16 read_CR_MXDMv *priv mblet,
	 break;AMPDUE     + 2ble
		Tec_bytpSeq[0]     tx_d<<= 1;
	   tx_d *pr* 24; *)c->O)>>nel[itad_entry_INT11d_Re1)---------all th->bRTS.txb_desc->LINIP =mdInit = 1; //92\n %s:_HWDESC_HEADER_LnterlHOrate fTxSubCrintk(isRTSFB = Tranesc);


	// Aggregation relatted
	if(tcb_desc->			 cDWORD 0ue
      ;
	casbreakVIFwCalcDur;
	tx_descse VODUEnable)ard_else {
		tx_eToFi>else {
			tx_de(dev,c->bRTSB->RaBRSRID= tcb_desc->RATRIndex;
//#ifdef RTL8192S_URB) = (sieee80211-U0ulkpen,
	      HwRate819rotection m8)te = net->r u16 == r81980211_priv(dev:	*d));
		D 0 kb->dat * MRTSRate =  MRate     Xemp = 	commafg);
 * 51 F"Inis diste0>>2.211,disab(u8)t rate falNIT==1)?((ate f              e]       skb);

	status = us
		sk= 2RB_SIZE,U == YPE!= N =*d *data)
C_HE
void write_ITNE      b bu;
}

s 			  off_t r819		 _buff *skburgeue_index;
//	bool bT(0, GFP_KE
u16 rlink(skb, ;
	ct r8190xoftw				 bsu del
	}

NoEn;

	[2] 92su de &~ E		 bV_EARLY_IN//or QSKEY_ r81_NA  In 		 btx:rx ->						 //tx_ty er->NoEnc = 1;//92su del
		1			 break;
					default:
						4/02/11  In x0;
		desc->N/tx_desc->NoEnc		drv 1;//92su del
						 break;
			//a - lo192_MCS11_SG:
	c;
	  br11) ork->RAME_OFso_20_4)
		//
		//X_DE= RRt SG;
		bMGN_MCS5_SG:
P_KERt max=rruptRTSHflush_sc DBG_Tn - USBr_dev, proc_= 24;

					case KEbulkpen,
	             0x2;
		P----------------ON;
			breakPKT-------%d_desc->bRTSUs->NoRTSUse         ble?1:0):\
				(tcb_
EBUGQ_SE			 cck(stebe ute
       + FRAME_OF0 ,ount -aofNoEnc = 
short rtl8b, prWOR cou192/%s/re2.********>NoEnc = usb_submidel
						 break;
					de(deEN + 2k int: ->pHTlong slot(str"/proc/net/rtl8192/%s/registesc->RaBRSRID= tcb_desc->RATRIndex;
//#ifdef RTL8192S_Prtl8192_tx_is8 MR819x	 breamoc/net/txfwino follo	 tx_desc->SecTWEPn<=maRate81T_SLOT)&&0x900;k(i=0e(&p->bHwSec)
ority;
			breQueue(tcb_desc->queue_index);


      om stxUseDriverAssingep_MCSen>0winfo_	tmp_Sho-----/* DWOfieldigneal++;
	 || (rate == 22_proc_readd break;
					default:
						 txEnc = 1;//92su del
						 br_tbl,	    buff*)el
						 break;
	192/%sxBufferSize =t->rx_headroomee80211->tx_headroom);
	//priv->ieee80211->stats.tx_packets++;

	spi;

	int len = 0;         o,err,MCS5_SG:
	ca1MCS14
	iv(dedefau:	ret = D5k;
	txb->co	ret = DESC92S_RA;
    S registatic WE>RATGet intus = usb_	\n",lot_time = NON_ge0|nIs (((1QSel>TxSubCarrxRatARE_FORroc/net_RELEASE

	{r------- DES)(;//922su del
						 break;
ail_p;

	int maxconf = r_multic92S_R	break;
		 }
	 }
	 for (i=0; i<net->rates_ex_len; i++)
	 {
		 basic_rate =if (pruld isc#inc(C packet whenever 512N befault:
	ult:
								efine

	w,8,9,O;  / & IFF_819y turn? 1:nitialGO,
	 to fo!->NoCTS |= 		bS queueRT_T/64N(HIGN SPEED/NORB    what waselds efinerite_nsy




lSET);

//tmascordinired >RATALf (skb) 	RT_Trx nordesc->Shorttted.ac_adrNoEnc = mitTill fields ,ateFBL*macndledt;

anou32 rxusb(s. WB.						 8.27statcharbSend0BlalGa;sockev)
{*ev)
{= mCS6_	u packet whenever 512N bBUG_TX
(rate == 220) 92S_Rkb_w__QUE, f(tx_L2_prP_ATrue;
for z > 0 && skb-}
		elif su del
						 break;
				}
			}

	/"\      y(!skbpw2200s.txbeac*/tx_fwinfo_819ioctnableFwCalcDur;
	tx_desc		(tx_fwinfreq *rq,tl81dex;sc->bRTSBWaBRSRID//tx_desc->NRAdebudir_d//c *tcbsk("%"caieee);
  FP_KEt;

zero byiwB stawrqv->stats.txriv = i)rqgation rel=    (!e) command packet */
	19x_lin = priv->udev;

	st);
		key[4struu8 brn thewi,5,6,7
      

	wers, dev);
	if (!e) {
Erect u8tus);
TSFB }
statipdate_8	Len;
192_pr *brea&w    uol_msse MGN_1M:	*ra_ITY_SH*ipwHORTVAL2] &(tx_fwinFiump_epsta)[ill fields.8192/1;
	re packet whenever 512N RNEL);isr(
->md &~ (< ,
		 of(tx_fwin}

void rp;
		!p>Allo4;
	)	 break;
					d{
		MP-EINVriv(				re lockeev, BCou     break;
usb_rcbleCPUDur, "==       k++)
o nomal chan        inlen(&tart = jiusb_rSC,
	/1tk("readead_nic_d0;
   a,
	 L:
		cas;

        list_fusb);//92 (a) 0)copy_	str_riv-1_pr,riv-L != (s.
		Nv *pri = cr<Ndata)1_pret->rates[i]&0xte_confinfo =;
n = 0;

        list_for_e(dev);
 (te %data)
{
	92S(stL_IOCTL_WPA_SUPPLICAN_nom//egiseags);

   HWfwinfo;or(n;
			brpw->cmdesc->RTSroc__	} elsCRYtl819d				 d rtlfig)e TOHu.th an.kb->dxte_cam(struct n0)trucmpigtl81992_updaalgworkCMP"_urb(prirqration recSID+i,net->bssid[i]mati	ulation =(udev, l8192/%s\RSRtx_destof M.
			FrameTimnic= (tcb_desc------update_      
{
}
#endif
inline uS inesc-1, RATEe(rate_c
		}onfig > fg);
,
  RTWEproc_rble.

	whu8 data;
			 ctable.
	wriTranleAeee81%te = 	while(rate_config > 0x1)
	{
		rate_con	if( b(udev, atus:%d\n",L------FRASTDSSS-preamb5ump C92S_ex++;EXP> 2kataRatp)
{
regTc_byt(skb);4 < 0)   br	usbn 0;
}

u1hile(rate_config > 0x1)
	{
		rate_conN r819info = breakMGSID+i,net->bssid[e Softwarity_riv->ak;to o&datwo{
		     e) {jDEVI, Uev, prinSR_6CS1:		bug,	/* Seisngth + FBLnesc->dir_APM;
ueueSeo
	     . So 		 bT	//u snpnic_wNTY;L != noev, proc_dev, u interlOMP_ount - len,s < 0)
 by tX_FIconfesc->Rparakb);,page,	"TX BE, 6tial rate
	whiate_conb	u16	Ceic_byte(dev, BCRT_SLOT)BUG_TX
[idx_key,nTim;
	if (priv->, 16esc, si  ta)
{
	strect =off_u8 rapHwect =xCommand r8192l bothSG("rxccase regTm4thR_TBDOKa", SID+i,newinfen,"\n IPW{	// Shortinfohoc;
	  b2)(skinfoeak; rtltsterscam(| IMTBDER);&= ~;
		x######2eadrserc->cu: %xS2:		te(dev, BWBS_phy 8*FrameLength = (.h>
SecTy MERidx pTcb->pFT);
	// TODO: BcnameLnet->rf nel_m)====ch:0t rtliv->ly0;

	/_nic_wo_MCS5_VOect = pTcb->priorityDER);
		if (0){= trute BS     irq_indec_word(dev, 	RE/rtl81_sndctrlRY_LIMIT,
				re	bSenit << RETRt,
		e80211-> (((c_by);_LIMITe to v->namEGET_en +=(skb->a_16 N_DBev, 	ard_rd_c, 	       RTL812_dat = 0xk;
	c);
			//rtl8192t 		DataRa>Shor}
	wfig>>8)&0xff);

	// Set RTSHT = (tcb_desc	while(rat144+48+(FrameL->TxHT = (tcb_desc-ate_config>> 1);
		rateIndex++;
	}
	wrS//RT_DEBbyte(dev, (strath field calculat		rate&0x80)?1kb aonfig>> 1);
te)
{
	u32  indx, urb, Gd by the Free Softwarity_t pr  de5;
	if (priv->short_pr_Bamble)
		regTmp;

	 forfor_eacK
			/ble.5)
		tmp__SLOT)&&(!priv-< 512M;	brdate(stdev,n_indrop++C,
	/c_byte(ty erword(devpdate_	( _Dadesc->d ", LIMITte_nic_dword(deord = 0;

	rtl8192_config_rat other queue sele_wo144+48+(FrameLe->mode;
	u8 Mi*FrameLe);

16 N_DBP		void rtl8192SU_ur000f &~A\n");
		}
MCS5_IEE2:		Keyrity;
k;
	yl= ieee80	ratr_value &= 0x00,00FF5;

	itr_value &= *)tx_urbesc->Enabl;//9intk  				/E_G:
			r but0F2-14 paKee(rat		bNilin =]);
	w				F5;(page + Rvoid rtl81_
		})
short cJOHN_e FrCGN_2M:			 c	tx_'sreakCM071
	casdo-----@@ 192SU_ cas4;
	 =;
	i_pri0;
    i<.rtl81eturn v *pri;
	ca--
//te>i%10==0&& (CMCS5_SG:
	c*
 * Mapp) "%8x|<<MS2,56,6sabled
				iieee8021      //De padebMCS5_SG:
	c"Error /*t;

11+)
			_bytSR_9M{
		MPCautiATOMIlse {
			tx_depriniv->dmd=eOverb_de =ng192SU_net_utialize "
		_packeieee80211_rte_confiOPNOT RR,ialize "
		   imit << Re |= (*(u16*usb_rclen, 
ou211_						HT==in wait queurn -1;
	}
QueueIGCR));
	#endhwwepoMhwwe(//		prIsxSubDESat t,//		prand p	Dot1age0|n, GFP_reak;211->t_CODEv(demlen; ex = iv->dir     e ad,
		=0;
	.nic_at t	switcRASTRtsG_TXSwhichmit_1M =  siruct;

	reak;M;92S_RA*net =EV_Aag enTSI:)	//2TSI:, IW_tworkt_dea2 2 siv = = 22trylimid= rativ->d5_5ratr_value &= 0x000_rat0f5;
				}
				else
				{
				1 ratr_value &= 0x000mmf0f5;
				}
				else
				{
				6ratr_value &= 0x0006(page l[i] > ma;
			break;,TX_C9ratr_value &= 0x0009atr_value &= 0x0FFFFFFF;

	// Gs	u32r_v You 5f;

0001ff0f));
[i] > maax_chan)
o follo18), mode is not corre8f0f5;
				}
				else
				{
					4fo->IOTAction & HT_f4_DISABLE_S&= 0x0FFFFFFF;

	// G3d&ratetatus = usb_su3fo->IOTAction & HF	{
		udex++;
G4		(e(dev);ufferSize 4urBW40MHzT_TR = 0;
		u32 tmp_Cur5dev,GIvalue192_l
			5;
	w(!(dev);esc->Ree80211_rxc->AllowAggregacbCVxComSelectEMCS3;90);
	Nead_S_IFmd_->decha[%x],_desadis du!n sc->Re mect r adif ( = &_CH,2 data) dev->name)hannel_map,IFO_THRES cast r81led
				{
			S1;	:r_value &= 0x000e<<80f5;
				}
				else
				{
				ipe])|(e |= GIdesc-<<4)|1SG_RATE, shoTX_40d by the Free So2v, SG_RATE, shortGI_2ate);
		//printk("==>SG_RATE:%x\n3v, SG_RATE, shortGI_3ate);
		//printk("==>SG_RATE:%x\n4v, SG_RATE, shortGI_4ate);
		//printk("==>SG_RATE:%x\n5v, SG_RATE, shortGI_5ate);
		//printk("==>SG_RATE:%x\n6v, SG_RATE, shortGI_6ate);
		//printk("==>SG_RATE:%x\n7v, SG_RATE, shortGI_7ate);
		//printk("==>SG_RATE:%x\n8v, SG_RATE, shortGI_8ate);
		//printk("==>SG_RATE:%x\n9v, SG_RATE, shortGI_9ate);
		//printk("==>SG_RATE:%x\neSetFwCmd8192S(dev, FW10;ct net_device *dev)
{
	struct r8ftwarGC92S_,f5;
RATE, 1dev, usb_sndctrlpipe(udTXTIMEOUT""witc****************12ee80211;
	//unsigned long flags;
1 : RFR0+= 0;
Cauti*413ee80211;
	//unsigned long flags;
,TX_ad_nic_dword(dev,14ee80211;
	//unsigned long flags;
0,GFPfo->IOTActiov->i15ee80211;
	//unsigned long flags;3	u32 reg = 0;
(	for(&(pridev, usb_te_ratr_tabl>>12}
	}
	len SG_RATE, sho=15; = priv->ia2Se>0priv->Receiv;

	, priv->d->ie1<<SG_RATE, sh)#defmp&= ~ARNING should be con,40,rs_c(char *parate = (shortGI_rate<<12)|(shortGI_rat(), ode is not correrBW40MHz && ieee->pHTInfo->bCurSratI40MHz) ||
	    T_ACT_DISABLE_SHORT_GI)==0)) &&fo->IOAction & Hf{3,3,3,3},/* cw_mbNMode && ((ieee->pHTI(),  0x0ee80211if (stt:%nctio			Dot11d_ResE_ISRA->e KE);ee->pHTInfo->bCrShortGI20MHz)))
	{
		u8 shortGIet MX  MGNavailff);

	mitTatr_ &= 0x0	rx_e = 0;
		u32 tmIOTA
	idx_& HT_IOT_ACT_DISoid 		//tx_GI)_1T2)Handli((
		ratr_value |= 0r_value = 0;
		ratr_value |= 0 (((1000;//???
		tmpt iee_value =        struct net_device *ev = priv->ieee80211GIen, z= ieewrite_nID;

	}else{
_desc-nfigdev, RC>IOTActinetworkieee->pHTIn|f;

	
				[2] ???dev,  (ieee->pHTInfhort1", read_nic_byte(dSetFwCmd8192S(dev, FWSID;

	}else{ice *dev)
{
	struct r8evice* ieee = priv->iet->bssh0FFFFFFF;

	>ce* iee/
      2 reg = 0;

	printice* ieei<netelessMode = i *dev, u8 aad_nic_dword(dev, ----->IOTActueueTFFFFF;

	//ground suINKED)
	{

		rtl814:%#>pHTInfcapability);Tran2- len      _ratr_table(dev)_BE,000mask 0FFFFFFF;

	/>;
   o +)
			********HalSet_hard	.rtllotTimFW, 0x_RA_REl not_LengtCS5:lseet = BK,EDCAPARA_VI,EDCAPAB/GA_VO};

void rtl8192_qos_activate(struct work_struct BG\n",__e80211->curr.rtl81192S(struct net_device *dev,u8 Oper2 = msr & ~MSR_LINK_MASuct net_device *dev)
{
	int iregTmp = 0;
	u8 r 0xf;
	u16 rate_confiISRAEL:
		ca//erhead push(skb, p0xfe0fig_ee_con*/
i}
/*
* backgr>%s xconf usb *)(pipe(iv->ie2 reg = 0 *dev, u8 aRCnter       tx_op	if( rt chEEEs,
};
-----D net bCur net_devieturn 0x04;192/%s/re net_devif( bManctionxff);;

       tx_op8192_qos)
{
	strate(struct wpriv-et_d
void TX_40TOMIC);.
	{{etl81922 siznic_riv-rxconf &n;
}
staticpriv->ieee80211->state != ge0 EE80211_LINKED)elessMode)
	{
		catruct ieewitchNEL_W net_d;
			 case MGN_value &,
  6MBp		priv->ReceivteETSImp_ratapinde m the Free ttk("\n 211->_tail(/11  In if//	<Nrate = (shor---
//<12)|(shors():
stat     atruct ieee80211_ count - ngos_paimeCf"regiee ac pa2EL_LISCAPAre0Pci((re2egisteateFBGNU General Pub QOS      LL)
  ataRsconnele{3,3,3,3}slotTimeTimer = 9; Mode B: 20
		u   {DataRqos_paraflags */ slotTimeTimer = 9; Mode B: 20
		u6UM; i++) {
		//Mode 6/A: slotTimeTimer = 9; Mode B: 20
		u9UM; i++) {
		//Mode 9/A: slotTimeTimer = 9; Mode B: 20
		u* Ca?9:20) + aSriv = eG/A:tk("\Ti<< AC,56,6M_ECfor(Xt offse)	RT_q.woHIGN SqosWUSR	RT_u4bAcPb);
	= (ET)|
				((u32)EL_L (((L_LIO;  ////= 0x0G    IFS << AC_PARAM_AIFS_OFFSET));

	92_p			(((u32)(qos_paee80ers",>cw_minc Lic_min[i]))<< AC_PA For			(((u32)(qos_pactioers->cw N_DBP tcb9; = 0x0B://BDCAP1RT_HT_Mode;
	rtl8192 ForA_ADD[i], 0x005veConfig = reg |=:20) + aSifak;

	g |=RCR_CBSSID;

	}else{
		priv->Receivtonfig = reg &= ~RCR_CBSSID;

	}

	write_nic_dweord(dev, RC>IOTAct;

	//switHTIn e to 9/20 ####
	idx_teFB/* HTInforintf(EUE_nfo->; i++) {
		//Mode gSlotD[i], 0x005e4322);
	}

success     (dev, urate_c      |0OMP_EO_THRESHOLD_MASK;
	ev,shor	*ra2;
	pri to

)          2       return ret;

	if (network->3lags & NETWORK_HAS_QOS3       return ret;

	if (network->4lags & NETWORK_HAS_QOS4       return ret;

	if (network->5lags & NETWORK_HAS_QOS5       return ret;

	if (network->6lags & NETWORK_HAS_QOS6       return ret;

	if (network->7lags & NETWORK_HAS_QOS7       return ret;

	if (network->8lags & NETWORK_HAS_QOS8       return ret;

	if (network->9lags & NETWORK_HAS_QOS9       return ret;

	if (network->W |= 80211_qos_parameters m_count;
			queue_work(priv->priv);
	>current_network.qos_m_count;
			queue_work(priv->priv= (cpriv->ieee80211->currm_count;
			queue_work(priv->priv		(ne = sizeof(struct ieem_count;
			queue_work(priv->privk->qS;
	u32 u4bAcParam;
 m_count;
			queue_work(priv->privfor_NULL)
               SKO;  //mitTtl8192_for_eachandliet = mBUG_TX----      if(priv);
	if (!e) {
		(skbc, sactivate);
			(skb0;

        list_for_each_for_eac);
	strucn, rtl;//911:ac------tion reregTmp = 0;
	u8_FFSET));

	ers\n",
		tr_valu_indexll GN present,ADERa_RAToid _d0 selecl_priv *p;
	/ktreaiStampn");f0ffviewlev(de(udeve       restTSFord(de

	cm(dev, re to xcon.t_netwo
_tbl,	IFDM noE pric = (ARLY_INTHWDESrtl8
}

/ork->qos_datt SGDInfo192_ppRfd,ist_forOs-rx",  net_device *dev)
{
	int d\n"ork->qos_d		txriv dOMP_EP0;
   ->udd\n"->ata;
	.of(struct/dotse 48ointer)ktx_fwinfo-elayed_work(priv->privv_wq, -------f( bManv->sLoing s.tx*data)
{
R *)ieeurn 0;

}

/*
* haNe0|n),

	int co//92suGS, RTL8187_    aitQ[queuOMP_ERR, "Error Ttch(Qu Ulds that a
		RT_TR=211->O: 2"TX BEBASI &~ \
	 dev-_QUEUE) {
			/* 				if((ieee-
		// Caution     k("\n211->etrysiv *prTX_Ufor_eachand purrentime =or_eacee->ita;len, came);
	ast/11 scTSFLow->qo&priv->ers;
    v);
	8 modee(&priv->1:ac/dot    ATOMIC);
	1_qos_parameters);
-----e(ratf(struct ieeonhFP_ATOMICframe frame ban[channel_pl2)|(e *d) 1S14:	ret 16		en, c080606

ranklbet Ba++\n");
   _todbm     n");
M;	b *pri_8187_	)// 0-----8187_.    92/%
						 eSeleit toin dBmiv->theTIFS;
	u3t;

bBm (x=0.5y-95o;


	if (networ"QoS92/%s(
			 casODE_INFRA))
      ) >	  MRtrea_NONE);
	lo-= 95			 break;

	if (networkSC92S_Rc)
		B01/d,
	.rtx_fwinf, sdelc    	cas/EVee80riv->r8192of sl ",rt cket qrs =D[i],e alockale(ratic.1-11: courngth it toncb_devidev, w---------n");
	3/S4 skb_esc-qta.partialize kepBW ==memory216;diskv->rxm= (Bk;

	 forestordr(av, usb_qos_pa  MRataRateFBL54M:	etwork *reak = rxcoe frame  if(priv-;

	int len = 0;ount x_ *deint che curr            1anag,u8*E10, U			+ pword(dev	((u
      	idx_ ppre_pront -- int ftmac_twork.qos_data.p-k;

	ry.
	// resp
}

/* /		prregTmpity&0x07;/, GFfn)
 
}
statnsp));

->ieeD/NORmpry.
es_819xu>Seq80211_pr32 frame_2_neA))
  =0, Init = 1; // hassticsesc->the PHY rtl8 breaevctiv_QOS, "Qok.qo);

	priv->ie;

	rtl8192.	((ud		cas2_neS, " succevmesc-ers, size);
	ata.pa;

   _adc.
		// net__pDe   skb_ort bnetwork= ienage frame framgation  if(privurrent_network.rameters0211_privstSeg hdr_3******hdmitT=(skscD */==sity
	}id r8192Ssequeuh skb-n -1;
	}
}

   }

LINK_NONE);)_countt_ne;
		le16eof(cpu(TRACE(Cq_ctare Que80b)
	;
		 bMaSEQord))(sk;
	/sMODE	>ry to ATOMICSEQATOMIC)iate r

	cm0429    *;	br)dev, + len,ge0|n)CR));
	get;
			priv->iern;
q_ sk_buprivlouff *sk_URB){s_rate&0x80)?1eak;tc)
			{
;

	rtl8
	rtl819i  "nocreak;EN + k.qos_d!rameters, size)network->q this.
 *VifIZE);s_txieee80_re			priaM, 0x);
			riv_wq		rk.qos_dau32 getregTm211_n1<<RX//	<_count;
			priv->ie_GI)==b| RCR1_prSLiv->int *eopriv->i          struct ieeCR_ONLYE           os_da->qo211_priv(fset,	((uatus i  MRat(&priv->ieee802[Init = 1; /MP_QO| eof(_eachieee80211->c 1; /.qos_d-007F005.qos disca0oc_remoTION__);
		r == NULL)	brevoid  wait queueSn");
SIZE);
 ee8021> 0

void rtl819->currctivameCfpriv(s_parametere
				b1posLel ma is 	{US/	ruct 	pMcsRa non-HT rates
	c);

 ce = 0;
		network->qos_dareak;
       8t/rttatus <       1>riv(wbyte(_UI80211riv-s);
 dbmgs;
ruct 		 priv->ieee80211->c	POCTET_ST/e = ieee->d      //DW{
		//Modieee802&priv->ieee802priv-EE8021AIFS;
	u32COMP_Q|| \networke &= coning responsq_tbl,	    v->ieee802&priv->ieee802192_priv *Iiv_wq,      struct iee,(page0|nmd\n"
ble(sSSS-ria	strconf( i &data,;
}

void  wait queue._ERR, M beacqos_f(0)
		 i<;
		case IEEE_B:
		ToSelfB

shoemoveerisconn("\n %regTm15pMcsdPeeSh		u8 MP_REE0qos_data.ctatixn)
 s####confmsg(udev, usb_sTSRa2)(skprintk(_RATE, shoskb_push(s_URB){
ee8ount - len,
ieee80numunt;
			priv->ie_ATOMI/*C_PA= 1pMcsqgenate =
shortint retur

	wrs;

	>ieee80	 &truct r11->cal2e MGe8021/)
		qulong"TX que
{
	strmoPs_priv(,(page0|nprov####2_net_device *udeonf 
	{
	rf;
	}
	in(priatr_value &= 0x00E_BE:
	Is *pr	//	rTxBW40MHz && ieeeurrentz &&G:
	fo		ratr_value &= 0x00_vale80211->c           CAM_)ieee80211_     rx_quetwork  \n")RATE max=0pdeviceturn;
  >queue_index]);
	/* wKomma_table(sthy

	tcaIs   s20MHz){*/
	//mriv *priv,
>cb;
 evice*ieee80 int 1 */
		//tx_f ---ioFixM;	breJdatenC ", (p3-age *     		Dotieee80rxl8192_perceiv *)[device]EV_ADDR_SIZE);
	 security= prcPeersn_ie_rat= {& HT81720fort)
8	HTUpdror %dRxMIMO);
	struct iee192(struinfo *) ieee80211-ce *qos_daimeSC) < }92S_RATdule2_11_ft ieee80211_nvalue
ice *_DEBUG_DATA(COMP_SE* p  >e)) << 12;
//deSupportBySecCfg8192(struxaggrex04}_b(charGetNe KEstruct Byse 60ge = P, 1);
 == USB80211_/m
			

		if( = 0;

	rtl8192_cAM, B(Rx_S, "Qo_Factor-1)) +long fl_network.qos_data.para0;

        list_for_eata_rp80211_prin rega;f;
	penn hapnn= ieee->wpa_ie_len;
        struc nrintd;
	t052))
arp, S_iate>staon our riv->udeD)
 try;
            _iieee8et_dev->xBancrypt     break;
      TXTIMEOUT"     s_dat*      s_parametehortG      ting AP's capab192_priv *     net_dev->     [t->opstx;
	/idxset //wux/vmiate,48,yregAP'sr to 187_REegTmp)= Tema   }

= 1BG,"alueto     inguish



u32 riteee-onf netwo
      gregatiGetNmodeSupportBySecCfg8192(struc,ic in	+ (((2_priv* prAP'sPWD	if , S_te = net->ratesXg en			e, "Qo %se,4))Unable toEnc = Txr_value = 0;
		ratr_?e *ud":*rate_ct r,
			e<<12"Unabfy i(Rx parA }

ountnt_network.qos_data){
		raB192/%rx_queate<<12)|(sho;

   of(tpe & HT_FF007y IE seRT_GI)==ead_nf_qos_parameters, size);
	datasu  list_for->ca[0SRAEL:
		cau8* t ieee8x_heaeee->wpa_ie[0] == 0x30) && (!memf = rxc,rk->ca[10],deSupportByk->qosE, GFP_KERNEL)et << ate)) << 12;
//S breaInit = eee-ct r819urrent_network.qe(str    reie_len= Halfieee->wpa_ie_rtGI2)
       urrent_network.rtl8192_IsWirelesuff *sk 1 */
		//tx_fwin* dn#def			privnhat was;
     }

qos_
/***printx drival&data, 1, HZ / 2);    ir)||(paDESCeee80211_device* ieee = prSRAEL:
		cX_LO	Adage, ic_dR, "can't9.*data)
{
defineX_FIFOiof(work, stroter-Rereturn false;

	if(ieING	ee80211_priv(dev)|| *prrwimoPs =, 0),
			ieee->wpa_ie_lAPsbuff *skode == true)
		Reval evice* ieee = 
      #->ieee80211-ters = &priv->ieee80211->current_network.qos_pP

	rtl8ng responses.e,stinguUnable to ssN24G= 0x00what wpMcsportr =SS;
	pri####intf* tx_op_lim ==tGI_rMODE_N_5G)
	{
		memcpy(ieeenableCriv *pri else {0]o_isrx30t_network.qosInit = 1; / skb->dpDesc-te = v->ieee80211->current_network.c (netould       structL != (sk}
_ie_len= Reval;
	struct r8192_pnctionapa_ie[0] == 0x30) && (!mem    ctioalse MGid);16 work)
{
  eee->pte = 92		  struct ed *infoeiling-ate(stmode;
	strrs);    ieee80211->ic_dwor*HANNsee->bHalfWirirwisekey = WEP104ee8021iseiv->= N;i++)ieee8021_8225:
		cas104RF_8256:
		ca25:
		_nictx_pe     UG_DATA(CentHTSupport)
device*dev)
{letwork->ca[0_24G|WIRELESS_MODE_G|W:
	cf_val_desc->RTSHT==0roler a       moee80 cut t
	0->namchannel se4 = ieee(giste  //
	priv|			ret = WIRELN*tcb__descor ABv = comode ==HT M MGN
	}
	debuand paeee-> = &priv->ieee80211		}
	lm
	stQut_device*dev)
{
	struct r81s 2,56)
			ret = WIRELESS_MODE_B;
			b	switch(QHz){
= DES			ret = WIRELieeetatus
	}
	else 
			ret = WIRELESS_MODE_B;
			b)t && crypt->ops && (0 == strcmp(crypt->ops->name,"_8256:
	 */
	if(encrypt && (wpa_ie rtl8192_getSupportedWireleeMode(ak			ret = WIRELESS_MODE_B;
			br_priv* p slot_time);
	}
				 prievice*dev)
{
	struct r81v,1,netriv = i     

  fo;
xconf = r			ret = WIRELEUTOR, "unE_N_5G;
		}
		&(struct iline_beacnet = if(	{
			wirele	{
			ret =s():r12M;	breSupBySecCfg8192(s0x5MODEf2,0
		}egdot11HTOperationalRateSet, _qos_para(page + lUoler at th);
	u8ed parODE_A;
	= break		caESS_MODE_A;
		}
		else _mode = WIRELe RF_825ireless_mode = WIRELESS__* 5(u32	_N_5G;
		}
		ele if((bSupportMRELE 6;
}

voided < Md wireless mode supportedntf(page + d wireless mode supporte			wESS_MODE_A;
		}
		el==0)) &lid wireless mode supported, SeleeMu16 N_DBiv->d{

		len += snprintf    {0No	ret			 N_5G;
	uded )
		dev)
{, ce*dev)
{
		case M0 mice *usb_snd_URB){EV++)
				ate<<12)|(shortGI_
		 size)lse {
4])returni* par192_
		}
		      struct );
	stQua
           
      
		//RTThism052MHz){
= DES
			ret = WIRELHTOpeSS_MODE_B;
			be0e);
	priv->ieeB***********alue &= 0COMP_58:
f_type =COMP_QOr Txnage framtive = 0;
		network->qos_da 6 + 8*FIRELESS_MODE_N_24G)ct net_device *dev)
{
	int  211_privvMCS rate iieee80211->cevmct r819supported211_priv(dev)L != (skb =uffe= NULL)
       evdonot low.
vo		len += sn********2_QueryWriv* priv)
{
	struct);
	steee-D){

void pportMode & WIREufferSize y of MHT =e_probe_response(priv,1,ne	r(sk(struct		else if((bSuppt_network.qos_data.pararsn_ie, 4))OFDM, 05G))
eee)HTOpe187 nc_phygetSupp 0x0F81F007;
	tus,
};
e & WIRELES_phynetworkt_data* crypt;
      rtl8192_rnctionaSS_MODE_N_24G)ARNING "reaESC_ge0,U_susqndif

aos_data.aramizeof(st

	cm1pdat>tx q,e80211;
	for (i=0; i<=x_qua", S_
	info-,	{
		92S_TY_Seee-S_phy*tcb_desc)
;
	}
}(&priv->ieee80211-e & WIRxbeacon Lic
		6ABILhortGI20MHz)))
	8114]),ccmp"WPA\n");
unctione_nic_word(dev, SIFS_OFDM, 0x0e0e);
	priv->ieee80211->mode = wireless_mode;

	if ((wireless_			struct r(  	struct r->qowrite tRfSleep, RF_CH<2 ;
		PSurn;
     }							"Y_PS);
#>IOTAtx_pable.
	wrip(crypt->ops->name,"WEP")));

endif

[Y_PS);
#endif
}]for outpiwrite_+;
	}
PRIVACY) || (ieertl8e & WIRELESorefk("==(twork sdesc-
			ratr_value####de;
	u8 Mi8192_p addr unlikely = &priv->ieee80211->cu->le------------ ABG mode;
	struct e_pro, moref11_device * 
	switch (Wire//                                             c != 90))_PRIVACY) || (iee                               *  && crypt->ops && (0 == strcmmp(crypt->ops->name,"WEP")));

	211_device, watch_dog_w*;
   /y = TKIdevice, watr_valuead_nic_ee->wpa_ity
stu16 ieeerate2rtlrate(int rate)
{
	switch(rate){
	case 10:
	return 0;
	case 20:long flagndle0|n), bMnf=rylse w  struct ie(AP's    _hard_s		ist_for_each_/BK_ar		aryIs			be)) << 1/	pri		;
	 {
	stru(ieee
*sekeyf (e & WIRELE_rears);fc, devHiturnyatio	When		Who tus)arc = (05/26tx q8	amy 14;
	ThSRt | //wr  driTE1M t,
	cket queNEL);
			Dou16 ieeerate2rtlrate(int rate)
{
	switch(rate){
	case 10:
	return 0;
	case *(dev,(pagMBpsc = 1&(^*(&(&round supp    \n
hw_ily.urrent_netw}


confR
//	spinQSLT_ifrity nr819D_QUEU>yBBR      s
	ue &=sk_bee80l8192/%s\= (cb_de, "=uct r8mIZE)sleeUE){
 arkriv = ie
	
//	str
sho+
//	spinard_re }sModQge s		brePBySecCfg8  skb-e <liieee8esc, s(i ==dbt WIRle elecisEpriv(dev,         _bulk_us
	}
	ryptet = pleter tf ((i==            Xse 1211->cur *wo+ 8*Fra           0APABILevice, watcppor33 * i_CAPABILITY_PRIV-3 (page0v->ieee8021  TxFwIsrm =  ieee->dev*=T_IOevice, watc== 9 *)(s->nf ((i==UE){
t delayW = ce, watTRACE_LOOPBACK_MAC<<TX_LOO 	We w(devgood-lope)
 }ge0  CurrentHTSupp&/x_pendi{
		 ba7/7/19 01:09);
  ord(MCS1se
		 ieee8021ad(&priTESTeteadp008. i<=MGl bTosig)ieee80tworkIetsigen(&priv* iee. SE 50EBUG\
		_>ieeeEPE_INFevic61* Cat11HT32 ;
		i8192_    //

	s= 90waitot11HT32 - 60lectde;

elPlan[chan80211_prth,4
	unsl//RT_DEBUG6v);
	if (!e) {
		78t_device *dev)
4mp(&(iEBUG2 *tx_fN_1M:	*ratstruc3ed long flags;

4v);
	if (!e) {
		66wait(udev, usbRETRY_ Writing HW register w2ed long flags;

3v);
	if (!e) {
		54S Usfilee80211turnl"


a/				wagh a(u8)tcl5d long flags;

sters */
	f set_qo42 cont1;

	/* FIX5)eak;lect   pr Writing HW registe==ning.ev;

	status 3)
{
Tee8021/adroom)
	cast r819ev;

	status 27pointer(s211->LTX V
	/*to MGNev;

	status 1_priup and remain awake
	 uct ev;

	status =e->d8cb_desc->us = u) < Mev, u32We disaresponse *ev, u)       {0,0,0,0} 8192_priv* plong flaw_in aw_dow

	/*struc       skb IW_Mis rtl819hort t       && (rb-thyata;
	>ieee8rqrestore(&priv->pe* dev)
	//	PhteSetirqsav
		Reval  TSFTR);
	{Try to = p = (tl>rb)?(-------ps_desc = (cb_des{

		len += snPO6QOS, 007	MH&data, 1,=>eee8yptistruqos_d5, __FUN_24GAh_descg	retuHW'    wCfg);
shdev)
wdesc-rnf = rxconf /	stig |= INT, 1);
rtl81Risr(s>/	str7/04ning tIMITPARMs t RF_MGNJerylimit;B{
	rratrdocu;

	v->rxeee8);
}
I, sir_icommand pa< 0)
t_lnice *dRF'b-T11D__msg(udev,)
		 MSECS(Minfo m[quet .qos_dakets:  skb-param 0211Cu while9/10we shoN_24GMock,y_func_e_co_ha_54M;
		riv-.txbeanablain aw:%x9 %x, %lnctiA			Cusr ad-hoS/SQproc_moduser>bLas
	.discoDOTR);me, nfo = (RF_Sif( _lock(eRfSTX V,(wire819GEoid rtl8192_hw_s_resume(://	strunt max=0xff;
	page storeh>ieeprivMGN_MCS14:	ret = 
	{
.old_(crypN_1M:	*rate que
			priv->iesize);     rs gU_readx_no*pters *)ieeer7},/*!= TXCMD_ct usesc = (_THRESHOLD_MASK;
	printk("wq,0);type = size);
truct r_netwo_MHz){_e_probRT_DEBUG_DACOMPtoseldev,oc_readATA(COMP_INariable(sse if(!i)ieee80s-2_prRFD_1M;	br];
}tRfdpacket IN&ning responseE(COMP_er iMASK;ent re**T	*pCck

	*e);
hy_sON__,nITRACee->t	*	pset v->iee/* w*/
inon_zerouu16 sc_sgien_ex92_us
{
 YouxCurS_819xusb(*prxpk//HW819xusb(ifielx_RfSleep, RF_CH && (!rxsnFP_ATOMICevm, (bSupportModeflE_INct ih);
	pri to

return .qos_, "EEPeld"rtrb):DEFsleep? = skpw,48,52 succeb);
esc->//ATOMI	priv-avgnetwot usb_dev
#incretursnrXfalseevmoces9xusb(returork.qae8021d by Ratr_vb-tl) >k.qos_d dev)WDESCp92_Quer//false;; i<aitQ[queue_index])locdev, stru "	herw 0x0ateesc->9xusb(tx_h8192k_buff* _1M;	//	max=WLrx_sqry
shor(priv_ATOMI->bssid sil	MPDxif (pET_TYPE_NORl(&pri"
		"TX VO ev, 	"RX	    eINKED)
 ieeee802EN + frag, ouef5;
				}
	icess net_device *dev)
only non_zero  off_t lockHz){
		raOTAction &riv->i5;
				}
	e d>ieeeudgeFWsize /wr92_init_.ff *skb = neralmply jable fit use Fd by ithm;

	prt_mol;
	*, sizv->sdev,_geteaconv80211->curretx_queacon_interval = DEev,shortET_TYPE_NOR;//RXhar _IprivNIEUE_(pDrvn ", (p211->current_net= WIRELacon_interval = DEFAULT_TMAC_ = N 1 */
		//tx_RESET) {
	lse  if(!i= NULL,E
		iIEEESOFUE |
		ITMAEEE_SOFTMf ((w11->curr,y non_zero 8192_prik_list,r lenonHwamy 211->4 //|0000UE |v);
	OBERSIn_interval = DEUE;

	priv->ieee802SOFTMACive_
    HWDESC_THRESHOLD_MASK;
face akb = stivpriv(dT_TYPE_NORE_INF))MBps:%d\nFF_PROmpev, usb_sqos_dato_sInetwork.rd(deers-c", S_;
			 caue  S(dev)nfo->s,
};
kb, urb->as_priv(_networi	1_MODULAT, cou(dev Tmp |=topriv->ie == < MSva			       RTL(1)ad_nic_dweee->pHTInfo->	raieee8& ~MSR_meAee80211eRl-rb)(2) par, ArateN_CA = Wcaclu bMaskDW	default:(es[i]NOREgiste(chvic_dwOMIC);
VE		brpriv->ieset;
	}
esstimeATOMIC0211->or ofCckon_wq= &iMGNT      )ing[B RTLevice*->	privgcnnec#####c_desc-ool bToSe_isr(s>>)
{
	srate = (isr(ste = rate_cACE(COMP_ERR, "%s(mode;{
		(t_MODE_G))
		{
x;
		b: It i
         -38 , -26op;
14riv-iv =gs;
	cb_de	cb_d1->das5eee80_rem->
//	6X_URB)et, e paee8021			privrtsach_d5 - (atus = rtl8192SU_qset_chan3ord(dev should be du48+(Fprs-d\ndLD_MASK;
	_getwm23))
	_)[3]=ted = _THRESHOLD_MASK;
fULT_RETRAULer tAGeee8021******priv->ieee80211-11heck_nic_enough_desc = check_nic_enough_desc;
	priv->ieee80slot_time);
	}e802118heck_nic_enough_desc = check_nic_e//6->8nough_desc;
	pri;
		}
		el= pr;
//	bool bToSenesc = (->cfosh
}
s#####6uct r8192_prusb_fi&& (!5ume = ff *skb,sl8192SU_resume_THRESH slot_time);
	}e80211-m]))
	k_nic_enough_desc = check_ni dri1)        desc;
	priv->ieee8OLf &~am_count;
			priv- 			->    stru8		bMa rtl8192_hanhw_wakeempordesc-80211->sta_wake_up = tcb_desoo_PARTX_PACKE192_hw_wakeup;
//	priv->ieee80211->		bMt->bssid LPS0211->staa	Revaly WBrite_nix_li; i<=211->enter_sleep_state = rtl8192_hw_ net_-2 *devSw    80211->sta_hw_t; i<=MG_rate(dev, age stored while nic i80211->iniflagOMP_REC);

INGLE_QUEUleeMode(sMP_Q211_CCK_MODULAeSupportByA_IsWireless
	{
return fcv);
	stT_DISA->ieee80211;
	Us += soc_response;
	prSet80211->iniop_t_ne_hw_to_3)VU_dmiv);
#eendif

a(EVMoc_resped tocilable.
	priv->ieeeee8021120) sq;
			bre(ieee->Regdot11HTOp>trylim(16 + 8*Fsb;
uct r8usb_conU = 0oc_responsatus = rtlsq>iee;

voidte<<802.11g ugh_des > 6val *k.qos_daSR+2, r
        
	 	BcnTrtl8192_S<hort t_count;
			priv->dev);
//	u3; i<=((64-sq
		c<rb)
/ 4, (u16gTmp) = rtl81	priv6itialGais211;)
{
	st	/* eMidcan PwrTHR_L1mpty

voiwork.bv->ieee80211->modulation _80211_CCK_MODULATION | IEEE80211_O	idx_priv0x4>dir_don and pbI);
booltratrconfig |
};
CCKry("reA*intQUEUE |s,
};

			ry("re,1,netwk it temp
	2_QuCCK_MODULAT= rtl8192_setHTa_sleprad_entr/ite_eee80p;
	_encdord(teRX(deven>0).qos_daice *d	rxc//ampduddevicec_en?1:0 be referred fine //debuetwaree80211et_deviamy
	priv-s , DBGp;//-by (0)
		i92_qosIOate_confvalue = 0;
		eak;
5;e \n")GI20MHz){
	64},21}v->EarlyRxMANAGED<< =80211_priv>ieeo(dev,;
v = ieelOMP_QOS,3 (st* if_NONE;
			 RFworken>0)		bMaf92/%s/registeess? 92S only
	p	switcps_lov->IrpnableCP}
			elsN_5G;ruct net_dc uv->namizeof(struct iATate_cap(dev, net->capability);
d by thetx_deto tFSET) |	// ShmodeSupBySecCfg8192lot_time);
	}d_resume = t//HWlags);
	d_resume tl822es P;

	if(CS ratepEEE80= rtltrswsferS_XMGN_1M3F)*2)11_p0)
{
	sE: SWk;
	ivedRCR_PLCP legRetr>ShoLimit o success;1rate_conus;
	struc  DBtf(pa     EL_LIsSetti _te_nlen;
  priv- ieee->wpa_ie_len;
      ;
ad_phom speciV    ev, rtl8192_e_rx_ee8net_device *dev,u8 RFvoid w%d RXPWR=%xll, 2=ruct neiiv_wq, & 	//a(skb, dT, TUR0;

	x sn i<=MGNx_quDWEP_ETX fiASK;
	r=	leepm		priv__FUN
bool, priv-
			 c m ad
	}
)or(n __FUN
			    k weiv->/     CRC32ncryption, no SNRdB	//priv(tx_des
	}
	 if (os_av *priv,	}
		dOFFSETIGN 7<<->CSM:
	 d __FUN[i]/		priv/T_RXFAPv,BS//
#inpTemp+ 6) / N/(page0+ MAXFIFO__FIFO_F |riv->iPFCS|FIFO_O	(priv->E
      A Acce	if iv->PS-_REC  falmi = u
rtl8192	privI|= 0t ieebD/rtl81N  --lResld<<T_RXool bToSend0Byteariable_(COMP_QOen>crypt[ieee->tx_keyidx];
	//1,2,ue))riv->Ea5.0	else
	{MIT_AP_ADHOC;
  	}
	(priv->D = rT_RXFB) {
	//		>|RCR_A4tl8192_SetWirelessM0211->haassoc_response = rtl8192_handoc_response rtl8192_Setgs;
	cb_desc rtl8192_SetWirelessME: HW provie PLCPnetwecmd >11LENGEXT, URnfig	=_ie_len (rt ICV/CRC e>wpa_ie_l= (cb_ ruct 7f)tl<r)
{
	if (Rlimit;

	VODCSMethod Iylimit;

	BEMGNTDOK | IKylimi		\>wpa_ie_len;
  CF |RCR_APPFCS|						//accept co;

	ifeee->eee->Regdot11HTOpe1 = 0x3B;
	priv-0 |*/ IMR_RDU | IMR_RX81T0 |*/	if (essModeWRXFOVW	|			\
				k->fls
	pri//11->ps__B))
 = 0x0;
		t *info;
ilin	priv->AcmCoE_QUEetwork.be sizof>namate_conx40;

	te<<CSMethod RxHif 1;
	priFirmwarhwwe>=;
	priv->ieee80 NULL;_TBDOK     (tl<rFirm<at r8egation re1S_MODEdev,  u3reles)      DESb_deMCS
	st, 0

void rtl8t_uct ue QoS
* tx_fwnd rnf=r if (priv =ng tSY_RTS;
	priv->retr2(str }

	rrenF007;get->stresc->T )) {_ne<TCR_SRL>data;tTimeTimer = 9;1allo2)(skbUEse foment/d 1esc->n in8 MRe RF until  i<Y_RTS;
	priv->retrheadMproc_2048<<e_uc_en
	iv->EICV/CRC>enartl8    sx DMA Burs? 9APP_PHYST_STAFF evn_stop11->cuo(skb);
		th a	      "l cur#end"l PubMAX>>= 1"r todth anerr,
mpilo thiasablebuilITY]vironf* skskb_quK | Imt ne192_e IEEgh a	"Rngs.truc"));

	doeNoritsc->lockk.qos_datcated ate_	int lea& HTa07050skb_quet retrymaosi-----o   irveeSelectbmiv->iee(ct r81ded i
			i N_DBb = De802(&)d rt_hw_t"ssht.RTanymor"Una{
ral PubMAXF |	 adrzbming SoqMode(s                    c(stru( or wil = 0, B	sema=	//pl(&priv-w  ifm,e)
{
e Qur Rx DMA Burfferi] /*/ 2*/)ol frtruct.07.07,ware_tx_belen; XEUE_B3|RCXEVM_wat     sruct net&priv->skb_,emset(priv-
	mutex_ we ", nic_dwoskb_q>ieee80211->st	=TIME, slo Rx DMA ION;				{e");}lgth and Lniv *,eee80yreg.hd pa92_hwork)
GNU G2)(s       ;

	if(>MidHighPwrTHR_L//tx_dieee DRV_NAME "wlan0"
staue))<RCRf######_NONDHOC;
		priv->LongRetryLimit =        sHandler = GetHTION | IEEE80211_OF     de == true)
		Reval IFO_OFFSETPate<<12)|_stopwork.beacmpty( to
de(sprUEUEta = DEFAULT_R_wq b_aggQ [i]);oa = DEFAULT_RETRetur
	 =	E_QUEEEE8021 mode supported, Sode s? 9SETTR);(stru	****_WORK, "Qoratr_valu relawBWonf |tx;//

      b_queue_heived\>watcDELAYED_WOsettinelayed_we to id_bwR_AI[1+>ps_lock);
	sc]nableCtatus);
  2     (struct x_t r81       0x900;urn 0****(pag->tev, usUte(dw Likk;
	cao->IOTActio(e = privment/dwSec)
	ieuero 8;
		u32et net_0~ bulk //I i<=Musb_dek;
			restog_wq,v);
neee8along    ata.paOrP,7,8Rsp(o;

tasktl8192_SetWirelesne dev,d\n" "wRCR_AOnlyatic void rtl8192_init_ev)
	//	POC0211->e ION__Lb, rEEP_T50reg.strucSize pU | IMR_)(pri stapriv);

#ifde      const structSB_ng responses);
	struct ieePOCTED_WOR    tl819xusd_tbl,	    >ieee8rtl81);
	strperaM_wq, Ind
			bre		/->TransmitCUG_D(prirtl81S //Bilintl819xusatch_dog_<63; i+tx_fwinMa_lockrstueue_= rtl8memsr T      {hortReP!_bulk_ur if we go_pipct r8192_prs. if we goee802_dog_wq, rtl819dog_wq,f_patiable// Se, c= 0;
192_int *eoG6*)(p->ieODUL. only nw				"%}
}tl)&& (rb-t1_de> MSECS(iv *p	ers;
   _ratev,5;
		rxrite_ess.h, 4,,22iables here. only non_zero va	psrat aS	//RT_el
					( == NULL) ||ga frab->cbaseff *sc_worddonot considenetwork->qo= returne = network->qive_tmp = (e8021rp, .support_DEBUG_DATA(COMP(dev);
	Re_probe_response(pp_wq);

_dowpriv *)ieep_wq);
ktotal+has bek,fleue_i;

}

/*
* handling the bL != (skb =192_(f(dev))aitQ[queue_index])y non_zero valu
void r)ieee802!= TXCMD_QUEUE(tl<rb)

		queue_delo slice_i211->striEFAUsble.
	stingunon_us);
OTActi->force_resd rtl8192_init_	priv->ieee(struct sk_AL;
	priv->9346CR_donot consider set supteSet, 16);
;-ap\n",
		               Rx);
	struufof(tx_fwiINIT_D      ||(pai     priv-", EPROM_CMD, curCR);
	//wheth1->Lwaer need I consider BIT5?

}

/*
* handling the beaco struct iriv->epromtype = (cur}

voi/*schedule>datatry IE WIRmit  struct
	elseErlP bToSendce *dev)
{
. N&& nP_ATO_desc->rtsBW s,
};*ointer>stats.txborted = short rUs.rxst192_, QueueID);
			break;
	t(priv- 0 by = jiffi_priv *priv,
                                    struct ieee8t_TRACE(COMP_EPROM, "<====tFwCmd8192S;
	pieed\n", __FUNCTION___confi,C_nageokS0] == ICE(oc_response;
	 priv->ieee80211->cx_op_limi
	u16 criv->ieee80211->hw_LINK_NONE);
	unOM, "reas/registe16 fc,	INI187 draiv_wq, &pl8192_hwaiErlPU_UsbRX herrESC9FFSETR);tGI_raase MGU == ) You *L_RETv->iee//#endifruct short_snprintf	mdef (ntGI_rat , T},13lity);
->NobO       {???
				ratr>qos_ace ->short_// +    UE |
init_1);
	statS
	priv->t r819U eprock,Try to;
	//{

		len += snQe %x"%	}
	*******fnt_netw    ,rtl819__Fct iR,0,0} /nTRY_CODnt_neFCetwore->d(f	prive;

	iintf(__Ff (n_buff**>tl8192_APM |	ngcallba (i=0; i<=Mpriv->abshold<<E(COMP_EPROM, "<===MR_HCdesc->bRTSFulatioTL     UE |	priee80211_pic_bytf, voee80n);
	kb a((u3q  				/, ratr_value);
	wriv *)ieee80211_priv(dion:fc
	//rt =tl8192nt_nTODS)?rt_gl obal_ :amet1tl81i,
 * You	prileepDS s	{{1ddr[6] 2 :acAddr[6] 3)struct delaC_8192SU,TION__, priv->epr/long !11->curreHwpriv-ForAt	0;
		slot_CRC)*******v->ie CICVse 0:%s(), epromtypd


distribut.
/n data
r& 1; T_TRACE(COMev,shnctionality
*/FFSET)11,12,13},1zero = NUiate riv->iebyte
	int i,F>DIS     :
ueue2outpipe(pSters lags);)
//		priulation = IE = Nend = atoTXTIMEOUT");= WIREL2,to_sare is avaer()BEACONINTATOMIC)RS | IEuse FW read("enseAdaIEEE_Sof(work, ll be u	idxl, bute80211->staGNT_QUEUE:
			Qevice*    _TRACE(f_type = Foundation.
 *
 *istributed in t>bRFSidulation = I	casd &~ \BILITY orHOC<<*eoAckERT
	/* rxt_global_CH		|
//grega#incSEEPROMersi* Caet_determs o)f_path = 0;ndctrlpipe6	TemeiveConfig	=	//priv->ain;AL_RETRY_LIMIT_(COMP_EPROM,);
  og_wq, ->LongR//alwpriv-> BE prio 
#define;
}

//
//priv->****et_ruff *skP       (MAXI20MHz)))
	{
		u      struct iee(nitia stapriv queu We dB < MSECph;
	}v(dev);
	s0)?1:0;v(dev)ce* i#d211->ToterfaortMo *prU_Us		}
	80211_prt_dat4M;	br       D: e_pa      , __ty & WLAN_CAPABIL		{
#i
		{      struct iee_subuESC9uct  >11ieters, size);
 = usb_su,ieeeion #

/*
* handling theChanne
	//whe_SIZE;work, r8192taticeeprom_S

	ReE(COMP_EPROM, "<====%s(), epromtype 1 */
		//tx_flse if(!ierelatsc->NoEprom_)word(dev, 	R

tyaskalways o= %d\n", priv->ion relat*
//	write_nicated segcallbaFirmwrb)
CS5: drict ieeeUNCTION__);ice TXTIMEOUT");ob4692_rx)
E(COM					d\n"		netwo*_desueueID);
			break;
	 *===========%s(	  i_read__size(dev,d\n"evice* eLEEPble n
 *iieee80;
			bretl819n += sn []different QoSreak;
	
*ppinted p_raunt 
rms  rtlbPhyParam, 5);

	{
iNNEL_LI)
M, 0eak;
	caseDreless_mode;

}
	|    	COMP_U_dm.h"
//#inc See  USunlock4
	prTX   spin_:
 * Jerrisstats.txbeaconte = net->rates[i]&0xt ieee8*/
	{     cvte_ni/%s/r//0:C92S_R, 1:);
	2:which3:ICVral Pub6; i2
	}
_buff//NicIFSepi++)ble_guart bteri]))	//E1:
}

ct ifrID = /GI, 0	"TX/*ee =zeof(str_ISO_CTist_for_eachr itv);

	int>bLastI
     _add_OFFSEpr= 0;
	priv-Fte);

u16 CompIDR4,l_msg(uist_for_eachproc_P(u32*)d) other 
	 erID = ->8,9,10entAdd2_priega * rtsof ve(bSup,22},,8,9,10	cas0};

	Ac= %7,8,%0 GenIDR0,%2x\n", pr_byte(pM, "read frEQ_SET_R& n<=max  	//ETS48+(Fee8021_wake
	{{1_addwrite ], 0x005e4vu16*r[4]NUM; i//ETSMBoar5]))) ||ardType;
	pri{3,3,3,3winfPROM_DefaulRY_Lard			 ED_WORK(&rfG)) ?9led
				{s? 9e initdonot con snpyLZ / 2);se 0:(skb-,  	//ETSMBoardT_ADD[tDiff = EEPROMe_beT2R
	priv->EEPROMTHIGN SDiff = EEPROM_, &SI:
	priv->et_regTxype = +PROM_DefaulNETWstalCap;
	priv->EEPq_rx_PwrB//fo=get_inue;
			priv->Eel_plTyd by iv->EEPROMTSSructT2R
	priv->EEPROMT(struciv->EEPROMTSS))
	T2R
	priv->EEPROMT:q_rx_et_reg_D004-200]==(EST
 M_Default_T_devict_TxPwrTkMode;1->RAt;

	rfdump tri11rb-tl) thtl819puee8021Meter = EEPROM_DepHTInfo_TxPwrTkMode;2h++)
	{
		f_)[0]
			 T_RETR_TxPwrTkMode;3"           TxPwr[reaTx     TxPwrTkMode;4RfOfdmChnlAreaTxPwr1T[re fraTxPwrTkMode;5RfOfdmChnlAreaTxPwr1T[r, RF_TxPwrTkMode;6RfOfdmChnlAreaTxPwr1T[rfwinfTxPwrTkMode;7RfOfdmChnlAreaTxPwr1T[rse 0 "Una_dwordRT8RfOfdmChnlAreaTxPwr1T[reof(-%d CHan_Area9RfOfdmChnlAreaTxPwr1T[rata.iff = EEPROMR185/rChnlAreaTxPwr[rf	RT_			e
		//RT_TRACth-%d CHan_fPwr[rf_path][0:{
           -("           wr[rf_path][1    struIT_IfOfRT_TRdmlAreaTxPwr1T[1T[r->len
		//RT_TRAC		//RT_TRCOMP_EFUSE), "O/writ
		//RT_TRACFFSEISC)t_TxPwrTkMode;_T_each		//priv->RfOtrylimit;

	General PubMD);[i]);
	}

	// wirelee IEEt use FW struct i
		//RT_TRAC{
  _ADH%x\T_DISApower_trackingcallbacrmanent AGI[rmanent Address = %02x][ev);
	str SW prfunct = WIREicat< 3lt_B/int *hv, SLCHan0			index9)ut thof(Gnel COMP_LE 4-.
	ifOMIC);
	ry IE{
   ap, 0, h>bLastInnel 10-14
			iC_device *dev)
{
	sage storneed I(priv-nl
	str\n",ne#incl     
				 priv->ieee80211->cu>currn+=4,
	}
	Rxefaulub 1~3 n -1;case 0:
			nEndPoh MAC add				RT_TRAC480:		nterface ;
	n
			ratr_value &= 0OUNTRndex];
		privUsb;
	ratr_value &= 0P_POWEdex];
		privnel[i]uct ieee80212/11  In slot_UEUE_need I consid AssSID;

CHan_Area=0; i<14;t r819RF_2T
	priv->epromtype = (curistersU_suspX_40_MHZcount - len,
				"ipportByAPsHalData->ieee8tl819xugram isn<=minclude "USBUG_T(page +n), bMal0x%x1T[ 240:
	retur->bAMcmp_i pa  	COMP

	//
	//_ALLIn[i]92_pX_DRIVER=0;i<####### paOFTMAC			 b), "Rf-%d TxPwr C   	COMP_), "Rf{
  eaTxP CH{
   CK/ HTe
			80211_prix&prie(dev,  1T2R or	u16	rx normal		// Cet_r211_qos_pas_pa mnt rannen;"TX VnlCap;
	pm%32 ieee = priv
		cas80211_pri0xdd) && ********	stru-%d CCK O)[m; i+esrk, struct r81werays ate_confiet_re===	 case M    info
RTL818			snprintf< rbawevicadev);
	BBReg(d     rLiv->SetB= sn04
	pran	devde =priv-ate_conSieeeenna A     nel_ate_co* txquV	//RTS
	retUNIfine;
		priv-BufS);
	s areaPROMMinit    addes= 0;
	priv-EPROM_DefICVToEn->EEP;gQ [EPROM_Def Accstru;
}

//
//	Desc         rr RF|= 0;
	priv- SW_IC2
	pDevice*, (u1!te_confWDecusb_2,3,lMet
		on.
}=======c_ratx_u	/* SeHw/rtl8192_bdevice*acAdd-%d 211_privCRse 0:
		ate_conAtWir=="regisev, usb_sndctrl descri
		/PAGGRsuppode);e or EFe
			acc	init_rater = 0;
	priv->EPROM_DefSPLCffer;
		priv-Isee8021dated 4},2SE.  nel v)
{_byte//	.iv->ieegetchd>EEPROM    EEPROM_ngclude "ieeuct lock,fSE.  ze
			accin_otl810211P_IN
	case Mor(shortGIpriv(dev
			brGGff;/er()PartstatUnable to 8.10.21.
//Cck[0][ize
			H	if );
	rxconf =r310
r = EEPROM /******* need mpval;
	u1,2,)0xfnt l Anturren to in/ Twr[rda   priv->dir_dev, prresc->rtsh + 6)  rate)
&= 0x00000FF0;
	lic License for
 *set init	reaTxPwor(ns OKt the paU_Read/0x00};Aull GV0.6n & HT_IS_e sho:11N_AE,(page);

	ampd;
	pr219          A, "rea EEPROM_Uee->pterIpriv);
iv(d AE11HT16xe = R      false HWs_paramMer iwrit#ev);

I_Wor8.11. Publ* pMcsRatee IEl doma:%s()\0~3 foru8		ernk_change(
//
//	A	//R = rtl*100;
	prCck[0][i]atr_valA, prELAY*ted:(hwwe& HT_0preturn0, 0xEU_Re dedit{
	in != Cart_SE;
			brEMCS3;f g(dev(BOOLEAN) bMa txqn,
	RXHTil16)(e// S (VE
	//
	/92S)((2 reg = _quete responthe i***********xF);
	RT_t_regword(det->ops= EEPROM_2_set_m1;
	strrd_8192_versio, bit4~rsupp_version);
	ce or EPAGGR);
	else
		stats->rate = MGN_1M;

	//**** Collect Rx ****/AMPDU/TSFL*******
UpdateRxdRateHistogramStatistics8192S(Adapter, pRfd***

 ****pyri***
 ) 2008 - 2010 Realtek Corporation. All rights reservPktTimeStamper for RTL8192U
 *
 * Bs resereceiveght(c* Linux device driver 0(dev,****** * Based on thed.hts Linux device driver forscali.it>, e	//FIXLZM* Bas Linhe r8187oftware,tiu can redistt*****
 **Get PHYdevicus and RSVD parts.ion 2<Roger_Notes> It only appears on last aggregated packetcen****if (desc->PHYGNU Ge)
	{
		//ftware_info = (rx_drvat i_819x_usb *)(skb->data + RX_DESC_SIZE +n redi->RxBufShif vers	hope thuseft will be useful, but WITHOUThts ANY WAsizeofNTABILscr
 * FITN) + \
		version 2he implied warrif(0)
	in th	int m = 0or
 	printk("=ve 04-2005d a copy of t\n"for
 should haRRANTY; with:%d, c License along wDrvInfoSize:%d\n",enera	lic License an redilic LiThis for
e Founda8 - ; if noibutenfor(m=0; m<32; m++)etail 2110, eral Pub%2x ",((u8*)anty of MER)[m]eet, F}t, Feral Pub\ne received a copy of thhe ribu
This
file //YJ,add,090107
	skb_pull FOR,CULAR PURPOSE.  Sey ofe G * Bas LICENSEprog
,end version 2of tTotal offsetof tit>, Frame Bodyhis prog((,
 * 51 F8192, Inout eveetaieranklin Stre > ore etai*****
 bdef DU= 1or
 edae@realtek.co *****ntact m; irmDUMP_RX
#un_PASS_MP_T#undef DUMEBUG_TX_Dundef DEBUG_ZEPROM
m>hts calledinclude <in the
 e is includundation, Inashts pRX_Fshed byFRAG
Free Softc.,
 * 51 F8192UROM
#u TEST
#undram is distribuon.
detairtler foU_TransleservSignalStuffG_RX_VERBOS,  Jer, ndef DEBUG_R;ndef}

//
// Descrip8192:DEBU	The strarting addressc.h>wireldefilan headerHANTABsPASS_1 or 2<asm3uacc"more" bytes; yoFRAG
followANTYreason undef (1) QoS control :include2*/
# "3cx6.h2) Mesh NetworkG_RX */
# uacceM */
# "r8193)EBUG_ty oklin occupies 2U.h"
rontundatio.h>Rx Pghts s buffer(M */
#units
#unin 8B.h"
)ef DEBU 	It*/
#because Lextra CPUITY DESC
8186uacc865x serM */assert exceALLOC ifde "rING
ERO_ESC

#def
//	of IP92_IO_MAis not double word alignment.x/usdef Dfeature */
#supporUG_IR
#in8xbis innderSC
#u * FI
#inc_ludeee8ee80paef Lter: PRT_RFD, Pointer"
#ine4-2005 f   	* JerX_ALor which25.h"
itializd a ccordcludtoee80|
		110, RxUG_ZERO_RECee80return value: unsignglobat,  numb	COMP_tm is/M */
#OM */
# "			COTX_FI: Linu/06/28, creBUG_Iby DEBUG		COu32ude Rx
				Cense k Co_ef DUM(struct iRACE0211_rx_ING
#  *ribute, bool bIsRxAggrSub|x/us)
{ef DMP_DBG	_STATUS	pRtRfd#undef = &
 *
->ribute
#unMPBUG_R(mation:
 * Jerry chuang undeundefndef DEBUG_ZERO
			ATE	MP_SELicense for}

void_93cx6. DEBrx_nomal	C	|
 		sk_		CO* skb OMP_undef DC	|
 MERC* MERCHANMP_FIRMT		P_LED	|
//	AMS)FOR Acb;
OMP_FIRMnet_free so*dev=usef->devD	|
//	DOWrP_LEDpriv *ys opLED	|
//		C/alwan erre)	|
//	EFUSn erou cKLETMP_FIRML_CAM_ENTREOMP_FIRMP_FIRM=undef.ONTEal
#unte t.noison 2-98 rtl8*****
 dtbl[ "r8k */.mac_timon 2jiffnclubl[]freq = IEEEM_ENTR24GHZ_BAND,
	}		COMP_rx_pkt_len
#unYou CAM_CONTENT_COUNhdr_1e <l *CE(0x07aa, 0x = NULL		COMP_Dunicast_RQ
#un = falnclu/

#eral Pub*B_DEVICE(FOR Aega */, writCONTf6, 0 * Ba* 20				ek Cps-poll *undef DU* EnGen >=(20TIrmation:
 * Jerry chuang )) && 01)},
	/* <ARRAURBense )) { * f/* firsS_ph05E)}m */
	
#incCardainP_POdef DEBU2xU_#includ0, 0C#undef DEBquery_rx JerrING
usG_RX_V&INGundeZinwSE
#und* TODO	{}
/* B5ahardc.,
 reEGISP_SWOMP_}
};n er->ING
#.rxokPOWER++;  nfae@testltek.c8_TABLE(ProcG_RTe "r8unde04-2viedbl);
Mdef DrimG_RX_VE	/* Zinw- 4/*sCrcLng*/);EBUG_ERCH
};
/****dati */wlan%d";or
 */* Belkin"r81	{OMP_FIRML_CAM_ENTR 0x0043	/* CMD	|
 PARor
 x050dn */890)},* ZinwSit
#unmis_broadam(ifendef_e <l( sUG_TX
#usec->e <l1EVICE	drivDEVI5a57it>, GO|S_Imultram(ifmodule_param(hwseqnum,int, S_IRUGm(hwWUS****_IRUGO|S_nnels,*0param(ix0290)},ic ch|S_IWUSifname, chatruam(chcalldef !NTENT_COUNT (ODULE_m(hwseqnu,E_RINSION("Uram(hwWdev_kfree_def any,
	/E
#un}  Try to UC	|
ODULEludeRIPTnum,"am is ftware; yoe driverailsf(param(ifname, Try to bers. Zero=defa 
			

MMODUL+= Zinw use hwtion, In, wla//upUSB_Dirs pkt,OMP_Icl				Cextis incNYIndef*****
 m(hwity support. "urberr++m(cheral Pubactualdatigtht31)},
	/)},
	/*ius ITY  usb, rtl8802.1192_IO_}
y
staticcardsdexusb_p2 USB _");
   	fname, (OMP_ERR; NN	|
//				CORE,ne CAM_CONTENT_COUNT 8

*****pING
#
	OMP_//ICE(00bsb, rfd=rp, S, bqueueLl, bU_709)8* OMP_FI    16 */
id __ddati=
	{sk foEVI/#define TOTn err fL_struENTRY 32<linuinE(0x8," T.dex */
	{e secur.TID */
	{U//u16s in
modghts YouOMP_X_TS_RECORD	p|
//	e	= rtinclude <lRACKING);
MO"
#iStZERO_ude <linuin*id);
st172 cha.
			6.09.P_QOby EmilTEST
 rt_fn  by amy 0805wep,192_dr->virface    */
	.+= get_rE_PARM__M */
);
MOr
 * Fsb(e		= rfine CAM_C= e  = bl[]192U_resu intd onume     hw. rtl8192U_PARM_*intf,;
#ifdef ZerVrived f secabUMP_HCTt rtl! RTL819->bInHctTesX_DEdCountRxErrevice drivr RTL8192U
 *
 * #end


svin;
tl819ENRTL8_PSULE_ 	rtl81_resad	|
//ps func90x81pn fu1d.h
		     _N("LiOMP_HE rtevicam(ch// When RF				off, we* Zi/92SUable	untWiFi _PARM_D_reshw/sw synchroniz sk_//bal_de0, ie.192Sre mayBILIa dur(ifna1whcall	|
/witstaticchangOMP_nd hw		COMPOMP_RE, stbeude "/			sk2U_susp12.04OMP_Hshien_link_.
		mhat i(stHalFunc.GetHwRegHandlerr RTL8192UHW_VARff* 				C, ll G )(&92U_res <wlaBUG_Rl8192SU_== eRfOffevic MA 0      et int		|
				COty support. "C	|
g	COM|
//atiodev, sttic 
	RmMonitorRSundef ouldf,k CoU_tx	|
//			N	|
				Cecuri07/01/16 MH Add RX comm92S__PARM_Dh
	.ni 2U_r.n   (
	{UIC_813/01,
	.Wevicve to_resease RFD_92S_       if rxpecirtl8rmdx_cm92sure_rxdda, 0x8192SE	|_read_e
//			usu can  on t_RX192xU
skbd = r}
P_TXAGfrSW_CRC_CHECK
	SwCrcCheck(
	.nic_typid _
			e @realt("_cmd(chaGPLonneLE_PARP_FIRMWART 8
nelsd_RIN rtl8192	= rware;rtl819x_romal,r = {
CAN	|
//				COMP_DUN	|
//				COSCAce *static int ff;


atic//_interface *intf);

staSETN	|
//			SU_ICENSE(192_usb_id_tbl,	   lagSoft  */
	.susTOTA       /* PCI_ID table
 * Jerry chuang

stscCHANTABIST;Subframe)CHffdev, S_IRUin =ABILITY or
 * FITNE *anty of MERitecominclof t skb);_rxdesc klinrm(ifna****
 tl8192U_resume,        =/				 {
	{{1,2,3,eING
#E
Ls rtl =tic i 8192SUIOMP_H8,9,	SE		COatic intecuriUSBBOSE
#undef DUMM0,44,48,52,56      fn8192w. s8,9,103,4,5,-scrclng6,7,8,9,10frag,4,5,6,7,8,9,10,{1,2,3,4,5,6
} = {Spain. Cvmalloc,  	//ETSI
	{nN("Li,11,_UL
#uxdesc_statusRXn   	|
cmd *dev)ruct ****rtl818,9,1019x_query_rxdesc_statusS,
	.rtl819x_query_rxdesc_status = rtl8192_interface *intf);

sta8,9,

typedef omal DEVICE(_c struct usb_driver rtl8192ic = rink_changree s_i* Rea819x_chanid_tbl[]gain nst  driver 0,44,48,DEVICE(00x0bdin */819x charle	= rtl8},22},			// Fo70version 2*x_a				COph hange tto d,11,minS_IW t11/dis aOM  id carddef DEBUG(charp, Zinw>=,52,56,60,64},22}2001n */3,
	/* Zinw1,12e=1740,52,56,60 )//&&ee so(pHalData->SwChnlInProg0,11},= FALSEo,
	4,5,DEBU9,10,11,12*				C(,10,1//FCal(sRxntf);

staC;
vo_mal,SU_aoratio_start rtl819x, bulkb,modul" U_txx_c},11} //to_tx_don>
 *rtl81rtl8es0, UUSA_rxdDULEN,10,to ET, bumal,cha9sb_dis192SU,2,3sonnel819ys o* priit,4,5,6,7,8,9,disconnect	|
//			usbfraan=-1;
	sael.
	{{1,2,3,*id);,13},13}v/			 rtl81ini//RTIM */
TailListWithCnt(&pid 	InitiRfdIdleQswit,IPW61,16 COUODE_id 	InitiNumrive *
 * B_FRAN_ASSERT(
		c/* P_Case ISRAEL:EBUGODE_TELEC:
	Rfd, ("HalUsbInComal =Complete_RXDESsb():RY_CODE_MIC:
	case C(%d)	 CarCODE_TELEC:
		case CO
	,	//IRT_sconE(|
//	.diV, ff* LOUD2,13Dot11d_Init(	COM****			COM->bGP_SWlNOT enough Resources!! BufLenUstribac,
	/* S(priv8256turanels,ipef Dext->hiptus(RF_ally 8225 & 8256 rf c{
		ondef DEBUG_ZRe#inc56,6INuct usXT sinceff *had finishG_IRsb_dis|
//	heion 24,36,40in	in th		supportdef DEB//      R         2))
	EC:
		TEL, 605}
			o60,64,149,2Issue ano819x_bulk IN t		elfe_houl", __FUNCTIInMpduograChannelPipeIrobrtl81Cu32 rtt B,G,24N mode"<---              if ((priv->rf_chip =FRAGk.coI.
	{{1,2,3,4,_irqratitaskletannel[32];
	u8	Len;
1_rxl_ga110, UU92su*dev2,13,14,36,4blel8192U_r InitialGain8192S,
	;
n].Len;i+N	|
		(56,6 !ritykb,7,8	|
			swit(&1_rx_stkbe@reuex330m(hwS,
	.rtl819x_query_rxdesc_status = rtl8192] < min_] < min_+oid 	r(0,64},out_pipehan)14,36N,12,eproid 	.ChaSU_reifc=-1,3: is iy nnel_map, 0, sizeof(Gn10,1l in-	}
waysoberf_chip 1D_I_plan]	}
	nndef FUSE	|sIrpP|
		,13,*de--d
 * ;//thisop	{{1,l819x1,12,13,36,92_IO_M		break>chaO[i]]      iffunc		GEin set shal(pri}9		caseDE_TELEC:
		GLOBAL_DOnomal =ein funGET_DOT11D_INFOe is i func	e)->bEnablefollow 11d country IEhanntinETSI.rwise,ERCHshall"
#inusb_plt: /**skb);
boeare;  tl8199!		break   if ((priv->rERR, "Unknoset ,= trupriv->.
			Dot11
		[0] && ((a)[5]==(b)[}
	r_device_id *idr(a,b) ( ((a)[0]==(b}O(ieeemapundef

/,60,64},22Gene	_pUG_T theMCStus(n   		CORATE2M ||
			_pDesc->RxMCS == DESC92O(ieee-5_5 ||\
			_pDesc->RxMCS == in fSTUFF_5M ||\
			_pDesc->RxMCS ==
|\
			_pDesc->RxMCS == DESC92S_RATE2M ||\
			_pDesc->RxMCS == DESC922|\
			_ can'LZM Merg
#incm windows		breakSetTELECNFO(Mapping>rf_chip 090319
ETSIic0,11,C_. //ht(ctus(****_5M ||\
			_ _Da,10,11,119x_atic fa|
//s rt192su*devce *intf);

sta,  	//ETSI
	{,7,8,9,10,11,,ver =OID_AD/* PCI_ID tabl//ETSI
	19x_hostP_RE)
{};

 1TA a ableADD_WEP qnum,iendpFIRMe to r8192_V* How] && (a)  i,13},2110/thisep_in_
#unb,3,4,NFRASTRUC->bEE_MODE andmemset/LE_PARRtOutGN_5s,0,16_ADDOID_ADD_/ IetaiIncondi#undefCax_cmiid Cto 6,60,tf->cur_altsetuspeeralMlmeAssoE_MODElay_MAP

#			_sc.bNumEcalled on OI819x(_802_1 i <USA
G_IR     ; ++innel[i called _HIP/ifrporatio- called [i].willAP.er_s,not bcalled , to ulk_in(======== Try to tBILInitia de "[ MlmeAssoTURE_M2,13=============num can not Dndef DEB           CO,	//Iscomve scan,in=       //Ddrtl8ver,conRomalAllE

#de============_IO_MAseque        //Dbommand |=\n");UMP_         bgPould("on OID_s====di==========cyer  dril| (p=======================ex<TOTAL_CAM_E);
#elsqex<TOTAL_CAMBIT31ciate0;
	write_nic_dwo\n");
	ulcommand |=      for(ucIndex folUm(hw can not be rex	switcto/* P	}
	map,0,9ICENSE("d Resety_entTL81c6quence nS BK, BE, VI, VO, HCCA, TXCMD,l_maT, HIGHt_deACONreakn892suueto	}
	[e
  {3, 2, Glob, 4)},
e_nic_dw ETSIex<TOcpy      /* P;ucIrtl8++)      r SC

#, u32ex<TOTALd |= BIBIT3_empty_e

#disca4     wri);
et_dev

}
//				Cte_niccamps = {
	.nic_typevoid 	rtu8d(dev, RWCA ANY)
40,44, wr0     (C

#&0xffiscaliWCAMI, RWCAM;nic_dwordte_nic_dwordordiscaliR    , BIT31|BIT16|dr&0xff) )) );
}> 92 read_cam(struct net_device *dev, u8 addr)
{
        write_nic_dword(dev, RWCAM,{ad_nic_dword(d8, 7, 6, 5;
        return read_nic_dword(dev, 0xa8);
}

void write_nic_byte_{//#incsigle[5]) ead_cam(struct net_device *dev, u8 addr)
{
        write_nic_dword(dev, RWCAM,0 ind|dx|0xfe0{}
}, & ANY8709     return read_nic_dword(dev, 0xa8);
}

void write_nic0;ucIeral Pubve      rhis p    writ bfra     
	}
}

u8 rre
  mpty_entry = {
	.nic_     C = {
	.nic_);
#elspper t!

u8 rs:%         :			COifthiFloo can not b,60,    C; i++e(_pSAIRQ
#und },	//Isrneex<TOTAL_Cindatide " */
#d	// S tacase	//Isrr, RWCAM, opeys op=evicesb_co819x_pri);
#elsOMP_EFUSys oisca****,	//Israel.
void writde usbys o->udev****
	stru =_chan        write_nic_dwot i, (struct net_device *dev, u8 addr)
{
        wri:DM	|
		, usb_rcvctrl9		       RTL8187_REQ_GET_REGS,        write_nic_dwoEQT_REA, 0x	EP, OID_ind, buruct 
amReset1const2 ul_reaeralllow
 _opsn[ructnelntic e n8192_COD.ndo_d_tb		=below.
voc_by,yte	|
/stopN	|
				COMPclose OUNTinM_em ETSI.N	|
				COMPION("Vint indtx,60,6outN	|
pipe(udevv	|
//			do_ioctoid 
				COMPrcvctatus;

	xetwesume_paralist usb(udev,	       indx_READ,
			   ac,9,10,11}chanCard EE_mipe(ur	|
//			C
	{{ate      = rthEata;GS, RTL81	|
//			link_c_mtut usREQTx, &data, v = priv-,art_xm ope)il.
	{{1, can'tENTRus <,
 ETS*******stru_ funcniabelow.
vo6,7,8,9beN	|
				COMP_RE STA asso
	u3MA 02to 16, sozeof19x_U_rxti			C*idplanin11M )

//	is prio= 0x38,52,56,60,64el m  	//ETSI
	{,656,60,6c_by_CHANNEL_LIST
{
	u8eee80211_pudev, a8);
 *udev*uuct r80,
      rtl8192dev(t s	u8 dp[OT11D_nnel_map, 0, sINIT, "Oops: i'mx_re||\
    induct r8,13}, priv *)ieeRTL8187v *priv = (struct    
	19x_rol_ pri PAREAD,
 rtl
	.suSET_NETDEVtic SU_txx& prit d / 2);c WEP, OID Cam_KEY<asm0)
    LE_PAR_PARMfn   =
	strue 			       RTL8187_ ind= ind87_R	{{1				CCam      for(ucIps = {
	.U has e / 2)e      \n");e received a co>NICmpon)[1]				COM_EPR;
}
    elow.
vsunic_.Cha(ud->ice *dev,_dwoQ_GET_REvice *dev,n		       in    MESG(msg(    ,0),
	sndctrlpidev;

	sudev,CONFIG_R_ it wrr1},11 ulcomm_w),
			  _l819*) &
			  w8192_Card  = p*)ieee802type=ARPHRD_ETHER*)ieee802watchdogpe(ude = HZ*3 DEBmodifbfraby john, priv-r l_IWU v_ 0),
	et i 2);

if< 0))  {
 >bEn< min_      indx|0xfe00, 0		        usb_ddev {
  ce *ady taken! Tryfn  set 0%d...)
{

	in _word = "net_dd"rael.
	{dx|0xf {
 nic_dwordprivD,
			    callrite_nic_dword TimeOu
#inc2chanbeusb_priv-d1ctrlpi asso to E        {niDE_ET)!l Re01)}((_p3esc->e *u_deviI
//			ND// Forfaile  	COMPgotoe00,  14;
 usb_devnetif_cr
 *er_off */
	.supriv *x, u > max */
	.s extgs = rvice *d */
	.surite_nic_dword TimeOuI
	{ {
  = priv- %s)},
udev, u ANY;},
	nMP_Shan 7_REQ_onWRITE,
		usus = usb_copriv *)ieee8021usb_rcvctrlpipe(u)
{

	i       nctiusb:     domaRTL8187RITE,
			vice *udev = priv->unet_BUG_ZIR lo "un_dworusread_nic_dw}-ENODEVpaglled_bufch, 0)192SUiv->ul);
	DEVr2 ulcomur, strlareduacc int ;

	st	map word U7_REQ	strucnom. newterf.
vo mcancel = pendef_iv->1, H_&datRITE,
			*vctrl status:%atuontrovctrnablenelreWRITwqindxb_deviatus 
}



u16 TL8qos_activ92su(udev, usbd//ifword TimOT11D_IITE,
*dev Q_ue;
RITE,
	ta
    HZ / 2DE_F87_R rese_beato 16v, int indx)
{
	u8 dattatus:%d\npriv *)ie->hw_wakeupmal _dwo"

#i8187Outa;
	struct writ;
	struread_nsleeet_d	ret//REGS, RTL8187_REQT_READ,SetBWModeWorkIte	u8EQT_,v = priv-2_pr	u16 < 0)

,/
	.ttk("read_nge from 411M )

_dwoexteata;
}

u16 r
		case C=0;i<ableatus);
        }


us;

	struct rtus;

	struct r819x_turn L8187_REQT_R           11 be rbframe)us < 0)
        {
         to Ecvct{1,2,uny it1, HZta;
}
//as 92
				COMP    READrfac,  write_nitus;
>  write_nii0;//devremoviv = (strus:%d\n", stat(" statatus:%dhoul_ERR80211_pdown          strur&0xfpFirm4,36 sk_buff v ANYnet_RWCAomal _dwostatus:%d\n"ce *dev,c_byte Timata;umrity surfOUNTst OID_ADb_device *u}SetRFPoweshortndx)
{
rcvctrl_changstroyatus atus <iv *pririvead_nic	//->udev;
ap
	disabl  indruc    if (stat &dat  indx|0xm     (10     }n(structet_d
 = (ce *de92U hasfevicv, int ind            tus:%d	return
	st*U_tx wi/ETShe built-iRTL8187DD_K ETSck..SU_tx_xte=-1,ntic int chandebug7_REQ_ault); {et n10,11,Ctk(KERN_WARNect,	stad ULARof t ANY hould hate = %cryptos. Nalt, data);
//	}= %d\eturn****%_GET_REde< 0)
        {
         L8187_REQ_GET_REtki! stord TimeOut! status:%d\n" status)d har ((pri    lteturn rea//	} status:%d\n", statusccm		ifQ_GET_RE==  status)3,14ways o->chanerro//u8     
		}
        }



	return data;
}


wys op(status == -ENODEV) {
			priv->usb_erroevic//u8 read_ph>usb_erte_nic_ude ata;
}

u16 r_IRUGOc_dwor    * status)ret
   4,5,6,tatus = TELEC
	{{EBUG_ex<TOTAL_CAM_EN,36,\n", us:%d\nRTcmd,
eqnum,ineral Puate priv "ation)force_pci_postriv *privalid(dev usb"

#i      nERCHchanrcvv = inl.susatic stru_GET_REGS, Rng addr)
{
        write_2_pret_d13},13},	//ommiOUNTRY_COei, mts *4,5,6,7, *devps = {
	.nic_typ7_RE				C4,5,6,7,_reat ne((priread_ce *dev);
//void     _struct *workre, maxps = {
	.nruct *work);e *dev);
voidu_ERRsk*work);

void watch_incl_rcvctrl*incl work_stdr);
//ad_rq_tx_ack(struct work_struct *work);

void watch_do_channel_malback(				COed is prurn rea
*****-----
***************************_nic_dwor*****************************
   ----------------------------(struct wo_struct *work);

void watch_wo;
//void rtlk;
	 "\nam is kernel        riv RTL, RW badeviWLANNTRY_6,60,READ,
			 proc_    streservanfadreaeepr-_dev,6,40,sil W=(b) {
  crite_nic_dword TimeOu      indfn  ans arnt cstructread_nic_byte TiWatus = Tf dan,149steco			C %d", WIRELESS_EXTord TimeOut! stattruct  willp data;
	intxtend &data,4,5,ael.
	{{1,c_byte		priv       RTL8187_,	//Israel.
	{{1,uct iee//u8 read_80000_privet****OUNTlen funct status:%lis			  &data,atus < 0)
  Exic keread_nMP_EFUiv *)ieee8ieetus:%d11_n
em(hwseqnum,, u8 a  in] && ( net_    write_nic_dwopossir);
{iver =ge + len, colet wasFRAGntf(page + len, count *dev, i*)ieene "ronex_reaon code.._IRQ/
       priic_wor//u8 r	.nic_typt_fo priv->udev;
========       				C2U_refn  *DE_FCC:ta;
	s sta{

		 r8192_priv ,hannet_devo
			25) | willAP/      rANNEL_LIST
{
	u8	Channel[32];
	u8	Len;
}CHANNEL_LIST, *PCHANN
	spin_lock prisavEnableneltxc
}

,fcont-----Petatus
st = c,13,dev,  = {
	.nic_struS status	int sta = dunid *d= trueto>usb *eof, rtl81*r8192_pr	 to E = {
	.nic_e(_pndex<TOTAL_CAM_EN,pag_nic_word_E(strAN	|static En    HWSecuEQ_SConfiy(stru  "non_WPABIT31 status)->bEn< min_u8 SECRus <uev, uxructl8192U_res In static WEP, ol_msg(udev, usb_r;
}CHANNEL_LIST, *PCHANNE2 ulcomm1,12,13L8187v, i*      PMnprintf(d_E(str----ent   /7_REQ SCR_TxEnc=nels  |n ", RxDee0>>8))
		elMP_EFU (((KEY_TYPEL81840},13}tatus{{i(a,b)_key_sg(u) || += snhouldf(p104e +f(pa, oid * -r(i=0\n####"l Doma_nic_word_E(strucauth#inceeof,2 rtl819x###########|\		for(nUseDKstrun<=max;i++	"%2.2x T,AD,
			 ODE_TEL6|(adWUSR)->iwg	for(== IW_MfalsADHOCn++)
	(i=0;i<16 && n<=max;i++ &:   The> ",CCMP |  %2x > ",TKIP)i<16 && n<=max;i++page0|n)");
		}
	}_ude iscal(;
	s0|n)####[5]) }el usb_deRTL8187_REaddunct	p/ 2);

 e<=max;5,6,7cvctrriv-> u can'R);
c. wriv(peer AP|
				CN	staeSC
#und doma# len,
						"%8h"
#ine_aes(f;
	paHT_IOT_ACT_P    Ni=0;ie;
	ica forit)dex=0;(hwsuct *s	;
	s10,11,(pag  %2x > ",n);b,g,,13,+ lmixskU_tx,e0>>8));
		for(n8.8n<=ma=-1,  + le
iv(dee hw2x > ",n);aWBUG_T_dev.7.4len>0 |
	/*sec}



u len,
			 + len, coupHTklin->IOTAmap(u&BBRegen += snprintf, b"\nD:!hdev;)//rt. "	,153f_tdevict_)e *de7_REQ__byte Timoata)}

 totol USB_ EEP+ leec on/of   wrict r8truct r8/vmallo,  fn  ###########&= ~	ifthn=0;n<=max;)
statnnel_map, 0, sSEC,"%s:,usb_rchis pr& n<=max;i+his p##########:%x"====p
				meON__,def DEL8187_Rt i,n,page0(str=0;i<16 && n<=max;i++r *pn");
	en*dev)
			priv->usb_erwte_ni(stat);
M 2);

####, ,  snpri<=max//>8));
		for( 		for******atus sult;
at6 && n<=msetKey(_  "non_WPA\n");
       icnic_=====No %2x > Key if ( %2x 16=0;iT%2.2%2x > *MacA}



unic_D ;

	/Key
			l32 *K****0t#### }

  32 Targetp ==  donprint", (page0>>8len, c && n<=16 us0>>810;

	MP_E8 inet_devlen,"\n2,13n;
}CHANNEL_LIS	int ice *udev = priv->ucam een,"h"
#ied				C601)}n<,11,12 SdumpFRAG
curr######ndx)
{>to;
	in  }

	*eo:%;

	s", (phis p0;i<16 &struct ror(nintf(p*dev ="MAC_FMT    ord(s,len,"\n"=0;i<16 &&
		for(n= }
		ARG(192_pri,
    e +  += snprine(_pruct 
}
s|=te_n15 riv b_rcvc<<2-----
*****+ lenst def Doid *data)
{
	struc len;i<16 &int pr = pre + len, count - l(page0>>8, 2x >npri = <<5i=0;i<16 && n<P_Encrsb_r ; i<r *psuppob_driver	{
			page##page %x#oid  pro+100;

	/* T
 in f*	struct tusnprintf(page +n += s31|= sn6strucram(e00, //MAC|n;

}


	/0;

		)ieecont;
(u32)(ge */
int+
    < 16|]) ? 10;i<16 && n<=maxRUGO<< 24;n<=max;)
			for(\n#structn,
			\n##d car 2);

    re00;

	(page + *dev =###page %x#i<16 && n<Rn,"\
	*eof = 1mal =vctrlpch_entry(taetkey#####=%8priv *psta, u8 status+6= snpD:    wri=0;s    	lei++,1te_nnpriMCS  == ev;

	sl (statdtl818######page %x#i<16 && n<=max2)) 	 |r)
{
        write_8192_pe */
		len += sn;
	int i,n,page03snprin 8tGET_=f) )###########fname */
		len += snprintf(page + leg4	for(n=0;nlen += snprintf(page + len, count - nt - len,
				"\n###5snprintfMP_EPRO0;

	/* T3},13}iv *roc_rintf(page + lentic i		{
			len += snprintf		  int *#######\s_8(cha)ieee80211nnels,iKey Materialp,"    
e + len, c!=56,6uct nst len = 0;
	int i,n,pagage +;
	int ilen, count+i- */
ge %x# *priv age %x##n((privifthipriv-4	  int
	/* n+=4,0;

	/* TinTE1ate)\
			( _DataRate == MGN_1M |(_DataRate)\
			( _DataRate == MGN_1M |_RATE2_5M ||\
			_pDesc->rcvctrl   RT /92_pristubs	  off_t offset, T)  */
	.sus	tx_hal_is_cck_****(



void )
			_(uct 
void CamReset	return le /
uct ieee8021      EBUG_luct ieee802);		{
			l//u8 , &->rf_cn
#incl_us =,_dev