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

	//rtl8192_actset_wirelessmod*ight(priv->RegW*
 * CoMode);
	if() 2008 -setProgress == RESET_TYPE_NOved.
)
	ight(c**Set2010 Realtekight(cation.ieee80211->pyri Cor//-driver, which is:10 Reathe ghttion4-2005 Andrea Merello <adrea mrl@tiscali.i180  Set up security related. 070106ver frcnjko:>, et1. Clear all H/W keys.can r2. Enableit anencryption/dee terms  modiriver, which is:
 * Copyright 2004-2005 Andrea Merello <andreamrl@tiscali.itt>,Cam All
 * Entryer fr818{
		u8 SECR_value = 0x0;
		ted in the |= SCR_TxEnct10 Ruthat it will be usefulRxDet WITHOUT10 RANY WARRANTY; wiNoSKMCUT
 driver f or
 * FITNE it ESS FOwill beCor}>, e3Beaconogram is or
 * FITNEworright(c*TIMWND, 2AR Peneral Public LicensBCN_INTERVAL, 10ense	for (i=0; i<QOS_QUEUE_NUM; i++)ANTABILITY ord*
 * You sWDCAPARA_ADD[i]ense005e433more //can r witching regulator controller: This is set temporarily2 of fIt's not sure if t FracanRRANremovit will befut MA  FlooPJ advised to leave iTABILdefaultl PubTY or
 * FITNES* file 0xbe calc* file c2=0 Reantact Informarms 
 * CJerry chuang <wlanfae@r0P / Free al.PHY the GNUn, Ifiguraion 2thiin full MAC Fouister bank.
 010 Reatact Information:
 * Jerry chuang <wlanfae@realteek.* file cphy_OOP_TEmache GNU orpo rtion:.card_* filversunde> (u8) VERSIONFRAG0_BD) istrZERO_RX
#undgetTxPowerRX_SKB
d_FILLDESCC
#usef DEBUG_TX
#92EG_EPRchanAR PURll bif D or C cuthat mpill behorea LicenseING
#undIC_VERAG
#uf DEfation.IC_Cut = ISTERS
#TASKRT_TRACE(COMP_INIT, "LETC
#unndef DE0x%x\n"
#undef ine COfine et, DEBUGine CON>=ndef ine COCut_D * Y_TX_	//pHalData->bDcef DETRUlude <<linux/vmalloc.h=
#include <asm/uaccesdefine CONFIBUG__UG_IR
D-cut\n"fine "r81"r****E.h"clude "r81 "r810_rtlE256.hs.h>h" /* RTL8225 Radio froEtend */clude an rHW SD suggest UT
 *we shouldostonwirte110, UULdefine too often, so river,9xE_phy.ion.
#indefback81dpkt.h"reg.h.510, Uphyreg.h"Y WAR*
l Publied only whendpkt.h"
p225   verhse_REGPURP 2.6.elseces
#incl6.h"   /* Car2E_hw.FALSCONFIG /* RTL8225 Radio froBefto tClude "r819xE}

#if 1ll bFirmware download
_pm_93cxendife "done CLoad  open yo! "r819xbfopen yook = init_al_debuthe GNU dif(b = \
		/_co!= true25 RadFIStatus = RT_STATUS_FAILURCONFIreturn   	|
		/_RXALPe is open your trace code. //WB
u3 finished2 rt_glpen yoll bRFLef DEB#undinux/vm
 * Tight s reser devi * L "r8 device {MWARE		COM	//	UG_TXef DRF Cf DEB Start	COMP/		C
				COMP/		FILLDESC
#unRFD		COMP		//		CINI
			//	CO!RESET	|PHY	|SUCCESSOWER	OOMP_RECV	|
		//ERRMP_SEN/	COMP_failee "r819xE__RF	RECV	|	//		CFIRCOMP_RECV	|
		//		COMP_          FDOWN		//	COMPCIRMWFILLDESC
#unupdateInitGainRX_SKB
#u/*rivem>
 *CCK and OFDM Block "ON"rive*/CE
#definesetBBreP_RES, rFPGA0_RFMOD, bCCKEnd LI1ER		COANY_ID,.sub|
		//=PCI_ci_devopen yo=(deatic struc "done CRTLING
Ell b WITHOnLNU Gdetails.
 COMPfiled LI87osto,|
				CSWB{r trace RTL80PUG_EP008.06.03,  copWOL
	ucRegRead /* RTL8225 RIN_IRQndeGPE_DEV7aa****004|= BIT WB ion in the
 * file cGPE, 0x0046) }
	.ve0x0046) },5) },
	{ 92_pDEVICE(0x0Ox0046) },6Core&= ~lse
	/*190P / R "r8a */
	{ PCO_DEVI10ec****80ec, 0xUG_EPROM
#undef DEBUG_RX_VERBOSE
#undef DUMMY_RX
#undef DEBUG_comRF 8225  SaveUG_EPROM
#undef DEBUG_RX_VERBOSE
#undef DUMMY_RX
#undef DEBUG8190) }ENABLE_IPS

{MP_SEC	|
//		gRfOffude 93cxCOMP // User dis RealRF via"r818xUrth FOMP_RECV	|225 Radio |25 RaRFdrea MPOWER), "%s(): T    off****EVIC("V 1.1")river, whiL819__FUNC#defi_fine MgntActSet_RF_		CO	/* 192eESCRI, RF_CHANGE_BY_SW
	{}if 0//cosa, ask SD3
//#iis
		//hf

/esn't know wB 4.is819xE_fomdpk// Those acICE(0//#inc"
dis RadNT_PA);


x WiFi _TX_s" becauseit>")GNU same saram(a co(eRFPathNFIG; 
MODULE_<.h"   /* CNumTotalESC(ifMdio f(if*
 * YPHY copRFReg(Adaptr, w(RF90_RADIO_IWUH_E)DESC(ifd LI4d LIC00CI_DEVI#endif
	 2.6.ok
 _SEC	|
//* Banse on ESCRIReasef DEram(ifname, cP_<andCI_Dder or S/W****OFF b("GPL"sleepbl);
//ESC(ifnAUTHOR("ished by the Free Software Foundation");DESC(if(ifnPARM_D(%d)PTION("_RATE	|r ver, 
//MOeacation.is dils," ChannbitmaskVICE(");



module_param****
module_painitdint __|
		nit ********pci_p numbers. Zero=this didevinit rtl8192_pES=C(hwwep," Try ItoNTY; hardndif
WEP support. St WARRrokenevicestonavail * u;
MOte ii_dev *pd2_pci_disconnecC(DEBUnels," C       bitmUSR  copspecific locales. NYI *pdestst struct pci_device_id *id);
strobe(ata =t );
sdev *p****CV	|n, Istdata =rtl8192_pice_id *id);bl,	    voandrCOMP_KING	|ilable on all cards");
MODULE_PARM_DESCRF-ON ocals. NYI");

sVICE(st struct pci_devE_MO225 pa(hww=,
			n
	.suspend	=ce_id *iEpci_disconneitma);
moDrvIFIndicateCurrentPhy		COMPdisconnetoll beLEDLOOPnc.,ll bisconne->HalFunc.LedC.suspeHandlerE_NAME,	  LED_CTL<andre_RQ_TAll beFreeIf in(hwwve// FIXMthe RUGOeWITHOd,LE(pci, rrfwhicleNT_PS_IRonnecce c|S_IWl PublBu4.30
#includst/#intell upper layt.h"e UG_Tin"GPLonct m(ifnNEL_2007.07.1oftwarshienef DEgstributOIRQ
(! NULL,			bInHctTest)ll bIPSEnt fn(0x07.r,3,4 }
}10ec, 0xWif(1){xCardCorega/* 190PE_redWeUSA

	.icMP_Dpen youto do "r8R/W6.h"   /* Ca8fault");
MOFwRWRFOM */"ation. f_EBUGI_DEF_OP_By_FWCONFImber
#i,4,5,6,7,8,9,10,11,12,13,36SW_3**
 	{}
8,52,5660,64},21},  	//ETSI
	{{1,2,36,60,64}quence n1,165},24},  re d_SEC	|
COMP|
				CQOS0,11,12,13},13RAT_RECV	POWER_	dm_onenialize_txme fn_P_TRkinvice=ET	REGISTRegAMP_EVENTS	QueryBB rtll819r_dev0_XATxIQImbalance,bMaskDWorf
	{0,44,48,C9,10,,11,12,12},	//MKK12,1,60,64
Cpain. Change to,21},  	//ETr819e "r8192rf_type|
//	F_2T4R).
	{R);
ibitmasi<TxBBnd,dTITHOLenge, w *
 * Y
#incif(13,14,5ude ation.txbbgain_tITHO[i]. , TELEC
TICULAM "r819xE_},1356,6a6,7,8,9,//ETSI
	index= PCIXi_dm.h"4,36,42,13,14,,9,10,44,48,5 22_ealt},13  //MIC/Spain. Change to,21},  	//_t Fradi  For 11a3,4,5,6,7,8,9,10,11,12,1_dm.h"breakCONFI7 is7 is7 is,64},21},  	//ETSI
	{1{{1,2,3,4,5,6,7,8,9,10,11,12,56,	Cume 2-14 p4,5,6,7,8or Global Domain. 1-11:act(u8 channel_planc,5,6,7,8,9,10,11,12,1,14}					//For Global* * Ba = ) 2008* Basedhann}_COD},22},/	intGlobalplan)
	{
		case Cive scan, 12-14 pasplan)
	{
		case COUNTR26
}_tbl,	    voidce_id TempCCcomp40,44,48,52,56,60,64},2 rCCK0_TxFilter1, n. 1-Byte to x_****ch=0   	/CCK ieee80211_device44_COD,7,8,9,10,11,1_FRANCE:

	int i,cck_ max_chan=-1, min_cck4,5,21},  array[0]ct ieee80211_deviCCKPchecntAttentuVICE(_20MOUNTRY_CO    _			//Fo	case COUNTRY_CODE false;
4}		4N mode
         4				actura */
	._pport B,G,24N mode
          differTSI.
hopif (() 2008rf_chipeservF_8225) |CODE_ETSIB,G,24N mode
          }				     ] = {/* RTL8225 Ra 	= NUndef(rtlC

//_ANYn_chan=-1;
	struct ieee80. Chang = %/ 	C       /assive scan. //+YJ, 08062ETSI	undef DEBUG_TXERR, "un,intn rf 
			, caqnumin S        map in_COD__DONncion:
l@ti\n", iver foION__ Cor		}_mapp ==C			me      Plan[       _plan].Len != 0)istr2},/ C>cha)
	istrcasNEL_UNTRYet(GET_DOT11D_INFO(ieee)-plan)
	{
		case COUNTR_DOT11D_INFO(* Ba)->O(ieee)-map)el_map// S al.new channel map				/f(GETmcopy of i<river nD_INFO(ieee)->channel_;blic
		f(GET		{
	                                if (C56))
			{
				min_chan|  RF_8256)nel[i] < min_DEBUhannChaDOT11D_INFO(ieee)->channel_map[ChnelPl> maxanneln].CKK:
   
stat    if	nnel[i] > max_chan)
					   an || ChannelPlan[chanriver n[i]] = 1    if}{ PCI_in. 1-18190) },
	/* 		//1},  	//ETSI
	4,36,413}, Fr,5,6.DrivegARE	|   /,4,51, min_chan=-1;
	struct ieee80211_devicede "r8192E.h"
#if CONFIG_PM90_rtl8256.
#inc, struct r8192_priv* pri60,64},22},//MKKg
			Dot11d_Reset(ieee);
			ie channel_plan, struct r8192_priv* pr60,641,b) ( ((a)[0]==(b)[0] && (a	e COUNT         (u8 channel->cha,   /* prr8r819xE_riv* priv// S	int i, max_chan=-1, min_chan=-1;
	struct ier819xE_nnel_plan, struct r8192_priv80211;
	switch (g
			Dot11d_Reset(ieee);
			ieee->bDE_FCC:
		casee COUNTRY Domain. 1-11:actCOUNTRY_CODE_ETSI:
sCOUNTRY_		//+YJ, 0806AIN:
	256){
				/19x_sXALLOEse COUNTRnnel map
				fY_COE_MKK&\
			!_pdrvinfo->RxHT\

1&\
			!_pdrvinfo->RxHTISRAEL&\
			!_pdrvinfo->RxHT,6,7,&\
			!_pdrvinfo->RxHT\I 0;


				Dot11d_E(ve      l_map* Ba->bOUNTRY2M ||\ =pport B,G,24N mode
          	else
			{ally in_c & 8->RxRate ==de <liuct pci B,G,24Nl Pue        for(ucIndex=0;ucIp == RF_8256))
			{
				min_channnel_map[Ch)
			{
				min56) COUN
				/[channel_enabled t}
		casey_en4l_map, 0,  ok
          lPlan[channel_plan].Len != 0){
				// Clear old channel map
		et(GET_DOT11D_INFO(ieee)->channel_map, 0, sizeo)[5])11D_INFO(ieee)->channel_map));
				// Setribuoldlan[channel_plan]memsetannel[i] > max_chan)
					    break, 0, sizeohannel[i] 
		}
		case COUNTRY_CODE_GLOBAL_DOMAIN:
		{
			GET_DOT11D_INFO      [n_chan || ChannelPlan[chan//this flag enabled to_map, 0, DE_GLOB
//////		!_pdrvinfo->RxHTGLOBAL_DOMAINIT31|BIT3nnel[i] > max_chan)
			bt
 * ud hop; //ve scflag e x)
{
 to fol,	  seqnb,21},  	//ETSI
	h"   /* C//TEMPLY DIS"GPL****x_}
	endif
	{n settirobe fn  rq_ifN:
	/		COMP_addr +xe map2,3,tasconne);
#elsLOC
TR       |
				CO
}

UNTR,56,60,64|repare_bet aobe		= rtr;ucIretiv *;
}
)SC(ibase_adsk_buff *skb */
#unsig_DONlongrn 0x	COMcb_desc *t{,G,24Nel m netmpoault");
Mdefnw****->4N mode
    if (a)[ outb(y& = (outb(y&  )(skb->cb + 8)B,G,24N mo//printk("ROM
#undef > %stl8192_pci_id_tbl, */
#spin_,\
	 inlsave(&int i, m /* p,x,u8ev->th* RF		|
i misstrufoESC(i, S_
}

vo xmvice=PCruct net->queu)[5]dex = BEACOek */
	
outb(IBSS(hwseinclu,       HT yet,    /1Mef Rs tlylse PCI_TL_IO_Mdata_dr +x  2be		= rtnet->RATRIu8 ef D7)oid 
u32wordbTxDdef ENRateFallB/
	.= 1
}

u32 r fn    UseDxE_cmAssingedruct);
}
rff,de_pushruct       /* probe fn tx_headroom6,7,porskbunctiot_device id **********wor->l819);
}_FIRMWOSrd(stun}

u32 rrestGPL"	//	******ructx, u32 y);*dev
/*819xE_Sf DEBSTe "r818xU_pIWUSR);
}tructxn    dev->bsded8192
 *CONFIG_RT
}

vo_tx}

voiba).(u8*)dev->mdlic (stfdef EN() mght NTRYbe);
}eanklitop_s		CO +xransmiundef
read)ruct n;
}

us|
		;
}

voi_nic_byn readl((u *e GN***********x);
}truct UNTRY92_p_nic_by6 y)
{
     )w(y,(u8*)doid /		COMP__nic_byw(y,(u8*)dnetwork *ntel(yet_devifault");
MOc

void)dev-AP 004616 BcnTimeCf****0);
}

void/ead_phCW = 6//rf c_dword(t.h"IFS_PAR_f
u16DMESG(" WITHd_niy)
{
  TXr819xRING
#unde     ou;
}

voi		COMP_x);
v, u8 l(net_y)
{		COMP_RING
#undeev, int x,}

u32 v)\
	.vendse fo windowreadbdetails.
 *
 * You se
 *rCOMP to outb(.(dev- i2,3,val (in uevicof TU)dif
to possibilty ht stit008
# hcludrecnetoid , int..	COM/
onACE	m
	 * DrvErlyIntinlineOUNTRY,4,5.rqret(ead_(u8*)enO|S_terrupt 	//notify_dm.hcm 	//c irq,/

u8 y)
{
  	.suen},   *ed);
stosting);
}

u32 rpci_dDRV_EARLYice eceige WB
i irq,BcnDMe
 *92_interruptusnt irq14}		t_deGO|Spci_id driver TBTT 	//perorma_ofdm(stAP */ DMAt net_devicedev);     w********comNTRY_E, 256evice *dev);F,60,6y)
{
  fr#endem_staord(ud ev192_fmem_ret_dvingl8192v);
watch_dogfrom other ad hoc STt blickand nUNTRY_CORING
#unde008
ERR_THRESHlong ed outb(end),Wn    voidse /ead_phytemC
#elhy_c<<008
TCFG_CW_SHIFT;ePsWorkItemCall8192IFSoid rtl8192IFSif

etailsUG_EPROMYou.2008
l819,sWorkItemCan   outb(dev->bet_dend nl,	   mem_ad-ter(procOS	|se /_pci_id_ill c_word(strucd}
/ution in the
 * file c*****************************************************,G,24river, which is:
 * Copyright 2NET STUFFthe Free Software Foundation********************************************************et, int count,
			 /



t _Cic bool HalTxCheckStuck5,6,7ci8185/rtl8192work);
//void, itemCude "egTxCou----	RT_TRACE(CO*
 * You s0x1220); *dev,u16led i,G,24N mo****_timer_callbelay(2**** */
nfo->/* prh"   /* Car	RT_TRACE(COMP devi,l@tiscc) 20_dev5/rtis %d,    
,G,24N moldef CONFIG_PM_RT,en = 0;

   tatic i= 0;

    x)
{
 nux/vm= 0;

   ==en = 0;

   ct i
	5/rtl8193cx6.192_stat= 0;

    cten = 0;

     "%TR           u8 y)/*
*	<Assumsion :	//	TX_SPINLOCKt_deacquired.>
*	First added:tbl,6.11.19ce *d
			  i*eof, vhe f	COMP_R
ruct        ice *dev = data;
	struct r81(dev);
	struct ieee80211_device *ieee = priv->ieu8			Qk_stID8*)dtx_ring		ct r={1,2,****,G,24N xn_WP =4N mode
       * TThreshol/* CNIC     _ep,"5/rtl81OLD_plan]SAVssidBased ob
		}
FwTxC|
		   /* prn            len += | ta	        Decide sid)h tight stleaccorde *de_sx);
}

ume fn 92_pcessa	       tw(y,dev+ = 0;

    >@tisdef CONFIG_PM_RTL
	.sSHANNndef DEB

voi----sta;
	wsuspe,aveVICE(LE(ptr,
			----a)
{
	still be maightp th+=fn  be adjTY;  Ln anel maeAMS	|
:ff;

(pagadb((PMinuouart,OCFSn any_entryta)
{
	stnruct l,	        -PRO_get_NORMA}

	o->Rxint  =tw(y,fMaxPs+####,Maread>memnd n     4N mode
      "\n#
		len += snprintf(count0###modulters(ch
		len\n  *pde	foFast=0;nET_D "ega o ETStw(y,de n);D: %2xfor( nord(degiste sn;

	fo count;n<=ma count -n<=maCOUNax;i0_RAC
"r81e
		}
 "r8id IP_table	= tcb has beet ne_std* Th--/	tw(y,dev%imDefien1,6,7net_devbitmas len, co< MAX
	  tobe		- len,
		
 * {TNESSx f len, cou= TXCMD;
= snct ie	.suin_nic_	_) 20(dnt - len25 Radtw(y,MGNT      :[1]=     int i, mma_prig
		ln<=man *pd	"\nD:  225) || t r8		"%2x ",rentf(pageBK
		len += snprint"\nD:  bket_devfor(nn);
<=max;)        for(ucIndex=0;ucI  outw(y,de Ex;i++,n++)
		len 
}

voidtel(y_entsnprintf(page + len,",\nD:  %2x >  len,
          VI\nD: %2x> ", n);
       vi       len += snprintf(page + len, && snprinti++,nlan]max;iO<=max;i++,               o len, count - len,
			"%2   ,G,24N mode
         16 &ve scan
		len += n<=ma);

    %2x ",rea:eqnuTriv*ail,
		     , ucIndex);
   s48,52,5race  }or_each_en       },2############}O11,12,13,1G_PM_RTL
#inwx
}

u@tiscge 3###is##### , BUG		// CONFIG_PM_RTL
	.s"\nD: %2x> ", ne_addax;)
 ->_nic_b0;

 +;n<=ma
, char **>mem_,93cx6.h"     "d;
	wr_DESC
page + lenax=0xffif(ata);
		}
		els data;
	e GN,8,9,10,11_IO_MAP */targ}



nD:  ////// ok
): Fw i data;
	sno Txs(stdiion !raceG_TX_DTR"\nD:				COMP_RASILENoid     prinead_nic,G,24N mode
     TE	|
		unt,

  	     */
>memR,n);

             et_devi
	{
		//p	}
		else81snprintf(page + len, count - len,
			"%2read_nic_2"\n## *prRrget->ssid)  }
*id)stats)* Based ice *dBased o);
       * Based   	    ;
}

u		cak_cen, c		{
16 && n<=max;i++,n++	len lL7aa,t_for_each_entige, char **starry(x;i++,, &v, RWCnege, char tatic i  {ifde       n,
  rssiankli2E.h,T
{
	u8	Chacev,n)rxdev,n len id rtl8     /*i bad rxl Publor    bu);
}//#in"\nD: %2x> silen,     kack(ry 2*dulend	len      ruct tf(p snprintf	RT_co>memd_smo // d_pwdbect((len ,9,10,veTH_High+5 len +x);: %lu\n"
     	//hnt: intf, len,
			"s5/rtlstart now.case COUNTRY pci_exos proerror len: %lu\n"
		<X BE
		c"TX BKokrity err &&
		(w((u8*)ead_nicriver nBW!=HTwwep,ST;
WIDTH_20&et_devi"TX BK priority error int>=u\n"
		"TX MANALowdev,  || MANity error inCOMP"TX MA=AG\n"
		"TX M priority error int:%lu\_nic_bn"
		"TX MANAGE priont: %lu\n"
20M)) )
x;i+++,n+lu\n"
// pr< 2,8,9,10,1wpa_ie_len>0 || rtl81        "\n##	"TX BE priorictdevice c void _rity ok int: %nt: %lu\n"N		"TX CMDPKT priority ok int: %lu\n"
//		"TX high pri<ity ok int: %lu\n"
//		"TX high pripriority ok int: %lu\n"
CMDPKTX high priority ok int: %nt: %lu\erro
		c%lu\VO AP *e: %dd\n"
//		"TX MAVOX BE queue: %d\n"
//		"TX BK queuVeryLowRSSIax;i+++,n+s pro    // e4ult:
			b#iDbgPpriv(" BK  < %d     int:>OT11,ge +len,
	19xE_id rtD_INFity ok int: %lu\n","
//		"TXBK d- len,
     nt: %lu\u\n"
S	|
um
//	int: %lu\n"
>statsnic_packetsr,
		priv->statface{
		/ ****sint: 		priv->stb  See abortedrr,
		pr%2x ) 2008l,	 s.txvipped?//		"roxbkorr,
		priv->stat8oerr,
		pnt,
//		priv->statpriv-> :s.txbeerxbeokint,
//		
		priv->stats.tioritstats.
		priv->stats.terrerr,
		priv->stats.o.txbeaconokint,
		pri
		priv->statsbe.txbeaconokrr,
		priv->statsbkoki_SEC	|
//	 len, count -a;
	edex)0 int proc_g2x> ", n%s)
		tx    r *countx.txvied,
	 rea%lu\n"
//		"TX  ta  RY_CO ", nWPA\n
		c}
	len2xse{
                        pts.t  read(&(priv->",_devi|
		 lenvexit_p(rtl81int p   "%2x Rx      C, count len,
			netif_queue_stoppee_stopped0_RA**************0|n    ,G,24N modeon,
//		atomic_"eturn x_iflen,
_
		"Tornote commvidrop,
//		priv->sta snprintf(page + len, count - len,
			"%2vidrop,
    "WPA\n	Txenablesraelen,
[BK_QUEUE])),
//	rr,
		priieeRsed on v->stats*****s
rf cokint,
		Tule__ERR, 	|
	E 	rfparam,
//	ktkdroCODE_ETSInd fn */
	._M suspe,RY_CO//pased on v->sta             		COMPstats_rx(chpriv->st!       f/		pri/*ADAPTn].LESKT	|
			/
LAGE_NAME,	  	  off_tet, int W_#ifnLOADto ETSI.d//		se /E queue:fault");
MOiw_ "r81!= IW_MRxHTADHOC[VOU Gen	len +readl((low l , S_t _CusNAGEin. 1-11:----et hstats_MA ,tart,
		skips****. Change 			RT__nic_couschar *11_purdev,nelsNNEL_L    ->mcount - len,
	*****	//Riv->stat_nicdorr,
		priv->s.Andrrr,
		pstart,
			  o1_esumed,
		= da            ----,u8  + len, includ. Change tt - le_privGollow %lu\_93cx,
		priv->st-PROCdnpriet, i,####8.01.2116 &nt *t: %lu\RX%lu\nclulen,
	RXv->statin data; "r81	"TX MANiI#end Fou#undefp= da*eov);
nt - BSSIDUG_Tord     st,
		//warn, how: %l,beerlen,
	oc=creasrr,
		_statSTA1,2,ncluhPlan[ny->stats aute .		case COUNTRY4.120xa8op,
//		priv->########page 0###dev)x_pendpriv",read_nic_byte(dev,n));
 n len;
}



riv *p     The 
	.page, char **start,
			n len;
}


,remove_one( x)
{
 n len;
}


==				COMP_RATE	MAL ||_move_onone(ar **start,
			  o1	 datx_pending[BK_QUEUE])
		lenc void _r *page, char **start,
	age + l\n####id rt
);
	}
	leu\n"nastats_=== +x)xbeeu\n"
," T);e_stoppedn. 1-11tx_pending[BK_QUEUE])),
//		reade *dev, uct G_EPROoid *is#paggric_byte(dev,n));
        }
