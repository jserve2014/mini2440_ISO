/******************************************************************************
 * Copyright(c) 2008 - 2010 Realtek Corporation. All rights reserved.
 * Linux device driver for RTL8190P / RTL8192E
 *
 * Based on the r8180 driver, which is:
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


#undef LOOP_TEST
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

//#define CONFIG_RTL8192_IO_MAP
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include "r8192E_hw.h"
#include "r8192E.h"
#include "r8190_rtl8256.h" /* RTL8225 Radio frontend */
#include "r8180_93cx6.h"   /* Card EEPROM */
#include "r8192E_wx.h"
#include "r819xE_phy.h" //added by WB 4.30.2008
#include "r819xE_phyreg.h"
#include "r819xE_cmdpkt.h"
#include "r8192E_dm.h"
//#include "r8192xU_phyreg.h"
//#include <linux/usb.h>
// FIXME: check if 2.6.7 is ok

#ifdef CONFIG_PM_RTL
#include "r8192_pm.h"
#endif

#ifdef ENABLE_DOT11D
#include "dot11d.h"
#endif

//set here to open your trace code. //WB
u32 rt_global_debug_component = \
		//		COMP_INIT    	|
			//	COMP_EPROM   	|
		//		COMP_PHY	|
		//		COMP_RF		|
				COMP_FIRMWARE	|
			//	COMP_TRACE	|
		//		COMP_DOWN	|
		//		COMP_SWBW	|
		//		COMP_SEC	|
//				COMP_QOS	|
//				COMP_RATE	|
		//		COMP_RECV	|
		//		COMP_SEND	|
		//		COMP_POWER	|
			//	COMP_EVENTS	|
			//	COMP_RESET	|
			//	COMP_CMDPKT	|
			//	COMP_POWER_TRACKING	|
                        // 	COMP_INTR       |
				COMP_ERR ; //always open err flags on
#ifndef PCI_DEVICE
#define PCI_DEVICE(vend,dev)\
	.vendor=(vend),.device=(dev),\
	.subvendor=PCI_ANY_ID,.subdevice=PCI_ANY_ID
#endif
static struct pci_device_id rtl8192_pci_id_tbl[] __devinitdata = {
#ifdef RTL8190P
	/* Realtek */
	/* Dlink */
	{ PCI_DEVICE(0x10ec, 0x8190) },
	/* Corega */
	{ PCI_DEVICE(0x07aa, 0x0045) },
	{ PCI_DEVICE(0x07aa, 0x0046) },
#else
	/* Realtek */
	{ PCI_DEVICE(0x10ec, 0x8192) },

	/* Corega */
	{ PCI_DEVICE(0x07aa, 0x0044) },
	{ PCI_DEVICE(0x07aa, 0x0047) },
#endif
	{}
};

static char* ifname = "wlan%d";
static int hwwep = 1; //default use hw. set 0 to use software security
static int channels = 0x3fff;

MODULE_LICENSE("GPL");
MODULE_VERSION("V 1.1");
MODULE_DEVICE_TABLE(pci, rtl8192_pci_id_tbl);
//MODULE_AUTHOR("Andrea Merello <andreamrl@tiscali.it>");
MODULE_DESCRIPTION("Linux driver for Realtek RTL819x WiFi cards");


module_param(ifname, charp, S_IRUGO|S_IWUSR );
//module_param(hwseqnum,int, S_IRUGO|S_IWUSR);
module_param(hwwep,int, S_IRUGO|S_IWUSR);
module_param(channels,int, S_IRUGO|S_IWUSR);

MODULE_PARM_DESC(ifname," Net interface name, wlan%d=default");
//MODULE_PARM_DESC(hwseqnum," Try to use hardware 802.11 header sequence numbers. Zero=default");
MODULE_PARM_DESC(hwwep," Try to use hardware WEP support. Still broken and not available on all cards");
MODULE_PARM_DESC(channels," Channel bitmask for specific locales. NYI");

static int __devinit rtl8192_pci_probe(struct pci_dev *pdev,
			 const struct pci_device_id *id);
static void __devexit rtl8192_pci_disconnect(struct pci_dev *pdev);

static struct pci_driver rtl8192_pci_driver = {
	.name		= RTL819xE_MODULE_NAME,	          /* Driver name   */
	.id_table	= rtl8192_pci_id_tbl,	          /* PCI_ID table  */
	.probe		= rtl8192_pci_probe,	          /* probe fn      */
	.remove		= __devexit_p(rtl8192_pci_disconnect),	  /* remove fn     */
#ifdef CONFIG_PM_RTL
	.suspend	= rtl8192E_suspend,	          /* PM suspend fn */
	.resume		= rtl8192E_resume,                 /* PM resume fn  */
#else
	.suspend	= NULL,			          /* PM suspend fn */
	.resume      	= NULL,			          /* PM resume fn  */
#endif
};

#ifdef ENABLE_DOT11D

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
                        if ((priv->rf_chip == RF_8225) || (priv->rf_chip == RF_8256))
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
			GET_DOT11D_INFO(ieee)->bEnabled = 0; //this flag enabled to follow 11d country IE setting, otherwise, it shall follow global domain setting
			Dot11d_Reset(ieee);
			ieee->bGlobalDomain = true;
			break;
		}
		default:
			break;
	}
}
#endif


#define eqMacAddr(a,b) ( ((a)[0]==(b)[0] && (a)[1]==(b)[1] && (a)[2]==(b)[2] && (a)[3]==(b)[3] && (a)[4]==(b)[4] && (a)[5]==(b)[5]) ? 1:0 )
/* 2007/07/25 MH Defien temp tx fw info. */
static TX_FWINFO_T Tmp_TxFwInfo;


#define 	rx_hal_is_cck_rate(_pdrvinfo)\
			(_pdrvinfo->RxRate == DESC90_RATE1M ||\
			_pdrvinfo->RxRate == DESC90_RATE2M ||\
			_pdrvinfo->RxRate == DESC90_RATE5_5M ||\
			_pdrvinfo->RxRate == DESC90_RATE11M) &&\
			!_pdrvinfo->RxHT\


void CamResetAllEntry(struct net_device *dev)
{
	//u8 ucIndex;
	u32 ulcommand = 0;

#if 1
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

////////////////////////////////////////////////////////////
#ifdef CONFIG_RTL8180_IO_MAP

u8 read_nic_byte(struct net_device *dev, int x)
{
        return 0xff&inb(dev->base_addr +x);
}

u32 read_nic_dword(struct net_device *dev, int x)
{
        return inl(dev->base_addr +x);
}

u16 read_nic_word(struct net_device *dev, int x)
{
        return inw(dev->base_addr +x);
}

void write_nic_byte(struct net_device *dev, int x,u8 y)
{
        outb(y&0xff,dev->base_addr +x);
}

void write_nic_word(struct net_device *dev, int x,u16 y)
{
        outw(y,dev->base_addr +x);
}

void write_nic_dword(struct net_device *dev, int x,u32 y)
{
        outl(y,dev->base_addr +x);
}

#else /* RTL_IO_MAP */

u8 read_nic_byte(struct net_device *dev, int x)
{
        return 0xff&readb((u8*)dev->mem_start +x);
}

u32 read_nic_dword(struct net_device *dev, int x)
{
        return readl((u8*)dev->mem_start +x);
}

u16 read_nic_word(struct net_device *dev, int x)
{
        return readw((u8*)dev->mem_start +x);
}

void write_nic_byte(struct net_device *dev, int x,u8 y)
{
        writeb(y,(u8*)dev->mem_start +x);
	udelay(20);
}

void write_nic_dword(struct net_device *dev, int x,u32 y)
{
        writel(y,(u8*)dev->mem_start +x);
	udelay(20);
}

void write_nic_word(struct net_device *dev, int x,u16 y)
{
        writew(y,(u8*)dev->mem_start +x);
	udelay(20);
}

#endif /* RTL_IO_MAP */


///////////////////////////////////////////////////////////

//u8 read_phy_cck(struct net_device *dev, u8 adr);
//u8 read_phy_ofdm(struct net_device *dev, u8 adr);
/* this might still called in what was the PHY rtl8185/rtl8192 common code
 * plans are to possibilty turn it again in one common code...
 */
inline void force_pci_posting(struct net_device *dev)
{
}


//warning message WB
irqreturn_t rtl8192_interrupt(int irq, void *netdev);
//static struct net_device_stats *rtl8192_stats(struct net_device *dev);
void rtl8192_commit(struct net_device *dev);
//void rtl8192_restart(struct net_device *dev);
void rtl8192_restart(struct work_struct *work);
//void rtl8192_rq_tx_ack(struct work_struct *work);

void watch_dog_timer_callback(unsigned long data);
#ifdef ENABLE_IPS
void IPSEnter(struct net_device *dev);
void IPSLeave(struct net_device *dev);
void InactivePsWorkItemCallback(struct net_device *dev);
#endif
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
	int i,n;

	int max=0xff;

	/* This dump the current register page */
	len += snprintf(page + len, count - len,
                        "\n####################page 0##################\n ");

	for(n=0;n<=max;)
	{
		//printk( "\nD: %2x> ", n);
		len += snprintf(page + len, count - len,
			"\nD:  %2x > ",n);

		for(i=0;i<16 && n<=max;i++,n++)
		len += snprintf(page + len, count - len,
			"%2x ",read_nic_byte(dev,n));

		//	printk("%2x ",read_nic_byte(dev,n));
	}
	len += snprintf(page + len, count - len,"\n");
	len += snprintf(page + len, count - len,
                        "\n####################page 1##################\n ");
        for(n=0;n<=max;)
        {
                //printk( "\nD: %2x> ", n);
                len += snprintf(page + len, count - len,
                        "\nD:  %2x > ",n);

                for(i=0;i<16 && n<=max;i++,n++)
                len += snprintf(page + len, count - len,
                        "%2x ",read_nic_byte(dev,0x100|n));

                //      printk("%2x ",read_nic_byte(dev,n));
        }

	len += snprintf(page + len, count - len,
                        "\n####################page 3##################\n ");
        for(n=0;n<=max;)
        {
                //printk( "\nD: %2x> ", n);
                len += snprintf(page + len, count - len,
                        "\nD:  %2x > ",n);

                for(i=0;i<16 && n<=max;i++,n++)
                len += snprintf(page + len, count - len,
                        "%2x ",read_nic_byte(dev,0x300|n));

                //      printk("%2x ",read_nic_byte(dev,n));
        }


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
//		"TX VI priority error int: %lu\n"
		"TX VO priority ok int: %lu\n"
//		"TX VO priority error int: %lu\n"
		"TX BE priority ok int: %lu\n"
//		"TX BE priority error int: %lu\n"
		"TX BK priority ok int: %lu\n"
//		"TX BK priority error int: %lu\n"
		"TX MANAGE priority ok int: %lu\n"
//		"TX MANAGE priority error int: %lu\n"
		"TX BEACON priority ok int: %lu\n"
		"TX BEACON priority error int: %lu\n"
		"TX CMDPKT priority ok int: %lu\n"
//		"TX high priority ok int: %lu\n"
//		"TX high priority failed error int: %lu\n"
//		"TX queue resume: %lu\n"
		"TX queue stopped?: %d\n"
		"TX fifo overflow: %lu\n"
//		"TX beacon: %lu\n"
//		"TX VI queue: %d\n"
//		"TX VO queue: %d\n"
//		"TX BE queue: %d\n"
//		"TX BK queue: %d\n"
//		"TX HW queue: %d\n"
//		"TX VI dropped: %lu\n"
//		"TX VO dropped: %lu\n"
//		"TX BE dropped: %lu\n"
//		"TX BK dropped: %lu\n"
		"TX total data packets %lu\n"
		"TX total data bytes :%lu\n",
//		"TX beacon aborted: %lu\n",
		priv->stats.txviokint,
//		priv->stats.txvierr,
		priv->stats.txvookint,
//		priv->stats.txvoerr,
		priv->stats.txbeokint,
//		priv->stats.txbeerr,
		priv->stats.txbkokint,
//		priv->stats.txbkerr,
		priv->stats.txmanageokint,
//		priv->stats.txmanageerr,
		priv->stats.txbeaconokint,
		priv->stats.txbeaconerr,
		priv->stats.txcmdpktokint,
//		priv->stats.txhpokint,
//		priv->stats.txhperr,
//		priv->stats.txresumed,
		netif_queue_stopped(dev),
		priv->stats.txoverflow,
//		priv->stats.txbeacon,
//		atomic_read(&(priv->tx_pending[VI_QUEUE])),
//		atomic_read(&(priv->tx_pending[VO_QUEUE])),
//		atomic_read(&(priv->tx_pending[BE_QUEUE])),
//		atomic_read(&(priv->tx_pending[BK_QUEUE])),
//		read_nic_byte(dev, TXFIFOCOUNT),
//		priv->stats.txvidrop,
//		priv->stats.txvodrop,
		priv->ieee80211->stats.tx_packets,
		priv->ieee80211->stats.tx_bytes


//		priv->stats.txbedrop,
//		priv->stats.txbkdrop
			//	priv->stats.txdatapkt
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
		"RX desc err: %lu\n"
		"RX rx overflow error: %lu\n"
		"RX invalid urb error: %lu\n",
		priv->stats.rxint,
		priv->stats.rxrdu,
		priv->stats.rxoverflow,
		priv->stats.rxurberr);

	*eof = 1;
	return len;
}

static void rtl8192_proc_module_init(void)
{
	RT_TRACE(COMP_INIT, "Initializing proc filesystem");
	rtl8192_proc=create_proc_entry(RTL819xE_MODULE_NAME, S_IFDIR, init_net.proc_net);
}


static void rtl8192_proc_module_remove(void)
{
	remove_proc_entry(RTL819xE_MODULE_NAME, init_net.proc_net);
}


static void rtl8192_proc_remove_one(struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	printk("dev name=======> %s\n",dev->name);

	if (priv->dir_dev) {
	//	remove_proc_entry("stats-hw", priv->dir_dev);
		remove_proc_entry("stats-tx", priv->dir_dev);
		remove_proc_entry("stats-rx", priv->dir_dev);
	//	remove_proc_entry("stats-ieee", priv->dir_dev);
		remove_proc_entry("stats-ap", priv->dir_dev);
		remove_proc_entry("registers", priv->dir_dev);
	//	remove_proc_entry("cck-registers",priv->dir_dev);
	//	remove_proc_entry("ofdm-registers",priv->dir_dev);
		//remove_proc_entry(dev->name, rtl8192_proc);
		remove_proc_entry("wlan0", rtl8192_proc);
		priv->dir_dev = NULL;
	}
}


static void rtl8192_proc_init_one(struct net_device *dev)
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
}
/****************************************************************************
   -----------------------------MISC STUFF-------------------------
*****************************************************************************/

short check_nic_enough_desc(struct net_device *dev, int prio)
{
    struct r8192_priv *priv = ieee80211_priv(dev);
    struct rtl8192_tx_ring *ring = &priv->tx_ring[prio];

    /* for now we reserve two free descriptor as a safety boundary
     * between the tail and the head
     */
    if (ring->entries - skb_queue_len(&ring->queue) >= 2) {
        return 1;
    } else {
        return 0;
    }
}

static void tx_timeout(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	//rtl8192_commit(dev);

	schedule_work(&priv->reset_wq);
	printk("TXTIMEOUT");
}


/****************************************************************************
      ------------------------------HW STUFF---------------------------
*****************************************************************************/


static void rtl8192_irq_enable(struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	priv->irq_enabled = 1;
	write_nic_dword(dev,INTA_MASK, priv->irq_mask);
}


static void rtl8192_irq_disable(struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

	write_nic_dword(dev,INTA_MASK,0);
	force_pci_posting(dev);
	priv->irq_enabled = 0;
}


static void rtl8192_set_mode(struct net_device *dev,int mode)
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
    RT_TRACE(COMP_RF, "=====>%s()====ch:%d\n", __FUNCTION__, ch);
    priv->chan=ch;
#if 0
    if(priv->ieee80211->iw_mode == IW_MODE_ADHOC ||
            priv->ieee80211->iw_mode == IW_MODE_MASTER){

        priv->ieee80211->link_state = WLAN_LINK_ASSOCIATED;
        priv->ieee80211->master_chan = ch;
        rtl8192_update_beacon_ch(dev);
    }
#endif

    /* this hack should avoid frame TX during channel setting*/


    //	tx = read_nic_dword(dev,TX_CONF);
    //	tx &= ~TX_LOOPBACK_MASK;

#ifndef LOOP_TEST
    //TODO
    //	write_nic_dword(dev,TX_CONF, tx |( TX_LOOPBACK_MAC<<TX_LOOPBACK_SHIFT));

    //need to implement rf set channel here WB

    if (priv->rf_set_chan)
        priv->rf_set_chan(dev,priv->chan);
    //	mdelay(10);
    //	write_nic_dword(dev,TX_CONF,tx | (TX_LOOPBACK_NONE<<TX_LOOPBACK_SHIFT));
#endif
}

void rtl8192_rx_enable(struct net_device *dev)
{
    struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
    write_nic_dword(dev, RDQDA,priv->rx_ring_dma);
}

/* the TX_DESC_BASE setting is according to the following queue index
 *  BK_QUEUE       ===>                        0
 *  BE_QUEUE       ===>                        1
 *  VI_QUEUE       ===>                        2
 *  VO_QUEUE       ===>                        3
 *  HCCA_QUEUE     ===>                        4
 *  TXCMD_QUEUE    ===>                        5
 *  MGNT_QUEUE     ===>                        6
 *  HIGH_QUEUE     ===>                        7
 *  BEACON_QUEUE   ===>                        8
 *  */
static u32 TX_DESC_BASE[] = {BKQDA, BEQDA, VIQDA, VOQDA, HCCAQDA, CQDA, MQDA, HQDA, BQDA};
void rtl8192_tx_enable(struct net_device *dev)
{
    struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
    u32 i;
    for (i = 0; i < MAX_TX_QUEUE_COUNT; i++)
        write_nic_dword(dev, TX_DESC_BASE[i], priv->tx_ring[i].dma);

    ieee80211_reset_queue(priv->ieee80211);
}


static void rtl8192_free_rx_ring(struct net_device *dev)
{
    struct r8192_priv *priv = ieee80211_priv(dev);
    int i;

    for (i = 0; i < priv->rxringcount; i++) {
        struct sk_buff *skb = priv->rx_buf[i];
        if (!skb)
            continue;

        pci_unmap_single(priv->pdev,
                *((dma_addr_t *)skb->cb),
                priv->rxbuffersize, PCI_DMA_FROMDEVICE);
        kfree_skb(skb);
    }

    pci_free_consistent(priv->pdev, sizeof(*priv->rx_ring) * priv->rxringcount,
            priv->rx_ring, priv->rx_ring_dma);
    priv->rx_ring = NULL;
}

static void rtl8192_free_tx_ring(struct net_device *dev, unsigned int prio)
{
    struct r8192_priv *priv = ieee80211_priv(dev);
    struct rtl8192_tx_ring *ring = &priv->tx_ring[prio];

    while (skb_queue_len(&ring->queue)) {
        tx_desc_819x_pci *entry = &ring->desc[ring->idx];
        struct sk_buff *skb = __skb_dequeue(&ring->queue);

        pci_unmap_single(priv->pdev, le32_to_cpu(entry->TxBuffAddr),
                skb->len, PCI_DMA_TODEVICE);
        kfree_skb(skb);
        ring->idx = (ring->idx + 1) % ring->entries;
    }

    pci_free_consistent(priv->pdev, sizeof(*ring->desc)*ring->entries,
            ring->desc, ring->dma);
    ring->desc = NULL;
}


static void rtl8192_beacon_disable(struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	u32 reg;

	reg = read_nic_dword(priv->ieee80211->dev,INTA_MASK);

	/* disable Beacon realted interrupt signal */
	reg &= ~(IMR_BcnInt | IMR_BcnInt | IMR_TBDOK | IMR_TBDER);
	write_nic_dword(priv->ieee80211->dev, INTA_MASK, reg);
}

void rtl8192_rtx_disable(struct net_device *dev)
{
	u8 cmd;
	struct r8192_priv *priv = ieee80211_priv(dev);
        int i;

	cmd=read_nic_byte(dev,CMDR);
//	if(!priv->ieee80211->bSupportRemoteWakeUp) {
		write_nic_byte(dev, CMDR, cmd &~ \
				(CR_TE|CR_RE));
//	}
	force_pci_posting(dev);
	mdelay(30);

        for(i = 0; i < MAX_QUEUE_SIZE; i++) {
                skb_queue_purge(&priv->ieee80211->skb_waitQ [i]);
        }
        for(i = 0; i < MAX_QUEUE_SIZE; i++) {
                skb_queue_purge(&priv->ieee80211->skb_aggQ [i]);
        }


	skb_queue_purge(&priv->skb_queue);
	return;
}

static void rtl8192_reset(struct net_device *dev)
{
    rtl8192_irq_disable(dev);
    printk("This is RTL819xP Reset procedure\n");
}

static u16 rtl_rate[] = {10,20,55,110,60,90,120,180,240,360,480,540};
inline u16 rtl8192_rate2rate(short rate)
{
	if (rate >11) return 0;
	return rtl_rate[rate];
}




static void rtl8192_data_hard_stop(struct net_device *dev)
{
	//FIXME !!
	#if 0
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	priv->dma_poll_mask |= (1<<TX_DMA_STOP_LOWPRIORITY_SHIFT);
	rtl8192_set_mode(dev,EPROM_CMD_CONFIG);
	write_nic_byte(dev,TX_DMA_POLLING,priv->dma_poll_mask);
	rtl8192_set_mode(dev,EPROM_CMD_NORMAL);
	#endif
}


static void rtl8192_data_hard_resume(struct net_device *dev)
{
	// FIXME !!
	#if 0
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	priv->dma_poll_mask &= ~(1<<TX_DMA_STOP_LOWPRIORITY_SHIFT);
	rtl8192_set_mode(dev,EPROM_CMD_CONFIG);
	write_nic_byte(dev,TX_DMA_POLLING,priv->dma_poll_mask);
	rtl8192_set_mode(dev,EPROM_CMD_NORMAL);
	#endif
}

/* this function TX data frames when the ieee80211 stack requires this.
 * It checks also if we need to stop the ieee tx queue, eventually do it
 */
static void rtl8192_hard_data_xmit(struct sk_buff *skb, struct net_device *dev, int rate)
{
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
	int ret;
	//unsigned long flags;
	cb_desc *tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
	u8 queue_index = tcb_desc->queue_index;
	/* shall not be referred by command packet */
	assert(queue_index != TXCMD_QUEUE);

	//spin_lock_irqsave(&priv->tx_lock,flags);

        memcpy((unsigned char *)(skb->cb),&dev,sizeof(dev));
#if 0
	tcb_desc->RATRIndex = 7;
	tcb_desc->bTxDisableRateFallBack = 1;
	tcb_desc->bTxUseDriverAssingedRate = 1;
	tcb_desc->bTxEnableFwCalcDur = 1;
#endif
	skb_push(skb, priv->ieee80211->tx_headroom);
	ret = rtl8192_tx(dev, skb);
	if(ret != 0) {
		kfree_skb(skb);
	};

//
	if(queue_index!=MGNT_QUEUE) {
	priv->ieee80211->stats.tx_bytes+=(skb->len - priv->ieee80211->tx_headroom);
	priv->ieee80211->stats.tx_packets++;
	}

	//spin_unlock_irqrestore(&priv->tx_lock,flags);

//	return ret;
	return;
}

/* This is a rough attempt to TX a frame
 * This is called by the ieee 80211 stack to TX management frames.
 * If the ring is full packet are dropped (for data frame the queue
 * is stopped before this can happen).
 */
static int rtl8192_hard_start_xmit(struct sk_buff *skb,struct net_device *dev)
{
	struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);


	int ret;
	//unsigned long flags;
        cb_desc *tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
        u8 queue_index = tcb_desc->queue_index;


	//spin_lock_irqsave(&priv->tx_lock,flags);

        memcpy((unsigned char *)(skb->cb),&dev,sizeof(dev));
	if(queue_index == TXCMD_QUEUE) {
	//	skb_push(skb, USB_HWDESC_HEADER_LEN);
		rtl819xE_tx_cmd(dev, skb);
		ret = 0;
	        //spin_unlock_irqrestore(&priv->tx_lock,flags);
		return ret;
	} else {
	//	RT_TRACE(COMP_SEND, "To send management packet\n");
		tcb_desc->RATRIndex = 7;
		tcb_desc->bTxDisableRateFallBack = 1;
		tcb_desc->bTxUseDriverAssingedRate = 1;
		tcb_desc->bTxEnableFwCalcDur = 1;
		skb_push(skb, priv->ieee80211->tx_headroom);
		ret = rtl8192_tx(dev, skb);
		if(ret != 0) {
			kfree_skb(skb);
		};
	}

//	priv->ieee80211->stats.tx_bytes+=skb->len;
//	priv->ieee80211->stats.tx_packets++;

	//spin_unlock_irqrestore(&priv->tx_lock,flags);

	return ret;

}


void rtl8192_try_wake_queue(struct net_device *dev, int pri);

static void rtl8192_tx_isr(struct net_device *dev, int prio)
{
    struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);

    struct rtl8192_tx_ring *ring = &priv->tx_ring[prio];

    while (skb_queue_len(&ring->queue)) {
        tx_desc_819x_pci *entry = &ring->desc[ring->idx];
        struct sk_buff *skb;

        /* beacon packet will only use the first descriptor defautly,
         * and the OWN may not be cleared by the hardware
         * */
        if(prio != BEACON_QUEUE) {
            if(entry->OWN)
                return;
            ring->idx = (ring->idx + 1) % ring->entries;
        }

        skb = __skb_dequeue(&ring->queue);
        pci_unmap_single(priv->pdev, le32_to_cpu(entry->TxBuffAddr),
                skb->len, PCI_DMA_TODEVICE);

        kfree_skb(skb);
    }
    if (prio == MGNT_QUEUE){
        if (priv->ieee80211->ack_tx_to_ieee){
            if (rtl8192_is_tx_queue_empty(dev)){
                priv->ieee80211->ack_tx_to_ieee = 0;
                ieee80211_ps_tx_ack(priv->ieee80211, 1);
            }
        }
    }

    if(prio != BEACON_QUEUE) {
        /* try to deal with the pending packets  */
        tasklet_schedule(&priv->irq_tx_tasklet);
    }

}

static void rtl8192_stop_beacon(struct net_device *dev)
{
	//rtl8192_beacon_disable(dev);
}

static void rtl8192_config_rate(struct net_device* dev, u16* rate_config)
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

static void rtl8192_update_cap(struct net_device* dev, u16 cap)
{
	u32 tmp = 0;
	struct r8192_priv *priv = ieee80211_priv(dev);
	struct ieee80211_network *net = &priv->ieee80211->current_network;
	priv->short_preamble = cap & WLAN_CAPABILITY_SHORT_PREAMBLE;
	tmp = priv->basic_rate;
	if (priv->short_preamble)
		tmp |= BRSR_AckShortPmb;
	write_nic_dword(dev, RRSR, tmp);

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

static void rtl8192_net_update(struct net_device *dev)
{

	struct r8192_priv *priv = ieee80211_priv(dev);
	struct ieee80211_network *net;
	u16 BcnTimeCfg = 0, BcnCW = 6, BcnIFS = 0xf;
	u16 rate_config = 0;
	net = &priv->ieee80211->current_network;
	//update Basic rate: RR, BRSR
	rtl8192_config_rate(dev, &rate_config);
	// 2007.01.16, by Emily
	// Select RRSR (in Legacy-OFDM and CCK)
	// For 8190, we select only 24M, 12M, 6M, 11M, 5.5M, 2M, and 1M from the Basic rate.
	// We do not use other rates.
	 priv->basic_rate = rate_config &= 0x15f;
	//BSSID
	write_nic_dword(dev,BSSIDR,((u32*)net->bssid)[0]);
	write_nic_word(dev,BSSIDR+4,((u16*)net->bssid)[2]);
#if 0
	//MSR
	rtl8192_update_msr(dev);
#endif


//	rtl8192_update_cap(dev, net->capability);
	if (priv->ieee80211->iw_mode == IW_MODE_ADHOC)
	{
		write_nic_word(dev, ATIMWND, 2);
		write_nic_word(dev, BCN_DMATIME, 256);
		write_nic_word(dev, BCN_INTERVAL, net->beacon_interval);
	//	write_nic_word(dev, BcnIntTime, 100);
	//BIT15 of BCN_DRV_EARLY_INT will indicate whether software beacon or hw beacon is applied.
		write_nic_word(dev, BCN_DRV_EARLY_INT, 10);
		write_nic_byte(dev, BCN_ERR_THRESH, 100);

		BcnTimeCfg |= (BcnCW<<BCN_TCFG_CW_SHIFT);
	// TODO: BcnIFS may required to be changed on ASIC
	 	BcnTimeCfg |= BcnIFS<<BCN_TCFG_IFS;

		write_nic_word(dev, BCN_TCFG, BcnTimeCfg);
	}


}

void rtl819xE_tx_cmd(struct net_device *dev, struct sk_buff *skb)
{
    struct r8192_priv *priv = ieee80211_priv(dev);
    struct rtl8192_tx_ring *ring;
    tx_desc_819x_pci *entry;
    unsigned int idx;
    dma_addr_t mapping;
    cb_desc *tcb_desc;
    unsigned long flags;

    ring = &priv->tx_ring[TXCMD_QUEUE];
    mapping = pci_map_single(priv->pdev, skb->data, skb->len, PCI_DMA_TODEVICE);

    spin_lock_irqsave(&priv->irq_th_lock,flags);
    idx = (ring->idx + skb_queue_len(&ring->queue)) % ring->entries;
    entry = &ring->desc[idx];

    tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
    memset(entry,0,12);
    entry->LINIP = tcb_desc->bLastIniPkt;
    entry->FirstSeg = 1;//first segment
    entry->LastSeg = 1; //last segment
    if(tcb_desc->bCmdOrInit == DESC_PACKET_TYPE_INIT) {
        entry->CmdInit = DESC_PACKET_TYPE_INIT;
    } else {
        entry->CmdInit = DESC_PACKET_TYPE_NORMAL;
        entry->Offset = sizeof(TX_FWINFO_8190PCI) + 8;
        entry->PktSize = (u16)(tcb_desc->pkt_size + entry->Offset);
        entry->QueueSelect = QSLT_CMD;
        entry->TxFWInfoSize = 0x08;
        entry->RATid = (u8)DESC_PACKET_TYPE_INIT;
    }
    entry->TxBufferSize = skb->len;
    entry->TxBuffAddr = cpu_to_le32(mapping);
    entry->OWN = 1;

#ifdef JOHN_DUMP_TXDESC
    {       int i;
        tx_desc_819x_pci *entry1 =  &ring->desc[0];
        unsigned int *ptr= (unsigned int *)entry1;
        printk("<Tx descriptor>:\n");
        for (i = 0; i < 8; i++)
            printk("%8x ", ptr[i]);
        printk("\n");
    }
#endif
    __skb_queue_tail(&ring->queue, skb);
    spin_unlock_irqrestore(&priv->irq_th_lock,flags);

    write_nic_byte(dev, TPPoll, TPPoll_CQ);

    return;
}

/*
 * Mapping Software/Hardware descriptor queue id to "Queue Select Field"
 * in TxFwInfo data structure
 * 2006.10.30 by Emily
 *
 * \param QUEUEID       Software Queue
*/
static u8 MapHwQueueToFirmwareQueue(u8 QueueID)
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
			//QueueSelect = QSLT_HIGH;
			//break;

		default:
			RT_TRACE(COMP_ERR, "TransmitTCB(): Impossible Queue Selection: %d \n", QueueID);
			break;
	}
	return QueueSelect;
}

