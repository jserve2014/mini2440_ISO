/*
   This is part of rtl818x pci OpenSource driver - v 0.1
   Copyright (C) Andrea Merello 2004-2005  <andreamrl@tiscali.it>
   Released under the terms of GPL (General Public License)

   Parts of this driver are based on the GPL part of the official
   Realtek driver.

   Parts of this driver are based on the rtl8180 driver skeleton
   from Patric Schenke & Andres Salomon.

   Parts of this driver are based on the Intel Pro Wireless 2100 GPL driver.

   Parts of BB/RF code are derived from David Young rtl8180 netbsd driver.

   RSSI calc function from 'The Deuce'

   Some ideas borrowed from the 8139too.c driver included in linux kernel.

   We (I?) want to thanks the Authors of those projecs and also the
   Ndiswrapper's project Authors.

   A big big thanks goes also to Realtek corp. for their help in my attempt to
   add RTL8185 and RTL8225 support, and to David Young also.

   Power management interface routines.
   Written by Mariusz Matuszek.
*/

#undef RX_DONT_PASS_UL
#undef DUMMY_RX

#include <linux/syscalls.h>

#include "r8180_hw.h"
#include "r8180.h"
#include "r8180_rtl8225.h" /* RTL8225 Radio frontend */
#include "r8180_93cx6.h"   /* Card EEPROM */
#include "r8180_wx.h"
#include "r8180_dm.h"

#include "ieee80211/dot11d.h"

#ifndef PCI_VENDOR_ID_BELKIN
	#define PCI_VENDOR_ID_BELKIN 0x1799
#endif
#ifndef PCI_VENDOR_ID_DLINK
	#define PCI_VENDOR_ID_DLINK 0x1186
#endif

static struct pci_device_id rtl8180_pci_id_tbl[] __devinitdata = {
        {
                .vendor = PCI_VENDOR_ID_REALTEK,
                .device = 0x8199,
                .subvendor = PCI_ANY_ID,
                .subdevice = PCI_ANY_ID,
                .driver_data = 0,
        },
        {
                .vendor = 0,
                .device = 0,
                .subvendor = 0,
                .subdevice = 0,
                .driver_data = 0,
        }
};


static char* ifname = "wlan%d";
static int hwseqnum = 0;
static int hwwep = 0;
static int channels = 0x3fff;

#define eqMacAddr(a,b)		( ((a)[0]==(b)[0] && (a)[1]==(b)[1] && (a)[2]==(b)[2] && (a)[3]==(b)[3] && (a)[4]==(b)[4] && (a)[5]==(b)[5]) ? 1:0 )
#define cpMacAddr(des,src)	      ((des)[0]=(src)[0],(des)[1]=(src)[1],(des)[2]=(src)[2],(des)[3]=(src)[3],(des)[4]=(src)[4],(des)[5]=(src)[5])
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, rtl8180_pci_id_tbl);
MODULE_AUTHOR("Andrea Merello <andreamrl@tiscali.it>");
MODULE_DESCRIPTION("Linux driver for Realtek RTL8180 / RTL8185 WiFi cards");


module_param(ifname, charp, S_IRUGO|S_IWUSR );
module_param(hwseqnum,int, S_IRUGO|S_IWUSR);
module_param(hwwep,int, S_IRUGO|S_IWUSR);
module_param(channels,int, S_IRUGO|S_IWUSR);

MODULE_PARM_DESC(devname," Net interface name, wlan%d=default");
MODULE_PARM_DESC(hwseqnum," Try to use hardware 802.11 header sequence numbers. Zero=default");
MODULE_PARM_DESC(hwwep," Try to use hardware WEP support. Still broken and not available on all cards");
MODULE_PARM_DESC(channels," Channel bitmask for specific locales. NYI");


static int __devinit rtl8180_pci_probe(struct pci_dev *pdev,
				       const struct pci_device_id *id);

static void __devexit rtl8180_pci_remove(struct pci_dev *pdev);

static void rtl8180_shutdown (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	if (dev->netdev_ops->ndo_stop)
		dev->netdev_ops->ndo_stop(dev);
	pci_disable_device(pdev);
}

static int rtl8180_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *dev = pci_get_drvdata(pdev);

	if (!netif_running(dev))
		goto out_pci_suspend;

	if (dev->netdev_ops->ndo_stop)
		dev->netdev_ops->ndo_stop(dev);

	netif_device_detach(dev);

out_pci_suspend:
	pci_save_state(pdev);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));
	return 0;
}

static int rtl8180_resume(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	int err;
	u32 val;

	pci_set_power_state(pdev, PCI_D0);

	err = pci_enable_device(pdev);
	if (err) {
		printk(KERN_ERR "%s: pci_enable_device failed on resume\n",
				dev->name);

		return err;
	}

	pci_restore_state(pdev);

	/*
	 * Suspend/Resume resets the PCI configuration space, so we have to
	 * re-disable the RETRY_TIMEOUT register (0x41) to keep PCI Tx retries
	 * from interfering with C3 CPU state. pci_restore_state won't help
	 * here since it only restores the first 64 bytes pci config header.
	 */
	pci_read_config_dword(pdev, 0x40, &val);
	if ((val & 0x0000ff00) != 0)
		pci_write_config_dword(pdev, 0x40, val & 0xffff00ff);

	if (!netif_running(dev))
		goto out;

	if (dev->netdev_ops->ndo_open)
		dev->netdev_ops->ndo_open(dev);

	netif_device_attach(dev);
out:
	return 0;
}

static struct pci_driver rtl8180_pci_driver = {
	.name		= RTL8180_MODULE_NAME,
	.id_table	= rtl8180_pci_id_tbl,
	.probe		= rtl8180_pci_probe,
	.remove		= __devexit_p(rtl8180_pci_remove),
	.suspend	= rtl8180_suspend,
	.resume		= rtl8180_resume,
	.shutdown	= rtl8180_shutdown,
};

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

inline void force_pci_posting(struct net_device *dev)
{
	read_nic_byte(dev,EPROM_CMD);
	mb();
}

irqreturn_t rtl8180_interrupt(int irq, void *netdev, struct pt_regs *regs);
void set_nic_rxring(struct net_device *dev);
void set_nic_txring(struct net_device *dev);
static struct net_device_stats *rtl8180_stats(struct net_device *dev);
void rtl8180_commit(struct net_device *dev);
void rtl8180_start_tx_beacon(struct net_device *dev);

static struct proc_dir_entry *rtl8180_proc = NULL;

static int proc_get_registers(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
	int len = 0;
	int i,n;
	int max = 0xff;

	/* This dump the current register page */
	for (n = 0; n <= max;) {
		len += snprintf(page + len, count - len, "\nD:  %2x > ", n);

		for (i = 0; i < 16 && n <= max; i++, n++)
			len += snprintf(page + len, count - len, "%2x ",
					read_nic_byte(dev, n));
	}
	len += snprintf(page + len, count - len,"\n");

	*eof = 1;
	return len;
}

int get_curr_tx_free_desc(struct net_device *dev, int priority);

static int proc_get_stats_hw(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	int len = 0;

	*eof = 1;
	return len;
}

static int proc_get_stats_rx(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	int len = 0;

	len += snprintf(page + len, count - len,
		"RX OK: %lu\n"
		"RX Retry: %lu\n"
		"RX CRC Error(0-500): %lu\n"
		"RX CRC Error(500-1000): %lu\n"
		"RX CRC Error(>1000): %lu\n"
		"RX ICV Error: %lu\n",
		priv->stats.rxint,
		priv->stats.rxerr,
		priv->stats.rxcrcerrmin,
		priv->stats.rxcrcerrmid,
		priv->stats.rxcrcerrmax,
		priv->stats.rxicverr
		);

	*eof = 1;
	return len;
}

static int proc_get_stats_tx(char *page, char **start,
			  off_t offset, int count,
			  int *eof, void *data)
{
	struct net_device *dev = data;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	int len = 0;
	unsigned long totalOK;

	totalOK=priv->stats.txnpokint+priv->stats.txhpokint+priv->stats.txlpokint;
	len += snprintf(page + len, count - len,
		"TX OK: %lu\n"
		"TX Error: %lu\n"
		"TX Retry: %lu\n"
		"TX beacon OK: %lu\n"
		"TX beacon error: %lu\n",
		totalOK,
		priv->stats.txnperr+priv->stats.txhperr+priv->stats.txlperr,
		priv->stats.txretry,
		priv->stats.txbeacon,
		priv->stats.txbeaconerr
	);

	*eof = 1;
	return len;
}

void rtl8180_proc_module_init(void)
{
	DMESG("Initializing proc filesystem");
        rtl8180_proc=create_proc_entry(RTL8180_MODULE_NAME, S_IFDIR, init_net.proc_net);
}

void rtl8180_proc_module_remove(void)
{
        remove_proc_entry(RTL8180_MODULE_NAME, init_net.proc_net);
}

void rtl8180_proc_remove_one(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	if (priv->dir_dev) {
		remove_proc_entry("stats-hw", priv->dir_dev);
		remove_proc_entry("stats-tx", priv->dir_dev);
		remove_proc_entry("stats-rx", priv->dir_dev);
		remove_proc_entry("registers", priv->dir_dev);
		remove_proc_entry(dev->name, rtl8180_proc);
		priv->dir_dev = NULL;
	}
}

void rtl8180_proc_init_one(struct net_device *dev)
{
	struct proc_dir_entry *e;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	priv->dir_dev = rtl8180_proc;
	if (!priv->dir_dev) {
		DMESGE("Unable to initialize /proc/net/r8180/%s\n",
		      dev->name);
		return;
	}

	e = create_proc_read_entry("stats-hw", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_stats_hw, dev);
	if (!e) {
		DMESGE("Unable to initialize "
		      "/proc/net/r8180/%s/stats-hw\n",
		      dev->name);
	}

	e = create_proc_read_entry("stats-rx", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_stats_rx, dev);
	if (!e) {
		DMESGE("Unable to initialize "
		      "/proc/net/r8180/%s/stats-rx\n",
		      dev->name);
	}


	e = create_proc_read_entry("stats-tx", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_stats_tx, dev);
	if (!e) {
		DMESGE("Unable to initialize "
		      "/proc/net/r8180/%s/stats-tx\n",
		      dev->name);
	}

	e = create_proc_read_entry("registers", S_IFREG | S_IRUGO,
				   priv->dir_dev, proc_get_registers, dev);
	if (!e) {
		DMESGE("Unable to initialize "
		      "/proc/net/r8180/%s/registers\n",
		      dev->name);
	}
}

/*
  FIXME: check if we can use some standard already-existent
  data type+functions in kernel
*/

short buffer_add(struct buffer **buffer, u32 *buf, dma_addr_t dma,
		struct buffer **bufferhead)
{
        struct buffer *tmp;

	if(! *buffer){

		*buffer = kmalloc(sizeof(struct buffer),GFP_KERNEL);

		if (*buffer == NULL) {
			DMESGE("Failed to kmalloc head of TX/RX struct");
			return -1;
		}
		(*buffer)->next=*buffer;
		(*buffer)->buf=buf;
		(*buffer)->dma=dma;
		if(bufferhead !=NULL)
			(*bufferhead) = (*buffer);
		return 0;
	}
	tmp=*buffer;

	while(tmp->next!=(*buffer)) tmp=tmp->next;
	if ((tmp->next= kmalloc(sizeof(struct buffer),GFP_KERNEL)) == NULL){
		DMESGE("Failed to kmalloc TX/RX struct");
		return -1;
	}
	tmp->next->buf=buf;
	tmp->next->dma=dma;
	tmp->next->next=*buffer;

	return 0;
}

void buffer_free(struct net_device *dev,struct buffer **buffer,int len,short
consistent)
{

	struct buffer *tmp,*next;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct pci_dev *pdev=priv->pdev;

	if (!*buffer)
		return;

	tmp = *buffer;

	do{
		next=tmp->next;
		if(consistent){
			pci_free_consistent(pdev,len,
				    tmp->buf,tmp->dma);
		}else{
			pci_unmap_single(pdev, tmp->dma,
			len,PCI_DMA_FROMDEVICE);
			kfree(tmp->buf);
		}
		kfree(tmp);
		tmp = next;
	}
	while(next != *buffer);

	*buffer=NULL;
}

void print_buffer(u32 *buffer, int len)
{
	int i;
	u8 *buf =(u8*)buffer;

	printk("ASCII BUFFER DUMP (len: %x):\n",len);

	for(i=0;i<len;i++)
		printk("%c",buf[i]);

	printk("\nBINARY BUFFER DUMP (len: %x):\n",len);

	for(i=0;i<len;i++)
		printk("%02x",buf[i]);

	printk("\n");
}

int get_curr_tx_free_desc(struct net_device *dev, int priority)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u32* tail;
	u32* head;
	int ret;

	switch (priority){
		case MANAGE_PRIORITY:
			head = priv->txmapringhead;
			tail = priv->txmapringtail;
			break;
		case BK_PRIORITY:
			head = priv->txbkpringhead;
			tail = priv->txbkpringtail;
			break;
		case BE_PRIORITY:
			head = priv->txbepringhead;
			tail = priv->txbepringtail;
			break;
		case VI_PRIORITY:
			head = priv->txvipringhead;
			tail = priv->txvipringtail;
			break;
		case VO_PRIORITY:
			head = priv->txvopringhead;
			tail = priv->txvopringtail;
			break;
		case HI_PRIORITY:
			head = priv->txhpringhead;
			tail = priv->txhpringtail;
			break;
		default:
			return -1;
	}

	if (head <= tail)
		ret = priv->txringcount - (tail - head)/8;
	else
		ret = (head - tail)/8;

	if (ret > priv->txringcount)
		DMESG("BUG");

	return ret;
}

short check_nic_enought_desc(struct net_device *dev, int priority)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct ieee80211_device *ieee = netdev_priv(dev);
	int requiredbyte, required;

	requiredbyte = priv->ieee80211->fts + sizeof(struct ieee80211_header_data);

	if (ieee->current_network.QoS_Enable)
		requiredbyte += 2;

	required = requiredbyte / (priv->txbuffsize-4);

	if (requiredbyte % priv->txbuffsize)
		required++;

	/* for now we keep two free descriptor as a safety boundary
	 * between the tail and the head
	 */

	return (required+2 < get_curr_tx_free_desc(dev,priority));
}

void fix_tx_fifo(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	u32 *tmp;
	int i;

	for (tmp=priv->txmapring, i=0;
	     i < priv->txringcount;
	     tmp+=8, i++){
		*tmp = *tmp &~ (1<<31);
	}

	for (tmp=priv->txbkpring, i=0;
	     i < priv->txringcount;
	     tmp+=8, i++) {
		*tmp = *tmp &~ (1<<31);
	}

	for (tmp=priv->txbepring, i=0;
	     i < priv->txringcount;
	     tmp+=8, i++){
		*tmp = *tmp &~ (1<<31);
	}
	for (tmp=priv->txvipring, i=0;
	     i < priv->txringcount;
	     tmp+=8, i++) {
		*tmp = *tmp &~ (1<<31);
	}

	for (tmp=priv->txvopring, i=0;
	     i < priv->txringcount;
	     tmp+=8, i++){
		*tmp = *tmp &~ (1<<31);
	}

	for (tmp=priv->txhpring, i=0;
	     i < priv->txringcount;
	     tmp+=8,i++){
		*tmp = *tmp &~ (1<<31);
	}

	for (tmp=priv->txbeaconring, i=0;
	     i < priv->txbeaconcount;
	     tmp+=8, i++){
		*tmp = *tmp &~ (1<<31);
	}

	priv->txmapringtail = priv->txmapring;
	priv->txmapringhead = priv->txmapring;
	priv->txmapbufstail = priv->txmapbufs;

	priv->txbkpringtail = priv->txbkpring;
	priv->txbkpringhead = priv->txbkpring;
	priv->txbkpbufstail = priv->txbkpbufs;

	priv->txbepringtail = priv->txbepring;
	priv->txbepringhead = priv->txbepring;
	priv->txbepbufstail = priv->txbepbufs;

	priv->txvipringtail = priv->txvipring;
	priv->txvipringhead = priv->txvipring;
	priv->txvipbufstail = priv->txvipbufs;

	priv->txvopringtail = priv->txvopring;
	priv->txvopringhead = priv->txvopring;
	priv->txvopbufstail = priv->txvopbufs;

	priv->txhpringtail = priv->txhpring;
	priv->txhpringhead = priv->txhpring;
	priv->txhpbufstail = priv->txhpbufs;

	priv->txbeaconringtail = priv->txbeaconring;
	priv->txbeaconbufstail = priv->txbeaconbufs;
	set_nic_txring(dev);

	ieee80211_reset_queue(priv->ieee80211);
	priv->ack_tx_to_ieee = 0;
}

void fix_rx_fifo(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	u32 *tmp;
	struct buffer *rxbuf;
	u8 rx_desc_size;

	rx_desc_size = 8; // 4*8 = 32 bytes

	for (tmp=priv->rxring, rxbuf=priv->rxbufferhead;
	     (tmp < (priv->rxring)+(priv->rxringcount)*rx_desc_size);
	     tmp+=rx_desc_size,rxbuf=rxbuf->next){
		*(tmp+2) = rxbuf->dma;
		*tmp=*tmp &~ 0xfff;
		*tmp=*tmp | priv->rxbuffersize;
		*tmp |= (1<<31);
	}

	priv->rxringtail=priv->rxring;
	priv->rxbuffer=priv->rxbufferhead;
	priv->rx_skb_complete=1;
	set_nic_rxring(dev);
}

unsigned char QUALITY_MAP[] = {
	0x64, 0x64, 0x64, 0x63, 0x63, 0x62, 0x62, 0x61,
	0x61, 0x60, 0x60, 0x5f, 0x5f, 0x5e, 0x5d, 0x5c,
	0x5b, 0x5a, 0x59, 0x57, 0x56, 0x54, 0x52, 0x4f,
	0x4c, 0x49, 0x45, 0x41, 0x3c, 0x37, 0x31, 0x29,
	0x24, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
	0x22, 0x22, 0x21, 0x21, 0x21, 0x21, 0x21, 0x20,
	0x20, 0x20, 0x20, 0x1f, 0x1f, 0x1e, 0x1e, 0x1e,
	0x1d, 0x1d, 0x1c, 0x1c, 0x1b, 0x1a, 0x19, 0x19,
	0x18, 0x17, 0x16, 0x15, 0x14, 0x12, 0x11, 0x0f,
	0x0e, 0x0c, 0x0a, 0x08, 0x06, 0x04, 0x01, 0x00
};

unsigned char STRENGTH_MAP[] = {
	0x64, 0x64, 0x63, 0x62, 0x61, 0x60, 0x5f, 0x5e,
	0x5d, 0x5c, 0x5b, 0x5a, 0x57, 0x54, 0x52, 0x50,
	0x4e, 0x4c, 0x4a, 0x48, 0x46, 0x44, 0x41, 0x3f,
	0x3c, 0x3a, 0x37, 0x36, 0x36, 0x1c, 0x1c, 0x1b,
	0x1b, 0x1a, 0x1a, 0x19, 0x19, 0x18, 0x18, 0x17,
	0x17, 0x16, 0x16, 0x15, 0x15, 0x14, 0x14, 0x13,
	0x13, 0x12, 0x12, 0x11, 0x11, 0x10, 0x10, 0x0f,
	0x0f, 0x0e, 0x0e, 0x0d, 0x0d, 0x0c, 0x0c, 0x0b,
	0x0b, 0x0a, 0x0a, 0x09, 0x09, 0x08, 0x08, 0x07,
	0x07, 0x06, 0x06, 0x05, 0x04, 0x03, 0x02, 0x00
};

