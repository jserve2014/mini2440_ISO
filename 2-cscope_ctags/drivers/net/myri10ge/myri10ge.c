/*************************************************************************
 * myri10ge.c: Myricom Myri-10G Ethernet driver.
 *
 * Copyright (C) 2005 - 2009 Myricom, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Myricom, Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * If the eeprom on your board is not recent enough, you will need to get a
 * newer firmware image at:
 *   http://www.myri.com/scs/download-Myri10GE.html
 *
 * Contact Information:
 *   <help@myri.com>
 *   Myricom, Inc., 325N Santa Anita Avenue, Arcadia, CA 91006
 *************************************************************************/

#include <linux/tcp.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/inet_lro.h>
#include <linux/dca.h>
#include <linux/ip.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/ethtool.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <linux/crc32.h>
#include <linux/moduleparam.h>
#include <linux/io.h>
#include <linux/log2.h>
#include <net/checksum.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/processor.h>
#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include "myri10ge_mcp.h"
#include "myri10ge_mcp_gen_header.h"

#define MYRI10GE_VERSION_STR "1.5.1-1.451"

MODULE_DESCRIPTION("Myricom 10G driver (10GbE)");
MODULE_AUTHOR("Maintainer: help@myri.com");
MODULE_VERSION(MYRI10GE_VERSION_STR);
MODULE_LICENSE("Dual BSD/GPL");

#define MYRI10GE_MAX_ETHER_MTU 9014

#define MYRI10GE_ETH_STOPPED 0
#define MYRI10GE_ETH_STOPPING 1
#define MYRI10GE_ETH_STARTING 2
#define MYRI10GE_ETH_RUNNING 3
#define MYRI10GE_ETH_OPEN_FAILED 4

#define MYRI10GE_EEPROM_STRINGS_SIZE 256
#define MYRI10GE_MAX_SEND_DESC_TSO ((65536 / 2048) * 2)
#define MYRI10GE_MAX_LRO_DESCRIPTORS 8
#define MYRI10GE_LRO_MAX_PKTS 64

#define MYRI10GE_NO_CONFIRM_DATA htonl(0xffffffff)
#define MYRI10GE_NO_RESPONSE_RESULT 0xffffffff

#define MYRI10GE_ALLOC_ORDER 0
#define MYRI10GE_ALLOC_SIZE ((1 << MYRI10GE_ALLOC_ORDER) * PAGE_SIZE)
#define MYRI10GE_MAX_FRAGS_PER_FRAME (MYRI10GE_MAX_ETHER_MTU/MYRI10GE_ALLOC_SIZE + 1)

#define MYRI10GE_MAX_SLICES 32

struct myri10ge_rx_buffer_state {
	struct page *page;
	int page_offset;
	 DECLARE_PCI_UNMAP_ADDR(bus)
	 DECLARE_PCI_UNMAP_LEN(len)
};

struct myri10ge_tx_buffer_state {
	struct sk_buff *skb;
	int last;
	 DECLARE_PCI_UNMAP_ADDR(bus)
	 DECLARE_PCI_UNMAP_LEN(len)
};

struct myri10ge_cmd {
	u32 data0;
	u32 data1;
	u32 data2;
};

struct myri10ge_rx_buf {
	struct mcp_kreq_ether_recv __iomem *lanai;	/* lanai ptr for recv ring */
	struct mcp_kreq_ether_recv *shadow;	/* host shadow of recv ring */
	struct myri10ge_rx_buffer_state *info;
	struct page *page;
	dma_addr_t bus;
	int page_offset;
	int cnt;
	int fill_cnt;
	int alloc_fail;
	int mask;		/* number of rx slots -1 */
	int watchdog_needed;
};

struct myri10ge_tx_buf {
	struct mcp_kreq_ether_send __iomem *lanai;	/* lanai ptr for sendq */
	__be32 __iomem *send_go;	/* "go" doorbell ptr */
	__be32 __iomem *send_stop;	/* "stop" doorbell ptr */
	struct mcp_kreq_ether_send *req_list;	/* host shadow of sendq */
	char *req_bytes;
	struct myri10ge_tx_buffer_state *info;
	int mask;		/* number of transmit slots -1  */
	int req ____cacheline_aligned;	/* transmit slots submitted     */
	int pkt_start;		/* packets started */
	int stop_queue;
	int linearized;
	int done ____cacheline_aligned;	/* transmit slots completed     */
	int pkt_done;		/* packets completed */
	int wake_queue;
	int queue_active;
};

struct myri10ge_rx_done {
	struct mcp_slot *entry;
	dma_addr_t bus;
	int cnt;
	int idx;
	struct net_lro_mgr lro_mgr;
	struct net_lro_desc lro_desc[MYRI10GE_MAX_LRO_DESCRIPTORS];
};

struct myri10ge_slice_netstats {
	unsigned long rx_packets;
	unsigned long tx_packets;
	unsigned long rx_bytes;
	unsigned long tx_bytes;
	unsigned long rx_dropped;
	unsigned long tx_dropped;
};

struct myri10ge_slice_state {
	struct myri10ge_tx_buf tx;	/* transmit ring        */
	struct myri10ge_rx_buf rx_small;
	struct myri10ge_rx_buf rx_big;
	struct myri10ge_rx_done rx_done;
	struct net_device *dev;
	struct napi_struct napi;
	struct myri10ge_priv *mgp;
	struct myri10ge_slice_netstats stats;
	__be32 __iomem *irq_claim;
	struct mcp_irq_data *fw_stats;
	dma_addr_t fw_stats_bus;
	int watchdog_tx_done;
	int watchdog_tx_req;
	int watchdog_rx_done;
#ifdef CONFIG_MYRI10GE_DCA
	int cached_dca_tag;
	int cpu;
	__be32 __iomem *dca_tag;
#endif
	char irq_desc[32];
};

struct myri10ge_priv {
	struct myri10ge_slice_state *ss;
	int tx_boundary;	/* boundary transmits cannot cross */
	int num_slices;
	int running;		/* running?             */
	int csum_flag;		/* rx_csums?            */
	int small_bytes;
	int big_bytes;
	int max_intr_slots;
	struct net_device *dev;
	struct net_device_stats stats;
	spinlock_t stats_lock;
	u8 __iomem *sram;
	int sram_size;
	unsigned long board_span;
	unsigned long iomem_base;
	__be32 __iomem *irq_deassert;
	char *mac_addr_string;
	struct mcp_cmd_response *cmd;
	dma_addr_t cmd_bus;
	struct pci_dev *pdev;
	int msi_enabled;
	int msix_enabled;
	struct msix_entry *msix_vectors;
#ifdef CONFIG_MYRI10GE_DCA
	int dca_enabled;
#endif
	u32 link_state;
	unsigned int rdma_tags_available;
	int intr_coal_delay;
	__be32 __iomem *intr_coal_delay_ptr;
	int mtrr;
	int wc_enabled;
	int down_cnt;
	wait_queue_head_t down_wq;
	struct work_struct watchdog_work;
	struct timer_list watchdog_timer;
	int watchdog_resets;
	int watchdog_pause;
	int pause;
	char *fw_name;
	char eeprom_strings[MYRI10GE_EEPROM_STRINGS_SIZE];
	char *product_code_string;
	char fw_version[128];
	int fw_ver_major;
	int fw_ver_minor;
	int fw_ver_tiny;
	int adopted_rx_filter_bug;
	u8 mac_addr[6];		/* eeprom mac address */
	unsigned long serial_number;
	int vendor_specific_offset;
	int fw_multicast_support;
	unsigned long features;
	u32 max_tso6;
	u32 read_dma;
	u32 write_dma;
	u32 read_write_dma;
	u32 link_changes;
	u32 msg_enable;
	unsigned int board_number;
	int rebooted;
};

static char *myri10ge_fw_unaligned = "myri10ge_ethp_z8e.dat";
static char *myri10ge_fw_aligned = "myri10ge_eth_z8e.dat";
static char *myri10ge_fw_rss_unaligned = "myri10ge_rss_ethp_z8e.dat";
static char *myri10ge_fw_rss_aligned = "myri10ge_rss_eth_z8e.dat";