static u8 MRateToHwRate8190Pci(u8 rate)
{
	u8  ret = DESC90_RATE1M;

	switch(rate) {
		case MGN_1M:	ret = DESC90_RATE1M;		break;
		case MGN_2M:	ret = DESC90_RATE2M;		break;
		case MGN_5_5M:	ret = DESC90_RATE5_5M;	break;
		case MGN_11M:	ret = DESC90_RATE11M;	break;
		case MGN_6M:	ret = DESC90_RATE6M;		break;
		case MGN_9M:	ret = DESC90_RATE9M;		break;
		case MGN_12M:	ret = DESC90_RATE12M;	break;
		case MGN_18M:	ret = DESC90_RATE18M;	break;
		case MGN_24M:	ret = DESC90_RATE24M;	break;
		case MGN_36M:	ret = DESC90_RATE36M;	break;
		case MGN_48M:	ret = DESC90_RATE48M;	break;
		case MGN_54M:	ret = DESC90_RATE54M;	break;

		// HT rate since here
		case MGN_MCS0:	ret = DESC90_RATEMCS0;	break;
		case MGN_MCS1:	ret = DESC90_RATEMCS1;	break;
		case MGN_MCS2:	ret = DESC90_RATEMCS2;	break;
		case MGN_MCS3:	ret = DESC90_RATEMCS3;	break;
		case MGN_MCS4:	ret = DESC90_RATEMCS4;	break;
		case MGN_MCS5:	ret = DESC90_RATEMCS5;	break;
		case MGN_MCS6:	ret = DESC90_RATEMCS6;	break;
		case MGN_MCS7:	ret = DESC90_RATEMCS7;	break;
		case MGN_MCS8:	ret = DESC90_RATEMCS8;	break;
		case MGN_MCS9:	ret = DESC90_RATEMCS9;	break;
		case MGN_MCS10:	ret = DESC90_RATEMCS10;	break;
		case MGN_MCS11:	ret = DESC90_RATEMCS11;	break;
		case MGN_MCS12:	ret = DESC90_RATEMCS12;	break;
		case MGN_MCS13:	ret = DESC90_RATEMCS13;	break;
		case MGN_MCS14:	ret = DESC90_RATEMCS14;	break;
		case MGN_MCS15:	ret = DESC90_RATEMCS15;	break;
		case (0x80|0x20): ret = DESC90_RATEMCS32; break;

		default:       break;
	}
	return ret;
}


static u8 QueryIsShort(u8 TxHT, u8 TxRate, cb_desc *tcb_desc)
{
	u8   tmp_Short;

	tmp_Short = (TxHT==1)?((tcb_desc->bUseShortGI)?1:0):((tcb_desc->bUseShortPreamble)?1:0);

	if(TxHT==1 && TxRate != DESC90_RATEMCS15)
		tmp_Short = 0;

	return tmp_Short;
}

/*
 * The tx procedure is just as following,
 * skb->cb will contain all the following information,
 * priority, morefrag, rate, &dev.
 * */
short rtl8192_tx(struct net_device *dev, struct sk_buff* skb)
{
    struct r8192_priv *priv = ieee80211_priv(dev);
    struct rtl8192_tx_ring  *ring;
    unsigned long flags;
    cb_desc *tcb_desc = (cb_desc *)(skb->cb + MAX_DEV_ADDR_SIZE);
    tx_desc_819x_pci *pdesc = NULL;
    TX_FWINFO_8190PCI *pTxFwInfo = NULL;
    dma_addr_t mapping;
    bool  multi_addr=false,broad_addr=false,uni_addr=false;
    u8*   pda_addr = NULL;
    int   idx;

    mapping = pci_map_single(priv->pdev, skb->data, skb->len, PCI_DMA_TODEVICE);
    /* collect the tx packets statitcs */
    pda_addr = ((u8*)skb->data) + sizeof(TX_FWINFO_8190PCI);
    if(is_multicast_ether_addr(pda_addr))
        multi_addr = true;
    else if(is_broadcast_ether_addr(pda_addr))
        broad_addr = true;
    else
        uni_addr = true;

    if(uni_addr)
        priv->stats.txbytesunicast += (u8)(skb->len) - sizeof(TX_FWINFO_8190PCI);
    else if(multi_addr)
        priv->stats.txbytesmulticast +=(u8)(skb->len) - sizeof(TX_FWINFO_8190PCI);
    else
        priv->stats.txbytesbroadcast += (u8)(skb->len) - sizeof(TX_FWINFO_8190PCI);

    /* fill tx firmware */
    pTxFwInfo = (PTX_FWINFO_8190PCI)skb->data;
    memset(pTxFwInfo,0,sizeof(TX_FWINFO_8190PCI));
    pTxFwInfo->TxHT = (tcb_desc->data_rate&0x80)?1:0;
    pTxFwInfo->TxRate = MRateToHwRate8190Pci((u8)tcb_desc->data_rate);
    pTxFwInfo->EnableCPUDur = tcb_desc->bTxEnableFwCalcDur;
    pTxFwInfo->Short	= QueryIsShort(pTxFwInfo->TxHT, pTxFwInfo->TxRate, tcb_desc);

    /* Aggregation related */
    if(tcb_desc->bAMPDUEnable) {
        pTxFwInfo->AllowAggregation = 1;
        pTxFwInfo->RxMF = tcb_desc->ampdu_factor;
        pTxFwInfo->RxAMD = tcb_desc->ampdu_density;
    } else {
        pTxFwInfo->AllowAggregation = 0;
        pTxFwInfo->RxMF = 0;
        pTxFwInfo->RxAMD = 0;
    }

    //
    // Protection mode related
    //
    pTxFwInfo->RtsEnable =	(tcb_desc->bRTSEnable)?1:0;
    pTxFwInfo->CtsEnable =	(tcb_desc->bCTSEnable)?1:0;
    pTxFwInfo->RtsSTBC =	(tcb_desc->bRTSSTBC)?1:0;
    pTxFwInfo->RtsHT=		(tcb_desc->rts_rate&0x80)?1:0;
    pTxFwInfo->RtsRate =		MRateToHwRate8190Pci((u8)tcb_desc->rts_rate);
    pTxFwInfo->RtsBandwidth = 0;
    pTxFwInfo->RtsSubcarrier = tcb_desc->RTSSC;
    pTxFwInfo->RtsShort =	(pTxFwInfo->RtsHT==0)?(tcb_desc->bRTSUseShortPreamble?1:0):(tcb_desc->bRTSUseShortGI?1:0);
    //
    // Set Bandwidth and sub-channel settings.
    //
    if(priv->CurrentChannelBW == HT_CHANNEL_WIDTH_20_40)
    {
        if(tcb_desc->bPacketBW)
        {
            pTxFwInfo->TxBandwidth = 1;
#ifdef RTL8190P
            pTxFwInfo->TxSubCarrier = 3;
#else
            pTxFwInfo->TxSubCarrier = 0;	//By SD3's Jerry suggestion, use duplicated mode, cosa 04012008
#endif
        }
        else
        {
            pTxFwInfo->TxBandwidth = 0;
            pTxFwInfo->TxSubCarrier = priv->nCur40MhzPrimeSC;
        }
    } else {
        pTxFwInfo->TxBandwidth = 0;
        pTxFwInfo->TxSubCarrier = 0;
    }

    if (0)
    {
	    /* 2007/07/25 MH  Copy current TX FW info.*/
	    memcpy((void*)(&Tmp_TxFwInfo), (void*)(pTxFwInfo), sizeof(TX_FWINFO_8190PCI));
	    printk("&&&&&&&&&&&&&&&&&&&&&&====>print out fwinf\n");
	    printk("===>enable fwcacl:%d\n", Tmp_TxFwInfo.EnableCPUDur);
	    printk("===>RTS STBC:%d\n", Tmp_TxFwInfo.RtsSTBC);
	    printk("===>RTS Subcarrier:%d\n", Tmp_TxFwInfo.RtsSubcarrier);
	    printk("===>Allow Aggregation:%d\n", Tmp_TxFwInfo.AllowAggregation);
	    printk("===>TX HT bit:%d\n", Tmp_TxFwInfo.TxHT);
	    printk("===>Tx rate:%d\n", Tmp_TxFwInfo.TxRate);
	    printk("===>Received AMPDU Density:%d\n", Tmp_TxFwInfo.RxAMD);
	    printk("===>Received MPDU Factor:%d\n", Tmp_TxFwInfo.RxMF);
	    printk("===>TxBandwidth:%d\n", Tmp_TxFwInfo.TxBandwidth);
	    printk("===>TxSubCarrier:%d\n", Tmp_TxFwInfo.TxSubCarrier);

        printk("<=====**********************out of print\n");

    }
    spin_lock_irqsave(&priv->irq_th_lock,flags);
    ring = &priv->tx_ring[tcb_desc->queue_index];
    if (tcb_desc->queue_index != BEACON_QUEUE) {
        idx = (ring->idx + skb_queue_len(&ring->queue)) % ring->entries;
    } else {
        idx = 0;
    }

    pdesc = &ring->desc[idx];
    if((pdesc->OWN == 1) && (tcb_desc->queue_index != BEACON_QUEUE)) {
	    RT_TRACE(COMP_ERR,"No more TX desc@%d, ring->idx = %d,idx = %d,%x", \
			    tcb_desc->queue_index,ring->idx, idx,skb->len);
	    return skb->len;
    }

    /* fill tx descriptor */
    memset((u8*)pdesc,0,12);
    /*DWORD 0*/
    pdesc->LINIP = 0;
    pdesc->CmdInit = 1;
    pdesc->Offset = sizeof(TX_FWINFO_8190PCI) + 8; //We must add 8!! Emily
    pdesc->PktSize = (u16)skb->len-sizeof(TX_FWINFO_8190PCI);

    /*DWORD 1*/
    pdesc->SecCAMID= 0;
    pdesc->RATid = tcb_desc->RATRIndex;


    pdesc->NoEnc = 1;
    pdesc->SecType = 0x0;
    if (tcb_desc->bHwSec) {
        static u8 tmp =0;
        if (!tmp) {
            printk("==>================hw sec\n");
            tmp = 1;
        }
        switch (priv->ieee80211->pairwise_key_type) {
            case KEY_TYPE_WEP40:
            case KEY_TYPE_WEP104:
                pdesc->SecType = 0x1;
                pdesc->NoEnc = 0;
                break;
            case KEY_TYPE_TKIP:
                pdesc->SecType = 0x2;
                pdesc->NoEnc = 0;
                break;
            case KEY_TYPE_CCMP:
                pdesc->SecType = 0x3;
                pdesc->NoEnc = 0;
                break;
            case KEY_TYPE_NA:
                pdesc->SecType = 0x0;
                pdesc->NoEnc = 1;
                break;
        }
    }

    //
    // Set Packet ID
    //
    pdesc->PktId = 0x0;

    pdesc->QueueSelect = MapHwQueueToFirmwareQueue(tcb_desc->queue_index);
    pdesc->TxFWInfoSize = sizeof(TX_FWINFO_8190PCI);

    pdesc->DISFB = tcb_desc->bTxDisableRateFallBack;
    pdesc->USERATE = tcb_desc->bTxUseDriverAssingedRate;

    pdesc->FirstSeg =1;
    pdesc->LastSeg = 1;
    pdesc->TxBufferSize = skb->len;

    pdesc->TxBuffAddr = cpu_to_le32(mapping);
    __skb_queue_tail(&ring->queue, skb);
    pdesc->OWN = 1;
    spin_unlock_irqrestore(&priv->irq_th_lock,flags);
    dev->trans_start = jiffies;
    write_nic_word(dev,TPPoll,0x01<<tcb_desc->queue_index);
    return 0;
}

static short rtl8192_alloc_rx_desc_ring(struct net_device *dev)
{
    struct r8192_priv *priv = ieee80211_priv(dev);
    rx_desc_819x_pci *entry = NULL;
    int i;

    priv->rx_ring = pci_alloc_consistent(priv->pdev,
            sizeof(*priv->rx_ring) * priv->rxringcount, &priv->rx_ring_dma);

    if (!priv->rx_ring || (unsigned long)priv->rx_ring & 0xFF) {
        RT_TRACE(COMP_ERR,"Cannot allocate RX ring\n");
        return -ENOMEM;
    }

    memset(priv->rx_ring, 0, sizeof(*priv->rx_ring) * priv->rxringcount);
    priv->rx_idx = 0;

    for (i = 0; i < priv->rxringcount; i++) {
        struct sk_buff *skb = dev_alloc_skb(priv->rxbuffersize);
        dma_addr_t *mapping;
        entry = &priv->rx_ring[i];
        if (!skb)
            return 0;
        priv->rx_buf[i] = skb;
        mapping = (dma_addr_t *)skb->cb;
        *mapping = pci_map_single(priv->pdev, skb->tail,//skb_tail_pointer(skb),
                priv->rxbuffersize, PCI_DMA_FROMDEVICE);

        entry->BufferAddress = cpu_to_le32(*mapping);

        entry->Length = priv->rxbuffersize;
        entry->OWN = 1;
    }

    entry->EOR = 1;
    return 0;
}

static int rtl8192_alloc_tx_desc_ring(struct net_device *dev,
        unsigned int prio, unsigned int entries)
{
    struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
    tx_desc_819x_pci *ring;
    dma_addr_t dma;
    int i;

    ring = pci_alloc_consistent(priv->pdev, sizeof(*ring) * entries, &dma);
    if (!ring || (unsigned long)ring & 0xFF) {
        RT_TRACE(COMP_ERR, "Cannot allocate TX ring (prio = %d)\n", prio);
        return -ENOMEM;
    }

    memset(ring, 0, sizeof(*ring)*entries);
    priv->tx_ring[prio].desc = ring;
    priv->tx_ring[prio].dma = dma;
    priv->tx_ring[prio].idx = 0;
    priv->tx_ring[prio].entries = entries;
    skb_queue_head_init(&priv->tx_ring[prio].queue);

    for (i = 0; i < entries; i++)
        ring[i].NextDescAddress =
            cpu_to_le32((u32)dma + ((i + 1) % entries) * sizeof(*ring));

    return 0;
}


static short rtl8192_pci_initdescring(struct net_device *dev)
{
    u32 ret;
    int i;
    struct r8192_priv *priv = ieee80211_priv(dev);

    ret = rtl8192_alloc_rx_desc_ring(dev);
    if (ret) {
        return ret;
    }


    /* general process for other queue */
    for (i = 0; i < MAX_TX_QUEUE_COUNT; i++) {
        if ((ret = rtl8192_alloc_tx_desc_ring(dev, i, priv->txringcount)))
            goto err_free_rings;
    }

#if 0
    /* specific process for hardware beacon process */
    if ((ret = rtl8192_alloc_tx_desc_ring(dev, MAX_TX_QUEUE_COUNT - 1, 2)))
        goto err_free_rings;
#endif

    return 0;

err_free_rings:
    rtl8192_free_rx_ring(dev);
    for (i = 0; i < MAX_TX_QUEUE_COUNT; i++)
        if (priv->tx_ring[i].desc)
            rtl8192_free_tx_ring(dev, i);
    return 1;
}

static void rtl8192_pci_resetdescring(struct net_device *dev)
{
    struct r8192_priv *priv = ieee80211_priv(dev);
    int i;

    /* force the rx_idx to the first one */
    if(priv->rx_ring) {
        rx_desc_819x_pci *entry = NULL;
        for (i = 0; i < priv->rxringcount; i++) {
            entry = &priv->rx_ring[i];
            entry->OWN = 1;
        }
        priv->rx_idx = 0;
    }

    /* after reset, release previous pending packet, and force the
     * tx idx to the first one */
    for (i = 0; i < MAX_TX_QUEUE_COUNT; i++) {
        if (priv->tx_ring[i].desc) {
            struct rtl8192_tx_ring *ring = &priv->tx_ring[i];

            while (skb_queue_len(&ring->queue)) {
                tx_desc_819x_pci *entry = &ring->desc[ring->idx];
                struct sk_buff *skb = __skb_dequeue(&ring->queue);

                pci_unmap_single(priv->pdev, le32_to_cpu(entry->TxBuffAddr),
                        skb->len, PCI_DMA_TODEVICE);
                kfree_skb(skb);
                ring->idx = (ring->idx + 1) % ring->entries;
            }
            ring->idx = 0;
        }
    }
}

#if 1
extern void rtl8192_update_ratr_table(struct net_device* dev);
static void rtl8192_link_change(struct net_device *dev)
{
//	int i;

	struct r8192_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device* ieee = priv->ieee80211;
	//write_nic_word(dev, BCN_INTR_ITV, net->beacon_interval);
	if (ieee->state == IEEE80211_LINKED)
	{
		rtl8192_net_update(dev);
		rtl8192_update_ratr_table(dev);
#if 1
		//add this as in pure N mode, wep encryption will use software way, but there is no chance to set this as wep will not set group key in wext. WB.2008.07.08
		if ((KEY_TYPE_WEP40 == ieee->pairwise_key_type) || (KEY_TYPE_WEP104 == ieee->pairwise_key_type))
		EnableHWSecurityConfig8192(dev);
#endif
	}
	else
	{
		write_nic_byte(dev, 0x173, 0);
	}
	/*update timing params*/
	//rtl8192_set_chan(dev, priv->chan);
	//MSR
	rtl8192_update_msr(dev);

	// 2007/10/16 MH MAC Will update TSF according to all received beacon, so we have
	//	// To set CBSSID bit when link with any AP or STA.
	if (ieee->iw_mode == IW_MODE_INFRA || ieee->iw_mode == IW_MODE_ADHOC)
	{
		u32 reg = 0;
		reg = read_nic_dword(dev, RCR);
		if (priv->ieee80211->state == IEEE80211_LINKED)
			priv->ReceiveConfig = reg |= RCR_CBSSID;
		else
			priv->ReceiveConfig = reg &= ~RCR_CBSSID;
		write_nic_dword(dev, RCR, reg);
	}
}
#endif


static struct ieee80211_qos_parameters def_qos_parameters = {
        {3,3,3,3},/* cw_min */
        {7,7,7,7},/* cw_max */
        {2,2,2,2},/* aifs */
        {0,0,0,0},/* flags */
        {0,0,0,0} /* tx_op_limit */
};

static void rtl8192_update_beacon(struct work_struct * work)
{
        struct r8192_priv *priv = container_of(work, struct r8192_priv, update_beacon_wq.work);
        struct net_device *dev = priv->ieee80211->dev;
 	struct ieee80211_device* ieee = priv->ieee80211;
	struct ieee80211_network* net = &ieee->current_network;

	if (ieee->pHTInfo->bCurrentHTSupport)
		HTUpdateSelfAndPeerSetting(ieee, net);
	ieee->pHTInfo->bCurrentRT2RTLongSlotTime = net->bssht.bdRT2RTLongSlotTime;
	rtl8192_update_cap(dev, net->capability);
}
/*
* background support to run QoS activate functionality
*/
static int WDCAPARA_ADD[] = {EDCAPARA_BE,EDCAPARA_BK,EDCAPARA_VI,EDCAPARA_VO};
static void rtl8192_qos_activate(struct work_struct * work)
{
        struct r8192_priv *priv = container_of(work, struct r8192_priv, qos_activate);
        struct net_device *dev = priv->ieee80211->dev;
        struct ieee80211_qos_parameters *qos_parameters = &priv->ieee80211->current_network.qos_data.parameters;
        u8 mode = priv->ieee80211->current_network.mode;
//        u32 size = sizeof(struct ieee80211_qos_parameters);
	u8  u1bAIFS;
	u32 u4bAcParam;
        int i;

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
		printk("===>u4bAcParam:%x, ", u4bAcParam);
		write_nic_dword(dev, WDCAPARA_ADD[i], u4bAcParam);
		//write_nic_dword(dev, WDCAPARA_ADD[i], 0x005e4332);
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
                              struct ieee80211_beacon * beacon,
                              struct ieee80211_network * network)
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
#if 0
		if((priv->ieee80211->current_network.qos_data.param_count != \
					network->qos_data.param_count))
#endif
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


//updateRATRTabel for MCS only. Basic rate is not implement.
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
static bool GetNmodeSupportBySecCfg8190Pci(struct net_device*dev)
{
#if 1
	struct r8192_priv* priv = ieee80211_priv(dev);
	struct ieee80211_device* ieee = priv->ieee80211;
        int wpa_ie_len= ieee->wpa_ie_len;
        struct ieee80211_crypt_data* crypt;
        int encrypt;

        crypt = ieee->crypt[ieee->tx_keyidx];
        encrypt = (ieee->current_network.capability & WLAN_CAPABILITY_PRIVACY) || (ieee->host_encrypt && crypt && crypt->ops && (0 == strcmp(crypt->ops->name,"WEP")));

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
		//RT_TRACE(COMP_ERR,"In %s The GroupEncAlgorithm is [4]\n",__FUNCTION__ );
		return true;
	}

#if 0
        //In here we discuss with SD4 David. He think we still can send TKIP in broadcast group key in MCS rate.
        //We can't force in G mode if Pairwie key is AES and group key is TKIP
        if((pSecInfo->GroupEncAlgorithm == WEP104_Encryption) || (pSecInfo->GroupEncAlgorithm == WEP40_Encryption)  ||
           (pSecInfo->PairwiseEncAlgorithm == WEP104_Encryption) ||
           (pSecInfo->PairwiseEncAlgorithm == WEP40_Encryption) || (pSecInfo->PairwiseEncAlgorithm == TKIP_Encryption))
        {
                return  false;
        }
        else
                return true;
#endif
	return true;
#endif
}

static void rtl8192_refresh_supportrate(struct r8192_priv* priv)
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