void rtl8180_RSSI_calc(struct net_device *dev, u8 *rssi, u8 *qual)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	u32 temp;
	u32 temp2;
	u32 temp3;
	u32 lsb;
	u32 q;
	u32 orig_qual;
	u8  _rssi;

	q = *qual;
	orig_qual = *qual;
	_rssi = 0; // avoid gcc complains..

	if (q <= 0x4e) {
		temp = QUALITY_MAP[q];
	} else {
		if( q & 0x80 ) {
			temp = 0x32;
		} else {
			temp = 1;
		}
	}

	*qual = temp;
	temp2 = *rssi;

	switch(priv->rf_chip){
	case RFCHIPID_RFMD:
		lsb = temp2 & 1;
		temp2 &= 0x7e;
		if ( !lsb || !(temp2 <= 0x3c) ) {
			temp2 = 0x64;
		} else {
			temp2 = 100 * temp2 / 0x3c;
		}
		*rssi = temp2 & 0xff;
		_rssi = temp2 & 0xff;
		break;
	case RFCHIPID_INTERSIL:
		lsb = temp2;
		temp2 &= 0xfffffffe;
		temp2 *= 251;
		temp3 = temp2;
		temp2 <<= 6;
		temp3 += temp2;
		temp3 <<= 1;
		temp2 = 0x4950df;
		temp2 -= temp3;
		lsb &= 1;
		if ( temp2 <= 0x3e0000 ) {
			if ( temp2 < 0xffef0000 )
				temp2 = 0xffef0000;
		} else {
			temp2 = 0x3e0000;
		}
		if ( !lsb ) {
			temp2 -= 0xf0000;
		} else {
			temp2 += 0xf0000;
		}

		temp3 = 0x4d0000;
		temp3 -= temp2;
		temp3 *= 100;
		temp3 = temp3 / 0x6d;
		temp3 >>= 0x10;
		_rssi = temp3 & 0xff;
		*rssi = temp3 & 0xff;
		break;
	case RFCHIPID_GCT:
	        lsb = temp2 & 1;
		temp2 &= 0x7e;
		if ( ! lsb || !(temp2 <= 0x3c) ){
			temp2 = 0x64;
		} else {
			temp2 = (100 * temp2) / 0x3c;
		}
		*rssi = temp2 & 0xff;
		_rssi = temp2 & 0xff;
		break;
	case RFCHIPID_PHILIPS:
		if( orig_qual <= 0x4e ){
			_rssi = STRENGTH_MAP[orig_qual];
			*rssi = _rssi;
		} else {
			orig_qual -= 0x80;
			if ( !orig_qual ){
				_rssi = 1;
				*rssi = 1;
			} else {
				_rssi = 0x32;
				*rssi = 0x32;
			}
		}
		break;
	case RFCHIPID_MAXIM:
		lsb = temp2 & 1;
		temp2 &= 0x7e;
		temp2 >>= 1;
		temp2 += 0x42;
		if( lsb != 0 ){
			temp2 += 0xa;
		}
		*rssi = temp2 & 0xff;
		_rssi = temp2 & 0xff;
		break;
	}

	if ( _rssi < 0x64 ){
		if ( _rssi == 0 ) {
			*rssi = 1;
		}
	} else {
		*rssi = 0x64;
	}

	return;
}

void rtl8180_irq_enable(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	priv->irq_enabled = 1;
	write_nic_word(dev,INTA_MASK, priv->irq_mask);
}

void rtl8180_irq_disable(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	write_nic_dword(dev,IMR,0);
	force_pci_posting(dev);
	priv->irq_enabled = 0;
}

void rtl8180_set_mode(struct net_device *dev,int mode)
{
	u8 ecmd;

	ecmd=read_nic_byte(dev, EPROM_CMD);
	ecmd=ecmd &~ EPROM_CMD_OPERATING_MODE_MASK;
	ecmd=ecmd | (mode<<EPROM_CMD_OPERATING_MODE_SHIFT);
	ecmd=ecmd &~ (1<<EPROM_CS_SHIFT);
	ecmd=ecmd &~ (1<<EPROM_CK_SHIFT);
	write_nic_byte(dev, EPROM_CMD, ecmd);
}

void rtl8180_adapter_start(struct net_device *dev);
void rtl8180_beacon_tx_enable(struct net_device *dev);

void rtl8180_update_msr(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u8 msr;
	u32 rxconf;

	msr  = read_nic_byte(dev, MSR);
	msr &= ~ MSR_LINK_MASK;

	rxconf=read_nic_dword(dev,RX_CONF);

	if(priv->ieee80211->state == IEEE80211_LINKED)
	{
		if(priv->ieee80211->iw_mode == IW_MODE_ADHOC)
			msr |= (MSR_LINK_ADHOC<<MSR_LINK_SHIFT);
		else if (priv->ieee80211->iw_mode == IW_MODE_MASTER)
			msr |= (MSR_LINK_MASTER<<MSR_LINK_SHIFT);
		else if (priv->ieee80211->iw_mode == IW_MODE_INFRA)
			msr |= (MSR_LINK_MANAGED<<MSR_LINK_SHIFT);
		else
			msr |= (MSR_LINK_NONE<<MSR_LINK_SHIFT);
		rxconf |= (1<<RX_CHECK_BSSID_SHIFT);

	}else {
		msr |= (MSR_LINK_NONE<<MSR_LINK_SHIFT);
		rxconf &= ~(1<<RX_CHECK_BSSID_SHIFT);
	}

	write_nic_byte(dev, MSR, msr);
	write_nic_dword(dev, RX_CONF, rxconf);
}

void rtl8180_set_chan(struct net_device *dev,short ch)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	if ((ch > 14) || (ch < 1)) {
		printk("In %s: Invalid chnanel %d\n", __func__, ch);
		return;
	}

	priv->chan=ch;
	priv->rf_set_chan(dev,priv->chan);
}

void rtl8180_rx_enable(struct net_device *dev)
{
	u8 cmd;
	u32 rxconf;
	/* for now we accept data, management & ctl frame*/
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	rxconf=read_nic_dword(dev,RX_CONF);
	rxconf = rxconf &~ MAC_FILTER_MASK;
	rxconf = rxconf | (1<<ACCEPT_MNG_FRAME_SHIFT);
	rxconf = rxconf | (1<<ACCEPT_DATA_FRAME_SHIFT);
	rxconf = rxconf | (1<<ACCEPT_BCAST_FRAME_SHIFT);
	rxconf = rxconf | (1<<ACCEPT_MCAST_FRAME_SHIFT);
	if (dev->flags & IFF_PROMISC)
		DMESG("NIC in promisc mode");

	if(priv->ieee80211->iw_mode == IW_MODE_MONITOR || \
	   dev->flags & IFF_PROMISC){
		rxconf = rxconf | (1<<ACCEPT_ALLMAC_FRAME_SHIFT);
	}else{
		rxconf = rxconf | (1<<ACCEPT_NICMAC_FRAME_SHIFT);
		if(priv->card_8185 == 0)
			rxconf = rxconf | (1<<RX_CHECK_BSSID_SHIFT);
	}

	if(priv->ieee80211->iw_mode == IW_MODE_MONITOR){
		rxconf = rxconf | (1<<ACCEPT_CTL_FRAME_SHIFT);
		rxconf = rxconf | (1<<ACCEPT_ICVERR_FRAME_SHIFT);
		rxconf = rxconf | (1<<ACCEPT_PWR_FRAME_SHIFT);
	}

	if( priv->crcmon == 1 && priv->ieee80211->iw_mode == IW_MODE_MONITOR)
		rxconf = rxconf | (1<<ACCEPT_CRCERR_FRAME_SHIFT);

	rxconf = rxconf & ~RX_FIFO_THRESHOLD_MASK;
	rxconf = rxconf | (RX_FIFO_THRESHOLD_NONE << RX_FIFO_THRESHOLD_SHIFT);

	rxconf = rxconf | (1<<RX_AUTORESETPHY_SHIFT);
	rxconf = rxconf &~ MAX_RX_DMA_MASK;
	rxconf = rxconf | (MAX_RX_DMA_2048<<MAX_RX_DMA_SHIFT);

	rxconf = rxconf | RCR_ONLYERLPKT;

	rxconf = rxconf &~ RCR_CS_MASK;

	if (!priv->card_8185)
		rxconf |= (priv->rcr_csense<<RCR_CS_SHIFT);

	write_nic_dword(dev, RX_CONF, rxconf);

	fix_rx_fifo(dev);

	cmd=read_nic_byte(dev,CMD);
	write_nic_byte(dev,CMD,cmd | (1<<CMD_RX_ENABLE_SHIFT));
}

void set_nic_txring(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	write_nic_dword(dev, TX_MANAGEPRIORITY_RING_ADDR, priv->txmapringdma);
	write_nic_dword(dev, TX_BKPRIORITY_RING_ADDR, priv->txbkpringdma);
	write_nic_dword(dev, TX_BEPRIORITY_RING_ADDR, priv->txbepringdma);
	write_nic_dword(dev, TX_VIPRIORITY_RING_ADDR, priv->txvipringdma);
	write_nic_dword(dev, TX_VOPRIORITY_RING_ADDR, priv->txvopringdma);
	write_nic_dword(dev, TX_HIGHPRIORITY_RING_ADDR, priv->txhpringdma);
	write_nic_dword(dev, TX_BEACON_RING_ADDR, priv->txbeaconringdma);
}

void rtl8180_conttx_enable(struct net_device *dev)
{
	u32 txconf;

	txconf = read_nic_dword(dev,TX_CONF);
	txconf = txconf &~ TX_LOOPBACK_MASK;
	txconf = txconf | (TX_LOOPBACK_CONTINUE <<TX_LOOPBACK_SHIFT);
	write_nic_dword(dev,TX_CONF,txconf);
}

void rtl8180_conttx_disable(struct net_device *dev)
{
	u32 txconf;

	txconf = read_nic_dword(dev,TX_CONF);
	txconf = txconf &~ TX_LOOPBACK_MASK;
	txconf = txconf | (TX_LOOPBACK_NONE <<TX_LOOPBACK_SHIFT);
	write_nic_dword(dev,TX_CONF,txconf);
}

void rtl8180_tx_enable(struct net_device *dev)
{
	u8 cmd;
	u8 tx_agc_ctl;
	u8 byte;
	u32 txconf;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	txconf = read_nic_dword(dev, TX_CONF);

	if (priv->card_8185) {
		byte = read_nic_byte(dev,CW_CONF);
		byte &= ~(1<<CW_CONF_PERPACKET_CW_SHIFT);
		byte &= ~(1<<CW_CONF_PERPACKET_RETRY_SHIFT);
		write_nic_byte(dev, CW_CONF, byte);

		tx_agc_ctl = read_nic_byte(dev, TX_AGC_CTL);
		tx_agc_ctl &= ~(1<<TX_AGC_CTL_PERPACKET_GAIN_SHIFT);
		tx_agc_ctl &= ~(1<<TX_AGC_CTL_PERPACKET_ANTSEL_SHIFT);
		tx_agc_ctl |=(1<<TX_AGC_CTL_FEEDBACK_ANT);
		write_nic_byte(dev, TX_AGC_CTL, tx_agc_ctl);
		write_nic_byte(dev, 0xec, 0x3f); /* Disable early TX */
	}

	if (priv->card_8185)
		txconf = txconf &~ (1<<TCR_PROBE_NOTIMESTAMP_SHIFT);
	else {
		if(hwseqnum)
			txconf= txconf &~ (1<<TX_CONF_HEADER_AUTOICREMENT_SHIFT);
		else
			txconf= txconf | (1<<TX_CONF_HEADER_AUTOICREMENT_SHIFT);
	}

	txconf = txconf &~ TX_LOOPBACK_MASK;
	txconf = txconf | (TX_LOOPBACK_NONE <<TX_LOOPBACK_SHIFT);
	txconf = txconf &~ TCR_DPRETRY_MASK;
	txconf = txconf &~ TCR_RTSRETRY_MASK;
	txconf = txconf | (priv->retry_data<<TX_DPRETRY_SHIFT);
	txconf = txconf | (priv->retry_rts<<TX_RTSRETRY_SHIFT);
	txconf = txconf &~ (1<<TX_NOCRC_SHIFT);

	if (priv->card_8185) {
		if (priv->hw_plcp_len)
			txconf = txconf &~ TCR_PLCP_LEN;
		else
			txconf = txconf | TCR_PLCP_LEN;
	} else
		txconf = txconf &~ TCR_SAT;

	txconf = txconf &~ TCR_MXDMA_MASK;
	txconf = txconf | (TCR_MXDMA_2048<<TCR_MXDMA_SHIFT);
	txconf = txconf | TCR_CWMIN;
	txconf = txconf | TCR_DISCW;

	txconf = txconf | (1 << TX_NOICV_SHIFT);

	write_nic_dword(dev,TX_CONF,txconf);

	fix_tx_fifo(dev);

	cmd=read_nic_byte(dev,CMD);
	write_nic_byte(dev,CMD,cmd | (1<<CMD_TX_ENABLE_SHIFT));

	write_nic_dword(dev,TX_CONF,txconf);
}

void rtl8180_beacon_tx_enable(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	rtl8180_set_mode(dev,EPROM_CMD_CONFIG);
	priv->dma_poll_stop_mask &= ~(TPPOLLSTOP_BQ);
	write_nic_byte(dev,TPPollStop, priv->dma_poll_mask);
	rtl8180_set_mode(dev,EPROM_CMD_NORMAL);
}

void rtl8180_beacon_tx_disable(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	rtl8180_set_mode(dev,EPROM_CMD_CONFIG);
	priv->dma_poll_stop_mask |= TPPOLLSTOP_BQ;
	write_nic_byte(dev,TPPollStop, priv->dma_poll_stop_mask);
	rtl8180_set_mode(dev,EPROM_CMD_NORMAL);

}

void rtl8180_rtx_disable(struct net_device *dev)
{
	u8 cmd;
	struct r8180_priv *priv = ieee80211_priv(dev);

	cmd=read_nic_byte(dev,CMD);
	write_nic_byte(dev, CMD, cmd &~ \
		       ((1<<CMD_RX_ENABLE_SHIFT)|(1<<CMD_TX_ENABLE_SHIFT)));
	force_pci_posting(dev);
	mdelay(10);

	if(!priv->rx_skb_complete)
		dev_kfree_skb_any(priv->rx_skb);
}

short alloc_tx_desc_ring(struct net_device *dev, int bufsize, int count,
			 int addr)
{
	int i;
	u32 *desc;
	u32 *tmp;
	dma_addr_t dma_desc, dma_tmp;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct pci_dev *pdev = priv->pdev;
	void *buf;

	if((bufsize & 0xfff) != bufsize) {
		DMESGE ("TX buffer allocation too large");
		return 0;
	}
	desc = (u32*)pci_alloc_consistent(pdev,
					  sizeof(u32)*8*count+256, &dma_desc);
	if (desc == NULL)
		return -1;

	if (dma_desc & 0xff)
		/*
		 * descriptor's buffer must be 256 byte aligned
		 * we shouldn't be here, since we set DMA mask !
		 */
		WARN(1, "DMA buffer is not aligned\n");

	tmp = desc;

	for (i = 0; i < count; i++) {
		buf = (void *)pci_alloc_consistent(pdev, bufsize, &dma_tmp);
		if (buf == NULL)
			return -ENOMEM;

		switch(addr) {
		case TX_MANAGEPRIORITY_RING_ADDR:
			if(-1 == buffer_add(&(priv->txmapbufs),buf,dma_tmp,NULL)){
				DMESGE("Unable to allocate mem for buffer NP");
				return -ENOMEM;
			}
			break;
		case TX_BKPRIORITY_RING_ADDR:
			if(-1 == buffer_add(&(priv->txbkpbufs),buf,dma_tmp,NULL)){
				DMESGE("Unable to allocate mem for buffer LP");
				return -ENOMEM;
			}
			break;
		case TX_BEPRIORITY_RING_ADDR:
			if(-1 == buffer_add(&(priv->txbepbufs),buf,dma_tmp,NULL)){
				DMESGE("Unable to allocate mem for buffer NP");
				return -ENOMEM;
			}
			break;
		case TX_VIPRIORITY_RING_ADDR:
			if(-1 == buffer_add(&(priv->txvipbufs),buf,dma_tmp,NULL)){
				DMESGE("Unable to allocate mem for buffer LP");
				return -ENOMEM;
			}
			break;
		case TX_VOPRIORITY_RING_ADDR:
			if(-1 == buffer_add(&(priv->txvopbufs),buf,dma_tmp,NULL)){
				DMESGE("Unable to allocate mem for buffer NP");
				return -ENOMEM;
			}
			break;
		case TX_HIGHPRIORITY_RING_ADDR:
			if(-1 == buffer_add(&(priv->txhpbufs),buf,dma_tmp,NULL)){
				DMESGE("Unable to allocate mem for buffer HP");
				return -ENOMEM;
			}
			break;
		case TX_BEACON_RING_ADDR:
		        if(-1 == buffer_add(&(priv->txbeaconbufs),buf,dma_tmp,NULL)){
			DMESGE("Unable to allocate mem for buffer BP");
				return -ENOMEM;
			}
			break;
		}
		*tmp = *tmp &~ (1<<31); // descriptor empty, owned by the drv
		*(tmp+2) = (u32)dma_tmp;
		*(tmp+3) = bufsize;

		if(i+1<count)
			*(tmp+4) = (u32)dma_desc+((i+1)*8*4);
		else
			*(tmp+4) = (u32)dma_desc;

		tmp=tmp+8;
	}

	switch(addr) {
	case TX_MANAGEPRIORITY_RING_ADDR:
		priv->txmapringdma=dma_desc;
		priv->txmapring=desc;
		break;
	case TX_BKPRIORITY_RING_ADDR:
		priv->txbkpringdma=dma_desc;
		priv->txbkpring=desc;
		break;
	case TX_BEPRIORITY_RING_ADDR:
		priv->txbepringdma=dma_desc;
		priv->txbepring=desc;
		break;
	case TX_VIPRIORITY_RING_ADDR:
		priv->txvipringdma=dma_desc;
		priv->txvipring=desc;
		break;
	case TX_VOPRIORITY_RING_ADDR:
		priv->txvopringdma=dma_desc;
		priv->txvopring=desc;
		break;
	case TX_HIGHPRIORITY_RING_ADDR:
		priv->txhpringdma=dma_desc;
		priv->txhpring=desc;
		break;
	case TX_BEACON_RING_ADDR:
		priv->txbeaconringdma=dma_desc;
		priv->txbeaconring=desc;
		break;

	}

	return 0;
}

void free_tx_desc_rings(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct pci_dev *pdev=priv->pdev;
	int count = priv->txringcount;

	pci_free_consistent(pdev, sizeof(u32)*8*count+256,
			    priv->txmapring, priv->txmapringdma);
	buffer_free(dev,&(priv->txmapbufs),priv->txbuffsize,1);

	pci_free_consistent(pdev, sizeof(u32)*8*count+256,
			    priv->txbkpring, priv->txbkpringdma);
	buffer_free(dev,&(priv->txbkpbufs),priv->txbuffsize,1);

	pci_free_consistent(pdev, sizeof(u32)*8*count+256,
			    priv->txbepring, priv->txbepringdma);
	buffer_free(dev,&(priv->txbepbufs),priv->txbuffsize,1);

	pci_free_consistent(pdev, sizeof(u32)*8*count+256,
			    priv->txvipring, priv->txvipringdma);
	buffer_free(dev,&(priv->txvipbufs),priv->txbuffsize,1);

	pci_free_consistent(pdev, sizeof(u32)*8*count+256,
			    priv->txvopring, priv->txvopringdma);
	buffer_free(dev,&(priv->txvopbufs),priv->txbuffsize,1);

	pci_free_consistent(pdev, sizeof(u32)*8*count+256,
			    priv->txhpring, priv->txhpringdma);
	buffer_free(dev,&(priv->txhpbufs),priv->txbuffsize,1);

	count = priv->txbeaconcount;
	pci_free_consistent(pdev, sizeof(u32)*8*count+256,
			    priv->txbeaconring, priv->txbeaconringdma);
	buffer_free(dev,&(priv->txbeaconbufs),priv->txbuffsize,1);
}

void free_rx_desc_ring(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct pci_dev *pdev = priv->pdev;
	int count = priv->rxringcount;

	pci_free_consistent(pdev, sizeof(u32)*8*count+256,
			    priv->rxring, priv->rxringdma);

	buffer_free(dev,&(priv->rxbuffer),priv->rxbuffersize,0);
}