static char *myri10ge_fw_name = NULL;
module_param(myri10ge_fw_name, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(myri10ge_fw_name, "Firmware image name");

#define MYRI10GE_MAX_BOARDS 8
static char *myri10ge_fw_names[MYRI10GE_MAX_BOARDS] =
    {[0 ... (MYRI10GE_MAX_BOARDS - 1)] = NULL };
module_param_array_named(myri10ge_fw_names, myri10ge_fw_names, charp, NULL,
			 0444);
MODULE_PARM_DESC(myri10ge_fw_name, "Firmware image names per board");

static int myri10ge_ecrc_enable = 1;
module_param(myri10ge_ecrc_enable, int, S_IRUGO);
MODULE_PARM_DESC(myri10ge_ecrc_enable, "Enable Extended CRC on PCI-E");

static int myri10ge_small_bytes = -1;	/* -1 == auto */
module_param(myri10ge_small_bytes, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(myri10ge_small_bytes, "Threshold of small packets");

static int myri10ge_msi = 1;	/* enable msi by default */
module_param(myri10ge_msi, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(myri10ge_msi, "Enable Message Signalled Interrupts");

static int myri10ge_intr_coal_delay = 75;
module_param(myri10ge_intr_coal_delay, int, S_IRUGO);
MODULE_PARM_DESC(myri10ge_intr_coal_delay, "Interrupt coalescing delay");

static int myri10ge_flow_control = 1;
module_param(myri10ge_flow_control, int, S_IRUGO);
MODULE_PARM_DESC(myri10ge_flow_control, "Pause parameter");

static int myri10ge_deassert_wait = 1;
module_param(myri10ge_deassert_wait, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(myri10ge_deassert_wait,
		 "Wait when deasserting legacy interrupts");

static int myri10ge_force_firmware = 0;
module_param(myri10ge_force_firmware, int, S_IRUGO);
MODULE_PARM_DESC(myri10ge_force_firmware,
		 "Force firmware to assume aligned completions");

static int myri10ge_initial_mtu = MYRI10GE_MAX_ETHER_MTU - ETH_HLEN;
module_param(myri10ge_initial_mtu, int, S_IRUGO);
MODULE_PARM_DESC(myri10ge_initial_mtu, "Initial MTU");

static int myri10ge_napi_weight = 64;
module_param(myri10ge_napi_weight, int, S_IRUGO);
MODULE_PARM_DESC(myri10ge_napi_weight, "Set NAPI weight");

static int myri10ge_watchdog_timeout = 1;
module_param(myri10ge_watchdog_timeout, int, S_IRUGO);
MODULE_PARM_DESC(myri10ge_watchdog_timeout, "Set watchdog timeout");

static int myri10ge_max_irq_loops = 1048576;
module_param(myri10ge_max_irq_loops, int, S_IRUGO);
MODULE_PARM_DESC(myri10ge_max_irq_loops,
		 "Set stuck legacy IRQ detection threshold");

#define MYRI10GE_MSG_DEFAULT NETIF_MSG_LINK

static int myri10ge_debug = -1;	/* defaults above */
module_param(myri10ge_debug, int, 0);
MODULE_PARM_DESC(myri10ge_debug, "Debug level (0=none,...,16=all)");

static int myri10ge_lro_max_pkts = MYRI10GE_LRO_MAX_PKTS;
module_param(myri10ge_lro_max_pkts, int, S_IRUGO);
MODULE_PARM_DESC(myri10ge_lro_max_pkts,
		 "Number of LRO packets to be aggregated");

static int myri10ge_fill_thresh = 256;
module_param(myri10ge_fill_thresh, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(myri10ge_fill_thresh, "Number of empty rx slots allowed");

static int myri10ge_reset_recover = 1;

static int myri10ge_max_slices = 1;
module_param(myri10ge_max_slices, int, S_IRUGO);
MODULE_PARM_DESC(myri10ge_max_slices, "Max tx/rx queues");

static int myri10ge_rss_hash = MXGEFW_RSS_HASH_TYPE_SRC_PORT;
module_param(myri10ge_rss_hash, int, S_IRUGO);
MODULE_PARM_DESC(myri10ge_rss_hash, "Type of RSS hashing to do");

static int myri10ge_dca = 1;
module_param(myri10ge_dca, int, S_IRUGO);
MODULE_PARM_DESC(myri10ge_dca, "Enable DCA if possible");

#define MYRI10GE_FW_OFFSET 1024*1024
#define MYRI10GE_HIGHPART_TO_U32(X) \
(sizeof (X) == 8) ? ((u32)((u64)(X) >> 32)) : (0)
#define MYRI10GE_LOWPART_TO_U32(X) ((u32)(X))

#define myri10ge_pio_copy(to,from,size) __iowrite64_copy(to,from,size/8)

static void myri10ge_set_multicast_list(struct net_device *dev);
static netdev_tx_t myri10ge_sw_tso(struct sk_buff *skb,
					 struct net_device *dev);

static inline void put_be32(__be32 val, __be32 __iomem * p)
{
	__raw_writel((__force __u32) val, (__force void __iomem *)p);
}

static struct net_device_stats *myri10ge_get_stats(struct net_device *dev);

static int
myri10ge_send_cmd(struct myri10ge_priv *mgp, u32 cmd,
		  struct myri10ge_cmd *data, int atomic)
{
	struct mcp_cmd *buf;
	char buf_bytes[sizeof(*buf) + 8];
	struct mcp_cmd_response *response = mgp->cmd;
	char __iomem *cmd_addr = mgp->sram + MXGEFW_ETH_CMD;
	u32 dma_low, dma_high, result, value;
	int sleep_total = 0;

	/* ensure buf is aligned to 8 bytes */
	buf = (struct mcp_cmd *)ALIGN((unsigned long)buf_bytes, 8);

	buf->data0 = htonl(data->data0);
	buf->data1 = htonl(data->data1);
	buf->data2 = htonl(data->data2);
	buf->cmd = htonl(cmd);
	dma_low = MYRI10GE_LOWPART_TO_U32(mgp->cmd_bus);
	dma_high = MYRI10GE_HIGHPART_TO_U32(mgp->cmd_bus);

	buf->response_addr.low = htonl(dma_low);
	buf->response_addr.high = htonl(dma_high);
	response->result = htonl(MYRI10GE_NO_RESPONSE_RESULT);
	mb();
	myri10ge_pio_copy(cmd_addr, buf, sizeof(*buf));

	/* wait up to 15ms. Longest command is the DMA benchmark,
	 * which is capped at 5ms, but runs from a timeout handler
	 * that runs every 7.8ms. So a 15ms timeout leaves us with
	 * a 2.2ms margin
	 */
	if (atomic) {
		/* if atomic is set, do not sleep,
		 * and try to get the completion quickly
		 * (1ms will be enough for those commands) */
		for (sleep_total = 0;
		     sleep_total < 1000
		     && response->result == htonl(MYRI10GE_NO_RESPONSE_RESULT);
		     sleep_total += 10) {
			udelay(10);
			mb();
		}
	} else {
		/* use msleep for most command */
		for (sleep_total = 0;
		     sleep_total < 15
		     && response->result == htonl(MYRI10GE_NO_RESPONSE_RESULT);
		     sleep_total++)
			msleep(1);
	}

	result = ntohl(response->result);
	value = ntohl(response->data);
	if (result != MYRI10GE_NO_RESPONSE_RESULT) {
		if (result == 0) {
			data->data0 = value;
			return 0;
		} else if (result == MXGEFW_CMD_UNKNOWN) {
			return -ENOSYS;
		} else if (result == MXGEFW_CMD_ERROR_UNALIGNED) {
			return -E2BIG;
		} else if (result == MXGEFW_CMD_ERROR_RANGE &&
			   cmd == MXGEFW_CMD_ENABLE_RSS_QUEUES &&
			   (data->
			    data1 & MXGEFW_SLICE_ENABLE_MULTIPLE_TX_QUEUES) !=
			   0) {
			return -ERANGE;
		} else {
			dev_err(&mgp->pdev->dev,
				"command %d failed, result = %d\n",
				cmd, result);
			return -ENXIO;
		}
	}

	dev_err(&mgp->pdev->dev, "command %d timed out, result = %d\n",
		cmd, result);
	return -EAGAIN;
}

/*
 * The eeprom strings on the lanaiX have the format
 * SN=x\0
 * MAC=x:x:x:x:x:x\0
 * PT:ddd mmm xx xx:xx:xx xx\0
 * PV:ddd mmm xx xx:xx:xx xx\0
 */
static int myri10ge_read_mac_addr(struct myri10ge_priv *mgp)
{
	char *ptr, *limit;
	int i;

	ptr = mgp->eeprom_strings;
	limit = mgp->eeprom_strings + MYRI10GE_EEPROM_STRINGS_SIZE;

	while (*ptr != '\0' && ptr < limit) {
		if (memcmp(ptr, "MAC=", 4) == 0) {
			ptr += 4;
			mgp->mac_addr_string = ptr;
			for (i = 0; i < 6; i++) {
				if ((ptr + 2) > limit)
					goto abort;
				mgp->mac_addr[i] =
				    simple_strtoul(ptr, &ptr, 16);
				ptr += 1;
			}
		}
		if (memcmp(ptr, "PC=", 3) == 0) {
			ptr += 3;
			mgp->product_code_string = ptr;
		}
		if (memcmp((const void *)ptr, "SN=", 3) == 0) {
			ptr += 3;
			mgp->serial_number = simple_strtoul(ptr, &ptr, 10);
		}
		while (ptr < limit && *ptr++) ;
	}

	return 0;

abort:
	dev_err(&mgp->pdev->dev, "failed to parse eeprom_strings\n");
	return -ENXIO;
}

/*
 * Enable or disable periodic RDMAs from the host to make certain
 * chipsets resend dropped PCIe messages
 */

static void myri10ge_dummy_rdma(struct myri10ge_priv *mgp, int enable)
{
	char __iomem *submit;
	__be32 buf[16] __attribute__ ((__aligned__(8)));
	u32 dma_low, dma_high;
	int i;

	/* clear confirmation addr */
	mgp->cmd->data = 0;
	mb();

	/* send a rdma command to the PCIe engine, and wait for the
	 * response in the confirmation address.  The firmware should
	 * write a -1 there to indicate it is alive and well
	 */
	dma_low = MYRI10GE_LOWPART_TO_U32(mgp->cmd_bus);
	dma_high = MYRI10GE_HIGHPART_TO_U32(mgp->cmd_bus);

	buf[0] = htonl(dma_high);	/* confirm addr MSW */
	buf[1] = htonl(dma_low);	/* confirm addr LSW */
	buf[2] = MYRI10GE_NO_CONFIRM_DATA;	/* confirm data */
	buf[3] = htonl(dma_high);	/* dummy addr MSW */
	buf[4] = htonl(dma_low);	/* dummy addr LSW */
	buf[5] = htonl(enable);	/* enable? */

	submit = mgp->sram + MXGEFW_BOOT_DUMMY_RDMA;

	myri10ge_pio_copy(submit, &buf, sizeof(buf));
	for (i = 0; mgp->cmd->data != MYRI10GE_NO_CONFIRM_DATA && i < 20; i++)
		msleep(1);
	if (mgp->cmd->data != MYRI10GE_NO_CONFIRM_DATA)
		dev_err(&mgp->pdev->dev, "dummy rdma %s failed\n",
			(enable ? "enable" : "disable"));
}

static int
myri10ge_validate_firmware(struct myri10ge_priv *mgp,
			   struct mcp_gen_header *hdr)
{
	struct device *dev = &mgp->pdev->dev;

	/* check firmware type */
	if (ntohl(hdr->mcp_type) != MCP_TYPE_ETH) {
		dev_err(dev, "Bad firmware type: 0x%x\n", ntohl(hdr->mcp_type));
		return -EINVAL;
	}

	/* save firmware version for ethtool */
	strncpy(mgp->fw_version, hdr->version, sizeof(mgp->fw_version));

	sscanf(mgp->fw_version, "%d.%d.%d", &mgp->fw_ver_major,
	       &mgp->fw_ver_minor, &mgp->fw_ver_tiny);

	if (!(mgp->fw_ver_major == MXGEFW_VERSION_MAJOR
	      && mgp->fw_ver_minor == MXGEFW_VERSION_MINOR)) {
		dev_err(dev, "Found firmware version %s\n", mgp->fw_version);
		dev_err(dev, "Driver needs %d.%d\n", MXGEFW_VERSION_MAJOR,
			MXGEFW_VERSION_MINOR);
		return -EINVAL;
	}
	return 0;
}

static int myri10ge_load_hotplug_firmware(struct myri10ge_priv *mgp, u32 * size)
{
	unsigned crc, reread_crc;
	const struct firmware *fw;
	struct device *dev = &mgp->pdev->dev;
	unsigned char *fw_readback;
	struct mcp_gen_header *hdr;
	size_t hdr_offset;
	int status;
	unsigned i;

	if ((status = request_firmware(&fw, mgp->fw_name, dev)) < 0) {
		dev_err(dev, "Unable to load %s firmware image via hotplug\n",
			mgp->fw_name);
		status = -EINVAL;
		goto abort_with_nothing;
	}

	/* check size */

	if (fw->size >= mgp->sram_size - MYRI10GE_FW_OFFSET ||
	    fw->size < MCP_HEADER_PTR_OFFSET + 4) {
		dev_err(dev, "Firmware size invalid:%d\n", (int)fw->size);
		status = -EINVAL;
		goto abort_with_fw;
	}

	/* check id */
	hdr_offset = ntohl(*(__be32 *) (fw->data + MCP_HEADER_PTR_OFFSET));
	if ((hdr_offset & 3) || hdr_offset + sizeof(*hdr) > fw->size) {
		dev_err(dev, "Bad firmware file\n");
		status = -EINVAL;
		goto abort_with_fw;
	}
	hdr = (void *)(fw->data + hdr_offset);

	status = myri10ge_validate_firmware(mgp, hdr);
	if (status != 0)
		goto abort_with_fw;

	crc = crc32(~0, fw->data, fw->size);
	for (i = 0; i < fw->size; i += 256) {
		myri10ge_pio_copy(mgp->sram + MYRI10GE_FW_OFFSET + i,
				  fw->data + i,
				  min(256U, (unsigned)(fw->size - i)));
		mb();
		readb(mgp->sram);
	}
	fw_readback = vmalloc(fw->size);
	if (!fw_readback) {
		status = -ENOMEM;
		goto abort_with_fw;
	}
	/* corruption checking is good for parity recovery and buggy chipset */
	memcpy_fromio(fw_readback, mgp->sram + MYRI10GE_FW_OFFSET, fw->size);
	reread_crc = crc32(~0, fw_readback, fw->size);
	vfree(fw_readback);
	if (crc != reread_crc) {
		dev_err(dev, "CRC failed(fw-len=%u), got 0x%x (expect 0x%x)\n",
			(unsigned)fw->size, reread_crc, crc);
		status = -EIO;
		goto abort_with_fw;
	}
	*size = (u32) fw->size;

abort_with_fw:
	release_firmware(fw);

abort_with_nothing:
	return status;
}

static int myri10ge_adopt_running_firmware(struct myri10ge_priv *mgp)
{
	struct mcp_gen_header *hdr;
	struct device *dev = &mgp->pdev->dev;
	const size_t bytes = sizeof(struct mcp_gen_header);
	size_t hdr_offset;
	int status;

	/* find running firmware header */
	hdr_offset = swab32(readl(mgp->sram + MCP_HEADER_PTR_OFFSET));

	if ((hdr_offset & 3) || hdr_offset + sizeof(*hdr) > mgp->sram_size) {
		dev_err(dev, "Running firmware has bad header offset (%d)\n",
			(int)hdr_offset);
		return -EIO;
	}

	/* copy header of running firmware from SRAM to host memory to
	 * validate firmware */
	hdr = kmalloc(bytes, GFP_KERNEL);
	if (hdr == NULL) {
		dev_err(dev, "could not malloc firmware hdr\n");
		return -ENOMEM;
	}
	memcpy_fromio(hdr, mgp->sram + hdr_offset, bytes);
	status = myri10ge_validate_firmware(mgp, hdr);
	kfree(hdr);

	/* check to see if adopted firmware has bug where adopting
	 * it will cause broadcasts to be filtered unless the NIC
	 * is kept in ALLMULTI mode */
	if (mgp->fw_ver_major == 1 && mgp->fw_ver_minor == 4 &&
	    mgp->fw_ver_tiny >= 4 && mgp->fw_ver_tiny <= 11) {
		mgp->adopted_rx_filter_bug = 1;
		dev_warn(dev, "Adopting fw %d.%d.%d: "
			 "working around rx filter bug\n",
			 mgp->fw_ver_major, mgp->fw_ver_minor,
			 mgp->fw_ver_tiny);
	}
	return status;
}

static int myri10ge_get_firmware_capabilities(struct myri10ge_priv *mgp)
{
	struct myri10ge_cmd cmd;
	int status;

	/* probe for IPv6 TSO support */
	mgp->features = NETIF_F_SG | NETIF_F_HW_CSUM | NETIF_F_TSO;
	status = myri10ge_send_cmd(mgp, MXGEFW_CMD_GET_MAX_TSO6_HDR_SIZE,
				   &cmd, 0);
	if (status == 0) {
		mgp->max_tso6 = cmd.data0;
		mgp->features |= NETIF_F_TSO6;
	}

	status = myri10ge_send_cmd(mgp, MXGEFW_CMD_GET_RX_RING_SIZE, &cmd, 0);
	if (status != 0) {
		dev_err(&mgp->pdev->dev,
			"failed MXGEFW_CMD_GET_RX_RING_SIZE\n");
		return -ENXIO;
	}

	mgp->max_intr_slots = 2 * (cmd.data0 / sizeof(struct mcp_dma_addr));

	return 0;
}

static int myri10ge_load_firmware(struct myri10ge_priv *mgp, int adopt)
{
	char __iomem *submit;
	__be32 buf[16] __attribute__ ((__aligned__(8)));
	u32 dma_low, dma_high, size;
	int status, i;

	size = 0;
	status = myri10ge_load_hotplug_firmware(mgp, &size);
	if (status) {
		if (!adopt)
			return status;
		dev_warn(&mgp->pdev->dev, "hotplug firmware loading failed\n");

		/* Do not attempt to adopt firmware if there
		 * was a bad crc */
		if (status == -EIO)
			return status;

		status = myri10ge_adopt_running_firmware(mgp);
		if (status != 0) {
			dev_err(&mgp->pdev->dev,
				"failed to adopt running firmware\n");
			return status;
		}
		dev_info(&mgp->pdev->dev,
			 "Successfully adopted running firmware\n");
		if (mgp->tx_boundary == 4096) {
			dev_warn(&mgp->pdev->dev,
				 "Using firmware currently running on NIC"
				 ".  For optimal\n");
			dev_warn(&mgp->pdev->dev,
				 "performance consider loading optimized "
				 "firmware\n");
			dev_warn(&mgp->pdev->dev, "via hotplug\n");
		}

		mgp->fw_name = "adopted";
		mgp->tx_boundary = 2048;
		myri10ge_dummy_rdma(mgp, 1);
		status = myri10ge_get_firmware_capabilities(mgp);
		return status;
	}

	/* clear confirmation addr */
	mgp->cmd->data = 0;
	mb();

	/* send a reload command to the bootstrap MCP, and wait for the
	 *  response in the confirmation address.  The firmware should
	 * write a -1 there to indicate it is alive and well
	 */
	dma_low = MYRI10GE_LOWPART_TO_U32(mgp->cmd_bus);
	dma_high = MYRI10GE_HIGHPART_TO_U32(mgp->cmd_bus);

	buf[0] = htonl(dma_high);	/* confirm addr MSW */
	buf[1] = htonl(dma_low);	/* confirm addr LSW */
	buf[2] = MYRI10GE_NO_CONFIRM_DATA;	/* confirm data */

	/* FIX: All newest firmware should un-protect the bottom of
	 * the sram before handoff. However, the very first interfaces
	 * do not. Therefore the handoff copy must skip the first 8 bytes
	 */
	buf[3] = htonl(MYRI10GE_FW_OFFSET + 8);	/* where the code starts */
	buf[4] = htonl(size - 8);	/* length of code */
	buf[5] = htonl(8);	/* where to copy to */
	buf[6] = htonl(0);	/* where to jump to */

	submit = mgp->sram + MXGEFW_BOOT_HANDOFF;

	myri10ge_pio_copy(submit, &buf, sizeof(buf));
	mb();
	msleep(1);
	mb();
	i = 0;
	while (mgp->cmd->data != MYRI10GE_NO_CONFIRM_DATA && i < 9) {
		msleep(1 << i);
		i++;
	}
	if (mgp->cmd->data != MYRI10GE_NO_CONFIRM_DATA) {
		dev_err(&mgp->pdev->dev, "handoff failed\n");
		return -ENXIO;
	}
	myri10ge_dummy_rdma(mgp, 1);
	status = myri10ge_get_firmware_capabilities(mgp);

	return status;
}

static int myri10ge_update_mac_address(struct myri10ge_priv *mgp, u8 * addr)
{
	struct myri10ge_cmd cmd;
	int status;

	cmd.data0 = ((addr[0] << 24) | (addr[1] << 16)
		     | (addr[2] << 8) | addr[3]);

	cmd.data1 = ((addr[4] << 8) | (addr[5]));

	status = myri10ge_send_cmd(mgp, MXGEFW_SET_MAC_ADDRESS, &cmd, 0);
	return status;
}

static int myri10ge_change_pause(struct myri10ge_priv *mgp, int pause)
{
	struct myri10ge_cmd cmd;
	int status, ctl;

	ctl = pause ? MXGEFW_ENABLE_FLOW_CONTROL : MXGEFW_DISABLE_FLOW_CONTROL;
	status = myri10ge_send_cmd(mgp, ctl, &cmd, 0);

	if (status) {
		printk(KERN_ERR
		       "myri10ge: %s: Failed to set flow control mode\n",
		       mgp->dev->name);
		return status;
	}
	mgp->pause = pause;
	return 0;
}

static void
myri10ge_change_promisc(struct myri10ge_priv *mgp, int promisc, int atomic)
{
	struct myri10ge_cmd cmd;
	int status, ctl;

	ctl = promisc ? MXGEFW_ENABLE_PROMISC : MXGEFW_DISABLE_PROMISC;
	status = myri10ge_send_cmd(mgp, ctl, &cmd, atomic);
	if (status)
		printk(KERN_ERR "myri10ge: %s: Failed to set promisc mode\n",
		       mgp->dev->name);
}

static int myri10ge_dma_test(struct myri10ge_priv *mgp, int test_type)
{
	struct myri10ge_cmd cmd;
	int status;
	u32 len;
	struct page *dmatest_page;
	dma_addr_t dmatest_bus;
	char *test = " ";

	dmatest_page = alloc_page(GFP_KERNEL);
	if (!dmatest_page)
		return -ENOMEM;
	dmatest_bus = pci_map_page(mgp->pdev, dmatest_page, 0, PAGE_SIZE,
				   DMA_BIDIRECTIONAL);

	/* Run a small DMA test.
	 * The magic multipliers to the length tell the firmware
	 * to do DMA read, write, or read+write tests.  The
	 * results are returned in cmd.data0.  The upper 16
	 * bits or the return is the number of transfers completed.
	 * The lower 16 bits is the time in 0.5us ticks that the
	 * transfers took to complete.
	 */

	len = mgp->tx_boundary;

	cmd.data0 = MYRI10GE_LOWPART_TO_U32(dmatest_bus);
	cmd.data1 = MYRI10GE_HIGHPART_TO_U32(dmatest_bus);
	cmd.data2 = len * 0x10000;
	status = myri10ge_send_cmd(mgp, test_type, &cmd, 0);
	if (status != 0) {
		test = "read";
		goto abort;
	}
	mgp->read_dma = ((cmd.data0 >> 16) * len * 2) / (cmd.data0 & 0xffff);
	cmd.data0 = MYRI10GE_LOWPART_TO_U32(dmatest_bus);
	cmd.data1 = MYRI10GE_HIGHPART_TO_U32(dmatest_bus);
	cmd.data2 = len * 0x1;
	status = myri10ge_send_cmd(mgp, test_type, &cmd, 0);
	if (status != 0) {
		test = "write";
		goto abort;
	}
	mgp->write_dma = ((cmd.data0 >> 16) * len * 2) / (cmd.data0 & 0xffff);

	cmd.data0 = MYRI10GE_LOWPART_TO_U32(dmatest_bus);
	cmd.data1 = MYRI10GE_HIGHPART_TO_U32(dmatest_bus);
	cmd.data2 = len * 0x10001;
	status = myri10ge_send_cmd(mgp, test_type, &cmd, 0);
	if (status != 0) {
		test = "read/write";
		goto abort;
	}
	mgp->read_write_dma = ((cmd.data0 >> 16) * len * 2 * 2) /
	    (cmd.data0 & 0xffff);

abort:
	pci_unmap_page(mgp->pdev, dmatest_bus, PAGE_SIZE, DMA_BIDIRECTIONAL);
	put_page(dmatest_page);

	if (status != 0 && test_type != MXGEFW_CMD_UNALIGNED_TEST)
		dev_warn(&mgp->pdev->dev, "DMA %s benchmark failed: %d\n",
			 test, status);

	return status;
}

static int myri10ge_reset(struct myri10ge_priv *mgp)
{
	struct myri10ge_cmd cmd;
	struct myri10ge_slice_state *ss;
	int i, status;
	size_t bytes;
#ifdef CONFIG_MYRI10GE_DCA
	unsigned long dca_tag_off;
#endif

	/* try to send a reset command to the card to see if it
	 * is alive */
	memset(&cmd, 0, sizeof(cmd));
	status = myri10ge_send_cmd(mgp, MXGEFW_CMD_RESET, &cmd, 0);
	if (status != 0) {
		dev_err(&mgp->pdev->dev, "failed reset\n");
		return -ENXIO;
	}

	(void)myri10ge_dma_test(mgp, MXGEFW_DMA_TEST);
	/*
	 * Use non-ndis mcp_slot (eg, 4 bytes total,
	 * no toeplitz hash value returned.  Older firmware will
	 * not understand this command, but will use the correct
	 * sized mcp_slot, so we ignore error returns
	 */
	cmd.data0 = MXGEFW_RSS_MCP_SLOT_TYPE_MIN;
	(void)myri10ge_send_cmd(mgp, MXGEFW_CMD_SET_RSS_MCP_SLOT_TYPE, &cmd, 0);

	/* Now exchange information about interrupts  */

	bytes = mgp->max_intr_slots * sizeof(*mgp->ss[0].rx_done.entry);
	cmd.data0 = (u32) bytes;
	status = myri10ge_send_cmd(mgp, MXGEFW_CMD_SET_INTRQ_SIZE, &cmd, 0);

	/*
	 * Even though we already know how many slices are supported
	 * via myri10ge_probe_slices() MXGEFW_CMD_GET_MAX_RSS_QUEUES
	 * has magic side effects, and must be called after a reset.
	 * It must be called prior to calling any RSS related cmds,
	 * including assigning an interrupt queue for anything but
	 * slice 0.  It must also be called *after*
	 * MXGEFW_CMD_SET_INTRQ_SIZE, since the intrq size is used by
	 * the firmware to compute offsets.
	 */

	if (mgp->num_slices > 1) {

		/* ask the maximum number of slices it supports */
		status = myri10ge_send_cmd(mgp, MXGEFW_CMD_GET_MAX_RSS_QUEUES,
					   &cmd, 0);
		if (status != 0) {
			dev_err(&mgp->pdev->dev,
				"failed to get number of slices\n");
		}

		/*
		 * MXGEFW_CMD_ENABLE_RSS_QUEUES must be called prior
		 * to setting up the interrupt queue DMA
		 */

		cmd.data0 = mgp->num_slices;
		cmd.data1 = MXGEFW_SLICE_INTR_MODE_ONE_PER_SLICE;
		if (mgp->dev->real_num_tx_queues > 1)
			cmd.data1 |= MXGEFW_SLICE_ENABLE_MULTIPLE_TX_QUEUES;
		status = myri10ge_send_cmd(mgp, MXGEFW_CMD_ENABLE_RSS_QUEUES,
					   &cmd, 0);

		/* Firmware older than 1.4.32 only supports multiple
		 * RX queues, so if we get an error, first retry using a
		 * single TX queue before giving up */
		if (status != 0 && mgp->dev->real_num_tx_queues > 1) {
			mgp->dev->real_num_tx_queues = 1;
			cmd.data0 = mgp->num_slices;
			cmd.data1 = MXGEFW_SLICE_INTR_MODE_ONE_PER_SLICE;
			status = myri10ge_send_cmd(mgp,
						   MXGEFW_CMD_ENABLE_RSS_QUEUES,
						   &cmd, 0);
		}

		if (status != 0) {
			dev_err(&mgp->pdev->dev,
				"failed to set number of slices\n");

			return status;
		}
	}
	for (i = 0; i < mgp->num_slices; i++) {
		ss = &mgp->ss[i];
		cmd.data0 = MYRI10GE_LOWPART_TO_U32(ss->rx_done.bus);
		cmd.data1 = MYRI10GE_HIGHPART_TO_U32(ss->rx_done.bus);
		cmd.data2 = i;
		status |= myri10ge_send_cmd(mgp, MXGEFW_CMD_SET_INTRQ_DMA,
					    &cmd, 0);
	};

	status |=
	    myri10ge_send_cmd(mgp, MXGEFW_CMD_GET_IRQ_ACK_OFFSET, &cmd, 0);
	for (i = 0; i < mgp->num_slices; i++) {
		ss = &mgp->ss[i];
		ss->irq_claim =
		    (__iomem __be32 *) (mgp->sram + cmd.data0 + 8 * i);
	}
	status |= myri10ge_send_cmd(mgp, MXGEFW_CMD_GET_IRQ_DEASSERT_OFFSET,
				    &cmd, 0);
	mgp->irq_deassert = (__iomem __be32 *) (mgp->sram + cmd.data0);

	status |= myri10ge_send_cmd
	    (mgp, MXGEFW_CMD_GET_INTR_COAL_DELAY_OFFSET, &cmd, 0);
	mgp->intr_coal_delay_ptr = (__iomem __be32 *) (mgp->sram + cmd.data0);
	if (status != 0) {
		dev_err(&mgp->pdev->dev, "failed set interrupt parameters\n");
		return status;
	}
	put_be32(htonl(mgp->intr_coal_delay), mgp->intr_coal_delay_ptr);

#ifdef CONFIG_MYRI10GE_DCA
	status = myri10ge_send_cmd(mgp, MXGEFW_CMD_GET_DCA_OFFSET, &cmd, 0);
	dca_tag_off = cmd.data0;
	for (i = 0; i < mgp->num_slices; i++) {
		ss = &mgp->ss[i];
		if (status == 0) {
			ss->dca_tag = (__iomem __be32 *)
			    (mgp->sram + dca_tag_off + 4 * i);
		} else {
			ss->dca_tag = NULL;
		}
	}
#endif				/* CONFIG_MYRI10GE_DCA */

	/* reset mcp/driver shared state back to 0 */

	mgp->link_changes = 0;
	for (i = 0; i < mgp->num_slices; i++) {
		ss = &mgp->ss[i];

		memset(ss->rx_done.entry, 0, bytes);
		ss->tx.req = 0;
		ss->tx.done = 0;
		ss->tx.pkt_start = 0;
		ss->tx.pkt_done = 0;
		ss->rx_big.cnt = 0;
		ss->rx_small.cnt = 0;
		ss->rx_done.idx = 0;
		ss->rx_done.cnt = 0;
		ss->tx.wake_queue = 0;
		ss->tx.stop_queue = 0;
	}

	status = myri10ge_update_mac_address(mgp, mgp->dev->dev_addr);
	myri10ge_change_pause(mgp, mgp->pause);
	myri10ge_set_multicast_list(mgp->dev);
	return status;
}

#ifdef CONFIG_MYRI10GE_DCA
static void
myri10ge_write_dca(struct myri10ge_slice_state *ss, int cpu, int tag)
{
	ss->cpu = cpu;
	ss->cached_dca_tag = tag;
	put_be32(htonl(tag), ss->dca_tag);
}

static inline void myri10ge_update_dca(struct myri10ge_slice_state *ss)
{
	int cpu = get_cpu();
	int tag;

	if (cpu != ss->cpu) {
		tag = dca_get_tag(cpu);
		if (ss->cached_dca_tag != tag)
			myri10ge_write_dca(ss, cpu, tag);
	}
	put_cpu();
}

static void myri10ge_setup_dca(struct myri10ge_priv *mgp)
{
	int err, i;
	struct pci_dev *pdev = mgp->pdev;

	if (mgp->ss[0].dca_tag == NULL || mgp->dca_enabled)
		return;
	if (!myri10ge_dca) {
		dev_err(&pdev->dev, "dca disabled by administrator\n");
		return;
	}
	err = dca_add_requester(&pdev->dev);
	if (err) {
		if (err != -ENODEV)
			dev_err(&pdev->dev,
				"dca_add_requester() failed, err=%d\n", err);
		return;
	}
	mgp->dca_enabled = 1;
	for (i = 0; i < mgp->num_slices; i++)
		myri10ge_write_dca(&mgp->ss[i], -1, 0);
}

static void myri10ge_teardown_dca(struct myri10ge_priv *mgp)
{
	struct pci_dev *pdev = mgp->pdev;
	int err;

	if (!mgp->dca_enabled)
		return;
	mgp->dca_enabled = 0;
	err = dca_remove_requester(&pdev->dev);
}

static int myri10ge_notify_dca_device(struct device *dev, void *data)
{
	struct myri10ge_priv *mgp;
	unsigned long event;

	mgp = dev_get_drvdata(dev);
	event = *(unsigned long *)data;

	if (event == DCA_PROVIDER_ADD)
		myri10ge_setup_dca(mgp);
	else if (event == DCA_PROVIDER_REMOVE)
		myri10ge_teardown_dca(mgp);
	return 0;
}
#endif				/* CONFIG_MYRI10GE_DCA */

static inline void
myri10ge_submit_8rx(struct mcp_kreq_ether_recv __iomem * dst,
		    struct mcp_kreq_ether_recv *src)
{
	__be32 low;

	low = src->addr_low;
	src->addr_low = htonl(DMA_BIT_MASK(32));
	myri10ge_pio_copy(dst, src, 4 * sizeof(*src));
	mb();
	myri10ge_pio_copy(dst + 4, src + 4, 4 * sizeof(*src));
	mb();
	src->addr_low = low;
	put_be32(low, &dst->addr_low);
	mb();
}

static inline void myri10ge_vlan_ip_csum(struct sk_buff *skb, __wsum hw_csum)
{
	struct vlan_hdr *vh = (struct vlan_hdr *)(skb->data);

	if ((skb->protocol == htons(ETH_P_8021Q)) &&
	    (vh->h_vlan_encapsulated_proto == htons(ETH_P_IP) ||
	     vh->h_vlan_encapsulated_proto == htons(ETH_P_IPV6))) {
		skb->csum = hw_csum;
		skb->ip_summed = CHECKSUM_COMPLETE;
	}
}

static inline void
myri10ge_rx_skb_build(struct sk_buff *skb, u8 * va,
		      struct skb_frag_struct *rx_frags, int len, int hlen)
{
	struct skb_frag_struct *skb_frags;

	skb->len = skb->data_len = len;
	skb->truesize = len + sizeof(struct sk_buff);
	/* attach the page(s) */

	skb_frags = skb_shinfo(skb)->frags;
	while (len > 0) {
		memcpy(skb_frags, rx_frags, sizeof(*skb_frags));
		len -= rx_frags->size;
		skb_frags++;
		rx_frags++;
		skb_shinfo(skb)->nr_frags++;
	}

	/* pskb_may_pull is not available in irq context, but
	 * skb_pull() (for ether_pad and eth_type_trans()) requires
	 * the beginning of the packet in skb_headlen(), move it
	 * manually */
	skb_copy_to_linear_data(skb, va, hlen);
	skb_shinfo(skb)->frags[0].page_offset += hlen;
	skb_shinfo(skb)->frags[0].size -= hlen;
	skb->data_len -= hlen;
	skb->tail += hlen;
	skb_pull(skb, MXGEFW_PAD);
}

static void
myri10ge_alloc_rx_pages(struct myri10ge_priv *mgp, struct myri10ge_rx_buf *rx,
			int bytes, int watchdog)
{
	struct page *page;
	int idx;

	if (unlikely(rx->watchdog_needed && !watchdog))
		return;

	/* try to refill entire ring */
	while (rx->fill_cnt != (rx->cnt + rx->mask + 1)) {
		idx = rx->fill_cnt & rx->mask;
		if (rx->page_offset + bytes <= MYRI10GE_ALLOC_SIZE) {
			/* we can use part of previous page */
			get_page(rx->page);
		} else {
			/* we need a new page */
			page =
			    alloc_pages(GFP_ATOMIC | __GFP_COMP,
					MYRI10GE_ALLOC_ORDER);
			if (unlikely(page == NULL)) {
				if (rx->fill_cnt - rx->cnt < 16)
					rx->watchdog_needed = 1;
				return;
			}
			rx->page = page;
			rx->page_offset = 0;
			rx->bus = pci_map_page(mgp->pdev, page, 0,
					       MYRI10GE_ALLOC_SIZE,
					       PCI_DMA_FROMDEVICE);
		}
		rx->info[idx].page = rx->page;
		rx->info[idx].page_offset = rx->page_offset;
		/* note that this is the address of the start of the
		 * page */
		pci_unmap_addr_set(&rx->info[idx], bus, rx->bus);
		rx->shadow[idx].addr_low =
		    htonl(MYRI10GE_LOWPART_TO_U32(rx->bus) + rx->page_offset);
		rx->shadow[idx].addr_high =
		    htonl(MYRI10GE_HIGHPART_TO_U32(rx->bus));

		/* start next packet on a cacheline boundary */
		rx->page_offset += SKB_DATA_ALIGN(bytes);

#if MYRI10GE_ALLOC_SIZE > 4096
		/* don't cross a 4KB boundary */
		if ((rx->page_offset >> 12) !=
		    ((rx->page_offset + bytes - 1) >> 12))
			rx->page_offset = (rx->page_offset + 4096) & ~4095;
#endif
		rx->fill_cnt++;

		/* copy 8 descriptors to the firmware at a time */
		if ((idx & 7) == 7) {
			myri10ge_submit_8rx(&rx->lanai[idx - 7],
					    &rx->shadow[idx - 7]);
		}
	}
}

static inline void
myri10ge_unmap_rx_page(struct pci_dev *pdev,
		       struct myri10ge_rx_buffer_state *info, int bytes)
{
	/* unmap the recvd page if we're the only or last user of it */
	if (bytes >= MYRI10GE_ALLOC_SIZE / 2 ||
	    (info->page_offset + 2 * bytes) > MYRI10GE_ALLOC_SIZE) {
		pci_unmap_page(pdev, (pci_unmap_addr(info, bus)
				      & ~(MYRI10GE_ALLOC_SIZE - 1)),
			       MYRI10GE_ALLOC_SIZE, PCI_DMA_FROMDEVICE);
	}
}

#define MYRI10GE_HLEN 64	/* The number of bytes to copy from a
				 * page into an skb */

static inline int
myri10ge_rx_done(struct myri10ge_slice_state *ss, struct myri10ge_rx_buf *rx,
		 int bytes, int len, __wsum csum)
{
	struct myri10ge_priv *mgp = ss->mgp;
	struct sk_buff *skb;
	struct skb_frag_struct rx_frags[MYRI10GE_MAX_FRAGS_PER_FRAME];
	int i, idx, hlen, remainder;
	struct pci_dev *pdev = mgp->pdev;
	struct net_device *dev = mgp->dev;
	u8 *va;

	len += MXGEFW_PAD;
	idx = rx->cnt & rx->mask;
	va = page_address(rx->info[idx].page) + rx->info[idx].page_offset;
	prefetch(va);
	/* Fill skb_frag_struct(s) with data from our receive */
	for (i = 0, remainder = len; remainder > 0; i++) {
		myri10ge_unmap_rx_page(pdev, &rx->info[idx], bytes);
		rx_frags[i].page = rx->info[idx].page;
		rx_frags[i].page_offset = rx->info[idx].page_offset;
		if (remainder < MYRI10GE_ALLOC_SIZE)
			rx_frags[i].size = remainder;
		else
			rx_frags[i].size = MYRI10GE_ALLOC_SIZE;
		rx->cnt++;
		idx = rx->cnt & rx->mask;
		remainder -= MYRI10GE_ALLOC_SIZE;
	}

	if (dev->features & NETIF_F_LRO) {
		rx_frags[0].page_offset += MXGEFW_PAD;
		rx_frags[0].size -= MXGEFW_PAD;
		len -= MXGEFW_PAD;
		lro_receive_frags(&ss->rx_done.lro_mgr, rx_frags,
				  /* opaque, will come back in get_frag_header */
				  len, len,
				  (void *)(__force unsigned long)csum, csum);

		return 1;
	}

	hlen = MYRI10GE_HLEN > len ? len : MYRI10GE_HLEN;

	/* allocate an skb to attach the page(s) to. This is done
	 * after trying LRO, so as to avoid skb allocation overheads */

	skb = netdev_alloc_skb(dev, MYRI10GE_HLEN + 16);
	if (unlikely(skb == NULL)) {
		ss->stats.rx_dropped++;
		do {
			i--;
			put_page(rx_frags[i].page);
		} while (i != 0);
		return 0;
	}

	/* Attach the pages to the skb, and trim off any padding */
	myri10ge_rx_skb_build(skb, va, rx_frags, len, hlen);
	if (skb_shinfo(skb)->frags[0].size <= 0) {
		put_page(skb_shinfo(skb)->frags[0].page);
		skb_shinfo(skb)->nr_frags = 0;
	}
	skb->protocol = eth_type_trans(skb, dev);
	skb_record_rx_queue(skb, ss - &mgp->ss[0]);

	if (mgp->csum_flag) {
		if ((skb->protocol == htons(ETH_P_IP)) ||
		    (skb->protocol == htons(ETH_P_IPV6))) {
			skb->csum = csum;
			skb->ip_summed = CHECKSUM_COMPLETE;
		} else
			myri10ge_vlan_ip_csum(skb, csum);
	}
	netif_receive_skb(skb);
	return 1;
}

static inline void
myri10ge_tx_done(struct myri10ge_slice_state *ss, int mcp_index)
{
	struct pci_dev *pdev = ss->mgp->pdev;
	struct myri10ge_tx_buf *tx = &ss->tx;
	struct netdev_queue *dev_queue;
	struct sk_buff *skb;
	int idx, len;

	while (tx->pkt_done != mcp_index) {
		idx = tx->done & tx->mask;
		skb = tx->info[idx].skb;

		/* Mark as free */
		tx->info[idx].skb = NULL;
		if (tx->info[idx].last) {
			tx->pkt_done++;
			tx->info[idx].last = 0;
		}
		tx->done++;
		len = pci_unmap_len(&tx->info[idx], len);
		pci_unmap_len_set(&tx->info[idx], len, 0);
		if (skb) {
			ss->stats.tx_bytes += skb->len;
			ss->stats.tx_packets++;
			dev_kfree_skb_irq(skb);
			if (len)
				pci_unmap_single(pdev,
						 pci_unmap_addr(&tx->info[idx],
								bus), len,
						 PCI_DMA_TODEVICE);
		} else {
			if (len)
				pci_unmap_page(pdev,
					       pci_unmap_addr(&tx->info[idx],
							      bus), len,
					       PCI_DMA_TODEVICE);
		}
	}

	dev_queue = netdev_get_tx_queue(ss->dev, ss - ss->mgp->ss);
	/*
	 * Make a minimal effort to prevent the NIC from polling an
	 * idle tx queue.  If we can't get the lock we leave the queue
	 * active. In this case, either a thread was about to start
	 * using the queue anyway, or we lost a race and the NIC will
	 * waste some of its resources polling an inactive queue for a
	 * while.
	 */

	if ((ss->mgp->dev->real_num_tx_queues > 1) &&
	    __netif_tx_trylock(dev_queue)) {
		if (tx->req == tx->done) {
			tx->queue_active = 0;
			put_be32(htonl(1), tx->send_stop);
			mb();
			mmiowb();
		}
		__netif_tx_unlock(dev_queue);
	}

	/* start the queue if we've stopped it */
	if (netif_tx_queue_stopped(dev_queue)
	    && tx->req - tx->done < (tx->mask >> 1)) {
		tx->wake_queue++;
		netif_tx_wake_queue(dev_queue);
	}
}

static inline int
myri10ge_clean_rx_done(struct myri10ge_slice_state *ss, int budget)
{
	struct myri10ge_rx_done *rx_done = &ss->rx_done;
	struct myri10ge_priv *mgp = ss->mgp;
	struct net_device *netdev = mgp->dev;
	unsigned long rx_bytes = 0;
	unsigned long rx_packets = 0;
	unsigned long rx_ok;

	int idx = rx_done->idx;
	int cnt = rx_done->cnt;
	int work_done = 0;
	u16 length;
	__wsum checksum;

	while (rx_done->entry[idx].length != 0 && work_done < budget) {
		length = ntohs(rx_done->entry[idx].length);
		rx_done->entry[idx].length = 0;
		checksum = csum_unfold(rx_done->entry[idx].checksum);
		if (length <= mgp->small_bytes)
			rx_ok = myri10ge_rx_done(ss, &ss->rx_small,
						 mgp->small_bytes,
						 length, checksum);
		else
			rx_ok = myri10ge_rx_done(ss, &ss->rx_big,
						 mgp->big_bytes,
						 length, checksum);
		rx_packets += rx_ok;
		rx_bytes += rx_ok * (unsigned long)length;
		cnt++;
		idx = cnt & (mgp->max_intr_slots - 1);
		work_done++;
	}
	rx_done->idx = idx;
	rx_done->cnt = cnt;
	ss->stats.rx_packets += rx_packets;
	ss->stats.rx_bytes += rx_bytes;

	if (netdev->features & NETIF_F_LRO)
		lro_flush_all(&rx_done->lro_mgr);

	/* restock receive rings if needed */
	if (ss->rx_small.fill_cnt - ss->rx_small.cnt < myri10ge_fill_thresh)
		myri10ge_alloc_rx_pages(mgp, &ss->rx_small,
					mgp->small_bytes + MXGEFW_PAD, 0);
	if (ss->rx_big.fill_cnt - ss->rx_big.cnt < myri10ge_fill_thresh)
		myri10ge_alloc_rx_pages(mgp, &ss->rx_big, mgp->big_bytes, 0);

	return work_done;
}

static inline void myri10ge_check_statblock(struct myri10ge_priv *mgp)
{
	struct mcp_irq_data *stats = mgp->ss[0].fw_stats;

	if (unlikely(stats->stats_updated)) {
		unsigned link_up = ntohl(stats->link_up);
		if (mgp->link_state != link_up) {
			mgp->link_state = link_up;

			if (mgp->link_state == MXGEFW_LINK_UP) {
				if (netif_msg_link(mgp))
					printk(KERN_INFO
					       "myri10ge: %s: link up\n",
					       mgp->dev->name);
				netif_carrier_on(mgp->dev);
				mgp->link_changes++;
			} else {
				if (netif_msg_link(mgp))
					printk(KERN_INFO
					       "myri10ge: %s: link %s\n",
					       mgp->dev->name,
					       (link_up == MXGEFW_LINK_MYRINET ?
						"mismatch (Myrinet detected)" :
						"down"));
				netif_carrier_off(mgp->dev);
				mgp->link_changes++;
			}
		}
		if (mgp->rdma_tags_available !=
		    ntohl(stats->rdma_tags_available)) {
			mgp->rdma_tags_available =
			    ntohl(stats->rdma_tags_available);
			printk(KERN_WARNING "myri10ge: %s: RDMA timed out! "
			       "%d tags left\n", mgp->dev->name,
			       mgp->rdma_tags_available);
		}
		mgp->down_cnt += stats->link_down;
		if (stats->link_down)
			wake_up(&mgp->down_wq);
	}
}

static int myri10ge_poll(struct napi_struct *napi, int budget)
{
	struct myri10ge_slice_state *ss =
	    container_of(napi, struct myri10ge_slice_state, napi);
	int work_done;

#ifdef CONFIG_MYRI10GE_DCA
	if (ss->mgp->dca_enabled)
		myri10ge_update_dca(ss);
#endif

	/* process as many rx events as NAPI will allow */
	work_done = myri10ge_clean_rx_done(ss, budget);

	if (work_done < budget) {
		napi_complete(napi);
		put_be32(htonl(3), ss->irq_claim);
	}
	return work_done;
}

static irqreturn_t myri10ge_intr(int irq, void *arg)
{
	struct myri10ge_slice_state *ss = arg;
	struct myri10ge_priv *mgp = ss->mgp;
	struct mcp_irq_data *stats = ss->fw_stats;
	struct myri10ge_tx_buf *tx = &ss->tx;
	u32 send_done_count;
	int i;

	/* an interrupt on a non-zero receive-only slice is implicitly
	 * valid  since MSI-X irqs are not shared */
	if ((mgp->dev->real_num_tx_queues == 1) && (ss != mgp->ss)) {
		napi_schedule(&ss->napi);
		return (IRQ_HANDLED);
	}

	/* make sure it is our IRQ, and that the DMA has finished */
	if (unlikely(!stats->valid))
		return (IRQ_NONE);

	/* low bit indicates receives are present, so schedule
	 * napi poll handler */
	if (stats->valid & 1)
		napi_schedule(&ss->napi);

	if (!mgp->msi_enabled && !mgp->msix_enabled) {
		put_be32(0, mgp->irq_deassert);
		if (!myri10ge_deassert_wait)
			stats->valid = 0;
		mb();
	} else
		stats->valid = 0;

	/* Wait for IRQ line to go low, if using INTx */
	i = 0;
	while (1) {
		i++;
		/* check for transmit completes and receives */
		send_done_count = ntohl(stats->send_done_count);
		if (send_done_count != tx->pkt_done)
			myri10ge_tx_done(ss, (int)send_done_count);
		if (unlikely(i > myri10ge_max_irq_loops)) {
			printk(KERN_WARNING "myri10ge: %s: irq stuck?\n",
			       mgp->dev->name);
			stats->valid = 0;
			schedule_work(&mgp->watchdog_work);
		}
		if (likely(stats->valid == 0))
			break;
		cpu_relax();
		barrier();
	}

	/* Only slice 0 updates stats */
	if (ss == mgp->ss)
		myri10ge_check_statblock(mgp);

	put_be32(htonl(3), ss->irq_claim + 1);
	return (IRQ_HANDLED);
}

static int
myri10ge_get_settings(struct net_device *netdev, struct ethtool_cmd *cmd)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);
	char *ptr;
	int i;

	cmd->autoneg = AUTONEG_DISABLE;
	cmd->speed = SPEED_10000;
	cmd->duplex = DUPLEX_FULL;

	/*
	 * parse the product code to deterimine the interface type
	 * (CX4, XFP, Quad Ribbon Fiber) by looking at the character
	 * after the 3rd dash in the driver's cached copy of the
	 * EEPROM's product code string.
	 */
	ptr = mgp->product_code_string;
	if (ptr == NULL) {
		printk(KERN_ERR "myri10ge: %s: Missing product code\n",
		       netdev->name);
		return 0;
	}
	for (i = 0; i < 3; i++, ptr++) {
		ptr = strchr(ptr, '-');
		if (ptr == NULL) {
			printk(KERN_ERR "myri10ge: %s: Invalid product "
			       "code %s\n", netdev->name,
			       mgp->product_code_string);
			return 0;
		}
	}
	if (*ptr == '2')
		ptr++;
	if (*ptr == 'R' || *ptr == 'Q' || *ptr == 'S') {
		/* We've found either an XFP, quad ribbon fiber, or SFP+ */
		cmd->port = PORT_FIBRE;
		cmd->supported |= SUPPORTED_FIBRE;
		cmd->advertising |= ADVERTISED_FIBRE;
	} else {
		cmd->port = PORT_OTHER;
	}
	if (*ptr == 'R' || *ptr == 'S')
		cmd->transceiver = XCVR_EXTERNAL;
	else
		cmd->transceiver = XCVR_INTERNAL;

	return 0;
}

static void
myri10ge_get_drvinfo(struct net_device *netdev, struct ethtool_drvinfo *info)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);

	strlcpy(info->driver, "myri10ge", sizeof(info->driver));
	strlcpy(info->version, MYRI10GE_VERSION_STR, sizeof(info->version));
	strlcpy(info->fw_version, mgp->fw_version, sizeof(info->fw_version));
	strlcpy(info->bus_info, pci_name(mgp->pdev), sizeof(info->bus_info));
}

static int
myri10ge_get_coalesce(struct net_device *netdev, struct ethtool_coalesce *coal)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);

	coal->rx_coalesce_usecs = mgp->intr_coal_delay;
	return 0;
}