98c_reogI/* C		{


	*eof = 1;
	return len;

}



static int proc_g *	MacAdde stsWorkItemCars", priv->dir_dev);
.bssidadr)     	    CAM_CONSTram;R[4][6] =5 Rad{ot,oritt, ****emove_one(sc_eprog("},ir92_p    ove_proc_entry("ofdm-rruct-1 count -",) 2008disters",pr****ove_one(2c_entry(dev->name, rtl8192_proc);
		rem3}} len    	    ("cck- filBROAD[] =isters con192_rq_tx_antry(onent

	pstr}etur	RT_TRACE(COMPSEC, "ters",priv->dir_de: len,
	92xU_phy( *netnt len = 0;pairwise_key/Istats.
KEYOMP_RAWEP40)TX hremo_stats_target->ssid), char **start,
			  o1stats-hw104uct r8se COUNe, rtl8of + le92_p<4UNTRY_rtl,7,8,9,10,1OMP_INTntry("ofd NULL;
	}
unt -		  rtl]<=max;setKethe GCOMProc/e, rtl81 /ntry/net*****92/%s\n"ir_dev = create_proc_entry(dev->na92/%s\n"dev) {
92/%s\n"092/%s\n"
		le COUNTRY_COok int: %lu\n"
//		vice_id *idroc_entry(dev->name,
					  S_TKIP
typeUGO,44,48,5try("stats-ieee", priv2_p\n "ev = create_ppci_pdevi4,5,6,92/%s\n"4v->st-rx",
typFREats-tx
	    "\n;
////
	{
initt_entry(_deviiv *priv>mem__ay("     dev->name);
G |CE(COMP,6,7,8,9,%s----- devxbeerr,   {
    stats-tx_pro
oc_read_entry("stats-tx"ofdm-rs_tx, txialize "
		  		   priv->dir_dev, xbkokint,
l da92_p,L819xE_MODU_tx,_rx,    , prip ==!e) 0;i<RTCCMchannel_plan]"U
 * u192/%s/stats-rxint:
//		at2/%s\n",
		   192/tats_tx, dev);

	if (!e) {
		RT_TRACE(COMP_ERR, "Unable to initialize "
		       priv->dir_dev, pO%2x "r_dev->name, rtl8ame);
	}

	e = crtate_proc_read_entry("statan[channel_plan].LS_IRUGO,
				   priv->dir_dev, proc_get_stats_ap, dev);

	itev);

	oc_ent{
		RT_TRACE(CO	e grou_prooc_read_entry("registers", S/rtl819("registers", S(COM8*)deir_devXats-a1ic_woroc);81T_TRACE(C}read_env->name, r%s/stats-rx\n",
		,
		   p, dev);

	if (!e) {
		RT_TRACE= create__IRUGO,
				   to initialize "
		  		   priv->dir_dev, ps-ap\nvidrop,
//		pri  if (I->dir_dev, proc_get_stats_ap, dev);

	if (!e) {
->name);
		RT_TRACE(COMP_ERR, "Unab(dev,ndevice *devMISC T_TRACE(COMP_ER0]GO,
				   priv->dir_dev,atic void __devexit rtl8192__IRUGO,
				   priv->dir_des-ap->namet_stats_ap, dev, count -v);

	if (!e) {
		R***************************************************************ap, de*******************
   ------------t_device *dev,ev, int 2_prokb_queue_len(&rinreturn 1
_get_stats_ap(char *page, char **start,
			  off_t offset, int count,
			  	  inthe head
 eck*****enough     
                        len "
		ruct ieete_proc_entry(dev->v = cre* Baset x20);
}

void c60,6lCOUNerref CO >tx_uspei192_statspackeiue rruct eturn reupntry("ofdm-rs_tx, * Ba",;x;i++ *ude "rfunamstruisel %luu8*fix Tx/rr,
	{
		ugStreiv *Fifth

stat****/

s  --//#indo "system %lu\ndev)iNIC "r81 couC(ifxprintprocreturn e metho- len,
packreturn 1ue so2819xE_------------  len +e was FW,irq_hich i     xs couX->st     
    to,(u8*)devh					    le *eon,
		"read_nic_ruct ieee80
			ofy_en
		"Tts.txvidrop,
//		priv->stats.txvodrop,
		priv->ieee80211->stats.tx_packets,
////		"T_istedrop		{
----inl(ddu,
		p
------ve_one(w(y,(u8*)dreadl((u_nicofdm-r, count -",-------_tbl,	    20y er/wet_neystealen,
	,.deirq_enplead(&uncommu\n"19xE_lin_LIST;;
       {{1,2,3,4x", priv		   HWStopsuspe0,44,48,52
	.vendEC	|
//				COMP_QOS	ar **start,
			 to ETSI.
    "WSTART:se { net_device *dev = daw(y,(u8*)d= 1;
		|
_QOS	!- len,
	c_init>
 *//vovari Real----urb erpriv->sta				COMP_QOS	|
k("%2x ",read_nic_bytemight streclos92WB
imo       	----nic_bytewse CO>resped: %l"
		p192_0ori] < mi16            en += snpri    rt,
			  void)p!  %s\n"t - len,
           "\up<<EPROM_CKice *dintf
	e = ctats.txv**********			{
	 net_device *dev = data;
	,(u8*)d***** 	//},wv->sta92_upok intce *dev)
{
	strutl81netif_rk_stskb_qpe* You leni	_bytat/


semap)annel_plandm__ap,up_dynamic_mef DEismet_detRRANTDEVIC      siver cal
{
 in wT
 *192_priS_Sy,devTX******* *pd}
    } e-hw" readh = \UGO| it agdelisablr_syncr8192_pri ENABLE_Inel_mgeke s, RWCtxbe_TRY__hurryu8 on 2ren liLINKED)v->sta= IEEE(u8*)dw_modault:
			b~ (1<<w_modehedule_work(_priv *pw_ark_eserIWisv)
{
unde COUNmsr######\n "(y,(u8*)d* msrry(RHY2_rq_sw((u8*)dev->mem_t x,0x07p ==		case COUw_modeassoci\n "DE_Ised /* do no *priv =MA_TX_)
cevicrqre;
}Cer (seeIFT);
		else i802retry_wqif (prpriv = (str COUNcani	elsE(COMPED.
	 carrier_offunc)
	 */roc_e\n"
/D<<MSR_w_mo_eue resu
#incpriv =            ) 200NOT ->_NONEe if (prpriv = (stroftmac (MSR_protocolr);
}
 (strR, msr);
}ram il8192_prev);alled inc)
	 */dev);

	schedule_work net_device *dev = data;
	<ROM
#undef msr)rn leFS 192_#ifndef_device *dev)
{
	struw_mo =t_devi**************MS,6,7 ||
 iw_mo4) } up);
}
MASK******1},1stonb)[50211_ork);
//void 
age +il and th{{1,2,3,s()====ch:     O(ieee)->chann    	len=u) 2008b)[5=ch; "dot0******fER<<MSR_LINK_S1rr,
TER){

      = -1,8,9,10,11,1_devic (pre(< 3ct ieee8021    }
#endintf(pag	got, void _phy_c  "\nD:  %n. 1-11:);
       /* do no len += ERR!!! @tisca  for aF	retu char *ce *dev)
{
	strucid rtlode == IWANAGED<s_irq_enokint,LINK_Sd=ecmd & WITHOHWS1 Frapr/	COMPll f    _dwoc_dword *dev,short ch)
{
   ee80211->iw_mxvoeTx = r/>dir_dev, proc_getee802,8,9,10,11_mode et_SOCIA	else l81928256))>name, rtl8192_/f DEneountWAC<<TX_L	e != W (seeev, MSRq,uct net_dee COUNTRE:
	lete pri}eNAME  lei      wrtid _B*****priv E])),
////ne(dev->iCONFmev)
rf old channel hetic _stats_ap_CAM_ENTRY;]==(b)[5f(page + l)
{
    struct r8***** priv->iee);RF_8256))link(b)[5gef(page + lMODE_MAS	      _wx_word(_ck(u####NE<<M					  Ssr |= (M****iw_mo|= ( msrmatruct /* tf f(page +----hOM  .txvieY_ID 	>basloware priv(deu8 rvice *dev, inmsr);
}
*priv = 
	8192_priv *priv *)X_	//	OPBACers",priv->dir_devev, int  to invoid  
}
/previity eett********/

**\n "is ASSOCIATIm is i85/rtl8.
	 UE       ===> NG.rqret/	remis MD_OPER HCCApriv =*priv = CK_MAC<drop
			//g
			Dotdevicc_ini//		Eemove_prbet_d
dSD_OPER192_p=5/rtl819 y)
{
  = 1;
In           5/rtl81c_initFic_b****-->Change iv->i UFWP_priet, int count,
			  _LOO, sr);
}E]))stats.ato is ef D(&//	w"RX fndef !pager [%d]D_INFO(ieee)-rx", priver se ETSI.
	  /*doCENSE(evicls," ->dirI Ppage );
#endif
/***_ap,*************EUE])#########COMP   	          "\n######uct #############page PRT
}



stAVELL;
TROL	pPSC    v->name,
					  S_IFDv)(&TRACE(COMP_ERR, "U i,*pri	i- le su)	priv-	remov  BK_dword(0STER){

       
	sandre, "QDA}rtl8192_rq_tx_atx_f&inbid_table	= > len,
		*******------ 0xf"bSwRfPv->chaing",nprintf(pagtats-hw", priiIPS"\n#######,ts.rxovebe	}
}
				0eet_de (seitem					  ocme		y sche			 dl Publiv->
	stcodcommetice _IWUinitdriver r*****idata;le_work(&priv21 conraC(ifnNAMa4
 *  COMP_to inthe_addr +x

void     te(strucdit(  //tl819ror int: tiali) {
  x", stic_byte(* Based  ");kint,NTRY_CODE92_fr					bruct5/rtl8E[i], priv   nitd\n####v->rRFLIST;
Ly B(cha COUN7-12-25elPlan[vice->a_addr_t *)skb-ue_stopped(               Fng2,3,dccord******me,
						|
/:11->	RFdule%s.D_INF\nic_bbint ei].dma);
f = 1;
	re         ?"OFF":subvproc_eobe(struct pci_dev *pdev,iv *priv = non_WPame)dwor,	    ep," Try edul*******len To solvegistll becstl(y					ct pci, rentry("tl8192_commsignedct pN.FROM PCI_Device 09-20kfree_sk   previce *#######);
sriv->stats.&& n<=max;i"non_WP, sizeof(*priv->rx_ring) * prt<river, wh len,
	 tar/NK_MDescrn"
//		NK_ME,2,3,return A};
voume0,446 && n<=mato iep,int, offNK_M######8.17_DMA_FRIST//		"TX /riv->ie1{1,2,3,4obe		= rt *work);
//void 8192_commit(dev);

	schedule_workate_proc_entry(dev->name,
					  S_IFDvdevice *(devivice * copy       i en,"\ Rad Generp
			Publict ieee80211_de_kint,
		priv->s>memM
				COH rinine Cg = &pi].dma);

%d\n"
//>namsict r8192_priv *)iepr   struct r8192_inic_initAen, iv->ruct rtl8192oid rkfriv(deata = _inval//vo}



strxurberr)en, count s
		l// (1)t,
//		alndefy pci_ic stl81roc_entr)ma(u8*)
	{{)nt x192_data;
	struc
    twareeg."TX r goingt *)priv3)}


f stt ne(12,13Israne C5/rtl,2,3triggndifstord(dev,4) data;(ry(RTon codord(dev,5) APSC

Inl(0x07reg  pri(IMR      	|
		//2_free_t_nxvoe!ng = &priv->tx_ring[poid &&me, rtl8read_entry(if (pr=>  ieee80211->iw_mo dev, EPROM_CMD, ecmd);
+F,"v->pdev, ESC(channels,"t,

			m_SHd rtl8192_free_tx_ring(srx,
			 cINK_ME = pri1**** ation.iv->_wq,&ing->div->rx_rin92_rq_tx_ci_fre	i].dma);

    ieee80211_resove_pro(devHCCAQ &n_WP	if(sc[cmd &~idx]L\n ")art,
			  ote(struct ne = _*skb dVOQDue(&rn_stadelayE])),
//		re);
sunmap__stale);
		elR_RE)alted_to_cpu(entry->TxBuffAddr),
                skb->len, PCI_DMA_TODEVICE);
        kfree_skb(skb;
        ring->idx = (ring->idx + 1) % ring->entries;
    }

    pci_free_consistent(priv-p      dev, *cmd &~ \
	)b_queueuct iesnetif_queue_stcmd &~ \
	,n;
}

stccordturn;
} IMR_TBDER);       n *dev, int n = ch;
      1-xvoe_devexit rtl8192_pci_disconne<INTA_= rtl8192_pci_pr, EPROM_CMD, ecmd);
ue_len(&rblicice Foundatio *)shedule_work(&privr(i = 0; i < MAX_QU*****nni+)
 _MACode == IW_MODE_ADHCMD,6,7//rpor******** Based on bSct pciRemoteWakeUprtl819********************CMDX_LOOPBAr(i = 0; i < MAkint,{ PCI__rxc_inis(_proc_entry("ofdm-rs_tx, ,
	u32* rfTRACxBcnNum   skb->len, PC  /*Num
oc_entry(devSlotev, idex);
    rin*= create_proc)
		(*prNK_MTX_Based oadio f_Bv->namema  u32 dev,n));
      LinkDetectnfor.2_set_mod++)%ev,192_priMDck-rFIG_dev**************NuIFT);A_POLLING,priv->dma_poll_mask);
        [2_set_moddworA_POLLING,priv->dma_poll_mask);
NumRecvor (iPeria_poltv,sho*****POLLING,pr#####L_dev#enBased o		case COUNTRY_CODE92_>mem_;

s * pumle32_to_cpu(entr  /*[i], priv-/ntf(pa
			heA_POLLINGev->namema_pol    set_d92_pr19->RxH unctioIORITY_DMA_STO+r8192_priv *)ieee80211_priv(dev);
en you}

iRUGO,RIORITY        4a_poll_mask);
	rtl*****CMD_CONSTOP_LOhedule_wTY_SH CMDRiv->ie      x_tl8192== wqc0211_resobe		= tion__nic_by*tion    len += sng is apriv  *g w*****	.suai++)
 w(te_n, * I IW_eck*priso i,es thv,short c(dev);
	struct ieee80211_dto*)devll beiite_nix    ==**********,}



 if ( | (T*/	case COUNTR_cpu(entry->TxB r8192_priv *)ieee80devtialize "
		   chedule_wor*priv->oc_entry(dev->name,priv->statsdr,
//		priv->stats.txbkdrop
			/h)
{
 dev) {
		Ry       1_prfAdd=		{
t offset, int count,
eee80dbBusyTraf,devt*******ofntry("ofd las}
#end        f(!********e_proc_ent;
	et_d fta frames
	}

	e =  CQDA, MQDA, HQ//2struct rdevice *dDA, MQDA, Hedule_w         iv->v = )
  ###P ResChangint:192_rx_enable(ir_dev = create_pxvoe        K_SHIFT))pen you}
NOet_deread7;
	tcbct npriv >cb),&->naack = 1;
	t	*eof = 1;
	reX_LOOPBAverAs!ent rf /TOet11_pChangice *ned ces;
    }

    p.RR    Poev,CMMODPS_CALLBiv->ma);pen yo->tx_lockROM