short alloc_rx_desc_ring(struct net_device *dev, u16 bufsize, int count)
{
	int i;
	u32 *desc;
	u32 *tmp;
	dma_addr_t dma_desc,dma_tmp;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct pci_dev *pdev=priv->pdev;
	void *buf;
	u8 rx_desc_size;

	rx_desc_size = 8; // 4*8 = 32 bytes

	if((bufsize & 0xfff) != bufsize){
		DMESGE ("RX buffer allocation too large");
		return -1;
	}

	desc = (u32*)pci_alloc_consistent(pdev,sizeof(u32)*rx_desc_size*count+256,
					  &dma_desc);

	if (dma_desc & 0xff)
		/*
		 * descriptor's buffer must be 256 byte aligned
		 * should never happen since we specify the DMA mask
		 */
		WARN(1, "DMA buffer is not aligned\n");

	priv->rxring=desc;
	priv->rxringdma=dma_desc;
	tmp=desc;

	for (i = 0; i < count; i++) {
		if ((buf= kmalloc(bufsize * sizeof(u8),GFP_ATOMIC)) == NULL){
			DMESGE("Failed to kmalloc RX buffer");
			return -1;
		}

		dma_tmp = pci_map_single(pdev,buf,bufsize * sizeof(u8),
					 PCI_DMA_FROMDEVICE);

		if(-1 == buffer_add(&(priv->rxbuffer), buf,dma_tmp,
			   &(priv->rxbufferhead))){
			   DMESGE("Unable to allocate mem RX buf");
			   return -1;
		}
		*tmp = 0; //zero pads the header of the descriptor
		*tmp = *tmp |( bufsize&0xfff);
		*(tmp+2) = (u32)dma_tmp;
		*tmp = *tmp |(1<<31); // descriptor void, owned by the NIC

		tmp=tmp+rx_desc_size;
	}

	*(tmp-rx_desc_size) = *(tmp-rx_desc_size) | (1<<30); // this is the last descriptor

	return 0;
}


void set_nic_rxring(struct net_device *dev)
{
	u8 pgreg;
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	pgreg=read_nic_byte(dev, PGSELECT);
	write_nic_byte(dev, PGSELECT, pgreg &~ (1<<PGSELECT_PG_SHIFT));

	write_nic_dword(dev, RXRING_ADDR,priv->rxringdma);
}

void rtl8180_reset(struct net_device *dev)
{
	u8 cr;

	rtl8180_irq_disable(dev);

	cr=read_nic_byte(dev,CMD);
	cr = cr & 2;
	cr = cr | (1<<CMD_RST_SHIFT);
	write_nic_byte(dev,CMD,cr);

	force_pci_posting(dev);

	mdelay(200);

	if(read_nic_byte(dev,CMD) & (1<<CMD_RST_SHIFT))
		DMESGW("Card reset timeout!");
	else
		DMESG("Card successfully reset");

	rtl8180_set_mode(dev,EPROM_CMD_LOAD);
	force_pci_posting(dev);
	mdelay(200);
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

static u16 rtl_rate[] = {10,20,55,110,60,90,120,180,240,360,480,540,720};

inline u16 rtl8180_rate2rate(short rate)
{
	if (rate > 12)
		return 10;
	return rtl_rate[rate];
}

inline u8 rtl8180_IsWirelessBMode(u16 rate)
{
	if( ((rate <= 110) && (rate != 60) && (rate != 90)) || (rate == 220) )
		return 1;
	else
		return 0;
}

u16 N_DBPSOfRate(u16 DataRate);

u16 ComputeTxTime(u16 FrameLength, u16 DataRate, u8 bManagementFrame,
		  u8 bShortPreamble)
{
	u16	FrameTime;
	u16	N_DBPS;
	u16	Ceiling;

	if (rtl8180_IsWirelessBMode(DataRate)) {
		if (bManagementFrame || !bShortPreamble || DataRate == 10)
			/* long preamble */
			FrameTime = (u16)(144+48+(FrameLength*8/(DataRate/10)));
		else
			/* short preamble */
			FrameTime = (u16)(72+24+(FrameLength*8/(DataRate/10)));

		if ((FrameLength*8 % (DataRate/10)) != 0) /* get the ceilling */
			FrameTime++;
	} else {	/* 802.11g DSSS-OFDM PLCP length field calculation. */
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

	switch (DataRate) {
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

//{by amy 080312
//
//	Description:
// 	For Netgear case, they want good-looking singal strength.
//		2004.12.05, by rcnjko.
//
long NetgearSignalStrengthTranslate(long LastSS, long CurrSS)
{
	long RetSS;

	// Step 1. Scale mapping.
	if (CurrSS >= 71 && CurrSS <= 100)
		RetSS = 90 + ((CurrSS - 70) / 3);
	else if (CurrSS >= 41 && CurrSS <= 70)
		RetSS = 78 + ((CurrSS - 40) / 3);
	else if (CurrSS >= 31 && CurrSS <= 40)
		RetSS = 66 + (CurrSS - 30);
	else if (CurrSS >= 21 && CurrSS <= 30)
		RetSS = 54 + (CurrSS - 20);
	else if (CurrSS >= 5 && CurrSS <= 20)
		RetSS = 42 + (((CurrSS - 5) * 2) / 3);
	else if (CurrSS == 4)
		RetSS = 36;
	else if (CurrSS == 3)
		RetSS = 27;
	else if (CurrSS == 2)
		RetSS = 18;
	else if (CurrSS == 1)
		RetSS = 9;
	else
		RetSS = CurrSS;

	// Step 2. Smoothing.
	if(LastSS > 0)
		RetSS = ((LastSS * 5) + (RetSS)+ 5) / 6;

	return RetSS;
}

//
//	Description:
//		Translate 0-100 signal strength index into dBm.
//
long TranslateToDbm8185(u8 SignalStrengthIndex)
{
	long SignalPower;

	// Translate to dBm (x=0.5y-95).
	SignalPower = (long)((SignalStrengthIndex + 1) >> 1);
	SignalPower -= 95;

	return SignalPower;
}

//
//	Description:
//		Perform signal smoothing for dynamic mechanism.
//		This is different with PerformSignalSmoothing8185 in smoothing fomula.
//		No dramatic adjustion is apply because dynamic mechanism need some degree
//		of correctness. Ported from 8187B.
//	2007-02-26, by Bruce.
//
void PerformUndecoratedSignalSmoothing8185(struct r8180_priv *priv,
					   bool bCckRate)
{
	// Determin the current packet is CCK rate.
	priv->bCurCCKPkt = bCckRate;

	if (priv->UndecoratedSmoothedSS >= 0)
		priv->UndecoratedSmoothedSS = ( (priv->UndecoratedSmoothedSS * 5) + (priv->SignalStrength * 10) ) / 6;
	else
		priv->UndecoratedSmoothedSS = priv->SignalStrength * 10;

	priv->UndercorateSmoothedRxPower = ( (priv->UndercorateSmoothedRxPower * 50) + (priv->RxPower* 11)) / 60;

	if (bCckRate)
		priv->CurCCKRSSI = priv->RSSI;
	else
		priv->CurCCKRSSI = 0;
}

//by amy 080312}

/* This is rough RX isr handling routine*/
void rtl8180_rx(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct sk_buff *tmp_skb;
	short first,last;
	u32 len;
	int lastlen;
	unsigned char quality, signal;
	u8 rate;
	u32 *tmp,*tmp2;
	u8 rx_desc_size;
	u8 padding;
	char rxpower = 0;
	u32 RXAGC = 0;
	long RxAGC_dBm = 0;
	u8	LNA=0, BB=0;
	u8 	LNA_gain[4]={02, 17, 29, 39};
	u8  Antenna = 0;
	struct ieee80211_hdr_4addr *hdr;
	u16 fc,type;
	u8 bHwError = 0,bCRC = 0,bICV = 0;
	bool	bCckRate = false;
	u8     RSSI = 0;
	long	SignalStrengthIndex = 0;
	struct ieee80211_rx_stats stats = {
		.signal = 0,
		.noise = -98,
		.rate = 0,
		.freq = IEEE80211_24GHZ_BAND,
	};

	stats.nic_type = NIC_8185B;
	rx_desc_size = 8;

	if ((*(priv->rxringtail)) & (1<<31)) {
		/* we have got an RX int, but the descriptor
		 * we are pointing is empty*/

		priv->stats.rxnodata++;
		priv->ieee80211->stats.rx_errors++;

		tmp2 = NULL;
		tmp = priv->rxringtail;
		do{
			if(tmp == priv->rxring)
				tmp  = priv->rxring + (priv->rxringcount - 1)*rx_desc_size;
			else
				tmp -= rx_desc_size;

			if(! (*tmp & (1<<31)))
				tmp2 = tmp;
		}while(tmp != priv->rxring);

		if(tmp2) priv->rxringtail = tmp2;
	}

	/* while there are filled descriptors */
	while(!(*(priv->rxringtail) & (1<<31))){
		if(*(priv->rxringtail) & (1<<26))
			DMESGW("RX buffer overflow");
		if(*(priv->rxringtail) & (1<<12))
			priv->stats.rxicverr++;

		if(*(priv->rxringtail) & (1<<27)){
			priv->stats.rxdmafail++;
			//DMESG("EE: RX DMA FAILED at buffer pointed by descriptor %x",(u32)priv->rxringtail);
			goto drop;
		}

		pci_dma_sync_single_for_cpu(priv->pdev,
				    priv->rxbuffer->dma,
				    priv->rxbuffersize * \
				    sizeof(u8),
				    PCI_DMA_FROMDEVICE);

		first = *(priv->rxringtail) & (1<<29) ? 1:0;
		if(first) priv->rx_prevlen=0;

		last = *(priv->rxringtail) & (1<<28) ? 1:0;
		if(last){
			lastlen=((*priv->rxringtail) &0xfff);

			/* if the last descriptor (that should
			 * tell us the total packet len) tell
			 * us something less than the descriptors
			 * len we had until now, then there is some
			 * problem..
			 * workaround to prevent kernel panic
			 */
			if(lastlen < priv->rx_prevlen)
				len=0;
			else
				len=lastlen-priv->rx_prevlen;

			if(*(priv->rxringtail) & (1<<13)) {
				if ((*(priv->rxringtail) & 0xfff) <500)
					priv->stats.rxcrcerrmin++;
				else if ((*(priv->rxringtail) & 0x0fff) >1000)
					priv->stats.rxcrcerrmax++;
				else
					priv->stats.rxcrcerrmid++;

			}

		}else{
			len = priv->rxbuffersize;
		}

		if(first && last) {
			padding = ((*(priv->rxringtail+3))&(0x04000000))>>26;
		}else if(first) {
			padding = ((*(priv->rxringtail+3))&(0x04000000))>>26;
			if(padding) {
				len -= 2;
			}
		}else {
			padding = 0;
		}
               padding = 0;
		priv->rx_prevlen+=len;

		if(priv->rx_prevlen > MAX_FRAG_THRESHOLD + 100){
			/* HW is probably passing several buggy frames
			* without FD or LD flag set.
			* Throw this garbage away to prevent skb
			* memory exausting
			*/
			if(!priv->rx_skb_complete)
				dev_kfree_skb_any(priv->rx_skb);
			priv->rx_skb_complete = 1;
		}

		signal=(unsigned char)(((*(priv->rxringtail+3))& (0x00ff0000))>>16);
		signal=(signal&0xfe)>>1;	// Modify by hikaru 6.6

		quality=(unsigned char)((*(priv->rxringtail+3)) & (0xff));

		stats.mac_time[0] = *(priv->rxringtail+1);
		stats.mac_time[1] = *(priv->rxringtail+2);
		rxpower =((char)(((*(priv->rxringtail+4))& (0x00ff0000))>>16))/2 - 42;
		RSSI = ((u8)(((*(priv->rxringtail+3)) & (0x0000ff00))>> 8)) & (0x7f);

		rate=((*(priv->rxringtail)) &
			((1<<23)|(1<<22)|(1<<21)|(1<<20)))>>20;

		stats.rate = rtl8180_rate2rate(rate);
		Antenna = (((*(priv->rxringtail +3))& (0x00008000)) == 0 )? 0:1 ;
//by amy for antenna
		if(!rtl8180_IsWirelessBMode(stats.rate))
		{ // OFDM rate.

			RxAGC_dBm = rxpower+1;	//bias
		}
		else
		{ // CCK rate.
			RxAGC_dBm = signal;//bit 0 discard

			LNA = (u8) (RxAGC_dBm & 0x60 ) >> 5 ; //bit 6~ bit 5
			BB  = (u8) (RxAGC_dBm & 0x1F);  // bit 4 ~ bit 0

   			RxAGC_dBm = -( LNA_gain[LNA] + (BB *2) ); //Pin_11b=-(LNA_gain+BB_gain) (dBm)

			RxAGC_dBm +=4; //bias
		}

		if(RxAGC_dBm & 0x80) //absolute value
   			RXAGC= ~(RxAGC_dBm)+1;
		bCckRate = rtl8180_IsWirelessBMode(stats.rate);
		// Translate RXAGC into 1-100.
		if(!rtl8180_IsWirelessBMode(stats.rate))
		{ // OFDM rate.
			if(RXAGC>90)
				RXAGC=90;
			else if(RXAGC<25)
				RXAGC=25;
			RXAGC=(90-RXAGC)*100/65;
		}
		else
		{ // CCK rate.
			if(RXAGC>95)
				RXAGC=95;
			else if(RXAGC<30)
				RXAGC=30;
			RXAGC=(95-RXAGC)*100/65;
		}
		priv->SignalStrength = (u8)RXAGC;
		priv->RecvSignalPower = RxAGC_dBm ;  // It can use directly by SD3 CMLin
		priv->RxPower = rxpower;
		priv->RSSI = RSSI;
//{by amy 080312
		// SQ translation formular is provided by SD3 DZ. 2006.06.27, by rcnjko.
		if(quality >= 127)
			quality = 1;//0; //0 will cause epc to show signal zero , walk aroud now;
		else if(quality < 27)
			quality = 100;
		else
			quality = 127 - quality;
		priv->SignalQuality = quality;
		if(!priv->card_8185)
			printk("check your card type\n");

		stats.signal = (u8)quality;//priv->wstats.qual.level = priv->SignalStrength;
		stats.signalstrength = RXAGC;
		if(stats.signalstrength > 100)
			stats.signalstrength = 100;
		stats.signalstrength = (stats.signalstrength * 70)/100 + 30;
	//	printk("==========================>rx : RXAGC is %d,signalstrength is %d\n",RXAGC,stats.signalstrength);
		stats.rssi = priv->wstats.qual.qual = priv->SignalQuality;
		stats.noise = priv->wstats.qual.noise = 100 - priv ->wstats.qual.qual;
//by amy 080312}
		bHwError = (((*(priv->rxringtail))& (0x00000fff)) == 4080)| (((*(priv->rxringtail))& (0x04000000)) != 0 )
			| (((*(priv->rxringtail))& (0x08000000)) != 0 )| (((~(*(priv->rxringtail)))& (0x10000000)) != 0 )| (((~(*(priv->rxringtail)))& (0x20000000)) != 0 );
		bCRC = ((*(priv->rxringtail)) & (0x00002000)) >> 13;
		bICV = ((*(priv->rxringtail)) & (0x00001000)) >> 12;
		hdr = (struct ieee80211_hdr_4addr *)priv->rxbuffer->buf;
		    fc = le16_to_cpu(hdr->frame_ctl);
	        type = WLAN_FC_GET_TYPE(fc);

			if((IEEE80211_FTYPE_CTL != type) &&
				(eqMacAddr(priv->ieee80211->current_network.bssid, (fc & IEEE80211_FCTL_TODS)? hdr->addr1 : (fc & IEEE80211_FCTL_FROMDS )? hdr->addr2 : hdr->addr3))
				 && (!bHwError) && (!bCRC)&& (!bICV))
			{
//by amy 080312
				// Perform signal smoothing for dynamic mechanism on demand.
				// This is different with PerformSignalSmoothing8185 in smoothing fomula.
				// No dramatic adjustion is apply because dynamic mechanism need some degree
				// of correctness. 2007.01.23, by shien chang.
				PerformUndecoratedSignalSmoothing8185(priv,bCckRate);
				//
				// For good-looking singal strength.
				//
				SignalStrengthIndex = NetgearSignalStrengthTranslate(
								priv->LastSignalStrengthInPercent,
								priv->SignalStrength);

				priv->LastSignalStrengthInPercent = SignalStrengthIndex;
				priv->Stats_SignalStrength = TranslateToDbm8185((u8)SignalStrengthIndex);
		//
		// We need more correct power of received packets and the  "SignalStrength" of RxStats is beautified,
		// so we record the correct power here.
		//
				priv->Stats_SignalQuality =(long) (priv->Stats_SignalQuality * 5 + (long)priv->SignalQuality + 5) / 6;
				priv->Stats_RecvSignalPower = (long)(priv->Stats_RecvSignalPower * 5 + priv->RecvSignalPower -1) / 6;

		// Figure out which antenna that received the lasted packet.
				priv->LastRxPktAntenna = Antenna ? 1 : 0; // 0: aux, 1: main.
//by amy 080312
			    SwAntennaDiversityRxOk8185(dev, priv->SignalStrength);
			}

//by amy for antenna
#ifndef DUMMY_RX
		if(first){
			if(!priv->rx_skb_complete){
				/* seems that HW sometimes fails to reiceve and
				   doesn't provide the last descriptor */
				dev_kfree_skb_any(priv->rx_skb);
				priv->stats.rxnolast++;
			}
			/* support for prism header has been originally added by Christian */
			if(priv->prism_hdr && priv->ieee80211->iw_mode == IW_MODE_MONITOR){

			}else{
				priv->rx_skb = dev_alloc_skb(len+2);
				if( !priv->rx_skb) goto drop;
			}

			priv->rx_skb_complete=0;
			priv->rx_skb->dev=dev;
		}else{
			/* if we are here we should  have already RXed
			* the first frame.
			* If we get here and the skb is not allocated then
			* we have just throw out garbage (skb not allocated)
			* and we are still rxing garbage....
			*/
			if(!priv->rx_skb_complete){

				tmp_skb= dev_alloc_skb(priv->rx_skb->len +len+2);

				if(!tmp_skb) goto drop;

				tmp_skb->dev=dev;

				memcpy(skb_put(tmp_skb,priv->rx_skb->len),
					priv->rx_skb->data,
					priv->rx_skb->len);

				dev_kfree_skb_any(priv->rx_skb);

				priv->rx_skb=tmp_skb;
			}
		}

		if(!priv->rx_skb_complete) {
			if(padding) {
				memcpy(skb_put(priv->rx_skb,len),
					(((unsigned char *)priv->rxbuffer->buf) + 2),len);
			} else {
				memcpy(skb_put(priv->rx_skb,len),
					priv->rxbuffer->buf,len);
			}
		}

		if(last && !priv->rx_skb_complete){
			if(priv->rx_skb->len > 4)
				skb_trim(priv->rx_skb,priv->rx_skb->len-4);
#ifndef RX_DONT_PASS_UL
			if(!ieee80211_rx(priv->ieee80211,
					 priv->rx_skb, &stats)){
#endif // RX_DONT_PASS_UL

				dev_kfree_skb_any(priv->rx_skb);
#ifndef RX_DONT_PASS_UL
			}
#endif
			priv->rx_skb_complete=1;
		}

#endif //DUMMY_RX
		pci_dma_sync_single_for_device(priv->pdev,
				    priv->rxbuffer->dma,
				    priv->rxbuffersize * \
				    sizeof(u8),
				    PCI_DMA_FROMDEVICE);

drop: // this is used when we have not enought mem
		/* restore the descriptor */
		*(priv->rxringtail+2)=priv->rxbuffer->dma;
		*(priv->rxringtail)=*(priv->rxringtail) &~ 0xfff;
		*(priv->rxringtail)=
			*(priv->rxringtail) | priv->rxbuffersize;

		*(priv->rxringtail)=
			*(priv->rxringtail) | (1<<31);

		priv->rxringtail+=rx_desc_size;
		if(priv->rxringtail >=
		   (priv->rxring)+(priv->rxringcount )*rx_desc_size)
			priv->rxringtail=priv->rxring;

		priv->rxbuffer=(priv->rxbuffer->next);
	}
}


void rtl8180_dma_kick(struct net_device *dev, int priority)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	rtl8180_set_mode(dev,EPROM_CMD_CONFIG);
	write_nic_byte(dev, TX_DMA_POLLING,
			(1 << (priority + 1)) | priv->dma_poll_mask);
	rtl8180_set_mode(dev,EPROM_CMD_NORMAL);

	force_pci_posting(dev);
}

void rtl8180_data_hard_stop(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	rtl8180_set_mode(dev,EPROM_CMD_CONFIG);
	priv->dma_poll_stop_mask |= TPPOLLSTOP_AC_VIQ;
	write_nic_byte(dev,TPPollStop, priv->dma_poll_stop_mask);
	rtl8180_set_mode(dev,EPROM_CMD_NORMAL);
}

void rtl8180_data_hard_resume(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);

	rtl8180_set_mode(dev,EPROM_CMD_CONFIG);
	priv->dma_poll_stop_mask &= ~(TPPOLLSTOP_AC_VIQ);
	write_nic_byte(dev,TPPollStop, priv->dma_poll_stop_mask);
	rtl8180_set_mode(dev,EPROM_CMD_NORMAL);
}

/* this function TX data frames when the ieee80211 stack requires this.
 * It checks also if we need to stop the ieee tx queue, eventually do it
 */
void rtl8180_hard_data_xmit(struct sk_buff *skb,struct net_device *dev, int
rate)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	int mode;
	struct ieee80211_hdr_3addr  *h = (struct ieee80211_hdr_3addr  *) skb->data;
	short morefrag = (h->frame_ctl) & IEEE80211_FCTL_MOREFRAGS;
	unsigned long flags;
	int priority;

	mode = priv->ieee80211->iw_mode;

	rate = ieeerate2rtlrate(rate);
	/*
	* This function doesn't require lock because we make
	* sure it's called with the tx_lock already acquired.
	* this come from the kernel's hard_xmit callback (trought
	* the ieee stack, or from the try_wake_queue (again trought
	* the ieee stack.
	*/
	priority = AC2Q(skb->priority);
	spin_lock_irqsave(&priv->tx_lock,flags);

	if(priv->ieee80211->bHwRadioOff)
	{
		spin_unlock_irqrestore(&priv->tx_lock,flags);

		return;
	}

	if (!check_nic_enought_desc(dev, priority)){
		DMESGW("Error: no descriptor left by previous TX (avail %d) ",
			get_curr_tx_free_desc(dev, priority));
		ieee80211_stop_queue(priv->ieee80211);
	}
	rtl8180_tx(dev, skb->data, skb->len, priority, morefrag,0,rate);
	if (!check_nic_enought_desc(dev, priority))
		ieee80211_stop_queue(priv->ieee80211);

	spin_unlock_irqrestore(&priv->tx_lock,flags);
}

/* This is a rough attempt to TX a frame
 * This is called by the ieee 80211 stack to TX management frames.
 * If the ring is full packet are dropped (for data frame the queue
 * is stopped before this can happen). For this reason it is better
 * if the descriptors are larger than the largest management frame
 * we intend to TX: i'm unsure what the HW does if it will not found
 * the last fragment of a frame because it has been dropped...
 * Since queues for Management and Data frames are different we
 * might use a different lock than tx_lock (for example mgmt_tx_lock)
 */
/* these function may loops if invoked with 0 descriptors or 0 len buffer*/
int rtl8180_hard_start_xmit(struct sk_buff *skb,struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	unsigned long flags;
	int priority;

	priority = MANAGE_PRIORITY;

	spin_lock_irqsave(&priv->tx_lock,flags);

	if (priv->ieee80211->bHwRadioOff) {
		spin_unlock_irqrestore(&priv->tx_lock,flags);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	rtl8180_tx(dev, skb->data, skb->len, priority,
		0, 0,ieeerate2rtlrate(priv->ieee80211->basic_rate));

	priv->ieee80211->stats.tx_bytes+=skb->len;
	priv->ieee80211->stats.tx_packets++;
	spin_unlock_irqrestore(&priv->tx_lock,flags);

	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;
}

// longpre 144+48 shortpre 72+24
u16 rtl8180_len2duration(u32 len, short rate,short* ext)
{
	u16 duration;
	u16 drift;
	*ext=0;

	switch(rate){
	case 0://1mbps
		*ext=0;
		duration = ((len+4)<<4) /0x2;
		drift = ((len+4)<<4) % 0x2;
		if(drift ==0 ) break;
		duration++;
		break;
	case 1://2mbps
		*ext=0;
		duration = ((len+4)<<4) /0x4;
		drift = ((len+4)<<4) % 0x4;
		if(drift ==0 ) break;
		duration++;
		break;
	case 2: //5.5mbps
		*ext=0;
		duration = ((len+4)<<4) /0xb;
		drift = ((len+4)<<4) % 0xb;
		if(drift ==0 )
			break;
		duration++;
		break;
	default:
	case 3://11mbps
		*ext=0;
		duration = ((len+4)<<4) /0x16;
		drift = ((len+4)<<4) % 0x16;
		if(drift ==0 )
			break;
		duration++;
		if(drift > 6)
			break;
		*ext=1;
		break;
	}

	return duration;
}

void rtl8180_prepare_beacon(struct net_device *dev)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct sk_buff *skb;

	u16 word  = read_nic_word(dev, BcnItv);
	word &= ~BcnItv_BcnItv; // clear Bcn_Itv
	word |= cpu_to_le16(priv->ieee80211->current_network.beacon_interval);//0x64;
	write_nic_word(dev, BcnItv, word);

	skb = ieee80211_get_beacon(priv->ieee80211);
	if(skb){
		rtl8180_tx(dev,skb->data,skb->len,BEACON_PRIORITY,
			0,0,ieeerate2rtlrate(priv->ieee80211->basic_rate));
		dev_kfree_skb_any(skb);
	}
}

/* This function do the real dirty work: it enqueues a TX command
 * descriptor in the ring buffer, copyes the frame in a TX buffer
 * and kicks the NIC to ensure it does the DMA transfer.
 */
short rtl8180_tx(struct net_device *dev, u8* txbuf, int len, int priority,
		 short morefrag, short descfrag, int rate)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u32 *tail,*temp_tail;
	u32 *begin;
	u32 *buf;
	int i;
	int remain;
	int buflen;
	int count;
	u16 duration;
	short ext;
	struct buffer* buflist;
	struct ieee80211_hdr_3addr *frag_hdr = (struct ieee80211_hdr_3addr *)txbuf;
	u8 dest[ETH_ALEN];
	u8			bUseShortPreamble = 0;
	u8			bCTSEnable = 0;
	u8			bRTSEnable = 0;
	u16 			Duration = 0;
	u16			RtsDur = 0;
	u16			ThisFrameTime = 0;
	u16			TxDescDuration = 0;
	u8 			ownbit_flag = false; //added by david woo for sync Tx, 2007.12.14

	switch(priority) {
	case MANAGE_PRIORITY:
		tail=priv->txmapringtail;
		begin=priv->txmapring;
		buflist = priv->txmapbufstail;
		count = priv->txringcount;
		break;
	case BK_PRIORITY:
		tail=priv->txbkpringtail;
		begin=priv->txbkpring;
		buflist = priv->txbkpbufstail;
		count = priv->txringcount;
		break;
	case BE_PRIORITY:
		tail=priv->txbepringtail;
		begin=priv->txbepring;
		buflist = priv->txbepbufstail;
		count = priv->txringcount;
		break;
	case VI_PRIORITY:
		tail=priv->txvipringtail;
		begin=priv->txvipring;
		buflist = priv->txvipbufstail;
		count = priv->txringcount;
		break;
	case VO_PRIORITY:
		tail=priv->txvopringtail;
		begin=priv->txvopring;
		buflist = priv->txvopbufstail;
		count = priv->txringcount;
		break;
	case HI_PRIORITY:
		tail=priv->txhpringtail;
		begin=priv->txhpring;
		buflist = priv->txhpbufstail;
		count = priv->txringcount;
		break;
	case BEACON_PRIORITY:
		tail=priv->txbeaconringtail;
		begin=priv->txbeaconring;
		buflist = priv->txbeaconbufstail;
		count = priv->txbeaconcount;
		break;
	default:
		return -1;
		break;
 	}

		memcpy(&dest, frag_hdr->addr1, ETH_ALEN);
		if (is_multicast_ether_addr(dest) ||
				is_broadcast_ether_addr(dest))
		{
			Duration = 0;
			RtsDur = 0;
			bRTSEnable = 0;
			bCTSEnable = 0;

			ThisFrameTime = ComputeTxTime(len + sCrcLng, rtl8180_rate2rate(rate), 0, bUseShortPreamble);
			TxDescDuration = ThisFrameTime;
		} else {// Unicast packet
			u16 AckTime;

			//YJ,add,080828,for Keep alive
			priv->NumTxUnicast++;

			// Figure out ACK rate according to BSS basic rate and Tx rate, 2006.03.08 by rcnjko.
			AckTime = ComputeTxTime(14, 10,0, 0);	// AckCTSLng = 14 use 1M bps send

			if ( ((len + sCrcLng) > priv->rts) && priv->rts )
			{ // RTS/CTS.
				u16 RtsTime, CtsTime;
				//u16 CtsRate;
				bRTSEnable = 1;
				bCTSEnable = 0;

				// Rate and time required for RTS.
				RtsTime = ComputeTxTime( sAckCtsLng/8,priv->ieee80211->basic_rate, 0, 0);
				// Rate and time required for CTS.
				CtsTime = ComputeTxTime(14, 10,0, 0);	// AckCTSLng = 14 use 1M bps send

				// Figure out time required to transmit this frame.
				ThisFrameTime = ComputeTxTime(len + sCrcLng,
						rtl8180_rate2rate(rate),
						0,
						bUseShortPreamble);

				// RTS-CTS-ThisFrame-ACK.
				RtsDur = CtsTime + ThisFrameTime + AckTime + 3*aSifsTime;

				TxDescDuration = RtsTime + RtsDur;
			}
			else {// Normal case.
				bCTSEnable = 0;
				bRTSEnable = 0;
				RtsDur = 0;

				ThisFrameTime = ComputeTxTime(len + sCrcLng, rtl8180_rate2rate(rate), 0, bUseShortPreamble);
				TxDescDuration = ThisFrameTime + aSifsTime + AckTime;
			}

			if(!(frag_hdr->frame_ctl & IEEE80211_FCTL_MOREFRAGS)) { //no more fragment
				// ThisFrame-ACK.
				Duration = aSifsTime + AckTime;
			} else { // One or more fragments remained.
				u16 NextFragTime;
				NextFragTime = ComputeTxTime( len + sCrcLng, //pretend following packet length equal current packet
						rtl8180_rate2rate(rate),
						0,
						bUseShortPreamble );

				//ThisFrag-ACk-NextFrag-ACK.
				Duration = NextFragTime + 3*aSifsTime + 2*AckTime;
			}

		} // End of Unicast packet

		frag_hdr->duration_id = Duration;

	buflen=priv->txbuffsize;
	remain=len;
	temp_tail = tail;

	while(remain!=0){
		mb();
		if(!buflist){
			DMESGE("TX buffer error, cannot TX frames. pri %d.", priority);
			return -1;
		}
		buf=buflist->buf;

		if ((*tail & (1 << 31)) && (priority != BEACON_PRIORITY)) {
			DMESGW("No more TX desc, returning %x of %x",
			       remain, len);
			priv->stats.txrdu++;
			return remain;
		}

		*tail= 0; // zeroes header
		*(tail+1) = 0;
		*(tail+3) = 0;
		*(tail+5) = 0;
		*(tail+6) = 0;
		*(tail+7) = 0;

		if(priv->card_8185){
			//FIXME: this should be triggered by HW encryption parameters.
			*tail |= (1<<15); //no encrypt
		}

		if(remain==len && !descfrag) {
			ownbit_flag = false;	//added by david woo,2007.12.14
			*tail = *tail| (1<<29) ; //fist segment of the packet
			*tail = *tail |(len);
		} else {
			ownbit_flag = true;
		}

		for(i=0;i<buflen&& remain >0;i++,remain--){
			((u8*)buf)[i]=txbuf[i]; //copy data into descriptor pointed DMAble buffer
			if(remain == 4 && i+4 >= buflen) break;
			/* ensure the last desc has at least 4 bytes payload */

		}
		txbuf = txbuf + i;
		*(tail+3)=*(tail+3) &~ 0xfff;
		*(tail+3)=*(tail+3) | i; // buffer lenght
		// Use short preamble or not
		if (priv->ieee80211->current_network.capability&WLAN_CAPABILITY_SHORT_PREAMBLE)
			if (priv->plcp_preamble_mode==1 && rate!=0)	//  short mode now, not long!
			;//	*tail |= (1<<16);				// enable short preamble mode.

		if(bCTSEnable) {
			*tail |= (1<<18);
		}

		if(bRTSEnable) //rts enable
		{
			*tail |= ((ieeerate2rtlrate(priv->ieee80211->basic_rate))<<19);//RTS RATE
			*tail |= (1<<23);//rts enable
			*(tail+1) |=(RtsDur&0xffff);//RTS Duration
		}
		*(tail+3) |= ((TxDescDuration&0xffff)<<16); //DURATION
//	        *(tail+3) |= (0xe6<<16);
        	*(tail+5) |= (11<<8);//(priv->retry_data<<8); //retry lim ;

		*tail = *tail | ((rate&0xf) << 24);

		/* hw_plcp_len is not used for rtl8180 chip */
		/* FIXME */
		if(priv->card_8185 == 0 || !priv->hw_plcp_len){
			duration = rtl8180_len2duration(len, rate, &ext);
			*(tail+1) = *(tail+1) | ((duration & 0x7fff)<<16);
			if(ext) *(tail+1) = *(tail+1) |(1<<31); //plcp length extension
		}

		if(morefrag) *tail = (*tail) | (1<<17); // more fragment
		if(!remain) *tail = (*tail) | (1<<28); // last segment of frame

               *(tail+5) = *(tail+5)|(2<<27);
                *(tail+7) = *(tail+7)|(1<<4);

		wmb();
		if(ownbit_flag)
		{
			*tail = *tail | (1<<31); // descriptor ready to be txed
		}

		if((tail - begin)/8 == count-1)
			tail=begin;
		else
			tail=tail+8;

		buflist=buflist->next;

		mb();

		switch(priority) {
			case MANAGE_PRIORITY:
				priv->txmapringtail=tail;
				priv->txmapbufstail=buflist;
				break;
			case BK_PRIORITY:
				priv->txbkpringtail=tail;
				priv->txbkpbufstail=buflist;
				break;
			case BE_PRIORITY:
				priv->txbepringtail=tail;
				priv->txbepbufstail=buflist;
				break;
			case VI_PRIORITY:
				priv->txvipringtail=tail;
				priv->txvipbufstail=buflist;
				break;
			case VO_PRIORITY:
				priv->txvopringtail=tail;
				priv->txvopbufstail=buflist;
				break;
			case HI_PRIORITY:
				priv->txhpringtail=tail;
				priv->txhpbufstail = buflist;
				break;
			case BEACON_PRIORITY:
				/* the HW seems to be happy with the 1st
				 * descriptor filled and the 2nd empty...
				 * So always update descriptor 1 and never
				 * touch 2nd
				 */
				break;
		}
	}
	*temp_tail = *temp_tail | (1<<31); // descriptor ready to be txed
	rtl8180_dma_kick(dev,priority);

	return 0;
}