static u8 rtl8192_getSupportedWireleeMode(struct net_device*dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8 ret = 0;
	switch(priv->rf_chip)
	{
		case RF_8225:
		case RF_8256:
		case RF_PSEUDO_11N:
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

static void rtl8192_SetWirelessMode(struct net_device* dev, u8 wireless_mode)
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
	priv->ieee80211->mode = wireless_mode;

	if ((wireless_mode == WIRELESS_MODE_N_24G) ||  (wireless_mode == WIRELESS_MODE_N_5G))
		priv->ieee80211->pHTInfo->bEnableHT = 1;
	else
		priv->ieee80211->pHTInfo->bEnableHT = 0;
	RT_TRACE(COMP_INIT, "Current Wireless Mode is %x\n", wireless_mode);
	rtl8192_refresh_supportrate(priv);
#endif

}
//init priv variables here

static bool GetHalfNmodeSupportByAPs819xPci(struct net_device* dev)
{
	bool			Reval;
	struct r8192_priv* priv = ieee80211_priv(dev);
	struct ieee80211_device* ieee = priv->ieee80211;

	if(ieee->bHalfWirelessN24GMode == true)
		Reval = true;
	else
		Reval =  false;

	return Reval;
}

short rtl8192_is_tx_queue_empty(struct net_device *dev)
{
	int i=0;
	struct r8192_priv *priv = ieee80211_priv(dev);
	for (i=0; i<=MGNT_QUEUE; i++)
	{
		if ((i== TXCMD_QUEUE) || (i == HCCA_QUEUE) )
			continue;
		if (skb_queue_len(&(&priv->tx_ring[i])->queue) > 0){
			printk("===>tx queue is not empty:%d, %d\n", i, skb_queue_len(&(&priv->tx_ring[i])->queue));
			return 0;
		}
	}
	return 1;
}
static void rtl8192_hw_sleep_down(struct net_device *dev)
{
	RT_TRACE(COMP_POWER, "%s()============>come to sleep down\n", __FUNCTION__);
	MgntActSet_RF_State(dev, eRfSleep, RF_CHANGE_BY_PS);
}
static void rtl8192_hw_sleep_wq (struct work_struct *work)
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
static void rtl8192_hw_wakeup(struct net_device* dev)
{
//	u32 flags = 0;

//	spin_lock_irqsave(&priv->ps_lock,flags);
	RT_TRACE(COMP_POWER, "%s()============>come to wake up\n", __FUNCTION__);
	MgntActSet_RF_State(dev, eRfOn, RF_CHANGE_BY_PS);
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
static void rtl8192_hw_to_sleep(struct net_device *dev, u32 th, u32 tl)
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
static void rtl8192_init_priv_variable(struct net_device* dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	u8 i;
	priv->being_init_adapter = false;
	priv->txbuffsize = 1600;//1024;
	priv->txfwbuffersize = 4096;
	priv->txringcount = 64;//32;
	//priv->txbeaconcount = priv->txringcount;
	priv->txbeaconcount = 2;
	priv->rxbuffersize = 9100;//2048;//1024;
	priv->rxringcount = MAX_RX_COUNT;//64;
	priv->irq_enabled=0;
	priv->card_8192 = NIC_8192E;
	priv->rx_skb_complete = 1;
	priv->chan = 1; //set to channel 1
	priv->RegWirelessMode = WIRELESS_MODE_AUTO;
	priv->RegChannelPlan = 0xf;
	priv->nrxAMPDU_size = 0;
	priv->nrxAMPDU_aggr_num = 0;
	priv->last_rxdesc_tsf_high = 0;
	priv->last_rxdesc_tsf_low = 0;
	priv->ieee80211->mode = WIRELESS_MODE_AUTO; //SET AUTO
	priv->ieee80211->iw_mode = IW_MODE_INFRA;
	priv->ieee80211->ieee_up=0;
	priv->retry_rts = DEFAULT_RETRY_RTS;
	priv->retry_data = DEFAULT_RETRY_DATA;
	priv->ieee80211->rts = DEFAULT_RTS_THRESHOLD;
	priv->ieee80211->rate = 110; //11 mbps
	priv->ieee80211->short_slot = 1;
	priv->promisc = (dev->flags & IFF_PROMISC) ? 1:0;
	priv->bcck_in_ch14 = false;
	priv->bfsync_processing  = false;
	priv->CCKPresentAttentuation = 0;
	priv->rfa_txpowertrackingindex = 0;
	priv->rfc_txpowertrackingindex = 0;
	priv->CckPwEnl = 6;
	priv->ScanDelay = 50;//for Scan TODO
	//added by amy for silent reset
	priv->ResetProgress = RESET_TYPE_NORESET;
	priv->bForcedSilentReset = 0;
	priv->bDisableNormalResetCheck = false;
	priv->force_reset = false;
	//added by amy for power save
	priv->RegRfOff = 0;
	priv->ieee80211->RfOffReason = 0;
	priv->RFChangeInProgress = false;
	priv->bHwRfOffAction = 0;
	priv->SetRFPowerStateInProgress = false;
	priv->ieee80211->PowerSaveControl.bInactivePs = true;
	priv->ieee80211->PowerSaveControl.bIPSModeBackup = false;
	//just for debug
	priv->txpower_checkcnt = 0;
	priv->thermal_readback_index =0;
	priv->txpower_tracking_callback_cnt = 0;
	priv->ccktxpower_adjustcnt_ch14 = 0;
	priv->ccktxpower_adjustcnt_not_ch14 = 0;

	priv->ieee80211->current_network.beacon_interval = DEFAULT_BEACONINTERVAL;
	priv->ieee80211->iw_mode = IW_MODE_INFRA;
	priv->ieee80211->softmac_features  = IEEE_SOFTMAC_SCAN |
		IEEE_SOFTMAC_ASSOCIATE | IEEE_SOFTMAC_PROBERQ |
		IEEE_SOFTMAC_PROBERS | IEEE_SOFTMAC_TX_QUEUE;/* |
		IEEE_SOFTMAC_BEACONS;*///added by amy 080604 //|  //IEEE_SOFTMAC_SINGLE_QUEUE;

	priv->ieee80211->active_scan = 1;
	priv->ieee80211->modulation = IEEE80211_CCK_MODULATION | IEEE80211_OFDM_MODULATION;
	priv->ieee80211->host_encrypt = 1;
	priv->ieee80211->host_decrypt = 1;
	//priv->ieee80211->start_send_beacons = NULL;//rtl819xusb_beacon_tx;//-by amy 080604
	//priv->ieee80211->stop_send_beacons = NULL;//rtl8192_beacon_stop;//-by amy 080604
	priv->ieee80211->start_send_beacons = rtl8192_start_beacon;//+by david 081107
	priv->ieee80211->stop_send_beacons = rtl8192_stop_beacon;//+by david 081107
	priv->ieee80211->softmac_hard_start_xmit = rtl8192_hard_start_xmit;
	priv->ieee80211->set_chan = rtl8192_set_chan;
	priv->ieee80211->link_change = rtl8192_link_change;
	priv->ieee80211->softmac_data_hard_start_xmit = rtl8192_hard_data_xmit;
	priv->ieee80211->data_hard_stop = rtl8192_data_hard_stop;
	priv->ieee80211->data_hard_resume = rtl8192_data_hard_resume;
	priv->ieee80211->init_wmmparam_flag = 0;
	priv->ieee80211->fts = DEFAULT_FRAG_THRESHOLD;
	priv->ieee80211->check_nic_enough_desc = check_nic_enough_desc;
	priv->ieee80211->tx_headroom = sizeof(TX_FWINFO_8190PCI);
	priv->ieee80211->qos_support = 1;
	priv->ieee80211->dot11PowerSaveMode = 0;
	//added by WB
//	priv->ieee80211->SwChnlByTimerHandler = rtl8192_phy_SwChnl;
	priv->ieee80211->SetBWModeHandler = rtl8192_SetBWMode;
	priv->ieee80211->handle_assoc_response = rtl8192_handle_assoc_response;
	priv->ieee80211->handle_beacon = rtl8192_handle_beacon;

	priv->ieee80211->sta_wake_up = rtl8192_hw_wakeup;
//	priv->ieee80211->ps_request_tx_ack = rtl8192_rq_tx_ack;
	priv->ieee80211->enter_sleep_state = rtl8192_hw_to_sleep;
	priv->ieee80211->ps_is_queue_empty = rtl8192_is_tx_queue_empty;
	//added by david
	priv->ieee80211->GetNmodeSupportBySecCfg = GetNmodeSupportBySecCfg8190Pci;
	priv->ieee80211->SetWirelessMode = rtl8192_SetWirelessMode;
	priv->ieee80211->GetHalfNmodeSupportByAPsHandler = GetHalfNmodeSupportByAPs819xPci;

	//added by amy
	priv->ieee80211->InitialGainHandler = InitialGain819xPci;

	priv->card_type = USB;
	{
		priv->ShortRetryLimit = 0x30;
		priv->LongRetryLimit = 0x30;
	}
	priv->EarlyRxThreshold = 7;
	priv->enable_gpio0 = 0;

	priv->TransmitConfig = 0;

	priv->ReceiveConfig = RCR_ADD3	|
		RCR_AMF | RCR_ADF |		//accept management/data
		RCR_AICV |			//accept control frame for SW AP needs PS-poll, 2005.07.07, by rcnjko.
		RCR_AB | RCR_AM | RCR_APM |	//accept BC/MC/UC
		RCR_AAP | ((u32)7<<RCR_MXDMA_OFFSET) |
		((u32)7 << RCR_FIFO_OFFSET) | RCR_ONLYERLPKT;

	priv->irq_mask = 	(u32)(IMR_ROK | IMR_VODOK | IMR_VIDOK | IMR_BEDOK | IMR_BKDOK |\
				IMR_HCCADOK | IMR_MGNTDOK | IMR_COMDOK | IMR_HIGHDOK |\
				IMR_BDOK | IMR_RXCMDOK | IMR_TIMEOUT0 | IMR_RDU | IMR_RXFOVW	|\
				IMR_TXFOVW | IMR_BcnInt | IMR_TBDOK | IMR_TBDER);

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
	priv->rf_set_chan = rtl8192_phy_SwChnl;
}

//init lock here
static void rtl8192_init_priv_lock(struct r8192_priv* priv)
{
	spin_lock_init(&priv->tx_lock);
	spin_lock_init(&priv->irq_lock);//added by thomas
	spin_lock_init(&priv->irq_th_lock);
	spin_lock_init(&priv->rf_ps_lock);
	spin_lock_init(&priv->ps_lock);
	//spin_lock_init(&priv->rf_lock);
	sema_init(&priv->wx_sem,1);
	sema_init(&priv->rf_sem,1);
	mutex_init(&priv->mutex);
}

extern  void    rtl819x_watchdog_wqcallback(struct work_struct *work);

void rtl8192_irq_rx_tasklet(struct r8192_priv *priv);
void rtl8192_irq_tx_tasklet(struct r8192_priv *priv);
void rtl8192_prepare_beacon(struct r8192_priv *priv);
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

//	INIT_WORK(&priv->reset_wq, (void(*)(void*)) rtl8192_restart);
	INIT_WORK(&priv->reset_wq,  rtl8192_restart);
//	INIT_DELAYED_WORK(&priv->watch_dog_wq, hal_dm_watchdog);
	INIT_DELAYED_WORK(&priv->watch_dog_wq, rtl819x_watchdog_wqcallback);
	INIT_DELAYED_WORK(&priv->txpower_tracking_wq,  dm_txpower_trackingcallback);
	INIT_DELAYED_WORK(&priv->rfpath_check_wq,  dm_rf_pathcheck_workitemcallback);
	INIT_DELAYED_WORK(&priv->update_beacon_wq, rtl8192_update_beacon);
	//INIT_WORK(&priv->SwChnlWorkItem,  rtl8192_SwChnl_WorkItem);
	//INIT_WORK(&priv->SetBWModeWorkItem,  rtl8192_SetBWModeWorkItem);
	INIT_WORK(&priv->qos_activate, rtl8192_qos_activate);
	INIT_DELAYED_WORK(&priv->ieee80211->hw_wakeup_wq,(void*) rtl8192_hw_wakeup_wq);
	INIT_DELAYED_WORK(&priv->ieee80211->hw_sleep_wq,(void*) rtl8192_hw_sleep_wq);

	tasklet_init(&priv->irq_rx_tasklet,
	     (void(*)(unsigned long))rtl8192_irq_rx_tasklet,
	     (unsigned long)priv);
	tasklet_init(&priv->irq_tx_tasklet,
	     (void(*)(unsigned long))rtl8192_irq_tx_tasklet,
	     (unsigned long)priv);
        tasklet_init(&priv->irq_prepare_beacon_tasklet,
                (void(*)(unsigned long))rtl8192_prepare_beacon,
                (unsigned long)priv);
}

static void rtl8192_get_eeprom_size(struct net_device* dev)
{
	u16 curCR = 0;
	struct r8192_priv *priv = ieee80211_priv(dev);
	RT_TRACE(COMP_INIT, "===========>%s()\n", __FUNCTION__);
	curCR = read_nic_dword(dev, EPROM_CMD);
	RT_TRACE(COMP_INIT, "read from Reg Cmd9346CR(%x):%x\n", EPROM_CMD, curCR);
	//whether need I consider BIT5?
	priv->epromtype = (curCR & EPROM_CMD_9356SEL) ? EPROM_93c56 : EPROM_93c46;
	RT_TRACE(COMP_INIT, "<===========%s(), epromtype:%d\n", __FUNCTION__, priv->epromtype);
}

//used to swap endian. as ntohl & htonl are not neccessary to swap endian, so use this instead.
static inline u16 endian_swap(u16* data)
{
	u16 tmp = *data;
	*data = (tmp >> 8) | (tmp << 8);
	return *data;
}

/*
 *	Note:	Adapter->EEPROMAddressSize should be set before this function call.
 * 			EEPROM address size can be got through GetEEPROMSize8185()
*/
static void rtl8192_read_eeprom_info(struct net_device* dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);

	u8			tempval;
#ifdef RTL8192E
	u8			ICVer8192, ICVer8256;
#endif
	u16			i,usValue, IC_Version;
	u16			EEPROMId;
#ifdef RTL8190P
   	u8			offset;//, tmpAFR;
    	u8      		EepromTxPower[100];
#endif
	u8 bMac_Tmp_Addr[6] = {0x00, 0xe0, 0x4c, 0x00, 0x00, 0x01};
	RT_TRACE(COMP_INIT, "====> rtl8192_read_eeprom_info\n");


	// TODO: I don't know if we need to apply EF function to EEPROM read function

	//2 Read EEPROM ID to make sure autoload is success
	EEPROMId = eprom_read(dev, 0);
	if( EEPROMId != RTL8190_EEPROM_ID )
	{
		RT_TRACE(COMP_ERR, "EEPROM ID is invalid:%x, %x\n", EEPROMId, RTL8190_EEPROM_ID);
		priv->AutoloadFailFlag=true;
	}
	else
	{
		priv->AutoloadFailFlag=false;
	}

	//
	// Assign Chip Version ID
	//
	// Read IC Version && Channel Plan
	if(!priv->AutoloadFailFlag)
	{
		// VID, PID
		priv->eeprom_vid = eprom_read(dev, (EEPROM_VID >> 1));
		priv->eeprom_did = eprom_read(dev, (EEPROM_DID >> 1));

		usValue = eprom_read(dev, (u16)(EEPROM_Customer_ID>>1)) >> 8 ;
		priv->eeprom_CustomerID = (u8)( usValue & 0xff);
		usValue = eprom_read(dev, (EEPROM_ICVersion_ChannelPlan>>1));
		priv->eeprom_ChannelPlan = usValue&0xff;
		IC_Version = ((usValue&0xff00)>>8);

#ifdef RTL8190P
		priv->card_8192_version = (VERSION_8190)(IC_Version);
#else
	#ifdef RTL8192E
		ICVer8192 = (IC_Version&0xf);		//bit0~3; 1:A cut, 2:B cut, 3:C cut...
		ICVer8256 = ((IC_Version&0xf0)>>4);//bit4~6, bit7 reserved for other RF chip; 1:A cut, 2:B cut, 3:C cut...
		RT_TRACE(COMP_INIT, "\nICVer8192 = 0x%x\n", ICVer8192);
		RT_TRACE(COMP_INIT, "\nICVer8256 = 0x%x\n", ICVer8256);
		if(ICVer8192 == 0x2)	//B-cut
		{
			if(ICVer8256 == 0x5) //E-cut
				priv->card_8192_version= VERSION_8190_BE;
		}
	#endif
#endif
		switch(priv->card_8192_version)
		{
			case VERSION_8190_BD:
			case VERSION_8190_BE:
				break;
			default:
				priv->card_8192_version = VERSION_8190_BD;
				break;
		}
		RT_TRACE(COMP_INIT, "\nIC Version = 0x%x\n", priv->card_8192_version);
	}
	else
	{
		priv->card_8192_version = VERSION_8190_BD;
		priv->eeprom_vid = 0;
		priv->eeprom_did = 0;
		priv->eeprom_CustomerID = 0;
		priv->eeprom_ChannelPlan = 0;
		RT_TRACE(COMP_INIT, "\nIC Version = 0x%x\n", 0xff);
	}

	RT_TRACE(COMP_INIT, "EEPROM VID = 0x%4x\n", priv->eeprom_vid);
	RT_TRACE(COMP_INIT, "EEPROM DID = 0x%4x\n", priv->eeprom_did);
	RT_TRACE(COMP_INIT,"EEPROM Customer ID: 0x%2x\n", priv->eeprom_CustomerID);

	//2 Read Permanent MAC address
	if(!priv->AutoloadFailFlag)
	{
		for(i = 0; i < 6; i += 2)
		{
			usValue = eprom_read(dev, (u16) ((EEPROM_NODE_ADDRESS_BYTE_0+i)>>1));
			*(u16*)(&dev->dev_addr[i]) = usValue;
		}
	} else {
		// when auto load failed,  the last address byte set to be a random one.
		// added by david woo.2007/11/7
		memcpy(dev->dev_addr, bMac_Tmp_Addr, 6);
	}

	RT_TRACE(COMP_INIT, "Permanent Address = %02x-%02x-%02x-%02x-%02x-%02x\n",
			dev->dev_addr[0], dev->dev_addr[1],
			dev->dev_addr[2], dev->dev_addr[3],
			dev->dev_addr[4], dev->dev_addr[5]);

		//2 TX Power Check EEPROM Fail or not
	if(priv->card_8192_version > VERSION_8190_BD) {
		priv->bTXPowerDataReadFromEEPORM = true;
	} else {
		priv->bTXPowerDataReadFromEEPORM = false;
	}

	// 2007/11/15 MH 8190PCI Default=2T4R, 8192PCIE dafault=1T2R
	priv->rf_type = RTL819X_DEFAULT_RF_TYPE;

	if(priv->card_8192_version > VERSION_8190_BD)
	{
		// Read RF-indication and Tx Power gain index diff of legacy to HT OFDM rate.
		if(!priv->AutoloadFailFlag)
		{
			tempval = (eprom_read(dev, (EEPROM_RFInd_PowerDiff>>1))) & 0xff;
			priv->EEPROMLegacyHTTxPowerDiff = tempval & 0xf;	// bit[3:0]

			if (tempval&0x80)	//RF-indication, bit[7]
				priv->rf_type = RF_1T2R;
			else
				priv->rf_type = RF_2T4R;
		}
		else
		{
			priv->EEPROMLegacyHTTxPowerDiff = EEPROM_Default_LegacyHTTxPowerDiff;
		}
		RT_TRACE(COMP_INIT, "EEPROMLegacyHTTxPowerDiff = %d\n",
			priv->EEPROMLegacyHTTxPowerDiff);

		// Read ThermalMeter from EEPROM
		if(!priv->AutoloadFailFlag)
		{
			priv->EEPROMThermalMeter = (u8)(((eprom_read(dev, (EEPROM_ThermalMeter>>1))) & 0xff00)>>8);
		}
		else
		{
			priv->EEPROMThermalMeter = EEPROM_Default_ThermalMeter;
		}
		RT_TRACE(COMP_INIT, "ThermalMeter = %d\n", priv->EEPROMThermalMeter);
		//vivi, for tx power track
		priv->TSSI_13dBm = priv->EEPROMThermalMeter *100;

		if(priv->epromtype == EPROM_93c46)
		{
		// Read antenna tx power offset of B/C/D to A and CrystalCap from EEPROM
		if(!priv->AutoloadFailFlag)
		{
				usValue = eprom_read(dev, (EEPROM_TxPwDiff_CrystalCap>>1));
				priv->EEPROMAntPwDiff = (usValue&0x0fff);
				priv->EEPROMCrystalCap = (u8)((usValue&0xf000)>>12);
		}
		else
		{
				priv->EEPROMAntPwDiff = EEPROM_Default_AntTxPowerDiff;
				priv->EEPROMCrystalCap = EEPROM_Default_TxPwDiff_CrystalCap;
		}
			RT_TRACE(COMP_INIT, "EEPROMAntPwDiff = %d\n", priv->EEPROMAntPwDiff);
			RT_TRACE(COMP_INIT, "EEPROMCrystalCap = %d\n", priv->EEPROMCrystalCap);

		//
		// Get per-channel Tx Power Level
		//
		for(i=0; i<14; i+=2)
		{
			if(!priv->AutoloadFailFlag)
			{
				usValue = eprom_read(dev, (u16) ((EEPROM_TxPwIndex_CCK+i)>>1) );
			}
			else
			{
				usValue = EEPROM_Default_TxPower;
			}
			*((u16*)(&priv->EEPROMTxPowerLevelCCK[i])) = usValue;
			RT_TRACE(COMP_INIT,"CCK Tx Power Level, Index %d = 0x%02x\n", i, priv->EEPROMTxPowerLevelCCK[i]);
			RT_TRACE(COMP_INIT, "CCK Tx Power Level, Index %d = 0x%02x\n", i+1, priv->EEPROMTxPowerLevelCCK[i+1]);
		}
		for(i=0; i<14; i+=2)
		{
			if(!priv->AutoloadFailFlag)
			{
				usValue = eprom_read(dev, (u16) ((EEPROM_TxPwIndex_OFDM_24G+i)>>1) );
			}
			else
			{
				usValue = EEPROM_Default_TxPower;
			}
			*((u16*)(&priv->EEPROMTxPowerLevelOFDM24G[i])) = usValue;
			RT_TRACE(COMP_INIT, "OFDM 2.4G Tx Power Level, Index %d = 0x%02x\n", i, priv->EEPROMTxPowerLevelOFDM24G[i]);
			RT_TRACE(COMP_INIT, "OFDM 2.4G Tx Power Level, Index %d = 0x%02x\n", i+1, priv->EEPROMTxPowerLevelOFDM24G[i+1]);
		}
		}
		else if(priv->epromtype== EPROM_93c56)
		{
		#ifdef RTL8190P
			// Read CrystalCap from EEPROM
			if(!priv->AutoloadFailFlag)
			{
				priv->EEPROMAntPwDiff = EEPROM_Default_AntTxPowerDiff;
				priv->EEPROMCrystalCap = (u8)(((eprom_read(dev, (EEPROM_C56_CrystalCap>>1))) & 0xf000)>>12);
			}
			else
			{
				priv->EEPROMAntPwDiff = EEPROM_Default_AntTxPowerDiff;
				priv->EEPROMCrystalCap = EEPROM_Default_TxPwDiff_CrystalCap;
			}
			RT_TRACE(COMP_INIT,"EEPROMAntPwDiff = %d\n", priv->EEPROMAntPwDiff);
			RT_TRACE(COMP_INIT, "EEPROMCrystalCap = %d\n", priv->EEPROMCrystalCap);

			// Get Tx Power Level by Channel
			if(!priv->AutoloadFailFlag)
			{
				    // Read Tx power of Channel 1 ~ 14 from EEPROM.
			       for(i = 0; i < 12; i+=2)
				{
					if (i <6)
						offset = EEPROM_C56_RfA_CCK_Chnl1_TxPwIndex + i;
					else
						offset = EEPROM_C56_RfC_CCK_Chnl1_TxPwIndex + i - 6;
					usValue = eprom_read(dev, (offset>>1));
				       *((u16*)(&EepromTxPower[i])) = usValue;
				}

			       for(i = 0; i < 12; i++)
			       	{
			       		if (i <= 2)
						priv->EEPROMRfACCKChnl1TxPwLevel[i] = EepromTxPower[i];
					else if ((i >=3 )&&(i <= 5))
						priv->EEPROMRfAOfdmChnlTxPwLevel[i-3] = EepromTxPower[i];
					else if ((i >=6 )&&(i <= 8))
						priv->EEPROMRfCCCKChnl1TxPwLevel[i-6] = EepromTxPower[i];
					else
						priv->EEPROMRfCOfdmChnlTxPwLevel[i-9] = EepromTxPower[i];
				}
			}
			else
			{
				priv->EEPROMRfACCKChnl1TxPwLevel[0] = EEPROM_Default_TxPowerLevel;
				priv->EEPROMRfACCKChnl1TxPwLevel[1] = EEPROM_Default_TxPowerLevel;
				priv->EEPROMRfACCKChnl1TxPwLevel[2] = EEPROM_Default_TxPowerLevel;

				priv->EEPROMRfAOfdmChnlTxPwLevel[0] = EEPROM_Default_TxPowerLevel;
				priv->EEPROMRfAOfdmChnlTxPwLevel[1] = EEPROM_Default_TxPowerLevel;
				priv->EEPROMRfAOfdmChnlTxPwLevel[2] = EEPROM_Default_TxPowerLevel;

				priv->EEPROMRfCCCKChnl1TxPwLevel[0] = EEPROM_Default_TxPowerLevel;
				priv->EEPROMRfCCCKChnl1TxPwLevel[1] = EEPROM_Default_TxPowerLevel;
				priv->EEPROMRfCCCKChnl1TxPwLevel[2] = EEPROM_Default_TxPowerLevel;

				priv->EEPROMRfCOfdmChnlTxPwLevel[0] = EEPROM_Default_TxPowerLevel;
				priv->EEPROMRfCOfdmChnlTxPwLevel[1] = EEPROM_Default_TxPowerLevel;
				priv->EEPROMRfCOfdmChnlTxPwLevel[2] = EEPROM_Default_TxPowerLevel;
			}
			RT_TRACE(COMP_INIT, "priv->EEPROMRfACCKChnl1TxPwLevel[0] = 0x%x\n", priv->EEPROMRfACCKChnl1TxPwLevel[0]);
			RT_TRACE(COMP_INIT, "priv->EEPROMRfACCKChnl1TxPwLevel[1] = 0x%x\n", priv->EEPROMRfACCKChnl1TxPwLevel[1]);
			RT_TRACE(COMP_INIT, "priv->EEPROMRfACCKChnl1TxPwLevel[2] = 0x%x\n", priv->EEPROMRfACCKChnl1TxPwLevel[2]);
			RT_TRACE(COMP_INIT, "priv->EEPROMRfAOfdmChnlTxPwLevel[0] = 0x%x\n", priv->EEPROMRfAOfdmChnlTxPwLevel[0]);
			RT_TRACE(COMP_INIT, "priv->EEPROMRfAOfdmChnlTxPwLevel[1] = 0x%x\n", priv->EEPROMRfAOfdmChnlTxPwLevel[1]);
			RT_TRACE(COMP_INIT, "priv->EEPROMRfAOfdmChnlTxPwLevel[2] = 0x%x\n", priv->EEPROMRfAOfdmChnlTxPwLevel[2]);
			RT_TRACE(COMP_INIT, "priv->EEPROMRfCCCKChnl1TxPwLevel[0] = 0x%x\n", priv->EEPROMRfCCCKChnl1TxPwLevel[0]);
			RT_TRACE(COMP_INIT, "priv->EEPROMRfCCCKChnl1TxPwLevel[1] = 0x%x\n", priv->EEPROMRfCCCKChnl1TxPwLevel[1]);
			RT_TRACE(COMP_INIT, "priv->EEPROMRfCCCKChnl1TxPwLevel[2] = 0x%x\n", priv->EEPROMRfCCCKChnl1TxPwLevel[2]);
			RT_TRACE(COMP_INIT, "priv->EEPROMRfCOfdmChnlTxPwLevel[0] = 0x%x\n", priv->EEPROMRfCOfdmChnlTxPwLevel[0]);
			RT_TRACE(COMP_INIT, "priv->EEPROMRfCOfdmChnlTxPwLevel[1] = 0x%x\n", priv->EEPROMRfCOfdmChnlTxPwLevel[1]);
			RT_TRACE(COMP_INIT, "priv->EEPROMRfCOfdmChnlTxPwLevel[2] = 0x%x\n", priv->EEPROMRfCOfdmChnlTxPwLevel[2]);
#endif

		}
		//
		// Update HAL variables.
		//
		if(priv->epromtype == EPROM_93c46)
		{
			for(i=0; i<14; i++)
			{
				priv->TxPowerLevelCCK[i] = priv->EEPROMTxPowerLevelCCK[i];
				priv->TxPowerLevelOFDM24G[i] = priv->EEPROMTxPowerLevelOFDM24G[i];
			}
			priv->LegacyHTTxPowerDiff = priv->EEPROMLegacyHTTxPowerDiff;
		// Antenna B gain offset to antenna A, bit0~3
			priv->AntennaTxPwDiff[0] = (priv->EEPROMAntPwDiff & 0xf);
		// Antenna C gain offset to antenna A, bit4~7
			priv->AntennaTxPwDiff[1] = ((priv->EEPROMAntPwDiff & 0xf0)>>4);
		// Antenna D gain offset to antenna A, bit8~11
			priv->AntennaTxPwDiff[2] = ((priv->EEPROMAntPwDiff & 0xf00)>>8);
		// CrystalCap, bit12~15
			priv->CrystalCap = priv->EEPROMCrystalCap;
		// ThermalMeter, bit0~3 for RFIC1, bit4~7 for RFIC2
			priv->ThermalMeter[0] = (priv->EEPROMThermalMeter & 0xf);
			priv->ThermalMeter[1] = ((priv->EEPROMThermalMeter & 0xf0)>>4);
		}
		else if(priv->epromtype == EPROM_93c56)
		{
			//char	cck_pwr_diff_a=0, cck_pwr_diff_c=0;

			//cck_pwr_diff_a = pHalData->EEPROMRfACCKChnl7TxPwLevel - pHalData->EEPROMRfAOfdmChnlTxPwLevel[1];
			//cck_pwr_diff_c = pHalData->EEPROMRfCCCKChnl7TxPwLevel - pHalData->EEPROMRfCOfdmChnlTxPwLevel[1];
			for(i=0; i<3; i++)	// channel 1~3 use the same Tx Power Level.
			{
				priv->TxPowerLevelCCK_A[i]  = priv->EEPROMRfACCKChnl1TxPwLevel[0];
				priv->TxPowerLevelOFDM24G_A[i] = priv->EEPROMRfAOfdmChnlTxPwLevel[0];
				priv->TxPowerLevelCCK_C[i] =  priv->EEPROMRfCCCKChnl1TxPwLevel[0];
				priv->TxPowerLevelOFDM24G_C[i] = priv->EEPROMRfCOfdmChnlTxPwLevel[0];
			}
			for(i=3; i<9; i++)	// channel 4~9 use the same Tx Power Level
			{
				priv->TxPowerLevelCCK_A[i]  = priv->EEPROMRfACCKChnl1TxPwLevel[1];
				priv->TxPowerLevelOFDM24G_A[i] = priv->EEPROMRfAOfdmChnlTxPwLevel[1];
				priv->TxPowerLevelCCK_C[i] =  priv->EEPROMRfCCCKChnl1TxPwLevel[1];
				priv->TxPowerLevelOFDM24G_C[i] = priv->EEPROMRfCOfdmChnlTxPwLevel[1];
			}
			for(i=9; i<14; i++)	// channel 10~14 use the same Tx Power Level
			{
				priv->TxPowerLevelCCK_A[i]  = priv->EEPROMRfACCKChnl1TxPwLevel[2];
				priv->TxPowerLevelOFDM24G_A[i] = priv->EEPROMRfAOfdmChnlTxPwLevel[2];
				priv->TxPowerLevelCCK_C[i] =  priv->EEPROMRfCCCKChnl1TxPwLevel[2];
				priv->TxPowerLevelOFDM24G_C[i] = priv->EEPROMRfCOfdmChnlTxPwLevel[2];
			}
			for(i=0; i<14; i++)
				RT_TRACE(COMP_INIT, "priv->TxPowerLevelCCK_A[%d] = 0x%x\n", i, priv->TxPowerLevelCCK_A[i]);
			for(i=0; i<14; i++)
				RT_TRACE(COMP_INIT,"priv->TxPowerLevelOFDM24G_A[%d] = 0x%x\n", i, priv->TxPowerLevelOFDM24G_A[i]);
			for(i=0; i<14; i++)
				RT_TRACE(COMP_INIT, "priv->TxPowerLevelCCK_C[%d] = 0x%x\n", i, priv->TxPowerLevelCCK_C[i]);
			for(i=0; i<14; i++)
				RT_TRACE(COMP_INIT, "priv->TxPowerLevelOFDM24G_C[%d] = 0x%x\n", i, priv->TxPowerLevelOFDM24G_C[i]);
			priv->LegacyHTTxPowerDiff = priv->EEPROMLegacyHTTxPowerDiff;
			priv->AntennaTxPwDiff[0] = 0;
			priv->AntennaTxPwDiff[1] = 0;
			priv->AntennaTxPwDiff[2] = 0;
			priv->CrystalCap = priv->EEPROMCrystalCap;
			// ThermalMeter, bit0~3 for RFIC1, bit4~7 for RFIC2
			priv->ThermalMeter[0] = (priv->EEPROMThermalMeter & 0xf);
			priv->ThermalMeter[1] = ((priv->EEPROMThermalMeter & 0xf0)>>4);
		}
	}

	if(priv->rf_type == RF_1T2R)
	{
		RT_TRACE(COMP_INIT, "\n1T2R config\n");
	}
	else if (priv->rf_type == RF_2T4R)
	{
		RT_TRACE(COMP_INIT, "\n2T4R config\n");
	}

	// 2008/01/16 MH We can only know RF type in the function. So we have to init
	// DIG RATR table again.
	init_rate_adaptive(dev);

	//1 Make a copy for following variables and we can change them if we want

	priv->rf_chip= RF_8256;

	if(priv->RegChannelPlan == 0xf)
	{
		priv->ChannelPlan = priv->eeprom_ChannelPlan;
	}
	else
	{
		priv->ChannelPlan = priv->RegChannelPlan;
	}

	//
	//  Used PID and DID to Set CustomerID
	//
	if( priv->eeprom_vid == 0x1186 &&  priv->eeprom_did == 0x3304 )
	{
		priv->CustomerID =  RT_CID_DLINK;
	}

	switch(priv->eeprom_CustomerID)
	{
		case EEPROM_CID_DEFAULT:
			priv->CustomerID = RT_CID_DEFAULT;
			break;
		case EEPROM_CID_CAMEO:
			priv->CustomerID = RT_CID_819x_CAMEO;
			break;
		case  EEPROM_CID_RUNTOP:
			priv->CustomerID = RT_CID_819x_RUNTOP;
			break;
		case EEPROM_CID_NetCore:
			priv->CustomerID = RT_CID_819x_Netcore;
			break;
		case EEPROM_CID_TOSHIBA:        // Merge by Jacken, 2008/01/31
			priv->CustomerID = RT_CID_TOSHIBA;
			if(priv->eeprom_ChannelPlan&0x80)
				priv->ChannelPlan = priv->eeprom_ChannelPlan&0x7f;
			else
				priv->ChannelPlan = 0x0;
			RT_TRACE(COMP_INIT, "Toshiba ChannelPlan = 0x%x\n",
				priv->ChannelPlan);
			break;
		case EEPROM_CID_Nettronix:
			priv->ScanDelay = 100;	//cosa add for scan
			priv->CustomerID = RT_CID_Nettronix;
			break;
		case EEPROM_CID_Pronet:
			priv->CustomerID = RT_CID_PRONET;
			break;
		case EEPROM_CID_DLINK:
			priv->CustomerID = RT_CID_DLINK;
			break;

		case EEPROM_CID_WHQL:
			//Adapter->bInHctTest = TRUE;//do not supported

			//priv->bSupportTurboMode = FALSE;
			//priv->bAutoTurboBy8186 = FALSE;

			//pMgntInfo->PowerSaveControl.bInactivePs = FALSE;
			//pMgntInfo->PowerSaveControl.bIPSModeBackup = FALSE;
			//pMgntInfo->PowerSaveControl.bLeisurePs = FALSE;

			break;
		default:
			// value from RegCustomerID
			break;
	}

	//Avoid the channel plan array overflow, by Bruce, 2007-08-27.
	if(priv->ChannelPlan > CHANNEL_PLAN_LEN - 1)
		priv->ChannelPlan = 0; //FCC

	switch(priv->CustomerID)
	{
		case RT_CID_DEFAULT:
		#ifdef RTL8190P
			priv->LedStrategy = HW_LED;
		#else
			#ifdef RTL8192E
			priv->LedStrategy = SW_LED_MODE1;
			#endif
		#endif
			break;

		case RT_CID_819x_CAMEO:
			priv->LedStrategy = SW_LED_MODE2;
			break;

		case RT_CID_819x_RUNTOP:
			priv->LedStrategy = SW_LED_MODE3;
			break;

		case RT_CID_819x_Netcore:
			priv->LedStrategy = SW_LED_MODE4;
			break;

		case RT_CID_Nettronix:
			priv->LedStrategy = SW_LED_MODE5;
			break;

		case RT_CID_PRONET:
			priv->LedStrategy = SW_LED_MODE6;
			break;

		case RT_CID_TOSHIBA:   //Modify by Jacken 2008/01/31
			// Do nothing.
			//break;

		default:
		#ifdef RTL8190P
			priv->LedStrategy = HW_LED;
		#else
			#ifdef RTL8192E
			priv->LedStrategy = SW_LED_MODE1;
			#endif
		#endif
			break;
	}
/*
	//2008.06.03, for WOL
	if( priv->eeprom_vid == 0x1186 &&  priv->eeprom_did == 0x3304)
		priv->ieee80211->bSupportRemoteWakeUp = TRUE;
	else
		priv->ieee80211->bSupportRemoteWakeUp = FALSE;
*/
	RT_TRACE(COMP_INIT, "RegChannelPlan(%d)\n", priv->RegChannelPlan);
	RT_TRACE(COMP_INIT, "ChannelPlan = %d \n", priv->ChannelPlan);
	RT_TRACE(COMP_INIT, "LedStrategy = %d \n", priv->LedStrategy);
	RT_TRACE(COMP_TRACE, "<==== ReadAdapterInfo\n");

	return ;
}