static int
myri10ge_set_coalesce(struct net_device *netdev, struct ethtool_coalesce *coal)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);

	mgp->intr_coal_delay = coal->rx_coalesce_usecs;
	put_be32(htonl(mgp->intr_coal_delay), mgp->intr_coal_delay_ptr);
	return 0;
}

static void
myri10ge_get_pauseparam(struct net_device *netdev,
			struct ethtool_pauseparam *pause)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);

	pause->autoneg = 0;
	pause->rx_pause = mgp->pause;
	pause->tx_pause = mgp->pause;
}

static int
myri10ge_set_pauseparam(struct net_device *netdev,
			struct ethtool_pauseparam *pause)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);

	if (pause->tx_pause != mgp->pause)
		return myri10ge_change_pause(mgp, pause->tx_pause);
	if (pause->rx_pause != mgp->pause)
		return myri10ge_change_pause(mgp, pause->tx_pause);
	if (pause->autoneg != 0)
		return -EINVAL;
	return 0;
}

static void
myri10ge_get_ringparam(struct net_device *netdev,
		       struct ethtool_ringparam *ring)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);

	ring->rx_mini_max_pending = mgp->ss[0].rx_small.mask + 1;
	ring->rx_max_pending = mgp->ss[0].rx_big.mask + 1;
	ring->rx_jumbo_max_pending = 0;
	ring->tx_max_pending = mgp->ss[0].tx.mask + 1;
	ring->rx_mini_pending = ring->rx_mini_max_pending;
	ring->rx_pending = ring->rx_max_pending;
	ring->rx_jumbo_pending = ring->rx_jumbo_max_pending;
	ring->tx_pending = ring->tx_max_pending;
}