void rtl8180_irq_rx_tasklet(struct r8180_priv * priv);

void rtl8180_link_change(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u16 beacon_interval;
	struct ieee80211_network *net = &priv->ieee80211->current_network;

	rtl8180_update_msr(dev);

	rtl8180_set_mode(dev,EPROM_CMD_CONFIG);

	write_nic_dword(dev,BSSID,((u32*)net->bssid)[0]);
	write_nic_word(dev,BSSID+4,((u16*)net->bssid)[2]);

	beacon_interval  = read_nic_dword(dev,BEACON_INTERVAL);
	beacon_interval &= ~ BEACON_INTERVAL_MASK;
	beacon_interval |= net->beacon_interval;
	write_nic_dword(dev, BEACON_INTERVAL, beacon_interval);

	rtl8180_set_mode(dev, EPROM_CMD_NORMAL);

	if(priv->card_8185)
		rtl8180_set_chan(dev, priv->chan);
}

void rtl8180_rq_tx_ack(struct net_device *dev){

	struct r8180_priv *priv = ieee80211_priv(dev);

	write_nic_byte(dev,CONFIG4,read_nic_byte(dev,CONFIG4)|CONFIG4_PWRMGT);
	priv->ack_tx_to_ieee = 1;
}

short rtl8180_is_tx_queue_empty(struct net_device *dev){

	struct r8180_priv *priv = ieee80211_priv(dev);
	u32* d;

	for (d = priv->txmapring;
		d < priv->txmapring + priv->txringcount;d+=8)
			if(*d & (1<<31)) return 0;

	for (d = priv->txbkpring;
		d < priv->txbkpring + priv->txringcount;d+=8)
			if(*d & (1<<31)) return 0;

	for (d = priv->txbepring;
		d < priv->txbepring + priv->txringcount;d+=8)
			if(*d & (1<<31)) return 0;

	for (d = priv->txvipring;
		d < priv->txvipring + priv->txringcount;d+=8)
			if(*d & (1<<31)) return 0;

	for (d = priv->txvopring;
		d < priv->txvopring + priv->txringcount;d+=8)
			if(*d & (1<<31)) return 0;

	for (d = priv->txhpring;
		d < priv->txhpring + priv->txringcount;d+=8)
			if(*d & (1<<31)) return 0;
	return 1;
}
/* FIXME FIXME 5msecs is random */
#define HW_WAKE_DELAY 5

void rtl8180_hw_wakeup(struct net_device *dev)
{
	unsigned long flags;
	struct r8180_priv *priv = ieee80211_priv(dev);

	spin_lock_irqsave(&priv->ps_lock,flags);
	write_nic_byte(dev,CONFIG4,read_nic_byte(dev,CONFIG4)&~CONFIG4_PWRMGT);
	if (priv->rf_wakeup)
		priv->rf_wakeup(dev);
	spin_unlock_irqrestore(&priv->ps_lock,flags);
}

void rtl8180_hw_sleep_down(struct net_device *dev)
{
        unsigned long flags;
        struct r8180_priv *priv = ieee80211_priv(dev);

        spin_lock_irqsave(&priv->ps_lock,flags);
        if(priv->rf_sleep)
                priv->rf_sleep(dev);
        spin_unlock_irqrestore(&priv->ps_lock,flags);
}

void rtl8180_hw_sleep(struct net_device *dev, u32 th, u32 tl)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u32 rb = jiffies;
	unsigned long flags;

	spin_lock_irqsave(&priv->ps_lock,flags);

	/* Writing HW register with 0 equals to disable
	 * the timer, that is not really what we want
	 */
	tl -= MSECS(4+16+7);

	/* If the interval in witch we are requested to sleep is too
	 * short then give up and remain awake
	 */
	if(((tl>=rb)&& (tl-rb) <= MSECS(MIN_SLEEP_TIME))
		||((rb>tl)&& (rb-tl) < MSECS(MIN_SLEEP_TIME))) {
		spin_unlock_irqrestore(&priv->ps_lock,flags);
		printk("too short to sleep\n");
		return;
	}

	{
		u32 tmp = (tl>rb)?(tl-rb):(rb-tl);

		priv->DozePeriodInPast2Sec += jiffies_to_msecs(tmp);

		queue_delayed_work(priv->ieee80211->wq, &priv->ieee80211->hw_wakeup_wq, tmp); //as tl may be less than rb
	}
	/* if we suspect the TimerInt is gone beyond tl
	 * while setting it, then give up
	 */

	if(((tl > rb) && ((tl-rb) > MSECS(MAX_SLEEP_TIME)))||
		((tl < rb) && ((rb-tl) > MSECS(MAX_SLEEP_TIME)))) {
		spin_unlock_irqrestore(&priv->ps_lock,flags);
		return;
	}

	queue_work(priv->ieee80211->wq, (void *)&priv->ieee80211->hw_sleep_wq);
	spin_unlock_irqrestore(&priv->ps_lock,flags);
}