static short rtl8192_get_channel_map(struct net_device * dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
#ifdef ENABLE_DOT11D
	if(priv->ChannelPlan> COUNTRY_CODE_GLOBAL_DOMAIN){
		printk("rtl8180_init:Error channel plan! Set to default.\n");
		priv->ChannelPlan= 0;
	}
	RT_TRACE(COMP_INIT, "Channel plan is %d\n",priv->ChannelPlan);

	rtl819x_set_channel_map(priv->ChannelPlan, priv);
#else
	int ch,i;
	//Set Default Channel Plan
	if(!channels){
		DMESG("No channels, aborting");
		return -1;
	}
	ch=channels;
	priv->ChannelPlan= 0;//hikaru
	 // set channels 1..14 allowed in given locale
	for (i=1; i<=14; i++) {
		(priv->ieee80211->channel_map)[i] = (u8)(ch & 0x01);
		ch >>= 1;
	}
#endif
	return 0;
}

static short rtl8192_init(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
	memset(&(priv->stats),0,sizeof(struct Stats));
	rtl8192_init_priv_variable(dev);
	rtl8192_init_priv_lock(priv);
	rtl8192_init_priv_task(dev);
	rtl8192_get_eeprom_size(dev);
	rtl8192_read_eeprom_info(dev);
	rtl8192_get_channel_map(dev);
	init_hal_dm(dev);
	init_timer(&priv->watch_dog_timer);
	priv->watch_dog_timer.data = (unsigned long)dev;
	priv->watch_dog_timer.function = watch_dog_timer_callback;
#if defined(IRQF_SHARED)
        if(request_irq(dev->irq, (void*)rtl8192_interrupt, IRQF_SHARED, dev->name, dev)){
#else
        if(request_irq(dev->irq, (void *)rtl8192_interrupt, SA_SHIRQ, dev->name, dev)){
#endif
		printk("Error allocating IRQ %d",dev->irq);
		return -1;
	}else{
		priv->irq=dev->irq;
		printk("IRQ %d",dev->irq);
	}
	if(rtl8192_pci_initdescring(dev)!=0){
		printk("Endopoints initialization failed");
		return -1;
	}

	//rtl8192_rx_enable(dev);
	//rtl8192_adapter_start(dev);
	return 0;
}

/******************************************************************************
 *function:  This function actually only set RRSR, RATR and BW_OPMODE registers
 *	     not to do all the hw config as its name says
 *   input:  net_device dev
 *  output:  none
 *  return:  none
 *  notice:  This part need to modified according to the rate set we filtered
 * ****************************************************************************/
static void rtl8192_hwconfig(struct net_device* dev)
{
	u32 regRATR = 0, regRRSR = 0;
	u8 regBwOpMode = 0, regTmp = 0;
	struct r8192_priv *priv = ieee80211_priv(dev);

// Set RRSR, RATR, and BW_OPMODE registers
	//
	switch(priv->ieee80211->mode)
	{
	case WIRELESS_MODE_B:
		regBwOpMode = BW_OPMODE_20MHZ;
		regRATR = RATE_ALL_CCK;
		regRRSR = RATE_ALL_CCK;
		break;
	case WIRELESS_MODE_A:
		regBwOpMode = BW_OPMODE_5G |BW_OPMODE_20MHZ;
		regRATR = RATE_ALL_OFDM_AG;
		regRRSR = RATE_ALL_OFDM_AG;
		break;
	case WIRELESS_MODE_G:
		regBwOpMode = BW_OPMODE_20MHZ;
		regRATR = RATE_ALL_CCK | RATE_ALL_OFDM_AG;
		regRRSR = RATE_ALL_CCK | RATE_ALL_OFDM_AG;
		break;
	case WIRELESS_MODE_AUTO:
	case WIRELESS_MODE_N_24G:
		// It support CCK rate by default.
		// CCK rate will be filtered out only when associated AP does not support it.
		regBwOpMode = BW_OPMODE_20MHZ;
			regRATR = RATE_ALL_CCK | RATE_ALL_OFDM_AG | RATE_ALL_OFDM_1SS | RATE_ALL_OFDM_2SS;
			regRRSR = RATE_ALL_CCK | RATE_ALL_OFDM_AG;
		break;
	case WIRELESS_MODE_N_5G:
		regBwOpMode = BW_OPMODE_5G;
		regRATR = RATE_ALL_OFDM_AG | RATE_ALL_OFDM_1SS | RATE_ALL_OFDM_2SS;
		regRRSR = RATE_ALL_OFDM_AG;
		break;
	}

	write_nic_byte(dev, BW_OPMODE, regBwOpMode);
	{
		u32 ratr_value = 0;
		ratr_value = regRATR;
		if (priv->rf_type == RF_1T2R)
		{
			ratr_value &= ~(RATE_ALL_OFDM_2SS);
		}
		write_nic_dword(dev, RATR0, ratr_value);
		write_nic_byte(dev, UFWP, 1);
	}
	regTmp = read_nic_byte(dev, 0x313);
	regRRSR = ((regTmp) << 24) | (regRRSR & 0x00ffffff);
	write_nic_dword(dev, RRSR, regRRSR);

	//
	// Set Retry Limit here
	//
	write_nic_word(dev, RETRY_LIMIT,
			priv->ShortRetryLimit << RETRY_LIMIT_SHORT_SHIFT | \
			priv->LongRetryLimit << RETRY_LIMIT_LONG_SHIFT);
	// Set Contention Window here

	// Set Tx AGC

	// Set Tx Antenna including Feedback control

	// Set Auto Rate fallback control


}


static RT_STATUS rtl8192_adapter_start(struct net_device *dev)
{
	struct r8192_priv *priv = ieee80211_priv(dev);
//	struct ieee80211_device *ieee = priv->ieee80211;
	u32 ulRegRead;
	RT_STATUS rtStatus = RT_STATUS_SUCCESS;
//	static char szMACPHYRegFile[] = RTL819X_PHY_MACPHY_REG;
//	static char szMACPHYRegPGFile[] = RTL819X_PHY_MACPHY_REG_PG;
	//u8 eRFPath;
	u8 tmpvalue;
#ifdef RTL8192E
	u8 ICVersion,SwitchingRegulatorOutput;
#endif
	bool bfirmwareok = true;
#ifdef RTL8190P
	u8 ucRegRead;
#endif
	u32	tmpRegA, tmpRegC, TempCCk;
	int	i =0;
//	u32 dwRegRead = 0;

	RT_TRACE(COMP_INIT, "====>%s()\n", __FUNCTION__);
	priv->being_init_adapter = true;
        rtl8192_pci_resetdescring(dev);
	// 2007/11/02 MH Before initalizing RF. We can not use FW to do RF-R/W.
	priv->Rf_Mode = RF_OP_By_SW_3wire;
#ifdef RTL8192E
        //dPLL on
        if(priv->ResetProgress == RESET_TYPE_NORESET)
        {
            write_nic_byte(dev, ANAPAR, 0x37);
            // Accordign to designer's explain, LBUS active will never > 10ms. We delay 10ms
            // Joseph increae the time to prevent firmware download fail
            mdelay(500);
        }
#endif
	//PlatformSleepUs(10000);
	// For any kind of InitializeAdapter process, we shall use system now!!
	priv->pFirmware->firmware_status = FW_STATUS_0_INIT;

	// Set to eRfoff in order not to count receive count.
	if(priv->RegRfOff == TRUE)
		priv->ieee80211->eRFPowerState = eRfOff;

	//
	//3 //Config CPUReset Register
	//3//
	//3 Firmware Reset Or Not
	ulRegRead = read_nic_dword(dev, CPU_GEN);
	if(priv->pFirmware->firmware_status == FW_STATUS_0_INIT)
	{	//called from MPInitialized. do nothing
		ulRegRead |= CPU_GEN_SYSTEM_RESET;
	}else if(priv->pFirmware->firmware_status == FW_STATUS_5_READY)
		ulRegRead |= CPU_GEN_FIRMWARE_RESET;	// Called from MPReset
	else
		RT_TRACE(COMP_ERR, "ERROR in %s(): undefined firmware state(%d)\n", __FUNCTION__,   priv->pFirmware->firmware_status);

#ifdef RTL8190P
	//2008.06.03, for WOL 90 hw bug
	ulRegRead &= (~(CPU_GEN_GPIO_UART));
#endif

	write_nic_dword(dev, CPU_GEN, ulRegRead);
	//mdelay(100);

#ifdef RTL8192E

	//3//
	//3 //Fix the issue of E-cut high temperature issue
	//3//
	// TODO: E cut only
	ICVersion = read_nic_byte(dev, IC_VERRSION);
	if(ICVersion >= 0x4) //E-cut only
	{
		// HW SD suggest that we should not wirte this register too often, so driver
		// should readback this register. This register will be modified only when
		// power on reset
		SwitchingRegulatorOutput = read_nic_byte(dev, SWREGULATOR);
		if(SwitchingRegulatorOutput  != 0xb8)
		{
			write_nic_byte(dev, SWREGULATOR, 0xa8);
			mdelay(1);
			write_nic_byte(dev, SWREGULATOR, 0xb8);
		}
	}
#endif


	//3//
	//3// Initialize BB before MAC
	//3//
	RT_TRACE(COMP_INIT, "BB Config Start!\n");
	rtStatus = rtl8192_BBConfig(dev);
	if(rtStatus != RT_STATUS_SUCCESS)
	{
		RT_TRACE(COMP_ERR, "BB Config failed\n");
		return rtStatus;
	}
	RT_TRACE(COMP_INIT,"BB Config Finished!\n");

	//3//Set Loopback mode or Normal mode
	//3//
	//2006.12.13 by emily. Note!We should not merge these two CPU_GEN register writings
	//	because setting of System_Reset bit reset MAC to default transmission mode.
		//Loopback mode or not
	priv->LoopbackMode = RTL819X_NO_LOOPBACK;
	//priv->LoopbackMode = RTL819X_MAC_LOOPBACK;
	if(priv->ResetProgress == RESET_TYPE_NORESET)
	{
	ulRegRead = read_nic_dword(dev, CPU_GEN);
	if(priv->LoopbackMode == RTL819X_NO_LOOPBACK)
	{
		ulRegRead = ((ulRegRead & CPU_GEN_NO_LOOPBACK_MSK) | CPU_GEN_NO_LOOPBACK_SET);
	}
	else if (priv->LoopbackMode == RTL819X_MAC_LOOPBACK )
	{
		ulRegRead |= CPU_CCK_LOOPBACK;
	}
	else
	{
		RT_TRACE(COMP_ERR,"Serious error: wrong loopback mode setting\n");
	}

	//2008.06.03, for WOL
	//ulRegRead &= (~(CPU_GEN_GPIO_UART));
	write_nic_dword(dev, CPU_GEN, ulRegRead);

	// 2006.11.29. After reset cpu, we sholud wait for a second, otherwise, it may fail to write registers. Emily
	udelay(500);
	}
	//3Set Hardware(Do nothing now)
	rtl8192_hwconfig(dev);
	//2=======================================================
	// Common Setting for all of the FPGA platform. (part 1)
	//2=======================================================
	// If there is changes, please make sure it applies to all of the FPGA version
	//3 Turn on Tx/Rx
	write_nic_byte(dev, CMDR, CR_RE|CR_TE);

	//2Set Tx dma burst
#ifdef RTL8190P
	write_nic_byte(dev, PCIF, ((MXDMA2_NoLimit<<MXDMA2_RX_SHIFT) | \
											(MXDMA2_NoLimit<<MXDMA2_TX_SHIFT) | \
											(1<<MULRW_SHIFT)));
#else
	#ifdef RTL8192E
	write_nic_byte(dev, PCIF, ((MXDMA2_NoLimit<<MXDMA2_RX_SHIFT) |\
				   (MXDMA2_NoLimit<<MXDMA2_TX_SHIFT) ));
	#endif
#endif
	//set IDR0 here
	write_nic_dword(dev, MAC0, ((u32*)dev->dev_addr)[0]);
	write_nic_word(dev, MAC4, ((u16*)(dev->dev_addr + 4))[0]);
	//set RCR
	write_nic_dword(dev, RCR, priv->ReceiveConfig);

	//3 Initialize Number of Reserved Pages in Firmware Queue
	#ifdef TO_DO_LIST
	if(priv->bInHctTest)
	{
		PlatformEFIOWrite4Byte(Adapter, RQPN1,  NUM_OF_PAGE_IN_FW_QUEUE_BK_DTM << RSVD_FW_QUEUE_PAGE_BK_SHIFT |\
                                       	NUM_OF_PAGE_IN_FW_QUEUE_BE_DTM << RSVD_FW_QUEUE_PAGE_BE_SHIFT | \
					NUM_OF_PAGE_IN_FW_QUEUE_VI_DTM << RSVD_FW_QUEUE_PAGE_VI_SHIFT | \
					NUM_OF_PAGE_IN_FW_QUEUE_VO_DTM <<RSVD_FW_QUEUE_PAGE_VO_SHIFT);
		PlatformEFIOWrite4Byte(Adapter, RQPN2, NUM_OF_PAGE_IN_FW_QUEUE_MGNT << RSVD_FW_QUEUE_PAGE_MGNT_SHIFT);
		PlatformEFIOWrite4Byte(Adapter, RQPN3, APPLIED_RESERVED_QUEUE_IN_FW| \
					NUM_OF_PAGE_IN_FW_QUEUE_BCN<<RSVD_FW_QUEUE_PAGE_BCN_SHIFT|\
					NUM_OF_PAGE_IN_FW_QUEUE_PUB_DTM<<RSVD_FW_QUEUE_PAGE_PUB_SHIFT);
	}
	else
	#endif
	{
		write_nic_dword(dev, RQPN1,  NUM_OF_PAGE_IN_FW_QUEUE_BK << RSVD_FW_QUEUE_PAGE_BK_SHIFT |\
					NUM_OF_PAGE_IN_FW_QUEUE_BE << RSVD_FW_QUEUE_PAGE_BE_SHIFT | \
					NUM_OF_PAGE_IN_FW_QUEUE_VI << RSVD_FW_QUEUE_PAGE_VI_SHIFT | \
					NUM_OF_PAGE_IN_FW_QUEUE_VO <<RSVD_FW_QUEUE_PAGE_VO_SHIFT);
		write_nic_dword(dev, RQPN2, NUM_OF_PAGE_IN_FW_QUEUE_MGNT << RSVD_FW_QUEUE_PAGE_MGNT_SHIFT);
		write_nic_dword(dev, RQPN3, APPLIED_RESERVED_QUEUE_IN_FW| \
					NUM_OF_PAGE_IN_FW_QUEUE_BCN<<RSVD_FW_QUEUE_PAGE_BCN_SHIFT|\
					NUM_OF_PAGE_IN_FW_QUEUE_PUB<<RSVD_FW_QUEUE_PAGE_PUB_SHIFT);
	}

	rtl8192_tx_enable(dev);
	rtl8192_rx_enable(dev);
	//3Set Response Rate Setting Register
	// CCK rate is supported by default.
	// CCK rate will be filtered out only when associated AP does not support it.
	ulRegRead = (0xFFF00000 & read_nic_dword(dev, RRSR))  | RATE_ALL_OFDM_AG | RATE_ALL_CCK;
	write_nic_dword(dev, RRSR, ulRegRead);
	write_nic_dword(dev, RATR0+4*7, (RATE_ALL_OFDM_AG | RATE_ALL_CCK));

	//2Set AckTimeout
	// TODO: (it value is only for FPGA version). need to be changed!!2006.12.18, by Emily
	write_nic_byte(dev, ACK_TIMEOUT, 0x30);

	//rtl8192_actset_wirelessmod******priv->RegW*
 * CoMode);
	if() 2008 -setProgress == RESET_TYPE_NOved.
)
	********Set2010 Realtek******) 2008ieee80211->pyri Cor//-driver, which is:
 * Copyright 2004-2005 Andrea Merello <andreamrl@tiscali.i180  Set up security related. 070106*****rcnjko:>, et1. Clear all H/W keys.>, et2. Enableit anencryption/dee terms  modidriver, which is:
 * Copyright 2004-2005 Andrea Merello <andreamrl@tiscali.it>,Cam All AllEntry**** Cor{
		u8 SECR_value = 0x0;
		ted in the |= SCR_TxEnct
 * uthat it will be usefulRxDet WITHOUT
 * ANY WARRANTY; wiNoSKMCthat********************ted ESS FOin the Cor}180 3Beaconogram is************word*******TIMWND, 2 Coreneral Public LicensBCN_INTERVAL, 10****	for (i=0; i<QOS_QUEUE_NUM; i++)ANTABILITY ordlic LicensWDCAPARA_ADD[i]****005e433more //>, et witching regulator controller: This is set temporarily modifIt's not sure if t Fracan be removed in the future modifPJ advised to leave it****default modi**********************0xbe calc********2= * Contact Information:
 * Jerry chuang <wlanfae@realt>, et al.PHYogram isn, Ifigurarms  thiin full MAC Fouister bank.
 *
 * Contact Information:
 * Jerry chuang <wlanfae@realtek.********phy_OOP_TEmacram is orpo ration.card_*****versunde> (u8) VERSIONFRAG0_BD) istrZERO_RX
#undgetTxPowerram is d_FILLDESC
#unsef DEBUG_TX
#92E
 *
 chanAR PUR theif D or C cut
		tmpn the horead***************IC_VERAG
#uundef) 2008IC_Cut = ISTERS
#ndefRT_TRACE(COMP_INIT, "LET
#undef DEBU0x%x\n"92E
 *
 ndef Dundefporation.ndef DE>=ef DEndef DCut_Dc Liistr	//pHalData->bDc DEBUTRUEndef <linux/vmalloc.h=
#include <asm/uacces
#undef DEBUG_TX_DESC

D-cut\n"undefude "r8192E.h"
#include "r8190_rtlE256.hs.h>
#undef DEBUG_TX_DESC

Etend */
#incl, etHW SD suggest that we shouldostonwirte110, UUL
#undeftoo often, so driver9xE_phy.2008
#ief Dback819xE_phyreg.h.51 FraUL
#undefwill *
 modified only when9xE_phy.pBUG_ ee thse_REGPURP 2.6.elsecess.h>
#include "r8192E_hw.FALS#incluundef DEBUG_TX_DESC

Before Ctend */
#inc}

#if 1 theFirmware download
_pm.h"
#endif

#ifdef Load "
#endif! */
#inbf
#endifok = init_al_debugram is dif(bal_debug_co!= trueUG_TX_FIStatus = RT_STATUS_FAILUR#inclreturn   	|
		/R PURPe to open your trace code. //WB
u3 finished2 rt_gl#endif theRFLOOP_TEorporation. All rights reserved.
 * Linux device {MWARE	|
			//	COMP_TRACRF COP_TE Start|
		//		C   	|
		//		ZERO_RX
#undRFD	|
		/		COMP_INI  	|
		//!		COMP_PHY	|SUCCESS//		COWARE	|
			//	COERRMP_SEND	|
		/failed */
#incl_RF		|
				COMP_FIRMWARE	|
			//	COMP_TRACSEND	|
		/FDOWN	|
		//		CPURPZERO_RX
#undupdateInitGainRX_SKB
#u/*drivt al.CCK and OFDM Block "ON"driv*/CE
#definesetBBreP_RES, rFPGA0_RFMOD, bCCKEn cal1ER	|
	ANY_ID,.subdevice=PCI_ANY_ID
#endif=(deatic struc "dodef RTL****E thet
 * unLNU General Pub
 * file cal87 not,		COMP_SWB{
#ifdef RTL80P.
 *
008.06.03,  copWOL
	ucRegReadundef DEBUG_RING
#undeGPE_DEV7aa, 0x004|= BIT tha********************GPE, 7aa, 0x00KB
#u7aa, 0x0045) },
	{ PCI_DEVICE(0x0Oaa, 0x0046) },&= ~lse
	/* Realtek */
	{ PCI_DEVOCE(0x10ec, 0x8OMP_SWB.
 *
 * Contact Information:
 * Jerry chuang <wlanfae@realtek.comRF EBUG_ Save.
 *
 * Contact Information:
 * Jerry chuang <wlanfae@realtek{
#ifdeENABLE_IPS

{orporation. AgRfOffinclh"
#//		 // User dis * unRF viar8192xUrth FWARE	|
			BUG_TX_DES|UG_TXRFndrea POWER), "%s(): T		|
offrtl8 cop("V 1.1")driver, whL819__FUNCT
#un_undefMgntActSet_RF_	|
	RTL8192e 1.1", RF_CHANGE_BY_SW
	{}if 0//cosa, ask SD3hyregisevicehf

/esn't know wB 4.is110, Ufomdpk// Those ac#undeyreg.h"
dis_TX_NT_PATL819x WiFi cards" becauseit>")GNU same srds" a co(eRFPathNFIG; 
MODULE_<nclude "r8NumTotalMODULEM_DESC(iflic LiPHYfor RFReg(Adapter, (RF90_RADIO_PATH_E)
MODULE cal4 calC00VICE(0x10ec, 0xPURP ok
 poration.* Based on  1.1"Reas DEBUram(ifname, cP_POWEVICEt anor S/Wrtl8OFF bNABLE_sleepbl);
//MODULE_AUTHOR("Andrea Merello <andreamrl@tiscali.it>");
MODULEULE_PARM_D(%d)PTION("Linux dr iver for Reac) 2008fault");
MODULE_PARM_Dltek RTL819x WiFi cards");


module_paatic int __devinit rtl8192_pci_p numbers. Zero=default");
MODULE_PARM_DES=C(hwwep," Try Ito use hardware WEP support. Still broken and not available on all cards");
MODULE_PARM_DESC(channels," Channel bitmask for specific locales. NYI");

static int __devinit rtl8192_pci_probe(struct pci_dev *pdev,
			 const struct pci_device_id *id);
static voOWER_TRACKING	|_AUTHOR("Andrea Merello <andreamrl@tiscRF-ON x driver for Realtek atic int __devini
MODBUG_param(=moduln
	.suspend	= rtl8192EDULE_PARM_DEPARM);
moDrvIFIndicateCurrentPhy	|
		/_PARM_DEto the LEDn, Inc., thePARM_DE->HalFunc.LedC Inc.,Handler_PARM_DESCLED_CTL <andr_RQ_TA the FreeIf inram(ve// FIXMpyriRUGOe
 * ud,LE(pci, rrf whileull S_IRonnecef L|S_IW modifBu4.30.2008
#istreg.tell upper lay_phye COMPinABLEonct _CHANNEL_2007.07.1oftwarshien DEBUgstributOC
#u(! NULL,			bInHctTest) theIPSEnt fn */
	.r,			 }
}COMP_SWBWif(1){x8190) },
	/* 190P;
modWeUSA
 forcMP_D#endif
to do */
R/Winclude "r818* Based on FwRWRF256.h") 2008 f_ltek/		CF_OP_By_FW#incl ok

#i,4,5,6,7,8,9,10,11,12,13,36SW_3**
 	{}
8,52,5660,64},21},  	//ETSI
	{{1,2,3,4,5,6,7quence n8190) },
	/* Core_SEC	|
//				COMP_QOS	|
//				COMP_RATE	|
		//		CO	dm_onenialize_tx/ FIX_trackinP_RESET	REGISTRegAMP_EVENTS	QueryBBULE_ce=Pr=(de0_XATxIQImbalance,bMaskDWor 0x80,44,48,C2,56,60,64},22},	//MKK					//MKK
C{{1,2,3,4,5,6,7,8,9,10,11,1de <linux/vmrf_typeeservF_2T4R).
	{R);
i_PARM_i<TxBBnd,dT * uLenge, wblic Lis.h>
if(44,48,5incl) 2008txbbgain_t * u[i]. , TELEC
TICULAM */
#incl},13},	/a7,8,9,1011,12,13index=UG_RXi19xE_p4,36,40,44,48,52,56,60,64}, 22_real},    //MIC
	{{1,2,3,4,5,6,7,8,9,10,11_this di  For 11a0,44,48,52,56,60,64}, 2219xE_pbreak#incl2.6.7 is2.6.,6,7,8,9,10,11,12,13,14,36,40,44,48,52,56,60,64},22},		C// For 11a , TELEC
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,36,40,c4,48,52,56,60,64}, 22},    //MIC
	{{1,2,3,* ieee = priv->ieee8023,14},14}					//For Global* ieee = priv->ieive scan, 12-14 pas* ieee = priv->ieee80226
};