static u32 myri10ge_get_rx_csum(struct net_device *netdev)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);

	if (mgp->csum_flag)
		return 1;
	else
		return 0;
}

static int myri10ge_set_rx_csum(struct net_device *netdev, u32 csum_enabled)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);
	int err = 0;

	if (csum_enabled)
		mgp->csum_flag = MXGEFW_FLAGS_CKSUM;
	else {
		u32 flags = ethtool_op_get_flags(netdev);
		err = ethtool_op_set_flags(netdev, (flags & ~ETH_FLAG_LRO));
		mgp->csum_flag = 0;

	}
	return err;
}

static int myri10ge_set_tso(struct net_device *netdev, u32 tso_enabled)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);
	unsigned long flags = mgp->features & (NETIF_F_TSO6 | NETIF_F_TSO);

	if (tso_enabled)
		netdev->features |= flags;
	else
		netdev->features &= ~flags;
	return 0;
}

static const char myri10ge_gstrings_main_stats[][ETH_GSTRING_LEN] = {
	"rx_packets", "tx_packets", "rx_bytes", "tx_bytes", "rx_errors",
	"tx_errors", "rx_dropped", "tx_dropped", "multicast", "collisions",
	"rx_length_errors", "rx_over_errors", "rx_crc_errors",
	"rx_frame_errors", "rx_fifo_errors", "rx_missed_errors",
	"tx_aborted_errors", "tx_carrier_errors", "tx_fifo_errors",
	"tx_heartbeat_errors", "tx_window_errors",
	/* device-specific stats */
	"tx_boundary", "WC", "irq", "MSI", "MSIX",
	"read_dma_bw_MBs", "write_dma_bw_MBs", "read_write_dma_bw_MBs",
	"serial_number", "watchdog_resets",
#ifdef CONFIG_MYRI10GE_DCA
	"dca_capable_firmware", "dca_device_present",
#endif
	"link_changes", "link_up", "dropped_link_overflow",
	"dropped_link_error_or_filtered",
	"dropped_pause", "dropped_bad_phy", "dropped_bad_crc32",
	"dropped_unicast_filtered", "dropped_multicast_filtered",
	"dropped_runt", "dropped_overrun", "dropped_no_small_buffer",
	"dropped_no_big_buffer"
};

static const char myri10ge_gstrings_slice_stats[][ETH_GSTRING_LEN] = {
	"----------- slice ---------",
	"tx_pkt_start", "tx_pkt_done", "tx_req", "tx_done",
	"rx_small_cnt", "rx_big_cnt",
	"wake_queue", "stop_queue", "tx_linearized", "LRO aggregated",
	    "LRO flushed",
	"LRO avg aggr", "LRO no_desc"
};

#define MYRI10GE_NET_STATS_LEN      21
#define MYRI10GE_MAIN_STATS_LEN  ARRAY_SIZE(myri10ge_gstrings_main_stats)
#define MYRI10GE_SLICE_STATS_LEN  ARRAY_SIZE(myri10ge_gstrings_slice_stats)

static void
myri10ge_get_strings(struct net_device *netdev, u32 stringset, u8 * data)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);
	int i;

	switch (stringset) {
	case ETH_SS_STATS:
		memcpy(data, *myri10ge_gstrings_main_stats,
		       sizeof(myri10ge_gstrings_main_stats));
		data += sizeof(myri10ge_gstrings_main_stats);
		for (i = 0; i < mgp->num_slices; i++) {
			memcpy(data, *myri10ge_gstrings_slice_stats,
			       sizeof(myri10ge_gstrings_slice_stats));
			data += sizeof(myri10ge_gstrings_slice_stats);
		}
		break;
	}
}

static int myri10ge_get_sset_count(struct net_device *netdev, int sset)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);

	switch (sset) {
	case ETH_SS_STATS:
		return MYRI10GE_MAIN_STATS_LEN +
		    mgp->num_slices * MYRI10GE_SLICE_STATS_LEN;
	default:
		return -EOPNOTSUPP;
	}
}

static void
myri10ge_get_ethtool_stats(struct net_device *netdev,
			   struct ethtool_stats *stats, u64 * data)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);
	struct myri10ge_slice_state *ss;
	int slice;
	int i;

	/* force stats update */
	(void)myri10ge_get_stats(netdev);
	for (i = 0; i < MYRI10GE_NET_STATS_LEN; i++)
		data[i] = ((unsigned long *)&mgp->stats)[i];

	data[i++] = (unsigned int)mgp->tx_boundary;
	data[i++] = (unsigned int)mgp->wc_enabled;
	data[i++] = (unsigned int)mgp->pdev->irq;
	data[i++] = (unsigned int)mgp->msi_enabled;
	data[i++] = (unsigned int)mgp->msix_enabled;
	data[i++] = (unsigned int)mgp->read_dma;
	data[i++] = (unsigned int)mgp->write_dma;
	data[i++] = (unsigned int)mgp->read_write_dma;
	data[i++] = (unsigned int)mgp->serial_number;
	data[i++] = (unsigned int)mgp->watchdog_resets;
#ifdef CONFIG_MYRI10GE_DCA
	data[i++] = (unsigned int)(mgp->ss[0].dca_tag != NULL);
	data[i++] = (unsigned int)(mgp->dca_enabled);
#endif
	data[i++] = (unsigned int)mgp->link_changes;

	/* firmware stats are useful only in the first slice */
	ss = &mgp->ss[0];
	data[i++] = (unsigned int)ntohl(ss->fw_stats->link_up);
	data[i++] = (unsigned int)ntohl(ss->fw_stats->dropped_link_overflow);
	data[i++] =
	    (unsigned int)ntohl(ss->fw_stats->dropped_link_error_or_filtered);
	data[i++] = (unsigned int)ntohl(ss->fw_stats->dropped_pause);
	data[i++] = (unsigned int)ntohl(ss->fw_stats->dropped_bad_phy);
	data[i++] = (unsigned int)ntohl(ss->fw_stats->dropped_bad_crc32);
	data[i++] = (unsigned int)ntohl(ss->fw_stats->dropped_unicast_filtered);
	data[i++] =
	    (unsigned int)ntohl(ss->fw_stats->dropped_multicast_filtered);
	data[i++] = (unsigned int)ntohl(ss->fw_stats->dropped_runt);
	data[i++] = (unsigned int)ntohl(ss->fw_stats->dropped_overrun);
	data[i++] = (unsigned int)ntohl(ss->fw_stats->dropped_no_small_buffer);
	data[i++] = (unsigned int)ntohl(ss->fw_stats->dropped_no_big_buffer);

	for (slice = 0; slice < mgp->num_slices; slice++) {
		ss = &mgp->ss[slice];
		data[i++] = slice;
		data[i++] = (unsigned int)ss->tx.pkt_start;
		data[i++] = (unsigned int)ss->tx.pkt_done;
		data[i++] = (unsigned int)ss->tx.req;
		data[i++] = (unsigned int)ss->tx.done;
		data[i++] = (unsigned int)ss->rx_small.cnt;
		data[i++] = (unsigned int)ss->rx_big.cnt;
		data[i++] = (unsigned int)ss->tx.wake_queue;
		data[i++] = (unsigned int)ss->tx.stop_queue;
		data[i++] = (unsigned int)ss->tx.linearized;
		data[i++] = ss->rx_done.lro_mgr.stats.aggregated;
		data[i++] = ss->rx_done.lro_mgr.stats.flushed;
		if (ss->rx_done.lro_mgr.stats.flushed)
			data[i++] = ss->rx_done.lro_mgr.stats.aggregated /
			    ss->rx_done.lro_mgr.stats.flushed;
		else
			data[i++] = 0;
		data[i++] = ss->rx_done.lro_mgr.stats.no_desc;
	}
}

static void myri10ge_set_msglevel(struct net_device *netdev, u32 value)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);
	mgp->msg_enable = value;
}

static u32 myri10ge_get_msglevel(struct net_device *netdev)
{
	struct myri10ge_priv *mgp = netdev_priv(netdev);
	return mgp->msg_enable;
}

static const struct ethtool_ops myri10ge_ethtool_ops = {
	.get_settings = myri10ge_get_settings,
	.get_drvinfo = myri10ge_get_drvinfo,
	.get_coalesce = myri10ge_get_coalesce,
	.set_coalesce = myri10ge_set_coalesce,
	.get_pauseparam = myri10ge_get_pauseparam,
	.set_pauseparam = myri10ge_set_pauseparam,
	.get_ringparam = myri10ge_get_ringparam,
	.get_rx_csum = myri10ge_get_rx_csum,
	.set_rx_csum = myri10ge_set_rx_csum,
	.set_tx_csum = ethtool_op_set_tx_hw_csum,
	.set_sg = ethtool_op_set_sg,
	.set_tso = myri10ge_set_tso,
	.get_link = ethtool_op_get_link,
	.get_strings = myri10ge_get_strings,
	.get_sset_count = myri10ge_get_sset_count,
	.get_ethtool_stats = myri10ge_get_ethtool_stats,
	.set_msglevel = myri10ge_set_msglevel,
	.get_msglevel = myri10ge_get_msglevel,
	.get_flags = ethtool_op_get_flags,
	.set_flags = ethtool_op_set_flags
};

static int myri10ge_allocate_rings(struct myri10ge_slice_state *ss)
{
	struct myri10ge_priv *mgp = ss->mgp;
	struct myri10ge_cmd cmd;
	struct net_device *dev = mgp->dev;
	int tx_ring_size, rx_ring_size;
	int tx_ring_entries, rx_ring_entries;
	int i, slice, status;
	size_t bytes;

	/* get ring sizes */
	slice = ss - mgp->ss;
	cmd.data0 = slice;
	status = myri10ge_send_cmd(mgp, MXGEFW_CMD_GET_SEND_RING_SIZE, &cmd, 0);
	tx_ring_size = cmd.data0;
	cmd.data0 = slice;
	status |= myri10ge_send_cmd(mgp, MXGEFW_CMD_GET_RX_RING_SIZE, &cmd, 0);
	if (status != 0)
		return status;
	rx_ring_size = cmd.data0;

	tx_ring_entries = tx_ring_size / sizeof(struct mcp_kreq_ether_send);
	rx_ring_entries = rx_ring_size / sizeof(struct mcp_dma_addr);
	ss->tx.mask = tx_ring_entries - 1;
	ss->rx_small.mask = ss->rx_big.mask = rx_ring_entries - 1;

	status = -ENOMEM;

	/* allocate the host shadow rings */

	bytes = 8 + (MYRI10GE_MAX_SEND_DESC_TSO + 4)
	    * sizeof(*ss->tx.req_list);
	ss->tx.req_bytes = kzalloc(bytes, GFP_KERNEL);
	if (ss->tx.req_bytes == NULL)
		goto abort_with_nothing;

	/* ensure req_list entries are aligned to 8 bytes */
	ss->tx.req_list = (struct mcp_kreq_ether_send *)
	    ALIGN((unsigned long)ss->tx.req_bytes, 8);
	ss->tx.queue_active = 0;

	bytes = rx_ring_entries * sizeof(*ss->rx_small.shadow);
	ss->rx_small.shadow = kzalloc(bytes, GFP_KERNEL);
	if (ss->rx_small.shadow == NULL)
		goto abort_with_tx_req_bytes;

	bytes = rx_ring_entries * sizeof(*ss->rx_big.shadow);
	ss->rx_big.shadow = kzalloc(bytes, GFP_KERNEL);
	if (ss->rx_big.shadow == NULL)
		goto abort_with_rx_small_shadow;

	/* allocate the host info rings */

	bytes = tx_ring_entries * sizeof(*ss->tx.info);
	ss->tx.info = kzalloc(bytes, GFP_KERNEL);
	if (ss->tx.info == NULL)
		goto abort_with_rx_big_shadow;

	bytes = rx_ring_entries * sizeof(*ss->rx_small.info);
	ss->rx_small.info = kzalloc(bytes, GFP_KERNEL);
	if (ss->rx_small.info == NULL)
		goto abort_with_tx_info;

	bytes = rx_ring_entries * sizeof(*ss->rx_big.info);
	ss->rx_big.info = kzalloc(bytes, GFP_KERNEL);
	if (ss->rx_big.info == NULL)
		goto abort_with_rx_small_info;

	/* Fill the receive rings */
	ss->rx_big.cnt = 0;
	ss->rx_small.cnt = 0;
	ss->rx_big.fill_cnt = 0;
	ss->rx_small.fill_cnt = 0;
	ss->rx_small.page_offset = MYRI10GE_ALLOC_SIZE;
	ss->rx_big.page_offset = MYRI10GE_ALLOC_SIZE;
	ss->rx_small.watchdog_needed = 0;
	ss->rx_big.watchdog_needed = 0;
	myri10ge_alloc_rx_pages(mgp, &ss->rx_small,
				mgp->small_bytes + MXGEFW_PAD, 0);

	if (ss->rx_small.fill_cnt < ss->rx_small.mask + 1) {
		printk(KERN_ERR
		       "myri10ge: %s:slice-%d: alloced only %d small bufs\n",
		       dev->name, slice, ss->rx_small.fill_cnt);
		goto abort_with_rx_small_ring;
	}

	myri10ge_alloc_rx_pages(mgp, &ss->rx_big, mgp->big_bytes, 0);
	if (ss->rx_big.fill_cnt < ss->rx_big.mask + 1) {
		printk(KERN_ERR
		       "myri10ge: %s:slice-%d: alloced only %d big bufs\n",
		       dev->name, slice, ss->rx_big.fill_cnt);
		goto abort_with_rx_big_ring;
	}

	return 0;

abort_with_rx_big_ring:
	for (i = ss->rx_big.cnt; i < ss->rx_big.fill_cnt; i++) {
		int idx = i & ss->rx_big.mask;
		myri10ge_unmap_rx_page(mgp->pdev, &ss->rx_big.info[idx],
				       mgp->big_bytes);
		put_page(ss->rx_big.info[idx].page);
	}

abort_with_rx_small_ring:
	for (i = ss->rx_small.cnt; i < ss->rx_small.fill_cnt; i++) {
		int idx = i & ss->rx_small.mask;
		myri10ge_unmap_rx_page(mgp->pdev, &ss->rx_small.info[idx],
				       mgp->small_bytes + MXGEFW_PAD);
		put_page(ss->rx_small.info[idx].page);
	}

	kfree(ss->rx_big.info);

abort_with_rx_small_info:
	kfree(ss->rx_small.info);

abort_with_tx_info:
	kfree(ss->tx.info);

abort_with_rx_big_shadow:
	kfree(ss->rx_big.shadow);

abort_with_rx_small_shadow:
	kfree(ss->rx_small.shadow);

abort_with_tx_req_bytes:
	kfree(ss->tx.req_bytes);
	ss->tx.req_bytes = NULL;
	ss->tx.req_list = NULL;

abort_with_nothing:
	return status;
}