void rtl8180_wmm_param_update(struct work_struct * work)
{
	struct ieee80211_device * ieee = container_of(work, struct ieee80211_device,wmm_param_update_wq);
	struct net_device *dev = ieee->dev;
	u8 *ac_param = (u8 *)(ieee->current_network.wmm_param);
	u8 mode = ieee->current_network.mode;
	AC_CODING	eACI;
	AC_PARAM	AcParam;
	PAC_PARAM	pAcParam;
	u8 i;

	if(!ieee->current_network.QoS_Enable){
		//legacy ac_xx_param update
		AcParam.longData = 0;
		AcParam.f.AciAifsn.f.AIFSN = 2; // Follow 802.11 DIFS.
		AcParam.f.AciAifsn.f.ACM = 0;
		AcParam.f.Ecw.f.ECWmin = 3; // Follow 802.11 CWmin.
		AcParam.f.Ecw.f.ECWmax = 7; // Follow 802.11 CWmax.
		AcParam.f.TXOPLimit = 0;
		for(eACI = 0; eACI < AC_MAX; eACI++){
			AcParam.f.AciAifsn.f.ACI = (u8)eACI;
			{
				u8		u1bAIFS;
				u32		u4bAcParam;
				pAcParam = (PAC_PARAM)(&AcParam);
				// Retrive paramters to udpate.
				u1bAIFS = pAcParam->f.AciAifsn.f.AIFSN *(((mode&IEEE_G) == IEEE_G)?9:20) + aSifsTime;
				u4bAcParam = ((((u32)(pAcParam->f.TXOPLimit))<<AC_PARAM_TXOP_LIMIT_OFFSET)|
					      (((u32)(pAcParam->f.Ecw.f.ECWmax))<<AC_PARAM_ECW_MAX_OFFSET)|
					      (((u32)(pAcParam->f.Ecw.f.ECWmin))<<AC_PARAM_ECW_MIN_OFFSET)|
					       (((u32)u1bAIFS) << AC_PARAM_AIFS_OFFSET));
				switch(eACI){
					case AC1_BK:
						write_nic_dword(dev, AC_BK_PARAM, u4bAcParam);
						break;
					case AC0_BE:
						write_nic_dword(dev, AC_BE_PARAM, u4bAcParam);
						break;
					case AC2_VI:
						write_nic_dword(dev, AC_VI_PARAM, u4bAcParam);
						break;
					case AC3_VO:
						write_nic_dword(dev, AC_VO_PARAM, u4bAcParam);
						break;
					default:
						printk(KERN_WARNING "SetHwReg8185():invalid ACI: %d!\n", eACI);
						break;
				}
			}
		}
		return;
	}

	for(i = 0; i < AC_MAX; i++){
		//AcParam.longData = 0;
		pAcParam = (AC_PARAM * )ac_param;
		{
			AC_CODING	eACI;
			u8		u1bAIFS;
			u32		u4bAcParam;

			// Retrive paramters to udpate.
			eACI = pAcParam->f.AciAifsn.f.ACI;
			//Mode G/A: slotTimeTimer = 9; Mode B: 20
			u1bAIFS = pAcParam->f.AciAifsn.f.AIFSN * (((mode&IEEE_G) == IEEE_G)?9:20) + aSifsTime;
			u4bAcParam = (	(((u32)(pAcParam->f.TXOPLimit)) << AC_PARAM_TXOP_LIMIT_OFFSET)	|
					(((u32)(pAcParam->f.Ecw.f.ECWmax)) << AC_PARAM_ECW_MAX_OFFSET)	|
					(((u32)(pAcParam->f.Ecw.f.ECWmin)) << AC_PARAM_ECW_MIN_OFFSET)	|
					(((u32)u1bAIFS) << AC_PARAM_AIFS_OFFSET));

			switch(eACI){
				case AC1_BK:
					write_nic_dword(dev, AC_BK_PARAM, u4bAcParam);
					break;
				case AC0_BE:
					write_nic_dword(dev, AC_BE_PARAM, u4bAcParam);
					break;
				case AC2_VI:
					write_nic_dword(dev, AC_VI_PARAM, u4bAcParam);
					break;
				case AC3_VO:
					write_nic_dword(dev, AC_VO_PARAM, u4bAcParam);
					break;
				default:
					printk(KERN_WARNING "SetHwReg8185(): invalid ACI: %d !\n", eACI);
					break;
			}
		}
		ac_param += (sizeof(AC_PARAM));
	}
}

void rtl8180_tx_irq_wq(struct work_struct *work);
void rtl8180_restart_wq(struct work_struct *work);
//void rtl8180_rq_tx_ack(struct work_struct *work);
void rtl8180_watch_dog_wq(struct work_struct *work);
void rtl8180_hw_wakeup_wq(struct work_struct *work);
void rtl8180_hw_sleep_wq(struct work_struct *work);
void rtl8180_sw_antenna_wq(struct work_struct *work);
void rtl8180_watch_dog(struct net_device *dev);

void watch_dog_adaptive(unsigned long data)
{
	struct r8180_priv* priv = ieee80211_priv((struct net_device *)data);

	if (!priv->up) {
		DMESG("<----watch_dog_adaptive():driver is not up!\n");
		return;
	}

	// Tx High Power Mechanism.
#ifdef HIGH_POWER
	if(CheckHighPower((struct net_device *)data))
	{
		queue_work(priv->ieee80211->wq, (void *)&priv->ieee80211->tx_pw_wq);
	}
#endif

	// Tx Power Tracking on 87SE.
#ifdef TX_TRACK
	//if( priv->bTxPowerTrack )	//lzm mod 080826
	if( CheckTxPwrTracking((struct net_device *)data));
		TxPwrTracking87SE((struct net_device *)data);
#endif

	// Perform DIG immediately.
#ifdef SW_DIG
	if(CheckDig((struct net_device *)data) == true)
	{
		queue_work(priv->ieee80211->wq, (void *)&priv->ieee80211->hw_dig_wq);
	}
#endif
   	rtl8180_watch_dog((struct net_device *)data);

	queue_work(priv->ieee80211->wq, (void *)&priv->ieee80211->GPIOChangeRFWorkItem);

   	priv->watch_dog_timer.expires = jiffies + MSECS(IEEE80211_WATCH_DOG_TIME);
	add_timer(&priv->watch_dog_timer);
}

static CHANNEL_LIST ChannelPlan[] = {
	{{1,2,3,4,5,6,7,8,9,10,11,36,40,44,48,52,56,60,64},19},  		//FCC
	{{1,2,3,4,5,6,7,8,9,10,11},11},                    				//IC
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,36,40,44,48,52,56,60,64},21},  	//ETSI
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,36,40,44,48,52,56,60,64},21},    //Spain. Change to ETSI.
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,36,40,44,48,52,56,60,64},21},  	//France. Change to ETSI.
	{{14,36,40,44,48,52,56,60,64},9},						//MKK
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,14, 36,40,44,48,52,56,60,64},22},//MKK1
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,36,40,44,48,52,56,60,64},21},	//Israel.
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,34,38,42,46},17},			// For 11a , TELEC
	{{1,2,3,4,5,6,7,8,9,10,11,12,13,14},14},  //For Global Domain. 1-11:active scan, 12-14 passive scan. //+YJ, 080626
	{{1,2,3,4,5,6,7,8,9,10,11,12,13},13} //world wide 13: ch1~ch11 active scan, ch12~13 passive //lzm add 080826
};

static void rtl8180_set_channel_map(u8 channel_plan, struct ieee80211_device *ieee)
{
	int i;

	//lzm add 080826
	ieee->MinPassiveChnlNum=MAX_CHANNEL_NUMBER+1;
	ieee->IbssStartChnl=0;

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
		{
			Dot11d_Init(ieee);
			ieee->bGlobalDomain = false;
			if (ChannelPlan[channel_plan].Len != 0){
				// Clear old channel map
				memset(GET_DOT11D_INFO(ieee)->channel_map, 0, sizeof(GET_DOT11D_INFO(ieee)->channel_map));
				// Set new channel map
				for (i=0;i<ChannelPlan[channel_plan].Len;i++)
				{
					if(ChannelPlan[channel_plan].Channel[i] <= 14)
						GET_DOT11D_INFO(ieee)->channel_map[ChannelPlan[channel_plan].Channel[i]] = 1;
				}
			}
			break;
		}
		case COUNTRY_CODE_GLOBAL_DOMAIN:
		{
			GET_DOT11D_INFO(ieee)->bEnabled = 0;
			Dot11d_Reset(ieee);
			ieee->bGlobalDomain = true;
			break;
		}
		case COUNTRY_CODE_WORLD_WIDE_13_INDEX://lzm add 080826
		{
		ieee->MinPassiveChnlNum=12;
		ieee->IbssStartChnl= 10;
		break;
		}
		default:
		{
			Dot11d_Init(ieee);
			ieee->bGlobalDomain = false;
			memset(GET_DOT11D_INFO(ieee)->channel_map, 0, sizeof(GET_DOT11D_INFO(ieee)->channel_map));
			for (i=1;i<=14;i++)
			{
				GET_DOT11D_INFO(ieee)->channel_map[i] = 1;
			}
			break;
		}
	}
}

void GPIOChangeRFWorkItemCallBack(struct work_struct *work);

//YJ,add,080828
static void rtl8180_statistics_init(struct Stats *pstats)
{
	memset(pstats, 0, sizeof(struct Stats));
}

static void rtl8180_link_detect_init(plink_detect_t plink_detect)
{
	memset(plink_detect, 0, sizeof(link_detect_t));
	plink_detect->SlotNum = DEFAULT_SLOT_NUM;
}
//YJ,add,080828,end