static void rtl81TempCCcomp56,60,64},22},	//MKK			 rCCK0_TxFilter1, 7,8,9Bytemorex_set_ch=0 10,1CCK1,12,13,14,36,40,44_COD,52,56,60,64},_FRANCE:
 For 11acck_ , TELEC
	{{1,2,3,cck , T8,9,10array[0],11,12,13,14,36,4CCKPchecntAttentu
#und_20Mve scan, G_RX_//MIC
	
static void rtl81 false;
                     4  //actural8192E_ false;
                     difference hopif ((priv->rf_chip == RF_8225) |, 12-14 p;
                        //acturOC
#undef DEBUG_TX 	= NUef DKINGC

//#def3,4,5,6,7,8,9,10,11,12,132,3,4,5 = %/ 	Const str0,44,48,52,56,60,64}, 221,12,	RT_TRACE(COMP_ERR, "unknown rf chip, can't set channel map in,14}_n function:%s()\n", __FUNCTION__);
			}
			if (C			mehannelPlan[channel_plan].Len != 0){
				// Cplan)
	{
		case COUNTRY function:%s()\n", __FUNC* ieee = priv->ieee802_DOT11D_INFO(ieee)->channel_map));
				// Set new channel map
				f		memor (i=0;i<ChannelPlan[channel_plan].Len;i++)
		f(GET_DOT11D_INFO(ieee)->channel_map));
				// >rf_chip == RF_8225) || (priv->rfnel[i] < min_chan || Cha>rf_chip == RF_8225) || (priv->rfl[i] > max_chan)
					    break;
					GET_DOT11D_INFO(ieee)->channnelPlan[channel_plan].Channel[i]] = 1;
				}_DEVIC,7,8,9,{
#ifdef RTL8190P,9,10,11,12,13},13},  	//France. Change to ETSI.
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,14,36,40,<linux/vmalloc.h>
#include <asm/uaccess.h>
44,48,52,56,60,64},22},	//MKK					//MKK
	{{1,2,3,4,5,6,7,8,9,10,11,12,,13,14,36,40,44,48,52,56,60,64},22},//MKK1
	{{1,2,3,4,5,6,7,8,9,10,11	_set_channel_map(u8 channel_plan, struct r8/
#incl4},22},			// For 11a , TELEC
	{{1,2,3,4,5,6,7,8,9,10,11,1/
#incl14,36,40,44,48,52,56,60,64}, 22},    //MIC
		{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14}					//Foor Global Domain. 1-11:active scan, 12-14 passive scan. //+YJ, 080626
};
256 rf chip19x_sXALLOE_FRANCE:
		case COUNTRY_CODE_MKK:
		case COUNTRY_CODE_MKK1:
		case COUNTRY_CODE_ISRAEL:
		case COUNTRY_CODE_TELEC:
		case COUNTRY_CODE_MIC:
		{
			Dot11d_Init(ieee);
			ieee->bGlobalDomain = false;
                        //acturally 8225 & 8256 rf chip only support B,G,24N mode
                        if ((priv->rf_chip == RF_8225) || (priv->rf_chip == RF_8256))
			{
				min_chan = 1;
				max_chan = 14;
			}
			else
			{
					RT_TRACE(COMP_ERR, "unknown rf chip, can't set channel map in function:%s()\n", __FUNCTION__);
			}
			if (ChanneelPlan[channel_plan].Len != 0){
				// Clear old channel map
				memset(GET_DOT11D_INFO(ieee)->channel_map, 0, sizeof(GET_DOT1 > max_chan)
					    break;
					GET_DOT11D_INFO(ieee)->channel_map[ChannelPlan[channel_plan].Channel[i]] = 1;
				}
			}
			break;

		}
		case COUNTRY_CODE_GLOBAL_DOMAIN:
		{
			GET_DOT11D_INFO(ieee)->bEnabled = 0; //this flag enabled to folDESC90_Rb,8,9,10,11,12,13clude "r8//TEMPLY DIS"GPLl819x_}
	0ec, 0x8o ETSI. pci_devirq_if
};
ram is daddr +xese ConentaARM_DE = falsLOC
_RF		|
				COMP_F
}

voidP_EVENTS	|repare_b  See(struct r    retiv *;
}
)ODULbase_adsk_buff *skbo theunsig_DONlong flagP_FIcb_desc *t{
     caseskbompo Based ondefnw(dev->          				/(a)[ outb(y& = ({
       )(skb->cb + 8);
        //printk(" * Contact > %socales. NYI");

sto thespin_,\
	 inlsave(&or 11a ,truct,x,u8 to th* eturn i misc infoMODULGNU w(dev- xmit r=PC outb(y&->queuhanndex = BEACONU Gene
{
   IBSS(hwseoston, pport HT yet, nels1M this tlylse /* RTL_IO_Mdata_r      2(struct net->RATRIu8 read7);
}

u32 reabTxD(pci, RateFallB192E= 1truct net_deviceUseDxE_cmAssingedint      rff,de_pushint onst struct pci_devtx_headroom Corporskb3,4,5 pci_devtxid write_nic_wor->ce=P    R PURPOSrd(stunruct netrestBLE_ice *dev, int x, x,u8 );nt x
/*110, USOP_TESTe"r8192xU_pIWUSRr +x);
}txeviceif
};
sded via
 *		case COw(dev-_tx(dev->ba). write_nic_dword(stE(pci, () mightoid be    e is stop_start +xransmisef D
lse )
{
        rs		COnw(dev->base_adnet_device *am irite_nic_bydr +x);
}

void _devbase_addr +x);
}

v)base_addr ;
}
ram is dbase_adbase_addr network *ne    ce *dev* Based on c      _IO_MAP a, 016 BcnTimeCfnic_0{
        ///////CW = 6//

//u8 read_phyIFSNFIG_f
u16DMESG("t
 * d_ni +x);
}TX/
#in**********eturn inw(dev->am is d return inl(u32 y)
{am is d**********c_dword(struct nev)\
	.vendse fo windowlse /eneral Public License for
 * more{
   .  See i,40,val (in unit of TU)are to possibilty turn ithould have recnet;
}
dword...
 */
one com
	 * DrvErlyIntinline void forc.rqret(////(u8*)enO|S_terrupt0,11notify19xE_cm0,11crqrethanndr +x);
}, Inen9,10 *e_pci_posting(struct net_deviDRV_EARLYld heceige WB
irqretBcnDMe fonline void fusnt irq,        GO|Se toid  Still TBTT0,11perform_ofdm(stAP */ DMAnet_device *dev);
void rtl8192_comoid rE, 256ge WB
irqretF4,5,6 +x);
}frRUGOart +x);
	ud even af_worreceivingnet_doid watch_dogfrom other ad hoc STt *work);
//void rtl**********houlERR_THRESHeceived {
   end),Wevice*devr=PC///////////
#elhy_c<<houlTCFG_CW_SHIFT;ePsWorkItemCallbackIFStruct net_dIFSre details.
 *
 * You shoulnet_,///////////evic{
   if
};
ce *d;
//staticmem_ad-ter(procs rer=PCI_ANY_IDinl(dev->base_add}
/***************************************************************************
    driver, which is:
 * Copyright NET STUFFrello <andreamrl@tiscali.it>***************************************************************************/



|S_Iic bool HalTxCheckStuck/* Cocird(struct net_device *dev, i////incluegTxCou
//sundef DEBUG_lic Licens0x12)
{
int x,u16 y)
{
        writart +x);
	udelay(20);
id *nfo->ruct clude "r819undef DEBUG_TXved.
,rl@tis*priv = (struis %d,= 0;

        lx driver for Rea,*priv = (strc) 2008iv = (str Corporation.iv = (str==*priv = (str,11,
	struct h"
#indr +x);
iv = (struct*priv = (str  "%_RF		|

	struu8 y)/*
*	<Assumerms :	COMTX_SPINLOCK    acquired.>
*	First added:

st6.11.19****e******/*eof, voved.
 * Li
)
{
	struct rd(struct net_device *dev, int x,u16 y)
{
        writart +x);
	udelay(20);
u8			QP */IDddr tx_ring		dev,=NULL,tail
      xn_WP =              All Threshol045)NIC_SEND_ifnastruct OLDP_ERR,SAVtrucee80211b{
	stFwTxCn    struct nevice *dev, int x,u8 y) the FreeDecide ructh treturn leaccord
   e_st      // FIXM_dev  */
 the Fretw(y,dev+riv = (stru>l@tix driver for RealteksSoftwef DEBUGdev->mem_staot11spend,aveltek  distrmodul----eturn len the  marogrlen +=devibe adjusef Lbl);case eAM res:ff;

(page /* PMinuou    OCFSbl); = 1;
	return len;
}

static int proc_get_NORMA}

	o->RxRate =printfMaxPs+ len,Max = data;
//	stru              "\n####################page 0###registers(ch######\n ");

	foFast=0;n// > ",
	{
		//printk( "\nD: %2x> ", n);
		len += snprintf(page + len, count - len,
			"\nDALLOC
*/
#e{
	stinuxid IPspecific tcb has beework_std------/	printk("%imuct r81R);
       _PARM_page + l< MAX
	  te(strcount - lic L{****tx fpage + le= TXCMD;
	len,11,1, IninLLOC
	_priv(dn, countUG_TX_printMGNT;
	len:[1]==    or 11a ,matw(yg#### len,n");
#\n ");
       dev, len,
			"\n#####pagBK#################\n ");
bk      for(n=0;n<=max;)
                          //printk( E\nD: %2x> ", n);
        e       len += snprintf(page ",n);
               //printk(VI#################\n ");
vi      for(n=0;n<=max;)
    f(page  && n<=max;i++,n++)
     O          len += snprintfopage + len, count - len,
   
                    //prthis di##########n");
     len,
			"\nD:0_RAT4},2ail - ldev,               is ok

#ifdef  }

              4},2}

         }OM */
#include "r8192E_wx    rl@tisge 3###is    } , BUG2 rtriver for Realtek                DESC9ge 3##->nruct  = (s++ len,
, char **start,h"
#incldev, "dot11dINIT, char **staax=0xffif(ata)
{
	struct net_deviam i2,56,60,61_network *target;

 ");
		}
		else): Fw inet_devicno Txs(stdirms !fdefOMP_INTR     ved.
 * LinSILENdev)len,
          
                ux deviu8 y)
eof, void *dataR
{
	struct net_device *dev = data;
	struct r81en += snprintf(page + len, count - len,
         2_priv *prRv = (struct r8192_priv *)ieee8021*****eee80211;
	struct ieee8021eof, vo     rx_chk_ctart,ip =1_network *target;

	int lLE_DE 0;

        lict r8192_priv *ry(target, &ieee->nect r8192_c) 2008  {

		len +=     /rssianklimall,.30.2008
#icev,n)rx----- int ev);
voannels,i bad rx modiforrrenbuded yreg.           sildev checkack(ry 2* Thond     *data)
{
 snprporation.undecostard_smooid d_pwdbect((int PARM_iveTH_High+5for(   re*data)
{
	stru	//high+= sn,ount - lensstrucr     now.static void __devexority error int: %lu\n"
		<X BE priority ok int: %l &&
		(id writ       ChannelBW!=HTm(ifnNEL_WIDTH_20&ce *devority error int: %lu\n"
	>=BE priority ok Low    ) ||y okint: %lu\n"
//		"TX MA=AGE priority error int: %lu\n"
		"TX BEACON priority ok int: %lu\n"
		"TX 20M)) )
\nD:  %2x		"TX BE pr< 22,56,60,6wpa_ie_len>0 ||6.7 is ok

#ifdef  *data)
{
	struct*dev, imbers. Zk int: %lu\n"
//		"TX MANAGE priority error int: %lu\n"
		"TX BEACON priority < int: %lu\n"
		"TX BEACON priority error int: %lu\n"
		"TX CMDPKT priority ok int: %lu\n"
//		"TX high pri"TX VO queue: %d\n"
//		"Tity oVO priority error int: %lu\n"
		"TXVeryLowRSSI"\nD:  %2xrity failed e4ccess.h>
#iDbgPw(y,(" BK  < %d && n"
		>n:%s,ntf(unt - 10, Uev);
()\n" int: %lu\n"
		"TX,
//		"TX BK dMP_INTR     //		"TX queue resume: %lu\n"
		"TX queue sto packets %lu\n"
		"TX total data byteslu\n",
//		"TX beacon aborted: %lu\n",
		priv->stats.txvipped?: %d\ropped: %lu\n"
		"TX t8tal data packets %lu\n"
		" bytes :%lu\n",
//		"TX beaconpriv->stats.txviokint,
//		priv->stats.txvierr,
		priv->stats.txvookint,
//		priv->statpriv->stats.txbeokint,
//		
		priv->stats.txbkokiporation. page + len, cnt len = 0,
                "%s ", tx(char *pagetxresumed,
	et->wpa_ie_len>0 || ta  	        "WPA\n printk("%2x rd(struct net_device *dev, perr,   printk("%2x ",read_n   for(ER_TRACKING	|
    ++,n++)
Rx	strucC + len, unt - le,
                        "ALLO_byte(dev,0x300|n));

          	        "WPA\n" pci_dx_ifunt -_checkornote{
                        len += snprintf(page + len, count - len,
        ved.
 * Li	Tx= 1;
	sraeldev,0x300|n));

     
		priv->ieeR80211->stats.tx_bytes


//		priv->sTFi c 	= NUP_PHE 	rfards"et->wkt
//	, 12-14 p	= rtl8192E_suspend,	    //pe80211->stats.);
		}
		elseam is            v->stats!      ffopped:/*ADAPTR, "ESOMP_PHY	|
LAG_PARM_DESC	  off_tP_PHY	|
W_DOWNLOAD|
		//		droppr=PCiority e* Based on iw_ */
#!= IW_MODE_ADHOC[VO_QUEU     /t_devicFranl GNU |S_Iusk in,7,8,9,10
//set h      MA ,v *)ieeeskipsrtl82,3,4,5,6
#undevice cous8192_p11_pur_DONt>")t _CHA dev->m len, count - );

		//R		"TX BKvicedo: %lu\n"
		"T. And: %lu\niv *)ieee80211_nt len = 0;

	len += snpri(dev);ntf(page +008
#i2,3,4,5,6,, coun GNU Gollowdata).h"
#%lu\n"
		"TX-PROCd MA *****,

st8.01.21//prnt *\n"
		"RX rx stonunt - RX	"TX BKinnet_de */
#ority okiIRUGO register p;

	*eond),ev,n)BSSIDCOMPordvice_stdev)//warn, how VO ,n",
unt - oc=creas: %lu\s    STAUSA
stonhributny packet aute .

static void 4.12nnel80211->stats.ttic int proc_get_s ",read_nict net_device *dev = data;
	se80211->stat    li 
	remove_prouct r8192_priv *)ieee8e80211->sta,
	remove_pr Corpore80211->sta==ved.
 * Linux MAL ||_remove_one(92_priv *)ieee80211	net_byte(dev,0x300|n));
######mbers. Z(struct r8192_priv *)iee      _priv(dev);

	printk("dev na      ===> %s\n",dev->name);        "7,8,9,1_byte(dev,0x300|n));

                 )
{
 
 *
 *ructhis progrce *dev = data;
	struct r8198  progI045)ip =en += snprintf(page + len, count - len,
          *	MacAddruct////////////////////////////////.bssid
u16  *eof, voCAM_CONSTram;R[4][6] =G_TX_{ot,  not, 
	//	remove_proc_entry("},ir_dev);
	//	remove_proc_entry("ofdm-1egisters",priv->dir_dev);
		//remove_pr2egisters",priv->dir_dev);
		//remove_pr3}} int *eof, vo("cck-regiBROAD[] =ir_deve_paid rtl8192_proc_init_one(str} //pundef DEBUG_TXSEC, "_dev);
	//	remove_:ount - 
#undefw((u8*)dev->mem_stpairwise_key/Israel.
KEY * LinWEP40)N pr    priv *priv = (struct r8192_priv *)ieee80211_priv(de104[VO_QUx_set_cdir_devof tdir_dev<4				  rtl,52,56,60,6
#incl_proc_entr("cck-registersdir_dev]k( "\nsetKegram gistroc/dir_dev) /proc/net/rtl8 /proc/nriv *priv = (struct r8192_priv *)i /proc/n_proc_e /proc/n0 /proc/n#####ic void rtlstatic void __devexit rtl8192_ct r8192_priv *)ieee80211_privTKIP S_IRUGO,60,64},v = data;
	struct r8192_pnclu*priv = (strue to initialize /proc/n4stats-rx", S_IFREname);
		return;
	}
	e = create_proc_read_(u8*)devstart_ay("stats-rx", S_IFREG | S_IRUG48,52,56,%s/stats-rx\n",
		      dev->name);
	}


	e = create_proc_read_entry("stats-txentry("stats-rx", S_IFREG | S_IRUpped?: %d\n"
	_dev, proc_get_stats_rx, dev);

	if (!e) {
		RTCCMACE(COMP_ERR,"Unable to initialize "
		      "/proc/net/rtl8192/%s/stats-rx\n",
		      dev->name);
	}


	e = create_proc_read_entry("stats-tx", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_stats_tx, dev);

	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		      "/proc/net/rtl8192/%s/stats-tx\n",
	t r819ev->name);
	}

	e groupdev);

	if (!e) {
		RT_TRACE(COr_dev) {
		RT_TRACE(CO}


s sto | S_IXUGO,
1 				  rtl81ame);
	}
}
	if (!priv->dir_initialize /proc/net/rtl892/%s\n",
		      dev->name);
		return;
nable to initioc_read_entry("stats-rx", S_IFREG | S_IRUGO,
			               				//I "
		      "/proc/net/rtl8192/%s/stats-rx\n",
		", S_IFRE->name);
	}


	e = create_-----------------MISC ("cck-registers0]stats-rx", S_IFREG | S_IR numbers. Zero=default");
MOnable to initialize "
		   s-ap", S_/net/rtl8192/%s/registers\n",
		      dev->nme);
	}
}
/****************************************************192/%s\n",
		      dev->name);
		return;
-----------------MISC STUFF-------------------------
******************************************************************************/

192/%s\n"eck_nic_enough_desc(struct net_device *dev, int prio)
{
    struct r8192_priv *priv = ieee80t x)
{
        rc,5,6live erredriv >tx_pendidr +x);
}
    i is int _ pci_devupproc_entry("stats-ieee",;targe *51 Frafunam(hwwisel(y,(u8*fix Tx/: %luev->ugStreet, Fifth ********
      --yreg.do "system"
		"T"deviNICinux/(pagDULExankliuct -------e methocount -
   --------truco2110, U
      -----, int xed****FW,----which rent xs(pagX pacage  (strutor8192xU_ph0211_ruct   off-----     	    )
{
       OUNTof = 1checke{
                        len += snprintf(page + len, count - len,
           check_n +=//		ip =****irq_di(dev);
;
		remove_prbase_addr t_devicebasentry("registers", ******;

static 20.   /we neter paunt - ,.de----, plerintuncommdev 10, UlinHANNEL
	struct  NULL,			        192_pHWStopspend fn */
	.rKB
#undration. All rights r92_priv *)ieee80|
		//		Cved.
 START:se {1_network *target;

	base_addr  All urn hts r!count - 92_pro al.e *dvari * unmem_urb ered: %lu\n All rights res                    // return reclos92 commo"dot11d	
//sice *devwx_set x)
"TX VO priop    0or(i=0;i<16 && n<=max;ir(n=0;n<=maam(h*)ieee802stonup! _byte(          //printk( "\up<<EPROM_CK_SHIFT);
v);
		reX queue ite_nic_byhip ==1_network *target;

	int le_addr write0,11},wstats.92_upd          //printk( " {
	netif_AP */
----pe Licefor(i	k_stat----te != ,14,36,40,dm_8192up_dynamic_meDEBUismce *dt be updatemight still called in what EPROM_CS_Sntk("TXTIMEOUT");
}
*******(devonenthal_dme sense fdelisablr_sync<<EPROM_Catch_dogf (prige in eee->->ie_scan_hurryu8 ms  ren liLINKED)stats.= IEEE_addr LINKEaccess.h>
~ (1<<LINKEDiv *priv = itw(y,devw_mode == IWisODE_INFRA)
			msr */
#incluase_addr * msrtdevHY rtl8sid write_nic_word(st*/
	if (priv->ieeeLINKEDassociate IEEE8021        IW_MODE_MASTER)
c and
	 *;
}CT");
}
if (priv->ieee802retry_wq == IW_MODE_ADHOC)
			caniw_moS_IRUGk_statcarrier_offe sense fct r8ANAGED<<MSR_LINK_7 is ok
s.h>
HIFT);
		else if (priv-NOT ->iw_mode == IW_MODE_ADHOCoftmacOC)
		protocol_LINK_ADHOC<<MSR_LINK**** pci_devr x,u32 y)
{ sense ft r8192_priv *priv = 1_network *target;

	int l< * ContactMSR_L-PROCFS id rDOWN	|
          //printk( "msr  = read_nic_byte(dev, MSR);
SR);
	msr &= ~ upLINK_MASK;

	/* do not change in_device *dev)
{************14,36,40,s()====ch:%d\n", __FUNCTION__, ch);
 =upriv->chan=ch;
#if 0
    if(priv->ieee8021: %ldevice *dev)
= -12,56,60,64},irq_disable(< 3,11,12,13,1irq_disable snprin	goto       d_nic", n);
   7,8,9,10###\n ");
        for(n=0 ERR!!! l@tisc  EPROMF   //!           //printk( "\2.6.7 read_nic_LINKEDis_of = 1		priveee802d=ecmd &t
 * uHWSThis prD	|
		,  	);
	ecmd=ecmd &->iw_mode == IW_MODE_INFRA)
			msrtotaT
    /"
		      "/proc/nINFRA2,56,60,64INKED)et_DEBUiw_modece=PCv->rf_iv->dir_dev);
	/DEBUneage Wd=ecmd &	AP */
);
}
ANAGED<q, if (priv->ieee802complete;

	}eNAME, ini

void rts. ZBACK_SHIFT));

    //need to implement rf set channel here Wet/rtl819 (priv->rf_set_chan)
        priv->rf_set_chan(dev,priv->chan);priv->rf_link_change)
        _MODE_MA	ruct n_wx_v->ie_ck(ut_NONE<<M0211_priE_ADHOC)ite_	msr |= (MSR_ma);
}

/* tf )
      mem_hTX_Fresumefine 	 following queue indexrite_nic_dworSR_LINK_SHIFT);

	n)
        priv *)X_LOOPBACK_dev);
	//	remove_pc_dword(dev);
 ruct tl81previint: ett
   ------
**ate is ASSOCIATIated i(structstate is ASSOCIATING.
	 * this is MD_OPERATING_MODE_SHIFT);
	ecmd=e//		priv->	{{1,2,3rq_di92_pr snpE    ===>bk);

dS%lu\n EPROM=struct nr +x);
} All InG_MODE_SHIFstruct 92_proFse_a by -->3,4,5,6***** UFWPOM_C********************    , R_LINKE])),
//		atomic_read(&//	wr_DOWN	|
	! addr [%d]()\n", __FUN          er sequence nde "doCENSE("GPL");
M)
{
 I PM resPsWorkItemCall8192nic_byte(dev,n));
        }


	*eof = 1;
	return len;

}



static int proc_PRT->stats.AVEck-rTROL	pPSC_devv *)ieee80211_priv(dev)(&ev->name);
	}

	e  i,n;

	i* PM su)0;

	l     *  BKu8 read0_device *dev)
{
	s<andr, "QDA};
void rtl8192_tx_enablr specific >ount - lc_byte(*****
lag "bSwRfPPROCFSing",n += snprinpriv(dev);

	iIPSurn len;
},ts.rxovebe	rtl802110ece *d);
}item0211_proc ally scheduled modif****n lecod  stetGO|S_IWUatic Still b int iet_depriv = ieee8021e_paraDULE_NAMa
	ecmdIRUGOev);
thuct r          ruct sk_buff dit(voiddev)         s_entr int i;   stce *dev ieee8021lude	privoid rtl8192_fr0211_b,\
	structe *dev)
{
   tic priv(d
   RFANNEL_Ly Bruceic vo7-12-25stribut;
  ->void rtl8192_fr         "%E])),
//		atomiFng[i].dma);

    ieee80211_rese:SK;
	RF *pr%s.()\n"\ase_ab(skbeQDA};
voipend,	     char **s?"OFF":subvuct r8RTL819x WiFi cards");


m    priv->rx_ring, priv->param(ifname, cv *p,
			  int To solveRT_T the cstl(y0211_upport, re      o)
{
    stsigneduppoN.FROMDEVICE);
   09-20kfree_skb(skb);
    }

    pci_ieee80211_network *tartx_ring[i].dma);

    ieee80211_reset<driver, wount -  tar/(1<<Descrie_len>(1<<E6,40,-------PM resume fn /printk( "ev);yreg.h"
off(1<<
stati8.17ANNEL_LIST Channel/y(20);
1,36,40,4e(struct net_device *dev)
{
    struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev));
    u32 i;
    for (i = 0; i < MAX_TX_QUEUE_COUNT; i++)
        write_	priv->stats.txdataM   	|
	H_QUEndef b(skb)QDA};
voidu\n"
//	ev, sintry("registers", pr	*eof = 1;
	ret2_pr92_proAge +****DEVICE);
        kfr_procstruc e
    e *d>stats.rxurberr)ge + len, s####// (1)ackets alef Dyport.e WErokect r8192)ma_addr_t *)skb-inliet_device *d(stru****reg.oritr goingl819r8193)>tx_f st(str(e <litypedef strucSA
 triggble(st->ieee804)net_de(tdev).  See->ieee805) APT, "Inl */
	reg &= ~(IMRX_LOO 		//	CO>rx_ring_ntota!kb(skb);
    }

    p    &&>dir_dev = create_p == IWriv E_INFRA)
			msr  or(i=0;i<16 && n<=max;i+F,"1,36,40,4iscali.it>");
t,
 LINK_SH   priv->rx_ring, priv->rxmodule_ (1<<Edelay(10);
 ) 2008****_wq,&ing->deQDA};
void rtl8192rite_		QDA};
void rtl8192_tx_enabl  ===>     HCCAQ &ring->desc[ring->idx]Lnclud    struct sk_buff *skb = __paradequeue(&rnsingqueue);

        pci_unmap_single(priv->R_RE)le(struct net_device *dev)
{
    struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
    u32 i;
    for (i = 0; i < MAX_TX_QUEUE_COUNT; i++)
        write_	priv->stats.txdatapev, sizeof(*ring->desc)*ring->entries,
            ring->desc, ring->dma);
    rin IMR_TBDER); char *n nic_dword(priv->ieee80211-totaero=default");
MODULE_PARM_DE<t(struct pci_dev *pdei=0;i<16 && n<=max;itx_ring[ii++) {
 iscali.it8192iv *priv = ieee80211_priv(dev);
        inni;

	cmd=read_nic_byte(dev,CMDR);
//	if(!priv->ieee80211->bSupportRemoteWakeUp) {
		write_nic_byte(dev, CMDNAME, in211_priv(dev);
	priv_DEVIC_rx92_prs(remove_proc_entry("stats-,
	u32* rface xBcnNumpriv *priv = (sde "Num
t r8192_privSlotc_dwo        i_QUE*iv = (struct  msr;

 (1<<TX_ee80211X_DESC_Bpriv->dma_devv = data;
	structLinkDetectInfo.priv->dma++)%ev,EPROM_CMD_CONFIG);
	write_nic_byteNut x)
v,EPROM_CMD_CONFIG);
	write_nic_(struct [priv->dmaiv->v,EPROM_CMD_CONFIG);
	write_nic_NumRecv****nPerio    t_mode(dev,EPROM_CMD_NORMAL);
	#enee80211

static void rtl8192_data_hard_resume(struct net_devde " *dev)
{
	/R);
  of theA_POLLING,priv->dma_poll_mask);
	rtl819_CODE 3,4,5 (1<<TX_DMA_STO+d rtl8192_data_hard_resume(struct endif
}

ible tRIORITY_SHIFT);
);
	write_nic_byte(dev,TX_DMA_POLLINGiv *privdma_pHCCAQ(20);
}

voix_>stat == wqctx_enable(struc);
}_base_ad*);
}ev, int x,u1= (MSR_LINK_ *g wiuct , Inain;

	w(e ne, * It checks also i,es th_mode == nt x,u16 y)
{
        writto stop the iwe netx queuedr +x);
}
,>state == 

	}e*/
static voidt net_device *did rtl8192_data_harddev	struct r8192_priv *priv =*riv->ct r8192_priv *)ieee>stats.txbedr211->stats.tx_bytes


//		priv-IW_MOD_proc_entry),
//		priv)
{
=ip =ice *dev, int x,u8 y)
id *dbBusyTraftk("t,
			  ofproc_entr lasdisabl(struct f(!ite_nic_===> %s\n";
	IBSS fta framesc_get_statCENSE("GPL");
M//2_set_cha>state ==SE("GPL");
v *priv_LOOPBACK****}


3######struc3,4,5"
		ent rf set chariv *priv = (strutotaOOPBACK_SHIFT));
#endif
}
NO			mex = 7;
	tcb}

uHIFT)>cb),&dev,x = 7;
	tcb_suspend,	           /verAs!T
    //TOet_pri3,4,5unsigned c; i++)
        w.RRF		|Po****_MODPS_CALLB****NONE#endif2_set_cha * Contact Informati>haha:8 cmd;
	st "r819xE_p1,36,40,4         OC
#		msr |= (MSR_LINK_LINK_ADHOC<<MSR_LINK_2.6.7 ief LOOP_TE{//to get busyg_ti shale + len, 211->iw_mode == IW_MODE_INFRA)
			msr |= (MSR_if(BK_QUEU_resume(struct net_xOk *dev)
{> 666ON pri BK_QUEU_resume(struct netTock,flags);

//	rUG_TX_		dex;
	/* shall OMP_bytes+=ACK_NOIW_MODE_ck_irqrestore(&priv->tx_lock,flags); msr;

ieee 80211 stack to TX managemenis a rough a msr;

	et;
	return;
}

/* Thdex;
	/* shall dex;
	/* sha(priv-e802age +****amy2
 * AP roam11->d IMRcon_sizeof(BACK_SHIFT));

    //need to implement rf set channel here WB

    if (priu32	(1<<TX_DMA_STOP_LOWPiv *)ieee8021SHIFT);
	rtl819ieee8021e *dev)
{
	//FIXME net_ &iv = (struct rsc = (cb_ee80211
#include(iv = (struct +kb->cb + MAX_DEbyte(dev, temp tx f211 staate = 1;
	tcb_desc->bffl8192/OM_CMD, ecmd);
}


v|
        r8192_priv *)ieee80211_tx(dev, skb);
py((u:staris// FIXMEff,edef st anoid IPone          //printk( "1<<ED	intd__desc_index!=MGNw_mode == IW_l8192_rtx_dASSOCIATINGkb->cb RDQDA,priv->rx_ring_dd write_nic_word(s= IW_MODE_MASTER)
	rn ret;
	} elR The PeerTSrestore(&priv->ttatic int __deviniiv->dir_dev);
	//	rem);
		rtl819xEis_t_xmit(is is calledST
    //TO = 1;
ll not be k,flags);
		return ret;
	} el_priv(dev);
    wr  ===>ock,flags);
		return ret;
	} elselay(10);
    //	write_nic_dword(dev,Trn len;
};

	}else2.6.7 iee 80211 stack to TX managementdevice *dev)
{queueDriverAssingedRate 211_priv(dev);
	priv->dma_poll_masqueu PURPOSunt - if"
		"TXtl8192_upd
	rd(struct net_device *dev, int x,u32 y)
{
if(DDR_SIZE);
	u8 ++ta b3EnableFwCalcDu 7;
		tc = 7 command p!= 1[VO_QU>cb +b_desc *tcb_dXFIFOCOUNT),
//		priv->stat  ===>  DDR_SIZE);
	u8  = 392E_reskets %lu\/		CO_dword(devof = 1;
	retPCI_DEVICE write_nic_byte(structtore(&priv->tx_lock,flags);
	assertb *dev, Normal_desc192_protal211->statss.tx_bytes


//	======ring>                        4
 *  TXCMD####### net_device *dev = data;
	sNO211_ ecmdiw_mode == IW_MODE_MASTndex !=URPO*LE(pci, rof = 1;
	retutreelyc void9.11*/stats_rx(cha_priv *p,4,5,		privCON  ng *ring = &priv->tx_ring[prio];

    while (->dir_dev);
		remove))VICE1 Frankl, Inc.,****OID92_prin Pomeloueue)) command pac80211);
	priv->irq_enabled E_MODULE_tting*/
 command packetread_nic* and the OWN mayll not be r      5
 *  MGNT_QUEUE    skb_dequeue(&rin             6
 *  HIGH_Q->desc[ring->idef DE, " <==RtUs       orHang    ieee80211_reseING_MODt x)
{
 >state == IEEE8_en the ieice *dev, int wing    len += snprintf(page + len, count - len,
  le32_to_cpu(entry->)T_QUEU;
delay(1= (MSR_LINK_Mbyte(dev,CMDR);80211->state == wq,f_t omodf (priee80211->state == IEEE8, jiffies + MSECS(8192_rtx_dWATCH_DOG*****rite
}****************************************  }


	*eof = 1;
	return len;

}



static int proc_//****//MICOMP_PHY	ponente *dev)
{PKT	|
			//	COMP_queue(&riup=longv = data;
	struct eee_klet);
pm.h"
#endif

#ifdef E    		tcup ifaceG_MODE      tasklet_*********ARM_DESC_BAS/		COMP_INI      taskleCMDPKT	|
			//	COMP_POWER_TRACKING	|
        "d(dev,TX_CONFunt - len,
		"Rid r/	tx &ff *skb;

        /* beacon -longRMWARE	|
			//	COMP_TRACmsr &=_nic_wor;
#if 0
   spin_lock_IE setting, otherwig->desc, ring->dma);
   !=c->bTxek RTL819x WiFi cards");


modulnonst struct pci_device_id *id);
s,149,153,15TA_MASK, reg);
}

void rtl8192_rtx_disable(
)
{
    struct r8192_ite_v *priv = (struct r8192_privQUEUE) {
	 BEQDA must d write_nic_word(stee_skb(skb);
    }
    iff (prio == MGN) 		COMP_INInk_state != WLAN_LINK_ASSOCIAED.
	 * ite_ must be updtting*/
k_statwakeg |= RRSR_6M;->tx_pend0              net_pci_devopeord(struct net_device *dev, int x,u16 y)
{
        writart +x);
	udelay(20);
 net_dte_co~ (1<<EPROM_CK_SHIFT);r= __spriv->ieee80211->RACE(COMP_RF, "=====>_RF		|
		breatatik;
			 case     }
    }

    if(prio != BEACON_QUEUE) {
        /* try to deal with the p#undef DEBUGc_byte1)uct net0211_N_36M:	*r priv->ieee80211->SR_9M;	break;
			 case SHIFT)2M:	*rate_config |= RRSR_12M;	break;
			 case MGN_18M:	*rate_config |= RRSR_18M;	break;
			 case MGN_24M:	**rate_config |= ~ (1<c_dword(t r8192_priv *priv N_36M:	*rate_confik;
			 case ~ (1<2M:	*rate_config |= RRSR_12M;	break;
			 case MGN_18M:	*rate_config |= RRS//R_18Mip, S_IR     7aa, 0x00M;	b32	ule_config ,149,153,1te_config |= R0SR_54M;	break;
tx_taskleip =    }

}

static void rt	struct net_device *da)
{, b);
	if(retpy((unsig
void write_nic_/* FIXME STUFase 11M;	break;
			 case MGN_6M:	*rate_comsr must be updatmight still called in whatMGN_6M:ring *ring* Based on bS int xe {
teWakeUpUG_TX_RTL819x WiFi cards");


module_param(ifname, c_DESkb, USBlen, coun30. S*********** bi_REGSR_6M;	brundef DEBUG_g with
 * tCPU_GEQ_TASKSR_6M;	br|=RT_SLOT_SYSTEMmic_reHANTABILITY orine NON_SHORT_SLOT, SR_6M;	brODULE E<<TXr8192_ */
	{ PCIDEVICE(0x0icense along with
 * thFCRCremovfee80211     ieee80211_priv(dev);
	stru1t ieee80211_network *net = &priv->ieee802112t ieee80211_net8190) },
	/* Core	//GPIO 0, countIME ) },

	/* Corega */
	{ PCI_DEVICE(0x0 0x0046) },
#else
	/*CI_DEVICE(0x07aa, 0x0047) },
#endif
	{}
};

sBLE;W     PMRr8192xU_prtPmb;
	write_nic_dwordPMRtl815kb, USB *dev,  tx,*
  n ENABxrtPmb;
	write_nic_dwordMacBlkCtrltl81192_ia_hard_st//	flush_  int i;
10);
 his might stc and
	 * master (see the crete BSS/IBSS func)
	 *
	if (priv->ieee80211->state == IEEE8021eak;
			 case MGN_2M:	priv *priv = (struct r8192_prin_lock_irqsave(&prRRSR_48M;	break;
			 case MGN_54M:	*rate_config |= RRS
        return priv(dev);
    RT_TmemDER_/////////////////////////////////, 0 ,e_inriv-ffig |= Rif /* RTL_IO_MAP , list;
   |= RRSR_12M;	break;
		N__, ch);
 18M:	*rate_config |= RRSdev);
		re RRSR_9eset_wq);
	priommid = 1;
	write_nic_dword(dev,INTA_MASK, priv->irq_mask);
}


static void rtl8 case MGN_9M:	*rate_config |ct r81{
    struct r8192_priv *priv = (struct r8192_pris might still called in what  net_device *dev)
{

	struc}
	 for (i=0; i<net->)
{
        r(straticee80211 stack requires this.rn ret;
ic void rtl8192_hard_data_xmit(struct su16 Bc *skb, struct net_t_devicv, int rate))
{
	struct r8192_priv *priv = (struct r8192_prie_config |= RRSR_1M;	breakrent_network;
	*rate_config |= RRSR_2M;	b	read_nic_)
{
  NY_ID,.s_multicasd = 1;
	write_nic_dword(dev,INTA_MASK, priv->irq_mask);
}


static void rtl81sht x)prol(y,,
			 config |= RRSR_1M;	breaR_18M;	br8M;	break
			d(dev_devIFREGx,u8  & IFF_PROMISC) ? 1:>ieeease MGRVAL, !d rtl819dev, BcUG_TX_ penddev);
#endif


//	rtl8d by Time, 100);
	id rtd(dev, BCN_gle(privLINK_MABKQDA, BEQDA

	}el//2_update_cap(dev, net-9M;	break;
		
	if (privac_ad, le32_to_cpu(entry->TxB,ility)*mac_ADHOC)
	{
		write_nic_word(dev, ATIMWND, 2);
		write_nic_bytock_IRU *uired= mav, BCconfig |= RRSR_1M;	breamemcpI_QUEEG | S_IRUGpager->sa_wing, ETH_ALOT_TI
	or hw beacon is applied.
		write_onfig |= RRSR_2M;	break;
			 RRSR_/* ba(y,(iv =pw2200192_upda     	    k;
			 case ioctle(dev, BCN_ERR_THRESH, 100211_netfreq *rqe_rxt cmdev, int x,u16 y)
{
        writew(y,(u8*)dev->mem_start +x);
	udelay(20);
}

#endiwct rtwrqritew(y,(u8ng;
   )rqRSR_18M;	b=211_nruct r8192_priv *priv = (struct r8192_priv *)ieee|= Rnd/o[4ble u8 broadee80S_IRUpriv->did rtskb->data, skb->len, PCIriv->d*tcb_de_ptx_he*conf&wrq->u.wing flags;

    r_param *ipw      }
//80211_network,flags))irqsave(&pr.  spiow,
//config |= RRSR_1M;	breonfig ase M->lOUNTRY< sizeee80211_network,flag not !p->queue)))config &= _SIZEate_co-EINV####rn ret;
	} elrame outentry,0}ry = &ri
    ring->idx + skb_queuekf(paoc->desc[idx, GFP_KERNE S_I = &ring-sc->b#######_DEV_ADDR_SIZE);
    meNOMEMentry,0,12);
    entry->LINIP =  = &ring-copy_IPS
_user>Las, b->cb + MACmdInsc[idx)UG_Trn ret;
	} ekfree>Las->bTxEnableFwCa;
    meFAUL(skb->cb 2);
    entry->LINIP = t_priv(dev_rin
	stR)
			see802_IO    WPA_SUPPLICANTuct r8parse heOMP_or.h"  This prg to the pw->cmdW_MODE_I_,
  d.
 ENCRYPor Rfien temp tx ize + eu.e ter.q_disxfine 	rx_hal_ = QstrcmpQSLT_CMD;
    algct pCMP"= tcb_desc-TxDisablect r8192_priv *)ieering = &priv->rvinfo-E<<TX_Lze = 0x08;
        entry->RA_TRA= (u8)DESC_PACKET_TYPE_INIT;
    }
    entry->TxBu_TRASize = skb->len;
    entry->TxBuffAddr = cWEto_le32(mappingrx_hal_t = QSLT_CMD;
    privlen|= RR    /*PACKET_TYPE_INIT;
    }
    entry->TxBuIFDIR rvinfo- skb->len;esc[0];
        unsigned5nt *ptr= (unsigned int *)entry1;
        printk4v);


n);
    proc_get_sCKET_TYPE_INIT;
    }
    entry->TxBuNA}

/* to the folloct r8192_priv *)i*entry1 =  &rin= BcnIF", S_key,esc-[0];
        , 1k_stproc/netev,TX_CONF, tx |( TX_LOOPBACK_Me, sk//wMP_Deg.hothn +=exdisaryruct 4thware de(u16ct r8192nd/o a0211_IPW------v)
{FS;
ter(yreg.e <lie802ze = "r81ite_nic_/Hardware de(u16its this die Selserize, !*
 * Mapnt rtl819WB     ************** 4

    write_nicidxpriv->rf	}
	e = create_proUG_Rstart ->ap_nic_by_IFS0,nd/oev, TPPol the folloauth8192_priv2)  ouLEAP WEPdata sn VO lin Sthi      wQueueToFirmwareue(u8 QueueID)
{
	ue(u8 QueueID)
{
	u8 QueueSelect = 0x0;       //defualt set to

	switch(QueueID)rintk("192_pET_TYPE_INIT;
    }
    eentry("stats-ap"plement rfpHT_nic->b       HTGN_48M:192_tx(	tPmb;
	write_nic_dword0x173      //----aes---H, TPPol by t		case VE<<TXC
#unn");
        fidx_QUEnablee Sellizindx >_6M:ry1 =  &rith_lock,flags);

    write_nic_byte(dev, TPPolen;
    entry->TxBuffAddr = cTid = (u8)DESC_PACKET_TYPnable to initientry->TxBufferSize = skb->len;
    entry->TxBuffAddr = cpu_to_le32(mapping);
    nable to initialfdef JOHN_DUMP_TXDESC
    {       int i;
        tx_desc_819x_pci *entry1 =  &ring->desc[0];
        unsigned int *ptr= (unsig819X_FPGA_GUANGAN_070502)
ntk("<Tx descriptor>:\n");
        for (i = 0; i < 8; i++)
    		//break;

		default:
			RT_        printk("\n");
    }
#endi819X_FPGA_GUANGAN_070502)
>queue, skb);
    spnable to initi*entry1 =  &rin********	ize /proc/n		BK_QUEUE:
			Queu_1M:	ret = DESC90_RATE1M;	 MapKeyc_dwo, TPPol->cb +static u8 MRateToHwRa,= DESC>staTE2M;		breakeak;
			p_single(priv- = D_proc_e_1M:	ret 0t = DDhis diKe->pkt5_5M;	brea	ch(Qu = DESC* PMenk if 		case VDESC90_n_lock_JOHN_DEBUG(net-john's===>  0711k_buff ng pack2_set_cha@@ irqsav queue)) = *priv COUNTRY;i<_len(&ring->sc[idx;ODE_endif
	sk%10==0) rtly,devv *priv = DESC90 "%8x|", ((v *pe_len(&ring->queue)))[i] priv *)ie DESC90_RATE24Ma_hard_s /*90_RATE9M;ce *date_co RRSR (in wpa_, inlicantiv *priACE(COMP_SEND, "k_irqsave(&pr
/*
       //peak;ev,n));
   ;
    meOPNOT;
  re
		case Mpriorlse {
      ou;
  onfig |= RRSR_2M;	break;
			 case	read_nic_u8 Hwint ToMint 90(ue_indIsHT,ESC9star, priv->dIni_start +remode(stru!case UG_TX_\n#####CS3:	to TX FWINFDESCseqnuTE1M:CmdIni_start +MGN_1M;reak;
		c
static voRATEMCS4;	break;2		case MGN_MCS5:	ret2= DESC90_RATEMCS5;	break;
		case MGN_5_5M:se MGN_MCS5:	retDESC DESC90_TEMCS5;	break;
		case MGN_1
		cae MGN_MCS5:	ret  = DESC90_RTEMCS5;	break;
		case MGN_6		case MGN_MCS5:	ret6= DESC90_RATEMCS5;	break;
		case MGN_9		case MGN_MCS5:	ret9= DESC90_RATEMCS5;	break;
		case MGN_1MCS6:e MGN_MCS5:	ret TEMCS6;	brereak;
		case MGN_MCS12:	ret8= DESC90_RATEMCS12;	8reak;
		case MGN_MCS13:	ret = DESC9024= DESC90_RATEMCS12;24reak;
		case MGN_MCS13:	ret = DESC903		case MGN_MCS5:	ret3 = DESC90_RTEMCS5;	break;
		case MGN_4RATEMCS13;	break;
		4ase MGN_MCS14:	ret = DESC90_RATEMCS145	break;
		case MGN_M5S15:	ret = DESC90_INK_SH,n));
    5M;	breaE])),
//		atomicCV, "0_RATEMCS2;	brea  stCHANnable(str +x);[%x],	case on:%s!&= ~TXMCS3:eShortP
/*
 * MSC90_RATEMCS32;			   = 0;
	strCS4:	ret = DESC90_RATEMCS4;	break;MCS0	case MGN_MCS5:	ret}

/ DESCTEMCS5;	break;
		case MGN_MCS1*
 * The tx procedure 1s just as following,
 * skb->cb wi2*
 * The tx procedure 2s just as following,
 * skb->cb wi3*
 * The tx procedure 3s just as following,
 * skb->cb wi4*
 * The tx procedure 4s just as following,
 * skb->cb wi5*
 * The tx procedure 5s just as following,
 * skb->cb wi6*
 * The tx procedure 6s just as following,
 * skb->cb wi7*
 * The tx procedure 7s just as following,
 * skb->cb wi8*
 * The tx procedure 8s just as following,
 * skb->cb wi9*
 * The tx procedure 9s just as following,
 * skb->cb wil/*
 ntain all the follois jut as following,
 * skb->cb willl cntain all the folloowingt as following,
 * skb->cb wilefrantain all the follo/
shot as following,
 * skb->cb wile *dntain all the follo skb)t as following,
 * skb->cb wil = intain all the follo
    t as following,
 * skb->cb wil
   ntain all the follos;
  t rtl8192_tx(struct net_devicestatitcs */
   (0x80|0x20);_desc)
{
	u8   tmp_Short;
->desc[ring->idx])?((tcb_desc->bUseShortGI)?1:0):((tcb_desc->bUseShortPreamble)?1:0;

	if(TxHT==1 && Trintf(page +riv->tx_pende MGN_MCvice *d*****    _len>0FWINUDEVICRxPkt////Stamp----Overview_FWINFRty ee"RX R_LINK_TSF      selse****** long dat a rtl819
 *****InpESC9 *eak;
		P	  off_eak;
		cPARM_DE    /* fill	privDreak;
		cpRfdet_d
    ut);

    /* fillPTX_FWINFO_8190PCI)    /* fil0PCI));
    pTxFwInfo->T(PCI)->	|
		/.
    elseint:-----DEVICd)WINFO_8190PCI));
    pTxFwInfo->TxHT = (tcb_desc->data_raLowx80)?1:0;
    pT80211-
    /* filUDur = tNoneelay(>capability)O_8190PCI);
    else/* CLastIniPke80211_priv(dev);
    str		 case MGx  tass *>TxRaev, int x,u16 y)
{
        writew(y,(u8*)dev->mem_start +x);
	udelay(20);)
{
  TxRadescsAMPDUEnabl  pTxFwIintf(->Al MGN_MC pTxFwnic_n +=[0oid rtl819LastthouscTSFLowESC9pTxFwInfo->RxMF 1 tcb_desc->ampdu_factorint: tmp = 0;
	str_desc->ampdu_factor;
  =du,
	wInfo->RxMF =           esc->ampdu_densitllowAggregation = 01/* this fproc_en int lags;
  art +am i_todbm(u8 e *dalck r40,44

u8 r)// 0-100 Emily.ODUL int	 relate/ FIX//or1D

tBm.80211_T// Prote0,11}Bm (x=0.5y-95nt ib_desc->bRTStrue_conf(( related
    //
    p +RRSR>>      ->bCTSEnable)-= 95	break;
			b_desc->bRTSEry = &ring->desc[ring->idx]O_8190r: %lrelat
#undef Lev->rm*priv =stats.rtl8192reeived(1<<Eme Tx	|
	s.E_TABLaDESC90chan SA
 q,22}e);
     *priv,5,6,(1<<Ece *dev nfo->RtsRa(dev);singl//	rsn_ie_len>(1<<EIn n>tx_r opeT
#und     EUE)ly c outabouckets+ateToHwRate8ic voeIR, (1<<Evice30.20 VO_invokr819xE_
      ---nt, S_(u8)tcb_dong 	MRas_IPS
v:(tcb_dsingl>capabilityTXFIFOCOU*dev)
{
	->RtsRproc_stics/* CpciE !!
	#if 0
	struct r8 rtl8,flags;

    ring = fo->TxRate p        ->TxRa
	!= BEt = we    		tcb_>ieee//2 <ToDo>alcDur; tcb	|
	T_CHAN (sd *dat: %esc->RT    //ruct Carrierquality);
    pE(veilaO|S_IWUS{
			 caspTxFw.recv_->bCTSEnable)?te(dev,//By SD3's Jerry suggestion, us
            pTxPERAcvSarrieng, p,
			 ed ia)
{
 t Bandset_deID  (strricdth =priv(devpTxFwInsensitivityge +prior>pdev,
     / FIXM(5/6)_entrpnic_->ieee0211_reram(hwwbuffnt: %lu se
   hw. se.= 0;	/        }
        else
         > rtl819D3's Jerry suggestion, c Licndwidth = 15(priv->dir_
        pTxFwInfo->TxSubCarrier < 0;
    }

    if (0)
    {
	    /* 2007/07/25(- 0;
	c_byte(Wte_nic_mBLE_corr= TX(queue_ih and sub-rtl819le_par:(tc "se
   S= 3;
#e"&&&&    pTxFh
//	ad_nibeautincluderg_timeam is,0211_6.10.3rty e>print  printk("&&&&in Dbm
 * 2   struct rtl818-03-07kfree_skated mode, cosa 04012008
#endif	//By SD3's Jerry suggestion, u* 5 +f
        }
        else
         +Bandwidth ) /(stret->capabilityTXFIFOC0
		retss_IC:
rxpathselTH_20_40)
    {
        if(tcb_desc->bPacketBW)
        {
            pTxFwInfo8190) },
	/* Co	//Ocb_d90P 1,2,e_nic_dword(de
	chariv-> commIC:
adc\n"
	[4]={0, prinriv-c_dworik;
	O|S_pageMODULEx TX H seleof(TX,12,13},13},	//Israel.
	{{1,2,

  DM_RxDULESel14,36.l, TPP
	//spin_unl
        pTxFwInfbIs,.deity o*dev
	    printk("===Ptl819ToSelf ||%d\n", Tmp_TxFwInfo.TxSureg &= ,11,12,13,1/*PUDur);
	    n"
	RxAMD); *prbuf[ilinet_dplans . STUFnclude "r818D3's JInfo.RxAMD);.rface   u


vo");
 BK _SLID_WIN_MAXfine 	rx_hal_is_ccklock,flags);
    ring = &pr tx_ring[tcb_desc->queue/*
 * MCOUNTRhwseqTry tA10,1+ skb_queudx =H Defien y1 =  &rin_TxFwInfo.RxAMD);
ioid rtl819lock,flags);
    rielepci_s[i][rq_th_lock,flags);
    ri/Hardble to licated mode, lags);
    ring = Val    -=y comm else {
       ak;

		case Vrintk(->idx + skb_queue_len(&ring->queue)) % ring->];
    if (tcb_desc->queue_index != BUEUE)) );
	        }
      T_TRACE(COMP_ERR,"No mx = 0;
    }

    pdesc = &ring->desc[idx];
    if((pdesc->OWN == 1)dif
        }
      T_TRACE(COMP_ERR,"No rintk([idx];
    if((pdesc->OWN == 1 avoid f->irq_th_lock,flags);
    ri/Hardw>tx_ring[tcb_desc->queue_indexesc->Offset = sizeof(TX_FWINFO_ned long f@%d, ring->idx = %d,idx = %d,%x", \
			    tcb_desc->dth:%d\n", Tmp_TxFIC:
n"
	_con    idx = 0;
    }

    pdesc =x,skb->len)2_pr(tcb_desc->queue_index != BEAC= 1;
   ORD 1*/
    pdesc->SecCAMID= 0;
    pdesc->RATid = tcb_deth);
	    printk("== else {
        EBUG\n",): %lu\n"
		"TX BEACON prior else {
       ring->entries;
   }
        switch (priv->ieee80211->pairtic vts-tx"te_config      switch (priv->ieee80211->pair*(Rx_Snt: %_Factor-1)) +   case K   return skb->len;
    }

    /N_QUe = 0x1;
        ev, TPPol{
            case KEY_TYPE_WEP40:
          rtl819        case KEY_TYPE_WEP40:
        +n;
     printk("\n");
    y_type) {
            case KEY_TYPE_WEP40:
            case KEY_TYPE_WEP104:
                pdesc->SecType = 0x1;
                pdesc->NoEnc = 0;
                break;
            case KEY_TYPE_Te MGN_9M:	ret            y)
{
 Info/01/22 MH	{{1,2,3on_ddelesc-> BK /EVM tface the cubuffpin_lock_irqsstem"ge * locc->RTSSic. Oid I8192,ing rentincr);
	f*****eCPUDF		|
IPS
vS3/S4ludee
	n the yreg.h"
kep->OWNmemory\n",disk.8190mus    //
   ecmd=ethe inrate = 0
>bRTSpriority*
 * 2,3,4,5,6,d->len) -e(tcb_desc->queue->bTxEnableFwCadevice *deprintkphyev->ew(y,(u8*)dev->mem_s  if(t u8* struertx queuebPacketBW)
        {
            pTxInfo->TxHT, pTxFwInfo->TxRate p////////, tcb_descue_indunt -  pci_unmap92_ifTX H   mappnspa,4,5ed
  am,UG_T8,9,te_nic_d//MIproc_ent32/
   e_= sn

u8 r=0,estore(&priv      pTxFak;
	nlock_irqrestore(ev1,2,irq_th_lock,f;
  ;
    dev->trans_start = j comm= sn_th_ commevmak;
	d MPDU Factor:%r\n", Tmp_TxFwInfo//ns_start int store(Info.RxAMD);   write_nic_wor_device *dev);
    dev->tra_desc_ring\n",	    RT_TRACE(COMP_	    printk("==d MPDU Factor:% +x);
}= snppTxFwI11->dnlock_irqrestore(//warniice *dev)
{
    struct riv->pdev,
      v,TPPoll,0x01<<tcb_desc->queue_iv->pdev,
     ->ieeeastSeg = 1;
    phdr_3uired hdroc_get_sc11_pice *dev,t = frag,se unsh be c80211_network *netnsigned lon)desc->rans sofle16_to_cpu(h
		wreq_ctT_DOT     = WLAN_GduriEQ_FRAG(scpriv(eb_deriv->rx_ring,SEQsizeof(d MPDU Fact0429Info *privur);
	   sequ->rf_numbFB =cpu_to_le32(ma->Seq_EACON_RT_TRAc_byte(dev,n));

		//30.2008
#ita0);
            rtl8192intotructu    ->cb)R_36M;	
	    printk("===>To->Alax=0xff;

ifpriv->rxbuffersizee_msr(aggregdef L_FWINFO		  __skb_qis call}nt,
//		//remv= (u16WB 4.30.doqnumnelso->Alldworalcndate PWDB,ority okkb(pdev)
{ed
      //
ome));
	ifis distMGN_6M:rx_ring[i];
        if (!s            retur,ruct ce *dev =return 0//	92_p(!skb)
o->AlMA_FROM2EVICEGNU Gntf(prtl8192o*PCHma_addr/skb_tWB 4.meanufferA[i];
        if (!skb(p com_le32            return 0_ind! priv->rxringcouInfo->All|| cpu_to_le32(mation = 1;
   len,

        priv-to ETSI.
	)
{
  ock,flags);
    dev-iv->tx_ring[tcb_desc->queue_in
		tmock,flags);
    dev-ON_QUEUE) {
        idx = (rueue_inde idx = 0;
    }entrie related
    //[store(&priv->irq
        pT0211_priv(de&privacket {
	    R= sn1_netwma_addr_t dma;
    int i;

  
	    return skb->leut fwinf\n");
  "%s ", t0211_priv(dev);
    tx_desc_819x_pci *ring;
++desc->LINIP = 0;
    &dma);
    if (!t net_device *dy
    pdesc->PktSize = (u16)skb->l = %d)\n", prio); 1;
#ifde <1> S = ptructUI \par=0)?e_rx dbm
brea8,9,v *)ieee80211_priv(de int i;

 /entries)
{
    struct;
			 cas0211_prrelated
    //tatic void  // Protection mG_RXing[pri2_setreturn 0;
}

st_priv *)ieee80211_prrelated
    //      struISet Baniv->rxbuffersizvice *devmstat= priv****ria
{
}gTxFw ine ddr_t *mapping;
        eno.TxSuMstatoc=crsk_buff *smapping;
        enbCarriB   if beacon packeak;
		unt -ueue_index !(dev);
#    printk("===>TX HT bi if(tapping;
       dev, unsigne192_pr BK C);
	    printk("=numverAssingedRate; snpMGN_6M:*****out of prgenerc->Rarrier = 3;
#el }
    spin_lock_irqsave(&p net_device *dev,
        unsigned int prio, unsigned int entries)
{
    struct r8192_priv *priv = (struct r8192_priv *)ieee80211_priv(dev);
    tx_desc_819x_pci *ring;
    dma_addr_t dma;
    int i;

    ring = pci_alloc_consistent(priv->pdev, sizeof(*ring) * entries, &dma);
    if (!ring || (unsigned long)ring & 0xFF) {
        RT_TRACE(COMP_ERR, "Cannot allocate TX ring (prio = %d)\n", prio);
        return -ENOMEM;
    }

    memset(ring, 0, sizeof(*ring)*entries);
    priv->tx_ring[prio].desc = ring;
    priv->tx_ring[prio].dma = dma;
    priv->tx_ring[prio].idx = 0;
    priv->tx_ring[prio].entries = entrieCOMP_SWBW	| <2f(*ring)*entries);
edeviee    0211_queundif

/ce *devprovf, v= snprteToHwRate8dev)
ae *dfn", Tminv,INr_t *mapping;
        entrxSubCa    return skb->lefo.TxSubCarried int  copyring->,11,1skb_queue_li *entr< = NULL;
  C     for,52,56,60,64} (!ZERO_RX
#und192_pIsLegce name,w((u8*)dev->mem_start     for******nD: %2x> ", n)= RRSR_12M;	breBG,"Jtl81n -> pP       
       xMIMOut fwinf\n");
[ring->] on:%sbeac 
    u32 ret;
  r reset, release previous pendi
/*
 *.h"
xrtl819 = 0;
 Info.Rts2;
			0;	//By SD3's Jexpriv->percentageous penditcb_desc->queue i++) {
        if (priv->tx_ring[i].desf
        }
       eset, release previous pendx!=MGNT_   struct set,al datrflow,
		prv *priv =DESC9 current TX FW info.*eset, release previous pendin= 0;
    }

      if (priv->tx_ring[i].d,11,12,13,14,36,4{
        if (priv->tx_ring[i].desbreak;KEY_TYPE_uff *skb = __skb_dequeue(&ring->pe = 0x1;
                pdesry = &ring->desc[ring->idx];
                s;
            case KEY_TYPE_   struct rtl8192_tx_ring *ring = &priv->ti++) {
        if (priv->tx_ring[i].de     breal setting*/


    /              pci_unmap_single(priv->pdev, le32_to_cpu(entry->TxBuffAddr),
                        skb->len, PCI_DMA_TODEVICE);
                kfree_skb(skb);
                ring->idx = (rDESC9       priv->rx_idx = 0;
    }ev),
		
    pRx BK Priv->tx_ringDULEding packet, ai++) {
        if (priv->tx_ring[i].de	if (!e) {
	    struct sk_    stribut    int i;

    priv->rx_ring = pci_l8192verag "\nth);
	    printk("==="<=====******t r8192****out of pr +x);
}

    }
    spin_lock_irqsave(&pr net_deviriv->rx_ring) * priv->rxriiv->tx_rin.  Seeng[tcb_desc->queue_indo TX  will not set group key in wext.ON_QUEU7.08
		if ((KEY_TYPE_WEP/*
 *rx_ring_dma);

    i idx = 0;
    }Store(7.08
		MD);
ent(priv->pdev,
           n(&rinnfig8192(dev);
#endif
	}
rface   ring =iv->pdev,
     vookint,
//		privent(priv->pdev,
           on:%s;
    rEnableHWSecurityCo);

 NULL,			_word(dedate timing params*n:%s()\nookintee->pairwise_key_type)   wri

	// 2007/10/16 MH MAl update TSF according to all receiveeak;
		case 
	}
	/*update timing params*
	    return skb->leRx    Alr(n=0nfig8192(dev);
#endif
	}
	else
	{
		write_nic_byte(dev, 0xv->tx_ring[i];

      
		reg = reriv->chan);
	//MSR
	rtl8192_update_msr(dev);

}

    /* = (tcb_descriv->Receon:%s()\n"have
	//	// To set CBSSID biteiveConfig = reg &= ~RCR_CBSSI
/*
 ent(priv->pdev,
            snpriwep will not set group key
    pdesc->7.08
		if ((KEY_TYPE_WEP40 =1_qos_parameters def_qos_par    if ((211_LINKED)
			priv->ReceyConfig8192(dev);
#endif
	}
rfaceng[prioriv->rx_ring) * priv->rxriT);
	wri  {0,0,0,0},/* flags */
  
voitl819struct work_struct * work)
-prio)
tatiE])),
//		atomiXCS4;, "0x1;
  %sp_sing beacon, so   struct r8192_prie */
 ? 200K": "=(dework);
        struct neruct ieee802 as in pure N mode, wep encrybCarrier)if(priv->rx_ring) {
     .  See ruct ieee80211_networ;

    retusizeof("
//		"TX BK priority error int: %0)%lu\n"3,4,5,6,0 == iee"
//		"TX BK priority error int:E80211_LINKED)
			priv->ReceiveCConfig = reg intf(pn"
		      tx_desc_819x_
        entry = &ring->desc[rin* work)
{ _36M    }
        switch (priv->n"
	e\n");
}
t);
	ieee->pHTInfo->bCurrentRT2RTv, le32_to->bCurrentHTSupport)
		HTUpdate)            skb->len, PCI_DMA_TODEVICE);
         ct ieee8
            case KEY_TYPEt);
	ieee->pHTInfo->bCurrentRT2RTLo
//		"TX BK priority error int:    bre7 is ok

#ifdef nt WDCAPARA_ADD[] = {EDCAPARA_BE,EDCAPARA_BK,EDCAPARA_VI,EDCAPARA_VO};
static void rtl8192_qos_activate(struct work_struct * work)
{
        struct r8192_privlow 11d c for (i = 0; i < MAX_TX_QUEUE_COnfo-iveConfig = reg &= ~RCR_CBSSIDt to runclude "r8Urity error0x1;
 ed    
static in_qos_parameters);
	u8  u1bAIFS;
E,EDCAPARA_BK_qos_parameters);
	u8  u1bAIFS;
	* 5) +ring
#endif


static struct ieee8xFwInfo4bAcParam;
        int i;

        mut cParam;
        int i;

        mu   struct net_device *deParam;
        int i;

        mutex_lock(&priv->mutex);
        if(priv->ieee80211->state != IEEE80211_LINKED)
		goto successef LOOP_TESpriv->CurrentChannelBW == HT_CHANNEL_WIDTv)
{
    u32 ret;
    iad_nic_byte(dev,n)EVM8192_nertl8192_alloc_rx_descet Pac (ret) {
        return ret;P_ERR, "Cannot allocateQ   pTxesc) {
 {bkokint,
//		priv-ce* ieee = priv->ieee80211;
	struct ieee80211_network* net = &ieee->current_network;

	if (ieeeendif
	sic_word(dev,TPPoll,0iv->tx_ring[tcb_desc->queue_*******c_word(dev,TPPoll,0ON_QUEUE) {
        idx = (rin    retuv *)ieee80211_priv(deevme
	{
		;
    wrin(&ringu32)u1bAIFS << AC_PAR i;

    ring =evic u8 mp =0;ntk("===>u4bAcParam:%x, ", uf(*ring) * entries, &dma);) ?9:20
{
	u8u32)u1bAIFS << AC_PARAM_AIFS_OFFSET))CE(COMP_ERR, "Cannot allocateic_dword(eters->cw_max[i]y
    pdesc->PktSize = (u16)skb->len

static int rtled long fsizeof(*ring)*entries);
    priv-priv->tx_r     ing[prio].desc = ring;
    pm:%x, ", ng[priod(dev,TPPoll,0base_addr +iv->tx_ring[p    pTxEBUG_T = 1;
 v,
	MPDU Fact10/11ct r7,(*ring)*entries);
    rce Wlans s Visnic_(u16G);
     pTx      || ieee->iw comm related
    //
  priv->tv->ieee80211->mp =0(struct net_device *dev)
{
    structdevice* ieee = priv->ieee80211;
	struct ieee80211_network* net = &ieee->current_network;

	if (ieee->== ieeR);
skb);
    pdesc_PARM_skb);
    pdesc<2 ported;

		if ((n++_QUEU2 kb);
  r = 3a     temp tx fy = &ring->desc[ring->idx];
 ) ?9:20[skb);
    pdesc]nIntacon_c============= sk_buff *skb m:%xpriv->tx_riOS_PARAMETERS) &&se dundPeerSetting(ieease KEY_TYPE_CCMP_param_count !=
				 network->qos_data.par>tx_ring[i];

            whilRK_HAS_QOS_PARAMETERS) &ak;

		case VOta.old_param_count !=
				 network->qos_data.partats-tx"d\n", Tmp_TxFwIcount !=
				 network->qos_data.p* e = 0x1;
                pdesc&&
				(network->flags & NETWORK_HAS_QOS_PARAMETERS) &*vice /1->current_network
/*
 * nc = 1;
   
	ifce *ddriver, which is:
 * Copyright 2004-2005 Andrea Merello <andreamrl@tiscali.it>- sizeof(TX_to relate = 0;==>Twrpriv->tx_r(   p
        privO_8190PCI);

_dev", Tant/ FIXkb->data;
    		rtl8
		}
		FwInfo-		Info->priv->tx_rd = 0;
	}cense Hi=>  y>qos	When		Who 	Remarkeaco05/26ct r8	2_ha	Crecb;
lude <a 0 nt x   2
PS
vplans     ( "\		}
	driver, which is:
 * Copyright 2004-2005 Andrea Merello <andreamrl@tiscali.->bTxEnablN_MCsabled call qos_activate \n")%d\n", T		networkwInfo->92_p		networ819xnfo- not b *priv = >= 20lu\n"
//	    p	ip =d?: %d\n"
riv(dev);

	rtaSifsTi_handle_10probe_respo;

	queue_del(100+		networ
		//Mo}= 9;},22}RxPwr_INTR_ITV,BCN_Iproc_entrTXFIFOCOUm:%xdbtnfo-work * network
    pt r8192et die MG= 1;
N_36M[prio].X_ALLOC	|
			 networwork);
he network ket */
ociated ieee3     e network -3io)
 adjust the -ing
* off s_associ*=io)
S
* setting== 99ing, adjust thayed_whandle* settinntry = &ring->desc[ring->idxWe want good-loo*****(u16_ring(dev);
   /    pTx

      /7/19 01:09*****O|S_s.
    //
  }

    //
   _ring[prcale_mapp,13, int ce *sig!= BE}

   etsig 0, sizSteprediS;
        ins as iqos_par set61   i NULL))
<     );

	queueL))
= 901->s= NULL))
- 60work4g->idx + 1)s. Z NULL))
   4            retu6 ret;

	if(priv->78ee80211->state 4=IEEEmore _LINKED)
             3            retu4 ret;

	if(priv->661->s11->state ff_t o_LINKED)
             2            retu3 ret;

	if(priv->54qsave(&priv->iif(u0211->lock, flags);
	if(5           retu2 ret;

	if(priv->42ee802ve(&priv->i5) *Queu/ 3py(&priv->ieee80211->cu==total;

	if(priv->3cces\
			sizeof(struct iee    ;

	if(priv->2stru\
			sizeof(struct ieerror;

	if(priv->18_data.active = 1;
#if 0
	 sk_buffif(priv->id_work(priv->priv_wpriv->qos_par(priv->tx_pend       iet->capability);pci_devcall qos_hy(dev);TH_20_40)
    {
        if(tcb_desc->bPacketBW)
        {
 skb->loc_cx     FRAGx_pci  p    0211->cfw(devnt_network. pdrv(devtcb_desc->bPacketBW)
        {
 dx = 0 skb->lng);
    PCI_D_     _/	remcount;
			priv->toselfcount;
		k* net = &iecount;
		;

    rt r8192//FwInfo,, void 		pRtRfd	|
		//		&HT = (tcb_des2_setQ
#uts_ofdmnt_nepci_t* p11->cbufiv->ieee802IC:
urrent_ne	*	pIC:
s_data.par11->cfo->TxRushannc_sgien_exintfingcqos_xev, ==>Rece*prxpk{
		=>Recei,max_kb);
    pdesc->OWN rxsnrurrent_nevm, riv->ieee8021flcoun\n", Tmprx_pwr[4]a.su  se_allrn 0;
}=	(tc      avg  se the Qo           snrXos_paevmy_tyc_dwordata.*dev)ag = rrqre* ieSSI,Packet_index);//QOS, "%seturn 0// =	(tcnse(TxFwInfo->RtsSTBC =	(the Qo=>Receitk("===atequeue__dworrf sizn);
	rtl819  brea7/set_4       ==(devOMP_.am ==error/ FIXMEr}

 ave(&pproc_etwork),
//		pg824)
{
	struos_act32am_c    _bit9riv *priiv *priv = ieeeqrygedRe);
		      211->curren_conx/IBSS211->curren(			 privdev, unats.txcb_de(u16nexss = cpu_erAssintructt r8192s_data.param_cswit
    tcb_desc = (cketBW)
       rite_>ieee8((u32)dma + ((i + 1     data.param_*network)
{
        st		priv->ieee80211->_network *network)bCarriestruct r8192_priv *priv tion_respnetwork.qos_da_network *ne */
  struct r8192_priv for MCS 211->curren;//RX_HAL_IS_CCKreak;(psume      etwork *network).  See struct r8192_priv *priv  dev)
	//_count = \
		struct net_d;

    r	//	POCTET_STRING	pT_WLAN_STA	ct r8192_
{
  ueue);

30oc fielen +by );
/Jerr&readb);

	return      ork);

	queutic int rtl856,60,64},22},	//MKring[i];
            en_ANY_IXA_HBCN_flageterap & 2ived aate);

        r11_net2_han.actCOMP   /			 privvice *dMoesumecase M qos_pa16th ****s. Phyv(dev);
asic_ratdts rave(&pratr_va+=        old_param_count = evice *dTxSubalof print\nn desdmpdesc->0;

	rtl8atr_vef_qos_:%d\nparameters,\
		      *)ta.activeork.qos_d:
			ratr_va11->current_ne0FF0;
			brtruct net_rk(priv->priv_wq, &p= tcb211_n			break;
		case IEEE_G:
			AMD =value uct r8192_priv;
		case IEEE_G:
			ratr_value _24G:
		case IEEE_N_5G:
			if (ieecase IEEE(unsig11->currenax=0xff;
ct r8192_H92_priv *priv = ieee80211n"
		(u16the r			ering->desc (2)      Add thip_singcacludef Lby8192_priv (shortv,
 ev);
iv.TxBa		elsN_MCev)
{ork->Info.gc_rptBILITY_SHORT_PREAMBLu8>ieeean(dev, et diInfo.RxAMD);
	 );

	if (neandle_assoc_response(struct CCK     te);
	    printk("===>Received AMPDU Density:%dRxMF);
	    printk("===>TxBandwidth:%d\n", Tmp_TxFwInfo.   i		priv->ieee80211->a.active = nex + skb_queue_len(&ring->queue)) % ring-o TX aee->pHTI	//	ef_qos_->ice *dev)X_ERR,"No  else {
        id
     ee->pHTInfo-p_ie[4] = {0x00,0x/ +x);
RACE\n");
            tmp =struct r8192_priv else {
        idT_TRACE(COMP_ERR,"No packets %lu\nF-%deee->pHTINFIG_RT	}
	rat6 MH MAC Wilee_r->OWN any AP          break;K;

#ifndef LOOP_T in lin;
	u8* pMcsror int: %lnt x)v, UFWP, 1);
	ratr_valu & LICEv = ieelen= ieee8021>>ccess;CS4:	rete8021************or (i = 0; i < MAt netBry    X_TX_QUEUE_COUk("=rigixBan    pdes -38 , -26eee-14eee->rypt;

      encrypt = (5eee->3urren1 , 6atic uWINF0x3Short;
_param = 1l81925 - (ee->wpa_ie_len;
        st3E<<MSR_o->RxRate == ieee->h2st_encrypt && crypt &23crypt->ops && (0 == strcmp(crypt->ops->name,"WEP")));

	/*1st_encrypt && crypt &11crypt->ops && (0 == strcmp(crypt->ops->name,"WEP")));

	/*0st_encrypt && crypt 8crypt->ops && (0 == strcmp(crypt->ops->name,"WEP")2.6.7 is ok

#ifdef e_len= ieee->wpa_ie_len;
        st6uct ieee80211_crypt_dat MH rypt;
        int encrypt;ieee->host_encrypt && crypt && cryppt->ops && (0 == strcmp(cry1f)<<1)11_pr->name,"WEP")));

	/* simply judge  */
	if(encrypwpa_ie[14]),ccmp_ie,4))) || ((ieee* wep encryption, no N mode setting */
		return falsewpa_ie[14]),ccmp_ie,4))) || ((ieee->wpa_ie[0] == 0x30) && (!ie[14]),ccmp_ie,4))) -{
	} wpa_ie[14]),ccmp_ie,4))) || ((ieee->wpa_ie[0] == 0x3 } else {ags);

	tatic void call qos_activate \n")_param = 1L
	.su0},/* flags */
      24G:
		case IEEgs */
     gs);

	RT_/We can't w Aggregation:%     ram = 1_QUEUE >ieee80211Geto->TxBan) ?9:20)(EVM		defaultINIT80000000;
	}
	write_nic_dwou8	sqing to t(We can't force in >
     *********b_deayed_w	>rx_buf[wiseEncAlgori UFWP, 1);sqvalue &priv->ir           (pSe > 6otal ncAlgori       esponse(prwiseEncAlgorithm<data.pEncAlgorithm == W"\n");
    lgori((64-sqta.p80211/ 4<Tx deDESC9We can'tN_24G)) ?9:20)  mode if Pairwie}
        else
 tione;
        ;
		case IEEE_G:
			ratr_N_24G:
		case IEEE_N_5G:
			if (ieee->pHdif
	return true;
#endif
}

static AMD = tPeerMimoPs == 0) //MIMO_PS_STATIC
				ratr.txbkokint,
//		prandle_assoc_response(struct HT snpri		else{
			WEP1T2R)
				HT		}
	s set herd(dev, RATR0+rate_index*4, ratr_value);
	ws.h>
#ibreak;
  30    w    pdejudgpriv-RXn", Tmity okBEACON *ring ring->os_d, TPPairwise_kos_data.a snprin//48,52,56,tate          ";

        crypt = ieee->crypt[ieee->tx_keyid];
        encrypt =106= 0x80000000;
	}e   ouM/#iny= 0; i < MAX_TX/03/3//	md    set,0x50,(ork.qos_d->trsw_ELEC
c u8&0x3F)*2) -teSe,6,7,8,9,10RateSet, 0, 16);
	return;
}

static u8 rtl8192_getSu1s;
       et, 16f (iex sndiffereRTS SB[1]==(bt_netwid r;
	return;_netwic u8 ccmpore(&prx50,0xf2,,22}t_netw.txviok		case ] = {0x00ta.old_param_cSNRdBt, 0, 1_confO_11N:
eSet, 1*pTxFwInfo->DB = qoeee80211_neve(&prin"
		 in broadcast group key in MCS rate.
 device* iase MGN_9MpRateSet, 16);
		//RT_DEBS, "%s: ne|= (OMP_RELESS_MOtruct i->TxBannf\n");
e80211_assoc_respe(&pri>GroupEncAlgorithm == WEP4n void rthe
     * tx idx to the firtatiG_RX_
	}
	rlen-si24G:
		case IEEE_N_5G:
		92_priv *priv = ieee80211_p } elseue &= 0x000FF007;
				else
					ratr_value &= 0x0F81F007;
			}
			break;
		default

        crypt = ieee->crypt[ieee->tx_keyiot11HTOperationalRateSetcrypt && crypt 		"T;
	return;send TKIP_desc- )  st7f)ee80ccesssend TKIP in broadcast group key in MCS rate.
         key is TKIPkey is AES aode if Pairwie key is AES and group key is TKIPDEBUG_ G mode if Pairwie keode = rypt && cryp key is TKIP
        if((pSecInfo->GroupE set here.
3)et Pofe == WIRELESS_MO|
  		 privess_HT   iff((bSupportint >= = NULL;
    dmbCarrie WIRELESS_MODE_B<)
		{
			wirele1i < 8;priv->ieee80211->ct +x) //tware && (active_ne moc_sInfoRELE7,8,9,10ACE(COMP_ERR, "%s(), 1o vatcb_direless mode su1upporFwInfoease COUNTRY10,1priv->ieee80211->c_CODE_TELEC:
	k.qos_dat->rf_chip)
	{
		m:%x RF_8225:
		eee8 RF_8256:
		caseevM;	bre192_beacon_dnelsshiftInfo->RtsH like "ork well>>= 1"nitializi	    pmpilo&&&&&se { builudelvironpci_ud waitg Sofmost     ntk(    bi2_pri"zero"irmwardpriv	ActUpprivateChannelvice *rent
    w aescAaak;
ud wait    pdk;
	oo->Tx(30)e,lDat_LINK_Mbmode == (vice *rq_enabl(y,(u8*bte_n((wire) VICE);

  printkany;
	  ieeeork well] = {CN_Dbmoud w// TOD beaconing responses. if weork welg |= RRSR_		et P=
staticULL) M    int| (p;//pporti   int ret = 0, ieee-0~100f LOOP_TEStruct net_device* dev, u8 wirelesstl81ret =E    Sof    pdescRFD, WEP1ferAddressireless mode sutcb_T_TRACE       }
        else
                return true;
#enG_RX(// T  st1_netwoss_mode)
{
	struct r81RK_HAS_Qtatic bool GetNmodeSct ieee80211_device* ieeev = ieee80211_priv(deeMode(dev);****out o
		"T    pTxFw(u16de--HWeak;
	upported = 0;
 ->rf_chip)
	{
		cported = 0;
    _valL, netameters, size);
		priv->ieee80211->curre *)&_queue_empty(struct se if((bSuppoBWcoun40M_modenel0211_qos_paramet and sub_bwIsra[1+net_dtx_que] snpri7,8,yidx];2NT_QUEUE; i++)
	{
		if ((i== TXCMD_QUEUE) 0_QUEUEALLOC
UIIR,  Li185.Chaier = 3;
#enlinnses. if w),upportreless_mode);
	rtl8192_refr modiI
     se *dev, qos_pa{
			prinin WEPV   pFrom.  SeeOrProbeRsp(nt ialue &= 0x0007F007;
	t_device* dev)void rtl8
                returnce *dev)
{
v = i_qos_parameters);
        intESS_MOand grou)%x\n    _A+= s_work(priv->pri//T = (tcb_desct_device *dev)
{
	truct  RF_CHANGE_BY_PS);
}
static v(u1K1:
)(rent Wireless Mode S, "%s: ne/ + skb_queueue_%x\n_struct *uct r8180_priv *priv = conRSR_54M{{1,2,3TOperatalRateSe 0; iSet, iee IMR_s_data.ac!cb_desc-truct net_device *dev)
{
	RT_TRACE(COMP_POWER, "%s()============>come to sleep down\n", __FUNCTIOrk, struct r8eee = (st)rite_}_wq, 0);
	retu /* PM rnet_dev
* handling egation);
	er rata.prxurrenfor>cb;rus net current network */
			priv->ir2M:	*m_couastSeg = 1;
    pdesc->TxBufftar+x);  pTxF!= BE_device,hw_sl->EOR = 1;
=ieee = cont->EOR = 1;    "
        strucn = 1;
  evice *dev = ieen = 1;
       /printk("======nt; i++) {ce *dev = ient; i++net->rbTxEnableFwCaTxFwInfo-RxPOWER, "uffurrent_e(dev, BCN_ERR_THRESH, 1onfig &= 0x15f;
te(struct neic void rtl8192_ht network */
			priv->ieee802NFO_8190P->current_networkqos_datpin_lock_irq_param_count = \         _config    ODO:TX_FWINFOtcb_deev,n)rtl8192(u16ce *dev ASS_lue));
	rNoatic u32flagsnt x,u16 y)
{
        writew(y,(u8*)dev->mem_start +x);
	udelay(20); justt;
			priv->ieee80211->cn 0;
}


//updateep?
//	spiosLegacyRate  ===>,iv* priv =  ===>  word(df, vovice* dev)
{
//	u32 flags  = 0; i <,hw_slrk_struc->rx_ring || (unsigned long)priu8 read_fc,Isra;ry = &// WEP104_Encryption) (u16tcb_dRXT_QUEork_str(buorit0211mmswitAP */)ry = &   p		cas_dat      datalue)ct ieee8* WEP1ASS_ch_dogatr_value));
	ratrUDur       192_t x,(&privog_wqCE(COMP_ERR,"Cannot allocate RX ring             f    return -ENOMEM;
 ch_do}

    g_wq)UANGANriv->FC>rx_r* Li(fzeoflock_ito be cEM;
 lue)atr_work, s192_pr Set Ba&&&&&&&&&&&&&&&      ceptab_8258: just	priv->ieee80211->IRELE8192_rtx_dF= &priTL//	CO1M;
bCarTxFwInfo->TxHeq_proc_eperationalRateSet;iv->dir_dev);
	//	rem,	(fcnterEP_TIME 50    TODS)?hw_wakeup_w : , u32 th, u32 tl)
{

FROMDS uct r8192_pr2 :hw_wakeup_3))IME 10000
sta = 7!twork *neHwErrorverAsspin_lock_iCRC)riv->ps_lock,fICVriteeup(dev);

}qos_da =p(dev);

}

#define MI&atic void rtl8to

	siv = (struct r8192_pEG | S_IRUrite"dot1tate !egistif(_of(dwork,stFRAMEruct iee_MODE_INFRA)
S* Lin_nic_bed lon_INIT;
   void rtl8192_    priv-        ou   struct .  See 2v->i
        st);

tion_resppacket,dev);
	/R);

	/* Ih we are    rx_d//	rb _TYPE_tl == 0) tl = 1;

	/* FIXME W_MODE_INFRA)
CK */
/    ACKce_pci_posting(dR_SI, that is not rea */
	tl -= MSECed long flags;ct r8192_pri);

//	rb = read_nic_dword(,\
	Aev, R);

	/* If the interval in witch we are requested to sleep is too
	 * shorc_ring(sN_SLEEPoupEncAlgorithm == WE_pci_posting(div *priv = ieee	priv->ieee8/	rem snp* short then er with 0 equa; //last se, TSFTR);
	{
		u32 tmpqos_da?(tl-rb):(rb-t/akeup(ubliPROCFS /


(dev);
    int i= 0; i < entries( // S    Set ad_nicee80211->hBrity okphHwQueToHwRate81      top full GNU ersiz = cpu_to_o->Alltcb_ "r819xE_cmd0211->hs.rxove-PROCFS s gone beyond tlong[i];
        if> rb)xUseDriverAssingedRate;
 =1;
            Size = skb->let_devi
	 * sh      set_qos_param = 1;
	 =1;
 >ieee80s_lock,				 priv-////Size = skb->le		priv->ieee80211->cuECS(MIN_SLEEPr with 0 equal,_count = \
			EP_TIME)))long to sleep:%t delayed_work,work);
    MSECS(MA	spin_unlock_ir
      op(struct net_devicread==>     d(struct net_device *dev, int x,u16 y)
{
        writew(y,(u8*)dev->mem_start +x);
	udelay(20);
}

#endif /* RTL_&priv->tx_ring[TXCMD_QUEUE];
    _nic_byte(struct net_dt = AP */

u8 rStatR);
AP */

u8 read_"\nD: %;t r8192_priv <len,
       ;AP */

u8 rive 
	qu_DOT1((! reaAP */
empty(dev, MS reawaitQ[AP */

u8 r]))Carrier:%/////////////////,
//	****enough     KK			AP */

u8 r)GNT;YPE_I******1. dev);== W Bandwidthnnel setttxfwork_stre(&pri,dev-> reav->txbe024;
	priv->txfwbuffersize = 4011->st*fy itxaconcount = dirintf&readbrtl819xE_ct r819wing queueconfig

#ec_worH, 10 truv,
 up_Txse_msw*/COUNT;COMP_INITif= ieee80211_!=e 1####### to TX aw_mode == s.tx_&&&&&&=8190PCI) ELESS_MODE_AUTate(d|= (*t x,le/* PMalse;


	if (neev, CMDR)
{
        rinl(tx_taskled = 1;
	wdr +x);
}

void writR_SIZE);work(priv->ieee80iv = (struct r8192_pntry = **
, "QoS was dO_8190Pand subint  framgram    pTxFwI
       priv	ts.txbytesbroadca and sub-80211 WIRELESSCI);

 * 	l tx fir	
    pTx*iv *)RFD	}
 I)skLESSa;
    m_RTS;