static void myri10ge_free_rings(struct myri10ge_slice_state *ss)
{
	struct myri10ge_priv *mgp = ss->mgp;
	struct sk_buff *skb;
	struct myri10ge_tx_buf *tx;
	int i, len, idx;

	/* If not allocated, skip it */
	if (ss->tx.req_list == NULL)
		return;

	for (i = ss->rx_big.cnt; i < ss->rx_big.fill_cnt; i++) {
		idx = i & ss->rx_big.mask;
		if (i == ss->rx_big.fill_cnt - 1)
			ss->rx_big.info[idx].page_offset = MYRI10GE_ALLOC_SIZE;
		myri10ge_unmap_rx_page(mgp->pdev, &ss->rx_big.info[idx],
				       mgp->big_bytes);
		put_page(ss->rx_big.info[idx].page);
	}

	for (i = ss->rx_small.cnt; i < ss->rx_small.fill_cnt; i++) {
		idx = i & ss->rx_small.mask;
		if (i == ss->rx_small.fill_cnt - 1)
			ss->rx_small.info[idx].page_offset =
			    MYRI10GE_ALLOC_SIZE;
		myri10ge_unmap_rx_page(mgp->pdev, &ss->rx_small.info[idx],
				       mgp->small_bytes + MXGEFW_PAD);
		put_page(ss->rx_small.info[idx].page);
	}
	tx = &ss->tx;
	while (tx->done != tx->req) {
		idx = tx->done & tx->mask;
		skb = tx->info[idx].skb;

		/* Mark as free */
		tx->info[idx].skb = NULL;
		tx->done++;
		len = pci_unmap_len(&tx->info[idx], len);
		pci_unmap_len_set(&tx->info[idx], len, 0);
		if (skb) {
			ss->stats.tx_dropped++;
			dev_kfree_skb_any(skb);
			if (len)
				pci_unmap_single(mgp->pdev,
						 pci_unmap_addr(&tx->info[idx],
								bus), len,
						 PCI_DMA_TODEVICE);
		} else {
			if (len)
				pci_unmap_page(mgp->pdev,
					       pci_unmap_addr(&tx->info[idx],
							      bus), len,
					       PCI_DMA_TODEVICE);
		}
	}
	kfree(ss->rx_big.info);

	kfree(ss->rx_small.info);

	kfree(ss->tx.info);

	kfree(ss->rx_big.shadow);

	kfree(ss->rx_small.shadow);

	kfree(ss->tx.req_bytes);
	ss->tx.req_bytes = NULL;
	ss->tx.req_list = NULL;
}

static int myri10ge_request_irq(struct myri10ge_priv *mgp)
{
	struct pci_dev *pdev = mgp->pdev;
	struct myri10ge_slice_state *ss;
	struct net_device *netdev = mgp->dev;
	int i;
	int status;

	mgp->msi_enabled = 0;
	mgp->msix_enabled = 0;
	status = 0;
	if (myri10ge_msi) {
		if (mgp->num_slices > 1) {
			status =
			    pci_enable_msix(pdev, mgp->msix_vectors,
					    mgp->num_slices);
			if (status == 0) {
				mgp->msix_enabled = 1;
			} else {
				dev_err(&pdev->dev,
					"Error %d setting up MSI-X\n", status);
				return status;
			}
		}
		if (mgp->msix_enabled == 0) {
			status = pci_enable_msi(pdev);
			if (status != 0) {
				dev_err(&pdev->dev,
					"Error %d setting up MSI; falling back to xPIC\n",
					status);
			} else {
				mgp->msi_enabled = 1;
			}
		}
	}
	if (mgp->msix_enabled) {
		for (i = 0; i < mgp->num_slices; i++) {
			ss = &mgp->ss[i];
			snprintf(ss->irq_desc, sizeof(ss->irq_desc),
				 "%s:slice-%d", netdev->name, i);
			status = request_irq(mgp->msix_vectors[i].vector,
					     myri10ge_intr, 0, ss->irq_desc,
					     ss);
			if (status != 0) {
				dev_err(&pdev->dev,
					"slice %d failed to allocate IRQ\n", i);
				i--;
				while (i >= 0) {
					free_irq(mgp->msix_vectors[i].vector,
						 &mgp->ss[i]);
					i--;
				}
				pci_disable_msix(pdev);
				return status;
			}
		}
	} else {
		status = request_irq(pdev->irq, myri10ge_intr, IRQF_SHARED,
				     mgp->dev->name, &mgp->ss[0]);
		if (status != 0) {
			dev_err(&pdev->dev, "failed to allocate IRQ\n");
			if (mgp->msi_enabled)
				pci_disable_msi(pdev);
		}
	}
	return status;
}

static void myri10ge_free_irq(struct myri10ge_priv *mgp)
{
	struct pci_dev *pdev = mgp->pdev;
	int i;

	if (mgp->msix_enabled) {
		for (i = 0; i < mgp->num_slices; i++)
			free_irq(mgp->msix_vectors[i].vector, &mgp->ss[i]);
	} else {
		free_irq(pdev->irq, &mgp->ss[0]);
	}
	if (mgp->msi_enabled)
		pci_disable_msi(pdev);
	if (mgp->msix_enabled)
		pci_disable_msix(pdev);
}

static int
myri10ge_get_frag_header(struct skb_frag_struct *frag, void **mac_hdr,
			 void **ip_hdr, void **tcpudp_hdr,
			 u64 * hdr_flags, void *priv)
{
	struct ethhdr *eh;
	struct vlan_ethhdr *veh;
	struct iphdr *iph;
	u8 *va = page_address(frag->page) + frag->page_offset;
	unsigned long ll_hlen;
	/* passed opaque through lro_receive_frags() */
	__wsum csum = (__force __wsum) (unsigned long)priv;

	/* find the mac header, aborting if not IPv4 */

	eh = (struct ethhdr *)va;
	*mac_hdr = eh;
	ll_hlen = ETH_HLEN;
	if (eh->h_proto != htons(ETH_P_IP)) {
		if (eh->h_proto == htons(ETH_P_8021Q)) {
			veh = (struct vlan_ethhdr *)va;
			if (veh->h_vlan_encapsulated_proto != htons(ETH_P_IP))
				return -1;

			ll_hlen += VLAN_HLEN;

			/*
			 *  HW checksum starts ETH_HLEN bytes into
			 *  frame, so we must subtract off the VLAN
			 *  header's checksum before csum can be used
			 */
			csum = csum_sub(csum, csum_partial(va + ETH_HLEN,
							   VLAN_HLEN, 0));
		} else {
			return -1;
		}
	}
	*hdr_flags = LRO_IPV4;

	iph = (struct iphdr *)(va + ll_hlen);
	*ip_hdr = iph;
	if (iph->protocol != IPPROTO_TCP)
		return -1;
	if (iph->frag_off & htons(IP_MF | IP_OFFSET))
		return -1;
	*hdr_flags |= LRO_TCP;
	*tcpudp_hdr = (u8 *) (*ip_hdr) + (iph->ihl << 2);

	/* verify the IP checksum */
	if (unlikely(ip_fast_csum((u8 *) iph, iph->ihl)))
		return -1;

	/* verify the  checksum */
	if (unlikely(csum_tcpudp_magic(iph->saddr, iph->daddr,
				       ntohs(iph->tot_len) - (iph->ihl << 2),
				       IPPROTO_TCP, csum)))
		return -1;

	return 0;
}

static int myri10ge_get_txrx(struct myri10ge_priv *mgp, int slice)
{
	struct myri10ge_cmd cmd;
	struct myri10ge_slice_state *ss;
	int status;

	ss = &mgp->ss[slice];
	status = 0;
	if (slice == 0 || (mgp->dev->real_num_tx_queues > 1)) {
		cmd.data0 = slice;
		status = myri10ge_send_cmd(mgp, MXGEFW_CMD_GET_SEND_OFFSET,
					   &cmd, 0);
		ss->tx.lanai = (struct mcp_kreq_ether_send __iomem *)
		    (mgp->sram + cmd.data0);
	}
	cmd.data0 = slice;
	status |= myri10ge_send_cmd(mgp, MXGEFW_CMD_GET_SMALL_RX_OFFSET,
				    &cmd, 0);
	ss->rx_small.lanai = (struct mcp_kreq_ether_recv __iomem *)
	    (mgp->sram + cmd.data0);

	cmd.data0 = slice;
	status |= myri10ge_send_cmd(mgp, MXGEFW_CMD_GET_BIG_RX_OFFSET, &cmd, 0);
	ss->rx_big.lanai = (struct mcp_kreq_ether_recv __iomem *)
	    (mgp->sram + cmd.data0);

	ss->tx.send_go = (__iomem __be32 *)
	    (mgp->sram + MXGEFW_ETH_SEND_GO + 64 * slice);
	ss->tx.send_stop = (__iomem __be32 *)
	    (mgp->sram + MXGEFW_ETH_SEND_STOP + 64 * slice);
	return status;

}

static int myri10ge_set_stats(struct myri10ge_priv *mgp, int slice)
{
	struct myri10ge_cmd cmd;
	struct myri10ge_slice_state *ss;
	int status;

	ss = &mgp->ss[slice];
	cmd.data0 = MYRI10GE_LOWPART_TO_U32(ss->fw_stats_bus);
	cmd.data1 = MYRI10GE_HIGHPART_TO_U32(ss->fw_stats_bus);
	cmd.data2 = sizeof(struct mcp_irq_data) | (slice << 16);
	status = myri10ge_send_cmd(mgp, MXGEFW_CMD_SET_STATS_DMA_V2, &cmd, 0);
	if (status == -ENOSYS) {
		dma_addr_t bus = ss->fw_stats_bus;
		if (slice != 0)
			return -EINVAL;
		bus += offsetof(struct mcp_irq_data, send_done_count);
		cmd.data0 = MYRI10GE_LOWPART_TO_U32(bus);
		cmd.data1 = MYRI10GE_HIGHPART_TO_U32(bus);
		status = myri10ge_send_cmd(mgp,
					   MXGEFW_CMD_SET_STATS_DMA_OBSOLETE,
					   &cmd, 0);
		/* Firmware cannot support multicast without STATS_DMA_V2 */
		mgp->fw_multicast_support = 0;
	} else {
		mgp->fw_multicast_support = 1;
	}
	return 0;
}

static int myri10ge_open(struct net_device *dev)
{
	struct myri10ge_slice_state *ss;
	struct myri10ge_priv *mgp = netdev_priv(dev);
	struct myri10ge_cmd cmd;
	int i, status, big_pow2, slice;
	u8 *itable;
	struct net_lro_mgr *lro_mgr;

	if (mgp->running != MYRI10GE_ETH_STOPPED)
		return -EBUSY;

	mgp->running = MYRI10GE_ETH_STARTING;
	status = myri10ge_reset(mgp);
	if (status != 0) {
		printk(KERN_ERR "myri10ge: %s: failed reset\n", dev->name);
		goto abort_with_nothing;
	}

	if (mgp->num_slices > 1) {
		cmd.data0 = mgp->num_slices;
		cmd.data1 = MXGEFW_SLICE_INTR_MODE_ONE_PER_SLICE;
		if (mgp->dev->real_num_tx_queues > 1)
			cmd.data1 |= MXGEFW_SLICE_ENABLE_MULTIPLE_TX_QUEUES;
		status = myri10ge_send_cmd(mgp, MXGEFW_CMD_ENABLE_RSS_QUEUES,
					   &cmd, 0);
		if (status != 0) {
			printk(KERN_ERR
			       "myri10ge: %s: failed to set number of slices\n",
			       dev->name);
			goto abort_with_nothing;
		}
		/* setup the indirection table */
		cmd.data0 = mgp->num_slices;
		status = myri10ge_send_cmd(mgp, MXGEFW_CMD_SET_RSS_TABLE_SIZE,
					   &cmd, 0);

		status |= myri10ge_send_cmd(mgp,
					    MXGEFW_CMD_GET_RSS_TABLE_OFFSET,
					    &cmd, 0);
		if (status != 0) {
			printk(KERN_ERR
			       "myri10ge: %s: failed to setup rss tables\n",
			       dev->name);
			goto abort_with_nothing;
		}

		/* just enable an identity mapping */
		itable = mgp->sram + cmd.data0;
		for (i = 0; i < mgp->num_slices; i++)
			__raw_writeb(i, &itable[i]);

		cmd.data0 = 1;
		cmd.data1 = myri10ge_rss_hash;
		status = myri10ge_send_cmd(mgp, MXGEFW_CMD_SET_RSS_ENABLE,
					   &cmd, 0);
		if (status != 0) {
			printk(KERN_ERR
			       "myri10ge: %s: failed to enable slices\n",
			       dev->name);
			goto abort_with_nothing;
		}
	}

	status = myri10ge_request_irq(mgp);
	if (status != 0)
		goto abort_with_nothing;

	/* decide what small buffer size to use.  For good TCP rx
	 * performance, it is important to not receive 1514 byte
	 * frames into jumbo buffers, as it confuses the socket buffer
	 * accounting code, leading to drops and erratic performance.
	 */

	if (dev->mtu <= ETH_DATA_LEN)
		/* enough for a TCP header */
		mgp->small_bytes = (128 > SMP_CACHE_BYTES)
		    ? (128 - MXGEFW_PAD)
		    : (SMP_CACHE_BYTES - MXGEFW_PAD);
	else
		/* enough for a vlan encapsulated ETH_DATA_LEN frame */
		mgp->small_bytes = VLAN_ETH_FRAME_LEN;

	/* Override the small buffer size? */
	if (myri10ge_small_bytes > 0)
		mgp->small_bytes = myri10ge_small_bytes;

	/* Firmware needs the big buff size as a power of 2.  Lie and
	 * tell him the buffer is larger, because we only use 1
	 * buffer/pkt, and the mtu will prevent overruns.
	 */
	big_pow2 = dev->mtu + ETH_HLEN + VLAN_HLEN + MXGEFW_PAD;
	if (big_pow2 < MYRI10GE_ALLOC_SIZE / 2) {
		while (!is_power_of_2(big_pow2))
			big_pow2++;
		mgp->big_bytes = dev->mtu + ETH_HLEN + VLAN_HLEN + MXGEFW_PAD;
	} else {
		big_pow2 = MYRI10GE_ALLOC_SIZE;
		mgp->big_bytes = big_pow2;
	}

	/* setup the per-slice data structures */
	for (slice = 0; slice < mgp->num_slices; slice++) {
		ss = &mgp->ss[slice];

		status = myri10ge_get_txrx(mgp, slice);
		if (status != 0) {
			printk(KERN_ERR
			       "myri10ge: %s: failed to get ring sizes or locations\n",
			       dev->name);
			goto abort_with_rings;
		}
		status = myri10ge_allocate_rings(ss);
		if (status != 0)
			goto abort_with_rings;

		/* only firmware which supports multiple TX queues
		 * supports setting up the tx stats on non-zero
		 * slices */
		if (slice == 0 || mgp->dev->real_num_tx_queues > 1)
			status = myri10ge_set_stats(mgp, slice);
		if (status) {
			printk(KERN_ERR
			       "myri10ge: %s: Couldn't set stats DMA\n",
			       dev->name);
			goto abort_with_rings;
		}

		lro_mgr = &ss->rx_done.lro_mgr;
		lro_mgr->dev = dev;
		lro_mgr->features = LRO_F_NAPI;
		lro_mgr->ip_summed = CHECKSUM_COMPLETE;
		lro_mgr->ip_summed_aggr = CHECKSUM_UNNECESSARY;
		lro_mgr->max_desc = MYRI10GE_MAX_LRO_DESCRIPTORS;
		lro_mgr->lro_arr = ss->rx_done.lro_desc;
		lro_mgr->get_frag_header = myri10ge_get_frag_header;
		lro_mgr->max_aggr = myri10ge_lro_max_pkts;
		lro_mgr->frag_align_pad = 2;
		if (lro_mgr->max_aggr > MAX_SKB_FRAGS)
			lro_mgr->max_aggr = MAX_SKB_FRAGS;

		/* must happen prior to any irq */
		napi_enable(&(ss)->napi);
	}

	/* now give firmware buffers sizes, and MTU */
	cmd.data0 = dev->mtu + ETH_HLEN + VLAN_HLEN;
	status = myri10ge_send_cmd(mgp, MXGEFW_CMD_SET_MTU, &cmd, 0);
	cmd.data0 = mgp->small_bytes;
	status |=
	    myri10ge_send_cmd(mgp, MXGEFW_CMD_SET_SMALL_BUFFER_SIZE, &cmd, 0);
	cmd.data0 = big_pow2;
	status |=
	    myri10ge_send_cmd(mgp, MXGEFW_CMD_SET_BIG_BUFFER_SIZE, &cmd, 0);
	if (status) {
		printk(KERN_ERR "myri10ge: %s: Couldn't set buffer sizes\n",
		       dev->name);
		goto abort_with_rings;
	}

	/*
	 * Set Linux style TSO mode; this is needed only on newer
	 *  firmware versions.  Older versions default to Linux
	 *  style TSO
	 */
	cmd.data0 = 0;
	status = myri10ge_send_cmd(mgp, MXGEFW_CMD_SET_TSO_MODE, &cmd, 0);
	if (status && status != -ENOSYS) {
		printk(KERN_ERR "myri10ge: %s: Couldn't set TSO mode\n",
		       dev->name);
		goto abort_with_rings;
	}

	mgp->link_state = ~0U;
	mgp->rdma_tags_available = 15;

	status = myri10ge_send_cmd(mgp, MXGEFW_CMD_ETHERNET_UP, &cmd, 0);
	if (status) {
		printk(KERN_ERR "myri10ge: %s: Couldn't bring up link\n",
		       dev->name);
		goto abort_with_rings;
	}

	mgp->running = MYRI10GE_ETH_RUNNING;
	mgp->watchdog_timer.expires = jiffies + myri10ge_watchdog_timeout * HZ;
	add_timer(&mgp->watchdog_timer);
	netif_tx_wake_all_queues(dev);

	return 0;

abort_with_rings:
	while (slice) {
		slice--;
		napi_disable(&mgp->ss[slice].napi);
	}
	for (i = 0; i < mgp->num_slices; i++)
		myri10ge_free_rings(&mgp->ss[i]);

	myri10ge_free_irq(mgp);

abort_with_nothing:
	mgp->running = MYRI10GE_ETH_STOPPED;
	return -ENOMEM;
}

static int myri10ge_close(struct net_device *dev)
{
	struct myri10ge_priv *mgp = netdev_priv(dev);
	struct myri10ge_cmd cmd;
	int status, old_down_cnt;
	int i;

	if (mgp->running != MYRI10GE_ETH_RUNNING)
		return 0;

	if (mgp->ss[0].tx.req_bytes == NULL)
		return 0;

	del_timer_sync(&mgp->watchdog_timer);
	mgp->running = MYRI10GE_ETH_STOPPING;
	for (i = 0; i < mgp->num_slices; i++) {
		napi_disable(&mgp->ss[i].napi);
	}
	netif_carrier_off(dev);

	netif_tx_stop_all_queues(dev);
	if (mgp->rebooted == 0) {
		old_down_cnt = mgp->down_cnt;
		mb();
		status =
		    myri10ge_send_cmd(mgp, MXGEFW_CMD_ETHERNET_DOWN, &cmd, 0);
		if (status)
			printk(KERN_ERR
			       "myri10ge: %s: Couldn't bring down link\n",
			       dev->name);

		wait_event_timeout(mgp->down_wq, old_down_cnt != mgp->down_cnt,
				   HZ);
		if (old_down_cnt == mgp->down_cnt)
			printk(KERN_ERR "myri10ge: %s never got down irq\n",
			       dev->name);
	}
	netif_tx_disable(dev);
	myri10ge_free_irq(mgp);
	for (i = 0; i < mgp->num_slices; i++)
		myri10ge_free_rings(&mgp->ss[i]);

	mgp->running = MYRI10GE_ETH_STOPPED;
	return 0;
}