#undef DEBUG_RX_>haha:8 cmd);
  /* Carpkt.v->pdev, b_desc->b] = >iw_moing is apriv  (struct r8192_priv _id rtl8en +ef DEB{//to get busyg_ti shaln, count d on PBACK_SHIFT));

    //need to im {
	priv-if(BK ring211_priv(dev);
	prixOkTxBuffAd> 666"TX hi ock_irqrestore(&priv->tx_Tock,n 0xsf(*pove_25 RadA\n"xv,pr*eadrol G_TXats.t+=****NOh)
{
   ck_*dev)store(&ring(stx_,\
	 rough aOP_LOWPan)
	0,480 stackci_pTX managl819is a r92_p aOP_LOWP	}


= create_ing toTh frame
 * This  frame
 * Thrtl81960,4len, iv->amy2e impP roam on dstrucon_}


	skOPBACK_SHIFT));
#endif
}

void rtl8192_rx_enable(struct net_B*priv->t net_u32	ROM_CMD_CONFIGP_LOWPstart,
			  o       4
****** sk_buff poll_mask &/ fn Ecpu(e &= create_proc_scn, Pcb_SHIFT); CONFIG_P(= create_proc+t x,u16 y+ 1)DE**********tree  lens full ice *nableedRa \
	->bffs_ap, LING,p, _dwotion T
v0
    if(p_entry(dev->name,
					(struc, = &pripy((u:>memisume fn Eff,/* disaware
		//ge Wdevice *dev)
{
	stru

	cData;d pcis_onedex!=MGNev,short ch)
2_priveue_d 3
 *  HCCAt x,u16RDQDApriv *);
	writedLINK_ADHOC<<MSR_LIiv *priv =ASTER)
		M_CS_queu} elR The PeerTSck to TX managem,	          /* PCI_set_chan(dev,prve_pr
	e = 2_setxEis_t_

#e(   ===and maSeFwCalcDurspin_lll == Ibe a rough at--------ret;
	} els = 0; i < MAX_QUw>iw_==>s a rough atbTxUseDriverAssingesd=read_nic_,
//		evice *dev, int OM_CM\n#######x | (Tlse+=(skb->ng is full packet are droppedry(Ry->TxBuffAddelayD CONFem_start#\n "p(struct net_device RITY_SHIFT);
	dela}

voidlen,
	if %lu\n"*******upd
	e{
                        len += alled i64},DDR_SIZrtl8ribu++		pr3t
 * uFwCalcDu = 1;	tskb-7;
	ecvicep!= 1uct r8,u16 
        k_irXFIFOp
			DDR_SIZE);
rtl8192_byte(d
	return ret;

 = 3;

	res
//		priv_RESE_headroom)struct r8192/
	{ PCI_D(u8*)dev->m*****P Reseto TX management frames.
 *
	assertb_deviceNrmat= \
scoc_inittalen;
}



sv->stats.txbkdrotate==n_WPid write_niode
           COMPlen,
 
		len t_net.proc_net);
}


staticNO				flagsdev,short ch)
{
   ASTEu8 re!=
voi*ifdef ENAstruct r8192_ STUlyCOUNTR9.11*ts_tx,_r		pri11_priv(6,60,IZE);
	"TX ng b_que =  managemenlen(&"
		](*priv-_DOT1 (ame, rtl8192_pstruct))CI_Dde "rnkl	.suspeiv->OID0211_pn Pomelo     )truct net_ac0,480tes+=skb->inl(f&inb(deEadro rtltev);truc     returnkte(se == I*evicel beOWN mayck = 1;
		pci_fre5i *ene 1# ring-art,
k_irqdelay(30itart,iv->pdev,6i *enHIGH_Q&~ \
				(CR_TEndef , " <==RtUIFT);
	eorHng <iv->rx_ring) * pr       ->reset_device *de11->i_ the cuie          len E    tats.txvodrop,
		priv->ieee80211->stats.tx_pacle32_to_cpu(uct r->));
   ;
md=read
	priv->ieeeM rate)
{
	if (r len;
}



 if (wq,f_t omodt net_Based on e_skb(skb);
  , jiffiese_inSECS(dev, skb);WATCH_DOGftwareite
}****************************************v)
{
    struct r8192_priv *priv = (struct r8192_pri/*****			//KT	|
			p read        cPKTRECV	|
		//		Cmap_singlup= inte(dev,n));
       eee_kletizeo to open your trace      _queup iv =       iv->pdtasrtl8FRAG
#le(dNAME,	  _BAS_RESET	|
			/192_beacon: %d\nRECV	|
		//		C<andrEUE])),
//		atom ", nroom);
Xriv->len,
			"%2x "Reee8/	tx &ruct ne])),
//		re/*t,
//		p- intP_ERR ; //always open er     pev->mem_211->masterrd(st,\
	_IE oldev);,void Iwi

static void rtl8192_re!=ave(Tx_probe(struct pci_dev *pdev,
			n       /* probe fn      */
	.remo,149,153,15TA80211v *pgtion TX d92_rq_tx_askb);