short rtl8180_init(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u16 word;
	u16 version;
	u8 hw_version;
	//u8 config3;
	u32 usValue;
	u16 tmpu16;
	int i, j;

	priv->channel_plan = eprom_read(dev, EEPROM_COUNTRY_CODE>>1) & 0xFF;
	if(priv->channel_plan > COUNTRY_CODE_GLOBAL_DOMAIN){
		printk("rtl8180_init:Error channel plan! Set to default.\n");
		priv->channel_plan = 0;
	}

	DMESG("Channel plan is %d\n",priv->channel_plan);
	rtl8180_set_channel_map(priv->channel_plan, priv->ieee80211);

	//FIXME: these constants are placed in a bad pleace.
	priv->txbuffsize = 2048;//1024;
	priv->txringcount = 32;//32;
	priv->rxbuffersize = 2048;//1024;
	priv->rxringcount = 64;//32;
	priv->txbeaconcount = 2;
	priv->rx_skb_complete = 1;

	priv->RegThreeWireMode = HW_THREE_WIRE_SI;

	priv->RFChangeInProgress = false;
	priv->SetRFPowerStateInProgress = false;
	priv->RFProgType = 0;
	priv->bInHctTest = false;

	priv->irq_enabled=0;

	rtl8180_statistics_init(&priv->stats);
	rtl8180_link_detect_init(&priv->link_detect);

	priv->ack_tx_to_ieee = 0;
	priv->ieee80211->current_network.beacon_interval = DEFAULT_BEACONINTERVAL;
	priv->ieee80211->iw_mode = IW_MODE_INFRA;
	priv->ieee80211->softmac_features  = IEEE_SOFTMAC_SCAN |
		IEEE_SOFTMAC_ASSOCIATE | IEEE_SOFTMAC_PROBERQ |
		IEEE_SOFTMAC_PROBERS | IEEE_SOFTMAC_TX_QUEUE;
	priv->ieee80211->active_scan = 1;
	priv->ieee80211->rate = 110; //11 mbps
	priv->ieee80211->modulation = IEEE80211_CCK_MODULATION;
	priv->ieee80211->host_encrypt = 1;
	priv->ieee80211->host_decrypt = 1;
	priv->ieee80211->sta_wake_up = rtl8180_hw_wakeup;
	priv->ieee80211->ps_request_tx_ack = rtl8180_rq_tx_ack;
	priv->ieee80211->enter_sleep_state = rtl8180_hw_sleep;
	priv->ieee80211->ps_is_queue_empty = rtl8180_is_tx_queue_empty;

	priv->hw_wep = hwwep;
	priv->prism_hdr=0;
	priv->dev=dev;
	priv->retry_rts = DEFAULT_RETRY_RTS;
	priv->retry_data = DEFAULT_RETRY_DATA;
	priv->RFChangeInProgress = false;
	priv->SetRFPowerStateInProgress = false;
	priv->RFProgType = 0;
	priv->bInHctTest = false;
	priv->bInactivePs = true;//false;
	priv->ieee80211->bInactivePs = priv->bInactivePs;
	priv->bSwRfProcessing = false;
	priv->eRFPowerState = eRfOff;
	priv->RfOffReason = 0;
	priv->LedStrategy = SW_LED_MODE0;
	priv->TxPollingTimes = 0;//lzm add 080826
	priv->bLeisurePs = true;
	priv->dot11PowerSaveMode = eActive;
//by amy for antenna
	priv->AdMinCheckPeriod = 5;
	priv->AdMaxCheckPeriod = 10;
// Lower signal strength threshold to fit the HW participation in antenna diversity. +by amy 080312
	priv->AdMaxRxSsThreshold = 30;//60->30
	priv->AdRxSsThreshold = 20;//50->20
	priv->AdCheckPeriod = priv->AdMinCheckPeriod;
	priv->AdTickCount = 0;
	priv->AdRxSignalStrength = -1;
	priv->RegSwAntennaDiversityMechanism = 0;
	priv->RegDefaultAntenna = 0;
	priv->SignalStrength = 0;
	priv->AdRxOkCnt = 0;
	priv->CurrAntennaIndex = 0;
	priv->AdRxSsBeforeSwitched = 0;
	init_timer(&priv->SwAntennaDiversityTimer);
	priv->SwAntennaDiversityTimer.data = (unsigned long)dev;
	priv->SwAntennaDiversityTimer.function = (void *)SwAntennaDiversityTimerCallback;
//by amy for antenna
//{by amy 080312
	priv->bDigMechanism = 1;
	priv->InitialGain = 6;
	priv->bXtalCalibration = false;
	priv->XtalCal_Xin = 0;
	priv->XtalCal_Xout = 0;
	priv->bTxPowerTrack = false;
	priv->ThermalMeter = 0;
	priv->FalseAlarmRegValue = 0;
	priv->RegDigOfdmFaUpTh = 0xc; // Upper threhold of OFDM false alarm, which is used in DIG.
	priv->DIG_NumberFallbackVote = 0;
	priv->DIG_NumberUpgradeVote = 0;
	priv->LastSignalStrengthInPercent = 0;
	priv->Stats_SignalStrength = 0;
	priv->LastRxPktAntenna = 0;
	priv->SignalQuality = 0; // in 0-100 index.
	priv->Stats_SignalQuality = 0;
	priv->RecvSignalPower = 0; // in dBm.
	priv->Stats_RecvSignalPower = 0;
	priv->AdMainAntennaRxOkCnt = 0;
	priv->AdAuxAntennaRxOkCnt = 0;
	priv->bHWAdSwitched = false;
	priv->bRegHighPowerMechanism = true;
	priv->RegHiPwrUpperTh = 77;
	priv->RegHiPwrLowerTh = 75;
	priv->RegRSSIHiPwrUpperTh = 70;
	priv->RegRSSIHiPwrLowerTh = 20;
	priv->bCurCCKPkt = false;
	priv->UndecoratedSmoothedSS = -1;
	priv->bToUpdateTxPwr = false;
	priv->CurCCKRSSI = 0;
	priv->RxPower = 0;
	priv->RSSI = 0;
	priv->NumTxOkTotal = 0;
	priv->NumTxUnicast = 0;
	priv->keepAliveLevel = DEFAULT_KEEP_ALIVE_LEVEL;
	priv->PowerProfile = POWER_PROFILE_AC;
    priv->CurrRetryCnt=0;
    priv->LastRetryCnt=0;
    priv->LastTxokCnt=0;
    priv->LastRxokCnt=0;
    priv->LastRetryRate=0;
    priv->bTryuping=0;
    priv->CurrTxRate=0;
    priv->CurrRetryRate=0;
    priv->TryupingCount=0;
    priv->TryupingCountNoData=0;
    priv->TryDownCountLowData=0;
    priv->LastTxOKBytes=0;
    priv->LastFailTxRate=0;
    priv->LastFailTxRateSS=0;
    priv->FailTxRateCount=0;
    priv->LastTxThroughput=0;
    priv->NumTxOkBytesTotal=0;
	priv->ForcedDataRate = 0;
	priv->RegBModeGainStage = 1;

	priv->promisc = (dev->flags & IFF_PROMISC) ? 1:0;
	spin_lock_init(&priv->irq_lock);
	spin_lock_init(&priv->irq_th_lock);
	spin_lock_init(&priv->tx_lock);
	spin_lock_init(&priv->ps_lock);
	spin_lock_init(&priv->rf_ps_lock);
	sema_init(&priv->wx_sem,1);
	sema_init(&priv->rf_state,1);
	INIT_WORK(&priv->reset_wq,(void*) rtl8180_restart_wq);
	INIT_WORK(&priv->tx_irq_wq,(void*) rtl8180_tx_irq_wq);
	INIT_DELAYED_WORK(&priv->ieee80211->hw_wakeup_wq,(void*) rtl8180_hw_wakeup_wq);
	INIT_DELAYED_WORK(&priv->ieee80211->hw_sleep_wq,(void*) rtl8180_hw_sleep_wq);
	INIT_WORK(&priv->ieee80211->wmm_param_update_wq,(void*) rtl8180_wmm_param_update);
	INIT_DELAYED_WORK(&priv->ieee80211->rate_adapter_wq,(void*)rtl8180_rate_adapter);//+by amy 080312
	INIT_DELAYED_WORK(&priv->ieee80211->hw_dig_wq,(void*)rtl8180_hw_dig_wq);//+by amy 080312
	INIT_DELAYED_WORK(&priv->ieee80211->tx_pw_wq,(void*)rtl8180_tx_pw_wq);//+by amy 080312

	INIT_DELAYED_WORK(&priv->ieee80211->GPIOChangeRFWorkItem,(void*) GPIOChangeRFWorkItemCallBack);

	tasklet_init(&priv->irq_rx_tasklet,
		     (void(*)(unsigned long)) rtl8180_irq_rx_tasklet,
		     (unsigned long)priv);

    init_timer(&priv->watch_dog_timer);
	priv->watch_dog_timer.data = (unsigned long)dev;
	priv->watch_dog_timer.function = watch_dog_adaptive;

    init_timer(&priv->rateadapter_timer);
        priv->rateadapter_timer.data = (unsigned long)dev;
        priv->rateadapter_timer.function = timer_rate_adaptive;
		priv->RateAdaptivePeriod= RATE_ADAPTIVE_TIMER_PERIOD;
		priv->bEnhanceTxPwr=false;

	priv->ieee80211->softmac_hard_start_xmit = rtl8180_hard_start_xmit;
	priv->ieee80211->set_chan = rtl8180_set_chan;
	priv->ieee80211->link_change = rtl8180_link_change;
	priv->ieee80211->softmac_data_hard_start_xmit = rtl8180_hard_data_xmit;
	priv->ieee80211->data_hard_stop = rtl8180_data_hard_stop;
	priv->ieee80211->data_hard_resume = rtl8180_data_hard_resume;

        priv->ieee80211->init_wmmparam_flag = 0;

	priv->ieee80211->start_send_beacons = rtl8180_start_tx_beacon;
	priv->ieee80211->stop_send_beacons = rtl8180_beacon_tx_disable;
	priv->ieee80211->fts = DEFAULT_FRAG_THRESHOLD;

	priv->MWIEnable = 0;

	priv->ShortRetryLimit = 7;
	priv->LongRetryLimit = 7;
	priv->EarlyRxThreshold = 7;

	priv->CSMethod = (0x01 << 29);

	priv->TransmitConfig	=
									1<<TCR_DurProcMode_OFFSET |		//for RTL8185B, duration setting by HW
									(7<<TCR_MXDMA_OFFSET) |	// Max DMA Burst Size per Tx DMA Burst, 7: reservied.
									(priv->ShortRetryLimit<<TCR_SRL_OFFSET) |	// Short retry limit
									(priv->LongRetryLimit<<TCR_LRL_OFFSET) |	// Long retry limit
									(0 ? TCR_SAT : 0);	// FALSE: HW provies PLCP length and LENGEXT, TURE: SW proiveds them

	priv->ReceiveConfig	=
								RCR_AMF | RCR_ADF |				//accept management/data
								RCR_ACF |						//accept control frame for SW AP needs PS-poll, 2005.07.07, by rcnjko.
								RCR_AB | RCR_AM | RCR_APM |		//accept BC/MC/UC
								(7<<RCR_MXDMA_OFFSET) | // Max DMA Burst Size per Rx DMA Burst, 7: unlimited.
								(priv->EarlyRxThreshold<<RCR_FIFO_OFFSET) | // Rx FIFO Threshold, 7: No Rx threshold.
								(priv->EarlyRxThreshold == 7 ? RCR_ONLYERLPKT:0);

	priv->IntrMask		= IMR_TMGDOK | IMR_TBDER | IMR_THPDER |
								IMR_THPDER | IMR_THPDOK |
								IMR_TVODER | IMR_TVODOK |
								IMR_TVIDER | IMR_TVIDOK |
								IMR_TBEDER | IMR_TBEDOK |
								IMR_TBKDER | IMR_TBKDOK |
								IMR_RDU |						// To handle the defragmentation not enough Rx descriptors case. Annie, 2006-03-27.
								IMR_RER | IMR_ROK |
								IMR_RQoSOK; // <NOTE> ROK and RQoSOK are mutually exclusive, so, we must handle RQoSOK interrupt to receive QoS frames, 2005.12.09, by rcnjko.

	priv->InitialGain = 6;

	hw_version =( read_nic_dword(dev, TCR) & TCR_HWVERID_MASK)>>TCR_HWVERID_SHIFT;

	switch (hw_version){
		case HW_VERID_R8185B_B:
                        priv->card_8185 = VERSION_8187S_C;
		        DMESG("MAC controller is a RTL8187SE b/g");
			priv->phy_ver = 2;
			break;
		case HW_VERID_R8185_ABC:
			DMESG("MAC controller is a RTL8185 b/g");
			priv->card_8185 = 1;
			/* you should not find a card with 8225 PHY ver < C*/
			priv->phy_ver = 2;
			break;
		case HW_VERID_R8185_D:
			DMESG("MAC controller is a RTL8185 b/g (V. D)");
			priv->card_8185 = 2;
			/* you should not find a card with 8225 PHY ver < C*/
			priv->phy_ver = 2;
			break;
		case HW_VERID_R8180_ABCD:
			DMESG("MAC controller is a RTL8180");
			priv->card_8185 = 0;
			break;
		case HW_VERID_R8180_F:
			DMESG("MAC controller is a RTL8180 (v. F)");
			priv->card_8185 = 0;
			break;
		default:
			DMESGW("MAC chip not recognized: version %x. Assuming RTL8180",hw_version);
			priv->card_8185 = 0;
			break;
	}

	if(priv->card_8185){
		priv->ieee80211->modulation |= IEEE80211_OFDM_MODULATION;
		priv->ieee80211->short_slot = 1;
	}
	/* you should not found any 8185 Ver B Card */
	priv->card_8185_Bversion = 0;

	// just for sync 85
	priv->card_type = PCI;
        DMESG("This is a PCI NIC");
	priv->enable_gpio0 = 0;

	usValue = eprom_read(dev, EEPROM_SW_REVD_OFFSET);
	DMESG("usValue is 0x%x\n",usValue);
	//3Read AntennaDiversity

	// SW Antenna Diversity.
	if ((usValue & EEPROM_SW_AD_MASK) != EEPROM_SW_AD_ENABLE)
		priv->EEPROMSwAntennaDiversity = false;
	else
		priv->EEPROMSwAntennaDiversity = true;

	// Default Antenna to use.
	if ((usValue & EEPROM_DEF_ANT_MASK) != EEPROM_DEF_ANT_1)
		priv->EEPROMDefaultAntenna1 = false;
	else
		priv->EEPROMDefaultAntenna1 = true;

	if( priv->RegSwAntennaDiversityMechanism == 0 ) // Auto
		/* 0: default from EEPROM. */
		priv->bSwAntennaDiverity = priv->EEPROMSwAntennaDiversity;
	else
		/* 1:disable antenna diversity, 2: enable antenna diversity. */
		priv->bSwAntennaDiverity = ((priv->RegSwAntennaDiversityMechanism == 1)? false : true);

	if (priv->RegDefaultAntenna == 0)
		/* 0: default from EEPROM. */
		priv->bDefaultAntenna1 = priv->EEPROMDefaultAntenna1;
	else
		/* 1: main, 2: aux. */
		priv->bDefaultAntenna1 = ((priv->RegDefaultAntenna== 2) ? true : false);

	/* rtl8185 can calc plcp len in HW.*/
	priv->hw_plcp_len = 1;

	priv->plcp_preamble_mode = 2;
	/*the eeprom type is stored in RCR register bit #6 */
	if (RCR_9356SEL & read_nic_dword(dev, RCR))
		priv->epromtype=EPROM_93c56;
	else
		priv->epromtype=EPROM_93c46;

	dev->dev_addr[0]=eprom_read(dev,MAC_ADR) & 0xff;
	dev->dev_addr[1]=(eprom_read(dev,MAC_ADR) & 0xff00)>>8;
	dev->dev_addr[2]=eprom_read(dev,MAC_ADR+1) & 0xff;
	dev->dev_addr[3]=(eprom_read(dev,MAC_ADR+1) & 0xff00)>>8;
	dev->dev_addr[4]=eprom_read(dev,MAC_ADR+2) & 0xff;
	dev->dev_addr[5]=(eprom_read(dev,MAC_ADR+2) & 0xff00)>>8;

	for(i=1,j=0; i<14; i+=2,j++){
		word = eprom_read(dev,EPROM_TXPW_CH1_2 + j);
		priv->chtxpwr[i]=word & 0xff;
		priv->chtxpwr[i+1]=(word & 0xff00)>>8;
	}
	if(priv->card_8185){
		for(i=1,j=0; i<14; i+=2,j++){
			word = eprom_read(dev,EPROM_TXPW_OFDM_CH1_2 + j);
			priv->chtxpwr_ofdm[i]=word & 0xff;
			priv->chtxpwr_ofdm[i+1]=(word & 0xff00)>>8;
		}
	}

	//3Read crystal calibtration and thermal meter indication on 87SE.

	// By SD3 SY's request. Added by Roger, 2007.12.11.

	tmpu16 = eprom_read(dev, EEPROM_RSV>>1);

		// Crystal calibration for Xin and Xout resp.
		priv->XtalCal_Xout = tmpu16 & EEPROM_XTAL_CAL_XOUT_MASK; // 0~7.5pF
		priv->XtalCal_Xin = (tmpu16 & EEPROM_XTAL_CAL_XIN_MASK)>>4; // 0~7.5pF
		if((tmpu16 & EEPROM_XTAL_CAL_ENABLE)>>12)
			priv->bXtalCalibration = true;

		// Thermal meter reference indication.
		priv->ThermalMeter =  (u8)((tmpu16 & EEPROM_THERMAL_METER_MASK)>>8);
		if((tmpu16 & EEPROM_THERMAL_METER_ENABLE)>>13)
			priv->bTxPowerTrack = true;

	word = eprom_read(dev,EPROM_TXPW_BASE);
	priv->cck_txpwr_base = word & 0xf;
	priv->ofdm_txpwr_base = (word>>4) & 0xf;

	version = eprom_read(dev,EPROM_VERSION);
	DMESG("EEPROM version %x",version);
	if( (!priv->card_8185) && version < 0x0101){
		DMESG ("EEPROM version too old, assuming defaults");
		DMESG ("If you see this message *plase* send your \
DMESG output to andreamrl@tiscali.it THANKS");
		priv->digphy=1;
		priv->antb=0;
		priv->diversity=1;
		priv->cs_treshold=0xc;
		priv->rcr_csense=1;
		priv->rf_chip=RFCHIPID_PHILIPS;
	}else{
		if(!priv->card_8185){
			u8 rfparam = eprom_read(dev,RF_PARAM);
			DMESG("RfParam: %x",rfparam);

			priv->digphy = rfparam & (1<<RF_PARAM_DIGPHY_SHIFT) ? 0:1;
			priv->antb =  rfparam & (1<<RF_PARAM_ANTBDEFAULT_SHIFT) ? 1:0;

			priv->rcr_csense = (rfparam & RF_PARAM_CARRIERSENSE_MASK) >>
					RF_PARAM_CARRIERSENSE_SHIFT;

			priv->diversity =
				(read_nic_byte(dev,CONFIG2)&(1<<CONFIG2_ANTENNA_SHIFT)) ? 1:0;
		}else{
			priv->rcr_csense = 3;
		}

		priv->cs_treshold = (eprom_read(dev,ENERGY_TRESHOLD)&0xff00) >>8;

		priv->rf_chip = 0xff & eprom_read(dev,RFCHIPID);
	}

	priv->rf_chip = RF_ZEBRA4;
	priv->rf_sleep = rtl8225z4_rf_sleep;
	priv->rf_wakeup = rtl8225z4_rf_wakeup;
	DMESGW("**PLEASE** REPORT SUCCESSFUL/UNSUCCESSFUL TO Realtek!");

	priv->rf_close = rtl8225z2_rf_close;
	priv->rf_init = rtl8225z2_rf_init;
	priv->rf_set_chan = rtl8225z2_rf_set_chan;
	priv->rf_set_sens = NULL;

	if(!priv->card_8185){
		if(priv->antb)
			DMESG ("Antenna B is default antenna");
		else
			DMESG ("Antenna A is default antenna");

		if(priv->diversity)
			DMESG ("Antenna diversity is enabled");
		else
			DMESG("Antenna diversity is disabled");

		DMESG("Carrier sense %d",priv->rcr_csense);
	}

	if (0!=alloc_rx_desc_ring(dev, priv->rxbuffersize, priv->rxringcount))
		return -ENOMEM;

	if (0!=alloc_tx_desc_ring(dev, priv->txbuffsize, priv->txringcount,
				  TX_MANAGEPRIORITY_RING_ADDR))
		return -ENOMEM;

	if (0!=alloc_tx_desc_ring(dev, priv->txbuffsize, priv->txringcount,
				 TX_BKPRIORITY_RING_ADDR))
		return -ENOMEM;

	if (0!=alloc_tx_desc_ring(dev, priv->txbuffsize, priv->txringcount,
				 TX_BEPRIORITY_RING_ADDR))
		return -ENOMEM;

	if (0!=alloc_tx_desc_ring(dev, priv->txbuffsize, priv->txringcount,
				  TX_VIPRIORITY_RING_ADDR))
		return -ENOMEM;

	if (0!=alloc_tx_desc_ring(dev, priv->txbuffsize, priv->txringcount,
				  TX_VOPRIORITY_RING_ADDR))
		return -ENOMEM;

	if (0!=alloc_tx_desc_ring(dev, priv->txbuffsize, priv->txringcount,
				  TX_HIGHPRIORITY_RING_ADDR))
		return -ENOMEM;

	if (0!=alloc_tx_desc_ring(dev, priv->txbuffsize, priv->txbeaconcount,
				  TX_BEACON_RING_ADDR))
		return -ENOMEM;

	if(!priv->card_8185){
		if(read_nic_byte(dev, CONFIG0) & (1<<CONFIG0_WEP40_SHIFT))
			DMESG ("40-bit WEP is supported in hardware");
		else
			DMESG ("40-bit WEP is NOT supported in hardware");

		if(read_nic_byte(dev,CONFIG0) & (1<<CONFIG0_WEP104_SHIFT))
			DMESG ("104-bit WEP is supported in hardware");
		else
			DMESG ("104-bit WEP is NOT supported in hardware");
	}
#if !defined(SA_SHIRQ)
        if(request_irq(dev->irq, (void *)rtl8180_interrupt, IRQF_SHARED, dev->name, dev)){
#else
        if(request_irq(dev->irq, (void *)rtl8180_interrupt, SA_SHIRQ, dev->name, dev)){
#endif
                DMESGE("Error allocating IRQ %d",dev->irq);
                return -1;
	}else{
		priv->irq=dev->irq;
		DMESG("IRQ %d",dev->irq);
	}

	return 0;
}

void rtl8180_no_hw_wep(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	if (!priv->card_8185) {
		u8 security;

		security  = read_nic_byte(dev, SECURITY);
		security &=~(1<<SECURITY_WEP_TX_ENABLE_SHIFT);
		security &=~(1<<SECURITY_WEP_RX_ENABLE_SHIFT);

		write_nic_byte(dev, SECURITY, security);
	}
}

void rtl8180_set_hw_wep(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	u8 pgreg;
	u8 security;
	u32 key0_word4;

	pgreg=read_nic_byte(dev, PGSELECT);
	write_nic_byte(dev, PGSELECT, pgreg &~ (1<<PGSELECT_PG_SHIFT));

	key0_word4 = read_nic_dword(dev, KEY0+4+4+4);
	key0_word4 &= ~ 0xff;
	key0_word4 |= priv->key0[3]& 0xff;
	write_nic_dword(dev,KEY0,(priv->key0[0]));
	write_nic_dword(dev,KEY0+4,(priv->key0[1]));
	write_nic_dword(dev,KEY0+4+4,(priv->key0[2]));
	write_nic_dword(dev,KEY0+4+4+4,(key0_word4));

	security  = read_nic_byte(dev,SECURITY);
	security |= (1<<SECURITY_WEP_TX_ENABLE_SHIFT);
	security |= (1<<SECURITY_WEP_RX_ENABLE_SHIFT);
	security &= ~ SECURITY_ENCRYP_MASK;
	security |= (SECURITY_ENCRYP_104<<SECURITY_ENCRYP_SHIFT);

	write_nic_byte(dev, SECURITY, security);

	DMESG("key %x %x %x %x",read_nic_dword(dev,KEY0+4+4+4),
	      read_nic_dword(dev,KEY0+4+4),read_nic_dword(dev,KEY0+4),
	      read_nic_dword(dev,KEY0));
}


void rtl8185_rf_pins_enable(struct net_device *dev)
{
//	u16 tmp;
//	tmp = read_nic_word(dev, RFPinsEnable);
	write_nic_word(dev, RFPinsEnable, 0x1fff);// | tmp);
}

void rtl8185_set_anaparam2(struct net_device *dev, u32 a)
{
	u8 conf3;

	rtl8180_set_mode(dev, EPROM_CMD_CONFIG);

	conf3 = read_nic_byte(dev, CONFIG3);
	write_nic_byte(dev, CONFIG3, conf3 | (1<<CONFIG3_ANAPARAM_W_SHIFT));
	write_nic_dword(dev, ANAPARAM2, a);

	conf3 = read_nic_byte(dev, CONFIG3);
	write_nic_byte(dev, CONFIG3, conf3 &~(1<<CONFIG3_ANAPARAM_W_SHIFT));
	rtl8180_set_mode(dev, EPROM_CMD_NORMAL);
}

void rtl8180_set_anaparam(struct net_device *dev, u32 a)
{
	u8 conf3;

	rtl8180_set_mode(dev, EPROM_CMD_CONFIG);

	conf3 = read_nic_byte(dev, CONFIG3);
	write_nic_byte(dev, CONFIG3, conf3 | (1<<CONFIG3_ANAPARAM_W_SHIFT));
	write_nic_dword(dev, ANAPARAM, a);

	conf3 = read_nic_byte(dev, CONFIG3);
	write_nic_byte(dev, CONFIG3, conf3 &~(1<<CONFIG3_ANAPARAM_W_SHIFT));
	rtl8180_set_mode(dev, EPROM_CMD_NORMAL);
}

void rtl8185_tx_antenna(struct net_device *dev, u8 ant)
{
	write_nic_byte(dev, TX_ANTENNA, ant);
	force_pci_posting(dev);
	mdelay(1);
}

void rtl8185_write_phy(struct net_device *dev, u8 adr, u32 data)
{
	u32 phyw;

	adr |= 0x80;

	phyw= ((data<<8) | adr);

	// Note that, we must write 0xff7c after 0x7d-0x7f to write BB register.
	write_nic_byte(dev, 0x7f, ((phyw & 0xff000000) >> 24));
	write_nic_byte(dev, 0x7e, ((phyw & 0x00ff0000) >> 16));
	write_nic_byte(dev, 0x7d, ((phyw & 0x0000ff00) >> 8));
	write_nic_byte(dev, 0x7c, ((phyw & 0x000000ff) ));

	/* this is ok to fail when we write AGC table. check for AGC table might be
	 * done by masking with 0x7f instead of 0xff
	 */
	//if(phyr != (data&0xff)) DMESGW("Phy write timeout %x %x %x", phyr, data,adr);
}

inline void write_phy_ofdm (struct net_device *dev, u8 adr, u32 data)
{
	data = data & 0xff;
	rtl8185_write_phy(dev, adr, data);
}

void write_phy_cck (struct net_device *dev, u8 adr, u32 data)
{
	data = data & 0xff;
	rtl8185_write_phy(dev, adr, data | 0x10000);
}

/* 70*3 = 210 ms
 * I hope this is enougth
 */
#define MAX_PHY 70
void write_phy(struct net_device *dev, u8 adr, u8 data)
{
	u32 phy;
	int i;

	phy = 0xff0000;
	phy |= adr;
	phy |= 0x80; /* this should enable writing */
	phy |= (data<<8);

	//PHY_ADR, PHY_R and PHY_W  are contig and treated as one dword
	write_nic_dword(dev,PHY_ADR, phy);

	phy= 0xffff00;
	phy |= adr;

	write_nic_dword(dev,PHY_ADR, phy);
	for(i=0;i<MAX_PHY;i++){
		phy=read_nic_dword(dev,PHY_ADR);
		phy= phy & 0xff0000;
		phy= phy >> 16;
		if(phy == data){ //SUCCESS!
			force_pci_posting(dev);
			mdelay(3); //random value
			return;
		}else{
			force_pci_posting(dev);
			mdelay(3); //random value
		}
	}
	DMESGW ("Phy writing %x %x failed!", adr,data);
}

void rtl8185_set_rate(struct net_device *dev)
{
	int i;
	u16 word;
	int basic_rate,min_rr_rate,max_rr_rate;

	basic_rate = ieeerate2rtlrate(240);
	min_rr_rate = ieeerate2rtlrate(60);
	max_rr_rate = ieeerate2rtlrate(240);

	write_nic_byte(dev, RESP_RATE,
			max_rr_rate<<MAX_RESP_RATE_SHIFT| min_rr_rate<<MIN_RESP_RATE_SHIFT);

	word  = read_nic_word(dev, BRSR);
	word &= ~BRSR_MBR_8185;

	for(i=0;i<=basic_rate;i++)
		word |= (1<<i);

	write_nic_word(dev, BRSR, word);
}

void rtl8180_adapter_start(struct net_device *dev)
{
        struct r8180_priv *priv = ieee80211_priv(dev);
	u32 anaparam;
	u16 word;
	u8 config3;

	rtl8180_rtx_disable(dev);
	rtl8180_reset(dev);

	/* enable beacon timeout, beacon TX ok and err
	 * LP tx ok and err, HP TX ok and err, NP TX ok and err,
	 * RX ok and ERR, and GP timer */
	priv->irq_mask = 0x6fcf;

	priv->dma_poll_mask = 0;

	rtl8180_beacon_tx_disable(dev);

	if(priv->card_type == CARDBUS ){
		config3=read_nic_byte(dev, CONFIG3);
		write_nic_byte(dev,CONFIG3,config3 | CONFIG3_FuncRegEn);
		write_nic_word(dev,FEMR, FEMR_INTR | FEMR_WKUP | FEMR_GWAKE |
			read_nic_word(dev, FEMR));
	}
	rtl8180_set_mode(dev, EPROM_CMD_CONFIG);
	write_nic_dword(dev, MAC0, ((u32*)dev->dev_addr)[0]);
	write_nic_word(dev, MAC4, ((u32*)dev->dev_addr)[1] & 0xffff );
	rtl8180_set_mode(dev, EPROM_CMD_NORMAL);

	rtl8180_update_msr(dev);

	if(!priv->card_8185){
		anaparam  = eprom_read(dev,EPROM_ANAPARAM_ADDRLWORD);
		anaparam |= eprom_read(dev,EPROM_ANAPARAM_ADDRHWORD)<<16;

		rtl8180_set_anaparam(dev,anaparam);
	}
	/* These might be unnecessary since we do in rx_enable / tx_enable */
	fix_rx_fifo(dev);
	fix_tx_fifo(dev);

	rtl8180_set_mode(dev, EPROM_CMD_CONFIG);

	/*
	   The following is very strange. seems to be that 1 means test mode,
	   but we need to acknolwledges the nic when a packet is ready
	   altought we set it to 0
	*/

	write_nic_byte(dev,
		       CONFIG2, read_nic_byte(dev,CONFIG2) &~\
		       (1<<CONFIG2_DMA_POLLING_MODE_SHIFT));
	//^the nic isn't in test mode
	if(priv->card_8185)
			write_nic_byte(dev,
		       CONFIG2, read_nic_byte(dev,CONFIG2)|(1<<4));

	rtl8180_set_mode(dev,EPROM_CMD_NORMAL);

	write_nic_dword(dev,INT_TIMEOUT,0);

	if(!priv->card_8185)
	{
		/*
		experimental - this might be needed to calibrate AGC,
		anyway it shouldn't hurt
		*/
		write_nic_byte(dev, CONFIG5,
			read_nic_byte(dev, CONFIG5) | (1<<AGCRESET_SHIFT));
		read_nic_byte(dev, CONFIG5);
		udelay(15);
		write_nic_byte(dev, CONFIG5,
			read_nic_byte(dev, CONFIG5) &~ (1<<AGCRESET_SHIFT));
	}else{
		write_nic_byte(dev, WPA_CONFIG, 0);
		//write_nic_byte(dev, TESTR, 0xd);
	}

	rtl8180_no_hw_wep(dev);

	if(priv->card_8185){
		rtl8185_set_rate(dev);
		write_nic_byte(dev, RATE_FALLBACK, 0x81);
	}else{
		word  = read_nic_word(dev, BRSR);
		word &= ~BRSR_MBR;
		word &= ~BRSR_BPLCP;
		word |= ieeerate2rtlrate(priv->ieee80211->basic_rate);
		word |= 0x0f;
		write_nic_word(dev, BRSR, word);
	}

	if(priv->card_8185){
		write_nic_byte(dev, GP_ENABLE,read_nic_byte(dev, GP_ENABLE) & ~(1<<6));

		//FIXME cfg 3 ClkRun enable - isn't it ReadOnly ?
		rtl8180_set_mode(dev, EPROM_CMD_CONFIG);
		write_nic_byte(dev,CONFIG3, read_nic_byte(dev, CONFIG3)
			       | (1 << CONFIG3_CLKRUN_SHIFT));
		rtl8180_set_mode(dev, EPROM_CMD_NORMAL);
	}

	priv->rf_init(dev);

	if(priv->rf_set_sens != NULL)
		priv->rf_set_sens(dev,priv->sens);
	rtl8180_irq_enable(dev);

	netif_start_queue(dev);
}

/* this configures registers for beacon tx and enables it via
 * rtl8180_beacon_tx_enable(). rtl8180_beacon_tx_disable() might
 * be used to stop beacon transmission
 */
void rtl8180_start_tx_beacon(struct net_device *dev)
{
	u16 word;

	DMESG("Enabling beacon TX");
	rtl8180_prepare_beacon(dev);
	rtl8180_irq_disable(dev);
	rtl8180_beacon_tx_enable(dev);

	word = read_nic_word(dev, AtimWnd) &~ AtimWnd_AtimWnd;
	write_nic_word(dev, AtimWnd,word);// word |=

	word  = read_nic_word(dev, BintrItv);
	word &= ~BintrItv_BintrItv;
	word |= 1000;/*priv->ieee80211->current_network.beacon_interval *
		((priv->txbeaconcount > 1)?(priv->txbeaconcount-1):1);
	// FIXME: check if correct ^^ worked with 0x3e8;
	*/
	write_nic_word(dev, BintrItv, word);

	rtl8180_set_mode(dev, EPROM_CMD_NORMAL);

        rtl8185b_irq_enable(dev);
}

static struct net_device_stats *rtl8180_stats(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	return &priv->ieee80211->stats;
}
//
// Change current and default preamble mode.
// 2005.01.06, by rcnjko.
//
bool
MgntActSet_802_11_PowerSaveMode(
	struct r8180_priv *priv,
	RT_PS_MODE		rtPsMode
)
{
	// Currently, we do not change power save mode on IBSS mode.
	if(priv->ieee80211->iw_mode == IW_MODE_ADHOC)
		return false;

	priv->ieee80211->ps = rtPsMode;

	return true;
}

void LeisurePSEnter(struct r8180_priv *priv)
{
	if (priv->bLeisurePs) {
		if (priv->ieee80211->ps == IEEE80211_PS_DISABLED)
			MgntActSet_802_11_PowerSaveMode(priv, IEEE80211_PS_MBCAST|IEEE80211_PS_UNICAST);//IEEE80211_PS_ENABLE
	}
}

void LeisurePSLeave(struct r8180_priv *priv)
{
	if (priv->bLeisurePs) {
		if (priv->ieee80211->ps != IEEE80211_PS_DISABLED)
			MgntActSet_802_11_PowerSaveMode(priv, IEEE80211_PS_DISABLED);
	}
}

void rtl8180_hw_wakeup_wq (struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ieee80211_device *ieee = container_of(dwork,struct ieee80211_device,hw_wakeup_wq);
	struct net_device *dev = ieee->dev;

	rtl8180_hw_wakeup(dev);
}

void rtl8180_hw_sleep_wq (struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
        struct ieee80211_device *ieee = container_of(dwork,struct ieee80211_device,hw_sleep_wq);
        struct net_device *dev = ieee->dev;

        rtl8180_hw_sleep_down(dev);
}

static void MgntLinkKeepAlive(struct r8180_priv *priv )
{
	if (priv->keepAliveLevel == 0)
		return;

	if(priv->ieee80211->state == IEEE80211_LINKED)
	{
		//
		// Keep-Alive.
		//

		if ( (priv->keepAliveLevel== 2) ||
			(priv->link_detect.LastNumTxUnicast == priv->NumTxUnicast &&
			priv->link_detect.LastNumRxUnicast == priv->ieee80211->NumRxUnicast )
			)
		{
			priv->link_detect.IdleCount++;

			//
			// Send a Keep-Alive packet packet to AP if we had been idle for a while.
			//
			if(priv->link_detect.IdleCount >= ((KEEP_ALIVE_INTERVAL / CHECK_FOR_HANG_PERIOD)-1) )
			{
				priv->link_detect.IdleCount = 0;
				ieee80211_sta_ps_send_null_frame(priv->ieee80211, false);
			}
		}
		else
		{
			priv->link_detect.IdleCount = 0;
		}
		priv->link_detect.LastNumTxUnicast = priv->NumTxUnicast;
		priv->link_detect.LastNumRxUnicast = priv->ieee80211->NumRxUnicast;
	}
}

static u8 read_acadapter_file(char *filename);

void rtl8180_watch_dog(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	bool bEnterPS = false;
	bool bBusyTraffic = false;
	u32 TotalRxNum = 0;
	u16 SlotIndex = 0;
	u16 i = 0;
	if(priv->ieee80211->actscanning == false){
		if((priv->ieee80211->iw_mode != IW_MODE_ADHOC) && (priv->ieee80211->state == IEEE80211_NOLINK) && (priv->ieee80211->beinretry == false) && (priv->eRFPowerState == eRfOn)){
			IPSEnter(dev);
		}
	}
	//YJ,add,080828,for link state check
	if((priv->ieee80211->state == IEEE80211_LINKED) && (priv->ieee80211->iw_mode == IW_MODE_INFRA)){
		SlotIndex = (priv->link_detect.SlotIndex++) % priv->link_detect.SlotNum;
		priv->link_detect.RxFrameNum[SlotIndex] = priv->ieee80211->NumRxDataInPeriod + priv->ieee80211->NumRxBcnInPeriod;
		for( i=0; i<priv->link_detect.SlotNum; i++ )
			TotalRxNum+= priv->link_detect.RxFrameNum[i];

		if(TotalRxNum == 0){
			priv->ieee80211->state = IEEE80211_ASSOCIATING;
			queue_work(priv->ieee80211->wq, &priv->ieee80211->associate_procedure_wq);
		}
	}

	//YJ,add,080828,for KeepAlive
	MgntLinkKeepAlive(priv);

	//YJ,add,080828,for LPS
#ifdef ENABLE_LPS
	if (priv->PowerProfile == POWER_PROFILE_BATTERY)
		priv->bLeisurePs = true;
	else if (priv->PowerProfile == POWER_PROFILE_AC) {
		LeisurePSLeave(priv);
		priv->bLeisurePs= false;
	}

	if(priv->ieee80211->state == IEEE80211_LINKED){
		priv->link_detect.NumRxOkInPeriod = priv->ieee80211->NumRxDataInPeriod;
		//printk("TxOk=%d RxOk=%d\n", priv->link_detect.NumTxOkInPeriod, priv->link_detect.NumRxOkInPeriod);
		if(	priv->link_detect.NumRxOkInPeriod> 666 ||
			priv->link_detect.NumTxOkInPeriod> 666 ) {
			bBusyTraffic = true;
		}
		if(((priv->link_detect.NumRxOkInPeriod + priv->link_detect.NumTxOkInPeriod) > 8)
			|| (priv->link_detect.NumRxOkInPeriod > 2)) {
			bEnterPS= false;
		} else
			bEnterPS= true;

		if (bEnterPS)
			LeisurePSEnter(priv);
		else
			LeisurePSLeave(priv);
	} else
		LeisurePSLeave(priv);
#endif
	priv->link_detect.bBusyTraffic = bBusyTraffic;
	priv->link_detect.NumRxOkInPeriod = 0;
	priv->link_detect.NumTxOkInPeriod = 0;
	priv->ieee80211->NumRxDataInPeriod = 0;
	priv->ieee80211->NumRxBcnInPeriod = 0;
}

int _rtl8180_up(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	priv->up=1;

	DMESG("Bringing up iface");
	rtl8185b_adapter_start(dev);
	rtl8185b_rx_enable(dev);
	rtl8185b_tx_enable(dev);
	if(priv->bInactivePs){
		if(priv->ieee80211->iw_mode == IW_MODE_ADHOC)
			IPSLeave(dev);
	}
#ifdef RATE_ADAPT
       timer_rate_adaptive((unsigned long)dev);
#endif
	watch_dog_adaptive((unsigned long)dev);
#ifdef SW_ANTE
        if(priv->bSwAntennaDiverity)
			SwAntennaDiversityTimerCallback(dev);
#endif
	ieee80211_softmac_start_protocol(priv->ieee80211);

	return 0;
}

int rtl8180_open(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	int ret;

	down(&priv->wx_sem);
	ret = rtl8180_up(dev);
	up(&priv->wx_sem);
	return ret;
}

int rtl8180_up(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	if (priv->up == 1) return -1;

	return _rtl8180_up(dev);
}

int rtl8180_close(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	int ret;

	down(&priv->wx_sem);
	ret = rtl8180_down(dev);
	up(&priv->wx_sem);

	return ret;
}

int rtl8180_down(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	if (priv->up == 0)
		return -1;

	priv->up=0;

	ieee80211_softmac_stop_protocol(priv->ieee80211);
	/* FIXME */
	if (!netif_queue_stopped(dev))
		netif_stop_queue(dev);
	rtl8180_rtx_disable(dev);
	rtl8180_irq_disable(dev);
	del_timer_sync(&priv->watch_dog_timer);
	del_timer_sync(&priv->rateadapter_timer);
	cancel_delayed_work(&priv->ieee80211->rate_adapter_wq);
	cancel_delayed_work(&priv->ieee80211->hw_wakeup_wq);
	cancel_delayed_work(&priv->ieee80211->hw_sleep_wq);
	cancel_delayed_work(&priv->ieee80211->hw_dig_wq);
	cancel_delayed_work(&priv->ieee80211->tx_pw_wq);
	del_timer_sync(&priv->SwAntennaDiversityTimer);
	SetZebraRFPowerState8185(dev,eRfOff);
	memset(&(priv->ieee80211->current_network),0,sizeof(struct ieee80211_network));
	priv->ieee80211->state = IEEE80211_NOLINK;
	return 0;
}

void rtl8180_restart_wq(struct work_struct *work)
{
	struct r8180_priv *priv = container_of(work, struct r8180_priv, reset_wq);
	struct net_device *dev = priv->dev;

	down(&priv->wx_sem);

	rtl8180_commit(dev);

	up(&priv->wx_sem);
}

void rtl8180_restart(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	schedule_work(&priv->reset_wq);
}

void rtl8180_commit(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);

	if (priv->up == 0)
		return ;

	del_timer_sync(&priv->watch_dog_timer);
	del_timer_sync(&priv->rateadapter_timer);
	cancel_delayed_work(&priv->ieee80211->rate_adapter_wq);
	cancel_delayed_work(&priv->ieee80211->hw_wakeup_wq);
	cancel_delayed_work(&priv->ieee80211->hw_sleep_wq);
	cancel_delayed_work(&priv->ieee80211->hw_dig_wq);
	cancel_delayed_work(&priv->ieee80211->tx_pw_wq);
	del_timer_sync(&priv->SwAntennaDiversityTimer);
	ieee80211_softmac_stop_protocol(priv->ieee80211);
	rtl8180_irq_disable(dev);
	rtl8180_rtx_disable(dev);
	_rtl8180_up(dev);
}