/* copy an array of struct mcp_kreq_ether_send's to the mcp.  Copy
 * backwards one at a time and handle ring wraps */

static inline void
myri10ge_submit_req_backwards(struct myri10ge_tx_buf *tx,
			      struct mcp_kreq_ether_send *src, int cnt)
{
	int idx, starting_slot;
	starting_slot = tx->req;
	while (cnt > 1) {
		cnt--;
		idx = (starting_slot + cnt) & tx->mask;
		myri10ge_pio_copy(&tx->lanai[idx], &src[cnt], sizeof(*src));
		mb();
	}
}

/*
 * copy an array of struct mcp_kreq_ether_send's to the mcp.  Copy
 * at most 32 bytes at a time, so as to avoid involving the software
 * pio handler in the nic.   We re-write the first segment's flags
 * to mark them valid only after writing the entire chain.
 */

static inline void
myri10ge_submit_req(struct myri10ge_tx_buf *tx, struct mcp_kreq_ether_send *src,
		    int cnt)
{
	int idx, i;
	struct mcp_kreq_ether_send __iomem *dstp, *dst;
	struct mcp_kreq_ether_send *srcp;
	u8 last_flags;

	idx = tx->req & tx->mask;

	last_flags = src->flags;
	src->flags = 0;
	mb();
	dst = dstp = &tx->lanai[idx];
	srcp = src;

	if ((idx + cnt) < tx->mask) {
		for (i = 0; i < (cnt - 1); i += 2) {
			myri10ge_pio_copy(dstp, srcp, 2 * sizeof(*src));
			mb();	/* force write every 32 bytes */
			srcp += 2;
			dstp += 2;
		}
	} else {
		/* submit all but the first request, and ensure
		 * that it is submitted below */
		myri10ge_submit_req_backwards(tx, src, cnt);
		i = 0;
	}
	if (i < cnt) {
		/* submit the first request */
		myri10ge_pio_copy(dstp, srcp, sizeof(*src));
		mb();		/* barrier before setting valid flag */
	}

	/* re-write the last 32-bits with the valid flags */
	src->flags = last_flags;
	put_be32(*((__be32 *) src + 3), (__be32 __iomem *) dst + 3);
	tx->req += cnt;
	mb();
}

/*
 * Transmit a packet.  We need to split the packet so that a single
 * segment does not cross myri10ge->tx_boundary, so this makes segment
 * counting tricky.  So rather than try to count segments up front, we
 * just give up if there are too few segments to hold a reasonably
 * fragmented packet currently available.  If we run
 * out of segments while preparing a packet for DMA, we just linearize
 * it and try again.
 */

static netdev_tx_t myri10ge_xmit(struct sk_buff *skb,
				       struct net_device *dev)
{
	struct myri10ge_priv *mgp = netdev_priv(dev);
	struct myri10ge_slice_state *ss;
	struct mcp_kreq_ether_send *req;
	struct myri10ge_tx_buf *tx;
	struct skb_frag_struct *frag;
	struct netdev_queue *netdev_queue;
	dma_addr_t bus;
	u32 low;
	__be32 high_swapped;
	unsigned int len;
	int idx, last_idx, avail, frag_cnt, frag_idx, count, mss, max_segments;
	u16 pseudo_hdr_offset, cksum_offset, queue;
	int cum_len, seglen, boundary, rdma_count;
	u8 flags, odd_flag;

	queue = skb_get_queue_mapping(skb);
	ss = &mgp->ss[queue];
	netdev_queue = netdev_get_tx_queue(mgp->dev, queue);
	tx = &ss->tx;

again:
	req = tx->req_list;
	avail = tx->mask - 1 - (tx->req - tx->done);

	mss = 0;
	max_segments = MXGEFW_MAX_SEND_DESC;

	if (skb_is_gso(skb)) {
		mss = skb_shinfo(skb)->gso_size;
		max_segments = MYRI10GE_MAX_SEND_DESC_TSO;
	}

	if ((unlikely(avail < max_segments))) {
		/* we are out of transmit resources */
		tx->stop_queue++;
		netif_tx_stop_queue(netdev_queue);
		return NETDEV_TX_BUSY;
	}

	/* Setup checksum offloading, if needed */
	cksum_offset = 0;
	pseudo_hdr_offset = 0;
	odd_flag = 0;
	flags = (MXGEFW_FLAGS_NO_TSO | MXGEFW_FLAGS_FIRST);
	if (likely(skb->ip_summed == CHECKSUM_PARTIAL)) {
		cksum_offset = skb_transport_offset(skb);
		pseudo_hdr_offset = cksum_offset + skb->csum_offset;
		/* If the headers are excessively large, then we must
		 * fall back to a software checksum */
		if (unlikely(!mss && (cksum_offset > 255 ||
				      pseudo_hdr_offset > 127))) {
			if (skb_checksum_help(skb))
				goto drop;
			cksum_offset = 0;
			pseudo_hdr_offset = 0;
		} else {
			odd_flag = MXGEFW_FLAGS_ALIGN_ODD;
			flags |= MXGEFW_FLAGS_CKSUM;
		}
	}

	cum_len = 0;

	if (mss) {		/* TSO */
		/* this removes any CKSUM flag from before */
		flags = (MXGEFW_FLAGS_TSO_HDR | MXGEFW_FLAGS_FIRST);

		/* negative cum_len signifies to the
		 * send loop that we are still in the
		 * header portion of the TSO packet.
		 * TSO header can be at most 1KB long */
		cum_len = -(skb_transport_offset(skb) + tcp_hdrlen(skb));

		/* for IPv6 TSO, the checksum offset stores the
		 * TCP header length, to save the firmware from
		 * the need to parse the headers */
		if (skb_is_gso_v6(skb)) {
			cksum_offset = tcp_hdrlen(skb);
			/* Can only handle headers <= max_tso6 long */
			if (unlikely(-cum_len > mgp->max_tso6))
				return myri10ge_sw_tso(skb, dev);
		}
		/* for TSO, pseudo_hdr_offset holds mss.
		 * The firmware figures out where to put
		 * the checksum by parsing the header. */
		pseudo_hdr_offset = mss;
	} else
		/* Mark small packets, and pad out tiny packets */
	if (skb->len <= MXGEFW_SEND_SMALL_SIZE) {
		flags |= MXGEFW_FLAGS_SMALL;

		/* pad frames to at least ETH_ZLEN bytes */
		if (unlikely(skb->len < ETH_ZLEN)) {
			if (skb_padto(skb, ETH_ZLEN)) {
				/* The packet is gone, so we must
				 * return 0 */
				ss->stats.tx_dropped += 1;
				return NETDEV_TX_OK;
			}
			/* adjust the len to account for the zero pad
			 * so that the nic can know how long it is */
			skb->len = ETH_ZLEN;
		}
	}

	/* map the skb for DMA */
	len = skb->len - skb->data_len;
	idx = tx->req & tx->mask;
	tx->info[idx].skb = skb;
	bus = pci_map_single(mgp->pdev, skb->data, len, PCI_DMA_TODEVICE);
	pci_unmap_addr_set(&tx->info[idx], bus, bus);
	pci_unmap_len_set(&tx->info[idx], len, len);

	frag_cnt = skb_shinfo(skb)->nr_frags;
	frag_idx = 0;
	count = 0;
	rdma_count = 0;

	/* "rdma_count" is the number of RDMAs belonging to the
	 * current packet BEFORE the current send request. For
	 * non-TSO packets, this is equal to "count".
	 * For TSO packets, rdma_count needs to be reset
	 * to 0 after a segment cut.
	 *
	 * The rdma_count field of the send request is
	 * the number of RDMAs of the packet starting at
	 * that request. For TSO send requests with one ore more cuts
	 * in the middle, this is the number of RDMAs starting
	 * after the last cut in the request. All previous
	 * segments before the last cut implicitly have 1 RDMA.
	 *
	 * Since the number of RDMAs is not known beforehand,
	 * it must be filled-in retroactively - after each
	 * segmentation cut or at the end of the entire packet.
	 */

	while (1) {
		/* Break the SKB or Fragment up into pieces which
		 * do not cross mgp->tx_boundary */
		low = MYRI10GE_LOWPART_TO_U32(bus);
		high_swapped = htonl(MYRI10GE_HIGHPART_TO_U32(bus));
		while (len) {
			u8 flags_next;
			int cum_len_next;

			if (unlikely(count == max_segments))
				goto abort_linearize;

			boundary =
			    (low + mgp->tx_boundary) & ~(mgp->tx_boundary - 1);
			seglen = boundary - low;
			if (seglen > len)
				seglen = len;
			flags_next = flags & ~MXGEFW_FLAGS_FIRST;
			cum_len_next = cum_len + seglen;
			if (mss) {	/* TSO */
				(req - rdma_count)->rdma_count = rdma_count + 1;

				if (likely(cum_len >= 0)) {	/* payload */
					int next_is_first, chop;

					chop = (cum_len_next > mss);
					cum_len_next = cum_len_next % mss;
					next_is_first = (cum_len_next == 0);
					flags |= chop * MXGEFW_FLAGS_TSO_CHOP;
					flags_next |= next_is_first *
					    MXGEFW_FLAGS_FIRST;
					rdma_count |= -(chop | next_is_first);
					rdma_count += chop & !next_is_first;
				} else if (likely(cum_len_next >= 0)) {	/* header ends */
					int small;

					rdma_count = -1;
					cum_len_next = 0;
					seglen = -cum_len;
					small = (mss <= MXGEFW_SEND_SMALL_SIZE);
					flags_next = MXGEFW_FLAGS_TSO_PLD |
					    MXGEFW_FLAGS_FIRST |
					    (small * MXGEFW_FLAGS_SMALL);
				}
			}
			req->addr_high = high_swapped;
			req->addr_low = htonl(low);
			req->pseudo_hdr_offset = htons(pseudo_hdr_offset);
			req->pad = 0;	/* complete solid 16-byte block; does this matter? */
			req->rdma_count = 1;
			req->length = htons(seglen);
			req->cksum_offset = cksum_offset;
			req->flags = flags | ((cum_len & 1) * odd_flag);

			low += seglen;
			len -= seglen;
			cum_len = cum_len_next;
			flags = flags_next;
			req++;
			count++;
			rdma_count++;
			if (cksum_offset != 0 && !(mss && skb_is_gso_v6(skb))) {
				if (unlikely(cksum_offset > seglen))
					cksum_offset -= seglen;
				else
					cksum_offset = 0;
			}
		}
		if (frag_idx == frag_cnt)
			break;

		/* map next fragment for DMA */
		idx = (count + tx->req) & tx->mask;
		frag = &skb_shinfo(skb)->frags[frag_idx];
		frag_idx++;
		len = frag->size;
		bus = pci_map_page(mgp->pdev, frag->page, frag->page_offset,
				   len, PCI_DMA_TODEVICE);
		pci_unmap_addr_set(&tx->info[idx], bus, bus);
		pci_unmap_len_set(&tx->info[idx], len, len);
	}

	(req - rdma_count)->rdma_count = rdma_count;
	if (mss)
		do {
			req--;
			req->flags |= MXGEFW_FLAGS_TSO_LAST;
		} while (!(req->flags & (MXGEFW_FLAGS_TSO_CHOP |
					 MXGEFW_FLAGS_FIRST)));
	idx = ((count - 1) + tx->req) & tx->mask;
	tx->info[idx].last = 1;
	myri10ge_submit_req(tx, tx->req_list, count);
	/* if using multiple tx queues, make sure NIC polls the
	 * current slice */
	if ((mgp->dev->real_num_tx_queues > 1) && tx->queue_active == 0) {
		tx->queue_active = 1;
		put_be32(htonl(1), tx->send_go);
		mb();
		mmiowb();
	}
	tx->pkt_start++;
	if ((avail - count) < MXGEFW_MAX_SEND_DESC) {
		tx->stop_queue++;
		netif_tx_stop_queue(netdev_queue);
	}
	return NETDEV_TX_OK;

abort_linearize:
	/* Free any DMA resources we've alloced and clear out the skb
	 * slot so as to not trip up assertions, and to avoid a
	 * double-free if linearizing fails */

	last_idx = (idx + 1) & tx->mask;
	idx = tx->req & tx->mask;
	tx->info[idx].skb = NULL;
	do {
		len = pci_unmap_len(&tx->info[idx], len);
		if (len) {
			if (tx->info[idx].skb != NULL)
				pci_unmap_single(mgp->pdev,
						 pci_unmap_addr(&tx->info[idx],
								bus), len,
						 PCI_DMA_TODEVICE);
			else
				pci_unmap_page(mgp->pdev,
					       pci_unmap_addr(&tx->info[idx],
							      bus), len,
					       PCI_DMA_TODEVICE);
			pci_unmap_len_set(&tx->info[idx], len, 0);
			tx->info[idx].skb = NULL;
		}
		idx = (idx + 1) & tx->mask;
	} while (idx != last_idx);
	if (skb_is_gso(skb)) {
		printk(KERN_ERR
		       "myri10ge: %s: TSO but wanted to linearize?!?!?\n",
		       mgp->dev->name);
		goto drop;
	}

	if (skb_linearize(skb))
		goto drop;

	tx->linearized++;
	goto again;

drop:
	dev_kfree_skb_any(skb);
	ss->stats.tx_dropped += 1;
	return NETDEV_TX_OK;

}

static netdev_tx_t myri10ge_sw_tso(struct sk_buff *skb,
					 struct net_device *dev)
{
	struct sk_buff *segs, *curr;
	struct myri10ge_priv *mgp = netdev_priv(dev);
	struct myri10ge_slice_state *ss;
	netdev_tx_t status;

	segs = skb_gso_segment(skb, dev->features & ~NETIF_F_TSO6);
	if (IS_ERR(segs))
		goto drop;

	while (segs) {
		curr = segs;
		segs = segs->next;
		curr->next = NULL;
		status = myri10ge_xmit(curr, dev);
		if (status != 0) {
			dev_kfree_skb_any(curr);
			if (segs != NULL) {
				curr = segs;
				segs = segs->next;
				curr->next = NULL;
				dev_kfree_skb_any(segs);
			}
			goto drop;
		}
	}
	dev_kfree_skb_any(skb);
	return NETDEV_TX_OK;

drop:
	ss = &mgp->ss[skb_get_queue_mapping(skb)];
	dev_kfree_skb_any(skb);
	ss->stats.tx_dropped += 1;
	return NETDEV_TX_OK;
}

static struct net_device_stats *myri10ge_get_stats(struct net_device *dev)
{
	struct myri10ge_priv *mgp = netdev_priv(dev);
	struct myri10ge_slice_netstats *slice_stats;
	struct net_device_stats *stats = &mgp->stats;
	int i;

	spin_lock(&mgp->stats_lock);
	memset(stats, 0, sizeof(*stats));
	for (i = 0; i < mgp->num_slices; i++) {
		slice_stats = &mgp->ss[i].stats;
		stats->rx_packets += slice_stats->rx_packets;
		stats->tx_packets += slice_stats->tx_packets;
		stats->rx_bytes += slice_stats->rx_bytes;
		stats->tx_bytes += slice_stats->tx_bytes;
		stats->rx_dropped += slice_stats->rx_dropped;
		stats->tx_dropped += slice_stats->tx_dropped;
	}
	spin_unlock(&mgp->stats_lock);
	return stats;
}

static void myri10ge_set_multicast_list(struct net_device *dev)
{
	struct myri10ge_priv *mgp = netdev_priv(dev);
	struct myri10ge_cmd cmd;
	struct dev_mc_list *mc_list;
	__be32 data[2] = { 0, 0 };
	int err;

	/* can be called from atomic contexts,
	 * pass 1 to force atomicity in myri10ge_send_cmd() */
	myri10ge_change_promisc(mgp, dev->flags & IFF_PROMISC, 1);

	/* This firmware is known to not support multicast */
	if (!mgp->fw_multicast_support)
		return;

	/* Disable multicast filtering */

	err = myri10ge_send_cmd(mgp, MXGEFW_ENABLE_ALLMULTI, &cmd, 1);
	if (err != 0) {
		printk(KERN_ERR "myri10ge: %s: Failed MXGEFW_ENABLE_ALLMULTI,"
		       " error status: %d\n", dev->name, err);
		goto abort;
	}

	if ((dev->flags & IFF_ALLMULTI) || mgp->adopted_rx_filter_bug) {
		/* request to disable multicast filtering, so quit here */
		return;
	}

	/* Flush the filters */

	err = myri10ge_send_cmd(mgp, MXGEFW_LEAVE_ALL_MULTICAST_GROUPS,
				&cmd, 1);
	if (err != 0) {
		printk(KERN_ERR
		       "myri10ge: %s: Failed MXGEFW_LEAVE_ALL_MULTICAST_GROUPS"
		       ", error status: %d\n", dev->name, err);
		goto abort;
	}

	/* Walk the multicast list, and add each address */
	for (mc_list = dev->mc_list; mc_list != NULL; mc_list = mc_list->next) {
		memcpy(data, &mc_list->dmi_addr, 6);
		cmd.data0 = ntohl(data[0]);
		cmd.data1 = ntohl(data[1]);
		err = myri10ge_send_cmd(mgp, MXGEFW_JOIN_MULTICAST_GROUP,
					&cmd, 1);

		if (err != 0) {
			printk(KERN_ERR "myri10ge: %s: Failed "
			       "MXGEFW_JOIN_MULTICAST_GROUP, error status:"
			       "%d\t", dev->name, err);
			printk(KERN_ERR "MAC %pM\n", mc_list->dmi_addr);
			goto abort;
		}
	}
	/* Enable multicast filtering */
	err = myri10ge_send_cmd(mgp, MXGEFW_DISABLE_ALLMULTI, &cmd, 1);
	if (err != 0) {
		printk(KERN_ERR "myri10ge: %s: Failed MXGEFW_DISABLE_ALLMULTI,"
		       "error status: %d\n", dev->name, err);
		goto abort;
	}

	return;

abort:
	return;
}

static int myri10ge_set_mac_address(struct net_device *dev, void *addr)
{
	struct sockaddr *sa = addr;
	struct myri10ge_priv *mgp = netdev_priv(dev);
	int status;

	if (!is_valid_ether_addr(sa->sa_data))
		return -EADDRNOTAVAIL;

	status = myri10ge_update_mac_address(mgp, sa->sa_data);
	if (status != 0) {
		printk(KERN_ERR
		       "myri10ge: %s: changing mac address failed with %d\n",
		       dev->name, status);
		return status;
	}

	/* change the dev structure */
	memcpy(dev->dev_addr, sa->sa_data, 6);
	return 0;
}

static int myri10ge_change_mtu(struct net_device *dev, int new_mtu)
{
	struct myri10ge_priv *mgp = netdev_priv(dev);
	int error = 0;

	if ((new_mtu < 68) || (ETH_HLEN + new_mtu > MYRI10GE_MAX_ETHER_MTU)) {
		printk(KERN_ERR "myri10ge: %s: new mtu (%d) is not valid\n",
		       dev->name, new_mtu);
		return -EINVAL;
	}
	printk(KERN_INFO "%s: changing mtu from %d to %d\n",
	       dev->name, dev->mtu, new_mtu);
	if (mgp->running) {
		/* if we change the mtu on an active device, we must
		 * reset the device so the firmware sees the change */
		myri10ge_close(dev);
		dev->mtu = new_mtu;
		myri10ge_open(dev);
	} else
		dev->mtu = new_mtu;

	return error;
}