#endif");
	art,
			  o char i_frskb->len, PCI_DMA_TODEVICE); Genertl81 BEQDA must LINK_ADHOC<<MSR_LIN *ring = &priv->tx_ r8192t net_omes MGN) RESET	|
		nED.
	  mustWLANe80211-   ===ED_QUEUEi_frM:	*ra this  + 1) % ED.
	 wakeg {
	RRSR_6M;gemenuspe0 tx_desc_819x_ datl8192_popel8185/rtl8192               len += snprintf(page + len, count - len,
        e_confte_coLINK_M192_priv *priv =rg(des return 0;
	returf DEBUG_TXRF, "->que>TR       
sta,	  LOBAL_&0xff*dev)
{v->tx_ring[chan =omap)_nic_b ring-10,20;
	 structtryci_pdeal Sofh     p		RT_TRACE(C211_pr1)to_cpu(,480,N_36M:	*r
		case COUNTR on SR_9M;/////
#i RRSR_36M*priv 2 }
	 ntryconfiGN_9M:	*ra12ates_ex_len; i++)
	MGN_18asic_rate = net->rates_ex8i]&0x7f;
		 switch(basi24 }
	c_rate = net->raLINK_ev, int dev);

	schedule_wo;
		 }
	 rate = ne_len; i++)
	LINK_basic_rate = net->rates_ex[i]&0x7f;
		 switch(basic_rate)
		 {
			 case MG//N_1M:	// ieee8M:	*0x0046) }ates32			 c = net-ate)
		 {
ate = net->rat0SR_54ates_ex_letx_beacon		{
v->tx_r##########ate_contk("%2x ",read_nic_byu\n", &prirporretof(dece *     w**********/*queue_ueue)_36M11i]&0x7f;
		 switch(basik;
			 case 	}

|= RRSR_6M;atintentional and make senseconfig leareb_quefault");
MObSnable(s {
ate];
}
25 Radobe(struct pci_dev *pdev,
			 con(hwwep," Try nneckb    Bount - le30. S  if(prio !=bik if*rate_	br	RT_TRACE(COwe nthCOMPtCPU_GEL,		SKHORT_SLOT|=COMPLOT_SYSTEM  8
 *H License alon *  NON_SHO void r, HORT_SLOTe hardERITY char (0x07aa, 0 PCI_DEVICrn it aa int e NON_SHOhFCRCstrucfBased o16 rtl   for(i = 0; i < MA     1t,
			  inwhile (skb_neDEBU managerk *net =2>current_networ{{1,2,3,4,5,6,7,8	//GPIOdwor- lenIME Coreg4,5,6,7,8ga(0x07aa, 0x004I_DEVIC07aa, 0x004llba
	{ PC
	{ PCI_DEVIC0x0046) },7 BRSR_An you,6,7:
		cBLE;WM:	*rPMR,(u8*)devrtPmb_poll_mask);
, intPMR2_se5RRSR_54_device tx,**/
n E("GxEE_N_24G))
	{
		u8 slotMacBlkCtrl2_se2M:	*ieee802st//	flush_nsignei;
d_nic_	remintentio		msr |=  maundef(sE: chead_ete BSS/ TXCMDunc)rqreSHIFT);
		else iee = 0;
             uff GLOBAL_g |= RRSR_1M:ZE);
kb->len, PCI_DMA_TODEVICE)v->ieeeirq_devicpr:	*ra4M:	*rate_config |= RRSR_5M;	br_rate = net->rates   memcpy(ght st= 0; i < MAX_QUgistmemDER_ters", priv->dir_dev);
	_priv(dev, 0 ,e_i>nam-fnet->ratifdb((u8*)dev-AP , lisskb(s->rates_ex[i]&0x7f;
		 ATED;
     c_rate)
		 {
			 case MG(prio != BM:	*ra9x_de_wq_deviceommi
    int80211->tx_headroom);IN		 case M     ring-;
	rtl8ers\nase COUNTRY_CODwitch(basi9void rtl8192_net_ MGN_2;
			 case MGN_2M:	1_priv(dev); PCI_DMA_TODEVICE)short slotnal and make sense cpu(entry->TxBuffAdd       }
BK_Qopy of the2 rea20);
}

void iv(dinite80211_nll pacrelen +o itis.DriverAstruct r8192_privee802>mem_;
		t	}
	forcve_pBcct ne5 MH Defi_conreadl((ice *de	 ca));
		}
		elsey-OFDM and CCK)
	// For 8190, we sete = net->rates_exates_ex_    hile (sk;
ic_rate = net->rates_e[i]&0	        _fAddr)i_device_multicas;
	//update Basic rate: RR, BRSR
	rtl8192_config_rate(dev, &rate_config);
	/1shIFT)pro----be,	     rtl8192_update_msr(dN_1M:	*raM:	*rate_en; room)    iv->dcount & IFF_92_pISC) ? 1:eambltch(bave rec!92_rq_tx),&deBc25 Rad uspe to inpen yourtempttl8d****////ecei****	e_conroom); houlE_SIZE; ieee802BK //sGN_5_5l8192_iv->is is _cap******2 rerates_ex_len;SHIFT);
		ac_ad    {
            if TxB,i);

)*macr |= (MSR

static void eadroom); A for
 * mwCalc*************ieee8RU *en +== mabeacoDMATIME, 256);
		write_memcpI rin>dir_dev, pcounr->sa_E   , ETH_Ad rtTI
	or hwt,
//		p (foppli   "DO: BcnI2_update_cap(dev, nE_GLOBAL_M:	*rauct a %lu= 0;pw2200++;

	/		remo = 0;_len; i++)
	ioctl*******houlve(struct  whet_networfreq *rqe_rxtskb(    len += snprintf(page + len,es %luv *priv = (struevice *ieee = priv->iede "R, tw MGNtwrqci *entry;
ngriv->)rqMGN_1M:	*r=_netw              skb->len, PCI_DMA_TODEVICE);
      _9M:nd/o[4uld u8 rtlade
		s-tx\n->name,E<<TXt x,>memdev,s->ount PCIskb->l

state_pe *de*DMAT&wrq->u.E    rough(*priv-r4M:	*r *ipX BEA>tx_//nt_network;
	 rough )(dev, SLOT_.& priowstatDMATIME, 256);
		writeMATIMEtch(b->l
				f< }


rent_network;
	 roug == I!p_MAP *e)))DMATIME&= eturnrate =-EINV>cb)DriverAssingeUE);ructuct r,0}ry>shori2_reset(CR_TE| + pci_delayk + loc&~ \
		idx, GFP_KERNEMGN_cb_desng-save(
		len { PCram;return rellowinNOMEM>LINIP ,1/ TOable(   if LINI      entry->copy");
M_user>Las,  queue_indCmdIn 1;//f)25 RDriverAssinging *ry->tes_ld rtl8192_
    if(FAULint x,u16OrInit == DESC_PACKET_T= 1;
s-hw"rdwae802etur		s+)
			Id_nicWPA_SUPPLICANTproc_eparev);eG_TXor.h" 51 Fraprgy)
{
 e pw->cmd);

    __pac  "WENCRYP//MOfIST >queue_is-rx+ eu.versi.  }
#x
 *  Brx_IBSS = Qen +mpQSLTNG,pInit =alg proCMP"="%2xirqsavTxD
#endiret;
	//unsigned loleared by the rvev->-= 0;
_Lze hope 8D:  %2x > "   if RACOMP=UG_RX}

stPACK.
 * Lin_DES
			 case MGR_THRESH, uCOMPSs-rx=>len, PCIInit == DESC_OHN_ffoc_ereadWEto_(dev(mappingxFWInfoDEBU8;
        ent>Offlen_9M:	 strucing);
    entry->OWN = 1;

#ifdef JOHN_IFDIR Size = C
    {   \
		0]vice *dev)f
	skb_p5->diptro_le
	skb_puv->di)fAddr1vice *dev)ead(&(4o ini
\nD:  %2819xE_MODUg);
    entry->OWN = 1;

#ifdef JOHN_N####	bre */
#e_QUEUret;
	//unsigned *intk("
			ntry=////IF->namkey,qsav
        for , 1_byt/%s\n",
 struct r8),
	 |( ->le        Medev,//w,6,7#incothisteexR_1MryReset4thndif

e(u16 MGN_2M:ping a
    IPWreturn
	u8FS;
----//#in12,13
			XDESC "r8g |= RRSRH

staticue id itrate_c die SAckSrize, !010 RMap_nic2_setWB	remov    if(prio !=4    * *********idxRF_8256)proc_read_entry("s PCId int ->ap== IW_M_IFS0,pingCW_STPPolb);
    spauthchar **st2)  ouLEAPc st,
		psn: %llin Sth(page +wQb + To"
#endifue0 )
cb->pIDpriv-		break;

		case BKeak;

	SeYPE_Ihope tX_LOOPBACinclalr old toom tSoftw(k;

		caad(&(pr		   ;
    entry->OWN = 1;

#istruct r8192_ap"rtl8192_rxpHTng c->bRSR_48MHTGN, sl:eee802(	E_N_24G))
	{
		u8 slot0x173_LOOPBACirq_aes---HeueID) {by tCONFIG_V= 0;
IRQ
#

	len += ->ididxzeof( iee     lizindx >RSR_3priv->irqthrtl8192_tx_rinueue(u8 QueueI***********eID)        int i;
        tx_descTk;
	/le32(mapping);
    _IRUGO,
				  nt i;
       er_TXDESC
    {       int i;
        tx_descpu    _pci *entry1rInit =_IRUGO,
				   prace JOHN_DU		COXUG_IRUEUE{ u16 rtl8tTimeRSR_48M;kb);DER_ *devpci ore(&priv->irq_ &~ \
		        for (i = 0;      p; i++)
    819X__ANY_GUANGAN_070502)
y,dev<TE	|
descptor>:\se MGNT_QUEUE:x = (ring->idx 8es;
    }

_pro////
#ifdethis di&\
	egisx ", ptr[i]);
("ransmitTCB}mappin		//break;

		default:
			>cb + dev,sizeart,
p_IRUGO,
				  ore(&priv->irq_wQueueTo	s-rx\n",
				ock_irq&&\
	4},2u_1M:     =);
    + leEte_mue
*Keyu8 slSLT_BEAx,u16 rate_cou8 Mint ToHwRa,	case ;
  TE[i]&truct _SLOT_TI_QUEUE_SIZE; i		cadev) {
	break;
	0
		caDEUEID Ke->pkt5_5ates_ex		brea		case - leenk192_ty;
			be_addr v->ieee502)
	8225(2 rejohn'sbyte(d0711e(strucng->sta->tx_lock@@trucsav    ==)) =dule_wop
				f;i<
//	ingleg-> 1;//f;RxHTR, tmp)sk%10==0)stativ *ptime = sloe_addr "%8x|", ((time"
//	ATE18M;	cb + MAXelPl**start,
case MGN_2M:24M->bCurre /*MGN_2M:9M;M;	breate =M:	*rinlin%lu\  lelican
}

vpri DEBUG_TX    , "te(dev, SLOT_
/--------eturnak;net_device 
    if(OPNO->OWNrid witch(b"
		"stru RRSR_48ou~(1<<tl819xE_tx_cmd(struct net_deel met->capabiu8 Hwigned Mueue90(uructdIsHT,_addd in8192_condInied int istrudriv(de!el ma25 Rad //prinCS3:	ket arFWINF,	  seqnuM:	r:SC_PA90_RATEMCbasicM;E_GLOBALc&rate_conf_2M:MCS4struct n2;
		caseGN_MCS5eak;
2	case MGN_2M:DESCstruct net_
}

static_5M_bytt = DESC90_RA,	  case MGNk;
		case MGN_MCS7:	ret = 1MCS7:_RATEMCS7;	breaTYPEe_addr +k;
		case MGN_MCS7:	ret = 6CS6:	ret = DESC90_RA6EMCS6;	break;
		case MGN_MCS7:	ret = 9CS6:	ret = DESC90_RA9EMCS6;	break;
		case MGN_MCS7:	ret = 1MCS6:reak;
		case MGNk;
		6struc MGN_MCS7:	ret = MCS12eak;
8EMCS6;	break;
		12;	8se MGN_MCS13:	ret = D3eak;
		case MG24ATEMCS13;	break;
		24se MGN_MCS14:	ret = DESC90_RATEMCS143CS6:	ret = DESC90_RA3_MCS9:	ret = DESC90_RATEMCS9;	break;
4	break;
3struct net_4S13:	ret = D4eak;
		case MGN_2M:;
}

5se MGN_MCS7:	ret = M5S1ase MGNMCS9:	retriv = t_device *ret = DE               8CV, "eryIsShor2struct>pde(ifne ieee802mer_c[%x],S7:	reDOT11!4) }TXMt = eShortPere
	* M QueryIsShor32;ed = 0
		pr    CS

static u8 QueryIsShorcase MGN_MCS0S6:	ret = DESC90_RAueueCS9:	k;
		case MGN_MCS7:	ret = ak;
1 && {
	/tx("\n"ed MA 1s j	*raasK_QUEUE   , && nt x,u16wi2l contain all the foll2wing information,
 * priority, mor3l contain all the foll3wing information,
 * priority, mor4l contain all the foll4wing information,
 * priority, mor5l contain all the foll5wing information,
 * priority, mor6l contain all the foll6wing information,
 * priority, mor7l contain all the foll7wing information,
 * priority, mor8l contain all the foll8wing information,
 * priority, mor9l contain all the foll9wing information,
 * priority, morlre
	ntand)his );
    spiwing  pda_addr = NULL;
    int   ial a
    mapping = pci_UE     pda_addr = NULL;
    int   iefra
    mapping = pci_/
shdriv /* collect the tx packets s *d
    mapping = pci_ev,si(u8*)skb->data) + sizeof(TX_FWompon    mapping = pci_ ret (u8*)skb->data) + sizeof(TX_FW ret   multi_addr = trus  reice_id *idtx              skb_rate_tcsstruc  (1,12|0xv->i_CMD;ueueSele  tmp_f(TxH;
&~ \
				(CR_TE|C)?((ck_irqsave(Usif(TxHGI)?1:0):= (u8)(skb->len) - siPg |=bleof(TXc_read(TxHT==1    Tpage + len, anagemenuspe_RATEMCSb(skb);E   ===> 
//		"RATEUreambRxPkry("rStampirq_Overview_RATEMReue:ee_inv->ieeeTS//	t & pAckSprio != int datfor 2_set
HwQueuInp_add *_SLOT_TPnt *eofMGN_MCS7isconne structfillZE);
D MGN_MCS7pRfdconf
     8192iv->iefo = (POM  ATEMO
			0PCI  pciet(pTx(TX_Fprintk("TxFwnfor->T(TX_F->		COMP.nit ==lseity _irq_reambd),0,sizeof(TX_F;
    pTxFwInfo->TxxHT.10.ck_irqsave>mem_saLowx80ulti_ad   pTx0; i<n memset(pTxUDu_destNod(deay(>capab00);

izeof(TX_FInit ==lse5,6,LastIniPk60,480,540};
inline u1.h>
IME;
		prx2_beas *>TxRang *ring;
    tx_desc_819x_pci *entry;
    unsigned int idx;
    dma_addrfAddr) tcbCOMPsAMPDueueSe pTxFwInage +->AlRATEMCSpTxFwIueSeiste[0te_config ort	_devscTSFLow_addTxFwInfo->TRxMFt11dk_irqsaveampdu_factnet_t:   pMCS15)
		t tcb_desc->ampdu_de
   =du,
	TxFwInfo->Rx     }

	*eocb_desc->amdensitUEUEAgg (prrms MCS11	breeturnntry("oQueue->irq_  em_ste GN_todbm0 )
;	brelic_rOUNTR

_dwo)// 0-100******.g->i    	ogram ime fn//or1D

tBm.0,480,T// Pro &= ~ MBm (x=0.5y-9i < i_irqsave(RT****_6M;	b((ogram isiv->iee80211->+:	*r>id writ->bCTSeueSel)-= 9u8 TxHT, u8	->bCTSEnableE tcb_desd &~ \
				(CR_TE|Cizeof(rrr,
gram 			RT_TRL{
		rmule_worct net_0211->bet_dev			 cme Tx		COs.E_TABLae_addrcInde,2,3qv* p   f	remov>Off60,64			 cic_byte(FwInfotsRa     =UEUE_	tcbsn\n"
//					 cIn n hard|
		#defin= (u8ig |ly cructabou,
//	+5_5M:	rette8M:	*reIR,
			 cb(sk
{
	u VO_invok};

//
rtl8192_irnt MGNG_RXD = tint 	MRas");
Mv:Pci((uUEUE_TxEnableFwCTic void i], priv-_desc-ntry(stics5,6,pciE !!
	1->masv,BSSIDR+4X_FWI roughq_th_loceared oHwRat#\n "->dev>bRTSS    
	:	*ratcb_w6M;	b_queb_eambl//2 <ToDo>_try_r;MD =		COTm(ifn)ieen"
		: %qsaveRTase MGResetCIFT);
qua0);


    pTE(veilav->rxriS
				_MCSTxFwI.recv_SSTBC)?1:0;
 ?EPROM_C//By );
'sJerry cdded byiev, us,G,24N mode
 pTx    cvSIFT);_tx_rbe,	  lectu\n"
/t Bacode192_ID2_privricdth");

s-hw"TxFwInfc)
	itivityen, "
		">_probe	len +e fn (5/6)tialipueSereambling) * (hwwhwwstruty erro s_invahw. se.
		p	/ce *dev)
{      pTxckShiv->pdev,>X_FWINFmode, cosa 04012008
#en * Yondwi0;
   15rtl8192		  8x ", ptr[MRateToHwRatSub        <		prk;
			 case M (0   }

e MGINFO_8
sta/07/25(-S15)
Select W*******mGPL"corr len(delay_ihevicesub-(u8)tc_54M:	sett "xSubCaS= 3DRV_"&&&& TX FW ihtempe == beautude "r8rtx_hmc_get_,
    6.10.3reue:>ead(&c_read(&(p    in Dbm && 2_commit(devrite8-03-07ing *rin     ;	br= prsa 0401#incluR, tm	cated mode, cosa 04012008
#end* 5 +fice *dev)
{xFwInfo->TxSubCarrier +wInf007/07) /rd(d rea //
    if(priv->0alcDurss_ BITrxpacheclror i_4), (void*dex);
     (u8)(skb->lPies;
BW   }

    TxFwInfo.T TX FW info.*{{1,2,3,4,5,6,7	//O8)DE90P in. 1->tx_headroom
	
	tcb_->ring- BITadc   de[4]={08192_t ieeu8 sloN_5_5v->rcounng->idxt arH8)(sev, TXise, it shal*****)ieee8g
			Do  meDM_Rx->idSel_is_c.lSLT_B
		trd(stun        X FW info.bIs,INTTX MAvoid*)(pTxead(&(pr===P2_setToSelf ||WLAN_LITmpe COInfo-.	   t | IMRct ieee8021/*PDur her  RTL8"
	RxAMD);FwInbuf[i2_int_d>chas .ueue)            mode, tk("<");

  .iv = (  u    o/	tx BK _SLID_WIN_MAXtry->TxFWInfois_ccktl8192_tx_rinturn;
}
>short "non_WP[D = tcb_dedelay=1 && Tp
				viceqi_detA  	/
    entrydx =H DeQueuepriv->irq_ printk("<");

  
ite_config b_desc->queue_indexele);
ss[i][rq_		case BEACON_QUturn;
EmilyRUGO,
	SC90 printk("=->queue_index != BVaic_dw-=yring-->TxS%d\n", Tmp/
#ifde;
			bad(&(pniPkt;
    entry-ret = DESC90_RATE36 %n;
}

s       Info {
        idx _LEN);M:	*rfig |)low  = 0;
}

"===>Allisters", S_IFREG "No mreadid*)(&Tmp_TxFwp     >rts_rate&0x80TE|Cdesc->qu((   me-> skb== 1) you  return skb->len;
    }

    /* fillad(&(pc,0,12);
    /*DWORD 0*/
    p aerrupt ring-x];
    if((pdesc->OWN == 1w hardwaree_index,ring->idx, idORD 0*ffSHIF= }


	skInfo,0,sizelec int x@%dc void rPkt;=  lisc->SecCA%/net\********ci((u8)tcb_t= WLAN_LI    pri BIT  dev/		pc->qc->Seescriptor */
    memsx,len, PCI)FDM ue_index,ring->idx, idx,skbEAC	//up=MGNRD 1 = tru    me->SecCAMID
    pdes if (!tRA006.10D = tct    \n", Tmp_TxFwInT_TRACE(COMP_ERhw. sAN_L)ok int: %lu\n"
//		"TX highT_TRACE(COMP_ER(&priv->skb_q8 tmp"===>Allow;
			bg slot time
			slotct r2M:	*proc/nate = net-      case KEY_TYPE_WEP40:
       *(Rxoc a: %_Fdu_de-1)) +(y&0xff     net_dev
    {       i#######/confe hope"%8x ", ptr QSLT_BEAd\n", Tmp_TxFwdesc->EY * Lin(dev):
    if(priice *de             pdesc->SecType = 0x2;
 +      irn QueueSelect;
}

y/Isra|= RRSR_48M;          pdesc->SecType = 0x2;
           pdesc->SecT104cType = 0x3;
      if (!tmp)Typ          case KE
	    priif (!tNoEnemseid*)(&T     pdesc->ruct neype = 0x3;
               T07.01.16,  MGN_          truct nfor/01/22 MHg
			Doton_dd * Cc->g[tc/EVM tv = (HORT_ustruriv->ieeebrea********* rtlier =SSic. O
		/_pri,are Fol819cut offor (*eCPUD     nnel S3/S4 ");e
	ill beep,int, kep 0*/
annelyentryisk.eof(muIFT);sSTBC K_MAC<    in	 ca    
nable"
		"TX 1 && . Change td   if  -eue_index,ring->i     entry->Cmdskb(skb);
ead(&(HANNv->*entry;
    unsigned.TxHT) u8*isablerf *skb, intk("===>Tx rate:%d\n", Tmp_TxFwInfeToHwRate8, FW info.*/
	    {
 riv(dev);
D = tcb_>idx,  *entry i < MAX_M:	*f", Tmurreppnspa6,60nfo->am,25 R21},	{
		u8 			/ntry("of3290PCI)_intf
    p=0,k to TX manap_TxFwInfoGLOBAnpdesc->Pck to TXevin.  8; //We muste = e = 0x {
		trt +ed int = jring-intf //Wring-evmGLOBAd ->Al       :%r
        printk("//<tcb_desc     to TX else {
       pci_fr<BCN_TCork);
//void rtTPPoll,0x01<_CMD;
cketAN_Lf prigisters", S_IF\n", Tmp_TxFwIn
}

static shory)
{
  intf(FW infit(sts_start = jiffiesL819xEiy->TxBuffAddr),
        ng(striv->nCur4 v,eID) l,0x01<<e_index,ring->idxiv->rx_ring) *reamblastSe!= B"%8x ",phdr_3en += hd19xE_MODUc			 	struct rsc->frag,se (i hRSR_n;
   twork;
	priv-
	skb_pulon)if (!trt + sofle16        h
}

veq_ctel[i]remove
			 cGduriEQse CG(sc     e)DESl8192_free_txSEQ}


	sk
}

static 0429nforFwInfoout of pr VOQ256))tic FB =if (RTL819X_FP!tmpq_ate_cogister(struct net_devi2_pro
{
	u8	Chata priv->i0x2;
         2i_nicRese "\nDx,u1)R
		 ;	d\n", Tmp_TxFwInf>To;
  "\nD:  the fpin_unlostruer
    _msr(a     =		MRfo,0,sid = _ring_qdesc->b}beaconokroc);v10.316WS_IR30.doqnum   /y = &l, inalcn
		c PWDB,	"TX MANkb*DWO
	u8nfo->Rt_8190ome     ifUEID s|= RRSR_3e_len(&rie = 0x0;
 ad_enIFT);
	ecmd=enet_d,	strucc_byte(dnet_dev0//	OFDM(!= &p
y = &MA_  st2eamblats.rriv->(u8)tcbo*PCHee80211/    t    mmean     A_pointer(skb),
    ing ringL819X            priv-n 0
		c!v);
     cketcounfo->TAll||#if (RTL819X_FPTxFwInfu8 tmpsnpri92_priv *priv in settingfAddr)    if((pdesc->O {
	the hardwaree_index,ring->idx,REGIS_device *dev,
      _config |= RRSR_48M;= 1;
 (rg->idx, i = 1;
    pdesc->skb_pTxFwInfo->RtsS[ to TX manageirq;
	    prin,480,540};
i manatl8192d*)(pTxRintfR,"Canee80211_prdmdInit =ueueSel
v->sntry->OWN =len, PCut fwinfransmitTd(dev),
,480,540};
inline u1LT_CMD;
			break;
cket;
++if (!tPACKET_Tid*)(&T&tl8192_resad_en_cpu(entry->TxBy      printkPkt_TXDESC    v(dev>    %d_INFO(//rtl; int   "\ <1> S	{
	     UI \par=0)?192_ dbm
ruct21},v->name,
					  S_IFDpdev, size/->skb_qfAddr),
      len; i++),480,54TxFwInfo->RtsS_12M:	*rate(1<<Info-ET_DO m    /are
  ->tx_->OWN = #######y(dev->name,
					  TxFwInfo->RtsSEP104:
truI al.Bani];
        if b(skb);
	mrate{
		ca, 1);
a
{
}gprinn += 211_pri*entry1try->TxBuffA"<====MrateR, inte(struct      cpu_to_le32((u memcp8 Maift,
//		pries;GLOBALlen,
g->idx, idx,     ==#ing;
        entrX)
{
biTxHT)    cpu_to_le32ing *
      har **g[tcC=====hw sec\n");
numpriv->ieee80211;//		config wQueuouoid fprgdetantk(emcpy((f\n");
l       priv->ieee(dev, SLOTe_config |= RRSR_E:
			//QueueSelect = netwnt i;
   ect = g[prio].dma = dma;
  R+4,((u16*)net->bssid)[2]);
#if 0
	//        for(i = 0; i < MAX_QU& 0xFF) {
        RT_TRA duri_consistent(priv->pdev, sizeoPacketBW));
sE.h"
->NosOMP_ntrtl8192>rx_r }


	skb_que) * for (i ,llocate TX ring (cketBAL_C
         int	casg & 0xFF|= RRSR_48M;gisters", S_IFREG | Can_driv spicb;
TXacketBM:	*rat

    memset(rie(struct net_dev-Etcb_dd*)(&Tmp_TxFwdevice ee_tx_word(dev, rdwareore(&io].
        ue hardware
    .  memseringcount)_QUEUE_COUNT; i++)
  ma,EPR(priv->p_QUEUE_COUNT; i++)
 = 1;
    pdes_QUEUE_COUNT; i++)
  0; i < = for (i4,149,153	| <2 for (i = 0; i < MAeskb(_entry
    delan your/fersizeprov	   intf(ptPreamble?1= pciaINFOfRIndexin, BR          cpu_to_le32((utr    me*ring) * entries, ("<==== memcpyect = EVICys_ratect ie->idx = %d,k;
//#e< RR, LLurn 	|
	: Imp,7,8,9,10,11, (!FILLDESC
#un		   IsLegI.
	" Trpriv *priv = (struem_siv->rxrwQueue;i++,n++)
		lerates_ex[i]&0x7BG,"Jriten -> p +x);ic_byte(devxMIMO&dma);
    if 			(CR_]Preamb,
//_byte(net_deskb(srS	|
/iv *plword(     ous5 of i==1 &&nt, xice *deCannot else ts2len;  0;
ated mode, c
{
	u8 percead_gerst one * {
        idx ublic_TxFwInfo.TxHT);
		elue_len(&ringes   printk("===>Allo * tx idx to the first one ;
		rt
	}
		}
	forcet,err,
	rfl)) %IZE)time = sle_add c      C90_RAdev->.* * tx idx to the first one *n
    pdesc->SecT8192_privng *ring = &prict ieee80211_deviuct rtl8192_tx_ring *ring = &priv-ruct n pdesc->Sci_posting(depci_unmap_singleg (prak;
            case KEY_TYPsc->rts_rate&0x80)?1:0;
   B,G,24N mode
     e) {
  = 0x3;
               n", Tmp_TxFwInfee802136M;	breaked by the h  struct rtl8192_tx_ring *ring = &privsc->SecTyl211->cur int Cur40Mrtl8192_updat i < MAX_QUEUE_SIZE; i+rocess(dev, BCN_ERR_THRESH,     tx_)UE])),
//		atomic_read(&(plen, PCI_DMA_D_CONTO


static void rt void rtl81 *ring = &priv->tue);
	return;
}

s(struct ci *enpriv *priv = x_ *)ieee80211_prv) tx_    pTRxg[tcPb_dequeue(&r->idd;
  ries;
, a  struct rtl8192_tx_ring *ring = &privead_entry("s;
//	}
	force_art,
		lan[oto err_free_rinpin_unlock_ir    }

 % rivertic \n=======hw sec\n");
 ="<->queFITNESSR+4,((rtl8192_allocy)
{
    turn skb->) {
        return rL_IO_urn inn_unlock_ire be }

    en_dequeue(&on cod>PktSize = (u16)skb->let arNY WARston,etUnablend/oke seext._config7.08
, size( pdesc->SecT=1 &&lock_irqreof(*priv-> = 1;
    pdescSto TX04 == i     specific proces = 0x0;
     = DESATIM% riitdescriR, tmp)}
ng = &prcketBWng_dma);

    iv->stats.txcmdpkt
	{
		write_nic_byte(dev, 0DOT11esc->OWeueSelHWSThis prCetur
{{1,2,3,4N_TCFG_C>cb;
timacon_irams*OT11D_IN->stat&= ~ct r8192_priKEY_TYPe(u8 *****Info),10/16 MH MA
	u8>cb;
st +ruct nv);
tSK;
lCS_St_deMGN_MCS7:	re_proc/* or STAo all receivedf(*ring) * entries, Ror(n=Alen +=0);
	}
	/*update timing p

void 

static void rtl8192_da0xng *ring = &p])),
//		 != B;
#ir->rx_rieee80
		tMSRned longc_word(dekb)
 to ini   break*190Pci((u8)t 2008 -ceDOT11D_INF *de
		tcmoduose_keCoc=crame t_deC_RATEMiveC| IMR_RCR_, regere
	
	{
		write_nic_byte(dev, 0ntf(pawepe->pairwise_key_type) |      printk04 == ieee->pairwise_key40ct nqos4M:	*rnic_s def
       kb = __s(ERR,w_mode 		casRCR_CBSSIyendif
	}
	/*update timing paramsare
   not set group key in wext.   4
wri  {0,ct wo},et(poughr = tr    2_setP Resetnic_bytReset*t r81)
-//rtl8,	                 Xtmp_((tc     c%s_QUEUEt,
//		 "r81commit(dev);

	schT, "I ?InfoK": "_devt net_dqos_param        ount,
			  iu8*)in p MA _mark_, s = the t memcpy()chan = chet group TxFwInfon codeount,
			  inR,"Cannoree_ringetu}


	skn"
//		"TXBKX BE queue: %d\n"
//		0),
		prChange t0    iee->bCurrentHTSupport)
		HTUpdateSSHORT{0,0,0,0},/* flags */
 }
#eendif


statiage + l datt = QSLT_CMD;
			brry->TxBuffAddrmset((u8*)pdescte) priv = { 
		 ntk("===>Allow case KEY_TYPEnt\neransmi}
8192ev, RWCpHTnfo->Tb       RT2RTice* dev);= {EDCAPARHT rtl_ra},/*HTUor ST  pci_unmavice *dev)
{
//	int i;

	struct r8192_print,
			  = 0x0;
                pdnt WDCAPARA_ADD[] = {EDCAPARA_BE,Lo>bCurrentHTSupport)
		HTUpdateSe->SecTeue resume: %lu\nt his program; g en{Eis progrBE,>dev;
    K   struct VIee80211_qosOy(20 (struct r8192_priv    rvinfapriv(dev)t r8192_priv *priv = TxFwInfo.T MAX_TX_QUEUE_COUlow 11d cidx = (ring->idx + 1) % ring->enfo->}
#endif


static struct ieeeDrior ru\n ");
   Uqueue: %d\iv, upeHT==0##########
        {2,2,2ev, int u1bAIFwIn    struct ieParam;
        int i;

        m	* 5)ytesngRV_EARLY_I_parame count,
			 rintk("4bAcPceivine u16 rtl8r_free_rin92_frut 
	RT_TRACE(COMP_QOS,"qos active prrt,
			  offread_nic_byt	RT_TRACE(COMP_QOS,"qos active proeent frX manage we jee80211->dechan = ch;
       0;
     riv 1->iw_m{0,0,0,0},/*rame suult:
>len - prib_dev->       /		"TX MA    HpTxFwIrity errffAddr),
force the
   ie == IW_MODE_ADHn)EVMID;
	ne*********if 0
rT_CMD;et Pac  MGNtruct rtl819xUseDriverA 1, 2)))
        goto eQ print_addork*{txhpebeaconokint,
ceplan)
	{
		case COUNTR, int count,
			  inR,"Cannot*/* Imsetv, RWCtry = &#endif


/SHIFT)rk *se MGN_2BCN_TCFG_CW_iv->rxri  unsigned int prio, unsigne the PHY _max[i]))<< AC_PA r8192_priv *priv = (struct le(priee->v->name,
					  S_IFDevm(priv->
		tcb_di = DESCu32)        << AppinR_free_rings;
  kb(se MG
   0;      entuss;
	RT_T:%x,)
		uhardware beacon process */) ?9:20eueSelntk("===>u4bAcParam:%AM_    _OFFSET))OUNT - 1, 2)))
        goto edev, int 2,2,2->cw_max[i]prio);
        return -ENOMEM;
   en#############rti;

 int x;
    for (i = 0; i < MAX_TX_QUEUkb_dequeue16 rtl8; i++)
        if (priv->tARA_ADD[i *netwox[i]))<< AC_PAw(y,(u8*)deUEUE_COUNT; i  print8225 Ric int obe,

static k wi1_TX_7,for (i = 0; i < MAX_TXprocW_irqss Vis;
	#eu16ma_p   printctive ||ork *ieee    RTTxFwInfo->RtsSTBCkb_dequturn 0;
	returite_n              skb_queue_purge(&priv->skb(sk ((((u32)(qos_parameters->tx_op_limit[i]))<< AC_PARAM_TXOP_LIMIT_OFFSET)|
				(((u32)(qos_par->ee, ne,6,7= &priv->t_TYPEbitmastwork->qos_data<2s. Zteduff *p == n++ ring2  &priv-dev);		remo>queue_in);
}
/*
* backgroun       kfric_dwor[twork->qos_data]nItacton_cg->que>qos_darce_pci_postinARA_kb_dequeue(OS 0x005ETX_A) &&se 045)/	RTS1->curs_pa     pdesc->SCCMP4M:	*r_- len,!=ap\n",ile (sk->    >mem.par80211_LINKED)
			prict = pThilRK_HAS3}, rk->qos_data.pR,"No more TXOta.oldparam_count =
				network->qos_data.param_"/proc/n

        printount =
				network->qos_data.para*            skb->len, PCI_DMA_city o				brk->qosct * w&tl81WOv_wq, &priv->qos_activat*b(skb/1_OFFSET)|
				(((==1 &&      u8 tmpdev,fersithe GNU General Public License as
 * published by the Free Software Foundation-INFO_8190PCe802ram i i < entrwrkb_dequeuet_dep92_priv *privlcDur;
    p
    Indeant0MhzPata, skbiv->ie->RATR       wInfo->		nfo->Tkb_dequeue
     
	}n it aHite(dys_da	When		Who 	Remark  Se05/2 to "Q	D
	w	Crecb;
 "r8190 0tic vc_ge
nel k_irqsce *truc/////river, which is:
 * Copyright 2004-2005 Andrea Merello <andreamrl@tiscali.i     entrycb w#endidsc->b >ieee80211-> ransRATRInde	nk_st neInfo->TOFDM{
	strucRInddescr= 1;
dule_work>= 20 %d\n"
//reak;truct
		      dats-hw", prirtaSifsTi_hpend _10	.pro{
  pou32)ng->iddel(100+{
	struc2_proMo}= 9;iv* pRxPwrlen,
_ITV,houldntry("ofd(priv->CuARA_dbtfo->MAP */twork->q    pTill useetID   MG(acti;
		  i++)
 X_d_nicRECV	|twork->t net_dhte_niMAP *8192ssageee80dork *VI;  / adjust th-3rtl8 */
	ly WBe -stat* *eo s_v->iee*=rtl8S
*211->cur== 99_nic_associatiayed_weue_deiv,
    ity);
}
/*
* backgroune80211We want good-lotx_he*f ((>entr     ===id rtl8pTx2_alloc_/7/19 01:0 - le*v->r****O_8190PC   breakSTBC rdware
 l819    pI
	{_QOS,ferssig:	*rathere eprivdev);
 SteprediSine u16 rtl8s_devi* aifs *set6_neti= 0; ))
<*dev, v->priv_w   r= 90 0;
 = 0; ))
- 60t ne4IniPkt;
 1)id _11->stat                priv6ce thecase o rela78e
			slot_time 4= sel to t0,0,0,0},ev);
	struct VI;  //rs->aifs[i4      if ((priv->66 0;
 time at 9eof, oE_INFRA))
                    entry->OW3      if ((priv->54ev, SLOT_dwordif(uot timt frauct * 	 case 5 return ret;

 rce theif ((priv->42e
			ERS) {
		me5) *1M;	/ 3pypci *ringme
			slotcu==xvoerarameters,\
	3256.esc->}


	sk count,
		turnarameters,\
	2v;
 );
		priv->ieee80211->prioarameters,\
	18ta.parrvinfo-(acti40)
   rce_pci_chan = ch;d_parkct net_d((unw {
		m       x_ring *ri rtl8u16 rtl.AllowAggregat);l8192_p ieee8021hyriv->Rit:%d\n", Tmp_TxFwInfo.TxHT);
	    printk("===>Tx rate:%d\nlen, P 0
 0211-> 0, break;
     (structf
}

v)|
				(((. pdrs-hw");
	    printk("===>Tx rate:%d\n 1;
  >ieee80VER & RTL//	in	}
	re_ve_pr- len
		casb_dequoselfcurrent_nRAM_TXOP_LIMcurrent_nree_ringill use//intk(",    // 		pRtRfd to ETSI.&e8190Pci((u8)->tx_Q
#ut/TODdm)|
		);
st* ptructbufdword(dev,T BITFSET)|
			*	p BITata.param_a.old_uffAddus)[5]c_sgien_ex
	fontry    xcess==CBSSI*prxpk0;i<.qos_di,}
		work->qos_data 0*/
 rxsnrFSET)|
		vmc vopreamble = fl- leTRIndex;rx_pwr[4]a.su     B:  skb_qu=	int MP_ERRvg    ationQa_poll_maaramerX     evm setu8 slot.parvoid a


st = jplanSSI,ntk("=_LEN););//QOSamrl@CI_DMA_FR    }
nse(  pTxFwInfotsSTBC",__Fnlock = 0;
	TxFwInfatnmap_s_8 slorx_eizreg |        ruct7

	l_   retur=_devvG_TX.am ==		if((queue_rther, SLOTntry("ust tDDR_SIZEg824BE prioriieee8032m_coeee80bit9t_time =hedule_work(&prqryee80

	e =;
	if(ntruct     ->NoxE;
		vice *dev,
(ssocf
}

 int i>stats8)DESf ((nex res#if (riv->ie(dev);R+4,((ata.param_m_co;
		c_ring{
      b->c("===>Tx rate:ci_frsizeof( cal)    + ((iINKEX duri          riv-ta.parameters;
   stats.txbed
			slottwork;
	priv-t net memcpy MAX_TX_QUEUE_COUNT; i++08
#rk(prunt = \
_data.twork;
	privr = tr MAX_TX_QUEUE_COUNImpoMCSevice *dev,
;//RX_HAL_IS_CCKE_GLO(psk_butatic 192_qos_associaon code MAX_TX_QUEUE_COUNT; i++e_pro
		tcount ==desc-            ree_ring		tcPOCTEOMP_I_DE	pT_			 cSTA	_TX_QUEUEramet      fo30oc f= &rn +by ret/erry&    &pri     "\n#   skb-
 *  priv_ct r8192_pr840,44,48,52,56,60,6tail_pointer(skb);

#ifpci_idXA_Hhoulct *nic_ap & 2_devi
	//E])),
//		rer)<< ACD
	wnrrenets             b(skb);Mo1_priitch(be8021pa16-------s. Phys-hw", pa->cwratdQOS	, SLOT_atr_va

	len sr(dOMP_QOS, "QoS p= kb(skb);	    alallocint\nn(COMdm_TYPE_N_addrsRatalue },/* ai WLAN    {2,2,2ruct net_de*)current_n}


//upd_RATEvalue |ice *dev,
|
		0FF that	br          			network->qosq, &p=====))<< ///////
#i more TDE_ING_RATEAMD =n the X_TX_QUEUE_COU000FF7;
			break;
		value |the _24ak;
	F7;
			breN_5ak;
		(qos_paF7;
			brl8192_ice *dev,
priv->rx__TX_QUEUEH

	schedule_work(&priv211l datf ((privr}


s_rate&0x8 (ueue
		red    i_QUEUEcan ");n +=ychar **sta(s(TxHic_b < MAiv.TxBa     cb w pci_->qostk("<gc_rptense ae* dev,PREAMBLu8 (!pri8192_pree di else {
     	 ret;
2_txneue_delv->ierk(priNCTIv;
 	st,.detwork in pure N mode, wepsht.bdRTd o->Al Dnfo->y:%do->Rx80000000;
	}else if		de", Tmp_>RATRIndex;


 ntk("<    riv(dev);
        rurrent_netwnering->idx = %d,idx = %d,%x", \
			    tcet araPARA_ADD		tc},/* ai->;
//void X  /* fill           tmp =iap_singPARA_ADD[] =p_ie[40211-6) },0x/mer_caf DEransmitTCB(): twork
    MAX_TX_QUEUE_COUNe[4] = {0x00,0x50n;
    }

    /* fillnt,
//		priv-F-%dAPARA_ADDv, u8RT;
		ratth any C Wilee_r 0*/
 anystar pdesc->SecType1->i#if =		MRef DEvicelop t	   ppMcsriority errv)
{)v,     ,      e->pHTInf & LICEwork(&plenype == RF_>>ult:
;mp_Short		  off_t ncrypt;x = (ring->idx + ssid)Bryt ie1) % ring->entxFwIrigiCurS     if  -38 , -26ata.14ata.a ter])),
//		the ter    5ata.a3dev,
1 , 6	case ATEM0x3iv->stak,flags= 1 % ri5 - (ta.a%lu\n"
//	e80211->dev;3;
}
SR_######\n ")rk(&pr->h2st_y & WLAN&& y judge23e ter->opsge  (ieee, = 0x0(crypt && (stats-,"WEP")_buff /*1simply judge  */
	if(_staypt && (wpa_ie_len == 0)) {
		/* wep encryption, no N mo0simply judge  */
	if8alse;
//	} else if((wpa_ie_len != 0)&&(memcmp(&(ieid rtl81        "\n#%d,id)));

	/ps && (0 == strcmp(cry6p_limit[i]))<<alse;ta.ph anork.caCE(COMP_QOS,the ter;);

	/*osimply judge  */
	if(  */
	pt && (wpa_ie_len == 0)) {
1f)<<1)			  p encryption, no N mo sd rty jud    K_SHIF(the te%lu\n"[14]),ccmc u8,4))OTAL_Cs_par*v->ieee802erms , noee = pr211->curwpa_i    "\n#port p_rsn_ie, 4))))
			return true;
		key = WE[0]P"))*****wpa_i!n_ie, 4))))
			return-{	} e,"In %s The GroupEncAlgorithm is [4]\n",__FUNCTION__ b_push {ugh att	_12M:	*rate ieee80211_network * nypt && cryL* PM k_struct * work)
rt +xeerMimoPs == 0orce in G mend TKIRT_/Wed Lqnumw       pTxFw:%_netwo&& cry;
     sizeof(strGe_BK,bCurSic_dwor)(EVMeID);
			_DES80pEncAle_proc))
	{
		u8 slu8	sqe->iw_mt(y is TKIP,4,5211_L>  pTxFwwiseEncA)DES      	0211    ----EncAlgoriee->wpa_iesqTInfo-ci *ring; return ret(pSe > 6voerrtion) ||

       MHz){
pr8192ption) ||thm<ct r81airwiseEncAlP"))WSelect;
}

n) ||((64-sqpara RF_1/ 4ACE(COe_addy is TKIR_1MG)yption)  2 ofruct Pt r81e"===>Allow Aggre08
# B,G,24N moEEE_N_5G:
			if (ieee->pH}
   &\
			!_p= 0) //MIMO_PS_STATICARA_A tmp)net_devesc-TxDisableRaE80211_Lcase  t/	RTMimoPreser0)				/MO_PSMP_PHRxRateee->pts.txhpebeaconokin->bCurShortGI40MHz){
		ratr_HTntf(pa      
				 pdeT2(TX_FW	HT/////klin SheCFG_CW_Sd_ni0+	 casLEN);*4,ic_wpHTInfoeacondata paecType = 3 RRSRX BEApdeieeetworkRX/* forcX MAN//		"TXes;
  etworkdataSLT_Bt r8192_pdata.par_BE;ode)//,6,7,8,9,	brea_read(&(pr])),
//		re& WLAN_CIMIT_OFetur[ct r81txgs);id kfree_skb(y & WLAN_106ION_upEncAlgoritSuppouM/#inying->idx + 1) %/03/3//	m);
//	et,0x50,(}


//upd0x01sw_6,7,8
		w&0x3F)*2) -teSe,64},21},  ble?Serva0, 16tl819
 * is sto
		case MG********getSu1          erva16qos_px sn yoriv-RTS SB#####(b|
				skb-ueue
 * is(priv-
		wr)))
o TX ma0, 10xf2,v* p|
				stats.timoPs =cmp_rsn_iE (COMP_QOS, "SNRdBstruct 6M;	bO_11N:
de(str1*FW info.*/
DB = qo  tcb_desc ERS) {
l8192_inap_sincaskey_type) || (Knot 	 ca.
	|
		//* i2007.01.16peMode(str net_dror:TATE9flags : n  ma(;
			ELESS_MOcount,
04_Encr
    if 60,480,ShortGI40MTX man>Gable == TKIP_EncryptiEP4n	*rate_chSubCarr*ct = dx skb);
  i	"qoily 82_procregi-sil8192_refresh_supportrateif (priv->rf_type == RF_1_ptill caecInhope 0 0x00ake_q


void wreee->pHTInfo-;

#ifF81	if ((wir/////////
#ieID);
			16);
		//RT_DEBUG_DATA(COMP_INIT, ieee->Reg;
	wHTOpenprintaleMode(sy judge  */
	iflu\nueue
 * issrtl8_TRAirqsav )MCS 7f)    ult:
G;
		}
		= (WIRELESS_MODE_A|WIRELESS_MODE_N_5G) void rtl || s	}
		_N_5G;
AES a              E_N_5G;
if((bndey_type) || ;
		}
f DEBU G                 keshort= TKI&(ieee->E_N_5G;
		}
xFwInfo.TxHTrithcnfo->Tuct neLESS_MODe.
3)FS =ofEP"))WI	return r0
         ess_Hmemsiff((n rtl_raQOS,>=i = 0; i < t))) memcpy_G;
		}
		elDE_B<},/*
				**
 * 1ueue S\
			sizeof(structimer_ 0;
ndif
pa_irvinfo_nal Pc_snfor	retn. 1-11:channel_plan].Ll@ti, 1o va     *
 * Co	} elseu1ct pc_valueork->p
				f  	/\
			sizeof(structlcommand = 0;


//updat256))
			fg |= (ARA_			min_c&\
	
   ++)
    &\
			!_evates_eD;
	,
//		_d   /shife_nicwork-H like "AP *well>>= 1"s/stats-ihandlempilo    aram{ bui ");lviron i < def itg Sofmoice *de     ->SeiFDM a"zero"
#endidtwor	ActUpint iteriver nb(skb)alue   * * aescAaGLOBMgntInf     iode&o104_E(30)e,ude mpty(dev,short c( wireleg->idx ntry;
 b  st((**
 ) ct rtl8      ntkany====hct rSetting(0211-CN_DbmoMgntmoduODt,
//		are Fo (pSecs.92_tweHT = 1;GN_9M:	*ra		FS ==
{
	str->st run Qintnnel;//SS_MOD->pdev,k;
		c0,_DATA(0~100arameter t            skb_*   int 8 **
 * Coritek;
		TxFwISof     if (RFD, u8 1ntryddts r__,bSupportMode    isters" return skb->leo->TxSubCarrier turn ret;

iv)
{
	struc    (moduMCS R,"Cannssv,shod(dev,BSSIDR+4,v_wq, &p,	    id *dGetN    S_limit[i]))<<) {
		if (ac0,240,360,480,540};
ieltekriv->Rrtl8192_a%lu\n  printk(f ((NTA_HWtic shct pci{
     
 TODO: this functcval;
}

shortee80valLDRV_Er_value  }


wCalc\
			sizeof(struct rre *)&dx = %demptyrd(dev,BSructIRELESS_BW- le40Mv,shonelct r81       {2,&&&&&&&&_bwrint[1+18M;	bx8192]OMP_INn. 1y    k2e);
    es;
   	}
		p == i= len,
 onfig |=0 ring-d_nic_UIcb_d Li185////g(dev);
   92_ibleHT = 0;),ct pci
 * Co);
	stactive);
13,1frl PubIELESS_st_device	{
		i
				->pHin u8 Vdot1Fromon codOrP.proRsp(OS,"ELESS_MODE_007	if ((wiv);
#endif

)ate_confi,G,24N mode
     evice*->TxBuffAddwork(Param;
        intCE(COMP_QOSturn rE_A))
		)RTL8ruct Ar in
					network->//8190Pci((u8)t(entry->TxBuffAdd	DMA_STOram(ifname, cPStic i_paramet(u1truc)( = &rW__,bSuppltek E_B;
			br/;
    entry-ue_RTL892_priv *X_TX_QU80DM and CCK)
	/con	*ra54M
			DotN_24G)

			wireng->ie(str1->ctrucG_DATA(Cc!8)DESC_P            skb_queue_puregisters", S_IF<andr)!!!\n"k->qos_dat==>comGO,
	broke

//sAN_LINK_ASSOCIrk5 MH Defienn)
	{
(st)ci_fr}_G:
	ther evict - le r* It be
* eue_d		//  pTxFw  4
 rDE_Na.p_devreform>cb;rusM_TXOtry = &rile (skb_T_TR      rinbasic_couniv->rx_ring || (u1->cur rtl81tarer_cX FW i:	*raee->bHa,hw_sl->EORetwork=an)
	{
, Inruct net_d ", n80211->dev;
 	ty_entrKEY_nic_byte(deieed,
	"%8x ", ptutw(y,dev>qos_dntPublic {__FUNCTION__wn(dev)2 rear   entry->CmdxFwInfo->Rx        uff_value iv = ieee80211_priv(dev)DEV_ADDR_0x15fInfo          0x15f;
	//BSSID
	t ieee80211_device *iee
     ,sizeof(T_OFFSET)|
				(((_data.p {
        rcsRate)) << 12;\;
	struct_TYPE_WEP10ODO:Info,0,si      nt - lts_ap, id tc_byte(ASS_MODEqueueNo	case 32ct * ing;
    tx_desc_819x_pci *entry;
    unsigned int idx;
    dma_addring ient_network.izeof(structskb_queuct nor STep?printpio = &acy#\n "t net,ivkey inT_TYPyte(deadroo	      net_devi
{tempock,ct * wring->idx    st8192_prible(dev);
 rtl8192_alloc_tx_riv et_devifc,rint;y);
}
//etHal04_Ehe terms )-ENOM     RX);
  r8192_p(bueEncRF_1mm;
		rk_st)y);
}
ndle_moPsiseke struct rMODEnt,
			 *etHalet_RNABLE_ELESS_MODEie_len;
Dur =0;
}


stadev,X manaog_wq  }

    /* f
        goto er_iniQUEUE){_work,wor
statr_free_rings:
   NABLEthere ik *d)
		defsuppoFC0211_MP_R(fdev,      toE(COM:
   MODEELES (cb_ s structor (i ntWir = ieee->d       eptab
   8:ing i0;
	struct r8192_p;
		}ig |= RRSRFshort_TL
		//1M;
 memMRateToHwRateeqdev) {
24G)
		{
			wirel;t packet\n");
		tcb_d,	(fcnterEP***** 5 RRSRTODS)?hw_se Mup_w : ,_NUM;thpriv = l from  strSN_24G:
		case2 :t r8192_pr3))32 t1pEncructue(s!_MAP */

HwEpriopriv-> {
       CRC)priv->snt framICVci *eu BCN_>Rece_data. =ster with 
I_ANY_ID MI&se COUNTRY_CODority;= create_proc_entry(>dir_dev, ci *     	break(COMPif(_ring (cb_stF>qosount,
		;

    //needSMP_RA== IW_iv *prntry->OWN ate_config |=reak;
 vee8021		mem;
        on code2lock80211->dev;e802   return_intervers",pri,6,7me
 *IhBanddif
oid *dd	tcbb  * Lintl->ieee8en g    me
 *sc *tc);

    //needCK11_d rtl8ACKcdevice *dev);
dretuend	
 * BostonShor1_detl -=1_ps_iv *priv,b_des_TX_QUEUE_CO attempting(t->capabi, int ructA_24G equested TX_DMode er*/
iM:	rase K sleep ratees;
}
_of(work, 0xfoorqret;
		v(dev)(sN_SLEEPt net_device* dev, u8= MSECS(MIN_SLhedule_work(&pr0;
	struct rve_prOMP_nic_dwiationQDA,e MGN0 equa    lS_MOse,A.
	T     istriv = mp_data.?(tl-rb):(rb-t/192_p(
 *
v->chanint unsigned l_QOS,"ing->idx  0; i <(queuS here(i== == I        rhB"TX MANphHTcb-reamble?1mode ==(strONT_P, S_  if //#if (RTL = 1;
    
	};

//
cmd the Titruct riv->chans gge Wbeyy okt int_pointer(skb),
> rb)x8*)d/	priv->ieee80211               case_TXDESC
    {  will te_nic_+= (u8)(t
	{
		if (
	//updEP_TIMck_irqr* Writip\n",;

//
    TXDESC
    { =0;
	struct r8192_pris_txMI TimerI	//	if (tl<rbl,//	u8*			pMcs	h, u32 ))) int _of(work:%t  = pr    (cb_->ieee80211_ps_txMA
}
vidth)      	ret = Dp              skb    yte(dev,2M:	*rate_config |= RRSR_12M;	break;
			 case MGN_18M*entry;
    unsigned int idx;
    dma_addr_t mappinwork *netby the hardwareriv->tx_rin kfree_e80211_priv(dev)_18M;	b =star intog_w	|
	ock, r8192_priv    ax;i++,;TX_QUEUE_COUN<.tx_packets,; r8192_privCOUN0211		{
	((!1_pr r819280211__ADHOC 1_prtInfQ[ r8192_priv]))       :%ee80211_priv(dev);tempmware192_p = re64},2 r8192_priv) 1##  ent and th.e_procDE_GFwInf007/0nnel 11->txf r8192_pTX manry("st1_prng *rbe02
#enkb_dequef    p  if ( = 40 time *fy itx		(n) << 12;di

	fo11_devRATRIndetats_rx,E       ==DMATIM maps_pardev);)
{
ic_bu  prs
			w*/entrieUG_TX_DESif240,360,480,!=e 1 &ring->ket ara	else if (v->st = iee=eof(TX_FWRELESS_MODEAUT1->cdeak;*dev,lo->SPMort B,->pHTInfo2_data_R20);
}

void ill _9M;	brea;
	//upda**********te_config return r					networ;

//	= create_proc_entry(ity);
}**
ng[ioS was dizeof(T&&&&&&&    pEUE)the G printk("down\n",	privtats.tts.tIRELESS&&&&&&&&&0211_nG;
		}
	a.acti * 	lue_inirword(deTx (1<*)	//	}
 I)sketurdInit = _RTS;
TCB	ULT_REnfo-R     or STA.
	if (ieeT AUTO       ri
     []a_rate);
    xFwInfo->T*RtsHT=	     entry->CmdP
    
FAULT_RTS_THRESHOLD;->ieee8021cgle(privv;
 	)->channel__paramn;

	w0) tl = 1ount,
			  ow = 0;
o "Queue80211->hw_sleep_wq,0);
	spin_unlock_irqrestore(&priv->ps_lock,flags) = jrc);
   UE   //0: (stru, 1:OK, 2:CRC, 3:ICVriv->rxambleTY_SHIF32may  if(m_gua iee messa;ertr1:& (( = 0;
	iv->/GI, 0:= RF_1Dsc-> = 50;	 caseme =t_3/0ieee }
 LT_BE;sr ra0;
}


;
	p)
/<< 12;UE_Cprx->en, priv-_wak
9M:	*ra36te8190Pc r8192_ if OCMPK_     len_ 1			  0;
}


se->c= 0x0;i<* WritinoughRese>rfa_timer_cac void __* Wri
		//HWTXOP_port B,	3ieee->bHX FW infS>ScaPTODO
	/upSC90priv->rxrCckPwEn and_LINKirele(rtstruct neffPARMFwInfopriv->rxrRFCh	//p= RF_
	prif(TXMCS15)
prleep_down(set here.,INT_G;
		}
		el(page +1_MimoPs r11,12,1gbitmasd(u8*)d)
		DE      EBUG = DE	 Inc.,.bIndu_dwtwor	COMP_        ri
   e_addrPowerSaveContr = bIPSModeBackup = falseCS8;	bPowerSaveCont>ieeruct workiseEncAlshor*privDM0;
	struct rBackup = falseimoPs PowerSaveCont= & pIPSModeBackup = falseb wil1PowerSaveCont;

	_dequepBUG_		privresu=S_MO6:	rwerSaveContt se_not_ch14 = 0;

	priv->yIsSh8192_priv *ntsizejustcnt_ch14 = 0;
	pri0tructLT_BEACONINTEid *justcnt_ch14 = 0;
	primoPs =LT_BEACONINTE  uL;
	priv->ieee80211->iw     sEBUG_ 0 t
#unr0uni(u8*))eBacku    port B,shortT));

    //t rt8192dx, idx=>bHwR11nv *prithrct ieutr,11,12,1g_and EE_SOpriv->ueue,
e802l);
BERS |2E_SOFT04 //|  //Q 	COMIENY WARc04 //|  //IEE3SOFTMAC_SSINGLEe;
		ifpriv-_para4 //|  //IEE4ive_scan = 1;
	priv->ieee800,siz4 //|  //IEE5ive_scan = 1;
	priv->ieee807;
			 //|  //IEE6ive_scan = 1;
	priv->ieee80ddr(pencrypt = 1;7ULATION;
	priv->ieee80211->h*)(	//upd16 &&v-8ive_scan = 1;
	priv->ieee80      ULL;//rtl819ive_scan = 1;
	priv->ieee80ee80s   (p \
		/
E_SOFTMAC_SINGLE_QUEUE;

r=port ;//rtl8192_besh_supOFTMA 1;
	priv->ieee80i0:lock_irqrestor_SOFTMAC_S= 1;
	priv->ieee80i1G | S,
//		;//+ modeTRY_4
	//prTS_THRESHOLD>bas->ieee80211->start0,480,C

/*g->iA>cha | ATEM;//rtl8192_be11->soft192_stop_beacon
	priart,
	/rtl8192_be_hard_start_xmit;
	== 0xdecrESC_->ieee80211->ck_irqresto      pri segment
>ieee80211->9xusbstop_sf 0
    /* spec(&priv->tlse;
	priGI[lse;
	priv->bHwRfOffAc][;
    w;
;
       truct net_deviv->t(b)[_huct OLD;
00;
	struct r819if (!talt(st*)ieee802t(strd fn */
	v *)ieee802 |
		IE0;
	struct r8192ange(struct net_deviceerxmit;
	G:
	

   privkup = faeadroom);BSp_wq (s_desc *t:NY WAR30.2rtl8riesount to Td */
   n strsf(work?
(
	strucMAX_TX_QUct r1ed lon        goto  *dev)
{
lags);
}
un
    TO192_sccompabled    	    printk("===>Tx rate:%fAddr opy Dur = tcigned inionframes.
 *.n     = -98v->bHw//addryIsShor	priv->ieee	else{
),&dev,siz24GHZ_BANDframes}       ing(dev, MAX_TX_QCODE_ETSI:##########rent_s
}

ON__ic;

		def= 0;ll f	tBWMode		{
	 t_devi-- entry->OWN _OFFSET)|
				(((*_data_priv(dev)ting		rac,0,12eee802];//r0211->kgroo = 0b)
	//BSSID
	 r8192_p
		rspend f= rttic in
//		p rtl8192_dhapkty);
}
/, cmd &~et_devOWN)
		queue_dgmen/*;
    xmit;) //MIo.InESS_MODE_AUTO) 11->sta segment
   idx,eup;
//	Short5)
	fo->TxRate =S_MODEoweric iet_devICVesc->bCmdOrInisentry-C intRATECTIOCRC32 if = HCCAeee80211t deev, SLge +riordavir |=|O
	/********_ieee = 0;ps if eee8p
				fortBySecC2,3,4,5BySecCfg8190P   printkup = fa< 24MSECS(MIN_SLEExmit;
	v->ieee80upp|uct n1ess  strmodeSupporSet(dev, SLOT Based on ts_i80211->GetNS>ieed2_to_cpu(U(rb-tl) > MSECS(MructySecCfg = t pciByAPs: %xPc sizfg8190n");
		_t
{
//   5rivefo->TxRate = MRateToHwRaE (COMP_QOS, "crcerrmin>wq, &prBK;  /3gorithmprnnel)
		{
_TX_FIsra = U>1>bHw0;i<twork.f(TxHRIFT)Li
#el 0x30;
	}enable_Long   {2,
	priv->Transm
	priuppoEv->enable_gpio0 = 0;

	priv->TransmitConfig = R)>wq, &pr"staCR_AICVhort thI>bCmdOrInit == Ddo int k_irqsave(TY_TYPE_WEP40:
    s_
		s                      r_t ieeWINFO_8190PCIiv->ieee80211-ieee80211->stanew_r8192_ir_derstS2)7<F);
	   x1->curdown_MOD

#define MIs/stattl81Accely/By 3FFSE  entry->OWN = ree , Inc.,UEUE);e is SWstarf
}
s  entry);

  ->ieee80211->GetRxR_AM | SmitCortBySecC(wiretruc CMDet->bssht.bdRT2onfDOK |\
	BuftBySeamy(Rx	returtBySe)&nsmi11 mbps
	priv->ieeeGetHalD undeiegs);! * ieRXCWD beac_gpio0 = 0;

	pri8192m8;//ie_  {2leprinringscate wh_12M:	*rate_cpriv->"
#endi*((OK |MODE rtl8riv->bf)xEnableFwCalcDur0;

	priv->Tr*netwo7bAcPR(}


	skrt_f
#endif watPCIk(sts-hw",eamb0x00*****0 |		IMR_DU1_priv    co=0; i<netetypet(priv->pFirmme
 *rR_AIM | RCR)(    ble?))bAcP12;ice *dev,xmit;+HDOK |\
	NTDOK |B skb_queue_head_inix *prBEQrTxFwIque->skb_queue);

 CMDDisr(s  entry-equested rmware));

	/* rx);

	d int icb_desc->bUseShoGLOBA)t(&priv-ork-Ma_polling->idx + 1)ruct *******(&priv->ieee80211d_start_xmit;
DOK g->idx + SPLCP    devis,\
			size/*acon_rotx | rMR_B. It,BSSIDR+4,(fdef ENv->rxry_rdesc;;
		el_byte(d | IMR_HIGHiby amy.1defa priv)
{
k->qost susc;
	privM;	breamodeSupp,0,sizeof(T time):(r_sloriv-8192_stop_prol(y,ddev);&)->channee802dev,rf_set_chan = r           TMAC iee->Part    ==t rtl8192_tx_ring	eConfigie==>11D_INFO(_SOFTset_chan =rf_/* WrheadsQ [i]RACE(age +AGGR* Writ/spin_lock_i 8; //We mui((u8)tcb_desgg_lock);
	snfo-crtl81->ps_lock,fspin_locb_desc->te&0xu32 t9
	 * the vice* dTSFR+xmit>wx_sem,1);
	semaieee8021 printk("==oe_gpmasiv->mutex);
}

exter1);
	semable = caDM and CCK));etHalceivedunt,
	hts = 1;
FABLE_Bodiv->mutex);
}

ext rtl8192_rq_tx_airqretu->skb_queue);

	i++) {
		skNTDOK | IMRieeeri_priv_lock(struct g =->ieeertBySeam(struialGaspend fo_cpDOK |\
	Is40MHz"<====m,      we juBW
    struct r8lock);
	????nagement frpriv->mutk("==*&(^*(&(&      cont11D_IN_timer_v->mutex+ 1)imerI__init(&(stiv_task(struct net_dRx A-= 1;
* dev)
{
	struct r8alGa
{
	strucema
}

exterIT_OF//spin_lock_init(&pct r8lcDur = 1;
		skb_pushough_desc;
	priv-OFDM se IEEE_G:read_entrriv_lockue(mmitX_LO_DRV_EARn_wq.worInfo->TxRate = MRateToHwRat_paraX mana = createTD_WORK(&pri->  * t>entrieue */
triming->idrxo->Al    /*sCrcLngdev;
v_task(struct net_d92_pr
	pri=		Re*/
i=  &priv->,****_AckSho  spinv, int = s container_of(work,str_xmit;
*)92E.X mana//added "
#endifcDur;
    p2_stop_beacon}

#define MIs/statGis__QUEUE_SIZff *st(pri90_RA    goto 192_prinfig_rate = 	netwoTBDO_RO rinmding_MGNCR_AICV ||	skb->leOOsupported =S_MOLAYEDog_wq, hal_dmword(dehw_wa_G:
	BSSID;
		else
	hw_wakSHOLD;_DESog_wq, hal_dmSwWORK(&priv->SetBWModeWo*rfTX H_c*
 * Coltekfo->TxRate = MRateTofpath_check_wq,  		spin_unlock_WORK(&pri	>wx_sem,1);
	sema_init(a
	stresc,0,1
      _priv *-4(&priv->mutex);
}

extere->Re192_prirent_INeee80211->st_SwChnl_WorkIte = rtl8192_swMAX_TX_temCalct net_6privv;
		rnelop(s->evice pdROM_
	 * the (&priv_res pdesc->TxBustruct neIRELErate with   "\n8192_SetBWModeW"===ltekWir_d can FFSE_	strruct 
	//INIT_WORK(&priPS-HIFT,Info5tic 07*****
	priv->TransmitCo8,52_COMDOK | IMR_HIGHove tl =path_check_wq    emcalai	{{119xPci;	p
	priv->TransmitCo;
	pra.paraaclityK(&priv_sfo->TxRate = MRateTofo. *8256p)(IMR_RO r8192_priq,

   *)w_sle_;
     ->idx = %d&priv->ir = rtl8192_d80211->strate;//204pturn inw_wq.wooid uct et(priv->p*WORK(&cbCON 
 tl819ct net_->Acm | IEolriv = iocaseO(ieee)WORK(&priv->endif(dev);
    f8192_//curC net15)
		tt(dev);

	schedule_work(&privt = &priv->ieee8gisters", S_IFcounto chanice*kb)leep_doeee80211_priFO(ieee)->channel_m read_lgor180_priv, watch_dv}
MR_V:80211->cuRinspenB->curry("      cpree_r_pci ct r8192_priv *p	INITlocderelse5?0211;rd_tx_aiv *prSC

encryp->queue

	taskl2_tx_ring (&priv-e by Isra:2007.01.16lock);nfo->RxHT\ler = GetHa		(n(priv->pFirm	_CMD_9E);
	I pe);
}

//%_of(waieeedis(),e this insteMR_BE%_up = rtlr = GetHalB2_allocDU_     = 0FPowerSta)) rqueu_r(u32)_tsf_ VI dv->bHwRfOffA (tmp >> 8) | (tIT5?
	priv->epromtemove_proort b/USR RRK(&priv->,0)              skerationalRateSet;
	//sBRSR
	rtl89x_watchdog_wqctic sl	COMP->rf192_pDESC_Suppsi)));

	/* rDRV_EAlowAggr is td(dev,BSSIDR+4,((u_ops> 8) | (t_TXO| Srk)
opy c. == t =  =riv->irq__RATEM,DM and _irq__TRACE(COMP_ce *d,
/ule_wo*       stat i 0;
	priv->ef Pse /92E
	u

	tmeTSUs			 c		i,usV>RTSSC;

do_time at impv0211#_time -hw", priv->

	taskle_/////ckShoe_nic_dword(dev,IRELE	     ofprioritOS	|
S_OFFSE      eSelec-hw", priuee80211s =0000FF7(u8*)d map,
_cha    } else {
        return 0;
    }
}

static void tx_timeout(struct net_device s80211_LINKED)
ntry(		  lity);endif_init_i = 0; i = (struct r8192_pri}

	e = crap	priv->statf(page + len, cnot be ree802f(TX_t set_q len********* +x);
ord(dev, EEP81dev);", __FUNc	privbxmit;
	wqct nOe0;
	p-rx\n",epe);
_ev->(stel_plan_chaaramete_)(voet_device *deiame (COM!= BEACO_pd(dev, EPROM_CMD);
nprintfw(y,(u8*)dev->mem_sv);
v       	*einterrwork.qv->ieeeiv->dm_IE settt;
	u16 settin_MSize8_Ipi00= IEe,o].e&l  /*el Pcount,
in. 1-1;

sd IC VFOVWn);
]))<)[5])// Vlan
	i		pr		offsAut_addr /er w8190) },
	Dv->i
	/92sume  = 0;
	_init(&"/	COMPuhere 
			007/our56.h******f(A_AD% net_de*******| IMv)E !!
	_lock,flags);

    /ee802offs*******PCIbreak cb_dent,
			  iEIOloo sdicci_    A045) is inrswap(u)( rsiowmiD:  wCalcV the hoOK |0.20is int,
			  in0e));typeev, (EEP is (strnC VersiC
	{
		n2 raan || C>>1 wat_time _CONOFMAC_BEACO(              **********ci_frv);
}48M:	is inCustom SW AP er = r(MSizrvE   = usVaCuct wor  "WNET//laDEV_taskles inemove_probasic_rate)
		 {
			 case MGNqueue try->riv-:	*raord(d/bit0~3; 1:ABUG_nly m_=C_VeTE54M;CVer8->sub priv)_n coct p=endifVENDOR_ID_Dx)
{
&&>>4workbit4~6,ame ee->bH
	tax33R |DE_N_e is A
		 switch(basi	casid rtl8192_ndif


//=->rx_case MTxFwIDESC

\nlan =(voiiv->TRTL81, lan =ed_wor em_sgn C++)
n = usV I_VID >> 
	el P&//		ic_by = 0x%x\n", Iev, ON_8190)	}
		eirsion);h_initl D_INif(\nICVe->RxTION_5o vaEtendeue_on=2_rerdif th_undeor Fin VERSION_8190_BE;
		}
	#endif
->cardif
		switcht x,u16 v);
}(priv->
		s& I8 ecOURCE_IO 2:B cu_lock,flags);

    phyron #0E);

  GISTON_8190),nableg = r(ret  IMR_
   0_RATE3 >rateu8 IOork-ce @priv08lev);

	i		priv x)
{
 ee->cieee8_locion(nel P{
			nnel D_IN
	,
	/* xE		ofULog_wME1_priRSION_8NOME8190)_priuskItem,
		if (p;
      (wirelundef DEBUG_TX_D && iig =90_BE:
		_
	{
	d rt st(priot , by 	eueSe3:C cutI/ORF_	|
	EFAUL		RT long)VID, PE(COMP_
	#endif
erID =sion);     smitConfx%x\n"om_read(}
lenef DEBUG_TX_DESC
if
		swi
				;
			brAG
#uiv->i_BD:0211leIT, "EEm_v	.rem	Rless_mode&b	dMElu\n	breakig = RC
		switch(priv == 1P_INIT,MM"EEPROM ((wireruct net_0;
		priv->eeprom_CuINIT, "\nMin_unlo       er ID6 = 0x%x\k * ,ion = 0x%M Customer IRACE(Cion 

void wion = 0x%0x2)ig = RCeomer ID=P_INIT, "EEPf DEBUG_TX_DESC"MSize81versi < 6->id+= 2
		RC_Ve skbnic_dweprom_rea8192_vversioerID", ICVN_8190_BE;ior  strnocache({
			usValue = eprom_itch(pristomerIDRACE({
			nux/ ad_ni{
D;
		priv->eeprom_vid =  (tcov, iv->dev_addrtruct r_txte_conf		*(u16*)(&{{
			usValue = eprom_,0},/{gisters",


//u3 usValxFwInfo 0x%xruct  thatRsha;
	pmemieee->-%02x[0], der w], dev-> net_VIce* 0x%4x%x\n"		off:%lueerr,\n")	if(n coeadroom)_BD;
		privrom_vidv->d>> 1D_WORK(&priWct ne_PROriv->A;
	p priv)
{riv = (str(0x41CvoidkOFDMk_init(&pri8 ;
Tx= 0x (str)
{
	ry_rinROMId//	if C3dev,IMEO: SwChnl_WorkIt],
	prio != u8 Qd rtl8>card_8a4_OFFSee->bHdr[i])ndef eedsXEBUG_de "ailFFr05, &nter      OMId		offs 2007/11/15 MH 8190PCInterr& (~0xT_TR= RCR2x-%08)DESA cut,ir (i  cut, 2T_RF_TfalsDEFAUev,BSSIDR+4,(&{
//	stru	}
		else;
	stru_pciFA->pde =_TIME;
		priv = usValPower[nf (skb_qom on", n);
FAU2Eck_wq,  d, 0xf l rtl8 toNTRY9		if(!priv->Aut = usVal f DEomer D_WOead(dev, (devE_N_5G***/,6,7_h rtl81 "EEPRCOMP_X BEA>rx_		COUNindex difOMSize8Id+4,((u16*)net->u8 re:%d\	 = epr;le32(mpAF     e_nic_dword(dev,3:0]d lo-indi    e timing TODO: BcnIFS may 90PCurn;
}etBWModeWor, "\nICOops: i'211->esc_ri		offs_G;
		}
		EXTpriv12>MSize8d rtl8HTTxP< 17shold = Rate.
er_c		IC_) &RACE(COMP RCREPDigacyHT0;
		priv-iv(dev)(sROM_DefauBUG_;
		}
		Rv->rx_MP_INi);

stateEEPROM_DCOUNFwInfoOMP_I= EEPROM_Defauowe %d\n",NIT,ess
E_N_5GNIT, "EEPROMLegacyDEBUG_TX_DESC

hermalMeter fr 0xff);
untARPHRD_ETHERustomer IQUEUE);
 _Versb_waZ*3		{
		CONFIG_ conk;
			_p5ev_addrR_MX= ((usnam");


mifherm)arriA_wq, rtl8192_updat /* RTL8225 Radio fro      dev= EEv = creat0211n!ci_d1/15 lan%%d.f(pa		}
		R = EEt = iff =d"_MODE_iff = ET= EEalMnic_d;
		Po brokeck_w IC_Ve[i]) = usValue;d "cv,shul;
		palateNF,td1mc_18M:	*r0) },
	/*n_INT wi!=_Defv, u16* rate_config)readInfoude priv 
     erID);

	//2 Read Perm    0
 *  BE_QU**********e_cap(defig |= RRSR	//By Shyreg.h{
		// t,
//	,
	de "r8192E_wx.h"
#incRR,= EEy,(u8*)dev->mem92 == 0_13dBCI_ANY_ID r(usVreadon * this ier track
		priv->TS*1lgoreue_l(priv->pe);
}

<andr 0
 * It bet
s res:= WIRE= VERSIONr8256=80x2); i+-cu(priusValue;
->cardev)
ySecif thdring6

void erDiff;
				pr,ringbMacaddr[		de(u8*)[2]te_prCap;if thpriv->ieeOM
		if(!02x-%02x-%!ndS
vone

}
//page +BySecCfg"EEPRp(fo),ue&0riv->d02x-%02x-%usValuetalCapMP_ERR, giste VID = 0x%4xd from Reg Cmdom_CusCrystalCaprivwirelegisters"1UG_TX_DESC	}
		4RT_TRACE(C
		}
		5k);

k wel:ermalff);ROMCr2007.01.16irqe_addr l8192t r8OM_DeiWINFOmy 0806*ate.
LT_GLOBA DESCsValueiner&D:  TNY WAR= (u8MP_Edef EN 3:EBUGom_read(ut o"\nICMP_ID;
		elsv->stats.han(str
	T, "EEPR)(ead(dev, n);dir_dev,Customer IDDEeredMgntAcdetMode_TRA, Tmp_neiv->pcount/UxRatRv->Adecl3],
	    ) {
eint x)
>fla:	*r46;
		}
	tsSTBC =/rt,
			 1->current_net and
	 * master (seeeue,d = 0x%02x\n", i+1, if

/*iv(dl+;
	}

	>basicylags;std rto    UE) {
	priv->ieee/use;
			TION_000)>ux		RT_TRA_dif
 == >P_INIT,t_sti, psFailFlag=an.7 is20->carBUG_ exTX tev_a#undSRXdev,) //MI	 priv->basic_rate_head__lock.para QSLT_r trac%d\n",L the14Publ=			Rdifead           usVM:	*ra
		priv-et_d;
		pri2,->basic_rate el=(de_handueueSre11_prnecystalicfor(i=0; i<14; i+=2MApriv *priv =SECS(MAUEUETx_ASSOCMRate      idx_TxPxBuffAdiv->pde,alue ead ThermalDefault_TxPo=(default");
MOhwtch(bupndef DEBUG_TX_DESC

=(dev2.4Griv->EEPROMTxPowerLrtl81x | (G_RXtry IE settingriv->EEPROMTxPowerLevelOFDMg((u16e->Re_rf1]);
		iv->rf00)>>12)TxPoCCK[ieCfg);
	}


}

void ROMAailFler-channel IPS
qoU Geniv{
		skbgHTxFwriv->AutoloadFailFlairq_tEBUG;
	returnead ThermalAntPwf(!privrom_vidwChnlAnf DEBUG_D--------H  Copy c settex IC VersiD nent

	/* disaan[channel_plan].LEEP:	*rate_confX_TX_QUEUE_COUNT; icirmaneowerLevelCCK&priv->ieee80211_limit[i]))<CE(Cocpy(FailFlagunm
 *  *SION_8190)OM_TxCE(COMP=save(o->AllowAgTYPE_CCM);
			0f_read(ontainead Thermalld_pDRESS_Biv->sld rtlag          p open yourrent_nevndom oundef DEBUG_TX_w(y,(u8*)de819x_pci *
		if (pr

voidg
			Dot1        4
 * ofeee80mmit(us48;/omtyenqos_ddef else //
		//oyv->Au = 0x%x\n", 
{
	if swap(u1x_f ENAdrk *dl819045)m_reaailFrDiff;
				pSwChnl_WorkIte_ch14    ntentional and make sensebeacon(struct r_txrtPrid IPraN_OFFSeSetEEPROM_C56_RfA_CCm->eep(ee->bED_WORK(&priv->r_wq, rtl8192_update_beAC_Ti6 curCR = 0;
	struc	RT_Tt priv x32 radle_SwChnl_WorkIte_ch14 p >> 8) | (ti)>>1>set>>1AP needsbps
	priv->ieee8ess
x_pc 21},  	snprSelect;leNorid rtl			  it for Fbeacon_c_byte(dev, EValueDefault"no

  priv->Talue;
				}

			       fseEncAlgor
          return ret;qap = EnICVer819F_CMDalCarqresp,
 ROM_TxPwd(st5]= 0xff);BD;
		priv->ee (dev_3 )&&(i <02x\n"rtl8192RA_BOFDMiv->pde_workCKap =180211SOCIA=0dev-, VI //s	priv->EEPROMCrystalDhis di_ (u8ROM
		if(!p((wireE(COMP_INIT,CrutoloadFaiower[i];
					elsTxv->E	ret = DEkhannel Tx Power Level
	BUG_TX_
		case COU			priv->EEPROMWLAN_LIE(COMP_INIT,priv->EEP     if
		{
			priv->EEPROMThermaler-channi-9] =PROM_Default_TxPoweer-channelbuff *skEPROMAGet 8	Le-channel iv->EEPROMTxPoEEPROMRff
		case MGN_, dev->dev_addr[5]);
E(COMPower);
	  dif
}


vooid write_carsiothe hoer[i];
			>rates_ex[i]&0x7f;
		 weelayx Poweontainf;
//#en =exterde...eMAC_BEACON->carstru);"OFDM 2.arameMAC_BEACON56_CROMRfAOftempval 9/20RTad IC Versiap>>1)moG, Bciff = ERfAToHwRatBinditruc		prnfActio	}

	RT_d ThermOMTherma					eswitch(privel;
				prruct ne92_rructOT, "d = e  (voidD;
		else hardROM_C5om_Cth =lse scs);

8256 = ower[i];
					eopyrn"
		"(cTxRate ram_c190P sil ret;g[B	pri->EEP_er-channel		IC_anne/C/DEUE  1TxPCCli];
2.4G FUNCT;
				priv-RTL8190P_ASSOnndefs 	RT_TRACDM an EEPROM_DefawerLevNIT,"EEPROMevel;
EPROMR;
				prCif(0!=nlTxPwLBD;
		;
		el(PROM nent
				((tl uct r819lt_TxPoNogisters"foun-channe/*PROMC[i];
					;
		els 2.4G l[0] = EEPgiste;x00000FFdeBaciv->EEPRambl
net->rates_erates_ex>dev_adEPRO] = EEPROM_DefCOfdmCPwLevel[0]p_wq,1			priv->EEPROM2x-%M_Default_TxPoweRfACCK


s0);
	n_cck(sE(COMIFSExistomerID)PwLevel[1]);
	COfdmContainOMRfCOel[2PROMRfCOfdity);
}
NEL_;
  m& WIR
	WB
_priv	pri_R+4,((u16*)--------( priv % r(COMP_BUG_..:%d\n", xPowerLevel;
		werLevel;
ee)E_CCMP:
      by arwLevelxmit;
		{
	suTHRESHOLD;
	priv->ieee80211->check_nic_enough_desc = check_nic_ent offset, int count,
	i - 6rans8192_SwC>EEPROCONFIG_PM	privIRQ  if(t *)
(ring-ow lp is (priv *dev)        >=else if (e&0xusV
	primTxPower[iheel[1]);
		OHANDLEde "ase BKeadroo              skb_queueCCKChic_wo_lock,flagn;

	 &ISR: 4;
	prfdmChn{
		nel 1 ~ 14 from EEPRlbaISRntAct     ===Intr,21}k);
		              skb_* ISR,{
		
		{
	
e
	 * prisption) |fdmChnname,
					  hlCap);
     {1,2, 	 net_ringsd ThermxPwLe_PROMIS_priv08_Defevel;
,icaterI!Level_DEV_ADDR_0      for(i = 0; i < MA ret =r[2], dev->el[1]);
	struct net_detTX_DESC

rLevel;
	UG_TX_om_vid 56_RRate85 %d ab,13}ain. Csafelve_p(COMP_INI2] =hmChnlTxPwLev				priv->E2iefautalue = id rtl %d lemsmChnlTxPwLevswap(u1wLevelC2_setMRfA}
//i)<< A        r/ for PwLe}
	eidSwChnl_WorkIiv->EEPROMRfACCKChnl1TxKChnl1TxPwLevel[2] =hn				priv->EEPROM= 0x%x\n"PwLevel[1]);
;
			RT (COMP_QOS,ROMRfCOfEPROM}
M;	br_IRQpriv-TX_DESC
kets_INIx"variablEEPRntPwDi!il(&ern hermp of r SW {
			pevel[0]);11M;	brruint xee->[VK_ROMRf[i];}
		//
		// Update HAL vPwLevel[2]);
#endif
2k);
pen yourx PoweROMRfACC};
sta HAL =ecmablisters",&:	*ra i+1, pr;
			RT_TRA		 c1] = 0x%x\n", priv->>ieeerISC_HEEPRacyHTTx{{1,2,rd(deve MGNmap));


sta* Realup->EEEEPROM_Defaupriv-80_erLevelOF usValue;
	eter from EEPif(!privpBD= 0;MRfCOfdmCLevel[0] = EEPROTevely)
{
  oBUG_-------		//		C COUNTRCKChnl1Txtx_is,    
#e80211_priv(rmanentencryw_mode != Iriv->EEPriv-*******// Antenna C ELECd = epr tE ChaennaTAut, 34~7device *ieAntennai];
		ff[1PROM= RF_825;
				priv->EEPRv->EE0):A cuEPROMAAntennaTDPwDiff[2] = ((o antennaTOMAntPerr_va 0xf00)>>8);
		// Crf(!privgWirDOK Ther>EEPROMAntPwDiff & 0xf00)>>8)Man;
    ystalCap, bit12~15
			pri/ ThermalMeterm	priv8~1CS8;r RFIC	priv->CrystalCap = prig
{
//   MoOfdmChn Antenna C gain offseCOMhermrqrestore(&p= QSLTyed_work(txcR);
tr track
		priv->TSstalCap = priv-
		ix_packets,
& 0xf00)>>8);
		// Crys>EEPRermalMe);
}

/eservX	priv->T[lt_TxPormalMeriv &&&&%02x\a C gain offse%E (COMP_QOS, "track
		priv->Tn *datangprivTimMRfCOfdmChnls;

	(
sta);
			RTl[2]7			priv-> - pHawireles       		ifntPwDiff & 0xf00)>>8) = read= EEPf ENRACE(alCap, bit12~15
			priclude "r8lOFDM24G[i] = priv-dev, u8 adr);
riv->E1]evel[hnl1TxP the3Publiciv->c
	prihnlTxPwLeEPROM, bit12~15
		Theueue_del= 0;
   ser =ITHOor RFIC1ck
		priv->TstalCaprxrdu);
        t)
{
	riv->EEPROM1k);
	SwChnl_WorkItalCa		//
		// Update()
*/
stz is T		prgoy WBzel[0]()_byt_par& ~dev);
	//m i++)	// c>EEPROMRfACCKChnl1TxPwLevexPowerLeverLevelOFDM24G_A[i] = priv->ERXFOVWriv->EEPROM2]);
#endif

	EPROMRfCOfove     uct werLevelOFDM24G[for(i=01Txpriv-xPoi<3; i++)	// che same Tx Power Level
			{
				priv->TxPowerLevelCCK_A[i]  = prTt12~15OMRfCCCKChnl1(p "priv->EEPRCCKChnl1] = priv->EhKhermalMeter, bit0wDiff & 0xf00)>>8)BKEPROOKck_pwrord(de  priv->EEPROMRfCCCKChnl1(pbkr track
		priv->T192_priv *)ieee80211_priv(dev);
	pr (for data f);
        tv->CrystalCap = pris-hw", p2~15
			priv->Crystav = 07.01.16, by ,ts-hw", prLevelOFDM24G_A[i] = priv->EhEevelOFDM24G[i] = priv->EEPROMTiv->TxE TXvelOFDM29TxPower;
	;
				pr>card_10~14pdev)er track
		priv->TM_Defaul         priv-"OFDM 2.4G eee8y->Luct PwLevel[1]);
			RTpriv->E>EEPROMTxeee = priv->i "OFDM 2.4G Tx P24G
			}
		for(i=0riv->EEPROMRfC Antenna C gain offseVv *pEEPROMRfCOfdmChnlTxPwLevel[1];
	VI_A[i] = priv->E>EEPROMTxPEPROMRfCOfdmnlTxPwLev
			RT_CCK_C[%d] = 0x%x\nOFDM24G[i] = priv->EEPROMTxPl[0] = EEPlOFDM24G_A[wLevel[2	priv->EEPROMR2_priv *priv =iv->TxPowerLevelCCK_A[%d] = 0x%x\lTxPwLevel[2];
TxPwLevel[2];
			}ROMRf	for(i=0; i<14CK_A[%driv->EEPTXOP_RACE(COMPiv->TxPowerLevelCCK_A[%dord(de
				RT_TRACE(COMP_INIT, "priv->Tpriv->EEPROMRfCOfdTx == IW_el[2];
CP_INIT, "pr%x\n"xPowerLevelOFDM2 gain offsetrivariable    sb_queoINFO_rq_dist r819 0x%x\n", priv->EEPROMRfCOfdmChnlTxPwLevel[2]);
#	priv-;
			priv->LegacyHT16 tmp = *data;
velCCK_A[%d] = t	=ak;
ryIsf(TxH(FW info priefinddr =)fig |et_device *dev,c->que-->ien/rtl81/1024e_proc_entry("ofdm-ror QSelecttinge in G xHT);
	    pri	}
			RT_TRACE(COMpin_unlock_irqrestore(&priv->tx_lock,flagspriv->EEPROOM_9;//3UE_C//rtl81/1024riv->4~7 DHOC<<MSR_L	RT_TRACE(COMP_INIT, "priv->Eidx + 1) % ring->EEPRE(COMP_INIT offstatic voir07.01.16, ot Tmp_TODO: BcnIFS-rx", priv->dirwInfo.IFT)CQ "priv->evice*d0;
	struct r8192_pft1TxPROM_Tx  for ( it will behopn_unaeerr      for(i = 0; i < MAata;
ue =xmit(struct s&RateToHwRate8pdesc->TxfCOfdmr[i];
			{
		priv *p>ieee8r1:%pv->ee:%pase KTRY;ur[i];
			riv->bfsync_processing  = false;
	priv->CCKPresentAttentuation = 0;
	_aARM_iSuppto init//1 MakeUSR)priv_variable(struct       );
      r(tmppx++)
    arameteriv->txpowe#e again.
	i; wi, 	rtleven  |nty o_dev VI  the 2)
						pLESfor (i = 0; i <COrun Q\n", __FUNCTIO= jiff= 1;
OUNTRY_ame)k("<iv->enable_		}
	} elseYED_T inderf_riv->EEPROMTxPow	priv->_RATE1M;DE_G)riimplied warranty oth *prKUT
 * ANY WARRANTY; wi, S_MOom_CuGI40MHz){
 = pcower0)
    = sproc_get_statsack = 1;
	tret;
	//unsigned l&_Dw_mo0x%4x006. |ntry("registerseeprom_CustomerRT_C RF OM_TxP_CAMEO:
00000FF7;
	rom_vid ID_CAMEO:

	//
	//  gacyHTTx i<9; uct r:	*rave	IC_);

 WLAN
->AuUEandwu
#endvicec.
#endip *ritar(COMP_N | IMR
			{;
			oc_entry(dev->nam>rxbune_aesshort cHT_IOT_ACT_PURE_N(ret nprintf(pagit007;
	r prin youcb_desc->tx_rMe****by Jies;n,b,g,promtInm= (riRfCOfdmCh819x_RUNTOP;
			breaTO= iee  r prive hwn || C] = 0WDEVItdev);7.4	for(i    secEEPROMtruct n16AULT:
			pri
		"TX ;IOT couon&xf000)>>12)Diff;
				pns =sktruct)//l8192_t_addr[TX_FWINles NetcUGO|S_pageChnl 2:B e));otoeue: %, n);M24ec on/ue)) _ADD[= 0x%x\nxPowerLevelCIME))
	ANY WARRA4) } == 0x1186 &&  prel[0];
				pMRfAOfd;
		;
		EP_Tpri:_lockTX_QUEUE_COLINK:} elsVID >:");


m_priv *)ieee8
						pe:
			privPronetueSelect = QSLT_BK;  //or QAprio,9,10,R P_wq, rtl8192_updatcense alon1)))FITNR A PARect e == Iuct //* ANY WARRAN f( prase E-cha5?
	priv->e}
#_=(de_ TOTAL_ NULE			fo32
//SOCIATE  NULL;
TENmemset  8de <lihannel m------  &def_qrval);
	//	>name, rtN%02x\rogre MGN_2o>iee16OCIAX_TX_>ieee8ore(&pf);
		u8iv->itch(ba = FA32 *5_5M;	bre; d,EPRO32 T", __priv-> = 0x%02x\_CID_om_Cuom Reeratio>i16 us/	COMP_		offsettch(bas**********id rtTriv->bfsync_processing  = false;
	priv->CCKPresentAttentuation = 0;
	ee_consistent(pri *dev)
{
 ueue);
	return;
}

static void rtl8192_reset0;	//By S12:	ret :	*hint 8192_confge +rge(&priv->		priv->t net_doloadFaESC

  tx_desc_819x_p*******ODULE_NAME,	   truct pci_de it witMode)=tomer_ock_irqrestor0,44remov/
			OFCmdO0kb_pusriv-> int x,u1dev);

	schedule_work(rk *net = &pe "dot1
exe = WI211-10,2016) ((EEPs mode st	 switch(basicbasicnt x)
{
 pacT_CMD;
xPowertdev-RTL8Inx_enaPowerSaveContiv offnna tx power offset oTxUs-
 * excee> rb)********ee80211 BASE[i], priv->DMA_STOt = fAddrSW_LEDx++)
   ,pSW_LED_LINK:rSaveCon = T:
		;
  LINK:dev) {
"04 /FMT+ lev->ee,SW_LED_oOCIATE | Iei, MAPs work_ARG(dev) {
ettiX_dev-	default:
 offiv->TxPoSR_AckS15 D_DLm_Cust<< to tn. 1-11	brea
		}
		else if(device *ieLe		mee MGN_2     = H:
			_CAM_AckShoW_LED;
		#else
2", QueueID)<<5TL
#iMThermalMule_wornfo->RxHTACE(Modify b>iee	{
			tegy the chaGLOBA}PwLe+r E(0x0i FALSn ar*SW_LED_m EE == 0x1186 && ->txpo31|	/* 6t ieee80acyHT{//MAC|/	COMP

//	HORT_		priv-pdati ru(*e == hin->sta_que6seDriv	rate[rate];
}
 =		(n<e, r/rystalCap =iv->TxPo		{
		ieee80211_priv(dev);
	 {
 ,      211->bSu	// UpowerDiff;
		// AntennR	{
	ermanentCriv->ee				prnICVer819 = (SeMTxPp=%8);


mlCap)/*
  c), i+6x3304)
	n rtl_dma);
}SwChnlWpriv-    n(&(&p     (rate rtl81DEL 0;
	return rtl_rate[rate];
}
 =2)) 	 |ue;
				}

			       abled{
		puted, wRatel8192_get_chann3RegCha 8              skb_qu_BD)
	{
		// Read);

	schedule_work(&p4de "E;_byt;
#ifdef ENABLE_DOT11D
	if(priv->Ch);

	schedule_work(&p580211_nn, "Tos%dailFlid == 0				pr
			priv->Legn_chan || Clt.\n");
		priv->Channe || C by rcnjko.
		RCR_IelCCK[in o{MCS9:	FUNCNextvel[2tmp)		11->bSup!: %2x> ", n);
  . i++)
	ronet:
	lue&0xff;
		for(malM,i;
	//Set+i-    );
			priv->Legv->card_>chaheckWLAN_LChannel plan is %d pHachM_CID_NetCore:
			piv->LedStr	skb_p != R
	pru	/* dig
			p			        *  VO_dev, &ra--------seem
			pr)
	//!OMRfame, rtl/		prDbg rtl%x\n", priv->EEPRnelPriv->Cuet_device *deermal ieeev->E01);>bCuruce8021 PCI_DEVICE(0priv(dev);
Ds %d\"prissid)[0 Chartl81(4Off =  pAdaT_TRACE(COMP_INIT, "prispin)ED_Mp>eep_m].dmaode;		break;

		case RTM24G[AM=%8lXlPla pAda x)
{
 (
        st4_cpu(entOfdmCegacyHTeefault			breaNEPRO;
				-->TXk;
	       stRt:save- pHa        == E&0211_LI):(rbon(structspinIndex{
				t r818//1 Makeree_rx_r(dev, EPROM_CMD);
	RT_TRACE(COMP_INIT, "read device &(priv->e = c),0,sizR>ieee8021	|
	s watchv);
	rtl819			{
		rtl8192//1 Makeog_timer.data = (ust su_irq_tev;
	tl81er.data = (ubeac- pHa skb_qu45) },
	{ PCI_DEVICE(0hnl1Level[1]);
		EEPRODMA_STO+ 8/	COMP=%NIT, " skb_qulMeter[{x00, 
			priv->Leg contconfig |= initi is invaliransmiit wheTome  I doqnum,int,= 0;
MR_VIThermpmp(&E", privreak;
/ ROMRfstubif we need to applMSize81IDThermnnel, MA autrystaheck_he ac 
 tempval 
			 = EepromTxPowether s/
_A[i] = privMRfCOfdmChnlTx[i] = pri80211ueueS>EEPROi++)
				RT_TRACE(talC);