static void r8180_set_multicast(struct net_device *dev)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	short promisc;

	promisc = (dev->flags & IFF_PROMISC) ? 1:0;

	if (promisc != priv->promisc)
		rtl8180_restart(dev);

	priv->promisc = promisc;
}

int r8180_set_mac_adr(struct net_device *dev, void *mac)
{
	struct r8180_priv *priv = ieee80211_priv(dev);
	struct sockaddr *addr = mac;

	down(&priv->wx_sem);

	memcpy(dev->dev_addr, addr->sa_data, ETH_ALEN);

	if(priv->ieee80211->iw_mode == IW_MODE_MASTER)
		memcpy(priv->ieee80211->current_network.bssid, dev->dev_addr, ETH_ALEN);

	if (priv->up) {
		rtl8180_down(dev);
		rtl8180_up(dev);
	}

	up(&priv->wx_sem);

	return 0;
}

/* based on ipw2200 driver */
int rtl8180_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct r8180_priv *priv = (struct r8180_priv *)ieee80211_priv(dev);
	struct iwreq *wrq = (is part of rtl) rq;
	int ret=-1;

	switch (cmd) {
	case RTL_IOCTL_WPA_SUPPLICANT:
		ret = ieee80211_wpa_supplicant_ioctl(priv->it>
   Re, &wrq->u.datahis iscaurn0.1
;
	defaultl@t
   Part-EOPNOT <an;
	}

 are based on the GPL}

static const i OpenSnet_device_opsce d8180_netdev driv=drea.ndo_open		=vert of trive, are  dstopr skeleton
 closeom Patriget_ltekshenke & Andrarts lomon.

tx_timeouthenke & Andrrestartlomon.

doe termhenke & Andrr.

 lomon.

cet_multicast_listenken theedeg rtl fromcodet of l818ac_addressd Youngver areRSSI rlomon.

validateSI ca	= eth   Some ideaare based change_mtuhenkroweng rtr inc39too.c dver _xmitel PL (General?) w,
};eaarts dr v 0_of thnitkeleton
 pci_probeci OpenShe
 d on*pdev,
				 
GeneAinux.
   APar's projse)