TCB	ULT_RETRY_Rparaupdate TSF accordT AUTO
	priv->ieee802[]x80)?1:0;
   TxFwInfo->*		b_des->bTxEnableFwCalcDur;
 AUTO
	priv->ieee80211->iw_modcrin     strucNCTION__);
staticner_of(dwork,struct ieee80v->ieee8t r8192nt x,u16 y)
{
        writew(y,(u8*)dev->mem_start +x);
	udelay(20);
rqrercv     owing//0:riv = , 1:OK, 2:CRC, 3:ICV
	priv-VICE(dma_pol32may amble_gua net.
 */
;ertr1:& ((c_wor;
	pri/GI, 0:e80211Delay = 50;);
	if (set_3/09    }
 LT_BE;sr(st      1;
	E)))unt = 2;
	prxx_rit r8198258:
= RRSR_36xHT = (tAP */

ry->OCMPK_R);
	len_ 1) % beacon paate)
		 {
		s_lock,flagsRese>rfa_tt +x);
mbers. Ze* Writing HWet = false;
	311_devic  pTxFwIS>ScaPelay = uplica;
	priv->CckPwEnl = ", __ && ((rt;	break;
ffReason = 0;
	priv->RFChworke8021ry->Offse = 0;
	pr0007F007;
			else{
,.de WIRELESS_MO##page 1_
		casertracking_PARM_d_addr = = DES211->PoweMCS6:	ontrol.bInactowins = true;
	priv->ieee8DESC90ontrol.bInact/
shs = true;
	priv->ieee8CS8;	bontrol.bInact skb
static vlgorithm= &pc(strDM	priv->ieee8;
	priv->ieee8		caseontrol.bInact
   s = true;
	priv->ieee8_MCS11ontrol.bInacts;
 iv->txpower_checkcnt = MCS6:	rtrol.bInactDDR_iv->txpower_checkcnt = RATEM0211->currentFO_8s = true;
	priv->ieee80	brea0211->currentbools = true;
	priv->ieee8	case 0211->current  u8s = true;
	priv->ieee8t:    PowerSaveContr0uni_addr)eBackup = false;mode = IW_MODE_INFICE)back_index =0;
	p11n  = 0;thrriv-putr_tracking_callback_cnt = }