/*
 * Enable ECRC to align PCI-E Completion packets on an 8-byte boundary.
 * Only do it if the bridge is a root port since we don't want to disturb
 * any other device, except if forced with myri10ge_ecrc_enable > 1.
 */

static void myri10ge_enable_ecrc(struct myri10ge_priv *mgp)
{
	struct pci_dev *bridge = mgp->pdev->bus->self;
	struct device *dev = &mgp->pdev->dev;
	unsigned cap;
	unsigned err_cap;
	u16 val;
	u8 ext_type;
	int ret;

	if (!myri10ge_ecrc_enable || !bridge)
		return;

	/* check that the bridge is a root port */
	cap = pci_find_capability(bridge, PCI_CAP_ID_EXP);
	pci_read_config_word(bridge, cap + PCI_CAP_FLAGS, &val);
	ext_type = (val & PCI_EXP_FLAGS_TYPE) >> 4;
	if (ext_type != PCI_EXP_TYPE_ROOT_PORT) {
		if (myri10ge_ecrc_enable > 1) {
			struct pci_dev *prev_bridge, *old_bridge = bridge;

			/* Walk the hierarchy up to the root port
			 * where ECRC has to be enabled */
			do {
				prev_bridge = bridge;
				bridge = bridge->bus->self;
				if (!bridge || prev_bridge == bridge) {
					dev_err(dev,
						"Failed to find root port"
						" to force ECRC\n");
					return;
				}
				cap =
				    pci_find_capability(bridge, PCI_CAP_ID_EXP);
				pci_read_config_word(bridge,
						     cap + PCI_CAP_FLAGS, &val);
				ext_type = (val & PCI_EXP_FLAGS_TYPE) >> 4;
			} while (ext_type != PCI_EXP_TYPE_ROOT_PORT);

			dev_info(dev,
				 "Forcing ECRC on non-root port %s"
				 " (enabling on root port %s)\n",
				 pci_name(old_bridge), pci_name(bridge));
		} else {
			dev_err(dev,
				"Not enabling ECRC on non-root port %s\n",
				pci_name(bridge));
			return;
		}
	}

	cap = pci_find_ext_capability(bridge, PCI_EXT_CAP_ID_ERR);
	if (!cap)
		return;

	ret = pci_read_config_dword(bridge, cap + PCI_ERR_CAP, &err_cap);
	if (ret) {
		dev_err(dev, "failed reading ext-conf-space of %s\n",
			pci_name(bridge));
		dev_err(dev, "\t pci=nommconf in use? "
			"or buggy/incomplete/absent ACPI MCFG attr?\n");
		return;
	}
	if (!(err_cap & PCI_ERR_CAP_ECRC_GENC))
		return;

	err_cap |= PCI_ERR_CAP_ECRC_GENE;
	pci_write_config_dword(bridge, cap + PCI_ERR_CAP, err_cap);
	dev_info(dev, "Enabled ECRC on upstream bridge %s\n", pci_name(bridge));
}

/*
 * The Lanai Z8E PCI-E interface achieves higher Read-DMA throughput
 * when the PCI-E Completion packets are aligned on an 8-byte
 * boundary.  Some PCI-E chip sets always align Completion packets; on
 * the ones that do not, the alignment can be enforced by enabling
 * ECRC generation (if supported).
 *
 * When PCI-E Completion packets are not aligned, it is actually more
 * efficient to limit Read-DMA transactions to 2KB, rather than 4KB.
 *
 * If the driver can neither enable ECRC nor verify that it has
 * already been enabled, then it must use a firmware image which works
 * around unaligned completion packets (myri10ge_rss_ethp_z8e.dat), and it
 * should also ensure that it never gives the device a Read-DMA which is
 * larger than 2KB by setting the tx_boundary to 2KB.  If ECRC is
 * enabled, then the driver should use the aligned (myri10ge_rss_eth_z8e.dat)
 * firmware image, and set tx_boundary to 4KB.
 */

static void myri10ge_firmware_probe(struct myri10ge_priv *mgp)
{
	struct pci_dev *pdev = mgp->pdev;
	struct device *dev = &pdev->dev;
	int status;

	mgp->tx_boundary = 4096;
	/*
	 * Verify the max read request size was set to 4KB
	 * before trying the test with 4KB.
	 */
	status = pcie_get_readrq(pdev);
	if (status < 0) {
		dev_err(dev, "Couldn't read max read req size: %d\n", status);
		goto abort;
	}
	if (status != 4096) {
		dev_warn(dev, "Max Read Request size != 4096 (%d)\n", status);
		mgp->tx_boundary = 2048;
	}
	/*
	 * load the optimized firmware (which assumes aligned PCIe
	 * completions) in order to see if it works on this host.
	 */
	mgp->fw_name = myri10ge_fw_aligned;
	status = myri10ge_load_firmware(mgp, 1);
	if (status != 0) {
		goto abort;
	}

	/*
	 * Enable ECRC if possible
	 */
	myri10ge_enable_ecrc(mgp);

	/*
	 * Run a DMA test which watches for unaligned completions and
	 * aborts on the first one seen.
	 */

	status = myri10ge_dma_test(mgp, MXGEFW_CMD_UNALIGNED_TEST);
	if (status == 0)
		return;		/* keep the aligned firmware */

	if (status != -E2BIG)
		dev_warn(dev, "DMA test failed: %d\n", status);
	if (status == -ENOSYS)
		dev_warn(dev, "Falling back to ethp! "
			 "Please install up to date fw\n");
abort:
	/* fall back to using the unaligned firmware */
	mgp->tx_boundary = 2048;
	mgp->fw_name = myri10ge_fw_unaligned;

}

static void myri10ge_select_firmware(struct myri10ge_priv *mgp)
{
	int overridden = 0;

	if (myri10ge_force_firmware == 0) {
		int link_width, exp_cap;
		u16 lnk;

		exp_cap = pci_find_capability(mgp->pdev, PCI_CAP_ID_EXP);
		pci_read_config_word(mgp->pdev, exp_cap + PCI_EXP_LNKSTA, &lnk);
		link_width = (lnk >> 4) & 0x3f;

		/* Check to see if Link is less than 8 or if the
		 * upstream bridge is known to provide aligned
		 * completions */
		if (link_width < 8) {
			dev_info(&mgp->pdev->dev, "PCIE x%d Link\n",
				 link_width);
			mgp->tx_boundary = 4096;
			mgp->fw_name = myri10ge_fw_aligned;
		} else {
			myri10ge_firmware_probe(mgp);
		}
	} else {
		if (myri10ge_force_firmware == 1) {
			dev_info(&mgp->pdev->dev,
				 "Assuming aligned completions (forced)\n");
			mgp->tx_boundary = 4096;
			mgp->fw_name = myri10ge_fw_aligned;
		} else {
			dev_info(&mgp->pdev->dev,
				 "Assuming unaligned completions (forced)\n");
			mgp->tx_boundary = 2048;
			mgp->fw_name = myri10ge_fw_unaligned;
		}
	}
	if (myri10ge_fw_name != NULL) {
		overridden = 1;
		mgp->fw_name = myri10ge_fw_name;
	}
	if (mgp->board_number < MYRI10GE_MAX_BOARDS &&
	    myri10ge_fw_names[mgp->board_number] != NULL &&
	    strlen(myri10ge_fw_names[mgp->board_number])) {
		mgp->fw_name = myri10ge_fw_names[mgp->board_number];
		overridden = 1;
	}
	if (overridden)
		dev_info(&mgp->pdev->dev, "overriding firmware to %s\n",
			 mgp->fw_name);
}

#ifdef CONFIG_PM
static int myri10ge_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct myri10ge_priv *mgp;
	struct net_device *netdev;

	mgp = pci_get_drvdata(pdev);
	if (mgp == NULL)
		return -EINVAL;
	netdev = mgp->dev;

	netif_device_detach(netdev);
	if (netif_running(netdev)) {
		printk(KERN_INFO "myri10ge: closing %s\n", netdev->name);
		rtnl_lock();
		myri10ge_close(netdev);
		rtnl_unlock();
	}
	myri10ge_dummy_rdma(mgp, 0);
	pci_save_state(pdev);
	pci_disable_device(pdev);

	return pci_set_power_state(pdev, pci_choose_state(pdev, state));
}

static int myri10ge_resume(struct pci_dev *pdev)
{
	struct myri10ge_priv *mgp;
	struct net_device *netdev;
	int status;
	u16 vendor;

	mgp = pci_get_drvdata(pdev);
	if (mgp == NULL)
		return -EINVAL;
	netdev = mgp->dev;
	pci_set_power_state(pdev, 0);	/* zeros conf space as a side effect */
	msleep(5);		/* give card time to respond */
	pci_read_config_word(mgp->pdev, PCI_VENDOR_ID, &vendor);
	if (vendor == 0xffff) {
		printk(KERN_ERR "myri10ge: %s: device disappeared!\n",
		       mgp->dev->name);
		return -EIO;
	}

	status = pci_restore_state(pdev);
	if (status)
		return status;

	status = pci_enable_device(pdev);
	if (status) {
		dev_err(&pdev->dev, "failed to enable device\n");
		return status;
	}

	pci_set_master(pdev);

	myri10ge_reset(mgp);
	myri10ge_dummy_rdma(mgp, 1);

	/* Save configuration space to be restored if the
	 * nic resets due to a parity error */
	pci_save_state(pdev);

	if (netif_running(netdev)) {
		rtnl_lock();
		status = myri10ge_open(netdev);
		rtnl_unlock();
		if (status != 0)
			goto abort_with_enabled;

	}
	netif_device_attach(netdev);

	return 0;

abort_with_enabled:
	pci_disable_device(pdev);
	return -EIO;

}
#endif				/* CONFIG_PM */

static u32 myri10ge_read_reboot(struct myri10ge_priv *mgp)
{
	struct pci_dev *pdev = mgp->pdev;
	int vs = mgp->vendor_specific_offset;
	u32 reboot;

	/*enter read32 mode */
	pci_write_config_byte(pdev, vs + 0x10, 0x3);

	/*read REBOOT_STATUS (0xfffffff0) */
	pci_write_config_dword(pdev, vs + 0x18, 0xfffffff0);
	pci_read_config_dword(pdev, vs + 0x14, &reboot);
	return reboot;
}

/*
 * This watchdog is used to check whether the board has suffered
 * from a parity error and needs to be recovered.
 */
static void myri10ge_watchdog(struct work_struct *work)
{
	struct myri10ge_priv *mgp =
	    container_of(work, struct myri10ge_priv, watchdog_work);
	struct myri10ge_tx_buf *tx;
	u32 reboot;
	int status, rebooted;
	int i;
	u16 cmd, vendor;

	mgp->watchdog_resets++;
	pci_read_config_word(mgp->pdev, PCI_COMMAND, &cmd);
	rebooted = 0;
	if ((cmd & PCI_COMMAND_MASTER) == 0) {
		/* Bus master DMA disabled?  Check to see
		 * if the card rebooted due to a parity error
		 * For now, just report it */
		reboot = myri10ge_read_reboot(mgp);
		printk(KERN_ERR
		       "myri10ge: %s: NIC rebooted (0x%x),%s resetting\n",
		       mgp->dev->name, reboot,
		       myri10ge_reset_recover ? " " : " not");
		if (myri10ge_reset_recover == 0)
			return;
		rtnl_lock();
		mgp->rebooted = 1;
		rebooted = 1;
		myri10ge_close(mgp->dev);
		myri10ge_reset_recover--;
		mgp->rebooted = 0;
		/*
		 * A rebooted nic will come back with config space as
		 * it was after power was applied to PCIe bus.
		 * Attempt to restore config space which was saved
		 * when the driver was loaded, or the last time the
		 * nic was resumed from power saving mode.
		 */
		pci_restore_state(mgp->pdev);

		/* save state again for accounting reasons */
		pci_save_state(mgp->pdev);

	} else {
		/* if we get back -1's from our slot, perhaps somebody
		 * powered off our card.  Don't try to reset it in
		 * this case */
		if (cmd == 0xffff) {
			pci_read_config_word(mgp->pdev, PCI_VENDOR_ID, &vendor);
			if (vendor == 0xffff) {
				printk(KERN_ERR
				       "myri10ge: %s: device disappeared!\n",
				       mgp->dev->name);
				return;
			}
		}
		/* Perhaps it is a software error.  Try to reset */

		printk(KERN_ERR "myri10ge: %s: device timeout, resetting\n",
		       mgp->dev->name);
		for (i = 0; i < mgp->num_slices; i++) {
			tx = &mgp->ss[i].tx;
			printk(KERN_INFO
			       "myri10ge: %s: (%d): %d %d %d %d %d %d\n",
			       mgp->dev->name, i, tx->queue_active, tx->req,
			       tx->done, tx->pkt_start, tx->pkt_done,
			       (int)ntohl(mgp->ss[i].fw_stats->
					  send_done_count));
			msleep(2000);
			printk(KERN_INFO
			       "myri10ge: %s: (%d): %d %d %d %d %d %d\n",
			       mgp->dev->name, i, tx->queue_active, tx->req,
			       tx->done, tx->pkt_start, tx->pkt_done,
			       (int)ntohl(mgp->ss[i].fw_stats->
					  send_done_count));
		}
	}

	if (!rebooted) {
		rtnl_lock();
		myri10ge_close(mgp->dev);
	}
	status = myri10ge_load_firmware(mgp, 1);
	if (status != 0)
		printk(KERN_ERR "myri10ge: %s: failed to load firmware\n",
		       mgp->dev->name);
	else
		myri10ge_open(mgp->dev);
	rtnl_unlock();
}

/*
 * We use our own timer routine rather than relying upon
 * netdev->tx_timeout because we have a very large hardware transmit
 * queue.  Due to the large queue, the netdev->tx_timeout function
 * cannot detect a NIC with a parity error in a timely fashion if the
 * NIC is lightly loaded.
 */
static void myri10ge_watchdog_timer(unsigned long arg)
{
	struct myri10ge_priv *mgp;
	struct myri10ge_slice_state *ss;
	int i, reset_needed, busy_slice_cnt;
	u32 rx_pause_cnt;
	u16 cmd;

	mgp = (struct myri10ge_priv *)arg;

	rx_pause_cnt = ntohl(mgp->ss[0].fw_stats->dropped_pause);
	busy_slice_cnt = 0;
	for (i = 0, reset_needed = 0;
	     i < mgp->num_slices && reset_needed == 0; ++i) {

		ss = &mgp->ss[i];
		if (ss->rx_small.watchdog_needed) {
			myri10ge_alloc_rx_pages(mgp, &ss->rx_small,
						mgp->small_bytes + MXGEFW_PAD,
						1);
			if (ss->rx_small.fill_cnt - ss->rx_small.cnt >=
			    myri10ge_fill_thresh)
				ss->rx_small.watchdog_needed = 0;
		}
		if (ss->rx_big.watchdog_needed) {
			myri10ge_alloc_rx_pages(mgp, &ss->rx_big,
						mgp->big_bytes, 1);
			if (ss->rx_big.fill_cnt - ss->rx_big.cnt >=
			    myri10ge_fill_thresh)
				ss->rx_big.watchdog_needed = 0;
		}

		if (ss->tx.req != ss->tx.done &&
		    ss->tx.done == ss->watchdog_tx_done &&
		    ss->watchdog_tx_req != ss->watchdog_tx_done) {
			/* nic seems like it might be stuck.. */
			if (rx_pause_cnt != mgp->watchdog_pause) {
				if (net_ratelimit())
					printk(KERN_WARNING
					       "myri10ge %s slice %d:"
					       "TX paused, check link partner\n",
					       mgp->dev->name, i);
			} else {
				printk(KERN_WARNING
				       "myri10ge %s slice %d stuck:",
				       mgp->dev->name, i);
				reset_needed = 1;
			}
		}
		if (ss->watchdog_tx_done != ss->tx.done ||
		    ss->watchdog_rx_done != ss->rx_done.cnt) {
			busy_slice_cnt++;
		}
		ss->watchdog_tx_done = ss->tx.done;
		ss->watchdog_tx_req = ss->tx.req;
		ss->watchdog_rx_done = ss->rx_done.cnt;
	}
	/* if we've sent or received no traffic, poll the NIC to
	 * ensure it is still there.  Otherwise, we risk not noticing
	 * an error in a timely fashion */
	if (busy_slice_cnt == 0) {
		pci_read_config_word(mgp->pdev, PCI_COMMAND, &cmd);
		if ((cmd & PCI_COMMAND_MASTER) == 0) {
			reset_needed = 1;
		}
	}
	mgp->watchdog_pause = rx_pause_cnt;

	if (reset_needed) {
		schedule_work(&mgp->watchdog_work);
	} else {
		/* rearm timer */
		mod_timer(&mgp->watchdog_timer,
			  jiffies + myri10ge_watchdog_timeout * HZ);
	}
}