d *id)
{
	unsigned long ioeas  = 0e)

 OpenS areojecis *roje= NULL my attempction to
  *to
 185 and Ru8 us an in mek corp. forcialpmem  We (,t intelen routinflags  PoDMESG("Configuring chiplessources")by Mif(ks goenableoGeneadd(t Au) ){
	Mariusz Failed to def DU PCI es alsNT_Pleton
 he oIOPLOpenS's ptbsd 'ster#inclu;"
0_rtl8dedmae "rk0_rtl, 0xftend 00ULL8225.h" /* Rinuxistenelloclud Radio fonend **/5.h" /*   /*18alloc_L (Genera(sizeofswrapperd unort, a)cludif (!rtl825.h" /* R "NOMEMlude, anth>

thanks OR_I   T addto
 GPL (Genera =pt toffifinede "_VEude "r8180Trthisn EEPROrPCI_VENSET_NETDEV_DEV PCI, &incl->fn#incPCIR_ID_BELKIN
	#deif
#ifndef NDfineDincl = incl

stautinrfaced_tbciless RX_DerfaceNK
	#de1clude Wries. = {GeneDOR_I{
 es.
EK,
     .vendort.h" PCI_VENDOR_ID_RE    deEK,
       211/dot(,
       de& IORESOURCE_MEM)nrtl8inux/sysregion #1 not a MMIOdif

  {
, abortingw.h"
#goto fail180.h"
S_UL! request_utine = PCor    tdata
   Wries.
ello of tMODULE_NAME      subf thisI_VENDOR_IDdor = ,
   ed!_icenp in,VENDOR_ID},
 r help i(wer managemen)ioremap_nocache(routindor = 0,
         Re/(ei0,
    wlan%d";
stat.drivid Ye <linux/sysver     .subdevic";
static    1180.h"
86
#evinit          .su; // shared interfacenelsp inx3fendi_devicee +d rtl8180_p_REALTEK .sub0)qMacAddr(a,b)		(nd *fndef read0_93fig_byteNK
	#deon05, &alsoclude "rwrite4] && (a)[5]==(b,src) ? 10 )
.& (~0x04
   [0]==(bi18x p1ls = inux -DOR_ID_rc)[1 thaols = he official
r&eleton
 he officia"rare ->wire    _handler[4]=(s of twxLICENSE("_defdes)[1]=(type=ARPHRD_ETHERsrc    (
atchdogcial
I = HZ*3(a)[added by david woo, 2007.12.13ic i     ondata inameINK 0xifnux ) < 0){

   A   /   /*18nux/sysOops:devior R al    y taken! Try.
*/wlan%%d...\n_data nagemR = ", S_Id"ata IPTION("Lm = ung rtr);
mode;TL81850 /ce =   r aref BBnit PCI_!=ek R         Initializat    .subdev= 0;altek dr v 0drivnenetif_carrier_offindif

st    rc)[,(des)[me," Net i80_hLKINr"Linnit_onn the(E(pcnux/sysDO|S_IW  Ndi completedS_IWUSR of the0;
USR);: .subd]==(b)[0]ff;

#! S_Id"GO|S_IWUSR);
hwseqnumiounmap( (voh>

). Zero=default"ON("Linl)[3]wlan%d";
statce = 0x8199,
   ards");
MODthorsbig t[1] && (a)[2]==(b)[2] &  . Sti}uencenumberDESCp," Tf int, GPLrqstatic	free_irqc lo
ealt_device_iep," rtl81=n my	}
 v 0__deifndef r8PCI_VENON("LincdisncluMMY_RX
0_er a22wwep," ,_para dTry toload .subde02.11 h)[1] && (a)[DLINKpci_dvid 11 header s

st_DLINRors of thohardoes eexid Yoault");eelesmoviswrappeks goes ecinclrs of /* Carclude "ieen.h>

 my attem799
MMY_RX

RTL8185_VENDORPpci_dev *pdeatic ivene "r81);
un    }
};

nux , , S_Id=dtek dris parthe
 f this id rt("Lis dri");
MODpartnePARM_ loc(hspendect Adown lteke)
{I");

rfres Sa_device *s is parreseUGO|S_ce *mdelay(100_suspifYIdev,ealteWUSR) = 0;
screeharp],(d%d",e(is par
   Ndpdev,
0x3ffver are bhe
   Ndout_pci_s_suspendject Autrx_desc_k.
*_device *pdev,ed ot_pcout_tl8180_suspbers. Zero=his dridev, pmvicege_t state)wwealese, ch>

usetructw of WEPpdev);
	mask lll brokenf (deY_ID,vailf DUfficall cards(pdev);
	pci_s_t state
MODULls," C
	stru bitmask this detachhors.
(pdev)iver.
s8180_suspend(struct Rea;ealtek drvoidev->neexit _messai_uct n}

/* fun withcial
built-inBELKIN
	#d( ((ck...inclexternhose to thanks cryptoS_IRUGtruc);ERR "%s:truct_def DUpend(st fadells.honlessume\n",
_suspv->name);

		rettkipalls.h;
	}

	pci_resors.dev->nux t_potisca*
	 *
	strPL par	he
 ess ore_devic(t Auonfig/ccm * Suspend/R

	pclessetscial
e "rcatuszekatETRY_pace, so wchoove>ndo	 * re-dist rtlial
RweY_TIMEOUT register (0x41) to keep PCI Tx re_sus
	 * from inr to
    pciprclso.pdev)
{
	strmodule* Suspend/Rs of v ts o%d=denali.to thanks 

		retlls.hhar* idors of3 CPprintk(KERN_ume\nevfron40, &valhis 1/do( .subdev%ve t,ts ose)ig thankt to
  }dwordsable_config_dword(pion sTIMEO(val & 0x0000ff00) != 0)
	 havecpMac_ PCI T_running
pdev, the nfig_>netdev_ff>ndoffonfig1/dot1_PARMrunning   Th
		d= 0,
out;triesTIMEO->netdev_ops->ndo_open)
		dev->netdev_ops->ndo_openO|S_IWer artif_device_attach(dev);
out:
	return 0;
}

static struct pci_d_suspess ->netdev_ops->ndo_open)
		dev->netdev_ops->ndo_opendev, PCI_Dtif_device_attach(dev);
out:
	return 0=(srcen)
		devINFO "\n S_IRUkernel, PCI_D0agem_device \
/_device5],(des)WLANesume(r = pce(_shut netAuth

u8Copyright (c)t>")4-20(desA    . Merellfail11 hheader_param
{
	harpic voits of this *Wv);
	pc me\nnsions vermem_v drivWIRELESS_EXTtaticevice(pdev);ic voi.ng w/i_ge/ RTL8. NYIis par }
};
 PCI_D(80_s)[4],180_sdev, ,
            Norc)[5_h founailed Otate(pder are bsn 0;
}
oid equb((u8*) >ndo_st t Auect Aua(pdev);evice *
	 * from s of   rame);

		(p PCI_D0*)the P +x)s 210 +x);nninis part   return rex,u32 y   Reo keep PCI Tx re PCIs
	 * el(y,(u8struct net_detriesng wiudelay(20);
}

void wit onlypci_elay(20);
}

void wurn erx0000turn reExi|S_I    partructint x,u3try_wake_queuet_device->ndo_stop)
		d,o
	 *prirs of er management   .dby	short enoughle th)
		deta(pdev);
	if (dev->x_sus"

#include "ieend)ev, inMEOUT iteb(y,(ucispin_locku8*)save(&I");

txgdd R,   .ddev),EPROM_CMD) = check_nic_net_dent, scdrives ofa)[5_regundd Regs)    ore(pdsetoid srxk.
*int x,u(hwwenet_device datato thanks einlf
#ivoiDOR_ID_BELKIN 0ice *de
	uing(de20);x_isrid netceo_stoposiverint x,u32 y),te

	n,rr0x0000rvicensable thev);

	ne> are b    }rupt( v 0irq,ate(pd*ased o,rtl8180	u32 *);
}

//roc_ virtual eas USR);
pic v   Par *r818s
{
	r *page, chbegin;//ff;

#of out_/		  off_t offset, inicv   Pnic po    }of0_proc =icena(pdeni,u32 y)
{
     dephysicatar - v 0es.
= );

ountprina) SR);
*eot max=(b)[faultn spaca(pde(b)[_ni;
	/*x6.hr ffseinclt of okpend32ce *s sincem inset DMA Card*/ <= cooffc_byd_coj,i	agem(ihdval & 032 y)
ts ov->f (!n.txretry++   Prony

u360601 p_devi *dev);
;
te(pduct net_device_st32 y)t +x)te(pc_dword(stMANAGE_PRIORITYver e = s= i++, ntxmaeadbgregig
		s dummp t -	/* ,"\ndev,of =rt,ek curn len;
}

int_t o*
		,(u8=     4x ",runnidriveTX_ snpritf( += s_RING_ADDRn 0;
I_VENdumr_tx_t __dv);
int dma get reak;word(stBKnc int pr+ len;dump trn len;bk

int
	*e	  of1;; n  writlen;a;
	int    Pcurage, char *a;
	intx,u32 y)
{
     LL;

e, chriority);
BKage, ch
	*eoetateats_hwoff_t offset, i_t oa;
	int ata(poff_Sourfset, iit count,
			  int *eof, void e
	int len;

	/* Thi0a)
{
	int (y,(r80;

	*eof 
Realtek dr8180_	  off_t offserx int count,
			  in*sWUSR);
		  off_t offset, int count,
			  in*
   Th void *data)
{
	struVIt count,
			  int *eof, voidvidata)
{
	int len = 0;

	*eof =Enet_(eturn len;
}

staticRC Errroc_get_stats_rx(char *page, char **VIart,
			  off_t offset, int count,
			  intRC Erry: %lu\n"
		"RX CRC EO Err0-500)priv->stats.rxcrceroC Err500-10riv->stats.rxcrcerresetp Err>stats.rxicverr
		*eof Vrmin,
priv->st	*eof f GP offs.rxint,Oats_tx(char *pagerrstats_tx(char *pagcrc off_t of void *data)
{
	struHC Err		priv->stats.rxcrcerrmahdata)
{
	int len = 0;

	*eof  0xffseturn len;
}

stati (struroc_get_stats_rx(char *page, char **HIGHe + len, count - len,
		"RX OK: %lu\n"
		" (stru void *data)
{   addGO|S_IW_stats_rx(ch)GO|S_IWUSnt x,u32 y)
{
   _st struct /* R
ev);
	p= 0;prou32*) ((,(u8-CI_VENdum) + y(20)v->staar* if(_curr<off_gis&& X Erv >acon O|| Thiv < _t o)) ||sets "TX ats.TX Kpriv->stats.TX &&		"TXent proc_gEOUT ;
outW(",(u8has losttats_rx(_data ts.txlpokint;
	es.
+=
statit count,
			  int *eoable the if_devico_stop(deu8 y)
v =_susp/**eof;
	lvx.h"isablevicriptors betweenisabl_currhwsesablnic,ng wibutNY_IDsabl lenenlyturn reeamrlaram(ch (aram(exth>

be txed).
*/pG("parampreviousrentce = _devrtl(mce *dbe inpci_c(20)??)
	 > "
	forle_rint proc_get_
;c_evice *heade/ 8 /4;
	hen;
s.txnp- v->stat/8dd Rtl8heade>= h8185	j0xff&r_p- 16 &&elseee802evice *+N out_pctxdev,t *eof,1 -hoi_ge	j-=2lu\n"
j<0) jct p
0; i(i=0;i<j;i++)
	al = P((	  in)180_1<<31));
}*data)
{
>
    Relpri&(0x10regien))dev,USisable the CurrRv->sCnt +v->s16)  Rels_tx(de0xoc_entff
   R = PNY!32 y)
{8*)dt - leNumTxOkTotal++eturn legdir(u8*stat	remd rtoc_enttryBytes("sta);
		*s.txn+3)try"regits-tffftif_de	  int=		  int&~;

stati_dev)pcic_by.device = /8"wlaoc_get_stats_rx(c-1);
		_t o=s dump
of =cr = ndo_r+=8int etr/*ealtekarius.txbte(p

	erc_ensabllast certaihwweTXed180_M(or ateturn32 yce *dops->ndo_roc=) packet.180_MTe802dev, p(ifn32 ycefully own *eoof *dev, in8180

	ns180_MIRar *it) {
	AMEparamFDE("Unabl_stats_rx(clen, co*priv ee802iv *)t+privoesn'stertter: it wilrd(p}void  *->namheess rueatemove_rts-h. Anywayle_rno morCPU sto iarea)
{prXedta(pdemory(y,(k occouint xae_pro
	/start +x)if (	}	priv->stats.txbeacon,
		pr, char **start,(y,( =	strustruct pci__pcacage +toce *de-disabstate, c S_srn rev);
_ops-ya)
{
txhpernet/r81 confo we     p

   yv->duct pci_deoc_e(ackYI");


BELKIN 0e(st				(u8*,*)de*)dedata)
{
	strucs dump tesets Realtek dr+ len, cowet_stat a;
	int lennt x,u32 y)
{
        Relentry("e the rxx\n",
		 x", the PCI cRC Err		priv->siv->stats.rxcICc inr_dev);
		x_paramFREG | Smidstats_tx(ch *)ieee80211_pri iny("stats-tx", S_IFREG | v->name);
	}


name)it>
   Relpriy("stats-tx", S_IFR ares.txretry,
		priv->stats.txbeacon,
		priv->stint x,u32 y)
{
     rq_wq80_interwork_ attemp* offdevice *dev)

statd_ off proprk#def  wrdreastrucG(rs, d my attemp o thanks o_stop)
/S_IRl8180_intero_IRUGO80/%s/reg*>nam*
  FIXME:ev);
v ifm incanci_chsrom stcon>namreadn("Unab,ig thankS_IFREG | 				  , _AUTH_OR("wtop)
dev_ops->ndo_stop)
		dev-S_IF#endiatic(struct s_rx(chint tats.txbeacon,
,c st}

	len;
	r_es pci cons_rx(ntry _susple_paoose_99
#e */
t
  dapt 0;
s net_*device *dev)->ndo_stop)
		dev-v);
void rtl8180_sta)7

	if(;
	mb(proc

irq 0;

	_v_ops->ndodir_entry *rtl8180_proc = NULL;

static; i <priv = n <= ma		fo*rtl8nta= (s/*rfacshouldts oif (IRQ_NONE,021132 y)now let me keepdev);
roc_, dev->nai_sdif

DUdev =0)ffer)->buf=bHANDLED: %l count - len, "%2x ",
			ry: thge + len, atdd R//ISR: 4,srcs; i <ats_rx(char *page, char ISR)p th&185 an;IntrM 813ffcpa)[3ar *page, charISR,xt;
des,s31/doto
	 *situstrucaltek drnriv *	shintsts-t +x) !ESGE(isabs.txlpokint;
	len += snprintf(ile(tmp->next!=(*bufs of this) = (ev);
	tu	/*
/*
 mp offrobablyemovean safely) = (*bufferuf;
	buffe=*buf = (*bdi_tdev_/%s\o a *tmp->ndlemsmove_pam(hwwe(y,(aet +x */
#00ff00/* HW_susppp=(b)[roc_ct buffer >next->ne->dma=dm0xffv *)ieee802n<= macr,int entry0;

	*0remove_odev, ine(pde);
	rn   Coinittmppci_serunonerr, dev0ff00r8180_priv *)ieee80211_priv(dev);
	struct ext=er,int v=priv->pdev;

	if (eint+mp,*iste&oc(s_TimeOut);
	_
		pEL)) =185 an){
 net/rIntdr(de,>nextdma
		re}roc_BDOK);
	t - le;
		re	lbe		"T
	tmp = DMA_FROMDEVICEesum		kt __->nextbuv);
	initerr	}
	while(!= *btmpMR_TMG= *b	kf int pbuf, dmaSI ca_t2 *besetstats-hbuf}
	tmA_FROMDEVICEHPESGE>ndo_op(tmp->buf);hprn 0;
	nt_r,int (buffer(uferv->name);
	,      && (a)[truct  0tmp->buf);_32 y)

	tmice =  fer(u =y(20),lenOK){ //High185 kmallotx okn_priv *	link_getect._proc_eInPeriod
statiYJ,add,080828roc_!= 0)"ASCII BUFtry,
deviP (len: %x):et_slenR DUMtruct r81len;
}
BINARY_tx_FER DURERs->ndo_opr)
		returx8180_pMP}
	struct r8180_pMBKev,le //correspond *eofo
		      "/pr}
	while(next != kil;
	u32P i<len;
int get%c",buf[i]R DUM
int get (len: %x):\n",len);		      "/pi<eof inet_devicLL) { *dev);
vif(! ,u32 y)
{
 har (pdev);

	erfac0_pTBEic vr - v 0.1
Copyright (Ctxbeacon,
	}
	while(next != *age l@ti	}
	tm=age,v->n,"\
intgpringta		tailad;
			tail = privngta;
 len)
{
	ina MereBKntf(page bepringhead;
			tailbk priv->txbep	u32* tail;
	u32Ndev,len,il;
			break;
		ca/dotendrea
	}
	while(next !=n->txbepringhead;
			tail = priv->txbepringtail;
			break;
		case VINORM		bf_t o
			head = priv->txvipringhead;
			tad;
			tailhp>txbepringtail;
			breaLxhpringheaVO priv->txviprin_IRUGO,
		
		case MANAGE_PRl->txbepringhead;
			tail = priv->txbepringtail;
			break;
		case VILOWriv->txhpringhead;
			tail = priv->txhpringtprivump t)
	ead;
			tail = priv->trruct>nami.it>
   Relpar **startasklthancic voi_priv(dev);
rx_->name)_enought_desc(struct neQoSOKseqnum_rx(char *page, char ught_desc(struct nenameredby *dev, int prev) {	struct r8180_pBcnInps->ndoint x,u32     (_ *buffv);
	int etEG | c(struct neDats-hwheadeWtdevnRXne(stheadertic in*bufIWUSR )quired;

	requdupriv(dev);
	int requiredbyte, required;

	required This is part eeeXFOVWstats_rx(char *pageoverflow= requiredbyte / (priv->txbuffsize-4);

	if (requiredbDMA_FROMDEVICEx,lens
		case MANAGE_PRI;) { net_no
		case VI_	riv->txhruct neNormalvice *dev, int prio	case ead = priv->txm02xpringhead;
			tail = }

intY:
		teturn len;
nt/r8180/%s/stats-hget_stats_rx(ail;
			breakr_priv(dev);
	int requir;

	ruct neLowsc(dev,priority));
}

void fix_tx_fifo(struct net_device *dev)
{
	struct r8180_priv *prlv = (struct r8180_priv *)ieee80rt check_nic	char ("BUGata)
{iv->pde			brReale(devv);
void s,EPROMnet/uct r8180_p2* hruct nel;
			break;
		casge, char priv Mere snprintf(pachar **star
}

void fix_tx_fifo(struct net_device *dev)
{
	struct 			break;
		case VI_ < get_cu
	 i <;
			tailrt check_tk("\ODULmp+ail = priv->txvipringtail;
			breabkp we ; i <tmp=(1<<31);
epre Bintf(page vipringhead;
			tail net_iv->txbp+=8,ove_;
	  mp-> =tmp-> &~ (v);
	i	tmp =or (tmp=priv->txvvit;
	 , i len)
{
	int i;head;
			tailtxhprin	*tmp = *tmp &~ (1<<31);txhpr are
	str osry to uI_VENinext;
		if(consistent){
			pci_free_consistent(pdtic void rurn 0;
}

vo(struct net_devirequired;

	reed to kmalloc head S_I
   Pdr.txbeaconxevice *c Sc(d(struct GPIOCO|S_IRFWorkItemCallBx,S_IF,en, count con O		  g;
	pstatic sretuv->name);
	}
iserset_sndarpdev(b)[yexx6.h"bufferta l818+functart goe;
	    ad;
			tail.ta(pdev)"/0211/ferSI cint x,u3,len); *",len)pokint;
	llen += snprintfpr TX/RXsnprint)
		rengco8 btPSev, xbkprMatuszr_deRT_RF_POWER_STATE	eRfPowerSevicToSokintbool 	bAc offlySet=false
		}		  inargv[3]evice *devs of thip = *d EEP			taPathce */etc/acpi/events/		*tmp = *.sh"txringcount;
	    ad;
	envp[]et pcHOME=/", "TERM=l<= mriv-PATH=/usr/bin:ad;
"ngtail}gcoune first 6    f__rx(ch(pci3]#if#incENABLE_LPSer_stabuf_susail;% 10t;
	sddule_retmcount;rofilet_s *dev <anapdatavice_"/v->tmp = b'The break/AC0"/prtedbyttic ingtail = prh
		case V =+1)%stats-;	if(dif
	tx\n"	//v,le		=*buf (*buoff LED bef
			toll *eoFF51[4].ci_g	//Tngtail;
			.pringd;
		s_rx(char *,src)priv-P8180ringnmap_singl GPLt>
   Re), (1evexe& ~BIT3,C(hwsiv->Iil =oc_e *nabli 4us suggeiver areJongi

u38-01-16statu

stat4foint x,uHW r	tai On/Off accorv->txbepridevalue(!pr	uct ne(ddr(deonrin	unt;
	 ;
 =opringttine ves of GPLt>
   CONFIGc struc

static;n	ieee = 0, n+ckge, to_*8 =riv = }
void | rx_f 0; i <->txringcountring;		}
c_180_ */
&(tmp4((de eRfOn :unctixbeaw", S_Ievice *dev, icom->bHw		tairiv-== s-hw)ts_txv->device)+s of GPrx=unctiif w(pr);
	ifct netuf=rhe tnsiste+){
		tmp=S_IR	.id_v, gcount;
	taile2) =1<<31)*)devregisngcoun,rxbuf>rxbuffersize;
		*tmp +2= (1<rxbufferiv(de		*tm= *tmp &~attac", pmplete=1;
	|;
			tarxbuffercount)
		*tmp|iv->rx=int  we hsigned priv->rxpriv-w", S_I	}

	priv->r(tmp=priv
}MgntActSettailcountt>
   v->rxring)+(priv->, RF_CHANGE_BY_HW <pringiv->do upom t->namUIriv->ushar Q			ta4c_conf9GO|S_Istats
  FIXME: check if we can use so	tmp=dev, buffer=priv->rxbufferhead *derx31fron29,
	0x241, 0x2, 0x21, 0x2101, 0x2ze;

1 (1<"RFOFFpriv->
			t0x1efrone, 0x1e, , 0x1d,regisodule_paraTL81855b 0x1ea 0x1e9 0x19, , 0x//LL;
+= 2;
et_deReason, 0x21, 0x0x12, 0x11, 0x0f,
	0x0e,,
	0x200 0x0a, 00x0c, 0, 0x04,N1f 0x1ex00
};, 0x1e, 0x1e,
	0x1d,  9,
	0x118 0x1d0x1e41, 04, 0x63, 0x1a, 0x19, 0x19,
	0x11808, 0x06, 0x04, 0x01,
};

unsi

unsigned char STRENGTH_MAP}0x61, 0x63x4a, 1, 0x621, 06 0x5a5ze;

0v->txringcount;
	;x00
};

unsigned char STRENGTH_MAPze;

x5d, vid Yoc, 0x4a, 0x48, 0x46, 0x44, 0x41, call_usRF cde
   ai(c, 0x3a, 0x37,,14, 0tailpringheef DU thutdown (su8bkpring;
	prionringD);

	priingSR);
tail 1;
	r_VENDORevice *dev, 8180_ PCI T0d, 0x0c, 0x);0d0x5a0c
	 * ct net_device *deuct ne);