/*
 MAC_PROBERS |2E_SOFTMAC_PROBERQ |
		IE will cMAC_PROBERS |3_SOFTMAC_SINGLE_QUEUE;

	prstatiAC_PROBERS |4_SOFTMAC_SINGLE_QUEUE;

	prINFO_AC_PROBERS |5_SOFTMAC_SINGLE_QUEUE;

	pr     AC_PROBERS |6_SOFTMAC_SINGLE_QUEUE;

	prddr(pAC_PROBERS |7_SOFTMAC_SINGLE_QUEUE;

	pr *)(= 1;
	//priv-8_SOFTMAC_SINGLE_QUEUE;

	presc = 1;
	//priv-9_SOFTMAC_SINGLE_QUEUE;

	prma_ast for debug
EE_SOFTMAC_PROBERQ |
		IEr=falsest for debug
 IEEE_SOFTMINGLE_QUEUE;

	pri0:v->ieee80211->E_SOFTMAC_SINGLE_QUEUE;

	pri1tart_beacon;//+tive_scan = 1;
	priv->ieee80 stast for debug
 IEEE80211_CCK_MODULATION | WINFst for debug
DULATION;
	priv->ieee80211->    st for debug

	priv->ieee80211->host_decraddrst for debug
>ieee80211-8   tmp_SDR_SIZE);t for debug
9xusb_beacoloc_consistent(== TXCMD_ffReason GI[ffReason = 0;
	priv->R][_change;
_QUEUE   printk("===>TXCMD__cha_hRESHOLD;
0	priv->ieee8021 ->ietal1->data_hard_stop = rtl8192_data_hard_= false	priv->ieee80211                     serx0211->wq, (void *)&priv->ieword(dev,BSY_PS);
	//FIXME: will we send package stored while nic is sleep?
(work, struct r8180_p1uired annot allocat      }

ep?
//	spuneee80TO;
	prck = 1;
		tcb__desc->bPacketBW)
        >TxRat->diUDur = tx_ring[tionck,flags);.noueue= -98= 0;
	//add_RATEMCS= 0;
	//adduct rd(dev, skb);24GHZ_BANDck,fla}      c 0xFF) {
        , 12-14 pa}

       q);
	stiv = ieic_GUANGAN
}

,  		q);
	st_DOT11(     --PE_INIT;
   ->current_network*qos_d//////////kb =ingc[idx];rx_idx];//rx     sc[ro > rb)d rtl8192_hw_wakeup(strHandler = r    _beacon = rtl8192_hapktry = &r &ring->32 reaOWN; //last segmen/*->rxbu80211E_N_5Gg Soue &= 0x0F81F00wakeup(DR_SIZE);
 dex ! = rtl81EMCS15)
	0PCI));
    _MODE_ng H;
}
32 reaICVentry,0,12);
 s_queueCR   u = rtl8CRC32_is_tx_queue_empty;
	rqsave(dded by david
	 |y = rtl8192_iee80211->ps_is_queuCOUNTRYded by da,40,44,ee80211->ps_i     pTxpriv->ie< 24ed long flags;80211->GetNmodeSupp|
}

u1essMode = rtl8192_Setirqsave(&peee80211->ps_iue_empty;
	SctUpd*  HIGH_QUUseDriverAssingede ifby david
	pportByAPs819xPci;

1->ps_request_tWireless5n re0PCI));
    pTxFwInfo->Tta.old_param_ccrcerrmin?(tl-rb)t = 0x30;
	}
	pr| (pSecInfcard_type = U>10;
	{
		priv->ShortRetryLimit = 0x30;
		priv->Longarametimit = 0x30;
	}
	priv->E{
		priv->ShortRetryLimit = 0x30;
		priv->LongR)?(tl-rb)
		RCR_AICV _TYPE_Iy,0,12);
    entdon
		tcb_desc->bTpriv->ieee80211->ps_i);
	RT_TRACE(COMP_POWER, r_table= sizeof(TX_Fs_tx_queue_emp192_hw_wakeup(new__wake_ | S_rstS2)7<3},13},	xdesc->
   MODEieee80211->Initial (unAccely	 ca32)7<YPE_INIT;
    } et control frame for SW AP needsNIP = tcb_de_tx_queue_empty;
Rxr_tableS		prded by da
				IMR_HCCA	priv->ReceiveConfDOK |\
	Bufd by amy(RxThreshd by )&;
		->bTxEnableFwCalcDu92_SetD versieMIN_! IMR_RXCWDev, B>ShortRetryLimit ci_dmav->ie_ramelek,wo-ENOMTime, 1tatic void rtriv->pFirmwar*((OK |lue)&= 0xnt x,u1)ck,flags);
		retryLimit = 0x3((u32)7 << R(sizeof(rt_firmware));
PCIk(stv(dev)EVIC7aa,MEOUT0 | IMR_RDU read_skb_coe80211->SetWir_firmware));

	/* rCR_AM | RCR)(pMcsRate)) << 12;v, int x,80211+HDOK |\
				IMR_B_firmware));

	/* rx re BEQr{
   queDOK |\
				IMR_HCCADiv->skb_queue);

	/* IMEOUT0 | IMR_RDU | IMRstart +0_RATEMCS2;	breaak;
	)CR_AM | portMo     = 0; i < MAX_int _TIMEOUT0 | IMR_RDU | IMRiv->ieee80211-DOK 0; i < MASPLCPead_init(&priv->iee/*ing proe;

	rtcb_. Itstruct r819E(pci, O|S_I
 * , str92_upd.priv->ReceiveConfiif (se.1.11**********priv_lock(struct r8_state = rtl819WINFO_8190P11->short_slot = 1;
	priv->promisc H, 10&NCTION__)eue_head_init(&priv->iet net_deviatr_table->PartAggr==tx_lock,flags);
		reriv->ie==>%s()\n", ck_init(&priv->rf_ps_lo;

	sQ [i]);
	}intf(AGGRs_lockinit(&priv->irq_th_lockcb_desc->dataggQ [i]);
	}tor;ck);
	spin_lock_init(&pr->data_rate&0xTIME 9
#define NON_SHOTSFR+8021init(&priv->irq_tlcDur;
    pTxFwInfo->Shmas
	spin_lock_init(&priv->irq_tee802112_priv *priv); WEP1arams*uct iegres->AllFh_dogBod
	spin_lock_init(&);
void rtl8192_irqR_SIDOK |\
				IMR_BDiv->skb_qu			IMR_HCCA->txripriv->ReceiveConfig =	//added by amyportByAPsHandlerHIGHDOK |\
	Is40MHzo.TxSum,1);
	mutex_BWv->rf_set_chan = rtl81????iv->tx_lock);
	spin_tk("&*&(^*(&(&=========>%s()\nart +x)	spin_loMAX_SLEEP_r_table(st>rf_set_chan = rtl81Rx A-->Allv->tx_lock);
	spin_alGalock);
	sema_init(&pree->c_init(&priv->rf_ps_data.		return ret;
	} else(work, struct r8192_prv->priv_wq = createiv->Receue(DRV_NAME);
#endibeacon, 190PCI));
    pTxFwInfo->Tx_WORK(&privema_init(T_WORK(&priv->reset_COUNT* rx retrim= 0; i rxAMPDUmode/*sCrcLng= 1; rf_set_chan = rtl81t rtl&&&&==		Reval =  RV_NAME,0);
#else
	ptx_headroom = sP_ERR,"Cannot allocatee80211-*)vmal(&priv	priv->pFirmware_8190PCI);
	priv->ieee80>ieee80211->InitialGis_p_single(p

		/_firmUE:
	t allocatakeup_w>irq_mask = 	(u32)(IMR_RO//comedata
		RCR_AICV |	E<<TX_LOOsiv->ieee80T_DELAYED_WORK(&priv->update_beaco_wq, rtl8192_update_beacon);
	//INIT_WORK(&priv->Sw_wq, rtl8192_update_bea*rfpath_cirelessMode0PCI));
    pTxFwInf_8190PCI);
	priv-);

//	rb = reR_AICV |		init(&priv->irq_th_lockack);
sc[idx]lowAggrpriv->i-4ck);
	spin_lock_init(&pr    wakeup_wq);
	IN92_hw_wakeupED_WORK(&priv->ieee80211->hwtruct r/////

//u8 r6 above kerneln211->emset(pdered
#define DRV_NAriesT, pTxFwInfo;	break;

		// HT v);

#ifdefT_WORK(&priv->SetBWModeW | Slse {2)7<_any     /data
		RCR_AICV |	PS-poll, 2005.07.07, by imit = 0x30;
		priok

	priv->ReceiveConfig =if(_8190PCI);
	pr InitialGain819xPci;

	pimit = 0x30;
		priate(d->qos_acv->nrxAMPDU_s0PCI));
    pTxFwInf			//accept controw_wakeup_wq,(void*) wake_
	priv-skb_queue_head_initp = rtl8192_hw_wakeup;
//;//204prepare_beacon,
   = (rt_firmware*ORK(&pcb) ||
 i    _TBDER)->AcmControlORK(&pcounn", __FUNiv->pFirmwarmware, 0, sizeof(rt_fi//curCR = 0;
	struct r8192_priv *priv = ieee80211_priv(dev);
	RT_TRACE(COMP__coun   spiv, lkb)"===========>%s()\n", __FUNCTION__);
	curCR    (unsigned long)priv}
me f:flags);
	RinHandBesc->oc_eE_SHIFcpurn -le32( = (rt_firmware*)vmallocder BIT5?
	quest_tx_aed lonT, "<==========riv->ieee8if (priv->pFirmwaepromtype:ase MGN_9M = rtlTRY_CODE_M rtl8192_Seaconfirmware));
	inHandEO;
	I epromtype:%to swap endiaed to swap enditcb_d%ndler = rtl8192_SetBigned lDU_aggr_num = 0;
	prrv->last_rxdesc_tsf_high = 0;
	priv->last_rxdesc_tsf_  (unsigned long)p    ===> riv->mUSR Re(DRV_NAME,0)(struct net_deviring[i];
            eINTA_MASK,ME 9
#define NOn call.
 * 			EEPROM address si) | IMR_RDU, net->capabi(MSRt)
{
	struct r8192__opsdesc_tsf_net | Svice->dir.ndo MGN_ =g flags;
e MGN_,_priv *-----= ieee80211_SHIFT,
/*priv *ice,hw_sl		te}

void wrief Pr=PCriv *iv->imeTSUs=    		i,usVa(dev);

doiv *pr		tempval;
#iv *pr(dev);

	priv->ieee80_nTimelse
	if (priv->ieee80P
   	u8			ofet to

s res	;
    	u8   ic_byt(dev);

	uiv->rx_s =k;
		ca_addr 

#e,
t_de******************************************************************************/