static void myri10ge_free_slices(struct myri10ge_priv *mgp)
{
	struct myri10ge_slice_state *ss;
	struct pci_dev *pdev = mgp->pdev;
	size_t bytes;
	int i;

	if (mgp->ss == NULL)
		return;

	for (i = 0; i < mgp->num_slices; i++) {
		ss = &mgp->ss[i];
		if (ss->rx_done.entry != NULL) {
			bytes = mgp->max_intr_slots *
			    sizeof(*ss->rx_done.entry);
			dma_free_coherent(&pdev->dev, bytes,
	****  ss->rx_done.entry,*************bus);*****************
 *  = NULL****}
		if (****fw_stats !ri10ge) {****
 * myrisizeof(*Myri-10G Eth*****
dma_free_coherent(&pde/****************
 * mdist005 - 2009 Myr005 - 20_ *
 * Redistribution enet dr.c: Myr}
	k, In(mgp->s009 Mficatiowith or w}

 - 2ic int mynet dr_alloc_slices(struche followinpriv *mgp)
{
	 * are met:
 * 1ition5 - 2e *ss;e mus of pci_dev *ight = re perghtthe ize_tedistr;
	 thei;

 *ce cCopyr
 *  (re permn * noticnumdde mus,s of permittkzg con Myri** GFP_KERNEe.c: com ns in binnet driver.
return -ENOMEM;out

	for (i = 0; i < * 2. Redistribut i++ noticebina&re perm[i]ust rhe follre pemax_intr_slots *lowing diC) 2s reserved.
 *
 * Redistrion and/o folicom formd.ce cAll wing s reserve Inc.ce cR	 tors
 & 3. Neither theconsclaimrs
 **produceions ab Myrthouither the name o
 *    er tgoto abortuctsmemset  nod from this so, 0 3. Neither the following dihe d005 -DED 9 Mynary forms, witrivedcom modc. norprodunamebove itsformendorse    me in sourceEXPRESS OR romote romotets deor writHOLDERS warece c  RS Ahout specifer tmgpr otheERCHANTA WARRA* 2. , perm	netif_napi_addrived fromIMPLISE
 , source co.oller thse or pmet:
 * 1IN N*wee, t.c: MyceTICULA0;
 OF M:
	met:
 * 1IBUTOstribu
iscl;GHT LIABL listove }

/ SOFTTermifuncimer.detn binesS IS"
umberND ANYe abosupported.ORhe foLUDING,  NOT LIi (INCLminumum BUTUTE ENT OF
BUTCPUS,PROCRVICES; LOSS OMSI-X irqLIMITED TO,,TUTE ES; LOSS O * SUBPROCSS
 * INT byATA, firmPLIED W/ded thatvoid EVENT SHAL obECT,ATA,Ions of source cowing disclaime above RRANTIES,cmd cmdroductovIES,lowing D WARRA* 2. * LIABLchar *old_fwistribu ,  - 2us, ncpADVImsix_capandollowing disclai = 1tionHCONT P =NG INfiSSIBIPability(e, t, PCI_CAP_ID_MSIXTAL,SED O =TION_online_h, y()OSSIISINGet:
 * 1mate SUCH DA= 1 || T *
 *
 urce0 ||
OPYRIwerY OF LIAB image aurce-1 &&Igh, yo< 2)MED.* LIABOSSI/*name to loadHEORY NOT  aPECI rssY OFSPECIAITY	S SOFTITY ANr fw_
 *  ARISINGR STRICTDArcadietware
 *noticdev_info( documhe names of"overridingIBUTOR, 325N Sto %s\n"HE COP1006
  3. Neither theenue, ******ITY x/tcp.h>
#incluS BE elsIES,m  Anita Ae.h>
#include alignedMED.inux/netdevic<linux/string.rsslinux/st;
	buffing.<l <linmodul<linux/string. <lin<linpcilinu IF ADle.h>
#incluri.c_., 325N IDEN, 0ia, CA 9ermodul!=on.
 *
 * THIS nclude <linux/iR**********/
not found\n"ncludaimer:
com, i/* hitHEORYboardANTIE ames ot

#iensuMyritSTITaliv Santaic prio&cmdion.
owing dcmd)enoude <lin<linux/striseeeprmdeenue.MXGEFW_CMD_RESET, 
#incluping.h>
#includex/if_vlanlinuerron.
 * 3. Neither tfaileder.h>
r.h>dca.NTIES OF M_ing.OFTWARapping.h>
#incenue,re irialsT NOvi=ISE).data0 /ux/ethtoons of scpo.h>
 aD WAR<helltrinm>
 *izeR SERVICinterrupt queues/in.h>
#includeAvenue,<linux/io.h>
#iY, Witteding.h>
#incnet/che<linux/timer.h> OF LIABapping.h>
#includex/dSET_INTRQ_SIZENFIG_MTRR
#include timerONFIG_MTRR
#include vmg conapping.h>
#incluing.h>
#incasm/mtre_mcp.erc32ONFIG_MTRR
#include /dma-#includaskTUTE GaxiS ORION) HOWEVER CAUIit AND ON cplinu/processor.h>
#ifdef CONFIG_MTRR
#include <GET_MAX_RSS_QUEUESndif

#include "myri10ge_mcp.R "1.5.1-1.451"

MODULE_ng.h>
#incluc., 3age at:M>
#includhecksumOnly  forw multiplD WARRMinux/; OR BUs usablx/in.hh>
#!download-Msi******1.5.1-1.451"

MODULE_DESCRIPiip.h>
admin didstrin MERCHy a lim/stro hPPEDany
	ED Ae Mscs/d should use, /wwS ANauten peaalllp@mythefine ES; LOSS., 3Us cur norly  need 1
#dE_MAXdownri.c-ived10GE.htmefinlude <wnload-Myri10GE.htm Contahecksnewe_ETHER_MTU 9014>.h>
#includyri10GE.h>
#includER_MTU 9014

RS 82)
#defiRI10G10YRI10GENowm.h>YRI1 formretaas
2)
#GE4

#_vecse o
#dewe havEMYRI1NO_RES. We gude up onYRI10GESfER 0can2048y get a singlefine _ALLOC.Sant.h>
#inc *
 )
#def_ORIGHT HOL 64ine M10GE_NO_Rncludese o * 1g disclaiMYRI1FRAGS_P)reromote productISING IN ZE + 1)

#deERPLIED WARRNTIESdiPING 4

#x;WARE, EVOSSIroduet:
 * 1_ALLOC_SImerD WAthe
CES 32
IDENuche ftati name of 10GE_ETwhileSMYRI10GE_MAX_ETHE101******/* make/io.h>
#incll powLOSS Otwo/in.hAP_LEN(l!is_DR(bs_of_2ct myrGefine ETHnTHE
IBISantR_MTU 901-- LIMITEDDECLARE_PCI_UNMt:
 RIPTOuffer0G Ete {
	_ADDR(R("Main.h>
opyendata0;
	ur>
#incCI_UNMAP_ADDR(busnclude <CI_UNMAP_ADDR(bu LIMITED clude <h>
#inclu	G IN 2 data0;
	uruct ncludeinux/striND CONTR Me <lin>al BSD DECLARE_PCI_UNMDAM_recv p_krx/strind {
	u32 data0;
	u32 }

r THE ecv rr:AX_SLICI_UNMAP_ADDR(busr************** modire peZE + 1)I10GE_de <linux2

struct myri_h or wi}
FOR AN<linux/dY DI* host shadow of AGE.
<linux/module. AnitaWARNFIG_MTRR
#include if_#incluinuxclude HETHconst ons of netowin musopsT OWNER_ETHe of colan=noti.ndo_open		

struct my	__b,ndq */
stope32 __iomem *close_go;	/* "gart_xmit doorbell ptr *
	__be32 _getorms, wdoorbell ptrll ptr */__be32 _validate* ARr	= eth*req_, EXe32 ___be32 _chanCONFtu doorbekreq_et hadowbyt__be32 __et_D 0
#castw of ssor.h>
#ifdef dat*****;
	 the_#inc
	u32 daace32 _essd_stop;	/* "st/smit sreq __,
};ruct mcp_ge;
	int pageASTRI/io {
	sG IN ANY WAY ,_kadow#incl_seG IN ANmem id *entING NEGLIGENndorbell ptrnai pthis AX_SLIY, OR TORT (INCLUD_cachelinesmit sd>
#i  >
#inclunuxct page *p NEGLnclude <l-ENXIO32 _pacdacata2;
} {
	unsux/st hdCLARfsai ptrtive;
}MYRIpros submiux/st_ENT OFOSSIne ___ =ftry;
tartedANTABq(page_offset),uct myrGEct myuct paacheline proeue;nclude <linux/innt pruct mcROM_Strintry;
ine ma_adES (complerc32.h>
inux/ssL, EXEMPLude <MYRINETDEV_DEVgr lro_,aligned;	pkcheckDECLARne ___rwingnsignedntions inro_mgtne ____c*/
	str***	u32 rxe_txes;
csum_flag =h"I10GE_FL

#dCKSUge_s;
	unsausule.h>
#includlow_XPRESold;
	unsiux/iocoal_dela foline MYRI1lowinition0G Etuct myrimsgke_queud longif_traninit0GE_NO_RESdebugsmit sidx;
	SG_DEFAULTts;
	unsiata0;
	u32   =ne_rx_buf rx_mc	0;
	_waht
 */t_headnclude ne M_wqchecksnew2  <liqueu;t "go"ge_o*/G_MTRR
#include
#include <liv;x_buf rx_SE
 * 0GE_>
#inclrc32.h>
keY EXPPTORtODEVONFIG_MTRR
#include ong rx_by#includFincoludevendor005 E_Ecp_kap so_UNMAPALcYRI1_MAX_UTE reboo;	/*gister lansmion/iude e pe10G Et_;
	X_LROtruct mom/scs Ifge;
	re.hom on youruct my isT OF*infVNDRbus;
sumSet our CON reant pquesFAILE4KBUTHOdata1;
	u3pciing igrbelrqruct my4096clude "myri10ge_mcp.h"
#include "mnclude <linuError %d wriORS B cacEXP{
	uCTL>
#inclurecv rSTR "1.5.1-1.451"

MOe_net/t;
	de <2 daaligne;
	istruct
	s wakslices;ots -1 ata1ct m3ev;
ING,ONTRmaskruct myDMA_BI(MYRSK(64
};

struct myri10ge_tnclud     max_g?  0ing myri10ge_tx_buftain tru"64-biligne 	/* tra ms?  waamesfusNTER"e *detr	stru320;
	e_netG EtheG Eth;;	32 _rx_ytese_stize;
	unsiligned;	s32inux/}ng tx_bmit sbigong tx_b	
	struct net_deviceunsignetx_setndaryDMAe_tx_ringcts statt crossgned longnumditionsunsign	(ER I)ram;
	ints sne;
nint sram_size;
	unsigned long i10g_byrq_dbymg?  ND CONTRIBUTORS "AS IS"
 * AND page_offset;
ol.009 MyrYRIGHT ALLOC_;
NTIESne MYRI10GE_MAX_SLICES 3mit slowin lonts -1 uct mcp_cmd_response followin lonspan *sram;reRRANTI_lenruct my
#ince follll p_baing  tr_slice_staybell __b
	struct mcp_imtr_big-
 *
lignewthe aterials pr#ifdef eq_bIG_MTRRableuct mpleteruct;	/*Eled;
ay_ptr;
	i followworkstructe <linYRIG rx__TYPE_WRCOMB, 1MYRI1uct page  the>uhadow;t;
	x_dolices;_don1;
#endifions in ramRE_Poremap_w
	inI10_ADDR(bi10ge	int mwatchdog_ia, CA 91 IN A_isclr*mgp;
	struet_gnedd	__be32 __iomgs[E_EEri10ge_  NEG%lNY Te foat 0x%lxtransmitsZE];
	char *prtm_addray_ptr;
	inu8 __iomem *s
	_ednt;
Tte {2MYRI10GE_NO_paud_	int_actruct m =e;
#ifntohl(__raw2];
}lodtx_bring;+ MCP_HEADER_PTR_OFFSET)) & 0xtry;cnt c_ADDR(bu =elong serial+ ruct msmnt pkt_s/strgen*****er* ARoffs
	chaots;
	unsim_st_clude=	cha thew_stat_ MERCHic_ofe;
};ing    fe_enab tx_boune_dmt wau32>SIZE];
	char *pri.ce;
#ifd_number;
	int <=_buf rx_smFWfo;
	supw_version[128]ink_chafw_inlocinadow  ber;
	int %dB oread_dmak_cha%ldBer_tinyink_chber;
	inth>
#estatOF THIromotem mac address */
r_majorigned memcpy_tteniord_numIG_MYR_ber;
	ie *d
	inHT notenable;
car *r * followibuf rx_smEEP_descTRINGON_STebell 
#inclfollowinfw_rsssc[32et;
ih_z8e.dat";

stam(mycadia=Y TH<linu2 MYRIprocessoe_mcp.hfdrbelgned;	/* etheenourted ecv rdow;/GPL");I10GE_MAXhp_z8e.dact page *page;
	in10GEALEN	 DECLLAR PUe names e32 _[i] MYRIt bood;	/* tpermdructAP_ADDselectyri10ge_tx_buchecknclude <linux/firmct myri10ge_tx_buf nk_chawg_rx
	struct myrfollowine;
	__be32 __iomed(myri@harp.co., 325N ON_STR "1.5.1-1.451"

MOX_BOARDS t";
se foONTR*/
	ige_fCT Lre imagnclude <linux/firmicom, T, INCIFIRM_DATA
 *  MYR * A,_z8e.p,i10ge,
			 0444); MYRI10GPARM_ formine MYyd_dctON_STR "1.5.1-1.451"

MOD, 325N igned I_UNMAP___ioe *d_tx;
	u/tcpAvenue,(INCLUDING NEMGE_ET(, S_IRUGO | S/dma-m_int boarnsigned long iomemRUGO);
MODULE_PARM_DESC(myrude cON_STR "1.5.1-1.451"

MOLUDING NE}lete net_ligned uf rx_smDCAMAP_ADDBOARDStup_dcate {10grss_unalram;
	intrvnclues, charp,10ge_ecr0GE_NO_RESigneiaint b +able HLEN) >_buf rx_smastr

#definee_cmine MYRI1_IRUGO | S_I>
#inc = "mule_param(msi -USRARM_DE;

static,t th,nallge SignallWnt myri10UL< 68, "Enwork Mess0GE.Signalled68_filr lro_->gned loate *s&aie32 _laPARM_DESC(noug_UNMAP_t, S_Itr_coal_delay, int, y, "Iincludice_e32 _USR)1;	/ay_ptr;
	iatic int myfeaturesow_contrharp, NUevice *dk_chaqueue__majorUNMAP_harp, NUL|= NETIF_F_HIGHDMAge_smparapts");

stafed;
};

stLROGOARM_DESC(mnclu_r MYRIG Eticd;
};

stroal_dX_SLICES 3fhp_z8
stat < 37E_PARM_DESC(x_doDAMA;
/dma&= ~;
};

stTSO6e_deasser_rx_dooal_delay = 25;
module_param(myr(myri10
module_param(dYRI10GEts -1  */
_stats_RDER)nBUSI, a <li at	dmatats_bE_MAX_EULE_ (har vailte {).  Alstlinux/stc int myirq_MAX_is_firri10correct*requf
stre of st max_intndif
	char irner: T 0x ptrdca_irqharp, NULecrc_etr_cooal_e ExtenY, WCRCI10GPCI-E 1;
modupts");

std Intelule_pk_stMAX_BOARIBUTOthat the f thecpaveets figuraENTIAspac/net.bhar storedRUNNING	int wicule_pas duri10ga parity ek_chain.hram;
avusle, "nning?  _IRUGOu;up
	cha	char *pINGS_pi_we and1;	_delanclude RIBUTO, YRI10GO EVENT SHAodule_param(napnterrupt thenet/tg   )}_PARM_lepin_lock?   tr******eNCE Oi10gnt;
MYRIETHTOOL_OPSnsdule_pao#inc MYRIg2.h>oldelayt;
INIT_WORKerrupts");

statworkS_IRUGmyri1"pu;
NAP_PARM_DESC(my*****tagsSC(myrx_paaddr_;
_IRUGO | S_IW int, S_IRUGO);
MODULE_PARM_Dule_t the followinitio: %ce_nte_dmic_offsestruct myrip_SE)
e, "igned 
	inic chill_cRM_DESC(myrnclude <linux/i%dYRI10GEIRQs, tx bndry %d, fw %s, WCg.h>
#includeontr*
 *
== aux_filteed;
ncluassioligned;		chae <linpu;
st;
	char *fw? "Elices;" : "D2 datad"inux/x/stringteUENTIAthreshos0GE_E_MSG	int idx;"Deb;
	strucow_cont0=noLINK

sms;
	strucpts")MSIge_r,xPIC#includupts");

st

static ige_row_contr* defaultscheline*/e_watcharameter");

stadule_oal_del0ARM_DE
	e_rx_buf rx_++AL, SPECIAE_EE;

stataterrq_lo:"Set  	stral_ge_watcharamete576;"T10ge_dld of/* n006
 **** 256;
modCare ima");

statillHER_MTU h = 256;
moddummy_rdm initoorbel_fill_threshr_major_buf rx_p0GE. ... (Mri10ge_DULE_Pt bo];		/r fw_versiomodulenlocktedunsigned lo)elay = 75MAC=struSN=%lRM_DEow(myrihoG, BUTempty rx;
MOyri1ser SigENT OF to beunmapmyri10ber;upts");

static pau:ll packets");

sct wo
	char *prpaustchdog_ par *de2 msg_erq_lmyri10gr_buESC(u8ge_fw_rss_unaligned rss_unalONTRIBUTORnc. nor the names of *THE
 _ALLOC_;ge_fweffollowi#inclPARM_DESCDCAupts");

staticcharp,  aggrtx_pte *inruct m_ADDR(pts");

staticc int :
	IBUTOs");

statets to * LIABLinfo;
inARY, PROCs;
	_ribu")movDECLPROCDo;
	dmetecacheSC(ma 0xfffshutstru oGE_NyriE_ETH_RTOR. C, S_IPROC  onceait, eachE_FWcast_c/strhttpORYkO_DEl when ant pule iUgh,   unDESCO, PROTY, WH

#de INhe fo);

ULE_cax_pact0G Ertram;
	pacING NEGLIGENCE OR OTHm(my  */smitgr lrs zk_struct waong rx_btes,myride_skreqs, int, S_IRoduct_code_32 link0G Eth>
#incl* flush_sre m((udi10got a
;
MODULentA PARTICULAun_irq_loops, int, Sa, int,kreqmax_ir 1;
module__tso(struct sk_sitear_versreq tr_coatic by d 75;
module_param(myrnterruptsSET 1fine a memory leakt, "Set  ateS_IRUGct net_deviceoal_delay = 75ARM_DESCt sk_bufitions, "Max tx/rx<net/tcl, (__force void _availablss_hash = de <lin0GE_HASHtchdogSRC_PORTge_watc 256;
module_param(myri10ge,	atchdostuck atchdX_LRO_ts all;

statiageenable;
am(myriPARM S_IRUGO);
MODULEvice *de_iomem *)p);
}

staticnterrupts");

static uf_bytes[s"TRUGO);
MODULE_caoal_del doe_get_stats(struct net_10ge_lrkts, int, S_IRUG*    	stru0GE_)d   is DEVICE*infeYRICOM__force voZ8E 	0x0008ma_high,mes ult,	 "Fo);

statsleep_total = _9;

	/*9ruct mcp_ TO,gned long toplicet, S_IRUGsci_tbl[ =
ta0;{f is align(resuVENDORed to 8 bytnt cac aligned to 8 bytes */
	buf = )}, <linu= htonl
	 ( <li-> <lin)h>
#in
	buf-1nl(data-ta2);
	buf-1cmd = htonl(_9a2nl(d0}(to,frMYRI10);
	dma_TABLEdev;pi_weight, L 8MYRIf *sax_i *)ALIGN((myrig*****)bufnt boa->respo sego;	odule.onseux/i_k_bu.*/
	islicescge_of*/
	ispon;
	chatats;
	_ri10ge	chama_hid_titODULgEol, "x_t _TO_U3,c ;

static intPM
	.suspeng?  i10ge_msi e_pio_ensuresude <linux/firmght (b,cmd(struo,frkb,
					 struct net_deviceailable;do
ai;	/* lanotifyeed  rom,yrigcappi = Mi10g *nb,  net_device * event,efine *DING NARE_epletelow = _for_ol, net_devi_delay = 7low = .low = nclude <t dr, &ts suruct myriwhichresu runed atnet_deviAX_PKTS 6rrfo;
	int ( * SIFY_BADdelay = 75};
mcomDONE(myri10ge_ma5LDERaimerunsitten a tthtions  2cas set,i);
	dma_lm those cable opy(cmd* AREset, do n__be3exyri1 (atoppackioable = 0(to,frss_unaibu/*			 struct net_devicGE_NOailable;delaysubmitted     ponse ?ice *yri10NG Nprintk(te p_INFOLE_P: Vers_DESstringpacke_cmd.low = X_LRl,e;
#ifm(myuf rx_smVERSIONRUGOp,
		 * andY OF LIne MYashstruincludel (t
myhar *pMAXtX_BOARta0;m(myuseE"Maxr;
	inHTep foIllegal modh  is atyperru,i10ge *innge Mess_sta  * Imed(myrep_totaai;	/* la(tes */
	bufmodule_paralow =ulPARM	imes ult == MYRI1teout, innGE_EEPRGE_NO_R_send_cm msi_eb,
					 struct net_device ions net_devicset, NTIE */
		for e->data);d_cmd(struc;

struct myrMAP_ADDLRO_nterrupts");

sw_versi "Enable MesSYS;
		} elsePARM_DESC(myrw_versse =* LIABL_u32) et_devilow =  MXGEFW_CMD_.2ms ow, dmmb();
out = s, charp,;
MODmb();
2ificatal <__exitefine MYRI10GE_cleanupS &&
				}
	} eltonl(gned to 		e, this0LE_MUkbuft, S_w_tso(sltc inde <linux/dUNKNOWN)SULT)
			   0-ONSEurn -ERANGERROR_RANGE &&
MODU ool.	} else {
			t = 0GE_NO_RESW watch_EN = M);