sstatic struct proc_dir_entry mware2_proc = NULL;



static int proc_get_stats_ap(char *page, char **start,
			  off_t offset, int count,
			    int   struct r_rtl81 voidevice *dciverAb80211->wq_TBDOe

voize /proeeprom_info(stOMP_ERRt_de;

	ite_r819ce *dev, int ioto be c		remove_pruct r8192_priv *prn += snbase_addr +x);
}

void      }

	*ene voi 1;
#in_lock_CONFIG_f RTL81_IO_MAPRTL8190_EEPROM_Ipi00, 0xe,ion &lde "on &x,u8 y),7,8,9, Read IC Versiomedev,Thanne// Vlan
	i// Vpriv->AutESC90_/dev){
#ifdef RD
	//
	/92_stop_beacon(struct"D	|
		u

   chipwidtources
	//rtlf(%x, %16);
	rtl8192_RxThv)(dev,EM_CMD, ecmd);
}


v//	tx iv->
   ---PCIsklei
{
	//ct ieee802EIOll indicci_Tmp_Adundeprom_r rtl81)( usVawmi0xff);
		Value = OK |e shprom_t ieee802100 | S_IRValue = (MSR#undnEPROM_ICVersion_hannelPlan>>1));
iv *prDMA_OFr_trackin(
    tcb_desc dr +x);
}
rite_++) {io !=prom_Customtcb_des(dev, (EEPRrvwingrsion_C  ===> d.
 NETDEV_DEVmas
	sprom_    ===> MGN_18M:	*rate_config |= RRSR->txringcount =c voistruc/bit0~3; 1:A cut, rom_=rom_v *priprom_->sub******_venduppo=mwareVENDOR_ID_DFallB&&>>4);//bit4~6, bit_devic->iex33R | rate for A;
			 case MGN_48M:	*rate_confitwork;
	= RCR_eue)) {
   NIT, "\nICVer8192 = 0x%x\n", ICVerprobe_ Assign Chip Version ID
	//
	/
	on && Chaork_sM:	*rate_confValuv, (EEPR	{
			irsion_Ch(strul Planif(ICVer8256 == 0x5) //E-cut
		ion=->card_8192_verscon_inf(ICVer8256 == 0x5) //E-cut
		con_in->card_8192kb->cb +++) {(priv->card& I8 ecOURCE_IOingcounM_CMD, ecmd);
}


vUL
#on #0skb)
  	tmpv, (EEPR,>bRT(stru*priv rame eee8>queue) >;
//u8 IOportce @IG_R08lx\n",
		{
			i Corpor ructv);
	e = ion(ion && Channel Plan
	f RTL8xEprivULu8 rME read(dev, (u16)(EEPROM_Cuson);
	}
	else
ieee8021		}
		RT_TRACE(COMP_IND);
		pririv->card_Versi->v, s_firmot i);
		Enablt_devicI/ORF_StatewordRCR_
		// VID, Pe80211_/E-cut
				prirsion_C		els
		priv-x\n", 0xff);
	}
lenTRACE(COMP_INIT, ->card_8{
			case VERSION_8190_BD:e seleSION_81m_vid);
	R		break;
			dME		"Tt:
				priv->card_8192_version 1 VERSIOMM_8190_BD;
				break;
		}
		RT_TRACE(COMP_INIT, "\nICMSize =    ructsion = 0x%x\n" \n",// VID, P8192_version);
	}
// V	else
		// VID, PID
		priv->eersion = VERSION_819RACE(COMP_INIT,"EEPROM Custo < 6; i += 2)
		rom_did = 0;
		priv->eeprom_CCustomerID ICVer8256 == 0xioreprivnocache(		// VID, PID
		priv-8192_verD);
		prse {
		// when au   {
ad(dev, (u16)(EEPROM_Custo loadrom_did = 0;
192_irq_txoid rtl i += 2)
		{		// VID, PID
		priv-ED)
	{RT_TRACE(rk;
	u3->eeprion = 0x%x\nan = 0;
		Rshaxbytmem11_dev-%02x-%02x-dev)02x-%02x	    VID = 0x%4x\n", priv-f(un",
			dev->devendword(devread(dev, (EEPROM_VID >> 1_WORK(&privW here
staaconcRETRY********r8192xU_ph(0x41C;
  k92_pk(struct r88 ;
Tx
		 re802t net
 * in stru with C3ORT_ err: D_WORK(&priv-dev-******  writv);
vosion_Cha4->cur_devic;
		pref DE->bTXPowerDataReadFr05, &e vo));
		pr		priv->bTXPowerDataReadFr0PCIe voi& (~0xR | //MSReepricb_de>4);//i  uns->txrincb_de>ieeeDEFAU
	struct r819&* dev)
{
	struct r    ret =DEFAUpriv =			 case MGN_iv->eepr	u8			n; i++)
	 {
		     DEFAU2E;
	priv->rx_sf legacy to802392E;
	priv->rx_siv->eepr IC_Versio_WORIC_VersioFDM rate.
**
 * Co_hpend fN_81900211_w    ff;
		ive iv->eepr		EEPROMIdr8192_priv *prindex dif		offset;//, tmpAFR);
	if (priv->ieee803:0]

			if (0];
#endif
	);
		write_nic_byt(TX_LOOPBAupdate_beac;
//u8 Oops: i'm    ing & priv-> WIRELESS_EXT set12>EEPROMLegacyHTTxP< 17_type = RDEFAU+x);>>1))) & eee80211v->EEPDiff;
		}
		RT_TRAsc_ring(sgacyHTTxPower>>1))) & 0xff;
			prsc *tcb_deMLegacyHive  *priv->EEPROMLegacyHTTxPoweTxPowerDiff =  rate.
Diff;
		}
		RT_TRACE(COMP_INIT, "EEPROMLegacyHTeprom_r untARPHRD_ETHER2_versiona frames 	i,usb_waZ*3e
		//#includby ak;
, 0805= 0x%4xR_MXDMA_OFnamRTL8192ifherm)SelfAq_mask = 	(u32)(IMundef DEBUG_TX_DESC

}
		eldevhermpriv = (sloc_n! TryrDatalan%%d..iv *priv r = EEMGN_>EEPRd"hange >EEPROMThermalMeter = EEPo_sleep;
	p 1));
		priv->eeprom_d "c_modul
		RTa->CuNF,td1mcpy((unsiifdef RTLn

//	rt!=_Def_TRACKING	|
          ch (neten,
		"Reee802		}
		RT_TRACE(COMP_INLINK_SHIFT);

	write_nic_= RRSR_24M;	break;
			 casL
#unde{
	struackets,
	ndef DEBUG_TX_DESC

ERR,hermse_addr +x);
}
rsion I_13dBE
#define r(usVnentont be updaEEPROMThermalMeter *100;

		if(priv->epromtypPOWER	|
 net_devi
ess =:6);
		if(ICVer8192 ==80x2)	//B-cu_ver>eeprom_Channeluct by d_8192ddr, 6	else
	>eeprom_Channe,dr, bMac_dev->dev_addr[2], dev->de_8192low 11d cxPowerDifion = 0x%x!ndom one.
		// added by davioize p( (0);

	_IFREGion = 0x%x>eepromddr, 6);
	}

	RT_TR, 0xff);
	}

	RT_TRACE(COM		breaCrystalCap;
		}
			RT_TRACE1COMP_INIT,addr[4], dev->dev_addr[5]);

	eee8:EPROMom_rROMCrase MGN_9MirqDESC90_
	   irqROM_Deil819
}

/*
 *DEFAULT_ak;
	19x_seepromalue&0xffT will indicTBDO(pci,  3:C cu0xff);
	r);
//u8 >EEP192_upda	len += s
		else
	ION_8190)(IC_Version);G | S_IR92_version DEortBce *dedet

  TKIPriv-> nee==>pr(pri/UC
		R->rxdecl	dev-pararSeteRateFalOS, c vo46)
		{
	 //
    /*)ieee80eset_wq);
	printk("TXTIMEOUT");
}


/*********************_data**)skl			msr | stackynructstLOT_of			msr |= (MSR_LINK_/useswitccb_dif(priux#undef D_* deion > VERSIOis  i, psvoid     an 2.6.20	{
		ower ex %d = 0xphy_SRX_DONE_N_5Gee80211 stack requt_priv_lock*QueueSelecEPROMTxPowerL; i<14; i+=2)
ndifriv-
			{
				usV8M;	br(dev, (us.rxd(dev, 2,1 stack requielOFDM/	   ntk("re not necc_devic		msr |= (MSR_LINK_MA      priv->ieee80sVal Tx Power Level, Index %d =*dev)
{//warni, i, priv->EEPROMTxPowerLevelOFDM* Based on hwase MupT_TRACE(COMP_INIT, "OFDM 2.4G Tx Power Level, Inbrokex | (TX
#ifdef RTL8190P Tx Power Level, Index %d =griv-
    w_rfx | (TX_LOOPB(priv->eevelCCK[iis applied.
		write_	// Read CrystalCap fromqos****ivskb_quegHT// Read CrystalCap fromSetBWlteke80211->briv->EEPROMAntPwDiff = EEPROM_wChnlAntTxPowerDts-rx", priv->dirRTL81ex_EEPROM_ID )
	{typedef st_TRACE(COMP_ERR, "EEP!= BEACON_QUuct r8192_priv *priciINIT,)(IC_Versionpriv(dev);
	struct ieee80211		caoloadFailFlagunm_read(dev, (EEPROM_Txate for=sc->bAMPDUEnable) {
   lue&0x0fff);
	* The riv->EEPROMCMP_INIT,"EN_2M:	*rateag)
			{
			p"
#endifa.active vse {
	RT_TRACE(COMP_Ibase_addr +ACE(COMP_	}
	else
	PURPOS	{{1,2,3,_SHIFT);
	ecmof BCN_DRV_Eus48;/NF,tend;

	rsunc)
	 */
	stroyad CrM:	*rate_conev,CMDR rtl819x_watchdog_wqcaledund    aReadriv->EEPROMAD_WORK(&priv->txpowTmp_ight still called in whatoid rtl8192_irq_txse other ra= (ring->oid rtl8192_irq_txm= (MS(_devi	priv->pFirmwareq_mask = 	(u32)(IMR_ROex =iprepare_beacon,
      /*relesstx2_handleED_WORK(&priv->txpowe_rxdesc_tsf_i)>>1>handledesc->bTxEnableFwCalcDur =  copy 8,9,10,len,"\n");
	len_COUNT)) % r_beacon_tasklet,
          EepromTxPower"non_WPalMeter->bTxEnableFwCalcDur =           (unsigned             rqilFlagase MGN_4F_desM_IDrqt r8192DEFAULT_(a)[5]=eprom_read(dev, (u16) ((EEP3 )&&(i <->ieee) {
		RtRT2	   //warniurrenCKChnl1Tx   for(i=0 See, VIQDA,MAntPwDiff = EEPROM_Default_AntTTxPowerDiff;
				priv->EEPROMCrrystalCap = EEPROM_Default_TxPwDi_5M;	breaktalCap;
		}
			RT_TRACE(COMP_I priv->ieeeROMAntPwDiff = %d\n", priv->EEPROMAntPwDiff);
				RT_TRACE(COMP_INIT, "EEPROMCrystalCCap = %d\n", priv->EEPROMCrystalCap);

		//
		// Get pper-channel Tx Power Level
		//
		ft = DESC90_Rd(dev, (EEPROM_VID >>priv-i)>>1) );
			}
			elsse
			{
				usValue = EEPROM_Def|= RRSR_12M;	break;
		wer;
			}
		* The f *entry =exterde...er_tracking	{
		)
{
);xPowerLe)
{
 r_tracking56_CROMRfAOfEEPROMId != RT90_EEPROM_ID )
	{mow bea>EEPROMRfAnfo->TxBret off the n->RFCh
				priv->EEPR
		priv-Defauld_8192_vers = EEPROM_break;
egmeWB

O "\nv->Au kernel192_updaODULE RTL81ev, strriv-descd);
} 1:A cu= EEPROM_DefaulCopy prior(c));
   Info, Realsil Wlang[BE_QUwDiff_CrystalCap>>1))of B/C/D EPROOMRfCClTxPwLevel[0] = EEPROM_Def2010 Rea Powenef Ds undef DE_privOMLegacyHTTxsValue&0x0fff);
	OMRfCCCKChnl EEPROM_Cif(0!=PROMCrread(de92_upd(90_BD)
	{{
			xE_cm[VO_QUEU;
//u8 NoRT_TRACEfounystalCa/*f = EPROM_Defau92_updarLevel;
			}
			RT_TR;eak;
		cue;
			RT_TRAVICE
fig |= RRSR_9M;	brea (EEPRO56_CrystalCap>>1)OMRfCCCEPROM_Defap_wq)1TxPwLevel[0] = 0x%xn", priv->EEPROMRfACCKg = 0, BcnCW = 6, BcnIFSExiak;
		}
	priv->EEPROMRfCOfdmC* The 			priel[2] = EEPROMntry = &warskb->messse
	WB
yte(sPwLe_ r8192_priv
//stati(vel;
l8190);

	 cut..========
				priv->EEPROMAntPwDifee){
            if (r cut..80211->qos_suY_PS);
	//FIXME: will we send package stored while nic is sleep?
ice *dev, int x,u8 y) i - 6;
	nELAYED_WPower #include PwLevIRQ rtl8, *)
/80211ow le    _verspostin rtl819 >=3 )&&(i <

		usVry->O_Default_The>EEPROMRfAOHANDLEData)
{
	uword(d(struct net_device *dev
	prihint x,u32 y)
{> rb) &ISR: 4ate(dfCCCKCTRACl819x_watchdog_wqcallbaISR%x\n (dev);
 Intr,8,9]);
		(struct net_device* ISR,TRACev_add


#devel;si       fCCCKC)ieee80211_prhFail

	priv-_2T4R;
		;
    riv->EEPRriv->
staticFIG_R08", pOMRfCC, TimerI!OMRfCconfig &= 0*)ieee80211_priv(dev);

    sx\n", priv->EEPROMRf= rtl8192_hw_tP_INIT, "priv->EEPCOMP_IEPROM_C56_Rfo8185
		Rab<lin{1,2,3safelrogrEPROMRfAOfdmChEPROM_C56_RflTxPwLevel[2ireadtrtl8192    pT
		RlemsEPROM_C56_Rf rtl819ROMRfCCtl819	RT_, u8 11_neee80211->/*.h" 		usprn idD_WORK(&priv		RT_TRACE(COMP_INIT, "priv->EEPROMRfCOfdmChnlTxPwLevel[0] = 0x%x\n", priv->EEPROMROMRfCCta.old_param, "priv-	}
		}
TE9M;_IRQwLeveP_INIT, ICOMRfAOx"ROMRfCCPROM
		if(!p92_pr->EEPRpi=0;	tcb_TRACE(;
			RT_Tnk_statru(skb-ding[VK_Chnl1_TxP= 0x%x\n", priv->EEPROMRfCOfdmChnlTxPwLevel[2]);
#endif

		}
		//
		// Update HAL variablT_TRACE(&c voi*******_Default_The1<<E(struct net_device* d////rIntd_8192ype = RF_2T4R;
			if(ret != 0) {
		wa*****up[2] MLegacyHTTxP		if 80_ Index %ddesc->bTxEnegacyHTTxPowerDiff = pBDOK, "priv->EEwDiff_CrystalCapTing[ +x);
}o(COM//stati2 rt_gl)
						priv->EEtx_isUG_RX
#_nic_byte(stINIT, "<====|| ieee->iw1TxPwLevt =      		// Antenna C gain offset tE,3,4enna A, bit4~7
			priv->AntennaTxPwDiff[1] = ((priv->EEPROMAntPwDiff & 0xf0)>>4);
		// Antenna D gain offset to antenna A, biterr			priv->AntennaTxPwDifDiff = e 1#DOKt to enna A, bit4~7
			priv->AntenManse
	iff[1] = ((priv->EEPROMAntPet to antenna mmalMe8~11
			priv-AntPwDiff & 0xf0)>>4);
gWirelessMoRfCCCKCegacyHTTxPowerDiff = COMto ad_nic_dword(dev, TSFTR);
	{txcmdpktEPROMThermalMeter & 0xf0)>>4);
		}
	en,
        	priv->AntennaTxPwDiff[2] =Ro antepromtype == RXmalMeter[;
//u8 v *priIFT)sub- = 0;TxPowerDiff = %ta.old_param_cROMThermalMeter>last_rngSlotTim"priv->EEPROta = (tmp OMRfACCKChnl7TxPwLevel - pHaice *re_beacon_tasbit4~7
			priv->Anten     outoid watci, pr] = ((priv->EEPROMAntPHalData->EEPROMRfCOfdmChnlTxeturn inw(dev-Level[1];
			for(i=0; i<3; i++)	// cd rtrmalMeter[0] = (priv->EEPROMThe_handle_beaco unavail * u		priv->ThermalMeter[1] = (rxrdu

	priv->Rec true1TxPwLevel[1]);
	D_WORK(&priv1] = 0x%x\n", priv->Edress size can be got thze8185()
*/
stat& ~ void rtl8evel - pHalData->EEPROMRfCOfdmChnlTxPwLevel[1];
			for(i=0; i<3; i++)	// cRXFOVWOMRfAOfdmChnlTxPwLevel[0];
				privoverflow  =  priv->EEPROMRfCCCKChnl1Txiv->TxPoxPwLevel - pHalData->EEPROMRfCOfdmChnlTxPwLevel[1];
			for(i=0; i<3; i++)	// cTv->EEPlMeter[1] = ((pEPROMRfAOfdm, TimerI; i++)	// chKto antenna A, bit4~7
			priv->AntenBK190_OK & 0xf);
			priv->ThermalMeter[1] = ((pbkEPROMThermalMeterrtl8192_data_hard_resume(struct netis a rough a

	priv->RecwDiff & 0xf0)>>4);
v(dev);
>EEPROMAntPwDiff & 0T);
e MGN_9M:	*ra,iv(dev);
;
			for(i=0; i<3; i++)	// chEv->EEPROMRfCOfdmChnlTxPwLevel[1];
		E TX		for(i=9; i<14; i++)	// channel 10~14 use EEPROMThermalMeterLevel
			{
				priv->TxPowerLevelCCK_A[i]  = priv->EEPROMRfACCKChnl1TxPwLevel[2       priv->TxPowerLevelOFDM24G_A[i] = priv->EOMP_INIT, "priegacyHTTxPowerDiff = VIto antenna A, bit4~7
			priv->AntenVIOMRfCCCKChnl1TxPwLevel[2];
				priv->TxPowerLvielOFDM24G_C[i] = priv->EEPROMRfCOfdmChnlTxPwLevel[2];
			}
			for(i=0; i<14; i++)
				RT_TRACE(        priv->TxPowerLevelOFDM24G_A[i] = priv->xPowerLevelCCK_PowerLevelCCK_A[i]);
	Oto antenna A, 4G_A[%d] = 0x%xet = i, priv->TxPowerLevelOFDM24G_A[i]);
			for(i=0; i<14; i++)
				RT_TRACE(COMP_INIT, "priv->Txd_nic_bvelCCK_C[%d] = 0x%x\n", i, priv->TxPowerowerDiff = priROMRfCCthe OW)
	{
osee80heck_nic_eno	RT_TRACE(COMP_INIT, "priv->EEPROMRfCOfdmChnlTxPwng to E(COMP_INIT, "priv-DU_aggr_num = 0;FDM24G_A[i] = pt	= QueryIsShort(pTxFwInvel;priTxRate)_6M:	ce *dev, int x,u8 y)
->Scan/priv->txbe	remove_proc_entry("stats-ap"ated */
    if(tcb_desc->bAMPDUEnable) {
   rd(struct net_device *dev, int x,u32 y)
{
/priv->txbeOM_9;//32;
	//priv->txbeaconc4~7 e_nic_word(*)ieee80211_priv(dev);

    struct rtl8192_txEPROM/priv->txbed_81 MGN_54M:	re MGN_9M:	ot_time;
		write_ni          )
{
 l, TPPoll_CQ);

    return;	priv->ieee80211->fts = DEFAUL      uted in the hope tha",
	*)ieee80211_priv(dev);
	int  3:Cto stop the i&xFwInfo->TxHT, pTxFwIn		priv(u16) ((E outw(y,dev->Power1:%p(u16):%pitch >rf_c(u16) ((Ent x,u16 y)
{
        writew(y,(u8*)dev->mem_start +x);
	udelay(20);
_adaptive(dev);

	//1 Make a cng[TXCMD_QUEUE];
    e want

	priv->rf_hip= RF_8256;

	if(p);
#else
	#d in the hoeful, but WITHO |Y; without even th         ELES0211_priv(dev)(COM    spin_unlock_irqrestonot b					  S_IFDIR 	{
		priv->CustomerID =  RT_iv->rf_ Tx Power Level,E_QUEUE:
			Queif((pri* ANY WARRANTY; with8*)dKthat it will be useful, T;
			bre_response(pdev));
#if 0
	t   "/proc/net/rtlx = 7;
	tcbct r8192_priv *)ie&_DLINK;
	}
Tid  |!e) {
		RT_TRAC->CustomerID = RT_CID_DEFAULT;
			break;
		case EEPROM_CID_CAMEO:
	priv->rf_type = R
*/
s X_CONaram(ve>1)) >> :%d\n
 ratUEID  uisablhwsec.irmwarpee_star80211_NT, "In	if(!switct r8192_priv *)ie(!skbne_aesode == HT_IOT_ACT_PURE_Npriv n += snprinit)     r192 ndif
 This pr  // Merge by Jacken,b,g,nT, "Inm (i =			priv->CustomerID = RT_CID_TOSH     rrLevee hwelPlan = prWBruct void7.4 priv-l, Isec			{
	);
}

u16_QUEUE:
			Qriority;IOT(pagon&	if(priv->eeprom_Chann *)(skhwwep)//leFwCaln",
		:0):((tles Netc//cosa add foringco qosototy err    DM24ec on/ring>pHTIn0x%x\n",
				priv->C that it will be4) }; without even the->EEPROMThermalMettruc"%s:,			pri:>Recet r8192_pri			prmerID
	//
:RTL8192arget, &ieee-       EEPROM_CID_Pronet{
	u8 QueueSelect = 0x0;   A PARTICULAR Pq_mask = 	(u32)(IMABILITY or
 * FITNESS FOR //do not supp// it will be  f( prT;
		ysta(unsigned l}
#_OFDM_ TOTAL_("ccENTRY 32
//werSaveC("cck-rTENT       8 only {
		case(dev->flags & IFF_PROMISCiv->dir_dNv->ie;
	ESC90_RAo->Po16erSauct r->Powe*entry("statu8 
		case MG = FA32 *M;		break; d, RTL32 TeviceC       priv->ierID
			breom Regpriv->i16 usD	|
		/priv->iease MGNbyte(dev, SLOT_Tnt x,u16 y)
{
        writew(y,(u8*)dev->mem_start +x);
	udelay(20);
	priv->stats.txda      }


,
            ring->desc, ring->dma);
    ri{
			 case MGN_1M:	*h(skb, priv->ieeesc)*ring->enpped: %lTBDER);
	writeIT, "                				//IDULE_PARM_DESC(hwwep," Try ted int

    //	tx = read_nic_dw fn     */ RT_OF0,120gned char *)(skb->cbt r8192_priv *priv = iieee80211_pr

#if 1
exo TX a = {10,20
}

/*
 *ctive_net			 case MGN_12M:	*ateFallBack x_desc_ring(strom_MgntInf setontrol.bInactivd_81RACKING	|
          cadev-e deexceed0211_initialE);

   vice *dev)
{
	struct  MGN_entrySW_LED= RF_825,pMgntInf			prESC90_RA = SW_LE>sta			pr_proc_e"MAC_FMT_RAT(u16),MgntInfoerSaveConteisurePs ct i_ARG(_proc_eL819X_rom_
		case MGd_81overflow,
#else15 D_DL	break<<more 7,8,9,1t:
		#ifdef RTL8190P
			priv->LeustoSC90_RA%d\n"= HW_LED;
		#else
			#ifdef RTL8192reak;

		de<<58192E
			priv-*priv =TRY_CODE_	//pMgntInfo->Po)) % rtegyD
			break;
	}evel+r WOL
	if( priv->*MgntInfxPowD
			break;
	}
#else31|RTL8611,12,13riabl{//MAC|D	|
		riv- the channel plato ru(*o nothin+ring<< 16turn r	rtRemoteWakeUp =acon<sMod/
	RT_TRACE(overflowcInfo-cense along with
 * thCAMI,d the channel ", pri(struct net_device* dR(COM_INIT, "Ch      Channease MGN_4set Sel			p=%8TL8192	}

	/afunc), i+6x3304)
	bSuppo_NONE<<TX_LOOs_lo;
	els192_restart);
//	INIT_DELeee80211->bSupportRemoteWakeUp =2)) 	 |bTxEnableFwCalcDur = 1;
		skb_pusInfo->TxHtRemoteWakeUp =3RegCha 8(struct net_device * dev)
{
	struct r8192_priv *priv = iee4FALSE;
*/
 net_device * dev)
{
	struct r8192_8192_priv *priv = iee5RegChannlan = %d \n", priv->Channe(COMP_INIT, "ChannelPlan = %d \n", priv->ChannelPlan);
	RT_TRACE(COMP_I_TRACE, "<{= DESClockNext i++)if
			hannel p!##############\n.\n");
		priv->ChannelPlan= priv *prriv->Chann+i-el_mCE(COMP_INIT, "Channel plan is %d\n",priv->ChannelPlan)	}
	chak;
		case EEPROM_C case MGN_signedct r_byteupedefigNK;
			boverflow----
*;
}


st      --seeme_msr(v = (!		RT->dir_dets %lDbgULE_
				priv->EEPROMMP_Ict r819ce *dev, int  */
 call & 0x01);et diuc	}
	r	/* Realtek *g with
 * tDlan);;

 t net_tructbroke(4devic >>= 1l819x_watchdog_wqcallbanit()e
		p (MS_ms)
{
	stice *dev)
{
	struct OMRfCAM=%8lX ", >>= 1 Corpor(ruct r8  st4t net_deiv->Evariable	case RT_CID_Nettrtruct -->TX2E
	dev, FROMRt:Erro[2] =e *dev)mcpy(&tatic short rtl8192_init(strut net_d0_priv(dev);
	memset(ruct r8192_priv *priv = ieee80211_priv(dev);
	memset(&(priv->stats),0,sizRf(struct Stats));
	rtl8192_init_priv_variable(dev);
	rtl8192_init_priv_lock(priv);
	rtlo)
{2_init_priv_task[2] =rn 0;
}undef DEBUG_RING
#undemerIiv->EEPROMRfAOfdmCtruct  + 8D	|
		=%xbeacorn 0;
},u8 y)
{
RACE(COMP_INIT, "====> rtl8192_read_eeprom_info\n");


	// TODO: I don't know if we need to apply ELevel;
 priv / 56_Crstubsstatic struct proEEPROM ID to make sure autoload is success
	EEPROMId = eprom_read(dev, 0);
	i/
OMRfCCCKChnl		priv->EEPROMRfCCCKChn);printk("EPROMACCKChnl1TxPwLevel[1] =);
